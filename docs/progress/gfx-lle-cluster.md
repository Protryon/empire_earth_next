# The Graphics + LLE cluster (the Graphics Thread, the Data Pre-Loading Thread, the rest of the LLE, the RTS Engine)

The lane's state; each round rewrites it. The round's story is in PORT_LEARNINGS.md ("The Graphics + LLE cluster,
round 1"). Earlier lanes over the same code: [obj-lane-r.md](obj-lane-r.md) (the fill and the effect readers over the
world views), [lle-rest.md](lle-rest.md), [r4-4-graphics-thread-27-8k.md](r4-4-graphics-thread-27-8k.md),
[r4-3-graphics-engine-17k.md](r4-3-graphics-engine-17k.md).

## Round 1 (branch `rust-gfx-cluster1`, 2026-11-18)

| Commit | What | Net C++ (src, include/ee, include/lle) |
|---|---|---|
| f39774579 | The graphic objects' tag points whole in Rust (ee_gfx tags.rs): the lists by kind and model, the weapon mounts, colour groups, attack points, CollectTagPoints, GetTagPoints, PickPoint, SetGroupColor, the points' geometry; `EETagPointList`/`EETagPoint` views of the Rust layouts | -182 |
| 27ee59022 | 35 effect answers in Rust over a render-side layout (ee_gfx effect_reads.rs, `EeGfxObjectLayout`, `EeGfxEffectRefs`) | -424 |
| 2993bb6d9 | RTSMap's terrain edits whole in Rust (ee_ge map_raise.rs): Raise, the spreads, SmoothVertex, UpdateTiles, GetNumRaisedCorners, IsTileFlat | -300 |
| 909fe4ebb | RTSMap's continent outlines in Rust (BuildAreaOutlines, AddOutlineEdge) | -99 |
| 9d873cb15 | 14 more effect answers (positions, members by address, ids) | -89 |
| 998839644 | EEOrbital's name test, speed and update in Rust (effect_orbit.rs) | -32 |
| 1ffe0d82b | EFMU's fold in Rust (ffi_mesh.rs `ee_gfx_effects_capture`) | -28 |

Round total: **-1,154** net C++ lines (target 4,000: see "What is left"). Rust added: tags.rs, effect_reads.rs,
map_raise.rs and their tests (17 + 7 + 8 tests), the orbital and EFMU additions (4 tests).

## What is left, and why

- **EEEffectHost.cpp** (about 1,500 lines): the answers that call C++ objects: media loads and unloads (`LoadMedia`,
  `GetGraphicObject` = `EEGraphicEffect::GetMedia`, a random draw), sounds, the effect manager's creations, the
  volcano's surface and model build, the farm's doppelgangers and drop sites, the gate's parts, the area effects,
  `UnitDamaged`, the 2D parts (`mParts`: a C++ vector of graphic objects whose media each part loads), the
  constructors. Next: the 2D parts as a Rust list in the record (one host entry for a part's media), the
  constructors' reads over effect_reads.rs (`ShotStart`, the winter breath, the tracers' pick point).
- **EERenderObject.cpp**: the fill's hosts still read the physical scale and frame, the model frames and the
  opacity's model centre and radius through C++ entries; effect_reads.rs' layout answers them now, but the fill's
  replay test (golden_fill_replay.rs) builds its own fake graphic objects: port its fakes to the object layout first.
- **EEGraphicObject.cpp**: LoadMedia/UnloadMedia (the model and texture managers, the sounds) and CollectParts
  (`EEOrbital`, `EEWheel`, `EETurretPart` objects other lanes copy) stay C++.
- **RTSMap.cpp**: Create, ApplyVertexColors, the line picks (`GetTileOnLine`, `GetLocationOnLine`: the mesh's
  intersection is Rust) are next; the tiles and their units are the units lane's objects.
- **The rest of the RTS Engine and the LLE** (RTSGraphics/RTSMouse: the graphics thread's message loop; GEModel's
  host: virtual calls on the store gate's GE subclasses; GEValues: the value types' forwarders) was not reached.

## Gates (round 1)

- Per commit: both builds 0/0, the oracle seeds 2-3, the effects capture (seeds 2-7 for the effect readers), the
  missile and oracle captures at seeds 2-3, the editor gate (for the terrain edits), the Rust gate.
- Tours (test profile) against the round's start exe: RFIL at the 1/20 clock (seed 7, 120 s, zoom captures): 248
  matched frames, all equal; the box-select/orders tour and the calamity casts (earthquake, hurricane, plague: the
  same casts pass and the same fire cast is refused on both exes) passed, RFIL equal on their matched frames but one
  (the orders tour at the normal clock, where selections change the world at wall times), captures looked at. The
  2D-draw digests differ by wall-time text (timers, resources) on both pairs alike.
- On the merge of origin/rust 2bccb3e94 (clean; strip_markers and the headers had nothing to do): both builds 0/0,
  the oracle seeds 2-7 identical, the captures oracle, late, effects and missile (seeds 2-3) PASS, the editor gate
  PASS, scenario_check 27/27, the Rust gate PASS, `bash tools/gates.sh` GATES PASS (tour, ai-game time-cap with both
  AIs alive on classic_lua, lan-game).
