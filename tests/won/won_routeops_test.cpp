// WONRouting's request helpers and its request and notification ops over 127.0.0.1, one line per event, compared
// with rust/crates/ee_won/src/golden_routeops.txt (recorded over the C++ op classes before their move to Rust): a
// plain RoutingConnection to a routing server stand-in on a loopback socket. Every request helper (and the ops the
// lobby's RoutingLogic builds itself) is run; the server logs the bytes it receives and answers with the replies
// the ops read (statuses, lists, records, a reply for another group, a join that waits, a password retry on the
// same op); each finished op is dumped, every member its readers use (RoutingLogic, EEWonNet, the smart
// connection). Then every notification the server sends, a derived server's message, and a group list body read
// by a connection-less op. `--print` prints the lines instead. The exit code is the number of failures.
//
// PeerAuthOp is a Rust op on the op base whose real run needs certificates: as won_servconn_test, this driver
// defines PeerAuthOp's constructor and RustServerOp's members itself (ServerOp.cpp is not linked).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "rust/ee_won.h"
#ifndef EE_WON_ROUTE_OPS_RUST
#include "ee/wonapi/WONRoutingDerivedServerOp.h"
#endif

using namespace WONAPI;

static std::vector<std::string> sLines;
static PlatMutex* sLinesLock = PlatMutexCreate();
static void Log(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    sLines.push_back(inLine);
    static const bool sTrace = getenv("WON_ROUTEOPS_TRACE") != NULL;
    if (sTrace)
        fprintf(stderr, "%s\n", inLine.c_str());
    PlatMutexUnlock(sLinesLock);
}
static std::string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string X(unsigned long inValue)
{
    char aText[16];
    snprintf(aText, sizeof(aText), "%lx", inValue);
    return aText;
}
static std::string HexBytes(const char* inData, unsigned long inLength)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N(inLength) + ":";
    for (unsigned long i = 0; i < inLength && i < 96; i++) {
        unsigned char c = static_cast<unsigned char>(inData[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static std::string Hex(const ByteBuffer* inBuffer)
{
    if (inBuffer == NULL)
        return "(null)";
    return HexBytes(inBuffer->data(), inBuffer->length());
}
static ByteBufferPtr Bytes(const void* inData, unsigned long inLength) { return new ByteBuffer(inData, inLength, false); }
static std::string Esc(const std::wstring& inText)
{
    std::string s = "\"";
    for (size_t i = 0; i < inText.length(); i++) {
        unsigned long c = (unsigned long)inText[i];
        if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
            s += (char)c;
        else
            s += "\\u" + X(c);
    }
    return s + "\"";
}
static std::string Esc(const std::string& inText) { return Esc(std::wstring(inText.begin(), inText.end())); }
static std::string Ids(const std::list<unsigned short>& inIds)
{
    std::string s = "[";
    for (std::list<unsigned short>::const_iterator i = inIds.begin(); i != inIds.end(); ++i)
        s += (i == inIds.begin() ? "" : ",") + N(*i);
    return s + "]";
}

// ---- the stand-in peer authentication: PeerAuthOp's constructor and RustServerOp's members

static std::map<const void*, IPAddr> sPeerAddr;
static int sListenPort;

namespace WONAPI {
ServerOp::ServerOp()
{
    this->mAuthType = AUTH_TYPE_NONE;
    this->mLengthFieldSize = 4;
    this->mClientKeySize = 8;
    this->mUseClientKey = false;
    this->mMaxConnectTime = 60000;
    this->mMaxSendTime = 60000;
    this->mMaxRecvTime = 60000;
}
RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp), mFile(NULL) {}
RustServerOp::~RustServerOp()
{
    PlatMutexLock(sLinesLock);
    sPeerAddr.erase(this);
    PlatMutexUnlock(sLinesLock);
}
void RustServerOp::RunHook()
{
    IPAddr anAddr = sPeerAddr[this];
    this->Finish(this->mSocket->Connect(anAddr, 5000));
}
void RustServerOp::CleanupHook() { AsyncOpWithTracker::CleanupHook(); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam) { return AsyncOpWithTracker::CallbackHook(inOp, inParam); }
PeerAuthOp::PeerAuthOp(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket,
                       bool)
    : RustServerOp(NULL)
{
    this->mSocket = inSocket;
    this->mAuthContext = inContext;
    this->mAuthType = inType;
    PlatMutexLock(sLinesLock);
    sPeerAddr[this] = inAddr;
    PlatMutexUnlock(sLinesLock);
}
}

// ---- the op dump: every member the op's readers use (the original's members, or the Rust op's accessors)

static void Line(std::vector<std::string>& out, const std::string& inText) { out.push_back(" " + inText); }

static std::string Client(const RoutingClientInfo* c)
{
    if (c == NULL)
        return "(none)";
    return "id=" + N(c->mClientId) + " " + Esc(c->mName) + " f=" + X(c->mFlags) + " user=" + N(c->mUserId) + " cd="
        + N(c->mAddress) + " trust=" + N(c->mTrustLevel);
}
static void Clients(std::vector<std::string>& out, const char* inWhat, const RoutingClientMap& inMap)
{
    Line(out, std::string(inWhat) + " " + N(inMap.size()));
    for (RoutingClientMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
        Line(out, " client " + N(i->first) + " " + Client(i->second.get()));
}
static void Members(std::vector<std::string>& out, const char* inWhat, const RoutingMemberMap& inMap)
{
    Line(out, std::string(inWhat) + " " + N(inMap.size()));
    for (RoutingMemberMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
        Line(out, " member " + N(i->first) + " id=" + N(i->second->mClientId) + " f=" + N(i->second->mFlags) + " c="
                      + Client(i->second->mClientInfo.get()));
}
static std::string Group(const RoutingGroupInfo* g)
{
    return "id=" + N(g->mId) + " " + Esc(g->mName) + " parent=" + N(g->mParentId) + " cap=" + N(g->mCaptainId)
        + " maxc=" + N(g->mMaxClients) + " f=" + X(g->mFlags) + " af=" + X(g->mAsyncFlags) + " obs="
        + N(g->mObserverCount) + " mp=" + N(g->mMaxPlayers);
}
static void Groups(std::vector<std::string>& out, const RoutingGroupMap& inMap)
{
    Line(out, "groups " + N(inMap.size()));
    for (RoutingGroupMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i) {
        Line(out, " group " + N(i->first) + " " + Group(i->second.get()));
        Members(out, "  members", i->second->mMembers);
    }
}
static void Objects(std::vector<std::string>& out, const RoutingDataObjectList& inList)
{
    Line(out, "objects " + N(inList.size()));
    for (RoutingDataObjectList::const_iterator i = inList.begin(); i != inList.end(); ++i)
        Line(out, " object link=" + N((*i)->mLinkId) + " type=" + Esc((*i)->mDataType) + " " + Esc((*i)->mName)
                      + " data=" + Hex((*i)->mData.get()));
}
static void BadUsers(std::vector<std::string>& out, const RoutingBadUserList& inList)
{
    Line(out, "bad users " + N(inList.size()));
    for (RoutingBadUserList::const_iterator i = inList.begin(); i != inList.end(); ++i)
        Line(out, " bad won=" + N(i->mWONId) + " " + Esc(i->mName) + " exp=" + N(i->mExpirationDiff) + " mod="
                      + N(i->mModeratorWONId) + " " + Esc(i->mModeratorName) + " " + Esc(i->mModeratorComment));
}

#ifndef EE_WON_ROUTE_OPS_RUST
static void DumpFields(RoutingOp* inOp, std::vector<std::string>& out)
{
    bool ok = inOp->GetStatus() == WS_Success;
    switch (inOp->GetType()) {
    case RoutingOp_AbortServerShutdown: break;
    case RoutingOp_AcceptClient: {
        RoutingAcceptClientOp* o = static_cast<RoutingAcceptClientOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " on=" + N(o->mAccepted) + " "
                      + Esc(o->mAcceptComment));
        break;
    }
    case RoutingOp_BanClient: {
        RoutingBanClientOp* o = static_cast<RoutingBanClientOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " on=" + N(o->mIsBanned) + " time=" + N(o->mBanTime) + " "
                      + Esc(o->mBanComment) + " bywon=" + N(o->mBanByWONId) + " cdkey=" + N(o->mWONIdIsCDKeyId)
                      + " won=" + N(o->mWONId) + " client=" + N(o->GetClientId()));
        break;
    }
    case RoutingOp_MuteClient: {
        RoutingMuteClientOp* o = static_cast<RoutingMuteClientOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " on=" + N(o->GetIsMuted()) + " time=" + N(o->mMuteTime) + " "
                      + Esc(o->mMuteComment) + " bywon=" + N(o->mMuteByWONId) + " cdkey=" + N(o->mWONIdIsCDKeyId)
                      + " won=" + N(o->mWONId) + " client=" + N(o->GetClientId()));
        break;
    }
    case RoutingOp_BecomeModerator:
        Line(out, "on=" + N(static_cast<RoutingBecomeModeratorOp*>(inOp)->mModeratorOn));
        break;
    case RoutingOp_CancelJoinGroup: {
        RoutingCancelJoinGroupOp* o = static_cast<RoutingCancelJoinGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " leave=" + N(o->mLeaveIfAlreadyInGroup));
        break;
    }
    case RoutingOp_ClientFlagsChanged: {
        RoutingClientFlagsChangedOp* o = static_cast<RoutingClientFlagsChangedOp*>(inOp);
        Line(out, "client=" + N(o->GetClientId()) + " f=" + X(o->GetClientFlags()));
        break;
    }
    case RoutingOp_ClientJoinedGroup: {
        RoutingClientJoinedGroupOp* o = static_cast<RoutingClientJoinedGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " mf=" + N(o->GetMemberFlags())
                      + " " + Esc(o->GetClientName()) + " cf=" + X(o->GetClientFlags()) + " has=" + N(o->HasClientFlags()));
        break;
    }
    case RoutingOp_ClientJoinedServer: {
        RoutingClientJoinedServerOp* o = static_cast<RoutingClientJoinedServerOp*>(inOp);
        Line(out, "client " + Client(o->mClientInfo.get()) + " name " + Esc(o->GetClientName()));
        break;
    }
    case RoutingOp_ClientLeftGroup: {
        RoutingClientLeftGroupOp* o = static_cast<RoutingClientLeftGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()));
        break;
    }
    case RoutingOp_ClientLeftServer:
        Line(out, "client=" + N(static_cast<RoutingClientLeftServerOp*>(inOp)->GetClientId()));
        break;
    case RoutingOp_CreateDataObject: {
        RoutingCreateDataObjectOp* o = static_cast<RoutingCreateDataObjectOp*>(inOp);
        Line(out, "f=" + N(o->GetFlags()) + " link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " "
                      + Esc(o->mDataName) + " data=" + Hex(o->GetData().get()));
        break;
    }
    case RoutingOp_CreateGroup: {
        RoutingCreateGroupOp* o = static_cast<RoutingCreateGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " parent=" + N(o->mParentGroupId) + " " + Esc(o->mGroupName) + " pw="
                      + Esc(o->mGroupPassword) + " maxp=" + N(o->mMaxPlayers) + " jf=" + N(o->mJoinFlags) + " gf="
                      + X(o->mGroupFlags) + " af=" + X(o->mAsyncFlags) + " leave=" + N(o->mLeaveGroupOnCancel));
        break;
    }
    case RoutingOp_DataObjectCreated: {
        RoutingDataObjectCreatedOp* o = static_cast<RoutingDataObjectCreatedOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " " + Esc(o->mName) + " data="
                      + Hex(o->GetData().get()));
        break;
    }
    case RoutingOp_DataObjectDeleted: {
        RoutingDataObjectDeletedOp* o = static_cast<RoutingDataObjectDeletedOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " " + Esc(o->mName));
        break;
    }
    case RoutingOp_DataObjectModified: {
        RoutingDataObjectModifiedOp* o = static_cast<RoutingDataObjectModifiedOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " " + Esc(o->mName) + " off="
                      + N(o->GetOffset()) + " ins=" + N(o->GetIsInsert()) + " data=" + Hex(o->GetData().get()));
        break;
    }
    case RoutingOp_DeleteDataObject: {
        RoutingDeleteDataObjectOp* o = static_cast<RoutingDeleteDataObjectOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " " + Esc(o->mDataName));
        break;
    }
    case RoutingOp_GetBadUserList: {
        RoutingGetBadUserListOp* o = static_cast<RoutingGetBadUserListOp*>(inOp);
        Line(out, "list=" + N(o->mListType) + " group=" + N(o->GetGroupId()) + " f=" + N(o->GetFlags()));
        BadUsers(out, o->mBadUserList);
        break;
    }
    case RoutingOp_GetClientList: {
        RoutingGetClientListOp* o = static_cast<RoutingGetClientListOp*>(inOp);
        Line(out, "ex=" + N(o->mIsEx));
        Clients(out, "clients", o->mClientMap);
        break;
    }
    case RoutingOp_GetGroupList: {
        RoutingGetGroupListOp* o = static_cast<RoutingGetGroupListOp*>(inOp);
        Line(out, "f=" + X(o->GetFlags()) + " parent=" + N(o->mParentGroupId) + " members=" + N(o->IncludeMembers()));
        Groups(out, o->GetGroupMap());
        break;
    }
    case RoutingOp_GetMembersOfGroup: {
        RoutingGetMembersOfGroupOp* o = static_cast<RoutingGetMembersOfGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId())
                      + (ok ? " obs=" + N(o->GetObserverCount()) + " names=" + N(o->mHasClientNames) + " cflags="
                                  + N(o->mHasClientFlags)
                            : std::string()));
        Members(out, "members", o->GetMembers());
        break;
    }
    case RoutingOp_GroupCaptainChanged: {
        RoutingGroupCaptainChangedOp* o = static_cast<RoutingGroupCaptainChangedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " captain=" + N(o->GetCaptainId()));
        break;
    }
    case RoutingOp_GroupCreated: {
        RoutingGroupCreatedOp* o = static_cast<RoutingGroupCreatedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " " + Esc(o->GetName()) + " maxc=" + N(o->GetMaxClients()) + " f="
                      + X(o->GetFlags()) + " af=" + X(o->GetAsyncFlags()));
        RoutingGroupInfoPtr g = o->GetGroupInfo();
        Line(out, "info " + Group(g.get()) + " members " + N(g->mMembers.size()));
        break;
    }
    case RoutingOp_GroupDeleted:
        Line(out, "group=" + N(static_cast<RoutingGroupDeletedOp*>(inOp)->GetGroupId()));
        break;
    case RoutingOp_GroupFlagsChanged: {
        RoutingGroupFlagsChangedOp* o = static_cast<RoutingGroupFlagsChangedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " f=" + X(o->GetGroupFlags()) + " af=" + X(o->mAsyncFlags));
        break;
    }
    case RoutingOp_GroupInvitation: {
        RoutingGroupInvitationOp* o = static_cast<RoutingGroupInvitationOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " on=" + N(o->GetInvited()) + " "
                      + Esc(o->GetComment()));
        break;
    }
    case RoutingOp_GroupJoinAttempt: {
        RoutingGroupJoinAttemptOp* o = static_cast<RoutingGroupJoinAttemptOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " " + Esc(o->GetComment()) + " f="
                      + N(o->GetFlags()));
        break;
    }
    case RoutingOp_GroupMaxPlayersChanged: {
        RoutingGroupMaxPlayersChangedOp* o = static_cast<RoutingGroupMaxPlayersChangedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " maxp=" + N(o->GetMaxPlayers()));
        break;
    }
    case RoutingOp_GroupMemberCount: {
        RoutingGroupMemberCountOp* o = static_cast<RoutingGroupMemberCountOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " count=" + N(o->mMemberCount));
        break;
    }
    case RoutingOp_GroupNameChanged: {
        RoutingGroupNameChangedOp* o = static_cast<RoutingGroupNameChangedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " " + Esc(o->mName));
        break;
    }
    case RoutingOp_GroupObserverCount: {
        RoutingGroupObserverCountOp* o = static_cast<RoutingGroupObserverCountOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " obs=" + N(o->GetObserverCount()));
        break;
    }
    case RoutingOp_InviteClient: {
        RoutingInviteClientOp* o = static_cast<RoutingInviteClientOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " on=" + N(o->GetInvited()) + " "
                      + Esc(o->mInviteComment));
        break;
    }
    case RoutingOp_JoinGroup: {
        RoutingJoinGroupOp* o = static_cast<RoutingJoinGroupOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " pw=" + Esc(o->mGroupPassword) + " " + Esc(o->mJoinComment) + " jf="
                      + N(o->GetJoinFlags()) + " mf=" + N(o->GetMemberFlags()));
        Members(out, "members", o->mMemberMap);
        break;
    }
    case RoutingOp_LeaveGroup:
        Line(out, "group=" + N(static_cast<RoutingLeaveGroupOp*>(inOp)->GetGroupId()));
        break;
    case RoutingOp_MemberFlagsChanged: {
        RoutingMemberFlagsChangedOp* o = static_cast<RoutingMemberFlagsChangedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " client=" + N(o->GetClientId()) + " mf=" + N(o->GetMemberFlags()));
        break;
    }
    case RoutingOp_ModifyDataObject: {
        RoutingModifyDataObjectOp* o = static_cast<RoutingModifyDataObjectOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " " + Esc(o->mDataName) + " off="
                      + N(o->mOffset) + " ins=" + N(o->mIsInsert) + " data=" + Hex(o->mData.get()));
        break;
    }
    case RoutingOp_PeerChat: {
        RoutingPeerChatOp* o = static_cast<RoutingPeerChatOp*>(inOp);
        Line(out, "client=" + N(o->GetClientId()) + " f=" + X(o->GetFlags()) + " whisper=" + N(o->IsWhisper()) + " "
                      + Esc(o->GetText()) + " to=" + Ids(o->GetRecipients()));
        break;
    }
    case RoutingOp_PeerData: {
        RoutingPeerDataOp* o = static_cast<RoutingPeerDataOp*>(inOp);
        Line(out, "client=" + N(o->GetClientId()) + " data=" + Hex(o->GetData().get()));
        break;
    }
    case RoutingOp_ReadDataObject: {
        RoutingReadDataObjectOp* o = static_cast<RoutingReadDataObjectOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " f=" + N(o->mFlag) + " sub="
                      + N(o->mSubscribe));
        Objects(out, o->mDataObjects);
        break;
    }
    case RoutingOp_RegisterClient: {
        RoutingRegisterClientOp* o = static_cast<RoutingRegisterClientOp*>(inOp);
        Line(out, Esc(o->GetClientName()) + " pw=" + Esc(o->mServerPassword) + " reconnect=" + N(o->GetReconnectId())
                      + " af=" + X(o->GetAsyncMessageFlags()) + " rf=" + X(o->mRegisterFlags) + " cf="
                      + X(o->GetClientFlags()) + (ok ? " client=" + N(o->GetClientId()) : std::string()));
        Clients(out, "clients", o->GetClientMap());
        break;
    }
    case RoutingOp_SendChat: {
        RoutingSendChatOp* o = static_cast<RoutingSendChatOp*>(inOp);
        Line(out, "f=" + X(o->GetFlags()) + " " + Esc(o->mText) + " to=" + Ids(o->mRecipients));
        break;
    }
    case RoutingOp_SendData: {
        RoutingSendDataOp* o = static_cast<RoutingSendDataOp*>(inOp);
        Line(out, "f=" + X(o->GetFlags()) + " data=" + Hex(o->mData.get()) + " to=" + Ids(o->mRecipients));
        break;
    }
    case RoutingOp_SendServerAlert: {
        RoutingSendServerAlertOp* o = static_cast<RoutingSendServerAlertOp*>(inOp);
        Line(out, Esc(o->mAlertText) + " to=" + Ids(o->mRecipientList));
        break;
    }
    case RoutingOp_ServerAlert: Line(out, Esc(static_cast<RoutingServerAlertOp*>(inOp)->mText)); break;
    case RoutingOp_ServerShutdownAborted:
        Line(out, Esc(static_cast<RoutingServerShutdownAbortedOp*>(inOp)->mText));
        break;
    case RoutingOp_ServerShutdownStarted: {
        RoutingServerShutdownStartedOp* o = static_cast<RoutingServerShutdownStartedOp*>(inOp);
        Line(out, Esc(o->mText) + " secs=" + N(o->GetSeconds()));
        break;
    }
    case RoutingOp_SetClientFlags: {
        RoutingSetClientFlagsOp* o = static_cast<RoutingSetClientFlagsOp*>(inOp);
        Line(out, "cm=" + X(o->mClientFlagMask) + " cf=" + X(o->mClientFlags) + " am=" + X(o->mAsyncMessageFlagMask)
                      + " af=" + X(o->mAsyncMessageFlags));
        break;
    }
    case RoutingOp_SetGroupFlags: {
        RoutingSetGroupFlagsOp* o = static_cast<RoutingSetGroupFlagsOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " gm=" + X(o->mGroupFlagMask) + " gf=" + X(o->mGroupFlags) + " am="
                      + X(o->mAsyncFlagMask) + " af=" + X(o->mAsyncFlags));
        break;
    }
    case RoutingOp_StartServerShutdown: {
        RoutingStartServerShutdownOp* o = static_cast<RoutingStartServerShutdownOp*>(inOp);
        Line(out, Esc(o->mAlertText) + " secs=" + N(o->mSecondsUntilShutdown));
        break;
    }
    case RoutingOp_UnsubscribeDataObject: {
        RoutingUnsubscribeDataObjectOp* o = static_cast<RoutingUnsubscribeDataObjectOp*>(inOp);
        Line(out, "link=" + N(o->GetLinkId()) + " type=" + Esc(o->mDataType) + " f=" + N(o->GetFlags()));
        break;
    }
    case RoutingOp_YouWereBanned: {
        RoutingYouWereBannedOp* o = static_cast<RoutingYouWereBannedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " on=" + N(o->mIsBanned) + " secs=" + N(o->GetDuration()) + " "
                      + Esc(o->GetComment()));
        break;
    }
    case RoutingOp_YouWereMuted: {
        RoutingYouWereMutedOp* o = static_cast<RoutingYouWereMutedOp*>(inOp);
        Line(out, "group=" + N(o->GetGroupId()) + " on=" + N(o->GetIsMuted()) + " secs=" + N(o->GetDuration()) + " "
                      + Esc(o->GetComment()));
        break;
    }
    case RoutingOp_DerivedServerOp: {
        RoutingDerivedServerOp* o = static_cast<RoutingDerivedServerOp*>(inOp);
        Line(out, "msg=" + N(o->GetMsgType()) + " data=" + Hex(o->GetRecvMsg()));
        break;
    }
    default: break;
    }
}
#endif

