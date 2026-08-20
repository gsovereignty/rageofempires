#!/usr/bin/env python3
"""Reject broken local links in repository Markdown."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import urllib.parse


LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


def markdown_documents(root: pathlib.Path) -> list[pathlib.Path]:
    """Return tracked and non-ignored untracked Markdown documents."""
    try:
        result = subprocess.run(
            [
                "git",
                "-C",
                str(root),
                "ls-files",
                "--cached",
                "--others",
                "--exclude-standard",
                "-z",
                "--",
                "*.md",
            ],
            check=True,
            capture_output=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        # Keep script useful for unpacked source trees without Git metadata.
        return sorted(root.rglob("*.md"))

    documents = []
    for encoded_path in result.stdout.split(b"\0"):
        if encoded_path:
            documents.append(root / encoded_path.decode("utf-8", errors="surrogateescape"))
    return sorted(documents)


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []
    for document in markdown_documents(root):
        relative = document.relative_to(root)
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
