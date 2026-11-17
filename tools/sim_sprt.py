"""TEST ONLY: the sequential test of tools/sim_bench.py --sprt (a variant against a baseline on paired seeds), and its
replay over finished benches.

  python -I tools/sim_sprt.py replay BASE VARIANT [BASE VARIANT ...] [--player 2] [--gains 0,0.1] [--max 64]
  python -I tools/sim_sprt.py oc BASE VARIANT [...] [--gains 0,0.1] [--max 64] [--trials 20000]

A bench is a finished tools/sim_bench.py folder (build/sim/bench/<name>), or several joined by + (seeds 2-33 and
34-65). Its first repeats are paired with the other's by seed and matchup (size, map, difficulty, AIs, cheats,
personality, engine) and taken in seed order.

**The model.** A game scores the subject player (`--player`, default 2: the first computer seat) 1 for a win, 0.5
for a close game, 0 for a loss, by tools/ai_scorecard.py's rule against its strongest computer opponent (citizens +
army at the end at least 1.5 times the opponent's, or the opponent without citizens). A pair is the baseline's game
and the variant's game of one seed and matchup (the same map, start and opponent); its outcome is d = variant -
baseline, one of -1, -0.5, 0, 0.5, 1 (a pentanomial); the pairs are independent. The test is the generalized SPRT
(GSPRT, as chess engine testing uses it): H0 "the mean gain E[d] is G0" against H1 "it is G1" (--gains G0,G1,
default 0,0.1: about +6 points in 64 games). The LLR is that of a normal model of d, n (G1 - G0) (2 mean - G0 - G1)
/ (2 var), its variance the pairs' plus PRIOR_PAIRS pseudo-pairs of PRIOR_VAR (--prior-pairs, --prior-var), so that
a run of identical games (d = 0, common: a change that does not fire) is not read as a variance of 0. It stops at
the first seed where the LLR leaves (log(beta / (1 - alpha)), log((1 - beta) / alpha)): above, H1 (better by about
G1), below, H0 (not better: equal or worse). At --max pairs (default 64) without a decision it is inconclusive, and
the fixed-size result (W-L-C, the mean gain and its standard error) stands as before. --gains=-0.1,0 asks the other
question: H1 "no worse" against H0 "worse by 0.1" (a non-regression test).

--model exact takes the LLR of the two maximum-likelihood pentanomials whose means are G0 and G1 instead (five free
probabilities). It is too weak for 64 pairs: an outcome not seen yet (d = +1) can carry either mean at no cost, so
the LLR moves by log((1 - q1) / (1 - q0)) a pair whatever was seen, about -0.1 (32 identical pairs and 32 pairs
that lost every way gave the same LLR).

**--stat ahead** (optional): d is which build was ahead at the cap, +1, 0 or -1 (the subject's weighted share of
economy, military and tech against its strongest opponent, ai_scorecard.py --score mixed, differing by more than
AHEAD_TIE between the two games). It counts every pair whose games differ, not only those the 1.5x rule calls; it
measures "further ahead at the cap", not winning. Default gains 0,0.2: ahead in 60% of the pairs that differ against
40%. (The normal model on the share's difference itself has heavy tails: on recorded benches it accepted H1 after
13-16 pairs for a variant whose 32-pair mean was negative; the sign is robust.)

**Error rates.** Wald's bounds keep alpha (H1 accepted when the gain is G0) and beta (H0 accepted when it is G1) at
about their nominal values; the truncation at --max lowers both and turns the missing power into inconclusive runs.
`oc` measures them on the benches' own outcome distribution: it resamples pairs from their pentanomial tilted to a
mean gain of G0 - (G1 - G0), G0, the middle, G1 and G1 + (G1 - G0) and runs the truncated test --trials times each.
On the 959 pairs of the recorded benches of docs/AI_RESEARCH.md §8.86-8.94 (W-L-C, defaults): gain -0.1: H0 always,
after 22 pairs on average; 0: H1 0.3%, H0 94%, inconclusive 6%, 31 pairs; +0.05: H1 17%, H0 38%; +0.1: H1 58%, H0
9%, inconclusive 33%, 48 pairs; +0.2: H1 97%, 35 pairs. With --stat ahead: 0: H1 4.5%; +0.2: H0 3.8%.
"""
import argparse
import json
import math
import pathlib
import random
import statistics
import sys

