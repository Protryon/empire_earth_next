// MD5Digest (WONCrypt's MD5, used by the auth hashes): plain MD5, ee_won's (rust/crates/ee_won/src/md5.rs). A
// finished digest resets the state.
#ifndef __WON_MINICRYPT_MD5DIGEST_H__
#define __WON_MINICRYPT_MD5DIGEST_H__
#include "ee/wonapi/WONBlowfish.h"
namespace WONAPI
{
class MD5Digest
{
    EeWonMd5 mState;
public:
    MD5Digest();
    void reset();
    void update(const void *theData, unsigned long theLen);
    void update(const RawBuffer& input);
    RawBuffer digest();
    RawBuffer digest(const RawBuffer& in);
};
}; // namespace WONAPI
#endif
