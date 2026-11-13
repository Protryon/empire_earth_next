// WONServer's ServerConnection and ServerConnectionConnectOp over 127.0.0.1, one line per event, compared with
// rust/crates/ee_won/src/golden_servconn.txt (recorded over the C++ classes before their move to Rust): the
// blocking and async connects (the peer authentication is a stand-in below: it connects the connection's socket
// for real, or stays pending for port 0), the connect completion, the message callback on the repeat receive, the
// keep-alive timer and its no-op messages, a period of 0, the close hook killing a pending connect and the timer,
// the kill hook dropping the connect completion, a second connect while one is pending, and ServerConnectionConnectOp
// (connected, killed). Then RoutingConnection (recorded over its C++ class before its move): the completions by op
// type and the default one, notifications, requests and their status replies in the list's order, a reply no op
// wants, a reply cut short, a derived server's message, the close killing a waiting op, the kill dropping the
// completions. `--print` prints the lines instead. The exit code is the number of failures.
//
// PeerAuthOp is a Rust op on the op base (ee_won peerop.rs) whose real run needs certificates and a WON server:
// this driver defines PeerAuthOp's constructor and RustServerOp's members itself (ServerOp.cpp is not linked), so
// the connection's own steps are what the lines show.
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

using namespace WONAPI;

static std::vector<std::string> sLines;
static PlatMutex* sLinesLock = PlatMutexCreate();
static void Log(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    sLines.push_back(inLine);
    static const bool sTrace = getenv("WON_SERVCONN_TRACE") != NULL;
    if (sTrace)
        fprintf(stderr, "%s\n", inLine.c_str());
    PlatMutexUnlock(sLinesLock);
}
static std::string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string Hex(const ByteBuffer* inBuffer)
{
    if (inBuffer == NULL)
        return "(null)";
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N(inBuffer->length()) + ":";
    for (unsigned long i = 0; i < inBuffer->length() && i < 48; i++) {
        unsigned char c = static_cast<unsigned char>(inBuffer->data()[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static ByteBufferPtr Bytes(const void* inData, unsigned long inLength) { return new ByteBuffer(inData, inLength, false); }
static std::string TypeName(const void* inObject, const std::type_info& inType)
{
    if (inObject == NULL)
        return "NULL";
    std::string s = inType.name();
    size_t aColon = s.rfind("::");
    if (aColon != std::string::npos)
        return s.substr(aColon + 2);
    size_t aSpace = s.rfind(' ');
    return aSpace == std::string::npos ? s : s.substr(aSpace + 1);
}
// a routing op's class name: the original's op classes by type (the ops are one RoutingOp over a Rust op since net
// round 18; the golden keeps the names it recorded)
static std::string RouteName(RoutingOp* inOp)
{
#ifdef EE_WON_ROUTE_OPS_RUST
    switch (inOp->GetType()) {
    case 5: return "RoutingCancelJoinGroupOp";
    case 10: return "RoutingClientLeftServerOp";
    case 20: return "RoutingGetClientListOp";
    case 35: return "RoutingLeaveGroupOp";
    case 49: return "RoutingServerShutdownAbortedOp";
    case 60: return "RoutingDerivedServerOp";
    }
#endif
    return TypeName(inOp, typeid(*inOp));
}
#define LOG2(head, first, rest) do { std::string l_ = std::string(head) + (first); l_ += std::string() + rest; Log(l_); } while (0)

// ---- the stand-in peer authentication: PeerAuthOp's constructor and RustServerOp's members

static std::map<const void*, IPAddr> sPeerAddr;
static int sPeerMade, sPeerFreed;
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
    sPeerFreed++;
    PlatMutexUnlock(sLinesLock);
}
// connects the op's socket (the connection) for real, or stays pending for an address of port 0
void RustServerOp::RunHook()
{
    IPAddr anAddr = sPeerAddr[this];
    bool aPending = anAddr.GetPort() == 0;
    Log(std::string("peerauth run ") + (aPending ? "pending" : (anAddr.GetPort() == sListenPort ? "listener" : "other"))
        + " type " + N(this->mAuthType) + " context " + (this->mAuthContext.get() != NULL ? "set" : "null")
        + " mode " + N(this->GetMode()) + " timeout " + N((long)this->GetTimeout()) + " socket "
        + (this->mSocket.get() != NULL ? "set" : "null"));
    if (aPending)
        return;
    WONStatus aStatus = this->mSocket->Connect(anAddr, 5000);
    Log("peerauth connect " + St(aStatus));
    this->Finish(aStatus);
}
void RustServerOp::CleanupHook() { AsyncOpWithTracker::CleanupHook(); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam) { return AsyncOpWithTracker::CallbackHook(inOp, inParam); }
PeerAuthOp::PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket,
                       bool inUseAuth2)
    : RustServerOp(NULL)
{
    this->mSocket = inSocket;
    this->mAuthContext = inContext;
    this->mAuthType = inType;
    PlatMutexLock(sLinesLock);
    sPeerAddr[this] = inAddr;
    sPeerMade++;
    PlatMutexUnlock(sLinesLock);
    Log(std::string("peerauth new auth2 ") + N(inUseAuth2));
}
}

// ---- the connection, its completions, the peer

// a server connection that logs its hooks and messages
class LogConn : public ServerConnection {
public:
    LogConn(const char* inName) : mName(inName), mMessages(0) {}
    virtual ~LogConn() { Log(this->mName + " destroyed"); }
    virtual void OpenHook() { Log(this->mName + " OpenHook"); ServerConnection::OpenHook(); }
    virtual void CloseHook() { Log(this->mName + " CloseHook"); ServerConnection::CloseHook(); }
    virtual void KillHook() { Log(this->mName + " KillHook"); ServerConnection::KillHook(); }
    virtual void MsgCallback(const ByteBuffer* inMsg)
    {
        this->mMessages++;
        Log(this->mName + " message " + Hex(inMsg));
    }
    std::string mName;
    int mMessages;
};

// a server connection with the default message callback (it does nothing)
class PlainConn : public ServerConnection {
public:
    virtual ~PlainConn() { Log("plain destroyed"); }
};

class Done : public CompletionBase<AsyncOpPtr> {
public:
    Done(const char* inName) : mName(inName), mCount(0) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        AsyncOp* anOp = inOp.get();
        Log(this->mName + " completed " + TypeName(anOp, typeid(*anOp)) + " " + St(anOp->GetStatus()) + " killed "
            + N(anOp->IsKilled()));
        RecvMsgOp* aMsg = dynamic_cast<RecvMsgOp*>(anOp);
        if (aMsg != NULL && aMsg->GetStatus() == WS_Success)
            Log(this->mName + " message " + Hex(aMsg->GetMsg()));
    }
    std::string mName;
    int mCount;
};

