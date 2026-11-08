// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK disconnect request
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRoutingDisconnectClientOp.h"
namespace WONAPI {
void RoutingDisconnectClientOp::SendRequest()
{
	this->InitSendMsg(20);
	this->SendMsg();
}
}
