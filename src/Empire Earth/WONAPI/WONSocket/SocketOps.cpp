// WONAPI socket send and receive ops: raw bytes, length-prefixed messages, datagrams (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif
#include "platform/PlatFile.h"
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x006b5e50
// Starts sending the op's bytes from the beginning
WONStatus SendBytesOp::Start()
{
    this->mReadBuffer.Rewind();
    return this->Continue();
}

// FUNCTION: WON 0x006b5e70
// Sends the remaining bytes as the socket accepts them; returns WS_TimedOut when time runs out or the send error.
// The loop is Rust's (ee_won ops::send_all), under the socket's lock as AsyncSocket::Send took it
WONStatus SendBytesOp::Continue()
{
    uint32_t aPos = static_cast<uint32_t>(this->mReadBuffer.pos());
    WONStatus aStatus = ee_won_asyncsocket_send_all(this->GetSocket()->GetSocketObject(),
        reinterpret_cast<const uint8_t*>(this->mReadBuffer.data()), this->mReadBuffer.length(), &aPos,
        this->GetTiming());
    this->mReadBuffer.SetPos(static_cast<int>(aPos));
    return aStatus;
}

// FUNCTION: WON 0x006b5f10
// Sends the op's datagram to its address, or on the connection if the socket is connected
WONStatus SendBytesToOp::Continue()
{
    this->GetSocket()->WaitForWrite(this->TimeLeft());
    if (this->GetSocket()->IsConnected())
        return this->GetSocket()->Send(this->mBuffer->data(), this->mBuffer->length(), NULL);
    return this->GetSocket()->SendTo(this->mBuffer->data(), this->mBuffer->length(), this->mAddress);
}

// FUNCTION: WON 0x006b5fa0
// Runs the message through the socket's send transforms, then starts sending it
WONStatus SendMsgOp::Start()
{
    this->mBuffer = this->mMessage;
    WONStatus aStatus = this->GetSocket()->TransformSendMsg(this->mBuffer);
    if (aStatus != 0)
        return aStatus;
    this->mReadBuffer.SetData(this->mBuffer->data(), this->mBuffer->length());
    return this->Continue();
}

// FUNCTION: WON 0x006b6020
// Continues sending the message and counts it as sent once it is complete
WONStatus SendMsgOp::Continue()
{
    WONStatus aStatus = SendBytesOp::Continue();
    if (aStatus == WS_Success)
        NetStats::IncrementMessagesSent(1);
    return aStatus;
}

// FUNCTION: WON 0x006b4de0
// Creates an op that receives the given number of bytes (0 = until the peer closes) into memory
RecvBytesOp::RecvBytesOp(unsigned long inLength, AsyncSocket* inSocket)
    : SocketOp(inSocket), mWriteBuffer(0, true)
{
    this->SetWaitForEvent(0,true);
    this->mLength = inLength;
    this->mFile = NULL;
    this->mAppend = false;
    this->mBinary = true;
    this->mSkipBytes = false;
    this->mFileModifyTime = 0;
}

// FUNCTION: WON 0x006b5000
// Starts a receive: opens the target file if a path is set (WS_FailedToOpenFile on failure) or reserves memory
WONStatus RecvBytesOp::StartRecv()
{
    this->mBytesRead = 0;
    this->mBytesSinceProgress = 0;
    this->mOwnFile = false;
    if (this->mFile == NULL && !this->mPath.empty()) {
        if (this->mBinary)
            this->mFile = PlatFOpen(this->mPath.c_str(), this->mAppend ? "a+b" : "wb");
        else
            this->mFile = PlatFOpen(this->mPath.c_str(), this->mAppend ? "a+" : "w");
        if (this->mFile == NULL)
            return WS_FailedToOpenFile;
        this->mOwnFile = true;
    }
    if (this->mLength > 0 && this->mFile == NULL && !this->mSkipBytes)
        this->mWriteBuffer.Reserve(this->mLength);
    return this->ContinueRecv();
}

