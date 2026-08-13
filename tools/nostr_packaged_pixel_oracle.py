#!/usr/bin/env python3
"""Evaluate captured production pixels against independently decoded SLPs."""

from __future__ import annotations

import hashlib
import json
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance

from nostr_slp_decoder import (
    DECODER_VERSION,
    decode_slp_frame,
    drs_resource,
    parse_jasc_palette,
)
from nostr_visual_frame_oracle import expected_frame
from nostr_visual_pixel_oracle import (
    DEFAULT_MAXIMUM_EXPECTED_SCORE,
    evaluate_direction_pixels,
    write_evidence,
)


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


def render_selection_overlay(
    canvas_size: tuple[int, int], metadata: dict[str, object]
) -> Image.Image | None:
    """Reconstruct production selection diamond from submitted geometry."""
    overlay = metadata.get("selection_overlay")
    if overlay is None:
        return None
    if not isinstance(overlay, dict):
        raise PackagedPixelOracleError("selection overlay metadata is invalid")
    required = {
        "center", "half_width", "half_height", "color",
        "shadow_draw_order", "marker_draw_order",
    }
    if missing := sorted(required - overlay.keys()):
        raise PackagedPixelOracleError(
            "selection overlay metadata missing: " + ", ".join(missing)
        )
    center = overlay["center"]
    color = overlay["color"]
    if not isinstance(center, list) or len(center) != 2 or \
            not isinstance(color, list) or len(color) != 4:
        raise PackagedPixelOracleError("selection overlay vectors are invalid")
    if tuple(int(value) for value in color) != (250, 220, 65, 255):
        raise PackagedPixelOracleError("selection overlay color is not selected")
    if int(overlay["marker_draw_order"]) <= int(
        overlay["shadow_draw_order"]
    ):
        raise PackagedPixelOracleError("selection overlay order is invalid")
    zoom = float(metadata["zoom"])
    center_x = float(center[0]) * zoom
    center_y = float(center[1]) * zoom
    half_width = float(overlay["half_width"]) * zoom
    half_height = float(overlay["half_height"]) * zoom
    if half_width <= 0 or half_height <= 0:
        raise PackagedPixelOracleError("selection overlay extent is invalid")
    canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    shadow = [
        (center_x, center_y - half_height - zoom),
        (center_x + half_width + zoom, center_y),
        (center_x, center_y + half_height + zoom),
        (center_x - half_width - zoom, center_y),
        (center_x, center_y - half_height - zoom),
    ]
    marker = [
        (center_x, center_y - half_height),
        (center_x + half_width, center_y),
        (center_x, center_y + half_height),
        (center_x - half_width, center_y),
        (center_x, center_y - half_height),
    ]
    line_width = max(1, round(zoom))
    draw.line(shadow, fill=(38, 26, 12, 230), width=line_width)
    draw.line(marker, fill=(250, 220, 65, 255), width=line_width)
    return canvas


