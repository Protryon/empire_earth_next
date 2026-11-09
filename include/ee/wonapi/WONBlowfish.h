// Blowfish cipher: the key schedule, the cipher and WON's framing are ee_won's (rust/crates/ee_won/src/blowfish.rs);
// this class holds the keyed state by value (copied with the class, as the original's 0x10b4 bytes were).
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
    Blowfish(); //3d5e20
    Blowfish(const void* theKey, int theKeyLen); // explicit key ctor; no original address identified
    bool Create(int); //3d74f0, Random-filled key5..56
    const void* GetKey() const { return ee_won_blowfish_key(&this->mState); }
    int GetKeyLen() const { return ee_won_blowfish_key_length(&this->mState); }
    bool SetKey(const void* inKey, int inKeyLength); //3d5e90;5..56
    ConstSmartPtr<ByteBuffer> Encrypt(const void* inData, int inLength) const; //3d6dd0
    ConstSmartPtr<ByteBuffer> Decrypt(const void* inData, int inLength) const; //3d7150
private:
    EeWonBlowfish mState;
};
}
#endif
