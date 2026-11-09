// AuthBase (the original's 0x1c bytes, ctor 2dcce0): a signed auth block. The unpacking and the checks are ee_won's
// (rust/crates/ee_won/src/auth.rs); this class keeps the raw bytes, the header fields its getters return and the
// unpacked block.
#ifndef EE_WONAPI_WONAUTHBASE_H
#define EE_WONAPI_WONAUTHBASE_H
#include <time.h>
#include "ee/wonapi/WONBuffer.h"
namespace WONAPI {
class ElGamal;
class AuthBase : public RefCount {
    ByteBufferPtr mRawBuf; //08
    unsigned short mDataLen; //0c
    unsigned short mFamily; //0e
    time_t mIssueTime; //10
    time_t mExpireTime; //14
    bool mIsValid; //18
    EeWonAuth* mParsed; // the unpacked block (NULL before Unpack)
    AuthBase(const AuthBase&); // not copied: the unpacked block has one owner
    AuthBase& operator=(const AuthBase&);
protected:
    virtual ~AuthBase(); //2dcdd0
    virtual uint32_t UnpackKind() const { return EE_WON_AUTH_BASE; } // which block the bytes hold (EE_WON_AUTH_*)
    virtual void UnpackHook() {} // copies what a subclass keeps from the unpacked block
    bool Unpack(); //2dce40
    const EeWonAuthHeader& GetHeader() const { return this->mHeader; }
    EeWonAuthHeader mHeader;
public:
    AuthBase(const void* = NULL, unsigned long = 0); //2dcce0
    ByteBufferPtr GetRawBuf() const { return this->mRawBuf; }
    const void* GetRaw() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; }
    unsigned long GetRawLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length() : 0; }
    bool IsValid() const { return this->mIsValid; } //+18 (inline)
    const void* GetData() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; }
    unsigned long GetDataLen() const { return this->mRawBuf.get()!=NULL ? this->mDataLen : 0; }
    const void* GetSig() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data()+this->mDataLen : NULL; }
    unsigned long GetSigLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length()-this->mDataLen : 0; }
    time_t GetIssueTime() const { return this->mIssueTime; } //+10
    time_t GetExpireTime() const { return this->mExpireTime; } //+14 (inline)
    bool IsExpired(time_t = 0) const; //2dcf40
    bool Verify(const ElGamal&) const; //2dced0, external
    const EeWonAuth* GetParsed() const { return this->mParsed; } // the unpacked block (NULL before Unpack)
};
}
#endif
