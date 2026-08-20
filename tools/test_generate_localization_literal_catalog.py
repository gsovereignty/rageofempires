import importlib.util
import pathlib
import re
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "literal_catalog", ROOT / "tools/generate_localization_literal_catalog.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class LiteralCatalogTests(unittest.TestCase):
    def test_stable_key_matches_runtime_contract(self):
        self.assertEqual(
            MODULE.stable_key("OPTIONS"), "ui.literal.87cc05b46a17cf65"
        )

    def test_checked_in_catalog_is_current_and_collision_free(self):
        expected = ROOT / "generated/localization_literal_catalog.tsv"
        with tempfile.TemporaryDirectory() as directory:
            actual = pathlib.Path(directory) / "catalog.tsv"
            old_argv = __import__("sys").argv
            try:
                __import__("sys").argv = [
                    "generator",
                    *[str(path) for path in sorted((ROOT / "src").glob("*.cpp"))],
                    "--output", str(actual),
                ]
                MODULE.main()
            finally:
                __import__("sys").argv = old_argv
            self.assertEqual(actual.read_bytes(), expected.read_bytes())

    def test_only_central_renderer_calls_sdl_debug_text(self):
        source = (ROOT / "src/sdl_app.cpp").read_text(encoding="utf-8")
        start = source.index("void render_ui_debug_text(")
        end = source.index("\nvoid render_hud_text(", start)
        renderer = source[start:end]
        self.assertGreater(renderer.count("SDL_RenderDebugText("), 0)
        self.assertNotIn(
            "SDL_RenderDebugText(", source[:start] + source[end:]
        )

    def test_visible_stream_builders_are_confined_to_skipped_families(self):
        source = (ROOT / "src/sdl_app.cpp").read_text(encoding="utf-8")
        allowed_functions = {
            # Non-visible cache/evidence identifiers.
            "terrain_transition_texture",
            "terrain_elevation_texture",
            "begin_overlap_case",
            "browser_render_telemetry_json",
            # Bug families explicitly excluded from this localization pass.
            "render_campaign_presentation",
            "render_multiplayer_presentation",
            "render_save_browser_overlay",
        }

        function_declarations = {
            "terrain_transition_texture":
                "SDL_Texture* terrain_transition_texture(",
            "terrain_elevation_texture":
                "SDL_Texture* terrain_elevation_texture(",
            "begin_overlap_case": "void begin_overlap_case(",
            "browser_render_telemetry_json":
                "std::string browser_render_telemetry_json(",
            "render_campaign_presentation":
                "void render_campaign_presentation(",
            "render_multiplayer_presentation":
                "void render_multiplayer_presentation(",
            "render_save_browser_overlay":
                "void render_save_browser_overlay(",
        }

        def function_span(name):
            start = source.index(function_declarations[name])
            opening = source.index(") {", start) + 2
            depth = 0
            for offset in range(opening, len(source)):
                depth += source[offset] == "{"
                depth -= source[offset] == "}"
                if depth == 0:
                    return opening, offset
            self.fail(f"unterminated audited function {name}")

        allowed_spans = {
            name: function_span(name) for name in allowed_functions
        }

        def allowed_owner(offset):
            return next((
                name for name, (start, end) in allowed_spans.items()
                if start <= offset <= end
            ), None)

        streams = [
            (match.start(), allowed_owner(match.start()))
            for match in re.finditer(r"\bstd::ostringstream\b", source)
        ]
        self.assertGreater(len(streams), 0)
        self.assertEqual(
            [],
            [(source.count("\n", 0, offset) + 1, function)
             for offset, function in streams
             if function not in allowed_functions],
            "user-visible stream composition must use localization templates",
        )


if __name__ == "__main__":
    unittest.main()
