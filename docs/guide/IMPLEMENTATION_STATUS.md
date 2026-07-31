## Reconstruction status

Current code is a functional vertical slice, not the complete commercial game.
All high-impact items from the current DAT fidelity audit have graduated into
the represented roster. Exact source evidence remains recorded in
Backlog snapshots live outside product repository and must be revalidated
against current code before implementation.

## Recent issue closure: human verified

Six reported issues are addressed in code, covered by automated checks, and
accepted after human interactive verification on 2026-07-31:

| Addressed issue | Implemented behavior | Automated evidence | Human verification result |
|---|---|---|---|
| Building graphics ignored current age | Completed buildings choose graphics from selected/current age without leaking later-age layers. | Age-selection unit coverage and `building_age_graphics_sdl_smoke`. | Approved across representative Ages with original assets. |
| Initial gameplay camera missed starting Town Center | Startup centers first on local Town Center, then deterministic unit/map fallbacks. | Initial-camera unit tests and startup smoke coverage. | Approved in normal and unusual scenarios. |
| Main menu did not reproduce original entry flow | Packaged startup opens classic 800x600 menu flow, measured flyout, setup routes, and explicit fallback when original art is unavailable. | Frontend-menu unit and SDL smoke coverage. | Approved for real-desktop mouse/keyboard flow and reference comparison. |
| Resource top bar overflowed or overlapped | Responsive five-field row bounds text and icons at supported widths and drawable scales. | HUD layout contract tests and `aoe_hud_layout_sdl_smoke`. | Approved across display, value, and art variants. |
| Terrain boundaries rendered as hard seams | Transition masks now blend neighboring terrain with zoom-aware sampling and bounded fallbacks. | Terrain-transition unit tests and `terrain_edge_sdl_smoke`. | Approved across varied terrain and zoom views. |
| Fullscreen and resize state was inconsistent | Saved fullscreen applies at startup; `F11` and `Alt+Enter` share synchronized state; resize updates drawable, HUD, camera, and input extents; windowed geometry restores after fullscreen. UI extent now remains in window-coordinate units so Retina pixels do not shrink content. | `window_mode_tests`, including 2x-density policy; `window_mode_sdl_smoke`; settings tests; HUD smoke coverage. | Approved for real-desktop resize, fullscreen, Options, input, and high-DPI behavior. |

Automated checks prove defined policies and deterministic smoke paths only.
They do not replace subjective visual review, real window-manager behavior,
real-display DPI behavior, or end-to-end human input acceptance.

The current reconstruction-native Scenario v66 format carries the bounded
ordered typed trigger vectors introduced by v64, with deterministic priority
ordering, snapshot-before-effects evaluation,
loop-relative timers, player-scoped expiring messages, objectives, terminal
match results, and Save v109 persistence. Replay v63 reproduces those internal
firings from the same scenario and command stream. The reconstruction-native
`aoe-campaign 1` manifest provides ordered local `.scenario` missions, a
content-bound digest, linear victory-only unlocking, stale-progress detection,
and atomically replaced `aoe-campaign-progress 1` state. These formats do not
decode proprietary scenario or campaign containers.

Launching with `AOE_CAMPAIGN=/path/to/manifest.campaign` now opens a bounded
mission briefing before simulation starts. It shows campaign/scenario title,
mission order, human civilization, map dimensions, description, and visible
objectives. `Enter` or left click begins; `Escape` goes back. Victory or defeat
opens a debrief after progress is atomically committed, showing objective
results, next unlock/retry state, and continue/back controls. `F5` retains the
compact in-game campaign status panel. `resources/briefing-demo.campaign`
provides a two-mission visual audit, and `/tmp/aoe-campaign-briefing.png` is
the current 1280x720 briefing capture.

No supplied interface mapping identifies a campaign background, button, or
portrait: the proved archive mappings cover the HUD background and cursor,
while action-sheet and portrait-frame mappings remain inferred. Briefing and
debrief therefore use reconstruction-native beveled panels rather than
mislabeling unrelated art. `AOE_CAMPAIGN_NARRATION_PATH` and
`AOE_CAMPAIGN_CINEMATIC_PATH` may identify optional user-owned media and are
shown only as configured metadata; no commercial media is bundled, decoded,
or claimed compatible.

