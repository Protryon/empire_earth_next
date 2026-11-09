#ifndef EE_WONAPI_WONDIRALIAS_H
#define EE_WONAPI_WONDIRALIAS_H
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GetAliasDirOp : public ServerRequestOp
{
private:
	// List of requests
	typedef std::list<GetEntityRequestPtr> GetEntityRequestList;
	GetEntityRequestList mRequestList;
	GetEntityRequestList::iterator mCurRequest;

	DWORD mFlags;
	DirDataTypeSet mDataTypes;
	std::string mProductName;

	int  mNumFailedRequests;
	void Init();

protected:
	virtual WONStatus GetNextRequest();
	virtual WONStatus CheckResponse();
	virtual void Reset();

public:
	GetAliasDirOp(ServerContext *theDirContext);
	GetAliasDirOp(const IPAddr &theAddr);

	void AddRequest(GetEntityRequest* theRequest);

	void SetProductName(const std::string& theProductName) { this->mProductName = theProductName; }

	// Add path uses flags from SetPath and DataTypes from AddDataType to construct entity request for the path
	void AddService(const std::wstring& theService);
	void SetFlags   (DWORD theFlags) { this->mFlags = theFlags; } 
	DWORD GetFlags()				 { return this->mFlags;	  } 

	// Types of DataObjects to be retrieved
	void AddDataType(const std::string &theType) { this->mDataTypes.insert(theType); } 
	void ClearDataTypes() { this->mDataTypes.clear(); } 
	
	// Access to the request list
	const GetEntityRequestList& GetRequestList() { return this->mRequestList; }
};

typedef SmartPtr<GetAliasDirOp> GetAliasDirOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}
#endif
