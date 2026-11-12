// DetectFirewallOp: asks a firewall-detection server to connect or send a datagram back to us. The op's members and
// every step are ee_won's (rust/crates/ee_won/src/fwop.rs, a Rust object; the messages and the wait are
// miscops.rs's); this unit is the ServerRequestOp shim and the hooks for the listening socket and the receive ops.
// Finish, the base class's hooks, Track with RunAsync and the virtual RecvServerDatagram are called here, after
// Rust returns.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// the hooks (a friend: the request and response are ServerRequestOp's protected members)
struct DetectFirewallOpHooks {
    static RefCount* R(void* p) { return static_cast<RefCount*>(p); }
    static DetectFirewallOp* Op(void* c) { return static_cast<DetectFirewallOp*>(c); }
    static BlockingSocket* Sock(void* p) { return static_cast<BlockingSocket*>(R(p)); }
    static RecvBytesFromOp* Recv(void* p) { return static_cast<RecvBytesFromOp*>(R(p)); }

    static void Release(void* p) { R(p)->Release(); }
    static void* NewSocket(void*, bool udp)
    {
        BlockingSocket* s = new BlockingSocket(udp ? 1 : 0);
        s->AddRef();
        return static_cast<RefCount*>(s);
    }
    static long Bind(void*, void* s, uint16_t port) { return Sock(s)->Fn2B0D90(port, 0); }
    static long Listen(void*, void* s) { return Sock(s)->Listen(1000); }
    static uint16_t LocalPort(void*, void* s) { return Sock(s)->GetLocalPort(); }
    static void Close(void*, void* s) { Sock(s)->Close(); }
    static void* NewRecv(void*, void* s)
    {
        RecvBytesFromOp* r = new RecvBytesFromOp(255, Sock(s));
        r->AddRef();
        return static_cast<RefCount*>(r);
    }
    static long RecvRun(void*, void* r, uint32_t wait) { return Recv(r)->Run(2, wait); }
    static bool RecvFromCurrent(void* o, void* r) { return Recv(r)->GetAddr().GetHost() == Op(o)->GetCurAddr().GetHost(); }
    static uint32_t TimeLeft(void* o) { return (uint32_t)Op(o)->TimeLeft(); }
    static int32_t Mode(void* o) { return (int32_t)Op(o)->GetMode(); }
    static void SetRequest(void* o, const uint8_t* d, size_t n)
    {
        ByteBufferPtr aBuffer;
        WonByteBufferSink(&aBuffer, d, n);
        Op(o)->mRequest = aBuffer;
    }
    static void Response(void* o, EeWonByteSink s, void* c)
    {
        s(c, reinterpret_cast<const uint8_t*>(Op(o)->mResponse->data()), Op(o)->mResponse->length());
    }
    static long InvalidHeader(void* o) { return Op(o)->InvalidReplyHeader(); }
};

EeWonFirewallHooks DetectFirewallOp::Hooks()
{
    typedef DetectFirewallOpHooks H;
    EeWonFirewallHooks h;
    h.context = this;
    h.release = H::Release;
    h.new_socket = H::NewSocket;
    h.bind = H::Bind;
    h.listen = H::Listen;
    h.local_port = H::LocalPort;
    h.close = H::Close;
    h.new_recv = H::NewRecv;
    h.recv_run = H::RecvRun;
    h.recv_from_current = H::RecvFromCurrent;
    h.time_left = H::TimeLeft;
    h.mode = H::Mode;
    h.set_request = H::SetRequest;
    h.response = H::Response;
    h.invalid_header = H::InvalidHeader;
    h.set_public = NULL;    // the public address is the Rust object's
    return h;
}

