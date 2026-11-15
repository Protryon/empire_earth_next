"""TEST ONLY: run the headless simulation EE-SIM.exe (build.py --sim; src/Empire Earth/Empire Earth/EESim.cpp).

On Linux and macOS the simulation is ee-sim (tools/xbuild.py sim or game: the game's exe under that name; ee-aoc
--sim is the same), by default build/xbuild/<host>/game/ee-sim; --exe takes either.

EE-SIM.exe plays a seeded computer-vs-computer random-map game with the game's own code (no window, rendering,
sound or debugger harness) as fast as the CPU allows and writes a JSON report. It reads the data root EE_DATA_ROOT
and writes into it (error logs, the folders saved games go to), so every run gets a staged copy of a data tree here:
the read-only files hard-linked (the tree itself is never written), the folders the game writes into copied, as the
harness stages it (tools/harness/exe_boot.py stage_tree). A stage is kept and reused per slot (build/sim/stage/<slot>).

  python -I tools/sim.py [--data open|DIR] [--slot NAME] [--exe EXE] -- [EE-SIM options]
  python -I tools/sim.py -- --seed 3 --ai 2 --difficulty Hard --game-seconds 2700 --report r.json --ai-trace --trace t.txt
  python -I tools/sim.py -- --seed 3 --engine classic,classic_cpp --report r.json

--engine LIST sets the computer seats' engines (classic: the manifest's "classic" AI, classic_lua; classic_cpp: the C++
classic; random; an AI's id from Data/ai/ais.json such as boom; or next for its first AI; docs/AI_LUA_PLAN.md S0 and
3.3) through the lobby's engine boxes: one for every seat, or a comma list by seat (seat 1 is player 2), taken in
turn.

The stage's Data/ai and Data/db are always this checkout's data/ai and data/db, whatever the tree holds.
--ai-scripts DIR sets EE_AI_SCRIPTS (absolute): the next engine reads its Lua scripts from DIR before Data/ai
(docs/AI_LUA_PLAN.md 3.2), e.g. a test folder (tests/ai_units).

--set doc.key=value overrides a rules document (db/<doc>.yaml); a value "[a, b]" replaces a list, e.g.
--set "gameoptions.difficulties.hard.ai_cheats=[cost, reveal, epochs, extra, popband, catchup, retire]" (the original's cheats,
for every computer player of that difficulty; EE_AI_CHEATS narrows them per player).
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


def default_exe():
    """EE-SIM.exe (build.py --sim) on Windows; elsewhere ee-sim beside the game (tools/xbuild.py sim; EE_XBUILD_OUT
    moves it as it moves the build)"""
    import platform
    if sys.platform == "win32":
        return ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"
    arch = "arm64" if platform.machine().lower() in ("arm64", "aarch64") else "x86_64"
    host = f"{'macos' if sys.platform == 'darwin' else 'linux'}-{arch}"
    return pathlib.Path(os.environ.get("EE_XBUILD_OUT", ROOT / "build" / "xbuild")) / host / "game" / "ee-sim"


DEFAULT_EXE = default_exe()
BUILD_HINT = "python -I build.py --sim" if sys.platform == "win32" else "python3 -I tools/xbuild.py sim"
WRITABLE = {"saved games", "scenarios"}   # Data folders the game writes into: copied, not linked (exe_boot.py)


def data_tree(kind):
    """The data tree: 'open' (build/open_data/tree) or a directory. The main
    checkout's trees are used when this checkout has none (a worktree; they are only read); 'open' is last the
    released data that tools/fetch_data.py pulls into build/data (a machine without the retail installs)."""
    if kind == "binary":
        sys.exit("--data binary: there is no binary tree any more (the game reads only the text db tables): --data open")
    if kind != "open":
        tree = pathlib.Path(kind)
    else:
        name = "tree"
        tree = OPEN_DATA / name
        if not (tree / "Data").is_dir():
            main = pathlib.Path(subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"],
                                               capture_output=True, text=True).stdout.split("\n")[0].split(" ", 1)[1])
            tree = main / "build" / "open_data" / name
            if not (tree / "Data").is_dir() and kind == "open" and (main / "build" / "data" / "Data").is_dir():
                tree = main / "build" / "data"
    if not (tree / "Data").is_dir():
        sys.exit(f"{tree}: no Data folder (python -I tools/open_data.py build ...; without the retail installs: "
                 f"python -I tools/fetch_data.py, then --data open)")
    # a tree built by other converter code may not load in this build (the text db and rules formats change with the
    # code; the harness rebuilds such a tree, this tool only reads it)
    sys.path.insert(0, str(ROOT / "tools"))
    import open_data  # noqa: E402
    made = json.loads((tree / "open_data.json").read_text()).get("converters") if (tree / "open_data.json").exists() else None
    if made is not None and made != open_data.converters_hash():   # (a fetched tree records no converters)
        print(f"sim: warning: {tree} was built by other converter code ({made} != {open_data.converters_hash()}) "
              f"or is being built", file=sys.stderr)
    return tree.resolve()


def overlay_hash(tree):
    """what stage_tree writes over a tree's Data/ai and Data/db: this checkout's data/ai and data/db"""
    sys.path.insert(0, str(ROOT / "tools"))
    import open_data  # noqa: E402
    return f"ai={open_data.scripts_hash()} db={open_data.db_hash()}"


def stage_tree(tree, d):
    """Stage a data tree into d (once per tree and checkout data): Data's files hard-linked, the written folders, Users
    and the root files copied; then Data/ai and Data/db replaced by this checkout's data/ai and data/db (copies), so
    a run never plays a stale tree's tables or scripts."""
    marker = d / "sim_stage.json"
    want = dict(tree=str(tree), made=(tree / "open_data.json").read_text() if (tree / "open_data.json").exists() else "",
                checkout=overlay_hash(tree))
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
    import open_data  # noqa: E402  (overlay_hash put tools/ on the path)
    open_data.refresh_scripts(d)
    open_data.refresh_db(d)
    marker.write_text(json.dumps(want))
    return d


