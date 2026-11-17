"""TEST ONLY: a matrix of headless simulations (EE-SIM.exe, tools/sim.py): seeds x settings, several at once.

Every cell of seeds x map sizes x map types x difficulties x AI counts x cheat sets x personalities x engines x rules
variants is one EE-SIM.exe game (seeded, with the AI trace), `--repeat` times. `--jobs N` runs N games at once, each in its
own staged data root (build/sim/stage/<bench>-<k>). Output in build/sim/bench/<name>/: runs/<id>.json (the game's
report), runs/<id>.trace.txt (its EE-AI lines, the harness's session-log form), runs/<id>.err.txt (progress),
results.json and results.csv (one row per run and player). With --repeat 2 the repeats' traces are compared
(tools/harness/ai_trace.py compare: the determinism check).

  python -I tools/sim_bench.py --name base --seeds 2-7 --cheats none --personality balanced --jobs 2
  python -I tools/sim_bench.py --name floor --seeds 2,3 --vary aipersonalities.balanced.military.army_floor=0.1,0.2,0.3
  python -I tools/sim_bench.py --name det --seeds 2 --repeat 2
  python -I tools/sim_bench.py --name eng --seeds 2-7 --engine "classic;classic,boom"   # the engine by seat
  python -I tools/sim_bench.py --name v1 --sprt --engine classic,ageofai --ai-scripts X --base-ai-scripts Y --cache
  python -I tools/sim_bench.py --name v1 --queue --priority confirm --seeds 2-65 --engine classic,ageofai --cache

--set DOC.KEY=VALUE applies to every run; each --vary DOC.KEY=V1,V2,... adds a dimension (one run per value).
--env NAME=VALUE sets an environment variable for every run; each --env-vary NAME=V1,V2,... adds a dimension, e.g.
--env-vary EE_AI_DEMAND=1/0,0/1 plays a per-player switch from both seats (the per-player sets of ee/EECPTrace.h).
tools/ai_scorecard.py scores the result (wins, time to each epoch, economy and army over time).
--ai-scripts DIR plays the next engine's Lua scripts from DIR (EE_AI_SCRIPTS, e.g. data/ai: the checkout's); each run's
script errors (EE-SIM's "SIM next.error" lines) are counted in lua_errors.
--sim-arg passes an option to EE-SIM.exe as it is (e.g. --sim-arg=--lobby --sim-arg=181=#0).
The games run below normal priority (Windows: BELOW_NORMAL_PRIORITY_CLASS; POSIX: nice 10), so that the windowed
gates and a played game keep their CPU while a bench runs; --normal-priority turns that off.

--cache [DIR] reuses a finished game: a run whose EE-SIM.exe, data tree, rules documents, AI scripts, settings and seed
all equal an earlier cached run's copies that run's report, trace and progress instead of playing it (the game is
deterministic: the same inputs give the same game). The key hashes the exe's bytes, the data tree's Data files (names,
sizes, times; Data/ai and Data/db by content), the stage's generated rules documents, the EE_AI_SCRIPTS folder's
files by content (not its path), every option a run passes to EE-SIM.exe and every EE_* environment variable. DIR
defaults to the main checkout's build/sim/cache (shared by the worktrees: a different exe is a different key). Runs of
--repeat past the first are always played (the determinism check). Only finished games (rc 0, a report, no
wall-timeout) are stored. Delete the folder to drop the cache.

--screen SEEDS plays those seeds first (a subset of --seeds) and stops every variant whose screened player
(--screen-player, default 2: the first computer seat) is defeated clearly more often than the baseline's on the same
seeds: at least --screen-factor times (default 2) and --screen-margin more games (default 3). The others go on to
every seed. The baseline is --screen-baseline: a finished bench's folder (its runs matched by seed and matchup:
size, map, difficulty, AIs, cheats, personality, engine), or "first" (default): this bench's runs with the first --vary and
--env-vary values (matched in the same way), which always go on. With "first", engines, personalities and cheats are
matchups, not variants: to screen one engine against another, bench the baseline engine first and name its folder. A stopped variant keeps its first-stage rows in results.csv; results.json's "screen" has the counts.
16 seeds at a factor of 2 only stops gross regressions: defeats vary by several games in 32 from chance alone.

--sprt plays a variant against a baseline seed by seed and stops when the sequential test decides (tools/sim_sprt.py:
the model, the error rates, the replay of finished benches). The two sides of a pair are the same seed and matchup:
  - --base-exe EXE, --base-ai-scripts DIR, --base-set DOC.KEY=V, --base-env NAME=V: the baseline is this bench with
    those changed (its run ids end in -base; without --base-ai-scripts it plays without the variant's --ai-scripts);
  - or one --vary/--env-vary of two values: the first is the baseline;
  - or --sprt-baseline DIR[+DIR]: a finished bench's games (by seed and matchup).
The seeds (--seeds in their order, default 2 and on) start in turn, both sides of a seed together, --jobs games at
once; the test reads every seed whose games are all done, in seed order (an early defeat must not count first); no
seed starts after the decision (the games out finish, for the cache). It stops at H1 (better by about G1), H0 (not
better) or --sprt-max seeds (default 64: inconclusive; the W-L-C stands as before). --sprt-gains G0,G1 (default
0,0.1 a game, W 1, close 0.5, L 0), --sprt-stat ahead (which build was further ahead at the cap, default 0,0.2),
--sprt-alpha/--sprt-beta (0.05), --sprt-player (2). results.json's "sprt" has the verdict and the LLR by seed. With
the cache the baseline's games are usually free: only the variant's are played.

--queue plays the games on the shared bench queue (tools/bench.py: one daemon for every checkout, started when none
runs): it picks the slots and ports (--port-base is not used), plays identical games (the same cache key) once for
every bench that asks, and starts the games by --priority (confirm before explore), then first come. --jobs is then
how many games an --sprt bench has out at once; a fixed bench submits all its games. Killing the bench drops its
queued games. tools/bench.py pause stops the queue for the gates.
"""
import argparse
import concurrent.futures
import csv
import gzip
import hashlib
import itertools
import json
import os
import pathlib
import shutil
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import sim  # noqa: E402
import sim_sprt  # noqa: E402

