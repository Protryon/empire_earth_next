"""Record layouts of a build profile, by member name (the harness's offsets).

  python -I tools/harness/layout_gen.py [--stl dinkum|msvc] [--check]

The live-game harness reads game memory: UI forms, the world, units, the physics clock. ee's harness has those
offsets written in as VC6's numbers, which only hold for VC6/Dinkum layouts. Here every offset comes from the build
itself. clang dumps the record layout of every class a translation unit sees (-fdump-record-layouts-complete,
private members and STL internals included). The dumps of the units in UNITS (together they see every class the
harness reads) are parsed into

  build/harness/layout-<stl>.json   {class: {"size": n, "fields": {"member.sub.sub": offset}}}

and read through layout.py (field offsets, a container's first/last element pointers whatever the library).

--check (dinkum profile): compare every member that carries a `// +0x..` offset comment in include/ (VC6's layout,
proven from the original's code) with the dumped offset. The Dinkum clang build must reproduce VC6's layout exactly;
any difference is reported and the exit status is 1.
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import build          # noqa: E402
import clang_build    # noqa: E402
import clang_vc6inc   # noqa: E402
import verify         # noqa: E402
from ee import COMPILER, MODULE_FLAGS, TOOLCHAIN   # noqa: E402

OUT = ROOT / "build" / "harness"
# (module, unit): together they include every class the harness reads
UNITS = [
    ("lle", "Low-Level Engine/User Interface/UIManager.cpp"),
    ("lle", "Low-Level Engine/User Interface/UIForm.cpp"),
    ("lle", "Low-Level Engine/Graphics Engine/GERasterizer.cpp"),
    ("lle", "Low-Level Engine/Input System/ISMouse.cpp"),
    ("lle", "Low-Level Engine/Network Engine/NESynchronousServer.cpp"),
    ("dx7hr", "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Screen.cpp"),
    ("exe", "Empire Earth/Empire Earth/Empire Earth.cpp"),
    ("exe", "Empire Earth/Empire Earth/EEWorld.cpp"),
    ("exe", "Empire Earth/Physics Thread/EEPhysics.cpp"),
    ("exe", "Empire Earth/AI Thread/EEComputerPlayer.cpp"),
    ("exe", "Empire Earth/Units/EEComplexUnit.cpp"),
    ("exe", "Empire Earth/Player/EEPlayer.cpp"),
]
LINE = re.compile(r"^\s*(\d+)(?::(\d+)-(\d+))? \|( *)(.*)$")


def dump(mod, rel, stl):
    """clang's record-layout dump of one unit under the profile."""
    clang_build.STL = stl
    src = (ROOT / "src" / rel).resolve()
    staged = verify.stage_source(src)
    iflags, inc = verify.include_paths(staged)
    if stl == "msvc":
        inc = clang_build.modern_include(inc)
        base = [clang_build.MSVC_FLAGS.get(f, f) for f in clang_build.CLANG_FLAGS] + ["/std:c++17"]
    else:
        vc6inc = clang_vc6inc.prepare(TOOLCHAIN / COMPILER / "Include", ROOT / "build" / "clang" / "vc6inc")
        inc = inc.replace(str(TOOLCHAIN / COMPILER / "Include"), str(vc6inc))
        base = list(clang_build.CLANG_FLAGS)
    base = [f for f in base if f != "/c"]
    flags = verify.unit_flags(src, MODULE_FLAGS[mod])
    cmd = [str(clang_build.CLANG), *base, *clang_build.clang_flags(flags, "-O1"), *verify.DEFINES.split(), *iflags,
           "/Zs", "-Xclang", "-fdump-record-layouts-complete", str(staged)]
    r = subprocess.run(cmd, env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True, encoding="latin-1")
    if r.returncode:
        sys.exit(f"{rel}: clang failed\n{r.stderr[-2000:]}")
    return r.stdout


def parse(text, into):
    """Every '*** Dumping AST Record Layout' block -> into[class] = {size, fields: {dotted path: offset}}."""
    for block in text.split("*** Dumping AST Record Layout")[1:]:
        lines = block.strip("\n").splitlines()
        m = LINE.match(lines[0])
        if not m:
            continue
        head = m.group(5)
        name = re.sub(r"^(class|struct|union) ", "", head.strip())
        if name in into:
            continue
        fields, stack, size = {}, [], None
        for l in lines[1:]:
            if "[sizeof=" in l:
                size = int(re.search(r"sizeof=(\d+)", l).group(1))
                break
            m = LINE.match(l)
            if not m:
                continue
            off, depth, decl = int(m.group(1)), (len(m.group(4)) - 1) // 2, m.group(5).strip()
            del stack[depth:]
            if decl.endswith("(base)") or decl.endswith("(base) (empty)") or decl.endswith("(primary base)") \
                    or "vftable pointer" in decl or "vbtable pointer" in decl or "(virtual base)" in decl:
                stack.append(None)   # base subobject: its members join the derived class's names
                continue
            member = re.split(r"\s", decl)[-1]
            stack.append(member)
            fields.setdefault(".".join(s for s in stack if s), off)
        into[name] = {"size": size, "fields": fields}


def check(layout):
    """Dinkum profile: the dumped offsets against the headers' `// +0x..` comments (VC6's layout)."""
    sys.path.insert(0, str(HERE))
    import layout as L
    bad, n = [], 0
    for cls, members in L.header_offsets().items():
        if cls not in layout:
            continue
        for member, off in members.items():
            got = layout[cls]["fields"].get(member)
            if got is None:
                continue
            n += 1
            if got != off:
                bad.append(f"{cls}::{member}: header +{off:#x}, clang +{got:#x}")
    print(f"--check: {n} documented members compared, {len(bad)} differ")
    for b in bad[:40]:
        print("  " + b)
    return not bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--stl", choices=["dinkum", "msvc"], default="msvc")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    layout = {}
    for mod, rel in UNITS:
        parse(dump(mod, rel, a.stl), layout)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / f"layout-{a.stl}.json"
    path.write_text(json.dumps(layout, indent=0, sort_keys=True))
    print(f"{len(layout)} records -> {path}")
    if a.check and not check(layout):
        sys.exit(1)


if __name__ == "__main__":
    main()