def prepare(data="open", slot="default", sets=()):
    """-> the staged data root for a slot."""
    tree = data_tree(data)
    return stage_tree(tree, STAGES / slot)   # (sets: the overrides apply in the game; the documents are all there)


def command(exe, stage, args, config=None):
    cfg = config or (stage / "config.yaml")
    # the game's own POSIX exe runs the simulation with --sim first (ee-sim and EE-SIM.exe need none)
    sim = ["--sim"] if pathlib.Path(exe).name == "ee-aoc" else []
    return [str(exe), *sim, "--data", str(stage), "--config", str(cfg), *args]


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
    ap.add_argument("--data", default="open", help="open (default: build/open_data/tree) or a tree directory")
    ap.add_argument("--slot", default="default", help="the stage's name (one per concurrent run)")
    ap.add_argument("--exe", type=pathlib.Path, default=DEFAULT_EXE)
    ap.add_argument("--ai-scripts", type=pathlib.Path, help="EE_AI_SCRIPTS: the next engine's scripts read from this "
                                                            "folder first (e.g. ai, the checkout's)")
    ap.add_argument("args", nargs=argparse.REMAINDER, help="EE-SIM.exe's options (after --)")
    a = ap.parse_args()
    args = a.args[1:] if a.args[:1] == ["--"] else a.args
    if not a.exe.exists():
        sys.exit(f"{a.exe} is missing: {BUILD_HINT}")
    sets = [args[k + 1] for k, v in enumerate(args) if v == "--set" and k + 1 < len(args)]
    if a.ai_scripts:
        os.environ["EE_AI_SCRIPTS"] = str(a.ai_scripts.resolve())   # absolute: the game runs in its data root
    stage = prepare(a.data, a.slot, sets)
    cfg = stage / "config.yaml"
    cfg.unlink(missing_ok=True)   # a fresh config.yaml per run
    sys.exit(subprocess.run(command(a.exe, stage, args, cfg)).returncode)


if __name__ == "__main__":
    main()
