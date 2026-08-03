## Build on macOS

The root `Makefile` provides the shortest development workflow:

```sh
cd reconstruction
make
make run
make test
```

`make` configures a Release build and compiles it. `make run` launches the
Finder-compatible app bundle on macOS, while `make test` builds and runs the
complete CTest suite. `make clean` removes compiled outputs while preserving
the CMake configuration.

The build directory, build type, parallelism, and extra CMake options are
overrideable:

```sh
make BUILD_DIR=build-local BUILD_TYPE=Debug JOBS=8
make CMAKE_ARGS="-DAOE_BUILD_SDL3=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sdl3"
```

The equivalent direct CMake workflow is:

```sh
cd reconstruction
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/aoe_reconstruction
```

Apple builds default to Universal 2 (`arm64` and `x86_64`) with a macOS 11.0
deployment target. CMake extracts vendored SDL3 3.4.12 source from
`third_party/SDL-3.4.12-f87239e.tar.gz` and compiles both slices under the same
target, avoiding network access and host-package deployment-version leakage.
Set `AOE_BUILD_SDL3=OFF` and provide `SDL3_DIR` only for local development with
a compatible prebuilt SDL.

Finder-compatible app bundle:

```sh
open "build/AoE Archaeology.app"
```

The game is self-contained. Development builds and installed bundles do not
search sibling directories or accept an external asset-root override. Required
legacy sprite inputs live under `game_data/Data` in this repository and are
copied beside the development executable or into the app bundle. Procedural
rendering remains the deterministic fallback for missing or unreadable sprite
records.

Legacy research tools may inspect an explicitly supplied archive path outside
the normal build/test/runtime graph. Product binaries never consume that path.
See [self-containment contract](../SELF_CONTAINMENT.md).

Packaged game data may contain loose `Terrain/Textures` PNGs for Grass, Water,
Beach, and Shallows. Missing loose textures fall back individually to SLPs
15001, 15002, 15017, and 15014 from `Data/terrain.drs`, using palette 50500
from `Data/interfac.drs`. Forest, berry, gold, and stone ground tiles reuse
Grass; fog shading remains active. Resource objects prefer live archive sprites
for berry 2560, gold 2561, stone 1034, and fish 420, with procedural silhouettes
retained per resource when its mapped sprite is missing or unreadable. Missing,
unreadable, or unset terrain assets fall back individually to built-in
procedural terrain. Original assets stay at their source path and are never
copied into build outputs.

When the same local root contains `Data/graphics.drs` and
`Data/interfac.drs`, original sprites are also used for sheep, deer, boar,
villagers, current military units, oak/forest trees, and gold mines. Directional
animations use the five stored viewing angles plus mirrored eastern views.
Mappings come from `empires2_x1_p1.dat` (animal SLPs 3629, 342, and 2557;
villager 1479; tree 4652; gold 2561), with palette 50500 from `interfac.drs`.
Missing archives, palette entries, mapped SLPs, or undecodable frames fall back
to procedural art. Town Center mapping is
recognized as a multi-layer DAT composition, but remains procedural when any
required layer is absent. DRS contents are read from packaged `game_data` and
never from the parent research workspace.

To inspect one original sprite as a BMP contact sheet, build the tools and pass
its SLP resource ID directly:

```sh
cmake --build build --target slp_contact_sheet
./build/slp_contact_sheet \
  game_data/Data/graphics.drs \
  game_data/Data/interfac.drs \
  3629 \
  /tmp/sheep.bmp
open /tmp/sheep.bmp
```

The tool prints each frame's index, dimensions, and hotspot. Its older loose
SLP form remains available through `slp_contact_sheet SLP INTERFAC_DRS OUTPUT`.

Release bundle embeds SDL3 under `Contents/Frameworks`; it has no Homebrew
runtime dependency. Verifier checks both Universal 2 slices and macOS 11.0 in
the executable, SDL dylib, and Info.plist alongside resources, linkage, signature,
bundled-scenario loading, headless rendering, screenshot dimensions, and clean
app exit:

```sh
./scripts/verify_macos_bundle.sh "build-release/AoE Archaeology.app"
```

Fresh-copy isolation gate:

```sh
./scripts/test_isolated_build.sh .
```

