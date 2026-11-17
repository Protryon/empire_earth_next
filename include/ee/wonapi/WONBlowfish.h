// Blowfish cipher: the key schedule, the cipher and WON's framing are ee_won's (rust/crates/ee_won_net/src/blowfish.rs);
// this class holds the keyed state by value (copied with the class, as the original's 0x10b4 bytes were). Header-only
// since net round 28 (Blowfish.cpp's forwarders inline).
#ifndef EE_WONAPI_WONBLOWFISH_H
#define EE_WONAPI_WONBLOWFISH_H
#include <string>
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
class ByteBuffer;
typedef std::basic_string<unsigned char> RawBuffer;
class Blowfish {
public:
    enum { minKeyLen = 5, blockLength = 8, maxKeyLen = 56 };
    Blowfish() { ee_won_blowfish_init(&this->mState); } //3d5e20
    Blowfish(const void* theKey, int theKeyLen) { ee_won_blowfish_init(&this->mState); this->SetKey(theKey, theKeyLen); }
    // a new random key of the length (5..56; false otherwise) (3d74f0)
    bool Create(int length) { return ee_won_blowfish_create(&this->mState, length); }
    const void* GetKey() const { return ee_won_blowfish_key(&this->mState); }
    int GetKeyLen() const { return ee_won_blowfish_key_length(&this->mState); }
    // the key schedule for a key of valid length (5..56), the raw key kept; false otherwise (3d5e90)
    bool SetKey(const void* inKey, int inKeyLength)
    { return ee_won_blowfish_set_key(&this->mState, static_cast<const uint8_t*>(inKey), inKeyLength > 0 ? inKeyLength : 0); }
    // CBC with a 4-byte length prefix and zero padding; NULL without a key (3d6dd0)
    ConstSmartPtr<ByteBuffer> Encrypt(const void* inData, int inLength) const
    { return this->Run(&ee_won_blowfish_encrypt, inData, inLength); }
    // the length prefix checked and removed; NULL on bad input (3d7150)
    ConstSmartPtr<ByteBuffer> Decrypt(const void* inData, int inLength) const
    { return this->Run(&ee_won_blowfish_decrypt, inData, inLength); }
private:
    template <class F> ByteBufferPtr Run(F inStep, const void* inData, int inLength) const
    {
        ByteBufferPtr aResult;
        inStep(&this->mState, static_cast<const uint8_t*>(inData), inLength > 0 ? inLength : 0, WonByteBufferSink, &aResult);
        return aResult;
    }
    EeWonBlowfish mState;
};
}
#endif
