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
#include <string.h>
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "platform/PlatSocket.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/sockobj.h"
// The socket error codes WON reports are Winsock's, the platform layer's numbering everywhere (checked by
// ee_won_net status_codes.rs's the_socket_errors_are_winsocks_numbering)
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
    return static_cast<AsyncOp*>(inOp)->CallbackHook(static_cast<AsyncOp*>(inChild),inParam);
}
static const EeWonAsyncOpHooks kAsyncOpHooks={HookRun,HookCleanup,HookCoreQueue,HookTimerAdd,HookTimerRemove,
                                             HookCompletionAddRef,HookCompletionRelease,HookCompletionCall,
                                             HookOpAddRef,HookOpRelease,HookOpKill,HookRefAdd,HookRefRelease,
                                             HookNewCompletion,HookCallback};
const EeWonAsyncOpHooks* AsyncOpHooks() { return &kAsyncOpHooks; }

// ---- the library's core table (ee_won cppcore.rs): what the Rust objects' own calls still need the C++ for
// a C++ object's Rust object of the kind (NULL when it is not of the kind's class)
static void* CoreObjectOf(void* inObject,EeWonCoreObject inKind)
{
    RefCount* r=static_cast<RefCount*>(inObject);
    if (WonRustRef* x=dynamic_cast<WonRustRef*>(r)) return x->Get(inKind); // the HTTP connections and sessions
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
    EE_WON_CORE_OF(Context,ServerContext,GetListObject())
#undef EE_WON_CORE_OF
    default: break;
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
    IPAddr anAddr=inAddr!=NULL ? IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)) : IPAddr();
    SocketOp* anOp=new SocketOp(inKind,static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket)),
                                static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBytes)),&anAddr,inSize);
    return static_cast<RefCount*>(static_cast<AsyncOp*>(anOp));
}
static void* CoreNewQueueSocket(int32_t inType,bool inBlocking)
{ return static_cast<RefCount*>(inBlocking ? new BlockingSocket(inType) : new QueueSocket(inType)); }
static void* CoreNewUdpRecvOp(const EeWonIpAddr* inAddr)
{
    return static_cast<RefCount*>(inAddr!=NULL ? new UDPManagerRecvOp(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr))) : new UDPManagerRecvOp);
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
static void CoreAddToSocketThread(void* inOp) { ee_won_api_socket_op(static_cast<void*>(SockOp(inOp)), true); } // WONAPICore's AddToSocketThread
static void* CoreBytesAdopt(EeWonByteBuffer* inBytes) { return static_cast<RefCount*>(new ByteBuffer(inBytes)); } // (its first reference is Rust's)
// the socket's virtual Duplicate (cppcore.rs n_accept: AsyncSocket::Accept is Rust's)
static void* CoreSocketDuplicate(void* inSocket)
{
    return static_cast<RefCount*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket))->Duplicate());
}
static void CoreSocketHook(void* inSocket,int32_t inWhich)
{
    AsyncSocket* aSocket=static_cast<AsyncSocket*>(static_cast<RefCount*>(inSocket));
    if(inWhich==0) aSocket->OpenHook();
    else if(inWhich==1) aSocket->CloseHook();
    else aSocket->KillHook();
}
static void* CoreNewHttpConn() { return static_cast<RefCount*>(new WonRustRef(EeWonCoreObject::HttpConn)); }
// the routing classes' (ee_won routecpp.rs): a new op on a connection, the connection's two virtuals
static RoutingConnection* RouteConn(void* inConn)
{ return static_cast<RoutingConnection*>(static_cast<AsyncSocket*>(static_cast<RefCount*>(inConn))); }
static void* CoreNewRouteOp(void* inConn,int32_t inType)
{
    return static_cast<RefCount*>(static_cast<AsyncOp*>(new RoutingOp(inConn!=NULL ? RouteConn(inConn) : NULL,inType)));
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
static long CoreSocketOpContinue(void* inOp)
{
    return static_cast<SocketOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp)))->Continue();
}
static const EeWonCoreHooks kCoreHooks={HookRefAdd,HookRefRelease,HookCompletionCall,CoreObjectOf,
                                        CoreSocketOpDuplicate,CoreNewOp,HookNewCompletion,CoreNewSocketOp,
                                        CoreNewQueueSocket,CoreNewUdpRecvOp,CoreUdpCheck,
                                        reinterpret_cast<void*>(&UDPManagerSendRecvOp::StaticCheckResponse),
                                        CoreBindLocal,CoreBytesOf,CoreSocketOpContinue,CoreSocketOpStart,
                                        CoreAddToSocketThread,CoreBytesAdopt,NULL /* the transform: cppcore.rs n_transform */,
                                        CoreSocketDuplicate,CoreSocketHook,CoreNewRouteOp,CoreRouteVirtual,CoreNewHttpConn};
const EeWonCoreHooks* WonCore() { return &kCoreHooks; }
// the Rust objects' maker of a Rust op's C++ op (cppcore.rs new_host_op: the peer auth and HTTP get ops a Rust op
// makes, GameSpySupport's service lookup; ServerOp.cpp's until R6 WON lobby and network round 38)
static void* NewHostOp(EeWonHostOp* inOp) { return static_cast<RefCount*>(static_cast<AsyncOp*>(new RustServerOp(inOp))); }
[[maybe_unused]] static const bool sHostOpMaker = (ee_won_core_set_new_host_op(NewHostOp), true);

}
