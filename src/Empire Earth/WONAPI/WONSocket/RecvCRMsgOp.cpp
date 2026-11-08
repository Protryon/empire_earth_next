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
// FUNCTION: WON 0x006b59b0
// Reads one byte at a time until CR-LF; stores the line (CR replaced by a NUL) as the message
// and returns success, WS_TimedOut when time runs out, or the socket error
WONStatus RecvCRMsgOp::Continue()
{
    for (;;) {
        this->mSocket->WaitForRead(this->TimeLeft());
        unsigned char aByte;
        WONStatus aStatus=this->mSocket->Recv(&aByte,1,NULL);
        if (aStatus==WS_Success) {
            this->mBuffer.AppendByte(aByte);
            if (aByte==10 && this->mBuffer.mLength>=2 && this->mBuffer.mData[this->mBuffer.mLength-2]==13) {
                this->mBuffer.data()[this->mBuffer.mLength-2]=0;
                this->mBuffer.SetSize(this->mBuffer.length()-1);
                this->mMessage=this->mBuffer.ToByteBuffer(true);
                return WS_Success;
            }
        } else if (aStatus==WS_TimedOut) {
            if (this->TimeLeft()==0) return WS_TimedOut;
        } else return aStatus;
    }
}

}
