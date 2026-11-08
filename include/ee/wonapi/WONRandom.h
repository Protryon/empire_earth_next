// Random (0xd8 bytes); the constructor does not initialize counter/used
#ifndef EE_WONAPI_WONRANDOM_H
#define EE_WONAPI_WONRANDOM_H
#include "ee/wonapi/WONMD5Digest.h"
namespace WONAPI {
class Random {
    RawBuffer state; //00
    MD5Digest digest; //10..b8
    RawBuffer randomBytes; //b8..c8
    int randomBytesUsed; //c8; naturalcc..d0 alignment
    __int64 counter; //d0..d8
    static long usageCount; //566bec
public:
    Random(); //3da510
    Random(const RawBuffer&);
    void setSeed(__int64);
    void nextBytes(void*, int);
    unsigned int next(int);
    void setSeed(const RawBuffer&); //3da5e0
    void nextBytes(RawBuffer&); //3da770
    static RawBuffer getSeed(int); //3da950, ignores the requested length, 8 bytes
    RawBuffer longToByteArray(__int64); //3dab20
};
}
#endif
