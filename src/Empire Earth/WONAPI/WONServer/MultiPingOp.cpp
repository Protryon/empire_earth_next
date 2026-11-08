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
using namespace std;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MultiPingOp::MultiPingOp()
{
	this->mBatchSize = 20;
	this->mMaxFailures = 3;
	this->mMaxPingTime = 3000;
	this->mMaxPingReplySize = 2048;
	this->mOrderListByPingTime = true;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::AddServer(const IPAddr &theAddr)
{
	this->mPingList.push_back(new MultiPingStruct(theAddr));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::AddServers(const DirEntityList &theDir, const wchar_t *theNameFilter)
{
	wstring aNameFilter;
	if(theNameFilter!=NULL)
		aNameFilter = theNameFilter;

	DirEntityList::const_iterator anItr = theDir.begin();
	while(anItr!=theDir.end())
	{
		const DirEntity *anEntity = *anItr;
		if(anEntity->mType!='D')
		{
			if(aNameFilter.empty() || aNameFilter==anEntity->mName)
			{
				IPAddr anAddr = anEntity->GetNetAddrAsIP();
				if(anAddr.IsValid())
					this->mPingList.push_back(new MultiPingStruct(anAddr,anEntity));
			}
		}

		++anItr;
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::ClearServers()
{
	this->mPingList.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ByteBufferPtr MultiPingOp::GetRequest(MultiPingStruct *theStruct)
{
	theStruct->mPingId = rand();
	theStruct->mStartPingTick = GetTickCount();

	WriteBuffer aBuf(0,true);
	aBuf.AppendByte(3);
	aBuf.AppendByte(1);
	aBuf.AppendByte(5);
	aBuf.AppendLong(theStruct->mPingId);
	aBuf.AppendBool(false);
	return aBuf.ToByteBuffer(true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MultiPingOp::PingAction MultiPingOp::HandleResponse(MultiPingStruct *theStruct, const ByteBuffer *theResponse)
{
	ReadBuffer aBuf(theResponse->data(),theResponse->length());
	try
	{
		unsigned char aHeader = aBuf.ReadByte();
		unsigned char aService = aBuf.ReadByte();
		unsigned char aMsg = aBuf.ReadByte();

		if(aHeader!=3 || aService!=1 || aMsg!=6)
			return PingAction_InvalidReply;

		int anId = aBuf.ReadLong();
		if(anId!=theStruct->mPingId)
			return PingAction_InvalidReply;

		theStruct->mPingTime = GetTickCount()-theStruct->mStartPingTick;
		return PingAction_Done;
	}
	catch(ReadBufferException&)
	{
		return PingAction_InvalidReply;
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::StaticRecvCallback(AsyncOpPtr theOp, SmartPtr<RefCount> theParam)
{
	MultiPingOp *thisOp = (MultiPingOp*)theParam.get();
	RecvBytesFromOp *aRecvOp = (RecvBytesFromOp*)theOp.get();
	thisOp->RecvCallback(aRecvOp);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::RecvCallback(RecvBytesFromOp *theOp)
{
	if(this->mStatus!=WS_Pending || theOp->GetSocket()!=this->mSocket.get())
		return;

	if(!this->HandleResponsePrv(theOp->GetStatus(),theOp->GetBytes(),theOp->GetAddr()))
		this->Finish(theOp->GetStatus());

	if(this->mPingMap.size()!=0)
		return;
	if(this->mPingList.size()==0)
		this->Finish((WONStatus)WS_Success);
	else
		this->PingBatchAsync();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::StaticTimeoutCallback(AsyncOpPtr theOp, SmartPtr<RefCount> theParam)
{
	MultiPingOp *thisOp = (MultiPingOp*)theParam.get();
	thisOp->TimeoutCallback((AsyncOp*)theOp.get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::TimeoutCallback(AsyncOp *theOp)
{
	if(this->mStatus!=WS_Pending || theOp!=this->mRecvTimeoutOp.get())
		return;

	this->mRecvTimeoutOp = NULL;
	this->FinishBatch();
	this->PingBatchAsync();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::PrepareBatch()
{
	this->mPingMap.clear();

	MultiPingList::iterator anItr = this->mPingList.begin();
	int aCount = 0;

	while(anItr!=this->mPingList.end() && aCount<this->mBatchSize)
	{
		this->mPingMap[(*anItr)->mAddr] = *anItr;
		this->mPingList.erase(anItr++);
		aCount++;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::FinishBatch()
{
	MultiPingMap::iterator anItr = this->mPingMap.begin();
	while(anItr!=this->mPingMap.end())
	{
		MultiPingStruct *aStruct = anItr->second;
		aStruct->mNumFailures++;
		if(aStruct->mNumFailures >= this->mMaxFailures)
			this->mFinishedList.push_back(aStruct);
		else
			this->mPingList.push_back(aStruct);

		++anItr;
	}

	this->mPingMap.clear();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::PingBatchAsync()
{
	if(this->mRecvTimeoutOp.get()!=NULL)
	{
		this->mRecvTimeoutOp->Kill(true);
		this->mRecvTimeoutOp = NULL;
	}

	this->PrepareBatch();
	MultiPingMap::iterator anItr = this->mPingMap.begin();
	while(anItr!=this->mPingMap.end())
	{
		ByteBufferPtr aPing = this->GetRequest(anItr->second);
		this->mSocket->QueueOp(new SendBytesToOp(aPing,anItr->first), 0xffffffff);
		++anItr;
	}

	this->mRecvTimeoutOp = new AsyncOp;
	this->mRecvTimeoutOp->mCompletion = new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(MultiPingOp::StaticTimeoutCallback,this);
	this->mRecvTimeoutOp->RunAsync(this->mMaxPingTime);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DWORD MultiPingOp::ResponseTimeLeft(DWORD theStartTick)
{
	DWORD anElapsed = GetTickCount() - theStartTick;
	if(anElapsed > this->mMaxPingTime)
		return 0;


	DWORD anAbsTimeLeft = this->TimeLeft();
	DWORD aTimeLeft = this->mMaxPingTime - anElapsed;

	if(aTimeLeft < anAbsTimeLeft)
		return aTimeLeft;
	else
		return anAbsTimeLeft;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool MultiPingOp::HandleResponsePrv(WONStatus theStatus, const ByteBuffer *theResponse, const IPAddr &theAddr)
{
	if(theStatus!=(WONStatus)WS_Success && theStatus!=(WONStatus)WS_WSAEMSGSIZE)
		return false;

	if(theStatus==(WONStatus)WS_Success)
	{
		MultiPingMap::iterator anItr = this->mPingMap.find(theAddr);
		if(anItr!=this->mPingMap.end())
		{
			PingAction anAction = this->HandleResponse(anItr->second,theResponse);
			if(anAction==PingAction_Done)
			{
				this->mFinishedList.push_back(anItr->second);
				this->mPingMap.erase(anItr);
			}
			else if(anAction==PingAction_More)
			{
				this->mPingList.push_back(anItr->second);
				this->mPingMap.erase(anItr);
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus MultiPingOp::PingBatchBlock()
{
	this->PrepareBatch();

	MultiPingMap::iterator anItr = this->mPingMap.begin();
	WONStatus aStatus;
	while(this->TimeLeft()>0 && anItr!=this->mPingMap.end())
	{
		ByteBufferPtr aPing = this->GetRequest(anItr->second);
		aStatus = this->mSocket->SendBytesTo(aPing, anItr->first, this->TimeLeft());
		if(aStatus!=(WONStatus)0)
			return aStatus;

		++anItr;
	}

	DWORD aStartTick = GetTickCount();
	while(this->ResponseTimeLeft(aStartTick)>0 && !this->mPingMap.empty())
	{
		ByteBufferPtr aReply;
		IPAddr anAddr;
		aStatus = this->mSocket->RecvBytesFrom(aReply,this->mMaxPingReplySize,anAddr,this->ResponseTimeLeft(aStartTick));
		if(!this->HandleResponsePrv(aStatus,aReply,anAddr))
			return aStatus;

	}

	return aStatus;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::RunHook()
{
	srand(time(NULL));
	this->mSocket = new BlockingSocket(AsyncSocket::UDP);
	this->mSocket->Fn2B0D90(0,false);
	this->mFinishedList.clear();

	if(this->mRunMode==OP_MODE_ASYNC)
	{
		this->mSocket->SetRepeatCompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(MultiPingOp::StaticRecvCallback,this));
		this->mSocket->SetRepeatOp(new RecvBytesFromOp(this->mMaxPingReplySize));
		this->PingBatchAsync();
		return;
	}

	WONStatus aStatus;
	while(this->TimeLeft()>0 && !this->mPingList.empty())
	{
		aStatus = this->PingBatchBlock();
		this->FinishBatch();
		if(aStatus!=(WONStatus)0 && aStatus!=(WONStatus)100001)
		{
			this->Finish(aStatus);
			return; 
		}
	}

	this->Finish((WONStatus)WS_Success);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MultiPingOp::CleanupHook()
{
	if(this->mSocket.get()!=NULL)
	{
		this->mSocket->Close();
		this->mSocket = NULL;
	}

	if(this->mRecvTimeoutOp.get()!=NULL)
	{
		this->mRecvTimeoutOp->Kill(true);
		this->mRecvTimeoutOp = NULL;
	}

	this->FinishBatch();

	MultiPingList::iterator aListItr = this->mPingList.begin();
	while(aListItr!=this->mPingList.end())
	{
		this->mFinishedList.push_back(*aListItr);
		++aListItr;
	}
	this->mPingList.clear();

	if(!this->mOrderListByPingTime)
		return;
	typedef std::multimap<DWORD,MultiPingStructPtr> SortMap;
	SortMap aMap;
	aListItr = this->mFinishedList.begin();
	while(aListItr!=this->mFinishedList.end())
	{
		aMap.insert(SortMap::value_type((*aListItr)->mPingTime,*aListItr));
		++aListItr;
	}

	this->mFinishedList.clear();

	SortMap::iterator aMapItr = aMap.begin();
	while(aMapItr!=aMap.end())
	{
		this->mFinishedList.push_back(aMapItr->second);
		aMapItr++;
	}

}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<ServerContext> MultiPingOp::GetServerContext(bool discardNonResponsiveServers)
{
	SmartPtr<ServerContext> aContext = new ServerContext;
	MultiPingList::iterator anItr = this->mFinishedList.begin();
	while(anItr!=this->mFinishedList.end())
	{
		MultiPingStruct *aStruct = *anItr;
		if(aStruct->mPingTime!=-1 || !discardNonResponsiveServers)
			aContext->AddAddress(aStruct->mAddr);

		++anItr;
	}

	aContext->mShuffleAddresses = false;
	return aContext;
}
