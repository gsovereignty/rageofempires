import importlib.util
import pathlib
import unittest


PATH = pathlib.Path(__file__).with_name("generate_content_localization_map.py")
SPEC = importlib.util.spec_from_file_location("content_localization", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ContentLocalizationMapTests(unittest.TestCase):
    def test_deduplicates_civilization_variants_and_preserves_all_id_roles(self):
        rows = MODULE.generate({
            "object_variants": [
                {"id": 4, "language_dll_name": "Num(5083)",
                 "language_dll_help": "Num(105083)"},
                {"id": 4, "language_dll_name": "Num(5083)",
                 "language_dll_help": "Num(105083)"},
            ],
            "technologies": [
                {"id": 22, "language_name_id": 7022,
                 "language_help_id": 107022,
                 "language_description_id": 8022},
            ],
        })
        self.assertEqual(rows, [
            ("object", 4, 5083, 105083, 0),
            ("technology", 22, 7022, 107022, 8022),
        ])


if __name__ == "__main__":
    unittest.main()
