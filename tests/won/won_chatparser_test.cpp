// ChatCommandParser whole in Rust (R6 WONLobby round 17, rust/crates/ee_won_lobby/src/chatobj.rs): ChatCommandParser.cpp's
// statements as they were (tests/won/won_chatparser_orig.h, by tools/won_chatparser_orig.py) against the game's
// forwards (WONLobbyParser.h), each over its own world of real model shells (LobbyClientList, LobbyClient: the model's
// Rust objects) built the same way. Each sequence sets random command names and help lines in the resource slots,
// makes the room lists, and runs random steps: the client list set or dropped, inputs, name look-ups, the help, command
// names added, the settings, the world's clients renamed, added and removed, its own references given back, the
// commands initialised again. After every step both parsers' whole state (every getter, the target's and the list's
// identity, the command strings of ids 0..41), every object freed since the last step (in order) and every live
// object's reference count are compared; at the end the parsers are freed and the worlds emptied, and the traces compared again.
//   won_chatparser_test [--seqs N] [--seed S] [--print]     exit code: the number of mismatching sequences (max 100)
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONLobbyResourceGlobals.h"

using namespace WONAPI;

LobbyResourceSlot gLobbyResources[EE_WON_LOBBYRES_COUNT];
wchar_t gEmptyWideString[2];

#include "won_chatparser_orig.h"

// ---- the worlds: every model object's world and handle, and what happened to it

struct Tracked { int mWorld; int mHandle; };
static std::map<const void*, Tracked> sObjects;
static std::vector<std::string> sTrace[2];
static int sNextHandle[2];
static int sBuilding = -1; // the world the next shells belong to

static void Note(int inWorld, const std::string& inLine) { if (inWorld >= 0) sTrace[inWorld].push_back(inLine); }
static int HandleOf(const void* p)
{
    std::map<const void*, Tracked>::iterator i = sObjects.find(p);
    return i == sObjects.end() ? (p == NULL ? 0 : -1) : i->second.mHandle;
}
static int WorldOf(const void* p)
{
    std::map<const void*, Tracked>::iterator i = sObjects.find(p);
    return i == sObjects.end() ? -1 : i->second.mWorld;
}

namespace WONAPI {
static intptr_t ModelHost(EeWonModelOp inOp, intptr_t a, intptr_t, intptr_t)
{
    RefCount* r = reinterpret_cast<RefCount*>(a);
    // the references are compared as counts after each step (the original's SmartPtrs take theirs directly)
    if (inOp == EeWonModelOp::AddRef) {
        r->AddRef();
    } else if (inOp == EeWonModelOp::Release) {
        r->Release();
    } else {
        Note(WorldOf(r), "op " + std::to_string((int)inOp));
    }
    return 0;
}
LobbyModelObject::LobbyModelObject(EeWonModelKind inKind)
    : mModel(ee_won_model_new(this, inKind, ModelHost, GetEmptyGUIStringData()))
{
    Tracked t = {sBuilding, sBuilding >= 0 ? ++sNextHandle[sBuilding] : 0};
    sObjects[this] = t;
}
LobbyModelObject::~LobbyModelObject()
{
    Tracked t = sObjects[this];
    Note(t.mWorld, "delete " + std::to_string(t.mHandle));
    sObjects.erase(this);
    ee_won_model_free(this->mModel);
}
}

// ---- random numbers (one stream for the choices, shared by both sides' steps)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n == 0 ? 0 : Rand() % n; }

static std::wstring W(const char* s) { std::wstring w; while (*s) w += (wchar_t)(unsigned char)*s++; return w; }
static std::string Units(const GUIString& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (int i = 0; i < inText.length(); i++) {
        unsigned c = (unsigned)inText.at(i);
        if (c > 0x20 && c < 0x7f && c != '%') anOut += (char)c;
        else { snprintf(aBuffer, sizeof(aBuffer), "%%%x;", c); anOut += aBuffer; }
    }
    return anOut;
}

static const wchar_t* const kNames[] = {
    L"Alice", L"alice", L"Alicia", L"Al", L"Bob", L"BOB", L"Bobby", L"Carl Jr", L"carl", L"\x00e9mile", L"\x00c9MILE",
    L"\x263a", L"Dave", L"dave", L"D", L"x y", L"\"q\"", L"", L"Zed", L"zed2",
};
static const unsigned kNameCount = sizeof(kNames) / sizeof(kNames[0]);
static const char* const kWords[] = {
    "w", "whisper", "t", "tell", "e", "em", "me", "emote", "i", "ignore", "away", "afk", "block", "invite",
    "uninvite", "r", "reply", "clr", "cls", "?", "h", "help", "team", "mod", "smute", "sban", "mute", "ban",
    "unmute", "sunmute", "unban", "sunban", "alert", "warn", "startshutdown", "abortshutdown", "zap", "W", "Tell",
};
static const char* const kTexts[] = {"", " ", "hi", "hi there", "  two  spaces ", "5m", "2h rude", "INFINITE x",
                                     "10", "1d", "0s", "-1h", "\"quoted\"", "x\ty"};
