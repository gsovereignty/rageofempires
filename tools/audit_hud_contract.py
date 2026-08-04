#!/usr/bin/env python3
"""Audit exact HUD-sheet evidence and fail closed on unproved layout maps."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


HUD_SLP = 51141
BUTTON_GAME_SLP = 50751
PINNED_EXECUTABLE_SHA256 = (
    "e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def drs_resource(path: Path, wanted_id: int) -> bytes:
    data = path.read_bytes()
    if len(data) < 64:
        raise ValueError("DRS header truncated")
    table_count = struct.unpack_from("<I", data, 56)[0]
    first_table = 64
    if first_table + table_count * 12 > len(data):
        raise ValueError("DRS table directory outside archive")
    for table in range(table_count):
        offset = first_table + table * 12
        extension, entries_offset, entry_count = struct.unpack_from(
            "<4sII", data, offset
        )
        if entries_offset + entry_count * 12 > len(data):
            raise ValueError("DRS entry table outside archive")
        if extension[::-1].strip(b"\0 ") != b"slp":
            continue
        for entry in range(entry_count):
            resource, payload_offset, size = struct.unpack_from(
                "<III", data, entries_offset + entry * 12
            )
            if payload_offset + size > len(data):
                raise ValueError("DRS payload outside archive")
            if resource == wanted_id:
                return data[payload_offset:payload_offset + size]
    raise ValueError(f"DRS resource {wanted_id} absent")


def slp_metadata(payload: bytes) -> dict:
    if len(payload) < 64 or payload[:4] not in {b"2.0N", b"3.0N", b"4.0X"}:
        raise ValueError("invalid SLP header")
    frame_count = struct.unpack_from("<I", payload, 4)[0]
    if frame_count < 1 or 32 + frame_count * 32 > len(payload):
        raise ValueError("SLP frame table outside payload")
    frames = []
    for index in range(frame_count):
        base = 32 + index * 32
        command, outline, palette, properties, width, height, hot_x, hot_y = (
            struct.unpack_from("<IIIIiiii", payload, base)
        )
        if command >= len(payload) or outline >= len(payload):
            raise ValueError("SLP frame pointer outside payload")
        frames.append({
            "index": index,
            "width": width,
            "height": height,
            "hotspot_x": hot_x,
            "hotspot_y": hot_y,
            "properties": properties,
            "command_table_offset": command,
            "outline_table_offset": outline,
            "palette_offset": palette,
        })
    return {
        "version": payload[:4].decode("ascii"),
        "frame_count": frame_count,
        "payload_bytes": len(payload),
        "payload_sha256": sha256(payload),
        "frames": frames,
    }


def executable_evidence(path: Path) -> dict:
    data = path.read_bytes()
    strings = [b"game_b%d.slp", b"map1024.bmp", b"Game Screen"]
    digest = sha256(data)
    return {
        "filename": path.name,
        "bytes": len(data),
        "sha256": digest,
        "pinned_executable": digest == PINNED_EXECUTABLE_SHA256,
        "string_offsets": {
            value.decode(): data.find(value)
            for value in strings
        },
        "proved_call_sites": {
            "background_filename": "0x005f33f1..0x005f341b",
            "loose_only_loader": "0x005f3437..0x005f3446",
            "background_compositor": "0x005e7cb0",
            "large_minimap_frame": "0x005f3ad3",
        } if digest == PINNED_EXECUTABLE_SHA256 else {},
    }


def make_catalog(interface_drs: Path, executable: Path) -> dict:
    archive = interface_drs.read_bytes()
    hud = slp_metadata(drs_resource(interface_drs, HUD_SLP))
    native = (
        hud["frame_count"] == 1 and
        hud["frames"][0]["width"] == 1280 and
        hud["frames"][0]["height"] == 1024
    )
    executable_report = executable_evidence(executable)
    pinned = executable_report["pinned_executable"]
    try:
        button_game = slp_metadata(
            drs_resource(interface_drs, BUTTON_GAME_SLP)
        )
    except ValueError:
        button_game = None
    return {
        "schema": "aoe-hud-layout-contract-v3",
        "sources": {
            "interfac_drs": {
                "filename": interface_drs.name,
                "bytes": len(archive),
                "sha256": sha256(archive),
            },
            "executable": executable_report,
        },
        "unlinked_candidate_sheet": {
            "resource_id": HUD_SLP,
            **hud,
            "native_dimensions_match": native,
            "game_background_compatible": hud["frame_count"] >= 8,
        },
        "renderer_resolution": {
            "window": {"width": 1280, "height": 720},
            "world_viewport": {"x": 0, "y": 0, "width": 1280, "height": 640},
            "hud_band": {"x": 0, "y": 640, "width": 1280, "height": 80},
            "classification": "reconstruction_policy",
        },
        "executable_relative_layout": {
            "classification": (
                "exact_relative" if pinned else "unproved_non_pinned_executable"
            ),
            "callsite": "FUN_005f37c0",
            "vertical_layout": {
                "bottom": "screen_height - stored_bottom_height",
                "optional_top_child": {
                    "x": 0,
                    "y": "stored_top",
                    "width": "screen_width",
                    "height": "top_child_visible ? 30 : 0",
                },
                "main_child": {
                    "x": 0,
                    "y": "stored_top + (top_child_visible ? 30 : 0)",
                    "width": "screen_width",
                    "height": (
                        "bottom - main_child.y + 1"
                    ),
                },
            },
            "command_grid": {
                "count": 15,
                "columns": 5,
                "rows": 3,
                "slot_width": 40,
                "slot_height": 40,
                "x": "37 + 41 * (index % 5)",
                "y": "bottom + 31 + 41 * floor(index / 5)",
                "index_domain": "0..14",
            },
            "other_rectangles": {
                "anchored_large_panel": (
                    "(screen_width - 336, screen_height - 169, 326, 164)"
                ),
                "top_status_strip": "(2, 2, 420, 16)",
                "centered_top_control": (
                    "(screen_width / 2 - 155, 16, 310, 20)"
                ),
                "top_right_controls": (
                    "(screen_width - 260 + 50 * index, 3, 50, 19), "
                    "index 0..4"
                ),
            },
            "resource_status_cells": {
                "count": 5,
                "roles": ["wood", "food", "gold", "stone", "population"],
                "weight_partition": [3, 3, 3, 3, 4],
                "classification": "exact_strip_and_interface_sheet_partition",
            },
            "limits": (
                "stored field producers, absolute world/HUD split, semantic "
                "resolution labels, and panel roles remain unproved"
            ),
        },
        "button_chrome": {
            "classification": (
                "exact_relative" if pinned else "unproved_non_pinned_executable"
            ),
            "sheet": "btngame.shp",
            "resource_id": BUTTON_GAME_SLP,
            "archive_frame_count": (
                button_game["frame_count"] if button_game else None
            ),
            "normal_frame": 36,
            "pressed_frame": 37,
            "pressed_icon_offset": {"x": 1, "y": 1},
            "disabled_state": "hidden",
            "hover_state": "normal",
            "frame_metrics": (
                [button_game["frames"][index] for index in (36, 37)]
                if button_game and button_game["frame_count"] > 37
                else None
            ),
            "limits": (
                "FUN_005c5e40 proves btngame frames 36/37, one-pixel pressed "
                "icon offset, and unchanged normal hover; FUN_005c6050 proves "
                "inactive controls clear capture and remain hidden"
            ),
        },
        "proved": {
            "asset_identity": (
                "interfac.drs contains unlinked SLP 51141 with one "
                "1280x1024 frame"
            ),
            "executable_names": (
                "executable contains game_b%d.slp, map1024.bmp, and Game Screen"
            ),
            "background_filename_selector": (
                "pinned executable formats game_b%d.slp with local player's "
                "civilization byte at player +0x15d"
            ) if pinned else "unproved for non-pinned executable",
            "background_load_source": (
                "pinned executable passes resource ID -1 to the SLP loader, "
                "so game_b%d.slp is loaded from loose slp/ path rather than DRS"
            ) if pinned else "unproved for non-pinned executable",
            "background_composition": (
                "pinned FUN_005e7cb0 composes game_b frames 0 through 7: "
                "frame 0 tiles across top, frame 6 overlays at origin, frames "
                "2/3 alternate across bottom span, frames 1/4 cap its sides, "
                "frame 5 centers, and frame 7 anchors to a sibling view"
            ) if pinned else "unproved for non-pinned executable",
            "candidate_rejection": (
                "SLP 51141 has one frame, while executable compositor requires "
                "game_b frames 0 through 7; it cannot be promoted as game_b"
            ),
            "relative_layout": (
                "pinned FUN_005f37c0 proves field-relative vertical geometry, "
                "a 15-slot 5x3 command grid, and several screen-edge-relative "
                "rectangles; these formulas do not prove absolute split or "
                "semantic panel roles"
            ) if pinned else "unproved for non-pinned executable",
            "button_chrome": (
                "frames 36 and 37 belong to btngame resource 50751, not "
                "btncmd resource 50721; archive metadata and executable "
                "dispatch agree"
            ) if pinned else "unproved for non-pinned executable",
        },
        "unproved": {
            "resolution_to_resource": (
                "game_b selection is by civilization, not resolution; no "
                "authoritative mapping connects its loose files to SLP 51141"
            ),
            "world_hud_split": (
                "FUN_005f37c0 formulas are exact relative to stored fields, "
                "but field producers and loose game_b dimensions do not prove "
                "numeric world/HUD split or resolution-class labels"
            ),
            "crop_anchor_scale": (
                "single-frame SLP 51141 bottom crop/scaling is contradicted by "
                "the proved eight-frame game_b compositor"
            ),
            "panel_rectangles": (
                "portrait, resource, information, minimap, and command panel "
                "rectangles are not linked to exact sheet coordinates"
            ),
            "button_states": (
                "generic normal/pressed chrome is proved; hover, disabled, "
                "command semantics, icon ordering, and hotkey layout are not"
            ),
        },
        "renderer_decision": {
            "exact_layout_enabled": False,
            "candidate_sheet_available": True,
            "candidate_sheet_promoted": False,
            "fallback": (
                "retain procedural panels; do not promote or crop SLP 51141 "
                "as game_b; exact background requires original loose game_b files"
            ),
            "reconstruction_policy": [
                "1280x720 window",
                "640-pixel world viewport",
                "80-pixel HUD band",
                "procedural panel role assignments",
                "absolute placement derived from those reconstruction dimensions",
            ],
            "exact_relative_contract_enabled": pinned,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--interface-drs", type=Path, required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/hud_layout_catalog.json"),
    )
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(
            make_catalog(args.interface_drs, args.executable),
            indent=2,
        ) + "\n"
    )


if __name__ == "__main__":
    main()
