#!/usr/bin/env python3
"""Independent semantic pixel-direction comparison for Nostr audit crops."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops


PIXEL_ORACLE_VERSION = "occlusion-aware-direction-score-v2"
# Exact packaged reconstruction can differ on direction-discriminating pixels
# through interpolation, occlusion edges, and palette composition while the
# expected direction remains the unique best match. Retained production runs
# observed 42.390805 as the highest expected-best score; 48 preserves bounded
# pixel fidelity with deterministic headroom. Synthetic mutations use 0.
DEFAULT_MAXIMUM_EXPECTED_SCORE = 48.0


class PixelOracleError(ValueError):
    """Pixel evidence cannot support a semantic direction verdict."""


def image_digest(image: Image.Image) -> str:
    rgba = image.convert("RGBA")
    return hashlib.sha256(
        rgba.width.to_bytes(4, "little") +
        rgba.height.to_bytes(4, "little") + rgba.tobytes()
    ).hexdigest()


def composite(background: Image.Image, sprite: Image.Image) -> Image.Image:
    if background.size != sprite.size:
        raise PixelOracleError("background and sprite sizes differ")
    result = background.convert("RGBA")
    result.alpha_composite(sprite.convert("RGBA"))
    return result


def discriminating_mask(
    sprites: dict[str, Image.Image], alpha_threshold: int,
) -> Image.Image:
    if len(sprites) < 2:
        raise PixelOracleError("direction oracle needs alternatives")
    sizes = {image.size for image in sprites.values()}
    if len(sizes) != 1:
        raise PixelOracleError("alternative sprite sizes differ")
    values = [image.convert("RGBA") for image in sprites.values()]
    width, height = values[0].size
    mask = bytearray(width * height)
    payloads = [image.tobytes() for image in values]
    for pixel in range(width * height):
        offset = pixel * 4
        rgba = {payload[offset:offset + 4] for payload in payloads}
        visible = any(payload[offset + 3] >= alpha_threshold
                      for payload in payloads)
        mask[pixel] = 255 if visible and len(rgba) > 1 else 0
    return Image.frombytes("L", (width, height), bytes(mask))


def masked_mean_absolute_error(
    actual: Image.Image, expected: Image.Image, mask: Image.Image,
) -> tuple[float, int]:
    if actual.size != expected.size or actual.size != mask.size:
        raise PixelOracleError("pixel comparison sizes differ")
    difference = ImageChops.difference(
        actual.convert("RGB"), expected.convert("RGB")
    )
    mask_bytes = mask.tobytes()
    difference_bytes = difference.tobytes()
    selected = sum(value != 0 for value in mask_bytes)
    if selected == 0:
        return 0.0, 0
    total = sum(
        difference_bytes[index * 3 + channel]
        for index, value in enumerate(mask_bytes) if value
        for channel in range(3)
    )
    return total / (selected * 3.0), selected


def observable_mask(
    actual: Image.Image, sprites: dict[str, Image.Image],
    discriminating: Image.Image, *, alpha_threshold: int,
    color_tolerance: int,
) -> Image.Image:
    """Keep pixels visibly belonging to some candidate, excluding occluders."""
    actual_bytes = actual.convert("RGB").tobytes()
    sprite_bytes = [sprite.convert("RGBA").tobytes()
                    for sprite in sprites.values()]
    base = discriminating.tobytes()
    output = bytearray(len(base))
    for pixel, selected in enumerate(base):
        if not selected:
            continue
        actual_offset = pixel * 3
        actual_rgb = actual_bytes[actual_offset:actual_offset + 3]
        sprite_offset = pixel * 4
        for payload in sprite_bytes:
            if payload[sprite_offset + 3] < alpha_threshold:
                continue
            candidate_rgb = payload[sprite_offset:sprite_offset + 3]
            if max(abs(actual_rgb[channel] - candidate_rgb[channel])
                   for channel in range(3)) <= color_tolerance:
                output[pixel] = 255
                break
    return Image.frombytes("L", actual.size, bytes(output))


def translated_sprite(
    sprite: Image.Image, offset_x: int, offset_y: int,
) -> Image.Image:
    output = Image.new("RGBA", sprite.size, (0, 0, 0, 0))
    output.alpha_composite(sprite.convert("RGBA"), (offset_x, offset_y))
    return output


def alpha_mask(sprite: Image.Image, alpha_threshold: int) -> Image.Image:
    return sprite.convert("RGBA").getchannel("A").point(
        lambda value: 255 if value >= alpha_threshold else 0
    )


def evaluate_direction_pixels(
    *,
    actual: Image.Image,
    background: Image.Image,
    expected_direction: str,
    sprites: dict[str, Image.Image],
    confidence_margin: float = 1.0,
    alpha_threshold: int = 32,
    minimum_discriminating_pixels: int = 48,
    visibility_color_tolerance: int = 0,
    maximum_expected_score: float = DEFAULT_MAXIMUM_EXPECTED_SCORE,
) -> tuple[dict[str, object], dict[str, Image.Image]]:
    if expected_direction not in sprites:
        raise PixelOracleError("expected direction missing from alternatives")
    if confidence_margin < 0:
        raise PixelOracleError("confidence margin must be non-negative")
    if maximum_expected_score < 0:
        raise PixelOracleError("maximum expected score must be non-negative")
    direction_mask = discriminating_mask(sprites, alpha_threshold)
    mask = observable_mask(
        actual, sprites, direction_mask,
        alpha_threshold=alpha_threshold,
        color_tolerance=visibility_color_tolerance,
    )
    composites = {
        direction: composite(background, sprite)
        for direction, sprite in sprites.items()
    }
    scores: dict[str, float] = {}
    discriminating_pixels = 0
    for direction, candidate in composites.items():
        score, count = masked_mean_absolute_error(actual, candidate, mask)
        scores[direction] = score
        discriminating_pixels = count
    ranked = sorted(scores.items(), key=lambda item: (item[1], item[0]))
    best_direction, best_score = ranked[0]
    expected_score = scores[expected_direction]
    runner_up_score = min(
        score for direction, score in ranked
        if direction != expected_direction
    )
    margin = runner_up_score - expected_score
    spatial_scores: dict[str, float] = {}
    spatial_counts: dict[str, int] = {}
    expected_sprite = sprites[expected_direction]
    for offset_y in range(-4, 5):
        for offset_x in range(-4, 5):
            shifted = translated_sprite(expected_sprite, offset_x, offset_y)
            shifted_composite = composite(background, shifted)
            score, count = masked_mean_absolute_error(
                actual, shifted_composite, alpha_mask(shifted, alpha_threshold)
            )
            key = f"{offset_x},{offset_y}"
            spatial_scores[key] = score
            spatial_counts[key] = count
    best_spatial_offset, best_spatial_score = min(
        spatial_scores.items(), key=lambda item: (item[1], item[0])
    )
    displaced_match = (
        best_spatial_offset != "0,0" and
        best_spatial_score <= maximum_expected_score and
        spatial_counts[best_spatial_offset] >= minimum_discriminating_pixels
    )
    if discriminating_pixels < minimum_discriminating_pixels:
        if displaced_match:
            verdict = "FAIL"
            blocker = None
        else:
            verdict = "BLOCKED"
            blocker = "insufficient discriminating pixels"
    elif (best_direction != expected_direction or
          margin < confidence_margin or
          expected_score > maximum_expected_score):
        verdict = "FAIL"
        blocker = None
    else:
        verdict = "PASS"
        blocker = None
    expected_composite = composites[expected_direction]
    difference = ImageChops.difference(
        actual.convert("RGBA"), expected_composite
    )
    report = {
        "schemaVersion": 1,
        "oracleVersion": PIXEL_ORACLE_VERSION,
        "verdict": verdict,
        "blocker": blocker,
        "expectedDirection": expected_direction,
        "bestDirection": best_direction,
        "expectedScore": expected_score,
        "bestScore": best_score,
        "runnerUpScore": runner_up_score,
        "confidenceMargin": margin,
        "requiredConfidenceMargin": confidence_margin,
        "maximumExpectedScore": maximum_expected_score,
        "discriminatingPixels": discriminating_pixels,
        "minimumDiscriminatingPixels": minimum_discriminating_pixels,
        "alphaThreshold": alpha_threshold,
        "visibilityColorTolerance": visibility_color_tolerance,
        "alternativeDirectionScores": scores,
        "bestSpatialOffset": best_spatial_offset,
        "bestSpatialScore": best_spatial_score,
        "spatialOffsetScores": spatial_scores,
        "actualDigest": image_digest(actual),
        "backgroundDigest": image_digest(background),
        "spriteDigests": {
            direction: image_digest(sprite)
            for direction, sprite in sprites.items()
        },
    }
    return report, {
        "actual": actual.convert("RGBA"),
        "expected": expected_composite,
        "difference": difference,
        "discriminatingMask": mask,
    }


def write_evidence(
    root: Path, report: dict[str, object], images: dict[str, Image.Image]
) -> dict[str, object]:
    root.mkdir(parents=True, exist_ok=True)
    paths: dict[str, str] = {}
    for name, image in images.items():
        path = root / f"{name}.png"
        image.save(path, format="PNG")
        paths[name] = path.name
    retained = {**report, "images": paths}
    (root / "report.json").write_text(
        json.dumps(retained, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return retained
