#!/usr/bin/env python3
"""Tests for unexpected runtime fallback classification and diagnostics."""

from __future__ import annotations

import unittest

from check_runtime_fallbacks import format_fallback, unexpected_fallbacks


def report(*events: dict) -> dict:
    return {
        "schema": "aoe-runtime-render-fallback-v1",
        "events": list(events),
    }


def event(
    *,
    status: str,
    call_site: str = "render_unit:procedural_body",
) -> dict:
    return {
        "stable_render_state_key": "unit|villager|idle",
        "entity_id": 42,
        "simulation_tick": 7,
        "renderer_call_site": call_site,
        "requested_asset": {
            "graphic_id": 123,
            "slp_id": 456,
            "shadow_slp_id": None,
        },
        "status": status,
        "reason": "selected legacy animation did not render",
    }


class RuntimeFallbackTests(unittest.TestCase):
    def test_detects_unexpected_procedural_unit_body(self) -> None:
        fallback = event(status="renderer_failure")
        self.assertEqual(
            [fallback],
            unexpected_fallbacks(report(fallback)),
        )

    def test_allows_reviewed_intentional_procedural_body(self) -> None:
        self.assertEqual(
            [],
            unexpected_fallbacks(
                report(event(status="intentional_procedural"))
            ),
        )

    def test_ignores_non_unit_procedural_fallbacks(self) -> None:
        self.assertEqual(
            [],
            unexpected_fallbacks(
                report(
                    event(
                        status="renderer_failure",
                        call_site="render_building:procedural_body",
                    )
                )
            ),
        )

    def test_diagnostic_contains_repair_context(self) -> None:
        diagnostic = format_fallback(event(status="renderer_failure"))
        for detail in (
            "entity=42",
            "tick=7",
            "unit|villager|idle",
            "graphic_id=123",
            "slp_id=456",
            "selected legacy animation did not render",
        ):
            self.assertIn(detail, diagnostic)

    def test_rejects_wrong_schema(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported schema"):
            unexpected_fallbacks({"schema": "wrong", "events": []})


if __name__ == "__main__":
    unittest.main()
