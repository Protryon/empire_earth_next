# Crate restructure: ~40 crates into the owner's layout

Lane state (one file per lane). Branch `rust-crates`, from `origin/rust` 907e34d92. Phase 1 (this file and
`tools/crate_move.py`) is done. Phase 2 (the move) is done on the merge of `origin/rust` 22734d86b (ae379c7ad, the
snapshot): steps base, data, audio, net, render, sim, game, won, down (one commit each), the module headers, the
include rewrite. Its results are in "Phase 2: results" at the end.

The owner's decision (2026-11-15): `ee_base`, `ee_data`, `ee_sim`, `ee_render`, `ee_audio`, `ee_net`, `ee_ui`, the WON
crates and `ee_game` (the one staticlib, replacing the `ee_rust` umbrella). `xtask` stays tooling. Behaviour must not
change: this is a move of files and paths, nothing else.

## Summary

- **Every edge but six follows the rules already.** The six and their fixes are in "Edges that break the rules";
  all are file or item moves (nothing behind a callback, no new code).
- **The GE math is eight modules, not four**: `types`, `um`, `transform`, `hull` and also `vector`, `point`,
  `line`, `bbox` (the four name them; nothing else in the cluster names anything outside it). `ee_ge::ffi` is cut in
  two at `// ---- GETerrainMesh's queries ----`: the math's C ABI goes to `ee_base::math::ffi`.
- **ee_won splits cleanly into three crates** (`ee_won_net` <- `ee_won_gui` <- `ee_won_lobby`): after two modules are
  reclassified (`chat` and `plaintext` to net), no path goes upward. Recommendation: three crates. Six items become
  `pub`, and one `cfg(test)` stand-in needs a feature (`c_locale`) so that the gui and lobby tests see what they saw.
- **Headers**: during the crate moves they keep today's names (a 20-line change in `xtask/src/split.rs`), so no C++
  changes until the header commit. Then one header per module, `include/rust/<crate>/<module>.h`: 262 headers, one
  cycle (four ee_won_net modules, merged into one header).
- **A trial of the whole move** (the script over a copy of 907e34d92, then `cargo fmt`): `cargo check` 0 errors and
  0 warnings, clippy `-D warnings` clean, rustfmt clean, and `cargo test --workspace --no-fail-fast` runs 4,342 tests
  (baseline on 907e34d92: 4,342 passed, 9 ignored, 83 test binaries) with 3 failing: two only because the trial did
  not regenerate the headers (`the_checked_in_header_is_current`, `units::header_consts`), one to look at in phase 2
  (`plat_sys::thread::tests::auto_reset_event_releases_one_wait_and_clears`: timing, or R2). The headers regenerated
  in the trial differ only in `ee_app.h`, `ee_ge.h`, `ee_gfx.h`, `ee_openformats.h`, `ee_won.h` and the new
  `ee_math.h`, `ee_png.h` (step `down`, as planned). The C++ builds and the runtime gates are phase 2.
- **Main risks**: which objects the linker pulls changes with the crate boundaries (examples, test binaries, the WON
  drivers, tools that link only part of the library); `cfg(test)` is per crate; a POSIX-only duplicate symbol in
  `ee_base`'s tests. See "Risks" and, for what phase 2 met, "Phase 2: results".

## The layout

