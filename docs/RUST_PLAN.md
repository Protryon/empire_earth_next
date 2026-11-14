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

| Step | State (details: docs/progress/, one file per lane) |
|---|---|
| R0 Rust library | Done |
| R1.1-R1.3 platform leaves, files, threads, sockets | Done (`ee_plat_core`): [progress/r1-1-r1-3-platform-leaves-files-threads-sockets.md](progress/r1-1-r1-3-platform-leaves-files-threads-sockets.md) |
| R1.4-R1.5 window, fonts | Done (`ee_plat_ui`, `ee_plat_sdl`): SDL3 kept (D4), stb_truetype ported line by line, 51,520 glyphs byte-equal |
| R2.1-R2.3 rules schema, rustgen, text compiler | Done (`ee_data`) |
| R2.4 glTF and PNG | Done (`ee_openformats`), 7,150 of 7,150 gate cases equal to the C++ digests |
| D9 models and textures at run time | Done: [progress/d9-models-and-textures-at-run-time.md](progress/d9-models-and-textures-at-run-time.md) |
| D9 open formats only, the data side | Done: [progress/d9-open-formats-only-the-data-side.md](progress/d9-open-formats-only-the-data-side.md) |
| R3.1-R3.3 mixer logic, compression, encryption, TCP | Done (`ee_mixer`, `ee_codec`, `ee_net_tcp`). miniaudio stays; no audio has been listened to through the Rust mixer |
| R3.4 random maps | Done (`ee_rmap`), 2,088 of 2,088 maps equal to the C++ digests. The `.rmv` reader (`Unit10.cpp`) stays C++ |
| R3.5 File System | Done (`ee_fs`) behind the C++ facades |
| R3.6 Sound Engine | Logic done (`ee_sound`): [progress/r3-6-sound-engine.md](progress/r3-6-sound-engine.md) |
| R4.1 GX on the `wgpu` crate | Done (`ee_gx`); wgpu-native is gone |
| R4.2 DX7 plug-in | Done (`ee_dx7`), a faithful port (D5) with C++ trampolines. C++ keeps the classes, object lifetimes, `Set3DMode`'s matrix composition, the PIM table and pl_mpeg |
| R5b.1 formations | Done (`ee_formations`): the C++ classes keep their members and call Rust for the geometry. 14,068 `Place` calls captured from the C++ in the six oracle games (`tools/sim_capture.py`: real games plus a sweep of every formation type over real units) equal bit for bit |
| **R5b.1 A* pathfinding, continents, choke points** | **In progress** (`ee_astar`): [progress/r5b1-a-pathfinding-continents-choke-points.md](progress/r5b1-a-pathfinding-continents-choke-points.md) |
| **R5b.2 tech tree, events, calamities** | **In progress** (`ee_tech`): [progress/r5b2-tech-tree-events-calamities.md](progress/r5b2-tech-tree-events-calamities.md) |
| **R4.3 Graphics Engine (17k)** | **In progress** (`ee_ge`): [progress/r4-3-graphics-engine-17k.md](progress/r4-3-graphics-engine-17k.md) |
| **R4.4 Graphics Thread (27.8k)** | **Started** (`ee_gfx`): [progress/r4-4-graphics-thread-27-8k.md](progress/r4-4-graphics-thread-27-8k.md) |
| **R6 LLE User Interface toolkit (14.4k)** | **Started** (`ee_ui`): [progress/r6-lle-user-interface-toolkit-14-4k.md](progress/r6-lle-user-interface-toolkit-14-4k.md) |
| **R5b.2 combat and unit formulas** | **Done, second part** (`ee_combat`): [progress/r5b2-combat-and-unit-formulas.md](progress/r5b2-combat-and-unit-formulas.md) |
| **LLE whole classes (NE, FS, Utilities, Sound, TS/TM/ES, Input)** | **Started**: [progress/lle-whole-classes-ne-fs-utilities-sound-ts-tm-es-input.md](progress/lle-whole-classes-ne-fs-utilities-sound-ts-tm-es-input.md) |
| **R6 game screens (ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¾Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â°ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â‚¬Å¾Ã‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¹ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Â¦Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â 60k lines on the toolkit)** | **Started** (`ee_screens`): [progress/r6-game-screens-60k-lines-on-the-toolkit.md](progress/r6-game-screens-60k-lines-on-the-toolkit.md) |
| R5a.1 per-CT checksum trace | Done: `EE-SIM --checksum-trace`, `sim_oracle.py run --checksums` and `ticks` (the first divergent tick). The option leaves the traces of seeds 2-7 equal to the reference |
| R5a.2 side-effect-free save | Done: [progress/r5a2-side-effect-free-save.md](progress/r5a2-side-effect-free-save.md) |
| R6 WON leaves (D8) | Done (`ee_won`): [progress/r6-won-leaves-d8.md](progress/r6-won-leaves-d8.md) |
| R6 WON HTML (D8) | Removed: the HTML parser, documents and the segment layout engine; the lobby's pages (MOTD, terms of use, help, news) are plain text in a `TextArea` (`ee_won` plaintext) |
| **R6 WON, the rest** | **In progress** (`ee_won`): [progress/r6-won-the-rest.md](progress/r6-won-the-rest.md) |
| **R6 WON, Internet code** | **In progress** (`ee_won`): [progress/r6-won-internet-code.md](progress/r6-won-internet-code.md) |
| **R6 WON, the lobby screens** | **Started** (`ee_won`): [progress/r6-won-the-lobby-screens.md](progress/r6-won-the-lobby-screens.md) |
| **R5a.7 gates on layout-independent observation** | **Done, gate runs pending**: [progress/r5a7-gates-on-layout-independent-observation.md](progress/r5a7-gates-on-layout-independent-observation.md) |
| **R5a.4-R5a.6 handles, names, the RTS layer** | **Started**: [progress/r5a4-r5a6-handles-names-the-rts-layer.md](progress/r5a4-r5a6-handles-names-the-rts-layer.md) |
| D6 game text is UTF-8 | Done: [progress/d6-game-text-is-utf-8.md](progress/d6-game-text-is-utf-8.md) |
| Fix: dead fishing boats in the AI's gatherer lists | Done: [progress/fix-dead-fishing-boats-in-the-ai-s-gatherer-lists.md](progress/fix-dead-fishing-boats-in-the-ai-s-gatherer-lists.md) |
| **R5b.4 world and units** | **Started**: [progress/r5b4-world-and-units.md](progress/r5b4-world-and-units.md) |
| **R5b.5 scenario manager and triggers** | **Started** (`ee_scenario`): [progress/r5b5-scenario-manager-and-triggers.md](progress/r5b5-scenario-manager-and-triggers.md) |
| **R5a.3 the renderer reads a snapshot** | **Done for units, players, the map and the camera**: [progress/r5a3-the-renderer-reads-a-snapshot.md](progress/r5a3-the-renderer-reads-a-snapshot.md) |
| Merge of main 79be8619 (the Lua AI) | Done: [progress/merge-of-main-79be8619-the-lua-ai.md](progress/merge-of-main-79be8619-the-lua-ai.md) |
| **R5a.8 the AI host (Lua) and bindings** | **Started** (`ee_ai`): [progress/r5a8-the-ai-host-lua-and-bindings.md](progress/r5a8-the-ai-host-lua-and-bindings.md) |
| **LLE Network Engine whole classes** | **Started** (`ee_net`): [progress/lle-network-engine-whole-classes.md](progress/lle-network-engine-whole-classes.md) |

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
  - RTS -> Graphics Thread (146): the base layer includes the game. Measured by symbols (R5a.6), the RTS objects
    import one Graphics Thread symbol, the RTS class's own `RTSMouse::EndFrame`; the edges were the RTS classes
    declared in game headers.
