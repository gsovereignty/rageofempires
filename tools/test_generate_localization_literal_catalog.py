import importlib.util
import pathlib
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


if __name__ == "__main__":
    unittest.main()
