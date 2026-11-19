# The UI cluster lane (the main game HUD, the menus, the UI events, the civilization builder)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 4").

**Scope (from round 4).** `src/Empire Earth/User Interface Thread/*` but the editor-only controls (`EESE*`,
`EEUIFSE*`) and `EEFOFindNewTargetNearOldTarget.cpp` (the units cluster's); `src/Empire Earth/UI Events`,
`Civilization Builder`; their `include/ee` headers; the shells (`ee/EEUIShell.h`, `EEUIShell.cpp`, ee_game
`screens/shells*.rs`) and `ee_ui`/`ee_game` screens. The scenario editor (`Scenario Editor`, `Scenario Editor
Actions`, the editor's controls, forms and factories: `SeplMake`, the effect slots) is a lane of its own from round
4; it builds on the shells.

## The generic shells

A game class that only overrode a toolkit class's virtuals can be deleted whole:

- `ee/EEUIShell.h`: one C++ shell per toolkit base: `EEUIShellButton`, `EEUIShellRadio`, `EEUIShellCheck`,
  `EEUIShellCombo`, `EEUIShellList` (`UILBString`), `EEUIShellEdit`, `EEUIShellSlider`, **`EEUIShellControl`** (a
  plain `UIControl`; the pure virtuals `EEUIRustControl`'s defaults) and **`EEUIShellForm`** (a `UIForm`: Activate,
  Deactivate, DrawBackground, RelayoutForm, SetHelpTextPopupLocation, UpdateKeyboard, UpdateMouse,
  UpdateTranslatedKey, Close, CopyData, DrawPolygons, DrawTextA, Open, SetRectangle; both constructors).
- A shell holds a Rust kind (`EE_SCREENS_SK_*`); each virtual asks the kind when its mask has the bit
  (`EE_SCREENS_SV_*`), else runs the base's. The kind calls the base back (`EE_SCREENS_SH_BASE + v`; a form's
  `EE_SCREENS_SH_FORM_BASE + v`), its own virtuals (`EE_SCREENS_SH_SELF`, a form's `EE_SCREENS_SH_FORM_SELF`), and
  the game's C++ through one host, `ee_screenscpp_shell` (`User Interface Thread/EEUIShell.cpp`).
- A C++ caller of a deleted class's method calls `EEUIShell::Op(control or form, EE_SCREENS_SKOP_*)`.
- The editor and civilization gates print a shell's `KindName()` (the deleted class's name) for controls and, from
  round 4, for forms (`EESimCivBuilder`; `EESim`'s top form class too).
- Numbers taken: kinds 1..0x54 (round 3), 0x60..0x61 (paint areas), 0x68..0x6a (message box), 0x80 (saved
  group), 0x81 (icon bar); kind ops alike; host ops 0x80..0x96, 0xb0..0xbd; SV 0..33; `SH_FORM_SELF` 0xff,
  `SH_FORM_BASE` 0x200. The scenario editor lane's: SK, SKOP 0x70..0x7f, host ops 0xa0..0xaf.

Kinds (deleted classes), round 3: `EEUIBColor`, `EESEPSSetPlayers`, `EEUIFSERBBrush`, the trigger screens' six combo
boxes, `EEUIFMGErrorLog`, `EEUIFSEESElevation`, `EESETSAEditBox`, `EESETSOUnitList`, `EESEBSelect`, `EESETab`,
`EESERBUnitTypes`, `EESECBMaxPlayer`, `EEUIFSEMainButton`, `UIBMap`, `UIRBMap`, `EEUIFSETRBPaintType`,
`EESEUITechListBox`, `EEUIFSELBUnitList`, `EEUIFSELBCargoList`, `EEUIFSEBUnitType`, `EEUIFSEBRotation`,
`EEUIFSEBRemoveCargo`. Round 4: `EEUIFSEPaint`, `EEUIFSEPaintElevation`, `EEUIFSETerrainPaint` (shells_paint.rs),
`EEUIFError`, `EEUIFEBOK`, `EEUIFEBCancel` (shells_dialog.rs), `EEUIBSavedGroup` (shells_saved.rs),
`EEUICUnitIconBar` (shells_icon_bar.rs).

## What is left (the lane's part)

- **`EEUIRustControl`'s subclasses** (`EEUICPing`, `EEUICChatLine`, `EEUICSelectUnits`, `EEUICCommandUnits`,
  `EEUICPortrait`, `EEUIMiniMap`): their logic is Rust over records the C++ class's base holds
  (`EeScreens*Fields`, found by address, e.g. `hud_ctl_ffi.rs`'s chat line offset); each owns C++ sub-objects
  (texts, timers, lines, bitmaps, actions) its `RustOp` drives. The way over `EEUIShellControl`: the record owned by
  the kind, the sub-objects in the shell's slots, the class ops one host case per kind (as the icon bar's). `EEUICPing` is the WON
  forms' (wait for the WON lane); `EEUICSelectUnits`, `EEUICCommandUnits`, `EEUICPortrait` and `EEUIMiniMap` have
  editor users (`EESEUICTriggerSelectUnits`, `EESEUICCommandUnits`, the upgrade panel, the editor form) and other
  lanes' readers (`EERenderObject`, the fixture, `EEMiniMap`): agree each move with them. `EEUICChatLine` is
  the next (main game only; its record is found by an offset from the control, `hud_ctl_ffi.rs`).
- **The improvement buttons** (`EEUIBUpgrade`, `EEUIFMGAttribute`, `EEUIFMGStatusBar`): upgrade.rs has the logic; the
  status bar's constructor makes six C++ parts the status box reads (`mBar`, `mIcon`, `mValue`...). The editor's
  upgrade panel constructs them (the editor lane's file: a factory function keeps it to one line there).
- `EEUIFMGStatusButton` (a record base and a 250-line host of world calls), `EECBUIListBox` (virtuals the list
  shell lacks: AppendStringArray, AddLine, DeleteLine; the civilization gate reads its members), the form
  `EEUIFMainGame` (50 files call it: a free-function facade first) and the HUD's host cases (the world, the picker,
  communications: each needs that object's Rust).

## Checks

- The round's start test-profile exe and its harness runs (`build/uc4/pre_*`: the tour `--input-check --view-check
  --seed 7`, the orders game `--box-select --orders --scroll-check --seed 7`, the editor exercise, each with the 2D
  draw digest `EE_DRAW2D_DIGEST`); `build/uc4/digdiff.py A B` lists the commands only one digest has.
- After each step (`build/uc4/step.sh TAG`): the build `--sim`, the Rust gate, the oracle seeds 2-3, the editor gate,
  the civilization gate, scenario_check, the test-profile build, the three harness runs and their digests.

## Round 4

See PORT_LEARNINGS.md "The UI cluster lane, round 4".
