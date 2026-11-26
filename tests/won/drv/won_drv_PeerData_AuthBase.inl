// AuthBase's members only the drivers use, cut from ee/wonapi/WONAuthPeerData.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
time_t GetIssueTime() const { return (time_t)this->Num(EE_WON_AUTHOBJ_ISSUE); }
time_t GetExpireTime() const { return (time_t)this->Num(EE_WON_AUTHOBJ_EXPIRE); }
bool IsValidAndVerified(const ElGamal& inKey) const { return ee_won_authobj_verified_by(this->mImpl, inKey.Raw()); }
