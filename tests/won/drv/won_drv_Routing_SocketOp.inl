// SocketOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 39): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
bool IsInSocketThread() const { return ee_won_socketop_in_socket_thread(this->mSockOp); }
