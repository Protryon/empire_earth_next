"""How far the C++ -> Rust port has come, measured at git revisions.

usage: python -I tools/port_metrics.py [--repo DIR] [--json] REV [REV ...]

For each revision (and its change from the first one given):

  C++ that is left (src, include/ee, include/lle; ThirdParty excluded):
    cpp_lines   every line of every C/C++ file                       <- the main measure
    cpp_files   compiled C/C++ sources (.c/.cpp under src, ThirdParty included)
    code        lines that are not blank, comment, preprocessor or a lone brace
    shim        code lines of one-statement function bodies and one-statement cases
    logic       code - shim: behaviour still written in C++
    classes     class/struct definitions (src, include/ee, include/lle, include/gx, include/dx7)
  The bridge between the two languages (must reach zero at the end):
    extern_c    `extern "C" fn` in rust/crates
    rust_hdrs   generated C headers under include/rust
  Rust that still mirrors C++ memory (should fall once data is Rust-owned):
    unsafe      `unsafe` uses in rust/crates
    layout      C++ `offsetof`/`sizeof` static_asserts plus Rust `offset_of!`/`size_of::<` asserts
  For scale:
    rust_lines  every line of rust/crates/**/*.rs (tests and generated code included)

The counts are heuristics, but the same heuristics at every revision, so changes are comparable.
"""
import argparse
import json
import re
import subprocess
import sys

CPP_EXT = (".cpp", ".c", ".h", ".inl", ".hpp")
CPP_ROOTS = ("src", "include/ee", "include/lle")
CLASS_ROOTS = ("src", "include/ee", "include/lle", "include/gx", "include/dx7")
NON_CODE = ("{", "}", "};", "{}", "} // namespace")


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            seg = text[i:n if j < 0 else j + 2]
            out.append("\n" * seg.count("\n"))
            i = n if j < 0 else j + 2
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def is_code(line):
    s = line.strip()
    return bool(s) and not s.startswith("#") and s not in NON_CODE


FUNC_TAIL = re.compile(r"\)\s*(const|override|noexcept|final|\s)*(:[^;{}]*)?$")
CONTROL_TAIL = re.compile(r"\b(if|for|while|switch|catch)\s*\([^;{}]*\)\s*$")
CASE = re.compile(r"^\s*(case\s+(?:[^:]|::)+|default)\s*:(?!:)(.*)$")


def code_and_shim(text):
    """(code lines, shim lines) of one C/C++ file."""
    lines = strip_comments(text).split("\n")
    code = [is_code(l) for l in lines]
    shim = [False] * len(lines)
    stack = []
    for idx, l in enumerate(lines):
        for k, ch in enumerate(l):
            if ch == "{":
                before = (" ".join(lines[max(0, idx - 3):idx]) + " " + l[:k]).rstrip()
                stack.append((idx, bool(FUNC_TAIL.search(before)) and not CONTROL_TAIL.search(before)))
            elif ch == "}" and stack:
                start, is_func = stack.pop()
                if not is_func:
                    continue
                body = "\n".join(lines[start:idx + 1])
                flat = body[body.find("{") + 1: body.rfind("}")]
                while True:  # brace groups with no statement are initializers, not blocks
                    nxt = re.sub(r"\{[^{};]*\}", "()", flat)
                    if nxt == flat:
                        break
                    flat = nxt
                if flat.count(";") <= 1 and "{" not in flat:
                    sig = start
                    while sig > 0 and code[sig - 1] and not lines[sig - 1].rstrip().endswith((";", "}", "{")):
                        sig -= 1
                    for t in range(sig, idx + 1):
                        shim[t] = True
    for idx, l in enumerate(lines):
        m = CASE.match(l)
        if not m:
            continue
        rest = m.group(2).strip()
        two = re.match(r"^([^;{}]*;)\s*(return\b[^;]*;|break;)$", rest)
        if two and not two.group(1).lstrip().startswith(("if", "for", "while")):
            shim[idx] = True
            continue
        if rest.endswith(";") and rest.count(";") == 1:
            nxt = idx + 1
            while nxt < len(lines) and not code[nxt]:
                nxt += 1
            if nxt >= len(lines) or CASE.match(lines[nxt]) or lines[nxt].strip().startswith("}"):
                shim[idx] = True
            elif lines[nxt].strip().startswith(("return", "break")) and lines[nxt].strip().count(";") == 1:
                shim[idx] = shim[nxt] = True
        elif rest in ("", "{"):
            nxt = idx + 1
            while nxt < len(lines) and not code[nxt]:
                nxt += 1
            if nxt >= len(lines):
                continue
            stmt = lines[nxt].strip()
            if stmt.count(";") != 1 or stmt.startswith(("if", "for", "while", "switch")):
                continue
            if stmt.startswith("return"):
                shim[idx] = shim[nxt] = True
                continue
            brk = nxt + 1
            while brk < len(lines) and not code[brk] and lines[brk].strip() != "}":
                brk += 1
            if brk < len(lines) and lines[brk].strip() in ("break;", "} break;", "return 0;", "return;"):
                shim[idx] = shim[nxt] = shim[brk] = True
    return sum(code), sum(1 for a, b in zip(code, shim) if a and b)


