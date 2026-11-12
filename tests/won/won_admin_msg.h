// WONInternal's admin ops' messages through ee_won's admin module (rust/crates/ee_won/src/admin.rs), as the C++ op
// classes used them before the ops became Rust's (adminop.rs): each op's request format and reply records are a
// table in Rust; these hand the members over in the table's order and build the records from the values read (the
// original's statements: MasterKeyOp.cpp, BannedKeyOp.cpp, KeySubscribeOp.cpp, GetAccountFromCDKeyOp.cpp, the
// version and patch ops). Templates over stand-ins: won_admin_test checks them against the pre-port statements,
// won_adminop_test runs the ops' statements over them against the Rust ops. WONStatus comes from WONRouting.h,
// included first.
#pragma once
#include <string.h>
#include <list>
#include <string>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

namespace WONAPI {

// the members of a request, in its table's order
class AdminValues {
public:
    AdminValues& Num(long long inValue)
    {
        EeWonAdminValue v = {static_cast<int64_t>(inValue), NULL, 0};
        this->mValues.push_back(v);
        return *this;
    }
    AdminValues& Str(const std::string& inText)
    {
        EeWonAdminValue v = {0, reinterpret_cast<const uint8_t*>(inText.data()), inText.size()};
        this->mValues.push_back(v);
        return *this;
    }
    AdminValues& WStr(const std::wstring& inText)
    {
        this->mKeep.push_back(std::string());
        WideAppendUtf16LE(this->mKeep.back(), inText);
        return this->Str(this->mKeep.back());
    }
    AdminValues& Raw(const void* inData, size_t inLength)
    {
        EeWonAdminValue v = {0, static_cast<const uint8_t*>(inData), inLength};
        this->mValues.push_back(v);
        return *this;
    }
    AdminValues& Keys(const std::list<std::string>& inKeys)
    {
        this->Num(static_cast<long long>(inKeys.size()));
        for (std::list<std::string>::const_iterator i = inKeys.begin(); i != inKeys.end(); ++i) this->Str(*i);
        return *this;
    }
    // the request data (with what follows appended by the caller when it has a record to add)
    void AppendTo(int inOp, WriteBuffer& ioBuffer) const
    {
        ee_won_admin_request(inOp, this->mValues.empty() ? NULL : &this->mValues[0], this->mValues.size(),
                             AdminAppendSink, &ioBuffer);
    }
    ByteBufferPtr Build(int inOp) const
    {
        WriteBuffer w(0, true);
        this->AppendTo(inOp, w);
        return w.ToByteBuffer(true);
    }

private:
    static void AdminAppendSink(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<WriteBuffer*>(inContext)->AppendBytes(inBytes, static_cast<unsigned long>(inLength));
    }
    std::vector<EeWonAdminValue> mValues;
    std::list<std::string> mKeep;
};

// a reply's values: whole records' values in order, then the end (or the cut)
class AdminReply {
public:
    AdminReply(int inOp, const ByteBuffer* inData) : mBase(inData->data())
    {
        ee_won_admin_reply(inOp, reinterpret_cast<const uint8_t*>(inData->data()), inData->length(), Sink, &this->mFields);
    }
    size_t Values() const { return this->mFields.size() - 1; }
    long Num(size_t i) const { return this->mFields[i].num; }
    std::string Str(size_t i) const
    {
        const EeWonAdminField& f = this->mFields[i];
        return f.whole ? std::string(this->mBase + f.at, f.len) : std::string();
    }
    std::wstring WStr(size_t i) const
    {
        std::wstring s;
        const EeWonAdminField& f = this->mFields[i];
        if (f.whole) WideAssignUtf16LE(s, this->mBase + f.at, f.len);
        return s;
    }
    const char* At(size_t i) const { return this->mBase + this->mFields[i].at; }
    unsigned long Len(size_t i) const { return this->mFields[i].len; }
    bool Cut() const { return this->mFields.back().tag == EE_WON_ADMIN_CUT; }
    unsigned long End() const { return this->mFields.back().at; }

private:
    static void Sink(void* inContext, const EeWonAdminField* inFields, size_t inCount)
    {
        static_cast<std::vector<EeWonAdminField>*>(inContext)->assign(inFields, inFields + inCount);
    }
    const char* mBase;
    std::vector<EeWonAdminField> mFields;
};

// ---- the master keys

template <class Op> ByteBufferPtr AdminInsertMasterKeyRequest(Op& o)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Num(o.mSequenceNumber).Num(o.mIsBeta)
        .Num(o.mActivateTime).Num(o.mDeactivateTime).Num(o.mKeyLength).Raw(o.mKeyP, o.mKeyLength).Num(o.mCDKeyFlags)
        .Build(EE_WON_ADMIN_INSERT_MASTER_KEY);
}
template <class Op> ByteBufferPtr AdminUpdateMasterKeyRequest(Op& o)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Num(o.mSequenceNumber).Num(o.mIsBeta)
        .Num(o.mActivateTime).Num(o.mDeactivateTime).Num(o.mCDKeyFlags).Build(EE_WON_ADMIN_UPDATE_MASTER_KEY);
}
template <class Op> ByteBufferPtr AdminDeleteMasterKeyRequest(Op& o)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Num(o.mSequenceNumber)
        .Build(EE_WON_ADMIN_DELETE_MASTER_KEY);
}
// the community and product only (QueryMasterKeyOp, QueryBannedKeyOp)
template <class Op> ByteBufferPtr AdminQueryRequest(Op& o, int inOp)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Build(inOp);
}
template <class Op> bool AdminQueryMasterKeyReply(Op& o, const ByteBuffer* inData)
{
    AdminReply r(EE_WON_ADMIN_QUERY_MASTER_KEY, inData);
    o.mMasterKeyList.clear();
    for (size_t i = 0; i + 5 <= r.Values(); i += 5) {
        typename Op::MasterKey aKey;
        aKey.mSequenceNumber = static_cast<unsigned short>(r.Num(i));
        aKey.mActivateDate = r.Num(i + 1);
        aKey.mDeactivateDate = r.Num(i + 2);
        aKey.mIsBeta = r.Num(i + 3) != 0;
        aKey.mCDKeyFlags = static_cast<unsigned long>(r.Num(i + 4));
        o.mMasterKeyList.push_back(aKey);
    }
    return !r.Cut();
}

