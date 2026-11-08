// Blowfish cipher storage
// Target constructor3d5e20, SetKey3d5e90, P/S geometry and byte-COW helpers independently read.

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
    Blowfish(); //3d5e20; only RawBuffers default initialization +validfalse, arrays intentionally uninitialized
    Blowfish(const void* theKey, int theKeyLen); // explicit key ctor; no original address identified
    bool Create(int); //3d74f0, Random-filled key5..56
    const void* GetKey() const { return this->mRawKey.data(); }
    int GetKeyLen() const { return this->mRawKey.length(); }
    bool SetKey(const void* inKey, int inKeyLength); //3d5e90;5..56
    ConstSmartPtr<ByteBuffer> Encrypt(const void* inData, int inLength) const; //3d6dd0
    ConstSmartPtr<ByteBuffer> Decrypt(const void* inData, int inLength) const; //3d7150
    // Destructor implicit; the two RawBuffer members own their COW allocations
private:
    static const int Pinit[18];
    static const int Sinit[1024];
    void encryptTwoIntegers(int[], int, int[], int) const;
    void encryptBlock(const unsigned char*, unsigned char*) const;
    void decryptBlock(const unsigned char*, unsigned char*) const;
    void initXORMask() const;
    int S0[256]; //0000
    int S1[256]; //0400
    int S2[256]; //0800
    int S3[256]; //0c00
    int P[18]; //1000
    mutable RawBuffer XOR_MASK; //1048
    RawBuffer mRawKey; //1058
    int P0, P1, P2, P3, P4, P5, P6, P7, P8; //1068..1088
    int P9, P10, P11, P12, P13, P14, P15, P16, P17; //108c..10ac
    bool mIsValid; //10b0, 3 bytes padding; size 10b4
};
}
#endif
