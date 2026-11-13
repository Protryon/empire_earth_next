// The routing server's protocol (WONRouting: every routing op's SendRequest and HandleReply, the notifications,
// RoutingGetUserStatusOp's datagram, RoutingGetGroupListReqOp's request and reply, ParseGroupListObj, the
// connection's message header, reply dispatch rule and unsolicited table): the original's statements (copied from
// the C++ before the port, `this` replaced by a stand-in with the ops' members) against the ops' new code
// (tests/won/won_routemsg_glue.h, until net round 18 include/ee/wonapi/WONRoutingMsg.h, over ee_won's routemsg
// module) on a second stand-in. Requests over generated
// members; replies generated for every kind, read whole, cut at every length and mutated; after each, both
// stand-ins' members and records, the return or the throw, the reader's position and the factories' calls must
// agree. The exit code is the number of failed checks.
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
#include "ee/wonapi/WONBuffer.h"
#include "won_routemsg_glue.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 9191;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

// ---- the stand-in: every member the routing ops read or write, with the type the op declares (mFlags varies)

static int sFactory[3];

template <class FlagT> struct Mock {
    Mock() : mSendMsg(2, true) { this->Zero(); }
    void Zero()
    {
        mIsDerivedServerOp = false;
        mGroupId = mClientId = mLinkId = mParentGroupId = mMaxPlayers = mOffset = mListenPort = mCaptainId = 0;
        mMaxClients = mObserverCount = mMemberCount = mNewCaptainId = 0;
        mMemberFlags = mJoinFlags = mFlag = mMsgType = 0;
        mFlags = 0;
        mIsBanned = mIsMuted = mBanByWONId = mMuteByWONId = mWONIdIsCDKeyId = mLeaveIfAlreadyInGroup = false;
        mModeratorOn = mAccepted = mInvited = mIsInsert = mSubscribe = mIsEx = mHasClientFlags = false;
        mHasClientNames = mWaitingForStatusReply = false;
        mBanTime = mMuteTime = mWONId = mGroupFlagMask = mGroupFlags = mAsyncFlagMask = mAsyncFlags = 0;
        mSecondsUntilShutdown = mAsyncMessageFlagMask = mAsyncMessageFlags = mClientFlagMask = mClientFlags = 0;
        mReconnectId = mRegisterFlags = mMaxConnectWaitTime = mSeconds = mDuration = 0;
        mReplyCount = mNumRepliesNeeded = 0;
        mListType = RoutingBadUserListType_BannedUsers;
    }
    // RoutingOp
    WriteBuffer mSendMsg;
    bool mIsDerivedServerOp;
    void InitSendMsg(unsigned char inType)
    {
        this->mSendMsg.Reset();
        this->mSendMsg.AppendByte(3);
        this->mSendMsg.AppendByte(this->mIsDerivedServerOp ? 8 : 7);
        this->mSendMsg.AppendByte(inType);
    }
    RoutingClientInfoPtr GetNewClientInfo() { sFactory[0]++; return new RoutingClientInfo; }
    RoutingMemberInfoPtr GetNewMemberInfo() { sFactory[1]++; return new RoutingMemberInfo; }
    RoutingGroupInfoPtr GetNewGroupInfo() { sFactory[2]++; return new RoutingGroupInfo; }
    // RoutingCreateGroupOp::CheckLeaveGroup's calls
    std::vector<std::pair<int, int> > mLeaves;
    void CheckLeaveGroup(short inStatus) { this->mLeaves.push_back(std::make_pair((int)inStatus, (int)this->mGroupId)); }
    // RoutingGetGroupListOp
    bool IncludeName() const { return (this->mFlags & 1) ? true : false; }
    bool IncludeCaptain() const { return (this->mFlags & 2) ? true : false; }
    bool IncludeMaxPlayers() const { return (this->mFlags & 4) ? true : false; }
    bool IncludeFlags() const { return (this->mFlags & 8) ? true : false; }
    bool IncludeAsyncFlags() const { return (this->mFlags & 0x10) ? true : false; }
    bool IncludeMemberCount() const { return (this->mFlags & 0x20) ? true : false; }
    bool IncludeObserverCount() const { return (this->mFlags & 0x40) ? true : false; }
    bool IncludeMembers() const { return (this->mFlags & 0x80) ? true : false; }
    FlagT GetFlags() const { return this->mFlags; }

    unsigned short mGroupId, mClientId, mLinkId, mParentGroupId, mMaxPlayers, mOffset, mListenPort, mCaptainId;
    unsigned short mMaxClients, mObserverCount, mMemberCount, mNewCaptainId;
    unsigned char mMemberFlags, mJoinFlags, mFlag, mMsgType;
    FlagT mFlags;
    bool mIsBanned, mIsMuted, mBanByWONId, mMuteByWONId, mWONIdIsCDKeyId, mLeaveIfAlreadyInGroup;
    bool mModeratorOn, mAccepted, mInvited, mIsInsert, mSubscribe, mIsEx, mHasClientFlags;
    bool mHasClientNames, mWaitingForStatusReply;
    unsigned long mBanTime, mMuteTime, mWONId, mGroupFlagMask, mGroupFlags, mAsyncFlagMask, mAsyncFlags;
    unsigned long mSecondsUntilShutdown, mAsyncMessageFlagMask, mAsyncMessageFlags, mClientFlagMask, mClientFlags;
    unsigned long mReconnectId, mRegisterFlags, mMaxConnectWaitTime, mSeconds, mDuration;
    int mReplyCount, mNumRepliesNeeded;
    std::wstring mBanComment, mMuteComment, mAlertText, mText, mAcceptComment, mDataName, mInviteComment;
    std::wstring mGroupName, mGroupPassword, mJoinComment, mClientName, mServerPassword, mName, mComment;
    std::string mDataType;
    ByteBufferPtr mData, mRecvMsg;
    std::list<unsigned short> mRecipientList, mRecipients;
    RoutingClientMap mClientMap;
    RoutingMemberMap mMembers, mMemberMap;
    RoutingGroupMap mGroupMap;
    RoutingDataObjectList mDataObjects;
    RoutingBadUserList mBadUserList;
    RoutingBadUserListType mListType;
    SmartPtr<RoutingClientInfo> mClientInfo;
};
typedef Mock<unsigned short> M16;
typedef Mock<unsigned char> M8;
typedef Mock<unsigned long> M32;

// ---- generated values (small domains, so that ids match)

static std::wstring AWString()
{
    static const wchar_t* const kTexts[] = { L"", L"a", L"Room", L"n\x263a", L"\x00e9t\x00e9", L"\xd800" };
    std::wstring s = kTexts[Pick(6)];
    if (Pick(6) == 0) s += std::wstring(Pick(300), L'x');
    return s;
}
static std::string AString()
{
    static const char* const kTexts[] = { "", "t", "_KeyId", "\xff\x01" };
    std::string s = kTexts[Pick(4)];
    if (Pick(8) == 0) s += std::string(Pick(300), 'y');
    return s;
}
static ByteBufferPtr ABuffer()
{
    if (Pick(4) == 0) return NULL;
    std::string s(Pick(Pick(8) ? 8 : 400), '\0');
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)Pick(256);
    return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
}
static unsigned long ALong()
{
    static const unsigned long kL[] = { 0, 1, 4, 0x10, 0x14, 0xffffffffUL, 0x80000000UL };
    return Pick(3) ? kL[Pick(7)] : (unsigned long)(Next() * 0x10001u);
}
static unsigned short AShort() { return (unsigned short)(Pick(3) ? Pick(4) : Next()); }
static std::list<unsigned short> AIds()
{
    std::list<unsigned short> l;
    int n = Pick(8) == 0 ? Pick(300) : Pick(4);
    for (int i = 0; i < n; i++) l.push_back(AShort());
    return l;
}

template <class M> void Fill(M& m)
{
    m.mIsDerivedServerOp = Pick(4) == 0;
    m.mSendMsg.AppendBytes("junk", Pick(5));
    m.mGroupId = AShort(); m.mClientId = AShort(); m.mLinkId = AShort(); m.mParentGroupId = AShort();
    m.mMaxPlayers = AShort(); m.mOffset = AShort(); m.mListenPort = AShort(); m.mCaptainId = AShort();
    m.mMaxClients = AShort(); m.mObserverCount = AShort(); m.mMemberCount = AShort(); m.mNewCaptainId = AShort();
    m.mMemberFlags = (unsigned char)Pick(256); m.mJoinFlags = (unsigned char)(Pick(2) ? Pick(2) : Pick(256));
    m.mFlag = (unsigned char)Pick(256); m.mMsgType = (unsigned char)Pick(256);
    m.mFlags = (decltype(m.mFlags))ALong();
    bool* bools[] = { &m.mIsBanned, &m.mIsMuted, &m.mBanByWONId, &m.mMuteByWONId, &m.mWONIdIsCDKeyId,
                      &m.mLeaveIfAlreadyInGroup, &m.mModeratorOn, &m.mAccepted, &m.mInvited, &m.mIsInsert,
                      &m.mSubscribe, &m.mIsEx, &m.mHasClientFlags, &m.mHasClientNames, &m.mWaitingForStatusReply };
    for (size_t i = 0; i < sizeof(bools) / sizeof(bools[0]); i++) *bools[i] = Pick(2) != 0;
    unsigned long* longs[] = { &m.mBanTime, &m.mMuteTime, &m.mWONId, &m.mGroupFlagMask, &m.mGroupFlags,
                               &m.mAsyncFlagMask, &m.mAsyncFlags, &m.mSecondsUntilShutdown, &m.mAsyncMessageFlagMask,
                               &m.mAsyncMessageFlags, &m.mClientFlagMask, &m.mClientFlags, &m.mReconnectId,
                               &m.mRegisterFlags, &m.mMaxConnectWaitTime, &m.mSeconds, &m.mDuration };
    for (size_t i = 0; i < sizeof(longs) / sizeof(longs[0]); i++) *longs[i] = ALong();
    m.mReplyCount = Pick(3);
    m.mNumRepliesNeeded = 1 + Pick(2);
    std::wstring* texts[] = { &m.mBanComment, &m.mMuteComment, &m.mAlertText, &m.mText, &m.mAcceptComment,
                              &m.mDataName, &m.mInviteComment, &m.mGroupName, &m.mGroupPassword, &m.mJoinComment,
                              &m.mClientName, &m.mServerPassword, &m.mName, &m.mComment };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) *texts[i] = AWString();
    m.mDataType = AString();
    m.mData = ABuffer();
    m.mRecvMsg = ABuffer();
    m.mRecipientList = AIds();
    m.mRecipients = AIds();
    m.mListType = (RoutingBadUserListType)(1 + Pick(2));
    // a record left from before in each container
    if (Pick(2)) { RoutingClientInfoPtr c = new RoutingClientInfo; c->mClientId = AShort(); c->mName = L"old"; m.mClientMap[c->mClientId] = c; }
    if (Pick(2)) { RoutingMemberInfoPtr c = new RoutingMemberInfo; c->mClientId = AShort(); m.mMembers[c->mClientId] = c; }
    if (Pick(2)) { RoutingMemberInfoPtr c = new RoutingMemberInfo; c->mClientId = AShort(); m.mMemberMap[c->mClientId] = c; }
    if (Pick(2)) { RoutingGroupInfoPtr g = new RoutingGroupInfo; g->mId = AShort(); m.mGroupMap[g->mId] = g; }
    if (Pick(2)) { RoutingDataObjectPtr o = new RoutingDataObject; o->mLinkId = 7; m.mDataObjects.push_back(o); }
    if (Pick(2)) { RoutingBadUser u; u.mWONId = 7; m.mBadUserList.push_back(u); }
    if (Pick(2)) { m.mClientInfo = new RoutingClientInfo; m.mClientInfo->mName = L"old"; }
}

