// WONAPI UDPManager: one UDP socket shared by receive ops matched by sender, plus send/retry ops
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

// FUNCTION: WON 0x006b76b0
// Creates an unbound UDP manager with 1024-byte receives and auto-receive off
UDPManager::UDPManager() : mCleanListItr(NULL), mCleanMapItr(NULL)
{
    this->mCleanListItr = this->mOpList.begin();
    this->mCleanMapItr = this->mOpMap.begin();
    this->mMaxRecvBytes = 1024;
    this->mAutoRecv = false;
}

// FUNCTION: WON 0x006b78e0
// Kills the socket, the auto-receive op and every waiting receive op and forgets them
void UDPManager::Close()
{
    if (this->mSocket.get() == NULL)
        return;
    this->mSocket->Kill();
    this->mSocket = NULL;
    if (this->mAutoRecvOp.get() != NULL) {
        this->mAutoRecvOp->Kill(true);
        this->mAutoRecvOp = NULL;
    }
    for (OpList::iterator aListItr = this->mOpList.begin(); aListItr != this->mOpList.end(); ++aListItr)
        (*aListItr)->Kill(true);
    for (OpMap::iterator aMapItr = this->mOpMap.begin(); aMapItr != this->mOpMap.end(); ++aMapItr)
        aMapItr->second->Kill(true);
    this->mOpList.clear();
    this->mOpMap.clear();
    this->mCleanListItr = this->mOpList.end();
    this->mCleanMapItr = this->mOpMap.end();
}

// FUNCTION: WON 0x006b7b40
// Turns auto-receive on (unmatched datagrams finish a catch-all op) or off
void UDPManager::SetAutoRecv(bool inAutoRecv)
{
    if (inAutoRecv) {
        if (this->mAutoRecvOp.get() == NULL)
            this->mAutoRecvOp = new UDPManagerRecvOp;
    }
    else
        this->mAutoRecvOp = NULL;
}

// FUNCTION: WON 0x006b7c50
// Registers a receive op (by sender address if it has one) and starts it; kills it at once when unbound
void UDPManager::QueueOp(UDPManagerRecvOp* inOp, unsigned long inTimeout)
{
    if (inOp->mCompletion.get() == NULL)
        inOp->mCompletion = this->mDefaultCompletion;
    if (this->mSocket.get() == NULL) {
        if (inOp->GetStatus() != WS_Pending)
            inOp->RunAsync(inTimeout);
        inOp->Kill(true);
        return;
    }
    this->CleanOps();
    if (inOp->mAddress.IsValid())
        this->mOpMap.insert(OpMap::value_type(inOp->mAddress, inOp));
    else
        this->mOpList.push_back(inOp);
    if (inOp->GetStatus() != WS_Pending)
        inOp->RunAsync(inTimeout);
}

// FUNCTION: WON 0x006b7ec0
// Queues a datagram to an address on the manager's socket
void UDPManager::Send(const ByteBuffer* inData, const IPAddr& inAddress)
{
    this->mSocket->QueueOp(new SendBytesToOp(inData, inAddress), -1);
}

// FUNCTION: WON 0x006b8000
// Drops up to 10 finished ops from the list and from the map, resuming where the last clean-up stopped
void UDPManager::CleanOps()
{
    for (int i = 0; i < 10; ++i) {
        if (this->mCleanListItr == this->mOpList.end()) {
            this->mCleanListItr = this->mOpList.begin();
            break;
        }
        if ((*this->mCleanListItr)->GetStatus() != 100000)
            this->mOpList.erase(this->mCleanListItr++);
        else
            ++this->mCleanListItr;
    }
    for (int j = 0; j < 10; ++j) {
        if (this->mCleanMapItr == this->mOpMap.end()) {
            this->mCleanMapItr = this->mOpMap.begin();
            break;
        }
        if (this->mCleanMapItr->second->GetStatus() != 100000)
            this->mOpMap.erase(this->mCleanMapItr++);
        else
            ++this->mCleanMapItr;
    }
}


// FUNCTION: WON 0x006b6bf0
// Sets the callback (and its context) that decides whether a datagram answers this op
void UDPManagerRecvOp::SetMessageVerifier(MessageVerifier inVerifier, void* inContext)
{
    this->mVerifier = inVerifier;
    this->mVerifierContext = inContext;
}
// FUNCTION: WON 0x006b6c10
// Asks the message check callback whether a datagram answers this op (WS_None = no); without a callback accepts all
WONStatus UDPManagerRecvOp::CheckMessage(ReadBuffer& inMessage)
{
    return this->mVerifier ? this->mVerifier(inMessage, this->mVerifierContext) : 0;
}
// FUNCTION: WON 0x006b6d70
// Forwards a successful datagram receive to its receive op
void UDPManagerRecvOp::StaticRecvCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    if (!inOp->Succeeded())
        return;
    static_cast<UDPManagerRecvOp*>(inParam.get())->RecvCallback(static_cast<RecvBytesFromOp*>(inOp.get()));
}

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
    static_cast<UDPManagerSendRecvOp*>(inParam.get())->RecvCallback(inOp.get());
}

