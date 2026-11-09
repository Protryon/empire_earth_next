// AuthBase: owned raw certificate bytes; the unpacking and the checks are ee_won's (rust/crates/ee_won/src/auth.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAuthBase.h"
#include "ee/wonapi/WONElGamal.h"
namespace WONAPI {
// FUNCTION: WON 0x006dcce0
// Creates an unvalidated auth block, copying the raw bytes when any are given
AuthBase::AuthBase(const void* data, unsigned long length)
    : mDataLen(0), mFamily(0), mIssueTime(0), mExpireTime(0), mIsValid(false), mParsed(NULL), mHeader()
{
    if (data != NULL && length != 0)
        this->mRawBuf = new ByteBuffer(data, length, false);
}
// FUNCTION: WON 0x006dcdd0
// Destroys the auth block and its unpacked form
AuthBase::~AuthBase() { ee_won_auth_free(this->mParsed); }
// FUNCTION: WON 0x006dce40
// Unpacks the raw bytes as this class's kind of block (valid when its fields parse and a signature follows), then
// lets the subclass copy its fields; returns validity
bool AuthBase::Unpack()
{
    ee_won_auth_free(this->mParsed);
    this->mParsed = NULL;
    this->mHeader = EeWonAuthHeader();
    if (this->mRawBuf.get() != NULL) {
        this->mParsed = ee_won_auth_unpack(this->UnpackKind(), reinterpret_cast<const uint8_t*>(this->mRawBuf->data()),
                                           this->mRawBuf->length());
        ee_won_auth_header(this->mParsed, &this->mHeader);
    }
    this->mFamily = this->mHeader.family;
    this->mIssueTime = this->mHeader.issue_time;
    this->mExpireTime = this->mHeader.expire_time;
    this->mDataLen = this->mHeader.data_len;
    this->mIsValid = this->mHeader.valid;
    if (this->mParsed != NULL)
        this->UnpackHook();
    return this->mIsValid;
}
// FUNCTION: WON 0x006dcf40
// Returns true when the block is invalid or expires within the given number of seconds
bool AuthBase::IsExpired(time_t extra) const
{
    if (!this->mIsValid || this->mParsed == NULL)
        return true;
    return ee_won_auth_is_expired(this->mParsed, static_cast<int64_t>(time(NULL)), static_cast<int64_t>(extra));
}
// FUNCTION: WON 0x006dced0
// Verifies the block's signature over its data with the given ElGamal key
bool AuthBase::Verify(const ElGamal& key) const
{
    return key.Verify(this->GetData(), this->GetDataLen(), this->GetSig(), this->GetSigLen());
}
}
