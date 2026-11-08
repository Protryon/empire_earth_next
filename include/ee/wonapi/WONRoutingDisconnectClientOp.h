#ifndef EE_WONAPI_WONROUTINGDISCONNECTCLIENTOP_H
#define EE_WONAPI_WONROUTINGDISCONNECTCLIENTOP_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingDisconnectClientOp : public RoutingOp
{
private:
	virtual void SendRequest();

public:
	RoutingDisconnectClientOp(RoutingConnection *theConnection) : RoutingOp(theConnection) {}
	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_DisconnectClient; }
};

typedef SmartPtr<RoutingDisconnectClientOp> RoutingDisconnectClientOpPtr;
}
#endif
