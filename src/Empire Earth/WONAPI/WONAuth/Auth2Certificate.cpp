// Auth2Certificate: version 2 auth certificate (user, key, community trust, user data, nicknames, key ids), unpacked
// by ee_won (rust/crates/ee_won/src/auth.rs); the getters ask it
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
// Destroys the certificate
Auth2Certificate::~Auth2Certificate() {}
// FUNCTION: WON 0x006dda60
// Checks the user id and that the given community (or any community) grants at least the given trust
bool Auth2Certificate::VerifyPermissions(DWORD userId, DWORD community, unsigned short minTrust) const
{
    return this->GetParsed() != NULL && ee_won_auth_verify_permissions(this->GetParsed(), userId, community, minTrust);
}
// FUNCTION: WON 0x006ddb50
// Returns the first community id in the trust map, or 0 when there is none
DWORD Auth2Certificate::GetFirstCommunity() const
{
    return this->GetParsed() != NULL ? ee_won_auth_cert2_query(this->GetParsed(), EeWonCert2Query::FirstCommunity, 0) : 0;
}
// the UTF-16 units of a wide string, appended to the std::basic_string<uint16_t> at inContext
static void AppendUnits(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    static_cast<std::basic_string<uint16_t>*>(inContext)->append(inUnits, inCount);
}
// No original address identified: WON SDK support body
std::wstring Auth2Certificate::GetNickname(const std::wstring &theKey) const
{
	std::wstring nickname;
	if(this->GetParsed()==NULL)
		return nickname;
	std::basic_string<uint16_t> key;
	ee_won_utf16_from_wide(theKey.data(),sizeof(wchar_t),theKey.length(),AppendUnits,&key);
	ee_won_auth_cert2_nickname(this->GetParsed(),false,key.data(),key.length(),WonWideSink,&nickname);
	return nickname;
}
// No original address identified: WON SDK support body
ByteBufferPtr Auth2Certificate::GetUserData(DWORD theCommunityId) const
{
	ByteBufferPtr data;
	if(this->GetParsed()!=NULL)
		ee_won_auth_cert2_user_data(this->GetParsed(),false,theCommunityId,WonByteBufferSink,&data);
	return data;
}
// No original address identified: WON SDK support body
DWORD Auth2Certificate::GetKeyId(DWORD theCommunityId) const
{
	return this->GetParsed()!=NULL ? ee_won_auth_cert2_query(this->GetParsed(),EeWonCert2Query::KeyId,theCommunityId) : 0;
}
// No original address identified: WON SDK support body (looks the first entry's value up as a key, as the SDK did)
std::wstring Auth2Certificate::GetFirstNickname() const
{
	std::wstring nickname;
	if(this->GetParsed()!=NULL)
		ee_won_auth_cert2_nickname(this->GetParsed(),true,NULL,0,WonWideSink,&nickname);
	return nickname;
}
// No original address identified: WON SDK support body
unsigned short Auth2Certificate::GetFirstCommunityTrust() const
{
	return this->GetParsed()!=NULL
	    ? static_cast<unsigned short>(ee_won_auth_cert2_query(this->GetParsed(),EeWonCert2Query::FirstCommunityTrust,0)) : 0;
}
// No original address identified: WON SDK support body
ByteBufferPtr Auth2Certificate::GetFirstUserData() const
{
	ByteBufferPtr data;
	if(this->GetParsed()!=NULL)
		ee_won_auth_cert2_user_data(this->GetParsed(),true,0,WonByteBufferSink,&data);
	return data;
}
}
