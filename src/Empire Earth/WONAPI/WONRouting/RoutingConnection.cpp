// WONAPI routing server connection: the request helpers and the reply dispatch hooks
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingDetectFirewallOp.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
// The plain RoutingConnection; SmartRoutingConnection is in SmartRoutingConnection.cpp. The op list, the completions
// and the reply dispatch are a Rust object (rust/crates/ee_won/src/routeconn.rs EeWonRouteConn); what it calls back is
// below. Every op the request helpers make is a RoutingOp over a Rust op (routeop.rs)
#include "rust/ee_won.h"
namespace WONAPI {
// Every op and completion crosses as its RefCount*
static RefCount* Ref(void* inObject) { return static_cast<RefCount*>(inObject); }
static RoutingOp* Rop(void* inOp) { return static_cast<RoutingOp*>(static_cast<AsyncOp*>(Ref(inOp))); }
static RoutingConnection* Route(void* inConn) { return static_cast<RoutingConnection*>(inConn); }
static void* Out(RoutingOp* inOp) { return static_cast<RefCount*>(static_cast<AsyncOp*>(inOp)); }

static void HookAddRef(void* inObject) { Ref(inObject)->AddRef(); }
static void HookRelease(void* inObject) { Ref(inObject)->Release(); }
static void HookOpKill(void* inOp) { Rop(inOp)->Kill(true); }
static void* HookOpCompletion(void* inOp) { return static_cast<RefCount*>(Rop(inOp)->GetCompletion()); }
static void HookOpSetCompletion(void* inOp, void* inCompletion)
{
    Rop(inOp)->SetCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(Ref(inCompletion)));
}
static long HookOpType(void* inOp) { return Rop(inOp)->GetType(); }
static bool HookOpDerived(void* inOp) { return Rop(inOp)->IsDerivedServerOp(); }
// the op's HandleReply over the message from the header's end; a read past the end is a flag
static int32_t HookHandleReply(void* inOp, uint8_t inType, const uint8_t* inData, size_t inLength, size_t inPos,
                            bool* outThrew)
{
    ReadBuffer msg(inData, (unsigned long)inLength);
    msg.SetPos((int)inPos);
    try {
        return (int32_t)Rop(inOp)->HandleReply(inType, msg);
    } catch (ReadBufferException&) {
        *outThrew = true;
        return 0;
    }
}
static void HookOpEnd(void* inOp, bool inUnsolicited, long inStatus)
{
    RoutingOp* op = Rop(inOp);
    op->SetMode(OP_MODE_BLOCK);
    if (inUnsolicited) op->ForceFinish(inStatus);
    else op->Finish(inStatus);
}
static void HookOpComplete(void* inOp) { Rop(inOp)->Complete(); }
static void HookCompleteHook(void* inConn, void* inOp) { Route(inConn)->RoutingOpCompleteHook(Rop(inOp)); }
// GetUnsolicitedOp's new op: the notification of a kind (ee_won routemsg.rs's table; its op type routeop.rs's), or
// the derived server's; one reference for the caller
static void* HookNewUnsolicited(void* inConn, int32_t inKind, uint8_t inType)
{
    RoutingConnection* c = Route(inConn);
    SmartPtr<RoutingOp> op;
    if (inKind == -1) op = c->GetUnsolicitedDerivedOp(inType);
    else if (ee_won_routeop_notice_type(inKind) != 0) op = new RoutingOp(c, ee_won_routeop_notice_type(inKind));
    if (op.get() == NULL) return NULL;
    op->AddRef();
    return Out(op.get());
}
static void HookServerCloseHook(void* inConn) { Route(inConn)->ServerConnection::CloseHook(); }
static void HookServerKillHook(void* inConn) { Route(inConn)->ServerConnection::KillHook(); }
static const EeWonRouteConnHooks kHooks = {HookAddRef, HookRelease, HookOpKill, HookOpCompletion, HookOpSetCompletion,
    HookOpType, HookOpDerived, HookHandleReply, HookOpEnd, HookOpComplete, HookCompleteHook, HookNewUnsolicited,
    HookServerCloseHook, HookServerKillHook};

// FUNCTION: WON 0x006ba050
// Creates a routing connection with 2-byte length fields that treats every derived-server message as unsolicited
RoutingConnection::RoutingConnection()
{
    this->SetLengthFieldSize(2);
    this->mAllDerivedOpsAreUnsolicited = true;
    this->mRoute = ee_won_routeconn_new(static_cast<void*>(this), &kHooks, static_cast<void*>(this->GetDataLock()));
}

