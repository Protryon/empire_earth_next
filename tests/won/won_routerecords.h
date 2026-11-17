// The routing server's C++ records and the copies the drivers keep. Since R6 WONLobby round 12 the game reads a
// routing op's records and the smart connection's model in Rust (ee_won_lobby modelobj.rs, the lobby's bad user
// list and EEWonNet.cpp over the C ABI), so the records (WONRouting.h's RoutingClientInfo .. RoutingBadUser),
// RoutingOp's getters (RoutingOp.cpp) and SmartRoutingConnection's copies (SmartRoutingConnection.cpp) live here,
// their statements unchanged: the drivers print the copies as they printed the classes'.
#ifndef EE_TESTS_WON_ROUTERECORDS_H
#define EE_TESTS_WON_ROUTERECORDS_H
#include "rust/won_net/routeop.h"
#include "rust/won_net/smartroute.h"
namespace WONAPI {

// a client of the routing server (+8 its name)
struct RoutingClientInfo : public RefCount {
    RoutingClientInfo() : mClientId(0),mFlags(0),mUserId(0),mAddress(0),mTrustLevel(0) {} //inline default (WON SDK)
    std::wstring mName;                     // +0x08
    unsigned short mClientId;               // +0x18
    unsigned long mFlags;                   // +0x1c (Client init 0x3a5350)
    unsigned long mUserId;                  // +0x20
    unsigned long mAddress;                 // +0x24, CDKeyId (historical spelling retained)
    unsigned short mTrustLevel; //28, the base factory allocates 0x2c and stores the WORD at 28
};
typedef SmartPtr<RoutingClientInfo> RoutingClientInfoPtr;

typedef std::map<unsigned short, RoutingClientInfoPtr> RoutingClientMap;

// a member of a group: the client it is
struct RoutingMemberInfo : public RefCount {
    RoutingMemberInfo() : mClientId(0),mFlags(0) {} //inline default (WON SDK); mClientInfo defaults to NULL
    unsigned short mClientId;               // +0x08
    unsigned char mFlags; //0a, BYTE; natural padding 0b; 2c5da5/2c64e7 write a BYTE
    RoutingClientInfoPtr mClientInfo;       // +0x0c
};
typedef SmartPtr<RoutingMemberInfo> RoutingMemberInfoPtr;
typedef std::map<unsigned short, RoutingMemberInfoPtr> RoutingMemberMap;

// a group (room or game) of the routing server
struct RoutingGroupInfo : public RefCount {
    RoutingGroupInfo() : mId(0),mParentId(0xffff),mCaptainId(0),mMaxClients(0),mFlags(0),mAsyncFlags(0),mObserverCount(0),mMaxPlayers(0) {} //WON SDK; historical protected names retained
    std::wstring mName;                     // +0x08
    unsigned short mId;                     // +0x18
    unsigned short mParentId; //1a, SDK mParentId; 2ba361 stores ffff
    unsigned short mCaptainId;             // +0x1c
    unsigned short mMaxClients;            // +0x1e
    unsigned long mFlags;                   // +0x20 (0x800: the room is public; 0x8 in byte +0x21: a chat room)
    unsigned long mAsyncFlags; //24, the group reply stores a DWORD
    unsigned short mObserverCount; //28, stores a WORD
    unsigned short mMaxPlayers;             // +0x2a
    RoutingMemberMap mMembers;              // +0x2c
};
typedef SmartPtr<RoutingGroupInfo> RoutingGroupInfoPtr;
typedef std::map<unsigned short, RoutingGroupInfoPtr> RoutingGroupMap;

// a data object of the routing server (the game's room/game descriptions)
struct RoutingDataObject : public RefCount {
    RoutingDataObject() : mLinkId(0) {} //new 0x30, inline ctor 2c81c9..2c8209
    unsigned short mLinkId;                 // +0x08
    std::string mDataType;                  // +0x0c
    std::wstring mName;                     // +0x1c
    ByteBufferPtr mData;                    // +0x2c

