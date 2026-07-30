# Visual fidelity audit

## Audit boundary

This is a read-only source/evidence audit. No repository capture image was
present at audit time, and prior `/tmp` proof captures were ephemeral and no
longer available. Visual claims below therefore cite renderer paths and the
committed live-archive inventories; appearance that needs an original-runtime
side-by-side capture is marked **unverifiable** rather than inferred.

Status vocabulary:

- **exact** — identity, frame metadata, and rendered role are proved.
- **archive-backed** — decoded commercial art is used, but selection,
  composition, timing, crop, or layout is not fully proved.
- **procedural** — reconstruction geometry/color/text.
- **missing** — represented feature lacks the required visual.
- **unverifiable** — source or archive proves too little for a visual match.

Primary evidence is `src/sdl_app.cpp`, `TERRAIN_FIDELITY.md`,
`ELEVATION_FIDELITY.md`, `UI_ARCHIVE_FIDELITY.md`,
`RENDERER_ASSET_COVERAGE.md`, the bounded `*_ASSET_MAP.md` reports, and
`generated/renderer_asset_coverage.json`.

## Battlefield

| Area | Status | Current evidence and qualification |
| --- | --- | --- |
| Grass, water, beach, shallows top tiles | **archive-backed** | Live DAT maps SLPs 15001/15002/15017/15014; `load_terrain_archive_frames` loads the 100 physically present frames. Loose PNG override and procedural color fallback remain. Exact original frame-selection cadence and adjacency selection are not proved. |
| Terrain transitions / coast blending | **archive-backed, bounded** | `terrain_transition` decodes user-owned classic Blendomatic masks and composes proved masks 0–30 over exact terrain SLP frames. Hashed executable evidence proves North 12–15, East 4–7, South 0–3, West 8–11, and variant `(x + y) & 3`. Missing assets or destination position retain fallback. |
| Elevation projection | **procedural** | Stored levels shift isometric tile tops; south/east exposed faces, shading, and ambient-occlusion edge are reconstructed. `ELEVATION_FIDELITY.md` proves the archive has no extractable frames 100–115, so no exact slope/cliff face is claimed. |
| Elevation topology | **unverifiable** | Scenario bytes prove levels, not the reconstruction's face geometry, slope silhouettes, or cliff edge policy. Requires paired original-runtime captures of `resources/elevation-transition-matrix.scenario`. |
| Buildings, standing | **archive-backed, 24/27 kinds** | Coverage audit finds reachable sprite mappings for 24 of 27 represented kinds. Farm and both palisade-gate orientations remain procedural. Some composite roots/layers are absent, so “mapped” does not mean every civilization/state is exact. |
| Buildings, construction/damage/death | **mixed, classified fallback** | Exact death bodies cover 25/27 kinds. Construction roots resolve only layer-10 art; capture exposed an opaque rectangle when misclassified as bodies, so all 27 construction bodies fail closed to procedural rendering. Damage thresholds and replacement semantics are now runtime-proved and exact for 26/27 kinds as detailed below. See `BUILDING_BODY_STATE_FIDELITY.md`. |
| Walls and gates | **mixed** | Stone Wall uses five family SLPs and validated connectivity frames. Palisade Wall has exact archive shadow/wall/animated flag only at full health; damaged states are procedural. Stone gate axes use distinct roots. Palisade gates remain procedural because required composite components are absent. Open-state timing remains unverifiable. |
| Units, standing/moving/action | **archive-backed, bounded exact bindings** | All 96 represented kinds have reachable mapping evidence. Exact identity/layout contracts now guard Villager idle/move/attack/hunting, all three Militia and Knight states, Archer move/attack, and both Woad Raider tiers. Ambiguous Archer idle, generic gather, and unproved build/repair graphics fail closed to procedural fallback. Coverage still records absent Sheep attack SLP 3623; cadence and logical-angle selection remain unproved. See `ANIMATION_FIDELITY.md`. |
| Unit/building player colors | **exact archive-backed remap** | SLP player-color commands use palette 50500 and DAT bases `[16,32,48,64,96,112,128,80]` for roster slots 0..7. Source indices 0..9 map directly to `base + source`; neutral remains unchanged. Blue/red output remains byte-identical. Missing/unsupported slots fail closed, never alias red. Unit, building, death, and animated-composite caches carry all eight variants. See `PLAYER_COLOR_PALETTE_FIDELITY.md`. |
| Building damage states | **exact for 26/27 kinds** | Runtime-proved strict damage thresholds select exact DAT/DRS animated compositions for 103/104 civilization families. Flag 0 overlays standing or construction art; Stone Wall flag 2 replaces standing art. Repair reverses selection and record changes reset animation. Fish Trap roots contain no drawable layer and fail closed. See `BUILDING_DAMAGE_RUNTIME_EVIDENCE.md`. |
| Shadows | **archive-backed where DAT-proved; procedural fallback** | Existing DAT composites sort/render exact lower layers. Direct unit and building paths bind only one unambiguous neutral layer-10 child with exact DAT offsets/frame/direction/cadence metadata and present DRS payload. Building standing coverage rises from 15/27 to 24/27 guarded paths; ambiguous/missing roots keep procedural fallback. See `generated/shadow_binding_catalog.json`. |
| Animation cadence | **procedural timing over archive frames** | Archive frames are genuine where mapped, but `animation_tick` modulo logic and fixed frame stepping are reconstruction policy. DAT frame duration/action synchronization has not been shown to drive all states. |
| Projectiles and impacts | **mixed, cataloged** | Exact DAT-guarded flight coverage rises from 1/9 to 8/9 represented families; both proved impact families animate all frames instead of frame zero. Exact installer SLP counts plus pinned angle code prove Scorpion's 18-direction body/shadow transform. Arrow remains procedural because its 176 body frames do not satisfy the proved 187-frame 32-direction layout. See `PROJECTILE_IMPACT_FIDELITY.md` and generated evidence JSON. |
| Fog of war | **procedural compositing, exact geometry contract** | Executable and exact `TileEdge.Dat`/`BlkEdge.Dat` prove compass bits, 17 shape tables, 47 canonical edge classes, 256-mask normalization, hidden/explored/visible table selection, and 0xff-terminated add/remove row spans. Payloads contain geometry only, not palette or alpha. Final hidden/explored colors, blending, and minimap fog colors remain unproved, so SDL compositing stays procedural. See `FOG_RENDERING_FIDELITY.md`. |
| Selection, health, order feedback | **mixed, bounded** | Runtime uses the original inclusive 25x2 health geometry, truncation/gates, and exact RGB values of palette indexes 36/241. Executable evidence also proves original procedural white square/cube selection frames; see `SELECTION_FEEDBACK_RUNTIME_EVIDENCE.md`. Current stance badges, waypoint/order lines, build footprints, attack-range diagnostics, and allied signals remain reconstruction overlays. `health.shp` and `unithalo.shp` load identities are known, but their world-feedback roles are unproved. |

