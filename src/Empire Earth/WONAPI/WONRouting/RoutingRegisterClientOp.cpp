// WONAPI routing op that registers the client with the routing server
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
// FUNCTION: WON 0x006c8450
// Creates a register request with no reconnect id and no flags
RoutingRegisterClientOp::RoutingRegisterClientOp(RoutingConnection* inConnection)
    : RoutingOp(inConnection)
{
    this->mReconnectId = 0;
    this->mAsyncMessageFlags = 0;
    this->mRegisterFlags = 0;
    this->mClientFlags = 0;
    this->mClientId = 0;
}

// FUNCTION: WON 0x006c8680
// Creates a register request with the client name, server password, reconnect id and flags
RoutingRegisterClientOp::RoutingRegisterClientOp(RoutingConnection* inConnection,
    const std::wstring& inName,const std::wstring& inPassword,
    unsigned long inReconnectId,unsigned long inAsyncFlags,
    unsigned long inRegisterFlags,unsigned long inClientFlags)
    : RoutingOp(inConnection),mClientName(inName),mServerPassword(inPassword),
      mReconnectId(inReconnectId),mAsyncMessageFlags(inAsyncFlags),
      mRegisterFlags(inRegisterFlags),mClientFlags(inClientFlags)
{
    // The original leaves mClientId and the reply counters unset here
}

// FUNCTION: WON 0x006c8540
// Returns the op type (register client)
RoutingOpType RoutingRegisterClientOp::GetType() const { return (RoutingOpType)RoutingOp_RegisterClient; }

// FUNCTION: WON 0x006c87a0
// Sends the register request; expects a second reply (the client list) when flag 4 is set
void RoutingRegisterClientOp::SendRequest()
{
    this->mReplyCount = 0;
    this->mNumRepliesNeeded = 1;
    if (this->mRegisterFlags & 4) this->mNumRepliesNeeded = 2;
    this->InitSendMsg(52);
    this->mSendMsg.AppendWString(this->mClientName, 2);
    this->mSendMsg.AppendWString(this->mServerPassword, 2);
    this->mSendMsg.AppendLong(this->mReconnectId);
    this->mSendMsg.AppendLong(this->mAsyncMessageFlags);
    this->mSendMsg.AppendLong(this->mRegisterFlags);
    this->mSendMsg.AppendLong(this->mClientFlags);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x006c8840
// Reads the register reply (client id, name, reconnect id) and the client list; waits until all replies are in
WONStatus RoutingRegisterClientOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    WONStatus status;
    if (inMsgType != 23 && inMsgType != 74) {
        if (inMsgType == 51) {
            status = inMsg.ReadShort();
            if (status == 0) {
                this->mClientId = inMsg.ReadShort();
                inMsg.ReadWString(this->mClientName, 2);
                this->mReconnectId = inMsg.ReadLong();
            }
            ++this->mReplyCount;
        } else {
            return (WONStatus)WS_RoutingOp_DontWantReply;
        }
    } else {
        if (!(this->mRegisterFlags & 0x14))
            return (WONStatus)WS_RoutingOp_DontWantReply;
        SmartPtr<RoutingGetClientListOp> op = new RoutingGetClientListOp(this->mConnection.get());
        status = op->HandleReply(inMsgType, inMsg);
        this->mClientMap = op->mClientMap;
        ++this->mReplyCount;
    }
    if (status == 0 && this->mReplyCount != this->mNumRepliesNeeded) return (WONStatus)WS_RoutingOp_NeedMoreReplies;
    return status;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8570
// (public: virtual __thiscall WONAPI::RoutingRegisterClientOp::~RoutingRegisterClientOp(void)    [??1RoutingRegisterClientOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8550
// (public: virtual void * __thiscall WONAPI::RoutingRegisterClientOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingRegisterClientOp@WONAPI@@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c96c0
// (protected: static struct std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingClientInfo> >,s)
