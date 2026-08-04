# Main-menu text evidence

## Recovered contract

The supplied English `language.dll` RT_STRING resources resolve main-menu
label IDs exactly: 9500 `Single Player`, 9501 `Multiplayer`, 9503
`Learn to Play`, 9504 `Map Editor`, 9505 `History`, 9506 `Options`, and 9509
`Exit`. `FUN_00603970` assigns the corresponding help IDs 31000, 31001,
31003, 31004, 31005, 31006, and 31009.

`FUN_004f3b30` constructs font slot zero, `RGE_FONT_BUTTON1`, from localized
IDs 110/111/112. The supplied English RT_STRING resources resolve those as
`Lucida Blackletter`, `14`, and `N`. `FUN_004f3010` proves `N` selects normal
400 weight and no italic style. Original executable instructions at
`0x004f1237`-`0x004f1252` pass height 14 through `MulDiv(14,96,72)`, negate the
result, and store GDI `lfHeight = -19`; reconstruction therefore rasterizes at
19 logical pixels. The executable registers `lblack.ttf`, whose internal
family name is `Lucida Blackletter`.

`main.sin` supplies normal foreground `(217,208,176)`, secondary/shadow
`(0,0,0)`, focus `(202,207,1)`, and state/disabled `(255,255,255)`. Text-label
setup calls `FUN_005bee40(2,0)` after each label, proving centered horizontal
alignment. Archive inspection proves `main_32.slp` frames 10-48 contain blank
plaques and control art, not baked label pixels.

## Geometry

`FUN_006042a0` directly supplies six text rectangles:

- Multiplayer `(502,284,145,23)`
- Learn to Play `(150,13,188,40)`
- Map Editor `(420,355,107,18)`
- History `(304,213,128,32)`
- Options `(304,450,117,24)`
- Exit `(200,704,160,26)`

The decompiler omitted the first constructor operands, but original executable
instructions preserve them. At `0x00604a3e`-`0x00604a62`, four pushes before
the label object's virtual `+0x10` call supply height `38`, width `178`, y `20`,
and x `542`, proving the Single Player rectangle `(542,20,178,38)`. The
preceding virtual call independently supplies image bounds `(532,9,192,258)`.

## Reconstruction proof

Runtime resolves the exact supplied/user-installed face without adding a new
font asset. On macOS it first opens reconstruction-local packaged
`Data/fonts/LBLACK.TTF`, then accepts a system-installed font only when
CoreText reports the exact `Lucida Blackletter` family; otherwise the native
archive remains unlabelled rather than substituting a visibly false face.
Localized strings are loaded by numeric RT_STRING ID. Each state draws its
documented black secondary layer and normal, focus/pressed, or disabled color
inside the recovered centered rectangle.

`frontend_menu_tests` pins all recovered geometry and typography values.
`frontend_menu_sdl_smoke` makes deterministic archive-backed 1366x768 normal,
focused, pressed, and disabled captures and verifies exact foreground/shadow
colors inside the recovered rectangles. It also preserves 1024x768
letterboxing and alpha-mask activation coverage.
