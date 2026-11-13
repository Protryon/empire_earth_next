// WONRouting's SmartRoutingConnection over 127.0.0.1, one line per event, compared with
// rust/crates/ee_won/src/golden_smartroute.txt (recorded over the C++ class before its move to Rust): a routing
// server stand-in on a loopback socket sends the notifications and replies the connection's ops read, the
// connection's model (clients, groups, members, the ignore list, the handled records, the chat command state) is
// dumped after each step, the snapshot calls and iterators are logged, and every request the connection sends
// (register, lists, joins, the chat commands' chats, flags, mutes, bans, invitations) is logged as the server
// receives it. `--print` prints the lines instead. The exit code is the number of failures.
//
// PeerAuthOp is a Rust op on the op base whose real run needs certificates: as won_servconn_test, this driver
// defines PeerAuthOp's constructor and RustServerOp's members itself (ServerOp.cpp is not linked).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <typeinfo>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static std::vector<std::string> sLines;
static PlatMutex* sLinesLock = PlatMutexCreate();
static void Log(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    sLines.push_back(inLine);
    static const bool sTrace = getenv("WON_SMARTROUTE_TRACE") != NULL;
    if (sTrace)
        fprintf(stderr, "%s\n", inLine.c_str());
    PlatMutexUnlock(sLinesLock);
}
static std::string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string X(unsigned long inValue)
{
    char aText[16];
    snprintf(aText, sizeof(aText), "%lx", inValue);
    return aText;
}
static std::string HexBytes(const char* inData, unsigned long inLength)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N(inLength) + ":";
    for (unsigned long i = 0; i < inLength && i < 64; i++) {
        unsigned char c = static_cast<unsigned char>(inData[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static std::string Hex(const ByteBuffer* inBuffer)
{
    if (inBuffer == NULL)
        return "(null)";
    return HexBytes(inBuffer->data(), inBuffer->length());
}
static ByteBufferPtr Bytes(const void* inData, unsigned long inLength) { return new ByteBuffer(inData, inLength, false); }
static std::string Esc(const std::wstring& inText)
{
    std::string s = "\"";
    for (size_t i = 0; i < inText.length(); i++) {
        unsigned long c = (unsigned long)inText[i];
        if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
            s += (char)c;
        else
            s += "\\u" + X(c);
    }
    return s + "\"";
}

// ---- the stand-in peer authentication: PeerAuthOp's constructor and RustServerOp's members

static std::map<const void*, IPAddr> sPeerAddr;
static int sListenPort;

namespace WONAPI {
ServerOp::ServerOp()
{
    this->mAuthType = AUTH_TYPE_NONE;
    this->mLengthFieldSize = 4;
    this->mClientKeySize = 8;
    this->mUseClientKey = false;
    this->mMaxConnectTime = 60000;
    this->mMaxSendTime = 60000;
    this->mMaxRecvTime = 60000;
}
RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp), mFile(NULL) {}
RustServerOp::~RustServerOp()
{
    PlatMutexLock(sLinesLock);
    sPeerAddr.erase(this);
    PlatMutexUnlock(sLinesLock);
}
void RustServerOp::RunHook()
{
    IPAddr anAddr = sPeerAddr[this];
    this->Finish(this->mSocket->Connect(anAddr, 5000));
}
void RustServerOp::CleanupHook() { AsyncOpWithTracker::CleanupHook(); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam) { return AsyncOpWithTracker::CallbackHook(inOp, inParam); }
PeerAuthOp::PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket,
                       bool)
    : RustServerOp(NULL)
{
    this->mSocket = inSocket;
    this->mAuthContext = inContext;
    this->mAuthType = inType;
    PlatMutexLock(sLinesLock);
    sPeerAddr[this] = inAddr;
    PlatMutexUnlock(sLinesLock);
}
}

// ---- completions and the pump

static WONAPICore* sCore;

static SmartRoutingConnection* sConn;
static std::string HandlerLine();

class RouteDone : public CompletionBase<AsyncOpPtr> {
public:
    RouteDone(const char* inName, bool inQuiet = false) : mName(inName), mCount(0), mQuiet(inQuiet) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        if (this->mQuiet)
            return;
        RoutingOp* anOp = dynamic_cast<RoutingOp*>(inOp.get());
        if (anOp == NULL) {
            Log(this->mName + " completed " + St(inOp->GetStatus()));
            return;
        }
        Log(this->mName + " completed type " + N(anOp->GetType()) + " " + St(anOp->GetStatus()));
        if (this->mName == "default" && sConn != NULL)
            Log(HandlerLine());
    }
    std::string mName;
    int mCount;
    bool mQuiet;
};
static void PumpRoute(RouteDone* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}

// ---- the routing server stand-in

