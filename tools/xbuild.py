"""Cross-platform builds: the platform layer's tests, the portable test programs and the Linux/macOS compile census.

  python -I tools/xbuild.py test     [--target T] [--no-run] [--no-network]     platform layer tests (tests/platform)
  python -I tools/xbuild.py fonttest [--target T] [--no-run]                    text rasteriser test (PlatFont)
  python -I tools/xbuild.py luatest  [--target T] [--no-run]                    the computer player's Lua runtime (EECPLua)
  python -I tools/xbuild.py tcptest  [--target T] [--no-run]                    TCP/IP plug-in loopback (tools/tcp_test)
  python -I tools/xbuild.py wontest  [--target T] [--no-run] [PROGRAM ...]      WON wire format, crypto, sockets, ops (tests/won)
  python -I tools/xbuild.py sdl3     [--target T]                               SDL3 as a static library (CMake)
  python -I tools/xbuild.py sdltest  [--target T] [--no-run] [--frames N]       SDL3 window + GX presentation test
  python -I tools/xbuild.py census   [--target T] [--dialect ms|gnu] [-j N] [--only TEXT] [module ...]
  python -I tools/xbuild.py game     [--target T] [-j N] [--debug] [--incremental] [--smoke] [--sanitize address|undefined]
                                                                                the game (POSIX targets): ee-aoc
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
import rust_build

ROOT = ee.ROOT
TC = ee.TOOLCHAIN
OUT = pathlib.Path(os.environ.get("EE_XBUILD_OUT", ROOT / "build" / "xbuild"))
IS_WINDOWS = sys.platform == "win32"
IS_LINUX = sys.platform.startswith("linux")
IS_MACOS = sys.platform == "darwin"

PLATFORM_HEADERS = ROOT / "include"
# the platform layer that is still C++; the rest is Rust (rust/crates/ee_plat_core and ee_plat_sys, in ee_rust:
# rust_libs below)
POSIX_IMPL = []
WIN32_IMPL = []
COMMON_IMPL = []                  # src/Platform/Common: the same code on every system
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


# -ffp-contract=off everywhere (RUST_PLAN.md 2.1): no fused multiply-add, so a float expression rounds the same on
# x86_64 and arm64 (the Lua VM and the game's simulation; tools/ee.py MODULE_FLAGS for the Windows build)
WIN_CXX = ["--target=x86_64-pc-windows-msvc", "/nologo", "/std:c++17", "/EHsc", "/MT", "/O1", "/GS-", "/W3",
           "-Wno-deprecated-declarations", "/clang:-ffp-contract=off"]
POSIX_CXX = ["-std=c++17", "-O1", "-g", "-Wall", "-Wno-unused-function", "-ffp-contract=off"]


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
        flags = ["-std=c11", "-O1", "-g", "-ffp-contract=off"] if is_c else POSIX_CXX
        run([target.clang(cxx=not is_c), *base, *flags, "-c", *[f"-I{i}" for i in includes],
             *[f"-D{d}" for d in defines], "-o", obj, src])
        objs.append(obj)
    run([target.clang(), *target.link_flags(), "-o", exe, *objs, *libs, *extra_link])
    return exe


def rust_libs(target):
    """The Rust library (the platform layer lives in it) and what its objects need from the system."""
    lib = str(rust_build.ensure(target.name))
    if target.os == "windows":
        return [lib]
    if target.os == "linux":
        return [lib, "-lpthread", "-ldl", "-lm"]
    return [lib, "-framework", "Foundation", "-framework", "Security", "-framework", "CoreFoundation", "-liconv"]


def platform_sources(target):
    impl = ROOT / "src" / "Platform" / ("Win32" if target.os == "windows" else "Posix")
    names = WIN32_IMPL if target.os == "windows" else POSIX_IMPL
    return [impl / n for n in names] + [ROOT / "src" / "Platform" / "Common" / n for n in COMMON_IMPL]


def run_program(target, exe, args=(), env=None, cwd=None):
    if not target.native():
        print(f"{exe.name}: built for {target.name}; this machine cannot run it (not run)")
        return 0
    print(f"running {exe.name} {' '.join(args)}", flush=True)
    r = subprocess.run([str(exe), *args], env=env, cwd=cwd)
    print(f"{exe.name}: exit {r.returncode}")
    return r.returncode


# ---- test / fonttest


def cmd_test(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    srcs = [*platform_sources(t), ROOT / "tests" / "platform" / "plat_test.cpp"]
    libs = rust_libs(t)
    exe = build_program(t, "plat_test", srcs, includes=[PLATFORM_HEADERS], libs=libs)
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe, ["--no-network"] if a.no_network else [])


# the computer player's Lua runtime (ee/EECPLua.h) with the vendored Lua and the platform layer (PlatFOpen); the
# test scripts are tests/platform/lua/ai/*.lua, the folder passed in plays the part of Data
def cmd_luatest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    srcs = [*platform_sources(t), ROOT / "src" / "Empire Earth" / "AI Thread" / "EECPLua.cpp",
            ROOT / "tests" / "platform" / "lua_test.cpp"]
    libs = rust_libs(t)
    exe = build_program(t, "lua_test", srcs, includes=[PLATFORM_HEADERS],
                        libs=libs, c_sources=[ROOT / "src" / "ThirdParty" / "lua" / "ee_lua.c"])
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe, [str(ROOT / "tests" / "platform" / "lua") + os.sep])


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
    # PlatFont is Rust (rust/crates/ee_plat_ui, in ee_rust); nothing else of the library pulls SDL in
    srcs = [ROOT / "tests" / "platform" / "font_test.cpp"]
    exe = build_program(t, "font_test", srcs, includes=[PLATFORM_HEADERS], libs=rust_libs(t))
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe, [str(font_dir())])


# ---- tcptest: the TCP/IP network plug-in over 127.0.0.1 (tools/tcp_test/tcp_test.cpp)

TCP_UNIT = ROOT / "src" / "Low-Level Engine" / "Network Engine" / "TCP Protocol" / "TCP Protocol.cpp"
# the engine units the plug-in links with (tcp_test.cpp stands in for the rest)
TCP_ENGINE_UNITS = [ROOT / "src" / "Low-Level Engine" / p for p in (
    "Utilities/UString.cpp", "Utilities/UWideString.cpp", "Network Engine/NEProtocolConfiguration.cpp",
    "Thread System/TSCriticalSection.cpp", "Thread System/TSEvent.cpp")]


def cmd_tcptest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    out = t.out / "tcp_test"
    out.mkdir(parents=True, exist_ok=True)
    exe = out / ("tcp_test" + t.exe_suffix)
    game = [TCP_UNIT, *TCP_ENGINE_UNITS, ROOT / "tools" / "tcp_test" / "tcp_test.cpp"]
    # the game's include path and dialect (tools/clang_build.py; on POSIX the census's, include/compat/ first)
    inc = [ROOT / "include", TCP_UNIT.parent, *[ROOT / "src" / d for d in ee.EEX_INCLUDE_DIRS]]
    defs = ["EE_STATIC", "WIN32", "NDEBUG", "_WINDOWS"]
    # the engine units with the engine's module define (tools/ee.py MODULE_FLAGS "lle")
    unit_defs = lambda src: [*defs, *(["LOWLEVELENGINE_EXPORTS"] if src in TCP_ENGINE_UNITS else [])]
    rust_lib = rust_build.ensure(t.name)   # the plug-in itself (rust/crates/ee_net_tcp)
    objs = []
    if t.os == "windows":
        env = windows_env()
        game_flags = ["-fms-compatibility", "-fms-extensions", "-fms-compatibility-version=19.44", "/Zc:wchar_t",
                      "-fdelayed-template-parsing", "-fno-strict-aliasing", "-fwrapv", "/GR", "-Wno-everything"]
        for src in [*game, *platform_sources(t)]:
            obj = out / (pathlib.Path(src).stem.replace(" ", "_") + ".obj")
            flags = WIN_CXX + (game_flags if src in game else [])
            if src in TCP_ENGINE_UNITS:   # no exported inline members or vtables of the engine classes it does not link
                flags = flags + ["/Zc:dllexportInlines-"]
            run([t.clang(), *flags, "/c", *[f"/I{i}" for i in inc], *[f"/D{d}" for d in unit_defs(src)],
                 f"/Fo{obj}", src], env=env)
            objs.append(obj)
        run([ee.LLD_LINK, "/nologo", f"/OUT:{exe}", "/SUBSYSTEM:CONSOLE", "/DEBUG", *objs, *rust_libs(t), "libcmt.lib",
             "libvcruntime.lib", "libucrt.lib", "libcpmt.lib", "oldnames.lib", *WIN32_SYSLIBS], env=env)
    else:
        base = t.base_flags()
        game_flags = ["-fms-extensions", "-fms-compatibility", "-fdelayed-template-parsing",
                      "-fms-compatibility-version=19.44", "-fno-strict-aliasing", "-fwrapv", "-ffp-contract=off", "-w"]
        for src in [*game, *platform_sources(t)]:
            obj = out / (pathlib.Path(src).stem.replace(" ", "_") + ".o")
            if src in game:
                # -O0: with __OPTIMIZE__ glibc's inline string.h needs the GNU macros -fms-compatibility hides
                flags = ["-std=c++17", "-O0", "-g", *game_flags, f"-I{COMPAT}", *[f"-I{i}" for i in inc],
                         *[f"-D{d}" for d in unit_defs(src)]]
            else:
                flags = [*POSIX_CXX, f"-I{PLATFORM_HEADERS}"]
            run([t.clang(), *base, *flags, "-c", "-o", obj, src])
            objs.append(obj)
        run([t.clang(), *t.link_flags(), "-o", exe, *objs, *rust_libs(t), *(["-ldl"] if t.os == "linux" else [])])
    print(f"built {exe}")
    if a.no_run:
        return 0
    return run_program(t, exe)


# ---- wontest: the WON library's wire format and socket layer (tests/won), built like the game's units

WON_SRC = ROOT / "src" / "Empire Earth" / "WONAPI"
# program -> the WON units it links besides its test source and the platform layer
WON_TESTS = {
    "won_wire_test": [],
    # the message buffers' script (golden: rust/crates/ee_won/src/golden_buffer.txt, recorded over the C++ classes)
    "won_buffer_test": [],
    # the forwarders to ee_won (rust/crates/ee_won)
    "won_crypto_test": [*[WON_SRC / "WONCommon" / f for f in ("CRC16.cpp", "MD5.cpp")],
                        *[WON_SRC / "WONCrypt" / f for f in ("Blowfish.cpp", "ElGamal.cpp", "MD5Digest.cpp")],
                        *[WON_SRC / "WONAuth" / f for f in ("AuthBase.cpp", "AuthPubKeyBlock.cpp")]],
    "won_socket_test": [WON_SRC / "WONSocket" / f for f in ("AsyncSocket.cpp", "AsyncSocketDefaults.cpp",
                                                            "AsyncSocketInternet.cpp", "IPAddr.cpp")],
    # the socket ops, BlockingSocket / QueueSocket, the async op machinery and WONAPICore's threads
    "won_ops_test": [*[WON_SRC / "WONSocket" / f for f in ("AsyncSocket.cpp", "AsyncSocketDefaults.cpp",
                                                         "AsyncSocketInternet.cpp", "IPAddr.cpp",
                                                         "BlockingSocket.cpp", "QueueSocket.cpp", "SocketOps.cpp",
                                                         "SocketOperationDispatch.cpp", "SocketRunHooks.cpp",
                                                         "SocketOpsAccessors.cpp", "RecvCRMsgOp.cpp",
                                                         "SocketThread.cpp")],
                     *[WON_SRC / "WONCommon" / f for f in ("AsyncOp.cpp", "AsyncOpMode.cpp", "AsyncOpTracker.cpp",
                                                          "AsyncOpWithTracker.cpp", "WONDebug.cpp")],
                     WON_SRC / "WONAPI.cpp"],
    # the async machinery's events (golden: rust/crates/ee_won/src/golden_asyncop.txt, recorded over the C++ classes)
    "won_asyncop_test": "won_ops_test",
    # the socket classes' events (golden: rust/crates/ee_won/src/golden_sockobj.txt, recorded over the C++ classes)
    "won_sockobj_test": "won_ops_test",
    # the LAN lobby's messages, the original's WriteBuffer statements against ee_won's lan module
    "won_lan_test": [],
    # the lobby's game room (StagingLogic, LobbyGame's rules and summary), the original's statements against ee_won's
    # staging module
    "won_staging_test": [],
    # the lobby's saved settings (LobbyPersistentData's files and rules), the original's statements against ee_won's
    # persist module
    "won_persist_test": [WON_SRC / "WONCommon" / f for f in ("FileReader.cpp", "FileReaderWideStrings.cpp",
                                                            "FileWriter.cpp", "WONFile.cpp", "CRC16.cpp",
                                                            "StringUtil.cpp")],
    # the lobby's data model (LobbyModel's client, group, game and server lists), the original's statements against
    # ee_won's model module
    "won_model_test": [],
    # LanLogic's flow (what a message, a closed socket, a summary, a join status or a command does), the original's
    # statements against ee_won's lanlogic module
    "won_lanlogic_test": [],
    # StagingLogic's and InitLogic's flows (hosting, joining, leaving, readying, kicks, game actions; the start-up
    # checks and the advert's lines), the original's statements against ee_won's flow module
    "won_flow_test": [],
    # RoutingLogic's decisions (room flags, Quick Play's pick, the friend search, the default server), the original's
    # statements against ee_won's routing module
    "won_routing_test": [],
    # WONServer's exchange (ServerRequestOp's RunHook and CallbackHook over scripted servers, the time limits, the
    # ping requests and replies, MultiPingOp's rules and order, the context's shuffle), the original's statements
    # against ee_won's server module
    "won_server_test": [],
    # WONGUI's layout arithmetic (rectangles, sizes, the child layout instructions and layouts, the LAYOUT rule), the
    # original's statements against ee_won's guilayout module
    "won_guilayout_test": [],
    # WONGUI's scrolling (the scrollbar's thumb and steps, the scroll area's clamp and view, the scrollers'
    # conditions), the original's statements against ee_won's guiscroll module
    "won_guiscroll_test": [],
    # WONGUI's input box text model (the selection, cursor moves, deletes, paste limit, blink, mouse character and
    # scroll), the original's statements against ee_won's guiedit module
    "won_guiedit_test": [],
    # WONGUI's list lines and selection (the line at a y, visibility, stacking, the selectable walk, the keys, the
    # scrolls of inserts, refreshes and deletes), the original's statements against ee_won's guilist module
    "won_guilist_test": [],
    # WONGUI's focus rules, hit-test verdict and buttons' state machine, the original's statements against ee_won's
    # guifocus module
    "won_guifocus_test": [],
    # WONGUI's word wrap (WrappedText's breaks and rewrap rule), the original's loop against ee_won's guiwrap module
    "won_guiwrap_test": [],
    # WONGUI's rich text area (the chat: segments on lines, the wrap, limits, renumbering, the segment and
    # character under a point, the selection, scrolls, the text splits and the lobby's format commands), the
    # original's statements against ee_won's guitext module
    "won_guitext_test": [],
    # WONGUI's compound controls (the combo box's list size and keys, the tab control's bars, scroll, page and
    # arrow keys, the splitter's split, panes and bars), the original's statements against ee_won's guictl module
    "won_guictl_test": [],
    # WONGUI's multi-column lists and tree rows (the column resizes and places, the header's buttons, sorts and
    # resize edges, the tree's +/- box and child places), the original's statements against ee_won's guimulti module
    "won_guimulti_test": [],
    # WONGUI's tree list (the walks over the items, the range, last-child, open, scroll, insert, remove, sort and
    # mouse rules), the original's statements against ee_won's guitree module
    "won_guitree_test": [],
    # WONGUI's menus' keys and choices (a popup menu's keys and submenus, the menu bar's keys, items, menus,
    # presses and events), the original's statements against ee_won's guimenu module
    "won_guimenu_test": [],
    # WONGUI's event dispatch decisions (a window's mouse, keys, double click, resize, hover and cursor; the window
    # manager's timer step, popups, focus and capture; the root's window drag; a container's mouse child, desired
    # size and insert place), the original's statements against ee_won's guievent module
    "won_guievent_test": [],
    # WONGUI's config readers' key tables (the instruction keys, the flag and value names), the original's compares
    # against ee_won's guiconf tables, the names through the real GUIString
    "won_guiconf_test": [WON_SRC / "WONGUI" / f for f in ("GUIStringData.cpp", "GUIStringDataMutations.cpp",
                                                         "GUIStringDataResize.cpp", "GUIStringDataSearch.cpp",
                                                         "GUIStringSingleton.cpp", "GUIStringSliceAdapters.cpp")],
    # WONGUI's config readers' integer expressions and operand names, the original's statements against ee_won's
    # guicfg module (the port's exception-carrying wrapper included)
    "won_guicfg_test": [],
    # WONGUI's popups' places, mouse windows and ending presses, the modality and the popup menus' timing, the
    # original's statements against ee_won's guiwin module
    "won_guiwin_test": [],
    # the lobby dialogs' decisions (the create and OK buttons, the skill, the LAN port, the adapters' order, the
    # status dialog's sizes), the original's statements against ee_won's dialogs module
    "won_dialogs_test": [],
    # StringUtil's case maps and compares (D6: ASCII only), the original's loops against the forwarders
    "won_strcase_test": [WON_SRC / "WONCommon" / "StringUtil.cpp"],
    # StringParser, the forwarders to ee_won's parser (golden: rust/crates/ee_won/src/golden_parse.txt)
    "won_parse_test": [WON_SRC / "WONCommon" / f for f in ("StringParser.cpp", "StringParserCollections.cpp",
                                                          "StringParserLists.cpp", "StringUtil.cpp")],
    # FileReader / FileWriter (golden: rust/crates/ee_won/src/golden_file.txt)
    "won_file_test": [WON_SRC / "WONCommon" / f for f in ("FileReader.cpp", "FileReaderWideStrings.cpp",
                                                         "FileWriter.cpp", "WONFile.cpp")],
    # ConfigParser, the lobby's .cfg reader (golden: rust/crates/ee_won/src/golden_config.txt)
    "won_config_test": [*[WON_SRC / "WONConfig" / f for f in ("ConfigParser.cpp", "ConfigToken.cpp")],
                        *[WON_SRC / "WONCommon" / f for f in ("FileReader.cpp", "FileReaderWideStrings.cpp",
                                                             "StringUtil.cpp")]],
    # AuthContext's secrets, community data and hashes: the original's statements against ee_won's authctx module
    "won_auth_test": [WON_SRC / "WONCrypt" / "MD5Digest.cpp"],
    # the directory server's entity replies (golden: rust/crates/ee_won/src/golden_dir.txt)
    "won_dir_test": [*[WON_SRC / "WONDir" / f for f in ("DirEntityReplyParser.cpp", "DirEntity.cpp")],
                     WON_SRC / "WONSocket" / "IPAddr.cpp"],
    # the directory server's ops (every request's bytes, the status replies, the multi-directory accounting, the
    # find's parser choice, the listing's grouping), the original's statements against ee_won's dirops module
    "won_dirops_test": [],
    # the routing server's protocol (every routing op's request and reply, the notifications, the user status
    # datagram, the group list request, the connection's header, dispatch rule and table), the original's
    # statements against include/ee/wonapi/WONRoutingMsg.h over ee_won's routemsg module
    "won_routemsg_test": [],
    # WONDB (ProfileData's shim over the Rust profile, the DBProxy family's, the contest's and the event report's
    # requests and replies), the original's statements against include/ee/wonapi/WONDBMsg.h over ee_won's db module
    "won_db_test": [WON_SRC / "WONDB" / "ProfileData.cpp"],
    # GetCertOp's requests and replies (the reply's entries read in Rust, the certificate, key block and key checks
    # applied in order), the original's statements against include/ee/wonapi/WONCertMsg.h over ee_won's cert
    # module (golden: rust/crates/ee_won/src/golden_cert.txt)
    "won_cert_test": [],
    # WONMisc's HTTP (HTTPGetOp's dates, lines, request and URL split, URL, HTTPCache's ids and names), the
    # original's statements against include/ee/wonapi/WONHttpMsg.h over ee_won's http module and the URL unit
    # (golden: rust/crates/ee_won/src/golden_http.txt)
    "won_http_test": [WON_SRC / "WONMisc" / "URL.cpp", WON_SRC / "WONCommon" / "WONFile.cpp",
                      WON_SRC / "WONCommon" / "StringUtil.cpp"],
    # WONMisc's FTP and patch code (FTPGetOp's URL, commands and reply steps; the version and patch records; the
    # version filter; the patch ops' requests and replies), the original's statements against
    # include/ee/wonapi/WONFtpMsg.h and WONPatchMsg.h over ee_won's ftp and patch modules
    "won_misc_test": [WON_SRC / "WONCommon" / "StringUtil.cpp"],
    # WONInternal's admin ops (the key ops', account's, version and patch ops' requests and replies), the original's
    # statements against include/ee/wonapi/WONAdminMsg.h over ee_won's admin module
    "won_admin_test": [],
    # WONInternal's TitleConfig and CommunityConfig (the instructions' children and errors, PostParse's rules, the
    # texts), the original's statements against include/ee/wonapi/WONTitleConfigMsg.h over ee_won's titlecfg module
    # (golden: rust/crates/ee_won/src/golden_titlecfg.txt)
    "won_titlecfg_test": [WON_SRC / "WONCommon" / "StringUtil.cpp"],
    # WONAuth's peer authentication (PeerAuthClient's and PeerAuthServer's handshake) and AuthSession, the original's
    # statements against include/ee/wonapi/WONPeerAuthMsg.h over ee_won's peerauth module and its session object
    # (golden: rust/crates/ee_won/src/golden_peerauth.txt)
    "won_peerauth_test": [WON_SRC / "WONCrypt" / "Blowfish.cpp"],
    # FindPatchOp's patch choice, DetectFirewallOp's request, reply and wait, RegAuth's password and port, the
    # original's statements against include/ee/wonapi/WONMiscOpsMsg.h over ee_won's miscops module
    # (golden: rust/crates/ee_won/src/golden_miscops.txt)
    "won_miscops_test": [WON_SRC / "WONCrypt" / "Blowfish.cpp"],
    # WONFile's path logic (golden: rust/crates/ee_won/src/golden_path.txt)
    "won_path_test": [WON_SRC / "WONCommon" / "WONFile.cpp"],
    # RegKey on config.yaml (golden: rust/crates/ee_won/src/golden_regkey.txt)
    "won_regkey_test": [WON_SRC / "WONCommon" / "RegKey.cpp"],
    # the lobby's chat command parser (golden: rust/crates/ee_won/src/golden_chat.txt)
    "won_chat_test": [*[WON_SRC / "WONLobby" / f for f in ("ChatCommandParser.cpp",)],
                      *[WON_SRC / "WONGUI" / f for f in ("GUIStringData.cpp", "GUIStringDataMutations.cpp",
                                                       "GUIStringDataResize.cpp", "GUIStringDataSearch.cpp",
                                                       "GUIStringSingleton.cpp", "GUIStringSliceAdapters.cpp")],
                      *[WON_SRC / "WONCommon" / f for f in ("StringParser.cpp", "StringParserCollections.cpp",
                                                           "StringParserLists.cpp", "StringUtil.cpp")]],
    # WONGUI's GUIStringData (golden: rust/crates/ee_won/src/golden_guistring.txt)
    "won_guistring_test": [WON_SRC / "WONGUI" / f for f in ("GUIStringData.cpp", "GUIStringDataMutations.cpp",
                                                           "GUIStringDataResize.cpp", "GUIStringDataSearch.cpp",
                                                           "GUIStringSingleton.cpp", "GUIStringSliceAdapters.cpp")],
    # ConfigObject / ConfigValue, the .cfg grammar (golden: rust/crates/ee_won/src/golden_cfgobj.txt)
    "won_cfgobj_test": [*[WON_SRC / "WONConfig" / f for f in ("ConfigObject.cpp", "ConfigValue.cpp", "ConfigParser.cpp",
                                                             "ConfigToken.cpp")],
                        *[WON_SRC / "WONCommon" / f for f in ("FileReader.cpp", "FileReaderWideStrings.cpp",
                                                             "StringParser.cpp", "StringParserCollections.cpp",
                                                             "StringParserLists.cpp", "StringUtil.cpp")]],
}


def build_won_test(t, name, units):
    """One tests/won program: its source and WON units in the game's dialect and include path (module won's defines),
    the platform layer as standard code -> the executable."""
    out = t.out / "won_test" / name
    out.mkdir(parents=True, exist_ok=True)
    exe = out / (name + t.exe_suffix)
    game = [ROOT / "tests" / "won" / f"{name}.cpp", *units]
    inc = [ROOT / "include", *[ROOT / "src" / d for d in ee.EEX_INCLUDE_DIRS]]
    defs = ["EE_STATIC", "WIN32", "NDEBUG", "_WINDOWS"]
    objs = []
    if t.os == "windows":
        env = windows_env()
        game_flags = ["-fms-compatibility", "-fms-extensions", "-fms-compatibility-version=19.44", "/Zc:wchar_t",
                      "-fdelayed-template-parsing", "-fno-strict-aliasing", "-fwrapv", "/GR", "/EHsc",
                      "-Wno-everything"]
        for src in [*game, *platform_sources(t)]:
            obj = out / (pathlib.Path(src).stem.replace(" ", "_") + ".obj")
            flags = WIN_CXX + (game_flags if src in game else [])
            run([t.clang(), *flags, "/c", *[f"/I{i}" for i in inc], *[f"/D{d}" for d in defs], f"/Fo{obj}", src],
                env=env)
            objs.append(obj)
        run([ee.LLD_LINK, "/nologo", f"/OUT:{exe}", "/SUBSYSTEM:CONSOLE", "/DEBUG", *objs, *rust_libs(t), "libcmt.lib",
             "libvcruntime.lib", "libucrt.lib", "libcpmt.lib", "oldnames.lib", *WIN32_SYSLIBS], env=env)
    else:
        base = t.base_flags()
        for src in [*game, *platform_sources(t)]:
            obj = out / (pathlib.Path(src).stem.replace(" ", "_") + ".o")
            if src in game:
                # -O0: with __OPTIMIZE__ glibc's inline string.h needs the GNU macros -fms-compatibility hides
                flags = ["-std=c++17", "-O0", "-g", *game_dialect_flags(t, True), "-fno-strict-aliasing", "-fwrapv",
                         "-w", f"-I{COMPAT}", *[f"-I{i}" for i in inc], *[f"-D{d}" for d in defs]]
            else:
                flags = [*POSIX_CXX, f"-I{PLATFORM_HEADERS}"]
            run([t.clang(), *base, *flags, "-c", "-o", obj, src])
            objs.append(obj)
        run([t.clang(), *t.link_flags(), "-o", exe, *objs, *rust_libs(t),
             *(["-ldl", "-lpthread"] if t.os == "linux" else [])])
    return exe


def cmd_wontest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    result = 0
    for name, units in WON_TESTS.items():
        if a.programs and name not in a.programs:
            continue
        if isinstance(units, str):   # the same units as another program
            units = WON_TESTS[units]
        exe = build_won_test(t, name, units)
        print(f"built {exe}")
        if not a.no_run:
            # each program in its own directory: their scratch files (won_regkey.yaml as $EE_CONFIG, the config,
            # file and persist tests' files) are relative to the current directory, and two runs, or two programs,
            # sharing one would see each other's
            scratch = exe.parent / "run"
            scratch.mkdir(exist_ok=True)
            result |= run_program(t, exe, cwd=scratch)
    return result


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


def rust_lib(t):
    """The Rust static library (GX and wgpu are inside)."""
    return rust_build.ensure(t.name)


def cmd_sdltest(a):
    t = Target(a.target)
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    wlib = rust_lib(t)
    # the window layer (rust/crates/ee_plat_sdl) with the platform layer it uses, all in the Rust library; it comes
    # before SDL on the link line (it references SDL, and a POSIX link scans each archive once)
    srcs = [ROOT / "tools" / "sdl_test" / "sdl_test.cpp",
            ROOT / "src" / "Low-Level Engine" / "Graphics Engine" / "DX7 Display Plug-ins" / "GX" / "GX.cpp"]
    libs = [wlib, *sdl_libs(t)]
    if t.os == "linux":
        libs += ["-lm", "-ldl"]
    exe = build_program(t, "sdl_test", srcs,
                        includes=[sdl_prefix(t) / "include", *([COMPAT] if t.posix else []), PLATFORM_HEADERS],
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


# what clang-cl's Microsoft mode lets through on Windows that a GNU target with the same dialect flags refuses (the
# Windows build compiles them): the address of a temporary (`&GE3DPoint(...)` arguments) and narrowing in braced
# initializers (Windows' long and unsigned long are both 32-bit; LP64's are not, the values are small)
MS_DIALECT_LENIENCE = ["-Wno-address-of-temporary", "-Wno-c++11-narrowing"]

# Linux: -fms-compatibility on a GNU target hides clang's __GCC_ATOMIC_* macros, which libstdc++ reads: atomic_flag
# (reached through <memory>) wants __GCC_ATOMIC_TEST_AND_SET_TRUEVAL and <atomic>'s is_always_lock_free the
# *_LOCK_FREE values (EETestFixture.cpp). These are what clang defines for x86_64 in GNU mode (2: always lock-free).
LINUX_GCC_ATOMIC_DEFINES = ["-D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1"] + \
    [f"-D__GCC_ATOMIC_{k}_LOCK_FREE=2" for k in ("BOOL", "CHAR", "CHAR8_T", "CHAR16_T", "CHAR32_T", "WCHAR_T",
                                                  "SHORT", "INT", "LONG", "LLONG", "POINTER")]


def census_flags(t, dialect):
    f = [*t.base_flags(), "-fsyntax-only", "-ferror-limit=0", "-fno-color-diagnostics", "-w",
         "-fno-strict-aliasing", "-fwrapv", "-DNDEBUG"]
    if dialect == "ms":
        f += ["-fms-extensions", "-fms-compatibility", "-fdelayed-template-parsing",
              "-fms-compatibility-version=19.44", *MS_DIALECT_LENIENCE, *MACOS_GNUC(t)]
        if t.os == "linux":   # as the game build (game_dialect_flags): libstdc++'s atomics need them
            f += LINUX_GCC_ATOMIC_DEFINES
    elif dialect == "msext":   # the extensions without -fms-compatibility (which retypes va_list on Darwin)
        f += ["-fms-extensions", "-fdelayed-template-parsing"]
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
    std = ["-x", "c", "-std=gnu11"] if is_c else ["-x", "c++", "-std=gnu++17" if dialect != "ms" else "-std=c++17"]
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
           *[ROOT / "src" / d for d in ee.EEX_INCLUDE_DIRS]]
    if (TC / "src" / f"SDL3-{SDL_VERSION}" / "include").exists():   # the window layer (src/Platform/SDL)
        inc.append(TC / "src" / f"SDL3-{SDL_VERSION}" / "include")
    if (TC / "fonts").exists():   # the built-in fonts for compilers without #embed
        inc.append(font_arrays(t))
    have = {pathlib.Path(j[1]).resolve() for j in jobs}
    for s in platform_sources(t):
        if pathlib.Path(s).resolve() not in have:
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


# ---- the game


GAME_NAME = "ee-aoc"


FONT_ARRAYS = [("kSansRegular", "LiberationSans-Regular.ttf"), ("kSansBold", "LiberationSans-Bold.ttf"),
               ("kSansItalic", "LiberationSans-Italic.ttf"), ("kSansBoldItalic", "LiberationSans-BoldItalic.ttf"),
               ("kSerifRegular", "LiberationSerif-Regular.ttf"), ("kSerifBold", "LiberationSerif-Bold.ttf"),
               ("kSerifItalic", "LiberationSerif-Italic.ttf"), ("kSerifBoldItalic", "LiberationSerif-BoldItalic.ttf"),
               ("kMonoRegular", "LiberationMono-Regular.ttf"), ("kMonoBold", "LiberationMono-Bold.ttf"),
               ("kMonoItalic", "LiberationMono-Italic.ttf"), ("kMonoBoldItalic", "LiberationMono-BoldItalic.ttf")]


def font_arrays(t):
    """<out>/gen/PlatFontDataArrays.h: the built-in fonts as C arrays, for compilers without #embed (clang < 19,
    Apple clang) -> the directory"""
    gen = t.out / "gen"
    header = gen / "PlatFontDataArrays.h"
    sources = [TC / "fonts" / f for _, f in FONT_ARRAYS]
    for s in sources:
        if not s.exists():
            sys.exit(f"{s} is missing: python -I tools/fetch_posix.py fonts")
    if header.exists() and all(header.stat().st_mtime > s.stat().st_mtime for s in sources):
        return gen
    gen.mkdir(parents=True, exist_ok=True)
    lines = ["/* GENERATED by tools/xbuild.py from toolchain/fonts (src/Platform/Common/PlatFontData.c) */"]
    for name, f in FONT_ARRAYS:
        data = (TC / "fonts" / f).read_bytes()
        lines.append(f"static const unsigned char {name}[] = {{")
        for k in range(0, len(data), 32):
            lines.append(",".join(str(b) for b in data[k:k + 32]) + ",")
        lines.append("};")
    header.write_text("\n".join(lines) + "\n")
    return gen


GAME_ICON = ROOT / "src" / "Empire Earth" / "res" / "icon102.ico"   # the exe's icon (EE-AOC.rc ICON 102)


def icon_bgra():
    """The game icon's largest 32-bit image -> (size, BGRA rows top-down)"""
    import struct
    data = GAME_ICON.read_bytes()
    best = None
    for i in range(struct.unpack_from("<H", data, 4)[0]):
        w, h, _, _, _, bpp, size, off = struct.unpack_from("<BBBBHHII", data, 6 + 16 * i)
        w = w or 256
        if bpp == 32 and data[off:off + 4] != b"\x89PNG" and (best is None or w > best[0]):
            best = (w, off)
    if best is None:
        sys.exit(f"{GAME_ICON}: no 32-bit image")
    w, off = best
    header = struct.unpack_from("<I", data, off)[0]   # BITMAPINFOHEADER; rows bottom-up, BGRA
    pixels = data[off + header: off + header + w * w * 4]
    rows = [pixels[(w - 1 - y) * w * 4:(w - y) * w * 4] for y in range(w)]
    return w, b"".join(rows)


