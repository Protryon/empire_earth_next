// WONDB's ContestCallOp and EventReportOp, WONMisc's ReportPatchStatusOp and WONServer's PingOp as whole ops: the
// original's RunHook, GetNextRequest, CheckResponse and Reset (copied from the C++ op classes before the port, over
// stand-ins with the ops' members; the wire statements through include/ee/wonapi/WONDBMsg.h and WONPatchMsg.h and
// ee_won's ping functions, which won_db_test, won_misc_test and won_server_test check against the pre-port
// statements) against the Rust ops (ee_won reqops.rs) through their C ABI. Each sequence: generated members, the
// RunHook verdict and the request, then up to five generated replies (headers right and wrong, cut and mutated),
// each reply's verdict and the op's members after it; a ping op is also Reset between replies at times.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_reqops.txt, recorded
// before the port; `reqops::tests::golden` replays it). The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONDBMsg.h"
#include "ee/wonapi/WONPatchMsg.h"
#include "rust/ee_won.h"

// 0 records the golden from the original's half alone
#ifndef REQOPS_RUST
#define REQOPS_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 8181;
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
static string Utf16(const wstring& w)
{
    string b;
    WideAppendUtf16LE(b, w);
    return b;
}

// ---- the kinds (ee_won reqops.rs)

enum { K_CONTEST = 1, K_EVENT = 2, K_PATCH_STATUS = 3, K_PING = 4 };

static const char* const kInv = "inv";   // InvalidReplyHeader()
static const char* const kCut = "cut";   // a ReadBufferException

// ---- the members handed over (the OP line, and the Rust op's values)

struct Values {
    std::vector<string> mTok;     // "n<dec>" or "b<hex>"
    std::list<string> mKeep;
#if REQOPS_RUST
    std::vector<EeWonAdminValue> mV;
#endif
    void N(long long v)
    {
        this->mTok.push_back("n" + Num(v));
#if REQOPS_RUST
        EeWonAdminValue x = {static_cast<int64_t>(v), NULL, 0};
        this->mV.push_back(x);
#endif
    }
    void S(const string& s)
    {
        this->mKeep.push_back(s);
        this->mTok.push_back("b" + (s.empty() ? string() : Hex(s)));
#if REQOPS_RUST
        EeWonAdminValue x = {0, reinterpret_cast<const uint8_t*>(this->mKeep.back().data()), this->mKeep.back().size()};
        this->mV.push_back(x);
#endif
    }
    void W(const wstring& w) { this->S(Utf16(w)); }
    string Line() const
    {
        string s;
        for (size_t i = 0; i < this->mTok.size(); i++) s += " " + this->mTok[i];
        return s;
    }
};

// ---- the stand-ins: the ops' members, as the C++ classes had them

// ServerRequestOp's members the ops read
struct SBase {
    unsigned char mLengthFieldSize;
    int mAuthType;
    ByteBufferPtr mRequest, mResponse;
    SBase() : mLengthFieldSize(4), mAuthType(AUTH_TYPE_NONE) {}
};
// ContestCallOp (the certificate's user id handed in)
struct SContest : SBase {
    unsigned short mProcNum;
    ConstSmartPtr<ByteBuffer> mRequestData, mReplyData;
    unsigned long mUserId;
};
// EventReportOp
struct SEvent : SBase {
    unsigned short mActivityType;
    bool mHasRelatedServer;
    unsigned short mServerType;
    wstring mServerLogicalName;
    string mServerNetAddress;
    bool mHasRelatedClient;
    wstring mClientName;
    string mClientNetAddress;
    bool mHasRelatedUser;
    unsigned long mUserId;
    wstring mUserName;
    struct DetailStruct {
        unsigned short mDetailType;
        int mDataType;
        unsigned long mNumber;
        wstring mString;
    };
    std::list<DetailStruct> mDetailList;
    struct AttachmentStruct {
        wstring mDescription;
        unsigned char mContentType;
        ConstSmartPtr<ByteBuffer> mData;
    };
    std::list<AttachmentStruct> mAttachmentList;
};
// ReportPatchStatusOp over DBProxyOp's members
struct SReport : SBase {
    short mMessageType, mMessageReplyType, mSubMessageType, mSubMessageReplyType;
    ByteBufferPtr mRequestData, mReplyData;
    WriteBuffer mReceivedData;
    string mReplyErrorString;
    unsigned short mMsgType;
    string mProductName, mConfigName, mFromVersion, mToVersion, mNetAddress;
    BYTE mPatchStatus;
    bool mUDP;
    SReport()
        : mMessageType(0), mMessageReplyType(0), mSubMessageType(0), mSubMessageReplyType(0), mReceivedData(0, true),
          mReplyErrorString(""), mMsgType(19), mPatchStatus(0), mUDP(false)
    {
    }
};
// PingOp (the original left mLag, mVersion, mPID and mIsAuth unset until a reply or Reset; zero here, as the port)
struct SPing : SBase {
    bool mExtended;
    DWORD mLag;
    string mAppName, mLogicalName, mImage, mPorts, mRegData, mServData;
    time_t mVersion;
    long mPID;
    bool mIsAuth;
    DWORD mRequestTick, mReplyTick;   // WONTickCount() at the request and at the replies
    SPing() : mExtended(false), mLag(0), mVersion(0), mPID(0), mIsAuth(false), mRequestTick(0), mReplyTick(0) {}
};

