"""TEST ONLY (round 39): run several live game sessions (tools/boot/game_session.py) in parallel.

  python -I tools/boot/game_batch.py [-P 2] [--out DIR] [--job IMAGE[:SEED[:LABEL]]]... [--jobs FILE.json]
         [--job-timeout S] [--stagger S] [session options...]

IMAGE is original, fulllink (build/exe_fulllink/EE-AOC.exe) or fulllink=PATH. Every option this script does not know
is forwarded to every job's game_session.py (e.g. --exercise random-game --pt-rate 40 --play-seconds 60, and the plugin
options --time-factor F / --exercise ai-game --ai-players 4 / --diff-sampler ...). --jobs FILE: a JSON list of
{"image": ..., "seed": N, "label": "...", "args": [...], "timeout": S}; a job's own args come after the shared ones.

Settings: the game keeps them in config.ini in its stage (no registry), so the parallel sessions (--instance K, own
stage dirs) share nothing; exe_boot.batch_begin/batch_end are no-ops kept for this script. If the runner is killed, the
next batch kills the sessions it left on its instances, by command-line/executable-path filter (own stage dirs only).
--instance-base B: instances B..B+P-1 (disjoint ranges for batches at once).

At most P jobs run at once (instances 1..P are reused). A job that exceeds its timeout is killed by the same filter.
Per job: <out>/<label>.log (session stdout), <label>.json (the session json + batch fields: rc, wall time, instance,
PT rate: gPhysics->mUB4 read every 5 s, read only). <out>/batch.json: the summary incl. whole-box CPU samples.
"""
import argparse
import ctypes
import datetime
import json
import pathlib
import re
import subprocess
import sys
import time
from ctypes import wintypes

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exe_boot as boot  # noqa: E402

ROOT = boot.ROOT
PY = sys.executable
SESSION = HERE / 'game_session.py'
STAGE_RE = re.compile(r'\\build\\boot\\windowed_[orf]_(\d+)(\\|"|\s|$)', re.I)


def processes():
    """[(pid, name, command line, executable path)] of this user's processes (CIM; read only)."""
    script = ('Get-CimInstance Win32_Process | ForEach-Object { "{0}`t{1}`t{2}`t{3}" -f $_.ProcessId,$_.Name,'
              '($_.CommandLine -replace "`t"," "),$_.ExecutablePath }')
    out = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script], capture_output=True,
                         text=True, encoding='utf-8', errors='replace', creationflags=0x08000000).stdout
    rows = []
    for line in out.splitlines():
        parts = line.split('\t')
        if len(parts) == 4 and parts[0].isdigit():
            rows.append((int(parts[0]), parts[1], parts[2], parts[3]))
    return rows


def own_processes(instances=None):
    """Processes of batch sessions, by command line or executable path inside build\\boot\\windowed_?_K (K in
    instances, or any K): game_session.py --instance K --registry-managed-by-batch, exe_boot.exe <stage>,
    EE-AOC.exe staged there, plugin helpers that name the stage. Never anything outside these stage dirs."""
    root = str(ROOT).lower()
    found = []
    for pid, name, cmd, exe in processes():
        k = None
        for text in (cmd, exe):
            m = STAGE_RE.search(text or '')
            if m and root in (text or '').lower():
                k = int(m.group(1))
                break
        if k is None and 'game_session.py' in cmd and '--registry-managed-by-batch' in cmd:
            m = re.search(r'--instance\s+(\d+)', cmd)
            k = int(m.group(1)) if m else None
        if k is not None and (instances is None or k in instances):
            found.append((pid, name, k, cmd))
    return found


def kill(rows, why):
    for pid, name, k, cmd in rows:
        r = subprocess.run(['taskkill.exe', '/PID', str(pid), '/T', '/F'], capture_output=True, text=True)
        print(f'BATCH kill ({why}) instance {k} pid {pid} {name}: {"ok" if r.returncode == 0 else r.stdout.strip() or r.stderr.strip()}',
              flush=True)


def kill_leftovers(pruned):
    """Dead batch leases (an interrupted runner): kill the sessions it left on its instances (batch-managed ones)."""
    for text in pruned:
        m = re.search(r'^json (.*)$', text, re.M)
        instances = set(json.loads(m.group(1)).get('instances', [])) if m else set()
        pid = re.search(r'^pid (\d+)', text, re.M)
        left = [row for row in own_processes(instances) if instances]
        if left:
            kill(left, f'leftover of interrupted batch pid {pid.group(1) if pid else "?"}')
            time.sleep(3)


