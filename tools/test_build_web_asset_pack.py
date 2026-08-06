#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("build_web_asset_pack.py")
SPEC = importlib.util.spec_from_file_location("build_web_asset_pack", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class WebAssetPackTests(unittest.TestCase):
    def test_two_clean_runs_are_byte_identical(self) -> None:
        source_root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = root / "first"
            second = root / "second"
            MODULE.build_pack(source_root, first)
            MODULE.build_pack(source_root, second)
            first_files = {
                path.relative_to(first): path.read_bytes()
                for path in first.rglob("*")
                if path.is_file()
            }
            second_files = {
                path.relative_to(second): path.read_bytes()
                for path in second.rglob("*")
                if path.is_file()
            }
            self.assertEqual(first_files, second_files)

    def test_subset_archives_contain_only_declared_ids(self) -> None:
        source_root = Path(__file__).resolve().parents[1]
        metadata = json.loads(
            (source_root / "resources/browser-risk-spike.json").read_text(
                encoding="utf-8"
            )
        )
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "output"
            MODULE.build_pack(source_root, output)
            for node in metadata["asset_graph"].values():
                selections = node.get("drs_entries")
                if selections is None:
                    continue
                _, resources = MODULE.read_drs(output / node["source"])
                expected = {
                    (extension, resource_id)
                    for extension, resource_ids in selections.items()
                    for resource_id in resource_ids
                }
                self.assertEqual(set(resources), expected)

    def test_missing_drs_resource_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.drs"
            archive = bytearray(88 + 3)
            archive[40:44] = b"1.00"
            struct.pack_into("<ii", archive, 56, 1, 88)
            archive[64:68] = b" pls"
            struct.pack_into("<ii", archive, 68, 76, 1)
            struct.pack_into("<iii", archive, 76, 1, 88, 3)
            archive[88:] = b"slp"
            source.write_bytes(archive)
            with self.assertRaisesRegex(ValueError, "missing DRS dependencies"):
                MODULE.write_drs_subset(source, root / "subset.drs", {"slp": [2]})

    def test_missing_dependency_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fixture = root / "resources" / "browser-risk-spike.json"
            fixture.parent.mkdir(parents=True)
            fixture.write_text(
                json.dumps(
                    {
                        "budgets": {"maximum_packaged_asset_bytes": 1},
                        "asset_graph": {
                            "risk-scenario": {
                                "reason": "root",
                                "dependencies": ["absent"],
                            }
                        },
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "missing dependency node"):
                MODULE.build_pack(root, root / "output")


if __name__ == "__main__":
    unittest.main()
