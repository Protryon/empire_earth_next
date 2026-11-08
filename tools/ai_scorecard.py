"""TEST ONLY: the AI scorecard of tools/sim_bench.py runs (docs/AI_RESEARCH.md): per AI variant, the wins, the time to
every epoch, the economy and army over time and the citizens lost.

  python -I tools/ai_scorecard.py build/sim/bench/NAME [more bench dirs] [--key demand] [--json OUT]

The computer players are told apart by a field of their trace start line (`--key`: engine, exp, demand, personality, cheats; by
default the first of those that differs between the players of a game, else "all": one variant). In a game of two
variants a side wins when the other has no citizens left or its citizens + army are at least 1.5 times the other's,
else the game is close. `--score mixed` (docs/AI_RESEARCH.md §8.29; the owner: "a mix of factors, economy, military")
judges by economy, military and tech instead: each a side's share of the pair's total (economy: the cost of its live
units and buildings plus its stock; military: its army's hit points x attack; tech: its epoch + 1), weighted
MIXED_WEIGHTS; a side wins with a share of at least MIXED_WIN, or when the other has no citizens left (reports
from before the samples' strength and value fall back to the body count). Epochs: how many of the variant's players reached each epoch and the median and mean game
minute (the report's epoch_ms). Samples: the mean citizens and army at 10, 20, 30, 40 minutes (the report's samples;
a defeated player counts as 0). Losses: citizens lost in the first 20 minutes and in all (the trace's losses lines).
"""
import argparse
import collections
import json
import pathlib
import re
import statistics
import sys

KEYS = ("engine", "exp", "demand", "personality", "cheats")
MARKS = (10, 20, 30, 40)
MIXED_WEIGHTS = (0.4, 0.4, 0.2)   # economy, military, tech
MIXED_WIN = 0.6


def mixed_share(a, b):
    """Player a's weighted share of economy, military and tech against player b (final report rows)."""
    def share(x, y):
        return 0.5 if x + y <= 0 else x / (x + y)
    eco = lambda p: p.get("value", 0) + sum(p.get("resources", {}).values())
    parts = (share(eco(a), eco(b)), share(a.get("strength", 0), b.get("strength", 0)),
             share(a.get("epoch", 0) + 1, b.get("epoch", 0) + 1))
    return sum(w * s for w, s in zip(MIXED_WEIGHTS, parts))


def start_fields(trace):
    """{player: {field: value}} from the trace's start lines"""
    out = {}
    for line in open(trace, errors="replace"):
        m = re.match(r"debugstring EE-AI \d+ p(\d+) cp start (.*?) \|", line)
        if m:
            out[m[1]] = dict(kv.split("=", 1) for kv in m[2].split() if "=" in kv)
    return out


def losses(trace):
    """{player: (citizens lost by 20 min, citizens lost)}"""
    early, total = collections.Counter(), collections.Counter()
    for line in open(trace, errors="replace"):
        m = re.match(r"debugstring EE-AI (\d+) p(\d+) cp losses citizens=(\d+)", line)
        if m:
            total[m[2]] += int(m[3])
            if int(m[1]) <= 20 * 60000:
                early[m[2]] += int(m[3])
    return {p: (early[p], total[p]) for p in total}