static WONAPICore* sCore;
static void PumpUntil(Done* inDone, int inCount, unsigned inMs = 5000)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + inMs;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}
static void PumpMessages(LogConn* inConn, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inConn->mMessages < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inConn->mMessages < inCount)
        Log(inConn->mName + " TIMED OUT waiting for message " + N(inCount));
}
static void PumpFor(unsigned inMs)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + inMs;
    while (PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
}

static std::string Show(const char* inName, AsyncSocket* inSocket)
{
    return std::string(inName) + " valid " + N(inSocket->IsValid()) + " open " + N(inSocket->IsOpen()) + " connected "
        + N(inSocket->IsConnected()) + " type " + N(inSocket->GetType()) + " lengthfield "
        + N(inSocket->GetLengthFieldSize());
}

// the server: a listener on 127.0.0.1 and the socket it accepted last
struct Server {
    SmartPtr<BlockingSocket> mListener;
    SmartPtr<AsyncSocket> mPeer;
    void Open()
    {
        this->mListener = new BlockingSocket(0);
        Log("listener bind " + St(this->mListener->Bind(IPAddr("127.0.0.1", 0), true)));
        Log("listener listen " + St(this->mListener->Listen(5)));
        sListenPort = this->mListener->GetLocalPort();
    }
    IPAddr Addr() const { return IPAddr("127.0.0.1", (unsigned short)sListenPort); }
    void Accept()
    {
        this->mListener->WaitForAccept(5000);
        Log("listener accept " + St(this->mListener->Accept(this->mPeer)));
    }
    BlockingSocket* Peer() { return static_cast<BlockingSocket*>(this->mPeer.get()); }
    // waits for one message from the connection, pumping meanwhile (the keep-alive timer completes on the pump)
    void Expect(const char* inName, unsigned inMs = 5000)
    {
        SmartPtr<Done> aDone = new Done(inName);
        SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(this->Peer());
        aRecv->SetCompletion(aDone);
        aRecv->RunAsync(inMs);
        PumpUntil(aDone, 1, inMs + 2000);
    }
    // a message as WON frames it: the 4-byte length (its own bytes included), then the text
    void Send(const char* inText)
    {
        std::string aMsg(4, ' ');
        unsigned long aLength = 4 + (unsigned long)strlen(inText);
        for (int i = 0; i < 4; i++)
            aMsg[i] = (char)((aLength >> (8 * i)) & 0xff);
        aMsg += inText;
        Log(std::string("peer send ") + inText + " "
            + St(this->Peer()->SendMsg(Bytes(aMsg.data(), (unsigned long)aMsg.size()), 5000)));
    }
};

