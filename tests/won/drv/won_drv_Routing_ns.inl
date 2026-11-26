// the functions only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
inline const char* GetBufferData(const ByteBufferPtr& inBuffer)
{
return inBuffer->data();
}
inline int HttpSessionNumOps(HTTPSession* inSession) { return ee_won_http_session_num_ops(HttpSessionOf(inSession)); }
