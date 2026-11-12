// WONDB's CreateAccountOp and WONRouting's RoutingGetGroupListReqOp as whole ops: the original's Init, Reset,
// GetNextRequest and CheckResponse (copied from CreateAccountOp.cpp and RoutingGetGroupListReqOp.cpp before the
// port, over stand-ins with the ops' members: the real ElGamal, Blowfish and ProfileData classes, the routing
// reply's records through WONRoutingMsg.h) against the Rust ops (ee_won reqlast.rs) through their C ABI. The
// random generator's system seed is fixed (ee_won_random_test_seed) and set again before each op is made, so the
// session key and the key encryption are the same bytes on both sides. Each sequence: generated members and the
// request, then up to six generated replies (headers right and wrong, statuses, a good and bad server keys, group
// lists, cut and mutated), each reply's verdict and the op's members after it; a Reset at times, then the next
// server's request. The group list's map is the original's inner op's against the Rust op's replies applied again
// through RouteGroupListOf (what RoutingLogic::QueryServerGroupsDone reads).
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_reqlast.txt, recorded
// before the port; `reqlast::tests::golden` replays it). The exit code is the number of failed checks.
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
#include "won_profiledata.h"
#include "ee/wonapi/WONElGamal.h"
#include "won_db_msg.h"
#include "ee/wonapi/WONRoutingMsg.h"
#include "rust/ee_won.h"

// 0 records the golden from the original's half alone
#ifndef REQLAST_RUST
#define REQLAST_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4242;
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
static string Unhex(const char* inHex)
{
    string r;
    for (size_t i = 0; inHex[i] && inHex[i + 1]; i += 2) {
        char b[3] = { inHex[i], inHex[i + 1], 0 };
        r += (char)strtol(b, NULL, 16);
    }
    return r;
}
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
static string FromSet(const char* inSet, int n)
{
    string s;
    size_t k = strlen(inSet);
    for (int i = 0; i < n; i++) s += inSet[Pick((int)k)];
    return s;
}
static string AStr() { return Pick(30) ? FromSet("ab1_@.", Pick(9)) : FromSet("x", Pick(300)); }
static wstring AWStr()
{
    static const wchar_t* const k[] = { L"", L"a", L"Player", L"\x263a", L"\x00e9t\x00e9", L"pw1" };
    wstring s = k[Pick(6)];
    if (Pick(30) == 0) s += wstring(Pick(200), L'w');
    return s;
}

// the server key (won_crypto_test's 512-bit test key, public part)
static const char* const kPubKey =
    "3081df024100c37d6f28fb6c0e9aa489f1d80d5a41952bb7d31ebeb9ca0031dcf9a0c134e9395d5e21986c08c9491a49c334"
    "a82d7fff2603de67ab87a6801de67f3c3dec7211021500eeac675c1162f5b42138f37567274df60cbb1485024100abbb86ba"
    "89355c5c884dfc2f9c1e6692390d87ef1220f9eb98b763cc3631c652f83640f980949efa8b80256910f55e629ce126227b13"
    "b856bad2720bd64c60360240228d68730c33fa487c1f0c0154e0dec1f064072e366b35b75439c4f08e3a0fe4fd52a3d18348"
    "0fc30601fe4ed2739518140e9196a72f1bce2e908e04c732843c";

enum { K_CREATE = 14, K_GROUPS = 15 };
static const char* const kInv = "inv";   // InvalidReplyHeader()
static const char* const kCut = "cut";   // a ReadBufferException

// ---- the members handed over (the OP line, and the Rust op's values)

