// WONAPI routing op that fetches the routing server's client list
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c56d0
// Clears the client map and asks the routing server for its client list (extended form when requested)
void RoutingGetClientListOp::SendRequest()
{
    this->mClientMap.clear();
    this->InitSendMsg(24);
    if (this->mIsEx) this->mSendMsg.AppendBool(true);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c57a0
// Parses the (extended) client list reply into the client map by client id; returns the status
WONStatus RoutingGetClientListOp::HandleReply(unsigned char inMsgType,ReadBuffer& inMsg)
{
    if (inMsgType!=23 && inMsgType!=74) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mIsEx=inMsgType==74;
    WONStatus status=inMsg.ReadShort();
    if (status==0) {
        int count=(unsigned short)inMsg.ReadShort();
        for (int i=0;i<count;++i) {
            RoutingClientInfoPtr client=this->GetNewClientInfo();
            client->mClientId=inMsg.ReadShort();
            inMsg.ReadWString(client->mName,2);
            client->mFlags=inMsg.ReadLong();
            if (this->mIsEx) RoutingClientJoinedServerOp::ParseExClientData(inMsg,client.get());
            this->mClientMap[client->mClientId]=client;
        }
    }
    return status;
}
}
