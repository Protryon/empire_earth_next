# empire_earth_next

*Empire Earth: The Art of Conquest* built from source with clang-cl and lld-link: one statically linked x86_64
`EE-AOC.exe` (Windows), plus Linux and macOS builds. The game may diverge from the original. Long-term goals: Rust and
deep, data-driven modding. There are no compatibility goals with the retail data, saves or multiplayer.

Read first: `README.md` (what builds and how), `PORT.md` (the port), `PORT_LEARNINGS.md` (every problem met and its
fix; keep it current), `docs/MODDING_PLAN.md` (rules as data, lifted caps, status), `docs/DATA_FORMATS.md`,
`docs/MODEL_FORMATS.md`, `docs/AI_RESEARCH.md`, `tools/harness/README.md`.

## Building the game

```
python -I tools/fetch_wgpu.py                                      # once: wgpu-native into toolchain/ (sha-pinned)
python -I build.py -j 10                                           # -> build/clang/exe/EE-AOC.exe (+ .map, .pdb)
python -I build.py -j 10 --test-profile windowed-software          # -> build/clang-test/exe/EE-AOC.exe (harness image)
python -I build.py --compile-only                                  # compile every unit, link nothing
python -I build.py -j 10 --sim                                     # + build/clang/exe/EE-SIM.exe (headless simulation)
python -I tools/sim_bench.py --name X --seeds 2-7 --jobs 2         # EE-SIM games: seeds x settings (README "Testing")
python -I tools/clang_build.py [-j N] [module ...]                 # compile census, ranked error summary
```

- Requirements: LLVM, the VS 2022 Build Tools C++ workload (MSVC STL, UCRT, Windows SDK), Python 3.12 and 7-Zip.
  The harness also needs MSYS2 `mingw-w64-x86_64-gcc` and pip `pefile capstone`. See README.md.
