// LanLogic as a whole object (R6 WON round 20): the original LanLogic.cpp's methods (the statements copied from the
// C++ before the port, every call on a C++ object replaced by the host op that makes it: LobbyLogicHost.cpp runs the
// same calls in the same order; the texts as the UTF-16 LE bytes the original's LanUtf16 made) against ee_won's Rust
// LanLogic (lanobj.rs) over one scripted host. Each of 20,000 sequences creates both objects and makes eight random
// inward calls (the buttons, the events, hosting and joining with every join reply, the staging sends, the
// completions: received game messages of every type, closed sockets, the timers, discovery packets, accepts); the
// host answers every op from a seeded generator, the same stream for both, and its "take" ops hand over generated
// messages, replies, packets and names. The traces (every op with its arguments and answer, the messages' bytes)
// must be the same. Where the original's HandleMessage re-entered itself for a local delivery both do so here too.
// The first 100 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_lanobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <set>
#include <string>
#include <vector>
#include "rust/ee_won.h"

typedef EeWonLobbyOp Op;
typedef EeWonLobbyText LT;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- the scripted host

static unsigned sAnswerSeed;
static unsigned sGameTypeBase;
static bool sRustRun;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sAnswerSeed) % n; }
static std::vector<std::string> sTrace;
static void* P(uintptr_t inValue) { return reinterpret_cast<void*>(inValue); }

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::string Hex(const void* inBytes, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inBytes)[i]);
        anOut += aBuffer;
    }
    return anOut;
}
static const uint8_t* U8(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

// a UTF-16 LE name or text: empty, ASCII or with a character past Latin-1
static std::string Text()
{
    std::string s;
    unsigned n = R(5);
    for (unsigned i = 0; i < n; i++) {
        uint16_t u = R(6) == 0 ? (uint16_t)0x263a : (uint16_t)('a' + R(26));
        s += (char)(u & 0xff);
        s += (char)(u >> 8);
    }
    return s;
}
static uint16_t Id() { return (uint16_t)(1 + R(3)); }

// a game-channel message as a socket receives it (sometimes cut short, or with the host's 2-byte prefix)
static std::string GameMessage()
{
    std::string m;
    switch (R(9)) {
    case 0: { std::string a = Text(), b = Text(); ee_won_lan_join_request(U8(a), a.size(), U8(b), b.size(), Append, &m); break; }
    case 1: { std::string a = Text(); ee_won_lan_client_joined(Id(), U8(a), a.size(), Append, &m); break; }
    case 2: ee_won_lan_client_left(Id(), Append, &m); break;
    case 3: { std::string a = Text(); ee_won_lan_chat_request((uint16_t)(R(2) ? 4 : R(2) ? 0x100 : 1), U8(a), a.size(), Id(), Append, &m); break; }
    case 4: { std::string a = Text(); ee_won_lan_chat_line((uint16_t)(R(2) ? 4 : R(2) ? 0x100 : 1), Id(), U8(a), a.size(), Id(), Append, &m); break; }
    case 5: { std::string a = "\x05\x01"; ee_won_lan_staging(U8(a), R(3), Append, &m); break; }
    case 6: ee_won_lan_ping(Append, &m); break;
    case 7: m = std::string(1, (char)R(12)); break;
    default: break;
    }
    // a socket's receive strips the 2-byte length the builders write; sometimes it is left (the host's own form)
    if (m.size() >= 2 && R(4) != 0) m = m.substr(2);
    if (R(6) == 0 && !m.empty()) m.resize(R((unsigned)m.size()));
    return m;
}
static std::string JoinReply()
{
    std::string m;
    switch (R(4)) {
    case 0: case 1: {
        unsigned n = R(4);
        std::vector<uint16_t> anIds;
        std::vector<size_t> anOffsets(1, 0);
        std::string aNames;
        for (unsigned i = 0; i < n; i++) { anIds.push_back(Id()); aNames += Text(); anOffsets.push_back(aNames.size()); }
        ee_won_lan_join_accepted(Id(), Id(), anIds.size(), anIds.empty() ? NULL : &anIds[0], U8(aNames), &anOffsets[0],
                                 Append, &m);
        break;
    }
    case 2: ee_won_lan_join_refused((int16_t)(2 + R(4)), Append, &m); break;
    default: m = std::string(1, (char)R(4)); break;
    }
    if (m.size() >= 2 && R(8) != 0) m = m.substr(2);
    if (R(6) == 0 && !m.empty()) m.resize(R((unsigned)m.size()));
    return m;
}
static std::string DiscoveryPacket()
{
    std::string m;
    ee_won_lan_discovery((uint8_t)(1 + R(5)), (uint16_t)(R(3) * 1000), Append, &m);
    if (R(3) == 0) m += "summary";
    if (R(6) == 0) m.resize(R((unsigned)m.size() + 1));
    return m;
}

static intptr_t Obj(uintptr_t inBase) { unsigned r = R(8); return r < 2 ? 0 : (intptr_t)(inBase + (r & 1) + 1); }
static intptr_t Bool(unsigned inTrueIn8) { return R(8) < inTrueIn8 ? 1 : 0; }

static bool IsTake(Op inOp)
{
    return inOp == Op::LanRecvMsgTake || inOp == Op::LanDiscoveryTake || inOp == Op::LanRecvReplyTake ||
           inOp == Op::ClientNameTake || inOp == Op::ParserTextTake || inOp == Op::LanNameTake ||
           inOp == Op::LanJoinPasswordTake;
}
static bool IsMessage(Op inOp)
{
    return inOp == Op::LanSocketSend || inOp == Op::LanClientSocketSend || inOp == Op::LanServerSend ||
           inOp == Op::LanStagingRead || inOp == Op::LanDiscoverySend || inOp == Op::LanSendSummary ||
           inOp == Op::MessageBox;
}
static bool IsText(Op inOp)
{
    return inOp == Op::LanNewClient || inOp == Op::LanGamePasswordIs || inOp == Op::LanRecvChat;
}

static intptr_t Answer(Op inOp, void* inA, std::string& outTaken)
{
    switch (inOp) {
    case Op::LanMyClient: case Op::LanFindClient: case Op::LanClientAt: case Op::LanSocketClient:
    case Op::EventClientActionClient: case Op::ParserTarget: case Op::LanRemoveClient: return Obj(0x2000);
    case Op::LanNewClient: return 0x2005;
    case Op::ClientHold: return (intptr_t)inA;
    case Op::LanListenSocket: case Op::LanServerSocket: case Op::LanDiscoverySocket: case Op::LanOpSocket:
    case Op::LanPendingAt: case Op::LanClientSocket: case Op::LanAcceptedSocket: return Obj(0x5000);
    case Op::LanNetworkTimer: case Op::LanDiscoveryTimer: return Obj(0x6000);
    case Op::LanDiscoveryAddress: return 0x8001;
    case Op::LanRecvFrom: return 0x8002;
    case Op::LanMyGame: case Op::EventDirectGame: return Obj(0x1000);
    case Op::ChatParser: return Obj(0x7000);
    case Op::EventComponent: return 0x4100;
    case Op::LanPendingCount: case Op::LanClientCount: return R(4);
    case Op::LanSocketLastTime: return 1000 - (intptr_t)R(600);
    case Op::Now: return 1000 + R(40);
    case Op::LanSubnets: return R(2) ? 0x01020102 : 0x01020304;
    case Op::ClientId: return Id();
    case Op::GameType: return (intptr_t)((((uintptr_t)inA & 3) + sGameTypeBase) % 5);
    case Op::GameScreenType: return R(5);
    case Op::LanOpenDiscovery: return R(4) == 0 ? 1 : 0;
    case Op::LanNameDialogRun: case Op::LanPortDialogRun: case Op::LanPasswordDialogRun:
        return R(8) < 6 ? 0x186a1 : 0x186a2;
    case Op::LanNameDialogLength: return R(3);
    case Op::LanBroadcastPort: case Op::LanPortDialogPort: return 2300 + R(3);
    case Op::GameAddressPort: return R(3) * 1000;
    case Op::ParserRoomFlags: return R(32);
    case Op::ParserCommand: return R(12);
    case Op::EventClientActionRoomSpec: return R(2) ? 8 : R(16);
    case Op::EventClientActionAction: return R(20);
    case Op::LanRecvMsgTake: { intptr_t r = Obj(0x3200); if (r != 0) outTaken = GameMessage(); return r; }
    case Op::LanDiscoveryTake: outTaken = DiscoveryPacket(); return 0;
    case Op::LanRecvReplyTake: { unsigned r = R(10); if (r == 0) return -1; if (r == 1) return 0; outTaken = JoinReply(); return 1; }
    case Op::ClientNameTake: case Op::ParserTextTake: case Op::LanNameTake: case Op::LanJoinPasswordTake:
        outTaken = Text(); return 0;
    case Op::ClientRelease: case Op::LanSetMyClient: case Op::LanAuthAutoRefreshOff: case Op::LanCloseSockets:
    case Op::LanSocketSetClient: case Op::LanForgetSocket: case Op::LanLoseHost: case Op::LanSocketClose:
    case Op::LanStartNetworkTimer: case Op::LanSocketSend: case Op::LanAddClient: case Op::LanClientChanged:
    case Op::ClientLeftGame: case Op::GameSetCaptainId: case Op::LanServerClientIsCaptain: case Op::LanSocketStamp:
    case Op::LanAddPending: case Op::LanClientSocketSend: case Op::LanServerSend: case Op::LanWhisperReceived:
    case Op::LanRecvChat: case Op::LanStagingRead: case Op::LanNameDialogSave: case Op::LanSetBroadcastPort:
    case Op::LanDirectRemove: case Op::LanDirectAdd: case Op::LanShowClientOptions: case Op::SetClientIgnored:
    case Op::LanSetChatInput: case Op::KickClientFromLobby: case Op::LanChatCommandError: case Op::LanClearLanGames:
    case Op::LanPingDirectGames: case Op::LanEnterScreenLists: case Op::ShowLanScreen: case Op::KillFirewallTest:
    case Op::LanHostMyClient: case Op::LanSendSummary: case Op::LanDiscoverySend: case Op::LanRemoveStaleGames:
    case Op::LanStartDiscoveryTimer: case Op::LanGameSummary: case Op::LanGameKeepAlive: case Op::LanGameRemoved:
    case Op::LanDiscoveryArm: case Op::LanStopDiscovery: case Op::LanClearDirectGames: case Op::LanStartPeerAuth:
    case Op::LanNewServerSocket: case Op::LanJoinPasswordClear: case Op::LanJoinArm: case Op::MessageBox:
        return 0;
    default: return Bool(6);
    }
}

static intptr_t ScriptedHost(void* inLogic, Op inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    std::string aTaken;
    intptr_t aResult = Answer(inOp, inA, aTaken);
    char aLine[160];
    std::string aText;
    if (IsTake(inOp)) {
        if (sRustRun) ee_won_lobby_sink(inB, U8(aTaken), aTaken.size());
        else static_cast<std::string*>(inB)->append(aTaken);
        snprintf(aLine, sizeof(aLine), "%u %llx s %lld %lld -> %lld t:", (unsigned)inOp,
                 (unsigned long long)(uintptr_t)inA, (long long)inC, (long long)inD, (long long)aResult);
        aText = std::string(aLine) + Hex(aTaken.data(), aTaken.size());
    } else if (IsMessage(inOp) && inOp != Op::MessageBox) {
        snprintf(aLine, sizeof(aLine), "%u %llx %llx m:", (unsigned)inOp, (unsigned long long)(uintptr_t)inA,
                 (unsigned long long)(uintptr_t)inB);
        aText = std::string(aLine) + (inC != 0 ? Hex(P(inC), (size_t)inD) : std::string("-")) + " -> " +
                std::to_string((long long)aResult);
    } else if (IsText(inOp)) {
        size_t aCount = (size_t)(inD & 0xffffffff);
        snprintf(aLine, sizeof(aLine), "%u %llx %llx u:", (unsigned)inOp, (unsigned long long)(uintptr_t)inA,
                 (unsigned long long)(uintptr_t)inB);
        aText = std::string(aLine) + Hex(P(inC), aCount * 2) + " " + std::to_string((long long)(inD >> 32)) + " -> " +
                std::to_string((long long)aResult);
    } else {
        snprintf(aLine, sizeof(aLine), "%u %llx %llx %lld %lld -> %lld", (unsigned)inOp,
                 (unsigned long long)(uintptr_t)inA, (unsigned long long)(uintptr_t)inB, (long long)inC,
                 (long long)inD, (long long)aResult);
        aText = aLine;
    }
    sTrace.push_back(aText);
    (void)inLogic;
    return aResult;
}

static intptr_t H(Op inOp, void* inA = NULL, void* inB = NULL, intptr_t inC = 0, intptr_t inD = 0)
{
    return ScriptedHost(P(0x52), inOp, inA, inB, inC, inD);
}
static void* HP(Op inOp, void* inA = NULL, void* inB = NULL, intptr_t inC = 0, intptr_t inD = 0)
{
    return P((uintptr_t)H(inOp, inA, inB, inC, inD));
}
// a take op: its result, the bytes into outBytes
static intptr_t HT(Op inOp, void* inA, std::string& outBytes) { return H(inOp, inA, &outBytes); }

// ---- the original LanLogic.cpp (its statements; the objects' calls are the host's ops)

// a message: the C++ ByteBuffer (NULL for one built here), its bytes, and whether there is one
struct Msg {
    void* mBuffer;
    std::string mBytes;
    bool mPresent;
};
static Msg Built(const std::string& inBytes) { Msg m = {NULL, inBytes, true}; return m; }
static Msg None() { Msg m = {NULL, std::string(), false}; return m; }
static intptr_t HM(Op inOp, const Msg& inMsg, void* inB)
{
    return H(inOp, inMsg.mBuffer, inB, inMsg.mPresent ? (intptr_t)inMsg.mBytes.data() : 0, (intptr_t)inMsg.mBytes.size());
}
static intptr_t HU(Op inOp, void* inA, const std::string& inUtf16, void* inB = NULL, intptr_t inHigh = 0)
{
    return H(inOp, inA, inB, (intptr_t)inUtf16.data(), (intptr_t)(inUtf16.size() / 2) | (inHigh << 32));
}

struct HeldClient {
    void* mClient;
    explicit HeldClient(void* inClient) : mClient(inClient) {}
    ~HeldClient() { if (mClient != NULL) H(Op::ClientRelease, mClient); }
};

struct Texts {
    std::string mFirst, mSecond; // UTF-16 LE
    static void First(void* inContext, const uint16_t* inUnits, size_t inCount)
    { static_cast<Texts*>(inContext)->mFirst.append(reinterpret_cast<const char*>(inUnits), inCount * 2); }
    static void Second(void* inContext, const uint16_t* inUnits, size_t inCount)
    { static_cast<Texts*>(inContext)->mSecond.append(reinterpret_cast<const char*>(inUnits), inCount * 2); }
};
static EeWonLanMsg LanRead(const Msg& inBuffer, size_t inSkip, Texts& outTexts)
{
    EeWonLanMsg aMsg;
    ee_won_lan_read_game(U8(inBuffer.mBytes), inBuffer.mBytes.size(), inSkip, &aMsg, Texts::First, Texts::Second,
                         &outTexts);
    return aMsg;
}
static std::string Discovery(uint8_t inKind, uint16_t inPort)
{
    std::string m;
    ee_won_lan_discovery(inKind, inPort, Append, &m);
    return m;
}

struct Orig {
    unsigned short mNextClientId;
    bool mIsCaptain, mRequirePeerAuth;
    long long mLastServerPing, mLastBroadcastTime;
    Orig() : mNextClientId(0), mIsCaptain(false), mRequirePeerAuth(false), mLastServerPing(0), mLastBroadcastTime(0) {}

    unsigned short GetNextClientId() { return ee_won_lan_next_client_id(&this->mNextClientId); }
    bool IsConnected() { return H(Op::LanListenSocket) != 0 || H(Op::LanServerSocket) != 0; }
    bool IsCaptain() { return this->mIsCaptain; }
    void* MyClient() { return HP(Op::LanMyClient); }

    void CloseConnections()
    {
        H(Op::LanAuthAutoRefreshOff);
        this->mNextClientId = 0;
        this->mIsCaptain = false;
        this->mRequirePeerAuth = false;
        H(Op::LanCloseSockets);
    }
    void HandleSocketClosed(void* inOp)
    {
        void* aSocket = HP(Op::LanOpSocket, inOp);
        bool aHas = aSocket != NULL && HP(Op::LanSocketClient, aSocket) != NULL;
        bool aServer = aSocket != NULL && aSocket == HP(Op::LanServerSocket);
        EeWonLanClosed aClosed = ee_won_lanlogic_socket_closed(aSocket != NULL, this->IsCaptain(), aHas, aServer);
        if (aClosed.announce_left) {
            std::string aMessage;
            ee_won_lan_client_left((uint16_t)H(Op::ClientId, HP(Op::LanSocketClient, aSocket)), Append, &aMessage);
            H(Op::LanSocketSetClient, aSocket, NULL);
            this->BroadcastMessage(Built(aMessage), true);
        }
        if (aClosed.forget_socket)
            H(Op::LanForgetSocket, aSocket);
        if (aClosed.lost_host)
            H(Op::LanLoseHost);
    }
    void HandleNetworkTimer(void* inOp)
    {
        if (inOp != HP(Op::LanNetworkTimer))
            return;
        if (this->IsCaptain()) {
            long long aTime = H(Op::Now);
            for (intptr_t i = 0; i < H(Op::LanPendingCount); ++i) {
                void* aSocket = HP(Op::LanPendingAt, NULL, NULL, i);
                if (ee_won_lan_host_socket_stale(aTime, H(Op::LanSocketLastTime, aSocket)))
                    H(Op::LanSocketClose, aSocket, NULL, 1000);
            }
        } else if (H(Op::LanServerSocket) != 0 && ee_won_lan_joiner_ping_due(H(Op::Now), this->mLastServerPing)) {
            std::string aMessage;
            ee_won_lan_ping(Append, &aMessage);
            this->SendMessageToCaptain(Built(aMessage));
        }
        H(Op::LanStartNetworkTimer);
    }
    void HandleJoinRequest(const std::string& inName, const std::string& inPassword, void* inSocket)
    {
        if (MyClient() == NULL)
            return;
        void* aGame = HP(Op::LanMyGame);
        if (aGame == NULL)
            return;
        unsigned char aPeerNet[2] = {0, 0}, aLocalNet[2] = {0, 0};
        if (H(Op::GameType, aGame) == 1) {
            uint32_t aNets = (uint32_t)H(Op::LanSubnets, inSocket);
            aPeerNet[0] = (unsigned char)aNets;
            aPeerNet[1] = (unsigned char)(aNets >> 8);
            aLocalNet[0] = (unsigned char)(aNets >> 16);
            aLocalNet[1] = (unsigned char)(aNets >> 24);
        }
        int32_t aType = (int32_t)H(Op::GameType, aGame);
        bool aMatches = HU(Op::LanGamePasswordIs, aGame, inPassword) != 0;
        short aStatus = ee_won_lan_join_status(aType, aPeerNet[0], aPeerNet[1], aLocalNet[0], aLocalNet[1], aMatches);
        if (aStatus != 0) {
            std::string aRefusal;
            ee_won_lan_join_refused(aStatus, Append, &aRefusal);
            HM(Op::LanSocketSend, Built(aRefusal), inSocket);
            return;
        }
        HeldClient aClient(HP(Op::LanNewClient, NULL, NULL, (intptr_t)inName.data(), (intptr_t)inName.size() / 2));
        H(Op::LanAddClient, aClient.mClient, NULL, this->GetNextClientId());
        std::string aName;
        HT(Op::ClientNameTake, aClient.mClient, aName);
        std::string aNotify;
        ee_won_lan_client_joined((uint16_t)H(Op::ClientId, aClient.mClient), U8(aName), aName.length(), Append, &aNotify);
        this->BroadcastMessage(Built(aNotify), true);
        H(Op::LanSocketSetClient, inSocket, aClient.mClient);
        std::vector<uint16_t> anIds;
        std::vector<size_t> anOffsets(1, 0);
        std::string aNames;
        for (intptr_t anItr = 0; anItr < H(Op::LanClientCount); ++anItr) {
            void* aListedClient = HP(Op::LanClientAt, NULL, NULL, anItr);
            anIds.push_back((uint16_t)H(Op::ClientId, aListedClient));
            HT(Op::ClientNameTake, aListedClient, aNames);
            anOffsets.push_back(aNames.length());
        }
        std::string aReply;
        uint16_t aCaptain = (uint16_t)H(Op::ClientId, MyClient());
        ee_won_lan_join_accepted(aCaptain, (uint16_t)H(Op::ClientId, aClient.mClient), anIds.size(),
                                 anIds.empty() ? NULL : &anIds[0], U8(aNames), &anOffsets[0], Append, &aReply);
        this->SendMessageToClient(aClient.mClient, Built(aReply));
    }
    struct Players {
        std::vector<std::pair<unsigned short, std::string> > mList;
        static void Add(void* inContext, uint16_t inId, const uint16_t* inName, size_t inCount)
        {
            Players* aPlayers = static_cast<Players*>(inContext);
            aPlayers->mList.push_back(std::make_pair((unsigned short)inId,
                                                     std::string(reinterpret_cast<const char*>(inName), inCount * 2)));
        }
    };
    int ReadJoinReply(const Msg& inBuffer, void* inGame)
    {
        if (!inBuffer.mPresent || H(Op::LanServerSocket) == 0 || inGame == NULL)
            return 1;
        Players aPlayers;
        bool aHaveCaptain = false;
        uint16_t aCaptainId = 0, aMyId = 0;
        int aStatus = ee_won_lan_read_join_reply(U8(inBuffer.mBytes), inBuffer.mBytes.size(), &aHaveCaptain,
                                                 &aCaptainId, &aMyId, Players::Add, &aPlayers);
        if (aHaveCaptain)
            H(Op::GameSetCaptainId, inGame, NULL, aCaptainId);
        for (size_t i = 0; i < aPlayers.mList.size(); ++i) {
            unsigned short anId = aPlayers.mList[i].first;
            HeldClient aClient(HP(Op::LanNewClient, NULL, NULL, (intptr_t)aPlayers.mList[i].second.data(),
                                  (intptr_t)aPlayers.mList[i].second.size() / 2));
            H(Op::LanAddClient, aClient.mClient, NULL, anId);
            switch (ee_won_lanlogic_reply_role(anId, aMyId, aCaptainId)) {
            case 1: H(Op::LanSetMyClient, aClient.mClient); break;
            case 2: H(Op::LanServerClientIsCaptain, aClient.mClient); break;
            }
        }
        return aStatus;
    }
    void HandleNewSocketMessage(const Msg& inBuffer, void* inSocket)
    {
        if (!inBuffer.mPresent || inSocket == NULL || !this->IsCaptain())
            return;
        Texts aTexts;
        EeWonLanMsg aMsg = LanRead(inBuffer, 0, aTexts);
        if (aMsg.kind == 1)
            this->HandleJoinRequest(aTexts.mFirst, aTexts.mSecond, inSocket);
    }
    void HandleRecvMsg(void* inOp)
    {
        void* aSocket = HP(Op::LanOpSocket, inOp);
        bool anOk = H(Op::LanRecvOk, inOp) != 0;
        bool aHas = anOk && HP(Op::LanSocketClient, aSocket) != NULL;
        EeWonLanRecv aRecv = ee_won_lanlogic_recv(anOk, this->IsCaptain(), aHas);
        if (!aRecv.handle)
            return;
        if (aRecv.stamp)
            H(Op::LanSocketStamp, aSocket);
        std::string aBytes;
        intptr_t aBuffer = HT(Op::LanRecvMsgTake, inOp, aBytes);
        Msg aMsg = {P((uintptr_t)aBuffer), aBytes, aBuffer != 0};
        if (aRecv.new_socket) {
            this->HandleNewSocketMessage(aMsg, aSocket);
            return;
        }
        this->HandleMessage(aMsg, HP(Op::LanSocketClient, aSocket));
    }
    void SendMessageToClient(void* inClient, const Msg& inBuffer)
    {
        bool aCaptain = this->IsCaptain() && this->IsConnected();
        bool aToMe = aCaptain && inClient == MyClient();
        bool aHasSocket = aCaptain && !aToMe && HP(Op::LanClientSocket, inClient) != NULL;
        switch (ee_won_lanlogic_send_to_client(this->IsCaptain(), this->IsConnected(), aToMe, aHasSocket)) {
        case EeWonLanRoute::Local: this->HandleMessage(inBuffer, MyClient()); break;
        case EeWonLanRoute::Socket: HM(Op::LanClientSocketSend, inBuffer, inClient); break;
        default: break;
        }
    }
    void SendMessageToCaptain(const Msg& inBuffer)
    {
        bool aConnected = this->IsConnected();
        bool aServer = H(Op::LanServerSocket) != 0;
        switch (ee_won_lanlogic_send_to_captain(aConnected, this->IsCaptain(), aServer)) {
        case EeWonLanRoute::Local: this->HandleMessage(inBuffer, MyClient()); break;
        case EeWonLanRoute::Socket:
            this->mLastServerPing = H(Op::Now);
            HM(Op::LanServerSend, inBuffer, NULL);
            break;
        default: break;
        }
    }
    void BroadcastMessage(const Msg& inBuffer, bool inDeliverLocal)
    {
        bool aCaptain = this->IsCaptain();
        if (ee_won_lanlogic_broadcast(aCaptain, inDeliverLocal, NULL, NULL, 0))
            this->HandleMessage(inBuffer, MyClient());
        if (!aCaptain)
            return;
        std::vector<void*> aSockets;
        for (intptr_t i = 0; i < H(Op::LanPendingCount); ++i)
            aSockets.push_back(HP(Op::LanPendingAt, NULL, NULL, i));
        if (aSockets.empty())
            return;
        std::vector<char> aHasClient(aSockets.size()), aSend(aSockets.size());
        for (size_t i = 0; i < aSockets.size(); ++i)
            aHasClient[i] = HP(Op::LanSocketClient, aSockets[i]) != NULL;
        ee_won_lanlogic_broadcast(aCaptain, false, reinterpret_cast<const bool*>(&aHasClient[0]),
                                  reinterpret_cast<bool*>(&aSend[0]), aSockets.size());
        for (size_t i = 0; i < aSockets.size(); ++i)
            if (aSend[i])
                HM(Op::LanSocketSend, inBuffer, aSockets[i]);
    }
    static std::string Staging(const Msg& inBuffer)
    {
        std::string aMessage;
        ee_won_lan_staging(inBuffer.mPresent ? U8(inBuffer.mBytes) : NULL, inBuffer.mPresent ? inBuffer.mBytes.size() : 0,
                           Append, &aMessage);
        return aMessage;
    }
    void SendToCaptain(const Msg& inBuffer) { this->SendMessageToCaptain(Built(Staging(inBuffer))); }
    void BroadcastToGame(const Msg& inBuffer) { this->BroadcastMessage(Built(Staging(inBuffer)), false); }
    void SendToClient(void* inClient, const Msg& inBuffer) { this->SendMessageToClient(inClient, Built(Staging(inBuffer))); }
    void HandleClientJoined(unsigned short inId, const std::string& inName)
    {
        void* aFound = NULL;
        if (this->IsCaptain()) {
            aFound = HP(Op::LanFindClient, NULL, NULL, inId);
            if (aFound != NULL) H(Op::ClientHold, aFound);
        } else {
            aFound = HP(Op::LanNewClient, NULL, NULL, (intptr_t)inName.data(), (intptr_t)inName.size() / 2);
            H(Op::LanAddClient, aFound, NULL, inId);
        }
        HeldClient aClient(aFound);
        if (aClient.mClient != NULL)
            H(Op::LanClientChanged, aClient.mClient, NULL, 0);
    }
    void HandleClientLeft(unsigned short inClientId)
    {
        HeldClient aClient(HP(Op::LanRemoveClient, NULL, NULL, inClientId));
        if (aClient.mClient != NULL) {
            H(Op::LanClientChanged, aClient.mClient, NULL, 1);
            H(Op::ClientLeftGame, aClient.mClient);
        }
    }
    void HandleChatRequest(unsigned short inFlags, const std::string& inText, unsigned short inTargetId, void* inSender)
    {
        if (!this->IsCaptain())
            return;
        std::string aLine;
        ee_won_lan_chat_line(inFlags, (uint16_t)H(Op::ClientId, inSender), U8(inText), inText.length(), inTargetId,
                             Append, &aLine);
        Msg aMessage = Built(aLine);
        switch (ee_won_lan_chat_route(inFlags)) {
        case 1: {
            void* aTarget = HP(Op::LanFindClient, NULL, NULL, inTargetId);
            if (aTarget == NULL)
                return;
            this->SendMessageToClient(aTarget, aMessage);
            if (inSender != aTarget)
                this->SendMessageToClient(inSender, aMessage);
            break;
        }
        case 2: {
            for (intptr_t i = 0; i < H(Op::LanClientCount); ++i) {
                void* aClient = HP(Op::LanClientAt, NULL, NULL, i);
                if (H(Op::ClientIsTeammate, aClient, inSender))
                    this->SendMessageToClient(aClient, aMessage);
            }
            break;
        }
        default:
            this->BroadcastMessage(aMessage, true);
        }
    }
    void HandleChatMessage(unsigned short inFlags, unsigned short inSenderId, const std::string& inText,
                           unsigned short inTargetId)
    {
        void* aSender = HP(Op::LanFindClient, NULL, NULL, inSenderId);
        if (aSender == NULL)
            return;
        if (H(Op::ClientIsIgnored, aSender))
            return;
        void* aTarget = NULL;
        unsigned short aFlags = ee_won_lan_chat_line_flags(inFlags);
        if (ee_won_lan_chat_route(inFlags) == 1) {
            aTarget = HP(Op::LanFindClient, NULL, NULL, inTargetId);
            if (aTarget == NULL)
                return;
            if (ee_won_lanlogic_whisper_received(true, aSender == MyClient()))
                H(Op::LanWhisperReceived, aSender);
        }
        HU(Op::LanRecvChat, aSender, inText, aTarget, aFlags);
    }
    void HandleMessage(const Msg& inBuffer, void* inClient)
    {
        if (!inBuffer.mPresent || inClient == NULL)
            return;
        size_t anOffset = ee_won_lanlogic_message_offset(this->IsCaptain(), inClient == MyClient());
        Texts aTexts;
        EeWonLanMsg aMsg = LanRead(inBuffer, anOffset, aTexts);
        switch (aMsg.kind) {
        case 3: this->HandleClientJoined(aMsg.id, aTexts.mFirst); break;
        case 4: this->HandleClientLeft(aMsg.id); break;
        case 5: this->HandleChatRequest(aMsg.flags, aTexts.mFirst, aMsg.target, inClient); break;
        case 6: this->HandleChatMessage(aMsg.flags, aMsg.id, aTexts.mFirst, aMsg.target); break;
        case 8: HM(Op::LanStagingRead, Built(inBuffer.mBytes.substr(anOffset)), inClient); break;
        }
    }
    void HandleControlEvent(int inWhich)
    {
        switch (inWhich) {
        case 1: this->ChangeLanName(); return;
        case 2: this->ChangeLanPort(); return;
        case 3: this->RefreshGames(); return;
        case 4: this->EnterLanScreen(); return;
        }
    }
    void HandleComponentEvent(int inType, void* inEvent)
    {
        switch (inType) {
        case 100005: this->HandleClientAction(inEvent); break;
        case 100008: this->HandleChangeDirectConnect(inEvent); break;
        case 0x186ac: this->HandleChatCommand(HP(Op::EventComponent, inEvent)); break;
        }
    }
    void ChangeLanName()
    {
        intptr_t aResult = H(Op::LanNameDialogRun);
        if (ee_won_lanlogic_name_accepted((int)aResult, aResult == 0x186a1 ? (size_t)H(Op::LanNameDialogLength) : 0))
            H(Op::LanNameDialogSave);
    }
    void ChangeLanPort()
    {
        intptr_t aPort = H(Op::LanBroadcastPort);
        intptr_t aResult = H(Op::LanPortDialogRun, NULL, NULL, aPort);
        intptr_t aNewPort = aResult == 0x186a1 ? H(Op::LanPortDialogPort) : aPort;
        if (!ee_won_lanlogic_port_accepted((int)aResult, (int)aPort, (int)aNewPort))
            return;
        H(Op::LanSetBroadcastPort, NULL, NULL, aNewPort);
        this->StartDiscovery();
    }
    void HandleChangeDirectConnect(void* inEvent)
    {
        if (H(Op::EventDirectIsDelete, inEvent)) {
            void* aGame = HP(Op::EventDirectGame, inEvent);
            if (aGame != NULL)
                H(Op::LanDirectRemove, aGame);
        } else if (H(Op::EventDirectIsAdd, inEvent)) {
            H(Op::LanDirectAdd);
        }
    }
    void ShowClientOptions(void* inClient, intptr_t inRoomSpec)
    {
        if (MyClient() == NULL || inClient == NULL)
            return;
        H(Op::LanShowClientOptions, inClient, NULL, inRoomSpec);
    }
    void HandleClientAction(void* inEvent)
    {
        void* aClient = HP(Op::EventClientActionClient, inEvent);
        bool aConnected = this->IsConnected();
        int aRoomSpec = (int)H(Op::EventClientActionRoomSpec, inEvent);
        int anAction = (int)H(Op::EventClientActionAction, inEvent);
        switch (ee_won_lanlogic_client_action(aConnected, aRoomSpec, aClient != NULL, anAction)) {
        case 18: this->ShowClientOptions(aClient, H(Op::EventClientActionRoomSpec, inEvent)); return;
        case 5: H(Op::SetClientIgnored, aClient, NULL, 1, 8); return;
        case 12: H(Op::SetClientIgnored, aClient, NULL, 0, 8); return;
        case 16: H(Op::LanSetChatInput, aClient, NULL, H(Op::EventClientActionRoomSpec, inEvent)); return;
        case 7: H(Op::KickClientFromLobby, aClient); return;
        }
    }
    void SendChat(const std::string& inText, unsigned short inFlags, unsigned short inTarget)
    {
        std::string aRequest;
        ee_won_lan_chat_request(inFlags, U8(inText), inText.length(), inTarget, Append, &aRequest);
        this->SendMessageToCaptain(Built(aRequest));
    }
    void HandleChatCommand(void* inTarget)
    {
        if (!this->IsConnected())
            return;
        void* aParser = HP(Op::ChatParser);
        if (aParser == NULL || !(H(Op::ParserRoomFlags, aParser) & 8))
            return;
        void* aClient = HP(Op::ParserTarget, aParser);
        int aRoomFlags = (int)H(Op::ParserRoomFlags, aParser);
        int aCommandId = (int)H(Op::ParserCommand, aParser);
        EeWonLanChatCommand aCommand = ee_won_lanlogic_chat_command(true, aRoomFlags, aCommandId, aClient != NULL);
        switch (aCommand.action) {
        case 1: {
            std::string aText;
            HT(Op::ParserTextTake, aParser, aText);
            unsigned short aTo = aCommand.whisper ? (unsigned short)H(Op::ClientId, aClient) : 0;
            this->SendChat(aText, aCommand.flags, aTo);
            return;
        }
        case 2: H(Op::SetClientIgnored, aClient, NULL, !H(Op::ClientIsIgnored, aClient), 4); return;
        case 3: H(Op::LanChatCommandError, inTarget); break;
        }
    }
    void DiscoverySend(const std::string& inBytes) { HM(Op::LanDiscoverySend, Built(inBytes), HP(Op::LanDiscoveryAddress)); }
    void RefreshGames()
    {
        int aKind = ee_won_lanlogic_refresh((int)H(Op::GameScreenType));
        if (aKind == 1) {
            H(Op::LanClearLanGames);
            if (H(Op::LanDiscoverySocket) == 0)
                return;
            this->DiscoverySend(Discovery(4, 0));
        } else if (aKind == 3) {
            H(Op::LanPingDirectGames);
        }
    }
    void EnterLanScreen()
    {
        H(Op::LanEnterScreenLists);
        this->StartDiscovery();
        H(Op::ShowLanScreen);
    }
    bool NeedsPeerAuth(void* inGame)
    {
        if (H(Op::GameType, inGame) != 3) return false;
        int aType = (int)H(Op::GameType, inGame);
        return ee_won_lanlogic_needs_peer_auth(aType, H(Op::DoCDKeyCheck) != 0);
    }
    bool HostGame(void* inGame)
    {
        this->CloseConnections();
        if (this->NeedsPeerAuth(inGame)) {
            this->mRequirePeerAuth = true;
            if (!H(Op::LanRefreshCertificate))
                return false;
        }
        H(Op::KillFirewallTest);
        this->mIsCaptain = true;
        H(Op::LanHostMyClient, inGame, NULL, this->GetNextClientId());
        if (!H(Op::LanHostListen, inGame))
            return false;
        this->mLastBroadcastTime = 0;
        H(Op::LanStartNetworkTimer);
        return true;
    }
    void KickClient(void* inClient)
    {
        if (this->IsCaptain())
            H(Op::LanSocketClose, HP(Op::LanClientSocket, inClient), NULL, 1000);
    }
    void AdvertiseGame(void* inGame)
    {
        if (ee_won_lanlogic_advertises((int)H(Op::GameType, inGame)))
            this->BroadcastGameSummary(inGame, HP(Op::LanDiscoveryAddress));
    }
    void RemoveGameAdvertisement(void* inGame)
    {
        if (ee_won_lanlogic_advertises((int)H(Op::GameType, inGame)))
            this->BroadcastGameRemoved(inGame);
    }
    bool CanBroadcastGame(void* inGame)
    {
        bool aDiscovery = H(Op::LanDiscoverySocket) != 0;
        int aType = inGame != NULL ? (int)H(Op::GameType, inGame) : 0;
        return ee_won_lanlogic_can_broadcast(this->IsCaptain(), aDiscovery, inGame != NULL, aType);
    }
    void BroadcastGameSummary(void* inGame, void* inAddress)
    {
        if (!this->CanBroadcastGame(inGame))
            return;
        std::string aHead = Discovery(1, (uint16_t)H(Op::GameAddressPort, inGame));
        H(Op::LanSendSummary, inGame, inAddress, (intptr_t)aHead.data(), (intptr_t)aHead.size());
    }
    void BroadcastGameKeepAlive(void* inGame)
    {
        if (!this->CanBroadcastGame(inGame))
            return;
        long long aTime = H(Op::Now);
        if (ee_won_lan_summary_due(aTime, this->mLastBroadcastTime))
            this->BroadcastGameSummary(inGame, HP(Op::LanDiscoveryAddress));
        else
            this->DiscoverySend(Discovery(2, (uint16_t)H(Op::GameAddressPort, inGame)));
        this->mLastBroadcastTime = aTime;
    }
    void BroadcastGameRemoved(void* inGame)
    {
        if (!this->CanBroadcastGame(inGame))
            return;
        this->DiscoverySend(Discovery(3, (uint16_t)H(Op::GameAddressPort, inGame)));
    }
    void HandleDiscoveryTimer(void* inOp)
    {
        if (inOp != HP(Op::LanDiscoveryTimer))
            return;
        if (this->IsCaptain()) {
            void* aGame = HP(Op::LanMyGame);
            if (aGame != NULL)
                this->BroadcastGameKeepAlive(aGame);
        }
        H(Op::LanRemoveStaleGames);
        H(Op::LanStartDiscoveryTimer);
    }
    void HandleDiscoveryRecv(void* inOp)
    {
        if (!H(Op::LanDiscoveryOk, inOp))
            return;
        std::string aBytes;
        HT(Op::LanDiscoveryTake, inOp, aBytes);
        uint8_t aKind = 0;
        uint16_t aPort = 0;
        if (!ee_won_lan_read_discovery(U8(aBytes), aBytes.size(), &aKind, &aPort))
            return;
        switch (aKind) {
        case 1: H(Op::LanGameSummary, inOp, NULL, aPort); break;
        case 2: H(Op::LanGameKeepAlive, inOp, NULL, aPort); break;
        case 3: H(Op::LanGameRemoved, inOp, NULL, aPort); break;
        case 4: {
            void* aFrom = HP(Op::LanRecvFrom, inOp);
            void* aGame = HP(Op::LanMyGame);
            if (aGame != NULL)
                this->BroadcastGameSummary(aGame, aFrom);
            break;
        }
        }
    }
    void StartDiscovery()
    {
        H(Op::LanStopDiscovery);
        if (H(Op::LanOpenDiscovery) != 0) {
            H(Op::LanStopDiscovery);
            return;
        }
        H(Op::LanDiscoveryArm);
        H(Op::LanStartDiscoveryTimer);
        this->DiscoverySend(Discovery(4, 0));
        if (!this->IsCaptain())
            return;
        void* aGame = HP(Op::LanMyGame);
        if (aGame != NULL)
            this->BroadcastGameSummary(aGame, HP(Op::LanDiscoveryAddress));
    }
    void Logout()
    {
        H(Op::LanStopDiscovery);
        H(Op::LanClearDirectGames);
    }
    void HandleAccept(void* inOp)
    {
        void* aSocket = HP(Op::LanAcceptedSocket, inOp);
        if (aSocket == NULL) return;
        if (this->mRequirePeerAuth) H(Op::LanStartPeerAuth, aSocket);
        else H(Op::LanAddPending, aSocket);
    }
    bool JoinGame(void* game)
    {
        this->CloseConnections();
        if (this->NeedsPeerAuth(game)) {
            this->mRequirePeerAuth = true;
            if (!H(Op::LanRefreshCertificate)) return false;
        }
        H(Op::LanNewServerSocket);
        bool joined = false;
        bool needPassword = H(Op::GameHasPassword, game) != 0;
        bool needConnect = true;
        while (!joined) {
            H(Op::LanJoinPasswordClear);
            if (needPassword) {
                if (H(Op::LanPasswordDialogRun) != 100001) return false;
            }
            if (needConnect) {
                if (!H(Op::LanConnect, game, NULL, this->mRequirePeerAuth)) return false;
                needConnect = false;
            }
            std::string name, secret;
            HT(Op::LanNameTake, NULL, name);
            HT(Op::LanJoinPasswordTake, NULL, secret);
            std::string request;
            ee_won_lan_join_request(U8(name), name.length(), U8(secret), secret.length(), Append, &request);
            HM(Op::LanServerSend, Built(request), NULL);
            this->mLastServerPing = H(Op::Now);
            std::string aReply;
            intptr_t aGot = HT(Op::LanRecvReplyTake, NULL, aReply);
            if (aGot < 0) return false;
            Msg aMsg = {NULL, aReply, aGot != 0};
            int status = this->ReadJoinReply(aMsg, game);
            switch (ee_won_lanlogic_join_step(status)) {
            case EeWonLanJoinStep::Joined: joined = true; break;
            case EeWonLanJoinStep::Fail: return false;
            case EeWonLanJoinStep::WrongPassword:
                H(Op::MessageBox, NULL, NULL, (intptr_t)LT::WrongPassword);
                needPassword = true;
                break;
            case EeWonLanJoinStep::OtherSubnet:
                H(Op::MessageBox, NULL, NULL, (intptr_t)LT::OtherSubnet);
                return false;
            case EeWonLanJoinStep::Again: break;
            }
        }
        H(Op::LanJoinArm, game);
        return true;
    }
};

// ---- the sequences

struct Call {
    EeWonLanCall mCall;
    void* mA;
    void* mB;
    intptr_t mC;
    Msg mMsg;
};

static unsigned sCallSeed;
static unsigned C(unsigned n) { return Rand(sCallSeed) % n; }
static void* AClient() { unsigned r = C(6); return r == 0 ? NULL : P(0x2001 + (r & 1)); }
static void* AGame() { return P(0x1001 + C(2)); }
static void* AnOp(uintptr_t inBase) { return P(inBase + 1 + C(2)); }

static bool IsMessageCall(EeWonLanCall inCall)
{
    return inCall == EeWonLanCall::SendToCaptain || inCall == EeWonLanCall::BroadcastToGame ||
           inCall == EeWonLanCall::SendToClient;
}

static Call NextCall()
{
    Call c = {EeWonLanCall::Control, NULL, NULL, 0, {NULL, std::string(), false}};
    switch (C(24)) {
    case 0: c.mCall = EeWonLanCall::Control; c.mC = C(6); break;
    case 1: c.mCall = EeWonLanCall::ComponentEvent; c.mA = P(0x4001);
            c.mC = C(4) == 0 ? 100001 : C(3) == 0 ? 100005 : C(2) ? 100008 : 0x186ac; break;
    case 2: case 3: c.mCall = EeWonLanCall::HostGame; c.mA = AGame(); break;
    case 4: case 5: c.mCall = EeWonLanCall::JoinGame; c.mA = AGame(); break;
    case 6: case 7: {
        unsigned k = C(3);
        c.mCall = k == 0 ? EeWonLanCall::SendToCaptain : k == 1 ? EeWonLanCall::BroadcastToGame : EeWonLanCall::SendToClient;
        if (k == 2) c.mB = AClient();
        if (C(4) != 0) { c.mMsg.mBuffer = P(0x3100); c.mMsg.mPresent = true; c.mMsg.mBytes = std::string("\x05\x01\x00\x01", C(5)); }
        break;
    }
    case 8: c.mCall = C(2) ? EeWonLanCall::AdvertiseGame : EeWonLanCall::RemoveGameAdvertisement; c.mA = AGame(); break;
    case 9: c.mCall = EeWonLanCall::KickClient; c.mB = P(0x2001 + C(2)); break;
    case 10: c.mCall = C(2) ? EeWonLanCall::LeaveGame : EeWonLanCall::Logout; break;
    case 11: case 12: c.mCall = EeWonLanCall::SocketClosed; c.mA = AnOp(0x6100); break;
    case 13: c.mCall = EeWonLanCall::NetworkTimer; c.mA = AnOp(0x6000); break;
    case 14: case 15: case 16: case 17: c.mCall = EeWonLanCall::RecvMsg; c.mA = AnOp(0x6200); break;
    case 18: c.mCall = EeWonLanCall::DiscoveryTimer; c.mA = AnOp(0x6000); break;
    case 19: case 20: case 21: c.mCall = EeWonLanCall::DiscoveryRecv; c.mA = AnOp(0x6300); break;
    default: c.mCall = EeWonLanCall::Accept; c.mA = AnOp(0x6400); break;
    }
    return c;
}

static void Args(const Call& c, void*& outA, intptr_t& outC, intptr_t& outD)
{
    outA = c.mA;
    outC = c.mC;
    outD = 0;
    if (IsMessageCall(c.mCall)) {
        outA = c.mMsg.mBuffer;
        outC = c.mMsg.mPresent ? (intptr_t)c.mMsg.mBytes.data() : 0;
        outD = (intptr_t)c.mMsg.mBytes.size();
    }
}

static std::string CallLine(const Call& c)
{
    void* aA;
    intptr_t aC, aD;
    Args(c, aA, aC, aD);
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "== %u %llx %llx %lld m:", (unsigned)c.mCall, (unsigned long long)(uintptr_t)aA,
             (unsigned long long)(uintptr_t)c.mB, (long long)(IsMessageCall(c.mCall) ? 0 : aC));
    return std::string(aLine) + (c.mMsg.mPresent ? Hex(c.mMsg.mBytes.data(), c.mMsg.mBytes.size()) : "-");
}

