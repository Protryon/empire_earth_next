// WONAPI directory server entity (directory or service record)
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
namespace WONAPI {
// FUNCTION: WON 0x006f0190
// Creates an empty directory entity (no type, times, CRC or addresses)
DirEntity::DirEntity()
{
    this->mType = 0;
    this->mLifespan = 0;
    this->mCreated = 0;
    this->mTouched = 0;
    this->mCRC = 0;
    this->mCreateIP = 0;
    this->mTouchIP = 0;
    this->mCreateId = 0;
    this->mTouchId = 0;
    this->mReqUnique = false;
}
// FUNCTION: WON 0x006f0290
// Destroys the entity, releasing its names, address, data objects and ACLs
DirEntity::~DirEntity()
{
}
// FUNCTION: WON 0x006f0400
// Returns the service's network address as an IP address and port (empty when shorter than 6 bytes)
IPAddr DirEntity::GetNetAddrAsIP() const
{
    IPAddr result;
    if (this->mNetAddr.get() != NULL && this->mNetAddr->length() >= 6)
        result.SetSixByte(this->mNetAddr->data());
    return result;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f0270
// (protected: virtual void * __thiscall WONAPI::DirEntity::`scalar deleting destructor'(unsigned int)    [??_GDirEntity@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1d50
// (public: __thiscall std::list<struct WONAPI::DirDataObject,class std::allocator<struct WONAPI::DirDataObject> >::~list<struct WONAPI::DirDataObject,cla)
// TEMPLATE: WON 0x006f1e00
// (public: class std::list<struct WONAPI::DirDataObject,class std::allocator<struct WONAPI::DirDataObject> >::iterator __thiscall std::list<struct WONAPI)