// a message body: little-endian numbers, WON's length-prefixed strings
struct W {
    std::string b;
    W& B(int v) { b += (char)(v & 0xff); return *this; }
    W& S(int v) { B(v); B(v >> 8); return *this; }
    W& L(unsigned long v) { S((int)(v & 0xffff)); S((int)(v >> 16)); return *this; }
    // a wide string of ASCII text, 2-byte (or 1-byte) unit count
    W& W2(const char* s) { size_t n = strlen(s); S((int)n); for (size_t i = 0; i < n; i++) S((unsigned char)s[i]); return *this; }
    W& W1(const char* s) { size_t n = strlen(s); B((int)n); for (size_t i = 0; i < n; i++) S((unsigned char)s[i]); return *this; }
    W& N1(const char* s) { size_t n = strlen(s); B((int)n); b += s; return *this; }
    W& Buf2(const std::string& d) { S((int)d.size()); b += d; return *this; }
    // a sized entry: its 2-byte length, then its bytes
    W& Entry(const W& e) { S((int)e.b.size()); b += e.b; return *this; }
};
// a routing server message: header 3 7 type
static W Msg(int inType) { W w; w.B(3).B(7).B(inType); return w; }

struct Server {
    SmartPtr<BlockingSocket> mListener;
    SmartPtr<AsyncSocket> mPeer;
    void Open()
    {
        this->mListener = new BlockingSocket(0);
        this->mListener->Bind(IPAddr("127.0.0.1", 0), true);
        this->mListener->Listen(5);
        sListenPort = this->mListener->GetLocalPort();
    }
    IPAddr Addr() const { return IPAddr("127.0.0.1", (unsigned short)sListenPort); }
    void Accept()
    {
        this->mListener->WaitForAccept(5000);
        Log("server accept " + St(this->mListener->Accept(this->mPeer)));
        this->Peer()->SetLengthFieldSize(2);
    }
    BlockingSocket* Peer() { return static_cast<BlockingSocket*>(this->mPeer.get()); }
    // waits for one request from the connection and logs it
    void Expect(const char* inName)
    {
        SmartPtr<RouteDone> aDone = new RouteDone(inName, true);
        SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(this->Peer());
        aRecv->SetCompletion(aDone);
        aRecv->RunAsync(5000);
        PumpRoute(aDone, 1);
        if (aRecv->GetStatus() == WS_Success)
            Log(std::string("server got ") + inName + " " + Hex(aRecv->GetMsg()));
        else
            Log(std::string("server got ") + inName + " " + St(aRecv->GetStatus()));
    }
    void Send(const char* inName, const W& inMsg)
    {
        std::string aMsg(2, '\0');
        size_t aLength = 2 + inMsg.b.size();
        aMsg[0] = (char)(aLength & 0xff);
        aMsg[1] = (char)((aLength >> 8) & 0xff);
        aMsg += inMsg.b;
        WONStatus aStatus = this->Peer()->SendMsg(Bytes(aMsg.data(), (unsigned long)aMsg.size()), 5000);
        if (aStatus != WS_Success)
            Log(std::string("server send ") + inName + " " + St(aStatus));
    }
};

// ---- the model's dump (the original's members, or the Rust object's dump)

typedef SmartRoutingConnection Smart;

