// PeerAuthClient: client side of the WON peer authentication handshake (request, challenges, complete). The messages,
// the state and the checks are ee_won's (rust/crates/ee_won/src/peerauth.rs, through include/ee/wonapi/
// WONPeerAuthMsg.h); the certificates, the peer data's keys and the two secrets stay this class's, reached as hooks.
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
// FUNCTION: WON 0x006e5c30
// Creates an idle client that prefers auth2 certificates and an 8-byte server-chosen session key
PeerAuthClient::PeerAuthClient()
{
    this->Reset();
    this->mUseAuth2=true;
    this->mUseClientSecret=false;
    this->mClientKeySize=8;
}
// FUNCTION: WON 0x006e5cb0
// Returns the handshake to the not-started state
void PeerAuthClient::Reset()
{
    this->mState=STATE_NOT_STARTED;
}

// FUNCTION: WON 0x6e6730
// Builds the peer auth request with the auth type, encryption flags and our certificate
ByteBufferPtr PeerAuthClient::Start(const AuthPeerData *theData, AuthType theType, unsigned char theLengthFieldSize)
{
	this->mPeerData = theData;
	ByteBufferPtr aMsg;
	PeerAuthMsg<PeerAuthClient>::ClientStart(*this, theType, theLengthFieldSize, WonByteBufferSink, &aMsg);
	return aMsg;
}

// FUNCTION: WON 0x6e68d0
// Dispatches a received handshake message to the challenge or complete handler (0x6e5cc0, 0x6e5fb0, 0x6e61d0);
// returns the status
WONStatus PeerAuthClient::HandleRecvMsg(const void *inMsg, unsigned long inMsgLen, ByteBufferPtr &outMsg)
{
	outMsg = NULL;
	return PeerAuthMsg<PeerAuthClient>::ClientRecv(*this, inMsg, inMsgLen, WonByteBufferSink, &outMsg);
}

// ---- the hooks (WONPeerAuthMsg.h): the original's statements on the certificates, keys and secrets
bool PeerAuthClient::PaPrivateDecrypt(const void* theData, unsigned long theLen, std::string& out)
{
	ByteBufferPtr aBuf = this->mPeerData->GetPrivateKey().Decrypt(theData, theLen);
	if(aBuf.get()==NULL)
		return false;
	out.assign(aBuf->data(), aBuf->length());
	return true;
}
bool PeerAuthClient::PaMakeCert(bool auth2, const void* theData, unsigned long theLen)
{
	if(auth2)
		this->mServerCertificate = new Auth2Certificate(theData, theLen);
	else
		this->mServerCertificate = new AuthCertificate(theData, theLen);
	return this->mServerCertificate->IsValid();
}
bool PeerAuthClient::PaVerifyCert() { return this->mPeerData->Verify(this->mServerCertificate.get()); }
bool PeerAuthClient::PaCertExpired() { return this->mServerCertificate->IsExpired(this->mPeerData->GetAuthDelta()); }
bool PeerAuthClient::PaCertEncrypt(const void* theData, unsigned long theLen, std::string& out)
{
	ByteBufferPtr aBuf = this->mServerCertificate->GetPubKey().Encrypt(theData, theLen);
	if(aBuf.get()==NULL)
		return false;
	out.assign(aBuf->data(), aBuf->length());
	return true;
}
bool PeerAuthClient::PaHasOwnCert2() { return this->mPeerData->GetCertificate2()!=NULL; }
void PeerAuthClient::PaOwnCertRaw(bool auth2, std::string& out)
{
	const AuthCertificateBase* aCert = auth2 ? static_cast<const AuthCertificateBase*>(this->mPeerData->GetCertificate2())
	                                         : static_cast<const AuthCertificateBase*>(this->mPeerData->GetCertificate());
	if(aCert->GetRawLen()>0)
		out.assign(static_cast<const char*>(aCert->GetRaw()), aCert->GetRawLen());
}
void PeerAuthClient::PaSecretCreate(int which, int theLen) { (which ? this->mSecretB : this->mSecretA).Create(theLen); }
bool PeerAuthClient::PaSecretSetKey(int which, const void* theKey, int theLen)
{
	return (which ? this->mSecretB : this->mSecretA).SetKey(theKey, theLen);
}
void PeerAuthClient::PaSecretKey(int which, std::string& out)
{
	const Blowfish& aKey = which ? this->mSecretB : this->mSecretA;
	out.assign(static_cast<const char*>(aKey.GetKey()), aKey.GetKeyLen());
}
void PeerAuthClient::PaNewSession(int theType, unsigned short theId, int which, unsigned char theLengthFieldSize)
{
	this->mSession = new AuthSession((AuthType)theType, theId, which ? this->mSecretB : this->mSecretA, theLengthFieldSize);
}
void PeerAuthClient::PaGetState(EeWonPeerState& s)
{
	s.state = this->mState;
	s.auth_type = this->mAuthType;
	s.length_field_size = this->mLengthFieldSize;
	s.use_auth2 = this->mUseAuth2;
	s.use_client_secret = this->mUseClientSecret;
	s.client_key_size = this->mClientKeySize;
}
void PeerAuthClient::PaSetState(const EeWonPeerState& s)
{
	this->mState = (PeerAuthClientState)s.state;
	this->mAuthType = (AuthType)s.auth_type;
	this->mLengthFieldSize = s.length_field_size;
	this->mUseAuth2 = s.use_auth2;
	this->mUseClientSecret = s.use_client_secret;
	this->mClientKeySize = s.client_key_size;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e6450
// (public: __thiscall WONAPI::Blowfish::Blowfish(class WONAPI::Blowfish const &))
