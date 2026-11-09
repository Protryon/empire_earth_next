// ElGamal: the keys, the big-integer arithmetic, the random generator, block encryption and the signatures are
// ee_won's (rust/crates/ee_won/src/elgamal.rs, bigint.rs, random.rs, der.rs); this class owns one key. Copies are
// deep (the key and its generator state), as the original's 0x1a8 bytes of value members were.
#ifndef EE_WONAPI_WONELGAMAL_H
#define EE_WONAPI_WONELGAMAL_H
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI {
class ElGamal {
    EeWonElGamal* mKey;
    ByteBufferPtr Run(EeWonElGamalOp inOp, const void* inData, int inLength) const;
public:
    ElGamal(); //3d7690
    ElGamal(const void*, int, bool); //3d7740
    ElGamal(const ElGamal&);
    ElGamal& operator=(const ElGamal&);
    ~ElGamal();
    void Invalidate(); //3d7820
    bool IsPrivate() const; //3d7830
    bool IsPublic() const; //3d7840
    bool SetPrivateKey(const void*, int); //3d7850
    bool SetPublicKey(const void*, int); //3d79e0
    ByteBufferPtr Encrypt(const void*, int) const; //3d7b50
    ByteBufferPtr Decrypt(const void*, int) const; //3d7fc0
    bool Verify(const void*, int, const void*, int) const; //3d8620
    ByteBufferPtr Sign(const void*,int) const;
    ByteBufferPtr GetRawPrivateKey() const;
    ByteBufferPtr GetRawPublicKey() const;
    // takes ownership of a key ee_won made (an auth block's); NULL leaves no key
    void Adopt(EeWonElGamal* inKey);
};
}
#endif
