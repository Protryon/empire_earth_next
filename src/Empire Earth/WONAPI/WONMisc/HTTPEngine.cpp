// HTTPSession and HTTPConnection: per-host keep-alive connections with request pipelining
// (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <time.h>
#include <list>
#include <vector>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x006d28f0
// Queues an HTTP get on the connection for its host, creating the connection on first use
void HTTPSession::AddOp(HTTPGetOp* inOp)
{
    AutoCrit guard(this->mDataCrit);
    ConnectionMap::iterator anItr = this->mConnectionMap.insert(ConnectionMap::value_type(inOp->mHost, HTTPConnectionPtr())).first;
    HTTPConnectionPtr& stored = anItr->second;
    if (stored.get() == NULL) stored = new HTTPConnection(NULL);
    HTTPConnectionPtr connection = stored.get();
    connection->QueueOp(inOp);
}

// FUNCTION: WON 0x006d2bc0
// Kills every connection's running and queued requests
void HTTPSession::KillAll()
{
    AutoCrit guard(this->mDataCrit);
    ConnectionMap::iterator it=this->mConnectionMap.begin();
    while(it!=this->mConnectionMap.end()) { it->second->Kill();
        ++it;
    }
}

// FUNCTION: WON 0x006d2c80
// Creates an idle connection with pipelining on
HTTPConnection::HTTPConnection(HTTPSession* inSession)
{
    this->mCurOp[1] = NULL;
    this->mCurOp[0] = NULL;
    this->mDoPipeline = true;
    this->mSession = inSession;
    this->mRetryMode = false;
    this->mNumPipelineFailures = 0;
    this->mNumPipelineSuccesses = 0;
}

// FUNCTION: WON 0x006d3600
// Kills the running and queued requests (finishing queued ones as killed) and drops the socket
void HTTPConnection::Kill()
{
    AutoCrit guard(this->mDataCrit);
    this->mRetryMode = false;
    this->mNumPipelineFailures = 0;
    this->mNumPipelineSuccesses = 0;
    this->mDoPipeline = true;
    for (int id=0; id<2; ++id) {
        if (this->mCurOp[id] != NULL) {
            this->mCurOp[id]->Kill(true);
            this->mCurOp[id] = NULL;
            this->mCurOpCompletion[id] = NULL;
        }
        for (OpQueue::iterator it=this->mOpQueue[id].begin(); it!=this->mOpQueue[id].end(); ++it) {
            HTTPGetOp* op = it->get();
            bool wasNotPending = op->GetStatus() != WS_Pending;
            op->Kill(true);
            if (wasNotPending) op->ForceFinish(WS_Killed);
        }
        this->mOpQueue[id].clear();
    }
    if (this->mSocket.get() != NULL) {
        this->mSocket->Kill();
        this->mSocket = NULL;
    }
}

// FUNCTION: WON 0x006d33a0
// Queues a request as send-only on the pipeline queue, or as a full request when pipelining is off
void HTTPConnection::QueueOp(HTTPGetOp* inOp)
{
    AutoCrit guard(this->mDataCrit);
    if (this->mDoPipeline) {
        inOp->mOnlySend = true;
        inOp->mOnlyRecv = false;
        this->QueueOp(inOp,0);
    } else {
        inOp->mOnlySend = false;
        inOp->mOnlyRecv = false;
        this->QueueOp(inOp,1);
    }
}
// FUNCTION: WON 0x006d3290
// Appends a request to the send (0) or receive (1) queue and starts it when the queue was empty
void HTTPConnection::QueueOp(HTTPGetOp* inOp,int inId)
{
    AutoCrit guard(this->mDataCrit);
    this->mOpQueue[inId].push_back(inOp);
    if (this->mOpQueue[inId].size() == 1) this->RunOp(inId);
}


// HTTPConnection is 0x64 bytes, HTTPSession 0x2c bytes

HTTPSession::HTTPSession()
{
}
// FUNCTION: WON 0x006d3430
// Starts the next request of a queue on the shared socket, routing its completion back to the connection
void HTTPConnection::RunOp(int inId)
{
    AutoCrit guard(this->mDataCrit);
    if (this->mRetryMode)
        return;
    if (this->mOpQueue[inId].empty())
        return;
    if (this->mCurOp[inId]!=NULL)
        return;
    SmartPtr<HTTPGetOp> op=this->mOpQueue[inId].front();
    this->mOpQueue[inId].pop_front();
    this->mCurOp[inId]=op.get();
    this->mCurOpCompletion[inId]=this->mCurOp[inId]->GetCompletion();
    this->mCurOp[inId]->SetCompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
        HTTPConnection::StaticOpCallback,SmartPtr<RefCount>(this)));
    this->mCurOp[inId]->SetCloseConnection(false);
    this->mCurOp[inId]->SetDoRangeResume(false);
    this->mCurOp[inId]->SetSocket(this->mSocket.get());
    this->mCurOp[inId]->RunAsync(this->mCurOp[inId]->GetTimeout());
}