// ---- the key checks and banned keys

template <class Op> ByteBufferPtr AdminAuthCheckKeyRequest(Op& o)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Keys(o.mKeyList).Build(EE_WON_ADMIN_AUTH_CHECK_KEY);
}
template <class Op> ByteBufferPtr AdminInsertBannedKeyRequest(Op& o)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Num(o.mBannedUntil).Keys(o.mKeyList)
        .Build(EE_WON_ADMIN_INSERT_BANNED_KEY);
}
// QueryListBannedKeyOp, RemoveBannedKeyOp
template <class Op> ByteBufferPtr AdminKeyListRequest(Op& o, int inOp)
{
    return AdminValues().WStr(o.mCommunityName).Str(o.mProductName).Keys(o.mKeyList).Build(inOp);
}
// a list of statuses (the key check clears it first)
template <class List> bool AdminStatusReply(int inOp, const ByteBuffer* inData, List& outList, bool inClear)
{
    AdminReply r(inOp, inData);
    if (inClear) outList.clear();
    for (size_t i = 0; i < r.Values(); i++) outList.push_back(static_cast<short>(r.Num(i)));
    return !r.Cut();
}
template <class Op> bool AdminQueryBannedKeyReply(Op& o, const ByteBuffer* inData)
{
    AdminReply r(EE_WON_ADMIN_QUERY_BANNED_KEY, inData);
    for (size_t i = 0; i + 2 <= r.Values(); i += 2) {
        typename Op::BANNED_KEY_PAIR aBannedKey;
        aBannedKey.first = r.Str(i);
        aBannedKey.second = r.Num(i + 1);
        o.mKeyList.push_back(aBannedKey);
    }
    return !r.Cut();
}
template <class Op> bool AdminQueryListBannedKeyReply(Op& o, const ByteBuffer* inData)
{
    AdminReply r(EE_WON_ADMIN_QUERY_LIST_BANNED_KEY, inData);
    for (size_t i = 0; i + 2 <= r.Values(); i += 2) {
        typename Op::BANNED_KEY_PAIR aBannedKey;
        aBannedKey.first = r.Num(i);
        aBannedKey.second = static_cast<short>(r.Num(i + 1));
        o.mKeyDataList.push_back(aBannedKey);
    }
    return !r.Cut();
}

// ---- the key subscriptions (Pack's product, community and keys, then each op's numbers)

template <class Op> AdminValues AdminSubscribeValues(Op& o)
{
    AdminValues v;
    v.Str(o.mProductName).WStr(o.mCommunityName).Keys(o.mKeyList);
    return v;
}
template <class Op> bool AdminQueryKeySubscribeReply(Op& o, const ByteBuffer* inData)
{
    AdminReply r(EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE, inData);
    o.mKeyDataList.clear();
    for (size_t i = 0; i + 6 <= r.Values(); i += 6) {
        typename Op::KEY_DATA aKey;
        aKey.mStatus = static_cast<WONStatus>(static_cast<short>(r.Num(i)));
        aKey.mAllocateDate = r.Num(i + 1);
        aKey.mAllocateDuration = r.Num(i + 2);
        aKey.mActivateTime = r.Num(i + 3);
        aKey.mActivateDuration = r.Num(i + 4);
        aKey.mActivateUserSeq = r.Num(i + 5);
        o.mKeyDataList.push_back(aKey);
    }
    return !r.Cut();
}

// ---- the CD key account: the reply's strings, each assigned as read; *outProfile: where the profile starts (-1:
// the read was cut before it)
template <class Op> long AdminAccountReply(Op& o, const ByteBuffer* inData)
{
    AdminReply r(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY, inData);
    for (size_t i = 0; i < r.Values(); i++) {
        if (i == 0) o.mUserName = r.WStr(i);
        else if (i == 1) o.mEmail = r.Str(i);
        else if (i == 2) o.mPassword = r.WStr(i);
        else if (i == 3) o.mLastLogin = r.Str(i);
        else o.mActivated = r.Str(i);
    }
    return r.Cut() ? -1 : static_cast<long>(r.End());
}

// ---- the version list (each record read by the version's own reader) and the module list

template <class V, class List> void AdminVersionListReply(const ByteBuffer* inData, List& outList)
{
    AdminReply r(EE_WON_ADMIN_GET_VERSION_LIST, inData);
    for (size_t i = 0; i < r.Values(); i++) {
        SmartPtr<V> aVersion = new V;
        ReadBuffer aVersionBuf(r.At(i), r.Len(i));
        aVersion->ReadFromBuffer(aVersionBuf);
        outList.push_back(aVersion);
    }
    if (r.Cut()) throw ReadBufferException("Attempt to read past end of buffer.");
}
template <class List> void AdminModuleListReply(const ByteBuffer* inData, List& outList)
{
    AdminReply r(EE_WON_ADMIN_MODULE_LIST, inData);
    for (size_t i = 0; i < r.Values(); i++) outList.push_back(static_cast<unsigned long>(r.Num(i)));
    if (r.Cut()) throw ReadBufferException("Attempt to read past end of buffer.");
}

} // namespace WONAPI
