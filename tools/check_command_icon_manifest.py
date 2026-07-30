#!/usr/bin/env python3
"""Validate command-icon evidence and PanelCommand coverage."""

from __future__ import annotations

import json
import pathlib
import re
import sys


def panel_commands(header: str) -> set[str]:
    match = re.search(
        r"enum\s+class\s+PanelCommand\s*\{(?P<body>.*?)\};",
        header,
        re.DOTALL,
    )
    if not match:
        raise ValueError("PanelCommand enum not found")
    return {
        token.strip()
        for token in match.group("body").split(",")
        if token.strip()
    }


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    expected = panel_commands(
        (root / "include/aoe/command_panel.hpp").read_text(encoding="utf-8")
    )
    document = json.loads(
        (root / "generated/command_icon_manifest.json").read_text(
            encoding="utf-8"
        )
    )
    records = document["commands"]
    names = [record["panel_command"] for record in records]
    errors: list[str] = []
    if len(names) != len(set(names)):
        errors.append("duplicate panel_command record")
    missing = expected - set(names)
    extra = set(names) - expected
    if missing:
        errors.append("missing commands: " + ", ".join(sorted(missing)))
    if extra:
        errors.append("unknown commands: " + ", ".join(sorted(extra)))
    sheet_bounds = {50706: 52, 50721: 69, 50729: 118, 50730: 134}
    for record in records:
        name = record["panel_command"]
        evidence = record["evidence"]
        sheet = record["slp_id"]
        frame = record["frame"]
        if evidence.startswith("exact"):
            if not record["citation"]:
                errors.append(f"{name}: exact binding lacks citation")
            if sheet not in sheet_bounds:
                errors.append(f"{name}: exact binding has unknown sheet")
        if isinstance(frame, int):
            if sheet not in sheet_bounds or not 0 <= frame < sheet_bounds[sheet]:
                errors.append(f"{name}: frame outside sheet bounds")
        if evidence == "unproved":
            if sheet is not None or frame is not None:
                errors.append(f"{name}: unproved action claims archive binding")
            if record["runtime_state"] != "procedural_fallback":
                errors.append(f"{name}: unproved action lacks procedural fallback")
    if errors:
        print("\n".join(errors))
        return 1
    print(f"command icon manifest passed: {len(records)} commands")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