static intptr_t RunOrig(Orig& o, const Call& c)
{
    switch (c.mCall) {
    case EeWonLanCall::Control: o.HandleControlEvent((int)c.mC); break;
    case EeWonLanCall::ComponentEvent: o.HandleComponentEvent((int)c.mC, c.mA); break;
    case EeWonLanCall::HostGame: return o.HostGame(c.mA);
    case EeWonLanCall::JoinGame: return o.JoinGame(c.mA);
    case EeWonLanCall::SendToCaptain: o.SendToCaptain(c.mMsg); break;
    case EeWonLanCall::BroadcastToGame: o.BroadcastToGame(c.mMsg); break;
    case EeWonLanCall::SendToClient: o.SendToClient(c.mB, c.mMsg); break;
    case EeWonLanCall::AdvertiseGame: o.AdvertiseGame(c.mA); break;
    case EeWonLanCall::RemoveGameAdvertisement: o.RemoveGameAdvertisement(c.mA); break;
    case EeWonLanCall::KickClient: o.KickClient(c.mB); break;
    case EeWonLanCall::LeaveGame: o.CloseConnections(); break;
    case EeWonLanCall::Logout: o.Logout(); break;
    case EeWonLanCall::SocketClosed: o.HandleSocketClosed(c.mA); break;
    case EeWonLanCall::NetworkTimer: o.HandleNetworkTimer(c.mA); break;
    case EeWonLanCall::RecvMsg: o.HandleRecvMsg(c.mA); break;
    case EeWonLanCall::DiscoveryTimer: o.HandleDiscoveryTimer(c.mA); break;
    case EeWonLanCall::DiscoveryRecv: o.HandleDiscoveryRecv(c.mA); break;
    case EeWonLanCall::Accept: o.HandleAccept(c.mA); break;
    }
    return 0;
}

