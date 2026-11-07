# empire_earth_next

*Empire Earth: The Art of Conquest* (Stainless Steel Studios / Sierra, 2002), built from the source reconstruction of
the `empire_earth` project with a current toolchain: clang-cl and lld-link, x86_64, the Microsoft C++ standard
library and the Universal C runtime. It starts from the reconstruction's `fixes-all` branch (the original game plus
every original-bug fix) and is free to diverge from the original from here on. `PORT.md` describes the port, and
`PORT_LEARNINGS.md` records every problem met on the way and its fix.

This repository contains no game data and no original binaries. You need a legal copy of the game to play anything
you build.

## What it builds

One statically linked x86_64 `EE-AOC.exe` with the static C runtime. The original's DLLs are linked into it: the engine
(`Low-Level Engine.dll`, with libjpeg and the DCL), the TCP protocol, the mixer, the Default random map generator
and the DX7 hardware T&L renderer (the plug-ins are registered in `src/Low-Level Engine/PIMModule.cpp` instead of
being loaded from DLLs), and the language strings of `Language.dll`. The non-T&L renderer `DX7HRDisplay.dll` is
gone. The renderer draws through GX, its DirectDraw 7/Direct3D 7 subset reimplemented on wgpu (statically linked
wgpu-native; DX12, or Vulkan where available, with WARP on machines without a GPU). Sound goes through miniaudio,
music through dmusic and movies through pl_mpeg (all vendored, statically linked). The exe imports Windows system
DLLs only.

| Module | Was | Source |
|---|---|---|
| `exe` | `EE-AOC.exe` (game; with libpng, zlib) | `src/Empire Earth/`, `src/RTS Engine/`, `src/ThirdParty/` |
| `lle`, `jpeg` | `Low-Level Engine.dll` (engine core, libjpeg, the DCL) | `src/Low-Level Engine/`, `src/ThirdParty/` |
| `default` | `Default.dll` (random map generators) | `src/Empire Earth/Random Map/Random Map Generators/Default/` |
| `dx7hrtnl` | `DX7HRTnLDisplay.dll` (T&L renderer; now on wgpu) | `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/` |
| `tcp` | `TCP Protocol.dll` (network) | `src/Low-Level Engine/Network Engine/TCP Protocol/` |
| `mixer` | `Miles Sound System Mixer.dll` (sound; now on miniaudio) | `src/Low-Level Engine/Sound Engine/Mixer/` |
| (resources) | `Language.dll` | `src/Empire Earth/Language/Language.rc` |

The WON online library is not part of the reconstruction (its services no longer exist): the exe links source
stand-ins for it (`src/Empire Earth/WONAPI/WONAPIStubs.cpp`). Every symbol is defined in source; the exe is one plain
lld-link.

## Building

Requirements (Windows 10/11):

- LLVM (clang-cl, lld-link, llvm-lib, llvm-nm): `winget install LLVM.LLVM`
- Visual Studio 2022 Build Tools with the C++ workload, for the MSVC standard library, the UCRT and the Windows
  SDK: `winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add
  Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add
  Microsoft.VisualStudio.Component.Windows11SDK.22621"`
- Python 3.12 (standard library only) and 7-Zip (`winget install 7zip.7zip`)
- wgpu-native, the renderer's graphics library (static, sha256-pinned GitHub release download into `toolchain/`,
  which is not tracked): `python -I tools/fetch_wgpu.py`

Then:

```
python -I build.py                  # -> build/clang/exe/EE-AOC.exe (x86_64, the only target)
python -I tools/clang_build.py      # compile only, with a ranked error census (--census: the pointer-size warnings)
```

The game reads its data as one loose `Data` folder in its own directory: the contents of both retail `data.ssa`
archives (*Art of Conquest* and the original *Empire Earth*) and the original game's loose data, merged in the game's
search order, with the db tables as text (CSV/YAML), the models as glTF and the textures as PNG. Neither a `data.ssa`
nor an installation of the original game is needed. `tools/open_data.py` makes that tree from the two installs (only
read) and packs it as a bundle (it is derived from the retail game):

