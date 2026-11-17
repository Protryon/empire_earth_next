"""TEST ONLY: the shared bench queue. One daemon per machine plays the EE-SIM games every checkout's
tools/sim_bench.py --queue submits, on all cores.

  python -I tools/bench.py status                  # running and queued games, requesters, slots
  python -I tools/bench.py pause [--no-wait]       # start no game; wait for the running ones (before tools/gates.sh)
  python -I tools/bench.py resume
  python -I tools/bench.py jobs N                  # games at once from now on
  python -I tools/bench.py serve [--jobs N] [--port-base P]   # the daemon (sim_bench --queue starts it if needed)
  python -I tools/bench.py stop                    # the daemon ends once the running games are done

The daemon listens on 127.0.0.1:EE_BENCH_QUEUE_PORT (default 47940; JSON lines). Its state lives in the main
checkout's build/sim/queue: queue.log (one line per event), jobs/<id>/ (each game's report, trace, progress and
worker.log; kept 6 hours).

- **Slots and ports.** --jobs N slots (default: the cores less 2); slot k plays in the requesting checkout's stage
  build/sim/stage/queue-<k>, on the port offset --port-base + 64 k (default 9000: TCP 42334 and up), so no requester
  picks ports.
- **A game** is played by the requesting checkout's own tools (`python -I <checkout>/tools/bench.py work JOB`:
  sim_bench.play, the same code as a direct bench), with the requester's EE_* variables, exe and data. Before it
  starts, the worker recomputes the game's cache key; a game whose inputs (exe, scripts, data, documents) changed
  since it was submitted fails instead of playing other inputs under the old key.
- **Identical games** (the same cache key: tools/sim_bench.py --cache) are played once: a second request for a queued
  or running game waits for the same one, and a finished game is served again for 6 hours (and from the cache with
  --cache). Runs of --repeat past the first have no key and are always played.
- **Order.** Priority first (sim_bench --priority confirm before explore), then first come.
- **A requester killed** (its connection gone): its queued games no other requester waits for are dropped; its
  running games finish (and go to the cache with --cache).
- **pause** finishes the running games and starts none until resume; it waits for them unless --no-wait.
- **One EE-SIM process per slot** (serve's default; --single: one process per game). Slot k's worker
  (`<checkout>/tools/bench.py slotwork`, started for the first game of a checkout in that slot) keeps a sim.Batch: its
  EE-SIM plays the slot's games one after the other (EE-SIM --games: the start-up paid once; each game's report and
  trace as alone), a new process when the exe, the process options or the EE_* variables change. A game that fails
  before its game ran is retried in a process of its own (sim_bench.play). A worker idle 60 s, and every worker on
  pause or stop, ends with its EE-SIM; its log is build/sim/queue/slot-<k>.log.
"""
import argparse
import heapq
import itertools
import json
import os
import pathlib
import shutil
import socket
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
PORT = int(os.environ.get("EE_BENCH_QUEUE_PORT", "47940"))
PRIORITY = {"confirm": 0, "explore": 1}
KEEP_SECONDS = 6 * 3600


def main_checkout():
    """the main checkout (the first of git worktree list), else this one"""
    try:
        first = subprocess.run(["git", "-C", str(ROOT), "worktree", "list", "--porcelain"], capture_output=True,
                               text=True).stdout.split("\n")[0]
    except OSError:
        first = ""
    return pathlib.Path(first.split(" ", 1)[1].strip()) if first.startswith("worktree ") else ROOT


def state_dir():
    """build/sim/queue of the main checkout (EE_BENCH_QUEUE_DIR: another, e.g. a second daemon's for a test, with
    EE_BENCH_QUEUE_PORT)"""
    d = os.environ.get("EE_BENCH_QUEUE_DIR")
    return pathlib.Path(d) if d else main_checkout() / "build" / "sim" / "queue"


# ------------------------------------------------------------------------------------------------ the protocol

class Conn:
    """a JSON-lines connection"""

    def __init__(self, sock):
        self.sock = sock
        self.rfile = sock.makefile("rb")
        self.lock = threading.Lock()

    def send(self, msg):
        data = (json.dumps(msg) + "\n").encode()
        with self.lock:
            self.sock.sendall(data)

    def recv(self):
        line = self.rfile.readline()
        if not line:
            return None
        return json.loads(line)

    def close(self):
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.sock.close()


