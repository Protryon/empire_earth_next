# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The scenario editor lane, round 1" .. "round 4").

**Scope** (agent rules "Lane scopes"): `src/Empire Earth/Scenario Editor` and `Scenario Manager` (the triggers),
their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`, `shells_seditor.rs`;
`shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs` are the UI lane's kinds of editor classes). The scenario
manager's Rust is `ee_sim::scenario`. The UI lane owns the shells' core (`ee/EEUIShell.h`, `EEUIShell.cpp`,
`shells.rs`, `shells_ffi.rs`) and the toolkit (`ee_ui`, `ee_ui::direct`).

## Where it stands (round 4)

Rounds 3 and 4 work under "Calling C++ from Rust is fine": a host case or a C++ function that was a statement
sequence over C++ objects is composed in Rust of one-statement C++ shims (a host switch's case of one statement, or
a one-statement function), in the C++'s order. The composition sits in the C ABI adapter (`compose(op, args)`
answers the old op from the new statements, `None` for the C++'s own), so the older Rust and its recorded-stream
tests see the same ops. C++ logic lines (`logic_lines.py`, whole repository): 85,861 at round 4's base (effb22404),
82,693 with the round's work: **-3,168** (net C++ lines -1,323). The lane's own `.cpp` files hold about 1,600 logic
lines now (EESMEffect.cpp 198, EEScenarioManager.cpp 197, EESMObject.cpp 142, EESEUnit.cpp 115, EEUIFSETerrain.cpp
98, EESMArea.cpp 98, EEUIFScenarioEditor.cpp 93, EESERBUnitTypes.cpp 81, the rest under 70 each), and its headers
about 900 (mostly the id tables EEScenarioIDs.h and EEScenarioEditorIDs.h).

### The scenario manager (`ee_sim::scenario`)

- **Whole in Rust**: the scenario capture records (`sm_capture.rs`, round 3); `EESMStoreAI` and its field tables
  (`ai_store.rs`; ee/EESMAIFields.h deleted); the trigger's members (`trigger_members.rs`: `EESMTrigger`'s methods
  are one-line forwards over `ee_scenariocpp_trigger`, `EE_SCENARIO_TR_*`); the area tiles' hash in the SMAR
  capture.
- **Composed over one-statement C++**: the effect run's world (`world_compose.rs`, WX 400..636, WC 700..732) and its
  texts, sounds, dialogue, chat, calamity tests, camera key and Change Unit's garrison and kill (`effect_text.rs`,
  WY 810..853); `EESMEffect::Reset`/`Copy` (`effect_copy.rs`); a class attribute's world calls (`ca_compose.rs`, WK
  740..765); the host's reads and the pending population write (`read_compose.rs`, RX 770..801); the object reads,
  `EESMUnitField`, the tile lists (`object_reads.rs`, WZ 900..938); EEEvent.cpp's `Trigger`, `RunEffect` and every
  handler, the pathfinding functor's step and `LoadEvents` (`event.rs`, `EE_SCENARIO_EV_*` 0..131; their EVRN/EVAT
  captures are built with the C++'s bytes in place); the manager's cinematics (`cinematic.rs`, ROUND 20..75) and
  its destructor, pending copies, timed flag, camera step, object change, round capture, the save's element loads
  and dialog message vector, the list host's media and strings and the import errors (`manager_compose.rs`, ROUND
  76..94, IO 22..31, LIST_OP 22..23, TEXT_OP 12..15); `EESMDialogMessage::Load`/`Save`; StreamIO's trigger entries
  (through `EE_SCENARIO_IO_TRIGGER` = 32 and the trigger's members).
- Left in C++ (about 760 logic lines): EESMEffect.cpp (the statement switches' remaining multi-statement cases, the
  effect table), EEScenarioManager.cpp (`Update`'s order, the `Load`/`Save` framing, the list and text hosts'
  remaining sequences), EESMObject.cpp, EESMArea.cpp, EESMMediaStrings.cpp (49, untouched), EEScenarioHost.cpp (45),
  EEEvent.cpp (37, the class shell and its table).

### The scenario editor (`ee_game::screens`)

- **Objects**: the form (`EEUIFScenarioEditor`, a C++ `UIForm` over its Rust object `editor_form.rs`); the tab
  groups (`editor_groups_ffi.rs`); the trigger tab's five sub-screens (`editor_tsubs.rs`); the condition and effect
  screens' control records (`editor_direct.rs`); controls as kinds (`EESEUIBDiplomacy`, `EEUIFSEActionButton`,
  `shells_seditor.rs`).
- **Composed over one-statement C++** (round 4 unless noted): the shared maker (`editor_sepl.rs`, round 3; SEPX
  0..173) and the player tab's civilization, diplomacy, texts, lines and the brush group (`editor_seplx.rs`, SEPX
  200..308); the load thread, the text export and import (`editor_selt.rs`, SELTX); the form's host and `Close`, the
  map, elevation and events hosts, the upgrade panel (`editor_sefmx.rs`, `editor_tabsx.rs`: SEFMX, SEMPX, SEELX,
  SEUPX, SEX, SEEX; calls onto the tabs' Rust objects go direct, `editor_calls.rs`); the trigger tab family: the
  effect and condition screens' `SeetOp`, the object, trigger, area and instructions screens and the trigger tab
  (`editor_trigx.rs`: SEETX, SEOBX 200.., SETRX 300.., SETSX 400.., SEARX 500.., SEINX 600..; TRGX 900.. through
  `ee_ui::direct`); the unit tab (`editor_seunx.rs`, SEUNX) and the terrain tab with its list boxes' draws
  (`editor_setnx.rs`, SETNX, SETLB). `ee_screenscpp_sefm` and `ee_screenscpp_seel` are gone.
- Left in C++ (about 840 logic lines): the hosts' remaining multi-statement cases (EESEUnit.cpp 115,
  EEUIFSETerrain.cpp 98, EEUIFScenarioEditor.cpp 93, EESERBUnitTypes.cpp 81, EESETSEffect.cpp 65, the trigger
  screens 30-56 each), the C++ class shells (constructors that build controls, the vtables), EEUIEOpenCivBuilder.cpp.

## Numbers this lane uses

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf. The hosts' op ranges: `_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..,
  `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., `_setn` 1000..; `_sepx` 0..173 and
  200..308; the statement hosts (`_seltx`, `_sefmx`, `_seunx`, `_setnx`, `_seetx` with `_seobx`.. `_seinx`, `_sempx`,
  `_seelx`, `_seupx`, `_sex`, `_seex`) have their own numbers each.
- The scenario world channel (one number space; `EESMEffectWorld` dispatches by range): `EE_SCENARIO_WORLD_*` 0..198,
  `WX` 400..636, `WC` 700..732, `WK` 740..765, `RX` 770..801, `WY` 810..853, `WZ` 900..938. `EE_SCENARIO_ROUND_*`
  0..17 (round.rs), 20..75 (cinematic.rs), 76..94 (manager_compose.rs). The stream channel `EE_SCENARIO_IO_*` 0..21
  (stream.rs), 22..31 (manager_compose.rs), 32 (the trigger). `EE_SCENARIO_EV_*` 0..131, `EE_SCENARIO_TR_*` 0..46,
  `EE_SCENARIO_CAP_*` 0..171. Parallel forks: give each its range up front (two forks both took IO 22 in round 4).

## Left

- The remaining ~1,600 logic lines above, with the same composition. The biggest pieces: EESMEffect.cpp's switches,
  EEScenarioManager.cpp's `Update`/`Load`/`Save`, the unit and terrain tabs' hosts, the form's constructor.
- **The counter's one-liners**: `case X: stmt; return 0;` on one line counts as logic in `logic_lines.py` (two
  semicolons); the lane's older hosts hold about 150 such lines that are one-statement shims in kind. Not reformatted.

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
  outside EE-SIM's scenario dialogs, and the drawing of every tab past the editor's first frame.
