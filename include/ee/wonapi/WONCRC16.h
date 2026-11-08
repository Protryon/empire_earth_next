// CRC16 (WON SDK WONCommon/CRC.h).
// 2a0c70 writes the ushorts at offsets 0/2/4; Put 2a0c90 reads the ushort at 0.
// The SDK defaults/Get/Reset agree with the product-ID code.
#ifndef EE_WONAPI_WONCRC16_H
#define EE_WONAPI_WONCRC16_H
namespace WONAPI {
class CRC16 {
public:
    explicit CRC16(unsigned short inInit = 0, unsigned short inXorOut = 0); //2a0c70
    void Put(const void* inData, unsigned int inLength); //2a0c90
    unsigned short Get() const { return this->mRegister ^ this->mXOROutValue; }
    void Reset() { this->mRegister = this->mInitValue; }
private:
    unsigned short mRegister; //0
    unsigned short mInitValue; //2
    unsigned short mXOROutValue; //4; size 6
};
}
#endif