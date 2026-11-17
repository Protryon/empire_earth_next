// The references and lifetimes of WON's queue socket, UDP manager, UDP receive and request ops, timer thread, op
// container and socket thread (QueueSocket.cpp's, UDPManager.cpp's, SocketThread.cpp's and AsyncOp.cpp's C++ hooks
// and the completion classes they made: ParamCompletion, QueueOpFastCompletion, the close timer, QueueCloseOp,
// CloseOp), over real sockets on 127.0.0.1, one line per event, compared with
// rust/crates/ee_won_net/src/golden_core.txt (recorded over those units before their hooks were answered in Rust, net
// round 25). Every line carries the reference counts of the objects involved, and the objects made here log their
// destruction: a completion that holds its socket, manager or op one reference too many, or one too few, shows as a
// count or a missing (or early) destruction. The core runs single-threaded (SetDoSocketAndTimerThreads(false)): the
// socket and timer threads are pumped by Pump on this thread, so no count depends on another thread's timing. Waits
// are on events (a completion's count), never on elapsed time, but for the timers' own time-outs. `--print` prints
// the lines instead, `--trace` each line as it is logged. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatTime.h"
#include "won_blocking.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static bool sTrace;
static void Log(const string& inLine)
{
    sLines.push_back(inLine);
    if (sTrace) fprintf(stderr, "%s\n", inLine.c_str());
}
static string N(long long inValue) { return std::to_string(inValue); }
static string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static long long Refs(const RefCount* inObj) { return inObj == NULL ? -1 : (long long)inObj->GetRefCount(); }
static ByteBufferPtr Bytes(const void* inData, unsigned long inLength) { return new ByteBuffer(inData, inLength, false); }

// a socket that logs its destruction
class CoreSocket : public BlockingSocket {
public:
    CoreSocket(const char* inName, int inType) : BlockingSocket(inType), mName(inName) {}
    virtual ~CoreSocket() { Log(this->mName + " destroyed"); }
    virtual AsyncSocket* Duplicate() { return new CoreSocket("accepted", this->GetType()); }
    string mName;
};
// a completion that counts its calls and logs its destruction
class Done : public CompletionBase<AsyncOpPtr> {
public:
    Done(const char* inName) : mName(inName), mCount(0) {}
    virtual ~Done() { Log(this->mName + " destroyed"); }
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        Log(this->mName + " completed " + St(inOp->GetStatus()) + " killed " + N(inOp->IsKilled()) + " op refs "
            + N(Refs(inOp.get())));
    }
    string mName;
    int mCount;
};
// ops that log their destruction
class CoreRecvMsgOp : public RecvMsgOp {
public:
    CoreRecvMsgOp(const char* inName) : mName(inName) {}
    virtual ~CoreRecvMsgOp() { Log(this->mName + " destroyed"); }
    string mName;
};
class CoreSendMsgOp : public SendMsgOp {
public:
    CoreSendMsgOp(const char* inName, const ByteBuffer* inMsg) : SendMsgOp(inMsg), mName(inName) {}
    virtual ~CoreSendMsgOp() { Log(this->mName + " destroyed"); }
    string mName;
};
class CoreTimer : public AsyncOp {
public:
    CoreTimer(const char* inName) : mName(inName) {}
    virtual ~CoreTimer() { Log(this->mName + " destroyed"); }
    string mName;
};
class CoreRecvOp : public UDPManagerRecvOp {
public:
    CoreRecvOp(const char* inName) : mName(inName) {}
    CoreRecvOp(const char* inName, const IPAddr& inAddr) : UDPManagerRecvOp(inAddr), mName(inName) {}
    virtual ~CoreRecvOp() { Log(this->mName + " destroyed"); }
    string mName;
};
class CoreSendRecv : public UDPManagerSendRecvOp {
public:
    CoreSendRecv(const char* inName, char inExpect) : mName(inName), mExpect(inExpect) {}
    virtual ~CoreSendRecv() { Log(this->mName + " destroyed"); }
    virtual WONStatus CheckResponse(ReadBuffer& inMessage)
    {
        return (char)inMessage.ReadByte() == this->mExpect ? WS_Success : WS_None;
    }
    string mName;
    char mExpect;
};
class CoreManager : public UDPManager {
public:
    CoreManager(const char* inName) : mName(inName) {}
    virtual ~CoreManager() { Log(this->mName + " destroyed"); }
    string mName;
};

