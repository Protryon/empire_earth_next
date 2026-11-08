// No original address identified: WON SDK Winsock2 event-based socket thread backend
// (written for the WONSocket flags: -O2 -MD -GR -GX -DEE_DINKUM_SXL233)
#include "ee/wonapi/WONSocketThreadWinsock2.h"
#include <crtdbg.h>
using namespace WONAPI;


static CriticalSection gDataCrit;
static bool gThreadConstructed = false;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SocketWaitThread::SocketWaitThread(CriticalSection &theCrit) 
{
	this->mNumObjects = 1;
#ifndef __WON_SINGLETHREADED__
	this->mWaitArray[0] = this->mSignalEvent.mEvent;
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SocketWaitThread::~SocketWaitThread()
{
	AutoCrit aCrit(gDataCrit);
	for(int i=1; i<this->mNumObjects; i++)
	{
		AsyncSocket *aSocket = this->mSocketArray[i];
		this->RemoveSocket(aSocket);
	}

	this->ReleaseSockets();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketWaitThread::ReleaseSockets(void) 
{
	AutoCrit aCrit(gDataCrit);

	if(this->mReleaseSet.empty())
		return;

	ReverseSet::iterator anItr = this->mReleaseSet.begin();
	int aCount = 0;
	while(anItr!=this->mReleaseSet.end()) 
	{
		int aPos = *anItr;

		CloseHandle(this->mWaitArray[aPos]);
//		mSocketArray[aPos]->SetThreadData(NULL);

		if(aPos!=this->mNumObjects-1)
		{
			// Swap released handled with the last handle (mNumObjects-1)
			AsyncSocket* aSocket = this->mSocketArray[this->mNumObjects-1];
			this->mSocketArray[aPos] = aSocket;

			// Maintain invariant on the non-released handle
			SocketData* aData = (SocketData*)aSocket->mThreadData.get();
			aData->mArrayPos = aPos;
			this->mWaitArray[aPos] = aData->mEvent;
		}

		this->mNumObjects--;
		this->mSocketArray[this->mNumObjects] = NULL;

		++anItr;
		++aCount;
	}

	this->mReleaseSet.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketWaitThread::ThreadFunc()
{
	int aHandleIndex;
	while(true)
	{
		DWORD aWaitResult = WaitForMultipleObjects(this->mNumObjects, this->mWaitArray, false, INFINITE);
		switch(aWaitResult) 
		{
			case WAIT_FAILED:				
				return; 

			case WAIT_TIMEOUT:
				break;

			default:
			{
				AutoCrit aCrit(gDataCrit);

				aHandleIndex = (aWaitResult - WAIT_OBJECT_0);
				if(aHandleIndex==0)  // REHUP
				{
					if(this->mStopped)
						return;
				}
				else
				{
					SmartPtr<AsyncSocket> aSocket = this->mSocketArray[aHandleIndex];
					aCrit.Leave();

					// Make sure to enter the socket critical section first since DoOp will probably enter
					// the socket critical section as well.  When SocketOps run immediately, we may be in
					// the socket's critical section when we try to enter the thread's critical section.
					AutoCrit aSocketCrit(aSocket->mDataCrit);
					aCrit.Enter();
//					SocketThreadWinsock2::Notify(aSocket);
					SocketWaitThread::SocketData* aData = (SocketWaitThread::SocketData *)aSocket->mThreadData.get();
					if(aData==NULL)
						break;

					SocketWaitThread::OpList::iterator anItr = aData->mOps.begin();
					while(anItr!=aData->mOps.end())
					{
						if(SocketThread::DoOp(*anItr))
							aData->mOps.erase(anItr++);
						else
							++anItr;
					}
				}
			}
		}

		this->ReleaseSockets();
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool SocketWaitThread::AddSocket(AsyncSocket *theSocket)
{
	AutoCrit aCrit(gDataCrit);

	if(this->mNumObjects==MAXIMUM_WAIT_OBJECTS)
		return false;

	int aPos = this->mNumObjects;

	SocketDataPtr aData = new SocketData(this);
	aData->mEvent = CreateEvent(NULL,false,false,NULL);
	WSAEventSelect(theSocket->mSocket, aData->mEvent, FD_READ | FD_WRITE | FD_CLOSE | FD_CONNECT | FD_ACCEPT);
	aData->mArrayPos = aPos;
	theSocket->mThreadData = aData.get();

	this->mSocketArray[aPos] = theSocket;
	this->mWaitArray[aPos] = aData->mEvent;
	this->mNumObjects++;

	aCrit.Leave();
	this->mSignalEvent.Set();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketWaitThread::RemoveSocket(AsyncSocket *theSocket)
{
	AutoCrit aCrit(gDataCrit);

	SocketData* aData = (SocketData*)theSocket->mThreadData.get();
	if(aData==NULL)
		return;

	this->mReleaseSet.insert(aData->mArrayPos);
	OpList::iterator anItr = aData->mOps.begin();
	while(anItr!=aData->mOps.end())
	{
		(*anItr)->Kill(true);
		++anItr;
	}
	aData->mOps.clear();
	theSocket->mThreadData = NULL;
	theSocket->mThreadCloseCallback = NULL;

	aCrit.Leave();
	this->mSignalEvent.Set();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SocketWaitThread::SocketData::SocketData(SocketWaitThread *theThread)
{
	this->mThread = theThread;
	this->mEvent = NULL;
	this->mArrayPos = -1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SocketThreadWinsock2::SocketThreadWinsock2()
{
	gThreadConstructed = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SocketThreadWinsock2::~SocketThreadWinsock2()
{
	gThreadConstructed = false;
	this->PurgeOps();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketThreadWinsock2::PurgeOps()
{
	ThreadList::iterator anItr = this->mThreadList.begin();
	while(anItr!=this->mThreadList.end())
	{
		SocketWaitThread *aThread = *anItr;
		aThread->Stop();
		delete aThread;
		++anItr;
	}
	this->mThreadList.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketThreadWinsock2::StaticSocketCloseCallback(AsyncSocket *theSocket)
{
//	if(!gThreadConstructed)
//		return;

	AutoCrit aCrit(gDataCrit);
	SocketWaitThread::SocketData *aData = (SocketWaitThread::SocketData *)theSocket->mThreadData.get();
	if(aData!=NULL)
	{
		SocketWaitThread *aThread = aData->mThread;
		aThread->RemoveSocket(theSocket);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketThreadWinsock2::AddSocketOp(SocketOp *theOp)
{
	AutoCrit aCrit(gDataCrit);

	AsyncSocket* aSocket = theOp->GetSocket();
	SocketWaitThread::SocketData* aData = (SocketWaitThread::SocketData *)aSocket->mThreadData.get();
	bool foundThread = false;
	if(aData==NULL)
	{
		aSocket->mThreadCloseCallback = SocketThreadWinsock2::StaticSocketCloseCallback;
		ThreadList::iterator anItr = this->mThreadList.begin();
		while(anItr!=this->mThreadList.end())
		{
			if((*anItr)->AddSocket(aSocket))
			{
				foundThread = true;
				if(anItr!=this->mThreadList.begin())
				{
					this->mThreadList.push_front(*anItr);
					this->mThreadList.erase(anItr++);
				}
				break;
			}
			else
				++anItr;
		}

		if(!foundThread)
		{
			this->mThreadList.push_front(new SocketWaitThread(gDataCrit));
			this->mThreadList.front()->Start();
			this->mThreadList.front()->AddSocket(aSocket);
		}
	}

	aData = (SocketWaitThread::SocketData *)aSocket->mThreadData.get();
	aData->mOps.insert(theOp);
	theOp->mInSocketThread = true;
	SetEvent(aData->mEvent);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadWinsock2::RemoveSocketOp(SocketOp *theOp)
{
	AutoCrit aCrit(gDataCrit);
	AsyncSocket* aSocket = theOp->GetSocket();
	SocketWaitThread::SocketData* aData = (SocketWaitThread::SocketData *)aSocket->mThreadData.get();
	theOp->mInSocketThread = false;
	if(aData!=NULL)
	{
		aData->mOps.erase(theOp);

//		if(aData->mOps.empty())
//			aData->mThread->RemoveSocket(aSocket);
	}	
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/*
void SocketThreadWinsock2::Notify(AsyncSocket *theSocket)
{
	SocketWaitThread::SocketData* aData = (SocketWaitThread::SocketData *)theSocket->mThreadData.get();
	if(aData==NULL)
		return;

	SocketWaitThread::OpList::iterator anItr = aData->mOps.begin();
	while(anItr!=aData->mOps.end())
	{
		if(DoOp(*anItr))
			aData->mOps.erase(anItr++);
		else
			++anItr;
	}
}

*/