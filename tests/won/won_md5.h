#ifndef EE_WON_COMMON_MD5_H
#define EE_WON_COMMON_MD5_H
#include <stdint.h>
#include <stddef.h>
#include "rust/ee_won.h"
namespace WONAPI {
// WONCommon MD5 (the original's 0x58 bytes): plain MD5, the state and rounds ee_won's (rust/crates/ee_won/src/md5.rs).
// Digest resets the state.
class MD5 {
    EeWonMd5 mState;
public:
    // ee_won's (md5.rs); header-only since net round 27
    MD5() { ee_won_md5_reset(&this->mState); } //2a26b0
    void Reset() { ee_won_md5_reset(&this->mState); } //2a26c0
    void Update(const void* inData, unsigned long inLength) { ee_won_md5_update(&this->mState, static_cast<const uint8_t*>(inData), inLength); } //2a26f0
    void Digest(unsigned char outDigest[16]) { ee_won_md5_finish(&this->mState, outDigest); } //2a27d0: padded, then reset
};
}
#endif
