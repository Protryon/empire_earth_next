// No original address identified: WON SDK derived op. Its listen port, longest wait, listen flag and status wait are
// the Rust op's members (routeop.rs, EE_WON_RTF_LISTEN_PORT / MAX_WAIT / DO_LISTEN / WAITING); the class keeps the
// listening socket (RoutingDetectFirewallOp.cpp is inline here since net round 26)
#ifndef EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#define EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "rust/won_net/routeop.h"

namespace WONAPI
{

class RoutingDetectFirewallOp : public RoutingOp
{
private:
	SmartPtr<BlockingSocket> mListenSocket;

	// listens first when asked (the listen port becomes the socket's), then sends the request and waits for the status
	virtual void SendRequest()
	{
		if(this->GetDoListen())
		{
			this->mListenSocket = new BlockingSocket(AsyncSocket::TCP);
			WONStatus aStatus = this->mListenSocket->Fn2B0D90(this->GetListenPort(),0);
			if(aStatus==0) aStatus = this->mListenSocket->Listen(1000);
			if(aStatus!=0) { this->Finish(aStatus); return; }
			this->SetListenPort(this->mListenSocket->GetLocalPort());
		}
		this->SetNum(EE_WON_RTF_WAITING, 1);
		RoutingOp::SendRequest();
	}
	virtual void CleanupHook()
	{
		RoutingOp::CleanupHook();
		if(this->mListenSocket.get()!=NULL) { this->mListenSocket->Close(); this->mListenSocket = NULL; }
	}

public:
	RoutingDetectFirewallOp(RoutingConnection *theConnection) : RoutingOp(theConnection, RoutingOp_DetectFirewall) {}

	void SetListenPort(unsigned short thePort) { this->SetNum(EE_WON_RTF_LISTEN_PORT, thePort); }
	void SetMaxConnectWaitTime(unsigned long theNumSeconds) { this->SetNum(EE_WON_RTF_MAX_WAIT, theNumSeconds); }
	void SetDoListen(bool doListen) { this->SetNum(EE_WON_RTF_DO_LISTEN, doListen); }

	unsigned short GetListenPort() const { return (unsigned short)this->Num(EE_WON_RTF_LISTEN_PORT); }
	unsigned long GetMaxConnectWaitTime() const { return (unsigned long)this->Num(EE_WON_RTF_MAX_WAIT); }
	bool GetDoListen() { return this->Num(EE_WON_RTF_DO_LISTEN) != 0; }
};


typedef SmartPtr<RoutingDetectFirewallOp> RoutingDetectFirewallOpPtr;


}; // namespace WONAPI



#endif
