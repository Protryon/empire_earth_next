// AuthBase: owned raw certificate bytes, unpack dispatch and expiry checks
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
{
    this->mIsValid = false;
    if (data != NULL && length != 0)
        this->mRawBuf = new ByteBuffer(data, length, false);
}
// FUNCTION: WON 0x006dcdd0
// Destroys the auth block (members only)
AuthBase::~AuthBase() {}
// FUNCTION: WON 0x006dce40
// Unpacks the raw bytes through UnpackHook and records whether a signature follows the data; returns validity
bool AuthBase::Unpack()
{
    bool valid = false;
    if (this->mRawBuf.get() != NULL) {
        try {
            ReadBuffer input(this->mRawBuf->data(), this->mRawBuf->length());
            valid = this->UnpackHook(input);
            if (valid) {
                this->mDataLen = static_cast<unsigned short>(input.mPos);
                if (static_cast<unsigned long>(input.mLength) <= this->mDataLen)
                    valid = false;
            }
        }
        catch (ReadBufferException&) {}
    }
    this->mIsValid = valid;
    return this->mIsValid;
}
// FUNCTION: WON 0x006dcf40
// Returns true when the block is invalid or expires within the given number of seconds
bool AuthBase::IsExpired(time_t extra) const
{
    if (!this->mIsValid)
        return true;
    return this->mExpireTime <= time(NULL) + extra;
}
// FUNCTION: WON 0x006dcf80
// Reads the family, issue time and expiry time from the buffer
bool AuthBase::UnpackHook(ReadBuffer& input)
{
    this->mFamily = input.ReadShort();
    this->mIssueTime = input.ReadLong();
    this->mExpireTime = input.ReadLong();
    return true;
}
// FUNCTION: WON 0x006dced0
// Verifies the block's signature over its data with the given ElGamal key
bool AuthBase::Verify(const ElGamal& key) const
{
    return key.Verify(this->GetData(), this->GetDataLen(), this->GetSig(), this->GetSigLen());
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dcdb0
// (protected: virtual void * __thiscall WONAPI::AuthBase::`scalar deleting destructor'(unsigned int)    [??_GAuthBase@WONAPI@@MAEPAXI@Z])
