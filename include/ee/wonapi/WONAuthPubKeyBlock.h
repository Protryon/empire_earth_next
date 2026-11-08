// AuthPubKeyBlock (0x2c bytes)
#ifndef EE_WONAPI_WONAUTHPUBKEYBLOCK_H
#define EE_WONAPI_WONAUTHPUBKEYBLOCK_H
#include <list>
#include "ee/wonapi/WONAuthBase.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
class AuthPubKeyBlock : public AuthBase {
    typedef std::list<ElGamal> KeyBlock;
    KeyBlock mKeyBlock; //1c allocator /20header /24count
    unsigned short mKeyBlockId; //28, natural extent2c
protected:
    virtual ~AuthPubKeyBlock(); //2e2350, explicit destructor
    virtual bool UnpackHook(ReadBuffer&); //2e2400 discoverygap,732code+4pad
public:
    AuthPubKeyBlock(const void* data = NULL,unsigned long len = 0)
        :AuthBase(data,len) { this->Unpack(); } //inline constructor; 2e4b6a/2e5a31 build the key list
    unsigned short GetId() const { return this->mKeyBlockId; } //+28
    const ElGamal& GetCurrentKey() const { return this->mKeyBlock.front(); } //first key node's payload, +8
    bool Verify(const AuthBase*) const; //2e26e0
    bool IsValidAndVerified(const ElGamal&) const; //2e2740
};
}
#endif