BENCH = ROOT / "build" / "sim" / "bench"


def split(text):
    out = []
    for part in str(text).split(","):
        part = part.strip()
        if "-" in part and part.replace("-", "").isdigit() and not part.startswith("-"):
            a, b = part.split("-", 1)
            out += [str(v) for v in range(int(a), int(b) + 1)]
        elif part:
            out.append(part)
    return out


def slots_index(slot):
    """the slot's number (NAME-k -> k; a single slot -> 0)"""
    tail = slot.rsplit("-", 1)[-1]
    return int(tail) if tail.isdigit() else 0


# ------------------------------------------------------------------------------------------------ the cache

CACHE_FORMAT = 1
NOT_INPUTS = {"EE_ORIG_SRC_ROOT", "EE_LLVM", "EE_XBUILD_OUT", "EE_SIM_CACHE", "EE_BENCH_QUEUE_PORT"}   # tool settings
_memo = {}
_memo_lock = threading.Lock()


def file_hash(path):
    st = path.stat()
    key = ("file", str(path), st.st_size, st.st_mtime_ns)
    with _memo_lock:
        if key in _memo:
            return _memo[key]
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    with _memo_lock:
        _memo[key] = h.hexdigest()
    return _memo[key]


def folder_hash(folder, content=True):
    """every file under the folder: its relative path and its content (or its size and time)"""
    folder = pathlib.Path(folder)
    if not folder.is_dir():
        return "missing"
    h = hashlib.sha256()
    for f in sorted(p for p in folder.rglob("*") if p.is_file()):
        rel = f.relative_to(folder).as_posix().lower()
        if content:
            h.update(f"{rel}\0{file_hash(f)}\n".encode())
        else:
            st = f.stat()
            h.update(f"{rel}\0{st.st_size}\0{st.st_mtime_ns}\n".encode())
    return h.hexdigest()


def tree_hash(data):
    """the data tree: open_data.json, Data/ai and Data/db by content, the rest of Data by name, size and time"""
    key = ("tree", data)
    with _memo_lock:
        if key in _memo:
            return _memo[key]
    tree = sim.data_tree(data)
    h = hashlib.sha256()
    meta = tree / "open_data.json"
    h.update(meta.read_bytes() if meta.exists() else b"-")
    h.update(sim.overlay_hash(tree).encode())   # the stage's Data/ai and Data/db: this checkout's (sim.stage_tree)
    for sub in sorted((tree / "Data").iterdir()):
        h.update(f"{sub.name.lower()}\0".encode())
        h.update((folder_hash(sub, sub.name.lower() in ("ai", "db")) if sub.is_dir() else file_hash(sub)).encode())
    with _memo_lock:
        _memo[key] = h.hexdigest()
    return _memo[key]


def doc_hash(path):
    """a rules document as the stage holds it (sim.stage_tree: this checkout's data/db, LF line ends)"""
    if not path.exists():
        return "-"
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def cache_key(run, args, opts):
    """-> (the hash of everything a game's result depends on (the module doc), its parts); opts: game_opts"""
    env = {k: v for k, v in run_env(run).items() if k.upper().startswith("EE_") and k.upper() not in NOT_INPUTS}
    scripts = env.pop("EE_AI_SCRIPTS", None)
    # the options without the run's own paths and ports (they do not change the game)
    skip = {"--report", "--trace", "--port-offset", "--wall-timeout", "--log"}
    game_opts, k = [], 0
    while k < len(opts):
        if opts[k] in skip:
            k += 2
            continue
        game_opts.append(opts[k])
        k += 1
    db = ROOT / "data" / "db"   # (the stage's Data/db is a copy of it)
    docs = sorted({s.split(".", 1)[0] for s in run["sets"]})
    parts = dict(format=CACHE_FORMAT, exe=file_hash(run_exe(run, args).resolve()), tree=tree_hash(args.data),
                 docs={d: doc_hash(db / f"{d}.yaml") for d in docs},
                 scripts=folder_hash(scripts) if scripts else None, env=sorted(env.items()), opts=game_opts)
    return hashlib.sha256(json.dumps(parts, sort_keys=True).encode()).hexdigest(), parts


def cache_dir(arg):
    """--cache's folder: DIR, or the main checkout's build/sim/cache"""
    if arg not in (None, "", "default"):
        return pathlib.Path(arg).resolve()
    first = subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"], capture_output=True,
                           text=True).stdout.split("\n")[0]
    main = first.split(" ", 1)[1].strip() if first.startswith("worktree ") else ""
    return (pathlib.Path(main) if main else ROOT) / "build" / "sim" / "cache"


