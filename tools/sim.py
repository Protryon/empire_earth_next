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
  python -I tools/sim.py -- --seed 3 --engine classic,boom --report r.json

--engine LIST sets the computer seats' engines (classic: the manifest's "classic" AI, classic_lua; random; an AI's id from Data/ai/ais.json such as boom; or next for its first AI; docs/AI_LUA_PLAN.md S0 and
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
    released media that tools/fetch_data.py unpacks into data/ (build/data-root, this checkout's or the main one's:
    a machine without the retail installs)."""
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
            if not (tree / "Data").is_dir():
                sys.path.insert(0, str(ROOT / "tools"))
                import open_data  # noqa: E402
                tree = open_data.fetched_root(ROOT) or open_data.fetched_root(main) or tree
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


# ------------------------------------------------------------------------------------------------ several games a process

# EE-SIM's options that belong to one game (EESim.cpp GameOption; the value-less ones in GAME_FLAGS): a batch process
# takes them per game, the rest (--set, --cheats, --personality, --ai-trace, --pt-rate, --autosave-ms, --port-offset,
# --data, --config, --quiet) hold for the process
GAME_VALUED = {"--seed", "--ai", "--map-size", "--map-type", "--difficulty", "--lobby", "--game-seconds",
               "--sample-seconds", "--wall-timeout", "--flare", "--volcano", "--engine", "--report", "--trace", "--log",
               "--err", "--out", "--id"}
GAME_FLAGS = {"--human-defeat", "--no-human-defeat", "--keep-human"}
PROCESS_VALUED = {"--set", "--cheats", "--personality", "--pt-rate", "--autosave-ms", "--port-offset", "--data",
                  "--config"}


def split_args(args):
    """EE-SIM options -> (process options, game options), each in its given order"""
    proc, game = [], []
    k = 0
    while k < len(args):
        a = args[k]
        if a in GAME_VALUED or a in PROCESS_VALUED:
            (game if a in GAME_VALUED else proc).extend(args[k:k + 2])
            k += 2
        else:
            (game if a in GAME_FLAGS else proc).append(a)
            k += 1
    return proc, game


def game_line(args):
    """a game's options as a line of EE-SIM --games (EESim.cpp Words: "..." quotes, \\" a quote)"""
    return " ".join('"' + str(a).replace('"', '\\"') + '"' for a in args)


