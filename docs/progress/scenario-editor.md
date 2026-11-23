# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The scenario editor lane, round 1" .. "round 6").

**Scope** (agent rules "Lane scopes"): `src/Empire Earth/Scenario Editor`, `Scenario Editor Actions` and `Scenario
Manager` (the triggers), their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`,
`shells_seditor.rs`; `shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs` are the UI lane's kinds of editor
classes). The scenario manager's Rust is `ee_sim::scenario`. The UI lane owns the shells' core (`ee/EEUIShell.h`,
`EEUIShell.cpp`, `shells.rs`, `shells_ffi.rs`) and the toolkit (`ee_ui`, `ee_ui::direct`).

## Where it stands (round 6)

Rounds 3-6 work under "Calling C++ from Rust is fine": a host case or a C++ function that was a statement sequence
over C++ objects is composed in Rust of one-statement C++ shims, in the C++'s order. Round 6 (three forks: sm, act,
ed) took the lane's C++ from about 1,440 logic lines (`.cpp` and headers) to about 877 (-560 repository-wide, see
PORT_LEARNINGS.md "The scenario editor lane, round 6"). The round's target (-1,300) was not reached.

### The ids

`ee/EEScenarioIDs.h` and `ee/EEScenarioEditorIDs.h` are gone (round 5): their constants are `ee_sim::scenario::ids`
(`EE_SCENARIO_SM_*`) and `ee_game::screens::editor_ids` (`EE_SCREENS_SE_*`). Tests that compare C++ constants with
Rust's read the C++ headers (`editor_cpp_consts_tests.rs`, `scenario/cpp_consts_tests.rs`).

### The scenario manager (`ee_sim::scenario`)

- **Whole in Rust**: the capture records, `EESMStoreAI`, the trigger's members, `EESMArea` (its numbers a record the
  class derives from), `EESMDialogMessage`'s numbers, `EEEventEffect` (Rust's `EeScenarioEvEffect`), the elements'
  glue (`entry.rs`). Since round 6 also the world channel's routing (`world_route.rs`: Rust picks the C++ handler by
  op range), the effect run's context and Create Units' context made by Rust, the round methods and capture records
  called from Rust, the trigger entries as Rust records; `ee/EESMTEffectWrapper.h` and `ee/EESMTConditionWrapper.h`
  are gone.
- **Composed over one-statement C++**: the effect run's world (`world_compose.rs`), its texts (`effect_text.rs`), the
  class attributes (`ca_compose.rs`), the host's reads (`read_compose.rs`), the object reads (`object_reads.rs`),
  EEEvent's handlers (`event.rs`), the cinematics (`cinematic.rs`), the manager's statements (`manager_compose.rs`).
- C++ callbacks whose C++ definitions take a typed context are in `rust/cbindgen.toml`'s `exclude` (the C++ declares
  them itself).
- Left in C++ (about 410 logic lines): EESMEffect.cpp 90 (the atom and Y worlds' multi-line cases, the run's
  members), EEScenarioManager.cpp 65, EEScenarioManager.h 50 (the class's declarations and members), EEScenarioHost
  .h/.cpp 28/25, EESMMediaStrings.h 23, EESMTrigger.h 22, EESMEffect.h 21, EEEvent.h/.cpp 21/20, EESMObject.h/.cpp
  20/15, EESMArea.h 15, EESMCondition.h 12.

### The scenario editor (`ee_game::screens`)

- **Objects**: the form (`editor_form.rs`), the tab groups, the trigger tab's sub-screens, the condition and effect
  screens' records (round 6: their C++ control headers deleted, Rust's tables alone), the controls as kinds. Round 6
  also: the upgrade panel whole in Rust (`ee/EEUIFSEUpgradePanel.h` deleted; `ee_screens_seup_new/_show/_free`), the
  trigger text export/import as Rust action kinds (`ee/EESEIAExport.h` deleted), the terrain lists' change events
  and the player-count action as Rust kinds.
- **Composed over one-statement C++**: the maker and the player tab, the load thread (round 6: Post40B, Post40C),
  the form's hosts, the trigger family, the unit and terrain tabs, the actions' hosts. Round 6 folded the small hosts
  into their statement hosts: seun into seunx, setn into setnx, sea/sew/sep/seu into seax at offset bases
  4200/4400/4600/4800, the trigger screens' small hosts into theirs.
- Left in C++ (about 470 logic lines): EEUIFScenarioEditor.h 54 (the form's class), EESETriggerScreen.cpp 37,
  EESEUnit.cpp 29, EESETSEffect.cpp 28, EEUIFSETerrain.h 27 (the list box classes), EESEARust.cpp 25, EESERBUnitTypes
  .cpp 23, EEUIFScenarioEditor.cpp 21, EESEInstructions.cpp 21, EESETSObject.cpp 20, EEUIFSETerrain.cpp 17,
  EEUIFSEBrushGroup.h 16 (game core includes it), EESELoadThread.h 16, EESETSArea.cpp 12, smaller files.

## Numbers this lane uses

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf. The hosts' op ranges: `_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..,
  `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., `_setn` 1000..; `_sepx` 0..173 and
  200..308; round 5's additions 2000.. in each statement host; `EE_SCREENS_SEAX_*` 3000..3181.
- Round 6: act's new ops 4000..4014 (`EE_SCREENS_SEE_TERRAIN_TYPE/_COLOR` 4000/4001, `SEW_OP_SET_MAX_PLAYERS` 4000;
  the folded action hosts reached through seax at bases 4200/4400/4600/4800, sew's 4000 at 8400); ed's 5000..
  (`SEW_OP_TEXT_RUN/_FREE` 5000/5001) and, because `editor_trigx_ffi.rs` routes the trigger family by number range
  (600+ to seinx), SETSX 480 and SEARX 580..586.
- The scenario world channel (`ee_scenariocpp_host_world`, routed in Rust by range): `EE_SCENARIO_WORLD_*` 0..198,
  `WX` 400..636, `WC` 700..732, `WK` 740..765, `RX` 770..801, `WY` 810..853, `WZ` 900..938, round 6's 3000..3004.
  `EE_SCENARIO_ROUND_*` 0..94; the stream channel `EE_SCENARIO_IO_*` 0..32 and 3000; `EE_SCENARIO_EV_*` 0..131 and
  3000..3002; `EE_SCENARIO_TR_*` 0..46; `EE_SCENARIO_CAP_*` 0..171; `EE_SCENARIO_EN_*` 0..47; `EE_SCENARIO_LS_*`
  0..9; `EE_SCENARIO_MN_*` 0..3 and 3000..3008. Parallel forks: give each its range up front.

## Left

- About 877 logic lines (above). What remains is mostly class shells other lanes' code uses (EEScenarioManager,
  EEUIFScenarioEditor, the terrain list boxes, the brush group, the wall/gate and select-units classes that derive
  from UI-lane bases), the effect run's multi-line world cases, and the trigger screens' control constructions.
  Moving those needs whole-class moves (Rust-owned lists behind the manager's accessors, the form as a shell kind).
- act: about five seax statics could be Rust-held values; the rest of EESERBUnitTypes.cpp needs `editor_seplx.rs`.
- The AI lane can switch BuildRegions to `ee_scenario_area_region_inputs` (its file).
- EECampaign* is the world lane's (it moved EECampaign's editor side in round 1).

## Checks

- Per step: the build `--sim` 0/0, `cargo test` on the touched modules and clippy `-D warnings`, the editor gate
  (`tools/sim_editor.py`, PASS 14,448 lines), `--civbuilder` (PASS 4,062), `scenario_check.py` 27/27, the oracle
  seeds 2-3, the scenario capture profile at seeds 2-3; eedata `check` and `cppgate` when a record eedata reads
  changes.
- Round end: both builds, the Rust gate, the oracle 2-7, the 12 capture profiles at seeds 2-3, the plain saveload
  and the matrix, the editor tour (`fixture_game.py --exercise editor` through slot.py, frames against the base
  build's; the game view and the mini map vary by run), eedata check and cppgate, `gates.sh`.
- Only a person reaches: the load thread's posts, the text export/import dialogs, the unit tab's mouse previews and
  hold/cancel, the terrain paint control's drag, the terrain lists' drawing, preview tint and stretch, the type
  list's destructor at the editor's close, the diplomacy events' clicks, the actions' pause and resume, real mouse
  picks, the lasso, drag paints and ghost placement, the area screen's anchor and rectangle setters, the campaign
  media listing, the drawing of every tab past the editor's first frame.
