# Rust: a research plan for an iterative rewrite

Status (2026-11-09): R0-R3, R4.1-R4.2 and R5b.1's formations built on the `rust` branch, except what the table below lists as open.
Revised for the WON library (module `won`, the upstream merge to 3f51317465d6) and its lobby on the platform layer. The goal is a game written wholly in Rust. The way there is one binary
in which C++ and Rust coexist for a long time. Each step moves one module across a seam, and the game keeps passing
its gates at every commit. This document covers:

- what the code base looks like from that angle (modules, coupling, the seams that already exist);
- the invariants a Rust replacement must keep;
- the existing work that makes the rewrite testable;
- an ordered plan and the decisions it needs from the owner.

Companion documents: PORT.md (the port, the platform layer, GX), docs/MODDING_PLAN.md (schemas, rules as data;
several items there are deferred "to the Rust rewrite"), docs/AI_RESEARCH.md (the AI and its scripting runtime: mlua is
the chosen Rust path), PORT_LEARNINGS.md.

## Progress (the `rust` branch, 2026-11-09)

| Step | State |
|---|---|
| R0 Rust library | Done |
| R1.1-R1.3 platform leaves, files, threads, sockets | Done (`ee_plat_core`, `ee_plat_sys`). Windows PlatString still calls the CRT, whose behaviour depends on the locale |
| R1.4-R1.5 window, fonts | Done (`ee_plat_ui`, `ee_plat_sdl`): SDL3 kept (D4), stb_truetype ported line by line, 51,520 glyphs byte-equal |
| R2.1-R2.3 rules schema, rustgen, text compiler | Done (`ee_data`) |
| R2.4 glTF and PNG | Done (`ee_openformats`), 7,150 of 7,150 gate cases equal to the C++ digests |
| R3.1-R3.3 mixer logic, compression, encryption, TCP | Done (`ee_mixer`, `ee_codec`, `ee_net_tcp`). miniaudio stays; no audio has been listened to through the Rust mixer |
| R3.4 random maps | Done (`ee_rmap`), 2,088 of 2,088 maps equal to the C++ digests. The `.rmv` reader (`Unit10.cpp`) stays C++ |
| R3.5 File System | Done (`ee_fs`) behind the C++ facades |
| **R3.6 Sound Engine** | **Open.** `SESoundManager`'s nodes are held by simulation objects and shared between threads |
| R4.1 GX on the `wgpu` crate | Done (`ee_gx`); wgpu-native is gone |
| R4.2 DX7 plug-in | Done (`ee_dx7`), a faithful port (D5) with C++ trampolines. C++ keeps the classes, object lifetimes, `Set3DMode`'s matrix composition, the PIM table and pl_mpeg |
| **R4.3 Graphics Engine (17k)** | **Open.** May move with R5's renderer decoupling, as this plan allows |
| R5b.1 formations | Done (`ee_formations`): the C++ classes keep their members and call Rust for the geometry. 14,068 `Place` calls captured from the C++ in the six oracle games (`tools/sim_capture.py`: real games plus a sweep of every formation type over real units) equal bit for bit |
| **R5b.1 A* pathfinding, continents, choke points** | **Open.** No plain-data seam yet: the goals read the live world tile by tile, lazily and with side effects, and walk the storage's lists (PORT_LEARNINGS "R5b.1"). The address-ordered set at `EEAIAStarPathAreaGoal.cpp:282` turned out order-independent |

Checked on Windows at the end: link 0/0, the Rust gate, `eedata check` and `cppgate`, `gx_test`, `gates.sh` (tours and AI games on both
trees), six seeded 45-minute sim traces equal to the pre-Rust reference, menu captures pixel-equal, a saved game loaded and
played on. Linux: the game cross-compiles and links, but has not been run. macOS: Rust type-checks only; nothing was
built or run.

---

## 0. Summary

- **Size.** About 570k lines of first-party C++: `src/` 484k, `include/` 86k. Of that, 131k is the WON library and
  its lobby; the other ~440k is the engine and the game. The vendored C is ~210k more (miniaudio, zlib, libpng,
  libjpeg, dmusic, pl_mpeg, stb, cgltf, Lua, the codec headers WON carries). The engine (`src/Low-Level Engine`, 68k)
  is cleanly layered. The game (`src/Empire Earth` without WON, ~300k) is one strongly connected component around
  `EEWorld`, the units, `EEPlayer` and the physics tick.
- **The seams are better than the coupling suggests.**
  - The platform layer is already a C ABI that names Rust as an intended implementation (`include/platform/Platform.h`).
  - The renderer sits behind the GX contract and four abstract engine classes.
  - Plug-ins are name-indexed entry-point tables (PIM).
  - UI input reaches the simulation only as network messages (`NEMessage`), never as direct writes.
  - Every on-disk record is size-pinned, and every network message has a fixed wire format.
  - The WON lobby reaches the screen as one pulled BGRA frame, and its GUI backend is already ours (`WONGUI/Plat*.cpp`).