static WONAPICore* sCore;
static void PumpUntil(Done* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 10000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd) sCore->Pump(5);
    if (inDone->mCount < inCount) Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}
// pumps until the op is no longer pending and its completion delivered
static void PumpOp(AsyncOp* inOp)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 10000;
    while ((inOp->GetStatus() == WS_Pending || inOp->IsCompletionPending()) && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
}
static void Settle() { for (int i = 0; i < 4; i++) sCore->Pump(0); }

// a listener, a client connected to it and the accepted socket
struct Pair {
    SmartPtr<CoreSocket> mListener, mClient;
    SmartPtr<AsyncSocket> mServer;
    void Make(const char* inPrefix)
    {
        string p = inPrefix;
        this->mListener = new CoreSocket((p + " listener").c_str(), 0);
        this->mClient = new CoreSocket((p + " client").c_str(), 0);
        this->mListener->Bind(IPAddr("127.0.0.1", 0), true);
        this->mListener->Listen(5);
        WONStatus aConnect = WonBlocking::Connect(this->mClient, IPAddr("127.0.0.1", this->mListener->GetLocalPort()), 5000);
        this->mListener->WaitForAccept(5000);
        Log(p + " connect " + St(aConnect) + " accept " + St(this->mListener->Accept(this->mServer)));
    }
    BlockingSocket* Server() { return static_cast<BlockingSocket*>(this->mServer.get()); }
    void Drop()
    {
        if (this->mServer.get() != NULL) this->mServer->Close();
        this->mClient->Close();
        this->mListener->Close();
        Settle();
        this->mServer = NULL;
        this->mClient = NULL;
        this->mListener = NULL;
        Settle();
    }
};
static void SendMsg(BlockingSocket* inSocket, char inTag)
{
    unsigned char aMsg[] = {5, 0, 0, 0, (unsigned char)inTag};
    WONStatus aStatus = WonBlocking::SendBytes(inSocket, Bytes(aMsg, sizeof(aMsg)), 5000);
    if (aStatus != WS_Success) Log(string("send ") + inTag + " " + St(aStatus));
}

// ---- the queue socket