def cache_get(cache, key, report, trace, err):
    """copy a cached run's files to the run's paths -> its meta (rc, wall, lua_errors), or None"""
    d = cache / key[:2] / key
    if not (d / "meta.json").exists():
        return None
    try:
        meta = json.loads((d / "meta.json").read_text())
        shutil.copyfile(d / "report.json", report)
        with gzip.open(d / "trace.txt.gz", "rb") as src, open(trace, "wb") as dst:
            shutil.copyfileobj(src, dst)
        shutil.copyfile(d / "err.txt", err)
        return meta
    except (OSError, ValueError, EOFError):
        return None


def cache_put(cache, key, parts, report, trace, err, meta):
    d = cache / key[:2] / key
    if (d / "meta.json").exists():
        return
    tmp = cache / key[:2] / f".{key}.{os.getpid()}.{threading.get_ident()}"
    try:
        tmp.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(report, tmp / "report.json")
        with open(trace, "rb") as src, gzip.open(tmp / "trace.txt.gz", "wb", compresslevel=6) as dst:
            shutil.copyfileobj(src, dst)
        shutil.copyfile(err, tmp / "err.txt")
        (tmp / "key.json").write_text(json.dumps(parts, indent=1, sort_keys=True) + "\n")
        (tmp / "meta.json").write_text(json.dumps(meta) + "\n")
        tmp.rename(d)   # whole or not at all
    except OSError:
        pass   # another bench stored it first, or the disk is full: the run itself is fine
    finally:
        if tmp.exists():
            shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------------------------------------------------ a run

def run_exe(run, args):
    """the run's EE-SIM exe: its side's (--base-exe), else --exe"""
    return pathlib.Path(run.get("exe") or args.exe)


def run_env(run):
    """the run's environment: this process's, with the run's variables (a None value unsets one: a baseline side
    without the variant's --ai-scripts)"""
    env = dict(os.environ, **run["env"])
    return {k: v for k, v in env.items() if v is not None}


def priority(args):
    """subprocess options for a game below normal priority (Windows), unless --normal-priority"""
    if args.normal_priority or sys.platform != "win32":
        return {}
    return dict(creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)


def lowered(command, args):
    """POSIX: the command under nice (a preexec_fn is not safe with the bench's threads)"""
    if args.normal_priority or sys.platform == "win32" or not shutil.which("nice"):
        return command
    return ["nice", "-n", "10", *command]


def game_opts(run, args):
    """EE-SIM's options for a run, without its own paths, ports and time-out (the cache key's opts)"""
    opts = ["--seed", str(run["seed"]), "--ai", str(run["ai"]), "--difficulty", run["difficulty"],
            "--map-size", run["size"], "--game-seconds", str(args.game_seconds),
            "--sample-seconds", str(args.sample_seconds), "--pt-rate", str(args.pt_rate), "--ai-trace"]
    if run["map"] != "-":
        opts += ["--map-type", run["map"]]
    if args.start_epoch:
        opts += ["--lobby", "183=" + args.start_epoch]   # the lobby's starting epoch (late-start benches)
    if run["cheats"] != "-":
        opts += ["--cheats", run["cheats"]]
    if run["personality"] != "-":
        opts += ["--personality", run["personality"]]
    if run["engine"] != "-":
        opts += ["--engine", run["engine"]]
    if args.human_defeat:
        opts.append("--human-defeat")
    for s in run["sets"]:
        opts += ["--set", s]
    return opts + list(args.sim_arg)


def run_one(run, args, slots, out):
    """a run in the next free slot of this bench (its ports: --port-base + 64 per slot)"""
    slot = slots.get()
    try:
        return play(run, args, slot, args.port_base + 64 * slots_index(slot), out)
    finally:
        slots.put(slot)


def play(run, args, slot, port_offset, out, quiet=False):
    """a run in a slot's stage with that port offset, its files in out -> its result (the cached game's, or the one
    played; with --cache a finished game is stored)"""
    rid = run["id"]
    report, trace, err = out / f"{rid}.json", out / f"{rid}.trace.txt", out / f"{rid}.err.txt"
    opts = game_opts(run, args)
    key = None
    if args.cache_dir is not None and run["repeat"] == 0:
        key, parts = cache_key(run, args, opts)
        meta = cache_get(args.cache_dir, key, report, trace, err)
        if meta is not None:
            rep = json.loads(report.read_text())
            print(f"== {rid} [cache {key[:12]}]: {rep.get('outcome')} game s {rep.get('game_ms', 0) // 1000} "
                  f"(played in {meta['wall']:.1f} s)", flush=True)
            return dict(run, rc=meta["rc"], wall=meta["wall"], report=rep, lua_errors=meta["lua_errors"],
                        cached=key)
    stage = sim.prepare(args.data, slot, run["sets"])
    cfg = stage / "config.yaml"
    cfg.unlink(missing_ok=True)
    opts += ["--report", str(report), "--trace", str(trace), "--wall-timeout", str(args.wall_timeout)]
    # each slot its own TCP ports (sims picking a free pair at once could both take one, and the local server
    # then failed to publish: a crash at the main menu)
    if not any(o == "--port-offset" for o in args.sim_arg):
        opts += ["--port-offset", str(port_offset)]
    t0 = time.monotonic()
    env = run_env(run)
    for attempt in range(2):   # one retry of a start-up fault: no report, or an error before the game ran
        report.unlink(missing_ok=True)
        with open(err, "w", encoding="utf-8") as f:
            rc = subprocess.run(lowered(sim.command(run_exe(run, args), stage, opts, cfg), args),
                                stdout=subprocess.DEVNULL, stderr=f, env=env, **priority(args)).returncode
        try:
            rep = json.loads(report.read_text())
        except (OSError, ValueError):
            rep = dict(outcome="no-report")
        # (the lobby's "slot 0 human: index #0 outside the combo's 0 lines": a menu race, the game never started)
        if rep.get("outcome") not in ("no-report", "error") or rep.get("game_ms", 0) > 0:
            break
        print(f"== {rid} [slot {slot}]: rc {rc}, {rep.get('outcome')} {rep.get('error', '')} before the game"
              + (": retrying" if attempt == 0 else ""), flush=True)
    wall = time.monotonic() - t0
    # the next engine's script errors (EE-SIM writes "SIM next.error ..." to stderr: a seat without orders that step)
    lua_errors = sum(1 for x in err.read_text(errors="replace").splitlines() if x.startswith("SIM next.error"))
    print(f"== {rid} [slot {slot}]: rc {rc} {rep.get('outcome')} game s {rep.get('game_ms', 0) // 1000} "
          f"in {wall:.1f} s (game part {rep.get('wall_seconds_game', 0):.1f} s)"
          + (f", {lua_errors} script errors" if lua_errors else ""), flush=True)
    if key is not None and rc == 0 and rep.get("outcome") not in ("no-report", "error", "wall-timeout") \
            and trace.exists():
        cache_put(args.cache_dir, key, parts, report, trace, err,
                  dict(rc=rc, wall=round(wall, 2), lua_errors=lua_errors, run=rid))
    return dict(run, rc=rc, wall=round(wall, 2), report=rep, lua_errors=lua_errors)


