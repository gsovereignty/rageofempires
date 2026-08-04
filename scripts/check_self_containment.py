#!/usr/bin/env python3
"""Reject build/runtime dependencies on the parent research workspace."""

from __future__ import annotations

import pathlib
import re
import sys


FORBIDDEN = re.compile(
    r"""
    \.\./(?:Crack|decompiled|original-assets-hd|original-assets-1999)
    |/ISO/(?:Crack|decompiled|original-assets-hd|original-assets-1999)
    |AOE_DEFAULT_ASSET_ROOT
    |getenv\s*\(\s*"AOE_ASSET_ROOT"
    """,
    re.VERBOSE,
)

SCANNED_SUFFIXES = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".in",
    ".py",
    ".sh",
    ".txt",
}
SCANNED_NAMES = {"CMakeLists.txt", "Makefile"}
IGNORED_PARTS = {
    ".codebase-memory",
    ".git",
    ".worktrees",
    "artifacts",
    "docs",
    "generated",
}


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        if not path.is_file() or any(part in IGNORED_PARTS for part in relative.parts):
            continue
        if relative == pathlib.Path("scripts/check_self_containment.py"):
            continue
        if any(part.startswith("build") for part in relative.parts):
            continue
        if path.name not in SCANNED_NAMES and path.suffix not in SCANNED_SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            if FORBIDDEN.search(line):
                failures.append(f"{relative}:{line_number}: {line.strip()}")
    if failures:
        print("forbidden parent-workspace dependency:")
        print("\n".join(failures))
        return 1
    print("self-containment guard passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
