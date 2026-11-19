# The Graphics + LLE cluster (the Graphics Thread, the Data Pre-Loading Thread, the rest of the LLE, the RTS Engine)

The lane's state; each round rewrites it. The rounds' stories are in PORT_LEARNINGS.md ("The Graphics + LLE cluster,
round 1", "... round 2"). Earlier lanes over the same code: [obj-lane-r.md](obj-lane-r.md) (the fill and the effect
readers over the world views), [lle-rest.md](lle-rest.md), [r4-4-graphics-thread-27-8k.md](r4-4-graphics-thread-27-8k.md),
[r4-3-graphics-engine-17k.md](r4-3-graphics-engine-17k.md).

## Round 2 (branch `rust-gfx-cluster2`, 2026-11-18)

| Commit | What | Net C++ (src, include/ee, include/lle) |
|---|---|---|
| 59197e216 | Other lanes' readers of an effect through accessors (`GetKind`, `GetHandle`, `SetLocalTo`): EEMessage, EEARust, EEWonNetAvailableGameFormManager, EEContagionManager, EEEMStorm | -5 |
| 5d0bfcdd5 | `EEEffect` and `EEEffectManager` as Rust objects (ee_gfx effect_obj.rs, ffi_manager.rs, effect_sweep.rs): lifetime, Load/Save/Repair, regions, render queues, 2D parts, base queues, the manager, the goal markers' effects, EFMU/EFST/EFSW; EEEffect.cpp, EEEffectManager.cpp, EEGraphicalGoalManager.cpp, EEGraphicalGoalManager.h, EEEffectsTable.h deleted | -1,794 |
| 717dbabe1 | The effects' constructors in Rust (effect_ctor.rs) | -313 |
| 42acd7d89 | The tracers' hits, the rain's media, the particles' tick and ground, the emitter's place, the wind in Rust | -104 |

Round total: **-2,216** net C++ lines (target 4,000: see "What is left"). Rust added: effect_obj.rs (+10 tests),
ffi_manager.rs rewritten (+2 tests), effect_sweep.rs, effect_ctor.rs (+1 test); ffi_effect.rs and ffi_mesh.rs lost
the C ABI wrappers of the old C++ effect code.

Other lanes' files edited: the five readers above (their own commit), EEMap.h/.cpp (the regions' type, their new and
free: in 5d0bfcdd5, coupled to the change).

## What is left, and why

- **EEEffectHost.cpp** (about 1,000 lines): the C++ answers that call other lanes' C++ (the volcano's surface and
  model build, the farm's drop sites, doppelgangers and scaled model, the wall's posts and pieces, the confirmation's
  goals, the area effects, the icons' selection, the sounds, the map's ground and glow area), the goal markers'
  questions, and the two registered worlds. Next: the answers whose reads have world views (glow area and rows over
  the terrain mesh, the area effect count), the sound answers once the sound objects are Rust.
- **EEGraphicObject.cpp / EEGraphicEffect.cpp** (not reached): LoadMedia/UnloadMedia/CollectParts over the model and
  texture managers (the shared-frames `std::map` can become the slice both consumers already convert it to), the
  media-by-kind table and GetMedia's random draw.
- **EERenderObject.cpp**: the fill's hosts over the layout (golden_fill_replay's fakes first).
- **RTSMap Create, the line picks, RTSGraphics/RTSMouse/gViewport, GEModel's host, GEValues** (not reached): mostly
  shells over GE objects that are C++ subclasses (rasterizer, surfaces) or one-line forwarders.

## Gates (round 2)

- Per commit: both builds 0/0, the oracle seeds 2-3 identical, the effects capture (seeds 2-3; the EFSW sweep makes
  every kind through every entry point), the oracle and missile captures (seeds 2-3), the Rust gate. The first Rust
  version differed in one EFSW record (a tracer beam's saved target zero-extended: PORT_LEARNINGS); found by diffing
  the effects capture against the lead's EE-SIM record by record, then byte-identical to it.
- On the merge of origin/rust c9d7de90c (clean; strip_markers, the headers and rust_includes had nothing to do): both
  builds 0/0 (EE-AOC 620 inputs, EE-SIM 621), the test-profile build 0/0, the Rust gate PASS, the oracle seeds 2-7
  identical, the captures oracle, late, effects and missile (seeds 2-3) PASS, the editor gate PASS, scenario_check
  27/27, `bash tools/gates.sh` GATES PASS (tour, ai-game time-cap with both AIs alive on classic_lua, lan-game).
- Tours (test profile) against the round's start exe, seed 7: the 1/20-clock tour's RFIL 326 matched frames, all
  equal; the box-select/orders tour at the normal clock: both pass, RFIL 52 matched, 10 differ, all after the tour
  kills a unit at a wall time (tick 6638 on), as round 1 saw for this tour; captures looked at (player colours,
  health bars, selection rings, the HUD), no GX errors.
