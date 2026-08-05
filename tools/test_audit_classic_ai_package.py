#!/usr/bin/env python3

import struct
import tempfile
import unittest
from pathlib import Path

from audit_classic_ai_package import audit


class ClassicAiPackageAuditTests(unittest.TestCase):
    def test_inventory_is_nonverbatim_and_counts_operators(self) -> None:
        resources = []
        for resource_id in range(60001, 60030):
            source = (
                b'#load-if-defined DIFFICULTY-HARD\r\n'
                b'(load "economy")\r\n'
                b"(defrule\r\n(true)\r\n=>\r\n(attack-now)\r\n)\r\n"
            )
            resources.append((resource_id, source))
        table_offset = 76
        data_offset = table_offset + len(resources) * 12
        header = bytearray(64)
        header[40:44] = b"1.00"
        struct.pack_into("<I", header, 56, 1)
        table = struct.pack("<4sII", b"anib", table_offset, len(resources))
        index = bytearray()
        data = bytearray()
        for resource_id, source in resources:
            index.extend(struct.pack("<iII", resource_id, data_offset, len(source)))
            data.extend(source)
            data_offset += len(source)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gamedata_x1.drs"
            path.write_bytes(bytes(header) + table + bytes(index) + bytes(data))
            result = audit(path)
        self.assertEqual(len(result["resources"]), 29)
        self.assertEqual(result["fact_operators"], ["true"])
        self.assertEqual(result["action_operators"], ["attack-now"])
        self.assertNotIn("source", result["resources"][0])
        self.assertEqual(result["resources"][0]["loads"], ["economy"])
        self.assertEqual(
            result["resources"][0]["condition_symbols"],
            ["DIFFICULTY-HARD"],
        )


if __name__ == "__main__":
    unittest.main()
