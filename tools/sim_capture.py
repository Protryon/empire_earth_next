"""TEST ONLY: the simulation capture gate of the Rust leaves (docs/RUST_PLAN.md R5b; ee/EESimCapture.h).

  python -I tools/sim_capture.py run NAME [--exe EXE] [--seeds 2-7] [--jobs 2] [--data open]  -> build/sim_capture/NAME/
  python -I tools/sim_capture.py digest NAME [--write]     per seed and tag: records and FNV-1a of their bytes
`digest` and `check` compute the digests with `cargo run --release -p xtask -- capture-digest` when cargo is there
(the same lines; EE_SIM_CAPTURE_PYTHON_DIGEST=1 forces the Python digest, the fallback without cargo).
  python -I tools/sim_capture.py check NAME                the digests equal tools/sim_capture/golden_cpp.txt
  python -I tools/sim_capture.py replay NAME               the Rust leaves recompute every captured output

`run` plays the oracle's games (tools/sim_oracle.py: 2-AI Hard, 2700 game seconds, no cheats) with EE_SIM_CAPTURE
set, so every call of a captured leaf (formations: FMLN/FMWG/FMCI) appends its inputs and outputs. The captures are
game-derived and stay under build/; only their digests are checked in. golden_cpp.txt was written (`digest --write`)
from a capture of the C++ leaves before they were deleted: a later build whose games and leaves are unchanged gives
the same records, so `check` gates the whole path (the inputs the game hands a leaf and its outputs), and `replay`
gates the Rust leaves alone over the C++ capture, even after the game has changed.

EFMU is the effect manager's state after every Update (EEEffectManager.cpp: the last handle, the sizes and an
FNV-1a 64 of the index, the list and the destruction queue); its golden lines come from the C++ manager (R4.4, sixth
round; the missile profile's golden does not list it).
ASRN is the tile A* searches' record (ee/EEAIAStarCapture.h): one per EEAIAStarMachine::Run, with the run's inputs,
the count and FNV-1a digest of everything the machine asked its goal and the goal asked its view (in order, with the
answers), and the run's outputs (the machine's members, the goal's flags, a digest of every storage node in list
order). Its golden lines come from the C++ machine (R5b.1 step 2). EE_SIM_CAPTURE_ASTAR_EVENTS=1 also writes the
events themselves (about 25 bytes each, some 35 million in the six games): for finding where two builds part.
ASGR is the graph searches' record (EEAIAStarGraphMachine::Run: the sub sector paths and the continental chains), in
the same shape: inputs, the digest of the machine's questions to its goal, the outputs and the storage's lists. Its
golden lines come from the C++ graph machine, storage and goals.
CNTB and CNTR are the continent manager's analysis (ee/EEAIContinentCapture.h): one record after BuildRegions (CNTB,
with the work grids still allocated) and one after every Rebuild (CNTR), each the whole state the game reads: the
counters, the per-tile continent and sub sector ids and tile values, the continent adjacency matrix, every continent,
path area, path sector, sub sector, choke point, hill, forest and rectangle area with its tiles, ids and links, the
shore tiles. Their golden lines come from the C++ analysis (the continent manager, the choke point pass and the area
classes unchanged; the tile and graph A* searches under it already Rust and equal by ASRN/ASGR).
CNTN is every GetTilesNextTo call of the continent manager (the tile, the continent id, the result, the neighbour
tiles), PMSP the path manager's splicing (EEPathManager.cpp: TrimPath, AppendWaypoints, JoinPaths, FinishFailedPath's
detour search, the bounded searches' revolutions and bounds; a kind, the inputs, the outputs). Their golden lines
come from the C++ (R5b.1, branch rust-r5b-cont2).
HULL is the AI hulls' geometry (lle/U2DPolygon.h, U2DConvexHull<long>: every SetPoints with its input and the whole
hull, GetClosestOutlinePoint, GetClosestPoints, GetClosestPoint, Contains and GetScanLines with a hash of the hull, the
input and the result), AREA the areas' geometry on it (EEAIArea::GetClosestPoints, GetClosestHullPoint,
GetHullDistanceSquared, GetTiles; EEAIContinent::GetClosestPoints, GetShoreInRegions). Only the outermost call of a
thread is recorded, not the calls it makes inside. Their golden lines come from the C++ (R5b.1, branch
rust-r5b-cont3).
CB** are the combat and unit formulas' records (ee/EECombatFormulas.h: damage, attack factor, elevation, morale, hit
points, healing, conversion, the ballistic pitch, the projectile arc, the reach and range tests, the damage rows): one
per call, the input record's bytes and the output's. Their golden lines come from the C++ formulas (R5b.2).
CBTC is the goals' and shots' trigger tests (ee/EECombatGoals.h: the plain reads, the lazy reads in order, the outcome),
folded 4096 tests to a record (count and FNV-1a 64), CBGS every goal step with its operations in order; their golden
lines come from the instrumented original C++ bodies (R5b.2, sixth round); the missile profile's golden lists them too.


The tech tree, event effect and calamity records (ee/EETechCapture.h: TQRS research starts, TTRS/TNRS/TNPR/TSOL/TEPO
research, prerequisites and epochs, TCST/TSUM costs, EVRN/EVAT/EVHP effects with the values before and after,
CALC/CALS/CALU calamities, TSWP the tech trees' sweep every five game minutes) are in every profile. The oracle's
computer players stay in the first epochs and never cast a calamity, so --profile late and space play other games
(later start epochs, the computer players' cheats, more players) and --profile calamity the oracle's game with every
calamity class cast in turn (EE-SIM --calamities); their digests are golden_cpp_<profile>.txt.
--profile missile is the oracle's game with EE-SIM --missile-sweep: when it ends, the players' units fire every missile
and torpedo class (CBMA/CBMH, the missile's flight steps, and its impact tests' CBGI/CBWI); its golden file lists only
those tags (and CBTC, CBGS), every other tag of it is the oracle profile's (reported as without a golden).
CALW (every calamity update with its state before and after and its world calls), CCAS (CanCastAt: the game's
calls and a sweep over every class, the computer players' first units and real points and targets), TRTE
(ResearchToEpoch's steps) and EVUN (the units an effect walks, with what applies and the values) were recorded from
the C++ before the calamity updates, CanCastAt, ResearchToEpoch and the effects' unit walks moved (R5b.2, round 2).
CALT (every calamity class's Start and Stop with the calamity's, its units' and the contagion manager's state and the
start's placed effects and sounds), CONT (every contagion helper update: the spread's candidates with their draws and
distances, the new, applied and removed units, the functors' damage) and TNWK (the tech tree's walks and tests:
Unlock, ResearchAll, ResearchKind, ObsoleteOutdatedNodes, MakeObsolete, SetObsolete, NeedsUnitMedia, GetBuildTime,
SetEpoch, AdvanceEpoch, SetNodeAvailable) were recorded from the C++ before those moved (R5b.2, round 3).
GetBuildTime and NeedsUnitMedia, asked from the media loader's thread too, are TNMT: their records' order and count
depend on timing, so TNMT has no golden (TNWK was recorded again without them from the unmodified C++).
NM** are the game messages' records (ee/EEMessageCapture.h, R5b.4): NMIM every message the game's factory builds (the
bytes it came from, the bytes it exports again, its members as the import left them), NMSW the same for a sweep of
every game message type over edge payloads (once, at the first sweep), NMCU the box selection's unit packing, NMNE
the engine messages over edge payloads (with the players' inputs nested in the communications ticks). Their
golden lines come from the C++ codec (EEMessage.cpp's original ExportMessage/ImportMessage bodies).
NMDC is the messages' Execute decisions (ee/EEMessageExecute.h: the selection id check, the resources): every call in
game, their tables over edge values once, the real players' prices, exchanges and refunds every five game minutes.
Its golden lines come from the C++ decisions (R5b.4 leaf 4).
USCK is the unit store's check (ee/EEUnitStore.h, R5b.4 leaf 5): at every physics tick and before every executed game
message, every unit of every player's list against its Rust record (the units compared; a difference stops EE-SIM with
exit code 3). USPK is the box selection's pick (EESelectPick): every game call and, every five game minutes, a sweep
over every player's units; its golden lines come from the C++ pick (EEPlayer::SelectUnits' loop). USFL is the active
groups' Fill test (EEFillMembers: which of a selection's units an active group takes for a formation slot): every
EEActiveGroup::Fill (the computer players' attack groups; the group commands) and, every five game minutes, every
player's units in every slot; its golden lines come from the C++ test (EEActiveGroup::Fill's loop).
USGC is the group commands' sweep (ee/EEUnitHost.h, EEGroupCommandSweep.cpp): every five game minutes, for every
player and a set of selections and arguments, the Rust body of a group command run over the dry host (every world
call logged, nothing changed); the original body's statements run over the same host must give the same log (else
EE-SIM exits 3). The record holds the Rust body's log.
USTM is the unit step's leaves (ee/EEUnitStore.h, ee_units step.rs): one record per physics tick, the calls of the
reload, target and heal search countdowns and RegeneratePower and the FNV-1a of each call's kind, unit, inputs and
outputs. Its golden lines come from the C++ bodies (oracle, late 3-5, space 2-3); late 2, 6, 7 (the fishing-boat
rebaseline's games), space 4-7 and calamity from the Rust bodies, whose every call the original's statements recheck
while the capture runs (EE-SIM exits 3 on a difference).
SM** are the scenario manager's records (ee/EEScenarioCapture.h, R5b.5): SMCE every trigger condition's test, SMTE
every trigger's evaluation, SMOB every scenario object's, SMCP the player choices and SMAR the area queries outside an
object's evaluation, SMEX every effect run, SMUP each trigger round. No random map has a trigger, so --profile
scenario plays a retail campaign scenario per seed (SCENARIOS, extracted from the data tree's campaign archives into
build/sim_capture/scenarios; EE-SIM --scenario); its golden file lists only the SM tags (recorded from the C++
scenario manager before its logic moved); its USCK stops the game on a mismatch like every profile's (a scenario's
path points, on the map in no list, are the store's unlisted records: R5b.4's fourteenth round).
`replay` covers the formations, the combat formulas and the tech tree's records (ee_tech: TNRS, TNPR, TSOL, TSWP,
EVHP).
"""
import argparse
import concurrent.futures
import os
import pathlib
import struct
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import rust_build  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "sim_capture"
GOLDEN = ROOT / "tools" / "sim_capture" / "golden_cpp.txt"
# the games of a profile: EE-SIM's options after the seed (the oracle's: tools/sim_oracle.py)
PROFILES = {
    "oracle": ["--difficulty", "Hard", "--game-seconds", "2700", "--cheats", "none"],
    # from the Middle Ages, three computer players with the original's cheats: castles, priests, prophets
    "late": ["--difficulty", "Hard", "--game-seconds", "2700", "--cheats", "all", "--ai", "3", "--lobby", "183=#5"],
    # from the Atomic Age (WW I), to the space age's techs and powers
    "space": ["--difficulty", "Hard", "--game-seconds", "2700", "--cheats", "all", "--ai", "3", "--lobby", "183=#9"],
    # the oracle's game with every calamity class cast in turn (EE-SIM --calamities: the computer players cast none)
    "calamity": ["--difficulty", "Hard", "--game-seconds", "2700", "--cheats", "none", "--calamities"],
    # the oracle's game, then every missile class fired at its end (EE-SIM --missile-sweep: no game of the other
    # profiles flies a missile); its golden lists only the sweep's tags (CBMA, CBMH, CBGI, CBWI)
    "missile": ["--difficulty", "Hard", "--game-seconds", "2700", "--cheats", "none", "--missile-sweep"],
    # a retail campaign scenario per seed (SCENARIOS; EE-SIM --scenario: the lobby's Play Scenario screen), the
    # scenario manager's triggers, conditions, objects, areas and effects (SM** records; no random map runs one)
    "scenario": ["--game-seconds", "1800", "--scenario", "{scenario}"],
}

