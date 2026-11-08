// WONAPI BlockingSocket: socket calls that run a socket op and wait for it (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006b1c10
// Connects to an address, waiting up to the timeout; returns the connect status
WONStatus BlockingSocket::Connect(const IPAddr& inAddress,unsigned long inTimeout)
{
    SmartPtr<ConnectOp> op=new ConnectOp(inAddress,this);
    op->RunBlock(inTimeout);
    return op->GetStatus();
}

// FUNCTION: WON 0x006b1f10
// Waits up to the timeout for one length-prefixed message; returns it and the receive status
WONStatus BlockingSocket::RecvMsg(ByteBufferPtr& outMessage,unsigned long inTimeout)
{
    SmartPtr<RecvMsgOp> op=new RecvMsgOp(this);
    if (op->RunBlock(inTimeout)) outMessage=op->GetMsg().get();
    return op->GetStatus();
}

// FUNCTION: WON 0x006b2040
// Waits up to the timeout for one CR-LF terminated line; returns it and the receive status
WONStatus BlockingSocket::RecvCRMsg(ByteBufferPtr& outMessage,unsigned long inTimeout)
{
    SmartPtr<RecvCRMsgOp> op=new RecvCRMsgOp(this);
    if (op->RunBlock(inTimeout)) outMessage=op->GetMsg();
    return op->mStatus;
}
// 2b1d00, 0x228 bytes in the original
// FUNCTION: WON 0x006b1d00
// Sends raw bytes, waiting up to the timeout; returns the send status
WONStatus BlockingSocket::SendBytes(const ByteBuffer* bytes,unsigned long timeout)
{
    SmartPtr<SendBytesOp> op=new SendBytesOp(bytes,this);
    op->RunBlock(timeout);
    return op->GetStatus();
}

// 2b1e30, 0x219 bytes in the original
// FUNCTION: WON 0x006b1e30
// Sends one length-prefixed message, waiting up to the timeout; returns the send status
WONStatus BlockingSocket::SendMsg(const ByteBuffer* message,unsigned long timeout)
{
    SmartPtr<SendMsgOp> op=new SendMsgOp(message,this);
    op->RunBlock(timeout);
    return op->GetStatus();
}

} // namespace WONAPI

namespace WONAPI {
// FUNCTION: WON 0x006b1bd0
// Creates a blocking socket of the given type (stream or datagram)
BlockingSocket::BlockingSocket(int type) : QueueSocket(type) {}
// FUNCTION: WON 0x006aab50
// Returns a new, unopened blocking socket of the same type
AsyncSocket* BlockingSocket::Duplicate()
{
    return new BlockingSocket(this->mType);
}
}

// No original address identified: WON SDK datagram send/receive calls
namespace WONAPI {
WONStatus BlockingSocket::SendBytesTo(const ByteBuffer *theBytes, const IPAddr &theAddr, DWORD theTimeout)
{
	SmartPtr<SendBytesToOp> anOp = new SendBytesToOp(theBytes,theAddr,this);
	anOp->RunBlock(theTimeout);
	return anOp->GetStatus();
}

WONStatus BlockingSocket::RecvBytesFrom(ByteBufferPtr &theBytes, unsigned short theMaxBytes, IPAddr &theAddr, DWORD theTimeout)
{
	SmartPtr<RecvBytesFromOp> anOp = new RecvBytesFromOp(theMaxBytes,this);
	if(anOp->RunBlock(theTimeout))
	{
		theAddr = anOp->GetAddr();
		theBytes = anOp->GetBytes();
	}
	
	return anOp->GetStatus();
}
}

// No original address identified: WON SDK blocking byte receive
namespace WONAPI {
WONStatus BlockingSocket::RecvBytes(ByteBufferPtr &theBytes, DWORD theNumBytes, DWORD theTimeout)
{
	SmartPtr<RecvBytesOp> anOp = new RecvBytesOp(theNumBytes,this);
	if(anOp->RunBlock(theTimeout))
		theBytes = anOp->GetBytes();

	return anOp->GetStatus();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aa960
// (public: virtual void * __thiscall WONAPI::SendBytesToOp::`scalar deleting destructor'(unsigned int)    [??_GSendBytesToOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006aa980
// (public: virtual __thiscall WONAPI::SendBytesToOp::~SendBytesToOp(void)    [??1SendBytesToOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ab470
// (public: virtual __thiscall WONAPI::RecvMsgOp::~RecvMsgOp(void)    [??1RecvMsgOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ab5a0
// (public: virtual __thiscall WONAPI::RecvBytesOp::~RecvBytesOp(void)    [??1RecvBytesOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006abad0
// (public: virtual __thiscall WONAPI::SendBytesOp::~SendBytesOp(void)    [??1SendBytesOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006abb80
// (public: virtual __thiscall WONAPI::SendMsgOp::~SendMsgOp(void)    [??1SendMsgOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006b1df0
// (public: class WONAPI::SmartPtr<class WONAPI::ByteBuffer> __thiscall WONAPI::RecvCRMsgOp::GetMsg(void)const     [?GetMsg@RecvCRMsgOp@WONAPI@@QBE?AV?$Sm)
// TEMPLATE: WON 0x006b2180
// (public: virtual class WONAPI::AsyncOp * __thiscall WONAPI::RecvCRMsgOp::Duplicate(void)    [?Duplicate@RecvCRMsgOp@WONAPI@@UAEPAVAsyncOp@2@XZ])
// TEMPLATE: WON 0x006b2280
// (public: virtual __thiscall WONAPI::RecvCRMsgOp::~RecvCRMsgOp(void)    [??1RecvCRMsgOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b1bf0
// (public: virtual void * __thiscall WONAPI::BlockingSocket::`scalar deleting destructor'(unsigned int)    [??_GBlockingSocket@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b2260
// (public: virtual void * __thiscall WONAPI::RecvCRMsgOp::`scalar deleting destructor'(unsigned int)    [??_GRecvCRMsgOp@WONAPI@@UAEPAXI@Z])
