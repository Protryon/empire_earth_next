// WON SDK Windows Winsock2 backends. No original address identified.
#ifndef __WON_SocketThreadWinsock2_H__
#define __WON_SocketThreadWinsock2_H__
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"





namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SocketWaitThread : public Thread
{
public:
	typedef std::set<SmartPtr<SocketOp> > OpList;
	struct SocketData : public RefCount
	{
		SocketWaitThread *mThread;
		HANDLE mEvent;
		OpList mOps;
		int mArrayPos;

		SocketData(SocketWaitThread *theThread);

	protected:
		~SocketData() { }
	};
	typedef SmartPtr<SocketData> SocketDataPtr;

protected:
	HANDLE mWaitArray[MAXIMUM_WAIT_OBJECTS];
	SmartPtr<AsyncSocket> mSocketArray[MAXIMUM_WAIT_OBJECTS];
	int mNumObjects;

	class ReverseLess
	{
	public:
		bool operator()(int a, int b) const { return b<a; }
	};
	typedef std::set<int,ReverseLess> ReverseSet;
	ReverseSet mReleaseSet;

	virtual void ThreadFunc();
	void ReleaseSockets();

public:
	SocketWaitThread(CriticalSection &theCrit);
	virtual ~SocketWaitThread();

	bool AddSocket(AsyncSocket *theSocket);
	void RemoveSocket(AsyncSocket *theSocket);

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SocketThreadWinsock2 : public SocketThread
{
private:
	typedef std::list<SocketWaitThread*> ThreadList;
	ThreadList mThreadList;

private:
	virtual bool NeedThread() { return false; } 

	static void StaticSocketCloseCallback(AsyncSocket *theSocket);

public:
	SocketThreadWinsock2();
	virtual ~SocketThreadWinsock2();
	virtual void PurgeOps();

	virtual void AddSocketOp(SocketOp *theSocketOp);
	virtual void RemoveSocketOp(SocketOp *theSocketOp);
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}; // namespace WONAPI


#endif
