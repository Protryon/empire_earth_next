// GetCertOp: logs in to the auth server and fetches the certificates, key block and private key
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
// FUNCTION: WON 0x006e3c30
// Creates a certificate request on the auth context's server list
GetCertOp::GetCertOp(AuthContext* context)
    :ServerRequestOp(context->GetServerContext().get()),mAuthContext(context)
{
    this->mCreateAccount=false;
}
// FUNCTION: WON 0x006e3d90
// Destroys the request (members only)
GetCertOp::~GetCertOp()
{
}

// FUNCTION: WON 0x006e3f40
// Starts the request with the current key block and saved secrets; fails at once without a signing key
void GetCertOp::RunHook()
{
    if(!this->mAuthContext->GetVerifierKey().IsPublic())
    {
        this->Finish(WS_GetCert_NeedVerifierKey);
        return;
    }

    ConstSmartPtr<AuthPeerData> peerData=this->mAuthContext->GetPeerData();
    this->mKeyBlock=peerData->GetPubKeyBlock();
    this->mAuthDelta=peerData->GetAuthDelta();
    this->mAuthContext->LoadSecretList();
    ServerRequestOp::RunHook();
}

// FUNCTION: WON 0x006e4270
// Builds the request for the auth servers' public key block
WONStatus GetCertOp::SendPubKeyRequest()
{
    this->mState=GETTING_PUB_KEYS;
    WriteBuffer msg(4,true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(1);
    this->mRequest=msg.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006e4380
// Builds the login request: a new session key encrypted with the server key, then the encrypted login data
WONStatus GetCertOp::SendLoginRequest()
{
    WriteBuffer msg(4,true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(3);
    msg.AppendShort(this->mKeyBlock->GetId());

    const ElGamal& pubKey=this->mKeyBlock->GetCurrentKey();
    this->mSessionKey.Create(8);
    ByteBufferPtr encryptedSessionKey=pubKey.Encrypt(this->mSessionKey.GetKey(),this->mSessionKey.GetKeyLen());
    if(encryptedSessionKey.get()==NULL)
        return WS_GetCert_PubKeyEncryptFailure;

    msg.AppendShort(encryptedSessionKey->length());
    msg.AppendBytes(encryptedSessionKey->data(),encryptedSessionKey->length());

    WriteBuffer loginData(0,true);
    loginData.AppendBytes(this->mSessionKey.GetKey(),2);
    loginData.AppendShort(this->mKeyBlock->GetId());
    int flags=0x0002|0x0004|0x0008;
    if(this->mCreateAccount)
        flags|=0x0001;
    loginData.AppendLong(flags);
    loginData.AppendWString(this->mAuthContext->GetUserName(),2);
    loginData.AppendWString(this->mAuthContext->GetPassword(),2);
    loginData.AppendWString(this->mNewPassword,2);
    this->mAuthContext->AppendCommunityData(loginData);

    ByteBufferPtr encryptedLoginData=this->mSessionKey.Encrypt(loginData.mData,loginData.mLength);
    msg.AppendShort(encryptedLoginData->length());
    msg.AppendBytes(encryptedLoginData->data(),encryptedLoginData->length());
    this->mRequest=msg.ToByteBuffer(true);
    this->mState=GETTING_CHALLENGE;
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006e46a0
// Builds the login confirmation carrying the file hashes keyed with the challenge
WONStatus GetCertOp::SendLoginConfirm()
{
    WriteBuffer msg(4,true);
    msg.AppendByte(5);
    msg.AppendShort(12);
    msg.AppendShort(5);

    WriteBuffer dataBlock(0,true);
    this->mAuthContext->AppendHashes(dataBlock,this->mChallengeSeed);
    ByteBufferPtr encrypted=this->mSessionKey.Encrypt(dataBlock.mData,dataBlock.mLength);
    msg.AppendShort(encrypted->length());
    msg.AppendBytes(encrypted->data(),encrypted->length());
    this->mRequest=msg.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}
namespace {
enum {
    WS_GetCert_DecryptFailure=100074,
    WS_GetCert_InvalidCertificate=100072,
    WS_GetCert_InvalidPrivateKey=100075,
    WS_GetCert_InvalidPubKeyBlock=100073,
    WS_GetCert_InvalidSecretConfirm=100076,
    WS_GetCert_MissingCertificate=100077,
    WS_GetCert_UnexpectedLoginChallenge=100069,
    WS_ServerReq_Send=100058,
    WS_ServerReq_TryNextServer=100057,
    WS_Success=0
};
}

// FUNCTION: WON 0x006e4050
// On failure hands a valid key block to the auth context when it has none, then cleans up the request
void GetCertOp::CleanupHook()
{
	// Give the pub key block to the AuthContext if we managed to at least get that
	if(this->mStatus!=WS_Success && this->mKeyBlock->IsValid() && !this->mAuthContext->GetPeerData()->GetPubKeyBlock()->IsValid())
		this->mAuthContext->SetPeerData(new AuthPeerData(this->mKeyBlock));

	ServerRequestOp::CleanupHook();
}

// FUNCTION: WON 0x006e4130
// Returns to the key-fetch state and clears the challenge, errors and community status list
void GetCertOp::Reset()
{
	this->mState = GETTING_PUB_KEYS;
	this->mChallengeSeed.erase();
	this->mErrorList.clear();
	this->mCommunityStatusList.clear();
}

// FUNCTION: WON 0x006e4850
// Picks the next message: key block request, login request or login confirmation
WONStatus GetCertOp::GetNextRequest()
{	
 	if(!this->mKeyBlock->IsValid()) 
		return this->SendPubKeyRequest();
	else if(this->mState==GETTING_CHALLENGE)
		return this->SendLoginConfirm();
	else
		return this->SendLoginRequest();
}

// FUNCTION: WON 0x006e4880
// Reads the login reply's errors, certificates and encrypted private key and installs the new peer data
WONStatus GetCertOp::HandleLoginReply(ReadBuffer &theMsg)
{

	ConstSmartPtr<AuthCertificate> aCert1;
	ConstSmartPtr<Auth2Certificate> aCert2;
	ElGamal aPrivKey;
	time_t anAuthDelta = 0;

	WONStatus aStatus = (WONStatus)theMsg.ReadShort();
	if(aStatus<0) // Failure status
	{
		if(!this->mChallengeSeed.empty())
			this->mAuthContext->RemoveSecret(this->mChallengeSeed.data());

		if(aStatus==-1) // unknown auth error
		{
			this->SetLastServerError(aStatus);
			return WS_ServerReq_TryNextServer;
		}

//		return aStatus;
	}
	
	unsigned char anErrorCount = theMsg.ReadByte();
	unsigned long i;
	const char *data;
	for(i=0; i<anErrorCount; i++)
	{
		std::string anErrorString;
		theMsg.ReadString(anErrorString);
		this->mErrorList.push_back(anErrorString);
	}
	
	unsigned char aNumClearEntries = theMsg.ReadByte();

	for(i=0; i<aNumClearEntries; i++)
	{
		unsigned char aType = theMsg.ReadByte();
		unsigned short aLen = theMsg.ReadShort();
		
		data = static_cast<const char*>(theMsg.ReadBytes(aLen));
		switch(aType)
		{
			case 1:	
			{
				aCert1 = new AuthCertificate(data,aLen);
				if(!aCert1->IsValid() || !this->mKeyBlock->Verify(aCert1)) 
				{
					this->SetLastServerError(WS_GetCert_InvalidCertificate);
					return WS_ServerReq_TryNextServer; 
				}
				else
					anAuthDelta = aCert1->ComputeAuthDelta();
			}
			break;
			
			case 3: 
			{
				ConstSmartPtr<AuthPubKeyBlock> aKeyBlock = new AuthPubKeyBlock(data,aLen);
				if(!aKeyBlock->IsValidAndVerified(this->mAuthContext->GetVerifierKey()))
				{
					this->SetLastServerError(WS_GetCert_InvalidPubKeyBlock);
					return WS_ServerReq_TryNextServer;
				}
				else
					this->mKeyBlock = aKeyBlock;	
			}
			break;

			case 6:
			{
				aCert2 = new Auth2Certificate(data,aLen);
				if(!aCert2->IsValid() || !this->mKeyBlock->Verify(aCert2))
				{
					this->SetLastServerError(WS_GetCert_InvalidCertificate);
					return WS_ServerReq_TryNextServer; 
				}
				else
					anAuthDelta = aCert2->ComputeAuthDelta();
			}
			break;

			case 7:
			{
				ReadBuffer aSubMsg(data,aLen);
				this->mCommunityStatusList.push_back(COMMUNITY_STATUS());
				COMMUNITY_STATUS &aCommunityStatus = this->mCommunityStatusList.back();

				aCommunityStatus.mStatus = aSubMsg.ReadShort();
				aCommunityStatus.mCommunityID = aSubMsg.ReadLong();
				aSubMsg.ReadWString(aCommunityStatus.mCommunityName,2);
			}
			break;
		}
	}
	
	if(aStatus<0)
		return aStatus;

	// Commit secrets and clean up secret file using auth server clock
	time_t anAuthTime = 0, aCertDuration;
	if(aCert2.get()!=NULL)
	{
		anAuthTime = aCert1->GetIssueTime();
		aCertDuration = aCert1->GetExpireTime() - aCert1->GetIssueTime();
	}
	else if(aCert1.get()!=NULL)
	{
		anAuthTime = aCert2->GetIssueTime();
		aCertDuration = aCert2->GetExpireTime() - aCert2->GetIssueTime();
	}

	this->mAuthContext->CommitSecrets(anAuthTime, aCertDuration);

	
	int aNumLeft = theMsg.length() - theMsg.mPos;

	ByteBufferPtr aDecryptBuf = this->mSessionKey.Decrypt(theMsg.ReadBytes(aNumLeft),aNumLeft);
	if(aDecryptBuf.get()==NULL)
	{
		this->SetLastServerError(WS_GetCert_DecryptFailure);
		return WS_ServerReq_TryNextServer;
	}
	
	ReadBuffer aDecrypt(aDecryptBuf->data(),aDecryptBuf->length());		
	unsigned char aNumCryptEntries = aDecrypt.ReadByte();
	
	for(i=0; i<aNumCryptEntries; i++)
	{
		unsigned char aType = aDecrypt.ReadByte();
		unsigned short aLen = aDecrypt.ReadShort();
		data = static_cast<const char*>(aDecrypt.ReadBytes(aLen));
		switch(aType)
		{
			case 2: 
				if(!aPrivKey.SetPrivateKey(data,aLen))
				{
					this->SetLastServerError(WS_GetCert_InvalidPrivateKey);
					return WS_ServerReq_TryNextServer;
				}
				break;

			case 4:
				if((aLen!=this->mSessionKey.GetKeyLen()+2) || memcmp(data+2,this->mSessionKey.GetKey(),aLen-2)!=0)
				{
					this->SetLastServerError(WS_GetCert_InvalidSecretConfirm);
					return WS_ServerReq_TryNextServer;
				}
				break;

			case 5: 
				//ReadNickNameInfo(aDecryptIn);
				break;
		}
	}

	if(aCert1.get()==NULL || aCert2.get()==NULL)
	{
		this->SetLastServerError(WS_GetCert_MissingCertificate);
		return WS_ServerReq_TryNextServer;
	}

	this->mPeerData = new AuthPeerData(aCert1, this->mKeyBlock, aPrivKey, anAuthDelta, aCert2);
	if(!this->mNewPassword.empty())
		this->mAuthContext->SetPassword(this->mNewPassword);

	this->mAuthContext->SetPeerData(this->mPeerData.get());	
	this->mAuthContext->ClearOneTimeData();
	return WS_Success;
}

// FUNCTION: WON 0x006e5800
// Decrypts the login challenge and stores its 16-byte seed as a new login secret
WONStatus GetCertOp::HandleLoginChallenge(ReadBuffer &theMsg)
{
	unsigned short aLen = theMsg.ReadShort();
	ByteBufferPtr aDecrypt = this->mSessionKey.Decrypt(theMsg.ReadBytes(aLen),aLen);
	if(aDecrypt.get()==NULL)
	{
		this->SetLastServerError(WS_GetCert_DecryptFailure);
		return WS_ServerReq_TryNextServer;
	}

	ReadBuffer aBuf(aDecrypt->data(),aDecrypt->length());
	this->mChallengeSeed.assign(static_cast<const unsigned char*>(aBuf.ReadBytes(16)),16);
	this->mAuthContext->AddSecret(this->mChallengeSeed.data());
	return WS_ServerReq_Send;
}

// FUNCTION: WON 0x006e5980
// Checks the auth server reply and dispatches it to the key block, challenge or login reply handler
WONStatus GetCertOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned short aServiceType = aMsg.ReadShort();
	unsigned short aMessageType = aMsg.ReadShort();

	if(aHeaderType!=5 || aServiceType!=12)
		return this->InvalidReplyHeader();

	if(this->mState==GETTING_PUB_KEYS)
	{
		if(aMessageType!=2) // GetPubKeysReply
			return this->InvalidReplyHeader();

		WONStatus aStatus = (WONStatus)aMsg.ReadShort();
		if(aStatus<0) // shouldn't get this
		{
			this->SetLastServerError(aStatus);
			return WS_ServerReq_TryNextServer;
		}

		unsigned short aLen = aMsg.ReadShort();
		ConstSmartPtr<AuthPubKeyBlock> aKeyBlock = new AuthPubKeyBlock(aMsg.ReadBytes(aLen),aLen);
		if(!aKeyBlock->IsValidAndVerified(this->mAuthContext->GetVerifierKey()))
		{
			this->SetLastServerError(WS_GetCert_InvalidPubKeyBlock);
			return WS_ServerReq_TryNextServer;
		}
		else
		{
			this->mKeyBlock = aKeyBlock;
			return WS_ServerReq_Send;
		}
	}

	
	
	if(aMessageType==6)
		return this->HandleLoginReply(aMsg);
	
	if(aMessageType!=4)
		return this->InvalidReplyHeader();

	if(this->mState!=GETTING_CHALLENGE)
	{
		this->SetLastServerError(WS_GetCert_UnexpectedLoginChallenge);
		return WS_ServerReq_TryNextServer;
	}

	return this->HandleLoginChallenge(aMsg);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e3d70
// (protected: virtual void * __thiscall WONAPI::GetCertOp::`scalar deleting destructor'(unsigned int)    [??_GGetCertOp@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006e57c0
// (public: __thiscall WONAPI::GetCertOp::COMMUNITY_STATUS::~COMMUNITY_STATUS(void)    [??1COMMUNITY_STATUS@GetCertOp@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e5760
// (public: __thiscall WONAPI::AuthCertificateBase::AuthCertificateBase(void const *,unsigned long)    [??0AuthCertificateBase@WONAPI@@QAE@PBXK@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9820
// (public: __thiscall std::list<class WONAPI::ElGamal,class std::allocator<class WONAPI::ElGamal> >::list<class WONAPI::ElGamal,class std::allocator<clas)
// TEMPLATE: WON 0x006ea0d0
// (public: __thiscall std::vector<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<class std:)
// TEMPLATE: WON 0x006ebdd0
// (public: class std::list<struct WONAPI::GetCertOp::COMMUNITY_STATUS,class std::allocator<struct WONAPI::GetCertOp::COMMUNITY_STATUS> >::iterator __this)
// TEMPLATE: WON 0x006ef690
// (protected: void __thiscall std::vector<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<cl)
// TEMPLATE: WON 0x006f00f0
// (public: void * __thiscall WONAPI::GetCertOp::COMMUNITY_STATUS::`scalar deleting destructor'(unsigned int)    [??_GCOMMUNITY_STATUS@GetCertOp@WONAPI@@Q)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ebd70
// (public: void __thiscall std::list<struct WONAPI::GetCertOp::COMMUNITY_STATUS,class std::allocator<struct WONAPI::GetCertOp::COMMUNITY_STATUS> >::_Inse)
// TEMPLATE: WON 0x006ef6e0
// (protected: void __thiscall std::list<struct WONAPI::GetCertOp::COMMUNITY_STATUS,class std::allocator<struct WONAPI::GetCertOp::COMMUNITY_STATUS> >::_I)
// TEMPLATE: WON 0x006efca0
// (void __cdecl std::_Construct(struct WONAPI::GetCertOp::COMMUNITY_STATUS *,struct WONAPI::GetCertOp::COMMUNITY_STATUS const &))
// TEMPLATE: WON 0x006eb150
// (protected: struct std::list<class WONAPI::ElGamal,class std::allocator<class WONAPI::ElGamal> >::_Node * __thiscall std::list<class WONAPI::ElGamal,cl)
