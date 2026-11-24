"""The store gate (docs/RUST_PLAN.md D9; eemodel.py cppgate runs it): what the game stores for every model and texture
of an open data tree, through the game's own loader code.

  python -I tools/eemodel/storegate.py [--tree TREE] [--build OUT] [--record] [--subset N] [--keep FILE]

Builds tools/eemodel/storegate/ (a console driver, the engine's renderer classes as memory objects: fakes.h) against
the objects of a game build (default build/clang: `python -I build.py` first) and its Rust library, runs it over the
tree's Data folder and compares its output with the digests recorded in storegate/digests.json:

* every model (Models\\*.gltf, *.glb) through GEModelManager::LoadModel, with three shared-frame maps (none, the odd
  frames, all frames): a digest of the whole GEModel, its arrays and sub-models (digest.cpp), or "null";
* every texture (Textures\\*.png; <name>@1.png ... are its frames) through GETextureManager::LoadTexture with seven
  descriptions (frame formats 0 and 1, 16 and 32 bits, alpha or not, MIP maps, dropped levels, the texture pack's
  path), then ReloadTexture and RefreshAllTextures: the frames, sizes, MIP levels and every surface's pixels.

The digests were recorded from the game's loaders before D9 (glTF -> .cem bytes -> GEModel::Load, PNG -> .sst bytes ->
GETexture::Load) and the native loaders after it gave the same output (PORT_LEARNINGS.md "D9 at run time"). They hold
for one tree (a hash of its model and texture names and sizes is recorded with them): another tree, or a deliberate change of what is stored,
needs --record. Only digests are kept; never retail data.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import units   # noqa: E402
import verify   # noqa: E402
import clang_build   # noqa: E402
from ee import CLANG, LLD_LINK, LLVM_LIB, LLVM_NM, MODULE_FLAGS, RUNTIME_LIBS, lib_paths   # noqa: E402

SRC = HERE / "storegate"
DIGESTS = SRC / "digests.json"
DEFAULT_TREE = pathlib.Path("C:/Users/max/Desktop/empire_earth_next/build/open_data/tree")
# the Rust functions storegate.cpp defines as its memory surfaces and screen (GESurface's and GEScreen's members call
# them): the link takes the driver's first (/FORCE:MULTIPLE), and these are the only duplicates it may report
STORE_DUPLICATES = re.compile(r"ee_dx7s[fc]_\w+")


def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr)


def build(game_out, out):
    """The driver over a game build's objects -> its path."""
    game_out, out = game_out.resolve(), out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    import rust_build
    mods = ["lle", "default", "tcp", "mixer", "dx7hrtnl"]
    objs = [units.obj_path(game_out, j) for m in mods for j in units.jobs_for(m)]
    missing = [o for o in objs if not o.exists()]
    if missing:
        sys.exit(f"{len(missing)} objects missing (first {missing[0]}): build the game first (python -I build.py)")
    (out / "engine.rsp").write_text("".join(f'"{o}"\n' for o in objs))
    code, log = run([str(LLVM_LIB), "/nologo", "/out:engine.lib", "@engine.rsp"], out)
    if code:
        sys.exit(f"llvm-lib: {log}")
    flags = MODULE_FLAGS["lle"]
    drivers = []
    for src in ("storegate.cpp", "digest.cpp"):
        obj = out / (pathlib.Path(src).stem + ".obj")
        ok, log = clang_build.compile_to(SRC / src, flags, obj, "-O1", extra=[f"/I{SRC}", f"/I{out}", "/EHsc"])
        if not ok:
            sys.exit(f"{src}:\n{log[-4000:]}")
        drivers.append(obj)
    exe = out / "storegate.exe"
    import build as game_build
    inputs = [*drivers, out / "engine.lib", *game_build.SDK_LIBS, *game_build.static_libs(), *RUNTIME_LIBS]
    (out / "link.rsp").write_text("".join(f'"{x}"\n' for x in inputs))
    # The one Rust library (ee_game.lib) and the engine's objects call into the game's C++ (the screens', units', AI
    # and music hosts, Lua, the database's file search), which the driver does not link: it never reaches them. A
    # symbol may stay unresolved only when the game's own objects (the exe and WON modules, their library members)
    # define it, so an engine or driver symbol that went missing still fails the link.
    code, log = run([str(LLD_LINK), "/nologo", "/INCREMENTAL:NO", "/MACHINE:X64", "/SUBSYSTEM:CONSOLE",
                     "/NODEFAULTLIB", "/DEBUG", "/FORCE:UNRESOLVED", "/FORCE:MULTIPLE", "/DEMANGLE:NO", f"/OUT:{exe}", *lib_paths(),
                     "@link.rsp"], out)
    log = "\n".join(l for l in log.splitlines() if "ignoring section .debug$S" not in l)
    (out / "link.log").write_text(log)
    duplicates = set(re.findall(r"duplicate symbol: (\S+)", log))
    stray = sorted(d for d in duplicates if not STORE_DUPLICATES.fullmatch(d))
    if stray:
        sys.exit(f"link failed ({out / 'link.log'}): duplicate symbols other than the memory surfaces' {stray}")
    unresolved = set(re.findall(r"undefined symbol: (\S+)", log))
    unexpected = sorted(unresolved - game_symbols(game_out)) if unresolved else []
    if code or unexpected:
        sys.exit(f"link failed ({out / 'link.log'}): unresolved {unexpected}\n{log[-4000:]}")
    return exe


