#!/usr/bin/env python3
"""Catalog exact SLP cursor metadata and only proved executable selectors."""

import argparse
import hashlib
import json
import struct
from pathlib import Path


def drs_resource(path, extension, wanted_id):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise ValueError("invalid DRS header")
    table_count = struct.unpack_from("<i", data, 56)[0]
    for table in range(table_count):
        offset = 64 + table * 12
        if offset + 12 > len(data):
            raise ValueError("DRS table outside archive")
        current_extension = data[offset:offset + 4][::-1].decode(
            "ascii", errors="strict"
        ).strip("\0 ").lower()
        info, count = struct.unpack_from("<ii", data, offset + 4)
        for entry in range(count):
            entry_offset = info + entry * 12
            if entry_offset < 0 or entry_offset + 12 > len(data):
                raise ValueError("DRS entry outside archive")
            resource_id, payload, size = struct.unpack_from(
                "<iii", data, entry_offset
            )
            if payload < 0 or size < 0 or payload + size > len(data):
                raise ValueError("DRS payload outside archive")
            if current_extension == extension and resource_id == wanted_id:
                return data[payload:payload + size]
    raise KeyError(f"{extension} resource {wanted_id} absent")


def slp_frames(data):
    if len(data) < 32:
        raise ValueError("short SLP header")
    frame_count = struct.unpack_from("<I", data, 4)[0]
    if frame_count > (len(data) - 32) // 32:
        raise ValueError("SLP frame table is truncated")
    frames = []
    for index in range(frame_count):
        offset = 32 + index * 32
        (
            command_table,
            outline_table,
            palette_offset,
            properties,
            width,
            height,
            hotspot_x,
            hotspot_y,
        ) = struct.unpack_from("<IIIIiiii", data, offset)
        if width <= 0 or height <= 0:
            raise ValueError("invalid SLP frame dimensions")
        if command_table + height * 4 > len(data):
            raise ValueError("SLP command table is truncated")
        if outline_table + height * 4 > len(data):
            raise ValueError("SLP outline table is truncated")
        frames.append({
            "frame_index": index,
            "width": width,
            "height": height,
            "hotspot_x": hotspot_x,
            "hotspot_y": hotspot_y,
            "metadata_classification": "exact_archive_header",
            "context_state": None,
            "context_state_classification": "unknown",
        })
    return frames


def make_catalog(interfac_path, executable_path=None):
    payload = drs_resource(interfac_path, "slp", 51000)
    archive = Path(interfac_path).read_bytes()
    source = {
        "interfac_drs": Path(interfac_path).name,
        "interfac_drs_sha256": hashlib.sha256(archive).hexdigest(),
        "slp_resource_id": 51000,
        "slp_payload_sha256": hashlib.sha256(payload).hexdigest(),
    }
    if executable_path is not None:
        executable = Path(executable_path).read_bytes()
        source.update({
            "hd_executable": Path(executable_path).name,
            "hd_executable_sha256": hashlib.sha256(executable).hexdigest(),
        })
    frames = slp_frames(payload)
    proved_contexts = 0
    if executable_path is not None:
        frames[0]["context_state"] = "normal"
        frames[0]["context_state_classification"] = (
            "exact_hd_executable_selector"
        )
        frames[6]["context_state"] = "modal_busy"
        frames[6]["context_state_classification"] = (
            "exact_hd_executable_selector"
        )
        proved_contexts = 2
    return {
        "schema": "aoe-cursor-catalog-v1",
        "source": source,
        "executable_evidence": {
            "filename": "mcursors.shp",
            "resource_id": 51000,
            "resource_binding_classification": "exact_hd_executable_load",
            "decompile_evidence": (
                'ppuStack_78 = 0xc738; pcStack_7c = "mcursors.shp"; '
                "FUN_005e5140()"
            ),
            "state_to_frame_contract": "partial_exact_hd_executable",
            "classic_aoc_state_mapping": "unknown",
            "manager_selector": {
                "function": "FUN_004dcca0",
                "vtable_offset": "0x1c",
                "behavior": (
                    "accepts 0 <= frame < 19 and stores the frame directly"
                ),
                "cadence": (
                    "none; selected frame persists until another selector"
                ),
            },
            "proved_selectors": [
                {
                    "state": "normal",
                    "frame_index": 0,
                    "callsite": "FUN_005b2f20",
                    "evidence": (
                        "LoadCursorA(IDC_ARROW); FUN_004ed910(0)"
                    ),
                },
                {
                    "state": "modal_busy",
                    "frame_index": 6,
                    "callsite": "FUN_005b2ec0",
                    "evidence": (
                        "LoadCursorA(IDC_WAIT); FUN_004ed910(6)"
                    ),
                },
            ],
            "unproved_gameplay_states": [
                "select", "move", "attack", "gather", "build", "repair",
                "heal", "convert", "invalid", "scroll_north",
                "scroll_north_east", "scroll_east", "scroll_south_east",
                "scroll_south", "scroll_south_west", "scroll_west",
                "scroll_north_west",
            ],
            "visibility": {
                "show": "FUN_004ed780 -> manager vtable +0x38",
                "hide": "FUN_004ed7e0 -> manager vtable +0x34",
                "classification": "exact_hd_executable_control_flow",
            },
        },
        "summary": {
            "frame_count": len(frames),
            "frames_with_exact_dimensions": len(frames),
            "frames_with_exact_hotspots": len(frames),
            "frames_with_proved_context_state": proved_contexts,
        },
        "frames": frames,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("interfac_drs")
    parser.add_argument("--hd-executable")
    parser.add_argument(
        "--output", default="generated/cursor_catalog.json"
    )
    args = parser.parse_args()
    result = make_catalog(args.interfac_drs, args.hd_executable)
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
