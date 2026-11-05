"""Build and run the GX self-test (gx_test.cpp + the renderer's GX layer) against the pinned wgpu-native.

  python -I tools/gx_test/build.py [--run] [-- test arguments]

-> build/gx_test/x86_64/gx_test.exe (clang-cl, lld-link, static runtime as the game). Needs tools/fetch_wgpu.py.
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
from ee import CLANG, LLD_LINK, TOOLCHAIN, modern_dirs   # noqa: E402
from build import LIBS, TARGETS   # noqa: E402  (tools/wgpu_probe/build.py)

GX = ROOT / "src" / "Low-Level Engine" / "Graphics Engine" / "DX7 Display Plug-ins" / "GX"
SOURCES = [HERE / "gx_test.cpp", GX / "GX.cpp", GX / "GXDevice.cpp"]


def build(arch):
    triple, libarch, machine = TARGETS[arch]
    wgpu = TOOLCHAIN / "wgpu" / arch
    msvc, sdk_inc, sdk_lib, _ = modern_dirs()
    out = ROOT / "build" / "gx_test" / arch
    out.mkdir(parents=True, exist_ok=True)
    inc = os.pathsep.join([str(ROOT / "include"), str(GX), str(wgpu / "include"), str(msvc / "include"),
                           str(sdk_inc / "ucrt"), str(sdk_inc / "um"), str(sdk_inc / "shared")])
    objs = []
    for src in SOURCES:
        obj = out / (src.stem + ".obj")
        r = subprocess.run([str(CLANG), f"--target={triple}", "/nologo", "/c", "/O1", "/MT", "/GS-", "/EHsc",
                            "/std:c++17", "/W3", str(src), f"/Fo{obj}"],
                           env=dict(os.environ, INCLUDE=inc), capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"{arch}: {src.name} failed\n{r.stdout}{r.stderr}")
        if r.stdout.strip() or r.stderr.strip():
            print(f"{arch}: {src.name}:\n{(r.stdout + r.stderr).strip()[:3000]}")
        objs.append(obj)
    exe = out / "gx_test.exe"
    cmd = [str(LLD_LINK), "/nologo", f"/MACHINE:{machine}", "/SUBSYSTEM:CONSOLE", "/NODEFAULTLIB", "/INCREMENTAL:NO",
           "/OPT:REF", f"/OUT:{exe}", *map(str, objs), str(wgpu / "lib" / "wgpu_native.lib"), "libcpmt.lib", *LIBS,
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