- **The rewrite is testable.** EE-SIM plays a seeded 45-minute 2-AI game in ~15 s. Its AI trace is identical
  between runs and equal to the harness's. "Same trace for seeds 2-7 over 2700 game seconds" is a ready-made,
  layout-independent oracle for any change on the simulation side. The data side has the content-dump parity gate
  (`eedata.py cppgate`). The platform, GX and TCP layers have self-tests that can run against a second
  implementation.
- **The hard parts:**
  - the simulation's bit-exact float and RNG behaviour (lockstep, checksums of float bit patterns);
  - 638 `dynamic_cast`s and intrusive, non-atomic reference counts on objects shared between threads;
  - the Windows harness, which reads game memory by clang record layout and hooks C++ mangled names;
  - `UString`/`std::vector` in nearly every interface, including the "C" plug-in entry points.
- **The plan, in order:**
  - R0: one Rust static library in the build, absorbing wgpu-native.
  - R1: platform-layer leaves.
  - R2: the data layer.
  - R3: engine services and plug-ins.
  - R4: the renderer.
  - R5: the simulation, after C++-side untangling.
  - R6: UI, editor and the lobby (what is left of WON is decision D8).
  - R7: the entry point and the last C/C++.

  Every Rust module lands with unit tests (section 5). Each phase ends with the existing gates. R5 is about half the code outside WON and most of the risk; R0-R4 are
  worth doing even if R5 slips, because they remove the C toolchain's hardest dependencies (webgpu.h pinning,
  Win32/POSIX forks, the text compilers).

---

## 1. The code base from a rewrite's point of view

### 1.1 Modules

Lines include each module's headers. "Build module" is `tools/units.py`.

| Area | Lines | What | Coupling |
|---|---|---|---|
| Platform (`src/Platform`, `include/platform`) | 7.7k | Time, threads, files, paths, strings, config, sockets, window (SDL3), fonts (stb_truetype) | **No outbound edges.** C ABI. Tests in `tests/platform` (about 265 checks) |
| LLE Utilities | 9.4k | UString, URandom, compression (PKWARE DCL), encryption, graphs, 2D geometry | Leaf; fan-in 35 modules |
| LLE File System | 3.9k | FSFile, FSArchive, FSChunk, FSMemoryFile, FSPersistent | Utilities, threads, platform; fan-in 31 |
| LLE Thread System / Time Manager / Error System | 1.8k | TSThread queues, locks, scheduler, timers | Platform |
| LLE Sound Engine (+ Mixer plug-in) | 3.5k | Sound manager, conductor (dmusic), mixer on miniaudio | Platform, core |
| LLE Network Engine (+ TCP plug-in) | 7.2k | Lockstep server/client, messages, TCP on PlatSocket | Platform, core |
| LLE Graphics Engine | 26.6k | GE* models, textures, fonts, cameras; the DX7 renderer plug-in (9.7k) over GX (3.1k on webgpu.h) | Clean; fan-in 31 |
| LLE User Interface | 14.4k | UI* widget toolkit, forms from the UI db tables | Graphics Engine; one edge to Database |
| RTS Engine | 3.6k | RTSWorld, RTSUnit, RTSTechTreeNode base classes | Points *upward* into Units, Player, Map, AI |
| Database | 8.5k | db tables, `EEDataText` text compiler, `EERules`, `EEDataDump` | Mostly GE/FS; 2 files touch `gWorld` |
| Random Map (+ Default generator) | 7.3k | Map scripts (.rmv), terrain/player/resource placement | Generator depends only on Utilities and `EERandomMap.h` |
| Formations, Map, Tech Tree | 5.3k | Formation geometry; tiles/minimap; techs (declared in `EEPlayer.h`) | Small, but Tech Tree is tied to `gWorld` |
| Units, Physics, Player, Calamities, Actions | 61k | The simulation | The strongly connected component |
| AI Thread | 63k | Computer player (EECP*), A* pathfinding (5.9k), continents | Runs *inside* the physics tick (`EEPhysics.cpp:756`), not on a thread |
| Communications | 12.9k | Game messages (EEM*), input -> commands | |
| Graphics Thread | 27.1k | The game's renderer; reads the live world under the read lock | Graphics Engine heavy |
| User Interface Thread, UI Events | 36.5k | In-game UI | |
| Scenario Editor (+ Actions), Scenario Manager, Campaign | 39.5k | Editor, triggers | Largest fan-out (30 modules) |
| Empire Earth | 14.4k | WinMain, EEWorld, game state, EESim, test fixture, game log | Hub |
| WON library (`src/Empire Earth/WONAPI`, `include/ee/wonapi`) | 131k | WONGUI toolkit 49k; WONLobby 28k (with the game glue in `GameCode/EmpireEarth`); sockets, async ops, crypto, auth, directory, routing, HTTP and the rest 32k; headers 21k, not split by area | On the platform layer (46 files call `Plat*`). Own `SmartPtr` objects (173 typedefs), 52 `dynamic_cast`s. The game side is `EEWonNet`, `EEAWonNet` and the WonNet forms (~4k, counted in the rows above) |

### 1.2 Coupling

- **Engine is layered.** The LLE subsystems depend only on the core header, Utilities, Thread System, File System
  and Platform. The only cycles are core <-> subsystem ones, created by the `LowLevelEngine.h` umbrella header.
