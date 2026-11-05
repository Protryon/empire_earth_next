"""Cross-platform builds: the platform layer's tests, the portable test programs and the Linux/macOS compile census.

  python -I tools/xbuild.py test     [--target T] [--no-run] [--no-network]     platform layer tests (tests/platform)
  python -I tools/xbuild.py fonttest [--target T] [--no-run]                    text rasteriser test (PlatFont)
  python -I tools/xbuild.py sdl3     [--target T]                               SDL3 as a static library (CMake)
  python -I tools/xbuild.py sdltest  [--target T] [--no-run] [--frames N]       SDL3 window + wgpu surface test
  python -I tools/xbuild.py census   [--target T] [--dialect ms|gnu] [-j N] [--only TEXT] [module ...]
  python -I tools/xbuild.py targets                                             what each target needs, and if it is here

Targets (--target, default: the host):
  windows-x86_64   clang-cl, the MSVC runtime and the Windows SDK (VS 2022 Build Tools; tools/ee.py), src/Platform/Win32
  linux-x86_64     clang --target=x86_64-linux-gnu: native on Linux, else against the Debian sysroot that
                   tools/fetch_posix.py fetches (cross-compiles and links here; runs only on Linux); src/Platform/Posix
  macos-arm64      clang -arch arm64 / x86_64 with the Xcode SDK: native on macOS only (the SDK may not be copied to
  macos-x86_64     another system: see PORT_LEARNINGS.md "Linux and macOS"); src/Platform/Posix

The census compiles every unit of the game (tools/units.py, the same list as the Windows build) for a POSIX target
with -fsyntax-only and include/compat/ in front (the Windows-only headers: <windows.h> -> platform/wintypes.h, types
and macros only, and the MSVC runtime headers on the platform layer). It writes <out>/census.tsv (unit, result,
first error) and <out>/census_errors.tsv (every error), ranks the blockers (missing names by the units they stop,
error classes) and prints the units that compile. --dialect ms keeps the Microsoft language extensions the Windows
build uses (-fms-extensions -fms-compatibility -fdelayed-template-parsing); gnu drops them.

Outputs go to build/xbuild/<target>/ (env EE_XBUILD_OUT overrides). Nothing here touches tools/build.py's outputs.
"""
import argparse
import concurrent.futures
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ee  # noqa: E402  (paths and the Windows toolchain)

ROOT = ee.ROOT
TC = ee.TOOLCHAIN
OUT = pathlib.Path(os.environ.get("EE_XBUILD_OUT", ROOT / "build" / "xbuild"))
IS_WINDOWS = sys.platform == "win32"
IS_LINUX = sys.platform.startswith("linux")
IS_MACOS = sys.platform == "darwin"

PLATFORM_HEADERS = ROOT / "include"
POSIX_IMPL = ["PlatTime.cpp", "PlatThread.cpp", "PlatFile.cpp", "PlatPath.cpp", "PlatPreferences.cpp",
              "PlatString.cpp", "PlatSocket.cpp", "PlatMessage.cpp"]
WIN32_IMPL = ["PlatTime.cpp", "PlatThread.cpp", "PlatFile.cpp", "PlatPath.cpp", "PlatPreferences.cpp",
              "PlatString.cpp", "PlatSocket.cpp", "PlatMessage.cpp"]
COMMON_IMPL = ["PlatFont.cpp"]   # src/Platform/Common: the same code on every system
WIN32_SYSLIBS = ["kernel32.lib", "user32.lib", "advapi32.lib", "ole32.lib", "winmm.lib", "ws2_32.lib",
                 "iphlpapi.lib", "shell32.lib", "gdi32.lib", "imm32.lib", "version.lib", "setupapi.lib",
                 "oleaut32.lib", "uuid.lib", "cfgmgr32.lib", "propsys.lib", "d3d12.lib", "dxgi.lib",
                 "d3dcompiler.lib", "opengl32.lib", "ntdll.lib", "bcrypt.lib", "userenv.lib", "dwmapi.lib",
                 "runtimeobject.lib", "synchronization.lib", "ws2_32.lib"]

