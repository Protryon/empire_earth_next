// UDPManagerRecvOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
bool HasVerifier() { return ee_won_udprecvop_verifier(this->mUdp) != NULL; }
void SetFromAddr(const IPAddr& theAddr) { ee_won_udprecvop_set_address(this->mUdp, theAddr.GetValue()); }
UDPManager* GetUDPManager() const { return static_cast<UDPManager*>(static_cast<RefCount*>(ee_won_udprecvop_manager(this->mUdp))); }
