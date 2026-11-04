"""Clang build, link step (port phase 1): link the images from clang-cl objects with lld-link.

  python -I tools/clang_link.py [--out DIR] [--opt -O1] [target ...]   (default: tcp mixer default lle dx7hr dx7hrtnl exe)

Reuses the VC6 build's link recipes (tools/relink.py for the DLLs, tools/exe_fulllink.py for the exe: unit order, library members built from source, the vintage
import/static libraries, the by-name import libraries, the export .def with the original ordinals) with two swaps:
every source is compiled with clang-cl (tools/clang_build.py flags, objects cached by source + flags under
<out>/obj), and LINK 6 is replaced by lld-link (env EE_LLD_LINK overrides the path). LIB/DUMPBIN stay VC6's.
Outputs go to <out>/<target>/ (default out: build/clang). The exe's map is written with LINK 6's 8-digit addresses
(lld writes 16) so the map readers of the VC6 build's tools take it unchanged.

--test-profile windowed-software (default out: build/clang-test): a TEST-ONLY build for the headless live-game
harness (tools/clang_session.py). It is the source-level form of the original-image overlay that harness applies
(windowed_overlay.py PATCHES and the software-display EEInitializeDisplay policy): the staged copies of two units
are rewritten by TEST_PROFILES's text rules, each refusing unless it matches its expected count. src/ is never
changed, and the images are test artifacts.
"""
import argparse
import collections
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build
import clang_build
import clang_vc6inc
import relink
import verify
from ee import COMPILER, ROOT, TOOLCHAIN

LLD_LINK = pathlib.Path(os.environ.get("EE_LLD_LINK", r"C:\Program Files\LLVM\bin\lld-link.exe"))
DLL_TARGETS = ["tcp", "mixer", "default", "lle", "dx7hr", "dx7hrtnl"]
TARGETS = DLL_TARGETS + ["exe"]
# LINK 6 spellings lld-link does not take
FLAG_MAP = {"/MACHINE:I386": "/MACHINE:X86", "/MACHINE:IX86": "/MACHINE:X86"}
# the VC6 CRT objects predate SafeSEH (LINK 6 had no such table)
LLD_FLAGS = ["/SAFESEH:NO", "/ERRORLIMIT:0", "/DEMANGLE:NO"]
TEST_PROFILES = {
    "windowed-software": {
        # windowed_overlay.PATCHES: the settings default and Load keep mFullScreen (+0x5cf) false
        "Empire Earth/Empire Earth/EEPreferences.cpp": [
            ("    this->mFullScreen = true;", "    this->mFullScreen = false;   // TEST PROFILE windowed", 1),
            ("    this->mSettings.mFullScreen = true;",
             "    this->mSettings.mFullScreen = false;   // TEST PROFILE windowed", 1)],
        # the software-display policy (fulllink_locator.near_display_patches): the bit-depth checks accept and
        # store 32 where they accepted/stored 16, and the no-matching-mode fallback is 32
        "Empire Earth/Empire Earth/Empire Earth.cpp": [
            ("mGameBitDepth != 16)", "mGameBitDepth != 32)", 1),
            ("mTextureBitDepth != 16)", "mTextureBitDepth != 32)", 1),
            ("mGameBitDepth = 16;", "mGameBitDepth = 32;", 2),
            ("mTextureBitDepth = 16;", "mTextureBitDepth = 32;", 1)],
    },
}


class ClangObjects:
    """Stands in for verify.compile_candidate: the objects compiled up front, by (source, flags); anything else is
    compiled on the spot."""

    def __init__(self, out, opt, objs, overlays=None):
        self.out, self.opt, self.objs, self.overlays = out, opt, objs, overlays
        self.vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", out / "vc6inc")

    def __call__(self, src, flags, workdir):
        src = pathlib.Path(src).resolve()
        dst = pathlib.Path(workdir) / "cand.obj"
        obj = self.objs.get((str(build.rel_src(src)), verify.unit_flags(src, flags)))
        if obj is None:
            ok, log = clang_build.compile_to(src, flags, dst, self.opt, self.vc6inc, self.overlays)
            return (dst if ok else None), log
        shutil.copy(obj, dst)
        return dst, ""