def game_symbols(game_out):
    """The external symbols the game's own objects define (the modules the driver leaves out)."""
    mods = ["exe", "won"]
    objs = [units.obj_path(game_out, j) for m in mods for j in units.jobs_for(m)]
    missing = [o for o in objs if not o.exists()]
    if missing:
        sys.exit(f"{len(missing)} game objects missing (first {missing[0]}): build the game first (python -I build.py)")
    syms = set()
    for i in range(0, len(objs), 200):
        r = subprocess.run([str(LLVM_NM), "--defined-only", "--extern-only", "--format=just-symbols",
                            *map(str, objs[i:i + 200])], capture_output=True, text=True)
        syms.update(l.strip() for l in r.stdout.splitlines() if l.strip() and not l.endswith(":"))
    return syms


def tree_hash(tree):
    """The models and textures a tree holds: their names and sizes (a rebuild of the same media keeps it; the digests
    then check the contents)."""
    h = hashlib.sha256()
    for sub in ("Models", "Textures"):
        folder = tree / "Data" / sub
        for p in sorted(folder.iterdir()) if folder.is_dir() else []:
            h.update(f"{sub}/{p.name}:{p.stat().st_size};".encode("utf-8", "surrogateescape"))
    return h.hexdigest()[:16]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--tree", type=pathlib.Path, default=DEFAULT_TREE, help="an open data tree (read only)")
    ap.add_argument("--build", type=pathlib.Path, default=ROOT / "build" / "clang", help="the game build's output")
    ap.add_argument("--out", type=pathlib.Path, default=ROOT / "build" / "storegate")
    ap.add_argument("--record", action="store_true", help="record the output as the digests")
    ap.add_argument("--subset", type=int, default=1, help="every Nth model and texture (not with --record)")
    ap.add_argument("--keep", type=pathlib.Path, help="also copy the driver's output here")
    ap.add_argument("--exe", type=pathlib.Path, help="an already built driver")
    a = ap.parse_args()
    exe = a.exe or build(a.build, a.out)
    result = a.out / "storegate.txt"
    data = a.tree / "Data"
    if not data.is_dir():
        sys.exit(f"no {data}")
    r = subprocess.run([str(exe), str(data), str(result), "--subset", str(a.subset)], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode:
        sys.exit(f"storegate failed ({r.returncode}):\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
    lines = result.read_text().splitlines()
    if a.keep:
        a.keep.write_text("\n".join(lines) + "\n")
    if a.record:
        if a.subset != 1:
            sys.exit("--record needs the whole tree (no --subset)")
        DIGESTS.write_text(json.dumps({"tree": tree_hash(a.tree), "lines": lines}, indent=0) + "\n")
        print(f"recorded {len(lines)} lines -> {DIGESTS}")
        return
    want = json.loads(DIGESTS.read_text())
    if want["tree"] != tree_hash(a.tree):
        sys.exit(f"the digests are for another tree ({want['tree']}, this one {tree_hash(a.tree)}): --record")
    expected = want["lines"]
    if a.subset != 1:
        # a subset compares its own loads (a line without its digest is its key); the totals are the whole tree's
        totals = ("models ", "textures ", "refresh ")
        lines = [l for l in lines if not l.startswith(totals)]
        keys = {l.rsplit(" ", 1)[0] for l in lines}
        expected = [l for l in expected if not l.startswith(totals) and l.rsplit(" ", 1)[0] in keys]
    diff = [(i, w, h) for i, (w, h) in enumerate(zip(expected, lines)) if w != h]
    if len(expected) != len(lines) or diff:
        for i, w, h in diff[:20]:
            print(f"  line {i}: expected {w!r}, got {h!r}")
        sys.exit(f"FAIL: {len(diff)} of {len(lines)} lines differ (expected {len(expected)} lines)")
    print(f"PASS: {len(lines)} lines equal to the recorded digests "
          f"({sum(l.startswith('model ') for l in lines)} model loads, "
          f"{sum(l.startswith('texture ') for l in lines)} texture loads)")


if __name__ == "__main__":
    main()
