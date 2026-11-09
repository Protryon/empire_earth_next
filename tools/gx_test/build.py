"""Build and run the GX self-test (gx_test.cpp + the renderer's GX layer) against the pinned wgpu-native.

The window comes from the platform layer on SDL3 (src/Platform/SDL/PlatWindow.cpp; tools/xbuild.py sdl3) and the
text from PlatFont with the built-in fonts (src/Platform/Common; toolchain/fonts, embedded with #embed).

  python -I tools/gx_test/build.py [--run] [-- test arguments]

-> build/gx_test/x86_64/gx_test.exe (clang-cl, lld-link, static runtime as the game).
"""
import argparse
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "wgpu_probe"))
from ee import CLANG, LLD_LINK, TOOLCHAIN, modern_dirs, sdl3_dir   # noqa: E402
import rust_build   # noqa: E402
from build import LIBS, TARGETS   # noqa: E402  (tools/wgpu_probe/build.py)

GX = ROOT / "src" / "Low-Level Engine" / "Graphics Engine" / "DX7 Display Plug-ins" / "GX"
PLATFORM = ROOT / "src" / "Platform"
SOURCES = [HERE / "gx_test.cpp", GX / "GX.cpp", GX / "GXDevice.cpp", PLATFORM / "SDL" / "PlatWindow.cpp",
           *[PLATFORM / "Win32" / f for f in ("PlatMessage.cpp", "PlatString.cpp", "PlatTime.cpp")],   # PlatThread: in ee_rust
           PLATFORM / "Common" / "PlatFont.cpp", PLATFORM / "Common" / "PlatFontData.c", PLATFORM / "Common" / "PlatSim.cpp"]
# what SDL3 (static) imports besides LIBS
SDL_LIBS = ["setupapi.lib", "version.lib", "cfgmgr32.lib", "imm32.lib"]


def build(arch):
    triple, libarch, machine = TARGETS[arch]
    wgpu = rust_build.WGPU_HEADERS
    rust_lib = rust_build.ensure("windows-x86_64")
    msvc, sdk_inc, sdk_lib, _ = modern_dirs()
    out = ROOT / "build" / "gx_test" / arch
    out.mkdir(parents=True, exist_ok=True)
    sdl3 = sdl3_dir()
    if not (sdl3 / "lib" / "SDL3-static.lib").exists():
        sys.exit(f"{sdl3} is missing: python -I tools/xbuild.py sdl3")
    inc = os.pathsep.join([str(ROOT / "include"), str(GX), str(wgpu / "include"), str(sdl3 / "include"),
                           str(ROOT / "src" / "ThirdParty" / "stb"), str(msvc / "include"),
                           str(sdk_inc / "ucrt"), str(sdk_inc / "um"), str(sdk_inc / "shared")])
    objs = []
    for src in SOURCES:
        obj = out / (src.stem + ".obj")
        lang = ["/std:c11", "/clang:-Wno-c23-extensions", f"/clang:--embed-dir={TOOLCHAIN / 'fonts'}"] \
            if src.suffix == ".c" else ["/EHsc", "/std:c++17"]
        r = subprocess.run([str(CLANG), f"--target={triple}", "/nologo", "/c", "/O1", "/MT", "/GS-", *lang,
                            "/W3", str(src), f"/Fo{obj}"],
                           env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"{arch}: {src.name} failed\n{r.stdout}{r.stderr}")
        if r.stdout.strip() or r.stderr.strip():
            print(f"{arch}: {src.name}:\n{(r.stdout + r.stderr).strip()[:3000]}")
        objs.append(obj)
    exe = out / "gx_test.exe"
    cmd = [str(LLD_LINK), "/nologo", f"/MACHINE:{machine}", "/SUBSYSTEM:CONSOLE", "/NODEFAULTLIB", "/INCREMENTAL:NO",
           "/OPT:REF", f"/OUT:{exe}", *map(str, objs), str(rust_lib),
           str(sdl3 / "lib" / "SDL3-static.lib"), "libcpmt.lib", *LIBS, *SDL_LIBS,
           f"/LIBPATH:{msvc / 'lib' / libarch}", f"/LIBPATH:{sdk_lib / 'ucrt' / libarch}",
           f"/LIBPATH:{sdk_lib / 'um' / libarch}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{arch}: link failed\n{(r.stdout + r.stderr)[-3000:]}")
    print(f"{arch}: {exe}")
    return exe


def main():
    argv = sys.argv[1:]
    test_args = argv[argv.index("--") + 1:] if "--" in argv else []
    argv = argv[:argv.index("--")] if "--" in argv else argv
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--run", action="store_true")
    a = ap.parse_args(argv)
    failed = 0
    for arch in TARGETS:
        exe = build(arch)
        if a.run:
            r = subprocess.run([str(exe), *test_args], capture_output=True, text=True, timeout=300,
                               env=dict(os.environ, EE_GX_LOG="1"))
            print(r.stdout.strip() + ("\n" + r.stderr.strip()[-4000:] if r.stderr.strip() else ""))
            failed |= r.returncode != 0
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