CLASS_DEF = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?(class|struct)\s+(?:__declspec\([^)]*\)\s*)?(?:[A-Z_]+_API\s+)?"
                       r"(\w+)\b(?!\s*;)[^;]*$")
EXTERN_C = re.compile(r'extern\s+"C"\s+fn\b')
UNSAFE = re.compile(r"\bunsafe\b")
CPP_LAYOUT = re.compile(r"static_assert\s*\(\s*(offsetof|sizeof|alignof)\b")
RUST_LAYOUT = re.compile(r"(offset_of!|size_of::<|align_of::<)")


class Repo:
    def __init__(self, path):
        self.path = path
        self.proc = subprocess.Popen(["git", "-C", path, "cat-file", "--batch"], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE)

    def files(self, rev, roots):
        out = subprocess.run(["git", "-C", self.path, "ls-tree", "-r", "--name-only", rev, "--", *roots],
                             capture_output=True, text=True, check=True).stdout
        return [f for f in out.split("\n") if f]

    def read(self, rev, path):
        self.proc.stdin.write(f"{rev}:{path}\n".encode())
        self.proc.stdin.flush()
        header = self.proc.stdout.readline().split()
        if len(header) < 3 or header[1] == b"missing":
            return ""
        data = self.proc.stdout.read(int(header[2]))
        self.proc.stdout.read(1)
        return data.decode("utf-8", "replace")


def measure(repo, rev):
    m = dict.fromkeys(("cpp_lines", "cpp_files", "code", "shim", "logic", "classes", "extern_c", "rust_hdrs",
                       "unsafe", "layout", "rust_lines"), 0)
    for f in repo.files(rev, ("src",)):
        if f.endswith((".c", ".cpp")):
            m["cpp_files"] += 1
    classes = set()
    for f in repo.files(rev, CLASS_ROOTS):
        if not f.endswith(CPP_EXT) or "ThirdParty" in f:
            continue
        text = repo.read(rev, f)
        for line in text.split("\n"):
            cm = CLASS_DEF.match(line)
            if cm and not line.rstrip().endswith(";"):
                classes.add(cm.group(2))
        m["layout"] += len(CPP_LAYOUT.findall(text))
        if f.startswith(CPP_ROOTS):
            m["cpp_lines"] += text.count("\n")
            c, s = code_and_shim(text)
            m["code"] += c
            m["shim"] += s
    m["logic"] = m["code"] - m["shim"]
    m["classes"] = len(classes)
    m["rust_hdrs"] = sum(1 for f in repo.files(rev, ("include/rust",)) if f.endswith(".h"))
    for f in repo.files(rev, ("rust/crates",)):
        if not f.endswith(".rs"):
            continue
        text = repo.read(rev, f)
        m["rust_lines"] += text.count("\n")
        m["extern_c"] += len(EXTERN_C.findall(text))
        m["unsafe"] += len(UNSAFE.findall(text))
        m["layout"] += len(RUST_LAYOUT.findall(text))
    return m


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("revs", nargs="+")
    ap.add_argument("--repo", default=".")
    ap.add_argument("--json", action="store_true", help="one JSON object per revision")
    args = ap.parse_args()
    repo = Repo(args.repo)
    rows = []
    for rev in args.revs:
        sha = subprocess.run(["git", "-C", args.repo, "rev-parse", "--short=9", rev], capture_output=True, text=True,
                             check=True).stdout.strip()
        rows.append((rev, sha, measure(repo, sha)))
    if args.json:
        for rev, sha, m in rows:
            print(json.dumps({"rev": rev, "sha": sha, **m}))
        return 0
    keys = list(rows[0][2])
    width = max(12, *(len(r) for r, _, _ in rows))
    print(" " * width + "".join(f"{k:>11}" for k in keys))
    base = rows[0][2]
    for i, (rev, _, m) in enumerate(rows):
        print(f"{rev:<{width}}" + "".join(f"{m[k]:>11,}" for k in keys))
        if i:
            print(f"{'  change':<{width}}" + "".join(f"{m[k] - base[k]:>+11,}" for k in keys))
    return 0


if __name__ == "__main__":
    sys.exit(main())