static void AppendUtf8Sink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

struct Op {
    int mKind;
    Values mValues;
    SContest mC;
    SEvent mE;
    SReport mR;
    SPing mP;

    SBase& Base()
    {
        switch (this->mKind) {
        case K_CONTEST: return this->mC;
        case K_EVENT: return this->mE;
        case K_PATCH_STATUS: return this->mR;
        default: return this->mP;
        }
    }

    // the constructors' defaults, then generated members (each set or left)
    void Generate(int inKind)
    {
        this->mKind = inKind;
        Values& v = this->mValues;
        bool gen = Pick(6) != 0;
        switch (inKind) {
        case K_CONTEST: {
            SContest& o = this->mC;
            o.mProcNum = 0; o.mLengthFieldSize = 2; o.mUserId = 0;
            bool aData = false;
            if (gen) {
                o.mAuthType = Pick(4);
                o.mUserId = (unsigned long)ANum();
                o.mProcNum = (unsigned short)Next();
                if (Pick(4)) { string d = AStr(); o.mRequestData = new ByteBuffer(d.data(), (unsigned long)d.size(), false); aData = true; }
            }
            v.N(o.mAuthType); v.N((uint32_t)o.mUserId); v.N(o.mProcNum); v.N(aData);
            v.S(aData ? Bytes(o.mRequestData.get()) : string());
            break;
        }
        case K_EVENT: {
            SEvent& o = this->mE;
            o.mLengthFieldSize = 4; o.mHasRelatedServer = o.mHasRelatedClient = o.mHasRelatedUser = false;
            o.mActivityType = 0; o.mServerType = 0; o.mUserId = 0;
            if (gen) {
                o.mActivityType = (unsigned short)Next();
                if (Pick(2)) { o.mHasRelatedServer = true; o.mServerType = (unsigned short)Next(); o.mServerLogicalName = AWStr(); o.mServerNetAddress = AStr(); }
                if (Pick(2)) { o.mHasRelatedClient = true; o.mClientName = AWStr(); o.mClientNetAddress = AStr(); }
                if (Pick(2)) { o.mHasRelatedUser = true; o.mUserId = (unsigned long)ANum(); o.mUserName = AWStr(); }
                for (int n = Pick(4); n > 0; n--) {
                    SEvent::DetailStruct d;
                    d.mDetailType = (unsigned short)Next();
                    d.mDataType = Pick(2);
                    d.mNumber = d.mDataType == 0 ? (unsigned long)ANum() : 0;
                    if (d.mDataType == 1) d.mString = AWStr();
                    o.mDetailList.push_back(d);
                }
                for (int n = Pick(3); n > 0; n--) {
                    SEvent::AttachmentStruct a;
                    a.mDescription = AWStr();
                    a.mContentType = (unsigned char)Pick(256);
                    if (Pick(4)) { string d = AStr(); a.mData = new ByteBuffer(d.data(), (unsigned long)d.size(), false); }
                    o.mAttachmentList.push_back(a);
                }
            }
            v.N(o.mActivityType);
            v.N(o.mHasRelatedServer); v.N(o.mServerType); v.W(o.mServerLogicalName); v.S(o.mServerNetAddress);
            v.N(o.mHasRelatedClient); v.W(o.mClientName); v.S(o.mClientNetAddress);
            v.N(o.mHasRelatedUser); v.N((uint32_t)o.mUserId); v.W(o.mUserName);
            v.N((long long)o.mDetailList.size());
            for (std::list<SEvent::DetailStruct>::iterator i = o.mDetailList.begin(); i != o.mDetailList.end(); ++i) {
                v.N(i->mDetailType); v.N(i->mDataType); v.N((uint32_t)i->mNumber); v.W(i->mString);
            }
            v.N((long long)o.mAttachmentList.size());
            for (std::list<SEvent::AttachmentStruct>::iterator i = o.mAttachmentList.begin(); i != o.mAttachmentList.end(); ++i) {
                v.W(i->mDescription); v.N(i->mContentType); v.N(i->mData.get() != NULL); v.S(Bytes(i->mData.get()));
            }
            break;
        }
        case K_PATCH_STATUS: {
            SReport& o = this->mR;
            if (gen) {
                o.mProductName = AStr(); o.mConfigName = AStr(); o.mFromVersion = AStr(); o.mToVersion = AStr();
                o.mNetAddress = AStr(); o.mPatchStatus = (BYTE)Pick(256);
            }
            o.mUDP = Pick(4) == 0;
            v.S(o.mProductName); v.S(o.mConfigName); v.S(o.mFromVersion); v.S(o.mToVersion); v.S(o.mNetAddress);
            v.N(o.mPatchStatus);
            break;
        }
        default: {
            SPing& o = this->mP;
            o.mLengthFieldSize = Pick(2) ? 4 : 2;
            o.mExtended = Pick(2) != 0;
            o.mRequestTick = (DWORD)ANum();
            o.mReplyTick = Pick(3) ? o.mRequestTick + (DWORD)Pick(500) : (DWORD)ANum();
            v.N(o.mLengthFieldSize); v.N(o.mExtended); v.N((uint32_t)o.mRequestTick); v.N((uint32_t)o.mReplyTick);
            break;
        }
        }
    }

