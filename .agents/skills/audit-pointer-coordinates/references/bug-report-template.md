# Pointer bug record

Use one section per deduplicated bug.

```markdown
## PTR-###: Short behavior mismatch

- Status: confirmed
- Product impact: menu / HUD / minimap / world picking / drag / other
- Affected matrix cells: exact modes, resolutions, DPI, map sizes, screens
- Duplicate manifestations: identifiers or none
- Expected: independently derived result
- Actual: observed result
- Reproduction:
  1. Exact launch command/configuration
  2. Exact state/setup actions
  3. Input coordinate and declared coordinate space
  4. Expected visible or semantic outcome
- Coordinate trace:
  - OS/global: value or not observed
  - SDL window event: value
  - renderer logical: value
  - surface-local: value
  - resolved target: value
  - resulting state: value
- Adjacent passing control: coordinate and result
- Reproducibility: N/N clean runs
- Evidence: screenshot/log/state paths
- Relevant production path: file and symbol
- Original evidence: decompiled symbol/path, documented contract, or `none found`
- Likely cause: hypothesis, clearly labeled
- Security classification: `not security-classified`, or complete five-field proof
```

Never put hypothesis in Expected or Actual. Never split one root defect merely to raise bug count. Preserve distinct defects when they have different failing transformations or state transitions.
