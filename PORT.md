# empire_earth_next: the modern-compiler port

This repository is a fork of `empire_earth` (remote `upstream`), the source reconstruction of *Empire Earth: The Art
of Conquest*. It starts from upstream's `fixes-all` branch (the reconstruction plus every original-bug fix) and builds
the game with a current toolchain. Behaviour may diverge from the original; the long-term goal is a portable game.

## Upstream sync

Upstream rewrites `fixes-all` on every update (its fix commits are replayed onto the new upstream `main`). Take
upstream changes by merging, never by rebasing, so this repository's history is never rewritten:

```
git fetch upstream
git merge upstream/fixes-all
```

Source changes merge normally. The build tooling is this fork's own: upstream's VC6 build files (`tools/build.py`,
`relink.py`, `exe_roundtrip.py` (both deleted here), `implib.py`, `exe_fulllink.py`, `rsrc_rebuild.py`, `verify.py`, `ee.py`, the fetch scripts,
`include/dinkum/`) were rewritten or deleted here. When upstream changes one of them, keep this fork's version
(`git checkout --ours <file>`) and port anything relevant by hand. Upstream's matching stand-ins
(`include/LostDebug.h`, `EE_LOST_*`) are deleted here too: resolve new uses by dropping them.

## Where the port stands (2026-11-04)

1. **Compiler swap, validated.** The source was made to compile with clang-cl and link with lld-link next to the
   original VC6 build. Both builds were compared game tick by game tick: one-bit float differences (x87 rounding
   points) from the first tick, integer state identical for about 3,000 ticks (RNG to PT 3,066, AI to PT 2,856), then
   chaotic drift. No logic difference appeared inside that window, and the clang build played whole games. The VC6
   build has since been removed.
2. **Dinkum 2.33 replaced by the MSVC STL and the UCRT** (every module at once: STL objects and heap memory cross the
   DLL boundaries). All 846 units compile and all 8 images link with clang-cl, lld-link and the Windows SDK only.
   The game passes the menu/settings tour, plays, and plays AI games to their time cap in the harness.
3. **The test harness is this fork's own** (`tools/harness/`, ported from the reconstruction project): it stages a
   test-profile build and reads game memory by member name from the build's own record layouts.
4. **One static exe** (plan step 1 below, done): every game DLL and the language resources are linked into
   `EE-AOC.exe` with the static runtime; the plug-ins are a static registry. Tour, play and a 2-AI game to the time
   cap pass, also under full page heap.

## Rules of the build

- Compiler flags (`tools/clang_build.py`): i686, MSVC 19.44 compatibility, C++17, `/arch:IA32` (x87), `/GS-`,
  `-fno-strict-aliasing -fwrapv`. The last two tame undefined behaviour the code relies on; removing them is planned
  work (`-fno-delete-null-pointer-checks` went once the sanitizer games came back clean) (`tools/clang_ubdiff.py`, `build.py --sanitize-null`; see
  PORT_LEARNINGS.md).