def icon_header(t):
    """<out>/gen/PlatIconData.h: the game icon as BGRA (the platform layer's window icon off Windows)"""
    gen = t.out / "gen"
    header = gen / "PlatIconData.h"
    if header.exists() and header.stat().st_mtime > GAME_ICON.stat().st_mtime:
        return
    gen.mkdir(parents=True, exist_ok=True)
    size, bgra = icon_bgra()
    lines = [f"/* GENERATED by tools/xbuild.py from {GAME_ICON.relative_to(ROOT).as_posix()} (src/Platform/SDL/PlatWindowIcon.c) */",
             f"static const int kPlatIconSize = {size};", "static const unsigned char kPlatIconBGRA[] = {"]
    for k in range(0, len(bgra), 32):
        lines.append(",".join(str(b) for b in bgra[k:k + 32]) + ",")
    lines.append("};")
    header.write_text("\n".join(lines) + "\n")


def macos_icns(out_file):
    """AppIcon.icns for the app bundle: the 32x32 icon enlarged pixel for pixel (nearest neighbour) to every size an
    iconset has; iconutil makes the .icns"""
    import tempfile
    import zlib
    import struct
    size, bgra = icon_bgra()

    def png(n):
        raw = bytearray()
        for y in range(n):
            raw.append(0)
            sy = y * size // n
            for x in range(n):
                sx = x * size // n
                b, g, r, a = bgra[(sy * size + sx) * 4:(sy * size + sx) * 4 + 4]
                raw += bytes((r, g, b, a))

        def chunk(kind, body):
            return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xffffffff)
        return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    with tempfile.TemporaryDirectory() as temp:
        iconset = pathlib.Path(temp) / "AppIcon.iconset"
        iconset.mkdir()
        for n in (16, 32, 128, 256, 512):
            (iconset / f"icon_{n}x{n}.png").write_bytes(png(n))
            (iconset / f"icon_{n}x{n}@2x.png").write_bytes(png(n * 2))
        subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out_file)], check=True)


