// WONAPI routing ops: the AsyncOp shim over a Rust routing op (rust/crates/ee_won/src/routeop.rs EeWonRouteOp)
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
static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void Units(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    WideAssignUtf16LE(*static_cast<std::wstring*>(inContext), inUnits, (unsigned long)inCount);
}
static std::wstring Wide(const uint16_t* inUnits, size_t inCount)
{
    std::wstring s;
    WideAssignUtf16LE(s, inUnits, (unsigned long)inCount);
    return s;
}

// FUNCTION: WON 0x006c7b40
// Creates a routing op of a type on a connection (or none): the Rust op with its constructor's members
RoutingOp::RoutingOp(RoutingConnection* inConnection, RoutingOpType inType)
    : mConnection(inConnection), mRust(ee_won_routeop_new((int32_t)inType)) {}
RoutingOp::~RoutingOp() { ee_won_routeop_free(this->mRust); }
RoutingOpType RoutingOp::GetType() const { return (RoutingOpType)ee_won_routeop_type(this->mRust); }

// FUNCTION: WON 0x006c7d20
// Sends the request; fails at once in blocking mode, which routing ops do not support
void RoutingOp::RunHook()
{
    if (this->GetMode() == OP_MODE_BLOCK) {
        this->Finish((WONStatus)WS_BlockMode_NotSupported);
        return;
    }
    this->SendRequest();
}

// The op's request (Rust's): sent, then the op waits in the connection's list when it wants a reply
void RoutingOp::SendRequest()
{
    std::string aMessage;
    int32_t aSent = ee_won_routeop_request(this->mRust, Append, &aMessage);
    if (aSent & 1) this->SendMsg(aMessage);
    if (aSent & 2) this->AddOp();
}

// The op's read of a reply (Rust's); a read past the end throws as the original's did, and the create op's leave
// (CheckLeaveGroup 2c4cb0) runs here
WONStatus RoutingOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    EeWonRouteOpRead aRead;
    ee_won_routeop_reply(this->mRust, inMsgType, reinterpret_cast<const uint8_t*>(inMsg.data()), inMsg.length(),
                         (size_t)inMsg.pos(), this->GetStatus() == (WONStatus)WS_Pending, &aRead);
    if (aRead.leave) {
        SmartPtr<RoutingOp> aLeave = new RoutingOp(this->mConnection.get(), RoutingOp_LeaveGroup);
        aLeave->SetNum(EE_WON_RTF_GROUP_ID, aRead.leave_group);
        aLeave->RunAsync(-1);
    }
    if (aRead.cut) throw ReadBufferException("Attempt to read past end of buffer.");
    return (WONStatus)aRead.status;
}

// FUNCTION: WON 0x6c7c00
// Queues the message on the connection for sending, with the connection's length field
void RoutingOp::SendMsg(const std::string& inMessage)
{
    WriteBuffer aMsg(this->mConnection->GetLengthFieldSize(), true);
    if (!inMessage.empty()) aMsg.AppendBytes(inMessage.data(), (unsigned long)inMessage.size());
    this->mConnection->QueueOp(new SendMsgOp(aMsg.ToByteBuffer(true).get()), -1);
}

// FUNCTION: WON 0x006c7d10
// Registers the op with the connection to receive replies
void RoutingOp::AddOp() { this->mConnection->AddOp(this); }

// ---- the members

std::wstring RoutingOp::Text(int inWhich) const
{
    std::wstring s;
    ee_won_routeop_text(this->mRust, inWhich, Units, &s);
    return s;
}
void RoutingOp::SetText(int inWhich, const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    ee_won_routeop_set_text(this->mRust, inWhich, reinterpret_cast<const uint16_t*>(aBytes.data()), aBytes.size() / 2);
}
std::string RoutingOp::DataType() const
{
    std::string s;
    ee_won_routeop_data_type(this->mRust, Append, &s);
    return s;
}
void RoutingOp::SetDataType(const std::string& inType)
{
    ee_won_routeop_set_data_type(this->mRust, reinterpret_cast<const uint8_t*>(inType.data()), inType.size());
}
ByteBufferPtr RoutingOp::GetData() const
{
    std::string s;
    if (!ee_won_routeop_data(this->mRust, Append, &s)) return NULL;
    return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
}
void RoutingOp::SetData(const ByteBuffer* inData)
{
    if (inData == NULL) ee_won_routeop_set_data(this->mRust, false, NULL, 0);
    else
        ee_won_routeop_set_data(this->mRust, true, reinterpret_cast<const uint8_t*>(inData->data()), inData->length());
}
std::list<unsigned short> RoutingOp::Recipients() const
{
    size_t n = ee_won_routeop_ids(this->mRust, NULL, 0);
    std::vector<uint16_t> anIds(n + 1);
    ee_won_routeop_ids(this->mRust, &anIds[0], n);
    return std::list<unsigned short>(anIds.begin(), anIds.begin() + n);
}
void RoutingOp::SetRecipients(const std::list<unsigned short>& inIds)
{
    std::vector<uint16_t> anIds(inIds.begin(), inIds.end());
    ee_won_routeop_set_ids(this->mRust, anIds.empty() ? NULL : &anIds[0], anIds.size());
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

// FUNCTION: WON 0x006c6cb0
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

// FUNCTION: WON 0x006c5f70
// Parses a group list body into the group map (Rust's read); false when the body runs short (what was read stays)
bool RoutingOp::ParseGroupListObj(const void* inData, unsigned long inSize)
{
    return ee_won_routeop_parse_group_list(this->mRust, static_cast<const uint8_t*>(inData), inSize);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7b20
// (public: virtual void * __thiscall WONAPI::RoutingOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingOp@WONAPI@@UAEPAXI@Z])
