"""Round 39 differential harness (TEST ONLY): per-PT world-state hash stream for a staged EE-AOC session.

Read-only (ReadProcessMemory). Two modes:
  sync (default): the harness debugger (exe_boot.c `--pt-sync RVA NAME`, tools/boot/differential/ptsync.h) stops the
        game at the START of EVERY physics tick (the instruction after EEPhysics::Virtual20's mUB8--: the state after
        the previous PT and the CT-boundary processing, plus this PT's clock/RNG pre-step) and waits while it is
        read. Every PT is sampled and the result does not depend on wall-time rates (the game simply waits).
        Key = mUB4 (PTs completed) while it advances; after the engine end (mBE6: mUB4 freezes, PTs go on) the
        previous key + 1. With --pt-hash (session_hook default) the C code in ptsync.h hashes each PT and this
        process (mode csync) only takes the full snapshots.
  poll: no debugger help; polls and keeps a sample only when the physics thread is between PTs and nothing moved
        during the read (round-36 method). Misses PTs at high speed; the stream records only what it caught.

Stream (text, one line per sampled PT; gzip when the name ends in .gz):
  first line  '#' + JSON header (image, addresses, columns, layout digest)
  then        '<PT> <world> <render> <rng> <maprng> <nunits> <inputs> <player hashes ...>'   (hex; player hash per
              world player index 0..n-1 as 8 hex digits; world = SIM hash over everything incl. units without a
              player, without maprng; render = hash of the render-written unit fields only (layout.RENDER_FIELDS), never in world)
Snapshots (every --snapshot-every PT, default 100, and at every --full-at PT): JSON lines (gzip) with the physics
  block, per-player raw bytes + stats, per-unit raw object bytes (0x364 complex / 0x48 plain), goal bytes, class id.

Command (normally started by session_hook.py next to game_session.py):
  python -I tools/boot/differential/sampler.py --stage build/boot/windowed_o [--map build/exe_fulllink/EE-AOC.map]
         --out <evidence>/diff.stream.gz [--sync-name NAME] [--snapshot-every 100] [--full-at 976,977] [--stop-after 977]
"""
import argparse, ctypes, gzip, hashlib, json, os, pathlib, struct, sys, time
from ctypes import wintypes

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import layout as L
import ai_layout as AI

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]; k32.OpenProcess.restype = wintypes.HANDLE
k32.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                  ctypes.POINTER(ctypes.c_size_t)]
k32.ReadProcessMemory.restype = wintypes.BOOL
k32.OpenEventW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]; k32.OpenEventW.restype = wintypes.HANDLE
k32.SetEvent.argtypes = [wintypes.HANDLE]; k32.ResetEvent.argtypes = [wintypes.HANDLE]
k32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]; k32.WaitForSingleObject.restype = wintypes.DWORD
k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
k32.CloseHandle.argtypes = [wintypes.HANDLE]
k32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
psapi = ctypes.WinDLL('psapi')
psapi.EnumProcesses.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]

CHUNK = 0x1000


