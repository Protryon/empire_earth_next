// WONLobbyPrv's methods and the lobby model's buffer methods R6 WON round 33 made Rust (ee_won_lobby lobbyprv.rs,
// modelbuf.rs) against the deleted bodies as they were (won_lobby33_orig.h, by tests/won/won_lobby33_gen.py from
// fb32b70d8), over one stand-in world (won_lobby33_world.h) that logs every call with an effect. The Rust's calls
// out are answered by the hosts' new one-statement cases and helpers as they are (won_lobby33_new.h, by the same
// script from the tree). Each case sets the world's answers alike for the two sides (the file open, the parses,
// the logic objects' init and the caller's, the defines, the UDP manager and auth context, the hash list, the CD
// key check; the game's fields and hooks, the buffers' bytes, the clients and their names, the config), runs one
// call, and the traces, the objects' references and the state after it must be the same. EE_L33_MUTATE=k breaks the
// new side's host in one place (the mutations' table below) to show the cases catch it. The exit code is the number
// of mismatches (0 with a mutation means it was missed: the exit code is then 1).
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "won_lobby33_world.h"
#include "rust/won_lobby/lobbyprv.h"
#include "rust/won_lobby/modelbuf.h"
#include "rust/won_lobby/chatlink.h"
#include "won_lobby33_orig.h"
#include "won_lobby33_new.h"

using namespace L33World;

// ---- the members that dispatch to a side (a body the moved bodies call)

static intptr_t NewBuf(void* inObj, EeWonModelBufCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0);
short LobbyGame::HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer)
{
    if (sSide == 0) return L33Orig::HandleJoinRequest(this, inBuffer, inClient, outBuffer);
    return (short)NewBuf(this, EeWonModelBufCall::HandleJoinRequest, (intptr_t)&inBuffer, (intptr_t)inClient, (intptr_t)&outBuffer);
}
bool LobbyPlayer::ReadFromBuffer(ReadBuffer& inBuffer)
{
    if (sSide == 0) return L33Orig::ReadFromBuffer(this, inBuffer);
    return NewBuf(this, EeWonModelBufCall::PlayerReadFrom, (intptr_t)&inBuffer) != 0;
}
void LobbyPlayer::WriteToBuffer(WriteBuffer& outBuffer)
{
    if (sSide == 0) return L33Orig::WriteToBuffer(this, outBuffer);
    NewBuf(this, EeWonModelBufCall::PlayerWriteTo, (intptr_t)&outBuffer);
}

// ---- the mutations: the new side's host broken in one place

static int sMutate;
static std::map<int, int> sPrvHits, sBufHits; // the ops the new side reached
static intptr_t MutPrv(void* inPrv, EeWonLobbyPrvOp inOp, intptr_t a)
{
    typedef EeWonLobbyPrvOp Op;
    sPrvHits[(int)inOp]++;
    if (sMutate == 1 && inOp == Op::DefineAdd) return 0;
    if (sMutate == 2 && inOp == Op::ParserFree) return 0;
    if (sMutate == 3 && inOp == Op::ErrorParseReason) return 0;
    if (sMutate == 4 && inOp == Op::AuthSetCdKey) return 0;
    if (sMutate == 5 && inOp == Op::AddLanLogic) return 0;
    intptr_t r = L33New::PrvHost(inPrv, inOp, a);
    if (sMutate == 6 && inOp == Op::UdpIs) return !r;
    return r;
}
static intptr_t MutBuf(void* inObj, EeWonModelBufOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonModelBufOp Op;
    sBufHits[(int)inOp]++;
    if (sMutate == 7 && inOp == Op::AppendShort) b += 1;
    if (sMutate == 8 && inOp == Op::ClientSetPlayer && b == 0) return 0;
    if (sMutate == 9 && inOp == Op::ConfigAllowDuplicates) return 1;
    if (sMutate == 10 && inOp == Op::StatusHook) return L33New::BufHost(inObj, Op::StatusSuccess, 0, 0, 0);
    if (sMutate == 11 && inOp == Op::SetHaveDetails) return 0;
    if (sMutate == 12 && inOp == Op::Release && reinterpret_cast<RefCount*>(a)->mId >= 0) return 0;
    if (sMutate == 13 && inOp == Op::SelfSetReady) return 0;
    return L33New::BufHost(inObj, inOp, a, b, c);
}
static std::map<int, int> sLinkHits;
static intptr_t MutLink(void* inLink, EeWonChatLinkOp inOp, intptr_t a)
{
    sLinkHits[(int)inOp]++;
    if (sMutate == 14 && inOp == EeWonChatLinkOp::SetText) return 0;
    if (sMutate == 15 && inOp == EeWonChatLinkOp::ClearPrevious) return 0;
    if (sMutate == 16 && inOp == EeWonChatLinkOp::AddressOfServer) return 0;
    return L33New::ChatLinkHost(inLink, inOp, a);
}
static intptr_t NewBuf(void* inObj, EeWonModelBufCall inCall, intptr_t a, intptr_t b, intptr_t c)
{
    return ee_won_modelbuf_call(inObj, MutBuf, inCall, a, b, c);
}

