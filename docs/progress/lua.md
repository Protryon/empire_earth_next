# Lane: Lua (the computer players' script language out of the C++ build)

## Round 1 (branch rust-lua1, 2026-11-23): Lua 5.5 from mlua-sys's vendored build; src/ThirdParty/lua deleted

State: done. The 27 C files of `src/ThirdParty/lua` (Lua 5.4.7 and the one-unit build `ee_lua.c`), their headers
and licence text (55 files), `include/ee/EELuaApi.h` and the build's library units (`tools/units.py` `LIB_UNITS`,
`LIB_MEMBERS`, `lib_jobs`) are gone: 57 C/C++ files, 26,101 lines. The computer players' scripts run on Lua 5.5.1,
compiled unmodified from the `lua-src` crate by `mlua-sys` (`lua55`, `vendored`) into the Rust library.

### What was there

- **The version**: Lua 5.4.7, the files unmodified from github.com/lua/lua tag v5.4.7 (their header said so; the
  two commits that touched the folder, 4a66072de and feb391d18, changed only `ee_lua.c`).
- **The patches**: none in Lua's files, five through macros Lua leaves open, set in `ee_lua.c` before the sources
  were included as one unit: a fixed string hash seed (`luai_makeseed`), no pivot randomisation in `table.sort`
  (`l_randomizePivot`), `^` as square-and-multiply with an error for a non-integral exponent (`luai_numpow`),
  `nan`/`inf`/`-inf` as a float's text (`lua_number2str`), and ASCII-only character classes in the libraries
  (`isalpha` and the others redefined). Only base, table, string, math and utf8 were compiled.
- **The users**: one. The state, the sandbox, the bindings and the save code were already Rust (ee_sim
  `ai/lua_host.rs`, `units_lua.rs`, `units_bind.rs`, `units_more.rs` over `ai/lua_api.rs`, since R5a.8); the C++
  included Lua's headers only for seven `static_assert`s in `EECPNext.cpp`.

### The design

- **mlua-sys, not mlua's high-level API.** `mlua-sys = { features = ["lua55", "vendored"] }` builds Lua with the
  cc crate (cl on Windows, the platform's cc on macOS and Linux) and links it into `ee_game.lib`. The runtime keeps
  calling the C API through its own `extern "C"` declarations (`lua_api.rs`): mlua's safe API raises errors through
  `C-unwind` frames, which abort under the release profile's `panic = "abort"` (PORT_LEARNINGS R5a.8, second round),
  it adds its own objects to every state (the byte cap and the collector's pace would move with every mlua
  release), and the 8,000 lines of bindings are written against the stack API.
- **5.5, not 5.4.** A stock 5.4 build seeds its string hash from the time and an address, so `pairs` over string
  keys (some 25 loops in `data/ai`) would differ from run to run; lua-src offers no way to set the macro. In 5.5 the seed
  is `lua_newstate`'s third argument: the host passes the old build's constant (`HASH_SEED`).
- **The determinism the macros gave, now in the host** (the owner allowed a one-time behaviour change, 2026-11-23):

  | Old macro | Now |
  |---|---|
  | fixed hash seed | `lua_newstate(alloc, host, HASH_SEED)` |
  | `table.sort` without pivot randomisation | `ai/lua_sort.rs`: 5.5.1's quicksort, statement for statement, with `l_randomizePivot` a constant (`~0`, what ltablib.c suggests); installed over the library's function |
  | `nan`, `inf`, `-inf` | the host's `tostring`, `print` and `string.format("%s")`; `..` of a non-finite float gives the C library's text |
  | `^` integral only | not kept: `^` is the C library's `pow` (x^2 is x*x). No script uses `^` |
  | ASCII character classes | not kept: the string library follows the C locale for bytes above 127. The scripts and the names the engine hands them are ASCII |

- **math.random** was never Lua's: `math.random` and `randomseed` are removed by the sandbox, `ai.random` is the
  host's SplitMix64, its state saved with the memory. Unchanged.
- **The allocator** is the host's (`realloc`/`free` under the byte cap), as before. 5.5 allocates differently, so
  every byte count moved (a new state: 13,208 bytes, was 13,117).
- **Saves**: unchanged. A save holds the scripts' memory table as the host's own value encoding (EELM), not Lua's
  state; a loaded game runs the modules again.

### What changed for a script (Lua 5.4.7 -> 5.5.1)

- a float's text is `%.15g`, or `%.17g` when that does not read back (was `%.14g`): `tostring(math.pi)` is
  `3.1415926535897931`;
- `math.frexp` and `math.ldexp` exist (exact operations);
- `#t` of a table with holes may give another border (`#{1,nil,3}` is 1, was 3);
- a `for` loop's control variable is read-only; `global` declarations exist (`global` stays an ordinary name:
  `LUA_COMPAT_GLOBAL`);
