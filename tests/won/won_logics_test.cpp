// PingLogic (with PingGameOp), AccountLogic and InitLogic as Rust objects (R6 WONLobby round 8): the original
// PingLogic.cpp, AccountLogic.cpp and InitLogic.cpp, their statements copied from the C++ before the port with every
// call on a C++ object (the games, the ops, the tracker, the server lists, the dialogs, the users, the events, the
// files, the Lobby* helpers) replaced by the host op that makes it (the units' hosts run the same calls), against
// ee_won's logics.rs over one scripted host.
//
// Each of 60,000 sequences makes the three logics and makes 1 to 16 random calls on them and on the ping ops they
// create or make with any request id (the methods, the completions, the ping ops' RunHook and CheckResponse over
// random replies, received
// datagrams over random bytes), then frees them. The host answers every op from a seeded generator (the same stream
// for both sides); a tracked ping op's completion is run at once (re-entering the logic) a quarter of the time, and
// killing the tracker re-enters PingGameDone half of the time. References are not traced (the original's SmartPtr
// temporaries take and give back ones the Rust does not take); each object's count is, at the end of the sequence.
// The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_logics.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include "rust/ee_won.h"

typedef EeWonLogicOp Op;
typedef EeWonLogicCall Call;

static int sFailures;
static int sChecks;

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;
static std::map<intptr_t, int> sRefs;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0,
                 long long f = 0)
{
    char aBuffer[400];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f);
    sTrace.push_back(aBuffer);
}
static std::string Hex(const void* inData, size_t inLength)
{
    std::string s;
    char x[4];
    for (size_t i = 0; i < inLength; i++) { snprintf(x, sizeof(x), "%02x", static_cast<const unsigned char*>(inData)[i]); s += x; }
    return s;
}

static const intptr_t kPing = 0x7100, kAcct = 0x7200, kInit = 0x7300;
static bool sRust;
static EeWonLogic* sRP; // the Rust objects
static EeWonLogic* sRA;
static EeWonLogic* sRI;
static int sDepth;
static int sOps;
static intptr_t sNextHandle;
static std::vector<intptr_t> sPingOps; // the ping ops made (handles)
static std::map<intptr_t, void*> sRecords; // a ping op's record (the Rust's, or the original's)
static std::map<intptr_t, intptr_t> sOpGames; // a ping op's game
static std::wstring sEventName; // the login event's user name
static std::vector<unsigned char> sMsg; // the received datagram

// the documents DocBytes answers
static const char* const kDocs[] = {"link=http://ee/\nimage=http://ee/a.png\n", "image\n", "link x\nIMAGE  y\r\n", "z", ""};

