// Routing op holding a derived server's message payload, and the unsolicited derived-op factory
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRoutingDerivedServerOp.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006c5260
// Keeps the message type and the rest of the message as the op's payload; always succeeds
WONStatus RoutingDerivedServerOp::HandleReply(unsigned char messageType,ReadBuffer& message)
{
    this->mMsgType=messageType;
    this->mRecvMsg=new ByteBuffer(message.data()+message.pos(),message.Available(),false);
    return WS_Success;
}
// FUNCTION: WON 0x006bc6c0
// Returns a new derived-server op for an unsolicited message when all derived ops count as unsolicited, else NULL
SmartPtr<RoutingOp> RoutingConnection::GetUnsolicitedDerivedOp(unsigned char)
{
    if(this->mAllDerivedOpsAreUnsolicited)
        return new RoutingDerivedServerOp(this);
    return NULL;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006bc790
// (public: virtual void * __thiscall WONAPI::RoutingDerivedServerOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingDerivedServerOp@WONAPI@@UA)
// TEMPLATE: WON 0x006bc7b0
// (public: virtual __thiscall WONAPI::RoutingDerivedServerOp::~RoutingDerivedServerOp(void)    [??1RoutingDerivedServerOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006bc780
// (public: virtual long __thiscall WONAPI::RoutingDerivedServerOp::GetType(void)const )
