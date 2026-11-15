// MD5 message digest (WON SDK): forwarders to ee_won (rust/crates/ee_won/src/md5.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONMD5.h"
using namespace WONAPI;
// 0x6a26b0 (emitted from the list below): creates a digest in its initial state
MD5::MD5()
{
	ee_won_md5_reset(&this->mState);
}
// Resets the digest to the MD5 initial state with no bytes counted
void MD5::Reset()
{
	ee_won_md5_reset(&this->mState);
}
// Adds the given bytes to the digest
void MD5::Update(const void *theBuf, unsigned long len)
{
	ee_won_md5_update(&this->mState,static_cast<const uint8_t*>(theBuf),len);
}
// Pads the data, writes the 16-byte digest and resets
void MD5::Digest(unsigned char digest[16])
{
	ee_won_md5_finish(&this->mState,digest);
}
