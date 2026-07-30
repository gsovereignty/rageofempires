#!/usr/bin/env python3
"""Generate the bounded 500-item original-versus-reconstruction audit."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DRIFT = ROOT / "generated" / "core_rules_drift.json"
OUTPUT = ROOT / "docs" / "audits" / "2026-07-30-500-DISCREPANCY-AUDIT.md"
TARGET_COUNT = 500


def cell(value: object) -> str:
    if value is None:
        return "not in rules struct"
    if isinstance(value, str):
        return value.replace("|", r"\|")
    rendered = json.dumps(value, ensure_ascii=False, separators=(",", ":"))
    return rendered.replace("|", r"\|")


def main() -> None:
    source = json.loads(DRIFT.read_text(encoding="utf-8"))
    dat_rows: list[dict[str, object]] = []
    for entity in source["entities"]:
        for attribute in entity["attributes"]:
            if attribute["classification"] == "exact":
                continue
            dat_rows.append(
                {
                    "entity": entity["name"],
                    "kind": entity["kind"],
                    **attribute,
                }
            )

    runtime_rows = [
        {
            "area": "movement animation",
            "ours": "moving art selected from pending move-order flag",
            "original": "floating-coordinate service path represents physical motion",
            "classification": "fixed",
            "note": "99d7782 gates moving art on current-tick displacement",
        },
        {
            "area": "blocked fractional movement",
            "ours": "blocked ticks banked speed remainder",
            "original": "continuous speed evidence gives no basis for stored blocked bursts",
            "classification": "fixed",
            "note": "89e9f86 restores pre-tick accumulator on failed primary step",
        },
        {
            "area": "authoritative coordinates",
            "ours": "integer tile positions",
            "original": "binary object coordinates are floating point",
            "classification": "open",
            "note": "render interpolation only; sub-tile simulation remains absent",
        },
        {
            "area": "walking frame period",
            "ours": "bounded 100 ms presentation period",
            "original": "per-graphic timing path exists but exact integrated cadence is unproved",
            "classification": "open",
            "note": "exact graphic timing metadata not integrated",
        },
    ]
    selected_dat = dat_rows[: TARGET_COUNT - len(runtime_rows)]
    if len(selected_dat) != TARGET_COUNT - len(runtime_rows):
        raise RuntimeError(
            f"need {TARGET_COUNT - len(runtime_rows)} DAT rows, "
            f"found {len(selected_dat)}"
        )

    lines = [
        "# Original versus reconstruction: 500 discrepancies",
        "",
        "Date: 2026-07-30",
        "",
        "## Scope and meaning",
        "",
        "This bounded audit records exactly 500 evidenced differences between",
        "the supplied 2013 runtime/data corpus and the reconstruction. A",
        "discrepancy is any non-exact representation: confirmed bug, transformed",
        "unit/scale, deliberate reconstruction policy, missing field, or open",
        "fidelity gap. It is not automatically a defect and does not imply that",
        "the decompiler recovered original source semantics.",
        "",
        "Items 001-004 come from the movement audit against",
        "`decompiled/AoK-HD-patched.c`, the matching supplied binary, and the",
        "movement presentation contract. Items 005-500 are a deterministic",
        "prefix of non-exact live VER 5.7 DAT comparisons in",
        "`generated/core_rules_drift.json`. Exact rows are excluded.",
        "",
        "Regenerate with:",
        "",
        "```sh",
        "python3 tools/generate_500_discrepancy_audit.py",
        "```",
        "",
        "## Runtime and binary discrepancies",
        "",
        "| ID | Area | Reconstruction | Original evidence | Class | Note |",
        "|---:|---|---|---|---|---|",
    ]
    for index, row in enumerate(runtime_rows, 1):
        lines.append(
            f"| D{index:03d} | {cell(row['area'])} | {cell(row['ours'])} | "
            f"{cell(row['original'])} | {cell(row['classification'])} | "
            f"{cell(row['note'])} |"
        )

    lines.extend(
        [
            "",
            "## Live DAT discrepancies",
            "",
            "| ID | Kind | Entity | Attribute | Reconstruction | VER 5.7 DAT | Class | Note |",
            "|---:|---|---|---|---:|---:|---|---|",
        ]
    )
    for index, row in enumerate(selected_dat, len(runtime_rows) + 1):
        lines.append(
            f"| D{index:03d} | {cell(row['kind'])} | {cell(row['entity'])} | "
            f"{cell(row['attribute'])} | {cell(row.get('represented'))} | "
            f"{cell(row.get('dat'))} | {cell(row['classification'])} | "
            f"{cell(row.get('note', ''))} |"
        )

    lines.extend(
        [
            "",
            "## Count gate",
            "",
            f"- Runtime/binary discrepancies: {len(runtime_rows)}",
            f"- Live DAT discrepancies: {len(selected_dat)}",
            f"- Total: {len(runtime_rows) + len(selected_dat)}",
            "",
        ]
    )
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
