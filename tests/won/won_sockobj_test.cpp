// WON's socket classes over 127.0.0.1 and IPAddr's values, one line per event, compared with rust/crates/ee_won/src/golden_sockobj.txt:
// AsyncSocket (open, bind, listen, connect, accept and the accepted socket's Duplicate, the hooks, the message
// transforms' order and refusals, sends and receives, datagrams, type changes, close, kill, disconnect), the socket
// ops run blocking (connect, accept, close, raw bytes with the progress completion, length-prefixed messages,
// CR-LF lines, datagrams, the op without a socket, each op's Duplicate), the same ops run async through WONAPICore's
// socket thread, QueueSocket (queued ops in order with their completions, the default completion, the repeat op and
// its duplicates, QueueOpFast, QueueClose, the close timer, the close and kill hooks) and BlockingSocket's calls
// (LobbyLanSocket needs the lobby's headers and units: the LAN game is its check); round 15's part: the socket's lock
// and destination, more of each op (a large send continued on the socket thread, progress chunks, file receives,
// message limits, refusing transforms, re-runs, connect and accept async, connected datagram ops) and UDPManager with
// its receive and request ops; round 16's part: the receive and request ops' own sockets, given sockets, kills,
// time-outs, re-runs and checks that throw. Each part was recorded over the C++ classes before their move to Rust. The timeouts
// are long where an op succeeds and short (or 0) where it times out, so no line depends on the clock; async
// completions are waited for before the next step, so their order is the queue's. `--print` prints the lines
// instead. The exit code is the number of failures.
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
#include <sys/stat.h>
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
static PlatMutex* sLinesLock = PlatMutexCreate();   // a socket closed on the socket thread logs its hooks there
static void Log(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    sLines.push_back(inLine);
    static const bool sTrace = getenv("WON_SOCKOBJ_TRACE") != NULL;   // each line at once (a crash's last step)
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
static std::string Addr(const IPAddr& inAddr) { return N(inAddr.GetHost()) + (inAddr.IsValid() ? " valid" : " invalid"); }
// a port is the system's choice: only whether it is set
static std::string Port(unsigned short inPort) { return inPort != 0 ? "port set" : "port 0"; }
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

// a stream socket that logs its hooks and its Duplicate
class LogSocket : public BlockingSocket {
public:
    LogSocket(const char* inName, int inType = 0) : BlockingSocket(inType), mName(inName) {}
    virtual ~LogSocket() { Log(this->mName + " destroyed"); }
    virtual void OpenHook() { Log(this->mName + " OpenHook"); BlockingSocket::OpenHook(); }
    virtual void CloseHook() { Log(this->mName + " CloseHook"); BlockingSocket::CloseHook(); }
    virtual void KillHook() { Log(this->mName + " KillHook"); BlockingSocket::KillHook(); }
    virtual AsyncSocket* Duplicate() { Log(this->mName + " Duplicate"); return new LogSocket("accepted", this->GetType()); }
    std::string mName;
};

// appends its tag on send, checks and drops it on receive; refuses with mRefuse
class TagTransform : public MsgTransform {
public:
    TagTransform(char inTag) : mTag(inTag), mRefuse(0) {}
    virtual ~TagTransform() { Log(std::string("transform ") + this->mTag + " destroyed"); }
    virtual WONStatus Send(ByteBufferPtr& ioMessage)
    {
        Log(std::string("transform ") + this->mTag + " send " + Hex(ioMessage));
        if (this->mRefuse != 0)
            return this->mRefuse;
        std::string s(ioMessage->data(), ioMessage->length());
        s += this->mTag;
        ioMessage = new ByteBuffer(s.data(), s.size(), false);
        return 0;
    }
    virtual WONStatus Recv(ByteBufferPtr& ioMessage)
    {
        Log(std::string("transform ") + this->mTag + " recv " + Hex(ioMessage));
        if (this->mRefuse != 0)
            return this->mRefuse;
        if (ioMessage->length() == 0 || ioMessage->data()[ioMessage->length() - 1] != this->mTag)
            return WS_RecvMsg_InvalidMessageLength;
        ioMessage = new ByteBuffer(ioMessage->data(), ioMessage->length() - 1, false);
        return 0;
    }
    char mTag;
    WONStatus mRefuse;
};

// logs each completion with the op's class, status and kill flag
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
        RecvBytesOp* aBytes = dynamic_cast<RecvBytesOp*>(anOp);
        if (aMsg == NULL && aBytes != NULL && aBytes->GetStatus() == WS_Success)
            Log(this->mName + " bytes " + Hex(aBytes->GetBytes()));
    }
    std::string mName;
    int mCount;
};

// the first part is evaluated (and its call made) before the rest: MSVC's ABI evaluates `a + f() + g()` right to left
#define LOG2(head, first, rest) do { std::string l_ = std::string(head) + (first); l_ += std::string() + rest; Log(l_); } while (0)

static WONAPICore* sCore;
static void PumpUntil(Done* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}

static std::string Show(const char* inName, AsyncSocket* inSocket)
{
    return std::string(inName) + " valid " + N(inSocket->IsValid()) + " open " + N(inSocket->IsOpen()) + " connected "
        + N(inSocket->IsConnected()) + " type " + N(inSocket->GetType()) + " lengthfield "
        + N(inSocket->GetLengthFieldSize()) + " " + Port(inSocket->GetLocalPort());
}

// a listener, a client connected to it and the accepted socket
struct Pair {
    SmartPtr<LogSocket> mListener, mClient;
    SmartPtr<AsyncSocket> mServer;
    void Make(const char* inPrefix)
    {
        std::string p = inPrefix;
        this->mListener = new LogSocket((p + "listener").c_str());
        this->mClient = new LogSocket((p + "client").c_str());
        Log(p + " bind " + St(this->mListener->Bind(IPAddr("127.0.0.1", 0), true)));
        Log(p + " listen " + St(this->mListener->Listen(5)));
        Log(p + " connect " + St(this->mClient->Connect(IPAddr("127.0.0.1", this->mListener->GetLocalPort()), 5000)));
        this->mListener->WaitForAccept(5000);
        Log(p + " accept " + St(this->mListener->Accept(this->mServer)));
    }
    BlockingSocket* Server() { return static_cast<BlockingSocket*>(this->mServer.get()); }
};

// IPAddr's value (dotted addresses only: a name lookup is the system's): the parses, the text forms, the wire
// form, the order, the copies (a copy keeps its value when the original changes), the remembered host, the local
// host
static std::string IpShow(const char* inName, const IPAddr& inAddr)
{
    return std::string(inName) + " host " + N(inAddr.GetHost()) + " port " + N(inAddr.GetPort()) + " valid "
        + N(inAddr.IsValid()) + " text " + inAddr.GetHostString(true) + " / " + inAddr.GetHostString(false) + " "
        + inAddr.GetHostAndPortString(true);
}
static void Addresses()
{
    Log("== IPAddr");
    Log(IpShow("default", IPAddr()));
    Log(IpShow("host port", IPAddr("10.1.2.3", 80)));
    Log(IpShow("colon", IPAddr("10.1.2.3:8080")));
    // (the original left the sockaddr of an address made from a text without a colon unset: its golden line showed
    // the stack's last address, 10.1.2.3:8080; the Rust value is zero)
    Log(IpShow("no colon", IPAddr("10.1.2.3")));
    Log(IpShow("numeric", IPAddr(0x7f000001, 25)));
    Log(IpShow("broadcast", IPAddr::GetBroadcastAddr(9999)));
    Log(IpShow("port text", IPAddr("1.2.3.4: 77x")));
    IPAddr a;
    LOG2("nodns ", N(a.SetWithDefaultPortNoDNS("192.168.0.9", 33)), " " + IpShow("a", a));
    LOG2("nodns port ", N(a.SetWithDefaultPortNoDNS("192.168.0.9:44", 33)), " " + IpShow("a", a));
    LOG2("nodns name ", N(a.SetWithDefaultPortNoDNS("somewhere", 33)), " " + IpShow("a", a));
    LOG2("default port ", N(a.SetWithDefaultPort("10.0.0.1", 55)), " " + IpShow("a", a));
    LOG2("default port colon ", N(a.SetWithDefaultPort("10.0.0.2:66", 55)), " " + IpShow("a", a));
    IPAddr r;
    r.SetRememberHostString(true);
    LOG2("remember ", N(r.Fn2B2660("10.9.8.7", 21)), " " + IpShow("r", r));
    IPAddr rc = r;
    r.Fn2B2C20(0x0a000001, 22);
    Log(IpShow("r numeric", r));
    Log(IpShow("r copy", rc));
    rc.SetPort(23);
    Log(IpShow("r copy port", rc));
    rc = r;
    Log(IpShow("r assigned", rc));
    rc = rc;
    Log(IpShow("r self", rc));
    const char* six = r.GetSixByte();
    std::string aSix;
    for (int i = 0; i < 6; i++)
        aSix += N(static_cast<unsigned char>(six[i])) + ",";
    Log("six " + aSix);
    IPAddr s;
    const unsigned char aWire[6] = {0x04, 0xd2, 127, 0, 0, 1};
    s.SetSixByte(aWire);
    Log(IpShow("six set", s));
    PlatSockAddrIn sa = s.GetSockAddrIn();
    Log("sockaddr family " + N(sa.mFamily) + " port " + N(sa.mPort) + " address " + N(sa.mAddress));
    Log(IpShow("from sockaddr", IPAddr(sa)));
    // the order: the wire form (port first); StringCompare: the address first
    IPAddr x(0x01000000, 2), y(0x02000000, 1), z(0x01000000, 2);
    Log("x<y " + N(x < y) + " y<x " + N(y < x) + " x==z " + N(x == z) + " x!=y " + N(x != y) + " strcmp "
        + N(x.StringCompare(y)) + " " + N(y.StringCompare(x)) + " " + N(x.StringCompare(z)));
    std::map<IPAddr, int> aMap;
    aMap[IPAddr(0x0a000001, 3)] = 1;
    aMap[IPAddr(0x0a000002, 1)] = 2;
    aMap[IPAddr(0x0a000001, 2)] = 3;
    aMap[IPAddr(0x0a000001, 3)] = 4;
    std::string aOrder;
    for (std::map<IPAddr, int>::iterator i = aMap.begin(); i != aMap.end(); ++i)
        aOrder += i->first.GetHostAndPortString(true) + "=" + N(i->second) + " ";
    Log("map " + aOrder);
    // the local host
    Log("local host " + N(IPAddr::GetLocalHost()));
    LOG2("set local 7f000002 ", N(IPAddr::SetLocalHost(0x7f000002, false)), " " + N(IPAddr::GetLocalHost()));
    Log(IpShow("local addr", IPAddr::GetLocalAddr()));
    LOG2("set local bad ", N(IPAddr::SetLocalHost(0x01020304, true)), " " + N(IPAddr::GetLocalHost()));
    LOG2("set local 0 ", N(IPAddr::SetLocalHost(0, true)), " " + N(IPAddr::GetLocalHost()));
}

