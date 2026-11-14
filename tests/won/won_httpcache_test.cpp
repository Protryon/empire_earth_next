// The HTTP cache and its entries (HTTPCache.cpp: the cache's directory, index file, ids, file names, entries, the
// 14-day keep rule and the deleted files; the global cache) through the HTTP get op's own cache calls
// (EE_WON_HOST_CACHE_ENTRY, _ENTRY_GET, _ENTRY_SET, _CACHE_UPDATE and the references' ADD_REF / RELEASE), one line
// per call, compared with rust/crates/ee_won/src/golden_httpcache.txt (recorded over the C++ HTTPCache and
// ServerOp.cpp's HostCall before they moved into Rust). Every call goes through a real HTTP get op's call path
// (ee_won_hostop_call); only the cache's construction and the global cache's getter and reset name the class (the
// three functions below). Times are logged relative to now; files are listed by name. `--print` prints the lines
// instead. The exit code is the number of failures.
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
#include <filesystem>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;
namespace fs = std::filesystem;

// ---- the three statements that name the cache (a new cache with one reference; the global one, borrowed; reset).
// The golden was recorded with the C++ class here: new HTTPCache(path, full) and AddRef, HTTPCache::GetGlobalCache(),
// HTTPCache::SetGlobalCache(NULL)

static void* NewCache(const string& inPath, bool inFullPath)
{
    return ee_won_http_cache_new(reinterpret_cast<const uint8_t*>(inPath.data()), inPath.size(), inFullPath);
}
static void* GlobalCache()
{
    const string& aDir = WONAPICore::GetDefaultFileDirectory();
    return ee_won_http_cache_global(reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size());
}
static void ResetGlobalCache() { ee_won_http_cache_reset_global(); }

// ---- the log

