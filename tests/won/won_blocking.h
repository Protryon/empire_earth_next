// BlockingSocket's seven blocking calls as the drivers' own helpers: the original's statements (BlockingSocket.cpp,
// 0x2b1c10, 0x2b1d00, 0x2b1e30, 0x2b1f10, 0x2b2040 and the SDK's SendBytesTo, RecvBytesFrom, RecvBytes) over the
// socket op classes. The class lost them in net round 24 (no game caller was left: the op base's blocking calls
// are Rust's, ee_won hostsock.rs, compared with these statements by won_hostsock_test); the drivers whose goldens
// were recorded through them call these. Requires ee/wonapi/WONRouting.h.
#ifndef EE_TESTS_WON_BLOCKING_H
#define EE_TESTS_WON_BLOCKING_H

namespace WonBlocking {
using namespace WONAPI;

// connects to an address, waiting up to the timeout; the connect status
inline WONStatus Connect(AsyncSocket* inSocket, const IPAddr& inAddress, unsigned long inTimeout)
{
    SmartPtr<ConnectOp> op = new ConnectOp(inAddress, inSocket);
    op->RunBlock(inTimeout);
    return op->GetStatus();
}
// sends raw bytes, waiting up to the timeout; the send status
inline WONStatus SendBytes(AsyncSocket* inSocket, const ByteBuffer* inBytes, unsigned long inTimeout)
{
    SmartPtr<SendBytesOp> op = new SendBytesOp(inBytes, inSocket);
    op->RunBlock(inTimeout);
    return op->GetStatus();
}
// sends one length-prefixed message, waiting up to the timeout; the send status
inline WONStatus SendMsg(AsyncSocket* inSocket, const ByteBuffer* inMessage, unsigned long inTimeout)
{
    SmartPtr<SendMsgOp> op = new SendMsgOp(inMessage, inSocket);
    op->RunBlock(inTimeout);
    return op->GetStatus();
}
// waits up to the timeout for one length-prefixed message; it and the receive status
inline WONStatus RecvMsg(AsyncSocket* inSocket, ByteBufferPtr& outMessage, unsigned long inTimeout)
{
    SmartPtr<RecvMsgOp> op = new RecvMsgOp(inSocket);
    if (op->RunBlock(inTimeout)) outMessage = op->GetMsg().get();
    return op->GetStatus();
}
// waits up to the timeout for one CR-LF terminated line; it and the receive status
inline WONStatus RecvCRMsg(AsyncSocket* inSocket, ByteBufferPtr& outMessage, unsigned long inTimeout)
{
    SmartPtr<RecvCRMsgOp> op = new RecvCRMsgOp(inSocket);
    if (op->RunBlock(inTimeout)) outMessage = op->GetMsg();
    return op->GetStatus();
}
// sends a datagram, waiting up to the timeout; the send status
inline WONStatus SendBytesTo(AsyncSocket* inSocket, const ByteBuffer* inBytes, const IPAddr& inAddr,
                             unsigned long inTimeout)
{
    SmartPtr<SendBytesToOp> op = new SendBytesToOp(inBytes, inAddr, inSocket);
    op->RunBlock(inTimeout);
    return op->GetStatus();
}
// waits up to the timeout for one datagram of up to inMaxBytes; it, its sender and the receive status
inline WONStatus RecvBytesFrom(AsyncSocket* inSocket, ByteBufferPtr& outBytes, unsigned short inMaxBytes,
                               IPAddr& outAddr, unsigned long inTimeout)
{
    SmartPtr<RecvBytesFromOp> op = new RecvBytesFromOp(inMaxBytes, inSocket);
    if (op->RunBlock(inTimeout)) {
        outAddr = op->GetAddr();
        outBytes = op->GetBytes();
    }
    return op->GetStatus();
}
// waits up to the timeout for inNumBytes bytes (0: until the peer closes); them and the receive status
inline WONStatus RecvBytes(AsyncSocket* inSocket, ByteBufferPtr& outBytes, DWORD inNumBytes,
                           DWORD inTimeout = 0xffffffff)
{
    SmartPtr<RecvBytesOp> op = new RecvBytesOp(inNumBytes, inSocket);
    if (op->RunBlock(inTimeout)) outBytes = op->GetBytes();
    return op->GetStatus();
}
}

#endif
