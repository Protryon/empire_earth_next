# R5b calamities (src/Empire Earth/Calamities)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("Calamities lane, round 1").

**Round 1** (branch `rust-calamities1`, `ee_tech`): before it, R5b.2 had moved every class's `Update`, `Start`,
`Stop`, the cast tests and the contagion update into Rust over host calls; the classes' data, `Initialize`, media,
`Reset`, the per-class run wrappers and the saves were C++ (7,969 lines in the directory).

- **The classes' data as one Rust record, every per-class virtual in the base** (`calamity_class.rs`):
  `EECalamityClass` derives from `EeCalamityClassFields` (the base members, then the twenty subclasses' numbers by
  their C++ names; the area effects' and the leprosy's radius in tiles is `mRadiusTiles`, the teleport's float share
  `mDamageShare`) and holds the effects, sounds and functor (`mGraphicEffect1..4`, `mSound`, `mSecondarySound`,
  `mFunctor`). `Initialize` (each slot's fetch mode, the numbers, the functor), the destructor, `LoadMedia`,
  `UnloadMedia`, `PlaySound`, `Reset`, `Start`, `Stop`, `Update`, `GetContagionFunctor`, `IsValidTarget` and
  `IsValidLocation` are the base's (or the unit/location base's), run by Rust tables keyed by `mClassEnum`. The
  `EECC*` classes keep only their world steps (`StartStep`, `UpdateStep`, their helpers) and the special virtuals
  (`CanCastAt` of the line of sight and special attack, `AffectsOwnUnits`, `Virtual4C`, the volcano's `Place`).
- **An active calamity's record** (`calamity_record.rs`): `EECalamity` derives from `EeCalamityFields` (its 17 plain
  members); `Reset`'s numbers, the out-of-sync checksum, `Load` and `Save` are Rust over one I/O callback.
- **The contagion saves** (`contagion_save.rs`): `EEContagionHelper` derives from `EeContagionHelperFields`; the
  helper's and the manager's `Load`/`Save` and the helper's checksum are Rust over one I/O callback.
- **C++**: net -2,546 this round (the directory 7,969 -> 6,293 lines with the 514-line test capture).
- **Gate**: CALI (every class as `Initialize` left it) and CALK (each active calamity's checksum, save bytes and
  reload, the contagion manager's), recorded from the unmodified C++ (calamity profile, seeds 2-3), with CALT/CALW/
  CONT/CCAS; every class is cast, started, updated and stopped in the calamity profile's game.

**Left** (C++): the world steps of each class (`StartStep`/`UpdateStep` and their helpers: the effects, sounds,
units and orders; by design until the world calls are Rust), `EECalamityManager` (the active list, `Update`, the
creates, the class factory), `EECalamity`'s ctor/dtor/sound setters and out-of-sync dump, the contagion helper's
unit list (`AddUnit`, `RemoveUnit`, `SwapUnit`, `RestorePointers`) and the manager's helper list, the functors
(`EE*Functor.cpp`), `EECalamityUpdate.cpp`/`EECalamityCast.cpp` (the shared hosts).
