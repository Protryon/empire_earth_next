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
for one tree (its open_data.json hash is recorded with them): another tree, or a deliberate change of what is stored,
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
from ee import CLANG, LLD_LINK, LLVM_LIB, MODULE_FLAGS, RUNTIME_LIBS, lib_paths   # noqa: E402

SRC = HERE / "storegate"
DIGESTS = SRC / "digests.json"
DEFAULT_TREE = pathlib.Path("C:/Users/max/Desktop/empire_earth_next/build/open_data/tree")
# the classes whose pure virtuals fakes.h leaves to the generated headers: (header, class, generated file, written by hand)
FAKES = [
    ("GESurface.h", "GESurface", "fakes_generated_surface.h",
     {"AttachSurface", "CopyTo", "CopyWithFilter", "DetachSurface", "GetAllocatedSize", "GetAttachedSurface",
      "GetAttachedSurfaceCount", "GetBytesPerRow", "GetHeight", "GetIs3DRenderable", "GetIsTextLocked",
      "GetIsTransparencyEnabled", "GetMemoryAddress", "GetParentScreen", "GetPixelFormat", "GetPixelDepth",
      "GetSurfaceVolatility", "GetSurfaceDescription", "GetTransparencyColor", "GetType", "GetWidth", "Lock",
      "RestoreSurface", "SetTransparencyColor", "TextLock", "TextUnlock", "Unlock"}),
    ("GEScreen.h", "GEScreen", "fakes_generated_screen.h", {"CreateSurface"}),
    ("GERasterizer.h", "GERasterizer", "fakes_generated_rasterizer.h",
     {"CreatePolygonArray", "CreateVertexArray", "ActivateRasterizer", "CreateTextureHandle",
      "DestroyTextureHandle"}),
]


def class_body(text, name):
    m = re.search(r"class\s+LLE_API\s+" + name + r"\b[^{;]*\{", text)
    if not m:
        sys.exit(f"no class {name}")
    depth, i = 1, m.end()
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[m.end():i - 1]


def preprocess(text, defined=("__clang__",)):
    """The header's #ifdef/#ifndef/#else/#endif branches as the engine's units see them (LLE_EXE_HEADERS unset)."""
    out, stack = [], []
    for line in text.splitlines():
        m = re.match(r"\s*#\s*(ifdef|ifndef|if|else|elif|endif)\b\s*(\w*)", line)
        if m:
            kind, name = m.groups()
            if kind in ("ifdef", "ifndef", "if"):
                on = (name in defined) if kind == "ifdef" else (name not in defined) if kind == "ifndef" else True
                stack.append(on)
            elif kind in ("else", "elif") and stack:
                stack[-1] = not stack[-1]
            elif kind == "endif" and stack:
                stack.pop()
            continue
        if all(stack):
            out.append(line)
    return "\n".join(out)


def generate_fakes(out):
    """The overrides fakes.h does not write: every other pure virtual of the class, returning a zero value."""
    for header, cls, target, done in FAKES:
        text = re.sub(r"//[^\n]*", "", preprocess((ROOT / "include" / "lle" / header).read_text(errors="replace")))
        body = class_body(text, cls)
        lines = []
        for decl in re.findall(r"virtual\s+([^;{}]*?)=\s*0\s*;", body, re.S):
            decl = " ".join(decl.split())
            m = re.match(r"(.*?)\b(\w+)\s*\((.*)\)\s*(const)?\s*$", decl)
            if not m:
                sys.exit(f"{cls}: cannot read {decl!r}")
            ret, fname, params, const = m.group(1).strip(), m.group(2), m.group(3), m.group(4) or ""
            if fname in done:
                continue
            if ret == "void":
                impl = "{}"
            elif ret.endswith("&"):
                impl = f"{{ static std::remove_const_t<std::remove_reference_t<{ret}>> s; return s; }}"
            else:
                impl = "{ return {}; }"
            lines.append(f"    {ret} {fname}({params}) {const} override {impl}")
        (out / target).write_text("\n".join(lines) + "\n")


def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr)


def build(game_out, out):
    """The driver over a game build's objects -> its path."""
    game_out, out = game_out.resolve(), out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    generate_fakes(out)
    import rust_build
    mods = ["lle", "jpeg", "default", "tcp", "mixer", "dx7hrtnl"]
    objs = [units.obj_path(game_out, j) for m in mods for j in units.jobs_for(m)]
    objs += [units.obj_path(game_out, j) for j in units.lib_jobs(mods)]
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
    code, log = run([str(LLD_LINK), "/nologo", "/INCREMENTAL:NO", "/MACHINE:X64", "/SUBSYSTEM:CONSOLE",
                     "/NODEFAULTLIB", "/DEBUG", f"/OUT:{exe}", *lib_paths(), "@link.rsp"], out)
    log = "\n".join(l for l in log.splitlines() if "ignoring section .debug$S" not in l)
    (out / "link.log").write_text(log)
    if code:
        sys.exit(f"link failed ({out / 'link.log'}):\n{log[-4000:]}")
    return exe


def tree_hash(tree):
    meta = tree / "open_data.json"
    return hashlib.sha256(meta.read_bytes()).hexdigest()[:16] if meta.exists() else "none"


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
