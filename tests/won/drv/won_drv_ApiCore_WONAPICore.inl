// WONAPICore's members only the drivers use, cut from ee/wonapi/WONAPICore.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
static bool IsStarted() { return ee_won_api_is_started(); } //2f2d00
static void RemoveFromSocketThread(SocketOp* op) { ee_won_api_socket_op(static_cast<void*>(op),false); } //2f2f20