def connect(timeout=None):
    s = socket.create_connection(("127.0.0.1", PORT), timeout=timeout)
    s.settimeout(None)
    return Conn(s)


def request(msg):
    """one request and its answer"""
    c = connect(5)
    try:
        c.send(msg)
        return c.recv()
    finally:
        c.close()


def ensure_daemon(log=print):
    """connect to the daemon, starting it (detached, from this checkout's tools) when none answers -> Conn"""
    try:
        return connect(2)
    except OSError:
        pass
    d = state_dir()
    d.mkdir(parents=True, exist_ok=True)
    out = open(d / "serve.out", "ab")
    kw = dict(stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT, close_fds=True)
    if sys.platform == "win32":
        kw["creationflags"] = (subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
                               | subprocess.CREATE_NO_WINDOW)
    else:
        kw["start_new_session"] = True
    subprocess.Popen([sys.executable, "-I", str(HERE / "bench.py"), "serve"], **kw)
    log(f"queue: started the daemon (log {d / 'queue.log'})")
    for _ in range(100):
        time.sleep(0.2)
        try:
            return connect(2)
        except OSError:
            pass
    sys.exit(f"queue: the daemon did not start (see {d / 'serve.out'})")


# ------------------------------------------------------------------------------------------------ the daemon

class Job:
    seq = itertools.count()

    def __init__(self, key, priority, spec):
        self.id = f"{time.strftime('%Y%m%d-%H%M%S')}-{next(Job.seq):05d}"
        self.key, self.priority, self.spec = key, priority, spec
        self.order = (priority, next(Job.seq))
        self.waiters = []   # (client, request id)
        self.state = "queued"
        self.slot = self.proc = self.started = None
        self.who = spec.get("who", "?")


class SlotWorker:
    """a slot's persistent worker (bench.py slotwork) and its EE-SIM process"""

    def __init__(self, tool, proc):
        self.tool, self.proc = tool, proc
        self.busy = False
        self.idle_since = time.time()

    def close(self):
        def end(proc):
            try:
                proc.stdin.close()   # EOF: the worker closes its EE-SIM and ends
                proc.wait(timeout=60)
            except (OSError, subprocess.TimeoutExpired):
                proc.kill()
        threading.Thread(target=end, args=(self.proc,), daemon=True).start()


WORKER_IDLE_SECONDS = 60


