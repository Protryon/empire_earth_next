"""TEST ONLY: the simulation oracle for the Rust port (docs/RUST_PLAN.md 2.1, 3): seeded EE-SIM games and their AI traces.

  python -I tools/sim_oracle.py run NAME [--exe EXE] [--seeds 2-7] [--jobs 3] [--data open] [--checksums]
                                       [--autosave-ms N]                              -> build/rust_oracle/NAME/
  python -I tools/sim_oracle.py compare BASE NAME [--root DIR]
  python -I tools/sim_oracle.py ticks BASE NAME [--root DIR]
  python -I tools/sim_oracle.py selftest

`run` plays a 2-AI Hard game of 2700 game seconds per seed (no cheats) and keeps its EE-AI trace. `compare` checks the
decision lines (EE-AI, not EE-AIPROF's timings) of two runs seed by seed and names the first difference. A port must
leave every seed identical; a deliberate change is a rebaseline in its own commit. --root reads BASE from another
checkout's build/rust_oracle (e.g. the main checkout's reference run `ref`).

--checksums also writes EE-SIM's per-communications-tick checksum lines (--checksum-trace, ct-SEED.txt: the tick, the
physics random seed, the out-of-synch checksum, units, AI; docs/RUST_PLAN.md R5a.1). Reading them changes nothing, so
the EE-AI trace of such a run equals one without. `ticks` compares two runs' checksum lines and names the first
divergent communications tick and the fields that differ there: it pins a divergence to one 350 ms tick where
`compare` pins it to a decision. --autosave-ms runs the game's autosave (EE-SIM's default is none): a save must not
change the game (R5a.2), so such a run must trace like one without. `selftest` checks the tick comparison on made-up
traces (no game).
"""
import argparse
import concurrent.futures
import io
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "rust_oracle"


def seeds(text):
    out = []
    for part in text.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def one(name, exe, seed, data, checksums=False, autosave_ms=0):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    trace, report = d / f"trace-{seed}.txt", d / f"report-{seed}.json"
    extra = ["--checksum-trace", str(d / f"ct-{seed}.txt")] if checksums else []
    if autosave_ms:
        extra += ["--autosave-ms", str(autosave_ms)]
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot",
                        f"oracle-{name}-{seed}", "--exe", str(exe), "--", "--seed", str(seed), "--difficulty", "Hard",
                        "--game-seconds", "2700", "--cheats", "none", "--report", str(report), "--ai-trace",
                        "--trace", str(trace), *extra], cwd=ROOT, capture_output=True, text=True)
    (d / f"log-{seed}.txt").write_text(r.stdout + r.stderr)
    return seed, r.returncode, trace.stat().st_size if trace.exists() else -1


def decisions(p):
    return [l.rstrip("\r\n") for l in p.read_text(errors="replace").splitlines() if " EE-AI " in l]


def tick_lines(p):
    """A checksum trace's lines (EESim.cpp ChecksumLine: "CT <n> key=value ...") -> [(ct, {key: value})]."""
    out = []
    for line in p.read_text(errors="replace").splitlines():
        parts = line.split()
        if len(parts) < 2 or parts[0] != "CT":
            continue
        out.append((int(parts[1]), dict(kv.split("=", 1) for kv in parts[2:] if "=" in kv)))
    return out


def first_tick_difference(la, lb):
    """Two checksum traces -> None when equal, else (index, the fields that differ, why). A run that stops earlier
    (a shorter trace whose lines all agree) differs at the first tick it lacks."""
    for i, ((ca, fa), (cb, fb)) in enumerate(zip(la, lb)):
        if ca != cb or fa != fb:
            keys = (["ct"] if ca != cb else []) + [k for k in dict.fromkeys([*fa, *fb]) if fa.get(k) != fb.get(k)]
            return i, keys, "differs"
    if len(la) != len(lb):
        return min(len(la), len(lb)), [], "ends"
    return None


def show_tick(line):
    return f"CT {line[0]} " + " ".join(f"{k}={v}" for k, v in line[1].items()) if line else "(none)"