# the scenario profile's games by seed (seed 2 the first): (campaign archive, scenario) of the data tree's
# Data/Campaigns, extracted once into build/sim_capture/scenarios (retail data: never committed)
SCENARIOS = [
    ("AOCRoman", "een1ve1s_a conqueror learns his trade"),
    ("AOCAsian", "een1ve1s_a new dynasty"),
    ("AOCPacific", "een1ve1s_carrier strike"),
    ("AOCRoman", "een1ve1s_the gallic wars"),
    ("AOCAsian", "een1ve1s_the breaking point"),
    ("AOCPacific", "een1ve1s_the meat grinder"),
]


def scenario_file(seed, data):
    """The scenario of a seed, extracted from its campaign archive in the data tree (once)."""
    sys.path.insert(0, str(ROOT / "tools"))
    sys.path.insert(0, str(ROOT / "tools" / "eedata"))
    import sim  # noqa: E402
    import ssa  # noqa: E402
    campaign, name = SCENARIOS[(seed - 2) % len(SCENARIOS)]
    out = OUT / "scenarios" / f"{name}.scn"
    if not out.exists():
        entries, _tags, d = ssa.read_ssa(sim.data_tree(data) / "Data" / "Campaigns" / f"{campaign}.ssa")
        key = next(k for k in entries if k.lower() == f"scenarios\\{name}.scn")
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(ssa.entry_bytes(d, *entries[key]))
    return out