// ---- the stand-in's state as text

static void D(std::string& s, unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lx,", v); s += b; }
static void D(std::string& s, const std::wstring& t)
{
    s += '"';
    for (size_t i = 0; i < t.size(); i++) D(s, (unsigned long)t[i]);
    s += '"';
}
static void D(std::string& s, const std::string& t) { s += "'" + t + "'"; }
static void D(std::string& s, const ByteBuffer* b)
{
    if (b == NULL) { s += "null,"; return; }
    s += "[" + std::string(b->data(), b->length()) + "]";
}
static void D(std::string& s, const RoutingClientInfo* c)
{
    if (c == NULL) { s += "noclient,"; return; }
    s += "C{"; D(s, c->mName); D(s, c->mClientId); D(s, c->mFlags); D(s, c->mUserId); D(s, c->mAddress);
    D(s, c->mTrustLevel); s += "}";
}
static void D(std::string& s, const RoutingMemberInfo* m)
{
    s += "M{"; D(s, m->mClientId); D(s, m->mFlags); D(s, m->mClientInfo.get()); s += "}";
}
static void D(std::string& s, const RoutingMemberMap& mm)
{
    s += "{";
    for (RoutingMemberMap::const_iterator i = mm.begin(); i != mm.end(); ++i) { D(s, i->first); D(s, i->second.get()); }
    s += "}";
}

template <class M> std::string Dump(M& m)
{
    std::string s;
    s += std::string(m.mSendMsg.data(), m.mSendMsg.length()) + "|";
    D(s, m.mIsDerivedServerOp);
    unsigned long v[] = { m.mGroupId, m.mClientId, m.mLinkId, m.mParentGroupId, m.mMaxPlayers, m.mOffset,
                          m.mListenPort, m.mCaptainId, m.mMaxClients, m.mObserverCount, m.mMemberCount,
                          m.mNewCaptainId, m.mMemberFlags, m.mJoinFlags, m.mFlag, m.mMsgType,
                          (unsigned long)m.mFlags, m.mIsBanned, m.mIsMuted, m.mBanByWONId, m.mMuteByWONId,
                          m.mWONIdIsCDKeyId, m.mLeaveIfAlreadyInGroup, m.mModeratorOn, m.mAccepted, m.mInvited,
                          m.mIsInsert, m.mSubscribe, m.mIsEx, m.mHasClientFlags, m.mHasClientNames,
                          m.mWaitingForStatusReply, m.mBanTime, m.mMuteTime, m.mWONId, m.mGroupFlagMask,
                          m.mGroupFlags, m.mAsyncFlagMask, m.mAsyncFlags, m.mSecondsUntilShutdown,
                          m.mAsyncMessageFlagMask, m.mAsyncMessageFlags, m.mClientFlagMask, m.mClientFlags,
                          m.mReconnectId, m.mRegisterFlags, m.mMaxConnectWaitTime, m.mSeconds, m.mDuration,
                          (unsigned long)m.mReplyCount, (unsigned long)m.mNumRepliesNeeded,
                          (unsigned long)m.mListType };
    for (size_t i = 0; i < sizeof(v) / sizeof(v[0]); i++) D(s, v[i]);
    const std::wstring* t[] = { &m.mBanComment, &m.mMuteComment, &m.mAlertText, &m.mText, &m.mAcceptComment,
                                &m.mDataName, &m.mInviteComment, &m.mGroupName, &m.mGroupPassword, &m.mJoinComment,
                                &m.mClientName, &m.mServerPassword, &m.mName, &m.mComment };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) D(s, *t[i]);
    D(s, m.mDataType);
    D(s, m.mData.get());
    D(s, m.mRecvMsg.get());
    for (std::list<unsigned short>::iterator i = m.mRecipientList.begin(); i != m.mRecipientList.end(); ++i) D(s, *i);
    s += "|";
    for (std::list<unsigned short>::iterator i = m.mRecipients.begin(); i != m.mRecipients.end(); ++i) D(s, *i);
    s += "|clients";
    for (RoutingClientMap::iterator i = m.mClientMap.begin(); i != m.mClientMap.end(); ++i) { D(s, i->first); D(s, i->second.get()); }
    s += "|members";
    D(s, m.mMembers);
    D(s, m.mMemberMap);
    s += "|groups";
    for (RoutingGroupMap::iterator i = m.mGroupMap.begin(); i != m.mGroupMap.end(); ++i) {
        const RoutingGroupInfo* g = i->second.get();
        D(s, i->first);
        s += "G{"; D(s, g->mName); D(s, g->mId); D(s, g->mParentId); D(s, g->mCaptainId); D(s, g->mMaxClients);
        D(s, g->mFlags); D(s, g->mAsyncFlags); D(s, g->mObserverCount); D(s, g->mMaxPlayers); D(s, g->mMembers);
        s += "}";
    }
    s += "|objects";
    for (RoutingDataObjectList::iterator i = m.mDataObjects.begin(); i != m.mDataObjects.end(); ++i) {
        D(s, (*i)->mLinkId); D(s, (*i)->mDataType); D(s, (*i)->mName); D(s, (*i)->mData.get());
    }
    s += "|bad";
    for (RoutingBadUserList::iterator i = m.mBadUserList.begin(); i != m.mBadUserList.end(); ++i) {
        D(s, i->mName); D(s, i->mWONId); D(s, i->mExpirationDiff); D(s, i->mModeratorWONId);
        D(s, i->mModeratorName); D(s, i->mModeratorComment);
    }
    s += "|client";
    D(s, m.mClientInfo.get());
    s += "|leaves";
    for (size_t i = 0; i < m.mLeaves.size(); i++) { D(s, (unsigned long)m.mLeaves[i].first); D(s, (unsigned long)m.mLeaves[i].second); }
    return s;
}

// ---- the original's requests (each op's SendRequest before the port, without SendMsg and AddOp)

