// FindPatchOp's patch choice (GetPatchList, GetValidPatchList, GetHighestPatchList and DoFinish's patch ids and 4-byte
// data), DetectFirewallOp's GetNextRequest, CheckResponse and datagram wait, and RegAuth's password cipher and port
// default: the original's statements (copied from FindPatchOp.cpp, DetectFirewallOp.cpp and RegAuth.cpp before the
// port, over stand-ins) against include/ee/wonapi/WONMiscOpsMsg.h over ee_won's miscops module, on generated input.
// The version compares log their calls, so their order and count are compared too.
//
// `--print N` prints N patch-list cases in the golden's format (rust/crates/ee_won/src/golden_miscops.txt; the Rust
// replay is `miscops::tests::golden`). The exit code is the number of failed checks.
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
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLittleEndian.h"
#include "ee/wonapi/WONMiscOpsMsg.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 6161;
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
static string RandBytes(int n)
{
    string s;
    for (int i = 0; i < n; i++) s += (char)Next();
    return s;
}

// ---- FindPatchOp

// the compares a title could set, each logging its calls: 0 the default (std::string::compare), 1 numeric (atoi),
// 2 reversed, 3 by length
static string sCmpLog;
static int sCmpKind;
static int VersionCompare(const string& a, const string& b)
{
    sCmpLog += Hex(a) + "/" + Hex(b) + ";";
    switch (sCmpKind) {
    case 0: return a.compare(b);
    case 1: return atoi(a.c_str()) - atoi(b.c_str());
    case 2: return b.compare(a);
    default: return (int)a.size() - (int)b.size();
    }
}
typedef int (*VersionCompareFn)(const string&, const string&);

struct SPatch {
    string mFromVersion, mToVersion;
    int mTag;
};
struct SFind {
    std::list<SPatch> mPatchList;
    VersionCompareFn mVersionCompare;
    std::set<string> mValidVersionSet, mFilteredVersionSet;
};

namespace OrigFind {
static bool ValidFromVersion(SFind& o, const std::string& theFromVersion, const SPatch& theInfo)
{
    if (theInfo.mFromVersion == theFromVersion)
        return true;
    else if (theInfo.mFromVersion.empty() && o.mVersionCompare(theInfo.mToVersion, theFromVersion) > 0)
        return true;
    else
        return false;
}
static void GetPatchList(SFind& o, const std::string& theFromVersion, std::list<SPatch>& aList)
{
    aList.clear();
    std::list<SPatch>::iterator anItr = o.mPatchList.begin();
    while (anItr != o.mPatchList.end()) {
        const SPatch& anInfo = *anItr;
        if (ValidFromVersion(o, theFromVersion, anInfo))
            aList.push_back(anInfo);
        ++anItr;
    }
}
static void GetValidPatchList(SFind& o, const std::string& theFromVersion, std::list<SPatch>& aList)
{
    aList.clear();
    std::list<SPatch>::iterator anItr = o.mPatchList.begin();
    while (anItr != o.mPatchList.end()) {
        const SPatch& anInfo = *anItr;
        if (ValidFromVersion(o, theFromVersion, anInfo) && o.mValidVersionSet.find(anInfo.mToVersion) != o.mValidVersionSet.end())
            aList.push_back(anInfo);
        ++anItr;
    }
}
static void GetHighestPatchList(SFind& o, const std::string& theFromVersion, const std::string& theToVersion, std::list<SPatch>& aList)
{
    aList.clear();
    std::list<SPatch>::iterator aPatchItr;
    std::string aHighestVersion;
    bool highestVersionHasFromVersion = false;
    for (aPatchItr = o.mPatchList.begin(); aPatchItr != o.mPatchList.end(); ++aPatchItr) {
        const SPatch& anInfo = *aPatchItr;
        if (!ValidFromVersion(o, theFromVersion, anInfo))
            continue;
        if (o.mVersionCompare(anInfo.mToVersion, theToVersion) <= 0)
            continue;
        if (o.mFilteredVersionSet.find(anInfo.mToVersion) != o.mFilteredVersionSet.end())
            continue;
        if (aHighestVersion.empty()) {
            aHighestVersion = anInfo.mToVersion;
            highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
        }
        int aComp = o.mVersionCompare(anInfo.mToVersion, aHighestVersion);
        if (aComp < 0)
            continue;
        else if (aComp > 0) {
            aHighestVersion = anInfo.mToVersion;
            highestVersionHasFromVersion = !anInfo.mFromVersion.empty();
            aList.clear();
            aList.push_back(anInfo);
        } else {
            if (!highestVersionHasFromVersion) {
                if (!anInfo.mFromVersion.empty()) {
                    highestVersionHasFromVersion = true;
                    aList.clear();
                    aList.push_back(anInfo);
                } else
                    aList.push_back(anInfo);
            } else {
                if (!anInfo.mFromVersion.empty())
                    aList.push_back(anInfo);
            }
        }
    }
}
static void SplitId(SPatch& anInfo, const std::string& aPatchId)
{
    int aPos = aPatchId.find(L'~');
    if (aPos == std::wstring::npos)
        anInfo.mToVersion = aPatchId;
    else {
        anInfo.mFromVersion = aPatchId.substr(0, aPos);
        anInfo.mToVersion = aPatchId.substr(aPos + 1);
    }
}
} // namespace OrigFind

