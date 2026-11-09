// No original address identified: WON SDK derived op
#ifndef EE_WONAPI_WONRECVSOMEBYTESOP_H
#define EE_WONAPI_WONRECVSOMEBYTESOP_H


#include "ee/wonapi/WONRouting.h"

namespace WONAPI
{

class RecvSomeBytesOp : public SocketOp
{
protected:
	char *mCurRecv;
	ByteBufferPtr mRecvBytes;
	unsigned short mMaxBytes;
	bool mDoDelay;
	DWORD mStartTime;

	virtual ~RecvSomeBytesOp();

	virtual WONStatus Start();
	virtual WONStatus Continue();
	virtual AsyncOp* Duplicate() { return new RecvSomeBytesOp(this->mMaxBytes,this->mSocket,this->mDoDelay); }

public:
	static DWORD mInitDelay;

public:
	RecvSomeBytesOp(unsigned short theMaxBytes = 1024, AsyncSocket *theSocket = NULL, bool doDelay = false);

	const ByteBuffer* GetBytes() const { return this->mRecvBytes; }

};

}; // namespace WONAPI

#endif