- **Global state.** 224 `g*` names are declared in headers and ~459 are defined in `src`; the
  simulation's address-named ones are named since R5a.5 (`gU4E8708` is `gDamageTable`); the UI's, the editor's and the
  renderer's are not yet. The ones touched most:
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

`UString`, `UWideString` and the records' `char[]` fields hold UTF-8 (decision D6, done on branch `rust-d6-utf8`;
PORT_LEARNINGS "D6: game text is UTF-8"). Rust keeps game text as bytes (`&[u8]`/`Vec<u8>`): a stray byte is one
character of its own (`ee_plat_core::utf8`), so a `String` would refuse what the game must carry. Characters are code
points everywhere a character is counted (`PlatMbs*`, the UI's stepping and wrapping, the fonts' glyphs); old
Windows-1252 inputs (saves before 0.255, files without a version) are converted where they enter.

WON's text is different: `GUIString` and `wchar_t` in memory, UTF-8 at the platform calls, and UTF-16 LE code units
on the wire and in WON's files (`wchar_t` is 4 bytes on Linux and macOS, which is why `WideAppendUtf16LE` exists).
Rust keeps those as `u16` units until they reach a display, because a name from the wire need not be valid UTF-16;
the game's bytes meet them in `ee_won`'s `text.rs` (`StringUtil.cpp`).

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
Done in R5a.7: the gates run `tools/harness/fixture_game.py` (fixture, stderr, game log, window captures) and the
harness clock is `PlatSimSetTimeFactor`; only `gates.sh --legacy` and the debugger tools still need the layout.

---

## 3. Existing work that carries the rewrite

