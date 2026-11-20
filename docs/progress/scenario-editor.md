# The scenario editor lane

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The scenario editor lane, round 1", "round 2", "round 3").

**Scope** (agent rules "Lane scopes"): `src/Empire Earth/Scenario Editor` and `Scenario Manager` (the triggers),
their `include/ee` headers, and the editor's Rust in `ee_game` screens (`editor_*.rs`, `shells_seditor.rs`;
`shells_editor.rs`, `shells_unit.rs`, `shells_tech.rs` are the UI lane's kinds of editor classes). The scenario
manager's Rust is `ee_sim::scenario`. The UI lane owns the shells' core (`ee/EEUIShell.h`, `EEUIShell.cpp`,
`shells.rs`, `shells_ffi.rs`) and the toolkit (`ee_ui`, `ee_ui::direct`).

## Where it stands (round 3)

Round 3 is the first under "Calling C++ from Rust is fine": a host case or a C++ function that was a statement
sequence over C++ objects is now composed in Rust of one-statement C++ shims (a host switch's case of one statement,
or a one-statement function), in the C++'s order. C++ logic lines (`logic_lines.py`): 91,787 at the round's base
(840aeb3c0), 90,435 after it: **-1,352** (the target was -3,000).

- **The scenario capture records** (TEST ONLY): whole in Rust (`ee_sim::scenario::sm_capture`, the game entry
  points `sm_capture_game.rs`); EEScenarioCapture.cpp is the hooks' forwards and `ee_scenariocpp_capture` /
  `_capture_f`, the reads (`EE_SCENARIO_CAP_*`).
- **The effect run's world** (`ee_sim::scenario::world_compose`): every multi-statement case of `EESMEffectWorld` and
  `SMEUnitsWorld` but the texts' (PLAY_SOUND, CHANGE_TEXT, DIALOGUE, CHAT_LOG, CHAT_BOX), CAL_TYPE, CAMERA_KEY,
  ADD_PENDING_PLAYER, FLARE, AREA_CENTER and GOAL_LOAD_TILE, and `EESMCreateUnit`, `SMETakeOffMap`,
  `SMEPlaceInArea`, `SMETechAdd`, `SMEClassValue`/`SMENotLand`; the C++ is `SMEAtomWorld` (`EE_SCENARIO_WX_*`, on the
  run's context, which holds the objects a sequence builds) and `SMECreateWorld` (`EE_SCENARIO_WC_*`, on a plain
  host context). `EeScenarioHost::world` and `::write` answer the composed ops before the C++; effect.rs, task.rs and
  their tests see the same ops as before.
- **The manager's cinematics** (`ee_sim::scenario::cinematic`): `EndCinematic`, `StartCinematic`, `ExitCinematic`,
  the round's shown message, hidden messages and human test, `ApplyPendingEffects`, the media unloads of `Prepare`
  and the destructor, `ReplaceUnit`; the C++ members forward with `EESMRoundHost(this)`; `RoundOp` holds the
  statements (`EE_SCENARIO_ROUND_*` 20..75).
- **The shared maker** (`ee_game::screens::editor_sepl`): `SeplMake` (every control kind, its settings and groups),
  `SeplWorld`, `SeplPlayer`, `SeplAIFlag`, `SeplTechTypes`, `SeplFillTechs` and the SEPL host's event, screen event,
  player count, tech rebuild, unit limit, colour, mini map and load thread cases; the C++ is `ee_screenscpp_sepx`
  (`EE_SCREENS_SEPX_*`) in EESERBUnitTypes.cpp. `editor_direct::sepl` answers them after `sepl_direct`.

## The editor's objects

- **The form** (`EEUIFScenarioEditor`, a `UIForm`): a C++ class over its Rust object (`editor_form.rs`).
- **The tab groups** (round 1): Rust objects (`editor_groups_ffi.rs`, `ee_screens_segr_*`).
- **The trigger tab's five sub-screens** (round 1): one Rust record in the trigger tab (`editor_tsubs.rs`).
- **The condition and effect screens' control records** (round 2): Rust allocations (`editor_direct.rs`).
- **Controls as kinds**: `EESEUIBDiplomacy`, `EEUIFSEActionButton` (`shells_seditor.rs`) and the UI lane's.

## Numbers this lane uses

- Shell kinds `EE_SCREENS_SK_*` 0x70-0x7f; kind ops `EE_SCREENS_SKOP_*` 0x70-0x7f; shell host ops `EE_SCREENS_SH_SE_*`
  0xa0-0xaf. The hosts' op ranges: `_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..,
  `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., `_setn` 1000..; `_sepx` its own
  (0..173); `_seet`/`_sect` and `_see` have their own numbers.
- The scenario world: `EE_SCENARIO_WORLD_*` 0..198 (the composed and the C++'s), `EE_SCENARIO_WX_*` 400..636 (the
  run's statements), `EE_SCENARIO_WC_*` 700..732 (Create Units' unit); `EE_SCENARIO_ROUND_*` 0..17 (round.rs), 20..75
  (cinematic.rs); `EE_SCENARIO_CAP_*` 0..171.

## Left

- **Scenario Manager** (about 1,930 logic lines): EEEvent.cpp's handlers (506: the tech events, every game; their
  EVAT/EVRN captures stay in place), EESMObject.cpp's reads (293), EEScenarioManager.cpp (the dialog message's
  Load/Save, the list and text hosts), EESMEffect.cpp's text cases, `SMEClassAttributeWorld`, `SMEChangeUnitWorld`,
  `EESMStoreAI`, EEScenarioHost.cpp's multi-statement reads and StreamIO entries.
- **Scenario Editor** (about 3,700 logic lines): the form's host (`EEUIFScenarioEditor::Host`, 398: its constructor
  sequences), the unit tab's host (`ee_screenscpp_seun`, 261), the effect screens' `SeetOp` (192), the terrain
  tab's host (164), the events' host (134), the object, area, trigger, map, instructions hosts, the upgrade panel,
  the load thread, the import/export actions. The same composition (a statement host per C++ file, the sequences in
  the FFI adapters) applies to each.
- **The counter's one-liners**: `case X: stmt; return 0;` on one line counts as logic in `logic_lines.py` (two
  semicolons); this lane's hosts hold about 150 such lines that are one-statement shims in kind. Not reformatted.

## Checks

- Per step: the build `--sim` 0/0, the editor gate (`tools/sim_editor.py`, PASS 14,448), the civilization gate
  (`--civbuilder`, PASS 4,062), `scenario_check.py` 27/27, the oracle seeds 2-3 against `ref`, the scenario capture
  profile at seeds 2-3 (`build/se3/step.sh TAG` in the round-3 worktree).
- Round end: both builds, the Rust gate, the oracle 2-7, the 12 capture profiles at seeds 2-3, the plain saveload and
  the matrix, the editor tour (`fixture_game.py --exercise editor` through slot.py: compare its `editor.bmp` with the
  base build's outside the game view and the mini map), eedata check and cppgate, `gates.sh`.
