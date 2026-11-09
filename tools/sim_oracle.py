"""TEST ONLY: the simulation oracle for the Rust port (docs/RUST_PLAN.md 2.1, 3): seeded EE-SIM games and their AI traces.

  python -I tools/sim_oracle.py run NAME [--exe EXE] [--seeds 2-7] [--jobs 3] [--data open]   -> build/rust_oracle/NAME/
  python -I tools/sim_oracle.py compare BASE NAME [--root DIR]

`run` plays a 2-AI Hard game of 2700 game seconds per seed (no cheats) and keeps its EE-AI trace. `compare` checks the
decision lines (EE-AI, not EE-AIPROF's timings) of two runs seed by seed and names the first difference. A port must
leave every seed identical; a deliberate change is a rebaseline in its own commit. --root reads BASE from another
checkout's build/rust_oracle (e.g. the main checkout's reference run `ref`).
"""
import argparse
import concurrent.futures
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "rust_oracle"


def seeds(text):
    out = []
    for part in text.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def one(name, exe, seed, data):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    trace, report = d / f"trace-{seed}.txt", d / f"report-{seed}.json"
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot",
                        f"oracle-{name}-{seed}", "--exe", str(exe), "--", "--seed", str(seed), "--difficulty", "Hard",
                        "--game-seconds", "2700", "--cheats", "none", "--report", str(report), "--ai-trace",
                        "--trace", str(trace)], cwd=ROOT, capture_output=True, text=True)
    (d / f"log-{seed}.txt").write_text(r.stdout + r.stderr)
    return seed, r.returncode, trace.stat().st_size if trace.exists() else -1


def decisions(p):
    return [l.rstrip("\r\n") for l in p.read_text(errors="replace").splitlines() if " EE-AI " in l]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("name")
    r.add_argument("--exe", default=str(ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"))
    r.add_argument("--seeds", default="2-7")
    r.add_argument("--jobs", type=int, default=3)
    r.add_argument("--data", default="open")
    c = sub.add_parser("compare")
    c.add_argument("base")
    c.add_argument("name")
    c.add_argument("--root", help="the checkout holding BASE (default: this one)")
    c.add_argument("--seeds", default="2-7")
    a = ap.parse_args()
    if a.cmd == "run":
        bad = 0
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            for seed, code, size in ex.map(lambda s: one(a.name, a.exe, s, a.data), seeds(a.seeds)):
                print(f"{a.name} seed {seed}: exit {code}, trace {size} bytes", flush=True)
                bad += code != 0 or size <= 0
        return 1 if bad else 0
    base_dir = (pathlib.Path(a.root) / "build" / "rust_oracle" if a.root else OUT) / a.base
    bad = 0
    for seed in seeds(a.seeds):
        la, lb = decisions(base_dir / f"trace-{seed}.txt"), decisions(OUT / a.name / f"trace-{seed}.txt")
        if not la:
            print(f"seed {seed}: EMPTY baseline")
            bad += 1
        elif la == lb:
            print(f"seed {seed}: identical ({len(la)} lines)")
        else:
            bad += 1
            i = next((i for i, (x, y) in enumerate(zip(la, lb)) if x != y), min(len(la), len(lb)))
            print(f"seed {seed}: DIFFERS at line {i} of {len(la)}/{len(lb)}\n  {la[i] if i < len(la) else ''}\n"
                  f"  {lb[i] if i < len(lb) else ''}")
    print("oracle:", "PASS" if not bad else f"FAIL ({bad})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
