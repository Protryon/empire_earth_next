// AuthBase (0x1c bytes), ctor 2dcce0
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
    bool mIsValid; //18; size 1c
protected:
    virtual ~AuthBase(); //2dcdd0
    virtual bool UnpackHook(ReadBuffer&); //2dcf80
    bool Unpack(); //2dce40
public:
    AuthBase(const void* = NULL, unsigned long = 0); //2dcce0
    ByteBufferPtr GetRawBuf() const { return this->mRawBuf; } //returns an owning pointer; 2e8060/51 bytes, sret, ret 4
    const void* GetRaw() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; } //SDK borrowed raw data at +08; Start 2e6730
    unsigned long GetRawLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length() : 0; } //SDK raw +08 length; Start 2e6730
    bool IsValid() const { return this->mIsValid; } //+18 (inline)
    const void* GetData() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data() : NULL; }
    unsigned long GetDataLen() const { return this->mRawBuf.get()!=NULL ? this->mDataLen : 0; }
    const void* GetSig() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->data()+this->mDataLen : NULL; }
    unsigned long GetSigLen() const { return this->mRawBuf.get()!=NULL ? this->mRawBuf->length()-this->mDataLen : 0; }
    time_t GetIssueTime() const { return this->mIssueTime; } //+10
    time_t GetExpireTime() const { return this->mExpireTime; } //+14 (inline)
    bool IsExpired(time_t = 0) const; //2dcf40
    bool Verify(const ElGamal&) const; //2dced0, external
};
}
#endif
