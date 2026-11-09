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
    "won": "Empire Earth/WONAPI",
}
EEX_INCLUDE_DIRS = ["Low-Level Engine/Thread System", "Low-Level Engine/Graphics Engine",
                    "Empire Earth/Database", "Empire Earth/AI Thread"]


def module_src(mod):
    return ROOT / "src" / MODULE_SRC.get(mod, mod)


# per-module flags (clang-cl syntax; the optimisation level comes from the build's --opt). Every module is linked
# into the one exe: the static runtime (-MT) and EE_STATIC (no dllexport/dllimport). The other defines select the
# header views the modules were reconstructed with (the engine's, the plug-ins', the game's). FP: no floating-point
# contraction (no fused multiply-add), so a float expression rounds the same on every target (docs/RUST_PLAN.md 2.1;
# tools/xbuild.py has the POSIX builds' -ffp-contract=off).
FP = "/clang:-ffp-contract=off"
MODULE_FLAGS = {
    "tcp": f"-MT -GR {FP} -DEE_STATIC",
    "mixer": f"-MT -GR {FP} -DEE_STATIC",
    "default": f"-MT -GR {FP} -DEE_STATIC",
    "lle": f"-MT -GR {FP} -DEE_STATIC -DLOWLEVELENGINE_EXPORTS -DDM_STATIC",
    "dx7hrtnl": f"-MT -GR {FP} -DEE_STATIC -DLLE_CURRENT_HEADERS",
    "exe": f"-MT -GR {FP} -DEE_STATIC -DLLE_CURRENT_HEADERS -DLLE_EXE_HEADERS",
    "jpeg": f"-MT {FP}",
    "exeimg": f"-MT {FP}",
    # the WON library (online lobby, routing, the lobby GUI toolkit): C++ exceptions on (it was built -GX)
    "won": f"-MT -GR -EHsc {FP} -DEE_STATIC",
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


# the one target of the build: x86_64 (the MSVC and Windows SDK library directories are named "x64")
ARCH = "x64"


def wgpu_dir():
    """wgpu-native x86_64 (tools/fetch_wgpu.py): toolchain/wgpu/x86_64."""
    return TOOLCHAIN / "wgpu" / "x86_64"


def sdl3_dir():
    """SDL3 3.4.18, static, for windows-x86_64 (built from the pinned sources by tools/xbuild.py sdl3; EE_SDL3
    overrides): <dir>/include, <dir>/lib/SDL3-static.lib."""
    return pathlib.Path(os.environ.get("EE_SDL3", ROOT / "build" / "xbuild" / "windows-x86_64" / "sdl3"))


def lib_paths():
    """The /LIBPATH: arguments for the x64 MSVC runtime and Windows SDK libraries."""
    msvc, _, sdk_lib, _ = modern_dirs()
    return [f"/LIBPATH:{msvc / 'lib' / ARCH}", f"/LIBPATH:{sdk_lib / 'ucrt' / ARCH}",
            f"/LIBPATH:{sdk_lib / 'um' / ARCH}"]


# the static runtime: MSVC CRT + vcruntime + UCRT + the MSVC STL (+ kernel32, which their startup code imports)
RUNTIME_LIBS = ["libcmt.lib", "libvcruntime.lib", "libucrt.lib", "libcpmt.lib", "oldnames.lib", "kernel32.lib"]


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
