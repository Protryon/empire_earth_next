// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// ReportPatchStatusOp: reports a patch download result to the patch server (WON SDK)
// No original address identified (file grouping inferred)
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONPatchProtocol.h"
#include "ee/wonapi/WONStatus.h"

using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ReportPatchStatusOp::ReportPatchStatusOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(19)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ReportPatchStatusOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mSubMessageReplyType != this->mMsgType+1)
		return this->InvalidReplyHeader();
	
	// Do extended unpack

	// Finished
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ReportPatchStatusOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	requestData.AppendString(this->mProductName);
	requestData.AppendString(this->mConfigName);
	requestData.AppendString(this->mFromVersion);
	requestData.AppendString(this->mToVersion);
	requestData.AppendString(this->mNetAddress);		// patch url
	requestData.AppendByte(this->mPatchStatus);

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);

	if(this->mUDPSocket.get()==NULL) // just do TCP ServerRequest
	{
		DBProxyOp::RunHook();
		return;
	}

	// Do UDP server request

	IPAddr anAddr = this->GetAddr();
	if(!anAddr.IsValid())
	{
		this->Finish((WONStatus)WS_ServerReq_NoServersSpecified);
		return;
	}

	this->Reset();
	unsigned char aLengthFieldSize = this->mLengthFieldSize;
	this->mLengthFieldSize=0;
	this->GetNextRequest();
	this->mLengthFieldSize=aLengthFieldSize;
	SmartPtr<SendBytesToOp> anOp = new SendBytesToOp(this->mRequest, anAddr, this->mUDPSocket);
	anOp->Run(this->mRunMode,this->mTimeout);
	this->Finish((WONStatus)WS_Success);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ReportPatchStatusOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mProductName	= "";
	this->mConfigName		= "";
	this->mFromVersion	= "";
	this->mToVersion		= "";
	this->mNetAddress		= "";		// patch url
	this->mPatchStatus	= 0;
}

