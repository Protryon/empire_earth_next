// AsyncOp: lifecycle, status, timeout and completion
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONTimerThread.h"
#include "ee/wonapi/WONRoutingDetectFirewallOp.h"
#include <string.h>
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "platform/PlatSocket.h"
// The socket error codes WON reports are Winsock's; the platform layer reports them in that numbering everywhere
static_assert(WS_WSAEINTR == PLAT_SOCK_EINTR, "Winsock numbering");
static_assert(WS_WSAEBADF == PLAT_SOCK_EBADF, "Winsock numbering");
static_assert(WS_WSAEACCES == PLAT_SOCK_EACCES, "Winsock numbering");
static_assert(WS_WSAEFAULT == PLAT_SOCK_EFAULT, "Winsock numbering");
static_assert(WS_WSAEINVAL == PLAT_SOCK_EINVAL, "Winsock numbering");
static_assert(WS_WSAEMFILE == PLAT_SOCK_EMFILE, "Winsock numbering");
static_assert(WS_WSAEWOULDBLOCK == PLAT_SOCK_EWOULDBLOCK, "Winsock numbering");
static_assert(WS_WSAEINPROGRESS == PLAT_SOCK_EINPROGRESS, "Winsock numbering");
static_assert(WS_WSAEALREADY == PLAT_SOCK_EALREADY, "Winsock numbering");
static_assert(WS_WSAENOTSOCK == PLAT_SOCK_ENOTSOCK, "Winsock numbering");
static_assert(WS_WSAEDESTADDRREQ == PLAT_SOCK_EDESTADDRREQ, "Winsock numbering");
static_assert(WS_WSAEMSGSIZE == PLAT_SOCK_EMSGSIZE, "Winsock numbering");
static_assert(WS_WSAEPROTOTYPE == PLAT_SOCK_EPROTOTYPE, "Winsock numbering");
static_assert(WS_WSAENOPROTOOPT == PLAT_SOCK_ENOPROTOOPT, "Winsock numbering");
static_assert(WS_WSAEPROTONOSUPPORT == PLAT_SOCK_EPROTONOSUPPORT, "Winsock numbering");
static_assert(WS_WSAESOCKTNOSUPPORT == PLAT_SOCK_ESOCKTNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEOPNOTSUPP == PLAT_SOCK_EOPNOTSUPP, "Winsock numbering");
static_assert(WS_WSAEPFNOSUPPORT == PLAT_SOCK_EPFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEAFNOSUPPORT == PLAT_SOCK_EAFNOSUPPORT, "Winsock numbering");
static_assert(WS_WSAEADDRINUSE == PLAT_SOCK_EADDRINUSE, "Winsock numbering");
static_assert(WS_WSAEADDRNOTAVAIL == PLAT_SOCK_EADDRNOTAVAIL, "Winsock numbering");
static_assert(WS_WSAENETDOWN == PLAT_SOCK_ENETDOWN, "Winsock numbering");
static_assert(WS_WSAENETUNREACH == PLAT_SOCK_ENETUNREACH, "Winsock numbering");
static_assert(WS_WSAENETRESET == PLAT_SOCK_ENETRESET, "Winsock numbering");
static_assert(WS_WSAECONNABORTED == PLAT_SOCK_ECONNABORTED, "Winsock numbering");
static_assert(WS_WSAECONNRESET == PLAT_SOCK_ECONNRESET, "Winsock numbering");
static_assert(WS_WSAENOBUFS == PLAT_SOCK_ENOBUFS, "Winsock numbering");
static_assert(WS_WSAEISCONN == PLAT_SOCK_EISCONN, "Winsock numbering");
static_assert(WS_WSAENOTCONN == PLAT_SOCK_ENOTCONN, "Winsock numbering");
static_assert(WS_WSAESHUTDOWN == PLAT_SOCK_ESHUTDOWN, "Winsock numbering");
static_assert(WS_WSAETOOMANYREFS == PLAT_SOCK_ETOOMANYREFS, "Winsock numbering");
static_assert(WS_WSAETIMEDOUT == PLAT_SOCK_ETIMEDOUT, "Winsock numbering");
static_assert(WS_WSAECONNREFUSED == PLAT_SOCK_ECONNREFUSED, "Winsock numbering");
static_assert(WS_WSAELOOP == PLAT_SOCK_ELOOP, "Winsock numbering");
static_assert(WS_WSAENAMETOOLONG == PLAT_SOCK_ENAMETOOLONG, "Winsock numbering");
static_assert(WS_WSAEHOSTDOWN == PLAT_SOCK_EHOSTDOWN, "Winsock numbering");
static_assert(WS_WSAEHOSTUNREACH == PLAT_SOCK_EHOSTUNREACH, "Winsock numbering");
static_assert(WS_WSAENOTEMPTY == PLAT_SOCK_ENOTEMPTY, "Winsock numbering");
static_assert(WS_WSAEPROCLIM == PLAT_SOCK_EPROCLIM, "Winsock numbering");
static_assert(WS_WSAEUSERS == PLAT_SOCK_EUSERS, "Winsock numbering");
static_assert(WS_WSAEDQUOT == PLAT_SOCK_EDQUOT, "Winsock numbering");
static_assert(WS_WSAESTALE == PLAT_SOCK_ESTALE, "Winsock numbering");
static_assert(WS_WSAEREMOTE == PLAT_SOCK_EREMOTE, "Winsock numbering");
static_assert(WS_WSASYSNOTREADY == PLAT_SOCK_SYSNOTREADY, "Winsock numbering");
static_assert(WS_WSAVERNOTSUPPORTED == PLAT_SOCK_VERNOTSUPPORTED, "Winsock numbering");
static_assert(WS_WSANOTINITIALISED == PLAT_SOCK_NOTINITIALISED, "Winsock numbering");
static_assert(WS_WSAEDISCON == PLAT_SOCK_EDISCON, "Winsock numbering");
namespace WONAPI {
// The op is a Rust object (rust/crates/ee_won_net/src/opobj.rs EeWonAsyncOp): its state, lock, completion and
// container, and every step below. These are the calls it makes back into the C++ (one table for every op and
// AsyncOpTracker): the virtual hooks, WONAPICore's queue and timer, the completion's references and call, the op's
// references and kill.
static void HookRun(void* inOp) { static_cast<AsyncOp*>(inOp)->RunHook(); }
static void HookCleanup(void* inOp) { static_cast<AsyncOp*>(inOp)->CleanupHook(); }
static void HookCoreQueue(void* inOp) { WONAPICore::QueueOp(static_cast<AsyncOp*>(inOp)); }
static void HookTimerAdd(void* inOp,uint32_t inTimeout) { WONAPICore::AddTimerOp(static_cast<AsyncOp*>(inOp),inTimeout); }
static void HookTimerRemove(void* inOp) { WONAPICore::RemoveTimerOp(static_cast<AsyncOp*>(inOp)); }
static void HookCompletionAddRef(void* inCompletion)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->AddRef(); }
static void HookCompletionRelease(void* inCompletion)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->Release(); }
static void HookCompletionCall(void* inCompletion,void* inOp)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->Complete(static_cast<AsyncOp*>(inOp)); }
static void HookOpAddRef(void* inOp) { static_cast<AsyncOp*>(inOp)->AddRef(); }
static void HookOpRelease(void* inOp) { static_cast<AsyncOp*>(inOp)->Release(); }
static void HookOpKill(void* inOp) { static_cast<AsyncOp*>(inOp)->Kill(true); }
static void HookRefAdd(void* inObject) { static_cast<RefCount*>(inObject)->AddRef(); }
static void HookRefRelease(void* inObject) { static_cast<RefCount*>(inObject)->Release(); }
// a completion whose call is Rust's (ee_won opobj.rs EeWonCompletion): the tracker's and the op base's timers' and
// repeating receives' (their C++ classes, TrackerCompletion and HostCompletion, are deleted)
class RustCompletion : public CompletionBase<AsyncOpPtr> {
    EeWonCompletion* mBody; // owned
public:
    explicit RustCompletion(EeWonCompletion* inBody) : mBody(inBody) {}
    virtual ~RustCompletion() { ee_won_completion_free(this->mBody); }
    virtual void Complete(AsyncOpPtr inOp) { ee_won_completion_run(this->mBody, inOp.get()); }
};
static void* HookNewCompletion(EeWonCompletion* inBody)
{
    return static_cast<CompletionBase<AsyncOpPtr>*>(new RustCompletion(inBody));
}
static bool HookCallback(void* inOp,void* inChild,int32_t inParam)
{
    return static_cast<AsyncOpWithTracker*>(static_cast<AsyncOp*>(inOp))->CallbackHook(static_cast<AsyncOp*>(inChild),
                                                                                      inParam);
}
static const EeWonAsyncOpHooks kAsyncOpHooks={HookRun,HookCleanup,HookCoreQueue,HookTimerAdd,HookTimerRemove,
                                             HookCompletionAddRef,HookCompletionRelease,HookCompletionCall,
                                             HookOpAddRef,HookOpRelease,HookOpKill,HookRefAdd,HookRefRelease,
                                             HookNewCompletion,HookCallback};
