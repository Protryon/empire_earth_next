"""Clang build driver (port phase 1): compile every unit with clang-cl, 32-bit, MSVC ABI, x87.

  python -I tools/clang_build.py [-j N] [--out DIR] [--opt /Od] [module ...]     compile only (for now)

Same units, flags, defines and include order as tools/build.py (the VC6 reference build), with:
  - clang-cl --target=i686-pc-windows-msvc in MSVC 12.00 (VC6) compatibility mode, wchar_t = unsigned short
    (/Zc:wchar_t-, as VC6);
  - /arch:IA32 (x87 floating point, no SSE) while the clang build is compared against the VC6 build;
  - -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks (the code relies on all three: VC6 never exploited
    that undefined behaviour);
  - VC6-only flags dropped (-G6, -Zm, -Zi; /O1 replaced by --opt);
  - the stock VC6 headers the code includes are read through build/clang/vc6inc, a copy of toolchain/msvc6.0/Include
    with the patches in tools/clang_vc6inc.py applied (the toolchain itself stays untouched).

--stl msvc (port phase 2): compile against Microsoft's STL and the UCRT/Windows SDK instead of Dinkum 2.33 and the
VC6 headers (VS Build Tools 2022; env EE_MSVC_DIR and EE_WINSDK_DIR override the detected directories), with
_MSC_VER 19.44 (the STL refuses older), C++17, native wchar_t. Default out: build/clang-msvc.

Objects go to <out>/obj/<module>/<unit>.obj (default out: build/clang). A failed unit's diagnostics go to
<out>/log/<module>/<unit>.log; the summary lists the first error of each.
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import build
import clang_vc6inc
import verify
from ee import COMPILER, ROOT, TOOLCHAIN

CLANG = pathlib.Path(os.environ.get("EE_CLANG", r"C:\Program Files\LLVM\bin\clang-cl.exe"))
CLANG_FLAGS = ["--target=i686-pc-windows-msvc", "/nologo", "/c", "-fms-compatibility", "-fms-extensions",
               "-fms-compatibility-version=12.00", "/Zc:wchar_t-", "-fdelayed-template-parsing", "/arch:IA32", "/GS-",
               "-fno-strict-aliasing", "-fwrapv", "-fno-delete-null-pointer-checks",
               "-Wno-everything", "-ferror-limit=0", "-fno-color-diagnostics"]
DROP = re.compile(r"^[-/](G[3-7B]|Zm\d+|Zi|Z7|O[12dsxty]|Ob\d|Og|Gy|Gf|GF)$")
ERROR = re.compile(r"^(.*?)\((\d+),\d+\)\s*:\s*error: (.*)$", re.M)
STL = "dinkum"            # or "msvc" (--stl; clang_link.py sets it too)
MSVC_FLAGS = {"-fms-compatibility-version=12.00": "-fms-compatibility-version=19.44", "/Zc:wchar_t-": "/Zc:wchar_t"}


def _newest(d):
    subs = [p for p in pathlib.Path(d).iterdir() if p.is_dir()]
    if not subs:
        raise SystemExit(f"nothing in {d}")
    return max(subs, key=lambda p: [int(x) if x.isdigit() else 0 for x in p.name.split(".")])


def modern_dirs():
    """(MSVC tools dir, Windows SDK include dir, Windows SDK lib dir) of the installed VS Build Tools / SDK."""
    msvc = pathlib.Path(os.environ.get("EE_MSVC_DIR") or _newest(
        r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"))
    kits = pathlib.Path(r"C:\Program Files (x86)\Windows Kits\10")
    sdk_inc = pathlib.Path(os.environ.get("EE_WINSDK_DIR") or _newest(kits / "Include"))
    return msvc, sdk_inc, kits / "Lib" / sdk_inc.name


def modern_include(inc):
    """The VC6 build's INCLUDE list with Dinkum and the VC6 headers replaced by the MSVC STL, UCRT and Windows SDK
    (placed before include/winsdk, which supplies headers the modern SDK dropped, e.g. dmusici.h)."""
    msvc, sdk, _ = modern_dirs()
    drop = {os.path.normcase(str(ROOT / "include" / "dinkum")), os.path.normcase(str(TOOLCHAIN / COMPILER / "Include"))}
    parts = [x for x in inc.split(os.pathsep) if os.path.normcase(x) not in drop]
    modern = [str(msvc / "include"), str(sdk / "ucrt"), str(sdk / "um"), str(sdk / "shared")]
    k = next((i for i, x in enumerate(parts) if pathlib.Path(x).name == "winsdk"), len(parts))
    return os.pathsep.join(parts[:k] + modern + parts[k:])


def clang_flags(flags, opt):
    return [f for f in flags.split() if not DROP.match(f)] + opt.split()


def apply_overlay(staged, rules):
    """A copy of the staged unit rewritten with (old, new, expected count) text rules, next to it (so its sibling
    headers resolve) under a per-compile name (two modules may compile the same unit in parallel) -> its path. Test
    profiles only; the source tree is never touched. Refuses when a rule does not match exactly its expected count."""
    import threading
    staged = pathlib.Path(staged)
    raw = staged.read_bytes()
    crlf = b"\r\n" in raw
    text = raw.decode("latin-1").replace("\r\n", "\n")
    for old, new, want in rules:
        n = text.count(old)
        if n != want:
            raise SystemExit(f"overlay {staged.name}: {old!r} matches {n} times, expected {want}")
        text = text.replace(old, new)
    copy = staged.with_name(f"{staged.stem}.test{os.getpid()}-{threading.get_ident()}{staged.suffix}")
    copy.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("latin-1"))
    return copy


def compile_to(src, flags, obj, opt, vc6inc, overlays=None):
    """Compile one source with clang-cl into obj -> (ok, log). overlays: {source path relative to src/: rules}
    (apply_overlay) for a test profile."""
    flags = verify.unit_flags(src, flags)
    staged = verify.stage_source(src)
    rules = overlays.get(str(build.rel_src(src))) if overlays else None
    iflags, inc = verify.include_paths(staged)
    if rules:
        staged = apply_overlay(staged, rules)
    base = CLANG_FLAGS
    if STL == "msvc":
        inc = modern_include(inc)
        base = [MSVC_FLAGS.get(f, f) for f in CLANG_FLAGS] + ([] if str(src).endswith(".c") else ["/std:c++17"])
    else:
        inc = inc.replace(str(TOOLCHAIN / COMPILER / "Include"), str(vc6inc))
    pathlib.Path(obj).parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(CLANG), *base, *clang_flags(flags, opt), *verify.DEFINES.split(), *iflags, f"/Fo{obj}",
           str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if rules:
        staged.unlink(missing_ok=True)
    return r.returncode == 0, (r.stdout + r.stderr).strip()


def compile_one(job, out, opt, vc6inc, overlays=None):
    mod, src, flags, pkg = job
    rel = build.rel_src(src)
    ok, log = compile_to(src, flags, out / "obj" / mod / rel.with_suffix(".obj"), opt, vc6inc, overlays)
    logf = out / "log" / mod / rel.with_suffix(".log")
    if ok:
        logf.unlink(missing_ok=True)
    else:
        logf.parent.mkdir(parents=True, exist_ok=True)
        logf.write_text(log)
    return job, ok, log


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*", metavar="module", help=f"default: all ({' '.join(build.MODULES)})")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--out", help="output root (default build/clang, build/clang-msvc with --stl msvc)")
    ap.add_argument("--stl", choices=["dinkum", "msvc"], default="dinkum")
    ap.add_argument("--opt", default="/Od", help="optimisation flags for clang-cl (default /Od)")
    ap.add_argument("--only", help="compile only units whose path contains this text")
    a = ap.parse_args()
    global STL
    STL = a.stl
    a.out = a.out or str(ROOT / "build" / ("clang-msvc" if a.stl == "msvc" else "clang"))
    if not CLANG.exists():
        sys.exit(f"{CLANG} is missing: install LLVM (winget install LLVM.LLVM) or set EE_CLANG")
    mods = a.names or build.MODULES
    jobs = [j for m in mods for j in build.jobs_for(m)] + build.lib_jobs(mods)
    if a.only:
        jobs = [j for j in jobs if a.only in str(j[1])]
    out = pathlib.Path(a.out).resolve()
    vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", out / "vc6inc")
    for j in jobs:
        verify.include_paths(verify.stage_source(j[1]))
    t0 = time.time()
    done, errors = {}, {}
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as ex:
        for job, ok, log in ex.map(lambda j: compile_one(j, out, a.opt, vc6inc), jobs):
            done.setdefault(job[0], [0, 0])[0 if ok else 1] += 1
            if not ok:
                errs = ERROR.findall(log)
                first = errs[0] if errs else ("?", "?", log.splitlines()[-1] if log else "no output")
                print(f"FAILED {job[0]} {build.rel_src(job[1])}: {len(errs)} errors; first: "
                      f"{pathlib.Path(first[0]).name}({first[1]}): {first[2]}", flush=True)
                for f, _, msg in errs:
                    key = (pathlib.Path(f).name, re.sub(r"'[^']*'", "'X'", msg))
                    errors[key] = errors.get(key, 0) + 1
    print()
    for m, (ok, bad) in done.items():
        print(f"{m:9} {ok:4} compiled  {bad:3} failed")
    n_bad = sum(b for _, b in done.values())
    print(f"{len(jobs) - n_bad}/{len(jobs)} units compiled in {time.time() - t0:.0f} s -> {out / 'obj'}")
    if errors:
        print("\nmost frequent errors (file, message with names masked):")
        for (f, msg), n in sorted(errors.items(), key=lambda kv: -kv[1])[:40]:
            print(f"{n:6}  {f}: {msg}")
    sys.exit(1 if n_bad else 0)


if __name__ == "__main__":
    main()