// ---- the comparison

static int sChecks, sFailures;
static std::map<std::string, int> sFailed;
static void Compare(const std::string& inWhat, std::vector<std::string> inTraces[2])
{
    sChecks++;
    if (inTraces[0] == inTraces[1]) return;
    sFailed[inWhat]++;
    if (sFailures++ < 6) {
        printf("FAIL %s:\n", inWhat.c_str());
        for (size_t i = 0; i < inTraces[0].size() || i < inTraces[1].size(); i++) {
            const std::string& x = i < inTraces[0].size() ? inTraces[0][i] : std::string();
            const std::string& y = i < inTraces[1].size() ? inTraces[1][i] : std::string();
            printf("  %s %-70.70s | %.70s\n", x == y ? " " : "*", x.c_str(), y.c_str());
        }
    }
}
static void Refs()
{
    for (RefCount* o : sObjects) Log("refs " + Id(o) + " " + N(o->mRefs));
}

// ---- WONLobbyPrv: every combination of the world's answers its callers allow

static void TestInit(ComponentConfig* c)
{
    Log("initFunc " + Id(c));
    if (sInitFunc == 2) throw WorldThrow{5};
    if (sInitFunc == 3) throw ConfigObjectException("the game's config error");
}
static void PrvCases()
{
    static const char* kHashes[3] = {NULL, "a.dll", "a.dll,b.exe,c.dat"};
    // Init: open, parse, read master (0..2), logic init (0..3 but 0), initFunc (none, ok, throw, config), defines 0..2
    for (int k = 0; k < 3 * 3 * 3 * 3 * 4 * 3; k++) {
        std::vector<std::string> aTraces[2];
        int v = k;
        int open = v % 3; v /= 3;
        int parse = v % 3; v /= 3;
        int master = v % 3; v /= 3;
        int logic = 1 + v % 3; v /= 3;
        int func = v % 4; v /= 4;
        int defines = v % 3;
        for (int aSide = 0; aSide < 2; aSide++) {
            sObjects.clear();
            sTrace.clear();
            ConfigParser::sNext = 0;
            sOpen = open, sParse = parse, sReadMaster = master, sLogicInit = logic == 1 ? 1 : logic == 2 ? 2 : 3;
            sInitFunc = func == 0 ? 1 : func == 1 ? 1 : func == 2 ? 2 : 3;
            WONLobbyPrv p;
            p.mConfig = new LobbyConfig;
            p.mChatCommandParser = new ChatCommandParser;
            static const char* kKeys[2][2] = {{"Product", "EmpireEarth"}, {"lan", "1"}};
            for (int d = 0; d < defines; d++) p.mConfig->mDefines[kKeys[d][0]] = kKeys[d][1];
            ResourceConfigTable* anInput = new ResourceConfigTable;
            void (*aFunc)(ComponentConfig*) = func == 0 ? NULL : TestInit;
            sTrace.clear();
            try {
                bool r;
                if (aSide == 0) r = L33Orig::PrvInit(&p, anInput, aFunc);
                else r = WonRethrow(ee_won_lobbyprv_call(&p, MutPrv, EeWonLobbyPrvCall::Init, (intptr_t)anInput, (intptr_t)aFunc)) != 0;
                Log("result " + N(r));
            } catch (WorldThrow& e) {
                Log("threw " + N(e.mWhich));
            }
            p.Dump();
            Refs();
            aTraces[aSide].swap(sTrace);
        }
        Compare("Init", aTraces);
    }
    // Destroy: the UDP manager and the auth context there or not
    for (int k = 0; k < 4; k++) {
        std::vector<std::string> aTraces[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            sObjects.clear();
            sTrace.clear();
            WONLobbyPrv p;
            if (k & 1) p.mUDPManager = new UDPManager;
            if (k & 2) p.mAuthContext = new AuthContext;
            sTrace.clear();
            if (aSide == 0) L33Orig::PrvDestroy(&p);
            else ee_won_lobbyprv_call(&p, MutPrv, EeWonLobbyPrvCall::Destroy, 0, 0);
            p.Dump();
            Refs();
            aTraces[aSide].swap(sTrace);
        }
        Compare("Destroy", aTraces);
    }
    // InitAuth: the hash lists, the CD key check
    for (int k = 0; k < 6; k++) {
        std::vector<std::string> aTraces[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            sObjects.clear();
            sTrace.clear();
            WONLobbyPrv p;
            p.mConfig = new LobbyConfig;
            p.mConfig->mCDKeyCheck = k >= 3;
            p.mAuthContext = new AuthContext;
            std::wstring aCommunity(L"EmpireEarth");
            sTrace.clear();
            if (aSide == 0) L33Orig::PrvInitAuth(&p, aCommunity, kHashes[k % 3]);
            else ee_won_lobbyprv_call(&p, MutPrv, EeWonLobbyPrvCall::InitAuth, (intptr_t)&aCommunity, (intptr_t)kHashes[k % 3]);
            p.Dump();
            Refs();
            aTraces[aSide].swap(sTrace);
        }
        Compare("InitAuth", aTraces);
    }
    // the constructor
    {
        std::vector<std::string> aTraces[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            sObjects.clear();
            sTrace.clear();
            sAllocatedConfig = new LobbyConfig;
            WONLobbyPrv p;
            if (aSide == 0) L33Orig::PrvConstruct(&p);
            else ee_won_lobbyprv_call(&p, MutPrv, EeWonLobbyPrvCall::Construct, 0, 0);
            p.Dump();
            Refs();
            aTraces[aSide].swap(sTrace);
        }
        Compare("Construct", aTraces);
    }
}

