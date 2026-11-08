// Random: MD5-based random byte generator seeded from disk space, tick count and system time
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRandom.h"
namespace WONAPI {
long Random::usageCount = 0;
// The original leaves randomBytesUsed and counter uninitialized
// FUNCTION: WON 0x007da510
// Creates a generator seeded from the system
Random::Random()
{
    this->setSeed(Random::getSeed(20));
}
// FUNCTION: WON 0x007da5e0
// Mixes a seed into the generator state with MD5
void Random::setSeed(const RawBuffer& seed)
{
    if (this->state.length() != 0)
        this->digest.update(this->state);
    this->state = this->digest.digest(seed);
}
// FUNCTION: WON 0x007da770
// Fills the buffer with random bytes, hashing the state and a counter for each 16-byte block
void Random::nextBytes(RawBuffer& bytes)
{
    int requested = bytes.length();
    int received = 0;
    while (received < requested) {
        if (this->randomBytes.length() == 0 || this->randomBytesUsed == this->randomBytes.length()) {
            this->digest.update(this->state);
            this->randomBytes = this->digest.digest(this->longToByteArray(this->counter++));
            this->randomBytesUsed = 0;
        }
        bytes[received++] = this->randomBytes[this->randomBytesUsed++];
    }
}
// FUNCTION: WON 0x007da950
// Returns 8 seed bytes from disk space, tick count, system time and a use counter (size ignored)
RawBuffer Random::getSeed(int)
{
    RawBuffer bytes(8, static_cast<unsigned char>(0));
    long usage = ++Random::usageCount;
    unsigned long sectorsPerCluster, bytesPerSector, freeClusters, usedClusters;
    __int64 seed = 0;
    GetDiskFreeSpaceA(NULL, &sectorsPerCluster, &bytesPerSector, &freeClusters, &usedClusters);
    usedClusters -= freeClusters;
    seed = (usedClusters < freeClusters ? freeClusters : usedClusters) * sectorsPerCluster * bytesPerSector;
    unsigned long tick = GetTickCount();
    *reinterpret_cast<unsigned short*>(&seed) = reinterpret_cast<unsigned short*>(&tick)[1];
    ULARGE_INTEGER systemTime;
    GetSystemTimeAsFileTime(reinterpret_cast<FILETIME*>(&systemTime));
    seed ^= systemTime.QuadPart;
    seed += usage;
    return bytes.assign(reinterpret_cast<const unsigned char*>(&seed), 8);
}
// FUNCTION: WON 0x007dab20
// Returns a 64-bit value as 8 little-endian bytes
RawBuffer Random::longToByteArray(__int64 value)
{
    RawBuffer bytes(8, static_cast<unsigned char>(0));
    for (int i=0; i<8; ++i) {
        bytes[i] = static_cast<unsigned char>(value);
        value >>= 8;
    }
    return bytes;
}
}

namespace WONAPI {

// No original address identified: WON SDK overload
Random::Random(const RawBuffer& seed) 
{
	this->setSeed(seed);
}

// No original address identified: WON SDK overload
void Random::setSeed(__int64 seed) 
{
	if (seed != 0)
		this->setSeed(this->longToByteArray(seed));
}

// No original address identified: WON SDK overload
void Random::nextBytes(void* data, int theLen)
{
	char *bytes = static_cast<char*>(data);

	int numRequested = theLen;
	int numGot = 0;

	while (numGot < numRequested) 
	{
		// If no more random bytes, make some more
		if (this->randomBytes.length() == 0 || this->randomBytesUsed == this->randomBytes.length()) 
		{
			this->digest.update(this->state);
			this->randomBytes = this->digest.digest(this->longToByteArray(this->counter++));
			this->randomBytesUsed = 0;
		}

		bytes[numGot++] = this->randomBytes[this->randomBytesUsed++];
	}
	
}

// No original address identified: WON SDK overload
unsigned int Random::next(int numBits) 
{
	int numBytes = (numBits+7)/8;
	RawBuffer b(numBytes,(unsigned char)0);
	
	unsigned int next = 0;

	this->nextBytes(b);
	for (int i=0; i<numBytes; i++)
		next = (next << 8) + (b[i] & 0xFF);

		return next >> (numBytes*8 - numBits);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dade0
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
