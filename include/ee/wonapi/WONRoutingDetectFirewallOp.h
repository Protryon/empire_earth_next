// No original address identified: WON SDK derived op
#ifndef EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#define EE_WONAPI_WONROUTINGDETECTFIREWALLOP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"

namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingDetectFirewallOp : public RoutingOp
{
private:
	unsigned short mListenPort;
	unsigned long mMaxConnectWaitTime;
	SmartPtr<BlockingSocket> mListenSocket;
	bool mDoListen;
	bool mWaitingForStatusReply;

	virtual void SendRequest();
	virtual WONStatus HandleReply(unsigned char theMsgType, ReadBuffer &theMsg);
	virtual void CleanupHook();

public:
	RoutingDetectFirewallOp(RoutingConnection *theConnection);

	void SetListenPort(unsigned short thePort) { this->mListenPort = thePort; }
	void SetMaxConnectWaitTime(unsigned long theNumSeconds) { this->mMaxConnectWaitTime = theNumSeconds; }
	void SetDoListen(bool doListen) { this->mDoListen = doListen; }

	unsigned short GetListenPort() const { return this->mListenPort; }
	unsigned long GetMaxConnectWaitTime() const { return this->mMaxConnectWaitTime; }
	bool GetDoListen() { return this->mDoListen; }

	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_DetectFirewall; // SDK RoutingOp_DetectFirewall
 }
};


typedef SmartPtr<RoutingDetectFirewallOp> RoutingDetectFirewallOpPtr;


}; // namespace WONAPI



#endif