def compile_all(targets, out, opt, jobs, overlays=None):
    """Compile every unit the targets link, in parallel -> {(rel source, flags): obj}; exits on a failure."""
    mods = [m for t in targets for m in build.TARGET_MODULES.get(t, [t])]
    work = [j for m in mods for j in build.jobs_for(m)] + build.lib_jobs(mods)
    vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", out / "vc6inc")
    for j in work:
        verify.include_paths(verify.stage_source(j[1]))
    objs, failed = {}, []
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for job, ok, log in ex.map(lambda j: clang_build.compile_one(j, out, opt, vc6inc, overlays), work):
            if ok:
                key = (str(build.rel_src(job[1])), verify.unit_flags(job[1], job[2]))
                objs[key] = out / "obj" / job[0] / build.rel_src(job[1]).with_suffix(".obj")
            else:
                failed.append(job)
                print(f"FAILED {job[0]} {build.rel_src(job[1])}\n{log[-2000:]}", flush=True)
    print(f"{len(work) - len(failed)}/{len(work)} units compiled", flush=True)
    if failed:
        sys.exit(1)
    return objs


LLVM_LIB = LLD_LINK.with_name("llvm-lib.exe")
# Full import libraries for third-party DLLs whose inline code clang expands further than VC6 did (the by-name
# import libraries list only what the original images imported).
EXTRA_IMPORT_DEFS = [ROOT / "link" / "clang" / "sxlrt233.def"]


def extra_import_libs(out):
    libs = []
    for d in EXTRA_IMPORT_DEFS:
        lib = out / "implib" / (d.stem + ".full.lib")
        if not lib.exists() or lib.stat().st_mtime < d.stat().st_mtime:
            lib.parent.mkdir(parents=True, exist_ok=True)
            r = subprocess.run([str(LLVM_LIB), "/nologo", "/machine:x86", f"/def:{d}", f"/out:{lib}"],
                               capture_output=True, text=True)
            if r.returncode:
                sys.exit(f"llvm-lib {d.name}: {r.stdout}{r.stderr}")
        libs.append(str(lib))
    return libs


LLVM_NM = LLD_LINK.with_name("llvm-nm.exe")


def defined_symbols(inputs):
    out = subprocess.run([str(LLVM_NM), "--defined-only", "--extern-only", "--format=just-symbols", *inputs],
                         capture_output=True, text=True)
    syms = {l.strip() for l in out.stdout.splitlines() if l.strip() and not l.endswith(":")}
    # a .def names C functions undecorated: _name (cdecl), _name@n (stdcall)
    return syms | {re.sub(r"@\d+$", "", s[1:]) for s in syms if s.startswith("_")}


def clang_def(path, inputs):
    """Keep only the exports some linked object or static library defines. VC6's images also exported things clang
    never emits as such: pooled string literals (??_C@, named differently and kept local by clang), implicit
    member-template instantiations and default-constructor closures of exported classes. Clang-built consumers
    instantiate those locally, so nothing imports them. Every dropped export is listed.

    A member-template instantiation that is defined keeps its VC6 export name as an alias: VC6 mangled it without
    the template arguments (?Name@scope...), clang with them (??$Name@<args>@scope...); the .def line becomes
    `vc6name=clangname`, so images built by VC6 (the retail exe) still find it."""
    defined = defined_symbols(inputs)
    templ = collections.defaultdict(list)       # (name, rest) -> clang ??$ symbols
    for s in defined:
        m = re.match(r"\?\?\$([^@?]+)@", s)
        if m:
            templ[m.group(1)].append(s)
    lines = path.read_text().splitlines(keepends=True)
    keep, dropped, aliased = [], [], []
    for l in lines:
        name = l.split()[0] if l.startswith("    ") else None
        if name and name not in defined:
            m = re.match(r"\?([^@?]+)@(.*)$", name)
            hits = [s for s in templ.get(m.group(1), []) if s.endswith("@" + m.group(2))] if m else []
            if len(hits) == 1:
                keep.append(l.replace(name, f"{name}={hits[0]}", 1))
                aliased.append(name)
            else:
                dropped.append(name)
        else:
            keep.append(l)
    if aliased:
        print(f"{path.name}: {len(aliased)} VC6 member-template export name(s) aliased to clang's")
    path.write_text("".join(keep))
    (path.parent / "exports_dropped.txt").unlink(missing_ok=True)
    if dropped:
        (path.parent / "exports_dropped.txt").write_text("".join(f"{n}\n" for n in dropped))
        print(f"{path.name}: {len(dropped)} export(s) not defined by any clang object, dropped "
              f"(exports_dropped.txt): {', '.join(dropped[:4])}{' ...' if len(dropped) > 4 else ''}")