| Asset | Compares | Use for Rust |
|---|---|---|
| EE-SIM + `tools/sim.py`, `sim_bench.py`, `tools/harness/ai_trace.py compare` | Seeded games: trace, report, first difference | **The simulation oracle.** A mixed C++/Rust build must print the reference trace for seeds 2-7 over 2700 s. `compare` pins down the first divergent *decision*; the per-CT checksum lines (`run --checksums`, `ticks`; R5a.1) pin down the first divergent *tick* and what differs there. Saving no longer changes the game (R5a.2), so `--autosave-ms` runs are comparable too |
| `eedata.py check` / `cppgate`, `EEDataDump` | Text -> records, content dump parity (floats as bits) | A Rust text compiler is gated by an identical dump and identical record bytes |
| `tools/eedata/specs.json` (38 tables, typed fields with offsets and sizes) + `cppgen.py` | Generates the C++ specs and `EEWorldConstants.h` | Add `rustgen.py` with the same `--check` gate: generated `#[repr(C)]` + serde structs, never hand-maintained twice (MODDING_PLAN §3) |
| `eemodel.py cppgate` | glTF/PNG -> .cem/.sst bytes, C++ vs Python | Gates a Rust model loader the same way |
| `tests/platform` (plat_test, font_test), `tools/gx_test`, `tools/sdl_test`, `tools/tcp_test` | Self-checks of a C API, exit code = failures | Run unchanged against the Rust implementation (link swap) |
| `tools/record_sizes.py`, the `static_assert` pins | Record sizes | Mirror each assert in Rust (`const _: () = assert!(size_of::<T>() == N)`) |
| `tools/harness/posix_game.py` + `EETestFixture.cpp` | Live game through the UI fixture | The layout-independent live harness; port its Windows half before R5 |
| `tests/won` (`xbuild.py wontest`: `won_wire_test`, `won_crypto_test`, `won_socket_test`) | WON's wire format byte for byte, its socket layer over loopback | Gates R1's PlatSocket and any Rust WON codec. Not part of `gates.sh` today. `won_gui_test.cpp` (the GUI backend's GDI rules, offscreen) is in the tree, but `wontest` does not build it |
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
   decision. (Done: `EE-SIM --checksum-trace`, `sim_oracle.py ticks`; PORT_LEARNINGS "R5a.1 and R5a.2".)
2. **Make saving free of side effects** (`EECPCivilizationManager::Save` set `mBuildingCountsChecked`;
   PORT_LEARNINGS "The headless simulation"), so games with autosaves stay comparable. (Done: that one and three
   more, in the gather goal, the wall manager and the defense commander.)
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

#### R5b.4: the map (measured 2026-11-10, branch `rust-r5b4-core`)

**`dynamic_cast` sites** (657 in code under `src/` and `include/`, 683 with comments; the 638 above is an older
count), by the class they test:

| Tested class | Sites | Becomes |
|---|---|---|
| unit goals `EEUG*` (13 classes; `EEUGAttackUnit` 58, `EEUGLocation` 34, `EEUGGather` 27) | 191 | the goal kind: `EEUnitGoal::mKind`, set by each constructor, never written after (`EEUGHealUnit` 15 derives from `EEUGLocation` 1) |
| unit objects `EEComplexUnit` 81, `EEUnit` 8 | 89 | the object kind (simple or complex), fixed at construction |
| unit classes `EEUC*` (`EEUCGate` 24, `EEUCTransport` 17, `EEUCBasicCitizen` 7) | 57 | the class kind: `CreateUnitClasses` builds the class from the record's category alone, and `mCategory` is written only by the constructor |
| players `EEComputerPlayer`, `EEHumanPlayer`, `EEPlayer` | 12 | a player kind |
| AI `EECP*`, `EEAI*` | 55 | held with the AI (R5b.3) |
| UI toolkit and in-game UI (`UI*`, `EEUI*`) | 121 | R6 |
| network (`EEServer`, `EEClient`, `NES*`, `EEMessage`) | 27 | the communications role; the message kind by type |
| other (WON 49, persistent helpers 17, input keys, formations, effects, calamities) | 105 | per module |

By module: AI Thread 162, Units 150, UI Events 92, WON 50, Physics Thread 29, Communications 26, User Interface
Thread 26, Empire Earth 24, Civilization Builder 20, Persistent 17, Scenario Manager 16, Graphics Thread 14, the rest 31.
The three unit tables (goal, object, class: 337 sites) are the simulation's; 97 of them are in the AI Thread.

**Commands** (`NEMessage`):

| | |
|---|---|
| Type ids | 108 game messages (`EEMessageID`, 0x41..0xfd, factory `EEMessage::BuildEEMessage`) and 28 engine messages (`NEMessageID`, 0x20..0x3c, `NEMessage::BuildNEMessage`) |
| Codecs | 133 classes with `ExportMessage`/`ExportSize`/`ImportMessage` (312 functions): 104 game classes in `EEMessage.cpp` (8,961 lines with the `Execute` bodies), 29 engine classes in `NEMessage.cpp`. 94 carry fixed fields only; the others carry wide strings (`UWideString::Export`: u32 length + 2, the bytes, u16 0), the compressed box selection (`CompressUnits`/`DecompressUnits`), vectors (`EEMSelectTheseUnits`, `EEMCivUpgrades`) or nested messages (`NEMPlayerInput`, `NEMCommunicationsTick[Response]`) |
| Encoded | `NEMessageManager` (the send queues), `NESynchronousClient::Send`, `NESynchronousServer`, `NETimestampedMessage`, `NERecordPlayback`, `NEMPlayerInput` (nested) |
| Decoded | `NEMessage::CreateMessage` -> `sConstructors[type]` (the two factories), also inside `NEMPlayerInput::ImportMessage` |
| Sent | 114 `new EEM*` in 47 files: UI events 11 files, the user interface thread 9, actions 6, the editor's actions 3, the computer player once (`EECPWallManager`: walls and gates), EE-SIM's save |
| Executed | `EEPhysics::DrainInput`, per communications tick: the selection types -> `EEMessage::ExecuteSelectionMessage`, the rest -> `ExecuteMessage` (the selection id check) -> `Execute`; engine messages -> `Execute` |

**Order of the leaves:**

1. The game messages' codec: the wire layout of the 104 game classes in Rust (`ee_netmsg`), the C++ classes keeping
   their members and calling it; `CompressUnits`/`DecompressUnits` with it. Gate: an NM** capture of every message the
   games import and a sweep of every class with edge values (this branch).
2. Unit kinds as data: the goal, object and class kinds as Rust tables, the `dynamic_cast` sites as kind queries,
   the simulation's first (this branch).
3. The engine messages and the nested player inputs into the codec; the message kind (game or engine) by type.
4. The `Execute` bodies' decisions (the selection id check, resource arithmetic, the box selection's pick) over the id
   API.
5. The unit store: each unit's plain state (position, hit points, the goal queues as ids) in a Rust struct of arrays
   behind the `EEUnit` accessors; the out-of-sync checksum over it.
