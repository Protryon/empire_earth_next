// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK patch/version DBProxy operation
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONPatchAdmin.h"
#include "ee/wonapi/WONStatus.h"


using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetVersionListOp::GetVersionListOp(const std::string& theProductName, ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(9),
	  mProductName(theProductName),
	  mStateFilter(VersionState_BadState)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetVersionListOp::GetVersionListOp(const std::string& theProductName, const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(9),
	  mProductName(theProductName)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Exceptions handled by ServerRequestOp
WONStatus GetVersionListOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mSubMessageReplyType != this->mMsgType+1)
		return this->InvalidReplyHeader();
	
	// Do extended unpack
	ReadBuffer aReadBuf(this->mReplyData->data(), this->mReplyData->length());
	unsigned short numVersions = aReadBuf.ReadShort();

	// Read in each version
	for (int i=0; i < numVersions; i++)
	{
		SmartPtr<VersionData> versionData = new VersionData;

		unsigned short versionDataSize = aReadBuf.ReadShort(); // record size for expandability

		versionData->ReadFromBuffer(ReadBuffer(aReadBuf.ReadBytes(versionDataSize), versionDataSize));
		this->mVersionDataList.push_back(versionData);
	}

	// Finished
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionListOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	requestData.AppendString(this->mProductName);
	requestData.AppendString(this->mConfigName);
	requestData.AppendByte(this->mStateFilter);
	

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionListOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mConfigName = "%";	// % means get every config
	this->mStateFilter = VersionState_BadState;

}
