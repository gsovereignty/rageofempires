# Production visual asset policy

Production rendering must use mapped, tracked visual assets. Procedural,
synthetic, placeholder, debug, missing-asset, and fallback visuals are defects,
including when they are deterministic or preserve correct simulation state.
No visual category is exempt: terrain, resources, units, buildings, shadows,
damage states, projectiles, impacts, effects, HUD, menus, minimap, and terminal
screens all follow this rule.

Automated audits fail a production frame as soon as telemetry, provenance, or
pixel evidence proves that such rendering occurred. Missing evidence remains
blocked only when automation cannot determine whether the visible result came
from a mapped asset. A known procedural or fallback draw is never blocked and
never intentional.

Visual audits use automated telemetry, provenance, synchronized screenshots,
pixel comparisons, ordered frame analysis, and reference-asset checks by
default. They must not request or schedule human testing, screenshot review,
contact-sheet review, or subjective classification unless a user explicitly
requests human testing for the current task. Human-reported visual bugs are
investigated individually.

Documentation describing an existing procedural fallback records current
implementation behavior, not accepted fidelity. This contract controls its
audit classification until that path is replaced with mapped asset rendering.
