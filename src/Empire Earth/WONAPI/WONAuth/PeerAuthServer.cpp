// PeerAuthServer: server side of the WON peer authentication handshake
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string.h>
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
// FUNCTION: WON 0x006e7d30
// Starts a handshake with our peer data and length-field size, waiting for a request
void PeerAuthServer::Start(const AuthPeerData* data,unsigned char lengthFieldSize)
{
    this->mPeerData=data;
    this->mLengthFieldSize=lengthFieldSize;
    this->mState=STATE_AWAITING_REQUEST;
}

// FUNCTION: WON 0x006e7dd0
// Builds challenge 1: a new secret encrypted with the client key, plus our certificate
WONStatus PeerAuthServer::GetChallenge1(ByteBufferPtr& challenge)
{
    WriteBuffer msg(this->mLengthFieldSize,true);
    msg.AppendLong(203);
    msg.AppendLong(51);
    this->mSecretB.Create(8);
    WriteBuffer secret(0,true);
    secret.AppendShort(this->mSecretB.GetKeyLen());
    secret.AppendBytes(this->mSecretB.GetKey(),this->mSecretB.GetKeyLen());

    ByteBufferPtr encrypted=this->mClientCertificate->GetPubKey().Encrypt(secret.mData,secret.mLength);
    if(encrypted.get()==NULL)
        return WS_PeerAuthServer_FailedToEncryptWithClientPubKey;

    msg.AppendShort(encrypted->length());
    msg.AppendBytes(encrypted->data(),encrypted->length());
    if(this->mUseAuth2)
        msg.AppendBuffer(this->mPeerData->GetCertificate2()->GetRawBuf().get(),2);
    else
        msg.AppendBuffer(this->mPeerData->GetCertificate()->GetRawBuf().get(),2);
    challenge=msg.ToByteBuffer(true);
    return WS_Success;
}

// FUNCTION: WON 0x006e80a0
// Checks the client's auth request and certificate, then builds challenge 1; returns the status
WONStatus PeerAuthServer::HandleRequest(ReadBuffer& request,ByteBufferPtr& challenge)
{
    unsigned char authMode=request.ReadByte();
    unsigned char encryptType=request.ReadByte();
    request.ReadShort(); // encryption flags are consumed even though this version ignores them
    if(authMode!=1)
        return WS_PeerAuthServer_InvalidAuthMode;
    if(encryptType!=0 && encryptType!=1)
        return WS_PeerAuthServer_InvalidEncryptType;
    bool encrypted=encryptType==1;
    this->mAuthType=encrypted?AUTH_TYPE_PERSISTENT:AUTH_TYPE_PERSISTENT_NOCRYPT;

    unsigned short len=request.ReadShort();
    if(this->mUseAuth2)
        this->mClientCertificate=new Auth2Certificate(request.ReadBytes(len),len);
    else
        this->mClientCertificate=new AuthCertificate(request.ReadBytes(len),len);

    if(!this->mClientCertificate->IsValid())
        return WS_PeerAuthServer_InvalidClientCertificate;
    if(this->mClientCertificate->IsExpired(this->mPeerData->GetAuthDelta()))
        return WS_PeerAuthServer_ExpiredClientCertificate;
    if(!this->mPeerData->Verify(this->mClientCertificate.get()))
        return WS_PeerAuthServer_FailedToVerifyClientCertificate;
    return this->GetChallenge1(challenge);
}