static void Sockets()
{
    Log("== AsyncSocket");
    SmartPtr<LogSocket> a = new LogSocket("a");
    Log(Show("a new", a));
    Log("a open " + St(a->Open()));
    Log(Show("a opened", a));
    Log("a open again " + St(a->Open()));
    Log("a nagling " + St(a->SetNagling(false)));
    Log("a bind " + St(a->Bind(IPAddr("127.0.0.1", 0), true)));
    Log(Show("a bound", a));
    Log("a listen " + St(a->Listen(5)));

    SmartPtr<LogSocket> b = new LogSocket("b");
    Log("b connect invalid " + St(b->Fn2B0E60(IPAddr())));
    Log(Show("b still", b));
    // a non-blocking connect on the loopback may finish at once (the system's choice): started either way
    WONStatus aStart = b->Fn2B0E60(IPAddr("127.0.0.1", a->GetLocalPort()));
    Log("b connect start " + N(aStart == WS_Success || aStart == WS_TimedOut));
    Log("b wait connect " + St(b->WaitForConnect(5000)));
    Log(Show("b connected", b));
    Log("b dest " + Addr(b->GetDestAddr()) + " same port " + N(b->GetDestAddr().GetPort() == a->GetLocalPort()));
    a->WaitForAccept(5000);
    SmartPtr<AsyncSocket> c;
    Log("a accept " + St(a->Accept(c)));
    Log(Show("c", c));
    Log("c class " + TypeName(c.get(), typeid(*c.get())) + " dest " + Addr(c->GetDestAddr()) + " same port "
        + N(c->GetDestAddr().GetPort() == b->GetLocalPort()));
    a->WaitForAccept(0);
    SmartPtr<AsyncSocket> none;
    LOG2("a accept none ", St(a->Accept(none)), " " + N(none.get() == NULL));

    // raw bytes
    int aSent = -1, aGot = -1;
    char aBuf[64];
    LOG2("b send ", St(b->Send("hello", 5, &aSent)), " " + N(aSent));
    c->WaitForRead(5000);
    memset(aBuf, 0, sizeof(aBuf));
    LOG2("c recv ", St(c->Recv(aBuf, 3, &aGot)), " " + N(aGot) + " " + std::string(aBuf, aGot > 0 ? aGot : 0));
    LOG2("c recv ", St(c->Recv(aBuf, 10, &aGot)), " " + N(aGot) + " " + std::string(aBuf, aGot > 0 ? aGot : 0));
    LOG2("c recv empty ", St(c->Recv(aBuf, 10, &aGot)), " " + N(aGot));
    Log("c sendto stream " + St(c->SendTo("x", 1, IPAddr("127.0.0.1", 9))));
    Log("c recvfrom stream " + St(c->RecvFrom(aBuf, 10, NULL, &aGot)));

    // the transforms: send in order, receive in reverse, the first refusal stops the chain
    TagTransform* x = new TagTransform('x');
    TagTransform* y = new TagTransform('y');
    b->AddMsgTransform(x);
    b->AddMsgTransform(y);
    ByteBufferPtr m = Bytes("m", 1);
    LOG2("b transform send ", St(b->TransformSendMsg(m)), " " + Hex(m));
    LOG2("b transform recv ", St(b->TransformRecvMsg(m)), " " + Hex(m));
    x->mRefuse = WS_Killed;
    m = Bytes("n", 1);
    LOG2("b transform send refused ", St(b->TransformSendMsg(m)), " " + Hex(m));
    m = Bytes("nyx", 3);
    LOG2("b transform recv refused ", St(b->TransformRecvMsg(m)), " " + Hex(m));
    b->RemoveAllMsgTransforms();
    m = Bytes("p", 1);
    LOG2("b transform none ", St(b->TransformSendMsg(m)), " " + Hex(m));

    // the length field, a type change
    b->SetLengthFieldSize(2);
    Log(Show("b field 2", b));
    b->SetType(AsyncSocket::TCP);
    Log(Show("b same type", b));
    b->SetType(AsyncSocket::UDP);
    Log(Show("b udp", b));
    b->SetType(AsyncSocket::TCP);

    // the peer closes: Shutdown
    c->Close();
    Log(Show("c closed", c));
    b->WaitForRead(5000);
    LOG2("b recv after close ", St(b->Recv(aBuf, 10, &aGot)), " " + N(aGot));
    c->Close();
    b->Disconnect();
    Log(Show("b disconnected", b));
    Log("b send closed " + St(b->Send("z", 1, &aSent)));
    a->Kill();
    Log(Show("a killed", a));

    // datagrams
    SmartPtr<LogSocket> u = new LogSocket("u", 1), v = new LogSocket("v", 1);
    Log("u sendto opens " + St(u->SendTo("ping", 4, IPAddr("127.0.0.1", 9))));
    Log(Show("u", u));
    Log("v bind " + St(v->Bind(IPAddr("127.0.0.1", 0), false)));
    Log("u sendto " + St(u->SendTo("ping", 4, IPAddr("127.0.0.1", v->GetLocalPort()))));
    v->WaitForRead(5000);
    IPAddr aFrom;
    LOG2("v recvfrom ", St(v->RecvFrom(aBuf, 64, &aFrom, &aGot)), " " + N(aGot) + " " + std::string(aBuf, aGot > 0 ? aGot : 0) + " from " + Addr(aFrom) + " " + N(aFrom.GetPort() == u->GetLocalPort()));
    Log("v recvfrom empty " + St(v->RecvFrom(aBuf, 64, &aFrom, &aGot)));
    Log("u listen " + St(u->Listen(1)));
    Log("u broadcast " + St(u->SetBroadcast(true)));
    Log("u connect " + St(u->Fn2B0E60(IPAddr("127.0.0.1", v->GetLocalPort()))));
    Log(Show("u connected", u));
    LOG2("u send ", St(u->Send("pong", 4, &aSent)), " " + N(aSent));
    v->WaitForRead(5000);
    LOG2("v recvfrom ", St(v->RecvFrom(aBuf, 64, NULL, &aGot)), " " + N(aGot));
    u->Disconnect();
    Log(Show("u disconnected", u));
    u->Close();
    v->Close();
    Log("sockets released");
}

class ProgressDone : public CompletionBase<AsyncOpPtr> {
public:
    virtual void Complete(AsyncOpPtr inOp)
    {
        RecvChunkOp* aChunk = dynamic_cast<RecvChunkOp*>(inOp.get());
        Log("progress " + TypeName(inOp.get(), typeid(*inOp.get())) + " " + St(inOp->GetStatus()) + " chunk " + N(aChunk != NULL));
    }
};

static void ShowDup(const char* inName, SocketOp* inOp)
{
    AsyncOp* d = inOp->Duplicate();
    SocketOp* s = dynamic_cast<SocketOp*>(d);
    Log(std::string("dup ") + inName + " " + TypeName(d, d != NULL ? typeid(*d) : typeid(void)) + " same socket "
        + N(s != NULL && s->GetSocket() == inOp->GetSocket()));
    if (d != NULL) {
        d->AddRef();
        d->Release();
    }
}