6. The physics tick (`EEPhysics::Update`, `DrainInput`, the units' steps) over the store.
7. The save codec, object by object, the 0.256 field order as the contract.
8. The other casts with their modules (AI in R5b.3, UI and WON in R6); then the caps, as declared rebaselines.

**Leaf 4, the `Execute` bodies (measured 2026-11-10):** 108 `EEM*::Execute` bodies in `EEMessage.cpp`, 2,204
lines (the largest `EEMSetRallyGoal` 156, `EEMSetAirportCommand` 104, `EEMPayTribute` 94, `EEMLoadTransport` 72,
`EEMCastCalamity` 69, `EEMPathTo` 68). 67 find the sender by slot (`GetPlayerBySlot`), 44 look units up by id
(`gWorld.GetUnit`), 43 work on the sender's active group, 18 hand out goals, 18 touch resources, 19 call the UI
(`gUserInterface`, the local player only) and 12 the communications object. The two entry points
(`ExecuteSelectionMessage`, `ExecuteMessage`) are the selection id check: under the fast-selection lock, a command
runs only when its selection id equals the sender's last one (`mFastSelection[player]`), else the sender's active
group is reset to the selection; `EEMGameRestarting` always runs, on a client `EEMMigrationSlotInfo` too. In order:
(a) that check and the id bookkeeping as a Rust decision over plain values (the ids, the type, server or client),
the locks and the group copy staying C++; (b) the bodies that only read ids and members and pick (the resource
arithmetic of `EEMAddRemoveResources`, `EEMExchangeResources`, `EEMPayTribute`; the box selection's pick), behind
the unit id API; (c) the goal-giving bodies with the unit store (leaf 5). The bodies that call the UI stay C++
until R6. Gate: the oracle and the capture, plus an NMEX-style record of each body's inputs and decisions.

**Leaf 4, done and left (2026-11-10, third round):** (a) and (b) are Rust (`ee_netmsg` execute.rs; gate NMDC). (c)
waits for the unit store: 15 bodies split the selection by category (`EEActiveGroup::Fill` for 1..5), spread it,
draw one `GetRandom` for the acknowledging unit and give goals (AddPatrolPoint, AttackGround, AttackUnit, BuildUnit,
BuildWall, CastCalamity, DropoffResource, Explore, GatherResource, HealUnit, PathTo, Patrol, RepairUnit,
SmartFarmers, UnloadTransport); 18 construct goals, 24 look units up by id, 22 call the UI (C++ until R6). Their
decisions read units' plain state between world calls that change it, so they need: each unit's plain state by id
from Rust without a call into C++ (category, hit points, built, type, tile, goal kind), the active group as an id
list, goal creation as a host call returning a handle. The box selection's pick (`EEPlayer::SelectUnits`) is the
first once the store exists.

**Leaf 5, the unit store, stage one: the design (2026-11-10, fourth round).** What the store is for first: the
decisions of the selection pick and the 15 goal-giving bodies read units' plain state by id between world calls that
change it, so the state must be current at every point of the tick, not only between ticks.

- **Where the plain state lives.** A Rust-owned record per unit (`ee_units` store.rs, `UnitRecord`: the owner's
  player id, the object kind, whether the unit is its own complex unit, valid, hit points, the tile, the class's
  category and unit type, for a complex unit built and the current goal's kind), keyed by the unit id. Its lifetime
  is the player list's: inserted by `RTSPlayer::InsertUnit` (seeded whole from the object: a new unit, a loaded one,
  a kept id), removed by `RTSPlayer::RemoveKilledUnits` when the unit leaves the list (and by `RTSUnit::FreeNode`
  after a teardown), and a player's records dropped by `~RTSPlayer`, by the player object. That is exactly the domain of `RTSPlayer::GetUnit` / `gWorld.GetUnit` (the ids the bodies
  look up); the registry behind `EEUnitHandle<T>` keeps its own, longer lifetime (to `FreeNode`, where the UI
  thread's last `Dereference` may run) and stays the way to the object. Every store write happens where the C++
  member is written, under the world's write lock (the physics tick, the loads, the editor); the store has its own
  mutex as well, so a read from another thread is never torn.
- **How it stays current: the members become write-through holders.** Each mirrored C++ member becomes an
  `EEUnitStoreField<T, F>` (ee/EEUnitStore.h): the same size and layout as `T` (offsets, `memcpy`, the save code's
  bytes unchanged), read implicitly as `T`, and every assignment form (`=`, `+=`, `-=`, `++`, `--`, holder to
  holder) stores the value and reports it to the store when the unit is in it (`RTSUnit::mInUnitStore`, set by the
  insert, cleared by the removal). The compiler finds every writer (no reference or pointer to a holder converts to
  `T&`/`T*`; a load reads through `EEUnitStoreRead`; a holder is not copyable), so no write but a `memcpy` can bypass
  the store (the hit points' load, before the insert). Derived fields
  are reported at the write that changes them: the class's category and type at `mUnitClass` (a class's own
  category and type are written only by its constructor), the goal's kind at `mGoalState` (fixed by the goal's
  constructor), the complex flag at `mComplexUnit`.
- **The alternative, a per-call view** (C++ fills plain records of the units a decision reads, before each call):
  nothing at the writes, but every decision needs its own filling code, a view goes stale at the first world call
  that changes a unit (`Fill`, `ClearGoal`, `SetGoal`) so an interleaved body must refill, and the C++ members stay
  the truth forever. The holders cost one store call per write and a second copy of each value until the members go;
  they buy a store that is current everywhere and a path to deleting the members: once every reader of a field
  reads the store (Rust, or C++ through `EEUnitStore` accessors), the holder drops its inline value and becomes an
  accessor over the store, and the member goes when the save codec writes from the store (leaf 7).
- **Determinism.** The game only looks records up by id (a `BTreeMap`: no hash order anywhere, and the only walk,
  the check's and a player's reset, is in id order and observes nothing the game reads). The game's own orders (a
  player's list, a group's vector) stay the C++ containers' until the tick moves; then the store gains the per-player
  order as an id list kept at the list's insert and unlink, never the map's order. A write is a plain store: no
  random draw, no float operation (the values are copied as they are, hit points and tiles as the C++ `long`).
- **Saves.** The store is not saved: the format is the save work's (0.262) and does not change. A load rebuilds it,
  since every loaded unit passes `InsertUnit` (seeded from the object as loaded) and the Restore pass writes the
  pointers (`mGoalState`, ...) through the holders.
- **Handles and the store.** A unit handle stays the id; the store is the second thing an id resolves to (the plain
  state, for decisions), the registry the first (the object, for world calls). Rust decisions read the store and
  hand ids back; C++ resolves them for the world calls. Goals have no ids: a Rust decision gets a goal as an opaque
  handle from a host call (`new EEUG*` behind it, one reference owned by the handle), hands it to the host's group
  calls and never dereferences it.
- **The gate.** TEST ONLY, with the simulation capture on: after every physics tick and before every game message
  the physics executes, C++ walks every player's list and hands every unit's fields to the store, which compares them
  with its records and counts the records (a USCK record per check: the units compared, the mismatches; a mismatch
  prints the unit and field and stops EE-SIM with exit code 3). So the store equals the objects on every unit at every
  point where a decision reads it, over the oracle games and the four capture profiles.