static intptr_t LogicCall(Call inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0);
static intptr_t Host(void* inObj, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
struct OrigPingOp;
static OrigPingOp* NewOrigPingOp(intptr_t inId, intptr_t inFlags);

static intptr_t Game() { return 0x3000 + 0x10 * (intptr_t)R(4); }
static intptr_t Status()
{
    static const intptr_t kStatus[] = {0, 0, 0, -1223, -2816, -2815, -2814, -2800, 100001, 5};
    return kStatus[R(10)];
}

static intptr_t Host(void* inObj, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    sOps++;
    intptr_t r = 0;
    intptr_t o = (intptr_t)inObj;
    switch (inOp) {
    case Op::AddRef: sRefs[a]++; return 0;
    case Op::Release: sRefs[a]--; return 0;
    case Op::BufAppend:
        Line("op %lld %lld %lld '", (long long)inOp, o, a);
        sTrace.back() += Hex(reinterpret_cast<const void*>(b), (size_t)c) + "'";
        return 0;
    case Op::RequestSend:
        Line("op %lld %lld '", (long long)inOp, o);
        sTrace.back() += Hex(reinterpret_cast<const void*>(a), (size_t)b) + "'";
        return 0;
    case Op::SetPromotionLink: case Op::FetchPromotionImage:
        Line("op %lld %lld '", (long long)inOp, o);
        sTrace.back() += std::string(reinterpret_cast<const char*>(a), (size_t)b) + "'";
        return 0;
    case Op::RecvMsg: {
        unsigned k = R(5);
        Line("op %lld %lld %lld = msg %lld %lld", (long long)inOp, o, a, k, (long long)sMsg.size());
        if (k == 0) { *reinterpret_cast<size_t*>(b) = 0; return 0; } // no message
        *reinterpret_cast<size_t*>(b) = sMsg.size();
        return sMsg.empty() ? (intptr_t)1 : (intptr_t)&sMsg[0]; // (an empty message's data is not NULL)
    }
    case Op::LoginName:
        *reinterpret_cast<size_t*>(b) = sEventName.size();
        Line("op %lld %lld %lld = %lld", (long long)inOp, o, a, (long long)sEventName.size());
        return (intptr_t)sEventName.data();
    case Op::LoginStripAt: sEventName = sEventName.substr(1); break;
    case Op::ReadSummary: case Op::ReadDetails: {
        EeWonReadBuffer* aReader = reinterpret_cast<EeWonReadBuffer*>(b);
        long aLeft = (long)aReader->length - (long)aReader->pos;
        Line("op %lld %lld %lld at %lld of %lld", (long long)inOp, o, a, (long long)aReader->pos, (long long)aReader->length);
        sTrace.back() += " '" + Hex(aReader->data + aReader->pos, (size_t)(aLeft > 0 ? aLeft : 0)) + "'";
        aReader->pos += (long)R((unsigned)(aLeft > 0 ? aLeft : 0) + 1);
        r = R(2);
        sTrace.back() += " = " + std::to_string((long long)r) + " " + std::to_string((long long)aReader->pos);
        return r;
    }
    case Op::TOUTimes: {
        int32_t s = (int32_t)R(100), g = (int32_t)R(100);
        *reinterpret_cast<int32_t*>(a) = s;
        *reinterpret_cast<int32_t*>(b) = g;
        Line("op %lld %lld = %lld %lld", (long long)inOp, o, s, g);
        return 0;
    }
    case Op::DocBytes: {
        unsigned k = R(6);
        if (k == 5) { *reinterpret_cast<size_t*>(c) = (size_t)-1; r = 0; }
        else { *reinterpret_cast<size_t*>(c) = strlen(kDocs[k]) + 1; r = (intptr_t)kDocs[k]; }
        Line("op %lld %lld %lld %lld = doc %lld", (long long)inOp, o, a, b, k);
        return r;
    }
    case Op::OpRecord: r = (intptr_t)sRecords[a]; Line("op %lld %lld %lld", (long long)inOp, o, a); return r;
    case Op::OpGame: r = sOpGames[a]; break;
    case Op::NewPingOp: {
        r = 0xA000 + 0x10 * sNextHandle++;
        sRefs[r]++;
        sPingOps.push_back(r);
        sOpGames[r] = a;
        sRecords[r] = sRust ? (void*)ee_won_pingop_new((void*)r, Host, (uint16_t)b, (uint8_t)c) : (void*)NewOrigPingOp(b, c);
        break;
    }
    case Op::QueryAccount: case Op::CreateAccount: case Op::GetCert:
        r = 0xC000 + 0x10 * sNextHandle++;
        sRefs[r]++;
        break;
    case Op::ReplyNew: r = 0xB000 + 0x10 * sNextHandle++; break;
    case Op::TextNew: case Op::TextStatus: r = 0xD000 + 0x10 * sNextHandle++; break;
    case Op::OpStatus: r = Status(); break;
    case Op::VersionStatus: r = R(3) == 0 ? -2814 : 0; break;
    case Op::OpKilled: r = R(5) == 0; break;
    case Op::AuthContext: case Op::Root: case Op::RootWindow: case Op::AccountServers: case Op::DirServers:
    case Op::FirewallServers: case Op::UdpManager:
        r = R(6) ? 0x1000 + 0x10 * (intptr_t)inOp : 0; break;
    case Op::MyGame: case Op::EventGame: case Op::TrackedGame: r = R(5) ? Game() : 0; break;
    case Op::GameType: r = R(4); break;
    case Op::ConnectionSpeed: r = R(4) == 0 ? 65535 : R(4); break;
    case Op::NumTracked: r = R(5); break;
    case Op::PingCeiling: r = R(2) ? 5000 : 1234; break;
    case Op::Elapsed: r = R(3) == 0 ? 70000 + R(10) : R(900); break;
    case Op::OutOfDateDialog: r = R(2) ? 100001 : 100002; break;
    case Op::ShowTOU: r = R(3); break;
    case Op::KeyMod: r = R(9); break;
    case Op::AdapterDialog: r = R(2) ? 100001 : 2; break;
    case Op::OpNum:
        r = b == 40 ? R(6) : b == 1 ? R(4) : b == 7 ? R(200) : R(3) == 0; break; // lists, type, modified, status
    case Op::ServerCount: case Op::TextLength: r = R(3); break;
    case Op::FirewallBind: case Op::FirewallListen: r = R(3) == 0 ? -1 : 0; break;
    case Op::CDKeyValid: case Op::DnsTestReady: case Op::VersionReady: case Op::DirReady: case Op::DnsReady:
    case Op::SaveTOU:
        r = R(4) != 0; break;
    case Op::EventFlag: case Op::GameIsHost: case Op::GameInProgress: case Op::GameIsPinging: case Op::GameQueryingDetails:
    case Op::NoPingInProgress: case Op::UserNeedsTOU: case Op::DoCDKeyCheck: case Op::CheckForInternet: case Op::HasInternet:
    case Op::LobbyMusic: case Op::RoomDirEmpty: case Op::HasAuthConfig: case Op::IsAcceptOp: case Op::AcceptedFromFirewall:
    case Op::HasFirewallSocket:
        r = R(2); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld %lld %lld", (long long)inOp, o, a, b, c, d);
    sTrace.back() += " = " + std::to_string((long long)r);
    // a tracked op whose completion runs at once; a kill that runs a completion
    if (sDepth == 0 && ((inOp == Op::RunTracked && R(4) == 0) || (inOp == Op::KillTracked && !sPingOps.empty() && R(2) == 0))) {
        sDepth++;
        intptr_t anOp = inOp == Op::RunTracked ? a : sPingOps[R((unsigned)sPingOps.size())];
        Line("reenter done %lld", anOp);
        LogicCall(Call::PingDone, anOp);
        sDepth--;
    }
    return r;
}

static intptr_t O(intptr_t inObj, Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return Host((void*)inObj, inOp, a, b, c, d);
}
static intptr_t P(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return O(kPing, inOp, a, b, c, d); }
static intptr_t A(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return O(kAcct, inOp, a, b, c, d); }
static intptr_t I(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return O(kInit, inOp, a, b, c, d); }

// ---- the original classes, every call on another object replaced by its op

struct ReadBufferException {};
// the original's ReadBuffer over the reader state the ops take
struct Reader {
    EeWonReadBuffer s;
    Reader(const void* inData, size_t inLength) { ee_won_reader_set(&s, static_cast<const uint8_t*>(inData), (unsigned long)inLength); }
    const uint8_t* Take(unsigned long n) { const uint8_t* p = NULL; if (!ee_won_reader_take(&s, n, &p)) throw ReadBufferException(); return p; }
    unsigned char ReadByte() { return *Take(1); }
    short ReadShort() { short v; memcpy(&v, Take(2), 2); return v; }
    int Available() { return (int)s.length - (int)s.pos; }
};

// SmartPtr<LobbyGame>: AddRef on taking, Release on letting go
struct Ptr {
    intptr_t p;
    Ptr(intptr_t q = 0) : p(q) { if (p) P(Op::AddRef, p); }
    Ptr(const Ptr& o) : p(o.p) { if (p) P(Op::AddRef, p); }
    ~Ptr() { if (p) P(Op::Release, p); }
    Ptr& operator=(const Ptr& o) { intptr_t old = p; p = o.p; if (p) P(Op::AddRef, p); if (old) P(Op::Release, old); return *this; }
};

// PingGameOp
struct OrigPingOp {
    unsigned short mRequestId, mPing;
    unsigned char mReplyFlags;
    unsigned short mRetries;
    bool mHasReply;
    std::vector<unsigned char> mReply;
};
static OrigPingOp* NewOrigPingOp(intptr_t inId, intptr_t inFlags)
{
    OrigPingOp* x = new OrigPingOp();
    x->mRetries = 0;
    x->mRequestId = (unsigned short)inId;
    x->mReplyFlags = (unsigned char)inFlags;
    x->mPing = 0;
    x->mHasReply = false;
    return x;
}

static void OrigRunHook(OrigPingOp* x, intptr_t inOp, intptr_t inGame)
{
    // WriteBuffer(0, true): 7, the request id, the game's type, the flags; SetOutgoing; mPing; the base's RunHook
    unsigned char aRequest[5] = {7, (unsigned char)(x->mRequestId & 0xff), (unsigned char)(x->mRequestId >> 8), 0, 0};
    aRequest[3] = (unsigned char)O(inOp, Op::GameType, inGame);
    aRequest[4] = x->mReplyFlags;
    x->mPing = 0xffff;
    O(inOp, Op::RequestSend, (intptr_t)aRequest, 5);
}

static long long OrigCheckResponse(OrigPingOp* x, intptr_t inOp, Reader& inBuffer)
{
    try {
        if (inBuffer.ReadByte() != 8)
            return 100004; // WS_None
        if (inBuffer.ReadShort() != x->mRequestId)
            return 100004; // the signed ReadShort is compared with the unsigned member
        x->mPing = (unsigned short)O(inOp, Op::Elapsed);
        x->mReplyFlags = 0;
        if (!(inBuffer.Available() > 0))
            return 0;
        x->mReplyFlags = inBuffer.ReadByte();
        unsigned long aLength = inBuffer.Available();
        const uint8_t* aBytes = inBuffer.Take(aLength);
        x->mReply.assign(aBytes, aBytes + aLength);
        x->mHasReply = true;
        return 0;
    } catch (ReadBufferException&) {
        return EE_WON_LOGIC_THREW;
    }
}

static struct OrigPing { unsigned short mNextRequestId; std::list<Ptr> mPending; } *sOP;

static void OrigPingGame(intptr_t inGame, bool inDetails, bool inForce);

static void OrigHandlePingRequest(Reader& inBuffer, intptr_t inRecv)
{
    intptr_t aGame = P(Op::MyGame);
    if (aGame == 0 || !P(Op::GameIsHost, aGame))
        return;
    unsigned short anId = inBuffer.ReadShort();
    unsigned char aKind = inBuffer.ReadByte();
    if (P(Op::GameType, aGame) != aKind)
        return;
    if (P(Op::GameInProgress, aGame)) {
        if (P(Op::NoPingInProgress))
            return;
    }
    intptr_t aBuffer = P(Op::ReplyNew); // WriteBuffer aBuffer(3, true)
    try {
        unsigned char aHead[3] = {8, (unsigned char)(anId & 0xff), (unsigned char)(anId >> 8)};
        P(Op::BufAppend, aBuffer, (intptr_t)aHead, 3); // AppendByte(8); AppendShort(anId) (consecutive appends as one)
        unsigned char aFlags = inBuffer.ReadByte() & 3;
        P(Op::BufAppend, aBuffer, (intptr_t)&aFlags, 1);
        if (aFlags & 1)
            P(Op::WriteSummary, aGame, aBuffer);
        if (aFlags & 2)
            P(Op::WriteDetails, aGame, aBuffer);
        intptr_t aManager = P(Op::UdpManager);
        if (aManager != 0)
            P(Op::ReplySend, aManager, aBuffer, inRecv);
    } catch (ReadBufferException&) {
        P(Op::BufFree, aBuffer);
        throw;
    }
    P(Op::BufFree, aBuffer);
}

static void OrigHandleUDPRecv(intptr_t inOp)
{
    if (P(Op::OpStatus, inOp) != 0 || P(Op::OpKilled, inOp))
        return;
    size_t aLength = 0;
    intptr_t aData = P(Op::RecvMsg, inOp, (intptr_t)&aLength);
    Reader aReadBuffer(aData != 0 ? (const void*)aData : NULL, aData != 0 ? aLength : 0);
    try {
        int aType = (char)aReadBuffer.ReadByte();
        switch (aType) {
        case 7: OrigHandlePingRequest(aReadBuffer, inOp); break;
        }
    } catch (ReadBufferException&) {}
}

static void OrigPingGameDone(intptr_t inOp)
{
    if (P(Op::OpKilled, inOp))
        return;
    OrigPingOp* anOp = (OrigPingOp*)P(Op::OpRecord, inOp);
    unsigned short aPing = anOp->mPing;
    intptr_t aGame = P(Op::OpGame, inOp);
    if (aPing != 0xffff)
        P(Op::GameSetPingReplied, aGame);
    else if (anOp->mRetries < 3) {
        ++anOp->mRetries;
        unsigned long aTimeout = (unsigned long)P(Op::PingCeiling);
        P(Op::RunTracked, inOp, (intptr_t)aTimeout);
        return;
    }
    P(Op::GameSetPing, aGame, aPing);
    if (anOp->mHasReply) {
        std::vector<unsigned char> aReply = anOp->mReply; // (the op's buffer, alive over the reads)
        Reader aBuffer(aReply.empty() ? NULL : &aReply[0], aReply.size());
        unsigned char aFlags = anOp->mReplyFlags;
        bool aReadOk = true;
        if (aFlags & 1)
            aReadOk = P(Op::ReadSummary, aGame, (intptr_t)&aBuffer.s) != 0;
        if ((aFlags & 2) && aReadOk)
            P(Op::ReadDetails, aGame, (intptr_t)&aBuffer.s);
    }
    P(Op::BroadcastGameChanged, aGame);
    P(Op::NotifyPing, aGame);
    int aMaximum = (unsigned short)P(Op::ConnectionSpeed);
    while (!sOP->mPending.empty()) {
        if (static_cast<long>(P(Op::NumTracked)) >= aMaximum && aMaximum != 0)
            break;
        Ptr aPendingGame = sOP->mPending.front();
        sOP->mPending.pop_front();
        if (!P(Op::GameIsPinging, aPendingGame.p))
            continue;
        if (P(Op::GameQueryingDetails, aPendingGame.p))
            continue;
        P(Op::GameSetPing, aPendingGame.p, 0xfffe);
        OrigPingGame(aPendingGame.p, false, true);
    }
}

static void OrigPingGame(intptr_t inGame, bool inDetails, bool inForce)
{
    if (inGame == 0)
        return;
    if (P(Op::GameInProgress, inGame)) {
        if (P(Op::NoPingInProgress))
            return;
    }
    if (inDetails) {
        if (P(Op::GameQueryingDetails, inGame))
            return;
    }
    else if (P(Op::GameIsPinging, inGame))
        return;
    if (!inForce && !inDetails) {
        unsigned short aMaximum = (unsigned short)P(Op::ConnectionSpeed);
        if (static_cast<long>(P(Op::NumTracked)) >= aMaximum && aMaximum > 0) {
            P(Op::GameSetPing, inGame, 0xfffd);
            P(Op::NotifyPing, inGame);
            sOP->mPending.push_back(Ptr(inGame));
            return;
        }
    }
    intptr_t aManager = P(Op::UdpManager);
    if (aManager == 0)
        return;
    P(Op::GameSetPing, inGame, inDetails ? 0xfffc : 0xfffd);
    P(Op::NotifyPing, inGame);
    unsigned char aFlags = 0;
    if (P(Op::GameType, inGame) == 3)
        aFlags = 1;
    if (inDetails)
        aFlags |= 2;
    intptr_t anOp = P(Op::NewPingOp, inGame, sOP->mNextRequestId++, aFlags, aManager); // held, the manager set
    unsigned long aTimeout = (unsigned long)P(Op::PingCeiling);
    P(Op::RunTracked, anOp, (intptr_t)aTimeout);
    P(Op::Release, anOp);
}

static void OrigKillPings()
{
    for (std::list<Ptr>::iterator it = sOP->mPending.begin(); it != sOP->mPending.end(); ++it)
        P(Op::GameSetPing, it->p, 0xfffe);
    for (int anIndex = 0; anIndex < (int)P(Op::NumTracked); anIndex++) {
        intptr_t aGame = P(Op::TrackedGame, anIndex);
        if (aGame != 0)
            P(Op::GameSetPing, aGame, 0xfffe);
    }
    sOP->mPending.clear();
    P(Op::KillTracked);
}

static intptr_t OrigPingCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::PingInit: { intptr_t m = P(Op::UdpManager); if (m != 0) P(Op::SetDefaultRecv, m); return 0; }
    case Call::PingEvent:
        if (b == 100037) {
            intptr_t aGame = P(Op::EventGame, a);
            bool aFlag = P(Op::EventFlag, a) != 0;
            OrigPingGame(aGame, aFlag, false);
        }
        return 0;
    case Call::PingRecv: OrigHandleUDPRecv(a); return 0;
    case Call::PingDone: OrigPingGameDone(a); return 0;
    case Call::PingNumToDo: { intptr_t n = (intptr_t)sOP->mPending.size(); return n + (int)P(Op::NumTracked); }
    case Call::PingGame: OrigPingGame(a, b != 0, c != 0); return 0;
    case Call::PingKill: OrigKillPings(); return 0;
    case Call::PingNextId: return sOP->mNextRequestId;
    case Call::PingPending: return (intptr_t)sOP->mPending.size();
    default: return 0;
    }
}

