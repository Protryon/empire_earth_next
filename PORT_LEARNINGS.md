# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later (the WON library: "The WON library from
source").

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

## The WON library from source (upstream merge, 2026-11-08)

Upstream's source revisions up to 3f51317465d6 brought the WON library (578 sources, 153 headers) and a renaming and
readability pass of the lobby, on top of byte-matching rewrites of the game code. Merged under this fork's changes;
the library is module `won` (tools/ee.py, tools/units.py) and `WONAPIStubs.cpp` is gone.

- **Review every file both sides changed, conflicted or not.** Git's line merge spliced upstream's reshuffles (swapped
  if/else with a negated test, hoisted locals, getters) into ported code outside the conflict markers: a doubled
  `else`, emptied branches, a VC6-scoped loop variable in a lifted-cap loop (EETechTree::GetChecksum), NULL
  assignments to STL iterators restored (EEWonNet::ConnectToLobby). The check that caught them per file: the working
  tree minus theirs must be exactly our delta, the working tree minus ours exactly upstream's delta (as line
  multisets, with the changes both made set aside). Then grep our own files (the ones upstream does not have) for the
  names upstream renamed (`gU5308A0` -> `gNetworkAdapter`, `Fn297ED0` -> `EELobbyIsShown`, `Fn2492C2` ->
  `EEGetSquareRingTiles`, ...).
- **Upstream's exactness fillers have no code:** `EE_LOST_DEBUG/TRACE/CASE/READ` and `LostDebug.h` only charged VC6's
  inline budget; drop every statement and include on each merge.
- **One view per class.** Upstream's headers carried `#ifdef EE_DINKUM_SXL233` splits: the WON library's revision of
  a class (built against the sxlrt233 Dinkum) and the exe's (e.g. `GUIString` inline in one, out of line at exe
  addresses in the other; `CanStartGameHook` `bool` vs `long`). In one static clang exe that is an ODR violation: the
  WON library's view is kept, the exe-side out-of-line copies are deleted (each checked to do the same).
- **Dialect passes WON needed** (30 of 576 units failed; the rest compiled as delivered): `std::auto_ptr` ->
  `std::unique_ptr<T[]>` (the buffers came from `new[]`: auto_ptr freed them with scalar delete), `random_shuffle` ->
  VC6's swap-with-`rand()%(i+1)` loop, STL iterators initialised from NULL -> default-constructed, temporaries bound to
  non-const references -> named locals, `typename`, Dinkum's `referent_type` -> `mapped_type`, and VC6's
  `unsigned short` for `wchar_t`: `extern unsigned short gEmptyWideString[]` compiles but mangles differently from
  the `wchar_t` definition, so the link names it.
- **x64 hazards WON had:** pointers through `DWORD` in the waveOut callback (`dwInstance`/`dwParam1` are
  `DWORD_PTR`), the window object in `GWL_USERDATA` (`GWLP_USERDATA`, `SetWindowLongPtr`), DbgHelp's 32-bit stack walk
  (StackWalk64, AMD64, Rip/Rsp/Rbp), the login-secret file written with `sizeof(time_t)` (8 bytes now: an `int32_t`).
  `time_t` narrowed to `long` for 32-bit wire fields is left as is (until 2038).
- **Linking every WONAPI source** (upstream's rule: each .cpp no unit list names) also links SDK reconstructions
  that duplicate an original unit (StringParserFormat.cpp) or that the original exe never contained (GameSpySupport,
  MSFileDialog: they drag in the GameSpy SDK and comdlg32): `units.WON_NOT_LINKED`. lld names undefined symbols only
  once the duplicates are gone, so a first "0 unresolved" next to duplicates means nothing.
- **Placeholder names for one address:** `Component::Fn2F5380` is `FireEvent` (both 0x2f5380: forwarded),
  `GUIStringData::Compare(other, bool)` was `findNoCase` (0x328340: the epoch/map filters are substring searches);
  globals of units upstream did not reconstruct (`gShowParseTime`, `gLinesParsed`) are defined where they are used.
- **Ours over upstream, kept deliberately:** EEUCBasicCitizen's `kTriggerSmartTaskNotAssigned` test keeps
  `mGoalState != NULL` although upstream's FIXES.md lists the inversion as a fix (it strands citizens here);
  EEEffect2D::CreateParts reads the unit's stats inside its NULL test (upstream's order dereferences NULL for an
  effect at a point).

### The WON lobby on the platform layer (every OS)

- **WONGUI's backend is ours.** The SDK separates it (WindowManager, Window, Graphics, NativeImage, Font, the RawImage
  platform calls, Clipboard): `WONGUI/Plat*.cpp` implement it on the platform layer and WON's own software rasteriser
  (`Surface<DWORD>`, WONSurface.h), with GDI's rules kept where the look depends on them (both line end points,
  rectangles x..x+w-1, mask blits AND then OR, text at the cell top, MSFont's size and underline). `units.WON_REPLACED`
  keeps the MS* GDI backend out of every link; `include/ee/wonapi/WONLobbyGUIMS.h` keeps its declarations for merges.
- **One frame, pulled by the graphics thread.** The window manager composites the lobby's windows into one BGRA frame
  (a serial per change, its own lock); `RTSGraphics::RenderLobby` copies it into a system-memory surface (the movie's
  pattern; `GEPackBGRXRow`, include/lle/GEPixelPack.h) and onto the back buffer, then ShowScene. Lock order:
  EELobbyLock, then the frame lock.
- **The lobby runs inside a posted action, which holds two locks.** `EEUserInterface::ProcessActions` holds the UI
  copy lock and the world write lock around every action, and EELobbyOpen's lobby loop never returns while the lobby
  is open; the graphics thread then blocked in `EEGraphics::FrameStarting` (the original did not care: its lobby was
  another GDI window). `EELobbyRunLoop` releases both while the lobby runs and takes them back in ProcessActions'
  order. Found with lldb, not traces: run the play directory's exe outside the harness (one debugger per process),
  drive it with the UI fixture (EE_TEST_UI, `click 12 1`), `lldb -p PID -o "thread backtrace all"`, and resolve the
  frames through EE-AOC.map (lldb shows only the nearest export; the exe is linked at 0x400000).
- **A 1024x768 layout, letterboxed.** The lobby's screens are drawn for 1024x768 (the original switched the display
  mode): the surface keeps that size and is scaled uniformly into the back buffer, centred (black bars), and
  `PlatWindowManager::ScaleToSurface` maps the pointer back the same way. `GERectangle(a, b, c, d)` is left, top,
  RIGHT, BOTTOM: a width there squeezed the image (identical only at the origin).
- **WON's files get absolute paths.** `WONAPICore::GetDefaultFileDirectory` was "" (the working directory, the data
  root); it now names the data root, so `_wonHTTPCache` and the other files are absolute (the harness refuses relative
  creates).
- **POSIX.** Sockets on PlatSocket (`PlatSocketWait`/`GetError`/`Shutdown`/options added; WSAE* are the PLAT_SOCK_*
  values), threads and events on PlatThread/PlatEvent (no SEH off Windows), time on `WONTickCount()`, the registry on
  PlatConfig (`won:` block of config.yaml: the CD key, UseLocalIP), files on PlatFOpen/PlatFile, cp1252 text on
  `PlatCp1252ToWide`/`PlatWideToCp1252`. Wide strings on the wire and in WON's files are UTF-16 LE per unit
  (`WideAppendUtf16LE`, WONBuffer.h): wchar_t is 4 bytes there, and ReadRawWString read past its buffer.
- **libstdc++'s [abi:cxx11] tag on Linux:** in this dialect a function returning std::string mangles with the tag
  or without it depending on whether glibc's `<string.h>` came before `<string>` in the unit, and two lobby units
  that start with `<string>` did not link against the definition (`GUIStringData::GetString`). Every game unit on
  Linux gets `-include string.h` (xbuild.py game_dialect_flags). The census's ms dialect also gets the Linux
  `__GCC_ATOMIC_TEST_AND_SET_TRUEVAL` define the game build has (it reported 384 phantom failures).
- **The lobby at the display's pixels (the UI round).** The 1024x768 layout, events and hit tests stay logical; the
  backend draws at a pixel scale k (`PlatWindowManager::SetPixelScale`: window images k times larger, PlatGraphics
  scales coordinates and clip, fonts rasterised at size x k, images resampled bilinearly with masks kept exact, or
  the 4x pack's `Data/Textures-4x/WONLobby Resources/...` when present: none yet, tools/upscale covers Data/Textures
  only). RenderLobby's surface has a 1024x768 logical size over the frame's pixels (`GESurface::SetLogicalSize`), and
  k = the letterbox box's back-buffer pixels / 768. Windowed GX has render scale 1, so k > 1 shows in full screen;
  fractional render scales round the copy rectangle to whole logical pixels.
- **Tests:** `xbuild.py wontest` (tests/won: the wire format byte for byte, the socket layer over loopback; built like
  game units), the harness's `--exercise lobby` (main menu -> MultiPlayer -> the lobby's frames -> Other Multiplayer
  -> the LAN screen; `--refuse-autodial`: the lookups of the gone WON servers fail as offline). The lobby owns the UI
  thread, so the UI fixture does not acknowledge commands while it is open: the exercise waits for presented frames.

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
  re-arms it on the next receive/accept (Winsock's re-enabling rule). Preferences were one INI-like file per root
  in the user's configuration directory (`EE_PREFS_DIR`); since branch nossa every system uses one config.ini in
  the game's directory instead ("No data.ssa, no original install, no registry").
- **Win32 registry quirks the tests found** (gone with the registry, branch nossa): a REG_SZ value written without
  its terminator can come back one byte longer through RegQueryValueExA; and the Win32 store reported
  `GetLastError()` after a failed Reg* call (as URegistry did), which is not the Reg* result.
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
  5 bits per channel (correct frames: 78-79%, ~3,000; the black ones: 0.1%, 181, the cursor); the mini map's inside
  (x 85-99.5%, y 74-93.5%) at least 0.2% lit pixels that are neither the white view outline nor a blue/red unit dot
  (correct at the start, grass and snow maps: 0.8-1.3%; the outline and dots alone: 0%). A capture used to need only
  16 distinct byte values in the whole frame, which the UI alone provides. The mini map was not part of this bug: it
  is drawn on the CPU and showed the explored start area in the black frames too (it is mostly black at the start
  because the map is unexplored).

## The POSIX game, part 1: the rest of the Win32 calls (branch xgame)

- **What blocked the Linux census after LP64 was mostly not Win32.** Of the 86 failing units, 30 stopped on one
  header (EEWonNet.h: `sockaddr_in`, `CRITICAL_SECTION`, `Interlocked*` in inline WON code), and a dozen on things a
  Windows build cannot see: names the code spelled through windows.h's A/W macros (`PlaySoundA`, `SetPortA` from
  winspool, a `DrawText()` call that only resolved to `DrawTextA` through the macro), `#ifdef _SET_` (the MSVC STL's
  include guard as a feature test), `_iobuf` for FILE, `HRESULT` vs `long` callback types (HRESULT is `int32_t` in
  wintypes.h), `std::max(LONG, long)` (LONG is `int32_t` there), `fpos_t` as an integer, `_getw/_putw`.
- **clang-cl's Microsoft mode is more lenient than the same flags on a GNU target**: the address of a temporary
  and narrowing in braced initializers compile on Windows and are errors for Linux even with -fms-compatibility.
  The census passes `-Wno-address-of-temporary -Wno-c++11-narrowing` (MS_DIALECT_LENIENCE), as the POSIX game build
  must.
- **Floating-point `from_chars`** is missing in libstdc++ 10 and in libc++ before macOS 13.3 (deployment target 11):
  `UFromChars` (include/lle/UFromChars.h) uses it on MSVC and strtof/strtod elsewhere.
- **Time sources and the harness**: GetTickCount64 (GX's read-back timeout, the null-3D key frames) is not shimmed
  by the time shim; timeGetTime and QueryPerformanceCounter are. Routing it through PlatTimeMilliseconds would have
  shrunk a 10 s timeout to 0.3 s under `--time-factor 32`: it got its own PlatTimeMilliseconds64 (GetTickCount64 on
  Win32).
- **String resources as data**: LoadStringA converts the UTF-16 resource to the ANSI code page; the generated table
  stores those bytes (cp1252: every character of Language.rc has a cp1252 code). The generator parses the .rc
  (escapes `""`, `\n`, `\xHHHH`) and build.py compares its result with the string tables rc.exe compiled (3,707
  equal), so a parser slip cannot reach the game. An empty string counts as missing (LoadStringA returns 0).
- **TCP Protocol**: TCPHost padded 8 bytes before its address, assuming a 4-byte SOCKET; on x64 the transfer
  connection read the family and port as the IP. Found by the loopback test (tools/tcp_test), which drives the
  plug-in's PIM entry points the way the network engine does. Socket globals starting at 0 would close stdin on POSIX.

## The POSIX game, part 2: the window on SDL3 (branch xgame)

- **Messages that are sent, not posted.** Win32 sends WM_ACTIVATE/WM_ACTIVATEAPP straight into the window procedure
  (during CreateWindowEx, DestroyWindow, SetFocus); the game's handler ran at that moment, with whatever state the
  game had then. SDL queues events, and the game pumps them later. Two consequences, both seen in the first SDL
  screenshots (the game cursor at the centre of 1024x768 instead of 640x480):
  - SDL reports keyboard focus only for the foreground window; under the harness's debugger (and over RDP) the game
    window was activated but never foreground, so no focus event came at all. On Win32 the window layer now
    subclasses its windows and reports activation from WM_ACTIVATEAPP / WM_ACTIVATE (not minimised), the two cases
    the original window procedure handled; SDL's focus events are used elsewhere.
  - The renderer's 32-bit probe (a 16x16 topmost popup created and destroyed while the display starts) hands the
    activation back to the game window when it is destroyed; the original centred its pointer then (at 640x480).
    The probe is a visible topmost borderless window again (not hidden), and the events a window's creation or
    destruction produce are dispatched right there (TSThread::DispatchWindowEvents), as the sent messages were.
  With both, the menu screenshots are identical to the Win32-window build.
- **The thread message queue does not need Win32.** TSThread::Post/Receive were PostThreadMessage/PeekMessage; only
  TSThread used them, and window messages reached the UI thread's window procedure through the same PeekMessage. Now
  each TSThread has its own queue, and Receive on the window thread pumps one window event per call when its queue is
  empty (posted messages first: Win32 also returns posted before input messages). The UI loop's "result" rule
  (a dispatched message counts only while ids queued by Post(..., true) are left) is kept.
- **The window class name is the harness's key.** exe_boot.c's capture checkpoint and its window bookkeeping look
  for "Empire Earth Expansion": SDL_RegisterApp sets SDL's class name (and SDL_HINT_WINDOWS_INTRESOURCE_ICON the
  icon) before the video subsystem starts. SDL's mouse set-up asks SPI_GETMOUSESPEED, which the harness's
  SystemParametersInfo guard had to learn (a GET).
- **Text input must be started.** WM_CHAR always arrived; SDL3 sends text events only between SDL_StartTextInput and
  SDL_StopTextInput. The game window starts text input when it is created (PlatTextInputStart).
- **macOS needs the Metal layer from the main thread**: GX takes the window's native handles when the primary surface
  is created (on the window thread) and keeps them; the render thread only reads the client size.

## The POSIX game, part 3: input without DirectInput (branch xgame)

- **Keep DirectInput's contract, change its source.** ISKeyboard and ISMouse were written against DirectInput's
  buffered data (GetDeviceData: changes only, 256 records, DI_BUFFEROVERFLOW once records were lost) and its device
  state (GetDeviceState: the keyboard's 256 bytes; the mouse's relative motion since the last state read). A small
  per-device buffer (ISDeviceInput) gives exactly that from the window events, so the two classes changed in their
  calls only: double presses, the shift/control/alt state, the overflow re-read and the "buffered movement" switch
  work as before. Auto-repeat is dropped (DirectInput never reported it), a focus loss releases held keys and buttons
  (a foreground device lost them too), and the mouse's buttons are renumbered from the window layer's (1 left,
  2 middle, 3 right) to DirectInput's (0 left, 1 right, 2 middle).
- **The exclusive mouse is relative mode.** DirectInput's exclusive foreground mouse hid the pointer and gave raw
  motion; ISMouse::Resume/Suspend switch SDL's relative mode on and off.
- **Testing input under the harness**: the window layer reads keys from WM_KEYDOWN/UP, so a key posted to the game
  window travels the real path (SDL, PlatInputEvent, ISDeviceInput, the main form's hot keys) without being the
  foreground window. `--input-check` presses F3 twice and requires the pause flag to go on and off. Posted mouse
  messages would not test relative mode (raw input), so the mouse path is covered by code review and play only.

## The POSIX game, part 4: text without GDI, the input method (branch xgame)

- **One text path for every surface.** GDI text reached surfaces two ways: a DC on the CPU copy (TextLock, the glyph
  atlases) and, for GPU-resident targets, two scratch DIBs drawn on black and white grounds to recover the coverage.
  PlatFont gives coverage directly, so GXSurface::DrawText now takes a PlatFont callback: on a GPU-resident surface
  the callback draws onto a transparent target in premultiplied colour (PLAT_FONT_TARGET_PREMULTIPLIED: alpha
  accumulates the coverage, an opaque background cell is alpha 255), which is blended with the existing
  premultiplied blit; on a CPU-side surface it draws onto a 32-bit copy of the pixels and the changed ones are
  written back in the surface's format (what the old 32-bit GetDC copy did for formats GDI cannot draw). TextLock
  is a flag now.
- **A new surface has no CPU memory yet**: the CPU path must allocate it (GX allocates lazily on the first Lock);
  tools/gx_test's 565 text check found it.
- **Fonts are built in.** Liberation Sans/Serif/Mono are compiled into the program (#embed with clang 19+, generated
  C arrays for older clang and Apple clang), registered under the names the game's font table uses; Tahoma (the
  table's fourth face) and MS Sans Serif are aliases of Arial. UIFontTable sized fonts by the screen's LOGPIXELSY:
  the original was DPI-unaware (96), the SDL process is DPI-aware, so the 96 is now a constant.
- **What changes on screen**: only the glyphs (1.5-4% of a menu's pixels): stb_truetype draws Liberation unhinted
  and antialiased where GDI drew hinted Arial; stems look a little heavier at small sizes and text sits a pixel
  higher in places. The layouts did not move and no text wrapped differently in the tour's screens.
- **The input method's composition stays the game's.** With SDL_HINT_IME_IMPLEMENTED_UI "composition" SDL sends
  the composition as text-editing events and lets the system draw the candidate list. The game's edit box showed the
  composition before (WM_IME_COMPOSITION GCS_COMPSTR), so TEXT_EDITING drives the same UIManager calls; the result
  string comes as a text event while composing (no WM_CHAR bytes follow it, as before). The game's own candidate
  window (UIIMECandidate) is no longer fed. Text crosses the window layer as Windows-1252, so CJK input (double-byte
  ANSI code pages before) needs the encoding decision PORT.md describes; Western input is complete.
- **PostQuitMessage has no reader now**: the thread queue is TSThread's own, so the two remaining PostQuitMessage
  calls (the lobby's Sierra Update, the window close) post WM_QUIT (0x12) to the UI thread's TSThread instead.

## The POSIX game, part 5: building and linking it (branch xgame)

- **-fms-compatibility on a GNU target drops __GNUC__**, and system headers decide by it:
  - glibc's optimised C++ string overloads (`__OPTIMIZE__`: `__extern_always_inline` and `__asm__` labels) fail;
    `-U__OPTIMIZE__` for the headers keeps them on their plain declarations while the code is still optimised.
    Claiming GCC back (`-fgnuc-version`) is no way out on Linux: glibc then wants `__STDC__` and its own va_list.
  - macOS's SDK types `__darwin_va_list` as `void*` without `__GNUC__`, which clashed with clang's
    `__builtin_va_list` in every unit (the "va_list" blocker of the census): `-fgnuc-version=4.2.1` on macOS only
    fixes it (851/870 units at once).
  - libstdc++ compares `type_info` inline only with `__GXX_WEAK__`, else it calls an out-of-line `operator==` the
    library does not export: `-D__GXX_TYPEINFO_EQUALITY_INLINE=1`.
  These are the game's C++ units only; C, the vendored libraries and the platform layer compile as standard code.
- **The census was not the build.** `-fsyntax-only` without `-O` hides the `__OPTIMIZE__` header paths, and does not
  link: the Itanium ABI does not encode a variable's type, so the engine's and the T&L plug-in's two `gRasterizer`s
  (different types, distinct MSVC symbols) clashed at the first link. That was the only duplicate in 845 units.
- **Header leanness differs by library version**: libstdc++ 10 (the Debian 11 sysroot here) brought `<string>` into
  GX.cpp transitively, CI's libstdc++ 13 did not. The cross build here does not prove the native one.
- **#embed is new** (clang 19+): CI's clang 18 and Apple clang get the fonts as generated C arrays
  (`PlatFontDataArrays.h`, chosen with `__has_include`); a skipped `#embed` inside a false `#if` group is harmless
  to older compilers.
- **The 2001 zlib/libpng take `TARGET_OS_MAC` for classic Mac OS** (`<fp.h>`, no `Byte`); modern Apple systems
  define it too. The checks now also require `!__APPLE__`.
- **CI without game data**: `--smoke-test` exercises what is portable and observable without the retail files: the
  window system, a window, the wgpu device on the runner's GPU path (lavapipe under Xvfb, Metal on macOS), 30 colour
  fills read back and presented; the normal start must stop with the "game data not found" message (exit 3).

## Ids for the db tables (branch ids)

- **References are integers of two kinds**: a row position (almost every table) or a key field (dbtechtree: nodes are
  found by `mID`, `id / 1000 - 1` names the epoch block; dbcivpowers: row = `mID - 0x26`). A retail reference that
  names no row (-1, 0 into a hole row, 100000, 0xFFFFFFFF, a dangling 1153) is a "none" value: it stays an integer.
  Only one field needed an explicit none list: `mPrerequisite1..4` uses 0 for "pre-ticked", and 0 is also the first
  epoch's tech id.
- **Text owns the ids after the first export**: the round-trip gate re-exports with the ids the importer indexed
  (not regenerated from names), so a renamed id survives text -> db -> text.
- **The resolver needs the other tables**: a table compiled alone cannot resolve its references, so the game's seam
  holds one `Content` for all db lookups and reads a referenced table's text through the same data path. dbobjects
  (5 MB of YAML) is parsed once, early (the tech tree references it), and freed when it is built.
- **Python text-mode writes on Windows turn LF into CRLF**: an edit script that uses `read_text`/`write_text` rewrote
  whole files (`.gitattributes` has `-text`). Use bytes.
- **Gate G1 needs the loaders' skip rules**: the retail .dat files hold the converter's 0xCC fill in rows the
  loaders skip (disabled dbobjects rows, dbuiforms `mFormID <= 0`, dbterrain `mID < 0`, dbtechtree `mFirstNodeID`
  outside epoch records), so the dump writes those rows as skipped; then retail and text dump identically.

## No data.ssa, no original install, no registry (branch nossa)

- **The data is one loose tree** (`tools/open_data.py build`): the game now searches only its own `Data\` (plus a
  `Data\data.ssa` when one is present). The tree merges the four places of the original data path (AoC Data, base
  Data, AoC data.ssa, base data.ssa; first wins, names without case): 4,565 archive entries and 718 loose files
  give 5,746 files (open form) / 4,770 (binary). Clashes: 201 base loose files under AoC loose, 125 base archive
  entries under the AoC archive, 242 base archive entries under the base game's own loose files; none between the
  base game's loose files and the AoC archive, so "AoC wins over the original" holds everywhere. Directory spelling
  is unified (archive `sounds\` -> loose `Sounds\`, archive-only `civilizations\` -> `Civilizations\`), so a
  case-sensitive file system gets one folder per name.
- **Every archive entry is used as is or converted**: 37 db tables -> text (dbrandommap, dbterraingraytextures and the
  base archive's `copy of dbgamevariant.dat`, `converteroutput.log`, `converterverification.log` stay as they are),
  862 model names -> 861 glTF (one skipped, below), 2,505 texture names -> PNG (2,666 files with the `@<i>` frames), the rest copied:
  sounds (.wav 891, .mp3 10), .tai 72, .civ 23, music (.sgt/.sty/.dls 8 each, from the original game's loose
  Data\Music), .rmv 94. The `.scc` files are Visual SourceSafe markers (`vssver.scc`), not data. Conversion is not
  worth it for .civ (a 2 KB record the civilization builder writes) and .scc.
- **The one model eemodel skips** is `Data\Models\amb_rock.cem` of both installs: a 24-byte text stub ("9E527E...",
  an SSMF tag it is not). The game fails to load it as well and no archive has a real copy; it is shipped as is.
- **Textures read around the texture manager**: ColorPalette.tga, Volcano.tga and the mouse pointers are loaded with
  `GESurface::Load(kImageFileTGA)` from a file the caller finds itself. `GEFindTargaFile` serves the PNG's Targa (the
  one frame of `PngToSst` minus its 15 header bytes), so no .tga has to stay next to its PNG.
- **Campaign packages stay archives.** A campaign's identity is the archive: name, description, author, password and
  required civilization are archive tags, the campaign editor writes .ssa, the built-in campaigns are validated by the
  archive file's checksum (EECampaignFormManager::UpdateChecksum) and the progress files are keyed by it. Loose
  campaigns would be a new format for four files. The original game's own campaigns are left out of the tree: AoC
  lists only its own Data\Campaigns, so merged in they would have appeared as custom campaigns.
- **The original's install and the registry are gone** (owner decision): the base path is the game's own directory,
  so the four-place search path became one folder, the requires-original check went, and the music folder is
  `Data\Music`. The settings went from the registry to `config.ini` in the data root (`platform/PlatConfig.h`, one
  implementation in `src/Platform/Common`; `EE_CONFIG` overrides; the user's configuration directory when the game's
  directory is read-only). `URegistry` became `UConfig`; EEPreferences.cpp keeps its 231 reads and writes, now
  `GetValue/SetValue(section, name, value)`; the `Installed From Volume/Directory` values are no longer stored (a
  play folder can move). Values are untyped text: an integer read of a string fails as a missing value did.
- **The game enters the data root on Windows too** (`PlatPathEnterDataRoot` before `FSInitialize`): it used the
  current directory, so a shortcut with another working directory found no data.
- **Multiplayer checksum**: with no archive mounted the archives' `Timestamp` tags add nothing; the loose text
  tables and glTF models are summed instead. Peers need the same tree.
- **Harness**: a fresh `config.ini` per run replaces the registry snapshot/restore (and the per-instance registry
  subtrees: every instance has its own stage). `exe_boot.c` stops a run on any registry write. The stage gets the
  tree by hard links (5,700 files in a few seconds); only Saved Games and Scenarios, which the game writes into, are
  copied. The file-open log of every run is the evidence that no data.ssa and nothing of either install is opened.
- **Windows tooling traps met here**: Python's `write_text` writes CRLF on Windows and the repository stores every
  file byte for byte (`* -text`), so a script edit turned whole files into CRLF diffs: write bytes. Git Bash's
  `sed -i` does the opposite to the few CRLF files (e.g. tools/harness/savefile.py): it writes them back as LF, a
  whole-file diff (`git ls-files --eol` lists which files are CRLF). Git Bash rewrites
  arguments that start with `/` (`reg export KEY FILE /y` fails): call reg.exe from PowerShell or set
  `MSYS_NO_PATHCONV=1`.
- **Gates** (test build, game key HKCU Mad Doc Software EE-AOC deleted for the runs and restored after; no data.ssa
  and nothing of either install staged): binary tree: tour + 120 s (80.8% lit, input check ok), 2-AI Hard game to
  the time cap (PT 14,248); open tree: tour + 120 s (79.6% lit, input check ok), AI game to the time cap (PT 13,991).
  Every run: 0 opens of a data.ssa, 0 of either install, no registry write, the key still absent afterwards, a
  config.ini of 76 values written by the game ([Game] and [Game Options]). plat_test 209/209 on Windows; the Linux
  game cross-builds (846/846 units). Bundle: 5,761 files, a 327 MB zip of the 426 MB tree.

## Caps from the data (branch ids)

- **A tech id named its epoch**: every node id is (block + 1) x 1000 + slot in the retail data, and the code found a
  node with `id / 1000 - 1`. Inserting an epoch then renumbers every later tech (and breaks `kTech*`). A map from
  node id to the block it was loaded in (filled by the epoch's Load) frees the ids; the epochs keep their tech ids
  0..count - 1 by position.
- **"15" had three meanings**: the epoch count, "never obsolete" (mObsoleteEpoch 15, dbobjects mEpoch 15 on 18
  rows) and "random start epoch" in the game options. With 16 epochs a stored 15 becomes a real epoch: the text
  writes `never` for the first two (compiled to the count) and the options use the count for random.
- **The two event hooks past the table** (0x7d1 on every research, 0x7d2 on every epoch availability) ran nothing
  only because 1500 < 2001; a table sized from the data would have run rows 2001/2002. They are named negative ids.

## Window presentation and harness races (2026-11-05)

- **Blits to the primary are not all frames.** At each display-mode change the game draws its 32x32 mouse pointer
  straight onto the primary surface (DirectDraw drew it into the visible screen). GX presented every blit to the
  primary as a frame, stretching the pointer over the window for a frame: a "huge cursor" flash at start-up,
  settings changes and entering the game. GX now holds the last whole frame (a source of at least half the client
  size in each direction) and draws smaller blits onto a copy of it at their place (`PrimaryBlt`). A "whole source,
  same-size destination" test is not enough: the pointer blit is exactly that. `GX: presenting WxH frames` is logged
  whenever the presented size changes; `EE_GX_CHECK_PRESENT=1` compares the GPU copy about to be presented with the
  CPU copy (a diagnostic; equal in every check so far).
- **`--no-render` sessions look garbled on screen by design.** `EE_GX_NULL3D` skips the device's draws and clears
  except one keyframe a second; menu backgrounds and buttons are device draws, text is not, so the window keeps an old
  picture with each screen's text piling up. Judge display only from rendering sessions (the tour).
- **Concurrent threads and return guards (exe_boot.c).** With config.ini written from two threads, two threads run
  through the same guarded return address. A thread that hits a return watch belonging to another thread is now
  stepped over (the watch stays pending); a breakpoint event that arrives after its guard was removed (the int3 was
  already executed, the event queued) is rewound instead of resumed one byte into the instruction (c000001d).

## Rules as data (branch rules)

- **The binary tree has no ids**, so a rule that names a row by id cannot be read there. Each rule therefore has a
  C++ default (the original's literal, integers for the rows) and a text form (ids) that `tools/eedata/rules.py`
  writes from the same code constants, which it reads out of the C++ headers. The content dump makes the two meet:
  dump(binary tree: the defaults) == dump(text tree: the vanilla documents read by the game's reader) catches any
  drift between the Python and the C++ copy.
- **A raw index stops compiling when the accessor takes an enum.** `gWorldTable.GetLongValue(EEWorldConstant)`: an
  `int` does not convert to an enum implicitly, so all 120 dbworld reads had to name their constant, and a new one
  cannot slip in. The enum is generated from the ids (cppgen), and `check` keeps it current.
- **Moving a field's text to another file needs the compilers to fill it back.** dbmusic's per-epoch flags became
  epochs.yaml's `music` lists; both compilers set the flags from the lists (`epoch_flags` in the specs), so the
  compiled table and its dump do not change while the text has one source.
- **Unifying duplicated code keeps each copy's float order.** ApplyHit, ApplyAreaDamage and the trample computed the
  same damage with different rounding (UMFloatToLong vs UMCeil for the dead food, the trample rounds the armour,
  only direct hits scale a fanatic attacker); `ComputeDamage` takes the form and keeps every difference. Pre-scaled
  tables (the difficulty damage tables) can be replaced by the multiply at the lookup: the same IEEE product.
- **The harness reads every `*.json` in the stage folder**: a test file there (an `EE_DUMP_CONTENT` dump) breaks the
  session's report; name it `.txt`. Writes outside the stage folder are stopped by exe_boot.

## The POSIX game with real data: macOS (2026-11-05)

The first runs with the data bundle on a Mac (arm64, Metal; the x86_64 build under Rosetta behaved the same). Every
bug below was invisible on Windows: four are LP64 leftovers the census could not see (a `long` that only matters at
run time), the others are Windows CRT behaviour the POSIX layer did not provide yet.

- **Game paths reached `fopen` unresolved.** Ten engine sites (the mixer's wave files, the music loader,
  `FSFileSpec::FOpen`, `FSDebugFile`, `FSMemoryFile`, the random map scripts, the scenario editor, the lobby file
  transfer, the out-of-sync log) passed `C:\...` paths straight to `fopen`; every sound failed to load and the first
  duplicate dereferenced the missing wave (`SPIMDuplicateSample`, now also tolerant of a sample that did not load).
  `PlatFOpen` (PlatFile.h) is `fopen` on Win32 and resolves the path first on POSIX.
- **Text mode.** The Windows CRT reads a file opened without `b` with CR LF turned into LF; every text file of the
  game has CR LF lines. Without it the last token of every random map script line kept its `\r`: numbers did not
  parse, `#include` names and unit names did not match, no start units were defined and every map failed the
  connectivity check (11 attempts, then a game of water with every player defeated at once). POSIX `PlatFOpen` reads
  a read-only text open through a CR LF filter (`funopen` on macOS, `fopencookie` on glibc; rewind and ftell only).
- **`strtof` reports subnormals as ERANGE**, `std::from_chars` (MSVC) does not: `dbobjects.yaml` holds
  `mFarmGatherTime = 4.684328e-39` and the db loader stopped with "outside the float range". `UFromChars`' fallback
  treats ERANGE as out of range only when the result overflowed or underflowed to zero.
- **LP64, the run-time kind**:
  - `GXVERTEX_STD_COLOR::color` was `unsigned long`: the vertex grew from 0x24 to 0x28 bytes while GX reads with a
    0x24 stride, so every vertex after the first was read 4 bytes further off (the menu background as a fan of
    stretched triangles; GX's stats showed clip x up to 3e20). Now `uint32_t` with a `static_assert`.
  - 32-bit pixels through `unsigned long*` (GESurface's loaders, DX7Surface's filtered copy, the movie packer, the
    terrain colour table, the volcano texture) wrote 8 bytes per pixel: a heap overflow past every 32-bit surface
    (first seen as AppKit's "method cache corrupted", then malloc's own check). Floats read through `unsigned long*`
    (fog start/end, mipmap bias) and `long*` (the checksums of calamities, area effects and two unit groups) read 4
    bytes past the float. All `uint32_t`/`int32_t` now.
  - `gBinaryChecksum` summed in a 64-bit `unsigned long` carried past 32 bits (0x25099fd31) while the connection
    request carries 32 (0x5099fd31): the local server refused its own player ("bad checksum") and Play Random Map
    never opened the lobby. The sum is `uint32_t` (the same value mod 2^32 on every data model).
  - `long theMaximumX = 0x80000000` (the visible terrain rows, `UGraph`'s bounds) is INT_MIN with a 32-bit long and
    +2^31 on LP64, so no row lowered it: a visible width of 2147483635 and a crash in the terrain lighting.
    `INT32_MIN`; the random map parser's `kRMUnset` keeps its 32-bit (negative) value the same way.
  The census looked for `long` in file and network formats; these are in-memory GPU layouts, pixel pointers, a
  checksum compared with a 32-bit wire value and 32-bit sentinels. Worth a grep elsewhere: `unsigned long*` into
  raw memory, `long x = 0x8.......`, `*reinterpret_cast<(unsigned) long*>(&aFloat)`.
- **The follow-up audit** (the patterns above, over the whole tree) found 21 more float-as-`long` checksum reads (unit
  goals: gather, location, attack unit/ground, unload, `EEComplexUnit::GetChecksum`; harmless after the 32-bit
  wire truncation but an 8-byte read of a 4-byte float each tick) and `TMGetTime` keeping the full 64-bit millisecond
  count on LP64 (past 2^32 after 49.7 days of uptime, macOS counting sleep, while ping and start times travel as
  32-bit fields): now 32-bit like Windows'. Clean: GX layouts, `#pragma pack` records, other pointer casts, the
  sentinels, the RNG and checksums on the wire, raw file opens. Left: `UMFloatToLong`/`UMFloor` use 64-bit `lrintf`
  (NaN/inf/out-of-range differ by platform; no reachable case found), the WON lobby's UTF-16 as 4-byte `wchar_t`.
- **Undefined behaviour the 32-bit original survived** (AddressSanitizer, `xbuild.py game --sanitize address`):
  a one-token random map `#define (NAME)` (and a bare `#if`/`#elif`, an unmatched `#elif`/`#else`/`#endif`, an
  unquoted `#include`) read the destroyed second element of a cleared vector (libc++ frees it);
  a script without `CliffTexture` (Continental and most others) passes -1 to `EEMap::SetCliffTexture`, which indexed
  the cliff table at [-1] (the original read the heap word before it as a table pointer). Both fixed (an empty value;
  the first cliff terrain, as `EEMap::Load` does with a bad saved index). Quitting a game opens the post-game
  summary, whose `AddStatusBar` assigns a status bar's `GEText` to itself; `UWideString::operator=` then strcpy'd
  the buffer onto itself (ASan: strcpy-param-overlap). Self-assignment now returns at once.
- **The test driver** (`tools/harness/posix_game.py`): the Windows harness's debugger and presentation bridge do not
  exist off Windows, so the game carries the bridge's UI fixture itself (`EETestFixture.cpp`, only with `EE_TEST_UI`):
  the same ui-state.txt and test.command formats, the controls' own methods, plus game telemetry (game time, ticks,
  players, populations, results). The driver reuses ai_game.py's lobby options, dismiss table and parser; macOS
  captures come from `screencapture -l` of the game window (`macos_window.swift`). The game changes into its data
  root at start: the fixture directory must be absolute.
- **Debugging off Windows**: `xbuild.py game --sanitize address` (a separate `game-address/` output) found each memory
  bug at its first bad access; lldb's regex breakpoints are too slow on this binary, a temporary `fprintf` was faster.
  The GX stats (`EE_GX_STATS`) located the vertex bug (pre-transformed 2D with clip x 3e20) before any debugger.

## macOS: sound, full screen, the menus' resolution and the render scale (2026-11-05)

- **Sound**: the mixer opened the first of WASAPI, DirectSound, WinMM and null; off Windows that was always the null
  device (silence). Each platform now tries its own backends (Core Audio; PulseAudio, ALSA) before null. Music heard
  long after the Mac game closed was a Windows VM / RDP session's audio on the same machine.
- **Full screen on macOS**: the window was created at the desktop's pixel size (twice the points on Retina), every
  mode change resized it (which takes a macOS window out of full screen), and the 32-bit probe opened a visible 16x16
  topmost window at every renderer activation (focus loss: minimized). Now: full screen without a Space of its own
  (`SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES` 0; the Spaces transition is asynchronous), created at the logical size,
  `PlatWindowSetSize` leaves a window asked to be full screen alone (SDL's flag follows late), no minimize on focus
  loss, full screen re-entered on focus, no probe window on macOS (GX supports 32 bits). A process started from a
  background shell while another application is in front is never made active and macOS refuses it full screen:
  posix_game.py runs are windowed for that reason.
- **Resolution**: a first full screen start (no saved size) plays at the display's logical size
  (`PlatWindowGetDisplayLogicalSize`), which GX lists as a mode. The menus, 1024x768 in the original, run at 768
  lines with the game resolution's aspect ratio (`EEMenuWidth`/`EEMenuHeight`; GX lists those modes too): the controls
  keep their designed proportions.
- **Render scale (GX)**: the back buffer and its Z buffer at the display mode's size get a GPU copy
  `GXRenderScale` times larger (display pixel height / mode height, 1..4; `EE_GX_RENDER_SCALE` overrides, 1 turns it
  off). The game keeps drawing in logical pixels: viewports and blits map onto the larger texture, text is rasterised
  with a font made that much larger (`PlatFontScaled`, `PlatFontTarget::mScale`), CPU access goes through a
  logical-size intermediate (`UploadScaled`/`DownloadScaled`). So the menus' controls and the game's HUD are drawn at
  the display's resolution: sharp text now, and sharp art once the textures are upscaled.

## Settings UI for modern displays (2026-11-05)

- **The settings form's new controls are copies of table rows** (`UIControlsTable::AddControlLike`: a template row's
  type and look, a new id, form index, group, proportional rectangle and string range; combo box lines are their
  strings, e.g. "UI Scale: 150%"), so no data format changes. A form's control arrays are made with the form at
  start-up, `mControlCount` long: indices past the table's need `UIForm::ReserveControlIndex`, which grows them
  (a write past them crashed in `SetTabbing`).
- **Control events are queued**, not run inside the call that fires them: a re-entrancy flag cannot stop two
  controls that set each other (the quality combo box and the detail check boxes looped forever). Set a control only
  when its value differs, and apply a preset only when the details do not already match it.
- **Replaced controls are hidden, not removed** (`SetDisplayMode(kDisplayModeHidden)` survives the tab groups'
  `Enable`): the quality preset still drives the hidden "Hi-Res" check boxes, and `ApplySettings` still reads them.
- **The game's resolution is the UI scale's** (`GXGameSize`): the display's logical size over the scale (Auto: its
  height over 1080 lines), windowed a fixed window of 80% of the display; GX lists every such size as a mode
  because the game only switches to listed modes. Out of a game a window mode, UI scale or render resolution
  change applies at once (the window, then `ResizeGameScreen` at the menu size).
- A stopped (lldb-attached) game blocks the next start: the single-instance check makes it exit 0 at once.

## The AI decision trace and benchmark (branch new-ai, 2026-11-05)

- **Debug strings are the trace channel.** The harness debugger copies each `OutputDebugString` into the session log
  as `debugstring ...`, but only its first 255 bytes (`exe_boot.c`, `OUTPUT_DEBUG_STRING_EVENT`). `EE-AI` lines are
  kept well under that. The game writes no trace file: the harness guards file writes.
- **The trace must not touch the simulation.** The stats line only reads (`GetTotalPopulation`, `GetIdleCitizens`,
  the unit list). Real time is measured only around `EEComputerPlayer::Update` (`std::chrono::steady_clock`) and goes
  into separate `EE-AIPROF` lines that the comparison skips. Two runs of one seed print identical traces with
  the trace on (4/4 pairs, cheats on and off).
- **The ai-game time cap is coarse.** `--max-game-seconds 240` and `600` both stopped at about 860-905 game s
  (PT ~13,700, as the gate); 3600 stopped at 3,882 s. So `ai_trace.py compare` cuts both traces at the earlier run's
  last game ms.
- **"Idle" citizens.** `EECPUnitManager::GetIdleCitizens` returns the citizens free for building, gatherers included.
  The trace counts `idle` (no active goal) and `free` (that list) separately.
- **Harness AI games have an idle human in slot 0.** The pop-cap rubber band (rows 92/93) and the human-first target
  choice both react to it: the cap is ~112 instead of 400 × 1.4, and an honest AI game can end when the AIs defeat
  the human (docs/AI_RESEARCH.md §8).

## The headless simulation (branch headless-sim, 2026-11-06)

`EE-SIM.exe` (`build.py --sim`, README "Testing") is EE-AOC.exe's objects plus a console entry. The switches are in
`platform/PlatSim.h` (all off in the game); the driver is `src/Empire Earth/Empire Earth/EESim.cpp`.

- **Keep the start-up, replace the player.** The path from the lobby to a running world goes through the local
  server's messages (EEMStartLoading, the preloading thread's EEMStartLoadingResponse, EEMLoadMainScreen and its
  response, NEMStartGame, which also draws one number from the physics random generator) and through the UI, graphics
  and physics threads answering each other (pause/resume, the physics start). Re-implementing it would fork the game;
  so the sim runs the game's own WinMain and UI loop, and a driver thread clicks the menus and sets the lobby combos
  through the controls' own methods, as the harness's UI fixture does (the same controls, defaults and order as
  ai_game.py). Only the front end is stubbed: the window is created hidden (DX7Screen::CreateAWindow), the graphics
  thread answers its messages but renders nothing (RTSGraphics::Main), the mixer opens miniaudio's null device, the
  splash screen, intro movies and the single-instance mutex are skipped, message boxes go to stderr.
- **The clock is the communications tick.** In the game the server's send thread sends a communications tick (CT)
  every send interval (350 ms) with `PTsPerCT` physics ticks (14 at the harness's pinned 40 PTs per second) and the
  pending inputs, and the physics thread runs them on its timer. A physics tick depends only on the CT stream, not on
  the wall clock, so the sim's physics thread sends the next CT itself as soon as it has run the last
  (`NESynchronousServer::SimCommunicationsTick`, with the rate the pin gives and no wall-clock tuning) and the send
  thread sends none once the game runs. One of the thread's messages is handled after each physics tick, as the timer
  loop does. Single player sends a CT synchronously (NEMessageManager::Send -> HandleSentMessage ->
  EEServer::ProcessSystemMessage queues the physics tick), so this needs no thread hand-off.
- **Same seed, same game, same trace as the harness.** The driver pins the start seed where the harness does
  (EEMStartLoading::Execute), keeps the idle human in (the harness's --no-human-defeat: RemovePlayerFromGame and
  SetGameResult(2) for the local player do nothing) and sets the human's civilization flags as the harness's
  pre-empt leaves them (mCivilizationChosen, mCivilizationLocked) before the first physics tick. Seeds 2-7, Hard,
  Small, 2 AIs, honest, balanced: two sim runs of a seed print identical traces (6/6), and the sim's trace equals the
  harness's (build/boot/ai_bench/trips-honest, the same commit) for the whole 2700 game s for seeds 2-6. The `pt=`
  of the hash lines matches too (14 PTs per CT); only `inputs=` differs (the harness's civilization-timer messages
  land at wall-time-dependent CTs; the sim has 3 inputs, the harness 4).
- **The autosave changes the game.** Seed 7 differs from the harness from 1520 game s. The harness game autosaved
  at ~1503 s (AutoSave In Milliseconds 600000 counts PT time: 24,000 PTs = 1500 game s); the sim has the autosave off
  by default. `EECPCivilizationManager::Save` sets `mBuildingCountsChecked = true` before writing it: saving changes
  the computer player's state. With `--autosave-ms 600000` the sim saves too and its hash lines (units, random seed)
  agree with the harness's again, but the civilization manager's next build-queue line comes 1-2 s earlier: where the
  save lands depends on which CT carries the save request (SendToAllAndWait), which in the harness depends on the wall
  clock. So harness games are not reproducible past their first autosave either (the harness's determinism pairs were
  15-minute games). Not fixed here (a game change): writing `true` without setting the member would make saving
  side-effect free.
- **Several at once.** Two sims crashed on start: the TCP protocol's fixed ports 33334/33335 (the local server binds
  them in single player too). The sim moves them by `--port-offset` (default: a free pair from 35334 up; the
  harness's parallel instances use 33334 + 16 k).
- **A file lookup race in the game, found by the speed.** 2 of ~40 sim games died of heap corruption at ~60 game s
  (seed 7): the physics thread, loading a finished building's media (EEUGBuildRepair::Work -> EEComplexUnit::LoadMedia
  -> GEFindTextureFile -> FSPath::FindFile), and the preloading thread, loading a unit class's, searched the shared
  data path at once. FSPath::FindFile ran the search on the path's own directory spec, whose single search handle
  both threads then closed (PlatFindClose twice). It searches with a copy of the spec now (FindFile and ListFiles);
  0 of 66 games failed after. The same race is in the game at normal speed, only rarer.
- **The UI loop competes for the world lock.** EEUserInterface::Run updates the interface every millisecond under
  the world's read lock; while the sim's game runs it does so every 20 ms. A 45-minute game's game part went from
  8-11 s to 4-7 s with identical traces.
- **Where the time goes.** A 45-minute 2-AI Hard Small game: ~2 s to the main menu, ~5 s of lobby (the driver waits
  for each combo's update to come back through the local server's lobby ticks), ~3.5 s loading, 4-7 s of game
  (400-600x real time) -> 13-17 s per game, two at once on this machine; the harness took ~172 s.
- **The game moves the current directory** to its data root at start (PlatPathEnterDataRoot): the sim makes its
  report, trace and log paths absolute first. The game is built without exceptions: the driver reports a failure and
  exits (no throw).
- **The binary tree has no rules documents.** `--set` changes a value as the document is parsed (EERulesAddOverride
  in EEDataText::Content::Document; a list entry by its id). With the binary tree, tools/sim.py writes the document
  into the stage from tools/eedata/rules.py (only those without db references: aipersonalities, gameoptions); a run
  with a value equal to the default prints the same trace as a run without the document. A tree built by other
  converter code (open_data.json `converters`) may not load: the main checkout's open tree had an aipersonalities key
  (`demand_split`) this branch does not know, and the game stops on an unknown key; tools/sim.py warns.
- **POSIX**: `ee-aoc --sim <EE-SIM options>` runs the same driver from the game's main (EESimMain returns -1 and
  main goes on with the game). It compiles for linux-x86_64 with the game's flags; it has not been run there.

## macOS playtesting under lldb and ASan (2026-11-06)

- **Play under the debugger.** `tools/play_posix.py` runs the ASan build under `lldb --batch`; a crash, a sanitizer
  stop or a kill leaves `crash.txt` (every thread), a stack core and `asan.*` in `build/playtest/<time>/`. A freeze
  leaves nothing by itself: `sample <pid> 3` reads the stacks of a running (even debugged) game without stopping it.
- **A dead unit in the x-sorted unit list** (`gWorld.mUnits`, the RTSUnitLink chain): one crash
  (`EEWorld::ResortUnit` read a null `mPrev`, fault address 0x38 = `mTileX`) and two freezes (`ResortUnit` looping
  for good on the physics thread, the UI and graphics threads waiting on the world lock). `ResortUnit` ends on any
  well-formed chain, sorted or not, so the chain itself was broken.
  - `EE_CHECK_UNIT_LIST=1` (play_posix.py's default) checks the chain at every `Link`/`Unlink` and walks it once
    per physics flush; it caught a removed unit (`mIsValid` false) still linked.
  - Cause: `EEWorld::PlaceQueuedUnits` placed queued units without testing `mIsValid`. A unit removed between its
    creation and the next flush keeps its node (freed only by `FreeNode`), so it was linked as a dead unit. In
    practice: the PathPoint markers of a player's move orders, created and replaced within one tick. Fix: such a
    unit is skipped (logged `EE-UNITLIST: placement skips a removed unit`), and a creator that left the list is no
    longer used as the insertion point. Original-game bug; the 32-bit game presumably survived it more often.
- **`GETerrainMesh::UpdateFogOfWar`** read the row headers of rows -1 and count + 1 at the map's edges (ASan heap
  overflow after loading a save). Harmless junk reads on 32-bit Windows; only existing rows are read now.
- **The game log was never written on POSIX**: `PlatPathGetDataRoot` returns the game's `C:\` form there, which
  `PlatCreateDirectory` translates but plain `fopen` does not. Any file opened under the data root goes through
  `PlatFOpen`.

## config.ini -> config.yaml (2026-11-07)

- The settings file is YAML now (`platform/PlatConfig.h`: a block path such as `display.resolution` plus a key in
  place of the INI section and name; `PlatConfigSetBool`, `UConfig::GetFlag`/`SetFlag`). `EEGamePreferences::Load`
  and `Save` go through one table of rows (`EESettingRows`) instead of a hand-written read and write per value, and
  the obsolete values are no longer stored (docs/DATA_FORMATS.md 2.1a). An old `config.ini` is ignored.
- **`EEGameOptions::operator=` reads the world rules** (`EERuleLong`): copying a whole `EEGameSettings` before the
  database is loaded crashes (null `gWorldTable`, fault address 0x87c). `Load` runs before it, so the defaults are a
  default-constructed `EEGameSettings` plus the resource names, never a copy of `sDefaultSettings`.
- The test profile's `windowed-software` patch matches `mFullScreen = mWindowMode == 0` once in Load now (tools/build.py).
- Harness pins are `block.key=value` (`tools/harness/config_yaml.py`); the bit-depth pins are gone (Load sets 32).

## A transport's unload line off the map (EE-SIM, 2026-11-07)

- **Crash:** `EEGetBlockingUnits` read `inTile->mUnits` through a NULL tile. `EEUCSpaceTransport::FindUnloadTiles`
  (and `EEUCNavalTransport`'s copy) walks a tile line from the cargo's tile toward the unload destination and asks
  `EEMap::GetTile` for each point; past the map edge that is NULL. The 32-bit original read through it without a
  fault. Found by a 150-minute EE-SIM game (frames resolved with `tools/sim.py resolve`).
- **Fix:** the line ends at the first tile off the map, and `EEGetBlockingUnits` treats a NULL tile as holding no
  units (its other callers pass `GetTile` results too).

## Unit upgrades lost on loading a save, on macOS/Linux (2026-11-08)

- **Symptom** (owner's playtest): after every load of an autosave, units kept some purchased improvements (attack,
  the armours) but lost others (hit points, range, speed, ...).
- **Cause:** `EEUnitClassImprovement::Save`/`Load` move `mLevels[16]` as 0x40 raw bytes (16 four-byte words, as the
  32-bit original). `mLevels` was `long`: on LP64 that is 16 eight-byte values, so only levels 0..7 (area effect,
  attack, six armours) reached the file and came back; levels 8..15 (flight time/power, hit points, leadership,
  range, repair, speed, transport) loaded as 1. Windows (LLP64) was unaffected. The LP64 census looked for `long` in
  raw records read as structs, not in member arrays read through `Read(char*, n)`.
- **Fix:** `mLevels` (and Load's `theOld`) are `int32_t`, pinned by a `static_assert` of 0x40 bytes. Windows
  layout and save format unchanged. A macOS/Linux save written before the fix holds levels 0..7 as eight-byte
  pairs: it loads with its improvement levels shuffled (and some at 0), so such saves are best not reused.

## Shift-queued builds and the smart task (playtest, 2026-11-07)

- **Report:** "if i shift click a worker, they shouldnt do a smart task": a citizen sent with shift to build a
  settlement and then to a mine went off chopping trees.
- **Cause:** the citizen script (`citizen.tai`) sends a citizen whose build queue is empty from a finished building to
  `TaskSmartCitizen`, and for human players `EEUGBuildRepair::Step` runs `FindNextWork` there: it picks a job from the
  building (a drop site: the nearest resource, often trees). Its `SetGoal` replaces the finished build goal, so a
  shift-queued goal behind it (a gather at the mine) waited forever behind the endless gather. The original does the
  same; queued builds in the middle of the queue never took the smart task (`BuildQueueEmpty`).
- **Change (deliberate divergence):** `EEUGBuildRepair::IsShiftQueueNext` skips the smart task when another goal is
  queued behind the build goal (the script's `Idle` then starts it), or when the build queue came from a shift-click
  (`EEComplexUnit::mHasShiftQueuedBuild`, set by `EEMBuildUnit`/`EEMRepairUnit` with shift, cleared by
  `OrderRepair`, `ClearBuildTargets` and going idle with an empty build queue): the citizen idles after the last
  queued building. A farm is still worked. A build without shift keeps the smart task. Computer players never take
  the smart task (`IsHuman`) and never send these messages, so the AI is unchanged. The flag is not saved: after a
  load the citizen takes the smart task as before.

## The next computer player's tactics layer (S1, docs/AI_LUA_PLAN.md §2.1, 2026-11-08)

- **One switch, many gates.** Every classic-only decision asks `EECPNextEngine(player)`; with the switch off each gate
  is a short-circuited test with no side effect, so a classic game's EE-AI trace (sim hash lines included) is
  byte-identical to the one before the change. Compare traces, not results: `ai_trace.py compare`.
- **CanAfford is called from inside the executors.** `EEComplexUnit::QueueResearch`, `EEPlayer::CanProduce` (the
  build runner's `CanProduceUnitType`) and TryResearch all reach `EEComputerPlayer::CanAfford`, so its single branch
  to `EECPNextCanAfford` puts the strategy's reserve on every spend; the epoch advance is exempt.
- **The view must not depend on the trace.** The classic income hook (`EECPTraceIncome`) only counts while the
  trace is on; the next engine keeps its own income ring in `EECPNextState` (fed by `AddResource`), so a game with and
  without `EE_AI_TRACE` takes the same orders. Order trace lines are formatted only when the trace is on.
- **A queued building can be dropped silently.** `StartBuilding` with a tile calls `EEMakeBuilding` and counts the
  request done even when that fails; `build()` finds its site at order time (and reports `no_site` there), so a site
  taken in the meantime shows only as a building count that did not rise.
- **The build runner stops at the first unaffordable request of an epoch** (`CanProduceUnitType` fails: the walk
  goes on with the next epoch's queue), so the order of builds matters while the strategy saves.
- **Drop-site distance.** A settlement placed in the rings round a patch (up to kEECPPatchDropRange + 2) can stand
  further from the patch than kEECPPatchDropRange measured to its centre: the view's `has_drop` uses the placement's
  reach, or a strategy builds a second settlement at the same patch.
- **EE_AI_EXP takes sets from player 1**, the idle human: a per-seat experiment for p2 alone is `-/x/-` (`x/-`
  gives it to p1 and p3). The next engine itself is chosen by S0's `--engine` (seat 1 = p2), not by an experiment.
- **The Stone Age's SOL is not zero.** dbtechtree.yaml's `mRequiredSOL` is 0 on every epoch row, yet the next epoch
  can report SOL missing in the Stone Age (the classic engine meets it through ResearchCheapest): a strategy that
  only saves the cost never advances. The view's `sol_missing` and the costs' `counts_toward_epoch` are there for it.

## The boom script (S3, docs/AI_LUA_PLAN.md §4.1, 2026-11-08)

- **The start-up claims far sites, and the finder takes the farthest.** Regroup activates resource centres 30-40
  tiles out, and FindResourceTarget walks the claimed centres backwards (farthest first). A strategy that only
  claims what it wants still sends its miners far: it has to release the rest.
- **A resource centre can hold two minerals.** The view's `resource` is the one most units hold; the finders take any
  unit of the resource they look for, so a stone-labelled centre feeds the gold miners. Releasing it "because stone
  is not wanted" took the town's only gold (the view's `holds` now says what each holds).
- **A resource nobody can reach makes gatherers wander.** Wanted gatherers with no reachable site (unseen, an enemy
  near) are idle at each pass; AssignIdleGatherers puts them elsewhere and RebalanceGatherers moves them back: 1,520
  moves in a game. The view's `blocked` gives the finders' reason, and the script asks only for what the sites hold.
- **The next engine runs no explorers.** IsVisibleTo stays false on the larger mines 35-60 tiles out, so the finders
  never sent anyone; for the next engine they now send gatherers to a claimed site not yet visible.
- **`workers` (GetResourceUsers) is nearly always 0** in the view, even at worked sites: it counts the units at the
  resource in that instant, not the ones assigned. Use `gatherers` per resource, not per-site workers. It also
  inserts a 0 entry for an unknown unit (EEWorld::mResourceUsers), harmless but not a pure read.
- **A granary needs room for its farm plots round the claimed land** (FindGranarySite); the classic engine's
  ClaimExpand grew the claim when it had none, the next engine reports `no_site` and the script grows it. Farms come
  from the granary's plots (FindSite for a farm finds an unbuilt farm foundation), so no granary means no farms.
- **Defend groups from `defend` hold no units**: `attack.group ... units=0`, and their task names the first enemy
  player (EE-SIM's idle human). The boom uses none.
- **Python's `write_text` writes CR LF on Windows**: a patch script must pass `newline='\n'` (the repository is LF).

## The next computer player's Lua strategy (S2 gate, docs/AI_LUA_PLAN.md §3.2, 2026-11-08)

- **Orders after the step play the same game as verbs inside it** only because the C++ test opening reads nothing
  but the view snapshot taken before its first verb: its verbs never see each other's effects. A script returns the
  same calls in the same order, so the verbs' trace lines match byte for byte (the gate). A strategy that wants a
  verb's effect sees it in the next step's view and `results`.
- **0 is true in Lua.** The C++ view's 0/1 flags (`claimed`, `patch`, `advancing`, ...) are booleans in the script's
  view; `if site.claimed then` with an integer would always hold.
- **The db ids exist only while the tables load.** `EEDataText::Content` indexes the ids when it compiles a text
  table; afterwards nothing in the game keeps them. `EEDbFindFile` now snapshots dbobjects' and dbtechtree's ids
  (`EEDbRowIds`) when it compiles them; a binary tree has none, and its classes are named `dbobjects_<row>`.
- **`EE_AI_SCRIPTS` must be absolute**: the game changes into its data root at start, so a relative folder is read
  from there. sim.py and sim_bench.py `--ai-scripts` make it absolute.
- **The converters' hash leaves the scripts out**: a script change would otherwise rebuild the 4-minute data tree.
  `open_data.json` records the scripts' own hash (`ai`), and `open_data.py ai TREE` refreshes only `Data/ai`.
- **Saving a Lua state means saving its memory, not the state.** A loaded game runs the modules from scratch
  (`LoadPersonality`) and restores the memory and the RNG (`RestoreMemory`), never `init`; everything else the next
  engine keeps between steps (the income ring, the scouting, the last step's results) must be in the save too, or the
  resumed game's first view differs. `EE_AI_SAVE_CHECK=1` proves it in EE-SIM, which cannot load a game: each save's
  bytes are decoded into a fresh state, re-encoded (same bytes) and restored into a shadow Lua state that runs the
  next step beside the live one (`next.savecheck step orders=same memory=same`).

## The next computer player's army (S4, docs/AI_LUA_PLAN.md §5 "S4 as built", 2026-11-08)

- **A strategy group from `CreateGroupFromStrategy` never filled.** MakePlan runs the UnitSelector (its own sizing,
  randomised domain shares), the sub-groups ask Personnel for classes, and Personnel fills a class slot only from its
  free pool or by ordering production, which is off for the next engine: S1's `defend` and `attack` groups held 0
  units. The next engine now keeps its own groups of named units (EECPNextArmy.cpp) and gives their goals itself.
- **Taking a soldier from the classic AI is `DetachUnitFromAI`**: out of Personnel and the area groups, and
  `mIsDetachedFromAI` keeps the drafting, the militia and the combat manager's counts off it. But
  `EEComputerPlayer::UnitGoalDone` re-attaches a detached unit as soon as its goal queue runs out (the original's
  "the player gave it an order" detach), so a group's soldier went back to an area group after its first fight; it
  now asks `EECPNextHoldsUnit` first.
- **Every soldier is already in a group.** Personnel drafts each new soldier into an area group of the attack
  commander, which walks it to a town, a choke point or a random point of the continent; a script that never names
  its soldiers leaves them there.
- **sim_bench `--env-vary` splits its values on commas**, so a per-player list like `EE_AI_NEXT_SCRIPT=x,rush,boom`
  cannot be a value: the variable takes `/` too.

## The next computer player's towers and the epochs (2026-11-08)

- **A computer player's towers upgrade by themselves.** The tower upgrades (`copper_tower_upgrade`,
  `bronze_wall_and_tower_upgrade`, ...) are dbtechtree kind-7 techs with `mResearchedWithEpoch`, and
  `EEComputerPlayer::EpochReached` -> `EnterEpoch` -> `EETechTreeEpoch::ResearchKind(7, true)` researches them
  free the moment the epoch is reached, for the classic and the next engine alike; their events convert every tower
  of the old class (dbeffects kind 19, `Fn1CACB4`) and obsolete its tech. Reading the scripts suggested the towers
  "stay Prehistoric all game" (AI_RESEARCH §8.31); a trace of the view's `buildings[]` each minute showed the
  opposite. Measure before fixing. The `research` verb answered such a tech `busy`; it is now `done` (n=0).
- **A research started at a building leaves `mEpochIDs`** (TryResearch hands it to the producer's queue), so the
  verb's busy check missed it and a second order queued it again; it now also looks at the producers' queues.
- **The later towers found no room round their sites** because only far or contested sites were left unguarded by
  then: the ring tiles out of sight (`EECanPlaceUnitOnTile` takes only tiles visible now), under trees or the mine,
  or with an armed enemy near. The classic FindTowerSite fails, too, once the centres nearest the town are guarded.
  `EECPNextBuild` now falls back to the rings round the town, spaced by the tower's range. A TEMP per-tile
  classification of the failed searches (which test failed first) found it in one run, where reading the code had
  not.
- **A data tree being rebuilt under EE-SIM** (another session's `open_data.py build`) fails the runs that stage it
  then (`EETerrainColorTable` verification failure at start-up, rc 0x80000003). For a long bench, hard-link a copy
  (`cp -al build/open_data/tree build/tw/tree`, seconds) and pass it with `--data`.

## The next computer player's citizens' losses (2026-11-08, docs/AI_RESEARCH.md §8.33)

- **Measure the deaths, not the guesses.** The suspects were remote raids, far hunting and slow defenders. A
  `next.death` line per citizen lost (killer, place, what it was doing at its first hit, foes and own soldiers near)
  showed the real causes in one bench: armies in the town, the militia fighting them, and no army. Animals and remote
  sites were minor.
- **The militia's odds count a tower's hit points as the citizens'.** The odds are (sum of hit points) x (sum of
  attack), so a tower's 1000-1450 hit points multiply the citizens' attack: 12 citizens and a tower against 8 rock
  throwers came out at 7.9 to 1. Yet running was worse: citizens run no faster than spearmen or horsemen, and the
  militia's 4-second re-decisions plus the 10-second gatherer pass sent 25-29 citizens back and forth through the
  attack.
- **"Reserve less what the soldiers cost" does not let them be made when the stock is below the reserve.**
  CanAfford asks stock - cost >= reserve, so a reserve of 1344 food against a stock of 743 blocked every soldier
  whatever was subtracted. The reserve has to be capped at stock - cost.
- **`EEUnit::GetGoal()` is `mGoalState`, not the goal the unit is carrying out.** A check of "is this enemy
  attacking my unit" through `dynamic_cast<EEUGAttackUnit*>(GetGoal())` never fired. `mActiveGoal` with
  `mKind == kGoalAttackUnit` does (the militia's own test).
- **The trace's `losses` line lacks braces** (EECPTrace.cpp EECPTraceUnitLost: an `if` without braces before
  several statements). `citizens_by_distance` and `citizens_by_killer` count every unit lost, projectiles included,
  and the `else` binds to the town-centre test. It is left as it is, because fixing it changes the classic traces;
  `next.death` is the reliable count.
- **EE-SIM game-to-game variance is large**: 1 to 224 citizens lost in 16 games of one setting. A 16-game bench
  resolves differences of about 15 losses a game. One change looked 14 worse on 16 games and was neutral on 32.
- **A fishing boat sees 4 tiles; a frigate shoots from 5.** A boat that flees "on sight" is already under fire. The
  docks and towers see further, which is what the fish finder's warship check uses.

## The next computer player's resource sites (2026-11-08, docs/AI_RESEARCH.md §8.34)

- **A worked mine has 0 hit points.** `EEUGGather::Work` hits any resource with hit points not 0 (meant for hunted
  animals). A mine has 1 hit point in the data, and `EEUnit::TakeHit` sets a resource's hit points to 0 instead of
  killing it. So a mine is at 0 from its first gather and stays gatherable while its amount (300000) lasts. Code
  that asks `mHitPoints > 0` of a resource loses every worked mine and patch. The next engine's view did that, and
  the playtest's only safe iron mine vanished the minute its gatherers reached it. Test resources by
  `mResourceAmount > 0`, as `EECPResourceCenter::FindResourceUnit` does. The classic `cp sites` trace,
  EECPRMSettlePatches and claim_needed still use the hit point test (left alone: changing them changes the classic
  traces).
- **`HasEnemyNear` is neither a threat test nor fog-honest.** It sums the influence regions (9 tiles each) round
  the tile, then takes every unit in the overlapping unit regions (up to 27 tiles across): every armed unit not its
  own, citizens (attack 5) and towers whatever their range, seen or not. 77% of the next engine's blocked
  site-minutes were enemy citizens, 91% of all the blockers were out of sight. Its collection is in pointer order;
  pick a cause by distance and unit id for a deterministic trace.
- **A trace of who blocked what** (`EE_AI_EXP sitetrace`) settled in one bench what the playtest log could only
  suggest. Build the before image with the trace commit alone and keep it apart (`build/trace_exe`). An edit
  made while build.py runs can mix two header versions in one image, so stash or patch the work aside while it
  builds.

## Unit improvements and the next computer player's fifth playtest (2026-11-09, docs/AI_RESEARCH.md §8.35)

- **No unit improvement could be bought on 64-bit Windows.** `EEUnitClass` takes its improvement table by
  `reinterpret_cast<EEUnitClassImprovementTable*>(EEUCUpgradeTable::sTable[i])`. EEUCUpgradeTable has a virtual
  destructor, so its data starts after the vtable pointer (8 bytes on x64); the reading struct began with a `long`
  (4 bytes on Windows). Every entry was read 4 bytes off (a cost factor from the previous step, a step from the
  cost) and the maximum level came from a step factor, 0, so `EEPlayer::CanBuy` refused every level (the
  next engine's new `improve` verb found it: `level=1 max=0`). On LP64 macOS/Linux `long` is 8 bytes and the layout
  happened to match. The first member is now `void*`, and a `static_assert` in EEUnitClass.cpp pins the shared
  offsets. The classic AI's attack groups (`GiveImprovements`) buy improvements again on Windows, so its traces
  differ from the images before.
- **`gWorld.GetResourceUsers` counts the gatherers working a resource, not those sent to it.** The finders tested
  it against the mining limit, so a patch of 6 drew 9 or more citizens: the walkers and those waiting were never
  counted, and hunters whose game ran out walked to the nearest bushes on their own. Count the gather goals that
  name the unit (`EECPNextAssigned`) and shed the extras.
- **A tree's tile is on no land continent.** A placement search round a forest tile that asks the tile's own
  continent (`GetContinent(x, y, kAIPathLandMass)`) refuses every site: take the town's continent.
- **Docks from `FindShoreSite`** use the shore of the largest water body nearest the town, which can be 60 tiles out
  while the town's own fish lie in other water by it.

## The next computer player's settlement-first rule and the binary tree (2026-11-09, docs/AI_RESEARCH.md §8.36)

- **A binary data tree has no row ids.** The Lua AIs name classes and techs by the text tables' `id`; on the binary
  tree (`.dat` only) every class was `dbobjects_<n>`, so boom found no `citizen`, released every site and never
  trained, and `research copper_tower_upgrade` was refused as unknown. The gates' AI games play classic only, so
  nothing caught it. The binary tree now carries `Data/db/dbobjects.ids` / `dbtechtree.ids` (`key id` lines from
  `ids.generate`, the same ids the converter gives the text rows); `EEDbFindFile` reads them with the `.dat`. The
  boom traces of both trees are identical (seeds 26-27, 40 minutes).
- **`EETile::IsVisibleTo` is "in sight now" (a count of the units seeing it), not "explored".** A home patch 7 tiles
  from the town centre read as unexplored all game, and the finders (which test it) never sent anyone there. Building
  placement (`EECanPlaceUnitOnTile`) tests it per footprint tile too: a settlement at a site nobody stands by went 5-6
  tiles off, or nowhere. The next engine sends a walker and keeps it standing there until the settlement is placed,
  and a resource by the player's own finished drop site skips the visibility test.
- **A resource centre can hold gold and iron 6 tiles apart.** A per-site `has_drop` from its first unit said the iron
  had a drop site; per-resource `drop`/`ready` in the view, and a settlement placed by the units that have none.

## R1.3: PlatFile, PlatThread and PlatSocket in Rust (rust/crates/ee_plat_sys, 2026-11-08)

- **One crate, one function, cfg per OS**: `file.rs` / `thread.rs` / `socket.rs` hold the shared tests and pick
  `*_win32.rs` or `*_posix.rs` with `#[path]`. `src/Platform/{Win32,Posix}/Plat{File,Thread,Socket}.cpp` are gone;
  `tools/units.py` and `xbuild.py` link them from `ee_rust` (`xbuild.rust_libs`). `tests/platform`, `tests/won` and
  `tools/tcp_test` are unchanged and link against Rust.
- **The harness sees the same imports.** windows-sys's `link!` is an ordinary import through the final link's
  `kernel32.lib`, so `plat_test.exe` shows `WaitForSingleObject` / `WaitForMultipleObjects` / `EnterCriticalSection` /
  `CreateFileA` under `KERNEL32.dll` (the time shim patches by DLL and name; checked with `llvm-readobj --coff-imports`).
  `PlatMutex` stays a heap `CRITICAL_SECTION` at offset 0 (the harness casts the pointer), a `PlatEvent*`/`PlatThread*`/
  `PlatFile*` stays the HANDLE, `FIONBIO` the Winsock number.
- **windows-sys is a second version, on purpose.** The wgpu tree has 0.59 (through winapi-util/termcolor into naga);
  adding Win32 features to 0.59 recompiles naga, wgpu-core and wgpu-hal in both profiles. `ee_plat_sys` uses 0.60 with
  its own feature list: two small crates, no rebuild of the graphics stack.
- **The POSIX C++ that remains reaches the last error through Rust.** `PlatGetLastError` is Rust; `PlatPosix.h` wraps
  `PlatPosixSetLastError` / `PlatPosixErrorFromErrno` (exported by `last_error.rs`) so `PlatPath.cpp` and `PlatTime.cpp`
  keep their `PlatPosix::Fail` calls. Rust reaches the other way through `PlatPathToNative` / `PlatPathFromNative` only
  (`native_path.rs`), the extern seam with a `cfg(test)` stand-in, so `cargo test` needs no PlatPath.
- **windows-sys's `SOMAXCONN` is Winsock 1's 5**, not winsock2.h's `0x7fffffff`. `PLAT_SOCKET_MAX_BACKLOG` (`0x7fffffff`)
  is passed through unchanged as the C++ did; do not compare against that constant.
- **`cargo test` of Windows timing:** a 60 ms `WaitForSingleObject` can return a timer tick (15.6 ms) early, so timed
  checks allow 40 ms, as `plat_test` does.
- **A target directory shared between worktrees** gives every worktree the same `xtask` fingerprint (the package id is
  relative to the workspace), so `CARGO_MANIFEST_DIR` is baked from whichever worktree compiled it last and
  `xtask cbindgen` then reads that worktree. The Rust gate's header step needs its own target directory or a single
  worktree per target dir.

## The platform leaves in Rust (R1.1, R1.2; crate ee_plat_core, 2026-11-08)

- **Windows keeps the C runtime and the Win32 calls**: `PlatStrICmp`/`_mbs*`/`_itoa`/`_strdup` stay the CRT's, because
  `setlocale` (EEUserInterface) and the process's ANSI code page change `_stricmp`, `_mbsicmp` and the lead bytes at
  run time; a reimplementation would have to track both. POSIX is the single-byte port (`string::sbcs`). PlatTime
  calls the same imports as before, checked in the built exe with `llvm-readobj --coff-imports` (WINMM five, KERNEL32
  `QueryPerformance*`/`Sleep`/`GetTickCount64`/`GetLocalTime`/`GetSystemTimeAsFileTime`), so the harness shim still
  works. PlatConfig reads and writes through the CRT's `fopen` family, not `std::fs`: its paths are the game's bytes.
- **PlatSim's variables are `#[no_mangle] static CVar<T>`**: a transparent `UnsafeCell`, readable as `read_volatile`.
  `gPlatSimNoHumanDefeatHits` is `c_long` (32-bit on Windows, 64 on LP64), as the header says.
- **Golden values come from running the deleted C++**: `gen_strings.cpp` (built against the POSIX and the Win32
  PlatString.cpp, the latter under `_setmbcp(1252)` and `932`) and `gen_config.cpp` (`#include "PlatConfig.cpp"`) wrote
  `golden_*.txt` in `rust/crates/ee_plat_core/src`. A lone lead byte at the end of a source makes `_mbsnbcat` fail its
  parameter check (a process-terminating invalid-parameter fast fail) under 932; those rows are skipped.
- **strtoll("0x")**: glibc stops after the 0, the UCRT reports no conversion, and neither is a whole-string conversion.
  `PlatConfigGetInt` only keeps whole-string conversions, so the Rust strtoll matches both.
- **A config value `~` is rendered `key: ` and reads back as a block** on the next start (kept from the C++; the value
  was empty anyway).
- **Shared `CARGO_TARGET_DIR` across worktrees**: `rust_build.py gate`'s header step ran another worktree's `xtask`
  (its `include/rust/ee_rust.h` is reported) when two checkouts share the target directory; run it with a private
  target directory to check this checkout's header.
- **The POSIX C++ still here** (`PlatFile.cpp`, `PlatThread.cpp`, `PlatSocket.cpp`) reaches the moved code through
  `src/Platform/Posix/PlatPosixBridge.cpp` (`PlatPosix::ResolvePath`/`GameForm`, and the two C functions Rust calls to
  set the thread's last error), which goes away with R1.3.

## The window and the fonts in Rust (R1.4, R1.5; crates ee_plat_ui, ee_plat_sdl, 2026-11-08)

- **Two crates, on purpose.** The window layer imports SDL, the fonts and PlatCp1252ToWide do not. `ee_rust.lib` is
  one archive, and the linker pulls whole object files; rustc's codegen units cut across modules, so with one crate a
  program that only needs text (`plat_test`, `font_test`, the WON tests: no SDL on their link line) could pull in an
  object that references SDL. `ee_plat_ui` (logic, fonts, tests) has no SDL symbols; `ee_plat_sdl` (the imports and the
  PlatWindow exports, compiled out of `cargo test`) depends on it. POSIX links scan archives once, so the Rust library
  now comes *before* SDL on the link line (`sdl_libs` after `wlib`).
- **No `sdl3-sys`.** The imports are hand-written (each checked against the pinned 3.4.18 headers) and the
  layouts (`SDL_Event`, `SDL_DisplayMode`, `PlatInputEvent`, ...) are tested against the C compiler's `offsetof` from
  a generator run (`golden_window.txt`). No binding crate means no second SDL build and no pkg-config step on
  every `cargo test`.
- **The rasteriser is a port of stb_truetype, not fontdue or ab_glyph.** `ttf.rs` is v1.26's cmap, glyf, composite and
  rasteriser-v2 code line by line (same float order, same casts, same quicksort: the order of equal edges feeds the
  float sums). Result: byte-equal coverage on the Liberation fonts (5 faces x 23 sizes x bold, 51,520 glyphs against a
  dump of the C++ font code; rerun with the ignored test `liberation_fonts_against_a_cpp_dump`) and on the synthetic
  `golden_font.ttf` (simple and composite glyphs, on- and off-curve starts). CFF/OTF outlines are not ported (no font
  the game registers has them): `PlatFontRegister` refuses such a font. C reads past a malformed font's tables were
  undefined; the port reads 0.
- **Golden values come from the real C++ text**: `PlatFont.cpp` built as is, `PlatWindow.cpp` sliced (key table,
  `Translate`, the key-state functions) and run against stub SDL functions in a throwaway generator
  (`golden_font.txt`, `golden_window.txt`).
- **The icon crosses the language boundary through a constructor.** `PlatIconData.h` is generated for the POSIX game
  only, and Rust cannot take a weak symbol on stable, so `src/Platform/SDL/PlatWindowIcon.c` hands the pixels to
  `PlatWindowSetIcon` before `main` (compiled into the POSIX game; no program without the header).

## Rust R3: compression and UEncryption (2026-11-08)

- **cbindgen only exports functions from crates named in `[parse] extra_bindings`.** `parse_deps`/`include` make it
  parse a dependency, and the "Take ... opaque" lines look like it worked, but functions outside the binding crate
  are dropped silently (`should_generate_top_level_item`). Each crate with `#[no_mangle]` functions goes into both
  `include` and `extra_bindings` in `rust/cbindgen.toml`.
- **UEncryption's output depends on the `UString` buffer.** The padding `+=` never grows the string, so a string in an
  exact-fit buffer (`UString("hi")`) stays unpadded and its decryption's checksum lookup misses (the checksum block is
  read at padded length * 2 + 16). `UEDecryptUString` caps its text at 255 characters (a default-capacity `UString`).
  Both quirks are kept (the forwarders pass the capacity); golden vectors pin them. A block with no hex digits read
  uninitialised memory in C++; Rust reads 0, and scanning past the end of the input stops there.
- **The DCL encoder and decoder are now Rust (`ee_codec`).** `Compress` of empty input gives a 4-byte stream, which
  `explode` refuses (first read <= 4 bytes): the C++ quirk is preserved and a test pins it.

## Rust R3: the TCP/IP plug-in (2026-11-08)

- **`extern "C"` declarations in a dependency reach the header.** cbindgen copied the `extern "C"` block of
  `ffi.rs` (the `PlatSocket*` imports) into `ee_rust.h`, which then conflicted with `platform/PlatSocket.h`, and it
  exported every `pub const` of the crate as a C++ constant, colliding with Winsock macros (`SOCK_STREAM`, `AF_INET`).
  Foreign blocks and internal modules carry `/// cbindgen:ignore`; only the entry points and the sink typedefs are in
  the header. The plug-in's logic is generic over a `Host` trait (the real one is `ffi.rs`, compiled out of
  `cargo test`, so tests link without the platform layer).
- **`tcp_test` shares its ports with the game** (33334/33335). `EE_TCP_TEST_PORT_OFFSET` moves it (through
  `gPlatSimPortOffset`) so it can run beside a live game or harness.
- **Behaviour changes in the port**: the cipher loop has no 16-bit counter (by reading the C++, its 16-bit counter never ended the loop for datagrams above 32767 bytes;
  not exercised), `strcpy` into packet fields is bounded, `connect`'s failed-resolve socket and the search's sockets on
  failure are closed. Everything else, including the search thread's timing and the quirky result codes (`13`, a
  socket-open failure in `SendSingle` being `OK`) is carried over.

## Rust R3: the mixer plug-in (2026-11-08)

- **The mixer's math moved; the device and decoder stayed C++.** miniaudio's `ma_device`/`ma_decoder` structs are
  version-specific and large, so `Mixer.cpp` keeps the miniaudio calls and `SESample` access; `ee_mixer` owns the
  channels, the interpolation and the volumes. Golden values come from the real text of the former `Mixer.cpp`
  (sliced into a throwaway tool), not a rewrite.
- **Audio sources have their own lock.** C++ held the channel mutex (recursive `PlatMutex`) across the conductor's and
  the movie's render callback; Rust's `Mutex` is not reentrant, so the sources sit behind a second lock the device
  thread holds while calling them. `SetMusicSource(NULL)` still waits until the device thread is out of the old
  source, which the conductor and `DX7Movie` rely on before freeing it.
- **cbindgen writes `Option<alias>` literally** when the alias is a function pointer; a callback parameter must be
  spelled inline as `Option<unsafe extern "C" fn(...)>` to become a nullable C function pointer.

## R2: the data layer in Rust (ee_data, 2026-11-08)

- **`std::from_chars` and `f32::from_str` agree.** The text compiler's floats moved to `f32::from_str` (correctly
  rounded); `cppgate` compared C++ and Rust on a 12,074-string sweep (tables' float cells, subnormals, 7e-46 / 7.1e-46
  halfway cases, exact-tie decimals) with identical bits and messages; from_chars' `result_out_of_range` and Rust's
  inf/0 land in the same message through the C++'s own post-checks.
- **`strchr` matches the NUL.** The C++ YAML subset's `strchr(" [,:-", s[i - 1])` and the plain-scalar check treat a
  NUL byte as "in the set" (the terminator). The port keeps that (`strchr_set`), byte-level parity needs it.
- **cbindgen writes a dependency's functions only from loose sources.** `parse_deps` follows the crate tree but
  functions from any crate other than the binding crate are skipped ("not used"); xtask adds
  `rust/crates/ee_data/src/ffi.rs` with `with_src` to get them into `include/rust/ee_rust.h`.
- **A shared `CARGO_TARGET_DIR` hides worktrees' `cargo run` binaries.** Cargo's uplifted `target/debug/xtask.exe` is
  whichever worktree linked it last; `env!("CARGO_MANIFEST_DIR")` then pointed at another worktree (`cbindgen` wrote the
  r1-core header once from the r2-data worktree). Touch the crate or run with a private target dir before relying on it.
- **Generated headers next to a staged unit must end in `.h`.** `verify.stage_source` copies a unit's sibling `*.h`
  to `C:\EEX\` (or `EE_ORIG_SRC_ROOT`); a generated `.inl` was not found. Hence `EERulesGenDecode.h`.
- **Rules live in Rust; the game keeps a C++ copy.** `EERules` holds a Rust handle (`ee_data_rules_*`); every rules
  field, default and dump key is generated from `tools/eedata/rules_schema.json` (`rulesgen.py`); C++ reads a binary
  snapshot after each change (`Refresh`). The dump formats `(long)float_bits` as signed like the Windows 32-bit `long`
  (gate pins it); Rust's `RuleNumber::constant` truncates as x86's `cvttss2si` does (INT_MIN out of range).
- **cppgate after the C++ compiler is gone** compares against the Python importer (independent) and against digests
  recorded from the C++ compiler (`tools/eedata/cppgate/golden_cpp.json`), which apply while the data's texts are the
  recorded ones. A data change requires re-recording against an independent reference, not the Rust compiler itself.
- **R2 step 4 (GEOpenFormats in Rust):** done, crate `ee_openformats`; see the section "R2.4" below. The port had
  waited for the byte gate to fit on the disk (`eemodel.py cppgate` now streams in bounded batches).

## A shared CARGO_TARGET_DIR (R0-R3)

Worktrees must not share a cargo target directory. Cargo hashes a path crate by its path relative to the workspace
root, so the same crate in two worktrees gets one artifact name, and a checkout can then run or link what another
checkout compiled: `xtask` checked another worktree's header, and a test run reported failures in code that was not the
checkout's. `tools/rust_build.py` always uses this checkout's `build/rust`.

The gate's PT at the time cap is timing, not game state: `main` (9cc83d51) gave PT 13,986 and 17,633 in two runs on one
machine. The seeded EE-SIM trace is the check that a change left the simulation alone.

# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later (the WON library: "The WON library from
source").

Order that worked: compile census → STL headers → stock-header patches → mechanical source passes → header bridges →
one-off sources → DLL links → exe link → stub audit against VC6.

## The random map generator in Rust (R3.4)

`rust/crates/ee_rmap` is the generator (`Random Map Generators/Default`, minus the `.rmv` reader). The `rmap` PIM stays
in C++: `Default.cpp` copies `EERandomMap` into flat arrays (`EeRmapState`), runs one step in Rust and copies the result
back, per step, because the game mutates the state between steps (water points, smoothing, feature claims). The grids
are copied in and out of Rust at every step; the cost is small next to the generation.

- **The `.rmv` reader is `Unit10.cpp`, not `EERandomMap.cpp`** (RUST_PLAN's R3.4 text is off). It stays in C++: it is
  a text parser over `UString` and `std::map`, and its script-language semantics (`%f`-style number parsing through
  `sprintf`/`sscanf`, `#if` evaluation over tokens) would need a gate of their own. `SetRandomMapValues` calls the
  adapter's `ResetPlayerOrder`, now in `Default.cpp`.
- **The byte gate.** `python -I tools/rmap_gate/rmap_gate.py check` runs 2088 maps (12 scripts x 6 sizes x player counts x
  teams x 6 seeds) and compares digests of every `EERandomMap` output (floats by bits) with `golden_cpp.txt`, recorded
  from the C++ generator before it was deleted. `diff "Script|size|players|mode|seed"` prints the digest after every
  step to find a divergence. The tool links the real `Unit10.cpp` and engine math (`UString`, `UMath`, `GE2DPoint`,
  `GETransformation`, `U2DFractal`...) with stubs for the platform layer (`/FORCE:UNRESOLVED` for code it never calls)
  and the generator through `ee_rmap_gatelib`, a tiny staticlib (`ee_rust` needs wgpu, far too big for a helper).
  - The game-side passes between the PIM steps are approximated by the tool (`rmap_dump.cpp`: the diamond map shape,
    water points, smoothing, feature claims): the gate needs both generators to see the same input, not the game's.
  - Mode `forced` turns on the filter pass and the shallows, which no shipped script enables, so that code is gated.
- **`UMCos`/`UMArcCos` are lookup tables, and the tables are not a formula.** `UMTables.h` holds the `%f` spelling of
  an `f32` cosine. Rust recomputes them the same way: `cos(f32(i * pi32 / 16384))`, formatted `{:.6}`, parsed back as
  `f32` — and one entry differs from the formula: `cos(pi/2)` is a tiny negative that the table spells `0.000000`, a
  plus zero (`-0.000000` parses to minus zero). `math::tables_digest` pins all 49,154 entries to the C++.
- **`lrintf` has an "indefinite" value.** `round_ties_even` + `as i32` saturates; the C++ conversion gives `0x80000000`
  for NaN and out-of-range values (a neutral share over zero neutrals is NaN). `ee_rmap::math::lrintf` keeps that
  value (`ee_random::float_to_long` does not; it was not needed there).
- **`unsigned long` is `u32` in the Rust**, on every target. The C++ on Linux/macOS had a 64-bit `unsigned long` here
  (LP64). The Rust matches the Windows C++, which is what the gate recorded; on POSIX it may differ from the old POSIX
  C++ where a value wraps (no POSIX C++ dump was recorded or compared).
- **`EERMObject::mType` is uninitialized on every kind but trees** in the C++ (a stack record), so the digest skips it
  there; the game reads it only for trees.
- **All-valid test grids crash the C++.** A point off the map passes `AreAllMasked`'s clamped scan when every cell is
  valid, and then indexes `mHeights` out of range. The real maps' diamond corners are invalid, so this never happens
  in a game; `ee_rmap`'s golden test uses a one-tile invalid border as a real map has. A Rust index there would panic.

# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later (the WON library: "The WON library from
source").

Order that worked: compile census → STL headers → stock-header patches → mechanical source passes → header bridges →
one-off sources → DLL links → exe link → stub audit against VC6.

## GX in Rust (R4.1)

GX is `rust/crates/ee_gx` on the `wgpu` crate at tag v24.0.3 (the version wgpu-native v24.0.3.1 was built from; later
ones broke the read-backs, see above). `include/gx/GX.h` keeps its classes as C++ shims with no members: a `GXSurface*`
is a pointer to a small Rust handle naming a slot in GX's table, `GXDevice*`/`GXVertexBuffer*`/`GXDirectDraw*` are
Rust boxes, each freed by its last `Release`. The harness's `GXTestSetPresentHook`/`GXTestReadFrame` are `#[no_mangle]`
Rust functions with the same names; `GX.cpp` keeps the `/include` pragmas that hold them in the image.

- **The wgpu crate's defaults differ from wgpu-native's in ways that show.** `Device`'s default error handler panics
  (wgpu-native's callback logged): `on_uncaptured_error` routes to `GXLog`, set before anything else on the device.
  `Instance::new` must get explicit flags: the `wgpu` crate defaults to `from_build_config` (debug layers in a debug
  build), `InstanceFlags::empty()` is what wgpu-native gave a release build. `EE_WGPU_FLAGS` still selects debug and
  validation. Limits are wgpu-native's `get_base_device_limits_from_adapter_limits`, ported.
- **The `wgpu` crate forces wgpu-core's `indirect-validation` feature** on native (wgpu-native did not): device
  creation builds its compute pipeline. No draw uses it and it adds one shader compile at start; on WARP/FXC it
  works (gx_test, the device tests).
- **The open render pass spans API calls**: `begin_render_pass(...).forget_lifetime()` gives a `RenderPass<'static>`;
  dropping it ends the pass, which must happen before `encoder.finish()`.
- **One reentrant lock, and no borrow across a callback.** GX's state is a `RefCell` inside a `parking_lot::ReentrantMutex`
  (the C++'s `recursive_mutex`). A callback into C++ must not run with the `RefCell` borrowed, because it can call back
  (the bridge's present hook calls `GXTestReadFrame`): `Blt`/`Flip` return a `PresentRequest` that the FFI layer runs
  after the hook, and `DrawText` is two calls around its callback. The enumerations (`EnumDisplayModes`, ...) and the
  display settings take no lock at all, as in the C++ (the settings are atomics).
- **Everything GPU-shaped goes through a `Backend` trait** (`gx.rs`): the CPU/GPU bookkeeping, the pass and ring
  tracking, the blits and presentation are tested against a recording fake with golden values generated from the
  C++ before it was deleted; `gpu.rs` is the wgpu `Backend` and has device tests (a fill, a keyed blit, a textured draw,
  a scaled target's read-back) that skip when no adapter is reachable.
- `tools/sdl_test` now goes through GX (the swap chain is ee_gx's); `tools/wgpu_probe` (the phase-0 probe of the C API)
  is deleted and wgpu-native is gone from the workspace, so the Rust build no longer needs libclang or bindgen.

## The File System in Rust (R3.5, ee_fs)

- **FSMemoryFile's quirks are the golden file.** Reading ignores `mIsOpen`, writing checks it; `Compress` closes the file
  on success but `Decompress` of plain data leaves it open; `Compact` rewinds even when it shrinks nothing; a failed
  `Load` empties and closes. A scripted interpreter (`memfile.rs` tests, the same op language dumped once by a C++
  driver from the real class) pins them; the golden driver, re-linked against the facades, reproduced its own output
  byte for byte.
- **`mIsOpen` of a memory file stays false in the C++ object**: the state lives in Rust and `FSFileSpec::~FSFileSpec`
  must not call `FSFileSpec::Close` on a handle it does not have. The facade's destructor makes that true.
- **`FSMemoryFile(inData, inLength, ...)` now copies** the caller's `new[]` buffer into Rust's and frees it at once: the
  only callers hand their buffer over (`EEDataTextFile`, `GEOpenFormatsFile`), and the one copy is short against the
  conversion that made the bytes.
- **FSChunk's state is an embedded POD** (`EeFsChunk`, cbindgen emits the struct, `FSChunk.h` includes `rust/ee_rust.h`):
  an implicit `FSChunk` copy (a `std::vector<FSChunk>` needs one to exist) still copies the fields, not a Rust pointer.
- **The FAT and tag maps are BTreeMaps** with the C++ orders: FAT keys byte order (strcmp, unsigned), tags through
  `PlatMbsCmp`; `std::map::insert` becomes `entry().or_insert` (an existing name keeps its entry).
- **`ReadFAT` counts the name's `strlen + 5`, not the stored length**, and an empty name consumes one byte too many of
  the table; kept, because the table's size bound is the only thing that ends the loop.
- **`FSArchive`'s implicit copy shared the mapped view**; the explicit copy constructor clones the Rust tables and keeps
  sharing the view (only closed archives are copied: `FSPath::AddArchive` closes them first).
- **`FSMemoryFile::Save(file, n)` past the buffer**: C++ read out of bounds; Rust clamps. Likewise a chunk saved without
  data reports `kErrorNotOpen` instead of crashing, and an archive entry outside the view fails with an empty file.

## R4.2: the DX7 plug-in in Rust, first steps (ee_dx7, 2026-11-09)

The plug-in moves file by file behind trampolines (`Common/DX7Trampolines.cpp`, `tools/units.py` `DX7_TRAMPOLINES`).
So far: the vertex arrays and polygon array (whole files), the rasterizer's redundant-state setters and SetTexture's
stage diff, the Copy16/32BitWithFilter loops. Everything else stays C++ for now (rasterizer activation and the
scene/VB/clear loops, the Draw* units, DX7Screen, the rest of DX7Surface, DX7Movie, DX7TnLRasterizer, the plug-in
entry).

- **`ee_dx7` does not depend on `ee_gx`.** It declares the GX functions it uses `extern "C"` (`src/gx.rs`, `mod sys`,
  gated out of `cargo test`), so `cargo test -p ee_dx7` needs no wgpu build (3 GB of disk; the crate tests in seconds).
  Logic is tested through a `Device` trait with a recorder; the raw handle `GXDevice*` is the same opaque pointer as the
  C++ `GX.h` shim's `this`.
- **DX7RScreenState is a pinned record.** The C++ draw code (DrawModel, DrawTerrain, Draw2D) still reads
  `mVertexBuffer`, `mVertexBufferPosition`, `mViewportChanged`, ... off it, so the record stays C++'s; the setters write
  through a `#[repr(C)]` mirror whose offsets the trampolines `static_assert` (field by field, `c_ulong` for
  `unsigned long`). Only the DX7-side caches go to Rust. The GE-side caches (GERScreenState) stay C++ and arrive as
  arguments (`current`); a setter returns whether the C++ base class must record.
- **Which side reads or writes a C++ object** decides the seam. GETexture, GELight, GESurface stay C++: SetTexture's
  reference counting, `Promote` and `ValidateChanges` are C++, the stage diff gets a plain `StageView`. The pixel
  format helpers (GEUnpack*/GEPackColor) stay in the Graphics Engine and are reached through the `PixelOps` seam
  (`ee_dx7cpp_*`), not re-implemented.
- **`float_to_long` is `UMFloatToLong`** (`lrintf`, ties to even), not `as i32`; a NaN or out-of-range float gives
  `i32::MIN` as the x87 did. Golden values and the filter loop's output come from scratch C++ generators compiled with
  clang-cl (the same scratch loop as the shipped C++).
- **cbindgen:** an `extern "C"` block inside `/// cbindgen:ignore` must be in a `mod sys` (a doc comment on the `extern`
  block itself draws `unused_doc_comments`, and without it the imports appear in `ee_rust.h`). A `pub const` in an
  included crate is exported too: internal constants are `pub(crate)` (a `GX_OK` export collided with `GX.h`'s macro).
- **Line endings:** this repository stores files byte for byte (`* -text`); Windows Python text mode (`open` without
  `newline=''`) turned LF files to CRLF and produced whole-file diffs. Patch scripts open with `newline=''`.

### The rasterizer's draw paths (R4.2)

`DX7Rasterizer Draw2D/DrawModel/DrawTerrain.cpp` moved to Rust (`src/draw2d.rs`, `draw_model.rs`, `draw_terrain.rs`);
the C++ members forward, `Common/DX7DrawSeam.cpp` (new unit) and the `ee_dx7cpp_*` functions in the three units are the
way back into C++ objects.

- **The seam is per-object, not per-field.** `GEModel`/`GEMaterial`/`GEViewport`/`GETerrainMesh` cross as plain records
  built by C++ (`ModelInfo`, `MaterialInfo`, `ViewportInfo`, `TerrainMeshView`, `MaterialView`); the visible vertex
  arrays (`GETM_VISIBLE_POINTS`^2) and `GETMPolygon` are pinned records (`static_assert`s in the Draw unit); per-point
  reads (height, normal, colour) stay `ee_dx7cpp_mesh_*`. No Rust reference lives across a host call: vertex memory is
  written through raw pointers between calls and `ScreenState` is only touched in the `Host`.
- **The transform stack became a Rust parameter.** `GERasterizer::sTransformStack` was only used by
  `DrawModelHierarchy`; `compose` (a port of `GETransformation::Apply(transformation)`) replaces it.
- **`sWaterTime` stays a C++ static** passed by pointer: Rust owns the arithmetic, C++ the storage. Its tests pin the
  clock as a parameter instead of a static.
- **The C++ `sin` overload is float.** `TransformToCamera`'s wave calls `sin` on a float expression: with the MSVC STL
  headers it resolves to `float sin(float)` (`sinf`), and Rust's `f32::sin` matched bit-for-bit in the goldens. Don't
  "fix" it to `f64` or to a `libm` crate.
- **The goldens** (`golden_draw.txt`, 40+ sections) come from a scratch program that `#include`s the original C++ bodies
  unchanged against fake engine objects (headers copied with `virtual`/`= 0` stripped so a `calloc`ed rasterizer needs no
  vtable; `private`/`protected` opened with macros; `UMath.cpp` real). Every test rebuilds the same inputs in Rust and
  compares the event log line for line (device calls, vertex bits, the `WASSTILLDRAWING` retry, the VB restart at 0x960).
- **Protected rasterizer members** are reached by `struct DX7DrawAccess : DX7Rasterizer { using ...; }`, a view of the
  same object (no data of its own), so `DX7Rasterizer.h` needs no friend declarations.

### R4.2, the second half: surface, movie, T&L rasterizer, screen, rasterizer

`DX7Surface.cpp`, `DX7Movie.cpp`, `DX7TnLRasterizer.cpp`, `DX7Screen.cpp` and the scene, clear, vertex-buffer, light,
2D/3D and presentation parts of `DX7Rasterizer.cpp` are forwarders into `ee_dx7` (surface.rs, movie.rs, raster.rs,
screen.rs). `DX7HRTnLDisplay.cpp` is the plug-in entry (the PIM table, a validity probe of three lines) and stays C++.

- **The golden generators compile the real `.cpp` against a fake of `GX.h`.** A scratch TU `#include`s the real
  DX7Surface/Screen/Rasterizer/TnLRasterizer unit and defines every `GX*` method (`GXSurface::Blt`, ...) as a line of
  trace plus a result from a queue (`gx_fake.inc`); the Rust tests' recorder prints the same lines and the golden files
  are those traces (`golden_surface.txt`, `golden_screen.txt`, `golden_raster.txt`). The engine classes the units touch
  are stubbed only as far as the code under test needs (the `GERasterizer` ctor, `UString`, a fake vtable slot for
  `GetIsFullScreen`, `RoundToNearest`, `Apply`); `/FORCE:UNRESOLVED` keeps the link for the code the case does not run.
  Floats print as hex bits, so an exact bit difference fails the test. Bash heredocs with `'` in the body do not survive
  the tool's quoting on Windows: write patch scripts and generators with the editor instead.
- **`(long)float` vs `lrintf`.** Rust `as i32` saturates; the C++'s `cvttss2si` gives `INT_MIN` for NaN and out-of-range,
  `trunc_to_long` is that (`DX7Surface`'s pixel rectangles). `UMFloor` and `UMFloorUnsigned` are `lrintf` then a decrement
  when the rounded value is above the input, compared as a float; `GXRGB`'s `(unsigned long)(c * 255)` is a 64-bit
  truncation (`as i64 as u32`).
- **What `GX.cpp` answers itself:** `RestoreAllSurfaces`, `TestCooperativeLevel`, `GXDevice::EndScene`,
  `GXVertexBuffer::Unlock`, `GXSurface::Restore` and `ValidateDevice` are constants; `RawGx` answers the same without a
  `gx_*` call, and passes `NULL` or drops what the shim drops (`SetDisplayMode`'s rate and flags, the vertex buffer's
  lock flags, the device kind of `CreateDevice`). The golden generators fake the methods above the shim, where those
  arguments are still visible; the tests pin them, `RawGx` must then drop them.
- **Rule: no Rust reference across a call into C++.** The FFI layers copy the pinned `DX7RScreenState` and the
  descriptions in and out around the algorithm (they hold `&mut` only over GX, which is Rust), and hand the
  movie its file name as an owned `Vec`.
- **`gLightsInUse` stays `unsigned long`** (the DrawTerrain unit reads it; its mangled name must not change, and the
  32/64-bit `unsigned long` means `c_ulong` in Rust, never `u32`).
- **Files with CRLF:** `DX7Movie.cpp` is CRLF and `Common/*.cpp` next to it LF (the repository stores bytes as they
  are): a replace script working on LF markers silently misses a CRLF file; check `"\r\n" in text` first.
- **The DX7Movie thread stays a C++ `TSThread`**, with the trampolines as the seam and the pl_mpeg, event and critical
  section objects C++; Rust owns the sound ring, which the mixer's device thread and the decoder share under C++'s
  `mSoundAccess`.

## R2.4: the open formats in Rust (crate ee_openformats, 2026-11-09)

- **Hand ports of cgltf and stb_image, not the crates:** `cgltf.rs` walks jsmn's token stream key by key (`jsmn.rs` is
  a literal port), `png.rs` is stb_image's inflate/chunk walk, because which edited file loads or refuses is the
  behaviour under test. cgltf's C arithmetic is kept on purpose: `cgltf_json_to_int` of a non-number is -1, which a
  pointer-index treats as absent (`(cgltf_size)-1 + 1 == 0`) while -2 becomes a huge reference that fixup refuses.
  `atoi`/`atoll`/`atof` are the Windows CRT's prefix rules (saturating integers, hex floats, inf/nan). Material
  and node extension objects keep the raw token range, and "extras" a string-typed value's text without the quotes.
- **The byte gate cannot see the parser's configuration:** the first port read jsmn as non-strict and without parent
  links (the code's `#ifdef` branches, the defines are 900 lines above) and still passed all 7150 cases, valid and
  refused, because well-formed JSON tokenizes the same either way. A differential run of mutated files (C++ exe
  against Rust, one process per case) found it within 600 mutants: `"count":"componentType":,` makes the comma return to
  the value's parent, miscounts the object and cgltf refuses it. Fixed: strict and parent-linked, tests added; 2500
  mutants then gave 0 differences, 4 C++ crashes on malformed files (out-of-bounds in the C), 0 Rust crashes. The
  C++ is gone now, so a future parser change has only unit tests and the Python's verdicts to go by.
- **stb_image's failure reason is a C string and sticky:** thread-local and only set when a step reports one, so a
  corrupt deflate block of type 3 reports nothing new (and reports the previous failure, as the C does; the C++'s
  `std::string(stbi_failure_reason())` on a null pointer has no equivalent, the message is then empty). An unknown
  critical chunk's message starts with its type bytes, so a NUL in them shortens it to nothing. `stbi__skip` only moves
  a pointer: past the end the reader just returns zeros.
- **Why the earlier "failed a syntax check with no diagnostics":** `GEOpenFormats.cpp` needs `/std:c++17` (it uses
  `<charconv>`/`from_chars`) and `-EHsc` (`// UNIT-FLAGS`). The real compile and `cppgate` pass both and it compiles
  cleanly; a `-fsyntax-only` run without them fails ("no type named 'from_chars_result'"), with diagnostics unless the
  tool swallows clang's stderr.
- **cppgate after the C++:** `eemodel.py cppgate` builds the Rust `eemodel_gate` (rust/crates/ee_openformats) and
  compares it with the Python converter (independent) and with the digests of the C++ (`tools/eemodel/cppgate/golden_cpp.json`,
  subset 1 over tree-binary, 7150 cases; they include the refusal messages). The gate streams: a batch of cases
  (at most 300 MB with estimated outputs) is written, converted, compared and deleted before the next.
- **The Rust gate program ran on tree-binary; `sources.sources` defaults to the two installs:** use `--tree` to run
  from a built tree. Cases are numbered in data order; change the data and the golden no longer applies (the gate
  says so).

# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later (the WON library: "The WON library from
source").

Order that worked: compile census → STL headers → stock-header patches → mechanical source passes → header bridges →
one-off sources → DLL links → exe link → stub audit against VC6.

## R5b.1: formations in Rust (ee_formations), and why A* stays C++ (2026-11-09)

`rust/crates/ee_formations` is the formation geometry: `EEFLine`'s column offsets and `Place`, `EEFWedge`'s arms and
`Place`, `EEFCircle::Place`. The C++ classes stay (callers `dynamic_cast` them, set `mCenter`, `mHeading`,
`mOrientation`, `SetMinRadius` and `mHalfArc` themselves); their constructors and `Place` hand plain data to Rust
(`EEFormationRadii`, `include/ee/EEFormationRust.h`: the frame, the units' outer collision radii in input order) and
get slots back that name units by index.

- **The gate is a capture of the real game, not a dump tool.** `EE_SIM_CAPTURE=<file>` makes EE-SIM append one record
  per captured call (`ee/EESimCapture.h`: tag, size, the inputs, the outputs); `tools/sim_capture.py run` plays the
  oracle's six seeds with it, `check` compares per-seed digests with `tools/sim_capture/golden_cpp.txt` (recorded from
  the C++ before the geometry was deleted; the captures themselves stay in `build/`), and `replay` runs the Rust over a
  capture and compares every slot by bits. The capture writes nothing back: the six traces stay equal to the reference
  with it on.
- **The computer players form only circles** (herds, `EEHerd`; the attack formation's outward circle). Lines and
  wedges come from human commands, which EE-SIM has none of, so EE-SIM with the capture on also runs a sweep every
  five game minutes (`EEFormationCaptureSweep`): every formation type over the newest 1..64 units of each player (real
  radii, real positions, the yaw between the first and the last unit; odd sizes keep the identity orientation as
  `EEHerd` does). It reads the world and allocates only the formations, and the traces stayed equal with it.
- **What the capture found:** `EECPAttackFormationNode` sets an outward circle's `mHalfArc` to pi/2, so a circle's arc
  is caller state like its minimum radius; only its mode is the constructor's.
- **Circles append, lines and wedges clear** `outSlots` first; the C++ adapter keeps that.
- `UMTan` (from `UMCos`/`UMSin`, an infinity where the table cosine is +0) is in `ee_formations`; the tables and
  `UMArcTan2` come from `ee_rmap::math`, which should become a shared crate when a third leaf needs them. `UMCos(NaN)`
  indexes the table with `lrintf`'s indefinite value: out of bounds in the C++, a panic in Rust.

**A* (`EEAIAStar*`, ~6k lines) and the continent/choke-point analysis stay C++.** Not a divergence found by a gate:
the plain-data seam RUST_PLAN R5b.1 asks for does not exist in this code yet.

- The machine (`EEAIAStarMachine<S, G, M>`) is generic over seven goals, and each goal's `AcceptTile`/`GetCost` reads
  the live world per tile: `EEMapTile` unit lists, each unit's command, active group, collision radius and
  `IsPassableBy` (a virtual per unit kind), `CanMoveOnTerrain`, `gContinentManager`'s continents and path areas, the
  formation rank of the walker, and for the bounded goals a polymorphic modifier (`EEAIBPGModifier*`: enemy
  attackable by the unit, walls do not block, sea, space).
- `AcceptTile` is evaluated lazily, once per tile per search (`TestTile` caches it in the storage's flag grid), and the
  flood goals count and record tiles in it (`EEAIAStarPathAreaGoal`: `mTileCount`, bounds, `mTiles`). A grid snapshot
  computed up front would call it on tiles the search never reaches and in another order: not the same search.
- The goals walk the storage's intrusive lists directly (`CollectTiles`, the visited lists, `mPath`'s parent chain,
  the modifiers' `CheckNode`), and the nodes come from one static free list shared by every storage.
- So the order of work is: first, in C++, a per-search world view that the goals query by tile (accept, cost
  components) with no side effects, and the flood goals' bookkeeping moved out of `AcceptTile`; then the storage and
  the machine move to Rust over that view, gated by a capture of searches (start, goal, the view's answers, the path)
  in the shape of `tools/sim_capture.py`.
- **The address-ordered set (`EEAIAStarPathAreaGoal::AssignKindIDs`, `std::set<EEAIPathArea*>`) is not a hazard.**
  Its second loop only calls `AddIDBC`/`CC`/`DC`/`EC(id)` on each area's continent, with one id for the whole loop;
  each call appends that id to that continent's own vector unless it is there (`std::find`). Every continent ends with
  the same vectors in any visiting order, so address order, id order or any other order gives the same state, and a
  Rust port may use any deterministic order. `GetContinentFromID` is a plain array read. It is the only
  pointer-ordered container in the A*, continent and choke-point code; their other sets and sorts key on ids, serials
  and tiles.

## R3.6: the sound manager in Rust (ee_sound, 2026-11-09)

`SESoundManager`'s bookkeeping and decisions moved; its objects did not. The instance nodes are pooled and
intrusively reference counted and are held by units (`EEComplexUnit::mSound`), unit classes and effects; the samples
are shared with the mixer plug-in. So C++ keeps the nodes, the samples, the pool, the path, the mixer, the scavenger
thread, its events and the manager's lock; Rust (`rust/crates/ee_sound`) keeps the sound table, categories, channel
table, paused queue and instance map, one-shot list and the manager's own `URandom`, and sees nodes and samples as
opaque handles through a `Host` trait (C++ trampolines `ee_soundcpp_*` in `SESoundManager.cpp`, a nested
`SESoundManager::Rust` so they share the class's friendships with the node and `SEMixer`).

- **Reference counts change only through the host, in the C++ order.** The golden dumps print every reachable node's
  count and the pool's live count after each step, so a missing or extra `Reference`/`Dereference` shows at once. The
  test host asserts on any read or write of a node back in the pool or of a deleted sample.
- **Locks:** the facade takes the C++ lock around each Rust call where the original took it. Three checks the
  original made before the lock (the sample id in `PlayAndReturnReferencedNode`, `Unload`; `SetCategoryPercentage`,
  `SetCategoryPriority`, `GetAvailableChannels` took none) read Rust state now and so run under the lock. The checks
  on a null node or a none instance id stay before `this` is touched: EE-SIM has no sound manager
  (`gSoundManager == NULL`) and still calls `gSoundManager->Stop(mSound, false)` with a null node.
- **No reentrancy:** no trampoline reaches the manager; the FFI wrapper's `busy` flag aborts instead of aliasing if
  one ever does.
- **`std::sort` is not stable.** The scavenger sorts the paused queue by (category priority, node volume); equal
  keys come out in MSVC's introsort order (insertion sort to 32 elements, a fat partition with Tukey's ninther above
  40, heap sort after 1.5 log2 N divisions). `msvc_sort.rs` is that algorithm, pinned by 63 C++ permutations
  including the heap path (`std::_Sort_unchecked` with a small budget). The Linux/macOS C++ builds used libstdc++ /
  libc++ sorts, so their paused order already differed from Windows; Rust gives MSVC's everywhere.
- **The golden driver** compiled the original `SESoundManager.cpp` (git HEAD) into one scratch TU with
  `#define private public` (`_ALLOW_KEYWORD_MACROS` keeps the MSVC STL from refusing), link fakes for TSScheduler,
  the events, `UString` and the plug-in's SPIM table, `/FORCE:UNRESOLVED` for the inline-only references, and
  `FindFile`/`delete` of the file spec replaced by fakes. A clang-cl ASan build of the driver (`/fsanitize=address
  /MD`, `-D_DISABLE_VECTOR_ANNOTATION -D_DISABLE_STRING_ANNOTATION`, `/NODEFAULTLIB:stl_asan.lib`) found two bugs in
  the driver itself whose garbage had reached the golden file; every scenario is ASan-clean.
- **The fake mixer must behave like the real one where it matters:** `ee_mixer_delete_wave_data` takes a sample off
  its channel; without that, the destructor's `StopAllChannels` wrote into deleted samples.
- **The C++ UB that Rust defines:** `GetTotalPlayTimeInMilli` with a sample id past the table read out of bounds
  (Rust: 0); `GetTimeLeftInMilli` of a paused instance without a sample read through a null sample (Rust: 0); an
  out-of-range category in `SetCategoryPercentage` wrote out of bounds (Rust: ignored); `Unload` erased
  `map::end()` when a paused node was not in the instance map (Rust: no-op).
- **Covered only by the goldens** (the harness cannot hear): which channel a sound gets, voice stealing by priority
  and time left, category limits, paused looping sounds and their restart order, the random start offsets, one-shot
  unloading, `SetMaxChannels`, the destructor's cleanup.

## R4.3: the Graphics Engine's math in Rust (ee_ge, 2026-11-09)

The value classes `GE2DPoint`, `GE3DPoint`, `GEVector`, `GE3DLine`, `GE3DPlane`, `GETransformation`, `GEOrientation`,
`GEQuaternion`, `GEBoundingBox`, `GERectangle` stay C++ (every caller holds them by value); their algorithms forward to
`rust/crates/ee_ge` with the objects themselves as arguments (`#[repr(C)]` mirrors `EeGe*`, layouts pinned in
`Graphics Engine/GERustMath.h`). `UMath.cpp`'s table trigonometry (`UMCos`, `UMArcCos`, `UMArcTan`, `UMArcTan2`, `UMTan`,
`UMQuadraticEquation`) moved with them, and `UMTables.h` is gone: the tables are rebuilt the way R3.4 found (`um.rs`).

- **This is simulation code.** Units, physics, the AI, formations and the map call the distances, `SetOrientationYPR`,
  `GetYaw`, `ConvertToOrientation`, `UMCos`/`UMArcTan2` and the solvers every tick, so RUST_PLAN 2.1 applies to all of it:
  the C++'s operand order (`DotProduct` sums z first; `Apply` sums z, y, x, then the translation), `f32` throughout,
  `f64` exactly where the C++ is `double` (`Slerp`, `UMQuadraticEquation`), `lrintf` with the "indefinite" value. The
  C++ `(float)sqrt(float)` and a float square root give the same bits (correct rounding), so `sqrt` goes through `f64`
  as `UMFastMath::Sqrt` does.
- **The header inlines stay C++**: `GEVector::Normalize`/`DotProduct`/`Scale`, `GETransformation::Apply(point, vector,
  2D point)`, `SetPosition`, `Translate`, the getters, `GE3DLine::GetPointAt` (inline in the exe's view), `UMSin`,
  `UMFastMath::Sqrt`, `UMFloatToLong`. The game inlines them into simulation code; moving them would turn inlined
  float code into calls for nothing. Rust re-states the ones its own algorithms need, in the same order, and the goldens
  cover them through those algorithms. Trivial out-of-line members (component-wise operators, constructors, setters,
  comparisons, `MakeIdentity`, `ZeroOrientation`, `SetScale`) and the `FSFile` I/O stay too. `GERhombus` (vertex-array
  virtuals; its rotation is `RotatePoint`, now Rust), `GEColor` and `GETextureCoordinate` (only `Load`/`Save`) stay.
- **Golden values from the build's own objects.** A scratch driver linked the original `.obj` files of the units (the
  game build's, before the change) and printed per function the first cases and an FNV-1a digest of every output over
  inputs from an LCG (27 sections, 1.27 M cases; `ee_ge/src/golden_math.txt`). Three snags:
  - MSVC mangles access: `#define private public` renames `?f@C@@AEAA...` to `QEAA`, so the driver reads private
    members through the public getters or `memcpy`.
  - `GEBoundingBox.obj` emits the header-inline block of half the engine with static objects that need the engine at
    start-up (the scratch exe died before `main`); the driver compiled the unit's own function text alone.
  - C++ argument evaluation order is unspecified (`GE3DPoint(V(), V(), V())`): every random draw is its own
    statement, in the driver and in the Rust test alike.
- **Then end to end:** the same driver relinked against the new forwarders plus a Rust staticlib (`ee_rmap_gatelib`,
  which now links `ee_ge`) printed a byte-identical file. That checks the C++ glue (argument order, the `SetOrientation`
  codes, the axis numbers), which the Rust tests cannot see.
- **Aliasing is behaviour.** `CrossProduct(in, out)` and `MakeInverse(out)` store component by component, and an output
  that is also an input changes the later components (`v.CrossProduct(w, v)`; `t.MakeInverse(t)` transposes through its
  own stores). The Rust repeats the loads and stores through raw pointers (`cross_product_raw`, `make_inverse_raw`);
  every other function loads everything first, so value semantics are exact. The goldens have the aliased calls as cases.
- **Uninitialized reads in the C++** become 0: `InterpolateTo` for two points within 0.0001 of each other but not equal
  (the normalized direction is zero, no solver succeeds, the end parameter is stack garbage: the scratch driver itself
  died there, filling a vector without end), and `GE3DPlane::Intersects` for degenerate normals. A step of 0 or below
  never ends the loop, in both languages. The generator draws neither case.
- **`RemoveIntersectingRectangle` tests the output vector's size, not its own pieces**: the whole rectangle is added only
  when the caller's vector was empty, so the C ABI takes the size before the call.
- **cbindgen exports public constants and associated constants** of every included module (`PI`, `QUANT_COUNT`, a
  `constexpr EeGeTransformation_IDENTITY`): the algorithm modules are `/// cbindgen:ignore`, only `ffi` and `types`
  are read.
- **`offsetof` on private members**: the four classes with private data (`GE3DLine`, `GE3DPlane`, `GEQuaternion`,
  `GEBoundingBox`) declare `friend struct GERustLayout`, whose `static_assert`s pin size and offsets.
- `tools/rmap_gate`: `rmap_dump --tables` printed `UMTables.h`'s digest; the tables are gone, the digest stays pinned in
  `um.rs`'s test. The tool's `UMath.cpp` and GE units forward to `ee_ge`, so `ee_rmap_gatelib` links it.

### R4.3: GETerrainMesh's queries

`GetMeshZ`, `GetMeshNormal`, both `GetClosestTile`s, `InitializePlanes` and `Intersects(line)` (with its helper
`DoesLineIntersectTile`) are Rust (`ee_ge` terrain.rs). Units and the AI ask them for heights and tiles every tick.

- **The mesh stays C++; the queries read it in place.** `EeGeTerrainView` is built per call from the class (the point
  and tile rows, their count, a pointer to `mFrontPlane`: the four map-edge planes are contiguous members, which
  `GERustTerrain.h` pins along with the row layouts; a separate friend, `GERustTerrainLayout`, because only
  `GETerrainMesh.cpp` includes the mesh header).
- **`unsigned long` and `long` stay `c_ulong`/`c_long`**: `mPoints[x - mStart]` wraps on each data model as the C++'s
  does, and `UMFloorUnsigned`/`UMFloor` round through a `long` (`lrintf` 32 bits on Windows, 64 on LP64).
- **A point is read only where the C++ reads it.** `GetMeshZ` reads the (x, y + 1) height only in its upper triangle and
  `DoesLineIntersectTile` the (x + 1, y + 1) height only in its second pass; at the diamond's edge the other point may
  not exist, and an eager read would index before the row.
- **`Intersects(line)` writes its distance whenever a triangle's plane is met, hit or not**, through the reference it
  shares with `DoesLineIntersectTile`; the C ABI writes the last value, or nothing.
- The goldens (`golden_terrain.txt`) come from the original function text compiled alone (the unit drags in the
  renderer) over diamond meshes built as `CreateFlatMesh` builds them, 44,004 cases; the same driver linked against the
  forwarders and Rust printed the same file.

## R6: the UI toolkit in Rust, first part (ee_ui, 2026-11-09)

`UILayoutUtility.cpp` moved whole (its members forward from the new unit `User Interface/UITrampolines.cpp`,
`tools/units.py` `UI_TRAMPOLINES`); `UISlider`'s arithmetic, a multi-line `UITextLabel`'s word wrapping (`ReadWord`,
`ReadLine` deleted) and `UIForm`'s focus walk (`Tab`, `ShiftTab`, `CheckedRadioButton`) and control hit test moved
as functions the C++ members forward to. The C++ classes keep every member, vtable and exported name, so the
harness's hooks and `layout_gen.py`'s offsets are untouched (`UIForm` gained only a `friend`).

- **The UCRT's `lrintf` is not the x87's.** NaN and values outside [-2^31, 2^31] give 0, and 2^31 itself gives
  `LONG_MIN` (measured from the clang-cl build: `rect::float_to_long`). `UMFloor`/`UMFloorUnsigned` inherit it:
  `UMFloor(-1e30)` is -1. `ee_dx7`'s `float_to_long` returns `i32::MIN` for these (the x87's value); whether its
  inputs can be NaN or out of range was not checked here. glibc on x86-64 gives `LONG_MIN` (64-bit), arm64
  saturates; `ee_ui` follows each system.
- **Golden drivers over the real UI units.** A scratch TU `#include`s the unit after the STL headers and the engine
  headers under `#define private public` / `#define protected public`, then supplies only what the case runs: the
  `UIControl` ctor and `SetDirty`/`SetLocation`/`FireTriggerEvent` as trace lines, `GERhombus()`,
  `GERectangle::Contains` (GEBitmap.cpp's body), `UString(const char*)` for the U2D key statics a unit's headers
  define (without it the exe dies before `main`), the real `UWideString.cpp` and `ee_rust.lib` for `PlatMbs*`.
  Objects are `calloc`ed (`UIForm`, `UIManager`) or constructed through their real ctor; `/FORCE:UNRESOLVED` keeps the
  vtables' other slots. The same driver, recompiled over the forwarders and linked with the new `ee_rust.lib`, must
  print the same file: that checks the C++ seam (offsets, host calls, argument order), not only the Rust.
- **Host calls are virtual, so the record is re-read after each.** Rust works on the C++ object's own members (a
  pinned record from `mThumbRectangle` on, offsets `static_assert`ed in the unit) through raw pointers, writes them
  back before a host call (`ChangeRange`, `SetDirty`, `UIControl::SetLocation`) and reads them after:
  `EEUIFSEESElevation::ChangeRange` reads the slider. Protected virtuals are reached through a derived view
  (`UISliderAccess`), private members through a friend struct (`UIFormRust`).
- **Text stays bytes stepped by the C runtime** until the D6 switch (UTF-8, decided as a later track). Every reading of
  text is in `text.rs` (stepping, spaces, word and line ends), so the switch changes that module; `wrap.rs` takes an `Mbcs` (`PlatMbsInc`,
  `PlatMbsNInc`, `PlatIsMbcSpace`, `PlatIsMbbLead` through `ee_plat_core`), so a double-byte code page wraps as the
  C++ did; the tests use a single-byte page (the goldens' cp1252) and a Shift-JIS-shaped fake.
  `GetMultiByteChar` reads signed chars, so 0xa0 is never a space. `ReadWord` stops after 256 characters but returns
  257 when text follows, so the next line starts one character later: kept.
- **A driver trap:** `printf("%d %lu", f->SetTabStop(s), f->mTabStop)` reads `mTabStop` first (MSVC ABI, arguments
  right to left); the replay must print the value from before the call.
- **Builds:** `build.py` recompiles every unit (about 11 minutes at `-j 3` with other builds running). A unit is checked
  with `tools/clang_build.py --only NAME lle` in seconds; the drivers check behaviour; the full link comes last.
- **Gates run for this part:** both Windows builds link 0/0 (EE-AOC, EE-SIM, the windowed-software test profile),
  `rust_build.py gate`, the seeded sim oracle (seeds 2-7 identical to `ref`), `layout.json`'s records for UIForm,
  UIManager, UISlider, UITextLabel and UILayoutUtility unchanged, and the harness's 34 UI symbols present in the map.
  No tour or screenshot comparison was run here.

## R5a.1 and R5a.2: the per-tick checksum trace and saving without side effects (EE-SIM, 2026-11-09)

`EE-SIM --checksum-trace FILE` writes one line per communications tick (`EESim.cpp` ChecksumLine, from the
`gPlatSimAfterTick` hook): the game's CT index, PT, tick, game ms, the physics random seed, `GetOutOfSynchChecksum`,
the units' `GetChecksum(3)`, `EECPOutOfSynchChecksum` and the input count. `tools/sim_oracle.py run --checksums` keeps
them (`ct-SEED.txt`) and `sim_oracle.py ticks BASE NAME` names the first divergent tick and which fields differ there.

- **Reading must not write.** `GetOutOfSynchChecksum`'s flag 4 (the map) is not a read: `EEMap::ComputeChecksum`
  checks a 64 x 64 window and advances `mChecksumCursor`. Flag 0x20 (the goals) copies every goal queue and
  dereferences goal members the multiplayer check (flag 1 only) never reaches. The trace uses 1|2|0x10|0x40|0x100 and
  puts back `gU531CF4`, the one global the function writes. Proof: seeds 2-7 with the option on and off print EE-AI
  traces equal to the reference, and two runs' checksum traces are identical (3086 ticks each).
- **Two numbers that are not the game's.** The communications' own CT count includes the CTs the send thread sent
  before the sim took over (22-25, wall-clock dependent): the line counts from the game's first CT. Around a save the
  physics loop reaches the after-tick hook many times without running a tick (151 times at one autosave of seed 2):
  a line is written only after ticks ran.
- **Saving changed the game in three more places** (the first, `EECPCivilizationManager::Save` setting
  `mBuildingCountsChecked`, was fixed on 2026-11-06): `EEUGGather::Save` cleared `mGathering`/`mWaiting` when the
  resource was gone; `EECPWallManager::Save` cancelled an unfinished wall path request and planned the walls again
  (`AddWalls(mFrom)`, `ClearRequest`, the next wall 30 s later); `EECPDefenseCommander::Save` left its loose sub group
  ids in `mLoadedSubGroupIDs` (no reader outside Load/Restore). Each now computes the file's values on locals: the
  file's bytes are unchanged, the running objects untouched. Found by grepping every `Save*`/`Write*` body for member
  and global writes (266 functions); the rest of the save path (`SaveGame`, `BuildSaveGameChunks`, `GetChecksum`) only
  touches presentation (`ApplyVertexColors`, `ShowSelectionAreas`, the sound volumes).
- **The evidence.** With `--autosave-ms 60000` (a save every 150 game s, 18 per game) seeds 2 and 4 diverged from the
  unsaved game before the fix, reproducibly: first divergent tick at 763.9 s (13 s after the 750 s save) and 2402.8 s,
  both first in the random seed and the units. An earlier build with the same save code did not diverge at all: where
  the save lands between ticks follows the wall clock, so whether a side effect shows depends on timing. After the fix
  the six seeds' checksum traces equal the unsaved game's at 60000 (two runs) and 600000. A build with only the
  gather fix also cleared seeds 2 and 4: the gather flags were the side effect these games hit; the wall and defense
  fixes are by inspection.
- **Not done: a loaded game still differs from the saved one.** The file keeps the original's values
  (`mBuildingCountsChecked` true, the gather flags cleared, the walls re-planned), so a game loaded from a save plays
  differently from the game that went on. Writing the live values instead would change the save files.

## R4.3, second part: GETerrainMesh's edits, fog and file codec (ee_ge terrain_mesh.rs, 2026-11-09)

`ComputeVertexNormal`, `ComputeShorelines`, `UpdateFogOfWar`, `Load` and `Save` are Rust (`ee_ge` `terrain_mesh.rs`);
the C++ members forward. The mesh, its rows, the render arrays and the materials stay C++. `ee_dx7` now takes
`GETransformation::Apply` (the composition and the point and vector forms) from `ee_ge` instead of its own copy; its
`golden_draw` model sections pin it.

- **Simulation code.** The random map generator, `RTSMap`'s height edits and the scenario editor call
  `ComputeVertexNormal` and `ComputeShorelines` while a game is set up and played, and units read the normals and the
  water bits, so RUST_PLAN 2.1 applies: the C++'s `f32` order (`d + ne + nw`, `ne + n - nw`), `unsigned long` index
  tests that wrap (`inY - 1 < mPointRowCount`), a neighbour row read only when it exists. The Rust returns the
  unquantized normal; the C++ stores `GetQuantizedVersion()` of it (already Rust). `ComputeShorelines` returns the
  bits for `SetWaterInfo`, or -1 where the C++ leaves a land tile alone.
- **A file codec as a field stream.** `Load`/`Save` keep going field by field through the file's own typed
  `Read`/`Write` (`EeGeFieldIo`, filled by `Graphics Engine/GERustIO.h` from an `FSFile&`; `io.rs`), so the bytes,
  each file class's short-read rule (`FSMemoryFile` reads nothing, `FSFileSpec` reads a part and says
  `kErrorFailed`) and what a failed load leaves behind are the C++'s. A whole-block read would have changed all three.
  `Read(long&)` goes through a 32-bit copy of the old value and stores it back even on failure; the Rust repeats that
  for the map size and the fractal seed. `Load` is split where it builds C++ objects: Rust reads the size, the C++
  runs `CreateFlatMesh`, Rust reads the tiles, points and seed (an unregistered texture set id becomes 0 through a
  256-entry table the C++ fills), the C++ recomputes the fractal and the normals. The engine's codes are
  `kErrorFailed` = 9 and `kErrorEndOfFile` = 0x12, not small negatives.
- **`UpdateFogOfWar`** reads the visible area through `EeGeTerrainFog` (`GERustTerrain.h`): the two `[257][257]`
  point-visibility arrays, and the render vertices' `mVisibility` reached as a pointer plus `sizeof(GETMRenderVertex)`
  (0x28 on 32-bit, larger on LP64), so no render-vertex mirror is needed.
- **Goldens** (`golden_terrain_mesh.txt`, 6,146 cases over six meshes including 1 x 1 and space maps): the original
  function text compiled alone (`#define private public` around `GETerrainMesh.h` only, after the STL), with the real
  `CreateFlatMesh` and `InitializePlanes`, a driver `FSFile` with `FSMemoryFile`'s rules and a write limit, cut files
  at every field boundary kind, and `GEVector::GetQuantizedVersion` replaced by a copy so the stored normals are the
  unquantized vectors (the quantization would hide a last-bit difference). The driver relinked over the forwarders
  and `ee_rmap_gatelib` printed the same file.
- **Line endings:** `.gitattributes` is `* -text`, so each file keeps its own (the sources are LF,
  `PORT_LEARNINGS.md` is CR LF). From Git Bash on Windows, a text-mode Python write turns LF into CR LF and `sed -i`
  turns CR LF into LF: edit in binary, or with `newline=""`, and check `git diff --stat`.
- **`GEPhysicalModel`'s per-triangle tests** (`physical.rs`): the line-against-triangle step of
  `Intersects(line)` (the plane through the triangle, the crossing, the line from the first corner through it against
  the opposite edge) and `ComputeTriangleBox`. Units test projectile lines against target models with them
  (`EEUnitClass.cpp`), so they are simulation code. The C++ keeps the walks (materials, LODs, sub-models, the vertex
  arrays behind `GEVertexArray`'s virtuals) and the transform set-up. The distance is written whenever the plane is not
  parallel, hit or not, as the C++'s reference was. Golden: `golden_physical.txt`, 40,000 triangles and lines (through
  the first corner, parallel to the plane, aimed inside, anywhere), from the original loop body compiled alone; its
  math is the build's GE units, themselves pinned to the original by `golden_math.txt`.
- Not moved: `UpdateVisibleTerrain` and the eight `Generate*EdgePolygons` (they build C++ material maps keyed by
  `GETexture*`, reference textures and fill polygon pointers into the mesh), `GetTileRowsInVolume` (the
  `U2DConvexHull` template), `SetGlobalWaterState`/`SetColorSet`/`SetWaterColorSet` (URandom, the fractal, the
  registered sets), `UpdateWaterAnimation` (the clock and texture sets), `CreateFlatMesh` (allocation).
- **Gates run for this part:** `build.py -j 3 --sim` links EE-AOC and EE-SIM 0/0, the windowed-software test profile 0/0,
  `rust_build.py gate` PASS (940 tests), `eedata.py check` PASS, `gx_test` PASS, the sim oracle (seeds 2-7 identical
  to `ref`). Not run: `eemodel.py cppgate` (no model or texture bytes changed), tours, a saved game loaded through the
  new `Load` in the game (the codec is pinned by the goldens only).

## R6: edit boxes and list boxes in Rust (ee_ui edit.rs, list.rs, 2026-11-09)

`UIEditBox`'s text model (insertion with word wrap, deletion and re-wrap, caret moves, selection, single-line
scrolling, the caret from a mouse location, the IME composition's bookkeeping: 30 members) and `UIListBox<T>`'s
selection and scrolling (`ChangeSelectionIndex`, `CheckScrollBar`, the arrow keys, clicks, double clicks, auto-select,
`SetLocation`, the bookkeeping after inserts and deletes) moved to `rust/crates/ee_ui`. The C++ classes keep every
member, vtable and exported name; `include/lle/UIEditBox.h` gained only `friend struct UIEditBoxRust`, `UIListBox.h`
`friend struct UIListBoxRust<T>`. What stays C++: construction, drawing (`CopyData`, `DrawPolygons`, `DrawTextA`), the
clipboard, mouse and keyboard capture, key dispatch and repeat timers, focus, the list data (`std::vector<T>`),
`FindString`, `std::sort`.

- **The lines stay a `std::vector<UWideString>`.** The harness reads an edit box through `GetText`/`GetLineCount`
  (header inlines over `mLines`) and a list box through `GetData`/`GetLineCount`, so Rust reads and writes the lines
  through host calls (`ee_uicpp_edit_line`, `..._set_line`, `..._insert_line`, ...) instead of owning them. Only the
  `UWideString` capacities differ from before (no one reads them).
- **A template's host calls are a table.** `UIListBox<T>` is instantiated for `UWideString` (exported, emitted by
  `UIComboBox.cpp`) and by the game for its own line types, whose subclasses override virtuals. `UIListBoxRust<T>` in
  the header instantiates one static `EeUiListHost` of function pointers per `T` and passes it in `EeUiListRefs`; the
  virtual members the algorithms call (`GetNumLines`, `ChangeSelectionIndex`, `ClearSelection`) are called virtually,
  so an override still runs. The record offsets are `static_assert`ed in `Refs`' constructor, which every forwarder
  instantiates.
- **Re-entry.** A virtual host call can come back into Rust for the same object (`SetRectangle` -> the box's own
  `SetLocation`; a list's virtual `ChangeSelectionIndex` -> the forwarder). Each FFI call loads the record at entry and
  stores it at exit, and stores/reloads it around every host call with side effects (`Edit::call`, `List::call`), as
  `UISlider` did.
- **Text.** Everything that interprets text is in `text.rs`: characters through `Mbcs` (lengths, `CopyString`'s
  prefixes, `_mbsninc` tails, `_mbschr(s, '\n')`, `_mbsnccnt`, spaces, digits) and the case-folded order
  (`_mbsicmp`, `UWSNoCaseAlphaSort` now calls `ee_ui_text_less_no_case`). `SingleByte::inc` no longer steps past the
  terminator. In `AddChar`, `\t` and `\r` are spaces to `_ismbcspace`, so their own branches never ran: kept.
- **One divergence (a crash in the original):** `GetLocationFromIndex` with an index before the drawing index asked
  `UWideString::CopyString` for 2^32 - k characters, and `_mbsncpy` padded that many NUL bytes into a small buffer. A
  multi-line box gets a drawing index from `SetCaretLocation`'s `ComputeDrawingIndex` (a caret line whose prefix is as
  wide as the box); typing at the start of such a line then crashed the scratch driver. Rust measures nothing there
  (the caret at the left edge). The golden driver ran the original with that one line patched.
- **One fix (`InsertLine`):** the inserted line's index was taken after `mData.insert`, from an iterator that a
  reallocation had invalidated, so the selection's bump after an insert depended on the heap. The index is now taken
  before the insert (the header's member template; the golden driver ran the original with the same change).
- **`_mbsicmp` folds to lower case** in the UCRT's C locale (as `_stricmp`): `'_'` sorts before the letters. The
  golden showed it; `ee_plat_core`'s POSIX fallback (`mbs_icmp`) folds to upper, so Linux and macOS sort names with
  `[`, `\`, `]`, `^`, `_` or `` ` `` differently from Windows. Not changed here (it is ee_plat_core's).
- **MSVC's `std::lower_bound`** is a binary search: over an unsorted range (`InsertLine(line, at, sort)` from an
  arbitrary index) it does not give the first greater element; the replay searches the same way.
- **Other original hazards kept:** a single character wider than a multi-line box's text area makes `AddChar` add
  lines forever; `DeleteChar` with a selection (the game deletes the selection instead) can index past the lines.
  The drivers avoid both, as the game does.
- **Goldens:** `golden_edit.txt` (80 boxes x 60 operations: typing, paste, Enter, Backspace, Delete, arrows, Home,
  End, selections, `SetText`, clears, mouse positions, `CheckScrollBar`, width and location changes, IME open, change,
  close, scrolling) and `golden_list.txt` (60 boxes x 60 operations). The drivers over the forwarders print the same
  files and also equal the original on 2,000 edit boxes x 150 operations (979k lines) and 400 list boxes x 100.
  MSVC's `std::sort` is an insertion sort (stable) up to 32 elements; the list driver keeps lists shorter so that the
  replay's stable sort matches.
- **Python and line endings:** `PORT_LEARNINGS.md` and some goldens are CRLF, the Rust sources LF, and
  `.gitattributes` stores bytes as they are (`* -text`): write files in binary. A Python heredoc that carries Rust
  escapes (`b"\0"`, `'\n'`) put raw NUL and newline bytes into `text.rs`; write such code with an editor, not through
  Python string literals.
- **Gates run for this part:** both commits built EE-AOC and EE-SIM (`build.py -j 3 --sim`) and the windowed-software
  profile, each 0 unresolved, 0 duplicates; the sim oracle (seeds 2-7, open tree) identical to `ref` after each;
  `layout.json`'s 38 UI records unchanged from the pre-change build; the 24 names `presentation_bridge.c` resolves
  (the `UIListBox<UWideString>`, `UIEditBox`, `UIComboBox`, `UICheckBox`, `UIRadioButton` members and vtables, the
  `UWideString` members, `gUIManager`) present in both maps; `rust_build.py gate`'s steps pass except
  `ee_plat_sys`'s two socket tests, which fail when the tests run in parallel on this shared machine and pass with
  `--test-threads=1` (the crate is untouched here). No tour, screenshot or live edit-box/list-box use was run.
- **Still C++ in R6's toolkit:** `UIComboBox`, the buttons, check boxes and radio buttons (state machines over tab
  stops, sounds and capture), `UIControlsTable`, `UIManager`, every `CopyData`/drawing member, the edit box's key
  dispatch and repeat timers.

## R6: WON's leaves in Rust, and the lobby without HTML (ee_won, 2026-11-09)

Decision D8 ports the WON library to Rust, leaves first. Crate `ee_won` (pure code, `cargo test` without a game) now
holds the message buffers' format (`wire.rs`), WON's text conversions (`text.rs`, the one place: D6), CRC-16, MD5,
the random generator, Blowfish, the big integers, ElGamal, the key encoding, the auth blocks and the CD key logic.
The C++ classes stay where C++ holds them (ByteBuffer, ReadBuffer/WriteBuffer, CRC16, MD5, MD5Digest, Blowfish,
ElGamal, AuthBase and the certificates, CDKey) as forwarders; BigInteger, Random and IntegerExtractor/Inserter are
gone (nothing outside WONCrypt used them).

- **Golden vectors needed AddressSanitizer.** The generator links the real WON units (clang-cl, like the game) and
  prints synthetic cases; it was built twice, plain and with `/fsanitize=address` (`/MD`, the dynamic ASan runtime,
  `/NODEFAULTLIB:stl_asan.lib`, `_DISABLE_STRING_ANNOTATION`/`_DISABLE_VECTOR_ANNOTATION`, `ASAN_OPTIONS=replace_intrin=0`
  past BigInteger's overlapping `memcpy` in an in-place shift). Only cases that ran clean and printed the same in both
  builds became goldens: several big-integer cases gave different answers with different heap contents.
- **The clang-built `MD5Digest` returned MD5's initial state for every input.** Its rounds rotate with
  `(unsigned)a >> -s` (Java's `>>>`): a negative shift count is undefined behaviour, the original compiler masked it to
  `32 - s`, and clang dropped the rounds. So in the C++ game `Random` repeated the same 16 bytes, every ElGamal
  signature was over the same constant, and the golden program's `Verify` accepted a signature for a different
  message. WONCommon's `MD5` (macros, unsigned) was right. The Rust MD5 is RFC 1321 (tested on its vectors), and ElGamal and Random are
  checked against a Python model with exact integers and real MD5 (`py*` lines of `golden_won.txt`), not against the
  broken C++.
- **`st_kmul`'s Karatsuba was wrong for odd word counts above 16** (`a[N-1]*b[N-1]` multiplied in 32 bits, operand
  words read past their length, writes past the work buffer that ASan reported as heap-buffer-overflow). WON's real
  keys (1024 and 2048-bit moduli, tiny top words in the reduction constants) mostly avoided it. `ee_won` multiplies
  exactly; the other algorithms (shift-and-subtract division and modulo with their sign rules, Barrett reduction with
  at most three corrections, binary gcd, extended Euclid) are ported as they were, with the C++'s "target keeps its
  sign" rule of the shifts. `gcd` with a negative operand never returned in C++ (not ported as a case).
- **Other C++ faults the port does not keep:** `WriteBuffer::ToByteBuffer` on a buffer never written wrote the prefix
  before NULL (it now reserves first); Blowfish left the second block of a 4-byte input uninitialised (zero now); an
  ElGamal block whose plain text starts with zero bytes read before its buffer (the zeros are restored); a key whose
  modulus has no reciprocal let `ArithmeticException` escape `SetPublicKey` (now false); `IntegerInserter` writes a
  two-byte length low byte first, which its own reader takes big-endian: kept, and tested (keys over 255 bytes per
  integer do not round-trip).
- **On LP64 the C++ crypto was broken anyway:** `MD5`'s and `BigInteger`'s `unsigned long` words were 64 bits on
  Linux and macOS. The Rust is the same on every system.
- **Seams:** fixed-size state is a `#[repr(C)]` struct embedded by value (`EeWonMd5`, `EeWonBlowfish`: Blowfish is
  copied by value, e.g. into `AuthSession`); keys and auth blocks are boxed Rust objects behind opaque pointers
  (`EeWonElGamal`: the C++ `ElGamal` deep-copies through `ee_won_elgamal_clone`, as `std::list<ElGamal>` needs);
  results of variable length go to a sink (`WonByteBufferSink` makes the `ByteBuffer`, `WonWideSink` the
  `std::wstring`; WONBuffer.h). `WONBuffer.h` includes `rust/ee_rust.h`, so every WON unit sees the C ABI.
  cbindgen renames the generic Rust names (`Md5`, `Blowfish`, `ElGamal`, `AuthBlock`): a global `Blowfish` next to
  `WONAPI::Blowfish` is ambiguous under `using namespace WONAPI`.
- **The auth blocks** unpack in Rust in one call per block (`ee_won_auth_unpack`); `AuthBase::Unpack` asks the
  subclass for its kind (`UnpackKind`) instead of passing a `ReadBuffer` down the `UnpackHook` chain, and the
  certificate getters ask the unpacked block. Header fields read before a failure stay set, as before.
- **Tests:** `cargo test -p ee_won` (unit tests and the goldens); `xbuild.py wontest` gains `won_crypto_test` (the C++ API
  over the forwarders: CRC16, MD5, MD5Digest, Blowfish copies, ElGamal copies, signing, a key block).
- **build.py has no incremental compile:** every build stages and compiles all ~1,400 units (an hour at `-j 3` on a
  shared machine). To check a change, compile only the units that include the changed headers (transitively) into the
  last build's object folder and relink with `build.link_exe` (a scratch script); run the full builds once at the end.

### The lobby without HTML (D8)

WON's HTML rendering is gone: the parser and documents (`HTMLParser*`, `HTMLDocument*`, `HTMLTag`, `HTMLFromText`,
`HTMLDisplay*`), the line-segment layout engine that only HTML used (`LineSegment`, `MultiLineElement`,
`MultiLineBuilder`, `TextLineSegment`, `Table`, `TableElement`, links and image maps) and the browser's downloads.
The chat window never used it (`TextArea` has its own lines).

- **`BrowserComponent` stays as the layout type** the lobby's .cfg files name (MOTD, login help, terms of use, the
  Help and News tabs; their keys `FontSize`, `Transparent`, `Scroller` and the child `HTMLVIEW` still parse). It is a
  `TextArea` in a scroller now: a page file's text, HTML pages with their tags dropped, block tags as line breaks and
  entities decoded (`ee_won/src/plaintext.rs`, from Windows-1252 bytes). Back, forward and stop stay disabled (no
  links, no history), find finds nothing, and a web address (the News tab's community URL) is shown as text: the WON
  servers are gone and nothing fetched pages but the browser.
- `MOTDCtrl` and `TOUDialog` built an HTML document of two files (`HTMLDocumentGen`, flags IsOptionalHTML and, for the
  MOTD, SkipFirstCharIfNotHTML: a text MOTD's first character is WON's "is new" flag); `ShowFiles` keeps that rule.
- `WrappedText::mRichText` was never set; its branches went with the segment engine.

## R2 performance: the text db load under the Windows debug heap (ee_data, 2026-11-09)

Every harness session starts the game as a debugged process (exe_boot.c, `DEBUG_ONLY_THIS_PROCESS`), which turns on the
NT debug heap (no low-fragmentation front end, fill and free checks) unless `_NO_DEBUG_HEAP=1`. There a heap operation
costs 30-80 microseconds instead of ~50 ns, and the Rust text compiler (R2.3) did not reach the main menu in 120 s.

- **Measure with the same entry points.** `ee_data_startup <Data/db> [--repeat N] [--tables]` (rust/crates/ee_data/src/bin)
  runs the game's load (one `Content`, `Rules::load`, then every table with a text form compiled, the ids of dbobjects
  and dbtechtree) with a counting global allocator: time, allocations, reallocations, frees, bytes, peak, and the time
  spent inside the system heap by block size. Run it under a debugger to get the debug heap: a ctypes launcher with
  `CreateProcessW(..., DEBUG_ONLY_THIS_PROCESS)` and a `WaitForDebugEvent`/`ContinueDebugEvent` loop is enough
  (`DEBUG_EVENT`'s union starts at offset 16 on x64, not 12).
- **Before / after, on the open tree's Data/db** (11 MB of text, 8.0 MB of db files):

  | | allocations | reallocations | allocated | release | under the debug heap |
  |---|---|---|---|---|---|
  | before | 3,861,008 | 197,240 | 223 MB | 0.9-1.3 s | 115 s (rules 41 s, tables 74 s) |
  | after | 56,552 | 1,962 | 125 MB | 0.45-0.8 s | 5.2-6.2 s (rules 0.3 s) |

  Release times are noisy on a shared machine; the debug-heap time is the number the harness sees.
- **The whole game under the debug heap** (EE-SIM.exe started with `DEBUG_ONLY_THIS_PROCESS`, the open tree, seed 2):
  the main menu at 90.0 s before (the main checkout's EE-SIM.exe of 2026-11-09 10:11) and at 19.9 s after; with
  `_NO_DEBUG_HEAP=1` the whole 1-game-second run takes 9.7 s. The ~15 s left before the menu are outside ee_data.
- **Where they went:** (1) the YAML parser copied every line, key and scalar (`Vec<u8>` each, a `Rc<Node>` per node,
  keys/items grown by push); (2) `find_table` lowercased its argument into a new `Vec` on every reference cell
  (`resolve` calls `table(r.table)` per cell: 190,000 allocations for dbobjects alone); (3) `parse_f32` lowercased
  every float; (4) `msg!` places built eagerly per row, per field (`g.where_.clone()`) and per reference
  (`m.where_at(x)`), each growing a `Vec` and formatting integers through `to_string`; (5) `get_index` cloned the
  rows and kept a `line_of` map of place strings; (6) `elems` built a `Vec` per array field per row.
- **The shape now:** `node::Bytes` is a slice of the text it was parsed from (an `Rc<[u8]>` and a range; a quoted
  scalar with escapes is the one copy), `Node::items` is `Vec<Node>` built off a parser stack at its exact size (one
  allocation per collection), a placed record is its list's `Rc` and an index, a place in a message is
  `msg::At { what, line }` (any `msg::Part`) written only when a message is, the readers in text.rs take
  `where_: &W where W: Part`, a row's place / key marks / string field reuse buffers (`Scratch`), and every row's
  record lives in one buffer per table. Messages and output bytes are unchanged (`eedata.py check`, `cppgate`).
- **What is left is mostly frees.** After the fix the debug heap spends ~3.4 s in 45,000 frees and ~1.2 s in 56,000
  allocations; small blocks cost as much as large ones, so the count is what matters. Remaining per row: a CSV
  record's keys and items, a YAML record's keys and items and one per list value, a lead table's strings, and one
  shared id per indexed row. The next step, if needed, is a flat document (all nodes of a text in one vector,
  children as ranges), which would bring a table's parse to a handful of allocations; it changes the `Node` API that
  rules.rs, textrow.rs and the generated `text_rows` use.

## R5a.7: the gates on the game's own fixture, the harness clock through PlatSim (2026-11-09)

`tools/gates.sh` now runs `tools/harness/fixture_game.py`: the test-profile exe started directly with `EE_TEST_UI`,
read only through the fixture (`EETestFixture.cpp`), its stderr and its game log. `--legacy` keeps the debugger path.

- **What the gates read and where.** PT, game ms, game over, world players, map, local player: the fixture's `game ms`
  line (it always had them). New: `comm ct <n> last <n> paused <0/1> multiplayer <0/1>` (the CTs and the pause the
  input check needs), `civ <locked> <chosen> stock f/w/s/g/i` per player (the civilization pre-empt's check, the
  summary's resources), `clock <num>/<den> ms <t> frames <n>`. The units the old summary called `units_lb14` are
  `population` (`mPopulationCount`), the victory units `victory` (`mEndOfGameCount`). A command's verdict is the
  `TEST UI <verdict> ...` stderr line. The game log (`Logs/game-*.log`) names the computer players; the driver checks
  their number.
- **Windows' `rename` never replaces.** The fixture wrote `ui-state.txt` through `.tmp` + `rename`, which on Windows
  fails when the target exists: the file was written once, at the first poll, and the driver saw a frozen main menu.
  `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING)`; the reader may hold the file for a moment, the next poll writes it.
- **The harness clock is a seam of the platform layer.** `PlatSimSetTimeFactor(num, den)` (`ee_plat_core::sim_clock`):
  `PlatTimeMilliseconds(64)` and `PlatCounterTicks` run F x real time from their values at the set (no jump); `PlatSleep`,
  the timer intervals and the finite timeouts of `PlatEventWait`/`PlatEventWaitAny`/`PlatThreadWait` last 1/F. Without
  a factor every function calls exactly the Win32 import it called before, so the legacy time shim still works.
  Windows waits shorter than a millisecond on a high-resolution waitable timer per thread (added to the wait set, as
  the shim did); scaled timers are a thread each with deadlines on the performance counter (a multimedia timer fires
  at most once a millisecond), ids from 0x7e000001. `EETestFixtureConfigure` (first in `EERun`, only with
  `EE_TEST_UI`) takes `EE_TEST_TIME_FACTOR`, `EE_TEST_SEED` (`gPlatSimSeed`, as the old seed pin) and
  `EE_TEST_NO_HUMAN_DEFEAT`; a malformed one stops the game. GX's own `Instant` clock (frame pacing, the null-3D key
  frames) stays real time, as the key frames were meant to be.
- **Game time follows the clock, PTs follow the timer.** At factor 32 and PT rate 40 a seeded 2-AI Hard game ran 1280
  PTs per real second and about 16 PTs per game second (Very Fast); the legacy shim gave about 23. The driver stops at
  its first poll past the cap (once a real second = about 1,300 PTs) and samples once more 3 s later, so the final
  PT is the driver's, not the game's.
- **Frames: a window capture, not GX's read-back.** `GXTestReadFrame` from the fixture's present hook timed out (10 s,
  "GX: read-back ... timed out") on every frame on this machine's WARP adapter; the legacy log has the same timeouts
  between successes. The driver captures the game window with `PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT)`,
  which returns the composed swap chain. `frame.request` -> `frame.bmp` stays for other platforms. A window keeps its
  last picture when the renderer stops presenting, so a game capture must differ from the one before it (a running
  game changes about 0.2% of the picture between captures 120 s apart): this found GX's "Not enough memory left"
  bind-group failures, after which the window showed the loading screen while the game ran on.
- **Debug output without a debugger.** With `EE_TEST_UI`, `PlatDebugSetHandler` sends `PlatDebugOutput` to stderr as
  `debugstring <text>`, so GX's errors and the EE-AI lines reach `game.log`; `summary.json` counts GX's errors.
- **What no longer runs on the gate path:** exe_boot.c's guards (registry writes, files outside the stage,
  non-loopback sockets), its crash/abort hooks (a crash ends the process and fails the run; a message box fails it;
  `SetErrorMode` keeps WER dialogs away), the RNG trace, `--instance`, the human driver and the differential tools.
  They still need the layout and the mangled names (`layout_gen.py`, `game_state_probe.py`, `hbuild.py`,
  `exe_boot.c`) and run with `gates.sh --legacy` / `game_session.py`.

## R5b.1: the tile A* searches in Rust (ee_astar, branch rust-r5b-astar, 2026-11-09)

The order proposed in "R5b.1" above held through step 3 and the first goals of step 4. `rust/crates/ee_astar` is the
node storage and the search machine of every `EEAIAStarMachine<S, G, M>` (the land/sea/space path searches, the
bounded search, and the continent, hill, choke point, path area and path sector flood fills), plus the continent,
hill and choke point goals' tile tests. The graph searches (`EEAIAStarGraphMachine`, `EEAIAStarGraphStorage`, the
path graph and continental chain goals) are untouched C++ and not captured.

- **Step 1, the view (`ee/EEAIAStarView.h`).** Every read the tile goals' `AcceptTile`, `GetCost` and
  `ClassifyTile` made of the live world is one query with a plain answer (about 35 queries: the tile's terrain, edge,
  units, height level; the continent manager's ids, tile flag and value, path areas; the choke pass's grids; the
  unit loops of the area fills, each loop its own query because their predicates differ slightly; the walker's
  facts). The expressions moved unchanged; the goals keep their state and side effects in the same order. The path
  goals' modifiers (`EEAIPathGoalModifierBase` and subclasses, the storm's) stay world-side policies, asked as one
  query each (`CanEnter`, `GetModifierCost`). The search set-up's reads (the walker's blocked tiles in `Prepare`) are
  not per tile and stay with the goal; their effect reaches the search as `MarkTile` calls.
  - A blocker query that also returns the unit's player would read `GetPlayer()` of units the C++ never asked: so
    there are three (`HasBlocker`, `GetFirstBlockerID`, `GetFirstBlocker`), each reading what its caller read.
- **Step 2, the capture (ASRN, `ee/EEAIAStarCapture.h`).** One record per `Run`: its inputs; the count and FNV-1a of
  every event (the machine's questions to its goal: AcceptTile, GetCost, Estimate, IsGoalNode, SearchDone, MarkTile;
  and the goal's view queries, in order, with the answers); its outputs (the machine's members, the goal's flags,
  the current and path nodes, the bounds, every node of every list in order). The six oracle games make 82,302 runs
  (by machine type: 26,182 land paths, 2,001 water, 47,709 bounded, 127 continent, 52 choke point, 34 hill, 1,304 path
  area, 4,893 path sector fills) and 34.8 million events; a capture is about 3 MB per game. Recording the events
  themselves (`EE_SIM_CAPTURE_ASTAR_EVENTS=1`) gives 160 MB for seed 2, enough to name the first event where two
  builds part. Two captures of the same build are identical, and the capture runs' traces equal the reference.
- **Step 3, the storage and the machine.** `storage.rs` and `machine.rs` are line-by-line ports. Nodes are an arena
  with a linked free list per storage instead of the C++'s one static list: which free node a search reuses is not
  observable (`NewNode` clears every field, nothing compares node addresses), and the C++ never releases a node twice.
  Kept quirks: the hash's high-nibble fold replaces the hash; `RemoveNearestOpen` on a one-node sorted list sets the
  head from the node's (null) prev; `PrepareGrid`'s clears stop one tile short of their span's end, so a stale
  accept/reject flag can survive into the next search; the terrain storages never clear `mBounds`;
  `Init` sets the size before `PrepareGrid`, so a "size changed" test there only ever sees the first allocation.
  - The C++ machine keeps its members (`mTile`, `mRevs`, `mSteps`, `mFound`, `mCheckCorners`): goals write
    `mCheckCorners` from inside `AcceptTile` (the area fills) and the path manager reads the others. The Rust machine
    reads and writes them through callbacks every time and never caches one across a call into the goal.
  - No callback re-enters the running storage: what a goal read of the storage during a search (`SearchDone`: the
    current node and whether an open node is left; `GetCost`: the current node's parent) arrives as callback
    arguments, cached on the C++ storage (`mHook*`). `IsGoalNode` gets a stack node with only `mX`/`mY` (all that
    the goals and `CheckNode` read).
  - The C++ storages keep their own per-tile id grids (continent, hill, choke point, path area, sector), which the
    managers index, and forward the rest; `mDirtyBounds` and `mHasBounds` became setters.
  - **Gate:** the six games' ASRN digests equal the C++ machine's (and seed 2's 15,055 records equal event by event),
    the sim oracle identical. It held at the first build.
- **Step 4, the goals, first ones.** The continent, hill and choke point goals' `AcceptTile` are Rust (`goals.rs`),
  asking the view through `EeAStarViewCallbacks`; the goal's counters go in and come back (the choke goal appends the
  returned serial). Their constant hooks (goal node, costs, limits) stay one-line C++ inlines. Not ported yet: the
  path goals (their modifiers first: they read units, regions and goal state), the path area and path sector goals
  (their `ClassifyTile` and side effects on `mCheckCorners`).
- **`long` is `i32` here** (the Windows build's). On LP64 the C++ costs (`unsigned long`) were 64-bit; the Rust
  machine wraps at 32 bits. The costs never come near that, but a Linux/macOS run is not proven equal by this gate.
- **Builds:** a full `build.py -j 3 --sim` took 27-28 minutes on this machine with other agents' builds running.
  For iteration a scratch wrapper skipped units whose object was newer than the unit and the A* headers it
  includes (4-6 minutes); every commit's gate used the full build.
- **Gates run:** `build.py -j 3 --sim` links 0/0 (EE-AOC, EE-SIM) at every commit; `rust_build.py gate` (one rerun:
  `ee_plat_sys`'s `suspended_thread_runs_only_after_resume` failed once under load, then passed); the sim oracle
  identical on seeds 2-7; `sim_capture.py check` PASS from step 2 on. Not run: the test-profile build, `gates.sh`,
  `eedata check`/`cppgate` (no data change), any macOS or Linux build.

## R5a.4-R5a.6: unit handles, named globals, the RTS layer's real surface (branch rust-r5a-handles, 2026-11-09)

### R5a.4 measured: who holds `EEComplexUnit*`/`EEUnit*`/`RTSUnit*`

Counted by regexes over comment-stripped source (a header belongs to the module of the `.cpp` of the same name;
the WON library left out). *member*: a class member pointer; *container*: a member container of them; *param*: a
parameter of a function definition; *local* / *local cont.*: a local pointer / local container.

| Module | member | container | param | local | local cont. |
|---|---|---|---|---|---|
| AI Thread | 24 (+5 in headers of no unit) | 17 | 312 | 408 | 193 |
| User Interface Thread | 4 (+2) | 1 | 25 | 169 | 29 |
| Graphics Thread | 5 (+1) | 1 | 84 | 75 | 3 |
| Communications | 0 | 0 | 0 | 95 | 14 |
| Scenario Editor Actions | 3 | 0 | 2 | 17 | 3 |
| Scenario Editor | 0 | 2 | 5 | 11 | 0 |
| UI Events | 1 | 0 | 0 | 15 | 1 |
| Actions | 1 | 0 | 2 | 16 | 1 |
| Empire Earth (EEWorld, the camera) | 2 | 5 | 14 | 32 | 0 |
| *for scale:* Units | 29 | 3 | 742 | 347 | 62 |
| *for scale:* Physics Thread | 5 | 1 | 166 | 39 | 1 |
| *for scale:* Player | 5 | 6 | 42 | 50 | 4 |

Outside Units/Physics, the members that live across ticks (where a freed pool slot can come back as another unit):

- AI Thread, 14, all referenced (`Reference`/`Dereference`): `EECPASTargetUnit::mUnit`, `EECPAttackSubGroupUnit::mUnit`/`mTarget`,
  `EECPAGTask::mUnit`, `EECPCivilizationManager::mGarrison`/`mFortress`, `EECPPersonnelRequest::Unit::mUnit`,
  `EECPResourceCenter::mUnit`, `EECPWMWall::mUnit`, `EECPWallManager::mFrom`/`mTo`, `EEHerd::mLeader`/`mTarget`,
  `EEPathManagerNode::mUnit`; plus the A* goals' `mUnit` (`EEAIAStarPathfindingGoalBase`, `EEAIAStarPathGraphGoal`,
  `EEAIPathGoalModifiers`: the A* branch's files). The 17 containers (`EECPUnitManager`, `EECPResourceManager::mMines`/`mFish`,
  `EECPWallManager::mGates`, `EEHerd::mUnits`, `EECPPersonnel::mUnits` (a multimap by id), ...) are the larger part.
- User Interface Thread: `EEUICPortrait::mUnit` (referenced). UI Events: none (the hot keys' entry is a local).
- Scenario Editor Actions: `EESEAMoveUnit::mUnit`/`mContainer`, `EESEAPlaceUnit`'s `mContainer` (undo records, referenced).
  Scenario Editor: `EEUIFScenarioEditor::mSelectedUnits`, `EESEUnit`'s `mCargo` (containers).
- Actions: `EEAUpgradeImprovement::mUnit`. Empire Earth: `EECameraControl::mUnitA`/`mUnitB` (referenced).
- Graphics Thread (left to rust-r5a-snap): `EEE3DMarchingLine::mFromUnit`/`mToUnit`, `EEE3DTracerBeam::mTarget`,
  `EEEffect::mUnit`, `EEUnitPicker::mUnit`, `mCarriers`.
- Communications holds none: its 95 locals live inside one message's Execute.

The other members are function objects built and dropped inside one call (`EECPBMBuildsTarget`, `EECPWMRunLess`,
`EECPAILoadGroup`, the `EEFO*` target finders). Parameters and locals stay pointers.

### The handle (`ee/EEUnitHandle.h`)

- **The id is the handle; no generation counter is needed.** A unit id is the player id in the top byte and a
  per-player counter (`RTSPlayer::AddUnit`: `mNextUniqueID`, 24 bits) below; ids are never reused within a game
  (a converted unit is a new unit with a new id, `EEConvertUnit`; a swapped unit too, `EEPhysics::SwapUnit` passes
  both ids to the scenario manager). So a freed pool slot reborn as another unit has another id, and a handle to the
  freed unit resolves to NULL.
- **The registry maps the id to the object for the object's life**, not for its player membership:
  `RTSRegisterUnit` at `RTSPlayer::InsertUnit` (every new, loaded or kept id passes there), `RTSUnregisterUnit` at
  `RTSUnit::FreeNode` (the last reference, the complex units' pool return, the destructor), and only if the entry is
  still that object. A killed unit that a holder still references resolves to itself, as the pointer did; that is
  what makes the conversion behaviour-neutral for the referencing holders (all the converted ones). Resolving through
  `RTSWorld::GetUnit` instead (the player's hash buckets) would have changed behaviour: a dead unit leaves the
  buckets at `RemoveKilledUnits` while its referencing holders still read it.
- `std::unordered_map<uint32_t, RTSUnit*>`, looked up only, never iterated: it orders nothing and draws no random
  number. It is touched by the same threads, under the same world lock, as the pool and the reference counts.
- `EEUnitHandle<T>` (T = `RTSUnit`, `EEUnit`, `EEComplexUnit`): 4 bytes, converts to and from `T*`, so a member's
  type changes and its uses read the same (`mUnit->X`, `if (mUnit)`, `mUnit = p`, comparisons). The compiler finds
  the places that need a decision: `USavedID(handle)` does not deduce, `a ? handle : ptr` is ambiguous,
  `dynamic_cast<X*>(handle)` is refused.
- **Saves are unchanged:** the save code writes `mUnit ? mUnit->GetUnitID() : -1` as before. The Load/Restore swizzle
  that kept the raw id in the pointer field keeps it in the handle (`EEUnitHandle::FromSavedID`, `GetSavedID`),
  and Restore resolves it through `gWorld.GetUnit` as before. **Trap:** a getter returning `T*` (`GetUnit()`,
  `GetLeader()`, `GetTarget()`) compiles inside `USavedID(...)` and silently resolves; three Restore passes read the
  saved id that way (`EECPAttackSubGroupUnit`, `EECPResourceCenter`, `EEHerd`) and were changed by hand (grep
  `USavedID([^)]*Get` after any further conversion). Between Load and Restore a handle resolves its saved id to the
  live unit of that id, where the pointer held a garbage address. A reader in that window (none in the converted
  classes' own Load/Restore; other classes not audited) would now see a real unit, and a destructor run after a
  failed load would dereference one where the original crashed. No saved game was loaded on this branch.
- A unit without an id (pending until `RTSPlayer::AddPendingUnits` at the tick's flush) has no handle: storing one
  prints `EE-HANDLE` once on stderr. None appeared in the six oracle games' logs.
- **Threads.** Only the physics thread (the AI runs inside its tick, under the write lock) resolves handles today, so
  the map has no lock. A unit's last `Dereference` can run on the UI thread under the read lock (`FreeNode` erases
  there), so before a UI, editor or graphics holder resolves handles the registry needs a lock, or the frees must
  move to the physics thread. That is why the UI's `EEUICPortrait::mUnit`, the editor's undo records and the camera
  stayed pointers here.
- **Converted:** the AI's 14 stored members listed above. Their classes shrink on x64 (an 8-byte pointer becomes a
  4-byte id); none is read by the harness (`game_state_probe.py` reads EEWorld, EEPlayer, EEPhysics and
  NESynchronousCommunications; `hbuild.py`/`exe_boot.c` hook no AI symbol).
- **Containers are next, and harder:** a `std::set<EEComplexUnit*>` or a map keyed by unit pointer iterates in
  address order; as handles it would iterate in id order. Each needs its order kept (a vector of handles where it
  was a vector) or proven irrelevant, as "R5b.1" did for the A* set.
- The handle holds no reference: a holder that referenced the unit still calls `Reference`/`Dereference` through it.
  Dropping those (weak handles, so that Rust can own units) changes when units return to the pool, which moves pool
  addresses and so any address-ordered container: a rebaseline of its own, after the address-keyed containers are
  gone.

### R5a.5: the address-named globals in the simulation modules

Named from what they do, the address kept in each comment (the R5a.5 commit message has the list): the damage
tables `gDamageTable`/`gDamageTableScaled1`/`gDamageTableRowCount`/`gDamageTableColumnCount` (`gU4E8708`/`870C`/
`8714`/`8718`), `gMapTileSize` (`gF52988C`), `UpdateFanaticismColor`, `SetRandomReloadTimer`, `EEUnitClass::UpdateDone`
(the empty hook every unit class's Update ends with), the AI's `RunsEveryManager`, `GetRandomElementIndex`, the
military aid package's `EnterState2/3` and `UpdateState2/3` (states nothing enters in a game; their meaning is not
known, so they keep numbers), `EEComputeTileCenterPoint`, `gHerdUnreadValue` (only cleared). Folded, a pure move:
`gU531CF0`/`gU531CF4` became `EEPhysics::mInputMessagesProcessed`/`mLastOutOfSynchChecksum` (appended, so the
harness's EEPhysics offsets are unchanged; gPhysics is made once and Init clears the count as before).

- A renamed record comment is a data-gate change: `EEDamageTableRecord`'s comment named `gU4E8708` and is copied
  into `tools/eedata/specs.json`, so `eedata.py specs` had to run (`check` reports it stale otherwise).
- Left: the names in Graphics Thread files (`Fn23CEE2`, the cloak opacity; `EEUnit::Fn27C0B4`;
  `EEComplexUnit::FnE123D`; `gB529065/66/68`), for the renderer branch; the UI, editor, WON and lobby ones
  (`gU5654E4`, the `gU531930..38` fonts, `EEWonNet::FnCFFDD`, ...), which are not simulation. `tools/datafmt` and
  DATA_FORMATS.md, generated once from old headers, still say `gU4E8708`. The x86-only differential harness looked
  up `?gU531CF0@@3JA` by symbol; its other 32-bit names do not resolve on x64 either.

### R5a.6: what the RTS layer really uses of the game

`llvm-nm --undefined-only` on the 11 RTS Engine objects, each symbol attributed to the module whose object defines
it: 146 into the LLE, 115 external (CRT, platform, Rust), and 26 into the game:

| Into | Symbols | What |
|---|---|---|
| Empire Earth | 12 | globals: `gWorld`, `gContinentManager`, `gDoppelgangerManager` (RTSMap); `gMouse`, `gUserInterface`, `gSoundManager`, `gConductor`, the thread names (RTSGraphics, RTSMousePointer, RTSPhysics); `gCommunications` (RTSPhysics) |
| WON lobby glue | 7 | `EELobby*` (RTSGraphics::RenderLobby) |
| AI Thread | 3 | `EEAIContinentManager::GetContinent`; `RTSMapTileRun`'s ctor and `kHeightEpsilon`, both defined in AI objects by the original's placement |
| Graphics Thread | 1 (2 refs) | `RTSMouse::EndFrame`: the RTS class's own method, placed in EEGraphics.cpp by its address |
| User Interface Thread | 1 | `EEUserInterface::PostMovieDone` (RTSGraphics) |
| Units | 1 | `EEDoppelgangerManager::UnregisterPlayer` (RTSMap) |

So "RTS -> Graphics Thread" is headers, not code: the RTS classes were declared in the game's headers (`RTSMouse`
in EEMouse.h; `RTSUnit` in EEUnit.h, which pulls EEEffectManager.h, EEGraphicalGoalManager.h and EEEffect.h).
Done here, each unit disassembled before and after (identical where no code was meant to change):

- `RTSMouse` (with `RTSMouseSaveUnder`) moved to `ee/RTSMouse.h`, `RTSUnit` (with `RTSUnitLink`) to `ee/RTSUnit.h`;
  EEMouse.h and EEUnit.h include them where the classes were. RTSMouse.cpp, RTSWorld.cpp and RTSUnit.cpp include the
  RTS headers instead. The handle's EEUnit/EEComplexUnit halves moved from RTSUnit.cpp to EEUnit.cpp. The
  differential harness's header parser (`UNIT_CLASSES`) reads RTSUnit from the new header.
- RTSMousePointer's thread read `gMouse` (an `EEMouse*`) three times per pass; it now calls `RTSGetInputMouse()`,
  declared by the RTS header and defined in Empire Earth.cpp (`return gMouse;`), read at each call as before.
- RTSMap.cpp dropped two includes it did not use (EEAreaUtilities.h, EETileLocation.h).
- Measured by a text include closure (quoted includes; a header belongs to the module of the `.cpp` of its name):
  RTS -> Graphics Thread went from 3 direct / 12 transitive edges to 1 / 4, RTS -> Units from 4 / 10 to 2 / 4.
  RTSGraphics.cpp still needs `gMouse` and so EEMouse.h; RTSPlayer.cpp still includes EEUnit.h, because EEPlayer.h
  types its unit lists `EEUnit*`. The plan's "146" was another count; it is not reproducible from the includes as
  they are now.
- Left: RTSGraphics.cpp (its lobby, movie and mouse uses; the renderer branch rust-r5a-snap works there), RTSMap's
  world, AI and doppelganger globals, `RTSMouse::EndFrame`'s placement in EEGraphics.cpp, RTSPlayer's `EEUnit*`
  lists.

### Builds and gates on this branch

- `build.py` recompiles every unit: about 25 minutes at `-j 3` with four other agents building. Between commits a
  scratch wrapper (not committed) skipped the units whose object was newer than the source and every quoted header
  it reaches; the per-commit EE-SIM link and sim oracle ran on that, the full `build.py` at the start and the end.
- A killed `build.py` leaves objects compiled from a mix of staged (old) sources and live (new) headers: `include/`
  is read in place, only `src/` is staged. Do not edit headers while a build of the same worktree runs, and delete
  the objects of a killed build.
- Gates run: per commit, EE-SIM linked 0/0 and the sim oracle (seeds 2-7, 2700 s) identical to the main checkout's
  `ref`; `eedata check` PASS after R5a.5; at the end the full `build.py --sim` (EE-AOC and EE-SIM 0/0, its EE-SIM
  identical on seeds 2-7 again) and `--test-profile windowed-software` (0/0; the harness's hooked symbols in its map).
  The records `game_state_probe.py` reads (EEWorld, EEPlayer, NESynchronousCommunications, EEPhysics up to its old
  end) and UIManager/UIForm/EEMouse/RTSMouse have the same layout.json offsets as before.
  Not run: the harness (tours, `gates.sh`), a saved game loaded, Linux or macOS.

## R5a.2: load equals continue (EE-SIM save/load, save version 0.253, 2026-11-09)

The owner approved changing the saved-game contents: a game loaded from a save must play exactly like the game that
went on. The test is `tools/sim_oracle.py saveload NAME --at 600,1350,2100`: per seed one game to 2700 s with
`EE-SIM --save-at MS=FILE` (the autosave's request, EEMLoadOrSaveGame through SendToAllAndWait, under a fixed name),
then one fresh process per save with `EE-SIM --load-save FILE` (the game's own Load Game screen: single player ->
Load Game -> the file -> Load) to the same end. The checksum lines are compared from the load on, aligned by game ms;
the CT number (EE-SIM counts from the process's first tick) and `inputs` (gU531CF0, reset by every load as in the
original; `oos` is compared less it) are left out.

- **Where a save lands.** The request is put into a CT; DrainInput executes it at the start of the next CT and
  ServeLoadSaveRequests saves right after, before that CT's ticks: the loaded game's first line is the CT after the
  save request's CT. The continued games with three saves each trace like the reference (`compare ref`: PASS), and
  the no-save oracle is unchanged at every commit.
- **The numbers.** Before: all 18 saves of seeds 2-7 diverged at the first tick after the load (rng, oos, units).
  After: still 18 failures, but five now hold for 2-7 ticks (seed 2 600 s and 1350 s, seed 6 600 s, seed 7 1350 s).
  The tick count is coarse: within the first CT the random draws matched for 0 of seed 2's draws before (the seed
  itself was another) and for 291 of its 752 draws after.
- **The tools of the hunt** (TEST ONLY, committed): `--dump-at MS=FILE` and `--dump-load FILE` write every unit's
  fields, each goal's own DumpOutOfSynch and the unit's raw bytes (build/clang/layout.json names the offsets;
  pointers differ by run), the loaded one at its first tick after the fix-ups. The random draws were compared with a
  temporary caller trace in URandom::Generate (RtlCaptureStackBackTrace, resolved by `tools/sim.py resolve`), removed
  again: the LCG gives the same values in both games, so what differs is the caller of draw n.
- **What a load rebuilt instead of reading, and what 0.253 saves** (`EESaveHasLiveState`, ee/EESaveGameFiles.h;
  older saves load as before):
  - the physics seed: rebuilding the players drew 23 numbers after the physics chunk was read (favourite unit types
    as the epochs are entered again, herds, patrol points, the lobby's player records); the first tick puts the
    saved seed back (no format change);
  - the four running values (civilization manager's mBuildingCountsChecked, the gather flags, the wall manager's
    plans and the wall being planned: Update asks for its path again; the defense commander's loose ids were already
    the live ones since R5a.2);
  - path points and town patrols: not saved; the towns made new ones from their hulls (other units, ids and, for a
    town whose patrol lagged its hull, other tiles). Now saved, and a rebuilt town adopts its saved patrol;
  - goals: the attack goal's destination and sidestep count; the build/repair goal's site (RepairPointers made it
    from the building's position: 61.85, 44.15 became 61, 43); an attack sub group unit's next update and pick;
  - a herd's area (RepairPointers rebuilt it from the animals' tiles, drew a new margin, picked the leader again);
  - each forest's next renewal: the continent manager is rebuilt at load with none, so the first tree cut regrew
    its forest at once;
  - unit fields the original never saved: animation fraction and next frame, collision radius, push velocity,
    mF24C, AI state, trigger time, ignored targets, bounded path tiles. The load's own rebuilding sets several of
    them again, so they go back at the first tick (`EEAfterLoad`, run once by EEPhysics::Tick);
  - the order of the world's unit chain (x-sorted, ties by arrival), of the stepped units and of the activation
    queues: a load linked units in its own order, so units stepped (and drew) in another order;
  - and, no format change: Load truncated the tick length into ms where UpdateTiming rounds (same at 62.5 ms).
- **Still different after a load** (first divergent draw of seed 2's 600 s save: #292 of the first CT, an
  EECPAttackSubGroupUnit::SetState draw from EEComputerPlayer::Update that the loaded game does not make): the
  computer players' own update schedule; every unit's area effect list (empty after a load until the area effect
  tick; 195 units at seed 2's 600 s save); references to units that are dead or gone (mLastResource, a gather goal's
  invalid resource: saved as -1, loaded as NULL where the running game keeps the dead unit); the continent manager
  and the AI's other derived state (the `ai` checksum parts differ at the first tick and agree after the first AI
  update); a herd's path request and the path manager's pending requests; the wall request is asked again at the
  first Update, not where it was in the queue. In-game times far off the default (game speed) are untested.

## GX on WARP: "Not enough memory left" in create_bind_group, the submissions in flight (2026-11-09)

Game sessions on WARP logged thousands of `wgpu error 2: ... In Device::create_bind_group ... Not enough memory left`,
then `BindGroup with '' label is invalid` in `set_bind_group`, and the view stopped presenting.

- **The cause was not a leak of GX's objects.** GX drops a texture's bind groups with its slot, as the C++ released
  them (`GXSurfaceData` destruction). The new "GX live" line shows GX's cached groups staying flat. The problem is
  that every submission keeps what it used until the GPU has finished it: bind groups and textures destroyed since,
  and the staging buffers of `write_texture`/`write_buffer`. GX never waited for the GPU. On WARP the CPU outran it,
  and the destroyed objects piled up: DrawText's temporaries, the window image and uploads. `Queue::submit` already
  runs a non-blocking maintain (wgpu-core 24 `queue.rs`), so it freed only the work already finished.
- **The limit is DX12's shader-visible sampler heap**: wgpu-hal 24 gives it 2048 descriptors
  (`dx12/device.rs` `capacity_samplers`), and every bind group with a sampler takes one, so "live bind groups"
  includes those waiting for submissions in flight. The view heap (1,000,000) is not the limit. wgpu-native 24 had
  the same heap and the C++ GX had the same lack of waiting, so the bug probably dates from before R4.1. It showed
  only when the harness's per-second `GXTestReadFrame` was absent: that read-back waits for its copy, which comes
  after all earlier work, so it brought the GPU up to date once a second.
- **The fix** (`ee_gx` `gpu.rs`): each submission gets an `on_submitted_work_done` flag. `submit` waits while more
  than 8 are unfinished, and a present waits for the work of the frame before the previous one (2 frames in flight,
  as DXGI's frame latency). The wait polls as the read-back does (`poll_until`, no blocking `Maintain::Wait`). If it
  times out after 10 s, GX logs that once and stops waiting.
- **The reproducer** `device_dropped_bind_groups_stay_bounded_when_the_gpu_lags` makes, draws with and destroys 24
  textures per frame on a 1024x1024 target. Without the bound, the live bind groups reach 2049 (the heap is full) at
  138-143 submissions in flight and wgpu panics. With the bound, the peak is 194 and the submissions in flight peak
  at 8. A non-blocking `device.poll` after each submit alone also reached 2049, so polling once per frame is not
  enough.
- **`EE_GX_STATS=<n>`** now adds `GX live: surfaces, cached bind groups; wgpu bind groups, textures, views, samplers,
  buffers; submissions in flight; presents (no image acquired)` (wgpu-hal's counters: the `wgpu` crate's `counters`
  feature, on in ee_gx). The first 8 `get_current_texture` errors are logged as `GX: no swap chain image`. These are
  the first things to read when a resource error appears or the view stops changing.
- **The live check** used `fixture_game.py --exercise random-game --play-seconds 300` with `EE_GX_STATS`, on the
  test-profile exe started directly (no debugger) on `tree-binary`. Over 300 s of gameplay it logged no wgpu error and
  no GPU wait timeout. The live counts stayed flat (wgpu bind groups 60-64, textures 57-61, buffers about 650,
  submissions in flight 2), presents rose about 10 a second, and no swap chain image failed to be acquired. With the
  bound switched off (a temporary switch, not committed), the same exe logged 730 "Not enough memory left" errors
  within about 20 s of gameplay.
- **Still open, a separate bug:** in these fixture runs, the window capture (PrintWindow) keeps the first gameplay
  frame, even after a camera zoom. This happens with and without the bound and with no wgpu error, while GX keeps
  presenting. `fixture_game.py`'s play check ("the window shows the same picture as gameplay") fails on it.

## Window captures on Windows: PrintWindow is not proof of presenting (2026-11-09)

The fixture driver (tools/harness/fixture_driver.py capture_frame) captured Windows frames with PrintWindow because
GX's read-back timed out on WARP. Both symptoms were the same bug: GX's unbounded GPU backlog (the section "GX on
WARP" above) made a read-back wait behind seconds of queued work, and PrintWindow on the flip-model swap chain kept
returning the first gameplay frame while GX presented ~10 frames a second. With the backlog bounded, the read-back
(frame.request -> frame.bmp) works on WARP, and the driver uses it on every system; the window capture is only the
fallback. A fixture tour on the binary tree then passed with gameplay and play frames differing and no GX errors.

## R5a.3: the renderer reads a snapshot, not the live world (2026-11-09)

RUST_PLAN R5a.3, a C++ refactor with no change to the simulation. `ee/EERenderSnapshot.h`,
`src/Empire Earth/Graphics Thread/EERenderSnapshot.cpp` (units.EXTRA_UNITS), branch `rust-r5a-snap`.

- **The original renderer already had a capture phase.** A frame (`RTSGraphics::Render`) is `FrameStarting` under the
  world's read lock and the UI copy lock (`EEGraphics::FrameStarting`, lock mode 0: the only mode the game uses;
  message 0x502 goes to the physics thread, not here), then `SceneStarting`, `RenderScene` and `FrameDone` with no lock
  while the physics tick runs. Each render object's `FrameStarting` copies what it draws: `EERenderObject::FillRenderLists`
  the render units and objects (`EERenderLists`, 0x200 units, 0x600 objects), `QueueEffects` each visible effect's draw
  state (`Queue`: `mDrawPhase`, `mDrawPosition`, the farm's ground copies), `UIManager::CopyData` the UI, and the terrain
  mesh its visible tiles, fog of war and water animation (`UpdateVisibleTerrain`, `UpdateFogOfWar`, `UpdateWaterAnimation`:
  buffers only the graphics thread writes). R5a.3 completes that design: the snapshot is taken under the existing lock
  at frame start, so the physics tick, its order and its random draws are untouched.
- **Measured, not guessed.** `build/snap/ast_reads.py` (scratch, not checked in) runs clang's JSON AST over each of the
  77 units (`Graphics Thread/*.cpp`, `RTSGraphics.cpp`, the build's own flags, `-fsyntax-only`) and keeps every member
  expression written in the unit's own file, with its enclosing function; the base's object (`EEUnit`, `EEPlayer`, ...)
  names the module. `llvm-nm` alone does not do: member reads are not symbols, and most accessors are inline. A trap in
  the JSON: the file of a location is printed only when it changes, in print order over `loc`, `range.begin`,
  `range.end` and their `spellingLoc`/`expansionLoc`, and an `includedFrom` object names the includer, not the
  location's file. The functions are then sorted by the thread and lock they run under:

  | Phase (thread, lock) | Simulation records read (members) |
  |---|---|
  | **render** (graphics, no lock), before R5a.3: 44 members | the terrain mesh (`RTSWorld::mMap`, `RTSMap::mTerrainMesh`) in EEROTerrain/EEROWater/EEROGroundShadows; the map's coloured rectangles, their lock and erase (`RTSMap::mColoredRectangles`, `mCriticalSectionForColoredRectangles`); the camera (`RTSWorld::mTransformation`, `mCameraYaw`, `mCameraPitch`) in 17 effects' draw passes and `AddGlowSprite`/`AddBlendedSprite`; `EEWorld::mInCinematic`; the live unit through `gWorld.GetUnit(id)` and `EEUnit::mHitPoints` (EEROUnits::DrawUnitModel, a robotic farm's second model); `EEUnit::mComplexUnit`, `EEComplexUnit::mIsCloakable/mIsCloaked`, `RTSUnit::mPlayer`, `EEPlayer::mEnemies`, `RTSPlayer::mPlayerID`, `EEWorld::GetCurrentPlayer` (EEE3DEnergyWave::DrawPass6); `EEUnit::GetTileX/GetTileY/mHitPoints/mComplexUnit`, `EEComplexUnit::mIsBuilt` and the unit's opacity (EEE3DFarm::DrawPass2, which also rebuilt a moved field in free camera); `EEMap::GetTile/GetGroundPoint` (EEE2DMarchingIcons::DrawPass6); `EEPhysics::GetMinutesAndSeconds` (EEE3DGoal::DrawPass3, a pure function of its argument). Static class data and media: `EEUnitClass::mDomains/IsLandBuilding/GetScale/GetInnerCollisionSphere`, `EEGraphicObject` models, textures, alpha references, `mBar`, `mIsDrawn`, `EEOrbital::mModel` |
  | **capture** (graphics, read lock): 177 members | everything FillRenderLists, IsUnitDrawn, GetUnitOpacity, FillUnit/FillObject/FillDoppelganger*, FillGhostRecords, QueueEffects, the effects' Queue and IsTileVisible, Fn10C036 (lights, wind, sun and moon, time of day) read: positions (`mPhysicalModel`), animation frames (`mNextFrame`, `mAnimationFraction`, `GetAnimation`), turret/orbital/wheel angles, hit points, power, cargo, selection (`EEPlayer::mSelectedGroup`, `mSelectedViewedUnit`, `RTSUnit::mIsEditorSelected`), owner colours (`RTSPlayer::mColor`), visibility (`EETile::IsSeenBy/IsVisibleTo`, doppelgangers), area effects, scaffolding upgrades, the tile unit lists, ghost units under `mGhostUnitsLock` |
  | **pick** (UI thread under the world's read or write lock; also the capture): 21 members | `EEUnitPicker`'s entries' units: owner, class, resource, hit points, tile, `mPreviousPart`, `IsAlly`, the selected group's size |
  | **physics** (the effects' `Step`, `SetUp`, `Load`, `RepairPointers`: run by `gEffectManager->Update()` inside the tick, `EEPhysics.cpp:754`): 145 members | not the renderer's: the effects live in the Graphics Thread folder but are stepped by the simulation |

- **What moved (four commits, one area each):**
  - *terrain and world flags* (`EETakeRenderSnapshot` from EEGraphics::FrameStarting; `EETakeTerrainSnapshot` from
    EEROTerrain::FrameStarting): the camera, `mInCinematic`, the terrain mesh pointer and its space flag, the coloured
    rectangles (copied back to front, the drawing order; the opacity-0 erase markers are erased there under the map's
    lock, where RenderScene erased them a moment later) and the editor's brush highlight. EEROTerrain, EEROWater,
    EEROGroundShadows and EEROUnits read the snapshot.
  - *units*: `EERenderUnit::mSecondFullOpacity`, set at the end of FillRenderLists with DrawUnitModel's own lookup.
    The doppelgangers' render units keep their pool slot's stale `mUnitId` (FillDoppelgangerUnit never set it), so
    the lookup is the same expression on the same id, only earlier.
  - *effects*: 17 effects' draw passes and the two sprite helpers read `gRenderSnapshot`'s camera (checked: the helpers are
    called only from draw passes; no `Step` was touched). EEE3DEnergyWave (`mDrawHidden`) and EEE3DFarm
    (`mDrawOpacity`, `mDrawShown`, and the free camera's rebuild of a moved field, kept after the ground copies as
    before) take their unit reads in `Queue`.
  - *picker*: `EEUnitPickerEntry` holds the unit's id; the pickers resolve it with `gWorld.GetUnit` (the player's
    id buckets) under the caller's world lock. FillRenderLists picks with `PickUnitUnreferenced`.
- **Reference counts.** Before: every frame the graphics thread `Reference`d every drawn unit and the other parts of
  its chain (`AddUnit`, `ReferenceCarriers`), released last frame's (`Clear`), and referenced the unit under the mouse;
  the UI thread did the same in PickUnit, both under the read lock, so two threads changed the same non-atomic counts,
  and the last release could free a unit on the graphics thread. After: the renderer's own code takes no unit
  reference. Still shared: `EEUIFMainGame::CopyData` (the UI's copy, run on the graphics thread) calls PickUnit, which
  references and releases a unit; the effects' own reference counts (QueueEffects references each queued effect under
  the lock, so an effect the tick releases stays alive while it is drawn); the media counts `EEGraphicObject::LoadMedia/
  UnloadMedia` (FillRenderLists under the lock; `EEReleaseScene`'s `UnloadRenderUnits` outside it, unchanged).
- **One UI difference (picking only).** An entry whose unit has left the world since the frame is skipped
  (`GetUnit` gives NULL); the reference used to keep it pickable until the next frame. A remembered building
  (doppelganger) whose unit is gone from the player's id table is no longer picked. Ids are unique, so a reused pool
  slot can never be picked as the old unit. Units are also freed sooner (no frame's reference): the slot reuse order
  can differ in the windowed game; the sim cannot see it (EE-SIM draws nothing, so it never held these references), and
  lockstep games already depended on it being harmless, since the old references depended on the frame rate.
- **Still read live after FrameStarting, and why:**
  - *the effects' own state.* The tick steps every effect (`Step`) while the render phase draws it (`DrawPass*`):
    particle arrays, positions, ages written by physics, read by the draw. Queue copies some (`mDrawPhase`,
    `mDrawPosition`, `mDrawAge`), most effects draw from their live members. Moving it needs a draw record per effect
    class (~60 classes); a race of the original game, not new.
  - *the terrain mesh.* Its render buffers are the graphics thread's own, but heights and water info are the map's:
    an elevation change (calamities, editor) under the write lock can meet a draw. `GETerrainMesh` is a GE class
    (rust-r4-ge2's files), so it is drawn through the snapshot's pointer, not copied. `EEE2DMarchingIcons::DrawPass6`
    reads ground heights the same way.
  - *static data*: `EEUnitClass` (db tables, unchanged during a game), `EEGraphicObject` models and textures (media,
    loaded and unloaded under the lock), `gUnitClassTable`. `EEE3DGoal::DrawPass3` calls `gPhysics->GetMinutesAndSeconds`
    with its own time (no state read).
  - *the see-through flags.* `GetUnitOpacity` (capture, read lock) calls `EEUnit::UpdateHidden`, which writes
    `mHiddenFlags` on the units and on the obstacles beside them: presentation state stored on simulation objects,
    also written by `EEUnit::SetTile`. Nothing in the simulation reads the flags but UpdateHidden itself. Moving it
    needs a renderer-side table keyed by unit id.
  - *the UI's CopyData* (UI module; R5a.4's handles branch works there): run in FrameStarting under the lock, it
    reads units, players and the picker for the panels, the cursor and the gate preview.
  - *the minimap* is not the renderer's: the simulation draws it on the CPU into a surface (`EEMiniMap::MoveUnit`,
    `TellTile`, ...) and the UI draws that surface.
- **After R5a.3** the same measurement over the render phase leaves 23 members: the static class and media data above,
  the terrain mesh through the snapshot's pointer, `EEMap::GetTile/GetGroundPoint` (EEE2DMarchingIcons::DrawPass6) and
  `EEPhysics::GetMinutesAndSeconds` (pure); no unit, player or physics state. No unit `Reference`/`Dereference` is left
  in the renderer's units (before: `EEUnitPicker::AddUnit`, `ReferenceCarriers`, `Clear`, `FillRenderLists`); `PickUnit`
  still references for its UI callers.
- **Gates run.** At the last commit: `build.py -j 3 --sim` (EE-AOC and EE-SIM) and `--test-profile windowed-software`
  link 0/0; the sim oracle, seeds 2-7 over 2700 game s, identical to `ref` (the unmodified branch's EE-SIM matched `ref`
  as well). At each intermediate commit only its changed units were compiled alone (`clang_build.py --only`); the first
  commit's full build found a missing include in the new unit (fixed in that commit). No tour was run: the harness was
  in use by another session, and a comparison needs a tour of the unmodified branch too. Visual equality is unchecked.

## R6: WON's socket layer and ops in Rust (ee_won, branch rust-r6-won2, 2026-11-09)

`ee_won` now holds WON's network layer below the ops' objects, on the platform layer's Rust sockets
(`ee_plat_sys::socket`, called as Rust, not through the C ABI): the status names (`status.rs`, WONStatusToString's
table), NetStats (`netstats.rs`, atomics), IPAddr's parsing, lookups, text forms, order and the local address lists
(`ipaddr.rs`), AsyncSocket's core (`socket.rs`), and the ops' algorithms (`ops.rs`: an op's time left, the message
length prefix, the receive chunk, the send loop of SendBytesOp and the CR-LF loop of RecvCRMsgOp). The C++ classes
stay the types callers use; `NetStats.cpp` is gone.

- **`SendMsgOp` crashed in the clang-built game.** It constructs `SendBytesOp(NULL, ...)`, whose initialiser called
  `inData->data()`; `ByteBuffer::data()` / `length()` test `this` for NULL, which is undefined behaviour that clang
  folds away, so the op read through NULL (the original compiler kept the test). LanLogic sends every TCP message
  (to the captain, to the game, its join replies) with `SendMsgOp`, so a LAN game through the lobby crashed at its
  first message. The constructor now tests the pointer itself. Other `this ? ...` getters called on NULL are the same
  hazard (not audited).
- **The socket core is a `#[repr(C)]` struct in `AsyncSocket`** (`EeWonSocket mCore`: the handle, type, connected and
  open flags, status, last receive time and first blocked send time; the times are 64-bit now). The C++ keeps the
  lock (`mDataCrit`; the Rust functions expect it held, as the C++ held it), the virtual open / close / kill hooks
  (so `Open`, `Bind`'s implicit open, `SendTo`'s implicit open and `Disconnect`'s close stay C++ around the Rust
  calls), the destination `IPAddr`, the transforms and `Duplicate`. Users of the old members: the `Duplicate`s
  (`mCore.kind`) and `HTTPGetOp::CleanupHook` (`mCore.status`).
- **Ops that loop on a socket lock per call.** `ops::send_all` and `ops::recv_line` take the socket's core and its
  `PlatMutex` (`mDataCrit.mCrit`) and lock around each send / receive, the wait without the lock, as the C++
  `WaitForWrite` + `Send` did. The CR-LF test carries the line's last byte in from the C++ buffer, so a CR and LF split
  between two `Continue` calls still end the line.
- **`atoi` is the original's (MSVC):** a port text past `int` saturates (glibc's `(int)strtol` would differ for values
  between 2^31 and 2^63). `inet_addr`'s failure value is 255.255.255.255, so that address stays unparseable without a
  lookup, as before.
- **cbindgen:** modules with only Rust callers are `/// cbindgen:ignore` (status, netstats, ipaddr), and so are the
  socket's and op's constants: a `pub const` of a Rust type alias or of `ee_plat_sys`'s constants lands in
  `ee_rust.h` as an undeclared name and breaks every unit (the first full build here failed on it).
  `ee_plat_sys`'s `socket` and `thread` modules are public for Rust callers (`missing_safety_doc` allowed there: each
  function's contract is its C header's).
- **Golden first:** `tests/won/won_ops_test.cpp` (new, `xbuild.py wontest won_ops_test`) ran over the original C++
  units before any forwarder: BlockingSocket's Connect, SendMsg / RecvMsg with 1-, 2- and 4-byte length fields and the
  2-byte 0xffff escape, bad lengths and field sizes, a receive timeout, RecvCRMsg, RecvBytes (counted and until the
  peer closes), a 3 MB SendBytes to a reader thread, datagrams, refused and invalid connects; then the same ops async
  through WONAPICore (the op queue, the socket thread, the timer thread: a timeout, Kill, RunAsTimer) and
  WONStatusToString. The status table was printed by the C++ for every code from -40000 to 120000
  (`golden_status.tsv`). Both, and won_socket_test, pass over the forwarders. `wontest` takes program names now.

What the harness's `--exercise lobby` reaches of this: WONAPICore's start (StartWinsock, the socket and timer
threads, the op queue pumped by the lobby), InitLogic's `HasInternetConnection` (when the lobby config checks it) and
`SetLocalHost` (the chosen adapter), and on the LAN screen LanLogic's discovery: a UDP `QueueSocket` bound to the local
host's broadcast port (`Fn2B0D90`), the repeat `RecvBytesFromOp` on the socket thread, a broadcast `SendBytesToOp`
and the 15 s discovery timer (`RunAsTimer`). It does not reach: TCP between two instances (`LobbyLanSocket`'s
connect, accept, `SendMsgOp` / `RecvMsgOp` with 2-byte fields), peer auth, GetLocalAddr's join reply, the gone
servers' ops (directory, routing, DB, HTTP's `RecvCRMsgOp`, FTP). Those are covered only by won_ops_test over loopback
in one process.

## R5b.1, continued: the path goals, their modifiers and the area fills in Rust (ee_astar, branch rust-r5b-astar2, 2026-11-09)

The order of "R5b.1: the tile A* searches in Rust" held: every tile goal's hooks are Rust now. `ee_astar` gained
`modifiers.rs` (the path goals' modifiers' costs and tile tests), `path_goals.rs` (`EEAIAStarPathfindingGoal` and
`EEAIAStarBoundedPathGoal`: `AcceptTile`, `GetCost` with `GetDefaultCost`, `IsGoalNode`, `SearchDone`) and
`area_goals.rs` (`EEAIAStarPathAreaGoal` and `EEAIAStarPathSectorGoal`: `AcceptTile` with `ClassifyTile`).

- **The modifiers read through a quiet view first (C++, own commit).** `EEAIPathGoalModifierBase::GetStepCost` and
  `IsPassable`, `EEAIBPGModifierEnemyIsAttackableByUnit` / `WallsDoNotBlock` / `EEAIGPGModifierComputerPlayerWallPather`
  `CanEnter` and `EEAIBPGModifierStormShaper`'s two slots ask an `EEAIAStarView(gWorld.mMap, true)`. A quiet view
  does not note its answers: the goal's `CanEnter` / `GetModifierCost` query stays the one noted fact, so the ASRN
  digests (recorded from the C++ before any of this) still gate the modifiers bit for bit, and adding queries did
  not change them. New queries: `HasStepBlocker` (the old `IsSeaBlocker` / `IsLandBlocker` with the walker as an
  argument), `GetUnitRegionState` (-1 none, 0 empty, 1 with units: GetStepCost's five-region short cut),
  `GetEpoch`, `IsAlone`, `GetWaterState`, `IsSeaGoing`, `ArePassableExcept` (walls, trees), `HasHill`,
  `RandomAtMost` (the storm's draws from the physics stream: one `Generate()` per call, in the C++'s order).
- **The modifier classes stay C++** (the goals reference them, `SetModifier` `dynamic_cast`s the bounded one, the
  wall manager and the storm `new` them and set their members); each slot copies its state into a plain struct
  (`EeAStarStepGoal`, `EeAStarStorm`) and calls `ee_astar_mod_*`. The base class's default slots, the wall
  modifiers' 10/14 cost and the enemy modifier's `CheckNode` (goal state and one float compare) are one-liners and
  stay. The graph modifiers (route planners) are untouched.
- **A Rust goal calls a C++ modifier that calls Rust.** `path_goals::accept_tile` asks `view.can_enter(modifier)`;
  the C++ view calls the modifier's virtual, which runs `modifiers::*` over its own quiet view. Nothing Rust is
  borrowed across that call (the goal's state is a copy, the modifier's another), so the rule "no Rust reference
  across a call back into C++" holds.
- **Goal state is copied in for every hook, never cached.** `EEAIBPGModifierEnemyIsAttackableByUnit::CheckNode`
  moves the bounded goal's `mGoalTile` from inside `IsGoalNode`, after the Rust part of `IsGoalNode` returned; the
  next hook reads the new tile. `SearchDone` passes `GetMaxRevs()` along: a goal with `mMaxRevs` -1 fails every
  search that does not end at the goal (`revs >= -1`), kept.
- **The area fills' side effects come back as out fields.** `mCheckCorners` (the C++ wrote it through
  `dynamic_cast<EEAIAStarMachineBase*>(mStorage)`, unchecked in the sector fill) is the fill's last write, -1 for
  none; the machine reads it only after `AcceptTile` returns, so applying it at the end is the same. The sector
  fill's stale tile is a flag the C++ turns into the `mStaleTiles` push.
- **The graph searches are captured now (ASGR, `ee/EEAIAStarCapture.h`).** One record per
  `EEAIAStarGraphMachine::Run`: its inputs (sectors, tiles, limits, node count), the digest of its questions to the
  goal (start object, estimates, goal node, `CanEnter` per neighbour, step costs, `SearchDone`) and its outputs (the
  machine's members, the goal's flags, the current and path nodes, the closed list, both hash tables, the sorted and
  the fast open list). Graph nodes are named by index, never by address. The six games make 14,992 graph runs
  (3,555 / 2,546 / 2,155 / 1,844 / 2,641 / 2,251 for seeds 2-7); two captures of the same build are equal, the
  other 24 digests did not move, and the golden lines were recorded with the graph machine, storage, goals and route
  planners unchanged. Porting them is next: their goals and route planners take `EEAIAStarGraphNode*` (`mObject`,
  `mIndex`, the parent chain), so a Rust storage needs the stack-node trick of the tile `IsGoalNode` or node views.
- **Two builds share one checkout's data tree.** A sim run failed at start (`EETerrainColorTable.cpp (81)
  Verification failure`) while the main checkout's `build/open_data/tree` was being rebuilt by someone else
  (`open_data.json` missing): not the code. Wait for `open_data.json` before trusting a run.
- **Gates run:** `build.py -j 3 --sim` 0/0 for the C++ view step; the later steps were built with a scratch
  incremental wrapper (objects newer than their include closure kept), then the full build after the merge of
  `rust`; the sim oracle identical on seeds 2-7 and `sim_capture.py check` PASS at every step; the ee_astar tests
  (37: 15 new for the modifiers, path goals and area fills, each pinning the order of the view queries).

## R6: buttons, check boxes, radio buttons, scroll bars and combo boxes in Rust (ee_ui button.rs, scrollbar.rs, combo.rs, 2026-11-09)

The logic of `UIButton` (the normal/focused/pressed state machine over the mouse, the keyboard and tabbing,
`SetLocation`'s text rectangle, what `CopyData` shows: the bitmap for the state and the centred text), `UIDualButton`'s
right-button handlers, `UIScrollButton::ButtonDown` (the step, the 500 ms delay, the 100 ms repeat), `UICheckBox` and
`UIRadioButton` (check, uncheck, the neighbour run, the disabled box, the box rectangle, the bitmap offset, the label's
place), `UIScrollBar` (the constructor's layout, the mouse routing to the part under it, `SetLocation`, `IncrementBy`)
and `UIComboBox` (where the list opens, the popup and the capture, the mouse and keyboard routing, the selection,
`AddString`, the line edits' reopen, `SetLocation`, `ChangeWidth`, `CopyData`'s focus highlight) moved to
`rust/crates/ee_ui`. The C++ classes keep every member, vtable and exported name.

- **Access without new friends where possible.** `UIButtonRust.h` (a friend struct in `UIButton.h`, `UICheckBox.h`,
  `UIRadioButton.h`, `UIScrollButton.h`, `UIDualButton.h`) and `UIScrollBarRust` (friend in `UIScrollBar.h`,
  `UISlider.h`, `UIScrollButton.h`) hold the host calls. The combo box uses a **nested** struct, `UIComboBox::Rust`
  (declared public in `UIComboBox.h`, defined in `UIComboBox.cpp`): a member of the class has its friendships, so it
  reaches the parts' protected handlers (`UIControl` befriends `UIComboBox`), `UITextLabel`'s private
  `SetText(text, true)` and `UIListBox`'s `mLineHeight` without touching those headers. A protected static such as
  `UIControl::sSpaceKey` must be named through the befriending class (`UIButton::sSpaceKey`), not `UIControl::`.
- **A friend of the derived class may call the base's protected members only through the derived type.** The scroll
  bar's host calls name `UIScrollButton*`/`UISlider*`, not `UIControl*`; `UITextLabel::SetText(text, bool)` is
  private, its public one-argument overload is the same call.
- **Records.** `EeUiButtonFields` (from `mTextRectangle`), `EeUiCheckFields`, `EeUiRadioFields` (the subclasses' own
  members, passed as `sub` in `EeUiButtonRefs`), `EeUiScrollBarFields`, `EeUiComboFields` (from `mTextBox`, after the
  `GERhombus` member, so the rhombus's layout is not pinned). Offsets are `static_assert`ed where the refs are built.
  `ee_ui_button_init` writes the record whole: reading the not yet constructed members as a Rust struct with `bool`s
  would be undefined behaviour.
- **Unsigned long arithmetic.** `UIsNone32` looks at the low 32 bits (a -1 in memory has 64 on LP64); the radio
  button's `2 * inBorderThickness` is `unsigned long` arithmetic (wrapping on 32 or 64 bits), the check box's
  `2.0f * border` a float. `OpenListBox`'s `window height - floor(bottom) - 1` is unsigned arithmetic assigned to a
  `long`. Kept per system.
- **Text.** `UWideString`'s `operator==` (`_mbscmp == 0`, `AddString`'s search) and `GetLength() == 0` are
  `text::equal` and `text::is_empty`.
- **Golden drivers with fake classes.** The button driver links the five real units over fakes of what they reach
  (`UIControl`'s out-of-line members, `UIForm::SetTabStop`, the sound manager, the keys as an `ISEntity` subclass,
  `TMPoll`, `UISlider::SetCurrentRangeValue`, `UITextLabel`'s constructor and members) and dumps 12 controls under
  seeded operations, the bitmaps' reference counts included. The combo driver goes further: it predefines the header
  guards of `UIListBox.h`, `UILBString.h`, `UIEditBox.h` and `UITextLabel.h` and supplies its own classes with the
  member names the unit uses, so the parts are traced fakes and no real list box (GE objects, rasterizer) is built;
  `GERhombus` gets a fake `GEVertexArray` and `GERasterizer` a calloc'ed screen state with a 600-pixel window. One
  driver artefact: a fake list whose selection could pass its lines made both builds read past the vector (different
  garbage); the fake now keeps the selection inside the lines, as the real list does.
  A C escape written inside a Python string in a bash heredoc reached the driver as a real line break: write C
  sources with the editor.
- **Goldens:** `golden_button.txt` (1,500 ops), `golden_scrollbar.txt` (4 bars, 2,000 ops), `golden_combo.txt`
  (5 combo boxes of both text box types, 2,000 ops). The drivers over the forwarders and the new `ee_rust.lib` print
  the same files and equal the originals on 6 seeds x 4,000-5,000 ops each (buttons, scroll bars) and 8 seeds x 5,000
  (combo boxes). No behaviour change.
- **Still C++ in R6's toolkit:** `UITriStateCheckBox`, `UIBlinkingButton`'s fading, `UIScalableButton`'s three-part
  draw, `UIControlsTable` (forms from the db), `UIManager` (form stack, event dispatch), every `DrawPolygons`/
  `DrawTextA`, the edit box's key dispatch and repeat timers.

## R5b.2: the combat and unit formulas in Rust (ee_combat, branch rust-r5b-combat, 2026-11-09)

`rust/crates/ee_combat` holds the per-unit formulas of `src/Empire Earth/Units`, `Physics Thread` and the unit
utilities in `User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp`. The C++ methods keep their signatures and
their world access; each fills a plain record (`EeCombat*In`, `ee/EECombatFormulas.h`), calls Rust and acts on the
result. Moved, with their callers:

| Formula | C++ caller(s) | Tag |
|---|---|---|
| damage of a hit (direct, splash, trample) | `EEUnitClass::ComputeDamage` (ApplyHit, ApplyAreaDamage, `EEPhysicsModel`'s trample) | CBDM |
| the attack factor's difficulty scale | `EEToHitTable::GetAttackFactor` (ComputeDamage; the AI, the UI target screens) | CBAF |
| elevation factor | `EEUnitClass::GetElevationFactor` | CBEL |
| morale | `EEComplexUnit::ApplyMorale` (TakeHit) | CBMO |
| a hit's hit points, badly hurt; the damage effect threshold | `EEUnit::TakeHit` | CBTH, CBDE |
| regeneration | `EEUnitClass::Regenerate` | CBRG |
| garrison healing (timer, per unit), the heal goal (clock, heal) | `EEUnitClass::HealGarrison`, `EEUGHealUnit::Work` | CBTM, CBHL, CBHS |
| heal range | `EEUGHealUnit::IsInRange` | CBHR |
| conversion cost; the conversion clock and chance | `EEGetConversionCost`; both priests' `UpdateConversion` | CBCC, CBCV |
| ballistic pitch | `EEGetBallisticPitch` (LaunchProjectile, the attack range checks) | CBBP |
| a projectile's arc | `EEPMProjectile::Step0` | CBPA |
| range to hit; contact, work and gather reach | `EEIsInRangeToHit`; `EECanReach`, `EEIsInWorkReach`, `EEIsInGatherReach` | CBRH, CBRC, CBRW, CBRT |
| the damage table rows and their difficulty twins | `EETerrainTypeTable::LoadDamageTables` | CBDT |

- **The gate is the capture, as for formations.** Every call appends its input record and its output
  (`EECombatCapture`). The golden lines in `tools/sim_capture/golden_cpp.txt` were written from a build where the same
  records went through C++ copies of the formulas (the commit before the switch; those copies were the original
  arithmetic moved into the header, and the sim oracle stayed identical with them). `sim_capture.py replay` runs
  `ee_combat`'s `combat_replay` over every record, byte for byte; `check` then gates the Rust build's own capture.
- **The AI games never heal, regenerate or convert** (CBHL, CBHS, CBTM, CBRG, CBCV, CBHR were absent from the
  six games, CBRH and CBCC in one each). As for formations, EE-SIM's capture sweep (`EECombatCaptureSweep`, every five
  game minutes) calls those formulas over each player's newest 64 living units and their neighbours: real stats,
  distances, timers and conversion factors, plus fixed variations (timers 0/tick/1000, full resistance with and
  without adaptation, dead units). It reads the world only; the traces stay equal with it. The healing and
  conversion paths themselves (which unit heals whom, the draw) are still not exercised by any game gate.
- **What stays C++ and why.** The draws (`URandom::Generate` in the conversion attempt), the effects, events and
  sounds, the target walks (`ApplyAreaDamage`'s tile loop), `GETransformation` in the arc, and the table storage:
  `gDamageTable` and its twins are indexed directly by the AI (EECPASG*, EEHerd, EECPAttackSubGroup*), the messages and
  the UI, which this branch does not touch; only the rows' arithmetic moved.
- **Lazy inputs stay lazy.** The original read some inputs only on one branch: the attack factor not for a direct hit
  without attack, the elevation and the fanatic flag only for a damaging hit (`ee_combat_hit_factor` lets the C++
  decide that), the adaptation power only at full resistance, the projectile's own launch speed only when the
  launcher's is zero, a crusader's heal range only for a crusader. Reading them eagerly would index
  `gDamageTable` at a projectile's -1 ammo column or dereference a missing launcher class.
- **Two C++ conversions to keep**: `(long)(tick_seconds * -1000.0f)` is a truncation (`ee_combat::truncate`, with
  x86's indefinite value, not `as`), and `UMCeil` compares the rounded `long` with the float as a float.

## R5b.2: the tech tree, the event effects and the calamities' age in Rust (ee_tech, branch rust-r5b-tech, 2026-11-09)

`rust/crates/ee_tech` owns the decisions and the arithmetic of the tech tree (`src/Empire Earth/Tech Tree`), of the
event effects (`Scenario Manager/EEEvent.cpp`) and the calamities' shared timing (`EECalamityClass::UpdateTimeAlive`).
The C++ classes keep their members, vtables and object lifetimes; they fill plain views (`ee/EETechRust.h`: a node,
the tree's epochs, an epoch's research counter, an effect's value/offset/factor) and apply what comes back, firing the
events and refreshing the interface in the C++'s order.

- **Moved:** a node's and an epoch's `GetIsAvailable`/`GetIsObsolete`, `SetPrerequisite`'s flags, `ResearchNode`'s and
  the epoch's research decision and the research delay, `AddSOL`/`UpdateAvailability`, `EETechTree::Research`'s
  epilogue, `GetCost` (a node's arithmetic, an epoch's scale), the per-class cost tables and prerequisite flags at
  `Load`, `GetChecksum`, `DependsOn` and `SumCosts` (walks over callbacks into the C++ tree: the C++ is asked in the
  C++'s order of calls, the costs are copied in and out so no Rust reference lives across a callback), both
  `EEApplyEventEffect`s (now forwarders), which stat an attribute effect changes (`ee_tech_attribute`: the switch of
  `ApplyEventAttribute` is on its operation), the maximum hit points, the hit point and flight time rescale
  (`UMCeil`), the gather times, the population bonus, the terrain masks, `RunEffect`'s dispatch, and the calamity's
  age. Kept C++: the effects that touch units, media, sounds, buttons and the interface; `ResearchToEpoch`, `Unlock`,
  `ResearchAll` and the saved-game code (orchestration of member calls); every calamity class's own `Update`,
  `Start`, `CanCastAt` and the contagion helpers.
- **The C cast.** `(long)f` is `cvttss2si`: toward zero, `i32::MIN` for NaN or out of range. `ee_tech::ftol` keeps that
  (`as i32` saturates); `UMCeil` is `lrintf` (ee_ge) plus one when below.
- **The capture first** (`ee/EETechCapture.h`, `EE_SIM_CAPTURE`): research starts (`QueueResearch`), the tree's and the
  nodes' research, prerequisites, epoch counters, every `GetCost` (7.8 million in the six oracle games) and `SumCosts`,
  every effect with the upgrade's numeric fields and the player's before and after, every rescaled unit, calamity
  creation, start and update, and a sweep of every player's tree every five game minutes (availability, obsolescence,
  costs at three scales, `SumCosts`, the whole `DependsOn` closure, the checksum). Digests from the unmodified C++ are
  in `tools/sim_capture/golden_cpp*.txt`; two captures of one build are identical and the capture leaves the six
  traces equal to the reference.
- **What the capture found.**
  - An epoch's `mPrerequisiteFlags` is never set: uninitialized memory (two runs of one build differed there). Nothing
    reads it (`SetPrerequisite` is only called on an epoch's nodes); the capture records 0 for epochs.
  - `SumCosts` recurses through its own public entry; the capture records only the outermost call, so a walk inside
    Rust gives the same records.
  - The oracle's computer players never leave the first epochs (41 effect runs in a seed-2 game) and never cast a
    calamity, even from a later start epoch with every cheat (`--profile late`, `space`: they stay in their start
    epoch with almost no army). So the capture has three more profiles: `late` (3 AIs from the Middle Ages, cheats),
    `space` (from the Atomic Age), and `calamity`: the oracle's game with `EE-SIM --calamities` (TEST ONLY), which
    every five game minutes has each computer player cast an eighth of the calamity classes through
    `EECalamityManager` (caster its first living unit, the next player's first unit or its tile the target). All 20
    classes the game has (enums 3..26) start, update and end; 176 calamities, 166,320 updates in seeds 2-4.
- **Gate:** `sim_capture.py check` equal to the C++ digests for the oracle's six seeds and the profiles' seeds (late
  2-3, space 2-3, calamity 2-4); `sim_capture.py replay` (the Rust decisions recomputed from the C++ captures: TNRS,
  TNPR, TSOL, the sweep's availability, closure and checksum, EVHP) found no difference before the C++ was replaced;
  the sim oracle identical.
- **Not ported (no oracle):** `CanCastAt` (only prophets and the interface ask it, and no captured game casts through
  it), the calamity classes themselves (their `Update`s are world code), `GetUnitClassMediaMask`/`IsLatestEffect`.

## R4.3, third part: GEModel's reader, writer and geometry (ee_ge model_file.rs, legacy.rs, model_save.rs, model_geom.rs, 2026-11-09)

`GEModel::Load` (both `.cem` formats, the sub-models), the version-1 conversion (`ConvertLegacyModel`, so
'GEModel Legacy.cpp' is gone), `GEModel::Save`'s chunk writers and `GEModel.cpp`'s geometry (`ComputeBoundingBoxes`,
`ComputeBoundingVolumes`, `ComputeModelNormals`, `PrepareTransforms`, `RemoveModelScale`'s matrices, `SortTagPoints`'
order, `Verify`'s test) are Rust. The model, its arrays (`GEVertexArray`/`GEPolygonArray` virtuals), the materials,
the allocation (`AllocateInternalData`), the lifetimes and `GEModelManager` stay C++.

- **Simulation reach.** `GEPhysicalModel::Intersects` (projectile hits, `EEUnitClass.cpp`) reads the loaded vertices,
  so the reader feeds the simulation, but it does no float arithmetic: the bytes it stores are the bytes read. The
  geometry's callers are the graphics thread, the UI and `Save`; its `f32` order is the C++'s anyway. The sim oracle
  is identical.
- **Decode, then store.** Rust reads the whole model into a plain `CemModel` through the field stream (now with
  bytes and the position: `GERustFieldIO(FSFileSpec&)`), and `GEModel::StoreLoaded` allocates from the counts and
  stores what was read. A load that fails keeps what the C++ had stored up to the failing field: every list in
  `CemModel` is that prefix (vertices whole or not at all, a transform's or box's values one by one, a material's
  values in the order the C++ set them). This matters: **a failing sub-model stays attached** (the parent ignores
  sub-model results), its partial arrays included, and the file goes back to where that sub-model started, so a
  file cut in its last sub-model loads "successfully". The assumption is that a failed read stores nothing, which
  is `FSMemoryFile`'s rule; every model load reads from one (`Load` copies any other file first).
- **The shared frames** (`ioTextures`, filled by `EEGraphicObject` for animations): a shared frame's vertices and tag
  points are read into the *last* frame's arrays, and its arrays are the neighbour's (`AllocateInternalData` in frame
  order). The reader records the target frame; the store writes there. An offset to a later shared frame read a
  pointer the C++ had not set yet (never in the data).
- **The header version was a global** (`sMajorVersion`/`sMinorVersion`, read by the legacy chunks and overwritten by
  every sub-model's header). Nothing reads it after a sub-model, so it is a local now.
- **`ConvertLegacyModel`'s quirks are kept:** the version-1 materials never get names, so the groups are named "" or
  "player color"; a polygon listed twice in a group is placed twice (the second time past the polygon array's end, a
  write the C++ made past its array and Rust drops); a vertex shared by two materials is copied into both. Reads past
  an array's end (the C++'s undefined behaviour) give 0 in Rust.
- **`Read(bool&)`** of a byte that is neither 0 nor 1 was undefined in the C++ (`!b` on a bool holding 2); Rust reads
  the byte and tests it against 0.
- **Golden driver** (scratch, `build/gemodel_golden`): the original 'GEModel Load.cpp', 'GEModel Legacy.cpp',
  'GEModel Save.cpp' and `GEModel.cpp` text (`git archive` of the commit before) compiled with the driver against the
  original headers (`EE_INCLUDE`), arrays of its own behind a fake `GERasterizer` (overrides generated from the
  header's pure virtuals), the driver's own `GEModelManager` (GEModel's friend, so it reaches the private members),
  `operator new` zeroing every allocation (so what the C++ leaves unset reads the same in both links), an
  `FSMemoryFile` subclass refusing writes past a limit. Two snags: `FSMemoryFile(data, length, ...)` frees the buffer
  it is given, and `FSFileSpec`'s constructor reads `gStartupDirectory` (the driver defines it). Each case prints the
  result, the position and a digest of the whole model (every array; a frame's array shared with an earlier frame as
  that frame's number). Corpus: 600 synthetic models (both formats, sub-models, strings with NULs, out-of-range
  indices, quantized normals past the table) whole, shared and cut at random lengths, 8,186 cases; the 862 `.cem`
  files of the binary tree whole, shared and cut, 45,402 cases; 500 in-memory models through the six geometry
  steps; 1,079 loaded game models through them (sub-models included); 2,400 synthetic saves and the game models'
  saves into limited files. Relinked over the forwarders, the driver printed identical files. The cargo tests replay
  the synthetic sections (`golden_model*.rs`: the generator, the store, the C++'s loops and its digest re-stated).
- **The driver hashes no out-of-range counter:** the original conversion's polygons listed twice land past the end
  (counted by the driver's arrays), and Rust drops them before they reach the array; the stored state is the same.
- Left: 'GEModel LOD.cpp' (`ComputeLODReduction`, the edge collapse) and 'GEModel Caching.cpp' (the vertex cache
  optimiser): neither has a caller in the game, the tools or the data path. `GEModelManager` is a map and a lock
  (lifetimes, kept C++). `GETexture::Load`'s description and grid planning, the managers, fonts, bitmaps and the rest
  of `GETerrainMesh` were not reached.

## D9 at run time: glTF and PNG straight into GEModel and GETexture (branch rust-d9-load, 2026-11-09)

The game's models and textures are glTF and PNG only (RUST_PLAN D9). `ee_openformats` decodes a glTF straight into
the model `GEModel::StoreLoaded` stores (`ee_ge` model_store.rs's `CemModel`: `gltf_to_model`, `ee_of_gltf_to_model`)
and PNG frames into the frames `GETexture::Load` stores (`png_to_texture`, `ee_of_png_to_texture`). Deleted: the `.cem`
reader and writer (ee_ge model_file.rs's parsing, legacy.rs, model_save.rs, their goldens and FFI, `GEModel::Load`,
`GEModel::Save`/`SaveData` and 'GEModel Save.cpp'), `GETexture`'s `.sst`/`.tga` reading and `GETexture::Save`,
`ee_openformats`' `.cem` and `.sst` writers (`gltf_to_cem`, `png_to_sst`, `GEOpenFormats::GltfToCem`/`PngToSst`), and
every lookup of a `.cem`, `.sst` or `.tga` file in the model and texture managers (and `*.cem` in the multiplayer
checksum). After the merge with the data side (rust-d9-data, whose pictures load through `GESurface::LoadPNG`):
`GESurface::LoadTGA` and the `GETGAHeader` record (their last caller was GETexture's frames, through `Load(file, 0)`
with the type in a variable, so no grep found it) and `GEFindTargaFile` (no caller left) are deleted too, and the
pictures' `png_to_picture` decodes through the textures' `decode_frame` (one PNG path; it now also refuses a bad
`ee:surface` text, as `PngToSst` did for the pictures before D9).

- **The decoded model is the reader's output, not the importer's.** `import.rs`'s `Model` matches `CemModel` field
  for field, but the `.cem` reader added rules of its own that the stored model shows: a name ends at its first NUL
  (`UString::Save`/`Load`); a model with no polygons, vertices, materials or frames fails before anything is stored;
  and a failed sub-model put the file back where it started, so every later model in file order (its siblings, its
  parent's later siblings) re-read the same bytes and failed the same way. `to_cem_model` carries a `poisoned` flag
  through the depth-first walk for that. A later LOD stores whole triangles (`count * 3` indices; an index accessor of
  another length desynchronised the old stream, never in the data). The shared frames' vertex and tag point arrays go
  to the last frame, as `LoadFrameChunk` put them.
- **A texture frame is stored where the Targa reader stored it.** `GEStoreFrame` (GETexture.cpp) is
  `GESurface::LoadTGA`'s loop over top-down RGB(A) instead of a bottom-up BGR(A) file: 32-bit surfaces get A8R8G8B8
  whatever their format, 16-bit ones `GEPackColor` of their format, no alpha is 0xff; each row continues after the
  skip computed from the surface's *pixel* width (so a frame narrower than its surface lands as LoadTGA put it); a
  frame larger than the surface is refused. `GETexture::Load` keeps its control flow and fills its old header struct
  from the decoded texture (one MIP level, not compressed).
- **The cache keys lost their extension; saves do not see them.** A model is cached under the name given
  (`models\x`, was `models\x.cem`), a texture under its stem: the name as given, or without its last extension when
  it contains `.sst` (`FindBackwards(".sst")`, as before), so `textures\x` and `textures\x.sst` stay one texture as
  they were one cached `textures\x.sst`. `ReloadTexture(name)` looks the name up as given: the same textures as the
  original's `name.tga`/`name.sst` lookups (a db button named `...sst` was never reloaded, and still is not). The texture
  pack maps the stem. Nothing in a saved game, a scenario or the db names a cache key; the keys only show in
  `DumpAllLoadedMedia` and `EE_TEXTURE_PACK_LOG` (now `textures\x`).
- **A refused model used to crash on its destruction.** `GEModel(GEModelManager*)` left `mFrameCount`,
  `mSubModelCount` and `mMaterials` unset and the destructor read `mPolygonArrays[0]` from a NULL array: the old path
  crashed on `amb_rock.cem` (the 24-byte text stub, the tree's one `.cem`) in a driver whose memory was zeroed, and
  read garbage in the game. Port fix: the constructor sets them and the destructor tests the array. The new path never
  looks `amb_rock.cem` up (NULL model as before, without constructing one).
- **The comparison came first (storegate).** `tools/eemodel/storegate.py` builds `storegate/` (a console driver)
  against a game build's own objects (`lle`, `jpeg`, `default`, `tcp`, `mixer`, `dx7hrtnl` as one archive, so only
  what is referenced links) and its Rust library; the renderer is memory objects (`fakes.h`: a surface with padded
  rows and a halving MIP chain, nearest and box copies; vertex and polygon arrays that count out-of-range sets; the
  rest of the 110 pure virtuals generated from the headers, preprocessed for `#ifdef LLE_EXE_HEADERS`). It loads every
  model of the tree with three shared-frame maps (861 x 3) and every texture (2,505) with seven descriptions (frame
  formats 0 and 1, 16/32 bits, alpha or not, MIP maps, one and two dropped levels, the texture pack's path), reloads
  and refreshes, and digests the stored `GEModel` (every array, material, sub-model, shared array as its frame
  number) and `GETexture` (every frame's surfaces and pixels) through `friend struct GEStoreDigest` (in GEModel,
  GEMaterial, GETexture, GETextureManager). First driver: the build before the change (objects copied aside), 20,118
  loads, 25,140 lines, twice identical. Then the new loaders: byte-identical output. Snags: `#define private public`
  for the digests breaks the link (MSVC mangles access into static data and functions: `UString::sDefaultAllocationLength`),
  hence the friend; `PIMModule.obj` drags the three plug-in tables in; every allocation is zero-filled
  (`operator new` replaced) so that what a loader leaves unset reads the same in both builds.
- **The decoder gate (cppgate) changed its goal.** It compared `.cem`/`.sst` bytes with the Python converter; now
  `eemodel_gate` writes a digest of the stored model (three shared maps) or texture, the cases come from the open tree
  itself (861 models, without EE_model, 34 as `.glb`, 2,568 edits; 2,505 textures, 275 PNG variants: 7,104 cases) and
  each must load or be refused as the Python decides and equal `cppgate/golden_store.json`. Recorded with a temporary
  `--old` mode that also ran the old path for each case (`gltf_to_cem` bytes through `model_file::load`; `png_to_sst`
  bytes read back as GETexture::Load and LoadTGA read them): 7,104 of 7,104 equal. `golden_cpp.json` (the `.cem`
  byte digests over tree-binary) is gone with the writers.
- Gates: storegate PASS (25,140 lines), cppgate PASS (7,104 cases), the sim oracle identical on seeds 2-7 (models
  feed it through `GEPhysicalModel` and the boxes), `gx_test`, `eedata check`. Not run: tours and screenshots (the
  lead's).

## D9, the data side: the game reads only the open forms (branch rust-d9-data, 2026-11-09)

The owner's decision D9 (docs/RUST_PLAN.md): glTF, PNG and the text db only at run time. This branch did the data side;
the model and texture managers (GEModel*, GETexture*, GEOpenFormats*) are another branch's.

- **Every binary read and fallback the game had outside those managers, and what it is now:**
  - `EEDbFindFile` (every db lookup: EEInitializeDatabase, EEUnitClass, UIManager::LoadDatabase): served the text when
    there was one, else `FindFile` of the `.dat`. Now text only; a missing text table or one with no text form stops
    the game with its name.
  - `EERandomMapTable::Load` read `db\dbrandommap.dat` (no text form), every record dropped, the table never read:
    the load, the function and the key `kDbRandomMapFile` are gone (and the unused `kDbTerrainGrayTexturesFile`).
  - `EE_DUMP_CONTENT` also dumped a `.dat` when a table had no text: gone.
  - The multiplayer checksum summed `Data\Db\*.dat`: now only the `*.csv`/`*.yaml` (it changes the version suffix
    only for a tree that still had `.dat` files).
  - `GEFindTargaFile` (pointers, `ColorPalette`, `Volcano`; the texture pack's pointers) served a `.png` as Targa
    bytes, else the `.tga`; `EEUISCIFormManager::OpenMap` tried `.jpg` then `.tga`; `EEUIBMapBackground` loaded a
    `.tga` or a `.jpg`. Now each reads a `.png` through `GESurface::LoadPNG` (ee_openformats `png_to_picture`), with
    no `.tga` fallback. `GEFindTargaFile` has no caller left (it is in GEOpenFormatsFile.cpp, the other branch's).
  - Not binary forms in D9's sense, so unchanged: saved games, scenarios and campaign archives (`.ssa`), `.civ`
    files, the scenario editor's elevation `.dat` maps, sounds, music.
- **The trap: `GESurface::LoadTGA` is the texture manager's too.** GETexture.cpp loads every `.sst`/`.tga` frame
  with `GESurface::Load(file, type)`, type 0 = Targa, a variable, so a grep for `kImageFileTGA` or `LoadTGA(` finds no
  caller. Deleting it compiles and links, and every texture fails to load at run time. It stays until the texture
  loader goes; `SaveAsTGA` and `GetTGAFileAttributes` (no caller) are deleted.
- **Pixels.** `png_to_picture` decides the channels as `png_to_sst` does (4 with colour types 4/6 or a tRNS chunk),
  so its pixels are the Targa frame `GEFindTargaFile` served. Checked in a scratch program (LoadTGA's 32-bit path
  ported line by line against LoadPNG's): the 25 retail `.tga` pictures the game reads (resolved on the data path as
  `FindFile` did) against the tree's PNGs, with and without `mNoAlpha`: 50 of 50 equal; the old path (png_to_sst's
  frame through LoadTGA) against the new one over every PNG of `Data\Textures` (2,666 files, the frames too): equal.
- **The tree.** `open_data.py build` writes no binary form (`binary_form`: `.cem`, `.sst`, `.tga` anywhere, `db\*.dat`)
  and lists the ones with no open form in `open_data.json` "dropped": `db\dbrandommap.dat`,
  `db\dbterraingraytextures.dat`, `db\copy of dbgamevariant.dat` and `Models\amb_rock.cem` (24 bytes of hex text,
  not a model; nothing names it). Otherwise the tree is byte-identical to the one before. `check` refuses any binary
  form. The releases until now carried them: the media part `open-media-ddb67f1` (read from its zip's central
  directory with a ranged download) has 5,721 entries, all open forms but `Data/Models/amb_rock.cem`; the db part
  `open-db-ce3df9e` has the three `.dat` files. `fetch_data.py` and `make_play_dir.py` delete binary forms after
  unzipping or copying, so an old release still plays.
- **The converters' hash changes** with `open_data.py`: the harness rebuilds `build/open_data/tree` on its first run
  after the merge (about 6 minutes) and `sim.py` warns until then. The new tree only lacks the four dropped files.
- **What still reads binary records:** the loaders still parse a record stream; it is compiled from the text in
  memory (`FSMemoryFile`), never read from a file. Handing them records instead is a per-loader change, not done.
- **The Bash tool and backslashes:** `"\\n"` inside a quoted heredoc reached Python as a line break, and `"\\/"`
  as `"\/"`. Edit scripts with backslashes are written with the editor, not through a heredoc.

## The Linux cross build after the Rust port (branch rust-linux, 2026-11-09)

The Linux and macOS builds had not been built since the first Rust crates landed; everything from R1.3 to R5b.2 and
R6 (about 140 commits, 26 crates) went in on Windows only. Brought back here: the Linux game cross build, the census,
the five test programs, clippy for the Linux and macOS targets. Linux cannot run on this machine; macOS was not built.

- **One failure in 1,336 units, and it was the compiler flags.** `EETestFixture.cpp` includes `<atomic>`; libstdc++ 10's
  `std::atomic<T>::is_always_lock_free` reads `__GCC_ATOMIC_<T>_LOCK_FREE`, which `-fms-compatibility` on a GNU target
  hides (as it hid `__GCC_ATOMIC_TEST_AND_SET_TRUEVAL`, part 5). `xbuild.py` now defines the whole family for the
  game's C++ units on Linux (`LINUX_GCC_ATOMIC_DEFINES`: the values clang defines for x86_64 in GNU mode, 2 =
  always lock-free), for the game build and the census alike. macOS (libc++, `-fgnuc-version=4.2.1`) does not need it.
- **The census stopped before compiling**: `cmd_census` still added `rust_build.WGPU_HEADERS` (the wgpu headers the
  C++ GX included; GX is Rust since R4.1 and the attribute went with it). No C++ includes `wgpu.h` any more. Census
  after the fix: 1,360/1,360 units in the ms dialect (the build's; the gnu dialect compiles 697, which is informational:
  the game's units rely on the Microsoft extensions, `UReferenceContainer.h` and `EEPlayer.h` first).
- **The LP64 mirrors held.** Every `#[repr(C)]` struct the C++ pins with `static_assert` (DX7Trampolines.cpp,
  UITrampolines.cpp, the UI classes' offset tables, GERustTerrain.h, GERustMath.h, DrawTerrain) passed on the LP64
  compile, and the link had no undefined or duplicate symbol: the one wrong width (ee_dx7's `TextRect`, `RECT`'s
  `LONG`s as `c_long`) had been found and fixed by the previous Linux build. Rule confirmed by the audit of the
  1,667 `c_long`/`c_ulong` uses: `c_long`/`c_ulong` mirror the game's own `long`/`unsigned long` (GX.h's `dw*`
  fields, the UI members, the terrain rows, GXRESULT), which are 64-bit on LP64 on both sides; Win32 `LONG`/`DWORD`
  (`wintypes.h`: `int32_t`/`uint32_t`) and the on-disk/wire `int32_t` fields are `i32`/`u32`. The `(unsigned long*)`
  casts left in the C++ seams are no-ops (`EEGamePreferences::mGameWindowWidth` is `unsigned long`); the one true
  8-byte read of a 4-byte field (`FindPatchOp.cpp`, `*(unsigned long*)data`) is in a WON unit the game does not build.
- **A pointer-width mismatch cannot hide in these seams.** The C++ passes `int32_t*`/`uint32_t*` to the generated
  header's `long*`/`unsigned long*` parameters nowhere (that is a compile error on LP64, and the build is clean); the
  `View` records (`EeDx7TerrainMeshView`, `EeGeTerrainFog`, ...) are filled field by field, where a width only converts.
  The hazard is confined to records that overlay C++ memory, and those are the pinned ones.
- **Test programs**: `plat_test`, `font_test`, `lua_test`, `tcp_test` and the four `tests/won` programs build and link
  for linux-x86_64 unchanged (the Rust library before SDL on the link line, as before). Not run: no Linux here.
- **Rust**: `cargo clippy --workspace --all-targets -D warnings` is clean for `x86_64-unknown-linux-gnu` and
  `aarch64-apple-darwin` (the macOS code paths type-check; `block v0.1.6` in wgpu's Metal path prints cargo's
  future-incompatibility note, not a warning of ours). The Linux `libee_rust.a` builds in 1 m 45 s.
- **Not proven here**: anything at run time on Linux or macOS (the Rust platform layer's POSIX halves against a real
  SDL window, the mixer's Core Audio / ALSA backends, wgpu on Metal with the Rust GX, the ASan run). CLAUDE.md's macOS
  checklist (full build, `--sanitize address`, an ASan `ai-game`) is the next step, on the Mac.

## The merge of main ce3df9e4 into the rust branch (AI manifest, settlement first; 2026-11-09)

- **Main stores PORT_LEARNINGS.md with LF, this branch with CRLF**, so git saw the whole file as one conflict (the
  base and main LF, ours CRLF, every line "changed"). Resolved by inserting main's added lines, converted to CRLF, at
  their places in this branch's file (difflib over the LF base). Expect the same at every upstream merge until one
  side normalises the file.
- **Both lines bumped the save version to 0.253 for different things**: here R5a.2's live state
  (`EESaveHasLiveState`), on main the next engine's AI id after a computer player's engine and the renumbered engine
  choices. Git merged it cleanly apart from the version comment, and a merged 0.253 save would have been read as both
  (the AI id's length read from the first live-state field). Main's 0.253 stays; the live state is from 0.254.
  Look at the version gates of both sides at every merge, not only at the conflicts.
- **Upstream rules changes land in the schema, not in EERules.cpp**: main removed the personalities' `next_script`
  (the manifest `Data/ai/ais.json` names the scripts; an older document's key is read and ignored). Here that is
  `tools/eedata/rules_schema.json` (the field and its per-personality values), `eedata.py rulesgen` (EERulesGen.h,
  EERulesGenDecode.h, rules_gen.rs), ee_data's loader (`x.get("next_script")`, ignored) and its tests, and
  rules_parity's case (a bad name is no longer refused: `a_script_obsolete`). rules.py came over from main as is.
- **No other semantic clash showed**: main's new AI code keeps unit pointers only within a call (finder locals, an
  address-keyed `std::map` used for lookups, not iterated into decisions), the improvement table fix
  (`EEUnitClassImprovementTable::mU00` pointer-sized) touches nothing ee_combat ported, `MoveControlsHorizontally`
  builds on the C++ `UILayoutUtility` facade, and main's `EEReadIdsFile` (binary trees' `.ids`) went with D9 (the
  last bullet). The oracle over the merge equalled main's own EE-SIM (refmain) on seeds 2-7 at once.
- **The capture goldens were rebaselined** (their C++ is gone): the merged game plays main's game, not the old one,
  so every A*, formation, combat and tech digest changed. Evidence: the oracle equal to refmain, two captures per
  profile equal, and `sim_capture.py replay` (the Rust leaves over the new captures) with no difference. The late,
  space and calamity goldens now hold all six seeds and every tag (before: seeds 2-3/2-4, no CB** or ASGR).
- The main open tree has no `Data/ai/ais.json` (built before the manifest): EE-SIM logs "the lobby offers only
  classic and random". The oracle plays classic, so it does not matter there; a next-engine run on that tree needs
  `--ai-scripts ai` (or `open_data.py ai`, which edits the tree).
- **D9 against main's binary-tree `.ids`** (at the merge of `rust` after it): main taught the binary tree row
  names (`Data/db/dbobjects.ids`, `EEReadIdsFile`, `open_data.py ids`, `build --binary` writing them); `rust` had
  retired the binary tree (D9). The C++ reader and the tool's `ids` command were dropped. Git merged open_data.py
  without a conflict but left main's `if a.binary:` in `cmd_build`, an AttributeError on every build now that
  `--binary` is gone. Main's `ai_files` (ais.json into a tree's Data/ai) kept.

## The next computer player's towers and the epochs (2026-11-08)

- **A computer player's towers upgrade by themselves.** The tower upgrades (`copper_tower_upgrade`,
  `bronze_wall_and_tower_upgrade`, ...) are dbtechtree kind-7 techs with `mResearchedWithEpoch`, and
  `EEComputerPlayer::EpochReached` -> `EnterEpoch` -> `EETechTreeEpoch::ResearchKind(7, true)` researches them
  free the moment the epoch is reached, for the classic and the next engine alike; their events convert every tower
  of the old class (dbeffects kind 19, `Fn1CACB4`) and obsolete its tech. Reading the scripts suggested the towers
  "stay Prehistoric all game" (AI_RESEARCH §8.31); a trace of the view's `buildings[]` each minute showed the
  opposite. Measure before fixing. The `research` verb answered such a tech `busy`; it is now `done` (n=0).
- **A research started at a building leaves `mEpochIDs`** (TryResearch hands it to the producer's queue), so the
  verb's busy check missed it and a second order queued it again; it now also looks at the producers' queues.
- **A data tree being rebuilt under EE-SIM** (another session's `open_data.py build`) fails the runs that stage it
  then (`EETerrainColorTable` verification failure at start-up, rc 0x80000003). For a long bench, hard-link a copy
  (`cp -al build/open_data/tree build/tw/tree`, seconds) and pass it with `--data`.

## R5b.1, the graph searches: EEAIAStarGraphStorage and EEAIAStarGraphMachine in Rust, the continent analysis captured (ee_astar graph.rs, branch rust-r5b-graph, 2026-11-09)

The graph searches (`EEAIAStarGraphMachine<S, G, M>`: the sub sector paths of `EEPathManager` over
`EEAIAStarGraphStorage`, the continental chains of `EEAIContinentManager::NumberContinentChains` over
`EEAIAStarGraphTerrainStorage`) are Rust now: `rust/crates/ee_astar/src/graph.rs` holds the node storage (the closed
list, the two hash tables of 27 buckets, the sorted and the fast open lists, the per-index flag and node arrays, the
path cursor) and the machine's `Start`, `Expand`, `Restart`, `NextNode` and `AddNode`, line by line. The goals
(`EEAIAStarPathGraphGoal`, `EEAIAStarGraphContinentalChainGoal`) and the route planners (`EEAIGraphPathGoalModifierBase`
and its subclasses) stay C++.

- **Node views instead of node pointers.** The goals take `EEAIAStarGraphNode*` (`IsGoalNode`, `GetStepCost`; the
  route planners read `inFrom->mObject`, the goals `mIndex` and `mTile`) and `SearchDone` read the storage's current
  node and open lists. The Rust machine hands the hooks an `EeAStarGraphNodeView` (tile, costs, index, object), which
  the C++ hook turns into a stack `EEAIAStarGraphNode` with NULL links: nothing reads a node's `mParent`/`mNext` but
  the storage itself. `SearchDone`'s reads are parked on the storage (`mHookHasCurrent`, `mHookCurrent`,
  `mHookHasOpen`) by the hook before it calls the goal, as the tile machine's hooks do; `NumberChain` reads the
  closed and fast open lists' objects through `ListObjects`.
- **The objects are opaque handles.** A node's `mObject` is an `AStarGraphableObjectBase*`, at +0x68 inside
  `EEAIContinent` (multiple inheritance) and at +0 inside `EEAIPathSubSector`. Rust stores the base pointer's bits
  (`usize`) and never reads them; the hooks convert `M*` -> `AStarGraphableObjectBase*` -> `void*` and back the same
  way (`static_cast` both times), so the base offset is applied and undone by the compiler. A `void*` -> `M*` cast
  without the intermediate base type would be wrong for the continents.
- **Kept quirks:** `RemoveNearestOpen` sets the sorted head from the removed node's (null) prev when it was the
  tail; `RemoveBestOpen`'s 15-best refill counts a replacement as a seen node; the nearest removals clear a node's
  flag but leave its node-array entry (the flag gates it); `AddNode` compares a node's `long` path cost with the new
  `unsigned long` cost unsigned; `Restart`'s fallback node has index -1 and sits on no list. `Expand`'s
  `theObject->mID != -1` test (an `unsigned short`, never -1) is asked once per node instead of per neighbour: a plain
  read, no event.
- **The capture gate found nothing to fix:** the six oracle games' 14,992 ASGR records and the late/space/calamity
  profiles' (6,943/2,230, 2,597/3,345, 1,370/1,155/942 graph runs) equal the C++ digests at the first build, with
  the sim oracle identical against the reference and against the C++ baseline exe on the same data tree.
- **The profile goldens were incomplete.** `golden_cpp_late/space/calamity.txt` listed no ASGR lines (recorded before
  that capture existed) and no CB** lines (the combat capture's; on this branch the combat formulas are already Rust,
  so no C++ golden of them can be recorded here any more), which made `sim_capture.py check --profile ...` fail on
  tags that simply had no golden. The ASGR lines are now recorded (from the C++ baseline exe of this branch, before
  the port); `check` reports a tag the golden file does not list for a seed as "no golden" and compares the rest,
  still failing on any line that differs or is missing. The CB** lines of the three profiles remain unrecorded.
- **The continent analysis is captured (CNTB, CNTR; `ee/EEAIContinentCapture.h`).** One record after
  `BuildRegions` (before it frees its work grids) and one after every `Rebuild`: the manager's counters, every tile's
  continent and sub sector id and tile value, the continent adjacency matrix, every continent, path area, path
  sector, sub sector, choke point, hill, forest and rectangle area with its tiles, ids and links, the shore tiles.
  The oracle games make one build and 38-53 rebuilds each (late/space: 106-125). The goldens were recorded from the
  C++ analysis with the graph machine already Rust (equal by ASGR and the oracle). A sub sector's `mNumTiles` is not
  in the record: the C++ never sets it. Every field is widened to 32 bits so the record reads the same on LP64.
- **The first leaf of the analysis is Rust:** the choke point pass's grid smoothing (`EEAIChokeSmooth`,
  `ee_astar/src/choke.rs`), over the pass's grids, which became fixed-width (`int32_t`/`uint32_t`) for the C ABI.
  The serial a tile takes is its last non-zero-kind neighbour's in the 3x3 walk, zero included: the C++'s
  `(theSerial = serials[other]) != 0` assigns before it tests. The first port assigned only non-zero serials and
  the sim oracle caught it at once (every seed diverged; the CNTB record would have too). Its `theSerial` lives
  outside the tile loops but is only used after a non-zero kind was read, so nothing carries over. The later passes
  skip the area tiles, so a one-tile gap between two areas is never flagged and a two-tile gap is; pinned by tests.
  Gate: the CNTB and CNTR
  digests equal to the C++ goldens in the four profiles, the sim oracle identical against both references.
- **The shared data tree moved under the gates.** The `rust` branch took the D9 data merge while this branch was
  being gated; the main checkout's `build/open_data/tree` was rebuilt by the newer converter and this branch's exe
  could not load it (`Empire Earth.cpp (1074) : Verification failure` at start, after `sim: warning: ... was built by
  other converter code`). The gates ran on a tree built by this worktree's own converter (`open_data.py build
  build/open_data/tree`, about 4 minutes), with the C++ baseline exe's oracle traces on the same tree as a second
  reference. A worktree should build its own tree whenever `sim.py` prints that warning.
- **Text-mode Python converts line endings on Windows.** `Path.read_text()`/`write_text()` turned four LF files into
  CRLF (two Rust files and `EEAIChokePoint.cpp`/`.h`), which `git diff --stat` showed as whole-file changes; restored
  from `git show HEAD:` in bytes mode. Edit these files in bytes mode only.
- **Gates run:** `build.py -j 3 --sim` 0/0 (EE-AOC, EE-SIM) for the graph port and for the choke smoothing;
  `sim_capture.py check` PASS in the four profiles; the sim oracle identical on seeds 2-7 (against `ref` and against
  the C++ baseline); `rust_build.py gate` PASS (ee_astar: 60 tests, 19 for the graph storage and machine, 4 for the
  smoothing). Not run: `gates.sh`, `eedata check`/`cppgate` (no data change), any macOS or Linux build.

## R4.3, fourth part: the pixel formats, GETexture's plans, the managers' decisions, the terrain's layout, water and visible rows; 'GEModel LOD.cpp' and 'GEModel Caching.cpp' deleted (ee_ge pixel.rs, texture.rs, managers.rs, terrain_volume.rs; branch rust-r4-ge4, 2026-11-09)

The texture leaf, the two media managers' logic and three more pieces of `GETerrainMesh` are Rust behind the C++
classes; the textures, surfaces, maps, locks and reference counts stay C++. None of it is simulation code (the graphics
thread, the UI and the loaders call it), but the C++'s `f32` order and `unsigned long` arithmetic are kept throughout.

- **Deleted, not ported: 'GEModel LOD.cpp' (`ComputeLODReduction`, the `GELOD*` edge-collapse classes of
  `lle/GELOD.h`) and 'GEModel Caching.cpp'** (`ComputeOptimalDataOrdering`, the genetic vertex cache optimiser). No
  caller in the game, the tools or the data path: the only mentions outside the two units were the `UList<GELOD*>`
  node-pool statistics in `EEReleaseGamePools` and three units including the header for the pools' init chains. The
  unit list (`link/lle/units.txt`) loses the two entries; `GEModel.h` the declarations and the `GELOD.h` include.
- **The pixel formats** (`pixel.rs`): `sPixelFormats` (the shift and position per channel of the eight formats) and
  `GEPackColor`/`GEUnpack*` ('Graphics Engine.cpp' forwards); `GETexture.cpp`'s `GEStoreFrame` pixel loop is
  `store_frame` (the C++ keeps the size check before the lock, the lock and the unlock). The quirk stays: the row skip is
  computed from the *surface's* pixel width, so a frame narrower than its surface puts its next row inside the first
  row's padding (`a_narrower_frame_continues_after_the_skip_of_the_surface_width`); the storegate pins the pixels.
- **`GETexture`'s plans** (`texture.rs`): `Load`'s MIP level count (how often the width halves to 0), the drop clamped to
  it, the frames' surface type and level count, the reduced surface's description (three cases: everything dropped to
  one pixel, a chain kept, one level kept), the frame grid of format 1 (the smallest power of four, `sqrt` in double,
  `1.0f / framesPerRow`), a grid cell's rectangle and `SetFrame`'s UV corner in the C++'s `f32` order,
  `Dereference`'s count decision, `GetAllocatedSize`, the texture pack's MIP filter override. `GETextureSet`'s grid
  arithmetic (`GetTexture`, `GetTextureCoordinate`, `GetTextureIndices`) and `GEMaterial::SizeOf` (from the C++'s sizes)
  are Rust too; the set's header inlines call it (not simulation code, a call per tile a frame).
- **The managers** (`managers.rs`): a texture's cache name (`texture_stem`: the name, or without its last extension when
  it holds ".sst"), the texture pack's settings (`TexturePack`: a `static Mutex`, the C++'s four statics; the
  directory stays when the pack is turned off, a directory over 62 bytes turns it off as the original's buffer did), a
  texture's pack name (case-insensitive "textures\\", the three data pictures refused), `LoadTexture`'s drop and MIP
  decisions, `SetTexturePackSizes`' arithmetic, the surface description a texture description asks for (16 bits:
  A4R4G4B4 with alpha else R5G6B5; else A8R8G8B8 with `mNoAlpha`), the removal decision (the manager's reference is the
  last: remove and 0, else 5), and the made textures' and created models' names with their counters (`AtomicU32`s in
  place of `sTextureNumber`/`sModelNumber`). `ReloadTexture`'s "not cached" 3 and the Close result stay C++.
- **`GETerrainMesh`** (`terrain_mesh.rs`, `terrain_volume.rs`): `CreateFlatMesh`'s row columns and edge sums
  (`flat_mesh`; the C++ allocates and `Initialize`s the rows), `SetGlobalWaterState`'s 33 x 33 texture grid (a fresh
  `URandom`, seed 1 as the default constructor gives: deterministic, so a Rust test pins the first draw through
  `ee_random`), `SetWaterColorSet`'s index (a negative product wraps through `i64` as the x86-64 conversion does, then
  clamps to the last entry), `UpdateWaterAnimation`'s step, and `GetTileRowsInVolume` whole: `U2DConvexHull<float>`
  (`operator<` sort, unique, Graham's scan with the pivot at the lowest point, `GetSide` in `double` of `float`
  differences), `GetScanLines`/`AddScanLine` (the tile walk with its `(long)` truncations, a coordinate at or below 0
  in the tile one less, the 2D crossing test with its -0.00001..1.0001 window) and the clipping to the map's rows with
  the `0x7fffffff`/`0x80000000` empty bounds. `GETerrainMesh.cpp` no longer includes `U2DPolygon.h` (`URandom.h` stays:
  `U2DFractal.h` needs it).
- **One declared difference: the hull's collinear points.** The original sorted the points around the pivot with a
  comparator (`GAAngleCompare`) that deleted the nearer of two collinear points as a side effect of the comparison, so
  which collinear points survived depended on which pairs the STL's `std::sort` happened to compare. The Rust decides
  every collinear pair up front (the farthest from the pivot survives; equal distances by index), which is what the
  comparator meant and what the sort gave whenever it compared them. The frustum's eight ground points are never
  collinear with the pivot in practice. A Graham stack of one point (the C++ indexed `size - 2` there) pushes the next
  point instead of reading before the vector.
- **Build order bites.** `build.py` stages each unit at compile time and reads the headers in place, and cargo builds
  from `rust/` in place: a background build picks up every edit made while it runs (two of the three builds here were
  mixes), and `cargo run -p xtask -- cbindgen` fails with "user-mapped section open" while the build's cargo holds the
  target. Edit, regenerate the header, then build, and touch nothing until it ends.
- **Gates run:** `build.py -j 3 --sim` 0/0 (EE-AOC, EE-SIM), `rust_build.py gate` PASS, `eemodel.py cppgate` (the
  store gate: 25,140 lines equal, 7,104 decoder cases), `eedata check`, `gx_test --run`, the sim oracle identical on
  seeds 2-7, the windowed-software test profile 0/0 (the numbers in the final report). Not run: tours (the lead's).
- **Left in R4.3:** `UpdateVisibleTerrain` with `AddTilePolys`/`AddWaterPolys`/`AddCliffPolys` and the eight
  `Generate*EdgePolygons` (they build the C++ material lists keyed by `GETexture*` and fill polygon pointers into the
  mesh's arrays: a plan-and-fill port with a golden driver over `GEViewport`), `GEPhysicalModel`'s walks and viewport
  test, fonts, bitmaps, the viewport.

## R5a.4, finished: the registry in Rust, the UI, editor, camera, world and AI holders as handles (branch rust-r5a-handles2, 2026-11-09)

The second half of "R5a.4-R5a.6: unit handles" above: the three things it left open (the registry's lock, the holders
outside the AI, the AI's containers), with no behaviour change (the sim oracle identical to `ref` after every commit).

### The registry is Rust and lock-free (`rust/crates/ee_units`)

- **A lock would have been paid on every `mUnit->X`.** The AI resolves handles on most of its reads; an uncontended
  critical section costs about what the `unordered_map::find` it would guard costs, so the registry became a structure
  that needs none: a three-level radix tree over the id's bits (8: the player byte, 12, 12) of atomic pointers. A
  resolve is at most three dependent loads (root -> branch -> leaf -> slot); register is a store; unregister a
  compare-exchange that clears the slot only if it still names that object (the C++'s "only if the entry is still
  this unit"). Branches and leaves (32 KiB each on x64) are allocated on first use with a compare-exchange (the loser
  frees its own) and never freed while the registry lives: a reader can hold no dangling node. Memory: one branch per
  player byte in use and one leaf per 4096 ids of that player.
- **What a reader sees.** The registered object or NULL, never a torn value: the pointer store is `Release`, the loads
  `Acquire`. Whether the unit behind the pointer is alive is the holder's business, as it was with the raw pointer (a
  holder that referenced the unit still does). So a unit's last `Dereference` on the UI thread (the registry erase in
  `FreeNode`) or a resolve in the graphics thread's `CopyData` may now race the physics tick's inserts and resolves.
- `RTSUnit.cpp` keeps the entry points (`RTSResolveUnitID`, `RTSRegisterUnit`, `RTSUnregisterUnit`) and the "no id"
  guard; the Rust side (`ee_units_register/unregister/resolve`) treats every 32-bit id alike. Nine `cargo test`s: the
  bit split (every bit of the id tells ids apart), replace and the same-object unregister, the pages a player's ids
  share, drop, four writers racing three readers on shared leaves, and the last-Dereference-on-another-thread case.
- **Measured.** EE-SIM's game time per seed (`wall_seconds_game` in the report) moved within the run-to-run noise of a
  machine shared with other agents: 8.4/8.5/6.7/6.7/6.4/5.0 s before, 7.5/8.1/6.1/5.9/7.9/5.9 s after the registry,
  7.7/8.6/7.1/6.8/9.0/6.9 s after the world's containers (the Step loop resolves every unit every tick). A side-by-side
  run of the kept baseline exe against the final one is in the gates paragraph.

### The holders converted (every one referenced its unit, so each resolves to what the pointer held)

- UI: `EEUICPortrait::mUnit`. Editor: `EESEAMoveUnitEntry::mUnit`/`mContainer`, `EESEAPlacedUnit::mContainer` (undo
  records), `EEUIFScenarioEditor::mSelectedUnits` and `EEUIFSELBCargoList::mCargo` (vectors of handles). Camera:
  `EECameraControl::mUnitA`/`mUnitB`. Actions: `EEAUpgradeImprovement::mUnit`. Communications holds none (its 95
  locals live inside one message's Execute); UI Events' one member (`EEUIEHKUnitEntry::mUnit`) is a hot key's
  per-call candidate list, left a pointer with the other function objects.
- EEWorld's five: `mUpdateUnits`, `mActivateQueue`, `mDeactivateQueue` (deques of handles: the order is the elements'
  order; `EEPhysics::UpdateUnits` steps through them, `EEPWorldHelper` saves and restores them as before),
  `mActiveUnits` (keyed by id, handle values) and `mResourceUsers`, a map keyed by the unit: keyed by handles through
  `EEUnitHandleLess<T>` (by id), which is only the same as the address order for a map nothing iterates (this one is
  looked up only). One edge differs by design: a freed resource unit's entry stays unmatched, where a new unit reusing
  the address used to inherit its gatherer count (the entries are never erased).
- AI: the prophet calamity analyzers' `mUnit` and the Teleport/ShieldBattery `mTarget` (members the first pass did not
  list), and 11 containers: `EECPResourceManager::mMines`/`mFish` (vectors; their `std::sort` with a distance comparator
  over `EEComplexUnit*` converts each handle per compare, the same comparisons in the same order),
  `EECPWallManager::mGates`, `EEHerd::mUnits`, `EECPResourceCenter::mUnits`, `EECPUnitSelectionGroup::mUnits`,
  `EEComputerPlayer::mUnitsToKill`, `EECPCivilizationManager::mRepairing` (vectors), `EECPPersonnel::mUnits` (a
  multimap keyed by unit type: equal keys keep insertion order, so the order is unchanged). No AI container is keyed
  by the unit's address, so none changed its iteration order; the oracle (identical, six seeds) is the proof that no
  computer player's decision moved.
- **A handle cannot hold a unit that has no id yet.** `EECPResourceManager::mGatherers[7]` converted and the game
  crashed at its first tick: `EEUnitClass::CreateUnitAtPoint` -> `ActionChanged` -> `EEComputerPlayer::UnitGoalDone` ->
  `AddIdleGatherer` puts a new citizen on the idle list while it is still pending (its id comes at the tick's
  `AddPendingUnits`), so the handle stored 0xffffffff and `AssignGatherers` read NULL. It is a pointer deque again,
  with the reason at the member. Found with a temporary stack capture in the EE-HANDLE report (every distinct caller
  chain once, symbolised by the exe's .map); with mGatherers back, six full seeds store no other id-less unit in any
  converted holder, which is the check to run after any further conversion (the message prints once, and only a
  holder read before the flush crashes).
- **Saves.** Every Load that read an id into a pointer field (`reinterpret_cast<EEUnit*>(id)`, `USavedPointer<T>`)
  now reads it into the handle with `FromSavedID`, and every Restore reads it back with `GetSavedID()` (grep
  `USavedID(` finds no handle left). The file formats are unchanged. `EEHerd::RepairPointers` tested the pointer
  against `(EEComplexUnit*)-1`: that is `GetSavedID() != 0xffffffff` now.
- **The compiler's catches.** `a ? ptr : handle` is ambiguous (both convert to each other): `handle.Get()`.
  `(EEComplexUnit*)handle` (a C-style downcast of an `EEUnitHandle<EEUnit>`) does not compile:
  `static_cast<EEComplexUnit*>(handle.Get())`. `vector<T*> v = handleVector;` does not convert: `v.assign(begin, end)`
  or the range constructor. `EEHerd::Attack` took `const std::vector<EEComplexUnit*>&` and was passed `mUnits` live (a
  copy would read a snapshot where the original read the list as GiveGoal ran): it gained an overload over the handle
  vector with the same body. `EEUnit*& theUnit = mSelectedUnits[i]` became a value. A `std::find` of a handle in a
  container of handles compares through the built-in pointer `==` after both convert; no comparison operator was added
  to the handle (one would make `handle == ptr` ambiguous).

### Left as pointers, and why

- `EECPResourceManager::mGatherers[7]`: filled with pending units (above).
- `EECPUnitManager::mUnits` (`std::map<long, std::deque<EEComplexUnit*>*>`): the deque type is the signature of
  `GetUnits()` and of `CountUnits(EEUnitClass*, std::deque<EEComplexUnit*>*)` on ~15 gatherer classes, with 103 uses
  in 21 units; the order is a deque's, so it converts without a behaviour change, but as a change of its own, and
  `UnitAdded` may well see pending units too (check with the EE-HANDLE report first).
- The function objects (built and dropped inside one call, so a unit cannot be freed under them): `EECPDefenseCommander.cpp`'s
  `EECPUGatherCitizenUnits::mCitizens`, `EECPASGLand.cpp`'s `EECPAILoadGroup::mUnits`/`mTransport`, `EECPBuildManager.cpp`'s
  and `EECPWallManager.cpp`'s target finders, `EEUIEHKUnitEntry`, `EEUUTargetSearch`.
- The A* goals' `mUnit`/`mTarget` (`EEAIAStarPathGraphGoal`, `EEAIAStarPathfindingGoalBase`, `EEAIPathGoalModifiers`:
  the `rust-r5b-astar` branches' files), the Graphics Thread's (`EEE3DMarchingLine`, `EEE3DTracerBeam`, `EEEffect::mUnit`,
  `EEUnitPicker::mCarriers`, `EERenderLists::mPickedUnit`: `rust-r5a-snap`'s), and the Units, Physics, Player,
  Calamities and Map modules (not in R5a.4's scope: `EEContagionHelper::mUnits`, `EEPPatrolGroup::mPatrolPoints`,
  `EEAirTrafficControl`, `EECalamity`, `EEPlayer`'s lists and viewed units).

### Record layouts and gates

- The harness's records: EEPlayer, EEPhysics and NESynchronousCommunications have the same layout.json as before;
  **EEWorld is 8 bytes smaller** (the camera's two pointers are 4-byte handles), so its members after `mCameraControl`
  (`mActiveUnits` on, among them the harness's WORLD_LOCAL and WORLD_VICTORY) are 8 bytes earlier. The harness reads
  them by member name from the layout.json the build regenerates, which is what that file is for. The AI classes that
  shrank are read by no tool.
- Builds between commits used a scratch wrapper over `tools/build.py` (not committed; `build/handles2/inc_build.py`):
  it recompiles the units whose source or quoted include closure is newer than their object (a header name resolves
  to every header of that name under include/ and src/) and links EE-SIM. A touch of `EEUnitHandle.h` recompiles 761
  of 1,361 units (it is in every unit through RTSUnit.h), `EEWorld.h` 433; the AI headers 70. It does not regenerate
  layout.json (`tools/harness/layout_gen.py --out build/clang` does).
- **Gates run.** Per commit: EE-SIM linked 0/0 (the incremental wrapper) and the sim oracle (seeds 2-7, 2700 s)
  identical to the main checkout's `ref`; `cargo test -p ee_units` (9), its clippy and rustfmt. At the last code
  commit: the full `build.py -j 3 --sim` (EE-AOC and EE-SIM 0/0, 569 s), its oracle identical, `--test-profile
  windowed-software` 0/0, `rust_build.py gate` PASS (cargo test over the workspace, clippy, rustfmt, the header
  current). Timing, the baseline exe kept from the unmodified branch against the final one, alternating per seed
  over two rounds (12 pairs, `--jobs 1`): 94.8 s against 96.5 s of game time in all (ratio 1.017; single pairs swing
  by 40 % either way on this shared machine), so nothing the oracle or EE-SIM's clock can see got slower. After the
  merge of `rust` (667440fa, rust-r4-ge4; no conflicts; the generated header merged current): `build.py -j 3 --sim` 0/0 (556 s), its oracle identical on seeds 2-7, `rust_build.py gate` PASS, `--test-profile windowed-software` linked last (its result in the branch's report).
  Not run: the harness (tours, `gates.sh`), a saved game loaded, Linux or macOS.

## R6: the controls table, the form stack and the last button subclasses in Rust (ee_ui, branch rust-r6-ui4, 2026-11-09)

`UIControlsTable` (the record's fields, colours, layout and texture rectangles from the database record, `AddControlLike`'s
searches and edits, `MoveControlsHorizontally`, the scaled font's size, the string-table range, the rectangles
`CreateControl` lays a control out with), `UIManager`'s form stack (which forms are visible under which, `Push`, `Pop`,
`PopTo`, `SwapForm`, the dirty and clip rectangles, `RedrawAll`, `RelayoutAllForms`, `GetFormByID`, the event queue's
dispatch, the wheel's step), `UITriStateCheckBox` (state, bitmaps, label geometry), `UIBlinkingButton`'s fade and
`UIScalableButton`'s three-slice draw moved to `rust/crates/ee_ui` (`controls_table.rs`, `manager.rs`,
`button_variants.rs`). The C++ classes keep every member, vtable and exported name; `UIControlsTable.h`, `UIManager.h`,
`UIForm.h` and `UITriStateCheckBox.h` each gained a `friend struct`. What stays C++: the file reads (`Create`), every
`new` of a control, texture, bitmap or font, the locks, the IME and music members of `UIManager`, `UpdateKeyboard`/
`UpdateMouse`'s input tests, `SetIsBlinking`, every `DrawPolygons`/`DrawTextA`.

- **`#define private public` in a driver hides access errors.** The host functions (`extern "C"` free functions in the
  unit reaching the class's protected lists) compiled in the driver TU, where the unit is included under the override,
  and failed in the real build twice (`sUIControlsArray`, `mCopiedForms`, `mEventsToTrigger`). Reach such members
  through the friend struct's static accessors, and compile the unit alone (`clang_build.py --only NAME lle`) before
  the full build.
- **The override also changes MSVC's mangling.** Access is part of a member's decorated name (`?x@C@@0KA` private
  static, `@@2KA` public), so a TU under `#define private public` references a *different* symbol for a private static
  defined in a normally compiled unit (`UString::sDefaultAllocationLength`), which `/FORCE:UNRESOLVED` leaves at
  address 0: the driver died before `main`. Define the override only around the `#include` of the unit under test.
- **The forms are opaque handles.** `manager.rs` never dereferences a `UIForm*`: the stack, the visible list and the
  forms list stay the manager's `std::vector`s, read a slot at a time (`ee_uicpp_manager_len/at/push/pop/insert/erase`)
  and the calls on a form go through two switches (`ee_uicpp_manager_form_call`, `..._form_rect_call`). `Open`/`Close`
  may change what a form covers, so nothing is cached across a host call. The harness reads `mFormStack` by its
  offsets from `layout.json` (`presentation_bridge.c` `EE_OFF_UIM_FORMS_FIRST/LAST`): the member is untouched.
- **Two reads before the vector's start, guarded.** `AddClippedForms` with no visible form (`mVisibleForms[-1]`) and
  `AddToVisibleForms` with a form not on the stack indexed before the lists' start; the Rust skips those steps. Neither
  happens in a valid sequence (the top form is always visible), so the goldens never reach them.
- **`AddClippedForms`/`AddToVisibleForms`/`RemoveClippedForms` have no C++ definition any more**: they are private
  members only Push/Pop/SwapForm called; the declarations stay (no layout, vtable or export change).
- **A manager without its constructor.** The manager driver `calloc`s the `UIManager`, placement-news its vectors and
  deque, gives it `calloc`ed critical sections with no-op `Enter`/`Leave` bodies and a fake rasterizer; the vptr is
  NULL, so it never calls a virtual (`UpdateUI` is not in the golden: its event loop is three lines, `process_all_events`,
  over the same host calls `ProcessAllEventsForForm` exercises).
- **The record of the controls database stays binary.** `EEDbFindFile` compiles the text table to the 860-byte record
  stream; `DbControl` is that record in Rust (`#[repr(C)]`, bools as bytes), the C++ `UIDbControl` static-asserted
  against it. A random record with a left or top offset type 5 (`kLayoutFromStartEdge`) recurses for ever in
  `CalculateRectangle` (the original's hazard, PORT_LEARNINGS "first part"): the driver keeps type 5 for right/bottom.
- **`UMSin` is the table cosine.** `UIBlinkingButton`'s fade calls `ee_ge::um::um_sin` (the `UMCos` table through
  `UMCos(pi/2 - x)`), bit-equal to the C++'s `UMSin` inline; `ee_ui` now depends on `ee_ge` (no C++ imports there).
- **Pure returns instead of host calls where the C++ only applies values:** the blinking button's `CopyData` returns the
  four opacities (`EeUiBlinkShow`), the scalable button's `DrawPolygons` the three slices (`EeUiSlice`); the C++ sets
  and draws them in the original order. The slices' arithmetic keeps the C++'s association (`(right - width) + 1`).
- **`cbindgen` exports only what an exported function reaches**: a record used by the host calls alone
  (`TableKey`, `Trigger`) goes into `[export] include`; a `pub const` named `BITMAP` would clash with wingdi's.
- **Goldens:** `golden_controls.txt` (48 records, 24 `AddControlLike`, 10 moves, per table `SetupFont`,
  `SetupStringArray`, `CreateControl`, `CreateControlEvent`, `RelayoutControl`; the replay checks the ported values
  and the geometry of each `create`/`relayout` block as an ordered subsequence, the rest being C++ glue),
  `golden_manager.txt` (7 forms whose rectangles nest, overlap and coincide; 400 operations), `golden_buttons2.txt`
  (2 tri-state boxes, 2 blinking buttons, 2 scalable buttons; 600 operations). The three drivers over the forwarders
  print the same files (`cmp`).
- **Gates run:** `build.py -j 3 --sim` 0/0 (EE-AOC, EE-SIM) after each of the three commits and the windowed-software profile at the end; the sim oracle (seeds 2-7, the main checkout's open tree) identical to `ref` after each commit; `rust_build.py gate` PASS after each (ee_ui: 79 tests, 8 for the controls table, 5 for the manager, 4 for the three subclasses); the five changed units compiled alone before each full build; the 26 names `presentation_bridge.c` resolves (the `UIListBox<UWideString>`, `UIEditBox`, `UIComboBox`, `UICheckBox` members and vtables, `UIControl::GetTriggerEvent`/`FireTriggerEvent`, `TSCriticalSection::Enter/Leave/TryEnter`, the `UWideString` members, `gUIManager`) present in the map. The `UIForm`/`UIManager` layouts are unchanged (friend declarations only). No tour, screenshot or live UI use was run; `gates.sh`, `eedata check` (no data change) and the POSIX builds were not run.
- **Still C++ in R6's toolkit:** the drawing (`CopyData`'s rhombus/text copies, every `DrawPolygons`/`DrawTextA`),
  `UIManager`'s music and IME members, the edit box's key dispatch and repeat timers, `UIForm`'s Open/Close/dirty
  rectangle bookkeeping, the backgrounds, hot keys, `UIFontTable`, `UIBackgroundTable`, `UIEvent`.

## R6: a LAN game through the lobby's own UI, and WON's async-op machinery in Rust (branch rust-r6-won3, 2026-11-10)

### The two-instance LAN test: `tools/harness/lan_game.py`

The first oracle for what the lobby must keep doing (RUST_PLAN R6 pre-work 1, PORT.md "Other next steps"): two
instances of the test-profile exe on this machine, each under the UI fixture, host and join a LAN game through the
WON lobby's own screens (main menu -> MultiPlayer -> Other Multiplayer -> Host Game / the LAN list -> Join -> Ready ->
Start -> the connect dialog -> the game), and both play in lockstep for `--play-seconds`. `mp_game.py` bypassed the
lobby; this does not. Run 3 (`build/lan_game/run3`): PASS, both ends at CT 199 after 60 s, PT 2736 / 2732, world
players 3 (the two humans and Gaia), no GX error. So a LAN game through the lobby works on the current code, with
the earlier `SendMsgOp` fix (the LAN join, the chat and the staging messages all go through it).

- **The lobby owns the UI thread, so the fixture reaches it through its own message loop.** `EETestFixture.cpp`
  posts a `UserEvent` to the lobby's `WindowManager` (`PostEvent` is thread-safe; `GetNextEvent` runs `ProcessEvents`
  inside every nested dialog loop too); its `Deliver` runs on the lobby thread, executes the pending command and
  takes a snapshot the fixture's poll copies into `ui-state.txt`: `lobby shown/screen/lan/login/modal/game/host/
  clients/ready/allready/name`, one `wonctl <id> <x> <y> <w> <h> <enabled> <text>` per visible component with a control
  id (root coordinates of the 1024x768 layout; a button's text), `wonlist <k> items <n> sel <i>` per visible list area,
  `wontext <text>` per label of the modal dialog (a message box's message). Nothing of the lobby's is touched from the
  fixture thread (the component tree is refcounted and relaid out on the lobby thread). The commands: `wonclick <id>`
  (a mouse move, press and release at the component's centre through the window's event queue, the real button path;
  the modal component is searched first), `wonactivate <id>` (`Button::Activate`), `wonkey <code>` (a typed
  character to the focus: the dialogs' `RequestFocus` input), `wonselect <k> <item>` (`ListArea::SetSelItem` with its
  event) and `wonclickat <x> <y>` (a tab, a list row). The verdict ("TEST UI lobby-clicked ...") comes from the lobby
  thread; the fixture's own line says "lobby-posted". Components have no names at run time (the .cfg names live in
  `ComponentConfig` during the load), so the control ids are the handles: `WONLobbyResourceGlobals.h`
  `LobbyResourceCtrlId` (200009 Other Multiplayer, 200011 Host Game, 200013 Join, 200015 Start/Ready, 200016 Refresh,
  200019 Add LAN name), `ID_Ok` 100001, and a `WONTabCtrl`'s tab buttons carry their index (the LAN tab's is 0: not
  listed, clicked at the Direct Connect tab's left).
- **Three things kept two instances apart on one machine, none of them the lobby's logic.** (1) The single-instance
  mutex: the second game exited with 0 at once (`EECreateInstanceMutex`); under `EE_TEST_UI`, `EE_TEST_INSTANCE=<name>`
  names the mutex per instance (TEST ONLY). (2) The LAN discovery socket was bound exclusively (`Fn2B0D90(port, 0)`),
  so the second lobby's bind of 0.0.0.0:8585 failed and it neither saw nor advertised a game; it binds with address
  reuse now (`LanLogic::StartDiscovery`): on Windows two reuse-bound UDP sockets both receive the limited broadcasts
  (255.255.255.255, `IPAddr::GetBroadcastAddr`), a unicast reaches one of them (measured with a Python probe), and
  the host's summary broadcasts (`AdvertiseGame` at creation, every 30 s from the discovery timer) are what the
  joiner's list is built from. The listening socket (8888) is the host's alone; the lobby's UDP port (`LobbyBindUDPPort`)
  already reused. (3) The default LAN name is "Unknown" plus a number made from the local IP address
  (`GetDefaultLanName`: CRC-16 of the address), the same in both, and the host refuses a duplicate name ("The name you
  have chosen is already being used by another player", read off the `wontext` dump): the driver sets a name per end
  through the Options tab (Add -> the name dialog -> OK -> back to the LAN tab).
- **What the driver waits for.** The lobby's snapshot stops with its message loop, so the end of the lobby is read
  from the game's own forms: `EELobbyShowGameScreen` pushes form 30 before `EELobbyHide`, then the game form and the
  telemetry (`wait_running`: a map and physics ticks). The game's port (33334, INADDR_ANY) is the host's only; the
  joiner connects by the host's IP from the discovery packet (`GetMyLobbyGame()->mAddress`) and binds an ephemeral
  port. Two games share one stage (data root): the game logs are named by the start second (the ends start 3 s
  apart), `_wonlobbypersistent.dat` is written by both (harmless).
- The lobby frames are captured through the fixture's read-back (1226x768 at this window size); the hit tests and
  the dumped coordinates are the 1024x768 layout's, which `PlatWindowManager::ScaleToSurface` maps the real pointer to.

### WON's async-op machinery in Rust (`ee_won/src/asyncop.rs`, `ffi_ops.rs`)

`AsyncOp`'s run state and its transitions (`Run`, `RunAsTimer`, `Finish`, `ForceFinish`, `Kill`, `Complete`,
`StartRunAsync`'s timer decision, `SetTimeout`, `SetMode`), `TimerThread`'s two deadline maps with the tick-wrap
switch and the wait time, `AsyncOpContainer`'s queue and the pump's accounting (the max-ops and wait-time cut-offs),
`SocketThreadSimple`'s op set (address order, as the `SmartPtr` comparator's), `SocketThread::DoOp`'s finish rule and
`QueueSocket::OpCallback`'s decisions (which completion, close or not, the next queued op or the repeat op's rerun)
are Rust. The C++ classes keep their objects, locks, threads, events, completions and virtual hooks.

- **The op's members stay, Rust gets a copy.** `mStatus`, `mKilled`, `mCompletionPending` are read all over the WON
  library, so `AsyncOp` keeps them and `GetState` / `SetState` copy them into an `EeWonOpState` under `mLock` around
  each transition; the transition returns what to do (queue in the container, arm the timer, time out now, run
  blocking, deliver the completion). `Kill` now takes the lock around its flag (the original set `mKilled` outside
  it); nothing else changed order.
- **Queues hold handles, the C++ holds the references.** The original queues were `std::list` / `std::multimap` /
  `std::set` of `SmartPtr`s. The Rust queues hold the op's address; the C++ side `AddRef`s when it hands an op in and
  `Release`s when the queue hands it back (the container's pop into `mCurPumpOp`, the timer's due and purged ops after
  `Finish` / `Kill`, the socket thread's finished ops after `DoOp`). A sink callback delivers the ops only after the
  queue's own borrow ended, so a sink may use the queue again; the socket thread's pump iterates a snapshot and
  removes each finished op by value, as the original erased while iterating under its lock.
- **A timer entry is a key, not an iterator.** `AsyncOp::mTimeoutMap` / `mTimeoutItr` became `mTimerQueue` /
  `mTimerKey`. After a 32-bit tick wrap the original copied the wrap-around map over the current one, which left
  every queued op's iterator dangling (a later `RemoveFromTimerMap(true)` erased through it); a key survives the
  switch. The arithmetic is the original's: a deadline past the wrap is `timeout - (0xffffffff - now)` of the next
  era, the wait with no timer `9 - lastTick` (unsigned).
- **The container's `PurgeOps` leaves `mCurPumpOp` on the last op** (the original did); kept.
- **Capture first:** `won_ops_test` gained scripted checks of the machinery (a hook that finishes with a scripted
  status or stays pending: a blocking run, a zero timeout, `Run` while pending and while awaiting completion,
  `ForceFinish` on an idle op and while the completion waits, `Kill(false)` then `Kill(true)`, the timeout frozen while
  running, the container's completion order and `Pump(0, 2)`, a zero timer, a timer finished by hand). They were run
  over the C++ machinery first (one expectation corrected: a blocking op's `TimeLeft` counts down from its run, it is
  not 0), then over the port: PASS, with `won_socket_test`. 96 `ee_won` tests (the wrap, equal deadlines' order, the
  pump plan's cut-offs, the decision table).
- `SocketThreadWinsock2.cpp`, `SocketThreadEx_Windows.cpp` and their headers are deleted: never linked
  (`units.WON_NOT_LINKED`), the game runs the polling `SocketThreadSimple`.

### StringParser in Rust (`ee_won/src/parser.rs`, `ffi_parse.rs`)

The cursor's reads (`ReadString` into narrow and wide strings: bare and quoted tokens, the escapes, `L"..."`,
`"a"+ "b"`, the trailing-whitespace trim; `ReadValue` of integers and booleans), `SkipWhitespace`,
`SkipNonWhitespace`, `CheckNextChar`, `StartsWith`, `Is`, `GetChar`/`GetWChar`/`EndOfString` and the decimal
formats are Rust. The C++ `StringParser` keeps its text pointer, position and width; the list readers and the list
formats (StringParserCollections.cpp, StringParserLists.cpp) stay C++ loops over these primitives.
StringParserNumbers.cpp is folded into StringParser.cpp; StringParserFormat.cpp (never linked, a duplicate) is deleted.

- **The ctype calls stay the C runtime's.** The game's UI thread runs `setlocale(LC_ALL, "English")`, and in that
  locale `isspace(0xA0)` is true: the golden's `english` pass differs from the `C` pass there (a no-break space ends a
  token). So `ffi_parse.rs` calls the CRT's `isspace` / `isdigit` / `isxdigit` / `isalnum` / `toupper` / `tolower`
  with the original's exact arguments, including `isalnum` of a `toupper` result cast to `char` (a byte above 0x7f
  arrives negative); the Rust tests use a C-locale `Ctype`. Superscript digits (0xB2, 0xB3, 0xB9) are not digits in
  either locale.
- **The text is read by index, not by length:** an escape at the end of a quoted string steps one past the
  terminator and back, as the original did, so the Rust side reads the C string through a raw pointer
  (`RawText`, units of 1, 2 or `sizeof(wchar_t)` bytes) instead of measuring it.
- **The integer reads wrap in a 32-bit int**, the 64-bit read too (the SDK accumulated in an `int`).
- **Behaviour change:** the header's sized `ReadValue` wrappers (`long`, `short`, `char`, ...) store only on success;
  the original assigned an uninitialised `int` after a failed read (clang happened to leave the output alone, which
  is what the golden recorded). On LP64 the `long` formats print the whole value (the original's `"%d"` of a `long`).
- **Capture first:** `won_parse_test` runs every read and format over 53 inputs in both widths and both locales and
  compares with `rust/crates/ee_won/src/golden_parse.txt`, recorded over the C++ before the port (3537 lines); the
  Rust test `parser_golden` replays the C-locale pass and the formats line. `WStringToString` of a wide string with a
  NUL stops at the NUL (`std::string(buffer)`), which the replay models.
- The Bash tool of this session collapses `\\` to `\` in heredocs and inline Python: write files with backslashes
  (C escapes, raw strings) through the editor, not the shell.

### The LAN protocol in Rust (`ee_won/src/lan.rs`, `ffi_lan.rs`)

LanLogic's messages are Rust: the game channel's (`WriteBuffer(2, true)`: join request, join reply refused and
accepted, client joined and left, chat request and line, ping, the staging wrapper) and the discovery packets' heads
(`WriteBuffer(0, true)`: summary, keep-alive, removed, query), both built and read, and the host's decisions: the join
status (the subnet check of a LAN game, then the password), the chat route (whisper, team, everyone) and the shown
flags, the network timer's stale-socket and ping rules, the discovery timer's summary-or-keep-alive rule, and
`GetNextClientId`. The C++ keeps the objects, sockets, events and `LobbyGame::WriteSummary` / `ReadSummary` (the
summary body) and the staging logic's own reads.

- **A message is read whole before anything acts on it.** The handlers now take the decoded fields (the header's
  `HandleJoinRequest`, `HandleClientJoined`, ... signatures changed). Every original handler read all its fields
  before acting, so a short message still does nothing, except `ReadJoinReply`, which set the game's captain id and
  added the players one by one as it read them: `read_join_reply` returns what had been applied before a short read
  (the captain once read, the players read whole) and the C++ applies exactly that.
- **`GetNextClientId` hands out 0 once after 0xffff** (it steps 0xffff to 0 and returns it); the comment said it
  skipped both. Kept, documented.
- The host delivers its own messages with their 2-byte prefix (`HandleMessage`'s offset); messages from sockets
  (`RecvMsgOp`) come without it.
- **Capture first:** `won_lan_test` builds every message with the original's `WriteBuffer` statements (copied from
  LanLogic.cpp before the port) and through the Rust ABI and compares the bytes, then reads them back (a short
  message, a reply cut inside a name). The LAN game (`lan_game.py`) passes over the port; the chat, the refusals and
  the timers' rules are not exercised by it (only by the tests).

### State and what is left

- `lan_game.py` on the branch head before this work (the test-profile exe of the merged tree): **PASS**, host and
  joiner both at PT 2795 / CT 205 after 60 s of play (`build/lan_game/baseline`). After the StringParser and LAN
  ports: **PASS**, PT 2744 / 2757, CT 200 / 201 (`build/lan_game/ported`). The earlier `EE-SIM` fault on seed 4
  (`NERecordPlayback::Process` from `NESynchronousServer::ProcessLocalUserInput` at the random-map screen, a null
  record-playback object) did not repeat in the rerun; the oracle passed on seeds 2-7. It is a race in the sim's
  start, not WON code; reported, not fixed.
- Still C++ of WON: AuthContext, PeerAuthClient/Server and their ops (the LAN path uses peer auth only for direct
  connect games with the CD-key check, `GetGameType() == 3`, so the LAN test does not reach it), LobbyLanSocket's
  object, the directory, routing, DB, HTTP and misc ops, FileReader/FileWriter, the config objects (WONConfig), the
  lobby logic (WONLobby) and WONGUI.

## D6: game text is UTF-8 (branch rust-d6-utf8, 2026-11-10)

The owner's decision D6 (docs/RUST_PLAN.md): game text becomes UTF-8 in memory, in files and in saves. A declared
rebaseline, done as its own track: outputs change where text was Windows-1252, old inputs are converted. The simulation
does not read text, so the sim oracle stays identical; menus with Latin-1 text render pixel for pixel as before.

### The plan (written before the work; the results follow in "As built")

1. **Inventory.** Where text enters: the db tables' text cells and lead strings (already UTF-8 on disk: the converter
   decoded retail's cp1252 bytes into UTF-8 YAML/CSV; the retail tables hold no byte above 0x7F), the Language strings
   (`Language.rc`, five strings with `\x2019`/`\x00a9`, compiled into `LanguageStrings.cpp` as cp1252), `.rmv` scripts
   (one `©` in a comment), scenario, campaign and civilization files (UString/UWideString `Load`: cp1252 bytes), saves
   (0.254, the same), `config.yaml` (bytes), player-typed input (SDL: UTF-8, converted to cp1252 in `event.rs`), the
   clipboard (the same), WON's wire and files (UTF-16 LE units; the lobby's `.cfg` are UTF-16 with a BOM, its help pages
   cp1252), file names and paths (bytes to `CreateFileA`/`fopen`). Where it is interpreted: `UString` (byte operations,
   `ToLower`/`ToUpper` through the CRT's `tolower`, which `setlocale(LC_ALL, "English")` makes code-page aware),
   `UWideString` and the UI (`PlatMbs*`: the Windows CRT's `_mbs*` under the process code page, a single-byte port on
   POSIX), `GetMultiByteChar` (`UIMultiByte.h`), `ee_ui`'s `text.rs` (stepping, spaces, digits, the case-folded sort),
   `GEText`/`PlatFont` (glyphs by byte: cell `b` draws the code point cp1252 gives `b`), `GEFontBitmap` (a 256-cell atlas
   by byte), WON's `StringUtil` (`PlatCp1252ToWide`/`PlatWideToCp1252`), `PlatGUIFont`, `PlatClipboard`,
   `PlatWindowManager`'s `PLAT_EVENT_TEXT` (one wide character per byte), the edit box's `PLAT_EVENT_TEXT` loop (one
   `UpdateTranslatedKey` per byte).
2. **Representation.** `UString`, `UWideString`, the records' `char[]` fields and every `const char*` hold UTF-8. A
   "character" of `UWideString`, the UI and the `PlatMbs*` functions is one code point; a byte that is not part of a
   well-formed sequence is one character of its own (drawn as U+FFFD, '?' when the font lacks it), never a crash and
   never a step past the terminator. Case folding is Unicode's simple lower-case mapping (Rust's `char::to_lowercase`
   when it gives one character): `PlatStrICmp`, `PlatMbsICmp` and the list boxes' order fold the same way on every
   system (the UCRT folded to lower case: `'_'` sorts before the letters, kept). In-place `ToLower`/`ToUpper` map only
   where the mapping keeps the byte length (`É` <-> `é`, Greek, Cyrillic; `ı`/`ſ` and `ß` stay).
3. **One implementation.** `ee_plat_core::utf8` holds the code point decoding/encoding and the `PlatMbs*`/`PlatStrICmp`
   family over it on every system; the CRT bindings and the POSIX single-byte port (`sbcs`) and their goldens go
   (the ASCII rows of the 1252 golden stay as the pin). `GetMultiByteChar` returns the code point (`PlatUtf8Decode`).
   `PlatCp1252ToWide`/`PlatWideToCp1252` become `PlatUtf8ToWide`/`PlatWideToUtf8` (UTF-16 units on Windows, UTF-32
   elsewhere), plus `PlatCp1252ToUtf8` for old inputs. `UString::ToLower`/`ToUpper` go through `PlatMbsLwr`/`Upr`; the
   UI thread keeps `LC_CTYPE` in the "C" locale so no CRT `tolower` touches a byte above 0x7F.
4. **Fonts.** `PlatFont` keys glyphs by code point: 0x20..0xFF rendered at creation (as before), others rendered on first
   use under the font's lock. `measure`/`draw` decode UTF-8. `GEFontBitmap` keeps its 256 cells, each the glyph of the
   code point cp1252 gives the byte, and maps a drawn code point back to its cell (code points outside cp1252 are
   skipped, as bytes outside the font were). So a Latin-1 or cp1252-representable string renders exactly as before.
5. **Data.** The text compiler (`ee_data`) stops encoding to cp1252: a text cell's UTF-8 is validated and kept. The Python
   importer's intermediate `.dat` holds UTF-8 too (`scalars.encode_str`/`decode_str` are UTF-8; retail's cp1252 bytes
   are converted once where they enter, `binfmt`'s retail parse), so `check` (b) compares the converted retail value
   with the compiled one and `cppgate` compares two UTF-8 implementations. The content dump escapes code points, not
   bytes. `lang_strings.py` writes UTF-8. The digests recorded from the C++ compiler apply to ASCII data unchanged; a
   parity case whose text has a non-ASCII character gets its digest re-recorded against the Python importer and the
   change is listed. `open_data.py` converts the lobby's cp1252 help pages to UTF-8 when it builds the tree.
6. **Saves.** `kSaveGameMinorVersion` 255. A save, scenario or campaign file of version < 0.255 loads its strings through
   a cp1252 -> UTF-8 conversion in `UString::Load`/`UWideString::Load`, switched on by the loader that read the header
   (`ULegacyTextLoad`, a process-wide flag held while such a file loads). New files are written as UTF-8.
7. **WON.** The game's bytes meet WON's `wchar_t` in `StringUtil.cpp`: those conversions go through `ee_won`'s `text.rs`
   (`utf8 <-> UTF-16 units`, lone surrogates to U+FFFD on the way out). `PlatGUIFont`, `PlatClipboard` and
   `PlatWindowManager` use `PlatUtf8ToWide`/`PlatWideToUtf8`; the lobby's text pages are read as UTF-8.
8. **Tests and gates.** Unit tests for every conversion and operation (round trips, invalid UTF-8, folding, sorting,
   wrapping with multi-byte characters) in `ee_plat_core`, `ee_plat_ui`, `ee_ui`, `ee_won`, `ee_data`; `plat_test`,
   `font_test` and `wontest` updated. Gates per commit: both links 0/0, the sim oracle identical to `ref`, the Rust
   gate, `eedata check` and `cppgate`, `xbuild.py test`/`fonttest`/`wontest`.

### As built

- **One text module.** `ee_plat_core::utf8` (decode/encode, stepping both ways, the folded compares, the in-place case
  change, the cp1252 table, UTF-16/UTF-32 conversions) and `string.rs` over it: the `PlatStrICmp`/`PlatMbs*`
  family, `PlatUtf8Decode`/`Encode`, `PlatCp1252ByteOf`, `PlatUtf8IsValid`, `PlatUtf8ToWide`, `PlatWideToUtf8`,
  `PlatCp1252ToUtf8`, the same code on every system. The CRT bindings, the POSIX single-byte port, its 932 and SBCS
  goldens and `ee_plat_ui`'s `cp1252.rs` are gone; the 1,280 ASCII rows of the Win32 1252 golden still pin the
  ASCII behaviour bit for bit. A stray byte decodes to `0xDC00 + byte` (so two texts compare equal only when their
  bytes do, and every byte string round-trips through decode/encode); it is drawn and sent as U+FFFD.
- **Who reads characters.** `GetMultiByteChar` is `PlatUtf8Decode`; `ee_ui`'s `text.rs` `Platform` answers from
  `utf8` (and `Mbcs::char_at` replaced the byte arithmetic of `multi_byte_char`, whose original lead/trail form stays
  the trait's default for the golden test doubles); `UString::ToLower`/`ToUpper` are `PlatMbsLwr`/`Upr`; the UI
  thread sets `LC_CTYPE` back to "C" after the language's locale; the edit box gets one `UpdateTranslatedKey` per
  character, WON's window manager one key event per UTF-16 unit; `EEWonNet::A2W`/`W2A`, `StringUtil.cpp` (through
  `ee_won_wide_from_utf8`/`ee_won_utf8_from_wide`, `text.rs`), `PlatGUIFont`, `PlatClipboard` and the SDL
  clipboard/text events carry UTF-8 unchanged; the lobby pages are read as UTF-8 (a page that is not is read as
  1252). The AI manifest's names (`EECPEngine.cpp`) are kept as UTF-8 (strays to U+FFFD) where they were squeezed
  into 1252.
- **Fonts.** `PlatFont` renders U+0000..U+00FF at creation and any other code point on first use (a boxed glyph in a
  locked map: the pointer outlives the lock). The bytes 0x80..0x9F of 1252 are now their own code points' glyphs (the
  euro sign is U+20AC, rendered on first use), so the GDI-port goldens are compared through the cp1252 table and still
  match. `GEFontBitmap` keeps its 256 cells (cell `b` holds the character 1252 gives `b`, as UTF-8) and maps a drawn
  code point back with `PlatCp1252ByteOf`: Latin-1 and 1252 text draws exactly as before, other characters are
  skipped there.
- **Data.** The text compiler keeps a text cell's well-formed UTF-8 (`encode_text`), the content dump escapes code
  points (a stray byte as `\udcXX`); `eedata`'s `load_db` converts retail's strings once (`binfmt.transcode_retail`:
  the retail tables are ASCII, so `check` and `cppgate` see the same bytes as before: the exported text tree is
  identical to rust's byte for byte); `scalars.encode_str`/`decode_str` are UTF-8, `decode_cp1252` is for retail
  inputs (`gltf.py` reading a `.cem`). The glTF importer (`gltf.py`, `ee_openformats`' `game_name`) keeps a name's
  UTF-8. `lang_strings.py` writes UTF-8 (five strings changed: `’` and `©`). `open_data.py` converts the 22 lobby
  pages (one, `Help/loginhelp.html`, had a 1252 byte; the db part is unchanged, so no db release is needed for this;
  the media part would carry the converted page).
- **Saves.** 0.255. `USetLegacyTextLoad` is set by `EEPSaveGameHeader::Load` around the body and by `LoadGame` /
  `EELoadPersistent` around the chunks when the version is older; `UString::Load`/`UWideString::Load` then convert
  every string from 1252 (`UConvertLoadedText`, which also converts any loaded string that is not well-formed
  UTF-8: the save name and description, read before the version, and files without a version such as old `.civ`
  binaries).
- **Tests.** `ee_plat_core` (utf8: 9, string: 4 including the ASCII golden), `ee_plat_ui` (font: code point
  measure/draw, Latin-1 drawn the same as its 1252 bytes; event: the text golden through `text_into`), `ee_ui` (UTF-8
  stepping, folding/sorting, `char_at`, wrapping by code point against the ASCII equivalent, the edit box typing,
  deleting, selecting and limiting by characters), `ee_won` (both `wchar_t` widths, strays, lone surrogates),
  `ee_data` (cells, quoting round trip, dump escapes), `ee_openformats` (names); `plat_test`'s "utf8" and "wide"
  sections, `sdl_test`'s clipboard, `won_gui_test`'s accented line.
- **Gates run (Windows):** both links 0/0, sim oracle identical to `ref` on seeds 2-7, Rust gate, `eedata check`
  and `cppgate` PASS (the cppgate golden digests no longer match the data on rust either: Python-only comparison, as
  before this branch). Menus were not looked at (no harness session): the lead's tours must confirm the pixels.
- **Left:** the fonts' coverage (Liberation: Latin, Greek, Cyrillic; no CJK, so CJK input shows `?`);
  `tools/rmap_gate/rmap_stubs.cpp` still maps `PlatMbs*` onto the CRT (a tool gate's stubs, ASCII inputs); campaign
  archive tags are not converted; the harness writes `config.yaml` as cp1252 (its text is ASCII); `GEFontBitmap`
  cannot draw characters outside 1252.

## R4.3, fifth part: the visible terrain as a plan, the viewport's math, the model's viewport test, the bitmap and rhombus corners (ee_ge terrain_visible.rs, viewport.rs, bitmap.rs; branch rust-r4-ge5, 2026-11-10)

`GETerrainMesh::UpdateVisibleTerrain` with `AddTilePolys`/`AddWaterPolys`/`AddCliffPolys` and the eight
`Generate*EdgePolygons`, `GEViewport`'s matrices, clipping planes and frustum, `GEPhysicalModel::Intersects(viewport)`,
`GEBitmap`'s and `GERhombus`'s corners and the stretched texture coordinates are Rust. The mesh, its render-vertex
arrays, the material lists keyed by `GETexture*`, the reference counts, the polygon arrays, the viewport object and the
bitmaps' vertex arrays stay C++. The graphics thread and the UI call all of it (not the simulation); the sim oracle is
identical.

- **Plan and fill.** The original built `std::map<GETexture*, unsigned long>` material maps, referenced textures and
  filled polygon pointers into the mesh's arrays as it walked the visible tiles. Rust now computes the whole frame
  into plain data (`terrain_visible.rs`, `VisiblePlan`): the visible rows, the two point-visibility arrays, the
  materials as texture handles (`GETexture*` values; a material's index is its position, the untextured one first,
  then on space maps one per cliff texture), every terrain and water polygon with its corners as references (a
  terrain or water render vertex `[a][b]`, a skirt vertex, a cliff vertex), alphas, texture coordinates and material
  index, the skirt and cliff world points, and per cliff vertex the terrain vertex whose visibility it follows.
  `GETerrainMesh.cpp` reads it through `EeGeVisibleOut`, releases the last frame's materials as before, rebuilds the
  map and list from the handles (one `Reference()` per non-null entry), creates the skirt and cliff
  `GETMRenderVertex`es, maps the references to pointers (`FillPolygon`) and resizes the index lists. The texture sets
  go in as flattened handle grids (`GETextureSet::GetTextureAt`); the 33 x 33 water texture grid in place.
- **Quirks kept, now stated in the Rust.** A tile layer's "best" texture (the one drawn opaque) is the lowest
  `GETexture*`: the original compared addresses, the Rust compares the handles as integers. A material map `insert`
  keeps the first entry of a repeated texture while the list gets both (the cliff textures can repeat): the Rust's
  `push_material` does the same, and `material_of` is the map's lookup. A skirt polygon never sets its texture
  coordinates, so the slot keeps the previous frame's values (`uv_set` is false and the fill leaves them; the golden
  test replays the 12,000 slots). `theQuad`'s alphas carry across a tile only where the original's did (every used
  layer sets all four). The `(long)(x + 0.5f)` truncations, `theCliff % 8`, the `unsigned long` array indices that
  wrap and the `0x7fffffff`/`0x80000000` empty bounds (which wrap through the span clip) are the C++'s.
- **One declared difference: the polygon arrays' capacity.** `mTerrainPolygons` holds 12,000 polygons and
  `mWaterPolygons` 5,000; the original wrote past them (into the render-vertex arrays) when a frame had more. The fill
  stops at the capacity. The golden driver found 38 of its 1,280 frames over it (wide far cameras over large maps;
  the game's zoom caps keep it below), printed as `skip` on the C++ side; the relinked driver prints the clamped
  frames there and the same bytes everywhere else.
- **Two defensive reads.** A tile's texture set id past the registered sets reads 0 (the C++ indexed past its
  vector), and a space map without its 13 cliff textures gets handle 0 for the shoreline layer (the C++ read past the
  vector; the renderer loads them before the first frame). The water material index is checked against the water
  list (the C++ indexed unchecked; a mesh whose water state was never set has no water materials).
- **`GEViewport` is a mirror** (`GeViewport`, 0x288 bytes, `GERustViewport.h` pins the offsets through a friend): the
  constructors, `Compute{Orthographic,Perspective}Matrix`, `ComputePlanes`, `GetFrustrumWorldPoints`/`Planes` and the
  setters run on the object itself; the header inlines (`SetCamera`, `SetOrthographicRectangle`, the normalizations)
  stay and call the forwarded members, so they are bit-identical by construction. The default constructor is Rust
  too (`GE3DPlane()` faces +z, the matrices are identity, the look-at vector is the identity's +y).
- **`GEPhysicalModel::Intersects(viewport)`** is `physical::model_visible` over the placed model's values (location,
  scale, the frame's internal transform, centre, radius, box).
- **`GEPhysicalModel`'s walks** (`physical_walk.rs`, a later commit of this branch): `Intersects(line)` (sphere, box,
  the triangles of LOD 0, the sub-models), the model scaled per axis (its hand-built inverse, the `max`/`min` macros'
  operand order) and model against model (the reduced LODs through `lrintf(0.8f * count)`, the other model's
  triangle boxes cached during the first triangle) are Rust over a callback table (`EeGePhysicalModels`) of
  `GEModel*` handles: the C++ statics in `GEPhysicalModel.cpp` read the accessors, the virtual vertex and polygon
  arrays and do the `unsigned long` index sum. Units test projectile lines against target models through these
  (`EEUnitClass.cpp`), so RUST_PLAN 2.1 applies. The exact retry (a top model with no hit tests the line at its
  smallest scale) builds a temporary `GEPhysicalModel`, whose constructor and destructor reference and release the
  model (`GEModel::Dereference` can reach its manager): it stays a callback (`retry_line`) into the C++, so that side
  effect is the original's. The FFI copies its inputs and writes the distance back once, so no Rust reference to a
  C++ object lives across a callback. Golden (`build/physical_golden`, scratch): the whole `GEPhysicalModel.cpp` of
  the commit before this work, over fake models in raw `GEModel`/`GEMaterial` storage (their constructors are not
  linked; three references held so a release never reaches the manager) with fake vertex and polygon arrays, 4
  worlds of 12 top models with sub-models to depth 2 (1 to 4 LODs, 1 to 3 materials, empty ranges, 1 to 3 frames),
  12,000 line cases (scales 1, 0, 0.00005 and random; every LOD; exact or not) and 12,000 scaled cases (uniform,
  per-axis and a zero axis): `golden_physical_walk.txt`, replayed by the Rust test from the same stream (hits at
  every LOD: 2,191 / 1,937 / 1,316 line hits, 2,681 scaled hits); the driver over the forwarders printed the same
  bytes.
- **`GEBitmap`/`GERhombus`**: `ComputeRotation`'s corners and `ComputeStretchedCoordsAndScale`'s arithmetic
  (`bitmap.rs`); the vertex arrays behind `GEVertexArray`'s virtuals and the textures stay C++. The original passed
  the bottom-left corner as `SetTextureCoordinates`' third argument, which the header names `inBottomRight`; the
  forwarder passes the same positions.
- **The golden driver** (`build/visible_golden`, scratch): the unmodified `GETerrainMesh.cpp` and `GEViewport.cpp`
  with the engine's math units and fake textures (`lle/GETexture.h` shadowed by a `/I` directory: a pool of 48
  reference-counted stand-ins in an array, so their addresses rise with the index as the handles do in the test),
  `#define private public` around `GETerrainMesh.h` only (around `GEViewport.h` too it re-mangled the private
  members the header inlines call, `?ComputeOrthographicMatrix@GEViewport@@AEAA` became `QEAA`, and the link lost
  them), `GEBoundingBox::ComputeDimensions` defined by the driver over the Rust (the unit drags the engine's static
  objects in), `/FORCE:UNRESOLVED` for the file classes nothing calls. 40 meshes from an LCG (flat, hilly with water,
  space with shorelines, hilly with random sets; 1 x 1 to 96 x 96; integer-hash value noise so the C++ and the Rust
  draw the same heights with no `libm`), 24 cameras each (perspective and orthographic, near and far, `SetOrientationYPR`
  and a look vector), the mesh dumped as decoded references (a vertex pointer located in the arrays or the vectors),
  the material lists as indices, the fake textures' reference counts (`refs`, C++ only), the viewport's 162 words
  after construction and after each setter, the frustum points and planes, `Intersects(viewport)` over random placed
  models, and the bitmap functions' bodies compiled alone. 1,280 frames, 1,920 viewport cases, 1,920 model tests,
  9,000 bitmap cases; `golden_visible.txt` is 230 KB (one line per frame, so a divergence names its mesh and camera).
  The Rust test replays the stream through `GeViewport::new`, `update_visible_terrain`, `model_visible` and
  `bitmap.rs`; the driver relinked over the forwarders printed the same file (the `refs` section included).
- **The golden driver, checked again** (a second agent; `build/visible_golden`, `build.py --tree orig|cur`). The
  first driver compiled whatever `src/` held, and its bitmap and model sections were copies of the function bodies
  in the driver, so relinking it proved nothing about those forwarders. Now `--tree orig` compiles a `git archive` of
  the commit before this work (its `src/`, `include/` and its own `rust/` workspace and `ee_rmap_gatelib`), `--tree
  cur` the worktree; the member functions of `GEBitmap`, `GERhombus` and `GEPhysicalModel::Intersects(viewport)` are
  copied out of the tree's own units (`extract_members.py`) and run on objects in raw storage with a fake vertex
  array that records every `SetPoint`/`SetTextureCoordinate` (the `obj_*` sections: 4,000 cases each, 6,000 model
  tests over four frames whose other frames are wrong on purpose). The orig driver reproduced `golden_visible.txt`
  exactly (so the committed golden is the original's). **Real meshes:** 15 random maps from EE-SIM (`--save-at
  1000`; every map type: Continental, Highlands, Large and Small Islands, Mediterranean, Plains, Tournament Islands
  and the five Planets maps, Tiny to Gigantic, 42 x 42 to 220 x 220), the map chunk of each save unpacked by
  `tools/harness/savefile.py dump` (PK01, DCL), the mesh found by a scan for `Save`'s layout (size, tiles, points) and
  loaded through `GETerrainMesh::Load` over a memory file; 255 fake texture sets (the real ids reach 92), 48 cameras a
  mesh (three of four game-like: pitched down, 8 to 55 away, small orthographic rectangles): 960 real frames, 851
  within the arrays. `compare.py`: 4,412 lines equal, the 147 frames the original overran (38 synthetic, 109 real)
  clamped by the Rust, no other difference.
- **Gates run:** `build.py -j 3 --sim` 0/0 (EE-AOC, EE-SIM), the sim oracle identical on seeds 2-7, `rust_build.py gate`
  PASS (1,250 tests; `ee_plat_core`'s scaled-wait timing test failed once under the oracle's load and passed alone),
  `eedata check` PASS, `eemodel.py cppgate`, `gx_test --run`, the windowed-software test profile 0/0 (the numbers in
  the final report). Not run: tours (the lead's; the visible terrain is what they check).
- **Left in R4.3:** the fonts (`GEFont`, `GEFontBitmap`, `GEText`: UTF-8 first, D6), `GEBitmap`'s and `GERhombus`'s
  vertex-array plumbing (no logic).

## R5a.2, second round: the AI's and the path manager's running state, area effects, tile order (save 0.256, branch rust-r5a-aisave, 2026-11-10)

Continues "R5a.2: load equals continue". Same test (`sim_oracle.py saveload NAME --seeds 2-7 --at 600,1350,2100`);
the saves land at game ms 600250, 1350125 and 2100000 every run. The format of this round was 0.255 on the branch
and became **0.256** when the D6 merge took 0.255 for UTF-8 text (`EESaveHasRunningState` is `>= 256`; a 0.255 save
loads with these fields absent, as an older one).

- **What 0.256 saves** (each put back at the loaded game's first tick through `EEAfterLoad` unless said):
  - the computer players' round-robin schedule and each computer player's own timers and flags (previous agent);
    the units' contacts, animation frame and exact counters (previous agent);
  - the resource manager's timers (Update's first-update re-initialisation no longer reschedules them) and its
    forest, animal and fish lists (RefreshResources made new ones at the first update, so gatherers were sent
    elsewhere: one of player 3's gatherers at seed 2's 2100 s save, 9 ticks after the load). A forest is saved by its
    first tile, because the continent manager is rebuilt at load with its own ids;
  - the research manager's started and history flags;
  - the path manager's request history, region maps and timers (`EEPathManager::SaveRunningState`);
  - the area effects (`EEAESaveRunningState`): each unit's own effects' locations and morale-from-buildings count,
    the effects acting on each unit as (owner, index), and the manager's active list. After a load these lists were
    empty until the next area-effect tick (up to 2 s);
  - the order of the units on each tile: a load placed them in its own order, and `EEComplexUnit::FindContacts` walks
    the tile lists, so contact lists came out reversed within a few ticks. The units are moved between the tile's own
    nodes, and each node keeps its reference;
  - each attack sub group's targets and siege targets (made by Restore), `mNextPositionTime`, `mGatherDeadline`, the
    area-effect and shore flags, and its path request. A finished request is rebuilt with its path. A running one is
    asked again at the first tick with the saved ends (the A* state itself is not saved), and a land group no longer
    falls back to "prepare travel" on a 0.256 load;
  - no format change: RepairPointers' MakeAreaEffects no longer overwrites the loaded area-effect icon count.
- **Tools** (TEST ONLY, committed): the state dumps now show each unit's contacts, last attacker, owner, holder and
  area effects (`refs` line) and every attack sub group (`ASG` line). The scratch scripts under the worktree's
  build/ (`pair.sh`, `ticks.sh`: a dump after each of the first N ticks of both games, `dumpdiff.py`, `rngdiff.py`)
  are not committed. A temporary goal trace in `EEComplexUnit::SetGoal` (stack per SetGoal, like `--rng-trace`)
  pinned the seed 3 divergence and was removed again.
- **The numbers** (merged with rust f01127dc+, oracle identical to ref at every commit): still **0 of 18** saves
  hold to the end, before and after. First divergent CT line after the load, before -> after: seed 2 4/14/0 -> 4/2/0,
  seed 3 0/0/0 -> 0/0/0, seed 4 0/1/0 -> 0/1/0, seed 5 0/12/13 -> 0/11/12, seed 6 11/12/0 -> 20/11/0, seed 7
  10/23/0 -> 3/22/1 (600/1350/2100 s; one CT line is 14 ticks). The lines are coarse. In the tick-by-tick dumps
  (`ticks.sh`), seed 2's 2100 s save now matches unit by unit for 16 ticks, where before the first unit difference
  came at tick 9. Seed 3's 600 s save matches for 4 ticks.
- **Still different after a load** (the next causes, measured):
  - *seed 2, 2100 s, tick 9*: player 3's build queue gets a drop site in the loaded game only (the `ai` part of the
    first CT line of saves 2/4/6 at 2100 s). The saved resource lists did not remove it, so the cause is elsewhere
    in the build or resource manager's derived state;
  - *references to dead units*: the running game keeps a dead attacker in `mLastAttacker`, held alive by its
    reference count. The save writes its id, and the load finds no unit. Triggers such as AttackerIsNotBuilding
    read the dead unit's class, and the `refs` lines of every save differ there (5 units at seed 2's 2100 s);
  - *seed 3, 600 s*: at the 5th tick after the load, `EEUCProjectile::EvaluateCondition` (ProjectileHitUnit) gives
    four citizens attack goals in the loaded game only. The citizens' `ObstacleDetected` draws then go missing.
    Not yet traced to the state behind FindImpactUnit;
  - *seed 2, 2100 s*: at tick 17 a land sub group of player 3 moves its formation in the continued game and checks
    its task in the loaded one. The ASG lines are equal up to there, so the cause is in state the dump does not
    show;
  - the continent manager and the AI's other derived state are still rebuilt at load, not saved; the path searches
    in progress are asked again, not resumed.

## Merging D6 and R4.3: what only the game showed

Both branches passed every non-harness gate (link, oracle, Rust gate, the goldens); the game found two bugs.

- **A NULL texture-set slot (R4.3):** `GETerrainMesh::mRegisteredTextureSets` has NULL slots for unregistered ids.
  The new `UpdateVisibleTerrain` read every slot's width for the Rust plan and crashed at a map's first frame. The
  goldens and EE-SIM never draw terrain, so only a tour reaches it. A NULL slot is now an empty view, and the plan
  gives no texture for an empty set (it would otherwise divide by a zero width).
- **Characters where bytes were meant (D6):** `UWideString::GetLength` counts characters, `GetTCHARLength` bytes.
  Under Windows-1252 they were equal, so the DX7 text draw passed the first to `PlatFontMeasure`/`PlatFontDraw`,
  which take bytes. Under UTF-8 each multi-byte character cut the drawn text one byte short: the main menu's
  copyright line ("© 2002 ...") lost its final period. Found by a pixel diff of the menu captures against the
  build before D6. Anything handing a `UWideString` length to a byte API must use `GetTCHARLength`.
- **A duplicate cbindgen name:** D6 and the WON branch each added an `EeWonWideSink` with different signatures.
  Each branch's header was current, but the merged header declared it twice and nothing compiled. Rerun
  `rust_build.py header` and a compile after every merge that touches two crates' FFI.
- **Flaky tests under load:** `scaled_waits_time_out_early_and_still_see_their_events` checked "no factor" outside
  the test lock the factor tests hold (fixed). `ee_data gate::tests::rules_batch_writes_err_for_a_broken_document`
  fails rarely, only in a whole-library run (the broken document loads without error); not yet explained.

## R5b.1, the continent analysis, leaf by leaf: the continent grid, the tile values, the choke pass's grids, the sub sector link walks, the sector grid, the tile kind, the region fills' start tests, the path area repair's choice and the queries in Rust (ee_astar continent_grid.rs, tile_values.rs, choke.rs, sub_sector_links.rs, sectors.rs, tile_kind.rs, path_area_kind.rs, repair.rs, queries.rs; branch rust-r5b-cont, 2026-11-10)

Nine leaves of `EEAIContinentManager`'s analysis are Rust, each a plain-data computation the C++ manager stores
into its objects, as `ee_rmap` did with `EERandomMap`: the C++ classes keep every member; the flood fills (the
machines and their C++ goal objects) and the object bookkeeping (`Rebuild`, the application of `RepairPathArea`'s
choice, `PruneChokePoints`, `CreateScenarioAreas`, the area classes' hulls) stay C++.

- **`CreateContinentGrid` whole** (`continent_grid.rs`, with `AddTileValue` 0x424fea and `AddShoreTile` 0x41f3e0,
  both deleted from the C++): from the fill's ids and kinds, every tile's terrain and the continents' categories
  it computes the id grid, the adjacency matrix, the neighbour pushes in the C++'s order (x, then y; the tile at
  x - 1, then at y - 1; a map edge continent's tiles make none), the shore tiles (the water and space tiles next to
  land, then the land tiles beside those, with the shore tile they were reached from on the land continent's list),
  the ice shore tiles and every continent's shore list after `SortTiles(true)`. The lists the C++ sorted come out
  sorted by `U2DPoint<long>::operator<` (the lower y first, then the *larger* x) and unique where the C++ made them
  unique, so only the neighbour order had to be kept as a push sequence. The result is an opaque `EeContinentGrid`
  with slice accessors; the C++ copies and frees it.
- **The map has NULL tiles.** `RTSMap::GetTile` range-checks and then returns the tile pointer, which is NULL
  outside the playable rhombus of a diagonal map. The first build read `mTerrainType` through one and every
  capture game crashed at `StartGame`. Every terrain grid handed to Rust carries -1 (`NO_TILE`) for such a tile, and
  the Rust treats it as the C++ treated a NULL tile: no shore, no value, and in the choke pass an area of its own
  with serial -1 (`u32::MAX`), as `EEAIChokeGetTileSerial` returned.
- **`UpdateTileValues`** (`tile_values.rs`): the C++ reads every tile's terrain and, for the land pass, every tile's
  `GetResourceWeight` into grids (pure reads; the original read only the 3x3 blocks of land tiles), the Rust gives
  every tile's value (0: unchanged) and the AI value byte (clamped to +-120); the C++ stores the non-zero ones. The
  shore and ice shore searches are `std::binary_search` by the point order above (`contains_point`).
- **The choke pass's grids** (`choke.rs` `build_grids`, `tile_serial`): the classification
  (`EEAIChokeGetTileSerial`) decides from a per-tile record the C++ fills in the original's reading order (the
  continent's category and serial; on land or cliff the resource, and only then the forest `GetForestAt` lists and
  the manager's entry under its id), then the passes loop (`EEAIChokeGetKinds`, `EEAIChokeSmooth`): each pass
  averages the *base* classification again from the previous pass's result, as the C++ re-copied the base grid
  every round. The smoothed kinds are dropped as before; the pass's globals (kinds, serials, flags) are what Rust
  fills and the flood goal's view reads. The `ee_astar_choke_smooth` export went with it.
- **The sub sector link walks** (`sub_sector_links.rs`): `LinkSubSectorGrid` and `RelinkSubSectors` are the same
  walk over a rectangle (the whole map; a sector grown by one tile, clipped to the map, with the walk's own end in
  the `x + 1 < end` test), reading the id at a tile and whether the manager lists an id, and emitting the
  `LinkSubSectors` calls in order (a diagonal link through its two corner sub sectors only when both exist and
  differ from the pair). The objects' state never feeds the walk, only which sub sectors exist, so the C++ applies
  the list afterwards (`ApplySubSectorLinks`); `LinkSubSectors` itself, which reads the objects, stays C++.
  `RelinkSubSectors` also wrote each tile's id back into the grid it had just read it from (when the grid exists):
  dropped as a no-op.
- **The sector grid** (`sectors.rs`): the sector and region sizes by map class (the sector size stays 7 past the
  largest class, the C++'s initial value), `Build`'s number of choke passes, `EEAIPathSectorRegionInformation::Init`'s
  sectors per side (`UMCeil((float)size / sectorSize)`: `lrintf`, then up when short) and each sector's rectangle
  (`GetSectorEnd` folded in; the C++ clips the right edge by comparing the map's last *row* with the sector's last
  *column*, kept).
- **`EEAIGetTileKind` and `EEAIGetTileWallID`** (`tile_kind.rs`): the C++ hands over the tile's continent category,
  terrain, AI value and one record per unit of its list (wood, amount left, blocking, gate, wall, foundation, alive,
  dock / space dock, id). These are pure member reads, `IsFoundation` included, so reading every unit where the C++
  stopped at the first hit answers the same. `EEAIHasResourceUnit`'s test is a wood unit on a tile not of deep water.
- **`CreatePathAreaKind`'s start test and the fills' corner test** (`path_area_kind.rs`): per tile, the terrain, the
  place against the four diagonal edge lines, the area's kind and region ids and the unit records decide whether a
  fill starts. The C++'s `theFlag` carries from one tile to the next (only kinds 1..4 come here, so it never matters;
  kept). The corner test is the continent fill's (`kTerrainCliff`, 9) and the region fill's, which compares with the
  literal *3*: kept as the original has it.
- **`RepairPathArea`'s choice and `GetAdjacentAreaID`** (`repair.rs`): both read only the tile's eight neighbours
  (`GetPathAreaAt` and the map tile: pure), so the C++ snapshots them in its push order and Rust returns a plan (join
  the largest same-kind area and merge the others, in the C++'s order; join the one neighbour's area; or make a new
  area) that the C++ applies to its objects. The diagonal test's two steps (`EEGetTileDirection`, then
  `EETurnDirection` by one either way) always land on the two orthogonal neighbours beside the diagonal. That follows
  from the direction table and the angles (a diagonal's sector value is n + 0.5, far from the truncation bounds), so
  no trigonometry moved. Two quirks kept: `theOK` carries from one neighbour to the next, so a diagonal that fails its
  steps makes every later (lower-index) non-obstacle neighbour fail too; and the single neighbour is compared with the
  *tile's* continent, the matches with the *old area's*. Ties: the matches are sorted by id and walked from the highest
  down, and the first strictly largest is kept (`mTiles.Size()` is a `long`).
- **The queries** (`queries.rs`): `AreConnected`'s chain for the crossing flags (the cliff + space and water + space
  cases keep their swapped chain names), the adjacency row walks of `GetConnectedIDs`/`GetAdjacentIDs` and
  `GetLargestContinent`'s pick (`(char)inKind` against the unsigned category byte, as the C++ widens them).
- **A capture flake, not a divergence:** one `sim_capture.py check` of the oracle profile failed on one EVAT digest
  (seed 2), and the rerun of the same exe passed. The capture differed in 4 bytes, in two records of
  `EETechCaptureUpgrade`'s raw upgrade bytes: `mMediaCount` and `mButtonLoadCount` (+0x154, +0x158), 22/21 against
  21/20. They count media loads on the presentation side, so their value at a tech effect depends on timing; the sim
  oracle and every other digest were equal. Leaving the two fields out of the EVAT record would remove the flake. A second one-off: one calamity-profile game
  (seed 3) faulted three seconds in, at the main menu, in `NERecordPlayback::Process` (offset 0xf, from
  `EEUIESPERandomMap::ProcessEvent` through `NESynchronousServer::ProcessLocalUserInput`). That was before any game
  or continent code ran. The same exe's rerun and the merged build passed all six seeds.
- **Not in this branch's scope, found while reading:** the continent manager's area objects hold no unit pointers
  (`UpdateWallUnit` and `RemoveForestTile` take a unit and store nothing of it), so the handles branch and this one did
  not meet in `EEAIContinent*`.
- **Gates run:** three full builds (`build.py -j 3 --sim` 0/0, EE-AOC and EE-SIM): the five earlier leaves, then with
  the next three, then everything after the merge of `rust` (with the queries leaf). On each, `sim_capture.py check`
  passed in the four profiles: CNTB/CNTR (40-41 rebuilds a seed) and every other digest equal to the goldens, with
  225/240/229/241 digests in the oracle, late, space and calamity profiles. The sim oracle was identical on seeds 2-7
  (2,691-2,825 trace lines a seed). `rust_build.py gate` PASS (ee_astar: 92 tests, 17 new in this part); the
  windowed-software test profile 0/0. Each leaf was committed with `cargo test`, clippy, rustfmt, the header and the
  touched unit compiled alone. Not run: `gates.sh`, `eedata check`/`cppgate` (no data change), any macOS or Linux
  build.
- **Left in the analysis:** the object bookkeeping of `Rebuild` and `RepairPathArea` (the areas' tile lists, the
  sectors and sub sectors), `PruneChokePoints` (it needs the areas' closest points), `CreateScenarioAreas`,
  `GetTilesNextTo` (its ring order is `EEGetSquareRingTiles`'), `CanGetTo`, and the area classes' geometry
  (`EEAIArea::Create`, the hulls, `GetClosestPoints`, `GetShoreInRegions`). Those sit on `U2DSparseArray` and
  `U2DPolygon<long>`, which would have to move first.

## The gates' intermittent failures, each to its cause (branch rust-flakes, 2026-11-10)

- **EE-SIM's start-up crash was a local server that was never published.** The fault (an access violation in
  `NERecordPlayback::Process`+0xf, from `EEUIESPERandomMap::ProcessEvent` through
  `NESynchronousServer::ProcessLocalUserInput`, seen once in 6-24 oracle and capture games) is the random-map screen's
  first input to an `EEServer` whose `NEProtocol::Publish` failed: the server then keeps `mRecordPlayback` NULL and
  starts no threads, and the original never checks. Publish fails when its datagram port (33334 + offset) or the shared
  listen port (33335 + offset) is taken. EE-SIM chose the offset at start (a bind test from a pair the process id picks)
  and the server binds it seconds later, at the random-map click: another sim's or bench's pair, or any connection's
  ephemeral port (offsets past 15818 lay in Windows' dynamic range 49152-65535, about one game in eight) could take it
  in between. A port held by another process reproduces the fault every time (`tools/sim.py resolve` on the FAULT
  line). Now EE-SIM sets `gPlatSimPortOffset = kPlatSimAnyPort` (-1) unless `--port-offset` is given, and ee_net_tcp
  then binds port 0 everywhere: the system picks a free port at the bind, so nothing can take it first. Note that
  ee_net_tcp's `create_socket` restarts Winsock (`deactivate`/`activate`, as the C++ did) before each socket: with the
  plug-in the only Winsock user, the count reaches 0 and Winsock resets every open socket, so a single-player server
  has no game stream socket after its publish (netstat shows only the datagram port and the listen port). Harmless in
  single player; worth knowing before a LAN game is debugged.
- **A process-wide list read by a test that did not add to it.** ee_data's rules overrides (EE-SIM `--set`) are a
  global list applied to every document a `Content` reads. `overrides_apply_and_report` adds
  `combat.damage.minimum=8`; a gate test running at the same time loaded its broken `minimum: x` with the override in
  place, so it compiled, `bad.out` was written instead of `bad.err`. Under `cfg(test)` the list is now per thread.
- **WARP faults when a test process drives several devices at once.** The ee_gx access violation is in WARP's own
  worker thread (d3d10warp.dll, reading freed heap memory) while the six device tests create, use and drop their own
  wgpu devices in parallel: 3 of 45 runs of `gpu::tests::` faulted under load (all at the same instruction), 0 of 45 with one test thread. The game
  has one device per process; the tests now take one device at a time (a lock held across the Gx's life).
- **The same pattern elsewhere** (a test reading or breaking a global that other tests change):
  `switches_start_off_and_have_c_layouts` read `gPlatSimMode` outside the test lock while a message test set it;
  ee_won's `start_and_stop_count_users` stopped once more than it started, which ran `WSACleanup` (closing other
  tests' sockets) whenever exactly one other test had started; ee_plat_sys's double close of a socket descriptor could,
  on POSIX, close a file test's descriptor that had taken the number in between.
- **Capture records carry simulation state only.** EVAT's raw upgrade bytes included `mMediaCount` and
  `mButtonLoadCount` (presentation-side load counts that depend on when the loader runs); they are written as 0 now and
  the EVAT digests were rebaselined (no other record's changed). The disk was full while the larger profiles ran
  (a late or space capture is 1.6 GB a profile), so their captures went through a named pipe
  (`EE_SIM_CAPTURE=\\.\pipe\<name>`; the game's `fopen(..., "wb")` opens a pipe) into a reader that digests the
  stream: no capture file. That slows the physics thread to the reader's speed, and it showed one more record that
  depends on timing: TCST (a node's or an epoch's `GetCost`) had 1,171,743 records against the golden's 1,174,010 in
  oracle seed 2, every other digest equal. Something outside the simulation's own order calls `GetCost` (the
  UI thread's status buttons, `EEUIFMGStatusButton.cpp`, are the suspect); a heavily loaded `check` could fail on it the way EVAT did.

## R5b.4: the map, the game messages' codec and unit kinds as data (ee_netmsg, ee_units kinds.rs; branch rust-r5b4-core, 2026-11-10)

The map (counts of the `dynamic_cast` sites by tested class and module, the message types and their codecs, the order
of the leaves) is in RUST_PLAN R5b item 4.

**The game messages' codec.** `rust/crates/ee_netmsg` owns the wire layout of the 104 game message classes of
`EEMessage.cpp` (107 types): `classes.rs` lists every class's members in wire order after the 4-byte header (written
once from the original `ExportMessage`/`ImportMessage` bodies by a scratch extractor, which also checked every fixed
class's sum against its `ExportSize` constant), `codec.rs` encodes values and decodes spans, `units.rs` is
`CompressUnits`/`DecompressUnits`. The C++ classes keep their members, constructors and `Execute`; each class has a
file-static `Slots()` that lists its members (address and `sizeof`) in wire order, and its three codec functions hand
them to `ee_netmsg_export_size`/`_export`/`_import`. A member whose `sizeof` differs from the layout stops the game
with its name (no silent misbinding); members of the same size in the wrong order are what the capture catches.
Strings stay `UWideString::Import`'s: the Rust reports where a string starts (and reads its u32 to find the next
member, as the original's `theLocation += Import()`); lists and the box selection's buffer likewise.

- **A single-player game never encodes a message.** The local server hands the sender's objects to the physics; the
  factory (`EEMessage::BuildEEMessage`) runs only in LAN games and for loaded replays. So the capture has three
  records: NMEX at `EEPhysics::DrainInput` (every executed game message: its export and its members, then the message
  built again from its bytes, NMIM), NMSW (a sweep of every type over edge payloads, once per game: all 0x00, 0xff,
  0x7f, 0x80, counting and random bytes; strings of 0..300 bytes, 0..63 ids, 0..40 upgrades) and NMCU (the packing of
  unit lists for five owners, single ids at the edges of the 8/16-bit groups, over 63 ids, many groups). The oracle
  games execute only three game messages each (the same three in all six seeds); the sweep is the codec's real
  coverage (1,644 messages, 130 packings).
- **Quirks kept:** `EEMChangeFormation`'s import reads two of its three members (the third stays the constructor's);
  `EEMCivUpgrades` writes its count as `(unsigned char)size()` and then every upgrade; `DecompressUnits` reads until
  its index equals the size exactly (on a buffer `CompressUnits` did not make the Rust stops at the 256 bytes where
  the original read out of bounds: the one declared difference, unreachable from the game's own buffers); the ids'
  top byte is the owner's, and on LP64 the C++ `long` is the 32-bit value sign-extended, as the original's
  `inBuffer[0] << 24` was.
- **Not moved:** the 29 engine messages (`NEMessage.cpp`, with `NEMPlayerInput`'s nested inputs) and every `Execute`.

**Unit kinds as data.** `ee_units/src/kinds.rs` holds two tables Rust owns and C++ reads inline (`ee/EEUnitKind.h`):
the goal classes of each `EEUnitGoal::mKind` (a heal goal is also a location goal, the load goals also load
containers) and the unit classes of each `EEUnitClass::mCategory` (with their `EEUC*` bases). Why a kind is the class:
every goal constructor passes its kind (`EEUGHealUnit` overwrites its base's), copies copy it, nothing else writes it,
and `EEUGLoadContainer` is abstract; `CreateUnitClasses` picks the class from the category alone, `mCategory` is
written only by the constructor, and no other code creates a unit class. `EEGoalCast<T>(g)` and
`EEUnitClassCast<T>(c)` are `dynamic_cast<T*>` for those classes, NULL included.

- **Converted so far:** the 118 goal casts of Units, Physics Thread, Communications and Player and the 34 unit class
  casts of Units, Physics Thread and Communications (a scratch regex over the code, comments
  left alone; every converted file compiled and the oracle stayed identical). Not converted: the object casts
  (`dynamic_cast<EEComplexUnit*>`, 81): an object's class is not a function of its class's data (`EEUnit::Init`
  sets `mComplexUnit`, `ResetEEUnit` clears it, the load path builds orphans of either kind), so they need a kind
  byte fixed by the constructors (and reset by the destructors, as the vptr is) first. The AI's casts are held.
- **The capture's sizes.** A whole oracle capture is about 900 MB (six seeds, mostly CB** and TCST); with the disk
  shared by several agents it filled the volume twice. The gate ran one seed at a time (`sim_capture.py run --seeds N`,
  `digest`) and deleted each capture after its digest. A write that failed on the full disk left a source file empty: check `git diff
  --stat` after a disk-full error.
- **EVAT seed 2 is flaky** in the C++ alone: one capture of the unchanged build gave `f8db2871dac2a51c`, the next
  the golden `6388c675769c7b58` (the tech capture's EVAT record; not looked into).

## R6: the form's bookkeeping, hot keys, fonts, music, IME and backgrounds in Rust (ee_ui, branch rust-r6-ui5, 2026-11-10)

Moved to `rust/crates/ee_ui` (the C++ classes keep every member, vtable and exported name; four headers gained a
`friend struct`): `UIForm`'s `AddCopyDirtyRectangle`, `AddDirtyRectangle` (after its popup test), `CopyData`, `Open`,
`Close`, `SetPopup`, `RedrawIntersectControls`, `SetAllControlsDirty`, `GetToolTipShadowRectangle` (`form_state.rs`);
`UIHotKey::CalculateKeyCount`, `GetHotKeyName`, `FireEvents`, `UIHotKeySorter` and `UIForm::UpdateKeyboard`
(`hotkey.rs`); `UIFontTable::Create`'s record reading, `CreateClosestFont`'s size and `FindOrCreateFont`'s search
(`font.rs`); `UIManager::SetMusicVolume`, `StartMusic`, `StopMusic`, `UpdateUI`'s music step, the sound-instance
members, the IME composition calls, the candidate window's change/open/close and `CopyData`'s part
(`manager_media.rs`); the backgrounds' `DrawBackground` of every kind (bitmap, tiled, left-right, top-bottom,
stretched, full screen), `DrawTiledBackground`, `UIBackground::Open`/`RelayoutBackground`'s screen rectangle,
`SetRectangle`, `SetVisibility` and `UIForm::DrawBackground`'s tiling (`background.rs`).

- **Nothing is cached across a host call.** A control's `SetDisplayMode`/`SetDirty` comes back into the form through
  the manager's `AddDirtyRectangle`; the golden driver's fake manager does exactly that, so `CopyData`'s dirty queue
  is re-entered mid-pass. The rectangle lists are reached a slot at a time; the controls, tables, events, hot keys and
  sound instances are opaque handles.
- **Virtual calls stay virtual.** `DrawTiledBackground` and `DrawStrectchedBackground` are virtual, so a tiling
  class's `DrawBackground` asks Rust for a plan (`tiled_plan`) and calls its own `DrawTiledBackground`, which forwards
  to the Rust tile loop and draws each tile through the virtual stretch. `FireEvents`, `GetIMEEnabled`, the forms' IME
  members and `UpdateKeyboard` are called the same way.
- **What stays C++:** the per-group hot-key lists and their `std::sort` (MSVC's is an insertion sort, stable, up to
  32 elements: the replay uses a stable sort), the removals, `CreateHotKeys`; the shared `GEFont` list; the sound
  manager; the lowercasing (`PlatMbsLwr`, a host call); `UIFullBackground::LoadBackground`'s file names;
  `UIBackgroundTable`; every draw (`ComputeStretchedCoordsAndScale`, the texture coordinates, `Draw`).
- **Kept quirks:** `Open` calls `SetTabbing` once per database record; `UpdateKeyboard` compares a group with the
  hot-key list's size as it is after the previous group's events; `StartMusic` keeps the lower-cased name but loads
  the name as given; `OpenIMECandidateWindow` asks the window's client origin twice; `CreateClosestFont` is
  `height * 72 / 96 + 1` in 64 bits. The unsigned tile loops of the C++ never end when a last row or column is
  `ULONG_MAX`; the Rust stops there (unreachable from real rectangles).
- **Golden drivers, new traps.**
  - A `calloc`ed object has no vptr: call the unit's virtuals qualified (`f->UIForm::CopyData()`), or give it its
    class's vtable through an asm label, `extern "C" const char k[] __asm__("??_7UIManager@@6B@")` (`UpdateUI` calls
    the virtual `UpdateKeyboard`).
  - Members that look header-inline are not always: `GERhombus()` (GERhombus.cpp), `UILayoutUtility()`
    (UITrampolines.cpp), `GERectangle::And` (GERectangle.cpp, linked as its object), `GERectangle::RoundToNearest`
    (GEBitmap.cpp); under `/FORCE:UNRESOLVED` each is a call to address 0. Define them in the driver or link the
    object.
  - Abstract engine classes (the rasterizer, a surface) are faked as tables of stubs: every rasterizer slot the
    traced `Draw(GEBitmap*)`, the surface's slot `N` returning `16 * (N + 1)`, and the driver prints the sizes it
    reads, so the replay takes them from the golden.
  - The Bash tool turned `"\n"` inside a quoted heredoc and a `sed` replacement into a real line break twice: write
    driver edits with the editor or a Python file.
- **Goldens:** `golden_form_state.txt` (24 forms x 30 operations, the form's whole state after each),
  `golden_hotkey.txt` (60 names, 300 `FireEvents`, 75 forms of `AddHotKey`s and `UpdateKeyboard`s),
  `golden_font.txt` (40 tables), `golden_media.txt` (30 managers x 30 operations), `golden_background.txt`
  (200 backgrounds x 3 redraws, relayout, move, visibility). Each Rust test replays the whole file; each driver over
  the forwarders prints the same file and equals the original on 3 or 4 more seeds at 10x the size.
- **Gates run:** after each of the four commits `build.py -j 4 --sim` 0/0 (EE-AOC, EE-SIM), the sim oracle (seeds
  2-7, the main checkout's open tree) identical to `ref`, `rust_build.py gate` PASS (ee_ui: 95 tests); the changed
  units compiled alone before each full build. No tour, screenshot, LAN game or POSIX build was run.
- **Only a tour exercises these** (EE-SIM draws no UI): every commit. `form_state` runs on every UI frame and at
  every form's open and close; the hot keys on every keyboard update of a form (the game's shortcuts, Tab between
  controls); the font table when the UI database loads and every scaled control; the music at every menu change and
  the volume slider; the IME only with an IME; the backgrounds on every redraw of a menu or panel.

## R5b.4, second round: the engine messages and the players' inputs in the codec, the message kind, the object kind (ee_netmsg engine.rs, ee_units kinds.rs; branch rust-r5b4-core, 2026-11-10)

**The engine messages.** `rust/crates/ee_netmsg/src/engine.rs` holds the wire layout of `NEMessage.cpp`'s 28
message classes (types 0x20..0x3c; 0x2c has none), with class ids after the game classes'. `NEMessage.cpp`'s
`ExportSize`/`ExportMessage`/`ImportMessage` hand their members as slots, as the game messages do; the fixed classes'
`ExportSize` is `ee_netmsg_fixed_size` (the old constants are the Rust table's sums, checked by a test), and
`NEMessage::GetHeaderSlots` replaced `EEMessage`'s copy. Two new member kinds: `Raw(n)` (`NEMXferMessage`'s whole
1 KB block) and `Inputs`, the players' inputs at the end of the two communications ticks (a u8 count, then each
nested message, game or engine, in its own layout).

- **The nested inputs' rules are Rust's:** only `(unsigned char)mPlayerInputs.size()` inputs travel (the count byte
  and the loop both use it), and the import's walk (`walk_inputs`) builds each input where the last one ended and,
  for a type no factory builds (`ConvertMemoryToMessage` returns NULL), stays where it is, so every later input is
  tried at the same place and skipped too (the original's loop, kept). The C++ builds each input through a callback
  (`ee_netmsg_inputs_import`; it returns the input's `ExportSize`, 0 for none); the inputs' own bytes are exported
  by their own classes and handed over as an array of slots.
- **The gate: NMNE**, a new capture record (ee/EEMessageCapture.h): every engine type over the sweep's edge payloads
  (fixed bytes in nine patterns, strings of 0..300 bytes), and the ticks over seven input lists: none, one game
  message, a chat with an engine message and a wall, an unknown type first (0x05), a known one then an unknown one
  (0xfe), a tick inside a tick, 255 inputs. 549 records per game; the nested game inputs are recorded inside the
  NMNE record (their own NMIM records are off during the engine sweep, so the older digests did not change). The
  goldens came from the unchanged C++ engine codec (24 games, one digest), recorded as pure insertions; the Rust
  codec gives them in all four profiles.
- **The NM** goldens the first round left out** (late and space seeds 4-7, calamity 5-7): recorded from the tree
  before the codec moved (27f8ffc2^, a scratch worktree with its own staging root), as pure insertions; the Rust
  codec's captures equal them.

**The message kind.** `ee_netmsg_message_kind(type)`: engine (the engine factory's types), game (0x41..0xfd, the
types the game registers its factory for, which every `EEMessage` class takes: `EEMEndCinematic`'s 0x9d is one the
factory does not build), none. A message object is an `EEMessage` exactly when its type is a game type (each
constructor passes its type; the import constructors take the type the factory dispatched on), so
`EEGameMessageCast` (ee/EEMessage.h) replaces the four `dynamic_cast<EEMessage*>` (`EEPhysics::DrainInput` twice,
`EEServer::PrepareLocalInput`, `EEClient`'s local input stamp).

**The object kind.** `RTSUnit::mObjectKind` (a byte in a non-copyable holder, so no implicit copy or assignment of a
unit can carry another object's class: the build proves there is none) is set first thing by each constructor
(`RTSUnit` 0, `EEUnit` 1, `EEComplexUnit` 2) and reset on entry by each destructor (`~EEUnit` 1, `~RTSUnit` 0), as
the vptr is; pooled units keep theirs, as their vptr. `EEUnitCast<EEComplexUnit>(u)` (ee/EEUnitKind.h) reads
`EE_UNIT_OBJECT_KINDS` (kinds.rs) and equals `dynamic_cast<EEComplexUnit*>(u)` from any `RTSUnit` (every `RTSUnit`
is an `EEUnit`: no other class derives from it).

- **Converted:** 28 casts in Units, Communications, Physics Thread, Calamities, Player and Scenario Manager.
  **Left** (54 of the 82 sites, one of them in a header): the 17 of the load path (`EEComplexDoppelganger.cpp` 10, the `RepairPointers` of `EEUGAttackUnit`,
  `EEUGCastCalamity`, `EEUGGather`, `EEUGPatrol` and `EEAirTrafficControl` 3: the save work's files), the AI's 31,
  the graphics thread's 3, `EEPWorldHelper`, the UI's and `EESim.cpp`'s one each, and the 8 `dynamic_cast<EEUnit*>`.

## R5a.2, third round: region order, AI timers, favourites, production, towns, gatherers, dead attackers (save 0.257-0.262, branch rust-r5a-aisave2, 2026-11-10)

Continues "R5a.2, second round". Same test (`sim_oracle.py saveload NAME --seeds 2-7 --at 600,1350,2100`, 18 saves).
Each save version below keeps the older ones' load path (an older save loads without the new block).

- **What changed, in commit order** (each measured on the 18 saves; the no-save oracle identical to ref on seeds 2-7
  after every one, sim_capture's four profiles PASS on the tree through 0.261):
  - *0.257, the units' order in each map region* (EEMap::mUnitRegions): a load added them in its own order, and
    FindImpactUnit and the target searches take the first match. Put back at the first tick when the region holds
    the same units. No change at CT granularity alone.
  - *0.258, the AI experiments' timers*: ten per-player function statics (patch settlements, guard towers, militia,
    naval flee, boatrush2's hold, the epoch schedule, the army threat check) were 0 in a loaded game's new process,
    so a loaded player queued a settlement at its first update (the "drop site" of the second round). Now one struct
    (ee/EECPExperimentTimers.h), saved by the world helper. Seed 3 600 s 0 -> 94 CT lines, 3 2100 s 0 -> 2, 5 2100 s
    12 -> 23. **Lesson: grep the AI for function statics** (`static (unsigned )?long s...[64]`): the port's own
    experiments keep state there, outside every saved object.
  - *favourite unit types* (no format change): the load enters each epoch again (EnterEpoch), which picks new
    favourites at random after EEComputerPlayer::Load read the saved ones, so the loaded game costed another army
    (class 207 for 24). Put back at the first tick.
  - *0.259, mNumInProduction*: never saved; also counted down by the load's AddAllUnits, so it goes back at the first
    tick. With the favourites: seed 2 2100 s 0 -> 1, 5 1350 s 11 -> 57, 7 1350 s 22 -> 57.
  - *unfinished buildings out of the rebuilt towns* (no format change): a building placed as a foundation joins its
    town at UnitCompleted, but RepairPointers added every unit; a town took a building under construction, grew a
    hull and made two patrol points the saved game did not have.
  - *0.260, the towns' ids, order and counters*: the rebuild numbered the towns on from the saved counter (6, 7 for
    the saved 2, 4), while the saved AI names towns by id; the counters differ because the running game counts a farm
    at its placement and again at its completion. A rebuilt town with a saved hull takes the saved id, counters and
    order (EEApplySavedTowns, in RepairPointers before the patrol groups are adopted).
  - *0.261, the gatherer lists*: Restore filed each citizen with a gather goal in its resource's list and the unit
    manager's AddAllUnits put every citizen in the idle list too, so a loaded player counted about twice its gatherers
    (45 for 23) when it split them by demand. The seven lists are saved by id and made the saved ones at the first
    tick. With the two town commits: seed 2 600 s 4 -> 58, 4 600 s 0 -> 6, 5 600 s 0 -> 23, 5 1350 s 57 -> 91, 6 600 s
    20 -> 59, 6 1350 s 11 -> 16, 7 1350 s 57 -> 178; the AI part no longer differs at the first line of seeds 4 and
    6 at 2100 s.
  - *0.262, the dead attackers*: mLastAttacker holds a reference, so the running game keeps a dead attacker out of the
    world (its id stays registered for the handles until the last reference) and reads its class and tile. Each one is
    saved once and made again at load as a registered, invalid unit out of the world (EEFindLoadedGhost, for
    EEComplexUnit::RepairPointers). Seed 4 2100 s: the units' references now match tick by tick for 8 ticks; no
    change at CT granularity.
  - *the managers restored after every player* (no format change): CreatePlayers restored each computer player's
    managers right after its own RepairPointers, before later players' towns existed. Now after the last player
    (gDeferComputerPlayerRestore) for saves from 0.260 on. No change at CT granularity alone.
- **The numbers** (first divergent CT line after the load, 600/1350/2100 s; base = the start of this round):
  seed 2 4/2/0 -> 58/2/1, seed 3 0/0/0 -> 94/0/2, seed 4 0/1/0 -> 6/1/0, seed 5 0/11/12 -> 23/91/23, seed 6 20/11/0
  -> 59/16/0, seed 7 3/22/1 -> 3/178/1. Still **0 of 18** saves continue identically to the end.
- **Tools** (TEST ONLY, committed): the state dumps' REGION, TOWN and PATROL lines, and the ASG line's task, next
  waypoint and task area (`area=kind/serial/exists`). The scratch scripts under the worktree's build/hunt
  (`pair.py`: a continued and a loaded game of one save with a dump after each of the first N ticks and the AI
  trace; `dd.py`: the first differing dump, the unit raw bytes named by layout.json with pointer-like values left
  out; `split.py`: the working tree written as one commit per cause) are not committed. Two runs with the same
  oracle NAME (saveload and run) share EE-SIM stage slots and break each other (`engine no Data/ai/ais.json`).
- **Still different after a load** (the next causes, measured):
  - *a task's town location* (seed 3 1350 s, seed 7 600 s): an EECPILocation made from a town area keeps mPlayer 0,
    so Update finds no town (it looks in player 0's towns) and drops the area. The running game keeps the area until
    its next group refresh; the loaded game's restore, and the group refresh the load runs (UpdateGroups through
    RefreshResourceCenters / the regroup), drop it at once, and the attack sub group then checks its task and draws
    where the saved game does not. A Restore that searched every player's towns was tried and did not help, because
    the refresh during the load calls Update again: the fix is either to save the location's town owner or to keep
    the load from refreshing the groups before the first tick;
  - *seed 2 2100 s, tick 17*: player 2's land sub group goes idle (state 2003 -> 0, a new task) in the loaded game,
    likely the same task check;
  - *seed 4 2100 s, tick 9*: player 3's iron gatherer is sent to another mine; the AI trace's trip distances differ
    for it at that update while the dumps are equal, so the state is not in the dumps yet;
  - dead units held by handles or other pointers than mLastAttacker (mLastResource, the AI's target handles) are not
    made again; the continent manager is rebuilt at load, the path searches are asked again.

## R6: WON's file reader and writer, the config parser, AuthContext's data and the directory replies in Rust (ee_won file.rs, config.rs, authctx.rs, dir.rs; branch rust-r6-won4, 2026-11-10)

The map of what is left of WON (sizes, reach, dependencies) is in RUST_PLAN's "R6 WON, the rest" row. This round
took the leaves that everything else stands on (FileReader, ConfigParser) and two pieces that only the gone
servers reach (AuthContext's login data, the directory replies), each captured over the C++ first.

- **FileReader / FileWriter (`file.rs`).** The state is a `#[repr(C)]` core the classes embed (`mCore`: the
  `FILE*`, the memory buffer, length, position, owned and memory-mode flags; the writer's `FILE*`). Every read,
  seek and write is Rust, on the C runtime's stdio exactly as before (`PlatFOpen`'s `FILE*`, `fread`, `fgetc`,
  `fseek`, `ftell`, `feof`, `rewind`; an owned buffer is `malloc`/`free`), so a text-mode open still turns CR LF
  into LF on every system and the quirks stay: `length()` sets the memory-mode flag, `ReadIntoMemory` at the end
  of the data throws after the buffer is open (at 0), a file seek past the end succeeds and the next read fails.
  The C++ throws `FileReaderException` / `FileWriterException` with the text a status names, and keeps the
  string loops (a failed read leaves the part read) and the writer's read-only retry (`WONFile`). ConfigParser,
  ImageDecoder and WONSound read the core instead of the old members. `won_file_test` (252 steps: writes, reads in
  file, text and memory modes, every exception) recorded the golden over the C++; the Rust test `file_golden`
  reads the script out of the test's source and replays it on real files in a temporary directory; the driver
  relinked over the forwarders prints the same.
- **ConfigParser (`config.rs`).** The characters (pushback first, 16-bit units in a UTF-16 file, CR LF and a lone CR
  as LF, the parser's line and the process-wide line count), the comments, `ReadToken` (narrow and wide, a key's
  value to the end of its line with `\` joins, skipping, `$name$` expansion through the root parser's scopes),
  the define scopes and the group defines with their saved states, and the open's UTF-16 sniff (FF FE ?? 00: reopen
  binary past two bytes) are Rust. The C++ keeps the reader, `mLineNum` and `mIsUnicode` (handed in per call as an
  `EeWonConfigIo`), the parent walk (it passes the root's lexer), the file names and the error and warning texts
  with their formatting. The define names compare through the CRT's `toupper` of each signed `char`, and the
  white space is the CRT's `isspace` (`ffi_parse::CrtCtype`): the English locale makes 0xA0 a space, which the
  golden's two passes show.
  - **Behaviour changes:** the pushback is 16-bit units on every system (as `std::wstring` on Windows; on POSIX a
    sign-extended byte now reads back as 0xffe9 like on Windows); `mIsUnicode` starts false (the original left it
    uninitialised); a token cut by an error keeps the part read (as before) but is assigned once at the end.
  - **The golden needed sequenced lines.** The first capture printed garbage for a group define's state: in
    `Line("x " + (Get(...) ? "1" : "0") + Int(state.pos))` clang evaluated `Int(state.pos)` before the call (the
    operands of a chain of overloaded `+` are not sequenced, also in C++17). Every value is computed into a local
    first now. `won_config_test` has 31 inputs, each as a plain and a UTF-16 file, six passes each (tokens, a token
    then its line, wide, wide to the line, skipped, characters with pushback) plus scenarios (scopes, parents,
    group defines across files, errors, reset): 4060 lines in the C and English locales, recorded over the C++,
    the same over the port; `config_golden` replays the C pass (2028 lines) through the Rust with the C++ glue
    modelled. `won_config_test --files DIR` prints each .cfg file's token stream CRC: the 63 lobby files of the open
    tree give the same over the C++ and the port.
- **AuthContext's data (`authctx.rs`).** The login secrets are a Rust list the context owns (`mSecrets`; the
  `std::list<SecretData>` is gone): the `_wonlogin.ks` file read with the original's stdio loop semantics (an EOF
  count byte is 255, one more zero record after a file that ends exactly at a record, a cut record keeps the bytes
  read; the original left the rest of a cut key uninitialised, zeros now), saved with the count's low byte and
  every record, added in front, removed (the first uncommitted match), committed (`unsigned long` sum + 120, the
  expired dropped, the list cut below 256). The login request's community data block (types 1, 7, 8, 6, 4, 3,
  the 16-bit lengths as the original's `short`s, the community count's low byte) and its hashes block (MD5 of the
  challenge and the section hashes) are built in Rust from the maps' entries in their order; `StartAutoRefresh`'s
  wait is Rust. The lock, maps, sessions, certificate ops and timers stay C++. `won_auth_test` runs the original's
  statements (copied before the port, over the same lists, maps, WriteBuffer and MD5Digest) next to the Rust ABI:
  237 checks, the same (files after every add, remove and commit, odd files, 306 communities, a wrapped count).
  Reached only by the internet login (gone) and, through `GetCertOp`, nothing else: peer auth reads the
  certificate, not these.
- **The directory replies (`dir.rs`).** `ParseMultiEntityReply` / `ParseSingleEntityReply` read in Rust; the C++
  makes the `DirEntity` objects from the fields read (a bit per field, the rest keep the constructor's values),
  converts the ACL values as the originals' locals did and throws `ReadBufferException` for a cut reply, keeping
  the entities read whole before the cut. `won_dir_test`: 548 replies (each get flag alone and together, services,
  directories and untyped entities, wrong headers, statuses, every cut), recorded over the C++, the same over the
  port. Reached by nothing: the directory servers are gone.
- **LobbyLanSocket** stays as it is: a `BlockingSocket` subclass (vtable, `Duplicate`) with a client link and a
  last-message time, whose stale rule is `lan.rs` already; nothing of its logic was left to move.
- **The header is the compiler's while a build runs:** `cargo run -p xtask -- cbindgen` failed with "user-mapped
  section open" during a full build (the compiler had `include/rust/ee_rust.h` mapped); the header was intact. Do
  not regenerate it, or edit sources (staged per compile job), while a build of the same worktree runs.
- **What only the lobby exercises:** FileReader / FileWriter and ConfigParser are read by every lobby load (the
  lobby's .cfg layout, its persistent data, its images and sounds): the LAN game (`lan_game.py`) or a lobby tour
  is their end-to-end check. The goldens cover them in isolation only; nothing of this round touches the
  simulation (the oracle is identical after each commit).
- Still C++ of WON: peer auth, AuthContext's sessions and certificate flow, GetCertOp, the other services' requests
  and replies (directory requests, routing, DB, HTTP, FTP, misc, the server connection), ConfigObject /
  ConfigValue (the object model on virtual hooks), WONFile, RegKey, the lobby logic and WONGUI.

## R5b.4, third round: the messages' Execute decisions, the selection id check and the resources (ee_netmsg execute.rs; branch rust-r5b4-core, 2026-11-10)

Leaf 4's (a) and (b) (RUST_PLAN R5b.4, "Leaf 4"): the decisions of the `Execute` bodies over plain values are
`rust/crates/ee_netmsg/src/execute.rs`; the C++ reads the values, asks (ee/EEMessageExecute.h), and applies the
answer, keeping the objects, the locks and every world call.

- **The selection id check** (`EEMessage::ExecuteSelectionMessage`, `ExecuteMessage`): whether a client marks the
  local selection as executing (`gWorld.mExecutingLocalSelection`), whether the local sender's selected group copies
  the active group after `Execute` (asked after it, as the original compared after it), whether a command runs
  (its id is the sender's last; Game Restarting 0x91 always; Migration Slot Info 0x7c on a client), and the
  selection toggles' passes (twice while a selection is replayed: `EEMAddUnitToSelectedGroup`,
  `EEMAddAndRemoveUnitsFromSelectedGroup`). The two entry points now take the server's or the client's lock, ids
  and last ids once, and run one body instead of two copies.
- **The resources:** `EEMAddRemoveResources`' take test (a positive amount up to the have, compared unsigned on the
  have's low 32 bits, as on Windows), the market price (`EEGetTradePrice`'s curve: EEWheel.cpp keeps the world
  table's rates and the trade balance, the formula and its two words moved), `EEMExchangeResources`' step (sell 100
  or all that is left at the price; buy 100 or what the gold pays for, nothing when that rounds to 0), `EEMPayTribute`'s
  two orders (tributes food, gold, iron, stone, wood; the chat line food, wood, stone, gold, iron) and whose line it
  is, `EEMDisband`'s refund (hit points over the maximum times 0.75, f32 in that order).
- **The gate: NMDC**, a new capture record (ee/EEMessageCapture.h): every decision call in game (its kind, inputs
  and outputs as 64-bit values, a float's bits), the decisions' tables over edge values once per game (the command
  check over all 256 types, both roles and edge ids; the take test over amounts and haves past 32 bits; the price
  over rates 0..1000 and negative and trade balances of +-1,000,000; the exchange steps over prices and haves; every
  tribute subset; the refund over edge hit points, a maximum of 0), and every five game minutes the real players'
  prices for every resource both ways through `EEGetTradePrice`, the exchange steps and take tests from what they
  have, and the refund of every unit (about 14,400 records per oracle game). Its goldens came from the C++ decisions
  (the bodies factored into C++ functions of the same names and signatures as the Rust ones, the oracle identical),
  recorded as pure insertions into the four profiles' golden files; the Rust decisions give them.
- **What the games do not reach.** Only a LAN game runs the client side of the check (the client's lock and ids,
  `mExecutingLocalSelection`, the 0x7c exception): a single-player game is the server. The selection toggles'
  second pass and the copy into the selected group run when the local human selects (`EEServer::PrepareLocalInput`
  executes the selection at once with `mIsExecutingSelection` set, and the physics again later): a played game or a
  tour reaches them, EE-SIM's idle human never. The captures cover these only through the tables.
- **Leaf 4 (c), mapped, not moved.** 15 bodies split the selection by category and give goals (`EEActiveGroup::Fill`
  for categories 1..5, then `SpreadAround*`, one `gPhysics->GetRandom` draw for the acknowledging unit, `new EEUG*`,
  `Plan`, `EESendGroupCommand`): AddPatrolPoint, AttackGround, AttackUnit, BuildUnit, BuildWall, CastCalamity,
  DropoffResource, Explore, GatherResource, HealUnit, PathTo, Patrol, RepairUnit, SmartFarmers, UnloadTransport; 18
  construct goals, 24 look units up by id. Their decisions are reads of the units' plain state (category, hit
  points, built, type, tile, goal kind) interleaved with world calls that change it (a unit removed from a group by
  `Fill`, a goal replaced by `ClearGoal`/`SetGoal`), so moving them first needs the unit store (leaf 5): each unit's
  plain state readable by id from Rust without a call into C++, the active group as an id list, and goal creation
  as a host call returning a handle. The box selection's pick (`EEPlayer::SelectUnits`: built, not a path point, not
  already in the group) is the first candidate once the store exists. 22 bodies call the UI and stay C++ until R6.

## R6: WON's paths, RegKey, the chat parser, GUIStringData and ConfigObject's grammar in Rust (ee_won path.rs, regkey.rs, chat.rs, guistring.rs, configobj.rs; branch rust-r6-won5, 2026-11-10)

Round 5 of R6 (the map of what is left is in RUST_PLAN's "R6 WON, the rest" row). Each piece was captured over the
C++ first by a driver in `tests/won` (added to `xbuild.py` `WON_TESTS`), the Rust tests replay the golden, and the
driver relinked over the forwarders prints the same.

- **`won_parse_test` after D6.** It failed 39 of 3537 lines: D6 made `WStringToString` UTF-8 and the golden and
  `parser_golden`'s model still had the code page (ASCII and 0xA0-0xFF as single bytes, anything else '?'). The
  model is `text::game_from_utf16` now and the golden is re-recorded from the current C++. The 39 lines are all
  `wstrs` (a wide string read and narrowed) plus the formats line (`L"w\xe9"`): inputs 02, 03, 37-41, 49-52 in both
  widths of the C pass (40 and 41 are wide only: 20 lines) and the same without 39 in the English pass (18: there
  0xA0 is a space, so input 39's token is plain "x"). U+4142 and U+263A were '?' and are their UTF-8 now; the
  Latin-1 units (e9, fc, a0, b2, b3, b9, ff) were one 1252 byte each and are two-byte UTF-8 now. Each is what D6
  asks: game text is UTF-8, nothing is squeezed into a code page.
- **WONFile's path logic (`path.rs`).** The separators, the directory/name and extension splits, `NormalizePath`,
  `CalcFullPath`'s join to the current directory and `GetRelativePath` (the CRT's `toupper` per byte). The class
  keeps its strings, cache flags and platform calls. `won_path_test` (170 lines) works in a directory it creates
  and prints `<T>` and `<V>` for it and its volume, so the golden is the same on any Windows machine; elsewhere the
  absolute-path lines are not compared (they are Windows paths). **Kept quirk:** `NormalizePath(".")` throws
  `std::out_of_range` (the original's `substr(1)` of the emptied path); the forwarder throws it. A name cached
  before a normalisation stays cached (only the directory is recomputed), as the golden's "cached" lines show.
- **RegKey (`regkey.rs`).** The config.yaml names (`.`, `:` and control characters to `_`, blanks trimmed,
  "(default)" for an empty name), the blocks, the binary values' hex both ways and the root range; RegKey keeps its
  block, path and open flag and calls `PlatConfig`. `won_regkey_test` runs on a scratch `$EE_CONFIG` and prints
  the file's lines too (97 lines). Every lobby load reads `UseLocalIP` through it (`LobbyConfig`).
- **The chat command parser (`chat.rs`).** Everything but the tables: the command word, the name matches (full,
  unique prefix, ambiguous), `ParseCommand` after the lookup, `HandleInput`'s kind of line, the help lines a
  client sees, the short names. The command maps stay C++ `std::map`s: the word is computed in Rust, looked up in
  C++, and the rest of the parse is a second call that returns what to set (`EeWonChatOutcome` plus a sink for the
  texts; "not set" leaves a member alone, as the original's early returns did). The CRT's `towupper`, `iswspace`,
  `toupper`, `isalpha` (of a signed `char`) and `atoi` are called as the original called them (`CrtChat`; the
  tests' `CChat` is the C locale). The `crt` module is `cbindgen:ignore`: cbindgen exported its `extern` block
  and a `cfg`'d type alias twice, and the command constants clashed with `windows.h`'s `IGNORE` until they became
  `pub(crate)`. The golden (439 lines) uses the lobby's own command names (`_LobbyResource.cfg`); the driver
  defines the resource globals and copies the four `LobbyClient` methods it needs (LobbyModel.cpp links the whole
  lobby). **Kept quirks:** a rest of line of one character is empty; `/unmute Bob` sets its text to "Bob" (the
  second `GetRestOfLine` starts at the name again); a duration needs a unit letter (`10` is error 7); `-1h` and
  `99999999d` wrap in the 32-bit int; `INFINITE` leaves the previous duration; the constructor leaves `mCommand`
  unset (the driver sets it). Reached by the LAN lobby's chat; `lan_game.py` does not chat, so only the golden
  covers it.
- **GUIStringData (`guistring.rs`).** WONGUI's text: `Compare`, both `CompareNoCase`s, `compare` with narrow text
  (0 only when the narrow text continues with '0': the original's bug, kept), `ToInt` (a 16-bit offset that
  wraps), `find` of a character and of a substring, `findNoCase`, `rFind` both ways (`std::wstring::rfind`),
  `findOneOf`, `toUpper` / `toLowerCase` and `GetString`. Reads at the length see the terminator, as the original
  did. The golden (1280 lines) has a C and an English pass: `towupper` folds Latin-1 only in the English locale
  (`a` vs `é` is -168 in C, -136 in English), so the case maps stay the CRT's. Every lobby label, list and chat
  line goes through this: only a lobby tour or the LAN game exercises it in the game.
  - **Found, not changed (D6):** `GUIString(const char*)` widens each byte and `GetString` narrows a unit below 256
    to its byte and any other to '?': Latin-1, not UTF-8. A lobby text that reaches the game's narrow strings this
    way (or comes from them) is not UTF-8 for a non-ASCII character. The UTF-8 meeting point D6 made is
    `StringUtil` (`StringToWString` / `WStringToString`); the GUIString constructors and `GetString` are a second,
    older one. Changing them changes what the lobby shows for any narrow resource text with a byte above 0x7F; it
    needs its own look at the callers.
- **ConfigObject's grammar (`configobj.rs`).** A token's kind in a group (`}`, `key:`, a group), the header's
  sequence (`[name] [: base] {`, a four-state step function the C++ feeds token by token), the instructions,
  which block of `IFDEFINED` / `IFNOTDEFINED` is parsed (`else` in any case), the `, optional` suffix and its two
  warnings, and an included file's path. The C++ keeps the objects, the key and group maps, the virtual hooks
  (WONGUI's resource and component configs override them), the lexer calls and the texts. `won_cfgobj_test`
  parses 44 snippets (and one with the include path ignored) into a test group with typed keys and three kinds of
  child group and prints every group's keys, the error and the warnings (103 lines); `--files DIR` prints the 61
  lobby files' results and warnings' CRCs, the same over the C++ and the port.
  - **Behaviour change:** `SimpleConfigValue<unsigned short>::ReadValue` assigned the `int` it read into even when
    the read failed (uninitialised: the first capture printed 0, the port's run 182). It keeps the value now, as
    the parser's sized reads do since round 3; the golden was re-recorded over the original ConfigObject with that
    fix.
  - **Kept quirk:** an `INCLUDEGROUP` on a file's last line throws `FileReaderException` out of the parse ("Attempt
    to setpos past end of mem buffer": the state saved before the include is at the end, and the reader, in memory
    mode, refuses a position equal to its length). The lobby's files never end so; snippet 26 records it.
- **What exercises what.** WONFile, RegKey, GUIStringData and ConfigObject run at every lobby load: the LAN game
  (`lan_game.py`) or a lobby tour is their end-to-end check. The chat parser runs only when someone types in the
  lobby's chat (no harness test does). Nothing here touches the simulation: the oracle was identical after each
  gated build.
- **Still C++ of WON:** the lobby logic proper (LobbyModel, LanLogic's objects, StagingLogic, InitLogic,
  LobbyPersistentData's file layout, the resource table, RoutingLogic), StringUtil's case functions, WONGUI's
  components and layout, and everything only the gone internet servers reach (peer auth, AuthContext's sessions and
  certificate flow, GetCertOp, the services' requests and replies).

## R5b.4, fourth round: the unit store's first stage, the box selection's pick in Rust (ee_units store.rs, select.rs; branch rust-r5b4-core, 2026-11-10)

The design is RUST_PLAN R5b.4 "Leaf 5, the unit store, stage one". What it took:

- **The holders.** `EEUnitStoreField<T, F>` (ee/EEUnitStore.h) replaced eight members: `RTSUnit::mTileX`, `mTileY`,
  `mIsValid`, `EEUnit::mUnitClass`, `mComplexUnit`, `mHitPoints`, `EEComplexUnit::mIsBuilt`, `mGoalState`. Same size
  and layout; every assignment form reports the new value (`EEUnitStoreWrote`, the owner from the member's
  `offsetof`, the record checked by the object's address). What the compiler then refused, and the fixes:
  - `dynamic_cast` of a pointer holder (6 sites: two in the AI, the render object, three in the UI's target
    finder): `.Get()`. `static_cast` and C-style downcasts (12) go through the holder's explicit conversion
    template; `EEGoalCast`/`EEUnitClassCast` (10) have holder overloads (ee/EEUnitKind.h).
  - `FSFile::Read(T&)` cannot bind a holder: `EEUnitStoreRead(file, holder)` reads the plain value and writes it
    through the holder (`RTSUnit::Load`'s tile and valid flag, `EEComplexDoppelganger`'s built flag: one line each).
    `Write(const T&)` takes the holder's conversion as it is.
  - **Variadic calls.** A holder passed to `printf`/`sprintf` compiles on Windows (clang-cl only warns,
    `-Wnon-pod-varargs`) but on the Itanium ABI a class with a user-written copy constructor goes by invisible
    reference: the Linux and Mac builds refuse it, or print an address. A scratch compile of the exe's units with
    `-Werror=non-pod-varargs` found 26 sites (the out-of-sync dumps, the goals' descriptions, `RTSUnit::Describe`);
    each is `.Get()`.
- **Where records come and go.** `RTSPlayer::InsertUnit` (after `mPlayer`: a new unit, a loaded one, a kept id),
  `RemoveKilledUnits` (after the unlink), `~RTSPlayer` (by the player object: the editor's forms build temporary
  players that can share a real player's id) and `RTSUnit::FreeNode` (a no-op unless a teardown dropped the list's
  reference). `RTSPlayer::Reset` runs only from the constructors, before any unit, so it is not a removal point.
- **The check** (TEST ONLY, with the capture on): at the start of every `EEPhysics::Tick` and before every game
  message `DrainInput` executes, every unit of every player's list against the store; a USCK record per check.
  Over the 24 capture games of the four profiles (oracle seed 2 alone: 43,204 tick checks and 3 message checks, up to
  878 units per check), no check found a difference: the store equals the objects at every point a decision reads it.
- **Cost.** One store call per write of a mirrored member (a mutex and a `BTreeMap` lookup). The check runs only
  with the capture on (a capture game's walk of every unit at every tick).
- **Not copyable.** A copy of a holder outside its unit would report its writes from a wrong owner (the
  `offsetof`), so the copy constructor is deleted: one `std::make_pair` in the AI's function objects deduced a pair of
  holders and takes `.Get()` now.

**The active group as ids, goals as handles** (`ee_units` host.rs, `EEUnitHost.cpp`): the `Host` trait a Rust
decision acts through (the game's `GameHost` imports the C++ calls; the tests use a mock): `group_ids` (an `EEGroup`'s
vector as ids, in its order), `goal_new` from an `EeGoalSpec` (the constructor and its arguments, units by id,
resolved through the registry; Explore, Attack Ground, Attack Unit, Gather, Heal Unit, Patrol: the constructors the
first bodies use) returning a `GoalHandle` that owns the goal, `goal_kind`, `goal_release`. Nothing in the game calls
them yet: creating a goal in a sweep would move the goals' global counter (`EEUnitGoal`'s ctor), so they are
exercised only once a body moves.

**The box selection's pick in Rust** (`ee_units` select.rs; `EESelectPick`, ee/EEUnitStore.h). Golden-driver
pattern: the loop of `EEPlayer::SelectUnits` was first factored into a C++ `EESelectPick` (ids in, the picked ids
out, in order) with a USPK capture record (every game call, and every five game minutes a sweep over every player:
its list in order, reversed and repeated, foreign, unknown and neighbouring ids, the 63-id chunks the game sends),
whose digests from that C++ became the goldens; then the body became `ee_units_select_pick` over the store, and the
captures equal the goldens in the four profiles. `SelectUnits` keeps the clear, the group's `Add` for each picked
id and the sound.

- **What the original did that the pick keeps:** the group was cleared first, so "not in the group" means "not
  picked yet"; but `EESelectedGroup::Add` adds only a valid unit, so an invalid unit is picked (and becomes the
  sound's unit) at each of its ids. Declared difference: a unit that is not its own complex unit is skipped (the
  original dereferenced a NULL `mComplexUnit` or read a simple unit's built flag past the object); USCK shows no
  player's list holds one.
- **Only a played game reaches the game call** (the local human's selection, `EEMSelectUnits`): EE-SIM's idle human
  never selects, so USPK's game records are empty in every capture and the sweep is the coverage. A tour or a played
  game must show box selection working (select, add, re-select a group with a dead unit).

## R5b.4, fifth round: a box selection through the real input path, the active groups' Fill test and Explore in Rust (ee_units group.rs; branch rust-r5b4-core, 2026-11-10)

- **The box selection exercise** (TEST ONLY; EETestFixture.cpp "the box selection", fixture_driver.box_select,
  `fixture_game.py --exercise random-game --play-seconds N --box-select`, also posix_game.py). No gate reached
  `EEMSelectUnits` before: EE-SIM's human never selects. The fixture's `boxselect` command drags a box on its own
  thread: the pointer set as the `pointer` command does (the game's mouse is relative, DirectInput-style: a
  platform motion event's delta is scaled by `RTSMouse::Update`, so an exact position is set, under the mouse's
  input lock), the left button pressed and released through `ISHandleEvent` (the platform's event path), Shift as
  a key event for the add variant. The game form starts the drag (`EEModeNormal` -> `EEUICSelectUnits::
  StartSelection`), the release builds the message (`EEALasso::SelectUnits`), the physics executes it (the Rust
  pick). The fixture sees each message's decoded ids through a TEST ONLY hook in `EEMSelectUnits`' ctor
  (`gEESelectUnitsTestHook`, NULL unless the fixture runs) and compares the local player's selected group with the
  original's rule over those ids, applied to the objects (not through the store); the driver waits until they
  match on two polls in a row.
  - The dead variant: a unit that can be killed at the player's command is needed. `EEMSelfDestruct` kills only
    classes with a death spawn (the Furies' bomb: one class), `EEMDisband` only classes with `mCanDisband` (none in
    the open db): the Delete key's `EEMKillUnit(0)` (the first selected unit, if `CanDelete`) is the one. The
    fixture queues `EEMSelectTheseUnits([unit])` and `EEMKillUnit(0)` as UI actions just before the release, so the
    box's ids are picked while the unit lives and the physics kills it before it executes the box selection.
  - Locks: the selection is read under the world's read lock before the fixture takes the UI lock, never inside it
    (the UI thread takes the world's read lock, then the UI manager's lock; the actions take the UI lock, then the
    world's write lock).
  - Not run here (no harness sessions): the lead's run is the only evidence that it works in the game.
- **The store's class fields:** `domains` (`EEUnitClass::mDomains`) and `land_building` (`IsLandBuilding()`), both
  fixed by the class's constructor, reported at the `mUnitClass` write with the category and type; USCK compares them.
- **Fill's test** (`EEActiveGroup::Fill`: valid, hit points, `IsInFormationSlot`) is `ee_units` group.rs
  `fill_members` over the store, for every caller of `Fill` (the 15 group-command bodies of EEMessage.cpp).
  Golden-driver pattern: the loop's test was first factored into a C++ `EEFillMembers` (the units in, the taken
  indices out) with a USFL capture record (every `Fill`, and every five game minutes every player's units in every
  slot 0..6, forward and reversed), whose digests from that C++ became the goldens (pure insertions); then the body became the Rust over the store. `Fill` keeps the clear, the formation,
  the sort and each taken unit's move. A unit in no list has no record and is not taken: the original read the
  object, but `RTSPlayer::RemoveKilledUnits` invalidates a unit right after unlinking it (`Invalidate`, the only way
  out of a list), so its test was false too (the C++ phase also stopped EE-SIM if it ever took such a unit).
  **Coverage:** the late profile's computer players build walls (`EECPWallManager` sends `EEMBuildWall`, whose
  body fills a group): 4 to 16 game calls a game, 60 over the six; the oracle, space and calamity games have none.
  The rest is the sweep (every five game minutes, every player's units in slots 0..6, forward and reversed: 504
  records an oracle game, 630 a late one).
- **Explore** (`EEMExplore::Execute`) is `ee_units` group.rs `explore` over host calls (EEUnitHost.cpp: a new active
  group, `Fill`, the pending list, `KeepMobileUnits`, `ClearGoal`, a goal from the host (`EeGoalSpec`), `SetGoal`, the
  action, `Plan`, `GetRandom`, the acknowledgement, the release), in the original's order with one draw per group
  that kept a unit. The coverage: unit tests of the call order over a mock host; `Fill`'s test by USFL. No capture
  game sends Explore (the computer players send no game messages but walls; EE-SIM's human idles), and a sweep cannot
  run the body: it creates goals (the goals' global counter) and gives them to units. A played game must show it
  (select units, the Explore button).
- **Gates:** each commit built 0/0 (EE-AOC, EE-SIM) and the oracle was identical on seeds 2-7; the C++ Fill factor
  and the Rust Fill gave all four capture profiles PASS (with the USFL goldens); the Rust gate PASS.

## R6: GUIString's narrow text as UTF-8, the lobby's game room and its saved settings in Rust (ee_won guistring.rs, staging.rs, persist.rs; branch rust-r6-won6, 2026-11-10)

Round 6 of R6 (the map of what is left is in RUST_PLAN's "R6 WON, the rest" row). Each piece was captured over the
C++ first: the golden driver pattern for GUIStringData; for the game room and the saved settings, drivers in
`tests/won` (added to `xbuild.py` `WON_TESTS`) that run the original's statements, copied before the port, next to
the Rust ABI over the same inputs.

- **GUIString's narrow crossing is UTF-8 (D6).** Round 5 found that `GUIString(const char*)` widened each byte and
  `GetString` narrowed a unit below 256 to its byte and any other to '?' (Latin-1). Now the narrow constructors
  (`const char*`, `std::string`, the bounded one: `AssignStr`) decode UTF-8 (a stray byte is U+FFFD), `GetString`
  and `operator std::string` encode UTF-8 (a lone surrogate is U+FFFD), and `compare` / `CompareNoCase` with narrow
  text decode it first (`guistring.rs` `widen` / `narrow` on `text.rs`, so `ee_plat_core::utf8` is still the one
  conversion). `LobbyPersistentData::ReadByteString` reads its bytes as UTF-8, as `WriteByteString` (GetString)
  writes them, so a game filter's name round-trips.
  - **Finding every caller:** the narrow APIs were marked `__attribute__((unavailable))` for one compile census
    (`clang_build.py won exe`): 328 sites. They are ASCII literals (unchanged), the game's UTF-8 text going in
    (`EELobbyResource_GetString`'s epoch names into the filters, file names and saved-game paths, numbers formatted
    with `sprintf`, `IPAddr::GetHostString`, config parse errors) and narrow results the game reads as UTF-8
    (paths for `WONFile` / `SetPath` / `ListFiles`, emails, config map keys, exception texts). Every one was wrong
    for a non-ASCII character and is right now; `CompareNoCase(const char*)` has 151 callers, all ASCII literals.
  - **The golden** (`won_guistring_test`) was re-recorded: 84 changed lines, in both locale passes: the narrow
    tables' `\xe9t` and `\xc9T` are stray bytes now (they compare as U+FFFD: `-136/-168` became `-65436/-65468`
    and so on for every text, n6 and n7), and `narrow=` of the four non-ASCII texts is UTF-8 (`e974e9` became
    `c3a974c3a9`, U+263A's `3f` became `e298ba`, `ff` became `c3bf`). 188 lines are new: four UTF-8 narrow texts
    (`\xc3\xa9t`, `\xc3\x89T`, U+263A, an astral character) against every text, and the narrow constructors (whole,
    `std::string`, the first half of the bytes: a cut sequence is U+FFFD) and back. 1468 lines, the same over the
    driver and the Rust model.
- **The game room (`staging.rs`).** StagingLogic's room messages (types 1-10: join request, reply, player joined,
  ready request and change, dissolved, start, ping report and change, kicked) are built in Rust (the heads of 1
  and 3, whose rest is the game code's), read in Rust (the type and the fixed fields; while a join waits only the
  reply counts; a short message does nothing, as before: no handler acted before its reads), and who acts on which
  is `room_accepts` over the sender's and our roles; `HandleMessage` positions the `ReadBuffer` after the fixed
  fields for the game's hooks. `UpdateReadyState`'s sounds and remembered state, `HandleStartGame`'s exit code,
  LobbyGame's `IsFull`, `GetJoinStatus` (before the hook), `HandleJoinRequest`'s first checks and `MatchesRoomSpec`,
  the listing summary (`WriteSummary` / `ReadSummary`: a short summary keeps the fields read before the cut, as the
  original assigned them while reading) and `LobbyPlayer::WriteToBuffer`'s head are Rust. The handlers for types
  3-10 take the read message (`const EeWonRoomMsg&`) instead of re-reading. `won_staging_test` compares the
  original's statements with the Rust over every message and value set, all 256 type bytes with their fields cut
  at every length in three join states and 16 role sets (with two bodies), the ready state over 32 x 8 inputs, the
  join rules and every summary of each game type cut at every length: 175,517 checks, 0 failures.
  - The new constants are `pub(crate)`: as global `constexpr`s in `ee_rust.h` (`START_GAME`, `GAME_LAN`,
    `PING_REPORT`, ...) they could clash with the game's names, as the chat commands did with `IGNORE`.
- **The saved settings (`persist.rs`).** Both files' layouts: written from the C++'s lists (`EeWonGlobalPrefsOut`,
  `EeWonUserPrefsOut`), read as a stream of events (`EeWonPrefsEvent`) in the order the original assigned them, so a
  cut file leaves exactly what the original left (a reconnect entry exists from its address on, a game filter from
  its name on, a saved user from its name on, the recent LAN name is chosen only after the LAN names were read
  whole). The C++ keeps the maps, the password key (Blowfish of the machine id: `EncryptString` / `DecryptString`)
  and the defaults; it reads the whole file (`ReadMaxBytes`) and writes it in one `WriteBytes`. The user file name,
  `FindUnusedUserId`, the pruning of users and LAN names (a multimap by time: equal times in map order), the recent
  LAN name and the default name's number (CRC-16 of the address) are Rust too. **Deleted:** ReadFileHeader,
  WriteFileHeader, ReadByteString, WriteByteString, ReadWideString, WriteWideString, ReadEncryptedString,
  WriteEncryptedString (its encryption is `EncryptString`), ReadLanPrefs, WriteLanPrefs.
  - **Kept quirks:** a game filter with no values is written without its count (the filters after it are misread
    on the next load); a setting byte is on only when it is 1; another version (or tag) returns before the dirty
    flags are touched.
  - **Behaviour change (POSIX only):** a wide string is written as its UTF-16 units and read back joining
    surrogate pairs; the original wrote each 32-bit `wchar_t` as a short (an astral character was cut). Windows is
    unchanged.
  - `won_persist_test` saves 32 variants of both files through the original's statements and the port (the same
    bytes), then loads each file cut at every length, with a wrong version and with an odd byte, both ways into
    stand-ins of the same containers (the same afterwards), plus the rules against the original's: 26,855 checks, 0 failures.
- **What exercises what.** The LAN game (`lan_game.py`) runs the summary (the joiner finds the game by it), the
  join request and reply, the player joined message, the ready state, the start message, the join rules and the
  saved settings of the LAN user (`_wonuser0.dat`, its LAN names and port; `_wonlobbypersistent.dat`) at the lobby's
  start and end; a lobby tour runs the settings and GUIString everywhere. Nothing in today's game sends a kick, a
  dissolve while others are in the room, a ping report (the LAN games are not pinged), or saves an Internet user
  (the login servers are gone): the drivers cover those. Nothing here touches the simulation; the oracle was
  identical after each gated build.
- **Still C++ of WON:** LobbyModel's client, group and server lists, LanLogic's objects, StagingLogic's flow (host,
  join, leave: dialogs, ops and events), InitLogic, the resource table, RoutingLogic, StringUtil's case functions,
  WONGUI's components and layout, and everything only the gone internet servers reach (peer auth, AuthContext's
  sessions and certificate flow, GetCertOp, the services' requests and replies).

## R5b.1, finishing: the sparse point set, the manager's last loops and the path manager's splicing in Rust (ee_astar sparse.rs, manager.rs, path_splice.rs; branch rust-r5b-cont2, 2026-11-10)

The map of what was left in the A* layer and the continent analysis went into RUST_PLAN's R5b.1 row first; this
round moved the pieces that need neither the unit store nor the hulls, each its own commit.

- **New capture records first, from the unchanged C++:** CNTN (every `GetTilesNextTo`: the tile, the id, the
  result, the tiles) and PMSP (every `TrimPath`, `AppendWaypoints`, `JoinPaths`, `FinishFailedPath` detour search
  and the bounded searches' revolutions and bounds, with a kind, the inputs and the outputs). `FinishFailedPath`'s
  detour search became its own static function for this (same code). The goldens were recorded with that commit's
  build in the four profiles, in the same runs that showed every older digest equal to its golden, and only the new
  tags' lines were inserted (`merge_golden`-style: (seed, tag) order kept; the CNTN counts are 600-900 a game,
  PMSP 12,000-21,000).
- **`U2DSparseArrayPointContainer` whole** (`sparse.rs`; `U2DSparseArray.cpp` is forwarders). The tile sets of
  every AI area (continents, path areas, hills, forests, choke points, rectangle areas), of the path sub sectors
  and the explore goal's bad tiles. The C++ class keeps its name, vtable and API and owns one `EeSparseArray`
  handle; the run node class, the column list header and their `UList` node pool (`SetPoolSize`,
  `DumpPoolStats`) are gone: nothing outside the class used them. Kept from the C++: the point lists walk the
  columns from the last and each column's runs upwards, `operator[]` from the first column, `GetClosestPoint` keeps
  the last tie walking the list from its end (`<=`), `Insert` ignores a point left of the array with x <= 0, a
  column emptied by `Remove` is deleted but the array keeps its size, and `GetRandomPoint` calls back into the
  caller's `URandom::Generate()` in the C++'s order and number (16 column tries, then a run, then a y) with the
  float expression `(long)(Generate() * (max + 1 - min) + min)` (a C cast: truncation, so `as` is right here).
  Changed: the copy constructor shared the source's column pointers, so both destructors freed them; a copy now owns
  its points. `Size()` is out of line.
- **A capture read uninitialised memory, and the smaller areas changed it.** After the sparse array moved, ASGR
  differed in all six oracle games with the same record counts and an identical oracle. A field diff of the two
  captures (one seed, both exes) put every difference in four bytes of ten records: `mStartSector` of the continental
  chain machine (type 7), a stack object in `NumberContinentChains` whose goal never sets it. The graph goal base's
  ctor left `mGoalSector`/`mStartSector` unset (only `Reset` zeroed them), and the golden happened to see 0. The
  ctor now sets both to 0; nothing reads them before `SetStart`/`SetGoal`. A record that reads a member the code
  never sets is a trap for every later layout change: such fields should be initialised, not left out.
- **`near` is a macro on Windows** (`windef.h`): a cbindgen struct field named `near` broke every unit including
  `ee_rust.h`. The prune host's callback is `is_near`. (The `near`/`far` parameter names in `ee_dx7`'s exports
  compile only because a parameter name may vanish.)
- **The manager's last loops** (`manager.rs`): `GetTilesNextTo` walks the ring of `EEGetSquareRingTiles(x, y, 1)`
  from its last tile, with the manager's `GetContinentID` as a callback (the id grid is freed in small games and the
  lookup then searches the sub sectors, so a snapshot of nine ids would cost the calls the C++ skipped);
  `PruneChokePoints`' loop asks the C++ objects through `EePruneCallbacks` in the C++'s order (the closest points
  are the areas' C++ geometry, the deletion the C++'s); `theCount >= mAreaSerials.size() - 1` is unsigned, so a
  choke point without areas never goes. `CreateScenarioAreas`' choice: the set areas with a positive height and
  width, from the last, ids from 1, the sides truncated as the `(long)` casts (no oracle game has scenario areas:
  only the unit test covers it).
- **The path manager's splicing** (`path_splice.rs`): `TrimPath`'s cut, `AppendWaypoints`' points, `JoinPaths`'
  test, the detour search, `TryShortPath`'s revolutions and the bounds of both bounded searches, and
  `SetGraphRevs`/`SetPathRevs`' budgets (ASRN and ASGR record the max revs). The path history's walk for the repeat
  stays C++ (saved state, branch rust-r5a-aisave3's). Points pass as `EePathPoint` over `GE3DPoint` (layout
  asserted) and compare by tile through `UMFloor` (`lrintf`, then one down when that went up). `MergeWaypoints` has
  no caller and stays as it was.
- **Left** (RUST_PLAN's row): the hulls (`U2DPolygon<long>`/`U2DConvexHull<long>`: float math with `Normalize`,
  shared with the float users) and the area geometry on them; the object bookkeeping of `Rebuild`/`RepairPathArea`
  and the object queries, after the objects; `CanGetTo`, the goals' unit reads and the path manager's request
  handling, after the unit store (the store's first stage on `rust` has owner, kind, tile, category, type, built,
  goal kind; `CanGetTo` would add the class's domains and `CanMoveOnTerrain`, the planet id with its lazy write and
  the footprint continent); the history and regions, saved state.
- **Gates** (on the merge of `rust` 4d136f16, the unit store's first stage; the merge needed no resolution, the
  header was current): `build.py -j 4 --sim` 0/0 for EE-AOC and EE-SIM; the sim oracle identical on seeds 2-7
  (2,691-2,825 lines a seed); `sim_capture.py check` PASS in the four profiles with no tag ungated (285, 300, 289
  and 301 digests in oracle, late, space and calamity, CNTB/CNTR/CNTN/PMSP/ASRN/ASGR among them); `rust_build.py
  gate` PASS (1,462 tests; ee_astar 108, 16 new); the windowed-software test profile 0/0. Before the merge the
  three leaves were built together (0/0, oracle identical) and each commit was checked with `cargo test`, clippy,
  rustfmt and the header; the intermediate commits were not linked one by one. Not run: `gates.sh` or any harness
  session, `eedata check`/`cppgate` (no data change), macOS or Linux builds. Nothing here is drawn: the sparse array
  is read by the renderer only through the AI's areas (no draw path), so no tour-only risk is known.

## R6: the lobby model's lists, LanLogic's and the game room's flows, the start-up checks, the resource table, RoutingLogic's decisions and StringUtil's case in Rust (ee_won model.rs, lanlogic.rs, flow.rs, resource.rs, routing.rs, strcase.rs; branch rust-r6-won7, 2026-11-10)

Round 7 of R6 (the map of what is left is in RUST_PLAN's "R6 WON, the rest" row). The lobby's logic classes keep
their objects (clients, groups, games, servers and their `std::map`s of `SmartPtr`s, the sockets, timers, ops,
dialogs and events) and ask Rust what to do with them. Each piece was captured first: a driver in `tests/won` (added
to `xbuild.py` `WON_TESTS`) runs the original's statements, copied from the C++ before the port with the objects
replaced by the state they answered, next to the Rust ABI over every combination of their inputs; the resource
table's golden is the original's statements read out of the C++.

- **The lobby model (`model.rs`).** `LobbyClientList::SyncClients` and `LobbyServer::SyncGroups` apply a merge plan
  (`sync_plan`: the sorted walk over the lobby's and the routing server's maps, drop / create / refresh / drop the
  rest, in the original's order, so a room read refreshes its members' flags in the same order); the populations
  (`SyncGroups`' sums, `AddClientToGroup`, `RemoveClientFromGroup`'s clamp at 0, `GetNumClients`); what a client's,
  a member's and a room's flags say; the friend's status and locate mark; the ping states; `RemoveStaleGames`' rule;
  `IsSameGame`; which games a list keys by id; the join status texts; `AreAllReady`; the directory entry's `_C` and
  `_L` objects (`ShortFromLittleEndian` deleted). `won_model_test`: 607,933 checks, 0 failures.
  - **Found: undefined behaviour in the original merges.** Both hinted their inserts with an iterator that started
    at `begin()`, and the first erase left it dangling. The first run of the driver's copy of the original hung
    (this STL loops in the tree walk). The hint starts at `end()` now, in the port and in both walks of the driver
    (a valid hint never changes a map's content). Only the routing maps (Internet) reach these merges.
- **LanLogic's flow (`lanlogic.rs`).** A received message (the host stamps the socket; a socket with no player only
  sends a join request; else `HandleMessage`, whose read skips the host's own length prefix), where a message goes
  (`SendMessageToClient`, `SendMessageToCaptain`, `BroadcastMessage`: the local delivery first, then the sockets as
  the loop met them after it), a closed socket (the host announces the departure and forgets the socket, a joiner
  reports the lost host), a received game summary (kept and stamped, or dropped; the event: added 0, changed 2, and
  a known game that sent an unreadable summary is reported changed, as the original's `aFlag ? 2 : 1` under
  `if (aFlag)` did), `CanBroadcastGame`, which games are advertised, when peers authenticate, `JoinGame`'s status
  loop (any status but 0-3 sends the request again), `ReadJoinReply`'s roles (the joiner's own id wins over the
  captain's), the room's player actions, the typed chat commands, the refresh command, the LAN name and port dialogs
  and a direct-connect entry's port, name and event. `won_lanlogic_test`: 10,881 checks, 0 failures.
- **The game room's and the start-up's flows (`flow.rs`).** StagingLogic: the message routes, an announced player
  (read it, announce it again as host, ignore it), the all-ready state a not-ready newcomer ends,
  `SetGameInProgress`, `ToggleReady` (`CanReady` asked only for ready), `HostGame`'s firewall warnings,
  `VerifyPingBeforeJoin`, which join statuses are shown (not 0 or 1), `LeaveGame`'s confirmation, `HandleKicked`'s
  event and message, `HandleGameAction`, `NotifyPingChange`, `KickClient`, `HandleClientLeft`. InitLogic:
  `SetError`'s kept and ignorable errors, the Internet check, the directory answer's target lists and the missing
  ones, the version status, the firewall connect-back, which terms of use are saved and how many bytes, the retry
  events, and `ParseCrossPromotion`'s lines: the original's `strtok` walk in Rust (words split at blanks, tabs,
  newlines and '=', `link` / `image` in any case take the rest of the line, so "link = x" gives "= x"; the document
  ends at its first NUL). The CRT and the dialogs are asked only where the original asked them (the C++ keeps the
  short circuits). `won_flow_test`: 96,285 checks, 0 failures (20,000 generated adverts among them).
- **The resource table (`resource.rs`).** The 469 lobby strings, images and sounds, each with its section, key and
  kind, in the original's load order (30 sections, each looked up again where it changes, as the 499 statements of
  `LobbyResource_SafeGlueResources` did); the unload ran the same order (strings emptied, images and sounds
  released). The two functions are now loops over the rows; the globals stay C++ (the lobby uses them by name) and
  are paired with the rows by name (`#theGlobal`, checked at each load: a mismatch stops the program). The golden
  (`golden_resource.txt`, 968 statements) was read out of the original functions; the Rust test renders the table
  back into the same statements. **Deleted:** the bodies of `LobbyResource_SafeGlueResources` and
  `LobbyResource_UnloadResources` (968 statements). The table by id (`gResources`, `LobbyResource_Get*`) and
  `LobbyResource_Init`'s control ids stay.
- **RoutingLogic's decisions (`routing.rs`).** `GetRoomSpec`, `GetGroup`, Quick Play's ping ceiling and rank
  (`SelectGame`; the rank goes into the original's `multimap<unsigned short>`, so a rank over 65535 still wraps),
  `ShouldLocateFriend` (the `unsigned long` difference as a `long`), `GetDefaultServer` (the last used server at
  once, else the busiest listing the language, else the busiest; the first of equals), the anonymous and dirty-word
  flags, the friends list's 255. `won_routing_test`: 75,029 checks, 0 failures. Internet only: the LAN path meets
  RoutingLogic only through shared events, which its guards drop without a server.
- **StringUtil's case (`strcase.rs`, D6).** Game text is UTF-8 and the UI thread sets `LC_CTYPE` "C", so the CRT's
  `toupper` / `tolower` / `towupper` / `towlower` changed only ASCII letters in the game; the port does exactly
  that, without the CRT: a UTF-8 sequence's bytes and every non-ASCII unit pass unchanged in any locale. The
  compares keep the original's order: a narrow text's upper-cased bytes compare as signed `char`s (a UTF-8 byte
  sorts before ASCII, as `StringLessNoCase`'s maps always sorted), wide units as `wchar_t`s (unsigned 16-bit on
  Windows, signed 32-bit elsewhere). `won_strcase_test` runs the original loops in the C locale against the port over
  every byte, every 16-bit unit and every pair of 241 narrow and 307 wide short texts: 417,690 checks, 0 failures.
  Not done: full Unicode case mapping (a behaviour change; nothing in the lobby needs it).
- **What exercises what.** The LAN game (`lan_game.py`) runs, of this round: the client list (`AddClient`,
  `FindClient`), the LAN game list (`GetOrCreateGame`, `RemoveStaleGames`, the key), `IsSameGame`, `AreAllReady`,
  the status texts on a refused join, all of LanLogic's flow but the chat commands, player actions, the name and port
  dialogs and direct connect, StagingLogic's routes, hosting, joining, ready and start, the resource table (every
  lobby load) and the case functions (the .cfg keys, the layout's component and font names: every lobby load). A lobby tour runs InitLogic's `Init`, `Start` and
  `SetError` (the directory lookup fails: the servers are gone), the resource table and the case functions. Nothing
  in today's game reaches the routing merges, the populations, the friends, the directory objects, RoutingLogic's
  decisions, InitLogic's directory, version, firewall, terms-of-use and advert paths, kicks, dissolves or ping
  reports: the drivers cover those. Nothing here touches the simulation; the oracle was identical after each gated
  build.
- **Gating.** The model, LanLogic and flow commits were each built (link 0/0), oracle-identical, Rust-gated and
  driver-checked on their own; the resource table, RoutingLogic and StringUtil commits were built and gated once
  together (their trees differ only by modules nothing else calls; each commit's header was regenerated for it).
- **Still C++ of WON:** WONGUI's components and layout (39.3k lines, next), RoutingLogic's flow over its ops and
  servers, the lobby's dialogs, and everything only the gone internet servers reach (peer auth, AuthContext's
  sessions and certificate flow, GetCertOp, the services' requests and replies).

## R5b.1, the geometry: the AI's hulls and the areas' geometry on them in Rust (ee_astar hull.rs, area_geom.rs; ee_ge hull.rs; branch rust-r5b-cont3, 2026-11-10)

The third round of the continent analysis' map (RUST_PLAN's R5b.1 row): `U2DPolygon<long>`/`U2DConvexHull<long>`
and the area classes' geometry on them, each its own commit, after a capture recorded from the unchanged C++.

- **Two capture records first** (`HULL`, `AREA`; `tools/sim_capture.py`): every hull `SetPoints` with its input and
  the whole hull (bounds, center, area), `GetClosestOutlinePoint`, `GetClosestPoints`, `GetClosestPoint`, `Contains`
  and `GetScanLines` with a 32-bit FNV hash of the hull as its identity, the input and the result; and the areas'
  `GetClosestPoints` (both classes), `GetClosestHullPoint`, `GetHullDistanceSquared`, `GetTiles` and
  `GetShoreInRegions`. **Only the outermost call of a thread is recorded** (`EEGeomCaptureScope`, a thread-local
  depth): a hull call made inside an area call, or inside another hull call, never reaches the digest, so the digests
  stay the same when the inner calls leave the C++ with their caller. Without that, moving
  `EEAIArea::GetClosestPoints` to Rust would have dropped thousands of `GetClosestOutlinePoint` records and failed
  the gate it was meant to pass. About 13,000 HULL and 700-2,000 AREA records a game.
- **The template is the exe's alone now.** R4.3 had moved the engine's `U2DConvexHull<float>` (the terrain
  volume's) to `ee_ge`, so nothing instantiated the float bodies: `U2DPolygon.h` lost them with
  `U2DGrahamsAlgorithm`, `U2DPointLess` and the engine's `std::less<U2DPoint<float> >`. The class keeps its members
  and slots; its points pass to Rust in place (`EeHullPoint` is two `c_long`s, `U2DPoint<long>`'s layout, asserted),
  so the Linux build's 64-bit `long` keeps its arithmetic too. Gone as dead: the polygon's crossing-number `Contains`
  (its one subclass overrides it; the slot is pure) and the hull's `AddScanLine`/`Compute` slots.
- **Shared, not duplicated:** the Graham scan, the point order, the side test (`double` from `T` differences) and the
  scan line walk moved from `terrain_volume.rs` into `ee_ge::hull`, generic over `f32` and `c_long`;
  `ee_astar` depends on `ee_ge` for it and for `GEVector`'s `Normalize`. The long hull's own arithmetic is the exe
  view's: `DistanceToSquaredF` is `(float)(dx * dx) + (float)(dy * dy)`, `ProjectPoint` scales `DistanceToF`
  and truncates `dir * d + start` (a C cast: truncation is right, not `lrintf`), the center is a float mean from
  the last point truncated once, the area a float fan from the first point.
- **The angle sort is exact for `long` without MSVC's sort.** The C++ comparator deleted the nearer of two collinear
  points as a side effect of the comparisons `std::sort` happened to make. With integer points the side test is
  exact, so the comparator is a strict weak order (angle around the lowest point, then distance) and any correct sort
  compares every two neighbours of its result: exactly the non-farthest point of each ray gets deleted, whatever the
  sort. A unit test replays the C++ comparator, side effects on the value being inserted included, through MSVC's
  insertion sort (its `std::sort` below 33 elements) on 3,000 crowded grids and gets the shared hull every time.
  (For `float` points the rounded side test can break the order; that hull is the renderer's only.)
- **Undefined behaviour kept out:** `GetClosestPoints` with an empty other hull read its point -1 (index -1 passed
  `-1 < (long)size()`); Rust returns (0, 0) twice there. The scan's `theStack[size - 2]` with one point on the stack
  cannot happen with an exact side test (kept as a guard).
- **The areas' geometry** (`area_geom.rs`): the closest points' loop of `EEAIArea` (over `BuildPointList(1)`) and
  `EEAIContinent` (over the shore) is one function; `GetClosestHullPoint` takes the bounds; `GetTiles` pushes through
  a callback; `GetShoreInRegions` asks the manager through `EeShoreCallbacks` (path area region, land test,
  `GetContinent`) in the C++'s order and number, `GetContinent` once per id as the C++ loop did. `PruneChokePoints`'
  near test reaches this through the areas' virtual `GetClosestPoints`, so it asks no C++ geometry any more.
- **Not moved, mapped** (RUST_PLAN's row): the object bookkeeping of `Rebuild`/`RepairPathArea` and the manager's
  object queries (`GetChokePointAt`, `GetNearestLandTile`, `GetRandomArea`, `RemoveForestTile`, `UpdateWallUnit`)
  walk the C++ area objects through their virtuals. The objects cannot move with them yet: `EEAIArea` has nine
  subclasses (the seven analysis areas plus `EETown`, `EEHerd` and `EECPIArmy`, which are AI decision objects and
  saved state), 48 AI and unit files read their members directly (`mHull`, `mTiles`, `mShoreTiles`, `mCategory`,
  the id lists), and the continent is also a graph node (`AStarGraphableObjectBase`). A handle per area needs
  accessors at every one of those sites and the save layout's owner (branch rust-r5a-aisave3).
- **Disk:** the stage slots of old capture runs (`build/sim/stage/capture-*`) hold copies of the written folders;
  delete them with their runs.
- **Gates** (each step on its own exe): `build.py -j 4 --sim` 0/0 for EE-AOC and EE-SIM at every step; the sim
  oracle identical on seeds 2-7 for the capture, hull and area builds (2,691-2,825 lines a seed); `sim_capture.py
  check` PASS in the four profiles for the capture build (285, 300, 289, 301 digests: the capture changes nothing),
  then for the hull and the area builds with HULL and AREA gated (297, 312, 301 and 313 digests, none ungated);
  `rust_build.py gate` PASS (1,490 tests; ee_astar 122, 14 new); the windowed-software test profile 0/0. The merge
  of `rust` was a no-op (no new commits). C: ran full during the area build's space profile (other agents' runs
  too): its truncated captures failed every tag by count, so that profile was rerun alone and passed. Not run:
  `gates.sh` or any harness session, macOS or Linux builds. **Tour only:** the terrain volume's hull
  (`terrain_volume.rs`) now calls the shared `ee_ge::hull` (same code moved; `trunc_long` saturates at `c_long`'s
  limits, the same on Windows); nothing else here is drawn.

## R5a.2, fourth round: the continent manager in the save, the sub groups' positions (save 0.263-0.264, branch rust-r5a-aisave3, 2026-11-10)

Continues "R5a.2, third round". Same test (`sim_oracle.py saveload NAME --seeds 2-7 --at 600,1350,2100`, 18 saves).
Each commit was built and gated on its own tree (build 0/0 for EE-AOC and EE-SIM, the plain oracle identical to ref
on seeds 2-7, the Rust gate, sim_capture's four profiles PASS) and measured with the full saveload run.

- **0.263, the continent manager's regions** (`EEAIContinentSave.cpp`, ee/EESaveGameFiles.h
  `EESaveHasContinentRegions`). A load ran BuildRegions over the map as it then was; the running game built its
  regions once at the start and repaired them since (RepairPathArea, forests losing trees). The loaded game had other
  path areas, sub sectors and choke points, under other serials: a load numbered the herds and towns (in
  RepairPointers) before the regions, the running game after them. The computer players name areas by serial and
  tile (EECPILocation), and the load re-found them (Restore calls Update) before BuildRegions had made any choke
  point: a choke point task became a continent task (seed 7's 600 s save), a town task whose location names player 0
  (two of the original's tasks leave mPlayer 0) lost its area, an inactive task with no area got a continent.
  - The save now holds the manager as it is, as one byte block of its own format after the dead attackers: the
    counters, the region information and gEEAIRegionSize, the grids (tile values, continent ids, sub sector ids,
    continent adjacency), every continent, hill, path area, forest, choke point and rectangle area (serial, kind, the
    sparse tile set as it is, empty columns too, the hull as it is, the bounds and each kind's fields), the sectors
    and sub sectors (numbered: their maps, neighbours and links), the shore tiles, free ids and pending sector tiles,
    the serial table (each serial's owner) and **the map tiles' AI values** (EETile::mAIValue).
  - A load makes the regions from it where it ran BuildRegions: Clear's continents and hills stay as objects and
    take the saved contents, the rest is the saved objects, the herds and towns the load made again take their saved
    serials by player and id, the counter is the saved one. From the start of the load to its first tick,
    EECPILocation::Update keeps the saved area (gLoadKeepsLocations): Restore and the load's group refresh no longer
    re-find it. The point sets are saved as they are (U2DSparseArrayPointContainer::GetRaw/SetRaw over ee_astar
    sparse.rs `raw`/`set_raw`, empty columns too: GetRandomPoint draws a column), the hulls too (U2DConvexHull's
    `PortWrite`/`PortRead`: AddTiles may leave a hull stale).
  - **Lesson: BuildRegions writes outside the manager.** UpdateTileValues sets every map tile's mAIValue, which the
    path searches read as a cost. Without it the manager's digest of the loaded game equalled the continued game's
    and still a gatherer's new path went one column over (seed 7's 2100 s save, tick 1): the first restore made 3
    saves worse at CT granularity until the tiles' values were in the block.
  - Measured: the manager's digest (the dump's `AREAS digest=`, the same bytes a save writes) of the loaded game equals
    the continued game's at the first tick on every save tried (7/600, 7/2100, 2/2100); the attack sub groups' task
    areas equal. No change at CT granularity (the same first divergent lines on all 18).
- **0.264, the attack sub groups' positions** (EECPAttackSubGroup::mPosition, `EESaveHasSubGroupPosition`).
  FindPosition keeps the position between its refreshes (every 5 s) and the target scan works around the area it
  leaves. A loaded sub group had (0, 0), so it took its rally point, scanned there, found no target, left the attack
  state and skipped its units' updates (seed 7's 600 s save: the defence commander drew where the continued game's
  sub group unit did). First divergent CT line, before -> after: seed 2 58/2/1 -> 58/86/20, seed 3 94/0/2 ->
  94/0/57, seed 4 6/1/0 -> 80/1/0, seed 7 3/178/1 -> 10/178/1; seeds 5 and 6 unchanged.
- **Tried and dropped** (no effect at tick or CT level, not committed): skipping the computer players'
  RefreshResourceCenters for 0.263 loads (the resource centres, groups and personnel are saved). Seed 4's 2100 s iron
  gatherer still goes to the other mine.
- **The load-path casts**: the 17 `dynamic_cast<EEComplexUnit*>` of the load path (EEComplexDoppelganger's Load and
  RepairPointers, the RepairPointers of EEUGAttackUnit, EEUGCastCalamity, EEUGGather, EEUGPatrol, and
  EEAirTrafficControl's) are `EEUnitCast<EEComplexUnit>` (ee/EEUnitKind.h).
- **The numbers** (first divergent CT line after the load, 600/1350/2100 s, start -> end of this round): seed 2
  58/2/1 -> 58/86/20, seed 3 94/0/2 -> 94/0/57, seed 4 6/1/0 -> 80/1/0, seed 5 23/91/23 unchanged, seed 6 59/16/0
  unchanged, seed 7 3/178/1 -> 10/178/1. Still **0 of 18** continue identically to the end.
- **Tools** (TEST ONLY, committed): the dumps' `AREAS` line (the manager's digest, serial counter, list sizes and
  serial table), the ASG line's `refresh=` (what Update would make of the task's area) and `player=`, each unit's
  waypoints (`wps=`). Scratch (not committed, under the worktree's build/hunt): `pair.py SEED T` (a continued and a
  loaded game of one save with a dump after each physics tick, `--rng` for the draw traces), `nd.py` (the first
  differing dump lines, raw bytes left out), `tr.py` (the first differing AI decision, the trace's statistics lines
  left out: their timers are trace statics), `many.sh`, `gate.sh` (the per-commit gates). A pair run of a 2100 s
  save takes about a minute. The rng trace's callers resolve with `tools/sim.py resolve --map
  build/clang/exe/EE-SIM.map` after adding the preferred base 0x400000.
- **Still different after a load** (measured next causes):
  - *seed 4 2100 s, tick 8*: player 3's idle iron gatherer gets mine 0x149 (far, near 70,33) in the continued game
    and 0x147 (near 89,64) in the loaded one, from EECPResourceManager::FindResourceTarget (the resource centres
    walked backwards, FindResourceUnit, IsVisibleTo, HasEnemyNear, EECPRMResourceInDanger). No random draw differs
    and the dumps are equal: the state is in one of those tests, not dumped yet;
  - *seed 6 2100 s, tick 0*: player 3's town 3 is rebuilt as two towns (the building at 19,52 alone as town 10, an
    extra patrol group): the load adds the buildings in its own order and CanTake refuses one the saved town's hull
    had taken. A town has no unit list (tiles, hull, counters): saving the towns' tiles and hulls and making them
    from the save would fix it;
  - *seed 3 1350 s, first CT*: the draws part between ticks 0 and 7 (the dumps' tickrandom differs); not traced yet;
  - *seed 2 2100 s, tick 16* and *seed 7 2100 s, tick 20*: a sub group picks another task / a unit another attack
    destination; the trace's "income" and other `cp` statistics lines differ at once after a load (trace statics,
    not game state).

## R4.3, sixth part: the text: GEText's extent, GEFontBitmap's cells, alpha pass and draws (ee_ge font.rs, font_ffi.rs; branch rust-r4-ge6, 2026-11-11)

`GEText::ComputeTextExtent`, `GEFontBitmap`'s glyph atlas layout (the constructor's row and texture decisions),
`CreateSurfaceAlpha` and both `GEFontBitmap::Draw`s (the walk over the UTF-8 text, the Windows-1252 cell lookup,
the pen, and the clipped form's whole / cut / skipped decision with its cut rectangles) are Rust. `GEFont`, `GEText`
and `GEFontBitmap` stay the C++ classes (members, reference counts, exported names); no class moved wholly, so no
C++ file was deleted. `GEFont` has no logic of its own (its constructor passes the style bits to `PlatFontCreate`),
and the DX7 text draw (`DX7Rasterizer Draw2D.cpp`, R4.2's `ee_dx7` `draw2d.rs`) was already Rust: with this part the
Graphics Engine owns no text logic in C++. There is no line breaking in the engine (the UI's `ee_ui` text.rs does it).

- **Plans, not callbacks.** The C++ hands Rust plain data and runs the side effects itself, in the original's order:
  the constructor asks `ee_ge_font_cell_place` (a `GeFontPacker` it keeps on its stack) for each cell and creates,
  unlocks and alpha-passes the textures and renders each glyph when told; a draw gets a vector of `GeGlyphDraw`
  (cell, location, and for a cut glyph the destination and source rectangles) and makes the bitmap calls
  (`SetTexture` also for a skipped glyph, as the original did; `ComputeStretchedCoordsAndScale(destination,
  destination, source)` for a cut one). `GEBitmap`'s own arithmetic was already Rust (fifth part).
- **No platform dependency in ee_ge.** The decoding and stepping are the platform's (`PlatUtf8Decode`,
  `PlatMbsNInc`, `PlatCp1252ByteOf`) and the measure is `PlatFontMeasure`: the C++ passes them in a function table
  (`GeTextOps`, `GERustText.h`). Linking `ee_plat_core` into `ee_ge` would have put its exported `PlatMbs*` into the
  random map gate's static library next to `tools/rmap_gate/rmap_stubs.cpp`'s own. `ee_plat_core` is a
  dev-dependency: the tests run the same functions the game passes.
- **Quirks kept.** The extent is measured in chunks of 256 *characters* (`GetLength`, D6) and each chunk's integer
  width is added as a float; the atlas starts a new row when a cell would reach x = 256 (`>=`: a cell may end at
  255, not 256) and a new texture when it would reach y = 256; a new texture does not reset the row height (only a
  new row does); the row height is an `unsigned long` the cell heights are truncated into; the pen advances by
  `(right - left) + x`, not by the width; a clipped glyph is inside when its inclusive rectangle (width and height
  less one) is, and a glyph fully outside still advances the pen and still sets the bitmap's texture.
- **A heap overrun D6 left in `UWideString(const char*)`** (its own commit). The constructor allotted two bytes a
  character plus two: under Windows-1252 always enough, under UTF-8 not for text of 3- and 4-byte characters ("€€"
  needs 7 bytes, got 6; 64 x "aé€😀" needs 641, got 514), and `strcpy` wrote past the buffer. The golden driver
  crashed on it at its first long emoji text. The room is now at least the bytes and the terminator; text of 1- and
  2-byte characters keeps the original's capacity (so every Latin-1 string allocates as before). Only the
  `const char*` constructor sized by characters; the assignments and `CopyString` size by bytes.
- **The golden driver** (`build/font_golden`, scratch, `build.py orig|cur`): `GEText.cpp`, `GEFont.cpp` and
  `GEFontBitmap.cpp` of `d3707423` (`git show`, unmodified) or of the worktree, their includes replaced by
  `fake_ge.h` (the `GERust*.h` seams kept), with the real `UWideString.cpp`, `GE2DPoint.h`, `GERectangle.h`,
  `GEText.h`, `GEFont.h`, `GEFontBitmap.h`, and a scratch Rust staticlib of `ee_ge` + `ee_plat_core`. Fakes: fonts
  (seven: a hash advance per code point times a scale, one pixel more per measured run so a chunked measure differs
  from a whole one, zero-width glyphs, character ranges 0..255, 32..126, 65..90, 32..255, one without a `PlatFont`),
  a rasterizer that writes a pattern into the surfaces with some zero greens, screens with row padding 0..2 and one
  dropped MIP level, textures, and a `GEBitmap` that logs every call and stretches through the Rust. Inputs: 26
  texts (empty, one glyph, ASCII, the copyright line, Latin-1, all the 1252 specials, the three C1 controls 1252
  leaves undefined, Greek, CJK, emoji, a stray byte, a truncated sequence, controls below the font, 255..1,000
  characters around the 256-character chunks, a chunk ending inside multi-byte characters), 182 extents (each text in
  each font, again after `SetFont`), 6 atlases (1 or 2 textures; a cell ending exactly at 256 occurs), 234 unclipped
  and 1,872 clipped draws (containing, exactly the text's cell, cut on every side, half, outside, one point,
  fractional, inverted) and 60 alpha surfaces (width and height 0, 1, odd pads). Each case's log is in
  `golden_font.txt` as its line count and FNV-1a 64 digest under a header carrying its inputs (2,387 cases, 236 KB);
  `golden_font.rs` replays every case through `font.rs` with the same fakes. The driver relinked over the forwarders
  printed the same file byte for byte (and the same 3.9 MB full log). Mutations checked: `>` for the row test's `>=`,
  `<` for the containment's `<=` and a chunk of 255 each fail the test.
- **What only a tour shows.** Every menu, tooltip, chat line and in-game label is measured through
  `ComputeTextExtent`; the bitmap font draws the status bar (`EEUIFMGStatusBar`), the fonts `UIControlsTable` gives
  a bitmap and any clipped draw of a text whose font has one. The goldens use fake fonts; the real `PlatFont` is
  unchanged. The lead's tour must compare the menu captures pixel for pixel against the build before this branch.

## R5b.4, sixth round: Attack Ground, Patrol, Gather and Repair in Rust over host calls, the dry host's sweep, the store's next fields (ee_units group.rs; branch rust-r5b4-core, 2026-11-11)

- **The store's next fields** (each a write-through holder or a class field reported with `mUnitClass`, compared by
  USCK at every tick and before every executed message; no difference over the 24 capture games): `EEComplexUnit::mIsCommandable` (the doppelganger's load
  reads it through `EEUnitStoreRead`), `mFormationSlot`, the class's `mCanAttackGround`, and `EEUnit::mResourceAmount`.
- **The bodies** (`ee_units` group.rs over host calls in EEUnitHost.cpp, in the original's order and draw count):
  `EEMAttackGround::Execute` (`attack_ground`: the draw before the goal, the drawn unit acknowledging by id; a group
  whose ground attackers all left stays pending with no goal), `EEMPatrol::Execute` (`patrol`: the target by id as
  `EEUnitCast<EEComplexUnit>(gWorld.GetUnit(id))` over the store, i.e. a complex unit whose record's owner is the id's
  top byte; target -1 asks the host for the nearest town's route point; a selected path point adds the target's tile;
  the route read once before the loop, the goal made before the group's goal is cleared), `EEMGatherResource::Execute`
  (`gather`: the target's resource amount from the store, the queued flag set before the spread, the goal queued as the
  message says) and `EEMRepairUnit::Execute` (`repair`: the spread around the target's tile from the store; queued,
  build targets; else every unit orders the repair, then all are added back). KeepGroundAttackers' test reads the store
  (`ee_units_keeps_ground_attacker`); KeepMobileUnits, the spreads, the repair orders stay host calls.
- **The holders found their writers again:** `mResourceAmount` is read by the unit load (`EEUnitStoreRead`) and passed to
  four `sprintf`s (the deleted copy constructor refuses a holder as a variadic argument on Windows too: `.Get()`).
- **A selected unit out of its list.** A body reads the first selected unit's class type from the store; a unit that
  left its list (dead, not yet validated out of the selection) has no record, so the type is then read on the object
  through the host (`object_unit_type`). The dry host does not log that read.
- **The dry host and the USGC sweep** (TEST ONLY; ee/EEUnitHost.h, EEGroupCommandSweep.cpp). No capture game sends a
  group command, and a body cannot run in the live world in a sweep (goals, the goals' counter, groups, draws,
  effects). So every host call has a dry branch (a thread-local state set by `EEUnitHostDryBegin`): groups are unit
  lists, Fill takes the units EEFillMembers takes (the store; neither the selection nor the group is sorted: the sort is
  logged), KeepMobileUnits runs the original test on the objects, KeepGroundAttackers the store's test checked against
  the object's flag, a goal is a record of its arguments, a draw walks its range without touching the generator, the
  reads (map height, nearest town's point, a route) are real and logged, the rest is logged. Every five game minutes,
  for every player, its list's complex units (in order, reversed, none, and after its first path point) with a set
  of arguments (points at the first unit's tile and the map's centre; targets -1, its first unit, the next player's,
  its path point, an id nobody has) run the Rust body (the FFI entry the game calls) and then the original's
  statements copied from EEMessage.cpp (each world call through the same host calls, each unit read on the object:
  `gWorld.GetUnit`, `EEUnitCast`, `mUnitClass`). The two logs must be equal (else EE-SIM exits 3); USGC holds the
  Rust log. The hull reads of FindNearestTown run inside an `EEGeomCaptureScope`, so HULL/AREA keep their goldens.
  **What it covers:** the decisions over real units (the targets, the store against the objects, the branches, which
  units the keeps leave, the draws' ranges and which unit acknowledges); **not** the world calls themselves (Fill's
  sort, Plan, SetGoal, the spreads, the markers, the acknowledgement's sound): only a played game runs those.
- **A trap:** a point off the map's diamond has no terrain row (`GeTerrainView::point` reads before a row's start):
  the sweep's first map corner (0.5, 0.5) crashed in `mesh_z`. The UI never sends one; the sweep uses on-map points.
- **The USGC goldens** were recorded with the Attack Ground and Patrol build (276-1,080 records a game), then again with
  the Gather and Repair cases (780-2,340); the first two bodies' records alone still give the first goldens (oracle
  seed 2: 810 records, the same digest), so adding cases changed nothing in them.
- **Disk:** every sim_oracle/sim_capture run leaves a staged data copy per seed under `build/sim/stage/<kind>-<NAME>-<seed>`
  that nothing deletes (about 400 MB each): C: reached 0 bytes during this round. The scratch gate scripts now delete
  each run's slots with its captures.
- **What only a played game runs** (no capture game sends these messages): the four bodies' world calls (Fill's sorts,
  KeepMobileUnits on a real group, the spreads, SetGoal, Plan, the command markers, the acknowledgement sounds, the
  repair orders). The fixture commands that would reach them, on a box-selected group: right-click on a tree or mine
  with citizens selected (EEMGatherResource), right-click on a damaged own building with citizens (EEMRepairUnit;
  Shift for the queued form), the Patrol button then a point or own unit (EEMPatrol), the Attack Ground button with a
  siege unit then a point (EEMAttackGround). Not written into the fixture: each needs its own outcome check (which goal
  each unit got), more than a small change.
- **Gates** (each body commit built 0/0 for EE-AOC and EE-SIM, the oracle identical on seeds 2-7, `rust_build.py gate`
  PASS, `sim_capture.py` digests of the four profiles equal to the goldens but USGC, which the next commit recorded):
  see the final report for the merged numbers. Not run: any harness session, macOS or Linux builds.

## R6: the controls' drawing: borders, clipped text, labels, list boxes, check boxes and the edit box (ee_ui draw.rs, edit_draw.rs; branch rust-r6-ui6, 2026-11-11)

Moved to `rust/crates/ee_ui` as plans the C++ executes (it keeps every draw call, the line, rhombus, bitmap and
text objects and their order; the classes keep their members, vtables and exported names, no header gained a
member): `UIControl::DrawBorder`'s lines (`border_line`: side by side, row by row, which of the two border colours);
`UIForm::DrawClippedText`'s pieces around the form's clip rectangles, the copied popup or the IME candidate window
(`draw_clipped_text`, the one sequence run in Rust: its draws go back through a function the C++ passes); a
`UITextLabel`'s one-line placement, its copied line range and each wrapped line's place and clip rectangle;
`UIListBox<T>`'s copied line range, selection rectangle and focus lines (`UIListBox.h`), `UILBString`'s rows; the
check box's and radio button's scaled bitmap (`draw.rs`); `UIEditBox::CopyData`'s copied selection and IME
composition (single- and multi-line), `DrawPolygons`' selection spans, `GetSelectedTextRectangle`'s row,
`DrawTextA`'s row and the IME underline loop (measures and draws through a function table, `edit_draw.rs`).

- **What stays C++, on purpose.** The pure call sequences: `UIButton`/`UIBitmap`/`UISlider`/`UIScrollBar`/
  `UIComboBox`/`UITriStateCheckBox`'s `DrawPolygons`/`DrawTextA` (each the base, then its parts' draws, no decision
  beyond a null test), `UIForm::DrawPolygons`/`DrawTextA` (the draw list, the popup, the tool tip's shadow, whose
  rectangle was already Rust) and `UIManager`'s; `UIControl::CopyData` and the parts' `CopyData` copies (member
  copies). They are in the golden trace all the same (the `form` and `button` cases), so a later move has its pin.
- **The golden driver** (`build/ui_draw_golden`, scratch, `build.py orig|cur [seeds]`): the UI units and `include/`
  of 863e464c extracted with `git archive` (`orig`) or the worktree's (`cur`), compiled as the game compiles them
  (`clang_build.compile_to`, `EE_INCLUDE` pointing at the tree's headers), with the real `GELine`, `GERhombus`,
  `GERectangle`, `UString`, `UWideString` and a scratch Rust staticlib (ee_ui, ee_ge, ee_plat_core), under
  `/FORCE:UNRESOLVED`. Controls are calloc'ed with their class's vtable (`??_7UITextLabel@@6B@` through an asm
  label) and their members set under `#define private public`; the scroll bars are real (`UIScrollBar`, `UISlider`,
  two `UIScrollButton`s), so their `CopyData` and draws run. The rasterizer is a calloc'ed block whose vptr is a
  table of stubs (the `Draw` overloads at the slots the header's comments give: text 15, rhombus 17, line 20, bitmap
  21; any other slot logs and exits); its vertex arrays are a real `GEVertexArray` subclass, so each draw logs what
  the line or rhombus really holds (ends, per-vertex colour and opacity). Text is measured by a fake
  `GEText::ComputeTextExtent` (a hash advance per code point times the font's height / 8 + 1). Cases: 200 borders
  (thickness 0..5, border types 0, 1, 7), 400 clipped texts (0..4 clip rectangles, a popup that is or is not the
  drawing control, the candidate window, a failing draw), 200 one-line and 200 multi-line labels (every alignment,
  fitted or not, with and without a scroll bar at any value, empty, UTF-8 and emoji lines), 200 string list boxes
  (selection inside, outside and absent, focus on and off, a line height whose row product wraps), 200 buttons,
  check boxes and radio buttons (scaled and unscaled bitmaps, labels, copied texts), 100 forms (draw lists, popups,
  tool tips with the shadow), 400 edit boxes (single- and multi-line, password, caret blinking, scrolled away,
  selections both ways across and past the copied lines, IME compositions with random clause attributes, lines with
  breaks). `golden_draw.txt` keeps each case's inputs and its log as a line count and an FNV-1a 64 digest (730 KB;
  the full log is 1.7 MB). `draw/tests.rs` and `edit_draw/tests.rs` rebuild every case's whole log (the C++ glue
  included: the background rhombus, the scroll thumb, the cursor line, the measures) over the Rust; the driver
  relinked over the forwarders printed the same log byte for byte, and on seeds 2 and 3 at ten times the size (17 MB
  each). Mutations checked: the label margin, the IME attribute test, the clipped selection end each fail a replay.
- **Driver traps, new.**
  - `UIForm::DrawPolygons`/`DrawTextA` are protected virtuals: under the override a qualified call references the
    public name and lands at address 0. Call them by their decorated names through an asm label
    (`?DrawPolygons@UIForm@@MEAAJXZ`). The same for `GERasterizer::sActiveRasterizer` (protected static,
    `@@1PEAV1@EA`) and the strings' `sDefaultAllocationLength` (the driver's TU references the public name
    `@@2KA`): define both names.
  - `GERScreenState`'s inline constructor reaches members with out-of-line constructors: calloc it.
  - `GEVertexArray`'s constructor and destructor are out of line (GEVertexArray.cpp): define them for the fake.
  - A random world must keep the box's invariants or it measures the original's undefined behaviour: a list box
    showing more lines than its rows has a scroll bar; a caret or anchor never sits after a line break; an IME
    composition stays on the copied lines (one reaching past them reads past `mDisplayLines` in `DrawPolygons`, the
    original's hazard, unreachable while the box scrolls to the composition).
  - Disk: C: ran full mid-session (another agent's runs); an `Edit` failed with ENOSPC and left the file unchanged.
- **Kept quirks.** A border line's end is `(right + 1) - i`; a check box's scale uses `2 * thickness` as an unsigned
  product; a label's wrapped lines are centred on the control's rectangle but start from the copied text rectangle,
  and move left by half the scroll bar's width whenever the label has one (shown or not); a string list box's row
  is one pixel wider than its text rectangle; a selection rectangle that misses the text rectangle is drawn
  unclipped (`And` leaves it); the multi-line edit box's measuring text keeps the last text the caret was measured
  with; an IME composition's end is not clipped to the copied lines.
- **Gates run:** after each of the two commits `build.py -j 4 --sim` 0/0 (EE-AOC, EE-SIM), the sim oracle (seeds
  2-7, the main checkout's open tree) identical to `ref`, `rust_build.py gate` PASS. No tour, screenshot, LAN game
  or POSIX build was run.
- **Only a tour exercises these** (EE-SIM draws no UI), both commits change what is drawn on every frame of every
  menu: the borders of every control with one, every text of every control (each goes through `DrawClippedText`),
  every label, list box (the map and civilization lists, the saved games), check box and radio button, the edit
  boxes (the player name, chat, the lobby), the IME underlines only with an IME. The lead's tour must compare the
  menu captures pixel for pixel against the build before this branch.

## R6: WONGUI's layout arithmetic, scrolling, input box text model, list lines, focus and button rules in Rust (ee_won guilayout.rs, guiscroll.rs, guiedit.rs, guilist.rs, guifocus.rs, guiwrap.rs; branch rust-r6-won8, 2026-11-11)

Round 8 of R6: the WON GUI toolkit (WONGUI, 39.2k lines; its map by area, size, lobby use and dependency is in
RUST_PLAN's "R6 WON, the rest" row). The C++ classes keep their objects, child lists, vtables, virtual calls,
fonts, events and drawing; Rust answers the numbers and decisions they made. Each piece was captured first: a
driver in `tests/won` (in `xbuild.py` `WON_TESTS`) runs the original's statements, copied from the C++ before the
port with the components replaced by the numbers and flags they read and their `SetPos` / `SetSize` /
`SetVisible` calls by the values they received, next to the Rust ABI over generated inputs (a fixed LCG; small,
edge and wrapping ints).

- **Layout (`guilayout.rs`).** `WONRectangle`'s intersection, union (an empty rectangle becomes the other) and the
  asymmetric overlap test; `Component::SetSize`'s clamp and no-change test and `SetSizeAtLeast`'s grow test;
  `ChildLayoutInstruction::Execute` (the 14 flags from the lowest bit up, each seeing the earlier ones: GrowToRight
  after Right measures from the new x); the centre, same-size, clear-right, space-evenly (the leftover pixels one
  each to the first gaps), no-overlap (clamped one way for the parent and the other for a sibling), size-at-least,
  percentage (`SetBorders`' anchor choice as indices, `DoCalc`) and vertical-flow layouts; the LAYOUT rule's
  instruction names (the C++ still compares each with `GUIString::CompareNoCase`, in the original's order) and
  which offsets a chained or repeated rule zeroes. `won_guilayout_test`: 2,914,011 checks, 0 failures. GridLayout
  stays C++ (nothing uses it; its `Execute()` overrides no layout slot).
- **Scrolling (`guiscroll.rs`).** `Scrollbar::LayoutThumb` (the thumb's share of the track, the position's share of
  the free track, a thumb pushed back inside always moved), `MouseDrag`'s thumb and position,
  `HandleComponentEvent`'s line and page steps and clamp; `ScrollArea`'s offsets clamp, `OnBottom`, the view inside
  the padding and the bottom pin, its desired and fitting sizes; `ScrollbarScroller`'s conditions (a visible
  conditional bar is left alone while conditions are applied) and the bars' totals; `ButtonScroller`'s button
  conditions and which buttons can still scroll. `won_guiscroll_test`: 18,889,559 checks, 0 failures. A zero total
  with a negative page divides by zero in both (the original crashed; so does the port).
- **The input box (`guiedit.rs`).** `MoveCursor` (found: the word move left stops just after the space before the
  word it crossed, "ab  cd ef" from the end goes to 6, not 7; kept), `SelectRange`, the Home / End / Left / Right
  selection (Home and End measure the real text, Left and Right walk the shown one, as before), what Backspace and
  Delete keep (as the `assign` calls the original made), `SetSelText`'s cut at the first unit below 32 (the
  original's `int` of a `wchar_t`) and its limit, the cursor's blink and blink rate, `CheckSel`'s character under
  the mouse and the selection it extends, `AdjustPosition`'s scroll and `TimerEvent`'s drag scroll. The font stays
  C++: the box measures the cursor's prefix and the characters the walk can reach (`GetCharacterWidth` for each, a
  few more calls than the original made, all without side effects but the font's glyph cache) and hands the widths
  over. The history, `NumericFilter` (the CRT's `isdigit`) and the clipboard stay C++. `won_guiedit_test`:
  9,437,056 checks, 0 failures.
- **The list (`guilist.rs`).** `FindLineAt` is the STL's `lower_bound` walk exactly (the halving loop both
  libraries use), reading the items' tops through a callback, so a batch insert's unsorted tops find what they
  found; `FindItemAt`'s test, `IsItemVisible` (partial lines with 0x80), `EnsureVisible`, `EnforceInvariants`'
  stacking (the heights read first, then each item's virtual `SetPos`: none of the overrides changes a height),
  the full-line scroll's extra height, `FindSelectableItem`, `SelectRange`'s bounds, the keys' target line and
  scroll, the scrolls an insert, a refresh and a delete leave, a batch insert's provisional top, a refresh's new
  place, `DeleteRange`'s bounds and the item tool tip's test. `won_guilist_test`: 22,613,891 checks, 0 failures.
  - **Found: two original faults.** With the wrap flag (0x800, the popup menus') and neither end item
    selectable, `FindSelectableItem` called itself without end (a stack overflow); the port answers no item
    (behaviour change: a crash becomes no selection). With the wrap flag, Left and Right's "no line" (-1) wrapped
    to the end like any line above the first, so they moved the selection too; kept.
- **Focus and buttons (`guifocus.rs`).** `WantFocus`, `FindChildAt`'s verdict on the topmost child hit (disabled
  or blocked by a modal state: no child at all), `GetNextFocusId` (a byte that skips 0),
  `SetFocusToLastFocusChild`'s pick (found: the byte difference is compared as an `int` and kept as a byte, so
  an id newer than the current one wins until a smaller byte replaces it, and the current id itself, when not
  skipped, gives 0, which counts as no pick: the next candidate is taken; kept), `TabFocus`'s start and its
  loop's bound; `Button`'s release (held, dragged off, repeat, activate on press), drag, keys (Enter on the default
  button, Space, the radio arrows) and repeat timer, and `WONButton::GetButtonState`'s state and its fallback
  cells. The C++ keeps the child lists, the virtual `TabFocus` / `HitTest` / `RequestFocus` calls, the radio
  groups, sounds, timers and events. `won_guifocus_test`: 905,952 checks, 0 failures.
- **The word wrap (`guiwrap.rs`).** `WrappedText::CalcWrap`'s loop (the breaks at the last space that fits, inside a
  word when none does, at CR, LF and CR LF; the text's width; the unit after a final CR is the terminator, as the
  original read it) and `SetMaxWidth`'s rewrap rule. The C++ measures every unit once with `GetCharacterWidth`
  (the original measured the units it visited, some twice after a backtrack: the same numbers, a different call
  count) and keeps the break list (`BreakInfo` still narrows to `unsigned short`), the height and the paint. A
  unit breaks at most twice (inside a word, then alone when it is wider than the line); the C++ hands room for
  `2 * len + 1` and asks again with more if the count says so. `won_guiwrap_test`: 3,559,089 checks, 0 failures.
  Its first oracle run failed when drive C: filled up (EE-SIM's per-run stage slots under `build/sim/stage`, now
  deleted by `tools/sim.py` when a game exits); it was committed after a clean rerun (seeds 2-7 identical).
- **What exercises what.** Every lobby load lays out the 63 .cfg files' components (the layouts, `SetSize`,
  the rectangles, the wrap of every label and button text). The LAN game (`lan_game.py`) adds: the clicks
  (`wonclick`: the hit verdict, `Button`'s press and release), typing (`wonkey`: `SetSelText`'s cut and limit, the
  caret's blink and scroll), the game and player lists and the chat (the list's lines, stacking, inserts,
  deletes and scrolls, the scrollbars' thumbs and the scroll areas' clamps), the button states at every paint.
  Only a person at the lobby reaches: the input box's mouse selection, drag scroll and arrow / Home / End keys,
  Backspace and Delete, the list's keys, wrap and tool tips, Tab focus, the last-focus pick, the radio arrows,
  Space, Enter and the button repeat, the scrollbar drag and its arrows and track. Every move here changes layout
  or drawing: a lobby run checks them. Nothing here touches the simulation: the oracle was identical after each
  gated build (six code commits, each built, oracle-checked and Rust-gated on its own).
- **Still C++ of WONGUI:** the component objects and their event dispatch (Window, WindowManager, RootContainer,
  the popups and modality), TextArea and its segments (the chat's rich text wrap), ComboBox, TabCtrl, Splitter,
  MultiListArea and its header, TreeItem, the menus and dialogs' rules, the config readers' expression parser and
  value readers, and the drawing (Surface, StretchImage's nine slices, the decoders, the platform backend).

## R6: WONGUI's text area, compound controls, multi-column lists, config expressions, popups and modality, and the lobby dialogs' decisions in Rust (ee_won guitext.rs, guictl.rs, guimulti.rs, guicfg.rs, guiwin.rs, dialogs.rs; branch rust-r6-won9, 2026-11-11)

Round 9 of R6, after round 8's map of WONGUI (RUST_PLAN "R6 WON, the rest"). Same pattern: a `tests/won` driver (in
`xbuild.py` `WON_TESTS`) runs the original's statements, copied from the C++ before the port with the components
replaced by the numbers and flags they read, their `SetPos` / `SetSize` / `SetEnabled` calls by the values they
received, against the Rust ABI over generated inputs (a fixed LCG; small, edge and wrapping ints). The C++ classes keep
their objects, child lists, vtables, events, fonts and the paint; Rust answers the numbers and decisions. No C++ class
moved wholly, so no unit was deleted.

- **The text area (`guitext.rs`, the chat, MOTD and help pages).** `AddSeg`'s line metrics (height, baseline,
  descent, the total height and whether the line's segments move), `AlignLine`, `AddSegmentPrv`'s fit test,
  `TextSegment::Wrap`'s loop (steps: a piece on the current line or a new line; the C++ measures every unit once and
  the rest of the text through a callback, as `GetStringWidth` may kern), `ShouldScrollOnBottom`, the delayed wrap
  (over 5000 characters, due after 2 to 10 ticks), `EnforceMaxChars` / `EnforceMaxLines`' trim points,
  `RemoveSegmentsUpTo`'s line search (through a flags callback), `EnforceInvariants`' renumbering, `FindSegment`'s
  clamp, cache and pick (a point below the line returns NULL and keeps the cache, as before), `CharAt` (a width
  callback asked only for the units the walk reaches), `CheckSel`, the drag and key scrolls, `GetSelText`'s parts,
  `GotoAnchor`'s clamp (a negative line compared as unsigned is past the end: the last line), `AddFormatedText`'s
  split and `LobbyAppendFormattedText`'s commands (`<n> <cN> <fN> <sN> <lN> <<`, executed by the C++ as steps; the
  NULL entries stay its check). `FindLineAt` and `IsLineVisible` are guilist's (the same statements).
  `won_guitext_test`: 8,400,570 checks, 0 failures.
- **Compound controls (`guictl.rs`).** ComboBox: the list box's size, the input box's selection rule, the keys, the
  item searches. TabBar / TabCtrl: a tab's x, the neighbour of a disabled tab, `KeepTabButtonInView`,
  `AdjustTabBarScroll` and the arrows' states, the page, page area, arrows' and bars' places (`VertPositionTabBar`
  reads the container's height after its `SetSize`, which clamps), the first tab's bar height, the arrow keys'
  target and the tab after a removal. Splitter: the maximised split, `SetSplitterPos`' clamp and edges, the
  remembered percentages, the layout (`Width() * percent / 100 + offset` in doubles, to an int as `cvttsd2si`:
  NaN and out of range give `INT_MIN`; the original negated the bar's size before halving it, `(-w)/2`, which
  differs from `-(w/2)` only at `INT_MIN`: the driver found it), panes, bars, cursor, drag and the maximise buttons.
  `won_guictl_test`: 11,578,060 checks, 0 failures. Found in the driver, not the game: with no pages,
  `TabCtrl::KeyDown`'s bound is the largest `size_t` and the walk runs to the int's wrap (both the original and the
  port); the control always has a page when a tab is selected.
- **Multi-column lists (`guimulti.rs`).** `GetColumnAt`, `SetColumnWidth` (a paired column takes from its partner,
  any other grows the total up to a positive maximum; every column's x), the dynamic column after a resize, the
  auto-grow test, the partner links; the header's buttons over the columns (a header column past the list's
  columns without a button moves the next ones one pixel left: the original added its -1), the sort a press asks
  for, the shown sort, `GetDragComponent`'s resize edge and the dragged width; TreeItem's +/- box and a child's
  indent and place. `won_guimulti_test`: 3,900,000 checks, 0 failures.
- **Config expressions (`guicfg.rs`).** `ComponentConfig::ParseExpression`'s recursive descent reads the
  StringParser and `GetIntValue` through callbacks; `GetIntValue` can throw (a missing named value, nested
  expressions in function calls), so its callback catches into a `std::exception_ptr` and the C++ wrapper rethrows
  after the Rust returns: no exception crosses a Rust frame. `GetIntValue`'s name scan and kinds (`@ID_` needs
  upper case: the original's `toupper` took the comparison's result) are Rust too. Behaviour change: `INT_MIN / -1`,
  a divide fault in the original, gives `INT_MIN`. `won_guicfg_test`: 300,271 checks (the exceptions' texts and the
  parser's position compared too), 0 failures.
- **Popups and modality (`guiwin.rs`).** `AdjustPopupPosition` as its `SetPos` sequence, `DoPopup`'s mouse default,
  own window and joining rules, `FindMouseWindow`'s topmost popup window (it now reads every popup window's
  rectangle; the original stopped at the topmost hit; `GetRect` only reads), `CheckPopupClick`'s button test,
  `CalcModality`'s decisions, `EndDialog`'s target; PopupMenu's submenu delay, first release, keyboard mode's end,
  `MenuItem::CalcDimensions` and the submenu's place. `won_guiwin_test`: 8,000,000 checks, 0 failures.
- **The lobby dialogs (`dialogs.rs`).** CreateGameDialog's and CreateRoomDialog's create button, the skill clamp,
  the Internet-only radios, LanPortDialog's port rule, AddFriendDialog's OK, the network adapters' order and
  chosen address, StatusDialog's message width, padding, widening and its first 50 ms (on the 32-bit tick's wrap on
  every platform; LP64's `unsigned long` start differed only at the 49-day wrap). `won_dialogs_test`: 12,000,000
  checks, 0 failures. RoutingLogic's flow stays C++: it runs only against the Internet servers, which are gone, so
  nothing can exercise a port of it (its decisions are round 7's `routing.rs`).
- **What exercises what.** Every lobby load: the config expressions (every number in the 63 .cfg files), the text
  area's layout and format commands (the MOTD and help pages, the chat's lines), the tab controls', splitters' and
  multi-column lists' layout. The LAN game (`lan_game.py`, `wonclick` / `wonselect` / `wonkey`) adds: the chat's
  appends, wrap, trims and bottom scroll, the game and player lists' columns and header, the create-game dialog's
  button and skill, popups and modality when the dialogs open, the combo boxes when one is clicked. Only a person
  reaches: the text area's mouse selection, links, drag scroll, keys and copy; the combo box's keys; the tab arrow
  keys and scroll arrows; the splitter's drag and maximise buttons; the header's resize drag and sort clicks; the
  popup menus (right-click); the LAN port, add-friend and network-adapter dialogs; the status dialog's sizing.
  Every commit here changes layout or drawing inputs: a lobby run checks them. Nothing touches the simulation: the
  oracle was identical after each gated build (six code commits, each built, oracle-checked and Rust-gated on its
  own).
- **Script edits and blanks.** The C++ edits were made by scripts matching the original statements exactly; where a
  file's statements carried trailing blanks the scripts strip the file's trailing blanks first, so some diffs (e.g.
  TextSegments.cpp, ComboBox.cpp, TabCtrl.cpp, Splitter.cpp, WindowManager.cpp) also hold whitespace-only lines.
- **Still C++ of WONGUI:** the component objects and their event dispatch (Window, WindowManager's flow,
  RootContainer, the popups' windows), TreeArea, the menus' key handling (`TryKeyDown` recurses through the
  submenus), the per-class config readers, and the drawing (Surface, StretchImage's nine slices, the decoders, the
  platform backend, the text area's and menus' paint).

## R5b.4, seventh round: the fixture's orders, Add Patrol Point, Attack Unit, Heal Unit and Path To in Rust over host calls (ee_units group.rs; EETestFixtureOrders.cpp; branch rust-r5b4-core, 2026-11-11)

- **The orders exercise** (TEST ONLY; EETestFixtureOrders.cpp, fixture_driver.orders, `fixture_game.py --exercise
  random-game --play-seconds N --orders`, also posix_game.py). "<seq> order <form> <kind>" picks a group of the local
  player's units, selects it with `EEMSelectTheseUnits` (as the unit icon bar sends it: a UI action), and gives the
  order through the platform's events (ISHandleEvent: the pointer set under the mouse's input lock, the button press
  and release, Shift as a key event): gather (citizens, a right click on a world-player resource in view that a
  citizen can gather), repair and repair queued (citizens, a right click, with Shift for the queued form, on one of
  the player's built land buildings in view whose hit points the fixture first halves under the world's write lock:
  no game message damages a building), patrol (the Patrol status button, button object 18), attack ground (the
  Attack Ground button, object 6, then a left click on a seen point beyond the first unit's minimum range) and move
  (a right click on open ground). The click points come from the game's own picker (`PickUnitUnreferenced`, the hit
  test the click itself uses) over a 12-pixel grid of the game view, under the world's read lock.
  - **The verdict** comes from a hook in `EEPhysics::DrainInput` (`gEEMessageExecutedTestHook`, NULL unless the
    fixture runs, so EE-SIM and the gates do not see it) called before and after each input executes, on the physics
    thread under the world's write lock: before, the sender's active group and each unit's current goal; after, every
    unit judged by the original's rule (EEMessage.cpp's body, Fill, the keeps, the spreads and Plan, read on the
    objects, not through the store): a Gather goal on the target (or a Location goal at its tile when the continents
    say it cannot reach it), the target first in the build queue (queued: in it), a Patrol goal on the nearest town's
    route (or a Location goal for a class that cannot patrol; every goal unchanged when the town has no route), an
    Attack Ground goal at the message's point, a Location goal after a move. Units the rule does not judge here (the
    spread's own groups of non-citizens, transports in containers) are "skip".
  - **What the reconstruction's UI really sends.** The Patrol button (`EEUIFMGStatusButton::Command` 0x12) sends
    `EEMPatrol(-1)` at once: no point is clicked; the patrol of the nearest town's route. `EEMAddPatrolPoint` has no
    sender in the reconstruction (only its import constructor): the original's patrol-point mode is not there.
    `StartPatrol` (buttons 0x1b, 0x22: heal_mech, heal) enters UI state 0x10, whose mouse test is `EEModeHeal`: the
    name is wrong (it starts the heal mode). Attack Unit needs a seen enemy, Heal Unit a healer: a random map's start
    has neither, so the fixture does not reach them; Attack Ground needs a unit whose class attacks the ground (none
    at the start: the step is "skipped (no-unit)").
  - Not run here (no harness sessions). The self test's stand-in speaks the protocol; the lead's run is the only
    evidence that the orders work in the game.
- **Add Patrol Point** (`EEMAddPatrolPoint::Execute`) is `group::add_patrol_point` over host calls (six new ones:
  KeepNonCitizens, the lone patrol group, the path point's insert, the first unit's patrol goal's route, a route's
  AddPoint, a new patrol goal's point and registration). `EEActiveGroup::KeepNonCitizens`' test reads the store
  (`ee_units_keeps_non_citizen`: commandable, hit points, category; its only caller is this body).
- **Attack Unit and Heal Unit** (`EEMAttackUnit::Execute`, `EEMHealUnit::Execute`) are `group::attack_unit` and
  `group::heal_unit` (two new host calls: SpreadAroundUnit, a unit's complex unit as an id). Attack Unit's goal takes
  the selection's first unit as it is when the goal is made (read again there, after the slots' Fills), as the
  original did. **Behaviour change:** a heal target with no complex unit is refused before any group; the original built the
  goal over NULL and its Plan dereferenced it (a crash). The UI sends only complex targets.
- **Path To** (`EEMPathTo::Execute`) is `group::path_to`. Its statements before the loop (a point off the map to the
  closest tile's centre, a tile under a building the first unit cannot pass to the building's nearest footprint tile,
  the final tile's height) moved unchanged into one host call (`ee_units_host_path_to_point`, a read); the message
  keeps the adjusted point (the FFI writes it back, as the original wrote `mX`/`mY`). Its goal is a new host
  constructor (`EE_GOAL_CTOR_LOCATION`: the spec gained `value`, the facing, and `flag_c`, the group movement flag;
  the dry log adds those two words for this constructor only, so the other bodies' records keep their digests). The
  acknowledgement comes before the move marker, which shows for every group that took units, as in the original.
- **The sweep** (USGC) gained the three bodies' original statements and cases (Add Patrol Point at the two Attack
  Ground points; Attack Unit at the next player's unit, the player's own, the resource and no unit, plain and queued,
  the first as a conversion; Heal Unit at the player's own first unit, the next player's and no unit; Path To at the
  two Attack Ground points with no option and with all four). The goldens'
  USGC lines were recorded again with each body (the new records); every other digest stayed equal to its golden.
- **A trap:** Git Bash's `sed -i` rewrote a CRLF file (EEGroupCommandSweep.cpp) with LF line ends, silently: edit
  CRLF files through a script that reads and writes the bytes (`file` shows the line ends).
- **Gates** (each body commit built 0/0 for EE-AOC and EE-SIM, the oracle identical on seeds 2-7, the Rust gate PASS,
  every capture digest of the four profiles equal to its golden but USGC, whose lines the next commit recorded; the
  live Rust-against-original comparison inside the sweep never stopped a game): see the final report. Not run: any harness session, macOS or Linux builds.

## R4.4: the Graphics Thread mapped, the camera's math in Rust (ee_gfx camera.rs; branch rust-gfx7, 2026-11-11)

RUST_PLAN's new row R4.4 holds the map of the Graphics Thread (what each part decides, which GE/GX calls it makes,
what it reads). The first leaf is the camera's math (`EECameraControl.cpp`), in a new crate `ee_gfx` over `ee_ge`'s
math. The class keeps its members, the key list, the followed units (handles), `MoveTo`, `KeepPose`, `Revolve`,
`Rotate`, the setters and every viewport and world call; Rust gets plain values (the GE classes as their
`#[repr(C)]` mirrors) and gives back values the C++ stores.

- **What moved.** `GetPitch`; `GetOffset`/`GetLocation`; `GoTo`'s speed and its two snaps (the followed unit's
  speed times the game speed is read in C++ only when following, as before); `ClampHeight`; `BuildView`;
  `SetClippingDistances` with `EECameraRayDistance`; `ClipToEdges`; `GetGroundPoint`; `Scroll`'s ground direction,
  its near test and its clamp to the map's edge planes; `Update`'s step of the move (wait out a key, the path's next
  key, a jump, an advance) and its orthographic view. The terrain is three C++ callbacks over the mesh
  (`GetIsValidTile`/`GetIsValidPoint` after `UMFloorUnsigned`, `GetMeshZ`), so the platform's `unsigned long`
  stays on the C++ side.
- **The camera is simulation code too.** `EEPhysics.cpp:756` calls `Update` every tick (EE-SIM included), which sets
  the world's camera transformation, its yaw and pitch, the viewport and the minimap's view. RUST_PLAN 2.1 applies:
  the C++'s `f32` order, `(float)pow((double)x, e)` and `(float)sqrt` through `f64`, the table trigonometry.
- **`pow` through `black_box`.** The golden caught Rust's `pow(x, 0.5)` giving `+NaN` where the C++ gave `-NaN`
  (mode 3 past zoom 1): LLVM rewrites a constant exponent of 0.5 as `fabs(sqrt(x))` unless fast-math is off for
  libcalls, which it is not for Rust. The exponent now goes through `std::hint::black_box`, so the C library's
  `pow` runs as the C++'s does.
- **`Scroll` reads the zoom before its mode switch.** The original computes the delta's scale (`mZoom + 0.5`), the
  pitch and the minimum height before `SetMode(0)`/`ResetZoom` (free camera outside a cinematic) reset the zoom to
  1; the first forwarder passed the zoom after it and the golden failed 28 scroll cases. The forwarder passes the
  scale taken first.
- **One change of shape, none of behaviour.** The original built each of `SetClippingDistances`' three view rays
  twice (`EECameraRayDistance` per limit); `EEMap::GetScreenLine` reads only the viewport, which changes after all
  six, so the forwarder builds each ray once and the Rust measures it twice.
- **A hazard found, not fixed: the free camera past zoom 1 crashes.** In mode 3 `GetPitch` is
  `pow(1 - |zoom|, 0.5)`: NaN past zoom 1, and the port's farther zoom-out (`gCameraMaxZoom` 2) allows that zoom.
  `SetOrientationYPR(NaN)` reaches the table cosine, which faults on a NaN angle (the C++ indexed at `INT_MIN`;
  `ee_ge::um::um_cos` panics). `EESMEffect.cpp:2096/2152` and `EEACenterCamera.cpp:79` switch to mode 3 with the
  current zoom, so a scenario's cinematic after a zoom past 1 likely crashes the game. Modes 0 and 1 clamp the zoom
  at 1 for the pitch; mode 3 should too (a one-line fix in `camera::pitch`, a behaviour change left to the lead).
- **The golden driver** (`build/camera_golden`, scratch, `build.py orig|cur [seed [scale [full]]]`): the camera unit
  of 9432f6e7 (`git archive`) or the worktree's, compiled with the exe module's flags, linked with the real GE math
  units and `GEViewport` (the lle module's flags) and a scratch Rust staticlib (ee_ge, ee_gfx, ee_plat_core) under
  `/FORCE:UNRESOLVED`. The fakes: `gWorld` is zeroed storage under its decorated name (`__asm__` label), the map a
  calloc'ed `EEMap` whose mesh (`#define private public` around `GETerrainMesh.h` only) has rectangle or diamond
  rows, the real edge planes (`ee_ge_terrain_planes`) and a hash height per half tile; `EEMap::GetScreenLine` a ray
  from the viewport's camera; `RTSMap::GetLocationOnLine` the hit at height 1; `EEWorld::SetCameraTransformation`
  records the camera and sets the viewport's; the unit handles resolve to none. 24 meshes of 600 cases: GetPitch,
  the interpolation, GoTo, ClampHeight, BuildView, SetClippingDistances, ClipToEdges and GetGroundPoint, Scroll
  (with the free camera's mode switch), Update (with and without a key path, orthographic or not) and 60 cases of
  the movers that stay C++ (MoveTo with locations, SetZoom, SetMode, Revolve, Rotate, SetLocation, SetOffset, the
  turns, KeepPose). Each line is the case's FNV-1a 64 over its whole log (inputs, every height query with its
  answer, the recorded camera, the outputs), 410 KB; `full` prints the logs. `golden_camera.rs` draws the same LCG
  stream, rebuilds the generator's camera, viewport (`ee_ge`'s `GeViewport`) and mesh, runs each case through
  `camera.rs` with the C++ glue rewritten in the test (KeepPose, GoTo, `MoveTo` without units for the mode switch,
  NextKey, ResetPath) and compares the digests: 12,984 cases; the 1,440 C++-only movers only advance the stream.
  The relinked driver printed the same file, and the same on seeds 2 and 3 at four times the size (57,696 lines
  each). A mutation (the height clamp's `height + minimum`) fails the replay at its first case.
- **Driver traps.** One random draw per statement: the order in which a call's arguments are evaluated is the
  compiler's, and `GE3DPoint(F(), F(), F())` is not a stream the Rust can follow. A `std::vector` of the mesh's rows
  destroys the old rows through `DeallocateInternalData` (out of line, unresolved): define it. The empty unit
  handles call `EEUnitHandleResolve`: define it. The camera's file statics (`gGameVersion`) need `UString` and
  `UWideString`. And never clean a scratch folder with `rm orig/g*.txt`: it took `golden_camera.txt` with it.
- **Only a tour exercises this** beyond the sim's ticks: EE-SIM runs `Update` every tick (no unit followed, no
  scroll), so the oracle covers the update's step, `BuildView` and the clipping distances on the perspective path;
  scrolling, zooming, rotating, following a unit, a cinematic's key path and the orthographic view run only in the
  windowed game.

## R6: the edit box's key dispatch, the background table, UI events, the full background's tile names and the status bar (ee_ui edit_keys.rs, background_table.rs, event.rs, status.rs; branch rust-r6-ui7, 2026-11-11)

Moved to `rust/crates/ee_ui` (the C++ classes keep every member, vtable and exported name; `UIBackgroundTable.h` and
`UIEvent.h` gained a `friend struct`; `UIEditBox.h` lost three private members): `UIEditBox::UpdateKeyboard`'s repeat
(the 500 ms delay, then 50 ms steps, the caret shown while a key is held), `ProcessInput`, `ProcessShiftInput` and
`ProcessControlInput` (deleted from the C++: which keys move, select, delete, cut, copy, paste, clear a line, scroll
or fire Enter), `Cut`/`Copy`/`Paste`, `AddIMEResultString`'s typing and `MouseMove`'s drag selection and auto-scroll
(`edit_keys.rs`); `UIBackgroundTable::Create`'s loop and record copy (`background_table.rs`);
`UIEvent::CreateControlEvents`/`CreateFormEvents`' loop and `Initialize`/`Uninitialize`'s count (`event.rs`);
`UIFullBackground::LoadBackground`'s tile file names (`background.rs`); `UIStatusBar`'s value clamp, fill width, move
rectangles and `CopyData`'s inset fill and centred text (`status.rs`).

- **Keys are host questions, asked as the C++ asked them.** `UIControl`'s predicates stay C++ (`DeleteKeyPressed`
  reads the keypad's Del only with Num Lock off, `CutKeyPressed` is Ctrl+X or Shift+Del, `ShiftKeyPressed` reads
  `JustPressed` then `GetValue`), so Rust asks `ee_uicpp_edit_key(k)` at each place the C++ called one, in its order,
  short-circuits included. The golden driver's key entities digest every read (`reads <n> <fnv>` per operation), so a
  reordered or extra question fails the replay. The four timers are `TMPoll`s behind `ee_uicpp_edit_poll`.
- **Two calls come back into the box.** `ProcessEnterKey` is virtual (the scenario editor's `EESETSAEditBox`
  overrides it) and an IME result's characters go through `UIManager::UpdateTranslatedKey` to the focused control's
  `UpdateTranslatedKey`: both run inside `Edit::call` (the record stored before, reloaded after). The driver checked
  the virtual stays virtual with a subclass whose vtable is `UIEditBox`'s with slot 35 replaced.
- **UTF-8 (D6):** every editing key's golden includes characters of one to four bytes; Backspace, Delete, the arrows
  and an IME result step whole code points (the model's `Mbcs`), and Rust's unit tests say so on `a€😀` and `日本語`.
- **Kept quirks:** Shift+Del without a selection deletes backwards (only the keypad's Del reaches it: the main Del
  with Shift is Cut, which returns first); Ctrl+A, Ctrl+L and Ctrl+Enter do not count as handled (no repeat);
  Shift+Right moves `mCharScroll` characters; a drag's line moves wrap their unsigned arithmetic as the C++ did.
  `Uninitialize` decrements a count already at 0 (it wraps); a background record whose read fails keeps the previous
  record's bytes (a second entry from it); a full background's name with no `.` in its file part but one in a
  directory takes the numbers before the directory's `.`; a status bar's value is clamped below the minimum first (a
  reversed range takes its minimum).
- **Original hazards, found by the drivers, left as they were (each reachable only as described):**
  - a multi-line box narrower than its widest character adds lines forever (known; the driver keeps boxes wide);
  - `SetText` or `SetText`-like edits during an IME composition leave the composition's start past the text, and the
    next typed key reads past `mLines` (the game's text input closes the composition first);
  - `AddIMEResultString` with no composition open reads the stale composition start (the IME sends a result only
    during a composition);
  - an edit during a mouse drag (Backspace deleting the anchor's line, or `SetText`) leaves the drag's anchor past
    the text, and the next `MouseMove` selects from it: **a person can reach this** by typing while dragging a
    selection;
  - Shift held with a deletion and a move in the same frame (Shift+keypad Del with Num Lock off, plus an arrow): the
    deletion leaves the anchor at the selection's far end, the move then selects from it, past the text when it was
    the end. **A person can reach this** too. Neither is fixed here (no behaviour change); a fix would reset the
    anchor after a deletion.
  - `UIBackgroundTable::Create` and the events' loops copy an uninitialised record when the first record's read
    fails (the Rust starts from zeros; the driver never fails the first record read);
  - `LoadBackground` built a name in 256 bytes and the no-extension numbers in 3: names over about 240 bytes or grids
    over 99 overflowed; the Rust name is whole (the only divergence, unreachable from the UI database).
- **Golden drivers, new traps.**
  - With `-DLOWLEVELENGINE_EXPORTS` every dllexport class's inline members are emitted in the driver's unit, and
    under `#define private public` those copies reference the statics by their public names (`@@2`), so a COMDAT
    from the driver can win and read address 0. Compile the driver with `-DEE_STATIC -DLLE_CURRENT_HEADERS
    -DLLE_EXE_HEADERS` (no dllexport) and reach private statics by their decorated names
    (`?sEnterKey@UIControl@@1PEAVISEntity@@EA`, `?sShiftKey@UIForm@@0...`, the controls array's `@@0V?$vector...`).
  - A driver-side subclass of a unit's class gets a vtable naming the base's protected virtuals by the public
    decoration (address 0), and the MS ABI emits a vftable only where a constructor runs; clang-cl crashed building a
    subclass constructor under `EE_STATIC`. Copy the real vtable (`??_7UIEditBox@@6B@`) into an array and replace one
    slot, checking it holds the member's address first.
  - `-Xclang -fdump-vtable-layouts` on a two-line unit with a derived class whose constructor is inline prints a
    class's slots (FSFileSpec's: Close 2, Open 12, `Read(int32_t&)` 22, `Read(uint32_t&)` 23,
    `Read(unsigned char*, ...)` 25); a fake file is then a block whose vptr is a table of stubs.
  - `GERhombus::SetRectangle` is inline and calls the private `ComputeRotation`: call it from a unit without the
    `private` define; `GERectangle::Contains` and `UILayoutUtility()` are out of line (GEBitmap.cpp,
    UITrampolines.cpp): define them in the driver.
  - A random world that holds a box's invariants: the frame's `CheckScrollBar` after every operation (CopyData calls
    it first), no `SetText` or press during a composition, no edit during a drag, a shown control needs entity groups
    (`SetRectangle`/`SetLocation` reach `GetEntityGroupById`): set `mDisplayMode` 0. `HeapValidate` after every
    operation (`UI_GOLDEN_HEAPCHECK=1`) turned a heap corruption at a later allocation into the operation that caused
    it.
  - The driver's clipboard is the process's: the replay carries it from one box to the next.
  - Run driver builds with their own `EE_ORIG_SRC_ROOT`: `clang_build.compile_to` stages a unit at the same path as
    the game's build, and a driver build during a game build races on it.
- **Goldens:** `golden_keys.txt` (60 boxes x 70 operations, 688 KB), `golden_bg.txt` (100 background files, 200 event
  files, 100 count sequences, 60 full backgrounds; 706 KB), `golden_status.txt` (100 bars x 25 operations, 527 KB).
  Each replays whole in Rust; each driver relinked over the forwarders printed the same file, and the same on three or
  four more seeds at ten times the size (the Rust replays also pass on those). Mutations checked: Shift+Right's
  `mCharScroll`, Shift+Del's backward deletion each fail the key replay.
- **Gates run:** after each commit `build.py -j 4 --sim` 0/0 (EE-AOC, EE-SIM), the sim oracle (seeds 2-7, the main
  checkout's open tree) identical to `ref`, `rust_build.py gate` PASS. No tour, screenshot, LAN game or POSIX build
  was run.
- **What a real game exercises (EE-SIM has no UI):** the key dispatch runs on every frame an edit box has the
  keyboard: the lobby's player-name and chat boxes, the save game name, the scenario editor's boxes, multiplayer chat
  in a game. The fixture's `wonkey` only types characters (`UpdateTranslatedKey`, already Rust before this round), so
  only a person reaches the keys: Backspace, Delete, the arrows, Home/End, Shift selections, Ctrl+X/C/V/A/L, Enter in
  a single-line box (the chat's send), a mouse drag selection, an IME. The background table and the form and control
  events load once at start-up (every menu's backgrounds and every button's event come from them); the full
  background's names load the scenario editor's 4 x 3 picture (`textures\Scenfull_<row>_<column>`); the status bar
  draws the post-game summary's bars.

## R5a.2, fifth round: claim ranges, behaviours, unit order, attack counts, repair timers, towns (save 0.265-0.268, branch rust-r5a-aisave4, 2026-11-11)

Continues "R5a.2, fourth round". Same test (`sim_oracle.py saveload NAME --seeds 2-7 --at 600,1350,2100`, 18 saves).
Each commit was built and gated on its own tree (build 0/0 for EE-AOC and EE-SIM, the plain oracle identical to ref
on seeds 2-7, the Rust gate, sim_capture's four profiles PASS) and measured with the full saveload run. All of these
are load and save code: they run in the real game's Load Game screen as in EE-SIM (no change to how a game plays).

- **0.265, the claim ranges and danger zones** (`EECPSaveResourceState`, `EESaveHasResourceState`). Seed 4's 2100 s
  iron gatherer: FindResourceTarget's first pass (economy.forage_patches) takes only the mines within the claim
  range, which EECPClaimExpand grows in steps kept in EECPTrace.cpp's statics. A loaded player started at the first
  range (25 where the saved game had 40) and took the near mine. The steps and their last time are now in
  gCPExperimentTimers, and the resource managers' mDangerZones (also read there) are saved with them; a load puts
  both in place at the first tick. The dumps' AI line shows `claim=` and `danger=`. Measured: seed 3 2100 s 57 -> 569,
  seed 4 600/2100 s 80 -> 173, 0 -> 52, seed 5 1350/2100 s 91 -> 106, 23 -> 35.
- **The harass and cautious behaviours** (no format change, saves from 0.254). EEComplexUnit::RepairPointers made a
  saved behaviour 3 or 4 the class's own. The computer players give their explorers harass on Hard, which can flee
  (dbunitbehavior mCanFlee): a loaded explorer stood and fought where the saved one ran (seed 7's 600 s save: the
  unit AI's ShouldIRun trigger). Measured: seed 7 600/2100 s 10 -> 22, 1 -> 99, seed 2 2100 s 20 -> 27.
- **0.266, the order of the units the unit managers track** (`EESaveHasUnitManagerOrder`). UnitAdded files a
  building by its distance to the town centre of that moment, so EECPUnitManager::mUnits' order is the game's
  history; the load's AddAllUnits filed them in the unit list's order. StartProduction's round robin walks it (seed
  6's 600 s save trained at the other of two barracks 50 s after the load: found by the buildings' mBuildQueue and
  mCurrentResearch swapped in the dumps' raw bytes, layout.json names the offsets). Saved as each class's ids;
  reordered at the first tick. Measured: seed 4 600 s 173 -> 208, seed 6 600 s 59 -> 73, seed 7 2100 s 99 -> 284.
- **The attack count and combat state** (no format change, saves from 0.254). EEUGAttackUnit::RepairPointers counted
  every loaded attack goal in mNumAttacks and set the combat state to 2 for the first; the constructor and destructor
  leave out an animal target, so a load counted the hunts for good and overwrote the saved state (seed 3's 1350 s
  save: state 2 with 2 attacks against 1 and 0; EnableManagers built other priority lists, two shuffle draws
  fewer, at the first AI update). Measured: seed 3 1350 s 0 -> 107.
- **0.267, the repair and site timers** (`EESaveHasCivilizationTimers`). EECPCivilizationManager's mNextRepairTime
  (and the airport and space dock site timers) were never saved: a loaded player repaired at its first update and
  every 31 s from there, sending other citizens to repair at other times (seed 4 1350 s at once, seeds 2 and 5 600 s
  later). The largest effect this round: seed 2 58/86/27 -> 308/683/27, seed 3 94 -> 207 and
  107 -> 309, seed 4 1350 s 1 -> 205, seed 5 23/106/35 -> 312/479/71, seed 6 73/16 -> 205/202, seed 7 22/178/284 ->
  171/559/420.
- **0.268, the towns' areas** (`EESaveHasTownAreas`, `EEWriteAreaBytes`/`EEReadAreaBytes` over the continent block's
  area format). The towns' block now also holds each town's continent and area (tiles as they are, hull, bounds);
  RepairPointers makes the towns from it (EEMakeSavedTowns) instead of rebuilding them from the buildings in the
  load's order, which made seed 6's 2100 s town 3 two towns and an extra patrol group. An older save, or a town that
  cannot be read, rebuilds as before. Measured: seed 6 2100 s 0 -> 101.
- **The numbers** (first divergent CT line after the load, 600/1350/2100 s, start -> end of this round): seed 2
  58/86/20 -> 308/683/27, seed 3 94/0/57 -> 207/309/569, seed 4 80/1/0 -> 208/205/52, seed 5 23/91/23 -> 312/479/71,
  seed 6 59/16/0 -> 205/202/101, seed 7 10/178/1 -> 171/559/420. Still **0 of 18** continue identically to the end
  (2700 s), but no save now parts in its first 25 CTs.
- **The unit store's checks on the load path**: all 18 loads of the 0.264 + behaviour build and of the final 0.268
  build ran to the end with EE_SIM_CAPTURE on (USCK every tick, from the first tick after the load): no mismatch.
- **Lesson: a stage slot per run fills the disk.** sim.py keeps build/sim/stage/<slot> per run name, seed and save
  (the written folders copied) and nothing deletes it; C: ran out with 149 of this worktree's slots in place
  (deleting them and the other agents' took it from 4 to 26 GB free). The scratch scripts now
  delete their slots and saves as soon as a run's numbers are in.
- **Tools** (scratch, under the worktree's build/hunt, not committed): `pair.py` (now removes its slots), `capload.py
  NAME` (each save of a saveload run loaded with captures on, the capture deleted after), `rawdiff.py DIR UNIT K`
  (the differing non-pointer words of a unit's raw bytes in two dumps), `sum.py` (a saveload log as the per-seed
  numbers), `clean.sh`.
- **Still different after a load** (not traced this round): seed 2 2100 s at CT 27, seed 4 2100 s at CT 52 (an `ai`
  checksum only: the computer players' state, no unit), seed 5 2100 s at 71, seed 6 2100 s at 101, the 600 s and
  1350 s saves after 200 to 680 CTs.

## R6: WONGUI's tree list, the menus' keys, the config readers' key tables and the event dispatch decisions in Rust (ee_won guitree.rs, guimenu.rs, guiconf.rs, guievent.rs; branch rust-r6-won10, 2026-11-11)

Round 10 of R6 (RUST_PLAN "R6 WON, the rest"). Same pattern as rounds 8 and 9: a `tests/won` driver (in `xbuild.py`
`WON_TESTS`) runs the original's statements, copied from the C++ before the port with the objects replaced by the
flags and numbers they read, against the Rust ABI over generated inputs. The C++ classes keep their objects, lists,
vtables, events and every call; Rust decides which call happens. No class moved wholly, so no unit was deleted.

- **The tree list (`guitree.rs`).** TreeItem's walks go through `EeWonTreeOps` (callbacks answering parent, index,
  child count, child and open, and doing a row insert or a close): `GetNextNonChild`, `InsertChildren` (children last
  to first, an open child's own rows right after it), `CloseAll`. Rust holds only the item pointers between the
  callbacks. The rest are decisions: the range and last-child tests (an index compared as `size_t`: a negative one is
  past the last child), `SetOpen`'s work (no children: only the flag, no event) and its scroll, `InsertChild`'s row,
  `RemoveChild`'s steps, `TreeArea::Sort`'s plan, `GetMouseItem`'s exit and enter (a callback asks `Contains` exactly
  where the original did), `CanSelect`, `ContainsPoint`'s x, the row width, the expand button's event, the flags.
  The tree has no keys of its own: its keyboard movement is ListArea's (round 8's `guilist`). `won_guitree_test`:
  6,233,856 checks, 0 failures.
- **The menus (`guimenu.rs`).** `PopupMenu::TryKeyDown`'s step once the open submenu declined the key (the C++ keeps
  the recursion into the submenu), `CheckSubMenu`'s open, keep or close, the submenu's selection rules; MenuBar's
  keys, `CheckItem`'s choice, its menu's place, flags (`StandardPopup` less `WantInput` and `EndOnEscape`: 0x2347)
  and avoided rectangle, presses, events, moves, the items' places and sizes. The original has no mnemonics: a letter
  does nothing in a menu. `won_guimenu_test`: 22,000,002 checks, 0 failures.
- **cbindgen exports every `pub const` into the global namespace** without a prefix: `guimenu`'s `VK_RETURN` became
  `constexpr static const int32_t VK_RETURN = 13;` in `ee_rust.h`, which windows.h's macro turns into
  `int32_t 0x0D = 13` in every unit that includes both. The modules' constants are private now (round 9's
  `POSITION_LEFT` and others are still exported; harmless while nothing defines those names).
- **The config readers' key tables (`guiconf.rs`).** Each reader compared its instruction with its keys
  (`key=="TEXT"`, exact) and the flag and value names it read with its names (`name.CompareNoCase("Bold")==0`); those
  lists are 103 tables of 509 names in the original's order (`EE_WON_CFG_KEYS_<class>_<function>_<variable>`,
  `EE_WON_CFG_NAMES_...`). A reader asks once (`ee_won_gui_cfg_key` over the `std::string`'s bytes,
  `ee_won_gui_cfg_name` over the GUIString's units through `ee_won_guistring_compare_narrow`, the same compare as
  before) and tests the index where it compared a text; a name listed twice answers its first place, the branch the
  original took (`WantDefault` in the button flags). The 42 units' edit was made by a script that checked that undoing
  it gives the original text back, and that no compared GUIString changes between the lookup and its last test (the
  one variable declared twice in one function, ServerListCtrlConfig's `aType`, has a lookup in each scope). The C++
  keeps the reads, the objects and what each entry does (the values stay in its branches). `won_guiconf_test`:
  839,246 checks, every name of every table in four cases and with a character more or less, through the real
  GUIStringData, in the C and the English locale, 0 failures.
- **The event dispatch (`guievent.rs`).** Window: the enter, a move's end of double-click detection (the original's
  `detect && x != dx || y != dy` kept: any change of y ends it), drag or move, a left release outside ending the drag
  with an exit, the double click (tick 0 kept for "none", the tick read only when the press asks for the check), the
  size change, the resize's delay, the forced cursor refresh, the hover, the place after an exit, centring, the
  cursor's refresh, closing. WindowManager: the 50 ms timer step and its 1 s cap, the popups' end or keep on a focus
  change, the resolution's restore, a press's focus and capture, a release's capture, a move's or key's end of the
  popups, the wheel and keys sent to the popup, the merged size changes, a closing window's popup, and
  `CheckPopupClick`'s walk down the clicked children. RootContainer: the window drag and the cursor component's
  place. Container: the mouse child's exit and enter, the release only to the child that had the press,
  `GetDesiredSize`, `FindTop` (now a walk over the flags of the children before the place: O(n) per `AddChild`
  instead of O(on-top children)). `won_guievent_test`: 40,578,784 checks, 0 failures.
- **`DWORD` is `unsigned long` in the Windows build** (windows.h) and `uint32_t` on POSIX (`platform/wintypes.h`): a
  `DWORD*` passed for the ABI's `uint32_t*` does not compile on Windows (and would on POSIX). The out-parameters are
  `uint32_t` locals; values cross by copy.
- **What exercises what.** Every lobby load: the config tables (every instruction and flag of the 63 .cfg files goes
  through them), Container's `FindTop` and `GetDesiredSize` (every child added, every fitted container), the window
  manager's timer step, cursor and resize rules, Window's enter and moves, the mouse child's routing as soon as the
  mouse moves. The LAN game (`lan_game.py`'s `wonclick` / `wonselect` / `wonkey`) adds the presses, releases, double
  clicks, focus and capture, the keys' routing, and the popups' rules when its dialogs open. Only a person reaches the
  window drag (a ParentDrag child), the hover, the wheel and the popup menus (right-click: `CheckSubMenu`, the
  selection rules). Nothing reaches the tree list (the Internet room dialog's room list, RoomCtrl) or the menu bar
  (unused) and `TryKeyDown` (only MenuBar calls it). The simulation is untouched: the oracle was identical after each
  of the four gated builds.
- **Still C++ of WONGUI:** the objects and the calls above, the config readers' branch bodies, and the drawing (Surface,
  StretchImage's nine slices, the decoders, the platform backend, the paint).

## R6: the mouse dispatch, the edit box's anchor fix, entity groups, the list's type-to-find, the text label's geometry and the scrolling label (ee_ui mouse.rs, entity_group.rs, find_key.rs, label_fit.rs, scrolling.rs; branch rust-r6-ui8, 2026-11-11)

Moved to `rust/crates/ee_ui` (the C++ classes keep every member, vtable and exported name; `UIControl.h`,
`UILBString.h`, `UITextLabel.h` and `UITLScrolling.h` gained a `friend struct`): `UIForm::UpdateMouse` and
`UIControl::UpdateMouse` (`mouse.rs`: the captured control or the one under the mouse, the wheel, the form's eleven
mouse events, a control's press, release and double-click dispatch and its button-down members);
`UIEntityGroup::Disable`/`Enable`'s loops (`entity_group.rs`); `UILBString::UpdateTranslatedKey`'s search
(`find_key.rs`); `UITextLabel::ComputeTextLabelRect`, `CheckScrollBar` and `SetLocation`'s rectangles
(`label_fit.rs`); `UITLScrolling`'s `CopyData` scroll and snapshot, both `ScrollText`s, `Pause`/`Resume`/
`StopScrolling` and `DrawTextA`'s line places (`scrolling.rs`). `RadioUnCheck` was already Rust (round 3's
`RemoveOtherChecks`); what is left of it is `UIButtonRust::RadioSwap`'s reference counts.

- **A fix, the owner's decision (the only behaviour change):** after an edit box's Backspace, Delete, Cut, Ctrl+L or
  a paste over a selection, a selection anchor that is no longer a place in the text (its line gone, or its index
  past its line's end) becomes the caret (`Edit::keep_anchor_in_text`, `edit_keys.rs`). The original kept it, so
  round 7's two hazards read past the lines: typing Backspace while drag-selecting with the mouse (the drag's next
  `MouseMove` selected from the old anchor), and Shift + the keypad's Del (Num Lock off) + an arrow in one frame
  with a selection (the arrow selected from the deleted selection's far end). An anchor still inside the text stays
  where it was, as in the original (a reset to the caret after every deletion would change nothing the game shows
  but would move the printed anchor in 34 of the key golden's 60 boxes). Two Rust tests reproduce the hazards (they
  failed before the fix); `golden_keys.txt` changed in 12 of its 60 boxes, each only in the anchor and the state
  lines that follow from it, re-recorded from the key driver over the fixed forwarders (seeds 2-5: 158, 186, 149 and
  185 of 600 boxes, the same kind of change only). Typing a character over a selection (`UpdateTranslatedKey`)
  deletes it and ends the drag (`selecting` false), so it needs no reset.
- **Mouse:** the captured control is read again after a control's handlers (they capture and release the mouse);
  a control whose `MouseWheelTurned` fails is forgotten for the form's wheel event and the result is OK; a press
  while the control thinks the button held is a press again (`MouseLButtonDown` once more), and a button up without
  a release event still releases. `gISMouse->GetEntity`'s out pointer starts NULL in the host (the original's was
  uninitialised; the real mouse always writes it).
- **Type-to-find under D6:** `UWideString::operator[](0)` is a byte, so a key compares its lower- and upper-case
  forms' first bytes with each line's first byte: `é` and `É` share the lead byte 0xc3 and match any line starting
  with a two-byte Latin letter (kept).
- **Text label:** `CheckScrollBar` does not look at the label type (a one-line label with more lines than rows gets
  a scroll bar; the game never gives it more than one line). `ComputeTextLabelRect`'s visible lines run from the
  slider's value for `mNumberLinesDisplayed` lines unchecked against the line count (the real slider's range keeps
  them inside).
- **Scrolling label:** the pixel offset is a `long` compared as a float, the snapshot loop's `y` an `unsigned long`
  against a `long` bottom (a negative offset skips the loop), `ScrollText(time, end)` divides `time / lines` in
  integers and its end delay wraps (`4294957151` in the golden): all kept.
- **Golden drivers, new traps.**
  - A calloc'ed form has no vtable: call `sForm->UIForm::UpdateMouse()` (qualified), or the virtual call jumps to 0.
  - A vtable of `template <int N>` stubs that log their slot number finds the slots a unit calls without a layout
    dump (`UIListBox<T>::ChangeSelectionIndex` is slot 36, `GetNumLines` 42, `HideControl` 12, `SetDirty` 25); then
    give those slots typed stubs (a generic stub's extra argument registers hold garbage that differs per build).
  - A fake class with an out-of-line constructor (the scroll bar, whose real one needs the manager's bitmaps):
    define the constructor's decorated name (`??0UIScrollBar@@QEAA@KAEBVGE2DPoint@@MKPEAVUIForm@@KK@Z`) as an
    `extern "C"` function taking `this` first and returning it, and fill the object there (memset, a stub vtable).
  - Private statics of a unit that is not linked are defined by their decorated names in an `extern "C" { T v
    __asm__("?sLMouseButton@UIManager@@0PEAVISEntity@@EA") = NULL; }` block; protected and private members called
    from the driver under `#define protected public` link by their real decoration
    (`?CheckScrollBar@UITextLabel@@MEAAXXZ`, `?ComputeTextLabelRect@UITextLabel@@AEAAXXZ`).
  - `GERectangle::RoundToNearest` is GEBitmap.cpp's (copied into the driver); `UIManager::AddEventToTrigger` is
    inline and pushes onto `mEventsToTrigger` (placement-new the deque in a calloc'ed manager; the event needs
    `UIEvent`'s size for its `Reference`).
- **Goldens:** `golden_mouse.txt` (1,500 worlds), `golden_group.txt` (400 groups and 400 lists),
  `golden_label_fit.txt` (150 labels x 30 operations), `golden_scroll.txt` (120 labels x 40 operations). Each
  replays whole in Rust; each driver relinked over the forwarders printed the same file, and the same on two more
  seeds at ten times the size; the label and scroll replays also pass on those seeds. Mutations checked: the
  wheel's forgotten control (mouse), the label margin and the centred left edge (label), the end offset's
  comparison (scroll) each fail their replay; an equality-only change of the end comparison and a reassociation of
  the vertical centring did not (no case lands on them).
- **Gates run:** after each commit `build.py -j 3 --sim` (the first `-j 4`) 0/0 (EE-AOC, EE-SIM), the sim oracle
  (seeds 2-7) identical to `ref`, `rust_build.py gate` PASS. No tour, screenshot, LAN game or POSIX build was run.
- **What a real game exercises (EE-SIM has no UI):** `UIForm::UpdateMouse` and `UIControl::UpdateMouse` run on every
  frame of every menu with the mouse over a form (hover, clicks, the wheel over a list): every menu load of a tour.
  `UIEntityGroup::Enable`/`Disable` run whenever the game enables or disables a group of buttons and hot keys
  (greyed buttons). `ComputeTextLabelRect` and `CheckScrollBar` run on every text label's `SetText` and
  `UITextLabel::SetLocation` on every label's move: every menu load; a scroll bar appears only on a multi-line label
  with more lines than rows (long texts, the chat). The scrolling label is the
  credits and the in-game scrolling texts (trigger 0x3f4): a tour reaches it only if it opens them.
  `UILBString::UpdateTranslatedKey` runs only when a person types a letter into a focused string list (a combo
  box's drop-down). The edit box fix changes only what a person's typing during a drag, or Shift + keypad Del with
  an arrow, does.

## R3.6, second round: the conductor, the music manager, the mixers' registry and the engine's start in Rust (ee_sound conductor.rs, music.rs, mixers.rs; branch rust-snd2, 2026-11-11)

Moved to `rust/crates/ee_sound`, each over a `Host` trait and C++ trampolines, the C++ classes keeping every member
the game reads and every exported name:

- `SEConductor` (`conductor.rs`): the score table (names, ids, load counts, the loaded `SEScore*`; the member
  `mScores` became the handle `mRust`), loading and the start's reload, the id counter's wrap past -1, start and
  stop, `Play`/`Queue`/`Stop`, `TransitionTo`'s pattern, the volume curve (`volume^1.1512925` in double, silent at
  0.01), the intensity and channel-group rules, `UnloadScore`. dmusic's loader and performance, the scores, the
  mixer's music source and the lock stay C++; the trampolines `ee_conductorcpp_*` are the dmusic call sequences.
- `EEMusicManager` (`music.rs`): the music thread's step (the intensity delay's countdown, what each wake-up does),
  `DoStart`/`DoPause`/`DoStop`, `NextSong` (a random song of the epoch, a second draw against the same song),
  `FadeIn`/`FadeOut`, the intensity rules (a drop waits 5 s) and the time to the next song (`lrintf(r * 120001 +
  300000)`, 420001 taken as 420000). This is where "which piece next, fades, intensity, timers" live: `SEConductor`
  and `SEScore` only carry them out. The manager has no Rust state; the members stay C++ (layout proven) and are
  read and written one by one through the host in the original's order, because `SetIntensity` runs on the render
  and simulation threads while the music thread is in a seconds-long fade (a copy in and out would lose their
  writes).
- `SEMixer` and `Sound Engine.cpp` (`mixers.rs`): the registry (`SEMixer::sMixers` became a Rust global behind a
  lock that is never held across a trampoline: the destructor a trampoline runs comes back for it), `GetMixer`,
  `GetCount` (out of line now), `UseMixer`'s stop/deactivate/activate decision, the destructor's, the start's walk
  of the plug-in table (a mixer per sound module, the invalid ones deleted at once, `kErrorNotFound` for none) and
  the shutdown's order. `sActiveMixer` stays a C++ static (the movie player reads it inline); the SPIM entry points,
  the conductor and `new`/`delete` of the mixers stay C++.

What did not move, and why:

- **There is no volume, pan, 3D attenuation or channel allocation in `SEMixer`.** It is a table of the plug-in's
  entry points. The mixing (volumes, pan, pitch, the channels' sums) is the plug-in's, in Rust since R3.1
  (`ee_mixer`); which channel a sound gets and the voice limits are `SESoundManager`'s, in Rust since the first
  round (`manager.rs`). The positional sound (volume and pan from the camera and the source) is the game's:
  `EEComputeSoundParameters` (`EEFOFindNewTargetNearOldTarget.cpp` 2905; file-static copies in
  `EEUCBasicPriest.cpp`, `EEUCBuildingPriest.cpp`), used by `PlayAnimationSound` (`EEGameUtilities.cpp`, falloff
  4.0), the units, the calamities (`kCalamitySoundFalloff`), `EEAmbientSound`, `EEE3DRain`.
- **`SEScore` has no decisions**: a dmusic segment's wrapper (repeat count, release). It stays C++ with dmusic.
- **MCI CD audio is dead on this port**: `SEInitialize` is called with `inUseCD == false` (Empire Earth.cpp 2564),
  so `SEInitializeCD`, `SECD` and the CD half of `SEShutdown` never run. Not ported.
- **Microsoft DirectMusic is gone** (PORT_LEARNINGS "DirectMusic -> dmusic"); the vendored dmusic library
  (`src/ThirdParty/dmusic`, C) plays the music and stays, D4-style, like miniaudio under the mixer.

The sound triggers in the game (simulation lanes, not moved; for later):

- unit acknowledgements: `EEMessage.cpp` picks the unit with `gPhysics->GetRandom` (the simulation's draw: it must
  move with the message's Execute, RUST_PLAN 2.1), then `EEPlayer::PlayAcknowledgement` (Player/EEPlayer.cpp 2747:
  the acknowledgement delay from `kWorldAcknowledgementDelay`, the group's change time) and
  `EEUnitClassUpgrade::GetAcknowledgeSound(-1)` (a file-static alternation, `sModel74Index`);
- selection sounds: `EEPlayer.cpp` 399/471 and `EEMessage.cpp` 6093 through `EEPlayForLocalPlayer`
  (`EEGameUtilities.cpp` 89: local player only);
- combat, death, construction and projectile sounds: `Units/` (17 units: `EEComplexUnit`, `EEUnit`, `EEUnitClass`,
  the `EEUC*` classes, `EEUGBuildRepair`), with `EEComputeSoundParameters`;
- calamities (16 units under `Calamities/`), the ambient sounds (`EEAmbientSound*.cpp`), the UI's clicks
  (`User Interface Thread/`, 6 units), the music's intensity from the battle (`EERenderObject.cpp` 1627: the
  fighting units' share every 250 ms, `gMusicFollowsBattle`).

Golden drivers (each over the unmodified units, `#define private public`, the units included whole; relinked over
the forwarders, each printed the same text):

- **Fakes that answer must print their answers.** The music driver's fakes (the wait's result, the time, the random
  generator, the songs, `GetScore`) draw from a seeded generator and print `name args -> answer`; the Rust fake
  host takes its answers from the golden text at the point it asks, so the replay needs no copy of the driver's
  generator and a call out of order fails at once ("wanted ..., the Rust asked ...").
- **Other threads inside the wait.** The music driver's `WaitAny` may call `SetIntensity` or change the current
  player before answering, as the render thread and the world would; the replay does the same at the same lines.
- **Globals the unit reads by object** (`gWorld`, `gGamePreferences`, `PIMModule::sDirectory`) are zeroed storage
  under `/ALTERNATENAME:<mangled>=storage`; `/FORCE:MULTIPLE` lets the driver's `PlatSleep` win over the Rust
  library's.
- **`#define private public` hides access errors in the forwarders.** The mixers' driver compiled the new
  `SEMixer.cpp` with every member public, so its `extern "C"` trampolines reading `SEMixer::sActiveMixer` (private)
  passed there and failed in the game's build: trampolines go through the nested `Rust` struct, and the full build
  is the check, not the driver.
- **cbindgen exports every `pub const` of the crate**: `WAIT_TIMEOUT` met Windows' macro of that name in the first
  driver build. The modules' constants are `pub(crate)`; the conductor's (`OK`, `BUSY`, ...) had reached the
  header in the conductor's commit and left it in the music manager's.

- **The Rust gate under load**: `ee_plat_core`'s timer tests failed twice with eight agents on the machine ("0 ticks");
  they pass alone (`--test-threads=1`), which is how this round's gate ran them.

Behaviour changes: `EEMusicObject::GetRandom` returning NULL (no song for the epoch) crashed `DoStart` and `NextSong`
(`LoadScore` on NULL); in Rust `DoStart` silences and asks to stop, `NextSong` keeps the old score id and plays
whatever the conductor still has for it (pinned by a unit test; the shipped tables have songs for every epoch).
`SEConductor::LoadScore(secondary)` with a name not in the table wrote entry `size - 1` (or `-1` when empty, UB);
Rust writes the last entry or deletes the new score when the table is empty (unreachable: the start reloads names
taken from the table).

Only a played game with sound exercises these (EE-SIM has no conductor; the goldens are fakes; nobody has listened):
the music's start at the first game frame (`EEWorld.cpp` 1053), every song change (5 to 7 minutes) and its fades,
the battle intensity's transitions, pause and resume (the scenario editor's pause), the settings' music on/off and
volume, loading a save (the music muted and restored), the start's choice of the mixer plug-in and the shutdown.

## R5b.2, second round: the conversion and heal tests, the drop point, the line of fire, splash victims and the new target search in Rust (ee_combat decide.rs, walks.rs; branch rust-combat2, 2026-11-11)

| Piece | C++ caller | Rust | Tag |
|---|---|---|---|
| can a priest convert a target | `EECanConvert` (`EEUCBasicPriest.cpp`) | `decide::can_convert` | CBCN |
| can a unit heal a target | `EECanHeal` | `decide::can_heal` | CBCH |
| the drop point on a target (model point, tile) | `EEFindDropLocation` | `decide::drop_point` | CBDP |
| the line of fire | `EEIsLineToUnitBlocked` | `walks::line_blocked` over `EeCombatLineHost` | CBLB |
| splash damage's victims | `EEUnitClass::ApplyAreaDamage` | `walks::area_victims` over `EeCombatAreaHost` | CBAD |
| the new target search | `EEFindNewTarget` (+ the deleted `EEUUGetUnitsInArea`) | `walks::units_in_area`, `walks::choose_target` over `EeCombatTargetHost` | CBFT |

- **Golden first, then the move.** Commit b96a53c2 wrapped the unmodified C++ (renamed `...Original`) with capture
  probes: the decisions' inputs gathered exactly as the Rust records later take them, the walks' observable results
  (units by `mUnitID`: the victims in hit order, the candidates in order and the chosen one, the line's result, last
  tile and blocker). Its digests are the new lines of the four golden files; the moved build gives the same records.
  The walks' records are not replayable without a world: `check` gates them, the unit tests pin the order.
- **Out flags that are not always written.** `EECanConvert` writes `outFlag` only on its last branch, and several
  callers declare `bool theFlag;` uninitialized and read it after a true result (a cyber priest's): the Rust result
  says `flag_written`, and the C++ writes the flag only then. Likewise `EEIsTileTopAbove` writes the blocker on every
  call but `EEIsLineToUnitBlocked` may make none: the golden wrapper detected that with a sentinel pointer, the host
  writes the caller's pointer directly.
- **Eager reads where the original was lazy**, all pure: the adaptation power, the shield, the damage table cell, a
  building priest's distance, the unit set. Two guards: the damage table is read only for an ammo column >= 0, the unit
  set only for a set below the manager's size (`EEUnitSetManager::Contains` indexes `mSets` unchecked; the original
  only reached it after every other test passed).
- **Walks with C++ mutations inside.** `ApplyAreaDamage` hits each victim (TakeHit, kills, food) before reading the
  tile node's `mNext`, as the original did: the host's `hit` runs inside the walk and `node_next` is asked after it.
  `IsTargetIgnored` erases expired entries, so `choose_target` asks each test lazily, in the original's order
  (`TARGET_TEST_*`); the technology kind keeps its last answer across candidates (asked only without `inArg6`).
- **Handles, not references.** Units and tile nodes cross as `uint64_t` (the pointers); Rust compares them (the
  victims' "hit once" list) and hands them back. The sort of `EESortUnique` is by `mUnitID` through `unit_id`;
  equal ids are one unit, so the stable Rust sort gives the C++'s order. The tile line crosses as a C++-owned pair
  array copied before the next host call.
- **Coverage.** The AI games never heal: EE-SIM's capture sweep (`EECombatCaptureSweep`) calls `EECanHeal` over its
  pairs both ways, a unit and itself, and the previous player's newest unit; CBCH's golden came from that sweep over
  the C++ test (oracle profile). CBCN comes from priests in the oracle's seed 6 and the late and space games, CBFT
  from a few dozen calls a game.
- **A generic type name in a crate breaks another crate's header type.** `rust/cbindgen.toml` renames `Handle` to
  `EeDx7Handle` (ee_dx7's `*mut c_void`); a `pub type Handle = u64` in ee_combat was renamed to the same C name and
  replaced it (`using EeDx7Handle = uint64_t`), and only the DX7 rasterizer units failed to compile. The Rust gate's
  header check passed (the header was current). Name exported aliases uniquely (`EeCombatHandle`).
- **What stays C++:** the weapon tag point and the target model's top (model data), `EEGetTileLine`, the walls'
  `EEIsTileTopAbove`, the region table, the screens' `Virtual08`, every target test, `EERetargetAttack`, the goal's
  location in `EEFindDropLocation`, the hits themselves.

## R4.4, second round: the free camera's pitch held past zoom 1, the pointer's arithmetic, the units' screen point and bars in Rust, a scroll check (ee_gfx mouse.rs, units.rs; branch rust-gfx8, 2026-11-11)

- **Behaviour change (the owner's call): the free camera holds its pitch past |zoom| 1.** Mode 3's `GetPitch` was
  `pow(1 - |zoom|, 0.5)`, NaN past zoom 1 (the port's `gCameraMaxZoom` is 2), and the table cosine faults on a NaN
  angle (round one's hazard). `camera::pitch` now clamps |zoom| to 1 in mode 3 as modes 0 and 1 do (the sign kept:
  the far pitch, negated for a negative zoom). Of the camera golden's 14,424 lines exactly the 71 GetPitch cases of
  mode 3 with |zoom| > 1 changed (each had a NaN output before); they were re-recorded from the driver over the
  fixed unit, every other line is still the original's. `free_pitch_holds_past_zoom_one` failed before the fix and
  passes after. The camera's other cases never had a mode-3 zoom past 1, and the simulation (EE-SIM) never enters
  mode 3: the oracle cannot see it; a cinematic after a far zoom-out can (only a person or a scenario reaches it).
- **The pointer (`mouse.rs`, `RTSMouse.cpp`).** `ClampPosition` and `Update`'s move: the raw axes scaled by the
  bounds over the desktop, the sensitivity and the smallest move seen. The smallest moves stay the unit's file
  statics (passed in and out, 32-bit: a mouse axis never reaches past it); the C++ keeps the entity reads in their
  order (x then y), the "MinXValue now" debug lines (the result says when a minimum changed), the frozen test and
  the wheel. `x < 0 ? -x : x` of `LONG_MIN` is `wrapping_neg` (the C++'s wrap).
- **The units' render object (`units.rs`, `EEROUnits.cpp`).** `GetScreenPoint` (over `ee_ge`'s `GeViewport` mirror
  of `gU4E8F08`), `DrawBars`' sizes from the window, `DrawBar`'s tag point, origin and rectangle split (full: two
  rectangles; partial: four, each at least a pixel wide; empty: two) and `DrawObjectBars`' (one or two). The C++
  keeps every rhombus call (SetRectangle, the colours, the draws) in the original's order by the result's `kind`.
  Not moved: the selection rings' size choice and the area-effect ring's colours (trivial, but no golden covers
  them: their models' members are private and the fakes were not worth it), the labels, the placement footprint.
- **The golden drivers** (`build/gfx8_golden`, scratch: `build.py LEAF orig|cur [seed [scale [full]]]`, BASE
  9635b89c). New trick: the driver `#include`s the unit (`EE_UNIT`, its absolute path, as a define) to reach its
  file statics (`sFGBar`, `sBarWidth`), and calls its private passes through explicit template instantiations
  (`template struct Rob<Tag, &EEROUnits::DrawBars>`: an explicit instantiation may name a private member). The
  rasterizer and the vertex arrays are fake objects whose vtable is 160/40 template stubs that log their slot (and
  the bar or colour argument); `GERhombus::ComputeRotation` (private, out of line) logs the rectangle just set. The
  rasterizer's inline getters dereference its screen states: the fake's words all point at a block of words that
  point at themselves, so any such chain reads harmlessly. Mouse: a fake `ISDevice` subclass stands in for
  `gISMouse` (`GetEntity` is the base's first slot), fake entities log each read, `ESDebugOut` logs its text.
  Results: mouse 4,000 cases, rounits 4,344; the relinked drivers printed the same files, and the same on seeds 2
  and 3 at four times the size (16,000 and 17,376 lines). Mutations (24.5 for 24 in the bar width, 0.0626 for
  0.0625 in the y move) fail the replays.
- **Python on Windows writes CRLF** (`Path.write_text` in text mode): a quick scripted edit of a Rust file turned
  it CRLF and showed a whole-file diff. Edit through `read_bytes`/`write_bytes`.
- **The scroll check (TEST ONLY).** The fixture writes a `camera` telemetry line (the focus now and the move's
  target in tiles, the zoom, the mode, whether each is a point of the mesh, the map size) and takes `key <form>
  <dik> <ms>` (a key held through `ISHandleEvent`, the entry the window layer feeds a key press to: the window
  message -> SDL -> event path itself is what `--input-check`'s F3 proves) and `wheel <form> <0|1> <n>`.
  `fixture_game.py --scroll-check` scrolls by an arrow key (the opposite one when the first is at the map's edge)
  and by the pointer at the window's left (else right) edge, requires each move of at least half a tile with the
  camera still on the map, then sets zoom 1 and requires the zoom hot keys (`]` in, `[` out) and the wheel to change
  it each way; `--zoom-captures P ...` is posix_game's. The self test's stand-in covers both, including a FAIL with
  its numbers. Not run against the game here (no harness sessions for agents).
- **Only a tour, a scroll/zoom run or a person reaches:** every change of this round draws or reads input on the
  UI and render threads: the bars and the screen point (any frame with a selected unit), the pointer's move (any
  mouse motion), the mode-3 pitch past zoom 1 (a cinematic or the free camera after a far zoom-out; a person or a
  scenario), the fixture's new commands (the scroll check). EE-SIM touches none of them.

## The C++ object cache: incremental builds shared by every worktree (tools/objcache.py; branch rust-objcache, 2026-11-11)

- **Problem:** `build.py` compiled every one of the 1,364 jobs from scratch on every build (`compile_all` -> `compile_one` -> `compile_to`, no reuse), and `--test-profile` compiled them all again into `build/clang-test`. With nine agents on 12 cores, a `-j 3 --sim` build took 34-40 minutes, 27-33 of them compiling.
- **Fix:** a content-addressed object cache in `compile_to`, two-level like ccache's direct mode. The manifest key is the schema, clang-cl's identity (path, size, mtime), every command-line argument and the INCLUDE path with the three per-worktree roots (staging root, repository root, toolchain directory) replaced by tokens, the unit's staged name and the content of the source as compiled (after an overlay). The manifest (zlib JSON, up to 8 entries newest first) holds the dependency sets seen for that key: every file clang's `-MD` list names (headers, the `#embed` fonts too), each with its content hash, and the object's key. A lookup re-hashes the recorded files (once per file per build). A missing file or object is a miss. Nothing is keyed on a source's mtime. The store is `C:\EEX-objcache-store` (`EE_OBJ_CACHE` moves it, `=off` disables it). Every write is a unique temp file plus `os.replace`, and a lost rename is dropped. Lookups copy the object (no hard link: a later miss must never write through into the store). Least-recently-used pruning to `EE_OBJ_CACHE_MAX_GB` (default 4) runs at the end of each build, and a hit touches its object and manifest. `/Zs`-style compiles (no object) bypass it.
- **Embedded paths:** `__FILE__` held the staging root (`C:\EEX-objcache\...`), and a test-profile overlay unit held its temporary copy's name (`Empire Earth.test18536-23464.cpp`, 5 such strings in a test exe). Every compile, cached or not, now maps the staging root to `C:\EEX`, the repository root to `C:\EE-ROOT`, the toolchain to `C:\EE-TOOLCHAIN` and an overlay copy to its unit's own name (`/clang:-ffile-prefix-map`; clang tries the longest prefix first). Every compile is also `/Brepro`: clang-cl's objects otherwise carry a time stamp, and that was the only difference between two compiles. The objects have no debug info (no `/Z7`), so `__FILE__` was the only path in them. Effect on the exe: none for a build staged at the default `C:\EEX` (its 131 `C:\EEX\...` strings are unchanged). A build staged elsewhere now embeds `C:\EEX\...` too, and the test exe names the overlay units by their real names.
- **Measured** (12 cores shared with eight agents, `-j 3 --sim`): clean with the cache off, 1600 s compile / 40 min total; clean with a cold cache, 1979 s / 34 min (heavier load at the time); from a full cache, 32 s compile / 2 min 16 s total (the two links are most of it now); one `.cpp` touched, 1 compile, 2 min 37 s total; `include/ee/EEWorld.h` touched, 427 compiles in 506 s (`--compile-only`); reverted, 1363/1363 hits. The test profile after a normal build: 8 misses (the 8 overlay units: the window/bit-depth rules and the harness hooks), the rest hits, then 1363/1363 on its second build. A second worktree at the same commit (another staging root, another repository root) got 1363/1363 hits. 24 of the jobs are libjpeg compiled twice (modules `jpeg` and `exeimg`; the link skips `exeimg`'s copy). They now cost nothing.
- **Identity:** EE-AOC.exe and EE-SIM.exe from a clean cache-off build, a cold-cache build and a full-cache build differ only in lld's link time stamp (the PE header's TimeDateStamp and the debug directory's: 4 bytes). The PDBs are byte-identical. sim_oracle seeds 2-7 identical to `ref` on the full-cache build.
- **Limits:** like ccache's direct mode, a header newly added earlier in an include search path (shadowing one a unit used) is not noticed. Staging never deletes, so this needs a new file in a staging or include directory with the name of a header another directory provides. `EE_OBJ_CACHE=off` or `objcache.py clear` covers it. The Rust library is separate (cargo, one target dir per worktree). `RUSTC_WRAPPER=sccache` shares its compiles where sccache is installed.

## R5b.4, eighth round: the fixture's gather scout, the store's on-the-map flag and the checksum's flag 1, Dropoff Resource, Build Wall and Unload Transport in Rust (ee_units store.rs, group.rs; branch rust-r5b4-core8, 2026-11-11)

- **The fixture's gather had no target** (the lead's two `--orders` runs: `ORDER gather: skipped (no-target)`). Not a
  filter bug: the captures show an arctic start and a grass start with no tree or mine in the view, and the mini
  map's explored area is the view itself (the human's citizens idle; the start's line of sight is short).
  `CanGather` also wants the resource's tile visible now (`IsVisibleTo`), as the real right click does. The fixture
  now scouts: with nothing in view it takes the world player's resource nearest the first citizen that the citizen
  could gather were it seen (`CanGather(.., true, false)`) and can reach (`CanGetTo`), selects the group, posts an
  `EEMPathTo` there, polls until the player sees the tile (60 s at most), centres the camera on it
  (`EEACenterCamera`, the mini map's own action, posted outside the world's lock) and scans the new view. A failed
  scout is "scout-unseen" / "scout-no-pick", which the driver fails (they are not skips). The driver waits 120 s for
  gather. The self test's stand-in scouts (phase 1, note "scouting", 1.5 s) and refuses its second gather
  "scout-unseen" (the driver must raise). Not run here: only the lead's harness run shows it in the game.
- **The on-the-map flag.** The out-of-sync checksum walks `gWorld.mUnits`, one x-sorted chain through `mTileNode`
  (not the tiles' lists). A unit is in it when `mTileNode->mPrev` is set (`Link` puts the head or a unit there,
  `Unlink` clears it); `RTSUnit::Link` and `Unlink` report the flag (`EEUnitStoreLinked`), the insert seeds it.
  USCK now also walks the chain at every check: each chain unit must have a record of that object with the flag, and
  the records with the flag must be the chain (`Store::check_on_map`); and the checksum's flag 1 over the chain
  (`GetChecksum(1)`) must equal the store's sum (`Store::checksum_flag1`). A difference counts as a mismatch, so the
  USCK records (and their digests) are unchanged while there is none.
- **Flag 1 over the store.** `EEPhysics::GetOutOfSynchChecksum` adds `ee_units_store_checksum_flag1()` for flag 1
  and walks the chain only for the other unit flags (2, 0x20, 8), with flag 1 masked off; the sums wrap and commute,
  so the total is the original's (the oracle's `oos=` is identical). The game's own lockstep check (flag 1 alone,
  `EEPhysics.cpp` DrainInput) now walks no unit. The sum is a wrapping 64-bit sum of the C++ `long`s: its low 32
  bits equal the Windows `unsigned long` sum, all 64 bits the LP64 one.
- **Dropoff Resource** (`group::dropoff_resource`): the existing host calls (Fill, the pending list, the tile spread,
  a Gather goal with no resource and the drop-off, the action, Plan, the draw). **Build Wall** (`group::build_wall`):
  three new host calls (`wall_plan`: `EEPlanWall(.., true, false, pieces)` into a list the host owns; `wall_build`:
  `EEBuildWall`; `wall_release`), and `EEActiveGroup::KeepCitizens`' test over the store (`keeps_citizen`; its
  three callers: Build Unit, Build Wall, Smart Farmers). The dry plan places nothing: it logs and answers two pieces.
  Build Wall is the one goal-giving body a capture game sends (the late profile's computer players' walls), so the
  late captures run its live path; the others only through the USGC sweep (cases added: Dropoff at the player's own
  first unit, the next player's, its path point and no unit; Build Wall eight tiles east of the first unit and a
  one-tile wall at the map's centre).
- **Unload Transport** (`group::unload_transport`): five new host calls (`group_spread_for_unload`; `unit_can_unload`,
  the class's transport or paratrooper-bomber flag and the cargo, read on the object; the unload goal's
  `SetTransport` and `CanUnloadAtDestination` before it is given; the "cannot unload there" event) and two goal
  constructors (`new EEUGUnloadTransport(first's complex unit, x, y, false)`; the move `new EEUGLocation(x, y, 0,
  false, true, false, 0, false, 0)`). The cargo walk keeps the original's shape: it stops at the first loaded
  transport, and when none is found the walk's last unit is the one that acknowledges (and is tested again for the
  event). The dry goal answers "can unload" yes. No capture game unloads: the sweep (the first unit's tile and the
  map's centre) and mock-host tests are the coverage; only a played game runs the world calls.
- **The patrol-point mode the UI lacks (round 7):** not a reconstruction bug. In the retail `EE-AOC.exe` the only
  store of `EEMAddPatrolPoint`'s vtable (0x83df18) is its import constructor (0x4ba47b): the game never builds one to
  send. The only `EEMPatrol` sender (its constructor COMDAT 0x66fa28) is called from 0x66f5c8, the status button's
  command 0x12, with `push -1`. The main game form's mouse-mode table (its constructor) has no patrol mode: state
  0x10 is `EEModeHeal` (the "StartPatrol" name at 0x66681d, buttons 0x1b/0x22, is the heal mode). So the Art of
  Conquest UI patrols only the nearest town's route; `EEMAddPatrolPoint` and the path points' routes come from
  elsewhere (the scenario side, saves, the AI's towns).

## R6: the post-game graphs, UIBFull's tiles, the IME candidate window, the hot keys' setters, the tool tip's place and the toolkit's audit (ee_ui graph.rs, background.rs, ime.rs, hotkey.rs, form_state.rs; branch rust-r6-ui9, 2026-11-11)

Moved to `rust/crates/ee_ui` (the C++ classes keep every member, vtable and exported name; `UIGraph.h`, `UIBFull.h`
and `UIIMECandidate.h` gained a `friend struct`): `UIGraph::CopyData`'s layout of the title, the axis labels and
the grid values' margins and its range copy, `ZoomIn`/`ZoomOut`, `DrawPolygons`' inner border and grid lines,
`DrawGridText`'s value places, `DrawTextA`'s choice of labels, `UILineGraph::DrawPolygons`' series choice and
`DrawSeries`' skip, clip and scale (`graph.rs`; `UILGClipSegment` deleted from the C++); `UIBFull::DrawBackground`'s
tile placement and `LoadBackground`'s names (`background.rs` `bfull_draw`, round 7's `full_tile_name`);
`UIIMECandidate::OpenCandidateWindow`'s rectangle, `SetCandidateList`'s page, numbers and width, `DrawPolygons`'
selection bar and border lines and `DrawTextA`'s rows (`ime.rs`); `UIHotKey`'s four event setters, `SetModifier`
and `CreateHotKeys`' record loop (`hotkey.rs`); `UIForm::SetHelpTextPopupLocation` (`form_state.rs`
`help_popup_location`). The texts (the graphs' formats are virtual: the post-game summary prints times and counts),
the candidate strings' walk, the bitmaps, lines, rhombi and every draw stay C++, reached through host calls in the
C++'s order.

- **Behaviour:** one divergence, where the original crashed: `SetCandidateList` into a candidate window lower than a
  line (its rectangle is still the constructor's zeros before the first `OpenCandidateWindow`) divided by a page size
  of 0; the Rust page is empty (no line, the selection kept). The driver skips those calls (the C++ would fault).
- **Kept quirks:** the graph's grid values are measured on the displayed range's maxima before `CopyData` copies the
  graph's range; `DrawSeries` scales with the live `mGraphRectangle` and the displayed range (not the copies); a
  vertical segment is skipped at or left of the series' minimum x compared unsigned; `UMCeilFloat(0 / zoom)` is 0
  so 0 divisions draw one NaN-spaced line; `UIBFull`'s redraw rectangle is clipped to the form in place, so a redraw
  that misses the form draws the whole form; `SetCandidateList` walks past the list's strings when the selection's
  page starts after its count (reads only, no line is made); setting a hot key's event to the one it holds
  releases it before referencing it again (an event held only there is deleted first); a hot key record whose read
  fails keeps the previous record's bytes.
- **Original hazards found, left as they were:** `UIIMECandidate`'s constructor leaves `mSelection` and
  `mCopySelection` uninitialised (a `DrawPolygons` before the first `CopyData` reads garbage; the driver sets -1);
  `CreateHotKeys`' record buffer is uninitialised when the first record's read fails (the C++ reads stack bytes,
  the Rust zeros; the driver never fails the first record read).
- **Round 8's two undetected mutations, now covered:** the scroll driver (`build/ui_scroll_golden`) gained 30
  labels on whole pixels scrolled one pixel a frame to the end, so the end offset's comparison meets equality
  (`<` -> `<=` now fails the replay); the label driver (`build/ui_label_golden`) gained 40 one-line fitted labels
  whose rectangles a search picked so that the vertical centring, `(height * 0.5 + top) - text * 0.5`, rounds
  (`RoundToNearest`) differently from either reassociation (both now fail). Both goldens grew at the end only (the
  first 150 / 120 labels are byte for byte the old file); both drivers relinked over the round-8 forwarders print
  the same.
- **Golden drivers, new traps.**
  - `UGraphSeries::AddDataPoint` sorts the points by x and replaces a repeated last x: log the stored points, not
    the inputs.
  - An x64 vtable's slots go by declaration order (one slot for the deleting destructor; overloads in reverse):
    `GESurface::GetWidth` is slot 24, `GetHeight` 12, not the header's 32-bit offsets / 4. The numbered-stub trick
    (`template <int N>` logging N) found it in one run.
  - A unit's static object with an out-of-line constructor (`UIHotKey::sPressedPoll`, a `TMPoll`) runs at start-up:
    an unresolved constructor under `/FORCE:UNRESOLVED` faults before `main`; define it in the stubs.
  - An inline constructor calling a private member (`UIHotKey()` -> `CalculateKeyCount`) must be instantiated in a
    unit compiled without `#define private public` (the stubs file), or it links to the public decoration.
  - An `extern "C"` host function is no friend: private members go through the friend struct's statics.
  - Do not reuse an existing golden's file name (`golden_hotkey.txt` was round 5's: this round's is
    `golden_hotkey_setters.txt`).
- **Goldens:** `golden_graph.txt` (120 graphs x 30 operations), `golden_bfull.txt` (150 backgrounds x 12),
  `golden_ime.txt` (150 windows x 20), `golden_hotkey_setters.txt` (150 hot keys x 12, 200 files), `golden_help.txt`
  (3,000 tips); each replays whole in Rust, each driver (`build/u9_golden/build.py PIECE orig|cur`) relinked over the
  forwarders printed the same, and the same on seeds 2 and 3 at ten times the size (the Rust replays pass on those,
  `EE_UI_GOLDEN=<file> cargo test -- --ignored`). Mutations checked: the grid values' row loop `<` -> `<=`, the
  vertical skip's `<=` -> `<` (after adding vertical segments at the minimum), the y-label rectangle's
  reassociation and the title's left each fail the graph replay.
- **What a real game exercises (EE-SIM has no UI):** the graphs are the post-game summary's line graph
  (`EEUIPGSLineGraph`): only the summary screen after a game reaches them (zoom from its buttons). `UIBFull` is a
  full-form background of the UI database (type 9's grid of textures): every menu load whose form has one draws it
  (the tour). `SetHelpTextPopupLocation` places every tool tip: a tour's hover over a button with help text (the
  main game overrides it with an empty body). The hot keys' `CreateHotKeys` runs once at start-up (every hot key
  of every menu comes from it), the setters on every form open that copies a hot key; `SetModifier` only from
  game code that builds a hot key. The IME candidate window only appears with an IME composing (a person typing
  Japanese, Chinese or Korean into an edit box): only a person reaches it.

**The toolkit's audit (round 9).** `build/u9_golden/census.py` lists every function body of the 46 units
(`src/Low-Level Engine/User Interface/*.cpp`; the headers' inline bodies are accessors, the inline constructors and
the `UMCeilFloat`/`GetBorderColors` helpers): 552 bodies, 5,780 lines. By kind: 243 forwarders to Rust (1,913 lines,
including the host glue the Rust calls back), 75 constructors and destructors (1,275), 12 platform calls (295: the
file lists' directory search, the clipboard, the database files, the texture loads), 78 pure call sequences (408)
and 144 bodies with a branch (1,888). Of those 144, by what the branch decides:
- *object lifetime and ownership* (the bulk): `UIControlsTable::Create*` (which class a record makes and its
  constructor's arguments, 300 lines), `UIManager::CreateBackgrounds` (the background class per type),
  `LoadDatabase`, the `Set*Event` member setters of `UIForm` (16 x 12 lines: the hot keys' pattern, release the
  old event and take the new one), `SetFormEvent`'s switch, `AddHotKey`/`RemoveHotKey*`, `AddBackground`,
  `RemoveBackground`, `ReserveControlIndex`, `DeleteAll`s, `UIGraph::SetFont`, the radio button's bitmap setters,
  `UIButtonRust::RadioSwap` (reference counts);
- *NULL checks before a draw or a copy* (`DrawPolygons`/`DrawTextA`/`CopyData`/`ChangeFont`/`SetText` of the
  status bar, text label, check boxes, buttons, bitmap, movie, combo box, form and manager);
- *lookups in the C++ containers* (`GetControlById`, `GetBackgroundByID`, `GetEntityGroupById`,
  `GetAvailableIndex`);
- *dispatch to a virtual or a captured control* (`UIManager::UpdateKeyboard`/`UpdateMouse`/`UpdateTranslatedKey`,
  the forms' IME pass-throughs, the events' `ProcessEvent`s, the text label's mouse handlers to its scroll bar);
- *state that is still C++ and small*: `UIForm::SetTabStop`/`SetTabbing`/`SetMouseOverControl`/`Deactivate`/
  `RelayoutForm`/`UpdateHelpText`/`UpdateHelpString` (flags and calls in order), `UIControl::SetDisplayMode`/
  `SetDirty`/`SetLocation`/`SetRectangle`/`SetTriggerEvent`, `UIBlinkingButton::SetIsBlinking`,
  `UIEditBox::MouseLButtonDown/Up/DoublePressed` (capture and dispatch to the Rust text model),
  `UITextLabel::RecomputeDisplayRange` (one line count, then the Rust label calls), `UIManager::SetMouseLeftHanded`
  (a swap).
Every layout, placement, tiling, scroll, wrap, selection, hit test, timer rule and text model is Rust; the only
arithmetic left in C++ is the constructors' first rectangles (the status bar's fill, the text label's text and
scroll bar areas, the scroll bar's parts), `UITextLabel::RecomputeDisplayRange`'s line count (one `UMFloor`, as
`label_fit.rs` computes it elsewhere) and `GetAvailableIndex`' scan. What is left decides which C++ object to make, call, keep or release, or
whether a pointer is set; moving it means moving the objects (the classes wholly), which is the next stage, not
more leaves.
- **Gates run:** the full build (`build.py -j 3 --sim`, 0/0 for EE-AOC and EE-SIM) after the hot keys' commit and on
  the merged result, `rust_build.py gate`, the sim oracle (see the report); the test-profile build at the end. The
  per-commit oracle was not run for each commit (the lighter gates of the overloaded machine: one build for the
  first four commits). No tour, screenshot, LAN game or POSIX build was run.

## R6: WONGUI's drawing as plans the C++ executes (ee_won guidraw.rs; branch rust-r6-won11, 2026-11-11)

Round 11 of R6 (RUST_PLAN "R6 WON, the rest"). The paints' decisions and arithmetic moved to `rust/crates/ee_won`
as plans, the pattern of `ee_ui` draw.rs and `ee_ge` terrain_visible.rs: the C++ reads the object's members, Rust
returns the ordered draw operations (`EeWonDrawOp`: a kind and eight ints), and `WONDrawRun`
(`include/ee/wonapi/WONGUIDraw.h`, new) executes them on the `Graphics`; an image, font, string or wrapped text is
named by a slot, and the calls into other objects (a child's paint, a background, a cell) come back to the caller as
`EE_WON_DRAW_CALL`. A plan longer than the 48-operation stack buffer is asked again at its length (the plans are
pure). The Surface, PlatGraphics, image, font and object calls stay C++; no class lost a member, no header gained
one.

- **What moved.** `StretchImageFilter::Filter`'s nine-piece placement (`stretch_filter`: the new size, the centre's
  stretch or tiles, the four sides, the four corners; the tile loops are expanded into the copies they made, the
  centre's one-way tiles copying from the new image itself as before; `RawTileHorz`, `RawTileVert` and
  `RawTileHorzVert` deleted, `CopyRawImage` and `RawStretch`, the pixel loops, stay C++), `StretchImage::Paint`'s
  skip-centre strips, `StretchCenterImage::Paint`, both `WONFrame::Paint`s (the full one is the padded one with no
  paddings now), `WONComboBox::Paint`'s and `WONScrollContainer::Paint`'s frames and corner, `Background::Paint`,
  `WONButton::Paint` and `DrawButtonText`, `WONSimpleButton::Paint`'s image, `WONHeaderButton::Paint`'s sort arrow,
  `Label::Paint`'s text place, `WrappedText::Paint`, `ListArea::ScrollPaint` (the paint width and the visible items,
  through a place callback; the visibility is round 8's `guilist`), `MultiListArea::ScrollPaint`'s passes,
  `MultiListItem::Paint`, `TextListItem::Paint`, `InputBox::Paint` (text, selection, caret), `TextArea::ScrollPaint`,
  `TextSegment::Paint`, `TextImage::Paint` and `SelectionColor::GetForegroundColor`.
- **Measures.** Where a paint measured text (the input box's selection and caret, a segment's selection, a centred
  wrapped text's last line) the plan asks the C++ through `EeWonTextMeasure` (`Font::GetStringWidth`,
  `GetCharacterWidth` of a unit or of 'M') at the same point of its statements, so the font sees the same calls in the
  same order. They now happen while the plan is made, before its draws run; between a paint's draws they did not.
  `GetHeight` and `WrappedText::GetWidth/GetHeight` are read before the plan (pure getters).
- **What stays C++, on purpose.** The pure call sequences: `ScrollArea::Paint` (background, clip, children, translate,
  `ScrollPaint`), `Container::Paint`'s child walk, `WONInputBox::Paint` and `WONFrameComponent::Paint` (base, then
  frame), `Component::Paint`; TabCtrl, ComboBox, Splitter, Scrollbar and Dialog have no paint of their own (their
  children's are the ones above). Not ported: `ImageComponent`, `RectangleComponent(3D)`, `ColHeaderButton`,
  `ImageCheckButton`, `BorderImageButton`, the menus' and tree items' paints (MS-skin and unused controls).
- **The golden driver** (`build/won_draw_golden`, scratch: `build.py orig|cur [seeds]`, `digest.py`): the WONGUI units
  of 7f0e83de (`git archive`) or the worktree's, compiled with the won module's flags (`clang_build.compile_to`, -O1),
  with the worktree's `ee_rust.lib`, under `/FORCE:UNRESOLVED`. `fakes.cpp` replaces `Graphics` (every call one log
  line), `Font` (a width per unit, `(unit % 7) + 2 + size / 4`, its height its size; every measure logged to a second
  list written after the case's draws), names images and strings, and defines `MultiListHeader::GetSortTypeForButton`
  (a chosen sort), `Component::SetComponentFlags`, `GetOwnColorScheme` and `gEmptyWideString`. The objects are real
  where their constructor's unit is cheap (WONButton, WONHeaderButton, WONSimpleButton, StretchImage, TextSegment,
  TextLine, TextListItem, MultiListItem, WrappedText through its real wrap) and calloc'ed with their containers
  placement-constructed otherwise; their Paint is called qualified, so no vtable is needed. 4500 cases in 19 kinds:
  300 filters (source 1..24 square, borders, target 0..60, both tiles, skip-centre; the result's size and pixel hash,
  each pixel its source coordinate, fresh buffers 0xEE), 200 stretch paints, 100 stretch-centre, 200 frames, 100
  combo frames, 100 scroll containers, 300 backgrounds (stretch, tiles from negative offsets, border, component), 400
  buttons and 200 header buttons (every alignment, scale, push, disabled grey, focus, state cells), 200 simple
  buttons, 200 labels, 300 wrapped texts (wrapping, centred, line breaks, no font), 200 lists, 200 multi-column lists
  (real rows and text cells, null cells, full-row selection, a column whose width less the border is -1), 300 text
  rows, 400 input boxes (both backgrounds, icon, selection both ways, caret bar and underline past the end), 200 text
  areas' line passes, 400 segments and pieces, 200 text images. `golden_draw.txt` keeps each case's inputs, its log's
  line count and FNV-1a 64 (530 KB; the full log is 45k lines); `guidraw/tests.rs` rebuilds every case's whole log
  over the plans and a model of the executor and fakes. The driver relinked over the forwarders printed the same log
  byte for byte, and on seeds 2 and 3 at ten times the cases (435k and 407k lines); the Rust replay also passed seed
  2's 45,000 cases. Mutations checked: the text row's x, the segment's empty-selection test, the caret's end each fail
  the replay; a tile loop's `<` against `<=` does not (the last full tile is the same pixels either way).
- **Driver traps, new.** `Graphics`'s constructor needs `WONRectangle()` (Rectangle.cpp); `GetEmptyGUIStringData`
  reads `gEmptyWideString` (EEWonNet.cpp) at the first string; `Button::mDefaultButton`'s static destructor releases a
  half-built button at exit (the driver `_exit`s); a button's or label's text needs a font (the original's `GetHeight`
  dereferences it). Python's `write_text` on Windows turns an LF file into CR LF: edit sources as bytes.
- **Name clash caught by the header.** guidraw's first line-break struct was `EeWonBreak`, guiwrap's name: cbindgen
  kept one silently (the header lost guiwrap's). Renamed `EeWonLineBreak`; check `git diff include/rust/ee_rust.h`
  for removed lines after adding a type.
- **Kept quirks.** An alignment outside the enum left `DrawButtonText`'s x unset (here 0); `focus_shown` is evaluated
  with the other inputs (the parent is read for a focused button with a focus style even when the text is grey or
  absent; such a button always has one); a background tiled with an image of width or height 0 loops without end, an
  offset modulo 0 faults, as before; `MultiListItem` reads a cell only for a column that is one of the row's, as the
  original's unsigned compare did.
- **What draws what.** Every lobby load (and so the LAN game's lobby captures): the skins' StretchImage filters and
  paints (buttons, frames, scroll bars, the tab control's skip-centre frame in `_LobbySkin.cfg`), the WON buttons and
  simple buttons (scroll arrows and thumbs), the frames of the input boxes, combo boxes and scroll containers and the
  scroll containers' corner, the backgrounds (the tiled `@TiledBackground`), every label and wrapped button text, the
  game and player lists (multi-column rows, text cells), the input boxes (the name and chat lines: text, caret),
  the MOTD and chat text areas' lines and segments. The LAN game adds: pressed, hover and focus states of the buttons
  it clicks, the list rows' selection, the chat input's caret and typed text, the chat lines. Only a person reaches:
  the input boxes' and text areas' mouse selection (the selection rectangles, `sel_foreground`), the header buttons'
  sort arrow (after a header click), disabled grey text on a focused button with the dialog keys used (Tab), the
  underline caret. Nothing reaches `TextImage` (`TextArea::AddImage` has no caller). EE-SIM draws no lobby: the gates
  below cannot see a drawing fault; the lead's LAN game and a lobby tour must compare captures against the build
  before this branch.

## R5b.2, second round: the calamity classes' updates, CanCastAt, ResearchToEpoch and the effects' unit walks in Rust (ee_tech calamity_update.rs, calamity_cast.rs; branch rust-tech2, 2026-11-11)

Every calamity class's `Update`, `CanCastAt` with every class's `IsValidTarget`/`IsValidLocation`, the steps of
`EETechTree::ResearchToEpoch` and which units an effect walks applies to (with the values it writes) are Rust. The
C++ keeps the classes, their members, vtables and object lifetimes, and every world effect.

- **Capture first, from the unmodified C++** (commits fcb0f0bb, 9b084b41; `ee/EETechCapture.h`): CALW (one record per
  calamity update: the calamity's numeric state before and after, the elapsed time, the result, and the update's
  world calls in order: damage with unit, amount, player and mode, immunity, strikes, cracks, camera shakes, every
  physics random draw with its value, storms, meteors, volcanoes, the time warp's, teleport's and logic bomb's steps,
  helpers, placed and destroyed effects, morale, the special orders), CCAS (every outermost `CanCastAt` of the game
  and a sweep every five game minutes over every calamity class, every player's first living unit, six points
  (its tile, a target's, the map centre, an offset, a far unit's, off the map) and four targets, checked and not),
  TRTE (ResearchToEpoch's steps and the tree after), EVUN (the line of sight, hit point, flight time, area effect
  and conversion walks: per unit its view, whether the effect applies, the value). In the same runs every existing
  digest of the four profiles equalled its golden. Counts: CALW 13,802-63,259 updates in the calamity games, 8,503
  in late seed 2 (a computer player casting by itself); CCAS 41,364 sweep records per oracle game (49,061-55,152 in
  late, with the prophets' own calls); EVUN only where effects rescale or convert (late: about 1,000 a game).
- **The updates over a host.** `calamity_update::update` dispatches on the class enum; each class's body is the
  C++'s, with every world effect an `EeCalamityOp` call (`EeCalamityCall`/`EeCalamityReply`). The calamity's
  members travel as `EeCalamityState` (time alive, the float, int and bool placeholders, the target, the epicenter
  factor, the effect handle): the C++ copies it into the `EECalamity` before every host call and back after it, so a
  world step that reads or changes the calamity (`Depart` writes four of its members, `CreateStorm` its target) sees
  and leaves what the C++ update saw. The shared calls (helpers, sounds, units near, the damage loops' unit views,
  `TakeHit`, the three physics draws, the target and caster) are `EECalamityUpdate.cpp`'s; a class's own steps (its
  effects, units, orders, file statics such as the time warp's colours and the logic bomb's `AdjustLocation`) are
  its static `UpdateStep` in its own unit. The unit views ask in the C++'s short-circuit order and stop where the C++
  stopped (the continent of a unit that is no land building is never asked).
- **Kept quirks.** `min`/`max` are the Windows macros (`(a < b) ? a : b`: the second for NaN). The earthquake's
  damage is the C cast of `factor * damage * ticks` (float); the hurricane's tick count is `lrintf(time / tick)` and
  its remainder `lrintf(time - tick * ticks)`; unsigned members (`mTickTime`, `mDuration`, `mTime1`, the effect
  delays) are converted to float as the C++ compares them. The earthquake's `UpdateEpicenterFactor`,
  `UpdateQuakeSound` and `ShakeCamera` are gone (the camera distance and the shake itself are host calls).
- **CanCastAt.** `calamity_cast::can_cast_at` is the base test and the five overrides' compositions (global 0, the
  location's and unit's extra test, the special attack's 2 allowed); `IsValidLocation`/`IsValidTarget` are virtual
  and asked through the host, and the overrides (area effects and leprosy, earthquake, hurricane, meteor storm,
  volcano, teleport, special location; conscription, drop object, logic bomb, morale, special attack, time warp)
  are forwarders into `valid_location`/`valid_target` over `EECalamityCast.cpp`. The fire's and the plague's
  `IsValidTarget` stay their functor's `CanInfect` (no decision of their own). `GetCost()` is asked up front (no
  class overrides it).
- **ResearchToEpoch** asks Rust for its steps (unlock, research all, research self, research kind 7, show the name)
  and runs them; **the effects' walks** take `ee_tech_unit_step` (hit points, pending hit points, flight time: whether
  the unit is rescaled, whether the value is written, the value), `ee_tech_sight_applies`,
  `ee_tech_area_effect_applies`, `ee_tech_conversion` and `ee_tech_conversion_unit` (a carried unit on a tile is kept,
  and the previous new unit's behaviour is set again, as the C++ did).
- **Only the calamity profile and late exercise most of this**: no oracle game casts a calamity or runs CanCastAt
  except the sweep; the prophets (late, space) call CanCastAt; CALW covers every class in the calamity profile. The
  media effects, `Unlock`, `ResearchAll` and saves stay C++. Nothing here draws: no change is tour-only.

## R5a.2, sixth round: the managers' timers, the players' unit lists, the intelligence files, the tracked units (save 0.269-0.271, branch rust-r5a-aisave5, 2026-11-11)

Continues "R5a.2, fifth round". Same test (`sim_oracle.py saveload NAME --seeds 2-7 --at 600,1350,2100`, 18 saves).
Each change was built (0/0 for EE-AOC and EE-SIM), checked with the oracle (identical to ref; seeds 2-7 for the
first two, seeds 2-3 under the lighter per-commit gates after that) and the Rust gate, and measured with the full
saveload run. All of it is save and load code: it runs in the real game's Load Game screen as in EE-SIM (no change to
how a game plays, the plain oracle stays identical).

- **0.269, the managers' timers** (`EESaveHasManagerTimers`, EEComputerPlayer::GetManagerTimers/SetManagerTimers,
  after the computer player's 0.256 running values; put back at the first tick). Five timers the original kept out of
  the save, so a loaded player started them at 0: the attack commander's next drop of spent groups (mNextDropTime:
  DropSpentGroups ran at the first update and every 45 s from the load), the exploration manager's next update
  (mNextTime, every 30 s), the personnel's next update (every 30 s), the build manager's next builder check (every
  120 s) and the defence commander's next raid check. Found from the checksum lines themselves: many saves parted
  on `oos, ai` only, the AI part off by 93 (= 31 x 3: player 2's unit manager mNumWaiting, one explorer request
  early or late) or by -3/-4 (player 2's or 3's attack group count: a group dropped at another time); the first
  differing AI decision of seeds 2/2100, 7/600, 5/1350 and 7/1350 was `explore train.request` 1 to 6 s apart.
  Measured: seed 2 308/683/27 -> END/END/472, seed 3 207 -> END, seed 4 208/205/52 -> 480/END/END, seed 5
  312/479/71 -> END/END/END, seed 6 205 -> 308, seed 7 171/559 -> END/END: **10 of 18** saves continue identically
  to the end (the first ever).
- **0.270, the order of each player's unit list** (`EESaveHasPlayerUnitOrder`, EEPWorldHelper's
  SavePlayerUnitOrder/RestorePlayerUnitOrder). RTSPlayer::InsertUnit puts a unit after the first unit of its id's
  hash bucket (id & 0x7f), or at the end of the list when that bucket is empty, so the running list holds the
  bucket groups in the order the buckets last filled. RTSPlayer::SaveUnits writes them in bucket index order (each
  bucket's own order kept: first, then last to second) and the load inserts them in that order, so every walk of a
  player's units (mFirstUnit, mNext) saw the groups in another order after a load. Found at seed 6's 2100 s save:
  the dumps parted when two citizens of a militia fight attacked another raider (EECPRMMilitia takes the nearest
  raider, the first in the attacker's list among equals). The ids are saved after the resource state; at the first
  tick the list (mNext, mPrevious, first and last) and each bucket's first unit are relinked to the saved order (a
  unit only the load has goes after the last unit of its bucket, keeping each bucket's units together as GetUnit
  walks them). Measured: seed 2 2100 s 472 -> END, seed 3 2100 s 569 -> END, seed 6 1350/2100 s 202/101 -> END/174:
  **13 of 18**.
- **0.271, the intelligence files** (`EESaveHasIntelligence`, EECPIntelligence::SaveFiles/ReadFiles/RestoreFiles,
  after the manager timers). What a computer player knows of each player (EECPIntelligencePlayerFile: the armies,
  the unit type and family counts, the bounds, strengths and counts, its next update) was never saved: a loaded
  player made each file again from the world at its first use, where the running game's file held what it saw up
  to 30 s before. Found at seed 7's 2100 s save with the draw traces (`pair.py --rng-from`): the continued game drew
  eight numbers in EETown::GetRandomPoint (EECPDefenseCommander::AssignSubGroupTask) where the loaded game drew two;
  the dumps' ASG line showed the sub group's task in another town (FindThreatenedTown weighs each town by the files'
  armies). The files are one byte block per computer player; the armies' areas use the continent block's area format
  (EEWriteAreaBytes) and they are put in place of the load's at the first tick. Measured: seed 4 600 s 480 -> END,
  seed 6 600/2100 s 308/174 -> END/END, seed 7 2100 s 420 -> END: **17 of 18**.
- **The tracked units the running game did not track** (no format change, saves from 0.266). The load's
  AddAllUnits tracks every unit of a computer player; the 0.266 order put the saved units first and kept the others.
  Seed 3's 1350 s save tracked one citizen more than the continued game (the dumps' new `workers=`: 18 against 17 at
  the load), so GetNumWorkers and with it the standing army target and the demand for soldiers were higher (the
  trace's `demand t1=` 30 food and 30 gold apart 150 s after the load, the AI checksum 309 CTs after it). A unit the
  saved order lacks is now let go (with the reference UnitAdded took). Measured: seed 3 1350 s 309 -> END:
  **18 of 18**, `saveload: PASS`.
- **Tools**: the dumps' AI line shows the manager timers (`timers=`) and the demand's inputs (`wantcit= workers=
  gathercap=`), PLAYERUNITS lines each player's unit list (TEST ONLY, committed). Scratch, under build/hunt (not
  committed): `pair.py --rng-from MS` (the draw trace from save ms + MS: the 50,000-draw cap ends long before a late
  divergence otherwise), `td.py` (the first differing AI decision of two oracle traces, the trace's statistics lines
  left out), `gate2.sh` (the lighter per-commit gates), `sl.sh` (one saveload measure, its slots deleted).
- **Lesson: read the checksum's arithmetic.** EECPOutOfSynchChecksum sums per player `theSum * (id + 1)`, its parts
  chained by `* 31`: a difference of 93 is 31 x 3 (player 2, the part before the last), -3 or -4 the attack group
  count of player 2 or 3. That named the unsaved timers before any dump was read.
- **The numbers** (first divergent CT line after the load, 600/1350/2100 s, start -> end of this round): seed 2
  308/683/27 -> END/END/END, seed 3 207/309/569 -> END/END/END, seed 4 208/205/52 ->
  END/END/END, seed 5 312/479/71 -> END/END/END, seed 6 205/202/101 -> END/END/END, seed 7 171/559/420 ->
  END/END/END. **All 18 saves continue identically to the end** (2700 s; `sim_oracle.py saveload`: PASS).
- **Not covered**: saves of other maps, difficulties, game speeds or player counts; the next engine (EECPNext) and
  human players' state; multiplayer saves. The saveload test is the measure only for its 18 saves; a new divergence
  elsewhere names its own cause with the same tools.

## R6: WON's Internet code, round 1 (WONServer's exchange, ServerContext, WONDir's ops)

Branch `rust-won-net1`. The owner keeps WON's Internet-only code and ports it as a reference; nothing reaches it
(the WON servers are gone), so no game, tour or LAN gate exercises any line of this section: only the drivers do.

- **The exchange over hooks.** `ServerRequestOp` is the base of about 60 service ops and an `AsyncOp` over a
  `BlockingSocket`, both C++ objects; its algorithm (failover across the address list, the expired-session retry,
  the last-error rule, the blocking loop, the asynchronous completions) moved to `server.rs` over a trait of the
  op's primitives, which the C++ hands over as a struct of function pointers plus pointers to the five members the
  exchange keeps. The members are read and written through the pointers at every step, never cached across a
  hook: a subclass's `CheckResponse` calls `SetLastServerError` (C++, forwarding to Rust on the same members) in
  the middle of a step.
- **Scripted-environment drivers.** The driver copies the original's statements into a class whose primitives are
  a scripted environment (an address list, scripted connects, peer auths, requests, sends, receives, replies,
  checks and time left, each logged); the same environment behind the hooks runs the Rust exchange, and the two
  traces and final members must agree. Defaults when a script runs out and a call cap (300) make every scenario
  end. The golden (`golden_server.txt`) carries the script with the trace, so the Rust replay needs no generator.
- **A golden must hit the rare paths.** The first 500-scenario golden missed a mutation (the expired-session test
  dropped from the context's failure notice): an expiry needs a reused session, an expired reply and a server
  context together. `--print` now keeps every scenario whose session expired and one in four of the rest; the
  mutation then fails the replay. The 200,000 direct comparisons in the driver cover the rest.
- **Partial reads.** `PingOp::CheckResponse` assigns each field as it reads it, so a cut reply leaves the first
  fields set before the throw: Rust returns how many fields it read and the C++ assigns that many, then throws
  `ReadBufferException`. The same holds for the directory status replies' returned address (assigned only once
  read whole).
- **ServerContext wholly in Rust.** Its map of list iterators keyed by the six address bytes is a `Vec` searched
  by those bytes (equality is all the list needs; the set copy sorts by them). An `IPAddr` crosses as its value
  (sockaddr, validity, remembered host string) because callers read the host string of the copies. The shuffle
  keeps the CRT's `rand()` (and `srand(time(NULL))`) through a callback, so the order is the original's.
- **Tool traps.** Git Bash rewrites a command-line argument starting with `//` (an MSYS path): a splice script
  given `"// FUNCTION: ..."` as its marker matched one character late and left `/// FUNCTION` in the file. Pass
  markers in a file, or check the result. Heredocs with unbalanced quotes inside (an apostrophe in a comment)
  failed in the Bash tool; write such scripts with the editor instead.
- **Network-layer APIs these call** (for the merge with the lobby lane's whole move of the network layer):
  `BlockingSocket(0)`, `SetLengthFieldSize`, `Close`, `Connect`, `SendMsg`, `RecvMsg`, `QueueOp`,
  `AddMsgTransform`, `CryptTransform`, `ConnectOp`, `SendMsgOp`, `RecvMsgOp::GetMsg`, `SendBytesToOp`,
  `RecvBytesFromOp`, `SetRepeatOp` / `SetRepeatCompletion`, `AsyncOp::Finish` / `Kill` / `TimeLeft` /
  `GetStatus` / `RunAsync` / `RunBlock` / `mKilled` / `mRunMode`, `AsyncOpWithTracker::Track` /
  `mTracker.KillAll`, `ByteBuffer(data, length, false)`, `ReadBufferException`, `WideAppendUtf16LE`,
  `WideAppendUtf16Unit`, `WONTickCount`; and IPAddr's public members `mAddr`, `mIsValid`, `mHost`,
  `mRememberHostString` (ServerContext's value conversion).

## R6: WON's first whole classes: the buffers, WONStatus, Thread, TimerThread and AsyncOpContainer as Rust objects (ee_won buffer.rs, thread.rs; branch rust-r6-won12, 2026-11-11)

Round 12 of R6 (RUST_PLAN "R6 WON, the rest"), the first round of the owner's decision that WON becomes wholly
internal to Rust: whole classes move, bottom-up; a Rust object owns the state, and the C++ class is deleted or
reduced to a thin shim while C++ callers remain. The Internet services (WONDir, DB, Routing, Server, Misc,
Internal, GetCertOp, peer auth) are kept and ported by a second lane (branch `rust-won-net1`); the network layer
below them keeps its C++ API as shims over the Rust objects.

- **What moved.** `ByteBuffer` (a `RefCount` shim holding an `EeWonByteBuffer`: owned bytes, possibly a view into a
  writer's former allocation, or none for `ByteBuffer(NULL)`), `WriteBuffer` (holding an `EeWonWriteBuffer`: the
  capacity's growth from 32, doubling, `ReserveChunk`'s cap, 6 spare bytes in front for the prefix, `ToByteBuffer`'s
  prefix and hand-over), `ReadBuffer` (embedding its `EeWonReadBuffer` by value, a view over bytes it does not own:
  the bound, the sign-extended length fields; the shim throws `ReadBufferException`), all in `buffer.rs`;
  `WONStatusToString` (an inline shim over `ee_won_status_text`, a thread's own buffer for unnamed statuses);
  `ThreadBase`/`Thread` (an `EeWonThread`: the run flags, priority, auto-reset signal, platform thread and its id;
  the object's thread calls the C++ `ThreadFunc` back, inside the crash dump's SEH filter on Windows, and deletes an
  auto-deleting thread after it), `TimerThread` (wholly an `EeWonTimerThread`: the two deadline maps, which op holds
  which entry (the original kept the multimap iterator in the op: `AsyncOp::mTimerQueue`/`mTimerKey` are gone), the
  lock, the thread's loop), `AsyncOpContainer` (wholly an `EeWonOpContainer`: the queue, the op being pumped, the
  lock, the signal, the pump and the purge), in `thread.rs`. Deleted: ByteBuffer.cpp (with `OffsetByteBuffer`),
  ReadBuffer.cpp, WriteBuffer.cpp, WONStatus.cpp, Thread.cpp, TimerThread.cpp, AsyncOpContainer.cpp, and the C ABI
  of the queues the C++ drove before (`ee_won_timers_*`, `ee_won_opqueue_*`, `ee_won_pump_plan_*`,
  `ee_won_read_*`, `ee_won_write_*`, `ee_won_encode_length`, `ee_won_length_prefix`).
- **Same locks, same threads, same order.** The Rust objects lock with the platform's recursive mutex (the C++
  `CriticalSection` was one) and wait on the platform's events (whose waits follow the harness clock), so the ops'
  callbacks (`Finish(WS_TimedOut)`, `Kill(true)`, `StartRunAsync`, `Complete`, the references) run under the same
  locks, on the same threads, in the same order as the statements they replace. A std `Mutex` would have deadlocked
  the first op whose `Finish` queues itself back into the container it is pumped from.
- **The goldens, recorded first.** `won_buffer_test` (3103 steps: every append, growth, reserve, chunk, set, prefix
  size and hand-over, the reads, their throws and what a throw leaves in the string) and `won_asyncop_test` (128
  events with every op's reference count: queued, pumped, finished from outside, rerun, killed with and without
  finishing, the op being pumped queuing itself, the pump's op limit, run immediately, blocking, a zero timeout, the
  purge, ops at the container's deletion; timers due, replaced, removed, purged, on the thread, after a stop, at the
  thread's deletion; a thread's start, `InThisThread`, stop, priority and auto-delete) were recorded over the
  unmodified C++ classes, committed before the move, and print the same over the Rust objects (the asyncop driver
  20 runs out of 20; its two lines reading the threads' protected flag and signal go through the shim's
  `IsStopped`/`WaitForSignal` now). `buffer_golden` replays the buffer script whole in Rust. Driver traps:
  `new[]` is zero-filled in the buffer driver (a growth's bytes were indeterminate); a reader position past the end
  is not asked (the bound wraps and reads outside the buffer, as before); the auto-deleting thread logs from itself
  (the log needs a lock).
- **Mechanical edits outside the lane** (the lead coordinates them with `rust-won-net1`): the readers of
  `ReadBuffer::mPos/mData/mLength` and `WriteBuffer::mData/mLength` use `pos()`, `data()`, `length()` (or
  `Available()` where the original compared signed): WONAuth AuthSession, GetCertOp, PeerAuthClient,
  PeerAuthServer; WONDB DBProxyOp, ProfileData; WONMisc DetectFirewallOp. No other service file changed.
- **Behaviour changes.** A growth's, `SetSize`'s or `SkipBytes`' new bytes are zero (indeterminate before); a
  `SetBytes` past the allocation stops the game (it wrote outside it); `ReadRawWString`'s byte count no longer wraps
  at 32 bits on Windows (it read outside the buffer); `WriteBuffer::Release` is `Reset` (nobody took the allocation
  any more: `ToByteBuffer(true)` hands it to the byte buffer); `AddTimerOp` takes the op's new reference before
  dropping the old entry's (the original released first: a timer entry holding the last reference would have
  freed the op it then used); a `ByteBuffer` made with `inTakeOwnership` copies the bytes and frees the caller's array at once; the
  thread's platform handle is closed with its object (it was never closed).
- **Not moved, and why.** `AsyncOp` keeps its members: about 30 service files of the other lane read `mRunMode`,
  `mKilled`, `mStatus` directly, so moving its state into a Rust object needs a coordinated accessor change first
  (`IsAsync()`, `IsKilled()`, `GetStatus()`); its transitions are Rust's since round 3. The socket core (IPAddr,
  AsyncSocket, the socket ops, RecvCRMsgOp, SocketThread, BlockingSocket, QueueSocket, LobbyLanSocket) is the next
  round's.
- **What runs it.** Every lobby load reads its files through `ReadBuffer`/`WriteBuffer` (the saved settings) and
  starts WONAPICore's timer and socket threads and its op container; the LAN game adds every lobby and game-room
  message (`WriteBuffer` built, `ReadBuffer` read), the LAN socket ops queued and completed through the container,
  the broadcast and join timeouts through the timer thread. EE-SIM never loads the lobby: the oracle cannot see a
  fault here; the lead's LAN game is the end-to-end check.

## R5b.2, third round: the target searches' scoring, EEGetPursuerOf, the tile line, the weapon tag point and the damage table storage in Rust (ee_combat search.rs, pursuer.rs, geometry.rs, damage_table.rs; branch rust-combat3, 2026-11-11)

| Piece | C++ caller | Rust | Tag |
|---|---|---|---|
| the region target search (`EEUUTargetSearch`, both `EEUUSearchTiles`) | `EESearchTarget` | `search::search_target` over `EeCombatSearchHost` | CBST |
| the unit-list target search | `EESearchTargetAlongList` | `search::search_along_list` | CBSL |
| the nearest pursuer | `EEGetPursuerOf` | `pursuer::pursuer_of` | CBPU |
| the tiles of a line | `EEGetTileLine` | `geometry::tile_line` | CBTL (replayed) |
| the line of fire's start (the weapon tag point's height) | `EELineOfFireStartHeight` | `geometry::weapon_height` over `EeCombatWeaponIn` | CBWP (replayed) |
| the damage tables' storage | `EETerrainTypeTable::LoadDamageTables`, `ClearDamageTables` | `damage_table` (`ee_combat_damage_factor`, `_row`) | CBDT |

- **Golden first, then the move.** Commit 1510a730 put TEST ONLY probes into the unmodified C++: each Score call (and
  each list unit the list search looks at) appended (unit id, outcome, score) to a trace, outcome 0 rejected, 1 a
  large unit scored already, 2 not better, 3 the best so far, 4 unreachable and ignored, 5 the preferred type; the
  record holds the call's inputs, the trace, the best and its score. CBPU lists each pursuer with its distance, CBTL
  the line's tiles, CBWP the weapon point's inputs as the Rust record takes them (the model chain's transforms
  appended) and the height. The moved code reports the same trace through the host's `trace` callback, so the
  candidate order, the scores and the ties are compared record by record.
- **No draw in these searches.** `PermuteValues` (the region order) takes `gPhysics->mTickRandom`, a value drawn once a
  tick, not a draw of its own: `search::permute` is the shuffle, the URandom stream is untouched. The draws behind
  `EEIsAttackReachable` stay C++ behind the host.
- **Test order is the gate.** Several tests write (IsTargetIgnored erases expired entries; GetAttackFactor and
  EEIsLineToUnitBlocked write CBAF / CBLB records): the Rust asks each when the original evaluated it, short-circuits
  included. Two quirks kept: the list search's backward side has no `mShootsHigh` test, and its loop ends when the
  forward neighbour is missing even with the backward side going on; a preferred unit there ends the search
  before the step. The technology kind is read before the tests (eager in the original too). The region search's
  priest test is the candidate's class, the list search's the searching unit's (as written in each original).
- **Handles differ by search.** A region holds complex units, the list holds units whose complex unit may be missing;
  one host table (`EeCombatSearchHost`) serves both, its context saying which (`EESearchContext::mKind`), and each
  test is the original expression for its search (a NULL complex unit reaches `CanAttackClass` exactly where the
  original dereferenced it).
- **The multimap of the melee search** is a vector with `partition_point(!(key < k))` inserts: equal distances keep
  insertion order, as `std::multimap::insert` does.
- **The tile line** is a straight transcription of the four-step rasteriser (its `theSteep ? (y, x) : (x, y)` is
  one closure); the mutations of `++theX` / `theY += theStep` inside the pushes are done before each push. CBTL is
  replayed byte for byte by `combat_replay`, as is CBWP.
- **The weapon tag point**: the C++ gathers the tag point at the frame, the model chain's internal transforms
  (`EETagPoint::GetPoint`'s walk), the turret's yaw and the box; Rust does `EETagPointList::Scale`,
  `SetOrientationYPR(yaw, 0, 0)` and the applies with ee_ge's `GeTransformation` (the header's summation order).
- **The damage tables** are boxed rows in a `RwLock` (written only while the db loads); `gDamageTable` and the twins
  stay `float**` arrays whose rows point into the Rust storage, so the direct readers keep working. CBDT is now
  recorded from the Rust rows (the same bytes). See below for the AI's sites.
- **The AI's direct reads.** 24 reads of `gDamageTable[row][column]` (EECPASGAir, Submarine, UHero, UMechProphet,
  UProphet, EECPAttackSubGroup(Unit), EEHerd, EEMessage, EEUCBasicPriest, EEUGAttackUnit, EEUnitAI,
  EEFOFindNewTargetNearOldTarget, EEUIFMGStatusButton, GetAttackFactor) now call `EEDamageFactor` (EmpireEarth.h),
  ee_combat's checked accessor: the same value inside the table, 0 outside it (where the C++ read before or past
  an array: the -1 ammo column the earlier rounds guarded). The sim oracle and the four capture profiles stayed
  identical with the switch. The two stores (the load) stay; the float** arrays stay for any reader left.
- **CBCH in every profile.** Round 2 recorded CBCH (EE-SIM's sweep over `EECanHeal`) only for the oracle profile. The
  late, space and calamity lines were recorded from a temporary build whose `EECanHeal` ran b96a53c2's C++ test
  (the world otherwise this round's, every other digest of those runs equal to the C++ goldens).
- **Coverage.** No game of the four profiles calls `EESearchTargetAlongList` (anti-air ammunition 0x3c only reaches
  it through `EEFindNearbyTarget`): CBSL has no golden and the list search is gated by its unit tests only. The
  searches only change what an AI game simulates through the host's tests, so the oracle covers the rest.
- **Builds are slow on a loaded machine** (27 minutes for `build.py -j 3 --sim`): the goldens of all five tags were
  recorded from one instrumented build (its EE-SIM copied aside, so the moves could build while the four profiles
  ran), and pieces that touch different units were built together, then committed one by one (the intermediate
  commits' headers regenerated for each).
- **What stays C++:** every candidate test, `EEIsAttackReachable`, `IgnoreTarget`, `GetNearestFootprintTile`, the
  goal casts, the model data walk (tag points, chains, boxes), `EEFindNewTarget`'s screens, the other weapon point
  users (`LaunchShot`, `LaunchFromTurret`, `EEApproachAttachPoint`, the effects).

## R6: WON's Internet code, round 2 (the routing protocol, WONDB)

Branch `rust-won-net2`. As in round 1, nothing in the game reaches this code (the WON servers are gone): only the drivers exercise it, and no tour, LAN game or EE-SIM run does.

- **A reply as a list of assignments.** A routing op's `HandleReply` assigns its members, builds records through the connection's virtual factories, inserts them into maps, calls `CheckLeaveGroup` in the middle and sometimes hands the message to a temporary inner op. Rust (`routemsg.rs`) reads the message and returns the original's assignments in its order: a field to a member slot, a new record from a factory, a record field, an insert into a container, an action. The C++ applies them (`WONRoutingMsg.h`) and throws `ReadBufferException` where the original's read failed. A cut reply then leaves exactly what the original left: a member list's member is inserted before its client's name is read, a group only after all its members, the newsletter entry of `ProfileData::UnpackGetData` exists (false) before its value is read.
- **A string cut after its length is erased.** `ReadRawString` and `ReadRawWString` erase the target before `ReadBytes` throws. A partial read is an assignment of the empty string, not "nothing assigned"; a buffer (`ReadBuf`) cut short is never assigned.
- **The order of assignments is the order of reads.** The first version of the members reply read the observer count before it assigned the two client-flag bools; the driver failed on replies cut at that short. The rule applies to every reader that hands values back: emit each assignment right after the read that produced it.
- **Templates over the op for the driver.** The op classes cannot be linked into a test (AsyncOp, the connection and sockets come with them), so each op's request and reply is a template over the op in the header, and the driver instantiates it on a stand-in with the ops' members and types (`mFlags` is a byte, a short or a long depending on the op). Members that are private (`RoutingDetectFirewallOp`, the group admin ops, `RoutingDerivedServerOp`'s protected ones, the account ops) are handed over by reference instead; access is checked in the template's context, not the caller's.
- **ProfileData wholly in Rust.** Nothing outside `ProfileData.cpp` read its members, so the class is a shim over a Rust `EeWonProfile` (birth date, newsletter map, custom action). `GetBirthDate` was computed in a signed int (`year << 16` goes negative from 32768); the FFI returns an `i32` and the shim widens it as the original did.
- **Network-layer APIs these call** (for the lobby lane's whole move): `WriteBuffer(size, true)`, `Reset`, `AppendBytes`, `ToByteBuffer`, `data()`, `length()`; `ReadBuffer(data, length)`, `data()`, `length()`, `pos()`, `SetPos`; `ByteBuffer(data, length, false)`; `ReadBufferException`; `WonByteBufferSink`; `WideAppendUtf16LE`, `WideAssignUtf16LE`.

## R6: WON's AsyncOp, AsyncOpTracker and SocketThreadSimple as Rust objects (ee_won opobj.rs, sockthread.rs; branch rust-r6-won13, 2026-11-11)

Round 13 of R6 (RUST_PLAN "R6 WON, the rest"), the second round of whole-class moves: the op every WON service
derives from, its sub-op tracker and the socket thread now keep their state in Rust objects; the C++ classes are
shims while C++ callers remain.

- **Accessors first, then the move.** About 40 files read `AsyncOp`'s members directly (`mStatus`, `mKilled`,
  `mRunMode`, `mTimeout`, `mCompletion`, `mContainer`, `mStartTime`) and two read `AsyncOpTracker`'s multimaps
  (PingLogic, HTTPDocumentOps). The members were made private and the compiler listed the sites; a script rewrote
  each at the reported line and column (a read becomes `GetStatus()`, `IsKilled()`, `GetMode()`, `GetTimeout()`,
  `GetCompletion()`, `GetStartTime()`, `GetContainer()`; an assignment `x->mCompletion = e;` becomes
  `x->SetCompletion(e);`, ending at the statement's first `;` outside parentheses). That commit changed no
  behaviour and carried the new golden; the next swapped the implementation under the same accessors. clang-cl
  reports only a few errors per unit after a template's first one, so the loop ran the build until no private-member
  error was left (six passes). Files touched outside the lane (mechanical, accessor calls only): EEWonNet.cpp;
  WONAPI.cpp; WONAuth AuthContext, GetCertOp, PeerAuthOp, PeerAuthServerOp; WONInternal FilePushOp; WONMisc
  DetectFirewallOp, FTPGetOp, FindPatchOp, GetHTTPDocumentOp, GetVersionOp, HTTPDocumentOps, HTTPEngine, HTTPGetOp,
  ReportPatchStatusOp; WONRouting RoutingConnection, RoutingCreateGroupOp, RoutingOp, SmartRoutingConnection;
  WONServer MultiPingOp, ServerConnection, ServerOp, ServerRequestOp; WONSocket BlockingSocket, QueueSocket,
  SocketOps, SocketRunHooks, UDPManager; the lobby's InitLogic, LanLogic, LobbyLogic, PingLogic, RoutingLogic,
  StagingLogic, EELobbyFileTransfer.
- **What moved.** `EeWonAsyncOp`: the run state as atomics (the original read `mStatus`, `mTimeout` and
  `mCompletion` outside the lock in `Run`'s result, `StartRunAsync` and `Complete`; atomics keep those reads without
  a data race), the lock (the platform's recursive mutex, taken where the C++ took it), the completion's reference,
  the container (the C++ object for `GetContainer`, its Rust `EeWonOpContainer` for the queueing), and every step:
  `Run`, `RunAsTimer`, `StartRunAsync`, `Finish`, `ForceFinish`, `Kill`, `Complete`, `QueueInOpContainer`, the time
  left. The C++ `AsyncOp` holds the object; its virtual `RunHook`/`CleanupHook`, the completion's `Complete` and
  references, WONAPICore's queue and timer, and the ops' references and kill come back through one static hook
  table in AsyncOp.cpp. `EeWonOpTracker` is `AsyncOpTracker` whole (the two multimaps `AsyncOpTrackerMaps` and
  `AsyncOpTrackerEntry` are deleted). `EeWonSocketThread` is `SocketThreadSimple`'s op set, lock, `DoOp`, pump,
  purge, adds and removes; the C++ class keeps the `Thread` shim and waits on its signal for the time the pump
  returns. Deleted C ABI: the `OpState` copies (`ee_won_op_state_init`, `_run`, `_finish`, `_kill`, ...,
  `ee_won_op_time_left`), the op set's (`ee_won_opset_*`), `ee_won_socket_op_finishes`, `ee_won_socket_pump_wait`.
- **The tracker's order.** The original inserted with the id's first entry as the hint, and MSVC's multimap puts a
  hinted equal key before the hint: among equal ids the latest op comes first (`KillAll`'s and PingLogic's order).
  The golden shows it (`kd kf kb ka ke` for ids 0, 1, 1, 1, 7 added as ka, kb, kd, ke, kf).
- **Goldens, recorded first.** `won_asyncop_test` grew from 128 to 285 lines: 120 over the C++ AsyncOp and tracker
  (finish twice, force-finish pending and delivered, an idle kill, a blocking run left pending, the completion's
  references, the time left of blocking and async runs, the tracker's order, refusals, remove, `KillAll`, deletion,
  `AsyncOpWithTracker`'s `CallbackHook` and cleanup), then 37 over the C++ socket thread (adds, pumps logged sorted
  by name since the set is in address order, removes, an op finishing in its own `Continue`, the purge, the thread's
  loop, ops held at deletion). Each part printed the same over the Rust objects, 30 runs out of 30 under a parallel
  build. Driver traps: `a + f(x) + f(x)` evaluates right to left under the MSVC ABI (the two `RemoveOp` results came
  out swapped: sequence the calls); a parent op's completion must not cast it as the test's own op class; a 1 ms
  timeout "past" after `PlatSleep(5)` is not, at the system clock's 15.6 ms resolution (sleep 40); a socket op handed
  to `WONAPICore::AddToSocketThread` with no core started is killed (the `killed 1` in the golden is the original's).
- **Behaviour changes.** A tracked op holds one reference (two before: the op map's key and the entry);
  `KillAll` takes the first op each turn, so an op tracked during a kill is killed as well (the original's iterator
  had passed it). Nothing else: the same locks, threads and order.
- **Not moved, and why.** IPAddr is a value type (copied into maps, messages and every socket, its `std::string`
  host by value) whose members the Internet lane's ServerContext reads directly; its logic is Rust's since round 2,
  and making it a Rust object means a handle with copy semantics on every copy site. AsyncSocket, the socket ops,
  RecvCRMsgOp, BlockingSocket, QueueSocket and LobbyLanSocket are next: about 80 files name `mCore`, `mDataCrit`,
  `mLengthFieldSize` and friends (many of them other classes' members of the same name), so the same
  private-members pass comes first.
- **What runs it.** Every WON op of the lobby (the LAN socket ops, the broadcast and join timers, the lobby
  logics' tracked ops, PingLogic's pings) now runs through `EeWonAsyncOp`; the LAN game's socket ops are pumped by
  `EeWonSocketThread`. EE-SIM never loads the lobby, so the oracle cannot see a fault here; the lead's LAN game is
  the end-to-end check (`lan_game.py`: the lobby start-up, LanLogic's discovery and game-channel sockets, their
  QueueSocket and BlockingSocket ops through the container, socket thread and timer thread, the join, the game
  room's messages, the start).

## R5a.2, seventh round: the load test over other games, empty unit lists, past-due epoch advances, dead gatherers and carcasses (save 0.272, branch rust-r5a-aisave6, 2026-11-11)

Continues "R5a.2, sixth round", whose 18 saves (seeds 2-7, the oracle's game) all continue identically. This round
widens the test and fixes what the wider test found. Each fix was built (0/0 for EE-AOC and EE-SIM), checked with the
oracle on seeds 2-3 (identical to ref) and the Rust gate, and measured with the matrix below. All of it is save and
load code (the loaded game, not the running one; the plain oracle stays identical) except the TEST ONLY tools.

- **The matrix** (`sim_oracle.py saveload NAME --settings LIST|all [--clean]`, committed): SETTINGS holds the games:
  `oracle`, `plains-medium-3ai`, `islands` (Large Islands), `highlands-large-4ai`, `mediterranean-easy`,
  `medium-difficulty` (the lobby's middle difficulty; gameoptions has three), `late` and `space` (sim_capture's
  profiles: Middle Ages / Atomic Age start, 3 AIs, the original's cheats), `calamity` (--calamities) and `next`
  (--engine next). Each setting goes to build/rust_oracle/NAME-SETTING; the loaded game gets the continued game's
  options (cheats, casts and engines are not in the save); every game's stage slot is deleted when it ends and
  --clean deletes the saves, AI traces and reports once compared (a whole matrix run leaves about 6 MB). It ends with a
  table of the saves that continue identically per setting. Saves at 1000 and 1900 s (not multiples of 300 s, where
  the calamity casts fall). EE-SIM: a loaded game with --calamities casts on the continued game's five-minute schedule
  (it cast at its first tick before, a harness artefact).
- **First matrix** (seed 2, 1000/1900 s, 519 s wall at --jobs 2): oracle 2/2, islands 2/2, medium difficulty 1/2, all
  the others 0/2: plains-medium-3ai 3/168 ticks, highlands-large-4ai 149/82, mediterranean-easy 168/564, medium
  difficulty -/406, late 3/3, space 12/0, calamity 1/1, next 0/0.
- **The unit manager's empty class lists** (no format change). GetUnits returns an empty list for a class the player
  tracked and lost every unit of, NULL for one it never tracked; TryResearch resets the epoch advance (to retry it)
  only for the empty list. The 0.266 order lists every class the running manager holds, empty ones too: the first tick
  now makes an empty list for each saved class the load lacks and drops an empty one the save lacks. Found in the
  calamity game: a player whose town centre a calamity destroyed retried the epoch advance every second (a random
  draw each time, SetNextEpochAdvanceTime) where the loaded player never did (dumps' new `advflags=` and `rq=`).
  Calamity 1/1 -> 229/228 ticks (both then part at the next cast).
- **The next epoch advance time past due** (no format change). Save writes the time less the game time; the
  original's Restore took a negative delay for none: it dropped the advance under way and scheduled a new one, a
  random draw the running game never made. The next engine never sets the time (0, always past due), so every load of
  it lost its advance. The sum is now put back as it was (the unsigned arithmetic wraps back). Next 1900 s save 0 ->
  identical to the end.
- **Dead fishing boats in the gatherer lists** (ghosts, no format change). UnitLost calls CitizenLost only for the two
  citizen types, so a lost fishing boat (a naval citizen, class 109) stays, dead, in its resource manager's gatherer
  list for the rest of the game; the 0.261 order dropped such an id at the load. SaveGhosts now writes these units too
  (EECPOutOfWorldGatherers) and the gatherer lists' fixup takes the ghost for an id the world lacks (the players'
  fixups run before the ghosts' own, the players load before the world helper). Late 3/3 -> 822/484 ticks. Found with a
  temporary stack print in AssignGatherers (four dead class 109 units in player 4's lists).
- **0.272, a gather goal's carcass** (`EESaveHasGhostResources`). EEUGGather::Save wrote -1 for a resource no longer
  valid (a hunted animal eaten up while its gatherer still walks to it); the loaded goal had no resource and
  EECPRMFoodOutsideCenters counted one hunter less, so the food capacity (the dumps' new `cap=` and `food=`) and with it
  the wanted gatherers parted at the first resource update. The goal now writes the id, SaveGhosts adds the gather
  goals' resources and drop sites out of the world, every ghost record also holds its resource type and amount, and
  RepairPointers takes the ghost. Plains 1000 s 3 -> 1197 ticks.
- **The turret angles** (no format change). Save writes the model's part count of turret angles and Load reads them,
  but the load's LoadMedia makes the array again, all zero, after Load (the running game never sets mNumTurretAngles;
  only Load and RepairPointers' resize do). A loaded frigate's turrets pointed ahead, so FacesTarget failed and it
  turned to its target (FaceEnemyUnit, the dumps' `cmd=2012`) where the continued one fired (`cmd=2021`
  WaitForReload, a shell in the world). The angles read are put back with the other live state at the first tick.
  Space 12/0 -> 598/745 ticks. Found with a temporary print in FacesPoint (the angles 0 against -1.41 and 1.98).
- **Tools** (TEST ONLY, committed): the dumps' AI line shows the epoch advance's guards (`advflags=`, `later=`), the
  research queues (`rq=`), each resource's gatherers filed/wanted (`gath=`), the gather capacities (`cap=`), the food
  capacity's parts and lists (`food= fish= mines= minelist= food2=`) and the resource centers (`rc=`); each unit line
  its animation, frame, animation time, pause flag and reload timer (`anim= reload=`). Scratch under build/hunt:
  `pair7.py SETTING SEED T SAVE_MS` (a continued and a loaded game of one save of a setting, dumps at chosen ticks,
  the draw trace; the continued game's dump-load.txt is the saved state at SAVE_MS - 875), `rd.py` (the first differing
  draw of two draw traces, its callers resolved), `rs.sh` (exe-relative addresses through EE-SIM.map: image base
  0x400000), `dl.py` (a dump diff).
- **Lesson: a dead unit the running game still references is state.** Three of this round's four causes were the same
  shape as 0.262's dead attackers: a list or goal that keeps a unit out of the world (dead boats, an eaten carcass, a
  lost class's empty list). The save must say what the running game holds, dead or not; the ghosts are the way.
- **The numbers** (`saveload as7-fin --settings all --seeds 2-3 --at 1000,1900`, 1067 s wall at --jobs 2, after the
  merge of rust; END = identical to 2700 s, else the CT lines equal after the load; seed 2 1000/1900, seed 3
  1000/1900): oracle END/END END/END; plains-medium-3ai 1197/168 END/END; islands END/END 1429/400;
  highlands-large-4ai 149/82 4/0; mediterranean-easy 168/564 434/133; medium-difficulty END/406 END/0; late 822/484
  143/197; space 598/745 75/0; calamity 229/228 229/228; next 0/END END/END. **13 of 40 saves continue identically**
  (first matrix: 5 of 20 on seed 2; this round's fixes moved seed 2's plains 3 -> 1197, late 3/3 -> 822/484, space
  12/0 -> 598/745, calamity 1/1 -> 229/228, next 1900 s 0 -> END). `tools/saveload_expect.txt` holds these outcomes;
  `--expect` fails only when a save does worse.
- **Left** (each names its own cause with the same tools): the calamity casts (both saves part at the next cast,
  1200/2100 s: the cast after a load differs, oos only); the next engine's 1000 s save on seed 2 (a citizen of player 3
  gets an exploration route at the load's first tick: EECPNextExploring's state or the Lua step); the AI-only
  divergences (`oos, ai`: plains 2048 s, highlands 1973 s, mediterranean 2394 s); the 0-4 tick ones of seed 3
  (highlands 1000/1900 s, medium difficulty 1900 s, space 1900 s); late and islands on seed 3.
- **A gate for the merge chain**: `python -I tools/sim_oracle.py saveload NAME --settings oracle,calamity,late,space,next,plains-medium-3ai --seeds 2 --at 1000,1900 --jobs 2 --clean --expect tools/saveload_expect.txt`
  covers every cause fixed in rounds 6 and 7 (the oracle setting: the 18 saves' causes at two times; calamity: the
  empty class lists; late: the dead boats; space: the turret angles; next: the past-due epoch advance; plains: the
  carcass ghosts). Six settings x (1 continued + 2 loaded games): 18 games, 355 s wall at --jobs 2 on this machine
  (measured after the merge of rust: `saveload expect: PASS`; the 60-game run took 1067 s).

## Dead fishing boats leave the computer player's gatherer lists (a declared rebaseline; branch rust-fishboat, 2026-11-11)

The owner's declared rebaseline (RUST_PLAN 2.1: a deliberate behaviour change in its own commits, with new references).
Found by the save lane ("R5a.2, sixth round" and "seventh round": the late game's loads parted on dead class 109 units
in player 4's lists; round 7 saves them as ghosts so that a load matches).

- **The bug.** `EECPCivilizationManager::UnitLost` (0x470ed1) calls `EECPResourceManager::CitizenLost` (RemoveGatherer
  and a citizen's cost back on the needs) only for `kUnitTypeCitizen` and `kUnitTypeFemaleCitizen`, but the gatherer
  lists take every unit of the two citizen categories: `EECPUnitManager::AddAllUnits` and
  `EEComputerPlayer::UnitGoalDone` file land citizens (category 6) and naval citizens (category 22) with
  `AddIdleGatherer`, `UnitsSwapped` moves both. The units of those categories in the db: `citizen`, `female_citizen`,
  `x10_engineer` (category 6, neither citizen type) and the five fishing boats (`s02_fishing_boat_stone`,
  `s04_..._bronze` = class 109, `s08_..._imperial`, `s11_..._modern`, `s13_digital_fishing_boat`). A fishing boat
  that was sunk, converted or deleted stayed in its list (mostly the idle list: a boat goes there whenever its goal
  ends), dead and referenced, for the rest of the game. Every list sum counted it: `EECPRMWantedGatherers`' total
  (`of=` in the demand trace) and with it the wanted gatherers per resource, the idle list that AssignGatherers walks.
- **The fix** (EECPCivilizationManager.cpp, one `else if`): a lost unit of a citizen category that is neither citizen
  type leaves every list (`RemoveGatherer`, dropping the lists' references); no citizen cost is added (CitizenLost's
  `AddCosts` is the citizen's price). The citizens' path is unchanged.
- **The ghosts** (round 7's `EECPOutOfWorldGatherers`, `SaveGhosts`, the gatherer lists' load fixup) stay as they are:
  a fixed game's lists lose every unit the player loses, so it writes no gatherer ghost; a 0.272 save of an unfixed
  build holds its dead boats as ghosts and loads them back as that game held them (no format change, no version). One
  way is left for a dead unit to stay in the lists: a unit lost while its computer player is inactive (`SetActive`
  false: UnitTaken skips the managers, ReleaseUnits does not clear the lists); the ghosts still cover it. Comments in
  EESaveGameFiles.h and EECPResourceManager::Load say so.
- **Where it shows** (EE-SIM, the unfixed build of the same tree against the fixed, `--checksums`):
  - The sim oracle (2 AIs, seeds 2-7): **identical** to `ref`, every decision line and every checksum line (3086
    ticks a seed). Its computer players fish (the food lines' `fish=` up to 8 boats) but none loses a boat in 2700 s.
    `ref` is therefore **not** re-recorded: other lanes keep comparing against the main checkout's `ref` as before.
  - sim_capture: oracle, space and calamity profiles equal to their goldens on all six seeds; the late profile
    (Middle Ages start, 3 AIs, the original's cheats: navies) differs on seeds 2, 6 and 7 (seeds 3-5 equal).
  - The late game's first divergent tick (`ticks`): seed 2 CT 696 (game ms 609000), seed 6 CT 2940 (2572500), seed
    7 CT 1389 (1215375); each time `rng, oos, units`, the `ai` checksum still equal (it sums no gatherer list).
  - The cause at each: the state dump at the last equal tick (`--dump-at`, the AI line's `gath=` filed/wanted per
    list, round 7) shows exactly one entry more in the unfixed game's idle list (list 0), with the same number of
    living boats (`food=` third field, the unit manager's count): seed 2 player 4 10 against 9; seed 7 player 2 6
    against 5 (and the wanted wood/gold 8/6 against 7/5); seed 6 player 3 6 against 5 (wanted wood 7 against 6).
    The first differing decision of seed 2 is player 4's demand at 602687 ms, `of=23` against `of=22`: the dead boat
    counted as a gatherer. The unfixed build's late captures of seeds 2, 6 and 7 equal the old golden.
- **The rebaseline** (its own commit): `tools/sim_capture/golden_cpp_late.txt`, seeds 2, 6 and 7, from the fixed
  build (the tree merged with rust): every tag of those seeds changed (the game parts at 609/2572/1215 s: A* areas and
  searches, combat, formations, tech costs, unit store checks), which they can only through the game itself; no
  record format changed. CALC/CALS/CALU/CALW of seeds 2 and 7 and CBCN of seed 7 are no longer recorded (the new
  games cast no calamity, and seed 7 tests no conversion) and were dropped from the golden (144 lines changed, 9
  dropped): the late golden now holds no calamity record (the calamity profile still covers them all). The other
  three goldens and `ref` are unchanged. The captures were digested seed by seed and deleted at once (the disk was
  under 5 GB), with sim_capture's own digest functions (`build/fbhunt/cap.py`, scratch).
- **The load test.** The gate (`saveload --settings oracle,calamity,late,space,next,plains-medium-3ai --seeds 2 --at
  1000,1900 --expect`) and the whole matrix (`--settings all --seeds 2-3`, 1018 s wall at --jobs 2): 38 of the 40
  saves have the outcomes of `tools/saveload_expect.txt`; late seed 2, a new game from 609 s, went 1900 s 484 -> 794
  ticks and 1000 s 822 -> **0**. That save parts at the load's first tick on two citizens of player 2 that the
  continued game holds in an active group (`EEComplexUnit::mActiveGroup`, the dumps' `grp=1`; the unit checksum adds
  `mActiveGroup != NULL`), which no save version stores: the group a unit moves with after a group order is lost by
  every load. The old game had no unit in a group at its 1000 s save. Not a dead-boat cause, and not fixed here (it is
  the save lane's: the active groups in the save); the expect file's two late seed 2 rows are re-recorded (own commit)
  with a comment that says so, so the gate passes on the new game. Seen in the same dump, outside the checksums: the
  loaded computer players' resource lists (`mines=` 21 against 9, `fish=` 65 against 70 in the AI lines), rebuilt by
  the load where the running game refreshes them on its own schedule.
- **What other lanes do**: nothing for the sim oracle (`ref` is unchanged; compare against the main checkout's `ref`
  as before). A lane whose late capture check runs before it merges this branch sees seeds 2, 6 and 7 fail on every
  tag: merge `rust` (with this) first. The saveload gate needs the new expect rows (in this branch).
- **Gates** (the tree merged with rust at d1894b28): `build.py -j 3 --sim` 0/0 (EE-AOC, EE-SIM); the sim oracle seeds
  2-7 identical to `ref`; sim_capture oracle, late, space and calamity equal to their goldens (late: the new one); the
  saveload gate PASS with the new expect file; `rust_build.py gate` PASS (1750 tests); `--test-profile
  windowed-software` 0/0. No harness session (tours, AI games, LAN) was run; the fix is in the computer player's
  simulation only (EE-SIM covers it; no renderer or UI path changed).

## R5b.2, fourth round: the weapon point's other users, the flight steps, the screens and the unit-list search's sweep (ee_combat launch.rs, flight.rs, screen.rs; branch rust-combat4, 2026-11-11)

| Piece | C++ caller | Rust | Tag |
|---|---|---|---|
| the shot's launch transform, tile, tag point and flaming arrows | `EEUnitClass::LaunchShot` | `launch::launch_shot`, `launch_index` | CBLS (replayed) |
| the turret's launch | `EEUnitClass::LaunchFromTurret` | `launch::launch_from_turret` | CBLT (replayed) |
| the attach point | `EEApproachAttachPoint` | `launch::attach_point` | CBAP (replayed) |
| the projectile's placement on its arc, its tick | `EEPMProjectile::Step0`, `Step2` | `flight::projectile_launch`, `projectile_step` | CBPJ, CBPS (replayed) |
| the missile's boost and homing ticks | `EEPMMissile::Accelerate`, `Home` | `flight::missile_accelerate`, `missile_home` | CBMA, CBMH (no game records one: unit tests only) |
| the new target search's screens | `EEFOFindNewTargetInMeleeRange`, `EEFOFindNewTargetNearOldTarget` `Virtual04/08` | `screen::screen_accepts` over a distance callback | CBFS (replayed) |
| the hit distance's arithmetic | `EEGetHitDistanceSquared` | `screen::hit_distance` | CBHD (replayed) |
| the unit-list search (round 3's move) | `EESearchTargetAlongList` | `search::search_along_list` | CBSL, now from EE-SIM's end sweep |

- **Golden first.** Commit 7d8fe54a put TEST ONLY probes into the unmodified C++ (the record layouts as local C
  structs in `EECombatFormulas.h`, the decisions read off the original branches: the tag point index it took, whether
  the flaming-arrow branch ran, which early return Step2 took). The goldens were recorded from that commit with one
  more, temporary change: round 3's C++ `EESearchTargetAlongList` (from cfe840e7, its trace helper renamed) in place
  of the Rust-backed one, so CBSL is the C++ search's. Every earlier digest of those runs equalled the existing
  goldens in all four profiles. The moves then replaced the local structs with ee_combat's records (same layout).
- **All of this round's tags replay.** Each is a pure function of its record: `combat_replay` recomputes every
  CBLS/CBLT/CBAP (the record, the model chain's matrices, the output), CBPJ/CBPS/CBMA/CBMH, CBFS and CBHD byte for
  byte. CBSL is a world walk: gated by its digest.
- **The model chain once more.** `EECombatTagPointChain` (a template, so `EECombatFormulas.h` needs no model header)
  gathers a tag point's local point at a frame and its model chain's internal transforms, as round 3's line of fire
  did; `geometry::up_the_chain` walks it, `launch_from_turret` also composes the chain's transforms over the turret's
  turn for the launch's orientation (`EETagPoint::GetTransformation`). `EEUnitClass.cpp` now includes `lle/GEModel.h`.
- **The C++ literals are the std constants.** 3.1415927f, 6.2831855f, 1.5707964f and 0.31830987f have the bits of
  `f32::consts::PI`, `TAU`, `FRAC_PI_2` and `FRAC_1_PI` (clippy's approx_constant rejects the literals); the
  missile's dot product clamp stays the C++'s two tests (`allow(clippy::manual_clamp)`).
- **Laziness kept at the seams.** Step2 reads the ground point only at or below height 0 (`!(z > 0)`, NaN included,
  as the original's early return), the screens ask the distance (a drop point for the melee screen: CBDP and CBHD
  records) only for a living candidate (`ee_combat_screen` calls the host back and writes `read`/`distance` into the
  record for the capture).
- **The unit-list search's sweep.** No game of the four profiles reaches `EESearchTargetAlongList` (anti-air ammo
  only). EE-SIM now runs it once, when a captured game ends (`FinishGame` before `Finish`), over each player's newest
  64 armed units on the map (priests left out: a list unit without a complex unit reaches `CanAttackClass` through
  NULL there), at the unit's tile and two tiles off with 1.5 times the range. It must not change anything another
  record sees: `EESimCaptureRecord::Only(tag)` (thread-local) drops every other tag's record while it runs (the
  CBAF/CBLB/CBDP/CBHD records of the tests it calls), each unit's `mIgnoredTargets` is copied back after each call
  (`IgnoreTarget`, the expired entries' erase), and nothing simulates after it. 180 to 230 CBSL records a game.
- **The first CBSL comparison found a record difference, not a behaviour one.** Round 3's C++ probe wrote the
  search's `inScale` after the original's `inScale = theRange * inScale`, the Rust-backed record wrote the argument:
  every trace, best and score were equal (134 of 134 records of a seed, all but that field). The moved record now
  writes `in.range * inScale` (the golden is the C++'s). Round 3 had no CBSL golden, so this had never been compared.
- **Coverage.** CBLT comes only from the space profile (turrets); CBFS from none to a few hundred screens a game
  (absent in 3 late and 2 calamity seeds and every space seed); CBHD from every search's distance (100k+ a game).
  **No game of the four profiles flies a missile: CBMA and CBMH have no golden** and `missile_accelerate` and
  `missile_home` are gated by their unit tests only (a capture of a later profile with missiles would add them).
- **Process.** A second `build.py` started while the first still ran (the first's compile errors were printed mid
  build) collided on the object files (`PermissionError` unlinking an .obj): wait for the build's own end. A
  `rust_build.py header` piped into `tail` hid its failure and a commit went out without its header: check the
  header for the new names before building. The python `check` re-digests every capture file; the golden runs here
  digested seed pairs and deleted them right away (disk), and compared the digests with the golden files directly.
- **What stays C++:** the shot's creation, goal and launch, the closest tile, the turret part lookup, the drop target,
  the ground point, `EEUpdateUnitTile`, the projectiles' and missiles' ground and water impact tests
  (`CheckGroundImpact`, `CheckWaterImpact`), `DistanceSquaredTo`, the screens' objects.

## R6: WON's Internet code, round 3 (GetCertOp, WONMisc's HTTP, FTP and patch code, WONInternal's admin ops)

Branch `rust-won-net3`. As before, nothing in the game reaches this code except `URL` (the lobby's browser components parse their links with it, so the LAN game runs it): only the drivers exercise the rest.

- **Entries read up to the cut, checks applied in order.** `GetCertOp`'s login reply interleaves each entry's read with certificate, key block, session key and private key checks that return early. Rust (`cert.rs`) reads the whole reply into its entries in the original's order and ends with a cut marker where a read failed; `WONCertMsg.h` walks them and applies each check where the original did, so an early return leaves the reads before it and nothing after. A community status entry carries how many of its sub-message's fields were read (the record is pushed before its fields).
- **Stand-ins whose verdicts follow their bytes.** The cert driver's certificates, key blocks, keys and decryption decide validity from the bytes they are made from and log every call, so the original's statements and the template run against identical, deterministic environments; the trace and the members are compared whole. A first mutation (the community's id assigned only with its name) failed 22,197 checks.
- **The original crashed in places; the port defines them.** GetCertOp's secret commit read the other certificate's times (a NULL dereference when only one came) and passed an indeterminate duration when none came; the driver marks those runs and checks the new code's trace up to the crash point. `VersionBaseOp::IsFiltered`'s `unsigned int` positions made `substr` throw `std::out_of_range` for some version data (55,372 of 300,000 generated; the game would stop); the port answers.
- **The UCRT's scans, measured.** `sscanf("%d")` is `strtoll` saturating to 64 bits, then the low 32 bits (`"4294967295"` is -1, `"9223372036854775808"` is -1, `"-99999999999999999999"` is 0); `%x` is `strtoull` (saturating to `UINT64_MAX`, a minus sign negating), and `"0x"` with no digit after fails; `atoi` saturates to 32 bits. The first HTTP driver run caught the wrapping version; the values came from calling `ucrtbase!__stdio_common_vsscanf` through ctypes with a `va_list` of pointers.
- **Tool trap: the Bash tool's heredocs.** A `\\` in a heredoc reached Python as `\`, so `"\\r\\n"` matched a real CR LF, and an `index()` of C++ text with `"\r\n"` literals failed. Write such scripts with the editor and run them as files.
- **Stashing staged new files.** `git stash push -- . ':!build'` stashed only part and left the new files staged; the pop restored them staged, and the next commit took them all. Check `git show --stat` after a commit that follows a pop.
- **Network-layer APIs these call** (for the lobby lane): `AsyncOp::IsAsync()`, `ReadBuffer::SetPos`/`Available`, `ByteBuffer(data, length, false)`, `IPAddr::Fn2B2C20`, `WriteBuffer::AppendBytes`, `WonByteBufferSink`, `WideAppendUtf16LE`, `WideAssignUtf16LE`.

## R5b.4, ninth round: Build Unit, Smart Farmers and Cast Calamity in Rust, the store's positions and the checksum's flag 2, the tick's step mapped (ee_units commands.rs, store.rs; branch rust-r5b4-core9, 2026-11-11)

- **The last three goal-giving bodies** (`ee_units` commands.rs over host calls in EEUnitHost.cpp, the original's
  order and draw count; all 15 are now Rust):
  - `EEMBuildUnit::Execute` (`build_unit`): the technology's class (a node the player lacks is refused: the original
    read through NULL), the capitol count, `CanProduce(.., 0x20, 1, true, 3, NULL)`, `EECanPlaceOnTile(.., false, 1)`,
    `EEMakeBuilding`, for a granary the free farms (`EEFindFreeFarm` from ring 1 until none) sorted around the first
    unit's tile; each unit builds (queued: a build target, shift-queued, a unit with a goal other than a build goes
    idle; else `OrderRepair`) and takes the farm at its place (past the farms one drawn); a unit the order took out
    of the group is added back after the loop. The goal kind it tests is the store's.
  - `EEMSmartFarmers::Execute` (`smart_farmers`): the load goal's farm, its drop sites (`EEGetFarmDropSites`),
    `CanTake`, the drop, the nearest free farm (`EEFarmDistanceGreater`, the last taken), `EEWorkFarm`, the gather or
    the 2D move goal (a new constructor, `EE_GOAL_CTOR_LOCATION_2D`), the acknowledgement, the pending push, `Finish`.
    The carried amount and the tiles are the store's.
  - `EEMCastCalamity::Execute` (`cast_calamity`): the calamity classes stay C++ (the tech lane's): the class and its
    flags, `CanCastAt`, `Contains`, the power and the cost, the casters' split, the goal's `Clone` are host calls.
    The cast goal is a new constructor (`EE_GOAL_CTOR_CAST_CALAMITY`; `EeGoalSpec` gained `ints`: the tile and the
    technology). A node with no class is refused (the original read through NULL). A selected unit out of its list
    has its hit points read on the object (`object_hit_points`), as its type already was.
- **The sorts stay C++.** Both farm sorts are `std::sort` (unstable: equal keys keep the order the algorithm leaves).
  The comparators are inline in EEMessage.cpp, so the sorts are there too (`EESortFarms`, called by the host's
  `sort_farms` over the ids the body holds); the sort COMDAT annotations still name them.
- **The sweep** (USGC) gained the three bodies' original statements and cases: Build Unit with the technologies of
  the player's tree that make a granary, a house and a capitol and one it has no node of, at the first unit's tile
  and the centre, plain and queued; Smart Farmers at the usual targets and the player's first granary; Cast Calamity
  with its first unit and its first unit with power as casters, no target and the next player's unit, one calamity
  technology of each class kind (0x12, cast by all, other) and Teleport. The dry host answers `CanProduce`,
  `EECanPlaceOnTile` and `CanCastAt` "yes" (only the placement asked first, its real answer logged), makes a dry building
  (0x7e000002) whose farms are two dry ids, and runs the split's keep test on the objects.
- **A trap:** the dry `CanProduce` first asked the real one: its cost reads are the tech capture's TCST records, so
  the sweep added 3,000 TCST records a game. The dry host now answers without asking (like the hull reads, a sweep
  must not reach a captured function).
  The same after the merge with the tech lane's round 2: `CanCastAt` is now its CCAS record (20,000 more a game);
  the dry `CanCastAt` answers 0 without asking too. Every dry read of another lane's code can become a captured
  call when that lane adds a record: run the capture after each merge.
- **A trap of the environment:** a build moved to the background kept running after its first compile error while a
  second build of the same worktree started; both wrote the same log. Wait for a background build's own exit before
  the next.
- **The store's positions and flag 2's fields.** `EEComplexUnit::mPhysicalModel` is an `EEUnitStoreModel`
  (GEPhysicalModel's layout, no member of its own: its four location setters call the base and report x, y, z in
  one store call), and `mHeight`, `mTimesUpdated`, `mActiveGroup` (the record keeps whether it is set) and `mCommand`
  (the record keeps its `mType`: an action's type is fixed by its constructor) are holders. USCK compares the new
  fields and checks the checksum's flag 2 over the unit chain against the store's sum (`checksum_flag2`: each word
  sign-extended into a wrapping 64-bit sum, so the low 32 bits are the Windows `long` sum, all 64 the LP64 one).
  Facing (the transform's rotation) is not in the store: nothing reads it yet.
- **The writers the holders did not catch.** The first USCK run stopped at tick 0 of a later minute: the location z of
  one unit differed. `EEUCProjectile` and `EEUCMissile` wrote a dying shot's location through
  `const_cast<GETransformation&>(GetLocation())`; they now call `SetLocationXYZ` (the same three stores in the same
  order). `EEPMBuilding::Step2` set the location through a `GEPhysicalModel&`, which a derived class's setters do not
  intercept: the reference is now the reporting type. A base reference or a const_cast is the hole in a
  "shadowing" holder: grep for both. `mTimesUpdated` is never incremented in the game (reset, loaded, saved).
- **Flag 2 from the store.** `GetOutOfSynchChecksum` adds `ee_units_store_checksum_flag2()` for flag 2 and walks the
  chain only for 0x20 and 8; the oracle's `oos=` (flags 0x153) is identical.
- **What only a played game runs:** the three bodies' world calls (no capture game sends Build Unit, Smart Farmers or
  Cast Calamity; the computer players build through their own code): the building's placement, the orders, the farm
  assignments, the casts and the split. The UI sends Build Unit on every building placement (EEABuild), Smart Farmers on
  a right click on an own granary, or on a farm with several citizens selected (EEAMouseClick), Cast Calamity from
  EEACalamity: a played game (the lead's) is the first to run them; the fixture's orders do not send them yet.
- **The tick's step, mapped**: RUST_PLAN "Leaf 4 (c) and leaf 5, ninth round" (the call tree, the fields each piece
  touches, what the store lacks, the next three rounds).
- **Gates** (each code commit built 0/0 for EE-AOC and EE-SIM, the oracle identical on seeds 2-3, the Rust gate PASS;
  on the merge with rust: the oracle identical on seeds 2-7, the four capture profiles (24 games, run two seeds at a
  time for the disk) equal to the goldens but USGC, which the last commit recorded, and the combat lane's CBCH
  lines, which have no golden on rust; USCK found no difference; the test-profile build 0/0). The Rust gate's
  timer test `timers_under_a_factor_fire_at_the_scaled_rate_until_stopped` failed once under load and passed
  alone. Not run: any harness session, macOS or Linux builds.

## R6: WON's socket classes and IPAddr as Rust objects (ee_won sockobj.rs, sockop.rs, qsock.rs, ipobj.rs; branch rust-r6-won14, 2026-11-11)

Round 14 of R6 (RUST_PLAN "R6 WON, the rest"), the third round of whole-class moves: the socket layer under the
lobby's LAN game now keeps its state in Rust objects; the C++ classes are shims while C++ callers remain.

- **Accessors first, again.** AsyncSocket, SocketOp and its ops, QueueSocket and LobbyLanSocket made their members
  private and the compiler listed the sites; `build/w14/fix.py` rewrote them by (class, member) from the error's
  class name, so the many other classes with an `mSocket`, `mDataCrit` or `mLengthFieldSize` of their own were left
  alone. Header-inline code reports its errors with the header's path (the script's pattern took only `src/` paths:
  those few were edited by hand), and a derived op of the same family (RecvMsgOp, SendMsgOp) is the base's
  `friend` rather than growing accessors for every buffer. Files touched outside the lane (mechanical, accessor
  calls only): EEWonNet.cpp; WONMisc DetectFirewallOp, HTTPGetOp; WONRouting RoutingConnection,
  SmartRoutingConnection(+Commands, +Snapshots); WONServer ServerConnection, ServerContext (IPAddr's conversions);
  the lobby's InitLogic, LanLogic, RoutingLogic, EELobbyFileTransfer; WONSocket's own units.
- **What moved.** `EeWonAsyncSocket` (sockobj.rs): the socket's core, length field size, message transforms (the
  C++ `MsgTransform` objects, owned) and the socket thread's close callback, and every step of AsyncSocket.cpp,
  under the C++ `mDataCrit`'s mutex (the subclasses lock it around their own state too, so the C++ keeps the
  `CriticalSection` and the object borrows its mutex). `EeWonSocketOp` (sockop.rs): every socket op's base (its
  socket and the reference, the thread flag, the wait flags and event indices) and `RunHook`; RecvCRMsgOp's line.
  `EeWonQueueSocket` (qsock.rs): the queue, the queued and repeat ops, the four completions and the close timer
  (a reference each, dropped under the lock where a C++ `SmartPtr` assignment dropped it), the queued-close flag and
  every step of QueueSocket.cpp; the ops and completions it makes (`ParamCompletion`, `QueueOpFastCompletion`,
  `QueueCloseOp`, `CloseOp`, the timer) come from C++ hooks. `EeWonLanSocket`: LobbyLanSocket's client (a
  reference) and time, and `SetClient`'s two-way link. `EeWonIpAddr` (ipobj.rs): IPAddr's value; the C++ class is a
  handle whose copy constructor clones, assignment copies and destructor frees the value (every copy site copies the
  whole value, as the original's implicit copy did); `mLocalHost` is Rust's. BlockingSocket has no state (each call
  makes one op and runs it blocking): nothing to move. Deleted C ABI: `ee_won_socket_init` .. `_set_nagling` (all
  but start / stop), `ee_won_op_send_all`, `ee_won_op_recv_line`, and the IPAddr helpers the C++ called.
- **Goldens, recorded first.** `won_sockobj_test` (new, `xbuild wontest`): 319 lines over 127.0.0.1, recorded over
  the C++ classes before each move (the IPAddr part before IPAddr's): AsyncSocket's steps and hooks, the
  transforms' order and refusals, the ops blocking and async, each op's Duplicate, QueueSocket's queue, repeat op,
  QueueOpFast, queued close, close timer and hooks, and IPAddr's parses, texts, wire form, order, copies and local
  host. The same over the Rust objects, run after run. Driver traps: MSVC's ABI evaluates `"x" + St(op->Run()) +
  St(op->GetStatus())` right to left (the status was read before the run: `LOG2` evaluates the call first); a
  socket op's repeat copy is queued in the op container, not started, until the next pump (pump before changing
  the socket under it, or two ops race to fail); a datagram longer than the buffer is the system's (WSAEMSGSIZE on
  Windows, cut on POSIX) and the setsockopt error of a closed socket too (`WSAENOTSOCK` / `EBADF`): left out so the
  golden holds on every system; an op with no socket is not run (RunHook finishes it and starts it anyway: every
  start reads the socket). LobbyLanSocket needs the lobby's headers and units, so only the LAN game runs it.
- **Found: an unset sockaddr.** `IPAddr(const std::string&)` on a text without a colon set only the remember flag
  and the validity: the sockaddr was whatever the stack held (the C++ golden line showed the previous temporary's
  10.1.2.3:8080, the same run after run). The Rust value starts at zero; that one golden line was re-recorded.
- **Behaviour changes.** That address is 0.0.0.0:0; RecvCRMsgOp on a socket-less op returns
  `WS_AsyncSocket_InvalidSocket` instead of reading a null socket; LobbyLanSocket's last message time starts at 0
  (the original left it unset); every IPAddr is a small heap value (a copy allocates). Nothing else: the same
  locks, threads and order.
- **What runs it.** EE-SIM never loads the lobby, so the oracle cannot see a fault here. The lead's LAN game
  (`lan_game.py`) is the end-to-end check, and it runs every moved class: the lobby start-up (IPAddr's local
  address, the adapters), LanLogic's discovery socket (a datagram QueueSocket: RecvBytesFromOp as the repeat op,
  SendBytesToOp broadcasts, IPAddr from the sender), the host's listening LobbyLanSocket (AcceptOp as the repeat op,
  the accepted socket's Duplicate, SetClient), the joiner's ConnectOp, the game room's messages (RecvMsgOp repeat
  ops and SendMsgOp queued ops with the 2-byte length field, through the socket thread and the op container), the
  close timers and queued closes at the start, and the sockets' destruction.

## R6: WON's socket ops, AsyncSocket's lock and destination, and UDPManager as Rust objects (ee_won sockops.rs, udpmgr.rs, sockobj.rs; branch rust-r6-won15, 2026-11-11)

Round 15 of R6 (RUST_PLAN "R6 WON, the rest"), the fourth round of whole-class moves: the socket layer under the
lobby's LAN game now keeps all of its state in Rust objects.

- **The golden first.** `won_sockobj_test` grew from 319 to 452 lines, recorded over the unmodified C++ before any
  move (commit `R6 WON: won_sockobj_test's round 15 golden`): the destination's set, copy and connect; the data
  lock's recursion and a second thread waiting on it; a 2 MB send continued on the socket thread while the receiver
  reads blocking (progress chunks); the chunks' bytes read; re-runs of a timed-out RecvBytesOp and RecvMsgOp; file
  receives (append, text mode, the modify time, the path); the message length limits; refusing transforms on each
  side; connect and accept async, an invalid address, a refused connect; datagram ops on a connected socket; and
  UDPManager with its receive and request ops (unbound kills, the auto-receive op, address and address-less ops with
  checks, WS_ServerReq_Recv's new op, a time-out, the blocking refusals, Send, a request through the manager with a
  wrong then a right reply, retransmits to WS_NoReply, the ops' own sockets, Close, a rebind). The same 452 lines
  over the C++ five times and after each of the three moves (4 more runs each). `won_ops_test`'s units now include
  UDPManager.cpp. Driver traps: an op's RunAsync only queues it in the op container (pump once before blocking on its
  peer, or the peer waits for a send that has not started); two completions finishing on two threads (the send and
  the receive of one transfer, a connect and its accept) come in either order: count them and log afterwards.
- **Found: the re-run quirk.** A RecvBytesOp run again after a time-out keeps its write buffer: the second
  `StartRecv` reserves the same length, so the receive reads only the room left, then asks for 0 bytes, which the
  system reports as a close (`WS_AsyncSocket_Shutdown`); a RecvMsgOp re-run reads the old prefix bytes as the body
  (`WS_RecvMsg_InvalidMessageLength`). The original's behaviour, kept and in the golden.
- **AsyncSocket's lock and destination** (sockobj.rs). EeWonAsyncSocket owns the recursive lock (created with the
  object, destroyed after its close) and the destination (an EeWonIpAddr under the lock); `Fn2B0E60` is Rust's whole
  (`ee_won_asyncsocket_connect_to`), Accept's adopt sets the accepted socket's destination from the peer. The C++
  keeps `GetDataLock()` (the mutex), `GetDestAddr()` (now a copy: `IPAddr::FromValue` adopts a Rust value) and
  `SetDestAddr`. `AutoCrit` holds a `PlatMutex*` and takes a CriticalSection or a mutex. Mechanical edits outside
  the lane (`GetDataCrit()` -> `GetDataLock()`): EEWonNet.cpp; WONRouting RoutingConnection, SmartRoutingConnection
  (+Commands, +Snapshots); WONServer ServerConnection; QueueSocket. LanLogic's join check held a pointer into the
  temporary that `GetDestAddr()` now returns: it keeps a copy (look for the same shape when a getter turns from a
  reference into a value: `x->Get().Bytes()` kept past the statement).
- **The socket ops** (sockops.rs). SendBytesOp, SendMsgOp, SendBytesToOp, RecvBytesOp, RecvMsgOp, RecvBytesFromOp,
  ConnectOp and AcceptOp keep their state in an `OpKind` inside their EeWonSocketOp (the bytes and position, the
  message and its transformed copy, the destination, the receive's write buffer, length, count, skip flag, file,
  path, file flags and modify time, the progress completion, the message stage, the datagram buffer and sender, the
  connect address, the accepted socket), and every step of SocketOps.cpp and SocketOperationDispatch.cpp runs in
  Rust. The C++ classes are shims (constructors, Start/Continue, accessors, Duplicate). What stays C++ is in
  SocketOp's one hook table: the `RefCount*` references (ByteBuffers, the progress completion, the accepted socket),
  a ByteBuffer's bytes and a new one adopting Rust's, the socket's transform chain on a message slot, the target
  file (PlatFOpen's modes, fwrite, fclose, utime), the progress chunk (a RecvChunkOp in the op's mode; its completion
  reads the op's bytes so far, so no borrow of the state lives across it), and `AsyncSocket::Accept` (the virtual
  Duplicate). `ConnectOp::GetAddr`, `RecvBytesFromOp::GetAddr` and `RecvBytesOp::GetFilePath` return copies.
  RecvMsgOp's constructor takes a tag type: a `(AsyncSocket*, bool)` overload was ambiguous with
  `RecvBytesOp(0, NULL)`.
- **UDPManager** (udpmgr.rs). EeWonUdpManager holds the socket, the default completion, the auto-receive op, the
  address-less list and the by-sender multimap (the address's wire order, so the port first; an equal address after
  those already there) with their clean-up cursors, and does Bind, Close, SetAutoRecv, QueueOp, Send, CleanOps, the
  iterator-keeping erases and RecvCallback's dispatch. The iterators are the entries' ids (end is none), so a cursor
  survives inserts and erases as the original's node-based containers allowed, including the new op that
  WS_ServerReq_Recv queues into the map during the walk (the walk reaches it, as the golden shows). Like the
  original it takes no lock. The ops it holds (`UDPManagerRecvOp`, `UDPManagerSendRecvOp`) stay C++: they are
  AsyncOps the lobby subclasses (PingGameOp, RoutingGetUserStatusOp) with public members the lobby reads. Mechanical
  edit outside the lane: PingLogic's `mDefaultCompletion =` became `SetDefaultCompletion` (UDPManager.cpp and the
  ops are the Internet lane's file, but entirely socket plumbing: moved here by agreement).
- **Behaviour changes.** An op without a socket returns `WS_AsyncSocket_InvalidSocket` (the original read a null
  socket); a progress interval of 0 resets the count (the original divided by zero); `UDPManager::Send` while
  unbound does nothing (the original used the null socket); `GetDestAddr` and the ops' address getters are copies.
  Nothing else: the same locks, threads and order.
- **What runs it.** EE-SIM never loads the lobby, so the oracle cannot see a fault here. The lead's LAN game is the
  end-to-end check; it exercises: the discovery socket (a datagram QueueSocket: RecvBytesFromOp as its repeat op,
  `RecvBytesFromOp::RunHook`'s buffer, the datagram and its sender; SendBytesToOp broadcasts on an unconnected
  socket, SendTo), the host's listening socket (AcceptOp as the repeat op, the hook's Accept and Duplicate, the
  accepted socket's destination from Rust's adopt, which LanLogic's join subnet check reads through GetDestAddr), the
  joiner's ConnectOp (`connect_to`, the destination, the wait), the game room's RecvMsgOp repeat ops (the 2-byte
  length prefix, `StartRecvBody`, `FinishRecvMsg`, no transforms on LAN sockets) and SendMsgOp queued ops, the locks
  of QueueSocket and the sockets through `GetDataLock`, and the lobby's UDPManager (LobbyBindUDPPort's Bind with
  broadcast, SetAutoRecv, the repeat receive, PingLogic's default completion for unsolicited datagrams; a game ping, PingGameOp's request through the
  manager's Send and its reply op in the map, runs only for a direct-connect game or the game list's Ping action,
  which lan_game.py does not use). RecvBytesOp with a file, the progress chunks, CloseFile and SendBytesOp alone are reached by the lobby's file
  transfer and the Internet code only.
- **Left.** UDPManagerRecvOp and UDPManagerSendRecvOp (C++ AsyncOps over the manager); BlockingSocket has no
  state; the lobby model and logic (LobbyModel 1587 lines: LobbyClient, LobbyFriend, LobbyGroup, LobbyServer,
  LobbyServerList, LobbyGameList; LobbyPersistentData 1039 + its accessor units, whose layouts and rules are
  persist.rs's since round 6: its members are read by about 60 `Lobby*` free functions, so the accessors-first pass
  comes before its move).

## R6: WON's Internet code, round 4 (TitleConfig, peer authentication and AuthSession, FindPatchOp, DetectFirewallOp, RegAuth)

Branch `rust-won-net4`. As before, nothing in the game reaches this code (the WON servers are gone; the LAN game does no peer authentication): only the drivers exercise it, and no tour, LAN game or EE-SIM run does.

- **Hooks for the objects, Rust for the protocol.** Peer authentication's messages, states, checks and statuses moved whole (`peerauth.rs`); what the handshake does to the certificates, the peer data's ElGamal keys and the two secret Blowfish objects goes through 11 hooks (decrypt, make the other side's certificate and test it, verify, expired, encrypt, own certificate, create, set and read a secret, make the session). The C++ classes keep those members and copy the six state members into an `EeWonPeerState` for the call and back after it (no hook reads them). Every hook is a statement of the original, in its order, so a message cut short leaves what the original left when its `ReadBufferException` came: secret A created, secret B set, the certificate made, then the throw.
- **AuthSession wholly in Rust.** Its members were protected and read only through getters, so the class is a `RefCount` shim over `EeWonAuthSession` (type, id, key, sequences, last use, both framings). `GetKey()` returns the Rust key as a `Blowfish&`: the C++ `Blowfish` is one `EeWonBlowfish` member, so the two are pointer-interconvertible. The shim passes `time(NULL)` in, keeping the clock where it was. The Rust type is named `EeWonAuthSession`: cbindgen would otherwise emit a `struct AuthSession;` that clashes with the C++ class.
- **Bare `pub const`s leak into the header.** The first peerauth build exported `AUTH_TYPE_SESSION` and the status constants as C++ `constexpr`s at namespace scope; the driver, which also sees `WONAPI::AUTH_TYPE_SESSION`, failed with "reference is ambiguous". Module constants are `pub(crate)` now (the rule in the agent notes, met again).
- **Stand-ins whose verdicts follow their bytes, again.** The peer-auth driver's keys decrypt (drop a byte, unmask) or fail by their first byte, encrypt or fail by length, certificates are valid, expired and verified by their bytes, and secrets are created from a counter; each logs its calls into its owner's trace, and the Rust replay reimplements them to check the golden's traces. A copy of a stand-in peer has to rebuild its certificates (each holds a pointer to its owner's trace); the first run shared them and compared one side's trace against both.
- **The original read past a short secret.** `HandleChallenge1` read the decrypted secret's two length bytes whatever its length (and the maximum wrapped below 2); the port calls such a secret an invalid length. The driver stops comparing a scenario there (11,857 of 150,000 client scenarios).
- **An equivalent mutation.** Setting the server's state to "awaiting challenge 2" before the request is read instead of after made no difference: every failure, thrown or returned, sends a complete that resets the state. A second mutation (the session id read for persistent sessions too) failed 19,471 checks and the Rust golden.
- **A compare that is the title's.** `FindPatchOp`'s lists call the op's `mVersionCompare`, a function pointer a title may set; the Rust choice calls it back in the original's order and count (the driver logs every call over four compares: the default, numeric, reversed, by length).
- **What moved whole, and why the rest waits.** `AuthSession` moved whole (its state was private and read through getters only). The others stay C++ objects over Rust logic: `TitleConfig` and `CommunityConfig` are `ConfigObject`s (the C++ parser calls their virtual hooks and binds their keys to members); `PeerAuthClient` and `PeerAuthServer` hold C++ certificates, the peer data and ElGamal keys (Rust objects only once `AuthCertificate`, `AuthPeerData` and `ElGamal` are); and every op (`HTTPGetOp`, `FTPGetOp`, `FilePushOp`, `FindPatchOp`, `DetectFirewallOp`, `PeerAuthOp`, `PeerAuthServerOp`, `GetCertOp`) is built from the socket ops the lobby lane's round 15 is moving now (`SendBytesOp` ... `AcceptOp`, `RecvBytesFromOp`, `UDPManager`): their state machines, `HTTPEngine`'s connection queue and `FilePushOp`'s transfer are a chain of those ops' completions, so they move as Rust objects after that round lands, not before (editing them now would collide with it).
- **Network-layer APIs these call** (for the lobby lane): `ReadBufferException`, `WonByteBufferSink`, `ByteBuffer(data, length, false)`, `Blowfish::GetKey`/`GetKeyLen`/`Create`/`SetKey`, `IPAddr::Fn2B2C20`, `RecvBytesFromOp(255, socket)`, `RunAsync`, `Run`, `GetAddr().GetHost()`, `AsyncOp::TimeLeft()`, `GetMode()`, `Track`.
- **HTTPCache's index on the Rust file layer.** `cachemap.rs` opens, reads and writes `cachemap.dat` with `file.rs`'s `FileReader` / `FileWriter` state (over `PlatFOpen`, as the C++ classes were); the C++ keeps the entry objects and maps (built from each entry read whole, as the original inserted it before reading the next) and the deletion of dropped files (a callback per index in the id map's order, between the writes, where the original deleted them). `HTTPCacheEntry::ReadFromFile` and `WriteToFile` had no other caller and are deleted. The driver writes each generated cache through the original's statements and through Rust into two directories and compares the files and the deletions, then reads the same (cut, mutated, foreign-version or missing) file through both and compares the maps.
- **A 2-byte length is unsigned in a file.** `FileReader::ReadLength(2)` reads an unsigned 16-bit field, where `ReadBuffer`'s is sign-extended; the first unit test's reader assumed the message rule.
- **Tool trap: a crash without a word.** The cache driver ended with 0xC0000409 and no output: its own `fwrite` to a file it could not open (a directory that does not exist, on purpose) reached the CRT's invalid-parameter handler, which fails fast. Check `fopen` in drivers too.

## R6: WON's Internet code, round 5 (AuthContext's sessions and certificate flow, HTTPEngine as Rust objects)

Branch `rust-won-net5`. As before, nothing in the game reaches this code (the WON servers are gone; the LAN game
does no peer authentication and fetches no HTTP document): only the drivers exercise it, and no tour, LAN game or
EE-SIM run does.

- **The pattern: a Rust object over C++ RefCounts, the C++ lock kept.** `EeWonAuthFlow` (authflow.rs) holds what
  AuthContext's sessions and certificate refresh were (the session map by the address's 6-byte wire form and its
  clean-up cursor, the waiting LightGetCertOps by address, the GetCertOp, the auto-refresh timer, the refresh
  status and flag); `EeWonHttpConn` and `EeWonHttpSession` (httpengine.rs) hold HTTPConnection's socket, queues,
  running ops, saved completions, pipelining counts and retry mode, and HTTPSession's connections by host
  (`StringLessNoCase`: the upper-cased bytes, signed, then the length). The objects they hold (sessions, ops,
  completions, sockets, connections, peer data) stay C++ RefCounts reached through one hook table per object;
  Rust holds one reference each and drops it where the original's SmartPtr assignment dropped it. The state is
  behind a Rust `Mutex` taken only between hooks, never across one: a kill, a run or a finish can complete the op
  inside the call and its completion comes straight back into the same object (the timer into
  `auto_refresh_callback`, the request into `get_cert_callback`, a connection's op into `op_callback`, a user's
  completion into a new `AddOp`). AuthContext keeps taking its CriticalSection where the original did; HTTPEngine's
  locks are taken through hooks at the original's points (OpCallback runs mostly unlocked, as before).
- **Walks that survive inserts.** The original walked node-based containers while completions inserted into them
  (a waiting op's completion starting another refresh during GetCertCallback's walk; a user completion adding an op
  during KillAll's or Kill's walk): a walk reaches what was inserted past it. The Rust walks re-read the live
  container each step ("the next key after this one", or the next index of a queue), so they reach the same
  entries; the goldens contain both cases.
- **Goldens, recorded first.** `won_authctx_test` and `won_httpengine_test` (new, `xbuild wontest`) run generated
  scripts over stand-ins that log every call, creation and free (ops whose run is held by a "thread" list until the
  script ends it, completing inside a kill or a run, user completions that re-enter, sockets, sessions, peer
  data), allocated in creation order from an arena so the address-ordered sets agree; runs of consecutive frees are
  compared sorted (a map's clear frees in its tree's order). The original's statements were copied into the
  drivers and their traces recorded (`golden_authflow.txt`, 400 scripts; `golden_httpengine.txt`, 100 scripts)
  before the Rust objects existed (the drivers' Rust halves off); the Rust tests replay them over Rust stand-ins
  that behave the same. Each driver then compares the original's statements with the Rust object over the same
  stand-ins on 100,000 scripts. The first authflow generator never put a session exactly 300 s old: two boundary
  mutations (`>= 300`, `<= 300`) passed; steps that move the clock by 298-302 s and put sessions that old were
  added and the golden re-recorded (from the original's half); both mutations now fail. The first HTTPEngine
  generator never turned pipelining off (three retries without three replies): busy scripts with sends that end
  with a new socket or none were added.
- **clippy: raw-pointer arguments.** Public methods taking `*mut c_void` fail `not_unsafe_ptr_arg_deref`; the
  objects' methods are `pub(crate)` (the C ABI functions are `unsafe`). A test stand-in printing a `c_long` as
  Windows' 32-bit value needs the cast on LP64 and `allow(clippy::unnecessary_cast)` on Windows.
- **Behaviour changes.** None intended: the same calls in the same order and count (the drivers' traces), the same
  locks. Release timing inside a call can differ by a statement (Rust drops a reference after its own lock is let
  go); destruction order within one clear is the map's order instead of the tree's.
- **Not moved this round.** GetCertOp's object (its members are a C++ Blowfish, AuthPubKeyBlock and AuthPeerData,
  which CertMsg's templates and the callers read: it follows those classes), HTTPGetOp's and FTPGetOp's state
  machines, FilePushOp, PeerAuthOp, PeerAuthServerOp, FindPatchOp and DetectFirewallOp as objects, the routing and
  DB op families, ServerRequestOp.