// ---- the model's buffers: random games, clients and bytes

static unsigned sRand;
static unsigned Rand(unsigned n)
{
    sRand = sRand * 1103515245u + 12345u;
    return (sRand >> 16) % n;
}
struct World {
    LobbyGame* mGame;
    LobbyClient* mClient; // the joiner
    LobbyClientList* mList;
    LobbyConfig* mConfig;
    std::string mBytes;   // the read buffer's
};
static std::string Shorts(std::initializer_list<int> l)
{
    std::string s;
    for (int v : l) {
        s.push_back((char)(v & 0xff));
        s.push_back((char)((v >> 8) & 0xff));
    }
    return s;
}
static void Make(World& w, unsigned seed, int inCall)
{
    sObjects.clear();
    sRand = seed;
    static const wchar_t* kNames[4] = {L"ann", L"bob", L"ann", L"Cy"};
    w.mGame = new LobbyGame;
    w.mGame->AddRef();
    LobbyGame& g = *w.mGame;
    g.mType = 1 + (int)Rand(3);
    for (int i = 0; i < 6; i++) g.mAddress.mSix[i] = (char)Rand(256);
    g.mInProgress = Rand(4) == 0;
    g.mInviteOnly = Rand(2) != 0;
    g.mAskToJoin = Rand(2) != 0;
    g.mFull = Rand(4) == 0;
    g.mPassword = Rand(2) ? GUIString("pw") : GUIString();
    g.mName = GUIString(std::wstring(kNames[Rand(4)]) + L"'s game");
    g.mSkill = (int)Rand(5);
    g.mPlayers = (int)Rand(9);
    g.mMaxPlayers = (int)Rand(9);
    g.mPing = (unsigned short)Rand(1000);
    g.mHookRead = (int)Rand(3);
    g.mJoinHook = Rand(3) == 0 ? (short)(1 + Rand(9)) : 0;
    g.mJoinHookReads = Rand(2) != 0;
    g.mReplyHookReads = Rand(2) != 0;
    g.mStatusHook = Rand(2) ? GUIString("the game's text") : GUIString();
    w.mList = new LobbyClientList;
    w.mList->AddRef();
    g.mClients = w.mList;
    int n = (int)Rand(4);
    for (int i = 0; i < n; i++) {
        LobbyClient* c = new LobbyClient;
        c->AddRef();
        c->mName = GUIString(std::wstring(kNames[Rand(4)]));
        c->mClientId = (unsigned short)(10 + i);
        if (Rand(3) != 0) c->mPlayer = LobbyPlayer::CreatePlayer();
        w.mList->mClients.push_back(c);
    }
    w.mClient = new LobbyClient;
    w.mClient->AddRef();
    w.mClient->mName = GUIString(std::wstring(kNames[Rand(4)]));
    if (Rand(2) && n < 4) w.mList->mClients.push_back(w.mClient);
    int c = (int)Rand(3);
    w.mConfig = new LobbyConfig;
    w.mConfig->mAllowDuplicateNames = c == 1;
    sConfig = c == 0 ? NULL : w.mConfig;
    // the read buffer: a summary (written by the original), a join request, a join reply, or random bytes, cut anywhere
    switch (inCall) {
    case 0: case 1: {
        sSide = 0;
        WriteBuffer aWrite(0, true);
        L33Orig::WriteSummary(w.mGame, aWrite);
        w.mBytes = aWrite.mBytes;
        if (Rand(3) == 0) w.mBytes[0] = (char)(w.mBytes[0] ^ 1); // another product or type
        break;
    }
    case 2:
        w.mBytes = Shorts({(int)Rand(1000)}) + std::string(1, (char)Rand(256));
        break;
    case 3: {
        int status = Rand(3) == 0 ? (int)Rand(9) : 0;
        int m = (int)Rand(4);
        w.mBytes = Shorts({status, m});
        for (int i = 0; i < m; i++) w.mBytes += Shorts({10 + (int)Rand(5), (int)Rand(1000)}) + std::string(1, (char)Rand(2)) + std::string(1, 'd');
        break;
    }
    default:
        w.mBytes = Shorts({(int)Rand(1000)}) + std::string(1, (char)Rand(2)) + std::string(1, 'd');
        break;
    }
    if (Rand(3) == 0) w.mBytes.resize(Rand((unsigned)w.mBytes.size() + 1));
    for (LobbyClient* cl : w.mList->mClients)
        if (cl->mPlayer.get() != NULL) cl->mPlayer->mReadData = (int)Rand(2);
    sTrace.clear();
}
static const char* kCalls[] = {"WriteSummary", "ReadSummary", "HandleJoinRequest", "HandleJoinReply", "PlayerReadFrom",
                               "ReadDetails", "JoinRequest", "JoinReply", "StatusString", "PlayerWriteTo"};