OUTCOMES = (-1.0, -0.5, 0.0, 0.5, 1.0)
MATCHUP = ("size", "map", "difficulty", "ai", "cheats", "personality", "engine")
STATS = ("wlc", "ahead")
DEFAULT_GAINS = {"wlc": (0.0, 0.1), "ahead": (0.0, 0.2)}
AHEAD_TIE = 0.01   # shares closer than this: neither build ahead
PRIOR_VAR = {"wlc": 0.2, "ahead": 0.5}   # the pseudo-pairs' variance of d
PRIOR_PAIRS = 4
EPS = 1e-3   # the pseudo-count of every outcome (an outcome not seen yet must not make a mean infeasible)
MIXED_WEIGHTS = (0.4, 0.4, 0.2)   # tools/ai_scorecard.py's


# ------------------------------------------------------------------------------------------------ a game's score

def _final(report):
    return {p.get("index"): p for p in report.get("players") or [] if not p.get("human")}


def opponent(report, player):
    """the subject's strongest computer opponent's final row (citizens + army), or None"""
    final = _final(report)
    others = [p for i, p in final.items() if i != player]
    if player not in final or not others:
        return None
    return max(others, key=lambda p: (p.get("citizens", 0) + p.get("army", 0), -p.get("index", 0)))


def score(report, player):
    """1 won, 0.5 close, 0 lost (ai_scorecard.py's bodies rule against the strongest opponent); None: no result"""
    if not report or report.get("outcome") in (None, "no-report", "error", "wall-timeout"):
        return None
    me, other = _final(report).get(player), opponent(report, player)
    if me is None or other is None:
        return None
    ca, cb = me.get("citizens", 0), other.get("citizens", 0)
    sa, sb = ca + me.get("army", 0), cb + other.get("army", 0)
    if (cb == 0 and ca > 0) or sa >= 1.5 * max(sb, 1):
        return 1.0
    if (ca == 0 and cb > 0) or sb >= 1.5 * max(sa, 1):
        return 0.0
    return 0.5


def share(report, player):
    """the subject's weighted share of economy, military and tech against its strongest opponent (0..1), or None"""
    if score(report, player) is None:
        return None
    a, b = _final(report)[player], opponent(report, player)
    part = lambda x, y: 0.5 if x + y <= 0 else x / (x + y)
    eco = lambda p: p.get("value", 0) + sum((p.get("resources") or {}).values())
    if "strength" not in a:   # reports from before the strength samples: the body count
        return part(a.get("citizens", 0) + a.get("army", 0), b.get("citizens", 0) + b.get("army", 0))
    parts = (part(eco(a), eco(b)), part(a.get("strength", 0), b.get("strength", 0)),
             part(a.get("epoch", 0) + 1, b.get("epoch", 0) + 1))
    return sum(w * s for w, s in zip(MIXED_WEIGHTS, parts))


def pair_outcome(stat, base_report, var_report, player):
    """-> (d, base score, variant score) of a pair, or None when either game has no result"""
    x, y = score(base_report, player), score(var_report, player)
    if x is None or y is None:
        return None
    if stat == "wlc":
        return y - x, x, y
    sx, sy = share(base_report, player), share(var_report, player)
    return (0.0 if abs(sy - sx) <= AHEAD_TIE else 1.0 if sy > sx else -1.0), x, y


# ------------------------------------------------------------------------------------------------ the test

def bounds(alpha=0.05, beta=0.05):
    """Wald's bounds (lower: accept H0, upper: accept H1)"""
    return math.log(beta / (1 - alpha)), math.log((1 - beta) / alpha)


def tilt(freq, mu):
    """the maximum-likelihood distribution over OUTCOMES with mean mu, given frequencies summing to 1:
    p_i = f_i / (1 + lam (x_i - mu)), lam the root of sum f_i (x_i - mu) / (1 + lam (x_i - mu)) = 0"""
    xs = [x - mu for x in OUTCOMES]
    hi_x, lo_x = max(x for x, f in zip(xs, freq) if f > 0), min(x for x, f in zip(xs, freq) if f > 0)
    if hi_x <= 0 or lo_x >= 0:
        raise ValueError(f"mean {mu} outside the outcomes")
    lo, hi = -1 / hi_x, -1 / lo_x   # 1 + lam x > 0 for every x
    g = lambda lam: sum(f * x / (1 + lam * x) for f, x in zip(freq, xs))
    for _ in range(200):
        mid = (lo + hi) / 2
        if g(mid) > 0:
            lo = mid
        else:
            hi = mid
    lam = (lo + hi) / 2
    p = [f / (1 + lam * x) for f, x in zip(freq, xs)]
    s = sum(p)
    return [v / s for v in p]


def counts(ds):
    c = [0] * len(OUTCOMES)
    for d in ds:
        c[OUTCOMES.index(d)] += 1
    return c


