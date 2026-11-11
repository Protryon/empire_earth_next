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
static void MultiPingAppend(void* inContext, const uint8_t* inBytes, size_t inLength)
{
	static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

// the datagram's bytes are Rust's (ee_won server.rs)
ByteBufferPtr MultiPingOp::GetRequest(MultiPingStruct *theStruct)
{
	theStruct->mPingId = rand();
	theStruct->mStartPingTick = WONTickCount();

	std::string aBytes;
	ee_won_server_multi_ping_request(theStruct->mPingId, MultiPingAppend, &aBytes);
	return new ByteBuffer(aBytes.data(), (unsigned long)aBytes.length(), false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MultiPingOp::PingAction MultiPingOp::HandleResponse(MultiPingStruct *theStruct, const ByteBuffer *theResponse)
{
	// the reply's test is Rust's (ee_won server.rs)
	if(!ee_won_server_multi_ping_reply(reinterpret_cast<const uint8_t*>(theResponse->data()),theResponse->length(),
	                                   theStruct->mPingId))
		return PingAction_InvalidReply;

	theStruct->mPingTime = WONTickCount()-theStruct->mStartPingTick;
	return PingAction_Done;

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

	int aNext = ee_won_server_multi_ping_next(this->mPingMap.size(),this->mPingList.size());
	if(aNext==1)
		this->Finish((WONStatus)WS_Success);
	else if(aNext==2)
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
		if(ee_won_server_multi_ping_gives_up(aStruct->mNumFailures,this->mMaxFailures))
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
	// Rust's (ee_won server.rs); the op's time left is read only when the batch's wait is not over, as before
	DWORD aNow = WONTickCount();
	if(ee_won_server_multi_ping_time_left(aNow,theStartTick,this->mMaxPingTime,0xffffffff)==0)
		return 0;
	return ee_won_server_multi_ping_time_left(aNow,theStartTick,this->mMaxPingTime,this->TimeLeft());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool MultiPingOp::HandleResponsePrv(WONStatus theStatus, const ByteBuffer *theResponse, const IPAddr &theAddr)
{
	int aRule = ee_won_server_multi_ping_recv_status(theStatus);
	if(aRule==0)
		return false;

	if(aRule==2)
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

	DWORD aStartTick = WONTickCount();
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
		if(!ee_won_server_multi_ping_continues(aStatus))
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
	// the order is Rust's (ee_won server.rs): by ping time as a DWORD, equal times in list order
	std::vector<MultiPingStructPtr> aStructs(this->mFinishedList.begin(),this->mFinishedList.end());
	std::vector<int> aTimes;
	for(size_t i=0; i<aStructs.size(); i++)
		aTimes.push_back(aStructs[i]->mPingTime);
	std::vector<size_t> anOrder(aStructs.size());
	if(!aStructs.empty())
		ee_won_server_multi_ping_order(&aTimes[0],aTimes.size(),&anOrder[0]);

	this->mFinishedList.clear();
	for(size_t i=0; i<anOrder.size(); i++)
		this->mFinishedList.push_back(aStructs[anOrder[i]]);

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
		if(ee_won_server_multi_ping_keeps(aStruct->mPingTime,discardNonResponsiveServers))
			aContext->AddAddress(aStruct->mAddr);

		++anItr;
	}

	aContext->mShuffleAddresses = false;
	return aContext;
}