static void DumpOp(const std::string& inWhy, RoutingOp* inOp)
{
    std::vector<std::string> aLines;
    aLines.push_back(inWhy + " type " + N(inOp->GetType()) + " " + St(inOp->GetStatus()));
    DumpFields(inOp, aLines);
    for (size_t i = 0; i < aLines.size(); i++)
        Log(aLines[i]);
}

// ---- completions and the pump

static WONAPICore* sCore;

class RouteDone : public CompletionBase<AsyncOpPtr> {
public:
    RouteDone(const char* inName, bool inQuiet = false) : mName(inName), mCount(0), mQuiet(inQuiet) {}
    virtual void Complete(AsyncOpPtr inOp)
    {
        this->mCount++;
        if (this->mQuiet)
            return;
        RoutingOp* anOp = dynamic_cast<RoutingOp*>(inOp.get());
        if (anOp == NULL) {
            Log(this->mName + " completed " + St(inOp->GetStatus()));
            return;
        }
        DumpOp(this->mName + " completed", anOp);
    }
    std::string mName;
    int mCount;
    bool mQuiet;
};
static void PumpRoute(RouteDone* inDone, int inCount)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
    if (inDone->mCount < inCount)
        Log(inDone->mName + " TIMED OUT waiting for " + N(inCount));
}
static void PumpFor(unsigned inMs)
{
    uint64_t anEnd = PlatTimeMilliseconds64() + inMs;
    while (PlatTimeMilliseconds64() < anEnd)
        sCore->Pump(5);
}