class Proc:
    """ReadProcessMemory with a per-sample page cache (units come from memory pools: neighbours share pages)."""
    def __init__(self, pid):
        self.pid = pid
        self.h = k32.OpenProcess(0x1410, False, pid)   # VM_READ | QUERY_INFORMATION | SYNCHRONIZE... (0x1000 limited)
        if not self.h:
            raise ctypes.WinError(ctypes.get_last_error())
        self.cache = {}
        self.buf = ctypes.create_string_buffer(0x10000)
        self.got = ctypes.c_size_t()
        self.reads = 0

    def alive(self):
        code = wintypes.DWORD()
        return k32.GetExitCodeProcess(self.h, ctypes.byref(code)) and code.value == 259

    def raw(self, va, n):
        if n > len(self.buf):
            self.buf = ctypes.create_string_buffer(n)
        self.reads += 1
        if not k32.ReadProcessMemory(self.h, va, self.buf, n, ctypes.byref(self.got)) or self.got.value != n:
            raise OSError('read failed at %x+%x' % (va, n))
        return ctypes.string_at(self.buf, n)

    def page(self, p):
        b = self.cache.get(p)
        if b is None:
            b = self.cache[p] = self.raw(p, CHUNK)
        return b

    def read(self, va, n):
        p = va & ~(CHUNK - 1)
        if va + n <= p + CHUNK:
            return self.page(p)[va - p:va - p + n]
        out = bytearray()
        while n:
            p = va & ~(CHUNK - 1); k = min(n, p + CHUNK - va)
            out += self.page(p)[va - p:va - p + k]; va += k; n -= k
        return bytes(out)

    def u32(self, va):
        return struct.unpack('<I', self.read(va, 4))[0]

    def new_sample(self):
        self.cache.clear()