// FUNCTION: WON 0x006ba4b0
// Destroys the routing connection, releasing its pending ops and completions
RoutingConnection::~RoutingConnection() { ee_won_routeconn_free(this->mRoute); }

// FUNCTION: WON 0x006bcb20
// Kills every pending routing op and empties the list, then closes the server connection
void RoutingConnection::CloseHook() { ee_won_routeconn_close_hook(this->mRoute); }

// FUNCTION: WON 0x006bcbb0
// Drops the default and per-op-type completions, then kills the server connection
void RoutingConnection::KillHook() { ee_won_routeconn_kill_hook(this->mRoute); }

// Sets the completion of an op type (mCompletions[type]), or the default one
void RoutingConnection::SetOpCompletion(int inType, CompletionBase<AsyncOpPtr>* inCompletion)
{
    ee_won_routeconn_set_completion(this->mRoute, inType, static_cast<RefCount*>(inCompletion));
}
void RoutingConnection::SetDefaultOpCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    ee_won_routeconn_set_completion(this->mRoute, -1, static_cast<RefCount*>(inCompletion));
}

// FUNCTION: WON 0x006ba630
// Adds an op (with its completion resolved: its own, its type's, else the default one) to the list waiting for replies
void RoutingConnection::AddOp(RoutingOp* inOp) { ee_won_routeconn_add_op(this->mRoute, Out(inOp)); }

// Each request helper makes the op of its type (a Rust op, routeop.rs), sets its members and runs it async
static RoutingOp* NewOp(RoutingConnection* inConn, RoutingOpType inType, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = new RoutingOp(inConn, inType);
    op->SetCompletion(inCompletion);
    return op;
}
static void Run(RoutingOp* inOp)
{
    SmartPtr<RoutingOp> op = inOp;
    op->RunAsync(-1);
}
// a ban or mute: the group, on, the seconds, the explanation, then the client (by client id or by WON / CD key id)
static void Moderate(RoutingConnection* inConn, RoutingOpType inType, unsigned short inGroupId, bool inOn,
                     unsigned long inSeconds, const std::wstring& inExplanation, int inBy, unsigned long inId,
                     CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(inConn, inType, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetNum(EE_WON_RTF_ON, inOn);
    op->SetNum(EE_WON_RTF_TIME, inSeconds);
    op->SetText(EE_WON_RTT_COMMENT, inExplanation);
    if (inBy == 0) op->SetNum(EE_WON_RTF_CLIENT_ID, inId);
    else op->SetNum(EE_WON_RTF_WON_ID, inId);
    op->SetNum(EE_WON_RTF_BY_WON_ID, inBy != 0);
    if (inBy != 0) op->SetNum(EE_WON_RTF_IS_CDKEY, inBy == 2);
    Run(op);
}
// a data object request: its link id, type and name
static RoutingOp* DataOp(RoutingConnection* inConn, RoutingOpType inType, unsigned short inLink,
                         const std::string& inType2, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(inConn, inType, inCompletion);
    op->SetNum(EE_WON_RTF_LINK_ID, inLink);
    op->SetDataType(inType2);
    return op;
}

// FUNCTION: WON 0x006bd430
// Starts an async request to create a group with the given name, password, size and flags
void RoutingConnection::CreateGroup(unsigned short inGroupId, const std::wstring& inName, const std::wstring& inPassword,
    unsigned short inMaxPlayers, unsigned long inGroupFlags, unsigned char inJoinFlags, unsigned long inAsyncFlags,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_CreateGroup, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetText(EE_WON_RTT_NAME, inName);
    op->SetText(EE_WON_RTT_PASSWORD, inPassword);
    op->SetNum(EE_WON_RTF_MAX_PLAYERS, inMaxPlayers);
    op->SetNum(EE_WON_RTF_GROUP_FLAGS, inGroupFlags);
    op->SetNum(EE_WON_RTF_JOIN_FLAGS, inJoinFlags);
    op->SetNum(EE_WON_RTF_ASYNC_FLAGS, inAsyncFlags);
    Run(op);
}

// FUNCTION: WON 0x006bd580
// Starts an async request to join a group with a password, comment and join/member flags
void RoutingConnection::JoinGroup(unsigned short inGroupId, const std::wstring& inPassword, const std::wstring& inComment,
    unsigned char inJoinFlags, unsigned char inMemberFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_JoinGroup, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetText(EE_WON_RTT_PASSWORD, inPassword);
    op->SetText(EE_WON_RTT_COMMENT, inComment);
    op->SetNum(EE_WON_RTF_JOIN_FLAGS, inJoinFlags);
    op->SetNum(EE_WON_RTF_MEMBER_FLAGS, inMemberFlags);
    Run(op);
}

// FUNCTION: WON 0x006bf590
// Starts an async request to ban or unban a client (by client id) in a group for some seconds
void RoutingConnection::BanClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned short inClientId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_BanClient, inGroupId, inOn, inSeconds, inExplanation, 0, inClientId, inCompletion);
}
// FUNCTION: WON 0x006bf6b0
// Starts an async request to ban or unban a user by WON id in a group for some seconds
void RoutingConnection::BanClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned long inWONId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_BanClient, inGroupId, inOn, inSeconds, inExplanation, 1, inWONId, inCompletion);
}
// No original address identified: WON SDK ban by CD-key id
void RoutingConnection::BanClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned long inCDKeyId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_BanClient, inGroupId, inOn, inSeconds, inExplanation, 2, inCDKeyId, inCompletion);
}
// FUNCTION: WON 0x006bf7d0
// Starts an async request to mute or unmute a client (by client id) in a group for some seconds
void RoutingConnection::MuteClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned short inClientId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_MuteClient, inGroupId, inOn, inSeconds, inExplanation, 0, inClientId, inCompletion);
}
// FUNCTION: WON 0x006bf8f0
// Starts an async request to mute or unmute a user by WON id in a group for some seconds
void RoutingConnection::MuteClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned long inWONId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_MuteClient, inGroupId, inOn, inSeconds, inExplanation, 1, inWONId, inCompletion);
}
// No original address identified: WON SDK mute by CD-key id
void RoutingConnection::MuteClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
    const std::wstring& inExplanation, unsigned long inCDKeyId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    Moderate(this, RoutingOp_MuteClient, inGroupId, inOn, inSeconds, inExplanation, 2, inCDKeyId, inCompletion);
}