// FUNCTION: WON 0x006b6c30
// Queues a receive of one datagram (up to 1024 bytes) on the op's own socket
void UDPManagerRecvOp::QueueRecv()
{
    if (this->mSocket.get() == NULL)
        return;
    SmartPtr<RecvBytesFromOp> anOp = new RecvBytesFromOp(1024, NULL);
    anOp->mCompletion = new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManagerRecvOp::StaticRecvCallback, this);
    this->mSocket->QueueOp(anOp.get(), -1);
}

// FUNCTION: WON 0x006b6e60
// Finishes the op with a datagram from the expected sender that its message check accepts; otherwise receives again
void UDPManagerRecvOp::RecvCallback(RecvBytesFromOp* inOp)
{
    if (this->mStatus != WS_Pending)
        return;
    if (this->mAddress.IsValid() && inOp->mFromAddress != this->mAddress) {
        this->QueueRecv();
        return;
    }
    const ByteBuffer* aBuffer = inOp->GetBytes().get();
    try {
        ReadBuffer aMessage(aBuffer->data(), aBuffer->length());
        WONStatus aStatus = this->CheckMessage(aMessage);
        if (aStatus != WS_None) {
            this->Finish(aStatus);
            return;
        }
    }
    catch (ReadBufferException&) {
    }
    this->QueueRecv();
}

// FUNCTION: WON 0x006b68b0
// Creates a receive op that accepts a datagram from any sender
UDPManagerRecvOp::UDPManagerRecvOp() { this->mVerifier = NULL; }

// FUNCTION: WON 0x006b6a10
// Creates a receive op that waits for a datagram from the given address
UDPManagerRecvOp::UDPManagerRecvOp(const IPAddr& inAddress)
{
    this->mAddress = inAddress;
    this->mVerifier = NULL;
}

// FUNCTION: WON 0x006b6f60
// Kills the op's own socket when the op ends
void UDPManagerRecvOp::CleanupHook()
{
    if (this->mSocket.get() != NULL && this->mOwnSocket) {
        this->mSocket->Kill();
        this->mSocket = NULL;
    }
}

// FUNCTION: WON 0x006b6fb0
// Runs the op: refused in blocking mode; registers with the manager, or binds its own UDP socket and receives
void UDPManagerRecvOp::RunHook()
{
    if (this->mRunMode == OP_MODE_BLOCK) {
        this->Finish(WS_BlockMode_NotSupported);
        return;
    }
    if (this->mManager.get() != NULL) {
        this->mManager->QueueOp(this, this->mTimeout);
        return;
    }
    if (this->mSocket.get() == NULL) {
        this->mSocket = new QueueSocket(1);
        this->mSocket->Fn2B0D90(0, 0);
        this->mOwnSocket = true;
    }
    this->QueueRecv();
}

// FUNCTION: WON 0x006b70b0
// Creates a request/reply op with no retries and a 10-second reply wait
UDPManagerSendRecvOp::UDPManagerSendRecvOp() : mRetryLimit(0) { this->mReceiveTimeout=10000; }

// FUNCTION: WON 0x006b7260
// Sends the request datagram (via the manager or the op's socket) and waits for a matching reply
void UDPManagerSendRecvOp::SendRequest()
{
    this->mRecvOp = new UDPManagerRecvOp(this->mAddress);
    this->mRecvOp->mCompletion = new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManagerSendRecvOp::StaticRecvCallback, this);
    this->mRecvOp->SetMessageVerifier(UDPManagerSendRecvOp::StaticCheckResponse, this);
    if (this->mManager.get() != NULL) {
        this->mManager->Send(this->mOutgoing.get(), this->mAddress);
        this->mRecvOp->SetUDPManager(this->mManager);
    }
    else {
        this->mSocket->QueueOp(new SendBytesToOp(this->mOutgoing.get(), this->mAddress, NULL), -1);
        this->mRecvOp->SetSocket(this->mSocket);
    }
    this->mRecvOp->RunAsync(this->mReceiveTimeout);
}

// FUNCTION: WON 0x006b7470
// Runs the op: refused in blocking mode; binds its own UDP socket without a manager, then sends the request
void UDPManagerSendRecvOp::RunHook()
{
    if (this->mRunMode == OP_MODE_BLOCK) {
        this->Finish(WS_BlockMode_NotSupported);
        return;
    }
    this->mAttemptCount = 0;
    if (this->mManager.get() == NULL) {
        this->mSocket = new QueueSocket(1);
        this->mSocket->Fn2B0D90(0, 0);
    }
    this->SendRequest();
}