# ------------------------------------------------------------------------------------------------ the queue

class QueueClient:
    """--queue: the runs played by the shared daemon (tools/bench.py), which this starts when none runs. start(run)
    -> a Future of run_one's result. A game found in --cache is taken here, without the daemon."""
    ARGS = ("data", "game_seconds", "sample_seconds", "pt_rate", "wall_timeout", "start_epoch", "human_defeat",
            "sim_arg", "normal_priority")

    def __init__(self, args, out):
        import bench
        self.args, self.out = args, out
        self.conn = bench.ensure_daemon()
        self.lock = threading.Lock()
        self.pending = {}   # request id -> (run, Future)
        self.ids = itertools.count(1)
        self.conn.send(dict(op="hello", who=f"{ROOT.name}:{args.name}", pid=os.getpid()))
        hello = self.conn.recv()
        print(f"queue: the daemon plays {hello['jobs']} games at once" + (" (PAUSED)" if hello["paused"] else ""),
              flush=True)
        self.environ = {k: v for k, v in os.environ.items()
                        if k.upper().startswith("EE_") and k.upper() not in NOT_INPUTS}
        threading.Thread(target=self.reader, daemon=True).start()

    def start(self, run):
        f = concurrent.futures.Future()
        a = self.args
        key = None
        if run["repeat"] == 0:   # (the determinism check's repeats are always played)
            key, _ = cache_key(run, a, game_opts(run, a))
            if a.cache_dir is not None:
                rid = run["id"]
                meta = cache_get(a.cache_dir, key, self.out / f"{rid}.json", self.out / f"{rid}.trace.txt",
                                 self.out / f"{rid}.err.txt")
                if meta is not None:
                    rep = json.loads((self.out / f"{rid}.json").read_text())
                    print(f"== {rid} [cache {key[:12]}]: {rep.get('outcome')} game s {rep.get('game_ms', 0) // 1000} "
                          f"(played in {meta['wall']:.1f} s)", flush=True)
                    f.set_result(dict(run, rc=meta["rc"], wall=meta["wall"], report=rep,
                                      lua_errors=meta["lua_errors"], cached=key))
                    return f
        spec = dict(tool=str(HERE / "bench.py"), who=f"{ROOT.name}:{a.name}", rid=run["id"], run=run,
                    environ=self.environ, args=dict({k: getattr(a, k) for k in self.ARGS}, exe=str(a.exe.resolve()),
                                                    data=a.data if a.data == "open" else str(pathlib.Path(a.data).resolve()),
                                                    cache_dir=str(a.cache_dir) if a.cache_dir else None))
        with self.lock:
            n = next(self.ids)
            self.pending[n] = (run, f)
        self.conn.send(dict(op="submit", id=n, key=key, priority=a.priority, job=spec))
        return f

    def reader(self):
        while True:
            try:
                msg = self.conn.recv()
            except (OSError, ValueError):
                msg = None
            if msg is None:
                with self.lock:
                    for run, f in self.pending.values():
                        f.set_exception(RuntimeError("the queue daemon went away (tools/bench.py status)"))
                    self.pending.clear()
                return
            if msg.get("op") == "queued":
                if msg.get("shared"):
                    print(f"queue: {self.pending.get(msg['id'], ({'id': '?'},))[0]['id']} shares job {msg['job']} "
                          f"({msg['state']})", flush=True)
                continue
            if msg.get("op") != "done":
                continue
            with self.lock:
                run, f = self.pending.pop(msg["id"], (None, None))
            if f is None:
                continue
            try:
                f.set_result(self.finished(run, msg))
            except Exception as e:   # noqa: BLE001 (handed to the bench's thread)
                f.set_exception(e)

    def finished(self, run, msg):
        """a game the daemon finished: its files copied to this bench under the run's id -> run_one's result"""
        res, rid, src = msg["result"], run["id"], pathlib.Path(msg["dir"])
        for ext in (".json", ".trace.txt", ".err.txt"):
            if (src / f"{msg['rid']}{ext}").exists():
                shutil.copyfile(src / f"{msg['rid']}{ext}", self.out / f"{rid}{ext}")
        try:
            rep = json.loads((self.out / f"{rid}.json").read_text())
        except (OSError, ValueError):
            rep = dict(outcome="no-report", error=res.get("error", ""))
        if res.get("error"):
            print(f"== {rid} [queue]: {res['error']}", flush=True)
        else:
            print(f"== {rid} [queue {src.name}]: rc {res.get('rc')} {rep.get('outcome')} game s "
                  f"{rep.get('game_ms', 0) // 1000} in {res.get('wall', 0):.1f} s", flush=True)
        out = dict(run, rc=res.get("rc", -1), wall=res.get("wall", 0), report=rep, lua_errors=res.get("lua_errors", 0))
        if res.get("cached"):
            out["cached"] = res["cached"]
        return out