// FUNCTION: WON 0x006cf5c0
// Sets the base's defaults (2-byte lengths, no receive limit); the op's own (listen on any port over TCP, wait for
// the connect) are the Rust object's
void DetectFirewallOp::Init()
{
    this->mLengthFieldSize=2;
    this->mMaxRecvTime=0xffffffff;
}
// FUNCTION: WON 0x006cf600
// Creates a firewall detection op against the given servers
DetectFirewallOp::DetectFirewallOp(ServerContext* inContext)
    : ServerRequestOp(inContext)
{
    this->mFw=ee_won_fwop_new();
    this->Init();
}
// The original's destructor was implicit (0x006cf690): the Rust object lets the socket and receive op go
DetectFirewallOp::~DetectFirewallOp()
{
    EeWonFirewallHooks h=this->Hooks();
    ee_won_fwop_free(this->mFw,&h);
}
// FUNCTION: WON 0x006cf760
// Opens the local TCP listener or UDP socket, notes its port and starts the server request
void DetectFirewallOp::RunHook()
{
    EeWonFirewallHooks h=this->Hooks();
    long aStatus=0;
    if (ee_won_fwop_run_hook(this->mFw,&h,&aStatus)) {
        this->Finish(aStatus);
        return;
    }
    ServerRequestOp::RunHook();
}
// FUNCTION: WON 0x006cf8b0
// Handles the datagram receive: keeps waiting while datagrams come from other hosts, else finishes
bool DetectFirewallOp::CallbackHook(AsyncOp* inOp,int inParam)
{
    if (inParam!=1000) return ServerRequestOp::CallbackHook(inOp,inParam);
    EeWonFirewallHooks h=this->Hooks();
    long aStatus=0;
    if (ee_won_fwop_callback_hook(this->mFw,&h,inParam,inOp->GetStatus(),&aStatus)==1) {
        this->RecvServerDatagram();
        return true;
    }
    this->Finish(aStatus);
    return true;
}
// FUNCTION: WON 0x006cf920
// Closes and drops the listen socket when the op finishes
void DetectFirewallOp::CleanupHook()
{
    ServerRequestOp::CleanupHook();
    EeWonFirewallHooks h=this->Hooks();
    ee_won_fwop_cleanup_hook(this->mFw,&h);
}
// FUNCTION: WON 0x006cf970
// Builds the detect request (TCP or UDP, our port, connect wait settings) and asks to receive the reply
WONStatus DetectFirewallOp::GetNextRequest()
{
    EeWonFirewallHooks h=this->Hooks();
    return ee_won_fwop_get_next_request(this->mFw,&h,this->mLengthFieldSize);
}
// FUNCTION: WON 0x006cfad0
// Checks the reply header, records our public address if given and returns the status (UDP: waits for the datagram)
WONStatus DetectFirewallOp::CheckResponse()
{
    EeWonFirewallHooks h=this->Hooks();
    int aKind=0;
    long aStatus=ee_won_fwop_check_response(this->mFw,&h,&aKind);
    if (aKind<0) throw ReadBufferException("Attempt to read past end of buffer.");
    if (aKind==1) return this->RecvServerDatagram();
    return aStatus;
}
// FUNCTION: WON 0x006cfba0
// Waits for a datagram from the server's host on the UDP socket, async or blocking; returns the status
WONStatus DetectFirewallOp::RecvServerDatagram()
{
    EeWonFirewallHooks h=this->Hooks();
    uint32_t aWait=0;
    long aStatus=0;
    if (ee_won_fwop_recv_datagram(this->mFw,&h,&aWait,&aStatus)) {
        RecvBytesFromOp* aRecv=DetectFirewallOpHooks::Recv(ee_won_fwop_recv_op(this->mFw));
        this->Track(aRecv,1000);
        aRecv->RunAsync(aWait);
        return WS_ServerReq_ExitCommunicationLoop;
    }
    return aStatus;
}
// the public address the server saw (set with port 0), a copy
IPAddr DetectFirewallOp::GetPublicAddr() const
{
    IPAddr anAddr;
    int32_t aHost=0;
    if (ee_won_fwop_public_host(this->mFw,&aHost))
        anAddr.Fn2B2C20(static_cast<long>(aHost),0);
    return anAddr;
}
}
