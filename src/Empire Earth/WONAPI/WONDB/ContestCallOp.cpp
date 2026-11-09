// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK authenticated contest request/reply
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONContestCallOp.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;

void ContestCallOp::Init()
{
	this->mProcNum = 0;
	this->mLengthFieldSize = 2;
	this->mUseAuth2 = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ContestCallOp::ContestCallOp(ServerContext *theContestContext) : ServerRequestOp(theContestContext)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ContestCallOp::ContestCallOp(const IPAddr &theAddr) : ServerRequestOp(theAddr)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ContestCallOp::RunHook()
{
	if(this->mAuthType==AUTH_TYPE_NONE)
	{
		this->Finish((WONStatus)WS_ServerReq_NeedAuthContext);
		return;
	}

	ServerRequestOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ContestCallOp::GetNextRequest()
{
	WriteBuffer aBuf(this->mLengthFieldSize,true);

	aBuf.AppendByte(3);			// mini message
	aBuf.AppendByte(4);		// Contest Service
	aBuf.AppendByte(1);		// ContestDBCall

	aBuf.AppendLong(this->mAuthContext->GetPeerData()->GetCertificate()->GetUserId()); // WONUserSeq
	aBuf.AppendShort(this->mProcNum); // Contest DB Procedure number

	for(int i=0; i<16; i++) // 16 Byte GUID (not needed)
		aBuf.AppendByte(0);

	aBuf.AppendBuffer(this->mRequestData);
	this->mRequest = aBuf.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ContestCallOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned short aServiceType = aMsg.ReadByte();
	unsigned short aMessageType = aMsg.ReadByte();
	if(aHeaderType!=3 || aServiceType!=4 || aMessageType!=2)
		return this->InvalidReplyHeader();

	short aStatus = aMsg.ReadShort();
	this->mReplyData = new ByteBuffer(aMsg.data() + aMsg.pos(), aMsg.Available(),false);

	return (WONStatus)aStatus;
}

