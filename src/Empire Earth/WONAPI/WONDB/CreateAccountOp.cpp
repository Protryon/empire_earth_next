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
    WriteBuffer request(this->mLengthFieldSize,true);
    if (this->mState==State_GettingPubKey) {
        request.AppendByte(5);
        request.AppendShort(14);
        request.AppendShort(2);
        request.AppendShort(1);
        this->mRequest=request.ToByteBuffer(true);
        return WS_ServerReq_Recv;
    } else if (this->mState==State_SettingSessionKey) {
        request.AppendByte(5);
        request.AppendShort(14);
        request.AppendShort(4);
        request.AppendShort(1);
        ByteBufferPtr encryptedKey=this->mPubKey.Encrypt(this->mSymKey.GetKey(),this->mSymKey.GetKeyLen());
        request.AppendBuffer(encryptedKey.get(),2);
        this->mRequest=request.ToByteBuffer(true);
        this->mState=State_CreatingAccount;
        return WS_ServerReq_Send;
    } else {
        request.AppendByte(5);
        request.AppendShort(14);
        request.AppendShort(5);
        request.AppendShort(1);
        WriteBuffer account(0,true);
        account.AppendByte(5);
        account.AppendShort(14);
        account.AppendShort(1002);
        account.AppendShort(2);
        account.AppendWString(this->mUserName,2);
        account.AppendWString(this->mPassword,2);
        account.AppendString(this->mEmail,2);
        this->mProfileData->PackRequestData(account,true);
        ByteBufferPtr encrypted=this->mSymKey.Encrypt(account.data(),account.length());
        request.AppendBuffer(encrypted.get(),0);
        this->mRequest=request.ToByteBuffer(true);
        return WS_ServerReq_Recv;
    }
}
// FUNCTION: WON 0x006cc8d0
// Checks a reply: stores the server's public key and moves on, or returns the account creation status
WONStatus CreateAccountOp::CheckResponse()
{
    ReadBuffer reply(this->mResponse->data(),this->mResponse->length());
    unsigned char header=reply.ReadByte();
    unsigned short service=reply.ReadShort();
    unsigned short message=reply.ReadShort();
    unsigned short subMessage=reply.ReadShort();
    WONStatus status=reply.ReadShort();
    reply.ReadString(this->mErrorString,2);
    reply.ReadByte();
    if (header!=5 || service!=14) return this->InvalidReplyHeader();
    if (status!=0) return status;
    if (this->mState==State_GettingPubKey) {
        if (message!=3) return this->InvalidReplyHeader();
        unsigned short keyLength=reply.ReadShort();
        if (!this->mPubKey.SetPublicKey(reply.ReadBytes(keyLength),keyLength)) {
            this->SetLastServerError(WS_InvalidPublicKey);
            return WS_ServerReq_TryNextServer;
        }
        this->mState=State_SettingSessionKey;
        return WS_ServerReq_Send;
    } else {
        if (message!=1002 || subMessage!=3) return this->InvalidReplyHeader();
        return status;
    }
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
