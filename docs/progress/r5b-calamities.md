# R5b calamities (src/Empire Earth/Calamities)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("Calamities lane, round 1", "Calamities lane, round 2").

**Rust** (`ee_tech`): every class's `Start`, `Stop` and `Update` decisions, the cast tests and the contagion update
(R5b.2); the classes' data as one record with every per-class virtual as a table by `mClassEnum`, the base each
kind is made as (`calamity_class.rs`); an active calamity's record, `Reset`, checksum, `Load` and `Save`
(`calamity_record.rs`); the contagion helpers' record and saves (`contagion_save.rs`); the contagion functors as one
record, their getters, `Update`'s discrete time and `Apply`'s hit (`contagion_functor.rs`).

**C++ left** (5,000 lines in the directory with the 439-line test capture; 7,969 before round 1):
- `EECalamityUpdate.cpp` (1,350): the host every Rust run asks, one switch over `EeCalamityOp`: the shared calls and
  every class's own world steps (the effects, sounds, units, orders, the camera shake), keyed by the class enum for
  the start steps (`ClassStep`). This is where the twenty `EECC*` classes' bodies live now; they move further only as
  the world calls they make (effect manager, units, goals, map) become Rust.
- The three bases (`EEGlobalCalamityClass`, `EELocationCalamityClass`, `EEUnitCalamityClass`) and `EECalamityClass`
  (379): the C++ object glue around the Rust tables (effects, sounds and functor pointers, media load/unload,
  `PlaySound`, `UpdateSound`, the volcano's `Place`).
- `EECalamityManager` (410): the active list and the class list (`std::vector`s of C++ objects), `Update`'s loop,
  the creates, `Restore`, the capture hooks.
- `EECalamity` (365): the constructor/destructor (references, media), the sound setters, the out-of-sync dump, the
  `Load`/`Save` I/O callback.
- `EEContagionHelper` (402) and `EEContagionManager` (412): the helper's unit list (`std::vector<EEUnit*>`,
  referenced), the manager's helper list, the helper's update host, the functor's world steps (`Infect`, `Cure`,
  `Apply` by kind).
- `EECalamityCast.cpp` (216): the cast tests' host.

**Next**: the unit lists as ids (the helper's units, the calamity's target and caster) once a pending unit (no id
yet: a conversion's new unit in `SwapUnit`) can be held by id; the manager's lists in a Rust store once the
calamities are Rust objects; the world steps as the effect manager's and the units' calls become Rust.

**Gates**: the calamity profile's CALI, CALK (each calamity at its start, every five seconds of its life and every
five game minutes), CALT, CALW, CONT, CCAS, CALS, CALU records at seeds 2-3, recorded from the unmodified C++; the
oracle; the saveload matrix (`oracle,calamity`). EE-SIM draws and plays nothing: the effects, sounds, camera shake and
media loads are tour-only paths (no fixture command casts a calamity yet).
