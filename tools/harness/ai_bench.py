"""TEST ONLY: the AI-vs-AI benchmark (docs/AI_RESEARCH.md stage 0), a thin loop over
`game_session.py --exercise ai-game` with the decision trace on (EE_AI_TRACE=1; tools/harness/ai_trace.py).

Every run of the matrix (seeds x map types x difficulties x AI counts x cheat sets) is one seeded, time-shimmed,
headless game to the time cap, one after the other (one harness session at a time on a machine). Per run it keeps
the session log and the game's summary, and writes:

  build/boot/ai_bench/<name>/results.json   every run: outcome, units, per-player trace summary (epochs, stats rows)
  build/boot/ai_bench/<name>/results.csv    one row per (run, computer player)
  build/boot/ai_bench/<name>/runs/<run>.session.txt

  python -I tools/harness/ai_bench.py --name baseline --seeds 1,2 --difficulty Hard --cheats all,none
  python -I tools/harness/ai_bench.py --name determinism --seeds 2 --repeat 2    # also compares the two traces
  python -I tools/harness/ai_bench.py --report build/boot/ai_bench/baseline     # the summary table again

--cheats takes cheat sets separated by spaces or commas between sets with '+' inside one set ("all", "none",
"reveal+extra"; '/' between per-player sets taken in turn, "none/all"; EE_AI_CHEATS in ee/EECPTrace.h). The local human idles in slot 0 (ai_game.py). By default the runs
pass --no-human-defeat, which keeps it in, so the computers play on once it has lost its units (--human-defeat:
the game ends there). Its population still pulls the AI pop cap down unless the cheat set leaves out popband.
"""
import argparse
import concurrent.futures
import csv
import itertools
import json
import os
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import ai_trace

OUT = ROOT / 'build' / 'boot' / 'ai_bench'


def split(value, sep=','):
    return [v.strip() for v in value.split(sep) if v.strip()]


def run_one(args, run, out, instance=None):
    env = dict(os.environ, EE_AI_TRACE='1')
    cheats = run['cheats'].replace('+', ',')
    if cheats == 'all':
        env.pop('EE_AI_CHEATS', None)
    else:
        env['EE_AI_CHEATS'] = cheats
    if run.get('personality', '-') != '-':   # aipersonalities ids, '+' joined: taken in turn by player
        env['EE_AI_PERSONALITY'] = run['personality'].replace('+', ',')
    else:
        env.pop('EE_AI_PERSONALITY', None)
    cmd = [sys.executable, '-I', str(HERE / 'game_session.py'), '--exercise', 'ai-game',
           '--ai-players', str(run['ai']), '--map-size', run['size'], '--difficulty', run['difficulty'],
           '--max-game-seconds', str(args.game_seconds), '--time-factor', str(args.time_factor), '--no-render',
           '--seed', str(run['seed']), '--pt-rate', str(args.pt_rate), '--timeout', str(args.timeout)]
    if run['map'] != '-':
        cmd += ['--map-type', run['map']]
    if not args.human_defeat:
        cmd.append('--no-human-defeat')
    if args.build_dir:
        cmd += ['--build-dir', str(args.build_dir)]
    if instance is not None:   # --jobs: game_session's parallel instance (own stage, config.ini, ports, mutex bypass)
        cmd += ['--instance', str(instance)]
    print(f"== {run['id']}{f' [instance {instance}]' if instance else ''}: {' '.join(cmd[3:])} (EE_AI_CHEATS={env.get('EE_AI_CHEATS', 'all')})", flush=True)
    t0 = time.monotonic()
    log = out / 'runs' / f"{run['id']}.harness.log"
    with log.open('w', encoding='utf-8') as stream:
        rc = subprocess.run(cmd, stdout=stream, stderr=subprocess.STDOUT, env=env, cwd=ROOT).returncode
    return collect(run, out, rc, round(time.monotonic() - t0))


def collect(run, out, rc=None, wall_s=None):
    """A run's result from its harness log (runs/<id>.harness.log) and evidence: the game's summary and the trace's."""
    log = out / 'runs' / f"{run['id']}.harness.log"
    text = log.read_text(encoding='utf-8', errors='replace')
    m = re.search(r'^EVIDENCE (.+)$', text, re.M)
    result = dict(run, returncode=rc, wall_s=wall_s, evidence=m and m[1].strip())
    if not m:
        result['error'] = 'no EVIDENCE line (see the harness log)'
        return result
    evidence = pathlib.Path(m[1].strip())
    session = ai_trace.log_path(evidence)
    kept = out / 'runs' / f"{run['id']}.session.txt"
    kept.write_bytes(session.read_bytes())
    result['session'] = str(kept)
    game = evidence / 'ai_game.json'
    if game.exists():
        s = json.loads(game.read_text(encoding='utf-8'))['summary']
        result.update(outcome=s.get('outcome'), final_pt=s.get('final_pt'), game_ms=s.get('game_ms'),
                      winner=s.get('winner'), alive_computers=s.get('alive_computers'),
                      units=[p.get('units_lb14') for p in s.get('players', [])],
                      resources=[p.get('resources') for p in s.get('players', [])])
    result['trace_lines'] = len(ai_trace.trace(kept))
    result['players'] = ai_trace.summary(kept)
    return result


