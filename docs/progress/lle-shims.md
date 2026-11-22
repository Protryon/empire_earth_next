# LLE: bodies over one-statement shims (rounds 8-9)

The lane's state after LLE round 9 (branch `rust-lle9`, 2026-11-21). The rounds' stories are PORT_LEARNINGS.md
"LLE round 8" and "LLE round 9". Earlier state: [lle-rest.md](lle-rest.md) (rounds 1-2), [gfx-lle-cluster.md](gfx-lle-cluster.md),
[lle-network-engine-whole-classes.md](lle-network-engine-whole-classes.md), [game-leaves.md](game-leaves.md).

Scope (from this round): src/Low-Level Engine but its User Interface, include/lle but UI*, include/gx, include/dx7,
src/RTS Engine.

## The pattern

`ee_base::cpp_shims!` (ee_base shim.rs) makes, from one list of `fn name(args) -> ret = symbol;` lines, a trait of
the C++ calls a moved body makes, its FFI implementation (`ShimFfi`, absent from a test binary) and its
implementation on `ShimFake` (every call logged, answers and out values from per-shim queues). A moved body is
generic over the trait, so its unit tests assert the calls it makes, in order. The C++ side is an `extern "C"`
function of one statement per shim (a member's slot, a virtual call, a `new`/`delete`), in the class's own unit.

Plain members a body reads and writes become a `#[repr(C)]` record the C++ class derives from (as the units' and
the players' records before): `EeRtsPhysicsFields`, `EeRtsGraphicsFields`, `EeRtsMouseFields`. The Thread System,
Time Manager, Input System and sound samples are Rust objects already: the bodies call them directly.

## Round 9 (branch `rust-lle9`)

The measure is `logic_lines.py` (fixed for round 9: one-statement `case EeX::Y:` cases count as shims).

| Commit | What | Logic (whole tree) | Net C++ (src, include but include/rust) |
|---|---|---|---|
| 0b58b5b47 | `GERScreenState`/`GERFogAttributes` are the Rust records; 266 one-statement out-of-line members moved inline into their class headers (GERasterizer, GETerrainMesh, GEPhysicalModel, GEModel(Manager), GETexture(Manager), GEFontBitmap, NEMRecord, NEMessage, the NE server and client, FSPath, RTSGraphics, RTSMouse, RTSMousePointer, RTSUnit, RTSWorld) | -241 | -299 |
| dbbe0159f | the C++ `FSFile`'s calls for Rust (ee_base fs cppfile.rs, registered by File System.cpp); `UString`/`UWideString` `Load`/`Save`, the memory file's and the chunk's file loads and saves over them | -75 | -90 |
| 046e6a402 | `GELight` whole (ee_render ge light.rs); DX7 text draws, `CreateAWindow`, the clears' and highlight's rectangles, the light and stage reads; the field stream's byte calls | -192 | -190 |
| 12532feca | `FSPath` whole (ee_base fs path_obj.rs); `URandom`'s draws (ee_base random), `GEVector::Normalize`, `U2DPoint<long>::GetLineTiles`, `U2DRectangle<float>::GetClosestPoint` (math u2d.rs); dead U2D templates deleted | -316 | -420 |
| ae4274fe5 | `GETextureSet` whole (ee_render ge texset.rs); `FSMemoryFile`'s `Close`/`Copy`/`FOpen` disk copies; `GetImageFileAttributes` | -93 | -104 |
| | **the round** (b80c2286f -> ae4274fe5: 69,811 -> 68,894) | **-917** | **-1,103** (+539 -1,642) |

In scope (src/Low-Level Engine but User Interface, include/lle but UI*, src/RTS Engine, include/ee/RTS*.h): 4,381
-> 3,452. Shims: about 20 one-statement C++ shims added (the FSFile calls table's six lambdas, FSPath's seven, the
texture set's dereference, CreateAWindow's window constructor and debug line, GetLineTiles' vector assign, the DX7
host's split cases); the moved one-liners are moves, not new shims. Deleted: the DX7 host's `LightParams` case.

## Round 8 (branch `rust-lle8`)

| Commit | What | Logic (whole tree) | Net C++ (src, include but include/rust) |
|---|---|---|---|
| c4fb6b43a | `UMemoryPool<T>` and `UList<T*>` whole in Rust (ee_base util pool.rs) | -212 | -303 |
| 0fe29478a | the mixer plug-in whole in Rust (ee_audio mixer ffi.rs, plugin.rs) | -120 | -180 |
| 5e73f23f7 | RTSPhysics' bodies (ee_game rts physics.rs); `ee_base::cpp_shims!` | -235 | -246 |
| 5b806af90 | RTSGraphics, RTSMouse, RTSMousePointer, RTSMiniMap, RTSTile, RTSWorld, RTSUnit, RTSPlayer (ee_game rts) | -939 | -1,164 |
| 66328b822 | NEMRecord's bodies (ee_game lle netrecord.rs) | -150 | -234 |
| b03aad460 | RTSMap's own bodies (ee_game rts map.rs) | -66 | -61 |
| | **the round** (old measure) | **-1,722** | **-2,188** |

## Gates (round 9, on the tree merged with origin/rust 439543b4b)

- both builds 0/0 (EE-AOC 414 inputs, EE-SIM 415) and the test profile 0/0 (414); the headers current (296); the
  Rust gate PASS (5,452 tests, 0 failed; test, clippy, rustfmt, header: the first run's header check failed only
  because the headers were regenerated after it; rerun PASS);
- the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines);
- the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects
  6, missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests); run dirs deleted;
- the plain saveload PASS (seeds 2-7, exit 0); the matrix `--settings all --seeds 2-3 --at 1000,1900 --expect
  tools/saveload_expect.txt` exit 0, `saveload expect: PASS`; scenario_check 27/27; sim_editor PASS (14,448 lines),
  `--civbuilder` PASS (4,062); eedata check PASS, cppgate PASS;
- through slot.py, the test profile: the tour `--input-check --view-check --play-seconds 120 --seed 7` (F3 ok, VIEW
  CHECK PASS, RESULT PASS, no GX errors), the same tour on the older exe for the frame comparison (RESULT PASS);
  `random-game --box-select --orders --seed 7` (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move;
  RESULT PASS), the box-selected frame looked at;
- `bash tools/gates.sh` (EE_GATES_TREE the main checkout's tree): the first run GATES FAIL on the ai-game's
  "main-menu: the window capture is blank" (tour and lan-game PASS); the rerun GATES PASS (tour; ai-game time-cap
  PT 13146, units [4, 40, 40], alive [2, 3], classic_lua x 2; lan-game PT 3220/3223).
- Not run: the POSIX builds, the store gate (tools/eemodel/storegate.py), `gates.sh --legacy`.

## What is left, by directory (logic lines, round 9's end)

- src/Low-Level Engine/Graphics Engine: GEModel.cpp 142 (the host prologues, the model manager's create args and
  sub-model, the terrain host, the physical model's reference counting, the PM table), GETexture.cpp 109
  (GEStoreFrame, the copy cases' rectangles, the manager's lock/dump/pack-log cases, the layout pins), DX7Rasterizer.cpp
  79 (the surface metrics and probe-window cases: the scrobj replay goldens record their shapes; layout pins),
  GEBitmap.cpp 65 (the font bitmap's text byte and range cases), GERasterizer.cpp 46.
- src/Low-Level Engine/Network Engine: NESynchronousServer.cpp 101, Client 32, Communications: the hosts' compound
  cases. Not moved this round: tests_server.rs and the NE goldens replay each op's argument shape, so splitting a
  case means re-recording them.
- src/RTS Engine: RTSMap.cpp 82 (EEMap's bodies: game core's), RTSUnit.cpp 64, RTSPlayer.cpp 30.
- include/lle: the constant enums (GERenderState.h 82, ISKeyCode.h 74, NEMessageID.h 43), LowLevelEngineCore.h 81
  (ESError, the UM rounding inlines), the value types' members and inline math (GERectangle, GEViewport, U2DRectangle,
  GESurface's 30 pure virtuals: the store gate's fakes subclass them), UString/UWideString/FSFile declarations.

## The measure (round 8)

`logic_lines.py`'s case pattern `case\s+[^:]+` stops at the first `:` of a scoped enum (`case EeGeModOp::X:`), so
every one-statement case of the cbindgen enums' host switches counts as logic: about 470 lines in this scope at
the round's base (a variant that accepts `::` measured 5,812 in-scope logic lines against 6,281).

Round 9's numbers are the fixed script's (`case EeX::Y:` one-statement cases count as shims).