def lld_run(inner, extra_libs):
    def run(cmd, cwd, pkg):
        if pathlib.Path(cmd[0]).name.upper() == "LINK.EXE":
            cmd = [str(LLD_LINK), *LLD_FLAGS, *[FLAG_MAP.get(a.upper(), a) for a in cmd[1:]], *extra_libs]
            inputs = [str(pathlib.Path(cwd) / a) for a in cmd[1:] if a.lower().endswith((".obj", ".lib"))
                      and not a.startswith("/")]
            for a in cmd:
                if a.upper().startswith("/DEF:"):
                    clang_def(pathlib.Path(cwd) / a[5:], inputs)
            (pathlib.Path(cwd) / "lld-link.cmd").write_text("".join(f'"{a}"\n' for a in cmd[1:]))
        return inner(cmd, cwd, pkg)
    return run


def lld_parse_log(log):
    """exe_fulllink.parse_log for lld-link output (/demangle:no): {undefined: [referencing objects]}, [(duplicate,
    object, object)], [other errors]."""
    unres, dups, other = {}, [], []
    lines = log.splitlines()
    for k, ln in enumerate(lines):
        m = re.match(r"^lld-link: error: (undefined symbol|duplicate symbol): (\S+)", ln)
        if not m:
            if "error:" in ln and "too many errors" not in ln:
                other.append(ln)
            continue
        where = []
        for l2 in lines[k + 1:]:
            if not l2.startswith(">>> "):
                break
            mm = re.match(r">>> (?:referenced by|defined at) (?:.*?\()?([^():]+\.obj|[^():]+\.lib\([^)]*\))", l2)
            where.append(mm.group(1) if mm else l2[4:])
        if m.group(1) == "undefined symbol":
            unres.setdefault(m.group(2), []).extend(where)
        else:
            dups.append((m.group(2), *(where + ["?", "?"])[:2]))
    return unres, dups, other


def clang_coff_symbols(inner):
    """exe_fulllink.coff_symbols for clang objects. VC6 types every undefined function symbol 0x20; clang leaves the
    type of undefined symbols null, so the stub planner would take every missing function for data. A C++ name is
    a function when its demangled form has a parameter list (implib.is_cpp_data); a C name when a call/jmp rel32
    targets it."""
    import implib

    def coff_symbols(path):
        defined, undef = inner(path)
        c = verify.Coff(path)
        called = set()
        for i, sec in enumerate(c.secs, 1):
            if not sec["chars"] & 0x20:      # code
                continue
            data = c.section_data(i)
            for va, sym, typ in c.relocs(i):
                if typ == 0x14 and va > 0 and data[va - 1] in (0xE8, 0xE9):
                    called.add(sym["name"])
        for n, f in undef.items():
            if not f and not n.startswith("__imp_"):
                undef[n] = (not implib.is_cpp_data(n)) if n.startswith("?") else n in called
        return defined, undef
    return coff_symbols


def link6_map(path):
    """lld's MSVC-style map with LINK 6's 8-digit Rva+Base column (the image is 32-bit)."""
    if path.exists():
        text = path.read_text(encoding="latin-1")
        path.write_text(re.sub(r"(?m)^( [0-9a-f]{4}:[0-9a-f]{8}\s+\S+\s+)0{8}([0-9a-f]{8})", r"\1\2", text),
                        encoding="latin-1")