def golden(profile):
    return GOLDEN if profile == "oracle" else GOLDEN.with_name(f"golden_cpp_{profile}.txt")


def seeds(text):
    out = []
    for part in text.split(","):
        a, _, b = part.partition("-")
        out += list(range(int(a), int(b or a) + 1))
    return out


def one(name, exe, seed, data, profile="oracle"):
    d = OUT / name
    d.mkdir(parents=True, exist_ok=True)
    capture, trace, report = d / f"capture-{seed}.bin", d / f"trace-{seed}.txt", d / f"report-{seed}.json"
    capture.unlink(missing_ok=True)
    env = dict(os.environ, EE_SIM_CAPTURE=str(capture.resolve()))
    args =[str(scenario_file(seed, data)) if v == "{scenario}" else v for v in PROFILES[profile]]
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "sim.py"), "--data", data, "--slot",
                        f"capture-{name}-{seed}", "--exe", str(exe), "--", "--seed", str(seed), *args,
                        "--report", str(report), "--ai-trace", "--trace", str(trace)], cwd=ROOT, capture_output=True,
                       text=True, env=env)
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


def rust_digest_lines(paths):
    """{seed: lines} from xtask capture-digest (rust/crates/xtask/src/capture_digest.rs: the same lines, the seeds in
    parallel, about a hundred times faster), or None when cargo or the tool is missing or fails."""
    if not paths or os.environ.get("EE_SIM_CAPTURE_PYTHON_DIGEST"):
        return None
    try:
        cargo = rust_build.cargo()
    except SystemExit:
        return None
    try:
        r = subprocess.run([cargo, "run", "-q", "--release", "-p", "xtask", "--", "capture-digest",
                            *(f"{seed}={p.resolve()}" for seed, p in paths)], cwd=rust_build.RUST,
                           env=rust_build.cargo_env(), capture_output=True, encoding="utf-8")
    except OSError:
        return None
    if r.returncode:
        print(f"  (xtask capture-digest failed, digesting in Python: {r.stderr.strip()[-300:]})", file=sys.stderr)
        return None
    out = {seed: [] for seed, _ in paths}
    for line in r.stdout.splitlines():
        out[int(line.split()[1])].append(line)
    return out