def llr(c, mu0, mu1):
    """the GSPRT log-likelihood ratio of H1 (mean mu1) against H0 (mean mu0) for outcome counts c"""
    if sum(c) == 0:
        return 0.0
    reg = [x + EPS for x in c]
    freq = [x / sum(reg) for x in reg]
    p0, p1 = tilt(freq, mu0), tilt(freq, mu1)
    return sum(x * math.log(b / a) for x, a, b in zip(c, p0, p1))


def llr_normal(ds, mu0, mu1, prior_var, prior_pairs):
    """the GSPRT log-likelihood ratio of H1 (mean mu1) against H0 (mean mu0) on a normal model of d, its variance
    the pairs' with prior_pairs pseudo-pairs of variance prior_var (so that a run of identical games is not taken
    for a variance of 0)"""
    n = len(ds)
    if n == 0:
        return 0.0
    m = sum(ds) / n
    var = (sum((d - m) ** 2 for d in ds) + prior_pairs * prior_var) / (n + prior_pairs)
    return n * (mu1 - mu0) * (2 * m - mu0 - mu1) / (2 * var)


class Test:
    """the sequential test's settings, and its verdict on the paired outcomes so far"""

    def __init__(self, stat="wlc", gains=None, alpha=0.05, beta=0.05, max_pairs=64, min_pairs=8, model="normal",
                 prior_var=None, prior_pairs=PRIOR_PAIRS):
        self.model, self.prior_pairs = model, prior_pairs
        self.prior_var = prior_var if prior_var is not None else PRIOR_VAR[stat]
        if stat not in STATS:
            raise ValueError(f"stat {stat!r}: one of {STATS}")
        self.stat = stat
        self.g0, self.g1 = gains if gains is not None else DEFAULT_GAINS[stat]
        if not -1 < self.g0 < self.g1 < 1:
            raise ValueError(f"the gains {self.g0}, {self.g1}: -1 < H0's < H1's < 1")
        self.alpha, self.beta, self.max_pairs, self.min_pairs = alpha, beta, max_pairs, min_pairs
        self.lower, self.upper = bounds(alpha, beta)

    def llr(self, ds):
        if self.model == "exact":
            return llr(counts(ds), self.g0, self.g1)
        return llr_normal(ds, self.g0, self.g1, self.prior_var, self.prior_pairs)

    def verdict(self, ds):
        """-> (decision: 'H1' better, 'H0' not better, 'max' inconclusive at the maximum, None: go on; the LLR)"""
        value = self.llr(ds)
        if len(ds) >= self.min_pairs:
            if value >= self.upper:
                return "H1", value
            if value <= self.lower:
                return "H0", value
        if len(ds) >= self.max_pairs:
            return "max", value
        return None, value

    def describe(self):
        what = "W 1, close 0.5, L 0" if self.stat == "wlc" else "ahead at the cap +1, level 0, behind -1"
        return (f"SPRT {self.stat} ({what}): H0 gain {self.g0:g}, H1 gain {self.g1:g} a pair; alpha {self.alpha:g}, "
                f"beta {self.beta:g}: LLR bounds ({self.lower:.2f}, {self.upper:.2f}); {self.min_pairs}..{self.max_pairs} pairs")

    @staticmethod
    def word(decision):
        return {"H1": "H1 (better)", "H0": "H0 (not better)", "max": "inconclusive at the maximum",
                None: "undecided"}[decision]


def summary(pairs):
    """W-L-C of each side, the paired mean gain (W-L-C scores) with its standard error; pairs: [(d, x, y)]"""
    wlc = lambda xs: f"{sum(v == 1 for v in xs)}-{sum(v == 0 for v in xs)}-{sum(v == 0.5 for v in xs)}"
    xs, ys = [p[1] for p in pairs], [p[2] for p in pairs]
    ds = [y - x for x, y in zip(xs, ys)]
    m = statistics.mean(ds) if ds else 0.0
    se = statistics.stdev(ds) / math.sqrt(len(ds)) if len(ds) > 1 else float("nan")
    return f"base {wlc(xs)}, variant {wlc(ys)}, gain {m:+.3f} +- {se:.3f} a game ({len(ds)} pairs)"


# ------------------------------------------------------------------------------------------------ recorded benches

def bench_runs(d):
    """{(seed, matchup): report} of a finished bench's first repeats; several folders joined by +"""
    out = {}
    for part in str(d).split("+"):
        res = pathlib.Path(part) / "results.json"
        if not res.exists():
            sys.exit(f"{part}: no results.json")
        for r in json.loads(res.read_text()).get("runs", []):
            if r.get("repeat", 0) == 0:
                out[(r["seed"], tuple(str(r.get(k)) for k in MATCHUP))] = r.get("report") or {}
    return out


