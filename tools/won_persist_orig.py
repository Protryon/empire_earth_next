"""Writes tests/won/won_persistobj_orig.h: the saved settings' C++ as it was at the round's start (WONLobbyMap.h's
containers, WONLobbyPersistence.h's class and declarations, the seven units' statements), for namespace Orig."""
import pathlib
import subprocess

root = pathlib.Path(r"C:\Users\max\Desktop\empire_earth_next-won4")
start = "5d078efba"


def show(path):
    r = subprocess.run(["git", "show", f"{start}:{path}"], cwd=root, capture_output=True, check=True)
    return r.stdout.decode("utf-8").replace("\r\n", "\n")


def unwrap(s):
    """Drops each `namespace WONAPI {` and its closing brace (by brace depth; comments and literals skipped)."""
    out, depth, ns, i = [], 0, [], 0
    key = "namespace WONAPI {"
    while i < len(s):
        if s.startswith(key, i):
            ns.append(depth)
            depth += 1
            out.append("/* namespace WONAPI { */")
            i += len(key)
            continue
        c = s[i]
        if s.startswith("//", i):
            j = s.find("\n", i)
            j = len(s) if j < 0 else j
            out.append(s[i:j])
            i = j
            continue
        if s.startswith("/*", i):
            j = s.find("*/", i) + 2
            out.append(s[i:j])
            i = j
            continue
        if c in "\"'":
            j = i + 1
            while s[j] != c:
                j += 2 if s[j] == "\\" else 1
            out.append(s[i:j + 1])
            i = j + 1
            continue
        if c == "{":
            depth += 1
        if c == "}":
            depth -= 1
            if ns and ns[-1] == depth:
                ns.pop()
                out.append("/* } */")
                i += 1
                continue
        out.append(c)
        i += 1
    assert depth == 0 and not ns
    return "".join(out)


import re

# the functions the game's header now defines in WONAPI as forwards: a call in the copy (an argument of a WONAPI
# type brings WONAPI's in by argument-dependent lookup) is qualified with Orig::
WB = chr(92) + 'b'
LP = chr(92) + '('
API = sorted(set(re.findall(WB + r'(Lobby[A-Za-z]+)' + LP, show('include/ee/wonapi/WONLobbyPersistence.h'))) - {'LobbyPersistentData'})
NL = chr(10)


def qualify(s):
    def fix(line):
        def rep(m):
            before = line[:m.start()].rstrip()
            if line[:m.start()] == '':
                return m.group(0)  # a definition at column 0
            if before == '' or before[-1] in '=(!,?:&|' or before.endswith('return'):
                return 'Orig::' + m.group(0)
            return m.group(0)
        return re.sub(WB + '(?:' + '|'.join(API) + ')' + LP, rep, line)
    return NL.join(fix(l) for l in s.split(NL))


out = [
    "// The lobby's saved settings' C++ as it was at R6 WONLobby round 15's start (5d078efba), copied for",
    "// won_persistobj_test.cpp's namespace Orig (by tools/won_persist_orig.py): WONLobbyMap.h's containers,",
    "// WONLobbyPersistence.h's class and declarations, and the statements of LobbyPersistentData.cpp,",
    "// LobbyPersistentAccessors.cpp, LobbyPersistentSetters.cpp, LobbyPersistentQueries.cpp, LobbyPersistentFilters.cpp,",
    "// LobbyPersistentLoad.cpp and LobbyRestoreDefaults.cpp, unchanged but for their includes and namespace lines and",
    "// the calls of the Lobby* functions, qualified with Orig:: (the game's forwards are WONAPI's).",
    "",
]
m = show("include/ee/wonapi/WONLobbyMap.h")
out += ["// ---- WONLobbyMap.h", m[m.index("namespace WONAPI {\n") + len("namespace WONAPI {\n"):m.rindex("}\n#endif")]]
h = show("include/ee/wonapi/WONLobbyPersistence.h")
out += ["// ---- WONLobbyPersistence.h (the saved settings' part)", h[h.index("// GUIStringLessNoCase comes"):h.rindex("}\n\n#endif")]]
W = "src/Empire Earth/WONAPI/WONLobby/"
for u in ["LobbyPersistentData", "LobbyPersistentAccessors", "LobbyPersistentSetters", "LobbyPersistentQueries",
          "LobbyPersistentFilters", "LobbyPersistentLoad", "LobbyRestoreDefaults"]:
    s = "\n".join(l for l in show(W + u + ".cpp").split("\n") if not l.startswith("#include"))
    out += [f"// ---- {u}.cpp", qualify(unwrap(s))]
p = root / "tests" / "won" / "won_persistobj_orig.h"
p.write_bytes("\n".join(out).encode("utf-8"))
print(p, len("\n".join(out).split("\n")))
