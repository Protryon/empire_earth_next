"""Checks that every value of the WON lobby's host enums has an answer: a `case` in the C++ host, an arm of the
Rust function that answers it in the game (modelobj.rs's `ctl_op`, ...), or an entry of ALLOWED with its reason.

Round 13's LAN capture comparison found host cases a move had lost (every member of a new event read 0): the
goldens' scripted hosts answer every op themselves, so only this check (and a live game) sees the C++ host's
switch. Run it after any change to a lobby host or to the Rust enums it answers:

    python -I tools/won_host_coverage.py            # prints the values without an answer; exit 1 if any
    python -I tools/won_host_coverage.py -v         # also each enum's counts

A value is answered by a host when the host's files hold `case <anything>::<Value>:` (a `typedef` alias such as
`Op::` is fine); a value the C++ makes (the controls' events) must be named there (`E::<Value>`).
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOBBY = ROOT / "src" / "Empire Earth" / "WONAPI" / "WONLobby"
GUI = ROOT / "src" / "Empire Earth" / "WONAPI" / "WONGUI"
HDR = ROOT / "include" / "rust"
RS = ROOT / "rust" / "crates"

# (the C enum, its generated header, the C++ host files, the Rust answering files and function, values allowed
# without a case and why)
CHECKS = [
    dict(enum="EeWonLobbyCtlOp", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp", LOBBY / "LobbyNetCtrlHost.cpp", LOBBY / "EELobbyGameHost.cpp",
                LOBBY / "EELobbyGlueHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "ctl_op"),
               (RS / "ee_won_lobby" / "src" / "persistobj.rs", "ctl_op"),
               (RS / "ee_won_lobby" / "src" / "rows.rs", "ctl_op"),
               (RS / "ee_won_lobby" / "src" / "lobbyctl.rs", "game_string_op"),
               (RS / "ee_won_lobby" / "src" / "lobbyctl.rs", "game_image_op")],
         allowed={}),
    dict(enum="EeWonLobbyCtlEvField", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp", LOBBY / "LobbyCtrlHost.cpp"], rust=[], allowed={}),
    dict(enum="EeWonLobbyCtlItemField", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp", LOBBY / "LobbyCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "rows.rs", "ctl_op")], allowed={}),
    # the C++ makes these (CtlEventOf maps a ComponentEvent's type to one): each must be made somewhere
    dict(enum="EeWonLobbyCtlEvent", header="won_lobby/lobbyctl.h", mentions=True,
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[], allowed={}),
    dict(enum="EeWonLobbyCtlType", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[], allowed={}),
    # the strings go out as resource rows (R6 WONLobby round 17): each named in its table
    dict(enum="EeWonLobbyCtlString", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "lobbyctl.rs", "string_row", True)],
         allowed={}),
    dict(enum="EeWonLobbyPopupString", header="won_lobby/lobbypopup.h",
         hosts=[LOBBY / "LobbyDialogHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "lobbypopup.rs", "string_row", True)],
         allowed={}),
    # the dialogs' strings: the lobby's as rows, the EE game's two by the host (EeString); Empty is the original's
    # empty default
    dict(enum="EeWonLobbyDlgString", header="won_lobby/lobbydlg.h", mentions=True,
         hosts=[LOBBY / "LobbyDialogHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "lobbydlg.rs", "string_row", True)],
         allowed={"Empty": "the original switch's empty default (string_row None, EeString's empty answer)"}),
    dict(enum="EeWonLobbyCtlImage", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "rows.rs", "icon_res", True)],
         allowed={}),
    dict(enum="EeWonLobbyCtlClientTest", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "rows.rs", "client_is")], allowed={}),
    dict(enum="EeWonLobbyCtlPref", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyCtrlHost.cpp"], rust=[(RS / "ee_won_lobby" / "src" / "persistobj.rs", "pref_flag")],
         allowed={}),
    dict(enum="EeWonLobbyCtlLinkField", header="won_lobby/chatctl.h",
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp"], rust=[], allowed={}),
    dict(enum="EeWonRouteLogicOp", header="won_lobby/routelogic.h",
         hosts=[LOBBY / "RoutingLogic.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "route_op"),
               (RS / "ee_won_lobby" / "src" / "modelobj.rs", "route_record_op_in"),
               (RS / "ee_won_lobby" / "src" / "routelogic.rs", "conn_request"),
               (RS / "ee_won_lobby" / "src" / "persistobj.rs", "route_op")],
         allowed={}),
    dict(enum="EeWonLobbyDlgOp", header="won_lobby/lobbydlg.h",
         hosts=[LOBBY / "LobbyDialogHost.cpp", LOBBY / "EELobbyGameHost.cpp", LOBBY / "EELobbyGlueHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "lobbydlg.rs", "game_op")], allowed={}),
    dict(enum="EeWonLobbyPopupOp", header="won_lobby/lobbypopup.h",
         hosts=[LOBBY / "LobbyDialogHost.cpp", LOBBY / "LobbyCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "popup_op"),
               (RS / "ee_won_lobby" / "src" / "lobbypopup.rs", "game_string_op")], allowed={}),
    dict(enum="EeWonLobbyOp", header="won_lobby/lobbyhost.h",
         hosts=[LOBBY / "LobbyLogicHost.cpp", LOBBY / "LanLogicHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "lobby_op"),
               (RS / "ee_won_lobby" / "src" / "lanown.rs", "op"),
               (RS / "ee_won_lobby" / "src" / "persistobj.rs", "lobby_op")], allowed={}),
    dict(enum="EeWonLogicOp", header="won_lobby/logics.h",
         hosts=[LOBBY / "PingLogic.cpp", LOBBY / "AccountLogic.cpp", LOBBY / "InitLogic.cpp"],
         rust=[], allowed={}),
    dict(enum="EeWonEeGameOp", header="won_lobby/eegame.h",
         hosts=[LOBBY / "EELobbyGameHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "eegame_op")], allowed={}),
    dict(enum="EeWonEeGlueOp", header="won_lobby/eeglue.h",
         hosts=[LOBBY / "EELobbyGlueHost.cpp"], rust=[], allowed={}),
    dict(enum="EeWonDlgLogicOp", header="won_lobby/dlglogic.h",
         hosts=[LOBBY / "DialogLogic.cpp"], rust=[], allowed={}),
]


def enum_values(header, name):
    text = (HDR / header).read_text(encoding="utf-8")
    m = re.search(r"enum class " + name + r"\b[^{]*\{(.*?)\n\};", text, re.S)
    if not m:
        return None
    body = re.sub(r"///[^\n]*", "", m.group(1))
    return [v for v in re.findall(r"^\s*(\w+)\s*(?:=[^,]*)?,", body, re.M)]


def host_cases(paths, mentions=False):
    found = set()
    for p in paths:
        if p.exists():
            text = p.read_text(encoding="utf-8", errors="replace")
            if mentions:
                found.update(re.findall(r"\b\w+::(\w+)\b", text))
            else:
                found.update(re.findall(r"case\s+(?:[\w]+::)*(\w+)\s*:", text))
    return found


def rust_arms(path, fn, mentions=False):
    """The `X::Value` names in the arms of the first `match` in `fn` (up to the function's end)."""
    if not path.exists():
        return set()
    text = path.read_text(encoding="utf-8")
    m = re.search(r"\bfn " + fn + r"\b", text)
    if not m:
        return None
    # the function's body: from its first `{` to the brace that closes it
    i = text.index("{", m.end())
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                break
    body = text[i:j]
    if mentions:  # a table's entries (`S::Value` anywhere in the function)
        return set(re.findall(r"\b\w+::(\w+)\b", body))
    return set(re.findall(r"\b\w+::(\w+)\s*(?:\|[^=]*)?(?:\{[^}]*\})?\s*=>", body)) | \
        set(re.findall(r"\|\s*\w+::(\w+)", body))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-v", "--verbose", action="store_true")
    a = ap.parse_args()
    bad = 0
    for c in CHECKS:
        values = enum_values(c["header"], c["enum"])
        if values is None:
            print(f"{c['enum']}: not in include/rust/{c['header']}")
            bad += 1
            continue
        cases = host_cases(c["hosts"], c.get("mentions", False))
        rust = set()
        for path, fn, *flag in c["rust"]:
            arms = rust_arms(path, fn, bool(flag and flag[0]))
            if arms is None:
                print(f"{c['enum']}: no fn {fn} in {path.relative_to(ROOT)}")
                bad += 1
                continue
            rust |= arms
        missing = [v for v in values if v not in cases and v not in rust and v not in c["allowed"]]
        if a.verbose or missing:
            print(f"{c['enum']}: {len(values)} values, {len(cases & set(values))} host cases, "
                  f"{len(rust & set(values))} answered in Rust, {len(missing)} without an answer")
        for v in missing:
            print(f"  {c['enum']}::{v}")
        bad += len(missing)
    print("won_host_coverage: " + ("PASS" if bad == 0 else f"FAIL ({bad})"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
