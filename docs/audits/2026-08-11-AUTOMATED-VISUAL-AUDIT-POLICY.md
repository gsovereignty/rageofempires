# Automated visual-audit policy addendum — 2026-08-11

This addendum changes classification for future audits without rewriting dated
historical evidence:

- every known procedural, synthetic, placeholder, debug, missing-asset, or
  fallback production visual is a confirmed visual defect;
- missing provenance is blocked only when automated evidence cannot establish
  whether the rendered source is an approved mapped asset;
- visual testing and classification are automated by default; no human review
  or testing is requested unless a user explicitly requests it for that task;
- human-reported visual bugs remain eligible for individual investigation.

Earlier audit language treating intentional procedural rendering as blocked or
acceptable is superseded by
[`PRODUCTION_VISUAL_ASSETS.md`](../contracts/PRODUCTION_VISUAL_ASSETS.md).