def game_dialect_flags(t, cxx_game):
    """The game's C++ units keep the Microsoft dialect of the Windows build (with the lenience clang-cl's Microsoft
    mode has, MS_DIALECT_LENIENCE); C, the vendored libraries and the platform layer are standard code. Optimised
    builds undefine __OPTIMIZE__ for the headers only: glibc's optimised inline string functions need GNU mode
    (__asm__ labels, __extern_always_inline), which -fms-compatibility switches off; the code is optimised all the
    same."""
    if not cxx_game:
        return []
    # libstdc++ compares type_info inline only when the compiler says it supports weak symbols (__GXX_WEAK__, which
    # -fms-compatibility does not define); otherwise it calls an out-of-line operator== the library does not export
    f = ["-fms-extensions", "-fdelayed-template-parsing", *MS_DIALECT_LENIENCE, "-D__GXX_TYPEINFO_EQUALITY_INLINE=1",
         "-fms-compatibility", "-fms-compatibility-version=19.44", *MACOS_GNUC(t)]
    # Linux: -fms-compatibility also hides the __GCC_ATOMIC_* macros libstdc++ reads (LINUX_GCC_ATOMIC_DEFINES)
    if t.os == "linux":
        f += LINUX_GCC_ATOMIC_DEFINES
        # Linux: functions returning std::string mangle with libstdc++'s [abi:cxx11] tag or without it depending on
        # whether glibc's <string.h> came before <string> in the unit (in this dialect); units that include the
        # compat <windows.h> first get it first, units that start with <string> did not, and the two halves of a call
        # did not link (GUIStringData::GetString). Every game unit sees <string.h> first.
        f += ["-include", "string.h"]
    return f