static std::vector<std::string> Sequence(int inSeq, bool inRust)
{
    sTrace.clear();
    sRustRun = inRust;
    sAnswerSeed = 0x7a31u + (unsigned)inSeq * 7919u;
    sCallSeed = 0x33u + (unsigned)inSeq * 104729u;
    sGameTypeBase = (unsigned)inSeq % 5;
    Orig o;
    EeWonLan* r = ee_won_lan_new(P(0x52), ScriptedHost);
    for (int i = 0; i < 8; i++) {
        Call c = NextCall();
        sTrace.push_back(CallLine(c));
        intptr_t aResult;
        if (inRust) {
            void* aA;
            intptr_t aC, aD;
            Args(c, aA, aC, aD);
            aResult = ee_won_lan_call(r, c.mCall, aA, c.mB, aC, aD);
        } else {
            aResult = RunOrig(o, c);
        }
        sTrace.push_back("=> " + std::to_string((long long)aResult));
    }
    ee_won_lan_free(r);
    return sTrace;
}

static std::set<std::string> sSeen;

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000, kGolden = 100;
    size_t aLines = 0;
    for (int s = 0; s < kSequences; s++) {
        std::vector<std::string> aWant = Sequence(s, false);
        std::vector<std::string> aGot = Sequence(s, true);
        aLines += aWant.size();
        CHECK(aWant.size() == aGot.size());
        for (size_t i = 0; i < aWant.size() && i < aGot.size(); i++) {
            sChecks++;
            if (aWant[i] != aGot[i]) {
                sFailures++;
                if (sFailures < 20)
                    printf("FAIL sequence %d line %u: want %s got %s\n", s, (unsigned)i, aWant[i].c_str(), aGot[i].c_str());
                break;
            }
        }
        bool aNew = false;
        for (size_t i = 0; i < aWant.size(); i++) {
            const std::string& aLine = aWant[i];
            std::string aKey = aLine.substr(0, aLine.find(' ', aLine.compare(0, 2, "==") == 0 ? 3 : 0));
            if (aKey.compare(0, 2, "=>") != 0 && sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && (s < kGolden || aNew)) {
            printf("## %d\n", s);
            for (size_t i = 0; i < aWant.size(); i++) printf("%s\n", aWant[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_lanobj_test: %d sequences, %u trace lines, %u distinct ops and calls, %d checks, %d failures\n",
               kSequences, (unsigned)aLines, (unsigned)sSeen.size(), sChecks, sFailures);
    return sFailures;
}
