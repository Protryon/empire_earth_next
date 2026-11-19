# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The scenario editor lane, round 1").

**Scope.** `src/Empire Earth/Scenario Editor/*`, `Scenario Editor Actions/*`, the editor-only classes in `User
Interface Thread`, their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`,
`shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs`, `shells_seditor.rs`). The UI lane owns the shells' core
(`ee/EEUIShell.h`, `EEUIShell.cpp`, `shells.rs`, `shells_ffi.rs`); the area effects (`EEAEManager`, `EEAreaEffect`)
are simulation code and stay out.

## The editor's objects (round 1)

- **The form** (`EEUIFScenarioEditor`, a `UIForm`): still a C++ class (about 45 files reach it through
  `gUIManager->GetScenarioEditorForm()`); its object is Rust (`editor_form.rs`), its members are one-line inline calls
  (`Call`), its virtuals (UpdateKeyboard, Open, DrawBackground, Close, CopyData, UpdateMouse) forward. It keeps the
  civilization builder's `EECivilization` (`mC1A0`).
- **The tab groups** (once `EEUIFSEMainGroup` and its seven subclasses): Rust objects (`editor_groups_ffi.rs`,
  `ee_screens_segr_*`), the form's slots hold them. A group holds its tab's FFI object (`editor_map_ffi.rs` ..
  `editor_instructions_ffi.rs`) and answers the slots the class overrode (`editor_groups.rs`). C++ reaches a tab through
  the form: `GetTab(kEditorTab*)`, `GetTriggerTab`, `GetUnitTab`, `GetPlayerTab`, and `TriggerCall`, `UnitCall`,
  `PlayerCall`, `GetTriggerBox`. The map and terrain tabs' shell is the group (`ee_screens_segr_tab`); the unit tab's
  C++ parts (the build tools, the rhombus, the panel rectangle: `struct EESEUnit`) are its shell, owned by the group
  (`ee_screens_segr_parts`).
- **The trigger tab's sub-screens** (once `EESETSBaseObject` and `EESETSTrigger`, `EESETSCondition`, `EESETSEffect`,
  `EESETSObject`, `EESETSArea`): one Rust record in the trigger tab (`editor_tsubs.rs`: the selections and the slots'
  table; `ee_screens_sets_sub`, `_sub_op`, `_sub_call`, `_sub_select`).
- **Controls as kinds** (`shells_seditor.rs`, this lane's numbers): `EESEUIBDiplomacy`, `EEUIFSEActionButton`. Round
  3 of the UI lane made 23 editor classes kinds (`shells_editor.rs`, `shells_tech.rs`, `shells_unit.rs`); the UI round
  4 lane the paint areas (`shells_paint.rs`).

## Numbers this lane uses (so the lanes do not collide)

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf (`EEUIShell.cpp`'s cases under "the scenario editor lane's").

## Left (C++ lines in the lane's files, about 7,000)

- **Host cases** (most of what is left): one C++ case per call the Rust makes into a C++ object: the toolkit's
  constructors (`SeplMake` and the tabs' own makers), the world, the scenario manager, the tech tree, the actions'
  map edits (`EESEARust.cpp`), the load thread's media loads (`EESELoadThread`). Each needs that object's Rust (the
  world's views, the scenario manager's records) or stays a case.
- **Classes still C++**: the form (`EEUIFScenarioEditor`: a form-shell kind next; its callers are other lanes'
  files), the terrain list boxes (`EESEUITerrainColorLB`, `EESEUITerrainTypeLB`: `UIListBox<T>` templates, no shell)
  and their two events, the brush group (`EEUIFSEBrushGroup`, read by the render snapshot), the upgrade panel
  (`EEUIFSEUpgradePanel`, HUD controls), the load thread, the import and export actions (`EESEIAExport`), the
  editor's HUD subclasses (`EESEUICCommandUnits`, `EESEUICTriggerSelectUnits`, `EESEUIBuildWall`/`Gate`), the
  editor's actions (`EESEARust`, `EESEAPaintRust`).

## Checks

- `build/sed/step.sh TAG` (per step): the Rust gate, the editor gate (PASS 14,448), the civilization gate (PASS 4,062),
  `scenario_check` 27/27, the oracle seeds 2-3 against the main checkout's `ref`.
- The round's start test-profile exe: `build/sed/start/EE-AOC.exe` (the editor exercise's baseline).
