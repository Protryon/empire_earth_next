"""Shared paths, module table, compiler flags and toolchain locations of the build (clang-cl + lld-link, MSVC STL)."""
import os
import pathlib
ROOT = pathlib.Path(__file__).resolve().parent.parent
LINK_DIR = ROOT / "link"
TOOLCHAIN = pathlib.Path(os.environ.get("EE_TOOLCHAIN", ROOT / "toolchain"))   # downloaded headers (DX7 SDK)
ORIG_SRC_ROOT = pathlib.Path(os.environ.get("EE_ORIG_SRC_ROOT", r"C:\EEX"))   # where units are staged to compile
MODULE_SRC = {
    "tcp": "Low-Level Engine/Network Engine/TCP Protocol",
    "mixer": "Low-Level Engine/Sound Engine/Mixer",
    "default": "Empire Earth/Random Map/Random Map Generators/Default",
    "lle": "Low-Level Engine",
    "dx7hrtnl": "Low-Level Engine/Graphics Engine/DX7 Display Plug-ins",
    "exe": "",
    "jpeg": "ThirdParty/libjpeg",
    "exeimg": "ThirdParty",
}
EEX_INCLUDE_DIRS = ["Low-Level Engine/Thread System", "Low-Level Engine/Graphics Engine",
                    "Empire Earth/Database", "Empire Earth/AI Thread"]


def module_src(mod):
    return ROOT / "src" / MODULE_SRC.get(mod, mod)


# per-module flags (clang-cl syntax; the optimisation level comes from the build's --opt). Every module is linked
# into the one exe: the static runtime (-MT) and EE_STATIC (no dllexport/dllimport). The other defines select the
# header views the modules were reconstructed with (the engine's, the plug-ins', the game's).
MODULE_FLAGS = {
    "tcp": "-MT -GR -DEE_STATIC",
    "mixer": "-MT -GR -DEE_STATIC",
    "default": "-MT -GR -DEE_STATIC",
    "lle": "-MT -GR -DEE_STATIC -DLOWLEVELENGINE_EXPORTS -DDM_STATIC",
    "dx7hrtnl": "-MT -GR -DEE_STATIC -DLLE_CURRENT_HEADERS",
    "exe": "-MT -GR -DEE_STATIC -DLLE_CURRENT_HEADERS -DLLE_EXE_HEADERS",
    "jpeg": "-MT",
    "exeimg": "-MT",
}
MODULE_FILES = {m: "EE-AOC.exe" for m in MODULE_FLAGS}

# ---- toolchain (LLVM, the MSVC STL and the Windows SDK of VS 2022 Build Tools; env overrides) ----
LLVM = pathlib.Path(os.environ.get("EE_LLVM", r"C:\Program Files\LLVM\bin"))
CLANG = LLVM / "clang-cl.exe"
LLD_LINK = LLVM / "lld-link.exe"
LLVM_LIB = LLVM / "llvm-lib.exe"
LLVM_NM = LLVM / "llvm-nm.exe"


def _newest(d):
    subs = [p for p in pathlib.Path(d).iterdir() if p.is_dir()] if pathlib.Path(d).exists() else []
    if not subs:
        raise SystemExit(f"nothing in {d}: install VS 2022 Build Tools with the C++ workload (see README)")
    return max(subs, key=lambda p: [int(x) if x.isdigit() else 0 for x in p.name.split(".")])


_VS = None


def _vs_install():
    """The newest Visual Studio installation with the x86/x64 C++ tools (vswhere; any edition: Build Tools here,
    Enterprise on CI runners)."""
    global _VS
    if _VS is None:
        import subprocess
        vswhere = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
        path = ""
        if vswhere.exists():
            path = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                                   "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                                  capture_output=True, text=True).stdout.strip()
        if not path:
            raise SystemExit("no Visual Studio with the C++ x86/x64 tools: install VS 2022 Build Tools with the C++ "
                             "workload (README), or set EE_MSVC_DIR")
        _VS = pathlib.Path(path.splitlines()[0])
    return _VS


def modern_dirs():
    """(MSVC tools dir, Windows SDK include dir, Windows SDK lib dir, Windows SDK bin dir)."""
    msvc = os.environ.get("EE_MSVC_DIR")
    if not msvc:
        msvc = _newest(_vs_install() / "VC" / "Tools" / "MSVC")
    msvc = pathlib.Path(msvc)
    kits = pathlib.Path(os.environ.get("EE_WINSDK_ROOT", r"C:\Program Files (x86)\Windows Kits\10"))
    sdk_inc = pathlib.Path(os.environ.get("EE_WINSDK_DIR") or _newest(kits / "Include"))
    return msvc, sdk_inc, kits / "Lib" / sdk_inc.name, kits / "bin" / sdk_inc.name


def lib_paths():
    """The /LIBPATH: arguments for the MSVC runtime and the Windows SDK (x86)."""
    msvc, _, sdk_lib, _ = modern_dirs()
    return [f"/LIBPATH:{msvc / 'lib' / 'x86'}", f"/LIBPATH:{sdk_lib / 'ucrt' / 'x86'}",
            f"/LIBPATH:{sdk_lib / 'um' / 'x86'}"]


# the static runtime: MSVC CRT + vcruntime + UCRT + the MSVC STL (+ kernel32, which their startup code imports)
RUNTIME_LIBS = ["libcmt.lib", "libvcruntime.lib", "libucrt.lib", "libcpmt.lib", "oldnames.lib", "kernel32.lib"]
# system DLLs the images import, linked against the Windows SDK's import libraries (by DLL stem, upper case)
SDK_IMPORT_LIBS = {n: n.lower() + ".lib" for n in ("KERNEL32", "USER32", "GDI32", "ADVAPI32", "OLE32", "WINMM",
                                                    "IMM32", "WS2_32", "SHELL32", "WSOCK32", "IPHLPAPI", "DDRAW")}
# DLLs the original images imported that are not linked at all: the VC6 build's Dinkum C++ library (the MSVC STL
# replaces it), the VC6 CRT (the static runtime replaces it), and the game's own DLLs (linked into the exe)
DROPPED_IMPORTS = {"SXLRT233", "MSVCRT", "LOW-LEVEL ENGINE", "TCP PROTOCOL", "MILES SOUND SYSTEM MIXER", "DEFAULT",
                   "DX7HRDISPLAY", "DX7HRTNLDISPLAY"}


def module_filename(mod):
    if mod not in MODULE_FILES:
        raise SystemExit(f"unknown module {mod!r}; known: {', '.join(MODULE_FILES)}")
    return MODULE_FILES[mod]


def module_units(mod):
    uf = LINK_DIR / mod / "units.txt"
    if not uf.exists():
        return [(None, None, p) for p in sorted(module_src(mod).glob("*.cpp"))]
    units = []
    for line in open(uf):
        line = line.split("#", 1)[0].strip()
        if line:
            lo, hi, name = line.split(None, 2)
            units.append((int(lo, 16), int(hi, 16), module_src(mod) / name.strip()))
    return units
