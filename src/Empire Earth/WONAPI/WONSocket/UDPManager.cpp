// WONAPI UDPManager: one UDP socket shared by receive ops matched by sender, plus send/retry ops; the manager is a
// Rust object since round 15 (rust/crates/ee_won/src/udpmgr.rs), its receive and request ops since round 16
// (udpops.rs)
// (file grouping inferred)
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

// UDPManager is a Rust object since round 15 (rust/crates/ee_won/src/udpmgr.rs EeWonUdpManager: the socket, the
// default completion, the auto-receive op, the address-less list and the by-sender multimap with their clean-up
// cursors, the receive size, and Bind, Close, SetAutoRecv, QueueOp, Send, CleanOps, the erases and RecvCallback's
// dispatch); these are the class's shims and the C++ side the object calls back (the ops, their checks and
// completions, the socket)
static RefCount* AsRef(const ByteBuffer* inBytes) { return const_cast<ByteBuffer*>(inBytes); }
static UDPManagerRecvOp* RecvOpOf(void* inOp) { return static_cast<UDPManagerRecvOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp))); }
static AsyncOp* OpOf(void* inOp) { return static_cast<AsyncOp*>(static_cast<RefCount*>(inOp)); }
static QueueSocket* SocketOf(void* inSocket) { return static_cast<QueueSocket*>(static_cast<RefCount*>(inSocket)); }

static void UdpRefAdd(void* inObject) { static_cast<RefCount*>(inObject)->AddRef(); }
static void UdpRefRelease(void* inObject) { static_cast<RefCount*>(inObject)->Release(); }
static long UdpOpStatus(void* inOp) { return OpOf(inOp)->GetStatus(); }
static void* UdpOpCompletion(void* inOp) { return static_cast<RefCount*>(OpOf(inOp)->GetCompletion()); }
static void UdpOpSetCompletion(void* inOp, void* inCompletion)
{ OpOf(inOp)->SetCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(static_cast<RefCount*>(inCompletion))); }
static void UdpOpRunAsync(void* inOp, long inTimeout) { OpOf(inOp)->RunAsync(inTimeout); }
static void UdpOpKill(void* inOp) { OpOf(inOp)->Kill(true); }
static void UdpOpFinish(void* inOp, long inStatus) { OpOf(inOp)->Finish(inStatus); }
static void UdpOpForceFinish(void* inOp, long inStatus) { OpOf(inOp)->ForceFinish(inStatus); }
static uint32_t UdpOpTimeLeft(void* inOp) { return static_cast<uint32_t>(OpOf(inOp)->TimeLeft()); }
static EeWonIpAddr* UdpRecvAddress(void* inOp) { return ee_won_ipaddr_clone(RecvOpOf(inOp)->GetFromAddr().GetValue()); }
static void UdpRecvSetAddress(void* inOp, const EeWonIpAddr* inAddr)
{ RecvOpOf(inOp)->SetFromAddr(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr))); }
static void UdpRecvSetMsg(void* inOp, void* inBytes)
{ RecvOpOf(inOp)->SetMsg(static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBytes))); }
static long UdpRecvCheck(void* inOp, const uint8_t* inData, size_t inLength, bool* outThrew)
{
    *outThrew = false;
    try {
        ReadBuffer aMessage(inData, static_cast<unsigned long>(inLength));
        return RecvOpOf(inOp)->CheckMessage(aMessage);
    }
    catch (ReadBufferException&) {
        *outThrew = true;
        return 0;
    }
}
static void* UdpNewRecvOp(const EeWonIpAddr* inAddr)
{
    UDPManagerRecvOp* anOp = inAddr != NULL ? new UDPManagerRecvOp(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)))
                                            : new UDPManagerRecvOp;
    anOp->AddRef();
    return static_cast<RefCount*>(anOp);
}
static void* UdpSocketNew()
{
    QueueSocket* aSocket = new QueueSocket(1);
    aSocket->AddRef();
    return static_cast<RefCount*>(aSocket);
}
static void UdpSocketKill(void* inSocket) { SocketOf(inSocket)->Kill(); }
static void UdpSocketSetBroadcast(void* inSocket, bool inOn) { SocketOf(inSocket)->SetBroadcast(inOn); }
static long UdpSocketBind(void* inSocket, int32_t inPort)
{ return SocketOf(inSocket)->Fn2B0D90(static_cast<unsigned short>(inPort), 0); }
static void UdpSocketRepeat(void* inSocket, void* inManager, int32_t inSize)
{
    QueueSocket* aSocket = SocketOf(inSocket);
    aSocket->SetRepeatCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManager::StaticRecvCallback,
                                                                                      static_cast<UDPManager*>(inManager)));
    aSocket->SetRepeatOp(new RecvBytesFromOp(static_cast<unsigned short>(inSize), NULL));
}
static void UdpSocketSend(void* inSocket, void* inBytes, const EeWonIpAddr* inAddr)
{
    SocketOf(inSocket)->QueueOp(new SendBytesToOp(static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBytes)),
                                                  IPAddr::FromValue(ee_won_ipaddr_clone(inAddr))), -1);
}
static const EeWonUdpHooks kUdpHooks = {UdpRefAdd, UdpRefRelease, UdpOpStatus, UdpOpCompletion, UdpOpSetCompletion,
                                        UdpOpRunAsync, UdpOpKill, UdpOpFinish, UdpOpForceFinish, UdpOpTimeLeft,
                                        UdpRecvAddress, UdpRecvSetAddress, UdpRecvSetMsg, UdpRecvCheck, UdpNewRecvOp,
                                        UdpSocketNew, UdpSocketKill, UdpSocketSetBroadcast, UdpSocketBind,
                                        UdpSocketRepeat, UdpSocketSend};

