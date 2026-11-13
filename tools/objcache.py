"""The content-addressed object cache of the C++ build (tools/build.py, tools/clang_build.py), shared by every
worktree of the machine.

  EE_OBJ_CACHE=<dir>        the store (default C:\\EEX-objcache-store); EE_OBJ_CACHE=off disables the cache
  EE_OBJ_CACHE_MAX_GB=<n>   its size cap (default 4): the least recently used entries go at the end of a build
  python -I tools/objcache.py stats|prune|clear

A unit's key is two-level, like ccache's direct mode:
  - the manifest key: the schema, the compiler's identity (path, size, mtime of clang-cl.exe), every argument of the
    command line and the INCLUDE search path (the staging root, the repository root and the toolchain directory
    replaced by tokens), the unit's path under the staging root and the CONTENT of the source as compiled (after a
    test profile's overlay);
  - the manifest (<store>/m/) lists, newest first, the dependency sets seen for that key: every file the compile
    read (clang's -MD list: headers, #embed files), each with its content hash. A lookup re-hashes the recorded
    files; the first entry whose files all exist with those hashes names the object (<store>/o/, keyed by the
    manifest key and the dependency hashes). A missing file or object is a miss.
Nothing is keyed on a modification time except the compiler binary's identity.

Objects do not depend on the staging root or the worktree: compile_to always maps the staging root to C:\\EEX, the
repository root to C:\\EE-ROOT and the toolchain directory to C:\\EE-TOOLCHAIN in every path the compiler embeds
(-ffile-prefix-map: __FILE__), and compiles /Brepro (no time stamp in the object). So an object built in one
worktree is the object every other worktree would build from the same content.

Writes go to a unique temporary file renamed into place, so concurrent builds are safe; a rename that loses to
another writer (or to a reader holding the file open) is dropped: the other writer's file is the same object.
"""
import hashlib
import json
import os
import pathlib
import re
import shutil
import sys
import threading
import time
import uuid
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import CLANG, ORIG_SRC_ROOT, ROOT, TOOLCHAIN

SCHEMA = "ee-objcache-1"
DEFAULT_STORE = r"C:\EEX-objcache-store"
DEFAULT_MAX_GB = 4.0
MANIFEST_ENTRIES = 8   # dependency sets kept per manifest key (a header edited and reverted hits again)

# the canonical paths compile_to maps the per-worktree roots to (-ffile-prefix-map)
CANONICAL = {"stage": r"C:\EEX", "root": r"C:\EE-ROOT", "toolchain": r"C:\EE-TOOLCHAIN"}


def _norm(p):
    return os.path.normpath(str(pathlib.Path(p).resolve()))


def roots():
    """[(path, token)] of the per-worktree roots, longest first (the toolchain usually lives under a repository
    root, and the main checkout's root is a prefix of every other worktree's)."""
    r = [(_norm(ORIG_SRC_ROOT), "stage"), (_norm(TOOLCHAIN), "toolchain"), (_norm(ROOT), "root")]
    return sorted(r, key=lambda x: -len(x[0]))


def prefix_map_flags():
    """The clang-cl arguments that make every embedded path independent of the staging root and the worktree."""
    return [f"/clang:-ffile-prefix-map={p}={CANONICAL[t]}" for p, t in roots()]


def store_dir():
    """The store, or None when the cache is off (EE_OBJ_CACHE=off)."""
    v = os.environ.get("EE_OBJ_CACHE", "").strip()
    if v.lower() in ("off", "0", "no", "false", "none"):
        return None
    return pathlib.Path(v or DEFAULT_STORE)


def max_bytes():
    try:
        return int(float(os.environ.get("EE_OBJ_CACHE_MAX_GB", DEFAULT_MAX_GB)) * (1 << 30))
    except ValueError:
        return int(DEFAULT_MAX_GB * (1 << 30))


def _h(*parts):
    h = hashlib.blake2b(digest_size=20)
    for p in parts:
        b = p if isinstance(p, bytes) else str(p).encode("utf-8", "surrogateescape")
        h.update(len(b).to_bytes(8, "little"))
        h.update(b)
    return h.hexdigest()


