// CRC16 (WON SDK WONCommon/CRC.h).
// 2a0c70 writes the ushorts at offsets 0/2/4; Put 2a0c90 reads the ushort at 0.
// The SDK defaults/Get/Reset agree with the product-ID code.
#ifndef EE_WONAPI_WONCRC16_H
#define EE_WONAPI_WONCRC16_H
#include <stdint.h>
#include <stddef.h>
#include "rust/won_net/ffi.h"
namespace WONAPI {
class CRC16 {
public:
    // ee_won's (crc16.rs: the 0x8005 table); header-only since net round 27
    explicit CRC16(unsigned short inInit = 0, unsigned short inXorOut = 0) //2a0c70
        : mInitValue(inInit), mXOROutValue(inXorOut) { this->Reset(); }
    void Put(const void* inData, unsigned int inLength) //2a0c90
    { this->mRegister = ee_won_crc16_put(this->mRegister, static_cast<const uint8_t*>(inData), inLength); }
    unsigned short Get() const { return this->mRegister ^ this->mXOROutValue; }
    void Reset() { this->mRegister = this->mInitValue; }
private:
    unsigned short mRegister; //0
    unsigned short mInitValue; //2
    unsigned short mXOROutValue; //4; size 6
};
}
#endif