const EeWonAsyncOpHooks* AsyncOpHooks() { return &kAsyncOpHooks; }

// ---- the library's core table (ee_won cppcore.rs): what the Rust objects' own calls still need the C++ for. The
// queue socket's queued close: a socket op without a socket whose Continue succeeds (0x5c bytes, RTTI 44caac)
class QueueCloseOp : public SocketOp {
public:
    QueueCloseOp() : SocketOp(NULL) {}
    virtual WONStatus Continue() { return WS_Success; }
};
// a C++ object's Rust object of the kind (NULL when it is not of the kind's class)
static void* CoreObjectOf(void* inObject,EeWonCoreObject inKind)
{
    RefCount* r=static_cast<RefCount*>(inObject);
    switch(inKind) {
#define EE_WON_CORE_OF(kind,cls,get) case EeWonCoreObject::kind: { cls* x=dynamic_cast<cls*>(r); return x ? (void*)x->get : NULL; }
    EE_WON_CORE_OF(Op,AsyncOp,GetOpObject())
    EE_WON_CORE_OF(SocketOp,SocketOp,GetSocketOpObject())
    EE_WON_CORE_OF(Socket,AsyncSocket,GetSocketObject())
    EE_WON_CORE_OF(Queue,QueueSocket,GetQueueObject())
    EE_WON_CORE_OF(UdpManager,UDPManager,GetManagerObject())
    EE_WON_CORE_OF(UdpRecv,UDPManagerRecvOp,GetUdpObject())
    EE_WON_CORE_OF(UdpSendRecv,UDPManagerSendRecvOp,GetUdpObject())
    EE_WON_CORE_OF(ServerConn,ServerConnection,GetConnObject())
    EE_WON_CORE_OF(RouteOp,RoutingOp,mRust)
    EE_WON_CORE_OF(RouteConn,RoutingConnection,GetRouteObject())
    EE_WON_CORE_OF(AuthCtx,AuthContext,Core())
    EE_WON_CORE_OF(HostOp,RustServerOp,GetRustOp())
    EE_WON_CORE_OF(HttpConn,HTTPConnection,GetConnObject())
    EE_WON_CORE_OF(HttpSession,HTTPSession,GetSessionObject())
    EE_WON_CORE_OF(Context,ServerContext,GetListObject())
#undef EE_WON_CORE_OF
    case EeWonCoreObject::Lock: {
        if (HTTPConnection* c=dynamic_cast<HTTPConnection*>(r)) return c->GetLock();
        if (HTTPSession* s=dynamic_cast<HTTPSession*>(r)) return s->GetLock();
        return NULL;
    }
    }
    return NULL;
}
static void* CoreSocketOpDuplicate(void* inOp)
{
    return static_cast<SocketOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp)))->Duplicate();
}
static void* CoreNewOp() { return static_cast<RefCount*>(new AsyncOp); }
static void* CoreNewSocketOp(EeWonCoreSocketOp inKind,void* inSocket,void* inBytes,const EeWonIpAddr* inAddr,
                             uint32_t inSize)
{
    AsyncSocket* aSocket=static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket));
    const ByteBuffer* aBytes=static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBytes));
    SocketOp* anOp=NULL;
    switch(inKind) {
    case EeWonCoreSocketOp::Close: anOp=new CloseOp(aSocket); break;
    case EeWonCoreSocketOp::QueueClose: anOp=new QueueCloseOp; anOp->SetRunAsyncImmediately(false); break;
    case EeWonCoreSocketOp::RecvFrom: anOp=new RecvBytesFromOp((unsigned short)inSize,aSocket); break;
    case EeWonCoreSocketOp::RecvMsg: anOp=new RecvMsgOp(aSocket); break;
    case EeWonCoreSocketOp::SendMsg: anOp=new SendMsgOp(aBytes,aSocket); break;
    case EeWonCoreSocketOp::SendTo:
        anOp=new SendBytesToOp(aBytes,IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)),aSocket);
        break;
    case EeWonCoreSocketOp::Connect: anOp=new ConnectOp(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)),aSocket); break;
    case EeWonCoreSocketOp::SendBytes: anOp=new SendBytesOp(aBytes,aSocket); break;
    case EeWonCoreSocketOp::RecvBytes: anOp=new RecvBytesOp(inSize,aSocket); break;
    case EeWonCoreSocketOp::RecvCrMsg: anOp=new RecvCRMsgOp(aSocket); break;
    }
    return static_cast<RefCount*>(static_cast<AsyncOp*>(anOp));
}
static void* CoreNewQueueSocket(int32_t inType,bool inBlocking)
{ return static_cast<RefCount*>(inBlocking ? new BlockingSocket(inType) : new QueueSocket(inType)); }
static void* CoreNewUdpRecvOp(const EeWonIpAddr* inAddr)
{
    UDPManagerRecvOp* anOp=inAddr!=NULL ? new UDPManagerRecvOp(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)))
                                        : new UDPManagerRecvOp;
    return static_cast<RefCount*>(anOp);
}
static long CoreUdpCheck(void* inOp,const uint8_t* inData,size_t inLength,bool* outThrew)
{
    *outThrew=false;
    try {
        ReadBuffer aMessage(inData,static_cast<unsigned long>(inLength));
        return static_cast<UDPManagerRecvOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp)))->CheckMessage(aMessage);
    }
    catch(ReadBufferException&) {
        *outThrew=true;
        return 0;
    }
}
static long CoreBindLocal(void* inSocket,uint16_t inPort,bool inReuse)
{
    return static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket))->Fn2B0D90(inPort,inReuse);
}
static const EeWonByteBuffer* CoreBytesOf(void* inBuffer)
{
    return static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBuffer))->mBytes;
}
// ---- the socket op base's and the socket's calls (SocketRunHooks.cpp and AsyncSocket.cpp are deleted)
static SocketOp* SockOp(void* inOp) { return static_cast<SocketOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp))); }
static long CoreSocketOpStart(void* inOp) { return SockOp(inOp)->Start(); }
void SocketOp::HookAddToSocketThread(void* inOp) { WONAPICore::AddToSocketThread(SockOp(inOp)); }
static void* CoreBytesAdopt(EeWonByteBuffer* inBytes)
{
    RefCount* aBuffer=new ByteBuffer(inBytes);
    aBuffer->AddRef();
    return aBuffer;
}
// the socket's transform chain on the message in *ioMessage (a reference held; the result's replaces it)
static long CoreTransform(void* inSocket,void** ioMessage,bool inSend)
{
    ByteBufferPtr aMessage=static_cast<const ByteBuffer*>(static_cast<RefCount*>(*ioMessage));
    AsyncSocket* aSocket=static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket));
    WONStatus aStatus=inSend ? aSocket->TransformSendMsg(aMessage) : aSocket->TransformRecvMsg(aMessage);
    RefCount* aResult=const_cast<ByteBuffer*>(aMessage.get());
    if(aResult!=NULL) aResult->AddRef();
    if(*ioMessage!=NULL) static_cast<RefCount*>(*ioMessage)->Release();
    *ioMessage=aResult;
    return aStatus;
}
// a progress chunk: a RecvChunkOp of the receiving op with its completion, run in the op's mode
static void CoreProgress(void* inOp,void* inCompletion)
{
    RecvBytesOp* anOp=static_cast<RecvBytesOp*>(SockOp(inOp));
    SmartPtr<RecvChunkOp> aChunk=new RecvChunkOp(anOp);
    aChunk->SetCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(static_cast<RefCount*>(inCompletion)));
    long aMode=anOp->GetMode();
    aChunk->Run(aMode,0);
}
// Accept (the virtual Duplicate): the accepted socket in *outSocket with a reference, on success only
static long CoreAccept(void* inSocket,void** outSocket)
{
    SmartPtr<AsyncSocket> anAccepted;
    WONStatus aStatus=static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket))->Accept(anAccepted);
    if(aStatus==0) {
        RefCount* aResult=anAccepted.get();
        if(aResult!=NULL) aResult->AddRef();
        *outSocket=aResult;
    }
    return aStatus;
}
static void CoreSocketHook(void* inSocket,int32_t inWhich)
{
    AsyncSocket* aSocket=static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket));
    if(inWhich==0) aSocket->OpenHook();
    else if(inWhich==1) aSocket->CloseHook();
    else aSocket->KillHook();
}
static void* CoreNewHttpConn() { return static_cast<RefCount*>(new HTTPConnection(NULL)); }
// the routing classes' (ee_won routecpp.rs): a new op on a connection, the connection's two virtuals
static RoutingConnection* RouteConn(void* inConn)
{ return static_cast<RoutingConnection*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(inConn))); }
static void* CoreNewRouteOp(void* inConn,int32_t inType)
{
    RoutingConnection* aConn=inConn!=NULL ? RouteConn(inConn) : NULL;
    RoutingOp* anOp=inType==RoutingOp_DetectFirewall ? new RoutingDetectFirewallOp(aConn) : new RoutingOp(aConn,inType);
    return static_cast<RefCount*>(static_cast<AsyncOp*>(anOp));
}
static void* CoreRouteVirtual(void* inConn,int32_t inWhich,void* inOp,uint8_t inType)
{
    if(inWhich==0) {
        RouteConn(inConn)->RoutingOpCompleteHook(static_cast<RoutingOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp))));
        return NULL;
    }
    SmartPtr<RoutingOp> anOp=RouteConn(inConn)->GetUnsolicitedDerivedOp(inType);
    if(anOp.get()==NULL) return NULL;
    anOp->AddRef();
    return static_cast<RefCount*>(static_cast<AsyncOp*>(anOp.get()));
}
// leaves the socket thread if it was handed there
void SocketOp::CleanupHook()
{
    if(this->IsInSocketThread()) WONAPICore::RemoveFromSocketThread(this);
}
bool SocketOp::mRunAsyncImmediatelyDef; // a zero byte at 565f08 in the original
static uint32_t AddressOf(const IPAddr& inAddress) { return inAddress.GetSockAddrIn().mAddress; }
static uint16_t PortOf(const IPAddr& inAddress) { return inAddress.GetSockAddrIn().mPort; }
static IPAddr MakeAddr(uint32_t inAddress, uint16_t inPort)
{
    PlatSockAddrIn anAddress;
    memset(&anAddress, 0, sizeof(anAddress));
    anAddress.mFamily = PLAT_AF_INET;
    anAddress.mAddress = inAddress;
    anAddress.mPort = inPort;
    return IPAddr(anAddress);
}

