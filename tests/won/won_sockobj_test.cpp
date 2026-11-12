// WON's socket classes over 127.0.0.1 and IPAddr's values, one line per event, compared with rust/crates/ee_won/src/golden_sockobj.txt:
// AsyncSocket (open, bind, listen, connect, accept and the accepted socket's Duplicate, the hooks, the message
// transforms' order and refusals, sends and receives, datagrams, type changes, close, kill, disconnect), the socket
// ops run blocking (connect, accept, close, raw bytes with the progress completion, length-prefixed messages,
// CR-LF lines, datagrams, the op without a socket, each op's Duplicate), the same ops run async through WONAPICore's
// socket thread, QueueSocket (queued ops in order with their completions, the default completion, the repeat op and
// its duplicates, QueueOpFast, QueueClose, the close timer, the close and kill hooks) and BlockingSocket's calls
// (LobbyLanSocket needs the lobby's headers and units: the LAN game is its check). The golden was recorded over the
// C++ classes before their move to Rust (round 14). The timeouts
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
