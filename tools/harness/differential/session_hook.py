"""Round 39 differential harness (TEST ONLY): game_session.py plugin that runs the per-PT sampler next to a session.

Options (all opt-in; without --diff-stream nothing happens):
  --diff-stream PATH|auto   stream output (auto: <evidence>/diff.stream.gz); snapshots beside it (*.snap.jsonl.gz)
  --diff-mode sync|poll     sync (default): the harness stops the game at each PT end (exe_boot --pt-sync) and every
                            PT is hashed; poll: no debugger help (misses PTs at speed)
  --diff-hasher c|python    sync: c (default) = the harness hashes each PT in C (ptsync.h --pt-hash; the stream is
                            plain text, written by exe_boot) and sampler.py only takes the full snapshots;
                            python = sampler.py hashes every PT (slower; streams of the two hashers do not compare)
  --diff-snapshot-every N   full snapshot every N PT (default 100; 0 = none)
  --diff-sync-every N       C hasher stride: hash every Nth PT only (each PT still stops briefly at the guard to
                            count it); compare uses the common PTs
  --diff-full-at A,B        full snapshots at these PTs (dump mode)
  --diff-stop-after X       end the session after PT X (dump mode; the sampler writes <stage>/stop.request)
  --diff-ai-every N         C hasher: the ai column (computer-player objects, ai_layout.py) every Nth PT only
                            (default 1; '=' in the other lines; snapshot PTs always)
"""
import json, pathlib, subprocess, sys, time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))


def add_session_arguments(ap):
    g = ap.add_argument_group('differential sampler (TEST ONLY, round 39)')
    g.add_argument('--diff-stream', help='per-PT hash stream path, or "auto" (<evidence>/diff.stream.gz)')
    g.add_argument('--diff-mode', choices=['sync', 'poll'], default='sync')
    g.add_argument('--diff-hasher', choices=['c', 'python'], default='c',
                   help='sync mode: c = the harness hashes each PT (ptsync.h --pt-hash; fast), python = sampler.py does')
    g.add_argument('--diff-snapshot-every', type=int, default=100)
    g.add_argument('--diff-sync-every', type=int, default=1,
                   help='C hasher: hash only every Nth PT (every PT is still counted at the guard; stream has gaps)')
    g.add_argument('--diff-full-at', default='')
    g.add_argument('--diff-stop-after', type=int)
    g.add_argument('--diff-ai-every', type=int, default=1,
                   help='C hasher: hash the AI objects (ai column) every Nth PT only')


def _out(ctx):
    a = ctx['args']
    out = (pathlib.Path(ctx['evidence']) / 'diff.stream.gz') if a.diff_stream == 'auto' else pathlib.Path(a.diff_stream).resolve()
    if a.diff_mode == 'sync' and a.diff_hasher == 'c' and out.suffix == '.gz':   # exe_boot writes plain text
        out = out.with_suffix('.txt')
    return out


