// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK bounded/delayed byte receive operation
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRecvSomeBytesOp.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
DWORD RecvSomeBytesOp::mInitDelay = 0;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RecvSomeBytesOp::RecvSomeBytesOp(unsigned short theMaxBytes, AsyncSocket *theSocket, bool doDelay) : SocketOp(theSocket)
{
	this->mWaitForEvent[0] = true;
	this->mMaxBytes = theMaxBytes;
	this->mCurRecv = NULL;

	this->mDoDelay = doDelay;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RecvSomeBytesOp::~RecvSomeBytesOp()
{
	delete this->mCurRecv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RecvSomeBytesOp::Start()
{
	if(this->mCurRecv!=NULL)
		delete this->mCurRecv;
	
	this->mCurRecv = new char[this->mMaxBytes];
	this->mStartTime = GetTickCount();

	return this->Continue();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RecvSomeBytesOp::Continue()
{
	this->mSocket->WaitForRead(this->TimeLeft());

	int aRecvLen = 0;
	WONStatus aStatus;

	if (this->mDoDelay && RecvSomeBytesOp::mInitDelay>0 && GetTickCount()-this->mStartTime<RecvSomeBytesOp::mInitDelay)
		return WS_TimedOut /* SDK WS_TimedOut */;
	
	aStatus = this->mSocket->Recv(this->mCurRecv,this->mMaxBytes,&aRecvLen);	
	if(aRecvLen<=0)
		return aStatus;
	this->mRecvBytes = new ByteBuffer(this->mCurRecv,aRecvLen,true);
	this->mCurRecv = NULL;
	aStatus = 0;
	
	return aStatus;
}

}
