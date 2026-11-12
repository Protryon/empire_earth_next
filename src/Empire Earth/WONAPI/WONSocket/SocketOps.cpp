// WONAPI socket send and receive ops: raw bytes, length-prefixed messages, datagrams (file grouping inferred). Rust
// objects since round 15 (rust/crates/ee_won/src/sockops.rs: each op's state in its EeWonSocketOp and every step
// below: the starts and continues, the receive's buffer, file and progress, the message's length stages, the
// datagram's buffer and sender); these are the shims and the C++ side the ops call back (SocketRunHooks.cpp's
// table: the references, the byte buffers, the transforms, the file, the progress chunk, Accept).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

static RefCount* AsRef(const ByteBuffer* inBuffer) { return const_cast<ByteBuffer*>(inBuffer); }
static const ByteBuffer* AsBytes(void* inRef) { return static_cast<const ByteBuffer*>(static_cast<RefCount*>(inRef)); }

// FUNCTION: WON 0x006b5e50
// Starts sending the op's bytes from the beginning
WONStatus SendBytesOp::Start()
{
    return ee_won_socketop_start(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b5e70
// Sends the remaining bytes as the socket accepts them; returns WS_TimedOut when time runs out or the send error
WONStatus SendBytesOp::Continue()
{
    return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming());
}

// Creates an op sending the bytes (a reference held; NULL sends nothing)
SendBytesOp::SendBytesOp(const ByteBuffer* inData, AsyncSocket* inSocket) : SocketOp(inSocket)
{
    this->SetWaitForEvent(1,true);
    ee_won_socketop_init_send(this->GetSocketOpObject(), AsRef(inData), NULL, false);
}

// Creates an op sending the message through the socket's send transforms
SendMsgOp::SendMsgOp(const ByteBuffer* inMessage, AsyncSocket* inSocket) : SendBytesOp(NULL, inSocket)
{
    ee_won_socketop_init_send(this->GetSocketOpObject(), NULL, AsRef(inMessage), true);
}

// Creates an op sending a datagram to the address (or on the connection when the socket is connected)
SendBytesToOp::SendBytesToOp(const ByteBuffer* inData, const IPAddr& inAddress, AsyncSocket* inSocket)
    : SocketOp(inSocket)
{
    this->SetWaitForEvent(1,true);
    ee_won_socketop_init_send_to(this->GetSocketOpObject(), AsRef(inData), inAddress.GetValue());
}

// FUNCTION: WON 0x006b5f10
// Sends the op's datagram to its address, or on the connection if the socket is connected
WONStatus SendBytesToOp::Continue()
{
    return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b5fa0
// Runs the message through the socket's send transforms, then starts sending it
WONStatus SendMsgOp::Start()
{
    return ee_won_socketop_start(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b6020
// Continues sending the message and counts it as sent once it is complete
WONStatus SendMsgOp::Continue()
{
    return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b4de0
// Creates an op that receives the given number of bytes (0 = until the peer closes) into memory
RecvBytesOp::RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket) : SocketOp(inSocket)
{
    this->SetWaitForEvent(0,true);
    ee_won_socketop_init_recv(this->GetSocketOpObject(), static_cast<uint32_t>(inLength), false);
}

// RecvMsgOp's: a receive of length-prefixed messages
RecvBytesOp::RecvBytesOp(AsyncSocket* inSocket, MessageTag) : SocketOp(inSocket)
{
    this->SetWaitForEvent(0,true);
    ee_won_socketop_init_recv(this->GetSocketOpObject(), 0, true);
}

// FUNCTION: WON 0x006b5b00
// Starts receiving a message by reading its 1-, 2- or 4-byte length prefix
WONStatus RecvMsgOp::Start()
{
    return ee_won_socketop_start(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b5e20
// Continues receiving the length prefix or the body and moves to the next stage when it is complete
WONStatus RecvMsgOp::Continue()
{
    return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming());
}

// FUNCTION: WON 0x006b5580
// Leaves the socket thread and closes the target file when the op ends
void RecvBytesOp::CleanupHook()
{
    SocketOp::CleanupHook();
    ee_won_socketop_close_file(this->GetSocketOpObject(), this->GetStatus());
}

// FUNCTION: WON 0x006b55a0
// Creates an op that receives one datagram of up to the given size
RecvBytesFromOp::RecvBytesFromOp(unsigned short inMaxLength, AsyncSocket* inSocket)
    : SocketOp(inSocket)
{
    this->SetWaitForEvent(0,true);
    ee_won_socketop_init_recv_from(this->GetSocketOpObject(), inMaxLength);
}
// FUNCTION: WON 0x006b56f0
// Destroys the op (its buffer goes with the Rust object)
RecvBytesFromOp::~RecvBytesFromOp() {}
// FUNCTION: WON 0x006b57d0
// Allocates the receive buffer if needed, clears the last datagram and runs the op
void RecvBytesFromOp::RunHook()
{
    ee_won_socketop_recv_from_prepare(this->GetSocketOpObject());
    SocketOp::RunHook();
}
// FUNCTION: WON 0x006b5820
// Receives one datagram and its sender (the peer address if connected) into a new byte buffer
WONStatus RecvBytesFromOp::Continue()
{
    return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming());
}
}
namespace WONAPI {
// FUNCTION: WON 0x006b4f00
// Returns a new receive-bytes op with the same length and socket
AsyncOp* RecvBytesOp::Duplicate()
{
    return new RecvBytesOp(ee_won_socketop_length(this->GetSocketOpObject()),this->GetSocket());
}
// FUNCTION: WON 0x006ab3d0
// Returns a new receive-message op on the same socket
AsyncOp* RecvMsgOp::Duplicate()
{
    return new RecvMsgOp(this->GetSocket());
}
// FUNCTION: WON 0x006b5660
// Returns a new receive-datagram op with the same size and socket
AsyncOp* RecvBytesFromOp::Duplicate()
{
    return new RecvBytesFromOp(static_cast<unsigned short>(ee_won_socketop_length(this->GetSocketOpObject())),
                               this->GetSocket());
}
// FUNCTION: WON 0x006b4f90
// Sets the completion called after every interval bytes received
void RecvBytesOp::SetProgressCompletion(CompletionBase<AsyncOpPtr>* completion,long interval)
{
    ee_won_socketop_set_progress(this->GetSocketOpObject(), static_cast<RefCount*>(completion),
                                 static_cast<int32_t>(interval));
}
// FUNCTION: WON 0x006ab770
// Returns the received message
ByteBufferPtr RecvMsgOp::GetMsg() const
{
    return AsBytes(ee_won_socketop_message(this->GetSocketOpObject()));
}
// FUNCTION: WON 0x006aa920
// Returns the received datagram
SmartPtr<ByteBuffer> RecvBytesFromOp::GetBytes() const
{
    return const_cast<ByteBuffer*>(AsBytes(ee_won_socketop_received(this->GetSocketOpObject())));
}
// The datagram's sender (a copy)
IPAddr RecvBytesFromOp::GetAddr() const
{
    return IPAddr::FromValue(ee_won_socketop_address(this->GetSocketOpObject()));
}
static void PathSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
// The target file's path
std::string RecvBytesOp::GetFilePath() const
{
    std::string aPath;
    ee_won_socketop_path(this->GetSocketOpObject(), PathSink, &aPath);
    return aPath;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b5380
// (public: virtual void * __thiscall WONAPI::RecvChunkOp::`scalar deleting destructor'(unsigned int)    [??_GRecvChunkOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006b53a0
// (public: virtual __thiscall WONAPI::RecvChunkOp::~RecvChunkOp(void)    [??1RecvChunkOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006b56d0
// (public: virtual void * __thiscall WONAPI::RecvBytesFromOp::`scalar deleting destructor'(unsigned int)    [??_GRecvBytesFromOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b5370
// (public: virtual void __thiscall WONAPI::RecvChunkOp::Virtual04(void)    [?Virtual04@RecvChunkOp@WONAPI@@UAEXXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ab450
// (public: virtual void * __thiscall WONAPI::RecvMsgOp::`scalar deleting destructor'(unsigned int)    [??_GRecvMsgOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b4f70
// (public: virtual void * __thiscall WONAPI::RecvBytesOp::`scalar deleting destructor'(unsigned int)    [??_GRecvBytesOp@WONAPI@@UAEPAXI@Z])
