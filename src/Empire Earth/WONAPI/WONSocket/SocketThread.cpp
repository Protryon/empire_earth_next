// WONAPI SocketThread and SocketThreadSimple: the thread that drives pending async socket ops
// (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006b60d0
// Creates a socket thread that runs on its own thread
SocketThread::SocketThread() { this->mSingleThreaded = false; }
// FUNCTION: WON 0x006b6120
// Destroys the socket thread (members only)
SocketThread::~SocketThread() {}
// FUNCTION: WON 0x006b6140
// Pumps the socket ops with a 1-second wait until the thread is stopped
void SocketThread::ThreadFunc()
{
    while(!this->mStopped) this->Pump(1000);
}
// FUNCTION: WON 0x006b6160
// Continues a pending op and finishes it unless it is still waiting; returns true once the op is done
bool SocketThread::DoOp(SocketOp* inOp)
{
    if(inOp->GetStatus()==WS_Pending) {
        WONStatus aStatus=inOp->Continue();
        if(aStatus!=WS_TimedOut) {
            inOp->mInSocketThread=false;
            inOp->Finish(aStatus);
        }
    }
    if(inOp->GetStatus()!=WS_Pending) {
        inOp->mInSocketThread=false;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006b61a0
// Destroys the socket thread, killing every op it still holds
SocketThreadSimple::~SocketThreadSimple()
{
    this->PurgeOps();
}
// FUNCTION: WON 0x006b6380
// Kills every op the thread holds and forgets them
void SocketThreadSimple::PurgeOps()
{
    OpSet::iterator anItr=this->mOpSet.begin();
    while(anItr!=this->mOpSet.end()) {
        SmartPtr<SocketOp> anOp=*anItr;
        anOp->Kill(true);
        ++anItr;
    }
    this->mOpSet.clear();
}
// FUNCTION: WON 0x006b64f0
// Hands an op to the thread to be continued on each pump
void SocketThreadSimple::AddSocketOp(SocketOp* inOp)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mOpSet.insert(SmartPtr<SocketOp>(inOp));
    inOp->mInSocketThread=true;
}
// FUNCTION: WON 0x006b6660
// Takes an op back from the thread
void SocketThreadSimple::RemoveSocketOp(SocketOp* inOp)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mOpSet.erase(SmartPtr<SocketOp>(inOp));
    inOp->mInSocketThread=false;
}
// FUNCTION: WON 0x006b6760
// Continues every held op once, dropping finished ones, then waits up to 20 ms (or less) for a signal
void SocketThreadSimple::Pump(unsigned long inWaitTime)
{
    AutoCrit aCrit(this->mDataCrit);
    OpSet::iterator anItr=this->mOpSet.begin();
    while(anItr!=this->mOpSet.end()) {
        SocketOp* anOp=anItr->get();
        if(SocketThread::DoOp(anOp)) this->mOpSet.erase(anItr++);
        else ++anItr;
    }
    aCrit.Leave();
    if(inWaitTime>=20) inWaitTime=20;
    if(inWaitTime>0) this->mSignalEvent.WaitFor(inWaitTime);
}
// FUNCTION: WON 0x006b6890
// Pumps the socket ops every 20 ms until the thread is stopped
void SocketThreadSimple::ThreadFunc()
{
    while(!this->mStopped) this->Pump(20);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b6100
// (public: virtual void * __thiscall WONAPI::SocketThread::`scalar deleting destructor'(unsigned int)    [??_GSocketThread@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b62a0
// (public: virtual void * __thiscall WONAPI::SocketThreadSimple::`scalar deleting destructor'(unsigned int)    [??_GSocketThreadSimple@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b62c0
// (public: __thiscall std::set<class WONAPI::SmartPtr<class WONAPI::SocketOp>,struct WONAPI::SmartPtr<class WONAPI::SocketOp>::Comp,class std::allocator<)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b8720
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::SocketOp>,class WONAPI::SmartPtr<class WONAPI::SocketOp>,struct std::_Setk<class WONAPI:)
// TEMPLATE: WON 0x006b9160
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::SocketOp>,class WONAPI::SmartPtr<class WONAPI::SocketOp>,struct std::_Setk<class WONAPI:)
// TEMPLATE: WON 0x006b9210
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::SocketOp>,class WONAPI::SmartPtr<class WONAPI::SocketOp>,struct std::_Setk<class WONA)
// TEMPLATE: WON 0x006b9940
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::SocketOp>,class WONAPI::SmartPtr<class WONAPI::SocketOp>,struct std::_Setk<)
