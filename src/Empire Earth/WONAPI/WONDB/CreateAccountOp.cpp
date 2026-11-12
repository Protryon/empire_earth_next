// WONAPI op that creates a WON account through the DBProxy service (encrypted exchange)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONElGamal.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDBMsg.h"
namespace WONAPI {
// FUNCTION: WON 0x006cbe70
// Sets 4-byte length fields, creates an empty profile and a new 8-byte session key
void CreateAccountOp::Init()
{
    this->mLengthFieldSize=4;
    this->mProfileData=new ProfileData;
    this->mSymKey.Create(8);
}
// FUNCTION: WON 0x006cbf20
// Creates an account-creation request for a server context
CreateAccountOp::CreateAccountOp(ServerContext* inContext)
    : ServerRequestOp(inContext)
{
    this->Init();
}
// FUNCTION: WON 0x006cc4e0
// Restarts the exchange at fetching the server's public key
void CreateAccountOp::Reset()
{
    this->mState=State_GettingPubKey;
}
// FUNCTION: WON 0x006cc4f0
// Builds the next message: public key request, encrypted session key, or the encrypted account data
WONStatus CreateAccountOp::GetNextRequest()
{
    // the messages are Rust's (ee_won db.rs); the keys stay C++
    if (this->mState==State_GettingPubKey) {
        DbRequest q(EE_WON_DB_REQ_CREATE_KEY,this->mLengthFieldSize);
        this->mRequest=q.Build();
        return WS_ServerReq_Recv;
    } else if (this->mState==State_SettingSessionKey) {
        ByteBufferPtr encryptedKey=this->mPubKey.Encrypt(this->mSymKey.GetKey(),this->mSymKey.GetKeyLen());
        DbRequest q(EE_WON_DB_REQ_CREATE_SESSION,this->mLengthFieldSize);
        q.mReq.data=DbRequest::Buffer(encryptedKey.get());
        this->mRequest=q.Build();
        this->mState=State_CreatingAccount;
        return WS_ServerReq_Send;
    } else {
        DbRequest account(EE_WON_DB_REQ_CREATE_DATA,0);
        account.mReq.name=account.Text(this->mUserName);
        account.mReq.password=account.Text(this->mPassword);
        account.mReq.email=DbRequest::Narrow(this->mEmail);
        account.mReq.profile=this->mProfileData->GetProfile();
        ByteBufferPtr aPlain=account.Build();
        ByteBufferPtr encrypted=this->mSymKey.Encrypt(aPlain->data(),aPlain->length());
        DbRequest q(EE_WON_DB_REQ_CREATE_ACCOUNT,this->mLengthFieldSize);
        q.mReq.data=DbRequest::Buffer(encrypted.get());
        this->mRequest=q.Build();
        return WS_ServerReq_Recv;
    }
}
// FUNCTION: WON 0x006cc8d0
// Checks a reply: stores the server's public key and moves on, or returns the account creation status
WONStatus CreateAccountOp::CheckResponse()
{
    // the reply's read is Rust's (ee_won db.rs)
    EeWonDbReply r=DbAccountReply(this->mState==State_GettingPubKey ? EE_WON_DB_CREATE_KEY : EE_WON_DB_CREATE,
                                  this->mResponse.get(),this->mErrorString);
    if (r.verdict==EE_WON_DB_INVALID_HEADER) return this->InvalidReplyHeader();
    if (r.verdict==EE_WON_DB_PUBLIC_KEY) {
        if (!this->mPubKey.SetPublicKey(this->mResponse->data()+r.key_at,r.key_len)) {
            this->SetLastServerError(WS_InvalidPublicKey);
            return WS_ServerReq_TryNextServer;
        }
        this->mState=State_SettingSessionKey;
        return WS_ServerReq_Send;
    }
    return (WONStatus)r.result;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cc010
// (public: __thiscall WONAPI::ElGamal::~ElGamal(void)    [??1ElGamal@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006cc140
// (public: __thiscall WONAPI::Random::~Random(void)    [??1Random@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006cc230
// (public: __thiscall WONAPI::Blowfish::~Blowfish(void)    [??1Blowfish@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006cc2e0
// (public: virtual __thiscall WONAPI::CreateAccountOp::~CreateAccountOp(void)    [??1CreateAccountOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cc210
// (public: virtual void * __thiscall WONAPI::CreateAccountOp::`scalar deleting destructor'(unsigned int)    [??_GCreateAccountOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ced50
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Tidy(bool)    )
