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
`relink.py`, `implib.py`, `exe_fulllink.py`, `rsrc_rebuild.py`, `verify.py`, `ee.py`, the fetch scripts,
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

## Rules of the build

- Compiler flags (`tools/clang_build.py`): i686, MSVC 19.44 compatibility, C++17, `/arch:IA32` (x87), `/GS-`,
  `-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks`. The last three tame undefined behaviour the code
  relies on; removing them is planned work (`tools/clang_ubdiff.py`, `build.py --sanitize-null`; see
  PORT_LEARNINGS.md).
- `link/` holds the original images' import and export tables and the exe's link tables (WON stubs, member aliases).
  The exe's member aliases (data the original placed inside other objects) carry VC6 offsets. The build moves them to
  this build's layout by member name, through the frozen VC6 layout reference `tools/harness/vc6_layout.json.gz`.
  Making them real members in source is a pending cleanup.
- Test-only builds (`--test-profile`, `--sanitize-null`) change only staged copies of a few units; `src/` is never
  edited by a build.

## Next steps

- The null-pointer UB: run the `--sanitize-null` build through tour and AI games, fix every reported site, drop
  `-fno-delete-null-pointer-checks`; then `-fwrapv` and `-fno-strict-aliasing`.
- Remove the clang dialect bridges: the `#ifdef __clang__` `const T&` forwarding overloads become `const` parameters,
  the `theTempN` call-site locals go.
- Member aliases in source instead of link tables; the WON library's source (PORT_LEARNINGS.md "For WONAPI/WONLobby").
- Port the rest of the harness (human driver, per-PT differential) where still useful.
- Platform layer (DirectDraw/D3D7, DirectInput, Miles, Winsock), then 64-bit and other platforms.