class Daemon:
    def __init__(self, jobs, port_base, batch=True):
        self.lock = threading.Lock()
        self.idle = threading.Condition(self.lock)
        self.max_jobs, self.port_base = jobs, port_base
        self.batch = batch        # one EE-SIM process per slot (SlotWorker), else one per game
        self.workers = {}         # slot -> SlotWorker
        self.paused = self.stopping = False
        self.heap = []            # (order, job id)
        self.jobs = {}            # job id -> queued or running Job
        self.by_key = {}          # key -> queued or running Job
        self.done = {}            # key -> (time, dir, rid, result): finished games served again
        self.running = {}         # slot -> Job
        self.clients = {}         # client id -> (name, Conn)
        self.cids = itertools.count(1)
        self.dir = state_dir()
        (self.dir / "jobs").mkdir(parents=True, exist_ok=True)
        self.logf = open(self.dir / "queue.log", "a", encoding="utf-8")
        self.stats = dict(played=0, shared=0, served=0, dropped=0, failed=0)

    def log(self, text):
        self.logf.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {text}\n")
        self.logf.flush()

    # -- scheduling (under the lock)

    def schedule(self):
        while not self.paused and not self.stopping and self.heap and len(self.running) < self.max_jobs:
            _, jid = heapq.heappop(self.heap)
            job = self.jobs.get(jid)
            if job is None or job.state != "queued":
                continue
            slot = min(k for k in range(self.max_jobs + len(self.running) + 1) if k not in self.running)
            self.start(job, slot)

    def start(self, job, slot):
        d = self.dir / "jobs" / job.id
        d.mkdir(parents=True, exist_ok=True)
        spec = dict(job.spec, slot=f"queue-{slot}", port=self.port_base + 64 * slot, dir=str(d), key=job.key)
        (d / "job.json").write_text(json.dumps(spec, indent=1))
        if self.batch:
            self.start_batch(job, slot, spec, d)
            return
        log = open(d / "worker.log", "wb")
        kw = dict(stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
        if sys.platform == "win32":
            kw["creationflags"] = subprocess.CREATE_NO_WINDOW
        try:
            job.proc = subprocess.Popen([sys.executable, "-I", spec["tool"], "work", str(d / "job.json")], **kw)
        except OSError as e:
            log.close()
            self.finish(job, dict(rc=-4, error=f"worker did not start: {e}"), d)
            return
        log.close()
        job.state, job.slot, job.started = "running", slot, time.time()
        self.running[slot] = job
        self.log(f"start {job.id} slot {slot} {job.spec['rid']} for {job.who} (priority {job.priority}, "
                 f"{len(job.waiters)} waiting)")
        threading.Thread(target=self.watch, args=(job, d), daemon=True).start()

    def start_batch(self, job, slot, spec, d):
        """the game to the slot's worker (started, or restarted for another checkout's tools) (under the lock)"""
        w = self.workers.get(slot)
        if w is not None and (w.tool != spec["tool"] or w.proc.poll() is not None):
            del self.workers[slot]
            w.close()
            w = None
        if w is None:
            log = open(self.dir / f"slot-{slot}.log", "ab")
            kw = dict(stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, text=True, bufsize=1)
            if sys.platform == "win32":
                kw["creationflags"] = subprocess.CREATE_NO_WINDOW
            try:
                proc = subprocess.Popen([sys.executable, "-I", spec["tool"], "slotwork"], **kw)
            except OSError as e:
                log.close()
                self.finish(job, dict(rc=-4, error=f"slot worker did not start: {e}"), d)
                return
            log.close()
            w = self.workers[slot] = SlotWorker(spec["tool"], proc)
            self.log(f"slot {slot}: worker pid {proc.pid} ({spec['tool']})")
        w.busy = True
        job.proc = w.proc
        job.state, job.slot, job.started = "running", slot, time.time()
        self.running[slot] = job
        self.log(f"start {job.id} slot {slot} {job.spec['rid']} for {job.who} (priority {job.priority}, "
                 f"{len(job.waiters)} waiting, slot process)")
        threading.Thread(target=self.watch_batch, args=(job, w, d), daemon=True).start()

    def watch_batch(self, job, w, d):
        line = ""
        try:
            w.proc.stdin.write(str(d / "job.json") + "\n")
            w.proc.stdin.flush()
            line = w.proc.stdout.readline()
        except OSError:
            pass
        try:
            result = json.loads((d / "result.json").read_text())
        except (OSError, ValueError):
            result = dict(rc=-1, error=f"slot worker ended without a result ({line.strip() or 'no answer'}; see "
                                       f"{self.dir / f'slot-{job.slot}.log'})")
        with self.lock:
            w.busy = False
            w.idle_since = time.time()
            if not line and self.workers.get(job.slot) is w:   # the worker died: the next game starts a new one
                del self.workers[job.slot]
                w.close()
            self.running.pop(job.slot, None)
            self.finish(job, result, d)
            self.schedule()
            self.idle.notify_all()

    def reap_workers(self):
        """end the idle slot workers: idle 60 s, past the slot count, or all on pause or stop (under the lock)"""
        for slot, w in list(self.workers.items()):
            if w.busy:
                continue
            if self.paused or self.stopping or slot >= self.max_jobs or \
                    time.time() - w.idle_since > WORKER_IDLE_SECONDS or w.proc.poll() is not None:
                self.log(f"slot {slot}: worker pid {w.proc.pid} ends")
                del self.workers[slot]
                w.close()

    def watch(self, job, d):
        rc = job.proc.wait()
        try:
            result = json.loads((d / "result.json").read_text())
        except (OSError, ValueError):
            result = dict(rc=rc or -1, error=f"worker exit {rc}, no result (see {d / 'worker.log'})")
        with self.lock:
            self.running.pop(job.slot, None)
            self.finish(job, result, d)
            self.schedule()
            self.idle.notify_all()

    def finish(self, job, result, d):
        """send a finished game to its waiters (under the lock)"""
        self.jobs.pop(job.id, None)
        if job.key and self.by_key.get(job.key) is job:
            del self.by_key[job.key]
        job.state = "done"
        ok = result.get("rc") == 0 and not result.get("error")
        self.stats["played" if ok else "failed"] += 1
        if job.key and ok:
            self.done[job.key] = (time.time(), str(d), job.spec["rid"], result)
        msg = dict(op="done", dir=str(d), rid=job.spec["rid"], result=result)
        for client, rid in job.waiters:
            self.reply(client, dict(msg, id=rid))
        self.log(f"done {job.id} {job.spec['rid']}: rc {result.get('rc')} {result.get('outcome', '')} "
                 f"{result.get('error', '')} -> {len(job.waiters)} waiting")

    def reply(self, client, msg):
        entry = self.clients.get(client)
        if entry is None:
            return
        try:
            entry[1].send(msg)
        except OSError:
            pass

    def prune(self):
        now = time.time()
        for key in [k for k, v in self.done.items() if now - v[0] > KEEP_SECONDS]:
            del self.done[key]
        for d in (self.dir / "jobs").iterdir():
            try:
                if now - d.stat().st_mtime > KEEP_SECONDS and not any(j.id == d.name for j in self.jobs.values()):
                    shutil.rmtree(d, ignore_errors=True)
            except OSError:
                pass

    # -- requests

    def submit(self, client, msg):
        key, rid = msg.get("key"), msg["id"]
        with self.lock:
            if key and key in self.done:
                _, d, jrid, result = self.done[key]
                self.stats["served"] += 1
                self.log(f"served {jrid} again to {self.clients[client][0] or client}")
                return dict(op="done", id=rid, dir=d, rid=jrid, result=result)
            if key and key in self.by_key:
                job = self.by_key[key]
                job.waiters.append((client, rid))
                p = PRIORITY.get(msg.get("priority"), 1)
                if job.state == "queued" and p < job.priority:   # a confirmation joining an exploration's game
                    job.priority, job.order = p, (p, job.order[1])
                    heapq.heappush(self.heap, (job.order, job.id))
                self.stats["shared"] += 1
                self.log(f"shared {job.id} {job.spec['rid']} with {self.clients[client][0] or client} ({job.state})")
                return dict(op="queued", id=rid, job=job.id, shared=True, state=job.state)
            job = Job(key, PRIORITY.get(msg.get("priority"), 1), msg["job"])
            job.waiters.append((client, rid))
            self.jobs[job.id] = job
            if key:
                self.by_key[key] = job
            heapq.heappush(self.heap, (job.order, job.id))
            self.schedule()
            return dict(op="queued", id=rid, job=job.id, shared=False, state=job.state)

    def drop_client(self, client):
        """a requester gone: its queued games nobody else waits for are dropped"""
        with self.lock:
            name = self.clients.pop(client, (None,))[0]
            dropped = 0
            for job in list(self.jobs.values()):
                job.waiters = [w for w in job.waiters if w[0] != client]
                if job.state == "queued" and not job.waiters:
                    self.jobs.pop(job.id, None)
                    if job.key and self.by_key.get(job.key) is job:
                        del self.by_key[job.key]
                    dropped += 1
            self.stats["dropped"] += dropped
            if name or dropped:
                self.log(f"bye {name}" + (f": {dropped} queued games dropped" if dropped else ""))

    def status(self):
        with self.lock:
            now = time.time()
            queued = sorted((j for j in self.jobs.values() if j.state == "queued"), key=lambda j: j.order)
            return dict(op="status", paused=self.paused, stopping=self.stopping, jobs=self.max_jobs,
                        port_base=self.port_base, pid=os.getpid(), stats=dict(self.stats),
                        clients=sorted(n for n, _ in self.clients.values() if n),
                        running=[dict(job=j.id, slot=s, rid=j.spec["rid"], who=j.who, seconds=round(now - j.started),
                                      waiting=len(j.waiters)) for s, j in sorted(self.running.items())],
                        queued=[dict(job=j.id, rid=j.spec["rid"], who=j.who, priority=j.priority,
                                     waiting=len(j.waiters)) for j in queued])

    def handle(self, sock):
        conn = Conn(sock)
        cid = next(self.cids)
        with self.lock:
            self.clients[cid] = (None, conn)   # (named by its hello: a requester)
        try:
            while True:
                try:
                    msg = conn.recv()
                except (OSError, ValueError):
                    msg = None
                if msg is None:
                    break
                op = msg.get("op")
                if op == "hello":
                    with self.lock:
                        self.clients[cid] = (f"{msg.get('who', 'client')}#{cid}", conn)
                        self.log(f"hello {self.clients[cid][0]} pid {msg.get('pid')}")
                    conn.send(dict(op="hello", jobs=self.max_jobs, paused=self.paused))
                elif op == "submit":
                    conn.send(self.submit(cid, msg))
                elif op == "status":
                    conn.send(self.status())
                elif op == "pause":
                    with self.lock:
                        self.paused = True
                        self.log("pause")
                    conn.send(dict(op="paused", running=len(self.running)))
                elif op == "resume":
                    with self.lock:
                        self.paused = False
                        self.log("resume")
                        self.schedule()
                    conn.send(dict(op="resumed"))
                elif op == "jobs":
                    with self.lock:
                        self.max_jobs = max(1, int(msg["n"]))
                        self.log(f"jobs {self.max_jobs}")
                        self.schedule()
                    conn.send(dict(op="jobs", jobs=self.max_jobs))
                elif op == "stop":
                    with self.lock:
                        self.stopping = True
                        self.log("stop")
                    conn.send(dict(op="stopping", running=len(self.running)))
                else:
                    conn.send(dict(op="error", error=f"unknown op {op!r}"))
        finally:
            self.drop_client(cid)
            conn.close()

    def serve(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if sys.platform == "win32":   # one daemon: a second bind on the port fails
            srv.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        try:
            srv.bind(("127.0.0.1", PORT))
        except OSError as e:
            sys.exit(f"queue: 127.0.0.1:{PORT} is taken ({e}): a daemon runs already?")
        srv.listen(64)
        srv.settimeout(1.0)
        self.log(f"serve pid {os.getpid()} port {PORT} jobs {self.max_jobs} port base {self.port_base} "
                 f"({HERE / 'bench.py'})")
        last_prune = 0
        while True:
            with self.lock:
                self.reap_workers()
                if self.stopping and not self.running and not self.workers:
                    self.log("stopped")
                    return
            if time.time() - last_prune > 600:
                last_prune = time.time()
                with self.lock:
                    self.prune()
            try:
                sock, _ = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                continue
            sock.settimeout(None)
            threading.Thread(target=self.handle, args=(sock,), daemon=True).start()


# ------------------------------------------------------------------------------------------------ the worker

def work(job_file, batches=None):
    """play one queued game with this checkout's sim_bench (the daemon starts it) -> result.json beside the job.
    batches: the slot worker's sim.Batch by slot (slotwork)"""
    spec = json.loads(pathlib.Path(job_file).read_text())
    out = pathlib.Path(spec["dir"])
    sys.path.insert(0, str(HERE))
    import sim_bench  # noqa: E402
    # the requester's game inputs from the environment: its EE_* variables, not the daemon's
    for k in [k for k in os.environ if k.upper().startswith("EE_") and k.upper() not in sim_bench.NOT_INPUTS]:
        del os.environ[k]
    os.environ.update(spec["environ"])
    a = argparse.Namespace(**spec["args"])
    a.exe = pathlib.Path(a.exe)
    a.cache_dir = pathlib.Path(a.cache_dir) if a.cache_dir else None
    run = spec["run"]
    result = None
    if spec.get("key"):
        key, _ = sim_bench.cache_key(run, a, sim_bench.game_opts(run, a))
        if key != spec["key"]:
            result = dict(rc=-3, error="the game's inputs changed after it was queued (exe, scripts, data or "
                                       "documents): not played")
    if result is None:
        res = sim_bench.play(run, a, spec["slot"], spec["port"], out, batch=batches)
        rep = res.get("report") or {}
        result = dict(rc=res["rc"], wall=res["wall"], lua_errors=res.get("lua_errors", 0), cached=res.get("cached"),
                      outcome=rep.get("outcome"))
    (out / "result.json").write_text(json.dumps(result) + "\n")
    return 0


def slot_work():
    """a slot's worker (the daemon starts it): a job file a line on stdin, played in the slot's one EE-SIM process
    (sim_bench.play with a sim.Batch), answered "done FILE" on stdout; EOF closes the EE-SIM process"""
    proto = sys.stdout
    sys.stdout = sys.stderr   # (sim_bench's progress to the slot's log)
    environ = dict(os.environ)
    batches = {}
    try:
        for line in sys.stdin:
            job_file = line.strip()
            if not job_file:
                continue
            os.environ.clear()
            os.environ.update(environ)
            try:
                work(job_file, batches)
            except Exception as e:   # noqa: BLE001 (a bad job fails alone; the worker goes on)
                out = pathlib.Path(json.loads(pathlib.Path(job_file).read_text())["dir"])
                (out / "result.json").write_text(json.dumps(dict(rc=-5, error=f"slot worker: {e!r}")) + "\n")
            print(f"[{time.strftime('%H:%M:%S')}] done {job_file}", flush=True)
            proto.write(f"done {job_file}\n")
            proto.flush()
    finally:
        for b in batches.values():
            b.close()
    return 0


# ------------------------------------------------------------------------------------------------ commands

def print_status(st):
    print(f"queue pid {st['pid']}: {'PAUSED, ' if st['paused'] else ''}{'stopping, ' if st['stopping'] else ''}"
          f"{len(st['running'])}/{st['jobs']} running, {len(st['queued'])} queued; port base {st['port_base']}; "
          + ", ".join(f"{k} {v}" for k, v in st["stats"].items()))
    for r in st["running"]:
        print(f"  slot {r['slot']:2} {r['seconds']:5} s  {r['rid']}  ({r['who']}"
              + (f", {r['waiting']} waiting" if r["waiting"] > 1 else "") + ")")
    by = {}
    for q in st["queued"]:
        by.setdefault((q["who"], q["priority"]), []).append(q)
    for (who, p), qs in sorted(by.items(), key=lambda x: x[0][1]):
        print(f"  queued {len(qs):4} for {who} ({[k for k, v in PRIORITY.items() if v == p][0]})")
    if st["clients"]:
        print("  requesters: " + ", ".join(st["clients"]))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=("serve", "status", "pause", "resume", "jobs", "stop", "work", "slotwork"))
    ap.add_argument("arg", nargs="?")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 4) - 2))
    ap.add_argument("--port-base", type=int, default=9000)
    ap.add_argument("--no-wait", action="store_true", help="pause: do not wait for the running games")
    ap.add_argument("--single", action="store_true", help="serve: one EE-SIM process per game (default: one per slot)")
    ap.add_argument("--json", action="store_true", help="status: the raw status")
    a = ap.parse_args()
    if a.cmd == "work":
        return work(a.arg)
    if a.cmd == "slotwork":
        return slot_work()
    if a.cmd == "serve":
        Daemon(a.jobs, a.port_base, batch=not a.single).serve()
        return 0
    try:
        if a.cmd == "status":
            st = request(dict(op="status"))
            print(json.dumps(st, indent=1) if a.json else "", end="")
            if not a.json:
                print_status(st)
        elif a.cmd == "pause":
            r = request(dict(op="pause"))
            print(f"queue: paused; {r['running']} games running")
            while not a.no_wait:
                st = request(dict(op="status"))
                if not st["running"]:
                    print("queue: paused, no game running")
                    break
                print(f"queue: waiting for {len(st['running'])} running games: "
                      + ", ".join(f"{r['rid']} ({r['seconds']} s)" for r in st["running"]), flush=True)
                time.sleep(5)
        elif a.cmd == "resume":
            request(dict(op="resume"))
            print("queue: resumed")
        elif a.cmd == "jobs":
            r = request(dict(op="jobs", n=int(a.arg)))
            print(f"queue: {r['jobs']} games at once")
        elif a.cmd == "stop":
            r = request(dict(op="stop"))
            print(f"queue: stopping after {r['running']} running games")
    except OSError as e:
        if a.cmd == "pause":   # nothing runs: nothing to pause
            print(f"queue: no daemon on 127.0.0.1:{PORT} ({e}): nothing to pause")
            return 0
        sys.exit(f"queue: no daemon on 127.0.0.1:{PORT} ({e})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