// the connection: the complete hook logged
class LogRouting : public RoutingConnection {
public:
    virtual void RoutingOpCompleteHook(RoutingOp* inOp)
    {
        Log("hook type " + N(inOp->GetType()) + " " + St(inOp->GetStatus()));
    }
};

// ---- the routing server stand-in

// a message body: little-endian numbers, WON's length-prefixed strings
struct W {
    std::string b;
    W& B(int v) { b += (char)(v & 0xff); return *this; }
    W& S(int v) { B(v); B(v >> 8); return *this; }
    W& L(unsigned long v) { S((int)(v & 0xffff)); S((int)(v >> 16)); return *this; }
    // a wide string of ASCII text, 2-byte (or 1-byte) unit count
    W& W2(const char* s) { size_t n = strlen(s); S((int)n); for (size_t i = 0; i < n; i++) S((unsigned char)s[i]); return *this; }
    W& W1(const char* s) { size_t n = strlen(s); B((int)n); for (size_t i = 0; i < n; i++) S((unsigned char)s[i]); return *this; }
    W& N1(const char* s) { size_t n = strlen(s); B((int)n); b += s; return *this; }
    W& Buf2(const std::string& d) { S((int)d.size()); b += d; return *this; }
    // a sized entry: its 2-byte length, then its bytes
    W& Entry(const W& e) { S((int)e.b.size()); b += e.b; return *this; }
};
// a routing server message: header 3 7 type (a derived service: 3 8 type)
static W Msg(int inType, int inService = 7) { W w; w.B(3).B(inService).B(inType); return w; }
static W Status(int inStatus) { W w = Msg(66); w.S(inStatus); return w; }