class Batch:
    """One EE-SIM process that plays games one after another (EE-SIM --games -: one game's options a line on stdin,
    read when the last game has ended). The start-up (the exe, the database, the menus' media) is paid once; each game
    is left as a player leaves it and the next one starts from the main menu, with a report and trace equal to the
    game's run alone (PORT_LEARNINGS.md "The headless simulation", several games a process).

      with sim.Batch(stage, popen=dict(creationflags=...)) as b:
          r = b.play(["--seed", "3", "--engine", "classic,boom", "--ai-trace", "--report", rep, "--trace", tr],
                     err=err_path, env=dict(os.environ, EE_AI_EXP="x"))
          # r: dict(rc, outcome, wall, game_wall, restarted, report: the parsed report or None)

    play() takes a run's whole option list: the game's options go on the line, the process's (--set, --cheats,
    --personality, --ai-trace, ...) and the environment start the process; a game whose process options or environment
    differ from the running process's (or the exe, Batch.exe) starts a new process first. A game that fails (rc 2 or 3: an error, the wall
    timeout) or crashes ends its process; the next play() starts a new one. err (the game's stderr: its progress, script
    errors, a fault's frames) is the game's own file even in a shared process. A batch process is not safe to share
    between threads: one Batch per slot (its stage, its --port-offset)."""

    def __init__(self, stage, exe=DEFAULT_EXE, config=None, popen=None, wrap=None, log=None):
        self.stage = pathlib.Path(stage)
        self.exe = exe
        self.config = config or (self.stage / "config.yaml")
        self.popen = popen or {}
        self.wrap = wrap or (lambda cmd: cmd)   # e.g. sim_bench's nice
        self.log = log                          # a file for the process's own stderr (start-up, between games)
        self.proc = None
        self.key = None
        self.lines = None
        self.index = 0
        self.games = 0       # games played by the current process
        self.processes = 0   # processes started

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def close(self):
        if self.proc is None:
            return
        try:
            self.proc.stdin.close()   # EOF: the process ends after its game
            self.proc.wait(timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            self.proc.kill()
            self.proc.wait()
        self.proc = None

    def _start(self, proc_args, env):
        import queue
        import threading
        self.close()
        self.config.unlink(missing_ok=True)   # a fresh config.yaml per process
        cmd = self.wrap(command(self.exe, self.stage, [*proc_args, "--games", "-"], self.config))
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                     env=env, text=True, errors="replace", bufsize=1, **self.popen)
        self.lines = queue.Queue()
        proc, lines, log = self.proc, self.lines, self.log

        def pump():
            with open(log, "a", encoding="utf-8") if log else open(os.devnull, "w") as f:
                for line in proc.stderr:
                    if line.startswith("EE-SIM GAME ") or line.startswith("EE-SIM BATCH"):
                        lines.put(line.rstrip("\n"))
                    f.write(line)
            lines.put(None)
        threading.Thread(target=pump, daemon=True).start()
        self.index = 0
        self.games = 0
        self.processes += 1

    def play(self, args, err=None, env=None, timeout=None):
        """one game (EE-SIM options; --report is required, read back) -> dict(rc, outcome, wall, ...)"""
        import time
        proc_args, game_args = split_args([str(a) for a in args])
        report = pathlib.Path(game_args[game_args.index("--report") + 1])
        if err:
            game_args += ["--err", str(err)]
        env = dict(os.environ if env is None else env)
        key = (str(self.exe), tuple(proc_args), tuple(sorted(env.items())))
        restarted = False
        if self.proc is None or self.proc.poll() is not None or key != self.key:
            self._start(proc_args, env)
            self.key = key
            restarted = True
        report.unlink(missing_ok=True)
        t0 = time.monotonic()
        index = self.index
        self.index += 1
        try:
            self.proc.stdin.write(game_line(game_args) + "\n")
            self.proc.stdin.flush()
        except OSError:
            pass
        rc, outcome = None, None
        deadline = None if timeout is None else t0 + timeout
        while True:
            try:
                line = self.lines.get(timeout=None if deadline is None else max(0.1, deadline - time.monotonic()))
            except Exception:   # queue.Empty: the wall limit
                self.proc.kill()
                rc, outcome = -9, "killed"
                break
            if line is None:    # the process ended without the game's end: a crash
                rc = self.proc.wait()
                break
            parts = line.split()
            if parts[:3] == ["EE-SIM", "GAME", str(index)] and parts[3] == "END":
                rc, outcome = int(parts[4]), parts[5] if len(parts) > 5 else None
                break
        wall = time.monotonic() - t0
        self.games += 1
        try:
            rep = json.loads(report.read_text())
        except (OSError, ValueError):
            rep = None
        if rc != 0:
            self.close()   # a failed game's process is not reused (its state is unknown)
        if rep is None and err:
            with open(err, "a", encoding="utf-8") as f:
                f.write(f"sim.Batch: the process ended with {rc} during the game (no report)\n")
        return dict(rc=rc, outcome=(rep or {}).get("outcome", outcome or "no-report"), wall=round(wall, 2),
                    game_wall=(rep or {}).get("wall_seconds_game"), restarted=restarted, report=rep)


def play_games(games, stage, exe=DEFAULT_EXE, env=None, popen=None, wrap=None, retries=1, on_result=None):
    """A list of games in one process after the other (Batch): games is a list of dicts with "args" (EE-SIM options
    with --report; the process options must agree to share a process) and optionally "err"; a crashed or failed game
    is retried `retries` times when it failed before its game ran (no game time), then recorded; the rest go on in a
    new process. on_result(game, result) is called after each game; returning False stops the list. -> the results in
    order (None for the games not played)"""
    results = [None] * len(games)
    with Batch(stage, exe=exe, popen=popen, wrap=wrap) as b:
        for k, g in enumerate(games):
            for attempt in range(retries + 1):
                r = b.play(g["args"], err=g.get("err"), env=env)
                if r["rc"] == 0 or (r["report"] or {}).get("game_ms", 0) > 0:
                    break
            results[k] = r
            if on_result is not None and on_result(g, r) is False:
                break
    return results


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