// FUNCTION: WON 0x006e8300
// Builds the complete message (error text, or the client secret encrypted) and creates the session on success
WONStatus PeerAuthServer::GetComplete(WONStatus status,ByteBufferPtr& complete)
{
    this->mState=STATE_NOT_STARTED;
    WriteBuffer msg(this->mLengthFieldSize,true);
    msg.AppendLong(203);
    msg.AppendLong(53);
    if(status!=0)
    {
        msg.AppendShort(-4); // WS_CommServ_InvalidParameters
        msg.AppendShort(1);
        msg.AppendString(WONStatusToString(status));
    }
    else
    {
        msg.AppendShort(0);
        WriteBuffer secret(0,true);
        secret.AppendShort(this->mSecretA.GetKeyLen());
        secret.AppendBytes(this->mSecretA.GetKey(),this->mSecretA.GetKeyLen());
        ByteBufferPtr encrypted=this->mClientCertificate->GetPubKey().Encrypt(secret.mData,secret.mLength);
        if(encrypted.get()==NULL)
            return WS_PeerAuthServer_FailedToEncryptWithClientPubKey;
        msg.AppendShort(encrypted->length());
        msg.AppendBytes(encrypted->data(),encrypted->length());
        this->mSession=new AuthSession(this->mAuthType,0,this->mSecretB,this->mLengthFieldSize);
    }
    complete=msg.ToByteBuffer(true);
    return WS_Success;
}

// FUNCTION: WON 0x006e8630
// Decrypts challenge 2, checks our secret came back, stores the client secret and builds complete
WONStatus PeerAuthServer::HandleChallenge2(ReadBuffer& challenge,ByteBufferPtr& complete)
{
    unsigned short encryptedLen=challenge.ReadShort();
    ByteBufferPtr decrypted=this->mPeerData->GetPrivateKey().Decrypt(challenge.ReadBytes(encryptedLen),encryptedLen);
    if(decrypted.get()==NULL)
        return WS_PeerAuthServer_FailedToDecryptWithPrivateKey;
    ReadBuffer msg(decrypted->data(),decrypted->length());
    unsigned short secretBLen=msg.ReadShort();
    if(secretBLen!=this->mSecretB.GetKeyLen() || memcmp(this->mSecretB.GetKey(),msg.ReadBytes(secretBLen),secretBLen)!=0)
        return WS_PeerAuthServer_InvalidSecretB;
    if(!this->mSecretA.SetKey(msg.mData+msg.mPos,msg.mLength-msg.mPos))
        return WS_PeerAuthServer_InvalidSecretA;
    return this->GetComplete(0,complete);
}

// FUNCTION: WON 0x006e87c0
// Dispatches a received handshake message by state; on failure builds a failure complete message
WONStatus PeerAuthServer::HandleRecvMsg(const void* data,unsigned long len,ByteBufferPtr& output)
{
    output=NULL;
    if(this->mState==STATE_NOT_STARTED)
        return WS_PeerAuthServer_NotStarted;
    WONStatus status;
    try
    {
        ReadBuffer msg(data,len);
        DWORD serviceType=msg.ReadLong();
        DWORD messageType=msg.ReadLong();
        if(serviceType!=203)
            return WS_PeerAuthServer_InvalidServiceType;
        if(messageType==50)
        {
            if(this->mState!=STATE_AWAITING_REQUEST)
                return WS_PeerAuthServer_UnexpectedRequest;
            status=this->HandleRequest(msg,output);
            this->mState=STATE_AWAITING_CHALLENGE2;
        }
        else if(messageType==52)
        {
            if(this->mState!=STATE_AWAITING_CHALLENGE2)
                return WS_PeerAuthServer_UnexpectedChallenge;
            status=this->HandleChallenge2(msg,output);
        }
        else
            return WS_PeerAuthServer_InvalidMessage;
    }
    catch(ReadBufferException&)
    {
        status=100043; // WS_PeerAuthServer_MsgUnpackFailure
    }
    if(status!=0)
        this->GetComplete(status,output);
    return status;
}


PeerAuthServer::PeerAuthServer()
{
    this->mState=STATE_NOT_STARTED;
    this->mUseAuth2=true;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e7cc0
// (public: __thiscall WONAPI::PeerAuthServer::PeerAuthServer(void)    [??0PeerAuthServer@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006e8060
// (public: class WONAPI::SmartPtr<class WONAPI::ByteBuffer> __thiscall WONAPI::AuthBase::GetRawBuf(void)const     [?GetRawBuf@AuthBase@WONAPI@@QBE?AV?$Sm)
