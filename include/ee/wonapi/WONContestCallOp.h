// No original address identified: WON SDK contest op; const buffer aliases expanded
#ifndef EE_WONAPI_WONCONTESTCALLOP_H
#define EE_WONAPI_WONCONTESTCALLOP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"

namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class ContestCallOp : public ServerRequestOp
{
private:
	unsigned short mProcNum;
	ConstSmartPtr<ByteBuffer> mRequestData;
	ConstSmartPtr<ByteBuffer> mReplyData;
	
	void Init();

protected:
	virtual WONStatus GetNextRequest();
	virtual WONStatus CheckResponse();
	virtual void RunHook();

public:
	ContestCallOp(ServerContext *theContestContext);
	ContestCallOp(const IPAddr &theAddr);

	void SetProcNum(unsigned short theNum) { this->mProcNum = theNum; }
	void SetRequestData(const ByteBuffer *theData) { this->mRequestData = theData; }

	unsigned short GetProcNum() const { return this->mProcNum; }
	ConstSmartPtr<ByteBuffer> GetRequestData() const { return this->mRequestData; }
	ConstSmartPtr<ByteBuffer> GetReplyData() const { return this->mReplyData; }
};

typedef SmartPtr<ContestCallOp> ContestCallOpPtr;


}; // namespace WONAPI

#endif
