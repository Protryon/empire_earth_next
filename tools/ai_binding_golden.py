"""TEST ONLY: the AI bindings' golden (docs/progress/ai-lua-host.md): seeded EE-SIM games with the binding trace on.

  python -I tools/ai_binding_golden.py run NAME [--exe EXE] [--games a,b,...] [--jobs 2] [--data DIR]
                                                                              -> build/ai_golden/NAME/<game>.txt.gz
  python -I tools/ai_binding_golden.py compare BASE NAME [--games a,b,...] [--root DIR]
  python -I tools/ai_binding_golden.py list

Each game runs with EE_AI_BINDING_TRACE=1 (ee_ai binding_trace.rs): every `ai.*` binding call writes its arguments'
and its results' FNV-1a 64 (`EE-AIB > name a=...`, `EE-AIB < name n=... r=...`), every strategy step the hash of its
view and its orders (`EE-AIB step ...`). The golden keeps those lines and the EE-AI decision lines, in order, gzipped.
`compare` names the first line that differs per game, with the binding calls before it. A port of the bindings must
leave every game identical; rerun a game with EE_AI_BINDING_TRACE=full (`--full`) to see the values' text.

The games cover the three AIs of the manifest (classic_lua, ageofai, boom), later start epochs, a water map, and the
per-unit test scripts of tests/ai_units (EE_AI_NEXT_SCRIPT, EE_AI_TEST_SPAWN), which call the rarer bindings.
"""
import argparse
import concurrent.futures
import gzip
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "ai_golden"
STAGES = ROOT / "build" / "sim" / "stage"
TESTS = ROOT / "tests" / "ai_units"

# name: (seed, game seconds, EE-SIM options, environment, the test scripts' folder)
GAMES = {
    "classic-2": (2, 2700, [], {}, False),
    "classic-3": (3, 2700, [], {}, False),
    "ageofai-2": (2, 2700, ["--engine", "ageofai"], {}, False),
    "ageofai-3": (3, 2700, ["--engine", "ageofai"], {}, False),
    "boom-2": (2, 1800, ["--engine", "boom"], {}, False),
    "late-2": (2, 1500, ["--engine", "ageofai,classic,ageofai", "--cheats", "all", "--ai", "3", "--lobby", "183=#5"], {},
               False),
    "space-2": (2, 1500, ["--engine", "ageofai,classic,ageofai", "--cheats", "all", "--ai", "3", "--lobby", "183=#9"],
                {}, False),
    "islands-2": (2, 1800, ["--engine", "ageofai,classic", "--map-type", "Large Islands"], {}, False),
    "helpers-2": (2, 900, [], {"EE_AI_NEXT_SCRIPT": "helpers"}, True),
    "commands-2": (2, 400, [], {"EE_AI_NEXT_SCRIPT": "commands"}, True),
    "types-2": (2, 400, [], {"EE_AI_NEXT_SCRIPT": "types"}, True),
    "raid-2": (2, 400, [], {"EE_AI_NEXT_SCRIPT": "raid", "EE_AI_TEST_SPAWN": "1"}, True),
    "airwing-2": (2, 400, ["--engine", "next,classic"], {"EE_AI_NEXT_SCRIPT": "airwing", "EE_AI_TEST_SPAWN": "1"},
                  True),
    "kite-2": (2, 400, [], {"EE_AI_NEXT_SCRIPT": "kite", "EE_AI_TEST_SPAWN": "1"}, True),
}


def keep(line):
    return " EE-AIB " in line or " EE-AI " in line


def one(name, game, exe, data, full):
    seed, seconds, options, env, tests = GAMES[game]
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    trace = d / f"trace-{game}.txt"
    slot = f"aig-{name}-{game}"
    e = dict(os.environ)
    e.update(env)
    e["EE_AI_BINDING_TRACE"] = "full" if full else "1"
    cmd = [sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot", slot, "--exe", str(exe)]
    if tests:
        cmd += ["--ai-scripts", str(TESTS)]
    cmd += ["--", "--seed", str(seed), "--difficulty", "Hard", "--game-seconds", str(seconds), "--cheats", "none",
            *options, "--report", str(d / f"report-{game}.json"), "--ai-trace", "--trace", str(trace)]
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=e)
    (d / f"log-{game}.txt").write_text(r.stdout + r.stderr)
    n = 0
    if trace.exists():
        with open(trace, "r", errors="replace") as f, gzip.open(d / f"{game}.txt.gz", "wt", compresslevel=6) as g:
            for line in f:
                if keep(line):
                    g.write(line.rstrip("\r\n") + "\n")
                    n += 1
        trace.unlink()
    shutil.rmtree(STAGES / slot, ignore_errors=True)
    return game, r.returncode, n


def lines(p):
    with gzip.open(p, "rt", errors="replace") as f:
        for line in f:
            yield line.rstrip("\n")


def compare(base, name, games):
    bad = 0
    for game in games:
        a, b = base / f"{game}.txt.gz", name / f"{game}.txt.gz"
        if not a.exists() or not b.exists():
            print(f"{game}: missing ({a if not a.exists() else b})")
            bad += 1
            continue
        recent = []
        n = 0
        diff = None
        ia, ib = lines(a), lines(b)
        while True:
            la, lb = next(ia, None), next(ib, None)
            if la is None and lb is None:
                break
            n += 1
            if la != lb:
                diff = (la, lb)
                break
            recent.append(la)
            recent = recent[-6:]
        if diff is None:
            print(f"{game}: identical ({n - 1} lines)")
        else:
            bad += 1
            print(f"{game}: differs at line {n}")
            for r in recent:
                print(f"    {r}")
            print(f"  - {diff[0]}")
            print(f"  + {diff[1]}")
    print("ai_binding_golden:", "PASS" if bad == 0 else f"FAIL ({bad} games)")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    r = sub.add_parser("run")
    r.add_argument("name")
    r.add_argument("--exe", type=pathlib.Path, default=ROOT / "build" / "clang" / "exe" / "EE-SIM.exe")
    r.add_argument("--games", default=",".join(GAMES))
    r.add_argument("--jobs", type=int, default=2)
    r.add_argument("--data", default="C:/Users/max/Desktop/empire_earth_next/build/open_data/tree")
    r.add_argument("--full", action="store_true", help="EE_AI_BINDING_TRACE=full: the values' text too")
    c = sub.add_parser("compare")
    c.add_argument("base")
    c.add_argument("name")
    c.add_argument("--games", default=",".join(GAMES))
    c.add_argument("--root", type=pathlib.Path, default=ROOT)
    sub.add_parser("list")
    a = ap.parse_args()
    if a.command == "list":
        for k, v in GAMES.items():
            print(k, v)
        return 0
    games = [g for g in a.games.split(",") if g]
    if a.command == "compare":
        return compare(a.root / "build" / "ai_golden" / a.base, OUT / a.name, games)
    exe = a.exe.resolve()
    if not exe.exists():
        print(f"no {exe}", file=sys.stderr)
        return 2
    exe_copy = OUT / a.name / exe.name   # the run's own copy: a rebuild meanwhile does not change it
    exe_copy.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(exe, exe_copy)
    for p in exe.parent.glob(exe.stem + "*.dll"):
        shutil.copy2(p, exe_copy.parent / p.name)
    code = 0
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        for game, rc, n in pool.map(lambda g: one(a.name, g, exe_copy, a.data, a.full), games):
            print(f"{a.name} {game}: exit {rc}, {n} lines")
            code |= rc != 0
    return code


if __name__ == "__main__":
    sys.exit(main())