# ---- targets


class Target:
    def __init__(self, name):
        self.name = name
        self.os = name.split("-")[0]
        self.arch = name.split("-")[1]
        self.out = OUT / name

    @property
    def posix(self):
        return self.os in ("linux", "macos")

    @property
    def exe_suffix(self):
        return ".exe" if self.os == "windows" else ""

    def native(self):
        """True when this machine runs the target's programs."""
        host = host_target_name()
        if host == self.name:
            return True
        return IS_MACOS and self.os == "macos" and self.arch == "x86_64" and platform.machine() == "arm64" \
            and shutil.which("arch") is not None and os.environ.get("EE_XBUILD_ROSETTA") == "1"

    # --- the compiler
    def sysroot(self):
        if self.os == "linux" and not IS_LINUX:
            root = TC / "sysroot" / "debian_bullseye_amd64"
            if not root.exists():
                sys.exit(f"{root} is missing: python -I tools/fetch_posix.py sysroot")
            return root
        return None

    def clang(self, cxx=True):
        if self.os == "windows":
            return str(ee.CLANG)
        env = os.environ.get("CXX" if cxx else "CC")
        if env:
            return env
        if IS_WINDOWS:
            return str(ee.LLVM / ("clang++.exe" if cxx else "clang.exe"))
        return "clang++" if cxx else "clang"

    def base_flags(self):
        """Target flags for a POSIX compile (no language or warning flags)."""
        if self.os == "linux":
            f = ["--target=x86_64-linux-gnu"]
            root = self.sysroot()
            if root:
                f += [f"--sysroot={root}"]
            return f
        if self.os == "macos":
            arch = "arm64" if self.arch == "arm64" else "x86_64"
            f = ["-arch", arch, "-mmacosx-version-min=" + ("11.0" if arch == "arm64" else "10.15")]
            if IS_MACOS:
                sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True).stdout.strip()
                if sdk:
                    f += ["-isysroot", sdk]
            else:
                f = [f"--target={'arm64' if arch == 'arm64' else 'x86_64'}-apple-macos11"]
            return f
        raise SystemExit(f"{self.name}: not a POSIX target")

    def link_flags(self):
        if self.os == "linux":
            f = self.base_flags()
            if not IS_LINUX:
                f += ["-fuse-ld=lld"]
            return f + ["-pthread"]
        return self.base_flags()

    def can_build(self):
        """'' when this machine can build for the target, else why not."""
        if self.os == "windows":
            return "" if IS_WINDOWS else "Windows builds run on Windows only"
        if self.os == "macos" and not IS_MACOS:
            return "macOS builds need the Xcode SDK: they run on macOS (CI: macos-latest)"
        if self.os == "linux" and not IS_LINUX and not (TC / "sysroot" / "debian_bullseye_amd64").exists():
            return "no Linux sysroot: python -I tools/fetch_posix.py sysroot"
        return ""


def host_target_name():
    m = platform.machine().lower()
    arch = "arm64" if m in ("arm64", "aarch64") else "x86_64"
    if IS_WINDOWS:
        return "windows-x86_64"
    if IS_MACOS:
        return f"macos-{arch}"
    return f"linux-{arch}"


TARGETS = ["windows-x86_64", "linux-x86_64", "macos-arm64", "macos-x86_64"]