def MACOS_GNUC(t):
    """macOS: -fms-compatibility stops clang defining __GNUC__, and the SDK then types __darwin_va_list as void*
    against clang's __builtin_va_list (every unit failed on it): claim GCC 4.2.1 as clang normally does"""
    return ["-fgnuc-version=4.2.1"] if t.os == "macos" else []


def game_job_command(t, job, out, inc, opt):
    mod, src, flags = job
    import verify
    src = pathlib.Path(src).resolve()
    rel = src.relative_to(ROOT / "src").as_posix()
    is_c = src.suffix.lower() == ".c"
    plain = is_c or rel.startswith(("ThirdParty/", "Platform/"))
    defs = ["-D" + d[2:] for d in verify.unit_flags(src, flags).split() if d.startswith(("-D", "/D"))]
    own = []
    third = ROOT / "src" / "ThirdParty"
    if src.is_relative_to(third):
        lib = third / src.relative_to(third).parts[0]
        if (lib / "include_dirs.txt").exists():
            own = [str(lib / d.strip()) for d in (lib / "include_dirs.txt").read_text().splitlines() if d.strip()]
    std = ["-x", "c", "-std=gnu11"] if is_c else ["-x", "c++", "-std=gnu++17" if plain else "-std=c++17"]
    obj = out / "obj" / (rel.replace("/", "__").replace(" ", "_") + ".o")
    cmd = [t.clang(cxx=not is_c), *t.base_flags(), "-c", *std, *opt, "-g", "-w", "-fno-strict-aliasing", "-fwrapv",
           "-ffp-contract=off",
           "-U__OPTIMIZE__", "-DNDEBUG", *game_dialect_flags(t, not plain), *defs,
           f"-I{src.parent}", *[f"-I{i}" for i in own], *[f"-I{i}" for i in inc], "-o", str(obj), str(src)]
    return obj, cmd


