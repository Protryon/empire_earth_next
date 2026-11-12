# tools/harness: the live-game test harness

**The gates' path: the fixture driver** (`fixture_game.py`, `fixture_driver.py`; docs/RUST_PLAN.md R5a.7). It runs
the test-profile exe directly, with the game's own UI fixture (`src/Empire Earth/Empire Earth/EETestFixture.cpp`,
on when `EE_TEST_UI` names a directory) and nothing else: no debugger, no record layout, no mangled name, no import
hook. What it reads:

| Fact | Source |
|---|---|
| form stack, controls, combos, check boxes, command acknowledgement | `ui-state.txt` (`form`, `stack`, `control`, `combo`, `check`, `command`) |
| a command's verdict (`triggered`, `selected`, `rejected`) | the game's stderr (`TEST UI <verdict> ... (seq N, ...)`) |
| game time, PTs, game over, world players, map, local player | `game ms <t> ticks <PT> over <0/1> players <n> map <0/1> local <i>` |
| CTs, pause | `comm ct <n> last <n> paused <0/1> multiplayer <0/1>` |
| per player: observer, result, units (`mPopulationCount`), victory units, civilization flags, stock | `player <i> observer .. result .. population .. victory .. civ <locked> <chosen> stock f/w/s/g/i` |
| the harness clock's factor, frames written | `clock <num>/<den> ms <t> frames <n>` |
| frames (rendering checks) | Windows: the game window captured with `PrintWindow` (`PW_RENDERFULLCONTENT`); a game capture must differ from the one before it (a renderer that stopped presenting leaves the last picture). Elsewhere: `frame.request` -> `frame.bmp`, the next presented frame (GX's `GXTestSetPresentHook`/`GXTestReadFrame`, whose read-back times out on WARP) |
| GX errors | stderr `debugstring GX: wgpu error ...` (the game's debug output, `PlatDebugSetHandler`), counted in `summary.json` `gx_errors` |
| computer players, samples, the end | the game log `Logs/game-*.log` (EE-GAME lines), copied into the evidence |
| the switches taken | stderr `TEST UI clock factor N/D`, `TEST UI seed S` |
| input check | F3 posted to the game window (`WM_KEYDOWN/UP`), the pause read from `comm ... paused` |

The switches are environment variables the fixture takes only with `EE_TEST_UI` (`EETestFixtureConfigure`, first
in `EERun`): `EE_TEST_TIME_FACTOR` (the harness clock: `PlatSimSetTimeFactor`, `platform/PlatSim.h`; the platform
layer's clocks run F x real time and its sleeps, timers and wait timeouts 1/F, replacing the time shim),
`EE_TEST_SEED` (`gPlatSimSeed`, as `--seed`), `EE_TEST_NO_HUMAN_DEFEAT`; `--no-render` is GX's `EE_GX_NULL3D`.

```
python -I tools/harness/fixture_game.py --exercise tour --input-check --play-seconds 120 [--data TREE]
python -I tools/harness/fixture_game.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard        --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40 [--data TREE]
```

The stage is `build/fixture_game/stage` (the exe, the tree as hard links, a fresh `config.yaml` with the pins), the
evidence `build/fixture_game/<time>/` (`summary.json`, `game.log`, `ui/`, the frames, `rendering.json`,
`samples.json`, the game log). The last line is `RESULT PASS|FAIL <exercise> ...`. Not covered compared with the
debugger path: the guards (registry writes, files outside the stage, non-loopback sockets), the crash/abort hooks
(a crash ends the process and fails the run; a message box fails it), the RNG trace, `--instance`, the human driver
and the differential tools. `posix_game.py` is the same core with macOS captures. `tools/gates.sh --legacy` still
runs the debugger path below.

The rest of this file is the debugger harness (`game_session.py`), which depends on the build's record layout and
mangled names.

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

**The human command log** (TEST ONLY; the game log, on in every normal game, config.yaml debug "Game Log"): one
`EE-GAME <ms> p<n> order verb=<verb> units=<n> classes=<class>:<n>,... ids=<id>,... [target=<id>:<class>:p<owner>
@<x>,<y>] [at=<x>,<y>] [class=|tech=<name>] [queued=1] [forced=1]` line per human order as it executes in the
lockstep (EEPhysics::DrainInput: the same in multiplayer and replays), and `EE-GAME <ms> p<n> select units=...` when a
human's selection changes. The verbs and fields are in `include/ee/EEGameLog.h`. It only reads; the sim (no game
log) is unchanged by it. Off in the headless simulation.

**AI decision trace and benchmark** (docs/AI_RESEARCH.md stage 0). `EE_AI_TRACE=1` in the environment makes the
game write one `EE-AI` debug string per computer-player decision (`ee/EECPTrace.h`); `EE_AI_CHEATS` keeps only the
named AI cheats (`none` turns every one off; sets separated by `/` go to the players in turn from player 1, the idle
human, so `none/all` makes p2 cheat and p3 honest; TEST ONLY). `EE_AI_DEMAND` (`0`, `1`, or per player
as `0/1`) overrides the personalities' `economy.demand_split` (TEST ONLY). `EE_AI_EXP` names experiments per player the same way (`EECPExperiment`;
`ai_scorecard.py --key exp`; `nomilitia`, `nomtower`, `noprearmy`, `nofish`, `nofoodeco`, `nogoldfix`, `nonavalflee`, `noprepeace` and `nostone` turn off the personalities' `military.militia`, `military.militia_towers`, `military.prehistoric_army`, `economy.coast_fishing`, `economy.forage_patches`, `economy.claim_needed`, `military.naval_flee`, the Prehistoric peace and `epoch_pressure.stone_span`; `oldpatches` keeps forage_patches without the §8.28 claim range,
patch settlements and site towers, `nosettle` and `noguard` turn off one of those; the experiments `droprule`,
`span20`/`span25`/`span30`, `boatrush` and `threatspend` are §8.28's). `ai_trace.py extract|compare|summary` reads them from a
session log; `compare` is the determinism check (two runs of one seed must print the same lines).
`ai_bench.py` runs a matrix of seeded headless AI games with the trace on and writes `build/boot/ai_bench/<name>/`
(`results.json`, `results.csv`, the session logs; `--repeat 2` also compares the traces). `--jobs N` runs N games
at once as `game_session.py --instance 1..N` (own stage, config.yaml and ports each): 8 15-minute games take ~3.5 min
with 4 jobs instead of ~13, and same-seed pairs still match:

```
python -I tools/harness/ai_bench.py --name stage0 --seeds 2,3 --cheats all,none --repeat 2 --jobs 4
```

Requirements: the two game installs (Art of Conquest and the original Empire Earth; only read), MSYS2
`mingw-w64-x86_64-gcc` (`C:\msys64\mingw64`; the debugger harness `exe_boot.exe`, the bridge and the time shim are
built with it), pip `pefile capstone`.

**Data and settings.** Every run stages the unified open data tree of `tools/open_data.py` (`build/open_data/tree`:
db text, glTF, PNG; built from the installs the first time; hard links, the folders the game writes into are
copied). `EE_HARNESS_OPEN_DATA=DIR` (and `fixture_game.py --data DIR`) names another tree. The game reads only these
open forms (docs/RUST_PLAN.md D9): the binary tree (`tree-binary`) and the `EE_HARNESS_TEXT_DB` /
`EE_HARNESS_OPEN_ASSETS` installs into it are retired. No data.ssa and no original install is staged. The game's settings are a fresh `config.yaml` in
the stage per run, holding the run's pins (`audio.music.enabled=true`, `--pt-rate`; `config_yaml.py`); the file the game leaves is kept as
`build/boot/<stage>.session.config.yaml`. The game uses no registry: `exe_boot.c` stops a run on any registry write.

**x86_64 only.** The exe and every harness piece are x86_64 (`hbuild` refuses another image; `--x64` is accepted and
ignored). The harness names the game's symbols by their x64 decorations (`?gWorld@@3VEEWorld@@A`,
`?Enter@TSCriticalSection@@QEAAXXZ`, `GXTestReadFrame`). Offsets come from the build's `layout.json` (`hbuild`
checks it has 8-byte pointers). Exe addresses are passed as RVAs, so an ASLR image works too. Not ported:
`differential/` (VC6 layouts with 32-bit pointers; `--pt-sync`, `--pt-hash`, `--pt-watch` are refused).

