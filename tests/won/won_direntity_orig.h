// DirEntity, its data objects and ACLs, DirEntityList and DirEntityReplyParser::TakeEntity as they were at ab1b33bcb
// (ee/EEWonNet.h, WONDir/DirEntityReplyParser.cpp, ee/wonapi/WONRouting.h): R6 WON round 34 made them Rust's
// (ee_won_net dirlist.rs, the C++ DirList); the drivers that built and read the C++ objects keep this copy, and
// won_dir34_test checks the Rust list against it. Include after ee/EEWonNet.h, ee/wonapi/WONBuffer.h and
// rust/won_net/dir.h.
#ifndef WON_DIRENTITY_ORIG_H
#define WON_DIRENTITY_ORIG_H
#include <list>
#include <string>
#include "rust/won_net/dir.h"
namespace WONAPI {
struct DirDataObject {
    std::string mDataType; //00..0f
    ConstSmartPtr<ByteBuffer> mData; //10, owning
};
typedef std::list<DirDataObject> DirDataObjectList;
struct DirACL {
    char mType; //00, naturalalignment01..03
    unsigned long mUser; //04
    unsigned long mCommunity; //08
    unsigned short mTrust; //0c, naturalalignment0e..0f
};
typedef std::list<DirACL> DirACLList;
// Complete native7c owner; ctor2f0190/new7c2f066e/dtor2f0290.
class DirEntity : public RefCount {
protected:
    virtual ~DirEntity();
public:
    DirEntity(); // DirEntityReplyParser.cpp (its members need ByteBuffer)
    explicit DirEntity(const EeWonDirEntity* inEntity); // the members ee_won read (DirEntityReplyParser.cpp)
    IPAddr GetNetAddrAsIP() const; //native2f0400
    char mType; //08, naturalalignment09..0b
    std::wstring mPath; //0c
    std::wstring mName; //1c
    std::wstring mDisplayName; //2c
    long mLifespan; //3c, native32bit time_t
    unsigned long mCRC; //40
    long mCreated; //44
    unsigned long mCreateIP; //48
    unsigned long mCreateId; //4c
    long mTouched; //50
    unsigned long mTouchIP; //54
    unsigned long mTouchId; //58
    bool mReqUnique; //5c, naturalalignment5d..5f
    ConstSmartPtr<ByteBuffer> mNetAddr; //60
    DirDataObjectList mDataObjects; //64..6f
    DirACLList mACLs; //70..7b
};

typedef SmartPtr<DirEntity> DirEntityPtr;
typedef std::list<DirEntityPtr> DirEntityList;
class DirEntityReplyParser {
public:
    static void TakeEntity(void* inList, const EeWonDirEntity* inEntity); // appends to the DirEntityList
};
// DirMakeEntity's members (its decisions are ee_won dir.rs's ee_won_dir_entity_fill, R6 WON round 33)
inline void OrigDirSet(void* inEntity, EeWonDirSet inMember, int64_t inNum, const void* p, size_t n)
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
inline DirEntity::DirEntity()
    : mType(0), mLifespan(0), mCRC(0), mCreated(0), mCreateIP(0), mCreateId(0), mTouched(0), mTouchIP(0), mTouchId(0),
      mReqUnique(false)
{
}
inline DirEntity::~DirEntity() {}
inline DirEntity::DirEntity(const EeWonDirEntity* inEntity) : DirEntity() { ee_won_dir_entity_fill(inEntity, OrigDirSet, this); }
// Returns the service's network address as an IP address and port (empty when shorter than 6 bytes; 2f0400)
inline IPAddr DirEntity::GetNetAddrAsIP() const
{
    IPAddr result;
    if (this->mNetAddr.get() != NULL && this->mNetAddr->length() >= 6)
        result.SetSixByte(this->mNetAddr->data());
    return result;
}

// No original address identified: the parsers' entity sink, for the Rust ops' results
// Appends a DirEntity made of what ee_won read to a DirEntityList
inline void DirEntityReplyParser::TakeEntity(void* inList, const EeWonDirEntity* inEntity)
{
    static_cast<DirEntityList*>(inList)->push_back(new DirEntity(inEntity));
}
}
#endif