// FUNCTION: WON 0x006b75e0
// Finishes with the reply's status, resends on a timeout until the retry limit, then finishes with WS_NoReply
void UDPManagerSendRecvOp::RecvCallback(AsyncOp* inOp)
{
    if (this->mStatus != WS_Pending || inOp != this->mRecvOp.get())
        return;
    WONStatus aStatus = inOp->GetStatus();
    if (aStatus != WS_TimedOut) {
        this->Finish(aStatus);
        return;
    }
    if (this->mAttemptCount >= this->mRetryLimit) {
        this->Finish(WS_NoReply);
        return;
    }
    ++this->mAttemptCount;
    this->SendRequest();
}

// FUNCTION: WON 0x006b7630
// Kills the reply wait and the op's socket when the op ends
void UDPManagerSendRecvOp::CleanupHook()
{
    if (this->mRecvOp.get() != NULL) {
        this->mRecvOp->Kill(true);
        this->mRecvOp = NULL;
    }
    if (this->mSocket.get() != NULL) {
        this->mSocket->Kill();
        this->mSocket = NULL;
    }
}
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
    this->Close();
    this->mSocket = new QueueSocket(1);
    if (inBroadcast)
        this->mSocket->SetBroadcast(true);
    WONStatus aStatus = this->mSocket->Fn2B0D90(inPort, 0);
    if (aStatus == 0) {
        this->mSocket->SetRepeatCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(UDPManager::StaticRecvCallback, this));
        this->mSocket->SetRepeatOp(new RecvBytesFromOp(this->mMaxRecvBytes, NULL));
    }
    return aStatus;
}


// FUNCTION: WON 0x006b8160
// Erases a list op, first moving the clean-up cursor past it
void UDPManager::Erase(const OpList::iterator& inItr)
{
    if (inItr == this->mCleanListItr)
        ++this->mCleanListItr;
    this->mOpList.erase(inItr);
}

// FUNCTION: WON 0x006b81b0
// Erases a map op, first moving the clean-up cursor past it
void UDPManager::Erase(const OpMap::iterator& inItr)
{
    if (inItr == this->mCleanMapItr)
        ++this->mCleanMapItr;
    this->mOpMap.erase(inItr);
}

// FUNCTION: WON 0x006b8320
// Gives a datagram to the first waiting op for that sender whose message check accepts it, then to address-less ops,
// else finishes the auto-receive op with it and arms a new one
void UDPManager::RecvCallback(RecvBytesFromOp* inOp)
{
    const ByteBuffer* aBuffer = inOp->GetBytes().get();
    ReadBuffer aMessage(aBuffer->data(), aBuffer->length());
    if (!this->mOpMap.empty()) {
        OpMap::iterator aMapItr = this->mOpMap.lower_bound(inOp->mFromAddress);
        while (aMapItr != this->mOpMap.end() && !(aMapItr->first != inOp->mFromAddress)) {
            UDPManagerRecvOp* anOp = aMapItr->second.get();
            if (anOp->GetStatus() != WS_Pending) {
                this->Erase(aMapItr++);
                continue;
            }
            try {
                aMessage.Rewind();
                WONStatus aStatus = anOp->CheckMessage(aMessage);
                if (aStatus == WS_ServerReq_Recv)
                    this->QueueOp(new UDPManagerRecvOp(anOp->mAddress), anOp->TimeLeft());
                else if (aStatus != WS_None) {
                    anOp->SetMsg(aBuffer);
                    anOp->Finish(aStatus);
                    this->Erase(aMapItr);
                    return;
                }
            }
            catch (ReadBufferException&) {
            }
            ++aMapItr;
        }
    }
    if (!this->mOpList.empty()) {
        OpList::iterator aListItr = this->mOpList.begin();
        while (aListItr != this->mOpList.end()) {
            UDPManagerRecvOp* anOp = aListItr->get();
            if (anOp->GetStatus() != WS_Pending) {
                this->Erase(aListItr++);
                continue;
            }
            try {
                aMessage.Rewind();
                WONStatus aStatus = anOp->CheckMessage(aMessage);
                if (aStatus != WS_None) {
                    anOp->mAddress = inOp->mFromAddress;
                    anOp->SetMsg(aBuffer);
                    anOp->Finish(aStatus);
                    this->Erase(aListItr);
                    return;
                }
            }
            catch (ReadBufferException&) {
            }
            ++aListItr;
        }
    }
    if (this->mAutoRecvOp.get() != NULL) {
        this->mAutoRecvOp->SetMsg(aBuffer);
        this->mAutoRecvOp->mAddress = inOp->GetAddr();
        this->mAutoRecvOp->SetCompletion(this->mDefaultCompletion);
        this->mAutoRecvOp->ForceFinish(WS_Success);
        this->mAutoRecvOp = new UDPManagerRecvOp;
    }
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