#ifndef EE_WON_SMART_ROUTE_RUST
static std::string ClientRef(Smart* c, RoutingClientInfo* inClient)
{
    if (inClient == NULL)
        return "-";
    RoutingClientMap::iterator i = c->mClientMap.find(inClient->mClientId);
    bool shared = i != c->mClientMap.end() && i->second.get() == inClient;
    return N(inClient->mClientId) + (shared ? "s" : "o");
}
static std::string GroupRef(Smart* c, RoutingGroupInfo* inGroup)
{
    if (inGroup == NULL)
        return "-";
    RoutingGroupMap::iterator i = c->mGroupMap.find(inGroup->mId);
    bool shared = i != c->mGroupMap.end() && i->second.get() == inGroup;
    return N(inGroup->mId) + (shared ? "s" : "o");
}
static std::string MemberRef(RoutingMemberInfo* inMember)
{
    return inMember == NULL ? std::string("-") : N(inMember->mClientId);
}
static void DumpLines(Smart* c, bool inAsync, std::vector<std::string>& out)
{
    out.push_back("model me=" + ClientRef(c, c->mMyClient.get()) + " id=" + N(c->mClientId)
                  + (inAsync ? " async=" + X(c->mAsyncFlags) : std::string()) + " default=" + N(c->mDefaultGroupId)
                  + " tol=" + N(c->mMatchTolerance) + " wide=" + N(c->mServerWideCommands) + " colon="
                  + N(c->mDoColonEmote));
    for (RoutingClientMap::iterator i = c->mClientMap.begin(); i != c->mClientMap.end(); ++i) {
        SmartRoutingClientInfo* s = static_cast<SmartRoutingClientInfo*>(i->second.get());
        out.push_back("client " + N(i->first) + " id=" + N(s->mClientId) + " " + Esc(s->mName) + " f=" + X(s->mFlags)
                      + " cd=" + N(s->mAddress) + " g=" + N(s->mNumGroups) + " ign=" + N(s->mIgnored));
    }
    for (RoutingGroupMap::iterator i = c->mGroupMap.begin(); i != c->mGroupMap.end(); ++i) {
        RoutingGroupInfo* g = i->second.get();
        out.push_back("group " + N(i->first) + " id=" + N(g->mId) + " " + Esc(g->mName) + " cap=" + N(g->mCaptainId)
                      + " maxc=" + N(g->mMaxClients) + " f=" + X(g->mFlags) + " af=" + X(g->mAsyncFlags) + " obs="
                      + N(g->mObserverCount) + " mp=" + N(g->mMaxPlayers));
        for (RoutingMemberMap::iterator m = g->mMembers.begin(); m != g->mMembers.end(); ++m)
            out.push_back(" member " + N(m->first) + " id=" + N(m->second->mClientId) + " f="
                          + N(m->second->mFlags) + " c=" + ClientRef(c, m->second->mClientInfo.get()));
    }
    out.push_back("handler c=" + ClientRef(c, c->mHandlerClient.get()) + " g=" + GroupRef(c, c->mHandlerGroup.get())
                  + " m=" + MemberRef(c->mHandlerMember.get()));
    std::string anIgnore = "ignore";
    for (Smart::IgnoreSet::iterator i = c->mIgnoreSet.begin(); i != c->mIgnoreSet.end(); ++i)
        anIgnore += " " + Esc(*i);
    out.push_back(anIgnore);
}
static void DumpCommands(Smart* c, std::vector<std::string>& out)
{
    std::string s = "commands";
    for (Smart::CommandStringMap::iterator i = c->mCommandStringMap.begin(); i != c->mCommandStringMap.end(); ++i)
        s += " " + Esc(i->first) + "=" + N(i->second);
    out.push_back(s);
}
static std::string InputState(Smart* c, bool inCommand)
{
    return "input err=" + N(c->mLastInputError) + (inCommand ? " cmd=" + N(c->mLastInputCommand) : std::string())
        + " target m=" + MemberRef(c->mLastTargetMember.get()) + " c=" + ClientRef(c, c->mLastTargetClient.get())
        + " name=" + Esc(c->mLastMatchName);
}
#else
// the Rust object's dump (ee_won smartroute.rs), the same lines
static void DumpSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::string RustDump(Smart* c, int inWhat, bool inFlag)
{
    std::string aText;
    ee_won_smart_dump(c->mModel, inWhat, inFlag, DumpSink, &aText);
    return aText;
}
static void DumpLines(Smart* c, bool inAsync, std::vector<std::string>& out)
{
    std::string aText = RustDump(c, 0, inAsync);
    size_t aStart = 0;
    while (true) {
        size_t anEnd = aText.find((char)10, aStart);
        out.push_back(aText.substr(aStart, anEnd == std::string::npos ? std::string::npos : anEnd - aStart));
        if (anEnd == std::string::npos)
            break;
        aStart = anEnd + 1;
    }
}
static void DumpCommands(Smart* c, std::vector<std::string>& out) { out.push_back(RustDump(c, 1, false)); }
static std::string InputState(Smart* c, bool inCommand) { return RustDump(c, 2, inCommand); }
#endif

static std::string HandlerLine()
{
    std::vector<std::string> aLines;
    PlatMutexLock(sConn->GetDataLock());
    DumpLines(sConn, true, aLines);
    PlatMutexUnlock(sConn->GetDataLock());
    for (size_t i = 0; i < aLines.size(); i++)
        if (aLines[i].compare(0, 8, "handler ") == 0)
            return aLines[i];
    return "handler ?";
}

static void Dump(Smart* c, const char* inWhy, bool inAsync = true)
{
    Log(std::string("-- dump ") + inWhy);
    std::vector<std::string> aLines;
    PlatMutexLock(c->GetDataLock());
    DumpLines(c, inAsync, aLines);
    PlatMutexUnlock(c->GetDataLock());
    for (size_t i = 0; i < aLines.size(); i++)
        Log(aLines[i]);
}

// ---- the snapshot calls