def cmd_game(a):
    """Builds the game for a POSIX target: every unit of the Windows exe (tools/units.py) with the POSIX platform
    layer, SDL3 and the Rust library, one link -> <out>/game/ee-aoc (macOS: also <out>/game/Empire Earth AoC.app)."""
    t = Target(a.target)
    if not t.posix:
        sys.exit("the game for Windows is tools/build.py")
    why = t.can_build()
    if why:
        sys.exit(f"{t.name}: {why}")
    import units
    if not (sdl_prefix(t) / "include" / "SDL3" / "SDL.h").exists():
        cmd_sdl3(argparse.Namespace(target=t.name, j=a.j, fresh=False))
    wlib = rust_lib(t)
    out = t.out / ("game" if not a.sanitize else f"game-{a.sanitize}")
    (out / "obj").mkdir(parents=True, exist_ok=True)
    mods = units.TARGET_MODULES["exe"]
    jobs = [j for m in mods for j in units.jobs_for(m)] + units.lib_jobs(mods)
    jobs = [j for j in jobs if "Platform/Win32" not in units.rel_src(j[1]).as_posix()]
    have = {pathlib.Path(j[1]).resolve() for j in jobs}
    for s in platform_sources(t):
        if pathlib.Path(s).resolve() not in have:
            jobs.append(("platform", s, ""))
    # the image libraries' libjpeg is the engine's (as build.py links it); the image units go into an archive
    jobs = [j for j in jobs if not (j[0] == "exeimg" and units.rel_src(j[1]).parts[1] == "libjpeg")]
    # the window icon's pixels (the generated gen/PlatIconData.h) handed to the Rust window layer at start-up
    jobs.append(("platform", ROOT / "src" / "Platform" / "SDL" / "PlatWindowIcon.c", ""))
    inc = [COMPAT, ROOT / "include", ROOT / "src" / "ThirdParty" / "dmusic" / "include",
           *[ROOT / "src" / d for d in ee.EEX_INCLUDE_DIRS],
           sdl_prefix(t) / "include", font_arrays(t)]
    icon_header(t)   # gen/PlatIconData.h, beside the font arrays
    opt = ["-O1"] if not a.debug else ["-O0"]
    san = [f"-fsanitize={a.sanitize}", "-fno-omit-frame-pointer"] if a.sanitize else []
    opt += san
    t0 = time.time()
    commands = [(j,) + game_job_command(t, j, out, inc, opt) for j in jobs]

    def compile_one(item):
        job, obj, cmd = item
        if a.incremental and obj.exists() and obj.stat().st_mtime > pathlib.Path(job[1]).stat().st_mtime:
            return job, obj, True, ""
        r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return job, obj, r.returncode == 0, r.stdout + r.stderr
    failed, objs = [], []
    with concurrent.futures.ThreadPoolExecutor(max(1, a.j)) as ex:
        for job, obj, ok, log in ex.map(compile_one, commands):
            if ok:
                objs.append((job, obj))
            else:
                failed.append(job)
                print(f"FAILED {units.rel_src(job[1])}\n{log[-1500:]}", flush=True)
    print(f"{len(commands) - len(failed)}/{len(commands)} units compiled in {time.time() - t0:.0f} s -> {out / 'obj'}")
    if failed:
        return 1
    # link order as build.py's: the engine and the plug-ins (and their libraries) first, then the game, then the
    # image libraries as an archive
    first = [o for j, o in objs if j[0] not in ("exe", "exeimg")]
    game = [o for j, o in objs if j[0] == "exe"]
    img = [o for j, o in objs if j[0] == "exeimg"]
    archive = out / "libexeimg.a"
    archive.unlink(missing_ok=True)
    ar = str(ee.LLVM / "llvm-ar.exe") if IS_WINDOWS else (shutil.which("llvm-ar") or "ar")
    run([ar, "rcs", archive, *img])
    exe = out / GAME_NAME
    rsp = out / "link.rsp"
    rsp.write_text("".join(f'"{pathlib.Path(o).as_posix()}"\n' for o in [*first, *game]))
    # the Rust library references SDL (the window layer), so it comes before SDL: a POSIX link scans each archive once
    libs = [archive, wlib, *sdl_libs(t)]
    if t.os == "linux":
        libs += ["-ldl", "-lm", "-lpthread"]
    else:
        libs += ["-framework", "Metal", "-framework", "QuartzCore", "-framework", "Foundation"]
    run([t.clang(), *t.link_flags(), *san, "-o", exe, f"@{rsp}", *libs])
    print(f"built {exe} ({exe.stat().st_size // 1024} KB) in {time.time() - t0:.0f} s")
    if t.os == "macos":
        app = out / "Empire Earth AoC.app" / "Contents"
        if app.parent.exists():
            shutil.rmtree(app.parent)
        (app / "MacOS").mkdir(parents=True)
        (app / "Resources").mkdir()
        shutil.copy2(exe, app / "MacOS" / GAME_NAME)
        (app / "Info.plist").write_text(MACOS_INFO_PLIST)
        macos_icns(app / "Resources" / "AppIcon.icns")   # the original icon (EE-AOC.rc ICON 102)
        print(f"app bundle {app.parent} (put the game's data in Contents/Resources, or set EE_DATA_ROOT)")
    if a.smoke and t.native():
        return run_program(t, exe, ["--smoke-test"])
    return 0


