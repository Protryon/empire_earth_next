"""TEST ONLY: the fights of tools/sim_bench.py runs (docs/AI_RESEARCH.md §8.86): kills against losses by unit class, and
the fights themselves - who won them at what odds and where.

  python -I tools/fight_stats.py build/sim/bench/NAME [more bench dirs] [--key engine] [--json OUT]

Read from each run's trace (EE-AI lines): every unit a computer player loses - the `cp unit.death` line (soldiers,
ships, a C++ seat's citizens: the dead unit's class, tile, distance from its town centre, the killer's
player:class:kind) and the `next next.death` line (a script seat's citizens and boats) - and from its report the
army samples (every 30 game s: the army count and its strength, hit points x attack).

- Classes: the class name's family: citizen, boat (fishing), inf (foot melee and thrown), arch (archers, guns on
  foot), cav, siege (gun_, siege), ship, tower (killer only), other. Path points and deaths with no killer of
  another computer player (animals, deletions) are left out.
- Kills against losses: per variant (the --key field of the trace's `cp start` line: engine, exp, personality), the
  enemy units its classes killed and its own units lost, by class.
- Fights: the deaths between two players joined while each falls within FIGHT_GAP_MS of the fight's last death and
  FIGHT_R tiles of its middle; FIGHT_MIN deaths at least. Each fight: the two sides' losses (the count; citizens apart),
  the odds at its start (the strength samples at or before its first death: a / (a + b)), the place (the home of the
  side whose town centre lies within HOME_R tiles of the fight's middle, else the field; each town centre found from
  its side's dead, their tiles and their distances to it). A side wins a fight when the
  other lost at least 1.5 times as many units (citizens counted), else it is even.
"""
import argparse
import collections
import gzip
import json
import pathlib
import re
import sys

FIGHT_GAP_MS, FIGHT_R, FIGHT_MIN, HOME_R = 30000, 16, 3, 25
ODDS_BINS = ((0.0, 0.4, "behind (<40%)"), (0.4, 0.6, "even (40-60%)"), (0.6, 1.01, "ahead (>60%)"))

UNIT_DEATH = re.compile(r"EE-AI (\d+) p(\d+) cp unit\.death unit=(\S+) x=(-?\d+) y=(-?\d+) d=(-?\d+) .*?"
                        r"killer=(-?\d+):(\S+?):(\S)")
NEXT_DEATH = re.compile(r"EE-AI (\d+) p(\d+) next next\.death unit=(\S+) x=(-?\d+) y=(-?\d+) d=(-?\d+) .*?"
                        r"killer=(-?\d+):(\S+?):(\S)")
START = re.compile(r"EE-AI \d+ p(\d+) cp start (.*?) \|")


def family(name, kind=None):
    if kind == "t" or name.startswith("b_"):
        return "tower" if "tower" in name or kind == "t" else "building"
    if name in ("citizen", "female_citizen") or kind == "c":
        return "citizen"
    if name in ("boat", "fishing_boat") or "fishing" in name:
        return "boat"
    if name.startswith("gun_") or name.startswith("siege"):
        return "siege"
    if name.startswith("inf"):
        return "inf"
    if name.startswith("arch"):
        return "arch"
    if name.startswith("cav"):
        return "cav"
    if re.match(r"s\d\d_", name) or kind == "n":
        return "ship"
    return "other"


def read_trace(path):
    opener = gzip.open if path.suffix == ".gz" else open
    starts, deaths = {}, []
    with opener(path, "rt", errors="replace") as f:
        for line in f:
            if "unit.death" in line or "next.death" in line:
                m = UNIT_DEATH.search(line) or NEXT_DEATH.search(line)
                if not m:
                    continue
                t, p, unit, x, y, d, kp, kc, kk = m.groups()
                if unit == "pathpoint" or kp in ("-1", "0") or kp == p or kk == "a":
                    continue
                deaths.append(dict(t=int(t), p=p, cls=family(unit), x=int(x), y=int(y), d=int(d), kp=kp,
                                   kcls=family(kc, kk)))
            elif " cp start " in line:
                m = START.search(line)
                if m:
                    starts[m.group(1)] = dict(kv.split("=", 1) for kv in m.group(2).split() if "=" in kv)
    deaths.sort(key=lambda e: e["t"])
    return starts, deaths