    // RunHook's statements before the base's: false when it finished the op (*outStatus)
    bool RunHook(WONStatus* outStatus)
    {
        if (this->mKind == K_CONTEST && this->mC.mAuthType == AUTH_TYPE_NONE) {
            *outStatus = WS_ServerReq_NeedAuthContext;
            return false;
        }
        if (this->mKind == K_PATCH_STATUS) {
            SReport& o = this->mR;
            o.mMessageType = 1004;
            o.mSubMessageType = o.mMsgType;
            std::vector<std::string> aStrings;
            aStrings.push_back(o.mProductName);
            aStrings.push_back(o.mConfigName);
            aStrings.push_back(o.mFromVersion);
            aStrings.push_back(o.mToVersion);
            aStrings.push_back(o.mNetAddress);
            o.mRequestData = PatchRequest(1, aStrings, o.mPatchStatus);
            // the UDP path: the request without a length field, sent as one datagram
            if (o.mUDP) o.mLengthFieldSize = 0;
        }
        return true;
    }

    WONStatus GetNextRequest()
    {
        switch (this->mKind) {
        case K_CONTEST: {
            SContest& o = this->mC;
            DbRequest q(EE_WON_DB_REQ_CONTEST, o.mLengthFieldSize);
            q.mReq.user_id = (uint32_t)o.mUserId;
            q.mReq.message = (short)o.mProcNum;
            q.mReq.data = DbRequest::Buffer(o.mRequestData.get());
            o.mRequest = q.Build();
            return WS_ServerReq_Recv;
        }
        case K_EVENT: {
            SEvent& o = this->mE;
            DbRequest q(0, o.mLengthFieldSize);
            EeWonDbEvent e;
            memset(&e, 0, sizeof(e));
            e.field_size = o.mLengthFieldSize;
            e.activity_type = o.mActivityType;
            e.has_server = o.mHasRelatedServer;
            e.server_type = o.mServerType;
            e.server_name = q.Text(o.mServerLogicalName);
            e.server_addr = DbRequest::Narrow(o.mServerNetAddress);
            e.has_client = o.mHasRelatedClient;
            e.client_name = q.Text(o.mClientName);
            e.client_addr = DbRequest::Narrow(o.mClientNetAddress);
            e.has_user = o.mHasRelatedUser;
            e.user_id = (uint32_t)o.mUserId;
            e.user_name = q.Text(o.mUserName);
            std::vector<EeWonDbDetail> aDetails;
            for (std::list<SEvent::DetailStruct>::iterator i = o.mDetailList.begin(); i != o.mDetailList.end(); ++i) {
                EeWonDbDetail d = {i->mDetailType, (int32_t)i->mDataType, (uint32_t)i->mNumber, q.Text(i->mString)};
                aDetails.push_back(d);
            }
            std::vector<EeWonDbAttachment> anAttachments;
            for (std::list<SEvent::AttachmentStruct>::iterator i = o.mAttachmentList.begin(); i != o.mAttachmentList.end(); ++i) {
                EeWonDbAttachment a = {q.Text(i->mDescription), i->mContentType, DbRequest::Buffer(i->mData.get())};
                anAttachments.push_back(a);
            }
            e.details = aDetails.empty() ? NULL : &aDetails[0];
            e.detail_count = aDetails.size();
            e.attachments = anAttachments.empty() ? NULL : &anAttachments[0];
            e.attachment_count = anAttachments.size();
            ByteBufferPtr aRequest;
            ee_won_db_event_request(&e, WonByteBufferSink, &aRequest);
            o.mRequest = aRequest;
            return WS_ServerReq_Recv;
        }
        case K_PATCH_STATUS: {
            // DBProxyOp::GetNextRequest
            SReport& o = this->mR;
            DbRequest q(EE_WON_DB_REQ_PROXY, o.mLengthFieldSize);
            q.mReq.message = o.mMessageType;
            q.mReq.sub = o.mSubMessageType;
            q.mReq.data = DbRequest::Buffer(o.mRequestData.get());
            o.mRequest = q.Build();
            return WS_ServerReq_Recv;
        }
        default: {
            SPing& o = this->mP;
            std::string aBytes;
            ee_won_server_ping_request(o.mLengthFieldSize, o.mRequestTick, o.mExtended, AppendUtf8Sink, &aBytes);
            o.mRequest = new ByteBuffer(aBytes.data(), (unsigned long)aBytes.length(), false);
            return WS_ServerReq_Recv;
        }
        }
    }

