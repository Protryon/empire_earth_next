// IntegerExtractor (0x18 bytes): RawBuffer at 0, offset at 10, valid flag at 14; no vtable
#ifndef EE_WONAPI_WONINTEGEREXTRACTOR_H
#define EE_WONAPI_WONINTEGEREXTRACTOR_H
#include "ee/wonapi/WONBigInteger.h"
namespace WONAPI {
class IntegerExtractor {
    RawBuffer input; //00..10 owned byte string
    int offset; //10
    bool mIsValid; //14, natural padding15..18
    bool LengthDecode(); //3d90a0
    unsigned char GetByte(); //3d8eb0
    unsigned char PeekByte(); //3d8fb0
public:
    enum { INTEGER=0x02,BIT_STRING=0x03,SEQUENCE=0x10,CONSTRUCTED=0x20 };
    IntegerExtractor(const void*,unsigned long,bool isBufferedTransform=true); //3d8e10
    bool Decode(int,BigInteger&); //3d9100
    bool Extract(BigInteger&); //3d92a0
    // Implicit destructor releases the RawBuffer; no vtable or explicit destructor
};
// WON SDK class without a vtable; its only member is the RawBuffer out (size 10)
class IntegerInserter {
    RawBuffer out;
public:
    enum { INTEGER=0x02,BIT_STRING=0x03,SEQUENCE=0x10,CONSTRUCTED=0x20 };
    IntegerInserter();
    void AddLength(RawBuffer&,int);
    void Insert(const BigInteger&);
    ByteBufferPtr Get();
};
}
#endif
