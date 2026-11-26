// ServerConnection's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// the connect's result; on success the messages and the keep-alive start (2ab1f0, virtual in the SDK; nothing
// overrides it)
void ConnectCallback(AsyncOp* inOp) { ee_won_servconn_connect_callback(this->mConn,static_cast<RefCount*>(inOp)); }
// the socket closed, a blocking connect, then the connect callback's report; its status
WONStatus ConnectBlock(const IPAddr& theAddr,DWORD theTimeout=0xffffffff)
{ return ee_won_servconn_connect_block(this->mConn,static_cast<const void*>(&theAddr),theTimeout); }
void SetKeepAlivePeriod(DWORD theSeconds) { ee_won_servconn_set_keep_alive_period(this->mConn,theSeconds); }
