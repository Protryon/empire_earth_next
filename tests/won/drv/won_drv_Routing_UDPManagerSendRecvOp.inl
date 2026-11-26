// UDPManagerSendRecvOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void SetMsgTimeout(DWORD theTimeout) { ee_won_udpsendrecvop_set_timeout(this->mUdp, static_cast<uint32_t>(theTimeout)); }
void SetNumRetransmits(int theNum) { ee_won_udpsendrecvop_set_retry_limit(this->mUdp, theNum); }
// the request datagram
void SetOutgoing(const ByteBuffer* theMsg)
{ ee_won_udpsendrecvop_set_outgoing(this->mUdp, const_cast<RefCount*>(static_cast<const RefCount*>(theMsg))); }
const ByteBuffer* GetOutgoing() const
{ return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_udpsendrecvop_outgoing(this->mUdp))); }
int GetAttemptCount() const { return ee_won_udpsendrecvop_attempts(this->mUdp); }
int GetNumRetransmits() const { return ee_won_udpsendrecvop_retry_limit(this->mUdp); }
unsigned long GetMsgTimeout() const { return ee_won_udpsendrecvop_timeout(this->mUdp); }
