// ElGamal: forwarders to ee_won (rust/crates/ee_won/src/elgamal.rs): key loading, block encryption and decryption,
// MD5 signatures. The big integers (BigInteger*.cpp), the random generator (Random.cpp) and the key encoding
// (IntegerExtractor.cpp, ElGamalKey.cpp) are ee_won's as well.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
// The original's default constructor is at 0x007d7690
// Creates a key holder with no key (its random generator seeded from the system)
ElGamal::ElGamal() : mKey(ee_won_elgamal_new()) {}
// The original's key constructor is at 0x007d7740
// Creates a key holder and loads a private or a public key
ElGamal::ElGamal(const void *theKey, int theKeyLen, bool isPrivate) : mKey(ee_won_elgamal_new())
{
    if(isPrivate)
        this->SetPrivateKey(theKey,theKeyLen);
    else
        this->SetPublicKey(theKey,theKeyLen);
}
// Copies the key and its generator state
ElGamal::ElGamal(const ElGamal& other) : mKey(ee_won_elgamal_clone(other.mKey)) {}
ElGamal& ElGamal::operator=(const ElGamal& other)
{
    if(this!=&other) {
        EeWonElGamal* copy=ee_won_elgamal_clone(other.mKey);
        ee_won_elgamal_free(this->mKey);
        this->mKey=copy;
    }
    return *this;
}
ElGamal::~ElGamal() { ee_won_elgamal_free(this->mKey); }
void ElGamal::Adopt(EeWonElGamal* inKey)
{
    ee_won_elgamal_free(this->mKey);
    this->mKey=inKey!=NULL ? inKey : ee_won_elgamal_new();
}
// FUNCTION: WON 0x007d7820
// Marks the key as holding neither a public nor a private key
void ElGamal::Invalidate() { ee_won_elgamal_invalidate(this->mKey); }
// FUNCTION: WON 0x007d7830
// Returns whether a private key is loaded
bool ElGamal::IsPrivate() const { return ee_won_elgamal_is_private(this->mKey); }
// FUNCTION: WON 0x007d7840
// Returns whether a public key is loaded
bool ElGamal::IsPublic() const { return ee_won_elgamal_is_public(this->mKey); }
// FUNCTION: WON 0x007d7850
// Loads a private key (p, q, g, y, x) and its reduction reciprocal; false when the data is malformed
bool ElGamal::SetPrivateKey(const void *theKey, int theKeyLen)
{
    return ee_won_elgamal_set_key(this->mKey,static_cast<const uint8_t*>(theKey),theKeyLen>0 ? theKeyLen : 0,true);
}
// FUNCTION: WON 0x007d79e0
// Loads a public key (p, q, g, y) and its reduction reciprocal; false when the data is malformed
bool ElGamal::SetPublicKey(const void *theKey, int theKeyLen)
{
    return ee_won_elgamal_set_key(this->mKey,static_cast<const uint8_t*>(theKey),theKeyLen>0 ? theKeyLen : 0,false);
}
// One computation into a new byte buffer; NULL when the key cannot do it. Encrypting and signing advance the key's
// random generator (the original's `mutable Random`), so a const key changes there as it did.
ByteBufferPtr ElGamal::Run(EeWonElGamalOp op, const void* data, int length) const
{
    ByteBufferPtr result;
    ee_won_elgamal_op(this->mKey,op,static_cast<const uint8_t*>(data),length>0 ? length : 0,WonByteBufferSink,&result);
    return result;
}
// FUNCTION: WON 0x007d7b50
// Returns the data encrypted in modulus-sized blocks behind a block count; NULL without a public key
ByteBufferPtr ElGamal::Encrypt(const void *thePlainText, int thePlainTextLen) const
{ return this->Run(EeWonElGamalOp::Encrypt,thePlainText,thePlainTextLen); }
// FUNCTION: WON 0x007d7fc0
// Returns the decrypted data of a block envelope; NULL without a private key or on bad input
ByteBufferPtr ElGamal::Decrypt(const void *theCipherText, int theCipherTextLen) const
{ return this->Run(EeWonElGamalOp::Decrypt,theCipherText,theCipherTextLen); }
// No original address identified: WON SDK body
// Returns the ElGamal signature of the message's MD5 digest; NULL without a private key
ByteBufferPtr ElGamal::Sign(const void* theMessage, int theMessageLen) const
{ return this->Run(EeWonElGamalOp::Sign,theMessage,theMessageLen); }
// No original address identified: WON SDK body
ByteBufferPtr ElGamal::GetRawPrivateKey() const { return this->Run(EeWonElGamalOp::RawPrivateKey,NULL,0); }
// No original address identified: WON SDK body
ByteBufferPtr ElGamal::GetRawPublicKey() const { return this->Run(EeWonElGamalOp::RawPublicKey,NULL,0); }
// FUNCTION: WON 0x007d8620
// Checks an ElGamal signature over the message's MD5 digest with the public key
bool ElGamal::Verify(const void *theMessage, int theMessageLen, const void *theSignature, int theSignatureLen) const
{
    return ee_won_elgamal_verify(this->mKey,static_cast<const uint8_t*>(theMessage),theMessageLen>0 ? theMessageLen : 0,
                                 static_cast<const uint8_t*>(theSignature),theSignatureLen>0 ? theSignatureLen : 0);
}
}