static string RandVersion()
{
    static const char* kV[] = {"", "1", "2", "10", "1.0", "1.1", "1.2", "2.0", "9", "01", "a", "b2"};
    return Pick(6) == 0 ? RandBytes(Pick(3)) : string(kV[Pick(12)]);
}
static string Tags(const std::list<SPatch>& l)
{
    string s;
    char b[16];
    for (std::list<SPatch>::const_iterator i = l.begin(); i != l.end(); ++i) {
        snprintf(b, sizeof(b), "%s%d", s.empty() ? "" : ",", i->mTag);
        s += b;
    }
    return s.empty() ? "-" : s;
}
static string SetText(const std::set<string>& s)
{
    string r;
    for (std::set<string>::const_iterator i = s.begin(); i != s.end(); ++i) r += (r.empty() ? "" : ",") + Hex(*i);
    return r.empty() ? "*" : r;
}

static void FindCase(bool inPrint)
{
    SFind o;
    o.mVersionCompare = VersionCompare;
    sCmpKind = Pick(4);
    int n = Pick(9);
    for (int i = 0; i < n; i++) {
        SPatch p;
        p.mFromVersion = Pick(2) ? "" : RandVersion();
        p.mToVersion = RandVersion();
        p.mTag = i;
        o.mPatchList.push_back(p);
    }
    int k = Pick(4);
    for (int i = 0; i < k; i++) o.mValidVersionSet.insert(RandVersion());
    k = Pick(3);
    for (int i = 0; i < k; i++) o.mFilteredVersionSet.insert(RandVersion());
    string from = RandVersion(), to = RandVersion();
    int which = Pick(3);
    std::list<SPatch> a, b;
    if (Pick(8) == 0) b.push_back(SPatch());  // a list that held something is cleared
    sCmpLog.clear();
    if (which == 0) OrigFind::GetPatchList(o, from, a);
    else if (which == 1) OrigFind::GetValidPatchList(o, from, a);
    else OrigFind::GetHighestPatchList(o, from, to, a);
    string logA = sCmpLog;
    sCmpLog.clear();
    FindPatchMsg<SFind>::List(o, which, from, to, b);
    CHECK(Tags(a) == Tags(b));
    CHECK(logA == sCmpLog);
    if (inPrint) {
        string ps;
        for (std::list<SPatch>::const_iterator i = o.mPatchList.begin(); i != o.mPatchList.end(); ++i)
            ps += (ps.empty() ? "" : ",") + Hex(i->mFromVersion) + "~" + Hex(i->mToVersion);
        const std::set<string>& set = which == 1 ? o.mValidVersionSet : o.mFilteredVersionSet;
        printf("find %d %d %s %s %s %s %s %s\n", which, sCmpKind, Hex(from).c_str(), Hex(to).c_str(),
               ps.empty() ? "*" : ps.c_str(), SetText(set).c_str(), Tags(a).c_str(), Hex(logA).c_str());
    }
    // the patch ids and their data
    string id = Pick(3) ? RandVersion() + "~" + RandVersion() : RandVersion() + (Pick(5) == 0 ? "~~" : "");
    SPatch pa, pb;
    OrigFind::SplitId(pa, id);
    FindPatchMsg<SFind>::SplitId(pb, id);
    CHECK(pa.mFromVersion == pb.mFromVersion && pa.mToVersion == pb.mToVersion);
    string d = RandBytes(Pick(3) ? 4 : Pick(7));
    unsigned long va = 77, vb = 77;
    if (d.size() == 4) va = LongFromLittleEndian(*(unsigned long*)d.data());
    FindPatchMsg<SFind>::Le32(vb, d.data(), (unsigned long)d.size());
    CHECK(va == vb);
}

