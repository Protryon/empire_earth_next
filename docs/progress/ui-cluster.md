# The UI cluster lane (the main game HUD, the UI events, the scenario editor, the civilization builder)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The UI cluster lane, round 1" .. "round 3").

**Scope.** `src/Empire Earth/User Interface Thread/*` (but `EEFOFindNewTargetNearOldTarget.cpp`, the units
cluster's), `src/Empire Earth/UI Events`, `src/Empire Earth/Scenario Editor`, `Scenario Editor Actions`,
`Civilization Builder`, their `include/ee` headers and the matching Rust (`ee_ui`, `ee_game` screens).

## The generic shells (round 3)

A game control class (a C++ subclass of a toolkit class for its virtual overrides) can now be deleted whole:

- `ee/EEUIShell.h`: one C++ shell per toolkit base: `EEUIShellButton`, `EEUIShellRadio`, `EEUIShellCheck`,
  `EEUIShellCombo`, `EEUIShellList` (`UILBString`), `EEUIShellEdit`, `EEUIShellSlider`. A shell holds a Rust kind
  (`EE_SCREENS_SK_*`); each virtual asks the kind when the kind's override mask has its bit (`EE_SCREENS_SV_*`),
  else runs the base's. The kind calls the base's back (`EE_SCREENS_SH_BASE + v`), its own virtuals through the
  vtable (`EE_SCREENS_SH_SELF`), and the game's C++ through one host, `ee_screenscpp_shell`
  (`User Interface Thread/EEUIShell.cpp`; a few ops are answered in Rust before the hop: the id, the map and
  terrain tabs' checks, a kind op on another shell). A C++ caller of a deleted class's own method calls
  `EEUIShell::Op(control, EE_SCREENS_SKOP_*)`.
- `ee_game screens/shells.rs` (the trait `Kind`, the host trait, the first kinds), `shells_editor.rs`,
  `shells_tech.rs`, `shells_unit.rs`, `shells_ffi.rs` (the C ABI and the shells by control).
- The editor and civilization gates print a shell's kind name (`EEUIShell::KindName`: the deleted class's name),
  so their expected outputs are unchanged.
- C++ sub-objects the kinds own (the editor's cell bitmaps, the rotation button's arrows) are made by host ops
  (`EE_SCREENS_SH_NEW_BITMAP`, `_BITMAP_COORDS`, `_BITMAP_SCALE`, `_BITMAP_RELEASE`); `Kind::free` is the
  destructor's body.

Kinds (deleted classes): `EEUIBColor`, `EESEPSSetPlayers`, `EEUIFSERBBrush`, `EESETSCComboBox`, `EESETSEComboBox`,
`EESETSEScenarioComboBox`, `EESETSTriggerUIComboBox`, `EESETSAComboBox`, `EESETSUICBPlayer`, `EEUIFMGErrorLog`,
`EEUIFSEESElevation`, `EESETSAEditBox`, `EESETSOUnitList`, `EESETSShowIDsCheckBox`, `EESEBSelect`, `EESETab`,
`EESERBUnitTypes`, `EESECBMaxPlayer`, `EEUIFSEMainButton`, `UIBMap`, `UIRBMap`, `EEUIFSETRBPaintType`,
`EESEUITechListBox` (whole: its sort is `ee_sim::units::msvc_sort`, MSVC 14.44's `std::sort`), `EEUIFSELBUnitList`,
`EEUIFSELBCargoList`, `EEUIFSEBUnitType`, `EEUIFSEBRotation`, `EEUIFSEBRemoveCargo`.

## The measure

About 34k C++ lines left in the lane's files. What is left is of three kinds:

- **Host cases** (about a thousand): one C++ `case` per call the Rust makes into a C++ object of the game (the
  world, the picker, ghosts, communications, the scenario manager, gMouse, gViewport, the tech tree, the graphics
  thread). Each needs that object's Rust or stays a case.
- **Classes still C++ over the toolkit**: `EEUIRustControl` and its subclasses (`EEUICPing`, `EEUICChatLine`,
  `EEUICSelectUnits`, `EEUICCommandUnits`, `EEUICUnitIconBar`, `EEUICPortrait`, `EEUIMiniMap`: a `UIControl`
  shell is the next step, its pure virtuals need defaults), the paint areas (`EEUIFSEPaint`,
  `EEUIFSEPaintElevation`, `EEUIFSETerrainPaint`: the same `UIControl` shell, and their geometry), `EEUIBUpgrade`,
  `EEUIFMGAttribute`, `EEUIFMGStatusBar`, `EEUIFMGStatusButton`, `EEUIBSavedGroup`, `EEUIBGenericBackground` (a
  background, not a control), `EESEUIBDiplomacy` (its six shared bitmaps are passed to the button's constructor),
  `EEUIFSEActionButton`, the message box's `EEUIFEBOK`/`EEUIFEBCancel` (the box reads their members), `EECBUIListBox`
  (the civilization gate reads its members), `EEUIFileComboBox`, the forms (`EEUIFMainGame`, `EEUIFScenarioEditor`,
  `EEUIFError`).
- **C++ logic not yet ported**: the paint areas, the saved groups, the map backgrounds, the status bar, the upgrade
  panel, the editor's control factories (`SeplMake`, the effect slots), the civilization builder's area effects
  (`EEAEManager`, `EEAreaEffect`: simulation code; they need the sim_capture profiles).

## Checks

- The round's start test-profile exe and its harness runs (`build/uc3/pre`: the tour `--input-check --view-check
  --seed 7`, the orders game `--box-select --orders --scroll-check --seed 7`, the editor exercise, each with the 2D
  draw digest `EE_DRAW2D_DIGEST`); `build/uc3/digdiff.py A B` lists the commands only one digest has.
- After each step (`build/uc3/step.sh TAG`): the build `--sim`, the Rust gate, the oracle seeds 2-3, the editor gate,
  the civilization gate, scenario_check, the test-profile build, the three harness runs and their digests.

## Round 3

See PORT_LEARNINGS.md "The UI cluster lane, round 3" (the commits, the gates, what exercises what).