// FUNCTION: WON 0x006d2df0
// Completion trampoline: passes a finished request to its connection's OpCallback
void HTTPConnection::StaticOpCallback(AsyncOpPtr inOp,SmartPtr<RefCount> inConnection)
{
    static_cast<HTTPConnection*>(inConnection.get())->OpCallback(
        static_cast<HTTPGetOp*>(inOp.get()));
}

// FUNCTION: WON 0x006d2e80
// Handles a finished request: queues a sent one for its reply or completes it; requeues all when the pipeline drops
void HTTPConnection::OpCallback(HTTPGetOp* inOp)
{
    int anId;
    for (anId=0;anId<2;++anId)
        if (inOp==this->mCurOp[anId])
            break;
    if (anId>=2)
        return;
    this->mCurOp[anId]=NULL;
    OpCompletionBasePtr aCompletion=this->mCurOpCompletion[anId];
    this->mCurOpCompletion[anId]=NULL;
    inOp->SetCompletion(aCompletion);

    if (!this->mRetryMode) {
        this->mSocket=inOp->GetSocket();
        if (anId==1 && this->mSocket.get()!=NULL) {
            if (++this->mNumPipelineSuccesses>2)
                this->mNumPipelineFailures=0;
        }
        if (this->mSocket.get()==NULL && this->mDoPipeline &&
            (!this->mOpQueue[1].empty() || this->mCurOp[1]!=NULL || this->mCurOp[0]!=NULL)) {
            AutoCrit aGuard(this->mDataCrit);
            this->mRetryMode=true;
            if (this->mCurOp[0]!=NULL)
                this->mCurOp[0]->Kill(true);
            if (this->mCurOp[1]!=NULL)
                this->mCurOp[1]->Kill(true);
        }
    }

    if (this->mRetryMode) {
        if (anId!=0 && inOp->GetStatus()==WS_Success) {
            if (aCompletion.get()!=NULL)
                aCompletion->Complete(AsyncOpPtr(inOp));
        }
        else
            this->QueueOp(inOp);

        if (this->mCurOp[0]==NULL && this->mCurOp[1]==NULL) {
            AutoCrit aGuard(this->mDataCrit);
            int aLastQueue=1;
            if (this->mDoPipeline && ++this->mNumPipelineFailures>2) {
                this->mDoPipeline=false;
                aLastQueue=0;
            }
            for (anId=1;anId>=aLastQueue;--anId) {
                OpQueue aQueue=this->mOpQueue[anId];
                this->mOpQueue[anId].clear();
                for (OpQueue::iterator anItr=aQueue.begin();anItr!=aQueue.end();++anItr)
                    this->QueueOp(anItr->get());
            }
            this->mRetryMode=false;
            this->RunOp(this->mDoPipeline?0:1);
        }
        return;
    }
    else {
        if (anId==0 && this->mSocket.get()!=NULL && inOp->GetStatus()==WS_HTTP_SendSuccess) {
            inOp->SetOnlyRecv();
            this->QueueOp(inOp,1);
        }
        else {
            if (aCompletion.get()!=NULL)
                aCompletion->Complete(AsyncOpPtr(inOp));
            this->mLastUseTime=time(NULL);
        }
        this->RunOp(anId);
    }
}

}

// No original address identified: WON SDK request counting
namespace WONAPI {
int HTTPSession::GetNumOps()
{
	AutoCrit aCrit(this->mDataCrit);
	int aNumOps = 0;
	ConnectionMap::iterator anItr = this->mConnectionMap.begin();
	while(anItr!=this->mConnectionMap.end())
	{
		HTTPConnection *aConnection = anItr->second;
		aNumOps += aConnection->GetNumOps();
		++anItr;
	}

	return aNumOps;
}

int HTTPConnection::GetNumOps()
{
	int aNumOps = 0;
	for(int i=0; i<2; i++)
	{
		aNumOps += this->mOpQueue[i].size();
		if(this->mCurOp[i]!=NULL)
			aNumOps++;
	}

	return aNumOps;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2d50
// (public: virtual __thiscall WONAPI::HTTPConnection::~HTTPConnection(void)    [??1HTTPConnection@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2d30
// (public: virtual void * __thiscall WONAPI::HTTPConnection::`scalar deleting destructor'(unsigned int)    [??_GHTTPConnection@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d27b0
// (public: __thiscall WONAPI::HTTPSession::HTTPSession(void)    [??0HTTPSession@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006d2870
// (public: virtual __thiscall WONAPI::HTTPSession::~HTTPSession(void)    [??1HTTPSession@WONAPI@@UAE@XZ])

// TEMPLATE: WON 0x006d2850
// (HTTPSession scalar deleting destructor)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2b30
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dad40
// (public: class std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp> > >::iter)
// TEMPLATE: WON 0x006dc9b0
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>)
// TEMPLATE: WON 0x006d9c10
// (public: __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp> > >:)
// TEMPLATE: WON 0x006daa30
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x006dc680
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x006dc6c0
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGet)
// TEMPLATE: WON 0x006da980
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006dc160
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006dcac0
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