// up to the time until writable or readable (no wait for 0 or a closed socket)
void AsyncSocket::WaitForWrite(unsigned long inWaitTime)
{ ee_won_asyncsocket_wait(this->mSock,PLAT_SOCKET_WAIT_WRITE,static_cast<uint32_t>(inWaitTime)); }
void AsyncSocket::WaitForRead(unsigned long inWaitTime)
{ ee_won_asyncsocket_wait(this->mSock,PLAT_SOCKET_WAIT_READ,static_cast<uint32_t>(inWaitTime)); }
WONStatus AsyncSocket::SendTo(const void* inData,int inLength,const IPAddr& inAddress)
{ return ee_won_asyncsocket_send_to(this->mSock,inData,inLength,AddressOf(inAddress),PortOf(inAddress)); }
WONStatus AsyncSocket::RecvFrom(void* outData,int inLength,IPAddr* outAddress,int* outReceived)
{
    uint32_t anAddress=0;
    uint16_t aPort=0;
    bool aFrom=false;
    WONStatus aStatus=ee_won_asyncsocket_recv_from(this->mSock,outData,inLength,&anAddress,&aPort,&aFrom,outReceived);
    if(aFrom && outAddress!=NULL) *outAddress=MakeAddr(anAddress,aPort);
    return aStatus;
}
WONStatus AsyncSocket::Bind(const IPAddr& inAddress,bool inReuse)
{ return ee_won_asyncsocket_bind(this->mSock,AddressOf(inAddress),PortOf(inAddress),inReuse); }
WONStatus AsyncSocket::Accept(SmartPtr<AsyncSocket>& outSocket)
{
    AutoCrit aCrit(this->GetDataLock());
    PlatSocket aSocket=PLAT_SOCKET_INVALID;
    uint32_t anAddress=0;
    uint16_t aPort=0;
    WONStatus aStatus=ee_won_asyncsocket_accept(this->mSock,&aSocket,&anAddress,&aPort);
    if(aStatus!=0) return aStatus;
    outSocket=this->Duplicate();
    ee_won_asyncsocket_adopt(outSocket->mSock,aSocket,anAddress,aPort);
    return 0;
}
static long CoreSocketOpContinue(void* inOp)
{
    return static_cast<SocketOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp)))->Continue();
}
static const EeWonCoreHooks kCoreHooks={HookRefAdd,HookRefRelease,HookCompletionCall,CoreObjectOf,
                                        CoreSocketOpDuplicate,CoreNewOp,HookNewCompletion,CoreNewSocketOp,
                                        CoreNewQueueSocket,CoreNewUdpRecvOp,CoreUdpCheck,
                                        reinterpret_cast<void*>(&UDPManagerSendRecvOp::StaticCheckResponse),
                                        CoreBindLocal,CoreBytesOf,CoreSocketOpContinue,CoreSocketOpStart,
                                        &SocketOp::HookAddToSocketThread,CoreBytesAdopt,CoreTransform,CoreProgress,
                                        CoreAccept,CoreSocketHook,CoreNewRouteOp,CoreRouteVirtual,CoreNewHttpConn};
const EeWonCoreHooks* WonCore() { return &kCoreHooks; }

}

// ---- the C++ side of the Rust thread, timer thread and op container (ee_won thread.rs): their calls into the
// objects they hold (Thread.cpp, TimerThread.cpp and AsyncOpContainer.cpp are deleted)
namespace WONAPI {
// the thread's body: ThreadFunc, with the crash dump's SEH filter on Windows (elsewhere there is no SEH)
void ThreadBase::RunThreadFunc(void* inThread)
{
    ThreadBase* aThread=static_cast<ThreadBase*>(inThread);
#ifdef _WIN32
    __try {
        aThread->ThreadFunc();
    } __except(WONDebug::DumpStackToFile(GetExceptionInformation())) {
    }
#else
    aThread->ThreadFunc();
#endif
}
void ThreadBase::DeleteThread(void* inThread) { delete static_cast<ThreadBase*>(inThread); }
ThreadBase::ThreadBase()
{
    EeWonThreadBody aBody={&ThreadBase::RunThreadFunc,&ThreadBase::DeleteThread,this};
    this->mThread=ee_won_thread_new(aBody);
}

}