// FUNCTION: WON 0x006b76b0
// Creates an unbound UDP manager with 1024-byte receives and auto-receive off
UDPManager::UDPManager() : mManager(ee_won_udpmanager_new(static_cast<void*>(this), &kUdpHooks)) {}

// The object's references go (the original's implicit destructor closed nothing)
UDPManager::~UDPManager() { ee_won_udpmanager_free(this->mManager); }

void UDPManager::SetDefaultCompletion(CompletionBase<AsyncOpPtr>* theCompletion)
{ ee_won_udpmanager_set_default_completion(this->mManager, static_cast<RefCount*>(theCompletion)); }

QueueSocket* UDPManager::GetSocket() const { return SocketOf(ee_won_udpmanager_socket(this->mManager)); }

// FUNCTION: WON 0x006b78e0
// Kills the socket, the auto-receive op and every waiting receive op and forgets them
void UDPManager::Close() { ee_won_udpmanager_close(this->mManager); }

// FUNCTION: WON 0x006b7b40
// Turns auto-receive on (unmatched datagrams finish a catch-all op) or off
void UDPManager::SetAutoRecv(bool inAutoRecv) { ee_won_udpmanager_set_auto_recv(this->mManager, inAutoRecv); }

// FUNCTION: WON 0x006b7c50
// Registers a receive op (by sender address if it has one) and starts it; kills it at once when unbound
void UDPManager::QueueOp(UDPManagerRecvOp* inOp, unsigned long inTimeout)
{ ee_won_udpmanager_queue_op(this->mManager, static_cast<RefCount*>(inOp), static_cast<uint32_t>(inTimeout)); }

// FUNCTION: WON 0x006b7ec0
// Queues a datagram to an address on the manager's socket
void UDPManager::Send(const ByteBuffer* inData, const IPAddr& inAddress)
{ ee_won_udpmanager_send(this->mManager, AsRef(inData), inAddress.GetValue()); }

// FUNCTION: WON 0x006b8000
// Drops up to 10 finished ops from the list and from the map, resuming where the last clean-up stopped
void UDPManager::CleanOps() { ee_won_udpmanager_clean_ops(this->mManager); }