`AOE_EDITOR=1` opens the bounded reconstruction Scenario66 editor and freezes
gameplay simulation while preserving the normal isometric preview. Its panel
offers `1/2/3` Grass/Water/Forest paint, `E` elevation raise, `U` blue
Villager, `B` blue House, `X` erase, `Ctrl+Z`/`Ctrl+Y` undo/redo, left-click
application, and `Ctrl+S` validated save. `AOE_EDITOR_PATH` selects the output;
the default is the application user-data directory. The separate
`ScenarioEditor` model/controller additionally exposes resources, ages,
civilizations, diplomacy, objectives, strict triggers, and match rules for
future property panels. Validation compiles strict triggers through normal
simulation creation before Scenario66 serialization.

`resources/editor-roundtrip.scenario` exercises painted terrain/elevation,
players, placements, objectives, triggers, and match settings.
`aoe_scenario_editor_tests` proves edits, rejection, undo/redo, save/load, and
round-trip preservation. `/tmp/aoe-scenario-editor.png` is the current UI
capture. No proprietary editor format, layout, or compatibility is claimed;
the panel is reconstruction-native because the supplied archive proves no
editor-specific panel art.

`AOE_MAIN_MENU=1` opens the fixed-800×600 classic main screen before simulation
advances. Packaged `main_32.slp` frame 0 supplies optional original Age of
Kings background art; missing or malformed art uses an explicitly
non-equivalent procedural fallback. Keyboard and mouse choices cover Single Player setup, the loaded
Scenario66, configured campaign briefing, Scenario66 editor, and
preconfigured localhost Host/Join. The Single Player setup confirms player,
scenario civilization, computer difficulty source, and loaded map/rules
before `Enter` starts; `Escape` returns. Campaign and multiplayer choices
report the required `AOE_CAMPAIGN` or `AOE_MULTIPLAYER` configuration instead
of inventing Internet discovery. Single Player opens a measured flyout with
seven ordered entries, contextual help, wrapping keyboard focus, pointer
focus, and shared activation routing. See
[`MAIN_MENU_FIDELITY.md`](../fidelity/MAIN_MENU_FIDELITY.md) for asset and
layout evidence. Flyout framing, English text, and debug-glyph typography are
reconstruction-native and do not claim pixel parity.

Single Player setup drives the deterministic reconstruction random-map
generator. `M` cycles Arabia/Black Forest/Islands/Rivers, `Z` cycles
Tiny/Small/Medium/Large, `-`/`+` changes the displayed seed, `C` cycles
civilizations, `D` cycles computer difficulty, and `V` cycles
Conquest/Wonder/Relic victory. Every map change regenerates and validates,
shows a deterministic hash, and updates the procedural minimap preview.
`Enter` applies the selected civilization and rules and starts that exact
generated Scenario66; validation failures remain visible with their reason.

`AOE_RANDOM_MAP_SETUP=1` and `AOE_RANDOM_MAP_SEED=N` support scripted audits.
`aoe_random_map_sdl_smoke` renders seed `424242` twice and requires
byte-identical screenshots. `/tmp/aoe-random-map-setup.png` is the current
capture. The archive-backed HUD remains available in matches; setup framing
and preview are reconstruction-native because no supplied archive mapping
proves commercial random-map setup panels.

`F9` opens a read-only civilization technology tree from setup or a running
match without changing simulation. The model lays out all 94 reconstructed
unit kinds, 27 building kinds, and 156 technology slots in Dark/Feudal/Castle/
Imperial columns. It derives available/disabled state from the generated
18-civilization availability matrices, marks already researched technologies,
and draws factual trained-at/researched-at dependency edges. Hovering shows
wood/food/gold/stone costs and age/building requirements. `Q`/`E` changes
civilization, arrows or WASD pan, `+`/`-` zoom, the mouse wheel zooms, and
right/middle drag pans; `F9` or `Escape` closes.

