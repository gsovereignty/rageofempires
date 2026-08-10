#!/usr/bin/env python3
"""Capture browser console output from the production WebAssembly bundle.

Uses Chrome DevTools Protocol directly so agent environments need no Selenium,
Playwright, or other Python package.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import select
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIST = ROOT / "build-web" / "dist"
DEFAULT_OUTPUT = ROOT / "artifacts" / "browser-console" / "console.json"


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        return

    def copyfile(self, source: Any, outputfile: Any) -> None:
        try:
            super().copyfile(source, outputfile)
        except BrokenPipeError:
            pass


class WebSocket:
    def __init__(self, url: str) -> None:
        if not url.startswith("ws://"):
            raise RuntimeError(f"unsupported DevTools URL: {url}")
        authority, path = url[5:].split("/", 1)
        host, port_text = authority.rsplit(":", 1)
        self.socket = socket.create_connection((host, int(port_text)), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (
            f"GET /{path} HTTP/1.1\r\nHost: {authority}\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        self.socket.sendall(request.encode("ascii"))
        response = self._read_until(b"\r\n\r\n")
        if not response.startswith(b"HTTP/1.1 101"):
            raise RuntimeError(f"DevTools WebSocket handshake failed: {response!r}")
        accept = base64.b64encode(
            hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")
            ).digest()
        )
        if f"Sec-WebSocket-Accept: {accept.decode('ascii')}".lower() not in response.decode("latin1").lower():
            raise RuntimeError("DevTools WebSocket handshake returned wrong accept key")
        self.socket.settimeout(None)

    def _read_until(self, marker: bytes) -> bytes:
        data = bytearray()
        while marker not in data:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise RuntimeError("DevTools WebSocket closed during handshake")
            data.extend(chunk)
        return bytes(data)

    def send(self, value: dict[str, Any]) -> None:
        payload = json.dumps(value, separators=(",", ":")).encode("utf-8")
        mask = os.urandom(4)
        length = len(payload)
        if length < 126:
            header = bytes((0x81, 0x80 | length))
        elif length < 65536:
            header = bytes((0x81, 0xFE)) + struct.pack("!H", length)
        else:
            header = bytes((0x81, 0xFF)) + struct.pack("!Q", length)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.socket.sendall(header + mask + masked)

    def receive(self, timeout: float) -> dict[str, Any] | None:
        ready, _, _ = select.select([self.socket], [], [], timeout)
        if not ready:
            return None
        first, second = self._read_exact(2)
        opcode = first & 0x0F
        length = second & 0x7F
        if length == 126:
            length = struct.unpack("!H", self._read_exact(2))[0]
        elif length == 127:
            length = struct.unpack("!Q", self._read_exact(8))[0]
        if second & 0x80:
            mask = self._read_exact(4)
        else:
            mask = b""
        payload = self._read_exact(length)
        if mask:
            payload = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        if opcode == 0x8:
            raise RuntimeError("DevTools WebSocket closed")
        if opcode == 0x9:
            self._send_control(0xA, payload)
            return None
        if opcode != 0x1:
            return None
        return json.loads(payload.decode("utf-8"))

    def _read_exact(self, length: int) -> bytes:
        data = bytearray()
        while len(data) < length:
            chunk = self.socket.recv(length - len(data))
            if not chunk:
                raise RuntimeError("DevTools WebSocket closed")
            data.extend(chunk)
        return bytes(data)

    def _send_control(self, opcode: int, payload: bytes) -> None:
        mask = os.urandom(4)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.socket.sendall(bytes((0x80 | opcode, 0x80 | len(payload))) + mask + masked)

    def close(self) -> None:
        self.socket.close()


def free_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def chrome_path(explicit: str | None) -> str:
    candidates = [
        explicit,
        shutil.which("google-chrome"),
        shutil.which("chromium"),
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(candidate)
    raise RuntimeError("Chrome/Chromium executable not found; pass --chrome")


def devtools_target(port: int, deadline: float) -> dict[str, Any]:
    endpoint = f"http://127.0.0.1:{port}/json"
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(endpoint, timeout=1) as response:
                targets = json.load(response)
            pages = [target for target in targets if target.get("type") == "page"]
            if pages:
                return pages[0]
        except (OSError, ValueError):
            pass
        time.sleep(0.05)
    raise RuntimeError("timed out waiting for Chrome DevTools target")


def remote_value(message: dict[str, Any]) -> Any:
    return message.get("result", {}).get("result", {}).get("value")


def capture(args: argparse.Namespace) -> dict[str, Any]:
    dist = args.dist.resolve()
    page = dist / "aoe_web.html"
    if not page.is_file():
        raise RuntimeError(f"browser distribution missing: {page}")

    handler = lambda *values, **keywords: QuietHandler(
        *values, directory=str(dist), **keywords
    )
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    debug_port = free_port()
    with tempfile.TemporaryDirectory(prefix="aoe-browser-console-") as profile:
        url = f"http://127.0.0.1:{server.server_address[1]}/aoe_web.html"
        command = [
            chrome_path(args.chrome),
            "--headless=new",
            "--disable-gpu",
            "--no-first-run",
            "--no-default-browser-check",
            f"--remote-debugging-port={debug_port}",
            f"--user-data-dir={profile}",
            url,
        ]
        process = subprocess.Popen(
            command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True
        )
        websocket: WebSocket | None = None
        messages: list[dict[str, Any]] = []
        responses: dict[int, dict[str, Any]] = {}
        next_id = 1

        def send(method: str, params: dict[str, Any] | None = None) -> int:
            nonlocal next_id
            identifier = next_id
            next_id += 1
            assert websocket is not None
            websocket.send({"id": identifier, "method": method, "params": params or {}})
            return identifier

        def pump(timeout: float) -> None:
            assert websocket is not None
            message = websocket.receive(timeout)
            if message is None:
                return
            if "id" in message:
                responses[int(message["id"])] = message
            elif message.get("method") in {
                "Log.entryAdded",
                "Runtime.consoleAPICalled",
                "Runtime.exceptionThrown",
            }:
                messages.append(message)

        def evaluate(expression: str, await_promise: bool = False) -> Any:
            identifier = send(
                "Runtime.evaluate",
                {
                    "expression": expression,
                    "awaitPromise": await_promise,
                    "returnByValue": True,
                },
            )
            deadline = time.monotonic() + 10
            while identifier not in responses and time.monotonic() < deadline:
                pump(0.1)
            if identifier not in responses:
                raise RuntimeError("timed out waiting for Runtime.evaluate")
            return remote_value(responses.pop(identifier))

        try:
            target = devtools_target(debug_port, time.monotonic() + 10)
            websocket = WebSocket(str(target["webSocketDebuggerUrl"]))
            send("Runtime.enable")
            send("Log.enable")
            send("Network.enable")
            ready_deadline = time.monotonic() + args.start_timeout
            ready = False
            while time.monotonic() < ready_deadline:
                pump(0.1)
                ready = bool(evaluate(
                    "typeof Module !== 'undefined' && Module.storageReady === true "
                    "&& !document.getElementById('start').hidden"
                ))
                if ready:
                    break
            if ready:
                center = evaluate(
                    "(() => {const rect=document.getElementById('start')"
                    ".getBoundingClientRect(); return {x:rect.x+rect.width/2, "
                    "y:rect.y+rect.height/2};})()"
                )
                send("Input.dispatchMouseEvent", {
                    "type": "mousePressed", "x": center["x"], "y": center["y"],
                    "button": "left", "buttons": 1, "clickCount": 1
                })
                send("Input.dispatchMouseEvent", {
                    "type": "mouseReleased", "x": center["x"], "y": center["y"],
                    "button": "left", "buttons": 0, "clickCount": 1
                })
            capture_deadline = time.monotonic() + args.seconds
            while time.monotonic() < capture_deadline:
                pump(min(0.25, capture_deadline - time.monotonic()))
            state = evaluate(
                "({title:document.title, body:document.body.innerText, "
                "canvasHidden:document.getElementById('canvas').hidden, "
                "runtimeCalled:Boolean(Module.calledRun), "
                "storageReady:Boolean(Module.storageReady), "
                "uncaught:Module.browserUncaughtErrors || [], "
                "telemetry:Module.browserTelemetry || null})"
            )
        finally:
            if websocket is not None:
                websocket.close()
            process.terminate()
            try:
                _, chrome_stderr = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                _, chrome_stderr = process.communicate()
            server.shutdown()
            server_thread.join(timeout=5)

    return {
        "url": url,
        "startControlActivated": ready,
        "page": state,
        "devtools": messages,
        "chromeStderr": chrome_stderr.splitlines(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", type=Path, default=DEFAULT_DIST)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--chrome")
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--start-timeout", type=float, default=30.0)
    args = parser.parse_args()
    try:
        evidence = capture(args)
    except Exception as error:
        evidence = {"collectorFailure": str(error)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(args.output)
    return 1 if "collectorFailure" in evidence else 0


if __name__ == "__main__":
    raise SystemExit(main())
