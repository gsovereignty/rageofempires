# Bounded scenario editor

`ScenarioEditor` is a reconstruction-native model/controller for Scenario66.
It supports terrain and elevation paint, blue/red unit and building placement,
tile removal, resources, ages, civilizations, diplomacy, match rules,
objectives, strict trigger records, snapshot undo/redo, validation, and
load/save.

The controller rejects out-of-map painting and placement, elevations outside
`0..7`, duplicate objective/trigger IDs, empty objective text, and empty
trigger conditions/effects. Saving validates first and then uses the existing
Scenario66 serializer; loading uses the same strict parser.

`resources/editor-roundtrip.scenario` is the stable fixture. The
`aoe_scenario_editor_tests` target edits a scenario, exercises undo/redo,
saves, reloads, and checks map, elevation, placement, player setup, objective,
and trigger preservation.

No proprietary scenario-editor file or UI compatibility is claimed. The
supplied interface audit proves a HUD background and cursor only; it does not
prove editor palettes or panels. Any SDL editor surface must therefore reuse
only those proved assets and label other framing as reconstruction-native.
## SDL authoring surface

Main-menu option 4 opens localized editor chrome without environment-only
controls. Mouse and keyboard cursor paths expose terrain/elevation painting,
blue/red unit and building placement, removal, player selection, age,
civilization, economy, diplomacy and match-rule changes, objective and trigger
creation, validation, save/load, and undo/redo. Tab/Shift-Tab moves visible
panel focus; arrows move bounded tile cursor and Enter applies selected tool.
Escape returns to main menu.

`AOE_EDITOR_INPUT` accepts bounded native scenario fixture for deterministic
editor startup. Dedicated SDL smoke coverage loads
`resources/editor-roundtrip.scenario`; controller tests prove validation,
round-trip persistence, undo, and redo. Panel geometry, keyboard mapping, and
default generated objective/trigger text are reconstruction-native.
