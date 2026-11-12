// WONInternal's admin ops as whole ops (the master key, key check, banned key, key subscription, CD key account,
// version, patch and module list ops, each a DBProxyOp): the original's RunHook, GetNextRequest and CheckResponse
// (copied from the C++ op classes before the port, over stand-ins with the ops' members, the messages through
// tests/won/won_admin_msg.h and tests/won/won_db_msg.h, which won_admin_test and won_db_test check against the
// pre-port statements) against the Rust ops (ee_won adminop.rs) through their C ABI. Each sequence: generated
// members, the request, then up to five generated replies (DB proxy headers right and wrong, record lists whole, cut
// and mutated, replies spread over several messages), each reply's verdict and the op's members after it.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_adminop.txt, recorded
// before the port; `adminop::tests::golden` replays it). The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "won_profiledata.h"
#include "won_db_msg.h"
#include "ee/wonapi/WONPatchMsg.h"
#include "won_admin_msg.h"
#include "rust/ee_won.h"

// 0 records the golden from the original's half alone
#ifndef ADMINOP_RUST
#define ADMINOP_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7171;
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
static string AStr() { return Pick(8) ? FromSet("ab1,", Pick(6)) : FromSet("x", Pick(300)); }
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
static string Bytes(const ByteBuffer* b) { return b == NULL ? string() : string(b->data(), b->length()); }
static string Num(long long v) { return std::to_string(v); }

// ---- the kinds (ee_won adminop.rs)

enum {
    K_INSERT_MASTER = 1, K_UPDATE_MASTER, K_DELETE_MASTER, K_QUERY_MASTER, K_AUTH_CHECK, K_INSERT_BANNED,
    K_QUERY_BANNED, K_QUERY_LIST_BANNED, K_REMOVE_BANNED, K_INSERT_SUB, K_UPDATE_SUB, K_REMOVE_SUB, K_QUERY_SUB,
    K_ACCOUNT, K_ACCOUNT_EX,
    K_ADD_VERSION = 30, K_UPDATE_VERSION, K_REMOVE_VERSION, K_GET_VERSION_LIST, K_ADD_PATCH, K_REMOVE_PATCH,
    K_ACTIVATE_PATCH, K_PATCH_CLEANUP, K_MODULE_LIST,
    K_INFO_EMAIL = 40, K_PASSWORD_EMAIL, K_GET_PATCH_LIST
};
static const int kKinds[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 30, 31, 32, 33, 34, 35, 36, 37, 38,
                              40, 41, 42 };

// the verdicts beside a status
static const char* const kInv = "inv";   // InvalidReplyHeader()
static const char* const kCut = "cut";   // a ReadBufferException

// ---- the members handed over (the OP line, and the Rust op's values)

struct Values {
    std::vector<string> mTok;     // "n<dec>" or "b<hex>"
    std::list<string> mKeep;
#if ADMINOP_RUST
    std::vector<EeWonAdminValue> mV;
#endif
    void N(long long v)
    {
        this->mTok.push_back("n" + Num(v));
#if ADMINOP_RUST
        EeWonAdminValue x = {static_cast<int64_t>(v), NULL, 0};
        this->mV.push_back(x);
#endif
    }
    void S(const string& s)
    {
        this->mKeep.push_back(s);
        this->mTok.push_back("b" + (s.empty() ? string() : Hex(s)));
#if ADMINOP_RUST
        EeWonAdminValue x = {0, reinterpret_cast<const uint8_t*>(this->mKeep.back().data()), this->mKeep.back().size()};
        this->mV.push_back(x);
#endif
    }
    void W(const wstring& w)
    {
        string b;
        WideAppendUtf16LE(b, w);
        this->S(b);
    }
    void Keys(const std::list<string>& k)
    {
        this->N((long long)k.size());
        for (std::list<string>::const_iterator i = k.begin(); i != k.end(); ++i) this->S(*i);
    }
    string Line() const
    {
        string s;
        for (size_t i = 0; i < this->mTok.size(); i++) s += " " + this->mTok[i];
        return s;
    }
};

// ---- the stand-ins: DBProxyOp's members and statements

struct SBase {
    unsigned char mLengthFieldSize;
    short mMessageType, mMessageReplyType, mSubMessageType, mSubMessageReplyType;
    ByteBufferPtr mRequestData, mRequest, mResponse, mReplyData;
    WriteBuffer mReceivedData;
    string mReplyErrorString;
    SBase()
        : mLengthFieldSize(4), mMessageType(0), mMessageReplyType(0), mSubMessageType(0), mSubMessageReplyType(0),
          mReceivedData(0, true), mReplyErrorString("")
    {
    }
    // DBProxyOp::GetNextRequest
    WONStatus GetNextRequest()
    {
        DbRequest q(EE_WON_DB_REQ_PROXY, this->mLengthFieldSize);
        q.mReq.message = this->mMessageType;
        q.mReq.sub = this->mSubMessageType;
        q.mReq.data = DbRequest::Buffer(this->mRequestData.get());
        this->mRequest = q.Build();
        return WS_ServerReq_Recv;
    }
    // DBProxyOp::CheckResponse (InvalidReplyHeader() as *outInv)
    WONStatus Check(bool* outInv)
    {
        bool anInvalid = false;
        WONStatus aStatus = DbProxyReply(this->mResponse.get(), this->mMessageReplyType, this->mSubMessageReplyType,
                                         this->mReplyErrorString, this->mReceivedData, this->mReplyData, &anInvalid);
        *outInv = anInvalid;
        return aStatus;
    }
    string Dump() const
    {
        return "t" + Num(this->mMessageReplyType) + "/" + Num(this->mSubMessageReplyType) + " e" +
               Hex(this->mReplyErrorString) + " d" + (this->mReplyData.get() == NULL ? string("n") : "x" + Hex(Bytes(this->mReplyData.get()))) +
               " r" + Num((long long)this->mReceivedData.length());
    }
};

