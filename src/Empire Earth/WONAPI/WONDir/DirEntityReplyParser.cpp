// The directory server's entity replies: the reading is ee_won's (rust/crates/ee_won/src/dir.rs), the ops that read
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

// the read fields onto a new DirEntity (the others keep the constructor's values). The ACL values: the
// multi-entity reader's locals were long / short, the single-entity reader's unsigned, the same bits in the members.
static DirEntityPtr DirMakeEntity(const EeWonDirEntity* inEntity)
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
        long user = aRead.user;
        long comm = aRead.community;
        short trust = aRead.trust;
        acl.mUser = user; acl.mCommunity = comm; acl.mTrust = trust;
        anEntity->mACLs.push_back(acl);
    }
    return anEntity;
}

// No original address identified: the parsers' entity sink, for the Rust ops' results
// Appends a DirEntity made of what ee_won read to a DirEntityList
void DirEntityReplyParser::TakeEntity(void* inList, const EeWonDirEntity* inEntity)
{
    static_cast<DirEntityList*>(inList)->push_back(DirMakeEntity(inEntity));
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1f10
// (protected: void __thiscall std::list<struct WONAPI::DirDataObject,class std::allocator<struct WONAPI::DirDataObject> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006f2680
// (void __cdecl std::_Construct(struct WONAPI::DirDataObject *,struct WONAPI::DirDataObject const &))
// TEMPLATE: WON 0x006f2700
// (void __cdecl std::_Construct(struct WONAPI::DirACL *,struct WONAPI::DirACL const &))