def evaluate_packaged_capture(
    *, manifest_path: Path, graphics_drs: Path, interface_drs: Path,
    expected_logical_direction: int, evidence_directory: Path,
    maximum_expected_score: float = DEFAULT_MAXIMUM_EXPECTED_SCORE,
) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    cases = manifest.get("cases", [])
    if len(cases) != 1:
        raise PackagedPixelOracleError("capture must contain exactly one case")
    case = cases[0]
    metadata = case.get("metadata", {})
    draws = metadata.get("sprite_frames", [])
    if not draws:
        raise PackagedPixelOracleError("direction oracle captured no layers")
    required = (
        "resource_id", "frame", "palette_player", "flip_horizontal",
        "visible", "ground", "action_frame", "frames_per_direction",
        "direction_count", "mirroring_mode", "physical_frame_count",
    )
    for layer_index, draw in enumerate(draws):
        missing = [field for field in required if field not in draw]
        if missing:
            raise PackagedPixelOracleError(
                f"capture layer {layer_index} metadata missing: " +
                ", ".join(missing)
            )
        if (int(draw["direction_count"]) > 1 and
                int(draw["logical_direction"]) != expected_logical_direction):
            raise PackagedPixelOracleError(
                f"captured layer {layer_index} changed direction before "
                "pixel readback"
            )
    root = manifest_path.parent
    with Image.open(root / case["actual"]) as source:
        actual = source.convert("RGBA")
    with Image.open(root / case["terrain"]) as source:
        background = source.convert("RGBA")
    if actual.size != background.size:
        raise PackagedPixelOracleError("actual and terrain dimensions differ")

    directional_counts = {
        int(draw["direction_count"]) for draw in draws
        if int(draw["direction_count"]) > 1
    }
    if len(directional_counts) != 1:
        raise PackagedPixelOracleError(
            "composite directional layers use incompatible direction counts"
        )
    direction_count = next(iter(directional_counts), 1)
    if not 0 <= expected_logical_direction < direction_count:
        raise PackagedPixelOracleError("expected direction outside layer range")
    alternatives = {
        str(direction): Image.new("RGBA", actual.size, (0, 0, 0, 0))
        for direction in range(direction_count)
    }
    alternative_frames: dict[str, list[dict[str, object]]] = {
        str(direction): [] for direction in range(direction_count)
    }
    asset_inputs: list[dict[str, object]] = []
    graphics_digest = file_sha256(graphics_drs)
    interface_digest = file_sha256(interface_drs)
    interface_payload = drs_resource(interface_drs, "bina", 50500)
    for layer_index, draw in enumerate(draws):
        resource_id = int(draw["resource_id"])
        payload, layer_palette, _, _, _ = packaged_asset_inputs(
            graphics_drs, interface_drs, resource_id
        )
        asset_inputs.append({
            "layer": layer_index, "resourceId": resource_id,
            "resourcePayloadSha256": hashlib.sha256(payload).hexdigest(),
        })
        layer_direction_count = int(draw["direction_count"])
        for direction in range(direction_count):
            layer_direction = direction if layer_direction_count > 1 else 0
            resolved = expected_frame(
                logical_direction_value=layer_direction,
                action_frame=int(draw["action_frame"]),
                frames_per_direction=int(draw["frames_per_direction"]),
                direction_count=layer_direction_count,
                mirroring_mode=int(draw["mirroring_mode"]),
                physical_frame_count=int(draw["physical_frame_count"]),
            )
            key = str(direction)
            alternatives[key].alpha_composite(render_decoded_draw(
                canvas_size=actual.size, payload=payload,
                palette=layer_palette,
                frame_index=resolved.physical_frame,
                legacy_player=int(draw["palette_player"]),
                ground=(float(draw["ground"][0]), float(draw["ground"][1])),
                zoom=float(metadata["zoom"]),
                flip_horizontal=resolved.flip_horizontal,
                visible=bool(draw["visible"]),
            ))
            alternative_frames[key].append({
                "layer": layer_index, "resourceId": resource_id,
                "frame": resolved.physical_frame,
                "storedDirection": resolved.stored_direction,
                "flipHorizontal": resolved.flip_horizontal,
            })
    selection_overlay = render_selection_overlay(actual.size, metadata)
    if selection_overlay is not None:
        for alternative in alternatives.values():
            alternative.alpha_composite(selection_overlay)
    expected_key = str(expected_logical_direction)
    report, images = evaluate_direction_pixels(
        actual=actual, background=background,
        expected_direction=expected_key, sprites=alternatives,
        maximum_expected_score=maximum_expected_score,
    )
    report.update({
        "decoderVersion": DECODER_VERSION,
        "graphicsDrsSha256": graphics_digest,
        "interfaceDrsSha256": interface_digest,
        "paletteResource": 50500,
        "palettePayloadSha256": hashlib.sha256(interface_payload).hexdigest(),
        "resourceId": int(draws[0]["resource_id"]),
        "actualFrame": int(draws[0]["frame"]),
        "actualFlipHorizontal": bool(draws[0]["flip_horizontal"]),
        "actualLayers": [{
            "layer": index, "resourceId": int(draw["resource_id"]),
            "frame": int(draw["frame"]),
            "flipHorizontal": bool(draw["flip_horizontal"]),
            "drawOrder": draw.get("draw_order"),
        } for index, draw in enumerate(draws)],
        "assetInputs": asset_inputs,
        "alternativeFrames": alternative_frames,
        "destination": draws[0].get("destination"),
        "clippedDestination": draws[0].get("clipped_destination"),
        "ground": draws[0]["ground"],
        "zoom": float(metadata["zoom"]),
        "entityId": int(metadata["entity_id"]),
        "tick": int(metadata["tick"]),
        "scenario": metadata.get("scenario"),
        "ownership": int(metadata.get("ownership", -1)),
        "directionCount": direction_count,
        "framesPerDirection": int(draws[0]["frames_per_direction"]),
        "physicalFrameCount": int(draws[0]["physical_frame_count"]),
        "mirroringMode": int(draws[0]["mirroring_mode"]),
        "actionFrame": int(draws[0]["action_frame"]),
        "selectionOverlay": metadata.get("selection_overlay"),
        "selectionOverlayPixelAssertion": selection_overlay is not None,
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
    draws = metadata["sprite_frames"]
    mutated_layer = next(
        (index for index, value in enumerate(draws)
         if int(value["direction_count"]) > 1), None
    )
    if mutated_layer is None:
        raise PackagedPixelOracleError("no directional composite layer")
    draw = draws[mutated_layer]
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
    evidence_directory.mkdir(parents=True, exist_ok=True)
    mutated_actual = background.copy()
    for layer_index, layer in enumerate(draws):
        resource_id = int(layer["resource_id"])
        payload, palette, _, _, _ = packaged_asset_inputs(
            graphics_drs, interface_drs, resource_id
        )
        if layer_index == mutated_layer:
            resolved = wrong_frame
        else:
            layer_direction = (
                expected_logical_direction
                if int(layer["direction_count"]) > 1 else 0
            )
            resolved = expected_frame(
                logical_direction_value=layer_direction,
                action_frame=int(layer["action_frame"]),
                frames_per_direction=int(layer["frames_per_direction"]),
                direction_count=int(layer["direction_count"]),
                mirroring_mode=int(layer["mirroring_mode"]),
                physical_frame_count=int(layer["physical_frame_count"]),
            )
        mutated_actual.alpha_composite(render_decoded_draw(
            canvas_size=background.size, payload=payload, palette=palette,
            frame_index=resolved.physical_frame,
            legacy_player=int(layer["palette_player"]),
            ground=(float(layer["ground"][0]), float(layer["ground"][1])),
            zoom=float(metadata["zoom"]),
            flip_horizontal=resolved.flip_horizontal,
            visible=bool(layer["visible"]),
        ))
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
        maximum_expected_score=0.0,
    )
    report = {
        "schemaVersion": 1,
        "mutation": "wrong-direction-pixels-metadata-unchanged",
        "expectedLogicalDirection": expected_logical_direction,
        "mutatedLogicalDirection": wrong_direction,
        "metadataLogicalDirection": int(draw["logical_direction"]),
        "mutatedLayer": mutated_layer,
        "mutatedResourceId": int(draw["resource_id"]),
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


def write_wrong_position_mutation(
    *, manifest_path: Path, graphics_drs: Path, interface_drs: Path,
    expected_logical_direction: int, evidence_directory: Path,
) -> dict[str, object]:
    """Retain correct sprite pixels shifted from unchanged draw metadata."""
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    case = manifest["cases"][0]
    metadata = case["metadata"]
    draws = metadata["sprite_frames"]
    root = manifest_path.parent
    with Image.open(root / case["terrain"]) as source:
        background = source.convert("RGBA")
    evidence_directory.mkdir(parents=True, exist_ok=True)
    attempts: list[dict[str, object]] = []
    final_verdict = None
    selected_offset = None
    for pixel_offset in (1, 2, 3, 4):
        mutated_actual = background.copy()
        for layer in draws:
            resource_id = int(layer["resource_id"])
            payload, palette, _, _, _ = packaged_asset_inputs(
                graphics_drs, interface_drs, resource_id
            )
            layer_direction = (
                expected_logical_direction
                if int(layer["direction_count"]) > 1 else 0
            )
            resolved = expected_frame(
                logical_direction_value=layer_direction,
                action_frame=int(layer["action_frame"]),
                frames_per_direction=int(layer["frames_per_direction"]),
                direction_count=int(layer["direction_count"]),
                mirroring_mode=int(layer["mirroring_mode"]),
                physical_frame_count=int(layer["physical_frame_count"]),
            )
            zoom = float(metadata["zoom"])
            mutated_actual.alpha_composite(render_decoded_draw(
                canvas_size=background.size, payload=payload,
                palette=palette, frame_index=resolved.physical_frame,
                legacy_player=int(layer["palette_player"]),
                ground=(float(layer["ground"][0]) + pixel_offset / zoom,
                        float(layer["ground"][1])),
                zoom=zoom, flip_horizontal=resolved.flip_horizontal,
                visible=bool(layer["visible"]),
            ))
        attempt_directory = evidence_directory / f"offset-{pixel_offset}"
        attempt_directory.mkdir()
        mutated_actual_path = attempt_directory / "mutated-actual.png"
        mutated_actual.save(mutated_actual_path)
        mutated_manifest = json.loads(json.dumps(manifest))
        mutated_manifest["cases"][0]["actual"] = mutated_actual_path.name
        mutated_manifest["cases"][0]["terrain"] = str(
            (root / case["terrain"]).resolve()
        )
        mutation_manifest_path = attempt_directory / "mutation-manifest.json"
        mutation_manifest_path.write_text(
            json.dumps(mutated_manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        verdict = evaluate_packaged_capture(
            manifest_path=mutation_manifest_path,
            graphics_drs=graphics_drs, interface_drs=interface_drs,
            expected_logical_direction=expected_logical_direction,
            evidence_directory=attempt_directory / "oracle",
            maximum_expected_score=0.0,
        )
        attempts.append({
            "pixelOffsetX": pixel_offset, "verdict": verdict["verdict"],
            "oracleReport": f"offset-{pixel_offset}/oracle/report.json",
        })
        if verdict["verdict"] == "FAIL":
            final_verdict = verdict
            selected_offset = pixel_offset
            break
    report = {
        "schemaVersion": 1,
        "mutation": "correct-pixels-wrong-position-metadata-unchanged",
        "expectedLogicalDirection": expected_logical_direction,
        "selectedPixelOffsetX": selected_offset,
        "verdict": final_verdict["verdict"] if final_verdict else "BLOCKED",
        "attempts": attempts,
    }
    (evidence_directory / "mutation.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    if final_verdict is None:
        raise PackagedPixelOracleError(
            "wrong-position packaged mutation did not fail"
        )
    return report
