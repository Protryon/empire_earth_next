# R5b calamities (src/Empire Earth/Calamities, and the area effects of Civilization Builder)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("Calamities lane, round 1", "round 2", "round 3").

**Rust** (`ee_tech`): every calamity class's `Start`, `Stop` and `Update` decisions, the cast tests and the contagion
update (R5b.2); the classes' data as one record with every per-class virtual as a table by `mClassEnum`, the base each
kind is made as (`calamity_class.rs`); an active calamity's record, `Reset`, checksum, `Load` and `Save`
(`calamity_record.rs`); the contagion helpers' record and saves (`contagion_save.rs`); the contagion functors as one
record, their getters, `Update`'s discrete time and `Apply`'s hit (`contagion_functor.rs`). Since round 3 the area
effects: the eight kinds (healing, morale, the two immunities, morale from buildings, from military heroes, economy
from units, cloaking) as one record `EeAreaEffectFields` keyed by the template's kind, every slot's body over a host
of world calls, the template's numbers, constructor, `Init`, player and area tests (`area_effect.rs`); the running
state saved since 0.256, its record and bytes (`area_effect_save.rs`).

**C++ left** (Calamities: 4,172 lines with the 439-line test capture):
- `EECalamityUpdate.cpp` (1,350): the host every Rust run asks, one switch over `EeCalamityOp`; the twenty classes'
  world steps (effects, sounds, units, orders, the camera shake).
- The three bases and `EECalamityClass` (379), `EECalamityManager` (410), `EECalamity` (365), `EEContagionHelper`
  (402), `EEContagionManager` (412), `EECalamityCast.cpp` (216).
- Area effects (Civilization Builder; 1,617 lines with the 208-line test capture, 2,067 before round 3):
  `EEAreaEffect.cpp` (the one class's slots as calls into Rust, the host switch `EEAEHost`: unit sets, tech nodes,
  player relations, positions, continents, hit points, the cloaking's world steps, the buildings' count; the
  template's ctor/dtor/Init/tests), `EEAreaEffectTemplate.cpp` (the template's media, the unit's list
  `EEUnitAreaEffectList`), `EEAEManager.cpp` (the tick's unit functor passes, `CollectEffects`, `FindEffect`, the
  templates' load, the running state's walk over the units and its after-load fix-up).

**Next**: the unit lists as ids (the contagion helper's units, the calamity's target and caster; the area effects'
owner unit and the units' two lists) once a pending unit (no id yet: a conversion's new unit in `SwapUnit`) can be
held by id; the manager's lists in a Rust store; the world steps as the effect manager's and the units' calls become
Rust.

**Gates**: the calamity profile's CALI, CALK, CALT, CALW, CONT, CCAS, CALS, CALU records at seeds 2-3; AETP and AEFX
(the area effects: every template, every unit's own and affecting effects, their state, modifiers and Covers, and
the running state's save bytes after every area-effect tick) in the oracle, late, space and calamity profiles at
seeds 2-3, recorded from the unmodified C++; the oracle; the saveload matrix (`oracle,calamity`).
**Tour**: `fixture_game.py --exercise random-game --cast earthquake,hurricane,meteor,volcano` casts each calamity at
the camera's focus through the fixture's cast command (EETestFixture.cpp, TEST ONLY) and captures it.