`AOE_TECH_TREE=1` supports scripted capture.
`aoe_technology_tree_tests` verifies complete node/dependency counts,
availability differences, researched state, and bounded layout.
`/tmp/aoe-technology-tree.png` is the current capture. Nodes use explicit
procedural colored cards because the live archive audit proves no complete
technology-icon mapping; existing archive-backed HUD/panel assets remain
unchanged underneath.

`F10` opens the read-only diplomacy/tribute surface over single-player or
multiplayer. It shows blue/red colors, civilizations and negotiated teams,
current Ally/Neutral/Enemy stance, allied-victory and Cartography shared-
vision state, current market buy/sell rates, selected tribute resource/amount,
and the Coinage/Banking-adjusted fee. `A`/`N`/`E` queues diplomacy,
`1`–`4` selects Food/Wood/Gold/Stone, `-`/`+` changes tribute in 100-resource
steps, and `Enter` queues tribute. Tribute is visibly disabled unless the
target is allied and the sender has enough resources. `T` opens All chat and
`Y` opens Allies chat in multiplayer; single-player reports chat unavailable.

Every state-changing action uses normal `SetDiplomacyCommand` or
`TributeResourceCommand`, so localhost lockstep, replay recording, ownership
checks, and single-player execution share one path. Existing core tests cover
fee tiers, replay round-trip, diplomacy, shared victory, and deterministic
execution; `aoe_diplomacy_panel_sdl_smoke` covers rendering.
`AOE_DIPLOMACY_PANEL=1` supports scripted capture, and
`/tmp/aoe-diplomacy-panel.png` is the current image. The panel is procedural
and labeled because no supplied archive mapping proves original diplomacy art.

The reconstruction also includes a transport-independent protocol-3 lockstep
core for two to eight occupied stable slots, plus a configured localhost TCP
star-relay harness and legacy two-peer nonblocking frame-loop runtime. Harness
binds accepted streams to configured stable slots and relays framed bundles
among one host and up to seven peers. Core performs occupied/active
slot ownership checks, explicit empty turns, ascending-slot canonical command
ordering, sourced Replay command framing, periodic native-Save-derived state
checksums, timeout/disconnect states, queued local input, and an immutable
canonical handshake covering build/schema/save/scenario/content identifiers,
cadence, input delay, seed, controllers, teams, cooperative-control flags,
civilizations, directed diplomacy, and explicit host slot. Exact configuration
echo is required before every occupied slot becomes ready, with categorized
mismatch states. They also provide
partial/coalesced TCP-frame handling and a separate host-sequenced bounded
UTF-8 chat stream for all/allies messages that never enters simulation hashes
or replays. A coordinated committed-tick save barrier permits a durable atomic
native Save plus checkpoint envelope v3 only after every occupied participant
reports the same state hash; loading verifies exact save, configuration,
roster, tick/hash, and ascending slot/sequence digests for a new lobby.
The interactive SDL flow still connects only host and one joiner. A separate
headless application target drives the same `Simulation`, protocol-3 session,
and localhost star relay through three real processes. It does not provide live
reconnect or commercial-save compatibility. Negotiated input delay schedules
commands at deterministic execution ticks with startup empty-bundle priming,
while monotonic ping/pong supplies RTT, peer-traffic age, waiting state, and
latency bands without influencing simulation outcomes. Synchronized
pause/resume and reconstruction-native slow/normal/fast speed
changes use host proposal, peer acknowledgement, and committed-tick control
barriers; pause and speed persist in multiplayer checkpoint envelope v3.
Five seconds without peer traffic exposes a waiting stall; thirty seconds
suspends at the last committed tick. Transport loss alone remains suspended,
not terminal. Only an explicit host drop or peer disconnect terminates, and no
AI takeover, live reconnect, ownership transfer, or host migration occurs.
The SDL application exposes an environment-driven localhost developer
integration, not a finished
user-facing network lobby. The protocol does not claim compatibility with the
commercial game.

The SDL localhost integration has a headless two-process smoke proof: host and
join execute a scripted lockstep command and ordered all/allies chat, write
their committed tick, full state checksum, and visible chat independently,
prove that the opponent cannot see an allies-only message, compare equal
simulation state, capture both screens, and exit cleanly.
Each smoke run asks the kernel for an ephemeral `127.0.0.1` port, retries host
startup up to three times for the rare close-to-bind race, uses an isolated
temporary artifact tree, and prints port/process/log/state diagnostics on an
eight-second attempt timeout. A failed full session is retried up to three
times with a fresh port and state tree. A ten-run soak is expected to pass
without shared port collisions.