// FUNCTION: WON 0x006bcc30
// Starts an async request to register this client with the routing server under a name and password
void RoutingConnection::Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inReconnectId,
    unsigned long inAsyncFlags, unsigned long inRegisterFlags, unsigned long inClientFlags,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_RegisterClient, inCompletion);
    op->SetText(EE_WON_RTT_NAME, inName);
    op->SetText(EE_WON_RTT_PASSWORD, inPassword);
    op->SetNum(EE_WON_RTF_RECONNECT_ID, inReconnectId);
    op->SetNum(EE_WON_RTF_ASYNC_MSG_FLAGS, inAsyncFlags);
    op->SetNum(EE_WON_RTF_REGISTER_FLAGS, inRegisterFlags);
    op->SetNum(EE_WON_RTF_CLIENT_FLAGS, inClientFlags);
    Run(op);
}
// FUNCTION: WON 0x006bcd40
// Starts an async request for the routing server's client list
void RoutingConnection::GetClientList(CompletionBase<AsyncOpPtr>* inCompletion)
{
    Run(NewOp(this, RoutingOp_GetClientList, inCompletion));
}
// No original address identified: WON SDK extended client list
void RoutingConnection::GetClientListEx(CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_GetClientList, inCompletion);
    op->SetNum(EE_WON_RTF_IS_EX, 1);
    Run(op);
}
// FUNCTION: WON 0x006bcf90
// Starts an async request for the group list with the given field flags
void RoutingConnection::GetGroupList(unsigned char inFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_GetGroupList, inCompletion);
    op->SetNum(EE_WON_RTF_FLAGS16, inFlags);
    Run(op);
}
// FUNCTION: WON 0x006bd1e0
// Starts an async request for the members of a group
void RoutingConnection::GetMembersOfGroup(unsigned short inId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_GetMembersOfGroup, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inId);
    Run(op);
}
// FUNCTION: WON 0x006bd850
// Starts an async request to leave a group
void RoutingConnection::LeaveGroup(unsigned short inId, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_LeaveGroup, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inId);
    Run(op);
}
// FUNCTION: WON 0x006bd6b0
// Starts an async request to cancel a pending group join (optionally leaving if already joined)
void RoutingConnection::CancelJoinGroup(unsigned short inId, bool inLeave, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_CancelJoinGroup, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inId);
    op->SetNum(EE_WON_RTF_ON, inLeave);
    Run(op);
}
// FUNCTION: WON 0x006bd9f0 (accept), 0x006bdb00 (invite)
// Starts an async answer accepting or rejecting a client's attempt to join a group; an invitation likewise
static void ClientAnswer(RoutingConnection* inConn, RoutingOpType inType, unsigned short inGroup, unsigned short inClient,
                         bool inOn, const std::wstring& inExplanation, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(inConn, inType, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroup);
    op->SetNum(EE_WON_RTF_CLIENT_ID, inClient);
    op->SetNum(EE_WON_RTF_ON, inOn);
    op->SetText(EE_WON_RTT_COMMENT, inExplanation);
    Run(op);
}
void RoutingConnection::AcceptClient(unsigned short inGroup, unsigned short inClient, bool inOn,
    const std::wstring& inExplanation, CompletionBase<AsyncOpPtr>* inCompletion)
{
    ClientAnswer(this, RoutingOp_AcceptClient, inGroup, inClient, inOn, inExplanation, inCompletion);
}
void RoutingConnection::InviteClient(unsigned short inGroup, unsigned short inClient, bool inOn,
    const std::wstring& inExplanation, CompletionBase<AsyncOpPtr>* inCompletion)
{
    ClientAnswer(this, RoutingOp_InviteClient, inGroup, inClient, inOn, inExplanation, inCompletion);
}
// FUNCTION: WON 0x006bdc10
// Starts an async request to change a group's flags and async flags under masks
void RoutingConnection::SetGroupFlags(unsigned short inId, unsigned long inMask, unsigned long inFlags,
    unsigned long inAsyncMask, unsigned long inAsyncFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SetGroupFlags, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inId);
    op->SetNum(EE_WON_RTF_GROUP_MASK, inMask);
    op->SetNum(EE_WON_RTF_GROUP_FLAGS, inFlags);
    op->SetNum(EE_WON_RTF_ASYNC_MASK, inAsyncMask);
    op->SetNum(EE_WON_RTF_ASYNC_FLAGS, inAsyncFlags);
    Run(op);
}
// FUNCTION: WON 0x006bddd0
// Starts an async request to broadcast a server alert text
void RoutingConnection::SendServerAlert(const std::wstring& inText, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SendServerAlert, inCompletion);
    op->SetText(EE_WON_RTT_TEXT, inText);
    Run(op);
}
// No original address identified: WON SDK alert to listed clients (nothing for an empty list)
void RoutingConnection::SendServerAlertWithRecipientList(const std::wstring& inText,
    std::list<unsigned short>& inRecipients, CompletionBase<AsyncOpPtr>* inCompletion)
{
    if (inRecipients.empty()) return;
    RoutingOp* op = NewOp(this, RoutingOp_SendServerAlert, inCompletion);
    op->SetText(EE_WON_RTT_TEXT, inText);
    op->SetRecipients(inRecipients);
    Run(op);
}
// FUNCTION: WON 0x006be010
// Starts an async request for a server shutdown in the given seconds, with an alert text
void RoutingConnection::StartServerShutdown(const std::wstring& inText, unsigned long inSeconds,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_StartServerShutdown, inCompletion);
    op->SetText(EE_WON_RTT_TEXT, inText);
    op->SetNum(EE_WON_RTF_TIME, inSeconds);
    Run(op);
}
// FUNCTION: WON 0x006be200
// Starts an async request to abort a pending server shutdown
void RoutingConnection::AbortServerShutdown(CompletionBase<AsyncOpPtr>* inCompletion)
{
    Run(NewOp(this, RoutingOp_AbortServerShutdown, inCompletion));
}
// FUNCTION: WON 0x006be390
// Starts an async request to change this client's flags under masks (the client mask is cut to 16 bits)
void RoutingConnection::SetClientFlags(unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask,
    unsigned long inAsyncFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SetClientFlags, inCompletion);
    op->SetNum(EE_WON_RTF_CLIENT_MASK, (unsigned short)inMask);
    op->SetNum(EE_WON_RTF_CLIENT_FLAGS, inFlags);
    op->SetNum(EE_WON_RTF_ASYNC_MSG_MASK, inAsyncMask);
    op->SetNum(EE_WON_RTF_ASYNC_MSG_FLAGS, inAsyncFlags);
    Run(op);
}
// FUNCTION: WON 0x006be540
// Sends a chat message to a single recipient
void RoutingConnection::SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inRecipient,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    std::list<unsigned short> aRecipients;
    aRecipients.push_back(inRecipient);
    this->SendChatTo(inText, (unsigned short)inFlags, aRecipients, inCompletion);
}
// FUNCTION: WON 0x006be6a0
// Starts an async chat message to a list of recipients
void RoutingConnection::SendChatTo(const std::wstring& inText, unsigned short inFlags,
    const std::list<unsigned short>& inRecipients, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SendChat, inCompletion);
    op->SetText(EE_WON_RTT_TEXT, inText);
    op->SetNum(EE_WON_RTF_FLAGS16, inFlags);
    op->SetRecipients(inRecipients);
    Run(op);
}
// FUNCTION: WON 0x006be910
// Sends a data block to a single recipient
void RoutingConnection::SendData(const ByteBuffer* inData, unsigned char inFlags, unsigned short inRecipient,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    std::list<unsigned short> aRecipients;
    aRecipients.push_back(inRecipient);
    this->SendData(inData, inFlags, aRecipients, inCompletion);
}
// FUNCTION: WON 0x006bea70
// Starts an async send of a data block to a list of recipients
void RoutingConnection::SendData(const ByteBuffer* inData, unsigned char inFlags,
    const std::list<unsigned short>& inRecipients, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SendData, inCompletion);
    op->SetData(inData);
    op->SetNum(EE_WON_RTF_FLAGS8, inFlags);
    op->SetRecipients(inRecipients);
    Run(op);
}
// FUNCTION: WON 0x006bf3f0
// Starts an async request to turn moderator mode on or off
void RoutingConnection::BecomeModerator(bool inOn, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_BecomeModerator, inCompletion);
    op->SetNum(EE_WON_RTF_ON, inOn);
    Run(op);
}

