#!/usr/bin/env python3
"""Batch sprite-overlap cases into one deterministic human-review gallery."""

from __future__ import annotations

import argparse
import html
import json
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as error:  # pragma: no cover - bad tool installation
    raise SystemExit(
        "batch visual overlap audit requires Pillow; install it with "
        "`python3 -m pip install Pillow`"
    ) from error

from visual_overlap_audit import audit_images


CASE_ID = re.compile(r"^[A-Za-z0-9._-]+$")
THRESHOLDS = {
    "alpha_threshold": 96,
    "terrain_threshold": 12,
    "composite_threshold": 24,
    "minimum_area": 4,
}


def _load_json(path: Path) -> dict[str, object]:
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read manifest {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError("manifest root must be an object")
    return value


def _path(base: Path, value: object, name: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a non-empty path string")
    path = Path(value)
    return path if path.is_absolute() else base / path


def _integer(value: object, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{name} must be an integer")
    return value


def _placement(case: dict[str, object]) -> tuple[int, int]:
    direct = "x" in case or "y" in case
    anchored = "screen_x" in case or "screen_y" in case
    if direct == anchored:
        raise ValueError(
            "case must use exactly one placement form: x/y or screen_x/screen_y"
        )
    if direct:
        if "x" not in case or "y" not in case:
            raise ValueError("x and y must be supplied together")
        return _integer(case["x"], "x"), _integer(case["y"], "y")
    if "screen_x" not in case or "screen_y" not in case:
        raise ValueError("screen_x and screen_y must be supplied together")
    screen_x = _integer(case["screen_x"], "screen_x")
    screen_y = _integer(case["screen_y"], "screen_y")
    anchor_x = _integer(case.get("anchor_x", 0), "anchor_x")
    anchor_y = _integer(case.get("anchor_y", 0), "anchor_y")
    return screen_x - anchor_x, screen_y - anchor_y


def _thresholds(
    defaults: dict[str, object], case: dict[str, object]
) -> dict[str, int]:
    values = dict(THRESHOLDS)
    for source in (defaults, case.get("thresholds", {})):
        if not isinstance(source, dict):
            raise ValueError("thresholds must be an object")
        unknown = set(source) - set(values)
        if unknown:
            raise ValueError(f"unknown thresholds: {', '.join(sorted(unknown))}")
        for name, value in source.items():
            values[name] = _integer(value, name)
    return values


def _save_rgba(source: Path, destination: Path) -> Image.Image:
    with Image.open(source) as image:
        converted = image.convert("RGBA")
    converted.save(destination, format="PNG")
    return converted


def _gallery(report: dict[str, object]) -> str:
    cards = []
    for case in report["cases"]:
        metadata = case["metadata"]
        metadata_rows = "".join(
            f"<dt>{html.escape(str(key))}</dt><dd>{html.escape(str(value))}</dd>"
            for key, value in sorted(metadata.items())
        )
        images = case.get("images", {})
        if case["status"] == "blocked":
            cards.append(f"""
<article class="case blocked" data-status="blocked" data-case="{case['id']}">
  <header><h2>{html.escape(case['id'])}</h2><strong>blocked</strong></header>
  <p>{html.escape(str(case['blocked_reason']))}</p><dl>{metadata_rows}</dl>
</article>""")
            continue
        cards.append(f"""
<article class="case {case['status']}" data-status="{case['status']}" data-case="{case['id']}">
  <header><h2>{html.escape(case['id'])}</h2><strong>{case['status']}</strong></header>
  <p>{case['component_count']} regions; {case['overlap_pixels']} overlap pixels</p>
  <div class="images">
    <figure><img src="{images['annotated']}" alt="annotated"><figcaption>Annotated</figcaption></figure>
    <figure><img src="{images['actual']}" alt="actual"><figcaption>Actual</figcaption></figure>
    <figure><img src="{images['terrain']}" alt="terrain"><figcaption>Terrain only</figcaption></figure>
    <figure class="sprite"><img src="{images['sprite']}" alt="sprite"><figcaption>Expected sprite</figcaption></figure>
  </div>
  <dl>{metadata_rows}</dl>
  <fieldset><legend>Human decision</legend>
    <label><input type="radio" name="decision-{case['id']}" value="bug"> Bug</label>
    <label><input type="radio" name="decision-{case['id']}" value="intentional"> Intentional</label>
    <label><input type="radio" name="decision-{case['id']}" value="uncertain"> Uncertain</label>
  </fieldset>
</article>""")
    review_seed = json.dumps(
        [{"id": case["id"], "detector_status": case["status"]}
         for case in report["cases"] if case["status"] != "blocked"],
        sort_keys=True,
    ).replace("</", "<\\/")
    summary = report["summary"]
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Visual overlap review</title>
<style>
body{{font:14px system-ui;margin:20px;background:#171717;color:#eee}}button{{margin:0 5px 15px 0}}
.case{{border:1px solid #555;padding:12px;margin:14px 0;background:#242424}}.overlap_detected{{border-color:#f33}}
header{{display:flex;justify-content:space-between}}h2{{margin:0}}.images{{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}}
figure{{margin:0}}img{{max-width:100%;image-rendering:pixelated;background:#444}}.sprite img{{background:repeating-conic-gradient(#777 0 25%,#aaa 0 50%) 50%/16px 16px}}
figcaption{{text-align:center}}dl{{display:grid;grid-template-columns:max-content 1fr;gap:3px 10px}}dt{{font-weight:bold}}dd{{margin:0}}
</style></head><body>
<h1>Visual overlap review</h1>
<p>{summary['total']} cases; {summary['flagged']} flagged; {summary['clean']} clean; {summary['blocked']} blocked.</p>
<button onclick="filterCases('all')">All</button><button onclick="filterCases('overlap_detected')">Flagged</button><button onclick="filterCases('pass')">Clean</button>
<button onclick="downloadDecisions()">Download decisions JSON</button>
{''.join(cards)}
<script>
const reviewSeed={review_seed};
function filterCases(status){{document.querySelectorAll('.case').forEach(x=>x.hidden=status!=='all'&&x.dataset.status!==status)}}
function downloadDecisions(){{const decisions=reviewSeed.map(x=>{{const chosen=document.querySelector('input[name="decision-'+x.id+'"]:checked');return {{...x,decision:chosen?chosen.value:'unreviewed'}}}});const blob=new Blob([JSON.stringify({{schema_version:1,decisions}},null,2)+'\\n'],{{type:'application/json'}});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='visual-overlap-decisions.json';a.click();URL.revokeObjectURL(a.href)}}
</script></body></html>
"""


def run_batch(manifest_path: Path, output_dir: Path) -> dict[str, object]:
    """Audit all manifest cases and write one self-contained review bundle."""
    manifest = _load_json(manifest_path)
    if manifest.get("schema_version") != 1:
        raise ValueError("manifest schema_version must be 1")
    raw_cases = manifest.get("cases")
    if not isinstance(raw_cases, list) or not raw_cases:
        raise ValueError("manifest cases must be a non-empty array")
    defaults = manifest.get("thresholds", {})
    if not isinstance(defaults, dict):
        raise ValueError("manifest thresholds must be an object")

    cases: list[dict[str, object]] = []
    identifiers: set[str] = set()
    for raw_case in raw_cases:
        if not isinstance(raw_case, dict):
            raise ValueError("every case must be an object")
        identifier = raw_case.get("id")
        if not isinstance(identifier, str) or not CASE_ID.fullmatch(identifier):
            raise ValueError("case id must contain only letters, digits, dot, dash, underscore")
        if identifier in identifiers:
            raise ValueError(f"duplicate case id: {identifier}")
        identifiers.add(identifier)
        cases.append(raw_case)
    cases.sort(key=lambda case: case["id"])

    assets = output_dir / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    base = manifest_path.parent
    results = []
    for case in cases:
        identifier = case["id"]
        metadata = case.get("metadata", {})
        if not isinstance(metadata, dict):
            raise ValueError(f"case {identifier} metadata must be an object")
        blocked_reason = case.get("blocked_reason")
        if blocked_reason is not None:
            if not isinstance(blocked_reason, str) or not blocked_reason:
                raise ValueError(f"case {identifier} blocked_reason must be non-empty")
            results.append({
                "id": identifier,
                "status": "blocked",
                "blocked_reason": blocked_reason,
                "component_count": 0,
                "overlap_pixels": 0,
                "metadata": metadata,
            })
            continue
        actual_path = _path(base, case.get("actual"), "actual")
        terrain_path = _path(base, case.get("terrain"), "terrain")
        sprite_path = _path(base, case.get("sprite"), "sprite")
        actual = _save_rgba(actual_path, assets / f"{identifier}-actual.png")
        terrain = _save_rgba(terrain_path, assets / f"{identifier}-terrain.png")
        sprite = _save_rgba(sprite_path, assets / f"{identifier}-sprite.png")
        detector, annotated = audit_images(
            actual, terrain, sprite, _placement(case), **_thresholds(defaults, case)
        )
        annotated_name = f"{identifier}-annotated.png"
        annotated.save(assets / annotated_name, format="PNG")
        results.append({
            "id": identifier,
            "status": detector["status"],
            "component_count": detector["component_count"],
            "overlap_pixels": detector["overlap_pixels"],
            "metadata": metadata,
            "images": {
                "actual": f"assets/{identifier}-actual.png",
                "terrain": f"assets/{identifier}-terrain.png",
                "sprite": f"assets/{identifier}-sprite.png",
                "annotated": f"assets/{annotated_name}",
            },
            "detector": detector,
        })

    flagged = sum(case["status"] == "overlap_detected" for case in results)
    blocked = sum(case["status"] == "blocked" for case in results)
    report: dict[str, object] = {
        "schema_version": 1,
        "summary": {
            "total": len(results), "flagged": flagged,
            "clean": len(results) - flagged - blocked, "blocked": blocked,
        },
        "cases": results,
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n"
    )
    (output_dir / "review.html").write_text(_gallery(report))
    return report


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""manifest case example:
  {"id":"tc-west-000","actual":"actual.png","terrain":"terrain.png",
   "sprite":"town-center.png","screen_x":420,"screen_y":310,
   "anchor_x":180,"anchor_y":260,
   "metadata":{"entity":"Town Center","frame":0,"terrain":"grass"}}""",
    )
    parser.add_argument("manifest", type=Path, help="JSON manifest containing all sprite cases")
    parser.add_argument("--output-dir", type=Path, required=True, help="review bundle directory")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    args = parser.parse_args(argv)
    try:
        report = run_batch(args.manifest, args.output_dir)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 1 if report["summary"]["flagged"] else 0


if __name__ == "__main__":
    sys.exit(main())
