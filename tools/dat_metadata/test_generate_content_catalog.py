import importlib.util
import pathlib
import unittest


PATH = pathlib.Path(__file__).with_name("generate_content_catalog.py")
SPEC = importlib.util.spec_from_file_location("generate_content_catalog", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class ContentCatalogGeneratorTests(unittest.TestCase):
    def test_parses_technology_semantics_without_debug_record(self):
        value = MODULE.parse_tech({
            "id": 7,
            "name": "Example",
            "record": (
                "Tech { required_techs: [TechID(1), TechID(2)], effects: "
                "[TechEffectRef { effect_type: 9, amount: 3, enabled: true }], "
                "civilization_id: Some(CivilizationID(4)), full_tech_mode: 1, "
                "location: Some(UnitTypeID(109)), language_dll_name: None, "
                "language_dll_description: None, time: 50, time2: 60, "
                "type_: 2, icon_id: Some(8), button_id: 6, "
                "language_dll_help: None, help_page_id: 0, hotkey: None, "
                'name: "Example" }'
            ),
        })
        self.assertEqual(value["required_technologies"], [1, 2])
        self.assertEqual(value["costs"][0]["resource_id"], 9)
        self.assertEqual(value["civilization_id"], 4)
        self.assertEqual(value["research_location_object_id"], 109)
        self.assertNotIn("record", value)

    def test_reduces_full_counts_and_never_copies_opaque_records(self):
        source = {
            "format": "VER 5.7",
            "civilizations": [{
                "id": 0, "name": "Gaia", "record":
                    "Civilization { civ_effect: 0, bonus_effect: None }",
                "units": [{
                    "id": 4, "copy_id": 4, "unit_group": 4,
                    "base_class": "Combat", "unit_class": 0,
                    "action": {"tasks": "None", "work_rate": 1.0},
                }],
            }],
            "techs": [{
                "id": 0, "name": "", "record": (
                    "Tech { required_techs: [], effects: [], "
                    "civilization_id: None, full_tech_mode: 0, location: None, "
                    "language_dll_name: None, language_dll_description: None, "
                    "time: 0, time2: 0, type_: 0, icon_id: None, button_id: 0, "
                    "language_dll_help: None, help_page_id: 0, hotkey: None, "
                    'name: "" }'
                ),
            }],
            "effects": [{"id": 0, "name": "", "commands": []}],
        }
        result = MODULE.generate(source)
        self.assertEqual(result["object_record_count"], 1)
        self.assertNotIn("record", result["technologies"][0])
        self.assertNotIn("record", result["civilizations"][0])
        self.assertEqual(result["civilizations"][0]["civilization_effect_id"], 0)
        action = result["object_variants"][0]["action"]
        self.assertEqual(action["tasks"], [])
        self.assertTrue(action["tasks_semantically_available"])

    def test_parses_task_semantics(self):
        tasks = MODULE.parse_tasks(
            "Some(TaskList([Task { id: 3, is_default: true, action_type: 5, "
            "object_class: 10, object_id: Some(UnitTypeID(83)), terrain_id: -1, "
            "attribute_types: (0, 1, -1, -1), work_values: (0.5, 1.0), "
            "work_range: 1.5, auto_search_targets: true, search_wait_time: 2.0, "
            "enable_targeting: false, combat_level: 1, work_flags: (2, 3), "
            "owner_type: 4, holding_attribute: 0, state_building: 0, "
            "move_sprite: Some(SpriteID(7)), work_sprite: None, "
            "work_sprite2: None, carry_sprite: None, work_sound: "
            "Some(SoundID(8)), work_sound2: None }]))"
        )
        self.assertEqual(tasks[0]["object_id"], 83)
        self.assertEqual(tasks[0]["move_graphic"], 7)
        self.assertEqual(tasks[0]["work_sound"], 8)


if __name__ == "__main__":
    unittest.main()
