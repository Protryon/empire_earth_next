# The UI cluster lane (the main game HUD, the menus, the UI events, the LLE UI's hosts)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 5", then "The UI lane, round 6").

**Scope (round 6, the lane scopes of 2026-11-19).** `src/Empire Earth/User Interface Thread/*` but the editor-only
controls (`EESE*`, `EEUIFSE*`), `EEFOFindNewTargetNearOldTarget.cpp` (the units' lane) and the WON boxes
(`EEUICWonNet*`, `EEWonNetAvailableGameFormManager.cpp`: the WON lanes'); `src/Empire Earth/UI Events` but the WON
events (`EEUIEWonNet*`); `src/Low-Level Engine/User Interface`; `include/lle/UI*`; their `include/ee` headers
(`EEUI*`, `EEScreens*`, `EEUserInterface*`); the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and the `ee_ui`/`ee_game` screens.

## The generic shells

A game class that only overrode a toolkit class's virtuals can be deleted whole:

- `ee/EEUIShell.h`: one C++ shell per toolkit base: `EEUIShellButton`, `EEUIShellRadio`, `EEUIShellCheck`,
  `EEUIShellCombo`, `EEUIShellList` (`UILBString`), `EEUIShellEdit`, `EEUIShellSlider`, `EEUIShellControl` (a
  plain `UIControl` over `EEUIRustControl`; four slots for the C++ objects a kind's host cases need) and
  `EEUIShellForm` (a `UIForm`).
- A shell holds a Rust kind (`EE_SCREENS_SK_*`); each virtual asks the kind when its mask has the bit
  (`EE_SCREENS_SV_*`), else runs the base's. The kind calls the base back (`EE_SCREENS_SH_BASE + v`; a form's
  `EE_SCREENS_SH_FORM_BASE + v`), its own virtuals (`EE_SCREENS_SH_SELF`, a form's `EE_SCREENS_SH_FORM_SELF`), and
  the game's C++ through one host, `ee_screenscpp_shell` (`User Interface Thread/EEUIShell.cpp`).
- A C++ caller of a deleted class's method calls `EEUIShell::Op(control or form, EE_SCREENS_SKOP_*)`, or an inline
  helper beside the class's factory; a Rust caller holding the control calls `shells_ffi::op_of(ctl, op, a, b)`.
- An `EEUIShellControl` kind's HUD ops (`EE_SCREENS_HC_*`) go through `EEUIRustControl::HostOp` as before (the
  shell is an `EEUIRustControl`); its class ops reach `EEUIShellControl::RustOp`.
- The editor and civilization gates print a shell's `KindName()` (the deleted class's name).
- Numbers taken: kinds 1..0x54 (round 3), 0x60..0x61 (paint areas), 0x68..0x6a (message box), 0x80 (saved
  group), 0x81 (icon bar), 0x82 (the file combo box, round 5), **0x83 (the chat line, round 6)**; kind ops alike
  (0x82..0x83 the file combo box's, **0x84..0x85 the chat line's**); host ops 0x80..0x96, 0xb0..0xbd, 0xc0..0xc2
  (round 5), **0xc3 (round 6)**; SV 0..33; `SH_FORM_SELF` 0xff, `SH_FORM_BASE` 0x200. The scenario editor lane's:
  SK, SKOP 0x70..0x7f, host ops 0xa0..0xaf. Free for this lane: kinds and kind ops 0x86..0x9f, host ops 0xc4..0xcf.

Kinds (deleted classes): round 3's 22 (`EEUIBColor` .. `EEUIFSEBRemoveCargo`), round 4's eight (the paint areas,
the message box and its buttons, `EEUIBSavedGroup`, `EEUICUnitIconBar`), round 5's `EEUIFileComboBox`
(shells_files.rs), round 6's `EEUICChatLine` (shells_chat.rs).

## The direct path and the world views

Two ways a C++ host case goes without a new C++ line:

- the toolkit's direct path (`ee_ui::direct`): a plain call on a toolkit object (a label's alignment, an edit box's
  password flag, a form's `AddControl`, a group's `Enable`, a control's `SetDisplayMode` through its record's host)
  made on its Rust object; `label_call_f` (round 6) is `label_call` with the float buffer;
- the world views (`ee_sim::world`: `UnitView`, `ClassView`, `PlayerView`, `GroupView`, `world_field!`): a plain read
  of a unit, its class, the selection (ee_game `hud_views.rs`).

