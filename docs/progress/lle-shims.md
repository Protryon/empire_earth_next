# LLE: bodies over one-statement shims (rounds 8-10)

The lane's state after LLE round 10 (branch `rust-lle10`, 2026-11-21). The rounds' stories are PORT_LEARNINGS.md
"LLE round 8", "LLE round 9" and "LLE round 10". Earlier state: [lle-rest.md](lle-rest.md) (rounds 1-2),
[gfx-lle-cluster.md](gfx-lle-cluster.md), [lle-network-engine-whole-classes.md](lle-network-engine-whole-classes.md),
[game-leaves.md](game-leaves.md).

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

## Round 10 (branch `rust-lle10`)

| Commit | What | Logic (whole tree) | Net C++ (src, include but include/rust) |
|---|---|---|---|
| ed45f650a | the LLE's constant enums are Rust's: GERenderState.h, ISKeyCode.h, NEMessageID.h and 15 more enums (fonts, clip planes, image types, terrain flags, text alignment, modifiers, mouse entities, file origins, FP precision, OS types, music transitions, record modes, server states) are `#[repr(C)]` enums of ee_render gestate.rs, ee_base lle_enums.rs, ee_net msgid.rs that cbindgen emits unscoped (`cbindgen:enum-class=false`) | -282 | -365 |
| 541141654 | GEViewport derives from the Rust EeGeViewport (`cbindgen:field-names` gives the C++ names; its bodies are geval viewport_ffi.rs calls); GESurfaceDescription, GEVideoMode, GETStage, TMTime are the Rust records; their layout pins go (the DX7/GE surface description equality is a Rust const assertion) | -148 | -162 |
| 767d22efd | the NE server's and client's compound host cases split in the Rust FFI encoding (one op per C++ call; the out arguments the C++ types); the drivers (tests/net) print the old ops' lines | -46 | -24 |
| aa3e6dc1b | GEInitialize/GEShutdown in Rust (ge startup.rs over seven `ee_gecpp_*` shims); GETerrainMesh's out-argument members as Rust forms | -37 | -58 |
| 68a5c05e7 | the GE, DX7 and NE hosts' prologue locals inlined as per-case casts | -47 | -51 |
| 6153552b9 | ESError and FSFileAccess as Rust enums (the aliases a C++ anonymous enum); rustfmt | -20 | -24 |
| 17beef8c2 | U2DRectangle's Load/Save and FSFile::Read(long&)/(unsigned long&) in Rust (math u2d.rs, fs cppfile.rs; the calls table gains int32/float) | -32 | -30 |
| 728462fdc | the NE server's constructor arguments packed in Rust (ee_net_sv_new_game) | -27 | -13 |
| 398506caf | the unit chains' check switch and failure report (EE_CHECK_UNIT_LIST) in Rust (rts units.rs) | -23 | -32 |
| 19f8a110e | FSMemoryFile's count adapters in Rust (fs ffi.rs) | -23 | -13 |
| 184d52ec5 | the texture file info op writes the open formats' record | -8 | -6 |
| | **the round** (670df2148 -> 184d52ec5: 64,944 -> 64,251) | **-693** | **-778** |

In scope (src/Low-Level Engine but User Interface, include/lle but UI*, src/RTS Engine, include/ee/RTS*.h): 3,452
-> 2,759. Shims: 7 added for the start-up (`ee_gecpp_*`), 4 lambdas in the FSFile calls table; 2 deleted (the unit
list's `ee_rtsucpp_check_on`, `_list_failed`); the NE hosts gained 7 one-statement cases for the 4 compound ones split.

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

## Gates (round 10, on the tree merged with origin/rust 8830a1187)

- both builds 0/0 (EE-AOC 414 inputs, EE-SIM 415) and the test profile 0/0 (414); the headers current (299); the
  Rust gate PASS (5,607 tests, 0 failed; test, clippy, rustfmt, header). Earlier runs under load failed on
  plat_sys::thread's 10 s waits twice and once with an access violation in the ee_base test binary; ee_base passed
  alone, serially and ten times in parallel, and the final run passed whole;
- the NE drivers (`xbuild.py nettest ne_client_test ne_server_test`): 1500 sequences each, 0 differing; `--print`
  writes golden_client.txt and golden_server.txt byte for byte;
- the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines);
- the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects
  6, missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests); run dirs deleted;
