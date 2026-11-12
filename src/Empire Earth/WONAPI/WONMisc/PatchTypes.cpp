// VersionData and PatchData: patch server version and patch records and their wire format
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONPatchMsg.h"
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
    PatchWriteVersion(*this,outBuffer,inFlags);
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
    PatchReadPatch(*this,inBuffer,inGetPatchList);
}
}

// No original address identified: WON SDK version and patch record methods
namespace WONAPI {
void VersionData::AddVersionType(const std::string& theVersionType)
{
	// the comma-separated pieces (ee_won patch.rs)
	PatchAddVersionType(this->mVersionTypeList,theVersionType);
}

void VersionData::ReadFromBuffer(ReadBuffer& theBuffer)
{
	PatchReadVersion(*this,theBuffer);
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
	PatchWritePatch(*this,theWriteBuffer,bUpdateOnly);
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