static void BufCase(int inCall, unsigned seed)
{
    std::vector<std::string> aTraces[2];
    int aBufferKind = inCall <= 1 ? 0 : inCall == 2 || inCall == 7 ? 2 : inCall == 3 ? 3 : 4;
    for (int aSide = 0; aSide < 2; aSide++) {
        World w;
        Make(w, seed, aBufferKind);
        sSide = aSide;
        LobbyGame* g = w.mGame;
        ReadBuffer aRead(w.mBytes.data(), (unsigned long)w.mBytes.size());
        WriteBuffer aWrite(0, true);
        LobbyPlayer* aPlayer = LobbyPlayer::CreatePlayer();
        aPlayer->AddRef();
        aPlayer->mPing = 321;
        aPlayer->mReady = true;
        sTrace.clear();
        intptr_t r = 0;
        std::string aText;
        switch (inCall) {
        case 0:
            if (aSide == 0) L33Orig::WriteSummary(g, aWrite);
            else NewBuf(g, EeWonModelBufCall::WriteSummary, (intptr_t)&aWrite);
            break;
        case 1:
            r = aSide == 0 ? L33Orig::ReadSummary(g, aRead) : NewBuf(g, EeWonModelBufCall::ReadSummary, (intptr_t)&aRead);
            break;
        case 2:
            r = g->HandleJoinRequest(aRead, w.mClient, aWrite);
            break;
        case 3:
            r = aSide == 0 ? L33Orig::HandleJoinReply(g, aRead) : NewBuf(g, EeWonModelBufCall::HandleJoinReply, (intptr_t)&aRead);
            break;
        case 4:
            r = aPlayer->ReadFromBuffer(aRead);
            break;
        case 5:
            r = aSide == 0 ? L33Orig::ReadDetails(g, aRead) : NewBuf(g, EeWonModelBufCall::ReadDetails, (intptr_t)&aRead);
            break;
        case 6: case 7: {
            ConstSmartPtr<ByteBuffer> aBuffer;
            if (aSide == 0) aBuffer = inCall == 6 ? L33Orig::GetJoinRequest(g) : L33Orig::GetJoinReply(g, aRead, w.mClient);
            else {
                intptr_t aHeld = inCall == 6 ? NewBuf(g, EeWonModelBufCall::JoinRequest)
                                             : NewBuf(g, EeWonModelBufCall::JoinReply, (intptr_t)&aRead, (intptr_t)w.mClient);
                aBuffer = reinterpret_cast<ByteBuffer*>(aHeld);
                reinterpret_cast<ByteBuffer*>(aHeld)->Release();
            }
            Log("buffer " + Id(aBuffer.get()) + " " + Hex(aBuffer->data(), aBuffer->length()));
            break;
        }
        case 8: {
            short aStatus = (short)((int)Rand(14) - 3);
            GUIString s;
            if (aSide == 0) s = L33Orig::GetStatusString(g, aStatus);
            else {
                intptr_t aHeld = NewBuf(g, EeWonModelBufCall::StatusString, aStatus);
                s = GUIString(reinterpret_cast<GUIStringData*>(aHeld));
                reinterpret_cast<GUIStringData*>(aHeld)->Release();
            }
            aText = Text(s) + " refs " + N(s.mData->mRefs);
            break;
        }
        case 9:
            aPlayer->WriteToBuffer(aWrite);
            break;
        }
        Log("result " + N((long long)r) + " " + aText + " read at " + N(aRead.mPos) + " wrote " + Hex(aWrite.mBytes.data(), aWrite.mBytes.size()));
        for (LobbyClient* c : w.mList->mClients) Log("client " + Id(c) + " player " + Id(c->mPlayer.get()));
        Log("joiner player " + Id(w.mClient->mPlayer.get()) + " player " + N(aPlayer->mPing) + " " + N(aPlayer->mReady));
        Refs();
        aTraces[aSide].swap(sTrace);
    }
    Compare(kCalls[inCall], aTraces);
}

