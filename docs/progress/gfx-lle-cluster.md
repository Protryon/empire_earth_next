# The Graphics + LLE cluster (the Graphics Thread, the Data Pre-Loading Thread, the rest of the LLE, the RTS Engine)

The lane's state; each round rewrites it. The rounds' stories are in PORT_LEARNINGS.md ("The Graphics + LLE cluster,
round 1", "... round 2", "... round 3"). Earlier lanes over the same code: [obj-lane-r.md](obj-lane-r.md) (the fill and the effect
readers over the world views), [lle-rest.md](lle-rest.md), [r4-4-graphics-thread-27-8k.md](r4-4-graphics-thread-27-8k.md),
[r4-3-graphics-engine-17k.md](r4-3-graphics-engine-17k.md).

## LLE round 8 (branch `rust-lle1`, 2026-11-19): the leaves as inlines, GEValues gone

| Commit | What | Net C++ (src, include/ee, include/lle) |
|---|---|---|
| 8bedeaa0d | EEWorld::DumpMedia writes the reference-leak title itself (other lane's file) | 0 |
| 456ed11cb | Error System, Time Manager, UMath, Utilities, UConfig, UString, UWideString, UResourceFile, UEncryption, FSPersistentList, UReferenceCounter inline over ee_base; UDataCompression deleted (12 units) | -357 |
| 114a91b1f | GEValues.cpp deleted: the GE value classes inline over lle/GERust.h; ee_render::geval (rust/render/geval.h) | -1,000 |
| a69fb39cd | ee_render::geval keeps ge's clippy allows | 0 |
| 1ef5e6157 | FSBasicFile.cpp, FSFileSpec.cpp deleted (inline over ee_base fs) | -255 |
| 2126d466c, 341188a2a | rust_includes after the merge (GE headers include lle/GERust.h; EEUnitGoal.h, other lane's) | +2 |

Round total: about -1,610 net C++ lines (target 4,000; see "What is left"). The census (tools/cpp_deps.py) went
from 532 units / 154,656 lines to 517 / 152,634 before the merge of origin/rust; its leaves from 62 units
(3,208 lines) to 63 (3,641).

## Gates (LLE round 8)

- Per step: both builds 0/0, the oracle seeds 2-3 identical, the Rust gate PASS (the step's gate after
  114a91b1f failed clippy on the moved geval code; a69fb39cd fixes it). Once, `fs::golden_spec_replays` failed
  under load (an rmdir result) and passed alone and in a full ee_base run.
- On the merge of origin/rust 7779d7db1 (GEValues.cpp modify/delete, GEVector.h and PORT_LEARNINGS conflicts;
  strip_markers clean; rust_includes added four includes): both builds 0/0 (EE-AOC 560 inputs, EE-SIM 561), the
  test profile 0/0, the oracle seeds 2-7 identical, the Rust gate PASS; the captures at seeds 2-3: oracle PASS (212
  digests, 6 without a golden), effects PASS (6), missile PASS (22, 200 without a golden); sim_editor PASS (14,448
  lines), the civ builder gate PASS (4,062 lines), scenario_check 27/27.
- Tours (test profile, seed 7) against origin/rust 7779d7db1's exe, built in build/lle1_base_wt:
  - the 1/20-clock tour: PASS, RFIL 401 matched frames, all equal;
  - the box-select/orders tour: PASS (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move). RFIL
    has 15 matched frames, 10 of them different. Two runs of the base exe also differ in 10 of 22 matched frames:
    the normal clock is not repeatable;
  - the editor exercise: PASS.
  No GX errors. Captures looked at: the game view (the town centre, the units, the player colour, the minimap), the
  video settings.
- `bash tools/gates.sh` (EE_GATES_TREE = the main checkout's open tree): GATES PASS (tour; ai-game time-cap PT 13935,
  units [4, 42, 44], alive [2, 3], classic_lua x 2; lan-game).
- The census after the merge: 515 units, 149,975 lines; leaves 63 units (3,812 lines).
- Not run: the POSIX builds (tools/xbuild.py). The changes are portable C++17 (`if constexpr`, templates) and
  Rust, but no macOS or Linux build was made.

## What is left in the LLE, and why (round 8)

- **SCC 3, the GE core** (GEModel, GETexture, GEBitmap, GERasterizer, DX7Rasterizer): what is left is mostly host
  ops, where Rust objects call C++ virtuals of rasterizers, vertex and polygon arrays, surfaces and textures. The
  store gate's fakes (tools/eemodel/storegate/fakes.h) subclass GEVertexArray, GEPolygonArray and GERasterizer, so
  the virtual seam is the gate's design. Next: give the store gate a Rust seam, a capture of the arrays' values on
  the Rust side, before the model's host can call ee_render::geval directly. The terrain mesh's Time, Fractal and
  Quantize ops can call ee_base now, but its tests fake the clock and the fractal through them.
- **SCC 2, the LLE UI** (UIForm 735, UIManager 667, UIControl 492, and 28 small forwarder units): the hosts call
  UIControl's virtuals, which the game's controls override, so they stay C++ until the controls they reach are
  Rust kinds (the UI lane's shells, ee/EEUIShell.h). The small units' forwarders could become header inlines. Their
  constructors need UIForm and UIManager, which keeps a unit for them. A fold into User Interface.cpp was tried and
  reverted: it moves nothing to Rust.
- **U2DSparseArray** (162): an inline would include rust/sim/astar.h (3,100 lines, the AI lane's) in 62 units.
  Moving the sparse array's C ABI to a module of its own first, as with geval, would avoid that.
- **RTSTile, RTSMiniMap**: RTSTile's unit list is C++ nodes in EETile's pool. RTSMiniMap makes C++ surface and
  texture calls.

## Round 3 (branch `rust-gfx-cluster3`, 2026-11-19)

| Commit | What | Net C++ (src, include/ee, include/lle) |
|---|---|---|
| 62874d655 | The raster table's model, material, vertex, texture and state calls on the GE's Rust objects directly (raster_direct.rs; `DX7Rasterizer::RustObjects`) | -242 |
| 792c6c544 | `EEGraphicObject`'s LoadMedia/UnloadMedia, part scan and texture loads in Rust (gobj_media.rs); `LoadModel`'s shared frames as a slice; the members this left without a caller | -216 |
| e797fa789 | The graphic effects' tables (`sEffects`, `sMediaByKind`, `sIDs`) and `Load`/`Unload` in Rust (geffect_table.rs); other lanes' readers through `Get`/`IndexOf` | -61 |
| dae688eb5 | RFIL and the units' render frame over one view of the render lists (rfil.rs, units_lists.rs; `EERenderListsView`) | -352 |
| 281dbeb50 | RTSMap's line picks in Rust (map_raise.rs) | -45 |
| a7f695e83 | Rust readers of `gMouse` and `gViewport` for the UI lane (input_views.rs) | +15 |
| 97124f286 | Graphics members left without a caller deleted (tools/dead_members.py) | -17 |

Round total: **-918** net C++ lines (target 4,000: see "What is left"). Rust added (ee_render gfx): gobj_media.rs
(+4 tests), geffect_table.rs (+2), rfil.rs (+5), units_lists.rs, raster_direct.rs (+1), input_views.rs (+1);
ee_ge map_raise.rs's line picks (+2); manobj.rs and model_ffi.rs's shared-frame slice.

Other lanes' files edited: EEARust.cpp (`EEGraphicEffect::Get` moved to EEGraphicEffect.cpp), EECalamityCapture.cpp
and EEAECapture.cpp (`std::find` over `sEffects` -> `EEGraphicEffect::IndexOf`): in the effects' tables commit, coupled
to it; tools/eemodel/storegate/storegate.cpp (the shared-frame slice) with the graphic objects' commit.

## What is left, and why

- **EEEffectHost.cpp** (about 1,000 lines): its answers still call other lanes' C++ (units' classes and goals, the
  sounds, the area effects, the map's ground, the doppelgangers); the terrain reads (farm, volcano, glow rows) need
  the map's mesh in a layout the fill replay's fakes also provide.
- **EERenderObject.cpp**: the fill (FillUnit/FillObject's hosts, FillRenderLists, the doppelgangers, Fn10C036) and
  the raster table's remaining entries (the draws, the managers' loads, the camera, the lights in `ApplyLighting`).
  The fill's hosts that are plain GE reads (mesh z, water and valid tiles, model frames) wait for golden_fill_replay's
  fakes on a real Rust mesh.
- **EEGraphicObject / EEGraphicEffect as Rust objects**: their records and vectors are read by other lanes in place
  (the units' and effects' code); the loads, the tables and the part scan are Rust now, the lifetime and the record
  loader (`LoadTable`) are C++.
- **RTSMap Create, RTSGraphics, RTSMouse, GEModel's host, GEValues**: shells over C++ subclasses (tiles, surfaces,
  the thread) or value types every lane calls; each is a forwarder per member, so a move frees few lines.

## Gates (round 3)

- Per step on the unmerged branch: both builds 0/0, the Rust gate PASS, the oracle seeds 2-3 identical (three times, on
  the growing tree). The commits were split from one gated tree afterwards (mod.rs staged per commit); the
  intermediate commits were not built one by one, the final tree was.
- On the merge of origin/rust 4d66dc666 (WON round 19; clean; strip_markers, the headers and rust_includes had
  nothing to do): both builds 0/0 (EE-AOC 588 inputs, EE-SIM 589), the test profile 0/0, the Rust gate PASS, the
  oracle seeds 2-7 identical, the captures at seeds 2-3: oracle PASS (212 digests, 6 without a golden), late PASS
  (214, 8), effects PASS (6), missile PASS (22, 200 without a golden); the editor gate PASS (14,448 lines),
  scenario_check 27/27, `bash tools/gates.sh` GATES PASS (tour; ai-game time-cap, units [4, 42, 42], both AIs alive
  on classic_lua; lan-game).
- Tours (test profile, seed 7) against the round's start exe (00c2f7673's): the 1/20-clock tour PASS, RFIL 102
  matched frames, all equal; the box-select/orders tour PASS (BOX SELECT PASS, ORDERS PASS: gather, repair,
  repair-queued, move), RFIL 6 matched frames, all equal (the normal clock matches few); no GX errors. Before the
  merge the same tours gave 132 and 11 matched, all equal. Captures looked at: the town centre's player colour (the
  colour groups through raster_direct.rs), the units, their bars and shadows, the selection's status box, the
  minimap.
