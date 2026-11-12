// WONInternal's admin ops (the master keys, the key check, the banned keys, the key subscriptions, the CD key
// account, the version and patch requests, the version and module lists) and TitleConfig's rules: the original's
// statements (copied from MasterKeyOp.cpp, BannedKeyOp.cpp, KeySubscribeOp.cpp, GetAccountFromCDKeyOp.cpp,
// GetAccountFromCDKeyExOp.cpp, the version and patch op files, GetPatchServerModuleList.cpp and TitleConfig.cpp
// before the port, over stand-ins with the ops' members) against include/ee/wonapi/WONAdminMsg.h over ee_won's admin
// module: every request over generated members, every reply generated, cut at every length and mutated. The exit
// code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAdminMsg.h"
#include "rust/ee_rust.h"

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4141;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const string& s)
{
    static const char kHex[] = "0123456789abcdef";
    string r;
    for (size_t i = 0; i < s.size(); i++) { r += kHex[(unsigned char)s[i] >> 4]; r += kHex[s[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string HexW(const wstring& w)
{
    string b;
    WideAppendUtf16LE(b, w);
    return Hex(b);
}
static string FromSet(const char* inSet, int n)
{
    string s;
    size_t k = strlen(inSet);
    for (int i = 0; i < n; i++) s += inSet[Pick((int)k)];
    return s;
}
static string AStr() { return Pick(6) ? FromSet("ab1", Pick(5)) : FromSet("x", Pick(300)); }
static wstring AWStr()
{
    static const wchar_t* const k[] = { L"", L"c", L"\x263a", L"\x00e9t" };
    wstring s = k[Pick(4)];
    if (Pick(8) == 0) s += wstring(Pick(200), L'w');
    return s;
}
static long ANum() { return Pick(3) ? Pick(5) - 1 : (long)(Next() << 8); }
static string Bytes(WriteBuffer& w)
{
    ConstSmartPtr<ByteBuffer> b = w.ToByteBuffer(true);
    return string(b->data(), b->length());
}
static string Bytes(const ByteBuffer* b) { return string(b->data(), b->length()); }

// ---- the stand-ins (the ops' members)

struct SKeyOp {
    wstring mCommunityName;
    string mProductName;
    unsigned short mSequenceNumber;
    bool mIsBeta;
    time_t mActivateTime, mDeactivateTime, mBannedUntil;
    BYTE mKeyLength;
    BYTE mKeyP[40];
    unsigned long mCDKeyFlags;
    std::list<string> mKeyList;
    std::list<short> mStatusList, mKeyStatusList;
    struct MasterKey {
        unsigned short mSequenceNumber;
        time_t mActivateDate, mDeactivateDate;
        bool mIsBeta;
        unsigned long mCDKeyFlags;
    };
    std::list<MasterKey> mMasterKeyList;
    void Generate()
    {
        this->mCommunityName = AWStr();
        this->mProductName = AStr();
        this->mSequenceNumber = (unsigned short)Next();
        this->mIsBeta = Pick(2) != 0;
        this->mActivateTime = ANum();
        this->mDeactivateTime = ANum();
        this->mBannedUntil = ANum();
        this->mKeyLength = (BYTE)Pick(21);
        for (int i = 0; i < 40; i++) this->mKeyP[i] = (BYTE)Pick(256);
        this->mCDKeyFlags = (unsigned long)ANum();
        int n = Pick(4);
        for (int i = 0; i < n; i++) this->mKeyList.push_back(AStr().substr(0, Pick(4) ? 20 : 255));
    }
    string Dump() const
    {
        string s;
        for (std::list<short>::const_iterator i = this->mStatusList.begin(); i != this->mStatusList.end(); ++i) s += " s" + std::to_string(*i);
        for (std::list<short>::const_iterator i = this->mKeyStatusList.begin(); i != this->mKeyStatusList.end(); ++i) s += " k" + std::to_string(*i);
        for (std::list<MasterKey>::const_iterator i = this->mMasterKeyList.begin(); i != this->mMasterKeyList.end(); ++i)
            s += " m" + std::to_string(i->mSequenceNumber) + ":" + std::to_string((long long)i->mActivateDate) + ":" +
                 std::to_string((long long)i->mDeactivateDate) + ":" + std::to_string(i->mIsBeta) + ":" + std::to_string(i->mCDKeyFlags);
        return s;
    }
};
struct SQueryBanned {
    typedef std::pair<string, time_t> BANNED_KEY_PAIR;
    std::list<BANNED_KEY_PAIR> mKeyList;
    string Dump() const
    {
        string s;
        for (std::list<BANNED_KEY_PAIR>::const_iterator i = this->mKeyList.begin(); i != this->mKeyList.end(); ++i)
            s += " " + Hex(i->first) + ":" + std::to_string((long long)i->second);
        return s;
    }
};
struct SQueryListBanned {
    typedef std::pair<time_t, short> BANNED_KEY_PAIR;
    std::list<BANNED_KEY_PAIR> mKeyDataList;
    string Dump() const
    {
        string s;
        for (std::list<BANNED_KEY_PAIR>::const_iterator i = this->mKeyDataList.begin(); i != this->mKeyDataList.end(); ++i)
            s += " " + std::to_string((long long)i->first) + ":" + std::to_string(i->second);
        return s;
    }
};
struct SSubscribe {
    wstring mCommunityName;
    string mProductName;
    std::list<string> mKeyList;
    time_t mAllocateDate, mActivateTime;
    unsigned long mAllocateDuration, mActivateDuration, mActivateUserSeq;
    unsigned short mUpdateFields;
    struct KEY_DATA {
        WONStatus mStatus;
        time_t mAllocateDate, mActivateTime;
        unsigned long mAllocateDuration, mActivateDuration, mActivateUserSeq;
    };
    std::list<KEY_DATA> mKeyDataList;
    string Dump() const
    {
        string s;
        for (std::list<KEY_DATA>::const_iterator i = this->mKeyDataList.begin(); i != this->mKeyDataList.end(); ++i)
            s += " " + std::to_string((long)i->mStatus) + ":" + std::to_string((long long)i->mAllocateDate) + ":" +
                 std::to_string(i->mAllocateDuration) + ":" + std::to_string((long long)i->mActivateTime) + ":" +
                 std::to_string(i->mActivateDuration) + ":" + std::to_string(i->mActivateUserSeq);
        return s;
    }
};
struct SAccount {
    wstring mUserName, mPassword;
    string mEmail, mLastLogin, mActivated;
    SAccount() : mUserName(L"u0"), mPassword(L"p0"), mEmail("e0"), mLastLogin("l0"), mActivated("a0") {}
    string Dump() const
    {
        return HexW(this->mUserName) + " " + Hex(this->mEmail) + " " + HexW(this->mPassword) + " " + Hex(this->mLastLogin) +
               " " + Hex(this->mActivated);
    }
};
struct SVersion : public RefCount {
    string mRead;
    void ReadFromBuffer(ReadBuffer& b)
    {
        // a stand-in reader: a byte count, then that many bytes
        unsigned char n = b.ReadByte();
        this->mRead.assign(static_cast<const char*>(b.ReadBytes(n)), n);
    }
};

// ---- the original's statements

namespace Orig {
enum { WS_InvalidMessage = -12 };
static string InsertMasterKey(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    requestData.AppendShort(o.mSequenceNumber);
    requestData.AppendBool(o.mIsBeta);
    requestData.AppendLong(o.mActivateTime);
    requestData.AppendLong(o.mDeactivateTime);
    requestData.AppendByte(o.mKeyLength);
    requestData.AppendBytes(o.mKeyP, o.mKeyLength);
    requestData.AppendLong(o.mCDKeyFlags);
    return Bytes(requestData);
}
static string UpdateMasterKey(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    requestData.AppendShort(o.mSequenceNumber);
    requestData.AppendBool(o.mIsBeta);
    requestData.AppendLong(o.mActivateTime);
    requestData.AppendLong(o.mDeactivateTime);
    requestData.AppendLong(o.mCDKeyFlags);
    return Bytes(requestData);
}
static string DeleteMasterKey(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    requestData.AppendShort(o.mSequenceNumber);
    return Bytes(requestData);
}
static string Query(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    return Bytes(requestData);
}
static string AuthCheckKey(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 1);
    requestData.AppendString(o.mProductName, 1);
    requestData.AppendShort(o.mKeyList.size());
    for (std::list<string>::const_iterator anItr = o.mKeyList.begin(); anItr != o.mKeyList.end(); ++anItr)
        requestData.AppendString(*anItr, 1);
    return Bytes(requestData);
}
static string InsertBannedKey(SKeyOp& o)
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    requestData.AppendLong(o.mBannedUntil);
    requestData.AppendShort(o.mKeyList.size());
    for (std::list<std::string>::const_iterator anItr = o.mKeyList.begin(); anItr != o.mKeyList.end(); ++anItr)
        requestData.AppendString(*anItr, 1);
    return Bytes(requestData);
}
static string KeyList(SKeyOp& o)  // QueryListBannedKeyOp, RemoveBannedKeyOp
{
    WriteBuffer requestData(0, true);
    requestData.AppendWString(o.mCommunityName, 2);
    requestData.AppendString(o.mProductName);
    requestData.AppendShort(o.mKeyList.size());
    for (std::list<std::string>::const_iterator anItr = o.mKeyList.begin(); anItr != o.mKeyList.end(); ++anItr)
        requestData.AppendString(*anItr, 1);
    return Bytes(requestData);
}
static void Pack(SSubscribe& o, WriteBuffer& theBufferR)
{
    theBufferR.AppendString(o.mProductName);
    theBufferR.AppendWString(o.mCommunityName, 2);
    theBufferR.AppendShort(o.mKeyList.size());
    for (std::list<std::string>::iterator anItr = o.mKeyList.begin(); anItr != o.mKeyList.end(); ++anItr)
        theBufferR.AppendString(*anItr, 1);
}
static string Subscribe(SSubscribe& o, int inKind)   // 0 insert, 1 update, 2 remove or query
{
    WriteBuffer requestData(0, true);
    Pack(o, requestData);
    if (inKind == 0) {
        requestData.AppendLong(o.mAllocateDate);
        requestData.AppendLong(o.mAllocateDuration);
        requestData.AppendLong(o.mActivateTime);
        requestData.AppendLong(o.mActivateDuration);
    } else if (inKind == 1) {
        requestData.AppendShort(o.mUpdateFields);
        requestData.AppendLong(o.mAllocateDate);
        requestData.AppendLong(o.mAllocateDuration);
        requestData.AppendLong(o.mActivateTime);
        requestData.AppendLong(o.mActivateDuration);
        requestData.AppendLong(o.mActivateUserSeq);
    }
    return Bytes(requestData);
}

// the replies' extended unpacks (the status they return)
static WONStatus QueryMasterKeyReply(SKeyOp& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        SKeyOp::MasterKey aKey;
        o.mMasterKeyList.clear();
        short aCount = theData.ReadShort();
        for (short i = 0; i < aCount; i++) {
            aKey.mSequenceNumber = theData.ReadShort();
            aKey.mActivateDate = theData.ReadLong();
            aKey.mDeactivateDate = theData.ReadLong();
            aKey.mIsBeta = (theData.ReadByte() == 0) ? false : true;
            aKey.mCDKeyFlags = theData.ReadLong();
            o.mMasterKeyList.push_back(aKey);
        }
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
static WONStatus AuthCheckReply(SKeyOp& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        o.mKeyStatusList.clear();
        short aCount = theData.ReadShort();
        for (short i = 0; i < aCount; i++)
            o.mKeyStatusList.push_back(theData.ReadShort());
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
static WONStatus StatusReply(SKeyOp& o, const ByteBuffer* d, bool inClear)  // banned keys, subscriptions (Unpack clears)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        if (inClear) o.mStatusList.clear();
        unsigned short aListSize = theData.ReadShort();
        for (unsigned short i = 0; i < aListSize; i++)
            o.mStatusList.push_back(theData.ReadShort());
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
static WONStatus QueryBannedReply(SQueryBanned& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        unsigned short aListSize = theData.ReadShort();
        SQueryBanned::BANNED_KEY_PAIR aBannedKey;
        for (unsigned short i = 0; i < aListSize; i++) {
            theData.ReadString(aBannedKey.first, 1);
            aBannedKey.second = theData.ReadLong();
            o.mKeyList.push_back(aBannedKey);
        }
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
static WONStatus QueryListBannedReply(SQueryListBanned& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        unsigned short aListSize = theData.ReadShort();
        SQueryListBanned::BANNED_KEY_PAIR aBannedKey;
        for (unsigned short i = 0; i < aListSize; i++) {
            aBannedKey.first = theData.ReadLong();
            aBannedKey.second = theData.ReadShort();
            o.mKeyDataList.push_back(aBannedKey);
        }
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
static WONStatus QueryKeySubscribeReply(SSubscribe& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        SSubscribe::KEY_DATA aKey;
        o.mKeyDataList.clear();
        short aListSize = theData.ReadShort();
        for (short i = 0; i < aListSize; i++) {
            aKey.mStatus = (WONStatus)theData.ReadShort();
            aKey.mAllocateDate = theData.ReadLong();
            aKey.mAllocateDuration = theData.ReadLong();
            aKey.mActivateTime = theData.ReadLong();
            aKey.mActivateDuration = theData.ReadLong();
            aKey.mActivateUserSeq = theData.ReadLong();
            o.mKeyDataList.push_back(aKey);
        }
    } catch (ReadBufferException&) {
        return (WONStatus)WS_InvalidMessage;
    }
    return 0;
}
// the strings and where the profile's unpack starts (-1: thrown before it)
static long AccountReply(SAccount& o, const ByteBuffer* d)
{
    ReadBuffer theData(d->data(), d->length());
    try {
        theData.ReadWString(o.mUserName, 2);
        theData.ReadString(o.mEmail, 2);
        theData.ReadWString(o.mPassword, 2);
        theData.ReadString(o.mLastLogin, 2);
        theData.ReadString(o.mActivated, 2);
        return theData.pos();
    } catch (ReadBufferException&) {
    }
    return -1;
}
static void VersionListReply(const ByteBuffer* d, std::list<SmartPtr<SVersion> >& mVersionDataList)
{
    ReadBuffer aReadBuf(d->data(), d->length());
    unsigned short numVersions = aReadBuf.ReadShort();
    for (int i = 0; i < numVersions; i++) {
        SmartPtr<SVersion> versionData = new SVersion;
        unsigned short versionDataSize = aReadBuf.ReadShort();
        ReadBuffer aVersionBuf(aReadBuf.ReadBytes(versionDataSize), versionDataSize);
        versionData->ReadFromBuffer(aVersionBuf);
        mVersionDataList.push_back(versionData);
    }
}
static void ModuleListReply(const ByteBuffer* d, std::list<unsigned long>& mModuleList)
{
    ReadBuffer aReadBuf(d->data(), d->length());
    unsigned short numModules = aReadBuf.ReadShort();
    for (int i = 0; i < numModules; i++) {
        unsigned long moduleID = aReadBuf.ReadLong();
        mModuleList.push_back(moduleID);
    }
}
} // namespace Orig

// ---- the checks

static void CheckRequests()
{
    for (int i = 0; i < 60000; i++) {
        SKeyOp o;
        o.Generate();
        CHECK(Orig::InsertMasterKey(o) == Bytes(AdminInsertMasterKeyRequest(o)));
        CHECK(Orig::UpdateMasterKey(o) == Bytes(AdminUpdateMasterKeyRequest(o)));
        CHECK(Orig::DeleteMasterKey(o) == Bytes(AdminDeleteMasterKeyRequest(o)));
        CHECK(Orig::Query(o) == Bytes(AdminQueryRequest(o, EE_WON_ADMIN_QUERY_MASTER_KEY)));
        CHECK(Orig::Query(o) == Bytes(AdminQueryRequest(o, EE_WON_ADMIN_QUERY_BANNED_KEY)));
        CHECK(Orig::AuthCheckKey(o) == Bytes(AdminAuthCheckKeyRequest(o)));
        CHECK(Orig::InsertBannedKey(o) == Bytes(AdminInsertBannedKeyRequest(o)));
        CHECK(Orig::KeyList(o) == Bytes(AdminKeyListRequest(o, EE_WON_ADMIN_QUERY_LIST_BANNED_KEY)));
        CHECK(Orig::KeyList(o) == Bytes(AdminKeyListRequest(o, EE_WON_ADMIN_REMOVE_BANNED_KEY)));
        SSubscribe s;
        s.mCommunityName = o.mCommunityName;
        s.mProductName = o.mProductName;
        s.mKeyList = o.mKeyList;
        s.mAllocateDate = ANum(); s.mActivateTime = ANum();
        s.mAllocateDuration = ANum(); s.mActivateDuration = ANum(); s.mActivateUserSeq = ANum();
        s.mUpdateFields = (unsigned short)Next();
        AdminValues a = AdminSubscribeValues(s);
        a.Num(s.mAllocateDate).Num(s.mAllocateDuration).Num(s.mActivateTime).Num(s.mActivateDuration);
        CHECK(Orig::Subscribe(s, 0) == Bytes(a.Build(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE)));
        AdminValues b = AdminSubscribeValues(s);
        b.Num(s.mUpdateFields).Num(s.mAllocateDate).Num(s.mAllocateDuration).Num(s.mActivateTime).Num(s.mActivateDuration)
            .Num(s.mActivateUserSeq);
        CHECK(Orig::Subscribe(s, 1) == Bytes(b.Build(EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE)));
        CHECK(Orig::Subscribe(s, 2) == Bytes(AdminSubscribeValues(s).Build(EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE)));
        CHECK(Orig::Subscribe(s, 2) == Bytes(AdminSubscribeValues(s).Build(EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE)));
        // the CD key account (before the profile), the version and patch requests
        string key = AStr();
        long community = ANum();
        unsigned char action = (unsigned char)Pick(3);
        WriteBuffer c(0, true);
        c.AppendString(key, 2);
        c.AppendLong(community);
        CHECK(Bytes(c) == Bytes(AdminValues().Str(key).Num(community).Build(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY)));
        WriteBuffer e(0, true);
        e.AppendString(key, 2);
        e.AppendLong(community);
        e.AppendByte(action);
        CHECK(Bytes(e) == Bytes(AdminValues().Str(key).Num(community).Num(action).Build(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY_EX)));
        string p[5] = { AStr(), AStr(), AStr(), AStr(), AStr() };
        WriteBuffer f(0, true);
        f.AppendString(p[0]);
        CHECK(Bytes(f) == Bytes(AdminValues().Str(p[0]).Build(EE_WON_ADMIN_PRODUCT)));
        WriteBuffer g(0, true);
        for (int k = 0; k < 3; k++) g.AppendString(p[k]);
        CHECK(Bytes(g) == Bytes(AdminValues().Str(p[0]).Str(p[1]).Str(p[2]).Build(EE_WON_ADMIN_REMOVE_VERSION)));
        WriteBuffer h(0, true);
        h.AppendString(p[0]);
        h.AppendString(p[1]);
        h.AppendByte(action);
        CHECK(Bytes(h) == Bytes(AdminValues().Str(p[0]).Str(p[1]).Num(action).Build(EE_WON_ADMIN_GET_VERSION_LIST)));
        WriteBuffer j(0, true);
        for (int k = 0; k < 5; k++) j.AppendString(p[k]);
        CHECK(Bytes(j) == Bytes(AdminValues().Str(p[0]).Str(p[1]).Str(p[2]).Str(p[3]).Str(p[4]).Build(EE_WON_ADMIN_REMOVE_PATCH)));
        j.AppendByte(action ? 1 : 0);
        WriteBuffer k2(0, true);
        for (int k = 0; k < 5; k++) k2.AppendString(p[k]);
        k2.AppendByte(action ? 1 : 0);
        CHECK(Bytes(k2) == Bytes(AdminValues().Str(p[0]).Str(p[1]).Str(p[2]).Str(p[3]).Str(p[4]).Num(action ? 1 : 0)
                                     .Build(EE_WON_ADMIN_ACTIVATE_PATCH)));
    }
}

// a generated reply of `kind`: a count (sometimes negative) and its records
static string AReply(int kind)
{
    WriteBuffer w(0, true);
    int n = Pick(5) ? Pick(4) : (Pick(2) ? -1 - Pick(3) : 0);
    if (kind == 7) {   // the account
        wstring u = AWStr();
        w.AppendWString(u, 2);
        w.AppendString(AStr());
        w.AppendWString(AWStr(), 2);
        w.AppendString(AStr());
        w.AppendString(AStr());
        w.AppendBytes("prof", 4);
        return Bytes(w);
    }
    w.AppendShort((short)n);
    int count = n < 0 ? 3 : n;
    for (int i = 0; i < count; i++) {
        switch (kind) {
        case 0: w.AppendShort((short)Next()); w.AppendLong(ANum()); w.AppendLong(ANum()); w.AppendByte((BYTE)Pick(3)); w.AppendLong(ANum()); break;
        case 1: case 2: w.AppendShort((short)Next()); break;
        case 3: w.AppendString(AStr().substr(0, 40), 1); w.AppendLong(ANum()); break;
        case 4: w.AppendLong(ANum()); w.AppendShort((short)Next()); break;
        case 5: for (int k = 0; k < 6; k++) { if (k == 0) w.AppendShort((short)Next()); else w.AppendLong(ANum()); } break;
        case 6: {
            string r = FromSet("v", Pick(5));
            string rec = string(1, (char)r.size()) + r;
            if (Pick(5) == 0) rec = rec.substr(0, Pick((int)rec.size() + 1));
            w.AppendShort((short)rec.size());
            w.AppendBytes(rec.data(), (unsigned long)rec.size());
        } break;
        default: w.AppendLong(ANum()); break;
        }
    }
    return Bytes(w);
}

template <class F> static string Run(F f)
{
    try {
        return f();
    } catch (ReadBufferException&) {
        return "throw";
    }
}

static void CheckReplyOn(int kind, const string& m)
{
    ConstSmartPtr<ByteBuffer> d = new ByteBuffer(m.data(), (unsigned long)m.size(), false);
    string a, b;
    switch (kind) {
    case 0: {
        SKeyOp x, y;
        SKeyOp::MasterKey k = { 1, 2, 3, true, 4 };
        x.mMasterKeyList.push_back(k);
        y.mMasterKeyList.push_back(k);
        a = std::to_string(Orig::QueryMasterKeyReply(x, d.get())) + x.Dump();
        b = std::to_string(AdminQueryMasterKeyReply(y, d.get()) ? 0 : -12) + y.Dump();
    } break;
    case 1: {
        SKeyOp x, y;
        x.mKeyStatusList.push_back(9);
        y.mKeyStatusList.push_back(9);
        x.mStatusList.push_back(8);
        y.mStatusList.push_back(8);
        a = std::to_string(Orig::AuthCheckReply(x, d.get())) + "|" + std::to_string(Orig::StatusReply(x, d.get(), false)) + x.Dump();
        b = std::to_string(AdminStatusReply(EE_WON_ADMIN_AUTH_CHECK_KEY, d.get(), y.mKeyStatusList, true) ? 0 : -12) + "|" +
            std::to_string(AdminStatusReply(EE_WON_ADMIN_INSERT_BANNED_KEY, d.get(), y.mStatusList, false) ? 0 : -12) + y.Dump();
    } break;
    case 2: {
        SKeyOp x, y;
        x.mStatusList.push_back(8);
        y.mStatusList.push_back(8);
        a = std::to_string(Orig::StatusReply(x, d.get(), true)) + x.Dump();
        b = std::to_string(AdminStatusReply(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE, d.get(), y.mStatusList, true) ? 0 : -12) + y.Dump();
    } break;
    case 3: {
        SQueryBanned x, y;
        a = std::to_string(Orig::QueryBannedReply(x, d.get())) + x.Dump();
        b = std::to_string(AdminQueryBannedKeyReply(y, d.get()) ? 0 : -12) + y.Dump();
    } break;
    case 4: {
        SQueryListBanned x, y;
        a = std::to_string(Orig::QueryListBannedReply(x, d.get())) + x.Dump();
        b = std::to_string(AdminQueryListBannedKeyReply(y, d.get()) ? 0 : -12) + y.Dump();
    } break;
    case 5: {
        SSubscribe x, y;
        SSubscribe::KEY_DATA k = { 1, 2, 3, 4, 5, 6 };
        x.mKeyDataList.push_back(k);
        y.mKeyDataList.push_back(k);
        a = std::to_string(Orig::QueryKeySubscribeReply(x, d.get())) + x.Dump();
        b = std::to_string(AdminQueryKeySubscribeReply(y, d.get()) ? 0 : -12) + y.Dump();
    } break;
    case 6: {
        std::list<SmartPtr<SVersion> > x, y;
        a = Run([&]() { Orig::VersionListReply(d.get(), x); return string("ok"); });
        b = Run([&]() { AdminVersionListReply<SVersion>(d.get(), y); return string("ok"); });
        for (std::list<SmartPtr<SVersion> >::iterator i = x.begin(); i != x.end(); ++i) a += " " + Hex((*i)->mRead);
        for (std::list<SmartPtr<SVersion> >::iterator i = y.begin(); i != y.end(); ++i) b += " " + Hex((*i)->mRead);
        std::list<unsigned long> mx, my;
        a += Run([&]() { Orig::ModuleListReply(d.get(), mx); return string("ok"); });
        b += Run([&]() { AdminModuleListReply(d.get(), my); return string("ok"); });
        for (std::list<unsigned long>::iterator i = mx.begin(); i != mx.end(); ++i) a += " " + std::to_string(*i);
        for (std::list<unsigned long>::iterator i = my.begin(); i != my.end(); ++i) b += " " + std::to_string(*i);
    } break;
    default: {
        SAccount x, y;
        a = std::to_string(Orig::AccountReply(x, d.get())) + " " + x.Dump();
        b = std::to_string(AdminAccountReply(y, d.get())) + " " + y.Dump();
    } break;
    }
    CHECK(a == b);
    if (a != b && sFailures < 8) printf("reply %d %s:\n  %s\n  %s\n", kind, Hex(m).c_str(), a.c_str(), b.c_str());
}

static void CheckReplies()
{
    for (int i = 0; i < 64000; i++) {
        int kind = i % 8;
        string m = AReply(kind);
        CheckReplyOn(kind, m);
        for (size_t n = 0; n < m.size(); n++) CheckReplyOn(kind, m.substr(0, n));
        for (int j = 0; j < 3 && !m.empty(); j++) {
            string x = m;
            x[Pick((int)x.size())] = (char)Pick(256);
            CheckReplyOn(kind, x);
        }
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    CheckRequests();
    CheckReplies();
    printf("won_admin_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