This copies product inputs, including `game_data`, to a temporary directory
outside the workspace, performs a fresh configure/build, launches the copied
executable, and proves its sprite loader resolves only copied packaged data.

For headless visual audits, set `AOE_SCREENSHOT_PATH` to a `.bmp` path. First
fully rendered frame is captured before presentation, including under SDL's
dummy video driver. `AOE_WINDOW_SIZE=800x600` can exercise resized,
letterboxed output. `AOE_SCREENSHOT_TICK` and `AOE_SCREENSHOT_ALPHA` delay
capture to a chosen simulation tick and interpolation fraction.
`AOE_SCENARIO_PATH` loads a custom scenario for targeted render audits.
`AOE_RENDER_FALLBACK_REPORT` writes deduplicated structured JSON whenever
runtime rendering reaches a reviewed procedural path or an unresolved asset
failure. See `docs/assets/RENDERER_ASSET_COVERAGE.md` for report fields and audit gates.
`AOE_SCREENSHOT_SELECT_TILE=X,Y` selects an initial blue entity so
selection-only HUD and order markers can be audited headlessly.
`AOE_SCREENSHOT_ATTACK_GROUND=X,Y` gives selected Mangonels an initial
Attack Ground order for projectile and target-marker audits.
`AOE_SCREENSHOT_COMMAND_TILE=X,Y` issues one normal contextual command to
the initial screenshot selection.
`AOE_EXIT_AFTER_SCREENSHOT=1` closes cleanly after a successful capture,
supporting bounded CI and bundle smoke tests without synthetic input.
`resources/rubble-audit.scenario` provides a focused destruction scene; its
ram destroys the red house before a tick-15 rubble capture.
`resources/damage-audit.scenario` shows half-health and critical-health fire
stages. Scenario v27 accepts validated `hit_points N` building markers.
`resources/villager-work-audit.scenario` exposes gathering tools and carried
wood when selected at `2,2`, commanded to `3,2`, and captured at tick 3.
`resources/villager-repair-audit.scenario` exposes hammer animation against a
burning house when selected at `3,2` and commanded to `4,2`.
`resources/combat-pose-audit.scenario` exposes melee and ranged attack frames
at tick 2.
`resources/movement-gait-audit.scenario` exposes foot, hoof, and wheel gait at
tick 1 with `AOE_SCREENSHOT_ALPHA=0.25`.
`resources/farm-render-audit.scenario` shows full, partly harvested, and
exhausted crop plots side by side.
`resources/water-render-audit.scenario` exposes animated pond wavelets and
shoreline transitions without revealing unexplored neighboring terrain.
`resources/sheep-gather-audit.scenario` exposes sheep pathing and finite food;
select `2,3`, command `5,3`, and capture tick 4.
`resources/huntable-animal-audit.scenario` places passive Deer and a dangerous
retaliating Boar beside blue hunters for deterministic hunt/carcass audits.
`resources/monk-conversion-audit.scenario` places a Castle Age Monk and enemy
Knight at conversion range for deterministic progress/recharge audits.
`resources/monk-relic-healing-audit.scenario` places a Monk, friendly
Villager, neutral Relic, and Monastery together for support/economy audits.
`resources/market-exchange-audit.scenario` places completed Feudal Markets for
both players with ample stock for deterministic shared-price exchange audits.
`resources/allied-trade-audit.scenario` supplies allied Markets and a blue
Trade Cart for route income, shared-vision, and non-hostile targeting audits.
`resources/civilization-bonuses-audit.scenario` contrasts Castle Age Briton
Crossbowman range with Frank Knight hit points without inventing unique units.
`resources/expanded-civilizations-audit.scenario` contrasts Viking infantry
hit points with a Byzantine counter unit and age-scaled building durability.
`resources/asian-saracen-civilizations-audit.scenario` places faster-attacking
Japanese infantry against a double-HP Persian Town Center.
`resources/final-civilizations-audit.scenario` contrasts Turkish Scout-line
pierce armor with Mongol Scout-line hit points.
`resources/conquerors-civilizations-audit.scenario` places a Korean Villager
and free-upgraded tower opposite a Mayan Archer/Archery Range.
`resources/naval-fishing-audit.scenario` places a Fishing Ship, 200-food fish
node, and shoreline Dock for deterministic water pathing/economy audits.
`resources/naval-combat-transport-audit.scenario` places opposing Galleys and
a shoreline Transport/Villager pair for projectile and passenger audits.
`resources/fire-demolition-ship-audit.scenario` places both classic Fire Ship
and Demolition Ship lines, with blue Imperial upgrades, for combat audits.
`resources/cannon-dock-technologies-audit.scenario` contrasts Cannon Galleon
levels and exposes Careening, Dry Dock, and Shipwright effects.
`resources/unique-naval-units-audit.scenario` contrasts Viking Longboat and
Korean Turtle Ship lines at base and elite levels.
`resources/castle-unique-units-audit.scenario` contrasts Briton Longbowman
and Teuton Teutonic Knight lines at base and elite levels.
`resources/eastern-castle-unique-units-audit.scenario` contrasts Japanese
Samurai and Persian War Elephant lines at base and elite levels.
`resources/loom-render-audit.scenario` exposes a selected Loom-upgraded
Villager with 40 HP and 1/2 armor in HUD.
`resources/double-bit-axe-audit.scenario` exposes upgraded wood gathering;
select `4,2`, command `5,2`, and capture tick 5 for six carried wood.
`resources/horse-collar-render-audit.scenario` shows 250-food full,
half-used, and exhausted upgraded farm density.
`resources/palisade-gate-render-audit.scenario` shows an automatically opened
NW-SE Palisade Gate beside a friendly Villager and a closed NE-SW gate.
`resources/stone-gate-render-audit.scenario` shows matching open and closed
Stone Gates in both orientations.
`resources/ai-building-attack-audit.scenario` shows red computer siege
explicitly attacking blue player's final structure.
`resources/footprint-visibility-audit.scenario` proves a large enemy building
appears in world and minimap when any footprint tile enters vision.
`resources/wheelbarrow-movement-audit.scenario` runs upgraded and baseline
Villagers in parallel; blue leads by one tile at tick 20.

