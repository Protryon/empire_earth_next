# R5b physics models

The lane's state (the physics models: `src/Empire Earth/Physics Thread/EEPhysicsModel.cpp` and the twelve `EEPM*`
models, `EEPPatrolGroup`). Each round rewrites this file to the current state; the round's story goes in
PORT_LEARNINGS.md ("The physics models, round 1").

**Round 1 (branch `rust-physics1`, 2026-11-14): the twelve models whole in Rust.** `ee_units` pm.rs (the kinds, the
virtual dispatch as each vtable chose it, the base model: `Step`, `Step3`, `AfterStep`, `Collide`, `Push`,
`PushMoving`, `Move`, `ClipStep`, the turns, speed and pitch modes with their USST notes), pm_ground.rs (`EEPMLand`
and `EEPMHovercraft`, `EEPMNaval`, `EEPMSubmergedNaval`, `EEPMSeaAnimal`, `EEPMSpaceship`, `EEPMBuilding`'s
collapse), pm_air.rs (`EEPMHelicopter`, `EEPMAirplane`, `EEPMAirAnimal`), pm_host.rs (the C++ calls). The unit's
plain members are read and written in place in its records (`EeUnitsComplexUnitFields` and the two others, the C++
object's bases); everything else is a host call made where the C++ made it (EEPhysicsModel.cpp
`ee_units_pm_host_*`). The twelve EEPM*.cpp files are deleted; `EEPhysicsModel` is one class with its kind and three
forwarders (`Step`, `Step0` for EE-SIM's missile sweep, `ComputeRoll` for the airplane classes' death roll).

Stays C++: the projectile's and missile's steps (their arithmetic is ee_combat's flight.rs already; the combat lane's
CBPJ/CBPS/CBMA/CBMH captures and the drop target live there), the carrier planes' deck walk (`IsDocked`,
`FollowCarrier`, `OrientOnCarrier`: the carrier model's tag points), the unit's group-aware rates
(`EEComplexUnit::GetAcceleration` and kin, `IsSwimming`), `EEPPatrolGroup`.

Gate: PMST (every model step's unit state after it, folded per tick) recorded on the unmodified C++ in the oracle,
late, space and calamity profiles at seeds 2-3; the USST notes of the arithmetic leaves keep their words; the sim
oracle (seeds 2-7 identical); the saveload matrix. C++ net -4,200 lines this round.

Coverage (EE-PM-COUNT): the gate games step land, building, naval, projectile and air animal units in every profile,
helicopters only in the calamity profile, and no airplane, hovercraft, submarine, sea animal or spaceship; those
bodies are checked by unit tests and review only.

**Next:** a golden for the unreached models (a scratch driver over the original C++ of 56802c88^, or a capture
profile with aircraft, submarines and spaceships); `EEPPatrolGroup` (a reference-counted list of patrol point units with its save; its callers are the patrol
goal, `EEMAddPatrolPoint`, `EETown` and the player); the deck walk once the model's tag points are readable from Rust
(R4.3's `GEModel` records); the projectile and missile steps' input gathering if the combat lane hands over its
captures.
