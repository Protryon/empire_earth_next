"""List dynamic_casts that only VC6's lenient RTTI runtime can resolve (port phase 1 audit).

  python -I tools/clang_rtti_audit.py [--image IMAGE ...] [--objs build/clang/obj]   (default images: exe + engine)

VC6's __RTDynamicCast finds the target type in the complete object's base list and does not check the source type.
The Windows msvcrt.dll takes a checked path for RTTI that records class descriptors (BCD_HASPCHD, which clang sets
and VC6 did not), so a cast between unrelated hierarchies returns null there: RTSUnit* -> RTSDummyUnit* broke the
first clang game load that way.

Each cast site comes from the objects: the two type descriptors (??_R0) pushed before each call to
___RTDynamicCast (target first, then source). Each class's bases come from the images' RTTI (complete object locator
-> hierarchy -> base class array; the exe's and the engine's: exe classes derive from engine ones). A cast is
reported when the target class has RTTI in some image and no class has both the source and the target among its
bases (itself included): only an object of an unrelated type could ever satisfy it. Targets without RTTI in any
image (the WON library: stubs until its source exists) are counted, not judged.

Heuristic: at -O1 clang may keep a type descriptor's address in a register and push that, so the two descriptors
before a call can belong to a neighbouring cast. Check every reported site in its source (phase 1: the three
EEUnitGoal* sites it reports in EEUCLandable/EEPMLand are such false positives; the real ones were the nine
RTSUnit* -> RTSDummyUnit* tests, now RTSDummyHead).
"""
import argparse
import collections
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from verify import Coff
from undname import undname

ROOT = pathlib.Path(__file__).resolve().parent.parent


def image_hierarchies(image, map_path):
    import pefile
    pe = pefile.PE(str(image), fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase

    def u(va, n=1):
        return struct.unpack(f"<{n}I", pe.get_data(va - base, 4 * n))

    def td_name(va):
        return pe.get_data(va + 8 - base, 256).split(b"\0")[0].decode("latin-1")
    bases = {}
    for line in pathlib.Path(map_path).read_text(encoding="latin-1").splitlines():
        f = line.split()
        if len(f) >= 3 and f[1].startswith("??_R4"):
            col = int(f[2], 16)
            _, _, _, td, chd = u(col, 5)
            _, _, n, bca = u(chd, 4)
            bases[td_name(td)] = {td_name(u(b)[0]) for b in u(bca, n)}
    return bases


def cast_sites(objdir):
    sites = collections.Counter()
    for obj in pathlib.Path(objdir).rglob("*.obj"):
        c = Coff(obj)
        for i, sec in enumerate(c.secs, 1):
            if not sec["chars"] & 0x20:
                continue
            recent = []
            for va, sym, typ in sorted(c.relocs(i), key=lambda r: r[0]):
                n = sym["name"]
                if n.startswith("??_R0"):
                    recent.append(n)
                elif n == "___RTDynamicCast" and len(recent) >= 2:
                    target, source = recent[-2], recent[-1]
                    sites[(source, target, str(obj.relative_to(objdir)))] += 1
                    recent = []
    return sites


def r0_to_td(name):
    # ??_R0?AVRTSUnit@@@8 -> .?AVRTSUnit@@
    return "." + name[len("??_R0"):-len("@8")]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--image", type=pathlib.Path, action="append")
    ap.add_argument("--objs", type=pathlib.Path, default=ROOT / "build" / "clang" / "obj")
    a = ap.parse_args()
    images = a.image or [ROOT / "build" / "clang" / "exe" / "EE-AOC.exe"]   # one image: the engine is inside
    bases = {}
    for img in images:
        bases.update(image_hierarchies(img, img.with_suffix(".map")))
    sites = cast_sites(a.objs)
    holders = collections.defaultdict(set)   # base type -> classes having it
    for cls, bs in bases.items():
        for b in bs:
            holders[b].add(cls)
    bad = collections.defaultdict(list)
    unknown = 0
    for (src, tgt, obj), n in sites.items():
        s, t = r0_to_td(src), r0_to_td(tgt)
        if t not in bases:
            unknown += n
            continue
        if not (holders[s] & holders[t]):
            bad[(s, t)].append((obj, n))
    print(f"{sum(sites.values())} dynamic_cast sites, {len(bases)} classes with RTTI in the image, "
          f"{unknown} sites whose target has no RTTI in these images")
    for (s, t), where in sorted(bad.items()):
        print(f"UNRELATED {s} -> {t}: " + ", ".join(f"{o} x{n}" for o, n in where))
    print(f"{sum(n for w in bad.values() for _, n in w)} sites in {len(bad)} unrelated (source, target) pairs")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