- **Game is one knot.** Every game module is in one strongly connected component. The heaviest mutual edges are:
  - AI <-> Units (181/81 include edges);
  - Graphics Thread <-> Units (89/18) and Graphics Thread <-> Physics (64/8);
  - Physics <-> Units (47/72);
  - RTS -> Graphics Thread (146): the base layer includes the game.
- **Global state.** 224 `g*` names are declared in headers and ~459 are defined in `src`; ~30 are still
  address-named (`gU4E8708`). The ones touched most:
  - `gWorld`: 310 files in 23 modules;
  - `gPhysics`: 174;
  - `gUIManager`: 123;
  - `gUserInterface`: 97;
  - `gContinentManager`: 76;
  - `gUnitClassTable`: 70.

  `EEPlayer.h` is a second god header: it also declares the tech tree and `gTechTree`.
- **Most include edges are noise.** Nearly every `.cpp` opens with the same ~40-header preamble. Real symbol use is
  much narrower: all 10 Formations files include UIForm and SESoundManager without using them. Before taking a
  module across, measure its real surface: the symbols its objects import, from `llvm-nm` on its objects, not its
  `#include`s.
- **WON sits beside the knot, not in it, as far as the source shows.** The game calls the lobby through about ten
  `EELobby*` functions. The other direction is wider: `EELobbyInterface.cpp` creates and drives `EEServer` and
  `gCommunications` itself. 538 WON files open with `lle/LowLevelEngine.h` and `ee/EEWonNet.h`, the same preamble
  noise as above, so its real surface still needs the `llvm-nm` measurement.

### 1.3 The seams that exist today

| Seam | Shape | Rust-ready? |
|---|---|---|
| `include/platform/Plat*.h` | `extern "C"`, `<stdint.h>` types, opaque handles; 11 headers, ~195 functions | **Yes**, as is (`#[no_mangle] extern "C"`). The tests run against any implementation |
| PIM plug-ins (`include/lle/PIMModule.h`) | Name -> `void*` tables: renderer `'rast'`, network `'ntwk'`, mixer `' snd'`, random map `'rmap'` | Mixer: nearly (36 `SPIM*`, POD and opaque pointers). The others pass C++ objects: `UString&`, `TCPServer**`, `EERandomMap*` (vectors of vectors), `GERasterizer*` |
| GX (`include/gx/GX.h`) | COM-style C++ classes (DirectDraw 7/D3D7 subset) over webgpu.h, behind one lock; ~15 caller files | Needs C++ shim classes forwarding to `extern "C"` (thin: ~60 methods) |
| GERasterizer / GEScreen / GESurface / GEVertexArray | Pure-virtual C++ (54/19/31/15 pure virtuals) | Via C++ trampolines (a vtable implemented by forwarding to Rust) |
| Commands into the simulation | `NEMessage`s through the local server, drained by `EEPhysics::DrainInput` | **A good seam.** The wire format is fixed-width bytes (`ExportMessage`/`ImportMessage`) |
| On-disk records | 37 raw records, 51 `static_assert(sizeof)` pins, `tools/record_sizes.py` | **Yes:** one `#[repr(C)]` mirror each, with the same asserts |
| Saved games | FSChunk tags, field-by-field writes with version gates (`tools/harness/savefile.py` documents it) | The order is the contract, not the layout; a Rust codec can match it |
| `platform/PlatSim.h` | C globals and a per-tick callback (the sim's switchboard) | Yes, but it switches C++ behaviour |
| Lobby frame (`EELobbyLockFrame`/`UnlockFrame`/`SetPresentation`/`GetSurfaceSize`, `RTSGraphics::RenderLobby`) | BGRA pixels, sizes and a present rectangle, pulled by the graphics thread | **Yes**, plain data; only `EELobbyLockFrame`'s C++ reference parameters need flattening |
| WONGUI backend (`WONGUI/Plat*.cpp`, 2.3k; `units.WON_REPLACED`) | WON's WindowManager, Window, Graphics, NativeImage and Font implemented on the platform layer and WON's software rasteriser | A replaceable C++ class boundary, if WONGUI is kept while the rest moves (D8) |
| WON wire messages (`WONBuffer.h`, `ReadBuffer`/`WriteBuffer`) | Little-endian fields, UTF-16 LE wide strings per unit, pinned by `tests/won` | **Yes:** a Rust codec has a byte-for-byte test |

---

## 2. Invariants a Rust replacement must keep

### 2.1 Determinism (simulation code only)

The lockstep sim and every oracle in section 3 rest on a bit-identical simulation. The out-of-sync checksum
(`EEPhysics::GetOutOfSynchChecksum`, `EEPhysics.cpp:249`) is a wrapping `uint32` sum that adds every unit's float
positions **as bit patterns** (`EEComplexUnit::GetChecksum`, `EEComplexUnit.cpp:1896-1911`). A Rust sim component
must reproduce:

- **The same float operations in the same order, in `f32`**, with no fused multiply-add and no reassociation.
  - Rust never contracts on its own. clang does with `-ffp-contract=on` (the default; no build sets it).
  - On x86-64 baseline there is no FMA instruction, so the Windows and Linux builds cannot fuse. **The arm64 macOS
    build can.** Check whether the Mac sim already differs from Windows; set `-ffp-contract=off` for both C++ builds
    before any C++/Rust comparison on arm64.
- **URandom** (`include/lle/URandom.h`): the LCG with 32-bit state, `Generate()` dividing in **double**, and
  `URandomIntegers` rounding with `lrintf`. Draws must happen in the same order and number. The physics stream has
  ~207 call sites.
- **Float-to-int conversion** with `UMFloatToLong` = `lrintf` (round half to even), not Rust's `as` (truncating,
  saturating).
