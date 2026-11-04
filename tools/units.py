"""The build's modules, targets and compile jobs.

Units come from link/<module>/units.txt (in link order), or every .cpp of the module's source folder when the module
has no unit list; each compiles with its module's flags (ee.MODULE_FLAGS) or its own `// UNIT-FLAGS:` line. Library
code built from source (LIB_UNITS: the PKWARE DCL implode/explode for the engine, Miles' MSS_auto_cleanup for the
mixer) is compiled with the modules that link it.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LINK_DIR, MODULE_FLAGS, ROOT, module_units

MODULES = ["tcp", "mixer", "default", "lle", "jpeg", "dx7hr", "dx7hrtnl", "exeimg", "exe"]
TARGETS = ["tcp", "mixer", "default", "lle", "dx7hr", "dx7hrtnl", "language", "exe"]
TARGET_MODULES = {"lle": ["lle", "jpeg"], "exe": ["exe", "exeimg"], "language": []}
PART_OF = {"jpeg": "lle", "exeimg": "exe"}
# units a module links besides its own (none since the VC6 build's inline-asm _ftol went: clang never calls it)
EXTRA_UNITS = {}
LIB_UNITS = {"mssauto": "ThirdParty/mss/mssauto.c",
             "implode": "ThirdParty/pkdcl/implode.c", "explode": "ThirdParty/pkdcl/explode.c",
             # DirectMusic and Direct3D 7 GUIDs (the Windows SDK's dxguid.lib no longer has them)
             "dmguids": "ThirdParty/dxguid/dmusic_guids.c", "dx7guids": "ThirdParty/dxguid/dx7_guids.c"}
LIB_MEMBERS = {"mixer": ["mssauto"], "lle": ["implode", "explode", "dmguids"], "dx7hr": ["dx7guids"],
               "dx7hrtnl": ["dx7guids"]}
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
