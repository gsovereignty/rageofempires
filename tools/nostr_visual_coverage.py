#!/usr/bin/env python3
"""Expand and evaluate tracked Nostr visual gameplay coverage cells."""

from __future__ import annotations

import itertools
import json
from pathlib import Path


KEY_FIELDS = (
    "peer", "owner", "unitKind", "action", "directionCount",
    "logicalDirection", "mirrored", "transitionKind",
)


def cell_key(cell: dict[str, object]) -> str:
    return "|".join(str(cell[field]).lower() for field in KEY_FIELDS)


def load_specification(path: Path) -> dict[str, object]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if value.get("schemaVersion") != 1:
        raise ValueError("unsupported visual coverage specification")
    if int(value.get("minimumSamplesPerCell", 0)) < 1:
        raise ValueError("coverage minimum must be positive")
    if not value.get("requiredMatrices"):
        raise ValueError("coverage specification has no required matrices")
    return value


def required_cells(specification: dict[str, object]) -> list[dict[str, object]]:
    dimensions = (
        ("peer", "peers"), ("owner", "owners"),
        ("unitKind", "unitKinds"), ("action", "actions"),
        ("directionCount", "directionCounts"),
        ("logicalDirection", "logicalDirections"),
        ("mirrored", "mirrored"),
        ("transitionKind", "transitionKinds"),
    )
    result: list[dict[str, object]] = []
    for matrix in specification["requiredMatrices"]:
        values = [matrix[source] for _, source in dimensions]
        for combination in itertools.product(*values):
            result.append({
                target: value
                for (target, _), value in zip(dimensions, combination,
                                              strict=True)
            })
    keys = [cell_key(cell) for cell in result]
    if len(keys) != len(set(keys)):
        raise ValueError("coverage specification contains duplicate cells")
    return result


def evaluate_coverage(
    specification: dict[str, object],
    oracle_records: list[dict[str, object]],
) -> dict[str, object]:
    required = required_cells(specification)
    minimum = int(specification["minimumSamplesPerCell"])
    observed: dict[str, dict[str, object]] = {}
    failures: list[int] = []
    for index, record in enumerate(oracle_records):
        if record.get("verdict") != "PASS":
            failures.append(index)
        normalized = {
            "peer": record.get("peer"),
            "owner": record.get("owner"),
            "unitKind": record.get("unitKind"),
            "action": record.get("action"),
            "directionCount": record.get("directionCount"),
            "logicalDirection": record.get("logicalDirection"),
            "mirrored": int(record.get("mirroringMode", 0)) != 0,
            "transitionKind": record.get(
                "transitionKind", "authoritative-step"
            ),
        }
        key = cell_key(normalized)
        cell = observed.setdefault(key, {
            **normalized, "sampleCount": 0,
            "firstTick": record.get("tick"),
            "lastTick": record.get("tick"),
            "entityIds": [], "oracleRecordIndexes": [], "screenshots": [],
        })
        cell["sampleCount"] = int(cell["sampleCount"]) + 1
        cell["lastTick"] = record.get("tick")
        if record.get("entity") not in cell["entityIds"]:
            cell["entityIds"].append(record.get("entity"))
        cell["oracleRecordIndexes"].append(index)
        screenshot = record.get("screenshot")
        if screenshot and screenshot not in cell["screenshots"]:
            cell["screenshots"].append(screenshot)

    missing = []
    for cell in required:
        key = cell_key(cell)
        sample_count = int(observed.get(key, {}).get("sampleCount", 0))
        if sample_count < minimum:
            missing.append({**cell, "sampleCount": sample_count,
                            "requiredSampleCount": minimum})
    status = "FAIL" if failures else "BLOCKED" if missing else "PASS"
    return {
        "schemaVersion": 1,
        "keyFields": list(KEY_FIELDS),
        "minimumSamplesPerRequiredCell": minimum,
        "status": status,
        "requiredCells": required,
        "cells": observed,
        "missingRequiredCells": missing,
        "failedOracleRecordIndexes": failures,
        "exclusions": specification.get("exclusions", []),
    }
