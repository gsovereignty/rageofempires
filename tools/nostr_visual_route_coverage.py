#!/usr/bin/env python3
"""Evaluate required Nostr visual route-shape coverage fail closed."""

from __future__ import annotations


REQUIRED_ROUTE_IDS = {
    "right-angle", "u-turn", "zigzag", "clockwise-loop",
    "counter-clockwise-loop", "queued-waypoints", "moving-target-chase",
    "obstacle-detour", "narrow-passage", "building-corner-navigation",
    "formation-regrouping", "shoreline-navigation",
}


def validate_route_catalog(specification: dict[str, object]) \
        -> list[dict[str, object]]:
    catalog = specification.get("routeCatalog")
    if not isinstance(catalog, list):
        raise ValueError("coverage specification has no route catalog")
    identifiers = [entry.get("id") for entry in catalog
                   if isinstance(entry, dict)]
    missing = REQUIRED_ROUTE_IDS - set(identifiers)
    if missing:
        raise ValueError("route catalog silently omits: " +
                         ", ".join(sorted(missing)))
    if len(identifiers) != len(set(identifiers)):
        raise ValueError("route catalog contains duplicate entries")
    for entry in catalog:
        status = entry.get("status")
        if status not in {"required", "excluded"}:
            raise ValueError(f"invalid route status: {status}")
        if not entry.get("evidence"):
            raise ValueError(f"route lacks evidence: {entry.get('id')}")
        if status == "excluded" and not entry.get("reason"):
            raise ValueError(f"route exclusion lacks reason: {entry.get('id')}")
    return catalog


def evaluate_route_coverage(
    specification: dict[str, object], observed: list[dict[str, object]],
) -> dict[str, object]:
    catalog = validate_route_catalog(specification)
    observed_by_id: dict[str, list[dict[str, object]]] = {}
    failures: list[dict[str, object]] = []
    blockers: list[dict[str, object]] = []
    for record in observed:
        identifier = str(record.get("id", ""))
        observed_by_id.setdefault(identifier, []).append(record)
        if record.get("verdict") == "FAIL":
            failures.append(record)
        elif record.get("verdict") != "PASS":
            blockers.append(record)
    missing = [
        {"id": entry["id"], "evidence": entry["evidence"]}
        for entry in catalog
        if entry["status"] == "required" and not observed_by_id.get(
            str(entry["id"])
        )
    ]
    status = (
        "FAIL" if failures else "BLOCKED" if missing or blockers else "PASS"
    )
    return {
        "schemaVersion": 1, "status": status,
        "requiredRoutes": [entry["id"] for entry in catalog
                           if entry["status"] == "required"],
        "observedRoutes": sorted(identifier for identifier, records
                                 in observed_by_id.items() if records),
        "missingRequiredRoutes": missing,
        "failedRoutes": failures,
        "blockedRoutes": blockers,
        "exclusions": [entry for entry in catalog
                       if entry["status"] == "excluded"],
    }
