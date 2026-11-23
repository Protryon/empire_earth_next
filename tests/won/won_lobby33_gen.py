"""Writes tests/won/won_lobby33_orig.h (the deleted bodies from fb32b70d8, as free functions over `self`) and
tests/won/won_lobby33_new.h (the new hosts' cases and helpers as they are in the working tree)."""
import re
import subprocess

import pathlib
ROOT = str(pathlib.Path(__file__).resolve().parents[2]).replace("\\", "/")
BASE = "fb32b70d8"
L = "src/Empire Earth/WONAPI/WONLobby/"


def show(path):
    return subprocess.run(["git", "-C", ROOT, "show", f"{BASE}:{path}"], capture_output=True, check=True).stdout.decode()


def body_at(text, start):
    """the text from `start` to the matching close of the first '{' after it"""
    i = text.index("{", start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[start:j + 1]
    raise ValueError(start)


def func(text, sig, new_sig):
    i = text.index(sig)
    b = body_at(text, i)
    b = new_sig + b[len(sig):]
    return b.replace("this->", "self->").replace("LobbyGame::mGameFactory", "LobbyGame::mGameFactory")


out = ["// the bodies R6 WON round 33 moved to Rust, as they were at fb32b70d8 (by tests/won/won_lobby33_gen.py),",
       "// over won_lobby33_world.h's stand-ins: a member function as a free function of `self`",
       "#ifndef WON_LOBBY33_ORIG_H", "#define WON_LOBBY33_ORIG_H", "namespace L33Orig {", "using namespace L33World;"]
prv = show(L + "WONLobbyPrv.cpp")
out.append(func(prv, "static ResourceConfigTablePtr GetLobbyResourceConfigTable(ResourceConfigTable *theTable)",
                "static ResourceConfigTablePtr GetLobbyResourceConfigTable(ResourceConfigTable *theTable)"))
out.append(func(prv, "bool WONLobbyPrv::Init(ResourceConfigTable* input,void (*initFunc)(ComponentConfig*))",
                "static bool PrvInit(WONLobbyPrv* self, ResourceConfigTable* input,void (*initFunc)(ComponentConfig*))"))
out.append(func(prv, "void WONLobbyPrv::Destroy()", "static void PrvDestroy(WONLobbyPrv* self)"))
out.append(func(prv, "void WONLobbyPrv::InitAuth(const std::wstring& loginCommunity,const char* hashFileListStr)",
                "static void PrvInitAuth(WONLobbyPrv* self, const std::wstring& loginCommunity,const char* hashFileListStr)"))
out.append(func(prv, "WONLobbyPrv::WONLobbyPrv()", "static void PrvConstruct(WONLobbyPrv* self)"))

mh = show(L + "LobbyModelHost.cpp")
i = mh.index("static void ModelAppendBytes")
j = mh.index("// ---- the directory's records")
out.append(mh[i:j].strip())
for sig, new in [
    ("void LobbyGame::WriteSummary(WriteBuffer& outBuffer)", "static void WriteSummary(LobbyGame* self, WriteBuffer& outBuffer)"),
    ("bool LobbyGame::ReadSummary(ReadBuffer& inBuffer)", "static bool ReadSummary(LobbyGame* self, ReadBuffer& inBuffer)"),
    ("bool LobbyGame::ReadDetails(ReadBuffer& inBuffer)", "static bool ReadDetails(LobbyGame* self, ReadBuffer& inBuffer)"),
    ("ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinRequest()", "static ConstSmartPtr<ByteBuffer> GetJoinRequest(LobbyGame* self)"),
    ("short LobbyGame::HandleJoinRequest(ReadBuffer& inBuffer,LobbyClient* inClient,WriteBuffer& outBuffer)",
     "static short HandleJoinRequest(LobbyGame* self, ReadBuffer& inBuffer,LobbyClient* inClient,WriteBuffer& outBuffer)"),
    ("ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinReply(ReadBuffer& inBuffer,LobbyClient* inClient)",
     "static ConstSmartPtr<ByteBuffer> GetJoinReply(LobbyGame* self, ReadBuffer& inBuffer,LobbyClient* inClient)"),
    ("short LobbyGame::HandleJoinReply(ReadBuffer& inBuffer)", "static short HandleJoinReply(LobbyGame* self, ReadBuffer& inBuffer)"),
    ("GUIString LobbyGame::GetStatusString(short status)", "static GUIString GetStatusString(LobbyGame* self, short status)"),
    ("bool LobbyPlayer::ReadFromBuffer(ReadBuffer& inBuffer)", "static bool ReadFromBuffer(LobbyPlayer* self, ReadBuffer& inBuffer)"),
    ("void LobbyPlayer::WriteToBuffer(WriteBuffer& outBuffer)", "static void WriteToBuffer(LobbyPlayer* self, WriteBuffer& outBuffer)"),
]:
    out.append(func(mh, sig, new))
nc = show(L + "LobbyNetCtrlHost.cpp")
i = nc.index("    explicit ChatLink(const EeWonLobbyCtlLink& l) : mKind(l.kind)")
b = body_at(nc, i)
b = b[b.index("{"):].replace("this->", "self->")
out.append("static void ChatLinkInit(ChatLink* self, const EeWonLobbyCtlLink& l)\n{\n    self->mKind = l.kind;\n    "
           + b.strip() + "\n}")
lc = show(L + "LobbyConfig.cpp")
out.append(func(lc, "LobbyConfig::LobbyConfig()", "static void ConfConstruct(LobbyConfig* self)"))
out.append(func(lc, "bool LobbyConfig::HandleValueInstruction(ConfigParser* parser, const std::string& key,\n                           StringParser& values)",
                "static bool ConfHandleValue(LobbyConfig* self, ConfigParser* parser, const std::string& key, StringParser& values)")
           .replace("return ConfigObject::", "return self->ConfigObject::"))
READERS = [
    ("LobbyNetCtrlHost.cpp", "bool RoomLabelCtrlConfig::Virtual24(const std::string& inKey)", "static bool RoomLabel_Virtual24(RoomLabelCtrlConfig* self, const std::string& inKey)"),
    ("LobbyNetCtrlHost.cpp", "bool UserListCtrlConfig::Virtual24(const std::string& instruction)", "static bool UserList_Virtual24(UserListCtrlConfig* self, const std::string& instruction)"),
    ("LobbyNetCtrlHost.cpp", "bool ChatCtrlConfig::Virtual24(const std::string& inKey)", "static bool Chat_Virtual24(ChatCtrlConfig* self, const std::string& inKey)"),
    ("LobbyNetCtrlHost.cpp", "bool ServerListCtrlConfig::GetNamedValue(const std::string& inName, int& outValue)", "static bool ServerList_GetNamedValue(ServerListCtrlConfig* self, const std::string& inName, int& outValue)"),
    ("LobbyNetCtrlHost.cpp", "bool ServerListCtrlConfig::Virtual24(const std::string& inInstruction)", "static bool ServerList_Virtual24(ServerListCtrlConfig* self, const std::string& inInstruction)"),
    ("LobbyNetCtrlHost.cpp", "void ServerListCtrlConfig::HandleAddColumn()", "static void ServerList_HandleAddColumn(ServerListCtrlConfig* self)"),
    ("LobbyCtrlHost.cpp", "bool GameBrowserCtrlConfig::Virtual24(const std::string& instruction)", "static bool GameBrowser_Virtual24(GameBrowserCtrlConfig* self, const std::string& instruction)"),
    ("LobbyCtrlHost.cpp", "bool GameUserListCtrlConfig::Virtual24(const std::string& instruction)", "static bool GameUserList_Virtual24(GameUserListCtrlConfig* self, const std::string& instruction)"),
    ("CrossPromotionButton.cpp", "bool CrossPromotionButtonConfig::Virtual24(const std::string& key)", "static bool Cross_Virtual24(CrossPromotionButtonConfig* self, const std::string& key)"),
    ("CrossPromotionButton.cpp", "bool ImageTextButtonConfig::Virtual24(const std::string& key)", "static bool ImageText_Virtual24(ImageTextButtonConfig* self, const std::string& key)"),
    ("CreditsDialog.cpp", "bool CreditsDialogConfig::Virtual24(const std::string& inInstruction)", "static bool Credits_Virtual24(CreditsDialogConfig* self, const std::string& inInstruction)"),
]
for f, sig, new_sig in READERS:
    b = func(show(L + f), sig, new_sig)
    for base in ("ContainerConfig::Virtual24(", "ComponentConfig::Virtual24(", "ComponentConfig::GetNamedValue("):
        b = b.replace(base, "self->" + base).replace("self->self->", "self->")
    out.append(b)
out += ["}", "#endif", ""]
open(ROOT + "/tests/won/won_lobby33_orig.h", "w", encoding="utf-8", newline="\n").write(
    "\n".join(out[:6]) + "\n\n" + "\n\n".join(out[6:]))

# the new side: the hosts' cases and helpers as they are
new = ["// the hosts' statements R6 WON round 33 added (by tests/won/won_lobby33_gen.py from the working tree), over",
       "// won_lobby33_world.h's stand-ins", "#ifndef WON_LOBBY33_NEW_H", "#define WON_LOBBY33_NEW_H",
       "namespace L33New {", "using namespace L33World;"]
cur = open(ROOT + "/" + L + "WONLobbyPrv.cpp", encoding="utf-8").read()
i = cur.index("// the methods' statements (lobbyprv.rs composes them")
j = cur.index("// Starts the lobby:", i)
new.append(cur[i:j].strip())
cur = open(ROOT + "/" + L + "LobbyModelHost.cpp", encoding="utf-8").read()
i = cur.index("static std::wstring UnitsText")
new.append(body_at(cur, i))
i = cur.index("static intptr_t HeldConst")
j = cur.index("static intptr_t Buf(void* inObj", i)
new.append(cur[i:j].strip())
cur = open(ROOT + "/" + L + "LobbyNetCtrlHost.cpp", encoding="utf-8").read()
i = cur.index("// ChatLink's constructor's statements")
new.append(body_at(cur, cur.index("static intptr_t ChatLinkHost", i)))
cur = open(ROOT + "/" + L + "LobbyConfig.cpp", encoding="utf-8").read()
i = cur.index("// the constructor's and HandleValueInstruction's statements")
new.append("\n".join(l for l in cur[i:cur.index("static intptr_t ConfHost", i)].split("\n") if l.startswith("static")))
new.append(body_at(cur, cur.index("static intptr_t ConfHost", i)))
for f, tag in (("LobbyNetCtrlHost.cpp", "Net"), ("LobbyCtrlHost.cpp", "Ctl"), ("CrossPromotionButton.cpp", "Cross"),
               ("CreditsDialog.cpp", "Credits")):
    cur = open(ROOT + "/" + L + f, encoding="utf-8").read()
    i = cur.index("static intptr_t CfgRdHost(")
    k = cur.rfind("\n//", 0, i)
    j = cur.index("static intptr_t CfgRd(ComponentConfig*", i)
    if tag == "Net":
        k = cur.index("// ---- the config readers: their bodies are lobbycfgrd.rs's")
    part = cur[k:j]
    part = "\n".join(l for l in part.split("\n") if not l.startswith("intptr_t LobbyCfgReadHost(ComponentConfig* c, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc);"))
    new.append(part.replace("static intptr_t CfgRdHost(", "static intptr_t CfgRdHost" + tag + "(").replace("\nintptr_t LobbyCfgReadHost(", "\nstatic intptr_t LobbyCfgReadHost(").strip())
new += ["}", "#endif", ""]
open(ROOT + "/tests/won/won_lobby33_new.h", "w", encoding="utf-8", newline="\n").write(
    "\n".join(new[:6]) + "\n\n" + "\n\n".join(new[6:]))
print("ok")
