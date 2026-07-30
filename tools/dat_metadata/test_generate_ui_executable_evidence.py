import importlib.util
import json
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_ui_executable_evidence.py")
SPEC = importlib.util.spec_from_file_location("ui_executable", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class UiExecutableEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/ui_executable_evidence.json").read_text()
        )

    def test_all_hd_font_slots_are_accounted_for_without_invented_values(self):
        fonts = self.data["fonts"]
        slots = {item["slot"]: item for item in fonts["slots"]}
        self.assertEqual(fonts["slot_count"], 37)
        self.assertEqual(fonts["empty_slots"], [5])
        self.assertEqual(set(slots), set(range(37)) - {5})
        self.assertEqual(slots[0]["role"], "RGE_FONT_BUTTON1")
        self.assertEqual(
            slots[0]["string_ids"],
            {"family": 110, "height": 111, "style": 112},
        )
        self.assertIsNone(slots[0]["resolved_values"])
        self.assertEqual(
            slots[36],
            {
                "slot": 36,
                "role": None,
                "constructor": "direct",
                "family": "Georgia",
                "height": 14,
                "weight": 700,
                "italic": False,
                "strikeout": False,
                "classification": "exact_hd",
            },
        )

    def test_exact_executable_sheet_roles_and_dispatch(self):
        sheets = {item["filename"]: item for item in self.data["icon_sheets"]}
        self.assertEqual(sheets["btncmd.shp"]["resource_id"], 50721)
        self.assertEqual(sheets["btntech.shp"]["resource_id"], 50729)
        self.assertEqual(sheets["ico_unit.shp"]["resource_id"], 50730)
        self.assertEqual(sheets["btncmd.shp"]["frame_count"], 69)
        self.assertEqual(sheets["btntech.shp"]["frame_count"], 118)
        self.assertEqual(sheets["ico_unit.shp"]["frame_count"], 134)
        for sheet in sheets.values():
            self.assertEqual(sheet["classification"], "exact_executable_load")
        self.assertEqual(
            sheets["btntech.shp"]["dat_index_to_frame_contract"],
            "identity_technology_record_plus_0x2c",
        )
        self.assertEqual(
            sheets["ico_unit.shp"]["dat_index_to_frame_contract"],
            "identity_ordinary_unit_record_plus_0x54",
        )

    def test_font_metrics_are_runtime_gdi_metrics(self):
        metrics = self.data["fonts"]["metrics"]
        self.assertEqual(
            metrics["width"], "GetTextMetricsA.tmAveCharWidth"
        )
        self.assertIn("tmExternalLeading", metrics["line_height"])
        self.assertEqual(len(self.data["fonts"]["external_files"]), 18)
        self.assertEqual(
            self.data["fonts"]["localized_style_grammar"],
            {
                "weight_700_tokens": ["B", "b"],
                "italic_tokens": ["I", "i"],
                "default_weight": 400,
                "classification": "exact_hd",
            },
        )


if __name__ == "__main__":
    unittest.main()
