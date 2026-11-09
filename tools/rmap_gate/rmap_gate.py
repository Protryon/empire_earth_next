"""The random map generator's byte gate: a canonical dump of every EERandomMap output for every map script x seeds x
map sizes x player counts, compared with the digests recorded from the C++ generator (golden_cpp.txt, RUST_PLAN R3.4).

  python -I tools/rmap_gate/rmap_gate.py build                           compile and link rmap_dump (build/rmap_gate/)
  python -I tools/rmap_gate/rmap_gate.py check [--data TREE] [-j N]      run the matrix, compare with the golden digests
  python -I tools/rmap_gate/rmap_gate.py record [--data TREE] [-j N]     (over)write the golden digests
  python -I tools/rmap_gate/rmap_gate.py diff SCRIPT SIZE PLAYERS TEAMS SEED   per-stage digests of one map

The tool compiles the .rmv reader (Unit10.cpp), the C++ adapter (Default.cpp) and the engine units they need, and links
the generator as ee_rmap_gatelib. The golden digests were recorded from the C++ generator (commit 7e470841) before
it was deleted; there is no way to re-record them from C++ now. `record` overwrites them from the Rust generator: only
do that at a declared rebaseline.
"""
import argparse
import concurrent.futures
import os
import pathlib
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import clang_build
import verify
import ee
import rust_build
from ee import LLD_LINK, MODULE_FLAGS, ROOT

OUT = ROOT / "build" / "rmap_gate"
GOLDEN = HERE / "golden_cpp.txt"
DATA_DEFAULT = ROOT.parent / "empire_earth_next" / "build" / "open_data" / "tree"
DEFAULT = ROOT / "src" / "Empire Earth" / "Random Map" / "Random Map Generators" / "Default"
UTIL = ROOT / "src" / "Low-Level Engine"
READER = ["Unit10.cpp"]
# engine units the dump tool compiles for real: the geometry math, the random stream, the strings and the fractal
LLE_UNITS = ["Utilities/UString.cpp", "Utilities/UMath.cpp", "Utilities/U2DFractal.cpp",
             "File System/FSBasicFile.cpp", "File System/FSFileSpec.cpp", "Graphics Engine/GE3DLine.cpp",
             "Graphics Engine/GE2DPoint.cpp", "Graphics Engine/GE3DPoint.cpp", "Graphics Engine/GETransformation.cpp"]
OPT = "-O1"
DEFAULT_GENERATOR = "rust"


def compile_units(generator, jobs=4):
    work = [(DEFAULT / n, MODULE_FLAGS["default"]) for n in READER + ["Default.cpp"]]
    work += [(UTIL / rel, MODULE_FLAGS["lle"]) for rel in LLE_UNITS]
    work += [(HERE / "rmap_dump.cpp", MODULE_FLAGS["default"])]
    work += [(HERE / "rmap_stubs.cpp", MODULE_FLAGS["default"])]

    def one(job):
        src, flags = job
        obj = OUT / "obj" / generator / (src.stem + ".obj")
        ok, log = clang_build.compile_to(src, flags, obj, OPT)
        return src, obj, ok, log
    objs, failed = [], []
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for src, obj, ok, log in ex.map(one, work):
            if ok:
                objs.append(obj)
            else:
                failed.append(src.name)
                print(f"FAILED {src.name}\n{log[-3000:]}", flush=True)
    if failed:
        sys.exit(f"compile failed: {failed}")
    return objs


def rust_staticlib():
    """The generator as a small static library (the game's ee_rust is far too big to build for a helper)."""
    r = subprocess.run([rust_build.cargo(), "build", "--release", "-p", "ee_rmap_gatelib"], cwd=ROOT / "rust",
                       env=rust_build.cargo_env())
    if r.returncode:
        sys.exit("cargo build of ee_rmap_gatelib failed")
    return rust_build.TARGET_DIR / "release" / "ee_rmap_gatelib.lib"


def link(objs, generator, extra=()):
    exe = OUT / f"rmap_dump_{generator}.exe"
    exe.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(LLD_LINK), "/nologo", "/INCREMENTAL:NO", "/MACHINE:X64", "/SUBSYSTEM:CONSOLE", "/NODEFAULTLIB",
           "/ENTRY:mainCRTStartup", "/ERRORLIMIT:0", "/FORCE:UNRESOLVED", f"/OUT:{exe}", *ee.lib_paths(), *[str(o) for o in objs],
           *[str(x) for x in extra], "kernel32.lib", "user32.lib", "advapi32.lib", "ole32.lib", "shell32.lib",
           "ws2_32.lib", "bcrypt.lib", "ntdll.lib", "userenv.lib", *ee.RUNTIME_LIBS]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stdout + r.stderr)
    return exe


def build(generator, jobs=4):
    extra = []
    if generator == "rust":
        extra = [rust_staticlib()]
    return link(compile_units(generator, jobs), generator, extra)


def run_matrix(exe, data, jobs):
    """The matrix over `jobs` processes -> {key: digest}; a process that crashes fails the gate."""
    procs = [subprocess.Popen([str(exe), str(data), "--shard", str(k), str(jobs)], stdout=subprocess.PIPE, text=True)
             for k in range(jobs)]
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        outs = list(ex.map(lambda p: p.communicate()[0], procs))
    for p in procs:
        if p.returncode != 0:
            sys.exit(f"rmap_dump exited with {p.returncode:#x}")
    table = {}
    for out in outs:
        for line in out.splitlines():
            key, digest = line.rsplit(" ", 1)
            table[key] = digest
    return table


def read_golden():
    return dict(line.rsplit(" ", 1) for line in GOLDEN.read_text().splitlines() if line)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["build", "check", "record", "diff"])
    ap.add_argument("key", nargs="?", help="diff: script|size|players|mode|seed")
    ap.add_argument("--generator", choices=["rust"], default=DEFAULT_GENERATOR)
    ap.add_argument("--data", default=str(DATA_DEFAULT))
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    exe = OUT / f"rmap_dump_{a.generator}.exe"
    if a.command == "build":
        print(build(a.generator, a.j))
        return 0
    if not exe.exists():
        build(a.generator, a.j)
    if a.command == "diff":
        return subprocess.run([str(exe), a.data, "--stages", "--only", a.key]).returncode
    t0 = time.time()
    table = run_matrix(exe, a.data, a.j)
    if a.command == "record":
        GOLDEN.write_text("".join(f"{k} {table[k]}\n" for k in sorted(table)), newline="\n")
        print(f"recorded {len(table)} maps from the {a.generator} generator in {time.time() - t0:.0f} s -> {GOLDEN}")
        return 0
    golden = read_golden()
    differ = sorted(k for k in golden if table.get(k) != golden[k])
    extra = sorted(k for k in table if k not in golden)
    for k in (differ + extra)[:20]:
        print(f"DIFFERS {k}: golden {golden.get(k)} now {table.get(k)}")
    print(f"{len(golden) - len(differ)}/{len(golden)} maps identical, {len(extra)} not in the golden file "
          f"({a.generator} generator, {time.time() - t0:.0f} s)")
    return 1 if differ or extra else 0


if __name__ == "__main__":
    sys.exit(main())
