# empire_earth_next: the modern-compiler port

This repository is a fork of `empire_earth` (remote `upstream`). It starts from upstream's `fixes-all` branch: the
reconstruction plus every original-bug fix. Its goal is a game built with a current compiler, and later a portable
one. Behaviour is allowed to diverge from the original once the compiler swap is validated.

## Upstream sync

Upstream rewrites `fixes-all` on every update: its fix commits are replayed onto the new upstream `main`. Take
upstream changes by merging, not rebasing, so this repository's history is never rewritten:

```
git fetch upstream
git merge upstream/fixes-all
```

The replayed fix commits carry the same changes as before, so they merge cleanly; conflicts come only from upstream
edits to lines this fork has changed.

## Phase 1: same program, new compiler (in progress)

The VC6 build (`python -I build.py`) stays the reference. `tools/clang_build.py` compiles the same units with clang-cl
(32-bit, MSVC ABI, x87 floating point, `-fno-strict-aliasing -fwrapv`, VC6 compatibility mode). Both builds are
compared game tick by game tick. Every divergence is then a compiler or undefined-behaviour issue, not a design
change.

- Requirements beyond the VC6 build: LLVM (`winget install LLVM.LLVM`; `clang-cl` and `lld-link`; env `EE_CLANG`
  overrides the path).
- `python -I tools/clang_build.py [-j N] [module ...]`: compile with clang-cl into `build/clang/obj/`. Per-unit
  diagnostics go to `build/clang/log/`, and the summary ranks the most frequent errors.
- The stock VC6 headers are read through `build/clang/vc6inc`: a patched copy made by `tools/clang_vc6inc.py`. The
  toolchain itself is never edited.
- Source changes made for clang must leave the VC6 build's code unchanged where possible. Check with
  `python -I tools/objcmp.py <vc6 objs before> <vc6 objs after>`, comparing `build.py --compile-only --out` trees.
  `EELobbyConnectDialog.obj` always differs: VC6 names its anonymous namespace after the compile time.

Floating point: x87 (`/arch:IA32`) only while the clang build is compared against VC6. Bit-exact x87 compatibility is
not a goal.

Status: all 845 units compile with clang-cl (`/Od`, warnings off for now). Dialect bridges, each to be removed once
the VC6 comparison phase is over:

- `#ifdef __clang__` `const T&` forwarding overloads in the headers, where VC6 bound temporaries to non-const
  reference parameters. The parameter types are fixed by the original's mangled names while VC6 is the reference;
  afterwards the parameters become const and the overloads go.
- Clang-only postfix `++`/`--` on Dinkum's deque iterators, with VC6's meaning (prefix).
- Call sites that passed a temporary to a one-off non-const reference name it (`theTempN`, `tools/clang_reftemp.py`).
- `for`-init variables used after their loop are declared before it (`tools/clang_forscope.py`).

Next steps: linking (lld-link, MSVC ABI, the same import libraries), the differential game comparison against the
VC6 build, and then replacing Dinkum 2.33 with a modern standard library.