struct Server {
    SmartPtr<BlockingSocket> mListener;
    SmartPtr<AsyncSocket> mPeer;
    void Open()
    {
        this->mListener = new BlockingSocket(0);
        this->mListener->Bind(IPAddr("127.0.0.1", 0), true);
        this->mListener->Listen(5);
        sListenPort = this->mListener->GetLocalPort();
    }
    IPAddr Addr() const { return IPAddr("127.0.0.1", (unsigned short)sListenPort); }
    void Accept()
    {
        this->mListener->WaitForAccept(5000);
        Log("server accept " + St(this->mListener->Accept(this->mPeer)));
        this->Peer()->SetLengthFieldSize(2);
    }
    BlockingSocket* Peer() { return static_cast<BlockingSocket*>(this->mPeer.get()); }
    // waits for one request from the connection and logs it
    void Expect(const char* inName)
    {
        SmartPtr<RouteDone> aDone = new RouteDone(inName, true);
        SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(this->Peer());
        aRecv->SetCompletion(aDone);
        aRecv->RunAsync(5000);
        PumpRoute(aDone, 1);
        if (aRecv->GetStatus() == WS_Success)
            Log(std::string("server got ") + inName + " " + Hex(aRecv->GetMsg()));
        else
            Log(std::string("server got ") + inName + " " + St(aRecv->GetStatus()));
    }
    void Send(const char* inName, const W& inMsg)
    {
        std::string aMsg(2, '\0');
        size_t aLength = 2 + inMsg.b.size();
        aMsg[0] = (char)(aLength & 0xff);
        aMsg[1] = (char)((aLength >> 8) & 0xff);
        aMsg += inMsg.b;
        WONStatus aStatus = this->Peer()->SendMsg(Bytes(aMsg.data(), (unsigned long)aMsg.size()), 5000);
        if (aStatus != WS_Success)
            Log(std::string("server send ") + inName + " " + St(aStatus));
    }
};

