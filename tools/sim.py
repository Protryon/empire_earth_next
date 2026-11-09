"""TEST ONLY: run the headless simulation EE-SIM.exe (build.py --sim; src/Empire Earth/Empire Earth/EESim.cpp).

EE-SIM.exe plays a seeded computer-vs-computer random-map game with the game's own code (no window, rendering,
sound or debugger harness) as fast as the CPU allows and writes a JSON report. It reads the data root EE_DATA_ROOT
and writes into it (error logs, the folders saved games go to), so every run gets a staged copy of a data tree here:
the read-only files hard-linked (the tree itself is never written), the folders the game writes into copied, as the
harness stages it (tools/harness/exe_boot.py stage_tree). A stage is kept and reused per slot (build/sim/stage/<slot>).

  python -I tools/sim.py [--data binary|open|DIR] [--slot NAME] [--exe EXE] -- [EE-SIM options]
  python -I tools/sim.py -- --seed 3 --ai 2 --difficulty Hard --game-seconds 2700 --report r.json --ai-trace --trace t.txt
  python -I tools/sim.py -- --seed 3 --engine next,classic --report r.json

--engine LIST sets the computer seats' engines (classic, random, an AI's id from Data/ai/ais.json such as boom, or next
for its first AI; docs/AI_LUA_PLAN.md S0 and 3.3) through the lobby's engine boxes: one for every seat, or a comma list by
seat (seat 1 is player 2), taken in turn.

--ai-scripts DIR sets EE_AI_SCRIPTS (absolute): the next engine reads its Lua scripts from DIR before the tree's
Data/ai (docs/AI_LUA_PLAN.md 3.2), e.g. --ai-scripts ai to play the checkout's scripts on an older tree.

--set doc.key=value overrides a rules document (db/<doc>.yaml); a value "[a, b]" replaces a list, e.g.
--set "gameoptions.difficulties.hard.ai_cheats=[cost, reveal, epochs, extra, popband, catchup, retire]" (the original's cheats,
for every computer player of that difficulty; EE_AI_CHEATS narrows them per player). The binary tree has no rules
documents (the game plays with the code's defaults, which equal the documents tools/eedata/rules.py writes), so for
an override the stage gets that document written by rules.py from the same constants (python -I
tools/eedata/eedata.py check keeps them equal).
"""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OPEN_DATA = ROOT / "build" / "open_data"
STAGES = ROOT / "build" / "sim" / "stage"
DEFAULT_EXE = ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"
WRITABLE = {"saved games", "scenarios"}   # Data folders the game writes into: copied, not linked (exe_boot.py)
RULE_DOCS = ("combat", "gameoptions", "civbuilder", "civpowers", "aipersonalities", "epochs")


def data_tree(kind):
    """The data tree: 'binary' (build/open_data/tree-binary), 'open' (build/open_data/tree) or a directory. The main
    checkout's trees are used when this checkout has none (a worktree; they are only read)."""
    if kind not in ("binary", "open"):
        tree = pathlib.Path(kind)
    else:
        name = "tree-binary" if kind == "binary" else "tree"
        tree = OPEN_DATA / name
        if not (tree / "Data").is_dir():
            main = pathlib.Path(subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"],
                                               capture_output=True, text=True).stdout.split("\n")[0].split(" ", 1)[1])
            tree = main / "build" / "open_data" / name
    if not (tree / "Data").is_dir():
        sys.exit(f"{tree}: no Data folder (python -I tools/open_data.py build ...)")
    # a tree built by other converter code may not load in this build (the text db and rules formats change with the
    # code; the harness rebuilds such a tree, this tool only reads it)
    sys.path.insert(0, str(ROOT / "tools"))
    import open_data  # noqa: E402
    made = json.loads((tree / "open_data.json").read_text()).get("converters") if (tree / "open_data.json").exists() else None
    if made != open_data.converters_hash():
        print(f"sim: warning: {tree} was built by other converter code ({made} != {open_data.converters_hash()}) "
              f"or is being built", file=sys.stderr)
    ai = json.loads((tree / "open_data.json").read_text()).get("ai") if (tree / "open_data.json").exists() else None
    if ai != open_data.scripts_hash() and not os.environ.get("EE_AI_SCRIPTS"):
        print(f"sim: warning: {tree}'s Data/ai is not this checkout's ai/ (python -I tools/open_data.py ai TREE, or "
              f"--ai-scripts)", file=sys.stderr)
    return tree.resolve()


def stage_tree(tree, d):
    """Stage a data tree into d (once per tree): Data's files hard-linked, the written folders, Users and the root
    files copied."""
    marker = d / "sim_stage.json"
    want = dict(tree=str(tree), made=(tree / "open_data.json").read_text() if (tree / "open_data.json").exists() else "")
    if marker.exists() and json.loads(marker.read_text()) == want:
        return d
    if d.exists():
        shutil.rmtree(d)
    d.mkdir(parents=True)
    for item in tree.iterdir():
        if item.name == "open_data.json":
            continue
        if item.is_file():
            shutil.copy(item, d / item.name)
        elif item.name.lower() != "data":
            shutil.copytree(item, d / item.name)
    for src in (tree / "Data").rglob("*"):
        rel = src.relative_to(tree / "Data")
        dst = d / "Data" / rel
        if src.is_dir():
            dst.mkdir(parents=True, exist_ok=True)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        if rel.parts[0].lower() in WRITABLE or len(rel.parts) == 1:
            shutil.copy(src, dst)
            continue
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy(src, dst)
    marker.write_text(json.dumps(want))
    return d


