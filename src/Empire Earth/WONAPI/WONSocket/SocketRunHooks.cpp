// SocketOp run step: start, then hand to the socket thread or finish in place
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// The op's base state and RunHook are a Rust object (rust/crates/ee_won/src/sockop.rs EeWonSocketOp); these are its
// calls back: the virtual Start and Continue, Finish, the run mode, the socket thread's add, the socket's references
// and its Rust object
static long HookStart(void* inOp) { return static_cast<SocketOp*>(inOp)->Start(); }
static long HookContinue(void* inOp) { return static_cast<SocketOp*>(inOp)->Continue(); }
static void HookFinish(void* inOp, long inStatus) { static_cast<SocketOp*>(inOp)->Finish(inStatus); }
static int32_t HookMode(void* inOp) { return static_cast<int32_t>(static_cast<SocketOp*>(inOp)->GetMode()); }
static void HookSocketAddRef(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->AddRef(); }
static void HookSocketRelease(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->Release(); }
static EeWonAsyncSocket* HookSocketObject(void* inSocket) { return static_cast<AsyncSocket*>(inSocket)->GetSocketObject(); }
void SocketOp::HookAddToSocketThread(void* inOp) { WONAPICore::AddToSocketThread(static_cast<SocketOp*>(inOp)); }
const EeWonSocketOpBaseHooks SocketOp::kHooks={HookStart,HookContinue,HookFinish,HookMode,
                                               &SocketOp::HookAddToSocketThread,HookSocketAddRef,
                                               HookSocketRelease,HookSocketObject};

// Creates an op on the socket (a reference held): no socket thread, no waits, the event indices -1
SocketOp::SocketOp(AsyncSocket* inSocket)
{
    this->mSockOp=ee_won_socketop_new(static_cast<void*>(this),&SocketOp::kHooks);
    this->SetSocket(inSocket);
    this->SetRunAsyncImmediately(SocketOp::mRunAsyncImmediatelyDef);
}
// Destroys the op: the socket's reference goes with the Rust object
SocketOp::~SocketOp() { ee_won_socketop_free(this->mSockOp); }

// FUNCTION: WON 0x006b6070
// Starts the op, first finishing it with InvalidSocket when it has no socket (and starting anyway); a pending
// start is queued on the socket thread (async) or continued and finished (blocking), any other result finishes it
void SocketOp::RunHook() { ee_won_socketop_run_hook(this->mSockOp); }
}

// SDK SocketOp static definition; in the original a zero byte at 565f08
namespace WONAPI {
bool SocketOp::mRunAsyncImmediatelyDef;
}