# ------------------------------------------------------------------------------------------------ the screening

MATCHUP = ("size", "map", "difficulty", "ai", "cheats", "personality", "engine")


def variant_of(run):
    """a run's variant: everything but the seed and the repeat"""
    return json.dumps([run[k] for k in MATCHUP] + [run["variant"], sorted(run["env"].items()), run.get("side")])


def defeated(report, player):
    """1 when the player was defeated, 0 when not, None without a report"""
    for p in report.get("players") or []:
        if p.get("index") == player:
            return 1 if p.get("result") == 2 else 0
    return None


def screening(results, a):
    """{variant: dict(label, seeds, defeats, baseline, stopped)} over the first stage's results"""
    base = {}   # (seed, matchup) -> defeated
    match = lambda r: (r["seed"],) + tuple(r[k] for k in MATCHUP)
    # "first": the runs with the first --vary and --env-vary values (the first run's)
    first = (results[0]["variant"], results[0]["env"]) if results and a.screen_baseline == "first" else None
    is_base = lambda r: first is not None and (r["variant"], r["env"]) == first
    if first is not None:
        for r in results:
            if is_base(r) and r["repeat"] == 0:
                base[match(r)] = defeated(r["report"], a.screen_player)
    else:
        bench = pathlib.Path(a.screen_baseline)
        for r in json.loads((bench / "results.json").read_text()).get("runs", []):
            key = (r["seed"],) + tuple(r[k] for k in MATCHUP)
            if r.get("repeat", 0) != 0:
                continue
            if key in base:
                sys.exit(f"--screen-baseline {bench}: two runs of seed {r['seed']} in one matchup")
            base[key] = defeated(r["report"], a.screen_player)
    rows = {}
    for r in results:
        if r["repeat"] != 0:
            continue
        v = variant_of(r)
        row = rows.setdefault(v, dict(label=r["id"].split("-", 1)[1].rsplit("-r", 1)[0].strip("-"), seeds=0,
                                      defeats=0, baseline=0, stopped=False, base=is_base(r)))
        d, b = defeated(r["report"], a.screen_player), base.get(match(r))
        if d is None or b is None:
            continue   # a run without a report, or a seed the baseline lacks: not compared
        row["seeds"] += 1
        row["defeats"] += d
        row["baseline"] += b
    for v, row in rows.items():
        row["stopped"] = not row["base"] and row["seeds"] > 0 and row["defeats"] >= max(
            a.screen_factor * row["baseline"], row["baseline"] + a.screen_margin)
        print(f"SCREEN {row['label']}: p{a.screen_player} defeated {row['defeats']} against the baseline's "
              f"{row['baseline']} in {row['seeds']} seeds: {'STOPPED' if row['stopped'] else 'goes on'}", flush=True)
    return rows


# ------------------------------------------------------------------------------------------------ the sequential test