// FUNCTION: WON 0x006becd0
// Starts an async request to create a data object (link id, type, name, data)
void RoutingConnection::CreateDataObject(unsigned char inFlags, unsigned short inLink, const std::string& inType,
    const std::wstring& inName, const ByteBuffer* inData, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = DataOp(this, RoutingOp_CreateDataObject, inLink, inType, inCompletion);
    op->SetNum(EE_WON_RTF_FLAGS8, inFlags);
    op->SetText(EE_WON_RTT_NAME, inName);
    op->SetData(inData);
    Run(op);
}
// FUNCTION: WON 0x006bee30
// Starts an async request to delete a data object by link id, type and name
void RoutingConnection::ReplaceDataObject(unsigned short inLink, const std::string& inType, const std::wstring& inName,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = DataOp(this, RoutingOp_DeleteDataObject, inLink, inType, inCompletion);
    op->SetText(EE_WON_RTT_NAME, inName);
    Run(op);
}
// FUNCTION: WON 0x006bef50
// Starts an async request to insert or replace data at an offset in a data object
void RoutingConnection::ModifyDataObject(unsigned short inLink, const std::string& inType, const std::wstring& inName,
    unsigned short inOffset, bool inInsert, const ByteBuffer* inData, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = DataOp(this, RoutingOp_ModifyDataObject, inLink, inType, inCompletion);
    op->SetText(EE_WON_RTT_NAME, inName);
    op->SetNum(EE_WON_RTF_OFFSET, inOffset);
    op->SetNum(EE_WON_RTF_ON, inInsert);
    op->SetData(inData);
    Run(op);
}
// FUNCTION: WON 0x006bf0c0 (read), 0x006bf1d0 (subscribe)
// Starts an async read of the data objects of a type under a link id (and a subscription to their changes)
static void ReadData(RoutingConnection* inConn, bool inSubscribe, unsigned short inLink, const std::string& inType,
                     unsigned long inFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = DataOp(inConn, RoutingOp_ReadDataObject, inLink, inType, inCompletion);
    op->SetNum(EE_WON_RTF_ON, inSubscribe);
    op->SetNum(EE_WON_RTF_FLAGS8, inFlags);
    Run(op);
}
void RoutingConnection::ReadDataObject(unsigned short inLink, const std::string& inType, unsigned long inFlags,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    ReadData(this, false, inLink, inType, inFlags, inCompletion);
}
void RoutingConnection::SubscribeDataObject(unsigned short inLink, const std::string& inType, unsigned long inFlags,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    ReadData(this, true, inLink, inType, inFlags, inCompletion);
}
// No original address identified: WON SDK CD key id list (the _KeyId data objects, flag 0)
void RoutingConnection::GetClientCDKeyIdList(CompletionBase<AsyncOpPtr>* inCompletion, bool inSubscribe)
{
    ReadData(this, inSubscribe, 0xffff, "_KeyId", 0, inCompletion);
}
// FUNCTION: WON 0x006bf2e0
// Starts an async request to stop updates for the data objects of a type
void RoutingConnection::UnsubscribeDataObject(unsigned short inLink, const std::string& inType, unsigned long inFlags,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = DataOp(this, RoutingOp_UnsubscribeDataObject, inLink, inType, inCompletion);
    op->SetNum(EE_WON_RTF_FLAGS8, inFlags);
    Run(op);
}