// AccountLogic
static struct OrigAcct { std::wstring mLastUserName; bool mNeedTOU; } *sOA;

static int WCompareNoCase(const std::wstring& x, const std::wstring& y)
{
#if WCHAR_MAX > 0xffff
    return ee_won_compare_no_case32(reinterpret_cast<const int32_t*>(x.data()), x.length(), reinterpret_cast<const int32_t*>(y.data()), y.length());
#else
    return ee_won_compare_no_case16(reinterpret_cast<const uint16_t*>(x.data()), x.length(), reinterpret_cast<const uint16_t*>(y.data()), y.length());
#endif
}
static std::wstring EventName(intptr_t inEvent)
{
    size_t n = 0;
    const wchar_t* p = reinterpret_cast<const wchar_t*>(A(Op::LoginName, inEvent, (intptr_t)&n));
    return std::wstring(p, n);
}

static bool OrigCheckLoginStatus()
{
    if (A(Op::VersionStatus) == -2814) {
        if (A(Op::OutOfDateDialog) == 100001)
            A(Op::Exit, 3);
        return false;
    }
    return true;
}
static bool OrigShowTOU() { return A(Op::ShowTOU) == 1; }

static void OrigHandleCreateAccount(intptr_t ev)
{
    if (!OrigCheckLoginStatus()) return;
    if (sOA->mNeedTOU) {
        if (!OrigShowTOU()) return;
        sOA->mNeedTOU = false;
    }
    intptr_t servers = A(Op::AccountServers);
    if (!servers) return;
    intptr_t op = A(Op::CreateAccount, servers, ev);
    if (A(Op::OpStatus, op) == 0)
        A(Op::AccountCreated, ev);
    A(Op::Release, op);
}

