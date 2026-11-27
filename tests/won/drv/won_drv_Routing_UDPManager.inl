// UDPManager's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 39): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void SetAutoRecv(bool inAutoRecv) { ee_won_udpmanager_set_auto_recv(this->mManager, inAutoRecv); }
void CleanOps() { ee_won_udpmanager_clean_ops(this->mManager); }