    // each op's CheckResponse: the status, or kInv / kCut
    string CheckResponse()
    {
        try {
            bool anInvalid = false;
            WONStatus aStatus = WS_Success;
            switch (this->mKind) {
            case K_CONTEST: {
                SContest& o = this->mC;
                ByteBufferPtr aData;
                aStatus = DbContestReply(o.mResponse.get(), &aData, &anInvalid);
                if (!anInvalid) o.mReplyData = aData;
                break;
            }
            case K_EVENT:
                aStatus = DbContestReply(this->mE.mResponse.get(), NULL, &anInvalid);
                break;
            case K_PATCH_STATUS: {
                SReport& o = this->mR;
                aStatus = DbProxyReply(o.mResponse.get(), o.mMessageReplyType, o.mSubMessageReplyType,
                                       o.mReplyErrorString, o.mReceivedData, o.mReplyData, &anInvalid);
                if (!anInvalid && aStatus == WS_Success && o.mSubMessageReplyType != o.mMsgType + 1) anInvalid = true;
                break;
            }
            default: {
                SPing& o = this->mP;
                const char* aData = o.mResponse->data();
                EeWonPingReply aReply;
                ee_won_server_ping_reply(reinterpret_cast<const uint8_t*>(aData), o.mResponse->length(), o.mReplyTick, &aReply);
                if (aReply.end == 1) return kInv;
                std::string* aTexts[6] = { &o.mAppName, &o.mLogicalName, &o.mImage, &o.mPorts, &o.mRegData, &o.mServData };
                static const unsigned kTextField[6] = { 2, 3, 5, 7, 9, 10 };
                if (aReply.fields >= 1) o.mLag = aReply.lag;
                for (int i = 0; i < 6; i++)
                    if (aReply.fields >= kTextField[i]) aTexts[i]->assign(aData + aReply.text_at[i], aReply.text_len[i]);
                if (aReply.fields >= 4) o.mVersion = aReply.version;
                if (aReply.fields >= 6) o.mPID = aReply.pid;
                if (aReply.fields >= 8) o.mIsAuth = aReply.is_auth;
                if (aReply.end == 2) throw ReadBufferException("Attempt to read past end of buffer.");
                break;
            }
            }
            if (anInvalid) return kInv;
            return Num(aStatus);
        } catch (ReadBufferException&) {
            return kCut;
        }
    }

    // PingOp::Reset (the others' Reset did nothing)
    void Reset()
    {
        if (this->mKind != K_PING) return;
        SPing& o = this->mP;
        o.mAppName.erase();
        o.mLogicalName.erase();
        o.mVersion = 0;
        o.mImage.erase();
        o.mPID = 0;
        o.mPorts.erase();
        o.mIsAuth = false;
        o.mRegData.erase();
        o.mServData.erase();
    }