// ---- ChatCtrl's links: every kind with each of its arguments there or not

static void LinkCases()
{
    for (int aKind = -1; aKind <= 6; aKind++)
        for (int k = 0; k < 8; k++) {
            std::vector<std::string> aTraces[2];
            for (int aSide = 0; aSide < 2; aSide++) {
                sObjects.clear();
                sTrace.clear();
                LobbyClient* aClient = new LobbyClient;
                aClient->AddRef();
                aClient->mName = GUIString("cy");
                LobbyFriend* aFriend = new LobbyFriend;
                aFriend->AddRef();
                aFriend->mName = GUIString("fr");
                LobbyServer* aServer = new LobbyServer;
                aServer->AddRef();
                Font* aFont = new Font;
                aFont->AddRef();
                EeWonIpAddr* anIp = ee_won_ipaddr_new();
                ee_won_ipaddr_set_host_order(anIp, 0xc0a80105, 2300);
                GUIString aText("[Accept]");
                EeWonLobbyCtlLink l = {};
                l.kind = aKind;
                l.obj = aKind == 4 || aKind == 5 ? (intptr_t)aFriend : (intptr_t)aClient;
                l.text = (k & 1) ? (intptr_t)aText.mData.get() : 0;
                l.id = 40 + k;
                l.addr = aKind == 5 ? (intptr_t)anIp : (k & 2) ? (intptr_t)aServer : 0;
                l.font = (k & 4) ? (intptr_t)aFont : 0;
                l.color = 0x123456 + k;
                ChatLink* aLink = new ChatLink;
                aLink->mKind = 55;
                sTrace.clear();
                if (aSide == 0) L33Orig::ChatLinkInit(aLink, l);
                else {
                    aLink->mKind = l.kind; // the C++ constructor's initialiser
                    ee_won_chatlink_init(aLink, &l, MutLink);
                }
                aLink->Dump();
                Refs();
                ee_won_ipaddr_free(anIp);
                aTraces[aSide].swap(sTrace);
            }
            Compare("ChatLink", aTraces);
        }
}

