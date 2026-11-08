#ifndef EE_WONAPI_WONPING_H
#define EE_WONAPI_WONPING_H
#include <time.h>
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class PingOp : public ServerRequestOp
{
private:
	bool mExtended;

public:
	DWORD mLag;
	std::string mAppName;
	std::string mLogicalName;
	std::string mImage;
	std::string mPorts;
	std::string mRegData;
	std::string mServData;
	time_t mVersion;
	long mPID;
	bool mIsAuth;

protected:
	virtual WONStatus GetNextRequest();
	virtual WONStatus CheckResponse();
	virtual void Reset();

public:
	PingOp(const IPAddr &theAddr, unsigned char theLengthFieldSize, bool extended = false);

	void SetExtended(bool isExtended) { this->mExtended = isExtended; }
	bool GetExtended() { return this->mExtended; }

};

typedef SmartPtr<PingOp> PingOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct MultiPingStruct : public RefCount
{
	IPAddr mAddr;
	int mPingTime;
	int mNumFailures;

	DWORD mStartPingTick;
	int mPingId;

	DirEntityPtr mDirEntity;

	MultiPingStruct(IPAddr theAddr, const DirEntity *theEntity = NULL) : mAddr(theAddr), mDirEntity(const_cast<DirEntity*>(theEntity)), mPingTime(-1), mNumFailures(0) {} 
};
typedef SmartPtr<MultiPingStruct> MultiPingStructPtr;
typedef std::list<MultiPingStructPtr> MultiPingList;
typedef std::map<IPAddr,MultiPingStructPtr> MultiPingMap;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class MultiPingOp : public AsyncOp
{
private:

	MultiPingList mPingList;
	MultiPingMap mPingMap;

	MultiPingList mFinishedList;

	unsigned short mBatchSize;
	unsigned char mMaxFailures;
	DWORD mMaxPingTime;
	unsigned short mMaxPingReplySize;
	bool mOrderListByPingTime;

	SmartPtr<BlockingSocket> mSocket;
	AsyncOpPtr mRecvTimeoutOp;

	void PrepareBatch();
	void PingBatchAsync();
	WONStatus PingBatchBlock();

	void FinishBatch();
	DWORD ResponseTimeLeft(DWORD theStartTick);

	static void StaticRecvCallback(AsyncOpPtr theOp, SmartPtr<RefCount> theParam);
	static void StaticTimeoutCallback(AsyncOpPtr theOp, SmartPtr<RefCount> theParam);
	void RecvCallback(RecvBytesFromOp *theOp);
	void TimeoutCallback(AsyncOp *theOp);

	bool HandleResponsePrv(WONStatus theStatus, const ByteBuffer *theResponse, const IPAddr &theAddr);

protected:
	enum PingAction
	{
		PingAction_InvalidReply,	// --> keep waiting for valid reply
		PingAction_Done,			// --> done pinging this guy
		PingAction_More				// --> ping this guy some more
	};

	virtual ByteBufferPtr GetRequest(MultiPingStruct *theStruct);
	virtual PingAction HandleResponse(MultiPingStruct *theStruct, const ByteBuffer *theResponse);

	virtual void RunHook();
	virtual void CleanupHook();

public:
	MultiPingOp();
	
	void AddServer(const IPAddr &theAddr);
	void AddServers(const DirEntityList &theDir, const wchar_t *theNameFilter = NULL);
	void ClearServers();

	const MultiPingList& GetPingList() const { return this->mFinishedList; }
	SmartPtr<ServerContext> GetServerContext(bool discardNonResponsiveServers = true);

	void SetBatchSize(unsigned short theSize) { this->mBatchSize = theSize; }
	void SetMaxFailures(unsigned char theMaxFailures) { this->mMaxFailures = theMaxFailures; }
	void SetMaxPingTime(DWORD theMilliseconds) { this->mMaxPingTime = theMilliseconds; }
};


typedef SmartPtr<MultiPingOp> MultiPingOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}
#endif
