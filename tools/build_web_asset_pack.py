#!/usr/bin/env python3
"""Build deterministic fixed-scenario browser asset closure."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
from collections import deque
from pathlib import Path


FIXTURE = Path("resources/browser-risk-spike.json")
DRS_HEADER_SIZE = 64
DRS_TABLE_SIZE = 12
DRS_ENTRY_SIZE = 12


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def bytes_sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def normalized_extension(raw: bytes) -> str:
    return raw[::-1].decode("ascii").strip().lower()


def read_drs(path: Path) -> tuple[bytes, dict[tuple[str, int], bytes]]:
    archive = path.read_bytes()
    if len(archive) < DRS_HEADER_SIZE:
        raise ValueError(f"DRS header is truncated: {path}")
    if not archive[40:44].startswith(b"1."):
        raise ValueError(f"unsupported DRS version: {path}")
    table_count = struct.unpack_from("<i", archive, 56)[0]
    if table_count < 0:
        raise ValueError(f"invalid DRS table count: {path}")
    table_end = DRS_HEADER_SIZE + table_count * DRS_TABLE_SIZE
    if table_end > len(archive):
        raise ValueError(f"DRS table directory is truncated: {path}")

    resources: dict[tuple[str, int], bytes] = {}
    for table_index in range(table_count):
        table_offset = DRS_HEADER_SIZE + table_index * DRS_TABLE_SIZE
        extension = normalized_extension(archive[table_offset : table_offset + 4])
        info_offset, entry_count = struct.unpack_from(
            "<ii", archive, table_offset + 4
        )
        if info_offset < 0 or entry_count < 0:
            raise ValueError(f"invalid DRS table metadata: {path}")
        info_end = info_offset + entry_count * DRS_ENTRY_SIZE
        if info_end > len(archive):
            raise ValueError(f"DRS file index is truncated: {path}")
        for entry_index in range(entry_count):
            entry_offset = info_offset + entry_index * DRS_ENTRY_SIZE
            resource_id, data_offset, data_size = struct.unpack_from(
                "<iii", archive, entry_offset
            )
            if min(resource_id, data_offset, data_size) < 0:
                raise ValueError(f"invalid DRS file entry: {path}")
            data_end = data_offset + data_size
            if data_end > len(archive):
                raise ValueError(f"DRS resource is truncated: {path}")
            key = (extension, resource_id)
            if key in resources:
                raise ValueError(f"duplicate DRS resource {extension}/{resource_id}")
            resources[key] = archive[data_offset:data_end]
    return archive[:DRS_HEADER_SIZE], resources


def write_drs_subset(
    source: Path,
    destination: Path,
    selections: dict[str, list[int]],
) -> list[dict[str, object]]:
    header, available = read_drs(source)
    requested = {
        (extension.strip().lower(), int(resource_id))
        for extension, resource_ids in selections.items()
        for resource_id in resource_ids
    }
    missing = sorted(requested - set(available))
    if missing:
        values = ", ".join(f"{extension}/{resource_id}" for extension, resource_id in missing)
        raise ValueError(f"missing DRS dependencies in {source}: {values}")

    grouped: dict[str, list[tuple[int, bytes]]] = {}
    for extension, resource_id in sorted(requested):
        grouped.setdefault(extension, []).append(
            (resource_id, available[(extension, resource_id)])
        )
    table_count = len(grouped)
    index_offset = DRS_HEADER_SIZE + table_count * DRS_TABLE_SIZE
    payload_offset = index_offset + len(requested) * DRS_ENTRY_SIZE
    output = bytearray(header)
    struct.pack_into("<ii", output, 56, table_count, payload_offset)

    table_bytes = bytearray()
    index_bytes = bytearray()
    payload_bytes = bytearray()
    records: list[dict[str, object]] = []
    next_index = index_offset
    next_payload = payload_offset
    for extension in sorted(grouped):
        encoded = extension.encode("ascii")
        if not 1 <= len(encoded) <= 4:
            raise ValueError(f"invalid DRS extension: {extension}")
        table_bytes.extend(encoded.ljust(4, b" ")[::-1])
        table_bytes.extend(struct.pack("<ii", next_index, len(grouped[extension])))
        for resource_id, payload in grouped[extension]:
            index_bytes.extend(struct.pack("<iii", resource_id, next_payload, len(payload)))
            payload_bytes.extend(payload)
            records.append(
                {
                    "extension": extension,
                    "resource_id": resource_id,
                    "sha256": bytes_sha256(payload),
                    "byte_size": len(payload),
                }
            )
            next_index += DRS_ENTRY_SIZE
            next_payload += len(payload)
    output.extend(table_bytes)
    output.extend(index_bytes)
    output.extend(payload_bytes)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)
    return records


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
        drs_entries = node.get("drs_entries")
        if drs_entries is not None:
            if not isinstance(drs_entries, dict) or not all(
                isinstance(extension, str)
                and isinstance(resource_ids, list)
                and all(isinstance(resource_id, int) for resource_id in resource_ids)
                for extension, resource_ids in drs_entries.items()
            ):
                raise ValueError(f"asset node has invalid DRS entries: {node_name}")
            included_entries = write_drs_subset(source, destination, drs_entries)
        else:
            shutil.copyfile(source, destination)
            included_entries = None
        record = {
            "source_relative_path": source_relative.as_posix(),
            "sha256": sha256(destination),
            "byte_size": destination.stat().st_size,
            "inclusion_reason": reason,
            "dependency_parent": parent,
        }
        if included_entries is not None:
            record["source_archive_sha256"] = sha256(source)
            record["included_entries"] = included_entries
        records.append(record)

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