def digest_lines(name, seed_list):
    found = [(seed, OUT / name / f"capture-{seed}.bin") for seed in seed_list]
    found = [(seed, p) for seed, p in found if p.exists()]
    fast = rust_digest_lines(found)
    lines = []
    for seed in seed_list:
        p = OUT / name / f"capture-{seed}.bin"
        if not p.exists():
            lines.append(f"seed {seed} MISSING")
            continue
        if fast is not None:
            lines += fast[seed]
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
    for cmd in ("digest", "check", "replay"):
        s = sub.add_parser(cmd)
        s.add_argument("name")
        if cmd == "digest":
            s.add_argument("--write", action="store_true", help="write the digests to golden_cpp.txt")
    for s in sub.choices.values():
        s.add_argument("--seeds", default="2-7")
        s.add_argument("--profile", default="oracle", choices=sorted(PROFILES))
    a = ap.parse_args()
    seed_list = seeds(a.seeds)
    if a.cmd == "run":
        bad = 0
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            for seed, code, size in ex.map(lambda s: one(a.name, a.exe, s, a.data, a.profile), seed_list):
                print(f"{a.name} seed {seed}: exit {code}, capture {size} bytes", flush=True)
                bad += code != 0 or size <= 0
        return 1 if bad else 0
    if a.cmd == "digest":
        lines = digest_lines(a.name, seed_list)
        print("\n".join(lines))
        if a.write:
            golden(a.profile).parent.mkdir(parents=True, exist_ok=True)
            golden(a.profile).write_bytes(("\n".join(lines) + "\n").encode())
        return 0
    if a.cmd == "check":
        want = [l for l in golden(a.profile).read_text().splitlines() if l.strip()]
        got = digest_lines(a.name, seed_list)
        want = [l for l in want if int(l.split()[1]) in seed_list]
        # a tag the golden file does not list for a seed has no golden yet: reported, not compared (the profile
        # goldens were recorded tag by tag as the leaves moved)
        gated = {(l.split()[1], l.split()[2]) for l in want}
        ungated = [l for l in got if (l.split()[1], l.split()[2]) not in gated]
        got = [l for l in got if (l.split()[1], l.split()[2]) in gated]
        for l in ungated:
            print(f"  no golden: {l}")
        if got == want:
            print(f"sim capture: PASS ({len(got)} digests equal to {golden(a.profile).name}"
                  f"{f', {len(ungated)} without a golden' if ungated else ''})")
            return 0
        for w, g in zip(want + [""] * len(got), got + [""] * len(want)):
            if w != g:
                print(f"  golden {w!r}\n  got    {g!r}")
        print("sim capture: FAIL")
        return 1
    files = [str(OUT / a.name / f"capture-{seed}.bin") for seed in seed_list]
    # this checkout's build/rust, as every cargo run of tools/rust_build.py
    codes = [rust_build.run_cargo(["run", "-q", "--release", "-p", crate, "--example", example, "--", *files])
             for crate, example in (("ee_formations", "formation_replay"), ("ee_combat", "combat_replay"),
                                    ("ee_tech", "tech_replay"))]
    return 1 if any(codes) else 0


if __name__ == "__main__":
    sys.exit(main())