// ---- DetectFirewallOp

struct SAddr {
    string mLog;
    void Fn2B2C20(long h, unsigned short p)
    {
        char b[48];
        snprintf(b, sizeof(b), "[%ld:%u]", h, (unsigned)p);
        this->mLog += b;
    }
};
struct SFw {
    unsigned char mLengthFieldSize;
    bool mUseUDP, mWaitForConnect;
    unsigned short mListenPort;
    unsigned long mMaxConnectWaitTime;
    ByteBufferPtr mRequest, mResponse;
    SAddr mPublicAddr;
    WONStatus InvalidReplyHeader() { return 100057; }
    WONStatus RecvServerDatagram() { this->mPublicAddr.mLog += "recv;"; return 777; }
};

namespace OrigFw {
static WONStatus GetNextRequest(SFw& o)
{
    WriteBuffer request(o.mLengthFieldSize, true);
    request.AppendByte(5);
    request.AppendShort(3);
    request.AppendShort(o.mUseUDP ? 3 : 1);
    request.AppendShort(o.mListenPort);
    if (!o.mUseUDP) {
        request.AppendBool(o.mWaitForConnect);
        request.AppendLong(o.mMaxConnectWaitTime);
        request.AppendBool(false);
    }
    o.mRequest = request.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}
static WONStatus CheckResponse(SFw& o)
{
    ReadBuffer reply(o.mResponse->data(), o.mResponse->length());
    unsigned char header = reply.ReadByte();
    unsigned short service = reply.ReadShort();
    unsigned short message = reply.ReadShort();
    if (header != 5 || service != 3 || message != 2) return o.InvalidReplyHeader();
    short status = reply.ReadShort();
    if (reply.Available() >= 4) {
        long host = reply.ReadLong();
        if (host != 0) o.mPublicAddr.Fn2B2C20(host, 0);
    }
    if (o.mUseUDP) return o.RecvServerDatagram();
    return status;
}
static unsigned long Wait(unsigned long inTimeLeft, unsigned long mMaxConnectWaitTime, bool inAsync)
{
    unsigned long wait = inTimeLeft;
    if (mMaxConnectWaitTime > 0 && mMaxConnectWaitTime < wait)
        wait = mMaxConnectWaitTime;
    if (inAsync && wait == 0) wait = 0xffffffff;
    return wait;
}
} // namespace OrigFw

static string Bytes(const ByteBuffer* b) { return b ? string(b->data(), b->length()) : string("*"); }