| New crate | Modules (old crate -> module) | Notes |
|---|---|---|
| ee_base | `plat_core` `plat_sys` `plat_ui` `plat_sdl` `util` `error` `panic` `random` `time` `ts` `ustring` `codec` `fs` `input`, `math` (8 GE modules + the math half of `ee_ge::ffi`), `png` (from ee_openformats) | windows-sys 0.59 (plat_core) and 0.60 (plat_sys) both kept: plat_core's is `windows_sys_059` |
| ee_data | the crate as it is | `ee_data_cabi` becomes its `staticlib` crate type (`tools/eedata/cppgate.py`: `-p ee_data --lib`) |
| ee_sim | `units` `combat` `astar` `formations` `rmap` `tech` `scenario` `ai` `netmsg`, `app` (`map`, `storm`, `cheats` of ee_app, with their `_ffi`/`_tests`) | ee_ai's `build.rs` (the test Lua) is ee_sim's; the three replay examples are ee_sim's |
| ee_render | `ge` (less the math) `gfx` `gx` `dx7` `openformats` (less png) | bin `eemodel_gate` (`tools/eemodel/cppgate.py`: `-p ee_render`) |
| ee_audio | `sound` `mixer` | |
| ee_net | `net` `net_tcp` | |
| ee_ui | the crate as it is | |
| ee_won_net, ee_won_gui, ee_won_lobby | ee_won's modules by group (below) | |
| ee_game | root: ee_rust's `lib.rs`; `screens`, `app` (the rest of ee_app: `ambient`, `camera`, `game_options`, `settings`, `minimap`, `save_io`, `map_ffi`, `ffi`) | the staticlib `ee_game.lib` / `libee_game.a` |
| ee_rmap_gatelib | unchanged (tooling) | depends on ee_sim, ee_render, ee_base (see F6) |
| xtask | unchanged | |

Module names are the old crate names less `ee_`, so a module is found from its C prefix (`ee_units_*` is
`ee_sim::units`) and every `rust/crates/ee_x/src/F` path maps to one new path. The old crate roots become
`<module>/mod.rs`; every other file keeps its place under its module's directory, so `#[path]`, `include_str!` and
`include!` paths stay valid (the script recomputes them anyway).

## Edges that break the rules

From `crate_move.py analyze` (every path in code, comments and strings left out, `use` trees expanded, `self`/`super`
resolved, re-exports followed). All other edges go down: base <- data <- sim <- render, base <- audio, base <- net,
base <- ui, base <- won, game <- everything.

