// WONAPI routing ops: the records a Rust routing op built (rust/crates/ee_won/src/routeop.rs EeWonRouteOp), copied
// into new C++ records; the op class is WONRouting.h's
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
#include "rust/ee_won.h"
namespace WONAPI {
static std::wstring Wide(const uint16_t* inUnits, size_t inCount)
{
    std::wstring s;
    WideAssignUtf16LE(s, inUnits, (unsigned long)inCount);
    return s;
}

// ---- the records, copied into new C++ records

namespace {
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
RoutingClientInfoPtr NewClient(const EeWonRouteRecord& r)
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
RoutingMemberInfoPtr NewMember(const EeWonRouteRecord& r)
{
    RoutingMemberInfoPtr m = new RoutingMemberInfo;
    m->mClientId = r.id;
    m->mFlags = r.member_flags;
    return m;
}
void OnRecord(void* inContext, const EeWonRouteRecord* inRecord)
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

void RoutingOp::GetClients(RoutingClientMap& outMap) const
{
    RecordCopy c;
    c.mClients = &outMap;
    outMap.clear();
    ee_won_routeop_records(this->mRust, EE_WON_RTR_CLIENT, OnRecord, &c);
}
void RoutingOp::GetMembers(RoutingMemberMap& outMap) const
{
    RecordCopy c;
    c.mMembers = &outMap;
    outMap.clear();
    ee_won_routeop_records(this->mRust, EE_WON_RTR_MEMBER, OnRecord, &c);
}
void RoutingOp::GetGroups(RoutingGroupMap& outMap) const
{
    RecordCopy c;
    c.mGroups = &outMap;
    outMap.clear();
    ee_won_routeop_records(this->mRust, EE_WON_RTR_GROUP, OnRecord, &c);
}
void RoutingOp::GetDataObjects(RoutingDataObjectList& outList) const
{
    RecordCopy c;
    c.mObjects = &outList;
    outList.clear();
    ee_won_routeop_records(this->mRust, EE_WON_RTR_OBJECT, OnRecord, &c);
}
void RoutingOp::GetBadUsers(RoutingBadUserList& outList) const
{
    RecordCopy c;
    c.mBadUsers = &outList;
    outList.clear();
    ee_won_routeop_records(this->mRust, EE_WON_RTR_BAD_USER, OnRecord, &c);
}
RoutingClientInfoPtr RoutingOp::GetClientInfo() const
{
    RecordCopy c;
    ee_won_routeop_records(this->mRust, EE_WON_RTR_OP_CLIENT, OnRecord, &c);
    return c.mClient;
}

// Returns a new group record filled from the group created notice
RoutingGroupInfoPtr RoutingOp::GetGroupInfo() const
{
    RoutingGroupInfoPtr group = new RoutingGroupInfo;
    group->mId = this->GetGroupId();
    group->mName = this->Text(EE_WON_RTT_NAME);
    group->mMaxClients = (unsigned short)this->Num(EE_WON_RTF_MAX_CLIENTS);
    group->mFlags = (unsigned long)this->Num(EE_WON_RTF_GROUP_FLAGS);
    group->mAsyncFlags = (unsigned long)this->Num(EE_WON_RTF_ASYNC_FLAGS);
    return group;
}

}