// The receive and request ops are Rust objects since round 16 (rust/crates/ee_won/src/udpops.rs EeWonUdpRecvOp and
// EeWonUdpSendRecvOp: their members and RunHook, CleanupHook, QueueRecv, RecvCallback and SendRequest); these are
// the classes' shims and the C++ side the objects call back (the AsyncOp, the virtual check, the sockets and their
// ops, the manager, the reply wait)
static RefCount* AsRefOp(AsyncOp* inOp) { return static_cast<RefCount*>(inOp); }
static UDPManagerSendRecvOp* SendRecvOf(void* inOp)
{ return static_cast<UDPManagerSendRecvOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inOp))); }
static UDPManager* ManagerOf(void* inManager) { return static_cast<UDPManager*>(static_cast<RefCount*>(inManager)); }
template <class T> static T* ObjectOf(void* inObject) { return static_cast<T*>(static_cast<RefCount*>(inObject)); }

static int UdpOpMode(void* inOp) { return static_cast<int>(OpOf(inOp)->GetMode()); }
static long UdpOpTimeout(void* inOp) { return static_cast<long>(OpOf(inOp)->GetTimeout()); }
static void* UdpSocketNewBound()
{
    QueueSocket* aSocket = new QueueSocket(1);
    aSocket->AddRef();
    aSocket->Fn2B0D90(0, 0);
    return static_cast<RefCount*>(aSocket);
}
static void UdpSocketQueueRecv(void* inSocket, void* inOp)
{
    SmartPtr<RecvBytesFromOp> anOp = new RecvBytesFromOp(1024, NULL);
    anOp->SetCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManagerRecvOp::StaticRecvCallback,
                                                                             RecvOpOf(inOp)));
    SocketOf(inSocket)->QueueOp(anOp.get(), -1);
}
static void UdpManagerQueue(void* inManager, void* inOp, uint32_t inTimeout)
{ ManagerOf(inManager)->QueueOp(RecvOpOf(inOp), inTimeout); }
static void UdpManagerSend(void* inManager, void* inBytes, const EeWonIpAddr* inAddr)
{ ManagerOf(inManager)->Send(ObjectOf<ByteBuffer>(inBytes), IPAddr::FromValue(ee_won_ipaddr_clone(inAddr))); }
static void* UdpNewWait(void* inRequest, const EeWonIpAddr* inAddr)
{
    UDPManagerSendRecvOp* aRequest = SendRecvOf(inRequest);
    UDPManagerRecvOp* anOp = new UDPManagerRecvOp(IPAddr::FromValue(ee_won_ipaddr_clone(inAddr)));
    anOp->AddRef();
    anOp->SetCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManagerSendRecvOp::StaticRecvCallback,
                                                                             aRequest));
    anOp->SetMessageVerifier(UDPManagerSendRecvOp::StaticCheckResponse, aRequest);
    return AsRefOp(anOp);
}
static const EeWonUdpRecvOp* UdpRecvObject(void* inOp) { return RecvOpOf(inOp)->GetUdpObject(); }
static const EeWonUdpOpHooks kUdpOpHooks = {UdpRefAdd, UdpRefRelease, UdpOpMode, UdpOpTimeout, UdpOpStatus,
                                            UdpOpFinish, UdpOpRunAsync, UdpOpKill, UdpRecvCheck, UdpSocketNewBound,
                                            UdpSocketKill, UdpSocketQueueRecv, UdpSocketSend, UdpManagerQueue,
                                            UdpManagerSend, UdpNewWait, UdpRecvObject};

// FUNCTION: WON 0x006b68b0
// Creates a receive op that accepts a datagram from any sender
UDPManagerRecvOp::UDPManagerRecvOp() : mUdp(ee_won_udprecvop_new(AsRefOp(this), &kUdpOpHooks, NULL)) {}

// FUNCTION: WON 0x006b6a10
// Creates a receive op that waits for a datagram from the given address
UDPManagerRecvOp::UDPManagerRecvOp(const IPAddr& inAddress)
    : mUdp(ee_won_udprecvop_new(AsRefOp(this), &kUdpOpHooks, inAddress.GetValue())) {}

