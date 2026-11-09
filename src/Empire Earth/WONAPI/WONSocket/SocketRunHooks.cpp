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
// FUNCTION: WON 0x006b6070
// Starts the op, first finishing it with InvalidSocket when it has no socket (and starting anyway); a pending
// start is queued on the socket thread (async) or continued and finished (blocking), any other result finishes it
void SocketOp::RunHook()
{
    if(this->mSocket.get()==NULL)
        this->Finish(WS_SocketOp_InvalidSocket);  //invalid-socket status; the original continues to Start
    WONStatus status=this->Start();
    if(status!=WS_TimedOut) //WS_TimedOut signals pending socket work
    {
        this->Finish(status);
        return;
    }
    if(this->mRunMode==OP_MODE_ASYNC)
        WONAPICore::AddToSocketThread(this);
    else
    {
        status=this->Continue();
        this->Finish(status);
    }
}
}

// SDK SocketOp static definition; in the original a zero byte at 565f08
namespace WONAPI {
bool SocketOp::mRunAsyncImmediatelyDef;
}
