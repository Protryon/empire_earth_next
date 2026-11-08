// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK routing firewall request/reply lifecycle
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingDetectFirewallOp.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
RoutingDetectFirewallOp::RoutingDetectFirewallOp(RoutingConnection *theConnection) :
	RoutingOp(theConnection),
	mListenPort(0),
	mMaxConnectWaitTime(0),
	mDoListen(true),
	mWaitingForStatusReply(false)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RoutingDetectFirewallOp::CleanupHook()
{
	RoutingOp::CleanupHook();
	if(this->mListenSocket.get()!=NULL)
	{
		this->mListenSocket->Close();
		this->mListenSocket = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RoutingDetectFirewallOp::SendRequest()
{
	this->InitSendMsg(18 /* SDK RoutingDetectFirewallRequest */);
	if(this->mDoListen)
	{
		this->mListenSocket = new BlockingSocket(AsyncSocket::TCP);
		WONStatus aStatus = this->mListenSocket->Fn2B0D90(this->mListenPort,0);
		if(aStatus!=0)
		{
			this->Finish(aStatus);
			return;
		}

		aStatus = this->mListenSocket->Listen(1000);
		if(aStatus!=0)
		{
			this->Finish(aStatus);
			return;
		}

		this->mListenPort = this->mListenSocket->GetLocalPort();
	}


	this->mWaitingForStatusReply = true;
	this->mSendMsg.AppendShort(this->mListenPort);
	this->mSendMsg.AppendLong(this->mMaxConnectWaitTime);
	this->SendMsg();
	this->AddOp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RoutingDetectFirewallOp::HandleReply(unsigned char theMsgType, ReadBuffer &theMsg)
{
	WONStatus aStatus;
	if(this->mWaitingForStatusReply && theMsgType==66 /* SDK RoutingStatusReply */)
	{
		this->mWaitingForStatusReply = false;
		aStatus = (WONStatus)theMsg.ReadShort();
		if(aStatus!=0)
			return aStatus;
		else
			return WS_RoutingOp_NeedMoreReplies /* SDK WS_RoutingOp_NeedMoreReplies */;
	}
	else if(theMsgType==19 /* SDK RoutingDetectFirewallResult */)
	{
		aStatus = (WONStatus)theMsg.ReadShort();
		this->mListenPort = theMsg.ReadShort();
		return aStatus;
	}
	else
		return WS_RoutingOp_DontWantReply /* SDK WS_RoutingOp_DontWantReply */;
}

}