- **The path to the tick.** Stage one: the fields above, the active group as ids, goal creation as a host call, the
  pick. Then the fields the bodies' helpers read (the formation slot, mobility, the resource amount), the 15 bodies;
  then positions and the out-of-sync checksum over the store (leaf 5's end); then each unit step moves with the
  fields it writes, the holders turning into accessors as their last C++ readers go, and the physics tick (leaf 6)
  iterates the store's per-player orders.

**Leaf 5, stage one, done (2026-11-10, fourth round).** The store is `ee_units` store.rs (records by id: owner and
player object, object kind, complex flag, valid, hit points, tile, category, unit type, built, goal kind), kept by
eight holders (`RTSUnit::mTileX`, `mTileY`, `mIsValid`, `EEUnit::mUnitClass`, `mComplexUnit`, `mHitPoints`,
`EEComplexUnit::mIsBuilt`, `mGoalState`; ee/EEUnitStore.h); the USCK check found no difference over the 24 capture
games (every tick, every executed message). The world calls are `ee_units` host.rs (a group's units as ids, goals
from an `EeGoalSpec` as owning handles; six constructors), not called in game yet. The box selection's pick is Rust
(`ee_units` select.rs over the store; USPK equal to the C++ pick's goldens in the four profiles); only a played game
reaches its game call. Next: the first of the 15 bodies (Explore is the smallest: the host calls of `EEActiveGroup`'s
`Fill`, `KeepMobileUnits`, `ClearGoal`, `SetGoal`, `Plan`, the pending list, one `GetRandom` draw and the
acknowledgement), then the fields their helpers read (PORT_LEARNINGS "R5b.4, fourth round").

**Leaf 4 (c) and leaf 5, fifth round (2026-11-10).** `EEActiveGroup::Fill`'s test (valid, hit points, the formation
slot) is Rust over the store (`ee_units` group.rs; the store gained the class's domains and land-building flag), for
every `Fill`; its gate USFL (every call, every player's units in every slot every five game minutes) equals the C++
test's goldens. Explore is the first body in Rust (`group::explore` over the host calls of EEUnitHost.cpp: the active
group, `Fill`, the pending list, `KeepMobileUnits`, `ClearGoal`, the goal, `SetGoal`, the action, `Plan`, the draw,
the acknowledgement). No capture game sends a group command (the human idles, the computer players send none), and a
body cannot run in a sweep (it creates goals and gives them to units), so the bodies' call order is gated by unit tests
over a mock host and only a played game runs them. Next, in order: Patrol and AttackGround (the same shape: Fill per
slot, a goal from the host, Plan, the draw), then the bodies that spread a group (`SpreadAroundTile`: the formation
slot), then those that read a target's state (Gather, Repair: the resource amount, `CanTake`); `KeepMobileUnits`
(commandable, the stats' velocity: the stats objects change in place with upgrades, so not a holder) stays a host call.

**Leaf 4 (c) and leaf 5, sixth round (2026-11-11).** Attack Ground, Patrol, Gather and Repair are Rust (`ee_units`
group.rs `attack_ground`, `patrol`, `gather`, `repair` over the host calls of EEUnitHost.cpp, in the original's order
and draw count); the store gained the commandable flag, the formation slot, the class's ground attack flag and the
resource amount (USCK compares them), and KeepGroundAttackers' test reads it. The coverage is a sweep over a dry
host (TEST ONLY, ee/EEUnitHost.h): every host call has a branch that logs and answers without changing the world, and
every five game minutes the Rust bodies and the original bodies' statements (copied from EEMessage.cpp, world calls
through the same host calls, units read on the objects) run for every player over its units and a set of targets
and points; the logs must be equal (USGC, 780-2,340 records a game). The world calls themselves (Fill's sorts,
the keeps but the ground one, the spreads, SetGoal, Plan, the markers, the acknowledgements) are not run by the sweep:
only a played game does. Next, in order: AddPatrolPoint (KeepNonCitizens over the store: commandable, hit points,
category; the patrol groups' creation as host calls), PathTo (the formation slot), AttackUnit and HealUnit (the
targets' kinds), the others with the UI calls.

**Leaf 4 (c), seventh round (2026-11-11).** Add Patrol Point, Attack Unit, Heal Unit and Path To are Rust (`ee_units`
group.rs `add_patrol_point`, `attack_unit`, `heal_unit`, `path_to` over the host calls of EEUnitHost.cpp, in the
original's order and draw count; KeepNonCitizens' test reads the store), each gated by the USGC sweep (its original
statements next to the Rust body over the dry host) and mock-host tests; nine of the 15 goal-giving bodies are now
Rust. The UI fixture gives orders through the real input path (`order`: gather, repair plain and queued, the Patrol
button, Attack Ground, a move; fixture_game.py `--orders`) and judges every unit by the original's rule after the
message executed: the first run of these bodies' world calls in a played game (the lead's). The reconstruction's UI
sends no `EEMAddPatrolPoint` (the original's patrol-point mode is missing; its Patrol button sends `EEMPatrol(-1)`).
Next, in order: the store's on-the-map flag (the out-of-sync checksum's flag 1), BuildUnit and BuildWall (the late
profile's walls run them), DropoffResource and SmartFarmers, then the bodies with UI calls.

**Leaf 4 (c) and leaf 5, eighth round (2026-11-11).** The store's records carry an on-the-map flag (in the world's unit
chain, reported by `RTSUnit::Link`/`Unlink`); USCK checks the chain against it and the checksum's flag 1 over the chain
against the store's sum, and `GetOutOfSynchChecksum` now takes flag 1 from the store (the lockstep check, flag 1 alone,
walks no unit). Dropoff Resource, Build Wall and Unload Transport are Rust (`group::dropoff_resource`, `build_wall`,
`unload_transport`), `KeepCitizens`' test reads the store; twelve of the 15 goal-giving bodies are Rust. Next: Build
Unit (the granary's farms, the placement, the moved units: about eight host calls), Smart Farmers, Cast Calamity
(`CanCastAt`, the casters' split, the goal clones: the calamity lane's classes), then flag 2 (positions, times updated,
the active group, the command) with the tick.

**Leaf 4 (c) and leaf 5, ninth round (2026-11-11).** Build Unit, Smart Farmers and Cast Calamity are Rust (`ee_units`
commands.rs `build_unit`, `smart_farmers`, `cast_calamity` over 28 new host calls in EEUnitHost.cpp: the class read,
the capitol count, `CanProduce`, `EECanPlaceOnTile`, `EEMakeBuilding`, the free farms, the two farm sorts (the C++
`std::sort`s with their comparators, EEMessage.cpp `EESortFarms`), the build orders, the load goal's farm, the drop
sites, `CanTake`, `EEWorkFarm`, the unit goals, `Finish`, the calamity class, `CanCastAt`, `Contains`, the power and
cost, the casters' split, the goal clone); all 15 goal-giving bodies are Rust, each gated by the USGC sweep and
mock-host tests. The store gained each complex unit's location (x, y, z of `mPhysicalModel`, through
`EEUnitStoreModel`'s setters), height, times updated, active group (set or not) and command type (USCK compares them
and checks the checksum's flag 2 over the chain against the store's sum); `GetOutOfSynchChecksum` takes flags 1 and 2
from the store.

**The tick's per-unit step, mapped (2026-11-11).** `EEPhysics::Tick` runs `gWorld.UpdateActiveUnits` (the activate
and deactivate queues into `mUpdateUnits`: appended at the back, erased in place), then, after the weather, the path
manager and the area-effect clock, `UpdateUnits`: every player's `UpdatePlayer`, then `mUpdateUnits[j]->Step()` from
the last unit to the first. `EEComplexUnit::Step` is `AIStep` (unless disabled) then `mUnitClass->Update(unit)`:

| Piece | Where, size | Reads / writes | Draws, calls out |
|---|---|---|---|
| `AIStep` | EEComplexUnit.cpp 0x1ff2e8, ~40 lines | `mCommand`, `mNextTriggerTime`, the goal kinds; `mCommand->Evaluate` (the trigger script: `EEUnitClass::EvaluateCondition`, EEUnitAI.cpp ~350 lines), `DropFinishedGoals`, the next command | none directly |
| `ActionChanged` | EEUnitAI.cpp + the EEUC* overrides, ~250 lines | the action switch: `mGoalState->Step(unit)` (the goals' bodies, EEUG*.cpp ~6,800 lines), `SetGoal`, animation, death and rotting, flee | the idle wait's draws; `gPathManager` (cancel, drop paths) |
| the class's `Update` (+0xb8) | EEUCLand.cpp and ~30 overrides | the physics model's step, turrets, wheels, effects, the reload / search / camouflage timers, `Animate`, sound, `RegeneratePower`, the attack frame (`FireShot` -> `ApplyHit`: the target's hit points), regeneration | `Animate`'s and the damage roll's draws; effects, sound |
| `EEPhysicsModel::Step` | EEPhysicsModel.cpp 1,160 + EEPM*.cpp ~4,500 | the transform (`SetLocation*`), velocities, roll, push, waypoints, `mHeight`; `AfterStep`'s `Collide` (the contacts, in their list's order) and `Move` | `EEPMBuilding`'s draws; `EEUnit::MoveTo` |
| `EEUnit::MoveTo` | EEUnit.cpp | the tile (`mTileX`/`mTileY`), the tile lists and the world chain (`Link`/`Unlink`, `ResortUnit`), regions, sight, the off-map list, the mini map, activation | vis map, mini map |

`mTimesUpdated` is never incremented in a step (only reset, saved and summed). One random stream (`gPhysics->mRandom`)
serves every unit, in `mUpdateUnits`' order: a Rust tick must walk that order, not the store's id order.

**What the store still lacks for the step:** the update order (`mUpdateUnits` as an id list, kept at
`UpdateActiveUnits`), the timers (`mReloadTimer`, the target and heal search timers, `mNextTriggerTime`, the rotting
time, `mPower` and its regeneration timer, the camouflage timer), the class's per-unit stats reads (`mStats`: an object
upgrades change in place, so a per-class table keyed by the stats' version, not a holder), the velocities and the
orientation, the waypoints, the contacts' order, and the goal queues as ids (goals have no ids: the save codec's
work, leaf 7).

**The next three rounds, in order:**
1. The store's update order and timers: `mUpdateUnits` as a Rust id list (checked against the deque at every USCK
   check), the timers and `mPower` as holders; then the smallest leaves of `Update` in Rust over the store, called
   from the C++ step: the reload, target-search and heal-search countdowns, the rotting countdown, the idle trigger
   time, `RegeneratePower` (the combat lane's `Regenerate` already is), the camouflage timer's arithmetic (its mini map
   calls returned as events). Gate: the oracle and the capture's USCK.
2. `AIStep` and the trigger script's decision (`EEUnitTrigger::Evaluate`, `EvaluateCondition`) in Rust over the store,
   `ActionChanged` and the goals' steps staying C++ host calls; the physics models' pure arithmetic (`AfterStep`'s roll
   decay, `Move`'s push scale and clamp) with the velocities in the store. Gate: a USST-style capture of every unit's
   step inputs and outputs every tick against the C++ goldens, the oracle.
3. The step loop itself in Rust (`UpdateUnits` walking the store's order, each unit's step a sequence of host calls and
   Rust leaves), then the goals' steps one class at a time, starting with Location and Explore (the movement), which
   needs the goal queues as ids (with the save work).

**Leaf 6, tenth round (2026-11-11): the first round of the three, done in part.** The stepped units are a Rust list
(`ee_units` order.rs: the deque's ids in its order, every change reported where it is made, a load's reorder copied
after the fixups; USCK compares them). The step's timers and power are holders (`mReloadTimer`, `mTargetSearchTimer`,
`mHealSearchTimer`, `mNextTriggerTime`, `mRottingAwayTime`, `mPower`, `mPowerRegenTimer`, `mCamouflageTimer`; USCK).
In Rust over the store (step.rs), called from the C++ step: `UpdateReloadTimer`, `UpdateTargetSearchTimer`,
`UpdateHealSearchTimer`, `RegeneratePower` (USTM: golden from the C++ bodies; the original's statements recheck each
call while the capture runs). A stepped unit can be out of the store (a killed unit leaves its list first: 3 to 5 a
game), so a leaf takes the member's value for it. What the per-unit step does in Rust now: those four leaves' arithmetic;
everything else in `Step` is C++.

**The updated plan:**
1. (the rest of round 1) The rotting countdown (17 inline copies in the classes' `Update`s, one `UpdateRottingAwayTime`
   helper over step.rs) and the camouflage timer's arithmetic (its two timers and the cloak flags as a decision, the
   mini map moves and the chain setters returned as events for the C++ to apply), each added to USTM from the C++ first
   (a new USTM kind changes the digest: record the C++ goldens in the same round, before the Rust body).
