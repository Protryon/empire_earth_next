"""The build's modules, targets and compile jobs.

Units come from link/<module>/units.txt (in link order), or every .cpp of the module's source folder when the module
has no unit list; each compiles with its module's flags (ee.MODULE_FLAGS) or its own `// UNIT-FLAGS:` line. Library
code built from source (LIB_UNITS: the PKWARE DCL implode/explode and dmusic for the engine, miniaudio for the mixer)
is compiled with the modules that link it.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, MODULE_FLAGS, ROOT, module_units

# Every module is linked into the one exe (no DLLs): the game, its image libraries, the engine with libjpeg, and the
# plug-ins (TCP protocol, mixer, Default random maps, the DX7 T&L renderer), registered in PIMModule.cpp.
MODULES = ["tcp", "mixer", "default", "lle", "jpeg", "dx7hrtnl", "exeimg", "exe"]
TARGETS = ["exe"]
TARGET_MODULES = {"exe": ["exe", "exeimg", "lle", "jpeg", "tcp", "mixer", "default", "dx7hrtnl"]}
PART_OF = {m: "exe" for m in MODULES if m != "exe"}
# units a module links besides its own: the platform layer's implementation (include/platform/, src/Platform/<os>/)
# with the engine, and new game units that have no address in the original (the db text loader)
PLATFORM_UNITS = ["Platform/Win32/PlatTime.cpp", "Platform/Win32/PlatThread.cpp", "Platform/Win32/PlatString.cpp",
                  "Platform/Win32/PlatPreferences.cpp", "Platform/Win32/PlatFile.cpp"]
EXTRA_UNITS = {"lle": PLATFORM_UNITS,
               "exe": ["Empire Earth/Database/EEDataText.cpp", "Empire Earth/Database/EEDataTextFile.cpp",
                       # a unit with no code range of its own (every member was ICF-folded into another unit's)
                       "Empire Earth/Calamities/EEGlobalCalamityClass.cpp"]}
LIB_UNITS = {"miniaudio": "ThirdParty/miniaudio/miniaudio.c", "pl_mpeg": "ThirdParty/pl_mpeg/pl_mpeg.c",
             "implode": "ThirdParty/pkdcl/implode.c", "explode": "ThirdParty/pkdcl/explode.c"}
# dmusic (DirectMusic reimplementation, src/ThirdParty/dmusic): every source but the POSIX thread shim
_DMUSIC = sorted(p for p in (ROOT / "src" / "ThirdParty" / "dmusic").rglob("*.c") if p.name != "Thread.Posix.c")
LIB_UNITS.update({"dmusic-" + p.relative_to(ROOT / "src" / "ThirdParty" / "dmusic").with_suffix("").as_posix()
                  .replace("/", "-"): p.relative_to(ROOT / "src").as_posix() for p in _DMUSIC})
LIB_MEMBERS = {"mixer": ["miniaudio"], "dx7hrtnl": ["pl_mpeg"], "lle": ["implode", "explode"] + sorted(
    n for n in LIB_UNITS if n.startswith("dmusic-"))}
LANGUAGE_RC = pathlib.PurePosixPath("Empire Earth/Language/Language.rc")
EXE_RC = ROOT / "src" / "Empire Earth" / "EE-AOC.rc"
EXE_MEMBERS = LINK_DIR / "exe" / "fulllink_members.tsv"


def rel_src(path):
    p = pathlib.Path(path).resolve()
    if not p.is_relative_to(ROOT / "src"):
        raise SystemExit(f"{path} is not under src/")
    return pathlib.PurePosixPath(p.relative_to(ROOT / "src").as_posix())


def jobs_for(mod):
    """[(module, source, flags)] of a module's units (and the shared extra units it links)."""
    paths = [p for _, _, p in module_units(mod)] + [ROOT / "src" / rel for rel in EXTRA_UNITS.get(mod, [])]
    for p in paths:
        if not p.exists():
            sys.exit(f"{mod}: unit {p} does not exist")
    return [(mod, p, MODULE_FLAGS[mod]) for p in paths]


def lib_jobs(mods):
    """[("lib", source, flags)] of the library members the modules link."""
    return [("lib", ROOT / "src" / LIB_UNITS[n], MODULE_FLAGS[m]) for m in mods for n in LIB_MEMBERS.get(m, [])]


def obj_path(out, job):
    return pathlib.Path(out) / "obj" / job[0] / rel_src(job[1]).with_suffix(".obj")