def find_pid(stage):
    """The EE-AOC.exe process whose image lives in the stage dir (parallel instances: one per stage)."""
    want = str((pathlib.Path(stage) / 'EE-AOC.exe').resolve()).lower()
    arr = (wintypes.DWORD * 4096)(); got = wintypes.DWORD()
    psapi.EnumProcesses(arr, ctypes.sizeof(arr), ctypes.byref(got))
    for pid in arr[:got.value // 4]:
        h = k32.OpenProcess(0x1000, False, pid)
        if not h:
            continue
        name = ctypes.create_unicode_buffer(1024); size = wintypes.DWORD(1024)
        ok = k32.QueryFullProcessImageNameW(h, 0, name, ctypes.byref(size))
        k32.CloseHandle(h)
        if ok and name.value.lower() == want:
            return pid
    return None


def h64(*parts):
    h = hashlib.blake2b(digest_size=8)
    for p in parts:
        h.update(p)
    return h


class Sampler:
    def __init__(self, proc, addr):
        self.p, self.a = proc, addr
        self.utab, self.ptab = L.unit_table(), L.player_table()
        self.umask = L.mask_for(self.utab, L.COMPLEX_SIZE)
        self.emask = L.mask_for(self.utab, L.EEUNIT_SIZE)
        self.pmask = L.mask_for(self.ptab, L.PLAYER_SIZE)
        self.urmask = L.mask_for(self.utab, L.COMPLEX_SIZE, render=True)
        self.ermask = L.mask_for(self.utab, L.EEUNIT_SIZE, render=True)
        self.refs = [next(r[0] for r in self.utab if r[2] == n) for n in L.UNIT_REFS]
        self.class_ids = {}
        self.digest = layout_digest()

    def unit_id(self, ptr):
        if not ptr:
            return 0xffffffff
        try:
            return self.p.u32(ptr + 0x24)
        except OSError:
            return 0xfffffffe

    def class_id(self, ptr):
        v = self.class_ids.get(ptr)
        if v is None:
            v = self.class_ids[ptr] = self.p.u32(ptr + 0x28) if ptr else 0xffffffff
        return v

    def stats(self, player):
        """31 statistics series: (count, last value, last time, max, flag) each."""
        out = []
        for i in range(31):
            s = player + 0x4a8 + 0x18 + 0x18 * i
            _, first, last, _, mx, flag = struct.unpack('<IIIIIB', self.p.read(s, 0x15))
            n = (last - first) // 8 if last >= first else -1
            lv = struct.unpack('<II', self.p.read(last - 8, 8)) if n > 0 else (0, 0)
            out.append((n, lv[0], lv[1], mx, flag))
        return out

    def sample(self, full=False):
        p, a = self.p, self.a
        p.new_sample()
        phys = p.u32(a['physics'])
        pb = p.read(phys, L.PHYSICS_SIZE)
        pt = struct.unpack_from('<I', pb, 0xb4)[0]
        world = a['world']
        wb = p.read(world, 0xd8)
        nplayers = struct.unpack_from('<I', wb, L.WORLD_NPLAYERS)[0]
        players = list(struct.unpack_from('<17I', wb, L.WORLD_PLAYERS))[:min(nplayers, 17)]
        mp = struct.unpack_from('<I', wb, L.WORLD_MAP)[0]
        maprng = p.u32(mp + L.MAP_RANDOM) if mp else 0
        inputs = p.u32(a['inputs'])
        rng = struct.unpack_from('<I', pb, 0x18)[0]
        index = {ptr: i for i, ptr in enumerate(players) if ptr}
        ph = [h64() for _ in players]
        snap = dict(pt=pt, physics=pb.hex(), maprng=maprng, inputs=inputs, players=[], units=[], ai=[]) if full else None
        for i, pl in enumerate(players):
            if not pl:
                continue
            b = p.read(pl, L.PLAYER_SIZE)
            masked = (int.from_bytes(b, 'little') & self.pmask).to_bytes(L.PLAYER_SIZE, 'little')
            st = self.stats(pl)
            civ = p.read(pl + 0x468 + 0x34, 8)
            ph[i].update(masked); ph[i].update(repr(st).encode()); ph[i].update(civ)
            if full:
                snap['players'].append(dict(index=i, slot=b[0x464], raw=b.hex(), stats=st, epoch=civ.hex()))
                if a.get('vt_computer_player') and struct.unpack_from('<I', b, 0)[0] == a['vt_computer_player']:
                    w = AI.Walker(p, players)   # the AI objects of this computer player (records by object path)
                    w.walk(pl, 'EEComputerPlayer', 'P%d' % i, snap['ai'])
        gaia = h64(); render = h64()
        head = world + L.WORLD_UNITS
        u = p.u32(world + L.WORLD_UNITS + 4)
        n = 0
        vtc, vtu = a['vt_complex'], a['vt_unit']
        while u and u != head and n < 200000:
            hdr = p.read(u, 0x2c)
            vt, link, player = struct.unpack_from('<I4xI12xI', hdr, 0)
            complex_ = vt == vtc
            size = L.COMPLEX_SIZE if complex_ else L.EEUNIT_SIZE
            b = p.read(u, size)
            hb = L.unit_zero_if(b) if complex_ else b   # v6: mU118 not hashed while mF214 is unset
            bi = int.from_bytes(hb, 'little')
            masked = (bi & (self.umask if complex_ else self.emask)).to_bytes(size, 'little')
            render.update(b[0x24:0x28]); render.update((bi & (self.urmask if complex_ else self.ermask)).to_bytes(size, 'little'))
            cls = struct.unpack_from('<I', b, 0x2c)[0]
            extra = [self.class_id(cls), 0 if complex_ else (1 if vt == vtu else 2)]
            goal = b''
            if complex_:
                extra += [self.unit_id(struct.unpack_from('<I', b, o)[0]) for o in self.refs]
                g = struct.unpack_from('<I', b, 0x1f0)[0]
                if g:
                    goal = p.read(g, L.GOAL_SIZE)
                    extra += [goal[5], struct.unpack_from('<I', goal, 0x14)[0]]
            rec = masked + struct.pack('<%dI' % len(extra), *extra)
            (ph[index[player]] if player in index else gaia).update(rec)
            if full:
                snap['units'].append(dict(id=struct.unpack_from('<I', b, 0x24)[0], player=index.get(player, -1),
                                          kind='complex' if complex_ else 'unit' if vt == vtu else 'other:%x' % vt,
                                          cls=extra[0], raw=b.hex(), goal=goal.hex(), refs=extra[2:2 + len(self.refs)]))
            n += 1
            if not link:
                break
            u = p.u32(link)
        phys_fields = b''.join(pb[o:o + 4] for o, _, _ in L.PHYSICS_FIELDS)
        pdig = [x.digest() for x in ph]
        # EEMap::mRandom is excluded from the world hash: seeded per run outside the pins (differs between two runs of
        # the original from PT 1) and never drawn during play (constant over a 4,589-PT game); it has its own column
        # gU531CF0 (inputs processed) is not in the world hash either: the civ-timer messages EEMCivTimerExpired go
        # through the local-input path and execute at a CT-batching (wall-time) dependent PT (setup, 2026-11-01)
        world_h = h64(phys_fields, struct.pack('<I', n), gaia.digest(), *pdig).hexdigest()
        line = '%d %s %s %08x %08x %d %d %s' % (pt, world_h, render.hexdigest()[:8], rng, maprng, n, inputs,
                                                ' '.join(d.hex()[:8] for d in pdig))
        return pt, line, snap


def hash_config(addr, stream, header, snapshot_every=100, full_at='', stop_after=None, hash_every=1, ai_every=1):
    """Text config for the harness C hasher (ptsync.h --pt-hash): same addresses and masks as Sampler."""
    utab, ptab = L.unit_table(), L.player_table()
    hx = lambda m, n: m.to_bytes(n, 'little').hex()
    rows = (AI.config_lines(addr['vt_computer_player'], ai_every) if addr.get('vt_computer_player') else []) + ['physics %x' % addr['physics'], 'world %x' % addr['world'], 'inputs %x' % addr['inputs'],
            'vt_complex %x' % addr['vt_complex'], 'vt_unit %x' % addr['vt_unit'],
            'umask ' + hx(L.mask_for(utab, L.COMPLEX_SIZE), L.COMPLEX_SIZE),
            'urmask ' + hx(L.mask_for(utab, L.COMPLEX_SIZE, render=True), L.COMPLEX_SIZE),
            'emask ' + hx(L.mask_for(utab, L.EEUNIT_SIZE), L.EEUNIT_SIZE),
            'ermask ' + hx(L.mask_for(utab, L.EEUNIT_SIZE, render=True), L.EEUNIT_SIZE),
            'pmask ' + hx(L.mask_for(ptab, L.PLAYER_SIZE), L.PLAYER_SIZE),
            'refs ' + ' '.join('%x' % next(r[0] for r in utab if r[2] == n) for n in L.UNIT_REFS),
            'physics_fields ' + ' '.join('%x' % o for o, _, _ in L.PHYSICS_FIELDS),
            *['uzero_if %x %x %x %x' % z[:4] for z in L.UNIT_ZERO_IF],
            *['uneg1 %x' % off for off, _ in L.UNIT_NEG1],   # opt-in (round 40 mp); none by default
            'snapshot_every %d' % snapshot_every, 'header ' + json.dumps(header), 'stream ' + str(stream),
            'keyfile ' + str(stream) + '.key']
    if hash_every > 1:
        rows.append('hash_every %d' % hash_every)
    if full_at:
        rows.append('full_at ' + full_at)
    if stop_after is not None:
        rows.append('stop_after %d' % stop_after)
    return '\n'.join(rows) + '\n'


def layout_digest():
    # sets in sorted order: repr(set) follows the per-process string hash seed, so the digest used to flip between
    # runs of the same code (a702f75bb49e / 79e97611a208, dbg3 2026-11-01)
    return hashlib.sha1(repr((L.unit_table(), L.player_table(), sorted(L.NOISE), sorted(L.UI_FIELDS), L.RENDER_FIELDS,
                              L.UNIT_REFS, L.PHYSICS_FIELDS, L.UNIT_ZERO_IF, AI.digest())).encode()).hexdigest()[:12]


def open_events(name):
    acc = 0x00100002   # SYNCHRONIZE | EVENT_MODIFY_STATE
    ev = [k32.OpenEventW(acc, False, 'Local\\ee_ptsync_%s_%s' % (name, k)) for k in ('req', 'ack', 'on')]
    return ev if all(ev) else None


def opener(path):
    path = pathlib.Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    return gzip.open(path, 'wt', encoding='ascii', compresslevel=6) if path.suffix == '.gz' else open(path, 'w', encoding='ascii')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--stage', help='stage dir (build/boot/<name>); the game is the EE-AOC.exe running from it')
    ap.add_argument('--pid', type=int)
    ap.add_argument('--map', help='full-link LINK map (addresses by name); omit for the original')
    ap.add_argument('--exe', help='image file for the pt-sync rva (default: <map>.exe or the frozen original)')
    ap.add_argument('--out', help='stream path (.gz to compress); snapshots go to <out minus .gz/.txt>.snap.jsonl.gz')
    ap.add_argument('--mode', choices=['sync', 'csync', 'poll'], default='sync',
                    help='csync: the harness (ptsync.h --pt-hash) writes the stream; this process only takes the full '
                         'snapshots it is woken for and handles --stop-after')
    ap.add_argument('--sync-name', help='event name shared with exe_boot --pt-sync (default: the stage dir name)')
    ap.add_argument('--snapshot-every', type=int, default=100)
    ap.add_argument('--full-at', default='', help='comma list of PTs with a full snapshot (dump mode)')
    ap.add_argument('--stop-after', type=int, help='after this PT: write <stage>/stop.request and exit (dump mode)')
    ap.add_argument('--interval', type=float, default=0.001, help='poll mode sleep')
    ap.add_argument('--wait', type=float, default=1800, help='seconds to wait for the game process / events')
    ap.add_argument('--max-seconds', type=float, help='stop sampling after this many wall seconds (testing)')
    ap.add_argument('--harness-args', action='store_true', help='print the exe_boot args for this image and exit')
    a = ap.parse_args()
    addr = L.resolve('fulllink' if a.map else 'original', a.map, a.exe)
    name = a.sync_name or (pathlib.Path(a.stage).name if a.stage else 'default')
    if a.harness_args:
        print(json.dumps(['--pt-sync', '%x' % (addr['pt_sync'] - 0x400000), name]))
        return
    if not a.out:
        ap.error('--out is required')
    full_at = {int(x) for x in a.full_at.split(',') if x}
    out = pathlib.Path(a.out)
    snap_path = out.with_name(out.name.removesuffix('.gz').removesuffix('.txt') + '.snap.jsonl.gz')
    deadline = time.monotonic() + a.wait
    events = None
    if a.mode in ('sync', 'csync'):
        while not (events := open_events(name)):
            if time.monotonic() > deadline:
                sys.exit('sampler: no pt-sync events %s (harness not started with --pt-sync?)' % name)
            time.sleep(0.05)
    pid = a.pid
    while pid is None:
        pid = find_pid(a.stage) if a.stage else None
        if pid is None:
            if time.monotonic() > deadline:
                if events: k32.ResetEvent(events[2])
                sys.exit('sampler: no game process')
            time.sleep(0.1)
    proc = Proc(pid)
    if events:
        k32.SetEvent(events[2])   # attach (the process exists long before its first PT)
        print('attached to pt sync', name, flush=True)
    s = Sampler(proc, addr)
    header = dict(tool='tools/boot/differential/sampler.py', version=6, image='fulllink' if a.map else 'original',
                  map=a.map, pid=pid, mode=a.mode, sync_name=name, snapshot_every=a.snapshot_every,
                  addresses={k: ('%x' % v if v else None) for k, v in addr.items()}, layout=s.digest,
                  columns='pt world render rng maprng nunits inputs player0..n-1', started=time.time())
    stream = None if a.mode == 'csync' else opener(out)
    snaps = gzip.open(snap_path, 'wt', encoding='ascii', compresslevel=6)
    if stream:
        stream.write('#' + json.dumps(header) + '\n')
    print('sampling pid', pid, 'mode', a.mode, '->', out, flush=True)
    nsamples = nfail = 0; last_pt = None; gaps = 0; t_sample = 0.0; t0 = time.perf_counter(); seen = set()
    last_ub4 = last_key = None
    keyfile = pathlib.Path(str(out) + '.key')
    try:
        while True:
            if a.max_seconds and time.perf_counter() - t0 > a.max_seconds:
                break
            if a.mode in ('sync', 'csync'):
                r = k32.WaitForSingleObject(events[0], 500)
                if r != 0:
                    if not proc.alive():
                        break
                    continue
                try:
                    ts = time.perf_counter()
                    phys = proc.u32(addr['physics'])
                    ub4 = proc.u32(phys + 0xb4)
                    if a.mode == 'csync':   # the C hasher wrote this PT's key before waking us
                        pt = int(keyfile.read_text().split()[0])
                    else:   # key rule (as ptsync.h): mUB4 while it advances, else previous key + 1 (engine end)
                        pt = last_key + 1 if last_ub4 is not None and ub4 == last_ub4 else ub4
                        last_ub4 = ub4
                    last_key = pt
                    full = a.mode == 'csync' or (a.snapshot_every and pt % a.snapshot_every == 0) or pt in full_at
                    _, line, snap = s.sample(full)
                    line = '%d %s' % (pt, line.split(' ', 1)[1])
                    if snap:
                        snap['pt'], snap['ub4'] = pt, ub4
                    t_sample += time.perf_counter() - ts
                except OSError as e:
                    nfail += 1; line = snap = None
                    print('sample failed:', e, flush=True)
                k32.SetEvent(events[1])
            else:
                if not proc.alive():
                    break
                line = snap = None
                try:
                    phys = proc.u32(addr['physics'])
                    t0_, d0, b0 = struct.unpack('<I48xII', proc.raw(phys + 0x80, 0x3c))[:3]
                    if d0 + b0 == t0_ and d0 not in seen:
                        full = (a.snapshot_every and d0 % a.snapshot_every == 0) or d0 in full_at
                        ts = time.perf_counter()
                        pt, line, snap = s.sample(full)
                        t_sample += time.perf_counter() - ts
                        t1_, d1, b1 = struct.unpack('<I48xII', proc.raw(phys + 0x80, 0x3c))[:3]
                        if (t1_, b1, d1) != (t0_, b0, d0) or pt != d0:
                            line = snap = None
                except OSError:
                    nfail += 1
                time.sleep(a.interval)
            if line is None:
                continue
            if pt in seen:
                continue
            seen.add(pt) if a.mode == 'poll' else None
            if last_pt is not None and pt != last_pt + 1 and a.mode != 'csync':
                gaps += 1
            last_pt = pt
            if stream:
                stream.write(line + '\n')
            nsamples += 1
            if snap:
                snaps.write(json.dumps(snap) + '\n')
            if nsamples % 2000 == 0 and stream:
                stream.flush()
                print('pt %d samples %d gaps %d mean %.2f ms/sample reads %d' % (pt, nsamples, gaps,
                      1000 * t_sample / nsamples, proc.reads), flush=True)
            if a.stop_after is not None and pt >= a.stop_after:
                if a.stage:
                    (pathlib.Path(a.stage) / 'stop.request').write_text('sampler --stop-after %d\n' % pt)
                print('stop after PT', pt, flush=True)
                break
    finally:
        if events:
            k32.ResetEvent(events[2])
            k32.SetEvent(events[1])   # never leave the debugger waiting
        trailer = dict(samples=nsamples, gaps=gaps, failures=nfail, last_pt=last_pt, wall=time.perf_counter() - t0,
                       sample_ms=1000 * t_sample / max(nsamples, 1))
        if stream:
            stream.write('#end ' + json.dumps(trailer) + '\n')
            stream.close()
        snaps.close()
        print('done', json.dumps(trailer), flush=True)


if __name__ == '__main__':
    main()