- One image: every module's objects go into the exe link (engine and plug-ins first), the import libraries come from
  every module's original import table minus the game's own DLLs, the VC6 CRT and Dinkum. The engine and renderer
  units still declare their classes `dllexport` (clang-cl's vector deleting destructors; PORT_LEARNINGS.md "One
  static exe"), so the exe has an export table. A unit's `// UNIT-FLAGS:` line only adds its defines now.
- `link/` holds the original images' import and export tables and the exe's link tables (WON stubs, member aliases).
  The exe's member aliases (data the original placed inside other objects) carry VC6 offsets. The build moves them to
  this build's layout by member name, through the frozen VC6 layout reference `tools/harness/vc6_layout.json.gz`.
  Making them real members in source is a pending cleanup.
- Test-only builds (`--test-profile`, `--sanitize-null`) change only staged copies of a few units; `src/` is never
  edited by a build.

## Next steps

- The remaining UB flags: `-fwrapv` (`clang_ubdiff.py` + `-fsanitize=signed-integer-overflow` games), then
  `-fno-strict-aliasing`.
- x64: what is left is the `-Wshorten-64-to-32` class (1,274 sites, 1,139 of them `size()` into `long`, e.g.
  reverse loops). They are benign (no container approaches 2^31 elements) and the code is headed for a Rust
  rewrite, so they stay unless a site turns out to matter. Saved games and network messages are fine on x64:
  loaders read typed 32-bit ids and rebuild pointers, messages copy `long`/`float` members, and every raw on-disk
  record has the same size on both targets (`tools/record_sizes.py`; each is pinned by a `static_assert`).

## Plan: one static exe, no proprietary third-party code (owner, 2026-11-04)

Decisions: one statically linked `EE-AOC.exe` (static CRT `/MT`; only Windows system DLLs remain); Bink movies
converted once to MPEG-1/MP2 and played with pl_mpeg; only the T&L renderer is kept. The long-term direction is a
port to Rust, so new third-party code stays small, C, permissively licensed and behind narrow interfaces that a Rust
crate can replace.

1. **Fold the game DLLs into the exe.** (Done.) The engine, TCP protocol, mixer, Default random maps and
   DX7HRTnLDisplay are linked into the exe. The PIM plug-in scan (`PIMModule.cpp`, LoadLibrary + `PIMGetName`/
   `PIMGetType`/`PIMIsValid`) is a static table of the same entry points; `Language.dll`'s resources are the exe's.
   DX7HRDisplay is dropped. `/MT` replaces `/MD`. The harness stages one exe. Open: the engine's export table
   (until polymorphic `new[]`/`delete[]` are gone or clang emits vector deleting destructors on its own), the
   per-module header views (`tools/odr_check.py`: 28 inline functions with divergent copies, mostly older ODR
   clashes inside the game).
2. **Audio output: miniaudio** (done). `src/Low-Level Engine/Sound Engine/Mixer/Mixer.cpp` mixes the game's channels
   (volume, pan, pitch by resampling, loop, position) into one miniaudio playback device (WASAPI, DirectSound,
   waveOut, else the null device); samples are decoded whole on load (WAV/MP3/FLAC). miniaudio 0.11.21 is vendored
   (`include/miniaudio/`, options in `ee_miniaudio.h`, implementation `src/ThirdParty/miniaudio/miniaudio.c`).
   `Mss32.dll`, the Miles headers and `redist\win32` are gone, and so is the harness's silent mixer (the null device
   serves). Still open: the mixer hands a DirectSound object to the DirectMusic and Bink players (steps 3 and 4).
3. **Music: GothicKit/dmusic** (MIT, C11; segments, styles, bands, DLS, composer, its own synth) renders PCM into
   the miniaudio mix. `SEConductor`/`SEScore` move from the DirectMusic COM interfaces to it; motifs
   (`EnumMotif`/`GetMotif`), `AutoTransition` and the segment-end notifications are the parts to check first and
   extend if needed. The DirectMusic GUIDs and `include/winsdk/dmusici.h` go.
4. **Movies: pl_mpeg** (MIT, single header) decodes MPEG-1 video and MP2 audio; `DX7Movie` blits frames to its
   DirectDraw surface and feeds audio to miniaudio. `tools/convert_movies.py` turns the retail `.bik` files
   (Movies\, scenario cinematics) into `.mpg` with a user-installed ffmpeg; the game looks for `.mpg` first.
   `binkw32.dll` goes.
5. Then: DirectDraw/D3D7 and DirectInput (Microsoft system components, not in this pass), Winsock.

Each step ends with the harness gates and a PORT_LEARNINGS.md entry.

## Other next steps

- Remove the clang dialect bridges: the `#ifdef __clang__` `const T&` forwarding overloads become `const` parameters,
  the `theTempN` call-site locals go.
- Member aliases in source instead of link tables; the WON library's source (PORT_LEARNINGS.md "For WONAPI/WONLobby").
- Port the rest of the harness (human driver, per-PT differential) where still useful.
- Platform layer beyond the plan above, then 64-bit and other platforms.
