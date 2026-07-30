# DAT shadow composition fidelity

## Evidence boundary

The live VER 5.7 DAT parser exposes for every graphic: SLP ID, layer,
player-color field, frame count, angle count, mirroring mode, and recursive
delta graphic links with x/y offset and display-angle override. The inspected
asset maps repeatedly identify shadow children at layer 10 below body/projectile
layers 20/30. Examples include arrow 3378 → shadow 3379, throwing axe
3380 → 3381, Onager projectile 3396 → 3397, and the proved Palisade Wall
shadow SLP 4682 below wall SLP 1828.

This establishes draw ordering and synchronization metadata. It does not prove
that every dark-looking layer is a shadow. The guarded catalog therefore uses
only a direct child with exact layer 10, neutral `player_color=-1`, present
positive animation metadata, and unambiguous root-SLP association.

## Renderer contract

Mapped static and animated DAT composites already recurse through delta
graphics, stable-sort by layer, apply accumulated offsets, honor display-angle
overrides, and render lower layers first. This covers the mapped naval and
building composite paths in `generated/shadow_binding_catalog.json`.

Direct SLP unit animations and direct static/animated building body paths now
ask `find_exact_shadow_binding` for an exact DAT root match. A binding is
accepted only when:

- every DAT graphic sharing that root SLP agrees on the same shadow;
- there is exactly one direct layer-10 child;
- the child has its own present SLP ID and neutral player color;
- root and child frame/angle counts are positive;
- the shadow is static or has the same frames-per-angle as the root.

The shadow SLP is decoded without player remapping, preserves its own hotspot
and alpha, uses the DAT delta offset, derives direction from its own angle
count/display-angle rule, mirrors only through existing stored-angle policy,
and advances from the same action tick as the body. It renders before the body.

Ambiguous roots, multiple layer-10 children, player-colored children,
incompatible cadence, absent DAT/DRS/palette, malformed SLP, and missing action
mappings retain the existing procedural fallback. No shared or visually
similar root is inferred.

Nine formerly unbound standing building paths now use guarded binding: House,
Lumber Camp, Mining Camp, Blacksmith, University, Stone Wall, Market, Bombard
Tower, and Fish Trap. Construction binding applies only to Wonder, Outpost,
Fish Trap, and Bombard Tower paths actually rendered from direct archive art.
Bombard Tower death gains guarded binding; Wonder and Outpost death already
used archive animation shadows. Damage reuses standing body/shadow art and
keeps its procedural flame overlay; no distinct exact damage-layer claim is
made.

## Machine inventory

`generated/shadow_binding_catalog.json` contains one row for every 94
`UnitKind` and 27 `BuildingKind` values. Every building row separately records
standing, construction, damage, and death before/after status. Coverage is:

- standing shadows: 15/27 before, 24/27 guarded after;
- construction shadows: 0/27 before, 4/27 guarded after;
- damage-state standing shadows: 15/27 before, 24/27 guarded after, with
  procedural damage overlays unchanged;
- death shadows: 2/27 before, 3/27 guarded after.

The inventory separates:

- existing DAT animated/static composite paths;
- explicit exact Palisade Wall shadow composition;
- direct unit/building art eligible for guarded runtime root binding;
- guaranteed procedural fallbacks, including Farm and both Palisade Gate axes.

Regenerate it with:

```sh
python3 tools/generate_shadow_binding_report.py
```

The report is a renderer-path inventory, not a claim that proprietary payloads
are bundled or that every guarded candidate resolves on every installation.

## Tests

`shadow_catalog_tests` covers exact layer-10 resolution, DAT offsets, static
shadow acceptance, player-color rejection, mismatched-frame rejection,
multiple-child ambiguity, and conflicting shared-root offset/display-angle
rejection. Existing composite tests remain responsible for recursive layer
ordering and display-angle behavior.

## Capture plan

After build recovery, capture an action matrix under a valid
`AOE_ASSET_ROOT`:

1. infantry, cavalry, siege, and naval standing/moving/attack/death states for
   both players;
2. one static-shadow and one frame-synchronized shadow binding;
3. every mapped building composite plus Palisade Wall connectivity frames;
4. the same matrix without assets to prove procedural fallback;
5. a deliberately missing shadow SLP fixture to prove body art still renders;
6. side-by-side original-runtime comparison for hotspot, offset, direction,
   mirror, alpha, and action-frame synchronization.

Do not promote indirect, ambiguous, or player-colored layer-10 children based
only on visual resemblance.