template <class M> void OrigSend(int inKind, M* op)
{
    switch (inKind) {
    case EE_WON_RT_REQ_BAN:
        op->InitSendMsg(3);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendBool(op->mIsBanned);
        op->mSendMsg.AppendLong(op->mBanTime);
        op->mSendMsg.AppendWString(op->mBanComment, 2);
        if (op->mBanByWONId) {
            op->mSendMsg.AppendByte(op->mWONIdIsCDKeyId ? 3 : 2);
            op->mSendMsg.AppendLong(op->mWONId);
        } else {
            op->mSendMsg.AppendByte(1);
            op->mSendMsg.AppendShort(op->mClientId);
        }
        break;
    case EE_WON_RT_REQ_MUTE:
        op->InitSendMsg(46);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendBool(op->mIsMuted);
        op->mSendMsg.AppendLong(op->mMuteTime);
        op->mSendMsg.AppendWString(op->mMuteComment, 2);
        if (op->mMuteByWONId) {
            op->mSendMsg.AppendByte(op->mWONIdIsCDKeyId ? 3 : 2);
            op->mSendMsg.AppendLong(op->mWONId);
        } else {
            op->mSendMsg.AppendByte(1);
            op->mSendMsg.AppendShort(op->mClientId);
        }
        break;
    case EE_WON_RT_REQ_CANCEL_JOIN:
        op->InitSendMsg(5);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendBool(op->mLeaveIfAlreadyInGroup);
        break;
    case EE_WON_RT_REQ_LEAVE:
        op->InitSendMsg(43);
        op->mSendMsg.AppendShort(op->mGroupId);
        break;
    case EE_WON_RT_REQ_SET_GROUP_FLAGS:
        op->InitSendMsg(61);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendLong(op->mGroupFlagMask);
        op->mSendMsg.AppendLong(op->mGroupFlags);
        op->mSendMsg.AppendLong(op->mAsyncFlagMask);
        op->mSendMsg.AppendLong(op->mAsyncFlags);
        break;
    case EE_WON_RT_REQ_SERVER_ALERT: {
        op->InitSendMsg(56);
        op->mSendMsg.AppendWString(op->mAlertText, 2);
        op->mSendMsg.AppendShort((short)op->mRecipientList.size());
        std::list<unsigned short>::const_iterator i;
        i=op->mRecipientList.begin();
        while(true) {
            if(!(i != op->mRecipientList.end())) break;
            op->mSendMsg.AppendShort(*i);
            ++i;
        }
        break;
    }
    case EE_WON_RT_REQ_START_SHUTDOWN:
        op->InitSendMsg(65);
        op->mSendMsg.AppendWString(op->mAlertText, 2);
        op->mSendMsg.AppendLong(op->mSecondsUntilShutdown);
        break;
    case EE_WON_RT_REQ_ABORT_SHUTDOWN:
        op->InitSendMsg(1);
        break;
    case EE_WON_RT_REQ_SET_CLIENT_FLAGS:
        op->InitSendMsg(60);
        op->mSendMsg.AppendLong(op->mAsyncMessageFlagMask);
        op->mSendMsg.AppendLong(op->mAsyncMessageFlags);
        op->mSendMsg.AppendLong(op->mClientFlagMask);
        op->mSendMsg.AppendLong(op->mClientFlags);
        break;
    case EE_WON_RT_REQ_SEND_CHAT:
        op->InitSendMsg(54);
        op->mSendMsg.AppendShort(op->mFlags);
        op->mSendMsg.AppendWString(op->mText, 2);
        op->mSendMsg.AppendShort((short)op->mRecipients.size());
        for (std::list<unsigned short>::iterator i = op->mRecipients.begin(); i != op->mRecipients.end(); ++i)
            op->mSendMsg.AppendShort(*i);
        break;
    case EE_WON_RT_REQ_SEND_DATA:
        op->InitSendMsg(55);
        op->mSendMsg.AppendByte(op->mFlags);
        op->mSendMsg.AppendBuffer(op->mData.get(), 2);
        op->mSendMsg.AppendShort((short)op->mRecipients.size());
        for (std::list<unsigned short>::iterator i = op->mRecipients.begin(); i != op->mRecipients.end(); ++i)
            op->mSendMsg.AppendShort(*i);
        break;
    case EE_WON_RT_REQ_BECOME_MODERATOR:
        op->InitSendMsg(4);
        op->mSendMsg.AppendBool(op->mModeratorOn);
        break;
    case EE_WON_RT_REQ_ACCEPT_CLIENT:
        op->InitSendMsg(2);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendShort(op->mClientId);
        op->mSendMsg.AppendBool(op->mAccepted);
        op->mSendMsg.AppendWString(op->mAcceptComment, 2);
        break;
    case EE_WON_RT_REQ_CREATE_DATA:
        op->InitSendMsg(11);
        op->mSendMsg.AppendByte(op->mFlags);
        op->mSendMsg.AppendShort(op->mLinkId);
        op->mSendMsg.AppendString(op->mDataType, 1);
        op->mSendMsg.AppendWString(op->mDataName, 1);
        op->mSendMsg.AppendBuffer(op->mData.get(), 2);
        break;
    case EE_WON_RT_REQ_DELETE_DATA:
        op->InitSendMsg(17);
        op->mSendMsg.AppendShort(op->mLinkId);
        op->mSendMsg.AppendString(op->mDataType, 1);
        op->mSendMsg.AppendWString(op->mDataName, 1);
        break;
    case EE_WON_RT_REQ_INVITE:
        op->InitSendMsg(40);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendShort(op->mClientId);
        op->mSendMsg.AppendBool(op->mInvited);
        op->mSendMsg.AppendWString(op->mInviteComment, 2);
        break;
    case EE_WON_RT_REQ_MODIFY_DATA:
        op->InitSendMsg(45);
        op->mSendMsg.AppendShort(op->mLinkId);
        op->mSendMsg.AppendString(op->mDataType, 1);
        op->mSendMsg.AppendWString(op->mDataName, 1);
        op->mSendMsg.AppendShort(op->mOffset);
        op->mSendMsg.AppendBool(op->mIsInsert);
        op->mSendMsg.AppendBuffer(op->mData.get(), 2);
        break;
    case EE_WON_RT_REQ_READ_DATA:
        op->InitSendMsg(op->mSubscribe ? 67 : 50);
        op->mSendMsg.AppendShort(op->mLinkId);
        op->mSendMsg.AppendString(op->mDataType, 1);
        op->mSendMsg.AppendByte(op->mFlag);
        break;
    case EE_WON_RT_REQ_UNSUBSCRIBE:
        op->InitSendMsg(68);
        op->mSendMsg.AppendShort(op->mLinkId);
        op->mSendMsg.AppendString(op->mDataType, 1);
        op->mSendMsg.AppendByte(op->mFlags);
        break;
    case EE_WON_RT_REQ_CREATE_GROUP:
        op->InitSendMsg(13);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendWString(op->mGroupName, 2);
        op->mSendMsg.AppendWString(op->mGroupPassword, 2);
        op->mSendMsg.AppendShort(op->mMaxPlayers);
        op->mSendMsg.AppendByte(op->mJoinFlags);
        op->mSendMsg.AppendLong(op->mGroupFlags);
        op->mSendMsg.AppendLong(op->mAsyncFlags);
        op->mSendMsg.AppendShort(op->mParentGroupId);
        break;
    case EE_WON_RT_REQ_JOIN_GROUP:
        op->InitSendMsg(42);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendWString(op->mGroupPassword, 2);
        op->mSendMsg.AppendWString(op->mJoinComment, 2);
        op->mSendMsg.AppendByte(op->mJoinFlags);
        op->mSendMsg.AppendByte(op->mMemberFlags);
        break;
    case EE_WON_RT_REQ_REGISTER:
        op->InitSendMsg(52);
        op->mSendMsg.AppendWString(op->mClientName, 2);
        op->mSendMsg.AppendWString(op->mServerPassword, 2);
        op->mSendMsg.AppendLong(op->mReconnectId);
        op->mSendMsg.AppendLong(op->mAsyncMessageFlags);
        op->mSendMsg.AppendLong(op->mRegisterFlags);
        op->mSendMsg.AppendLong(op->mClientFlags);
        break;
    case EE_WON_RT_REQ_CLIENT_LIST:
        op->InitSendMsg(24);
        if (op->mIsEx) op->mSendMsg.AppendBool(true);
        break;
    case EE_WON_RT_REQ_GROUP_LIST:
        op->InitSendMsg(26);
        op->mSendMsg.AppendShort(op->mFlags);
        op->mSendMsg.AppendShort(op->mParentGroupId);
        break;
    case EE_WON_RT_REQ_MEMBERS:
        op->InitSendMsg(30);
        op->mSendMsg.AppendShort(op->mGroupId);
        break;
    case EE_WON_RT_REQ_BAD_USERS:
        op->InitSendMsg(22);
        op->mSendMsg.AppendByte((unsigned char)op->mListType);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendByte(op->mFlags);
        break;
    case EE_WON_RT_REQ_FIREWALL:
        op->InitSendMsg(18 /* SDK RoutingDetectFirewallRequest */);
        op->mSendMsg.AppendShort(op->mListenPort);
        op->mSendMsg.AppendLong(op->mMaxConnectWaitTime);
        break;
    case EE_WON_RT_REQ_SET_GROUP_NAME:
        op->InitSendMsg(63 /* SDK RoutingSetGroupNameRequest */);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendWString(op->mGroupName,2);
        break;
    case EE_WON_RT_REQ_SET_GROUP_PASSWORD:
        op->InitSendMsg(64 /* SDK RoutingSetGroupPasswordRequest */);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendWString(op->mGroupPassword,2);
        break;
    case EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS:
        op->InitSendMsg(62 /* SDK RoutingSetGroupMaxPlayersRequest */);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendShort(op->mMaxPlayers);
        break;
    case EE_WON_RT_REQ_RELINQUISH_CAPTAIN:
        op->InitSendMsg(53 /* SDK RoutingRelinquishCaptaincyRequest */);
        op->mSendMsg.AppendShort(op->mGroupId);
        op->mSendMsg.AppendShort(op->mNewCaptainId);
        break;
    case EE_WON_RT_REQ_DISCONNECT:
        op->InitSendMsg(20);
        break;
    }
}

// the new code (the ops' SendRequest bodies)
template <class M> void NewSend(int inKind, M* op)
{
    switch (inKind) {
    case EE_WON_RT_REQ_BAN: RouteSendBan(op); break;
    case EE_WON_RT_REQ_MUTE: RouteSendMute(op); break;
    case EE_WON_RT_REQ_CANCEL_JOIN: RouteSendCancelJoin(op); break;
    case EE_WON_RT_REQ_LEAVE: RouteSendGroupId(op, EE_WON_RT_REQ_LEAVE); break;
    case EE_WON_RT_REQ_SET_GROUP_FLAGS: RouteSendSetGroupFlags(op); break;
    case EE_WON_RT_REQ_SERVER_ALERT: RouteSendServerAlert(op); break;
    case EE_WON_RT_REQ_START_SHUTDOWN: RouteSendStartShutdown(op); break;
    case EE_WON_RT_REQ_ABORT_SHUTDOWN: RouteSendBare(op, EE_WON_RT_REQ_ABORT_SHUTDOWN); break;
    case EE_WON_RT_REQ_SET_CLIENT_FLAGS: RouteSendSetClientFlags(op); break;
    case EE_WON_RT_REQ_SEND_CHAT: RouteSendChat(op); break;
    case EE_WON_RT_REQ_SEND_DATA: RouteSendData(op); break;
    case EE_WON_RT_REQ_BECOME_MODERATOR: RouteSendBecomeModerator(op); break;
    case EE_WON_RT_REQ_ACCEPT_CLIENT: RouteSendAcceptClient(op); break;
    case EE_WON_RT_REQ_CREATE_DATA: RouteSendCreateData(op); break;
    case EE_WON_RT_REQ_DELETE_DATA: RouteSendDeleteData(op); break;
    case EE_WON_RT_REQ_INVITE: RouteSendInvite(op); break;
    case EE_WON_RT_REQ_MODIFY_DATA: RouteSendModifyData(op); break;
    case EE_WON_RT_REQ_READ_DATA: RouteSendReadData(op); break;
    case EE_WON_RT_REQ_UNSUBSCRIBE: RouteSendUnsubscribe(op); break;
    case EE_WON_RT_REQ_CREATE_GROUP: RouteSendCreateGroup(op); break;
    case EE_WON_RT_REQ_JOIN_GROUP: RouteSendJoinGroup(op); break;
    case EE_WON_RT_REQ_REGISTER: RouteSendRegister(op); break;
    case EE_WON_RT_REQ_CLIENT_LIST: RouteSendClientList(op); break;
    case EE_WON_RT_REQ_GROUP_LIST: RouteSendGroupList(op); break;
    case EE_WON_RT_REQ_MEMBERS: RouteSendGroupId(op, EE_WON_RT_REQ_MEMBERS); break;
    case EE_WON_RT_REQ_BAD_USERS: RouteSendBadUsers(op); break;
    case EE_WON_RT_REQ_FIREWALL: RouteSendHeader(op, 18); RouteSendFirewall(op, op->mListenPort, op->mMaxConnectWaitTime); break;
    case EE_WON_RT_REQ_SET_GROUP_NAME: RouteSendGroupText(op, EE_WON_RT_REQ_SET_GROUP_NAME, op->mGroupId, op->mGroupName); break;
    case EE_WON_RT_REQ_SET_GROUP_PASSWORD: RouteSendGroupText(op, EE_WON_RT_REQ_SET_GROUP_PASSWORD, op->mGroupId, op->mGroupPassword); break;
    case EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS: RouteSendSetGroupMaxPlayers(op, op->mGroupId, op->mMaxPlayers); break;
    case EE_WON_RT_REQ_RELINQUISH_CAPTAIN: RouteSendRelinquishCaptain(op, op->mGroupId, op->mNewCaptainId); break;
    case EE_WON_RT_REQ_DISCONNECT: RouteSendBare(op, EE_WON_RT_REQ_DISCONNECT); break;
    }
}

