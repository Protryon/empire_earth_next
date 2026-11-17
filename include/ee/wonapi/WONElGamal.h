// ElGamal: the keys, the big-integer arithmetic, the random generator, block encryption and the signatures are
// ee_won's (rust/crates/ee_won_net/src/elgamal.rs, bigint.rs, random.rs, der.rs); this class owns one key. Copies are
// deep (the key and its generator state), as the original's 0x1a8 bytes of value members were. Header-only since net
// round 28 (ElGamal.cpp's forwarders inline). Encrypting and signing advance the key's random generator (the
// original's `mutable Random`), so a const key changes there as it did.
#ifndef EE_WONAPI_WONELGAMAL_H
#define EE_WONAPI_WONELGAMAL_H
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI {
class ElGamal {
    EeWonElGamal* mKey;
    // one computation into a new byte buffer; NULL when the key cannot do it
    ByteBufferPtr Run(EeWonElGamalOp inOp, const void* inData, int inLength) const
    {
        ByteBufferPtr aResult;
        ee_won_elgamal_op(this->mKey, inOp, static_cast<const uint8_t*>(inData), inLength > 0 ? inLength : 0,
                          WonByteBufferSink, &aResult);
        return aResult;
    }
public:
    ElGamal() : mKey(ee_won_elgamal_new()) {} //3d7690: no key, the generator seeded from the system
    ElGamal(const void* inKey, int inLength, bool inPrivate) : mKey(ee_won_elgamal_new()) //3d7740
    { inPrivate ? this->SetPrivateKey(inKey, inLength) : this->SetPublicKey(inKey, inLength); }
    ElGamal(const ElGamal& inOther) : mKey(ee_won_elgamal_clone(inOther.mKey)) {}
    ElGamal& operator=(const ElGamal& inOther)
    {
        if (this != &inOther) {
            EeWonElGamal* aCopy = ee_won_elgamal_clone(inOther.mKey);
            ee_won_elgamal_free(this->mKey);
            this->mKey = aCopy;
        }
        return *this;
    }
    ~ElGamal() { ee_won_elgamal_free(this->mKey); }
    void Invalidate() { ee_won_elgamal_invalidate(this->mKey); } //3d7820: neither key
    bool IsPrivate() const { return ee_won_elgamal_is_private(this->mKey); } //3d7830
    bool IsPublic() const { return ee_won_elgamal_is_public(this->mKey); } //3d7840
    // a private (p, q, g, y, x) or public (p, q, g, y) key and its reduction reciprocal; false when malformed
    bool SetPrivateKey(const void* inKey, int inLength) { return this->SetKey(inKey, inLength, true); } //3d7850
    bool SetPublicKey(const void* inKey, int inLength) { return this->SetKey(inKey, inLength, false); } //3d79e0
    // modulus-sized blocks behind a block count; NULL without a public key (3d7b50)
    ByteBufferPtr Encrypt(const void* inData, int inLength) const { return this->Run(EeWonElGamalOp::Encrypt, inData, inLength); }
    // a block envelope's data; NULL without a private key or on bad input (3d7fc0)
    ByteBufferPtr Decrypt(const void* inData, int inLength) const { return this->Run(EeWonElGamalOp::Decrypt, inData, inLength); }
    // a signature over the message's MD5 digest checked with the public key (3d8620)
    bool Verify(const void* inMessage, int inLength, const void* inSignature, int inSignatureLength) const
    {
        return ee_won_elgamal_verify(this->mKey, static_cast<const uint8_t*>(inMessage), inLength > 0 ? inLength : 0,
                                     static_cast<const uint8_t*>(inSignature), inSignatureLength > 0 ? inSignatureLength : 0);
    }
    // the signature of the message's MD5 digest; NULL without a private key
    ByteBufferPtr Sign(const void* inData, int inLength) const { return this->Run(EeWonElGamalOp::Sign, inData, inLength); }
    ByteBufferPtr GetRawPrivateKey() const { return this->Run(EeWonElGamalOp::RawPrivateKey, NULL, 0); }
    ByteBufferPtr GetRawPublicKey() const { return this->Run(EeWonElGamalOp::RawPublicKey, NULL, 0); }
    // takes ownership of a key ee_won made (an auth block's); NULL leaves no key
    void Adopt(EeWonElGamal* inKey)
    {
        ee_won_elgamal_free(this->mKey);
        this->mKey = inKey != NULL ? inKey : ee_won_elgamal_new();
    }
    const EeWonElGamal* Raw() const { return this->mKey; } // the Rust key (copied by ee_won_peer_new)
private:
    bool SetKey(const void* inKey, int inLength, bool inPrivate)
    { return ee_won_elgamal_set_key(this->mKey, static_cast<const uint8_t*>(inKey), inLength > 0 ? inLength : 0, inPrivate); }
};
}
#endif