class Cpu:
    """Whole-box CPU use from GetSystemTimes deltas."""
    def __init__(self):
        self.k = ctypes.WinDLL('kernel32')
        self.last = self.read()

    def read(self):
        t = [wintypes.FILETIME() for _ in range(3)]
        self.k.GetSystemTimes(*[ctypes.byref(x) for x in t])
        idle, kern, user = ((x.dwHighDateTime << 32) | x.dwLowDateTime for x in t)
        return idle, kern + user

    def sample(self):
        now = self.read()
        di, dt = now[0] - self.last[0], now[1] - self.last[1]
        self.last = now
        return round(100.0 * (1 - di / dt), 1) if dt else None


class PtProbe:
    """Read-only PT counter of a running instance: gPhysics->mUB4 (+0xb4, PTs completed; round 36 DETERMINISM.md),
    sampled by ReadProcessMemory (pid from <stage>/ui-state.txt, gPhysics from the original layout or the map)."""
    PHYSICS_VA_ORIGINAL = 0x9318f0

    def __init__(self, stage):
        self.stage, self.pid, self.handle, self.physics, self.rows = stage, None, None, None, []
        self.k = ctypes.WinDLL('kernel32', use_last_error=True)
        self.k.OpenProcess.restype = wintypes.HANDLE

    def _attach(self):
        try:
            text = (self.stage / 'ui-state.txt').read_text()
            pid = int(next(ln.split()[1] for ln in text.splitlines() if ln.startswith('pid ')))
            context = json.loads((self.stage / 'session.context.json').read_text(encoding='utf-8'))
        except (OSError, ValueError, StopIteration):
            return False
        if context.get('map'):
            from fulllink_locator import LinkMap
            self.physics = LinkMap(context['map']).address('?gPhysics@@3PAVEEPhysics@@A')
        else:
            self.physics = self.PHYSICS_VA_ORIGINAL
        self.handle = self.k.OpenProcess(0x0410, False, pid)   # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
        self.pid = pid
        return bool(self.handle)

    def _u32(self, va):
        buf, n = ctypes.c_uint32(), ctypes.c_size_t()
        if not self.k.ReadProcessMemory(self.handle, ctypes.c_void_p(va), ctypes.byref(buf), 4, ctypes.byref(n)):
            return None
        return buf.value

    def sample(self, t):
        if not self.handle and not self._attach():
            return
        phys = self._u32(self.physics)
        pt = self._u32(phys + 0xb4) if phys else None
        if pt is not None:
            self.rows.append((round(t, 1), pt))

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None

    def stats(self):
        rows = [r for r in self.rows if r[1]]
        if len(rows) < 2 or rows[-1][0] <= rows[0][0]:
            return dict(samples=len(rows))
        rates = [(b[1] - a[1]) / (b[0] - a[0]) for a, b in zip(rows, rows[1:]) if b[0] > a[0]]
        return dict(samples=len(rows), first=rows[0], last=rows[-1],
                    pt_per_s=round((rows[-1][1] - rows[0][1]) / (rows[-1][0] - rows[0][0]), 2),
                    pt_per_s_max_window=round(max(rates), 2) if rates else None)


def parse_jobs(a):
    jobs = []
    for spec in a.job:
        image, seed, label = (spec.split(':', 2) + [None, None])[:3]
        jobs.append(dict(image=image, seed=int(seed, 0) if seed else None, label=label, args=[]))
    if a.jobs:
        jobs += json.loads(pathlib.Path(a.jobs).read_text(encoding='utf-8'))
    seen = set()
    for n, job in enumerate(jobs, 1):
        image = job['image']
        if not (image in ('original', 'fulllink') or image.startswith('fulllink=')):
            raise SystemExit(f'job {n}: image must be original, fulllink or fulllink=PATH (got {image!r})')
        job.setdefault('args', [])
        job['label'] = job.get('label') or f'{n:02d}_{image.split("=")[0]}' + (f'_s{job["seed"]}' if job.get('seed') is not None else '')
        if job['label'] in seen or not re.fullmatch(r'[\w.-]+', job['label']):
            raise SystemExit(f'job {n}: label {job["label"]!r} is duplicate or not a plain file name')
        seen.add(job['label'])
    return jobs