template <class M> void TestRequest(int inKind)
{
    unsigned int aSeed = sSeed;
    M a, b;
    Fill(a);
    sSeed = aSeed;
    Fill(b);
    OrigSend(inKind, &a);
    NewSend(inKind, &b);
    std::string aWant(a.mSendMsg.data(), a.mSendMsg.length()), aGot(b.mSendMsg.data(), b.mSendMsg.length());
    CHECK(aGot == aWant);
    if (aGot != aWant && sFailures < 6) printf("request %d: %zu vs %zu bytes\n", inKind, aGot.size(), aWant.size());
    CHECK(Dump(a) == Dump(b));
}

static void TestRequests()
{
    for (int i = 0; i < 40000; i++) {
        int k = 1 + Pick(32);
        // the op's mFlags type: a short (chat, group list), a byte (data, bad users, unsubscribe, create data)
        if (k == EE_WON_RT_REQ_SEND_CHAT || k == EE_WON_RT_REQ_GROUP_LIST) TestRequest<M16>(k);
        else if (k == EE_WON_RT_REQ_SEND_DATA || k == EE_WON_RT_REQ_BAD_USERS || k == EE_WON_RT_REQ_UNSUBSCRIBE ||
                 k == EE_WON_RT_REQ_CREATE_DATA) TestRequest<M8>(k);
        else TestRequest<M16>(k);
    }
    // the header alone (InitSendMsg), the datagram and the group list request
    for (int i = 0; i < 2000; i++) {
        M16 a, b;
        a.mIsDerivedServerOp = b.mIsDerivedServerOp = Pick(2) != 0;
        unsigned char t = (unsigned char)Pick(256);
        a.InitSendMsg(t);
        RouteSendHeader(&b, t);
        CHECK(std::string(a.mSendMsg.data(), a.mSendMsg.length()) == std::string(b.mSendMsg.data(), b.mSendMsg.length()));

        std::map<unsigned long, unsigned char> aMap;
        int n = Pick(5) == 0 ? 250 + Pick(20) : Pick(5);
        for (int j = 0; j < n; j++) aMap.insert(std::make_pair(ALong(), (unsigned char)0));
        WriteBuffer message(0, true);
        message.AppendByte(3);
        message.AppendByte(7);
        message.AppendByte(72);
        message.AppendByte((unsigned char)aMap.size());
        std::map<unsigned long, unsigned char>::const_iterator k = aMap.begin();
        while (true) {
            if (k == aMap.end()) break;
            message.AppendLong(k->first);
            ++k;
        }
        ByteBufferPtr aWant = message.ToByteBuffer(true), aGot = RouteUserStatusRequest(aMap);
        CHECK(std::string(aWant->data(), aWant->length()) == std::string(aGot->data(), aGot->length()));

        static const unsigned char kSizes[] = { 0, 1, 2, 4 };
        unsigned char aSize = kSizes[Pick(4)];
        unsigned short aFlags = (unsigned short)ALong();
        WriteBuffer aRequest(aSize,true);
        aRequest.AppendByte(3);
        aRequest.AppendByte(7);
        aRequest.AppendByte(26);
        aRequest.AppendShort((unsigned char)aFlags);
        aWant = aRequest.ToByteBuffer(true);
        aGot = RouteGroupListRequest(aSize, aFlags);
        CHECK(std::string(aWant->data(), aWant->length()) == std::string(aGot->data(), aGot->length()));
    }
}

// ---- the original's replies (each HandleReply and parser before the port)

template <class M> WONStatus OrigStatus(M*, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
template <class M> void OrigGroupListBody(M* op, ReadBuffer& inMsg)
{
    op->mGroupMap.clear();
    op->mFlags = inMsg.ReadShort();
    int count = (unsigned short)inMsg.ReadShort();
    for (int i = 0; i < count; ++i) {
        unsigned short length = inMsg.ReadShort();
        long start = inMsg.pos();
        RoutingGroupInfoPtr group = op->GetNewGroupInfo();
        group->mId = inMsg.ReadShort();
        if (op->IncludeName()) inMsg.ReadWString(group->mName, 2);
        if (op->IncludeCaptain()) group->mCaptainId = inMsg.ReadShort();
        if (op->IncludeMaxPlayers()) group->mMaxClients = inMsg.ReadShort();
        if (op->IncludeFlags()) group->mFlags = inMsg.ReadLong();
        if (op->IncludeAsyncFlags()) group->mAsyncFlags = inMsg.ReadLong();
        if (op->IncludeObserverCount()) group->mObserverCount = inMsg.ReadShort();
        if (op->IncludeMemberCount()) group->mMaxPlayers = inMsg.ReadShort(); // holds the member count
        if (op->IncludeMembers()) {
            for (int j = 0; j < group->mMaxPlayers; ++j) {
                RoutingMemberInfoPtr member = op->GetNewMemberInfo();
                member->mClientId = inMsg.ReadShort();
                member->mFlags = inMsg.ReadByte(); // read as a byte
                group->mMembers[member->mClientId] = member;
            }
        }
        op->mGroupMap[group->mId] = group;
        inMsg.ReadBytes(length - inMsg.pos() + start);
    }
}
template <class M> WONStatus OrigGroupList(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 25) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) OrigGroupListBody(op, inMsg);
    return status;
}
template <class M> bool OrigParseGroupListObj(M* op, const void* data,unsigned long size)
{
    try {
        ReadBuffer msg(data,size);
        OrigGroupListBody(op, msg);
        return true;
    } catch(ReadBufferException&) {}
    return false;
}
template <class M> WONStatus OrigReadData(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 49) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) {
        int count = (unsigned short)inMsg.ReadShort();
        for (int i = 0; i < count; ++i) {
            RoutingDataObjectPtr data = new RoutingDataObject;
            data->mLinkId = inMsg.ReadShort();
            inMsg.ReadString(data->mDataType, 1);
            inMsg.ReadWString(data->mName, 1);
            data->mData = inMsg.ReadBuf(2);
            op->mDataObjects.push_back(data);
        }
    }
    return status;
}
template <class M> WONStatus OrigCreateGroup(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType == 41) {
        if (op->mJoinFlags == 0)
            return (WONStatus)WS_RoutingOp_DontWantReply;
        short status = inMsg.ReadShort();
        op->CheckLeaveGroup(status);
        return status;
    }
    if (inMsgType != 12)
        return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) {
        op->mGroupId = inMsg.ReadShort();
        inMsg.ReadWString(op->mGroupName, 2);
        op->CheckLeaveGroup((short)status);
        if (op->mJoinFlags != 0)
            return (WONStatus)WS_RoutingOp_NeedMoreReplies;
    }
    if ((unsigned long)inMsg.pos() < inMsg.length())
        op->mParentGroupId = inMsg.ReadShort();
    return status;
}
static void OrigParseExClientData(ReadBuffer& inMsg, RoutingClientInfo* inClient)
{
    unsigned char count = inMsg.ReadByte();
    for (int i = 0; i < count; ++i) {
        unsigned char kind = inMsg.ReadByte();
        unsigned char length = inMsg.ReadByte();
        switch (kind) {
        case 1: inClient->mUserId = inMsg.ReadLong(); break;
        case 2: inClient->mAddress = inMsg.ReadLong(); break;
        default: inMsg.ReadBytes(length); break;
        }
    }
}
template <class M> WONStatus OrigClientList(M* op, unsigned char inMsgType,ReadBuffer& inMsg)
{
    if (inMsgType!=23 && inMsgType!=74) return (WONStatus)WS_RoutingOp_DontWantReply;
    op->mIsEx=inMsgType==74;
    WONStatus status=inMsg.ReadShort();
    if (status==0) {
        int count=(unsigned short)inMsg.ReadShort();
        for (int i=0;i<count;++i) {
            RoutingClientInfoPtr client=op->GetNewClientInfo();
            client->mClientId=inMsg.ReadShort();
            inMsg.ReadWString(client->mName,2);
            client->mFlags=inMsg.ReadLong();
            if (op->mIsEx) OrigParseExClientData(inMsg,client.get());
            op->mClientMap[client->mClientId]=client;
        }
    }
    return status;
}
template <class M> WONStatus OrigMembers(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 29) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    unsigned short groupId = inMsg.ReadShort();
    if (groupId != op->mGroupId) return (WONStatus)WS_RoutingOp_DontWantReply;
    if (status == 0) {
        unsigned char flags = inMsg.ReadByte();
        op->mHasClientNames = (flags & 1) != 0;
        op->mHasClientFlags = (flags & 2) != 0;
        op->mObserverCount = inMsg.ReadShort();
        int count = (unsigned short)inMsg.ReadShort();
        for (int i = 0; i < count; ++i) {
            unsigned short length = inMsg.ReadShort();
            long start = inMsg.pos();
            RoutingMemberInfoPtr member = op->GetNewMemberInfo();
            member->mClientId = inMsg.ReadShort();
            member->mFlags = inMsg.ReadByte(); // read as a byte
            op->mMembers[member->mClientId] = member;
            if (flags != 0) member->mClientInfo = op->GetNewClientInfo();
            if (op->mHasClientNames) inMsg.ReadWString(member->mClientInfo->mName, 2);
            if (op->mHasClientFlags) member->mClientInfo->mFlags = inMsg.ReadLong();
            inMsg.ReadBytes(length - inMsg.pos() + start);
        }
    }
    return status;
}
template <class M> WONStatus OrigJoinGroup(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType == 41) {
        WONStatus status = inMsg.ReadShort();
        unsigned short groupId = inMsg.ReadShort();
        if (groupId == op->mGroupId) {
            inMsg.ReadWString(op->mJoinComment, 2);
            if (status == (WONStatus)-2541) return (WONStatus)WS_RoutingOp_NeedMoreReplies;
            if (status != 0) return status;
            return (op->mJoinFlags & 1) ? (WONStatus)100105 : (WONStatus)0;
        }
    } else if (inMsgType == 29) {
        // new RoutingGetMembersOfGroupOp(connection): its members empty, the factories the connection's
        M16 inner;
        inner.mGroupId = op->mGroupId;
        WONStatus status = OrigMembers(&inner, 29, inMsg);
        if (status == WS_Success) op->mMemberMap = inner.mMembers;
        return status;
    }
    return (WONStatus)WS_RoutingOp_DontWantReply;
}
template <class M> WONStatus OrigRegister(M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    WONStatus status;
    if (inMsgType != 23 && inMsgType != 74) {
        if (inMsgType == 51) {
            status = inMsg.ReadShort();
            if (status == 0) {
                op->mClientId = inMsg.ReadShort();
                inMsg.ReadWString(op->mClientName, 2);
                op->mReconnectId = inMsg.ReadLong();
            }
            ++op->mReplyCount;
        } else {
            return (WONStatus)WS_RoutingOp_DontWantReply;
        }
    } else {
        if (!(op->mRegisterFlags & 0x14))
            return (WONStatus)WS_RoutingOp_DontWantReply;
        M16 inner; // new RoutingGetClientListOp(connection)
        status = OrigClientList(&inner, inMsgType, inMsg);
        op->mClientMap = inner.mClientMap;
        ++op->mReplyCount;
    }
    if (status == 0 && op->mReplyCount != op->mNumRepliesNeeded) return (WONStatus)WS_RoutingOp_NeedMoreReplies;
    return status;
}
template <class M> WONStatus OrigBadUsers(M* op, unsigned char inMsgType,ReadBuffer& inMsg)
{
    if(inMsgType!=21) return (WONStatus)WS_RoutingOp_DontWantReply;
    op->mBadUserList.clear();
    WONStatus status=inMsg.ReadShort();
    if(status==0) {
        op->mListType=(RoutingBadUserListType)(signed char)inMsg.ReadByte();
        op->mGroupId=inMsg.ReadShort();
        op->mFlags=inMsg.ReadByte();
        int count=(unsigned short)inMsg.ReadShort();
        for(;count>0;--count) {
            unsigned short length=inMsg.ReadShort();
            long start=inMsg.pos();
            RoutingBadUser user;
            user.mWONId=inMsg.ReadLong();
            inMsg.ReadWString(user.mName,2);
            if(op->GetFlags()&1) user.mExpirationDiff=inMsg.ReadLong();
            if(op->GetFlags()&2) user.mModeratorWONId=inMsg.ReadLong();
            if(op->GetFlags()&4) inMsg.ReadWString(user.mModeratorName,2);
            if(op->GetFlags()&8) inMsg.ReadWString(user.mModeratorComment,2);
            op->mBadUserList.push_back(user);
            inMsg.ReadBytes(length-inMsg.pos()+start);
        }
    }
    return status;
}
template <class M> WONStatus OrigFirewall(M* op, unsigned char theMsgType, ReadBuffer &theMsg)
{
	WONStatus aStatus;
	if(op->mWaitingForStatusReply && theMsgType==66 /* SDK RoutingStatusReply */)
	{
		op->mWaitingForStatusReply = false;
		aStatus = (WONStatus)theMsg.ReadShort();
		if(aStatus!=0)
			return aStatus;
		else
			return WS_RoutingOp_NeedMoreReplies /* SDK WS_RoutingOp_NeedMoreReplies */;
	}
	else if(theMsgType==19 /* SDK RoutingDetectFirewallResult */)
	{
		aStatus = (WONStatus)theMsg.ReadShort();
		op->mListenPort = theMsg.ReadShort();
		return aStatus;
	}
	else
		return WS_RoutingOp_DontWantReply /* SDK WS_RoutingOp_DontWantReply */;
}
template <class M> WONStatus OrigDerived(M* op, unsigned char messageType,ReadBuffer& message)
{
    op->mMsgType=messageType;
    op->mRecvMsg=new ByteBuffer(message.data()+message.pos(),message.Available(),false);
    return WS_Success;
}

