# Lane A: the Lua AI host (EECPNextUnits, EECPNextLua, EECPNextView, ...) in Rust

Lane state (one file per lane). Branch `rust-ai-lua1`, from `origin/rust` 0bfe280ce. The lane moves the AI Thread's
Lua host (about 10k lines: EECPNextUnits.cpp, EECPNextLua.cpp, EECPNextView.cpp, EECPNextArmy.cpp, EECPNext.cpp,
EECPLua.cpp, EECPEngine*.cpp, EECPReflexes.cpp, EECPTrace.cpp, EECPCombatManager.cpp) into `ee_sim::ai`. The Lua
scripts (data/ai) do not change, and must see the same values in the same order.

## The golden

- `EE_AI_BINDING_TRACE` (TEST ONLY, `ee_sim::ai::binding_trace`): every `ai.*` binding call writes the FNV-1a 64 of
  its arguments' and its results' text (`EE-AIB > name a=...`, `EE-AIB < name n=.. r=...`; `full` adds the text),
  every strategy step the hash of its view and its orders (`EE-AIB step`/`init`). The text is read with raw
  `lua_next`, `lua_tointegerx`/`lua_tonumberx` and `lua_tolstring` on strings only: no metamethod, conversion or
  allocation in the state. Off, the registration is the same closures in the same order.
- `tools/ai_binding_golden.py run NAME` plays 15 seeded EE-SIM games (classic_lua seeds 2-3, ageofai 2-3, boom,
  ageofai+classic_lua late and space starts, a Large Islands map, the tests/ai_units scripts helpers, commands,
  types, raid, airwing, kite, cover) with the trace on and keeps the EE-AIB and EE-AI lines gzipped
  (build/ai_golden/NAME); `compare BASE NAME` names the first differing line per game. About 6 minutes, `--jobs 2`.
- `build/ai_golden/base` was recorded on the unmodified C++ (8c4258aa7, the trace only), twice, equal; the traced
  oracle seeds 2-3 equal `ref` (the trace changes nothing). 90 bindings; the games call 77, `tests/ai_units/cover.lua`
  the rest.
- Mutation: `sight` read from the range (push_record) is caught at the 59th line of classic-2 (the first `ai.unit`).

## What is where

- `rust/crates/ee_sim/src/ai/units_state.rs`: `UnitsState` (`EECPUnitsState`'s body): held set, events, swap
  forwarding, enemy memory, reflexes, per-step limits, the threat/lost/influence layers, the route's A*, the EENU
  block. Unit tests.
- `rust/crates/ee_sim/src/ai/units_bind.rs` (game only): the state's C ABI, the hooks (damage, dealt, created,
  built, idle, attack/defend player, flare, taken, swap), the memory and layers' rebuild every game second, and the
  bindings: me, time, players, map, units, unit, get, seen, memory, resources, tile, events, reach, strength, take,
  release, reflex, danger, danger_box, mark, route, path_len, config, gatherers; the commands move, attack,
  attack_ground, gather, build, repair, garrison, load, unload, train, research, stance, stop, explore, delete, heal,
  rally, cancel, formation, pack, dropoff, replant, farm, cast, wall, patrol, lock; the searches drop_site,
  enemy_force, player_file, test_town, find_site, shore. They read the world through
  `ee_sim::world`'s views; `ee_ai_units_host` (EECPNextUnits.cpp) answers the rest (`EE_AI_UNITS_HOST_*`: the unit
  lookup, a goal's facts, the continents, the names, the trace, the selection and a message's run, the tech tree's
  few tests).
- C++ left in EECPNextUnits.cpp: the host, `EECPUnitsState` as a shell (the Rust state's pointer, the scratch of
  the C++ bindings, the wall routes' path requests), the micro step's loop (it calls `EECPLuaState`), and the
  bindings over the continent manager's areas, the path manager, the tech tree and the classic tuning: wonders,
  class, can_hurt, can_place, test_spawn, water_sites, enemy_near, garrison_fill, area, forest_point, hill_point, wall_path/route/route_result/line/
  stretches, choke_point, area_point, unexplored, used, improve, improve_cost, gate, tribute, trade, diplomacy,
  tuning, techs, place, site_blocked, build_unsafe, epoch.

## Not done (next)

- The bindings above (unexplored and area need only views and the continent ops; techs, epoch, class and wonders
  a tech-node record op; tuning a table of the computer player's behaviour values).
- EECPNextLua.cpp (the view's Lua table, the orders' run), EECPNextView.cpp, EECPNextArmy.cpp, EECPTrace.cpp,
  EECPReflexes.cpp, EECPEngine*.cpp, EECPCombatManager.cpp, EECPNext.cpp, EECPLua.cpp: untouched this round.
- mlua (the owner's request of this round): not switched, see PORT_LEARNINGS "Lane A, round 1".
