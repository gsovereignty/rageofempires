import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("audit_builtin_random_maps.py")
SPEC = importlib.util.spec_from_file_location("audit_builtin_random_maps", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def make_drs(resources):
    count = len(resources)
    header = bytearray(64)
    header[40:44] = b"1.00"
    struct.pack_into("<I", header, 56, 1)
    table_offset = 76
    data_offset = table_offset + count * 12
    table = struct.pack("<4sII", b"anib", table_offset, count)
    entries = bytearray()
    data = bytearray()
    for resource_id, payload in resources.items():
        entries += struct.pack("<iII", resource_id, data_offset + len(data), len(payload))
        data += payload
    return bytes(header) + table + bytes(entries) + bytes(data)


class BuiltinRandomMapAuditTests(unittest.TestCase):
    def test_exact_resources_and_includes_are_inventory_only(self):
        resources = {
            resource_id: (
                b"#const ARABIA 9\n" if resource_id == 54000 else
                b"#include_drs random_map.def 54000\n<LAND_GENERATION>\n"
            )
            for resource_id in MODULE.RESOURCE_NAMES
        }
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "gamedata_x1.drs"
            path.write_bytes(make_drs(resources))
            report = MODULE.audit(path)
        self.assertEqual(report["schema"], "aoe-builtin-random-map-evidence-v1")
        self.assertEqual(len(report["resources"]), 5)
        arabia = report["resources"][1]
        self.assertEqual(arabia["name"], "Arabia")
        self.assertEqual(arabia["sections"], ["LAND_GENERATION"])
        self.assertEqual(
            arabia["includes"],
            [{"filename": "random_map.def", "resource_id": 54000}],
        )
        self.assertNotIn("text", arabia)

    def test_out_of_bounds_resource_fails_closed(self):
        broken = bytearray(make_drs({54000: b"text"}))
        struct.pack_into("<I", broken, 76 + 8, 999999)
        with self.assertRaisesRegex(ValueError, "outside archive"):
            MODULE.drs_entries(bytes(broken))


if __name__ == "__main__":
    unittest.main()