// the ops' members (each family its own, as the C++ classes had them)
struct SKey : SBase {
    wstring mCommunityName;
    string mProductName;
    unsigned short mSequenceNumber;
    bool mIsBeta;
    time_t mActivateTime, mDeactivateTime, mBannedUntil;
    BYTE mKeyLength;
    BYTE mKeyP[20];
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
};
struct SQueryBanned : SBase {
    wstring mCommunityName;
    string mProductName;
    typedef std::pair<string, time_t> BANNED_KEY_PAIR;
    std::list<BANNED_KEY_PAIR> mKeyList;
};
struct SQueryListBanned : SBase {
    wstring mCommunityName;
    string mProductName;
    std::list<string> mKeyList;
    typedef std::pair<time_t, short> BANNED_KEY_PAIR;
    std::list<BANNED_KEY_PAIR> mKeyDataList;
};
struct SSub : SBase {
    wstring mCommunityName;
    string mProductName;
    std::list<string> mKeyList;
    std::list<short> mStatusList;
    unsigned short mUpdateFields;
    time_t mAllocateDate, mActivateTime;
    unsigned long mAllocateDuration, mActivateDuration, mActivateUserSeq;
    struct KEY_DATA {
        WONStatus mStatus;
        time_t mAllocateDate;
        unsigned long mAllocateDuration;
        time_t mActivateTime;
        unsigned long mActivateDuration;
        unsigned long mActivateUserSeq;
    };
    std::list<KEY_DATA> mKeyDataList;
};
struct SAccount : SBase {
    string mCDKey;
    unsigned long mCommunity;
    int mActionFlag;
    wstring mUserName, mPassword;
    string mEmail, mLastLogin, mActivated;
    SmartPtr<ProfileData> mProfileData;
};
// VersionData and PatchData (their statements through WONPatchMsg.h, as PatchTypes.cpp)
struct SVersion : public RefCount {
    string mConfigName, mVersion, mDescriptionURL;
    VersionState mState;
    VersionTypeList mVersionTypeList;
    SVersion() : mState(VersionState_Invalid) {}
    void SetConfigName(const string& s) { this->mConfigName = s; }
    void SetVersion(const string& s) { this->mVersion = s; }
    void SetDescriptionURL(const string& s) { this->mDescriptionURL = s; }
    void SetState(VersionState s) { this->mState = s; }
    const string& GetConfigName() const { return this->mConfigName; }
    const string& GetVersion() const { return this->mVersion; }
    const string& GetDescriptionURL() const { return this->mDescriptionURL; }
    VersionState GetState() const { return this->mState; }
    const VersionTypeList& GetVersionTypeList() const { return this->mVersionTypeList; }
    void AddVersionType(const string& s) { PatchAddVersionType(this->mVersionTypeList, s); }
    void ReadFromBuffer(ReadBuffer& b) { PatchReadVersion(*this, b); }
    void WriteToBuffer(WriteBuffer& b, DWORD f) { PatchWriteVersion(*this, b, f); }
    string Dump() const
    {
        string t;
        for (VersionTypeList::const_iterator i = this->mVersionTypeList.begin(); i != this->mVersionTypeList.end(); ++i)
            t += (t.empty() ? "" : "+") + Hex(*i);
        return Hex(this->mConfigName) + "," + Hex(this->mVersion) + "," + (t.empty() ? string("-") : t) + "," +
               Hex(this->mDescriptionURL) + "," + Num(this->mState);
    }
};
struct SPatch : public RefCount {
    string mConfigName, mFromVersion, mToVersion, mDisplayName, mNetAddress;
    unsigned long mCheckSum, mPatchSize;
    string mHostName, mHostURL, mHostText, mHostImageURL;
    bool mMustVisitHost, mIsActive;
    SPatch() : mCheckSum(0), mPatchSize(0), mMustVisitHost(false), mIsActive(false) {}
    const string& GetConfigName() const { return this->mConfigName; }
    const string& GetFromVersion() const { return this->mFromVersion; }
    const string& GetToVersion() const { return this->mToVersion; }
    const string& GetDisplayName() const { return this->mDisplayName; }
    const string& GetNetAddress() const { return this->mNetAddress; }
    unsigned long GetCheckSum() const { return this->mCheckSum; }
    unsigned long GetPatchSize() const { return this->mPatchSize; }
    const string& GetHostName() const { return this->mHostName; }
    const string& GetHostURL() const { return this->mHostURL; }
    const string& GetHostText() const { return this->mHostText; }
    const string& GetHostImageURL() const { return this->mHostImageURL; }
    bool GetMustVisitHost() const { return this->mMustVisitHost; }
    bool GetIsActive() const { return this->mIsActive; }
    void WriteToBuffer(WriteBuffer& b, bool u) { PatchWritePatch(*this, b, u); }
    void SetConfigName(const string& s) { this->mConfigName = s; }
    void SetFromVersion(const string& s) { this->mFromVersion = s; }
    void SetToVersion(const string& s) { this->mToVersion = s; }
    void SetDisplayName(const string& s) { this->mDisplayName = s; }
    void SetNetAddress(const string& s) { this->mNetAddress = s; }
    void SetCheckSum(unsigned long v) { this->mCheckSum = v; }
    void SetPatchSize(unsigned long v) { this->mPatchSize = v; }
    void SetHostName(const string& s) { this->mHostName = s; }
    void SetHostURL(const string& s) { this->mHostURL = s; }
    void SetHostText(const string& s) { this->mHostText = s; }
    void SetHostImageURL(const string& s) { this->mHostImageURL = s; }
    void SetMustVisitHost(bool b) { this->mMustVisitHost = b; }
    void SetIsActive(bool b) { this->mIsActive = b; }
    void ReadFromBuffer(ReadBuffer& b, bool inList) { PatchReadPatch(*this, b, inList); }
    string Dump() const
    {
        return Hex(this->mConfigName) + "," + Hex(this->mFromVersion) + "," + Hex(this->mToVersion) + "," +
               Hex(this->mDisplayName) + "," + Hex(this->mNetAddress) + "," + Num((uint32_t)this->mCheckSum) + "," +
               Num((uint32_t)this->mPatchSize) + "," + Hex(this->mHostName) + "," + Hex(this->mHostURL) + "," +
               Hex(this->mHostText) + "," + Hex(this->mHostImageURL) + "," + Num(this->mMustVisitHost) + "," +
               Num(this->mIsActive);
    }
};
// WONDB's email requests and WONMisc's patch list
struct SEmail : SBase {
    unsigned short mMsgType;
    string mEmail;
    wstring mUserName;
    SmartPtr<ProfileData> mProfileData;
};
struct SPatchList : SBase {
    unsigned short mMsgType;
    string mProductName, mConfigName, mFromVersion, mToVersion;
    char mStateFilter;
    std::list<SmartPtr<SPatch> > mPatchDataList;
};
struct SVer : SBase {
    unsigned short mMsgType;
    string mProductName, mConfigName, mVersion, mFromVersion, mToVersion, mNetAddress;
    VersionState mStateFilter;
    BOOL mIsActive;
    bool mIsUpdate;
    SmartPtr<SVersion> mVersionData;
    SmartPtr<SPatch> mPatchData;
    std::list<SmartPtr<SVersion> > mVersionDataList;
    std::list<unsigned long> mModuleList;
};