def strength_at(samples, p, t):
    best = None
    for s in samples:
        if s["game_ms"] > t:
            break
        best = s
    if best is None:
        return None
    for q in best["players"]:
        if str(q["index"]) == p:
            return q.get("strength", q.get("army", 0)), q.get("army", 0)
    return None


def homes(deaths):
    """each player's town centre, from its dead units' tiles and distances to it (a grid search: the point whose
    distances to the dead best match theirs)"""
    out = {}
    by = collections.defaultdict(list)
    for e in deaths:
        if e["d"] >= 0:
            by[e["p"]].append(e)
    for p, es in by.items():
        es = es[:: max(1, len(es) // 150)]
        xs, ys = [e["x"] for e in es], [e["y"] for e in es]

        def err(cx, cy):
            return sum((((e["x"] - cx) ** 2 + (e["y"] - cy) ** 2) ** 0.5 - e["d"]) ** 2 for e in es)
        best = None
        for cx in range(min(xs) - 60, max(xs) + 61, 4):
            for cy in range(min(ys) - 60, max(ys) + 61, 4):
                v = err(cx, cy)
                if best is None or v < best[0]:
                    best = (v, cx, cy)
        _, bx, by_ = best
        for cx in range(bx - 4, bx + 5):
            for cy in range(by_ - 4, by_ + 5):
                v = err(cx, cy)
                if v < best[0]:
                    best = (v, cx, cy)
        out[p] = (best[1], best[2])
    return out


def fights_of(deaths):
    open_, done = [], []
    for e in deaths:
        pair = tuple(sorted((e["p"], e["kp"])))
        hit = None
        for f in open_:
            if f["pair"] == pair and e["t"] - f["last"] <= FIGHT_GAP_MS:
                cx, cy = f["sx"] / f["n"], f["sy"] / f["n"]
                if (e["x"] - cx) ** 2 + (e["y"] - cy) ** 2 <= FIGHT_R * FIGHT_R:
                    hit = f
                    break
        if hit is None:
            hit = dict(pair=pair, t0=e["t"], last=e["t"], sx=0, sy=0, n=0, dead=[])
            open_.append(hit)
        hit["last"], hit["sx"], hit["sy"], hit["n"] = e["t"], hit["sx"] + e["x"], hit["sy"] + e["y"], hit["n"] + 1
        hit["dead"].append(e)
        still = []
        for f in open_:
            (still if e["t"] - f["last"] <= FIGHT_GAP_MS else done).append(f)
        open_ = still
    done.extend(open_)
    return [f for f in done if f["n"] >= FIGHT_MIN]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("benches", nargs="+", type=pathlib.Path)
    ap.add_argument("--key", default=None, help="the cp start field telling the variants apart (default: the first of "
                    "engine, exp, personality that differs in a game)")
    ap.add_argument("--json", type=pathlib.Path)
    a = ap.parse_args()
    kills = collections.defaultdict(collections.Counter)    # variant -> killer class -> enemy units killed
    losses = collections.defaultdict(collections.Counter)   # variant -> own class -> lost
    fstat = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0, 0, 0]))   # var -> bin -> w,l,e,own,foe
    place = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0, 0, 0]))
    both = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0, 0, 0]))
    games = 0
    for b in a.benches:
        runs = b / "runs" if (b / "runs").is_dir() else b
        for rep in sorted(runs.glob("*.json")):
            trace = next((p for p in (rep.with_suffix(".trace.txt"), rep.with_suffix(".trace.txt.gz")) if p.exists()), None)
            if trace is None:
                continue
            r = json.loads(rep.read_text())
            starts, deaths = read_trace(trace)
            if len(starts) < 2:
                continue
            key = a.key or next((k for k in ("engine", "exp", "personality") if len({s.get(k) for s in starts.values()}) > 1),
                                "engine")
            var = {p: s.get(key, "?") for p, s in starts.items()}
            if len(set(var.values())) < len(var):   # a mirror: the seat tells them apart
                var = {p: f"{v}@p{p}" for p, v in var.items()}
            games += 1
            for e in deaths:
                if e["p"] in var:
                    losses[var[e["p"]]][e["cls"]] += 1
                if e["kp"] in var:
                    kills[var[e["kp"]]][e["kcls"]] += 1
            samples = r.get("samples", [])
            town = homes(deaths)
            for f in fights_of(deaths):
                pa, pb = f["pair"]
                if pa not in var or pb not in var:
                    continue
                la = sum(1 for e in f["dead"] if e["p"] == pa)
                lb = sum(1 for e in f["dead"] if e["p"] == pb)
                sa, sb = strength_at(samples, pa, f["t0"]), strength_at(samples, pb, f["t0"])
                if sa is None or sb is None or sa[0] + sb[0] <= 0:
                    continue
                odds = sa[0] / (sa[0] + sb[0])
                cx, cy = f["sx"] / f["n"], f["sy"] / f["n"]
                near = {q: ((cx - h[0]) ** 2 + (cy - h[1]) ** 2) ** 0.5 for q, h in town.items() if q in (pa, pb)}
                home = min(near, key=near.get) if near and min(near.values()) <= HOME_R else None
                for me, foe, own, lost_foe, o in ((pa, pb, la, lb, odds), (pb, pa, lb, la, 1 - odds)):
                    label = next(n for lo, hi, n in ODDS_BINS if lo <= o < hi)
                    res = 0 if lost_foe >= 1.5 * max(own, 1) and lost_foe > own else 1 if own >= 1.5 * max(lost_foe, 1) and own > lost_foe else 2
                    where = "at home" if home == me else "at the enemy's" if home == foe else "field"
                    for table, k in ((fstat, label), (place, where), (both, where + ", " + label.split(" ")[0])):
                        row = table[var[me]][k]
                        row[res] += 1
                        row[3] += own
                        row[4] += lost_foe
    out = dict(games=games, kills={k: dict(v) for k, v in kills.items()}, losses={k: dict(v) for k, v in losses.items()},
               fights={k: {b: v for b, v in d.items()} for k, d in fstat.items()},
               places={k: {b: v for b, v in d.items()} for k, d in place.items()},
               by_place_odds={k: {b: v for b, v in d.items()} for k, d in both.items()})
    print(f"{games} games")
    classes = ["citizen", "boat", "inf", "arch", "cav", "siege", "ship", "tower", "other"]
    for v in sorted(set(kills) | set(losses)):
        print(f"\n{v}")
        print("  class    " + " ".join(f"{c:>7}" for c in classes) + "    total")
        k, l = kills[v], losses[v]
        print("  killed   " + " ".join(f"{k.get(c, 0):>7}" for c in classes) + f" {sum(k.values()):>8}")
        print("  lost     " + " ".join(f"{l.get(c, 0):>7}" for c in classes) + f" {sum(l.values()):>8}")
        for title, table in (("odds", fstat[v]), ("place", place[v]), ("both", both[v])):
            for b in sorted(table):
                w, lo, ev, own, foe = table[b]
                n = w + lo + ev
                print(f"  {title} {b:<24} fights {n:>4}  won-lost-even {w}-{lo}-{ev}  lost {own} killed {foe}"
                      f"  ({foe / max(own, 1):.2f} a loss)")
    if a.json:
        a.json.write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    sys.exit(main())