static void OrigHandleLogin(intptr_t ev)
{
    intptr_t auth = A(Op::AuthContext);
    intptr_t window = A(Op::RootWindow);
    if (!auth || !window) return;
    int keyMod = (int)A(Op::KeyMod, window);
    if ((keyMod & 7) == 7) { A(Op::ShowCredits); return; }
    std::wstring aName = EventName(ev);
    if (aName.empty() || !OrigCheckLoginStatus()) return;
    bool admin = false;
    if (aName.at(0) == L'@') {
        admin = true;
        A(Op::LoginStripAt, ev);
        aName = EventName(ev);
    }
    if (WCompareNoCase(aName, sOA->mLastUserName) != 0) {
        if (A(Op::UserNeedsTOU, ev) && !OrigShowTOU()) return;
        sOA->mLastUserName = aName;
    }
    A(Op::SetCredentials, auth, ev);
    if (A(Op::DoCDKeyCheck) && !A(Op::CDKeyValid)) {
        A(Op::InvalidCDKeyBox);
        return;
    }
    A(Op::SetAdmin, auth, admin);
    intptr_t op = A(Op::GetCert, auth);
    if (A(Op::OpStatus, op) != 0) { A(Op::Release, op); return; }
    A(Op::AutoRefresh, auth);
    A(Op::LoginSaveUser, ev);
    A(Op::BroadcastLobby, 100052);
    A(Op::EnterInternet, (keyMod & 4) == 0);
    A(Op::Release, op);
}

