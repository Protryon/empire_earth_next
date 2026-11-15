// WONAPI datagram op that asks a routing server for the online status of friends
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The friend status map, the request datagram and the reply's read are a Rust object since net round 19
// (rust/crates/ee_won/src/userstatus.rs EeWonUserStatus); this class is the UDPManagerSendRecvOp that sends and waits.
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace {
void StatusSink(void* inContext, uint64_t inId, uint8_t inStatus)
{
    (*static_cast<WONAPI::RoutingGetUserStatusOp::FriendStatusMap*>(inContext))[(unsigned long)inId] = inStatus;
}
void BytesSink(void* inContext, const uint8_t* inData, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inData), inLength);
}
}
namespace WONAPI {
// Creates a friend status query to a server address, with 3 retransmits and a 1 s message timeout
RoutingGetUserStatusOp::RoutingGetUserStatusOp(const IPAddr& inAddress, UDPManager* inManager)
    : mStatus(ee_won_userstatus_new())
{
    this->SetDestAddr(inAddress);
    this->SetUDPManager(inManager);
    this->SetNumRetransmits(3);
    this->SetMsgTimeout(1000);
}
RoutingGetUserStatusOp::~RoutingGetUserStatusOp() { ee_won_userstatus_free(this->mStatus); }
// Adds a WON user id to query, with status 0 until the reply arrives
void RoutingGetUserStatusOp::AddWONUserId(unsigned long inUserId) { ee_won_userstatus_add(this->mStatus, inUserId); }
RoutingGetUserStatusOp::FriendStatusMap RoutingGetUserStatusOp::GetFriendStatusMap() const
{
    FriendStatusMap aMap;
    ee_won_userstatus_each(this->mStatus, StatusSink, &aMap);
    return aMap;
}
void RoutingGetUserStatusOp::SetFriendStatusMap(const FriendStatusMap& inMap)
{
    ee_won_userstatus_clear(this->mStatus);
    for (FriendStatusMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
        ee_won_userstatus_put(this->mStatus, i->first, i->second);
}
// Builds the user status datagram listing every queried WON id and starts the send/receive
void RoutingGetUserStatusOp::RunHook()
{
    std::string aBytes;
    ee_won_userstatus_request(this->mStatus, BytesSink, &aBytes);
    ByteBufferPtr aMessage = new ByteBuffer(aBytes.data(), (unsigned long)aBytes.size(), false);
    this->SetOutgoing(aMessage);
    UDPManagerSendRecvOp::RunHook();
}
// Checks the status reply header and count and stores each user's status byte; returns the result (a reply cut
// short is an unpack failure, what was read kept)
WONStatus RoutingGetUserStatusOp::CheckResponse(ReadBuffer& inMessage)
{
    size_t aPos = 0;
    WONStatus aStatus = ee_won_userstatus_reply(this->mStatus, reinterpret_cast<const uint8_t*>(inMessage.data()),
                                                inMessage.length(), (size_t)inMessage.pos(), &aPos);
    inMessage.SetPos((int)aPos);
    return aStatus;
}
}