def link_exe(out, objs, extra_libs):
    """build.link_exe with the clang objects (tools/exe_fulllink.py: pass 1, stubs for what the source does not
    define yet, pass 2, member aliases)."""
    import exe_fulllink
    import exe_roundtrip
    from ee import MODULE_FLAGS, module_units
    exe_roundtrip.run = lld_run(exe_roundtrip.run, extra_libs)
    exe_fulllink.parse_log = lld_parse_log
    exe_fulllink.coff_symbols = clang_coff_symbols(exe_fulllink.coff_symbols)
    exe_fulllink.MAP_RE = re.compile(r"^ ([0-9a-f]{4}):([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{8}|[0-9a-f]{16})\s+"
                                     r"(?:f\s+)?(?:i\s+)?(\S+)?", re.I)
    dst = out / "exe"
    dst.mkdir(parents=True, exist_ok=True)
    items = []
    for mod in ("exe", "exeimg"):
        for lo, hi, p in module_units(mod):
            key = (str(build.rel_src(p)), verify.unit_flags(p, MODULE_FLAGS[mod]))
            items.append({"mod": mod, "lo": lo, "hi": hi, "path": p, "flags": MODULE_FLAGS[mod], "rel": key[0],
                          "obj": objs[key]})
    libobj = {}
    for n in ("newaop", "delaop"):
        p = ROOT / "src" / relink.LIB_UNITS[n]
        libobj[n] = objs[(str(build.rel_src(p)), verify.unit_flags(p, build.EXE_LIB_FLAGS))]
    rep, _ = exe_fulllink.link_exe(dst, items, {}, libobj, members=build.EXE_MEMBERS)
    link6_map(dst / "EE-AOC.map")
    import json
    (dst / "link_report.json").write_text(json.dumps(rep, indent=1, default=str))
    p2 = rep.get("pass2", {})
    ma = rep.get("member_alias", {"exit": 0, "unresolved": {}, "duplicates_link": []})
    for k in ("unresolved", "duplicates_link", "other"):
        if p2.get(k):
            print(f"pass 2 {k}: {list(p2[k])[:15]}")
    ok = (p2.get("exit") == 0 and not p2.get("unresolved") and not p2.get("duplicates_link")
          and ma["exit"] == 0 and not ma["unresolved"] and not ma["duplicates_link"])
    print(f"exe: {'ok' if ok else 'FAILED'} -> {dst / 'EE-AOC.exe'}", flush=True)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("targets", nargs="*", default=TARGETS)
    ap.add_argument("--out", help="output root (default build/clang, build/clang-test with --test-profile)")
    ap.add_argument("--test-profile", choices=sorted(TEST_PROFILES), help="TEST ONLY: see the module doc")
    ap.add_argument("--opt", default="-O1",
                    help="clang-cl optimisation (default -O1: the dllimport inline functions must be expanded, as "
                         "VC6 -O1 did, or the images import functions the DLLs do not export)")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    a = ap.parse_args()
    if not LLD_LINK.exists():
        sys.exit(f"{LLD_LINK} is missing: install LLVM (winget install LLVM.LLVM) or set EE_LLD_LINK")
    out = pathlib.Path(a.out or ROOT / "build" / ("clang-test" if a.test_profile else "clang")).resolve()
    overlays = TEST_PROFILES.get(a.test_profile)
    for t in a.targets:
        if t not in TARGETS:
            sys.exit(f"unknown target {t!r}; known: {' '.join(TARGETS)}")
    targets = sorted(a.targets, key=lambda t: t != "lle")  # the engine first: the others link its import library
    objs = compile_all(targets, out, a.opt, a.j, overlays)
    verify.compile_candidate = ClangObjects(out, a.opt, objs, overlays)
    inner_run, extra_libs = relink.run, extra_import_libs(out)
    engine_lib = out / "lle" / "Low-Level Engine.lib"
    results = {}
    for t in targets:
        print(f"--- {t}: clang + lld-link", flush=True)
        libs = extra_libs + ([str(engine_lib)] if t != "lle" else [])
        if t != "lle" and not engine_lib.exists():
            sys.exit(f"{t}: {engine_lib} is missing: link lle first")
        if t == "exe":
            results[t] = link_exe(out, objs, libs)
            continue
        relink.run = lld_run(inner_run, libs)
        results[t] = relink.relink(t, out / t)
    print()
    for t, r in results.items():
        print(f"{t:9} {'ok' if r == 0 else 'FAILED'}")
    sys.exit(1 if any(results.values()) else 0)


if __name__ == "__main__":
    main()
