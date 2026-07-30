import unittest

from audit_renderer_asset_coverage import (
    build_report,
    direct_renderer_slps,
    mapped_kinds,
    refresh_from_baseline,
)


class RendererCoverageTests(unittest.TestCase):
    def test_groups_mapped_and_fallback_kinds(self):
        types = """
        enum class UnitKind { villager, knight, longbowman, galley };
        enum class BuildingKind { town_center, barracks, farm };
        """
        renderer = """
        LegacySprites load_local_legacy_sprites() {
          MilitaryMapping{UnitKind::knight, 20, 10};
          load_naval_roots(sprites.naval_idle, UnitKind::galley, roots);
          load_building_roots(BuildingKind::barracks, roots);
          return sprites;
        }
        bool render_unit(UnitKind kind) {
          return kind == UnitKind::longbowman;
        }
        """
        dat = {"graphics": [{"id": 1, "slp_id": 10},
                            {"id": 2, "slp_id": 99}]}
        report = build_report(types, renderer, dat, {10, 20})
        by_kind = {
            item["kind"]: item["status"]
            for group in report["groups"].values()
            for item in group
        }
        self.assertEqual(by_kind["villager"], "mapped")
        self.assertEqual(by_kind["knight"], "mapped")
        self.assertEqual(by_kind["longbowman"], "guaranteed_fallback")
        self.assertEqual(by_kind["galley"], "mapped")
        self.assertEqual(by_kind["town_center"], "mapped")
        self.assertEqual(by_kind["barracks"], "mapped")
        self.assertEqual(by_kind["farm"], "guaranteed_fallback")
        self.assertEqual(
            report["live_evidence"]["dat_linked_slps_absent_from_graphics_drs"],
            [99],
        )

    def test_enum_mentions_are_not_sprite_mappings(self):
        renderer = """
        LegacySprites load_local_legacy_sprites() {
          UnitKind::cavalry_archer;
          return sprites;
        }
        bool render_unit(UnitKind kind) {
          return kind == UnitKind::cavalry_archer ||
                 kind == UnitKind::heavy_cavalry_archer;
        }
        """
        self.assertEqual(mapped_kinds(renderer, "UnitKind"), set())
        pre_fix = build_report(
            """
            enum class UnitKind {
              cavalry_archer, heavy_cavalry_archer
            };
            enum class BuildingKind { farm };
            """,
            renderer,
            {"graphics": []},
            set(),
        )
        self.assertEqual(
            [item["status"] for item in pre_fix["groups"]["common"]],
            ["guaranteed_fallback", "guaranteed_fallback"],
        )

        fixed = """
        LegacySprites load_local_legacy_sprites() {
          constexpr std::array mappings{{
            {UnitKind::cavalry_archer, 326, 10},
            {UnitKind::heavy_cavalry_archer, 3763, 10},
          }};
          for (const auto& mapping : mappings) {
            attempt_animation(sprites.military[mapping.kind], mapping.slp, 10);
          }
          return sprites;
        }
        """
        self.assertEqual(
            mapped_kinds(fixed, "UnitKind"),
            {"cavalry_archer", "heavy_cavalry_archer"},
        )

    def test_extracts_direct_renderer_slps(self):
        renderer = """
        LegacySprites load_local_legacy_sprites() {
          attempt(sprites.tree, 4652, 1);
          attempt_animation(sprites.deer_animation, 342, 5);
          MilitaryMapping{UnitKind::archer, 8, 10};
          ActionMapping{UnitKind::archer, 12, 10, 2, 10};
          return sprites;
        }
        """
        self.assertEqual(direct_renderer_slps(renderer), {2, 8, 12, 342, 4652})

    def test_baseline_refresh_retains_archive_evidence(self):
        types = """
        enum class UnitKind { villager, archer };
        enum class BuildingKind { town_center, monastery, farm };
        """
        renderer = """
        LegacySprites load_local_legacy_sprites() {
          MilitaryMapping{UnitKind::archer, 20, 10};
          load_building_roots(BuildingKind::monastery, roots);
          return sprites;
        }
        """
        baseline = build_report(
            types,
            """
            LegacySprites load_local_legacy_sprites() {
              return sprites;
            }
            """,
            {"graphics": [{"id": 1, "slp_id": 20},
                          {"id": 2, "slp_id": 99}]},
            {20},
        )
        baseline["live_evidence"]["dat_graphics"] = 7014
        baseline["live_evidence"]["graphics_drs_slps"] = 1768
        refreshed = refresh_from_baseline(types, renderer, baseline)
        by_kind = {
            item["kind"]: item["status"]
            for group in refreshed["groups"].values()
            for item in group
        }
        self.assertEqual(by_kind["archer"], "mapped")
        self.assertEqual(by_kind["monastery"], "mapped")
        self.assertEqual(by_kind["farm"], "guaranteed_fallback")
        self.assertEqual(refreshed["live_evidence"]["dat_graphics"], 7014)
        self.assertEqual(refreshed["live_evidence"]["graphics_drs_slps"], 1768)
        self.assertEqual(
            refreshed["live_evidence"][
                "dat_linked_slps_absent_from_graphics_drs"
            ],
            [99],
        )


if __name__ == "__main__":
    unittest.main()
