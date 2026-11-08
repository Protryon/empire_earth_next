// ElGamal (0x1a8 bytes): storage and the external public-key API.
// 3d7690 constructs Random at 0, then six 0x20-byte BigIntegers; CreateAccount spans 108..2b0 (0x1a8).
// Implicit destruction, from 2cc010/2cc2e0: six BigInteger dtors and the Random COW members.
#ifndef EE_WONAPI_WONELGAMAL_H
#define EE_WONAPI_WONELGAMAL_H
#include "ee/wonapi/WONRandom.h"
#include "ee/wonapi/WONBigInteger.h"
namespace WONAPI {
class ElGamal {
    mutable Random mRandom; //00..d8
    BigInteger p; //d8..f8
    BigInteger q; //f8..118
    BigInteger g; //118..138
    BigInteger y; //138..158
    BigInteger x; //158..178
    BigInteger recip; //178..198
    int nb; //198
    int modulusLen; //19c
    bool mIsPrivate; //1a0
    bool mIsPublic; //1a1; natural1a2..1a8 alignment
    bool EncodeDigest(const RawBuffer&,BigInteger&) const; //3d84a0, external, ret 8
    bool encrypt(const BigInteger&,BigInteger[]) const; //3d8990, external, ret 8
    bool decrypt(const BigInteger&,const BigInteger&,BigInteger&) const; //3d8b70, external, ret 0c
    bool BogusVerify(const BigInteger&,const BigInteger&,const BigInteger&) const; //3d8c50, external, ret 0c
    static BigInteger GetRandBetween(Random&,const BigInteger&,const BigInteger&);
    bool BogusSign(const BigInteger&,BigInteger[]) const;
public:
    ElGamal(); //3d7690
    ElGamal(const void*, int, bool); //3d7740
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
    // Implicit destructor: no vtable, members above
};
}
#endif
