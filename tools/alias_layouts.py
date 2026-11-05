"""Check the exe link's function aliases against the x64 layouts.

  python -I tools/alias_layouts.py [--build build/clang-test-x64] [-j 4]

The exe link (exe_fulllink.py) resolves a referenced but unwritten function to the function the original's ICF
folded it with (classes icf, vslot and mismatch in <build>/exe/link_report.json). In VC6 the two bodies were byte
for byte the same, so they read the same offsets; that holds in this build only while both classes keep those
members at equal offsets. For every alias A::f -> B::g whose target body is not trivial (`ret`, `return constant`;
read from the linked exe) and whose classes differ, the members at each offset both classes have on x86 are looked
up on x64: a pair that moves apart is reported. Free functions, same-class aliases and classes the units do not show
are listed for a manual look. Fix a reported alias by writing the alias's body in source (an `// ICF-FOLDED:` body
next to the class's other members).
"""
import argparse
import concurrent.futures
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools" / "harness"))
sys.path.insert(0, str(ROOT / "tools"))
import layout_gen   # noqa: E402

TRIVIAL = re.compile(r"^(ret|xor eax, eax ; ret|mov (eax|al), (0x)?[0-9a-f]+ ; ret)$")


def scope(sym):
    m = re.match(r"\?(?:\?[0-9A-Z_]|[^?@][^@]*)@([^@]+)@@", sym) or re.match(r"\?[^@]+@([^@]+)@@", sym)
    return m.group(1) if m else None


def short(sym):
    m = re.match(r"\?(\??[^@]+)@([^@]+)@@", sym)
    return f"{m.group(2)}::{m.group(1)}" if m else sym


def body(exe, va, base, md):
    out = []
    for i in md.disasm(exe.get_data(va - base, 96), va):
        out.append(f"{i.mnemonic} {i.op_str}".strip())
        if i.mnemonic in ("ret", "jmp") or len(out) >= 12:
            break
    return " ; ".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", default=str(ROOT / "build" / "clang-test-x64"))
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    import capstone
    import pefile
    bd = pathlib.Path(a.build)
    unres = json.loads((bd / "exe" / "link_report.json").read_text())["pass1"]["unresolved"]
    addr = {}
    for line in (bd / "exe" / "EE-AOC.map").read_text(encoding="latin-1").splitlines():
        m = re.match(r"\s*[0-9a-f]{4}:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8,16})\s", line)
        if m:
            addr.setdefault(m.group(1), int(m.group(2), 16))
    exe = pefile.PE(str(bd / "exe" / "EE-AOC.exe"), fast_load=True)
    base = exe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64 if exe.FILE_HEADER.Machine == 0x8664
                     else capstone.CS_MODE_32)
    pairs = []
    for s, v in sorted(unres.items()):
        m = re.search(r"(?:twin|runs|defined as) (\S+)", v["detail"]) if v["class"] in ("icf", "vslot", "mismatch") \
            else None
        if not m or m.group(1) not in addr:
            continue
        b = body(exe, addr[m.group(1)], base, md)
        if not TRIVIAL.match(b):
            pairs.append((s, m.group(1), scope(s), scope(m.group(1)), b))
    texts = {p: p.read_text(encoding="latin-1") for p in (ROOT / "src").rglob("*.cpp")}

    def unit_for(cls):
        pat = re.compile(r"\b" + re.escape(cls) + r"::")
        hits = sorted((p for p, t in texts.items() if pat.search(t)), key=lambda p: (p.stem != cls, len(str(p))))
        return hits[0].relative_to(ROOT / "src").as_posix() if hits else None
    unit_of = {c: unit_for(c) for _, _, ca, cb, _ in pairs for c in (ca, cb) if c}

    def module(rel):
        return "exe" if rel.startswith(("Empire Earth/", "RTS Engine/")) else "dx7hrtnl" if "DX7" in rel else "lle"
    layouts = {}

    def dump(job):
        rel, tgt = job
        d = {}
        layout_gen.parse(layout_gen.dump(module(rel), rel, tgt), d)
        return job, d
    jobs = [(u, t) for u in sorted({u for u in unit_of.values() if u}) for t in ("x86", "x64")]
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for job, d in ex.map(dump, jobs):
            layouts[job] = d

    def lay(cls, tgt):
        return layouts.get((unit_of.get(cls), tgt), {}).get(cls)
    bad = review = 0
    for s, t, ca, cb, b in pairs:
        if not ca or not cb or ca == cb:
            print(f"review  {short(s)} -> {short(t)} ({'same class' if ca == cb and ca else 'free function'})\n"
                  f"        {b[:160]}")
            review += 1
            continue
        L = [lay(ca, "x86"), lay(cb, "x86"), lay(ca, "x64"), lay(cb, "x64")]
        if not all(L):
            print(f"review  {short(s)} -> {short(t)} (no layout)\n        {b[:160]}")
            review += 1
            continue
        inv = []
        for rec in L[:2]:
            d = {}
            for p, o in rec["fields"].items():
                d.setdefault(o, []).append(p)
            inv.append({o: min(ps, key=lambda p: (p.count("."), len(p))) for o, ps in d.items()})
        moved = [f"{o:#x}: {inv[0][o]} {L[2]['fields'].get(inv[0][o], -1):#x} / {inv[1][o]} "
                 f"{L[3]['fields'].get(inv[1][o], -1):#x}"
                 for o in sorted(set(inv[0]) & set(inv[1]))
                 if L[2]["fields"].get(inv[0][o]) != L[3]["fields"].get(inv[1][o])]
        if moved:
            bad += 1
            print(f"MOVED   {short(s)} -> {short(t)}: {'; '.join(moved[:3])}\n        {b[:160]}")
    print(f"{len(pairs)} non-trivial aliases: {bad} with members at other x64 offsets, {review} to review by hand")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