def prepare_session(ctx):
    a = ctx['args']
    if not getattr(a, 'diff_stream', None):
        return None
    import layout as L
    stage = pathlib.Path(ctx['stage'])
    addr = L.resolve(ctx['image'] if ctx['map'] else 'original', ctx['map'], stage / 'EE-AOC.exe')
    rec = dict(stream=str(_out(ctx)), mode=a.diff_mode, pt_sync_va='%x' % addr['pt_sync'], sync_name=ctx['name'])
    if a.diff_mode != 'sync':
        return rec
    rec['harness_extra'] = ['--pt-sync', '%x' % (addr['pt_sync'] - 0x400000), ctx['name']]
    if a.diff_hasher == 'c':
        import sampler as S
        human = bool(getattr(a, 'human_script', None))   # opt-in mask of the human's sound-throttle stamps (layout.py)
        # the same opt-in mask without a scripted human (EE_DIFF_ACK_MASK=1): computer players' commands also run
        # EEPlayer::PlayAcknowledgement 0x567f10, whose TMGetTime() stamps only gate the sound (final campaign, cov)
        human = human or __import__('os').environ.get('EE_DIFF_ACK_MASK') == '1'
        human = human or bool(getattr(a, 'mp_human_ui_mask', False))   # every end of an MP game with a scripted human
        mp = bool(getattr(a, 'mp_role', None))   # opt-in (round 40): MP mask of per-process effect handles (layout.py)
        # opt-in (fixes branch): leave out the unit fields the fixes branch zero-initialises (layout.ZEROFILL_FIELDS)
        zerofill = __import__('os').environ.get('EE_DIFF_ZEROFILL_MASK') == '1'
        header = dict(tool='tools/boot/differential/ptsync.h', version=10 if mp else 8 if human or zerofill else 7, hasher='c64', image=ctx['image'],
                      map=str(ctx['map']) if ctx['map'] else None, mode='sync', sync_name=ctx['name'],
                      snapshot_every=a.diff_snapshot_every, layout=S.layout_digest(),
                      addresses={k: ('%x' % v if v else None) for k, v in addr.items()},
                      columns='pt world render rng maprng nunits inputs ai player0..n-1', ai_every=a.diff_ai_every,
                      ai_layout=__import__('ai_layout').digest(), started=time.time())
        if human:
            header['ui_extra'] = L.enable_human_ui_mask()
        if zerofill:
            header['ui_extra'] = L.enable_zerofill_mask()
        if mp:
            header['unit_neg1'] = L.enable_mp_mask()
        cfg = stage / 'ptsync.cfg'
        out = _out(ctx); out.parent.mkdir(parents=True, exist_ok=True)
        header['hash_every'] = a.diff_sync_every
        cfg.write_text(S.hash_config(addr, out, header, a.diff_snapshot_every, a.diff_full_at, a.diff_stop_after,
                                     a.diff_sync_every, a.diff_ai_every),
                       encoding='ascii')
        rec['harness_extra'] += ['--pt-hash', str(cfg)]
        rec['hasher'] = 'c64'
    rec['interventions'] = [dict(kind='pt-sync', test_only=True, read_only=True,
                                 detail='harness guard at the end of each physics tick (rva %x); the game waits while '
                                        'tools/boot/differential/sampler.py reads memory' % (addr['pt_sync'] - 0x400000))]
    return rec


class _Run:
    def __init__(self, cmd, log):
        self.log = open(log, 'w', encoding='utf-8')
        self.cmd = cmd
        self.p = subprocess.Popen(cmd, stdout=self.log, stderr=subprocess.STDOUT, creationflags=0x08000000)

    def stop(self):
        try:
            self.p.wait(timeout=120)
        except subprocess.TimeoutExpired:
            self.p.kill(); self.p.wait()
        self.log.close()
        return dict(command=self.cmd, exit=self.p.returncode, log=self.log.name)


def sampler_command(ctx):
    a = ctx['args']
    cmd = [sys.executable, '-I', str(HERE / 'sampler.py'), '--stage', str(ctx['stage']), '--out', str(_out(ctx)),
           '--mode', 'csync' if a.diff_mode == 'sync' and a.diff_hasher == 'c' else a.diff_mode, '--sync-name', ctx['name'], '--snapshot-every', str(a.diff_snapshot_every),
           '--exe', str(pathlib.Path(ctx['stage']) / 'EE-AOC.exe')]
    if ctx['map']:
        cmd += ['--map', str(ctx['map'])]
    if a.diff_full_at:
        cmd += ['--full-at', a.diff_full_at]
    if a.diff_stop_after is not None:
        cmd += ['--stop-after', str(a.diff_stop_after)]
    return cmd


def start_session(ctx):
    if not getattr(ctx['args'], 'diff_stream', None):
        return None
    out = _out(ctx)
    out.parent.mkdir(parents=True, exist_ok=True)
    return _Run(sampler_command(ctx), str(out) + '.sampler.log')


def finish_session(ctx, lines):
    if not getattr(ctx['args'], 'diff_stream', None):
        return None
    sync = [l for l in lines if l.startswith('TEST pt sync')]
    rec = dict(pt_sync_lines=sync[-3:])
    out = _out(ctx)
    if out.suffix == '.txt' and out.exists():   # the C hasher's plain-text stream -> .txt.gz (~5x smaller)
        import gzip, shutil
        gz = out.with_name(out.name + '.gz')
        with open(out, 'rb') as src, gzip.open(gz, 'wb', compresslevel=6) as dst:
            shutil.copyfileobj(src, dst)
        rec.update(stream=str(gz), stream_bytes=out.stat().st_size, stream_gz_bytes=gz.stat().st_size)
        out.unlink()
        key = out.with_name(out.name + '.key')
        if key.exists():
            key.unlink()
    return rec
