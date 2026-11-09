// PeerAuthClient: client side of the WON peer authentication handshake (request, challenges, complete)
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
// FUNCTION: WON 0x6e5cc0
// Decrypts the server secret, checks the server certificate and builds challenge 2; returns the status
WONStatus PeerAuthClient::HandleChallenge1(ReadBuffer &theChallenge, ByteBufferPtr &challenge2)
{
	unsigned short aSecretLenWithLen = theChallenge.ReadShort();
	ByteBufferPtr aSecretBuf = this->mPeerData->GetPrivateKey().Decrypt(theChallenge.ReadBytes(aSecretLenWithLen),aSecretLenWithLen);
	if(aSecretBuf.get()==NULL)
		return WS_PeerAuthClient_Challenge1DecryptFailure /* WS_PeerAuthClient_Challenge1DecryptFailure */;
		
	unsigned short aSecretLen = aSecretBuf->data()[0] | (aSecretBuf->data()[1]<<8);
	unsigned long aMaxLen = aSecretBuf->length()-2;
	if(aSecretLen>aMaxLen)
		return WS_PeerAuthClient_Challenge1InvalidSecretLen /* WS_PeerAuthClient_Challenge1InvalidSecretLen */;
		
	this->mSecretA.Create(this->mClientKeySize);		
	const void* aKey = aSecretBuf->data()+2;
	if(!this->mSecretB.SetKey(aKey, aSecretLen))
		return WS_PeerAuthClient_Challenge1InvalidSecretKey /* WS_PeerAuthClient_Challenge1InvalidSecretKey */;

	unsigned short aCertLen = theChallenge.ReadShort();

	if(this->mUseAuth2)
		this->mServerCertificate = new Auth2Certificate(theChallenge.ReadBytes(aCertLen),aCertLen);
	else
		this->mServerCertificate = new AuthCertificate(theChallenge.ReadBytes(aCertLen),aCertLen);

	if(!this->mServerCertificate->IsValid())
		return WS_PeerAuthClient_Challenge1CertificateUnpackFailure /* WS_PeerAuthClient_Challenge1CertificateUnpackFailure */;

	if(!this->mPeerData->Verify(this->mServerCertificate.get()))
		return WS_PeerAuthClient_Challenge1CertificateVerifyFailure /* WS_PeerAuthClient_Challenge1CertificateVerifyFailure */;

	return this->GetChallenge2(challenge2);
}
	

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x6e5fb0
// Builds challenge 2: both secrets encrypted with the server certificate's public key
WONStatus PeerAuthClient::GetChallenge2(ByteBufferPtr &challenge2)
{
	this->mState = STATE_AWAITING_COMPLETE;

	WriteBuffer aMsg(this->mLengthFieldSize,true);
		
	aMsg.AppendLong(203);								// Auth1 Peer To Peer
	aMsg.AppendLong(52);								// Auth1 Challenge 2

	WriteBuffer anEncryptBuf(0,true);
	anEncryptBuf.AppendShort(this->mSecretB.GetKeyLen());
	anEncryptBuf.AppendBytes(this->mSecretB.GetKey(), this->mSecretB.GetKeyLen());
	anEncryptBuf.AppendBytes(this->mSecretA.GetKey(), this->mSecretA.GetKeyLen());

	ByteBufferPtr anEncrypt = this->mServerCertificate->GetPubKey().Encrypt(anEncryptBuf.mData,anEncryptBuf.mLength);
	if(anEncrypt.get()==NULL)
		return WS_PeerAuthClient_Challenge2EncryptFailure /* WS_PeerAuthClient_Challenge2EncryptFailure */;

	aMsg.AppendShort(anEncrypt->length());
	aMsg.AppendBytes(anEncrypt->data(),anEncrypt->length());
	challenge2 = aMsg.ToByteBuffer(true);
	return WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x6e61d0
// Checks the server's complete message echoes our secret and creates the session; returns the status
WONStatus PeerAuthClient::HandleComplete(ReadBuffer &theComplete)
{
	short aStatus = theComplete.ReadShort();
	if(aStatus<0)
	{		
		unsigned short aNumErrors = theComplete.ReadShort();
		for(int i=0; i<aNumErrors; i++)
		{
			std::string anError;
			theComplete.ReadString(anError);
		}
		
		return (WONStatus)aStatus;
	}
		
	unsigned short aLen = theComplete.ReadShort();
	ByteBufferPtr aDecrypt = this->mPeerData->GetPrivateKey().Decrypt(theComplete.ReadBytes(aLen),aLen);
	if(aDecrypt.get()==NULL)
		return WS_PeerAuthClient_CompleteDecryptFailure /* WS_PeerAuthClient_CompleteDecryptFailure */;

	if(aDecrypt->length()<2)
		return WS_PeerAuthClient_CompleteInvalidSecretLen /* WS_PeerAuthClient_CompleteInvalidSecretLen */;
	
	aLen = (aDecrypt->data()[0] | (aDecrypt->data()[1]<<8));
	if(aLen>aDecrypt->length()-2 || aLen!=this->mSecretA.GetKeyLen() || memcmp(this->mSecretA.GetKey(),aDecrypt->data()+2,aLen)!=0)
		return WS_PeerAuthClient_CompleteInvalidSecretKey /* WS_PeerAuthClient_CompleteInvalidSecretKey */;	

	unsigned short aSessionId = 0;
	if(this->mAuthType==AUTH_TYPE_SESSION)
		aSessionId = theComplete.ReadShort();

	if(this->mAuthType!=AUTH_TYPE_PERSISTENT_NOCRYPT)
		this->mSession = new AuthSession(this->mAuthType, aSessionId, this->mUseClientSecret?this->mSecretA:this->mSecretB, this->mLengthFieldSize);

	return WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x6e6730
// Builds the peer auth request with the auth type, encryption flags and our certificate
ByteBufferPtr PeerAuthClient::Start(const AuthPeerData *theData, AuthType theType, unsigned char theLengthFieldSize)
{
	this->mState = STATE_AWAITING_CHALLENGE;

	this->mPeerData = theData;
	this->mAuthType = theType;
	this->mLengthFieldSize = theLengthFieldSize;

	bool sessioned = (this->mAuthType==AUTH_TYPE_SESSION);
	bool encrypted = (sessioned || this->mAuthType==AUTH_TYPE_PERSISTENT);

	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendLong(203);								// Auth1 Peer To Peer
	aMsg.AppendLong(50);								// Auth1 Request


	aMsg.AppendByte(sessioned ? 2 : 1);					// 1 = persistent, 2 = sessioned
	aMsg.AppendByte(encrypted ? 1 : 0);					// Encrypt Blowfish

	int anEncryptFlags = 0;
	if(!sessioned) anEncryptFlags |= 0x0001;
	if(this->mUseClientSecret) anEncryptFlags |= 0x8000;
	aMsg.AppendShort(anEncryptFlags);					// 0x0000 None
														// 0x0001 Don't Use sequence numbers
														// 0x0002 Allow clear messages
														// 0x8000 Use client specified session key

	const AuthCertificateBase *aCert = this->mPeerData->GetCertificate2();
	if(aCert==NULL || !this->mUseAuth2)
	{
		this->mUseAuth2 = false;
		aCert = this->mPeerData->GetCertificate();
	}

	unsigned short aCertLen = aCert->GetRawLen();
	aMsg.AppendShort(aCertLen);
	aMsg.AppendBytes(aCert->GetRaw(),aCertLen);
	return aMsg.ToByteBuffer(true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x6e68d0
// Dispatches a received handshake message to the challenge or complete handler; returns the status
WONStatus PeerAuthClient::HandleRecvMsg(const void *inMsg, unsigned long inMsgLen, ByteBufferPtr &outMsg)
{
	outMsg = NULL;

	try
	{
		ReadBuffer aMsg(inMsg,inMsgLen);
		DWORD aServiceType = aMsg.ReadLong();
		DWORD aMessageType = aMsg.ReadLong();

		if(aServiceType!=203) // Auth 1 peer to peer
			return WS_PeerAuthClient_InvalidServiceType /* WS_PeerAuthClient_InvalidServiceType */;

		if(aMessageType==53)
			return this->HandleComplete(aMsg);
		else if(aMessageType==51)
		{
			if(this->mState!=STATE_AWAITING_CHALLENGE)
				return WS_PeerAuthClient_UnexpectedChallenge /* WS_PeerAuthClient_UnexpectedChallenge */;
			
			return this->HandleChallenge1(aMsg, outMsg);
		}
		else
			return WS_PeerAuthClient_InvalidMessage /* WS_PeerAuthClient_InvalidMessage */;
	}
	catch(ReadBufferException&)
	{
		return WS_PeerAuthClient_MsgUnpackFailure /* WS_PeerAuthClient_MsgUnpackFailure */;
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e6450
// (public: __thiscall WONAPI::Blowfish::Blowfish(class WONAPI::Blowfish const &))