// ---- an op: its kind, its members, the original's statements

struct Op {
    int mKind;
    Values mValues;
    SKey mKey;
    SQueryBanned mQB;
    SQueryListBanned mQLB;
    SSub mSub;
    SAccount mAcct;
    SVer mVer;
    SEmail mEmail;
    SPatchList mPL;

    SBase& Base()
    {
        switch (this->mKind) {
        case K_QUERY_BANNED: return this->mQB;
        case K_QUERY_LIST_BANNED: return this->mQLB;
        case K_INSERT_SUB: case K_UPDATE_SUB: case K_REMOVE_SUB: case K_QUERY_SUB: return this->mSub;
        case K_ACCOUNT: case K_ACCOUNT_EX: return this->mAcct;
        case K_INFO_EMAIL: case K_PASSWORD_EMAIL: return this->mEmail;
        case K_GET_PATCH_LIST: return this->mPL;
        default: return this->mKind >= K_ADD_VERSION ? static_cast<SBase&>(this->mVer) : this->mKey;
        }
    }

    static std::list<string> AKeys()
    {
        std::list<string> l;
        int n = Pick(4);
        for (int i = 0; i < n; i++) l.push_back(AStr().substr(0, Pick(4) ? 20 : 255));
        return l;
    }

    // the constructors' defaults, then generated members (each set or left)
    void Generate(int inKind)
    {
        this->mKind = inKind;
        Values& v = this->mValues;
        bool gen = Pick(6) != 0;
        if (inKind <= K_REMOVE_BANNED) {
            SKey& o = this->mKey;
            o.mCommunityName = L""; o.mProductName = ""; o.mSequenceNumber = 0; o.mIsBeta = false;
            o.mActivateTime = o.mDeactivateTime = o.mBannedUntil = 0; o.mKeyLength = 0; o.mCDKeyFlags = 1;
            memset(o.mKeyP, 0, 20);
            if (inKind == K_QUERY_BANNED) {
                if (gen) { this->mQB.mCommunityName = AWStr(); this->mQB.mProductName = AStr(); }
                v.W(this->mQB.mCommunityName); v.S(this->mQB.mProductName);
                return;
            }
            if (inKind == K_QUERY_LIST_BANNED) {
                if (gen) { this->mQLB.mCommunityName = AWStr(); this->mQLB.mProductName = AStr(); this->mQLB.mKeyList = AKeys(); }
                v.W(this->mQLB.mCommunityName); v.S(this->mQLB.mProductName); v.Keys(this->mQLB.mKeyList);
                return;
            }
            if (gen) {
                o.mCommunityName = AWStr(); o.mProductName = AStr(); o.mSequenceNumber = (unsigned short)Next();
                o.mIsBeta = Pick(2) != 0; o.mActivateTime = ANum(); o.mDeactivateTime = ANum(); o.mBannedUntil = ANum();
                o.mKeyLength = (BYTE)Pick(21); for (int i = 0; i < 20; i++) o.mKeyP[i] = (BYTE)Pick(256);
                o.mCDKeyFlags = (unsigned long)ANum(); o.mKeyList = AKeys();
            }
            v.W(o.mCommunityName); v.S(o.mProductName);
            switch (inKind) {
            case K_INSERT_MASTER:
                v.N(o.mSequenceNumber); v.N(o.mIsBeta); v.N(o.mActivateTime); v.N(o.mDeactivateTime); v.N(o.mKeyLength);
                v.S(string((const char*)o.mKeyP, 20)); v.N(o.mCDKeyFlags);
                break;
            case K_UPDATE_MASTER:
                v.N(o.mSequenceNumber); v.N(o.mIsBeta); v.N(o.mActivateTime); v.N(o.mDeactivateTime); v.N(o.mCDKeyFlags);
                break;
            case K_DELETE_MASTER: v.N(o.mSequenceNumber); break;
            case K_QUERY_MASTER: break;
            case K_INSERT_BANNED: v.N(o.mBannedUntil); v.Keys(o.mKeyList); break;
            default: v.Keys(o.mKeyList); break;   // the key check, the removal
            }
            return;
        }
        if (inKind <= K_QUERY_SUB) {
            SSub& o = this->mSub;
            o.mCommunityName = L""; o.mProductName = ""; o.mUpdateFields = 0; o.mAllocateDate = -1;
            o.mAllocateDuration = 0; o.mActivateTime = -1; o.mActivateDuration = 0; o.mActivateUserSeq = 0;
            if (gen) {
                o.mCommunityName = AWStr(); o.mProductName = AStr(); o.mKeyList = AKeys();
                o.mUpdateFields = (unsigned short)Next(); o.mAllocateDate = ANum(); o.mAllocateDuration = (unsigned long)ANum();
                o.mActivateTime = ANum(); o.mActivateDuration = (unsigned long)ANum(); o.mActivateUserSeq = (unsigned long)ANum();
            }
            v.S(o.mProductName); v.W(o.mCommunityName); v.Keys(o.mKeyList);
            if (inKind == K_INSERT_SUB) {
                v.N(o.mAllocateDate); v.N(o.mAllocateDuration); v.N(o.mActivateTime); v.N(o.mActivateDuration);
            } else if (inKind == K_UPDATE_SUB) {
                v.N(o.mUpdateFields); v.N(o.mAllocateDate); v.N(o.mAllocateDuration); v.N(o.mActivateTime);
                v.N(o.mActivateDuration); v.N(o.mActivateUserSeq);
            }
            return;
        }
        if (inKind == K_INFO_EMAIL || inKind == K_PASSWORD_EMAIL) {
            SEmail& o = this->mEmail;
            o.mMsgType = inKind == K_INFO_EMAIL ? 14 : 8;
            o.mProfileData = new ProfileData;
            if (gen) { o.mEmail = AStr(); o.mUserName = AWStr(); }
            if (inKind == K_INFO_EMAIL) v.S(o.mEmail); else v.W(o.mUserName);
            return;
        }
        if (inKind == K_GET_PATCH_LIST) {
            SPatchList& o = this->mPL;
            o.mMsgType = 17;
            o.mProductName = ""; o.mConfigName = ""; o.mFromVersion = ""; o.mToVersion = ""; o.mStateFilter = 2;
            if (gen) { o.mProductName = AStr(); o.mConfigName = AStr(); o.mFromVersion = AStr(); o.mToVersion = AStr(); o.mStateFilter = (char)Pick(256); }
            v.S(o.mProductName); v.S(o.mConfigName); v.S(o.mFromVersion); v.S(o.mToVersion); v.N((unsigned char)o.mStateFilter);
            return;
        }
        if (inKind <= K_ACCOUNT_EX) {
            SAccount& o = this->mAcct;
            o.mCDKey = gen ? AStr() : string(""); o.mCommunity = gen ? (unsigned long)ANum() : 0;
            o.mActionFlag = 0;
            if (inKind == K_ACCOUNT_EX && gen) o.mActionFlag = Pick(3);
            o.mUserName = L""; o.mPassword = L""; o.mEmail = ""; o.mLastLogin = "";
            o.mProfileData = new ProfileData();
            v.S(o.mCDKey); v.N(o.mCommunity);
            if (inKind == K_ACCOUNT_EX) v.N(o.mActionFlag);
            return;
        }
        SVer& o = this->mVer;
        static const unsigned short kMsg[] = { 3, 7, 5, 9, 11, 13, 15, 21, 1 };
        o.mMsgType = kMsg[inKind - K_ADD_VERSION];
        o.mProductName = ""; o.mConfigName = ""; o.mVersion = ""; o.mFromVersion = ""; o.mToVersion = "";
        o.mNetAddress = ""; o.mIsActive = FALSE; o.mIsUpdate = false; o.mStateFilter = VersionState_BadState;
        o.mVersionData = new SVersion; o.mPatchData = new SPatch;
        if (inKind == K_GET_VERSION_LIST) {
            o.mConfigName = "%";
            if (gen) { o.mProductName = AStr(); if (Pick(2)) o.mConfigName = AStr(); o.mStateFilter = (VersionState)Pick(5); }
            v.S(o.mProductName); v.S(o.mConfigName); v.N(o.mStateFilter);
            return;
        }
        if (gen) {
            o.mProductName = AStr(); o.mConfigName = AStr(); o.mVersion = AStr(); o.mFromVersion = AStr();
            o.mToVersion = AStr(); o.mNetAddress = AStr(); o.mIsActive = Pick(3); o.mIsUpdate = Pick(2) != 0;
            SVersion& d = *o.mVersionData.get();
            d.SetConfigName(AStr()); d.SetVersion(AStr()); d.SetDescriptionURL(AStr()); d.SetState((VersionState)Pick(4));
            for (int n = Pick(3); n > 0; n--) d.AddVersionType(AStr());
            SPatch& p = *o.mPatchData.get();
            p.mConfigName = AStr(); p.mFromVersion = AStr(); p.mToVersion = AStr(); p.mDisplayName = AStr();
            p.mNetAddress = AStr(); p.mCheckSum = (unsigned long)ANum(); p.mPatchSize = (unsigned long)ANum();
            p.mHostName = AStr(); p.mHostURL = AStr(); p.mHostText = AStr(); p.mHostImageURL = AStr();
            p.mMustVisitHost = Pick(2) != 0; p.mIsActive = Pick(2) != 0;
        }
        switch (inKind) {
        case K_ADD_VERSION: case K_UPDATE_VERSION: {
            SVersion& d = *o.mVersionData.get();
            v.S(o.mProductName); v.S(d.mConfigName); v.S(d.mVersion); v.N((long long)d.mVersionTypeList.size());
            for (VersionTypeList::const_iterator i = d.mVersionTypeList.begin(); i != d.mVersionTypeList.end(); ++i) v.S(*i);
            v.S(d.mDescriptionURL); v.N(d.mState);
            break;
        }
        case K_REMOVE_VERSION: v.S(o.mProductName); v.S(o.mConfigName); v.S(o.mVersion); break;
        case K_ADD_PATCH: {
            SPatch& p = *o.mPatchData.get();
            v.S(o.mProductName); v.S(p.mConfigName); v.S(p.mFromVersion); v.S(p.mToVersion); v.S(p.mDisplayName);
            v.S(p.mNetAddress); v.N(p.mCheckSum); v.N(p.mPatchSize); v.S(p.mHostName); v.S(p.mHostURL);
            v.S(p.mHostText); v.S(p.mHostImageURL); v.N(p.mMustVisitHost); v.N(p.mIsActive); v.N(o.mIsUpdate);
            break;
        }
        case K_REMOVE_PATCH: case K_ACTIVATE_PATCH:
            v.S(o.mProductName); v.S(o.mConfigName); v.S(o.mFromVersion); v.S(o.mToVersion); v.S(o.mNetAddress);
            if (inKind == K_ACTIVATE_PATCH) v.N(o.mIsActive);
            break;
        default: break;   // the cleanup and the module list have no members
        }
    }

