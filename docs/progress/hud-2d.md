# The HUD 2D lane (a Rust 2D draw list; the HUD's, the screens' and the toolkit's draws on it)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The HUD 2D lane, round 1").

**Scope.** The UI thread lane (docs/progress/ui-thread.md) found the HUD and screen host code blocked on C++ drawing
objects. This lane builds the missing piece, a Rust 2D draw list (`ee_render::ge::list2d`), and moves the draws of
the HUD controls, the screens and the toolkit onto it.

## The measure (round 1, at origin/rust 48c8472f0)

Every 2D draw goes through one of `GERasterizer`'s virtual draws, whose DX7 bodies were already Rust
(`ee_render dx7/draw2d.rs`): `Draw(GEBitmap*)`, `Draw(GERhombus*)`, `Draw(GELine*, n)`, `Draw(GEPoint*, n)`,
`Draw(GEText*, rect, surface)` (GDI or the font bitmap) and `Draw(GEText*, surface)` (the glyph atlases). The GE
objects themselves were whole Rust objects behind one-pointer C++ shells (`GERhombus`, `GELine`, `GEPoint`, `GEText`)
or a reference-counted shell (`GEBitmap`, `GEFontBitmap`). What stood between the HUD's Rust and those Rust draws was
host code: one C++ case per draw (`GetRasterizer()->Draw(object, GetScreenIndex())`), the GE objects the C++ classes
owned, and the temporaries the C++ built on its stack.

The call sites, by caller (round 1, before the move):

| Caller | Draws | Path |
|---|---|---|
| Toolkit (ee_ui whole objects) | button, media, slider, variant bitmaps; combo, edit, list rhombi; edit, graph, list lines; the form's bitmap, tool-tip shadow rhombus and every clipped text; the backgrounds' bitmaps | one `EeUiOp` per draw (`BitmapDraw`, `RhombusDraw`, `LineDraw`, `FormBitmapDraw`, `FormRhombusDraw`, `FormTextDraw`, `BgBitmapDraw`) to a C++ host case |
| Toolkit (C++) | `UIControl::DrawPolygons` (the copied background rhombus), `DrawBorder` (one GELine per border line), `UIStatusBar`'s two member rhombi | C++ |
| HUD (ee_game screens) | portrait (bitmap, 3 bars), ping (5 bars), icon bar (icon and frame bitmaps, black/red/green rhombi, red lines), status button (icon, dither), saved group (portrait), mini map (3 bitmaps, 4 lines under a viewport clip), select units (8 lines), command units (a model), error form buttons (temporary rhombi), generic background (temporary rhombus, frame pieces), texts of the portrait, chat line, attribute, status bar and button | the class's `RustOp` or C++ draw methods |
| Screens | scenario editor (tech list box rhombi; terrain list rhombi, bitmap, 4 lines), civilization builder list (text) | C++ |
| World view (graphics thread) | unit bars and lines, unit names, the effects' polygons, a line and a text of the render object | C++ (`EEROUnits.cpp`, `EERenderObject.cpp`) |
| GE | the font bitmap's glyphs (one bitmap per character) and its glyph rendering (texts onto the atlases) | `GEFontBitmap::Host` cases |

The 2D-draw digest (`EE_DRAW2D_DIGEST`, below) of the fixture tour with `--view-check --seed 7` on the unmodified
C++: 1,522 distinct commands (652 bitmaps, 553 lines, 107 rhombi, 210 texts) over 722 shown frames.

## What is in Rust (round 1)

- `ee_render::dx7::digest2d`: the 2D-draw digest. `EE_DRAW2D_DIGEST=<file>`: every 2D draw at the DX7 rasterizer's
  Rust entries with what it sends (the four vertices after the draw, the texture's frame, a text's string, place,
  colours, font, clip and bounds), `C <hash> <command>` the first time, `F <frame> <count> <hash>` per shown frame.
- `ee_render::ge::list2d`: the draw list. `Cmd` (bitmap, rhombus, lines, points, clipped text, text) on a `Target`
  (rasterizer, screen), `DrawList` (record, flush), `draw` (one command). On a DX7 rasterizer (registered by its
  constructor, `ee_ge_draw2d_direct`) the bitmap, rhombus, line and point draws are the `ee_dx7r_draw_*` bodies
  `DX7Rasterizer::Draw` forwards to; texts and other rasterizers (the store gate's) go through one C++ executor
  (`GERasterizerDraw2D`, a virtual draw per command). `Shape`: a rhombus or line made and owned in Rust with the C++
  layout. The GE objects' members by C++ pointer (`prim_set_color`, `rhombus_set_rectangle`, `bitmap_set_location`,
  ...). `ee_ge_draw2d_fill` for the C++'s temporary rhombi.
- `ee_ui::draw2d`: the toolkit's draws and GE object members on the list in the game (the replays keep their ops:
  no executor is registered in a cargo test); `UIControl::DrawBorder` whole.
- `ee_game screens/hud_draw.rs`: the HUD controls' draw objects per control (the ping's 5 bars, the portrait's 3
  bars and its bitmap) and the icon bar's temporaries, below the hosts' traits.
- `ee_render ge/fbobj.rs` `direct_op`: the font bitmap's working text, drawing bitmap and both draws.

## Commits (round 1)

| Commit | What | Net C++ |
|---|---|---|
| 46f5f9c6e | the 2D-draw digest | 0 |
| 08b0bf14d | the draw list, its executor and DX7 direct path; the toolkit's draw ops; `DrawBorder` | +1 |
| dd52fbfe1 | the HUD's draws (ping, portrait, icon bar, error form, generic background) | -55 |
| 6a7f6d89b | the font bitmap's text, bitmap and draw ops | -54 |
| e5901fab2 | rustfmt | 0 |
| 7f2d322df | the toolkit's GE object members | -49 |
| 7cdf80b3d | `GEBitmap`'s shell inline | -92 |

**Net C++ -249** (target 3,000: see PORT_LEARNINGS "The HUD 2D lane, round 1", "The measure").

## Checks (round 1)

The digest and pixel comparisons against the pre-round test-profile exe (`build/hud2d/pre`), the store gate, the
SGVW goldens, the editor and civilization gates, the harness sessions and gates.sh: PORT_LEARNINGS "The HUD 2D lane,
round 1".

## Left

- The toolkit's remaining GE host cases: the primitives' create and delete (`RhombusCreate`, `LineCreate`, `new
  GERhombus` in the C++ constructors), `UIStatusBar`'s member rhombi, the texts' members (`TextCopy`, `TextSetText`,
  the colours), the bitmaps' textures.
- The HUD: the status button's, saved group's and mini map's bitmaps (the C++ owns them for `SetFaceBitmap` and the
  texture manager), the select units' 8 lines, the clipped texts of the C++ `DrawTextA`s.
- The screens' editor list boxes (`EESEUITechListBox`, `EESEUITerrainTypeLB`) and the world view's 2D draws.
- `GEBitmap`'s C++ shell (its vtable is `UReferenceCounter`'s; `SetFaceBitmap` and the texture manager take it).
