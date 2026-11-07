"""TEST ONLY: a matrix of headless simulations (EE-SIM.exe, tools/sim.py): seeds x settings, several at once.

Every cell of seeds x map sizes x map types x difficulties x AI counts x cheat sets x personalities x rules variants
is one EE-SIM.exe game (seeded, with the AI trace), `--repeat` times. `--jobs N` runs N games at once, each in its
own staged data root (build/sim/stage/<bench>-<k>). Output in build/sim/bench/<name>/: runs/<id>.json (the game's
report), runs/<id>.trace.txt (its EE-AI lines, the harness's session-log form), runs/<id>.err.txt (progress),
results.json and results.csv (one row per run and player). With --repeat 2 the repeats' traces are compared
(tools/harness/ai_trace.py compare: the determinism check).

  python -I tools/sim_bench.py --name base --seeds 2-7 --cheats none --personality balanced --jobs 2
  python -I tools/sim_bench.py --name floor --seeds 2,3 --vary aipersonalities.balanced.military.army_floor=0.1,0.2,0.3
  python -I tools/sim_bench.py --name det --seeds 2 --repeat 2

--set DOC.KEY=VALUE applies to every run; each --vary DOC.KEY=V1,V2,... adds a dimension (one run per value).
--env NAME=VALUE sets an environment variable for every run; each --env-vary NAME=V1,V2,... adds a dimension, e.g.
--env-vary EE_AI_DEMAND=1/0,0/1 plays a per-player switch from both seats (the per-player sets of ee/EECPTrace.h).
tools/ai_scorecard.py scores the result (wins, time to each epoch, economy and army over time).
--sim-arg passes an option to EE-SIM.exe as it is (e.g. --sim-arg=--lobby --sim-arg=181=#0).
"""
import argparse
import concurrent.futures
import csv
import itertools
import json
import os
import pathlib
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import sim  # noqa: E402

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