    string Dump()
    {
        switch (this->mKind) {
        case K_CONTEST:
            return this->mC.mReplyData.get() == NULL ? string("n") : "x" + Hex(Bytes(this->mC.mReplyData.get()));
        case K_EVENT: return "-";
        case K_PATCH_STATUS: {
            SReport& o = this->mR;
            return "t" + Num(o.mMessageReplyType) + "/" + Num(o.mSubMessageReplyType) + " e" + Hex(o.mReplyErrorString) +
                   " d" + (o.mReplyData.get() == NULL ? string("n") : "x" + Hex(Bytes(o.mReplyData.get()))) + " r" +
                   Num((long long)o.mReceivedData.length());
        }
        default: {
            SPing& o = this->mP;
            return "l" + Num((uint32_t)o.mLag) + " a" + Hex(o.mAppName) + " n" + Hex(o.mLogicalName) + " i" + Hex(o.mImage) +
                   " p" + Hex(o.mPorts) + " r" + Hex(o.mRegData) + " s" + Hex(o.mServData) + " v" + Num((long long)o.mVersion) +
                   " d" + Num(o.mPID) + " u" + Num(o.mIsAuth);
        }
        }
    }
};

// ---- the replies

static string Cut(string r)
{
    if (Pick(6) == 0) r.resize(Pick((int)r.size() + 1));
    else if (Pick(10) == 0 && !r.empty()) r[Pick((int)r.size())] = (char)Pick(256);
    return r;
}

static string AReply(Op& op)
{
    WriteBuffer w(0, true);
    switch (op.mKind) {
    case K_CONTEST:
        w.AppendByte(Pick(10) ? 3 : (BYTE)Pick(6));
        w.AppendByte(Pick(10) ? 4 : (BYTE)Pick(6));
        w.AppendByte(Pick(10) ? 2 : (BYTE)Pick(6));
        w.AppendShort(Pick(3) ? 0 : (short)ANum());
        { string d = Pick(4) ? AStr() : string(); w.AppendBytes(d.data(), (unsigned long)d.size()); }
        break;
    case K_EVENT:
        w.AppendByte(Pick(10) ? 5 : (BYTE)Pick(7));
        w.AppendShort(Pick(10) ? 4 : (short)Pick(7));
        w.AppendShort(Pick(10) ? 2 : (short)Pick(7));
        w.AppendShort(Pick(3) ? 0 : (short)ANum());
        if (Pick(4) == 0) w.AppendByte((BYTE)Pick(256));
        break;
    case K_PATCH_STATUS: {
        SReport& b = op.mR;
        w.AppendByte(Pick(12) ? 5 : (BYTE)Pick(7));
        w.AppendShort(Pick(12) ? 14 : (short)Pick(20));
        w.AppendShort(Pick(3) ? b.mMessageType : (short)Pick(8));
        w.AppendShort(Pick(4) ? (short)(b.mSubMessageType + 1) : (short)Pick(36));
        w.AppendShort(Pick(5) ? 0 : (short)(Pick(9) - 6));
        w.AppendString(Pick(3) ? string() : AStr());
        w.AppendByte(Pick(4) ? 0x80 : (BYTE)(Pick(2) ? 0 : 0x7f));
        string d = Pick(3) ? AStr() : string();
        w.AppendBytes(d.data(), (unsigned long)d.size());
        break;
    }
    default: {
        SPing& p = op.mP;
        w.AppendByte(Pick(12) ? 3 : (BYTE)Pick(7));
        w.AppendByte(Pick(12) ? 1 : (BYTE)Pick(7));
        w.AppendByte(Pick(12) ? 6 : (BYTE)Pick(7));
        w.AppendLong(Pick(3) ? (int32_t)p.mRequestTick : (int32_t)ANum());
        if (Pick(3)) {
            w.AppendString(AStr(), 2); w.AppendString(AStr(), 2); w.AppendLong(ANum()); w.AppendString(AStr(), 2);
            w.AppendLong(ANum()); w.AppendString(AStr(), 2); w.AppendBool(Pick(2) != 0);
            if (Pick(2)) { w.AppendString(AStr(), 2); w.AppendString(AStr(), 2); }
        }
        break;
    }
    }
    return Cut(Bytes(w));
}

// ---- the Rust op