// ---- the scenario

static Server sServer;
static SmartPtr<RouteDone> sAll; // the default completion: every notification

// a request: the helper's call (`run`), the server's log of it, then each reply, until the op completes
#define REQUEST(name, run, ...) \
    do { \
        Log(std::string("== ") + name); \
        SmartPtr<RouteDone> d_ = new RouteDone(name); \
        RouteDone* done = d_.get(); \
        run; \
        sServer.Expect(name); \
        W replies_[] = {__VA_ARGS__}; \
        for (size_t i_ = 0; i_ < sizeof(replies_) / sizeof(replies_[0]); i_++) \
            sServer.Send(name, replies_[i_]); \
        PumpRoute(done, 1); \
    } while (0)

static void Note(const char* inName, const W& inMsg)
{
    Log(std::string("== note ") + inName);
    int aCount = sAll->mCount;
    sServer.Send(inName, inMsg);
    PumpRoute(sAll, aCount + 1);
}

static void Requests(RoutingConnection* c)
{
    std::list<unsigned short> aTwo;
    aTwo.push_back(11);
    aTwo.push_back(0x4001);
    ByteBufferPtr aData = Bytes("\x01\x02\x03", 3);

    // register: the reply, then the client list (flag 4); the extended list (flag 0x10); a failure
    REQUEST("register", c->Register(L"me", L"pw", 5, 2, 4, 0x10, done), Msg(51).S(0).S(10).W2("me!").L(77),
            Msg(23).S(0).S(2).S(10).W2("me!").L(1).S(11).W2("Alice").L(0x20));
    REQUEST("register ex", c->Register(L"you", L"", 0, 0, 0x10, 0, done), Msg(51).S(0).S(12).W2("you").L(3),
            Msg(74).S(0).S(1).S(12).W2("you").L(2).B(2).B(1).B(4).L(4242).B(2).B(4).L(99));
    REQUEST("register no list", c->Register(L"x", L"y", 1, 0, 0, 0, done), Msg(51).S(0).S(13).W2("x").L(1));
    REQUEST("register fails", c->Register(L"x", L"y", 1, 0, 4, 0, done), Msg(51).S(-2510));
    // the client lists
    REQUEST("client list", c->GetClientList(done), Msg(23).S(0).S(2).S(5).W2("Five").L(3).S(20).W2("Twenty").L(0));
    REQUEST("client list ex", c->GetClientListEx(done),
            Msg(74).S(0).S(2).S(20).W2("me2").L(0x30).B(1).B(2).B(4).L(808).S(40).W2("Forty").L(0).B(0));
    REQUEST("client list fails", c->GetClientList(done), Msg(23).S(-1));
    // the group lists
    {
        W w = Msg(25);
        w.S(0).S(0xff).S(2);
        W g1;
        g1.S(100).W2("Lobby").S(7).S(50).L(0x800).L(0x10).S(3).S(2).S(10).B(1).S(11).B(0);
        w.Entry(g1);
        W g2;
        g2.S(102).W2("Other").S(12).S(4).L(0).L(0).S(0).S(1).S(12).B(2);
        w.Entry(g2);
        REQUEST("group list", c->GetGroupList(0xff, done), w);
    }
    {
        W w = Msg(25);
        w.S(0).S(0x21).S(1);
        W g;
        g.S(201).W2("New").S(9).B(0x55);
        w.Entry(g);
        REQUEST("group list 21", c->GetGroupList(0x21, done), w);
    }
    REQUEST("group list fails", c->GetGroupList(1, done), Msg(25).S(-3));
    // the members: a reply for another group first (not wanted), then this group's, names and flags
    {
        W other = Msg(29);
        other.S(0).S(55).B(0).S(0).S(0);
        W w = Msg(29);
        w.S(0).S(102).B(3).S(4).S(2);
        W m1;
        m1.S(12).B(1).W2("bob").L(4);
        w.Entry(m1);
        W m2;
        m2.S(17).B(0).W2("Eve").L(0x40);
        w.Entry(m2);
        REQUEST("members", c->GetMembersOfGroup(102, done), other, w);
    }
    {
        W w = Msg(29);
        w.S(0).S(103).B(0).S(1).S(1);
        W m1;
        m1.S(30).B(4);
        w.Entry(m1);
        REQUEST("members plain", c->GetMembersOfGroup(103, done), w);
    }
    REQUEST("members fails", c->GetMembersOfGroup(104, done), Msg(29).S(-5).S(104));
    // joins: with the member list (join flag 1), a wait (-2541) then success, a failure
    {
        W m = Msg(29);
        m.S(0).S(100).B(1).S(0).S(2);
        W e1;
        e1.S(10).B(1).W2("me");
        m.Entry(e1);
        W e2;
        e2.S(13).B(0).W2("Bobby");
        m.Entry(e2);
        REQUEST("join", c->JoinGroup(100, L"pw", L"hi", 1, 2, done), Msg(41).S(0).S(100).W2("welcome"), m);
    }
    REQUEST("join waits", c->JoinGroup(101, L"", L"", 0, 0, done), Msg(41).S(-2541).S(101).W2("wait"),
            Msg(41).S(0).S(99).W2("other"), Msg(41).S(0).S(101).W2("ok"));
    REQUEST("join fails", c->JoinGroup(102, L"", L"", 0, 0, done), Msg(41).S(-2524).S(102).W2("no"));
    // creates: with the join status (join flag 1); without, the parent id after the name; a failure
    REQUEST("create", c->CreateGroup(0xffff, L"Game", L"pw", 8, 0x2, 1, 0x4, done),
            Msg(12).S(0).S(300).W2("Game!"), Msg(41).S(0));
    REQUEST("create no join", c->CreateGroup(5, L"G2", L"", 4, 0, 0, 0, done), Msg(12).S(0).S(301).W2("G2").S(7));
    REQUEST("create fails", c->CreateGroup(5, L"G3", L"", 4, 0, 0, 0, done), Msg(12).S(-9));
    // the status requests
    REQUEST("leave", c->LeaveGroup(100, done), Status(0));
    REQUEST("cancel join", c->CancelJoinGroup(101, false, done), Status(-1));
    REQUEST("group flags", c->SetGroupFlags(100, 0x3, 0x2, 0x10, 0x10, done), Status(0));
    REQUEST("alert", c->SendServerAlert(L"hear ye", done), Status(0));
    REQUEST("alert to", c->SendServerAlertWithRecipientList(L"to two", aTwo, done), Status(0));
    REQUEST("start shutdown", c->StartServerShutdown(L"bye", 300, done), Status(0));
    REQUEST("abort shutdown", c->AbortServerShutdown(done), Status(0));
    REQUEST("client flags", c->SetClientFlags(0x12345, 0x40, 0x6, 0x4, done), Status(0));
    REQUEST("chat", c->SendChat(L"hello", 0x105, 11, done), Status(0));
    REQUEST("chat to", c->SendChatTo(L"hi two", 0x3, aTwo, done), Status(0));
    REQUEST("data", c->SendData(aData, 0x81, 11, done), Status(0));
    REQUEST("data to", c->SendData(aData, 0x1, aTwo, done), Status(0));
    REQUEST("moderator", c->BecomeModerator(true, done), Status(0));
    REQUEST("accept", c->AcceptClient(300, 11, true, L"yes", done), Status(0));
    REQUEST("invite", c->InviteClient(300, 12, false, L"go", done), Status(0));
    REQUEST("ban", c->BanClient(100, true, 60, L"spam", 11, done), Status(0));
    REQUEST("ban won", c->BanClientByWONId(0xffff, false, 0, L"", 123456, done), Status(0));
    REQUEST("ban cdkey", c->BanClientByCDKeyId(100, true, 5, L"cd", 777, done), Status(0));
    REQUEST("mute", c->MuteClient(100, true, 600, L"loud", 12, done), Status(0));
    REQUEST("mute won", c->MuteClientByWONId(100, false, 0, L"", 654321, done), Status(0));
    REQUEST("mute cdkey", c->MuteClientByCDKeyId(100, true, 9, L"cdm", 888, done), Status(0));
    REQUEST("create data", c->CreateDataObject(2, 300, "Game", L"g", aData, done), Status(0));
    REQUEST("replace data", c->ReplaceDataObject(300, "Game", L"g", done), Status(0));
    REQUEST("modify data", c->ModifyDataObject(300, "Game", L"g", 4, true, aData, done), Status(0));
    REQUEST("unsubscribe", c->UnsubscribeDataObject(0xffff, "Game", 0x101, done), Status(0));
    REQUEST("group name", c->SetGroupName(300, L"Renamed", done), Status(0));
    REQUEST("group max players", c->SetGroupMaxPlayers(300, 6, done), Status(0));
    REQUEST("relinquish", c->RelinquishCaptaincy(300, 12, done), Status(0));
    REQUEST("status fails", c->LeaveGroup(5, done), Status(-2520));
    // the data object reads
    {
        W w = Msg(49);
        w.S(0).S(2);
        w.S(300).N1("Game").W1("g").Buf2(std::string("\x0a\x0b", 2));
        w.S(301).N1("Game").W1("").Buf2(std::string());
        REQUEST("read data", c->ReadDataObject(0xffff, "Game", 0x101, done), w);
    }
    {
        W w = Msg(49);
        w.S(0).S(1).S(12).N1("_KeyId").W1("").Buf2(std::string("\x01\x00\x00\x00\x2b\x02\x00\x00", 8));
        REQUEST("subscribe data", c->SubscribeDataObject(0xffff, "Room", 1, done), w);
        REQUEST("cd key ids", c->GetClientCDKeyIdList(done, true), w);
    }
    REQUEST("read data fails", c->ReadDataObject(7, "X", 0, done), Msg(49).S(-4));
    // the bad user lists: all four optional fields, none
    {
        W w = Msg(21);
        w.S(0).B(2).S(100).B(0x0f).S(2);
        W u1;
        u1.L(5001).W2("Mallory").L(3600).L(42).W2("Mod").W2("spam");
        w.Entry(u1);
        W u2;
        u2.L(5002).W2("Trudy").L(0).L(0).W2("").W2("");
        w.Entry(u2);
        REQUEST("bad users", c->GetBadUserList(RoutingBadUserListType_MutedUsers, 100, 0x0f, done), w);
        W v = Msg(21);
        v.S(0).B(1).S(0xffff).B(0).S(1);
        W u3;
        u3.L(6001).W2("Eve");
        v.Entry(u3);
        REQUEST("bad users plain", c->GetBadUserList(RoutingBadUserListType_BannedUsers, 0xffff, 0, done), v);
    }
    // the firewall check without listening: the status, then its result
    REQUEST("firewall", c->DetectFirewall(0, 5, false, done), Status(0), Msg(19).S(0).S(4321));

    // the ops the lobby (RoutingLogic) builds itself
    {
        Log("== logic ban");
        SmartPtr<RouteDone> d = new RouteDone("logic ban");
        SmartPtr<RoutingBanClientOp> ban = new RoutingBanClientOp(c);
        ban->SetBanComment(L"why");
        ban->SetBanTime(120);
        ban->SetGroupId(100);
        ban->SetWONId(5555, true);
        ban->SetCompletion(d);
        ban->RunAsync(-1);
        sServer.Expect("logic ban");
        sServer.Send("status", Status(0));
        PumpRoute(d, 1);
        SmartPtr<RouteDone> d2 = new RouteDone("logic kick");
        SmartPtr<RoutingBanClientOp> kick = new RoutingBanClientOp(c);
        kick->SetBanTime(1);
        kick->SetGroupId(300);
        kick->SetClientId(12);
        kick->SetCompletion(d2);
        kick->RunAsync(-1);
        sServer.Expect("logic kick");
        sServer.Send("status", Status(0));
        PumpRoute(d2, 1);
        Log("kick group " + N(kick->GetGroupId()) + " client " + N(kick->GetClientId()));
    }
    {
        Log("== logic unmute");
        SmartPtr<RouteDone> d = new RouteDone("logic unmute");
        SmartPtr<RoutingMuteClientOp> mute = new RoutingMuteClientOp(c);
        mute->SetMuteComment(L"ok");
        mute->SetMuteTime(0);
        mute->SetGroupId(0xffff);
        mute->SetWONId(4444, true);
        mute->SetIsMuted(false);
        mute->SetCompletion(d);
        mute->RunAsync(-1);
        sServer.Expect("logic unmute");
        sServer.Send("status", Status(0));
        PumpRoute(d, 1);
        Log("unmute group " + N(mute->GetGroupId()) + " muted " + N(mute->GetIsMuted()) + " client "
            + N(mute->GetClientId()));
    }
    {
        Log("== logic create");
        SmartPtr<RouteDone> d = new RouteDone("logic create");
        SmartPtr<RoutingCreateGroupOp> op = new RoutingCreateGroupOp(c);
        op->SetGroupPassword(L"secret");
        op->SetGroupName(L"MyGame");
        op->SetGroupFlags(0x4);
        op->SetJoinFlags(1);
        op->SetLeaveGroupOnCancel(true);
        op->SetCompletion(d);
        op->RunAsync(-1);
        sServer.Expect("logic create");
        sServer.Send("create reply", Msg(12).S(0).S(400).W2("MyGame"));
        sServer.Send("join status", Msg(41).S(0));
        PumpRoute(d, 1);
        Log("created group " + N(op->GetGroupId()));
    }
    {
        Log("== logic join retry");
        SmartPtr<RouteDone> d = new RouteDone("logic join");
        SmartPtr<RoutingJoinGroupOp> op = new RoutingJoinGroupOp(c);
        op->SetGroupId(400);
        op->SetGroupPassword(L"wrong");
        op->SetCompletion(d);
        op->RunAsync(-1);
        sServer.Expect("logic join");
        sServer.Send("join bad password", Msg(41).S(-2524).S(400).W2(""));
        PumpRoute(d, 1);
        PumpFor(50); // the first run settles before the op runs again
        op->SetGroupPassword(L"secret");
        op->RunAsync(-1);
        sServer.Expect("logic join again");
        sServer.Send("join ok", Msg(41).S(0).S(400).W2(""));
        PumpRoute(d, 2);
        Log("joined " + N(op->GetGroupId()) + " killed " + N(op->IsKilled()));
    }
    {
        Log("== logic bad users");
        SmartPtr<RouteDone> d = new RouteDone("logic bad users");
        SmartPtr<RoutingGetBadUserListOp> op =
            new RoutingGetBadUserListOp(c, RoutingBadUserListType_BannedUsers, 300, 0x0f);
        op->SetCompletion(d);
        op->RunAsync(-1);
        sServer.Expect("logic bad users");
        W w = Msg(21);
        w.S(0).B(1).S(300).B(0x01).S(1);
        W u;
        u.L(7001).W2("Oscar").L(30);
        w.Entry(u);
        sServer.Send("bad users", w);
        PumpRoute(d, 1);
    }
    {
        Log("== logic register");
        SmartPtr<RouteDone> d = new RouteDone("logic register");
        SmartPtr<RoutingRegisterClientOp> op = new RoutingRegisterClientOp(c);
        op->mRegisterFlags = 0x15;
        op->mReconnectId = 0x1234;
        op->mClientFlags = 0x44;
        op->SetCompletion(d);
        op->RunAsync(-1);
        sServer.Expect("logic register");
        sServer.Send("reply", Msg(51).S(0).S(50).W2("anon").L(0x5678));
        sServer.Send("clients", Msg(23).S(0).S(1).S(50).W2("anon").L(0x44));
        PumpRoute(d, 1);
        Log("register reconnect " + N(op->GetReconnectId()) + " client " + N(op->GetClientId()) + " clients "
            + N(op->GetClientMap().size()));
    }
    {
        Log("== logic group list");
        SmartPtr<RouteDone> d = new RouteDone("logic group list");
        SmartPtr<RoutingGetGroupListOp> op = new RoutingGetGroupListOp(c, 0xff);
        op->SetCompletion(d);
        op->RunAsync(-1);
        sServer.Expect("logic group list");
        W w = Msg(25);
        w.S(0).S(0xff).S(1);
        W g;
        g.S(400).W2("MyGame").S(50).S(8).L(0x4).L(0).S(0).S(1).S(50).B(1);
        w.Entry(g);
        sServer.Send("groups", w);
        PumpRoute(d, 1);
    }
    // a chat without a reply wanted (flag 1 clear): sent, never added to the list
    {
        Log("== chat no reply");
        SmartPtr<RouteDone> d = new RouteDone("chat no reply");
        c->SendChat(L"fire and forget", 0x4, 12, d);
        sServer.Expect("chat no reply");
        Log("chat no reply completions " + N(d->mCount));
        SmartPtr<RouteDone> d2 = new RouteDone("disconnect");
        c->DisconnectClient(d2);
        sServer.Expect("disconnect");
        Log("disconnect completions " + N(d2->mCount));
        SmartPtr<RouteDone> d3 = new RouteDone("alert to none");
        std::list<unsigned short> aNone;
        c->SendServerAlertWithRecipientList(L"nobody", aNone, d3);
        PumpFor(50);
        Log("alert to none completions " + N(d3->mCount));
    }
}

