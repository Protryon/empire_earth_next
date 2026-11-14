// WONAPI BlockingSocket: a queue socket the WON ops make (its blocking calls are the op base's, ee_won hostsock.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006b1bd0
// Creates a blocking socket of the given type (stream or datagram)
BlockingSocket::BlockingSocket(int type) : QueueSocket(type) {}
// FUNCTION: WON 0x006aab50
// Returns a new, unopened blocking socket of the same type
AsyncSocket* BlockingSocket::Duplicate()
{
    return new BlockingSocket(this->GetType());
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