    // each op's RunHook before DBProxyOp::RunHook
    void RunHook()
    {
        switch (this->mKind) {
        case K_INSERT_MASTER: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 18; o.mRequestData = AdminInsertMasterKeyRequest(o); break; }
        case K_UPDATE_MASTER: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 22; o.mRequestData = AdminUpdateMasterKeyRequest(o); break; }
        case K_DELETE_MASTER: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 24; o.mRequestData = AdminDeleteMasterKeyRequest(o); break; }
        case K_QUERY_MASTER: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 26; o.mRequestData = AdminQueryRequest(o, EE_WON_ADMIN_QUERY_MASTER_KEY); break; }
        case K_AUTH_CHECK: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 28; o.mRequestData = AdminAuthCheckKeyRequest(o); break; }
        case K_INSERT_BANNED: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 20; o.mStatusList.clear(); o.mRequestData = AdminInsertBannedKeyRequest(o); break; }
        case K_QUERY_BANNED: { SQueryBanned& o = this->mQB; o.mMessageType = 1002; o.mSubMessageType = 32; o.mKeyList.clear(); o.mRequestData = AdminQueryRequest(o, EE_WON_ADMIN_QUERY_BANNED_KEY); break; }
        case K_QUERY_LIST_BANNED: { SQueryListBanned& o = this->mQLB; o.mMessageType = 1002; o.mSubMessageType = 34; o.mKeyDataList.clear(); o.mRequestData = AdminKeyListRequest(o, EE_WON_ADMIN_QUERY_LIST_BANNED_KEY); break; }
        case K_REMOVE_BANNED: { SKey& o = this->mKey; o.mMessageType = 1002; o.mSubMessageType = 30; o.mStatusList.clear(); o.mRequestData = AdminKeyListRequest(o, EE_WON_ADMIN_REMOVE_BANNED_KEY); break; }
        case K_INSERT_SUB: {
            SSub& o = this->mSub; o.mMessageType = 1006; o.mSubMessageType = 1;
            AdminValues aValues = AdminSubscribeValues(o);
            aValues.Num(o.mAllocateDate).Num(o.mAllocateDuration).Num(o.mActivateTime).Num(o.mActivateDuration);
            o.mRequestData = aValues.Build(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE);
            break;
        }
        case K_UPDATE_SUB: {
            SSub& o = this->mSub; o.mMessageType = 1006; o.mSubMessageType = 5;
            AdminValues aValues = AdminSubscribeValues(o);
            aValues.Num(o.mUpdateFields).Num(o.mAllocateDate).Num(o.mAllocateDuration).Num(o.mActivateTime)
                .Num(o.mActivateDuration).Num(o.mActivateUserSeq);
            o.mRequestData = aValues.Build(EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE);
            break;
        }
        case K_REMOVE_SUB: { SSub& o = this->mSub; o.mMessageType = 1006; o.mSubMessageType = 7; o.mRequestData = AdminSubscribeValues(o).Build(EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE); break; }
        case K_QUERY_SUB: { SSub& o = this->mSub; o.mMessageType = 1006; o.mSubMessageType = 3; o.mRequestData = AdminSubscribeValues(o).Build(EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE); break; }
        case K_ACCOUNT: case K_ACCOUNT_EX: {
            SAccount& o = this->mAcct;
            o.mMessageType = 1002;
            o.mSubMessageType = this->mKind == K_ACCOUNT ? 10 : 12;
            WriteBuffer requestData(0, true);
            if (this->mKind == K_ACCOUNT)
                AdminValues().Str(o.mCDKey).Num(o.mCommunity).AppendTo(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY, requestData);
            else
                AdminValues().Str(o.mCDKey).Num(o.mCommunity).Num(o.mActionFlag).AppendTo(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY_EX, requestData);
            o.mProfileData->PackRequestData(requestData, true);
            o.mRequestData = requestData.ToByteBuffer(true);
            break;
        }
        case K_INFO_EMAIL: case K_PASSWORD_EMAIL: {
            SEmail& o = this->mEmail;
            o.mMessageType = 1002;
            o.mSubMessageType = o.mMsgType;
            if (this->mKind == K_INFO_EMAIL) {
                DbRequest q(EE_WON_DB_REQ_INFO_EMAIL, 0);
                q.mReq.email = DbRequest::Narrow(o.mEmail);
                q.mReq.profile = o.mProfileData->GetProfile();
                o.mRequestData = q.Build();
            } else {
                DbRequest q(EE_WON_DB_REQ_PASSWORD_EMAIL, 0);
                q.mReq.name = q.Text(o.mUserName);
                q.mReq.profile = o.mProfileData->GetProfile();
                o.mRequestData = q.Build();
            }
            break;
        }
        case K_GET_PATCH_LIST: {
            SPatchList& o = this->mPL;
            o.mMessageType = 1004;
            o.mSubMessageType = o.mMsgType;
            std::vector<std::string> aStrings;
            aStrings.push_back(o.mProductName);
            aStrings.push_back(o.mConfigName);
            aStrings.push_back(o.mFromVersion);
            aStrings.push_back(o.mToVersion);
            o.mRequestData = PatchRequest(1, aStrings, o.mStateFilter);
            break;
        }
        default: {
            SVer& o = this->mVer;
            o.mMessageType = this->mKind == K_MODULE_LIST ? 6 : 1004;
            o.mSubMessageType = o.mMsgType;
            if (this->mKind == K_MODULE_LIST) break;
            WriteBuffer requestData(0, true);
            switch (this->mKind) {
            case K_ADD_VERSION:
                AdminValues().Str(o.mProductName).AppendTo(EE_WON_ADMIN_PRODUCT, requestData);
                o.mVersionData->WriteToBuffer(requestData, VersionData::WriteFlag_AppendDescripURL);
                break;
            case K_UPDATE_VERSION:
                AdminValues().Str(o.mProductName).AppendTo(EE_WON_ADMIN_PRODUCT, requestData);
                o.mVersionData->WriteToBuffer(requestData, VersionData::WriteFlag_AppendDescripURL | VersionData::WriteFlag_AppendState);
                break;
            case K_REMOVE_VERSION:
                AdminValues().Str(o.mProductName).Str(o.mConfigName).Str(o.mVersion).AppendTo(EE_WON_ADMIN_REMOVE_VERSION, requestData);
                break;
            case K_GET_VERSION_LIST:
                AdminValues().Str(o.mProductName).Str(o.mConfigName).Num(o.mStateFilter).AppendTo(EE_WON_ADMIN_GET_VERSION_LIST, requestData);
                break;
            case K_ADD_PATCH:
                AdminValues().Str(o.mProductName).AppendTo(EE_WON_ADMIN_PRODUCT, requestData);
                o.mPatchData->WriteToBuffer(requestData, o.mIsUpdate);
                break;
            case K_REMOVE_PATCH:
                AdminValues().Str(o.mProductName).Str(o.mConfigName).Str(o.mFromVersion).Str(o.mToVersion)
                    .Str(o.mNetAddress).AppendTo(EE_WON_ADMIN_REMOVE_PATCH, requestData);
                break;
            case K_ACTIVATE_PATCH:
                AdminValues().Str(o.mProductName).Str(o.mConfigName).Str(o.mFromVersion).Str(o.mToVersion)
                    .Str(o.mNetAddress).Num(o.mIsActive).AppendTo(EE_WON_ADMIN_ACTIVATE_PATCH, requestData);
                break;
            default: break;   // the cleanup: no data
            }
            o.mRequestData = requestData.ToByteBuffer(true);
            break;
        }
        }
    }

    // each op's CheckResponse: the status, or kInv / kCut
    string CheckResponse()
    {
        SBase& b = this->Base();
        try {
            bool inv = false;
            WONStatus result = b.Check(&inv);
            if (inv) return kInv;
            if (result != WS_Success) return Num(result);
            if (this->mKind == K_MODULE_LIST) {
                if (b.mMessageReplyType != 7) return kInv;
            } else if (this->mKind == K_INFO_EMAIL || this->mKind == K_PASSWORD_EMAIL) {
                if (!ee_won_db_answers(b.mSubMessageReplyType, this->mEmail.mMsgType)) return kInv;
            } else if (b.mSubMessageReplyType != b.mSubMessageType + 1) {
                return kInv;
            }
            switch (this->mKind) {
            case K_QUERY_MASTER:
                if (!AdminQueryMasterKeyReply(this->mKey, b.mReplyData.get())) return Num(WS_InvalidMessage);
                break;
            case K_AUTH_CHECK:
                if (!AdminStatusReply(EE_WON_ADMIN_AUTH_CHECK_KEY, b.mReplyData.get(), this->mKey.mKeyStatusList, true)) return Num(WS_InvalidMessage);
                break;
            case K_INSERT_BANNED: case K_REMOVE_BANNED:
                if (!AdminStatusReply(this->mKind == K_INSERT_BANNED ? EE_WON_ADMIN_INSERT_BANNED_KEY : EE_WON_ADMIN_REMOVE_BANNED_KEY,
                                      b.mReplyData.get(), this->mKey.mStatusList, false))
                    return Num(WS_InvalidMessage);
                break;
            case K_QUERY_BANNED:
                if (!AdminQueryBannedKeyReply(this->mQB, b.mReplyData.get())) return Num(WS_InvalidMessage);
                break;
            case K_QUERY_LIST_BANNED:
                if (!AdminQueryListBannedKeyReply(this->mQLB, b.mReplyData.get())) return Num(WS_InvalidMessage);
                break;
            case K_INSERT_SUB: case K_UPDATE_SUB: case K_REMOVE_SUB: {
                // BaseKeySubscribeOp::Unpack over a reader at the reply data's start
                ReadBuffer theData(b.mReplyData->data(), b.mReplyData->length());
                ConstSmartPtr<ByteBuffer> aRest = new ByteBuffer(theData.data() + theData.pos(), theData.Available(), false);
                if (!AdminStatusReply(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE, aRest.get(), this->mSub.mStatusList, true)) return Num(WS_InvalidMessage);
                break;
            }
            case K_QUERY_SUB:
                if (!AdminQueryKeySubscribeReply(this->mSub, b.mReplyData.get())) return Num(WS_InvalidMessage);
                break;
            case K_ACCOUNT: case K_ACCOUNT_EX: {
                long aProfile = AdminAccountReply(this->mAcct, b.mReplyData.get());
                if (aProfile >= 0) {
                    ReadBuffer theData(b.mReplyData->data(), b.mReplyData->length());
                    theData.SetPos(aProfile);
                    try {
                        this->mAcct.mProfileData->UnpackGetData(theData);
                    } catch (ReadBufferException&) {
                    }
                }
                break;
            }
            case K_GET_VERSION_LIST:
                AdminVersionListReply<SVersion>(b.mReplyData.get(), this->mVer.mVersionDataList);
                break;
            case K_MODULE_LIST:
                AdminModuleListReply(b.mReplyData.get(), this->mVer.mModuleList);
                break;
            case K_GET_PATCH_LIST:
                PatchReplyData<SPatch, std::list<SmartPtr<SPatch> >, SVersion>(b.mReplyData.get(), NULL, NULL, NULL, this->mPL.mPatchDataList);
                break;
            default: break;   // no extended unpack
            }
            return Num(WS_Success);
        } catch (ReadBufferException&) {
            return kCut;
        }
    }

    string Dump()
    {
        string s = this->Base().Dump();
        switch (this->mKind) {
        case K_QUERY_MASTER:
            for (std::list<SKey::MasterKey>::const_iterator i = this->mKey.mMasterKeyList.begin(); i != this->mKey.mMasterKeyList.end(); ++i)
                s += " m" + Num(i->mSequenceNumber) + ":" + Num((long long)i->mActivateDate) + ":" + Num((long long)i->mDeactivateDate) +
                     ":" + Num(i->mIsBeta) + ":" + Num((uint32_t)i->mCDKeyFlags);
            break;
        case K_AUTH_CHECK:
            for (std::list<short>::const_iterator i = this->mKey.mKeyStatusList.begin(); i != this->mKey.mKeyStatusList.end(); ++i) s += " k" + Num(*i);
            break;
        case K_INSERT_BANNED: case K_REMOVE_BANNED:
            for (std::list<short>::const_iterator i = this->mKey.mStatusList.begin(); i != this->mKey.mStatusList.end(); ++i) s += " s" + Num(*i);
            break;
        case K_INSERT_SUB: case K_UPDATE_SUB: case K_REMOVE_SUB:
            for (std::list<short>::const_iterator i = this->mSub.mStatusList.begin(); i != this->mSub.mStatusList.end(); ++i) s += " s" + Num(*i);
            break;
        case K_QUERY_BANNED:
            for (std::list<SQueryBanned::BANNED_KEY_PAIR>::const_iterator i = this->mQB.mKeyList.begin(); i != this->mQB.mKeyList.end(); ++i)
                s += " " + Hex(i->first) + ":" + Num((long long)i->second);
            break;
        case K_QUERY_LIST_BANNED:
            for (std::list<SQueryListBanned::BANNED_KEY_PAIR>::const_iterator i = this->mQLB.mKeyDataList.begin(); i != this->mQLB.mKeyDataList.end(); ++i)
                s += " " + Num((long long)i->first) + ":" + Num(i->second);
            break;
        case K_QUERY_SUB:
            for (std::list<SSub::KEY_DATA>::const_iterator i = this->mSub.mKeyDataList.begin(); i != this->mSub.mKeyDataList.end(); ++i)
                s += " " + Num((long)i->mStatus) + ":" + Num((long long)i->mAllocateDate) + ":" + Num((uint32_t)i->mAllocateDuration) + ":" +
                     Num((long long)i->mActivateTime) + ":" + Num((uint32_t)i->mActivateDuration) + ":" + Num((uint32_t)i->mActivateUserSeq);
            break;
        case K_ACCOUNT: case K_ACCOUNT_EX: {
            SAccount& o = this->mAcct;
            WriteBuffer w(0, true);
            o.mProfileData->PackRequestData(w, true);
            s += " " + HexW(o.mUserName) + " " + Hex(o.mEmail) + " " + HexW(o.mPassword) + " " + Hex(o.mLastLogin) + " " +
                 Hex(o.mActivated) + " p" + Hex(Bytes(w));
            break;
        }
        case K_GET_VERSION_LIST:
            for (std::list<SmartPtr<SVersion> >::const_iterator i = this->mVer.mVersionDataList.begin(); i != this->mVer.mVersionDataList.end(); ++i)
                s += " v" + (*i)->Dump();
            break;
        case K_MODULE_LIST:
            for (std::list<unsigned long>::const_iterator i = this->mVer.mModuleList.begin(); i != this->mVer.mModuleList.end(); ++i)
                s += " " + Num((uint32_t)*i);
            break;
        case K_GET_PATCH_LIST:
            for (std::list<SmartPtr<SPatch> >::const_iterator i = this->mPL.mPatchDataList.begin(); i != this->mPL.mPatchDataList.end(); ++i)
                s += " " + (*i)->Dump();
            break;
        case K_INFO_EMAIL: case K_PASSWORD_EMAIL: {
            WriteBuffer w(0, true);
            this->mEmail.mProfileData->PackRequestData(w, true);
            s += " p" + Hex(Bytes(w));
            break;
        }
        default: break;
        }
        return s;
    }
};