int main()
{
    const char* m = getenv("EE_L33_MUTATE");
    sMutate = m != NULL ? atoi(m) : 0;
    PrvCases();
    LinkCases();
    int aPrvChecks = sChecks;
    for (int aCall = 0; aCall < 10; aCall++)
        for (unsigned seed = 1; seed <= 3000; seed++) BufCase(aCall, seed * 7919u + (unsigned)aCall);
    printf("won_lobby33_test: WONLobbyPrv and ChatLink %d cases, the model's buffers %d cases, %d mismatches\n", aPrvChecks,
           sChecks - aPrvChecks, sFailures);
    for (const auto& f : sFailed) printf("  %s: %d\n", f.first.c_str(), f.second);
    // every op of the two hosts reached (their enums' last values: AllocPlayerOptionsCtrl, PlayerWriteData)
    int aMissed = 0;
    for (int i = 1; i <= (int)EeWonLobbyPrvOp::AllocPlayerOptionsCtrl; i++)
        if (sPrvHits.count(i) == 0) {
            printf("  prv op %d not reached\n", i);
            aMissed++;
        }
    for (int i = 1; i <= (int)EeWonModelBufOp::PlayerWriteData; i++)
        if (sBufHits.count(i) == 0) {
            printf("  buf op %d not reached\n", i);
            aMissed++;
        }
    for (int i = 1; i <= (int)EeWonChatLinkOp::ClearPrevious; i++)
        if (sLinkHits.count(i) == 0) {
            printf("  link op %d not reached\n", i);
            aMissed++;
        }
    printf("ops reached: %d of %d\n", (int)(sPrvHits.size() + sBufHits.size() + sLinkHits.size()),
           (int)EeWonLobbyPrvOp::AllocPlayerOptionsCtrl + (int)EeWonModelBufOp::PlayerWriteData +
               (int)EeWonChatLinkOp::ClearPrevious);
    if (sMutate == 0) sFailures += aMissed;
    if (sMutate != 0) {
        printf("mutation %d: %s\n", sMutate, sFailures > 0 ? "caught" : "MISSED");
        return sFailures > 0 ? 0 : 1;
    }
    return sFailures;
}
