# Minimap mode controls

Bottom-right minimap exposes `NORMAL`, `COMBAT`, `ECONOMIC`, and `STATISTICS`
buttons at every supported resolution. Selected mode uses pressed/highlighted
art. Hovering any button writes its help in lower-left information area.

Clicking mode button changes and immediately persists local mode. Statistics
button toggles compact mode-dependent information for all populated player
slots. Existing `F12` detailed match-statistics overlay remains available.

Hotkeys:

- `Alt+N`: Normal
- `Alt+C`: Combat
- `Alt+E`: Economic
- `Alt+M`: cycle mode

Buttons derive from same resolution-anchored minimap frame as render and click
inversion. Map clicks, signals, and viewport rendering retain shared recovered
projection at every resolution.