def at_minute(samples, index, minute):
    """(citizens, army) of the player in the last sample at or before the minute"""
    best = None
    for s in samples:
        if s["game_ms"] <= minute * 60000:
            best = s
    if best is None:
        return None
    for p in best["players"]:
        if str(p["index"]) == index:
            return p["citizens"], p["army"]
    return 0, 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bench", nargs="+", type=pathlib.Path)
    ap.add_argument("--key", choices=KEYS + ("variant",),
                    help="variant: group by the bench's --vary/--env-vary values (both players alike)")
    ap.add_argument("--json", type=pathlib.Path)
    ap.add_argument("--score", choices=("bodies", "mixed"), default="bodies",
                    help="mixed: economy, military and tech shares (see above)")
    a = ap.parse_args()
    games = []
    for d in a.bench:
        variants = {}
        if (d / "results.json").exists():
            for run in json.loads((d / "results.json").read_text()).get("runs", []):
                tag = " ".join(run.get("variant", []) + [f"{k}={v}" for k, v in sorted(run.get("env", {}).items())])
                variants[run["id"]] = tag or "-"
        for rep in sorted((d / "runs").glob("*.json")):
            trace = rep.with_name(rep.stem + ".trace.txt")
            if not trace.exists():
                continue
            r = json.loads(rep.read_text())
            fields = start_fields(trace)
            if not fields:
                continue
            if a.key == "variant":
                for f in fields.values():
                    f["variant"] = variants.get(rep.stem, "-")
            key = a.key or next((k for k in KEYS if len({f.get(k) for f in fields.values()}) > 1), None)
            games.append(dict(id=rep.stem, report=r, fields=fields, key=key, losses=losses(trace)))
    if not games:
        sys.exit("no runs with a report and a trace")
    keys = {g["key"] for g in games}
    if len(keys) > 1:
        sys.exit(f"the games differ in different fields {sorted(map(str, keys))}: pass --key")
    key = keys.pop()
    var = lambda g, p: g["fields"][p].get(key, "?") if key else "all"
    score, seats, close = collections.Counter(), collections.Counter(), 0
    epochs = collections.defaultdict(lambda: collections.defaultdict(list))   # variant -> epoch -> [minutes]
    players = collections.Counter()
    marks = collections.defaultdict(lambda: collections.defaultdict(list))    # variant -> minute -> [(c, a)]
    lost = collections.defaultdict(list)
    for g in games:
        r = g["report"]
        final = {str(p["index"]): p for p in r.get("players", []) if not p.get("human")}
        ais = sorted(p for p in g["fields"] if p in final)
        for p in ais:
            v = var(g, p)
            players[v] += 1
            for e, ms in enumerate(r.get("epoch_ms", {}).get(p, [])):
                if ms >= 0:
                    epochs[v][e].append(ms / 60000)
            for m in MARKS:
                x = at_minute(r.get("samples", []), p, m)
                if x is not None:
                    marks[v][m].append(x)
            lost[v].append(g["losses"].get(p, (0, 0)))
        if key and len(ais) == 2 and var(g, ais[0]) != var(g, ais[1]):
            pa, pb = ais
            ca, aa, cb, ab = final[pa]["citizens"], final[pa]["army"], final[pb]["citizens"], final[pb]["army"]
            sa, sb = ca + aa, cb + ab
            if a.score == "mixed" and "strength" in final[pa]:
                m = mixed_share(final[pa], final[pb])
                w = pa if (cb == 0 and ca > 0) or (m >= MIXED_WIN and not (ca == 0 and cb > 0)) else                     pb if (ca == 0 and cb > 0) or m <= 1 - MIXED_WIN else None
            elif (cb == 0 and ca > 0) or sa >= 1.5 * max(sb, 1):
                w = pa
            elif (ca == 0 and cb > 0) or sb >= 1.5 * max(sa, 1):
                w = pb
            else:
                w = None
            if w is None:
                close += 1
            else:
                score[var(g, w)] += 1
                seats["p" + w] += 1
    out = dict(key=key, games=len(games), wins=dict(score), close=close, seats=dict(seats), variants={})
    print(f"{len(games)} games; variants by {key or '-'}")
    if score or close:
        print("wins: " + ", ".join(f"{v} {n}" for v, n in sorted(score.items())) + f", close {close}; seats "
              + ", ".join(f"{s} {n}" for s, n in sorted(seats.items())))
    top = max((e for v in epochs for e in epochs[v]), default=0)
    for v in sorted(players):
        n = players[v]
        row = dict(players=n, epochs={}, marks={}, lost={})
        print(f"\n{key or 'variant'}={v}: {n} players")
        cells = []
        for e in range(1, top + 1):
            t = epochs[v].get(e, [])
            row["epochs"][e] = dict(reached=len(t), median_min=round(statistics.median(t), 1) if t else None,
                                    mean_min=round(statistics.mean(t), 1) if t else None)
            cells.append(f"e{e} {len(t)}/{n}" + (f" at {statistics.median(t):.1f}m" if t else ""))
        print("  epochs (reached, median minute): " + ("  ".join(cells) if cells else "none past the start"))
        cells = []
        for m in MARKS:
            x = marks[v].get(m, [])
            if x:
                c, ar = statistics.mean(c for c, _ in x), statistics.mean(a for _, a in x)
                row["marks"][m] = dict(citizens=round(c, 1), army=round(ar, 1))
                cells.append(f"{m}m {c:.1f}/{ar:.1f}")
        print("  citizens/army: " + "  ".join(cells))
        e20, tot = statistics.mean(x for x, _ in lost[v]), statistics.mean(y for _, y in lost[v])
        row["lost"] = dict(by_20_min=round(e20, 1), total=round(tot, 1))
        print(f"  citizens lost: {e20:.1f} by 20 min, {tot:.1f} in all")
        out["variants"][v] = row
    if a.json:
        a.json.write_text(json.dumps(out, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