Controls:

- `Ctrl+1` through `Ctrl+9` assigns selected units or building to a control
  group. macOS `Command+number` works too. `Shift+number` adds selected units;
  plain number recalls a populated group and filters destroyed entities.
  Existing numeric build/research hotkeys remain active while slot is empty.
- Press `.` to cycle through idle Villagers. `Shift+.` selects every idle
  Villager. HUD keeps live idle count; gathering, construction, repair,
  movement, combat, garrison, guard, patrol, and queued orders count as work.
- Press `,` to cycle through idle military units. `Shift+,` selects all idle
  military. Movement, targeting, Attack Move, Attack Ground, patrol, guard,
  garrison, stance-return, and queued waypoints count as active orders.
- Left click blue unit: select.
- Shift-click blue unit: add it to selection or remove it when already
  selected. Double-click selects every visible blue unit of same type.
- Left drag: select multiple blue units. Hold Shift to merge box contents
  into current selection.
- Move pointer against map edge or use arrow keys to scroll camera. Mouse wheel
  zooms from 1× to 2× around pointer. Click minimap to center camera.
- `Ctrl+F1` compact, `Ctrl+F2` line, `Ctrl+F3` box, `Ctrl+F4` staggered,
  `Ctrl+F5` flank.
- Right click grass: move selected units in current formation.
- Hold Shift while right-clicking grass to append formation waypoints. Units
  traverse every leg in order; Stop or an unmodified order clears the queue.
- Right click with a completed building selected: set its rally point. Newly
  trained units immediately route there; villagers can rally directly onto a
  resource to begin gathering.
- Right click tree, berry bush, gold deposit, or stone deposit with villager:
  gather, carry up to 10, and return resources to nearest town center.
- Right click sheep with villager: gather its finite 100 food, deliver food,
  and continue until sheep is depleted. Sheep provide vision, move when
  selected, use no population, and do not keep an otherwise defeated player
  alive. Computer players gather owned sheep rather than commanding them as
  military units.
- Right click live Deer or Boar with a Villager to hunt it. Deer remain
  passive and leave 140 food. Boar carry 340 food, retaliate against their
  attacker, chase it, and can kill an unsupported Villager. Right click the
  carcass to gather and deliver its finite food. Deer and Boar use no
  population, provide no military survival, and are never AI military units.
