// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK routing group administration
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingGroupAdmin.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RoutingSetGroupMaxPlayersOp::SendRequest()
{
	this->InitSendMsg(62 /* SDK RoutingSetGroupMaxPlayersRequest */);
	this->mSendMsg.AppendShort(this->mGroupId);
	this->mSendMsg.AppendShort(this->mMaxPlayers);
	this->SendMsg();
	this->AddOp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RoutingSetGroupMaxPlayersOp::HandleReply(unsigned char theMsgType, ReadBuffer &theMsg)
{
	if(theMsgType!=66 /* SDK RoutingStatusReply */)
		return WS_RoutingOp_DontWantReply /* SDK WS_RoutingOp_DontWantReply */;

	WONStatus aStatus = (WONStatus)theMsg.ReadShort();
	return aStatus;
}


}