def paired(base_dir, var_dir, player, stat):
    """[(d, base score, variant score)] in seed order over the (seed, matchup) cells both benches have"""
    a, b = bench_runs(base_dir), bench_runs(var_dir)
    out = []
    for key in sorted(set(a) & set(b)):
        p = pair_outcome(stat, a[key], b[key], player)
        if p is not None:
            out.append(p)
    return out


def run_sequential(pairs, test):
    """the test over the pairs in order -> (decision, pairs used, llr)"""
    ds, value = [], 0.0
    for p in pairs:
        ds.append(p[0])
        decision, value = test.verdict(ds)
        if decision:
            return decision, len(ds), value
    return None, len(ds), value


def oc(ds, test, trials, rng):
    """the operating characteristic on the benches' own outcome distribution, tilted to five mean gains"""
    c = counts(ds)
    reg = [x + 0.5 for x in c]   # a softer prior: the resampled distribution keeps every outcome possible
    freq = [x / sum(reg) for x in reg]
    rows = []
    step = test.g1 - test.g0
    for mu in (test.g0 - step, test.g0, (test.g0 + test.g1) / 2, test.g1, test.g1 + step):
        if not -1 < mu < 1:
            continue
        p = tilt(freq, mu)
        tally = dict(H1=0, H0=0, max=0)
        used = 0
        for _ in range(trials):
            sample = []
            while True:
                sample.append(rng.choices(OUTCOMES, p)[0])
                decision, _ = test.verdict(sample)
                if decision:
                    tally[decision] += 1
                    used += len(sample)
                    break
        rows.append(dict(gain=round(mu, 4), H1=tally["H1"] / trials, H0=tally["H0"] / trials,
                         inconclusive=tally["max"] / trials, mean_pairs=used / trials, dist=[round(v, 3) for v in p]))
    return c, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=("replay", "oc"))
    ap.add_argument("benches", nargs="+", help="BASE VARIANT [BASE VARIANT ...] (DIR+DIR joins folders)")
    ap.add_argument("--player", type=int, default=2)
    ap.add_argument("--stat", choices=STATS, default="wlc")
    ap.add_argument("--gains", help="G0,G1: the mean gain a pair under H0 and H1 (default 0,0.1; ahead 0,0.2); "
                                    "--gains=-0.1,0 tests for no regression")
    ap.add_argument("--alpha", type=float, default=0.05)
    ap.add_argument("--beta", type=float, default=0.05)
    ap.add_argument("--max", type=int, default=64)
    ap.add_argument("--min", type=int, default=8)
    ap.add_argument("--trials", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--json", type=pathlib.Path)
    ap.add_argument("--model", choices=("normal", "exact"), default="normal")
    ap.add_argument("--prior-var", type=float)
    ap.add_argument("--prior-pairs", type=float, default=PRIOR_PAIRS)
    a = ap.parse_args()
    if len(a.benches) % 2:
        sys.exit("give the benches as BASE VARIANT pairs")
    test = Test(a.stat, tuple(float(x) for x in a.gains.split(",")) if a.gains else None, a.alpha, a.beta, a.max,
                a.min, a.model, a.prior_var, a.prior_pairs)
    print(test.describe())
    out, every = [], []
    for base, var in zip(a.benches[::2], a.benches[1::2]):
        pairs = paired(base, var, a.player, a.stat)
        every += [p[0] for p in pairs]
        if a.cmd != "replay":
            continue
        decision, n, value = run_sequential(pairs, test)
        name = lambda d: "+".join(pathlib.Path(x).name for x in d.split("+"))
        print(f"{name(base)} -> {name(var)}: {Test.word(decision)} after {n} of {len(pairs)} pairs, LLR {value:+.2f}"
              f"\n    at the stop: {summary(pairs[:n])}\n    all pairs:   {summary(pairs)}")
        out.append(dict(base=base, variant=var, decision=decision, pairs=n, of=len(pairs), llr=round(value, 3),
                        at_stop=summary(pairs[:n]), all=summary(pairs)))
    if a.cmd == "oc":
        c, rows = oc(every, test, a.trials, random.Random(a.seed))
        print(f"the benches' outcomes (d = -1, -0.5, 0, 0.5, 1): {c}, {sum(c)} pairs")
        for r in rows:
            print(f"true gain {r['gain']:+.3f}: H1 {r['H1']:.3f}, H0 {r['H0']:.3f}, inconclusive "
                  f"{r['inconclusive']:.3f}; mean pairs {r['mean_pairs']:.1f}")
        out = dict(test=test.describe(), counts=c, rows=rows)
    if a.json:
        a.json.write_text(json.dumps(out, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