def command(job, instance, forward):
    image = job['image']
    cmd = [PY, '-I', str(SESSION), '--instance', str(instance), '--registry-managed-by-batch']
    cmd += ['--original'] if image == 'original' else ['--fulllink'] if image == 'fulllink' else ['--fulllink', image.split('=', 1)[1]]
    if job.get('seed') is not None:
        cmd += ['--seed', str(job['seed'])]
    return cmd + list(forward) + list(job['args'])


def session_timeout_s(args, default):
    """game_session's own --timeout (ms; last one wins) + margin for staging and shutdown."""
    value = None
    for i, x in enumerate(args):
        if x == '--timeout' and i + 1 < len(args):
            value = int(args[i + 1])
        elif x.startswith('--timeout='):
            value = int(x.split('=', 1)[1])
    return value / 1000 + 240 if value else default


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-P', '--parallel', type=int, default=2, help='at most this many sessions at once (default 2)')
    ap.add_argument('--job', action='append', default=[], metavar='IMAGE[:SEED[:LABEL]]')
    ap.add_argument('--jobs', help='JSON list of jobs')
    ap.add_argument('--out', type=pathlib.Path, help='results dir (default build/game_batch/<utc stamp>)')
    ap.add_argument('--job-timeout', type=float, help='kill a job after S s (default: its --timeout + 240 s, else 900 s)')
    ap.add_argument('--stagger', type=float, default=5.0, help='seconds between job starts (default 5)')
    ap.add_argument('--instance-base', type=int, default=1,
                    help='instances are BASE..BASE+P-1 (default 1); separate batches running at once need disjoint ranges')
    ap.add_argument('--recover', action='store_true',
                    help='only prune dead registry leases and restore the snapshot if no live holder is left, then exit')
    a, forward = ap.parse_known_args()
    if a.recover:
        live = boot.shared_recover()
        print(f'registry: live lease holders {[str(x.name) for x in live]}' if live else
              'registry: no live lease holder; the key is restored (or was already)')
        return
    if not 1 <= a.parallel <= 32 or not 1 <= a.instance_base <= 33 - a.parallel:
        ap.error('-P must be 1..32 and BASE..BASE+P-1 within 1..32')
    jobs = parse_jobs(a)
    if not jobs:
        ap.error('no jobs (--job or --jobs)')
    out = a.out or ROOT / 'build/game_batch' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    boot.BOOT.mkdir(parents=True, exist_ok=True)
    instances = list(range(a.instance_base, a.instance_base + a.parallel))
    boot.build_harness()   # once, before any session (sessions would otherwise race to rebuild)
    begin = boot.batch_begin(instances)   # waits while a plain session owns the key
    try:
        kill_leftovers(begin['pruned'])
        stray = own_processes(set(instances))
        if stray:
            raise SystemExit(f'instances {instances} are in use by other sessions: {[(p, n, k) for p, n, k, _ in stray]}; '
                             f'pick another --instance-base')
    except BaseException:
        boot.batch_end(begin)
        raise
    print(f'BATCH {len(jobs)} jobs, P={a.parallel}; '
          f'out {out}', flush=True)
    cpu = Cpu()
    cpu_samples = []
    results = {}
    running = {}   # instance -> (job, Popen, start, deadline, log stream)
    pending = list(jobs)
    t0 = time.monotonic()
    last_cpu = last_start = 0.0
    restored = None
    try:
        while pending or running:
            now = time.monotonic()
            if now - last_cpu >= 5:
                last_cpu = now
                cpu_samples.append(dict(t=round(now - t0, 1), running=len(running), cpu_percent=cpu.sample()))
                for job, *_ in running.values():
                    job['_probe'].sample(now - t0)
            for k, (job, proc, start, deadline, stream) in list(running.items()):
                rc = proc.poll()
                if rc is None and now > deadline:
                    print(f'BATCH {job["label"]}: timeout after {now - start:.0f} s; killing instance {k}', flush=True)
                    proc.kill()
                    kill(own_processes({k}), f'job timeout {job["label"]}')
                    rc = proc.wait(timeout=60)
                    job['timed_out'] = True
                if rc is None:
                    continue
                stream.close()
                del running[k]
                job['_probe'].close()
                results[job['label']] = finish(job, k, rc, now - start, out)
                print(f'BATCH {job["label"]} done: rc {rc} completed {results[job["label"]].get("completed")} '
                      f'{results[job["label"]].get("pt", {}).get("pt_per_s", "?")} PT/s, {now - start:.0f} s', flush=True)
            free = [k for k in instances if k not in running]
            if pending and free and now - last_start >= a.stagger:
                job, k = pending.pop(0), free[0]
                cmd = command(job, k, forward)
                stream = open(out / f'{job["label"]}.log', 'w', encoding='utf-8')
                stream.write('# ' + subprocess.list2cmdline(cmd) + '\n')
                stream.flush()
                proc = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT, cwd=ROOT)
                stage_name = ('windowed_f' if job['image'].startswith('fulllink') else 'windowed_o') + f'_{k}'
                job['_probe'] = PtProbe(boot.BOOT / stage_name)
                limit = a.job_timeout or session_timeout_s(forward + job['args'], 900)
                limit = job.get('timeout', limit)
                running[k] = (job, proc, time.monotonic(), time.monotonic() + limit, stream)
                last_start = time.monotonic()
                print(f'BATCH start {job["label"]} instance {k} pid {proc.pid} (timeout {limit:.0f} s)', flush=True)
            time.sleep(0.5)
    finally:
        if running:
            print(f'BATCH interrupted: killing {len(running)} running job(s)', flush=True)
            for k, (job, proc, start, deadline, stream) in running.items():
                proc.kill()
            kill(own_processes(set(running)), 'batch interrupted')
            for k, (job, proc, start, deadline, stream) in running.items():
                try:
                    proc.wait(timeout=60)
                except subprocess.TimeoutExpired:
                    pass
                stream.close()
                results[job['label']] = dict(label=job['label'], instance=k, interrupted=True)
        try:
            restored = boot.batch_end(begin)
            if restored is True:
                print('BATCH registry restored byte-exact from the shared snapshot', flush=True)
            else:   # other lease holders (another user's --instance sessions or batch) are still running
                print(f'BATCH registry restore deferred to the last live lease holder(s): pids {restored}', flush=True)
                restored = dict(deferred_to_pids=restored)
        except SystemExit as error:
            restored = False
            print(f'BATCH REGISTRY RESTORE FAILED: {error}', flush=True)
        summary = dict(test_only=True, parallel=a.parallel, forwarded_args=forward, wall_s=round(time.monotonic() - t0, 1),
                       registry_restored=restored, registry_key_existed=begin['existed'],
                       cpu_samples=cpu_samples,
                       cpu_mean_percent=round(sum(s['cpu_percent'] or 0 for s in cpu_samples) / len(cpu_samples), 1) if cpu_samples else None,
                       jobs=[results.get(j['label'], dict(label=j['label'], started=False)) for j in jobs])
        (out / 'batch.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
        print(f'BATCH summary {out / "batch.json"}', flush=True)
    bad = [r['label'] for r in summary['jobs'] if not r.get('completed')]
    sys.exit(1 if bad or restored is False else 0)


def finish(job, k, rc, wall, out):
    name = ('windowed_f' if job['image'].startswith('fulllink') else 'windowed_o') + f'_{k}'
    session_json = boot.BOOT / f'{name}.session.json'
    log = (out / f'{job["label"]}.log').read_text(encoding='utf-8', errors='replace')
    record = dict(label=job['label'], image=job['image'], seed=job.get('seed'), args=job['args'], instance=k, stage=name,
                  rc=rc, wall_s=round(wall, 1), timed_out=job.get('timed_out', False), pt=job['_probe'].stats(),
                  pt_samples=job['_probe'].rows)
    m = re.search(r'^EVIDENCE (.+)$', log, re.M)
    if m and session_json.exists():
        data = json.loads(session_json.read_text(encoding='utf-8'))
        if data.get('evidence') == m.group(1).strip():   # this job's own json, not a previous job's
            record.update(completed=data.get('completed'), evidence=data.get('evidence'), stops=data.get('stops'))
            (out / f'{job["label"]}.json').write_text(json.dumps(dict(data, batch=record), indent=2) + '\n', encoding='utf-8')
            return record
    record['completed'] = False
    (out / f'{job["label"]}.json').write_text(json.dumps(dict(batch=record), indent=2) + '\n', encoding='utf-8')
    return record


if __name__ == '__main__':
    main()