- Castle Age Monasteries cost 175 wood and train Monks for 100 gold. A Monk
  converts one visible enemy unit within nine tiles after eight uninterrupted
  simulation ticks; success changes ownership and starts a 20-tick recharge.
  Friendly units, animals, dead units, garrisoned units, and unseen or
  out-of-range targets are invalid. Monks also heal damaged friendly organic
  units within four tiles by one HP per simulation tick. They can carry one
  adjacent neutral Relic and deposit it at an adjacent completed friendly
  Monastery; each stored Relic generates one gold every ten simulation ticks.
  Relics are neutral, unselectable by either player, noncombatant, population
  free, and do not keep a player alive in victory checks.
- Feudal Age Markets cost 175 wood. Food, wood, and stone trade in 100-unit
  lots against gold. Each commodity starts at a 100-gold base: buying costs
  130 gold and selling returns 70 gold, reproducing the original 30% fee.
  Every buy raises the shared base by 3 and every sale lowers it by 3, with
  the original-style 20-gold floor. A completed owned Market is required.
  Diplomacy can mark the two players allied, neutral, or enemy; enemy remains
  the default. Allies share vision and cannot attack or convert one another.
  Markets train 100-wood/50-gold Trade Carts, which use one population and
  repeatedly carry distance-scaled gold between an owned Market and a
  completed allied Market. Neutral and enemy Markets never permit trade.
- Civilization identity defaults to `generic` and is isolated per player.
  Briton Archers, Crossbowmen, and Arbalesters gain +1 range in Castle Age and
  another +1 in Imperial Age. Frank cavalry receive +20% base hit points,
  including future cavalry recognized by the shared cavalry classification.
  Teuton Farms cost 36 rather than 60 wood and Teuton Monks heal within eight
  tiles rather than four. DAT availability restrictions cover every currently
  represented unit, building, and technology; bonuses requiring an absent
  engine system remain omitted rather than approximated.
  Goth infantry costs 20% less in Dark Age, 25% in Feudal, 30% in Castle, and
  35% in Imperial; Goths also gain ten population capacity in Imperial Age.
  Celt Rams and Mangonels fire 25% faster. Viking infantry gain 20% HP from
  Feudal Age onward. Byzantine buildings gain
  10/20/30/40% HP by age, while Spearmen, Pikemen, Skirmishers, and Elite
  Skirmishers cost 25% less.
  Japanese infantry attack 33% faster from Feudal Age. Chinese technologies
  cost 10% less in Feudal, 15% less in Castle, and 20% less in Imperial.
  Persian Town Centers have double hit points. Persian Town Centers and Docks
  work 10/15/20% faster in Feudal/Castle/Imperial Age. Saracen Markets use a 5% fee,
  producing initial 105-gold buy and 95-gold sell prices instead of 130/70.
  Generated random maps give Chinese three extra Villagers, minus 50 wood,
  and minus 200 food; authored Scenarios remain unchanged. Bonuses lacking a
  represented damage or economy system remain explicitly omitted.
  Turkish Scout Cavalry, Light Cavalry, and Hussars gain +1 pierce armor.
  Mongol Scout-line units gain 30% HP, and Mongol Rams and Mangonels fire 25%
  faster. Spanish builders work 30% faster and Blacksmith technologies cost no
  gold. Huns need no Houses in this bounded 200-pop ruleset, and Hun Stables
  work 20% faster. Mongol hunters work 50% faster and Turkish gold miners work
  15% faster. Reciprocal allies receive Chinese +45 farm food, Byzantine 50%
  faster Monk healing, Persian Knight-line +2 attack versus archers, Saracen
  foot-archer +1 attack versus buildings, 15%-cheaper Viking Docks, and
  50%-cheaper Mayan walls. Spanish trade gold and Korean Mangonel-line minimum
  range also propagate dynamically. Aztec relic gold uses a per-player
  persisted hundredths remainder for exact 33% accumulation. Save110 stores
  both player remainders; Save107 and older migrate them to zero. Teuton
  conversion resistance remains unsupported because this deterministic
  conversion model lacks an evidenced AoC resistance formula. Cavalry Archers
  and all 18 Castle unique-unit lines are represented.
  Korean Villagers gain +3 line of sight, with Guard Tower free in Castle Age
  and Keep free in Imperial Age. Aztec Villagers carry three extra resources,
  and Aztec military units train 11% faster. Mayan archer-class units cost
  10% less in Feudal, 20% less in Castle, and 30% less in Imperial. Korean
  stone miners work 20% faster. Aztec Monks gain five hit points per researched
  Monastery technology. Generated random maps give Mayans one extra Villager
  and minus 50 food. Their finite resources last 15% longer through per-player
  credited-yield/depletion accounting, including Farms, animals, fish, and
  shared nodes. Vikings receive free Wheelbarrow in Feudal Age and free Hand
  Cart in Castle Age.