def rule_docs(stage, docs):
    """Write the rules documents an override needs into the stage's Data/db when the tree has none (tools/eedata
    rules.py: the code's defaults). A document the tree has is left as it is."""
    db = stage / "Data" / "db"
    missing = [n for n in docs if not (db / f"{n}.yaml").exists() or (db / f".sim-{n}").exists()]
    if not missing:
        return []
    sys.path.insert(0, str(ROOT / "tools" / "eedata"))
    import rules  # noqa: E402
    spec = json.loads((ROOT / "tools" / "eedata" / "specs.json").read_text())
    written = []
    for name in missing:
        # a document without references to the db tables' ids (aipersonalities, gameoptions) is written without the
        # tables' text; the others need the open tree
        try:
            text = rules.GENERATORS[name](rules._Ids({}), spec) if name in rules.GENERATORS else None
        except Exception:
            text = None
        if text is None:
            sys.exit(f"--set {name}: the data tree has no db/{name}.yaml and tools/eedata/rules.py cannot write it "
                     f"without the db tables' text: use --data open")
        (db / f"{name}.yaml").write_text(text, encoding="utf-8", newline="\n")
        (db / f".sim-{name}").write_text("written by tools/sim.py\n")
        written.append(name)
    return written


def prepare(data="binary", slot="default", sets=()):
    """-> the staged data root for a slot, with the rules documents the overrides name."""
    tree = data_tree(data)
    stage = stage_tree(tree, STAGES / slot)
    docs = sorted({s.split(".", 1)[0] for s in sets})
    db = stage / "Data" / "db"
    for marker in db.glob(".sim-*"):   # a document written for an earlier run's override: gone again
        name = marker.name[len(".sim-"):]
        if name not in docs:
            (db / f"{name}.yaml").unlink(missing_ok=True)
            marker.unlink()
    rule_docs(stage, docs)
    return stage


def command(exe, stage, args, config=None):
    cfg = config or (stage / "config.yaml")
    return [str(exe), "--data", str(stage), "--config", str(cfg), *args]


def resolve(map_path, addresses):
    """The functions of exe addresses (an "EE-SIM FAULT" line's frames) from the exe's map -> [(address, name+off)]."""
    import bisect
    import re
    line = re.compile(r'^ [0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)?', re.I)
    syms = []
    for text in pathlib.Path(map_path).read_text(errors="replace").splitlines():
        m = line.match(text)
        if m:
            syms.append((int(m[2], 16), m[1], m[3] or ""))
    syms.sort()
    keys = [s[0] for s in syms]
    out = []
    for a in addresses:
        k = bisect.bisect_right(keys, a) - 1
        out.append((a, f"{syms[k][1]}+{a - syms[k][0]:#x} ({syms[k][2]})" if k >= 0 and a - syms[k][0] < 0x100000
                    else "?"))
    return out


def main():
    if sys.argv[1:2] == ["resolve"]:   # tools/sim.py resolve [--map MAP] ADDR... (or a FAULT line)
        args = sys.argv[2:]
        map_path = DEFAULT_EXE.with_suffix(".map")
        if args[:1] == ["--map"]:
            map_path, args = pathlib.Path(args[1]), args[2:]
        addrs = [int(x, 16) for x in " ".join(args).replace(";", " ").split() if x.lower().startswith(("0x", "00"))]
        for a, name in resolve(map_path, addrs):
            print(f"{a:016x} {name}")
        return
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("--data", default="binary", help="binary (default), open, or a tree directory")
    ap.add_argument("--slot", default="default", help="the stage's name (one per concurrent run)")
    ap.add_argument("--exe", type=pathlib.Path, default=DEFAULT_EXE)
    ap.add_argument("--ai-scripts", type=pathlib.Path, help="EE_AI_SCRIPTS: the next engine's scripts read from this "
                                                            "folder first (e.g. ai, the checkout's)")
    ap.add_argument("args", nargs=argparse.REMAINDER, help="EE-SIM.exe's options (after --)")
    a = ap.parse_args()
    args = a.args[1:] if a.args[:1] == ["--"] else a.args
    if not a.exe.exists():
        sys.exit(f"{a.exe} is missing: python -I build.py --sim")
    sets = [args[k + 1] for k, v in enumerate(args) if v == "--set" and k + 1 < len(args)]
    if a.ai_scripts:
        os.environ["EE_AI_SCRIPTS"] = str(a.ai_scripts.resolve())   # absolute: the game runs in its data root
    stage = prepare(a.data, a.slot, sets)
    cfg = stage / "config.yaml"
    cfg.unlink(missing_ok=True)   # a fresh config.yaml per run
    sys.exit(subprocess.run(command(a.exe, stage, args, cfg)).returncode)


if __name__ == "__main__":
    main()
