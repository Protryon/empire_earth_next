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

## The next computer player's per-unit Lua interface (2026-11-09, docs/AI_LUA_UNITS.md §11)

- **Computer players have no slot of their own.** `RTSPlayer::mSlot` is `0xff` (`kSlotNone`) for every computer
  player, and `EEWorld::GetPlayerBySlot` returns the first player with the slot asked for. A human's message run for
  a computer player (`EEM*::Execute` finds its sender through the originating slot) has to give the player a slot
  that no peer uses (`0xfd`) while it runs, and put `0xff` back.
- **A building may start as a capitol.** The start town is not always a town centre (`b_capitol` in the open tree's
  Small Hard games). Code looking for "the town" asks for both classes.
- **`ai.tile` shows a continent only where the player has explored.** A test that places a fight towards the enemy
  cannot check the far tiles' land mass (they are unexplored): it takes them on trust.
- **The Python edits of a `-text` file must keep its line ends.** The repository's code is LF with `attr/-text`.
  Python's text mode on Windows writes CR LF, and git then shows the whole file as changed: open with
  `newline=''` both ways.

## The next computer player's army in Lua and the Age of AI port (2026-11-09, docs/AI_LUA_UNITS.md §11)

- **Projectiles are units of their owner.** A hunter's spear (`citizen_projectile`), a bomb, a missile or a torpedo
  in flight is an `EEUnit` in the player's unit list (category 9, 11, 12, 20) with hit points 1 and a speed.
  `ai.units{category = "soldier"}` listed them (the category fell through to "soldier"), boom's home guard took
  them and lost them a second later. The interface skips them everywhere now (`EECPUnitsIsProjectile`).
- **A library's memory keys must not clash with the script's.** `lib.groups` kept the enemies' gather spots in
  `mem.spots`, which boom already used for its own site spots (strings): `ai.keys` raised on the first step. A
  library's state lives under its own prefix (`mem.gspots`).
- **Only upstream's unit training was budgeted.** Age of AI's budget hook wraps the unit manager's training call;
  its buildings spend from the stockpile as the game's build manager always did. The port's first draft hid the
  military account from the citizens *and* the buildings, so no barracks was ever paid (the account held the wood).
  The military account is a ledger of what the army may spend; the civil side sees the stock above the epoch
  reserve.
- **Mineral worker targets by raw shortfall swing.** With need = reserve - stock, a mine's target drops to 0 the
  moment the stock crosses the reserve and the gatherers walk to the next mine, then back. A standing margin (half
  `mineral_worker_margin`) and a hysteresis of 3 a pass stop the swing.
- **The view's `sites[].resource` is a name,** not an index: `V.resources[s.resource + 1]` is an error.
- **A human-placed building is pending until a builder arrives.** `EEMBuildUnit` makes the foundation as a pending
  unit of the player (`GetPendingUnit`), not in `gWorld`'s lookup: `GetUnit(id)` is NULL until it is placed. A
  script that tests "is my foundation still there" through the world lookup sees it gone and orders another at the
  same tile (the pending one is not on the map, so the placement test passes). The interface looks in the pending
  list too.
- **A human-placed granary has no fields.** The AI build manager's granary (`CanBuildAt(..., farm, size * 3)`)
  reserves and lays the eight field foundations; a granary built through the human's build message does not.
  `EEMReplantFarms` (the human's replant command, `ai.replant`) lays the missing ones; `build b_farm` only ever
  finishes a foundation that exists (the classic `FindSite` returns a farm foundation with 1 hit point).
- **A building site at the map's edge crashed the game when a builder arrived.** `EEUGBuildRepair::BeginRepair` ->
  `ClearBuildSite` -> `EEStepAsideFromSite`, and `EEUGBuildRepair::IsFootprintClear` -> `EEGetMovableTileUnits`,
  walk the site's footprint tiles and dereference `GetTile(x, y)`, which is NULL for a tile off the map. A granary
  the Age of AI port placed through the human's build message (`ai.build`, placement by `EECanPlaceOnTile`) at an
  island's edge had such a footprint (Small Islands seed 11 at 49 min, deterministic: `python -I
  tools/sim_bench.py --name x --seeds 11 --data open --ai-scripts ai --engine ageofai,classic --maps "Small
  Islands" --game-seconds 5400`; `EE-SIM FAULT c0000005` in the run's err.txt, the frames resolved against the
  .map). A human's build order runs the same code. The three functions skip a NULL tile now (and `EEStepAsideFromSite`
  a unit that is not valid, has no tile or no stats; `BeginRepair` clears no footprint for a site not placed yet).
  The "placement skips a removed unit (PathPoint)" lines in the same seconds were the group move's path markers,
  not the cause.
