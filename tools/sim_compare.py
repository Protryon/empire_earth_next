"""TEST ONLY: the same simulations on two machines compared run by run (the cross-platform determinism check).

  python -I tools/sim_compare.py BENCH_A BENCH_B [--until MS]

BENCH_A and BENCH_B are tools/sim_bench.py output folders (build/sim/bench/<name>) of the same command on two
machines, e.g. Windows (EE-SIM.exe) and Linux (ee-sim), with the same data (--data: the same release in both) and the
same scripts (--ai-scripts ai at the same commit). For every run id in both: the EE-AI traces
(tools/harness/ai_trace.py compare: the decisions, the hash lines with the unit count and the random seed) and the
reports (outcome, game time, the final players, every sample) without their wall-clock fields. Exit 0 when every
common run is identical; else the first difference of each run that differs.
"""
import argparse
import io
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "harness"))
import ai_trace  # noqa: E402

WALL = {"wall", "wall_seconds", "wall_seconds_game"}


def strip(value):
    """the report without its wall-clock fields"""
    if isinstance(value, dict):
        return {k: strip(v) for k, v in value.items() if k not in WALL}
    if isinstance(value, list):
        return [strip(v) for v in value]
    return value


def first_diff(a, b, path="report"):
    if type(a) is not type(b):
        return f"{path}: {a!r} != {b!r}"
    if isinstance(a, dict):
        for k in sorted(set(a) | set(b)):
            if k not in a or k not in b:
                return f"{path}.{k}: only in {'B' if k not in a else 'A'}"
            d = first_diff(a[k], b[k], f"{path}.{k}")
            if d:
                return d
        return None
    if isinstance(a, list):
        for k, (x, y) in enumerate(zip(a, b)):
            d = first_diff(x, y, f"{path}[{k}]")
            if d:
                return d
        return f"{path}: {len(a)} != {len(b)} entries" if len(a) != len(b) else None
    return None if a == b else f"{path}: {a!r} != {b!r}"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", type=pathlib.Path)
    ap.add_argument("b", type=pathlib.Path)
    ap.add_argument("--until", type=int, help="compare the traces up to this game ms")
    a = ap.parse_args()
    runs = lambda d: {p.stem for p in (d / "runs").glob("*.json")}
    common = sorted(runs(a.a) & runs(a.b))
    if not common:
        sys.exit(f"no run id in both {a.a / 'runs'} and {a.b / 'runs'}")
    differ = 0
    for rid in common:
        ra = json.loads((a.a / "runs" / f"{rid}.json").read_text())
        rb = json.loads((a.b / "runs" / f"{rid}.json").read_text())
        notes = []
        ta, tb = a.a / "runs" / f"{rid}.trace.txt", a.b / "runs" / f"{rid}.trace.txt"
        if ta.exists() and tb.exists():
            buf = io.StringIO()
            if not ai_trace.compare(ta, tb, out=buf, until=a.until):
                notes.append("trace: " + buf.getvalue().strip().splitlines()[-1])
        else:
            notes.append("trace: missing on one side")
        if a.until is None:
            d = first_diff(strip(ra), strip(rb))
            if d:
                notes.append(d)
        differ += bool(notes)
        print(f"{rid}: " + ("identical" if not notes else "DIFFERENT: " + "; ".join(notes)), flush=True)
    only = (runs(a.a) ^ runs(a.b))
    print(f"{len(common)} runs compared, {differ} differ" + (f"; {len(only)} in one bench only" if only else ""))
    return 1 if differ else 0


if __name__ == "__main__":
    sys.exit(main())