// ---- the replies

static void AppendRecords(int inKind, WriteBuffer& w)
{
    int n = Pick(8) ? Pick(4) : (Pick(2) ? -1 - Pick(3) : 300 + Pick(70000));
    w.AppendShort((short)n);
    if (n < 0 || n > 6) n = Pick(4);
    for (int i = 0; i < n; i++) {
        switch (inKind) {
        case K_QUERY_MASTER: w.AppendShort((short)Next()); w.AppendLong(ANum()); w.AppendLong(ANum()); w.AppendByte((BYTE)Pick(3)); w.AppendLong(ANum()); break;
        case K_QUERY_BANNED: w.AppendString(AStr(), 1); w.AppendLong(ANum()); break;
        case K_QUERY_LIST_BANNED: w.AppendLong(ANum()); w.AppendShort((short)Next()); break;
        case K_QUERY_SUB: w.AppendShort((short)(Pick(5) - 2)); for (int k = 0; k < 5; k++) w.AppendLong(ANum()); break;
        case K_MODULE_LIST: w.AppendLong(ANum()); break;
        case K_GET_PATCH_LIST: {
            WriteBuffer r(0, true);
            r.AppendString(AStr()); r.AppendString(AStr()); r.AppendString(AStr()); r.AppendBool(Pick(2) != 0);
            r.AppendString(AStr()); r.AppendString(AStr()); r.AppendLong(ANum()); r.AppendLong(ANum());
            for (int k = 0; k < 4; k++) r.AppendString(AStr());
            r.AppendBool(Pick(2) != 0);
            string rb = Bytes(r);
            if (Pick(6) == 0) rb.resize(Pick((int)rb.size() + 1));
            w.AppendShort((short)rb.size());
            w.AppendBytes(rb.data(), (unsigned long)rb.size());
            break;
        }
        case K_GET_VERSION_LIST: {
            WriteBuffer r(0, true);
            r.AppendString(AStr()); r.AppendString(AStr());
            int t = Pick(3);
            r.AppendByte((BYTE)t);
            for (int k = 0; k < t; k++) r.AppendString(AStr());
            r.AppendString(AStr()); r.AppendByte((BYTE)Pick(4));
            string rb = Bytes(r);
            if (Pick(6) == 0) rb.resize(Pick((int)rb.size() + 1));
            w.AppendShort((short)rb.size());
            w.AppendBytes(rb.data(), (unsigned long)rb.size());
            break;
        }
        default: w.AppendShort((short)(Pick(7) - 3)); break;   // the statuses
        }
    }
}

