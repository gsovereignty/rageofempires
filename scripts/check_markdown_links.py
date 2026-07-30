#!/usr/bin/env python3
"""Reject broken local links in repository Markdown."""

from __future__ import annotations

import pathlib
import re
import sys
import urllib.parse


LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
IGNORED_PARTS = {".git", ".codebase-memory", "artifacts", "generated"}


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []
    for document in sorted(root.rglob("*.md")):
        relative = document.relative_to(root)
        if any(part in IGNORED_PARTS for part in relative.parts):
            continue
        if any(part.startswith("build") for part in relative.parts):
            continue
        text = document.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            for match in LINK.finditer(line):
                raw = match.group(1).strip().strip("<>")
                parsed = urllib.parse.urlsplit(raw)
                if parsed.scheme or raw.startswith(("#", "mailto:")):
                    continue
                path_text = urllib.parse.unquote(parsed.path)
                if not path_text:
                    continue
                target = (document.parent / path_text).resolve()
                try:
                    target.relative_to(root)
                except ValueError:
                    failures.append(
                        f"{relative}:{line_number}: link escapes repository: {raw}"
                    )
                    continue
                if not target.exists():
                    failures.append(
                        f"{relative}:{line_number}: missing link target: {raw}"
                    )
    if failures:
        print("broken Markdown links:")
        print("\n".join(failures))
        return 1
    print("Markdown link guard passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
