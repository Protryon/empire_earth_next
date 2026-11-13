// The routing server's protocol through ee_won's routemsg module (rust/crates/ee_won/src/routemsg.rs): the routing
// ops hand their members over for a request (RouteRequest) and apply a reply's assignments, made in the original's
// order, to their members and records (RouteReply). Each op's request and reply is a template over the op so that
// tests/won/won_routemsg_test.cpp runs the same code over stand-ins with the ops' members.
#pragma once
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"

namespace WONAPI {

// ---- requests

class RouteRequest {
public:
    RouteRequest(int inKind, bool inDerived)
    {
        memset(&this->mReq, 0, sizeof(this->mReq));
        this->mReq.kind = inKind;
        this->mReq.derived = inDerived;
    }
    EeWonRouteRequest mReq;
    EeWonDirBytes Text(const std::wstring& inText)
    {
        this->mTexts.push_back(std::string());
        WideAppendUtf16LE(this->mTexts.back(), inText);
        return Narrow(this->mTexts.back());
    }
    static EeWonDirBytes Narrow(const std::string& inText)
    {
        EeWonDirBytes b = {reinterpret_cast<const uint8_t*>(inText.data()), inText.size(), true};
        return b;
    }
    static EeWonDirBytes Buffer(const ByteBuffer* inBuffer)
    {
        EeWonDirBytes b = {NULL, 0, false};
        if (inBuffer != NULL) {
            b.data = reinterpret_cast<const uint8_t*>(inBuffer->data());
            b.len = inBuffer->length();
            b.present = true;
        }
        return b;
    }
    void Ids(const std::list<unsigned short>& inIds)
    {
        this->mIds.assign(inIds.begin(), inIds.end());
        this->mReq.ids = this->mIds.empty() ? NULL : &this->mIds[0];
        this->mReq.id_count = this->mIds.size();
    }
    template <class Map> void UserIds(const Map& inMap)
    {
        for (typename Map::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
            this->mUserIds.push_back(static_cast<uint32_t>(i->first));
        this->mReq.user_ids = this->mUserIds.empty() ? NULL : &this->mUserIds[0];
        this->mReq.user_id_count = this->mUserIds.size();
    }
    // the message replaces the buffer's contents (InitSendMsg's Reset, then the appends)
    void Into(WriteBuffer& outMsg)
    {
        std::string aBytes;
        ee_won_route_request(&this->mReq, Append, &aBytes);
        outMsg.Reset();
        if (!aBytes.empty()) outMsg.AppendBytes(aBytes.data(), (unsigned long)aBytes.size());
    }
    static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
    }

private:
    std::list<std::string> mTexts;
    std::vector<uint16_t> mIds;
    std::vector<uint32_t> mUserIds;
};

template <class Op> void RouteSendHeader(Op* op, unsigned char inType)
{
    RouteRequest q(EE_WON_RT_REQ_HEADER, op->mIsDerivedServerOp);
    q.mReq.byte1 = inType;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendBan(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_BAN, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.on = op->mIsBanned;
    q.mReq.longs[0] = (uint32_t)op->mBanTime;
    q.mReq.text1 = q.Text(op->mBanComment);
    q.mReq.by_won_id = op->mBanByWONId;
    q.mReq.is_cdkey = op->mWONIdIsCDKeyId;
    q.mReq.longs[1] = (uint32_t)op->mWONId;
    q.mReq.client_id = op->mClientId;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendMute(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_MUTE, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.on = op->mIsMuted;
    q.mReq.longs[0] = (uint32_t)op->mMuteTime;
    q.mReq.text1 = q.Text(op->mMuteComment);
    q.mReq.by_won_id = op->mMuteByWONId;
    q.mReq.is_cdkey = op->mWONIdIsCDKeyId;
    q.mReq.longs[1] = (uint32_t)op->mWONId;
    q.mReq.client_id = op->mClientId;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendCancelJoin(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_CANCEL_JOIN, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.on = op->mLeaveIfAlreadyInGroup;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendGroupId(Op* op, int inKind)
{
    RouteRequest q(inKind, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendSetGroupFlags(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_SET_GROUP_FLAGS, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.longs[0] = (uint32_t)op->mGroupFlagMask;
    q.mReq.longs[1] = (uint32_t)op->mGroupFlags;
    q.mReq.longs[2] = (uint32_t)op->mAsyncFlagMask;
    q.mReq.longs[3] = (uint32_t)op->mAsyncFlags;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendServerAlert(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_SERVER_ALERT, op->mIsDerivedServerOp);
    q.mReq.text1 = q.Text(op->mAlertText);
    q.Ids(op->mRecipientList);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendStartShutdown(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_START_SHUTDOWN, op->mIsDerivedServerOp);
    q.mReq.text1 = q.Text(op->mAlertText);
    q.mReq.longs[0] = (uint32_t)op->mSecondsUntilShutdown;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendBare(Op* op, int inKind)
{
    RouteRequest q(inKind, op->mIsDerivedServerOp);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendSetClientFlags(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_SET_CLIENT_FLAGS, op->mIsDerivedServerOp);
    q.mReq.longs[0] = (uint32_t)op->mAsyncMessageFlagMask;
    q.mReq.longs[1] = (uint32_t)op->mAsyncMessageFlags;
    q.mReq.longs[2] = (uint32_t)op->mClientFlagMask;
    q.mReq.longs[3] = (uint32_t)op->mClientFlags;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendChat(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_SEND_CHAT, op->mIsDerivedServerOp);
    q.mReq.flags16 = op->mFlags;
    q.mReq.text1 = q.Text(op->mText);
    q.Ids(op->mRecipients);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendData(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_SEND_DATA, op->mIsDerivedServerOp);
    q.mReq.byte1 = op->mFlags;
    q.mReq.data = RouteRequest::Buffer(op->mData.get());
    q.Ids(op->mRecipients);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendBecomeModerator(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_BECOME_MODERATOR, op->mIsDerivedServerOp);
    q.mReq.on = op->mModeratorOn;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendAcceptClient(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_ACCEPT_CLIENT, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.client_id = op->mClientId;
    q.mReq.on = op->mAccepted;
    q.mReq.text1 = q.Text(op->mAcceptComment);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendInvite(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_INVITE, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.client_id = op->mClientId;
    q.mReq.on = op->mInvited;
    q.mReq.text1 = q.Text(op->mInviteComment);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendCreateData(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_CREATE_DATA, op->mIsDerivedServerOp);
    q.mReq.byte1 = op->mFlags;
    q.mReq.link_id = op->mLinkId;
    q.mReq.narrow = RouteRequest::Narrow(op->mDataType);
    q.mReq.text1 = q.Text(op->mDataName);
    q.mReq.data = RouteRequest::Buffer(op->mData.get());
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendDeleteData(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_DELETE_DATA, op->mIsDerivedServerOp);
    q.mReq.link_id = op->mLinkId;
    q.mReq.narrow = RouteRequest::Narrow(op->mDataType);
    q.mReq.text1 = q.Text(op->mDataName);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendModifyData(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_MODIFY_DATA, op->mIsDerivedServerOp);
    q.mReq.link_id = op->mLinkId;
    q.mReq.narrow = RouteRequest::Narrow(op->mDataType);
    q.mReq.text1 = q.Text(op->mDataName);
    q.mReq.offset = op->mOffset;
    q.mReq.on = op->mIsInsert;
    q.mReq.data = RouteRequest::Buffer(op->mData.get());
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendReadData(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_READ_DATA, op->mIsDerivedServerOp);
    q.mReq.on = op->mSubscribe;
    q.mReq.link_id = op->mLinkId;
    q.mReq.narrow = RouteRequest::Narrow(op->mDataType);
    q.mReq.byte1 = op->mFlag;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendUnsubscribe(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_UNSUBSCRIBE, op->mIsDerivedServerOp);
    q.mReq.link_id = op->mLinkId;
    q.mReq.narrow = RouteRequest::Narrow(op->mDataType);
    q.mReq.byte1 = op->mFlags;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendCreateGroup(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_CREATE_GROUP, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.text1 = q.Text(op->mGroupName);
    q.mReq.text2 = q.Text(op->mGroupPassword);
    q.mReq.max_players = op->mMaxPlayers;
    q.mReq.byte1 = op->mJoinFlags;
    q.mReq.longs[0] = (uint32_t)op->mGroupFlags;
    q.mReq.longs[1] = (uint32_t)op->mAsyncFlags;
    q.mReq.parent_id = op->mParentGroupId;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendJoinGroup(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_JOIN_GROUP, op->mIsDerivedServerOp);
    q.mReq.group_id = op->mGroupId;
    q.mReq.text1 = q.Text(op->mGroupPassword);
    q.mReq.text2 = q.Text(op->mJoinComment);
    q.mReq.byte1 = op->mJoinFlags;
    q.mReq.byte2 = op->mMemberFlags;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendRegister(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_REGISTER, op->mIsDerivedServerOp);
    q.mReq.text1 = q.Text(op->mClientName);
    q.mReq.text2 = q.Text(op->mServerPassword);
    q.mReq.longs[0] = (uint32_t)op->mReconnectId;
    q.mReq.longs[1] = (uint32_t)op->mAsyncMessageFlags;
    q.mReq.longs[2] = (uint32_t)op->mRegisterFlags;
    q.mReq.longs[3] = (uint32_t)op->mClientFlags;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendClientList(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_CLIENT_LIST, op->mIsDerivedServerOp);
    q.mReq.is_ex = op->mIsEx;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendGroupList(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_GROUP_LIST, op->mIsDerivedServerOp);
    q.mReq.flags16 = op->mFlags;
    q.mReq.parent_id = op->mParentGroupId;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendBadUsers(Op* op)
{
    RouteRequest q(EE_WON_RT_REQ_BAD_USERS, op->mIsDerivedServerOp);
    q.mReq.byte1 = (unsigned char)op->mListType;
    q.mReq.group_id = op->mGroupId;
    q.mReq.byte2 = op->mFlags;
    q.Into(op->mSendMsg);
}
// (the next ops keep their members private: they hand them over)
template <class Op> void RouteSendFirewall(Op* op, unsigned short inListenPort, unsigned long inMaxConnectWaitTime)
{
    RouteRequest q(EE_WON_RT_REQ_FIREWALL, op->mIsDerivedServerOp);
    q.mReq.port = inListenPort;
    q.mReq.longs[0] = (uint32_t)inMaxConnectWaitTime;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendGroupText(Op* op, int inKind, unsigned short inGroupId, const std::wstring& inText)
{
    RouteRequest q(inKind, op->mIsDerivedServerOp);
    q.mReq.group_id = inGroupId;
    q.mReq.text1 = q.Text(inText);
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendSetGroupMaxPlayers(Op* op, unsigned short inGroupId, unsigned short inMaxPlayers)
{
    RouteRequest q(EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS, op->mIsDerivedServerOp);
    q.mReq.group_id = inGroupId;
    q.mReq.max_players = inMaxPlayers;
    q.Into(op->mSendMsg);
}
template <class Op> void RouteSendRelinquishCaptain(Op* op, unsigned short inGroupId, unsigned short inNewCaptainId)
{
    RouteRequest q(EE_WON_RT_REQ_RELINQUISH_CAPTAIN, op->mIsDerivedServerOp);
    q.mReq.group_id = inGroupId;
    q.mReq.client_id = inNewCaptainId;
    q.Into(op->mSendMsg);
}
// RoutingGetUserStatusOp's datagram (always the routing service's header)
template <class Map> ByteBufferPtr RouteUserStatusRequest(const Map& inStatusMap)
{
    RouteRequest q(EE_WON_RT_REQ_USER_STATUS, false);
    q.UserIds(inStatusMap);
    WriteBuffer aMessage(0, true);
    q.Into(aMessage);
    return aMessage.ToByteBuffer(true);
}
// RoutingGetGroupListReqOp's request
inline ByteBufferPtr RouteGroupListRequest(unsigned char inLengthFieldSize, unsigned short inFlags)
{
    RouteRequest q(EE_WON_RT_REQ_GROUP_LIST_REQ, false);
    q.mReq.flags16 = inFlags;
    WriteBuffer aRequest(inLengthFieldSize, true);
    q.Into(aRequest);
    return aRequest.ToByteBuffer(true);
}

// ---- replies

template <class T> inline void RouteSet(T& outValue, const EeWonRouteEvent& inEvent) { outValue = (T)inEvent.num; }

// A reply read by Rust: its assignments, applied by the op through Each; End returns the status or throws
// ReadBufferException where the original's read failed
class RouteReply {
public:
    RouteReply(int inKind, unsigned char inType, ReadBuffer& inMsg, const EeWonRouteState* inState = NULL)
        : mData(inMsg.data())
    {
        EeWonRouteState aState;
        memset(&aState, 0, sizeof(aState));
        ee_won_route_reply(inKind, inType, reinterpret_cast<const uint8_t*>(inMsg.data()), inMsg.length(),
                           (size_t)inMsg.pos(), inState != NULL ? inState : &aState, Collect, &this->mEvents,
                           &this->mResult);
    }
    // a whole message (CheckResponse, ParseGroupListObj)
    RouteReply(int inKind, const void* inData, unsigned long inLength, const EeWonRouteState* inState = NULL)
        : mData(static_cast<const char*>(inData))
    {
        EeWonRouteState aState;
        memset(&aState, 0, sizeof(aState));
        ee_won_route_reply(inKind, 0, reinterpret_cast<const uint8_t*>(inData), inLength, 0,
                           inState != NULL ? inState : &aState, Collect, &this->mEvents, &this->mResult);
    }
    std::vector<EeWonRouteEvent> mEvents;
    EeWonRouteResult mResult;
    const char* mData;

    void Text(const EeWonRouteEvent& e, std::wstring& outText) const { WideAssignUtf16LE(outText, this->mData + e.at, e.len); }
    void Narrow(const EeWonRouteEvent& e, std::string& outText) const { outText.assign(this->mData + e.at, e.len); }
    ByteBufferPtr Buf(const EeWonRouteEvent& e) const { return new ByteBuffer(this->mData + e.at, e.len, false); }
    template <class P> void Buf(const EeWonRouteEvent& e, P& outBuffer) const { outBuffer = this->Buf(e); }
    WONStatus End(ReadBuffer& inMsg) const
    {
        inMsg.SetPos((int)this->mResult.pos);
        return this->End();
    }
    WONStatus End() const
    {
        if (this->mResult.cut) throw ReadBufferException("Attempt to read past end of buffer.");
        return (WONStatus)this->mResult.status;
    }
    static void Collect(void* inContext, const EeWonRouteEvent* inEvents, size_t inCount)
    {
        std::vector<EeWonRouteEvent>& v = *static_cast<std::vector<EeWonRouteEvent>*>(inContext);
        v.assign(inEvents, inEvents + inCount);
    }
};

// The records a reply builds, from the op's factories, into the containers the op names
template <class Op> class RouteRecords {
public:
    RouteRecords(Op* inOp, const RouteReply& inReply)
        : mOp(inOp), mReply(inReply), mClients(NULL), mMembers(NULL), mGroups(NULL), mObjects(NULL),
          mBadUsers(NULL), mOpClient(NULL), mCommitClients(NULL), mCommitMembers(NULL) {}
    Op* mOp;
    const RouteReply& mReply;
    RoutingClientMap* mClients;
    RoutingMemberMap* mMembers;
    RoutingGroupMap* mGroups;
    RoutingDataObjectList* mObjects;
    RoutingBadUserList* mBadUsers;
    RoutingClientInfoPtr* mOpClient;
    RoutingClientMap* mCommitClients;
    RoutingMemberMap* mCommitMembers;
    RoutingClientInfoPtr mClient;
    RoutingMemberInfoPtr mMember;
    RoutingGroupInfoPtr mGroup;
    RoutingDataObjectPtr mObject;
    RoutingBadUser mBad;

    // true: a record's event (applied here); false: the op's own field or action
    bool Apply(const EeWonRouteEvent& e)
    {
        const RouteReply& r = this->mReply;
        switch (e.tag) {
        case EE_WON_RT_NEW:
            switch (e.slot) {
            case EE_WON_RT_KIND_CLIENT: this->mClient = this->mOp->GetNewClientInfo(); break;
            case EE_WON_RT_KIND_MEMBER: this->mMember = this->mOp->GetNewMemberInfo(); break;
            case EE_WON_RT_KIND_GROUP: this->mGroup = this->mOp->GetNewGroupInfo(); break;
            case EE_WON_RT_KIND_OBJECT: this->mObject = new RoutingDataObject; break;
            case EE_WON_RT_KIND_BAD_USER: this->mBad = RoutingBadUser(); break;
            case EE_WON_RT_KIND_MEMBER_CLIENT:
                this->mMember->mClientInfo = this->mOp->GetNewClientInfo();
                this->mClient = this->mMember->mClientInfo;
                break;
            case EE_WON_RT_KIND_OP_CLIENT:
                *this->mOpClient = this->mOp->GetNewClientInfo();
                this->mClient = *this->mOpClient;
                break;
            }
            return true;
        case EE_WON_RT_REC:
            switch (e.slot) {
            case EE_WON_RT_CLIENT_ID: RouteSet(this->mClient->mClientId, e); break;
            case EE_WON_RT_CLIENT_FLAGS: RouteSet(this->mClient->mFlags, e); break;
            case EE_WON_RT_CLIENT_USER_ID: RouteSet(this->mClient->mUserId, e); break;
            case EE_WON_RT_CLIENT_ADDRESS: RouteSet(this->mClient->mAddress, e); break;
            case EE_WON_RT_MEMBER_ID: RouteSet(this->mMember->mClientId, e); break;
            case EE_WON_RT_MEMBER_FLAGS: RouteSet(this->mMember->mFlags, e); break;
            case EE_WON_RT_GROUP_ID: RouteSet(this->mGroup->mId, e); break;
            case EE_WON_RT_GROUP_CAPTAIN: RouteSet(this->mGroup->mCaptainId, e); break;
            case EE_WON_RT_GROUP_MAX_CLIENTS: RouteSet(this->mGroup->mMaxClients, e); break;
            case EE_WON_RT_GROUP_FLAGS: RouteSet(this->mGroup->mFlags, e); break;
            case EE_WON_RT_GROUP_ASYNC_FLAGS: RouteSet(this->mGroup->mAsyncFlags, e); break;
            case EE_WON_RT_GROUP_OBSERVERS: RouteSet(this->mGroup->mObserverCount, e); break;
            case EE_WON_RT_GROUP_MAX_PLAYERS: RouteSet(this->mGroup->mMaxPlayers, e); break;
            case EE_WON_RT_OBJECT_LINK: RouteSet(this->mObject->mLinkId, e); break;
            case EE_WON_RT_BAD_WON_ID: RouteSet(this->mBad.mWONId, e); break;
            case EE_WON_RT_BAD_EXPIRATION: RouteSet(this->mBad.mExpirationDiff, e); break;
            case EE_WON_RT_BAD_MODERATOR_ID: RouteSet(this->mBad.mModeratorWONId, e); break;
            }
            return true;
        case EE_WON_RT_REC_TEXT:
            switch (e.slot) {
            case EE_WON_RT_CLIENT_NAME: r.Text(e, this->mClient->mName); break;
            case EE_WON_RT_GROUP_NAME: r.Text(e, this->mGroup->mName); break;
            case EE_WON_RT_OBJECT_NAME: r.Text(e, this->mObject->mName); break;
            case EE_WON_RT_BAD_NAME: r.Text(e, this->mBad.mName); break;
            case EE_WON_RT_BAD_MODERATOR_NAME: r.Text(e, this->mBad.mModeratorName); break;
            case EE_WON_RT_BAD_COMMENT: r.Text(e, this->mBad.mModeratorComment); break;
            }
            return true;
        case EE_WON_RT_REC_NARROW:
            if (e.slot == EE_WON_RT_OBJECT_TYPE) r.Narrow(e, this->mObject->mDataType);
            return true;
        case EE_WON_RT_REC_BUF:
            if (e.slot == EE_WON_RT_OBJECT_DATA) this->mObject->mData = r.Buf(e);
            return true;
        case EE_WON_RT_INSERT:
            switch (e.slot) {
            case EE_WON_RT_INTO_CLIENTS: (*this->mClients)[this->mClient->mClientId] = this->mClient; break;
            case EE_WON_RT_INTO_MEMBERS: (*this->mMembers)[this->mMember->mClientId] = this->mMember; break;
            case EE_WON_RT_INTO_GROUP_MEMBERS: this->mGroup->mMembers[this->mMember->mClientId] = this->mMember; break;
            case EE_WON_RT_INTO_GROUPS: (*this->mGroups)[this->mGroup->mId] = this->mGroup; break;
            case EE_WON_RT_INTO_OBJECTS: this->mObjects->push_back(this->mObject); break;
            case EE_WON_RT_INTO_BAD_USERS: this->mBadUsers->push_back(this->mBad); break;
            }
            return true;
        case EE_WON_RT_ACT:
            switch (e.slot) {
            case EE_WON_RT_ACT_CLEAR_GROUPS: this->mGroups->clear(); return true;
            case EE_WON_RT_ACT_CLEAR_BAD_USERS: this->mBadUsers->clear(); return true;
            case EE_WON_RT_ACT_COMMIT_CLIENTS: *this->mCommitClients = *this->mClients; return true;
            case EE_WON_RT_ACT_COMMIT_MEMBERS: *this->mCommitMembers = *this->mMembers; return true;
            }
            return false;
        }
        return false;
    }
};

// the status replies (message 66) of every op that only waits for its status
inline WONStatus RouteStatusReply(unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_STATUS, inType, inMsg);
    return r.End(inMsg);
}

// ---- the notifications

template <class Op> WONStatus RouteClientFlagsChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_CLIENT_FLAGS, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) RouteSet(op->mClientId, e);
        else RouteSet(op->mClientFlags, e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteClientJoinedGroup(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_CLIENT_JOINED_GROUP, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: RouteSet(op->mClientId, e); break;
        case 2: RouteSet(op->mMemberFlags, e); break;
        case 3: r.Text(e, op->mClientName); break;
        case 4: RouteSet(op->mClientFlags, e); break;
        case 5: RouteSet(op->mHasClientFlags, e); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteClientJoinedServer(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_CLIENT_JOINED_SERVER, inType, inMsg);
    RouteRecords<Op> a(op, r);
    a.mOpClient = &op->mClientInfo;
    for (size_t i = 0; i < r.mEvents.size(); ++i) a.Apply(r.mEvents[i]);
    return r.End(inMsg);
}
// two shorts: (group or client, client or captain or count)
template <class Op, class A, class B> WONStatus RouteTwoShorts(int inKind, A& outA, B& outB, unsigned char inType,
                                                               ReadBuffer& inMsg)
{
    RouteReply r(inKind, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        if (r.mEvents[i].slot == 0) RouteSet(outA, r.mEvents[i]);
        else RouteSet(outB, r.mEvents[i]);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteClientLeftGroup(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteTwoShorts<Op>(EE_WON_RT_N_CLIENT_LEFT_GROUP, op->mGroupId, op->mClientId, inType, inMsg);
}
template <class Op> WONStatus RouteCaptainChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteTwoShorts<Op>(EE_WON_RT_N_CAPTAIN_CHANGED, op->mGroupId, op->mCaptainId, inType, inMsg);
}
template <class Op> WONStatus RouteMaxPlayersChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteTwoShorts<Op>(EE_WON_RT_N_MAX_PLAYERS, op->mGroupId, op->mMaxPlayers, inType, inMsg);
}
template <class Op> WONStatus RouteMemberCount(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteTwoShorts<Op>(EE_WON_RT_N_MEMBER_COUNT, op->mGroupId, op->mMemberCount, inType, inMsg);
}
template <class Op> WONStatus RouteObserverCount(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteTwoShorts<Op>(EE_WON_RT_N_OBSERVER_COUNT, op->mGroupId, op->mObserverCount, inType, inMsg);
}
template <class Op> WONStatus RouteOneShort(int inKind, unsigned short& outValue, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(inKind, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) RouteSet(outValue, r.mEvents[i]);
    return r.End(inMsg);
}
template <class Op> WONStatus RouteClientLeftServer(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteOneShort<Op>(EE_WON_RT_N_CLIENT_LEFT_SERVER, op->mClientId, inType, inMsg);
}
template <class Op> WONStatus RouteGroupDeleted(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteOneShort<Op>(EE_WON_RT_N_GROUP_DELETED, op->mGroupId, inType, inMsg);
}
// a data object notice: link id, type, name, then (modified) offset and insert, then (created, modified) data
template <class Op, class D> WONStatus RouteDataObjectNotice(int inKind, Op* op, unsigned short* outOffset,
                                                             bool* outInsert, D* outData, unsigned char inType,
                                                             ReadBuffer& inMsg)
{
    RouteReply r(inKind, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.tag == EE_WON_RT_BUF) *outData = r.Buf(e);
        else if (e.slot == 0) RouteSet(op->mLinkId, e);
        else if (e.slot == 1) r.Narrow(e, op->mDataType);
        else if (e.slot == 2) r.Text(e, op->mName);
        else if (e.slot == 3) RouteSet(*outOffset, e);
        else RouteSet(*outInsert, e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteDataObjectCreated(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteDataObjectNotice(EE_WON_RT_N_DATA_CREATED, op, (unsigned short*)NULL, (bool*)NULL, &op->mData,
                                 inType, inMsg);
}
template <class Op> WONStatus RouteDataObjectDeleted(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteDataObjectNotice(EE_WON_RT_N_DATA_DELETED, op, (unsigned short*)NULL, (bool*)NULL,
                                 (ByteBufferPtr*)NULL, inType, inMsg);
}
template <class Op> WONStatus RouteDataObjectModified(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    return RouteDataObjectNotice(EE_WON_RT_N_DATA_MODIFIED, op, &op->mOffset, &op->mIsInsert, &op->mData, inType,
                                 inMsg);
}
template <class Op> WONStatus RouteGroupCreated(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_GROUP_CREATED, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: r.Text(e, op->mName); break;
        case 2: RouteSet(op->mMaxClients, e); break;
        case 3: RouteSet(op->mFlags, e); break;
        case 4: RouteSet(op->mAsyncFlags, e); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteGroupFlagsChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_GROUP_FLAGS, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: RouteSet(op->mGroupFlags, e); break;
        case 2: RouteSet(op->mAsyncFlags, e); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteGroupInvitation(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_INVITATION, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: RouteSet(op->mClientId, e); break;
        case 2: RouteSet(op->mInvited, e); break;
        case 3: r.Text(e, op->mComment); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteGroupJoinAttempt(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_JOIN_ATTEMPT, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mClientId, e); break;
        case 1: RouteSet(op->mGroupId, e); break;
        case 2: r.Text(e, op->mComment); break;
        case 3: RouteSet(op->mFlags, e); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteGroupNameChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_GROUP_NAME, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) RouteSet(op->mGroupId, e);
        else r.Text(e, op->mName);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteMemberFlagsChanged(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_MEMBER_FLAGS, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: RouteSet(op->mClientId, e); break;
        case 2: RouteSet(op->mMemberFlags, e); break;
        }
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RoutePeerChat(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_PEER_CHAT, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.tag == EE_WON_RT_ACT) op->mRecipients.push_back((unsigned short)e.num);
        else if (e.slot == 0) RouteSet(op->mClientId, e);
        else if (e.slot == 1) RouteSet(op->mFlags, e);
        else r.Text(e, op->mText);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RoutePeerData(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_PEER_DATA, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) RouteSet(op->mClientId, e);
        else op->mData = r.Buf(e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteServerAlert(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_SERVER_ALERT, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) r.Text(r.mEvents[i], op->mText);
    return r.End(inMsg);
}
inline WONStatus RouteShutdownAborted(unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_SHUTDOWN_ABORTED, inType, inMsg);
    return r.End(inMsg);
}
template <class Op> WONStatus RouteShutdownStarted(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_N_SHUTDOWN_STARTED, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) r.Text(e, op->mText);
        else RouteSet(op->mSeconds, e);
    }
    return r.End(inMsg);
}
// you were banned / muted: group, on, comment, duration
template <class Op> WONStatus RouteYouWere(int inKind, Op* op, bool& outOn, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(inKind, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        switch (e.slot) {
        case 0: RouteSet(op->mGroupId, e); break;
        case 1: RouteSet(outOn, e); break;
        case 2: r.Text(e, op->mComment); break;
        case 3: RouteSet(op->mDuration, e); break;
        }
    }
    return r.End(inMsg);
}

// ---- the replies with records or state

template <class Op> void RouteApplyGroupList(Op* op, const RouteReply& r)
{
    RouteRecords<Op> a(op, r);
    a.mGroups = &op->mGroupMap;
    for (size_t i = 0; i < r.mEvents.size(); ++i)
        if (!a.Apply(r.mEvents[i])) RouteSet(op->mFlags, r.mEvents[i]);
}
template <class Op> WONStatus RouteGroupList(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_GROUP_LIST, inType, inMsg);
    RouteApplyGroupList(op, r);
    return r.End(inMsg);
}
// ParseGroupListObj: false when the body ran short (what was read stays)
template <class Op> bool RouteParseGroupListObj(Op* op, const void* inData, unsigned long inSize)
{
    RouteReply r(EE_WON_RT_GROUP_LIST_BODY, inData, inSize);
    RouteApplyGroupList(op, r);
    return r.mResult.status == 0;
}
// RoutingGetGroupListReqOp::CheckResponse over its inner op; outInvalidHeader: the op returns InvalidReplyHeader()
template <class Op> WONStatus RouteGroupListReqReply(Op* inInner, const ByteBuffer* inResponse, bool* outInvalidHeader)
{
    RouteReply r(EE_WON_RT_GROUP_LIST_REQ, inResponse->data(), inResponse->length());
    *outInvalidHeader = false;
    RouteRecords<Op> a(inInner, r);
    a.mGroups = &inInner->mGroupMap;
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.tag == EE_WON_RT_ACT && e.slot == EE_WON_RT_ACT_INVALID_HEADER) *outInvalidHeader = true;
        else if (!a.Apply(e)) RouteSet(inInner->mFlags, e);
    }
    return r.End();
}
// RoutingGetGroupListReqOp's group map (a Rust op, ee_won reqlast.rs): the replies it checked, read again in order
// into a connection-less inner op as its CheckResponse read each one (a reply cut short keeps what it read).
// Source: GetNum / GetBytes by EE_WON_REQOP_* number (RustServerOp's)
template <class Op, class Source> void RouteGroupListOf(Op* inInner, const Source& inOp)
{
    long long n = inOp.GetNum(EE_WON_REQOP_GROUP_REPLIES);
    for (long long k = 0; k < n; ++k) {
        std::string aReply = inOp.GetBytes(EE_WON_REQOP_GROUP_REPLY, k);
        ByteBufferPtr aBuffer = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
        bool anInvalidHeader = false;
        try {
            RouteGroupListReqReply(inInner, aBuffer.get(), &anInvalidHeader);
        } catch (ReadBufferException&) {
        }
    }
}
template <class Op> WONStatus RouteReadData(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_READ_DATA, inType, inMsg);
    RouteRecords<Op> a(op, r);
    a.mObjects = &op->mDataObjects;
    for (size_t i = 0; i < r.mEvents.size(); ++i) a.Apply(r.mEvents[i]);
    return r.End(inMsg);
}
template <class Op> WONStatus RouteCreateGroup(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.join_flags = op->mJoinFlags;
    RouteReply r(EE_WON_RT_CREATE_GROUP, inType, inMsg, &s);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.tag == EE_WON_RT_ACT) op->CheckLeaveGroup((short)e.num);
        else if (e.slot == 0) RouteSet(op->mGroupId, e);
        else if (e.slot == 1) r.Text(e, op->mGroupName);
        else RouteSet(op->mParentGroupId, e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteJoinGroup(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.group_id = op->mGroupId;
    s.join_flags = op->mJoinFlags;
    RouteReply r(EE_WON_RT_JOIN_GROUP, inType, inMsg, &s);
    // the members op's map the original built before copying it
    RoutingMemberMap aMembers;
    RouteRecords<Op> a(op, r);
    a.mMembers = &aMembers;
    a.mCommitMembers = &op->mMemberMap;
    for (size_t i = 0; i < r.mEvents.size(); ++i)
        if (!a.Apply(r.mEvents[i])) r.Text(r.mEvents[i], op->mJoinComment);
    return r.End(inMsg);
}
template <class Op> WONStatus RouteRegister(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.register_flags = (uint32_t)op->mRegisterFlags;
    s.reply_count = op->mReplyCount;
    s.replies_needed = op->mNumRepliesNeeded;
    RouteReply r(EE_WON_RT_REGISTER, inType, inMsg, &s);
    RoutingClientMap aClients;
    RouteRecords<Op> a(op, r);
    a.mClients = &aClients;
    a.mCommitClients = &op->mClientMap;
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (a.Apply(e)) continue;
        if (e.tag == EE_WON_RT_ACT) ++op->mReplyCount;
        else if (e.slot == 0) RouteSet(op->mClientId, e);
        else if (e.slot == 1) r.Text(e, op->mClientName);
        else RouteSet(op->mReconnectId, e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteClientList(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_CLIENT_LIST, inType, inMsg);
    RouteRecords<Op> a(op, r);
    a.mClients = &op->mClientMap;
    for (size_t i = 0; i < r.mEvents.size(); ++i)
        if (!a.Apply(r.mEvents[i])) RouteSet(op->mIsEx, r.mEvents[i]);
    return r.End(inMsg);
}
template <class Op> WONStatus RouteMembers(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.group_id = op->mGroupId;
    RouteReply r(EE_WON_RT_MEMBERS, inType, inMsg, &s);
    RouteRecords<Op> a(op, r);
    a.mMembers = &op->mMembers;
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (a.Apply(e)) continue;
        if (e.slot == 0) RouteSet(op->mHasClientNames, e);
        else if (e.slot == 1) RouteSet(op->mHasClientFlags, e);
        else RouteSet(op->mObserverCount, e);
    }
    return r.End(inMsg);
}
template <class Op> WONStatus RouteBadUsers(Op* op, unsigned char inType, ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_BAD_USERS, inType, inMsg);
    RouteRecords<Op> a(op, r);
    a.mBadUsers = &op->mBadUserList;
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (a.Apply(e)) continue;
        if (e.slot == 0) op->mListType = (RoutingBadUserListType)e.num;
        else if (e.slot == 1) RouteSet(op->mGroupId, e);
        else RouteSet(op->mFlags, e);
    }
    return r.End(inMsg);
}
// RoutingDetectFirewallOp (its members are private)
inline WONStatus RouteFirewall(bool& ioWaitingForStatusReply, unsigned short& outListenPort, unsigned char inType,
                               ReadBuffer& inMsg)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.waiting = ioWaitingForStatusReply;
    RouteReply r(EE_WON_RT_FIREWALL, inType, inMsg, &s);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) RouteSet(ioWaitingForStatusReply, e);
        else RouteSet(outListenPort, e);
    }
    return r.End(inMsg);
}
// RoutingDerivedServerOp (its members are protected)
inline WONStatus RouteDerived(unsigned char& outMsgType, ByteBufferPtr& outRecvMsg, unsigned char inType,
                              ReadBuffer& inMsg)
{
    RouteReply r(EE_WON_RT_DERIVED, inType, inMsg);
    for (size_t i = 0; i < r.mEvents.size(); ++i) {
        const EeWonRouteEvent& e = r.mEvents[i];
        if (e.slot == 0) RouteSet(outMsgType, e);
        else outRecvMsg = r.Buf(e);
    }
    return r.End(inMsg);
}
// RoutingGetUserStatusOp::CheckResponse: each queried user's status, in the map's order
template <class Map> WONStatus RouteUserStatusReply(Map& ioStatusMap, ReadBuffer& inMessage)
{
    EeWonRouteState s;
    memset(&s, 0, sizeof(s));
    s.user_count = (uint32_t)ioStatusMap.size();
    RouteReply r(EE_WON_RT_USER_STATUS, 0, inMessage, &s);
    typename Map::iterator j = ioStatusMap.begin();
    for (size_t i = 0; i < r.mEvents.size(); ++i, ++j) j->second = (unsigned char)r.mEvents[i].num;
    inMessage.SetPos((int)r.mResult.pos);
    return (WONStatus)r.mResult.status;
}

} // namespace WONAPI