// No original address identified: WON SDK routing request helpers
void RoutingConnection::DisconnectClient(CompletionBase<AsyncOpPtr>* inCompletion)
{
    Run(NewOp(this, RoutingOp_DisconnectClient, inCompletion));
}
void RoutingConnection::SetGroupName(unsigned short inGroupId, const std::wstring& inName,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SetGroupName, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetText(EE_WON_RTT_NAME, inName);
    Run(op);
}
void RoutingConnection::SetGroupMaxPlayers(unsigned short inGroupId, unsigned short inMaxPlayers,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_SetGroupMaxPlayers, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetNum(EE_WON_RTF_MAX_PLAYERS, inMaxPlayers);
    Run(op);
}
void RoutingConnection::RelinquishCaptaincy(unsigned short inGroupId, unsigned short inNewCaptainId,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_RelinquishCaptaincy, inCompletion);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetNum(EE_WON_RTF_CLIENT_ID, inNewCaptainId);
    Run(op);
}
void RoutingConnection::DetectFirewall(unsigned short inListenPort, unsigned long inMaxConnectSeconds, bool inDoListen,
    CompletionBase<AsyncOpPtr>* inCompletion)
{
    SmartPtr<RoutingDetectFirewallOp> op = new RoutingDetectFirewallOp(this);
    op->SetListenPort(inListenPort);
    op->SetMaxConnectWaitTime(inMaxConnectSeconds);
    op->SetDoListen(inDoListen);
    op->SetCompletion(inCompletion);
    op->RunAsync(-1);
}
void RoutingConnection::GetBadUserList(RoutingBadUserListType inListType, unsigned short inGroupId,
    unsigned char inFlags, CompletionBase<AsyncOpPtr>* inCompletion)
{
    RoutingOp* op = NewOp(this, RoutingOp_GetBadUserList, inCompletion);
    op->SetNum(EE_WON_RTF_LIST_TYPE, inListType);
    op->SetNum(EE_WON_RTF_GROUP_ID, inGroupId);
    op->SetNum(EE_WON_RTF_FLAGS8, inFlags);
    Run(op);
}

