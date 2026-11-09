// ServerOp: base of the server operations (per-step connect/send/receive time limits)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006acc10
// Creates a server op with 60-second connect, send and receive limits and an 8-byte key size
ServerOp::ServerOp()
{
    this->mMaxConnectTime=60000;
    this->mMaxSendTime=60000;
    this->mMaxRecvTime=60000;
    this->mClientKeySize=8;
    this->mUseClientKey=false;
}
// FUNCTION: WON 0x006ace20
// Sets the connect, send and receive time limits
void ServerOp::SetMaxTimes(int inConnect,int inSend,int inRecv)
{
    this->mMaxConnectTime=inConnect;
    this->mMaxSendTime=inSend;
    this->mMaxRecvTime=inRecv;
}
// FUNCTION: WON 0x006ace50
// Returns the connect time limit, capped by the time left when blocking
DWORD ServerOp::ConnectTime()
{
    DWORD aTime=this->mMaxConnectTime;
    if(this->mRunMode!=OP_MODE_ASYNC && this->TimeLeft()<aTime) aTime=this->TimeLeft();
    return aTime;
}
// FUNCTION: WON 0x006ace80
// Returns the send time limit, capped by the time left when blocking
DWORD ServerOp::SendTime()
{
    DWORD aTime=this->mMaxSendTime;
    if(this->mRunMode!=OP_MODE_ASYNC && this->TimeLeft()<aTime) aTime=this->TimeLeft();
    return aTime;
}
// FUNCTION: WON 0x006aceb0
// Returns the receive time limit, capped by the time left when blocking
DWORD ServerOp::RecvTime()
{
    DWORD aTime=this->mMaxRecvTime;
    if(this->mRunMode!=OP_MODE_ASYNC && this->TimeLeft()<aTime) aTime=this->TimeLeft();
    return aTime;
}
// FUNCTION: WON 0x006acee0
// Copies the time limits of another server op
void ServerOp::CopyMaxTimes(ServerOp* inSource)
{
    this->SetMaxTimes(inSource->mMaxConnectTime,inSource->mMaxSendTime,inSource->mMaxRecvTime);
}
}

// No original address identified: WON SDK methods
namespace WONAPI {
void ServerOp::SendMsgAsync(ByteBufferPtr theMsg, int theTrackId)
{
	SmartPtr<SendMsgOp> anOp = new SendMsgOp(theMsg, this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}

void ServerOp::RecvMsgAsync(int theTrackId)
{
	SmartPtr<RecvMsgOp> anOp = new RecvMsgOp(this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accd0
// (public: virtual void * __thiscall WONAPI::ServerOp::`scalar deleting destructor'(unsigned int)    [??_GServerOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accf0
// (public: virtual __thiscall WONAPI::ServerOp::~ServerOp(void)    [??1ServerOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae9f0
// (public: __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,clas)
