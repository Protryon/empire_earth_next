"""Link one DLL with lld-link: its compiled units in link order, library code built from source, import libraries
(tools/implib.py: the Windows SDK's for system DLLs, by name from the original import table for the rest), the
export .def with the original names and ordinals, and the DLL runtime (MSVC CRT, vcruntime, UCRT, MSVC STL).

relink(module, out, objs, extra_libs) -> 0 on success. objs(source, flags) -> object path (tools/build.py compiles
everything first).
"""
import collections
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import implib
import units
import verify
from ee import LLD_LINK, LLVM_LIB, LLVM_NM, MODULE_FLAGS, RUNTIME_LIBS, lib_paths, module_filename, module_src, \
    module_units

LINK_FLAGS = ["/DLL", "/DEBUG", "/INCREMENTAL:NO", "/OPT:REF", "/OPT:ICF", "/MACHINE:X86", "/SUBSYSTEM:WINDOWS",
              "/NODEFAULTLIB", "/BASE:0x10000000", "/SAFESEH:NO", "/ERRORLIMIT:0", "/DEMANGLE:NO"]
# static libraries: the engine's libjpeg (built from source as module jpeg) and the SDK's dxguid
STATIC_LIBS = {"lle": [("libjpeg", "jpeg"), ("dxguid.lib", None), ("dinput8.lib", None)],   # dinput8: c_dfDI* data "dx7hr": [("dxguid.lib", None)],
               "dx7hrtnl": [("dxguid.lib", None)]}


def run(cmd, cwd):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.returncode, (r.stdout + r.stderr).strip()


def defined_symbols(inputs):
    out = subprocess.run([str(LLVM_NM), "--defined-only", "--extern-only", "--format=just-symbols", *inputs],
                         capture_output=True, text=True)
    syms = {l.strip() for l in out.stdout.splitlines() if l.strip() and not l.endswith(":")}
    # a .def names C functions undecorated: _name (cdecl), _name@n (stdcall)
    return syms | {re.sub(r"@\d+$", "", s[1:]) for s in syms if s.startswith("_")}


def write_def(module, path, inputs):
    """The export .def from the original table (link/<module>/exports.txt), with the original ordinals. Exports no
    object defines are dropped and listed in exports_dropped.txt: the VC6 images also exported things a clang build
    never emits as such (pooled string literals ??_C@, default-constructor closures ??_F; nothing imports them). A
    member-template instantiation keeps its VC6 export name as an alias: VC6 mangled it without the template
    arguments (?Name@scope...), clang with them (??$Name@<args>@scope...): `vc6name=clangname`."""
    exp = [ln.rsplit(" @", 1) for ln in (verify.LINK_DIR / module / "exports.txt").read_text(encoding="utf-8")
           .splitlines() if ln.strip() and not ln.startswith("#")]
    defined = defined_symbols(inputs)
    templ = collections.defaultdict(list)
    for s in defined:
        m = re.match(r"\?\?\$([^@?]+)@", s)
        if m:
            templ[m.group(1)].append(s)
    long_names = {n for n, _ in exp if len(n) == 512}   # names the export table truncated at 512 characters
    full = {n[:512]: n for n in defined if len(n) > 512 and n[:512] in long_names}
    lines, dropped, aliased = [], [], 0
    for name, ordinal in exp:
        name = full.get(name, name)
        if name in defined:
            lines.append(f"    {name} @{ordinal}")
            continue
        m = re.match(r"\?([^@?]+)@(.*)$", name)
        hits = [s for s in templ.get(m.group(1), []) if s.endswith("@" + m.group(2))] if m else []
        if len(hits) == 1:
            lines.append(f"    {name}={hits[0]} @{ordinal}")
            aliased += 1
        else:
            dropped.append(name)
    path.write_text(f'LIBRARY "{pathlib.Path(module_filename(module)).stem}"\nEXPORTS\n' + "\n".join(lines) + "\n")
    drop_file = path.parent / "exports_dropped.txt"
    drop_file.unlink(missing_ok=True)
    if dropped:
        drop_file.write_text("".join(f"{n}\n" for n in dropped))
    if aliased or dropped:
        print(f"{path.name}: {aliased} VC6 member-template export name(s) aliased, {len(dropped)} export(s) not "
              f"defined by any object dropped (exports_dropped.txt)")


def link6_map(path):
    """lld's MSVC-style map with LINK 6's 8-digit Rva+Base column (every image is 32-bit), the form the map readers
    (exe_fulllink, tools/harness) parse."""
    if path.exists():
        text = path.read_text(encoding="latin-1")
        path.write_text(re.sub(r"(?m)^( [0-9a-f]{4}:[0-9a-f]{8}\s+\S+\s+)0{8}([0-9a-f]{8})", r"\1\2", text),
                        encoding="latin-1")


def relink(module, out, objs, extra_libs=()):
    out = pathlib.Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    dll = module_filename(module)
    paths = [p for _, _, p in module_units(module) if p.exists()]
    paths += [verify.ROOT / "src" / rel for rel in units.EXTRA_UNITS.get(module, [])]
    inputs = [objs(p, MODULE_FLAGS[module]) for p in paths]
    inputs += [objs(verify.ROOT / "src" / units.LIB_UNITS[n], MODULE_FLAGS[module])
               for n in units.LIB_MEMBERS.get(module, [])]
    libs = implib.make_import_libs(module, out / "lib", inputs)
    for name, mod in STATIC_LIBS.get(module, []):
        if mod is None:
            libs.append(name)
            continue
        us = ([p for _, _, p in module_units(mod)] if (verify.LINK_DIR / mod / "units.txt").exists()
              else sorted(module_src(mod).glob("*.c")))
        lib = out / "lib" / f"{name}.lib"
        code, log = run([str(LLVM_LIB), "/nologo", f"/out:{lib}", *[str(objs(u, MODULE_FLAGS[mod])) for u in us]], out)
        if code:
            sys.exit(log)
        libs.append(str(lib))
    deffile = out / f"{module}.def"
    write_def(module, deffile, [str(p) for p in inputs] + [l for l in libs if os.path.isabs(l)])
    target = out / dll
    cmd = [str(LLD_LINK), "/nologo", *LINK_FLAGS, f"/DEF:{deffile.name}", f"/OUT:{target.name}",
           f"/PDB:{target.stem}.pdb", f"/MAP:{target.stem}.map", *map(str, inputs), *libs, *extra_libs,
           *RUNTIME_LIBS, *lib_paths()]
    (out / "link.cmd").write_text("".join(f'"{a}"\n' for a in cmd[1:]))
    code, log = run(cmd, out)
    log = "\n".join(l for l in log.splitlines() if "ignoring section .debug$S with unrecognized magic" not in l)
    if log:
        print(log)
    link6_map(target.with_suffix(".map"))
    print(f"link exit {code} -> {target}")
    return 1 if code else 0
