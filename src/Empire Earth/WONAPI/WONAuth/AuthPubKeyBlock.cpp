// AuthPubKeyBlock: the auth servers' public key block, unpacked and checked by ee_won (rust/crates/ee_won/src/auth.rs)
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
// The original's destructor is at 0x006e2350 (empty body; the key list cleans up)
AuthPubKeyBlock::~AuthPubKeyBlock() {}
// FUNCTION: WON 0x006e2400
// Takes the key block id and copies of its public keys from the unpacked block
void AuthPubKeyBlock::UnpackHook()
{
    this->mKeyBlockId=this->GetHeader().key_block_id;
    this->mKeyBlock.clear();
    size_t count=ee_won_auth_key_count(this->GetParsed());
    for(size_t i=0;i<count;++i) {
        ElGamal key;
        key.Adopt(ee_won_auth_key(this->GetParsed(),i));
        this->mKeyBlock.push_back(key);
    }
}
// FUNCTION: WON 0x006e26e0
// Returns true when the block and the given auth block are valid and any key verifies its signature
bool AuthPubKeyBlock::Verify(const AuthBase* base) const
{
    if(this->GetParsed()==NULL || base==NULL || base->GetParsed()==NULL) return false;
    return ee_won_auth_verify_with_block(this->GetParsed(),base->GetParsed());
}
// FUNCTION: WON 0x006e2740
// Returns whether the block is valid and signed by the given key
bool AuthPubKeyBlock::IsValidAndVerified(const ElGamal& verifier) const
{
    return this->IsValid() && verifier.Verify(this->GetData(),this->GetDataLen(),this->GetSig(),this->GetSigLen());
}
}