- **libm.** 15 sim files call `sin`/`cos`/`atan2`/`sqrt`. Rust's `f32::sin` lowers to the same CRT/libm call as
  clang's C++ on the same target; the pure-Rust `libm` crate does not give the same bits. Use std, or call the C
  functions explicitly, and never the `libm` crate in sim code.
- **Container iteration order.** No `HashMap` iteration in the sim. Address-ordered sets are a hazard; the one in the
  A* code (`EEAIAStarPathAreaGoal.cpp:282`) turned out harmless, since every visiting order gives the same state
  (PORT_LEARNINGS "R5b.1").
- **Wrapping integer arithmetic** where C++ relies on `-fwrapv`: `wrapping_*`, or `Wrapping<T>`.

Policy proposal: **bit-exact while porting, divergence only at declared rebaseline points.** CLAUDE.md allows the game
to diverge from the original, and multiplayer compatibility with retail is gone. But while code moves language, the
bit-exact trace is the only cheap way to catch a porting mistake. A deliberate behaviour change lands as its own
commit, with a statistical gate (`sim_bench.py`, `ai_scorecard.py`) and new reference traces, never mixed with a port.

### 2.2 Memory and threads

- **Unit hierarchy:** `UReferenceCounter -> RTSUnit -> EEUnit -> EEComplexUnit`, with 137 virtuals in `EEUnit.h`.
  Units come from a pool whose slots are reused at once. The bugs fixed this week (a freed slot reborn as a
  projectile inside an AI sub group) show the cost.
- **Reference counts are intrusive and not atomic.** Safety rests on the world lock: one `TSReadWriteLock`. The
  physics tick writes under it; the graphics thread renders straight from the live objects under the read lock; the
  UI updates under the read lock.
- **Rust cannot own an object that C++ still holds by raw pointer, calls virtuals on and `dynamic_cast`s.** Ownership
  can only move when the C++ users hold a handle. The ids exist already: saved games store 32-bit `USavedID`s and
  rebuild pointers on load.
- **No Rust reference may live across a call back into C++,** because C++ may reach the same object. Rust code
  called under the world lock receives `&mut` derived from the lock contract and drops it before returning.
- **The lobby runs inside a posted UI action and owns the UI thread while it is open.** `EEUserInterface::ProcessActions`
  holds the UI copy lock and the world write lock around every action, so `EELobbyRunLoop` releases both and takes
  them back in that order; otherwise the graphics thread blocks in `EEGraphics::FrameStarting`. The frame's lock
  order is `EELobbyLock`, then the frame lock. A replacement keeps both rules or removes the nesting.

### 2.3 Text

`UString` is Windows-1252 bytes; CJK and the encoding decision are open (PORT.md step 6). Until that decision, Rust
treats game text as bytes (`&[u8]`/`Vec<u8>`, or a `GameStr` newtype) and converts at the platform edge, which SDL3
already does in UTF-8. Do not map `UString` to Rust `String`.

WON's text is different: `GUIString` and `wchar_t` in memory, cp1252 at the platform calls, and UTF-16 LE code units
on the wire and in WON's files (`wchar_t` is 4 bytes on Linux and macOS, which is why `WideAppendUtf16LE` exists).
Rust keeps those as `u16` units until they reach a display, because a name from the wire need not be valid UTF-16.

### 2.4 Observability

The **Windows harness** depends on the C++ layout:

- `tools/harness/layout_gen.py` dumps clang record layouts, and `game_state_probe.py` reads `gWorld`/`gPhysics`/
  `EEPlayer` members by offset;
- `exe_boot.c` and `hbuild.py` hook mangled names: `?Generate@URandom@@QEAAMXZ`, `?NextInSeries@URandom@@AEAAKXZ`,
  `?gWorld@@3VEEWorld@@A`, `?ShowScene@DX7Rasterizer@@...`, `?Enter@TSCriticalSection@@...`.

Everything Rust owns is invisible to that harness. The layout-independent observation points survive any language:

- the `EE-AI` trace;
- the `EE-GAME` game log;
- the EE-SIM JSON report;
- `.ees` save diffs;
- the `EE_TEST_UI` fixture (`ui-state.txt`/`test.command`, used by `posix_game.py`);
- OOS dumps.

The fixture does not acknowledge commands while the lobby is open, because the lobby owns the UI thread. The
harness's `--exercise lobby` waits for presented frames instead, so a lobby replacement must keep presenting frames
or give the fixture a real way in.

**Move the gates onto these before R5.** R1-R4 can keep exporting the mangled C++ shims the harness hooks.

---

## 3. Existing work that carries the rewrite

