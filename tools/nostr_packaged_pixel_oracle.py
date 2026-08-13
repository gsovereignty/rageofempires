#!/usr/bin/env python3
"""Evaluate captured production pixels against independently decoded SLPs."""

from __future__ import annotations

import hashlib
import json
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageEnhance

from nostr_slp_decoder import (
    DECODER_VERSION,
    decode_slp_frame,
    drs_resource,
    parse_jasc_palette,
)
from nostr_visual_frame_oracle import expected_frame
from nostr_visual_pixel_oracle import evaluate_direction_pixels, write_evidence


PLAYER_PALETTE_BASES = (16, 32, 48, 64, 96, 112, 128, 80)


class PackagedPixelOracleError(ValueError):
    """Captured frame lacks evidence needed for independent pixel verdict."""


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


@lru_cache(maxsize=16)
def packaged_asset_inputs(
    graphics_drs: Path, interface_drs: Path, resource_id: int,
) -> tuple[bytes, list[tuple[int, int, int]], bytes, str, str]:
    interface_payload = drs_resource(interface_drs, "bina", 50500)
    return (
        drs_resource(graphics_drs, "slp", resource_id),
        parse_jasc_palette(interface_payload),
        interface_payload,
        file_sha256(graphics_drs),
        file_sha256(interface_drs),
    )


def palette_base(legacy_player: int) -> int:
    if legacy_player == 0:
        return 0
    if not 1 <= legacy_player <= len(PLAYER_PALETTE_BASES):
        raise PackagedPixelOracleError("unsupported legacy palette player")
    return PLAYER_PALETTE_BASES[legacy_player - 1]


def render_decoded_draw(
    *, canvas_size: tuple[int, int], payload: bytes,
    palette: list[tuple[int, int, int]], frame_index: int,
    legacy_player: int, ground: tuple[float, float], zoom: float,
    flip_horizontal: bool, visible: bool,
) -> Image.Image:
    decoded = decode_slp_frame(
        payload, palette, frame_index, palette_base(legacy_player)
    )
    sprite = decoded.image
    if flip_horizontal:
        sprite = sprite.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        hotspot_x = sprite.width - 1 - decoded.hotspot_x
    else:
        hotspot_x = decoded.hotspot_x
    if not visible:
        rgb = ImageEnhance.Brightness(sprite.convert("RGB")).enhance(82 / 255)
        rgb.putalpha(sprite.getchannel("A"))
        sprite = rgb
    width = max(1, round(sprite.width * zoom))
    height = max(1, round(sprite.height * zoom))
    sprite = sprite.resize((width, height), Image.Resampling.NEAREST)
    left = round((ground[0] - hotspot_x) * zoom)
    top = round((ground[1] - decoded.hotspot_y) * zoom)
    canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    canvas.alpha_composite(sprite, (left, top))
    return canvas


def evaluate_packaged_capture(
    *, manifest_path: Path, graphics_drs: Path, interface_drs: Path,
    expected_logical_direction: int, evidence_directory: Path,
) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cases = manifest.get("cases", [])
    if len(cases) != 1:
        raise PackagedPixelOracleError("capture must contain exactly one case")
    case = cases[0]
    metadata = case.get("metadata", {})
    draws = metadata.get("sprite_frames", [])
    if len(draws) != 1:
        raise PackagedPixelOracleError(
            "direction oracle currently requires one captured sprite layer"
        )
    draw = draws[0]
    required = (
        "resource_id", "frame", "palette_player", "flip_horizontal",
        "visible", "ground", "action_frame", "frames_per_direction",
        "direction_count", "mirroring_mode", "physical_frame_count",
    )
    missing = [field for field in required if field not in draw]
    if missing:
        raise PackagedPixelOracleError(
            f"capture draw metadata missing: {', '.join(missing)}"
        )
    if int(draw["logical_direction"]) != expected_logical_direction:
        raise PackagedPixelOracleError(
            "captured draw changed direction before pixel readback"
        )
    root = manifest_path.parent
    with Image.open(root / case["actual"]) as source:
        actual = source.convert("RGBA")
    with Image.open(root / case["terrain"]) as source:
        background = source.convert("RGBA")
    if actual.size != background.size:
        raise PackagedPixelOracleError("actual and terrain dimensions differ")

    resource_id = int(draw["resource_id"])
    payload, palette, interface_payload, graphics_digest, interface_digest = \
        packaged_asset_inputs(graphics_drs, interface_drs, resource_id)
    direction_count = int(draw["direction_count"])
    alternatives: dict[str, Image.Image] = {}
    alternative_frames: dict[str, dict[str, object]] = {}
    for direction in range(direction_count):
        resolved = expected_frame(
            logical_direction_value=direction,
            action_frame=int(draw["action_frame"]),
            frames_per_direction=int(draw["frames_per_direction"]),
            direction_count=direction_count,
            mirroring_mode=int(draw["mirroring_mode"]),
            physical_frame_count=int(draw["physical_frame_count"]),
        )
        key = str(direction)
        alternatives[key] = render_decoded_draw(
            canvas_size=actual.size, payload=payload, palette=palette,
            frame_index=resolved.physical_frame,
            legacy_player=int(draw["palette_player"]),
            ground=(float(draw["ground"][0]), float(draw["ground"][1])),
            zoom=float(metadata["zoom"]),
            flip_horizontal=resolved.flip_horizontal,
            visible=bool(draw["visible"]),
        )
        alternative_frames[key] = {
            "frame": resolved.physical_frame,
            "storedDirection": resolved.stored_direction,
            "flipHorizontal": resolved.flip_horizontal,
        }
    expected_key = str(expected_logical_direction)
    report, images = evaluate_direction_pixels(
        actual=actual, background=background,
        expected_direction=expected_key, sprites=alternatives,
    )
    report.update({
        "decoderVersion": DECODER_VERSION,
        "graphicsDrsSha256": graphics_digest,
        "interfaceDrsSha256": interface_digest,
        "paletteResource": 50500,
        "palettePayloadSha256": hashlib.sha256(interface_payload).hexdigest(),
        "resourceId": resource_id,
        "actualFrame": int(draw["frame"]),
        "actualFlipHorizontal": bool(draw["flip_horizontal"]),
        "alternativeFrames": alternative_frames,
        "destination": draw.get("destination"),
        "clippedDestination": draw.get("clipped_destination"),
        "ground": draw["ground"],
        "zoom": float(metadata["zoom"]),
        "entityId": int(metadata["entity_id"]),
        "tick": int(metadata["tick"]),
        "scenario": metadata.get("scenario"),
        "ownership": int(metadata.get("ownership", -1)),
        "directionCount": direction_count,
        "framesPerDirection": int(draw["frames_per_direction"]),
        "physicalFrameCount": int(draw["physical_frame_count"]),
        "mirroringMode": int(draw["mirroring_mode"]),
        "actionFrame": int(draw["action_frame"]),
    })
    return write_evidence(evidence_directory, report, images)