    unsigned long GetDataLength() const { return this->mData->length(); }
    const char* GetData() const { return this->mData->data(); }
};
typedef SmartPtr<RoutingDataObject> RoutingDataObjectPtr;
typedef std::list<RoutingDataObjectPtr> RoutingDataObjectList;

struct RoutingBadUser {
    RoutingBadUser() : mWONId(0), mExpirationDiff(0), mModeratorWONId(0) {} // 2c5429..2c5475
    std::wstring mName; //0..f
    unsigned long mWONId; //10
    unsigned long mExpirationDiff; //14
    unsigned long mModeratorWONId; //18
    std::wstring mModeratorName; //1c..2b
    std::wstring mModeratorComment; //2c..3b
    // Implicit cleanup/copy; no out-of-line destructor.
};
typedef std::list<RoutingBadUser> RoutingBadUserList;

// ---- RoutingOp.cpp: the records a Rust routing op built, copied into new C++ records

namespace RouteRecordsCopy {
inline std::wstring Wide(const uint16_t* inUnits, size_t inCount)
{
    std::wstring s;
    WideAssignUtf16LE(s, inUnits, (unsigned long)inCount);
    return s;
}
struct RecordCopy {
    RoutingClientMap* mClients;
    RoutingMemberMap* mMembers;
    RoutingGroupMap* mGroups;
    RoutingDataObjectList* mObjects;
    RoutingBadUserList* mBadUsers;
    RoutingClientInfoPtr mClient;
    RoutingGroupInfoPtr mGroup;
    RoutingMemberInfoPtr mMember;
    RecordCopy() : mClients(NULL), mMembers(NULL), mGroups(NULL), mObjects(NULL), mBadUsers(NULL) {}
};
inline RoutingClientInfoPtr NewClient(const EeWonRouteRecord& r)
{
    RoutingClientInfoPtr c = new RoutingClientInfo;
    c->mClientId = r.id;
    c->mName = Wide(r.name, r.name_len);
    c->mFlags = r.flags;
    c->mUserId = r.user_id;
    c->mAddress = r.address;
    c->mTrustLevel = r.trust;
    return c;
}
inline RoutingMemberInfoPtr NewMember(const EeWonRouteRecord& r)
{
    RoutingMemberInfoPtr m = new RoutingMemberInfo;
    m->mClientId = r.id;
    m->mFlags = r.member_flags;
    return m;
}
inline void OnRecord(void* inContext, const EeWonRouteRecord* inRecord)
{
    RecordCopy& c = *static_cast<RecordCopy*>(inContext);
    const EeWonRouteRecord& r = *inRecord;
    switch (r.kind) {
    case EE_WON_RTR_CLIENT: (*c.mClients)[r.key] = NewClient(r); break;
    case EE_WON_RTR_OP_CLIENT: c.mClient = NewClient(r); break;
    case EE_WON_RTR_MEMBER: c.mMember = NewMember(r); (*c.mMembers)[r.key] = c.mMember; break;
    case EE_WON_RTR_GROUP_MEMBER: c.mMember = NewMember(r); c.mGroup->mMembers[r.key] = c.mMember; break;
    case EE_WON_RTR_MEMBER_CLIENT: c.mMember->mClientInfo = NewClient(r); break;
    case EE_WON_RTR_GROUP: {
        RoutingGroupInfoPtr g = new RoutingGroupInfo;
        g->mId = r.id;
        g->mName = Wide(r.name, r.name_len);
        g->mParentId = r.parent;
        g->mCaptainId = r.captain;
        g->mMaxClients = r.max_clients;
        g->mFlags = r.flags;
        g->mAsyncFlags = r.async_flags;
        g->mObserverCount = r.observers;
        g->mMaxPlayers = r.max_players;
        (*c.mGroups)[r.key] = g;
        c.mGroup = g;
        break;
    }
    case EE_WON_RTR_OBJECT: {
        RoutingDataObjectPtr o = new RoutingDataObject;
        o->mLinkId = r.id;
        o->mDataType.assign(reinterpret_cast<const char*>(r.data_type), r.data_type_len);
        o->mName = Wide(r.name, r.name_len);
        if (r.has_data) o->mData = new ByteBuffer(r.data, (unsigned long)r.data_len, false);
        c.mObjects->push_back(o);
        break;
    }
    case EE_WON_RTR_BAD_USER: {
        RoutingBadUser b;
        b.mName = Wide(r.name, r.name_len);
        b.mWONId = r.user_id;
        b.mExpirationDiff = r.expiration;
        b.mModeratorWONId = r.moderator_id;
        b.mModeratorName = Wide(r.moderator_name, r.moderator_name_len);
        b.mModeratorComment = Wide(r.comment, r.comment_len);
        c.mBadUsers->push_back(b);
        break;
    }
    }
}
}

// RouteOpRecords(op).GetClients(map) etc.: the getters RoutingOp had
class RouteOpRecords {
public:
    explicit RouteOpRecords(const RoutingOp* inOp) : mOp(inOp) {}
    void GetClients(RoutingClientMap& outMap) const
    {
        RouteRecordsCopy::RecordCopy c;
        c.mClients = &outMap;
        outMap.clear();
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_CLIENT, RouteRecordsCopy::OnRecord, &c);
    }
    void GetMembers(RoutingMemberMap& outMap) const
    {
        RouteRecordsCopy::RecordCopy c;
        c.mMembers = &outMap;
        outMap.clear();
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_MEMBER, RouteRecordsCopy::OnRecord, &c);
    }
    void GetGroups(RoutingGroupMap& outMap) const
    {
        RouteRecordsCopy::RecordCopy c;
        c.mGroups = &outMap;
        outMap.clear();
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_GROUP, RouteRecordsCopy::OnRecord, &c);
    }
    void GetDataObjects(RoutingDataObjectList& outList) const
    {
        RouteRecordsCopy::RecordCopy c;
        c.mObjects = &outList;
        outList.clear();
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_OBJECT, RouteRecordsCopy::OnRecord, &c);
    }
    void GetBadUsers(RoutingBadUserList& outList) const
    {
        RouteRecordsCopy::RecordCopy c;
        c.mBadUsers = &outList;
        outList.clear();
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_BAD_USER, RouteRecordsCopy::OnRecord, &c);
    }
    RoutingClientInfoPtr GetClientInfo() const
    {
        RouteRecordsCopy::RecordCopy c;
        ee_won_routeop_records(this->mOp->mRust, EE_WON_RTR_OP_CLIENT, RouteRecordsCopy::OnRecord, &c);
        return c.mClient;
    }
    // Returns a new group record filled from the group created notice
    RoutingGroupInfoPtr GetGroupInfo() const
    {
        RoutingGroupInfoPtr group = new RoutingGroupInfo;
        group->mId = this->mOp->GetGroupId();
        group->mName = this->mOp->Text(EE_WON_RTT_NAME);
        group->mMaxClients = (unsigned short)this->mOp->Num(EE_WON_RTF_MAX_CLIENTS);
        group->mFlags = (unsigned long)this->mOp->Num(EE_WON_RTF_GROUP_FLAGS);
        group->mAsyncFlags = (unsigned long)this->mOp->Num(EE_WON_RTF_ASYNC_FLAGS);
        return group;
    }