| Asset | Compares | Use for Rust |
|---|---|---|
| EE-SIM + `tools/sim.py`, `sim_bench.py`, `tools/harness/ai_trace.py compare` | Seeded games: trace, report, first difference | **The simulation oracle.** A mixed C++/Rust build must print the reference trace for seeds 2-7 over 2700 s (with the autosave off). Today it pins down the first divergent *decision*; a per-CT checksum line (section 5) would pin down the first divergent *tick* |
| `eedata.py check` / `cppgate`, `EEDataDump` | Text -> records, content dump parity (floats as bits) | A Rust text compiler is gated by an identical dump and identical record bytes |
| `tools/eedata/specs.json` (38 tables, typed fields with offsets and sizes) + `cppgen.py` | Generates the C++ specs and `EEWorldConstants.h` | Add `rustgen.py` with the same `--check` gate: generated `#[repr(C)]` + serde structs, never hand-maintained twice (MODDING_PLAN §3) |
| `eemodel.py cppgate` | glTF/PNG -> .cem/.sst bytes, C++ vs Python | Gates a Rust model loader the same way |
| `tests/platform` (plat_test, font_test), `tools/gx_test`, `tools/sdl_test`, `tools/tcp_test` | Self-checks of a C API, exit code = failures | Run unchanged against the Rust implementation (link swap) |
| `tools/record_sizes.py`, the `static_assert` pins | Record sizes | Mirror each assert in Rust (`const _: () = assert!(size_of::<T>() == N)`) |
| `tools/harness/posix_game.py` + `EETestFixture.cpp` | Live game through the UI fixture | The layout-independent live harness; port its Windows half before R5 |
| `tests/won` (`xbuild.py wontest`: `won_wire_test`, `won_socket_test`) | WON's wire format byte for byte, its socket layer over loopback | Gates R1's PlatSocket and any Rust WON codec. Not part of `gates.sh` today. `won_gui_test.cpp` (the GUI backend's GDI rules, offscreen) is in the tree, but `wontest` does not build it |
| Harness `--exercise lobby` (`--refuse-autodial`) | Main menu -> MultiPlayer -> the lobby's frames -> Other Multiplayer -> the LAN screen | Smoke test of a lobby replacement. **It plays no game:** a LAN or Direct Connect game between two instances through the lobby is still untested (`mp_game.py` bypasses the lobby), and that test must exist before the lobby moves |
| wgpu-native in the exe (`tools/build.py:127`) | | **Precedent:** a Rust staticlib already links with lld-link and `/MT`, with the system libraries Rust std needs |
| MODDING_PLAN / AI_RESEARCH choices | | JSON-shaped canonical data "read by C++ and Rust alike"; strict YAML; `f32::from_str` float parsing; Lua through mlua; deferred caps (16 players, 6 armour kinds, 7 resources) planned for Rust |

---

## 4. Interop mechanics

**One Rust static library.** `wgpu_native.lib` already carries a copy of Rust std. A second, independent Rust
staticlib brings another copy, and the build rejects duplicate symbols. So:

- `rust/` is a cargo workspace with one `staticlib` crate (`ee_rust`) that depends on every Rust crate;
- in R0 it absorbs wgpu-native: pinned to the v24.0.3.1 tag as a git dependency, built from source, so the
  webgpu.h ABI GX uses is unchanged;
- later, GX in Rust uses the `wgpu` crate directly and the C API goes.

Before R0 lands, verify that a dependency's `#[no_mangle]` exports survive into the staticlib (they should, because
a staticlib carries all object code of its dependencies), and that the link reports `0 unresolved, 0 duplicates`.

**Profile:**

- `panic = "abort"`, plus a panic hook that writes to the game log and `ESRecordNativeError`, because a GUI exe has
  no stderr (PORT_LEARNINGS "wgpu errors are Rust panics");
- `-C target-feature=+crt-static` on MSVC; `rust-toolchain.toml` pins a stable version;
- `opt-level` matched to the C++ `-O1` for comparability, then free;
- a baseline `target-cpu` (no FMA) for the sim;
- no nightly, so no Rust ASan: the macOS ASan builds instrument only the C++ side. The Rust side gets
  `cargo test` and Miri on its pure crates.

**Binding style, per seam:**

- **Default: plain C ABI.** `extern "C"` in Rust, a header generated by cbindgen and checked in with a
  staleness check, like `cppgen --check`. It matches the existing seams and keeps the exported names stable for the
  harness, and the MS-dialect C++ (`-fms-compatibility`, delayed template parsing) never meets generated C++.
