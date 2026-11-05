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

Requirements: the game install (its data, Miles and Bink are staged from it), MSYS2 `mingw-w64-i686-gcc`
(`C:\msys64\mingw32`; the debugger harness, the bridge and the time shim are built with it), pip `pefile capstone`.

## Status

| Part | State |
|---|---|
| staging, registry, `exe_boot.c`, presentation bridge, silent mixer, RDP proxy, telemetry (`game_state_probe.py`), `--exercise tour/settings/random-game`, `--play-seconds` | ported |
| `--exercise ai-game`, `--time-factor`/`--no-render` (time shim), `--seed`, `--no-human-defeat` | ported |
| `human/` (`human_input.c`: ~90 offsets, Dinkum deque internals read raw) and so `--human-script` / `EE_HUMAN_LOCKSTEP` | not yet |
| `differential/` (`layout.py`, `ptsync.h`, `ai_layout.py`, `sampler.py`) | not yet |
| `mp_game.py`, `save_load.py`, `scenario_game.py`, `synth_scenario.py`, `game_batch.py`, `play.py` | not yet |
| `ubsan_minimal.c` | the recovering handlers of `build.py --sanitize-null` builds |

`fulllink_locator.py` and `windowed_overlay.py` remain only for their helpers (`LinkMap`, `test_output`).
