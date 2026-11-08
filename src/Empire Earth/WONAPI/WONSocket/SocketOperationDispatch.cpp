// Start/continue steps of the connect, accept and receive-bytes socket ops (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006b2320
// Starts connecting the op's socket to the op's address
WONStatus ConnectOp::Start() { return this->mSocket->Fn2B0E60(this->mAddr); }
// FUNCTION: WON 0x006b2330
// Waits for the connect to finish for the time the op has left
WONStatus ConnectOp::Continue()
{
    AsyncSocket* socket=this->mSocket.get();
    return socket->WaitForConnect(this->TimeLeft());
}
// FUNCTION: WON 0x006b4ee0
// Starts receiving the requested number of bytes
WONStatus RecvBytesOp::Start() { return this->StartRecv(); }
// FUNCTION: WON 0x006b4ef0
// Keeps receiving until the requested number of bytes has arrived
WONStatus RecvBytesOp::Continue() { return this->ContinueRecv(); }
}

#include "ee/wonapi/WONAPICore.h"
namespace WONAPI {
// FUNCTION: WON 0x006b0310
// Waits for an incoming connection for the time the op has left, then accepts it into the op
WONStatus AcceptOp::Continue()
{
    this->mSocket->WaitForAccept(this->TimeLeft());
    return this->mSocket->Accept(this->mAcceptedSocket);
}
// FUNCTION: WON 0x006b60c0
// Removes the op from the socket thread if it was handed there
void SocketOp::CleanupHook()
{
    if(this->mInSocketThread)
        WONAPICore::RemoveFromSocketThread(this);
}
}