#if REQOPS_RUST
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
struct RustOp {
    EeWonReqOp* mOp;
    const Op& mOrig;
    // the op's own members: not the contest's auth type and user id (the first two) nor the ping's ticks (the last two)
    RustOp(const Op& o) : mOrig(o)
    {
        const std::vector<EeWonAdminValue>& v = o.mValues.mV;
        size_t at = o.mKind == K_CONTEST ? 2 : 0;
        size_t n = o.mKind == K_PING ? 2 : v.size() - at;
        this->mOp = ee_won_reqops_new(o.mKind, n == 0 ? NULL : &v[at], n);
    }
    ~RustOp() { ee_won_reqop_free(this->mOp); }
    uint32_t UserId() const { return this->mOrig.mKind == K_CONTEST ? (uint32_t)this->mOrig.mC.mUserId : 0; }
    string Request(int inAuthType, unsigned char inFieldSize)
    {
        int32_t aFinish = 0;
        if (!ee_won_reqop_run_hook(this->mOp, inAuthType, &aFinish)) return "f" + Num(aFinish);
        // the base's GetNextRequest: the user id when the op asks for it, the tick
        ee_won_reqop_set_env(this->mOp, ee_won_reqop_needs_user_id(this->mOp) ? this->UserId() : 0, this->mOrig.mP.mRequestTick);
        string r;
        long s = ee_won_reqop_next_request(this->mOp, inFieldSize, StrSink, &r);
        return Num(s) + " " + Hex(r);
    }
    string Check(const string& inReply)
    {
        ee_won_reqop_set_env(this->mOp, 0, this->mOrig.mP.mReplyTick);
        int32_t v = 0;
        long s = ee_won_reqop_check_response(this->mOp, reinterpret_cast<const uint8_t*>(inReply.data()), inReply.size(), &v);
        string st = v == EE_WON_REQOP_INVALID_HEADER ? string(kInv) : v == EE_WON_REQOP_CUT ? string(kCut) : Num(s);
        return st + " " + this->Describe();
    }
    string Describe()
    {
        string d;
        ee_won_reqop_describe(this->mOp, StrSink, &d);
        return d;
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
        o.Generate(1 + Pick(4));
        SBase& b = o.Base();
        int aAuth = b.mAuthType;
        WONStatus aFinish = WS_Success;
        string aReq;
        bool aRuns = o.RunHook(&aFinish);
        unsigned char aFieldSize = b.mLengthFieldSize;
        if (aRuns) {
            WONStatus aReqStatus = o.GetNextRequest();
            aReq = Num(aReqStatus);
            aReq += " " + Hex(Bytes(b.mRequest.get()));
        } else {
            aReq = "f" + Num(aFinish);
        }
        string aQ = aFieldSize == (o.mKind == K_PING ? o.mP.mLengthFieldSize : o.mKind == K_CONTEST ? 2 : 4)
                        ? "Q " : "Q@" + Num(aFieldSize) + " ";
#if REQOPS_RUST
        RustOp r(o);
        string aRustReq = r.Request(aAuth, aFieldSize);
        CHECK(aRustReq == aReq);
        if (aRustReq != aReq && sFailures < 10) printf("OP %d%s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), aReq.c_str(), aRustReq.c_str());
#endif
        if (aPrint) printf("OP %d%s\n%s%s\n", o.mKind, o.mValues.Line().c_str(), aQ.c_str(), aReq.c_str());
        // a finished op, or a datagram sent: no reply
        if (!aRuns || aFieldSize == 0) continue;
        for (int n = 1 + Pick(5); n > 0; n--) {
            if (o.mKind == K_PING && Pick(4) == 0) {
                o.Reset();
                string aOrig = o.Dump();
                if (aPrint) printf("X => %s\n", aOrig.c_str());
#if REQOPS_RUST
                ee_won_reqop_reset(r.mOp);
                string aNew = r.Describe();
                CHECK(aNew == aOrig);
#endif
            }
            string aReply = AReply(o);
            b.mResponse = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
            string st = o.CheckResponse();
            string aOrig = st + " " + o.Dump();
            aSteps++;
            if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if REQOPS_RUST
            string aNew = r.Check(aReply);
            CHECK(aNew == aOrig);
            if (aNew != aOrig && sFailures < 10)
                printf("OP %d%s\nR %s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
            if (st != "100059" && Pick(3)) break;
        }
    }
    if (!aPrint) printf("won_reqops_test: %lld checks, %d failures (%ld replies of %d sequences)\n", sChecks, sFailures, aSteps, aSequences);
    return sFailures;
}
