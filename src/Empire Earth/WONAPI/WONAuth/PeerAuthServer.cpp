// PeerAuthServer: server side of the WON peer authentication handshake. The messages, the state and the checks are
// ee_won's (rust/crates/ee_won/src/peerauth.rs, through include/ee/wonapi/WONPeerAuthMsg.h); the certificates, the
// peer data's keys and the two secrets stay this class's, reached as hooks.
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
#include "ee/wonapi/WONAuthCertificate.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONPeerAuthMsg.h"

namespace WONAPI {
// FUNCTION: WON 0x006e7d30
// Starts a handshake with our peer data and length-field size, waiting for a request
void PeerAuthServer::Start(const AuthPeerData* data,unsigned char lengthFieldSize)
{
    this->mPeerData=data;
    PeerAuthMsg<PeerAuthServer>::ServerStart(*this,lengthFieldSize);
}

// FUNCTION: WON 0x006e87c0
// Dispatches a received handshake message by state (the request 0x6e80a0 and challenge 1 0x6e7dd0, challenge 2
// 0x6e8630, the complete 0x6e8300); on failure builds a failure complete message
WONStatus PeerAuthServer::HandleRecvMsg(const void* data,unsigned long len,ByteBufferPtr& output)
{
    output=NULL;
    return PeerAuthMsg<PeerAuthServer>::ServerRecv(*this,data,len,WonByteBufferSink,&output);
}

PeerAuthServer::PeerAuthServer()
{
    this->mState=STATE_NOT_STARTED;
    this->mUseAuth2=true;
}

// ---- the hooks (WONPeerAuthMsg.h): the original's statements on the certificates, keys and secrets
bool PeerAuthServer::PaPrivateDecrypt(const void* data,unsigned long len,std::string& out)
{
    ByteBufferPtr buf=this->mPeerData->GetPrivateKey().Decrypt(data,len);
    if(buf.get()==NULL)
        return false;
    out.assign(buf->data(),buf->length());
    return true;
}
bool PeerAuthServer::PaMakeCert(bool auth2,const void* data,unsigned long len)
{
    if(auth2)
        this->mClientCertificate=new Auth2Certificate(data,len);
    else
        this->mClientCertificate=new AuthCertificate(data,len);
    return this->mClientCertificate->IsValid();
}
bool PeerAuthServer::PaVerifyCert() { return this->mPeerData->Verify(this->mClientCertificate.get()); }
bool PeerAuthServer::PaCertExpired() { return this->mClientCertificate->IsExpired(this->mPeerData->GetAuthDelta()); }
bool PeerAuthServer::PaCertEncrypt(const void* data,unsigned long len,std::string& out)
{
    ByteBufferPtr buf=this->mClientCertificate->GetPubKey().Encrypt(data,len);
    if(buf.get()==NULL)
        return false;
    out.assign(buf->data(),buf->length());
    return true;
}
bool PeerAuthServer::PaHasOwnCert2() { return this->mPeerData->GetCertificate2()!=NULL; }
void PeerAuthServer::PaOwnCertRaw(bool auth2,std::string& out)
{
    ByteBufferPtr raw=auth2 ? this->mPeerData->GetCertificate2()->GetRawBuf() : this->mPeerData->GetCertificate()->GetRawBuf();
    if(raw.get()!=NULL && raw->length()>0)
        out.assign(raw->data(),raw->length());
}
void PeerAuthServer::PaSecretCreate(int which,int len) { (which ? this->mSecretB : this->mSecretA).Create(len); }
bool PeerAuthServer::PaSecretSetKey(int which,const void* key,int len)
{
    return (which ? this->mSecretB : this->mSecretA).SetKey(key,len);
}
void PeerAuthServer::PaSecretKey(int which,std::string& out)
{
    const Blowfish& key=which ? this->mSecretB : this->mSecretA;
    out.assign(static_cast<const char*>(key.GetKey()),key.GetKeyLen());
}
void PeerAuthServer::PaNewSession(int type,unsigned short id,int which,unsigned char lengthFieldSize)
{
    this->mSession=new AuthSession((AuthType)type,id,which ? this->mSecretB : this->mSecretA,lengthFieldSize);
}
void PeerAuthServer::PaGetState(EeWonPeerState& s)
{
    s.state=this->mState;
    s.auth_type=this->mAuthType;
    s.length_field_size=this->mLengthFieldSize;
    s.use_auth2=this->mUseAuth2;
    s.use_client_secret=false;
    s.client_key_size=0;
}
void PeerAuthServer::PaSetState(const EeWonPeerState& s)
{
    this->mState=(PeerAuthServerState)s.state;
    this->mAuthType=(AuthType)s.auth_type;
    this->mLengthFieldSize=s.length_field_size;
    this->mUseAuth2=s.use_auth2;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e7cc0
// (public: __thiscall WONAPI::PeerAuthServer::PeerAuthServer(void)    [??0PeerAuthServer@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006e8060
// (public: class WONAPI::SmartPtr<class WONAPI::ByteBuffer> __thiscall WONAPI::AuthBase::GetRawBuf(void)const     [?GetRawBuf@AuthBase@WONAPI@@QBE?AV?$Sm)