// FUNCTION: WON 0x006bc6c0
// Returns a new derived-server op for an unsolicited message when all derived ops count as unsolicited, else NULL
SmartPtr<RoutingOp> RoutingConnection::GetUnsolicitedDerivedOp(unsigned char)
{
    if (this->mAllDerivedOpsAreUnsolicited) return new RoutingOp(this, RoutingOp_DerivedServerOp);
    return NULL;
}

// FUNCTION: WON 0x006bc850
// Dispatches an incoming routing message to the first pending or unsolicited op that accepts it and completes that op
void RoutingConnection::MsgCallback(const ByteBuffer* inData)
{
    ee_won_routeconn_msg_callback(this->mRoute, reinterpret_cast<const uint8_t*>(inData->data()), inData->length());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ba3f0
// (public: virtual __thiscall WONAPI::RoutingGroupInfo::~RoutingGroupInfo(void)    [??1RoutingGroupInfo@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ba490
// (protected: virtual void * __thiscall WONAPI::RoutingConnection::`scalar deleting destructor'(unsigned int)    [??_GRoutingConnection@WONAPI@@MAEPAXI@Z)
// TEMPLATE: WON 0x006ba280
// (public: virtual void * __thiscall WONAPI::RoutingMemberInfo::`scalar deleting destructor'(unsigned int)    [??_GRoutingMemberInfo@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ba2a0
// (public: virtual __thiscall WONAPI::RoutingMemberInfo::~RoutingMemberInfo(void)    [??1RoutingMemberInfo@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ba3d0
// (public: virtual void * __thiscall WONAPI::RoutingGroupInfo::`scalar deleting destructor'(unsigned int)    [??_GRoutingGroupInfo@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ba1a0
// (public: virtual void * __thiscall WONAPI::RoutingClientInfo::`scalar deleting destructor'(unsigned int)    [??_GRoutingClientInfo@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ba1c0
// (public: virtual __thiscall WONAPI::RoutingClientInfo::~RoutingClientInfo(void)    [??1RoutingClientInfo@WONAPI@@UAE@XZ])