```
python -I tools/open_data.py build build/open_data/tree       # the tree (about 4 minutes; --binary: unconverted)
python -I tools/open_data.py pack build/ee-aoc-data.zip       # the bundle: the tree, fonts/, README.txt
```

The private releases ship it in three parts that unzip into one folder: the media (`pack --part media`: models, the
original textures, sounds, music, fonts), the 4x textures (`Data/Textures-4x`, tools/upscale) and the db
(`pack --part db`: the tables and the rules documents, the part that changes most). `python -I tools/fetch_data.py`
pulls the newest of each into `data/` and skips the ones already current.

To play, prepare a directory with the exe, that data and the movies converted for the build's player (`--movies DIR`
points at `.bik` files, e.g. an extracted retail disc; ffmpeg):

```
python -I tools/make_play_dir.py C:\EE-play [--data TREE|BUNDLE.zip] [--movies DIR]   # then run C:\EE-play\EE-AOC.exe
```

The game keeps its settings in `config.yaml` in its directory (it creates the file; `EE_CONFIG` names another file).
It uses no registry and no other settings store; when its directory is not writable, the file goes to the user's
configuration directory (`%APPDATA%\Empire Earth AoC`, `~/.config/empire-earth-aoc`, `~/Library/Application
Support/Empire Earth AoC`).