def compare_ticks(base_dir, name_dir, seed_list, out=sys.stdout):
    """Seed by seed, the first divergent communications tick of two runs' checksum traces -> the failure count."""
    bad = 0
    for seed in seed_list:
        pa, pb = base_dir / f"ct-{seed}.txt", name_dir / f"ct-{seed}.txt"
        if not pa.exists() or not pb.exists():
            print(f"seed {seed}: MISSING {pa if not pa.exists() else pb} (run with --checksums)", file=out)
            bad += 1
            continue
        la, lb = tick_lines(pa), tick_lines(pb)
        if not la:
            print(f"seed {seed}: EMPTY baseline", file=out)
            bad += 1
            continue
        diff = first_tick_difference(la, lb)
        if diff is None:
            last = la[-1][1]
            print(f"seed {seed}: identical ({len(la)} ticks, to PT {last.get('pt')}, game ms {last.get('ms')})",
                  file=out)
            continue
        bad += 1
        i, keys, _ = diff
        a, b = (la[i] if i < len(la) else None), (lb[i] if i < len(lb) else None)
        ct, ms = (a or b)[0], (a or b)[1].get("ms")
        print(f"seed {seed}: FIRST DIVERGENT TICK CT {ct} (game ms {ms}, line {i} of {len(la)}/{len(lb)}): "
              + (", ".join(keys) if keys else "one run ends there"), file=out)
        if i > 0:
            print(f"  last equal {show_tick(la[i - 1])}", file=out)
        print(f"  {base_dir.name:>10} {show_tick(a)}\n  {name_dir.name:>10} {show_tick(b)}", file=out)
    print("ticks:", "PASS" if not bad else f"FAIL ({bad})", file=out)
    return bad


def selftest():
    """The tick comparison on made-up traces: equal, a field change, an early end, a missing file, the parse."""
    line = "CT {} pt={} tick={} ms={} rng=0000abcd oos={} units=10/00000001 ai=00000002 inputs=3\n"
    checks = []
    with tempfile.TemporaryDirectory() as tmp:
        a, b = pathlib.Path(tmp, "a"), pathlib.Path(tmp, "b")
        a.mkdir()
        b.mkdir()
        rows = [line.format(k, 14 * k, 14 * k, 350 * k, "00000000") for k in range(1, 6)]
        for seed in (1, 2, 3, 4):
            (a / f"ct-{seed}.txt").write_text("".join(rows))
        (b / "ct-1.txt").write_text("".join(rows))
        (b / "ct-2.txt").write_text("".join(rows[:3]) + rows[3].replace("oos=00000000", "oos=0000beef") + rows[4])
        (b / "ct-3.txt").write_text("".join(rows[:2]))

        def run(seed):
            out = io.StringIO()
            return compare_ticks(a, b, [seed], out), out.getvalue()

        bad, text = run(1)
        checks.append(("equal", bad == 0 and "identical (5 ticks, to PT 70, game ms 1750)" in text))
        bad, text = run(2)
        checks.append(("field", bad == 1 and "FIRST DIVERGENT TICK CT 4 (game ms 1400, line 3 of 5/5): oos" in text
                       and "last equal CT 3 " in text and "oos=0000beef" in text))
        bad, text = run(3)
        checks.append(("ends", bad == 1
                       and "FIRST DIVERGENT TICK CT 3 (game ms 1050, line 2 of 5/2): one run ends there" in text))
        bad, text = run(4)
        checks.append(("missing", bad == 1 and "MISSING" in text and "ticks: FAIL (1)" in text))
        checks.append(("parse", tick_lines(a / "ct-1.txt")[1] == (2, dict(
            pt="28", tick="28", ms="700", rng="0000abcd", oos="00000000", units="10/00000001", ai="00000002",
            inputs="3"))))
        checks.append(("ct", first_tick_difference([(1, {}), (2, {})], [(1, {}), (3, {})]) == (1, ["ct"], "differs")))
    for what, ok in checks:
        print(f"selftest {what}: {'ok' if ok else 'FAILED'}")
    return 0 if all(ok for _, ok in checks) else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("name")
    r.add_argument("--exe", default=str(ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"))
    r.add_argument("--seeds", default="2-7")
    r.add_argument("--jobs", type=int, default=3)
    r.add_argument("--data", default="open")
    r.add_argument("--checksums", action="store_true", help="also the per-CT checksum lines (ct-SEED.txt)")
    r.add_argument("--autosave-ms", type=int, default=0, help="the game's autosave period (default 0: none)")
    for cmd in ("compare", "ticks"):
        c = sub.add_parser(cmd)
        c.add_argument("base")
        c.add_argument("name")
        c.add_argument("--root", help="the checkout holding BASE (default: this one)")
        c.add_argument("--seeds", default="2-7")
    sub.add_parser("selftest")
    a = ap.parse_args()
    if a.cmd == "selftest":
        return selftest()
    if a.cmd == "run":
        bad = 0
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            for seed, code, size in ex.map(lambda s: one(a.name, a.exe, s, a.data, a.checksums, a.autosave_ms),
                                           seeds(a.seeds)):
                print(f"{a.name} seed {seed}: exit {code}, trace {size} bytes", flush=True)
                bad += code != 0 or size <= 0
        return 1 if bad else 0
    base_dir = (pathlib.Path(a.root) / "build" / "rust_oracle" if a.root else OUT) / a.base
    if a.cmd == "ticks":
        return 1 if compare_ticks(base_dir, OUT / a.name, seeds(a.seeds)) else 0
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