- Dark Age Docks cost 150 wood, use a 3×3 shoreline footprint, and train
  75-wood Fishing Ships. Fishing Ships use one population, travel only across
  water/fish tiles, carry 15 food, harvest finite 200-food fish nodes, and
  deposit at completed owned Docks before returning automatically. Land units
  cannot traverse fish or water. This first naval slice intentionally omits
  deep-sea/shore-fish rate differences, fish traps, naval combat, transports,
  Dock technologies, and civilization-specific ship bonuses.
- Feudal Docks train 90-wood/30-gold Galleys (120 HP, 6 attack, range 5).
  Castle War Galley research upgrades the line to 135 HP, 7 attack, range 6;
  Imperial Galleon research upgrades it to 165 HP, 8 attack, range 7.
  Galley-line ships use water-only ranged projectile combat against ships and
  shoreline buildings. Dark Age Transport Ships cost 125 wood, use one
  population, carry five adjacent friendly land units, and disembark onto an
  adjacent open shore tile. Passenger state persists; passengers sink when
  their Transport is destroyed or deleted. This bounded slice omits naval
  formations, repair-at-sea, transport capacity technologies, and naval AI.
- Castle Docks train 75-wood/45-gold Fire Ships and 70-wood/50-gold
  Demolition Ships. Imperial Fast Fire Ship and Heavy Demolition Ship research
  upgrades existing and future ships. Fire Ships use rapid range-2 attacks.
  Demolition Ships chase into contact, self-destruct, and damage every nearby
  ship regardless of diplomacy; their class-11 damage also hits shoreline
  buildings. Save, replay, and scenario formats preserve both lines.
- Imperial Cannon Galleons cost 200 wood/150 gold and fire 50%-accurate siege
  projectiles from range 13 with a three-tile minimum range. Elite Cannon
  Galleon raises HP, attack, LOS, range, armor, and building damage from exact
  live DAT records. Careening adds one ship pierce armor and five Transport
  capacity; prerequisite-gated Dry Dock raises total Transport capacity to 15
  and all current/future ship speed by 15%. Shipwright cuts eligible ship cost
  to 80% and training time to 65%. Effects, research, production, saves,
  scenarios, and replays remain deterministic.
- Castle-age Longboats are Viking-only; Castle-age Turtle Ships are
  Korean-only. Their Imperial elite research remains civilization-locked and
  upgrades current and future units. Longboats launch one damaging projectile
  plus four deterministic visual volley arrows using DAT spread values.
  Turtle Ships use one heavy siege projectile with zero minimum range.
  Exact DAT HP, attack, range, reload, armor, cost, training, and movement
  rates drive both lines. Save, scenario, and replay round trips preserve them.
- Castles train civilization-locked Longbowmen for Britons, Throwing Axemen
  for Franks, Huskarls for Goths, and Teutonic Knights for Teutons. Their
  civilization-locked Imperial research upgrades current and future units.
  Longbowmen receive archer attack/range and armor technologies; other three
  lines receive infantry attack and armor technologies. Supported DAT classes
  preserve Longbow anti-spear, building bonuses, and Huskarl anti-archer
  damage. Eagle Warriors now provide reconstructed class-29 targets. The live
  Huskarl elite prerequisite label mismatch is recorded in
  `docs/assets/LAND_UNIQUE_ASSET_MAP.md`; this bounded engine enforces Goth civilization
  and Imperial Age without inventing tech 270.
