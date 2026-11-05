# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later, the WON libraries first
(WONAPI/WONLobby: still stubs in the exe link, see "Linking the exe").

Order that worked: compile census → STL headers → stock-header patches → mechanical source passes → header bridges →
one-off sources → DLL links → exe link → stub audit against VC6.

## Tooling

| Tool | Use |
|---|---|
Since the VC6 removal (see "Removing VC6 and Dinkum" at the end), the build is `python -I build.py`
(`tools/build.py`). The phase-1 tools `clang_link.py`, `clang_vc6inc.py`, `objcmp.py` and `clang_session.py` are
gone, and their roles appear below only as history. Kept for the WON port: `clang_forscope.py`, `clang_reftemp.py`,
`clang_rtti_audit.py`.

| Tool | Use |
|---|---|
| `tools/clang_build.py [-j N] [--opt X] [module ...]` | compile census. Per-unit logs go to `build/clang/log/`, and the summary ranks the most frequent errors (names masked). Rerun it after every fix batch. |
| `tools/build.py` (`build.py`) | the build: compiles everything up front, then links the one exe (`exe_fulllink.py`) with lld-link (the DLL links of `relink.py` are gone with the DLLs). Writes `build/clang/exe/link_report.json`. `--test-profile windowed-software`: harness images; `--sanitize-null`: UBSan builds. |
| `tools/clang_forscope.py [--apply]` | hoists for-init variables used after their loop. |
| `tools/clang_reftemp.py [--apply]` | names temporaries passed to non-const references at plain call sites. |
| `tools/clang_ubdiff.py` | functions whose code depends on a UB-taming flag (with/without, `/Gy`). |
| `tools/harness/game_session.py` | the fork's live-game harness (`tools/harness/README.md`). |
| `tools/clang_rtti_audit.py` | lists `dynamic_cast` sites only VC6's lenient RTTI runtime resolves (see "RTTI"). Heuristic: check each hit. |

Process rules learned the hard way:
- **Run a fixer once per fresh census.** A second `--apply` on stale logs follows old line numbers and rewrites the
  wrong loops. Always delete `build/clang/log` and rerun `clang_build.py` between applications.
- **Caches must key on everything that affects the output.** The first link-side object cache keyed on source mtime
  only, so header edits and new global flags (`/GS-`) were ignored. `clang_link.py` now recompiles everything (~40 s
  for 845 units at -j 12).
- **Git Bash rewrites `/O1` and similar into paths.** Pass clang options to the scripts as `--opt=-O1`.
- **Write Python edits with the Edit tool or a quoted heredoc.** Regex replacement strings in an unquoted heredoc lose
  their backslashes, and a raw newline inside an f-string broke a script once.
- A fresh machine needs: Python 3.12, 7-Zip, LLVM (`winget install LLVM.LLVM`). Python's TLS store rejected
  archive.org on one box while curl worked. Download those archives with curl into `toolchain/dl/` and let
  the fetch script verify the pinned hash (now only `tools/fetch_dx7sdk.py`).

## Compiler flags (clang-cl, phase 1)

`--target=i686-pc-windows-msvc -fms-compatibility -fms-extensions -fms-compatibility-version=12.00 /Zc:wchar_t-
-fdelayed-template-parsing /arch:IA32 /GS- -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks`, VC6-only
flags dropped (`-G6 -Zm -Zi`, `/O1` replaced).

