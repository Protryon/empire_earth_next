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

static void PingAppend(void* inContext, const uint8_t* inBytes, size_t inLength)
{
	static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

// the request's bytes are Rust's (ee_won server.rs)
WONStatus PingOp::GetNextRequest()
{
	std::string aBytes;
	ee_won_server_ping_request(this->mLengthFieldSize, WONTickCount(), this->mExtended, PingAppend, &aBytes);
	this->mRequest = new ByteBuffer(aBytes.data(), (unsigned long)aBytes.length(), false);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// the reply's read is Rust's (ee_won server.rs): the fields read before a cut are assigned, then the cut throws
WONStatus PingOp::CheckResponse()
{
	const char* aData = this->mResponse->data();
	EeWonPingReply aReply;
	ee_won_server_ping_reply(reinterpret_cast<const uint8_t*>(aData), this->mResponse->length(), WONTickCount(), &aReply);
	if(aReply.end==1)
		return this->InvalidReplyHeader();
	std::string* aTexts[6] = { &this->mAppName, &this->mLogicalName, &this->mImage, &this->mPorts, &this->mRegData,
	                           &this->mServData };
	static const unsigned kTextField[6] = { 2, 3, 5, 7, 9, 10 };
	if(aReply.fields>=1) this->mLag = aReply.lag;
	for(int i=0; i<6; i++)
		if(aReply.fields>=kTextField[i]) aTexts[i]->assign(aData+aReply.text_at[i], aReply.text_len[i]);
	if(aReply.fields>=4) this->mVersion = aReply.version;
	if(aReply.fields>=6) this->mPID = aReply.pid;
	if(aReply.fields>=8) this->mIsAuth = aReply.is_auth;
	if(aReply.end==2)
		throw ReadBufferException("Attempt to read past end of buffer.");
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
