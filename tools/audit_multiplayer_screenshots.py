#!/usr/bin/env python3
"""Automatically audit captured multiplayer PNGs and render provenance."""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path


FAIL_SOURCES = {
    "intentional_procedural", "procedural", "procedural_or_unproven",
    "fallback", "placeholder", "synthetic", "debug", "missing",
}


def read_png(path: Path) -> tuple[int, int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    offset = 8
    width = height = color_type = bit_depth = -1
    compressed = bytearray()
    while offset < len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(
                ">IIBB", payload[:10]
            )
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break
    channels = {2: 3, 6: 4}.get(color_type)
    if bit_depth != 8 or channels is None:
        raise ValueError(
            f"unsupported PNG format: depth={bit_depth} color={color_type}"
        )
    packed = zlib.decompress(compressed)
    stride = width * channels
    rows = bytearray()
    previous = bytearray(stride)
    cursor = 0
    for _ in range(height):
        filter_type = packed[cursor]
        cursor += 1
        raw = packed[cursor:cursor + stride]
        cursor += stride
        row = bytearray(stride)
        for index, value in enumerate(raw):
            left = row[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 0:
                decoded = value
            elif filter_type == 1:
                decoded = value + left
            elif filter_type == 2:
                decoded = value + above
            elif filter_type == 3:
                decoded = value + ((left + above) // 2)
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above),
                             abs(estimate - upper_left))
                decoded = value + (left, above, upper_left)[distances.index(
                    min(distances)
                )]
            else:
                raise ValueError(f"unsupported PNG filter: {filter_type}")
            row[index] = decoded & 255
        rows.extend(row)
        previous = row
    return width, height, channels, bytes(rows)


def pixel_findings(path: Path) -> list[dict[str, object]]:
    width, height, channels, pixels = read_png(path)
    pixel_count = width * height
    near_black = 0
    minimum_luminance = 255
    maximum_luminance = 0
    hud_top = min(565, max(0, height * 3 // 4))
    hud_bright = 0
    hud_colors: set[tuple[int, int, int]] = set()
    for pixel_index, index in enumerate(range(0, len(pixels), channels)):
        red, green, blue = pixels[index:index + 3]
        luminance = (red * 3 + green * 6 + blue) // 10
        near_black += luminance <= 8
        minimum_luminance = min(minimum_luminance, luminance)
        maximum_luminance = max(maximum_luminance, luminance)
        if pixel_index // width >= hud_top:
            hud_bright += max(red, green, blue) >= 150
            if len(hud_colors) < 12:
                hud_colors.add((red, green, blue))
    findings: list[dict[str, object]] = []
    if near_black >= pixel_count * 0.985 or (
        maximum_luminance - minimum_luminance < 12
    ):
        findings.append({"status": "FAIL", "kind": "black_frame"})
    if height >= 700 and (len(hud_colors) < 12 or hud_bright < 100):
        findings.append({
            "status": "FAIL", "kind": "hud_corruption",
            "hudUniqueColors": len(hud_colors), "hudBrightPixels": hud_bright,
        })
    return findings


def provenance_findings(root: Path) -> list[dict[str, object]]:
    path = root / "sprite-provenance.jsonl"
    findings: list[dict[str, object]] = []
    if not path.is_file():
        return [{"status": "BLOCKED", "kind": "missing_provenance"}]
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        record = json.loads(line)
        for entity in record.get("entities", []):
            source = str(entity.get("source", ""))
            status = str(entity.get("expectedAssetStatus", ""))
            layers = entity.get("layers") or []
            expected = set(entity.get("expectedResourceIds") or [])
            actual = {layer.get("resourceId") for layer in layers}
            common = {
                "line": line_number, "peer": record.get("peer"),
                "frame": record.get("frame"), "entityId": entity.get("id"),
                "category": entity.get("category"),
            }
            if source in FAIL_SOURCES or "procedural" in source:
                findings.append({**common, "status": "FAIL",
                                 "kind": "procedural_render", "source": source})
            elif source != "legacy":
                findings.append({**common, "status": "FAIL",
                                 "kind": "missing_sprite", "source": source})
            elif status != "renderable" or not layers:
                findings.append({**common, "status": "FAIL",
                                 "kind": "missing_sprite", "mapping": status})
            elif expected and not actual.issubset(expected):
                findings.append({**common, "status": "FAIL",
                                 "kind": "wrong_resource_mapping",
                                 "expected": sorted(expected),
                                 "actual": sorted(actual)})
    return findings


def audit(root: Path) -> dict[str, object]:
    findings = provenance_findings(root)
    screenshots = sorted((root / "frames").glob("**/*.png"))
    screenshots += sorted(root.glob("*.png"))
    for path in screenshots:
        for finding in pixel_findings(path):
            findings.append({**finding, "path": str(path.relative_to(root))})
    # Terrain-over-sprite overlap cannot be inferred from one composited PNG.
    # Require matched terrain-only and isolated RGBA inputs; absent inputs are
    # explicit blocked evidence, never a clean verdict.
    overlap_reports = sorted(root.glob("overlap/**/report.json"))
    if not overlap_reports:
        findings.append({"status": "BLOCKED", "kind": "overlap_evidence"})
    else:
        for path in overlap_reports:
            report = json.loads(path.read_text())
            if report.get("status") == "overlap_detected":
                findings.append({"status": "FAIL", "kind": "sprite_overlap",
                                 "path": str(path.relative_to(root)),
                                 "overlapPixels": report.get("overlap_pixels")})
    status = "FAIL" if any(item["status"] == "FAIL" for item in findings) \
        else "BLOCKED" if findings else "PASS"
    return {
        "schemaVersion": 1, "status": status,
        "screenshotsAudited": len(screenshots), "findings": findings,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = audit(args.run)
    output = args.output or args.run / "screenshot-audit.json"
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps({key: report[key] for key in (
        "status", "screenshotsAudited"
    )}, sort_keys=True))
    return 1 if report["status"] == "FAIL" else 2 if report["status"] == "BLOCKED" else 0


if __name__ == "__main__":
    raise SystemExit(main())
