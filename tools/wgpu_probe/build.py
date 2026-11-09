"""Build and run the wgpu phase-0 probe (probe.c) for x86_64 against wgpu-native (in the Rust library, tools/rust_build.py).

  python -I tools/wgpu_probe/build.py [--run] [-- probe arguments]

Compiles with clang-cl and links with lld-link and the static C runtime (/MT, as the game), into
build/wgpu_probe/x86_64/probe.exe; --run runs it (arguments after --).
"""
import argparse
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from ee import CLANG, LLD_LINK, TOOLCHAIN, modern_dirs   # noqa: E402
import rust_build   # noqa: E402

TARGETS = {"x86_64": ("x86_64-pc-windows-msvc", "x64", "X64")}
# the static runtime, then what the Rust standard library and wgpu's DX12/Vulkan/GL backends link against
LIBS = ["libcmt.lib", "libvcruntime.lib", "libucrt.lib", "oldnames.lib", "kernel32.lib", "user32.lib", "gdi32.lib",
        "advapi32.lib", "ws2_32.lib", "iphlpapi.lib", "userenv.lib", "ntdll.lib", "bcrypt.lib", "ole32.lib", "oleaut32.lib",
        "d3d12.lib", "dxgi.lib", "d3dcompiler.lib", "opengl32.lib", "propsys.lib", "runtimeobject.lib", "uuid.lib",
        "synchronization.lib", "shell32.lib", "dwmapi.lib", "winmm.lib"]


def build(arch):
    triple, libarch, machine = TARGETS[arch]
    wgpu = rust_build.WGPU_HEADERS
    rust_lib = rust_build.ensure("windows-x86_64")
    msvc, sdk_inc, sdk_lib, _ = modern_dirs()
    out = ROOT / "build" / "wgpu_probe" / arch
    out.mkdir(parents=True, exist_ok=True)
    inc = os.pathsep.join([str(wgpu / "include"), str(msvc / "include"), str(sdk_inc / "ucrt"), str(sdk_inc / "um"),
                           str(sdk_inc / "shared")])
    obj = out / "probe.obj"
    r = subprocess.run([str(CLANG), f"--target={triple}", "/nologo", "/c", "/O1", "/MT", "/GS-", "-Wno-everything",
                        str(HERE / "probe.c"), f"/Fo{obj}"], env=dict(os.environ, INCLUDE=inc),
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"{arch}: compile failed\n{r.stdout}{r.stderr}")
    exe = out / "probe.exe"
    cmd = [str(LLD_LINK), "/nologo", f"/MACHINE:{machine}", "/SUBSYSTEM:CONSOLE", "/NODEFAULTLIB", "/INCREMENTAL:NO",
           "/OPT:REF", f"/OUT:{exe}", str(obj), str(rust_lib), *LIBS,
           f"/LIBPATH:{msvc / 'lib' / libarch}", f"/LIBPATH:{sdk_lib / 'ucrt' / libarch}",
           f"/LIBPATH:{sdk_lib / 'um' / libarch}"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    log = (r.stdout + r.stderr).strip()
    (out / "link.log").write_text(log)
    if r.returncode:
        undefined = sorted({ln.split("undefined symbol: ")[1] for ln in log.splitlines() if "undefined symbol: " in ln})
        sys.exit(f"{arch}: link failed ({len(undefined)} undefined): {undefined[:40]}\n{log[-1500:]}")
    print(f"{arch}: {exe} ({exe.stat().st_size >> 10} KB)")
    return exe


def main():
    argv = sys.argv[1:]
    probe_args = argv[argv.index("--") + 1:] if "--" in argv else []
    argv = argv[:argv.index("--")] if "--" in argv else argv
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--run", action="store_true")
    a = ap.parse_args(argv)
    failed = 0
    for arch in TARGETS:
        exe = build(arch)
        if a.run:
            r = subprocess.run([str(exe), *probe_args], capture_output=True, text=True, timeout=120)
            print(r.stdout.strip() + ("\n" + r.stderr.strip() if r.stderr.strip() else ""))
            failed |= r.returncode != 0
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