def run_one(run, args, slots, out):
    slot = slots.get()
    try:
        stage = sim.prepare(args.data, slot, run["sets"])
        cfg = stage / "config.yaml"
        cfg.unlink(missing_ok=True)
        rid = run["id"]
        report, trace, err = out / f"{rid}.json", out / f"{rid}.trace.txt", out / f"{rid}.err.txt"
        opts = ["--seed", str(run["seed"]), "--ai", str(run["ai"]), "--difficulty", run["difficulty"],
                "--map-size", run["size"], "--game-seconds", str(args.game_seconds),
                "--sample-seconds", str(args.sample_seconds), "--pt-rate", str(args.pt_rate),
                "--report", str(report), "--trace", str(trace), "--ai-trace", "--wall-timeout", str(args.wall_timeout)]
        if run["map"] != "-":
            opts += ["--map-type", run["map"]]
        if run["cheats"] != "-":
            opts += ["--cheats", run["cheats"]]
        if run["personality"] != "-":
            opts += ["--personality", run["personality"]]
        if args.human_defeat:
            opts.append("--human-defeat")
        for s in run["sets"]:
            opts += ["--set", s]
        opts += args.sim_arg
        # each slot its own TCP ports (sims picking a free pair at once could both take one, and the local server
        # then failed to publish: a crash at the main menu)
        if not any(o == "--port-offset" for o in args.sim_arg):
            opts += ["--port-offset", str(2000 + 64 * slots_index(slot))]
        t0 = time.monotonic()
        env = dict(os.environ, **run["env"])
        for attempt in range(2):   # one retry of a run that died without a report (a start-up fault)
            report.unlink(missing_ok=True)
            with open(err, "w", encoding="utf-8") as f:
                rc = subprocess.run(sim.command(args.exe, stage, opts, cfg), stdout=subprocess.DEVNULL, stderr=f,
                                    env=env).returncode
            try:
                rep = json.loads(report.read_text())
                break
            except (OSError, ValueError):
                rep = dict(outcome="no-report")
                print(f"== {rid} [slot {slot}]: rc {rc}, no report" + (": retrying" if attempt == 0 else ""), flush=True)
        wall = time.monotonic() - t0
        print(f"== {rid} [slot {slot}]: rc {rc} {rep.get('outcome')} game s {rep.get('game_ms', 0) // 1000} "
              f"in {wall:.1f} s (game part {rep.get('wall_seconds_game', 0):.1f} s)", flush=True)
        return dict(run, rc=rc, wall=round(wall, 2), report=rep)
    finally:
        slots.put(slot)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("--name", required=True)
    ap.add_argument("--seeds", default="2", help="comma separated, ranges a-b (default 2)")
    ap.add_argument("--sizes", default="Small")
    ap.add_argument("--maps", default="-", help="map type lines (- the lobby default, Continental)")
    ap.add_argument("--difficulty", default="Hard")
    ap.add_argument("--ai", default="2")
    ap.add_argument("--cheats", default="none", help="EE_AI_CHEATS sets, comma separated (- unset: all)")
    ap.add_argument("--personality", default="balanced", help="EE_AI_PERSONALITY values, ';' between variants")
    ap.add_argument("--set", action="append", default=[], help="DOC.KEY=VALUE for every run")
    ap.add_argument("--vary", action="append", default=[], help="DOC.KEY=V1,V2,...: one dimension")
    ap.add_argument("--env", action="append", default=[], help="NAME=VALUE for every run")
    ap.add_argument("--env-vary", action="append", default=[], help="NAME=V1,V2,...: one dimension")
    ap.add_argument("--repeat", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--game-seconds", type=int, default=2700)
    ap.add_argument("--sample-seconds", type=int, default=30)
    ap.add_argument("--pt-rate", type=int, default=40)
    ap.add_argument("--wall-timeout", type=int, default=3600)
    ap.add_argument("--human-defeat", action="store_true", help="let the idle human be defeated")
    ap.add_argument("--data", default="binary")
    ap.add_argument("--exe", type=pathlib.Path, default=sim.DEFAULT_EXE)
    ap.add_argument("--sim-arg", action="append", default=[])
    a = ap.parse_args()
    if not a.exe.exists():
        sys.exit(f"{a.exe} is missing: python -I build.py --sim")
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
    cells = list(itertools.product([int(s, 0) for s in split(a.seeds)], split(a.sizes), split(a.maps),
                                   split(a.difficulty), [int(x) for x in split(a.ai)], split(a.cheats),
                                   [p for p in a.personality.split(";") if p] or ["-"], range(len(variants)),
                                   range(len(envs))))
    runs = []
    for (seed, size, map_type, difficulty, ai, cheats, pers, v, ev), k in itertools.product(cells, range(a.repeat)):
        tag = "-".join([s.split(".")[-1] + "=" + s.split("=", 1)[1] for s in variants[v]] +
                       [n + "=" + x.replace("/", "~") for n, x in envs[ev].items()])
        rid = (f"s{seed}-{map_type.replace(' ', '_')}-{size}-{difficulty}-{ai}ai-{cheats.replace('/', '~')}"
               f"-{pers.replace(',', '+')}" + (f"-{tag}" if tag else "") + f"-r{k}").replace("--", "-")
        runs.append(dict(id=rid, seed=seed, size=size, map=map_type, difficulty=difficulty, ai=ai, cheats=cheats,
                         personality=pers, variant=variants[v], sets=a.set + variants[v], repeat=k,
                         env=dict(fixed, **envs[ev])))
    out = BENCH / a.name / "runs"
    out.mkdir(parents=True, exist_ok=True)
    print(f"{len(runs)} runs, {a.jobs} at once -> {out.parent}", flush=True)
    import queue
    slots = queue.Queue()
    for k in range(max(1, a.jobs)):
        slots.put(f"{a.name}-{k}" if a.jobs > 1 else a.name)
    t0 = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(max(1, a.jobs)) as ex:
        results = list(ex.map(lambda r: run_one(r, a, slots, out), runs))
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
                                                             wall_seconds=round(total, 1)), indent=2) + "\n")
    fields = ["run", "seed", "size", "map", "difficulty", "ai", "cheats", "personality", "variant", "repeat", "rc",
              "outcome", "game_s", "wall", "wall_game", "winner", "player", "human", "result", "epoch", "population",
              "victory_units", "citizens", "army", "buildings", "food", "wood", "stone", "gold", "iron"]
    with open(out.parent / "results.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for r in results:
            rep = r["report"]
            base = dict(run=r["id"], seed=r["seed"], size=r["size"], map=r["map"], difficulty=r["difficulty"],
                        ai=r["ai"], cheats=r["cheats"], personality=r["personality"], variant=";".join(r["variant"]),
                        repeat=r["repeat"], rc=r["rc"], outcome=rep.get("outcome"),
                        game_s=rep.get("game_ms", 0) // 1000, wall=r["wall"], wall_game=rep.get("wall_seconds_game"),
                        winner=rep.get("winner"))
            players = rep.get("players") or [{}]
            for p in players:
                w.writerow(dict(base, player=p.get("index"), human=p.get("human"), result=p.get("result"),
                                epoch=p.get("epoch"), population=p.get("population"),
                                victory_units=p.get("victory_units"), citizens=p.get("citizens"), army=p.get("army"),
                                buildings=p.get("buildings"), **(p.get("resources") or {})))
    failed = [r["id"] for r in results if r["rc"]]
    print(f"done: {len(results)} runs in {total:.0f} s ({len(failed)} failed) -> {out.parent / 'results.csv'}", flush=True)
    sys.exit(1 if failed or any(not c["identical"] for c in checks) else 0)


if __name__ == "__main__":
    main()