- the plain saveload PASS (seeds 2-7, every save identical); the matrix `--settings all --seeds 2-3 --at 1000,1900
  --expect tools/saveload_expect.txt` exit 0, `saveload expect: PASS` (calamity 2/1900 missing and next 915/276 as
  expected); scenario_check 27/27; sim_editor PASS (14,448 lines), `--civbuilder` PASS (4,062); eedata check PASS,
  cppgate PASS;
- through slot.py, the test profile: the tour `--input-check --view-check --play-seconds 120 --seed 7` (F3 ok, VIEW
  CHECK PASS, RENDER CHECK ok: view 88.8% lit, 3,881 colours, mini map ok; RESULT PASS, no GX errors);
  `random-game --box-select --orders --seed 7` (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move;
  RESULT PASS);
- `bash tools/gates.sh` (EE_GATES_TREE the main checkout's tree): GATES PASS at the first run (tour; ai-game time-cap
  PT 12346, units [4, 38, 38], alive [2, 3], classic_lua x 2; lan-game PT 2837/2836).
- Not run: the POSIX builds, the store gate (tools/eemodel/storegate.py: it would need the FSFile calls table's four
  new entries if it links FSFile's long reads), `gates.sh --legacy`.

## What is left, by directory (logic lines, round 10's end; in scope 2,759)

- src/Low-Level Engine/Graphics Engine: GEModel.cpp 113 (GEPhysicalModel's reference-counting constructor,
  assignment and SetModel; the PM table and its retry line; the model manager's lock, dump line and create
  arguments; ModelNewSub; the terrain mesh's layout pins and Load/Save field streams), GETexture.cpp 86 (GEStoreFrame:
  the texobj_tex tests fake its op whole; the copy cases' rectangles; the surface description fill; the pack log; the
  lock), DX7Rasterizer.cpp 57 (the surface metrics and probe-window cases, the layout pins), GEBitmap.cpp 54.
- src/Low-Level Engine/Network Engine: NESynchronousServer.cpp 47: the message manager's compound cases (NewResponse,
  XferStatus, FileClone/Length/Read) are recorded at the op level by tests_server.rs `mm` (golden_server.txt):
  changing their arguments changes the golden's lines; ReportLaggers builds the C++ vector.
- src/RTS Engine: RTSMap.cpp 82 (EEMap's raise host, area outlines, vertex-colour corners, Load/Save contexts),
  RTSUnit.cpp 42 (the id registry, Describe, the save contexts).
- include/lle: class declarations the C++ users hold (GERasterizer.h 82, GESurface.h 48 and FSFile.h 44 pure virtual
  interfaces, UString.h 58 / UWideString.h 50 members returning `*this`, NESynchronousServer.h 48), the value types'
  members and inline math (GERectangle.h 77, U2DRectangle.h 56, U2DPoint.h 40, GE3DPoint, GEVector, ...: their Rust
  mirrors' C field names are snake_case and game-core code reads them, so deriving from the records is a cross-lane
  rename; the UM rounding inlines are the simulation's hottest code), forward declarations.

## The measure (round 8)

`logic_lines.py`'s case pattern `case\s+[^:]+` stops at the first `:` of a scoped enum (`case EeGeModOp::X:`), so
every one-statement case of the cbindgen enums' host switches counts as logic: about 470 lines in this scope at
the round's base (a variant that accepts `::` measured 5,812 in-scope logic lines against 6,281).

Round 9's numbers are the fixed script's (`case EeX::Y:` one-statement cases count as shims).