static void FirewallCase()
{
    SFw o;
    static const unsigned char kLfs[] = {0, 1, 2, 4};
    o.mLengthFieldSize = kLfs[Pick(4)];
    o.mUseUDP = Pick(2) != 0;
    o.mWaitForConnect = Pick(2) != 0;
    o.mListenPort = (unsigned short)Next();
    o.mMaxConnectWaitTime = Pick(3) ? Pick(100) : Next() << 8;
    SFw p = o;
    CHECK(OrigFw::GetNextRequest(o) == FirewallMsg<SFw>::GetNextRequest(p));
    CHECK(Bytes(o.mRequest.get()) == Bytes(p.mRequest.get()));
    // replies: well-formed with and without a host, then cut and mutated
    string r = string(1, (char)(Pick(8) ? 5 : Pick(8))) + "\x03" + string(1, '\0') + string(1, (char)(Pick(8) ? 2 : 3)) +
               string(1, '\0') + RandBytes(2) + (Pick(2) ? (Pick(3) ? RandBytes(4) : string(4, '\0')) : string()) +
               RandBytes(Pick(3));
    if (Pick(3) == 0) r = r.substr(0, Pick((int)r.size() + 1));
    if (Pick(6) == 0 && !r.empty()) r[Pick((int)r.size())] ^= (char)(1 << Pick(8));
    o.mResponse = new ByteBuffer(r.data(), (unsigned long)r.size(), false);
    p.mResponse = o.mResponse;
    long sa = -1, sb = -1;
    bool ta = false, tb = false;
    try { sa = OrigFw::CheckResponse(o); } catch (ReadBufferException&) { ta = true; }
    try { sb = FirewallMsg<SFw>::CheckResponse(p); } catch (ReadBufferException&) { tb = true; }
    CHECK(ta == tb && sa == sb && o.mPublicAddr.mLog == p.mPublicAddr.mLog);
    unsigned long tl = Pick(3) ? Pick(200) : Next() << 8, mw = Pick(3) ? Pick(200) : Next() << 8;
    bool as = Pick(2) != 0;
    CHECK(OrigFw::Wait(tl, mw, as) == FirewallMsg<SFw>::Wait(tl, mw, as));
}

// ---- RegAuth

static void RegAuthCase()
{
    static const unsigned char gKeyData[8] = {0x23, 0x77, 0xCF, 0x67, 0x99, 0xAB, 0xBA, 0xE5};
    Blowfish aKey(gKeyData, 8);
    string pw = RandBytes(Pick(40));
    ByteBufferPtr e = aKey.Encrypt(pw.c_str(), (int)pw.size());
    string eb;
    bool ok = RegAuthPassword(false, pw.data(), (unsigned long)pw.size(), eb);
    CHECK(ok == (e.get() != NULL) && (!ok || Bytes(e.get()) == eb));
    // stored values: a real one, cut or mutated, or garbage
    string c = Pick(3) ? Bytes(e.get()) : RandBytes(Pick(30));
    if (Pick(3) == 0 && !c.empty()) c[Pick((int)c.size())] ^= 1;
    if (Pick(4) == 0) c = c.substr(0, Pick((int)c.size() + 1));
    ByteBufferPtr d = aKey.Decrypt(c.data(), (int)c.size());
    string db;
    ok = RegAuthPassword(true, c.data(), (unsigned long)c.size(), db);
    CHECK(ok == (d.get() != NULL) && (!ok || string(d->data(), d->length()) == db));
    // the port: QueryDWORD's result and the value
    int q = Pick(3);
    DWORD v = Pick(2) ? Next() : Pick(100);
    DWORD anAuthPort(15200);
    anAuthPort = v;
    if (q == 1) anAuthPort = 15200;
    unsigned short theAuthPortR = anAuthPort;
    CHECK(theAuthPortR == ee_won_regauth_port(q, v));
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        for (int i = 0; i < aPrint; i++) FindCase(true);
        return 0;
    }
    for (int i = 0; i < 200000; i++) FindCase(false);
    for (int i = 0; i < 200000; i++) FirewallCase();
    for (int i = 0; i < 50000; i++) RegAuthCase();
    printf("won_miscops_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
