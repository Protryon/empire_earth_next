"""TEST ONLY (Windows): a sampling profiler for EE-SIM.exe (tools/sim.py's runs): where a headless game spends its time.

It starts EE-SIM.exe in a staged data root as tools/sim.py does, then, every 1/--hz seconds, stops each thread that used
CPU since the last sample, walks its stack (dbghelp's StackWalk64 over the image's unwind tables) and lets it go on.
The addresses are named from the exe's .map (build.py writes one beside the exe; the image is /FIXED at 0x400000), the
system DLLs' from their exports. The report (--out, default build/sim/profile/<time>/profile.txt):
  - per thread its CPU seconds and samples;
  - per function the self samples (the top frame) and the inclusive ones (anywhere on the stack, once a sample),
    for the busiest thread (the physics thread: the game and the computer players run on it) or --thread all;
  - --window GAME_S_FROM-GAME_S_TO keeps only the samples taken while the game time was in that range (from EE-SIM's
    "game s" progress lines on stderr), e.g. the late game;
  - stacks.txt: the collapsed stacks ("a;b;c count", flamegraph.pl / speedscope input).

  python -I tools/sim_profile.py [--data binary] [--exe EXE] [--hz 250] -- --seed 25 --engine boom,ageofai \
      --game-seconds 7200 --cheats none --personality balanced --report r.json

Stopping a thread does not change what it computes: the game's results are the same with or without the profiler
(it only runs slower). On Linux use perf (perf record -g -- ee-aoc --sim ...) instead.
"""
import argparse
import collections
import ctypes
import ctypes.wintypes as wt
import os
import pathlib
import re
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sim  # noqa: E402

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
dbghelp = ctypes.WinDLL("dbghelp", use_last_error=True)

THREAD_ACCESS = 0x0002 | 0x0008 | 0x0010 | 0x0040   # suspend/resume, get context, query information
TH32CS_SNAPTHREAD = 0x4
CONTEXT_FULL = 0x10000B
IMAGE_FILE_MACHINE_AMD64 = 0x8664
SYMOPT_DEFERRED_LOADS, SYMOPT_UNDNAME = 0x4, 0x2


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ThreadID", wt.DWORD),
                ("th32OwnerProcessID", wt.DWORD), ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG), ("dwFlags", wt.DWORD)]


class ADDRESS64(ctypes.Structure):
    _fields_ = [("Offset", ctypes.c_uint64), ("Segment", wt.WORD), ("Mode", ctypes.c_int)]


class STACKFRAME64(ctypes.Structure):
    _fields_ = [("AddrPC", ADDRESS64), ("AddrReturn", ADDRESS64), ("AddrFrame", ADDRESS64), ("AddrStack", ADDRESS64),
                ("AddrBStore", ADDRESS64), ("FuncTableEntry", ctypes.c_void_p), ("Params", ctypes.c_uint64 * 4),
                ("Far", wt.BOOL), ("Virtual", wt.BOOL), ("Reserved", ctypes.c_uint64 * 3), ("KdHelp", ctypes.c_byte * 512)]


class SYMBOL_INFO(ctypes.Structure):
    _fields_ = [("SizeOfStruct", wt.ULONG), ("TypeIndex", wt.ULONG), ("Reserved", ctypes.c_uint64 * 2),
                ("Index", wt.ULONG), ("Size", wt.ULONG), ("ModBase", ctypes.c_uint64), ("Flags", wt.ULONG),
                ("Value", ctypes.c_uint64), ("Address", ctypes.c_uint64), ("Register", wt.ULONG), ("Scope", wt.ULONG),
                ("Tag", wt.ULONG), ("NameLen", wt.ULONG), ("MaxNameLen", wt.ULONG), ("Name", ctypes.c_char * 512)]