## Round 6 (branch `rust-ui6`, 2026-11-19)

| Commit | What | Net C++ (src, include) |
|---|---|---|
| 69206b18b | `EEUICChatLine` as a kind over the control shell (shells_chat.rs); its unit and header deleted | -36 |
| d85a56b70 | the HUD controls' plain reads in Rust: the box selection's eight lines (hud_draw.rs) and unit reads, the command's selection count and categories, the mouse modes' picked/first unit fields (hud_views.rs) | -35 |
| de87baf95 | the message box's plain toolkit calls on the direct path (shells_dialog.rs) | -29 |
| e32577e45 | the lane's dead members (tools/dead_members.py) | -19 |

Round total **-119** net C++ lines (+48 -167). One unit and one header deleted. The brief's -3,000 was not
reached: see "What is left" and PORT_LEARNINGS.md "The UI lane, round 6".

## What is left (the lane's part), and why

- **The census** (`tools/cpp_deps.py report` at the round's start): no User Interface Thread or UI Events unit is in
  any cycle (EEFOFindNewTargetNearOldTarget, the unit utilities, aside); `layer --focus 'User Interface Thread/,UI
  Events/'` lists only EEFOFind's edges. The one leaf is EEScreenEvent (47 lines: the toolkit's `UIEvent` subclass
  the screens' events need, a C++ vtable by nature).
- **What the C++ is.** Being out of the cycle did not change what the lines are: host cases whose callees are C++
  objects of other lanes (the picker, the actions, communications, the tech tree, the players' improvement and
  research calls, the campaigns, `FSFileSpec`/`FSPath`/`FSArchive`, `GESurface`/`GETexture`/`GEModel`, the
  rasterizer's state, `gGameWindow`, `gUserInterface`'s posts). A case goes when its callee is Rust; this round took
  the ones that were. At the round's start: EEScreens.cpp 2,766 lines (319 cases; 34 name a control, few of them
  plain toolkit calls), EEUIFMainGame.cpp 1,141, EEUserInterface.cpp 657, EEUIManager.cpp 452.
- **Next candidates, each small**: EEScreens.cpp's plain toolkit text cases (`DS_BUTTON`, `LABEL_LINES`,
  `CL_APPEND`, `LE_CHECK_LABEL`: need a `UWideString` from Rust bytes and the virtual `AppendText`); the status
  box's status-bar cases (`BAR_RANGE`, `BAR_VALUE`, `BAR_COLOR`: need the status bar's `mBar` from Rust);
  `EEUICPortrait`'s scalar members (`mUnit`, `mLevel`, the bars' flags) as a Rust record, then its plain reads;
  `EEUIFMainGame::HostOp`'s `FIELD` cases; the message box's `MESSAGE_SIZE`, `PLACE`, `RECT` (need the window size
  in Rust: there is no view of `gGameWindow` yet).
- **`EEUIRustControl`'s subclasses** left (`EEUICSelectUnits`, `EEUICCommandUnits`, `EEUICPortrait`, `EEUIMiniMap`):
  each owns C++ sub-objects (the actions, the arrow model and texture, the portrait bitmap and text, the mini map's
  bitmaps) and has an editor subclass or other lanes' readers (EERenderObject reads the selection box,
  EETestFixture the drag, EESEUICTriggerSelectUnits derives from it).
- **The improvement buttons** (`EEUIBUpgrade`, `EEUIFMGAttribute`, `EEUIFMGStatusBar`): no shell base
  (`EEUIBUpgrade` is a `UIButton` with two new virtuals; the editor's upgrade panel uses it).
- **`EEUISCIFormManager`**, **`EEUIBMapBackground::LoadBackground`**: file and surface C++.
- **`EEUIBuildWall`, `EEUIBuildGate`**: world calls and the editor's subclasses.
- **The LLE UI hosts**: a forwarder per member and a host switch of the control's own C++ virtuals and GE objects;
  their rhombi could become `list2d::Shape`s owned by the Rust objects (a few lines each).

## Checks

- Per piece: the build `--sim` 0/0; the Rust gate; the oracle at seeds 2-3.
- At the round's end: both builds and the test profile 0/0, `rust_build.py header` and the Rust gate, the oracle
  seeds 2-7, sim_editor, the civilization gate, scenario_check, the tours (the random game with box select and
  orders, the editor exercise, the tour with `--input-check --view-check`), `bash tools/gates.sh`.
