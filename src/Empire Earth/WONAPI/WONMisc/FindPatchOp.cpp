// FindPatchOp: finds patches between product versions in the directory server (WON SDK)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified (file grouping inferred)
#include "ee/wonapi/WONFindPatchOp.h"
#include "ee/wonapi/WONLittleEndian.h"
#include "ee/wonapi/WONStatus.h"

using namespace WONAPI;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FindPatchOp::FindPatchOp(ServerContext *theDirServers, const std::string &theProduct)
{
	this->mDirServers = theDirServers;
	this->mProduct = theProduct;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FindPatchOp::FindPatchOp(GetVersionOp *theVersionOp)
{
	this->mDirServers = theVersionOp->GetDirServers();
	this->mProduct = theVersionOp->GetProduct();
	this->mExtraConfig = theVersionOp->GetExtraConfig();
	this->mValidVersionSet = theVersionOp->GetValidVersionSet();
	this->mVersionDescriptionMap = theVersionOp->GetVersionDescriptionMap();
	this->mFilteredVersionSet = theVersionOp->GetFilteredVersionSet();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FindPatchOp::ValidFromVersion(const std::string &theFromVersion, const PatchInfo &theInfo)
{
	if(theInfo.mFromVersion==theFromVersion)
		return true;
	else if(theInfo.mFromVersion.empty() && this->mVersionCompare(theInfo.mToVersion,theFromVersion)>0)
		return true;
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetPatchList(const std::string &theFromVersion, PatchList &aList)
{
	aList.clear();

	PatchList::iterator anItr = this->mPatchList.begin();
	while(anItr!=this->mPatchList.end())
	{
		const PatchInfo &anInfo = *anItr;
		if(this->ValidFromVersion(theFromVersion,anInfo))
			aList.push_back(anInfo);

		++anItr;
	}
	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetValidPatchList(const std::string &theFromVersion, PatchList &aList)
{
	aList.clear();

	PatchList::iterator anItr = this->mPatchList.begin();
	while(anItr!=this->mPatchList.end())
	{
		const PatchInfo &anInfo = *anItr;
		if(this->ValidFromVersion(theFromVersion,anInfo) && this->mValidVersionSet.find(anInfo.mToVersion)!=this->mValidVersionSet.end())
			aList.push_back(anInfo);

		++anItr;
	}
	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetHighestPatchList(const std::string &theFromVersion, const std::string &theToVersion, PatchList &aList) // all patches to the highest version <= theToVersion available from this version
{
	aList.clear();

	PatchList::iterator aPatchItr;

	std::string aHighestVersion;
	bool highestVersionHasFromVersion = false;

	for(aPatchItr = this->mPatchList.begin(); aPatchItr!=this->mPatchList.end(); ++aPatchItr)
	{
		const PatchInfo &anInfo = *aPatchItr;
		if(!this->ValidFromVersion(theFromVersion,anInfo))
			continue;

		if(this->mVersionCompare(anInfo.mToVersion, theToVersion)<=0) // only want versions <= theToVersion
			continue;

		// Don't show filtered versions
		if(this->mFilteredVersionSet.find(anInfo.mToVersion) != this->mFilteredVersionSet.end())
			continue;

		if(aHighestVersion.empty())
		{
			aHighestVersion = anInfo.mToVersion;
			highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
		}

		int aComp = this->mVersionCompare(anInfo.mToVersion, aHighestVersion);
		if(aComp<0) // ToVersion less than highest version found
			continue;
		else if(aComp>0) // ToVersion greater than highest version found
		{
			aHighestVersion = anInfo.mToVersion;
			highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
			aList.clear();
			aList.push_back(anInfo);
		}
		else // aComp = 0
		{
			if(!highestVersionHasFromVersion) // highest current patch is non-specific
			{
				if(!anInfo.mFromVersion.empty()) // specific patch for this version to high version
				{
					highestVersionHasFromVersion = true;
					aList.clear();
					aList.push_back(anInfo);
				}
				else
					aList.push_back(anInfo); // still no from version
			}
			else  // highest patch is specific --> only allow specific patches (no empty from versions)
			{
				if(!anInfo.mFromVersion.empty())
					aList.push_back(anInfo);
			}
		}
	}	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetHighestPatchList(const std::string &theFromVersion, PatchList &aList)
{
	this->GetHighestPatchList(theFromVersion,this->GetHighestValidVersion(),aList);
/*	aList.clear();

	PatchList::iterator aPatchItr;

	std::string aHighestVersion;
	bool highestVersionHasFromVersion = false;

	for(aPatchItr = mPatchList.begin(); aPatchItr!=mPatchList.end(); ++aPatchItr)
	{
		const PatchInfo &anInfo = *aPatchItr;
		if(!ValidFromVersion(theFromVersion,anInfo))
			continue;

		if(aHighestVersion.empty())
		{
			aHighestVersion = anInfo.mToVersion;
			highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
		}

		int aComp = mVersionCompare(anInfo.mToVersion, aHighestVersion);
		if(aComp<0) // ToVersion less than highest version found
			continue;
		else if(aComp>0) // ToVersion greater than highest version found
		{
			aHighestVersion = anInfo.mToVersion;
			highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
			aList.clear();
			aList.push_back(anInfo);
		}
		else // aComp = 0
		{
			if(!highestVersionHasFromVersion) // highest current patch is non-specific
			{
				if(!anInfo.mFromVersion.empty()) // specific patch for this version to high version
				{
					highestVersionHasFromVersion = true;
					aList.clear();
					aList.push_back(anInfo);
				}
				else
					aList.push_back(anInfo); // still no from version
			}
			else  // highest patch is specific --> only allow specific patches (no empty from versions)
			{
				if(!anInfo.mFromVersion.empty())
					aList.push_back(anInfo);
			}
		}
	}*/
	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::DoFinish(GetDirOp *theOp)
{
	if(!theOp->Succeeded())
	{
		this->Finish(theOp->GetStatus());
		return;
	}

	const DirEntityList &aList = theOp->GetDirEntityList();
	DirEntityList::const_iterator anItr = aList.begin();
	while(anItr!=aList.end())
	{
		const DirEntity *anEntity = anItr->get();
		if((anEntity->mType=='D'))
			this->BuildValidVersionSet(anEntity);
		else
		{
			PatchInfo anInfo;
			anInfo.mPatchLocation = anEntity->mNetAddr;

			const std::string &aPatchId = WStringToString(anEntity->mName);
			int aPos = aPatchId.find(L'~');
			if(aPos==wstring::npos)
				anInfo.mToVersion = aPatchId;
			else
			{
				anInfo.mFromVersion = aPatchId.substr(0,aPos);
				anInfo.mToVersion = aPatchId.substr(aPos+1);
			}

			anInfo.mName = anEntity->mDisplayName;
			DirDataObjectList::const_iterator anItr = anEntity->mDataObjects.begin();
			while(anItr!=anEntity->mDataObjects.end())
			{
				DirDataObject anObject = *anItr;
				if(anObject.mDataType=="_cs")
				{
					if(anObject.mData->length()==4)
						anInfo.mChecksum = LongFromLittleEndian(*(unsigned long*)anObject.mData->data());
				}
				else if(anObject.mDataType=="_ps")
				{
					if(anObject.mData->length()==4)
						anInfo.mSize = LongFromLittleEndian(*(unsigned long*)anObject.mData->data());
				}
				else
					anInfo.mData[anObject.mDataType] = anObject.mData;

				++anItr;
			}

			this->mPatchList.push_back(anInfo);
		}

		++anItr;
	}

	this->Finish(WS_Success);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FindPatchOp::CallbackHook(AsyncOp *theOp, int)
{
	this->DoFinish(static_cast<GetDirOp*>(theOp));
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::RunHook()
{
	this->mPatchList.clear();

	SmartPtr<GetDirOp> anOp = new GetDirOp(this->mDirServers);

	anOp->SetPath(this->GetVersionDir());
	DWORD aGetFlags = DIR_GF_DECOMPSERVICES | DIR_GF_SERVADDNAME | DIR_GF_ADDDISPLAYNAME | DIR_GF_SERVADDNETADDR 
		| DIR_GF_ADDDATAOBJECTS | DIR_GF_ADDDOTYPE | DIR_GF_ADDDODATA;

	if(this->mValidVersionSet.empty())
		aGetFlags |= DIR_GF_DECOMPROOT;

	anOp->SetFlags(aGetFlags);

	if(this->mRunMode==OP_MODE_ASYNC)
	{
		this->Track(anOp,0);
		anOp->RunAsync(INFINITE);
		return;
	}

	anOp->RunBlock(this->TimeLeft());
	this->DoFinish(anOp);
}