static const wchar_t* const kSpellings[] = {
    L"w, whisper, t, tell", L"", L" ", L"e,em, me ,emote", L"\"a b\", c", L"x,,y", L"one", L"ONE, two", L",",
    L"\x00e9t\x00e9, \x263a", L"L\"wide\"", L"a\"+\"b", L"h, ?, help",
};
static const unsigned kSpellingCount = sizeof(kSpellings) / sizeof(kSpellings[0]);

static void SetResources()
{
    for (int c = 0; c <= (int)EeWonLobbyRes::ChatCommandLogic_WarnHelp_String; c++)
        gLobbyResources[c].mString = GUIString();
    // the chat rows: random spellings (the help lines a tag each)
    for (int r = 0; r < EE_WON_LOBBYRES_COUNT; r++) {
        const char* aName = NULL;
        EeWonResource aRow;
        if (!ee_won_resource_row(r, &aRow)) continue;
        aName = aRow.name;
        if (strncmp(aName, "ChatCommandLogic_", 17) != 0) continue;
        if (strstr(aName, "Help_String") != NULL || strstr(aName, "Header") != NULL) {
            std::wstring t = W(aName + 17);
            if (R(5) == 0) t += L" \x00e9";
            gLobbyResources[r].mString = GUIString(t);
        } else {
            gLobbyResources[r].mString = GUIString(kSpellings[R(kSpellingCount)]);
        }
    }
}

struct World {
    std::vector<LobbyClientList*> mLists; // the world's own references (NULL once given back)
    std::vector<std::vector<unsigned short> > mIds;
};

static void AddClient(World& w, size_t inList, unsigned short inId, const wchar_t* inName, unsigned long inFlags,
                      bool inGame, bool inChat)
{
    LobbyClient* c = new LobbyClient;
    c->SetClientId(inId);
    c->SetName(GUIString(inName));
    c->SetClientFlags(inFlags);
    c->SetCaptain(inGame, true);
    c->SetCaptain(inChat, false);
    w.mLists[inList]->AddClient(c);
    w.mIds[inList].push_back(inId);
}

// one world's construction from a recorded script (so both are made the same way)
struct ClientSpec { size_t mList; unsigned short mId; unsigned mName; unsigned long mFlags; bool mGame, mChat; };
static void Build(World& w, int inWorld, size_t inLists, const std::vector<ClientSpec>& inSpecs)
{
    sBuilding = inWorld;
    for (size_t i = 0; i < inLists; i++) {
        LobbyClientList* l = new LobbyClientList;
        l->AddRef();
        w.mLists.push_back(l);
        w.mIds.push_back(std::vector<unsigned short>());
    }
    for (size_t i = 0; i < inSpecs.size(); i++)
        AddClient(w, inSpecs[i].mList, inSpecs[i].mId, kNames[inSpecs[i].mName], inSpecs[i].mFlags, inSpecs[i].mGame,
                  inSpecs[i].mChat);
}

template <class P> static std::string State(P& p, int r, int inName)
{
    char aLine[8192];
    snprintf(aLine, sizeof(aLine), "r=%d err=%d cmd=%d name=%s target=%d text=%s dur=%u inv=%d prefix=%d flags=%lu list=%d",
             r, p.GetError(), p.GetCommand(), Units(p.GetTargetName()).c_str(), HandleOf(p.GetTarget()),
             Units(p.GetText()).c_str(), (unsigned)p.GetDuration(), p.GetInvitation() ? 1 : 0, p.GetPrefix(),
             (unsigned long)p.GetRoomFlags(), HandleOf(p.GetClients()));
    std::string s = aLine;
    for (int id = 0; id <= 41; id++) s += " " + Units(p.GetCommandString(id));
    s += std::string(" ") + p.GetCommandName(inName);
    return s;
}

