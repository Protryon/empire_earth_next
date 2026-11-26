// AuthPeerData's members only the drivers use, cut from ee/wonapi/WONAuthPeerData.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
time_t GetAuthDelta() const { return (time_t)this->Num(EE_WON_AUTHOBJ_PEER_DELTA); }
bool CertificateExpired(time_t inExtra = 0) const { return this->Num(EE_WON_AUTHOBJ_PEER_EXPIRED, inExtra) != 0; }
bool Certificate2Expired(time_t inExtra = 0) const { return this->Num(EE_WON_AUTHOBJ_PEER_EXPIRED2, inExtra) != 0; }
time_t GetExpireDelta() const { return (time_t)this->Num(EE_WON_AUTHOBJ_PEER_EXPIRE_DELTA); }
