# Classic AI script fidelity

## Scope

`inspect_legacy_ai_script` is a bounded inspector for classic AoC-style
`.ai`/`.per` source. It recognizes:

- integer `defconst` declarations;
- ordinary `load` and weighted `load-random` declarations;
- `defrule` facts, the `=>` separator, and actions;
- quoted load paths, semicolon comments, and nested parenthesized syntax.

Every rule, fact, action, load, and unsupported top-level form retains its
source offset, length, and exact raw text. Inspection never drops an unknown
form. Syntax errors fail inspection; recognized-but-unsupported semantics make
the complete script inspection-only.

## Evidence

Grammar and behavior are bounded by:

- Microsoft’s classic *Computer Player Strategy Builder* guide, mirrored by
  the UserPatch AI scripting reference, which defines `defrule`, `defconst`,
  `load`, `load-random`, semicolon comments, and conditional load directives;
- open-source AoE2 AiScript grammar at commit
  `02054e0fb6a1ad70027685d8216c40d2013e5f17`;
- real public `.per` scripts used to check declaration and load shapes.

The inspector does not claim full Genie AI compatibility. Conditional
`#load-if-defined`/`#load-if-not-defined` directive tokens are preserved as
unsupported spans until a caller supplies proved game-setting symbols and
package-level file resolution. This refusal prevents either branch from being
silently selected.

## Limits

- source: 4 MiB;
- syntax nodes: 200,000;
- nesting: 64;
- rules: 10,000;
- facts or actions per rule: 128;
- weighted random-load probabilities: each 0–100, cumulative maximum 100.

Quoted escape sequences are rejected rather than guessed. Duplicate constants,
malformed rules, invalid random loads, and stray atoms remain unsupported and
block executable mode.

## Deterministic execution contract

Executable mode is allowed only when every rule uses the supported subset:

Facts:

- `true`;
- `food-amount`, `wood-amount`, `gold-amount`, `stone-amount`;
- `population`;
- `current-age`;
- `unit-type-count`;
- seeded `random-number`.

Actions:

- `train`, `research`, `build`, `attack-now`;
- `tribute-to-player`;
- `set-diplomacy`.

Rules evaluate once in source order against an immutable state snapshot.
Comparisons support `==`, `!=`, `<`, `<=`, `>`, and `>=`. Random facts use the
documented execution seed and fixed integer arithmetic. Default budgets are
4,096 evaluated rules and 1,024 emitted actions; callers may reduce both.
Budget exhaustion is explicit.

Unit, building, technology, resource, and age names require explicit
current-engine mappings. Execution emits typed deterministic intents. It does
not guess a producer building, builder, construction tile, attack force, or
target. Those are higher-level engine policy choices and must be applied by a
consumer explicitly.

Any unknown fact/action, unsupported source span, absent mapping, bad player,
bad amount, or unknown diplomatic relation blocks execution. No supported
actions run from a partially understood script. Parsed `load` and
`load-random` declarations also block execution until a package resolver has
expanded the selected files; inspection never treats an unresolved load as an
empty script.

## Tests

Hermetic tests cover comments, constants, loads, weighted/default random loads,
rule spans, resource/population/age/unit facts, all supported action intents,
source ordering, mapping failures, unknown-semantics refusal, raw unsupported
span preservation, and rule/action budgets.

## Supplied-package audit

The supplied `original-assets-hd/app/AI` directory contains only `AI.txt`, whose
entire content is the literal `AI.txt`. It provides neither a `.ai` manifest
nor referenced `.per` files, selection symbols, or package resolution context.
Per the fail-closed contract, this is inspection-only evidence. No branch,
load, or executable AI package is invented from it.
