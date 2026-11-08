// PeerAuthServerOp: answers a peer's authentication handshake on an accepted socket
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONStatus.h"

namespace WONAPI {
// FUNCTION: WON 0x006e8950
// Creates a server-side peer auth op for an auth context and socket
PeerAuthServerOp::PeerAuthServerOp(AuthContext* context,BlockingSocket* socket)
{
    this->mAuthContext=context;
    this->mSocket=socket;
}

// FUNCTION: WON 0x006e8bc0
// Destroys the op (members only)
PeerAuthServerOp::~PeerAuthServerOp()
{
}

// FUNCTION: WON 0x006e8d70
// Feeds each received message to the handshake, sends the reply and finishes after the last send
bool PeerAuthServerOp::CallbackHook(AsyncOp* op,int type)
{
    if(op->mStatus!=WS_Success)
    {
        this->Finish(op->mStatus);
        return true;
    }
    if(type==1) // PeerAuth_Track_Recv
    {
        RecvMsgOp* recvOp=dynamic_cast<RecvMsgOp*>(op);
        if(recvOp==NULL)
            return true;

        ByteBufferPtr sendMsg;
        ByteBufferPtr recvMsg=recvOp->GetMsg();
        WONStatus status=this->mPeerAuthServer.HandleRecvMsg(recvMsg->data(),recvMsg->length(),sendMsg);
        int sendType=0;
        if(status==WS_Success && this->mPeerAuthServer.GetState()==PeerAuthServer::STATE_NOT_STARTED)
            sendType=2; // PeerAuth_Track_LastSend
        if(sendMsg.get()!=NULL)
            this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new SendMsgOp(sendMsg.get()),sendType)),this->SendTime());
        if(status!=WS_Success)
            this->Finish(status);
        else if(sendType!=2)
            this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new RecvMsgOp,1)),this->RecvTime());
    }
    else if(type==2)
        this->Success();
    return true;
}

// FUNCTION: WON 0x006e8fa0
// Finishes the op with the status and returns true unless it is success
bool PeerAuthServerOp::CheckStatus(WONStatus status)
{
    if(status==0)
        return false;
    this->Finish(status);
    return true;
}

// FUNCTION: WON 0x006e8fc0
// Queues the first receive on the socket
void PeerAuthServerOp::AsyncRun()
{
    this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new RecvMsgOp,1)),this->RecvTime());
}

// FUNCTION: WON 0x006e9050
// Runs the server handshake, asynchronously or as two blocking receive/reply rounds
void PeerAuthServerOp::RunHook()
{
    this->mLengthFieldSize=this->mSocket->GetLengthFieldSize();
    this->mPeerAuthServer.Start(this->mAuthContext->GetPeerData(),this->mLengthFieldSize);
    if(this->mRunMode==OP_MODE_ASYNC)
    {
        this->AsyncRun();
        return;
    }

    ByteBufferPtr recvMsg;
    WONStatus status=this->mSocket->RecvMsg(recvMsg,this->RecvTime());
    if(this->CheckStatus(status))
        return;

    ByteBufferPtr sendMsg;
    status=this->mPeerAuthServer.HandleRecvMsg(recvMsg->data(),recvMsg->length(),sendMsg);
    this->CheckStatus(status);
    if(sendMsg.get()==NULL)
        return;

    status=this->mSocket->SendMsg(sendMsg.get(),this->SendTime());
    this->CheckStatus(status);
    if(this->mStatus!=WS_Pending)
        return;

    status=this->mSocket->RecvMsg(recvMsg,this->RecvTime());
    if(this->CheckStatus(status))
        return;

    status=this->mPeerAuthServer.HandleRecvMsg(recvMsg->data(),recvMsg->length(),sendMsg);
    this->CheckStatus(status);
    if(sendMsg.get()==NULL)
        return;

    status=this->mSocket->SendMsg(sendMsg.get(),this->SendTime());
    if(status==0)
        this->Success();
    else
        this->Finish(status);
}

// FUNCTION: WON 0x006e9370
// Adds the session encryption to the socket when the auth type encrypts, then finishes with success
void PeerAuthServerOp::Success()
{
    AuthSession* session=this->mPeerAuthServer.GetSession();
    AuthType type=session->GetAuthType();
    if(type==AUTH_TYPE_SESSION || type==AUTH_TYPE_PERSISTENT)
        this->mSocket->AddMsgTransform(new CryptTransform(session));
    this->Finish(WS_Success);
}


void PeerAuthServerOp::CleanupHook()
{
    AsyncOpWithTracker::CleanupHook();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e8a40
// (protected: virtual void * __thiscall WONAPI::PeerAuthServerOp::`scalar deleting destructor'(unsigned int)    [??_GPeerAuthServerOp@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e8a60
// (public: __thiscall WONAPI::PeerAuthServer::~PeerAuthServer(void)    [??1PeerAuthServer@WONAPI@@QAE@XZ])
