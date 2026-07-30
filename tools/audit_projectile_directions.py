#!/usr/bin/env python3
"""Audit Arrow/Scorpion SLP layouts and pinned angle-transform evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


PINNED_OPENAGE = "9a5a7ccbfc20c2de658fc746462cd4a69aa758ef"
RESOURCES = {
    "arrow_body": (3799, 11, 32),
    "arrow_shadow": (3800, 1, 72),
    "scorpion_body": (3812, 1, 18),
    "scorpion_shadow": (3813, 1, 18),
}


def drs_resource(path: Path, wanted_id: int) -> bytes:
    data = path.read_bytes()
    table_count = struct.unpack_from("<i", data, 56)[0]
    for table in range(table_count):
        table_offset = 64 + table * 12
        extension = data[table_offset:table_offset + 4][::-1]
        info, count = struct.unpack_from("<ii", data, table_offset + 4)
        if extension.strip(b"\0 ") != b"slp":
            continue
        for entry in range(count):
            entry_offset = info + entry * 12
            resource_id, payload, size = struct.unpack_from(
                "<iii", data, entry_offset
            )
            if resource_id == wanted_id:
                return data[payload:payload + size]
    raise KeyError(wanted_id)


def slp_metadata(payload: bytes) -> dict:
    frame_count = struct.unpack_from("<I", payload, 4)[0]
    hotspots = []
    for index in range(frame_count):
        offset = 32 + index * 32
        width, height, hotspot_x, hotspot_y = struct.unpack_from(
            "<iiii", payload, offset + 16
        )
        hotspots.append((width, height, hotspot_x, hotspot_y))
    return {
        "physical_frame_count": frame_count,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "hotspot_bounds": {
            "x_min": min(item[2] for item in hotspots),
            "x_max": max(item[2] for item in hotspots),
            "y_min": min(item[3] for item in hotspots),
            "y_max": max(item[3] for item in hotspots),
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--graphics-drs", type=Path, required=True)
    parser.add_argument("--openage-root", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("generated/projectile_direction_evidence.json"),
    )
    args = parser.parse_args()
    commit = subprocess.check_output(
        ["git", "-C", str(args.openage_root), "rev-parse", "HEAD"],
        text=True,
    ).strip()
    if commit != PINNED_OPENAGE:
        raise SystemExit(f"unexpected openage commit: {commit}")
    exporter = (
        args.openage_root /
        "openage/convert/entity_object/export/metadata_export.py"
    ).read_text(encoding="utf-8")
    phys = (
        args.openage_root / "libopenage/coord/phys.cpp"
    ).read_text(encoding="utf-8")
    required = (
        "degree_step = 360 / angle_count",
        "if degree > 180:",
        "subtex_index = frame_idx + angle_index * frame_count",
    )
    if any(item not in exporter for item in required) or \
            "std::atan2(det, dot)" not in phys:
        raise SystemExit("pinned transform evidence missing")

    resources = {}
    for name, (slp_id, frames, angles) in RESOURCES.items():
        metadata = slp_metadata(
            drs_resource(args.graphics_drs, slp_id)
        )
        expected = frames * (angles // 2 + 1)
        metadata.update({
            "slp_id": slp_id,
            "frames_per_angle": frames,
            "logical_angle_count": angles,
            "expected_half_plus_center_frames": expected,
            "layout_matches": (
                metadata["physical_frame_count"] == expected
            ),
        })
        resources[name] = metadata

    report = {
        "openage_commit": commit,
        "transform": {
            "front_vector": [-1, 1],
            "angle_formula": (
                "atan2(-(dx + dy), dy - dx), normalized to 0..360"
            ),
            "quantization": "nearest logical angle center",
            "mirror": "logical degrees above 180 mirror horizontally",
            "frame_layout": (
                "frame + stored_angle * frames_per_angle"
            ),
            "mirrored_draw_hotspot_x": "width - hotspot_x",
        },
        "resources": resources,
        "classification": {
            "scorpion_bolt": (
                "proved" if
                resources["scorpion_body"]["layout_matches"] and
                resources["scorpion_shadow"]["layout_matches"]
                else "fail_closed"
            ),
            "arrow": "fail_closed",
            "arrow_reason": (
                "body physical frame count does not match "
                "half-plus-center DAT layout"
            ),
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