def sequential(runs, a, start, window):
    """--sprt: the runs seed by seed (both sides of a seed together, at most `window` games out at once), the test fed
    the pairs of every seed whose games are all done, in seed order (a game that ends early, a defeat, must not
    come first); no seed is started after the decision (the games out finish, for the cache) -> (results, summary)"""
    test = sim_sprt.Test(a.sprt_stat, tuple(float(x) for x in a.sprt_gains.split(",")) if a.sprt_gains else None,
                         a.sprt_alpha, a.sprt_beta, 10 ** 9, a.sprt_min)
    print(test.describe().replace(f"..{test.max_pairs} pairs", f" pairs at least; at most {a.sprt_max} seeds"),
          flush=True)
    matchup = lambda r: (r["seed"],) + tuple(str(r[k]) for k in MATCHUP)
    base = {}
    if a.sprt_baseline:   # the baseline's games from finished benches
        base = {k: rep for k, rep in sim_sprt.bench_runs(a.sprt_baseline).items()}
    order, by_seed = [], {}
    for r in runs:
        if r["seed"] not in by_seed:
            order.append(r["seed"])
        by_seed.setdefault(r["seed"], []).append(r)
    done, out, ds, scores, path = {}, {}, [], [], []
    decision, value, k, played = None, 0.0, 0, 0
    nxt = iter(order)
    while True:
        while decision is None and len(out) < window:
            seed = next(nxt, None)
            if seed is None:
                break
            for r in by_seed[seed]:
                out[start(r)] = r
        if not out:
            break
        finished, _ = concurrent.futures.wait(out, return_when=concurrent.futures.FIRST_COMPLETED)
        for f in finished:
            done[out.pop(f)["id"]] = f.result()
        while decision is None and k < len(order) and all(r["id"] in done for r in by_seed[order[k]]):
            seed = order[k]
            k += 1
            played += 1
            sides = {}
            for r in by_seed[seed]:
                sides.setdefault(matchup(r), {})[r["side"] or "variant"] = done[r["id"]]["report"]
            for key, pair in sorted(sides.items()):
                b = pair.get("base", base.get((key[0], key[1:])))
                if b is None or "variant" not in pair:
                    print(f"SPRT seed {seed}: no baseline game for {key[1:]}: not counted", flush=True)
                    continue
                po = sim_sprt.pair_outcome(a.sprt_stat, b, pair["variant"], a.sprt_player)
                if po is None:
                    print(f"SPRT seed {seed}: a game without a result: not counted", flush=True)
                    continue
                ds.append(po[0])
                scores.append(po)
            decision, value = test.verdict(ds) if ds else (None, 0.0)
            if decision is None and played >= a.sprt_max:
                decision = "max"
            path.append(dict(seed=seed, pairs=len(ds), llr=round(value, 3)))
            print(f"SPRT seed {seed}: {len(ds)} pairs, LLR {value:+.2f} ({test.lower:.2f}, {test.upper:.2f})"
                  + (f": {sim_sprt.Test.word(decision)}" if decision else ""), flush=True)
    results = [done[r["id"]] for r in runs if r["id"] in done]
    verdict = (f"{sim_sprt.Test.word(decision)} after {played} seeds, {len(ds)} pairs, LLR {value:+.2f}; "
               f"{sim_sprt.summary(scores)}")
    return results, dict(test=test.describe(), decision=decision, seeds=played, pairs=len(ds), llr=round(value, 3),
                         verdict=verdict, path=path, games=len(results))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("--name", required=True)
    ap.add_argument("--seeds", help="comma separated, ranges a-b (default 2; with --sprt 2 and on, --sprt-max of them)")
    ap.add_argument("--sizes", default="Small")
    ap.add_argument("--maps", default="-", help="map type lines (- the lobby default, Continental)")
    ap.add_argument("--difficulty", default="Hard")
    ap.add_argument("--ai", default="2")
    ap.add_argument("--cheats", default="none", help="EE_AI_CHEATS sets, comma separated (- unset: all)")
    ap.add_argument("--personality", default="balanced", help="EE_AI_PERSONALITY values, ';' between variants")
    ap.add_argument("--engine", default="-", help="EE-SIM --engine values (classic: the manifest's classic AI, "
                                                  "classic_lua; random; an AI id from "
                                                  "Data/ai/ais.json such as boom; or next: its first AI; ',' by seat), "
                                                  "';' between variants (- the lobby's Classic)")
    ap.add_argument("--set", action="append", default=[], help="DOC.KEY=VALUE for every run")
    ap.add_argument("--start-epoch", default="", help="the lobby's starting epoch line (e.g. 'Copper Age'); "
                    "late-start benches (AI_RESEARCH 8.69)")
    ap.add_argument("--vary", action="append", default=[], help="DOC.KEY=V1,V2,...: one dimension")
    ap.add_argument("--env", action="append", default=[], help="NAME=VALUE for every run")
    ap.add_argument("--env-vary", action="append", default=[], help="NAME=V1,V2,...: one dimension")
    ap.add_argument("--repeat", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--game-seconds", type=int, default=2700)
    ap.add_argument("--sample-seconds", type=int, default=30)
    ap.add_argument("--pt-rate", type=int, default=40)
    ap.add_argument("--wall-timeout", type=int, default=3600)
    ap.add_argument("--port-base", type=int, default=2000,
                    help="the first slot's port offset (64 more per slot); give benches run at the same time ranges apart, e.g. 2000 and 6000")
    ap.add_argument("--human-defeat", action="store_true", help="let the idle human be defeated")
    ap.add_argument("--normal-priority", action="store_true",
                    help="run the games at normal priority (default: below normal, so that a windowed game or the "
                         "gates keep their CPU while a bench runs; the game's results do not depend on it)")
    ap.add_argument("--data", default="open")
    ap.add_argument("--exe", type=pathlib.Path, default=sim.DEFAULT_EXE)
    ap.add_argument("--sim-arg", action="append", default=[])
    ap.add_argument("--ai-scripts", type=pathlib.Path, help="EE_AI_SCRIPTS for every run (made absolute): the next "
                                                            "engine's scripts from this folder first (e.g. ai)")
    ap.add_argument("--cache", nargs="?", const="default", metavar="DIR",
                    help="reuse finished games with the same inputs (see above; default DIR: the main checkout's "
                         "build/sim/cache)")
    ap.add_argument("--screen", metavar="SEEDS", help="the first stage's seeds (see above)")
    ap.add_argument("--screen-baseline", default="first",
                    help="a finished bench folder, or first (this bench's first variant)")
    ap.add_argument("--screen-player", type=int, default=2)
    ap.add_argument("--screen-factor", type=float, default=2.0)
    ap.add_argument("--screen-margin", type=int, default=3)
    ap.add_argument("--queue", action="store_true", help="play the games on the shared queue (tools/bench.py; see "
                                                         "above); --jobs: how many this bench has out at once with "
                                                         "--sprt")
    ap.add_argument("--priority", choices=("confirm", "explore"), default="explore",
                    help="--queue: confirmations go before explorations (default explore)")
    ap.add_argument("--base-exe", type=pathlib.Path, help="the baseline side's EE-SIM exe (see --sprt)")
    ap.add_argument("--base-ai-scripts", type=pathlib.Path, help="the baseline side's EE_AI_SCRIPTS (see --sprt)")
    ap.add_argument("--base-set", action="append", default=[], help="DOC.KEY=VALUE for the baseline side only")
    ap.add_argument("--base-env", action="append", default=[], help="NAME=VALUE for the baseline side only")
    ap.add_argument("--sprt", action="store_true", help="the sequential test of the variant against the baseline "
                                                        "(see above)")
    ap.add_argument("--sprt-baseline", metavar="DIR", help="the baseline: finished bench folders (DIR+DIR)")
    ap.add_argument("--sprt-stat", choices=sim_sprt.STATS, default="wlc")
    ap.add_argument("--sprt-gains", metavar="G0,G1", help="the mean gain a pair under H0 and H1 (default 0,0.1; "
                                                          "ahead 0,0.2); --sprt-gains=-0.1,0: no regression")
    ap.add_argument("--sprt-alpha", type=float, default=0.05)
    ap.add_argument("--sprt-beta", type=float, default=0.05)
    ap.add_argument("--sprt-max", type=int, default=64, help="the most seeds (default 64)")
    ap.add_argument("--sprt-min", type=int, default=8, help="the fewest pairs before a decision (default 8)")
    ap.add_argument("--sprt-player", type=int, default=2, help="the player scored (default 2: the first computer)")
    a = ap.parse_args()
    if a.seeds is None:
        a.seeds = f"2-{1 + a.sprt_max}" if a.sprt else "2"
    base_side = bool(a.base_exe or a.base_ai_scripts or a.base_set or a.base_env)
    if a.sprt and (a.screen or a.repeat != 1):
        sys.exit("--sprt: not with --screen or --repeat")
    a.cache_dir = cache_dir(a.cache) if a.cache else None
    if not a.exe.exists():
        sys.exit(f"{a.exe} is missing: {sim.BUILD_HINT}")
    variants = [[]]
    for v in a.vary:
        key, _, values = v.partition("=")
        if not values:
            sys.exit(f"--vary {v}: expected DOC.KEY=V1,V2,...")
        variants = [old + [f"{key}={x}"] for old in variants for x in values.split(",")]
    envs = [{}]
    for v in a.env_vary:
        key, _, values = v.partition("=")
        if not values:
            sys.exit(f"--env-vary {v}: expected NAME=V1,V2,...")
        envs = [dict(old, **{key: x}) for old in envs for x in values.split(",")]
    fixed = dict(e.split("=", 1) for e in a.env)
    if a.ai_scripts:
        fixed["EE_AI_SCRIPTS"] = str(a.ai_scripts.resolve())
    # the sides: the variant (every option above) and, with --base-*, the baseline (those options changed)
    sides = [dict(side=None, exe=None, env={}, sets=[], tag="")]
    if base_side:
        benv = dict(e.split("=", 1) for e in a.base_env)
        if a.base_ai_scripts:
            benv["EE_AI_SCRIPTS"] = str(a.base_ai_scripts.resolve())
        elif "EE_AI_SCRIPTS" in fixed:
            benv["EE_AI_SCRIPTS"] = None   # (unset: the stage's Data/ai)
        if a.base_exe and not a.base_exe.exists():
            sys.exit(f"--base-exe {a.base_exe}: missing")
        sides = [dict(side="base", exe=str(a.base_exe.resolve()) if a.base_exe else None, env=benv,
                      sets=list(a.base_set), tag="-base"), dict(side="variant", exe=None, env={}, sets=[], tag="")]
    seeds = [int(s, 0) for s in split(a.seeds)]
    if a.sprt:
        seeds = seeds[:a.sprt_max]
        if not base_side and not a.sprt_baseline and len(variants) * len(envs) != 2:
            sys.exit("--sprt needs a baseline: --base-exe/--base-ai-scripts/--base-set/--base-env, --sprt-baseline "
                     "DIR, or one --vary or --env-vary of two values (the first the baseline)")
    cells = list(itertools.product(seeds, split(a.sizes), split(a.maps),
                                   split(a.difficulty), [int(x) for x in split(a.ai)], split(a.cheats),
                                   [p for p in a.personality.split(";") if p] or ["-"],
                                   [e for e in a.engine.split(";") if e] or ["-"], range(len(variants)),
                                   range(len(envs)), range(len(sides))))
    runs = []
    for (seed, size, map_type, difficulty, ai, cheats, pers, eng, v, ev, sd), k in itertools.product(cells, range(a.repeat)):
        side = sides[sd]
        tag = "-".join([s.split(".")[-1] + "=" + s.split("=", 1)[1] for s in variants[v]] +
                       [n + "=" + x.replace("/", "~") for n, x in envs[ev].items()])
        rid = (f"s{seed}-{map_type.replace(' ', '_')}-{size}-{difficulty}-{ai}ai-{cheats.replace('/', '~')}"
               f"-{pers.replace(',', '+').replace('/', '~')}" + (f"-eng={eng.replace(',', '+')}" if eng != "-" else "")
               + (f"-{tag}" if tag else "") + side["tag"] + f"-r{k}").replace("--", "-")
        vside = side["side"] if base_side else ("base" if (v, ev) == (0, 0) else "variant") if a.sprt else None
        runs.append(dict(id=rid, seed=seed, size=size, map=map_type, difficulty=difficulty, ai=ai, cheats=cheats,
                         personality=pers, engine=eng, variant=variants[v], sets=a.set + variants[v] + side["sets"],
                         repeat=k, env=dict(fixed, **envs[ev], **side["env"]), side=vside, exe=side["exe"]))
    out = BENCH / a.name / "runs"
    out.mkdir(parents=True, exist_ok=True)
    print(f"{len(runs)} runs, {a.jobs} at once -> {out.parent}", flush=True)
    import queue
    slots = queue.Queue()
    for k in range(max(1, a.jobs)):
        slots.put(f"{a.name}-{k}" if a.jobs > 1 else a.name)
    t0 = time.monotonic()

    pool = concurrent.futures.ThreadPoolExecutor(max(1, a.jobs))
    start = lambda r: pool.submit(run_one, r, a, slots, out)
    if a.queue:
        start = QueueClient(a, out).start

    def play(batch):
        return [f.result() for f in [start(r) for r in batch]]
    screen = sprt = None
    if a.sprt:
        results, sprt = sequential(runs, a, start, max(2, a.jobs))   # (--queue: --jobs games out at once)
    elif a.screen:
        first = {int(s, 0) for s in split(a.screen)}
        stage1 = [r for r in runs if r["seed"] in first]
        print(f"screen: the first stage, {len(stage1)} runs (seeds {a.screen})", flush=True)
        results = play(stage1)
        screen = screening(results, a)
        stopped = {v for v, row in screen.items() if row["stopped"]}
        rest = [r for r in runs if r["seed"] not in first and variant_of(r) not in stopped]
        print(f"screen: {len(stopped)} of {len(screen)} variants stopped; the second stage, {len(rest)} runs",
              flush=True)
        results += play(rest)
        order = {r["id"]: k for k, r in enumerate(runs)}
        results.sort(key=lambda r: order[r["id"]])
        screen = list(screen.values())
    else:
        results = play(runs)
    pool.shutdown()
    total = time.monotonic() - t0
    # determinism: the repeats of a cell print the same trace
    sys.path.insert(0, str(HERE / "harness"))
    import ai_trace  # noqa: E402
    import io
    checks = []
    by_cell = {}
    for r in results:
        by_cell.setdefault(r["id"].rsplit("-r", 1)[0], []).append(r)
    for cell, rs in by_cell.items():
        for other in rs[1:]:
            buf = io.StringIO()
            same = ai_trace.compare(out / f"{rs[0]['id']}.trace.txt", out / f"{other['id']}.trace.txt", out=buf)
            checks.append(dict(cell=cell, a=rs[0]["id"], b=other["id"], identical=same, detail=buf.getvalue().strip()))
            print(f"DETERMINISM {cell}: {'identical' if same else 'DIFFERENT'}: {buf.getvalue().strip().splitlines()[-1]}",
                  flush=True)
    (out.parent / "results.json").write_text(json.dumps(dict(test_only=True, runs=results, determinism=checks,
                                                             wall_seconds=round(total, 1), screen=screen, sprt=sprt),
                                                        indent=2) + "\n")
    fields = ["run", "seed", "size", "map", "difficulty", "ai", "cheats", "personality", "engine", "variant", "side", "repeat", "rc",
              "cached", "lua_errors", "outcome", "game_s", "wall", "wall_game", "winner", "player", "human", "result", "epoch", "population",
              "victory_units", "citizens", "army", "buildings", "food", "wood", "stone", "gold", "iron"]
    with open(out.parent / "results.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in results:
            rep = r["report"]
            base = dict(run=r["id"], seed=r["seed"], size=r["size"], map=r["map"], difficulty=r["difficulty"],
                        ai=r["ai"], cheats=r["cheats"], personality=r["personality"], engine=r["engine"],
                        variant=";".join(r["variant"]), side=r.get("side") or "",
                        repeat=r["repeat"], rc=r["rc"], cached=1 if r.get("cached") else 0, lua_errors=r.get("lua_errors", 0), outcome=rep.get("outcome"),
                        game_s=rep.get("game_ms", 0) // 1000, wall=r["wall"], wall_game=rep.get("wall_seconds_game"),
                        winner=rep.get("winner"))
            players = rep.get("players") or [{}]
            for p in players:
                w.writerow(dict(base, player=p.get("index"), human=p.get("human"), result=p.get("result"),
                                epoch=p.get("epoch"), population=p.get("population"),
                                victory_units=p.get("victory_units"), citizens=p.get("citizens"), army=p.get("army"),
                                buildings=p.get("buildings"), **(p.get("resources") or {})))
    failed = [r["id"] for r in results if r["rc"]]
    errors = sum(r.get("lua_errors", 0) for r in results)
    cached = sum(1 for r in results if r.get("cached"))
    print(f"done: {len(results)} runs in {total:.0f} s ({len(failed)} failed"
          + (f", {cached} from the cache" if cached else "")
          + (f", {errors} script errors" if errors else "") + f") -> {out.parent / 'results.csv'}", flush=True)
    if sprt:
        print(f"SPRT: {sprt['verdict']}", flush=True)
    sys.exit(1 if failed or any(not c["identical"] for c in checks) else 0)


if __name__ == "__main__":
    main()
