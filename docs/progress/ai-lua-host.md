# Lane A: the Lua AI host (EECPNext*, EECPReflexes, EECPTrace, EECPLua, ...) in Rust

Lane state (one file per lane). Round 3: branch `rust-ai-lua3`, from `origin/rust` 4929244f3 (rounds 1 and 2 merged).
The lane moves the AI Thread's Lua host into `ee_sim::ai`. The Lua scripts (data/ai) do not change, and must see the
same values in the same order. The Lua C library stays vendored (src/ThirdParty/lua, the owner's decision): Rust
calls its C API (lua_api.rs).

## The golden

- `EE_AI_BINDING_TRACE` (TEST ONLY, `ee_sim::ai::binding_trace`): every `ai.*` binding call writes the FNV-1a 64 of
  its arguments' and its results' text, every strategy step the hash of its view and its orders.
- `tools/ai_binding_golden.py run NAME` plays 15 seeded EE-SIM games with the trace on (build/ai_golden/NAME);
  `compare BASE NAME` names the first differing line per game. About 8 minutes, `--jobs 2`.
- Round 3's base is `build/ai_golden/r3a`, recorded on 4929244f3 (the round's start) twice, equal; the merge of
  origin/rust ec39733e7 (lane B round 2) left all 15 games identical to it. Main merge 4 (classic_lua changes) had not
  reached origin/rust by the end of the round: after it, re-record the base (two equal runs) and check the oracle
  against the new ref.
- The reflexes are off in every shipped AI (`ai.reflex{militia = false, danger = false}`), so the golden exercises
  only the naval flight. Two scratch copies of data/ai (EE_AI_SCRIPTS) leave them on (`ai.reflex{held = true}`; and
  `{militia = false, held = true}` with EE_AI_EXP=flee): classic-2 and space-2 on the old and the new EE-SIM gave
  identical traces with 31 militia, 19 flee, 10 raid.defend and 55 navalflee lines.

## What is where

- `rust/crates/ee_sim/src/ai/next_state.rs`: `NextState`, the per-player record (EECPNextState before): the epoch and
  income bookkeeping, the scouting memory (armies, towns, gather spots, towers), the threat memory, the events, the
  home resources, the forests' order, the Lua host (owned: freed with the record), the AI id and script, the order
  results, the TEST ONLY shadow; its pure logic with unit tests; `to_save`/`from_save` over next_save.rs.
- `next_value.rs`: the view (`View`, EECPNextView before), its Lua table (`to_value`, EECPNextViewValue), the
  orders' check (`run_orders`: every order is refused now), `one_line`; unit tests.
- `next_game.rs` (game only): the host `ee_ai_next_host` (EECPNext.cpp, ops `EE_AI_- EECPEngine.cpp: the manifest's entries are C++ (`EECPAIEntry`, read by the lobby, EESim and EEScreens); the folder
  hash (PlatFind), the EE_AI_ENGINE override and the seats could follow, with a host for the listing.
- EECPEngineHelpers.cpp: members of game classes (EEComplexUnit, EETile, EEUnitClass, ...): they belong to the
  objects' lanes, not this host's.
- EECPTrace.cpp's personality switches (EECPMilitiaOn ... EECPStoneSpan) read the C++ rules (`EEAIPersonalityRule`):
  they move with the rules.
- lua_ffi.rs: the C ABI of the Lua runtime (`ee_ai_lua_*`, the C++ tree ops) has no C++ caller now; delete it with
  `lua_host`'s `JobValue::Cpp` path.
- The units host (EECPNextUnits.cpp ops 1-93) and the next host (EECPNext.cpp) stay while the objects they reach
  are C++ (lanes B, C)._HOST_*`), the world reads,
  the gather safety (site threat, route tower, build unsafe, site blocked, assigned, has room, drops), the forests,
  the army's scan and view, the view build, the losses trace (next.death, next.raid), the strategy (open, prepare,
  init/step, shadow, errors), the tick after the bookkeeping, the save/load/check, the micro step.
- `next_reflex.rs` (a child of next_game): EECPRMNoteDanger, the militia, the naval flight, the flight from the danger
  zones (EECPReflexes.cpp before).
