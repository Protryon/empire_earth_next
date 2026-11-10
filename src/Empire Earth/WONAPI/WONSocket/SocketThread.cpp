// WONAPI SocketThread and SocketThreadSimple: the thread that drives pending async socket ops
// (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
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
// Continues a pending op and finishes it unless it is still waiting (Rust decides: ee_won_socket_op_finishes);
// returns true once the op is done
bool SocketThread::DoOp(SocketOp* inOp)
{
    if(inOp->GetStatus()==WS_Pending) {
        WONStatus aStatus=inOp->Continue();
        if(ee_won_socket_op_finishes(aStatus)) {
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
// The op set is Rust's (rust/crates/ee_won/src/asyncop.rs OpSet, in the ops' address order as the original's
// std::set of SmartPtrs): the thread holds a reference per op in it. The lock, the thread and the ops' steps stay here.
SocketThreadSimple::SocketThreadSimple()
{
    this->mOpSet=ee_won_opset_new();
}
// FUNCTION: WON 0x006b61a0
// Destroys the socket thread, killing every op it still holds
SocketThreadSimple::~SocketThreadSimple()
{
    this->PurgeOps();
    ee_won_opset_destroy(this->mOpSet);
    this->mOpSet=NULL;
}
static void SocketOpPurged(void*,void* inOp)
{
    SocketOp* anOp=static_cast<SocketOp*>(inOp);
    anOp->Kill(true);
    anOp->Release();
}
static void SocketOpCollect(void* inList,void* inOp)
{
    static_cast<std::vector<SocketOp*>*>(inList)->push_back(static_cast<SocketOp*>(inOp));
}
// FUNCTION: WON 0x006b6380
// Kills every op the thread holds and forgets them
void SocketThreadSimple::PurgeOps()
{
    ee_won_opset_take_all(this->mOpSet,SocketOpPurged,NULL);
}
// FUNCTION: WON 0x006b64f0
// Hands an op to the thread to be continued on each pump
void SocketThreadSimple::AddSocketOp(SocketOp* inOp)
{
    AutoCrit aCrit(this->mDataCrit);
    if(ee_won_opset_insert(this->mOpSet,inOp)) inOp->AddRef();
    inOp->mInSocketThread=true;
}
// FUNCTION: WON 0x006b6660
// Takes an op back from the thread
void SocketThreadSimple::RemoveSocketOp(SocketOp* inOp)
{
    AutoCrit aCrit(this->mDataCrit);
    bool aHeld=ee_won_opset_remove(this->mOpSet,inOp);
    inOp->mInSocketThread=false;
    if(aHeld) inOp->Release();
}
// FUNCTION: WON 0x006b6760
// Continues every held op once, dropping finished ones, then waits up to 20 ms (or less) for a signal
void SocketThreadSimple::Pump(unsigned long inWaitTime)
{
    AutoCrit aCrit(this->mDataCrit);
    std::vector<SocketOp*> anOps;
    ee_won_opset_snapshot(this->mOpSet,SocketOpCollect,&anOps);
    for(size_t i=0;i<anOps.size();i++) {
        SocketOp* anOp=anOps[i];
        if(SocketThread::DoOp(anOp) && ee_won_opset_remove(this->mOpSet,anOp)) anOp->Release();
    }
    aCrit.Leave();
    inWaitTime=ee_won_socket_pump_wait(static_cast<uint32_t>(inWaitTime));
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
