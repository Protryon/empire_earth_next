// DetectFirewallOp: asks a firewall-detection server to connect or send a datagram back to us
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
// FUNCTION: WON 0x006cf5c0
// Sets the defaults: listen on any port over TCP, wait for the connect, no receive limit
void DetectFirewallOp::Init()
{
    this->mLengthFieldSize=2;
    this->mListenPort=0;
    this->mMaxConnectWaitTime=0;
    this->mDoListen=true;
    this->mUseUDP=false;
    this->mWaitForConnect=true;
    this->mMaxRecvTime=0xffffffff;
}
// FUNCTION: WON 0x006cf600
// Creates a firewall detection op against the given servers
DetectFirewallOp::DetectFirewallOp(ServerContext* inContext)
    : ServerRequestOp(inContext)
{
    this->Init();
}
// FUNCTION: WON 0x006cf760
// Opens the local TCP listener or UDP socket, notes its port and starts the server request
void DetectFirewallOp::RunHook()
{
    if (this->mDoListen || this->mUseUDP) {
        this->mListenSocket=new BlockingSocket(this->mUseUDP?1:0);
        WONStatus status=this->mListenSocket->Fn2B0D90(this->mListenPort,0);
        if (status!=0) {
            this->Finish(status);
            return;
        }
        if (!this->mUseUDP) {
            status=this->mListenSocket->Listen(1000);
            if (status!=0) {
                this->Finish(status);
                return;
            }
        }
        this->mListenPort=this->mListenSocket->GetLocalPort();
    }
    ServerRequestOp::RunHook();
}
// FUNCTION: WON 0x006cf8b0
// Handles the datagram receive: keeps waiting while datagrams come from other hosts, else finishes
bool DetectFirewallOp::CallbackHook(AsyncOp* inOp,int inParam)
{
    if (inParam!=1000) return ServerRequestOp::CallbackHook(inOp,inParam);
    if (inOp->mStatus==WS_Success &&
        this->mRecvBytesFromOp->mFromAddress.GetHost()!=this->GetCurAddr().GetHost()) {
        this->RecvServerDatagram();
        return true;
    }
    this->Finish(inOp->mStatus);
    return true;
}
// FUNCTION: WON 0x006cf920
// Closes and drops the listen socket when the op finishes
void DetectFirewallOp::CleanupHook()
{
    ServerRequestOp::CleanupHook();
    if (this->mListenSocket.get()!=NULL) {
        this->mListenSocket->Close();
        this->mListenSocket=NULL;
    }
}
// FUNCTION: WON 0x006cf970
// Builds the detect request (TCP or UDP, our port, connect wait settings) and asks to receive the reply
WONStatus DetectFirewallOp::GetNextRequest()
{
    WriteBuffer request(this->mLengthFieldSize,true);
    request.AppendByte(5);
    request.AppendShort(3);
    request.AppendShort(this->mUseUDP?3:1);
    request.AppendShort(this->mListenPort);
    if (!this->mUseUDP) {
        request.AppendBool(this->mWaitForConnect);
        request.AppendLong(this->mMaxConnectWaitTime);
        request.AppendBool(false);
    }
    this->mRequest=request.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}
// FUNCTION: WON 0x006cfad0
// Checks the reply header, records our public address if given and returns the status (UDP: waits for the datagram)
WONStatus DetectFirewallOp::CheckResponse()
{
    ReadBuffer reply(this->mResponse->data(),this->mResponse->length());
    unsigned char header=reply.ReadByte();
    unsigned short service=reply.ReadShort();
    unsigned short message=reply.ReadShort();
    if (header!=5 || service!=3 || message!=2) return this->InvalidReplyHeader();
    short status=reply.ReadShort();
    if (reply.mLength-reply.mPos>=4) {
        long host=reply.ReadLong();
        if (host!=0) this->mPublicAddr.Fn2B2C20(host,0);
    }
    if (this->mUseUDP) return this->RecvServerDatagram();
    return status;
}
// FUNCTION: WON 0x006cfba0
// Waits for a datagram from the server's host on the UDP socket, async or blocking; returns the status
WONStatus DetectFirewallOp::RecvServerDatagram()
{
    if (this->mRunMode==OP_MODE_ASYNC) {
        this->mRecvBytesFromOp=new RecvBytesFromOp(255,this->mListenSocket.get());
        unsigned long wait=this->TimeLeft();
        if (this->mMaxConnectWaitTime>0 && this->mMaxConnectWaitTime<wait)
            wait=this->mMaxConnectWaitTime;
        this->Track(this->mRecvBytesFromOp.get(),1000);
        if (wait==0) wait=0xffffffff;
        this->mRecvBytesFromOp->RunAsync(wait);
        return WS_ServerReq_ExitCommunicationLoop;
    }
    do {
        this->mRecvBytesFromOp=new RecvBytesFromOp(255,this->mListenSocket.get());
        unsigned long wait=this->TimeLeft();
        if (this->mMaxConnectWaitTime>0 && this->mMaxConnectWaitTime<wait)
            wait=this->mMaxConnectWaitTime;
        WONStatus status=this->mRecvBytesFromOp->Run(2,wait);
        if (status!=WS_Success) return status;
    } while (this->mRecvBytesFromOp->mFromAddress.GetHost()!=this->GetCurAddr().GetHost());
    return WS_Success;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cf690
// (public: virtual __thiscall WONAPI::DetectFirewallOp::~DetectFirewallOp(void)    [??1DetectFirewallOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cf670
// (public: virtual void * __thiscall WONAPI::DetectFirewallOp::`scalar deleting destructor'(unsigned int)    [??_GDetectFirewallOp@WONAPI@@UAEPAXI@Z])