- `next_trace.rs` (a child of next_game): the stats lines, the losses' counters and unit.death, income and spend,
  the profile, the sim hash line, the out-of-synch checksum and its dump (EECPTrace.cpp's before). **The continent
  manager's `mAreas` is read in one host op, `EE_AI_- EECPEngine.cpp: the manifest's entries are C++ (`EECPAIEntry`, read by the lobby, EESim and EEScreens); the folder
  hash (PlatFind), the EE_AI_ENGINE override and the seats could follow, with a host for the listing.
- EECPEngineHelpers.cpp: members of game classes (EEComplexUnit, EETile, EEUnitClass, ...): they belong to the
  objects' lanes, not this host's.
- EECPTrace.cpp's personality switches (EECPMilitiaOn ... EECPStoneSpan) read the C++ rules (`EEAIPersonalityRule`):
  they move with the rules.
- lua_ffi.rs: the C ABI of the Lua runtime (`ee_ai_lua_*`, the C++ tree ops) has no C++ caller now; delete it with
  `lua_host`'s `JobValue::Cpp` path.
- The units host (EECPNextUnits.cpp ops 1-93) and the next host (EECPNext.cpp) stay while the objects they reach
  are C++ (lanes B, C)._HOST_AREAS`** (the dump's AREAS line): Lane C re-points it.
- `units_bind.rs`: the `ai` table's registration (`ee_ai_units_open`, the traced wrappers as `traced::<K>`), the
  per-unit state's save and load (`ee_ai_units_save`, `ee_ai_units_load`).
- The host's reads of other lanes' objects: the placement's town tile, the resource centres (`RC*`), the danger
  zones (`DANGER*`), a goal's target (`GOAL_UNIT`), the continent of a unit, a player's nearest area's hull point
  (`AREA_HULL`), the forests (`FOREST*`), the tech nodes' cost and tests, the population counts.

## C++ left

- EECPNext.cpp: the computer player's bookkeeping before the tick, the names (EEDbRowIds), the shims, the host.
- EECPNextUnits.cpp: the events' shims, the units host (ops 1-93) and the wall routes' path requests.
- EECPTrace.cpp: the trace switches and EECPTraceOut (variadic), the personality switches, the shims.
- EECPEngine.cpp: the manifest's C++ entries, the folder hash, the seats. EECPEngineHelpers.cpp: game classes'
  members (not the host's).

## Not done (next)

- EECPEngine.cpp: the manifest's entries are C++ (`EECPAIEntry`, read by the lobby, EESim and EEScreens); the folder
  hash (PlatFind), the EE_AI_ENGINE override and the seats could follow, with a host for the listing.
- EECPEngineHelpers.cpp: members of game classes (EEComplexUnit, EETile, EEUnitClass, ...): they belong to the
  objects' lanes, not this host's.
- EECPTrace.cpp's personality switches (EECPMilitiaOn ... EECPStoneSpan) read the C++ rules (`EEAIPersonalityRule`):
  they move with the rules.
- lua_ffi.rs: the C ABI of the Lua runtime (`ee_ai_lua_*`, the C++ tree ops) has no C++ caller now; delete it with
  `lua_host`'s `JobValue::Cpp` path.
- The units host (EECPNextUnits.cpp ops 1-93) and the next host (EECPNext.cpp) stay while the objects they reach
  are C++ (lanes B, C).

## Results (round 3)

Commits (net C++ in src and include, `git diff --numstat`):
- ad663fe78 the record, the tick, the view, the army, the strategy, the save, the micro step, the registration:
  -2,640 (EECPNextLua.cpp, EECPNextView.cpp, EECPNextArmy.cpp deleted);
- abe28622a the reflexes: -386 (EECPReflexes.cpp deleted);
- ab9b6b945 the rest of EECPTrace.cpp: -371;
- fe5aa3156 EECPLua.cpp and EECPLua.h deleted: -629 (and -482 lines of tests/platform/lua_test.cpp, luatest);
- 2a5cb5298 the per-unit state's save: -25;
- the merge of origin/rust ec39733e7 (tools/units.py's lists).
- Total -4,051 C++ (src, include), -4,533 with the test.

Checks:
- Binding golden (base r3a) identical in all 15 games after each commit and after the merge; a mutation (the view's
  citizens counted twice) caught at classic-2 line 4 (the first next.view line).
- Oracle seeds 2-3 identical after each commit; on the merged tree seeds 2-7 identical.
- Captures oracle (208 digests), late (210), space (206), calamity (218), seeds 2-3: PASS.
- Saveload plain 18/18 PASS; the matrix (`--settings all --seeds 2-3 --at 1000,1900 --expect`) PASS.
- Both builds 0/0 (and the test profile 0/0); the Rust gate PASS (4,515 tests).
- gates.sh through its slot: GATES PASS (tour; ai-game time-cap, units [4, 40, 40], alive [2, 3], classic_lua x2;
  lan-game).
