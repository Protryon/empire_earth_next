// WONAPI routing request ops: message builders, status-reply handlers and type ids
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
// FUNCTION: WON 0x6c4540
// Asks the routing server to cancel a pending join of a group (optionally leaving it if already in)
void RoutingCancelJoinGroupOp::SendRequest()
{
    this->InitSendMsg(5);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendBool(this->mLeaveIfAlreadyInGroup);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingCancelJoinGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bd7a0
// Returns the op type (cancel join group)
RoutingOpType RoutingCancelJoinGroupOp::GetType() const { return RoutingOp_CancelJoinGroup; }

// FUNCTION: WON 0x6c7620
// Asks the routing server to remove this client from a group
void RoutingLeaveGroupOp::SendRequest()
{
    this->InitSendMsg(43);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingLeaveGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bd940
// Returns the op type (leave group)
RoutingOpType RoutingLeaveGroupOp::GetType() const { return RoutingOp_LeaveGroup; }

// FUNCTION: WON 0x6c8c60
// Asks the routing server to change a group's flags and async flags under the given masks
void RoutingSetGroupFlagsOp::SendRequest()
{
    this->InitSendMsg(61);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendLong(this->mGroupFlagMask);
    this->mSendMsg.AppendLong(this->mGroupFlags);
    this->mSendMsg.AppendLong(this->mAsyncFlagMask);
    this->mSendMsg.AppendLong(this->mAsyncFlags);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingSetGroupFlagsOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bdd20
// Returns the op type (set group flags)
RoutingOpType RoutingSetGroupFlagsOp::GetType() const { return RoutingOp_SetGroupFlags; }

// FUNCTION: WON 0x6c8b20
// Sends the routing server an alert text for the listed recipient clients
void RoutingSendServerAlertOp::SendRequest()
{
    this->InitSendMsg(56);
    this->mSendMsg.AppendWString(this->mAlertText, 2);
    this->mSendMsg.AppendShort((short)this->mRecipientList.size());
    std::list<unsigned short>::const_iterator i;
    i=this->mRecipientList.begin();
    while(true) {
        if(!(i != this->mRecipientList.end())) break;
        this->mSendMsg.AppendShort(*i);
        ++i;
    }
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingSendServerAlertOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bdef0
// Returns the op type (send server alert)
RoutingOpType RoutingSendServerAlertOp::GetType() const { return RoutingOp_SendServerAlert; }

// FUNCTION: WON 0x6c8cc0
// Asks the routing server to start a shutdown countdown with an alert text
void RoutingStartServerShutdownOp::SendRequest()
{
    this->InitSendMsg(65);
    this->mSendMsg.AppendWString(this->mAlertText, 2);
    this->mSendMsg.AppendLong(this->mSecondsUntilShutdown);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingStartServerShutdownOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006be120
// Returns the op type (start server shutdown)
RoutingOpType RoutingStartServerShutdownOp::GetType() const { return RoutingOp_StartServerShutdown; }


// FUNCTION: WON 0x006c4170
// Asks the routing server to abort a pending shutdown
void RoutingAbortServerShutdownOp::SendRequest()
{
    this->InitSendMsg(1);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingAbortServerShutdownOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006be2e0
// Returns the op type (abort server shutdown)
RoutingOpType RoutingAbortServerShutdownOp::GetType() const { return RoutingOp_AbortServerShutdown; }

// FUNCTION: WON 0x6c8c10
// Asks the routing server to change this client's async message and client flags under the given masks
void RoutingSetClientFlagsOp::SendRequest()
{
    this->InitSendMsg(60);
    this->mSendMsg.AppendLong(this->mAsyncMessageFlagMask);
    this->mSendMsg.AppendLong(this->mAsyncMessageFlags);
    this->mSendMsg.AppendLong(this->mClientFlagMask);
    this->mSendMsg.AppendLong(this->mClientFlags);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingSetClientFlagsOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006be490
// Returns the op type (set client flags)
RoutingOpType RoutingSetClientFlagsOp::GetType() const { return RoutingOp_SetClientFlags; }

// FUNCTION: WON 0x6c8a20
// Sends a chat message to the listed recipients; waits for a reply only when flag 1 asks for one
void RoutingSendChatOp::SendRequest()
{
    this->InitSendMsg(54);
    this->mSendMsg.AppendShort(this->mFlags);
    this->mSendMsg.AppendWString(this->mText, 2);
    this->mSendMsg.AppendShort((short)this->mRecipients.size());
    for (std::list<unsigned short>::iterator i = this->mRecipients.begin(); i != this->mRecipients.end(); ++i)
        this->mSendMsg.AppendShort(*i);
    this->SendMsg();
    if (this->mFlags & 1) this->AddOp();
}


WONStatus RoutingSendChatOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006be7f0
// Returns the op type (send chat)
RoutingOpType RoutingSendChatOp::GetType() const { return RoutingOp_SendChat; }

// FUNCTION: WON 0x6c8aa0
// Sends a data block to the listed recipients; waits for a reply only when flag 1 asks for one
void RoutingSendDataOp::SendRequest()
{
    this->InitSendMsg(55);
    this->mSendMsg.AppendByte(this->mFlags);
    this->mSendMsg.AppendBuffer(this->mData.get(), 2);
    this->mSendMsg.AppendShort((short)this->mRecipients.size());
    for (std::list<unsigned short>::iterator i = this->mRecipients.begin(); i != this->mRecipients.end(); ++i)
        this->mSendMsg.AppendShort(*i);
    this->SendMsg();
    if (this->mFlags & 1) this->AddOp();
}


WONStatus RoutingSendDataOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bebb0
// Returns the op type (send data)
RoutingOpType RoutingSendDataOp::GetType() const { return RoutingOp_SendData; }

// FUNCTION: WON 0x6c4510
// Asks the routing server to turn moderator mode on or off for this client
void RoutingBecomeModeratorOp::SendRequest()
{
    this->InitSendMsg(4);
    this->mSendMsg.AppendBool(this->mModeratorOn);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingBecomeModeratorOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006bf4e0
// Returns the op type (become moderator)
RoutingOpType RoutingBecomeModeratorOp::GetType() const { return RoutingOp_BecomeModerator; }

// FUNCTION: WON 0x6c42d0
// Answers a client's join attempt for a group: accepted or rejected, with a comment
void RoutingAcceptClientOp::SendRequest()
{
    this->InitSendMsg(2);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendShort(this->mClientId);
    this->mSendMsg.AppendBool(this->mAccepted);
    this->mSendMsg.AppendWString(this->mAcceptComment, 2);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingAcceptClientOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c41f0
// Returns the op type (accept client)
RoutingOpType RoutingAcceptClientOp::GetType() const { return RoutingOp_AcceptClient; }

// FUNCTION: WON 0x6c4a20
// Asks the routing server to create a data object (link id, type, name, data)
void RoutingCreateDataObjectOp::SendRequest()
{
    this->InitSendMsg(11);
    this->mSendMsg.AppendByte(this->mFlags);
    this->mSendMsg.AppendShort(this->mLinkId);
    this->mSendMsg.AppendString(this->mDataType, 1);
    this->mSendMsg.AppendWString(this->mDataName, 1);
    this->mSendMsg.AppendBuffer(this->mData.get(), 2);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingCreateDataObjectOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c48e0
// Returns the op type (create data object)
RoutingOpType RoutingCreateDataObjectOp::GetType() const { return RoutingOp_CreateDataObject; }

// FUNCTION: WON 0x6c5210
// Asks the routing server to delete a data object by link id, type and name
void RoutingDeleteDataObjectOp::SendRequest()
{
    this->InitSendMsg(17);
    this->mSendMsg.AppendShort(this->mLinkId);
    this->mSendMsg.AppendString(this->mDataType, 1);
    this->mSendMsg.AppendWString(this->mDataName, 1);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingDeleteDataObjectOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c5100
// Returns the op type (delete data object)
RoutingOpType RoutingDeleteDataObjectOp::GetType() const { return RoutingOp_DeleteDataObject; }

// FUNCTION: WON 0x6c70f0
// Invites a client to (or uninvites it from) a group, with a comment
void RoutingInviteClientOp::SendRequest()
{
    this->InitSendMsg(40);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendShort(this->mClientId);
    this->mSendMsg.AppendBool(this->mInvited);
    this->mSendMsg.AppendWString(this->mInviteComment, 2);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingInviteClientOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c7010
// Returns the op type (invite client)
RoutingOpType RoutingInviteClientOp::GetType() const { return RoutingOp_InviteClient; }

// FUNCTION: WON 0x6c7840
// Asks the routing server to insert or replace data at an offset in a data object
void RoutingModifyDataObjectOp::SendRequest()
{
    this->InitSendMsg(45);
    this->mSendMsg.AppendShort(this->mLinkId);
    this->mSendMsg.AppendString(this->mDataType, 1);
    this->mSendMsg.AppendWString(this->mDataName, 1);
    this->mSendMsg.AppendShort(this->mOffset);
    this->mSendMsg.AppendBool(this->mIsInsert);
    this->mSendMsg.AppendBuffer(this->mData.get(), 2);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingModifyDataObjectOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c7700
// Returns the op type (modify data object)
RoutingOpType RoutingModifyDataObjectOp::GetType() const { return RoutingOp_ModifyDataObject; }

// FUNCTION: WON 0x6c8090
// Clears the result list and asks the routing server to read (or read and subscribe to) data objects of a type
void RoutingReadDataObjectOp::SendRequest()
{
    this->mDataObjects.clear();
    this->InitSendMsg(this->mSubscribe ? 67 : 50);
    this->mSendMsg.AppendShort(this->mLinkId);
    this->mSendMsg.AppendString(this->mDataType, 1);
    this->mSendMsg.AppendByte(this->mFlag);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x006c7ed0
// Returns the op type (read data object)
RoutingOpType RoutingReadDataObjectOp::GetType() const { return RoutingOp_ReadDataObject; }

// FUNCTION: WON 0x6c8e40
// Asks the routing server to stop sending updates for data objects of a type
void RoutingUnsubscribeDataObjectOp::SendRequest()
{
    this->InitSendMsg(68);
    this->mSendMsg.AppendShort(this->mLinkId);
    this->mSendMsg.AppendString(this->mDataType, 1);
    this->mSendMsg.AppendByte(this->mFlags);
    this->SendMsg();
    this->AddOp();
}


WONStatus RoutingUnsubscribeDataObjectOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c8d60
// Returns the op type (unsubscribe data object)
RoutingOpType RoutingUnsubscribeDataObjectOp::GetType() const { return RoutingOp_UnsubscribeDataObject; }

// FUNCTION: WON 0x006c8130
// Accepts the read-data-object reply, parses the objects into the result list on success; returns the status
WONStatus RoutingReadDataObjectOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 49) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) this->ParseReplyExceptForStatus(inMsg);
    return status;
}
// FUNCTION: WON 0x006c8170
// Appends each data object of the reply (link id, type, name, data) to the result list
void RoutingReadDataObjectOp::ParseReplyExceptForStatus(ReadBuffer& inMsg)
{
    int count = (unsigned short)inMsg.ReadShort();
    for (int i = 0; i < count; ++i) {
        RoutingDataObjectPtr data = new RoutingDataObject;
        data->mLinkId = inMsg.ReadShort();
        inMsg.ReadString(data->mDataType, 1);
        inMsg.ReadWString(data->mName, 1);
        data->mData = inMsg.ReadBuf(2);
        this->mDataObjects.push_back(data);
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x006c4190
// Creates an accept-client answer for group 0, client 0, not accepted
RoutingAcceptClientOp::RoutingAcceptClientOp(RoutingConnection* connection)
    : RoutingOp(connection),mGroupId(0),mClientId(0),mAccepted(false) {}
// FUNCTION: WON 0x006c6fb0
// Creates an invite request for group 0, client 0, not invited
RoutingInviteClientOp::RoutingInviteClientOp(RoutingConnection* connection)
    : RoutingOp(connection),mGroupId(0),mClientId(0),mInvited(false) {}
}

namespace WONAPI {
// FUNCTION: WON 0x006c4870
// Creates a create-data-object request with link id 0
RoutingCreateDataObjectOp::RoutingCreateDataObjectOp(RoutingConnection* connection)
    :RoutingOp(connection) { this->mLinkId=0; }
// FUNCTION: WON 0x006c5090
// Creates a delete-data-object request with link id 0
RoutingDeleteDataObjectOp::RoutingDeleteDataObjectOp(RoutingConnection* connection)
    :RoutingOp(connection) { this->mLinkId=0; }
// FUNCTION: WON 0x006c7690
// Creates a modify-data-object request with link id 0
RoutingModifyDataObjectOp::RoutingModifyDataObjectOp(RoutingConnection* connection)
    :RoutingOp(connection) { this->mLinkId=0; }
// FUNCTION: WON 0x006c8000
// Creates a read-data-object request with link id 0, no flag and no subscription
RoutingReadDataObjectOp::RoutingReadDataObjectOp(RoutingConnection* connection)
    :RoutingOp(connection) { this->mLinkId=0; this->mFlag=0; this->mSubscribe=false; }
// FUNCTION: WON 0x006c8d00
// Creates an unsubscribe-data-object request with link id 0 and no flags
RoutingUnsubscribeDataObjectOp::RoutingUnsubscribeDataObjectOp(RoutingConnection* connection)
    :RoutingOp(connection) { this->mLinkId=0; this->mFlags=0; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8390
// (public: virtual __thiscall WONAPI::RoutingDataObject::~RoutingDataObject(void)    [??1RoutingDataObject@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c8d90
// (public: virtual __thiscall WONAPI::RoutingUnsubscribeDataObjectOp::~RoutingUnsubscribeDataObjectOp(void)    [??1RoutingUnsubscribeDataObjectOp@WONAPI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c4200
// (public: virtual void * __thiscall WONAPI::RoutingAcceptClientOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingAcceptClientOp@WONAPI@@UAEP)
// TEMPLATE: WON 0x006c4220
// (public: virtual __thiscall WONAPI::RoutingAcceptClientOp::~RoutingAcceptClientOp(void)    [??1RoutingAcceptClientOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c7020
// (public: virtual void * __thiscall WONAPI::RoutingInviteClientOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingInviteClientOp@WONAPI@@UAEP)
// TEMPLATE: WON 0x006c7040
// (public: virtual __thiscall WONAPI::RoutingInviteClientOp::~RoutingInviteClientOp(void)    [??1RoutingInviteClientOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c48f0
// (public: virtual void * __thiscall WONAPI::RoutingCreateDataObjectOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingCreateDataObjectOp@WONA)
// TEMPLATE: WON 0x006c4910
// (public: virtual __thiscall WONAPI::RoutingCreateDataObjectOp::~RoutingCreateDataObjectOp(void)    [??1RoutingCreateDataObjectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c5110
// (public: virtual void * __thiscall WONAPI::RoutingDeleteDataObjectOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingDeleteDataObjectOp@WONA)
// TEMPLATE: WON 0x006c5130
// (public: virtual __thiscall WONAPI::RoutingDeleteDataObjectOp::~RoutingDeleteDataObjectOp(void)    [??1RoutingDeleteDataObjectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c7710
// (public: virtual void * __thiscall WONAPI::RoutingModifyDataObjectOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingModifyDataObjectOp@WONA)
// TEMPLATE: WON 0x006c7730
// (public: virtual __thiscall WONAPI::RoutingModifyDataObjectOp::~RoutingModifyDataObjectOp(void)    [??1RoutingModifyDataObjectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c7ee0
// (public: virtual void * __thiscall WONAPI::RoutingReadDataObjectOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingReadDataObjectOp@WONAPI@@)
// TEMPLATE: WON 0x006c7f00
// (public: virtual __thiscall WONAPI::RoutingReadDataObjectOp::~RoutingReadDataObjectOp(void)    [??1RoutingReadDataObjectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006c8d70
// (public: virtual void * __thiscall WONAPI::RoutingUnsubscribeDataObjectOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingUnsubscribeDataObj)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8370
// (public: virtual void * __thiscall WONAPI::RoutingDataObject::`scalar deleting destructor'(unsigned int)    [??_GRoutingDataObject@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ca600
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<struct WONAPI::RoutingDataObject>,class std::allocator<class WONAPI::SmartPtr<struct WONAP)
