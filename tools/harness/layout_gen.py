"""Record layouts of a build, by member name (the harness's offsets, and the exe link's member aliases).

  python -I tools/harness/layout_gen.py [--out build/clang]      (tools/build.py runs it before the exe link)

clang dumps the record layout of every class a translation unit sees (-fdump-record-layouts-complete, private members
and STL internals included). The dumps of the units in UNITS (together they see every class the harness and the
member table read) are parsed into

  <out>/layout.json   {class: {"size": n, "fields": {"member.sub.sub": offset}}}

read through layout.py. The VC6 layout the original image and ee's harness offsets use is frozen in
tools/harness/vc6_layout.json.gz (the last Dinkum/VC6-compatible build's dump; it reproduced all 3,386 member
offsets documented in include/), and layout.translate maps a VC6 offset to this build's by member name.
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
import clang_build    # noqa: E402
import units          # noqa: E402
import verify         # noqa: E402
from ee import CLANG, MODULE_FLAGS   # noqa: E402

# (module, unit): together they include every class the harness and the exe's member table read
UNITS = [
    ("lle", "Low-Level Engine/User Interface/UIManager.cpp"),
    ("lle", "Low-Level Engine/User Interface/UIForm.cpp"),
    ("lle", "Low-Level Engine/Graphics Engine/GERasterizer.cpp"),
    ("lle", "Low-Level Engine/Input System/ISMouse.cpp"),
    ("lle", "Low-Level Engine/Network Engine/NESynchronousServer.cpp"),
    ("dx7hr", "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Screen.cpp"),
    ("dx7hr", "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/Common/DX7Rasterizer.cpp"),
    ("exe", "Empire Earth/Empire Earth/Empire Earth.cpp"),
    ("exe", "Empire Earth/Empire Earth/EEWorld.cpp"),
    ("exe", "Empire Earth/Physics Thread/EEPhysics.cpp"),
    ("exe", "Empire Earth/AI Thread/EEComputerPlayer.cpp"),
    ("exe", "Empire Earth/Units/EEComplexUnit.cpp"),
    ("exe", "Empire Earth/Player/EEPlayer.cpp"),
    ("exe", "Empire Earth/Communications/EEMessage.cpp"),
]
LINE = re.compile(r"^\s*(\d+)(?::(\d+)-(\d+))? \|( *)(.*)$")


def dump(mod, rel):
    """clang's record-layout dump of one unit, compiled as the build compiles it."""
    src = (ROOT / "src" / rel).resolve()
    staged = verify.stage_source(src)
    iflags, inc = verify.include_paths(staged)
    flags = [f for f in clang_build.CLANG_FLAGS if f != "/c"] + ["/std:c++17"]
    cmd = [str(CLANG), *flags, *clang_build.clang_flags(verify.unit_flags(src, MODULE_FLAGS[mod]), "-O1"),
           *verify.DEFINES.split(), *iflags, "/Zs", "-Xclang", "-fdump-record-layouts-complete", str(staged)]
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
        name = re.sub(r"^(class|struct|union) ", "", m.group(5).strip())
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
            if decl.endswith(("(base)", "(base) (empty)", "(primary base)")) or "vftable pointer" in decl \
                    or "vbtable pointer" in decl or "(virtual base)" in decl:
                stack.append(None)   # base subobject: its members join the derived class's names
                continue
            stack.append(re.split(r"\s", decl)[-1])
            fields.setdefault(".".join(s for s in stack if s), off)
        into[name] = {"size": size, "fields": fields}


def generate(out, overlays=None, extra=()):
    """Write <out>/layout.json for the build in <out> (overlays/extra: accepted for the build's options; layouts do
    not depend on them)."""
    layout = {}
    for mod, rel in UNITS:
        parse(dump(mod, rel), layout)
    path = pathlib.Path(out) / "layout.json"
    path.write_text(json.dumps(layout, indent=0, sort_keys=True))
    print(f"layout: {len(layout)} records -> {path}", flush=True)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=str(ROOT / "build" / "clang"))
    a = ap.parse_args()
    generate(pathlib.Path(a.out).resolve())


if __name__ == "__main__":
    main()