The additive three-peer proof is
`aoe_multiplayer_roster_headless`. It leaves the legacy interactive
`AOE_MULTIPLAYER=host|join` behavior unchanged and requires an explicit
configured manifest:

```text
AOE_MULTIPLAYER=host|join
AOE_MULTIPLAYER_PORT=<localhost port>
AOE_MULTIPLAYER_ROSTER=0,1,2
AOE_MULTIPLAYER_LOCAL_SLOT=0|1|2
AOE_MULTIPLAYER_STATE_PATH=<proof path>
AOE_MULTIPLAYER_SCREENSHOT_PATH=<PPM path>
```

Host slot is 0. The host publishes `AOE_MULTIPLAYER_READY_PATH`; clients may
publish their per-process `AOE_MULTIPLAYER_CONNECTED_PATH`. These explicit
files let the smoke bind clients to stable slots without timing sleeps or
lobby/discovery invention. The smoke verifies canonical handshake, all-slot
ready/start quorum, three deterministic turns, one all-peer map signal, equal
state hashes, byte-equal deterministic screenshots, timeout diagnostics, and
clean exit. Run repeatability with:

```sh
ctest --test-dir build -R aoe_localhost_multiplayer_roster_headless_smoke \
  --repeat until-fail:10 --output-on-failure
```

In either the lobby or running game, `Enter` opens chat, `Tab` switches between
All and Allies, `Enter` sends, and `Escape` cancels. Chat input consumes
gameplay hotkeys only while the editor is active; it does not pause simulation.
The UI reports empty, invalid UTF-8, disconnected, and 4096-byte-limit
rejections. Hosts use `Ctrl+Enter` to start a ready lobby. For automated
audits, `AOE_MULTIPLAYER_SCRIPT_CHAT` accepts pipe-separated `all:` and
`allies:` messages; the multiplayer state proof includes ordered `chat` lines.
This remains a developer verification path, not a finished Internet lobby.

The lobby and running-game overlays also show the negotiated input delay,
measured heartbeat RTT and green/yellow/red band, peer-traffic age, and the
five-second waiting indication. These values are presentation-only and never
advance simulation. `F6` lets the host request a coordinated save barrier two
ticks ahead. Both peers pause at the committed tick and compare state hashes;
the overlay reports collecting, matched, or mismatch. On a match the host
atomically writes the current versioned save and its strict multiplayer
envelope, immediately
reloads both for verification, and displays the save path. Automated runs can
set `AOE_MULTIPLAYER_INPUT_DELAY`, `AOE_MULTIPLAYER_SCRIPT_CHECKPOINT`, and
`AOE_MULTIPLAYER_CHECKPOINT_PATH`. The two-process smoke negotiates a
three-tick delay, proves a matched tick-12 barrier, and verifies both durable
checkpoint files.

Host-only `F7` proposes pause/resume and `F8` cycles Normal, Fast, and Slow.
The peer acknowledges proposals before they commit at the shared barrier tick.
Both overlays show proposal/ack feedback, committed pause state, speed, and
effective cadence; a centered pause banner appears while paused. Chat and
network pumping remain active, while gameplay commands are rejected until
resume. `AOE_MULTIPLAYER_SCRIPT_CONTROL=1` makes the two-process smoke commit
Fast, commit Pause, prove the committed tick remains frozen across 24 rendered
frames, commit Resume, and then complete the matched checkpoint. Both peers
must report running, Fast, 100ms cadence, and an unchanged equal state hash.
These speed values and control policy are reconstruction-native.

Remaining full-game work also includes complete original resource-format
coverage, the rest of the construction and technology trees, proprietary
campaign/scenario import and cinematics, production multiplayer UI/lobby,
complete reactive sound,
UI panels, and broader behavioral validation against legally obtained builds. Existing
computer-player behavior is substantial but does not reproduce the complete
commercial AI.