| # | Edge | Fix (in the script) |
|---|---|---|
| F1 | ee_app `map_ffi` (to ee_sim) -> `ee_ge::tmobj` (the terrain mesh, render) | `map_ffi` stays in `ee_game::app`; `map.rs` (the logic) goes to ee_sim. Its C functions keep their names. |
| F2 | ee_won `imgdec` -> `ee_openformats::png_to_picture` (render) | the PNG reader (`png.rs`, stb_image's port; it names only `Failure` and `fail`) goes down to `ee_base::png`, with `Failure` and `fail` (now `pub`). ee_openformats names them there. |
| F3 | the math's C ABI (`ee_ge::ffi`) -> `terrain`, `physical` (render) | `ffi.rs` is cut at the terrain section: the math half is `ee_base::math::ffi`, the rest `ee_render::ge::ffi` (each with the `use` lines it needs; the 10-line private `put` copied to both). ge's `golden.rs` (the math golden) stays in render and names `ee_base::math::ffi`. |
| F4 | the math named only four modules | `vector`, `point`, `line`, `bbox` go with them (they name only `types`, `um`, `line`). |
| F5 | ee_combat `store.rs` imports `ee_combat_store_{unit,player,stats}_of`, which ee_rust exports | a link-time edge only (no cargo dependency), as today. Kept in ee_game for the pure move. Follow-up: units and combat now share a crate, so the bridge can move into ee_sim and become direct calls (the replay examples' stand-ins go then). |
| F6 | ee_rmap_gatelib -> ee_ge (whole) | the tool's C++ (`GEValues.cpp`) calls `ee_ge_viewport_*`, `ee_ge_va_*`, `ee_ge_pa_*`, which stay in render: the gatelib depends on ee_render (and so compiles wgpu once). If that is too slow: `gx` (wgpu) can sit behind a default feature of ee_render, which nothing else names. |

Rust-to-Rust `extern "C"` bridges (a crate declaring a function another Rust crate exports): 23 crate pairs; all go
down or stay inside one new crate except F5.

## ee_won

297 Rust files (386 with the goldens). Each module classified by the WON library part it ports (its doc comment and
C++ origin): **net** WONCommon, WONSocket, WONCrypt, WONAuth, WONServer, WONRouting, WONDir, WONDB, WONMisc,
WONInternal, WONConfig, WONFile, WONStatus; **gui** WONGUI with its platform layer, skins and image decoders; **lobby**
WONLobby and the EE lobby glue. The lists are `WON_GROUPS` in the script.

| Group | Modules | Files | Lines |
|---|---:|---:|---:|
| net | 107 | 144 | 70.9k |
| gui | 64 | 105 | 55.8k |
| lobby | 33 | 47 | 32.7k |

`crate::` paths between the groups (`crate_move.py won`):

| From | To | Refs (normal / dev) |
|---|---|---|
| gui | net | 16 / 5 |
| lobby | gui | 5 / 0 |
| lobby | net | 78 / 3 |
| any | upward | **0** |

There were five upward paths with the first classification, all fixed by placing two leaves right: `chat.rs` (the
chat command parser's character types, used by gui's `guistring`, `guiconf`, `cfgtab` and net's `smartroute`) and
`plaintext.rs` (D8's page text, used by net's `ffi`) are net; `chat_golden.rs` tests lobby's `chatobj`, so it is
lobby. The groups talk mostly through the C++ call tables, which is why so few paths cross them.

**Recommendation: three crates**, `ee_won_net` <- `ee_won_gui` <- `ee_won_lobby`, each depending on ee_base only
below it. What that needs (all fixups in the script):

- `pub(crate)` -> `pub` for what crosses: all of `chat.rs`'s (`CrtChat`, `units`, the command numbers) and
  `ffi::give`.
- `CrtChat` has a `#[cfg(test)]` C-locale stand-in for the C runtime's `towupper`, `isspace`, ... The gui and lobby
  tests used it (one crate, one `cfg(test)`); as separate crates they would get the real CRT. It becomes
  `cfg(any(test, feature = "c_locale"))`, a feature of ee_won_net that gui and lobby turn on in their
  dev-dependencies (the `no_cpp` pattern of ee_ui). The game's build never turns it on.
- No module of net or gui names a test helper of another group (checked: no `cfg(test)` item crosses).

The split's other benefit: 3 x 16 release codegen units instead of 16 for 160k lines (see R1).

## Headers

Today: `xtask cbindgen` runs cbindgen once over `ee_rust` (one shared header: renames, order, dropped duplicates)
and `split.rs` cuts it into one header per crate, giving each item to the crate whose sources define it.

**During the crate moves** (steps base ... down): the header names do not change. `split.rs` gets a `header_of`:
in a merged crate, a file under `src/<module>/` (or `src/<module>.rs`) gives its items to `ee_<module>` (`ee_units`,
`ee_ge`, ...), ee_game's root to `ee_rust` (the common header, as now), the three WON crates to `ee_won`. The
trial's headers are the same names; the header check decides whether they are byte-identical (cbindgen's item order
may move when crates merge; if so the commit regenerates them, and C++ is unaffected). The only new headers come with
step `down`: `ee_math.h` (included by `ee_ge.h`, whose items name the math types) and `ee_png.h`;
`rust_includes.py` adds them where a C++ file names their items. Old crates cbindgen never read (`plat_*`, `random`,
`panic`, `gx`: not in `extra_bindings`) get `/// cbindgen:ignore` on their module, so their `#[no_mangle]`
`Plat*` functions do not appear in the headers (C++ declares them in `PlatFile.h` etc.; cbindgen honours the
annotation on `mod` items, cbindgen 0.29.4 `utilities.rs`).

**The header commit**: one header per top-level module of each crate, `include/rust/<crate>/<module>.h`
(`rust/sim/units.h`, `rust/base/math.h`, `rust/ui/button.h`, `rust/won_net/hostop.h`, `rust/game/screens.h`; a
crate root's items in `rust/<crate>/<crate>.h`). `crate_move.py headers` computes it from today's headers:

- 14,280 items, 262 headers: base 10, data 2, sim 10, render 4, audio 2, net 2, ui 47, won_net 89, won_gui 63,
  won_lobby 30, game 3. A large module (units: 1,860 items; screens: 4,208) is still one header; going one level
  deeper there is a later option.
- Header includes follow the item references (code only, as `split.rs` does): one cycle,
  `won_net/{cppcore, hostop, opobj, thread}`; `split.rs` merges a cycle's modules into one header (named after the
  one with the most items, `won_net/hostop.h`).
- C symbol names do not change. cbindgen's renames are already global (one run over one crate); merging crates adds
  no new name to the shared namespace, because the per-crate split never changed what cbindgen saw. The new risk is
  the other way: a module cbindgen never parsed becoming visible to it, handled by `cbindgen:ignore` above.
- `ee_rust.h` (umbrella) becomes `rust/all.h` and `ee_rust_common.h` `rust/common.h`. The commit also writes
  forwarding headers at the old names (`rust/ee_units.h` including `rust/sim/units.h`, ...), not generated (xtask's
  stale check ignores them), so every C++ unit still compiles.

**The include commit**: `rust_includes.py` learns nested header paths (`rust/<crate>/<module>.h`) and treats an
include of an old name like the umbrella today: replaced by the headers of the modules whose names the file uses
(a file that names none keeps every module header of that old crate). Then the forwarding headers are deleted and
CLAUDE.md's rule becomes "include `rust/<crate>/<module>.h`". Today 95 C/C++ files include `rust/ee_won.h`; each will
include only the WON modules it names.

## Everything else that names crates or paths

| What | Change |
|---|---|
| `rust/cbindgen.toml` | `include`/`extra_bindings`: the new crates (the script writes them per state); renames unchanged |
| `xtask` | `main.rs`: the binding crate `rust/crates/ee_game` (a path rewrite); `split.rs`: `header_of` (crate steps), then the module headers (header commit); `capture-digest` unchanged |
| `tools/rust_build.py` | `-p ee_game`, `ee_game.lib` / `libee_game.a`; `fmt_packages` reads the crate dirs (unchanged) |
| `tools/build.py`, `tools/xbuild.py`, `tools/gx_test/build.py` | through `rust_build.lib_path`/`ensure` (unchanged code); xbuild's golden paths (`rust/crates/ee_won/src/golden_*.txt`) rewritten |
| `tools/eedata/cppgate.py` | `-p ee_data --lib`, `ee_data.lib` / `libee_data.a` |
| `tools/eemodel/cppgate.py` | `-p ee_render --bin eemodel_gate` |
| `tools/sim_capture.py` | the replay examples are `-p ee_sim` |
| `tools/rmap_gate/rmap_gate.py` | unchanged (`-p ee_rmap_gatelib`) |
| `tools/rust_includes.py` | header commit: nested paths, old names as umbrellas |
| `tests/won/*`, `include/ee/wonapi/*.h`, `include/lle/*.h`, `src/**` | comments and paths `rust/crates/ee_x/...` rewritten; their `#include "rust/ee_*.h"` in the include commit |
| `tools/eedata/specs.json`, `overlay.py`, ... | `source` paths into ee_data / ee_ui: unchanged crates, unchanged paths |
| golden replay paths, capture digests | in the crates' sources (`include_str!`), moved with them; `capture-digest` is xtask |
| docs | CLAUDE.md, README.md, PORT.md, tools/harness/README.md, docs/*_FORMATS.md, MODDING_PLAN, AI_RESEARCH rewritten; PORT_LEARNINGS, RUST_PLAN and the lanes' progress files keep their history (the new PORT_LEARNINGS section gives the map) |

The path rewrite is one rule for every text file: `rust/crates/<old>/<path>` -> the file's new path (exact files from
the move's map, then directories). The script's trial rewrote 2,088 files (1,685 under `rust/crates`).

## Risks

- **R1 What the linker pulls (met in the trial, fixed).** Archives are pulled object by object, and an object is a
  codegen unit. As separate crates, a test binary or example linked only the crates it named; merged, the objects it
  needs also hold other modules' calls into the game's C++ (the units' host, the Lua runtime, the DX7 trampolines,
  SDL). The trial's test build failed to link 9 cargo programs that way (the replay examples, the lib tests of
  ee_game, ee_ui, ee_won_*, ee_rmap_gatelib, ee_base's integration test). Fix: a `build.rs` in each such crate
  (`LOOSE_LINK` in the script) passes the linker "leave unresolved" (`/FORCE:UNRESOLVED`, `-Wl,--unresolved-symbols=
  ignore-all`, `-Wl,-undefined,dynamic_lookup` on macOS) for that crate's cargo-linked programs only. The game's
  static library is linked by build.py/xbuild.py, never by cargo, so it is untouched; a test that called a missing
  function would crash, not pass. The opposite case, two definitions: ee_sound's golden fakes of 17 functions ee_base
  defines for real (the Thread System, `PlatSleep`, ...). They get their own symbol names in the test build
  (`#[export_name = "ee_sound_fake_..."]`, and `#[cfg_attr(test, link_name = ...)]` on sys.rs's imports), so the
  golden still drives the fakes. Phase 2 met the same in the programs outside cargo: the WON and network drivers
  of xbuild.py link loosely too (`TEST_LINK_LOOSE`), and ee_net_tcp stayed a crate ("Phase 2: results").
- **R2 `cfg(test)` is per crate.** 257 `cfg(test)`/`cfg(not(test))` sites in library code. Once two old crates
  share a crate, a module's tests see a sibling compiled for test (and vice versa). `analyze` lists the sites whose
  crate another old crate of the same new crate names: astar (by units), ge (dx7, gfx, openformats), gx (dx7),
  net_tcp (net), plat_core (8 crates), plat_sdl (input), plat_sys (fs, ts), sound (mixer), time, ustring, util,
  screens (ee_rust). Most are test helpers (`test_globals`) and stand-ins for C++ imports, which only make a test
  binary link with less. The gate's test count and results decide; a behaviour difference gets a feature like WON's
  `c_locale`.
- **R3 A POSIX duplicate symbol.** ee_plat_core's tests define stand-ins for `ee_plat_posix_set_last_error`,
  `ee_plat_posix_set_last_error_errno` and `PlatEventSet` (`#[cfg(all(test, unix))]`), which ee_plat_sys defines for
  real. In one crate, on Linux and macOS, that is two definitions. Fix for phase 2: drop the stand-ins; the two tests
  that read them (`path.rs` `c_api_on_the_real_file_system`, `time.rs` `event_timers_set_the_event`) read
  `plat_sys`'s last error and a real `PlatEvent`. Windows does not compile them; `cargo check --target
  x86_64-unknown-linux-gnu` and an object build there check it here, but nothing on this machine runs the POSIX tests.
- **R4 Test identity.** Test names gain a module prefix (`units::...`); counts and results must match. The baseline
  is counted with `--no-fail-fast` on the merged base (a known flaky test, `movobj a_missing_file_ends_the_thread_at_once`,
  failed once in the phase-1 baseline).
- **R5 Mechanical edits are formatting.** Split `use` statements and longer paths need `cargo fmt` (run by the
  script's commit procedure); rustfmt reorders imports only in the touched files.
- **R6 Snapshot drift.** The script makes every state from a snapshot commit (`--from`), with all its edits as
  fixups that must apply exactly once (it stops otherwise). Phase 2 merges `origin/rust` first and uses the merge as
  the snapshot; a branch merged into rust after the move must be re-applied by path (the PORT_LEARNINGS map).
- **R7 Open branches.** Every lane branched before the move conflicts on every path. The move should land in a quiet
  window, right after a merge batch, as the owner planned.

## The script

`tools/crate_move.py` (read only except `move`):

- `analyze`, `won`, `headers`: the tables above (`--markdown OUT`).
- `move --from REV [--to STEP] [--out DIR] [--no-git] [--dry-run]`: writes the tree of a state. The snapshot is
  `git archive REV rust` (in `build/crates/snap-<sha>`); the other files are read from REV. Steps, cumulative, in
  commit order: `base`, `data`, `audio`, `net`, `render`, `sim`, `game` (each moves the old crates of that new
  crate whole), `won` (the three crates), `down` (F1-F4: the math, the cut `ffi.rs`, the PNG reader, ee_app's
  simulation parts). Without `--no-git` it stages `rust/crates` and every file it wrote.
- How it rewrites: a lexer blanks comments, strings and chars; every path (`use` trees expanded, `crate`/`self`/
  `super`/`$crate`, old crate names) is resolved in the old tree and in the new one; when they differ, the module
  part of the path is replaced (`ee_units::x` -> `ee_sim::units::x` or `crate::units::x`). A `use` tree whose leaves
  move apart is split into one `use` per leaf, attributes and visibility repeated. `use ee_x as _` (links a crate)
  becomes the new crate's, or goes inside that crate. Module declarations move to their new parent (made `pub`
  outside `cfg(test)`), old crate roots get one, `#[path]` is dropped where a file now sits at its name.
  `include_str!`/`include_bytes!`/`include!` are re-pointed. Manifests: a crate that keeps its name keeps its
  manifest with its path dependencies mapped; a merged crate gets one made from its old crates' (external
  dependencies merged, features unioned, renamed on a version clash) and its path dependencies from the paths its
  code names (normal, dev, build). All other edits are `FIXUPS` and `FIXUP_FUNCS`: the `pub`s and `c_locale` of
  WON, the PNG reader's `Failure`, two `# Safety` sections for functions that became `pub`, two
  `clippy::module_inception` allows (`render::gx::gx`, `audio::mixer::mixer`), ee_sound's fake names (R1), the
  loose link in ee_ai's `build.rs`, the tools' package names, `split.rs`'s `header_of`.

## Phase 2: commits and gates

Commits (each made by `crate_move.py move --from <merge> --to <step>`, then `cargo fmt`, then the C++ build):

1. Merge `origin/rust`, `strip_markers.py`, headers current (its own commit if needed).
2. `base`, `data`, `audio`, `net`, `render`, `sim`, `game`: one commit each (crate moves; `split.rs`'s `header_of`
   comes with `base`). Headers unchanged; C++ unchanged except comments and tool paths.
3. `won`: the three WON crates (fixups: `pub`, `c_locale`).
4. `down`: the math, the PNG reader, the cut ffi, ee_app's simulation parts; `ee_math.h`, `ee_png.h`;
   `rust_includes.py` run.
5. The header split (xtask), with the forwarding headers.
6. The include rewrite (rust_includes.py), forwarding headers deleted.
7. CLAUDE.md, the PORT_LEARNINGS section, this file.

Each commit: `cargo check --workspace --all-targets`, the header check, and `build.py --compile-only` where C++
changes; the full set on the last: both builds 0/0 (`--sim`, `--test-profile windowed-software`), the Rust gate
with the same test count, `xbuild.py wontest`, the oracle seeds 2-7 against the reference, every capture profile,
the editor and civ builder gates, `eedata check` + `cppgate`, `gates.sh` (harness lock), `xbuild.py targets` and a
POSIX check (`cargo check --target x86_64-unknown-linux-gnu`; the POSIX C++ build if this machine can).

## Appendix: the analysis, generated

`python -I tools/crate_move.py analyze`, `won`, `headers` on 907e34d92 print the full tables (every forbidden edge
with its files, every bridge, every `cfg(test)` site's crate, every location-dependent macro, every header's items
and includes).

## Phase 2: results

The move was made from the merge of `origin/rust` 22734d86b (ae379c7ad), one commit per step, then by hand where
the steps' tree met the tests and tools (each its own commit):

| Commits | What |
|---|---|
| base, data, audio, net, render, sim, game, won, down | `crate_move.py move --from ae379c7ad --to <step>`, `cargo fmt`, headers regenerated (their item sets equal the snapshot's after every step) |
| headers per module | `include/rust/<crate dir>/<module>.h`, 259 headers (ee_won_net's cppcore, opobj, thread share `won_net/hostop.h`), `rust/all.h`, `rust/common.h`; forwarding headers at the old names |
| C++ includes | `rust_includes.py`: 548 includes narrowed, 36 added, the forwarding headers deleted; the original units' copies under `tests/net` find the old names in `tests/net/rust_names` |
| fixes found by the gates | `lang_strings.py`'s path; `units::header_consts` reads `src/units`; a plat_sys timed-wait test takes `test_globals` (same test binary as plat_core's harness-clock tests now); the split test's umbrella order |
| ee_net_tcp | stays a crate of its own (see below); the WON and network test programs link loosely |

**Deviations from the layout (for the owner).**

- `ee_net_tcp` is a crate of its own beside `ee_net`. The Network Engine reaches its TCP plug-in only through the C
  ABI `ee_tcp_*`, and `ne_protocol_test` (xbuild.py nettest) puts stand-ins of that ABI in its place; merged, the
  plug-in's objects are the Network Engine's and the stand-ins clash with them (duplicate symbols). Its header is
  still `rust/net/net_tcp.h`.
- ee_plat_sdl was tried as a crate of its own too (its old manifest said it was one so that text-only programs
  don't pull SDL in); it did not help, because ee_input inside ee_base imports `PlatMouseSetRelative` and pulls it
  back, so it stays a module of ee_base (reverted) and the test programs link loosely instead.
- Test and example programs (cargo's, and xbuild.py's WON and network drivers) link with "leave unresolved"
  (R1): `build.rs` in the crates of `LOOSE_LINK`, `TEST_LINK_LOOSE` in xbuild.py. The game, EE-SIM and the gate
  tools' own links are unchanged.

**Gates on the final tree** (Windows; staging per CLAUDE.md, no `EE_ORIG_SRC_ROOT`):

| Gate | Result |
|---|---|
| `build.py -j 3 --sim` | EE-AOC 737 inputs, EE-SIM 738: 0 unresolved, 0 duplicates |
| `build.py -j 3 --test-profile windowed-software` | EE-AOC 737 inputs: 0 unresolved, 0 duplicates |
| Rust gate (`cargo test --workspace --no-fail-fast`, clippy `-D warnings`, rustfmt, header check) | 4,344 passed, 0 failed, 9 ignored: the merge's 4,343 (counted the same way on ae379c7ad) and the new `header_of` test; clippy, rustfmt clean; 259 headers current |
| oracle seeds 2-7 vs `empire_earth_next/build/rust_oracle/ref` | identical, all six (on the step-down exe and again on the final one) |
| capture profiles, seeds 2-3 | PASS: oracle 200, late 204, space 202, calamity 212, missile 22, effects 6, scenario 43, units 175, app 18, groups 2, transport 185 digests equal to the goldens |
| editor gate (`sim_editor.py`) / civ builder (`--civbuilder`) | PASS (14,448 lines) / PASS (4,062 lines) |
| `eedata check` / `cppgate` | PASS / PASS |
| `xbuild.py wontest` | exit 0, 143 programs exit 0 |
| `xbuild.py nettest` | the 7 NE programs exit 0 (client 0 of 1,500 differing, protocol over its stand-in plug-in); `ee_session_test` fails to compile exactly as on the snapshot (ee/EEARust.h against the ee_stand world): not this lane's |
| `xbuild.py targets`; Linux | windows-x86_64 and linux-x86_64 build here, macOS needs the Xcode SDK. `cargo check --target x86_64-unknown-linux-gnu --workspace --all-targets`: ee_base, ee_data, ee_sim, ee_audio, ee_net, ee_net_tcp and the WON crates check; ee_render's `openformats/surface_file.rs` has 4 LP64 `c_long`/`c_int` errors that the snapshot has too (so ee_ui, ee_game and the gatelib were not checked). ee_base's Linux test objects define the three R3 symbols once each. Running the POSIX tests, the POSIX C++ build and macOS: not verified here |
| `gates.sh` (harness lock, slot 0, open tree) | GATES PASS: tour PASS; ai-game time-cap, PT 14,026, units [4, 43, 44], alive_computers [2, 3]; lan-game PASS (host 2,693, joiner 2,687 PTs) |

**Left for the lead.** Every open lane branch conflicts on every moved path: merge them by path (old -> new is
mechanical: `ee_x` -> its module, PORT_LEARNINGS "The crate restructure"). The bridges of F5 can now move into
ee_sim. The two deviations above want the owner's yes. `C:\EEX-crates` (staged sources of the first phase-2 builds,
before CLAUDE.md said not to set `EE_ORIG_SRC_ROOT`) is outside this worktree and was left in place.