static void BlockingOps()
{
    Log("== blocking ops");
    Pair p;
    p.Make("p");
    BlockingSocket* s = p.Server();
    BlockingSocket* c = p.mClient.get();

    // a message per length field
    const unsigned char aMsg4[] = {9, 0, 0, 0, 'h', 'e', 'l', 'l', 'o'};
    SmartPtr<SendMsgOp> aSend = new SendMsgOp(Bytes(aMsg4, sizeof(aMsg4)), c);
    LOG2("sendmsg run ", N(aSend->RunBlock(5000)), " " + St(aSend->GetStatus()));
    SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(s);
    LOG2("recvmsg run ", N(aRecv->RunBlock(5000)), " " + St(aRecv->GetStatus()) + " " + Hex(aRecv->GetMsg()) + " " + Hex(aRecv->GetBytes()));
    s->SetLengthFieldSize(2);
    const unsigned char aMsgEsc[] = {0xff, 0xff, 7, 0, 0, 0, 'x', 'y', 'z'};
    Log("sendbytes " + St(c->SendBytes(Bytes(aMsgEsc, sizeof(aMsgEsc)), 5000)));
    ByteBufferPtr aGot;
    LOG2("recvmsg esc ", St(s->RecvMsg(aGot, 5000)), " " + Hex(aGot));
    s->SetLengthFieldSize(1);
    const unsigned char aMsg1[] = {3, 'q', 'r'};
    Log("sendbytes " + St(c->SendBytes(Bytes(aMsg1, sizeof(aMsg1)), 5000)));
    LOG2("recvmsg 1 ", St(s->RecvMsg(aGot, 5000)), " " + Hex(aGot));
    s->SetLengthFieldSize(4);
    const unsigned char aBad[] = {4, 0, 0, 0};
    Log("sendbytes " + St(c->SendBytes(Bytes(aBad, sizeof(aBad)), 5000)));
    Log("recvmsg bad " + St(s->RecvMsg(aGot, 5000)));
    s->SetLengthFieldSize(3);
    Log("recvmsg field 3 " + St(s->RecvMsg(aGot, 5000)));
    s->SetLengthFieldSize(4);
    aGot = NULL;
    LOG2("recvmsg none ", St(s->RecvMsg(aGot, 0)), " " + Hex(aGot));
    LOG2("recvmsg short ", St(s->RecvMsg(aGot, 50)), " " + Hex(aGot));

    // with transforms on both sides
    c->AddMsgTransform(new TagTransform('t'));
    s->AddMsgTransform(new TagTransform('t'));
    const unsigned char aMsgT[] = {6, 0, 0, 0, 'h', 'i'};
    Log("sendmsg transformed " + St(c->SendMsg(Bytes(aMsgT, sizeof(aMsgT)), 5000)));
    LOG2("recvmsg transformed ", St(s->RecvMsg(aGot, 5000)), " " + Hex(aGot));
    c->RemoveAllMsgTransforms();
    s->RemoveAllMsgTransforms();

    // lines and bytes
    const char aLines[] = "a\nb\r\nnext\r\n";
    Log("sendbytes " + St(c->SendBytes(Bytes(aLines, sizeof(aLines) - 1), 5000)));
    SmartPtr<RecvCRMsgOp> aLine = new RecvCRMsgOp(s);
    LOG2("recvcr run ", N(aLine->RunBlock(5000)), " " + St(aLine->GetStatus()) + " " + Hex(aLine->GetMsg()));
    LOG2("recvcr ", St(s->RecvCRMsg(aGot, 5000)), " " + Hex(aGot));
    Log("recvcr none " + St(s->RecvCRMsg(aGot, 0)));
    Log("sendbytes " + St(c->SendBytes(Bytes("par", 3), 5000)));
    LOG2("recvcr partial ", St(aLine->Run(OP_MODE_BLOCK, 100)), " " + Hex(aLine->GetMsg()));
    Log("sendbytes " + St(c->SendBytes(Bytes("t\r\n", 3), 5000)));
    LOG2("recvcr rest ", St(s->RecvCRMsg(aGot, 5000)), " " + Hex(aGot));

    // raw bytes with the progress completion every 3 bytes
    char aData[20];
    for (int i = 0; i < 20; i++)
        aData[i] = (char)('a' + i);
    Log("sendbytes " + St(c->SendBytes(Bytes(aData, 20), 5000)));
    SmartPtr<RecvBytesOp> aBytes = new RecvBytesOp(10, s);
    aBytes->SetProgressCompletion(new ProgressDone, 3);
    LOG2("recvbytes run ", N(aBytes->RunBlock(5000)), " " + St(aBytes->GetStatus()) + " " + Hex(aBytes->GetBytes()));
    SmartPtr<RecvBytesOp> aSkip = new RecvBytesOp(5, s);
    aSkip->SetSkipBytes(true);
    LOG2("recvbytes skip ", N(aSkip->RunBlock(5000)), " " + St(aSkip->GetStatus()) + " " + Hex(aSkip->GetBytes()));
    LOG2("recvbytes ", St(s->RecvBytes(aGot, 5, 5000)), " " + Hex(aGot));
    // into a file
    const char* aPath = "won_sockobj_recv.bin";
    remove(aPath);
    Log("sendbytes " + St(c->SendBytes(Bytes("filedata", 8), 5000)));
    SmartPtr<RecvBytesOp> aFile = new RecvBytesOp(8, s);
    aFile->SetFilePath(aPath);
    LOG2("recvbytes file ", N(aFile->RunBlock(5000)), " " + St(aFile->GetStatus()) + " " + Hex(aFile->GetBytes()));
    FILE* f = fopen(aPath, "rb");
    char aRead[16] = {0};
    size_t n = f != NULL ? fread(aRead, 1, sizeof(aRead), f) : 0;
    if (f != NULL)
        fclose(f);
    Log("file " + N((long long)n) + " " + std::string(aRead, n));
    remove(aPath);
    SmartPtr<RecvBytesOp> aNoFile = new RecvBytesOp(8, s);
    aNoFile->SetFilePath("no_such_dir_won_sockobj/x.bin");
    LOG2("recvbytes no file ", N(aNoFile->RunBlock(5000)), " " + St(aNoFile->GetStatus()));

    // the ops' duplicates
    ShowDup("sendmsg", aSend);
    ShowDup("recvmsg", aRecv);
    ShowDup("recvcr", aLine);
    ShowDup("recvbytes", aBytes);
    SmartPtr<CloseOp> aCloseOp = new CloseOp(c);
    ShowDup("close", aCloseOp);
    SmartPtr<AcceptOp> anAccept = new AcceptOp(p.mListener);
    ShowDup("accept", anAccept);
    SmartPtr<ConnectOp> aConnect = new ConnectOp(IPAddr("127.0.0.1", 9), c);
    ShowDup("connect", aConnect);
    Log("connect addr " + Addr(aConnect->GetAddr()));

    // (an op without a socket is not run: RunHook finishes it with WS_SocketOp_InvalidSocket and starts it anyway,
    // and every op's start reads its socket)

    // accept and connect as ops
    SmartPtr<LogSocket> d = new LogSocket("d");
    SmartPtr<ConnectOp> aConn = new ConnectOp(IPAddr("127.0.0.1", p.mListener->GetLocalPort()), d);
    LOG2("connectop run ", N(aConn->RunBlock(5000)), " " + St(aConn->GetStatus()));
    LOG2("acceptop run ", N(anAccept->RunBlock(5000)), " " + St(anAccept->GetStatus()) + " accepted " + N(anAccept->GetAcceptedSocket() != NULL));
    if (anAccept->GetAcceptedSocket() != NULL)
        Log(Show("acceptop socket", anAccept->GetAcceptedSocket()));
    SmartPtr<AcceptOp> anAccept2 = new AcceptOp(p.mListener);
    LOG2("acceptop none ", N(anAccept2->RunBlock(0)), " " + St(anAccept2->GetStatus()));
    SmartPtr<CloseOp> aCloseD = new CloseOp(d);
    LOG2("closeop run ", N(aCloseD->RunBlock(5000)), " " + St(aCloseD->GetStatus()));
    Log(Show("d", d));

    // datagram ops
    SmartPtr<BlockingSocket> u = new BlockingSocket(1), v = new BlockingSocket(1);
    LOG2("u bind ", St(u->Bind(IPAddr("127.0.0.1", 0), false)), " v bind " + St(v->Bind(IPAddr("127.0.0.1", 0), false)));
    SmartPtr<SendBytesToOp> aTo = new SendBytesToOp(Bytes("dgram", 5), IPAddr("127.0.0.1", v->GetLocalPort()), u);
    LOG2("sendbytesto run ", N(aTo->RunBlock(5000)), " " + St(aTo->GetStatus()));
    SmartPtr<RecvBytesFromOp> aFrom = new RecvBytesFromOp(512, v);
    LOG2("recvbytesfrom run ", N(aFrom->RunBlock(5000)), " " + St(aFrom->GetStatus()) + " " + Hex(aFrom->GetBytes()) + " from " + Addr(aFrom->GetAddr()) + " " + N(aFrom->GetAddr().GetPort() == u->GetLocalPort()));
    ShowDup("recvbytesfrom", aFrom);
    ShowDup("sendbytesto", aTo);
    LOG2("recvbytesfrom again ", N(aFrom->RunBlock(50)), " " + St(aFrom->GetStatus()) + " " + Hex(aFrom->GetBytes()));
    IPAddr anAddr;
    Log("sendbytesto " + St(u->SendBytesTo(Bytes("two", 3), IPAddr("127.0.0.1", v->GetLocalPort()), 5000)));
    LOG2("recvbytesfrom ", St(v->RecvBytesFrom(aGot, 512, anAddr, 5000)), " " + Hex(aGot) + " " + Addr(anAddr));
    // (a datagram longer than the buffer is the system's: WSAEMSGSIZE on Windows, cut on POSIX)
    u->Close();
    v->Close();

    // the peer closes
    Log("sendbytes " + St(c->SendBytes(Bytes("tail", 4), 5000)));
    c->Close();
    LOG2("recvbytes to close ", St(s->RecvBytes(aGot, 0, 5000)), " " + Hex(aGot));
    Log("recvmsg after close " + St(s->RecvMsg(aGot, 5000)));
    Log("sendmsg closed " + St(c->SendMsg(Bytes(aMsg4, sizeof(aMsg4)), 100)));
    s->Close();
    unsigned short aClosedPort = p.mListener->GetLocalPort();
    p.mListener->Close();
    SmartPtr<BlockingSocket> aRefused = new BlockingSocket(0);
    Log("connect refused " + St(aRefused->Connect(IPAddr("127.0.0.1", aClosedPort), 5000)));
    Log("connect invalid " + St(aRefused->Connect(IPAddr(), 100)));
    Log("blocking ops done");
}

