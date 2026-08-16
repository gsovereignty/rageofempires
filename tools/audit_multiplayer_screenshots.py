#!/usr/bin/env python3
"""Automatically audit captured multiplayer PNGs and render provenance."""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path

from PIL import Image, ImageChops

from visual_overlap_audit import audit_images


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
    with Image.open(path) as source:
        image = source.convert("RGB")
    width, height = image.size
    pixel_count = width * height
    luminance = image.convert("L")
    luminance_histogram = luminance.histogram()
    near_black = sum(luminance_histogram[:9])
    minimum_luminance, maximum_luminance = luminance.getextrema()
    hud_top = min(565, max(0, height * 3 // 4))
    hud = image.crop((0, hud_top, width, height))
    red, green, blue = hud.split()
    brightest_channel = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    hud_bright = sum(brightest_channel.histogram()[150:])
    hud_color_count = hud.getcolors(maxcolors=12)
    hud_unique_colors = 12 if hud_color_count is None else len(hud_color_count)
    findings: list[dict[str, object]] = []
    if near_black >= pixel_count * 0.985 or (
        maximum_luminance - minimum_luminance < 12
    ):
        findings.append({"status": "FAIL", "kind": "black_frame"})
    if height >= 700 and (hud_unique_colors < 12 or hud_bright < 100):
        findings.append({
            "status": "FAIL", "kind": "hud_corruption",
            "hudUniqueColors": hud_unique_colors, "hudBrightPixels": hud_bright,
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
            if (not entity.get("layers") and
                    not isinstance(entity.get("renderPosition"), dict)):
                # Authoritative candidates outside viewport have no draw
                # submission and therefore no sprite provenance to audit.
                continue
            source = str(entity.get("source", ""))
            status = str(entity.get("expectedAssetStatus", ""))
            layers = entity.get("layers") or []
            expected = set(entity.get("expectedResourceIds") or [])
            actual = {layer.get("resourceId") for layer in layers}
            primary_resource = (
                layers[0].get("resourceId") if layers else None
            )
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
            elif expected and primary_resource not in expected:
                findings.append({**common, "status": "FAIL",
                                 "kind": "wrong_resource_mapping",
                                 "expected": sorted(expected),
                                 "actual": sorted(actual)})
    return findings


def overlap_findings(root: Path) -> tuple[list[dict[str, object]], int]:
    """Recompute every matched overlap case; never trust a stored verdict."""
    manifest_path = root / "overlap" / "manifest.json"
    if not manifest_path.is_file():
        return ([{"status": "BLOCKED", "kind": "overlap_evidence",
                  "reason": "missing overlap/manifest.json"}], 0)
    try:
        manifest = json.loads(manifest_path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        return ([{"status": "BLOCKED", "kind": "overlap_evidence",
                  "reason": f"invalid manifest: {error}"}], 0)
    cases = manifest.get("cases")
    if not isinstance(cases, list) or not cases:
        return ([{"status": "BLOCKED", "kind": "overlap_evidence",
                  "reason": "manifest contains no cases"}], 0)
    findings: list[dict[str, object]] = []
    audited = 0
    overlap_root = manifest_path.parent
    for index, case in enumerate(cases):
        case_id = str(case.get("id") or f"case-{index + 1}")
        common = {"case": case_id, "manifest": "overlap/manifest.json"}
        try:
            paths = {
                key: overlap_root / str(case[key])
                for key in ("actual", "terrain", "sprite")
            }
            placement = (int(case["x"]), int(case["y"]))
            if any(not path.is_file() for path in paths.values()):
                missing = [key for key, path in paths.items()
                           if not path.is_file()]
                raise ValueError("missing matched input(s): " + ", ".join(missing))
            with Image.open(paths["actual"]) as actual, \
                    Image.open(paths["terrain"]) as terrain, \
                    Image.open(paths["sprite"]) as sprite:
                if sprite.mode != "RGBA":
                    raise ValueError("isolated sprite must be RGBA")
                report, annotated = audit_images(
                    actual, terrain, sprite, placement,
                    alpha_threshold=int(case.get("alpha_threshold", 96)),
                    terrain_threshold=int(case.get("terrain_threshold", 12)),
                    composite_threshold=int(case.get("composite_threshold", 24)),
                    minimum_area=int(case.get("minimum_area", 4)),
                )
            output = overlap_root / "reports" / case_id
            output.mkdir(parents=True, exist_ok=True)
            report.update({
                "case": case_id,
                "evidence": {
                    key: str(path.relative_to(root))
                    for key, path in paths.items()
                },
            })
            (output / "report.json").write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n"
            )
            annotated.save(output / "annotated.png", format="PNG")
            audited += 1
            if report["status"] == "overlap_detected":
                findings.append({
                    **common, "status": "FAIL", "kind": "sprite_overlap",
                    "report": str((output / "report.json").relative_to(root)),
                    "overlapPixels": report["overlap_pixels"],
                })
        except (KeyError, TypeError, ValueError, OSError) as error:
            findings.append({**common, "status": "BLOCKED",
                             "kind": "overlap_evidence", "reason": str(error)})
    return findings, audited


def audit(root: Path) -> dict[str, object]:
    findings = provenance_findings(root)
    screenshots = sorted((root / "frames").glob("**/*.png"))
    screenshots += sorted(root.glob("*.png"))
    for path in screenshots:
        for finding in pixel_findings(path):
            findings.append({**finding, "path": str(path.relative_to(root))})
    overlap, overlap_cases = overlap_findings(root)
    findings.extend(overlap)
    status = "FAIL" if any(item["status"] == "FAIL" for item in findings) \
        else "BLOCKED" if findings else "PASS"
    return {
        "schemaVersion": 1, "status": status,
        "screenshotsAudited": len(screenshots),
        "overlapCasesAudited": overlap_cases, "findings": findings,
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