static string AReplyData(int inKind)
{
    WriteBuffer w(0, true);
    int c = Pick(10);
    if (c == 0) return string();
    if (c == 1) return FromSet("\x01\x02\xff\x00", Pick(12));
    if (inKind == K_ACCOUNT || inKind == K_ACCOUNT_EX) {
        w.AppendWString(AWStr(), 2); w.AppendString(AStr()); w.AppendWString(AWStr(), 2); w.AppendString(AStr()); w.AppendString(AStr());
        if (Pick(3)) {
            // a profile: a field count, then newsletters, a custom action, a birth date
            int n = Pick(4);
            w.AppendShort((short)n);
            for (int i = 0; i < n; i++) {
                int f = Pick(4);
                if (f == 1) { w.AppendShort(1); w.AppendShort(3); w.AppendShort((short)Pick(5)); w.AppendBool(Pick(2) != 0); }
                else if (f == 2) { int k = Pick(4); w.AppendShort(2); w.AppendShort((short)(k + 2 - Pick(2) * 3)); w.AppendShort(7); w.AppendBytes("abcd", k); }
                else if (f == 3) { w.AppendShort(3); w.AppendShort(4); w.AppendByte((BYTE)Pick(13)); w.AppendByte((BYTE)Pick(32)); w.AppendShort((short)(1900 + Pick(120))); }
                else { w.AppendShort(9); w.AppendShort(0); }
            }
        }
        return Bytes(w);
    }
    AppendRecords(inKind, w);
    return Bytes(w);
}