struct Values {
    std::vector<string> mTok;     // "n<dec>" or "b<hex>"
    std::list<string> mKeep;
#if REQLAST_RUST
    std::vector<EeWonAdminValue> mV;
#endif
    void N(long long v)
    {
        this->mTok.push_back("n" + Num(v));
#if REQLAST_RUST
        EeWonAdminValue x = {static_cast<int64_t>(v), NULL, 0};
        this->mV.push_back(x);
#endif
    }
    void S(const string& s)
    {
        this->mKeep.push_back(s);
        this->mTok.push_back("b" + (s.empty() ? string() : Hex(s)));
#if REQLAST_RUST
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

// ---- the group list's inner op (RoutingGetGroupListOp without a connection: its factories' plain records)

struct Inner {
    Inner() : mFlags(0) {}
    unsigned short mFlags;
    RoutingGroupMap mGroupMap;
    RoutingClientInfoPtr GetNewClientInfo() { return new RoutingClientInfo; }
    RoutingMemberInfoPtr GetNewMemberInfo() { return new RoutingMemberInfo; }
    RoutingGroupInfoPtr GetNewGroupInfo() { return new RoutingGroupInfo; }
};
static string GroupMap(const RoutingGroupMap& inMap)
{
    string s = "m" + Num((long long)inMap.size());
    for (RoutingGroupMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i) {
        const RoutingGroupInfo* g = i->second.get();
        s += " {" + Num(i->first) + ":" + Num(g->mId) + "," + Num(g->mParentId) + "," + Num(g->mCaptainId) + "," +
             Num(g->mMaxClients) + "," + Num((long long)g->mFlags) + "," + Num((long long)g->mAsyncFlags) + "," +
             Num(g->mObserverCount) + "," + Num(g->mMaxPlayers) + "," + Hex(Utf16(g->mName));
        for (RoutingMemberMap::const_iterator m = g->mMembers.begin(); m != g->mMembers.end(); ++m) {
            const RoutingMemberInfo* r = m->second.get();
            s += " " + Num(m->first) + "/" + Num(r->mClientId) + "/" + Num(r->mFlags);
            if (r->mClientInfo.get() != NULL) s += "/c";
        }
        s += "}";
    }
    return s;
}

// ---- the original's statements, over stand-ins with the ops' members

struct Op {
    int mKind;
    Values mValues;
    unsigned char mLengthFieldSize;
    ByteBufferPtr mRequest;
    ByteBufferPtr mResponse;
    WONStatus mLastServerError;
    bool mServerErrorSet;
    // CreateAccountOp
    SmartPtr<ProfileData> mProfileData;
    std::string mErrorString;
    ElGamal* mPubKey;     // made where the class's member was (before the session key)
    Blowfish mSymKey;
    enum State { State_GettingPubKey, State_SettingSessionKey, State_CreatingAccount } mState;
    wstring mUserName;
    wstring mPassword;
    string mEmail;
    // RoutingGetGroupListReqOp
    Inner* mOp;
    int mReplies;

    Op() : mKind(0), mLengthFieldSize(4), mLastServerError(WS_None), mServerErrorSet(false), mInvalid(false), mPubKey(NULL),
           mState(State_GettingPubKey), mOp(NULL), mReplies(0) {}
    ~Op() { delete this->mPubKey; delete this->mOp; }

    void SetLastServerError(WONStatus inError) { this->mLastServerError = inError; this->mServerErrorSet = true; }
    bool mInvalid;
    WONStatus InvalidReplyHeader() { this->mInvalid = true; return WS_ServerReq_TryNextServer; }   // printed as kInv

    // the members, generated, then the constructor
    void Generate(int inKind, long long inKeySeed)
    {
        this->mKind = inKind;
        if (inKind == K_CREATE) {
            this->mUserName = AWStr();
            this->mPassword = AWStr();
            this->mEmail = AStr();
            this->mValues.W(this->mUserName);
            this->mValues.W(this->mPassword);
            this->mValues.S(this->mEmail);
            // the lobby's profile (newsletter 1, the birth date), sometimes more newsletters and a custom action
            SmartPtr<ProfileData> profile = new ProfileData;
            std::map<unsigned short, bool> aSubs;
            int n = Pick(4) ? 1 : Pick(4);
            for (int i = 0; i < n; i++) {
                unsigned short id = (unsigned short)(i == 0 ? 1 : Pick(5) ? Pick(9) : Next());
                bool on = Pick(2) != 0;
                profile->AddNewsletterSub(id, on);
                aSubs[id] = on;
            }
            this->mValues.N((long long)aSubs.size());
            for (std::map<unsigned short, bool>::const_iterator i = aSubs.begin(); i != aSubs.end(); ++i) {
                this->mValues.N(i->first);
                this->mValues.N(i->second ? 1 : 0);
            }
            unsigned char m = (unsigned char)(Pick(8) ? 1 + Pick(12) : Pick(256));
            unsigned char d = (unsigned char)(Pick(8) ? 1 + Pick(31) : Pick(256));
            unsigned short y = (unsigned short)(Pick(8) ? 1900 + Pick(110) : Next());
            profile->SetBirthDate(m, d, y);
            this->mValues.N(m);
            this->mValues.N(d);
            this->mValues.N(y);
            unsigned short anAction = 0;
            bool aHasData = false;
            string aData;
            if (Pick(6) == 0) {
                anAction = (unsigned short)Pick(4);
                aHasData = Pick(2) != 0;
                aData = AStr();
                ByteBufferPtr b = aHasData ? new ByteBuffer(aData.data(), (unsigned long)aData.size(), false) : NULL;
                profile->SetCustomAction(anAction, b.get());
            }
            this->mValues.N(anAction);
            this->mValues.N(aHasData ? 1 : 0);
            this->mValues.S(aHasData ? aData : string());
            // CreateAccountOp::CreateAccountOp: the members (the ElGamal first), then Init
            ee_won_random_test_seed(inKeySeed);
            this->mPubKey = new ElGamal;
            this->mProfileData = profile;
            this->Init();
        } else {
            unsigned char aFlags = (unsigned char)(Pick(3) ? 0x29 : Pick(256));
            this->mValues.N(aFlags);
            // RoutingGetGroupListReqOp::RoutingGetGroupListReqOp (the lobby then sets mOp->mFlags)
            this->mOp = new Inner;
            this->mLengthFieldSize = 2;
            this->mOp->mFlags = aFlags;
        }
    }

    // ---- CreateAccountOp.cpp
    void Init()
    {
        this->mLengthFieldSize=4;
        this->mSymKey.Create(8);
    }
    void Reset()
    {
        if (this->mKind == K_CREATE) this->mState=State_GettingPubKey;
        // RoutingGetGroupListReqOp: ServerRequestOp's empty Reset
    }
    WONStatus GetNextRequest()
    {
        if (this->mKind == K_GROUPS) {
            // RoutingGetGroupListReqOp::GetNextRequest
            this->mRequest=RouteGroupListRequest(this->mLengthFieldSize,this->mOp->mFlags);
            return WS_ServerReq_Recv;
        }
        if (this->mState==State_GettingPubKey) {
            DbRequest q(EE_WON_DB_REQ_CREATE_KEY,this->mLengthFieldSize);
            this->mRequest=q.Build();
            return WS_ServerReq_Recv;
        } else if (this->mState==State_SettingSessionKey) {
            ByteBufferPtr encryptedKey=this->mPubKey->Encrypt(this->mSymKey.GetKey(),this->mSymKey.GetKeyLen());
            DbRequest q(EE_WON_DB_REQ_CREATE_SESSION,this->mLengthFieldSize);
            q.mReq.data=DbRequest::Buffer(encryptedKey.get());
            this->mRequest=q.Build();
            this->mState=State_CreatingAccount;
            return WS_ServerReq_Send;
        } else {
            DbRequest account(EE_WON_DB_REQ_CREATE_DATA,0);
            account.mReq.name=account.Text(this->mUserName);
            account.mReq.password=account.Text(this->mPassword);
            account.mReq.email=DbRequest::Narrow(this->mEmail);
            account.mReq.profile=this->mProfileData->GetProfile();
            ByteBufferPtr aPlain=account.Build();
            ByteBufferPtr encrypted=this->mSymKey.Encrypt(aPlain->data(),aPlain->length());
            DbRequest q(EE_WON_DB_REQ_CREATE_ACCOUNT,this->mLengthFieldSize);
            q.mReq.data=DbRequest::Buffer(encrypted.get());
            this->mRequest=q.Build();
            return WS_ServerReq_Recv;
        }
    }
    WONStatus CheckResponse()
    {
        if (this->mKind == K_GROUPS) {
            // RoutingGetGroupListReqOp::CheckResponse
            this->mReplies++;
            bool anInvalidHeader=false;
            WONStatus aStatus=RouteGroupListReqReply(this->mOp,this->mResponse.get(),&anInvalidHeader);
            if(anInvalidHeader)
                return this->InvalidReplyHeader();
            return aStatus;
        }
        EeWonDbReply r=DbAccountReply(this->mState==State_GettingPubKey ? EE_WON_DB_CREATE_KEY : EE_WON_DB_CREATE,
                                      this->mResponse.get(),this->mErrorString);
        if (r.verdict==EE_WON_DB_INVALID_HEADER) return this->InvalidReplyHeader();
        if (r.verdict==EE_WON_DB_PUBLIC_KEY) {
            if (!this->mPubKey->SetPublicKey(this->mResponse->data()+r.key_at,r.key_len)) {
                this->SetLastServerError(WS_InvalidPublicKey);
                return WS_ServerReq_TryNextServer;
            }
            this->mState=State_SettingSessionKey;
            return WS_ServerReq_Send;
        }
        return (WONStatus)r.result;
    }

    // the verdict as the drivers print it
    string Check()
    {
        this->mServerErrorSet = false;
        this->mInvalid = false;
        try {
            WONStatus s = this->CheckResponse();
            if (this->mInvalid) return kInv;
            if (this->mServerErrorSet && s == WS_ServerReq_TryNextServer) return "err" + Num(this->mLastServerError);
            return Num(s);
        } catch (ReadBufferException&) {
            return kCut;
        }
    }
    string Dump()
    {
        if (this->mKind == K_GROUPS)
            return "f" + Num(this->mOp->mFlags) + " r" + Num(this->mReplies) + " " + GroupMap(this->mOp->mGroupMap);
        ByteBufferPtr k = this->mPubKey->GetRawPublicKey();
        return "s" + Num(this->mState) + " x" + Hex(this->mErrorString) + " k" + Hex(Bytes(k.get())) + " b" +
               Hex(this->mSymKey.GetKey(), (size_t)this->mSymKey.GetKeyLen());
    }
};

// ---- the replies

static string Cut(string r)
{
    if (Pick(6) == 0) r.resize(Pick((int)r.size() + 1));
    else if (Pick(10) == 0 && !r.empty()) r[Pick((int)r.size())] = (char)Pick(256);
    return r;
}

static void GroupListBody(WriteBuffer& w)
{
    int flags = Pick(3) ? (Pick(2) ? 0x29 : Pick(0x100)) : (int)Next();
    w.AppendShort((short)flags);
    int n = Pick(4);
    w.AppendShort((short)n);
    for (int i = 0; i < n; i++) {
        WriteBuffer e(0, true);
        e.AppendShort((short)Pick(6));
        if (flags & 1) e.AppendWString(AWStr(), 2);
        if (flags & 2) e.AppendShort((short)Pick(6));
        if (flags & 4) e.AppendShort((short)Pick(300));
        if (flags & 8) e.AppendLong((int)(Pick(2) ? Pick(16) : Next()));
        if (flags & 0x10) e.AppendLong((int)Next());
        if (flags & 0x40) e.AppendShort((short)Pick(9));
        int members = Pick(4);
        if (flags & 0x20) e.AppendShort((short)members);
        else members = 0;
        if (flags & 0x80)
            for (int j = 0; j < members; j++) { e.AppendShort((short)Pick(6)); e.AppendByte((BYTE)Pick(256)); }
        string body = Bytes(e);
        int extra = Pick(4) ? Pick(3) : -Pick(3);
        if ((int)body.size() + extra < 0) extra = 0;
        w.AppendShort((short)(body.size() + extra));
        w.AppendBytes(body.data(), (unsigned long)body.size());
        for (int k = 0; k < extra; k++) w.AppendByte((BYTE)Pick(256));
    }
}

static string AReply(Op& op)
{
    WriteBuffer w(0, true);
    if (op.mKind == K_GROUPS) {
        if (Pick(10)) { w.AppendByte(3); w.AppendByte(7); w.AppendByte(25); }
        else { w.AppendByte((BYTE)Pick(4)); w.AppendByte((BYTE)(7 + Pick(2))); w.AppendByte((BYTE)(24 + Pick(2))); }
        w.AppendShort((short)(Pick(3) ? 0 : (Pick(2) ? -2541 : Pick(0x10000))));
        GroupListBody(w);
        return Cut(Bytes(w));
    }
    bool aKey = op.mState == Op::State_GettingPubKey;
    if (Pick(10) == 0) aKey = !aKey;
    w.AppendByte(Pick(12) ? 5 : (BYTE)Pick(7));
    w.AppendShort(Pick(12) ? 14 : (short)Pick(20));
    w.AppendShort(Pick(8) ? (aKey ? 3 : 1002) : (short)Pick(8));
    w.AppendShort(Pick(6) ? (aKey ? 1 : 3) : (short)Pick(9));
    w.AppendShort(Pick(4) ? 0 : (short)(Pick(9) - 6));
    w.AppendString(Pick(3) ? string() : AStr());
    w.AppendByte(Pick(4) ? 0x80 : (BYTE)Pick(256));
    if (aKey) {
        string k = Unhex(kPubKey);
        if (Pick(5) == 0) k = AStr();
        else if (Pick(8) == 0) k[Pick((int)k.size())] = (char)Pick(256);
        w.AppendShort((short)k.size());
        w.AppendBytes(k.data(), (unsigned long)k.size());
    } else if (Pick(3) == 0) {
        string d = AStr();
        w.AppendBytes(d.data(), (unsigned long)d.size());
    }
    return Cut(Bytes(w));
}

// ---- the Rust op

#if REQLAST_RUST
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
struct RustOp {
    EeWonReqOp* mOp;
    int mKind;
    RustOp(const Op& o, long long inKeySeed) : mKind(o.mKind)
    {
        const std::vector<EeWonAdminValue>& v = o.mValues.mV;
        ee_won_random_test_seed(inKeySeed);
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
        string st = v == EE_WON_REQOP_INVALID_HEADER ? string(kInv) : v == EE_WON_REQOP_CUT ? string(kCut)
                    : v == EE_WON_REQOP_SERVER_ERROR ? "err" + Num(s) : Num(s);
        return st + " " + this->Describe();
    }
    // the accessors RouteGroupListOf reads (RustServerOp's GetNum and GetBytes)
    long long GetNum(int inWhich, long long = 0) const { return ee_won_reqop_num(this->mOp, inWhich); }
    string GetBytes(int inWhich, long long inIndex = 0) const
    {
        string b;
        ee_won_reqop_bytes_at(this->mOp, inWhich, inIndex, StrSink, &b);
        return b;
    }
    string Describe()
    {
        string d;
        ee_won_reqop_describe(this->mOp, StrSink, &d);
        if (this->mKind == K_GROUPS) {
            Inner aMap;
            RouteGroupListOf(&aMap, *this);
            d += " " + GroupMap(aMap.mGroupMap);
        }
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
    int aSequences = aPrint > 0 ? aPrint : 20000;
    long aSteps = 0;
    for (int q = 0; q < aSequences; q++) {
        long long aKeySeed = 1000 + q;
        Op o;
        o.Generate(Pick(3) ? K_CREATE : K_GROUPS, aKeySeed);
#if REQLAST_RUST
        RustOp r(o, aKeySeed);
#endif
        if (aPrint) printf("OP %d%s\n", o.mKind, o.mValues.Line().c_str());
        for (int aServer = 0; aServer < 2; aServer++) {
            bool aNeedRequest = true;
            for (int n = 1 + Pick(6); n > 0;) {
                if (aNeedRequest) {
                    // the base asks for a request first, after a reply's Send and after a request's Send
                    WONStatus aReqStatus = o.GetNextRequest();
                    string aReq = Num(aReqStatus) + " " + Hex(Bytes(o.mRequest.get()));
                    if (aPrint) printf("Q %s\n", aReq.c_str());
#if REQLAST_RUST
                    string aRustReq = r.Request(o.mLengthFieldSize);
                    CHECK(aRustReq == aReq);
                    if (aRustReq != aReq && sFailures < 10) printf("OP %d%s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), aReq.c_str(), aRustReq.c_str());
#endif
                    aNeedRequest = aReqStatus == WS_ServerReq_Send;
                    if (aNeedRequest) continue;
                }
                n--;
                string aReply = AReply(o);
                o.mResponse = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
                string st = o.Check();
                string aOrig = st + " " + o.Dump();
                aSteps++;
                if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if REQLAST_RUST
                string aNew = r.Check(aReply);
                CHECK(aNew == aOrig);
                if (aNew != aOrig && sFailures < 10)
                    printf("OP %d%s\nR %s\n  orig %s\n  new  %s\n", o.mKind, o.mValues.Line().c_str(), Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
                if (st == "100058") aNeedRequest = true;
                else if (st != "100059" && Pick(3)) break;
            }
            // the next server: the base's Reset, then its request again
            if (Pick(3)) break;
            o.Reset();
            string aOrig = o.Dump();
            if (aPrint) printf("X => %s\n", aOrig.c_str());
#if REQLAST_RUST
            ee_won_reqop_reset(r.mOp);
            string aNew = r.Describe();
            CHECK(aNew == aOrig);
#endif
        }
    }
    ee_won_random_test_seed(0);
    if (!aPrint) printf("won_reqlast_test: %lld checks, %d failures (%ld replies of %d sequences)\n", sChecks, sFailures, aSteps, aSequences);
    return sFailures;
}
