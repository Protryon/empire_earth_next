"""The Rust static library (rust/, docs/RUST_PLAN.md section 4): build it for a target, and its gates.

  python -I tools/rust_build.py build [--target T]     ee_rust for a target (default: the host) -> build/rust/<triple>/release
  python -I tools/rust_build.py gate                   cargo test, clippy -D warnings, rustfmt --check, the header check
  python -I tools/rust_build.py header                 regenerate include/rust/ee_rust.h (cbindgen)

Targets are tools/xbuild.py's names. Needs rustup (rust/rust-toolchain.toml pins the version) and libclang
(`bindgen`, for wgpu-native's C API). Outputs stay under this checkout's build/, so worktrees do not share them.
"""
import argparse
import os
import pathlib
import platform
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RUST = ROOT / "rust"
TC = pathlib.Path(os.environ.get("EE_TOOLCHAIN", ROOT / "toolchain"))
# CARGO_TARGET_DIR overrides it, so that the worktrees of one machine can share the third-party crates' build
TARGET_DIR = pathlib.Path(os.environ.get("CARGO_TARGET_DIR", ROOT / "build" / "rust"))
# the directory holding include/webgpu/{webgpu,wgpu}.h, which C++ includes
WGPU_HEADERS = RUST / "vendor" / "wgpu-native"
TRIPLES = {"windows-x86_64": "x86_64-pc-windows-msvc", "linux-x86_64": "x86_64-unknown-linux-gnu",
           "macos-arm64": "aarch64-apple-darwin", "macos-x86_64": "x86_64-apple-darwin"}
SYSROOT = TC / "sysroot" / "debian_bullseye_amd64"


def host_target():
    machine = platform.machine().lower()
    if sys.platform == "win32":
        return "windows-x86_64"
    if sys.platform == "darwin":
        return "macos-arm64" if machine in ("arm64", "aarch64") else "macos-x86_64"
    return "linux-x86_64"


def cargo():
    found = shutil.which("cargo")
    if found:
        return found
    home = pathlib.Path(os.environ.get("CARGO_HOME", pathlib.Path.home() / ".cargo"))
    candidate = home / "bin" / ("cargo.exe" if sys.platform == "win32" else "cargo")
    if candidate.exists():
        return str(candidate)
    sys.exit("cargo not found: install rustup (Windows: winget install Rustlang.Rustup; macOS: brew install rustup)")


def cargo_env(target=None):
    """The environment cargo runs in. Pure, so that tools/rust_build_test can check it."""
    env = dict(os.environ, CARGO_TARGET_DIR=str(TARGET_DIR))
    if target in ("macos-arm64", "macos-x86_64"):   # as the C++ (tools/xbuild.py)
        env.setdefault("MACOSX_DEPLOYMENT_TARGET", "11.0" if target == "macos-arm64" else "10.15")
    if sys.platform == "win32" and "LIBCLANG_PATH" not in env:
        llvm = pathlib.Path(env.get("EE_LLVM", r"C:\Program Files\LLVM"))
        env["LIBCLANG_PATH"] = str(llvm / "bin")
    # cross-compiling to Linux from another system: bindgen parses the C headers against the same sysroot as the C++
    if target == "linux-x86_64" and sys.platform != "linux" and SYSROOT.exists():
        clang = pathlib.Path(env.get("EE_LLVM", r"C:\Program Files\LLVM")) / "bin" / "clang.exe"
        resource = subprocess.run([str(clang), "-print-resource-dir"], capture_output=True, text=True).stdout.strip()
        env["BINDGEN_EXTRA_CLANG_ARGS_x86_64_unknown_linux_gnu"] = (
            f"--sysroot={SYSROOT.as_posix()} -isystem \"{pathlib.Path(resource).as_posix()}/include\"")
    return env


def lib_path(target):
    name = "ee_rust.lib" if target.startswith("windows") else "libee_rust.a"
    return TARGET_DIR / TRIPLES[target] / "release" / name


def run_cargo(args, target=None):
    return subprocess.run([cargo(), *args], cwd=RUST, env=cargo_env(target)).returncode


def ensure(target=None):
    """Builds ee_rust for the target if it is out of date; returns the static library's path."""
    target = target or host_target()
    code = run_cargo(["build", "--release", "-p", "ee_rust", "--target", TRIPLES[target]], target)
    if code:
        sys.exit(f"cargo build for {target} failed ({code})")
    return lib_path(target)


def fmt_packages():
    """rustfmt's -p list: the workspace's own crates, not the vendored one."""
    return [arg for crate in sorted(p.name for p in (RUST / "crates").iterdir() if p.is_dir()) for arg in ("-p", crate)]


def gate():
    steps = [("test", ["test", "--workspace", "--exclude", "wgpu-native"]),
             ("clippy", ["clippy", "--workspace", "--exclude", "wgpu-native", "--no-deps", "--all-targets", "--", "-D", "warnings"]),
             ("rustfmt", ["fmt", "--check", *fmt_packages()]),
             ("header", ["run", "-q", "-p", "xtask", "--", "cbindgen", "--check"])]
    for name, args in steps:
        print(f"== {name}", flush=True)
        code = run_cargo(args)
        if code:
            sys.exit(f"rust gate: {name} failed ({code})")
    print("rust gate: PASS")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("command", choices=["build", "gate", "header"])
    ap.add_argument("--target", choices=sorted(TRIPLES))
    a = ap.parse_args()
    if a.command == "build":
        print(ensure(a.target))
        return 0
    if a.command == "gate":
        return gate()
    return run_cargo(["run", "-q", "-p", "xtask", "--", "cbindgen"])


if __name__ == "__main__":
    sys.exit(main())