MACOS_INFO_PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>ee-aoc</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundleIdentifier</key><string>net.empire-earth-next.aoc</string>
    <key>CFBundleName</key><string>Empire Earth AoC</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>LSMinimumSystemVersion</key><string>11.0</string>
    <key>NSHighResolutionCapable</key><false/>
</dict>
</plist>
"""


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
    for name in ("test", "fonttest", "luatest", "tcptest", "wontest", "sdl3", "sdltest", "census", "game", "targets"):
        p = sub.add_parser(name)
        p.add_argument("--target", default=host_target_name(), choices=TARGETS)
        p.add_argument("-j", type=int, default=3)
        if name in ("test", "fonttest", "luatest", "tcptest", "wontest", "sdltest"):
            p.add_argument("--no-run", action="store_true")
        if name == "test":
            p.add_argument("--no-network", action="store_true")
        if name == "wontest":
            p.add_argument("programs", nargs="*", help="only these tests/won programs (default: all)")
        if name == "sdltest":
            p.add_argument("--frames", type=int, default=60)
        if name == "sdl3":
            p.add_argument("--fresh", action="store_true", help="configure from scratch")
        if name == "game":
            p.add_argument("--debug", action="store_true", help="-O0")
            p.add_argument("--incremental", action="store_true", help="keep objects newer than their unit")
            p.add_argument("--smoke", action="store_true", help="run the built game with --smoke-test (native only)")
            p.add_argument("--sanitize", choices=["address", "undefined"],
                           help="clang sanitizer build -> <out>/game-<sanitizer>/ (POSIX debugging)")
        if name == "census":
            p.add_argument("--dialect", choices=["ms", "msext", "gnu"], default="ms")
            p.add_argument("--only")
            p.add_argument("modules", nargs="*")
    a = ap.parse_args()
    sys.exit({"test": cmd_test, "fonttest": cmd_fonttest, "luatest": cmd_luatest, "tcptest": cmd_tcptest, "wontest": cmd_wontest, "sdl3": cmd_sdl3, "sdltest": cmd_sdltest,
              "census": cmd_census, "game": cmd_game, "targets": cmd_targets}[a.cmd](a))


if __name__ == "__main__":
    main()
