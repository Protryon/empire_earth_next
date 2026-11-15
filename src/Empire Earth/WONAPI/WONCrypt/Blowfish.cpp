// Blowfish: forwarders to ee_won (rust/crates/ee_won/src/blowfish.rs): the key schedule, the block cipher and the WON
// length-prefixed CBC framing
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI {
// FUNCTION: WON 0x007d5e20
// Creates a cipher with no key
Blowfish::Blowfish()
{
    ee_won_blowfish_init(&this->mState);
}
// No original address identified: WON SDK key constructor, needed by the RegAuth static key
Blowfish::Blowfish(const void *theKey, int theKeyLen)
{
    ee_won_blowfish_init(&this->mState);
    this->SetKey(theKey,theKeyLen);
}
// FUNCTION: WON 0x007d5e90
// Runs the key schedule for a key of valid length and keeps the raw key; false when the length is invalid
bool Blowfish::SetKey(const void *theKey, int theKeyLen)
{
    return ee_won_blowfish_set_key(&this->mState,static_cast<const uint8_t*>(theKey),theKeyLen>0 ? theKeyLen : 0);
}
// FUNCTION: WON 0x007d6dd0
// Returns the data CBC-encrypted with a 4-byte length prefix and zero padding; NULL without a key
ByteBufferPtr Blowfish::Encrypt(const void *inOrig, int inLen) const
{
    ByteBufferPtr result;
    ee_won_blowfish_encrypt(&this->mState,static_cast<const uint8_t*>(inOrig),inLen>0 ? inLen : 0,
                            WonByteBufferSink,&result);
    return result;
}
// FUNCTION: WON 0x007d7150
// Returns the CBC-decrypted data with the length prefix checked and removed; NULL on bad input
ByteBufferPtr Blowfish::Decrypt(const void *inOrig, int inLen) const
{
    ByteBufferPtr result;
    ee_won_blowfish_decrypt(&this->mState,static_cast<const uint8_t*>(inOrig),inLen>0 ? inLen : 0,
                            WonByteBufferSink,&result);
    return result;
}
// FUNCTION: WON 0x007d74f0
// Sets a new random key of the given length; false when the length is invalid
bool Blowfish::Create(int length)
{
    return ee_won_blowfish_create(&this->mState,length);
}
}