## Interface

| Area | Status | Current evidence and qualification |
| --- | --- | --- |
| In-game HUD background | **procedural / exact compositor cataloged, original asset missing** | Executable selects loose `game_b%d.slp` by civilization with resource ID -1 and composites frames 0–7. One-frame DRS SLP 51141 is structurally incompatible; prior bottom-218 crop is disproved, not archive-backed fidelity. Exact numeric split awaits original loose frame dimensions. See `HUD_LAYOUT_FIDELITY.md`. |
| Beveled panels and windows | **procedural** | Multiplayer lobby/chat, diplomacy, options, statistics, save browser, command panels, campaign screens, editor overlays, and technology tree use reconstruction rectangles, borders, colors, and spacing. |
| Main menu/setup screens | **procedural** | Menu composition, background, buttons, and typography are generated. No proved original menu-screen SLP/layout contract is applied. |
| Fonts and localized text rendering | **procedural / missing original font** | UI uses `SDL_RenderDebugText`. String-table localization may be archive-backed, but glyph shapes, kerning, wrapping, shadows, colors, and original font metrics are not. |
| Resource and action icons | **partial executable-exact** | Technology `+0x2c` dispatches unchanged to SLP 50729; ordinary-unit `+0x54` dispatches unchanged to SLP 50730. Train buttons use this exact subset. Command meanings, resource frames, disabled variants, and page ordering remain unproved. |
| Portrait frame | **archive-backed / inferred** | SLP 50713 metadata is proved; semantic use of frame 0 is inferred. Full unit/building portrait atlas selection is missing. |
| Cursor | **exact asset, bounded behavior** | Cursor sheet SLP 51000 has 19 exact frames/hotspots. Executable selectors prove static frame 0 for normal/restore and frame 6 for modal busy/wait, plus exact show/hide paths; SDL uses the proved normal contract. Gameplay and scroll-state mappings remain unproved and deliberately fall back to frame 0. See `CURSOR_FIDELITY.md`. |
| Minimap | **mixed, exact bounded contracts** | Runtime uses proved source-diagonal row sampling, exact eight-player marker palette RGB, 3×3 size-one bounds, type `0x112` center±4 signal outline and 333 ms phase, exact 1024-class frame placement, and proved viewport bounds. Terrain palette/fog semantics remain procedural. Unproved viewport polygon raster, alternate signal RGB, active frame crop, and 640 anchor fail closed rather than using prior beige/floating-point guesses. See `MINIMAP_RUNTIME_EVIDENCE.md`. |
| Command grid/hotkey labels/tooltips | **mixed, exact train icons** | Fifteen trainable unit actions use exact DAT icon IDs as unchanged frames in SLP 50730. Technology IDs likewise map exactly to SLP 50729 in the pure contract; raw command codes prove four SLP50721 frames, but no safe `PanelCommand` bridge. Layout/order, disabled states, tooltip geometry, hotkey underline, and pagination remain unmatched. Pressed state is proved to preserve the icon frame while shifting it by one pixel inside alternate chrome. See `UI_ICON_EVIDENCE.md`. |
| Chat/signal log | **procedural** | Bounded functionality exists, but type, colors, location, click target, and signal marker are reconstruction choices. No sound is used without exact sound-resource proof. |
| Menus/panels transitions | **missing** | No proved original fades, slide/open animations, modal dim treatment, or button-state animation inventory is implemented. |

