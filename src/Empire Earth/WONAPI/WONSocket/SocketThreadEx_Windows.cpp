// No original address identified: WON SDK select()-based socket thread (SocketThreadEx)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSocketThreadExWindows.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
SocketThreadEx::SocketThreadEx()
{
	AsyncSocket::StartWinsock();
	this->mSignalSocket = new AsyncSocket(AsyncSocket::UDP);

	// Bind SignalSocket and connect it to itself
	IPAddr anAddr = IPAddr::GetLocalAddr();
	anAddr.SetPort(0); // choose random port
	this->mSignalSocket->Bind(anAddr,false);  // SDK Bind address/reuse default
	anAddr.SetPort(this->mSignalSocket->GetLocalPort());
	this->mSignalSocket->Fn2B0E60(anAddr); // current Connect spelling

	for(int i=0; i<3; i++)
	{
		this->mSocketArray[i].reserve(64);
		this->mOpArray[i].reserve(64);
	}

	this->mSocketArray[0 /* SDK SocketEvent_Read */].push_back(this->mSignalSocket->mSocket /* genuine SDK borrowed descriptor getter */);
	this->mOpArray[0 /* SDK SocketEvent_Read */].push_back(NULL);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
SocketThreadEx::~SocketThreadEx()
{
	this->PurgeOps();
	AsyncSocket::StopWinsock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::PurgeOps()
{
	for(int i=0; i<3; i++)
	{
		OpMap::iterator anItr = this->mOpMap[i].begin();
		while(anItr!=this->mOpMap[i].end())
		{
			SocketOp *anOp = anItr->second;
			if(anOp->mInSocketThread)
			{
				anOp->mInSocketThread = false;
				anOp->Kill(true); // explicit genuine SDK finish default
			}

			++anItr;
		}

		this->mOpMap[i].clear();
		if(i!=0 /* SDK SocketEvent_Read */)
		{
			this->mSocketArray[i].clear();
			this->mOpArray[i].clear();
		}
	}	
	
	// Leave signal socket
	this->mSocketArray[0 /* SDK SocketEvent_Read */].resize(1);
	this->mOpArray[0 /* SDK SocketEvent_Read */].resize(1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::AddSocketOp(SocketOp *theOp)
{
	AutoCrit aCrit(this->mDataCrit);
	SOCKET aSocket = theOp->GetSocket()->mSocket /* genuine SDK borrowed descriptor getter */;
	for(int i=0; i<3; i++)
	{
		if(!theOp->mWaitForEvent[i])
			continue;
		std::pair<OpMap::iterator,bool> aRet;
		aRet = this->mOpMap[i].insert(OpMap::value_type(aSocket,theOp));
		if(!aRet.second) // old op still here
		{
			aRet.first->second->Kill(true); // explicit genuine SDK finish default
			this->RemoveSocketOp(aRet.first->second,i);
			aRet.first->second = theOp;
		}

		this->mOpMap[i][aSocket] = theOp;

		theOp->mEventIndex[i] = this->mSocketArray[i].size();
		this->mSocketArray[i].push_back(aSocket);
		this->mOpArray[i].push_back(theOp);
	}

	theOp->mInSocketThread = true;
	this->Signal();
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::RemoveSocketOp(SocketOp *theOp)
{
	this->RemoveSocketOp(theOp, -1);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::RemoveSocketOp(SocketOp *theOp, int dontErase)
{
	AutoCrit aCrit(this->mDataCrit);
	SmartPtr<SocketOp> anOp = theOp;
	for(int i=0; i<3; i++)
	{
		if(!theOp->mWaitForEvent[i])
			continue;
		int aPos = theOp->mEventIndex[i];
		if(aPos<0)
			continue;
		if(dontErase!=i)
			this->mOpMap[i].erase(this->mSocketArray[i][aPos]);

		int aSize = this->mSocketArray[i].size();

		if(aPos!=aSize-1)
		{
			swap(this->mSocketArray[i][aPos],this->mSocketArray[i][aSize-1]);
			swap(this->mOpArray[i][aPos],this->mOpArray[i][aSize-1]);

			this->mOpArray[i][aPos]->mEventIndex[i] = aPos;
		}

		this->mSocketArray[i].pop_back();
		this->mOpArray[i].pop_back();
		theOp->mEventIndex[i] = -1;
	}

	theOp->mInSocketThread = false;
	this->Signal();
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::Signal()
{
	static char aBuf = 1;
	this->mSignalSocket->Send(&aBuf,1,NULL); // SDK optional sent-length default
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::Pump(DWORD theWaitTime)
{

	AutoCrit aCrit(this->mDataCrit);
	int i,j;

	for(i=0; i<3; i++)
		this->m_fdset[i].Copy(this->mSocketArray[i]);

	aCrit.Leave();
	
	timeval timeoutval;
	timeoutval.tv_sec = theWaitTime/1000;
	timeoutval.tv_usec = (theWaitTime%1000)*1000;

	int aVal = select(0, this->m_fdset[0].mSet, this->m_fdset[1].mSet, this->m_fdset[2].mSet, &timeoutval);

	if(aVal>0)
	{
		aCrit.Enter();
		OpMap::iterator anItr;
		for(i=0; i<3; i++)
		{
			fd_set *aSet = this->m_fdset[i].mSet;
			for(j=0; j<aSet->fd_count; j++)
			{
				anItr = this->mOpMap[i].find(aSet->fd_array[j]);
				if(anItr!=this->mOpMap[i].end())
				{
					if(SocketThread::DoOp(anItr->second))
					{
						this->RemoveSocketOp(anItr->second, i);
						this->mOpMap[i].erase(anItr);
					}
				}
			}
		}
		aCrit.Leave();

		static char aBuf;
		while(this->mSignalSocket->Recv(&aBuf,1,NULL)==WS_Success /* WS_Success */)
		{
		}
	}
	else if(aVal<0)
	{
		aCrit.Enter();
		OpMap::iterator anItr;
		for(i=0; i<3; i++)
		{
			anItr = this->mOpMap[i].begin();
			while(anItr!=this->mOpMap[i].end())
			{
				if(SocketThread::DoOp(anItr->second))
				{
					this->RemoveSocketOp(anItr->second, i);
					this->mOpMap[i].erase(anItr++);
				}
				else
					++anItr;
			}
		}
		aCrit.Leave();
	}
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void SocketThreadEx::ThreadFunc()
{
	while(!this->mStopped)
		this->Pump(1000);
}

