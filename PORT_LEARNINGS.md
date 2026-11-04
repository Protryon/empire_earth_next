# Port learnings: VC6 source to clang-cl

These notes record what it took to move the reconstruction from VC6 to clang-cl (phase 1 of `PORT.md`): what broke, why,
and the fix that was kept. Use them as the checklist for code that is ported later, the WON libraries first
(WONAPI/WONLobby: still stubs in the exe link, see "Linking the exe").

Order that worked: compile census → STL headers → stock-header patches → mechanical source passes → header bridges →
one-off sources → DLL links → exe link → stub audit against VC6.

## Tooling

| Tool | Use |
|---|---|
| `tools/clang_build.py [-j N] [--opt X] [module ...]` | compile census. Per-unit logs go to `build/clang/log/`, and the summary ranks the most frequent errors (names masked). Rerun it after every fix batch. |
| `tools/clang_link.py [target ...]` | compiles everything up front, then links the DLLs (`relink.py` recipe) and the exe (`exe_fulllink.py`), with lld-link. Writes `build/clang/exe/link_report.json`. |
| `tools/clang_vc6inc.py` | patched copy of the stock VC6 `Include/` (with subdirectories). Each patch states how many times it must match. |
| `tools/clang_forscope.py [--apply]` | hoists for-init variables used after their loop. |
| `tools/clang_reftemp.py [--apply]` | names temporaries passed to non-const references at plain call sites. |
| `tools/objcmp.py A B` | compares two VC6 object trees by section contents (A/B of a dialect edit). |
| `tools/clang_link.py --test-profile windowed-software` | TEST-ONLY images in `build/clang-test/` for the live-game harness (see "Running the game"). |
| `tools/clang_session.py [game_session options]` | runs a clang exe through ee's `tools/boot/game_session.py` (env `EE_REPO`, default `../ee`). |
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
  `fetch_libs.py` verify their pinned hashes.

## Compiler flags (clang-cl, phase 1)

`--target=i686-pc-windows-msvc -fms-compatibility -fms-extensions -fms-compatibility-version=12.00 /Zc:wchar_t-
-fdelayed-template-parsing /arch:IA32 /GS- -fno-strict-aliasing -fwrapv`, VC6-only flags dropped (`-G6 -Zm -Zi`,
`/O1` replaced).

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

## For WONAPI/WONLobby later

The WON library is carved or stubbed (`won` class in the stub report) in both builds. When its source arrives, expect
the same sequence. Apply the dialect passes and header bridges first, then check the exports/imports both ways
(WON is linked into the exe, not a DLL, so the ODR/inline section matters most). Then make sure the `won` count in
the stub report reaches 0 with no new traps.
