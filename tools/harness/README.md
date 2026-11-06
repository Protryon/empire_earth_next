# tools/harness: the live-game test harness

A port of the `empire_earth` reconstruction's `tools/boot` (the headless live-game harness: staging, the debugger
harness `exe_boot.c`, exercises, telemetry, AI games, the human driver and per-PT differential tooling) for this
repository's builds. The reconstruction's version runs the original game or a VC6 full link next to the retail DLLs,
reads memory at VC6 offsets, and binary-patches the images for the VM. Here:

- **The build under test** (`hbuild.py`): a test-profile build of the one static exe
  (`python -I build.py --test-profile windowed-software` -> `build/clang-test/exe/EE-AOC.exe`). The windowed and
  software-display policy (for the T&L renderer: the RGB software device) is compiled in, `WinMain` loads the
  presentation bridge, and the functions the harness hooks are kept out of line. The game's own mixer runs (on
  miniaudio's null device when the machine has no audio output).
- **Offsets by member name** (`layout_gen.py`, `layout.py`): the build's record layouts (clang's dumps;
  `build.py` writes `<build>/layout.json`). `layout.off(cls, vc6_offset)` translates any of the reconstruction
  harness's VC6 offsets through the frozen VC6 layout `vc6_layout.json.gz`.
- **One image**: the engine, the renderer and the runtime are inside the exe, so every hook is an exe address from
  its map. The presentation bridge (`TestPresentation.dll`) gets the build's offsets, `ShowScene`'s RVA and the
  engine members it calls (`bridge_syms.h`, written at build time). `exe_boot.c --static-exe` arms the engine's
  `ESRecordNativeError`, the static runtime's `abort`, the RNG trace and the renderer checkpoints in the exe. The
  time shim renames the exe's WINMM import; the RDP mode proxy renames the exe's DDRAW import.

```
python -I build.py --test-profile windowed-software
python -I tools/harness/game_session.py --exercise tour [--play-seconds 120] --timeout 480000
python -I tools/harness/game_session.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard \
       --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40 --timeout 2400000
```

The tour and random-game exercises check the rendering of the gameplay frame and (from `--play-seconds 10`) of
the play frame: the game view must be lit and varied, the mini map must show terrain
(`session_exercise.check_rendering`, thresholds in PORT_LEARNINGS.md "x87 range"; evidence `rendering.json`).
`EE_GX_STATS=<n>` in the environment makes GX log a summary of the 3D draws every n scenes (debug strings in the
session log).

**AI decision trace and benchmark** (docs/AI_RESEARCH.md stage 0). `EE_AI_TRACE=1` in the environment makes the
game write one `EE-AI` debug string per computer-player decision (`ee/EECPTrace.h`); `EE_AI_CHEATS` keeps only the
named AI cheats (`none` turns every one off; sets separated by `/` go to the players in turn from player 1, the idle
human, so `none/all` makes p2 cheat and p3 honest; TEST ONLY). `ai_trace.py extract|compare|summary` reads them from a
session log; `compare` is the determinism check (two runs of one seed must print the same lines).
`ai_bench.py` runs a matrix of seeded headless AI games with the trace on and writes `build/boot/ai_bench/<name>/`
(`results.json`, `results.csv`, the session logs; `--repeat 2` also compares the traces). `--jobs N` runs N games
at once as `game_session.py --instance 1..N` (own stage, config.ini and ports each): 8 15-minute games take ~3.5 min
with 4 jobs instead of ~13, and same-seed pairs still match:

```
python -I tools/harness/ai_bench.py --name stage0 --seeds 2,3 --cheats all,none --repeat 2 --jobs 4
```

Requirements: the two game installs (Art of Conquest and the original Empire Earth; only read), MSYS2
`mingw-w64-x86_64-gcc` (`C:\msys64\mingw64`; the debugger harness `exe_boot.exe`, the bridge and the time shim are
built with it), pip `pefile capstone`.