2. `AIStep` over the store: `mNextTriggerTime` and the command's type are in the record; the trigger script's decision
   (`EEUnitTrigger::Evaluate`, `EvaluateCondition`) as Rust over the record and host calls, `ActionChanged` and the
   goals' steps staying C++; the velocities in the store with the physics models' pure arithmetic (`AfterStep`'s roll
   decay, `Move`'s push scale and clamp). Gate: a USST capture (each unit's step inputs and outputs per tick) from the
   C++ first, the oracle.
3. The step loop in Rust (`UpdateUnits` over order.rs: the collision period, the players' updates as a host call, each
   unit's `Step` from the last to the first), then the goals' steps one class at a time (Location and Explore first),
   which needs the goal queues as ids (the save work).

**Leaf 6, eleventh round (2026-11-12): the updated plan's items 1 and 2 done, 3 started.** In Rust, called from the
C++ step: the rotting countdowns and the resources' rotting (step.rs, over the record's time and resource amount),
`UpdateCamouflageTimer`'s two timers (a decision over the record's camouflage timer, the second timer `mU32C`
handed in, returning its events in the original's order for the C++ to apply; the mini map test made where an event
is applied) and its cloak fade; `AIStep` with the script's decision (ai.rs: the record's command type, next trigger
time and goal kind, re-read after the host calls that change them; the triggers read one at a time as plain views,
`EvaluateCondition`, `IsActive`, `StartAction`/`ActionChanged`, `DropFinishedGoals`, `UpdateCamouflage` and the
command's write as host calls); `AfterStep`'s roll decay and `Move`'s push arithmetic up to the new point (motion.rs,
over the record's push velocities, roll and location; `mVelocity`, `mPushVelocityX/Y` and `mRoll` are holders); the
unit loop (tick.rs: the collision period, the players' updates and each step by the list's index, host calls). Gate:
USTM kinds 5-8 and USST, recorded from the C++ bodies first, equal in the five profiles (PORT_LEARNINGS "R5b.4,
eleventh round").

**What the tick's per-unit step does in Rust after this round:** the loop over the stepped units (their order, the
collision period); in `AIStep`, every decision (which path, the script's walk over its triggers, the result's
handling, the idle wait and the next trigger time); in the class's `Update`, the reload, target and heal search
countdowns, the power's regeneration, the rotting countdowns, the camouflage timers' decision and the cloak fade; in
the physics model, the roll decay and the collision push's arithmetic. C++ still: the conditions themselves (the
classes' `EvaluateCondition`, the attack goals' tests: the combat lane's), `ActionChanged` and the goals' `Step`s,
the rest of the classes' `Update`s (animation, sound, effects, turrets, the attack frame's `FireShot`, regeneration's
glue), the physics models' `Step0..5` and `Collide`, `MoveTo` and the tile lists.

