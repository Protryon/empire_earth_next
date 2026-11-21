# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The scenario editor lane, round 1" .. "round 5").

**Scope** (agent rules "Lane scopes"): `src/Empire Earth/Scenario Editor` and `Scenario Manager` (the triggers),
their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`, `shells_seditor.rs`;
`shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs` are the UI lane's kinds of editor classes). The scenario
manager's Rust is `ee_sim::scenario`. The UI lane owns the shells' core (`ee/EEUIShell.h`, `EEUIShell.cpp`,
`shells.rs`, `shells_ffi.rs`) and the toolkit (`ee_ui`, `ee_ui::direct`).

## Where it stands (round 5)

Rounds 3-5 work under "Calling C++ from Rust is fine": a host case or a C++ function that was a statement sequence
over C++ objects is composed in Rust of one-statement C++ shims, in the C++'s order; the composition sits in the C ABI
adapter (`compose(op, args)`), so the older Rust and its recorded-stream tests see the same ops. Round 5 (three forks:
sm, act, ed) took the lane's C++ from about 3,220 logic lines (`.cpp` and headers) to about 1,440; see
PORT_LEARNINGS.md "The scenario editor lane, round 5" for the whole repository's count.

### The ids

`ee/EEScenarioIDs.h` and `ee/EEScenarioEditorIDs.h` are gone: their constants are `ee_sim::scenario::ids`
(`EE_SCENARIO_SM_*`, 248) and `ee_game::screens::editor_ids` (`EE_SCREENS_SE_*`, 151), with their evidence as docs;
the C++ reads them from `rust/sim/scenario.h` and `rust/game/screens.h`. `ee/EESEARust.h`'s `kSEA*`/`kSEE*` are gone
too (the C++ uses `EE_SCREENS_SEW_*`/`EE_SCREENS_SEE_*`; `kSEAPaint*` stay in the units lane's ee/EEARust.h). The
static_asserts that compared C++ constants with Rust's are `editor_cpp_consts_tests.rs` (it reads the C++ headers).

### The scenario manager (`ee_sim::scenario`)

- **Whole in Rust**: the capture records (`sm_capture.rs`); `EESMStoreAI` (`ai_store.rs`); the trigger's members
  (`trigger_members.rs`); `EESMArea` (round 5: its numbers are `EeScenarioAreaFields`, a record the C++ class derives
  from, every method a one-statement forward; `area_fields.rs`); `EESMDialogMessage`'s numbers
  (`EeScenarioMessageFields`); `EEEventEffect` is Rust's `EeScenarioEvEffect`; the elements' glue (round 5,
  `entry.rs`): Load/Save of the effect, condition, object, trigger, area, media strings, dialog message and manager,
  the Copy methods, the manager's Reset, Update, Prepare, ShowDialogMessage, the list operations (sort, new, move,
  delete, duplicate, names, finds), ExportText/ImportTriggerText, `EESMMediaStrings::ListMediaFiles`, and every
  capture wrapper (seed, run, record in the C++'s order). The host contexts and stream/list/round/text tables are
  built in Rust over `extern "C"` C++ callbacks (`ee_scenariocpp_host_*`, `_stream_io`, `_list_op`, `_round_op`,
  `_text_op`, `_entry`).
- **For the AI lane**: `ee_scenario_area_region_inputs(EeScenarioArea*, long* ids, size_t capacity) -> size_t` (C)
  and `ee_sim::scenario::entry::region_inputs()` (Rust) give BuildRegions' scenario inputs (set, the rectangle, height,
  width, the id); EEAIContinentManager.cpp still builds them itself.
- **Composed over one-statement C++** (rounds 3-4): the effect run's world (`world_compose.rs`), its texts
  (`effect_text.rs`), the class attributes (`ca_compose.rs`), the host's reads (`read_compose.rs`), the object reads
  (`object_reads.rs`), EEEvent's handlers (`event.rs`), the cinematics (`cinematic.rs`), the manager's statements
  (`manager_compose.rs`).
- Left in C++ (about 520 logic lines): `EESMEffect.cpp` (153: the run's context members and the statement switches'
  dispatch), `EEScenarioManager.cpp` (97: the round, list and text hosts), `EEScenarioManager.h` (83: declarations
  and inline getters), `EEScenarioHost.cpp` (44), `EEEvent.cpp` (37), CreateUnits' C++-typed locals.

### The scenario editor (`ee_game::screens`)

- **Objects**: the form (`editor_form.rs`; round 5: its constructor, CopyData, UpdateKeyboard and UpdateMouse
  composed, the slot enum is `EE_SCREENS_SEFM_*`), the tab groups, the trigger tab's sub-screens, the condition and
  effect screens' records, the controls as kinds.
- **Composed over one-statement C++**: the shared maker and the player tab (`editor_sepl.rs`, `editor_seplx.rs`), the
  load thread and text export/import (`editor_selt.rs`), the form's host, map, elevation, events, upgrade panel
  (`editor_sefmx.rs`, `editor_tabsx.rs`), the trigger tab family (`editor_trigx.rs`), the unit and terrain tabs
  (`editor_seunx.rs`, `editor_setnx.rs`; round 5: the gate and wall tools' Cancel, the unit lists), the editor
  actions' hosts (round 5, `editor_seax.rs`: sea, sew, sep, seu over `ee_screenscpp_seax`; the tile units, the
  bulldozer's remove, the elevation file read and the unit pick whole in Rust).
- Left in C++ (about 920 logic lines): `EEUIFSETerrain.cpp` (90), `EESEUnit.cpp` (81), `EESEARust.cpp` (64: the
  hosts' preludes, EESEPUnit/EESEPComplexUnit, the command control's constructor and RustOp), the headers' class
  shells (EEUIFScenarioEditor.h 54, EEUIFSETerrain.h 39), the trigger screens (30-42 each), EESERBUnitTypes.cpp (35).

## Numbers this lane uses

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf. The hosts' op ranges: `_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..,
  `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., `_setn` 1000..; `_sepx` 0..173 and
  200..308; the statement hosts (`_seltx`, `_sefmx`, `_seunx`, `_setnx`, `_seetx` with `_seobx`.. `_seinx`, `_sempx`,
  `_seelx`, `_seupx`, `_sex`, `_seex`) have their own numbers each; round 5's additions are 2000.. in each (SEFMX
  2000..2005, SEUN 2000..2001, SEUN_OP 2000..2003). The actions' statement host `ee_screenscpp_seax`:
  `EE_SCREENS_SEAX_*` 3000..3181.
- The scenario world channel (one number space; `ee_scenariocpp_host_world`, once `EESMEffectWorld`, dispatches by range): `EE_SCENARIO_WORLD_*` 0..198,
  `WX` 400..636, `WC` 700..732, `WK` 740..765, `RX` 770..801, `WY` 810..853, `WZ` 900..938. `EE_SCENARIO_ROUND_*`
  0..17 (round.rs), 20..75 (cinematic.rs), 76..94 (manager_compose.rs). The stream channel `EE_SCENARIO_IO_*` 0..21
  (stream.rs), 22..31 (manager_compose.rs), 32 (the trigger). `EE_SCENARIO_EV_*` 0..131, `EE_SCENARIO_TR_*` 0..46,
  `EE_SCENARIO_CAP_*` 0..171. Round 5's channels: the entries host `EE_SCENARIO_EN_*` 0..47, the list entries
  `EE_SCENARIO_LS_*` 0..9, the manager entries `EE_SCENARIO_MN_*` 0..3. Parallel forks: give each its range up front (two forks both took IO 22 in round 4).

## Left

- The remaining ~1,440 logic lines above (counted with `build/ld.py`-style per-file logic_lines over the lane's files),
  the same composition. The biggest: EESMEffect.cpp's run context and dispatch, EEScenarioManager.cpp's hosts and
  EEScenarioManager.h's declarations, the terrain and unit tabs' hosts, EESEARust.cpp's class parts.
- **The counter's one-liners**: `case X: stmt; return 0;` on one line counts as logic in `logic_lines.py` (two
  semicolons) unless its second statement is a return/break; write new shims two-line.
- The AI lane can switch BuildRegions to `ee_scenario_area_region_inputs` (its file, not this lane's).

## Checks

- Per step: the build `--sim` 0/0, `cargo test -p ee_sim --lib scenario::` and clippy `-D warnings` on the touched
  crates, the editor gate (`tools/sim_editor.py`, PASS 14,448: the tabs, the trigger screens, terrain and placement
  through EE-SIM, no drawing), the civilization gate (`--civbuilder`, PASS 4,062), `scenario_check.py` 27/27, the
  oracle seeds 2-3 against `ref`, the scenario capture profile at seeds 2-3.
- Round end: both builds, the Rust gate, the oracle 2-7, the 12 capture profiles at seeds 2-3, the plain saveload and
  the matrix, the editor tour (`fixture_game.py --exercise editor` through slot.py: compare its frames with the base
  build's; the game view and the mini map vary by run), eedata check and cppgate, `gates.sh`.
- Only a person reaches: the load thread's posts, the text export/import dialogs, the unit tab's mouse previews and
  hold/cancel, the terrain paint control's drag, the diplomacy events' clicks, the dialog messages' camera moves
  outside EE-SIM's scenario dialogs, the drawing of every tab past the editor's first frame; since round 5 also the
  actions' pause and resume, the elevation file read, real mouse picks, the lasso, drag paints and ghost placement,
  the area screen's anchor and rectangle setters, the campaign media listing.
