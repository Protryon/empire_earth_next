// WONAPI account profile data: newsletters, custom action and birth date (file grouping inferred)
// The profile is a Rust object (ee_won db.rs EeWonProfile); this class is its shim (the drivers' copy).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "won_profiledata.h"
namespace WONAPI {
// FUNCTION: WON 0x006cd770
// Creates an empty profile (no birth date, no custom action)
ProfileData::ProfileData()
    : mProfile(ee_won_profile_new())
{
}
// FUNCTION: WON 0x006cd830
// Destroys the profile, releasing its newsletter list and custom action data
ProfileData::~ProfileData() { ee_won_profile_free(this->mProfile); }
// FUNCTION: WON 0x006cd9e0
// Sets whether the account subscribes to a newsletter
void ProfileData::AddNewsletterSub(unsigned short inId,bool inSubscribe)
{
    ee_won_profile_set_newsletter(this->mProfile,inId,inSubscribe);
}
// FUNCTION: WON 0x006cdae0
// Returns whether the account subscribes to a newsletter (false if unknown)
bool ProfileData::GetNewsletterSub(unsigned short inId)
{
    return ee_won_profile_newsletter(this->mProfile,inId);
}
// FUNCTION: WON 0x006cdc20
// Forgets every newsletter subscription
void ProfileData::ClearNewsletterSubs()
{
    ee_won_profile_clear_newsletter(this->mProfile,0,true);
}
// FUNCTION: WON 0x006cdcc0
// Appends the profile fields to a request (newsletters as set or get, custom action, birth date)
void ProfileData::PackRequestData(WriteBuffer& inMsg,bool inIsSet)
{
    std::string aBytes;
    ee_won_profile_pack(this->mProfile,inIsSet,Append,&aBytes);
    inMsg.AppendBytes(aBytes.data(),(unsigned long)aBytes.size());
}
// FUNCTION: WON 0x006cde20
// Reads newsletter, custom action and birth date fields from a reply; stops quietly when it runs short
void ProfileData::UnpackGetData(ReadBuffer& inMsg)
{
    size_t aPos=ee_won_profile_unpack(this->mProfile,reinterpret_cast<const uint8_t*>(inMsg.data()),
                                      inMsg.length(),(size_t)inMsg.pos());
    inMsg.SetPos((int)aPos);
}
// FUNCTION: WON 0x006ce070
// Sets the birth month, day and year
void ProfileData::SetBirthDate(unsigned char inMonth,unsigned char inDay,unsigned short inYear)
{
    ee_won_profile_set_birth_date(this->mProfile,inMonth,inDay,inYear);
}
void ProfileData::Append(void* inContext,const uint8_t* inBytes,size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes),inLength);
}
}
// No original address identified: WON SDK profile methods
namespace WONAPI {
void ProfileData::ClearNewsletterSub(unsigned short theId)
{
	ee_won_profile_clear_newsletter(this->mProfile,theId,false);
}
void ProfileData::SetCustomAction(unsigned short theActionId, const ByteBuffer *theData)
{
	ee_won_profile_set_custom_action(this->mProfile,theActionId,
	                                 theData!=NULL ? reinterpret_cast<const uint8_t*>(theData->data()) : NULL,
	                                 theData!=NULL ? theData->length() : 0,theData!=NULL);
}
void ProfileData::ClearCustomAction()
{
	ee_won_profile_set_custom_action(this->mProfile,0,NULL,0,false);
}
unsigned long ProfileData::GetBirthDate()
{
	return (unsigned long)ee_won_profile_birth_date(this->mProfile);
}
}
