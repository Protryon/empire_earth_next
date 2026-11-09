// AuthPubKeyBlock: the auth servers' public key block (unpacking and signature checks)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthPubKeyBlock.h"
namespace WONAPI {
// The original's destructor is at 0x006e2350 (empty body; the key list and ElGamal members clean up)
AuthPubKeyBlock::~AuthPubKeyBlock() {}
// FUNCTION: WON 0x006e2400
// Reads the key block id and its public keys; false when there are none or one is invalid
bool AuthPubKeyBlock::UnpackHook(ReadBuffer& input)
{
    if(!AuthBase::UnpackHook(input)) return false;
    this->mKeyBlockId=input.ReadShort();
    unsigned short count=input.ReadShort();
    if(count==0) return false;
    this->mKeyBlock.clear();
    for(int i=0;i<count;++i) {
        unsigned short keyLength=input.ReadShort();
        ElGamal key;
        if(!key.SetPublicKey(input.ReadBytes(keyLength),keyLength)) return false;
        this->mKeyBlock.push_back(key);
    }
    return true;
}
// FUNCTION: WON 0x006e26e0
// Returns true when the block and the given auth block are valid and any key verifies its signature
bool AuthPubKeyBlock::Verify(const AuthBase* base) const
{
    if(!this->IsValid()) return false;
    if(!base->IsValid()) return false;
    for(KeyBlock::const_iterator key=this->mKeyBlock.begin();key!=this->mKeyBlock.end();++key)
        if(base->Verify(*key)) return true;
    return false;
}
// FUNCTION: WON 0x006e2740
// Returns whether the block is valid and signed by the given key
bool AuthPubKeyBlock::IsValidAndVerified(const ElGamal& verifier) const
{
    return this->IsValid() && verifier.Verify(this->GetData(),this->GetDataLen(),this->GetSig(),this->GetSigLen());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e2350
// (protected: virtual __thiscall WONAPI::AuthPubKeyBlock::~AuthPubKeyBlock(void)    [??1AuthPubKeyBlock@WONAPI@@MAE@XZ])

// Compiler-generated and header-inline functions this unit emits:

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ef990
// (void __cdecl std::_Construct(class WONAPI::ElGamal *,class WONAPI::ElGamal const &))
// TEMPLATE: WON 0x006ed7d0
// (public: class std::list<class WONAPI::ElGamal,class std::allocator<class WONAPI::ElGamal> >::iterator __thiscall std::list<class WONAPI::ElGamal,class)
// TEMPLATE: WON 0x006ed810
// (protected: void __thiscall std::list<class WONAPI::ElGamal,class std::allocator<class WONAPI::ElGamal> >::_Incsize(unsigned int))