## What can and cannot be called exact

Exactness is currently strongest at narrow asset identity boundaries: proved
terrain SLP identities, the normal cursor bitmap, specific wall/gate family
roots, and individual decoded animation frames. The composed screen is not
pixel-exact. Camera projection, scale, crop, frame timing, fog, shadows, UI
layout, debug font, minimap, and procedural overlays materially change the
result even when an original bitmap is present.

No screenshot-to-screenshot metric can be reported from this audit because no
current reconstruction capture and no paired original-runtime reference
capture were available in the workspace. That comparison remains required for
all **unverifiable** rows.

## Next three implementable visual gaps

1. **Capture cardinal terrain-mask variants.** Static analysis now proves
   cardinal families 0–15 and `(x + y) & 3`; paired original-runtime captures
   remain needed to validate rendered silhouettes and alpha placement.

2. **Capture building-shadow parity.** Static building gap is now closed where
   exact bindings can be proved: standing coverage rises from 15/27 to 24/27,
   with separate construction/damage/death inventory. Capture side-by-side
   offset, direction, cadence, alpha, and fallback behavior under live assets.

3. **Original font and command-icon atlas contract.** Audit interface DRS/DAT
   links for font resources and button-picture-to-frame mapping, then replace
   debug text and inferred action frames only where identities are proved.
   This upgrades every HUD/menu/panel while preserving procedural fallback for
   unresolved frames.

The farm and palisade-gate procedural gaps are real, but live archive evidence
already says required roots/components are absent. They are not ranked in the
top three because they are not immediately implementable without a new,
independently proved composition source.
