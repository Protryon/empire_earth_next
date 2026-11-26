// AsyncSocket's members only the drivers use, cut from ee/wonapi/WONRouting.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
void RemoveAllMsgTransforms() { ee_won_asyncsocket_remove_all_transforms(this->mSock); }
WONStatus SetNagling(bool on) { return ee_won_asyncsocket_set_nagling(this->mSock, on); }
// the socket system started on first use, its users counted (Winsock's numbering), cdecl in the SDK
static WONStatus StartWinsock() { return ee_won_socket_start(); }
unsigned char GetLengthFieldSize() const { return ee_won_asyncsocket_length_field_size(this->mSock); }
// the transform chain, the received message in reverse order, the outgoing in order; the first failure
WONStatus TransformRecvMsg(ByteBufferPtr& inMessage) { return ee_won_asyncsocket_transform_recv(this->mSock, &inMessage); }
WONStatus TransformSendMsg(ByteBufferPtr& inMessage) { return ee_won_asyncsocket_transform_send(this->mSock, &inMessage); }