- Japanese Samurai, Chinese Chu Ko Nu, Byzantine Cataphract, and Persian War
  Elephant lines use exact civilization locks and live-DAT elite upgrades.
  Samurai deal proved class-19 bonus damage to unique units. Chu Ko Nu attacks
  use one damaging arrow plus proved visual volley arrows; no unsupported
  full-damage multishot is invented. Cataphracts receive cavalry blacksmith
  upgrades and proved anti-infantry damage, but no splash because their DAT
  radius is zero. Eagle Warriors now supply class-29 targets. Only Elite War
  Elephants trample: DAT radius 0.5 is enclosed
  by one adjacent integer tile, using the engine's deterministic full-damage
  splash convention. Base War Elephants have no splash.
- Right click adjacent enemy: attack.
- Select military units, press `A`, then right click: attack-move in formation.
  Units engage visible enemies en route, then continue to destination.
- Select Mangonels, press `X`, then right click any map tile: Attack Ground.
  Mangonels move into range, respect their three-tile dead zone and reload
  time, then launch one friendly-fire splash projectile. Building-selected
  `X` remains Fletching research.
- Select military units, press `P`, then right click: patrol between current
  positions and formation endpoints, returning after combat.
- Select military units, press `G`, then right click a friendly unit or
  completed building: guard it and return after nearby combat. Villager `G`
  still enters Mining Camp build mode.
- Select military units and press `C` to cycle Aggressive, Defensive, Stand
  Ground, and Passive. Colored badge and selection HUD show current stance.
  Building-selected `C` still trains Archers.
- Right click a damaged completed friendly building with a villager: repair it,
  spending wood and stone in proportion to restored hit points.
- Right click friendly Town Center with selected foot units: garrison up to 15.
  Villagers and foot archers add defensive arrows. Select occupied Town Center
  and press `E` to ungarrison everyone.
- Left click blue town center, then `V`: train villager.
- Select a production building, then `Backspace`: cancel its last
  queued unit and refund that order's full paid cost without resetting the
  active unit.
- Forward Delete removes selected units or building. On compact Mac keyboards,
  Command-Backspace performs the same action. Plain Backspace remains
  production cancellation.
- Select blue villager, press `B`, then right click nearby: build barracks.
- Select several villagers before placing a building to assign all of them;
  right click an existing friendly foundation to add more builders.
- Select blue villager, press `A`, then right click nearby: build archery range.
- Select blue villager, press `D`, then right click nearby: build stable.
- Select blue villager, press `J`, then right click nearby: build blacksmith.
- Select blue villager, press `U`, then right click nearby: build Castle in
  Castle Age.
- Select blue villager, press `I`, then right click nearby: build University
  in Castle Age.
- With a completed Blacksmith, press `2` and right click nearby to build a
  Siege Workshop in Castle Age; select it and press `3` to train a Battering
  Ram.
- Press `4` and right click nearby to build a Palisade Wall segment.
- Press F9/F10 and right click nearby to build NW-SE/NE-SW Palisade Gates.
- In Feudal Age, press Home/End and right click nearby to build
  NW-SE/NE-SW Stone Gates.
- In Feudal Age, press `5` and right click nearby to build a Watch Tower.
- In Feudal Age, press `6` and right click nearby to build a Stone Wall
  segment.
- Select blue villager, press `H`, then right click nearby: build house.
- In Castle Age, press `1` and right click nearby to build another Town Center
  for 275 wood/100 stone.
- `F` builds mill, `T` builds lumber camp, and `G` builds mining camp.
- `Y` builds farm; select a Mill and press `E` to prepay one Farm reseed.
  The player-wide queue survives Mill loss and consumes one entry when a Farm
  exhausts.
- Select stable, then `Q`: train Scout Cavalry or `K`: train Knight in Castle
  Age.
- Select barracks, then `M`: train Militia or `Z`: train Spearman in Feudal
  Age.
- In Feudal Age, select Barracks and press `9` to research Man-at-Arms.
  Existing, queued, and future Militia become Men-at-Arms.
- Select archery range, then `C`: train archer.
- Select archery range, then `7`: train Skirmisher in Feudal Age.
- In Castle Age, select Archery Range and press `=` to research Crossbowman.
- In Castle Age, select Barracks and press `-` to research Pikeman.
- In Castle Age after Man-at-Arms, select Barracks and press `[` to research
  Long Swordsman.