// ---- the scenarios

static void BlockingConnect()
{
    Log("== ServerConnection, blocking");
    Server s;
    s.Open();
    SmartPtr<LogConn> c = new LogConn("conn");
    Log(Show("fresh", c));
    SmartPtr<Done> aConnect = new Done("connect");
    c->SetConnectCompletion(aConnect);
    c->SetAuth(NULL, AUTH_TYPE_PERSISTENT_NOCRYPT);
    c->SetKeepAlivePeriod(0);
    LOG2("connect block ", St(c->ConnectBlock(s.Addr(), 5000)), " completions " + N(aConnect->mCount));
    s.Accept();
    Log(Show("connected", c));
    s.Send("hello");
    PumpMessages(c, 1);
    s.Send("again");
    PumpMessages(c, 2);
    // no keep-alive at a period of 0: nothing arrives
    SmartPtr<Done> aNone = new Done("no keepalive");
    SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(s.Peer());
    aRecv->SetCompletion(aNone);
    aRecv->RunAsync(300);
    PumpUntil(aNone, 1);
    // the peer closes: the repeat receive fails, no message callback
    s.Peer()->Close();
    PumpFor(100);
    Log(Show("peer closed", c) + " messages " + N(c->mMessages));
    c->Close();
    Log(Show("closed", c));
    // a second blocking connect on the same connection, then the kill hook
    LOG2("connect block again ", St(c->ConnectBlock(s.Addr(), 5000)), " completions " + N(aConnect->mCount));
    s.Accept();
    s.Send("third");
    PumpMessages(c, 3);
    c->Kill();
    Log(Show("killed", c));
    // the kill dropped the connect completion: a connect completes nobody
    LOG2("connect after kill ", St(c->ConnectBlock(s.Addr(), 5000)), " completions " + N(aConnect->mCount));
    s.Accept();
    c->Close();
    s.Peer()->Close();
    s.mListener->Close();
    c = NULL;
    Log("blocking done");
}

static void AsyncConnect()
{
    Log("== ServerConnection, async and keep-alive");
    Server s;
    s.Open();
    SmartPtr<LogConn> c = new LogConn("aconn");
    SmartPtr<Done> aConnect = new Done("aconnect");
    c->SetConnectCompletion(aConnect);
    c->SetKeepAlivePeriod(1);
    PeerAuthOp* anOp = c->ConnectAsync(s.Addr(), 4000);
    Log(std::string("connect async op ") + (anOp != NULL ? "set" : "null") + " status " + St(anOp->GetStatus()));
    PumpUntil(aConnect, 1);
    s.Accept();
    // the keep-alive fires after a second: a no-op message (3 1 7 after the 4-byte length)
    s.Expect("keepalive 1", 3000);
    s.Expect("keepalive 2", 3000);
    s.Send("data");
    PumpMessages(c, 1);
    // a period of 0 from the next restart on: one more no-op, then none
    c->SetKeepAlivePeriod(0);
    s.Expect("keepalive 3", 3000);
    s.Expect("keepalive none", 1500);
    // close: the hooks, then a connect that stays pending and the close that kills it
    c->Close();
    Log(Show("aclosed", c));
    s.Peer()->Close();
    c->SetKeepAlivePeriod(1);
    anOp = c->ConnectAsync(IPAddr("127.0.0.1", 0), 4000);
    Log(std::string("pending op status ") + St(anOp->GetStatus()));
    PumpFor(50);
    LOG2("pending completions ", N(aConnect->mCount), "");
    c->Close();
    PumpUntil(aConnect, 2);
    // a second connect while one is pending: the first one is killed by the close the second does
    anOp = c->ConnectAsync(IPAddr("127.0.0.1", 0), 4000);
    PumpFor(20);
    anOp = c->ConnectAsync(s.Addr(), 4000);
    PumpUntil(aConnect, 4);
    s.Accept();
    s.Expect("keepalive 4", 3000);
    // the keep-alive stops at the close; the peer sees the connection end
    c->Close();
    s.Expect("after close", 1500);
    s.Peer()->Close();
    s.mListener->Close();
    c = NULL;
    Log("async done");
}

