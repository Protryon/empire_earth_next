# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later, the WON libraries first
(WONAPI/WONLobby: stand-ins in source, see "Plain link").

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
| `tools/build.py` (`build.py`) | the build: compiles everything up front, then links the one exe with one plain lld-link (`exe/link.rsp`, `exe/link.log`). `--test-profile windowed-software`: harness images; `--sanitize-null`: UBSan builds. |
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

## Linking the exe (exe_fulllink with lld; replaced by the plain link, see "Plain link")

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

## x64 link (`build.py --x64`)

- **One switch, two targets.** `ee.set_arch("x64")` selects the x64 runtime/SDK library directories
  (`lib_paths`), wgpu-native x86_64 (`ee.wgpu_dir`) and `llvm-lib /machine:x64` for the generated import
  libraries; `clang_build.set_target(x64=True)` the compile target (no `/arch:IA32`: SSE2). The census
  (`clang_build.py --x64`) is the same target plus the pointer-size warnings. The record layouts the member aliases
  are translated with (`layout_gen`, which reads `clang_build.CLANG_FLAGS`) are the x64 build's own. Default out
  `build/clang-x64-exe`; all 858 units compile, 15.6 MB exe (i686: 11.7 MB).
- **The link tables are 32-bit names.** `names.txt`, `vtable_slots.tsv`, `import_thunks.tsv` and
  `fulllink_members.tsv` hold VC6-mangled names. After pass 1, `exe_fulllink.translate_x64` maps each to the x64
  name of the same entity by `UnDecorateSymbolName(name, UNDNAME_NO_MS_KEYWORDS)` (no calling convention, no
  `__ptr64`; one key per entity on both targets), then again with `unsigned __int64`/`__int64` folded to
  `unsigned int`/`int` (size_t, ptrdiff_t), a key counting only when it is unique on both sides. C names lose their
  x86 decoration (`_name`, `_name@n`, `__imp__name@n`; `__purecall` is `_purecall`). Result: 442 names translated,
  1,337 spelled the same (no pointer in them), 65 without a counterpart (names the i686 stub planning looks up
  that this build does not reference).
- **What broke on the way, all from `__ptr64`**: a demangled x64 member function ends in `__ptr64` (the `this`
  qualifier), so `implib.is_cpp_data` took every undefined method for data (weak-external "data aliases" instead
  of jumps) and `signature` found no parameter list (103 WON functions became traps). Strip it before matching
  the text.
- **Census**: pass 1 classes identical to i686, symbol by symbol (won 304, icf 36, vslot 262, pure 11, data 75),
  stubs identical (won 278 graceful + 13 traps, aliases icf 36 / vslot 262 / pure 11, member aliases 75, data zero
  5 + bss 8), pass 2 0 unresolved / 0 duplicates. A WON stub's graceful-or-trap decision and return kind are read
  from the x86 name's signature, so the two targets cannot drift.
- **x64 stubs** (`Stubs64`): one calling convention, the caller pops, so a graceful stub only zeroes rax/rdx/xmm0
  (a constructor returns rcx with its vtable pointer cleared); traps are `lea rcx,[msg]; mov edx,id; jmp
  $ee_trap` (OutputDebugString, int3 under a debugger, ExitProcess(id)). Relocations: REL32 for calls and
  rip-relative data, REL32_1 for `cmp/mov byte [rip+x], imm8`, ADDR64 for vtable slots. Vtable slot inference
  reads 8-byte ADDR64 slots; data stubs get twice the original's size (its layout had 4-byte pointers).
- **Image base 0x400000.** Member aliases are absolute symbols (container + offset); a COFF symbol value is 32
  bits, so the x64 exe links `/FIXED /BASE:0x400000 /HIGHENTROPYVA:NO`. rip-relative references to absolute
  symbols work there, and the map keeps 8-digit addresses (`link6_map`). This goes when the member aliases become
  real members in source.