**Self test** (no game; ~1 min): `python -I tools/harness/selftest/selftest.py` runs 40 guard cases (every guarded API:
files, registry, windows, processes, display, sockets, the test interventions) under `exe_boot.exe` and checks each
case's decisions and STOP, then the time shim, the bridge, the telemetry probe and the session plumbing on small x64
exes, and the fixture driver against a stand-in of the fixture's protocol (`selftest/fixture_standin.py`). It writes only below `build/boot/selftest*`, `build/boot/windowed_o_32` and
`HKCU\Software\EE-harness-selftest*`.

**Linux and macOS** (`posix_game.py`): neither the debugger harness nor the bridge exists there; the game carries the
bridge's UI fixture itself (`src/Empire Earth/Empire Earth/EETestFixture.cpp`, on only when `EE_TEST_UI` names a
directory) with the same ui-state.txt / test.command formats plus game telemetry. `posix_game.py --exercise
menu|random-game|ai-game` reuses ai_game.py's lobby options and dismiss table, plays in real time (no time shim; the
lobby's Very Fast speed), writes the game's settings to its own config.yaml (`EE_CONFIG`) and, on macOS, captures the
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
| `fixture_game.py`, `fixture_driver.py` (the gates; no layout, no debugger) | menu, tour, random-game, ai-game; self-tested against `selftest/fixture_standin.py` |

`fulllink_locator.py` and `windowed_overlay.py` remain only for their helpers (`LinkMap`, `test_output`).
