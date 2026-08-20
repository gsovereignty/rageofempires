#!/usr/bin/env python3
"""Focused regressions for check_markdown_links.py discovery policy."""

from __future__ import annotations

import pathlib
import subprocess
import tempfile
import unittest


SCRIPT = pathlib.Path(__file__).with_name("check_markdown_links.py")


class MarkdownLinkGuardTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temporary_directory.name)
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def run_guard(self) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["python3", str(SCRIPT), str(self.root)],
            text=True,
            capture_output=True,
        )

    def test_ignored_dependency_markdown_is_not_checked(self) -> None:
        (self.root / ".gitignore").write_text("node_modules/\n", encoding="utf-8")
        dependency = self.root / "web" / "nostr" / "node_modules" / "package"
        dependency.mkdir(parents=True)
        (dependency / "README.md").write_text("[broken](missing.md)\n", encoding="utf-8")
        project_doc = self.root / "README.md"
        project_doc.write_text("[valid](target.md)\n", encoding="utf-8")
        (self.root / "target.md").write_text("target\n", encoding="utf-8")

        result = self.run_guard()

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_tracked_markdown_remains_checked_when_path_is_ignored(self) -> None:
        generated = self.root / "generated"
        generated.mkdir()
        document = generated / "tracked.md"
        document.write_text("[broken](missing.md)\n", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.root), "add", "-f", "generated/tracked.md"], check=True)
        (self.root / ".gitignore").write_text("generated/\n", encoding="utf-8")

        result = self.run_guard()

        self.assertEqual(result.returncode, 1)
        self.assertIn("generated/tracked.md:1: missing link target", result.stdout)

    def test_nonignored_untracked_markdown_is_checked(self) -> None:
        (self.root / "README.md").write_text("[broken](missing.md)\n", encoding="utf-8")

        result = self.run_guard()

        self.assertEqual(result.returncode, 1)
        self.assertIn("README.md:1: missing link target", result.stdout)


if __name__ == "__main__":
    unittest.main()