static intptr_t OrigAcctCall(Call inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case Call::AcctControl:
        switch (a) {
        case 1: if (OrigCheckLoginStatus()) { A(Op::ShowScreen, 1); sOA->mNeedTOU = true; } break;
        case 2: if (OrigCheckLoginStatus()) A(Op::ShowScreen, 2); break;
        case 3: if (OrigCheckLoginStatus()) A(Op::ShowScreen, 3); break;
        }
        return 0;
    case Call::AcctEvent:
        switch (b) {
        case 100010: OrigHandleCreateAccount(a); break;
        case 100018: case 100019: {
            intptr_t account = A(Op::AccountServers);
            if (account == 0 || !OrigCheckLoginStatus()) return 0;
            A(b == 100018 ? Op::RequestPasswordEmail : Op::RequestInfoEmail, a, account);
            break;
        }
        case 100035: OrigHandleLogin(a); break;
        case 100040: {
            intptr_t account = A(Op::AccountServers);
            intptr_t auth = A(Op::AuthContext);
            if (account == 0 || auth == 0) return 0;
            intptr_t op = A(Op::QueryAccount, account, auth);
            if (A(Op::OpStatus, op) == 0)
                A(Op::BroadcastAccount, op);
            A(Op::Release, op);
            break;
        }
        case 100060: {
            intptr_t account = A(Op::AccountServers);
            intptr_t auth = A(Op::AuthContext);
            if (account == 0 || auth == 0) return 0;
            A(Op::UpdateAccount, account, auth, a);
            break;
        }
        }
        return 0;
    case Call::AcctCredits: A(Op::ShowCredits); return 0;
    case Call::AcctShowTOU: return OrigShowTOU();
    case Call::AcctCheckLogin: return OrigCheckLoginStatus();
    case Call::AcctNeedTOU: return sOA->mNeedTOU;
    case Call::AcctLastNameLength: return (intptr_t)sOA->mLastUserName.size();
    default: return 0;
    }
}

// InitLogic
static struct OrigInit { unsigned long mErrors, mIgnoredErrors; } *sOI;
static const intptr_t kNoDesc = -1;
#define RES(n) ((intptr_t)EeWonLobbyRes::n)

