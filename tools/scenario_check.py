"""TEST ONLY: every shipped campaign scenario loads and plays in EE-SIM, and a corrupt one fails cleanly.

  python -I tools/scenario_check.py [--exe EXE] [--data TREE] [--seconds N] [--jobs 2] [--name NAME] [--only K ...]

The scenarios come from the data tree's Data/Campaigns archives (retail data: extracted into build/scenario_check,
never committed). The Art of Conquest campaigns' scenarios (AOCRoman, AOCAsian, AOCPacific) are multiplayer files
and play through the lobby's Play Scenario screen (EE-SIM --scenario FILE); the Learning Campaign's are single-player
files that screen refuses ("This is not a valid multiplayer scenario file.") and play through the campaign screens
(EE-SIM --campaign learning --scenario-index K: every Learning Campaign scenario is open). Each must reach the game
and end by EE-SIM's own rules (time-cap, game-end, last-computer-standing) after --seconds of play.

The corrupt case: the first Art of Conquest scenario with its players chunk (tag 5) cut to half its decompressed
length and stored raw, so the map, the scenario, and the effects (whose units are saved ids until the players are
repaired) load and the players' read runs off the chunk's end. EE-SIM must end with "load failed: Load game file is
corrupt." (EEGameStateLoader::LoadGame's teardown done, EEGameState::Clear), never a fault: the failed load's
teardown once dereferenced an effect's saved unit id (EEEffect::~EEEffect, PORT_LEARNINGS "Campaign scenarios: the
Learning Campaign, the attack groups, a failed load's teardown").

Exit 0 and "SCENARIO CHECK PASS" when every case passes.
"""
import argparse
import concurrent.futures
import json
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "eedata"))
sys.path.insert(0, str(ROOT / "tools" / "harness"))
import sim  # noqa: E402
import ssa  # noqa: E402

OUT = ROOT / "build" / "scenario_check"
AOC = ["AOCRoman", "AOCAsian", "AOCPacific"]
LEARNING = "EELearningCampaign"
GOOD_OUTCOMES = {"time-cap", "game-end", "last-computer-standing"}


def extract(data):
    """{campaign: [(name, path)]} of every scenario of the tree's campaign archives (extracted once)."""
    out = {}
    for campaign in AOC + [LEARNING]:
        entries, _tags, d = ssa.read_ssa(sim.data_tree(data) / "Data" / "Campaigns" / f"{campaign}.ssa")
        for key in sorted(entries):
            if not key.lower().endswith(".scn"):
                continue
            name = key.replace("\\", "/").split("/")[-1][:-4]
            path = OUT / "scenarios" / campaign / f"{name}.scn"
            if not path.exists():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(ssa.entry_bytes(d, *entries[key]))
            out.setdefault(campaign, []).append((name, path))
    return out


def chunks(raw):
    """The chunk list's start and its chunks (offset, major, minor, tag, size) of a scenario or saved game."""
    for start in range(4, len(raw) - 16):
        major, minor, tag = struct.unpack_from("<III", raw, start)
        if major != 0 or not 200 <= minor <= 400 or tag != 1:   # the checksum chunk comes first
            continue
        out, p = [], start
        while p + 16 <= len(raw):   # a header field can look like one: the walk must end at the file's end
            major, minor, tag, size = struct.unpack_from("<IIII", raw, p)
            out.append((p, major, minor, tag, size))
            p += 16 + size
        if p == len(raw):
            return start, out
    raise ValueError("no chunk list")


def corrupt(source, target):
    """A copy of source whose players chunk (tag 5) is its first half, decompressed and stored raw."""
    import savefile
    raw = source.read_bytes()
    _start, table = chunks(raw)
    out = bytearray(raw[:table[0][0]])
    for offset, major, minor, tag, size in table:
        body = raw[offset + 16:offset + 16 + size]
        if tag == 5:
            data = savefile.explode(body[12:]) if body[:4] == b"PK01" else body
            body = data[:len(data) // 2]
        out += struct.pack("<IIII", major, minor, tag, len(body)) + body
    struct.pack_into("<I", out, 0, len(out) - 4)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(bytes(out))
    return target


def run(name, exe, data, args, seconds, label):
    """One EE-SIM game: (label, exit code, report or None, last log line)."""
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    safe = "".join(c for c in label.lower() if c.isalnum())[:40]
    report = d / f"{safe}.json"
    report.unlink(missing_ok=True)
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot",
                        f"scncheck-{name}-{safe}", "--exe", str(exe), "--", "--seed", "2", *args, "--game-seconds",
                        str(seconds), "--report", str(report)], cwd=ROOT, capture_output=True, text=True)
    log = r.stdout + r.stderr
    (d / f"{safe}.log").write_text(log)
    try:
        rep = json.loads(report.read_text())
    except (OSError, ValueError):
        rep = None
    last = [ln for ln in log.splitlines() if "FAULT" in ln or "FAILED" in ln or ln.startswith("[sim")]
    return label, r.returncode, rep, (last[-1] if last else "")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=str(ROOT / "build" / "clang" / "exe" / "EE-SIM.exe"))
    ap.add_argument("--data", default="open")
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--name", default="check")
    ap.add_argument("--no-corrupt", action="store_true", help="skip the corrupt case")
    ap.add_argument("--corrupt-only", action="store_true", help="only the corrupt case")
    a = ap.parse_args()
    scenarios = extract(a.data)
    cases = []   # (label, args, expect)
    if not a.corrupt_only:
        for campaign in AOC:
            for name, path in scenarios[campaign]:
                cases.append((f"{campaign}/{name}", ["--scenario", str(path)], "play"))
        for k, (name, _path) in enumerate(scenarios[LEARNING]):
            cases.append((f"{LEARNING}/#{k}", ["--campaign", "learning", "--scenario-index", str(k)], "play"))
    if not a.no_corrupt:
        name, path = scenarios[AOC[0]][0]
        bad = corrupt(path, OUT / "scenarios" / "corrupt" / f"{name} (players cut).scn")
        cases.append((f"corrupt/{bad.stem}", ["--scenario", str(bad)], "fail"))
    failed = 0
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool:
        futures = {pool.submit(run, a.name, a.exe, a.data, args, a.seconds, label): (label, expect)
                   for label, args, expect in cases}
        for f in concurrent.futures.as_completed(futures):
            label, expect = futures[f]
            _label, code, rep, last = f.result()
            outcome = rep.get("outcome") if rep else None
            error = (rep or {}).get("error", "")
            if expect == "play":
                ok = code == 0 and outcome in GOOD_OUTCOMES
                what = f"{outcome} at {rep.get('game_ms')} ms" if rep else f"no report (exit {code})"
            else:
                ok = code == 2 and outcome == "error" and "load failed" in error
                what = f"exit {code} {outcome}: {error}" if rep else f"no report (exit {code})"
            if "FAULT" in last:
                ok, what = False, last
            failed += not ok
            print(f"{'ok  ' if ok else 'FAIL'} {label}: {what}" + ("" if ok else f" | {last}"), flush=True)
    print(f"SCENARIO CHECK {'PASS' if not failed else 'FAIL'}: {len(cases) - failed}/{len(cases)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