PFUNC = ctypes.WINFUNCTYPE(ctypes.c_void_p, wt.HANDLE, ctypes.c_uint64)
PBASE = ctypes.WINFUNCTYPE(ctypes.c_uint64, wt.HANDLE, ctypes.c_uint64)
dbghelp.SymFunctionTableAccess64.restype = ctypes.c_void_p
dbghelp.SymFunctionTableAccess64.argtypes = [wt.HANDLE, ctypes.c_uint64]
dbghelp.SymGetModuleBase64.restype = ctypes.c_uint64
dbghelp.SymGetModuleBase64.argtypes = [wt.HANDLE, ctypes.c_uint64]
dbghelp.StackWalk64.argtypes = [wt.DWORD, wt.HANDLE, wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
dbghelp.SymInitialize.argtypes = [wt.HANDLE, ctypes.c_char_p, wt.BOOL]
dbghelp.SymFromAddr.argtypes = [wt.HANDLE, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64), ctypes.c_void_p]
dbghelp.SymRefreshModuleList.argtypes = [wt.HANDLE]
k32.OpenThread.restype = wt.HANDLE
k32.OpenThread.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.OpenProcess.restype = wt.HANDLE
k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.SuspendThread.argtypes = [wt.HANDLE]
k32.ResumeThread.argtypes = [wt.HANDLE]
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(ctypes.c_uint64)] * 4
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
k32.CloseHandle.argtypes = [wt.HANDLE]
k32.Thread32First.argtypes = k32.Thread32Next.argtypes = [wt.HANDLE, ctypes.POINTER(THREADENTRY32)]

# the x64 CONTEXT: 1232 bytes, 16-aligned; ContextFlags at 0x30, Rsp 0x98, Rbp 0xa0, Rip 0xf8
_CTX_RAW = ctypes.create_string_buffer(1232 + 16)
_CTX = (ctypes.addressof(_CTX_RAW) + 15) & ~15


def ctx_u64(offset):
    return ctypes.c_uint64.from_address(_CTX + offset).value


def threads_of(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    out = []
    e = THREADENTRY32()
    e.dwSize = ctypes.sizeof(e)
    ok = k32.Thread32First(snap, ctypes.byref(e))
    while ok:
        if e.th32OwnerProcessID == pid:
            out.append(e.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(e))
    k32.CloseHandle(snap)
    return out


def cpu_time(h):
    c, x, kt, ut = (ctypes.c_uint64() for _ in range(4))
    if not k32.GetThreadTimes(h, ctypes.byref(c), ctypes.byref(x), ctypes.byref(kt), ctypes.byref(ut)):
        return None
    return (kt.value + ut.value) / 1e7


class Sampler:
    def __init__(self, process, hz):
        self.proc = process
        self.hproc = k32.OpenProcess(0x1F0FFF, False, process.pid)
        dbghelp.SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME)
        dbghelp.SymInitialize(self.hproc, None, False)
        self.period = 1.0 / hz
        self.handles, self.cpu = {}, {}
        self.samples = []            # (time, tid, (pc, ...))
        self.stop = threading.Event()
        self.walk_errors = 0
        self.refreshes = 0
        self.final_refresh = False

    def refresh(self):
        new = False
        for tid in threads_of(self.proc.pid):
            if tid not in self.handles:
                h = k32.OpenThread(THREAD_ACCESS, False, tid)
                if h:
                    self.handles[tid] = h
                    self.cpu[tid] = cpu_time(h) or 0.0
                    new = True
        # the modules (the unwinder needs their tables): often while the game starts, then now and then
        self.refreshes += 1
        if new or self.refreshes < 20 or self.refreshes % 10 == 0:
            dbghelp.SymRefreshModuleList(self.hproc)

    def stack(self, h):
        ctypes.c_uint32.from_address(_CTX + 0x30).value = CONTEXT_FULL
        if not k32.GetThreadContext(h, ctypes.c_void_p(_CTX)):
            return None
        f = STACKFRAME64()
        f.AddrPC.Offset, f.AddrPC.Mode = ctx_u64(0xF8), 3
        f.AddrFrame.Offset, f.AddrFrame.Mode = ctx_u64(0xA0), 3
        f.AddrStack.Offset, f.AddrStack.Mode = ctx_u64(0x98), 3
        pcs = []
        while len(pcs) < 128 and dbghelp.StackWalk64(IMAGE_FILE_MACHINE_AMD64, self.hproc, h, ctypes.byref(f),
                                                       ctypes.c_void_p(_CTX), None,
                                                       ctypes.cast(dbghelp.SymFunctionTableAccess64, ctypes.c_void_p),
                                                       ctypes.cast(dbghelp.SymGetModuleBase64, ctypes.c_void_p), None):
            if f.AddrPC.Offset == 0:
                break
            pcs.append(f.AddrPC.Offset)
        return tuple(pcs)

    def run(self):
        last_refresh = 0.0
        while not self.stop.is_set() and self.proc.poll() is None:
            now = time.monotonic()
            if now - last_refresh > 0.5:
                self.refresh()
                last_refresh = now
            for tid, h in list(self.handles.items()):
                t = cpu_time(h)
                if t is None:
                    continue
                busy = t > self.cpu[tid] + 1e-9
                self.cpu[tid] = t
                if not busy:
                    continue
                if k32.SuspendThread(h) == 0xFFFFFFFF:
                    continue
                try:
                    pcs = self.stack(h)
                finally:
                    k32.ResumeThread(h)
                if pcs:
                    self.samples.append((now, tid, pcs))
                else:
                    self.walk_errors += 1
            spent = time.monotonic() - now
            time.sleep(max(0.0, self.period - spent))

    def sym(self, addr):
        if not self.final_refresh:
            self.final_refresh = True
            dbghelp.SymRefreshModuleList(self.hproc)
        s = SYMBOL_INFO()
        s.SizeOfStruct = 88
        s.MaxNameLen = 500
        disp = ctypes.c_uint64()
        if dbghelp.SymFromAddr(self.hproc, addr, ctypes.byref(disp), ctypes.byref(s)):
            return s.Name.decode(errors="replace")
        return None


