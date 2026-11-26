// RoutingOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
std::string DataType() const { std::string s; ee_won_routeop_data_type(this->mRust, WonStringSink, &s); return s; }
unsigned short GetLinkId() const { return (unsigned short)this->Num(EE_WON_RTF_LINK_ID); }
//2c5f70, the group list op's body into its map (false when it runs short; what was read stays)
bool ParseGroupListObj(const void* inData, unsigned long inSize)
{ return ee_won_routeop_parse_group_list(this->mRust, static_cast<const uint8_t*>(inData), inSize); }
