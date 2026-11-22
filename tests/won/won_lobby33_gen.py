"""Writes tests/won/won_lobby33_orig.h (the deleted bodies from fb32b70d8, as free functions over `self`) and
tests/won/won_lobby33_new.h (the new hosts' cases and helpers as they are in the working tree)."""
import re
import subprocess

ROOT = "C:/Users/max/Desktop/empire_earth_next-w33lobby"
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
out += ["}", "#endif", ""]
open(ROOT + "/tests/won/won_lobby33_orig.h", "w", encoding="utf-8", newline="\n").write("\n\n".join(out))

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
new += ["}", "#endif", ""]
open(ROOT + "/tests/won/won_lobby33_new.h", "w", encoding="utf-8", newline="\n").write("\n\n".join(new))
print("ok")