static void AsyncOps()
{
    Log("== async ops");
    Pair p;
    p.Make("q");
    BlockingSocket* s = p.Server();
    BlockingSocket* c = p.mClient.get();
    SmartPtr<Done> aDone = new Done("recv");
    SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(s);
    aRecv->SetCompletion(aDone);
    aRecv->RunAsync(60000);
    LOG2("recv pending ", St(aRecv->GetStatus()), " in thread " + N(aRecv->IsInSocketThread()));
    SmartPtr<Done> aSent = new Done("send");
    const unsigned char aMsg[] = {8, 0, 0, 0, 'a', 's', 'y', 'n'};
    SmartPtr<SendMsgOp> aSend = new SendMsgOp(Bytes(aMsg, sizeof(aMsg)), c);
    aSend->SetCompletion(aSent);
    aSend->RunAsync(60000);
    PumpUntil(aSent, 1);
    PumpUntil(aDone, 1);
    Log("recv in thread " + N(aRecv->IsInSocketThread()));

    SmartPtr<Done> aLines = new Done("line");
    SmartPtr<RecvCRMsgOp> aLine = new RecvCRMsgOp(s);
    aLine->SetCompletion(aLines);
    aLine->RunAsync(60000);
    Log("sendbytes " + St(c->SendBytes(Bytes("li", 2), 5000)));
    sCore->Pump(20);
    Log("line count " + N(aLines->mCount));
    Log("sendbytes " + St(c->SendBytes(Bytes("ne\r\n", 4), 5000)));
    PumpUntil(aLines, 1);
    Log("line " + Hex(aLine->GetMsg()));

    SmartPtr<Done> aKilled = new Done("killed");
    SmartPtr<RecvBytesOp> aKill = new RecvBytesOp(4, s);
    aKill->SetCompletion(aKilled);
    aKill->RunAsync(60000);
    sCore->Pump(10);
    aKill->Kill(true);
    PumpUntil(aKilled, 1);
    Log("killed in thread " + N(aKill->IsInSocketThread()));

    SmartPtr<Done> aZero = new Done("zero");
    SmartPtr<RecvBytesOp> aNoTime = new RecvBytesOp(4, s);
    aNoTime->SetCompletion(aZero);
    aNoTime->RunAsync(0);
    PumpUntil(aZero, 1);
    c->Close();
    s->Close();
    p.mListener->Close();
    Log("async ops done");
}

static void Queues()
{
    Log("== QueueSocket");
    Pair p;
    p.Make("r");
    BlockingSocket* s = p.Server();
    BlockingSocket* c = p.mClient.get();
    SmartPtr<Done> aDefault = new Done("default");
    SmartPtr<Done> aOwn = new Done("own");
    SmartPtr<Done> aClose = new Done("close");
    s->SetQueueCompletion(aDefault);
    s->SetCloseCompletion(aClose);
    // three receives queued: they run one at a time in order
    for (int i = 0; i < 3; i++) {
        SmartPtr<RecvMsgOp> anOp = new RecvMsgOp;
        if (i == 1)
            anOp->SetCompletion(aOwn);
        s->QueueOp(anOp, 60000);
        Log("queued " + N(i) + " status " + St(anOp->GetStatus()) + " socket " + N(anOp->GetSocket() == s));
    }
    for (int i = 0; i < 3; i++) {
        unsigned char aMsg[] = {5, 0, 0, 0, (unsigned char)('0' + i)};
        Log("sendbytes " + St(c->SendBytes(Bytes(aMsg, sizeof(aMsg)), 5000)));
        if (i == 1)
            PumpUntil(aOwn, 1);
        else
            PumpUntil(aDefault, i == 0 ? 1 : 2);
    }

    // the repeat op: each finish duplicates it and runs the copy
    SmartPtr<Done> aRepeat = new Done("repeat");
    s->SetRepeatCompletion(aRepeat);
    s->SetRepeatOp(new RecvMsgOp);
    for (int i = 0; i < 3; i++) {
        unsigned char aMsg[] = {6, 0, 0, 0, 'r', (unsigned char)('0' + i)};
        Log("sendbytes " + St(c->SendBytes(Bytes(aMsg, sizeof(aMsg)), 5000)));
        PumpUntil(aRepeat, i + 1);
    }

    // QueueOpFast while the queue is idle finishes in place when it can, else waits as the queued op
    SmartPtr<Done> aFast = new Done("fast");
    SmartPtr<SendMsgOp> aFastSend = new SendMsgOp(Bytes("\x05\0\0\0f", 5));
    aFastSend->SetCompletion(aFast);
    c->QueueOpFast(aFastSend, 5000);
    PumpUntil(aFast, 1);
    // the repeat op got it
    PumpUntil(aRepeat, 4);

    // a failing op closes the socket and reports to the close completion; the repeat op dies with it
    SmartPtr<Done> aBadDone = new Done("bad");
    SmartPtr<RecvMsgOp> aBad = new RecvMsgOp;
    aBad->SetCompletion(aBadDone);
    // the repeat op's copy is queued in the op container (socket ops do not start at once): let it start first
    sCore->Pump(20);
    s->SetLengthFieldSize(3);
    s->QueueOp(aBad, 60000);
    PumpUntil(aBadDone, 1);
    PumpUntil(aClose, 1);
    Log(Show("s after failure", s));
    PumpUntil(aRepeat, 5);

    // a queued close after a pending op: kills the repeat op, runs after the op, closes and reports
    Pair p2;
    p2.Make("t");
    BlockingSocket* s2 = p2.Server();
    BlockingSocket* c2 = p2.mClient.get();
    SmartPtr<Done> aClose2 = new Done("close2");
    SmartPtr<Done> aRepeat2 = new Done("repeat2");
    SmartPtr<Done> aQueued2 = new Done("queued2");
    s2->SetCloseCompletion(aClose2);
    s2->SetRepeatCompletion(aRepeat2);
    s2->SetRepeatOp(new RecvCRMsgOp);
    SmartPtr<SendBytesOp> aPending = new SendBytesOp(Bytes("hey", 3));
    aPending->SetCompletion(aQueued2);
    s2->QueueOp(aPending, 60000);
    PumpUntil(aQueued2, 1);
    s2->QueueClose(-1);
    PumpUntil(aRepeat2, 1);
    PumpUntil(aClose2, 1);
    Log(Show("s2 queued close", s2));
    ByteBufferPtr aGot;
    LOG2("c2 got ", St(c2->RecvBytes(aGot, 3, 5000)), " " + Hex(aGot));

    // the close timer: the socket closes when it fires and reports a kill
    Pair p3;
    p3.Make("w");
    BlockingSocket* s3 = p3.Server();
    SmartPtr<Done> aClose3 = new Done("close3");
    s3->SetCloseCompletion(aClose3);
    s3->StartCloseTimer(0);
    s3->CancelCloseTimer();
    s3->StartCloseTimer(30);
    PumpUntil(aClose3, 1);
    Log(Show("s3 timer", s3));
    // a queued close with a timer while an op waits forever: the timer closes the socket and kills the op
    SmartPtr<Done> aClose4 = new Done("close4");
    SmartPtr<Done> aWait4 = new Done("wait4");
    BlockingSocket* c3 = p3.mClient.get();
    c3->SetCloseCompletion(aClose4);
    SmartPtr<RecvMsgOp> aWait = new RecvMsgOp;
    aWait->SetCompletion(aWait4);
    c3->QueueOp(aWait, -1);
    c3->QueueClose(30);
    PumpUntil(aWait4, 1);
    PumpUntil(aClose4, 1);
    Log(Show("c3 timer", c3));

    // the close hook kills the running and queued ops with their own completions; the kill hook drops the completions
    Pair p4;
    p4.Make("k");
    BlockingSocket* s4 = p4.Server();
    SmartPtr<Done> aRun = new Done("run");
    SmartPtr<Done> aQueued = new Done("queued");
    SmartPtr<Done> aRep = new Done("rep");
    SmartPtr<Done> aClose5 = new Done("close5");
    s4->SetRepeatCompletion(aRep);
    s4->SetCloseCompletion(aClose5);
    s4->SetRepeatOp(new RecvBytesOp(100));
    SmartPtr<RecvMsgOp> aRunOp = new RecvMsgOp;
    aRunOp->SetCompletion(aRun);
    s4->QueueOp(aRunOp, 60000);
    SmartPtr<RecvMsgOp> aQueuedOp = new RecvMsgOp;
    aQueuedOp->SetCompletion(aQueued);
    s4->QueueOp(aQueuedOp, 60000);
    s4->Close();
    PumpUntil(aRun, 1);
    PumpUntil(aQueued, 1);
    PumpUntil(aRep, 1);
    Log("close5 count " + N(aClose5->mCount));
    SmartPtr<Done> aRun6 = new Done("run6");
    s4->SetQueueCompletion(aRun6);
    s4->Kill();
    SmartPtr<RecvMsgOp> aAfterKill = new RecvMsgOp;
    s4->QueueOp(aAfterKill, 50);
    sCore->Pump(100);
    LOG2("after kill ", St(aAfterKill->GetStatus()), " run6 count " + N(aRun6->mCount));

    SmartPtr<AsyncSocket> aQueueDup = s4->Duplicate();
    Log("queue dup " + TypeName(aQueueDup.get(), typeid(*aQueueDup.get())) + " type " + N(aQueueDup->GetType()));
    SmartPtr<QueueSocket> aQueue = new QueueSocket(1);
    SmartPtr<AsyncSocket> aQueueDup2 = aQueue->Duplicate();
    Log("queue dup " + TypeName(aQueueDup2.get(), typeid(*aQueueDup2.get())) + " type " + N(aQueueDup2->GetType()));
    Log("queues done");
}

// ---- round 15: the socket's lock and destination, more of each socket op, UDPManager (recorded over the C++
// classes before their move)

// the socket's data lock (the subclasses' and connections' lock)
static void LockData(AsyncSocket* inSocket) { PlatMutexLock(inSocket->GetDataLock()); }
static void UnlockData(AsyncSocket* inSocket) { PlatMutexUnlock(inSocket->GetDataLock()); }

