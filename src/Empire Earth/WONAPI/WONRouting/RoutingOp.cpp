// WONAPI routing op base: message framing, sending and record factories
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c7bc0
// Starts a new outgoing routing message with the header for this message type (routing or derived service)
void RoutingOp::InitSendMsg(unsigned char inType)
{
    this->mSendMsg.Reset();
    this->mSendMsg.AppendByte(3);
    this->mSendMsg.AppendByte(this->mIsDerivedServerOp ? 8 : 7);
    this->mSendMsg.AppendByte(inType);
}

// FUNCTION: WON 0x6c7c00
// Queues the built message on the connection for sending
void RoutingOp::SendMsg()
{
    this->mConnection->QueueOp(new SendMsgOp(this->mSendMsg.ToByteBuffer(true).get()),-1);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006c7ab0
// Creates a routing op with no connection
RoutingOp::RoutingOp() : mSendMsg(0,true)
{
    this->mIsDerivedServerOp=false;
}
// FUNCTION: WON 0x006c7b40
// Creates a routing op on a connection, using its message length field size
RoutingOp::RoutingOp(RoutingConnection* connection)
    :mSendMsg(connection->GetLengthFieldSize(),true),mConnection(connection)
{
    this->mIsDerivedServerOp=false;
}

// FUNCTION: WON 0x006c7d20
// Sends the request; fails at once in blocking mode, which routing ops do not support
void RoutingOp::RunHook()
{
    if(this->mRunMode==OP_MODE_BLOCK) {
        this->Finish((WONStatus)WS_BlockMode_NotSupported);
        return;
    }
    this->SendRequest();
}

void RoutingOp::SendRequest() {}
// FUNCTION: WON 0x006bcd30
// Declines every reply (base op)
WONStatus RoutingOp::HandleReply(unsigned char,ReadBuffer&) { return (WONStatus)WS_RoutingOp_DontWantReply; }
// FUNCTION: WON 0x006c7b10
// Returns op type 0 (base op)
RoutingOpType RoutingOp::GetType() const { return RoutingOp_Undefined; }
// FUNCTION: WON 0x006c7d10
// Registers the op with the connection to receive replies
void RoutingOp::AddOp() { this->mConnection->AddOp(this); }
// FUNCTION: WON 0x006c4640
// Returns a new client record from the connection's factory
RoutingClientInfoPtr RoutingOp::GetNewClientInfo() { return this->mConnection->GetNewClientInfo(); }
// FUNCTION: WON 0x006c4660
// Returns a new member record from the connection's factory
RoutingMemberInfoPtr RoutingOp::GetNewMemberInfo() { return this->mConnection->GetNewMemberInfo(); }
}

namespace WONAPI {
// FUNCTION: WON 0x006c5bb0
// Returns a new group record from the connection's factory
RoutingGroupInfoPtr RoutingOp::GetNewGroupInfo() { return this->mConnection->GetNewGroupInfo(); }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7b20
// (public: virtual void * __thiscall WONAPI::RoutingOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingOp@WONAPI@@UAEPAXI@Z])
