#!/usr/bin/env python3
"""Join every DAT sound item to exact WAV resources in classic DRS archives."""

import argparse
import hashlib
import json
import struct
from pathlib import Path


def read_drs_wavs(path):
    """Return WAV entries keyed by resource ID, including their exact bytes."""
    data = Path(path).read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise ValueError(f"invalid DRS header: {path}")
    table_count = struct.unpack_from("<i", data, 56)[0]
    result = {}
    for table in range(table_count):
        table_offset = 64 + table * 12
        if table_offset + 12 > len(data):
            raise ValueError(f"DRS table outside archive: {path}")
        extension = data[table_offset:table_offset + 4][::-1].decode(
            "ascii", errors="strict"
        ).strip("\0 ").lower()
        info_offset, count = struct.unpack_from(
            "<ii", data, table_offset + 4
        )
        if info_offset < 0 or count < 0:
            raise ValueError(f"invalid DRS table: {path}")
        for entry in range(count):
            entry_offset = info_offset + entry * 12
            if entry_offset + 12 > len(data):
                raise ValueError(f"DRS entry outside archive: {path}")
            resource_id, payload_offset, size = struct.unpack_from(
                "<iii", data, entry_offset
            )
            if payload_offset < 0 or size < 0 or payload_offset + size > len(data):
                raise ValueError(f"DRS payload outside archive: {path}")
            if extension == "wav":
                if resource_id in result:
                    raise ValueError(
                        f"duplicate WAV ID {resource_id} inside {path}"
                    )
                result[resource_id] = data[payload_offset:payload_offset + size]
    return result


def wav_format(data):
    """Read factual RIFF/WAVE format fields without decoding audio."""
    result = {"riff_wave": False}
    if len(data) < 12 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        return result
    result["riff_wave"] = True
    offset = 12
    while offset + 8 <= len(data):
        chunk_id = data[offset:offset + 4]
        chunk_size = struct.unpack_from("<I", data, offset + 4)[0]
        chunk_start = offset + 8
        chunk_end = chunk_start + chunk_size
        if chunk_end > len(data):
            result["truncated_chunk"] = True
            break
        if chunk_id == b"fmt " and chunk_size >= 16:
            (
                result["audio_format"],
                result["channels"],
                result["sample_rate"],
                result["byte_rate"],
                result["block_align"],
                result["bits_per_sample"],
            ) = struct.unpack_from("<HHIIHH", data, chunk_start)
        elif chunk_id == b"data":
            result["data_size"] = chunk_size
        offset = chunk_end + (chunk_size & 1)
    return result


def make_catalog(metadata, archives):
    """Build the exhaustive exact-ID join. Later archives take precedence."""
    resolved = {}
    archive_rows = []
    appearances = {}
    for archive_name, archive_path in archives:
        path = Path(archive_path)
        wavs = read_drs_wavs(path)
        raw = path.read_bytes()
        archive_rows.append({
            "name": archive_name,
            "sha256": hashlib.sha256(raw).hexdigest(),
            "size": len(raw),
            "wav_count": len(wavs),
            "wav_resource_ids": sorted(wavs),
        })
        for resource_id, payload in wavs.items():
            appearances.setdefault(resource_id, []).append(
                (archive_name, payload)
            )
            resolved[resource_id] = (archive_name, payload)

    duplicate_rows = []
    for resource_id, records in sorted(appearances.items()):
        if len(records) > 1:
            duplicate_rows.append({
                "resource_id": resource_id,
                "archives": [name for name, _ in records],
                "byte_identical": len({
                    hashlib.sha256(payload).digest()
                    for _, payload in records
                }) == 1,
                "resolved_archive": records[-1][0],
            })

    referenced = set()
    sounds = []
    for sound in metadata["sounds"]:
        items = []
        for item in sound["items"]:
            resource_id = item["resource_id"]
            if resource_id >= 0:
                referenced.add(resource_id)
            joined = dict(item)
            joined["classification"] = "exact"
            if resource_id < 0:
                joined.update({
                    "available": False,
                    "resolved_archive": None,
                    "wav_size": None,
                    "wav_format": None,
                    "unavailable_reason": "negative DAT sentinel",
                })
                items.append(joined)
                continue
            match = resolved.get(resource_id)
            if match is None:
                joined.update({
                    "available": False,
                    "resolved_archive": None,
                    "wav_size": None,
                    "wav_format": None,
                    "unavailable_reason": "absent from supplied archives",
                })
            else:
                archive_name, payload = match
                joined.update({
                    "available": True,
                    "resolved_archive": archive_name,
                    "wav_size": len(payload),
                    "wav_format": wav_format(payload),
                    "unavailable_reason": None,
                })
            items.append(joined)
        sounds.append({
            "id": sound["id"],
            "play_delay": sound["play_delay"],
            "items": items,
        })

    available_references = referenced & set(resolved)
    return {
        "schema": "aoe-audio-catalog-v1",
        "source": metadata["source"],
        "lookup_contract": {
            "classification": "exact",
            "precedence": [name for name, _ in archives],
            "rule": "later archive replaces earlier archive for identical ID",
        },
        "summary": {
            "dat_sound_count": len(sounds),
            "dat_nonempty_sound_count": sum(bool(x["items"]) for x in sounds),
            "dat_sound_item_count": sum(len(x["items"]) for x in sounds),
            "referenced_unique_wav_count": len(referenced),
            "available_referenced_unique_wav_count": len(available_references),
            "missing_referenced_unique_wav_count": len(referenced - set(resolved)),
            "resolved_archive_unique_wav_count": len(resolved),
            "unreferenced_archive_wav_count": len(set(resolved) - referenced),
            "duplicate_resource_id_count": len(duplicate_rows),
            "byte_identical_duplicate_count": sum(
                row["byte_identical"] for row in duplicate_rows
            ),
        },
        "archives": archive_rows,
        "duplicates": duplicate_rows,
        "missing_referenced_resource_ids": sorted(referenced - set(resolved)),
        "unreferenced_archive_resource_ids": sorted(set(resolved) - referenced),
        "sounds": sounds,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("asset_root")
    parser.add_argument(
        "--output", default="generated/audio_catalog.json"
    )
    args = parser.parse_args()
    root = Path(args.asset_root)
    candidates = [
        ("sounds.drs", root / "Data" / "sounds.drs"),
        ("sounds_x1.drs", root / "Data" / "sounds_x1.drs"),
        ("sounds_x2.drs", root / "Data" / "sounds_x2.drs"),
    ]
    archives = [(name, path) for name, path in candidates if path.is_file()]
    if not archives:
        raise SystemExit("asset root contains no supported sound archive")
    metadata = json.loads(Path(args.metadata).read_text())
    output = make_catalog(metadata, archives)
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