- **`-fno-delete-null-pointer-checks`** (found by a crash in the first long AI game): `EEIsLineToUnitBlocked` calls
  `theList->Scale(...)` and later tests `theList != NULL`. `Scale` never touches `this`, so under VC6 a null
  `theList` was harmless. Clang inferred non-null from the member call, deleted the test, and the next read faulted.
  The original code may rely on this anywhere (VC6 never exploited the UB), so the flag stays for the whole port.
  The source should be cleaned up later, then the flag can go. Removing it (2026-11-04 attempt):
  - `tools/clang_ubdiff.py` compiles every unit with and without the flag (`/Gy`) and lists the functions whose code
    differs. 6,147 differ in all, because the flag also stops clang assuming `this` and references are non-null. 1,136
    of them have fewer null tests without it. A sample showed mostly benign cases (`delete p` after `p` was used, so
    its null test goes away).
  - `clang_link.py --sanitize-null` builds images without the flag and with `-fsanitize=null -fsanitize-trap=null`
    (a `ud2` at any member call or reference binding through null; no runtime needed). Run them through
    `clang_session.py`; every trap is a site the game really reaches.
  - The first trap (`EEUIManager` ctor: `GetFormByID(kFormSaveGame)` null) came from code that had already run
    differently, not from the trapping line: without the flag, clang dropped null tests after UB that this
    sanitizer does not watch. Much of that UB is Dinkum's own: its containers do arithmetic on the null `_First` of
    empty containers, and those inlined bodies sit in hundreds of the 1,136 game functions.
  - Decision: keep the flag through the Dinkum swap. A modern STL removes that class at the root. Then rerun
    `clang_ubdiff.py` and the sanitizer games (add `-fsanitize=nonnull-attribute,pointer-overflow` traps), fix the
    remaining game-code sites, and drop the flag.
  - Outcome (2026-11-04): after the swap, the sanitizer games found four sites ("Undefined behaviour the VC6 code
    relied on" below). With them fixed, the tour, a 2-AI Small and a 4-AI Medium Hard game to the time cap reported
    nothing, and the flag was dropped. (`clang_link.py`/`clang_session.py` above are now `build.py --sanitize-null`
    and `tools/harness/game_session.py --build-dir build/clang-test-nullsan`.)

- `/Zc:wchar_t-`: VC6's `wchar_t` is `unsigned short`. This fixes `std::wstring = unsigned short[]` assignments and
  keeps the mangling (`G`) the same as VC6's.
- `/GS-`: VC6 had no stack cookies. Without it, the link needs `__security_cookie`/`__security_check_cookie`, which
  the VC6 CRT lacks.
- `-O1` for linking, not `/Od`. Inline member functions of dllimport classes (engine accessors, Dinkum `basic_string`
  members) must be expanded as VC6 did, or the images import functions the DLLs never exported.
- Neither clang flag nor `-fms-compatibility` accepts VC6 for-scope leaks or temporaries bound to non-const references
  (`/Zc:forScope-` is ignored; `-fms-reference-binding` does not exist in clang 22). Both need source fixes.

## Headers

- **Dinkum 2.33** (`include/dinkum`): `this->allocator` in vector/deque/xtree. The allocator member lives in a
  dependent base, and two-phase lookup finds `std::allocator` (90% of all errors at first). Deque iterators had no
  postfix `++/--`. VC6 fell back to the prefix operator (C4620), so clang-only postfix operators return the prefix
  result, keeping that meaning.
- **Stock VC6 headers** (patched copy): default template arguments repeated from `<iosfwd>`; explicit
  specialisations without `template<>` (xlocale); `ios_base` flag names (`skipws`, `unitbuf`, `hex`, ...) that
  resolve to the manipulator functions inside the stream templates (qualify them); `FIELD_OFFSET`'s null-pointer cast
  is not a constant expression (use `__builtin_offsetof`). Copy subdirectories too (`SYS/types.h`).
- **Missing includes surface**: `U2DPoint.h` used `std::find` without `<algorithm>`. VC6 found it through earlier
  includes or never instantiated it there.

## Source dialect

- **for-scope leaks** (most "undeclared identifier"): declare the variable before the loop. With a class-type
  iterator this turns copy-construction into default construction plus assignment (same behaviour, different VC6
  code). Several declarators (`for (k = 0, n = ...)`) were fixed by hand.
- **Temporaries bound to non-const references** (VC6 extension). The parameter types are fixed by the original's
  mangled names while VC6 is still the reference. Two fixes:
  - a heavily used callee gets `#ifdef __clang__` `const T&` overloads that `const_cast` and forward (URegistry alone
    had 230 call sites; constructors forward through delegating constructors). Delete them once the parameters can
    become const;
  - a one-off call site gets a named local (`T theTempN(args);`). This is not possible in constructor initializer
    lists, `while`/`for`/`else if` conditions, braceless bodies or right after a `case` label (a declaration there is
    jumped over: wrap it in braces). Use an overload there.
  - **Name hiding**: a derived class that declares the same virtual hides the base's forwarding overload
    (DX7Screen::CreateAWindow). Fix those call sites locally.
  - "no matching constructor/member function" with the note "expects an lvalue for Nth argument" is the same
    problem.
- **`pointer > 0`**: VC6 compared unsigned, so it means `!= NULL`.

## ODR: inline functions defined in one .cpp, called from others

VC6 always left an out-of-line COMDAT of an inline function it had emitted, and other units linked against it.
Clang inlines every local use and emits nothing, so the other units' references stay unresolved. These turned up as
"unbound"/"unwritten" traps in the exe stub report. Fixes:
- defined `inline` in a `.cpp` and called elsewhere: drop `inline` (EEAIHasResourceUnit, IsShoreTile,
  GetGroundPoint, CanAttackClass, GetPathAreaIDAt);
- defined inline in a header the caller does not include: include it (EEActiveGroup.h);
- defined inline in a `.cpp` but needed by every caller: move the definition into the header
  (GetPathSubSectorAt/ID(long, long)), or move a template member definition into its header
  (UReferenceContainer::Contains);
- **declared but never defined** because the original's linker folded them (ICF) into an identical function: give
  them real bodies from the folded function's code. The vector-size accessors read `_First` at +8 of a vector at +4,
  so it's `this->mMember.size()` for the member at that offset. A provisional name of an existing function becomes an
  inline forwarder (EEFn08282 → U2DComputeCenterPoint, flag read as a byte).

## Linking the DLLs (lld-link)

- `/MACHINE:I386` → `/MACHINE:X86`. Add `/SAFESEH:NO` (the VC6 CRT objects predate SafeSEH) and `/DEMANGLE:NO`
  `/ERRORLIMIT:0` (the stub planner needs the raw names of all unresolved symbols).
- VC6's LIB/DUMPBIN, the VC6 CRT and the vintage import libraries work with clang objects and lld-link unchanged.
- **Import libraries**: the by-name ones list only what the original image imported. Clang references more:
  - sxlrt233 (Dinkum DLL): `link/clang/sxlrt233.def`, the retail DLL's full export list → `llvm-lib /def`, appended
    to every link;
  - the engine: link lle first and append its own lld import library to the others.
- **Exports** (`.def` from the original's table): drop entries no clang object defines, listed in
  `exports_dropped.txt` (lle: 33 pooled string literals `??_C@` and a default-constructor closure `??_F`, which no
  retail or clang image imports). The C exports are undecorated in the `.def`, so match `_name` and `_name@n` before
  dropping anything.
- **Member-template instantiations are mangled differently.** VC6 leaves out the template arguments
  (`?InsertLine@?$UIListBox@...`), clang and modern MSVC keep them (`??$InsertLine@<args>@?$UIListBox@...`). Clang
  only exports one if it is instantiated explicitly (`template LLE_API void UIListBox<UWideString>::InsertLine<P>(...)`
  in lle/UILBString.h, clang only). `clang_def` then exports the VC6 name as an alias (`vc6name=clangname`), so VC6-built
  images (the retail exe imports one of them) still load against the clang engine.
- **Check mixed loading both ways**: every clang image's imports must be exported by the retail DLLs, and every
  retail image's imports by the clang DLLs. Then each clang module can be swapped in alone, which the per-module
  comparisons need.
- **Exported class templates**: clang applies `dllexport/dllimport` on a class template to every instantiation
  (the modern MSVC rule). VC6 instantiated non-exported specialisations locally. Keep the attribute only on the
  explicit instantiation that crosses the DLL boundary (`UIListBox`: `UILISTBOX_API` is empty under clang).

## Linking the exe (exe_fulllink with lld)

- `exe_fulllink` parses the linker log, so lld output needs `lld_parse_log`. lld's `/MAP` is MSVC format except for
  16-digit addresses (`MAP_RE`).
- **Function vs data for undefined symbols**: VC6 types an undefined function symbol 0x20; clang leaves the type
  null. Without a fix every missing function became a zero-filled data stub. Classify C++ names by their demangled
  form (`implib.is_cpp_data`) and C names by call/jmp rel32 relocations (`clang_coff_symbols`).
- Audit the stub report against the VC6 build's (`pass 1 unresolved by class`). The target is the same set: WON
  stubs, ICF/vslot/pure aliases and data member aliases, with **no traps** for first-party functions. At the end of
  phase-1 linking: clang won 303 / icf 36 / vslot 263 / pure 11 / data 75 / library 1, against VC6's won 299 / icf 36
  / vslot 263 / pure 11 / mismatch 1 / data 75 / library 2 / engine 2.
- The VC6 full link had aliased GetPathSubSectorAt/ID(long, long) to same-scope functions with other signatures and
  stack cleanup (latent crash). Proper bodies fixed both builds.

## RTTI: casts the VC6 runtime tolerated

The images keep using the system `msvcrt.dll` for `__RTDynamicCast`, and Windows 11's is a newer implementation.
Its base class descriptors differ in one flag: clang sets `BCD_HASPCHD` (0x40) and VC6 left it 0. For RTTI with the
flag the runtime takes a checked path that also requires the source type to be a base of the target. VC6-style
RTTI gets the old lookup, which only searches the object's base list for the target.
- The source cast `dynamic_cast<RTSDummyUnit*>(RTSUnit*)` (a tile chain's dummy head, linked through `RTSUnit*`
  although it is no `RTSUnit`) is a cast between unrelated hierarchies. It worked in the original and returned null
  under clang, and the first clang game load crashed in `RTSUnit::Link`. Fix: `RTSDummyHead(link)` in
  ee/RTSDummyHeader.h asks the object's own type (`typeid(*link) == typeid(RTSDummyUnit)`), null-safe. That is the
  same answer, since nothing derives from RTSDummyUnit.
- Run `tools/clang_rtti_audit.py` after new code arrives: it lists casts no class of the program can satisfy, from
  the objects' cast sites and the images' RTTI. It is a heuristic (at -O1 a descriptor address can come from a
  register), so read each hit.

## Running the game (ee's live harness)

- ee's `tools/boot/game_session.py` needs on the machine: MSYS2 with `mingw-w64-i686-gcc` (`C:\msys64\mingw32`),
  pip `pefile capstone`, ee's own `toolchain/` (its fetch scripts, `--dl` pointing at a download cache), and from
  ee's build: `tools/relink.py mixer` and `tools/xref_noreloc.py exe`.
- The harness puts a full-link exe into its windowed/software test profile by locating the original image's binary
  patch sites and proving each function's body instruction by instruction. That cannot work for clang code. The
  same policy is built from source instead (`clang_link.py --test-profile windowed-software`: text rules applied to
  the staged copies of EEPreferences.cpp and Empire Earth.cpp, each with an expected match count; src/ untouched).
  `clang_session.py` replaces only that locator step and reads the telemetry globals from the map by name.
- lld's map has 16-digit addresses; ee's map readers expect LINK 6's 8. `clang_link.py` rewrites the exe's map.
- The harness refuses a relinked engine that lacks any original export. `clang_session.py` exempts exactly `??_C@`
  and `??_F`, which nothing imports.
- Results (2026-11-04): the clang exe with retail DLLs, and the clang exe + engine + TCP + Default (retail renderer
  in its software profile, silent mixer), pass the full menu/settings tour, load a random-map game and play 120 s
  without a fault.
- The human driver (`--human-script`, also used empty for lockstep) finds `EEPhysics::ReportStatistics`'s statics by
  VC6's register use. `clang_session.py` looks them up by map name instead (lld's map lists static symbols too).

## Differential comparison (VC6 build vs clang build, same source)

Recipe (run sequentially; synced streams are timing-independent): the reference is the fork's VC6 full link through
ee's own `--fulllink` path, the candidate the clang test image through `clang_session.py`, both with
`EE_HUMAN_LOCKSTEP=1 --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard --max-game-seconds 600
--time-factor 32 --no-render --seed 2 --pt-rate 40 --diff-stream auto --human-script <comment-only file>`. Then
`ee tools/boot/differential/compare.py A/diff.stream.txt.gz B/diff.stream.txt.gz`, and `dump.py --pt X --a .. --b ..`
for the differing fields at a snapshot PT (every 100).

What it shows: the SIM hash differs from PT 1, but only by one-ulp float differences (x87 intermediates spilled at
other points: animation fractions, unit matrices). Behaviour drifts only through them. Seed 2: RNG draws identical
to PT 3,066, AI identical to PT 2,856, unit counts to PT 3,361, then the games diverge chaotically. Both play the
full 10.5k PTs. `inputs` differing at PTs ~476–867 is the harness's civ-timer preemption on wall time, not the game.
With float noise this early, a tick oracle only validates the first few thousand PTs. Beyond that, the evidence is
"plays to the end without faults" plus behavioural checks.

## Phase 2: Dinkum 2.33 → MSVC STL (`--stl msvc`)

Setup: VS 2022 Build Tools with the C++ workload (`winget install Microsoft.VisualStudio.2022.BuildTools` with
`--add Microsoft.VisualStudio.Workload.VCTools`, the x86/x64 tools and a Windows 11 SDK). `clang_build.py --stl msvc`
and `clang_link.py --stl msvc` find them automatically (env `EE_MSVC_DIR`, `EE_WINSDK_DIR`). Outputs:
`build/clang-msvc`, `build/clang-test-msvc`.

- **Compile flags**: the STL refuses an old `_MSC_VER`, so `-fms-compatibility-version=19.44`, `/std:c++17` and
  native `wchar_t` (`/Zc:wchar_t`). The VC6 leniency the code needed under 12.00 was already fixed in phase 1;
  nothing new appeared.
- **Census** (845 units): 753 compiled at first, 817 once the SDK finished installing, 845 after these fixes:
  - `std::random_shuffle` (removed in C++17): `EEAIRandomShuffle` now spells out Dinkum 2.33's
    `random_shuffle(first, last, gen)` loop (`for d = 2..: iter_swap(x, first + gen(d))`). The AI keeps its exact
    draws and swaps on any library, so behaviour stays comparable with the Dinkum build.
  - Headers Dinkum pulled in transitively (`<string>` in EEWonNet.h); `std::binary_function` (removed) as a base.
  - Iterators initialised from `0`/`NULL` (Dinkum's were pointer-like): value-initialise them. `iterator(&v[i])`
    becomes `v.begin() + i`.
  - `unsigned short` used as `wchar_t` (`gEmptyWideString`): declare it `wchar_t`, which is the same type under VC6.
  - Dinkum-only library hooks (`std::gPointerToAssertFunction`): `#ifndef _MSVC_STL_VERSION`.
  - The ~41 lines of code that touched Dinkum internals by name were nearly all in comments (matching notes).
- **Link**: the vintage Windows import libraries map to the SDK's. VC6's MSVCRT.LIB becomes `msvcrt vcruntime
  ucrt msvcprt oldnames kernel32` (the modern startup code imports kernel32, which mixer and Default never linked
  before). Every `sxlrt233` library goes. Response files (`exe_fulllink`) are rewritten the same way.
- **Exports**: `UIListBox<UWideString>::Begin/End` return Dinkum's `_Ptrit`, so their names change with the
  library. Under the MSVC STL nothing old imports them.
- **Running needs harness work.** All modules switch runtime and STL together, so the retail renderer, the
  harness-built silent mixer (VC6, linked against sxlrt233) and the binary-patched retail renderer cannot share heap
  memory or STL objects with the new modules. Also, every class holding a container changes layout: a Dinkum
  vector is 16 bytes (allocator byte, first, last, end), MSVC's 12. The harness reads memory at VC6 offsets: the
  presentation bridge (~15: the UI manager's form vector at +0x30/+0x34, form fields, renderer state, the retail
  renderer's ShowScene entry), the telemetry, the per-PT hasher (`layout.py`) and the drivers.

## The fork's harness (tools/harness)

- The harness was ported, not adapted in place: the copy of ee's `tools/boot` runs the fork's own test builds.
  Every offset comes from the build (`layout_gen.py`: clang's `-fdump-record-layouts-complete` of a few units,
  parsed into `{class: {member.path: offset}}`). ee's VC6 numbers are translated by member name (`layout.off`).
  Under the MSVC STL 1,926 of 3,306 documented members moved.
- **The exe's member aliases had the same problem.** Data the original placed inside other objects
  (`link/exe/fulllink_members.tsv`: 75 globals, VC6 offsets) pointed at the wrong members once containers shrank:
  29 moved, and the first game start crashed on a zeroed `gGameOptions.mMapType` buffer. The exe link now
  translates each alias through the layouts. A Dinkum vector's `_First` (+4) becomes MSVC's `_Myfirst` (+0).
- **Functions the harness hooks must stay out of line**: a breakpoint at a function's entry misses calls that clang
  inlined into its own unit. The test profile adds `__declspec(noinline)` to them (`HARNESS_HOOKS`).
- **VC6-shaped checks in the harness that cannot hold**: instruction-pattern proofs (no_human_defeat), registers
  holding statics (`ReportStatistics`, found by map name instead), the retail renderer's RVAs, `msvcrt.dll`'s
  `abort` (the UCRT's is in `ucrtbase.dll`). Resolve everything by map name.
- **Matching stand-ins are UB.** `EE_LOST_DEBUG` (a member call through a null pointer that only charged VC6's inline
  budget) made clang compile `DX7Rasterizer::SetZBuffer` to a trap in the sanitizer build. Every `EE_LOST_*` and
  `LostDebug.h` were deleted.

## Removing VC6 and Dinkum

- **Freeze what VC6 knowledge is still needed before deleting it**: the VC6 layout (the reference for every VC6
  offset in the harness and the member table) is `tools/harness/vc6_layout.json.gz`. It came from the last Dinkum
  build's dump, after `--check` confirmed all 3,386 documented offsets.
- Tools: LINK/LIB/DUMPBIN become lld-link/llvm-lib and the COFF reader. VC6's RC becomes the Windows SDK's `rc.exe`.
  The vintage import libraries become the SDK's, with by-name libraries from `link/*/imports.txt` for the rest.
- **llvm-lib .def decoration differs from Microsoft LIB on x86**: a plain C name gets the underscore (`foo` ->
  `_foo`), a stdcall name does not (`foo@8` stays `foo@8`). Write stdcall names with the underscore (`_foo@8`);
  `fix_members` then sets the import-name type to UNDECORATE.
- **What the modern SDK dropped**: the DirectMusic GUIDs are gone from `dxguid.lib` (the Direct3D 7 ones are
  still there: a copy of them collided once the engine and renderer shared one link). Their values,
  read once from the DirectX 8.1 SDK's library, live in `src/ThirdParty/dxguid/*.c`. DirectInput 7's
  `DirectInputCreateA` comes from a by-name library; its data formats (`c_dfDIKeyboard`/`c_dfDIMouse`) come from
  the SDK's `dinput8.lib`. The DX7 SDK headers are still downloaded (`tools/fetch_dx7sdk.py`).
- Dinkum's `operator new[]`/`delete[]` members go (vcruntime has them), and so does the retail checksum compensation
  (the multiplayer version check only ever meets builds of this fork now).
- Result: 846 units, all 8 images, about two minutes, with the VC6 toolchain directories hidden. The harness tour,
  play and a 2-AI game to the time cap pass.

## Undefined behaviour the VC6 code relied on (sanitizer build)

- `build.py --sanitize-null` compiles without `-fno-delete-null-pointer-checks` and with
  `-fsanitize=null,pointer-overflow,nonnull-attribute,returns-nonnull-attribute`, the minimal runtime and recovery;
  `tools/harness/ubsan_minimal.c` logs `EE-UBSAN <check> <module>+<rva>` once per site. Run the harness on
  `build/clang-test-nullsan` and map each RVA through the module's map (nearest symbol below it).
- After the Dinkum swap, the tour, 60 s of play and a 2-AI Hard game reached only four sites:
  - `FSMemoryFile::IncreaseFileSize` rebased its pointers with `p += newBuffer - oldBuffer` (arithmetic across two
    allocations): rebase by offset, `p = newBuffer + (p - oldBuffer)`.
  - `EELoadScript` stepped a cursor back with `cursor += 5 - token.GetLength()`, an unsigned wrap used as a negative
    step (also a real x64 bug: `unsigned long` stays 32-bit, so the step would be +4 GB).
  - `EEIsLineToUnitBlocked`: `theList->Scale(...)` on a null `theList`, the crash that brought in the flag. `Scale`
    never reads `this`: it is a static member now, called as `EETagPointList::Scale` at all 29 sites.
  - Pattern: "a member function that ignores `this`, called through a pointer that may be null" is the VC6-era UB to
    look for first; `static` is the honest fix.

## 64-bit census (`clang_build.py --x64`)

- Compiles every unit for x86_64 with `-Wpointer-to-int-cast -Wvoid-pointer-to-int-cast -Wint-to-pointer-cast
  -Wint-to-void-pointer-cast -Wshorten-64-to-32` and writes each distinct site to `build/clang-x64/x64_census.tsv`.
  First run: 1,728 sites and 11 failed units; after the fixes below all 840 units compile, and the only pointer
  truncation left is Bink's own header macro (`BinkSoundUseDirectSound`). The 1,274 `-Wshorten-64-to-32` sites
  (`size()` into `long` and the like) are the next layer.
- **Callbacks**: declare them with the Windows types (`LRESULT CALLBACK f(HWND, UINT, WPARAM, LPARAM)`,
  `DWORD_PTR` timer user data, `LONG_PTR` enum lParams). On x86 these are the same types as the VC6 spellings, so
  32-bit code and names do not change. Watch for callbacks of the wrong convention cast into place: the resource
  enumerators were cdecl `bool` functions cast to `ENUMRESNAMEPROC` (Windows' thunk restored the stack).
- **Inline asm**: the naked `_ftol` is dead under clang (it never calls `_ftol`) and was removed; the `fld/fistp`
  helpers became `lrintf` (same round-to-nearest).
- **Saved-game pointer swizzling** (~100 sites): loaders read a 32-bit id into the pointer field and a fix-up pass
  (`RepairPointers`, `Restore`) casts it back and looks the object up. `USavedID(field)` (LowLevelEngineCore.h)
  takes the low 32 bits explicitly. The loaders themselves are x64-safe: they read typed 32-bit ids
  (`inFile.Read(theUnitID)`) and store them as pointers, element by element for vectors.
- **Raw on-disk records**: 38 record types are read or written as `sizeof(record)` bytes (database tables, UI
  tables, texture/TGA headers, scenario events). `tools/record_sizes.py` compares their x86 and x64 layouts from
  clang's record dumps: all equal (no pointers, `size_t` or vptrs inside; Windows `long` stays 32-bit). Each is now
  pinned by `static_assert(sizeof(T) == N)` after its definition, which also documents the formats for a later
  reader in another language. Network messages copy `long`/`float` members one by one, also fixed.
- **`-Wshorten-64-to-32`** (1,274 sites, 1,139 `size()` into `long`): benign, left alone (PORT.md).
- **Pointers kept in integers**: the physics-model table (`long gPhysicsModels[]`, passed through every unit-class
  ctor as `long`), the model manager's `map<UString, unsigned long>`, sector neighbour lists, the save/load events'
  form, a farm finder's return value: all retyped to the pointer they hold. Integers carried in `void*` message
  parameters narrow through `intptr_t`. Registry roots are `HKEY` (this renames nine engine exports; the export and
  import tables in `link/` follow).
- A count stored in the last slot of a `vector<GETMPolygon*>` (the terrain draw's visible lists) goes through two
  helpers in that unit.

## One static exe (folding the DLLs in)

- **Plug-ins**: the engine found plug-ins by loading every `*.dll` in a folder and asking `PIMGetName/PIMGetType/
  PIMIsValid`, then took their other entry points with `GetProcAddress`. Each plug-in now ends with a table of its
  former exports (`PIMPlugin`/`PIM_ENTRY`, generated from its `extern "C"` definitions; the counts matched the old
  export tables) and `PIMModule.cpp` walks the tables in the same first/next way. Only the `PIM*` triple collides
  between plug-ins (made `static`); the type-specific names (`NPIM*`, `SPIM*`, `EERMPIM*`, `RPIM*`) are unique.
- **Header views meet in one link.** The reconstruction gives each module its own view of shared headers
  (`LLE_EXE_HEADERS` etc.). In one link: a function inline in one view and out of line in another is a duplicate
  (non-COMDAT + COMDAT), and one declared but never defined in a view used to resolve against the engine DLL's
  export of the inline copy, so it became a trap stub. Both fixed by giving every view the same definition. Silent
  divergences (two COMDAT bodies, the linker keeps one) are listed by `tools/odr_check.py`; `tools/layout_views.py`
  compares record sizes between two units' views (the 2,561 shared by an engine and a game unit agree).
- **libjpeg twice**: the game's image libraries and the engine both compiled it; the engine's full copy serves both.
- **`// UNIT-FLAGS:` lines** replaced the module flags, so five units kept VC6's `-MD` and lost `-DEE_STATIC`
  (`/failifmismatch: RuntimeLibrary`). Now only their defines are added to the module flags.
- **By-name import libraries need the decorated name** (`_DirectInputCreateA@16`), which comes from the objects that
  call it: feed every module's objects, not just the game's.
- **Language.dll** is resources only: its `.res` is linked into the exe beside the exe's own (no ID overlap: the
  exe has an icon, a cursor and a bitmap, the language file dialogs and string tables); `UResourceFile` reads the
  game's own module. The multiplayer version checksum, which summed the engine/Dinkum/TCP DLL files, sums the exe.
- **clang-cl vector deleting destructors (the hard one).** After the merge the tour crashed with heap corruption
  (`c0000374`) when a form closed. Full page heap (Image File Execution Options `GlobalFlag 0x02000000`,
  `PageHeapFlags 0x3`, then `0x13` backwards) turned it into a verifier stop: a freed pointer that was not the start
  of a block. The DLL build of the previous commit passed the same tour under page heap, which proved the merge
  caused it. Cause: with the MSVC ABI, `delete[]` of a class with a virtual destructor calls the vtable's vector
  deleting destructor (`??_E`, flag 3). clang-cl emits a real `??_E` only for dllexport classes or in a unit that
  holds both the vtable and a `new[]`/`delete[]`; elsewhere `??_E` is a weak alias of the scalar `??_G`, which
  frees the array pointer past its cookie. The DLL build's engine classes were `dllexport`. Fix: the engine and
  renderer units keep `dllexport` (the exe gets an export table; nothing loads dynamically). 13 `delete[]` sites
  are affected (`push 3; call [vtable]`); removing them, or a clang that always emits `??_E`, lets the exports go.
- **An original bug the merge surfaced while looking**: `UWideString::Reallocate` dropped its fresh buffer and
  pointed the string at a `""` literal (then `Import` wrote into it and the destructor freed it). Fixed.
- **Harness for one image**: hooks that waited for a module to load (engine exports, the renderer's checkpoints,
  `abort` in the runtime DLL) are exe RVAs from the map, armed at process creation (`exe_boot.c --static-exe`).
  The presentation bridge was loaded by the plug-in scan (it is a DLL in the game folder): the test profile's
  `WinMain` loads it now, and it calls engine members at map addresses (`bridge_syms.h`). The silent mixer is
  compiled into the test exe in place of the Miles unit (`TEST_SWAPS`). The time shim's single `Sleep` slot is
  time-scaled except for the render loop. The T&L renderer needed its own software-device rules in the test
  profile (it insists on `D3DDEVCAPS_HWTRANSFORMANDLIGHT` and the T&L HAL device).
- Static initialisation order: the engine's and plug-ins' objects are linked before the game's, so their global
  constructors run first, as when the engine DLL loaded before the exe initialised (not the cause of the crash
  above, but the order the code was written for).

## Miles -> miniaudio (the mixer)

- The mixer plug-in's contract is small (33 `SPIM*` entry points, result codes, an `SESample` record the engine owns):
  channels with a bound sample, volume/pan/pitch/loop/position, whole-file loads, master volume. The new mixer keeps
  every entry point and mixes in software on miniaudio's device thread under one lock; samples decode whole at load
  into float PCM, shared by reference count between a sample and its duplicates, as the Miles `SampleData` was.
- **miniaudio's backend list only falls back during context creation**: WASAPI's context opens on a machine without
  an audio device and then the device fails. Try each backend (WASAPI, DirectSound, waveOut, null) through context
  and device until one starts. The null backend lets the game run on the headless VM, which retired the test-only
  silent mixer: the harness now exercises the real mixer (101 sounds decoded, 0 failed in a tour).
- **`MA_NO_RUNTIME_LINKING` breaks the context init** (MA_FAILED_TO_INIT_BACKEND even for the null backend, from a
  standalone probe), so miniaudio loads its system DLLs at run time as it does by default.
- **64-bit atomics**: `/arch:IA32` makes clang assume a CPU without `cmpxchg8b`, so miniaudio's 64-bit atomics became
  calls to a missing `__atomic_compare_exchange_8`. The build adds `-Xclang -target-feature -Xclang +cx8` (every
  Pentium has it); x87 stays.
- The game insists on a mixer ("requires a sound card"): any mixer activation failure ends the game at startup.

## DirectMusic -> dmusic (the music)

- The game's music manager uses little of DirectMusic: load a segment by file name, play, stop, an automatic
  transition (groove on a measure), the master groove level as "intensity", a port volume in hundredths of a dB
  (an amplitude of volume^(ln 10 / 2)), reverb parameters and a large repeat count to loop. It registers no
  notification callback and plays no motif. Read the callers before porting a whole COM surface.
- dmusic's public API lacked two of those (repeat count, master groove level); small additions to the vendored copy
  are cheaper than working around them, and its README lists them.
- The performance renders PCM on request: the mixer's device thread pulls it after mixing the effects (the mixer's
  master volume applies to effects only, as Miles' did; the music has the conductor's volume). The conductor clears
  the mixer's music source before releasing the performance, so the device thread never renders a freed one, and the
  render callback takes no conductor lock (no lock-order cycle with the mixer's).
- Third-party sources now compile in place (no staging), and a library can list its own include directories
  (`src/ThirdParty/<lib>/include_dirs.txt`, passed through `INCLUDE`, which copes with spaces in paths).
- The harness used to switch music off (DirectMusic needed a real device); with music on the null device it now
  plays in every session (Honor.sgt with its style and DLS bank, peaks 0.14-0.26).

## Bink -> pl_mpeg (the movies)

- Bink has no permissive decoder; converting the movies once (ffmpeg, outside the game) to MPEG-1/MP2 keeps every
  line of decoder code in the binary MIT. MPEG-1 allows only standard frame rates (23.976..60): Bink's usual 15 fps
  must be resampled (`-r 30`), or the encoder refuses to open.
- The game finds movies through its data path by their `.bik` names before it creates a player, so the converted
  file lives next to the original rather than replacing it.
- pl_mpeg's `plm_decode(elapsed)` with video and audio callbacks does the A/V pacing; the sound waits in a small ring
  that the mixer's device thread pulls with linear resampling (44.1 kHz movie audio into a 48 kHz device). The
  player clears the mixer's movie source before it frees anything.
- pl_mpeg.h names `FILE` before including `<stdio.h>`: include it first.
- With movies on pl_mpeg nothing uses DirectSound, and the exe's imports are Windows system DLLs only.

## DirectX -> wgpu: phase 0

- wgpu-native's release packages carry a static `wgpu_native.lib` (no default-library directives) for
  windows-x86_64-msvc and windows-i686-msvc; it links with clang-cl/lld-link and the static CRT (/MT) as is, plus
  the system libraries the Rust standard library and the backends need (d3d12, dxgi, d3dcompiler, userenv, ntdll,
  bcrypt, ws2_32, ole32/oleaut32, propsys, runtimeobject, synchronization, opengl32, ...).
- On a machine without a GPU the DX12 backend runs on WARP (adapter type CPU) without being asked; that keeps the
  headless harness viable for the new renderer.
- webgpu.h v29 style: callback-info structs (mode AllowProcessEvents + wgpuInstanceProcessEvents), WGPUStringView
  strings (length WGPU_STRLEN = NUL-terminated), chained structs for the HWND surface and the native instance
  extras (backends).

## DirectX -> wgpu: GX (phases 1-3)

- **Port the API, not the renderer.** The DX7 renderer uses about 60 DirectDraw 7/Direct3D 7 calls and a fixed
  vertex format. Reimplementing that subset (GX: same method names, the DirectX 7 SDK's constant values, GX prefix)
  and renaming the renderer's types turned a 6,600-line rewrite into a mechanical port (`gxport.py`-style regex
  renames) plus ~150 lines of hand edits. The renderer's logic, which matched the original, is untouched; a later
  Rust renderer can still redesign from the GX contract.
- **Surface model.** DirectDraw lets the CPU lock any surface; wgpu textures are GPU-only. Each GXSurface keeps both
  copies with valid/stale flags: Unlock/ReleaseDC make the GPU copy stale (upload before the next GPU use), GPU
  draws and blits make the CPU copy stale (read-back on the next Lock). Blits whose source and target are both
  current on the CPU (UI composition, the minimap, cursor building) run on the CPU with exact DirectDraw semantics
  (nearest stretch, colour key compared in the source format), so they never cost a read-back. An upload of a
  texture the open command encoder already used submits first, so earlier draws keep the old pixels.
- **GDI text on a GPU target without a read-back**: draw the text twice into scratch DIBs, once on black and once on
  white. Over black the pixel is colour x coverage; white - black = 1 - coverage. That gives premultiplied colour
  and alpha for any GDI antialiasing, composited with one blend draw. (GetDC on CPU surfaces moves the surface
  memory into a DIB section once, so GDI draws in place, exactly as on DirectDraw.)
- **Direct3D 7 pixel centres** are at integer coordinates, WebGPU's at +0.5: shift clip x by +1/width and y by
  -1/height (times w) in the vertex shader, or every 2D quad is half a pixel off and blurs.
- **Vertex colours**: the D3D ARGB dword is B,G,R,A in memory; wgpu-native v29's C API rejects
  `WGPUVertexFormat_Unorm8x4BGRA` (panic "invalid vertex format"), so read Unorm8x4 and swizzle `.zyxw` in WGSL.
- **wgpu-native i686**: from v25 (v29.0.0.0 and v29.0.1.1 checked) every DX12 render pipeline fails on i686 with
  "Shader translation error ... The parameter is incorrect (0x80070057)"; x86_64 is fine and v24.0.3.1 i686 works
  (same GX code: the C API differences between v24 and v29 did not touch what GX uses). The i686 pin goes away with
  the 64-bit exe.
- **wgpu errors are Rust panics**: an invalid object used in a submit aborts the process (c0000409, fast fail) with
  the message on stderr, which a GUI exe does not have. `EE_GX_STDERR=<file>` redirects stderr there;
  `EE_GX_LOG=warn|info|debug` routes wgpu's log to OutputDebugString (the harness logs it).
- **The screen's current mode must be enumerated**: DX7Screen looks its mode up in the enumerated list and never
  initialises the index otherwise (GetDesktopRectangle then reads garbage). GX always lists the desktop size.
- **Harness**: the DX12 runtime writes its shader cache under %LOCALAPPDATA%\D3DSCache; exe_boot.c refuses that
  open with ACCESS_DENIED (D3D12 works without the cache) instead of allowing a persistent write. The windowed
  capture checkpoint is the game window's creation (GX never takes the display), and the presentation bridge gets
  each frame from GX's test hook (GXTestSetPresentHook/GXTestReadFrame, found by map address) instead of patching
  ShowScene and locking DirectDraw surfaces. `--no-render` (accelerated AI games) used to null Direct3D's vtable
  slots from the time shim; it is now GX's own test switch, `EE_GX_NULL3D=<key ms>` (device draws and clears skipped
  except one scene per key interval), set by timeshim.py in the environment the game inherits.

## For WONAPI/WONLobby later

The WON library is carved or stubbed (`won` class in the stub report) in both builds. When its source arrives, expect
the same sequence. Apply the dialect passes and header bridges first, then check the exports/imports both ways
(WON is linked into the exe, not a DLL, so the ODR/inline section matters most). Then make sure the `won` count in
the stub report reaches 0 with no new traps.

## Cross-platform: the platform layer (branch xplat)

- **Census first** (`tools/winapi_census.py`): a name list per family plus regexes (`WM_*`, `Reg*`, `_mbs*`, ...)
  over the comment- and string-stripped source. Two traps made the first run lie: game constants that share a
  Windows prefix (`IDS_*` string ids: 3,765 false windowing hits) and the game's own members spelled like Win32
  functions (`TSScheduler::CreateEvent`, `GEWindow`'s `GetWindow`, `UIControl::DrawTextA`). Names used after `.`,
  `->` or `Class::` in a file are members there; such a name counts as Win32 only when written `::Name`.
- **windows.h renames members.** `CreateEvent`, `DrawText`, `CreateDirectory` are A/W macros, so the methods of those
  names are really `CreateEventA`/`DrawTextA`/`CreateDirectoryA` (the reconstruction even spells `DrawTextA`, from
  the mangled names). Removing `<windows.h>` from a unit silently renames them there: rename them in one batch
  before making headers windows-free (census family win-macro-names).
- **The harness constrains the wrappers.** Its time shim patches the exe's IAT (Sleep, QueryPerformanceCounter,
  WaitForSingleObject/WaitForMultipleObjects, timeGetTime/timeSetEvent/timeKillEvent), so the Win32 implementation
  must keep calling exactly those imports (a PlatEventWait is WaitForSingleObject, not WaitForMultipleObjects with
  one handle). It also caps the frame rate at the Sleep import by the caller's return address inside
  RTSGraphics::Main: a PlatSleep wrapper would hide the caller, so those Sleeps stay direct Win32 calls (commented).
  The harness also calls engine exports by mangled name (`?Enter@TSCriticalSection@@QAEXXZ`, the read/write lock):
  keep those signatures.
- **Opaque handles that are the Win32 handle.** On Win32 a PlatEvent*, PlatThread*, PlatFile*, PlatFind* is the
  HANDLE itself (no allocation, no behaviour change); TMTimer's event timers rely on PlatEvent* being the event
  handle (TIME_CALLBACK_EVENT_SET). Only PlatMutex is a heap object (a CRITICAL_SECTION has no stable size across
  systems). Class layouts kept their offsets where they held handles (FSFileSpec +0x40, FSArchive +0x64, TSThread
  +0x30); TSCriticalSection shrank (it is only ever heap-allocated by TSScheduler).
- **Keep the last error where the engine records it.** URegistry, FSFileSpec and friends call
  `ESRecordNativeError(GetLastError())` after a failure, sometimes after closing other handles. The platform
  functions capture the error at the failing call and restore it before returning (PlatFindClose, the preferences
  store), so `PlatGetLastError()` gives the value the old code recorded.
- **Wait results**: `WaitForMultipleObjects` switches (`case WAIT_OBJECT_0 + 3:`) became `TSEvent::WaitAny`
  returning the index or kTSWaitTimeout/kTSWaitFailed; INFINITE is kTSWaitInfinite (same value).
- **Multimedia timer callbacks** carry one user word: callback timers keep (callback, user) in a table keyed by the
  timer id, which the thunk looks up by the id it is passed (never through a pointer a concurrent Stop may free).
- **COM per thread** (CoInitialize in TSThread's entry) moved into the Win32 thread procedure: it is Windows' need,
  not the engine's. The unused `_beginthreadex` twin (POSIXThreadMain) went.
- **LP64 is the next format trap.** Windows `long` is 32-bit on x64 too, which is why the x64 census found the
  record formats safe. On Linux/macOS `long` is 64-bit: FSFile::Read(long&)/Write(const long&), message members and
  the static_asserted records with `long` change size. The preferences store already takes int32_t.
- **Multibyte text**: the game's strings are MBCS in the ANSI code page (`_mbs*`, double-byte on CJK Windows). The
  platform functions keep that meaning; a non-Windows implementation has to pick an encoding together with the
  fonts and IME code, so PlatString.h documents it instead of pretending to be portable.
- Process: a Git Bash heredoc lost backslashes in Python string literals here (a doubled backslash arrived single);
  write edit scripts with the editor tool. Edits that touch included headers must wait until a running compile has
  finished: units are restaged and `include/` is read live while it runs.
