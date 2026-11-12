// FindPatchOp: finds patches between product versions in the directory server (WON SDK); the patch choice, the
// patch ids and their data are ee_won's (rust/crates/ee_won/src/miscops.rs, through WONMiscOpsMsg.h)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified (file grouping inferred)
#include "ee/wonapi/WONFindPatchOp.h"
#include "ee/wonapi/WONLittleEndian.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONMiscOpsMsg.h"

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


// the lists (and ValidFromVersion, their test) are ee_won's (rust/crates/ee_won/src/miscops.rs), over this
// op's compare and sets
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetPatchList(const std::string &theFromVersion, PatchList &aList)
{
	FindPatchMsg<FindPatchOp>::List(*this, FindPatchMsg<FindPatchOp>::List_All, theFromVersion, std::string(), aList);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetValidPatchList(const std::string &theFromVersion, PatchList &aList)
{
	FindPatchMsg<FindPatchOp>::List(*this, FindPatchMsg<FindPatchOp>::List_Valid, theFromVersion, std::string(), aList);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetHighestPatchList(const std::string &theFromVersion, const std::string &theToVersion, PatchList &aList) // all patches to the highest version past theToVersion available from this version
{
	FindPatchMsg<FindPatchOp>::List(*this, FindPatchMsg<FindPatchOp>::List_Highest, theFromVersion, theToVersion, aList);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindPatchOp::GetHighestPatchList(const std::string &theFromVersion, PatchList &aList)
{
	this->GetHighestPatchList(theFromVersion,this->GetHighestValidVersion(),aList);
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
			FindPatchMsg<FindPatchOp>::SplitId(anInfo, aPatchId);

			anInfo.mName = anEntity->mDisplayName;
			DirDataObjectList::const_iterator anItr = anEntity->mDataObjects.begin();
			while(anItr!=anEntity->mDataObjects.end())
			{
				DirDataObject anObject = *anItr;
				if(anObject.mDataType=="_cs")
					FindPatchMsg<FindPatchOp>::Le32(anInfo.mChecksum, anObject.mData->data(), anObject.mData->length());
				else if(anObject.mDataType=="_ps")
					FindPatchMsg<FindPatchOp>::Le32(anInfo.mSize, anObject.mData->data(), anObject.mData->length());
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

	if(this->GetMode()==OP_MODE_ASYNC)
	{
		this->Track(anOp,0);
		anOp->RunAsync(INFINITE);
		return;
	}

	anOp->RunBlock(this->TimeLeft());
	this->DoFinish(anOp);
}