- **A sim bench shares the main checkout's open data tree.** `sim.py --data open` takes `build/open_data/tree` of
  the main checkout when the worktree has none; while the main checkout's gates rebuild that tree every game fails
  at start (`EETerrainColorTable.cpp (81) : Verification failure`, `rc 2147483651`, no report). A worktree that
  benches builds its own tree.
- **A bash heredoc over about 140 lines fails in the harness shell** ("unexpected EOF while looking for matching
  `''`"); long patch scripts go into a file first.

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

- **A wall tech is never "available" to a computer player (2026-11-10).** `EEPlayer::CanProduce` (the human UI's test
  before `EEMBuildWall`) says no for a computer player's wall and gate techs even in their epoch, and the classic wall
  manager never asks it: it sends the message as it is, and `EEPlanWall` / `EEBuildWall` check the placement and pay the
  pieces. The next engine's `ai.wall` / `ai.gate` therefore check the tech node and `CanAfford` only (the first version
  refused every wall `cannot_afford`). `EECPCivilizationManager::GetWallClass` returns 0 before the Copper Age: the
  `"wall"` alias is `bad_argument` there, a Prehistoric palisade goes by its class name.
- **An intermittent units-hash divergence after a direct build (open, 2026-11-10).** Small Islands seed 11, the port
  against classic Hard, two runs: identical traces for 65 minutes, then the `sim hash` units value differed (AI and RNG
  equal) ten seconds after `ai.build` placed a granary with three held citizens (`build_direct`) and the classic
  helper joined (`build.help sent=1`). A second 90-minute pair diverged the same way ten seconds after the same
  granary at the same tile (114,58), and the navy port's agent saw it once as well after a direct granary; a pair cut
  at 70 minutes was identical. In both pairs only the units hash differs from then on: the RNG and AI hashes stay
  equal to the end and the final states (citizens, army, epoch) are the same, so what differs is a hashed unit field
  that the game's decisions never read (or the units' order), not the play. The suspects are the build message's
  field assignment round a granary (EEMBuildUnit: EEFindFreeFarm's order, the std::sort ties) and the helper's pick;
  a human placing a granary goes through the same message.

- **The micro step's instruction budget (2026-11-10).** A per-unit module that queries the engine once per citizen
  (`ai.seen` near each, `ai.units` round its site, `ai.unit` of the site) blows the 250 ms micro step's 300,000
  instructions once the town has 100+ citizens: the step aborts with `instruction budget exceeded` and every module
  after it in that tick is skipped (the safety port cost 1,900-8,900 aborted ticks a 120-minute bench before it was
  rewritten). The shape that fits: one `ai.seen` of the enemy soldiers near home per step, their tiles read once with
  `ai.get`, the distances in Lua, and the engine asked about a site only when a Lua test has already singled it out.

- **An income share accumulated a step at a time truncates to nothing (2026-11-10).** `income x dt x share // (60000 x
  100)` with a one-second step and an income under 150 a minute is 0 every step: the Age of AI port's military account
  never grew past its start share in any bench so far (`mil=240/225/240/30/30` in every trace), and the epoch account
  of `copper_plan=next_epoch` filled only on income spikes - half of f6's games never left the Prehistoric for it.
  Carry the remainder (`frac = frac + income x dt x share; acc += frac // D; frac = frac % D`), as the three accounts
  now do (econ.lua, navy.lua).

## Boom's defence on the per-unit interface (2026-11-10, docs/AI_RESEARCH.md §8.40)

- **A unit killed arrives as a `converted` event, never as `died`.** `EECPUnitsOnTaken` calls a unit converted when
  its hit points are above 0, but `EEComplexUnit::Kill` calls `UnitTaken` before it zeroes them; a priest's
  conversion goes through `EEConvertUnit` -> `Kill(false, false, true)` with the swap flag, which the hook skips. So
  every kill is `converted` and no script ever saw `died` (Age of AI's `diplo.lua` counts its losses from `died`).
  `lib.defence` counts both; the C++ fix is left to the owner because it changes Age of AI's play.
- **A released gatherer is on no runner list.** `ai.release` put only idle citizens back on the engine's idle list,
  so a citizen sent back to its resource with `ai.gather` and then released worked unseen by the gatherer runner until
  the resource ran out. `ai.release(ids, {gather = true})` puts it on its resource's list.
- **A scout is no raid.** The first raid answer sent every citizen near any enemy soldier running: the rush's lone
  scout walking through the town made 61 flights in 5 minutes. A group is a raid only once it is stronger than boom's
  `THREAT_MIN` (2,500) or one of its units has hit an own unit in the last 15 s.
- **Citizens do not fight ranged raiders, whatever the odds.** Against 2-3 rock throwers the square-law odds of 6-12
  citizens came out at 3-4 x, and every such fight still lost a citizen (they kite, and their pierce armour takes 2
  of a citizen's 5 attack). The militia now answers melee raids only; ranged ones are run from, to the towers.
- **A militia counts per raid.** Re-deciding every second added one more citizen each time (the ones already fighting
  were not counted), and a second raid nearby called the first raid's militia off at its own odds; the militia now
  counts the citizens already fighting the raid nearest them.

## The classic computer player in Lua (2026-11-10, ai/classic_lua/ORIGIN, docs/AI_RESEARCH.md §8.39)

- **A script's gatherers are invisible to `ai.resource`'s `workers`.** `EECPNextAssigned` counts the classic resource
  manager's gatherer lists, and `AddGatherer` keeps every unit a script holds off them: the port's finder saw 0
  gatherers on every bush and sent all 15 food gatherers to one (60 food a minute). `ai.gatherers()` (one scan of the
  own units: the citizens whose gather goal is each resource) counts them whoever holds them.
- **A class's tech is `ai.class(t).tech`, not the first tech that makes it.** Several tech tree nodes make the same
  class (the citizen's own 1074 and others); a map from `ai.techs` keyed by `makes` took a later epoch's.
- **The classic puts an epoch at the front of the town centre's queue** (`QueueResearch(id, true)`); a human's order
  (EEMResearchTechnology) queues at the back, behind up to ten citizens (five minutes). The port cancels the citizens
  queued behind the current one (`ai.cancel`, refunded), orders the epoch and queues them again.
- **The classic attempts an epoch advance only with a later epoch's request standing** (or a human ahead): the first
  update queues the "future research sentinel" (dbtechtree 15020, a Space Age wonder) for that, and it stays queued
  all game. Leaving it out, the port never advanced.
- **The engine's reflexes skip a script's units.** EECPRMNoteDanger and EECPRMMilitia ignore held (detached) citizens,
  so a script that holds its citizens has no militia unless it runs one (militia.lua; `ai.reflex{militia = false,
  danger = false}`).
- **A step that asks per unit blows the 2M budget at 25 minutes** (the gather capacity's centres and the citizen
  target were recomputed for every caller): a per-step cache of the records, the own units by class and the derived
  numbers (`resource.lua` begin_step / unit / of_type / memo) keeps the step's charges bounded.
- **The favourite land unit of a "soldier" class can be the Prehistoric domestic wolf** (trained at the town
  centre): the favourite is drawn among the classes the barracks, archery range, stable and factories train.
- **A bench slot that faults at the lobby every time is a port held by a stray EE-SIM.** An EE-SIM left over from an
  earlier run kept listening on one slot's ports (40462/40463); every run that slot took faulted in
  NERecordPlayback::Process before the game and sim_bench moved the next seeds onto the same slot (13 of 16 runs
  failed). Another `--port-base` avoids it; `netstat -ano` names the holder.
- **Editing a script that a running bench reads, or the shell script that runs it, spoils the bench.** `--ai-scripts
  ai` reads the checkout's scripts at each game's start, so an edit mid-bench mixes versions; and bash reads a script
  as it runs, so rewriting the bench's own shell script made the old shell run garbage once sim_bench returned (it
  truncated the log). Benches here run from a copy of `ai/` taken at the start (`build/aisnap/<name>`).