struct LockProbe {
    AsyncSocket* mSocket;
    static uint32_t Run(void* inProbe)
    {
        LockProbe* p = static_cast<LockProbe*>(inProbe);
        LockData(p->mSocket);
        Log("thread locked");
        UnlockData(p->mSocket);
        return 0;
    }
};

// logs each progress chunk with the receiving op's bytes so far
class ChunkDone : public CompletionBase<AsyncOpPtr> {
public:
    ChunkDone() : mCount(0), mLog(true) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        RecvChunkOp* aChunk = dynamic_cast<RecvChunkOp*>(inOp.get());
        this->mCount++;
        if (this->mLog)
            Log("chunk read " + N(aChunk != NULL ? (long long)aChunk->GetRecvOp()->GetBytesRead() : -1));
    }
    int mCount;
    bool mLog;
};

class RefuseTransform : public MsgTransform {
public:
    virtual WONStatus Send(ByteBufferPtr& ioMessage) { Log("refuse send " + Hex(ioMessage)); return WS_Killed; }
    virtual WONStatus Recv(ByteBufferPtr& ioMessage) { Log("refuse recv " + Hex(ioMessage)); return WS_Killed; }
};

static std::string FileText(const char* inPath)
{
    FILE* f = fopen(inPath, "rb");
    if (f == NULL)
        return "(no file)";
    char aRead[64];
    size_t n = fread(aRead, 1, sizeof(aRead), f);
    fclose(f);
    return N((long long)n) + " " + std::string(aRead, n);
}

static void WaitCount(ChunkDone* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 10000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log("TIMED OUT waiting for " + N(inCount));
}

static void LockAndDest()
{
    Log("== lock and destination");
    SmartPtr<BlockingSocket> s = new BlockingSocket(0);
    Log("dest new " + Addr(s->GetDestAddr()) + " port " + N(s->GetDestAddr().GetPort()));
    s->SetDestAddr(IPAddr("10.0.0.7", 99));
    Log("dest set " + Addr(s->GetDestAddr()) + " port " + N(s->GetDestAddr().GetPort()));
    IPAddr aCopy = s->GetDestAddr();
    s->SetDestAddr(IPAddr("10.0.0.8", 98));
    Log("dest copy " + Addr(aCopy) + " now " + Addr(s->GetDestAddr()));
    LOG2("connect invalid ", St(s->Fn2B0E60(IPAddr())), " dest " + Addr(s->GetDestAddr()));
    // the lock is recursive, and the socket's own calls take it under a held lock
    LockData(s);
    LockData(s);
    LOG2("locked twice, open ", St(s->Open()), " valid " + N(s->IsValid()));
    UnlockData(s);
    UnlockData(s);
    // another thread waits for it
    LockProbe aProbe = {s.get()};
    LockData(s);
    uint32_t anId = 0;
    PlatThread* aThread = PlatThreadCreate(&LockProbe::Run, &aProbe, 0, 0, &anId);
    PlatSleep(50);
    Log("main unlocking");
    UnlockData(s);
    PlatThreadWait(aThread, PLAT_WAIT_INFINITE);
    PlatThreadClose(aThread);
    // a datagram socket's connect sets the destination
    SmartPtr<BlockingSocket> u = new BlockingSocket(1);
    LOG2("udp connect ", St(u->Fn2B0E60(IPAddr("127.0.0.1", 9))), " dest " + Addr(u->GetDestAddr()) + " port "
         + N(u->GetDestAddr().GetPort()) + " connected " + N(u->IsConnected()));
    u->Disconnect();
    Log("udp disconnected dest " + Addr(u->GetDestAddr()) + " connected " + N(u->IsConnected()));
    s->Close();
    u->Close();
}

