#!/usr/bin/env python3
"""Emit nonverbatim metadata for supplied classic Petersen AI resources."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def drs_entries(payload: bytes) -> dict[tuple[str, int], bytes]:
    if len(payload) < 64 or payload[40:44] != b"1.00":
        raise ValueError("unsupported or truncated DRS")
    count = struct.unpack_from("<I", payload, 56)[0]
    if count > 256 or 64 + count * 12 > len(payload):
        raise ValueError("DRS table directory outside archive")
    result: dict[tuple[str, int], bytes] = {}
    for table in range(count):
        raw_ext, offset, entries = struct.unpack_from(
            "<4sII", payload, 64 + table * 12
        )
        extension = raw_ext[::-1].decode("ascii").rstrip(" \0").lower()
        if entries > 1_000_000 or offset + entries * 12 > len(payload):
            raise ValueError("DRS entry table outside archive")
        for index in range(entries):
            resource_id, data_offset, size = struct.unpack_from(
                "<iII", payload, offset + index * 12
            )
            if data_offset + size > len(payload):
                raise ValueError("DRS resource outside archive")
            result[(extension, resource_id)] = payload[
                data_offset : data_offset + size
            ]
    return result


def operators(text: str) -> tuple[set[str], set[str]]:
    facts: set[str] = set()
    actions: set[str] = set()
    for match in re.finditer(r"\(defrule\b(.*?)\n\s*\)", text, re.I | re.S):
        body = match.group(1)
        if "=>" not in body:
            continue
        before, after = body.split("=>", 1)
        facts.update(re.findall(r"\(\s*(?:not\s+\()?\s*([\w-]+)", before))
        actions.update(re.findall(r"\(\s*([\w-]+)", after))
    return facts, actions


def audit(path: Path) -> dict[str, object]:
    archive = path.read_bytes()
    entries = drs_entries(archive)
    resources = []
    all_facts: set[str] = set()
    all_actions: set[str] = set()
    for resource_id in range(60001, 60030):
        payload = entries.get(("bina", resource_id))
        if payload is None:
            raise ValueError(f"classic AI resource {resource_id} absent")
        text = payload.decode("cp1252")
        facts, actions = operators(text)
        all_facts.update(facts)
        all_actions.update(actions)
        resources.append({
            "resource_id": resource_id,
            "bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(),
            "line_count": len(text.splitlines()),
            "rule_count": len(re.findall(r"\(defrule\b", text, re.I)),
            "load_count": len(re.findall(r"\(load(?:-random)?\b", text, re.I)),
            "loads": sorted(set(re.findall(
                r'\(load\s+"([^"]+)"\)', text, re.I
            ))),
            "condition_symbols": sorted(set(re.findall(
                r"^#load-if-(?:not-)?defined\s+([\w-]+)",
                text, re.I | re.M
            ))),
        })
    return {
        "schema": "aoe-classic-ai-package-evidence-v1",
        "archive": {
            "filename": path.name,
            "bytes": len(archive),
            "sha256": hashlib.sha256(archive).hexdigest(),
        },
        "resources": resources,
        "fact_operators": sorted(all_facts),
        "action_operators": sorted(all_actions),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    rendered = json.dumps(audit(args.archive), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
