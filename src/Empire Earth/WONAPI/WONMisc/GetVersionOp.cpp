// GetVersionOp: reads the valid product versions and their filters from the directory server (WON SDK)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified (file grouping inferred)
#include "ee/wonapi/WONGetVersionOp.h"
#include "ee/wonapi/WONStatus.h"

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
	FilterStringSet aFilterSet;

	// Is the data empty?
	if (theVersionData.empty())
		return false;
	
	// Find the data object data value for this version
	std::string versionDescription = StringToLowerCase(theVersionData);

	// Search the description for the "pt" key

	unsigned int aLineStart = 0;
	unsigned int aSpacePos = 0;
	std::string aKey = "";
	std::string aKeyValue = "";

	while (aLineStart != -1 && aKey != "pt")
	{
		aSpacePos = versionDescription.find(' ',aLineStart);
		aKey = versionDescription.substr(aLineStart, aSpacePos-aLineStart);

		aLineStart = versionDescription.find('\n',aSpacePos);
		if (aLineStart != -1)
			++aLineStart;
		else
		{
			aLineStart = versionDescription.find('\r',aSpacePos);
			if (aLineStart != -1)
				++aLineStart;
		}
	}

	if (aKey == "pt")
	{
		if (aLineStart != -1)
		{
			aKeyValue = versionDescription.substr(aSpacePos+1, aLineStart-(aSpacePos+1));

			// Depending upon how the info was entered, we might have '\n' or '\r\n' line breaks,
			// so assume the worst and remove both as needed.
			unsigned int aRemove = aKeyValue.find('\n');
			if (aRemove != std::string::npos)
				aKeyValue = aKeyValue.erase(aRemove, 1);

			aRemove = aKeyValue.find('\r');
			if (aRemove != std::string::npos)
				aKeyValue = aKeyValue.erase(aRemove, 1);
		}
		else
			aKeyValue = versionDescription.substr(aSpacePos+1, versionDescription.size()-(aSpacePos+1));
	}
	else
		// pt not found, do not filter
		return false;

	// Use the key value to build a filter set
	this->BuildVersionFilterSet(aKeyValue, aFilterSet);
	
	// Is aFilter a subset of mFilterStringList?
	FilterStringSet::const_iterator anItr = aFilterSet.begin();
	for (; anItr != aFilterSet.end(); ++anItr)
	{
		// Is this token also in our filter list? (Skip empty keys)
		if (*anItr == "")
			continue;

		if (this->mFilterStringSet.find( *anItr ) == this->mFilterStringSet.end())
			return true;
	}

	return false;
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
	// Parse each token and append to the list
	unsigned int lastPos = 0;
	unsigned int commaPos = theFilterString.find(',');

	theFilterSet.clear();
	if (theFilterString.length() == 0)
		return;

	while (commaPos != -1)
	{
		theFilterSet.insert(theFilterString.substr(lastPos, commaPos-lastPos));
		lastPos = commaPos+1;
		commaPos = theFilterString.find(',', lastPos);
	}
	
	theFilterSet.insert(theFilterString.substr(lastPos, theFilterString.size()-lastPos));
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
void GetVersionOp::DoFinish(GetDirOp *theOp)
{
	if(!theOp->Succeeded())
	{
		this->Finish(theOp->GetStatus());
		return;
	}


	DirEntityPtr anEntity = *(theOp->GetDirEntityList().begin());
	this->BuildValidVersionSet(anEntity);
	this->Finish(WS_Success);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GetVersionOp::CallbackHook(AsyncOp *theOp, int)
{
	this->DoFinish(static_cast<GetDirOp*>(theOp));
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionOp::RunHook()
{
	this->mValidVersionSet.clear();
	this->mFilteredVersionSet.clear();
	this->mHighestValidVersion.erase();

	SmartPtr<GetDirOp> anOp = new GetDirOp(this->mDirServers);

	anOp->SetPath(this->GetVersionDir());

	DWORD aFlags = DIR_GF_DECOMPROOT | DIR_GF_ADDDATAOBJECTS | DIR_GF_ADDDOTYPE | DIR_GF_ADDDODATA;

	anOp->SetFlags(aFlags);
	if(this->mRunMode==OP_MODE_ASYNC)
	{
		this->Track(anOp,0);
		anOp->RunAsync(INFINITE);
		return;
	}

	anOp->RunBlock(this->TimeLeft());
	this->DoFinish(anOp);
}