The exe comes with its `.map` and `.pdb`. Sources are staged under `C:\EEX\` before they are compiled (env
`EE_ORIG_SRC_ROOT` moves it). A full build takes about two minutes on a 12-core machine.

The movies need converting once: `python -I tools/convert_movies.py <game directory>` (needs ffmpeg) writes an
`.mpg` next to every `.bik` (`make_play_dir.py --movies` does it for a play directory).

Linux and macOS (in progress, PORT.md step 4): `tools/fetch_posix.py` fetches the pinned dependencies (a Debian
sysroot for cross-compiling to Linux, wgpu-native, SDL3 sources, Liberation fonts) and `tools/xbuild.py` builds the
platform layer's tests, the window/text test programs and the compile census for `linux-x86_64`, `macos-arm64` and
`macos-x86_64` (`python -I tools/xbuild.py targets` says what this machine can build).

## Linux and macOS

The game builds for Linux (x86_64; natively, or cross-compiled from Windows against the pinned Debian sysroot) and
macOS (arm64; on a Mac only: the Xcode SDK may not leave Apple hardware). Requirements: clang and lld (Linux: e.g.
`apt install clang lld cmake ninja-build` plus SDL3's X11/Wayland development packages, see
`.github/workflows/posix.yml`), Python 3.12, CMake.

```
python -I tools/fetch_posix.py wgpu sdl3 fonts     # wgpu-native, SDL3 sources, Liberation fonts (sha256-pinned)
python -I tools/xbuild.py game -j 8                # -> build/xbuild/<target>/game/ee-aoc (macOS: also the .app)
build/xbuild/<target>/game/ee-aoc --smoke-test     # window + wgpu device + 30 frames; needs no game data
```

The game needs the game data (the bundle above; never committed here). It reads it from the data root: `EE_DATA_ROOT`
if set, else the program's directory (a macOS app bundle: its `Contents/Resources`). Without it the game stops with
"game data not found". Its settings go to `config.yaml` in the data root (a read-only app bundle: the user's
configuration directory, see above). `tools/make_play_dir.py` prepares a complete directory (anywhere: it needs only
the data tree or bundle):

```
python -I tools/make_play_dir.py OUT --posix-exe build/xbuild/linux-x86_64/game/ee-aoc [--data BUNDLE.zip] [--movies DIR]
OUT/run.sh                                         # sets EE_DATA_ROOT=OUT
```

Status: the Linux and macOS builds are checked by CI (`.github/workflows/posix.yml`, branch `posix-ci`: the game is
built and run with `--smoke-test` under Xvfb + lavapipe and on Metal, and without data it must exit with the "game
data not found" message). On macOS (arm64, Metal) the game runs with the data bundle: menus, the lobby and AI games
(PORT_LEARNINGS.md "The POSIX game with real data"). Linux with real data is not tried yet.

`tools/harness/posix_game.py` drives the POSIX game the way the Windows harness does (the menus, the lobby, an AI
game, telemetry and, on macOS, window captures), through a UI fixture the game carries (on only with `EE_TEST_UI`):

```
python -I tools/harness/posix_game.py --exercise random-game --play-seconds 60     # evidence: build/posix_game/<time>/
python -I tools/harness/posix_game.py --exercise ai-game --ai-players 2 --max-game-seconds 1800 --capture-every 120
python -I tools/xbuild.py game --sanitize address                                  # -> build/xbuild/<target>/game-address/
python -I tools/harness/posix_game.py --exe build/xbuild/macos-arm64/game-address/ee-aoc --exercise ai-game --play-seconds 120
```

## Testing

**Game logs.** Every game writes `Logs/game-<date>-<time>.log` in the data root (config.yaml `debug: game_log: false`
turns it off). It holds every player's state every 30 game seconds, human and computer alike: epoch, citizens and
what they do, army, buildings, towers, stock, income and losses, with who caused them. Epoch times and the computer
players' decision trace are in the same file, and every chat line with its game time: type notes
in the chat while playing. `python -I tools/game_log.py [LOG]` summarises the newest one (under
`data/` by default; `--data ROOT` for another data root, `--every MIN` for the timeline step). The log only reads the
game: a game plays the same with it on or off.

`tools/harness/` is a headless live-game harness (ported from the reconstruction project): it stages a test build
(`python -I build.py --test-profile windowed-software`), drives the menus, plays AI games with a time shim and
records telemetry, all on a GPU-less VM. See `tools/harness/README.md`.

**Headless simulation** (TEST ONLY; AI benchmarks and parameter sweeps): `python -I build.py --sim` also links
`build/clang/exe/EE-SIM.exe`, the game's own code (every unit of EE-AOC.exe) with the front end stubbed: a hidden
window, no rendering, no sound, the menus and lobby driven in-process the way the harness drives them, and the
physics thread sending its own communications ticks back to back instead of waiting for the clock. A seeded 2-AI
Hard Small game of 45 game minutes takes 13-17 s (4-7 s of it the game itself) instead of the harness's ~170 s,
and prints the harness's `EE-AI` trace for the same seed (`platform/PlatSim.h`, `src/Empire Earth/Empire Earth/EESim.cpp`):

```
python -I tools/sim.py -- --seed 3 --difficulty Hard --game-seconds 2700 --cheats none --report r.json --ai-trace --trace t.txt
python -I tools/sim.py -- --seed 3 --set aipersonalities.balanced.military.army_floor=0.2 --report r.json
python -I tools/sim_bench.py --name base --seeds 2-7 --repeat 2 --jobs 2      # build/sim/bench/base/results.csv
python -I tools/sim_bench.py --name floor --seeds 2-7 --vary aipersonalities.balanced.military.army_floor=0.1,0.2,0.3
```

`tools/sim.py` stages the data tree per slot (`build/sim/stage/<slot>`: hard links, the tree is only read) and passes
EE-SIM.exe its options (`EE-SIM.exe --help`). The report (JSON) holds the outcome, the game time and per player the
epoch, citizens, army, buildings, population and resources every `--sample-seconds`. See PORT_LEARNINGS.md "The
headless simulation" for what it shares with the game and where it differs.

## Upstream

The `upstream` remote is the `empire_earth` reconstruction. Its `fixes-all` branch is merged in, never rebased (see
`PORT.md`).

## License

No license is granted. Empire Earth is the property of its rights holders. The third-party libraries under
`src/ThirdParty/` keep their own licenses.