- **Static checks**: PE32+ AMD64; imports only system DLLs (ADVAPI32, DINPUT, GDI32, IMM32, IPHLPAPI, KERNEL32,
  SHELL32, USER32, WINMM, WS2_32, WSOCK32, ntdll, ole32, oleaut32, opengl32, setupapi, bcryptprimitives, an
  api-ms-win synch set; every imported name exists in the System32 DLL's exports); i686 imports D3DCOMPILER_47
  where x64 imports setupapi (wgpu-native v24 vs v29). 4,745 exports (i686 4,744). `record_sizes.py` 38/38 equal,
  `layout_views.py --x64` UIManager/EEWorld 2,680 shared records and DX7Rasterizer/Empire Earth 2,607 agree,
  `odr_check.py` 81 divergent inline functions (i686 80: the same set plus a few MSVC STL `basic_string`
  internals).
- **Runtime hazards a link cannot show**, found with clang-query over every unit (int-to-pointer casts from 32-bit
  integers, explicit pointer-to-integer narrowing; 209 sites): most are the saved-game id swizzling and integers
  posted through void* thread-message parameters, both fine. Fixed:
  - `_controlfp(x, _MCW_PC)` is an invalid parameter on x64 (the CRT ends the process): four threads set 53-bit
    precision at start. Off the x87 it is a no-op and the get reports `_PC_53` (UMath).
  - `EEUserInterface::HandleMessage(msg, long, long)` received the UI thread's void* parameters, pointers for most
    messages, narrowed to long: `LONG_PTR` (long on x86, so the same code and name there).
  - Saved ids are read as `unsigned long` into pointer fields and compared with `(T*)-1` (none) after loading:
    0xffffffff zero-extends to 0x00000000ffffffff on x64 and the check fails (EESelectedGroup then indexed
    `mGroups[-1]`). `USavedPointer<T>(id)` sign-extends as x86 did, at the fields compared with -1.
- Checked and fine: no Get/SetWindowLong(Ptr), no DLGPROC, window/hook procs use the Windows types, timer user
  data is DWORD_PTR, Interlocked only on int32_t counters, no va_arg, no unions of pointers and integers, no
  pointer-sized arithmetic on struct offsets, `%x` only formats unit ids, memcpy sizes of 4 copy longs/floats.
- Left (not run yet, step 2/3 of the plan): whether DirectInput 7 (`DINPUT.dll`, present on x64 Windows) and the
  GX/wgpu renderer behave under the x64 game; the `-Wshorten-64-to-32` class (benign by the census); the 81 ODR
  divergences (as i686). The game has not been started as x64.

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
  (same GX code: the C API differences between v24 and v29 did not touch what GX uses).
- **wgpu-native v29 x86_64 in the game**: the GX self-test passes (read-backs between presents, on a second thread,
  under the harness debugger), but in the x64 game every buffer read-back of the 1024x768 frame stalls: a blocking
  `wgpuDevicePoll` never returns (the first hang: the frame dump after "Single Player"), a non-blocking poll never
  completes the map. The same game on v24.0.3.1 x86_64 has no stall, so both architectures pin v24.0.3.1. GX's
  read-back now polls without blocking and gives up after 10 s with a log line instead of hanging the graphics
  thread. Headers must match the linked library (verify.py takes them from the target's toolchain/wgpu/<arch>):
  v29 headers with the v24 library and vice versa crash at device creation.
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
- **Harness and the platform layer**: TSCriticalSection now holds a pointer to an opaque PlatMutex, so test code
  may no longer treat it as a CRITICAL_SECTION. The bridge's UI fixture calls the new
  `TSCriticalSection::TryEnter` instead (it silently never wrote ui-state.txt before, so accelerated AI games
  timed out at the main menu). tools/harness/human/human_input.c (not ported yet) still reads the raw layout and
  must switch to TryEnter/Enter/Leave when it is ported.

## Text db tables (tools/eedata and the text loader)

The 37 db tables the game reads convert to CSV/YAML (`tools/eedata/eedata.py`; formats in docs/DATA_FORMATS.md
section 8), and the game reads the text form when it is on the data path. Clean text only (owner, 2026-11-04): the
text carries what the game reads, nothing else.

- **Specs come from the sources, not from the survey's tables.** `specgen.py` parses the record structs, lays
  them out with MSVC x86 natural alignment and checks each size against the source's `static_assert` (one struct
  states its size only in a comment). The source names decide the class: `mUnused*` and gaps are not exported,
  `mLabel` is the text-only row label, everything else is a field the game reads and must have a real name
  (specgen refuses offset names). `overlay.py` holds only what the structs cannot: the containers, the defaults of
  unread fields, the hole values and the dbworld constants. `cppgen.py` turns the same `specs.json` into the C++
  header, and `eedata.py check` fails when either generated file is stale, so the Python and C++ sides cannot drift.
- **"Unused" is a compile census, not a grep.** Rename every member of a struct (`mX` -> `mX__P`), compile every
  unit syntax-only (`clang-cl /Zs`, `-ferror-limit=0`), and clang's "no member named 'mX' in 'Struct'" errors list
  every use site with its column (`eedata.py census`; the same positions drove the mechanical rename). Names like
  `mName`/`mID` are shared by dozens of classes, so text search cannot tell whose member a use is. A second census
  on the class members a loader copies into finds the dead copies (copied, never read): those fields are unused
  too. The census misses raw-offset reads and uninstantiated templates (-fdelayed-template-parsing); the records
  have neither (checked).
- **Hole rows are database defaults, not garbage.** The retail empty rows are mostly real values (-1 ids, 1.0
  opacities, mObsoleteEpoch 15) with 0xCC only in a few fields. The hole record keeps the readable values
  (`overlay.HOLES`, generated by `eedata.py holes`); check proves that the 0xCC fields of hole rows are in rows the
  loaders skip. A zero hole would have loaded as row id 0 in several tables.
- **Unread is not always zero-safe.** dbobjects' unread effect slots still make the unit class load the media of
  the effect row they name: zero would load effect row 0 for every unit, so their default is -1.
- **Floats.** Shortest decimal that rounds back to the same float32 bits, rounded *correctly* (Python: exact
  `Fraction` -> round half to even; C++: `std::from_chars`). Parsing through a double first can double-round, so
  never use `struct.pack('<f', float(s))` for text input. Out-of-range decimals are refused rather than turned
  into inf/0 (the two runtimes report them differently).
- **Strict scalar grammar on both sides.** Python's `str.isdigit()` accepts Unicode digits and `int()` accepts
  underscores; the C++ side does neither. Both use one ASCII grammar (regexes in scalars.py, hand-written matchers
  in EEDataText.cpp). The parity gate (117 edited texts, valid and broken) is what found these.
- **CP-1252 has five undefined bytes** (0x81 0x8d 0x8f 0x90 0x9d). They map to the C1 code points of the same
  value, so every byte string has a text form; the C++ table is generated from Python's decoder.
- **Exceptions.** The game units build without `/EH`. The text compiler is the one unit that throws (internally;
  `Compile` returns false with a message): `// UNIT-FLAGS: -EHsc`, which `verify.unit_flags` now passes through.
- **New units without an original address** go into `units.EXTRA_UNITS['exe']`; `build.py` links them (it used
  to link only the exe's units.txt units).
- **Building in a second worktree:** set `EE_TOOLCHAIN` to the main checkout's `toolchain/` and
  `EE_ORIG_SRC_ROOT` to a private staging root (the default `C:\EEX` is shared with the main checkout's builds).
- **The multiplayer checksum** summed only `Data\Db\*.dat`; it now adds the loose `*.csv`/`*.yaml` too (0 when
  there are none), since a text table replaces its .dat. Clean text never matches retail's checksum; peers need the
  same tables.
- Cost: compiling all 37 tables takes about 0.8 s at -O1 (dbobjects' 5.2 MB of YAML 0.33 s, dbgraphics 0.27 s). The
  modding plan's compiled cache would remove it.

## For WONAPI/WONLobby later

The WON library is stood in for by `src/Empire Earth/WONAPI/WONAPIStubs.cpp` ("Plain link"). When its source
arrives, apply the dialect passes and header bridges first; WON is linked into the exe, not a DLL, so the ODR/inline
section matters most. Then delete the stand-ins it defines (a duplicate symbol names each), with no new fatal ones.

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

## x64 harness (branch harness64)

- One debugger source, two builds: `exe_boot.exe` (mingw32) and `exe_boot64.exe` (mingw64). The harness must be
  native to its target (a 32-bit debugger cannot debug a 64-bit process; the x64 build stops at a machine mismatch),
  and being native it reads every Windows structure of the target (UNICODE_STRING, OBJECT_ATTRIBUTES, LARGE_STRING,
  PE headers, DDSURFACEDESC2) in its own layout: no per-arch struct offsets. What differs is only the calling
  convention at a guarded entry, isolated in an arch layer: `arg_n(c, n)` (x86 `[esp+4n]`; x64 RCX, RDX, R8, R9,
  then `[rsp+8n]` above the home area), `marg_n` (member functions: thiscall on x86, `this` = first argument on x64),
  `set_arg` (x64: a register, applied by the step-over's SetThreadContext) and `fake_return(c, nargs, status)` (x86
  stdcall/thiscall callees pop `4*nargs`, x64 callees pop nothing). Arguments that are ULONG are compared truncated:
  the upper half of an x64 register (or stack slot) is undefined.
- The self test is the gate (`tools/harness/selftest/selftest.py`): 40 cases, both harnesses, plus main's i686
  harness on the 36 cases it supports: identical decision logs after masking addresses, handles, thread ids and the
  system-DLL noise (WOW64 opens SysWOW64, native System32). Findings it made on the way (all three harnesses agree):
  handle-relative creates (`CreateFile("x")` from the stage as the current directory: `<rel>\x`) are not "below the
  stage" and stop; a console target stops at `\Device\ConDrv\Connect` (the game is GUI); `RegCreateKeyEx` of a key
  whose parent is missing walks up with MAXIMUM_ALLOWED and stops (the game's key exists); `accept` makes an AFD
  endpoint the loopback policy only allows with `--mp-loopback`; `socket()` reaches the guards twice (socket ->
  WSASocketW).
- Symbols: the harness keeps the i686 decorated names; `LinkMap` answers for them in an x64 map (`x64names.py`:
  `QAE` -> `QEAA`, `PAV` -> `PEAV`, pointer data `...@@EA`, C names without `_`/`@N`; else a unique match of the
  undecorated signature without `__ptr64`/conventions). All 46 names the session plumbing uses resolve mechanically
  in the x64 branch's map. The x64 exe is `/FIXED` at 0x400000, so its map has 8-digit addresses: an x64 map is told
  by its exe's PE header, not the address width.
- Addresses: everything the harness hands on is an RVA from the map's preferred base (bridge `ee_sym`, gUIManager,
  the time shim's frame-cap range); the absolute VAs exe_boot takes (`--rng-trace`, `--no-human-defeat`'s world)
  are moved by `--image-base`; game_state_probe relocates by the running exe's base and takes the pointer size from
  the process. Fixed today, but an ASLR x64 exe (the self test's are) works unchanged.
- Offsets: layout_gen dumps x86_64 layouts when clang_build targets x86_64 (or `--x64`); hbuild refuses a
  layout.json whose vector pointers are 4 apart for an x64 exe. Examples: EEMStartLoading seed +0x14 -> +0x24,
  EEPhysics::mRandom +0x18 -> +0x30, EEWorld::mPlayers +0x74 -> +0x88, sizeof(UWideString) 0x14 -> 0x20.
- The mutex bypass of a build by map guards the instruction right after the `call [GetLastError]` (eax is its result
  there whatever comes next); VC6's exact `push ...; call; test eax,eax; jnz` pattern also refused the i686 clang
  build (`xor eax,eax; push`).
- The time shim's Direct3D 7 null-device hooks are gone (GX: `EE_GX_NULL3D`); its x64 build has no stdcall
  decorations (`.def` names, entry `DllMain`) and needs `-fno-tree-loop-distribute-patterns` (gcc turned a copy loop
  into a memcpy the `-nostdlib` DLL cannot link).
- Not on x64: `differential/` (ptsync.h/ptwatch.h read VC6 layouts with 32-bit pointers; the x64 harness refuses
  their options), the rng trace's caller-frame fields (no frame-pointer chain: logged as 0).

## x64 runtime (branch x64run)

- **Symptom of the first bug**: after "Single Player", `EEWorld::CreatePlayers` crashed on
  `theUpgrade->GetUnitClass()` returning 0x00000000baadf00d (the heap's uninitialised fill, read as 32 bits).
  `EEUnitClassUpgrade::GetUnitClass` has no body in source: the exe link aliases it to the function the original's
  ICF folded it with, `EEComplexDoppelganger::GetTileX` (`mov eax,[ecx+0x60]` in VC6). On x64 that twin reads its
  own `mTileX` (a 32-bit load at +0x74), not `mUnitClass` (+0x78). On i686 both members still sat at +0x60.
- **Bug class: ICF aliases across classes.** Every referenced but unwritten function the link resolves to its VC6
  ICF twin (`icf`, `vslot`, `mismatch` in link_report.json) is only right while both classes keep the twin's
  members at the same offsets, and while the arguments travel the same way: on x64 a float argument is in XMM1
  where a pointer twin reads RDX (`EESoundObject::SetVolume` ran `EEContagionHelper::SetFunctor`). Fixed by
  writing the bodies in source (`// ICF-FOLDED:` next to the class's members): GetUnitClass, EEUGHealUnit::GetTarget,
  EESoundObject::SetVolume, EECalamity::StopSecondarySound (every calamity's end ran EEE2DFire::StopSound on the
  calamity), EESELoadThread::Post407, EESETSEffect::Hide, EESETriggerScreen::ShowText,
  EEUIMiniMap::MouseLButtonDown. `tools/alias_layouts.py` finds them: it reads each alias target's body from the
  linked exe (trivial `ret`/constant bodies are fine), dumps both classes' layouts for x86 and x64 and reports
  aliases whose classes have a common x86 offset holding members at different x64 offsets (now 0; 16 free-function,
  same-class or layout-less aliases are listed for review: they use only base members or arguments). The
  remaining 31 icf and 259 vslot aliases pass. Run it after any upstream merge that changes the link tables.
- **The member aliases** (`fulllink_members.tsv`, data the original placed inside other objects: 75) were checked
  by name: each VC6 offset resolves through the VC6 layout reference to the intended member (gWorld+0xcc
  `mNumPlayers` -> +0x12c, gUnitPicker+0x29c `mLock` -> +0x2a8, ...); tables of pointers double their offsets.
- **Varargs**: the engine's own formatters (ESDebugOut, ESReportMessage, sprintf into UString/UWideString, GXLog)
  had no format attribute, so nothing checked them. With `PLAT_PRINTF_FORMAT` a `-Wformat` census over every unit
  for x64 gives 434 sites: 396 `%d` of long/unsigned long (same size on Windows; they matter on LP64 Linux/macOS),
  12 `%d` of size_t (Win64 passes each vararg in an 8-byte slot; `%d` reads its low half, right for these counts),
  and three of the wrong class (a long for `%f`, a double for `%d`, a pointer for `%d`), fixed. All in debug traces
  and dumps. `sscanf("%d", long*)` in the random map parser is right on Windows only (step 4).
- **What did not break**: saved-id sentinels, thread-message pointers and x87 precision control were fixed in the
  link step ("x64 link"). The tour, play and AI games found no other x64 fault: no traps, the same native-error
  set as i686, wgpu-native v24.0.3.1 renders and reads back on x64.
- **Harness, not x64**: `--play-seconds 120` reaches the civilization timer (240-300 game s), whose form 7 pauses
  the game and blocked the final capture on i686 and x64 alike; the tour's play loop now cancels it as the AI game
  does.
- **Gates (x64 test profile)**: tour + 120 s play pass; seeded 2-AI Hard game to the time cap with the binary and
  the text db tables (PT 13,882 / 13,633, units 0/56/39 and 0/57/38: the same game up to the wall-time-dependent
  civilization preempt). i686 after the same changes: tour + 60 s and + 120 s pass, the AI game reaches the cap
  (PT 13,214, units 5/16/36).
- **32 vs 64 bit**: random map generation (the Default generator's land/water/tree reports) is identical on i686
  and x64 for the pinned seed. The AI trace lines (epoch timing, research timing, deaths) are identical between
  x64 runs (binary and text tables too), identical between i686 runs, and identical between x64 and an i686 build
  with `/arch:SSE2` until about game second 290 (the first unit deaths), while the x87 build leaves both at about
  256 s, its float formatting already differing in the last digit at the start. The shipped i686 build (x87) plays
  seed 2 as a quiet game (human keeps 5 units, AIs 14 and 35 units at the cap); x64 and i686 SSE2 both wipe the
  idle human by ~600 s and end with 37-59 units per AI. The split is the floating-point model, not pointer size:
  an SSE2 i686 build lands with x64. Locating what remains between x64 and i686 SSE2 (candidates: the x86 and x64
  UCRT math functions, anything ordered by pointer value) needs the per-PT differential tooling, not ported to x64.

## x64 only (branch plainlink)

- **One target.** `ee.ARCH` is fixed to x64 (the MSVC/SDK library directory name), `clang_build.CLANG_FLAGS` targets
  x86_64 (no `/arch:IA32`, no `+cx8`: x86-64 has cmpxchg8b), `build.py` writes build/clang and build/clang-test
  (`--x64` is accepted and ignored, as in `make_play_dir.py`, `layout_gen.py`, `layout_views.py`, `game_session.py`).
  The census is `clang_build.py --census` (old spelling `--x64`), out build/clang-census. `fetch_wgpu.py` fetches only
  the x86_64 package; wgpu_probe and gx_test build x86_64 only.
- **Checks that compared the two targets** were made target-independent: `record_sizes.py` now checks every raw
  record (Read/Write with `sizeof`) against its `static_assert(sizeof(T) == N)` pin, the on-disk size, instead of
  x86 == x64; two records had no pin (GETGAHeader 18, UIDbControl 860) and got one. That is also the check LP64 needs
  (a `long` member changes the size there). `layout_views.py` compares two units' views on the one target.
  `alias_layouts.py` (i686 vs x64 layouts of ICF alias pairs) went with its subject (see "Plain link").
- **Harness**: one `exe_boot.exe` (mingw64). Resolving the `_WIN64` conditionals mechanically (keep the x64 side,
  nesting-aware) left the arch layer as plain functions over RCX/RDX/R8/R9 and the stack; an `#error` refuses a
  32-bit compile. The symbol names the harness looks up were i686 decorations translated for an x64 map at run time
  (x64names.py): they were rewritten once to the x64 names the x64 map holds (46 literals; C names lose their `_`:
  `GXTestReadFrame`, `abort`, `__imp_GetLastError`), and LinkMap no longer translates. A stale literal fails loudly
  (`... is not in EE-AOC.map`). The unported tools (human/, differential/, save_load, mp_game, play) still carry
  i686 names and VC6 addresses: they need porting anyway.
- **Self test**: without a second harness the decision logs are checked only against each case's expectations (they
  were also compared x86 against x64 and against main's i686 harness); all 40 cases and the four in-process parts pass.
- Gates after the switch (x64 test profile): tour + 120 s pass; the seeded 2-AI Hard game reaches the time cap
  (binary PT 14,040, units 0/56/39; text db PT 14,149, 0/56/40). The same game before the switch: PT 15,199 / 15,059,
  0/58/40 both. The end PT and the last unit or two move with the wall-time-dependent civilization preempt (form 7
  opened at PT 4,970 / 5,684 in the baseline runs), as between the earlier x64 runs.

## Plain link (branch plainlink)

The exe link used to be `exe_fulllink.py`: pass 1 linked, every unresolved symbol was classified by what the
original image had at its VC6 address (`link/exe/names.txt`, `vtable_slots.tsv`, `import_thunks.tsv`,
`fulllink_members.tsv`, VC6-mangled and translated to x64 names), a generated `stubs.obj` answered them (WON stubs
and traps, jumps to ICF twins, absolute member-alias symbols), pass 2 linked with it and pass 3 placed the member
aliases. Census at the start: won 304 (278 graceful + 13 traps + 13 data), icf 31, vslot 259, pure 11, data 75.
ELF and Mach-O have neither MSVC names nor COFF absolute symbols, so all of it became source, and the exe is one
lld-link of every object (`build.py link_exe`; `exe/link.rsp`, `exe/link.log`).

- **Member aliases (75)**: globals the original addressed inside other objects. Each is a reference to its member,
  defined after the container (`long& gNumPlayers = gWorld.mNumPlayers;`, 66 in Empire Earth.cpp), and every
  `extern` says `T&` (117 declarations; a declaration left without `&` would not link: `?g@@3JA` vs `?g@@3AEAJEA`).
  The member names came from the VC6 layout reference, but its dotted paths nest siblings under the first record
  member (`mRegistry.mSettings.X` is `mSettings.X`): write paths from the class declarations, not the dump.
  Sign-mismatched members (18: `unsigned long` global over a `long` member and back) go through `reinterpret_cast`
  (corresponding signed/unsigned types may alias). Those in private members of engine classes (GEViewport: the
  render and main viewports' view rectangle, camera inverse, centre, y clipping plane; the camera inverse's row 1
  was read as a GEQuaternion "view plane") use the class's public accessors at their uses (23 lines); the picker lock got
  `EEUnitPicker::GetLock`. A reference whose initializer has a `reinterpret_cast` is dynamically initialised (not a
  constant expression); no static initializer reads these, so the order does not matter here.
- **A latent x64 bug the stand-in hid**: `gScenarioManager` was read through a local `struct EEU531864 { char
  _gap[0x238]; bool mB238; }`, i.e. byte +0x238 of an x64 EEScenarioManager, whose mIsEndingCinematic is at +0x300
  (the cinematic F3 test read a byte of mMediaStrings). Now `gWorld.GetScenarioManager()->mIsEndingCinematic`.
- **ICF / vtable-slot aliases (290)**: 264 `// ICF-FOLDED:` bodies in the alias class's unit, generated from the
  twin: a trivial twin body in the linked exe (`ret`, `xor eax,eax; ret`, `mov al,1; ret`, `mov eax,N; ret`) gives
  `{}` / `return 0 (false, NULL)` / `return true` / `return N` (152); otherwise the twin's source with the class
  renamed (111), only where the signatures agree. Checks that mattered: (1) every `this->m` member of a copy at the
  same x64 offset in both classes' dumps (45 copies read members; one did not: EEUGBuildRepair::GetGoalLocation's
  twin reads its `mTarget` point, the alias's `mTarget` is a unit pointer, its point is `mTargetLocation`, VC6 +0x20 in
  both); (2) qualified base calls: the twin's `EECPASGSpaceship::Load` / `EECPASGAir::Restore` are not bases of the
  alias, the VC6 bytes called one ICF body, so the alias calls its own base (EECPASGSea::Load) or the common one
  (EECPAttackSubGroup::Restore); (3) a twin in the same class (GetHeroCount = GetNumHeroes, ...) becomes a call of
  it; (4) the copied body needs the twin unit's includes, in their order (headers are not self-contained: EEMedia.h
  before EEGraphicEffect.h, EECPUnitSelector.h before EECPPersonnel.h). Signatures differing from the twin were
  wrong declarations: EEUCAirplane::FindUnloadTiles returns void like the other overrides, EECPASGSpaceship::
  IsAtLocation takes an EECPILocation, EEPCalamityHelper's slot +0x20 is a virtual operator= (the vtable entry
  pointed at it). The remaining aliases: 2 bodies already in EEGlobalCalamityClass.cpp, a unit with no code range
  of its own that no module compiled (now an extra unit); 23 in WON classes (below).
- **Pure (11)**: the slots that held `_purecall` are `= 0` (EEMedia, RTSPlayer, EEGroup::Validate,
  EEUIFSEPaint::CreatePaintAction); nothing instantiates the abstract classes.
- **WON (304 + 23 vtable slots)**: `src/Empire Earth/WONAPI/WONAPIStubs.cpp`, generated from the demangled names
  (dbghelp with UNDNAME_NO_MS_KEYWORDS | NO_ACCESS_SPECIFIERS | NO_MEMBER_TYPE gives a definition head nearly as
  is), then kept as source: a stand-in logs `EE-WON-STUB <signature>` once and returns 0/false/NULL or a static empty
  object for a reference; the 13 by-value class returns/arguments stop with `EE-WON-FATAL` and `abort()` (the
  harness reports `STOP abort`). Defining constructors emits the classes' vtables, which need every virtual: 48 more
  than the stubs had (iterate: link, take the undefined names, generate, relink). Two constructors the headers
  lacked (AsyncOpTracker, LobbyChangeEvent) were added.
- **Import libraries**: every import is a system DLL now: the Windows SDK's libraries, plus a one-line .def for
  DINPUT.dll's DirectInputCreateA (the SDK has dinput8.lib only). Keep the original tables' order: wsock32.lib
  before ws2_32.lib, or the names both export (Winsock 1.1 `getsockopt`/`setsockopt` number some options
  differently) bind to WS2_32. With that order the 306 imported names are the same as before.
- Kept: `/FIXED /BASE:0x400000 /HIGHENTROPYVA:NO` (the absolute member aliases needed a 32-bit base; the build has
  only been tested so: dropping it, and ASLR, is a separate step).
- **Upstream merges**: upstream keeps `link/exe/*.tsv`, `names.txt` and `imports.txt`; here they are deleted
  (modify/delete conflicts: keep them deleted). A new ICF alias, member alias or WON reference upstream shows up as
  an unresolved symbol of the plain link: write it in source as above.
- **Gates**: plain link 0 unresolved / 0 duplicates; tour + 120 s pass; seeded 2-AI Hard game to the time cap,
  binary PT 14,680 (units 0/59/39), text db PT 14,683 (0/59/39); before (part 1) 14,040 (0/56/39) / 14,149
  (0/56/40). The game's AI trace lines (epoch and research timing, deaths, building checks) are identical to the
  runs before and to the baseline up to where each run stops; the end PT and the last units follow the
  wall-time-dependent civilization preempt (PT 3,374 / 3,234 here, 3,948 / 3,304 before).

## Linux and macOS (branch posix)

- **Entry points**: `tools/fetch_posix.py` (sha256-pinned: the Debian 11 sysroot, wgpu-native v24.0.3.1 for Linux
  and macOS, SDL 3.4.18 sources, Liberation Fonts 2.1.5) and `tools/xbuild.py` (targets windows-x86_64,
  linux-x86_64, macos-arm64, macos-x86_64; commands test, fonttest, sdl3, sdltest, census, targets). Both stand
  beside `tools/build.py`, which they do not touch. CI: `.github/workflows/posix.yml` (ubuntu-latest, macos-latest)
  runs on pushes to the branch `posix-ci`.
- **Linux from Windows works with stock LLVM.** `clang --target=x86_64-linux-gnu --sysroot=<Debian 11>` plus
  `-fuse-ld=lld` compiles and links C++17 against libstdc++ 10 and glibc 2.31 with no other tool; CMake cross-builds
  SDL3 with a ten-line toolchain file (X11 only: Wayland needs the host's `wayland-scanner`, so the cross build has
  `-DSDL_WAYLAND=OFF`; CI builds both). The sysroot is Chromium's (one pinned tarball, 17,665 files). Unpacked on
  NTFS, symbolic links become copies and nine members are skipped (names that differ only in case, e.g.
  netfilter's `xt_CONNMARK.h` / `xt_connmark.h`; nothing the game includes).
- **libstdc++ 10 is old for C++17 corners**: `std::from_chars` for `double` (EEDataText.cpp) arrived in GCC 11 (and
  in libc++ with macOS 13.3/Xcode 14.3). The cross census flags it, Ubuntu 24.04's libstdc++ 13 does not. A newer
  sysroot (Debian 12) or a `strtod` fallback removes it.
- **macOS: CI only.** The Xcode SDK's license allows its use on Apple-branded computers only; osxcross-style SDK
  repacks are not redistributable and not fetched here. macOS builds and tests run natively on `macos-latest`
  (`xcrun --show-sdk-path`, `-arch arm64` / `-arch x86_64` from the same SDK). Without an SDK, a local
  `--target=arm64-apple-macos11` compile has no system headers at all, so there is no macOS census on Windows.
- **The Microsoft dialect on a GNU target breaks standard headers.** `-fms-compatibility` stops clang defining
  `__STDC__` and the `__GCC_ATOMIC_*` macros: zlib's `zconf.h` then types `voidpf` as `unsigned char*` and
  libstdc++'s `<atomic>` does not compile; miniaudio's SSE intrinsics fail too. So the census keeps the game's C++
  units in the dialect the Windows build uses (`-fms-extensions -fms-compatibility -fdelayed-template-parsing`,
  `--dialect ms`) and compiles C, the vendored libraries and the platform layer as standard code; `--dialect gnu`
  measures what the game's own code still needs from the Microsoft dialect.
- **libstdc++ undefines `min` and `max`** (`<bits/c++config.h>`), so `<windows.h>`'s macros cannot be emulated by
  macros: every standard header included later removes them (324 units failed on that first). `platform/wintypes.h`
  declares them as two-type function templates returning the `?:` type; with `using namespace std` the one-type
  `std::min`/`std::max` stay preferred (partial ordering), so nothing becomes ambiguous.
- **`<windows.h>` off Windows: types and macros only.** `include/compat/windows.h` (on the include path of POSIX
  compiles only) maps every `#include <windows.h>` to `platform/wintypes.h`: Windows' integer types with Windows'
  sizes (DWORD and LONG are `uint32_t`/`int32_t`, not `unsigned long`/`long`), STRICT handle types, RECT/POINT/
  SIZE/LARGE_INTEGER/FILETIME/GUID, MAKEWORD & co., the calling-convention keywords as nothing, and the game's key
  numbering (`platform/PlatKeys.h`: DIK_* scan codes and VK_* virtual keys, which stay the game's codes on every
  system). No function: every remaining Win32 call fails to compile and shows up in the census. `compat/mbstring.h`
  (the includes are vestigial: every call goes through PlatString) and `compat/dinput.h` (the Input System headers'
  data types only) are the other two.
- **Census (linux-x86_64, Microsoft dialect)**: 455 of 862 units compile (every module but exe, lle, dx7hrtnl,
  tcp is complete: jpeg 44/44, exeimg 44/44, default 12/12, mixer 1/1, the vendored libraries 27/27, the POSIX
  platform layer 9/9), and 324 more fail only on the on-disk record `static_assert`s (`long` is 64-bit on LP64:
  the lp64 branch's work). That leaves 83 units with real blockers, by header: EEWonNet.h (29 units: Winsock
  `sockaddr_in`, `CRITICAL_SECTION`, `Interlocked*` in the WON client), the LP64 type mismatches (`unsigned long*`
  where `DWORD*` is now `uint32_t*`), and one or two units each for the Win32 windowing/GDI/IME/DirectInput code
  of the window switch (EEUserInterface, Empire Earth.cpp, DX7Screen, GEFont/GEFontBitmap/GEText, UIEditBox,
  Input System), Winsock (TCP Protocol.cpp), GetLastError/Sleep/GetTickCount64 left in GX and the sound engine,
  `_snprintf`/`_stricmp` leftovers and the x87 control word (`_PC_53`). Before the two header fixes below, 234
  units stopped on `GetKeyState` in `UIForm.h` and 8 on `OutputDebugString` in `UMemoryPool.h`.
- **Header-inline Win32 calls fan out**: one inline function in a widely included header (UIForm::GetNumLockOff)
  blocked 234 units. Such calls go to the platform layer first (PlatKeyIsToggled, PlatDebugOutput); the Win32
  implementations call the same functions (GetKeyState, OutputDebugStringA).
- **POSIX platform layer decisions**: PlatGetLastError keeps Win32 error numbers (errno mapped: ENOENT 2, missing
  parent 3, EACCES 5, EEXIST 183, ...; others 0x20000000|errno) because the engine records and a few callers compare
  them; PlatSocket keeps Winsock's numbering for the same reason. Directory searches return a sorted snapshot in
  NTFS's case-insensitive order (readdir's order is arbitrary; the game lists saved games and scenarios through
  them). Paths: a volume letter maps to a native root (all to "/" by default), components are matched exactly first
  and by an ASCII case-insensitive scan only on a miss, Win32's trailing dot/space trimming and textual ".."
  apply, and paths handed back are in the game's form (`C:\home\u\ee\`), so FSFileSpec's volume + directory + file
  model round-trips. A read-only file is one without owner write permission (DeleteFile refuses it, as Win32
  does). Events share one condition variable (WaitAny exact, every set wakes every waiter: cheap for a dozen
  threads). Threads cannot be suspended or killed with pthreads: only a not-yet-started thread can be (the engine
  never does either to a running thread). WSAEventSelect is a poll thread that disarms a socket when it signals and
  re-arms it on the next receive/accept (Winsock's re-enabling rule). Preferences: one INI-like file per root in
  `$XDG_CONFIG_HOME/empire-earth-aoc` or `~/Library/Application Support/Empire Earth AoC` (`EE_PREFS_DIR`
  overrides), rewritten through a rename on every change.
- **Win32 registry quirks the tests found**: a REG_SZ value written without its terminator can come back one byte
  longer through RegQueryValueExA; and the Win32 store reports `GetLastError()` after a failed Reg* call (as
  URegistry did), which is not the Reg* result. The tests accept both on Windows only.
- **Fonts: metric-compatible is not GDI-identical.** Liberation Sans has Arial's advance widths and
  usWinAscent/usWinDescent, but Arial also carries VDMX and hdmx tables (GDI's hinted ascent/descent and per-size
  integer widths) and GDI hints; stb_truetype has no hinter. `tools/xbuild.py fonttest` on Windows measures both
  for 28 requests (8-72 px, regular/bold): every one differs, the cell height by 1-2 px at most sizes (Arial -12:
  GDI tmHeight 15 / tmAscent 12, PlatFont 14 / 11) and a 44-character line by up to 6 px. Closing it needs
  FreeType's TrueType interpreter (v35 is GDI-like) on Liberation's own hints, or per-size metric tables; until
  then layouts can shift by a pixel and line wraps can move.
- **SDL3 static with clang-cl**: CMake needs forward slashes in `-D` paths (backslashes are escapes in its cache
  files), and SDL's Windows core includes `roapi.h` from the SDK's `winrt` directory (missing from the build's
  INCLUDE until added). SDL3 3.4.18 static, video/events/clipboard/message box only (audio stays on miniaudio):
  4.7 MB `SDL3-static.lib` (Linux X11: 3.9 MB `libSDL3.a`). `tools/sdl_test` (SDL window -> HWND -> wgpu
  surface on WARP, offscreen clear and PlatFont text read back, 120 frames, clipboard round trip through UTF-8)
  passes on the first build.
- **The Microsoft dialect is the other half of the census.** Compiled as standard C++ (`--dialect gnu`), only 148
  units pass: 445 stop on a missing `typename` before a dependent name, 435 on member access into an incomplete
  type and 154 on declarations parsed early (both from `-fdelayed-template-parsing`: templates in headers use
  classes declared later), 142 on `__declspec` without `-fms-extensions`. clang keeps those extensions on every
  target, so the POSIX build uses the same dialect flags as Windows; making the code standard is a separate
  cleanup (it is also what a Rust port's C++ interop needs least).
- **CI, first run**: every test passes on ubuntu-latest (217 checks; sdl_test on Xvfb's X11 with lavapipe,
  "llvmpipe" via Vulkan) and macos-latest arm64 (217 checks; sdl_test on Metal, "Apple Paravirtual device": the
  hosted macOS runners do have a GPU and a window server), x86_64 macOS built from the same SDK without running.
- **macOS census: three blockers Linux does not have.** (1) `-fms-compatibility` on Darwin makes clang's
  `va_list` a `char*`-style type that conflicts with the SDK's `__darwin_va_list` (609 units); `--dialect msext`
  (`-fms-extensions -fdelayed-template-parsing` without it) avoids that and costs 86 units on Linux (693 vs 779
  with the LP64-only ones), so the macOS build needs either the missing `typename`s fixed or the va_list conflict
  worked around. (2) A forward declaration `namespace std { template <...> struct pair; }` (EEAreaUtilities.h)
  declares another class under libc++'s inline `std::__1`: "reference to 'pair' is ambiguous" (181 units);
  replaced by `#include <utility>`. (3) The vendored zlib 1.1.3 / libpng 1.0.12 (the 2001 originals) take
  macOS's `TARGET_OS_MAC` for classic Mac OS (`<fp.h>`, no `Byte` typedef): 19 image-library units; updating both
  libraries (also for their security fixes) is the clean way. With (2) fixed, CI's second run: macOS arm64
  `msext` 389/862 compile + 287 assert-only (Linux `msext` 407 + 287; the 19 image units are the difference),
  `ms` still 105 (va_list), `gnu` 156.

## Open model and texture formats (tools/eemodel, branch assets)

- **The file's reader is the specification, the data is the test.** Each field of `.cem`/`.sst` was classed by its
  readers (MODEL_FORMATS.md), then the retail data was measured against the conversion's assumptions before the
  mapping was fixed: texture coordinates never change between frames (no TEXCOORD morph targets), bounding boxes are
  always the vertex bounds (computed, not stored), centres and radii are not quite (stored), indices are relative to
  the material's first vertex (exactly glTF's per-primitive meaning), vertex ranges overlap (shared accessors), one
  model has NaN normals (compare bit patterns, not floats).
- **A model's texture names are dead data.** `LoadTexturesForScreen` has no caller; units are drawn with the
  dbgraphics texture and the material's texture index picks its frame. The glTF images therefore point at the
  dbgraphics texture's PNG (viewers show the game's look) and keep the dead name as the image name (round trip).
- **Morph deltas are not always exact in float32.** float32(base + float32(value - base)) misses the value in 290
  sub-models (opposite signs, very different magnitudes); a nudged delta is searched first, the rest go into a sparse
  accessor (`frameFixes`).
- **glTF forbids a matrix on an animated node, including a weights-only animation.** Static frame transforms are
  written as `matrix` (exact JSON floats); a node with frames gets TRS plus the exact matrices in EE_model.
- **Exact JSON floats:** write the float32's exact value as a double (`repr`), not its shortest float32 decimal: the
  validator compares accessor min/max as doubles, and any reader (float or double) gets the bits back.
- **Editors drop unknown extensions.** Blender keeps node/material extras but not EE_model; the importer therefore
  works from the core alone (one LOD, computed centre/radii, sampled transforms) and uses EE_model only when it is
  consistent with the core. Blender's `.001` duplicate suffix is stripped from names (models have duplicate sub-model,
  tag and material names).
- **Two implementations, one result.** The C++ loader (`GEOpenFormats.cpp`) mirrors the Python importer operation by
  operation: every float step in double precision rounded where Python rounds (f32), no FMA contraction
  (`#pragma clang fp contract(off)`), node TRS values rounded to float32 on both sides (cgltf stores floats), the
  same integer/number distinction for JSON values (a bool is not an integer). `cppgate` compares the bytes.
- **stb_image's `comp` is the file's channel count before tRNS expansion:** the C++ side scans the PNG chunks
  itself (colour type, tRNS, the `ee:surface` text) and asks stb_image for 3 or 4 channels.
- **cgltf hands strings over raw:** names and URIs keep their JSON escapes (`"orbital_cw_375	_1"` is a real
  sub-model name with a tab). GEOpenFormats.cpp unescapes them with its own JSON string reader.
- **x87 does not round a cast:** on the i686 build `(float)x` of a double can stay in an x87 register at double
  precision (one radius came out 1 ulp off the Python side's). The float32 rounding goes through a `volatile float`.
- **Tool name clashes:** tools/eedata and tools/eemodel both have a `cppgate.py`; a module that puts another tool
  folder on `sys.path` must append it, not insert it first.
- Archive reads are slow in Python (the PKWARE explode: about 2 minutes for the original game's models and
  textures); the gates take about 6 minutes.

## LP64 (branch lp64)

- **Why it matters**: Windows keeps `long` at 32 bits on x64 (LLP64), Linux and macOS make it 64 (LP64). The code
  writes `long`/`unsigned long` to files and network buffers (`sizeof(long)`, `memcpy(&x, p, 4)`), keeps 32-bit
  ids with -1 for "none", hashes and rotates in `unsigned long` and prints/scans it with `%d`. None of it shows on a
  Windows build, which is why the x64 census found the formats safe.
- **Method** (no LP64 compile is possible while every unit includes `<windows.h>`): `tools/lp64_census.py`
  compiles every first-party unit (743) for x86_64 with the build's flags and lists (1) every `-Wformat`
  diagnostic, (2) clang-query matches for the width-dependent constructs: `sizeof` of a long, memcpy/memset through
  a `long*`, casts from `long*`, shifts/bitwise ops/`~` on long, all-ones sentinel compares, long/unsigned int mixes,
  unions holding a long, `sizeof` of a record in an I/O call. `tools/record_sizes.py` dumps the raw records' x64
  layouts with canonical field types and fails on any `long` field. The behaviour gate on Windows is
  `tools/objdiff.py`: the same tree compiled before and after a change, compared function by function (bytes and
  relocation offsets/types). Retyping `long` as `int32_t` where both are 32 bits must give identical code; every
  commit lists the few functions that changed and why.
- **Census by category** (sites; before -> after): `-Wformat` 535 -> 0; `sizeof(long)` 586 -> 21 (all consistent
  in-memory sizes: arrays allocated and copied with the same `sizeof(long)`, the random-map parser's own `long`);
  memcpy/memset through `long*` 325 -> 21 (same, plus four float fraction words packed into `long` members between
  Load and FinishLoad: little-endian, the low four bytes round-trip); unsigned long compared with -1/0xffffffff 145 ->
  2 (`UIsNone32`, below; the two left compare Win32 results: GetFileAttributes, ResumeThread); raw records
  holding a long 37 -> 0;
  shifts 1,062 and bitwise ops 1,555 on long reviewed by file: masks, flags, colour packing and small coordinates,
  width-independent except the rotates and sums fixed below.
- **File I/O**: the `FSFile` value overloads are `int32_t`/`uint32_t` virtuals; `long`/`unsigned long` go through
  non-virtual adapters that read/write exactly four bytes (sign-/zero-extending back), so the hundreds of
  `inFile.Read(theLong)` callers keep their types and write the same bytes on every data model. A short read leaves
  what the direct read left (the 32-bit copy starts as the old value). `EEPTGeneric<unsigned long>` (the saved seed)
  writes through a 32-bit copy. Derived file classes bring the adapters in with `using FSFile::Read/Write`.
- **Records**: the 37 raw records hold `int32_t`/`uint32_t` (625 fields; `UIDbControl` and the TGA header newly
  pinned by `static_assert`). Retyping a field made a few `GERectangle(a, b, c, d)` calls ambiguous between the
  `long` and `float` constructors: they name the `long` one, as before.
- **Network**: every long member of the NE*/EEM* messages is `int32_t`/`uint32_t` (133), and the wire locals
  (counts, ids, the transfer id, the record file's timestamp and size, the UWideString export size, the PK01
  compression header) are 32-bit. The trap was `memcpy(&theUnsignedLong, p, 4)`: on LP64 it leaves the upper half
  undefined (a size read that way allocates gigabytes).
- **Sentinels**: an `unsigned long` set to -1 in memory has 64 bits set on LP64, the same id read from a file or
  message 32. `UIsNone32(x)` tests the low 32 bits (Windows: `x == 0xffffffff`); `USavedID` returns `uint32_t` and
  `USavedPointer` sign-extends from 32 bits; `(long)USavedID(p) != -1` became `(int32_t)`.
- **Mixed signedness**: `uint32_t` against `long` is an unsigned compare on Windows (both become unsigned long) and
  a signed one on LP64 (the uint32_t becomes long). Retyping an `unsigned long` member as `uint32_t` therefore
  changes LP64 semantics where it meets a `long`: the census's long-uint-mix finds those (two fixed with a cast to
  uint32_t, the rest are hashes of small coordinates and shift counts).
- **RNG determinism**: `URandom` is the only game generator (the CRT `rand()` only seeds a lobby game and a debug
  report period, and the host sends that seed). Its state is `uint32_t`: seed' = (0x343fd * seed + 0x269ec3) mod 2^31
  in unsigned 32-bit arithmetic, which is the same function on every data model (and was already: 2^31 divides
  2^32, so the old `unsigned long` LCG agreed in its low 31 bits; now it is explicit). `Generate()` divides in
  double, `URandomIntegers::Generate` scales a float and rounds with `lrintf`: those follow the floating-point model
  (SSE2 on x86_64 Windows, Linux and macOS alike), not the data model. Checksums (`ESChecksumEntry`, the total) are
  uint32_t sums, so a 64-bit accumulation cannot carry into bits a peer does not have.
- **Formats**: 512 conversions got the length modifier their argument needs (`%ld` for long, `%zu` for size_t,
  `%d` for a field retyped as int32_t), keeping the conversion letter (`%d` of an unsigned long became `%ld`, what
  Windows printed); five resource texts or debug strings passed as the format became `"%s"` arguments (none holds a
  `%`), two unused arguments were dropped. sscanf now writes the width it is given: `%ld` into `long`, and the
  random-map parser's `sscanf("%d", void*)` scans `%ld` into its `long`.
- **Left for the POSIX port** (not data-model issues of the game's own types, or not reachable): `wchar_t` is 4
  bytes on Linux/macOS (the WON lobby reads UTF-16 as `wchar_t` from the wire; UWideString is char-based, so the
  game's own formats do not depend on it); Windows SDK structs read from files (`BITMAPFILEHEADER`/`INFOHEADER`
  through `DWORD`/`LONG`, `#include <pshpack1.h>` around the TGA header) need fixed-width definitions there; every
  format is little-endian; `(unsigned long)-10000` coordinate sentinels (EEComplexUnit sight updates) compare a
  converted long, which sign-extends alike on LP64; in-memory `long` arithmetic that overflows 32 bits (with
  `-fwrapv`, Windows wraps, LP64 does not) is not censused beyond hashes, checksums and the RNG.

## x87 range: the black game view (branch render)

- **Symptom**: on x64 the 3D game view stayed black during play (terrain, units, shadows), while the UI, the cursor and
  the mini map (drawn on the CPU from the terrain) were fine; the logic ran normally. An i686 build with
  `/arch:SSE2` was black too, the x87 i686 build was not: a floating-point model difference, not pointer size.
- **Cause**: the graphics thread's light fade (EERenderObject.cpp, the per-frame update) computes
  `start + (1 - start) * (rate * elapsed)`; a fade "at once" (`EEStartLightFadeIn(0)`, which the main game form posts
  when the game starts) sets the rate to FLT_MAX. With x87 code the product stays in an extended-range register
  (FLT_MAX * 16 ms is about 5e39, finite), and `(1 - 1) * 5e39` is 0: the fade stays 1. In SSE float arithmetic the
  product overflows to infinity and `0 * inf` is a NaN, which passes the `> 1` / `< 0` clamps. From the first frame
  every light intensity, the ambient and the fog colour were NaN times their colour: GX drew all the geometry
  (`EE_GX_STATS`: ~140,000 triangles per 60 scenes, 90% of the vertices inside the clip volume) black. The original
  VC6 build and the x87 clang build never saw it.
- **Fix**: the fade and the letterbox progress (the same formula with the same FLT_MAX "at once" rate; the
  letterbox's NaN would also stop a scenario's ending cinematic, which waits for the zoom to be exactly 1) are
  evaluated in double, the x87 build's range; the result is clamped as before. Other FLT_MAX values in the code
  are minimum trackers that real values replace before any arithmetic, comparison bounds, or divisors (UMTan's
  FLT_MAX at a zero cosine: `x / FLT_MAX` is finite either way); none is multiplied.
- **Lesson**: "float arithmetic is the same on SSE" does not hold for code written against VC6's x87 code: products
  of a sentinel (FLT_MAX, 1e38) only stay finite in an x87 register. A NaN is silent: it passes every ordered
  clamp. Look for sentinel arithmetic when x64 and x87 builds differ wholesale, not in the last digit.
- **Diagnostics**: `EE_GX_STATS=<n>` (opt-in) logs every n scenes the 3D draws by topology, lit/textured/blended/
  fogged/z draws, the vertices after the world-view-projection (in front, inside the clip volume, clip ranges), their
  colour range and the fog range/colour with the vertices before/in/past the fog. It told "drawn black" from "not
  drawn" and "drawn off screen" in one session: geometry inside, fog colour and ambient 0xff000000.
- **Rendering gate**: the harness's tour (and random-game) exercise now checks the captured gameplay frame and, after
  `--play-seconds` of at least 10, the play frame (`session_exercise.check_rendering`, evidence `rendering.json`):
  the game view (x 0-80%, y 6-88% of the frame) needs at least 30% pixels with a channel above 24 and 512 colours at
  5 bits per channel (correct frames: 78%, ~3,000; the black ones: 0.1%, 181, the cursor); the mini map's inside
  (x 84-99.5%, y 73-93.5%) at least 0.2% coloured, non-grey, non-player-blue/red pixels (correct at the start:
  0.7-1.1%). A capture used to need only 16 distinct byte values in the whole frame, which the UI alone provides.
