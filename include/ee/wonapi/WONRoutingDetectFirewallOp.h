// No original address identified: WON SDK derived op. Its listen port, longest wait, listen flag and status wait are
// the Rust op's members (routeop.rs, EE_WON_RTF_LISTEN_PORT / MAX_WAIT / DO_LISTEN / WAITING); the class keeps the
// listening socket
#ifndef EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#define EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"

namespace WONAPI
{

class RoutingDetectFirewallOp : public RoutingOp
{
private:
	SmartPtr<BlockingSocket> mListenSocket;

	virtual void SendRequest();
	virtual void CleanupHook();

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
