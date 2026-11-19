"""Writes tests/won/won_chatparser_orig.h: ChatCommandParser as it was before R6 WONLobby round 17 (the class from
WONLobbyParser.h and the statements of ChatCommandParser.cpp at a given commit), in namespace Orig, for
won_chatparser_test (the original statements against the game's forwards over chatobj.rs).

    python -I tools/won_chatparser_orig.py [COMMIT]   # default 030204242, the round's start
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
COMMIT = sys.argv[1] if len(sys.argv) > 1 else "030204242"


def show(path):
    return subprocess.run(["git", "show", f"{COMMIT}:{path}"], cwd=ROOT, check=True, capture_output=True).stdout.decode()


header = show("include/ee/wonapi/WONLobbyParser.h").replace("\r\n", "\n")
start = header.index("class ChatCommandParser : public RefCount {")
end = header.index("};\n", start) + 3
klass = header[start:end]

unit = show("src/Empire Earth/WONAPI/WONLobby/ChatCommandParser.cpp").replace("\r\n", "\n")
body = unit[unit.index("namespace {\n"):]
body = body.replace("using namespace WONAPI;\n", "")

out = [
    "// won_chatparser_test's namespace Orig (by tools/won_chatparser_orig.py from " + COMMIT + "): ChatCommandParser's",
    "// class and ChatCommandParser.cpp's statements as they were before R6 WONLobby round 17 moved them to chatobj.rs.",
    "// Do not edit: regenerate.",
    "#ifndef WON_CHATPARSER_ORIG_H",
    "#define WON_CHATPARSER_ORIG_H",
    "extern wchar_t gEmptyWideString[];",
    "namespace Orig {",
    "using namespace WONAPI;",
    klass,
    body,
    "} // namespace Orig",
    "#endif",
    "",
]
dst = ROOT / "tests" / "won" / "won_chatparser_orig.h"
dst.write_bytes("\n".join(out).encode())
print(f"wrote {dst}")
