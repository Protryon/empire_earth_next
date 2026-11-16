// MD5Digest (WONCrypt's MD5, used by the auth hashes): plain MD5, ee_won's (rust/crates/ee_won/src/md5.rs). A
// finished digest resets the state. Header-only since net round 28 (MD5Digest.cpp's forwarders inline; its C++
// rounds had shifted by negative counts, PORT_LEARNINGS "R6: WON's leaves in Rust").
#ifndef __WON_MINICRYPT_MD5DIGEST_H__
#define __WON_MINICRYPT_MD5DIGEST_H__
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI
{
class MD5Digest
{
    EeWonMd5 mState;
public:
    MD5Digest() { ee_won_md5_reset(&this->mState); }
    void reset() { ee_won_md5_reset(&this->mState); }
    void update(const void* theData, unsigned long theLen)
    { ee_won_md5_update(&this->mState, static_cast<const uint8_t*>(theData), theLen); }
    void update(const RawBuffer& input) { this->update(input.data(), static_cast<unsigned long>(input.length())); }
    // the 16-byte digest of the data so far; the state reset
    RawBuffer digest()
    {
        unsigned char anOut[16];
        ee_won_md5_finish(&this->mState, anOut);
        return RawBuffer(anOut, 16);
    }
    RawBuffer digest(const RawBuffer& in) { this->update(in); return this->digest(); }
};
}; // namespace WONAPI
#endif