private:
    const RoutingOp* mOp;
};

// ---- SmartRoutingConnection.cpp: the model's records copied (SmartCopies(connection).GetClient(id, info) etc.)

class SmartCopies {
public:
    // WON SDK value-copy records; independent, no enclosing-object fields.
    struct ClientInfo {
        std::wstring mName;
        unsigned short mId;
        unsigned long mFlags;
        unsigned long mCDKeyId;
        ClientInfo() : mId(0), mFlags(0), mCDKeyId(0) {}
    };
    struct MemberInfo {
        std::wstring mName;
        unsigned short mClientId;
        unsigned long mClientFlags;
        unsigned char mMemberFlags;
        MemberInfo() : mClientId(0), mClientFlags(0), mMemberFlags(0) {}
    };
    struct GroupInfo {
        std::wstring mName;
        unsigned short mId;
        unsigned short mCaptainId;
        unsigned short mMaxPlayers;     // the group's client limit (RoutingGroupInfo::mMaxClients)
        unsigned long mFlags;
        unsigned long mAsyncFlags;
        unsigned short mObserverCount; // only applicable if non-detailed observers is on
        unsigned short mMemberCount;   // RoutingGroupInfo::mMaxPlayers (+0x2a): the group list's member count
        GroupInfo() : mId(0), mCaptainId(0), mMaxPlayers(0), mFlags(0), mAsyncFlags(0), mObserverCount(0),
            mMemberCount(0) {}
    };
    explicit SmartCopies(SmartRoutingConnection* inConn) : mConn(inConn) {}
    static std::wstring SmartText(const uint32_t* inUnits, size_t inCount)
    {
        std::wstring s;
        s.reserve(inCount);
        for (size_t i = 0; i < inCount; i++)
            s += static_cast<wchar_t>(inUnits[i]);
        return s;
    }
    static void CopyClient(ClientInfo& out, const EeWonSmartRecord& r)
    {
        out.mFlags = r.flags;
        out.mId = r.id;
        out.mName = SmartText(r.name, r.name_len);
        out.mCDKeyId = r.cdkey;
    }
    static void CopyGroup(GroupInfo& out, const EeWonSmartRecord& r)
    {
        out.mCaptainId = r.captain;
        out.mFlags = r.flags;
        out.mAsyncFlags = r.async_flags;
        out.mId = r.id;
        out.mMaxPlayers = r.max_clients;
        out.mName = SmartText(r.name, r.name_len);
        out.mObserverCount = r.observers;
        out.mMemberCount = r.member_count;
    }
    static void CopyMember(MemberInfo& out, const EeWonSmartRecord& r)
    {
        if (r.has_client) {
            out.mName = SmartText(r.name, r.name_len);
            out.mClientFlags = r.client_flags;
        }
        out.mClientId = r.id;
        out.mMemberFlags = r.member_flags;
    }
#define SMART_GET(which, a, b, copy, out) \
    AutoCrit aCrit(this->mConn->GetDataLock()); \
    EeWonSmartRecord r; \
    if (!ee_won_smart_get(this->mConn->mModel, which, a, b, &r)) return false; \
    copy(out, r); \
    return true
    bool GetClient(unsigned short id, ClientInfo& out) { SMART_GET(EeWonSmartGet::Client, id, 0, CopyClient, out); }
    bool GetGroup(unsigned short id, GroupInfo& out) { SMART_GET(EeWonSmartGet::Group, id, 0, CopyGroup, out); }
    bool GetMember(unsigned short group, unsigned short client, MemberInfo& out)
    { SMART_GET(EeWonSmartGet::Member, group, client, CopyMember, out); }
    bool GetNextClient(ClientInfo& out) { SMART_GET(EeWonSmartGet::NextClient, 0, 0, CopyClient, out); }
    bool GetNextGroup(GroupInfo& out) { SMART_GET(EeWonSmartGet::NextGroup, 0, 0, CopyGroup, out); }
    bool GetNextMember(MemberInfo& out) { SMART_GET(EeWonSmartGet::NextMember, 0, 0, CopyMember, out); }
    bool GetLastTargetMember(MemberInfo& out) { SMART_GET(EeWonSmartGet::LastMember, 0, 0, CopyMember, out); }
    bool GetLastTargetClient(ClientInfo& out) { SMART_GET(EeWonSmartGet::LastClient, 0, 0, CopyClient, out); }
#undef SMART_GET
    // Finds the client a typed name refers to: exact, unique prefix, ambiguous or not found
    SmartRoutingConnection::MatchResult FindClient(const std::wstring& inName, ClientInfo& out, bool& outFound)
    {
        AutoCrit aCrit(this->mConn->GetDataLock());
        EeWonSmartRecord r;
        int result = ee_won_smart_find(this->mConn->mModel, inName.data(), inName.length(), &r, &outFound);
        if (outFound) CopyClient(out, r);
        return (SmartRoutingConnection::MatchResult)result;
    }
private:
    SmartRoutingConnection* mConn;
};

}
#endif
