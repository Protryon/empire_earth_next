// Auth2Certificate: version 2 auth certificate (user, key, community trust, user data, nicknames, key ids)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthCertificate.h"
namespace WONAPI {
// FUNCTION: WON 0x006dd1d0
// Destroys the certificate, releasing its key and maps
Auth2Certificate::~Auth2Certificate() {}
// FUNCTION: WON 0x006dd6b0
// Reads the certificate's typed fields (user and key, community user data, nicknames, key ids); false on a bad key
bool Auth2Certificate::UnpackHook(ReadBuffer& input)
{
    if (!AuthBase::UnpackHook(input))
        return false;
    unsigned short count = input.ReadShort();
    for (int i=0; i<count; ++i) {
        unsigned short type = input.ReadShort();
        unsigned short length = input.ReadShort();
        int start = input.pos();
        switch (type) {
        case 0: {
            this->mUserId = input.ReadLong();
            unsigned short keyLength = input.ReadShort();
            if (!this->mPubKey.SetPublicKey(input.ReadBytes(keyLength), keyLength))
                return false;
            input.ReadWString(this->mUserName, 2);
            unsigned char communities = input.ReadByte();
            for (int j=0; j<communities; ++j) {
                DWORD id = input.ReadLong();
                unsigned short& trust = this->mCommunityTrustMap[id];
                trust = input.ReadShort();
            }
            break;
        }
        case 1: {
            DWORD id = input.ReadLong();
            this->mUserDataMap[id] = input.ReadBuf(2);
            break;
        }
        case 2: {
            std::wstring key, value;
            input.ReadWString(key, 2);
            input.ReadWString(value, 2);
            this->mNicknameMap[key] = value;
            break;
        }
        case 3: {
            DWORD id = input.ReadLong();
            DWORD keyId = input.ReadLong();
            this->mKeyIdMap[id] = keyId;
            break;
        }
        }
        input.ReadBytes(length - (input.pos() - start));
    }
    return true;
}
// FUNCTION: WON 0x006dda60
// Checks the user id and that the given community (or any community) grants at least the given trust
bool Auth2Certificate::VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
{
    if (userId != 0 && this->mUserId != userId)
        return false;
    if (community != 0) {
        CommunityTrustMap::const_iterator found = this->mCommunityTrustMap.find(community);
        if (found == this->mCommunityTrustMap.end())
            return false;
        return found->second >= minTrust;
    }
    CommunityTrustMap::const_iterator next = this->mCommunityTrustMap.begin();
    while (next != this->mCommunityTrustMap.end()) {
        if (next->second >= minTrust)
            return true;
        ++next;
    }
    return false;
}
// FUNCTION: WON 0x006ddb50
// Returns the first community id in the trust map, or 0 when there is none
DWORD Auth2Certificate::GetFirstCommunity() const
{
    if(this->mCommunityTrustMap.empty()) return 0;
    return this->mCommunityTrustMap.begin()->first;
}
}

namespace WONAPI {

// No original address identified: WON SDK support body
std::wstring Auth2Certificate::GetNickname(const std::wstring &theKey) const
{
	NicknameMap::const_iterator anItr = this->mNicknameMap.find(theKey);
	if(anItr==this->mNicknameMap.end())
		return L"";
	else
		return anItr->second;
}

// No original address identified: WON SDK support body
ByteBufferPtr Auth2Certificate::GetUserData(DWORD theCommunityId) const
{
	UserDataMap::const_iterator anItr = this->mUserDataMap.find(theCommunityId);
	if(anItr==this->mUserDataMap.end())
		return NULL;
	else
		return anItr->second;
}

// No original address identified: WON SDK support body
DWORD Auth2Certificate::GetKeyId(DWORD theCommunityId) const
{
	KeyIdMap::const_iterator anItr = this->mKeyIdMap.find(theCommunityId);
	if(anItr==this->mKeyIdMap.end())
		return 0;
	else
		return anItr->second;
}

// No original address identified: WON SDK support body
std::wstring Auth2Certificate::GetFirstNickname() const
{
	if (this->mNicknameMap.empty())
		return L"";
	else
		return this->GetNickname(this->mNicknameMap.begin()->second);
}

// No original address identified: WON SDK support body
unsigned short Auth2Certificate::GetFirstCommunityTrust() const
{
	if (this->mCommunityTrustMap.empty())
		return 0;
	else
		return (this->mCommunityTrustMap.begin()->second);
}

// No original address identified: WON SDK support body
ByteBufferPtr Auth2Certificate::GetFirstUserData() const
{
	if (this->mUserDataMap.empty())
		return NULL;
	else
		return (this->mUserDataMap.begin()->second);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dd520
// (protected: virtual void * __thiscall WONAPI::Auth2Certificate::`scalar deleting destructor'(unsigned int)    [??_GAuth2Certificate@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dd540
// (public: __thiscall std::map<unsigned long,unsigned short,struct std::less<unsigned long>,class std::allocator<struct std::pair<unsigned long const ,un)
// TEMPLATE: WON 0x006dd5f0
// (public: __thiscall std::map<unsigned long,class WONAPI::SmartPtr<class WONAPI::ByteBuffer>,struct std::less<unsigned long>,class std::allocator<struct)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dd3d0
// (public: virtual __thiscall WONAPI::AuthCertificateBase::~AuthCertificateBase(void)    [??1AuthCertificateBase@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ca5a0
// (public: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,)
// TEMPLATE: WON 0x006cb600
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,struct std)
// TEMPLATE: WON 0x006cbab0
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned sho)
// TEMPLATE: WON 0x006dad30
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006db900
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struct std::_Mapk<unsi)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ea130
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006eaf60
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006ecfa0
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struct std::_Mapk<unsi)
// TEMPLATE: WON 0x006ef830
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ebf40
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,struct std)
// TEMPLATE: WON 0x006ef940
// (public: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9760
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006eaa30
// (public: struct std::pair<class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned)
// TEMPLATE: WON 0x006eab20
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,struct std)
// TEMPLATE: WON 0x006eac20
// (public: struct std::pair<class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned)
// TEMPLATE: WON 0x006ead20
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006eafb0
// (public: struct std::pair<class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struc)
// TEMPLATE: WON 0x006eb0a0
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struct std::_Mapk<unsi)
// TEMPLATE: WON 0x006ec3f0
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned sho)
// TEMPLATE: WON 0x006ec430
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,struct )
// TEMPLATE: WON 0x006ec730
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006ecc60
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ecca0
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)
// TEMPLATE: WON 0x006ed490
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struct st)
// TEMPLATE: WON 0x006ed4d0
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::ByteBuffer> >,struct std::_Mapk<u)
// TEMPLATE: WON 0x006efe10
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsi)
// TEMPLATE: WON 0x006f0070
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006eadd0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0076f3c0
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned short>,struct std::_Mapk<unsigned long,unsigned short,struct )