static void MoreOps()
{
    Log("== more socket ops");
    Pair p;
    p.Make("m");
    BlockingSocket* s = p.Server();
    BlockingSocket* c = p.mClient.get();

    // a large send continued on the socket thread while the receiver reads blocking, with progress chunks
    const unsigned long kBig = 2 * 1024 * 1024;
    std::string aBig(kBig, '\0');
    unsigned long aSum = 0;
    for (unsigned long i = 0; i < kBig; i++) {
        aBig[i] = (char)(i * 7 + (i >> 11));
        aSum = aSum * 31 + (unsigned char)aBig[i];
    }
    // (counted, not logged: the send can finish before or after the receive, the system's choice)
    SmartPtr<ChunkDone> aBigSent = new ChunkDone;
    aBigSent->mLog = false;
    SmartPtr<SendBytesOp> aBigSend = new SendBytesOp(Bytes(aBig.data(), kBig), c);
    aBigSend->SetCompletion(aBigSent);
    aBigSend->RunAsync(60000);
    // (queued in the op container: the pump starts it, and the socket thread continues it)
    sCore->Pump(5);
    SmartPtr<RecvBytesOp> aBigRecv = new RecvBytesOp(kBig, s);
    SmartPtr<ChunkDone> aChunks = new ChunkDone;
    aChunks->mLog = false;
    aBigRecv->SetProgressCompletion(aChunks, 700000);
    LOG2("bigrecv run ", N(aBigRecv->RunBlock(20000)), " " + St(aBigRecv->GetStatus()) + " read "
         + N((long long)aBigRecv->GetBytesRead()));
    ByteBufferPtr aBigGot = aBigRecv->GetBytes();
    unsigned long aGotSum = 0;
    for (unsigned long i = 0; aBigGot.get() != NULL && i < aBigGot->length(); i++)
        aGotSum = aGotSum * 31 + (unsigned char)aBigGot->data()[i];
    Log("bigrecv length " + N(aBigGot.get() != NULL ? (long long)aBigGot->length() : -1) + " same " + N(aGotSum == aSum)
        + " chunks " + N(aChunks->mCount > 0));
    WaitCount(aBigSent, 1);
    LOG2("bigsend ", St(aBigSend->GetStatus()), " count " + N(aBigSent->mCount));

    // the progress chunks see the bytes read so far
    SmartPtr<RecvBytesOp> aChunked = new RecvBytesOp(12, s);
    aChunked->SetProgressCompletion(new ChunkDone, 2);
    Log("sendbytes " + St(c->SendBytes(Bytes("abcde", 5), 5000)));
    LOG2("chunked part ", St(aChunked->Run(OP_MODE_BLOCK, 50)), " read " + N((long long)aChunked->GetBytesRead()));
    Log("sendbytes " + St(c->SendBytes(Bytes("fghijklmnop", 11), 5000)));
    LOG2("chunked run ", N(aChunked->RunBlock(5000)), " " + St(aChunked->GetStatus()) + " " + Hex(aChunked->GetBytes()));
    LOG2("chunked rest ", St(s->RecvBytes(aBigGot, 4, 5000)), " " + Hex(aBigGot));

    // files: append, text mode on bytes without line ends, the modify time, the path
    const char* aPath = "won_sockobj_r15.bin";
    remove(aPath);
    FILE* f = fopen(aPath, "wb");
    fwrite("head-", 1, 5, f);
    fclose(f);
    SmartPtr<RecvBytesOp> anAppend = new RecvBytesOp(4, s);
    anAppend->SetFilePath(aPath);
    anAppend->SetAppendFile(true);
    anAppend->SetFileModifyTime(1000000000);
    Log("path " + anAppend->GetFilePath());
    Log("sendbytes " + St(c->SendBytes(Bytes("tail", 4), 5000)));
    LOG2("recv append ", N(anAppend->RunBlock(5000)), " " + St(anAppend->GetStatus()) + " " + Hex(anAppend->GetBytes()));
    Log("file " + FileText(aPath));
    struct stat aStat;
    Log("mtime " + N(stat(aPath, &aStat) == 0 ? (long long)aStat.st_mtime : -1));
    SmartPtr<RecvBytesOp> aText = new RecvBytesOp(3, s);
    aText->SetFilePath(aPath);
    aText->SetBinaryFile(false);
    aText->SetProgressCompletion(new ChunkDone, 1);
    Log("sendbytes " + St(c->SendBytes(Bytes("txt", 3), 5000)));
    LOG2("recv text ", N(aText->RunBlock(5000)), " " + St(aText->GetStatus()));
    Log("file " + FileText(aPath));
    remove(aPath);

    // message lengths at the limits, a refusing transform on each side
    s->SetLengthFieldSize(2);
    const unsigned char aTwo[] = {2, 0};
    Log("sendbytes " + St(c->SendBytes(Bytes(aTwo, 2), 5000)));
    Log("recvmsg length 2 " + St(s->RecvMsg(aBigGot, 5000)));
    s->SetLengthFieldSize(4);
    const unsigned char aHuge[] = {0x00, 0xe8, 0x03, 0x00};
    Log("sendbytes " + St(c->SendBytes(Bytes(aHuge, 4), 5000)));
    Log("recvmsg 256000 " + St(s->RecvMsg(aBigGot, 5000)));
    const unsigned char aOk[] = {7, 0, 0, 0, 'o', 'k', '!'};
    c->AddMsgTransform(new RefuseTransform);
    SmartPtr<SendMsgOp> aRefused = new SendMsgOp(Bytes(aOk, sizeof(aOk)), c);
    LOG2("sendmsg refused ", N(aRefused->RunBlock(5000)), " " + St(aRefused->GetStatus()));
    c->RemoveAllMsgTransforms();
    s->AddMsgTransform(new RefuseTransform);
    Log("sendmsg " + St(c->SendMsg(Bytes(aOk, sizeof(aOk)), 5000)));
    SmartPtr<RecvMsgOp> aRecvRefused = new RecvMsgOp(s);
    LOG2("recvmsg refused ", N(aRecvRefused->RunBlock(5000)), " " + St(aRecvRefused->GetStatus()) + " "
         + Hex(aRecvRefused->GetMsg()) + " " + Hex(aRecvRefused->GetBytes()));
    s->RemoveAllMsgTransforms();
    // a message whose body arrives later continues from the body
    const unsigned char aHead[] = {9, 0, 0, 0, 'l'};
    Log("sendbytes " + St(c->SendBytes(Bytes(aHead, sizeof(aHead)), 5000)));
    SmartPtr<RecvMsgOp> aLater = new RecvMsgOp(s);
    LOG2("recvmsg part ", St(aLater->Run(OP_MODE_BLOCK, 50)), " " + Hex(aLater->GetMsg()));
    Log("sendbytes " + St(c->SendBytes(Bytes("ater", 4), 5000)));
    LOG2("recvmsg rest ", St(aLater->Run(OP_MODE_BLOCK, 5000)), " " + Hex(aLater->GetMsg()));
    // a message received async
    SmartPtr<Done> aMsgDone = new Done("msg");
    SmartPtr<RecvMsgOp> anAsyncMsg = new RecvMsgOp(s);
    anAsyncMsg->SetCompletion(aMsgDone);
    anAsyncMsg->RunAsync(60000);
    Log("sendmsg " + St(c->SendMsg(Bytes(aOk, sizeof(aOk)), 5000)));
    PumpUntil(aMsgDone, 1);

    // connect and accept as ops: async with completions, an invalid address, a refusal
    // (counted: the two finish on two threads in either order)
    SmartPtr<ChunkDone> aConnDone = new ChunkDone, anAccDone = new ChunkDone;
    aConnDone->mLog = anAccDone->mLog = false;
    SmartPtr<AcceptOp> anAccept = new AcceptOp(p.mListener);
    anAccept->SetCompletion(anAccDone);
    anAccept->RunAsync(60000);
    SmartPtr<LogSocket> e = new LogSocket("e");
    SmartPtr<ConnectOp> aConn = new ConnectOp(IPAddr("127.0.0.1", p.mListener->GetLocalPort()), e);
    aConn->SetCompletion(aConnDone);
    aConn->RunAsync(60000);
    WaitCount(aConnDone, 1);
    WaitCount(anAccDone, 1);
    LOG2("conn ", St(aConn->GetStatus()), " acc " + St(anAccept->GetStatus()));
    AsyncSocket* anAccepted = anAccept->GetAcceptedSocket();
    Log("accepted " + N(anAccepted != NULL) + " connected " + N(anAccepted != NULL && anAccepted->IsConnected())
        + " peer port " + N(anAccepted != NULL && anAccepted->GetDestAddr().GetPort() == e->GetLocalPort()));
    Log("connect addr " + Addr(aConn->GetAddr()) + " e dest " + Addr(e->GetDestAddr()));
    SmartPtr<ConnectOp> aBadConn = new ConnectOp(IPAddr(), e);
    LOG2("connectop invalid ", N(aBadConn->RunBlock(5000)), " " + St(aBadConn->GetStatus()));
    unsigned short aClosed = 0;
    {
        SmartPtr<BlockingSocket> aGone = new BlockingSocket(0);
        aGone->Bind(IPAddr("127.0.0.1", 0), false);
        aGone->Listen(1);
        aClosed = aGone->GetLocalPort();
        aGone->Close();
    }
    SmartPtr<LogSocket> g = new LogSocket("g");
    SmartPtr<ConnectOp> aRefusedConn = new ConnectOp(IPAddr("127.0.0.1", aClosed), g);
    LOG2("connectop refused ", N(aRefusedConn->RunBlock(5000)), " " + St(aRefusedConn->GetStatus()));
    SmartPtr<AcceptOp> anIdle = new AcceptOp(p.mListener);
    LOG2("acceptop short ", N(anIdle->RunBlock(50)), " " + St(anIdle->GetStatus()) + " accepted "
         + N(anIdle->GetAcceptedSocket() != NULL));
    if (anAccepted != NULL)
        anAccepted->Close();
    e->Close();
    g->Close();

    // datagram ops on a connected socket: the send is a Send, the receive's sender is the destination
    SmartPtr<BlockingSocket> u = new BlockingSocket(1), v = new BlockingSocket(1);
    LOG2("u bind ", St(u->Bind(IPAddr("127.0.0.1", 0), false)), " v bind " + St(v->Bind(IPAddr("127.0.0.1", 0), false)));
    Log("u connect " + St(u->Fn2B0E60(IPAddr("127.0.0.1", v->GetLocalPort()))));
    Log("v connect " + St(v->Fn2B0E60(IPAddr("127.0.0.1", u->GetLocalPort()))));
    SmartPtr<SendBytesToOp> aTo = new SendBytesToOp(Bytes("conn", 4), IPAddr("10.9.9.9", 1), u);
    LOG2("sendbytesto connected ", N(aTo->RunBlock(5000)), " " + St(aTo->GetStatus()));
    SmartPtr<RecvBytesFromOp> aFrom = new RecvBytesFromOp(16, v);
    LOG2("recvbytesfrom connected ", N(aFrom->RunBlock(5000)), " " + St(aFrom->GetStatus()) + " " + Hex(aFrom->GetBytes())
         + " from " + Addr(aFrom->GetAddr()) + " " + N(aFrom->GetAddr().GetPort() == u->GetLocalPort()));
    LOG2("recvbytesfrom none ", N(aFrom->RunBlock(0)), " " + St(aFrom->GetStatus()) + " " + Hex(aFrom->GetBytes()));
    // async, the datagram sent after the start
    SmartPtr<Done> aDgram = new Done("dgram");
    SmartPtr<RecvBytesFromOp> anAsyncFrom = new RecvBytesFromOp(16, v);
    anAsyncFrom->SetCompletion(aDgram);
    anAsyncFrom->RunAsync(60000);
    Log("send " + St(u->SendBytes(Bytes("late", 4), 5000)));
    PumpUntil(aDgram, 1);
    Log("dgram " + Hex(anAsyncFrom->GetBytes()) + " " + N(anAsyncFrom->GetAddr().GetPort() == u->GetLocalPort()));
    u->Close();
    v->Close();
    c->Close();
    s->Close();
    p.mListener->Close();
    Log("more ops done");
}

// a datagram check: accepts messages starting with mTag (WS_ServerReq_Recv for mAgain), else none
struct Verifier {
    char mTag;
    char mAgain;
    const char* mName;
    static WONStatus Check(ReadBuffer& inMessage, void* inContext)
    {
        Verifier* v = static_cast<Verifier*>(inContext);
        char c = (char)inMessage.ReadByte();
        Log(std::string(v->mName) + " checks " + std::string(1, c));
        if (c == v->mAgain)
            return WS_ServerReq_Recv;
        return c == v->mTag ? WS_Success : WS_None;
    }
};

// logs a UDP op's completion with its message and sender
class UdpDone : public CompletionBase<AsyncOpPtr> {
public:
    UdpDone(const char* inName) : mName(inName), mCount(0) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        AsyncOp* anOp = inOp.get();
        std::string s = this->mName + " completed " + TypeName(anOp, typeid(*anOp)) + " " + St(anOp->GetStatus())
            + " killed " + N(anOp->IsKilled());
        UDPManagerRecvOp* aRecv = dynamic_cast<UDPManagerRecvOp*>(anOp);
        if (aRecv != NULL)
            s += " msg " + Hex(aRecv->GetMsg()) + " from " + Addr(aRecv->GetFromAddr());
        Log(s);
    }
    std::string mName;
    int mCount;
};

static void PumpFor(UdpDone* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}

// receives inCount requests on the server socket (blocking), answering each with its first byte + 1 when asked
static void Answer(BlockingSocket* inServer, int inCount, bool inReply)
{
    for (int i = 0; i < inCount; i++) {
        ByteBufferPtr aGot;
        IPAddr aFrom;
        WONStatus aStatus = inServer->RecvBytesFrom(aGot, 512, aFrom, 5000);
        Log("server got " + std::string(St(aStatus)) + " " + Hex(aGot));
        if (aStatus == WS_Success && inReply) {
            char aReply[2] = {(char)(aGot->data()[0] + 1), 'r'};
            inServer->SendBytesTo(Bytes(aReply, 2), aFrom, 5000);
        }
    }
}

class TestSendRecv : public UDPManagerSendRecvOp {
public:
    TestSendRecv(char inExpect) : mExpect(inExpect) {}
    virtual WONStatus CheckResponse(ReadBuffer& inMessage)
    {
        char c = (char)inMessage.ReadByte();
        Log("response " + std::string(1, c));
        return c == this->mExpect ? WS_Success : WS_None;
    }
    char mExpect;
};

