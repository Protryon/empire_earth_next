// VersionData and PatchData: patch server version and patch records and their wire format
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006d83a0
// Creates empty version data in the invalid state
VersionData::VersionData()
    : mConfigName(""),mVersion(""),mDescriptionURL(""),mState(VersionState_Invalid)
{
    this->mVersionTypeList.clear();
}
// FUNCTION: WON 0x006d8510
// Destroys the version data, clearing its version type list
VersionData::~VersionData()
{
    this->mVersionTypeList.clear();
}
// FUNCTION: WON 0x006d8660
// Writes config name, version and version types, plus the description URL (flag 2) and state (flag 1)
void VersionData::WriteToBuffer(WriteBuffer& outBuffer,DWORD inFlags)
{
    outBuffer.AppendString(this->mConfigName);
    outBuffer.AppendString(this->mVersion);
    outBuffer.AppendByte(this->mVersionTypeList.size());
    if (!this->mVersionTypeList.empty()) {
        VersionTypeList::const_iterator i=this->mVersionTypeList.begin();
        while (true) {
            if (i==this->mVersionTypeList.end()) break;
            outBuffer.AppendString(*i);
            ++i;
        }
    }
    if (inFlags&2) outBuffer.AppendString(this->mDescriptionURL);
    if (inFlags&1) outBuffer.AppendByte(this->mState);
}
// FUNCTION: WON 0x006d86f0
// Creates an empty, inactive patch record
PatchData::PatchData()
    : mConfigName(""),mFromVersion(""),mToVersion(""),mDisplayName(""),
      mNetAddress(""),mCheckSum(0),mPatchSize(0),mHostName(""),mHostURL(""),
      mHostText(""),mHostImageURL(""),mMustVisitHost(false),mIsActive(false)
{
}
// FUNCTION: WON 0x006d8a50
// Destroys the patch record (members only)
PatchData::~PatchData()
{
}
// FUNCTION: WON 0x006d8bc0
// Reads a patch record (with config and versions when from a patch list): name, URL, checksum, size, host info
void PatchData::ReadFromBuffer(ReadBuffer& inBuffer,bool inGetPatchList)
{
    if (inGetPatchList) {
        inBuffer.ReadString(this->mConfigName);
        inBuffer.ReadString(this->mFromVersion);
        inBuffer.ReadString(this->mToVersion);
        this->mIsActive=inBuffer.ReadBool();
    }
    inBuffer.ReadString(this->mDisplayName);
    inBuffer.ReadString(this->mNetAddress);
    this->mCheckSum=inBuffer.ReadLong();
    this->mPatchSize=inBuffer.ReadLong();
    inBuffer.ReadString(this->mHostName);
    inBuffer.ReadString(this->mHostURL);
    inBuffer.ReadString(this->mHostText);
    inBuffer.ReadString(this->mHostImageURL);
    this->mMustVisitHost=inBuffer.ReadBool();
}
}