**Next, in order:** the physics models' per-action steps' pure arithmetic (Step0's acceleration and turn, the
EEPM* subclasses' copies of `Move`'s push: Helicopter, Naval, Spaceship, SubmergedNaval) over the store's velocities
and orientation; the classes' `Update` dispatch as a Rust sequence of leaves and host calls (one class at a time,
EEUCLand first); `ActionChanged`'s action switch as a decision; then the goals' steps (Location and Explore first),
which needs the goal queues as ids (the save work).

**Leaf 6, twelfth round (2026-11-12): items 1 and 2 of that list done, 2 in part.** First the combat lane's store
fields by its round-8 count of the C++ host reads: (1) the condition fill's unit members (`mFlightTime`, `mCloaked`,
`mIsCloakable`, `mIsCloaked`, `mHerd`, `mContainer`) as holders and the class's fixed fields (family, cloak radius,
crusader, paratrooper bomber, heal threshold, and (7) the wall/projectile/paratrooper/melee flags in the combat reads'
bit order) with `mUnitClass`; (3) the records by object (`Store::by_object`; ee_rust's `ee_combat_store_unit_of`);
(5) the players' diplomacy tables by player object (`players.rs`: `mEnemies` and the id). USCK checks all of them.
Not yet: the stats (changed in place by upgrades: a table keyed by the stats' version), the human flag (virtual, and
the constructors call `Reset`), the regions (map state), the targetable table. Then the physics models' per-action
step arithmetic (physics.rs: the turn with its yaw mode, the spin down, the steady turn, the speed with its mode and
the helicopter's, the pitch mode and approach with the naval model's), the C++ keeping the reads (the headings,
the lazy path length and terrain pitch) and the writes; the push copies of EEPMNaval and EEPMSpaceship were the base
`Move` without the ground placement (one `MoveByPush` body now), EEPMHelicopter's and EEPMSubmergedNaval's keep their
tile work over the same Rust push. Gate: USST kinds 5-11 (and the subclasses' push notes), golden from the C++ bodies
in the five profiles. Last, `EEUCLand::Update` and `EEUCNaval::Update` as Rust sequences over one host (update.rs;
the herd, owner, tile, command, goal and unit type from the record); gated by the digests' order and the oracle. C++
this round (net): 844 lines deleted, 566 added (most of the added are the TEST ONLY notes, the store's reports and the
update host).

**What the tick's per-unit step does in Rust after this round:** the loop over the stepped units; in `AIStep`, every
decision; in `EEUCLand::Update` and `EEUCNaval::Update`, the sequence itself (which leaves, in which order); in the
classes' `Update`s, the reload, target and heal search countdowns, the power's regeneration, the rotting countdowns,
the camouflage timers' decision and the cloak fade; in the physics models, the turn (heading and yaw rate), the spin
down and steady turn, the speed and its mode, the pitch and its mode, the roll decay and the collision push's
arithmetic, for every model that uses the base functions (land, helicopter, naval, submarine, spaceship, sea animal,
airplane and air animal through `Accelerate`). C++ still: the conditions themselves, `ActionChanged` and the goals'
`Step`s, the other classes' `Update` sequences, the leaves the host calls (animation, sound, effects, turrets, the
attack frame's `FireShot`, regeneration's glue), the steps' transformations (`Step0..5`'s orientation, ground and
clip work), `Collide`, `MoveTo` and the tile lists.

**Next, in order:** the other classes' `Update` sequences over the same host (LandCitizen, the priests, Spaceship,
Building first: each a short list of the host's leaves, each deleting its C++ body); `ActionChanged`'s action switch
as a decision (EEUCLand's and EEUnitClass::HandleActionChanged first; a USST note of the action, the branch and its
outputs from the C++ first); the stats as a per-class table keyed by the stats' version (the combat lane's largest
remaining read); then the goal queues as ids (the save work) and the goals' steps (Location and Explore first).

