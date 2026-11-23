// The directory server's entity replies: the reading is ee_won's (rust/crates/ee_won_net/src/dir.rs), the ops that read
// them Rust's (reqdir.rs); this makes the DirEntity objects of what they read (RustServerOp::GetEntities). The
// parsers' statements (ParseMultiEntityReply 2f0510, ParseSingleEntityReply, OnlyDirs 2f04f0, OnlyServices 2f0500)
// are tests/won/won_dir_test.cpp's original half.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/won_net/dir.h"
namespace WONAPI {

// DirMakeEntity's members (its decisions are ee_won dir.rs's ee_won_dir_entity_fill, R6 WON round 33)
static void DirSet(void* inEntity, EeWonDirSet inMember, int64_t inNum, const void* p, size_t n)
{
    DirEntity* e = static_cast<DirEntity*>(inEntity);
    const uint8_t* b = static_cast<const uint8_t*>(p);
    const EeWonDirAcl* r = static_cast<const EeWonDirAcl*>(p);
    switch (inMember) {
    case EeWonDirSet::Kind: e->mType = (char)inNum; return;
    case EeWonDirSet::Path: WonWideSink(&e->mPath, static_cast<const uint16_t*>(p), n); return;
    case EeWonDirSet::Name: WonWideSink(&e->mName, static_cast<const uint16_t*>(p), n); return;
    case EeWonDirSet::NetAddr: e->mNetAddr = new ByteBuffer(b, (unsigned long)n, false); return;
    case EeWonDirSet::ReqUnique: e->mReqUnique = inNum != 0; return;
    case EeWonDirSet::DisplayName: WonWideSink(&e->mDisplayName, static_cast<const uint16_t*>(p), n); return;
    case EeWonDirSet::Lifespan: e->mLifespan = (long)inNum; return;
    case EeWonDirSet::Created: e->mCreated = (long)inNum; return;
    case EeWonDirSet::Touched: e->mTouched = (long)inNum; return;
    case EeWonDirSet::Crc: e->mCRC = (unsigned long)(int32_t)inNum; return;
    case EeWonDirSet::CreateIp: e->mCreateIP = (unsigned long)(int32_t)inNum; return;
    case EeWonDirSet::TouchIp: e->mTouchIP = (unsigned long)(int32_t)inNum; return;
    case EeWonDirSet::CreateId: e->mCreateId = (unsigned long)(int32_t)inNum; return;
    case EeWonDirSet::TouchId: e->mTouchId = (unsigned long)(int32_t)inNum; return;
    case EeWonDirSet::ObjectAdd: e->mDataObjects.push_back(DirDataObject{std::string(reinterpret_cast<const char*>(b), n)}); return;
    case EeWonDirSet::ObjectData: e->mDataObjects.back().mData = new ByteBuffer(b, (unsigned long)n, false); return;
    // the original's long user, long community and short trust locals, then the unsigned members
    case EeWonDirSet::Acl: e->mACLs.push_back(DirACL{(char)r->kind, (unsigned long)(long)r->user, (unsigned long)(long)r->community, (unsigned short)(short)r->trust}); return;
    }
}

// an empty entity: no type, times, CRC or addresses (DirEntity.cpp is merged here: net round 29)
DirEntity::DirEntity()
    : mType(0), mLifespan(0), mCRC(0), mCreated(0), mCreateIP(0), mCreateId(0), mTouched(0), mTouchIP(0), mTouchId(0),
      mReqUnique(false)
{
}
DirEntity::~DirEntity() {}
DirEntity::DirEntity(const EeWonDirEntity* inEntity) : DirEntity() { ee_won_dir_entity_fill(inEntity, DirSet, this); }
// Returns the service's network address as an IP address and port (empty when shorter than 6 bytes; 2f0400)
IPAddr DirEntity::GetNetAddrAsIP() const
{
    IPAddr result;
    if (this->mNetAddr.get() != NULL && this->mNetAddr->length() >= 6)
        result.SetSixByte(this->mNetAddr->data());
    return result;
}

// No original address identified: the parsers' entity sink, for the Rust ops' results
// Appends a DirEntity made of what ee_won read to a DirEntityList
void DirEntityReplyParser::TakeEntity(void* inList, const EeWonDirEntity* inEntity)
{
    static_cast<DirEntityList*>(inList)->push_back(new DirEntity(inEntity));
}
}