- tracebacks name C functions differently ("in global 'print'", "in field 'format'");
- a non-integral `^` computes instead of raising.

None of it changed a game: the oracle, the captures (classic_lua, and boom and Age of AI in the placement
profile) and the saveload rows are those of the old Lua. See the gates.

### Goldens

- Re-baselined (their own commit): the two Rust test goldens that print the state's byte counts,
  `golden_lua.txt` part B and `golden_units_lua.txt`, written by the tests (`EE_LUA_GOLDEN_WRITE=1`). They were the
  C++ driver's output; they are regression goldens of the Rust host now.
- Not re-baselined, identical: the oracle (seeds 2-7 against the main checkout's `ref`), the twelve capture
  profiles, `saveload_expect.txt`, the editor gates.

### Toolchains

- Windows (cargo through `tools/rust_build.py`): cc finds cl through the MSVC environment; Lua compiles with the
  profile's CRT (`+crt-static`). No extra set-up.
- Linux, cross from Windows: `rust_build.cargo_env` sets `CC_`, `AR_` and `CFLAGS_x86_64_unknown_linux_gnu` to
  LLVM's clang, llvm-ar and the Debian sysroot (as xbuild compiles the C++). Lua's archive builds. The library as a
  whole does not build for Linux on this branch, for reasons that are not this lane's: `alsa-sys` wants pkg-config
  for the target, and ee_sim has six `c_long` type errors on LP64 (`scenario/cinematic.rs`, `units/ucb_gate.rs`).
- macOS: builds on a Mac only (cc needs the SDK's headers), as the C++. Not run here.

### Gates (the final tree; origin/rust 17682d86c had not moved)

| Gate | Result |
|---|---|
| `build.py -j 10 --sim` | EE-AOC 410 inputs, EE-SIM 411, 0 unresolved, 0 duplicates |
| `build.py -j 10 --test-profile windowed-software` | 410 inputs, 0/0 |
| `rust_build.py gate` | PASS (6,660 tests, clippy, rustfmt, headers current); a first run lost ee_base's `com_is_initialised_on_platform_threads` under load |
| oracle, seeds 2-7 | identical to the main checkout's `ref`, the old Lua's (6,087, 5,300, 6,072, 7,649, 7,842 and 6,540 lines) |
| the 12 capture profiles, seeds 2-3 | PASS each: oracle 212, late 214, space 210, calamity 222, units 175, effects 6, missile 22, app 26, scenario 43, transport 185, groups 2, placement 4 digests equal |
| saveload, plain (seeds 2-7 at 600, 1350, 2100 s) | 18/18 identical (seed 7 in a second run: its first stage met a full disk) |
| saveload matrix (`--settings all --seeds 2-3 --at 1000,1900 --expect`) | PASS: every row as `saveload_expect.txt` (next seed 2: 915 and 276 ticks, as before) |
| `scenario_check.py` | 27/27 |
| `sim_editor.py`, `--civbuilder` | PASS (14,448 lines), PASS (4,062 lines) |
| `eedata.py check`, `cppgate` | PASS, PASS |
| `gates.sh` (the main checkout's tree) | GATES PASS: tour; ai-game time-cap, units [4, 38, 38], alive_computers [2, 3], engines classic_lua x 2; lan-game |
| Linux cross build | Lua's archive compiles (clang, the Debian sysroot); the library does not link there for other reasons (above) |

### Left

- A Linux and a macOS build and an ASan `ai-game` on the Mac (CLAUDE.md "macOS"): not possible on this machine.
- `^` and the locale-dependent classes are open to a script again. A check at `require` (refusing `^` in a chunk)
  or a locale-free string library would close them; neither is needed by `data/ai`.
- `docs/AI_RESEARCH.md` and `docs/AI_LUA_PLAN.md` §7 still discuss "Lua 5.4" as a choice: history, left as it is.
