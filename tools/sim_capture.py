"""TEST ONLY: the simulation capture gate of the Rust leaves (docs/RUST_PLAN.md R5b; ee/EESimCapture.h).

  python -I tools/sim_capture.py run NAME [--exe EXE] [--seeds 2-7] [--jobs 2] [--data open]  -> build/sim_capture/NAME/
  python -I tools/sim_capture.py digest NAME [--write]     per seed and tag: records and FNV-1a of their bytes
  python -I tools/sim_capture.py check NAME                the digests equal tools/sim_capture/golden_cpp.txt

`run` plays the oracle's games (tools/sim_oracle.py: 2-AI Hard, 2700 game seconds, no cheats) with EE_SIM_CAPTURE
set, so every call of a captured leaf (formations: FMLN/FMWG/FMCI) appends its inputs and outputs. The captures are
game-derived and stay under build/; only their digests are checked in. golden_cpp.txt was written (`digest --write`)
from a capture of the C++ leaves: a later build whose games and leaves are unchanged gives the same records, so
`check` gates the whole path (the inputs the game hands a leaf and its outputs).
"""
import argparse
import concurrent.futures
import os
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "sim_capture"
GOLDEN = ROOT / "tools" / "sim_capture" / "golden_cpp.txt"


def seeds(text):
    out = []
    for part in text.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def one(name, exe, seed, data):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    capture, trace, report = d / f"capture-{seed}.bin", d / f"trace-{seed}.txt", d / f"report-{seed}.json"
    capture.unlink(missing_ok=True)
    env = dict(os.environ, EE_SIM_CAPTURE=str(capture.resolve()))
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot",
                        f"capture-{name}-{seed}", "--exe", str(exe), "--", "--seed", str(seed), "--difficulty", "Hard",
                        "--game-seconds", "2700", "--cheats", "none", "--report", str(report), "--ai-trace",
                        "--trace", str(trace)], cwd=ROOT, capture_output=True, text=True, env=env)
    (d / f"log-{seed}.txt").write_text(r.stdout + r.stderr)
    return seed, r.returncode, capture.stat().st_size if capture.exists() else -1


def fnv(h, data):
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def digests(path):
    """{tag: [records, fnv]} over each record's tag, size and payload, in file order."""
    data = path.read_bytes()
    out, at = {}, 0
    while at < len(data):
        tag, size = struct.unpack_from("<II", data, at)
        name = struct.pack("<I", tag).decode("ascii", "replace")
        n, h = out.get(name, (0, 0xcbf29ce484222325))
        out[name] = (n + 1, fnv(h, data[at:at + 8 + size]))
        at += 8 + size
    return out


def digest_lines(name, seed_list):
    lines = []
    for seed in seed_list:
        p = OUT / name / f"capture-{seed}.bin"
        if not p.exists():
            lines.append(f"seed {seed} MISSING")
            continue
        for tag, (n, h) in sorted(digests(p).items()):
            lines.append(f"seed {seed} {tag} {n} {h:016x}")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("name")
    r.add_argument("--exe", default=str(ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"))
    r.add_argument("--jobs", type=int, default=2)
    r.add_argument("--data", default="open")
    for cmd in ("digest", "check"):
        s = sub.add_parser(cmd)
        s.add_argument("name")
        if cmd == "digest":
            s.add_argument("--write", action="store_true", help="write the digests to golden_cpp.txt")
    for s in sub.choices.values():
        s.add_argument("--seeds", default="2-7")
    a = ap.parse_args()
    seed_list = seeds(a.seeds)
    if a.cmd == "run":
        bad = 0
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            for seed, code, size in ex.map(lambda s: one(a.name, a.exe, s, a.data), seed_list):
                print(f"{a.name} seed {seed}: exit {code}, capture {size} bytes", flush=True)
                bad += code != 0 or size <= 0
        return 1 if bad else 0
    if a.cmd == "digest":
        lines = digest_lines(a.name, seed_list)
        print("\n".join(lines))
        if a.write:
            GOLDEN.parent.mkdir(parents=True, exist_ok=True)
            GOLDEN.write_bytes(("\n".join(lines) + "\n").encode())
        return 0
    if a.cmd == "check":
        want = [l for l in GOLDEN.read_text().splitlines() if l.strip()]
        got = digest_lines(a.name, seed_list)
        want = [l for l in want if int(l.split()[1]) in seed_list]
        if got == want:
            print(f"sim capture: PASS ({len(got)} digests equal to golden_cpp.txt)")
            return 0
        for w, g in zip(want + [""] * len(got), got + [""] * len(want)):
            if w != g:
                print(f"  golden {w!r}\n  got    {g!r}")
        print("sim capture: FAIL")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