static std::vector<string> sLines;
static void Log(const string& inLine) { sLines.push_back(inLine); }
static string N(long long inValue) { return std::to_string(inValue); }
static string Hex(const string& inBytes, size_t inMax = 64)
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
static string sCwd; // the current directory, '/' separated, with a trailing '/'
static string Slashes(string s)
{
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}
// a path as text: the current directory's prefix (any case, either separator) as "<cwd>/"
static string PathText(const string& inPath)
{
    string s = Slashes(inPath);
    if (s.size() >= sCwd.size()) {
        string head = s.substr(0, sCwd.size());
        bool same = true;
        for (size_t i = 0; i < head.size() && same; i++)
            same = tolower((unsigned char)head[i]) == tolower((unsigned char)sCwd[i]);
        if (same) return "<cwd>/" + s.substr(sCwd.size()) + " (sep " + (inPath.find('\\') != string::npos ? "\\" : "/") + ")";
    }
    return "'" + inPath + "'";
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
static Answer Call(int inCode, void* inObj0 = NULL, void* inObj1 = NULL, long long inNum0 = 0, long long inNum1 = 0,
                   const string& inData = string())
{
    Answer a;
    memset(&a.mCall, 0, sizeof(a.mCall));
    a.mCall.code = inCode;
    a.mCall.obj[0] = inObj0;
    a.mCall.obj[1] = inObj1;
    a.mCall.num[0] = inNum0;
    a.mCall.num[1] = inNum1;
    a.mCall.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    a.mCall.len = inData.size();
    a.mCall.out_sink = Sink;
    a.mCall.out_context = &a;
    ee_won_hostop_call(sOp->GetRustOp(), &a.mCall);
    return a;
}

// the entry for a location (a new reference), logged. Every answer gets a new number (an address freed and reused
// must not name two objects alike); identity is logged only where the driver compares two live entries.
static std::map<void*, int> sNames; // object -> its number in the log (the latest answer's)
static int sNextName;
static string Name(void* inObj)
{
    if (inObj == NULL) return "null";
    std::map<void*, int>::iterator i = sNames.find(inObj);
    return i == sNames.end() ? string("?") : "#" + N(i->second);
}
static void* Entry(void* inCache, const string& inLocation)
{
    Answer a = Call(EE_WON_HOST_CACHE_ENTRY, inCache, NULL, 0, 0, inLocation);
    if (a.mCall.out_obj != NULL) sNames[a.mCall.out_obj] = ++sNextName;
    Log("entry '" + inLocation + "' -> " + Name(a.mCall.out_obj) + " num " + N(a.mCall.out_num) + " bytes "
        + (a.mHasBytes ? Hex(a.mBytes) : string("none")));
    return a.mCall.out_obj;
}
static string Rel(long long inTime)
{
    long long now = (long long)time(NULL);
    if (inTime > now - 100000000LL && inTime < now + 100000000LL) {
        long long d = inTime - now;
        if (d >= -2 && d <= 2) return "now";
        return "now" + string(d < 0 ? "-" : "+") + "~" + N((d < 0 ? -d : d) / 10 * 10);
    }
    return N(inTime);
}
static void Show(const string& inWhat, void* e)
{
    Answer v = Call(EE_WON_HOST_ENTRY_GET, e, NULL, 0);
    Answer x = Call(EE_WON_HOST_ENTRY_GET, e, NULL, 1);
    Answer p = Call(EE_WON_HOST_ENTRY_GET, e, NULL, 2);
    Answer t = Call(EE_WON_HOST_ENTRY_GET, e, NULL, 3);
    Log(inWhat + " " + Name(e) + " valid " + N(v.mCall.out_num) + " expires " + Rel(x.mCall.out_num) + " path "
        + (p.mHasBytes ? PathText(p.mBytes) : string("none")) + " tag " + (t.mHasBytes ? Hex(t.mBytes) : string("none")));
}
static string LocalPath(void* e) { return Call(EE_WON_HOST_ENTRY_GET, e, NULL, 2).mBytes; }
static void Set(void* e, int inWhat, long long inNum, const string& inData = string())
{
    Answer a = Call(EE_WON_HOST_ENTRY_SET, e, NULL, inWhat, inNum, inData);
    Log("set " + Name(e) + " " + N(inWhat) + " " + N(inNum) + " " + Hex(inData) + " -> num " + N(a.mCall.out_num));
}
static void Release(void* o) { Call(EE_WON_HOST_RELEASE, o); }
static void AddRef(void* o) { Call(EE_WON_HOST_ADD_REF, o); }

// ---- files

static void MakeFile(const string& inPath, const string& inBytes)
{
    FILE* f = fopen(inPath.c_str(), "wb");
    if (f == NULL) {
        Log("cannot write " + PathText(inPath));
        return;
    }
    fwrite(inBytes.data(), 1, inBytes.size(), f);
    fclose(f);
}
static string ReadFile(const string& inPath, bool* outFound = NULL)
{
    FILE* f = fopen(inPath.c_str(), "rb");
    if (outFound != NULL) *outFound = f != NULL;
    if (f == NULL) return string();
    string s;
    char b[256];
    size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}
static void ListDir(const string& inDir)
{
    std::error_code ec;
    if (!fs::is_directory(inDir, ec)) {
        Log("dir " + inDir + " missing");
        return;
    }
    std::vector<string> names;
    for (fs::directory_iterator i(inDir, ec), end; !ec && i != end; i.increment(ec))
        names.push_back(i->path().filename().string());
    std::sort(names.begin(), names.end());
    string s = "dir " + inDir + ":";
    for (size_t i = 0; i < names.size(); i++) s += " " + names[i];
    Log(s);
}
// cachemap.dat as text: version, count, each entry (id, valid, expiration, last access, four strings)
struct Rd {
    string d;
    size_t p = 0;
    bool ok = true;
    long long I()
    {
        if (p + 4 > d.size()) { ok = false; return 0; }
        int32_t v;
        memcpy(&v, d.data() + p, 4);
        p += 4;
        return v;
    }
    int B()
    {
        if (p + 1 > d.size()) { ok = false; return 0; }
        return (unsigned char)d[p++];
    }
    string S()
    {
        if (p + 2 > d.size()) { ok = false; return string(); }
        int n = (unsigned char)d[p] | ((unsigned char)d[p + 1] << 8);
        p += 2;
        if (p + n > d.size()) { ok = false; return string(); }
        string s = d.substr(p, n);
        p += n;
        return s;
    }
};
static void Map(const string& inDir)
{
    bool found;
    Rd r;
    r.d = ReadFile(inDir + "cachemap.dat", &found);
    if (!found) {
        Log("map " + inDir + " none");
        return;
    }
    long long version = r.I(), count = r.I();
    Log("map " + inDir + " " + N((long long)r.d.size()) + " bytes version " + N(version) + " count " + N(count));
    for (long long i = 0; i < count && r.ok; i++) {
        long long id = r.I();
        int valid = r.B();
        long long expire = r.I(), last = r.I();
        string s[4];
        for (int k = 0; k < 4; k++) s[k] = r.S();
        if (!r.ok) break;
        Log("  id " + N(id) + " valid " + N(valid) + " expires " + Rel(expire) + " used " + Rel(last) + " '" + s[0] + "' '"
            + s[1] + "' '" + s[2] + "' " + Hex(s[3]));
    }
    Log("  end " + N((long long)r.p) + (r.ok ? "" : " short"));
}
struct Wr {
    string d;
    void I(long long v) { int32_t x = (int32_t)v; d.append(reinterpret_cast<const char*>(&x), 4); }
    void B(int v) { d += (char)v; }
    void S(const string& s) { B((int)s.size() & 0xff); B(((int)s.size() >> 8) & 0xff); d += s; }
    void E(long long id, int valid, long long expire, long long last, const string& remote, const string& file,
           const string& type, const string& tag)
    {
        I(id); B(valid); I(expire); I(last); S(remote); S(file); S(type); S(tag);
    }
};

static const long long kFuture = 0x7f000000;

int main(int argc, char** argv)
{
    std::error_code ec;
    sCwd = Slashes(fs::current_path(ec).string());
    if (sCwd.empty() || sCwd[sCwd.size() - 1] != '/') sCwd += '/';
    const string D = "httpcache_files/";
    fs::remove_all(D, ec);
    fs::create_directory(D, ec);
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(D.c_str());
        aCore.Startup();
        RustServerOpPtr anOp = RustServerOp::NewHttpGet("host.example", "/index.html");
        sOp = anOp.get();

        // ---- a cache in a new directory (no separator at the end)
        void* A = NewCache(D + "cacheA", false);
        Log("cache A " + string(fs::is_directory(D + "cacheA", ec) ? "dir made" : "no dir"));
        ListDir(D + "cacheA");
        void* e1 = Entry(A, "http://host.example/news/motd.txt");
        Show("new", e1);
        void* e1b = Entry(A, "http://host.example/news/motd.txt");
        Log(string("same entry ") + N(e1b == e1));
        Release(e1b);
        void* e2 = Entry(A, "http://host.example/img/logo.png");
        void* e3 = Entry(A, "noslash");
        void* e4 = Entry(A, "http://host.example/dir\\back.bmp");
        void* e5 = Entry(A, "http://host.example/a.b/c");
        void* e6 = Entry(A, "");
        void* e7 = Entry(A, "HTTP://HOST.EXAMPLE/news/motd.txt");
        void* all[] = {e2, e3, e4, e5, e6, e7};
        for (void* e : all) Show("new", e);
        Set(e1, 0, 1);
        Set(e1, 1, kFuture);
        Set(e1, 2, 0, "etag-1");
        Show("set", e1);
        Set(e2, 0, 1);
        Set(e2, 1, 5);
        Set(e2, 2, 0, string("\0\xff\"q\"", 5));
        Set(e3, 0, 7);
        Set(e3, 0, 0);
        Set(e4, 0, 1);
        Set(e4, 1, -1);
        Set(e5, 0, 1);
        Set(e5, 2, 0, "");
        Set(e7, 0, 1);
        Set(e7, 1, 0x100000000LL + 3);
        Show("set", e2);
        Show("set", e3);
        Show("set", e4);
        Show("set", e5);
        Show("set", e7);
        void* e2b = Entry(A, "http://host.example/img/logo.png");
        Log(string("same entry ") + N(e2b == e2));
        Show("again", e2b);
        Release(e2); // the second reference
        void* entries[] = {e1, e2, e3, e4, e5, e6, e7};
        for (void* e : entries) MakeFile(LocalPath(e), "cached " + Name(e));
        string e3Path = LocalPath(e3), e6Path = LocalPath(e6);
        MakeFile(D + "cacheA/other.txt", "not an entry");
        ListDir(D + "cacheA");
        Answer u = Call(EE_WON_HOST_CACHE_UPDATE, A, e1);
        Log("update num " + N(u.mCall.out_num) + " bytes " + (u.mHasBytes ? Hex(u.mBytes) : string("none")));
        AddRef(A);
        Release(A);
        Map(D + "cacheA/");
        // the cache released with entries still held: its index written then, the entries still answer
        Release(e1);
        Release(e3);
        Release(A);
        ListDir(D + "cacheA");
        Map(D + "cacheA/");
        Show("after", e2);
        Show("after", e4);
        Set(e2, 2, 0, "late");
        Show("after", e2);
        for (void* e : {e2, e4, e5, e6, e7}) Release(e);

        // ---- the same directory again, with a separator: the index read back
        void* A2 = NewCache(D + "cacheA/", false);
        void* f1 = Entry(A2, "http://host.example/news/motd.txt");
        Show("read", f1);
        void* f2 = Entry(A2, "http://host.example/img/logo.png");
        Show("read", f2);
        void* f4 = Entry(A2, "http://host.example/dir\\back.bmp");
        Show("read", f4);
        MakeFile(e3Path, "stale file at a dropped entry's name");
        MakeFile(e6Path, "stale file at a dropped entry's name");
        ListDir(D + "cacheA");
        void* f3 = Entry(A2, "noslash");
        Show("fresh", f3);
        void* f8 = Entry(A2, "http://other.example/x.jpeg");
        Show("fresh", f8);
        ListDir(D + "cacheA");
        for (void* e : {f1, f2, f3, f4, f8}) Release(e);
        Release(A2);
        Map(D + "cacheA/");

        // ---- a hand-written index: an old entry, an invalid one, two entries for one location, ids out of order
        fs::create_directory(D + "cacheC", ec);
        long long now = (long long)time(NULL);
        Wr w;
        w.I(1);
        w.I(6);
        w.E(9, 1, kFuture, now - 20, "http://c/nine.txt", "9.txt", "text", "t9");
        w.E(3, 1, kFuture, 1000, "http://c/old.txt", "old.txt", "", "");
        w.E(4, 0, kFuture, now, "http://c/invalid.txt", "invalid.txt", "", "");
        w.E(5, 1, 0, now - 14 * 86400 + 60, "http://c/dup", "5a.dat", "", "first");
        w.E(6, 1, 0, now - 14 * 86400 - 60, "http://c/dup", "6b.dat", "", "second");
        w.E(1, 1, 7, now, "http://c/one", "../escape.dat", "x", "");
        MakeFile(D + "cacheC/cachemap.dat", w.d);
        const char* files[] = {"9.txt", "old.txt", "invalid.txt", "5a.dat", "6b.dat", "2.dat"};
        for (const char* f : files) MakeFile(D + "cacheC/" + f, f);
        MakeFile(D + "escape.dat", "outside");
        ListDir(D + "cacheC");
        void* C = NewCache(D + "cacheC", false);
        void* g1 = Entry(C, "http://c/dup");
        Show("read", g1);
        void* g2 = Entry(C, "http://c/one");
        Show("read", g2);
        void* g3 = Entry(C, "http://c/new.txt");
        Show("fresh", g3);
        void* g4 = Entry(C, "http://c/new2.txt");
        Show("fresh", g4);
        Set(g3, 0, 1);
        Set(g4, 0, 1);
        for (void* e : {g1, g2, g3, g4}) Release(e);
        Release(C);
        ListDir(D + "cacheC");
        ListDir(D);
        Map(D + "cacheC/");
        void* C2 = NewCache(D + "cacheC", false);
        void* h1 = Entry(C2, "http://c/nine.txt");
        Show("read", h1);
        void* h2 = Entry(C2, "http://c/later.txt");
        Show("fresh", h2);
        Release(h1);
        Release(h2);
        Release(C2);
        Map(D + "cacheC/");

        // ---- another version, a cut index
        fs::create_directory(D + "cacheV", ec);
        Wr v;
        v.I(2);
        v.I(1);
        v.E(1, 1, kFuture, now, "http://v/a", "a.dat", "", "");
        MakeFile(D + "cacheV/cachemap.dat", v.d);
        void* V = NewCache(D + "cacheV", false);
        void* k1 = Entry(V, "http://v/a");
        Show("other version", k1);
        Release(k1);
        Release(V);
        Map(D + "cacheV/");
        Wr t;
        t.I(1);
        t.I(3);
        t.E(2, 1, kFuture, now, "http://t/a", "a.dat", "", "");
        t.E(5, 1, kFuture, now, "http://t/b", "b.dat", "", "");
        string cut = t.d.substr(0, t.d.size() - 3);
        fs::create_directory(D + "cacheT", ec);
        MakeFile(D + "cacheT/cachemap.dat", cut);
        void* T = NewCache(D + "cacheT", false);
        void* m1 = Entry(T, "http://t/a");
        Show("cut", m1);
        void* m2 = Entry(T, "http://t/b");
        Show("cut", m2);
        void* m3 = Entry(T, "http://t/c");
        Show("cut", m3);
        for (void* e : {m1, m2, m3}) Release(e);
        Release(T);
        Map(D + "cacheT/");

        // ---- a full path; a directory whose parent is missing (not made: no index written)
        void* B = NewCache(D + "cacheB", true);
        void* b1 = Entry(B, "http://b/x.txt");
        Show("full", b1);
        Release(b1);
        Release(B);
        ListDir(D + "cacheB");
        void* B2 = NewCache(D + "nothere/cacheB2/", true);
        Log("cache B2 " + string(fs::is_directory(D + "nothere/cacheB2", ec) ? "dir made" : "no dir"));
        void* b2 = Entry(B2, "http://b/y.txt");
        Show("no dir", b2);
        Release(b2);
        Release(B2);
        ListDir(D + "nothere");

        // ---- the global cache, in the WON directory
        void* G = GlobalCache();
        Log(string("global ") + (G != NULL ? "set" : "null") + " again same " + N(GlobalCache() == G));
        ListDir(D);
        void* n1 = Entry(G, "http://host.example/promo/image.bmp");
        Set(n1, 0, 1);
        Show("global", n1);
        // the op holds the cache it is given (the game's ParseCrossPromotion), released with the op
        ee_won_hostop_set(sOp->GetRustOp(), EE_WON_HTTPGET_CACHE, 0, NULL, 0, NULL, G);
        Release(n1);
        ResetGlobalCache();
        Map(D + "_wonHTTPCache/");
        anOp = NULL;
        sOp = NULL;
        Map(D + "_wonHTTPCache/");
        void* G2 = GlobalCache();
        RustServerOpPtr anOp2 = RustServerOp::NewHttpGet("host.example", "/index.html");
        sOp = anOp2.get();
        void* n2 = Entry(G2, "http://host.example/promo/image.bmp");
        Show("global again", n2);
        Release(n2);
        ResetGlobalCache();
        Map(D + "_wonHTTPCache/");
        anOp2 = NULL;
        sOp = NULL;
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
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aPath += "../../rust/crates/ee_won/src/golden_httpcache.txt";
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
    printf("won_httpcache_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
