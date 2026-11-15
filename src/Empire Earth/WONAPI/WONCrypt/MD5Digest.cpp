// MD5Digest: MD5 message digest (WONCrypt version): forwarders to ee_won (rust/crates/ee_won/src/md5.rs). The C++
// rounds shifted by a negative count (`(unsigned)a >> -s`): undefined behaviour, and the clang build returned MD5's
// initial state for every input (PORT_LEARNINGS "R6: WON's leaves in Rust").
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONMD5Digest.h"
namespace WONAPI {
// FUNCTION: WON 0x007d9350
// Creates a digest in its initial state
MD5Digest::MD5Digest()
{
	ee_won_md5_reset(&this->mState);
}
// FUNCTION: WON 0x007d93a0
// Resets the digest
void MD5Digest::reset()
{
	ee_won_md5_reset(&this->mState);
}
// FUNCTION: WON 0x007d93d0
// Finishes the hash over the data so far, returns the 16-byte digest and resets
RawBuffer MD5Digest::digest()
{
	unsigned char out[16];
	ee_won_md5_finish(&this->mState,out);
	return RawBuffer(out,16);
}
// FUNCTION: WON 0x007d94e0
// Adds the data, then finishes and returns the 16-byte digest
RawBuffer MD5Digest::digest(const RawBuffer& in)
{
	this->update(in);
	return this->digest();
}
// FUNCTION: WON 0x007d9810
// Adds a buffer to the hash
void MD5Digest::update(const RawBuffer& input)
{
	this->update(input.data(),static_cast<unsigned long>(input.length()));
}
// FUNCTION: WON 0x007d9830
// Adds bytes to the hash
void MD5Digest::update(const void *theData, unsigned long theLen)
{
	ee_won_md5_update(&this->mState,static_cast<const uint8_t*>(theData),theLen);
}
}
