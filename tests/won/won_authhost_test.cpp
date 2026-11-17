// RustServerOp's certificate and auth context calls (ServerOp.cpp's HostCall: GetCertOp's certificates, key blocks,
// private keys and peer data, the auth context's peer data, login texts, community data, hashes and secrets, the
// verifier key; the request ops' auth context: its user id, peer data, sessions and refreshes) and the AuthContext
// class itself, one line per call, compared with rust/crates/ee_won_net/src/golden_authhost.txt (recorded over the C++
// HostCall and AuthContext before they moved into Rust). Every host call goes through the op's own call path
// (ee_won_hostop_call) with real objects (certificates and key blocks signed with won_peerop_test's keys, the
// verifier key loaded from the WON directory), so the same lines are the C++ side's answers before the move and the
// Rust side's after it. Objects are only passed back as they came out; times are logged relative to now. `--print`
// prints the lines instead. The exit code is the number of failures.
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
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static void Log(const string& inLine) { sLines.push_back(inLine); }
static string N(long long inValue) { return std::to_string(inValue); }
static string Hex(const string& inBytes, size_t inMax = 48)
{
    static const char kDigits[] = "0123456789abcdef";
    string s = N((long long)inBytes.size()) + ":";
    for (size_t i = 0; i < inBytes.size() && i < inMax; i++) {
        unsigned char c = static_cast<unsigned char>(inBytes[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static string Text(const std::wstring& inText)
{
    string s;
    for (wchar_t c : inText) s += (c >= 32 && c < 127) ? (char)c : '?';
    return "'" + s + "'";
}

// ---- key material and certificates (won_peerop_test's keys: 0 the auth server, 1 a wrong signer, 2 the client)

static const char* const kPriv[3] = {
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02184bb17e485a32d0a3f02eb0bc5d2b9a09f3cb369e38ff200602080c931814d44a5e5b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a02183dda7c54fd793a4e534f60facc1301e37953f4df16f49861020808e58d3749117c1b",
};
static string Unhex(const char* h)
{
    string r;
    for (; h[0] && h[1]; h += 2) {
        int v = 0;
        for (int i = 0; i < 2; i++) v = v * 16 + (h[i] <= '9' ? h[i] - '0' : h[i] - 'a' + 10);
        r += (char)v;
    }
    return r;
}
static string Der(int k, bool priv)
{
    string d = Unhex(kPriv[k]);
    if (priv) return d;
    d.resize(d.size() - 10);
    d[1] = (char)(d.size() - 2);
    return d;
}
struct W {
    string d;
    void B(int v) { d += (char)(v & 0xff); }
    void S(int v) { B(v); B(v >> 8); }
    void L(long long v) { S((int)(v & 0xffff)); S((int)((v >> 16) & 0xffff)); }
    void Str(const string& s) { S((int)s.size()); d += s; }
    void WStr(const char* ascii) { S((int)strlen(ascii)); for (const char* c = ascii; *c; c++) S(*c); }
};
static const long long kFuture = 0x7f000000, kPast = 1000;
static const int kCommunity = 42;
static string Signed(const string& data, int signer)
{
    string key = Der(signer, true);
    ElGamal k(key.data(), (int)key.size(), true);
    ByteBufferPtr sig = k.Sign(data.data(), (int)data.size());
    return data + string(sig->data(), sig->length());
}
static string Cert1(int key, long long user, bool expired, int signer)
{
    W w;
    w.S(1); w.L(1000); w.L(expired ? kPast : kFuture); w.L(user); w.L(kCommunity); w.S(5);
    w.Str(Der(key, false));
    return Signed(w.d, signer);
}
static string Cert2(int key, long long user, bool expired, int signer)
{
    W w;
    w.S(2); w.L(1000); w.L(expired ? kPast : kFuture); w.S(1);
    W r;
    r.L(user); r.Str(Der(key, false)); r.WStr("Player"); r.B(1); r.L(kCommunity); r.S(5);
    w.S(0); w.S((int)r.d.size()); w.d += r.d;
    return Signed(w.d, signer);
}
static string Block(int signer, int id)
{
    W b;
    b.S(3); b.L(1000); b.L(kFuture); b.S(id); b.S(1); b.Str(Der(0, false));
    return Signed(b.d, signer);
}

// ---- one call through the op's own path

struct Answer {
    bool mHasBytes = false;
    string mBytes;
    EeWonHostCall mCall;
};
static void Sink(void* inContext, const uint8_t* inData, size_t inLength)
{
    Answer* a = static_cast<Answer*>(inContext);
    a->mHasBytes = true;
    a->mBytes.assign(reinterpret_cast<const char*>(inData), inLength);
}
static RustServerOp* sOp;
static Answer Call(int inCode, void* inObj0 = NULL, void* inObj1 = NULL, void* inObj2 = NULL, long long inNum0 = 0,
                   long long inNum1 = 0, const string& inData = string(), const IPAddr* inAddr = NULL)
{
    Answer a;
    memset(&a.mCall, 0, sizeof(a.mCall));
    a.mCall.code = inCode;
    a.mCall.obj[0] = inObj0;
    a.mCall.obj[1] = inObj1;
    a.mCall.obj[2] = inObj2;
    a.mCall.num[0] = inNum0;
    a.mCall.num[1] = inNum1;
    a.mCall.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    a.mCall.len = inData.size();
    if (inAddr != NULL) memcpy(a.mCall.addr, inAddr->GetSixByte(), 6);
    a.mCall.out_sink = Sink;
    a.mCall.out_context = &a;
    ee_won_hostop_call(sOp->GetRustOp(), &a.mCall);
    return a;
}
static string Show(const Answer& a)
{
    return "num " + N(a.mCall.out_num) + " obj " + (a.mCall.out_obj != NULL ? "set" : "null") + " bytes "
           + (a.mHasBytes ? Hex(a.mBytes) : string("none"));
}
// a call whose object result the driver keeps (logged as set or null)
static void* Obj(const string& inName, const Answer& a)
{
    Log(inName + " " + Show(a));
    return a.mCall.out_obj;
}
static void Num(const string& inName, const Answer& a) { Log(inName + " " + Show(a)); }

static string sDir; // the WON directory, with its separator
static void MakeFile(const string& inName, const string& inBytes)
{
    FILE* f = fopen((sDir + inName).c_str(), "wb");
    if (f == NULL) {
        printf("FAIL: cannot write %s\n", (sDir + inName).c_str());
        exit(1);
    }
    fwrite(inBytes.data(), 1, inBytes.size(), f);
    fclose(f);
}
static string ReadFile(const string& inName)
{
    FILE* f = fopen((sDir + inName).c_str(), "rb");
    if (f == NULL) return "(none)";
    string s;
    char b[256];
    size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f);
    return Hex(s, 200);
}
static string Utf16(const char* inAscii)
{
    string s;
    for (const char* c = inAscii; *c; c++) { s += *c; s += (char)0; }
    return s;
}
static string Write(void (AuthContext::*inAppend)(WriteBuffer&), AuthContext* ctx)
{
    WriteBuffer w(0, true);
    (ctx->*inAppend)(w);
    return Hex(string(w.data() != NULL ? w.data() : "", w.length()), 120);
}
static string Peer(AuthContext* ctx)
{
    ConstSmartPtr<AuthPeerData> p = ctx->GetPeerData();
    if (p.get() == NULL) return "peer null";
    long long now = (long long)time(NULL);
    long long d = (long long)p->GetExpireDelta();
    string exp = d == 0 ? "0" : (d + now + (long long)p->GetAuthDelta() - kFuture >= -2 && d + now + (long long)p->GetAuthDelta() - kFuture <= 2)
                                     ? "future" : "?" + N(d);
    return "peer valid " + N(p->IsValid()) + " user " + N((long long)p->GetUserId()) + " delta " + N((long long)p->GetAuthDelta())
           + " community " + N((long long)p->GetFirstCommunity()) + " expire " + exp + " expired " + N(p->CertificateExpired())
           + "/" + N(p->Certificate2Expired()) + " block " + (p->GetPubKeyBlock() != NULL ? N(p->GetPubKeyBlock()->Num(EE_WON_AUTHOBJ_BLOCK_ID)) : "null")
           + " ctx expired " + N(ctx->IsExpired()) + "/" + N(ctx->IsExpired(0x7e000000));
}
// a time relative to now: issue - now as "issue N" when it is within two seconds
static string Rel(const Answer& a, long long inIssue)
{
    long long d = a.mCall.out_num + (long long)time(NULL) - inIssue;
    return string("issue-now ") + (d >= -2 && d <= 2 ? "ok" : "off " + N(d));
}

static void Verifier(void* C)
{
    Num("verifier public", Call(EE_WON_HOST_CERT_VERIFIER_PUBLIC, C));
    Log("load missing " + N(AuthContext::LoadVerifierKey(sDir + "missing.pub")));
    Num("verifier public", Call(EE_WON_HOST_CERT_VERIFIER_PUBLIC, C));
    MakeFile("bad.pub", "not a key");
    Log("load bad " + N(AuthContext::LoadVerifierKey(sDir + "bad.pub")));
    Num("verifier public", Call(EE_WON_HOST_CERT_VERIFIER_PUBLIC, C));
    void* kb = Obj("block", Call(EE_WON_HOST_CERT_NEW_BLOCK, NULL, NULL, NULL, 0, 0, Block(0, 17)));
    Num("verified (bad verifier)", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, kb, C));
    Log("load good " + N(AuthContext::LoadVerifierKey(sDir + "_wonkver.pub")));
    Num("verifier public", Call(EE_WON_HOST_CERT_VERIFIER_PUBLIC, C));
    Num("verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, kb, C));
    Call(EE_WON_HOST_RELEASE, kb);
}

int main(int argc, char** argv)
{
    string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    sDir = (aSlash == string::npos ? string(".") : aExe.substr(0, aSlash)) + "/authhost_files/";
    PlatCreateDirectory(sDir.c_str());
    remove((sDir + "_wonlogin.ks").c_str());
    MakeFile("_wonkver.pub", Der(0, false));
    MakeFile("hashed.bin", string("some file to hash\r\n\0\1\2", 22));
    AsyncSocket::StartWinsock();
    ee_won_random_test_seed(4040);
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(sDir.c_str());
        aCore.Startup();
        SmartPtr<AuthContext> aCtx = new AuthContext; // loads _wonkver.pub, once per process
        AuthContext* ctx = aCtx.get();
        void* C = static_cast<RefCount*>(ctx);
        SmartPtr<GetCertOp> aCertOp = new GetCertOp(ctx);
        sOp = aCertOp.get();
        Log("start " + Peer(ctx));
        Verifier(C);

        // the empty peer data Init made
        void* p0 = Obj("ctx peer", Call(EE_WON_HOST_CERT_CTX_PEER, C));
        Num("peer delta", Call(EE_WON_HOST_CERT_PEER_DELTA, p0));
        void* b0 = Obj("peer block", Call(EE_WON_HOST_CERT_PEER_BLOCK, p0));
        Log(string("peer block again same ") + N(Call(EE_WON_HOST_CERT_PEER_BLOCK, p0).mCall.out_obj == b0));
        Num("block valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, b0));
        Num("block id", Call(EE_WON_HOST_CERT_BLOCK_ID, b0));
        Num("block encrypt", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, b0, NULL, NULL, 0, 0, "abc"));
        Num("block verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, b0, C));
        Num("peer as cert valid", Call(EE_WON_HOST_CERT_VALID, p0));
        Num("peer as block valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, p0));
        Num("peer times", Call(EE_WON_HOST_CERT_TIMES, p0));
        Num("peer delta as cert", Call(EE_WON_HOST_CERT_DELTA, p0));
        Num("peer encrypt", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, p0, NULL, NULL, 0, 0, "abc"));
        Num("block times", Call(EE_WON_HOST_CERT_TIMES, b0));

        // key blocks
        void* kb = Obj("new block", Call(EE_WON_HOST_CERT_NEW_BLOCK, NULL, NULL, NULL, 0, 0, Block(0, 17)));
        Num("valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, kb));
        Num("id", Call(EE_WON_HOST_CERT_BLOCK_ID, kb));
        Num("verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, kb, C));
        Num("times", Call(EE_WON_HOST_CERT_TIMES, kb));
        Log("delta " + Rel(Call(EE_WON_HOST_CERT_DELTA, kb), 1000));
        Num("encrypt", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, kb, NULL, NULL, 0, 0, "12345678"));
        Num("encrypt again", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, kb, NULL, NULL, 0, 0, "12345678"));
        Num("encrypt empty", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, kb));
        void* wrong = Obj("block by key 1", Call(EE_WON_HOST_CERT_NEW_BLOCK, NULL, NULL, NULL, 0, 0, Block(1, 18)));
        Num("valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, wrong));
        Num("verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, wrong, C));
        void* junk = Obj("junk block", Call(EE_WON_HOST_CERT_NEW_BLOCK, NULL, NULL, NULL, 0, 0, "junk"));
        Num("valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, junk));
        Num("id", Call(EE_WON_HOST_CERT_BLOCK_ID, junk));
        Num("verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, junk, C));
        Num("encrypt", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, junk, NULL, NULL, 0, 0, "x"));
        void* none = Obj("empty block", Call(EE_WON_HOST_CERT_NEW_BLOCK));
        Num("valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, none));

        // the context's peer data from a key block
        Call(EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK, C, kb);
        Log("from block " + Peer(ctx));
        void* p1 = Obj("ctx peer", Call(EE_WON_HOST_CERT_CTX_PEER, C));
        void* b1 = Obj("peer block", Call(EE_WON_HOST_CERT_PEER_BLOCK, p1));
        Log(string("peer block is the block ") + N(b1 == kb));
        Num("its id", Call(EE_WON_HOST_CERT_BLOCK_ID, b1));
        Num("its verified", Call(EE_WON_HOST_CERT_BLOCK_VERIFIED, b1, C));
        Num("peer valid", Call(EE_WON_HOST_CERT_BLOCK_VALID, p1));
        Call(EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK, C, p1); // a peer data as the block: a new empty block
        Log("from peer " + Peer(ctx));
        void* p2 = Obj("ctx peer", Call(EE_WON_HOST_CERT_CTX_PEER, C));
        Num("its block id", Call(EE_WON_HOST_CERT_BLOCK_ID, Call(EE_WON_HOST_CERT_PEER_BLOCK, p2).mCall.out_obj));

        // certificates
        void* c1 = Obj("cert1", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 1, 0, Cert1(2, 1000, false, 0)));
        void* c2 = Obj("cert2", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 2, 0, Cert2(2, 2000, false, 0)));
        void* x1 = Obj("cert1 by key 1", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 1, 0, Cert1(2, 1000, false, 1)));
        void* e1 = Obj("cert1 expired", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 1, 0, Cert1(2, 1000, true, 0)));
        void* j2 = Obj("cert2 junk", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 2, 0, "junkjunkjunk"));
        void* k0 = Obj("cert kind 0", Call(EE_WON_HOST_CERT_NEW, NULL, NULL, NULL, 0, 0, Cert2(2, 2000, false, 0)));
        void* certs[] = {c1, c2, x1, e1, j2, k0};
        const char* names[] = {"cert1", "cert2", "cert1 by key 1", "cert1 expired", "cert2 junk", "cert kind 0"};
        for (int i = 0; i < 6; i++) {
            string n = names[i];
            Num(n + " valid", Call(EE_WON_HOST_CERT_VALID, certs[i]));
            Num(n + " verify", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, kb, certs[i]));
            Num(n + " verify by empty", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, b0, certs[i]));
            Num(n + " verify by peer", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, p1, certs[i]));
            Answer t = Call(EE_WON_HOST_CERT_TIMES, certs[i]);
            Num(n + " times", t);
            long long anIssue = 0;
            if (t.mBytes.size() >= 8) memcpy(&anIssue, t.mBytes.data(), 8);
            Log(n + " delta " + Rel(Call(EE_WON_HOST_CERT_DELTA, certs[i]), anIssue));
            Num(n + " encrypt", Call(EE_WON_HOST_CERT_BLOCK_ENCRYPT, certs[i], NULL, NULL, 0, 0, "abcdefgh"));
        }
        Num("verify none", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, kb, NULL));
        Num("verify a block", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, kb, kb));

        // private keys and new peer data
        void* k = Obj("priv", Call(EE_WON_HOST_CERT_PRIV_NEW));
        Num("priv set junk", Call(EE_WON_HOST_CERT_PRIV_SET, k, NULL, NULL, 0, 0, "junk"));
        Num("priv set", Call(EE_WON_HOST_CERT_PRIV_SET, k, NULL, NULL, 0, 0, Der(2, true)));
        void* pp = Obj("new peer", Call(EE_WON_HOST_CERT_NEW_PEER, c1, kb, c2, (long long)(intptr_t)k, 5));
        void* k2 = Obj("priv 2", Call(EE_WON_HOST_CERT_PRIV_NEW));
        void* pq = Obj("new peer no cert2", Call(EE_WON_HOST_CERT_NEW_PEER, e1, kb, NULL, (long long)(intptr_t)k2, -7));
        void* pr = Obj("new peer no key", Call(EE_WON_HOST_CERT_NEW_PEER, c1, NULL, c2, (long long)(intptr_t)k2, 0));
        Call(EE_WON_HOST_CERT_PRIV_FREE, k);
        Call(EE_WON_HOST_CERT_PRIV_FREE, k2);
        void* peers[] = {pp, pq, pr};
        for (int i = 0; i < 3; i++) {
            string n = "peer " + N(i);
            Num(n + " valid", Call(EE_WON_HOST_CERT_VALID, peers[i]));
            Num(n + " delta", Call(EE_WON_HOST_CERT_PEER_DELTA, peers[i]));
            void* b = Call(EE_WON_HOST_CERT_PEER_BLOCK, peers[i]).mCall.out_obj;
            Log(n + " block " + (b == NULL ? "null" : b == kb ? "the block" : "other"));
            if (b != NULL) Num(n + " block id", Call(EE_WON_HOST_CERT_BLOCK_ID, b));
            Num(n + " verify", Call(EE_WON_HOST_CERT_BLOCK_VERIFY, peers[i], c1));
        }

        // the context's peer data set
        Call(EE_WON_HOST_CERT_SET_PEER, C, pp);
        Log("set " + Peer(ctx));
        void* p3 = Obj("ctx peer", Call(EE_WON_HOST_CERT_CTX_PEER, C));
        Num("its delta", Call(EE_WON_HOST_CERT_PEER_DELTA, p3));
        Num("its valid", Call(EE_WON_HOST_CERT_VALID, p3));
        Call(EE_WON_HOST_CERT_SET_PEER, C, pq);
        Log("set no cert2 " + Peer(ctx));
        Call(EE_WON_HOST_CERT_SET_PEER, C, c1); // a certificate as the peer data: none
        Log("set a cert " + Peer(ctx));
        Obj("ctx peer", Call(EE_WON_HOST_CERT_CTX_PEER, C));
        Call(EE_WON_HOST_CERT_SET_PEER, C, NULL);
        Log("set none " + Peer(ctx));
        Call(EE_WON_HOST_CERT_SET_PEER, C, pp);
        Log("set again " + Peer(ctx));

        // the login texts, community data, hashes and secrets
        ctx->SetUserName(L"Tester");
        ctx->SetPassword(L"secret");
        ctx->SetCommunity(L"EmpireEarth");
        ctx->AddNickname(L"nick");
        Num("user", Call(EE_WON_HOST_CERT_USER, C));
        Num("password", Call(EE_WON_HOST_CERT_PASSWORD, C));
        Num("community", Call(EE_WON_HOST_CERT_COMMUNITY, C));
        Num("hashes", Call(EE_WON_HOST_CERT_HASHES, C, NULL, NULL, 0, 0, "0123456789abcdef"));
        Log("hash file " + N(ctx->SetHashFile(L"EmpireEarth", sDir + "hashed.bin")));
        Num("hashes", Call(EE_WON_HOST_CERT_HASHES, C, NULL, NULL, 0, 0, "0123456789abcdef"));
        Num("hashes no seed", Call(EE_WON_HOST_CERT_HASHES, C));
        Call(EE_WON_HOST_CERT_SET_PASSWORD, C, NULL, NULL, 0, 0, Utf16("newpass") + "x");
        Log("password now " + Text(ctx->GetPassword()));
        Num("password", Call(EE_WON_HOST_CERT_PASSWORD, C));
        Call(EE_WON_HOST_CERT_SET_PASSWORD, C);
        Log("password empty " + Text(ctx->GetPassword()));
        ByteBufferPtr aData = new ByteBuffer("data!", 5, false);
        ctx->AddUserData(L"EmpireEarth", aData.get());
        Num("community with data", Call(EE_WON_HOST_CERT_COMMUNITY, C));
        Call(EE_WON_HOST_CERT_CLEAR_ONE_TIME, C);
        Num("community cleared", Call(EE_WON_HOST_CERT_COMMUNITY, C));
        Log("community data " + Write(&AuthContext::AppendCommunityData, ctx));
        Call(EE_WON_HOST_CERT_LOAD_SECRETS, C);
        Log("secrets " + Write(&AuthContext::AppendLoginSecrets, ctx) + " file " + ReadFile("_wonlogin.ks"));
        Call(EE_WON_HOST_CERT_ADD_SECRET, C, NULL, NULL, 0, 0, "AAAAAAAA");
        Call(EE_WON_HOST_CERT_ADD_SECRET, C, NULL, NULL, 0, 0, "BBBBBBBBextra");
        Call(EE_WON_HOST_CERT_ADD_SECRET, C, NULL, NULL, 0, 0, "CCCCCCCC");
        Log("added " + Write(&AuthContext::AppendLoginSecrets, ctx) + " file " + ReadFile("_wonlogin.ks"));
        Call(EE_WON_HOST_CERT_REMOVE_SECRET, C, NULL, NULL, 0, 0, "CCCCCCCC");
        Log("removed " + Write(&AuthContext::AppendLoginSecrets, ctx) + " file " + ReadFile("_wonlogin.ks"));
        Call(EE_WON_HOST_CERT_COMMIT, C, NULL, NULL, kFuture - 100, 50);
        Log("committed " + Write(&AuthContext::AppendLoginSecrets, ctx) + " file " + ReadFile("_wonlogin.ks"));
        Call(EE_WON_HOST_CERT_ADD_SECRET, C, NULL, NULL, 0, 0, "DDDDDDDD");
        Call(EE_WON_HOST_CERT_COMMIT, C, NULL, NULL, 100, 50); // expired at once
        Log("committed past " + Write(&AuthContext::AppendLoginSecrets, ctx) + " file " + ReadFile("_wonlogin.ks"));
        Call(EE_WON_HOST_CERT_LOAD_SECRETS, C);
        Log("loaded " + Write(&AuthContext::AppendLoginSecrets, ctx));

        // the request ops' auth context
        SmartPtr<RustServerOp> aPlain = RustServerOp::NewGroupListReq(IPAddr(), 0);
        sOp = aPlain.get();
        Num("plain has auth", Call(EE_WON_HOST_HAS_AUTH));
        SmartPtr<RustServerOp> anAuthOp = RustServerOp::NewGetAccount(ctx->GetServerContext().get(), ctx);
        sOp = anAuthOp.get();
        Num("has auth", Call(EE_WON_HOST_HAS_AUTH));
        Num("user id", Call(EE_WON_HOST_CERT_USER_ID));
        Answer pd = Call(EE_WON_HOST_PEER_CTX_DATA);
        EeWonAuthObj* aPeer = reinterpret_cast<EeWonAuthObj*>((intptr_t)pd.mCall.out_num);
        Log("ctx data " + string(aPeer != NULL ? "set" : "null") + " valid "
            + N(aPeer ? ee_won_authobj_num(aPeer, EE_WON_AUTHOBJ_VALID, 0, 0, 0) : -1) + " user "
            + N(aPeer ? ee_won_authobj_num(aPeer, EE_WON_AUTHOBJ_USER_ID, 0, 0, 0) : -1) + " delta "
            + N(aPeer ? ee_won_authobj_num(aPeer, EE_WON_AUTHOBJ_PEER_DELTA, 0, 0, 0) : -1));
        ee_won_authobj_free(aPeer);
        IPAddr anAddr, anOther;
        anAddr.SetWithDefaultPortNoDNS("10.0.0.5:15101", 1);
        anOther.SetWithDefaultPortNoDNS("10.0.0.6:15101", 1);
        // sessions are ee_won handles (round 22): made through the op's own path, as a peer auth op makes them
        Blowfish aNoKey;
        void* aSession = Call(EE_WON_HOST_PEER_NEW_SESSION, NULL, NULL, NULL, (long long)(intptr_t)ee_won_auth_session_new(
            AUTH_TYPE_SESSION, 7, reinterpret_cast<const EeWonBlowfish*>(&aNoKey), 4, (int64_t)time(NULL))).mCall.out_obj;
        void* aSession2 = Call(EE_WON_HOST_PEER_NEW_SESSION, NULL, NULL, NULL, (long long)(intptr_t)ee_won_auth_session_new(
            AUTH_TYPE_PERSISTENT, 8, reinterpret_cast<const EeWonBlowfish*>(&aNoKey), 2, (int64_t)time(NULL))).mCall.out_obj;
        Num("get none", Call(EE_WON_HOST_GET_SESSION, NULL, NULL, NULL, 0, 0, string(), &anAddr));
        Call(EE_WON_HOST_PUT_SESSION, aSession, NULL, NULL, 0, 0, string(), &anAddr);
        Call(EE_WON_HOST_PUT_SESSION, aSession2, NULL, NULL, 0, 0, string(), &anAddr);
        Call(EE_WON_HOST_PUT_SESSION, NULL, NULL, NULL, 0, 0, string(), &anOther);
        for (int i = 0; i < 3; i++) {
            Answer g = Call(EE_WON_HOST_GET_SESSION, NULL, NULL, NULL, 0, 0, string(), &anAddr);
            void* s = g.mCall.out_obj;
            Log("get " + N(i) + " " + (s == NULL ? "null" : s == aSession ? "first" : s == aSession2 ? "second" : "other"));
            if (s != NULL) Call(EE_WON_HOST_RELEASE, s);
        }
        Num("get other", Call(EE_WON_HOST_GET_SESSION, NULL, NULL, NULL, 0, 0, string(), &anOther));
        // sessions again (through the class until round 22, whose GetSession / PutSession had no other caller), and
        // SetPeerData's clear
        Call(EE_WON_HOST_PUT_SESSION, aSession, NULL, NULL, 0, 0, string(), &anAddr);
        void* aGot = Call(EE_WON_HOST_GET_SESSION, NULL, NULL, NULL, 0, 0, string(), &anAddr).mCall.out_obj;
        Log(string("class get ") + (aGot == aSession ? "first" : "?"));
        if (aGot != NULL) Call(EE_WON_HOST_RELEASE, aGot);
        Call(EE_WON_HOST_PUT_SESSION, aSession2, NULL, NULL, 0, 0, string(), &anAddr);
        ctx->SetPeerData(ctx->GetPeerData().get());
        aGot = Call(EE_WON_HOST_GET_SESSION, NULL, NULL, NULL, 0, 0, string(), &anAddr).mCall.out_obj;
        Log(string("after set peer ") + (aGot == NULL ? "null" : "kept"));
        if (aGot != NULL) Call(EE_WON_HOST_RELEASE, aGot);
        Call(EE_WON_HOST_RELEASE, aSession);
        Call(EE_WON_HOST_RELEASE, aSession2);
        // a blocking refresh with no auth servers, then an asynchronous one with a waiting op
        Num("refresh block", Call(EE_WON_HOST_PEER_REFRESH_BLOCK, NULL, NULL, NULL, 2000));
        Log("after refresh " + Peer(ctx));
        SmartPtr<AsyncOp> aLight = new AsyncOp;
        EeWonLightCert* aLightCert = ee_won_light_cert_new();
        Call(EE_WON_HOST_PEER_REFRESH_ASYNC, static_cast<RefCount*>(aLight.get()), NULL, NULL, (long long)(intptr_t)aLightCert);
        for (int i = 0; i < 100 && (aLight->GetStatus() == WS_Pending); i++) aCore.Pump(20);
        EeWonAuthObj* aLightPeer = ee_won_light_cert_peer(aLightCert);
        Log("light status " + N(aLight->GetStatus()) + " pending " + N((aLight->GetStatus() == WS_Pending)) + " peer " + (aLightPeer != NULL ? "set" : "null"));
        ee_won_authobj_free(aLightPeer);
        ee_won_light_cert_free(aLightCert);
        Log("after async " + Peer(ctx));

        // the class's server context and auto-refresh
        SmartPtr<ServerContext> aServers = ctx->GetServerContext();
        long long aCounts[8];
        aCounts[0] = (long long)aServers->GetNumAddresses();
        aCounts[1] = ctx->AddAddress(anAddr);
        aCounts[2] = ctx->AddAddress(anAddr);
        aCounts[3] = ctx->AddAddress(anOther);
        aCounts[4] = (long long)aServers->GetNumAddresses();
        aCounts[5] = ctx->RemoveAddress(anAddr);
        aCounts[6] = ctx->RemoveAddress(anAddr);
        aCounts[7] = (long long)aServers->GetNumAddresses();
        Log("servers " + N(aCounts[0]) + " add " + N(aCounts[1]) + " again " + N(aCounts[2]) + " other " + N(aCounts[3])
            + " now " + N(aCounts[4]));
        Log("remove " + N(aCounts[5]) + " again " + N(aCounts[6]) + " now " + N(aCounts[7]));
        ctx->RemoveAllAddresses();
        long long aLeft = (long long)aServers->GetNumAddresses();
        long long aFromDir = ctx->AddAddressesFromDir(DirEntityList());
        Log("remove all " + N(aLeft) + " from dir " + N(aFromDir));
        SmartPtr<ServerContext> aNew = new ServerContext;
        aNew->AddAddress(anOther);
        ctx->SetServerContext(aNew.get());
        bool aSame = ctx->GetServerContext().get() == aNew.get();
        Log(string("set servers ") + (aSame ? "same" : "?") + " " + N((long long)ctx->GetServerContext()->GetNumAddresses()));
        ctx->SetDoAutoRefresh(true);
        ctx->StartAutoRefresh(true);
        ctx->StopAutoRefresh();
        ctx->SetDoAutoRefresh(false);
        ctx->Kill();
        Log("end " + Peer(ctx));

        // every object the calls handed out, released (borrowed blocks apart)
        sOp = aCertOp.get();
        Call(EE_WON_HOST_ADD_REF, c1);
        void* owned[] = {p0, kb, wrong, junk, none, p1, p2, c1, c1, c2, x1, e1, j2, k0, pp, pq, pr, p3};
        for (void* o : owned)
            if (o != NULL) Call(EE_WON_HOST_RELEASE, o);
        aLight = NULL;
        aPlain = NULL;
        anAuthOp = NULL;
        aCertOp = NULL;
        sOp = NULL;
        aCtx = NULL;
        aCore.Pump(20);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    Log("done");
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    string aPath = __FILE__;
    aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aPath += "../../rust/crates/ee_won_net/src/golden_authhost.txt";
    FILE* aFile = fopen(aPath.c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", aPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_authhost_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