static string AReply(Op& op)
{
    SBase& b = op.Base();
    WriteBuffer w(0, true);
    w.AppendByte(Pick(12) ? 5 : (BYTE)Pick(7));
    w.AppendShort(Pick(12) ? 14 : (short)Pick(20));
    short m = op.mKind == K_MODULE_LIST ? (Pick(5) ? 7 : 6) : (Pick(3) ? b.mMessageType : (short)Pick(8));
    w.AppendShort(m);
    w.AppendShort(Pick(6) ? (short)(b.mSubMessageType + 1) : (short)Pick(36));
    w.AppendShort(Pick(5) ? 0 : (short)(Pick(9) - 6));
    w.AppendString(Pick(3) ? string() : AStr());
    w.AppendByte(Pick(4) ? 0x80 : (BYTE)(Pick(2) ? 0 : 0x7f));
    string d = AReplyData(op.mKind);
    w.AppendBytes(d.data(), (unsigned long)d.size());
    string r = Bytes(w);
    if (Pick(6) == 0) r.resize(Pick((int)r.size() + 1));
    else if (Pick(10) == 0 && !r.empty()) r[Pick((int)r.size())] = (char)Pick(256);
    return r;
}

// ---- the Rust op

#if ADMINOP_RUST
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
struct RustOp {
    EeWonReqOp* mOp;
    RustOp(const Op& o) { this->mOp = ee_won_adminop_new(o.mKind, o.mValues.mV.empty() ? NULL : &o.mValues.mV[0], o.mValues.mV.size()); }
    ~RustOp() { ee_won_reqop_free(this->mOp); }
    string Request()
    {
        int32_t aFinish = 0;
        ee_won_reqop_run_hook(this->mOp, 0, &aFinish);
        string r;
        long s = ee_won_reqop_next_request(this->mOp, 4, StrSink, &r);
        return Num(s) + " " + Hex(r);
    }
    string Check(const string& inReply)
    {
        int32_t v = 0;
        long s = ee_won_reqop_check_response(this->mOp, reinterpret_cast<const uint8_t*>(inReply.data()), inReply.size(), &v);
        string d;
        ee_won_reqop_describe(this->mOp, StrSink, &d);
        string st = v == EE_WON_REQOP_INVALID_HEADER ? string(kInv) : v == EE_WON_REQOP_CUT ? string(kCut) : Num(s);
        return st + " " + d;
    }
};
#endif

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    int aSequences = aPrint > 0 ? aPrint : 100000;
    long aSteps = 0;
    for (int q = 0; q < aSequences; q++) {
        Op o;
        o.Generate(kKinds[Pick(sizeof(kKinds) / sizeof(kKinds[0]))]);
        o.RunHook();
        SBase& b = o.Base();
        WONStatus aReqStatus = b.GetNextRequest();
        string aReq = Num(aReqStatus) + " " + Hex(Bytes(b.mRequest.get()));
#if ADMINOP_RUST
        RustOp r(o);
        string aRustReq = r.Request();
        CHECK(aRustReq == aReq);
        if (aRustReq != aReq && sFailures < 10) printf("OP %d%s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), aReq.c_str(), aRustReq.c_str());
#endif
        if (aPrint) printf("OP %d%s\nQ %s\n", o.mKind, o.mValues.Line().c_str(), aReq.c_str());
        for (int n = 1 + Pick(5); n > 0; n--) {
            string aReply = AReply(o);
            b.mResponse = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
            string st = o.CheckResponse();
            string aOrig = st + " " + o.Dump();
            aSteps++;
            if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if ADMINOP_RUST
            string aNew = r.Check(aReply);
            CHECK(aNew == aOrig);
            if (aNew != aOrig && sFailures < 10)
                printf("OP %d%s\nR %s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
            // the exchange moves on unless more replies are asked for (the next server's replies reach the same op)
            if (st != "100059" && Pick(3)) break;
        }
    }
    if (!aPrint) printf("won_adminop_test: %lld checks, %d failures (%ld replies of %d sequences)\n", sChecks, sFailures, aSteps, aSequences);
    return sFailures;
}