// The object's references go (the original's implicit destructor released the members)
UDPManagerRecvOp::~UDPManagerRecvOp() { ee_won_udprecvop_free(this->mUdp); }

bool UDPManagerRecvOp::HasVerifier() { return ee_won_udprecvop_verifier(this->mUdp) != NULL; }
void UDPManagerRecvOp::SetUDPManager(UDPManager* theManager)
{ ee_won_udprecvop_set_manager(this->mUdp, static_cast<RefCount*>(theManager)); }
void UDPManagerRecvOp::SetSocket(QueueSocket* theSocket)
{ ee_won_udprecvop_set_socket(this->mUdp, static_cast<RefCount*>(theSocket)); }
IPAddr UDPManagerRecvOp::GetFromAddr() const { return IPAddr::FromValue(ee_won_udprecvop_address(this->mUdp)); }
void UDPManagerRecvOp::SetFromAddr(const IPAddr& theAddr) { ee_won_udprecvop_set_address(this->mUdp, theAddr.GetValue()); }
const ByteBuffer* UDPManagerRecvOp::GetMsg() const
{
    void* aBytes = ee_won_udprecvop_msg(this->mUdp);
    return aBytes != NULL ? ObjectOf<ByteBuffer>(aBytes) : NULL;
}
void UDPManagerRecvOp::SetMsg(const ByteBuffer* theMsg) { ee_won_udprecvop_set_msg(this->mUdp, AsRef(theMsg)); }
QueueSocket* UDPManagerRecvOp::GetSocket() const
{
    void* aSocket = ee_won_udprecvop_socket(this->mUdp);
    return aSocket != NULL ? SocketOf(aSocket) : NULL;
}
UDPManager* UDPManagerRecvOp::GetUDPManager() const
{
    void* aManager = ee_won_udprecvop_manager(this->mUdp);
    return aManager != NULL ? ManagerOf(aManager) : NULL;
}

// FUNCTION: WON 0x006b6bf0
// Sets the callback (and its context) that decides whether a datagram answers this op
void UDPManagerRecvOp::SetMessageVerifier(MessageVerifier inVerifier, void* inContext)
{ ee_won_udprecvop_set_verifier(this->mUdp, reinterpret_cast<void*>(inVerifier), inContext); }

// FUNCTION: WON 0x006b6c10
// Asks the message check callback whether a datagram answers this op (WS_None = no); without a callback accepts all
WONStatus UDPManagerRecvOp::CheckMessage(ReadBuffer& inMessage)
{
    MessageVerifier aVerifier = reinterpret_cast<MessageVerifier>(ee_won_udprecvop_verifier(this->mUdp));
    return aVerifier ? aVerifier(inMessage, ee_won_udprecvop_verifier_context(this->mUdp)) : 0;
}

// FUNCTION: WON 0x006b6d70
// Forwards a successful datagram receive to its receive op
void UDPManagerRecvOp::StaticRecvCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    if (!inOp->Succeeded())
        return;
    static_cast<UDPManagerRecvOp*>(static_cast<AsyncOp*>(inParam.get()))
        ->RecvCallback(static_cast<RecvBytesFromOp*>(inOp.get()));
}

// FUNCTION: WON 0x006b6c30
// Queues a receive of one datagram (up to 1024 bytes) on the op's own socket
void UDPManagerRecvOp::QueueRecv() { ee_won_udprecvop_queue_recv(this->mUdp); }

// FUNCTION: WON 0x006b6e60
// Finishes the op with a datagram from the expected sender that its message check accepts; otherwise receives again
void UDPManagerRecvOp::RecvCallback(RecvBytesFromOp* inOp)
{
    ByteBufferPtr aBuffer = inOp->GetBytes();
    IPAddr aFrom = inOp->GetAddr();
    const ByteBuffer* aBytes = aBuffer.get();
    ee_won_udprecvop_recv(this->mUdp, aBytes != NULL ? reinterpret_cast<const uint8_t*>(aBytes->data()) : NULL,
                          aBytes != NULL ? aBytes->length() : 0, aFrom.GetValue());
}

