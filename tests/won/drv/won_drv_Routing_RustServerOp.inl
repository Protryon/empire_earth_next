// RustServerOp's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
BlockingSocket* GetHttpSocket() const
{ return static_cast<BlockingSocket*>(static_cast<RefCount*>(ee_won_hostop_connected_socket(this->mRustOp, EE_WON_HTTPGET_SOCKET))); }
// ServerOp's members (ee_won hostbase.rs): the time limits (2ace20, 2acee0, as DWORDs), the socket and auth
// context (borrowed; the op holds a reference), the auth type, the length field, the client key
void SetMaxTimes(int inConnect, int inSend, int inRecv) { ee_won_hostop_set_max_times(this->mRustOp, inConnect, inSend, inRecv); }
AuthType GetAuthType() const { return (AuthType)ee_won_hostop_base_get(this->mRustOp, EE_WON_HOSTBASE_AUTH_TYPE); }
void SetAuthType(AuthType inType, AuthContext* inContext = NULL) { ee_won_hostop_set_auth(this->mRustOp, inType, static_cast<RefCount*>(inContext), false, NULL); }
void SetClientKeySize(unsigned char inSize, bool inUseClientKey) { ee_won_hostop_set_client_key(this->mRustOp, inSize, inUseClientKey); }