static void Queue()
{
    Log("== queue");
    Pair p;
    p.Make("q");
    BlockingSocket* s = p.Server();
    BlockingSocket* c = p.mClient.get();
    Log("server refs " + N(Refs(s)) + " client refs " + N(Refs(c)));
    SmartPtr<Done> aDefault = new Done("default");
    SmartPtr<Done> anOwn = new Done("own");
    s->SetQueueCompletion(aDefault);
    Log("default refs " + N(Refs(aDefault)));
    {
        SmartPtr<CoreRecvMsgOp> a = new CoreRecvMsgOp("recv a");
        SmartPtr<CoreRecvMsgOp> b = new CoreRecvMsgOp("recv b");
        b->SetCompletion(anOwn);
        s->QueueOp(a, 60000);
        s->QueueOp(b, 60000);
        Log("queued: a refs " + N(Refs(a)) + " b refs " + N(Refs(b)) + " server refs " + N(Refs(s)) + " default refs "
            + N(Refs(aDefault)) + " own refs " + N(Refs(anOwn)));
        Settle();
        Log("started: a refs " + N(Refs(a)) + " b refs " + N(Refs(b)) + " server refs " + N(Refs(s)));
        SendMsg(c, '1');
        PumpUntil(aDefault, 1);
        Log("a done: a refs " + N(Refs(a)) + " b refs " + N(Refs(b)) + " server refs " + N(Refs(s)) + " default refs "
            + N(Refs(aDefault)));
        SendMsg(c, '2');
        PumpUntil(anOwn, 1);
        Settle();
        Log("b done: a refs " + N(Refs(a)) + " b refs " + N(Refs(b)) + " server refs " + N(Refs(s)) + " own refs "
            + N(Refs(anOwn)));
    }
    Settle();
    Log("ops dropped: server refs " + N(Refs(s)));

    // the repeat op and its duplicates
    SmartPtr<Done> aRepeat = new Done("repeat");
    s->SetRepeatCompletion(aRepeat);
    {
        SmartPtr<CoreRecvMsgOp> r = new CoreRecvMsgOp("repeat op");
        s->SetRepeatOp(r);
        Log("repeat set: op refs " + N(Refs(r)) + " server refs " + N(Refs(s)) + " repeat refs " + N(Refs(aRepeat)));
        Settle();
        SendMsg(c, '3');
        PumpUntil(aRepeat, 1);
        Settle();
        Log("repeat 1: op refs " + N(Refs(r)) + " server refs " + N(Refs(s)) + " repeat refs " + N(Refs(aRepeat)));
    }
    Settle();
    SendMsg(c, '4');
    PumpUntil(aRepeat, 2);
    Settle();
    Log("repeat 2: server refs " + N(Refs(s)) + " repeat refs " + N(Refs(aRepeat)));

    // QueueOpFast on an idle queue: finished in place, its own completion called through the fast completion
    SmartPtr<Done> aFast = new Done("fast");
    {
        SmartPtr<CoreSendMsgOp> f = new CoreSendMsgOp("fast op", Bytes("\x05\0\0\0f", 5));
        f->SetCompletion(aFast);
        c->QueueOpFast(f, 5000);
        Log("fast queued: op refs " + N(Refs(f)) + " client refs " + N(Refs(c)) + " fast refs " + N(Refs(aFast)));
        PumpUntil(aFast, 1);
        Settle();
        Log("fast done: op refs " + N(Refs(f)) + " client refs " + N(Refs(c)) + " fast refs " + N(Refs(aFast)));
    }
    PumpUntil(aRepeat, 3);
    Settle();
    Log("fast dropped: client refs " + N(Refs(c)) + " fast refs " + N(Refs(aFast)));
    // QueueOpFast without a completion of its own: the queue's default
    SmartPtr<Done> aClientDefault = new Done("client default");
    c->SetQueueCompletion(aClientDefault);
    {
        SmartPtr<CoreSendMsgOp> f = new CoreSendMsgOp("fast op 2", Bytes("\x05\0\0\0g", 5));
        c->QueueOpFast(f, 5000);
        PumpUntil(aClientDefault, 1);
        Settle();
        Log("fast 2 done: op refs " + N(Refs(f)) + " client refs " + N(Refs(c)) + " client default refs "
            + N(Refs(aClientDefault)));
    }
    PumpUntil(aRepeat, 4);
    // QueueOpFast while an op runs: queued behind it
    {
        SmartPtr<CoreRecvMsgOp> w = new CoreRecvMsgOp("client wait");
        c->QueueOp(w, 60000);
        SmartPtr<CoreSendMsgOp> f = new CoreSendMsgOp("fast op 3", Bytes("\x05\0\0\0h", 5));
        c->QueueOpFast(f, 5000);
        Settle();
        Log("fast 3 behind: wait refs " + N(Refs(w)) + " op refs " + N(Refs(f)) + " client refs " + N(Refs(c)));
        SendMsg(s, '5');
        PumpUntil(aClientDefault, 3);
        Settle();
        Log("fast 3 done: wait refs " + N(Refs(w)) + " op refs " + N(Refs(f)) + " client refs " + N(Refs(c)));
    }
    PumpUntil(aRepeat, 5);
    // QueueOpFast of an op run at once that finishes in its run: never the queue's op, its own completion called
    // through the fast completion's original
    SmartPtr<Done> aFastNow = new Done("fast now");
    {
        SmartPtr<CoreSendMsgOp> f = new CoreSendMsgOp("fast now op", Bytes("\x05\0\0\0i", 5));
        f->SetRunAsyncImmediately(true);
        f->SetCompletion(aFastNow);
        c->QueueOpFast(f, 5000);
        Log("fast now: status " + St(f->GetStatus()) + " op refs " + N(Refs(f)) + " client refs " + N(Refs(c))
            + " fast now refs " + N(Refs(aFastNow)));
        PumpUntil(aFastNow, 1);
        Settle();
        Log("fast now done: op refs " + N(Refs(f)) + " client refs " + N(Refs(c)) + " fast now refs "
            + N(Refs(aFastNow)));
    }
    PumpUntil(aRepeat, 6);
    Settle();
    Log("server refs " + N(Refs(s)) + " client refs " + N(Refs(c)));

    // the close timer: cancelled, then firing (the close completion gets a killed close op)
    SmartPtr<Done> aClose = new Done("close");
    c->SetCloseCompletion(aClose);
    c->StartCloseTimer(60000);
    Log("timer started: client refs " + N(Refs(c)));
    c->CancelCloseTimer();
    Settle();
    Log("timer cancelled: client refs " + N(Refs(c)));
    c->StartCloseTimer(30);
    PumpUntil(aClose, 1);
    Settle();
    Log("timer fired: client refs " + N(Refs(c)) + " close refs " + N(Refs(aClose)) + " valid " + N(c->IsValid()));
    PumpUntil(aRepeat, 7);
    Settle();
    Log("server after peer close: server refs " + N(Refs(s)) + " valid " + N(s->IsValid()));
    // the kill hook drops every completion
    s->Kill();
    c->Kill();
    Settle();
    Log("killed: server refs " + N(Refs(s)) + " client refs " + N(Refs(c)) + " default refs " + N(Refs(aDefault))
        + " repeat refs " + N(Refs(aRepeat)) + " close refs " + N(Refs(aClose)) + " client default refs "
        + N(Refs(aClientDefault)));
    p.Drop();
    Log("queue pair dropped");

    // a queued close behind a pending op, and one with a timer while an op waits forever
    Pair p2;
    p2.Make("q2");
    BlockingSocket* s2 = p2.Server();
    BlockingSocket* c2 = p2.mClient.get();
    SmartPtr<Done> aClose2 = new Done("close2");
    SmartPtr<Done> aRepeat2 = new Done("repeat2");
    SmartPtr<Done> aSent2 = new Done("sent2");
    s2->SetCloseCompletion(aClose2);
    s2->SetRepeatCompletion(aRepeat2);
    s2->SetRepeatOp(new CoreRecvMsgOp("repeat2 op"));
    {
        SmartPtr<CoreSendMsgOp> anOp = new CoreSendMsgOp("sent2 op", Bytes("\x05\0\0\0q", 5));
        anOp->SetCompletion(aSent2);
        s2->QueueOp(anOp, 60000);
        s2->QueueClose(-1);
        Log("close queued: op refs " + N(Refs(anOp)) + " server refs " + N(Refs(s2)));
        PumpUntil(aSent2, 1);
        PumpUntil(aClose2, 1);
        Settle();
        Log("repeat2 count " + N(aRepeat2->mCount));
        Log("closed: op refs " + N(Refs(anOp)) + " server refs " + N(Refs(s2)) + " close2 refs " + N(Refs(aClose2))
            + " valid " + N(s2->IsValid()));
    }
    SmartPtr<Done> aClose3 = new Done("close3");
    SmartPtr<Done> aWait3 = new Done("wait3");
    c2->SetCloseCompletion(aClose3);
    {
        SmartPtr<CoreRecvMsgOp> w = new CoreRecvMsgOp("wait3 op");
        w->SetCompletion(aWait3);
        c2->QueueOp(w, -1);
        c2->QueueClose(30);
        Log("timed close queued: op refs " + N(Refs(w)) + " client refs " + N(Refs(c2)));
        PumpUntil(aWait3, 1);
        PumpUntil(aClose3, 1);
        Settle();
        Log("timed close: op refs " + N(Refs(w)) + " client refs " + N(Refs(c2)) + " close3 refs " + N(Refs(aClose3)));
    }
    // the close hook kills the queued ops with their own completions
    Pair p3;
    p3.Make("q3");
    BlockingSocket* s3 = p3.Server();
    SmartPtr<Done> aRun = new Done("run");
    SmartPtr<Done> aQueued = new Done("queued");
    SmartPtr<Done> aRep = new Done("rep");
    s3->SetRepeatCompletion(aRep);
    s3->SetRepeatOp(new RecvBytesOp(100));
    {
        SmartPtr<CoreRecvMsgOp> r = new CoreRecvMsgOp("run op");
        r->SetCompletion(aRun);
        s3->QueueOp(r, 60000);
        SmartPtr<CoreRecvMsgOp> q = new CoreRecvMsgOp("queued op");
        q->SetCompletion(aQueued);
        s3->QueueOp(q, 60000);
        Settle();
        Log("before close: run refs " + N(Refs(r)) + " queued refs " + N(Refs(q)) + " server refs " + N(Refs(s3)));
        s3->Close();
        PumpUntil(aRun, 1);
        PumpUntil(aRep, 1);
        Settle();
        Log("queued count " + N(aQueued->mCount) + " status " + St(q->GetStatus()));
        Log("after close: run refs " + N(Refs(r)) + " queued refs " + N(Refs(q)) + " server refs " + N(Refs(s3))
            + " rep refs " + N(Refs(aRep)));
    }
    p2.Drop();
    p3.Drop();
    Log("queue pairs dropped");
}

