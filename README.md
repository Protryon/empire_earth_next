# empire_earth_next

*Empire Earth: The Art of Conquest* (Stainless Steel Studios / Sierra, 2002), built from the source reconstruction of
the `empire_earth` project with a current toolchain: clang-cl and lld-link, x86_64, the Microsoft C++ standard
library and the Universal C runtime. It starts from the reconstruction's `fixes-all` branch (the original game plus
every original-bug fix) and is free to diverge from the original from here on. `PORT.md` describes the port, and
`PORT_LEARNINGS.md` records every problem met on the way and its fix.

This repository contains no game data and no original binaries. You need a legal copy of the game to play anything
you build.

## Getting started

The game is the program plus one `Data` folder. Part of `Data` is in this repository and the rest comes from
GitHub releases:

| Part | Where it comes from |
|---|---|
| `Data/db` (the tables and rules) and `Data/ai` (the computer players) | this repository: `data/db`, `data/ai` |
| models, textures, sounds, music, fonts | the media release, [open-media-ddb67f1](https://github.com/Protryon/empire_earth_next/releases/tag/open-media-ddb67f1) (`ee_open_media-ddb67f1.zip`) |
| the 4x textures (optional, 1.3 GB) | [open-textures-4x-cb74e27](https://github.com/Protryon/empire_earth_next/releases/tag/open-textures-4x-cb74e27) (`ee_textures_4x-cb74e27.zip`) |

The links are the current media releases. A newer one replaces them when the models or textures change; the
[releases page](https://github.com/Protryon/empire_earth_next/releases) lists them all, and `tools/fetch_data.py`
always takes the newest. Ignore the old `open-db-*`, `open-data-*`, `textures-4x-*` and `build-*` releases: the
game no longer reads them.

**Windows.** Install the requirements under [Building](#building), plus the GitHub CLI (`winget install
GitHub.cli`, then `gh auth login`): `fetch_data.py` downloads the releases with it. Then:

```
git clone https://github.com/Protryon/empire_earth_next && cd empire_earth_next
python -I tools/fetch_wgpu.py               # the graphics library, into toolchain/
python -I build.py -j 10                    # -> build\clang\exe\EE-AOC.exe (about 2 minutes)
python -I tools/fetch_data.py               # the media releases into data\ (--without textures-4x: skip the 1.3 GB)
set EE_DATA_ROOT=%CD%\build\data-root       # cmd; PowerShell: $env:EE_DATA_ROOT = "$PWD\build\data-root"
build\clang\exe\EE-AOC.exe
```

`fetch_data.py` also makes `build/data-root`: its `Data` is a link to `data/` (the repository's db and ai plus the
media), beside the `Users` folder and the `.cfg` files. The game's settings (`config.yaml`) and logs land there.

**macOS (Apple Silicon) and Linux.** See [Linux and macOS](#linux-and-macos) for the requirements. Then:

```
python3.12 -I tools/fetch_posix.py wgpu sdl3 fonts
python3.12 -I tools/xbuild.py game -j 8      # -> build/xbuild/<target>/game/ee-aoc (macOS: also Empire Earth AoC.app)
python3.12 -I tools/fetch_data.py
EE_DATA_ROOT=$PWD/build/data-root build/xbuild/<target>/game/ee-aoc
```

`<target>` is `macos-arm64` or `linux-x86_64`.

**Movies** (optional): the game plays the retail intro and campaign movies if they are converted from a retail
disc's `.bik` files. `python -I tools/make_play_dir.py C:\EE-play --data build/data-root --movies DIR` (needs ffmpeg)
makes a stand-alone play folder with them.

**Staying up to date:**

```
git pull
python -I tools/fetch_wgpu.py               # only downloads when its pin changed
python -I build.py -j 10                    # macOS/Linux: python3.12 -I tools/xbuild.py game -j 8
python -I tools/fetch_data.py               # only downloads a media part that has a newer release
```

The db tables, rules and AI scripts come with `git pull`. The game reads them from `data/` directly, so there is
nothing to copy.

## What it builds

One statically linked x86_64 `EE-AOC.exe` with the static C runtime. The original's DLLs are linked into it: the engine
(`Low-Level Engine.dll`, with libjpeg and the DCL), the TCP protocol, the mixer, the Default random map generator
and the DX7 hardware T&L renderer (the plug-ins are registered in `src/Low-Level Engine/PIMModule.cpp` instead of
being loaded from DLLs), and the language strings of `Language.dll`. The non-T&L renderer `DX7HRDisplay.dll` is
gone. The renderer draws through GX, its DirectDraw 7/Direct3D 7 subset reimplemented on the wgpu crate (in the
Rust library, `ee_gx`; DX12, or Vulkan where available, with WARP on machines without a GPU). Sound goes through miniaudio,
music through dmusic and movies through pl_mpeg (all vendored, statically linked). The exe imports Windows system
DLLs only.

| Module | Was | Source |
|---|---|---|
| `exe` | `EE-AOC.exe` (game; its libpng and zlib are gone: PNG is ee_openformats' reader) | `src/Empire Earth/`, `src/RTS Engine/`, `src/ThirdParty/lua` |
| `lle`, `jpeg` | `Low-Level Engine.dll` (engine core, libjpeg, the DCL) | `src/Low-Level Engine/`, `src/ThirdParty/` |
| `default` | `Default.dll` (random map generators) | `src/Empire Earth/Random Map/Random Map Generators/Default/` |
| `dx7hrtnl` | `DX7HRTnLDisplay.dll` (T&L renderer; now on wgpu) | `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/` |
| `tcp` | `TCP Protocol.dll` (network) | `src/Low-Level Engine/Network Engine/TCP Protocol/` |
| `mixer` | `Miles Sound System Mixer.dll` (sound; now on miniaudio) | `src/Low-Level Engine/Sound Engine/Mixer/` |
| (resources) | `Language.dll` | `src/Empire Earth/Language/Language.rc` |

The WON online library (lobby, routing, the lobby's GUI toolkit; `src/Empire Earth/WONAPI`, module `won`) is built
from the reconstruction's source too; its online services no longer exist. Every symbol is defined in source; the exe
is one plain lld-link.

## Building

Requirements (Windows 10/11):

- LLVM (clang-cl, lld-link, llvm-lib, llvm-nm): `winget install LLVM.LLVM`
- Visual Studio 2022 Build Tools with the C++ workload, for the MSVC standard library, the UCRT and the Windows
  SDK: `winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add
  Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add
  Microsoft.VisualStudio.Component.Windows11SDK.22621"`
- Python 3.12 (standard library only) and 7-Zip (`winget install 7zip.7zip`)
- Rust, through rustup (`winget install Rustlang.Rustup`): `rust/rust-toolchain.toml` pins the version and rustup
  installs it on the first build. `build.py` builds the Rust library (`ee_game`, with GX and wgpu inside; about 3
  minutes the first time, `python -I tools/rust_build.py gate` runs its tests).
  Its C interface is generated by cbindgen (`python -I tools/rust_build.py header`), one header per module:
  C++ includes `rust/<crate>/<module>.h` for the modules it calls (`rust/sim/units.h`, ...), never the umbrella
  `rust/all.h`, so a module's interface change rebuilds only its own callers.

Then:

```
python -I build.py                  # -> build/clang/exe/EE-AOC.exe (x86_64, the only target)
python -I tools/clang_build.py      # compile only, with a ranked error census (--census: the pointer-size warnings)
```

The game reads its data as one loose `Data` folder in its own directory: the contents of both retail `data.ssa`
archives (*Art of Conquest* and the original *Empire Earth*) and the original game's loose data, merged in the game's
search order, with the db tables as text (CSV/YAML), the models as glTF and the textures and other pictures as PNG.
These open forms are the only ones the game reads (docs/RUST_PLAN.md D9: no `.dat`, `.cem`, `.sst` or `.tga`). Neither
a `data.ssa` nor an installation of the original game is needed.

Two folders of that `Data` are in git: `data/db` is the game's `Data/db` (the db tables as text and the rules
documents, what a mod tweaks) and `data/ai` is `Data/ai` (the computer players' Lua scripts and their manifest).
`tools/open_data.py` makes the rest of the tree (the media) from the two installs (only read; its converters read the
retail binary forms), copies `data/db` and `data/ai` into it, and packs it as a bundle (it is derived from the retail
game):

```
python -I tools/open_data.py build build/open_data/tree       # the tree (about 4 minutes)
python -I tools/open_data.py sync build/open_data/tree        # Data/db and Data/ai from data/db and data/ai (seconds)
python -I tools/open_data.py db-regen                         # data/db converted again from retail (tools/eedata)
python -I tools/open_data.py pack build/ee-aoc-data.zip       # the bundle: the tree, fonts/, README.txt
```

`python -I tools/eedata/eedata.py check` compares `data/db` with a fresh conversion of retail and lists every
difference. The media are too large for git: GitHub Releases hold them, in two parts, the media (`pack --part
media`: models, the original textures, sounds, music, fonts) and the 4x textures (`Data/Textures-4x`,
tools/upscale). `python -I tools/fetch_data.py` (a home-made git LFS) unpacks the newest of each straight into
`data/`, beside `data/db` and `data/ai`, which it never touches; `data/.fetched.json` records each part's release
and files, so a part already current is skipped and a newer one replaces only its own files. It also makes
`build/data-root`, a data root whose `Data` is a link to `data/` (a junction on Windows), with the bundle's `Users`
and `.cfg` files: run the game with `EE_DATA_ROOT=build/data-root`, and its logs and settings land there, not in
the repository. The `v*` release packages carry `Data/db` and `Data/ai`; there is no db release.

To play, prepare a directory with the exe, that data and the movies converted for the build's player (`--movies DIR`
points at `.bik` files, e.g. an extracted retail disc; ffmpeg):

```
python -I tools/make_play_dir.py C:\EE-play [--data TREE|BUNDLE.zip] [--movies DIR]   # then run C:\EE-play\EE-AOC.exe
```

The game keeps its settings in `config.yaml` in its directory (it creates the file; `EE_CONFIG` names another file).
It uses no registry and no other settings store; when its directory is not writable, the file goes to the user's
configuration directory (`%APPDATA%\Empire Earth AoC`, `~/.config/empire-earth-aoc`, `~/Library/Application
Support/Empire Earth AoC`).

The exe comes with its `.map` and `.pdb`. Sources are staged under `build/eex` in the checkout before they
are compiled (the compiler names them `C:\EEX\...` in `__FILE__`, as the original build tree did). A full build takes about two minutes on a 12-core machine.

Builds are incremental through an object cache shared by every worktree of the machine (`tools/objcache.py`):
a unit whose source, flags and every file it includes (by content) match an earlier compile copies that object
instead of compiling; the summary line says how many (`1364/1364 units (1363 cached, store 101 MB) in 39 s`).
The store is `C:\EEX-objcache-store` (`EE_OBJ_CACHE=<dir>` moves it, `EE_OBJ_CACHE=off` compiles everything),
pruned to `EE_OBJ_CACHE_MAX_GB` (default 4) at the end of each build; `python -I tools/objcache.py stats|prune|clear`.
Objects do not depend on the staging root or the worktree: every compile maps those paths to fixed names
(`__FILE__` reads `C:\EEX\...` whatever `EE_ORIG_SRC_ROOT` is) and is `/Brepro`. The Rust library is cargo's: set
`RUSTC_WRAPPER=sccache` (optional, if sccache is installed) to share its compiles too.

The movies need converting once: `python -I tools/convert_movies.py <game directory>` (needs ffmpeg) writes an
`.mpg` next to every `.bik` (`make_play_dir.py --movies` does it for a play directory).

Linux and macOS (in progress, PORT.md step 4): `tools/fetch_posix.py` fetches the pinned dependencies (a Debian
sysroot for cross-compiling to Linux, SDL3 sources, Liberation fonts) and `tools/xbuild.py` builds the
platform layer's tests, the window/text test programs and the compile census for `linux-x86_64`, `macos-arm64` and
`macos-x86_64` (`python -I tools/xbuild.py targets` says what this machine can build).

## Linux and macOS

The game builds for Linux (x86_64; natively, or cross-compiled from Windows against the pinned Debian sysroot) and
macOS (arm64; on a Mac only: the Xcode SDK may not leave Apple hardware). Requirements: clang and lld (Linux: e.g.
`apt install clang lld cmake ninja-build` plus SDL3's X11/Wayland development packages, see
`.github/workflows/posix.yml`), Python 3.12, CMake.

```
python -I tools/fetch_posix.py sdl3 fonts          # SDL3 sources, Liberation fonts (sha256-pinned)
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
`build/data-root` by default; `--data ROOT` for another data root, `--every MIN` for the timeline step). The log only reads the
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
python -I tools/sim.py -- --seed 3 --engine boom,classic --report r.json     # p2 boom, p3 classic (classic_lua)
python -I tools/sim_bench.py --name eng --seeds 2-7 --engine "classic;classic,boom"   # ';' between variants
python -I tools/sim_bench.py --name base --seeds 2-33 --engine boom,classic --cache    # a rerun reuses the games
python -I tools/sim_bench.py --name v1 --seeds 2-33 --engine boom,classic --ai-scripts X --screen 2-17 --screen-baseline build/sim/bench/base
```

`--cache` keeps every finished game (report, gzipped trace, progress) in the main checkout's `build/sim/cache`, keyed
by everything the game depends on: the EE-SIM exe's bytes, the data tree, the rules documents, the AI scripts'
contents (`--ai-scripts` by content, not by path), every option and `EE_*` variable, the seed. A commit builds
the same exe bytes in every checkout (PORT_LEARNINGS.md "Checkout-independent builds"), so the worktrees share
the games. A bench whose runs have all been played before (e.g. the baseline of each round) takes seconds; any
change to the exe or the scripts misses. `--screen SEEDS` plays those seeds first and drops every variant whose first computer is defeated at least
twice as often as the baseline's, and 3 games more (`--screen-factor`, `--screen-margin`); the rest go on to
`--seeds`. With 16 first-stage seeds it only catches gross regressions (32-seed defeat counts move by several games
from chance alone). The games run below normal priority (`--normal-priority` turns that off), so the windowed
gates can run beside a bench.

**Sequential tests** (`--sprt`; the statistics in `tools/sim_sprt.py`). A variant is played against a baseline on
paired seeds (the same seed and matchup with both builds), seed by seed, and the bench stops as soon as the test
decides, at most `--sprt-max` seeds (64):

```
python -I tools/sim_bench.py --name v1 --sprt --engine classic,ageofai --ai-scripts X --base-ai-scripts Y --cache
python -I tools/sim_bench.py --name v1 --sprt --engine classic,ageofai --ai-scripts X --sprt-baseline build/sim/bench/base
python -I tools/sim_sprt.py replay build/sim/bench/base build/sim/bench/v1      # the test over finished benches
```

The baseline is `--base-exe`/`--base-ai-scripts`/`--base-set`/`--base-env` (this bench with those changed), the first
of a two-value `--vary`/`--env-vary`, or `--sprt-baseline DIR`. A pair scores the first computer (`--sprt-player`) W
1, close 0.5, L 0 by the scorecard's rule; the test is a GSPRT of the mean gain d = variant - baseline, H0 0 against
H1 0.1 a game (`--sprt-gains`; about +6 points in 64 games), alpha = beta = 0.05, on a normal model of d whose
variance carries 4 pseudo-pairs of 0.2 (a run of identical games is not a variance of 0). It answers H1 (better), H0
(not better: equal or worse) or, at 64 seeds, inconclusive (the W-L-C stands as before). `--sprt-stat ahead` scores
which build was further ahead at the cap (the scorecard's mixed share) instead. Measured on the recorded benches' own
outcomes (`sim_sprt.py oc`): with W-L-C, a true gain of 0 is called better in 0.3% of runs and not better in 94%,
after 31 seeds on average; -0.1 is rejected after 22 seeds; +0.2 is accepted in 97% after 35; +0.1 in only 58% (33%
inconclusive: 64 seeds cannot resolve +0.1 reliably, nor +0.05, which needs about 170). Replayed over the 26 recorded
A/B benches of docs/AI_RESEARCH.md 8.86-8.94 (`sim_sprt.py replay`; those of 8.86 and 8.93 rebuilt from
build/sim/cache), it uses 74% of the pairs and contradicts no recorded conclusion: none is called better; the 8.86
options and the rejected 8.94 variants stop at H0 after 17-26 of 32 pairs (8.86's hurt after 59 of 63); the 8.93 raid
variants are undecided at 32 and the kept raids inconclusive at 64 (+0.047 a game); the kept 8.94 push is H0 in W-L-C
(53-2-9 -> 54-2-8: kept for the defeats it caused). `--sprt-stat ahead` calls the raids better after 12 pairs (and the
pre-merge raids after 19). For what the test leaves inconclusive the old rule stands: decide on 32 seeds, confirm on
64.

**The shared bench queue** (`--queue`; `tools/bench.py`). One daemon per machine plays every checkout's games on all
cores (`--jobs`, default the cores less 2; `bench.py jobs N` changes it live). A bench with `--queue` starts it when
none runs and submits its games; the daemon gives each slot its stage and ports (no `--port-base` to pick), plays a
game whose cache key a queued or running game already has once for every bench that asks (and serves a finished one
again for 6 hours), and starts games by `--priority` (`confirm` before `explore`), then first come. Each game is
played by the requesting checkout's own tools and inputs, its key checked again when it starts. Killing a bench drops
its queued games (its running ones finish, into the cache). Logs: the main checkout's `build/sim/queue/queue.log`,
each game's files in `build/sim/queue/jobs/`.
Each slot keeps one EE-SIM process that plays its games one after the other (`EE-SIM --games`, below; `bench.py serve
--single`: one process per game): a slot worker (`bench.py slotwork`, log `build/sim/queue/slot-<k>.log`) per
slot and checkout, ended after 60 s idle and on pause or stop. A direct bench does the same with `--batch`. A game
that fails before its game ran is retried in a process of its own.

```
python -I tools/sim_bench.py --name v1 --queue --priority confirm --seeds 2-65 --engine classic,ageofai --cache
python -I tools/bench.py status          # running and queued games by requester
python -I tools/bench.py pause           # before tools/gates.sh: the running games finish, none starts
python -I tools/bench.py resume
```

`tools/sim.py` stages the data tree per slot (`build/sim/stage/<slot>`: hard links, the tree is only read) and passes
EE-SIM.exe its options (`EE-SIM.exe --help`). `--engine` picks each computer seat's engine (classic,
random, an AI's id such as boom, or next for the manifest's first AI; docs/AI_LUA_PLAN.md) through the lobby's engine
boxes, as the lobby's host does: one value for every seat, or a comma list by seat. `classic` is the lobby's Classic:
the AI the manifest marks `"classic": true` (classic_lua, the classic computer player's port to Lua). Every computer
player runs a Lua AI: the C++ classic's decision code was deleted in stage 5 (docs/AI_RESEARCH.md §8.90). The report (JSON) holds the outcome, the game time and per player the
epoch, citizens, army, buildings, population and resources every `--sample-seconds`, and `wall_phases` (the wall
seconds of start-up, menus, lobby, loading and game). See PORT_LEARNINGS.md "The
headless simulation" for what it shares with the game and where it differs.

**The scripted scenario editor gate**: `python -I tools/sim_editor.py` opens the scenario editor in EE-SIM, runs
`tools/sim_editor/script.txt` (the editor's actions: maps, terrain, elevation, units, walls, gates, a quick save) and
compares the world's digests with `tools/sim_editor/expected.txt` (`--update` rewrites it; the commands:
`src/Empire Earth/Empire Earth/EESimEditor.cpp`). No tour opens the editor: this is its only automated check.

**Several games a process.** `EE-SIM.exe --games FILE` (or `-`: stdin) plays one game a line, one after the other, in
one process: the start-up is paid once, each game is left as a player leaves it (CancelGame, the post-game summary)
and the next starts from the single player menu. A line holds the game's options (`--seed --ai --map-size --map-type
--difficulty --lobby --engine --game-seconds ...`, `--out DIR` for DIR/report.json, trace.txt and err.txt, or
`--report`, `--trace`, `--err` FILE); the command line's other options (`--set`, `--cheats`, `--personality`,
`--ai-trace`, `--port-offset`, ...) hold for every game. A line is read when the last game has ended, so a bench
can feed games from a queue. stderr carries `EE-SIM GAME k BEGIN id`, `EE-SIM GAME k END code outcome` and `EE-SIM
BATCH END n`. Each game's report (its wall times aside) and trace equal the game's run alone, in any order
(PORT_LEARNINGS.md "Several games a process"). A failed game (an error, the wall timeout) ends its process. From
Python, `tools/sim.py`'s `Batch` keeps one process per slot and starts a new one after a failure or when the process
options or the environment change:

```
import sim
stage = sim.prepare("open", "slot0")
with sim.Batch(stage) as b:     # popen=dict(creationflags=...), wrap=nice-wrapper, log=FILE: optional
    r = b.play(["--seed", "3", "--engine", "classic,boom", "--ai-trace", "--port-offset", "2000",
                "--report", "r.json", "--trace", "t.txt"], err="e.txt")   # -> rc, outcome, wall, report, restarted
results = sim.play_games([dict(args=[...], err=...), ...], stage)          # a list, a failed start retried once
```

**Benches on a Linux machine** (headless, many cores; Ubuntu 24.04 x86_64). `python3 -I tools/xbuild.py sim` builds
the game and links `ee-sim` beside it (`build/xbuild/linux-x86_64/game/`): the same exe, started as the simulation by
its name (`ee-aoc --sim` is the same). It needs no display (SDL's dummy video driver) and no GPU, but a Vulkan
adapter: Mesa's lavapipe (`mesa-vulkan-drivers`). The sim tools find it by default. On a fresh machine:

```
sudo apt-get install -y git gh && gh auth login            # access to the private repository and its releases
gh repo clone Protryon/empire_earth_next && cd empire_earth_next
bash tools/setup_linux_bench.sh                            # packages, ports, build, media into data/, smoke game
python3 -I tools/sim_bench.py --name base --data open --ai-scripts data/ai --seeds 2-65 --jobs $(nproc)
python3 -I tools/ai_scorecard.py build/sim/bench/base
```

`--data open` takes `build/open_data/tree` when there is one, else `build/data-root` (tools/fetch_data.py, without
the 4x textures: `data/` with the media). Every run's stage gets this checkout's `data/db` and `data/ai`, so a pull needs no new data unless a
media release changed (then `tools/fetch_data.py --without textures-4x`); after a pull:
`python3 -I tools/xbuild.py sim --incremental` (a header change: a full build). A crash prints `EE-SIM FAULT signal ...`
frames as `ee-sim(+0xOFFSET)` in the run's err.txt: `addr2line -f -C -e build/xbuild/linux-x86_64/game/ee-sim
0xOFFSET`. The cross-platform check (the same seeds on Windows and Linux, the same data release and scripts):

```
python -I tools/sim_bench.py --name xplat --data DATA --ai-scripts data/ai --seeds 2-9 --difficulty Medium   # each machine
python -I tools/sim_compare.py WINDOWS_BENCH LINUX_BENCH     # build/sim/bench/xplat of each; exit 0: identical
```

## Upstream

The `upstream` remote is the `empire_earth` reconstruction. Its `fixes-all` branch is merged in, never rebased (see
`PORT.md`).

## License

No license is granted. Empire Earth is the property of its rights holders. The third-party libraries under
`src/ThirdParty/` keep their own licenses.

The "Age of AI" computer player (`data/ai/ageofai/`) is a Lua port of Grille's
[Empire Earth: Age of AI](https://github.com/Grille/Empire-Earth-Age-of-AI) (GPL-3.0, Copyright Grille and the Age
of AI contributors), ported with the author's permission; those files are GPL-3.0 (`data/ai/ageofai/LICENSE`), and
`data/ai/ageofai/UPSTREAM` names the upstream commit they follow.