def parse_depfile(text):
    """The prerequisites of a Make-style dependency file (clang -MD): spaces escaped as '\\ ', lines continued
    with a trailing backslash, Windows paths with their backslashes."""
    text = text.replace("\\\r\n", " ").replace("\\\n", " ")
    m = re.search(r":\s", text)   # the target ends at the first colon followed by space (a drive's colon is not)
    body = text[m.end():] if m else ""
    out, cur, i = [], [], 0
    while i < len(body):
        c = body[i]
        if c == "\\" and i + 1 < len(body) and body[i + 1] in " #":
            cur.append(body[i + 1])
            i += 2
            continue
        if c == "$" and body[i + 1:i + 2] == "$":
            cur.append("$")
            i += 2
            continue
        if c.isspace():
            if cur:
                out.append("".join(cur))
                cur = []
        else:
            cur.append(c)
        i += 1
    if cur:
        out.append("".join(cur))
    return out


class Cache:
    def __init__(self, store):
        self.store = pathlib.Path(store)
        self.roots = roots()
        self.lock = threading.Lock()
        self.hashes = {}   # file -> content hash, for one build (sources never change while a build runs)
        self.hits = self.misses = 0
        st = CLANG.stat()
        self.compiler = f"{_norm(CLANG)}|{st.st_size}|{st.st_mtime_ns}"

    # ---- paths ----
    def tokenize(self, s):
        """s with every per-worktree root (case-insensitively, at a path boundary) replaced by its token."""
        for p, t in self.roots:
            s = re.sub(re.escape(p) + r"(?=[\\/;=\"]|$)", "<" + t + ">", s, flags=re.I)
        return s

    def detokenize(self, s):
        for p, t in self.roots:
            s = s.replace("<" + t + ">", p)
        return s

    def file_hash(self, path):
        """The content hash of a file, or None when it is missing."""
        h = self.hashes.get(path)
        if h is None:
            try:
                h = _h(pathlib.Path(path).read_bytes())
            except OSError:
                return None
            self.hashes[path] = h
        return h

    # ---- keys ----
    def manifest_key(self, cmd_args, include_env, source_name, source_bytes):
        return _h(SCHEMA, self.compiler, *[self.tokenize(a) for a in cmd_args], self.tokenize(include_env),
                  self.tokenize(source_name), _h(source_bytes))

    def _mpath(self, key):
        return self.store / "m" / key[:2] / (key + ".json.z")

    def _opath(self, key):
        return self.store / "o" / key[:2] / (key + ".obj")

    def _read_manifest(self, key):
        try:
            return json.loads(zlib.decompress(self._mpath(key).read_bytes()))
        except (OSError, ValueError, zlib.error):
            return []

    def _write_atomic(self, dst, data=None, src=None):
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.with_name(f"{dst.name}.{uuid.uuid4().hex}.tmp")
        try:
            if src is not None:
                shutil.copyfile(src, tmp)
            else:
                tmp.write_bytes(data)
            os.replace(tmp, dst)
        except OSError:
            pass   # lost to another writer (or a reader holding it): theirs is as good
        finally:
            try:
                tmp.unlink()
            except OSError:
                pass

    @staticmethod
    def _touch(p):
        try:
            os.utime(p)
        except OSError:
            pass

    # ---- lookup / store ----
    def lookup(self, key, obj):
        """A hit copies the object to obj -> True."""
        for entry in self._read_manifest(key):
            if all(self.file_hash(self.detokenize(p)) == h for p, h in entry["deps"]):
                src, tmp = self._opath(entry["obj"]), None
                try:
                    if src.stat().st_size == 0:   # a truncated store write (a full disk): a miss, compiled again
                        continue
                except OSError:
                    continue
                try:
                    pathlib.Path(obj).parent.mkdir(parents=True, exist_ok=True)
                    tmp = pathlib.Path(f"{obj}.{uuid.uuid4().hex}.tmp")
                    shutil.copyfile(src, tmp)
                    os.replace(tmp, obj)
                except OSError:
                    try:
                        if tmp is not None:
                            tmp.unlink()
                    except OSError:
                        pass
                    return False
                self._touch(src)
                self._touch(self._mpath(key))
                with self.lock:
                    self.hits += 1
                return True
        return False

    def insert(self, key, obj, depfile, source):
        """Record a fresh compile: its object and its dependency set (the dependency file's prerequisites but the
        source itself, which the manifest key holds as compiled)."""
        with self.lock:
            self.misses += 1
        try:
            deps = parse_depfile(pathlib.Path(depfile).read_text(errors="surrogateescape"))
        except OSError:
            return
        src = _norm(source).lower()
        recs = []
        for d in deps:
            d = os.path.normpath(d)
            if d.lower() == src:
                continue
            h = self.file_hash(d)
            if h is None:
                return   # a file the compile read is gone: nothing to record
            recs.append([self.tokenize(d), h])
        recs.sort()
        okey = _h(key, *[x for r in recs for x in r])
        self._write_atomic(self._opath(okey), src=obj)
        entries = [e for e in self._read_manifest(key) if e.get("obj") != okey]
        entries = [{"deps": recs, "obj": okey}] + entries[:MANIFEST_ENTRIES - 1]
        self._write_atomic(self._mpath(key), data=zlib.compress(json.dumps(entries).encode(), 6))

    def note_uncached(self):
        with self.lock:
            self.misses += 1