// FUNCTION: WON 0x006b50c0
// Receives into memory or the file until the length arrives (or the peer closes when no length),
// reporting progress chunks; returns WS_TimedOut when time runs out or the socket error
WONStatus RecvBytesOp::ContinueRecv()
{
    for (;;) {
        this->GetSocket()->WaitForRead(this->TimeLeft());
        int aRead = 0;
        WONStatus aStatus;
        if (this->mLength > 0 && this->mFile == NULL && !this->mSkipBytes) {
            aStatus = this->GetSocket()->Recv(this->mWriteBuffer.data() + this->mWriteBuffer.length(),
                this->mWriteBuffer.capacity() - this->mWriteBuffer.length(), &aRead);
            if (aRead > 0)
                this->mWriteBuffer.SkipBytes(aRead);
        }
        else {
            char aBuffer[1024];
            int aLength = ee_won_op_chunk_length(static_cast<uint32_t>(this->mLength),
                                                 static_cast<uint32_t>(this->mBytesRead));
            aStatus = this->GetSocket()->Recv(aBuffer, aLength, &aRead);
            if (aRead > 0 && !this->mSkipBytes) {
                if (this->mFile != NULL) {
                    if (fwrite(aBuffer, 1, aRead, this->mFile) != aRead)
                        return this->FinishRecv(WS_FailedToWriteToFile);
                }
                else
                    this->mWriteBuffer.AppendBytes(aBuffer, aRead);
            }
        }
        this->mBytesRead += aRead;
        if (aRead > 0 && this->mProgressCompletion.get() != NULL) {
            this->mBytesSinceProgress += aRead;
            if (this->mBytesSinceProgress > this->mProgressInterval) {
                SmartPtr<RecvChunkOp> anOp = new RecvChunkOp(this);
                anOp->SetCompletion(this->mProgressCompletion);
                long aMode = this->GetMode();
                anOp->Run(aMode, 0);
                this->mBytesSinceProgress %= this->mProgressInterval;
            }
        }
        if (this->mLength == 0) {
            if (aStatus == 100020)
                return this->FinishRecv(WS_Success);
        }
        else if (this->mBytesRead == this->mLength)
            return this->FinishRecv(WS_Success);
        if (aStatus == 100001) {
            if (this->TimeLeft() == 0)
                return WS_TimedOut;
        }
        else if (aStatus != 0)
            return aStatus;
    }
}

// FUNCTION: WON 0x006b54a0
// Finishes a receive: hands over the received bytes, or closes the target file; returns the status
WONStatus RecvBytesOp::FinishRecv(WONStatus inStatus)
{
    if (this->mFile == NULL) {
        this->mReceivedData = this->mWriteBuffer.ToByteBuffer(true);
        return inStatus;
    }
    return this->CloseFile(inStatus);
}

// FUNCTION: WON 0x006b5b00
// Starts receiving a message by reading its 1-, 2- or 4-byte length prefix
WONStatus RecvMsgOp::Start()
{
    this->mState = 1;
    this->mLength = this->GetSocket()->GetLengthFieldSize();
    if (!ee_won_op_valid_length_field(static_cast<uint32_t>(this->mLength)))
        return WS_RecvMsg_InvalidLengthFieldSize;
    WONStatus aStatus = this->StartRecv();
    return aStatus == WS_Success ? this->StartRecvBody() : aStatus;
}

// FUNCTION: WON 0x006b5b50
// Reads the length prefix and starts receiving the body (a 2-byte 0xffff switches to a 4-byte prefix);
// rejects lengths not above the prefix size or of 256000 and more (the prefix's rules: ee_won ops::length_step)
WONStatus RecvMsgOp::StartRecvBody()
{
    this->mState = 2;
    ByteBufferPtr aBuffer = this->GetBytes();
    uint32_t aBody = 0;
    switch (ee_won_op_length_step(reinterpret_cast<const uint8_t*>(aBuffer->data()), aBuffer->length(), &aBody)) {
    case 1: {
        this->mLength = 4;
        this->mState = 1;
        WONStatus aStatus = this->StartRecv();
        if (aStatus != WS_Success)
            return aStatus;
        return this->StartRecvBody();
    }
    case 0: {
        this->mState = 2;
        this->mLength = aBody;
        WONStatus aStatus = this->StartRecv();
        if (aStatus == WS_Success)
            return this->FinishRecvMsg();
        return aStatus;
    }
    default:
        return WS_RecvMsg_InvalidMessageLength;
    }
}

// FUNCTION: WON 0x006b5d20
// Counts the received message and runs it through the socket's receive transforms
WONStatus RecvMsgOp::FinishRecvMsg()
{
    NetStats::IncrementMessagesReceived(1);
    this->mMessage = this->GetBytes();
    return this->GetSocket()->TransformRecvMsg(this->mMessage);
}