def run(cmd, env=None, cwd=None, quiet=False):
    if not quiet:
        print("  " + " ".join(str(c) for c in cmd[:6]) + (" ..." if len(cmd) > 6 else ""), flush=True)
    r = subprocess.run([str(c) for c in cmd], env=env, cwd=cwd, capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    if r.returncode:
        sys.stdout.write(r.stdout + r.stderr)
        sys.exit(f"failed ({r.returncode}): {cmd[0]}")
    return r


def windows_env():
    """INCLUDE / LIB for clang-cl and lld-link (x64): the MSVC runtime and the Windows SDK."""
    msvc, sdk, sdk_lib, _ = ee.modern_dirs()
    env = dict(os.environ)
    env["INCLUDE"] = os.pathsep.join([str(msvc / "include"), str(sdk / "ucrt"), str(sdk / "um"), str(sdk / "shared"),
                                      str(sdk / "winrt")])
    env["LIB"] = os.pathsep.join([str(msvc / "lib" / "x64"), str(sdk_lib / "ucrt" / "x64"),
                                  str(sdk_lib / "um" / "x64")])
    return env


WIN_CXX = ["--target=x86_64-pc-windows-msvc", "/nologo", "/std:c++17", "/EHsc", "/MT", "/O1", "/GS-", "/W3",
           "-Wno-deprecated-declarations"]
POSIX_CXX = ["-std=c++17", "-O1", "-g", "-Wall", "-Wno-unused-function"]


def build_program(target, name, sources, includes=(), defines=(), libs=(), extra_link=(), c_sources=()):
    """Compiles and links one test program -> its path."""
    out = target.out / name
    out.mkdir(parents=True, exist_ok=True)
    exe = out / (name + target.exe_suffix)
    objs = []
    if target.os == "windows":
        env = windows_env()
        for src in [*sources, *c_sources]:
            obj = out / (pathlib.Path(src).stem + ".obj")
            is_c = str(src).endswith(".c")
            flags = [f for f in WIN_CXX if not (is_c and f.startswith("/std"))]
            run([target.clang(), *flags, "/c", *[f"/I{i}" for i in includes], *[f"/D{d}" for d in defines],
                 f"/Fo{obj}", src], env=env)
            objs.append(obj)
        run([ee.LLD_LINK, "/nologo", f"/OUT:{exe}", "/SUBSYSTEM:CONSOLE", "/DEBUG", *objs, *libs,
             "libcmt.lib", "libvcruntime.lib", "libucrt.lib", "libcpmt.lib", "oldnames.lib", *WIN32_SYSLIBS,
             *extra_link], env=env)
        return exe
    base = target.base_flags()
    for src in [*sources, *c_sources]:
        obj = out / (pathlib.Path(src).stem + ".o")
        is_c = str(src).endswith(".c")
        flags = ["-std=c11", "-O1", "-g"] if is_c else POSIX_CXX
        run([target.clang(cxx=not is_c), *base, *flags, "-c", *[f"-I{i}" for i in includes],
             *[f"-D{d}" for d in defines], "-o", obj, src])
        objs.append(obj)
    run([target.clang(), *target.link_flags(), "-o", exe, *objs, *libs, *extra_link])
    return exe


def platform_sources(target):
    impl = ROOT / "src" / "Platform" / ("Win32" if target.os == "windows" else "Posix")
    names = WIN32_IMPL if target.os == "windows" else POSIX_IMPL
    return [impl / n for n in names] + [ROOT / "src" / "Platform" / "Common" / n for n in COMMON_IMPL]


def run_program(target, exe, args=(), env=None):
    if not target.native():
        print(f"{exe.name}: built for {target.name}; this machine cannot run it (not run)")
        return 0
    print(f"running {exe.name} {' '.join(args)}", flush=True)
    r = subprocess.run([str(exe), *args], env=env)
    print(f"{exe.name}: exit {r.returncode}")
    return r.returncode


# ---- test / fonttest


def cmd_test(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    srcs = [*platform_sources(t), ROOT / "tests" / "platform" / "plat_test.cpp"]
    libs = [] if t.os == "windows" else (["-ldl"] if t.os == "linux" else [])
    exe = build_program(t, "plat_test", srcs, includes=[PLATFORM_HEADERS, ROOT / "src" / "ThirdParty" / "stb"],
                        libs=libs)
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe, ["--no-network"] if a.no_network else [])


def font_dir():
    d = TC / "fonts"
    if not (d / "LiberationSans-Regular.ttf").exists():
        sys.exit(f"{d} has no Liberation fonts: python -I tools/fetch_posix.py fonts")
    return d


def cmd_fonttest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    srcs = [ROOT / "src" / "Platform" / "Common" / "PlatFont.cpp", ROOT / "tests" / "platform" / "font_test.cpp"]
    exe = build_program(t, "font_test", srcs, includes=[PLATFORM_HEADERS, ROOT / "src" / "ThirdParty" / "stb"])
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe, [str(font_dir())])


