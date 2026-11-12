// GetVersionOp: reads the valid product versions and their filters from the directory server (WON SDK)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified (file grouping inferred)
#include "ee/wonapi/WONGetVersionOp.h"
#include "ee/wonapi/WONStatus.h"

#include "ee/wonapi/WONPatchMsg.h"
using namespace WONAPI;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
std::wstring VersionBaseOp::GetVersionDir()
{
	wstring aPath = L"/" + StringToWString(this->mProduct) + L"/Version";
	if(!this->mExtraConfig.empty())
		aPath += L"/" + StringToWString(this->mExtraConfig);

	return aPath;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool VersionBaseOp::IsFiltered(const std::string &theVersion, const std::string& theVersionData)
{
	// the "pt" line's filters against ours (ee_won patch.rs)
	return PatchIsFiltered(this->mFilterStringSet,theVersionData);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void VersionBaseOp::BuildValidVersionSet(const DirEntity *theEntity)
{
	this->mValidVersionSet.clear();
	this->mFilteredVersionSet.clear();
	this->mHighestValidVersion.erase();

	const DirDataObjectList &aList = theEntity->mDataObjects;
	DirDataObjectList::const_iterator anItr = aList.begin();
	while(anItr!=aList.end())
	{
		const string &aVersion = anItr->mDataType;

		// Does this version match our filter type?
		if ((anItr->mData.get()==NULL) || !this->IsFiltered(aVersion, anItr->mData->data()))
		{
			// Is this a new highest version?
			if(this->mHighestValidVersion.empty() || this->mVersionCompare(aVersion,this->mHighestValidVersion)>0)
				this->mHighestValidVersion = aVersion;

			// Insert the version
			this->mValidVersionSet.insert(aVersion);
			std::string &aStr = this->mVersionDescriptionMap[aVersion];
			if(anItr->mData.get()!=NULL)
				aStr.assign(anItr->mData->data(), anItr->mData->length());
		}
		else	
		{
			// Store the version in the filtered list
			this->mFilteredVersionSet.insert(aVersion);
			std::string &aStr = this->mFilteredVersionDescriptionMap[aVersion];
			if(anItr->mData.get()!=NULL)
				aStr.assign(anItr->mData->data(), anItr->mData->length());
		}

		++anItr;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
VersionResult VersionBaseOp::CheckVersion(const std::string &theVersionId)
{
	VersionSet::iterator anItr = this->mValidVersionSet.find(theVersionId);
	if(anItr!=this->mValidVersionSet.end())
	{
		if(theVersionId==this->mHighestValidVersion)
			return Version_Valid_Latest;
		else
			return Version_Valid_NotLatest;
	}
	else
		return Version_Invalid;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool VersionBaseOp::VersionIsLatest(const std::string &theVersionId)
{
	return this->CheckVersion(theVersionId)==Version_Valid_Latest;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool VersionBaseOp::VersionIsValid(const std::string &theVersionId)
{
	return this->CheckVersion(theVersionId)!=Version_Invalid;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void VersionBaseOp::BuildVersionFilterSet(const std::string &theFilterString, FilterStringSet &theFilterSet)
{
	PatchFilterSet(theFilterString,theFilterSet);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void VersionBaseOp::SetFilterString(const std::string &theFilterString)
{
	// "opt,res"
	std::string aFilterString = StringToLowerCase(theFilterString);

	this->BuildVersionFilterSet(aFilterString, this->mFilterStringSet);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetVersionOp::GetVersionOp(ServerContext *theDirServers, const std::string &theProduct)
{
	this->mDirServers = theDirServers;
	this->mProduct = theProduct;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionOp::DoFinish(RustServerOp *theOp)
{
	if(!theOp->Succeeded())
	{
		this->Finish(theOp->GetStatus());
		return;
	}


	// the original read the first entity of an empty listing too (past the list's end): none is no version here
	const DirEntityList aList = theOp->GetEntities(EE_WON_REQOP_DIR_LIST);
	if(!aList.empty())
		this->BuildValidVersionSet(aList.front());
	this->Finish(WS_Success);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GetVersionOp::CallbackHook(AsyncOp *theOp, int)
{
	this->DoFinish(static_cast<RustServerOp*>(theOp));
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionOp::RunHook()
{
	this->mValidVersionSet.clear();
	this->mFilteredVersionSet.clear();
	this->mHighestValidVersion.erase();


	DWORD aFlags = DIR_GF_DECOMPROOT | DIR_GF_ADDDATAOBJECTS | DIR_GF_ADDDOTYPE | DIR_GF_ADDDODATA;

	SmartPtr<RustServerOp> anOp = RustServerOp::NewGetDir(this->mDirServers, this->GetVersionDir(), aFlags);
	if(this->GetMode()==OP_MODE_ASYNC)
	{
		this->Track(anOp,0);
		anOp->RunAsync(INFINITE);
		return;
	}

	anOp->RunBlock(this->TimeLeft());
	this->DoFinish(anOp);
}