static void Notifications()
{
    Note("client flags", Msg(6).S(11).L(0x20));
    Note("joined group plain", Msg(7).B(0).S(100).S(15).B(1));
    Note("joined group name", Msg(7).B(1).S(100).S(16).B(2).W2("Dave"));
    Note("joined group flags", Msg(7).B(2).S(100).S(17).B(3).L(0x77));
    Note("joined group both", Msg(7).B(3).S(101).S(18).B(4).W2("Fred").L(0x9));
    Note("joined server", Msg(8).S(15).W2("Dave").L(0x4));
    Note("joined server ex", Msg(8).S(16).W2("Gina").L(0).B(3).B(1).B(4).L(31337).B(2).B(4).L(4242).B(9).B(1).B(0x55));
    Note("left group", Msg(9).S(100).S(15));
    Note("left server", Msg(10).S(15));
    Note("data created", Msg(14).S(300).N1("Game").W1("g").Buf2(std::string("\x01\x02", 2)));
    Note("data deleted", Msg(15).S(300).N1("Game").W1("g"));
    Note("data modified", Msg(16).S(300).N1("Game").W1("g").S(2).B(1).Buf2(std::string("\x09", 1)));
    Note("captain", Msg(31).S(100).S(10));
    Note("group created", Msg(32).S(500).W2("Fresh").S(16).L(0x800).L(0x3));
    Note("group deleted", Msg(33).S(500));
    Note("group flags", Msg(34).S(100).L(0x801).L(0x5));
    Note("invitation", Msg(35).S(300).S(12).B(1).W2("come"));
    Note("join attempt", Msg(36).S(13).S(300).W2("let me in").B(2));
    Note("max players", Msg(37).S(300).S(6));
    Note("member count", Msg(71).S(100).S(42));
    Note("group name", Msg(38).S(100).W2("Lobby2"));
    Note("observers", Msg(39).S(100).S(7));
    Note("member flags", Msg(44).S(100).S(11).B(3));
    Note("peer chat", Msg(47).S(12).S(0x104).W2("psst").S(2).S(10).S(11));
    Note("peer chat none", Msg(47).S(13).S(0x100).W2("team").S(0));
    Note("peer data", Msg(48).S(12).Buf2(std::string("\x05\x06\x07", 3)));
    Note("server alert", Msg(57).W2("alert!"));
    Note("shutdown aborted", Msg(58));
    Note("shutdown started", Msg(59).W2("going down").L(120));
    Note("banned", Msg(69).S(100).B(1).W2("bye").L(5));
    Note("unbanned", Msg(69).S(0xffff).B(0).W2("").L(0));
    Note("muted", Msg(70).S(300).B(1).W2("hush").L(60));
    Note("derived", Msg(9, 8).B(1).B(2).B(3));
    Note("derived empty", Msg(4, 8));
}