- **cxx** only where an interface really is C++ objects for a long stretch: `UString`, `std::vector` members of
  `EERandomMap`, `GESurface` reference counts. It is the mature choice (KDAB's 2026 comparison). Its generated C++ is
  standard and compiles with clang-cl; verify it once with the dialect flags.
- **Not autocxx/Crubit:** they bind arbitrary C++ headers, and these headers are MS-dialect, macro-heavy and
  reverse-engineered. Crubit also expects Google's build system.
- **C++ trampolines** for the abstract engine classes: a C++ class implementing `GERasterizer` whose methods call
  `extern "C"` Rust functions with an opaque `this`.

**Build integration:**

- `tools/build.py`: `cargo build --release --target x86_64-pc-windows-msvc` before the link, the `.lib` next to
  wgpu in `static_libs()`. `/NODEFAULTLIB` already drops Rust's default-library directives.
- `tools/xbuild.py`: the same, for `x86_64-unknown-linux-gnu` (cross from Windows with the Debian sysroot: cargo's
  linker = clang + `--sysroot`) and `aarch64-apple-darwin`.
- CI: a rustup step. The gates gain `cargo test`, `cargo clippy -D warnings` and the cbindgen staleness check.
- **Sources are staged under `C:\EEX\`.** Cargo builds from `rust/` in place with its own `target/` under `build/`
  (per worktree; `EE_ORIG_SRC_ROOT`'s rule applies to `CARGO_TARGET_DIR` too).

**Allocation:** whoever allocates frees. A Rust object given to C++ is an opaque pointer with an explicit
`*_destroy`. A C++ object seen from Rust is an opaque handle, never a struct mirror, unless it is a pinned record.

---

## 5. The plan

Each step: one concern per commit, the CLAUDE.md gates, a PORT_LEARNINGS entry, the C++ it replaces deleted in the
same branch (no long-lived dual implementations beyond the comparison period).

**Everything written in Rust is unit tested (the owner's rule).** Every module lands with its tests, in the same
commit, in every phase:

- Tests cover each module's public behaviour, its edge cases and its error paths, and run under `cargo test` with no
  game, data tree or window. Code that seems to need those is split so that its logic can be tested alone, with the
  I/O or the FFI as a thin layer around it.
- Behaviour carried over from C++ is pinned by golden values captured from the C++ before it is deleted (RNG
  sequences, float-to-int rounding, record bytes, wire messages, map dumps).
- A bug fix comes with a test that fails without it.
- The oracles in section 3 (the sim trace, `cppgate`, the tours) are additional. They show that the whole game still
  agrees; they do not replace a module's tests.

### R0: the Rust library (foundation)

- `rust/` workspace, `ee_rust` staticlib, wgpu-native from source at v24.0.3.1, panic hook, cbindgen, build.py /
  xbuild.py / CI integration.
- Gates: both Windows builds link 0/0; the full gates (`gates.sh`); a macOS build plus an ASan `ai-game` run;
  `gx_test` and `sdl_test` pass. The exe behaves identically: same AI trace in EE-SIM.

### R1: platform leaves (prove the seam on the easiest code)

1. **PlatConfig** (7 functions, 1 caller, its own YAML subset, tests exist): Rust's first real code. The parser is a
   hand-written strict subset; no `serde_yaml`, which is archived.
2. **PlatPath, PlatString** (23 functions, 42 callers; the multibyte `_mbs*` semantics need care), **PlatTime,
   PlatMessage.**
3. **PlatFile, PlatThread, PlatSocket:** one Rust implementation for all three systems in place of
   `src/Platform/{Win32,Posix}`, with the Windows-only differences in `cfg(windows)`.
   - Constraint: the harness's time shim hooks the Win32 imports PlatTime calls, so the Rust PlatTime must call the
     same Win32 functions until the harness clock moves to PlatSim (section 5 pre-work).
   - WON is now a large PlatSocket and PlatThread client: `PlatSocketWait`/`GetError`/`Shutdown`, the socket options
     and thread wait were added for it, and its `WSAE*` names are the `PLAT_SOCK_*` values, so those numbers are
     part of the contract. PlatString's `PlatCp1252ToWide`/`PlatWideToCp1252` move with step 2.
4. **PlatWindow** on winit (PORT.md already names it) or kept on SDL3 through `sdl3-sys`. Decide on the evidence:
   SDL3 works on all three systems today.
5. **PlatFont** (stb_truetype -> `fontdue`/`ab_glyph`, or `swash`). The lobby's text goes through it too
   (`PlatGUIFont.cpp`, rasterised at size x the pixel scale). Gate: font_test and menu and lobby screenshots within
   the hinting tolerance.

Gate: `tests/platform` and `xbuild.py wontest` on all three systems with the Rust implementation, the tour,
`--exercise lobby`, an AI game.

### R2: the data layer

1. Pre-work: a machine-readable **rules schema** (today it exists only as C++ name tables in `EERules.cpp` and
   `tools/eedata/rules.py`). This is MODDING_PLAN's JSON Schema step, and both languages then generate from it.
2. `rustgen.py`: the 38 table specs -> `#[repr(C)]` record mirrors plus serde text structs, with `--check`.
3. **The text compiler in Rust** (`EEDataText`, `EEDataTextFile`, `EERules`, `EEDataDump`; ~4k lines). C++ calls
   `ee_data_compile(table, text, out)` and receives the same record bytes the C++ compiler gives.
   - Gates: `cppgate` (dump and byte parity) and `eedata.py check`.
   - Then the mod layer (MODDING_PLAN phase 3) is written in Rust from the start.
4. **Open formats:** `GEOpenFormats` (glTF -> .cem bytes, PNG -> .sst) in Rust (crate `ee_openformats`: hand ports of
   cgltf's and stb_image's readers, not the `gltf` and `png` crates, which accept and refuse differently), replacing
   cgltf and stb_image. Gate: `eemodel.py cppgate` (Rust vs Python and the C++ golden digests; done, see PORT_LEARNINGS
   "R2.4"; full-game gates not re-run).

### R3: engine services and plug-ins

Ordered by isolation:

1. **Mixer plug-in** (36 `SPIM*`; cpal or keep miniaudio through FFI). The first PIM plug-in in Rust.
2. **Compression (PKWARE DCL) and UEncryption:** pure, tiny, golden-testable against the save files.
3. **TCP Protocol plug-in** (33 `NPIM*` on PlatSocket). `tcp_test` is the gate; a C++ shim converts `UString`.
4. **Random map generator** (`Default`, 5.7k, deterministic). The entry points mutate `EERandomMap`, a struct of
   vectors and `UString`s.
   - Invert the interface: Rust generates into flat arrays that a C++ adapter copies into `EERandomMap`.
   - Gates: a map dump identical for every map script x seeds; the sim trace unchanged.
   - The `.rmv` script reader (`EERandomMap.cpp`) follows.
5. **File System:** `FSArchive`, `FSChunk`, `FSMemoryFile`, behind the existing classes (fan-in 911 includes, so
   the C++ facade stays until R6).
6. **Sound Engine** (SE*), then music (dmusic stays C until a Rust DirectMusic player exists) and movies (pl_mpeg ->
   a Rust MPEG-1 decoder or keep C).

### R4: the renderer

1. **GX in Rust on the `wgpu` crate (done: `rust/crates/ee_gx`).** GX's contract is ~60 methods with DirectX 7 semantics: surfaces with
   CPU/GPU copies, colour keys, the fixed-function WGSL shader. C++ keeps `GX.h` as shim classes. The webgpu.h C
   API and the wgpu-native pin go.
   - Gates: `gx_test`, the tour's screenshots, `EE_GX_STATS` summaries.
2. **The DX7 renderer plug-in** (9.7k; in progress on `ee_dx7`, PORT_LEARNINGS "R4.2") behind `GERasterizer`/`GEScreen`/`GESurface`/`GEVertexArray`
   trampolines. Then a choice: port it faithfully, or redesign the renderer in Rust from the GX contract
   (PORT_LEARNINGS "GX"). Either way the lobby's path stays: a system-memory surface with a 1024x768 logical size over
   the frame's pixels (`GESurface::SetLogicalSize`), letterboxed onto the back buffer by `RTSGraphics::RenderLobby`.
3. **The Graphics Engine** (GE*: models, textures, fonts, cameras; 17k) last in this phase. It is shared with the
   game's renderer (Graphics Thread), so it may move with R5's renderer decoupling instead.

### R5: the simulation

Two stages. The first is in C++ and changes no behaviour (each step gated by the sim trace); the second is the move.

**R5a. Untangle in C++:**

1. **Per-CT checksum trace** in EE-SIM: `GetOutOfSynchChecksum` and the RNG seed every CT, and an
   `ai_trace.py`-style compare. The oracle then names the first divergent tick, not only the first divergent
   decision.
2. **Make saving free of side effects** (`EECPCivilizationManager::Save` sets `mBuildingCountsChecked`;
   PORT_LEARNINGS "The headless simulation"), so games with autosaves stay comparable.
3. **The renderer reads a snapshot,** not the live world under the read lock. Graphics Thread gets a per-frame view:
   positions, animation states, selection. This cuts the Graphics <-> Units/Physics edges and is the precondition
   for Rust-owned units. It also lets the physics tick stop sharing reference counts with the render thread.
4. **Handles instead of pointers** at module boundaries: the AI, the UI and the editor hold unit ids (the `USavedID`
   machinery). Measure `EEComplexUnit*` across module boundaries first.
5. **Name the address-named globals** and fold the reachable ones into context objects (the world, the physics).
6. **The RTS base layer stops including the game** (146 edges RTS -> Graphics Thread): merge RTSWorld/RTSUnit into
   their EE subclasses, or invert the edges.
7. Move the gates to layout-independent observation (section 2.4): the Windows fixture driver for `gates.sh`, and the
   harness clock through PlatSim instead of import hooks.

**R5b. Port, leaves first, each behind a C ABI with plain-data inputs and outputs, gated by the trace:**

1. **Formations** (1k: geometry in, slots out), then **A* pathfinding** (5.9k) over a terrain/occupancy grid
   snapshot, then the continent and choke-point analysis.
2. **Combat and unit formulas** (the rules documents are already data: `combat.yaml`), the tech tree and events
   (dbevents/dbeffects are already a small effect language), calamities.
3. **The computer player** (63k). This should meet the AI rework:
   - the AI already runs inside the physics tick and talks to the world through managers;
   - AI_RESEARCH plans a narrow, C-shaped script API (Lua through mlua);
   - the new-AI work could target that API and Rust instead of growing the C++ managers. See decision D2.
4. **The world and the units themselves:** Rust owns the unit store (struct-of-arrays or an ECS, ids as handles),
   the physics tick, the command queue (`NEMessage` decode in Rust) and the save/load codec. The C++ that is still
   left (UI, editor) reads through the snapshot and the id API. This is where the 638 `dynamic_cast`s turn into
   unit-kind data.

   Lifting the deferred caps (16 players, 6 armour kinds, 7 resources: MODDING_PLAN §4) belongs here, as rebaselines.

### R6: UI, events, editor

The UI toolkit (14.4k, forms from the UI db tables), the in-game UI (36.5k) and the scenario editor and manager
(39.5k). These are large and not deterministic: their gates are the tour, the fixture-driven menu checks and
screenshot comparisons. Port the toolkit faithfully (the forms are data); an immediate-mode replacement is a design
change, not a port.

**The WON lobby** is a separate job with its own toolkit: WONGUI is not the UI* toolkit, and porting one does not
port the other. It blocks nothing in R0-R5. It only needs its platform calls (R1), its surface path (R4) and
`EEServer`/`gCommunications` to stay reachable from `EELobbyInterface.cpp` (R5). The scope is decision D8.

Pre-work, whichever way D8 goes:

1. A LAN and a Direct Connect game between two instances through the lobby, as a harness exercise (PORT.md "Other next
   steps"). It is the only oracle for what the lobby must keep doing.
2. A run on Linux and macOS through the lobby's screens.
3. The `llvm-nm` surface of the WON objects (section 1.2), which also shows how much of the 32k service code the LAN
   path pulls in. `LanLogic` uses `AuthContext` and the peer-auth ops, so some of WONAuth and WONCrypt is on that
   path even without the online services.

### R7: the end

- `main` in Rust;
- the last vendored C (zlib/libpng/libjpeg -> `flate2`/`png`/`jpeg-decoder`, miniaudio -> cpal, dmusic, pl_mpeg)
  replaced or kept behind `-sys` crates (WON's PNG and JPEG decoders use the same codecs, so they go first or
  together);
- the clang toolchain becomes optional, then gone;
- build.py and xbuild.py become `cargo build`, and the harness keeps only its layout-independent drivers.

### Size and order of value

| Phase | C++ replaced | Main risk | Value even alone |
|---|---|---|---|
| R0 | none | linking (std duplication) | the pipeline |
| R1 | ~8k | Win32 hook compatibility with the harness | one platform implementation for 3 systems |
| R2 | ~5k | none (strong gates) | the mod layer in Rust |
| R3 | ~25k | interface inversion | plug-ins and services |
| R4 | ~30k | D3D7 semantics | the renderer on the wgpu crate; no webgpu.h pin |
| R5 | ~190k | determinism, ownership | the game |
| R6 | ~90k, plus WON's 131k ported or retired (D8) | fidelity (no exact oracle) | |
| R7 | rest + vendored C | | total Rust |

---

## 6. Decisions for the owner

- **D1. Bit-exact policy** (section 2.1): exact while porting, rebaseline only in its own commits. Proposed: yes.
- **D2. Where new AI code goes.** The new-ai branch grows the C++ managers now. Options:
  - (a) keep C++ and port it in R5b.3;
  - (b) give the new AI a narrow API now (AI_RESEARCH's script API) and write new managers in Rust behind it;
  - (c) Lua first, as planned, which survives the port as is.

  Proposed: (c) for tuning, with (b) as the host of the API.
- **D3. Binding style:** C ABI + cbindgen by default, cxx only where named. Proposed: yes.
- **D4. Window and font backends in R1:** keep SDL3 (proven on three systems) or move to winit. Proposed: keep
  SDL3 through `sdl3-sys` until R7.
- **D5. Renderer in R4:** a faithful port of the DX7 plug-in, or a redesign from the GX contract.
- **D6. Text encoding: decided (owner, 2026-11-09): UTF-8 all the way.** Game text becomes UTF-8 in memory, in
  files and in saves; Windows-1252 survives only as an import conversion for old inputs. It is a game change, done as
  its own track, not mixed into a port: the data converters write UTF-8, `UString` holds UTF-8, PlatFont and the
  text layout work in code points, saves get a version gate (older saves converted on load), and WON's UTF-16 wire
  strings convert at the wire. Until that track lands, ported Rust keeps game text as bytes behind one text module,
  so that the switch is a change in one place.
- **D7. Lockstep across systems:** with `-ffp-contract` unset, arm64 may already disagree with x86-64. Decide
  whether cross-system multiplayer is a goal; if so, set `-ffp-contract=off` now and add a Mac-vs-Windows sim trace
  comparison.
- **D8. The WON lobby: decided (owner, 2026-11-09): port all of it for now (option a), except the HTML rendering**
  (the WONLobbyHTML* segments and parsers, HTMLParser2, ~6k lines), which is not ported: where the lobby shows HTML
  (news, help, messages) it shows plain text instead. The online services are ported with the rest even though their
  servers are gone.

## 7. First steps

1. R0 on a branch: the workspace, wgpu-native from source, the link at 0/0, CI.
2. The cheap oracles, independent of Rust and useful now:
   - the per-CT checksum trace (R5a.1);
   - the side-effect-free save (R5a.2);
   - `-ffp-contract=off` with a Mac/Windows trace comparison (D7).
3. PlatConfig in Rust (R1.1) as the first code across the seam.
4. `rustgen.py` from `specs.json` (R2.2): it costs little and makes every later phase's records free.