**Data and settings.** Every run stages the unified data tree of `tools/open_data.py` (built from the installs into
`build/open_data/` the first time; hard links, the folders the game writes into are copied): by default the binary
tree (`tree-binary`: the .dat/.cem/.sst/.tga as shipped, loose), with `EE_HARNESS_OPEN_DATA=1` (or
`EE_HARNESS_TEXT_DB=1 EE_HARNESS_OPEN_ASSETS=1`) the open one (`tree`: db text, glTF, PNG; `EE_HARNESS_OPEN_DATA=DIR`
names another tree). No data.ssa and no original install is staged. The game's settings are a fresh `config.ini` in
the stage per run, holding the run's pins (`Game Bit Depth = 32`, `--pt-rate`); the file the game leaves is kept as
`build/boot/<stage>.session.config.ini`. The game uses no registry: `exe_boot.c` stops a run on any registry write.

**x86_64 only.** The exe and every harness piece are x86_64 (`hbuild` refuses another image; `--x64` is accepted and
ignored). The harness names the game's symbols by their x64 decorations (`?gWorld@@3VEEWorld@@A`,
`?Enter@TSCriticalSection@@QEAAXXZ`, `GXTestReadFrame`). Offsets come from the build's `layout.json` (`hbuild`
checks it has 8-byte pointers). Exe addresses are passed as RVAs, so an ASLR image works too. Not ported:
`differential/` (VC6 layouts with 32-bit pointers; `--pt-sync`, `--pt-hash`, `--pt-watch` are refused).

**Self test** (no game; ~1 min): `python -I tools/harness/selftest/selftest.py` runs 40 guard cases (every guarded API:
files, registry, windows, processes, display, sockets, the test interventions) under `exe_boot.exe` and checks each
case's decisions and STOP, then the time shim, the bridge, the telemetry probe and the session plumbing on small x64
exes. It writes only below `build/boot/selftest*`, `build/boot/windowed_o_32` and
`HKCU\Software\EE-harness-selftest*`.

**Linux and macOS** (`posix_game.py`): neither the debugger harness nor the bridge exists there; the game carries the
bridge's UI fixture itself (`src/Empire Earth/Empire Earth/EETestFixture.cpp`, on only when `EE_TEST_UI` names a
directory) with the same ui-state.txt / test.command formats plus game telemetry. `posix_game.py --exercise
menu|random-game|ai-game` reuses ai_game.py's lobby options and dismiss table, plays in real time (no time shim; the
lobby's Very Fast speed), writes the game's settings to its own config.ini (`EE_CONFIG`) and, on macOS, captures the
game window (`macos_window.swift` + `screencapture -l`). See its docstring.

## Status

| Part | State |
|---|---|
| staging, registry, `exe_boot.c`, presentation bridge, silent mixer, RDP proxy, telemetry (`game_state_probe.py`), `--exercise tour/settings/random-game`, `--play-seconds` | ported |
| `--exercise ai-game`, `--time-factor`/`--no-render` (time shim), `--seed`, `--no-human-defeat` | ported |
| all of the above, x86_64 only | self-tested; tour + 120 s play and the seeded 2-AI game pass |
| `human/` (`human_input.c`: ~90 offsets, Dinkum deque internals read raw) and so `--human-script` / `EE_HUMAN_LOCKSTEP` | not yet |
| `differential/` (`layout.py`, `ptsync.h`, `ai_layout.py`, `sampler.py`) | not yet |
| `mp_game.py`, `save_load.py`, `scenario_game.py`, `synth_scenario.py`, `game_batch.py`, `play.py` | not yet (they still name i686 symbols and VC6 addresses) |
| `ubsan_minimal.c` | the recovering handlers of `build.py --sanitize-null` builds |

`fulllink_locator.py` and `windowed_overlay.py` remain only for their helpers (`LinkMap`, `test_output`).