def write_reports(out, results):
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    fields = ['run', 'seed', 'map', 'size', 'difficulty', 'ai', 'cheats', 'personality', 'repeat', 'outcome', 'game_s', 'player',
              'units', 'epoch_final', 'epoch_advances', 'first_epoch_s', 'last_epoch_s', 'first_attack_s',
              'pop', 'cap', 'citizens', 'idle', 'gather_food', 'gather_wood', 'gather_stone', 'gather_gold',
              'gather_iron', 'buildings', 'other', 'unspent_final', 'mean_unspent', 'mean_idle',
              'pop_capped_samples', 'stats_samples', 'build_starts', 'build_stalled', 'build_nosite',
              'build_nobuilder', 'trains', 'ai_mean_us', 'ai_max_us']
    with (out / 'results.csv').open('w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        for r in results:
            for pid, p in (r.get('players') or {}).items():
                last = p['last_stats'] or {}
                epochs = p['epoch_advances']
                units = r.get('units') or []
                w.writerow(dict(run=r['id'], seed=r['seed'], map=r['map'], size=r['size'], difficulty=r['difficulty'],
                                ai=r['ai'], cheats=r['cheats'], personality=r.get('personality', '-'), repeat=r['repeat'], outcome=r.get('outcome'),
                                game_s=(r.get('game_ms') or 0) // 1000, player=pid,
                                units=units[int(pid) - 1] if 0 < int(pid) <= len(units) else '',
                                epoch_final=last.get('epoch'), epoch_advances=len(epochs),
                                first_epoch_s=epochs[0]['t'] // 1000 if epochs else '',
                                last_epoch_s=epochs[-1]['t'] // 1000 if epochs else '',
                                first_attack_s=p['first_attack_ms'] // 1000 if p['first_attack_ms'] else '',
                                pop=last.get('pop'), cap=last.get('cap'), citizens=last.get('citizens'),
                                idle=last.get('idle'), **{k: last.get(k) for k in ('gather_food', 'gather_wood',
                                'gather_stone', 'gather_gold', 'gather_iron')}, buildings=last.get('buildings'), other=last.get('other'),
                                unspent_final=last.get('unspent'), mean_unspent=p['mean_unspent'],
                                mean_idle=p['mean_idle'], pop_capped_samples=p['pop_blocked_samples'],
                                stats_samples=p['stats_samples'], build_starts=p['events'].get('build.start', 0),
                                build_stalled=p['events'].get('build.stalled', 0),
                                build_nosite=p['events'].get('build.nosite', 0),
                                build_nobuilder=p['events'].get('build.nobuilder', 0),
                                trains=p['events'].get('train.request', 0), ai_mean_us=p['profile']['mean_us'],
                                ai_max_us=p['profile']['max_us']))


def report(out):
    results = json.loads((out / 'results.json').read_text(encoding='utf-8'))
    print(f"{'run':34} {'outcome':10} {'game s':>6}  per computer player: epoch, citizens, other units, "
          f"unspent, first attack s")
    for r in results:
        cells = []
        for pid, p in (r.get('players') or {}).items():
            last = p['last_stats'] or {}
            fa = p['first_attack_ms'] // 1000 if p['first_attack_ms'] else '-'
            cells.append(f"p{pid}: e{last.get('epoch')} c{last.get('citizens')} o{last.get('other')} "
                         f"u{last.get('unspent')} a{fa}")
        print(f"{r['id']:34} {str(r.get('outcome') or r.get('error')):10} {(r.get('game_ms') or 0) // 1000:>6}  "
              + '; '.join(cells))
    if (out / 'determinism.json').exists():
        for cell, verdict in json.loads((out / 'determinism.json').read_text(encoding='utf-8')).items():
            print(f'determinism {cell}: {verdict}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--name', default=time.strftime('%Y%m%d-%H%M%S'))
    ap.add_argument('--seeds', default='2', help='comma separated (default 2, the gate seed)')
    ap.add_argument('--maps', default='-', help="map types, comma separated ('-': the lobby default, Continental)")
    ap.add_argument('--sizes', default='Small')
    ap.add_argument('--difficulty', default='Hard', help='comma separated: Easy, Medium, Hard')
    ap.add_argument('--ai', default='2', help='computer player counts, comma separated')
    ap.add_argument('--cheats', default='all', help="cheat sets, comma separated; '+' joins cheats in one set")
    ap.add_argument('--personality', default='-',
                    help="personality sets, comma separated ('-': the rules' assignment); '+' joins the ones the computer "
                         "players take in turn (EE_AI_PERSONALITY), e.g. tall+wide")
    ap.add_argument('--repeat', type=int, default=1, help='runs per cell; with 2+ the traces are compared')
    ap.add_argument('--game-seconds', type=int, default=600)
    ap.add_argument('--time-factor', default='32')
    ap.add_argument('--pt-rate', type=int, default=40)
    ap.add_argument('--timeout', type=int, default=2400000)
    ap.add_argument('--human-defeat', action='store_true',
                    help='let the idle human be defeated (that ends the game); default: --no-human-defeat')
    ap.add_argument('--build-dir', type=pathlib.Path)
    ap.add_argument('--jobs', type=int, default=1,
                    help='games at once (game_session --instance 1..N; 32 at most); the machine needs a core or two per game')
    ap.add_argument('--report', type=pathlib.Path, help='print the summary of an earlier benchmark and exit')
    ap.add_argument('--rescan', type=pathlib.Path,
                    help='rebuild results.json/csv of an earlier benchmark from its kept logs (no games), then report')
    a = ap.parse_args()
    if a.report:
        report(a.report)
        return
    if a.rescan:
        old = {}
        if (a.rescan / 'results.json').exists():
            old = {r['id']: r for r in json.loads((a.rescan / 'results.json').read_text(encoding='utf-8'))}
        results = []
        for log in sorted((a.rescan / 'runs').glob('*.harness.log')):
            rid = log.name[:-len('.harness.log')]
            m = re.match(r's(\d+)-(.*)-(\w+)-(Easy|Medium|Hard)-(\d+)ai-(.+)-r(\d+)$', rid)
            run = old.get(rid) or dict(id=rid, seed=int(m[1]), map=m[2] or '-', size=m[3], difficulty=m[4],
                                       ai=int(m[5]), cheats=m[6], repeat=int(m[7]))
            run = {k: run.get(k, '-') for k in ('id', 'seed', 'map', 'size', 'difficulty', 'ai', 'cheats', 'personality',
                                                'repeat')}
            results.append(collect(run, a.rescan))
        write_reports(a.rescan, results)
        report(a.rescan)
        return
    out = OUT / a.name
    (out / 'runs').mkdir(parents=True, exist_ok=True)
    cells = list(itertools.product([int(s, 0) for s in split(a.seeds)], split(a.maps), split(a.sizes),
                                   split(a.difficulty), [int(n) for n in split(a.ai)], split(a.cheats),
                                   split(a.personality)))
    runs = []
    for (seed, map_type, size, difficulty, ai, cheats, pers), k in itertools.product(cells, range(a.repeat)):
        rid = f"s{seed}-{map_type.replace(' ', '_')}-{size}-{difficulty}-{ai}ai-{cheats.replace('/', '~')}"               + (f"-{pers}" if pers != '-' else '') + f"-r{k}"
        runs.append(dict(id=rid.replace('--', '-'), seed=seed, map=map_type, size=size, difficulty=difficulty, ai=ai,
                         cheats=cheats, personality=pers, repeat=k))
    results = [None] * len(runs)
    if a.jobs > 1:
        # the data tree once, before the parallel sessions: a tree made by other converter code is rebuilt, and
        # parallel sessions rebuilding the shared tree at once break it
        import exe_boot
        text_db, open_assets = os.environ.get('EE_HARNESS_TEXT_DB'), os.environ.get('EE_HARNESS_OPEN_ASSETS')
        exe_boot.data_tree('open' if os.environ.get('EE_HARNESS_OPEN_DATA') or (text_db and open_assets) else 'binary')
    if a.jobs <= 1:
        for n, run in enumerate(runs):
            results[n] = run_one(a, run, out)
            write_reports(out, [r for r in results if r])
    else:
        # parallel instances 1..jobs, each a free slot taken by the next run; the starts are staggered so the
        # sessions do not stage at the same moment
        slots = list(range(a.jobs, 0, -1))
        lock = __import__('threading').Lock()

        def work(n):
            with lock:
                instance = slots.pop()
            try:
                time.sleep(10 * (n % a.jobs) if n < a.jobs else 0)
                result = run_one(a, runs[n], out, instance)
                if not result.get('outcome'):   # a session that failed before the game (seen: a message form over
                    print(f"== {runs[n]['id']}: no outcome, retried once", flush=True)   # the main menu at start)
                    result = run_one(a, runs[n], out, instance)
                return n, result
            finally:
                with lock:
                    slots.append(instance)
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            for n, result in ex.map(work, range(len(runs))):
                results[n] = result
                write_reports(out, [r for r in results if r])
    determinism = {}
    if a.repeat > 1:
        by_cell = {}
        for r in results:
            by_cell.setdefault(r['id'].rsplit('-r', 1)[0], []).append(r)
        for cell, runs in by_cell.items():
            if len(runs) < 2 or not all(r.get('session') for r in runs):
                continue
            ok = all(ai_trace.compare(runs[0]['session'], r['session']) for r in runs[1:])
            determinism[cell] = 'identical' if ok else 'DIFFERENT'
        (out / 'determinism.json').write_text(json.dumps(determinism, indent=2) + '\n', encoding='utf-8')
    report(out)
    print(f'BENCH {out}')
    sys.exit(0 if all(v == 'identical' for v in determinism.values()) else 1)


if __name__ == '__main__':
    main()
