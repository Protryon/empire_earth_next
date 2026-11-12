// WONDir's GetDirOp, GetMultiDirOp, GetAliasDirOp, FindEntityOp and GetServiceOp and WONDB's GetAccountOp and
// UpdateAccountOp as whole ops: the original's GetNextRequest, CheckResponse and Reset (copied from the C++ op
// classes and GetEntityRequest before the port, over stand-ins with the ops' members; the entity replies' reading
// through DirEntityReplyParser's statements over ee_won's dir module, which won_dir_test checks against the
// pre-port parser, the requests through won_dir_msg.h and WONDBMsg.h, which won_dirops_test and won_db_test
// check) against the Rust ops (ee_won reqdir.rs) through their C ABI. Each sequence: generated members and the
// request, then up to six generated replies (headers right and wrong, statuses, sequences, cut and mutated), each
// reply's verdict and the op's results after it; a Reset at times, then the next server's request.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_reqdir.txt, recorded
// before the port; `reqdir::tests::golden` replays it). The exit code is the number of failed checks.
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
#include "won_dir_msg.h"
#include "rust/ee_won.h"

// 0 records the golden from the original's half alone
#ifndef REQDIR_RUST
#define REQDIR_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7373;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const void* inData, size_t inLength)
{
    static const char kHex[] = "0123456789abcdef";
    string r;
    const unsigned char* p = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inLength; i++) { r += kHex[p[i] >> 4]; r += kHex[p[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string Hex(const string& s) { return Hex(s.data(), s.size()); }
static string FromSet(const char* inSet, int n)
{
    string s;
    size_t k = strlen(inSet);
    for (int i = 0; i < n; i++) s += inSet[Pick((int)k)];
    return s;
}
static string AStr() { return Pick(8) ? FromSet("ab1_", Pick(5)) : FromSet("x", Pick(300)); }
static wstring AWStr()
{
    static const wchar_t* const k[] = { L"", L"/", L"/EE", L"\x263a", L"\x00e9t", L"/TitanServers/Auth" };
    wstring s = k[Pick(6)];
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

// ---- the kinds (ee_won reqdir.rs)

enum { K_DIR_GET = 7, K_DIR_MULTI = 8, K_DIR_ALIAS = 9, K_DIR_FIND = 10, K_DIR_SERVICE = 11, K_ACCOUNT_GET = 12,
       K_ACCOUNT_UPDATE = 13 };
static const int kFirstKind = 7, kKindCount = 7;

static const char* const kInv = "inv";   // InvalidReplyHeader()
static const char* const kCut = "cut";   // a ReadBufferException

// ---- the members handed over (the OP line, and the Rust op's values)

struct Values {
    std::vector<string> mTok;     // "n<dec>" or "b<hex>"
    std::list<string> mKeep;
#if REQDIR_RUST
    std::vector<EeWonAdminValue> mV;
#endif
    void N(long long v)
    {
        this->mTok.push_back("n" + Num(v));
#if REQDIR_RUST
        EeWonAdminValue x = {static_cast<int64_t>(v), NULL, 0};
        this->mV.push_back(x);
#endif
    }
    void S(const string& s)
    {
        this->mKeep.push_back(s);
        this->mTok.push_back("b" + (s.empty() ? string() : Hex(s)));
#if REQDIR_RUST
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

// ---- DirEntityReplyParser's statements (the DirEntity objects made from what ee_won's dir module read)

static void DirAssign(wstring& outText, const uint16_t* inUnits, size_t inCount)
{
    string aBytes;
    for (size_t i = 0; i < inCount; i++) {
        aBytes += (char)(inUnits[i] & 0xff);
        aBytes += (char)(inUnits[i] >> 8);
    }
    WideAssignUtf16LE(outText, aBytes.data(), (unsigned long)inCount);
}
static DirEntityPtr DirMakeEntity(const EeWonDirEntity* inEntity)
{
    DirEntityPtr anEntity = new DirEntity;
    uint32_t f = inEntity->fields;
    if (f & 0x1) anEntity->mType = (char)inEntity->kind;
    if (f & 0x2) DirAssign(anEntity->mPath, inEntity->path, inEntity->path_len);
    if (f & 0x4) DirAssign(anEntity->mName, inEntity->name, inEntity->name_len);
    if (f & 0x8) anEntity->mNetAddr = new ByteBuffer(inEntity->net_addr, (unsigned long)inEntity->net_addr_len, false);
    if (f & 0x10) anEntity->mReqUnique = inEntity->req_unique;
    if (f & 0x20) DirAssign(anEntity->mDisplayName, inEntity->display_name, inEntity->display_name_len);
    if (f & 0x40) anEntity->mLifespan = inEntity->lifespan;
    if (f & 0x80) anEntity->mCreated = inEntity->created;
    if (f & 0x100) anEntity->mTouched = inEntity->touched;
    if (f & 0x200) anEntity->mCRC = inEntity->crc;
    if (f & 0x400) { anEntity->mCreateIP = inEntity->create_ip; anEntity->mTouchIP = inEntity->touch_ip; }
    if (f & 0x800) { anEntity->mCreateId = inEntity->create_id; anEntity->mTouchId = inEntity->touch_id; }
    for (size_t i = 0; i < inEntity->object_count; i++) {
        const EeWonDirDataObject& o = inEntity->objects[i];
        DirDataObject aData;
        aData.mDataType.assign(reinterpret_cast<const char*>(o.data_type), o.data_type_len);
        if (o.has_data) aData.mData = new ByteBuffer(o.data, (unsigned long)o.data_len, false);
        anEntity->mDataObjects.push_back(aData);
    }
    for (size_t i = 0; i < inEntity->acl_count; i++) {
        const EeWonDirAcl& a = inEntity->acls[i];
        DirACL acl;
        acl.mType = (char)a.kind;
        long user = a.user, comm = a.community;
        short trust = a.trust;
        acl.mUser = user; acl.mCommunity = comm; acl.mTrust = trust;
        anEntity->mACLs.push_back(acl);
    }
    return anEntity;
}
static void TakeMulti(void* c, const EeWonDirEntity* e) { static_cast<DirEntityList*>(c)->push_back(DirMakeEntity(e)); }
static void TakeSingle(void* c, const EeWonDirEntity* e) { *static_cast<DirEntityPtr*>(c) = DirMakeEntity(e); }
static WONStatus ParseMulti(const ByteBuffer* r, DirEntityList& out)
{
    EeWonDirReply a = ee_won_dir_parse_multi(reinterpret_cast<const uint8_t*>(r->data()), r->length(), TakeMulti, &out);
    switch (a.kind) {
    case 0: return (WONStatus)a.status;
    case 1: return (WONStatus)WS_ServerReq_InvalidReplyHeader;
    case 2: return (WONStatus)a.status;
    default: throw ReadBufferException("Attempt to read past end of buffer.");
    }
}
static WONStatus ParseSingle(const ByteBuffer* r, DirEntityPtr& out)
{
    EeWonDirReply a = ee_won_dir_parse_single(reinterpret_cast<const uint8_t*>(r->data()), r->length(), TakeSingle, &out);
    switch (a.kind) {
    case 0: return WS_Success;
    case 1: return (WONStatus)WS_ServerReq_InvalidReplyHeader;
    case 2: return (WONStatus)a.status;
    default: throw ReadBufferException("Attempt to read past end of buffer.");
    }
}

// an entity as the drivers print it (won_dir_test's form)
static string Units(const wstring& inText)
{
    string s;
    char b[12];
    for (size_t i = 0; i < inText.size(); i++) { snprintf(b, sizeof(b), "%s%x", i ? "." : "", (unsigned)inText[i]); s += b; }
    return "|" + s + "|";
}
static string Entity(const DirEntity* e)
{
    char b[512];
    snprintf(b, sizeof(b), "type %d life %ld crc %lu created %ld cip %lu cid %lu touched %ld tip %lu tid %lu uniq %d",
             (int)e->mType, (long)e->mLifespan, (unsigned long)(uint32_t)e->mCRC, (long)e->mCreated,
             (unsigned long)(uint32_t)e->mCreateIP, (unsigned long)(uint32_t)e->mCreateId, (long)e->mTouched,
             (unsigned long)(uint32_t)e->mTouchIP, (unsigned long)(uint32_t)e->mTouchId, e->mReqUnique ? 1 : 0);
    string s = string(b) + " path " + Units(e->mPath) + " name " + Units(e->mName) + " display " + Units(e->mDisplayName) +
               " addr " + (e->mNetAddr.get() != NULL ? Hex(e->mNetAddr->data(), e->mNetAddr->length()) : string("null"));
    for (DirDataObjectList::const_iterator i = e->mDataObjects.begin(); i != e->mDataObjects.end(); ++i)
        s += " do " + Hex(i->mDataType.data(), i->mDataType.size()) + "=" +
             (i->mData.get() != NULL ? Hex(i->mData->data(), i->mData->length()) : string("null"));
    for (DirACLList::const_iterator i = e->mACLs.begin(); i != e->mACLs.end(); ++i) {
        snprintf(b, sizeof(b), " acl %d/%lu/%lu/%u", (int)i->mType, (unsigned long)(uint32_t)i->mUser,
                 (unsigned long)(uint32_t)i->mCommunity, (unsigned)i->mTrust);
        s += b;
    }
    return s;
}
static string List(const DirEntityList& l)
{
    string s = "[";
    for (DirEntityList::const_iterator i = l.begin(); i != l.end(); ++i) s += "{" + Entity(i->get()) + "}";
    return s + "]";
}

// ---- the stand-ins: the ops' members, as the C++ classes had them

// GetEntityRequest (the status, left unset by its constructor, starts at WS_None here, as the port)
struct SRequest {
    DWORD mFlags;
    wstring mPath;
    DirDataTypeSet mDataTypes;
    DirEntityList mDirEntityList;
    WONStatus mStatus;
    SRequest() : mFlags(0x06020837), mStatus((WONStatus)100004) {}
    void Reset() { this->mDirEntityList.clear(); }
    WONStatus ParseMultiEntityReply(const ByteBuffer* r)
    {
        this->mStatus = ParseMulti(r, this->mDirEntityList);
        return this->mStatus;
    }
    string Dump() const { return "s" + Num(this->mStatus) + List(this->mDirEntityList); }
};

struct Op {
    int mKind;
    Values mValues;
    unsigned char mLengthFieldSize;
    ByteBufferPtr mRequest, mResponse;
    // GetDirOp: one request; GetMultiDirOp / GetAliasDirOp: the list, the current one, the failures
    std::vector<SRequest> mRequests;
    size_t mCur;
    int mNumFailedRequests;
    string mProductName;
    // FindEntityOp / GetServiceOp
    bool mIsService;
    DWORD mGetFlags;
    unsigned char mMatchMode, mFindFlags;
    wstring mPath, mName, mDisplayName;
    ByteBufferPtr mNetAddr;
    DirDataObjectList mFindDataObjects;
    DirDataTypeSet mGetDataTypes;
    DirEntityPtr mService;
    DirEntityList mDirEntityList;
    // the account ops
    string mEmail, mErrorString;
    wstring mNewPassword;
    SmartPtr<ProfileData> mProfileData;

    Op() : mKind(0), mLengthFieldSize(4), mCur(0), mNumFailedRequests(0), mIsService(true), mGetFlags(0),
           mMatchMode(0), mFindFlags(0) {}

    void Types(DirDataTypeSet& outTypes)
    {
        Values& v = this->mValues;
        int n = Pick(4) ? Pick(3) : Pick(6);
        v.N(n);
        for (int i = 0; i < n; i++) {
            static const char* const k[] = { "_C", "_L", "", "\xff\x01", "type2" };
            string t = Pick(6) ? k[Pick(5)] : AStr();
            outTypes.insert(t);
            v.S(t);
        }
    }
    DWORD Flags()
    {
        static const DWORD k[] = { 0x04000622, 0x04000002, 0x0703003a, 0x06020837, 0x00000c10, 0x0000ffff };
        DWORD f = Pick(3) ? k[Pick(6)] : (DWORD)Next() << Pick(9);
        if (Pick(4) == 0) f ^= 1u << Pick(32);
        return f;
    }
    void Query()
    {
        SRequest r;
        if (Pick(5)) {
            r.mFlags = this->Flags(); r.mPath = AWStr();
            this->mValues.N(r.mFlags); this->mValues.W(r.mPath);
            this->Types(r.mDataTypes);
        }
        else { this->mValues.N(r.mFlags); this->mValues.W(r.mPath); this->mValues.N(0); this->mRequests.push_back(r); return; }
        this->mRequests.push_back(r);
    }
    void Profile()
    {
        Values& v = this->mValues;
        this->mProfileData = new ProfileData;
        int n = Pick(4);
        v.N(n);
        for (int i = 0; i < n; i++) {
            unsigned short id = (unsigned short)(Pick(3) ? Pick(4) : Next());
            bool on = Pick(2) != 0;
            this->mProfileData->AddNewsletterSub(id, on);
            v.N(id); v.N(on);
        }
        unsigned char m = 0, d = 0; unsigned short y = 0;
        if (Pick(2)) { m = (unsigned char)Pick(256); d = (unsigned char)Pick(256); y = (unsigned short)Next(); }
        this->mProfileData->SetBirthDate(m, d, y);
        v.N(m); v.N(d); v.N(y);
        unsigned short a = 0;
        bool aData = false;
        string data;
        if (Pick(3) == 0) {
            a = (unsigned short)Pick(4);
            aData = Pick(3) != 0;
            data = aData ? AStr() : string();
            ByteBufferPtr b = aData ? new ByteBuffer(data.data(), (unsigned long)data.size(), false) : NULL;
            this->mProfileData->SetCustomAction(a, b.get());
        }
        v.N(a); v.N(aData); v.S(data);
    }

    // the constructors' defaults, then generated members
    void Generate(int inKind)
    {
        this->mKind = inKind;
        Values& v = this->mValues;
        switch (inKind) {
        case K_DIR_GET:
            this->Query();
            break;
        case K_DIR_ALIAS:
            this->mProductName = Pick(3) ? AStr() : string();
            v.S(this->mProductName);
            // fall through: the queries
        case K_DIR_MULTI: {
            int n = Pick(5) ? 1 + Pick(4) : 0;
            v.N(n);
            for (int i = 0; i < n; i++) this->Query();
            break;
        }
        case K_DIR_FIND: {
            this->mIsService = Pick(2) != 0;
            this->mGetFlags = 0x06020837; // Init's
            if (Pick(3)) this->mGetFlags = this->Flags();
            this->mMatchMode = (unsigned char)(Pick(4) ? Pick(4) : Pick(256));
            this->mFindFlags = (unsigned char)(Pick(3) ? 0 : Pick(3) ? 1 << Pick(3) : Pick(256));
            this->mPath = AWStr(); this->mName = AWStr(); this->mDisplayName = Pick(2) ? AWStr() : wstring();
            bool anAddr = Pick(2) != 0;
            string addr = anAddr ? AStr() : string();
            if (anAddr) this->mNetAddr = new ByteBuffer(addr.data(), (unsigned long)addr.size(), false);
            v.N(this->mIsService); v.N(this->mGetFlags); v.N(this->mMatchMode); v.N(this->mFindFlags);
            v.W(this->mPath); v.W(this->mName); v.W(this->mDisplayName); v.N(anAddr); v.S(addr);
            int n = Pick(3);
            v.N(n);
            for (int i = 0; i < n; i++) {
                DirDataObject o;
                o.mDataType = AStr();
                bool has = Pick(3) != 0;
                string d = has ? AStr() : string();
                if (has) o.mData = new ByteBuffer(d.data(), (unsigned long)d.size(), false);
                this->mFindDataObjects.push_back(o);
                v.S(o.mDataType); v.N(has); v.S(d);
            }
            this->Types(this->mGetDataTypes);
            break;
        }
        case K_DIR_SERVICE: {
            this->mGetFlags = 0x06000822; // Init's
            if (Pick(3)) this->mGetFlags = this->Flags();
            this->mPath = AWStr(); this->mName = AWStr();
            bool anAddr = Pick(2) != 0;
            string addr = anAddr ? AStr() : string();
            if (anAddr) this->mNetAddr = new ByteBuffer(addr.data(), (unsigned long)addr.size(), false);
            v.N(this->mGetFlags); v.W(this->mPath); v.W(this->mName); v.N(anAddr); v.S(addr);
            this->Types(this->mGetDataTypes);
            break;
        }
        case K_ACCOUNT_GET:
            this->Profile();
            break;
        default:
            this->mNewPassword = AWStr(); this->mEmail = AStr();
            v.W(this->mNewPassword); v.S(this->mEmail);
            this->Profile();
            break;
        }
    }

    WONStatus GetNextRequest()
    {
        switch (this->mKind) {
        case K_DIR_GET: {
            // GetDirOp::GetNextRequest
            DirRequestFields aFields(0, true);
            aFields.mReq.flags = this->mRequests[0].mFlags;
            aFields.mReq.path = aFields.Text(this->mRequests[0].mPath);
            aFields.Types(this->mRequests[0].mDataTypes);
            this->mRequest = aFields.Build(this->mLengthFieldSize);
            return (WONStatus)WS_ServerReq_Recv;
        }
        case K_DIR_MULTI: case K_DIR_ALIAS: {
            // GetMultiDirOp::GetNextRequest / GetAliasDirOp::GetNextRequest
            DirRequestFields aFields(this->mKind == K_DIR_MULTI ? 1 : 2, true);
            if (this->mKind == K_DIR_ALIAS) aFields.mReq.product = DirRequestFields::Narrow(this->mProductName);
            for (size_t i = 0; i < this->mRequests.size(); ++i)
                aFields.AddQuery(this->mRequests[i].mFlags, this->mRequests[i].mPath, this->mRequests[i].mDataTypes);
            this->mRequest = aFields.Build(this->mLengthFieldSize);
            this->mCur = 0;
            return WS_ServerReq_Recv;
        }
        case K_DIR_FIND: {
            // FindEntityOp::GetNextRequest
            DirRequestFields aFields(4, this->mIsService);
            aFields.mReq.flags = this->mGetFlags;
            aFields.mReq.mode = (uint8_t)this->mMatchMode;
            aFields.mReq.find_flags = this->mFindFlags;
            aFields.mReq.path = aFields.Text(this->mPath);
            aFields.mReq.name = aFields.Text(this->mName);
            aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
            aFields.mReq.display_name = aFields.Text(this->mDisplayName);
            aFields.Objects(this->mFindDataObjects);
            aFields.Types(this->mGetDataTypes);
            this->mRequest = aFields.Build(this->mLengthFieldSize);
            return (WONStatus)WS_ServerReq_Recv;
        }
        case K_DIR_SERVICE: {
            // GetServiceOp::GetNextRequest
            DirRequestFields aFields(3, true);
            aFields.mReq.flags = this->mGetFlags;
            aFields.mReq.path = aFields.Text(this->mPath);
            aFields.mReq.name = aFields.Text(this->mName);
            aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
            aFields.Types(this->mGetDataTypes);
            this->mRequest = aFields.Build(this->mLengthFieldSize);
            return (WONStatus)WS_ServerReq_Recv;
        }
        case K_ACCOUNT_GET: {
            // GetAccountOp::GetNextRequest
            DbRequest q(EE_WON_DB_REQ_GET_ACCOUNT, this->mLengthFieldSize);
            q.mReq.profile = this->mProfileData->GetProfile();
            this->mRequest = q.Build();
            return WS_ServerReq_Recv;
        }
        default: {
            // UpdateAccountOp::GetNextRequest
            DbRequest q(EE_WON_DB_REQ_UPDATE_ACCOUNT, this->mLengthFieldSize);
            q.mReq.password = q.Text(this->mNewPassword);
            q.mReq.email = DbRequest::Narrow(this->mEmail);
            q.mReq.profile = this->mProfileData->GetProfile();
            this->mRequest = q.Build();
            return WS_ServerReq_Recv;
        }
        }
    }

    // each op's CheckResponse: the status, or kInv / kCut
    string CheckResponse()
    {
        try {
            WONStatus s;
            switch (this->mKind) {
            case K_DIR_GET:
                // GetDirOp::CheckResponse
                s = this->mRequests[0].ParseMultiEntityReply(this->mResponse.get());
                if (s == (WONStatus)100063) return kInv;
                return Num(s);
            case K_DIR_MULTI: case K_DIR_ALIAS: {
                // GetMultiDirOp::CheckResponse / GetAliasDirOp::CheckResponse
                if (this->mCur == this->mRequests.size()) {
                    SRequest r;
                    return Num(r.ParseMultiEntityReply(this->mResponse.get()));
                }
                this->mRequests[this->mCur].ParseMultiEntityReply(this->mResponse.get());
                int32_t aFailed = this->mNumFailedRequests;
                if (ee_won_dir_multi_step(this->mRequests[this->mCur].mStatus, &aFailed)) ++this->mCur;
                this->mNumFailedRequests = aFailed;
                if (this->mCur == this->mRequests.size())
                    return Num(ee_won_dir_multi_end(this->mNumFailedRequests, this->mRequests.size()));
                return Num(WS_ServerReq_Recv);
            }
            case K_DIR_FIND:
                // FindEntityOp::CheckResponse
                if (ee_won_dir_find_single(this->mIsService, this->mFindFlags)) {
                    s = ParseSingle(this->mResponse.get(), this->mService);
                    if (s == (WONStatus)0) this->mDirEntityList.push_back(this->mService);
                } else {
                    s = ParseMulti(this->mResponse.get(), this->mDirEntityList);
                    if (s == (WONStatus)0 && this->mIsService && !this->mDirEntityList.empty())
                        this->mService = this->mDirEntityList.front();
                }
                if (s == (WONStatus)100063) return kInv;
                return Num(s);
            case K_DIR_SERVICE:
                // GetServiceOp::CheckResponse
                s = ParseSingle(this->mResponse.get(), this->mService);
                if (s == (WONStatus)WS_ServerReq_InvalidReplyHeader) return kInv;
                return Num(s);
            case K_ACCOUNT_GET: {
                // GetAccountOp::CheckResponse
                EeWonDbReply r = DbAccountReply(EE_WON_DB_GET_ACCOUNT, this->mResponse.get(), this->mErrorString, &this->mEmail);
                if (r.verdict == EE_WON_DB_INVALID_HEADER) return kInv;
                if (r.email != 2) return Num(r.result);
                ReadBuffer aReply(this->mResponse->data(), this->mResponse->length());
                aReply.SetPos((int)r.profile_at);
                this->mProfileData->UnpackGetData(aReply);
                return Num(WS_Success);
            }
            default: {
                // UpdateAccountOp::CheckResponse
                EeWonDbReply r = DbAccountReply(EE_WON_DB_UPDATE_ACCOUNT, this->mResponse.get(), this->mErrorString);
                if (r.verdict == EE_WON_DB_INVALID_HEADER) return kInv;
                return Num(r.result);
            }
            }
        } catch (ReadBufferException&) {
            return kCut;
        }
    }

    void Reset()
    {
        switch (this->mKind) {
        case K_DIR_GET: case K_DIR_MULTI: case K_DIR_ALIAS:
            for (size_t i = 0; i < this->mRequests.size(); ++i) this->mRequests[i].Reset();
            break;
        case K_DIR_FIND:
            this->mService = NULL;
            this->mDirEntityList.clear();
            break;
        case K_DIR_SERVICE:
            this->mService = NULL;
            break;
        default:
            break;
        }
    }

    string Dump()
    {
        switch (this->mKind) {
        case K_DIR_GET: return this->mRequests[0].Dump();
        case K_DIR_MULTI: case K_DIR_ALIAS: {
            string s = "c" + Num((long long)this->mCur) + " f" + Num(this->mNumFailedRequests);
            for (size_t i = 0; i < this->mRequests.size(); ++i) s += " " + this->mRequests[i].Dump();
            return s;
        }
        case K_DIR_FIND:
            return "v" + (this->mService.get() ? "{" + Entity(this->mService.get()) + "}" : string("n")) + " l" +
                   List(this->mDirEntityList);
        case K_DIR_SERVICE:
            return "v" + (this->mService.get() ? "{" + Entity(this->mService.get()) + "}" : string("n"));
        default: {
            WriteBuffer w(0, true);
            this->mProfileData->PackRequestData(w, true);
            string s = "x" + Hex(this->mErrorString);
            if (this->mKind == K_ACCOUNT_GET) s += " e" + Hex(this->mEmail);
            return s + " p" + Hex(Bytes(w));
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

// an entity's fields as a reply of these flags carries them (won_dir_test's Fields, the values generated)
static void Fields(WriteBuffer& w, unsigned long inFlags, char inType, bool inMulti)
{
    if (inFlags & 0x10) w.AppendByte(inType);
    bool aService = inType == 'S' || (inFlags & 5) == 0 || !inMulti;
    bool aDir = inType == 'D' || (inFlags & 2) == 0;
    if (aService) {
        if (inFlags & 0x01000000) w.AppendWString(AWStr(), 2);
        if (inFlags & 0x02000000) w.AppendWString(AWStr(), 2);
        if (inFlags & 0x04000000) {
            string a = Pick(3) ? string("\x01\x02\x03\x04\x05\x06", 6) : AStr();
            w.AppendByte((unsigned char)a.size());
            w.AppendBytes(a.data(), (unsigned long)a.size());
        }
    } else if (aDir && inMulti) {
        if (inFlags & 0x00010000) w.AppendWString(AWStr(), 2);
        if (inFlags & 0x00020000) w.AppendWString(AWStr(), 2);
        if (inFlags & 0x00040000) w.AppendByte((unsigned char)Pick(3));
    }
    if (inFlags & 0x20) w.AppendWString(AWStr(), 2);
    if (inFlags & 0x100) w.AppendLong(ANum());
    if (inFlags & 0x40) w.AppendLong(ANum());
    if (inFlags & 0x80) w.AppendLong(ANum());
    if (inFlags & 0x2000) w.AppendLong(ANum());
    if (inMulti && (inFlags & 0x8000)) { w.AppendLong(ANum()); w.AppendLong(ANum()); }
    if (inMulti && (inFlags & 0x4000)) { w.AppendLong(ANum()); w.AppendLong(ANum()); }
    if (inFlags & 0x800) {
        int n = Pick(3);
        w.AppendShort((short)n);
        for (int i = 0; i < n; i++) {
            if (inFlags & 0x200) w.AppendString(Pick(2) ? string("_C") : AStr(), 1);
            if (inFlags & 0x400) { string d = AStr(); w.AppendShort((short)d.size()); w.AppendBytes(d.data(), (unsigned long)d.size()); }
        }
    }
    if (inFlags & 0x1000) {
        int n = Pick(3);
        w.AppendShort((short)n);
        for (int a = 0; a < n; a++) {
            w.AppendByte((unsigned char)Pick(256));
            int u = Pick(3);
            w.AppendShort((short)u);
            for (int k = 0; k < u; k++) { w.AppendLong(ANum()); w.AppendLong(ANum()); w.AppendShort((short)ANum()); }
        }
    }
}

static unsigned long AReplyFlags()
{
    static const unsigned long k[] = { 0x04000622, 0x04000002, 0x0703003a, 0x07073ff7, 0x00000c10, 0x0000ffff, 0x02ad0830 };
    unsigned long f = Pick(4) ? k[Pick(7)] : (unsigned long)Next() << Pick(9);
    return f;
}

static string AReply(Op& op)
{
    WriteBuffer w(0, true);
    if (op.mKind == K_ACCOUNT_GET || op.mKind == K_ACCOUNT_UPDATE) {
        w.AppendByte(Pick(12) ? 5 : (BYTE)Pick(7));
        w.AppendShort(Pick(12) ? 14 : (short)Pick(20));
        w.AppendShort(Pick(8) ? 1002 : (short)Pick(8));
        short sub = op.mKind == K_ACCOUNT_GET ? 7 : 5;
        w.AppendShort(Pick(6) ? sub : (short)Pick(9));
        w.AppendShort(Pick(4) ? 0 : (short)(Pick(9) - 6));
        w.AppendString(Pick(3) ? string() : AStr());
        w.AppendByte(Pick(4) ? 0x80 : (BYTE)Pick(256));
        if (op.mKind == K_ACCOUNT_GET && Pick(5)) {
            w.AppendString(AStr(), 1);
            int n = Pick(5) ? Pick(4) : Pick(70000);
            w.AppendShort((short)n);
            if (n > 5) n = Pick(5);
            for (int i = 0; i < n; i++) {
                int field = Pick(8) ? 1 + Pick(3) : Pick(9);
                w.AppendShort((short)field);
                switch (field) {
                case 1: w.AppendShort(3); w.AppendShort((short)(Pick(3) ? Pick(4) : Next())); w.AppendBool(Pick(2) != 0); break;
                case 2: { string d = AStr(); int len = Pick(6) ? (int)d.size() + 2 : Pick(4); w.AppendShort((short)len); w.AppendShort((short)Pick(5)); w.AppendBytes(d.data(), (unsigned long)d.size()); break; }
                case 3: w.AppendShort(4); w.AppendByte((BYTE)Pick(13)); w.AppendByte((BYTE)Pick(32)); w.AppendShort((short)(1900 + Pick(150))); break;
                default: w.AppendShort((short)Pick(4)); break;
                }
            }
        } else if (Pick(3) == 0) {
            string d = AStr();
            w.AppendBytes(d.data(), (unsigned long)d.size());
        }
        return Cut(Bytes(w));
    }
    bool aSingle = op.mKind == K_DIR_SERVICE ||
                   (op.mKind == K_DIR_FIND && ee_won_dir_find_single(op.mIsService, op.mFindFlags));
    if (Pick(12) == 0) aSingle = !aSingle;
    w.AppendByte(Pick(14) ? 5 : (BYTE)Pick(7));
    w.AppendShort(Pick(14) ? 2 : (short)Pick(4));
    w.AppendShort(Pick(14) ? (aSingle ? 2 : 3) : (short)Pick(5));
    static const short kStatus[] = { 0, 0, 0, 0, 0, -1, -5, -2600, 7 };
    w.AppendShort(kStatus[Pick(9)]);
    unsigned long f = AReplyFlags();
    if (aSingle) {
        w.AppendLong((int32_t)f);
        Fields(w, f, 'S', false);
    } else {
        w.AppendByte((BYTE)(Pick(3) ? 0x80 | Pick(4) : Pick(4)));
        w.AppendLong((int32_t)f);
        int n = Pick(5) ? Pick(4) : 0;
        w.AppendShort((short)n);
        for (int i = 0; i < n; i++) Fields(w, f, "SDX"[Pick(3)], true);
    }
    return Cut(Bytes(w));
}

// ---- the Rust op

#if REQDIR_RUST
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
struct RustOp {
    EeWonReqOp* mOp;
    RustOp(const Op& o)
    {
        const std::vector<EeWonAdminValue>& v = o.mValues.mV;
        this->mOp = ee_won_reqops_new(o.mKind, v.empty() ? NULL : &v[0], v.size());
    }
    ~RustOp() { ee_won_reqop_free(this->mOp); }
    string Request(unsigned char inFieldSize)
    {
        string r;
        long s = ee_won_reqop_next_request(this->mOp, inFieldSize, StrSink, &r);
        return Num(s) + " " + Hex(r);
    }
    string Check(const string& inReply)
    {
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
    int aSequences = aPrint > 0 ? aPrint : 40000;
    long aSteps = 0;
    for (int q = 0; q < aSequences; q++) {
        Op o;
        o.Generate(kFirstKind + Pick(kKindCount));
#if REQDIR_RUST
        RustOp r(o);
#endif
        if (aPrint) printf("OP %d%s\n", o.mKind, o.mValues.Line().c_str());
        for (int aServer = 0; aServer < 2; aServer++) {
            WONStatus aReqStatus = o.GetNextRequest();
            string aReq = Num(aReqStatus) + " " + Hex(Bytes(o.mRequest.get()));
            if (aPrint) printf("Q %s\n", aReq.c_str());
#if REQDIR_RUST
            string aRustReq = r.Request(o.mLengthFieldSize);
            CHECK(aRustReq == aReq);
            if (aRustReq != aReq && sFailures < 10) printf("OP %d%s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), aReq.c_str(), aRustReq.c_str());
#endif
            string st;
            for (int n = 1 + Pick(6); n > 0; n--) {
                string aReply = AReply(o);
                o.mResponse = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
                st = o.CheckResponse();
                string aOrig = st + " " + o.Dump();
                aSteps++;
                if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if REQDIR_RUST
                string aNew = r.Check(aReply);
                CHECK(aNew == aOrig);
                if (aNew != aOrig && sFailures < 10)
                    printf("OP %d%s\nR %s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
                if (st != "100059" && Pick(3)) break;
            }
            // the next server: the base's Reset, then its request again
            if (Pick(3)) break;
            o.Reset();
            string aOrig = o.Dump();
            if (aPrint) printf("X => %s\n", aOrig.c_str());
#if REQDIR_RUST
            ee_won_reqop_reset(r.mOp);
            string aNew = r.Describe();
            CHECK(aNew == aOrig);
#endif
        }
    }
    if (!aPrint) printf("won_reqdir_test: %lld checks, %d failures (%ld replies of %d sequences)\n", sChecks, sFailures, aSteps, aSequences);
    return sFailures;
}