**Leaf 6, thirteenth round (2026-11-12): items 1 and 2 of that list done.** Golden first: every leaf call of the original class `Update` bodies and every operation of the original `ActionChanged` bodies was noted in its place (USST kinds 12 and 13), the goldens recorded from that tree in the four profiles, then the bodies replaced. `Update` (update.rs, 20 class kinds): the land and naval citizens, the basic priest (the land, naval and helicopter priests'), spaceship, submerged naval (the sea animal's), helicopter, transport (the naval and space transports') with the land, helicopter and fortress variants, landable (the airplane's and planet spaceship's; `UpdateFlightState` inlined and deleted), air animal, building, gate, mining, naval and space building, ambient; the eight overrides that only called a base are deleted. `ActionChanged` (actions.rs): the common handling (`HandleActionChanged`'s 40-case switch: herd, contact location, the attacked and fleeing, waiting to reload, returning to contact, idle with its stance draw, rotting, dying, killing, moving, turning, healing, the goal steps, the goal's start time) and the land, naval, submerged naval, spaceship and basic priest overrides, 60 host operations (each one call or expression). C++ this round (without the generated header): 901 lines deleted, 768 added (the host EEUnitUpdate.cpp, 584 lines with the round-12 host it took over from EEUCLand.cpp, is most of the added: every read and operation one case).

**What the tick's per-unit step does in Rust after this round:** the loop over the stepped units; in `AIStep`, every decision; in every unit class's `Update` (all but the shots, which are combat's, the basic citizen's one call and the building priest's one test), the sequence itself (which leaves, in which order) and the arithmetic between them; inside the leaves, the reload, target and heal search countdowns, the power's regeneration, the rotting countdowns, the camouflage timers' decision and the cloak fade; when the script's command changes, `ActionChanged`'s decision for every class that reaches the common handling and the five moved overrides; in the physics models, the turn, spin down, steady turn, speed, pitch, roll decay and push arithmetic. C++ still: the leaves themselves (animation, sound, effects, turrets, `FireOnAttackFrame`, `UpdateCitizen`, `UpdateBuilding`, `UpdateArmed`, the conversion), the other classes' `ActionChanged` overrides, the goals' `Step`s, the conditions (the classes' `EvaluateCondition`), the steps' transformations, `Collide`, `MoveTo` and the tile lists.

**Next, in order:** the other classes' `ActionChanged` overrides over the same host and notes (the citizens, the building, the air units, the transports, the gate); the stats as a per-class table keyed by the stats' version (the combat lane's largest remaining read); then the goal queues as ids (the save work) and the goals' steps (Location and Explore first).

**The out-of-sync checksum, mapped (2026-11-11).** `EEPhysics::GetOutOfSynchChecksum` sums, over `gWorld.mUnits` (the
units on the map's tile lists, not the players' lists: the walk follows `mTileNode->mNext`), each unit's
`GetChecksum(flags)` (0 for a simple unit; for a complex one: flag 1 hit points + tile x + tile y, flag 2 the times
updated, the physical model's location and height as float bits, whether it has an active group, its command's type,
flag 0x20 its goals' checksums, flag 8 its area effects'), then per player the selection (0x200) and the air traffic
(0x40), the tick, the seed, the inputs processed, the AI's checksum, the map (4), the continents (0x100) and the
calamities (0x10). The sums wrap and commute, so the walk's order does not matter. Over the store today: flag 1 only,
and only with an "on the map" flag per record (a unit is in `gWorld.mUnits` while its tile node is linked:
`RTSWorld`'s tile list insert and removal): the two domains differ at least for units that are in a list but have
no tile node (a unit inside a transport or building has tile -10000; not yet measured). Flag 2 needs the positions, the times updated, the active group and the command (the physics tick's
fields: leaf 6), 0x20 the goals (save and AI work). So the checksum moves in two steps: flag 1 over the store with the
on-map flag (the oracle's `oos=` gates it), then the rest with the tick.

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
  strings convert at the wire. **Done** (branch `rust-d6-utf8`, the Progress row "D6"; PORT_LEARNINGS "D6: game text
  is UTF-8"): ported Rust keeps game text as bytes, now UTF-8 behind `ee_plat_core::utf8`.
- **D7. Lockstep across systems:** with `-ffp-contract` unset, arm64 may already disagree with x86-64. Decide
  whether cross-system multiplayer is a goal; if so, set `-ffp-contract=off` now and add a Mac-vs-Windows sim trace
  comparison.
- **D8. The WON lobby: decided (owner, 2026-11-09): port all of it for now (option a), except the HTML rendering**
  (the WONLobbyHTML* segments and parsers, HTMLParser2, ~6k lines), which is not ported: where the lobby shows HTML
  (news, help, messages) it shows plain text instead. The online services are ported with the rest even though their
  servers are gone.

- **D9. Open formats only: decided (owner, 2026-11-09).** The game reads glTF models, PNG textures and the text db
  tables (CSV/YAML) only. The binary forms are retired at run time: `.cem` models (current and legacy), `.sst`/`.tga`
  textures and the binary db records. The models and textures load natively (glTF -> GEModel, PNG -> texture) instead of
  through in-memory `.cem`/`.sst` bytes, and their loaders and savers are deleted rather than ported. The binary data
  tree leaves the gates; the importers that turn the retail installs into the open tree (tools/open_data.py,
  tools/eedata, tools/eemodel) keep reading the binary forms, since that is where the open data comes from.

## 7. First steps

1. R0 on a branch: the workspace, wgpu-native from source, the link at 0/0, CI.
2. The cheap oracles, independent of Rust and useful now:
   - the per-CT checksum trace (R5a.1, done);
   - the side-effect-free save (R5a.2, done);
   - `-ffp-contract=off` with a Mac/Windows trace comparison (D7).
3. PlatConfig in Rust (R1.1) as the first code across the seam.
4. `rustgen.py` from `specs.json` (R2.2): it costs little and makes every later phase's records free.