static void ConnectOps()
{
    Log("== ServerConnectionConnectOp");
    Server s;
    s.Open();
    SmartPtr<LogConn> c = new LogConn("opconn");
    c->SetKeepAlivePeriod(0);
    SmartPtr<Done> aPrevious = new Done("previous");
    c->SetConnectCompletion(aPrevious);
    // (run async and pumped: run blocking, the op would wait on the peer op's run, which the pump starts)
    SmartPtr<ServerConnectionConnectOp> anOp = new ServerConnectionConnectOp(c, s.Addr());
    anOp->RunAsync(4000);
    LOG2("connect op queued ", St(anOp->GetStatus()), "");
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (anOp->GetStatus() == WS_Pending && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    Log("connect op status " + St(anOp->GetStatus()) + " previous " + N(aPrevious->mCount));
    s.Accept();
    s.Send("op");
    PumpMessages(c, 1);
    // the op's completion was taken off the connection: a later connect completes nobody
    LOG2("connect block after op ", St(c->ConnectBlock(s.Addr(), 4000)), " previous " + N(aPrevious->mCount));
    s.Accept();
    // (a blocking op on a pending connect is left out: the op's time-out and the peer op's, the same time, race)
    c->Close();
    PumpFor(50);
    // async: completed through its own completion
    SmartPtr<Done> anOpDone = new Done("connect op");
    SmartPtr<ServerConnectionConnectOp> anAsync = new ServerConnectionConnectOp(c, s.Addr());
    anAsync->SetCompletion(anOpDone);
    anAsync->RunAsync(4000);
    PumpUntil(anOpDone, 1);
    s.Accept();
    // async on a pending connect, killed: the op finishes killed
    SmartPtr<Done> aKilledDone = new Done("killed op");
    SmartPtr<ServerConnectionConnectOp> aKilled = new ServerConnectionConnectOp(c, IPAddr("127.0.0.1", 0));
    aKilled->SetCompletion(aKilledDone);
    aKilled->RunAsync(4000);
    PumpFor(20);
    aKilled->Kill(true);
    PumpUntil(aKilledDone, 1);
    c->Close();
    PumpFor(50);
    // the plain connection's message callback does nothing
    SmartPtr<PlainConn> p = new PlainConn;
    LOG2("plain connect ", St(p->ConnectBlock(s.Addr(), 4000)), "");
    s.Accept();
    s.Send("ignored");
    PumpFor(100);
    p->Close();
    p = NULL;
    s.Peer()->Close();
    s.mListener->Close();
    c = NULL;
    Log("ops done");
}

// ---- RoutingConnection (round 16's second part): the op list, the completions by op type, the reply dispatch

class LogRouting : public RoutingConnection {
public:
    LogRouting(const char* inName) : mName(inName) {}
    virtual void CloseHook() { Log(this->mName + " CloseHook"); RoutingConnection::CloseHook(); }
    virtual void KillHook() { Log(this->mName + " KillHook"); RoutingConnection::KillHook(); }
    virtual void RoutingOpCompleteHook(RoutingOp* inOp)
    {
        Log(this->mName + " complete hook " + RouteName(inOp) + " " + St(inOp->GetStatus()));
    }
    std::string mName;
protected:
    virtual ~LogRouting() { Log(this->mName + " destroyed"); }
};

class RouteDone : public CompletionBase<AsyncOpPtr> {
public:
    RouteDone(const char* inName) : mName(inName), mCount(0) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        RoutingOp* anOp = dynamic_cast<RoutingOp*>(inOp.get());
        if (anOp == NULL) {
            Log(this->mName + " completed a non-routing op");
            return;
        }
        Log(this->mName + " completed " + RouteName(anOp) + " type " + N(anOp->GetType()) + " "
            + St(anOp->GetStatus()) + " killed " + N(anOp->IsKilled()) + " mode " + N(anOp->GetMode()));
    }
    std::string mName;
    int mCount;
};
static void PumpRoute(RouteDone* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}

// a routing message as the server frames it: the 2-byte length (its own bytes included), then the bytes
static void Push(Server& s, const char* inName, const unsigned char* inBytes, size_t inLength)
{
    std::string aMsg(2, '\0');
    size_t aLength = 2 + inLength;
    aMsg[0] = (char)(aLength & 0xff);
    aMsg[1] = (char)((aLength >> 8) & 0xff);
    aMsg.append(reinterpret_cast<const char*>(inBytes), inLength);
    Log(std::string("server ") + inName + " "
        + St(s.Peer()->SendMsg(Bytes(aMsg.data(), (unsigned long)aMsg.size()), 5000)));
}
#define PUSH(s, name, ...) do { static const unsigned char b_[] = {__VA_ARGS__}; Push(s, name, b_, sizeof(b_)); } while (0)