// ---- the UDP manager and its ops

static void Answer(BlockingSocket* inServer, bool inReply)
{
    ByteBufferPtr aGot;
    IPAddr aFrom;
    WONStatus aStatus = WonBlocking::RecvBytesFrom(inServer, aGot, 512, aFrom, 5000);
    Log("server got " + St(aStatus) + " " + (aGot.get() != NULL ? string(aGot->data(), aGot->length()) : string("-")));
    if (aStatus == WS_Success && inReply) {
        char aReply[2] = {(char)(aGot->data()[0] + 1), 'r'};
        WonBlocking::SendBytesTo(inServer, Bytes(aReply, 2), aFrom, 5000);
    }
}

static void Udp()
{
    Log("== udp");
    SmartPtr<CoreManager> m = new CoreManager("manager");
    SmartPtr<Done> aDefault = new Done("udp default");
    m->SetDefaultCompletion(aDefault);
    Log("manager refs " + N(Refs(m)) + " default refs " + N(Refs(aDefault)));
    Log("bind " + St(m->Bind(0, false)));
    QueueSocket* ms = m->GetSocket();
    unsigned short aPort = ms->GetLocalPort();
    Log("bound: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)));
    m->SetAutoRecv(true);
    Settle();
    Log("auto: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)));
    SmartPtr<CoreSocket> u = new CoreSocket("udp peer", 1);
    u->Bind(IPAddr("127.0.0.1", 0), false);
    IPAddr aMgr("127.0.0.1", aPort);
    IPAddr aU("127.0.0.1", u->GetLocalPort());
    WonBlocking::SendBytesTo(u, Bytes("hello", 5), aMgr, 5000);
    PumpUntil(aDefault, 1);
    Settle();
    Log("auto recv: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)) + " default refs " + N(Refs(aDefault)));
    SmartPtr<Done> anAddr = new Done("addr");
    {
        SmartPtr<CoreRecvOp> anOp = new CoreRecvOp("addr op", aU);
        anOp->SetCompletion(anAddr);
        m->QueueOp(anOp, 60000);
        Log("addr queued: op refs " + N(Refs(anOp)) + " manager refs " + N(Refs(m)));
        WonBlocking::SendBytesTo(u, Bytes("Ax", 2), aMgr, 5000);
        PumpUntil(anAddr, 1);
        Settle();
        Log("addr done: op refs " + N(Refs(anOp)) + " manager refs " + N(Refs(m)) + " msg refs " + N(Refs(anOp->GetMsg())));
    }
    m->CleanOps();
    Settle();
    Log("cleaned: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)));
    // the manager's Send and a request through the manager
    SmartPtr<CoreSocket> srv = new CoreSocket("udp server", 1);
    srv->Bind(IPAddr("127.0.0.1", 0), false);
    IPAddr aSrv("127.0.0.1", srv->GetLocalPort());
    m->Send(Bytes("Mm", 2), aSrv);
    Settle();
    Answer(srv, false);
    Log("sent: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)));
    SmartPtr<Done> aReq = new Done("request");
    {
        SmartPtr<CoreSendRecv> r = new CoreSendRecv("request op", 'b');
        r->SetUDPManager(m);
        r->SetDestAddr(aSrv);
        r->SetOutgoing(Bytes("a1", 2));
        r->SetCompletion(aReq);
        r->RunAsync(60000);
        Settle();
        Log("request sent: op refs " + N(Refs(r)) + " manager refs " + N(Refs(m)) + " wait refs "
            + N(Refs(r->GetRecvOp())));
        Answer(srv, true);
        PumpUntil(aReq, 1);
        Settle();
        Log("request done: op refs " + N(Refs(r)) + " manager refs " + N(Refs(m)) + " wait " + N(r->GetRecvOp() != NULL));
    }
    Settle();
    Log("request dropped: manager refs " + N(Refs(m)));
    // no reply: one retransmit, then WS_NoReply
    SmartPtr<Done> aNo = new Done("noreply");
    {
        SmartPtr<CoreSendRecv> r = new CoreSendRecv("noreply op", 'b');
        r->SetUDPManager(m);
        r->SetDestAddr(aSrv);
        r->SetNumRetransmits(1);
        r->SetMsgTimeout(40);
        r->SetOutgoing(Bytes("n", 1));
        r->SetCompletion(aNo);
        r->RunAsync(60000);
        PumpUntil(aNo, 1);
        Settle();
        Log("noreply done: op refs " + N(Refs(r)) + " manager refs " + N(Refs(m)));
        Answer(srv, false);
        Answer(srv, false);
    }
    // a request on its own socket
    SmartPtr<Done> anOwn = new Done("own");
    {
        SmartPtr<CoreSendRecv> r = new CoreSendRecv("own op", 'p');
        r->SetDestAddr(aSrv);
        r->SetOutgoing(Bytes("o", 1));
        r->SetCompletion(anOwn);
        r->RunAsync(60000);
        Settle();
        Log("own sent: op refs " + N(Refs(r)) + " socket refs " + N(Refs(r->GetSocket())) + " wait refs "
            + N(Refs(r->GetRecvOp())));
        Answer(srv, true);
        PumpUntil(anOwn, 1);
        Settle();
        Log("own done: op refs " + N(Refs(r)) + " socket " + N(r->GetSocket() != NULL));
    }
    // a receive op on its own socket
    SmartPtr<Done> aSelf = new Done("self");
    {
        SmartPtr<CoreRecvOp> r = new CoreRecvOp("self op");
        r->SetCompletion(aSelf);
        r->RunAsync(60000);
        Settle();
        QueueSocket* aSock = r->GetSocket();
        unsigned short aSelfPort = aSock != NULL ? aSock->GetLocalPort() : 0;
        Log("self waiting: op refs " + N(Refs(r)) + " socket refs " + N(Refs(aSock)));
        WonBlocking::SendBytesTo(srv, Bytes("ss", 2), IPAddr("127.0.0.1", aSelfPort), 5000);
        PumpUntil(aSelf, 1);
        Settle();
        Log("self done: op refs " + N(Refs(r)) + " socket " + N(r->GetSocket() != NULL) + " msg refs "
            + N(Refs(r->GetMsg())));
    }
    // close: a waiting op is killed
    SmartPtr<Done> aWait = new Done("wait");
    {
        SmartPtr<CoreRecvOp> w = new CoreRecvOp("wait op", aSrv);
        w->SetCompletion(aWait);
        m->QueueOp(w, 60000);
        Settle();
        Log("waiting: op refs " + N(Refs(w)) + " manager refs " + N(Refs(m)) + " socket refs " + N(Refs(ms)));
        m->Close();
        PumpUntil(aWait, 1);
        Settle();
        Log("closed: op refs " + N(Refs(w)) + " manager refs " + N(Refs(m)) + " socket " + N(m->GetSocket() != NULL));
    }
    Log("rebind " + St(m->Bind(0, true)));
    Settle();
    Log("rebound: manager refs " + N(Refs(m)) + " socket refs " + N(Refs(m->GetSocket())));
    m->Close();
    Settle();
    Log("closed again: manager refs " + N(Refs(m)));
    m = NULL;
    u->Close();
    srv->Close();
    Settle();
    u = NULL;
    srv = NULL;
    Settle();
    Log("udp dropped");
}

// ---- the timer thread, the op container and the socket thread

static void Machinery()
{
    Log("== machinery");
    SmartPtr<Done> aTimed = new Done("timed");
    {
        SmartPtr<CoreTimer> t = new CoreTimer("timer");
        t->SetCompletion(aTimed);
        t->RunAsTimer(30);
        Log("timer running: refs " + N(Refs(t)));
        PumpUntil(aTimed, 1);
        Settle();
        Log("timer done: refs " + N(Refs(t)) + " status " + St(t->GetStatus()));
        t->SetCompletion(NULL);
        t->RunAsTimer(60000);
        Log("timer again: refs " + N(Refs(t)));
        t->Kill(true);
        Settle();
        Log("timer killed: refs " + N(Refs(t)) + " status " + St(t->GetStatus()));
    }
    Settle();
    SmartPtr<Done> aQueued = new Done("container");
    {
        SmartPtr<CoreTimer> t = new CoreTimer("queued op");
        t->SetCompletion(aQueued);
        t->RunAsync(60000);
        Log("queued: refs " + N(Refs(t)) + " status " + St(t->GetStatus()));
        Settle();
        Log("started: refs " + N(Refs(t)) + " status " + St(t->GetStatus()));
        t->Finish(WS_Success);
        Log("finished: refs " + N(Refs(t)) + " pending completion " + N(t->IsCompletionPending()));
        PumpUntil(aQueued, 1);
        Settle();
        Log("completed: refs " + N(Refs(t)));
    }
    Settle();
    // a socket op waiting on the socket thread, then killed
    Pair p;
    p.Make("m");
    SmartPtr<Done> aRecv = new Done("thread recv");
    {
        SmartPtr<CoreRecvMsgOp> r = new CoreRecvMsgOp("thread recv op");
        r->SetSocket(p.Server());
        r->SetCompletion(aRecv);
        r->RunAsync(60000);
        Settle();
        Log("on the thread: refs " + N(Refs(r)) + " in thread " + N(r->IsInSocketThread()) + " socket refs "
            + N(Refs(p.Server())));
        r->Kill(true);
        PumpUntil(aRecv, 1);
        Settle();
        Log("killed: refs " + N(Refs(r)) + " in thread " + N(r->IsInSocketThread()));
    }
    Settle();
    p.Drop();
    Log("machinery done");
}

int main(int argc, char** argv)
{
    sTrace = argc > 1 && strcmp(argv[argc - 1], "--trace") == 0;
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDoSocketAndTimerThreads(false);
        aCore.Startup();
        sCore = &aCore;
        Queue();
        Udp();
        Machinery();
        aCore.Pump(20);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    Log("done");
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++) printf("%s\n", sLines[i].c_str());
        return 0;
    }
    string aGoldenPath = __FILE__;
    size_t aSlash = aGoldenPath.find_last_of("/\\");
    aGoldenPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aGoldenPath += "../../rust/crates/ee_won_net/src/golden_core.txt";
    FILE* aGolden = fopen(aGoldenPath.c_str(), "rb");
    if (aGolden == NULL) {
        printf("FAIL: cannot open %s\n", aGoldenPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aGolden)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r')) aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aGolden);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20) printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_core_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