def write_wrong_direction_mutation(
    *, manifest_path: Path, graphics_drs: Path, interface_drs: Path,
    expected_logical_direction: int, evidence_directory: Path,
) -> dict[str, object]:
    """Retain a metadata-preserving wrong-pixel mutation and its verdict."""
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    case = manifest["cases"][0]
    metadata = case["metadata"]
    draw = metadata["sprite_frames"][0]
    resource_id = int(draw["resource_id"])
    payload, palette, _, _, _ = packaged_asset_inputs(
        graphics_drs, interface_drs, resource_id
    )
    expected = expected_frame(
        logical_direction_value=expected_logical_direction,
        action_frame=int(draw["action_frame"]),
        frames_per_direction=int(draw["frames_per_direction"]),
        direction_count=int(draw["direction_count"]),
        mirroring_mode=int(draw["mirroring_mode"]),
        physical_frame_count=int(draw["physical_frame_count"]),
    )
    wrong_direction = None
    wrong_frame = None
    for direction in range(int(draw["direction_count"])):
        candidate = expected_frame(
            logical_direction_value=direction,
            action_frame=int(draw["action_frame"]),
            frames_per_direction=int(draw["frames_per_direction"]),
            direction_count=int(draw["direction_count"]),
            mirroring_mode=int(draw["mirroring_mode"]),
            physical_frame_count=int(draw["physical_frame_count"]),
        )
        if (candidate.physical_frame, candidate.flip_horizontal) != (
            expected.physical_frame, expected.flip_horizontal
        ):
            wrong_direction, wrong_frame = direction, candidate
            break
    if wrong_frame is None:
        raise PackagedPixelOracleError("no distinct wrong direction mutation")

    root = manifest_path.parent
    with Image.open(root / case["terrain"]) as source:
        background = source.convert("RGBA")
    wrong_sprite = render_decoded_draw(
        canvas_size=background.size, payload=payload, palette=palette,
        frame_index=wrong_frame.physical_frame,
        legacy_player=int(draw["palette_player"]),
        ground=(float(draw["ground"][0]), float(draw["ground"][1])),
        zoom=float(metadata["zoom"]),
        flip_horizontal=wrong_frame.flip_horizontal,
        visible=bool(draw["visible"]),
    )
    evidence_directory.mkdir(parents=True, exist_ok=True)
    mutated_actual = background.copy()
    mutated_actual.alpha_composite(wrong_sprite)
    mutated_actual_path = evidence_directory / "mutated-actual.png"
    mutated_actual.save(mutated_actual_path)
    mutated_manifest = json.loads(json.dumps(manifest))
    mutated_manifest["cases"][0]["actual"] = mutated_actual_path.name
    mutated_manifest["cases"][0]["terrain"] = str(
        (root / case["terrain"]).resolve()
    )
    mutation_manifest_path = evidence_directory / "mutation-manifest.json"
    mutation_manifest_path.write_text(
        json.dumps(mutated_manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    verdict = evaluate_packaged_capture(
        manifest_path=mutation_manifest_path,
        graphics_drs=graphics_drs, interface_drs=interface_drs,
        expected_logical_direction=expected_logical_direction,
        evidence_directory=evidence_directory / "oracle",
    )
    report = {
        "schemaVersion": 1,
        "mutation": "wrong-direction-pixels-metadata-unchanged",
        "expectedLogicalDirection": expected_logical_direction,
        "mutatedLogicalDirection": wrong_direction,
        "metadataLogicalDirection": int(draw["logical_direction"]),
        "verdict": verdict["verdict"],
        "oracleReport": "oracle/report.json",
        "mutatedActual": mutated_actual_path.name,
        "mutationManifest": mutation_manifest_path.name,
    }
    (evidence_directory / "mutation.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    if verdict["verdict"] != "FAIL":
        raise PackagedPixelOracleError(
            "wrong-direction packaged mutation did not fail"
        )
    return report
