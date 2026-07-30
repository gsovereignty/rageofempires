# Conversion roll runtime evidence

## Provenance and setup

- Binary: `../Crack/AoK HD.exe`
- SHA-256:
  `e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc`
  (supplied corpus manifest).
- Static capture: `../decompiled/AoK-HD-patched.c`, function
  `FUN_00413a80` at virtual address `0x00413a80`; address lookup is pinned by
  `../decompiled/function-index.tsv`.
- Numeric conversion helper: `FUN_0072421c` at `0x0072421c`.
- Reproduction fixture:
  `tests/simulation_tests.cpp::executable_conversion_arithmetic_is_exact`.

The fixture is hermetic. Run `aoe_core_tests`; its first case exercises the
captured integer scaling, threshold branches, and floating-point rounding.

## Static observation

Within the conversion action path in `FUN_00413a80`, the patched executable:

1. calls the CRT random source;
2. computes `(random * 100) / 0x7fff`;
3. accumulates target resistance as a floating-point value;
4. when resistance is positive, converts the scaled floating-point result
   through `FUN_0072421c`;
5. selects `-1000` before the minimum-time boundary and `1000` at/after the
   maximum-time boundary;
6. succeeds when the scaled roll is less than or equal to the selected
   threshold.

The recovered branch also distinguishes building conversion parameters from
ordinary-unit parameters. This note does not infer the meaning of every
object-class or resource offset in that branch.

## Interpretation

Confidence is high for integer random scaling, inclusive comparison, forced
minimum/maximum thresholds, and the executable’s floating-to-integer helper
path. Confidence is low for unnamed resistance contributors and commercial
target-class policy.

The reconstruction function `evaluate_conversion_check` preserves only the
proved arithmetic seam. Its inputs remain explicit; native simulation chooses
the still-documented deterministic resistance/timing policy.

## Known incompatibilities

- Native matches do not consume the commercial CRT random stream.
- Native resistance timing and participant selection remain reconstruction
  contracts documented in `RELIGIOUS_ASSET_MAP.md`.
- No original-runtime save, replay, or multiplayer equivalence is claimed.
- Unknown resource/object-class offsets remain untranslated.