static void Udp()
{
    Log("== UDPManager");
    SmartPtr<UDPManager> m = new UDPManager;
    SmartPtr<UdpDone> aDefault = new UdpDone("udp default");
    m->SetDefaultCompletion(aDefault);
    // unbound: a queued op is killed
    SmartPtr<UdpDone> anEarly = new UdpDone("early");
    SmartPtr<UDPManagerRecvOp> anEarlyOp = new UDPManagerRecvOp;
    anEarlyOp->SetCompletion(anEarly);
    m->QueueOp(anEarlyOp, 1000);
    PumpFor(anEarly, 1);
    LOG2("early status ", St(anEarlyOp->GetStatus()), " killed " + N(anEarlyOp->IsKilled()));
    Log("bind " + std::string(St(m->Bind(0, false))));
    unsigned short aPort = m->GetSocket()->GetLocalPort();
    Log("bound " + Port(aPort));
    m->SetAutoRecv(true);
    SmartPtr<BlockingSocket> u = new BlockingSocket(1);
    u->Bind(IPAddr("127.0.0.1", 0), false);
    IPAddr aMgr("127.0.0.1", aPort);
    IPAddr aU("127.0.0.1", u->GetLocalPort());
    // nothing waits: the auto-receive op takes it with the default completion
    Log("send " + std::string(St(u->SendBytesTo(Bytes("hello", 5), aMgr, 5000))));
    PumpFor(aDefault, 1);
    // an op for u's address with a check: a datagram it refuses goes to the auto-receive op
    Verifier aCheck = {'A', 'R', "addr op"};
    SmartPtr<UdpDone> anAddrDone = new UdpDone("addr");
    SmartPtr<UDPManagerRecvOp> anAddrOp = new UDPManagerRecvOp(aU);
    anAddrOp->SetMessageVerifier(&Verifier::Check, &aCheck);
    anAddrOp->SetCompletion(anAddrDone);
    m->QueueOp(anAddrOp, 60000);
    LOG2("addr op status ", St(anAddrOp->GetStatus()), " verifier " + N(anAddrOp->HasVerifier()));
    Log("send " + std::string(St(u->SendBytesTo(Bytes("Bx", 2), aMgr, 5000))));
    PumpFor(aDefault, 2);
    // WS_ServerReq_Recv queues a new op for the address (no check: it takes the next datagram from there)
    Log("send " + std::string(St(u->SendBytesTo(Bytes("Ry", 2), aMgr, 5000))));
    PumpFor(aDefault, 3);
    Log("send " + std::string(St(u->SendBytesTo(Bytes("Az", 2), aMgr, 5000))));
    PumpFor(anAddrDone, 1);
    Log("send " + std::string(St(u->SendBytesTo(Bytes("Cw", 2), aMgr, 5000))));
    PumpFor(aDefault, 4);
    // an address-less op with a check takes any sender's matching datagram (and keeps the sender)
    Verifier aAny = {'Q', 0, "any op"};
    SmartPtr<UdpDone> anAnyDone = new UdpDone("any");
    SmartPtr<UDPManagerRecvOp> anAnyOp = new UDPManagerRecvOp;
    anAnyOp->SetMessageVerifier(&Verifier::Check, &aAny);
    anAnyOp->SetCompletion(anAnyDone);
    m->QueueOp(anAnyOp, 60000);
    Log("send " + std::string(St(u->SendBytesTo(Bytes("Qq", 2), aMgr, 5000))));
    PumpFor(anAnyDone, 1);
    // a timed-out op
    SmartPtr<UdpDone> aShortDone = new UdpDone("short");
    SmartPtr<UDPManagerRecvOp> aShortOp = new UDPManagerRecvOp(IPAddr("127.0.0.1", 1));
    aShortOp->SetCompletion(aShortDone);
    m->QueueOp(aShortOp, 30);
    PumpFor(aShortDone, 1);
    // blocking runs are refused
    SmartPtr<UDPManagerRecvOp> aBlock = new UDPManagerRecvOp;
    LOG2("recvop block ", N(aBlock->RunBlock(100)), " " + std::string(St(aBlock->GetStatus())));
    SmartPtr<TestSendRecv> aBlockSR = new TestSendRecv('b');
    LOG2("sendrecv block ", N(aBlockSR->RunBlock(100)), " " + std::string(St(aBlockSR->GetStatus())));

    // the manager's Send
    SmartPtr<BlockingSocket> srv = new BlockingSocket(1);
    srv->Bind(IPAddr("127.0.0.1", 0), false);
    IPAddr aSrv("127.0.0.1", srv->GetLocalPort());
    m->Send(Bytes("Mm", 2), aSrv);
    sCore->Pump(20);
    Answer(srv, 1, false);
    // a request through the manager: a wrong reply (to the auto-receive op), then the right one
    SmartPtr<UdpDone> aSRDone = new UdpDone("sendrecv");
    SmartPtr<TestSendRecv> aSR = new TestSendRecv('b');
    aSR->SetUDPManager(m);
    aSR->SetDestAddr(aSrv);
    aSR->mOutgoing = Bytes("a1", 2);
    aSR->SetCompletion(aSRDone);
    aSR->RunAsync(60000);
    sCore->Pump(20);
    Answer(srv, 1, false);
    srv->SendBytesTo(Bytes("zz", 2), aMgr, 5000);
    PumpFor(aDefault, 5);
    srv->SendBytesTo(Bytes("bb", 2), aMgr, 5000);
    PumpFor(aSRDone, 1);
    Log("sendrecv dest " + Addr(aSR->GetDestAddr()));
    // no reply: one retransmit, then WS_NoReply
    SmartPtr<UdpDone> aNoDone = new UdpDone("noreply");
    SmartPtr<TestSendRecv> aNo = new TestSendRecv('b');
    aNo->SetUDPManager(m);
    aNo->SetDestAddr(aSrv);
    aNo->SetNumRetransmits(1);
    aNo->SetMsgTimeout(60);
    aNo->mOutgoing = Bytes("n", 1);
    aNo->SetCompletion(aNoDone);
    aNo->RunAsync(60000);
    PumpFor(aNoDone, 1);
    Answer(srv, 2, false);
    // without a manager: the op's own socket
    SmartPtr<UdpDone> anOwnDone = new UdpDone("own");
    SmartPtr<TestSendRecv> anOwn = new TestSendRecv('p');
    anOwn->SetDestAddr(aSrv);
    anOwn->mOutgoing = Bytes("o", 1);
    anOwn->SetCompletion(anOwnDone);
    anOwn->RunAsync(60000);
    sCore->Pump(20);
    Answer(srv, 1, true);
    PumpFor(anOwnDone, 1);
    Log("own socket gone " + N(anOwn->mSocket.get() == NULL));
    // a receive op on its own socket
    SmartPtr<UdpDone> aSelfDone = new UdpDone("self");
    SmartPtr<UDPManagerRecvOp> aSelf = new UDPManagerRecvOp;
    aSelf->SetCompletion(aSelfDone);
    aSelf->RunAsync(60000);
    sCore->Pump(20);
    unsigned short aSelfPort = aSelf->mSocket.get() != NULL ? aSelf->mSocket->GetLocalPort() : 0;
    Log("self socket " + Port(aSelfPort));
    srv->SendBytesTo(Bytes("ss", 2), IPAddr("127.0.0.1", aSelfPort), 5000);
    PumpFor(aSelfDone, 1);
    Log("self socket gone " + N(aSelf->mSocket.get() == NULL));

    // close: the waiting ops are killed
    SmartPtr<UdpDone> aWaitDone = new UdpDone("wait");
    SmartPtr<UDPManagerRecvOp> aWait = new UDPManagerRecvOp(aSrv);
    aWait->SetCompletion(aWaitDone);
    m->QueueOp(aWait, 60000);
    SmartPtr<UdpDone> aWait2Done = new UdpDone("wait2");
    SmartPtr<UDPManagerRecvOp> aWait2 = new UDPManagerRecvOp;
    aWait2->SetMessageVerifier(&Verifier::Check, &aAny);
    aWait2->SetCompletion(aWait2Done);
    m->QueueOp(aWait2, 60000);
    m->Close();
    PumpFor(aWaitDone, 1);
    PumpFor(aWait2Done, 1);
    Log("closed socket " + N(m->GetSocket() == NULL));
    m->Close();
    // a rebind with broadcast, then auto-receive off
    Log("rebind " + std::string(St(m->Bind(0, true))));
    m->SetAutoRecv(false);
    Log("send " + std::string(St(u->SendBytesTo(Bytes("lost", 4), IPAddr("127.0.0.1", m->GetSocket()->GetLocalPort()), 5000))));
    sCore->Pump(50);
    Log("default count " + N(aDefault->mCount));
    m->Close();
    u->Close();
    srv->Close();
    Log("udp done");
}

// ---- round 16: the receive and request ops' own paths (recorded over the C++ classes before their move)

// a check that reads two bytes (a one-byte datagram throws ReadBufferException), accepting "ok"
static WONStatus TwoByteCheck(ReadBuffer& inMessage, void*)
{
    char a = (char)inMessage.ReadByte();
    char b = (char)inMessage.ReadByte();
    Log(std::string("two-byte check ") + a + b);
    return a == 'o' && b == 'k' ? WS_Success : WS_None;
}

// a request whose response check reads two bytes
class TwoByteSendRecv : public UDPManagerSendRecvOp {
public:
    virtual WONStatus CheckResponse(ReadBuffer& inMessage) { return TwoByteCheck(inMessage, NULL); }
};

static std::string RecvState(const char* inName, UDPManagerRecvOp* inOp)
{
    return std::string(inName) + " status " + St(inOp->GetStatus()) + " killed " + N(inOp->IsKilled()) + " verifier "
        + N(inOp->HasVerifier()) + " msg " + Hex(inOp->GetMsg()) + " from " + Addr(inOp->GetFromAddr()) + " socket "
        + N(inOp->mSocket.get() != NULL) + " manager " + N(inOp->mManager.get() != NULL);
}
static std::string SendRecvState(const char* inName, UDPManagerSendRecvOp* inOp)
{
    return std::string(inName) + " status " + St(inOp->GetStatus()) + " killed " + N(inOp->IsKilled()) + " dest "
        + Addr(inOp->GetDestAddr()) + " socket " + N(inOp->mSocket.get() != NULL) + " recvop "
        + N(inOp->mRecvOp.get() != NULL) + " attempts " + N(inOp->mAttemptCount) + " retries " + N(inOp->mRetryLimit)
        + " timeout " + N(inOp->mReceiveTimeout) + " outgoing " + Hex(inOp->mOutgoing);
}