// No original address identified: WON SDK version and patch record methods
namespace WONAPI {
void VersionData::AddVersionType(const std::string& theVersionType)
{ 
	// Parse for any comma seperated strings
	int lastPos = 0;
	int aCommaPos = theVersionType.find(',');
	std::string versionString;

	for (; aCommaPos != -1; aCommaPos = theVersionType.find(',',lastPos))
	{
		versionString = theVersionType.substr(lastPos, aCommaPos-lastPos);
		lastPos = aCommaPos+1;
		this->mVersionTypeList.push_back(versionString);
	}
	
	versionString = theVersionType.substr(lastPos, aCommaPos-lastPos);
	this->mVersionTypeList.push_back(versionString);
}

void VersionData::ReadFromBuffer(ReadBuffer& theBuffer)
{
	theBuffer.ReadString(this->mConfigName);
	theBuffer.ReadString(this->mVersion);
	
	int numVersionTypeStrings = theBuffer.ReadByte();
	std::string versionTypeString;

	for (int k=0; k < numVersionTypeStrings; ++k)
	{
		theBuffer.ReadString(versionTypeString);
		this->AddVersionType(versionTypeString);
	}
	theBuffer.ReadString(this->mDescriptionURL);
	this->mState = (VersionState)theBuffer.ReadByte();
}

bool VersionData::operator==(const VersionData& theVersion) const
{
	if (this->mVersion == theVersion.GetVersion() &&
		this->mConfigName == theVersion.GetConfigName() &&
		this->mDescriptionURL == theVersion.GetDescriptionURL() &&
		this->mState == theVersion.GetState() &&
		this->mVersionTypeList == theVersion.GetVersionTypeList())
	{
		return true;
	}

	return false;
}

bool VersionData::operator!=(const VersionData& theVersion) const
{
	if (this->mVersion		 != theVersion.GetVersion()			||
		this->mConfigName		 != theVersion.GetConfigName()		||
		this->mDescriptionURL  != theVersion.GetDescriptionURL()	||
		this->mState			 != theVersion.GetState()			||
		this->mVersionTypeList != theVersion.GetVersionTypeList()		)
	{
		return true;
	}

	return false;
}

void PatchData::WriteToBuffer(WriteBuffer& theWriteBuffer, bool bUpdateOnly)
{
	theWriteBuffer.AppendString(this->mConfigName);
	theWriteBuffer.AppendString(this->mFromVersion);
	theWriteBuffer.AppendString(this->mToVersion);
	theWriteBuffer.AppendByte(bUpdateOnly);
	theWriteBuffer.AppendString(this->mDisplayName);
	theWriteBuffer.AppendString(this->mNetAddress);		// patch url
	theWriteBuffer.AppendLong(this->mCheckSum);
	theWriteBuffer.AppendLong(this->mPatchSize);
	theWriteBuffer.AppendString(this->mHostName);
	theWriteBuffer.AppendString(this->mHostURL);
	theWriteBuffer.AppendString(this->mHostText);
	theWriteBuffer.AppendString(this->mHostImageURL);
	theWriteBuffer.AppendByte(this->mMustVisitHost);
	theWriteBuffer.AppendByte(this->mIsActive);
}

bool PatchData::operator==(const PatchData& thePatchData) const
{
	if (this->mConfigName		== thePatchData.mConfigName		&&
		this->mFromVersion	== thePatchData.mFromVersion	&&
		this->mToVersion		== thePatchData.mToVersion		&&
		this->mNetAddress		== thePatchData.mNetAddress		&&
		this->mDisplayName	== thePatchData.mDisplayName	&&
		this->mCheckSum		== thePatchData.mCheckSum		&&
		this->mPatchSize		== thePatchData.mPatchSize		&&
		this->mHostName		== thePatchData.mHostName		&&
		this->mHostURL		== thePatchData.mHostURL		&&
		this->mHostText		== thePatchData.mHostText		&&
		this->mHostImageURL	== thePatchData.mHostImageURL	&&
		this->mMustVisitHost	== thePatchData.mMustVisitHost	&&
		this->mIsActive		== thePatchData.mIsActive			)
		return true;
	else
		return false;
}

bool PatchData::operator!=(const PatchData& thePatchData) const
{
	if (this->mConfigName		!= thePatchData.mConfigName		||
		this->mFromVersion	!= thePatchData.mFromVersion	||
		this->mToVersion		!= thePatchData.mToVersion		||
		this->mNetAddress		!= thePatchData.mNetAddress		||
		this->mDisplayName	!= thePatchData.mDisplayName	||
		this->mCheckSum		!= thePatchData.mCheckSum		||
		this->mPatchSize		!= thePatchData.mPatchSize		||
		this->mHostName		!= thePatchData.mHostName		||
		this->mHostURL		!= thePatchData.mHostURL		||
		this->mHostText		!= thePatchData.mHostText		||
		this->mHostImageURL	!= thePatchData.mHostImageURL	||
		this->mMustVisitHost	!= thePatchData.mMustVisitHost	||
		this->mIsActive		!= thePatchData.mIsActive			)
		return true;
	else
		return false;

}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d84f0
// (protected: virtual void * __thiscall WONAPI::VersionData::`scalar deleting destructor'(unsigned int)    [??_GVersionData@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006d8a30
// (protected: virtual void * __thiscall WONAPI::PatchData::`scalar deleting destructor'(unsigned int)    [??_GPatchData@WONAPI@@MAEPAXI@Z])
