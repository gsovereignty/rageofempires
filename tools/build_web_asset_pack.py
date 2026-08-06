#!/usr/bin/env python3
"""Build deterministic fixed-scenario browser asset closure."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from collections import deque
from pathlib import Path


FIXTURE = Path("resources/browser-risk-spike.json")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def destination_for(source: Path, output_root: Path) -> Path:
    if source.parts[0] == "resources":
        return output_root / source
    if source.parts[0] == "game_data":
        return output_root / source
    raise ValueError(f"asset source outside supported roots: {source}")


def build_pack(source_root: Path, output_root: Path) -> dict[str, object]:
    metadata_path = source_root / FIXTURE
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    graph = metadata["asset_graph"]
    if "risk-scenario" not in graph:
        raise ValueError("asset graph has no risk-scenario root")

    if output_root.exists():
        shutil.rmtree(output_root)
    (output_root / "resources").mkdir(parents=True)
    (output_root / "game_data").mkdir(parents=True)

    queue: deque[tuple[str, str | None]] = deque([("risk-scenario", None)])
    visited: set[str] = set()
    records: list[dict[str, object]] = []
    while queue:
        node_name, parent = queue.popleft()
        if node_name in visited:
            continue
        if node_name not in graph:
            raise ValueError(f"missing dependency node: {node_name}")
        visited.add(node_name)
        node = graph[node_name]
        reason = node.get("reason")
        dependencies = node.get("dependencies")
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"asset node has no reason: {node_name}")
        if not isinstance(dependencies, list) or not all(
            isinstance(value, str) for value in dependencies
        ):
            raise ValueError(f"asset node has invalid dependencies: {node_name}")
        for dependency in sorted(dependencies):
            queue.append((dependency, node_name))

        source_name = node.get("source")
        if source_name is None:
            continue
        source_relative = Path(source_name)
        source = source_root / source_relative
        if not source.is_file():
            raise FileNotFoundError(f"missing browser asset: {source_relative}")
        destination = destination_for(source_relative, output_root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        records.append(
            {
                "source_relative_path": source_relative.as_posix(),
                "sha256": sha256(source),
                "byte_size": source.stat().st_size,
                "inclusion_reason": reason,
                "dependency_parent": parent,
            }
        )

    unreachable = sorted(set(graph) - visited)
    if unreachable:
        raise ValueError(f"unreachable asset nodes: {', '.join(unreachable)}")
    records.sort(key=lambda record: record["source_relative_path"])
    manifest: dict[str, object] = {
        "schema": 1,
        "root": "risk-scenario",
        "total_bytes": sum(int(record["byte_size"]) for record in records),
        "records": records,
    }
    maximum = int(metadata["budgets"]["maximum_packaged_asset_bytes"])
    if int(manifest["total_bytes"]) > maximum:
        raise ValueError(
            f"browser asset pack is {manifest['total_bytes']} bytes; budget is {maximum}"
        )
    manifest_path = output_root / "web_asset_manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=Path.cwd())
    parser.add_argument(
        "--output-root", type=Path, default=Path("build-web/web-assets")
    )
    arguments = parser.parse_args()
    build_pack(arguments.source_root.resolve(), arguments.output_root.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