static std::wstring Line(int inHasList)
{
    std::wstring s;
    unsigned k = R(10);
    if (k == 0) return W(kTexts[R(sizeof(kTexts) / sizeof(kTexts[0]))]);
    if (k == 1) return L":" + W(kTexts[R(sizeof(kTexts) / sizeof(kTexts[0]))]);
    s = R(8) == 0 ? L"!" : L"/";
    s += W(kWords[R(sizeof(kWords) / sizeof(kWords[0]))]);
    if (R(6) == 0) return s;
    s += L" ";
    std::wstring n = kNames[R(kNameCount)];
    if (R(3) == 0 && n.length() > 1) n.resize(1 + R((unsigned)n.length() - 1));
    if (R(4) == 0) n = L"\"" + n + L"\"";
    s += n;
    if (R(3) != 0) { s += L" "; s += W(kTexts[R(sizeof(kTexts) / sizeof(kTexts[0]))]); }
    (void)inHasList;
    return s;
}

int main(int argc, char** argv)
{
    int aSeqs = 3000;
    unsigned aSeed = 17;
    bool aPrint = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seqs") && i + 1 < argc) aSeqs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) aSeed = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--print")) aPrint = true;
    }
    int aBad = 0;
    long long aChecks = 0, aLines = 0;
    for (int q = 0; q < aSeqs; q++) {
        sSeed = aSeed * 7919u + (unsigned)q * 104729u;
        SetResources();
        size_t aListCount = 1 + R(3);
        std::vector<ClientSpec> aSpecs;
        for (size_t l = 0; l < aListCount; l++) {
            unsigned n = R(9);
            for (unsigned k = 0; k < n; k++) {
                static const unsigned long kFlags[] = {0, 1, 0x20, 0x21, 0x10, 0xff};
                ClientSpec c = {l, (unsigned short)(1 + R(12)), R(kNameCount), kFlags[R(6)], R(3) == 0, R(3) == 0};
                aSpecs.push_back(c);
            }
        }
        World w[2];
        sNextHandle[0] = sNextHandle[1] = 0;
        Build(w[0], 0, aListCount, aSpecs);
        Build(w[1], 1, aListCount, aSpecs);
        sBuilding = -1;
        Orig::ChatCommandParser* o = new Orig::ChatCommandParser;
        ChatCommandParser* g = new ChatCommandParser;
        bool aInit = R(10) != 0;
        if (aInit) { o->InitCommands(); g->InitCommands(); }
        std::string aFirst;
        int aSteps = 1 + (int)R(40);
        for (int s = 0; s <= aSteps; s++) {
            int r[2] = {0, 0};
            unsigned aOp = s == aSteps ? 99 : R(12);
            switch (aOp) {
            case 0: {
                int l = (int)R((unsigned)aListCount + 1) - 1; // -1: none
                int f = (int)R(16) - (R(9) == 0 ? 20 : 0);
                o->SetClientList(l >= 0 ? w[0].mLists[l] : NULL, f);
                g->SetClientList(l >= 0 ? w[1].mLists[l] : NULL, f);
                break;
            }
            case 1: case 2: case 3: {
                GUIString t(Line(1));
                r[0] = o->HandleInput(t);
                r[1] = g->HandleInput(t);
                break;
            }
            case 4: {
                std::wstring n = kNames[R(kNameCount)];
                if (R(2) == 0 && n.length() > 1) n.resize(1 + R((unsigned)n.length() - 1));
                int m[2] = {-7, -7};
                bool aNull = R(5) == 0;
                LobbyClient* c0 = o->FindClient(GUIString(n), aNull ? NULL : &m[0]);
                LobbyClient* c1 = g->FindClient(GUIString(n), aNull ? NULL : &m[1]);
                r[0] = HandleOf(c0) * 10 + m[0];
                r[1] = HandleOf(c1) * 10 + m[1];
                break;
            }
            case 5: {
                unsigned short id = (unsigned short)(1 + R(14));
                std::string a = Units(o->GetHelp(id)), b = Units(g->GetHelp(id));
                r[0] = 0; r[1] = a == b ? 0 : 1;
                if (a != b && aFirst.empty()) {
                    aFirst = "help " + a + " | " + b;
                    LobbyClientList* l[2] = {o->GetClients(), g->GetClients()};
                    for (int x = 0; x < 2; x++) {
                        LobbyClient* c = l[x] != NULL ? l[x]->FindClient(id) : NULL;
                        aFirst += " / side " + std::to_string(x) + " list " + std::to_string(HandleOf(l[x])) + " client " +
                                  std::to_string(HandleOf(c)) + " flags " + std::to_string(o->GetRoomFlags()) + "/" +
                                  std::to_string(g->GetRoomFlags());
                        if (c) aFirst += " mod " + std::to_string(c->IsModerator()) + " cap " + std::to_string(c->IsCaptain(false)) +
                                         std::to_string(c->IsCaptain(true)) + " adm " + std::to_string(c->IsAdmin());
                    }
                }
                break;
            }
            case 6: {
                GUIString n(R(4) == 0 ? std::wstring() : W(kWords[R(sizeof(kWords) / sizeof(kWords[0]))]));
                int id = (int)R(45);
                o->AddCommand(n, id);
                g->AddCommand(n, id);
                break;
            }
            case 7: {
                std::wstring n = kSpellings[R(kSpellingCount)];
                int id = (int)R(45);
                o->AddCommands(n, id);
                g->AddCommands(n, id);
                break;
            }
            case 8: {
                unsigned k = R(4);
                if (k == 0) { GUIString n(kNames[R(kNameCount)]); o->SetInput(n); g->SetInput(n); }
                else if (k == 1) { int p = R(2) ? '/' : '!'; o->SetPrefix(p); g->SetPrefix(p); }
                else if (k == 2) { int t = (int)R(5); o->SetMatchThreshold(t); g->SetMatchThreshold(t); }
                else { bool a = R(2) != 0; o->SetAllowColonEmote(a); g->SetAllowColonEmote(a); }
                break;
            }
            case 9: { // the world's clients changed, the same on both
                size_t l = R((unsigned)aListCount);
                unsigned k = R(3);
                unsigned short id = (unsigned short)(1 + R(12));
                unsigned nm = R(kNameCount);
                unsigned long fl = R(2) ? 1 : 0;
                for (int x = 0; x < 2; x++) {
                    if (w[x].mLists[l] == NULL) continue;
                    sBuilding = x;
                    if (k == 0) {
                        LobbyClient* c = w[x].mLists[l]->FindClient(id);
                        if (c != NULL) c->SetName(GUIString(kNames[nm]));
                    } else if (k == 1) {
                        AddClient(w[x], l, id, kNames[nm], fl, false, false);
                    } else {
                        w[x].mLists[l]->RemoveClient(id);
                    }
                    sBuilding = -1;
                }
                break;
            }
            case 10: { // a world reference given back (the parser's keeps the list alive)
                size_t l = R((unsigned)aListCount);
                for (int x = 0; x < 2; x++)
                    if (w[x].mLists[l] != NULL) { w[x].mLists[l]->Release(); w[x].mLists[l] = NULL; }
                break;
            }
            case 11:
                o->InitCommands();
                g->InitCommands();
                break;
            case 99:
                delete o;
                delete g;
                for (int x = 0; x < 2; x++)
                    for (size_t l = 0; l < w[x].mLists.size(); l++)
                        if (w[x].mLists[l] != NULL) w[x].mLists[l]->Release();
                break;
            }
            aChecks++;
            std::string a, b;
            if (aOp != 99) {
                int aName = (int)R(30);
                a = State(*o, r[0], aName);
                b = State(*g, r[1], aName);
            }
            // every live object's reference count, by handle
            for (int x = 0; x < 2; x++) {
                std::map<int, int> aCounts;
                for (std::map<const void*, Tracked>::iterator i = sObjects.begin(); i != sObjects.end(); ++i)
                    if (i->second.mWorld == x)
                        aCounts[i->second.mHandle] = (int)static_cast<const LobbyModelObject*>(i->first)->GetRefCount();
                std::string c = "refs";
                for (std::map<int, int>::iterator i = aCounts.begin(); i != aCounts.end(); ++i)
                    c += " " + std::to_string(i->first) + ":" + std::to_string(i->second);
                sTrace[x].push_back(c);
            }
            aLines += (long long)sTrace[0].size();
            if (a != b && aFirst.empty()) aFirst = "step " + std::to_string(s) + " op " + std::to_string(aOp) + "\n  orig " + a + "\n  game " + b;
            if (sTrace[0] != sTrace[1] && aFirst.empty()) {
                aFirst = "step " + std::to_string(s) + " op " + std::to_string(aOp) + " trace:";
                for (size_t i = 0; i < sTrace[0].size() || i < sTrace[1].size(); i++)
                    aFirst += "\n  " + (i < sTrace[0].size() ? sTrace[0][i] : std::string("(none)")) + "  |  " +
                              (i < sTrace[1].size() ? sTrace[1][i] : std::string("(none)"));
            }
            if (aPrint) printf("seq %d step %d op %u %s\n", q, s, aOp, a.c_str());
            sTrace[0].clear();
            sTrace[1].clear();
        }
        if (!sObjects.empty() && aFirst.empty()) aFirst = "objects left: " + std::to_string(sObjects.size());
        sObjects.clear();
        if (!aFirst.empty()) {
            if (aBad < 5) printf("FAIL seq %d: %s\n", q, aFirst.c_str());
            aBad++;
        }
    }
    printf("won_chatparser_test: %d sequences, %lld steps, %lld trace lines, %d mismatching\n", aSeqs, aChecks, aLines, aBad);
    return aBad > 100 ? 100 : aBad;
}
