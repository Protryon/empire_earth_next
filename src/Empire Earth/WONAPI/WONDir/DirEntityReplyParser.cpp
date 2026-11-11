// WONAPI parser for the directory server's entity replies. The reading is ee_won's (rust/crates/ee_won/src/dir.rs);
// this makes the DirEntity objects from what it read and throws ReadBufferException for a reply cut short, as
// ReadBuffer did (the entities read whole before the cut are kept).
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
#include "ee/wonapi/WONDirExtended.h"
namespace WONAPI {

static void DirAssign(std::wstring& outText, const uint16_t* inUnits, size_t inCount)
{
    std::string aBytes;
    aBytes.reserve(inCount * 2);
    for (size_t i = 0; i < inCount; i++) {
        aBytes += (char)(inUnits[i] & 0xff);
        aBytes += (char)(inUnits[i] >> 8);
    }
    WideAssignUtf16LE(outText, aBytes.data(), (unsigned long)inCount);
}

// the read fields onto a new DirEntity (the others keep the constructor's values); the ACL values converted as the
// originals' locals did (the multi-entity reader's long / short, the single-entity reader's unsigned ones)
static DirEntityPtr DirMakeEntity(const EeWonDirEntity* inEntity, bool inSigned)
{
    DirEntityPtr anEntity = new DirEntity;
    uint32_t f = inEntity->fields;
    if (f & 0x1) anEntity->mType = (char)inEntity->kind;
    if (f & 0x2) DirAssign(anEntity->mPath, inEntity->path, inEntity->path_len);
    if (f & 0x4) DirAssign(anEntity->mName, inEntity->name, inEntity->name_len);
    if (f & 0x8)
        anEntity->mNetAddr = new ByteBuffer(inEntity->net_addr, (unsigned long)inEntity->net_addr_len, false);
    if (f & 0x10) anEntity->mReqUnique = inEntity->req_unique;
    if (f & 0x20) DirAssign(anEntity->mDisplayName, inEntity->display_name, inEntity->display_name_len);
    if (f & 0x40) anEntity->mLifespan = inEntity->lifespan;
    if (f & 0x80) anEntity->mCreated = inEntity->created;
    if (f & 0x100) anEntity->mTouched = inEntity->touched;
    if (f & 0x200) anEntity->mCRC = inEntity->crc;
    if (f & 0x400) {
        anEntity->mCreateIP = inEntity->create_ip;
        anEntity->mTouchIP = inEntity->touch_ip;
    }
    if (f & 0x800) {
        anEntity->mCreateId = inEntity->create_id;
        anEntity->mTouchId = inEntity->touch_id;
    }
    for (size_t i = 0; i < inEntity->object_count; i++) {
        const EeWonDirDataObject& anObject = inEntity->objects[i];
        DirDataObject aData;
        aData.mDataType.assign(reinterpret_cast<const char*>(anObject.data_type), anObject.data_type_len);
        if (anObject.has_data)
            aData.mData = new ByteBuffer(anObject.data, (unsigned long)anObject.data_len, false);
        anEntity->mDataObjects.push_back(aData);
    }
    for (size_t i = 0; i < inEntity->acl_count; i++) {
        const EeWonDirAcl& aRead = inEntity->acls[i];
        DirACL acl;
        acl.mType = (char)aRead.kind;
        if (inSigned) {
            long user = aRead.user;
            long comm = aRead.community;
            short trust = aRead.trust;
            acl.mUser = user; acl.mCommunity = comm; acl.mTrust = trust;
        } else {
            unsigned long user = (unsigned long)(long)aRead.user;
            unsigned long comm = (unsigned long)(long)aRead.community;
            unsigned short trust = (unsigned short)aRead.trust;
            acl.mUser = user; acl.mCommunity = comm; acl.mTrust = trust;
        }
        anEntity->mACLs.push_back(acl);
    }
    return anEntity;
}

static void DirTakeMulti(void* inContext, const EeWonDirEntity* inEntity)
{
    static_cast<DirEntityList*>(inContext)->push_back(DirMakeEntity(inEntity, true));
}

static void DirTakeSingle(void* inContext, const EeWonDirEntity* inEntity)
{
    *static_cast<DirEntityPtr*>(inContext) = DirMakeEntity(inEntity, false);
}

// FUNCTION: WON 0x006f04f0
// Returns whether the get flags ask for directories only (no services)
bool DirEntityReplyParser::OnlyDirs(DWORD inFlags)
{
    return (inFlags & 2) == 0;
}
// FUNCTION: WON 0x006f0500
// Returns whether the get flags ask for services only (no directories)
bool DirEntityReplyParser::OnlyServices(DWORD inFlags)
{
    return (inFlags & 5) == 0;
}
// FUNCTION: WON 0x006f0510
// Parses a multi-entity reply into entities with the fields its flags include; says whether more replies follow
WONStatus DirEntityReplyParser::ParseMultiEntityReply(const void* inData,
    unsigned long inLength, DirEntityList& outEntities, unsigned char* outSequence)
{
    EeWonDirReply aReply = ee_won_dir_parse_multi(static_cast<const uint8_t*>(inData), inLength, DirTakeMulti,
                                                  &outEntities);
    if (aReply.has_sequence && outSequence != NULL) *outSequence = aReply.sequence;
    switch (aReply.kind) {
    case 0: return (WONStatus)aReply.status;
    case 1: return (WONStatus)WS_ServerReq_InvalidReplyHeader;
    case 2: return (WONStatus)aReply.status;
    default: throw ReadBufferException("Attempt to read past end of buffer.");
    }
}
}

// No original address identified: WON SDK single-entity reply parser
namespace WONAPI {
WONStatus DirEntityReplyParser::ParseSingleEntityReply(const void *theMsg, unsigned long theMsgLen, DirEntityPtr &theEntity)
{
    EeWonDirReply aReply = ee_won_dir_parse_single(static_cast<const uint8_t*>(theMsg), theMsgLen, DirTakeSingle,
                                                   &theEntity);
    switch (aReply.kind) {
    case 0: return (WONStatus)WS_Success;
    case 1: return (WONStatus)WS_ServerReq_InvalidReplyHeader;
    case 2: return (WONStatus)aReply.status;
    default: throw ReadBufferException("Attempt to read past end of buffer.");
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1f10
// (protected: void __thiscall std::list<struct WONAPI::DirDataObject,class std::allocator<struct WONAPI::DirDataObject> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006f2680
// (void __cdecl std::_Construct(struct WONAPI::DirDataObject *,struct WONAPI::DirDataObject const &))
// TEMPLATE: WON 0x006f2700
// (void __cdecl std::_Construct(struct WONAPI::DirACL *,struct WONAPI::DirACL const &))
