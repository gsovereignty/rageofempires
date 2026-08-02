#!/usr/bin/env python3
"""Find terrain drawn over an expected RGBA sprite in a gameplay capture."""

from __future__ import annotations

import argparse
import json
import sys
from collections import deque
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError as error:  # pragma: no cover - exercised only on bad installs
    raise SystemExit("visual overlap audit requires Pillow") from error


def _byte_distance(left: tuple[int, ...], right: tuple[int, ...]) -> int:
    return max(abs(left[index] - right[index]) for index in range(3))


def _composite(
    sprite: tuple[int, ...], terrain: tuple[int, ...]
) -> tuple[int, int, int]:
    alpha = sprite[3]
    inverse = 255 - alpha
    return tuple(
        (sprite[channel] * alpha + terrain[channel] * inverse + 127) // 255
        for channel in range(3)
    )


def _components(
    pixels: set[tuple[int, int]], minimum_area: int
) -> list[dict[str, object]]:
    remaining = set(pixels)
    found: list[dict[str, object]] = []
    neighbors = tuple(
        (dx, dy)
        for dy in (-1, 0, 1)
        for dx in (-1, 0, 1)
        if dx or dy
    )
    while remaining:
        seed = min(remaining, key=lambda point: (point[1], point[0]))
        remaining.remove(seed)
        queue = deque([seed])
        region = [seed]
        while queue:
            x, y = queue.popleft()
            for dx, dy in neighbors:
                candidate = (x + dx, y + dy)
                if candidate in remaining:
                    remaining.remove(candidate)
                    queue.append(candidate)
                    region.append(candidate)
        if len(region) < minimum_area:
            continue
        xs = [point[0] for point in region]
        ys = [point[1] for point in region]
        found.append({
            "area": len(region),
            "bounds": {
                "x": min(xs),
                "y": min(ys),
                "width": max(xs) - min(xs) + 1,
                "height": max(ys) - min(ys) + 1,
            },
            "pixels": sorted(region, key=lambda point: (point[1], point[0])),
        })
    found.sort(key=lambda item: (
        item["bounds"]["y"], item["bounds"]["x"], -item["area"]
    ))
    return found


def audit_images(
    actual: Image.Image,
    terrain: Image.Image,
    sprite: Image.Image,
    placement: tuple[int, int],
    *,
    alpha_threshold: int = 96,
    terrain_threshold: int = 12,
    composite_threshold: int = 24,
    minimum_area: int = 4,
) -> tuple[dict[str, object], Image.Image]:
    """Return deterministic overlap report and annotated gameplay image."""
    if actual.size != terrain.size:
        raise ValueError("actual and terrain-only images must have identical dimensions")
    for name, value in (
        ("alpha threshold", alpha_threshold),
        ("terrain threshold", terrain_threshold),
        ("composite threshold", composite_threshold),
    ):
        if not 0 <= value <= 255:
            raise ValueError(f"{name} must be between 0 and 255")
    if minimum_area < 1:
        raise ValueError("minimum connected area must be at least 1")

    actual = actual.convert("RGBA")
    terrain = terrain.convert("RGBA")
    sprite = sprite.convert("RGBA")
    left, top = placement
    candidates: set[tuple[int, int]] = set()
    compared = 0
    for sprite_y in range(sprite.height):
        screen_y = top + sprite_y
        if screen_y < 0 or screen_y >= actual.height:
            continue
        for sprite_x in range(sprite.width):
            screen_x = left + sprite_x
            if screen_x < 0 or screen_x >= actual.width:
                continue
            sprite_pixel = sprite.getpixel((sprite_x, sprite_y))
            if sprite_pixel[3] < alpha_threshold:
                continue
            compared += 1
            actual_pixel = actual.getpixel((screen_x, screen_y))
            terrain_pixel = terrain.getpixel((screen_x, screen_y))
            expected = _composite(sprite_pixel, terrain_pixel)
            if (
                _byte_distance(actual_pixel, terrain_pixel) <= terrain_threshold
                and _byte_distance(actual_pixel, expected) >= composite_threshold
            ):
                candidates.add((screen_x, screen_y))

    internal_components = _components(candidates, minimum_area)
    annotated = actual.copy()
    draw = ImageDraw.Draw(annotated)
    for component in internal_components:
        region = set(component["pixels"])
        boundary = (
            point
            for point in component["pixels"]
            if any(
                (point[0] + dx, point[1] + dy) not in region
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))
            )
        )
        for point in boundary:
            draw.point(point, fill=(255, 0, 0, 255))

    components = []
    for index, component in enumerate(internal_components, 1):
        components.append({
            "id": index,
            "area": component["area"],
            "bounds": component["bounds"],
        })
    report: dict[str, object] = {
        "schema_version": 1,
        "status": "overlap_detected" if components else "pass",
        "placement": {"x": left, "y": top},
        "thresholds": {
            "alpha": alpha_threshold,
            "terrain_distance": terrain_threshold,
            "composite_difference": composite_threshold,
            "minimum_connected_area": minimum_area,
        },
        "opaque_pixels_compared": compared,
        "candidate_pixels": len(candidates),
        "overlap_pixels": sum(component["area"] for component in components),
        "component_count": len(components),
        "components": components,
    }
    return report, annotated


