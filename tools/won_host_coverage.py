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
               (RS / "ee_won_lobby" / "src" / "lobbyevt.rs", "ctl_op"),
               (RS / "ee_won_lobby" / "src" / "widgets.rs", "ctl_op"),
               (RS / "ee_won_lobby" / "src" / "lobbyctl.rs", "game_string_op"),
               (RS / "ee_won_lobby" / "src" / "lobbyctl.rs", "game_image_op"),
               # the cases whose statements are Rust (R6 WONLobby round 29)
               (RS / "ee_won_lobby" / "src" / "hostops.rs", "ctl_op"),
               # LobbyNetCtrlHost.cpp's compound cases (R6 WON round 35)
               (RS / "ee_won_lobby" / "src" / "hostops.rs", "split_op"),
               # the texts, the events' members and the text rows' members (R6 WON GUI round 40)
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "ctl_op")],
         # hostops.rs's split_op still composes TextBytes, TextAppend(Bytes) and ParserCommandText from these for
         # won_ctl35's world; in the game lobbytext.rs answers the three whole (R6 WON GUI round 40)
         allowed={v: "the game's compound is lobbytext.rs's (no host statement); won_ctl35's world answers it"
                  for v in ("BytesOfText", "TextTake", "TextStartEmpty", "TextAppendText", "TextAppendRaw",
                            "TextAppendParserPrefix", "TextAppendParserCommand", "TextAppendQuoteOpen",
                            "TextAppendParserTarget", "TextAppendQuoteClose", "TextHeldOut", "TextDrop")}),
    # the generic widget shells' calls out (R6 WONLobby round 20): the C++ host answers what the base classes'
    # Rust objects do not (shell.rs Sh routes the component's, button's and MS button's calls to them in the game)
    dict(enum="EeWonShellHostOp", header="won_gui/shell.h", hosts=[GUI / "GuiHost.cpp"],
         # R6 WON GUI round 37: the references are the game's reference function's (gamewin.rs rf)
         rust=[(RS / "ee_won_gui" / "src" / "shell.rs", "host", True),
               # R6 WON GUI round 38: the message box's window calls on a game window (gamewin.rs)
               (RS / "ee_won_gui" / "src" / "gamewin.rs", "msgbox_op", True)],
         allowed={v: "a base class's Rust object answers it in the game (shell.rs Sh); a driver's host otherwise"
                  for v in ("Comp", "Btn", "MsBtn", "Call")}),
    # the images' calls out (R6 WONLobby round 24: imgobj.rs): every one the images' host answers
    dict(enum="EeWonImageHost", header="won_gui/imgobj.h", hosts=[GUI / "WindowManager.cpp"],
         rust=[(RS / "ee_won_gui" / "src" / "imgobj.rs", "href", True),
               # R6 WON GUI round 39: the Graphics is gfxrun.rs's record, made and run there in the game
               (RS / "ee_won_gui" / "src" / "gfxrun.rs", "image_host_op")], allowed={}),
    # the component configs' calls out (R6 WONLobby round 25: compcfg.rs): every one the configs' host answers
    dict(enum="EeWonCcOp", header="won_gui/compcfg.h", hosts=[ROOT / "include" / "ee" / "wonapi" / "WONCompCfg.inl"], rust=[],
         allowed={}),
    dict(enum="EeWonCcKind", header="won_gui/compcfg.h", hosts=[ROOT / "include" / "ee" / "wonapi" / "WONCompCfg.inl"],
         rust=[], allowed={v: "R6 WON round 34: compcfg.rs's ref_op picks the kind's AddRef / Release op (no C++ case)"
                           for v in ("Config", "Res", "Image", "Font", "Sound", "Component")}),
    # the lobby events' members are read in Rust (R6 WONLobby round 18)
    dict(enum="EeWonLobbyCtlEvField", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp", LOBBY / "LobbyCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "lobbyevt.rs", "ctl_op"),
               # a WONGUI event's chat link: hostops.rs picks it (EvTlaLink, R6 WON round 35)
               (RS / "ee_won_lobby" / "src" / "hostops.rs", "split_op", True)],
         allowed={}),
    dict(enum="EeWonLobbyCtlItemField", header="won_lobby/lobbyctl.h",
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp", LOBBY / "LobbyCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "rows.rs", "ctl_op"),
               # the host's fields, one op each (R6 WON round 35)
               (RS / "ee_won_lobby" / "src" / "hostops.rs", "item_op", True)], allowed={}),
    # the C++ makes these (CtlEventOf maps a ComponentEvent's type to one): each must be made somewhere
    dict(enum="EeWonLobbyCtlEvent", header="won_lobby/lobbyctl.h", mentions=True,
         hosts=[LOBBY / "LobbyCtrlHost.cpp"],
         # the event's type maps to them in Rust (R6 WONLobby round 29: evmap.rs)
         rust=[(RS / "ee_won_lobby" / "src" / "evmap.rs", "ctl_event_of", True)], allowed={}),
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
         hosts=[LOBBY / "LobbyNetCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "hostops.rs", "link_op", True)], allowed={}),
    dict(enum="EeWonRouteLogicOp", header="won_lobby/routelogic.h",
         hosts=[LOBBY / "RoutingLogic.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "route_op"),
               (RS / "ee_won_lobby" / "src" / "modelobj.rs", "route_record_op_in"),
               (RS / "ee_won_lobby" / "src" / "routelogic.rs", "conn_request"),
               (RS / "ee_won_lobby" / "src" / "persistobj.rs", "route_op"),
               (RS / "ee_won_lobby" / "src" / "netops.rs", "route_op"),
               # the cases whose statements are Rust (R6 WON round 32)
               (RS / "ee_won_lobby" / "src" / "netlogicops.rs", "route_op"),
               # the texts' reads and the events' members (R6 WON GUI round 40)
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "route_op")],
         allowed={}),
    dict(enum="EeWonLobbyDlgOp", header="won_lobby/lobbydlg.h",
         hosts=[LOBBY / "LobbyDialogHost.cpp", LOBBY / "EELobbyGameHost.cpp", LOBBY / "EELobbyGlueHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "lobbydlg.rs", "game_op"),
               (RS / "ee_won_lobby" / "src" / "lobbydlg.rs", "comp_op"),
               # the widget ops through the widget registry (R6 WONLobby round 19)
               (RS / "ee_won_lobby" / "src" / "widgets.rs", "dlg_op"),
               # the compound cases composed in Rust over one-statement cases (R6 WON round 34)
               (RS / "ee_won_lobby" / "src" / "dlgsplit.rs", "game_op"),
               # the texts and the events' members (R6 WON GUI round 40)
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "dlg_op")], allowed={}),
    dict(enum="EeWonLobbyPopupOp", header="won_lobby/lobbypopup.h",
         hosts=[LOBBY / "LobbyDialogHost.cpp", LOBBY / "LobbyCtrlHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "popup_op"),
               (RS / "ee_won_lobby" / "src" / "widgets.rs", "popup_op"),
               (RS / "ee_won_lobby" / "src" / "lobbypopup.rs", "game_string_op"),
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "popup_op")], allowed={}),
    dict(enum="EeWonLobbyOp", header="won_lobby/lobbyhost.h",
         hosts=[LOBBY / "LobbyLogicHost.cpp", LOBBY / "LanLogicHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "lobby_op"),
               (RS / "ee_won_lobby" / "src" / "lanown.rs", "op"),
               (RS / "ee_won_lobby" / "src" / "lobbyevt.rs", "lobby_op"),
               (RS / "ee_won_lobby" / "src" / "persistobj.rs", "lobby_op"),
               (RS / "ee_won_lobby" / "src" / "netops.rs", "lobby_op"),
               # the cases whose statements are Rust (R6 WONLobby round 29)
               (RS / "ee_won_lobby" / "src" / "lobbyops.rs", "lobby_op"),
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "lobby_op")], allowed={}),
    dict(enum="EeWonLogicOp", header="won_lobby/logics.h",
         hosts=[LOBBY / "PingLogic.cpp", LOBBY / "AccountLogic.cpp", LOBBY / "InitLogic.cpp"],
         # the cases whose statements are Rust (R6 WON round 32)
         rust=[(RS / "ee_won_lobby" / "src" / "netlogicops.rs", "logic_op"),
               # InitLogic's texts (R6 WON GUI round 40)
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "logic_op")], allowed={}),
    dict(enum="EeWonEeGameOp", header="won_lobby/eegame.h",
         hosts=[LOBBY / "EELobbyGameHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "modelobj.rs", "eegame_op"),
               # the compound cases composed in Rust over one-statement cases (R6 WON round 34)
               (RS / "ee_won_lobby" / "src" / "eesplit.rs", "game_op"),
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "eegame_op")], allowed={}),
    # the saved game transfer's calls out (R6 WONLobby round 28: its network ops netops.rs's in the game)
    dict(enum="EeWonEeXferOp", header="won_lobby/eexfer.h",
         hosts=[LOBBY / "EELobbyGameHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "netops.rs", "xfer_op"),
               (RS / "ee_won_lobby" / "src" / "eesplit.rs", "xfer_op")], allowed={}),
    dict(enum="EeWonEeGlueOp", header="won_lobby/eeglue.h",
         hosts=[LOBBY / "EELobbyGlueHost.cpp"],
         # the multi-statement cases composed in Rust over one-statement cases (R6 WON round 32)
         rust=[(RS / "ee_won_lobby" / "src" / "gluesplit.rs", "game_op")],
         allowed={v: "R6 WON GUI round 40: the game's StringUnits reads the units in place (gluesplit.rs); won_split32's world answers it"
                  for v in ("StringLength", "StringUnitAt")}),
    dict(enum="EeWonDlgLogicOp", header="won_lobby/dlglogic.h",
         hosts=[LOBBY / "DialogLogic.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "logicsplit.rs", "dlg_op"),
               # the dialogs' texts on their Rust objects (R6 WON GUI round 40)
               (RS / "ee_won_lobby" / "src" / "lobbytext.rs", "dlglogic_op")],
         # logicsplit.rs still composes StatusSetStatus and OkCancelSetText from these for its drivers' worlds
         allowed={v: "R6 WON GUI round 40: the game's compound is lobbytext.rs's (dlglogic_op); the worlds answer it"
                  for v in ("StatusTextOf", "StatusSetHeldText", "StatusTextDrop", "ButtonTextSet", "OkCancelSetHeldText")}),
    # LobbyConfig::PostParse (R6 WON round 32: lobbycfg.rs; LobbyConfig.cpp's host)
    dict(enum="EeWonLobbyCfgOp", header="won_lobby/lobbycfg.h",
         hosts=[LOBBY / "LobbyConfig.cpp"], rust=[], allowed={}),
    # the lobby's network entry points (R6 WON round 32: netdisp.rs; LobbyStagingSupport.cpp's host)
    dict(enum="EeWonNetDispOp", header="won_lobby/netdisp.h",
         hosts=[LOBBY / "LobbyStagingSupport.cpp"], rust=[], allowed={}),
    # running the lobby (R6 WON round 32: lobbyrun.rs; LobbyRunState.cpp's host)
    dict(enum="EeWonRunOp", header="won_lobby/lobbyrun.h",
         hosts=[LOBBY / "WONLobbyPrv.cpp"], rust=[], allowed={}),
    # the lobby's root screen (R6 WON round 32: rootscreen.rs; RootScreen.cpp's host)
    dict(enum="EeWonRootScreenOp", header="won_lobby/rootscreen.h",
         hosts=[LOBBY / "RootScreen.cpp"], rust=[],
         allowed={"EventControlId": "R6 WON GUI round 40: rootscreen.rs's op reads it in the game (lobbytext.rs); the tests' hosts answer it"}),
    # the game's WON front end (R6 WON round 31: wonnet.rs; its host's statements are ee/EEWonNetHost.inl)
    dict(enum="EeWonNetOp", header="won_lobby/wonnet.h",
         hosts=[ROOT / "include" / "ee" / "EEWonNetHost.inl"], rust=[], allowed={}),
    # the WONGUI component hosts' one-statement cases (R6 WON round 32: guisplit.rs composes them; checked from the
    # round's first value on: the older ops are answered by the components' Rust objects and hosts this tool does
    # not follow)
    dict(enum="EeWonGuiOp", header="won_gui/guihost.h", since="ClipboardText",
         hosts=sorted(GUI.glob("*.cpp")),
         rust=[(RS / "ee_won_gui" / "src" / "guisplit.rs", fn)
               for fn in ("text_op", "input_op", "label_op", "split_op", "list_op", "wm_op")] +
              # R6 WON GUI round 37: a game window's and the manager's own Rust parts (gamewin.rs)
              [(RS / "ee_won_gui" / "src" / "gamewin.rs", fn) for fn in ("win_op", "wm_ref_op")] +
              # R6 WON GUI round 38: the list and text areas' scroll-pane and Component statements, the children's, in the game
              [(RS / "ee_won_gui" / "src" / "gamewin.rs", fn) for fn in ("list_host_op", "child_op")] +
              # R6 WON GUI round 40: the text area's segments, its link and its copy, in the game
              [(RS / "ee_won_gui" / "src" / "gamewin.rs", "text_host_op")] +
              # R6 WON GUI round 41: the input box's own members, in the game
              [(RS / "ee_won_gui" / "src" / "inputobj.rs", "game_host_op")],
         allowed={}),
    # the front end's screens (R6 WON round 32: wonfront.rs; its host's statements are ee/EEWonFrontHost.inl)
    # the front end's UI-thread action (R6 WON lobby and network round 37: wonaction.rs; ee/EEAWonNetHost.inl)
    dict(enum="EeWonActionOp", header="won_lobby/wonaction.h",
         hosts=[ROOT / "include" / "ee" / "EEAWonNetHost.inl"], rust=[], allowed={}),
    dict(enum="EeWonFrontOp", header="won_lobby/wonfront.h",
         hosts=[ROOT / "include" / "ee" / "EEWonFrontHost.inl"], rust=[], allowed={}),
    # R6 WON round 33 (lobby): WONLobbyPrv (lobbyprv.rs), the model's buffers (modelbuf.rs), ChatCtrl's link
    # (chatlink.rs), LobbyConfig's constructor (lobbyconf.rs), the controls' config readers (lobbycfgrd.rs: four
    # units' hosts, the reads in LobbyNetCtrlHost.cpp's)
    dict(enum="EeWonLobbyPrvOp", header="won_lobby/lobbyprv.h", hosts=[LOBBY / "WONLobbyPrv.cpp"], rust=[], allowed={}),
    dict(enum="EeWonModelBufOp", header="won_lobby/modelbuf.h", hosts=[LOBBY / "LobbyModelHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "lobbytext.rs", "buf_op")], allowed={}),
    dict(enum="EeWonChatLinkOp", header="won_lobby/chatlink.h", hosts=[LOBBY / "LobbyNetCtrlHost.cpp"], rust=[], allowed={}),
    dict(enum="EeWonLobbyConfOp", header="won_lobby/lobbyconf.h", hosts=[LOBBY / "LobbyConfig.cpp"], rust=[], allowed={}),
    dict(enum="EeWonLobbyCfgRdOp", header="won_lobby/lobbycfgrd.h",
         hosts=[LOBBY / f for f in ("LobbyNetCtrlHost.cpp", "LobbyCtrlHost.cpp", "CrossPromotionButton.cpp", "CreditsDialog.cpp")],
         rust=[], allowed={}),
    # R6 WON round 34 (net): the model's directory reads (dirread.rs)
    dict(enum="EeWonDirReadOp", header="won_lobby/dirread.h", hosts=[LOBBY / "LobbyModelHost.cpp"],
         rust=[(RS / "ee_won_lobby" / "src" / "dirread.rs", "game_op")], allowed={}),
    # R6 WON lobby and network round 35: the typed config values' containers, the logic manager, the error screen
    dict(enum="EeWonCfgValOp", header="won_lobby/cfgval.h",
         hosts=[ROOT / "src" / "Empire Earth" / "WONAPI" / "WONConfig" / "ConfigObject.cpp"], rust=[], allowed={}),
    dict(enum="EeWonLogicMgrOp", header="won_lobby/logicmgr.h", hosts=[LOBBY / "WONLobbyPrv.cpp"], rust=[], allowed={}),
    dict(enum="EeWonErrScreenOp", header="won_lobby/errscreen.h", hosts=[LOBBY / "WONLobbyPrv.cpp"], rust=[], allowed={}),
    # R6 WON GUI round 35: the cursors' calls out (cursorobj.rs)
    dict(enum="EeWonCursorOp", header="won_gui/cursorobj.h", hosts=[GUI / "WindowManager.cpp"],
         rust=[(RS / "ee_won_gui" / "src" / "cursorobj.rs", "host", True)], allowed={}),
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
        if "since" in c:
            values = values[values.index(c["since"]):]
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