- In Imperial Age after Long Swordsman, press `Shift+[` to research
  Two-Handed Swordsman.
- In Imperial Age after Two-Handed Swordsman, press `Option+Shift+[` to
  research Champion.
- In Imperial Age after Crossbowman, press `Shift+=` to research Arbalester.
- In Castle Age, select Archery Range and press `Option+7` to research
  Elite Skirmisher.
- Select Siege Workshop, then `8`: train Mangonel in Castle Age.
- HUD population display shows current units and completed housing capacity.
- Select completed town center and press `N` to research next Age.
- In Dark Age or later, `]` researches Loom at a town center.
- In Feudal Age or later, `\` researches Double-Bit Axe at a lumber camp.
- In Feudal Age or later, `;` researches Horse Collar at a mill.
- In Castle Age or later, `'` researches Fortified Wall at a university.
- In Castle Age or later, `/` researches Guard Tower at a university.
- In Imperial Age, `` ` `` researches Keep at a university after Guard Tower.
- In Castle Age or later, `F1` researches Bodkin Arrow at a blacksmith after
  Fletching.
- In Imperial Age, `F2` researches Bracer at a blacksmith after Bodkin Arrow.
- In Castle Age or later, `F3` researches Iron Casting at a blacksmith after
  Forging.
- In Imperial Age, `F4` researches Blast Furnace after Iron Casting.
- In Feudal Age or later, `F12` researches Scale Mail Armor at a blacksmith.
- In Castle Age or later, `Shift+F12` researches Chain Mail Armor after Scale
  Mail Armor.
- In Imperial Age, `Ctrl+F12` (or `Cmd+F12`) researches Plate Mail Armor after
  Chain Mail Armor.
- In Feudal Age or later, `Option+F1` researches Scale Barding Armor at a
  blacksmith.
- In Castle Age or later, `Option+F2` researches Chain Barding Armor after
  Scale Barding Armor.
- In Imperial Age, `Option+F3` researches Plate Barding Armor after Chain
  Barding Armor.
- In Feudal Age or later, `Option+F4` researches Padded Archer Armor at a
  blacksmith.
- In Castle Age or later, `Option+F12` researches Leather Archer Armor after
  Padded Archer Armor.
- In Imperial Age, `Option+Shift+F12` researches Ring Archer Armor after
  Leather Archer Armor.
- In Feudal Age or later, `Option+B` researches Bloodlines at a stable.
- In Castle Age or later, `Option+Shift+B` researches Husbandry at a stable.
- In Castle Age or later, `Option+Q` researches Light Cavalry at a stable.
- In Imperial Age after Light Cavalry, `Option+Shift+Q` researches Hussar.
- In Imperial Age, `Option+K` researches Cavalier at a stable.
- In Imperial Age after Cavalier, `Option+Shift+K` researches Paladin.
- In Feudal Age or later, `W` researches Wheelbarrow at a town center, `X`
  researches Fletching at a blacksmith, and `O` researches Forging there.
- Select a completed University and press `0` to research Murder Holes.
- `S`: stop all selected units and clear their current orders.
- F5: save to `archaeology-save.txt`.
- `L`: load save.
- Space: pause/resume.
- `R`: restart match.
- F8: restart and replay current command history.
- F6: save replay to `archaeology-replay.txt`.
- F7: load and play saved replay.
- F11 or Alt+Enter: toggle desktop fullscreen. The saved choice is applied at
  startup. Leaving fullscreen restores the prior usable windowed geometry.
- Drag any window edge or corner to resize down to 640x360. Drawable-size and
  high-DPI scale changes update the world viewport, bottom HUD, camera bounds,
  edge scrolling, and pointer coordinate space; the game does not retain a
  fixed 16:9 canvas. UI sizing uses window-coordinate units, then SDL scales
  to Retina/high-DPI drawable pixels, so higher pixel density does not make
  controls and text smaller.

- Escape: quit.

These fullscreen and resize behaviors have automated coverage. Human desktop
verification of continuous drag, high-DPI pointer alignment, startup
fullscreen, failure rollback, and restored window geometry was accepted on
2026-07-31.

Save and replay files live in the per-user SDL preference directory
(`~/Library/Application Support/Software Archaeology/AoE Archaeology/` on
macOS), so the Finder app never depends on its launch working directory.