// FUNCTION: WON 0x006b6f60
// Kills the op's own socket when the op ends
void UDPManagerRecvOp::CleanupHook() { ee_won_udprecvop_cleanup(this->mUdp); }

// FUNCTION: WON 0x006b6fb0
// Runs the op: refused in blocking mode; registers with the manager, or binds its own UDP socket and receives
void UDPManagerRecvOp::RunHook() { ee_won_udprecvop_run(this->mUdp); }

// FUNCTION: WON 0x006b70b0
// Creates a request/reply op with no retries and a 10-second reply wait
UDPManagerSendRecvOp::UDPManagerSendRecvOp() : mUdp(ee_won_udpsendrecvop_new(AsRefOp(this), &kUdpOpHooks)) {}

// The object's references go (the original's implicit destructor released the members)
UDPManagerSendRecvOp::~UDPManagerSendRecvOp() { ee_won_udpsendrecvop_free(this->mUdp); }

void UDPManagerSendRecvOp::SetUDPManager(UDPManager* theManager)
{ ee_won_udpsendrecvop_set_manager(this->mUdp, static_cast<RefCount*>(theManager)); }
void UDPManagerSendRecvOp::SetMsgTimeout(DWORD theTimeout)
{ ee_won_udpsendrecvop_set_timeout(this->mUdp, static_cast<uint32_t>(theTimeout)); }
void UDPManagerSendRecvOp::SetNumRetransmits(int theNum) { ee_won_udpsendrecvop_set_retry_limit(this->mUdp, theNum); }
void UDPManagerSendRecvOp::SetDestAddr(const IPAddr& theAddr)
{ ee_won_udpsendrecvop_set_address(this->mUdp, theAddr.GetValue()); }
IPAddr UDPManagerSendRecvOp::GetDestAddr() const { return IPAddr::FromValue(ee_won_udpsendrecvop_address(this->mUdp)); }
void UDPManagerSendRecvOp::SetOutgoing(const ByteBuffer* theMsg) { ee_won_udpsendrecvop_set_outgoing(this->mUdp, AsRef(theMsg)); }
const ByteBuffer* UDPManagerSendRecvOp::GetOutgoing() const
{
    void* aBytes = ee_won_udpsendrecvop_outgoing(this->mUdp);
    return aBytes != NULL ? ObjectOf<ByteBuffer>(aBytes) : NULL;
}
QueueSocket* UDPManagerSendRecvOp::GetSocket() const
{
    void* aSocket = ee_won_udpsendrecvop_socket(this->mUdp);
    return aSocket != NULL ? SocketOf(aSocket) : NULL;
}
UDPManagerRecvOp* UDPManagerSendRecvOp::GetRecvOp() const
{
    void* anOp = ee_won_udpsendrecvop_recv_op(this->mUdp);
    return anOp != NULL ? RecvOpOf(anOp) : NULL;
}
int UDPManagerSendRecvOp::GetAttemptCount() const { return ee_won_udpsendrecvop_attempts(this->mUdp); }
int UDPManagerSendRecvOp::GetNumRetransmits() const { return ee_won_udpsendrecvop_retry_limit(this->mUdp); }
unsigned long UDPManagerSendRecvOp::GetMsgTimeout() const { return ee_won_udpsendrecvop_timeout(this->mUdp); }

// FUNCTION: WON 0x006b7250
// Forwards a datagram to the send/receive op's response check
WONStatus UDPManagerSendRecvOp::StaticCheckResponse(ReadBuffer& inMessage, void* inParam)
{
    return static_cast<UDPManagerSendRecvOp*>(inParam)->CheckResponse(inMessage);
}
// FUNCTION: WON 0x006b7550
// Forwards the end of a reply wait to its send/receive op
void UDPManagerSendRecvOp::StaticRecvCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    static_cast<UDPManagerSendRecvOp*>(static_cast<AsyncOp*>(inParam.get()))->RecvCallback(inOp.get());
}

// FUNCTION: WON 0x006b7260
// Sends the request datagram (via the manager or the op's socket) and waits for a matching reply
void UDPManagerSendRecvOp::SendRequest() { ee_won_udpsendrecvop_send_request(this->mUdp); }

