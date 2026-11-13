// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK routing firewall request/reply lifecycle. The request and its two
// replies are the Rust op's (routeop.rs); this class keeps the listening socket
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
void RoutingDetectFirewallOp::CleanupHook()
{
	RoutingOp::CleanupHook();
	if(this->mListenSocket.get()!=NULL)
	{
		this->mListenSocket->Close();
		this->mListenSocket = NULL;
	}
}

// Listens first when asked (the listen port becomes the socket's), then sends the request and waits for the status
void RoutingDetectFirewallOp::SendRequest()
{
	if(this->GetDoListen())
	{
		this->mListenSocket = new BlockingSocket(AsyncSocket::TCP);
		WONStatus aStatus = this->mListenSocket->Fn2B0D90(this->GetListenPort(),0);
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
		this->SetListenPort(this->mListenSocket->GetLocalPort());
	}
	this->SetNum(EE_WON_RTF_WAITING, 1);
	RoutingOp::SendRequest();
}
}
