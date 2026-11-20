# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The scenario editor lane, round 1", "round 2").

**Scope** (agent rules "Lane scopes"): `src/Empire Earth/Scenario Editor` and `Scenario Manager` (the triggers),
their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`, `shells_seditor.rs`;
`shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs` are the UI lane's kinds of editor classes). The scenario
manager's Rust is `ee_sim::scenario`. The UI lane owns the shells' core (`ee/EEUIShell.h`, `EEUIShell.cpp`,
`shells.rs`, `shells_ffi.rs`) and the toolkit (`ee_ui`, `ee_ui::direct`).

## Where it stands (round 2)

- **No editor unit is in a cycle.** Cuts round 3 took the editor out of SCC 1; round 2 cut the last two editor
  cycles (EESEEvents <-> EESETSEffect: `FillScenarioComboBox` beside `FillPlaceComboBox`; EESEIAExport <->
  EESEInstructions: the import answer and the import button's event in EESEIAExport.cpp). The Scenario Manager's
  seven units (EESMArea, EESMCondition, EESMEffect, EESMMediaStrings, EESMObject, EEScenarioHost,
  EEScenarioManager, 3,514 lines) are a cycle inside SCC 1, and EEEvent.cpp is in SCC 1 (`cpp_deps.py layer --focus
  'Scenario Editor/,Scenario Manager/'`: 16 edges, 18 symbols from the rest of SCC 1: the tech tree's event tables,
  the physics' calls of the manager's Update, ShowDialogMessage and EndCinematic, EESMMediaStrings' Load/Save from
  the save header, EESMArea::IsUnset from the continent manager).
- **The editor's hosts answer the toolkit in Rust** (`editor_direct.rs`, round 2): every host case that was one
  call on a toolkit shell over a Rust object (texts, lines, selections, counts, display modes, rectangles, check,
  radio, tri-state and slider states, trigger events, help strings, groups, captures) is answered through
  `ee_ui::direct` in the FFI adapters, before the C++ host; the condition and effect screens' control records are
  Rust allocations with the C++ classes' layout (`EE_SCREENS_SEET_*_SLOTS`, asserted in EESETSEffect.cpp; the C++'s
  remaining cases read a member through `ee_screens_seet_slot`). All 12 adapters route the shared maker's ops
  through `editor_direct::sepl`; the form's, map's, object's, trigger's, tab's, unit's, area's and events' hosts
  have their own answer functions there.
- **What the C++ hosts hold now** (the round-2 census): constructors of C++ control, event and action classes
  (`SeplMake`, the tabs' `MAKE_*`, the form's `BASICS`, the brush group, the load thread), world statements on
  C++ objects (the scenario manager's lists and elements, the tech tree, `EEUnitClass::sUnitTypes`, the calamity
  and graphic effect classes, the editor's selection, the camera, the shells' kind ops through `EEUIShell::Op`),
  and the TEST ONLY alignment map (`gSeetAlignments`, read by EESimEditor.cpp).

## The editor's objects

- **The form** (`EEUIFScenarioEditor`, a `UIForm`): a C++ class (about 45 files reach it through
  `gUIManager->GetScenarioEditorForm()`) over its Rust object (`editor_form.rs`); members are inline `Call`s.
- **The tab groups** (round 1): Rust objects (`editor_groups_ffi.rs`, `ee_screens_segr_*`); C++ reaches a tab
  through the form (`GetTab`, `TriggerCall`, `UnitCall`, `PlayerCall`, `GetTriggerBox`, `TriggerSubOp`).
- **The trigger tab's five sub-screens** (round 1): one Rust record in the trigger tab (`editor_tsubs.rs`).
- **The condition and effect screens' control records** (round 2): Rust allocations (`editor_direct.rs`).
- **Controls as kinds**: this lane's `EESEUIBDiplomacy`, `EEUIFSEActionButton` (`shells_seditor.rs`); the UI lane's
  editor kinds (`shells_editor.rs`, `shells_tech.rs`, `shells_unit.rs`, `shells_paint.rs`).

## Numbers this lane uses

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf. The hosts' op ranges: `_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..,
  `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., `_setn` 1000..; `_seet`/`_sect` and
  `_see` have their own numbers.

## Left

- **Classes still C++**: the form; the terrain list boxes (`EESEUITerrainColorLB`, `EESEUITerrainTypeLB`:
  `UIListBox<T>` templates, no shell) and their two events; the brush group (`EEUIFSEBrushGroup`, read by the
  render snapshot); the upgrade panel (`EEUIFSEUpgradePanel`); the load thread (`EESELoadThread`); the import and
  export actions and events (`EESEIAExport.cpp`); the editor's HUD subclasses (`EESEUICCommandUnits`,
  `EESEUICTriggerSelectUnits`, `EESEUIBuildWall`/`Gate`); the editor's actions (`EESEARust`, `EESEAPaintRust`,
  Scenario Editor Actions).
- **What would free the hosts' remaining cases**: the toolkit classes the constructors `new` as kinds or Rust
  objects with a Rust constructor (the UI lane's); the scenario elements' strings and lists as Rust records (the
  editor gate, EESimEditor.cpp, reads them in place: the game-core lane's file); `EEUnitClass::sUnitTypes`, the
  calamity and graphic effect class tables and `gCPBehaviors` on the world views; the shells' kind ops reachable
  from Rust by C++ pointer (a registry like `ee_ui::direct`'s in the UI lane's `shells.rs`).
- **Scenario Manager**: EESMEffect.cpp's world host (1,426 lines with the effect's strings and EESMCreateUnit),
  EEEvent.cpp's handlers (each a C++ setter over ee_tech's decision), EEScenarioManager.cpp's cinematics and list,
  stream and round hosts, EESMObject.cpp's read host (units, map, AI), EEScenarioCapture.cpp (TEST ONLY, 615 lines,
  its bytes fixed by goldens). None is a whole move before the units, players and AI their statements call are Rust
  objects.

## Checks

- Per step: the build `--sim` 0/0, the Rust gate, the editor gate (`tools/sim_editor.py`, PASS 14,448), the
  civilization gate (`--civbuilder`, PASS 4,062), `scenario_check.py` 27/27, the oracle seeds 2-3 against `ref`
  (`build/se2/step.sh TAG` in the round-2 worktree).
- Round end: both builds, the oracle 2-7, the scenario capture profile at seeds 2-3, the editor tour
  (`fixture_game.py --exercise editor` through slot.py), `gates.sh`.
