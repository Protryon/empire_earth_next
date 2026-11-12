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
#include "ee/wonapi/WONCertMsg.h"

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

// The requests (WONCertMsg.h over ee_won's cert module)
namespace {
struct CertEnv {
    typedef GetCertOp Op;
    typedef AuthCertificate Cert1;
    typedef Auth2Certificate Cert2;
    typedef AuthPubKeyBlock KeyBlock;
    typedef AuthPeerData PeerData;
    typedef ElGamal PrivKey;
};
}

// FUNCTION: WON 0x006e4270
// Builds the request for the auth servers' public key block
WONStatus GetCertOp::SendPubKeyRequest()
{
    return CertMsg<CertEnv>::SendPubKeyRequest(*this);
}

// FUNCTION: WON 0x006e4380
// Builds the login request: a new session key encrypted with the server key, then the encrypted login data
WONStatus GetCertOp::SendLoginRequest()
{
    return CertMsg<CertEnv>::SendLoginRequest(*this);
}

// FUNCTION: WON 0x006e46a0
// Builds the login confirmation carrying the file hashes keyed with the challenge
WONStatus GetCertOp::SendLoginConfirm()
{
    return CertMsg<CertEnv>::SendLoginConfirm(*this);
}

// FUNCTION: WON 0x006e4050
// On failure hands a valid key block to the auth context when it has none, then cleans up the request
void GetCertOp::CleanupHook()
{
	// Give the pub key block to the AuthContext if we managed to at least get that
	if(this->GetStatus()!=WS_Success && this->mKeyBlock->IsValid() && !this->mAuthContext->GetPeerData()->GetPubKeyBlock()->IsValid())
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

// FUNCTION: WON 0x006e5980
// Checks the auth server reply and handles the key block, the login challenge (0x006e5800) or the login reply
// (0x006e4880): WONCertMsg.h
WONStatus GetCertOp::CheckResponse()
{
    return CertMsg<CertEnv>::CheckResponse(*this);
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
