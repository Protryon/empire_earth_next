// No original address identified: WON SDK ping operations
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONPing.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PingOp::PingOp(const IPAddr &theAddr, unsigned char theLengthFieldSize, bool extended) 
	: ServerRequestOp(theAddr), mExtended(extended) 
{
	this->mLengthFieldSize = theLengthFieldSize;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

WONStatus PingOp::GetNextRequest()
{
	WriteBuffer aBuf(this->mLengthFieldSize,true);
	aBuf.AppendByte(3);					// mini message
	aBuf.AppendByte(1);					// common service
	aBuf.AppendByte(5);					// ping message
	aBuf.AppendLong(GetTickCount());	// start tick
	aBuf.AppendBool(this->mExtended);			// extended ping flag
	this->mRequest = aBuf.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

WONStatus PingOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned char aServiceType = aMsg.ReadByte();
	unsigned char aMessageType = aMsg.ReadByte();
	if(aHeaderType!=3 || aServiceType!=1 || aMessageType!=6)
		return this->InvalidReplyHeader();

	// Read startTick and determine lag
	this->mLag = GetTickCount() - aMsg.ReadLong();

	if(aMsg.Available()>0)
	{
		aMsg.ReadString(this->mAppName);
		aMsg.ReadString(this->mLogicalName);
		this->mVersion = aMsg.ReadLong();
		aMsg.ReadString(this->mImage);
		this->mPID = aMsg.ReadLong();
		aMsg.ReadString(this->mPorts);
		this->mIsAuth = aMsg.ReadBool();
		if (aMsg.Available()>0)
		{
			aMsg.ReadString(this->mRegData);
			aMsg.ReadString(this->mServData);
		}
	}

	return (WONStatus)WS_Success;
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void PingOp::Reset()
{
	this->mAppName.erase();
	this->mLogicalName.erase();
	this->mVersion = 0;
	this->mImage.erase();
	this->mPID = 0;
	this->mPorts.erase();
	this->mIsAuth = false;
	this->mRegData.erase();
	this->mServData.erase();
}
