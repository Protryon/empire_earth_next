# LLE: bodies over one-statement shims (rounds 8-13)

The lane's state after LLE round 13 (branch `rust-lle13`, 2026-11-22). The rounds' stories are PORT_LEARNINGS.md
"LLE round 8" to "LLE round 13". Earlier state: [lle-rest.md](lle-rest.md) (rounds 1-2),
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

## Round 13 (branch `rust-lle13`; forks `rust-lle13-a` (the NE), `rust-lle13-b` (the File System, the U* utilities))

Measured in scope (with include/gx, include/dx7) by `lle_scope.py` over `logic_lines.py`'s count; net C++ is src and
include but include/rust.

| Commit | What | Logic (in scope) | Net C++ |
|---|---|---|---|
| da7decc18 (a) | **nettest runs every program again**: ne_protocol_test, ne_message_test, ee_session_test, ee_session_orig (their engine definitions under `NE_PROTOCOL_ORIG`; ESVerify's driver hook `ee_error_set_verify_hook`; a stand-in LowLevelEngineCore.h renaming the Rust calls the session golden traces); NEMRecord's AddPlayerInput/GetPlayerInput answer the original's error codes again | 0 | 0 |
| dffc50234 (a) | the mechanical cross-lane edit: EEARust.cpp, EEScreens.cpp cast to EEServer*/EEClient* (3 lines) | 0 | +1 |
| 7107ec4f9 (a) | NEMessage::GetHeaderSlots one Rust call (ee_sim netmsg ne_records.rs) | -8 | -11 |
| 22cf3bab9 (a) | **the NE classes without the game's virtuals**: NESynchronousServer's 17 and NESynchronousClient's 12 pure virtuals gone (the game class passes `GameHost<Game>` / `GameMessageManagerHost<Game>`, header templates calling its members by name); NESynchronousCommunications keeps only ExecuteRemovePlayer virtual, its slots 2..8 one-statement members over the server's or client's Rust object | -65 | -132 |
| 8ac228bd6 (a) | the message manager's FileClone through FSFileSpec's copy constructor | -4 | -3 |
| 715147fbe (b) | FSFileSpec without operator< and GetLengthFromClosedFile (no caller) | -22 | -32 |
| 4ab405705 (b) | FSChunk's persistent loads and saves, FSPersistentList's creation and registration, FSArchive::FindFile and ExtractAll's flush in Rust (ee_base fs cppobj.rs over `kFSCppCalls`, six one-statement C++ calls File System.cpp registers) | -37 | -28 |
| 4ba939cf2 (b) | ESChecksum::Compare's texts pushed from Rust; UWideString::GetChecksum one Rust call | -11 | -8 |
| e5203173e (b) | ~FSMemoryFile and the scalar MemRead/MemWrite one Rust call each | -15 | -20 |
| 03a228220 (b) | UGraph without its unread title and axis labels | -13 | -23 |
| d60998c2b (b) | U2DPoint's distances over DistanceToSquared; U2DRectangle::GetCenter inline; dead Grow, DistanceToSquaredF | -27 | -44 |
| da1e7fa1a (b) | **sprintf(UString&), sprintf(UWideString&), ESDebugOut variadic templates** of one statement over one C-variadic `UFormat` (format checking kept: `LLE_PRINTF_TEMPLATE`); the debug text's buffer, cut and send in Rust | -20 | -1 |
| 02f91a223 (b) | the strings' Load/Save inline; FSFile::GetFirstFile(spec) one call; FSArchive's unused ArchivedFile record | -15 | -17 |
| 20225280a (b) | TMTimer::Stop, TSEvent::WaitAny, TSScheduler::GetCurrentThreadIndex one Rust call each | -15 | -17 |
| 0b3af7637 | **GESurface and GEScreen concrete**: DX7Surface and DX7Screen folded in (include/dx7/DX7Surface.h, DX7Screen.h deleted), their 46 virtual slots gone, each member one call of the Rust object (`mDX7` in the base); the model store gate's memory surfaces and screen are the `ee_dx7sf_*`/`ee_dx7sc_*` functions it links before the Rust library's (`/FORCE:MULTIPLE`, storegate.py fails on any other duplicate), its digests unchanged | -51 | -86 |
| 4d49a0d44 | nettest's session pair over batch 18 (tests and xbuild.py only): the form ids are rust/game/screens.h's enum; EEMRecord's Rust bodies ask the records' reads of EEMessageCapture.cpp, now linked; the original units' missing EECore9OpD an `/alternatename` | 0 | 0 |
| | **the round** (617b93bd3 -> 7cf0a9c9c; the merges of origin/rust d968d89bd and 4a255ad0d changed no in-scope line) | **-301** (1,818 -> 1,517) | **-421** |

Short of the -600 target: see "What is left". Shims added: the NE's `GameHost`/`GameMessageManagerHost` templates'
cases (moved from the engine's host), NESynchronousCommunications' slot members, `kFSCppCalls`' six calls, the
variadic templates, GESurface's 30 and GEScreen's 15 inline forwarders (moved from DX7Surface/DX7Screen). Deleted: the
two interfaces' 46 pure virtuals and DX7Surface.h/DX7Screen.h, the NE's 29 game virtuals, the store gate's generated
fakes (storegate.py no longer reads the headers' pure virtuals).

## Gates (round 13)

On 7cf0a9c9c (the round merged with origin/rust 4a255ad0d, batch 19), and before that on 0b3af7637 and 4d49a0d44
(merged with d968d89bd, batch 18), both full runs:

- both builds 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0 (412); the headers current (320); the
  Rust gate PASS (6,409 tests, 0 failed; test, clippy, rustfmt, header). The first run at 0b3af7637 failed once on
  ee_base `codec::golden::implode_output_is_byte_identical_to_the_c_encoder` (a different output hash, with the
  machine loaded); the test passed alone, six whole ee_base runs passed, and both later gate runs passed;
- plain `xbuild.py nettest`, exit 0: ne_msgmgr_test 20000 sequences, ne_queues_test 1500, ne_client_test 1500,
  ne_tune_test 20000, ne_server_test 1500, each 0 differing; ne_protocol_test, ne_message_test (711 checks),
  ee_session_test and ee_session_orig (44,289 lines) equal to their goldens. Once, in the first run, ne_client_test
  differed in 1 of 1500 sequences (1330: the last character of a random, unterminated transfer file name, which the
  original and Rust sides read past); four later runs had 0;
- the store gate PASS: 25,140 lines equal (2,583 model loads, 17,535 texture loads);
- the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines);
- the twelve capture profiles at seeds 2-3 PASS (digests equal: oracle 598, late 598, space 590, calamity 623, units
  510, effects 18, missile 66, app 38, scenario 113, groups 6, placement 4, transport 185). In the batch 19 run the
  calamity profile ran out of disk (the shared C: reached 0 bytes) and was rerun alone: PASS, 623 digests equal;
- the plain saveload PASS (seeds 2-7 at 600, 1350, 2100 s: 18 identical); the matrix `--settings all --seeds 2-3 --at
  1000,1900 --expect tools/saveload_expect.txt` exit 0, `saveload expect: PASS`; scenario_check 27/27; sim_editor PASS
  (14,448 lines), `--civbuilder` PASS (4,062); eedata check PASS, cppgate PASS;
- through slot.py, the test profile, `--data` the main checkout's tree: the tour `--input-check --view-check
  --play-seconds 120 --seed 7` (F3 ok, VIEW CHECK PASS, RENDER CHECK ok: view 88.5% lit, 4,070 colours; RESULT PASS);
  `random-game --box-select --orders --seed 7` (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move;
  RESULT PASS); `bash tools/gates.sh`: GATES PASS (tour; ai-game time-cap PT 11309, units [4, 32, 36], alive [2, 3],
  classic_lua x 2; lan-game PT 3396/3339). The batch 18 run before it passed the same (ai-game PT 11802, units
  [4, 36, 36]; lan-game 2994/2994).
- Not run: the POSIX builds, `gates.sh --legacy`.

## What is left, by directory (logic lines, round 13's end; in scope 1,517)

- The structural floor, most of what remains: member and forward declarations other lanes read (RTSGraphics.h 44,
  RTSUnit.h 40, RTSWorld.h 30, GERasterizer.h 44: forward declarations, the draws needing a complete GE type),
  the host switches' signatures and `default:` lines, layout `static_assert`s; NEMessage.h 30 (real polymorphism:
  NEMRecord, the game's messages, the drivers').
- GX.h 62: the DirectX 7 constants as macros and enums and the classes' inline forwarders.
- LowLevelEngineCore.h 47: the UM rounding inlines (hot), the error-code enum, `UFormat` (about 12 lines while the
  formatting stays the C runtime's: a Rust printf would risk byte parity on %f/%g, and stable Rust cannot take a
  `va_list`).
- The NE: NESynchronousServer.cpp 27 (NewResponse, FileLength/FileRead: each needs new message-manager ops, the msgmgr
  and server drivers' fakes and re-recorded goldens), NESynchronousCommunications.cpp 18 (FpuCheck, MsgExport: the
  same encoding question), NESynchronousClient.h 20 (Connect's and GetDetectedGame's structs: ne_client_test uses the
  other mode of `ee_net_cl_connect`'s pointer).
- GESurface/GEScreen (20, 21): the logical-size members and the field declarations; the SurfaceMetrics op could go
  if the logical size moved into the Rust surface (the store gate's fakes would hold it too).
- RTSMap.cpp 40, RTSPlayer.cpp 30: the save contexts and their ops (the colour's and name's loads and the shade
  colour need the player record to hold the colours: the units lane's record), SettleTile's unit loop.
- The File System: FSFile's, FSArchive's and FSMemoryFile's member declarations; the `operator=` bodies of the form
  "Rust call; return *this"; TSScheduler::CreateCriticalSection (tests/sound and tools/tcp_test fake its Rust call).

## Round 12 (branch `rust-lle12`; forks `rust-lle12-a`, `rust-lle12-b`)

Measured in scope (with include/gx, include/dx7) by `lle_scope.py` over `logic_lines.py`'s count: each row is the
in-scope logic delta of its commit; net C++ is src and include but include/rust.

| Commit | What | Logic (in scope) | Net C++ |
|---|---|---|---|
| 3568758b4 | **GERasterizer without virtuals**: the game links one rasterizer, so its members are inline forwarders to `ee_dx7r_*` over `mDX7` (moved into the base); the terrain draws take the mesh, runs and rectangle as template parameters (DrawGridLines' two `LLE_EXE_HEADERS` declarations become one); DX7Rasterizer.h's 60 overrides gone | -118 | -267 |
| 1150f87fd | DX7Rasterizer folded into GERasterizer (its constructor makes both Rust objects; `DX7RasterizerHost` a function); unused forward declarations; EERenderObject.cpp names `GERasterizer::RustObjects` | -15 | -24 |
| 57a81f07f | GERasterizer(name): a rasterizer without a DX7 object for tools/eemodel/storegate (its StoreRasterizer subclass and generated rasterizer fakes go) | +4 | +11 |
| e2035c362, 6d9d310d2 | `NESynchronousServer::XferFileToClient` one Rust call (the FFI answers the error code as a long and writes the `unsigned long` id) | -9 | -11 |
| 0e5a10059 (a) | **GX.h's structures, callback types and functions are cbindgen's** (rust/render/gx.h from ee_render gx abi.rs and ffi.rs, renamed Gx* -> GX*; the constants an ignored consts.rs, GX.h keeps them as macros and enums; `Rect` ignored so the signatures name Windows' RECT); gx_test and sdl_test read the generated structures | -152 | -180 |
| 705ac30a9 (a) | FSFile one concrete class holding FSBasicFile's and FSFileSpec's members and virtual bodies (41 pure virtuals gone); FSMemoryFile::Close inline | -40 | -45 |
| 5fc4e19ed, 59cd76304, 933c1e98a, a44c0d86a, 641161183, 1bc69a851 (a) | FSChunk::GetChunkSize, FSArchive's Create, ListFiles, SetTag and tag sinks; URandom's seed fix-up and copy; UConfig's integer and flag reads and UResourceFile::GetString (ee_base util config.rs, strings.rs); the sparse point set's lists through the C++ vectors | -81 | -88 |
| 90880f236 (b) | GERhombus, GELine and GEBitmap's per-vertex members one Rust call each | -52 | -22 |
| 2b0d27f58 (b) | **GEStoreFrame in Rust** (texobj_tex.rs `store_frame`; the op split into `FileFrame` and the surface getters, the texobj_tex fake updated, goldens unchanged); the managers' dump text, lock and create arguments in Rust (`EeGeModelCreate`); ModelNewSub split | -120 | -158 |
| 6cf9715d6 (b) | the GE objects' PlatFont calls made from Rust directly; the font bitmap's cells | -29 | -32 |
| 5d18cba75 (b) | GEPhysicalModel inline over Rust; the physical walk table and its retry a Rust static; the mesh's records typedefs of the Rust ones; GETexture's constructors and `sRasterizer` in Rust | -62 | -116 |
| 17e120bb5, 0a587faec, acdaef0ad (b) | GEVertexArray::GetOpacity, GEPolygonArray::SetVertexIndices; the GE hosts' last compound cases; GEFontBitmap's constructor inline | -26 | -33 |
| | **the round** (78939ebc9 -> 57a81f07f; the merge of origin/rust changed no in-scope line) | **-700** (2,518 -> 1,818) | **-965** |

`logic_lines.py` over the whole tree (src, include/ee, include/lle: without include/gx and include/dx7): 55,758 ->
55,284 (-474, at the forks' merge 4610c9486). Short of the -1,000 target: see "What is left".

Shims added: GERasterizer's ~50 inline forwarders (moved, not new: DX7Rasterizer's out-of-line bodies), the GE hosts'
split ops (fork b: FileFrame, SurfWidth/Height/BytesPerRow/Memory, ModelName/NewSub/SetSub, BaseDereference,
LockVerify, VerifySurface/Texture), FSFile's merged forwarders, `ee_sparse_*` resize shim. Deleted: the DX7Rasterizer
class, GERasterizer's 47 virtual slots, FSFile's 41 pure virtuals, the `StoreFrame` op, `ee_ge_texture_frame_fits`.

## Gates (round 12)

On the tree merged with origin/rust a85f71862 (89bfc6b67), at 6d9d310d2 and the tip 57a81f07f:

- both builds 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0 (412), at the tip; the headers current (310);
  the Rust gate PASS (test, clippy, rustfmt, header) at 6d9d310d2 (the tip's last commit changes C++ only);
- the NE drivers (`xbuild.py nettest` with the five named): ne_msgmgr_test 20000 sequences, ne_queues_test 1500,
  ne_client_test 1500, ne_tune_test 20000, ne_server_test 1500: each 0 differing. Plain `xbuild.py nettest` stops at
  ne_protocol_test, which does not compile since the utility leaves became inlines (456ed11cb, 2026-11-19: it defines
  ESRecordNativeError, ESVerify, TMGetTime ... itself): not this round's, not fixed;
- the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines) at 6d9d310d2 and at the tip;
- the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects 6,
  missile 22, app 26, scenario 43, groups 2, placement 4, transport 185 digests), at 6d9d310d2;
- the plain saveload PASS (seeds 2-7 at 600, 1350, 2100 s: 18 identical); the matrix `--settings all --seeds 2-3 --at
  1000,1900 --expect tools/saveload_expect.txt` exit 0, `saveload expect: PASS`; scenario_check 27/27; sim_editor PASS
  (14,448 lines), `--civbuilder` PASS (4,062); eedata check PASS, cppgate PASS (at 6d9d310d2);
- the store gate (tools/eemodel/storegate.py) PASS at the tip: 25,140 lines equal (2,583 model loads, 17,535 texture
  loads). It failed to compile at 6d9d310d2 (its StoreRasterizer: fixed by 57a81f07f);
- through slot.py, the test profile, at the tip: the tour `--input-check --view-check --play-seconds 120 --seed 7`
  (F3 ok, VIEW CHECK PASS, RENDER CHECK ok: view 89.1% lit, 3,924 colours; RESULT PASS, no GX errors);
  `random-game --box-select --orders --seed 7` (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move;
  RESULT PASS); `bash tools/gates.sh` (EE_GATES_TREE the main checkout's tree): GATES PASS (tour; ai-game time-cap PT
  12586, units [4, 38, 39], alive [2, 3], classic_lua x 2; lan-game PT 2807/2804).
- Not run: the POSIX builds, `gates.sh --legacy`.
- Harness note: fixture_game.py builds `build/open_data/tree` in a worktree that has none unless `--data` names a
  tree (EE_GATES_TREE is gates.sh's); a killed build leaves a partial tree that sim.py then prefers (EE-SIM stops at
  EETerrainColorTable.cpp 81, no textures). Pass `--data` to the harness sessions in a worktree.


## What was left after round 12 (logic lines; in scope 1,818)

- The structural floor, most of what remains: pure-virtual interfaces with two implementations (GESurface.h 48 and
  GEScreen.h 32: DX7Surface/DX7Screen and the store gate's StoreSurface/StoreScreen), the game's own virtuals the
  NE server and client declare (NESynchronousServer.h 46, NESynchronousClient.h 35, NEMessage.h 30), member and
  forward declarations other lanes read (RTSGraphics.h 44, RTSUnit.h 40, RTSWorld.h 30), the host switches'
  signatures and `default:` lines, layout `static_assert`s.
- GX.h 62: the DirectX 7 constants as macros and enums (the renderer's spelling) and the classes' inline forwarders.
- GERasterizer.h 44 / DX7Rasterizer.cpp 34 / GERasterizer.cpp 29: the out-of-line members needing a complete GE
  type (ActivateScreen, the bitmap, text, polygon-array and terrain draws), the two hosts, the 2D list's text case.
- LowLevelEngineCore.h 41: the UM rounding inlines (hot), ESDebugOut (varargs).
- NESynchronousServer.cpp 40: the message manager's NewResponse (an NEMRecord built from the server's counters),
  XferStatus / FileLength / FileRead (out arguments the game's virtuals take by reference; the op encoding is shared
  with tests/net's drivers, so changing it means changing them too), ReportLaggers' vector.
- RTSMap.cpp 40, RTSPlayer.cpp 30, RTSUnit.cpp 14: the save contexts, SettleTile's unit loop, the shade colour.
- Fork a's: UString.h 32 (UFormat's varargs), FSFileSpec.h 27 (operator< and GetLengthFromClosedFile call the virtuals
  FSMemoryFile overrides), FSArchive.h 27 (ExtractAll/FindFile make C++ objects), FSMemoryFile.h 26 and UGraph.h 24
  (their constructors: a UString capacity question).
- Fork b's: GETextureDescription (EETerrainTable.cpp aggregate-initialises it), GEPhysicalModel's hot setters,
  GEWindow.h (kept free of rust/render/ge.h on purpose), the read/write locals of two hosts.

## Round 11 (branch `rust-lle11`)

| Commit | What | Logic (whole tree) | Net C++ (src, include but include/rust) |
|---|---|---|---|
| bc5f685a7 | UString/UWideString: results the Rust call fills (`UString(kUnset, fill, args...)`), `Do` for the self-returning changes, the tokens pushed from Rust (`ee_ustring_tokens`), one `sprintf` formatter (`UFormat`) | -54 | -10 |
| 5a07b4202 | the value types' codecs over the C++ FSFile from Rust (ee_render geval values.rs over ee_base `CppFile`; io.rs `cpp_file_io`): points, vectors, orientation, rectangle, colour, transformation, lines, planes, boxes, the terrain mesh; GERust.h's `GERustIO` field stream deleted; the FSFile calls table gains u8/u16 and `GetIsOpen` | -76 | -87 |
| ce0e08adf | U2DPoint's Load/Save in Rust (math u2d.rs `ee_u2d_point_io`) | -18 | -16 |
| d77b3e8c5 | RTSMap's `IsAreaOnMap` and raise bounds (map_raise.rs), its hosts in one expression; RTSUnit's registry entry points, handle id report and descriptions (ee_game rts units.rs) | -70 | -63 |
| ea9ad590f | the interpolations clear their vector from Rust; a float's bits through `__builtin_bit_cast`; the water ripple's values | -26 | -16 |
| b3860bb57 | GEPhysicalModel's model reference in Rust (`ee_ge_pm_set_model`); GETexture's copy rectangles; GEPolygon.h, GEPolygonGroup.h, GEVertex.h deleted (unused) | -83 | -145 |
| 30773a308 | U2DRectangle, U2DPoint, GEColor, GETextureCoordinate, RTSRegionTable without member-by-member copies (initializer lists, one-expression component-wise members) | -59 | -54 |
| 491c1dd82 | **the value types derive from their Rust records**: GE2DPoint, GE3DPoint, GEVector, GE3DLine, GE3DPlane, GETransformation, GEOrientation, GEQuaternion, GEBoundingBox, GERectangle (`cbindgen:field-names` on ee_base math types.rs); member lists, user copies, `GERustLayout` and GERustMath.h's pins gone; `ee_ge_bbox_define` | -155 (with the next) | -236 (with the next) |
| ca6eeffbd | the rename outside the LLE (mechanical, clang's positions): EECameraControl.cpp, EERenderObject.cpp, EEEffectHost.cpp, EEEffectMesh.h, 25 lines `x/y/z` -> `mX/mY/mZ` | | 0 |
| | **the round** (2cdafaff9 -> ca6eeffbd: 60,680 -> 60,139) | **-541** | **-627** |

In scope (with include/gx, include/dx7): 3,057 -> 2,516. Shims added: the FSFile calls table's five entries,
`UString`/`UWideString::PushToken`, the interpolations' `Push` clear branch, `EEMapRaiseOps::Ptr`; deleted: none.
No class moved wholly; the ten value types are now their Rust records with inline C++ members. The merge of
origin/rust 5143a9696 needed no further rename (no new reader of the records' fields).

Speed (EE-SIM, the oracle's seeds 2-7 at --jobs 2, whole-run wall time, the machine shared): the value types before
the derive (b3860bb57) 339 s and 335 s, after (ca6eeffbd) 337 s and 327 s: no slowdown. The hot inline math stays
inline; the component-wise members are one expression with the same float operations in the same order.

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

## Gates (round 11)

On the tree merged with origin/rust 5143a9696:

- both builds 0/0 (EE-AOC 412 inputs, EE-SIM 413) and the test profile 0/0 (412); the headers current (308); the
  Rust gate PASS (5,734 tests, 0 failed; test, clippy, rustfmt, header). Earlier runs: the first failed to compile tmobj_tests.rs
  (fixed, aca16a8e2), the second on rustfmt (fixed, d65fe6f6a), the third on plat_sys::thread
  `set_from_another_thread_wakes_a_waiter` (the known 10 s flake: it passed alone, and the next whole run passed);
- the NE drivers (`xbuild.py nettest`): ne_msgmgr_test 20000 sequences, ne_queues_test 1500, ne_client_test
  1500, ne_tune_test 20000, ne_server_test 1500: each 0 differing. Two drivers no longer compiled since earlier rounds and are fixed
  here: ne_queues_test defined URandom's draws (inline over Rust since round 9), ne_tune_test the slots and server
  states (Rust enums since round 10);
- the oracle seeds 2-7 identical (6087, 5300, 6072, 7649, 7842, 6540 lines);
- the twelve capture profiles at seeds 2-3 PASS (oracle 212, late 214, space 210, calamity 222, units 175, effects
  6, missile 22, app 26, scenario 43, groups 2, placement 4, transport 185 digests); run dirs deleted;
- the plain saveload PASS (seeds 2-7 at 600, 1350, 2100 s: 18 identical); the matrix `--settings all --seeds 2-3 --at
  1000,1900 --expect tools/saveload_expect.txt` exit 0, `saveload expect: PASS` (calamity 3/4: 2/1900 missing; next 2/4:
  seed 2 diverges 915 and 276 ticks after its loads, as expected); scenario_check 27/27; sim_editor PASS (14,448 lines), `--civbuilder` PASS (4,062); eedata check
  PASS, cppgate PASS;
- through slot.py, the test profile: the tour `--input-check --view-check --play-seconds 120 --seed 7` (F3 ok, VIEW
  CHECK PASS, RENDER CHECK ok: view 88.8% lit, 3,899 colours, mini map ok; RESULT PASS, no GX errors);
  `random-game --box-select --orders --seed 7` (BOX SELECT PASS, ORDERS PASS: gather, repair, repair-queued, move;
  RESULT PASS);
- `bash tools/gates.sh` (EE_GATES_TREE the main checkout's tree): GATES PASS at the first run (tour; ai-game time-cap
  PT 12337, units [4, 38, 38], alive [2, 3], classic_lua x 2; lan-game PT 2819/2820).
- Not run: the POSIX builds, the store gate (tools/eemodel/storegate.py: it links FSFile through the calls table,
  which gained five entries), `gates.sh --legacy`.

## What was left after round 11 (logic lines; in scope with gx/dx7 2,516)

- src/Low-Level Engine/Graphics Engine: GEModel.cpp 89 (the PM table and its retry line; the model manager's lock,
  dump line and create arguments; ModelNewSub; the terrain mesh's layout pins), GETexture.cpp 79 (GEStoreFrame: the
  texobj_tex tests fake its op whole, so moving it means splitting the op into the surface getters and updating the
  fake; the surface description fill; the pack log; the lock), DX7Rasterizer.cpp 57 (the surface metrics and probe
  window cases, the layout pins), GEBitmap.cpp 50 (the hosts' metric and text cases: PlatFont* is Rust, so these can
  call it directly once the smallobj/fbobj fakes answer the new calls).
- include/lle: interfaces the C++ users hold: GERasterizer.h 82 (one implementation, DX7Rasterizer: the virtuals
  could become inline forwarders to ee_dx7r_* with DX7Rasterizer.h's overrides deleted), GESurface.h 48 (two
  implementations: DX7Surface and the store gate's), FSFile.h 44, FSArchive.h 42, FSFileSpec.h 32; the NE server
  and client 48/35; LowLevelEngineCore.h 41 (the UM rounding inlines, hot: kept); GETransformation's `Apply` rows (they
  read the output, hot: kept).
- src/Low-Level Engine/Network Engine: NESynchronousServer.cpp 47 (the message manager's compound cases are recorded
  at the op level by tests_server.rs `mm`; XferFileToClient's out argument is read by tests/net's driver).
- src/RTS Engine: RTSMap.cpp 40 (SettleTile's unit loop, the Load/Save contexts), RTSPlayer.cpp 30 (the save
  context, the shade colour), RTSUnit.cpp (the save contexts); include/ee: RTSGraphics.h 44, RTSUnit.h 40, RTSWorld.h 30
  (declarations other lanes read).

## The measure (round 8)

`logic_lines.py`'s case pattern `case\s+[^:]+` stops at the first `:` of a scoped enum (`case EeGeModOp::X:`), so
every one-statement case of the cbindgen enums' host switches counts as logic: about 470 lines in this scope at
the round's base (a variant that accepts `::` measured 5,812 in-scope logic lines against 6,281).

Round 9's numbers are the fixed script's (`case EeX::Y:` one-statement cases count as shims).