def pretty(name):
    """?Member@Class@@... -> Class::Member (a light MSVC undecoration; the full name stays in stacks.txt)"""
    m = re.match(r"\?([^@?][^@]*)@((?:[^@?][^@]*@)*)@", name)
    if not m:
        return name
    scopes = [s for s in m[2].split("@") if s]
    return "::".join(reversed(scopes)) + "::" + m[1] if scopes else m[1]


def load_map(map_path):
    import bisect
    line = re.compile(r'^ [0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+(?:f\s+)?(?:i\s+)?(\S+)?', re.I)
    syms = []
    for text in pathlib.Path(map_path).read_text(errors="replace").splitlines():
        m = line.match(text)
        if m:
            syms.append((int(m[2], 16), m[1]))
    syms.sort()
    keys = [s[0] for s in syms]

    def name(addr):
        k = bisect.bisect_right(keys, addr) - 1
        return syms[k][1] if k >= 0 and addr - syms[k][0] < 0x100000 else None
    return name, (keys[0], keys[-1]) if keys else (0, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split("\n", 2)[2])
    ap.add_argument("--data", default="binary")
    ap.add_argument("--slot", default="profile")
    ap.add_argument("--exe", type=pathlib.Path, default=sim.DEFAULT_EXE)
    ap.add_argument("--hz", type=float, default=250)
    ap.add_argument("--out", type=pathlib.Path)
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--thread", default="busiest", help="busiest (default) or all")
    ap.add_argument("--window", action="append", default=[],
                    help="GAME_S_FROM-GAME_S_TO: a separate report of the samples in that game time (repeatable)")
    ap.add_argument("--ai-scripts", type=pathlib.Path)
    ap.add_argument("args", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    args = a.args[1:] if a.args[:1] == ["--"] else a.args
    out = a.out or sim.ROOT / "build" / "sim" / "profile" / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True, exist_ok=True)
    if a.ai_scripts:
        os.environ["EE_AI_SCRIPTS"] = str(a.ai_scripts.resolve())
    sets = [args[k + 1] for k, v in enumerate(args) if v == "--set" and k + 1 < len(args)]
    stage = sim.prepare(a.data, a.slot, sets)
    cfg = stage / "config.yaml"
    cfg.unlink(missing_ok=True)
    # the game time over the wall: EE-SIM's progress lines ("[sim   12.3] game s 600 PT ...") on stderr
    marks = []   # (monotonic, game s)
    proc = subprocess.Popen(sim.command(a.exe.resolve(), stage, args, cfg), stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE, text=True, errors="replace")
    sampler = Sampler(proc, a.hz)
    th = threading.Thread(target=sampler.run, daemon=True)
    th.start()
    t0 = time.monotonic()
    with open(out / "sim.err.txt", "w", encoding="utf-8") as err:
        for line in proc.stderr:
            err.write(line)
            m = re.search(r"\] game s (\d+) PT", line)
            if m:
                marks.append((time.monotonic(), int(m[1])))
    proc.wait()
    sampler.stop.set()
    th.join()
    wall = time.monotonic() - t0
    name_of, (lo, hi) = load_map(a.exe.resolve().with_suffix(".map"))
    names = {}

    def fn(addr):
        if addr not in names:
            n = name_of(addr) if lo <= addr <= hi + 0x100000 else None
            names[addr] = pretty(n) if n else (sampler.sym(addr) or f"0x{addr:x}")
        return names[addr]

    def game_s(t):
        g = None
        for wt_, gs in marks:
            if wt_ <= t:
                g = gs
            else:
                break
        return g

    by_thread = collections.Counter(tid for _, tid, _ in sampler.samples)
    busiest = by_thread.most_common(1)[0][0] if by_thread else None
    lines = [f"EE-SIM profile: {wall:.1f} s wall, rc {proc.returncode}, {len(sampler.samples)} samples at {a.hz:g} Hz "
             f"({sampler.walk_errors} failed walks)", "threads (id: cpu s, samples):"]
    for tid, n in by_thread.most_common():
        lines.append(f"  {tid}: {sampler.cpu.get(tid, 0):.1f} s, {n}" + ("  <- busiest" if tid == busiest else ""))
    windows = [("all", None, None)] + [(w, *map(float, w.split("-", 1))) for w in a.window]
    stacks = collections.Counter()
    for label, w0, w1 in windows:
        sel = []
        for t, tid, pcs in sampler.samples:
            if a.thread != "all" and tid != busiest:
                continue
            if w0 is not None:
                g = game_s(t)
                if g is None or not (w0 <= g < w1):
                    continue
            sel.append(pcs)
        self_c, incl = collections.Counter(), collections.Counter()
        for pcs in sel:
            fns = [fn(p) for p in pcs]
            self_c[fns[0]] += 1
            for f in set(fns):
                incl[f] += 1
            if label == "all":
                stacks[";".join(reversed(fns))] += 1
        n = max(1, len(sel))
        lines.append(f"\n== window {label}: {len(sel)} samples of {'every thread' if a.thread == 'all' else 'the busiest thread'}")
        lines.append("self %   incl %   function")
        for f, c in self_c.most_common(a.top):
            lines.append(f"{100 * c / n:6.2f}  {100 * incl[f] / n:6.2f}   {f}")
        lines.append("\ninclusive % (top):")
        for f, c in incl.most_common(a.top * 2):
            lines.append(f"{100 * c / n:6.2f}   {f}")
    (out / "profile.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    with open(out / "stacks.txt", "w", encoding="utf-8") as f:
        for s, c in stacks.most_common():
            f.write(f"{s} {c}\n")
    print("\n".join(lines[: 2 + len(by_thread) + 40]))
    print(f"-> {out / 'profile.txt'}")


if __name__ == "__main__":
    main()