static void ParseGroupBody()
{
    Log("== parse group list body");
    SmartPtr<RoutingGetGroupListOp> op = new RoutingGetGroupListOp;
    W w;
    w.S(0x81).S(2);
    W g1;
    g1.S(7).W2("Seven").S(2).S(1).B(1).S(2).B(0);
    w.Entry(g1);
    W g2;
    g2.S(8).W2("Eight").S(0);
    w.Entry(g2);
    bool ok = op->ParseGroupListObj(w.b.data(), (unsigned long)w.b.size());
    std::vector<std::string> aLines;
    Groups(aLines, op->GetGroupMap());
    Log("parsed " + N(ok) + " flags " + X(op->GetFlags()));
    for (size_t i = 0; i < aLines.size(); i++)
        Log(aLines[i]);
    W cut;
    cut.S(0x01).S(2);
    W g3;
    g3.S(9).W2("Nine");
    cut.Entry(g3);
    cut.S(40).S(10);
    ok = op->ParseGroupListObj(cut.b.data(), (unsigned long)cut.b.size());
    aLines.clear();
    Groups(aLines, op->GetGroupMap());
    Log("parsed cut " + N(ok) + " flags " + X(op->GetFlags()));
    for (size_t i = 0; i < aLines.size(); i++)
        Log(aLines[i]);
}

static void Scenario()
{
    sServer.Open();
    SmartPtr<LogRouting> c = new LogRouting;
    c->SetKeepAlivePeriod(0);
    sAll = new RouteDone("default");
    c->SetDefaultOpCompletion(sAll);
    Log("connect " + St(c->ConnectBlock(sServer.Addr(), 5000)));
    sServer.Accept();
    Requests(c);
    Notifications();
    c->Close();
    sServer.Peer()->Close();
    sServer.mListener->Close();
    PumpFor(100);
    c = NULL;
    ParseGroupBody();
    Log("route ops done");
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_routeops.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        Scenario();
        sAll = NULL;
        aCore.Pump(50);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_routeops_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
