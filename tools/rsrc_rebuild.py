"""Compile resource scripts with the Windows SDK's rc.exe and link resource-only DLLs with lld-link."""
import os
import pathlib
import subprocess
import sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from ee import LLD_LINK, modern_dirs


def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.returncode, (r.stdout + r.stderr).strip()


def rc_exe():
    return pathlib.Path(os.environ.get("EE_RC") or modern_dirs()[3] / "x86" / "rc.exe")


def compile_rc(rc, out):
    rc, out = pathlib.Path(rc).resolve(), pathlib.Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    res = out / f"{rc.stem}.res"
    code, log = run([str(rc_exe()), "/nologo", "/fo", str(res), rc.name], rc.parent)
    if code:
        sys.exit(f"rc {rc}: exit {code}\n{log}")
    return res