def _placement(args: argparse.Namespace) -> tuple[int, int]:
    direct = args.x is not None or args.y is not None
    anchored = args.screen_x is not None or args.screen_y is not None
    if direct and anchored:
        raise ValueError("use either --x/--y or --screen-x/--screen-y placement")
    if direct:
        if args.x is None or args.y is None:
            raise ValueError("--x and --y must be supplied together")
        if args.anchor_x or args.anchor_y:
            raise ValueError("--anchor-x/--anchor-y require screen-point placement")
        return args.x, args.y
    if anchored:
        if args.screen_x is None or args.screen_y is None:
            raise ValueError("--screen-x and --screen-y must be supplied together")
        return args.screen_x - args.anchor_x, args.screen_y - args.anchor_y
    raise ValueError("placement required: --x/--y or --screen-x/--screen-y")


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("actual", type=Path, help="actual gameplay image")
    parser.add_argument("terrain", type=Path, help="matching terrain-only image")
    parser.add_argument("sprite", type=Path, help="expected sprite RGBA image")
    parser.add_argument("--x", type=int, help="sprite top-left screen X")
    parser.add_argument("--y", type=int, help="sprite top-left screen Y")
    parser.add_argument("--screen-x", type=int, help="sprite anchor screen X")
    parser.add_argument("--screen-y", type=int, help="sprite anchor screen Y")
    parser.add_argument("--anchor-x", type=int, default=0, help="sprite anchor X")
    parser.add_argument("--anchor-y", type=int, default=0, help="sprite anchor Y")
    parser.add_argument("--alpha-threshold", type=int, default=96)
    parser.add_argument("--terrain-threshold", type=int, default=12)
    parser.add_argument("--composite-threshold", type=int, default=24)
    parser.add_argument("--minimum-area", type=int, default=4)
    parser.add_argument("--json-output", type=Path, required=True)
    parser.add_argument("--annotated-output", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    args = parser.parse_args(argv)
    try:
        placement = _placement(args)
        with Image.open(args.actual) as actual, Image.open(args.terrain) as terrain, Image.open(args.sprite) as sprite:
            report, annotated = audit_images(
                actual,
                terrain,
                sprite,
                placement,
                alpha_threshold=args.alpha_threshold,
                terrain_threshold=args.terrain_threshold,
                composite_threshold=args.composite_threshold,
                minimum_area=args.minimum_area,
            )
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.annotated_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        annotated.save(args.annotated_output, format="PNG")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 1 if report["component_count"] else 0


if __name__ == "__main__":
    sys.exit(main())