static std::string Show(const Smart::ClientInfo& i)
{
    return "client " + N(i.mId) + " " + Esc(i.mName) + " f=" + X(i.mFlags) + " cd=" + N(i.mCDKeyId);
}
static std::string Show(const Smart::GroupInfo& i)
{
    return "group " + N(i.mId) + " " + Esc(i.mName) + " cap=" + N(i.mCaptainId) + " mp=" + N(i.mMaxPlayers) + " f="
        + X(i.mFlags) + " af=" + X(i.mAsyncFlags) + " obs=" + N(i.mObserverCount);
}
static std::string Show(const Smart::MemberInfo& i)
{
    return "member " + N(i.mClientId) + " " + Esc(i.mName) + " cf=" + X(i.mClientFlags) + " mf=" + N(i.mMemberFlags);
}
static void Snapshots(Smart* c, const char* inWhy)
{
    Log(std::string("-- snapshots ") + inWhy);
    for (int i = 0; i < 2; i++) {
        unsigned short anId = i == 0 ? 11 : 99;
        Smart::ClientInfo a;
        bool ok = c->GetClient(anId, a);
        Log("GetClient " + N(anId) + " " + N(ok) + " " + Show(a));
    }
    {
        Smart::GroupInfo g;
        bool ok = c->GetGroup(100, g);
        Log("GetGroup 100 " + N(ok) + " " + Show(g));
        Smart::GroupInfo h;
        ok = c->GetGroup(555, h);
        Log("GetGroup 555 " + N(ok) + " " + Show(h));
        Smart::MemberInfo m;
        ok = c->GetMember(100, 11, m);
        Log("GetMember 100 11 " + N(ok) + " " + Show(m));
        Smart::MemberInfo n;
        ok = c->GetMember(100, 77, n);
        Log("GetMember 100 77 " + N(ok) + " " + Show(n));
    }
    c->StartClientItr();
    Smart::ClientInfo aClient;
    while (c->GetNextClient(aClient))
        Log("next " + Show(aClient));
    c->StartGroupItr();
    Smart::GroupInfo aGroup;
    while (c->GetNextGroup(aGroup)) {
        Log("next " + Show(aGroup));
        bool started = c->StartMemberItr(aGroup.mId);
        Smart::MemberInfo aMember;
        while (started && c->GetNextMember(aMember))
            Log(" next " + Show(aMember));
    }
    Log("StartMemberItr 555 " + N(c->StartMemberItr(555)));
    Smart::MemberInfo aMember;
    Log("GetNextMember after " + N(c->GetNextMember(aMember)));
}

// ---- the scenario

static void PumpFor(unsigned inMs)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + inMs;
    while (PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
}

static SmartPtr<RouteDone> sAll;    // the default completion: every notification that completes
static SmartPtr<RouteDone> sSync;   // the shutdown-aborted notification that ends each step

// a notification, then the sync message: once the sync completes the notification was handled
static void Note(Server& s, Smart*, const char* inName, const W& inMsg)
{
    Log(std::string("note ") + inName);
    s.Send(inName, inMsg);
    s.Send("sync", Msg(58));
    PumpRoute(sSync, sSync->mCount + 1);
}

// a line of chat input; a request is expected when it succeeds (but for an ignore toggle)
static void Input(Server& s, Smart* c, const wchar_t* inText, int inGroup = -1)
{
    bool ok = c->HandleInput(inText, inGroup);
    std::string aText;
    for (const wchar_t* p = inText; *p; p++)
        aText += (char)*p;
    Log("input \"" + aText + "\" group " + N(inGroup) + " -> " + N(ok));
    Log(InputState(c, true));
    Smart::MemberInfo m;
    Smart::ClientInfo cl;
    bool hasM = c->GetLastTargetMember(m);
    bool hasC = c->GetLastTargetClient(cl);
    Log("last target " + N(hasM) + " " + Show(m) + " / " + N(hasC) + " " + Show(cl));
    bool anIgnore = wcsncmp(inText, L"/i ", 3) == 0 || wcsncmp(inText, L"/ignore", 7) == 0;
    if (ok && !anIgnore)
        s.Expect(aText.c_str());
}

static std::string sIgnorePath;
static void LogIgnoreFile(const char* inWhy)
{
    FILE* f = fopen(sIgnorePath.c_str(), "rb");
    if (f == NULL) {
        Log(std::string("ignore file ") + inWhy + " absent");
        return;
    }
    char aData[512];
    size_t n = fread(aData, 1, sizeof(aData), f);
    fclose(f);
    Log(std::string("ignore file ") + inWhy + " " + HexBytes(aData, (unsigned long)n));
}

static W ClientEntry(W& w, int inId, const char* inName, unsigned long inFlags)
{
    w.S(inId).W2(inName).L(inFlags);
    return w;
}

