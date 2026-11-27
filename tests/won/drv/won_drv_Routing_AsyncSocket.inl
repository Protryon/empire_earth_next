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
// (R6 WON round 39)
bool IsOpen() { return ee_won_asyncsocket_is_open(this->mSock); }
void SetType(SocketType theType) { ee_won_asyncsocket_set_type(this->mSock, theType); }
static WONStatus StopWinsock() { return ee_won_socket_stop(); }
WONStatus SetBroadcast(bool on) { return ee_won_asyncsocket_set_broadcast(this->mSock, on); }
// a signed count; the sender on success, assigned as a new address of its platform address (sockobj.rs)
WONStatus RecvFrom(void* outData, int inLength, IPAddr* outAddress, int* outReceived)
{ return ee_won_asyncsocket_recv_from_ip(this->mSock, outData, inLength, outAddress != NULL ? const_cast<EeWonIpAddr*>(outAddress->GetValue()) : NULL, outReceived); }
// connects: an existing stream connection dropped, opened and bound to the local host if needed
WONStatus Fn2B0E60(const IPAddr& inAddress)
{ return ee_won_asyncsocket_connect_to(this->mSock, inAddress.GetValue(), static_cast<uint32_t>(IPAddr::GetLocalHost())); }
// success, the socket error or WS_TimedOut
WONStatus WaitForConnect(unsigned long inWaitTime)
{ return ee_won_asyncsocket_wait_for_connect(this->mSock, static_cast<uint32_t>(inWaitTime)); }
void WaitForAccept(unsigned long inTimeout) { this->WaitForRead(inTimeout); }
unsigned short GetLocalPort() { return ee_won_asyncsocket_local_port(this->mSock); } // or 0
void WaitForRead(unsigned long inWaitTime) { ee_won_asyncsocket_wait(this->mSock, PLAT_SOCKET_WAIT_READ, static_cast<uint32_t>(inWaitTime)); }
WONStatus SendTo(const void* inData, int inLength, const IPAddr& inAddress) // opened first if needed; fails on stream sockets
{ return ee_won_asyncsocket_send_to(this->mSock, inData, inLength, inAddress.GetSockAddrIn().mAddress, inAddress.GetSockAddrIn().mPort); }
// Shutdown when the peer closed, WS_TimedOut on a short stream read
WONStatus Recv(void* outData, int inLength, int* outReceived)
{ return ee_won_asyncsocket_recv(this->mSock, outData, inLength, outReceived); }
void SetDestAddr(const IPAddr& inAddr) { ee_won_asyncsocket_set_dest_addr(this->mSock,inAddr.GetValue()); }