static bool OrigSetError(int inError, intptr_t inText, intptr_t inRes)
{
    EeWonInitErrors anErrors = ee_won_init_errors(inError, (int)sOI->mIgnoredErrors);
    sOI->mErrors = anErrors.errors;
    if (sOI->mErrors != 0) {
        I(Op::InitStatus, RES(LobbyContainer_InitFailed_String), inText, inRes, (anErrors.allow_ignore ? 1 : 0) | 3 << 8);
        return true;
    }
    return false;
}
static void OrigStartDNSTest()
{
    if (!I(Op::DnsTestReady)) return;
    I(Op::InitStatus, RES(LobbyContainer_InitStart_String), 0, kNoDesc, 0 | 1 << 8);
    I(Op::RunDnsTest);
}
static void OrigStart(bool inRetry)
{
    I(Op::SetVersionStatus, 100004);
    sOI->mErrors = 0;
    if (I(Op::CheckForInternet) && ee_won_init_no_internet(true, I(Op::HasInternet) != 0)) {
        if (OrigSetError(1, 0, RES(LobbyContainer_NoConnection_String)))
            return;
    }
    if (!inRetry)
        I(Op::ClearServerLists);
    OrigStartDNSTest();
}
static void OrigInitInit()
{
    sOI->mIgnoredErrors = 0;
    I(Op::LoadPrefs);
    I(Op::LoadTOUTimes);
    I(Op::BindUdp);
    I(Op::BroadcastLobby, 100052);
    intptr_t aRoot = I(Op::Root);
    if (aRoot == 0) return;
    I(Op::EnableLogin, aRoot, 0);
    I(Op::Music, 0);
    if (I(Op::LobbyMusic)) I(Op::Music, 1);
    else I(Op::Music, 2);
    OrigStart(false);
}
static void OrigStartDirectoryQuery()
{
    intptr_t context = I(Op::DirServers);
    if (context == 0) return;
    I(Op::InitStatus, RES(LobbyContainer_QueryingServers_String), 0, kNoDesc, 0);
    I(Op::RunDirQuery, context);
}
static void OrigDNSCompletion(intptr_t inOp)
{
    if (!I(Op::DnsReady)) return;
    if (I(Op::OpStatus, inOp) == 0) {
        I(Op::FillDnsServers);
        if (I(Op::RoomDirEmpty)) I(Op::ResetRoomDir);
        I(Op::StartDocuments);
        OrigStartDirectoryQuery();
    } else {
        OrigSetError(2, 0, RES(LobbyContainer_DNSLookupFailed_String));
    }
}
struct Lines {
    std::string mLink, mImage;
    static void Link(void* c, const uint8_t* b, size_t n) { static_cast<Lines*>(c)->mLink.append(reinterpret_cast<const char*>(b), n); }
    static void Image(void* c, const uint8_t* b, size_t n) { static_cast<Lines*>(c)->mImage.append(reinterpret_cast<const char*>(b), n); }
};
static void OrigParseCrossPromotion(const char* inData, size_t inLength)
{
    Lines aLines;
    ee_won_init_cross_promotion(reinterpret_cast<const uint8_t*>(inData), inLength, Lines::Link, Lines::Image, &aLines);
    std::string imageURL(aLines.mImage);
    I(Op::SetPromotionLink, (intptr_t)aLines.mLink.data(), (intptr_t)aLines.mLink.size());
    if (!imageURL.empty())
        I(Op::FetchPromotionImage, (intptr_t)imageURL.data(), (intptr_t)imageURL.size());
}
static void OrigHandleTOU(intptr_t inOp)
{
    int32_t systemTime, gameTime;
    I(Op::TOUTimes, (intptr_t)&systemTime, (intptr_t)&gameTime);
    bool changed = false;
    int32_t aStatus = (int32_t)I(Op::OpNum, inOp, 5, 0);
    int32_t aModified = (int32_t)I(Op::OpNum, inOp, 7, 0);
    if (ee_won_init_tou_saved(aStatus, aModified, systemTime)) {
        if (I(Op::SaveTOU, inOp, 0)) changed = true;
    }
    aStatus = (int32_t)I(Op::OpNum, inOp, 5, 1);
    aModified = (int32_t)I(Op::OpNum, inOp, 7, 1);
    if (ee_won_init_tou_saved(aStatus, aModified, gameTime)) {
        if (I(Op::SaveTOU, inOp, 1)) changed = true;
    }
    if (changed) I(Op::LoadTOUTimes);
}
static void OrigHTTPDocumentCompletion(intptr_t inOp)
{
    I(Op::BroadcastDocument, inOp);
    switch (I(Op::OpNum, inOp, 1, 0)) {
    case 1: OrigHandleTOU(inOp); break;
    case 2: {
        size_t aLength = 0;
        intptr_t aData = I(Op::DocBytes, inOp, 1, (intptr_t)&aLength);
        if (aLength != (size_t)-1 && ee_won_init_document_bytes(aLength) != 0) {
            OrigParseCrossPromotion(reinterpret_cast<const char*>(aData), aLength);
            return;
        }
        I(Op::BroadcastNoPromotion);
        return;
    }
    }
}
static void OrigKillFirewallTest() { I(Op::KillAcceptOp); I(Op::FirewallSocketDrop); }
static void OrigStartVersionCheck()
{
    if (!I(Op::VersionReady)) return;
    I(Op::InitStatus, RES(LobbyContainer_CheckingVersion_String), 0, kNoDesc, 0);
    I(Op::RunVersionCheck);
}
static void OrigStartFirewallTest()
{
    intptr_t firewall = I(Op::FirewallServers);
    if (firewall == 0) return;
    OrigKillFirewallTest();
    I(Op::ResetFirewallTest);
    I(Op::RunFirewallDetect, firewall);
    I(Op::BroadcastLobby, 100055);
    I(Op::FirewallSocketNew);
    long aStatus = (long)I(Op::FirewallBind);
    if (aStatus == 0) {
        aStatus = (long)I(Op::FirewallListen);
        if (aStatus == 0) return;
    }
    I(Op::FirewallSocketDrop);
}
static void OrigDirectoryCompletion(intptr_t inOp)
{
    if (!I(Op::DirReady)) return;
    int status = (int)I(Op::OpStatus, inOp);
    if (ee_won_init_dir_answered(status)) {
        int count = (int)I(Op::OpNum, inOp, 40, 0);
        for (int index = 0; index < count; ++index) {
            if (I(Op::OpNum, inOp, 41, index) != 0) continue;
            int aTarget = ee_won_init_dir_request_target(index, I(Op::HasAuthConfig) != 0);
            if (aTarget != 0) I(Op::AddDirAddresses, aTarget, inOp, index);
        }
        if (I(Op::ServerCount, 1) == 0 || I(Op::ServerCount, 2) == 0 || I(Op::ServerCount, 4) == 0) {
            I(Op::DirNotifyFailed, inOp);
            intptr_t names = I(Op::TextNew, -1);
            size_t aAuth = (size_t)I(Op::ServerCount, 1);
            size_t aAccount = (size_t)I(Op::ServerCount, 2);
            size_t aVersions = (size_t)I(Op::ServerCount, 4);
            int errors = ee_won_init_missing_servers(aAuth, aAccount, aVersions);
            if (errors & 8) I(Op::TextAppendRes, names, RES(LobbyContainer_AuthServers_String));
            if (errors & 4) {
                if (I(Op::TextLength, names) != 0) I(Op::TextAppendComma, names);
                I(Op::TextAppendRes, names, RES(LobbyContainer_AccountServers_String));
            }
            if (errors & 0x10) {
                if (I(Op::TextLength, names) != 0) I(Op::TextAppendComma, names);
                I(Op::TextAppendRes, names, RES(LobbyContainer_VersionServers_String));
            }
            intptr_t description = I(Op::TextNew, RES(LobbyContainer_NoServers_String));
            I(Op::TextAppendText, description, names);
            bool aStop = OrigSetError(errors, description, kNoDesc);
            I(Op::TextFree, description);
            I(Op::TextFree, names);
            if (aStop) return;
        }
        OrigStartVersionCheck();
        OrigStartFirewallTest();
    } else {
        intptr_t description = I(Op::TextStatus, RES(LobbyContainer_DirLookupFailure_String), status);
        OrigSetError(0x20, description, kNoDesc);
        I(Op::TextFree, description);
    }
}
static void OrigVersionCompletion(intptr_t inOp)
{
    intptr_t root = I(Op::Root);
    if (root == 0) return;
    int status = (int)I(Op::OpStatus, inOp);
    I(Op::SetVersionStatus, status);
    int anOutcome = ee_won_init_version_outcome(status);
    if (anOutcome != 0) {
        if (anOutcome == 1) I(Op::VersionNotifyFailed, inOp);
        intptr_t description = I(Op::TextStatus, RES(LobbyContainer_VersionCheckFailure_String), status);
        OrigSetError(0x40, description, kNoDesc);
        I(Op::TextFree, description);
        return;
    }
    I(Op::EnableLogin, root, 1);
    I(Op::InitStatus, RES(LobbyContainer_InitComplete_String), 0, kNoDesc, 0 | 2 << 8);
}
static void OrigAcceptCompletion(intptr_t inOp)
{
    if (!I(Op::IsAcceptOp, inOp)) return;
    I(Op::ClearAcceptOp);
    bool anAccepted = I(Op::OpStatus, inOp) == 0;
    int anOutcome = ee_won_init_accept_outcome(anAccepted, anAccepted && I(Op::AcceptedFromFirewall, inOp) != 0);
    if (anOutcome == 0) { I(Op::SetBehindFirewall, 1); I(Op::BroadcastLobby, 100055); return; }
    if (anOutcome == 1) { I(Op::SetBehindFirewall, 0); I(Op::BroadcastLobby, 100055); return; }
    I(Op::RestartAccept, inOp);
}
static void OrigFirewallCompletion(intptr_t inOp)
{
    if (I(Op::OpStatus, inOp) == 0) {
        I(Op::SetPublicAddress, inOp);
        if (I(Op::HasFirewallSocket)) I(Op::StartAccept, inOp);
    } else {
        I(Op::SetBehindFirewall, 1);
    }
    I(Op::BroadcastLobby, 100055);
}