static void Routing()
{
    Log("== RoutingConnection");
    Server s;
    s.Open();
    SmartPtr<LogRouting> r = new LogRouting("route");
    Log(Show("route fresh", r));
    r->SetKeepAlivePeriod(0);
    SmartPtr<RouteDone> aLeft = new RouteDone("left server");
    SmartPtr<RouteDone> aDefault = new RouteDone("default");
    r->SetOpCompletion(10, aLeft);
    r->SetDefaultOpCompletion(aDefault);
    LOG2("route connect ", St(r->ConnectBlock(s.Addr(), 5000)), "");
    s.Accept();
    s.Peer()->SetLengthFieldSize(2);

    // notifications: the type's completion, then the default one
    PUSH(s, "client left server", 3, 7, 10, 5, 0);
    PumpRoute(aLeft, 1);
    PUSH(s, "shutdown aborted", 3, 7, 58);
    PumpRoute(aDefault, 1);
    // a short header and an unknown type with nothing waiting: nothing happens
    PUSH(s, "short header", 3, 7);
    PUSH(s, "unknown type", 3, 7, 200, 1, 2);
    PumpFor(100);
    Log("route after unknown left " + N(aLeft->mCount) + " default " + N(aDefault->mCount));

    // a request: its own completion, the server's status reply
    SmartPtr<RouteDone> aLeave = new RouteDone("leave");
    r->LeaveGroup(7, aLeave);
    s.Expect("server got leave");
    PUSH(s, "status 0", 3, 7, 66, 0, 0);
    PumpRoute(aLeave, 1);

    // two waiting requests: the first one without a completion takes the default; a status reply for each in order
    SmartPtr<RouteDone> aCancel = new RouteDone("cancel join");
    r->LeaveGroup(1, NULL);
    s.Expect("server got leave 1");
    r->CancelJoinGroup(2, false, aCancel);
    s.Expect("server got cancel");
    PUSH(s, "status -1", 3, 7, 66, 0xff, 0xff);
    PumpRoute(aDefault, 2);
    PUSH(s, "status 0", 3, 7, 66, 0, 0);
    PumpRoute(aCancel, 1);

    // a notification while a request waits: the notification goes first; the request still waits
    SmartPtr<RouteDone> aList = new RouteDone("client list");
    r->GetClientList(aList);
    s.Expect("server got client list");
    SmartPtr<RouteDone> aLeave3 = new RouteDone("leave 3");
    r->LeaveGroup(3, aLeave3);
    s.Expect("server got leave 3");
    PUSH(s, "client left server", 3, 7, 10, 6, 0);
    PumpRoute(aLeft, 2);
    // a status reply cut short: the client list op does not want it, the leave op's read fails (the catch finishes
    // it with the status the client list op returned)
    PUSH(s, "status cut", 3, 7, 66);
    PumpRoute(aLeave3, 1);
    // (a notification cut short is left out: its op, first in the list, is finished in the catch with a status no
    // op has returned yet, a variable the original never set)
    // a derived server's message: a derived op, unsolicited
    PUSH(s, "derived", 3, 8, 9, 1, 2, 3);
    PumpRoute(aDefault, 3);
    // the close kills the waiting client list op
    r->Close();
    PumpRoute(aList, 1);
    s.Peer()->Close();

    // the kill drops the completions: a notification completes nobody (the hook still runs); derived messages are
    // not unsolicited any more
    r->Kill();
    r->mAllDerivedOpsAreUnsolicited = false;
    LOG2("route reconnect ", St(r->ConnectBlock(s.Addr(), 5000)), "");
    s.Accept();
    s.Peer()->SetLengthFieldSize(2);
    PUSH(s, "client left server", 3, 7, 10, 7, 0);
    PUSH(s, "derived", 3, 8, 9, 1);
    PumpFor(150);
    Log("route after kill left " + N(aLeft->mCount) + " default " + N(aDefault->mCount));
    // a request's own completion still works
    SmartPtr<RouteDone> aLeave4 = new RouteDone("leave 4");
    r->LeaveGroup(4, aLeave4);
    s.Expect("server got leave 4");
    PUSH(s, "status 0", 3, 7, 66, 0, 0);
    PumpRoute(aLeave4, 1);
    r->Close();
    s.Peer()->Close();
    s.mListener->Close();
    PumpFor(50);
    r = NULL;
    Log("routing done");
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_servconn.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        BlockingConnect();
        AsyncConnect();
        ConnectOps();
        Routing();
        aCore.Pump(50);
        aCore.Shutdown();
    }
    Log("core shut down peerauth made " + N(sPeerMade) + " freed " + N(sPeerFreed));
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
    printf("won_servconn_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