- A full build takes about 2 minutes. A link must report `0 unresolved, 0 duplicates`.
- Sources are staged under `C:\EEX\` before compiling. `EE_ORIG_SRC_ROOT` moves the staging root. Use a separate
  root per worktree when two builds run at once.
- New units without an original address go into `units.EXTRA_UNITS` (`tools/units.py`, per module).
- Linux/macOS: `python -I tools/fetch_posix.py wgpu sdl3 fonts`, then `python -I tools/xbuild.py game -j 8`.
  - `xbuild.py test` runs the platform tests (`tests/platform`).
  - `xbuild.py targets` lists what this machine can build.
  - `posix_game.py` drives the game (see README.md).

## macOS (and Linux)

The owner also develops on an Apple Silicon Mac. There the Windows build, its harness (`tools/harness/game_session.py`,
a Win32 debugger) and `tools/gates.sh` cannot run; the POSIX tooling below stands in for them.

- Setup: `brew install cmake ninja python@3.12`. The system `python3` is 3.9: run every tool with
  `/opt/homebrew/bin/python3.12 -I ...`. Then `fetch_posix.py wgpu sdl3 fonts` (above).
- Build: `python3.12 -I tools/xbuild.py game -j 12` -> `build/xbuild/macos-arm64/game/ee-aoc` and `Empire Earth AoC.app`
  (about 40 s).
  - `--sanitize address` (or `undefined`) builds into `game-address/`. Run it first for any crash: ASan stops at the
    first bad access, which found most of the macOS bugs.
  - `--incremental` keeps objects newer than their `.cpp`; it does not track headers, so after a header change do a full build.
  - Don't relink the exe while a game runs from it. For a compile check meanwhile, set
    `EE_XBUILD_OUT=$PWD/build/xbuild-check`.
- Data: the repository's `data/` (gitignored) holds the latest release of each data part, unzipped into one folder
  by `python3 -I tools/fetch_data.py` (it skips the parts already current; `--without textures-4x` leaves the 1.3 GB
  part out). Rerun it when a data release changes the format. Run with
  `EE_DATA_ROOT=$PWD/data build/xbuild/macos-arm64/game/ee-aoc`.
- Live tests: `tools/harness/posix_game.py --exercise menu|random-game|ai-game`. Evidence goes to `build/posix_game/<time>/`:
  `game.log`, `summary.json`, `samples.json`, window captures. Useful options:
  - `--exe` (e.g. the ASan build), `--play-seconds`, `--max-game-seconds`;
  - `--capture-every`, `--zoom-captures 100 200`, `--lobby-check 215=1` (Reveal Map), `--ai-players`.
  - It drives the game's own UI fixture (`src/Empire Earth/Empire Earth/EETestFixture.cpp`, on only with `EE_TEST_UI`):
    the presentation bridge's `ui-state.txt` / `test.command` formats plus telemetry and a `zoom` command. The
    fixture directory must be absolute (the game changes into its data root).
  - Captures: `macos_window.swift` + `screencapture -l` (the terminal needs Screen Recording permission). One
    game at a time: captures find the window by process name.
  - A game started from a background shell is never made the active application, so macOS keeps it out of full
    screen: harness runs are windowed. Full screen can only be checked by a manual launch.
  - Before pushing from the Mac: a full build, `--sanitize address`, and an ASan `ai-game` run with no
    `ERROR: AddressSanitizer` in `game.log`. Say that the Windows gates did not run.
- Playtesting: `python3.12 -I tools/play_posix.py [--fast] [--build]` plays under lldb (default: the ASan build).
  Each session goes to `build/playtest/<time>/`: `game.log`, and after a crash `crash.txt` (every thread's
  backtrace, the registers), a stack-only `core` and any `asan.*` report.
- Diagnostics:
  - `EE_GX_STATS=<n>`: draw summaries; it found the vertex-stride bug from "clip x 3e20".
  - `EE_GX_RENDER_SCALE=1`: no render scale. `EE_WINDOW_EVENT_LOG=1`: window events.
  - `EE_CONFIG=<file>`: a scratch `config.yaml`. `lldb --batch -o run -k "bt"` for crashes.
  - lldb regex breakpoints are too slow on this binary; a temporary `fprintf` is faster (remove it before
    committing).
- Bug patterns found here (PORT_LEARNINGS.md "The POSIX game with real data", "macOS: sound, full screen..."):
  - LP64 leftovers: `long` in GPU structs, pixel pointers, float bits, 0x80000000 sentinels, 32-bit sums;
  - Windows CRT behaviour: CR LF text mode and `C:\` paths, now both in `PlatFOpen`;
  - arm64 float->int saturation;
  - UB that the 32-bit original survived.

## Building the data

The game reads one loose `Data` folder next to the exe:
- db tables as text (CSV/YAML, with an `id` on every row);
- the rules documents in `Data/db`: `combat.yaml`, `gameoptions.yaml`, `civbuilder.yaml`, `civpowers.yaml`,
  `aipersonalities.yaml`, `epochs.yaml`;
- models as glTF, textures as PNG.

There is no `data.ssa` and no registry. Settings live in `config.yaml` next to the exe (`EE_CONFIG` overrides it).

```
python -I tools/open_data.py build build/open_data/tree [--force]  # from the two installs (read only), ~4 min
python -I tools/open_data.py build build/open_data/tree-binary --binary   # same tree, binary forms unconverted
python -I tools/open_data.py pack build/ee_open_media-<sha>.zip --part media   # release part: all but Data/db
python -I tools/open_data.py pack build/ee_open_db-<sha>.zip --part db         # release part: Data/db (the tweakables)
python -I tools/fetch_data.py [OUT] [--without textures-4x]      # pull the release parts into one folder (data/)
python -I tools/open_data.py check build/open_data/tree
python -I tools/make_play_dir.py C:\EE-play [--data TREE|ZIP|DIR ...] [--movies DIR]   # exe + data + movies
```

The db converter is `tools/eedata/eedata.py`. Run `-h` for the full list.
- `check`: **the data gate**:
  - a text -> db -> text round trip of every table;
  - every value the game reads equals retail;
  - record sizes;
  - generated specs and headers current.
- `cppgate`: the game's C++ text compiler over every table, plus content-dump parity.
- `specs` / `cppgen`: regenerate `specs.json` and the generated C++ headers (e.g. `include/ee/EEWorldConstants.h`).
- `extra TEXTDIR OUT [--units N --effects --epoch --options --civ --rules-demo]`: test content past the old caps.
- `export` / `import` / `install`: binary <-> text conversion.
- `doc epochs TEXTDIR`: a readable epochs table.

The rules documents are written by `tools/eedata/rules.py` from the same constants as the C++ defaults
(`src/Empire Earth/Database/EERules.cpp`). After changing a rule default on either side, rerun `check` and `cppgate`.

`open_data.json` in a tree records a hash of the converter code. The harness rebuilds a tree whose converter code
changed, so a format change never runs against a stale tree.

## Gates

Before merging or pushing any code change, all of these are required:

1. `python -I build.py -j 10` and `python -I build.py -j 10 --test-profile windowed-software` both link (0/0).
2. `python -I tools/eedata/eedata.py check` prints PASS. If tables, rules or the text compiler changed, `cppgate`
   must pass as well.
3. `bash tools/gates.sh` (about 40 minutes, test profile; logs in `build/gates/`):
   - a tour on the binary tree and on the open tree: menus, settings tour, input check, 120 s of play. Pass:
     `STOP <windowed session complete> reached`;
   - a seeded 2-AI Hard Small game to the time cap on both trees. Pass: outcome `time-cap` at PT ~13,700-14,500 with
     both AI players alive (e.g. units `[0, 56, 39]`). Unit counts vary a little between runs.

- Harness self test, no game, about 1 minute: `python -I tools/harness/selftest/selftest.py`. Run it after any
  change under `tools/harness/`.
- One harness session at a time on this machine. Don't run the gates while another agent uses the harness.
- `--no-render` AI games skip most 3D draws, so their windows look garbled by design. Judge display only from the
  tours.

## Data and release rules

- Never commit retail game data or bulk derivatives of it. The `.gitignore` covers `build/` and `toolchain/`.
- Data bundles go only to **GitHub Releases** of this repository (no git, no LFS). Create them with
  `gh release create ... -R Protryon/empire_earth_next --target main` (always `-R`: with the upstream remote present,
  gh may pick the wrong repository).
- The data ships in three parts that unzip into one folder, each its own release (`tools/fetch_data.py` pulls them):
  - `open-media-<sha>`: `ee_open_media-<sha>.zip` (`pack --part media`: models, original textures, sounds, fonts);
  - `open-textures-4x-<sha>`: `ee_textures_4x-<sha>.zip` (`Data/Textures-4x`, tools/upscale);
  - `open-db-<sha>`: `ee_open_db-<sha>.zip` (`pack --part db`: `Data/db` and `open_data.json`).
- Republish the db part whenever the tables, rules documents or db converters change; the media part only when
  the models, textures or other converters change; the 4x part when tools/upscale's output changes.
- The original game's campaigns stay out of the bundle.

## Git

- Work on `main`. For parallel agents, create worktrees by hand
  (`git worktree add -b <name> ../empire_earth_next-<name> main`), merge them back with `--no-ff`, then remove them.
- Merge, never rebase: both for upstream (see PORT.md "Upstream sync") and for `origin/main` when a push is
  rejected. The owner also pushes from other machines (e.g. macOS), so fetch before pushing.
- Never push to the `upstream` remote (its push URL is disabled on purpose). Nothing about this repository goes to
  the upstream project: no branches, releases, README mentions or issues.
- One concern per commit. Commit messages end with the co-author line from the session.
- Secrets (API keys) are never written into the repository.
