#!/usr/bin/env python3
"""Independent temporal direction-transition oracle for captured render data."""

from __future__ import annotations

from collections import defaultdict

from nostr_visual_frame_oracle import logical_direction


TRANSITION_ORACLE_VERSION = "authoritative-step-transition-v1"


def evaluate_transitions(
    samples: list[dict[str, object]], *, maximum_stale_frames: int = 1,
) -> dict[str, object]:
    if maximum_stale_frames < 0:
        raise ValueError("maximum stale frames must be non-negative")
    histories: dict[tuple[str, int, int], list[dict[str, object]]] = defaultdict(list)
    failures: list[dict[str, object]] = []
    transition_count = 0
    for sample_index, sample in enumerate(samples):
        for peer in ("host", "join"):
            state = sample.get(peer) or {}
            for entity in state.get("entities", []):
                if not isinstance(entity, dict) or not entity.get("layers"):
                    continue
                previous = entity.get("previousPosition")
                current = entity.get("simulationPosition")
                if not isinstance(previous, dict) or not isinstance(current, dict):
                    continue
                previous_position = (int(previous["x"]), int(previous["y"]))
                current_position = (int(current["x"]), int(current["y"]))
                if previous_position == current_position:
                    continue
                layer = entity["layers"][0]
                direction_count = int(layer.get(
                    "directionCount", entity.get("expectedDirectionCount", 8)
                ))
                expected = logical_direction(
                    previous_position, current_position, direction_count
                )
                actual = int(layer.get(
                    "logicalDirection", entity.get("facing", -1)
                ))
                key = (peer, int(entity.get("owner", -1)),
                       int(entity.get("id", -1)))
                observation = {
                    "sampleIndex": sample_index,
                    "tick": state.get("tick"),
                    "renderFrame": state.get("frame"),
                    "previousPosition": list(previous_position),
                    "currentPosition": list(current_position),
                    "expectedDirection": expected,
                    "actualDirection": actual,
                    "actionFrame": layer.get("actionFrame"),
                    "physicalFrame": layer.get("frame"),
                }
                history = histories[key]
                prior = history[-1] if history else None
                if prior and prior["expectedDirection"] != expected:
                    transition_count += 1
                    if actual == prior["expectedDirection"]:
                        failures.append({
                            "classification": "STALE_DIRECTION_AFTER_TURN",
                            "peer": peer, "owner": key[1], "entity": key[2],
                            "fromDirection": prior["expectedDirection"],
                            "toDirection": expected, **observation,
                        })
                    reverse = (
                        int(expected) + direction_count // 2
                    ) % direction_count
                    if actual == reverse:
                        failures.append({
                            "classification": "REVERSE_FACING_FLASH",
                            "peer": peer, "owner": key[1], "entity": key[2],
                            "fromDirection": prior["expectedDirection"],
                            "toDirection": expected, **observation,
                        })
                if actual != expected:
                    consecutive = 1
                    for older in reversed(history):
                        if older["expectedDirection"] != expected or \
                                older["actualDirection"] == expected:
                            break
                        consecutive += 1
                    if consecutive > maximum_stale_frames:
                        failures.append({
                            "classification": "PRESENTATION_LATENCY_EXCEEDED",
                            "peer": peer, "owner": key[1], "entity": key[2],
                            "staleFrameCount": consecutive,
                            "maximumStaleFrames": maximum_stale_frames,
                            **observation,
                        })
                history.append(observation)
    return {
        "schemaVersion": 1,
        "oracleKind": "temporal-direction-transition",
        "oracleVersion": TRANSITION_ORACLE_VERSION,
        "verdict": "FAIL" if failures else "PASS",
        "transitionCount": transition_count,
        "observationCount": sum(len(value) for value in histories.values()),
        "maximumStaleFrames": maximum_stale_frames,
        "failures": failures,
    }
