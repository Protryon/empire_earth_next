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
    MD5(); //2a26b0
    void Reset(); //2a26c0
    void Update(const void*,unsigned long); //2a26f0
    void Digest(unsigned char[16]); //2a27d0
};
}
#endif
