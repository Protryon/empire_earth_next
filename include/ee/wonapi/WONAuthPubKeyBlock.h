// AuthPubKeyBlock (the original's 0x2c bytes): the auth servers' public keys, unpacked by ee_won
// (rust/crates/ee_won/src/auth.rs)
#ifndef EE_WONAPI_WONAUTHPUBKEYBLOCK_H
#define EE_WONAPI_WONAUTHPUBKEYBLOCK_H
#include <list>
#include "ee/wonapi/WONAuthBase.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
class AuthPubKeyBlock : public AuthBase {
    typedef std::list<ElGamal> KeyBlock;
    KeyBlock mKeyBlock; //1c
    unsigned short mKeyBlockId; //28
protected:
    virtual ~AuthPubKeyBlock(); //2e2350
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_PUB_KEY_BLOCK; }
    virtual void UnpackHook(); // the id and the keys
public:
    AuthPubKeyBlock(const void* data = NULL,unsigned long len = 0)
        :AuthBase(data,len),mKeyBlockId(0) { this->Unpack(); } //inline constructor
    unsigned short GetId() const { return this->mKeyBlockId; } //+28
    const ElGamal& GetCurrentKey() const { return this->mKeyBlock.front(); } //first key node's payload, +8
    bool Verify(const AuthBase*) const; //2e26e0
    bool IsValidAndVerified(const ElGamal&) const; //2e2740
};
}
#endif