// the notifications (RoutingNotifications.cpp)
template <class M> WONStatus OrigNotice(int inKind, M* op, unsigned char inMsgType, ReadBuffer& inMsg)
{
    switch (inKind) {
    case EE_WON_RT_N_CLIENT_FLAGS:
        if (inMsgType != 6) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientId = inMsg.ReadShort();
        op->mClientFlags = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_CLIENT_JOINED_GROUP: {
        if (inMsgType != 7) return (WONStatus)WS_RoutingOp_DontWantReply;
        unsigned char flags = inMsg.ReadByte();
        op->mGroupId = inMsg.ReadShort();
        op->mClientId = inMsg.ReadShort();
        op->mMemberFlags = inMsg.ReadByte();
        if (flags & 1) inMsg.ReadWString(op->mClientName, 2);
        if (flags & 2) {
            op->mClientFlags = inMsg.ReadLong();
            op->mHasClientFlags = true;
        }
        return (WONStatus)WS_Success;
    }
    case EE_WON_RT_N_CLIENT_JOINED_SERVER:
        if (inMsgType != 8) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientInfo = op->GetNewClientInfo();
        op->mClientInfo->mClientId = inMsg.ReadShort();
        inMsg.ReadWString(op->mClientInfo->mName, 2);
        op->mClientInfo->mFlags = inMsg.ReadLong();
        if (inMsg.Available() > 0)
            OrigParseExClientData(inMsg, op->mClientInfo.get());
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_CLIENT_LEFT_GROUP:
        if (inMsgType != 9) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mClientId = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_CLIENT_LEFT_SERVER:
        if (inMsgType != 10) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientId = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_DATA_CREATED:
        if (inMsgType != 14) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mLinkId = inMsg.ReadShort();
        inMsg.ReadString(op->mDataType, 1);
        inMsg.ReadWString(op->mName, 1);
        op->mData = inMsg.ReadBuf(2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_DATA_DELETED:
        if (inMsgType != 15) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mLinkId = inMsg.ReadShort();
        inMsg.ReadString(op->mDataType, 1);
        inMsg.ReadWString(op->mName, 1);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_DATA_MODIFIED:
        if (inMsgType != 16) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mLinkId = inMsg.ReadShort();
        inMsg.ReadString(op->mDataType, 1);
        inMsg.ReadWString(op->mName, 1);
        op->mOffset = inMsg.ReadShort();
        op->mIsInsert = inMsg.ReadBool();
        op->mData = inMsg.ReadBuf(2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_CAPTAIN_CHANGED:
        if (inMsgType != 31) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mCaptainId = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_GROUP_CREATED:
        if (inMsgType != 32) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        inMsg.ReadWString(op->mName, 2);
        op->mMaxClients = inMsg.ReadShort();
        op->mFlags = inMsg.ReadLong();
        op->mAsyncFlags = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_GROUP_DELETED:
        if (inMsgType != 33) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_GROUP_FLAGS:
        if (inMsgType != 34) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mGroupFlags = inMsg.ReadLong();
        op->mAsyncFlags = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_INVITATION:
        if (inMsgType != 35) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mClientId = inMsg.ReadShort();
        op->mInvited = inMsg.ReadBool();
        inMsg.ReadWString(op->mComment, 2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_JOIN_ATTEMPT:
        if (inMsgType != 36) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientId = inMsg.ReadShort();
        op->mGroupId = inMsg.ReadShort();
        inMsg.ReadWString(op->mComment, 2);
        op->mFlags = inMsg.ReadByte();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_MAX_PLAYERS:
        if (inMsgType != 37) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mMaxPlayers = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_GROUP_NAME:
        if (inMsgType != 38) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        inMsg.ReadWString(op->mName, 2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_OBSERVER_COUNT:
        if (inMsgType != 39) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mObserverCount = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_MEMBER_FLAGS:
        if (inMsgType != 44) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mClientId = inMsg.ReadShort();
        op->mMemberFlags = inMsg.ReadByte();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_PEER_CHAT: {
        if (inMsgType != 47) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientId = inMsg.ReadShort();
        op->mFlags = inMsg.ReadShort();
        inMsg.ReadWString(op->mText, 2);
        int count = (unsigned short)inMsg.ReadShort();
        for (int i = 0; i < count; ++i) {
            unsigned short recipient = inMsg.ReadShort();
            op->mRecipients.push_back(recipient);
        }
        return (WONStatus)WS_Success;
    }
    case EE_WON_RT_N_PEER_DATA:
        if (inMsgType != 48) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mClientId = inMsg.ReadShort();
        op->mData = inMsg.ReadBuf(2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_SERVER_ALERT:
        if (inMsgType != 57) return (WONStatus)WS_RoutingOp_DontWantReply;
        inMsg.ReadWString(op->mText, 2);
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_SHUTDOWN_ABORTED:
        if (inMsgType != 58) return (WONStatus)WS_RoutingOp_DontWantReply;
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_SHUTDOWN_STARTED:
        if (inMsgType != 59) return (WONStatus)WS_RoutingOp_DontWantReply;
        inMsg.ReadWString(op->mText, 2);
        op->mSeconds = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_BANNED:
        if (inMsgType != 69) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mIsBanned = inMsg.ReadBool();
        inMsg.ReadWString(op->mComment, 2);
        op->mDuration = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_MUTED:
        if (inMsgType != 70) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mIsMuted = inMsg.ReadBool();
        inMsg.ReadWString(op->mComment, 2);
        op->mDuration = inMsg.ReadLong();
        return (WONStatus)WS_Success;
    case EE_WON_RT_N_MEMBER_COUNT:
        if (inMsgType != 71) return (WONStatus)WS_RoutingOp_DontWantReply;
        op->mGroupId = inMsg.ReadShort();
        op->mMemberCount = inMsg.ReadShort();
        return (WONStatus)WS_Success;
    }
    return (WONStatus)-1;
}

template <class M> WONStatus OrigReply(int inKind, M* op, unsigned char t, ReadBuffer& m)
{
    switch (inKind) {
    case EE_WON_RT_STATUS: return OrigStatus(op, t, m);
    case EE_WON_RT_GROUP_LIST: return OrigGroupList(op, t, m);
    case EE_WON_RT_READ_DATA: return OrigReadData(op, t, m);
    case EE_WON_RT_CREATE_GROUP: return OrigCreateGroup(op, t, m);
    case EE_WON_RT_JOIN_GROUP: return OrigJoinGroup(op, t, m);
    case EE_WON_RT_REGISTER: return OrigRegister(op, t, m);
    case EE_WON_RT_CLIENT_LIST: return OrigClientList(op, t, m);
    case EE_WON_RT_MEMBERS: return OrigMembers(op, t, m);
    case EE_WON_RT_BAD_USERS: return OrigBadUsers(op, t, m);
    case EE_WON_RT_FIREWALL: return OrigFirewall(op, t, m);
    case EE_WON_RT_DERIVED: return OrigDerived(op, t, m);
    }
    return OrigNotice(inKind, op, t, m);
}

// the new code (the ops' HandleReply bodies)
template <class M> WONStatus NewReply(int inKind, M* op, unsigned char t, ReadBuffer& m)
{
    switch (inKind) {
    case EE_WON_RT_STATUS: return RouteStatusReply(t, m);
    case EE_WON_RT_GROUP_LIST: return RouteGroupList(op, t, m);
    case EE_WON_RT_READ_DATA: return RouteReadData(op, t, m);
    case EE_WON_RT_CREATE_GROUP: return RouteCreateGroup(op, t, m);
    case EE_WON_RT_JOIN_GROUP: return RouteJoinGroup(op, t, m);
    case EE_WON_RT_REGISTER: return RouteRegister(op, t, m);
    case EE_WON_RT_CLIENT_LIST: return RouteClientList(op, t, m);
    case EE_WON_RT_MEMBERS: return RouteMembers(op, t, m);
    case EE_WON_RT_BAD_USERS: return RouteBadUsers(op, t, m);
    case EE_WON_RT_FIREWALL: return RouteFirewall(op->mWaitingForStatusReply, op->mListenPort, t, m);
    case EE_WON_RT_DERIVED: return RouteDerived(op->mMsgType, op->mRecvMsg, t, m);
    case EE_WON_RT_N_CLIENT_FLAGS: return RouteClientFlagsChanged(op, t, m);
    case EE_WON_RT_N_CLIENT_JOINED_GROUP: return RouteClientJoinedGroup(op, t, m);
    case EE_WON_RT_N_CLIENT_JOINED_SERVER: return RouteClientJoinedServer(op, t, m);
    case EE_WON_RT_N_CLIENT_LEFT_GROUP: return RouteClientLeftGroup(op, t, m);
    case EE_WON_RT_N_CLIENT_LEFT_SERVER: return RouteClientLeftServer(op, t, m);
    case EE_WON_RT_N_DATA_CREATED: return RouteDataObjectCreated(op, t, m);
    case EE_WON_RT_N_DATA_DELETED: return RouteDataObjectDeleted(op, t, m);
    case EE_WON_RT_N_DATA_MODIFIED: return RouteDataObjectModified(op, t, m);
    case EE_WON_RT_N_CAPTAIN_CHANGED: return RouteCaptainChanged(op, t, m);
    case EE_WON_RT_N_GROUP_CREATED: return RouteGroupCreated(op, t, m);
    case EE_WON_RT_N_GROUP_DELETED: return RouteGroupDeleted(op, t, m);
    case EE_WON_RT_N_GROUP_FLAGS: return RouteGroupFlagsChanged(op, t, m);
    case EE_WON_RT_N_INVITATION: return RouteGroupInvitation(op, t, m);
    case EE_WON_RT_N_JOIN_ATTEMPT: return RouteGroupJoinAttempt(op, t, m);
    case EE_WON_RT_N_MAX_PLAYERS: return RouteMaxPlayersChanged(op, t, m);
    case EE_WON_RT_N_MEMBER_COUNT: return RouteMemberCount(op, t, m);
    case EE_WON_RT_N_GROUP_NAME: return RouteGroupNameChanged(op, t, m);
    case EE_WON_RT_N_OBSERVER_COUNT: return RouteObserverCount(op, t, m);
    case EE_WON_RT_N_MEMBER_FLAGS: return RouteMemberFlagsChanged(op, t, m);
    case EE_WON_RT_N_PEER_CHAT: return RoutePeerChat(op, t, m);
    case EE_WON_RT_N_PEER_DATA: return RoutePeerData(op, t, m);
    case EE_WON_RT_N_SERVER_ALERT: return RouteServerAlert(op, t, m);
    case EE_WON_RT_N_SHUTDOWN_ABORTED: return RouteShutdownAborted(t, m);
    case EE_WON_RT_N_SHUTDOWN_STARTED: return RouteShutdownStarted(op, t, m);
    case EE_WON_RT_N_BANNED: return RouteYouWere(EE_WON_RT_N_BANNED, op, op->mIsBanned, t, m);
    case EE_WON_RT_N_MUTED: return RouteYouWere(EE_WON_RT_N_MUTED, op, op->mIsMuted, t, m);
    }
    return (WONStatus)-1;
}

// ---- generated replies

struct G {
    std::string s;
    void b(int v) { s += (char)(unsigned char)v; }
    void h(int v) { b(v & 0xff); b((v >> 8) & 0xff); }
    void l(unsigned long v) { h((int)(v & 0xffff)); h((int)((v >> 16) & 0xffff)); }
    void len(unsigned long n, int size) { if (size == 1) b((int)n); else if (size == 2) h((int)n); else l(n); }
    void ws(int size)
    {
        std::wstring t = AWString();
        std::string u;
        WideAppendUtf16LE(u, t);
        if (size == 1 && u.size() / 2 > 127) u.resize(254);
        len(u.size() / 2, size);
        s += u;
    }
    void str(int size)
    {
        std::string t = AString();
        if (size == 1 && t.size() > 127) t.resize(127);
        len(t.size(), size);
        s += t;
    }
    void buf(int size)
    {
        int n = Pick(4) ? Pick(8) : Pick(300);
        len(n, size);
        for (int i = 0; i < n; i++) b(Pick(256));
    }
    void status() { h(Pick(3) ? 0 : (Pick(2) ? -2541 : Pick(0x10000))); }
    void count(int typical) { h(Pick(10) == 0 ? Pick(0x10000) : Pick(typical + 1)); }
    // a sized entry: its length short, the body, a little slack (or a wrong length)
    void entry(const std::string& body)
    {
        int extra = Pick(4) ? Pick(3) : -Pick(3);
        if ((int)body.size() + extra < 0) extra = 0;
        h((int)body.size() + extra);
        s += body;
        for (int i = 0; i < extra; i++) b(Pick(256));
    }
};

static void ClientEx(G& g)
{
    int n = Pick(4);
    g.b(n);
    for (int i = 0; i < n; i++) {
        int k = Pick(4);
        int len = Pick(6);
        g.b(k);
        g.b(len);
        if (k == 1 || k == 2) g.l(ALong());
        else for (int j = 0; j < len; j++) g.b(Pick(256));
    }
}
static void ClientListBody(G& g, bool ex)
{
    g.status();
    if (Pick(4) == 0) return;
    int n = Pick(5);
    g.h(n);
    for (int i = 0; i < n; i++) {
        g.h(AShort());
        g.ws(2);
        g.l(ALong());
        if (ex) ClientEx(g);
    }
}
static void GroupListBody(G& g)
{
    int flags = Pick(3) ? Pick(0x100) : (int)Next();
    g.h(flags);
    int n = Pick(4);
    g.h(n);
    for (int i = 0; i < n; i++) {
        G e;
        e.h(AShort());
        if (flags & 1) e.ws(2);
        if (flags & 2) e.h(AShort());
        if (flags & 4) e.h(AShort());
        if (flags & 8) e.l(ALong());
        if (flags & 0x10) e.l(ALong());
        if (flags & 0x40) e.h(AShort());
        int members = Pick(4);
        if (flags & 0x20) e.h(members);
        else members = 0;
        if (flags & 0x80)
            for (int j = 0; j < members; j++) { e.h(AShort()); e.b(Pick(256)); }
        g.entry(e.s);
    }
}
static void MembersBody(G& g)
{
    g.status();
    g.h(AShort());
    int flags = Pick(4) ? Pick(4) : Pick(256);
    g.b(flags);
    g.h(AShort());
    int n = Pick(4);
    g.h(n);
    for (int i = 0; i < n; i++) {
        G e;
        e.h(AShort());
        e.b(Pick(256));
        if (flags & 1) e.ws(2);
        if (flags & 2) e.l(ALong());
        g.entry(e.s);
    }
}

// a reply of a kind: its message type (mostly the expected one) and body
static unsigned char Generate(int inKind, G& g)
{
    static const unsigned char kOther[] = { 66, 41, 12, 29, 23, 74, 51, 19, 25, 49, 21, 0, 200 };
    unsigned char t = 0;
    switch (inKind) {
    case EE_WON_RT_STATUS: t = 66; g.status(); break;
    case EE_WON_RT_GROUP_LIST: t = 25; g.status(); GroupListBody(g); break;
    case EE_WON_RT_READ_DATA: {
        t = 49;
        g.status();
        int n = Pick(4);
        g.h(n);
        for (int i = 0; i < n; i++) { g.h(AShort()); g.str(1); g.ws(1); g.buf(2); }
        break;
    }
    case EE_WON_RT_CREATE_GROUP:
        t = Pick(2) ? 12 : 41;
        g.status();
        if (t == 12) { g.h(AShort()); g.ws(2); if (Pick(2)) g.h(AShort()); }
        break;
    case EE_WON_RT_JOIN_GROUP:
        t = Pick(2) ? 41 : 29;
        if (t == 41) { g.status(); g.h(AShort()); g.ws(2); }
        else MembersBody(g);
        break;
    case EE_WON_RT_REGISTER:
        t = Pick(2) ? 51 : (Pick(2) ? 23 : 74);
        if (t == 51) { g.status(); g.h(AShort()); g.ws(2); g.l(ALong()); }
        else ClientListBody(g, t == 74);
        break;
    case EE_WON_RT_CLIENT_LIST: t = Pick(2) ? 23 : 74; ClientListBody(g, t == 74); break;
    case EE_WON_RT_MEMBERS: t = 29; MembersBody(g); break;
    case EE_WON_RT_BAD_USERS: {
        t = 21;
        g.status();
        g.b(Pick(4) ? 1 + Pick(2) : Pick(256));
        g.h(AShort());
        int flags = Pick(3) ? Pick(16) : Pick(256);
        g.b(flags);
        int n = Pick(4);
        g.h(n);
        for (int i = 0; i < n; i++) {
            G e;
            e.l(ALong());
            e.ws(2);
            if (flags & 1) e.l(ALong());
            if (flags & 2) e.l(ALong());
            if (flags & 4) e.ws(2);
            if (flags & 8) e.ws(2);
            g.entry(e.s);
        }
        break;
    }
    case EE_WON_RT_FIREWALL: t = Pick(2) ? 66 : 19; g.status(); if (t == 19) g.h(AShort()); break;
    case EE_WON_RT_DERIVED: { t = (unsigned char)Pick(256); int n = Pick(20); for (int i = 0; i < n; i++) g.b(Pick(256)); break; }
    case EE_WON_RT_N_CLIENT_FLAGS: t = 6; g.h(AShort()); g.l(ALong()); break;
    case EE_WON_RT_N_CLIENT_JOINED_GROUP: t = 7; g.b(Pick(4)); g.h(AShort()); g.h(AShort()); g.b(Pick(256)); g.ws(2); g.l(ALong()); break;
    case EE_WON_RT_N_CLIENT_JOINED_SERVER: t = 8; g.h(AShort()); g.ws(2); g.l(ALong()); if (Pick(2)) ClientEx(g); break;
    case EE_WON_RT_N_CLIENT_LEFT_GROUP: t = 9; g.h(AShort()); g.h(AShort()); break;
    case EE_WON_RT_N_CLIENT_LEFT_SERVER: t = 10; g.h(AShort()); break;
    case EE_WON_RT_N_DATA_CREATED: t = 14; g.h(AShort()); g.str(1); g.ws(1); g.buf(2); break;
    case EE_WON_RT_N_DATA_DELETED: t = 15; g.h(AShort()); g.str(1); g.ws(1); break;
    case EE_WON_RT_N_DATA_MODIFIED: t = 16; g.h(AShort()); g.str(1); g.ws(1); g.h(AShort()); g.b(Pick(3)); g.buf(2); break;
    case EE_WON_RT_N_CAPTAIN_CHANGED: t = 31; g.h(AShort()); g.h(AShort()); break;
    case EE_WON_RT_N_GROUP_CREATED: t = 32; g.h(AShort()); g.ws(2); g.h(AShort()); g.l(ALong()); g.l(ALong()); break;
    case EE_WON_RT_N_GROUP_DELETED: t = 33; g.h(AShort()); break;
    case EE_WON_RT_N_GROUP_FLAGS: t = 34; g.h(AShort()); g.l(ALong()); g.l(ALong()); break;
    case EE_WON_RT_N_INVITATION: t = 35; g.h(AShort()); g.h(AShort()); g.b(Pick(3)); g.ws(2); break;
    case EE_WON_RT_N_JOIN_ATTEMPT: t = 36; g.h(AShort()); g.h(AShort()); g.ws(2); g.b(Pick(256)); break;
    case EE_WON_RT_N_MAX_PLAYERS: t = 37; g.h(AShort()); g.h(AShort()); break;
    case EE_WON_RT_N_MEMBER_COUNT: t = 71; g.h(AShort()); g.h(AShort()); break;
    case EE_WON_RT_N_GROUP_NAME: t = 38; g.h(AShort()); g.ws(2); break;
    case EE_WON_RT_N_OBSERVER_COUNT: t = 39; g.h(AShort()); g.h(AShort()); break;
    case EE_WON_RT_N_MEMBER_FLAGS: t = 44; g.h(AShort()); g.h(AShort()); g.b(Pick(256)); break;
    case EE_WON_RT_N_PEER_CHAT: {
        t = 47; g.h(AShort()); g.h(AShort()); g.ws(2);
        int n = Pick(5); g.h(n); for (int i = 0; i < n; i++) g.h(AShort());
        break;
    }
    case EE_WON_RT_N_PEER_DATA: t = 48; g.h(AShort()); g.buf(2); break;
    case EE_WON_RT_N_SERVER_ALERT: t = 57; g.ws(2); break;
    case EE_WON_RT_N_SHUTDOWN_ABORTED: t = 58; break;
    case EE_WON_RT_N_SHUTDOWN_STARTED: t = 59; g.ws(2); g.l(ALong()); break;
    case EE_WON_RT_N_BANNED: t = 69; g.h(AShort()); g.b(Pick(3)); g.ws(2); g.l(ALong()); break;
    case EE_WON_RT_N_MUTED: t = 70; g.h(AShort()); g.b(Pick(3)); g.ws(2); g.l(ALong()); break;
    }
    if (Pick(8) == 0) t = kOther[Pick(13)];
    if (Pick(4) == 0) { int n = Pick(4); for (int i = 0; i < n; i++) g.b(Pick(256)); }
    return t;
}

struct Outcome {
    bool threw;
    int status;
    int pos;
    int factory[3];
    std::string dump;
    bool operator==(const Outcome& o) const
    {
        return threw == o.threw && (threw || status == o.status) && pos == o.pos && factory[0] == o.factory[0] &&
               factory[1] == o.factory[1] && factory[2] == o.factory[2] && dump == o.dump;
    }
};

template <class M> Outcome RunReply(bool inNew, int inKind, M* op, unsigned char t, const std::string& msg)
{
    Outcome o;
    memset(sFactory, 0, sizeof(sFactory));
    ReadBuffer m(msg.data(), (unsigned long)msg.size());
    m.SetPos(3);
    o.threw = false;
    o.status = 0;
    try {
        o.status = (int)(inNew ? NewReply(inKind, op, t, m) : OrigReply(inKind, op, t, m));
    } catch (ReadBufferException&) {
        o.threw = true;
    }
    o.pos = m.pos();
    memcpy(o.factory, sFactory, sizeof(sFactory));
    o.dump = Dump(*op);
    return o;
}

static long long sReplies, sThrows;

template <class M> void TestReplyOn(int inKind, unsigned char t, const std::string& msg)
{
    unsigned int aSeed = sSeed;
    M a, b;
    Fill(a);
    sSeed = aSeed;
    Fill(b);
    // ids that match the message's (the join's and the members' group) half the time
    if (Pick(2) && msg.size() >= 7) {
        unsigned short g = (unsigned char)msg[5] | ((unsigned char)msg[6] << 8);
        a.mGroupId = b.mGroupId = g;
    }
    Outcome oa = RunReply(false, inKind, &a, t, msg), ob = RunReply(true, inKind, &b, t, msg);
    sReplies++;
    if (oa.threw) sThrows++;
    CHECK(oa == ob);
    if (!(oa == ob) && sFailures < 8)
        printf("kind %d type %d len %zu: threw %d/%d status %d/%d pos %d/%d\n", inKind, t, msg.size(), oa.threw,
               ob.threw, oa.status, ob.status, oa.pos, ob.pos);
}

template <class M> void TestReply(int inKind)
{
    G g;
    g.b(3);
    g.b(7);
    g.b(0);
    unsigned char t = Generate(inKind, g);
    g.s[2] = (char)t;
    // whole, every cut, a few mutations
    TestReplyOn<M>(inKind, t, g.s);
    for (size_t n = 3; n < g.s.size(); n++) TestReplyOn<M>(inKind, t, g.s.substr(0, n));
    for (int i = 0; i < 3 && g.s.size() > 3; i++) {
        std::string m = g.s;
        int k = 1 + Pick(3);
        for (int j = 0; j < k; j++) m[3 + Pick((int)m.size() - 3)] = (char)Pick(256);
        TestReplyOn<M>(inKind, t, m);
    }
}

static const int kKinds[] = {
    EE_WON_RT_STATUS, EE_WON_RT_GROUP_LIST, EE_WON_RT_READ_DATA, EE_WON_RT_CREATE_GROUP, EE_WON_RT_JOIN_GROUP,
    EE_WON_RT_REGISTER, EE_WON_RT_CLIENT_LIST, EE_WON_RT_MEMBERS, EE_WON_RT_BAD_USERS, EE_WON_RT_FIREWALL,
    EE_WON_RT_DERIVED, EE_WON_RT_N_CLIENT_FLAGS, EE_WON_RT_N_CLIENT_JOINED_GROUP, EE_WON_RT_N_CLIENT_JOINED_SERVER,
    EE_WON_RT_N_CLIENT_LEFT_GROUP, EE_WON_RT_N_CLIENT_LEFT_SERVER, EE_WON_RT_N_DATA_CREATED,
    EE_WON_RT_N_DATA_DELETED, EE_WON_RT_N_DATA_MODIFIED, EE_WON_RT_N_CAPTAIN_CHANGED, EE_WON_RT_N_GROUP_CREATED,
    EE_WON_RT_N_GROUP_DELETED, EE_WON_RT_N_GROUP_FLAGS, EE_WON_RT_N_INVITATION, EE_WON_RT_N_JOIN_ATTEMPT,
    EE_WON_RT_N_MAX_PLAYERS, EE_WON_RT_N_MEMBER_COUNT, EE_WON_RT_N_GROUP_NAME, EE_WON_RT_N_OBSERVER_COUNT,
    EE_WON_RT_N_MEMBER_FLAGS, EE_WON_RT_N_PEER_CHAT, EE_WON_RT_N_PEER_DATA, EE_WON_RT_N_SERVER_ALERT,
    EE_WON_RT_N_SHUTDOWN_ABORTED, EE_WON_RT_N_SHUTDOWN_STARTED, EE_WON_RT_N_BANNED, EE_WON_RT_N_MUTED };

static void TestReplies()
{
    const int n = sizeof(kKinds) / sizeof(kKinds[0]);
    for (int i = 0; i < 1500 * n; i++) {
        int k = kKinds[i % n];
        // the op's mFlags type
        if (k == EE_WON_RT_BAD_USERS || k == EE_WON_RT_N_JOIN_ATTEMPT) TestReply<M8>(k);
        else if (k == EE_WON_RT_N_GROUP_CREATED) TestReply<M32>(k);
        else TestReply<M16>(k);
    }
}

// ParseGroupListObj, RoutingGetGroupListReqOp::CheckResponse, RoutingGetUserStatusOp::CheckResponse
static int sInvalidHeaders;
static WONStatus OrigGroupListReqCheck(M16* inOp, const ByteBuffer* inResponse)
{
    ReadBuffer aMessage(inResponse->data(),inResponse->length());
    unsigned char aHeader=aMessage.ReadByte();
    unsigned char aService=aMessage.ReadByte();
    unsigned char aType=aMessage.ReadByte();
    if(aHeader!=3 || aService!=7 || aType!=25)
        return (WONStatus)(++sInvalidHeaders, WS_ServerReq_InvalidReplyHeader); // InvalidReplyHeader()
    WONStatus aStatus=aMessage.ReadShort();
    if(aStatus!=0)
        return aStatus;
    if(!OrigParseGroupListObj(inOp,aMessage.data()+aMessage.pos(),aMessage.Available()))
        return WS_MessageUnpackFailure;
    return WS_Success;
}
static WONStatus OrigUserStatusCheck(std::map<unsigned long, unsigned char>& mFriendStatusMap, ReadBuffer& inMessage)
{
    try {
        if (inMessage.ReadByte() != 3) return (WONStatus)WS_None;
        if (inMessage.ReadByte() != 7) return (WONStatus)WS_None;
        if (inMessage.ReadByte() != 73) return (WONStatus)WS_None;
        unsigned char count = inMessage.ReadByte();
        if (count != mFriendStatusMap.size()) return (WONStatus)WS_InvalidMessage;
        std::map<unsigned long, unsigned char>::iterator i = mFriendStatusMap.begin();
        for (int j = 0; j < count; ++i, ++j)
            i->second = inMessage.ReadByte();
        return (WONStatus)WS_Success;
    } catch (ReadBufferException&) {
        return (WONStatus)WS_MessageUnpackFailure;
    }
}

static void TestWholeMessages()
{
    for (int i = 0; i < 4000; i++) {
        // a group list request's reply
        G g;
        if (Pick(8)) { g.b(3); g.b(7); g.b(25); } else { g.b(Pick(4)); g.b(7 + Pick(2)); g.b(24 + Pick(2)); }
        g.status();
        GroupListBody(g);
        for (size_t n = 0; n <= g.s.size(); n++) {
            std::string msg = g.s.substr(0, n);
            if (n == g.s.size() && Pick(2) && n > 3) msg[3 + Pick((int)n - 3)] = (char)Pick(256);
            unsigned int aSeed = sSeed;
            M16 a, b;
            Fill(a);
            sSeed = aSeed;
            Fill(b);
            ByteBufferPtr r = new ByteBuffer(msg.data(), (unsigned long)msg.size(), false);
            Outcome oa, ob;
            oa.status = ob.status = 0;
            memset(sFactory, 0, sizeof(sFactory));
            sInvalidHeaders = 0;
            oa.threw = false;
            try { oa.status = OrigGroupListReqCheck(&a, r.get()); } catch (ReadBufferException&) { oa.threw = true; }
            int aInvalid = sInvalidHeaders;
            memcpy(oa.factory, sFactory, sizeof(sFactory));
            oa.pos = 0;
            oa.dump = Dump(a);
            memset(sFactory, 0, sizeof(sFactory));
            ob.threw = false;
            bool anInvalid = false;
            try {
                ob.status = RouteGroupListReqReply(&b, r.get(), &anInvalid);
                if (anInvalid) ob.status = WS_ServerReq_InvalidReplyHeader;
            } catch (ReadBufferException&) { ob.threw = true; }
            memcpy(ob.factory, sFactory, sizeof(sFactory));
            ob.pos = 0;
            ob.dump = Dump(b);
            CHECK(oa == ob);
            CHECK((aInvalid != 0) == anInvalid);
            // the body alone (ParseGroupListObj)
            if (msg.size() >= 5) {
                M16 c, d;
                bool x = OrigParseGroupListObj(&c, msg.data() + 5, (unsigned long)msg.size() - 5);
                bool y = RouteParseGroupListObj(&d, msg.data() + 5, (unsigned long)msg.size() - 5);
                CHECK(x == y);
                CHECK(Dump(c) == Dump(d));
            }
        }
        // a user status reply
        std::map<unsigned long, unsigned char> aMap, bMap;
        int n = Pick(5);
        for (int j = 0; j < n; j++) aMap.insert(std::make_pair(ALong(), (unsigned char)Pick(3)));
        bMap = aMap;
        G u;
        u.b(Pick(8) ? 3 : 2);
        u.b(Pick(8) ? 7 : 8);
        u.b(Pick(8) ? 73 : 72);
        u.b(Pick(6) ? n : Pick(6));
        for (int j = 0; j < n; j++) u.b(Pick(256));
        for (size_t k = 0; k <= u.s.size(); k++) {
            std::string msg = u.s.substr(0, k);
            std::map<unsigned long, unsigned char> x = aMap, y = bMap;
            ReadBuffer ra(msg.data(), (unsigned long)msg.size()), rb(msg.data(), (unsigned long)msg.size());
            WONStatus sa = OrigUserStatusCheck(x, ra), sb = RouteUserStatusReply(y, rb);
            CHECK(sa == sb);
            CHECK(x == y);
            CHECK(ra.pos() == rb.pos());
        }
    }
}

// MsgCallback's header and dispatch, GetUnsolicitedOp's table
static int OrigUnsolicited(unsigned char messageType)
{
    switch(messageType) {
    case 6: return EE_WON_RT_N_CLIENT_FLAGS;
    case 7: return EE_WON_RT_N_CLIENT_JOINED_GROUP;
    case 8: return EE_WON_RT_N_CLIENT_JOINED_SERVER;
    case 9: return EE_WON_RT_N_CLIENT_LEFT_GROUP;
    case 10: return EE_WON_RT_N_CLIENT_LEFT_SERVER;
    case 14: return EE_WON_RT_N_DATA_CREATED;
    case 15: return EE_WON_RT_N_DATA_DELETED;
    case 16: return EE_WON_RT_N_DATA_MODIFIED;
    case 31: return EE_WON_RT_N_CAPTAIN_CHANGED;
    case 32: return EE_WON_RT_N_GROUP_CREATED;
    case 33: return EE_WON_RT_N_GROUP_DELETED;
    case 34: return EE_WON_RT_N_GROUP_FLAGS;
    case 35: return EE_WON_RT_N_INVITATION;
    case 36: return EE_WON_RT_N_JOIN_ATTEMPT;
    case 37: return EE_WON_RT_N_MAX_PLAYERS;
    case 71: return EE_WON_RT_N_MEMBER_COUNT;
    case 38: return EE_WON_RT_N_GROUP_NAME;
    case 39: return EE_WON_RT_N_OBSERVER_COUNT;
    case 44: return EE_WON_RT_N_MEMBER_FLAGS;
    case 47: return EE_WON_RT_N_PEER_CHAT;
    case 48: return EE_WON_RT_N_PEER_DATA;
    case 57: return EE_WON_RT_N_SERVER_ALERT;
    case 58: return EE_WON_RT_N_SHUTDOWN_ABORTED;
    case 59: return EE_WON_RT_N_SHUTDOWN_STARTED;
    case 69: return EE_WON_RT_N_BANNED;
    case 70: return EE_WON_RT_N_MUTED;
    }
    return -1;
}

static void TestConnection()
{
    for (int t = 0; t < 256; t++) CHECK(ee_won_route_unsolicited((uint8_t)t) == OrigUnsolicited((unsigned char)t));
    for (int i = 0; i < 5000; i++) {
        std::string m;
        int n = Pick(6);
        for (int j = 0; j < n; j++) m += (char)(Pick(3) ? 7 : Pick(256));
        bool threw = false;
        unsigned char msgType = 0;
        bool derived = false;
        int pos = 0;
        try {
            ReadBuffer msg(m.data(), (unsigned long)m.size());
            msg.ReadByte();
            unsigned char service = msg.ReadByte();
            msgType = msg.ReadByte();
            derived = service != 7;
            pos = msg.pos();
        } catch (ReadBufferException&) {
            threw = true;
        }
        unsigned char t2 = 0;
        bool d2 = false;
        uint32_t p2 = 0;
        bool ok = ee_won_route_header(reinterpret_cast<const uint8_t*>(m.data()), m.size(), &t2, &d2, &p2);
        CHECK(ok == !threw);
        if (ok) { CHECK(t2 == msgType); CHECK(d2 == derived); CHECK((int)p2 == pos); }
    }
    static const int kStatuses[] = { 0, 1, -1, -2541, 100104, 100105, 100103, 100012 };
    for (size_t i = 0; i < sizeof(kStatuses) / sizeof(kStatuses[0]); i++) {
        WONStatus status = (WONStatus)kStatuses[i];
        int want = status == (WONStatus)100105 ? 1 : (status != (WONStatus)100104 ? 2 : 0);
        CHECK(ee_won_route_dispatch(kStatuses[i]) == want);
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    TestRequests();
    TestReplies();
    TestWholeMessages();
    TestConnection();
    printf("won_routemsg_test: %lld checks (%lld replies, %lld cut or bad), %d failures\n", sChecks, sReplies,
           sThrows, sFailures);
    return sFailures;
}