static intptr_t OrigInitCall(Call inCall, intptr_t a)
{
    switch (inCall) {
    case Call::InitInit: OrigInitInit(); return 0;
    case Call::InitStart: OrigStart(a != 0); return 0;
    case Call::InitKillHook: I(Op::KillHttp); OrigKillFirewallTest(); return 0;
    case Call::InitKillFirewall: OrigKillFirewallTest(); return 0;
    case Call::InitControl:
        switch (a) {
        case 1: I(Op::Exit, 0); break;
        case 2: I(Op::ShowInternet); break;
        case 3: if (I(Op::AdapterDialog) == 100001) I(Op::SetAdapter); break;
        }
        return 0;
    case Call::InitEvent:
        switch (ee_won_init_event((int)a)) {
        case 2: sOI->mIgnoredErrors |= sOI->mErrors; break;
        case 1: break;
        default: return 0;
        }
        OrigStart(true);
        return 0;
    case Call::InitDns: OrigDNSCompletion(a); return 0;
    case Call::InitDocument: OrigHTTPDocumentCompletion(a); return 0;
    case Call::InitPromotionImage: if (I(Op::OpStatus, a) == 0) I(Op::PromotionImage, a); return 0;
    case Call::InitDirectory: OrigDirectoryCompletion(a); return 0;
    case Call::InitVersion: OrigVersionCompletion(a); return 0;
    case Call::InitAccept: OrigAcceptCompletion(a); return 0;
    case Call::InitFirewall: OrigFirewallCompletion(a); return 0;
    case Call::InitField: return (intptr_t)(int32_t)(a == 0 ? sOI->mErrors : sOI->mIgnoredErrors);
    default: return 0;
    }
}

static intptr_t LogicCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    unsigned k = (unsigned)inCall;
    if (sRust) return ee_won_logic_call(k < 20 ? sRP : k < 40 ? sRA : sRI, inCall, a, b, c);
    return k < 20 ? OrigPingCall(inCall, a, b, c) : k < 40 ? OrigAcctCall(inCall, a, b) : OrigInitCall(inCall, a);
}

// ---- the sequences

enum What { kLogic, kRunHook, kCheck, kNewOp };
struct Step { What mWhat; int mCall; intptr_t a, b, c; std::vector<unsigned char> mBytes; std::wstring mName; };

static const wchar_t* const kNames[] = {L"", L"@Bob", L"bob", L"BOB", L"@", L"Alice", L"alice"};