// FUNCTION: WON 0x006b5e20
// Continues receiving the length prefix or the body and moves to the next stage when it is complete
WONStatus RecvMsgOp::Continue()
{
    WONStatus aStatus = this->ContinueRecv();
    if (aStatus != WS_Success)
        return aStatus;
    return this->mState == 1 ? this->StartRecvBody() : this->FinishRecvMsg();
}
// FUNCTION: WON 0x006b5410
// Closes the target file if the op opened it and applies the requested modify time; returns the status
WONStatus RecvBytesOp::CloseFile(WONStatus inStatus)
{
    if (this->mFile != NULL) {
        if (this->mOwnFile && fclose(this->mFile) != 0)
            inStatus = WS_FailedToWriteToFile;
        if (this->mFileModifyTime != 0) {
#ifdef _WIN32
            struct _utimbuf aTime;
#else
            struct utimbuf aTime;   // POSIX's utime: the same fields
#endif
            aTime.actime = this->mFileModifyTime;
            aTime.modtime = this->mFileModifyTime;
#ifdef _WIN32
            _utime(this->mPath.c_str(), &aTime);
#else
            utime(this->mPath.c_str(), &aTime);
#endif
        }
        this->mFile = NULL;
    }
    return inStatus;
}

// FUNCTION: WON 0x006b5580
// Leaves the socket thread and closes the target file when the op ends
void RecvBytesOp::CleanupHook()
{
    SocketOp::CleanupHook();
    this->CloseFile(this->GetStatus());
}


// FUNCTION: WON 0x006b55a0
// Creates an op that receives one datagram of up to the given size
RecvBytesFromOp::RecvBytesFromOp(unsigned short inMaxLength, AsyncSocket* inSocket)
    : SocketOp(inSocket)
{
    this->SetWaitForEvent(0,true);
    this->mMaxLength = inMaxLength;
    this->mRawBuffer = NULL;
}
// FUNCTION: WON 0x006b56f0
// Destroys the op, freeing its receive buffer
RecvBytesFromOp::~RecvBytesFromOp()
{
    delete this->mRawBuffer;
}
// FUNCTION: WON 0x006b57d0
// Allocates the receive buffer if needed, clears the last datagram and runs the op
void RecvBytesFromOp::RunHook()
{
    if (this->mRawBuffer == NULL)
        this->mRawBuffer = new char[this->mMaxLength];
    this->mReceivedData = NULL;
    SocketOp::RunHook();
}
// FUNCTION: WON 0x006b5820
// Receives one datagram and its sender (the peer address if connected) into a new byte buffer
WONStatus RecvBytesFromOp::Continue()
{
    this->GetSocket()->WaitForRead(this->TimeLeft());
    int aReceived = 0;
    WONStatus aStatus;
    if (this->GetSocket()->IsConnected()) {
        this->mFromAddress = this->GetSocket()->GetDestAddr();
        aStatus = this->GetSocket()->Recv(this->mRawBuffer, this->mMaxLength, &aReceived);
    }
    else
        aStatus = this->GetSocket()->RecvFrom(this->mRawBuffer, this->mMaxLength, &this->mFromAddress, &aReceived);
    if (aStatus == 0) {
        this->mReceivedData = new ByteBuffer(this->mRawBuffer, aReceived, true);
        this->mRawBuffer = NULL;
    }
    return aStatus;
}
}
namespace WONAPI {
// FUNCTION: WON 0x006b4f00
// Returns a new receive-bytes op with the same length and socket
AsyncOp* RecvBytesOp::Duplicate()
{
    return new RecvBytesOp(this->mLength,this->GetSocket());
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
    return new RecvBytesFromOp(this->mMaxLength,this->GetSocket());
}
// FUNCTION: WON 0x006b4f90
// Sets the completion called after every interval bytes received
void RecvBytesOp::SetProgressCompletion(CompletionBase<AsyncOpPtr>* completion,long interval)
{
    this->mProgressCompletion=completion;
    this->mProgressInterval=interval;
}
// FUNCTION: WON 0x006ab770
// Returns the received message
ByteBufferPtr RecvMsgOp::GetMsg() const
{
    return this->mMessage;
}
// FUNCTION: WON 0x006aa920
// Returns the received datagram
SmartPtr<ByteBuffer> RecvBytesFromOp::GetBytes() const
{
    return this->mReceivedData;
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