static void Scenario()
{
    Server s;
    s.Open();
    // an ignore file with two names (UTF-16 LE, NUL-separated)
    sIgnorePath = "smart_ignore.bin";
    {
        FILE* f = fopen(sIgnorePath.c_str(), "wb");
        const char kNames[] = "b\0o\0b\0\0\0C\0A\0R\0O\0L\0\0\0";
        fwrite(kNames, 1, sizeof(kNames) - 1, f);
        fclose(f);
    }
    SmartPtr<Smart> c = new Smart;
    sConn = c;
    {
        std::vector<std::string> aLines;
        DumpCommands(c, aLines);
        for (size_t i = 0; i < aLines.size(); i++)
            Log(aLines[i]);
    }
    c->SetProductName(sIgnorePath);
    c->SetKeepAlivePeriod(0);
    sAll = new RouteDone("default");
    sSync = new RouteDone("sync", true);
    c->SetDefaultOpCompletion(sAll);
    c->SetOpCompletion(49, sSync);
    Dump(c, "fresh", false);
    Log("connect " + St(c->ConnectBlock(s.Addr(), 5000)));
    s.Accept();

    // not logged in yet
    {
        bool ok = c->HandleInput(L"hello", -1);
        Log("input before login -> " + N(ok) + " " + InputState(c, false));
    }

    // register with the client list (flag 4): the ignore file is read, bob ignored
    SmartPtr<RouteDone> aReg = new RouteDone("register");
    c->Register(L"me", L"pw", 0, 2, 4, 0x10, aReg);
    s.Expect("register");
    {
        W w = Msg(51);
        w.S(0).S(10).W2("me").L(77);
        s.Send("register reply", w);
        W l = Msg(23);
        l.S(0).S(4);
        ClientEntry(l, 10, "me", 1);
        ClientEntry(l, 11, "Alice", 0);
        ClientEntry(l, 12, "bob", 4);
        ClientEntry(l, 13, "Bobby", 0);
        s.Send("client list", l);
    }
    PumpRoute(aReg, 1);
    Dump(c, "registered");

    // groups created
    {
        W w = Msg(32);
        w.S(100).W2("Lobby").S(50).L(0x800).L(0);
        Note(s, c, "group created 100", w);
        W v = Msg(32);
        v.S(101).W2("Game").S(8).L(0).L(1);
        Note(s, c, "group created 101", v);
    }
    Dump(c, "groups created");

    // the group list with members: 100 updated, 101 gone, 102 new (with an unknown client 14)
    SmartPtr<RouteDone> aList = new RouteDone("group list");
    c->GetGroupList(0xff, aList);
    s.Expect("group list");
    {
        W w = Msg(25);
        w.S(0).S(0xff).S(2);
        W g1;
        g1.S(100).W2("Lobby").S(0xffff).S(50).L(0x800).L(0x10).S(3).S(2).S(10).B(1).S(11).B(0);
        w.Entry(g1);
        W g2;
        g2.S(102).W2("Other").S(12).S(4).L(0).L(0).S(0).S(2).S(12).B(0).S(14).B(2);
        w.Entry(g2);
        s.Send("group list reply", w);
    }
    PumpRoute(aList, 1);
    Dump(c, "group list");

    // a client joins the server (with a CD key id in its extended data), then groups
    {
        W w = Msg(8);
        w.S(15).W2("Dave").L(0).B(2).B(2).B(4).L(0x1234).B(9).B(1).B(0x55);
        Note(s, c, "client joined server 15", w);
        W j = Msg(7);
        j.B(3).S(100).S(15).B(1).W2("Dave2").L(9);
        Note(s, c, "client 15 joined group 100", j);
        W k = Msg(7);
        k.B(0).S(999).S(15).B(1);
        Note(s, c, "client 15 joined unknown group 999", k);
        W m = Msg(7);
        m.B(0).S(100).S(16).B(0);
        Note(s, c, "unknown client 16 joined group 100", m);
        W n = Msg(7);
        n.B(1).S(102).S(11).B(4).W2("CAROL");
        Note(s, c, "client 11 joined group 102 as CAROL", n);
        W again = Msg(7);
        again.B(2).S(102).S(11).B(5).L(0x77);
        Note(s, c, "client 11 joined group 102 again", again);
    }
    Dump(c, "joins");

    // field notifications
    {
        W w = Msg(6);
        w.S(11).L(0x20);
        Note(s, c, "client flags 11", w);
        W w2 = Msg(6);
        w2.S(98).L(0x20);
        Note(s, c, "client flags unknown 98", w2);
        W cap = Msg(31);
        cap.S(100).S(10);
        Note(s, c, "captain 100", cap);
        W mp = Msg(37);
        mp.S(100).S(64);
        Note(s, c, "max players 100", mp);
        W gf = Msg(34);
        gf.S(100).L(0x801).L(5);
        Note(s, c, "group flags 100", gf);
        W gn = Msg(38);
        gn.S(100).W2("Lobby2");
        Note(s, c, "group name 100", gn);
        W ob = Msg(39);
        ob.S(100).S(7);
        Note(s, c, "observers 100", ob);
        W mf = Msg(44);
        mf.S(100).S(11).B(3);
        Note(s, c, "member flags 100 11", mf);
        W mf2 = Msg(44);
        mf2.S(100).S(55).B(3);
        Note(s, c, "member flags 100 55 (no member)", mf2);
        W bad = Msg(32);
        bad.S(555).W2("Gone").S(1).L(0).L(0);
        Note(s, c, "group created 555", bad);
        W del = Msg(33);
        del.S(555);
        Note(s, c, "group deleted 555", del);
        W del2 = Msg(33);
        del2.S(556);
        Note(s, c, "group deleted 556 (none)", del2);
    }
    Dump(c, "fields");

    // chat, invitations and join attempts: an ignored sender's completion is swallowed
    {
        W w = Msg(47);
        w.S(12).S(0).W2("from bob").S(1).S(10);
        Note(s, c, "chat from ignored bob", w);
        W w2 = Msg(47);
        w2.S(11).S(4).W2("from alice").S(1).S(10);
        Note(s, c, "chat from alice", w2);
        W w3 = Msg(47);
        w3.S(97).S(0).W2("from nobody").S(0);
        Note(s, c, "chat from unknown 97", w3);
        W inv = Msg(35);
        inv.S(100).S(12).B(1).W2("come");
        Note(s, c, "invitation from bob", inv);
        W inv2 = Msg(35);
        inv2.S(100).S(13).B(1).W2("come");
        Note(s, c, "invitation from Bobby", inv2);
        W att = Msg(36);
        att.S(12).S(100).W2("let me in").B(0);
        Note(s, c, "join attempt from bob", att);
        W att2 = Msg(36);
        att2.S(11).S(100).W2("let me in").B(0);
        Note(s, c, "join attempt from alice", att2);
    }
    Log("default completions " + N(sAll->mCount));
    Snapshots(c, "after notifications");

    // the member iterator stepped past a leaving member; the client in no group is dropped (async flag 2)
    c->StartMemberItr(100);
    {
        Smart::MemberInfo m;
        {
            bool ok_ = c->GetNextMember(m);
            Log("member itr first " + N(ok_) + " " + Show(m));
        }
        W w = Msg(9);
        w.S(100).S(11);
        Note(s, c, "client 11 left group 100", w);
        {
            bool ok_ = c->GetNextMember(m);
            Log("member itr next " + N(ok_) + " " + Show(m));
        }
        W w2 = Msg(9);
        w2.S(100).S(16);
        Note(s, c, "client 16 left group 100", w2);
        W w3 = Msg(9);
        w3.S(100).S(88);
        Note(s, c, "client 88 left group 100 (no member)", w3);
        W w4 = Msg(9);
        w4.S(444).S(10);
        Note(s, c, "client 10 left unknown group 444", w4);
        {
            bool ok_ = c->GetNextMember(m);
            Log("member itr rest " + N(ok_) + " " + Show(m));
        }
    }
    // the client iterator stepped past a leaving client
    c->StartClientItr();
    {
        Smart::ClientInfo a;
        {
            bool ok_ = c->GetNextClient(a);
            Log("client itr first " + N(ok_) + " " + Show(a));
        }
        W w = Msg(10);
        w.S(11);
        Note(s, c, "client 11 left server", w);
        {
            bool ok_ = c->GetNextClient(a);
            Log("client itr next " + N(ok_) + " " + Show(a));
        }
        W w2 = Msg(10);
        w2.S(96);
        Note(s, c, "client 96 left server (none)", w2);
    }
    Dump(c, "leaves");

    // the members of group 102: 14 goes (and its client), 12 updated, 17 new
    SmartPtr<RouteDone> aMembers = new RouteDone("members");
    c->GetMembersOfGroup(102, aMembers);
    s.Expect("members 102");
    {
        W w = Msg(29);
        w.S(0).S(102).B(3).S(1).S(2);
        W m1;
        m1.S(12).B(1).W2("bob").L(4);
        w.Entry(m1);
        W m2;
        m2.S(17).B(0).W2("Eve").L(0x40);
        w.Entry(m2);
        s.Send("members reply", w);
    }
    PumpRoute(aMembers, 1);
    Dump(c, "members 102");

    // join group 100 with its member list
    SmartPtr<RouteDone> aJoin = new RouteDone("join");
    c->JoinGroup(100, L"", L"hi", 1, 0, aJoin);
    s.Expect("join 100");
    {
        W w = Msg(41);
        w.S(0).S(100).W2("");
        s.Send("join reply", w);
        W m = Msg(29);
        m.S(0).S(100).B(1).S(0).S(4);
        W e1;
        e1.S(10).B(1).W2("me");
        m.Entry(e1);
        W e2;
        e2.S(12).B(0).W2("");
        m.Entry(e2);
        W e3;
        e3.S(13).B(0).W2("Bobby");
        m.Entry(e3);
        W e4;
        e4.S(15).B(2).W2("Dave3");
        m.Entry(e4);
        s.Send("join members", m);
    }
    PumpRoute(aJoin, 1);
    Dump(c, "joined 100");
    // a failed join: nothing changes
    SmartPtr<RouteDone> aJoinBad = new RouteDone("join bad");
    c->JoinGroup(102, L"", L"", 0, 0, aJoinBad);
    s.Expect("join 102");
    {
        W w = Msg(41);
        w.S(0xfffe).S(102).W2("no");
        s.Send("join bad reply", w);
    }
    PumpRoute(aJoinBad, 1);
    Dump(c, "join 102 failed");

    // async flags
    SmartPtr<RouteDone> aFlags = new RouteDone("set flags");
    c->SetClientFlags(0, 0, 0x6, 0x4, aFlags);
    s.Expect("set flags");
    {
        W w = Msg(66);
        w.S(0);
        s.Send("status", w);
    }
    PumpRoute(aFlags, 1);
    SmartPtr<RouteDone> aFlagsBad = new RouteDone("set flags bad");
    c->SetClientFlags(0, 0, 0xff, 0xff, aFlagsBad);
    s.Expect("set flags bad");
    {
        W w = Msg(66);
        w.S(0xffff);
        s.Send("status bad", w);
    }
    PumpRoute(aFlagsBad, 1);
    Dump(c, "async flags");

    // CD key ids from _KeyId data objects
    SmartPtr<RouteDone> aRead = new RouteDone("read");
    c->ReadDataObject(0xffff, "_KeyId", 0, aRead);
    s.Expect("read _KeyId");
    {
        W w = Msg(49);
        w.S(0).S(3);
        W d1;
        d1.L(1).L(555);
        w.S(12).N1("_KeyId").W1("").Buf2(d1.b);
        W d2;
        d2.L(1);
        w.S(13).N1("_KeyId").W1("").Buf2(d2.b);
        w.S(99).N1("_KeyId").W1("").Buf2(d1.b);
        s.Send("read reply", w);
    }
    PumpRoute(aRead, 1);
    SmartPtr<RouteDone> aReadOther = new RouteDone("read other");
    c->ReadDataObject(0xffff, "Other", 0, aReadOther);
    s.Expect("read Other");
    {
        W w = Msg(49);
        W d1;
        d1.L(1).L(666);
        w.S(0).S(1).S(12).N1("Other").W1("").Buf2(d1.b);
        s.Send("read other reply", w);
    }
    PumpRoute(aReadOther, 1);
    {
        W d;
        d.L(2).L(4242);
        W w = Msg(14);
        w.S(15).N1("_KeyId").W1("x").Buf2(d.b);
        Note(s, c, "data created _KeyId 15", w);
        W w2 = Msg(14);
        w2.S(13).N1("Room").W1("x").Buf2(d.b);
        Note(s, c, "data created Room 13", w2);
        W d3;
        d3.L(2).L(31337);
        W w3 = Msg(16);
        w3.S(12).N1("_KeyId").W1("").S(0).B(0).Buf2(d3.b);
        Note(s, c, "data modified _KeyId 12", w3);
        W w4 = Msg(15);
        w4.S(12).N1("_KeyId").W1("");
        Note(s, c, "data deleted _KeyId 12", w4);
    }
    Dump(c, "cd keys");
    Snapshots(c, "after cd keys");

    // chat input in group 100 (the default)
    Input(s, c, L"hello");
    Input(s, c, L"   ");
    Input(s, c, L":waves");
    Input(s, c, L":  ");
    Input(s, c, L"/w Bobby hi there");
    Input(s, c, L"/W bobby  spaced");
    Input(s, c, L"/w Bob x");
    Input(s, c, L"/w Dave3");
    Input(s, c, L"/w nobody hi");
    Input(s, c, L"/w");
    Input(s, c, L"/w \"Dave3\" quoted");
    Input(s, c, L"/me dances");
    Input(s, c, L"/emote");
    Input(s, c, L"/bogus");
    Input(s, c, L"/");
    Input(s, c, L"/away");
    Input(s, c, L"/afk");
    Input(s, c, L"/mod");
    Input(s, c, L"/mute Bobby 10m spam");
    Input(s, c, L"/mute Bobby 2h");
    Input(s, c, L"/mute Bobby 1d \"too much\"");
    Input(s, c, L"/mute Bobby 30");
    Input(s, c, L"/mute Bobby 30s");
    Input(s, c, L"/mute Bobby 5x");
    Input(s, c, L"/mute Bobby 0s");
    Input(s, c, L"/mute Bobby INFINITE");
    Input(s, c, L"/smute Bobby INFINITE");
    Input(s, c, L"/ban Bobby");
    Input(s, c, L"/sban Bobby 3m");
    Input(s, c, L"/sban %12345 2h why");
    Input(s, c, L"/unmute Bobby");
    Input(s, c, L"/sunmute \"Bobby\"");
    Input(s, c, L"/unmute");
    Input(s, c, L"hello 102", 102);
    Input(s, c, L"/w bob hi", 102);
    // whole-server names
    c->SetServerWideCommands(true);
    Input(s, c, L"/w Eve hi");
    Input(s, c, L"/w B hi");
    // exact names only
    c->SetAllowPartialNameMatch(false);
    Input(s, c, L"/w Bobb hi");
    Input(s, c, L"/w BOBBY hi");
    c->SetAllowPartialNameMatch(true);
    c->SetServerWideCommands(false);
    // colon emotes off, a custom command
    c->SetDoColonEmote(false);
    Input(s, c, L":plain");
    c->SetDoColonEmote(true);
    c->AddCommandString(L"tell", Smart::InputCommand_Whisper);
    c->AddCommandString(L"inv", Smart::InputCommand_Invite);
    c->AddCommandString(L"uninv", Smart::InputCommand_Uninvite);
    Input(s, c, L"/TELL Bobby x");
    Input(s, c, L"/inv Bobby welcome");
    Input(s, c, L"/uninv Bobby");
    {
        std::vector<std::string> aLines;
        DumpCommands(c, aLines);
        for (size_t i = 0; i < aLines.size(); i++)
            Log(aLines[i]);
    }
    // ignore toggles: the ignore file is written
    LogIgnoreFile("before");
    Input(s, c, L"/i Bobby");
    LogIgnoreFile("ignored Bobby");
    Input(s, c, L"/ignore bob");
    LogIgnoreFile("unignored bob");
    Input(s, c, L"/i   \"Dave3\"  ");
    Input(s, c, L"/i Dave3 x");
    Input(s, c, L"/i Dave3 ");
    LogIgnoreFile("ignored Dave3");
    Dump(c, "ignores");
    // the find and ignore calls EEWonNet makes
    {
        const wchar_t* kNames[] = {L"bobby", L"bo", L"B", L"zed", L"Eve"};
        for (int i = 0; i < 5; i++) {
#ifndef EE_WON_SMART_ROUTE_RUST
            RoutingClientInfoPtr aFound;
            Smart::MatchResult r = c->FindClient(kNames[i], aFound);
            std::string aWho = aFound.get() == NULL ? std::string("-") : N(aFound->mClientId) + " " + Esc(aFound->mName);
#else
            Smart::ClientInfo aFound;
            bool aHas = false;
            Smart::MatchResult r = c->FindClient(kNames[i], aFound, aHas);
            std::string aWho = !aHas ? std::string("-") : N(aFound.mId) + " " + Esc(aFound.mName);
#endif
            Log("FindClient " + Esc(kNames[i]) + " " + N(r) + " " + aWho);
        }
#ifndef EE_WON_SMART_ROUTE_RUST
        RoutingClientInfoPtr aFound;
        c->FindClient(L"Eve", aFound);
        c->IgnoreClient(aFound.get(), true);
#else
        c->IgnoreClient(17, true);
#endif
        LogIgnoreFile("ignored Eve");
        Log("my client id " + N(c->GetMyClientId()));
    }

    // the close forgets this client; the model stays
    c->Close();
    PumpFor(100);
    Dump(c, "closed");
    {
        bool ok = c->HandleInput(L"hello", -1);
        Log("input after close -> " + N(ok) + " " + InputState(c, false));
    }
    s.Peer()->Close();

    // reconnect: register without the client list (only this client), then client list merges
    Log("reconnect " + St(c->ConnectBlock(s.Addr(), 5000)));
    s.Accept();
    c->SetProductName("");
    SmartPtr<RouteDone> aReg2 = new RouteDone("register 2");
    c->Register(L"me2", L"", 0, 0, 0, 0x30, aReg2);
    s.Expect("register 2");
    {
        W w = Msg(51);
        w.S(0).S(20).W2("me2").L(1);
        s.Send("register 2 reply", w);
    }
    PumpRoute(aReg2, 1);
    Dump(c, "registered 2");
    // a group with members over the new client map
    {
        W w = Msg(32);
        w.S(200).W2("R").S(4).L(0).L(0);
        Note(s, c, "group created 200", w);
        W j = Msg(7);
        j.B(1).S(200).S(20).B(0).W2("me2");
        Note(s, c, "me joined 200", j);
        W j2 = Msg(7);
        j2.B(1).S(200).S(21).B(0).W2("Zed");
        Note(s, c, "21 joined 200", j2);
    }
    SmartPtr<RouteDone> aCl = new RouteDone("client list");
    c->GetClientList(aCl);
    s.Expect("client list");
    {
        W l = Msg(23);
        l.S(0).S(3);
        ClientEntry(l, 5, "Five", 3);
        ClientEntry(l, 20, "me2renamed", 0x30);
        ClientEntry(l, 30, "bob", 0);
        s.Send("client list reply", l);
    }
    PumpRoute(aCl, 1);
    Dump(c, "client list");
    SmartPtr<RouteDone> aClEx = new RouteDone("client list ex");
    c->GetClientListEx(aClEx);
    s.Expect("client list ex");
    {
        W l = Msg(74);
        l.S(0).S(2);
        l.S(20).W2("me2").L(0x30).B(1).B(2).B(4).L(808);
        l.S(40).W2("Forty").L(0).B(0);
        s.Send("client list ex reply", l);
    }
    PumpRoute(aClEx, 1);
    Dump(c, "client list ex");
    // the member iterator's group removed by a group list: the iteration ends
    c->StartMemberItr(200);
    SmartPtr<RouteDone> aList2 = new RouteDone("group list 2");
    c->GetGroupList(0x01, aList2);
    s.Expect("group list 2");
    {
        W w = Msg(25);
        w.S(0).S(0x01).S(1);
        W g;
        g.S(201).W2("New");
        w.Entry(g);
        s.Send("group list 2 reply", w);
    }
    PumpRoute(aList2, 1);
    {
        Smart::MemberInfo m;
        Log("member itr after its group went " + N(c->GetNextMember(m)));
    }
    Dump(c, "group list 2");
    Snapshots(c, "end");
    // the commands cleared
    c->ClearCommandStrings();
    Input(s, c, L"/w Five x");
    c->Close();
    s.Peer()->Close();
    s.mListener->Close();
    PumpFor(100);
    sConn = NULL;
    c = NULL;
    remove(sIgnorePath.c_str());
    Log("smart routing done");
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_smartroute.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        Scenario();
        sAll = NULL;
        sSync = NULL;
        aCore.Pump(50);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_smartroute_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