static void UdpOps()
{
    Log("== UDP ops");
    SmartPtr<BlockingSocket> u = new BlockingSocket(1);
    u->Bind(IPAddr("127.0.0.1", 0), false);
    SmartPtr<BlockingSocket> srv = new BlockingSocket(1);
    srv->Bind(IPAddr("127.0.0.1", 0), false);
    IPAddr aSrv("127.0.0.1", srv->GetLocalPort());
    IPAddr aU("127.0.0.1", u->GetLocalPort());

    // a fresh op's state, the from address set and read back, the check set and cleared
    SmartPtr<UDPManagerRecvOp> aFresh = new UDPManagerRecvOp;
    Log(RecvState("fresh", aFresh));
    aFresh->SetFromAddr(aSrv);
    Log(RecvState("fresh from", aFresh));
    aFresh->SetMessageVerifier(&TwoByteCheck, NULL);
    Log(RecvState("fresh checked", aFresh));
    aFresh->SetMessageVerifier(NULL, NULL);
    Log(RecvState("fresh unchecked", aFresh));
    SmartPtr<UDPManagerSendRecvOp> aFreshSR = new UDPManagerSendRecvOp;
    aFreshSR->mAttemptCount = 0;   // the constructor leaves it unset
    Log(SendRecvState("fresh sendrecv", aFreshSR));

    // an addressed receive op on its own socket: another sender's datagram, a refused one, one that throws, then one
    // accepted (the own-socket path keeps no message)
    SmartPtr<UdpDone> anOwnDone = new UdpDone("own recv");
    SmartPtr<UDPManagerRecvOp> anOwn = new UDPManagerRecvOp(aSrv);
    anOwn->SetMessageVerifier(&TwoByteCheck, NULL);
    anOwn->SetCompletion(anOwnDone);
    anOwn->RunAsync(60000);
    sCore->Pump(20);
    Log(RecvState("own recv running", anOwn));
    IPAddr anOwnAddr("127.0.0.1", anOwn->mSocket.get() != NULL ? anOwn->mSocket->GetLocalPort() : 0);
    u->SendBytesTo(Bytes("ok", 2), anOwnAddr, 5000);
    sCore->Pump(30);
    srv->SendBytesTo(Bytes("no", 2), anOwnAddr, 5000);
    sCore->Pump(30);
    srv->SendBytesTo(Bytes("o", 1), anOwnAddr, 5000);
    sCore->Pump(30);
    Log(RecvState("own recv waiting", anOwn) + " completions " + N(anOwnDone->mCount));
    srv->SendBytesTo(Bytes("ok", 2), anOwnAddr, 5000);
    PumpFor(anOwnDone, 1);
    Log(RecvState("own recv done", anOwn));

    // a receive op on a socket it was given: the socket outlives the op; a check's WS_ServerReq_Recv finishes it
    SmartPtr<QueueSocket> aGiven = new QueueSocket(1);
    aGiven->Fn2B0D90(0, 0);
    IPAddr aGivenAddr("127.0.0.1", aGiven->GetLocalPort());
    Verifier anAgain = {'G', 'R', "given op"};
    SmartPtr<UdpDone> aGivenDone = new UdpDone("given");
    SmartPtr<UDPManagerRecvOp> aGivenOp = new UDPManagerRecvOp;
    aGivenOp->SetSocket(aGiven);
    aGivenOp->SetMessageVerifier(&Verifier::Check, &anAgain);
    aGivenOp->SetCompletion(aGivenDone);
    aGivenOp->RunAsync(60000);
    sCore->Pump(20);
    u->SendBytesTo(Bytes("Rr", 2), aGivenAddr, 5000);
    PumpFor(aGivenDone, 1);
    Log(RecvState("given done", aGivenOp) + " socket open " + N(aGiven->IsValid()));
    aGiven->Close();

    // a receive op killed while waiting on its own socket: the socket goes
    SmartPtr<UdpDone> aKilledDone = new UdpDone("killed recv");
    SmartPtr<UDPManagerRecvOp> aKilled = new UDPManagerRecvOp;
    aKilled->SetCompletion(aKilledDone);
    aKilled->RunAsync(60000);
    sCore->Pump(20);
    Log(RecvState("killed recv running", aKilled));
    aKilled->Kill(true);
    PumpFor(aKilledDone, 1);
    Log(RecvState("killed recv", aKilled));
    // a receive op timing out on its own socket
    SmartPtr<UdpDone> aLateDone = new UdpDone("late recv");
    SmartPtr<UDPManagerRecvOp> aLate = new UDPManagerRecvOp;
    aLate->SetCompletion(aLateDone);
    aLate->RunAsync(30);
    PumpFor(aLateDone, 1);
    Log(RecvState("late recv", aLate));

    // a request on its own socket: two retransmits, then WS_NoReply; then run again (the attempts start over): a
    // reply that throws, a wrong one, the right bytes from another sender (ignored: the reply wait has the request's
    // address) and the right one
    SmartPtr<UdpDone> aRetryDone = new UdpDone("retry");
    SmartPtr<TwoByteSendRecv> aRetry = new TwoByteSendRecv;
    aRetry->SetDestAddr(aSrv);
    aRetry->SetNumRetransmits(2);
    aRetry->SetMsgTimeout(60);
    aRetry->mOutgoing = Bytes("t", 1);
    aRetry->SetCompletion(aRetryDone);
    aRetry->RunAsync(60000);
    PumpFor(aRetryDone, 1);
    Log(SendRecvState("retry", aRetry));
    // the retransmits never leave: the timed-out reply wait's datagram receive (no time-out of its own) stays first in
    // the own socket's queue, so the later sends wait behind it until the socket is killed
    for (int i = 0; i < 3; i++) {
        ByteBufferPtr aSent;
        IPAddr aSender;
        WONStatus aStatus = srv->RecvBytesFrom(aSent, 512, aSender, 300);
        Log("server got " + std::string(St(aStatus)) + " " + Hex(aSent));
    }
    aRetry->SetMsgTimeout(60000);
    aRetry->mOutgoing = Bytes("2", 1);
    aRetry->RunAsync(60000);
    sCore->Pump(20);
    Log(SendRecvState("retry again running", aRetry));
    ByteBufferPtr aGot;
    IPAddr aFrom;
    WONStatus aGotStatus = srv->RecvBytesFrom(aGot, 512, aFrom, 5000);
    Log("server got " + std::string(St(aGotStatus)) + " " + Hex(aGot));
    srv->SendBytesTo(Bytes("x", 1), aFrom, 5000);
    sCore->Pump(30);
    srv->SendBytesTo(Bytes("xx", 2), aFrom, 5000);
    sCore->Pump(30);
    u->SendBytesTo(Bytes("ok", 2), aFrom, 5000);
    sCore->Pump(30);
    Log(SendRecvState("retry again waiting", aRetry) + " completions " + N(aRetryDone->mCount));
    srv->SendBytesTo(Bytes("ok", 2), aFrom, 5000);
    PumpFor(aRetryDone, 2);
    Log(SendRecvState("retry again", aRetry));

    // a request killed while it waits: its reply wait is killed and its socket goes
    SmartPtr<UdpDone> aCutDone = new UdpDone("cut");
    SmartPtr<TestSendRecv> aCut = new TestSendRecv('c');
    aCut->SetDestAddr(aSrv);
    aCut->mOutgoing = Bytes("k", 1);
    aCut->SetCompletion(aCutDone);
    aCut->RunAsync(60000);
    sCore->Pump(20);
    Log(SendRecvState("cut running", aCut));
    SmartPtr<UDPManagerRecvOp> aCutWait = aCut->mRecvOp.get();
    aCut->Kill(true);
    PumpFor(aCutDone, 1);
    Log(SendRecvState("cut", aCut));
    if (aCutWait.get() != NULL)
        Log(RecvState("cut wait", aCutWait));
    Answer(srv, 1, false);
    // a request through an unbound manager: the reply wait is killed at once and the request ends with its status
    SmartPtr<UDPManager> anUnbound = new UDPManager;
    SmartPtr<UdpDone> anUnboundDone = new UdpDone("unbound");
    SmartPtr<TestSendRecv> anUnboundSR = new TestSendRecv('u');
    anUnboundSR->SetUDPManager(anUnbound);
    anUnboundSR->SetDestAddr(aSrv);
    anUnboundSR->mOutgoing = Bytes("u", 1);
    anUnboundSR->SetCompletion(anUnboundDone);
    anUnboundSR->RunAsync(60000);
    PumpFor(anUnboundDone, 1);
    Log(SendRecvState("unbound", anUnboundSR));
    // a receive op run through a manager, then run again and the manager closed under it
    SmartPtr<UDPManager> m = new UDPManager;
    m->Bind(0, false);
    IPAddr aMgr("127.0.0.1", m->GetSocket()->GetLocalPort());
    SmartPtr<UdpDone> aMgrDone = new UdpDone("mgr recv");
    SmartPtr<UDPManagerRecvOp> aMgrOp = new UDPManagerRecvOp(aU);
    aMgrOp->SetUDPManager(m);
    aMgrOp->SetCompletion(aMgrDone);
    aMgrOp->RunAsync(60000);
    sCore->Pump(20);
    Log(RecvState("mgr recv running", aMgrOp));
    u->SendBytesTo(Bytes("mg", 2), aMgr, 5000);
    PumpFor(aMgrDone, 1);
    Log(RecvState("mgr recv done", aMgrOp));
    aMgrOp->RunAsync(60000);
    sCore->Pump(20);
    m->Close();
    PumpFor(aMgrDone, 2);
    Log(RecvState("mgr recv closed", aMgrOp));
    m->Close();
    u->Close();
    srv->Close();
    Log("udp ops done");
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_sockobj.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    Addresses();
    Sockets();
    BlockingOps();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        AsyncOps();
        Queues();
        LockAndDest();
        MoreOps();
        Udp();
        UdpOps();
        aCore.Pump(50);
        aCore.Shutdown();
    }
    Log("core shut down");
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
    printf("won_sockobj_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
