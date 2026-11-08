// WONAPI account profile data: newsletters, custom action and birth date (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006cd770
// Creates an empty profile (no birth date, no custom action)
ProfileData::ProfileData()
    : mBirthMonth(0),mBirthDay(0),mBirthYear(0),mCustomActionId(0)
{
}

// FUNCTION: WON 0x006cd830
// Destroys the profile, releasing its newsletter list and custom action data
ProfileData::~ProfileData() {}

// FUNCTION: WON 0x006cd9e0
// Sets whether the account subscribes to a newsletter
void ProfileData::AddNewsletterSub(unsigned short inId,bool inSubscribe)
{
    this->mNewsletterSubs[inId]=inSubscribe;
}

// FUNCTION: WON 0x006cdae0
// Returns whether the account subscribes to a newsletter (false if unknown)
bool ProfileData::GetNewsletterSub(unsigned short inId)
{
    if (this->mNewsletterSubs.find(inId)==this->mNewsletterSubs.end()) return false;
    return this->mNewsletterSubs[inId];
}

// FUNCTION: WON 0x006cdc20
// Forgets every newsletter subscription
void ProfileData::ClearNewsletterSubs()
{
    this->mNewsletterSubs.clear();
}

// FUNCTION: WON 0x006cdcc0
// Appends the profile fields to a request (newsletters as set or get, custom action, birth date)
void ProfileData::PackRequestData(WriteBuffer& inMsg,bool inIsSet)
{
    unsigned short fieldCount=0;
    unsigned long fieldCountPos=inMsg.mLength;
    inMsg.AppendShort(0);
    std::map<unsigned short,bool>::iterator it=this->mNewsletterSubs.begin();
    while (it!=this->mNewsletterSubs.end()) {
        inMsg.AppendShort(1);
        inMsg.AppendShort(inIsSet ? 3 : 2);
        inMsg.AppendShort(it->first);
        if (inIsSet) inMsg.AppendBool(it->second);
        ++fieldCount;
        ++it;
    }
    if (this->mCustomActionId!=0) {
        inMsg.AppendShort(2);
        unsigned long dataLen=0;
        if (this->mCustomActionData.get()!=NULL) dataLen=this->mCustomActionData->length();
        inMsg.AppendShort(dataLen+2);
        inMsg.AppendShort(this->mCustomActionId);
        inMsg.AppendBuffer(this->mCustomActionData.get(),0);
        ++fieldCount;
    }
    inMsg.AppendShort(3);
    inMsg.AppendShort(4);
    inMsg.AppendByte(this->mBirthMonth);
    inMsg.AppendByte(this->mBirthDay);
    inMsg.AppendShort(this->mBirthYear);
    ++fieldCount;
    inMsg.SetShort(fieldCountPos,fieldCount);
}

// FUNCTION: WON 0x006cde20
// Reads newsletter, custom action and birth date fields from a reply; stops quietly when it runs short
void ProfileData::UnpackGetData(ReadBuffer& inMsg)
{
    try {
        short fieldCount=inMsg.ReadShort();
        for (int i=0;i<static_cast<unsigned short>(fieldCount);++i) {
            unsigned short field=static_cast<unsigned short>(inMsg.ReadShort());
            unsigned short length=static_cast<unsigned short>(inMsg.ReadShort());
            switch (field) {
            case 1: {
                unsigned short id=static_cast<unsigned short>(inMsg.ReadShort());
                bool& subscription=this->mNewsletterSubs[id];
                subscription=inMsg.ReadBool();
                break;
            }
            case 2:
                inMsg.ReadShort(); // the original discards the action id; mCustomActionId stays unchanged
                this->mCustomActionData=new ByteBuffer(inMsg.ReadBytes(static_cast<unsigned long>(length)-2),static_cast<unsigned long>(length)-2,false);
                break;
            case 3:
                this->mBirthMonth=inMsg.ReadByte();
                this->mBirthDay=inMsg.ReadByte();
                this->mBirthYear=inMsg.ReadShort();
                break;
            default:
                break; // the original ignores an unknown field without skipping its payload
            }
        }
    }
    catch (ReadBufferException&) {
    }
}

// FUNCTION: WON 0x006ce070
// Sets the birth month, day and year
void ProfileData::SetBirthDate(unsigned char inMonth,unsigned char inDay,unsigned short inYear)
{
    this->mBirthMonth=inMonth;
    this->mBirthDay=inDay;
    this->mBirthYear=inYear;
}
}

// No original address identified: WON SDK profile methods
namespace WONAPI {
void ProfileData::ClearNewsletterSub(unsigned short theId)
{
	this->mNewsletterSubs.erase(theId);
}

void ProfileData::SetCustomAction(unsigned short theActionId, const ByteBuffer *theData)
{
	this->mCustomActionId = theActionId;
	this->mCustomActionData = theData;
}

void ProfileData::ClearCustomAction()
{
	this->mCustomActionId = 0;
	this->mCustomActionData = NULL;
}

unsigned long ProfileData::GetBirthDate()
{
	return (this->mBirthMonth) | (this->mBirthDay<<8) | (this->mBirthYear<<16);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cd810
// (protected: virtual void * __thiscall WONAPI::ProfileData::`scalar deleting destructor'(unsigned int)    [??_GProfileData@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006cd930
// (public: __thiscall std::map<unsigned short,bool,struct std::less<unsigned short>,class std::allocator<struct std::pair<unsigned short const ,bool> > >)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ceda0
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,bool>,struct std::_Mapk<unsigned short,bool,struct std::less<unsigned s)
// TEMPLATE: WON 0x006cf250
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,bool>,struct std::_Mapk<unsigned short,bool,struct std::le)
// TEMPLATE: WON 0x006cf590
// (protected: struct std::_Tree<unsigned short,struct std::pair<unsigned short const ,bool>,struct std::_Mapk<unsigned short,bool,struct std::less<unsign)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cf290
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,bool>,struct std::_Mapk<unsigned short,bool,struct std::less<unsigne)