static std::vector<unsigned char> RandomBytes(bool inRequest)
{
    std::vector<unsigned char> v;
    unsigned n = R(8);
    unsigned char aFirst = inRequest ? (R(5) ? 7 : (unsigned char)R(10)) : (R(5) ? 8 : (unsigned char)R(10));
    if (n > 0) v.push_back(aFirst);
    for (unsigned i = 1; i < n; i++) v.push_back((unsigned char)(i <= 2 && R(2) ? R(3) : R(256)));
    if (!inRequest && v.size() >= 3 && R(3)) { v[1] = (unsigned char)R(3); v[2] = R(6) ? 0 : 0x80; }
    if (inRequest && v.size() >= 4 && R(2)) v[3] = (unsigned char)R(4);
    return v;
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sRefs.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sNextHandle = 0;
    sPingOps.clear();
    sRecords.clear();
    sOpGames.clear();
    if (inRust) {
        sRP = ee_won_logic_new(EeWonLogicKind::Ping, (void*)kPing, Host);
        sRA = ee_won_logic_new(EeWonLogicKind::Account, (void*)kAcct, Host);
        sRI = ee_won_logic_new(EeWonLogicKind::Init, (void*)kInit, Host);
    } else {
        sOP = new OrigPing();
        sOP->mNextRequestId = 0;
        sOA = new OrigAcct();
        sOA->mNeedTOU = false;
        sOI = new OrigInit();
        sOI->mErrors = sOI->mIgnoredErrors = 0;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        sOps = 0;
        sMsg = s.mBytes;
        sEventName = s.mName;
        Line("call %lld %lld %lld %lld %lld", s.mWhat, s.mCall, s.a, s.b, s.c);
        sTrace.back() += " '" + Hex(s.mBytes.empty() ? NULL : &s.mBytes[0], s.mBytes.size()) + "' [" +
                         std::string(s.mName.begin(), s.mName.end()) + "]";
        long long r = 0;
        intptr_t h = s.mWhat == kLogic || sPingOps.empty() ? 0 : sPingOps[(size_t)s.a % sPingOps.size()];
        switch (s.mWhat) {
        case kNewOp: // a ping op of any request id (the logic's own count from 0)
            h = 0xA000 + 0x10 * sNextHandle++;
            Line("op new %lld", h);
            sPingOps.push_back(h);
            sOpGames[h] = s.c;
            sRecords[h] = inRust ? (void*)ee_won_pingop_new((void*)h, Host, (uint16_t)s.a, (uint8_t)s.b) : (void*)NewOrigPingOp(s.a, s.b);
            break;
        case kLogic: {
            intptr_t a = s.a;
            if (s.mCall == (int)Call::PingDone) {
                if (sPingOps.empty()) { Line("skip"); continue; }
                a = sPingOps[(size_t)s.a % sPingOps.size()];
            }
            r = LogicCall((Call)s.mCall, a, s.b, s.c);
            break;
        }
        case kRunHook:
            if (h == 0) { Line("skip"); continue; }
            Line("op hook %lld", h);
            if (inRust) r = ee_won_pingop_call((EeWonPingOp*)sRecords[h], EeWonPingOpCall::RunHook, sOpGames[h]);
            else OrigRunHook((OrigPingOp*)sRecords[h], h, sOpGames[h]);
            break;
        case kCheck: {
            if (h == 0) { Line("skip"); continue; }
            Line("op check %lld", h);
            Reader aReader(s.mBytes.empty() ? NULL : &s.mBytes[0], s.mBytes.size());
            if (inRust) r = ee_won_pingop_call((EeWonPingOp*)sRecords[h], EeWonPingOpCall::CheckResponse, (intptr_t)&aReader.s);
            else r = OrigCheckResponse((OrigPingOp*)sRecords[h], h, aReader);
            break;
        }
        }
        Line("ret %lld", r);
    }
    Line("fields %lld %lld %lld %lld", (long long)LogicCall(Call::PingNextId), (long long)LogicCall(Call::PingPending),
         (long long)LogicCall(Call::AcctNeedTOU), (long long)LogicCall(Call::AcctLastNameLength));
    Line("fields %lld %lld", (long long)LogicCall(Call::InitField, 0), (long long)LogicCall(Call::InitField, 1));
    for (size_t i = 0; i < sPingOps.size(); i++) {
        intptr_t h = sPingOps[i];
        if (inRust) {
            EeWonPingOp* x = (EeWonPingOp*)sRecords[h];
            Line("pingop %lld %lld %lld %lld %lld", ee_won_pingop_call(x, EeWonPingOpCall::Field, 0),
                 ee_won_pingop_call(x, EeWonPingOpCall::Field, 1), ee_won_pingop_call(x, EeWonPingOpCall::Field, 2),
                 ee_won_pingop_call(x, EeWonPingOpCall::Field, 3), ee_won_pingop_call(x, EeWonPingOpCall::Field, 4));
            ee_won_pingop_free(x);
        } else {
            OrigPingOp* x = (OrigPingOp*)sRecords[h];
            Line("pingop %lld %lld %lld %lld %lld", x->mRequestId, x->mPing, x->mReplyFlags, x->mRetries,
                 x->mHasReply ? (long long)x->mReply.size() : -1LL);
            delete x;
        }
    }
    Line("free");
    if (inRust) {
        ee_won_logic_free(sRP);
        ee_won_logic_free(sRA);
        ee_won_logic_free(sRI);
    } else {
        delete sOP; // the queued games' SmartPtrs
        delete sOA;
        delete sOI;
    }
    for (std::map<intptr_t, int>::iterator it = sRefs.begin(); it != sRefs.end(); ++it)
        if (it->second != 0) Line("refs %lld %lld", it->first, it->second);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 60000;
    static const int kAcctEvents[] = {100010, 100018, 100019, 100035, 100035, 100040, 100060, 5};
    static const int kInitEvents[] = {100045, 100025, 7};
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        int aCount = 1 + (int)R(16);
        for (int i = 0; i < aCount; i++) {
            Step s = {kLogic, (int)Call::PingNumToDo, 0, 0, 0, std::vector<unsigned char>(), std::wstring()};
            switch (R(30)) {
            case 0: s.mCall = (int)Call::PingInit; break;
            case 1: s.mCall = (int)Call::PingEvent; s.a = 0xE00; s.b = R(4) ? 100037 : 100036; break;
            case 2: case 3: s.mCall = (int)Call::PingRecv; s.a = 0x900; s.mBytes = RandomBytes(true); break;
            case 4: case 5: s.mCall = (int)Call::PingDone; s.a = R(8); break;
            case 6: case 7: case 8: s.mCall = (int)Call::PingGame; s.a = R(6) ? Game() : 0; s.b = R(3) == 0; s.c = R(4) == 0; break;
            case 9: s.mCall = (int)Call::PingKill; break;
            case 10: if (R(2)) { s.mWhat = kRunHook; s.a = R(8); } else { s.mWhat = kNewOp; s.a = (R(2) ? 0x8000 : 0) | R(3); s.b = R(4); s.c = Game(); } break;
            case 11: case 12: s.mWhat = kCheck; s.a = R(8); s.mBytes = RandomBytes(false); break;
            case 13: s.mCall = (int)Call::AcctControl; s.a = 1 + R(4); break;
            case 14: case 15: case 16:
                s.mCall = (int)Call::AcctEvent; s.a = 0xE10; s.b = kAcctEvents[R(8)]; s.mName = kNames[R(7)]; break;
            case 17: s.mCall = (int)Call::InitInit; break;
            case 18: s.mCall = (int)Call::InitStart; s.a = R(2); break;
            case 19: s.mCall = R(2) ? (int)Call::InitKillHook : (int)Call::InitKillFirewall; break;
            case 20: s.mCall = (int)Call::InitControl; s.a = 1 + R(4); break;
            case 21: s.mCall = (int)Call::InitEvent; s.a = kInitEvents[R(3)]; break;
            case 22: s.mCall = (int)Call::InitDns; s.a = 0x910; break;
            case 23: s.mCall = (int)Call::InitDocument; s.a = 0x920; break;
            case 24: s.mCall = (int)Call::InitPromotionImage; s.a = 0x930; break;
            case 25: case 26: s.mCall = (int)Call::InitDirectory; s.a = 0x940; break;
            case 27: s.mCall = (int)Call::InitVersion; s.a = 0x950; break;
            case 28: s.mCall = (int)Call::InitAccept; s.a = 0x960; break;
            default: s.mCall = R(2) ? (int)Call::InitFirewall : (int)Call::PingNumToDo; s.a = 0x970; break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            size_t aSecond = a[i].find(' ', aSpace + 1);
            std::string aKey;
            if (aHead == "ret" || aHead == "free" || aHead == "refs" || aHead == "fields" || aHead == "pingop" || aHead == "skip")
                aKey = aHead;
            else if (aHead == "call")
                aKey = a[i].substr(0, a[i].find(' ', aSecond + 1));
            else
                aKey = a[i].substr(0, aSecond);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d %u\n", n, aSeed);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_logics_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
