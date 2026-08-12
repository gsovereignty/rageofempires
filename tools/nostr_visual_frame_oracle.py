#!/usr/bin/env python3
"""Independent audit oracle for logical direction to physical SLP frame."""

from __future__ import annotations

import math
from dataclasses import dataclass


FORMULA_VERSION = "aok-fun-00510160-v1"
EVIDENCE_REFERENCE = (
    "docs/fidelity/ANIMATION_FIDELITY.md#logical-angle-and-physical-frame-selection"
)


class FrameOracleError(ValueError):
    """Input cannot describe a proved DAT/SLP directional layout."""


@dataclass(frozen=True)
class ExpectedFrame:
    logical_direction: int
    stored_direction: int
    first_physical_frame: int
    last_physical_frame: int
    physical_frame: int
    flip_horizontal: bool
    formula_version: str = FORMULA_VERSION
    evidence_reference: str = EVIDENCE_REFERENCE


def logical_direction(
    previous: tuple[int, int],
    current: tuple[int, int],
    direction_count: int,
) -> int:
    """Quantize authoritative tile displacement independently of game code."""
    if direction_count <= 0 or direction_count > 256:
        raise FrameOracleError("invalid direction count")
    dx = current[0] - previous[0]
    dy = current[1] - previous[1]
    if dx == 0 and dy == 0:
        raise FrameOracleError("stationary displacement has no new direction")
    radians = math.atan2(dy - dx, dx + dy)
    if radians < 0.0:
        radians += 2.0 * math.pi
    step = 2.0 * math.pi / direction_count
    return int(math.floor(radians / step + 0.5)) % direction_count


def expected_frame(
    *,
    logical_direction_value: int,
    action_frame: int,
    frames_per_direction: int,
    direction_count: int,
    mirroring_mode: int,
    physical_frame_count: int,
) -> ExpectedFrame:
    """Apply pinned original selector formula without calling production code."""
    if direction_count <= 0 or direction_count > 256:
        raise FrameOracleError("invalid direction count")
    if frames_per_direction <= 0:
        raise FrameOracleError("invalid frames per direction")
    if physical_frame_count <= 0:
        raise FrameOracleError("invalid physical frame count")
    if not 0 <= logical_direction_value < direction_count:
        raise FrameOracleError("logical direction out of range")
    if not 0 <= action_frame < frames_per_direction:
        raise FrameOracleError("action frame out of range")
    if mirroring_mode < 0 or mirroring_mode >= direction_count:
        raise FrameOracleError("invalid mirroring mode")

    if mirroring_mode == 0:
        stored_count = direction_count
    elif direction_count == 2:
        stored_count = 1
    else:
        quarter = direction_count // 4
        if mirroring_mode < quarter:
            raise FrameOracleError("invalid mirroring mode")
        stored_count = mirroring_mode - quarter + 1
    if physical_frame_count != frames_per_direction * stored_count:
        raise FrameOracleError("physical frame count contradicts layout")

    stored_direction = logical_direction_value
    flip_horizontal = False
    if mirroring_mode != 0 and direction_count == 2:
        stored_direction = 0
        flip_horizontal = logical_direction_value != 0
    elif mirroring_mode != 0:
        quarter = direction_count // 4
        if (logical_direction_value > mirroring_mode or
                logical_direction_value < quarter):
            half = direction_count // 2
            stored_direction = (
                (half - logical_direction_value + direction_count)
                if logical_direction_value > half
                else (half - logical_direction_value)
            ) - quarter
            flip_horizontal = True
        else:
            stored_direction = logical_direction_value - quarter
    if stored_direction < 0 or stored_direction >= stored_count:
        raise FrameOracleError("stored direction out of range")

    first = stored_direction * frames_per_direction
    last = first + frames_per_direction - 1
    return ExpectedFrame(
        logical_direction=logical_direction_value,
        stored_direction=stored_direction,
        first_physical_frame=first,
        last_physical_frame=last,
        physical_frame=first + action_frame,
        flip_horizontal=flip_horizontal,
    )


def evaluate_layer(
    *,
    previous: tuple[int, int],
    current: tuple[int, int],
    direction_count: int,
    frames_per_direction: int,
    physical_frame_count: int,
    mirroring_mode: int,
    action_frame: int,
    actual_frame: int,
    actual_flip_horizontal: bool,
    actual_stored_direction: int | None = None,
) -> dict[str, object]:
    direction = logical_direction(previous, current, direction_count)
    expected = expected_frame(
        logical_direction_value=direction,
        action_frame=action_frame,
        frames_per_direction=frames_per_direction,
        direction_count=direction_count,
        mirroring_mode=mirroring_mode,
        physical_frame_count=physical_frame_count,
    )
    return {
        "verdict": (
            "PASS" if actual_frame == expected.physical_frame and
            actual_flip_horizontal == expected.flip_horizontal and
            (actual_stored_direction is None or
             actual_stored_direction == expected.stored_direction)
            else "FAIL"
        ),
        "movementVector": {
            "x": current[0] - previous[0],
            "y": current[1] - previous[1],
        },
        "logicalDirection": expected.logical_direction,
        "storedDirection": expected.stored_direction,
        "actualStoredDirection": actual_stored_direction,
        "firstPhysicalFrame": expected.first_physical_frame,
        "lastPhysicalFrame": expected.last_physical_frame,
        "expectedFrame": expected.physical_frame,
        "actualFrame": actual_frame,
        "expectedFlipHorizontal": expected.flip_horizontal,
        "actualFlipHorizontal": actual_flip_horizontal,
        "formulaVersion": expected.formula_version,
        "evidenceReference": expected.evidence_reference,
    }
