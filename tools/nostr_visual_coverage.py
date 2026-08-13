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

REQUIRED_CATALOG_IDS = {
    "villager-empty-moving", "villager-carrying-food",
    "villager-carrying-wood", "villager-carrying-gold",
    "villager-carrying-stone", "villager-gathering",
    "villager-returning", "villager-constructing", "villager-repairing",
    "infantry-before-upgrade", "infantry-after-upgrade",
    "archer-ranged-transition", "cavalry", "siege-composite",
    "ships-16-direction", "huntable-herdable-animals", "patrol", "chase",
    "flee", "formation", "attack-movement", "death-decay-direction",
    "projectile-impact-orientation",
}

REQUIRED_CATALOG_ASSERTIONS = {
    "movement-direction", "resolved-frame", "mirror",
    "animation-progress", "pixel-direction",
}


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
    catalog = value.get("unitActionCatalog")
    if not isinstance(catalog, list):
        raise ValueError("coverage specification has no unit/action catalog")
    identifiers = [entry.get("id") for entry in catalog
                   if isinstance(entry, dict)]
    missing = REQUIRED_CATALOG_IDS - set(identifiers)
    if missing:
        raise ValueError(
            "coverage catalog silently omits: " + ", ".join(sorted(missing))
        )
    if len(identifiers) != len(set(identifiers)):
        raise ValueError("coverage catalog contains duplicate entries")
    for entry in catalog:
        status = entry.get("status")
        if status not in {"required", "excluded"}:
            raise ValueError(f"invalid coverage catalog status: {status}")
        if not entry.get("evidence"):
            raise ValueError(f"coverage catalog lacks evidence: {entry.get('id')}")
        if status == "excluded" and not entry.get("reason"):
            raise ValueError(f"coverage exclusion lacks reason: {entry.get('id')}")
    assertions = set(value.get("requiredCatalogAssertions", []))
    if assertions != REQUIRED_CATALOG_ASSERTIONS:
        raise ValueError(
            "coverage catalog assertions must be exactly: " +
            ", ".join(sorted(REQUIRED_CATALOG_ASSERTIONS))
        )
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
    blockers: list[int] = []
    catalog_observed: dict[str, set[str]] = {}
    for index, record in enumerate(oracle_records):
        if record.get("verdict") == "FAIL":
            failures.append(index)
        elif record.get("verdict") == "BLOCKED":
            blockers.append(index)
        if record.get("verdict") == "PASS":
            assertions = {
                str(value) for value in record.get("assertions", [])
            }
            for catalog_id in record.get("catalogIds", []):
                catalog_observed.setdefault(str(catalog_id), set()).update(
                    assertions
                )
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
    required_assertions = set(
        specification.get("requiredCatalogAssertions", [])
    )
    missing_catalog_assertions = []
    catalog_coverage = []
    for entry in specification.get("unitActionCatalog", []):
        if entry.get("status") != "required":
            continue
        identifier = str(entry["id"])
        observed_assertions = catalog_observed.get(identifier, set())
        absent = sorted(required_assertions - observed_assertions)
        catalog_coverage.append({
            "id": identifier,
            "observedAssertions": sorted(observed_assertions),
            "missingAssertions": absent,
            "evidence": entry.get("evidence"),
        })
        if absent:
            missing_catalog_assertions.append({
                "id": identifier, "missingAssertions": absent,
            })
    status = (
        "FAIL" if failures else "BLOCKED" if (
            blockers or missing or missing_catalog_assertions
        ) else "PASS"
    )
    return {
        "schemaVersion": 1,
        "keyFields": list(KEY_FIELDS),
        "minimumSamplesPerRequiredCell": minimum,
        "status": status,
        "requiredCells": required,
        "cells": observed,
        "missingRequiredCells": missing,
        "catalogCoverage": catalog_coverage,
        "missingCatalogAssertions": missing_catalog_assertions,
        "failedOracleRecordIndexes": failures,
        "blockedOracleRecordIndexes": blockers,
        "exclusions": specification.get("exclusions", []),
    }
