# tools/harness: the fork's live-game test harness

A port of the ee repository's `tools/boot` (the headless live-game harness: staging, the debugger harness
`exe_boot.c`, exercises, telemetry, AI-game, human-driver and per-PT differential tooling) for this fork's own
builds. ee's version runs the original game or a VC6 full link next to the retail DLLs. Every offset it reads there
is VC6's, and the VM-compatibility policy is binary patches. Here:

- **The build under test** (`hbuild.py`): a clang test-profile build, `--stl msvc` (`build/clang-test-msvc`) or
  `--stl dinkum` (`build/clang-test`), every module from source. Built by
  `python -I tools/clang_link.py [--stl msvc] --test-profile windowed-software`, which compiles the windowed and
  software-display policy into the exe and the renderer (ee: binary overlays).
- **Offsets by member name** (`layout_gen.py`, `layout.py`): clang's record layouts of the build. `layout.off(cls,
  vc6_offset)` translates any of ee's VC6 offsets to the build's (identity for the Dinkum build, whose layout
  `layout_gen.py --check` proves equal to VC6's documented one).
- **Companion DLLs for the build**: the silent mixer is compiled by the build's profile and linked against its
  engine and runtime. The presentation bridge (`TestPresentation.dll`) gets the build's offsets and its renderer's
  `ShowScene` RVA as defines. `exe_boot.c` takes the renderer checkpoints (`--renderer-guards`) and guards `abort`
  in `ucrtbase.dll` too.

```
python -I tools/harness/layout_gen.py --stl msvc        # once per header change
python -I tools/harness/game_session.py --stl msvc --exercise tour [--play-seconds 120] --timeout 480000
```

Requirements: the game install, MSYS2 `mingw-w64-i686-gcc` (`C:\msys64\mingw32`), pip `pefile capstone`, the fork's
`toolchain/` (VC6, for the RDP mode proxy).

## Status

| Part | State |
|---|---|
| staging, registry, `exe_boot.c`, presentation bridge, silent mixer, RDP proxy, telemetry (`game_state_probe.py`), `--exercise tour/settings/random-game`, `--play-seconds` | ported: the MSVC STL build passes the tour and plays |
| `ai_game.py`, `no_human_defeat.py`, `human/` (`human_input.c`: ~110 offsets), `timeshim.*`, seed pin (`EEMStartLoading+0x14`) | not yet: still VC6 offsets |
| `differential/` (`layout.py`, `ptsync.h`, `ai_layout.py`, `sampler.py`) | not yet |
| `mp_game.py`, `save_load.py`, `scenario_game.py`, `synth_scenario.py`, `game_batch.py`, `play.py` | not yet |

`fulllink_locator.py` and `windowed_overlay.py` remain only for their helpers (`LinkMap`, `test_output`).