# ---- SDL3


SDL_VERSION = "3.4.18"


def sdl_source():
    d = TC / "src" / f"SDL3-{SDL_VERSION}"
    if not (d / "CMakeLists.txt").exists():
        sys.exit(f"{d} is missing: python -I tools/fetch_posix.py sdl3")
    return d


def sdl_prefix(t):
    return t.out / "sdl3"


def cmake_exe():
    for c in (os.environ.get("EE_CMAKE"), shutil.which("cmake"), r"C:\Program Files\CMake\bin\cmake.exe"):
        if c and pathlib.Path(c).exists():
            return c
    sys.exit("cmake not found (winget install Kitware.CMake; apt install cmake; brew install cmake)")


def ninja_exe():
    for c in (os.environ.get("EE_NINJA"), shutil.which("ninja"),
              os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Links\ninja.exe")):
        if c and pathlib.Path(c).exists():
            return c
    return None


# SDL subsystems the game uses: video (window, events, keyboard/mouse, clipboard, text input/IME), the message box.
# Audio stays on miniaudio, so SDL's audio, camera, haptic, joystick, sensor, HID and GPU are off; X11 and Wayland
# (Linux), Cocoa/Metal (macOS) and Win32 come with video.
SDL_OPTIONS = ["-DSDL_STATIC=ON", "-DSDL_SHARED=OFF", "-DSDL_TEST_LIBRARY=OFF", "-DSDL_TESTS=OFF",
               "-DSDL_EXAMPLES=OFF", "-DSDL_INSTALL=ON", "-DSDL_AUDIO=OFF", "-DSDL_CAMERA=OFF", "-DSDL_HAPTIC=OFF",
               "-DSDL_JOYSTICK=OFF", "-DSDL_SENSOR=OFF", "-DSDL_HIDAPI=OFF", "-DSDL_POWER=OFF", "-DSDL_GPU=OFF",
               "-DSDL_RENDER=OFF", "-DSDL_DIALOG=OFF", "-DSDL_TRAY=OFF", "-DCMAKE_BUILD_TYPE=Release",
               "-DSDL_DISABLE_INSTALL_DOCS=ON"]


def cmd_sdl3(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    src = sdl_source()
    build = t.out / "sdl3-build"
    prefix = sdl_prefix(t)
    cmake = cmake_exe()
    # CMake reads backslashes in -D values as escapes: forward slashes throughout
    gen = ["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={pathlib.Path(ninja_exe()).as_posix()}"] if ninja_exe() else []
    if (build / "CMakeCache.txt").exists() and a.fresh:
        shutil.rmtree(build)
    args = [cmake, "-S", src, "-B", build, *gen, f"-DCMAKE_INSTALL_PREFIX={prefix.as_posix()}", *SDL_OPTIONS]
    env = dict(os.environ)
    if t.os == "windows":
        env = windows_env()
        args += [f"-DCMAKE_C_COMPILER={ee.CLANG.as_posix()}", f"-DCMAKE_CXX_COMPILER={ee.CLANG.as_posix()}",
                 f"-DCMAKE_LINKER={ee.LLD_LINK.as_posix()}", f"-DCMAKE_AR={ee.LLVM_LIB.as_posix()}",
                 "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded", "-DCMAKE_C_FLAGS=--target=x86_64-pc-windows-msvc",
                 "-DCMAKE_RC_COMPILER=" + (ee.LLVM / "llvm-rc.exe").as_posix(), "-DSDL_LIBC=ON"]
    elif t.os == "linux" and not IS_LINUX:
        tcf = build.parent / "linux-x86_64.toolchain.cmake"
        build.parent.mkdir(parents=True, exist_ok=True)
        root = t.sysroot().as_posix()
        llvm = ee.LLVM.as_posix()
        tcf.write_text(f"""set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSROOT "{root}")
set(CMAKE_C_COMPILER "{llvm}/clang.exe")
set(CMAKE_CXX_COMPILER "{llvm}/clang++.exe")
set(CMAKE_C_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_AR "{llvm}/llvm-ar.exe")
set(CMAKE_RANLIB "{llvm}/llvm-ranlib.exe")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
""")
        # no pkg-config and no wayland-scanner on Windows: X11 only in a cross build (CI builds Wayland too)
        args += [f"-DCMAKE_TOOLCHAIN_FILE={tcf}", "-DSDL_WAYLAND=OFF", "-DSDL_X11=ON", "-DSDL_X11_SHARED=ON",
                 "-DSDL_PIPEWIRE=OFF", "-DSDL_PULSEAUDIO=OFF", "-DSDL_ALSA=OFF", "-DSDL_JACK=OFF",
                 "-DSDL_SNDIO=OFF", "-DSDL_DBUS=OFF", "-DSDL_IBUS=OFF", "-DSDL_LIBUDEV=OFF",
                 "-DSDL_KMSDRM=OFF", "-DSDL_OPENGL=ON", "-DSDL_VULKAN=ON"]
    elif t.os == "macos":
        args += [f"-DCMAKE_OSX_ARCHITECTURES={'arm64' if t.arch == 'arm64' else 'x86_64'}",
                 "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + ("11.0" if t.arch == "arm64" else "10.15")]
    run(args, env=env)
    run([cmake, "--build", build, "--config", "Release", "-j", str(a.j)], env=env)
    run([cmake, "--install", build, "--config", "Release"], env=env)
    print(f"SDL3 {SDL_VERSION} (static) -> {prefix}")
    return 0


def sdl_libs(t):
    prefix = sdl_prefix(t)
    if t.os == "windows":
        lib = prefix / "lib" / "SDL3-static.lib"
        if not lib.exists():
            sys.exit(f"{lib} is missing: python -I tools/xbuild.py sdl3 --target {t.name}")
        return [lib]
    lib = prefix / "lib" / "libSDL3.a"
    if not lib.exists():
        lib64 = prefix / "lib64" / "libSDL3.a"
        if lib64.exists():
            lib = lib64
        else:
            sys.exit(f"{lib} is missing: python -I tools/xbuild.py sdl3 --target {t.name}")
    if t.os == "linux":
        return [lib, "-ldl", "-lm", "-lpthread", "-lrt"]
    return [lib, "-framework", "Cocoa", "-framework", "IOKit", "-framework", "CoreVideo", "-framework", "Metal",
            "-framework", "QuartzCore", "-framework", "Carbon", "-framework", "ForceFeedback",
            "-framework", "GameController", "-framework", "CoreHaptics", "-framework", "UniformTypeIdentifiers",
            "-framework", "AVFoundation", "-framework", "CoreMedia", "-framework", "CoreAudio",
            "-framework", "AudioToolbox", "-liconv"]


def wgpu_dir(t):
    name = {"windows-x86_64": "x86_64", "linux-x86_64": "linux-x86_64", "macos-arm64": "macos-aarch64",
            "macos-x86_64": "macos-x86_64"}[t.name]
    d = TC / "wgpu" / name
    lib = d / "lib" / ("wgpu_native.lib" if t.os == "windows" else "libwgpu_native.a")
    if not lib.exists():
        sys.exit(f"{lib} is missing: python -I tools/" + ("fetch_wgpu.py" if t.os == "windows" else
                                                           "fetch_posix.py wgpu"))
    return d, lib


def cmd_sdltest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    wgpu, wlib = wgpu_dir(t)
    # the window layer on SDL3 (src/Platform/SDL) with the platform layer it uses; on Windows the Win32 key-state
    # file (PlatInput.cpp) is the game's and stays out: the SDL file implements the same functions
    srcs = [ROOT / "tools" / "sdl_test" / "sdl_test.cpp", ROOT / "src" / "Platform" / "SDL" / "PlatWindow.cpp",
            ROOT / "src" / "Platform" / "Common" / "PlatFont.cpp",
            *[s for s in platform_sources(t) if s.name in ("PlatMessage.cpp", "PlatThread.cpp", "PlatTime.cpp")]]
    libs = [*sdl_libs(t), wlib]
    if t.os == "linux":
        libs += ["-lm", "-ldl"]
    exe = build_program(t, "sdl_test", srcs,
                        includes=[sdl_prefix(t) / "include", wgpu / "include", PLATFORM_HEADERS,
                                  ROOT / "src" / "ThirdParty" / "stb"],
                        libs=libs, defines=["SDL_MAIN_HANDLED"])
    print(f"built {exe}")
    if a.no_run:
        return 0
    args = ["--frames", str(a.frames)]
    if (TC / "fonts" / "LiberationSans-Regular.ttf").exists():
        args += ["--fonts", str(TC / "fonts")]
    return run_program(t, exe, args)


# ---- the compile census

COMPAT = ROOT / "include" / "compat"
DIAG = re.compile(r"^(.*?):(\d+):(\d+): (error|fatal error): (.*)$", re.M)
UNDECLARED = re.compile(r"use of undeclared identifier '([^']+)'|unknown type name '([^']+)'|"
                        r"no type named '([^']+)' in the global namespace|"
                        r"no member named '([^']+)' in the global namespace|"
                        r"'([^']+)' file not found|use of undeclared label '([^']+)'")


def classify(msg):
    """An error class with names masked (the census's ranking key)."""
    return re.sub(r"'[^']*'", "'X'", re.sub(r"\(aka '[^']*'\)", "", msg)).strip()


def census_flags(t, dialect):
    f = [*t.base_flags(), "-fsyntax-only", "-ferror-limit=0", "-fno-color-diagnostics", "-w",
         "-fno-strict-aliasing", "-fwrapv", "-DNDEBUG"]
    if dialect == "ms":
        f += ["-fms-extensions", "-fms-compatibility", "-fdelayed-template-parsing",
              "-fms-compatibility-version=19.44"]
    return f


def census_job(t, job, dialect, inc, logdir):
    mod, src, flags = job
    import verify   # unit_flags: the module flags plus a unit's // UNIT-FLAGS: defines
    defs = [x for x in verify.unit_flags(src, flags).split() if x.startswith(("-D", "/D"))]
    defs = ["-D" + d[2:] for d in defs]
    is_c = str(src).lower().endswith(".c")
    # the Microsoft dialect is the game's (C++ units); C code, the vendored libraries and the platform layer are
    # standard code (-fms-compatibility on a GNU target also hides __STDC__ and the __GCC_ATOMIC_* macros that zlib
    # and libstdc++'s <atomic> test)
    rel = pathlib.Path(src).resolve().relative_to(ROOT / "src").as_posix()
    if is_c or rel.startswith(("ThirdParty/", "Platform/")):
        dialect = "gnu"
    std = ["-x", "c", "-std=gnu11"] if is_c else ["-x", "c++", "-std=gnu++17" if dialect == "gnu" else "-std=c++17"]
    own = []
    p = pathlib.Path(src).resolve()
    third = ROOT / "src" / "ThirdParty"
    if p.is_relative_to(third):
        lib = third / p.relative_to(third).parts[0]
        if (lib / "include_dirs.txt").exists():
            own = [str(lib / d.strip()) for d in (lib / "include_dirs.txt").read_text().splitlines() if d.strip()]
    cmd = [t.clang(cxx=not is_c), *census_flags(t, dialect), *std, *defs, f"-I{p.parent}",
           *[f"-I{i}" for i in own], *[f"-I{i}" for i in inc], str(p)]
    r = subprocess.run(cmd, capture_output=True, text=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    log = (r.stdout + r.stderr)
    return job, r.returncode == 0, log


def cmd_census(a):
    t = Target(a.target)
    if not t.posix:
        sys.exit("the census is for a POSIX target (the Windows build is tools/build.py)")
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    import units
    mods = a.modules or units.MODULES
    jobs = [j for m in mods for j in units.jobs_for(m)] + units.lib_jobs(mods)
    jobs = [j for j in jobs if "Platform/Win32" not in units.rel_src(j[1]).as_posix()]
    if a.only:
        jobs = [j for j in jobs if a.only in str(j[1])]
    # the include path of the Windows build (verify.include_paths) minus the MSVC/SDK directories, with the compat
    # headers first; the platform layer's POSIX implementation replaces src/Platform/Win32 (compiled as units here)
    inc = [COMPAT, ROOT / "include", ROOT / "src" / "ThirdParty" / "dmusic" / "include",
           ROOT / "src" / "ThirdParty" / "stb",
           *[ROOT / "src" / d for d in ee.EEX_INCLUDE_DIRS]]
    if (TC / "wgpu" / ("linux-x86_64" if t.os == "linux" else "macos-aarch64") / "include").exists():
        inc.append(TC / "wgpu" / ("linux-x86_64" if t.os == "linux" else "macos-aarch64") / "include")
    for s in platform_sources(t):
        jobs.append(("platform", s, ""))
    out = t.out / f"census-{a.dialect}"
    out.mkdir(parents=True, exist_ok=True)
    logdir = out / "log"
    if logdir.exists():
        shutil.rmtree(logdir)
    t0 = time.time()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as ex:
        for job, ok, log in ex.map(lambda j: census_job(t, j, a.dialect, inc, logdir), jobs):
            results.append((job, ok, log))
    rows, errs = [], []
    by_class, by_name, by_header, by_file = {}, {}, {}, {}
    mod_counts = {}
    lp64_only = 0   # units whose only errors are the record-size asserts (the LP64 work, PORT.md)
    for job, ok, log in results:
        rel = units.rel_src(job[1]).as_posix()
        mc = mod_counts.setdefault(job[0], [0, 0])
        mc[0 if ok else 1] += 1
        diags = DIAG.findall(log)
        first = ""
        names, classes, files = set(), set(), set()
        for f, line, col, kind, msg in diags:
            fp = pathlib.Path(f)
            try:
                frel = fp.resolve().relative_to(ROOT).as_posix()
            except (ValueError, OSError):
                frel = f
            errs.append((rel, frel, line, msg))
            if not first:
                first = f"{pathlib.Path(frel).name}:{line}: {msg}"
            classes.add(classify(msg))
            files.add(frel)
            m = UNDECLARED.search(msg)
            if m:
                names.add(next(g for g in m.groups() if g))
            if kind == "fatal error" and "file not found" in msg:
                by_header[msg] = by_header.get(msg, 0) + 1
        for c in classes:
            by_class[c] = by_class.get(c, 0) + 1
        for n in names:
            by_name[n] = by_name.get(n, 0) + 1
        for fl in files:
            by_file[fl] = by_file.get(fl, 0) + 1
        if not ok and classes and all(c.startswith("static assertion failed") and "on-disk record" in c
                                      for c in classes):
            lp64_only += 1
        if not ok:
            (logdir / (rel.replace("/", "__") + ".log")).parent.mkdir(parents=True, exist_ok=True)
            (logdir / (rel.replace("/", "__") + ".log")).write_text(log, encoding="utf-8")
        rows.append((job[0], rel, "ok" if ok else "fail", str(len(diags)), first))
    rows.sort()
    (out / "census.tsv").write_text("".join("\t".join(r) + "\n" for r in rows), encoding="utf-8")
    (out / "census_errors.tsv").write_text("".join("\t".join(map(str, e)) + "\n" for e in sorted(set(errs))),
                                           encoding="utf-8")
    n_ok = sum(1 for r in rows if r[2] == "ok")
    print(f"\n{t.name} census ({a.dialect} dialect): {n_ok}/{len(rows)} units compile "
          f"({time.time() - t0:.0f} s) -> {out}")
    for m, (ok, bad) in sorted(mod_counts.items()):
        print(f"  {m:9} {ok:4} compile  {bad:4} fail")
    print(f"  {lp64_only} more fail only on the on-disk record size asserts (long is 64-bit: the LP64 work)")
    print("\nfiles the errors are in (units; first 25):")
    for fl, k in sorted(by_file.items(), key=lambda kv: -kv[1])[:25]:
        print(f"  {k:5}  {fl}")
    print("\nmissing names (units they stop; first 40):")
    for n, k in sorted(by_name.items(), key=lambda kv: -kv[1])[:40]:
        print(f"  {k:5}  {n}")
    print("\nerror classes (units; first 30):")
    for c, k in sorted(by_class.items(), key=lambda kv: -kv[1])[:30]:
        print(f"  {k:5}  {c[:150]}")
    if by_header:
        print("\nmissing headers:")
        for h, k in sorted(by_header.items(), key=lambda kv: -kv[1])[:20]:
            print(f"  {k:5}  {h}")
    summary = out / "summary.txt"
    with open(summary, "w", encoding="utf-8") as f:
        f.write(f"{t.name} {a.dialect}: {n_ok}/{len(rows)} units compile, {lp64_only} more fail only on the "
                f"record-size asserts\n")
        for m, (ok, bad) in sorted(mod_counts.items()):
            f.write(f"{m}\t{ok}\t{bad}\n")
        f.write("\n# files the errors are in\n")
        for fl, k in sorted(by_file.items(), key=lambda kv: -kv[1]):
            f.write(f"{k}\t{fl}\n")
        f.write("\n# missing names\n")
        for n, k in sorted(by_name.items(), key=lambda kv: -kv[1]):
            f.write(f"{k}\t{n}\n")
        f.write("\n# error classes\n")
        for c, k in sorted(by_class.items(), key=lambda kv: -kv[1]):
            f.write(f"{k}\t{c}\n")
    return 0


def cmd_targets(a):
    host = host_target_name()
    for name in TARGETS:
        t = Target(name)
        why = t.can_build()
        print(f"{name:16} {'host' if name == host else '    '}  {'builds here' if not why else why}"
              f"{'; runs here' if not why and t.native() else ''}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    for name in ("test", "fonttest", "sdl3", "sdltest", "census", "targets"):
        p = sub.add_parser(name)
        p.add_argument("--target", default=host_target_name(), choices=TARGETS)
        p.add_argument("-j", type=int, default=3)
        if name in ("test", "fonttest", "sdltest"):
            p.add_argument("--no-run", action="store_true")
        if name == "test":
            p.add_argument("--no-network", action="store_true")
        if name == "sdltest":
            p.add_argument("--frames", type=int, default=60)
        if name == "sdl3":
            p.add_argument("--fresh", action="store_true", help="configure from scratch")
        if name == "census":
            p.add_argument("--dialect", choices=["ms", "gnu"], default="ms")
            p.add_argument("--only")
            p.add_argument("modules", nargs="*")
    a = ap.parse_args()
    sys.exit({"test": cmd_test, "fonttest": cmd_fonttest, "sdl3": cmd_sdl3, "sdltest": cmd_sdltest,
              "census": cmd_census, "targets": cmd_targets}[a.cmd](a))


if __name__ == "__main__":
    main()
