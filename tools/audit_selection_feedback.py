#!/usr/bin/env python3
"""Extract bounded classic selection/health feedback fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from collections import Counter, defaultdict
from pathlib import Path


SQUARE_BACK = 0x00587E00
SQUARE_FRONT = 0x00587FF0
CUBE_BACK = 0x005882C0
CUBE_FRONT = 0x00588730
HD_FEEDBACK_DRAW = 0x0058BF30
SELECT_OBJECT = 0x00583C90
CLEAR_SELECTION = 0x00583E00
HEALTH_BACKGROUND = 0x0081D163
HEALTH_FILL = 0x0081D164
QUARTER = 0x00774EAC
THREE_QUARTERS = 0x00772F20


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pe_sections(data: bytes) -> list[tuple[int, int, int]]:
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    table = pe + 24 + optional_size
    sections = []
    for index in range(count):
        item = table + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, item + 8
        )
        sections.append((
            virtual_address,
            max(virtual_size, raw_size),
            raw_offset,
        ))
    return sections


def at_va(data: bytes, address: int, size: int) -> bytes:
    rva = address - 0x00400000
    for start, length, raw in pe_sections(data):
        if start <= rva and rva + size <= start + length:
            offset = raw + rva - start
            return data[offset:offset + size]
    raise ValueError(f"VA {address:#x} outside PE sections")


def require_at(data: bytes, address: int, expected: bytes) -> None:
    actual = at_va(data, address, len(expected))
    if actual != expected:
        raise ValueError(
            f"VA {address:#x}: expected {expected.hex()}, got {actual.hex()}"
        )


def executable_evidence(path: Path) -> dict:
    data = path.read_bytes()
    # Function starts and decisive operands. These fail closed if another build
    # is accidentally supplied.
    require_at(data, SQUARE_BACK, bytes.fromhex("83ec18538bd9"))
    require_at(data, SQUARE_FRONT, bytes.fromhex("83ec24538bd9"))
    require_at(data, CUBE_BACK, bytes.fromhex("83ec2c538bd9"))
    require_at(data, CUBE_FRONT, bytes.fromhex("83ec2c538bd9"))
    require_at(data, HD_FEEDBACK_DRAW, bytes.fromhex("83ec5c"))
    require_at(data, SELECT_OBJECT, bytes.fromhex("53558b6c240c"))
    require_at(data, CLEAR_SELECTION, bytes.fromhex("5333db"))
    require_at(data, 0x005881EA, bytes.fromhex("8b542428d94230"))
    require_at(data, 0x0058824D, bytes.fromhex("0fbf5a2a"))
    require_at(data, 0x00588255, bytes.fromhex("0fb61563d18100"))
    require_at(data, 0x00588286, bytes.fromhex("0fb60564d18100"))
    require_at(data, 0x0058828E, bytes.fromhex("8d446d0003c003c003c0"))
    require_at(data, QUARTER, struct.pack("<f", 0.25))
    require_at(data, THREE_QUARTERS, struct.pack("<f", 0.75))
    if at_va(data, HEALTH_BACKGROUND, 1)[0] != 36:
        raise ValueError("unexpected health background palette index")
    if at_va(data, HEALTH_FILL, 1)[0] != 241:
        raise ValueError("unexpected health fill palette index")

    strings = {}
    for name in (
        b"stat_obj::draw_frame_3d_square_back",
        b"stat_obj::draw_frame_3d_square_front",
        b"stat_obj::draw_frame_3d_cube_back",
        b"stat_obj::draw_frame_3d_cube_front",
        b"health.shp",
        b"unithalo.shp",
    ):
        strings[name.decode()] = data.find(name)
    return {
        "filename": path.name,
        "sha256": sha256(path),
        "bytes": len(data),
        "string_offsets": strings,
        "functions": {
            "square_back": f"0x{SQUARE_BACK:08x}",
            "square_front_and_health": f"0x{SQUARE_FRONT:08x}",
            "cube_back": f"0x{CUBE_BACK:08x}",
            "cube_front": f"0x{CUBE_FRONT:08x}",
            "hd_feedback_draw": f"0x{HD_FEEDBACK_DRAW:08x}",
            "select_object": f"0x{SELECT_OBJECT:08x}",
            "clear_selection": f"0x{CLEAR_SELECTION:08x}",
        },
    }


def unit_feedback(metadata: Path) -> dict:
    root = json.loads(metadata.read_text())
    by_id: dict[int, set[tuple]] = defaultdict(set)
    for civilization in root["civilizations"]:
        for unit in civilization["units"]:
            if unit.get("disabled"):
                continue
            values = (
                unit["selection_shape"],
                tuple(unit["outline_radius"]),
                tuple(unit["radius"]),
                unit["hit_points"],
                unit["selected_sound"],
            )
            by_id[unit["id"]].add(values)

    exact = []
    variants = []
    shape_counts: Counter[int] = Counter()
    for unit_id, records in sorted(by_id.items()):
        if len(records) != 1:
            variants.append({"unit_id": unit_id, "variant_count": len(records)})
            continue
        shape, outline, radius, hp, sound = next(iter(records))
        shape_counts[shape] += 1
        exact.append({
            "unit_id": unit_id,
            "selection_shape": shape,
            "outline_radius": list(outline),
            "collision_radius": list(radius),
            "maximum_hit_points": hp,
            "selected_sound": sound,
        })
    return {
        "metadata_filename": metadata.name,
        "metadata_sha256": sha256(metadata),
        "cross_civilization_exact_records": exact,
        "civilization_variant_records": variants,
        "exact_selection_shape_counts": {
            str(key): value for key, value in sorted(shape_counts.items())
        },
    }


def archive_feedback(ui_catalog: Path) -> dict:
    root = json.loads(ui_catalog.read_text())
    found: dict[int, dict] = {}

    def visit(value: object) -> None:
        if isinstance(value, dict):
            resource_id = value.get("resource_id")
            if resource_id in {50403, 50404, 50405, 50745, 53003}:
                found[resource_id] = {
                    key: value[key]
                    for key in ("extension", "resource_id", "size", "frame_count")
                    if key in value
                }
            for child in value.values():
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(root)
    expected_frames = {50403: 9, 50404: 1, 50745: 26, 53003: 1}
    for resource_id, frame_count in expected_frames.items():
        if found.get(resource_id, {}).get("frame_count") != frame_count:
            raise ValueError(
                f"resource {resource_id}: expected {frame_count} frames"
            )
    if 50405 in found:
        raise ValueError("resource 50405 unexpectedly present")
    return {
        "catalog_filename": ui_catalog.name,
        "catalog_sha256": sha256(ui_catalog),
        "resources": {
            str(resource_id): found.get(resource_id)
            for resource_id in (50403, 50404, 50405, 50745, 53003)
        },
    }


def make_catalog(
    executable: Path,
    metadata: Path,
    ui_catalog: Path,
) -> dict:
    return {
        "schema": "aoe-classic-selection-feedback-v1",
        "sources": {
            "executable": executable_evidence(executable),
            "dat_metadata": unit_feedback(metadata),
            "archive_inventory": archive_feedback(ui_catalog),
        },
        "procedural_original_primitives": {
            "application_dispatch": {
                "cube": "renderer +0xe8 == 1 or application +0x78 == 1",
                "square": "application +0x78 in [2, 3]",
                "hardware": (
                    "hardware available, renderer +0xe8 != 1, and "
                    "application +0x78 in [2, 3]"
                ),
                "hardware_mode_2_radii": "outline",
                "hardware_mode_3_radii": "collision",
                "numeric_mode_names_proved": False,
            },
            "selection_pen": {
                "win32_stock_object": 6,
                "semantic": "WHITE_PEN",
                "palette_or_player_ramp": False,
            },
            "square": {
                "radii": ["master.outline_radius_x", "master.outline_radius_y"],
                "back_segments": 2,
                "front_segments": 2,
                "projection_z": 0.0,
                "screen_origin": [
                    "draw_x + object.screen_offset_x",
                    "draw_y + object.screen_offset_y",
                ],
            },
            "cube": {
                "radii": [
                    "master.outline_radius_x",
                    "master.outline_radius_y",
                    "master.outline_radius_z",
                ],
                "corner_trim_factors": [0.25, 0.75],
                "screen_y_extra_offset": -16,
                "back_segments": 6,
                "front_segments": 18,
            },
            "health_bar": {
                "current_hit_points": "truncate_toward_zero(object.current_hp_float)",
                "maximum_hit_points": "signed_short(master.maximum_hp)",
                "draw_gate": "current_hp_integer > 0 and maximum_hp > 0",
                "anchor_world": [
                    "master.outline_radius_x",
                    "-master.outline_radius_y",
                    "master.object_height",
                ],
                "background_inclusive_offsets": [-12, -2, 12, -1],
                "background_pixels": [25, 2],
                "fill_right_offset": "-12 + trunc(current_hp * 24 / maximum_hp)",
                "fill_pixels_at_full_health": [25, 2],
                "color_thresholds": [],
                "background_palette_index": 36,
                "background_rgb": [255, 0, 0],
                "fill_palette_index": 241,
                "fill_rgb": [0, 255, 0],
            },
            "hd_selection_state": {
                "selected_flag": "object.feedback_flags bit 0",
                "proof": (
                    "select_object appends object to a maximum-40 selection "
                    "list and writes feedback_flags=1; clear_selection writes 0"
                ),
                "selected_overlay_gate": (
                    "bit0 set and bit3 clear; health additionally requires "
                    "master.flags_0xb8 bit1 clear"
                ),
                "base_color_palette_indices": {
                    "default": 255,
                    "alternate_mode": 133,
                    "bit3_with_bit0": 243,
                },
                "flashing_overrides": {
                    "bit1": {
                        "primary_palette_index": 241,
                        "support_palette_index": 36,
                        "phase_mask_milliseconds": 256,
                    },
                    "bit2": {
                        "primary_palette_index": 243,
                        "support_palette_index": 36,
                        "phase_mask_milliseconds": 256,
                    },
                },
                "hd_health_raw_geometry": {
                    "anchor_world": [
                        "selected_radius_x",
                        "-selected_radius_y",
                        "selected_radius_z",
                    ],
                    "left_offset": -16,
                    "top_offset": -3,
                    "right_offset": 15,
                    "bottom_offset": 0,
                    "fill_split": (
                        "clamp(left + trunc(current_hp * 32 / maximum_hp), "
                        "right - 2)"
                    ),
                },
            },
        },
        "asset_backed_candidates": {
            "health.shp": {
                "resource_id": 0xC639,
                "load_proved": True,
                "archive_frame_count": 26,
                "world_health_bar_role_proved": False,
            },
            "unithalo.shp": {
                "resource_id": 0xCF0B,
                "load_proved": True,
                "archive_frame_count": 1,
                "selection_or_hover_role_proved": False,
            },
            "groupnum.shp": {
                "resource_id": 0xC4E3,
                "load_proved": True,
                "archive_frame_count": 9,
                "selected_group_number_role_proved": True,
                "logical_groups": [1, 9],
                "logical_to_physical_frame": "group_number - 1",
                "glyph_advance_x": 8,
                "draw_gate": (
                    "selected bit 0; bit 3 clear; nonzero group mask; "
                    "local ownership"
                ),
            },
            "waypoint.shp": {
                "resource_id": 0xC4E4,
                "load_proved": True,
                "archive_frame_count": 1,
                "destination_or_path_role_proved": False,
            },
            "moveto.shp": {
                "resource_id": 0xC4E5,
                "load_proved": True,
                "archive_present": False,
                "destination_or_order_role_proved": False,
            },
        },
        "unproved": {
            "selection_shape_dispatch": (
                "DAT shape values and square/cube virtual methods are exact, "
                "but no recovered callsite maps shape IDs to methods"
            ),
            "visibility_and_state_gates": (
                "selected bit and selected-overlay gates are exact; hovered, "
                "ownership, visibility, and fog callsite gates are not recovered"
            ),
            "order_feedback": (
                "destination flags, order lines, waypoint colors, and range "
                "rings are not linked to original executable primitives"
            ),
            "asset_roles": (
                "health.shp and unithalo.shp loads are exact; their draw "
                "calls and world-feedback states are not"
            ),
            "selected_sound_trigger": (
                "DAT selected_sound IDs are exact; this catalog does not "
                "promote a hover/click/change trigger"
            ),
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--ui-catalog", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/selection_feedback_catalog.json"),
    )
    args = parser.parse_args()
    args.output.write_text(
        json.dumps(
            make_catalog(args.executable, args.metadata, args.ui_catalog),
            indent=2,
        ) + "\n"
    )


if __name__ == "__main__":
    main()
