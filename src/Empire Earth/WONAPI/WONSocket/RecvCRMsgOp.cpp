// WONAPI RecvCRMsgOp: receives one CR-LF terminated line (file grouping inferred)
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
// FUNCTION: WON 0x006b5970
// Starts receiving a line: clears the buffer and the previous message, then reads what is available
WONStatus RecvCRMsgOp::Start()
{
    this->mBuffer.Reset();
    this->mMessage=NULL;
    return this->Continue();
}
static void AppendLineBytes(void* inBuffer, const uint8_t* inBytes, size_t inLength)
{
    static_cast<WriteBuffer*>(inBuffer)->AppendBytes(inBytes, static_cast<unsigned long>(inLength));
}
// FUNCTION: WON 0x006b59b0
// Reads one byte at a time until CR-LF (the loop is Rust's: ee_won ops::recv_line); stores the line (CR replaced by
// a NUL) as the message and returns success, WS_TimedOut when time runs out, or the socket error
WONStatus RecvCRMsgOp::Continue()
{
    int32_t aLast=this->mBuffer.length()>0
        ? static_cast<unsigned char>(this->mBuffer.data()[this->mBuffer.length()-1]) : -1;
    WONStatus aStatus=ee_won_op_recv_line(&this->mSocket->mCore,this->mSocket->mDataCrit.mCrit,aLast,
                                          this->GetTiming(),AppendLineBytes,&this->mBuffer);
    if (aStatus==WS_Success) {
        this->mBuffer.data()[this->mBuffer.length()-2]=0;
        this->mBuffer.SetSize(this->mBuffer.length()-1);
        this->mMessage=this->mBuffer.ToByteBuffer(true);
    }
    return aStatus;
}

}