def files(store):
    for sub in ("m", "o"):
        d = pathlib.Path(store) / sub
        if d.exists():
            for p in d.rglob("*"):
                if p.is_file():
                    yield p


def prune(store, limit=None):
    """Delete the least recently used objects and manifests until the store is under its cap, and temporary files
    an interrupted write left (older than an hour) -> (bytes kept, files deleted)."""
    limit = max_bytes() if limit is None else limit
    now = time.time()
    entries, deleted = [], 0
    for p in files(store):
        try:
            st = p.stat()
        except OSError:
            continue
        if p.name.endswith(".tmp"):
            if now - st.st_mtime > 3600:
                try:
                    p.unlink()
                    deleted += 1
                except OSError:
                    pass
            continue
        entries.append((st.st_mtime, st.st_size, p))
    total = sum(s for _, s, _ in entries)
    for _, size, p in sorted(entries, key=lambda e: e[0]):
        if total <= limit:
            break
        try:
            p.unlink()
            total -= size
            deleted += 1
        except OSError:
            pass   # in use by another build
    return total, deleted


_CACHE = None
_CACHE_LOCK = threading.Lock()


def get():
    """The process's cache, or None when it is off."""
    global _CACHE
    with _CACHE_LOCK:
        if _CACHE is None:
            store = store_dir()
            _CACHE = Cache(store) if store is not None else False
        return _CACHE or None


def summary():
    """' (N cached)' for a build's summary line ('' when the cache is off), after pruning the store."""
    c = get()
    if c is None:
        return " (cache off)"
    total, deleted = prune(c.store)
    return f" ({c.hits} cached, store {total / (1 << 20):.0f} MB{f', {deleted} pruned' if deleted else ''})"


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("action", choices=["stats", "prune", "clear"])
    a = ap.parse_args()
    store = store_dir()
    if store is None:
        sys.exit("the object cache is off (EE_OBJ_CACHE=off)")
    if a.action == "clear":
        shutil.rmtree(store / "m", ignore_errors=True)
        shutil.rmtree(store / "o", ignore_errors=True)
        print(f"cleared {store}")
        return
    if a.action == "prune":
        total, deleted = prune(store)
        print(f"{store}: {total / (1 << 20):.0f} MB kept, {deleted} files deleted (cap {max_bytes() >> 20} MB)")
        return
    n = {"m": [0, 0], "o": [0, 0]}
    for p in files(store):
        k = n[p.relative_to(store).parts[0]]
        k[0] += 1
        k[1] += p.stat().st_size
    print(f"{store}: {n['o'][0]} objects ({n['o'][1] / (1 << 20):.0f} MB), {n['m'][0]} manifests "
          f"({n['m'][1] / (1 << 20):.0f} MB), cap {max_bytes() >> 20} MB")


if __name__ == "__main__":
    main()
