// IPAddr's members only the drivers use, cut from ee/EEWonNet.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
bool SetWithDefaultPortNoDNS(const std::string& inText, unsigned short inPort)
{ return ee_won_ipaddr_set_no_dns(this->mAddr, Bytes(inText), inText.length(), inPort); }
void SetSixByte(const void* inSix) { ee_won_ipaddr_set_six(this->mAddr, static_cast<const uint8_t*>(inSix)); }
// ServerContext's conversions to and from servctx.rs's EeWonAddr (the host borrowed while this value lives)
EeWonAddr ToRaw() const { return ee_won_ipaddr_to_raw(this->mAddr); }
