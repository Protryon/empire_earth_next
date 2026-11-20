# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5").

**Scope (round 5).** `src/Empire Earth/User Interface Thread/*` but the editor-only controls (`EESE*`,
`EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes (`EEUICWonNet*`,
`EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON events
(`EEUIEWonNet*`); the hosts left in `src/Low-Level Engine/User Interface`; their `include/ee` and `include/lle`
headers; the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game `screens/shells*.rs`) and `ee_ui`/`ee_game` screens.
The scenario editor (`Scenario Editor`, `Scenario Editor Actions`, the editor's controls, forms and factories) is a
lane of its own; it builds on the shells.

## The generic shells

A game class that only overrode a toolkit class's virtuals can be deleted whole:

- `ee/EEUIShell.h`: one C++ shell per toolkit base: `EEUIShellButton`, `EEUIShellRadio`, `EEUIShellCheck`,
  `EEUIShellCombo`, `EEUIShellList` (`UILBString`), `EEUIShellEdit`, `EEUIShellSlider`, `EEUIShellControl` (a
  plain `UIControl`; the pure virtuals `EEUIRustControl`'s defaults) and `EEUIShellForm` (a `UIForm`).
- A shell holds a Rust kind (`EE_SCREENS_SK_*`); each virtual asks the kind when its mask has the bit
  (`EE_SCREENS_SV_*`), else runs the base's. The kind calls the base back (`EE_SCREENS_SH_BASE + v`; a form's
  `EE_SCREENS_SH_FORM_BASE + v`), its own virtuals (`EE_SCREENS_SH_SELF`, a form's `EE_SCREENS_SH_FORM_SELF`), and
  the game's C++ through one host, `ee_screenscpp_shell` (`User Interface Thread/EEUIShell.cpp`).
- A C++ caller of a deleted class's method calls `EEUIShell::Op(control or form, EE_SCREENS_SKOP_*)`, or an inline
  helper beside the class's factory (`EEUIFileComboEntries`, `EEUIFileComboSelect`).
- The editor and civilization gates print a shell's `KindName()` (the deleted class's name).
- Numbers taken: kinds 1..0x54 (round 3), 0x60..0x61 (paint areas), 0x68..0x6a (message box), 0x80 (saved
  group), 0x81 (icon bar), **0x82 (the file combo box, round 5)**; kind ops alike (0x82..0x83 the file combo
  box's); host ops 0x80..0x96, 0xb0..0xbd, **0xc0..0xc2 (round 5)**; SV 0..33; `SH_FORM_SELF` 0xff, `SH_FORM_BASE`
  0x200. The scenario editor lane's: SK, SKOP 0x70..0x7f, host ops 0xa0..0xaf. Free for this lane: kinds and kind
  ops 0x83..0x9f, host ops 0xc3..0xcf.

Kinds (deleted classes): round 3's 22 (`EEUIBColor` .. `EEUIFSEBRemoveCargo`), round 4's eight (the paint areas,
the message box and its buttons, `EEUIBSavedGroup`, `EEUICUnitIconBar`), round 5's `EEUIFileComboBox`
(shells_files.rs).

## Round 5 (branch `rust-uicluster5`, 2026-11-19)

| Commit | What | Net C++ (src, include) |
|---|---|---|
| 98c450b86 | `UIGetStringCutOffIndex` in Rust (ee_ui cutoff.rs; ee_render smallobj.rs text_measure); the chat line's cut answered in Rust; `EEUICChatLine::RustOp` and `Display` deleted | -73 |
| a1169cb3f | `UIEMouseWheelScroll` inline; its unit deleted | -48 |
| 1bfca8840 | `EEUIFileComboBox` as a kind (shells_files.rs); its unit and header deleted | -66 |
| d5bba71ae | the main form's view rectangle and chat log count in Rust (hud_direct.rs) | -7 |
| 20f073a19 | `EEUIManager`'s five message box members inline; ee/EEUIFError.h folded into ee/EEUIManager.h | -32 |
| 9c3496610 | `EEUICPing`'s constructor inline; its unit deleted | -16 |
| 419265407 | the HUD controls' view rectangle in Rust; the chat log case out of the cargo test build | -9 |

Round total **-251** net C++ lines (+121 -372). Four units and two headers deleted (User Interface.cpp kept its
init). The gates and what only the tours exercise: PORT_LEARNINGS.md "The UI cluster lane, round 5".

## What is left (the lane's part), and why

- **The census** (tools/cpp_deps.py, run here with rust-cuts2's `cuts` copy; this branch has cycle cuts round 1, not
  round 2): every game UI unit is in SCC 1 (236 units, 114,201 lines on this branch); no User Interface Thread or
  UI Events unit is a leaf or a near-leaf. The LLE UI's 22 units are a cycle of their own; UIScrollBar,
  UIStatusBar, UISlider, UIGraph, UIBackgroundTable, UIFontTable and UIHotKey are near-leaves inside it (their
  targets UIControl, UIButton, UIManager, UIForm).
- **What the C++ is.** Nearly every line left is host code (one case per call on a C++ object: the world, the
  picker, communications, GE bitmaps, sounds, files, the toolkit's virtuals) or a C++ class whose members other
  lanes read in place. A case moves when its target is Rust (the world views, the toolkit's direct path, ee_gfx
  input_views, the chat log); this round took the ones that were.
- **`EEUIRustControl`'s subclasses** (`EEUICChatLine`, `EEUICSelectUnits`, `EEUICCommandUnits`, `EEUICPortrait`,
  `EEUIMiniMap`): their logic is Rust over records found by an offset; each owns C++ sub-objects. As shell kinds
  they save only the class boilerplate (the sub-objects move into a factory, the record into the kind, which the
  Rust then finds through the shells' map). The chat line is the cheapest (main game only, about -40); the others
  have editor users and other lanes' readers.
- **The improvement buttons** (`EEUIBUpgrade`, `EEUIFMGAttribute`, `EEUIFMGStatusBar`): no shell base
  (`EEUIBUpgrade` is a `UIButton` with two new virtuals); the status box reads the status bar's parts.
- **`EEUISCIFormManager`** (UI Events/EEUIEScenarioIntroEvents.cpp, 205 lines): decomp-style C++ over FSFileSpec and
  `EEOpenSaveGameFile`; its flags and media strings are read in place by EEScreens.cpp (ten cases), EESMEffect.cpp,
  the WON form manager and the main form.
- **`EEUIBuildWall`, `EEUIBuildGate`**: world calls (the tech tree, the unit class media, the ghost units) and
  the editor's subclasses (`EESEUIBuildWall`, `EESEUIBuildGate`).
- **The LLE UI hosts** (UISlider, UIStatusBar, UIScrollBar, UIButton, ...): a forwarder per member and a host
  switch whose cases are the control's own C++ virtuals or GE objects (rhombi, bitmaps); they go when the
  toolkit's virtual dispatch and the GE objects they draw are Rust's.
- `EEScreens.cpp`, `EEUIFMainGame.cpp`, `EEUserInterface.cpp`, `EEUIManager.cpp`, `EEUIFMGStatusButton.cpp`: host
  switches (the UI thread lane's measure, docs/progress/ui-thread.md). The status button's host has a few reads of
  C++ globals (`gFortressUnloadCost`, `gGameResources`, `gWorldTable`) that a Rust view of them would answer.

## Checks

- Per piece: the build `--sim` 0/0; the editor gate (`tools/sim_editor.py`) for the file combo box.
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, sim_editor, the civilization gate, scenario_check, the tours (the random game with box select and
  orders, the editor exercise, the tour with its settings tabs, `--input-check --view-check`), `bash tools/gates.sh`.
- Captures: `build/ui5/tools/bmp2png.py` (a scratch converter, not committed) makes the harness's BMPs viewable.
