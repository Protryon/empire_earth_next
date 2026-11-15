# R5b physics models

The lane's state (the physics models: `src/Empire Earth/Physics Thread/EEPhysicsModel.cpp` and the twelve `EEPM*`
models, `EEPPatrolGroup`). Each round rewrites this file to the current state; the rounds' stories go in
PORT_LEARNINGS.md ("The physics models, round 1", "round 2").

**In Rust (ee_units):** the twelve models whole (round 1: pm.rs, pm_ground.rs, pm_air.rs, pm_host.rs; the vtables'
dispatch by kind, the base model, the turns, speed and pitch modes); the carrier planes' deck walk (round 2:
pm_air.rs `docked`, `follow_carrier`, `orient_on_carrier`; the carrier model's tag points through one host call);
`EEPPatrolGroup`'s bodies and its id counter (round 2: patrol_group.rs; the C++ class keeps its fields, which its
holders read, and forwards every member).

**Stays C++:** `EEPhysicsModel` (the kind and the forwarders `Step`, `Step0`, `ComputeRoll`) and the host the Rust
calls; the projectile's and missile's steps (ee_combat's flight arithmetic; the combat lane's CBPJ/CBPS/CBMA/CBMH
captures and the drop target); the unit's group-aware rates (`EEComplexUnit::GetAcceleration` and kin: one branch
on `mActiveGroup`, whose rates are `EEActiveGroup`'s) and `IsSwimming` (C++ callers besides the models' host).

**Gates:** PMST (every model step's unit state, folded per tick) recorded on the unmodified C++ models in the
oracle, late, space and calamity profiles (seeds 2-3, round 1) and the units profile (seeds 2-7, round 2); the sim
oracle; the saveload matrix.

**Coverage (EE-PM-COUNT, steps over a game's first 24,000 ticks).** The units profile (round 2: EE-SIM
`--unit-sweep`, Physics Thread/EEPhysicsSweep.cpp; Continental on even seeds, Planets Small on odd ones) steps the
airplane (620k-845k a game), helicopter (450k-1.28 million), submarine (65k-120k, water maps), sea animal
(86k-330k, water maps), spaceship (210k-230k, space maps) and missile (22k-38k, water maps) models and the deck walk.
The other profiles step land, building, naval, projectile and air animal units, helicopters only in the calamity
game. No class uses the hovercraft model (no constructor passes `gPhysicsModels[3]`); its body is the land model's.

**Next:** the group-aware rates and `IsSwimming` together with their C++ callers (the units' code of the core lane);
the projectile and missile steps' input gathering if the combat lane hands over its captures.