// FUNCTION: WON 0x006b7470
// Runs the op: refused in blocking mode; binds its own UDP socket without a manager, then sends the request
void UDPManagerSendRecvOp::RunHook() { ee_won_udpsendrecvop_run(this->mUdp); }

// FUNCTION: WON 0x006b75e0
// Finishes with the reply's status, resends on a timeout until the retry limit, then finishes with WS_NoReply
void UDPManagerSendRecvOp::RecvCallback(AsyncOp* inOp) { ee_won_udpsendrecvop_recv_done(this->mUdp, AsRefOp(inOp)); }

// FUNCTION: WON 0x006b7630
// Kills the reply wait and the op's socket when the op ends
void UDPManagerSendRecvOp::CleanupHook() { ee_won_udpsendrecvop_cleanup(this->mUdp); }
// FUNCTION: WON 0x006b8230
// Forwards a successful datagram receive to its UDP manager
void UDPManager::StaticRecvCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    if (!inOp->Succeeded())
        return;
    static_cast<UDPManager*>(inParam.get())->RecvCallback(static_cast<RecvBytesFromOp*>(inOp.get()));
}




// FUNCTION: WON 0x006b79c0
// Rebinds the manager to a local port (optionally broadcast) and starts receiving datagrams repeatedly
WONStatus UDPManager::Bind(int inPort, bool inBroadcast)
{
    return ee_won_udpmanager_bind(this->mManager, inPort, inBroadcast);
}

// FUNCTION: WON 0x006b8320
// Gives a datagram to the first waiting op for that sender whose message check accepts it, then to address-less ops,
// else finishes the auto-receive op with it and arms a new one
void UDPManager::RecvCallback(RecvBytesFromOp* inOp)
{
    ByteBufferPtr aBuffer = inOp->GetBytes();
    IPAddr aFrom = inOp->GetAddr();
    const ByteBuffer* aBytes = aBuffer.get();
    ee_won_udpmanager_recv(this->mManager, AsRef(aBytes),
                           aBytes != NULL ? reinterpret_cast<const uint8_t*>(aBytes->data()) : NULL,
                           aBytes != NULL ? aBytes->length() : 0, aFrom.GetValue());
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b6930
// (public: virtual __thiscall WONAPI::UDPManagerRecvOp::~UDPManagerRecvOp(void)    [??1UDPManagerRecvOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006b7140
// (public: virtual __thiscall WONAPI::UDPManagerSendRecvOp::~UDPManagerSendRecvOp(void)    [??1UDPManagerSendRecvOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006b7790
// (public: virtual void * __thiscall WONAPI::UDPManager::`scalar deleting destructor'(unsigned int)    [??_GUDPManager@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b7e30
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> >::~pair<class WONAPI::IPAddr const ,c)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b6910
// (public: virtual void * __thiscall WONAPI::UDPManagerRecvOp::`scalar deleting destructor'(unsigned int)    [??_GUDPManagerRecvOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b7120
// (public: virtual void * __thiscall WONAPI::UDPManagerSendRecvOp::`scalar deleting destructor'(unsigned int)    [??_GUDPManagerSendRecvOp@WONAPI@@UAEPAX)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b77b0
// (public: virtual __thiscall WONAPI::UDPManager::~UDPManager(void)    [??1UDPManager@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b8c10
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> >,st)
// TEMPLATE: WON 0x006b9570
// (public: struct std::pair<class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPMa)
// TEMPLATE: WON 0x006b95f0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> >,st)
// TEMPLATE: WON 0x006b9e10
// (protected: struct std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> )
// TEMPLATE: WON 0x006b9510
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::UDP)
// TEMPLATE: WON 0x006b9ad0
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManage)
// TEMPLATE: WON 0x006b9b10
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> >)
// TEMPLATE: WON 0x006b9fa0
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp> > *,struct std::pair<)
// TEMPLATE: WON 0x006b9980
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::UDPManagerRecvOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::)
