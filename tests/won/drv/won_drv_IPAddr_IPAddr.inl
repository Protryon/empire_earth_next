// IPAddr's members only the drivers use, cut from ee/EEWonNet.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
bool SetWithDefaultPortNoDNS(const std::string& inText, unsigned short inPort)
{ return ee_won_ipaddr_set_no_dns(this->mAddr, Bytes(inText), inText.length(), inPort); }
void SetSixByte(const void* inSix) { ee_won_ipaddr_set_six(this->mAddr, static_cast<const uint8_t*>(inSix)); }
// ServerContext's conversions to and from servctx.rs's EeWonAddr (the host borrowed while this value lives)
EeWonAddr ToRaw() const { return ee_won_ipaddr_to_raw(this->mAddr); }
// (R6 WON round 39)
// 2b2d50 / 2b2df0: from "host[:port]" (by DNS / dotted only), the default port if none; the validity
bool SetWithDefaultPort(const std::string& inText, unsigned short inPort)
{ return ee_won_ipaddr_set_with_default_port(this->mAddr, Bytes(inText), inText.length(), inPort); }
unsigned short GetPort() const { return ee_won_ipaddr_port(this->mAddr); } //2b31c0, host byte order
void FromRaw(const EeWonAddr& inRaw) { ee_won_ipaddr_from_raw(this->mAddr, &inRaw); }
