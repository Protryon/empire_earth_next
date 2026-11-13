// RustServerOp's host calls that need no C++ object (ServerOp.cpp's HostCall: the clock, the CRT's random numbers,
// an address resolved, the op's file and the files it measures, the web proxy file), one line per call, compared
// with rust/crates/ee_won/src/golden_serverop.txt (recorded over ServerOp.cpp's HostCall before these calls moved
// into the Rust op base). Every call goes through the op's own call path (ee_won_hostop_call), so the same lines are
// the C++ side's answers before the move and the Rust op's after it. Files are made fresh in a directory next to
// the program. `--print` prints the lines instead. The exit code is the number of failures.
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
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "platform/PlatTime.h"

using namespace WONAPI;

static std::vector<std::string> sLines;
static void Log(const std::string& inLine) { sLines.push_back(inLine); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string Hex(const std::string& inBytes)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N((long long)inBytes.size()) + ":";
    for (size_t i = 0; i < inBytes.size() && i < 40; i++) {
        unsigned char c = static_cast<unsigned char>(inBytes[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}

struct Answer {
    bool mHasBytes = false;
    std::string mBytes;
    EeWonHostCall mCall;
};
static void Sink(void* inContext, const uint8_t* inData, size_t inLength)
{
    Answer* a = static_cast<Answer*>(inContext);
    a->mHasBytes = true;
    a->mBytes.assign(reinterpret_cast<const char*>(inData), inLength);
}
static RustServerOp* sOp;
// one call through the op's own path
static Answer Call(int inCode, long long inNum0 = 0, long long inNum1 = 0, const std::string& inData = std::string())
{
    Answer a;
    memset(&a.mCall, 0, sizeof(a.mCall));
    a.mCall.code = inCode;
    a.mCall.num[0] = inNum0;
    a.mCall.num[1] = inNum1;
    a.mCall.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    a.mCall.len = inData.size();
    a.mCall.out_sink = Sink;
    a.mCall.out_context = &a;
    ee_won_hostop_call(sOp->GetRustOp(), &a.mCall);
    return a;
}
static std::string Show(const Answer& a)
{
    std::string s = "num " + N(a.mCall.out_num) + " obj " + (a.mCall.out_obj != NULL ? "set" : "null");
    static const char kDigits[] = "0123456789abcdef";
    s += " addr ";
    for (int i = 0; i < 6; i++) {
        s += kDigits[a.mCall.out_addr[i] >> 4];
        s += kDigits[a.mCall.out_addr[i] & 15];
    }
    s += " bytes " + (a.mHasBytes ? Hex(a.mBytes) : std::string("none"));
    return s;
}

static std::string sDir; // the files' directory, with its separator
static void MakeFile(const std::string& inName, const std::string& inBytes)
{
    FILE* f = fopen((sDir + inName).c_str(), "wb");
    if (f == NULL) {
        printf("FAIL: cannot write %s\n", (sDir + inName).c_str());
        exit(1);
    }
    fwrite(inBytes.data(), 1, inBytes.size(), f);
    fclose(f);
}

static void Files()
{
    std::string aBin;
    for (int i = 0; i < 200; i++)
        aBin += (char)(i % 3 == 0 ? '\r' : i % 3 == 1 ? '\n' : (char)i);
    MakeFile("a.bin", aBin);
    MakeFile("empty.txt", "");
    MakeFile("text.txt", "Hello\r\nWorld\n");
    MakeFile("crlf.txt", "\r\nX");
    MakeFile("zero.bin", std::string("\0abc", 4));
    PlatCreateDirectory((sDir + "sub").c_str());
    const char* kNames[] = {"a.bin", "empty.txt", "text.txt", "crlf.txt", "zero.bin", "sub", "missing.txt", ""};
    for (const char* n : kNames) {
        std::string p = sDir + n;
        // a directory's size is the system's (0 on Windows, its entries' on POSIX): only whether it has one
        Answer z = Call(EE_WON_HOST_FILE_SIZE, 0, 0, p);
        bool aDir = strcmp(n, "sub") == 0 || n[0] == 0;
        Log(std::string("size ") + n + " " + (aDir ? std::string(z.mCall.out_num >= 0 ? "dir" : "none") : Show(z)));
        Log(std::string("first ") + n + " " + Show(Call(EE_WON_HOST_FILE_FIRST_CHAR, 0, 0, p)));
        for (int k = 0; k < 2; k++)
            Log(std::string("stat ") + N(k) + " " + n + " " + Show(Call(EE_WON_HOST_FILE_STAT, k, 0, p)));
        Answer t = Call(EE_WON_HOST_FILE_STAT, 2, 0, p);
        long long aNow = (long long)time(NULL);
        long long d = t.mCall.out_num - aNow;
        Log(std::string("stat 2 ") + n + " " + (t.mCall.out_num == 0 ? std::string("0") : (d > -86400 && d < 86400) ? "recent" : "old")
            + " bytes " + (t.mHasBytes ? "set" : "none"));
    }
    // the op's file: reads, a reopen closing the first, reads with none open
    Log("read none " + Show(Call(EE_WON_HOST_FILE_READ, 4)));
    Log("open a.bin " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "a.bin")));
    Log("read 7 " + Show(Call(EE_WON_HOST_FILE_READ, 7)));
    Log("read 0 " + Show(Call(EE_WON_HOST_FILE_READ, 0)));
    Log("read 500 " + Show(Call(EE_WON_HOST_FILE_READ, 500)));
    Log("read 10 " + Show(Call(EE_WON_HOST_FILE_READ, 10)));
    Log("open text.txt " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "text.txt")));
    Log("read 100 " + Show(Call(EE_WON_HOST_FILE_READ, 100)));
    Log("close " + Show(Call(EE_WON_HOST_FILE_CLOSE)));
    Log("read 4 " + Show(Call(EE_WON_HOST_FILE_READ, 4)));
    Log("close again " + Show(Call(EE_WON_HOST_FILE_CLOSE)));
    Log("open missing " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "missing.txt")));
    Log("read 4 " + Show(Call(EE_WON_HOST_FILE_READ, 4)));
    Log("open dir " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "sub")));
    Log("open zero.bin " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "zero.bin")));
    Log("read 2 " + Show(Call(EE_WON_HOST_FILE_READ, 2)));
    Log("open empty " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "empty.txt")));
    Log("read 3 " + Show(Call(EE_WON_HOST_FILE_READ, 3)));
    Log("open a.bin " + Show(Call(EE_WON_HOST_FILE_OPEN, 0, 0, sDir + "a.bin")));
    Log("read 3 " + Show(Call(EE_WON_HOST_FILE_READ, 3))); // left open: the op's destructor closes it
}

static void Clock()
{
    Answer t = Call(EE_WON_HOST_TICK);
    long long d = t.mCall.out_num - (long long)(uint32_t)PlatTimeMilliseconds64();
    Log(std::string("tick ") + (d > -2000 && d <= 0 ? "now" : "off " + N(d)) + " bytes " + (t.mHasBytes ? "set" : "none"));
    Answer s = Call(EE_WON_HOST_TIME);
    long long e = s.mCall.out_num - (long long)time(NULL);
    Log(std::string("time ") + (e >= -2 && e <= 0 ? "now" : "off " + N(e)) + " bytes " + (s.mHasBytes ? "set" : "none"));
    srand(4242);
    for (int i = 0; i < 4; i++)
        Log("rand " + Show(Call(EE_WON_HOST_RAND)));
    srand(4242);
    Log("rand again " + Show(Call(EE_WON_HOST_RAND)));
    Answer r = Call(EE_WON_HOST_SRAND);
    Log("srand num " + N(r.mCall.out_num) + " bytes " + (r.mHasBytes ? "set" : "none"));
    int aExpect;
    {
        srand((unsigned)time(NULL));
        aExpect = rand();
    }
    Call(EE_WON_HOST_SRAND);
    Answer q = Call(EE_WON_HOST_RAND);
    // the seed is the time: the same as a seed of time(NULL) unless the second turned in between
    Log(std::string("rand after srand ") + (q.mCall.out_num == aExpect ? "seeded" : "?") + " "
        + (q.mCall.out_num >= 0 && q.mCall.out_num <= RAND_MAX ? "in range" : "out of range"));
}

static void Resolve()
{
    struct { const char* mText; int mPort; } kCases[] = {
        // (an empty host is the local host's address: not here)
        {"127.0.0.1:15101", 80}, {"10.1.2.3", 0x1234}, {"1.2.3.4:", 7}, {"1.2.3.4:abc", 8},
        {"255.255.255.255:65535", 1}, {"localhost:99", 2}, {"0.0.0.0", 0},
    };
    for (auto& c : kCases)
        Log(std::string("resolve '") + c.mText + "' " + N(c.mPort) + " " + Show(Call(EE_WON_HOST_RESOLVE, c.mPort, 0, c.mText)));
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_serverop.txt";
}

int main(int argc, char** argv)
{
    std::string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    sDir = (aSlash == std::string::npos ? std::string(".") : aExe.substr(0, aSlash)) + "/serverop_files/";
    PlatCreateDirectory(sDir.c_str());
    // the web proxy file, read once per process when the first HTTP get op is made
    MakeFile("_wonwebproxy.txt", "proxy.example:8080\r\nsecond line\n");
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(sDir.c_str());
        aCore.Startup();
        IPAddr anAddr;
        anAddr.SetWithDefaultPortNoDNS("127.0.0.1:1", 1);
        SmartPtr<RustServerOp> anOp = RustServerOp::NewGroupListReq(anAddr, 0);
        sOp = anOp.get();
        Log("proxy before " + Show(Call(EE_WON_HOST_HTTP_PROXY)));
        SmartPtr<RustServerOp> aGet = RustServerOp::NewHttpGet("example.com", "/a");
        Log("proxy after " + Show(Call(EE_WON_HOST_HTTP_PROXY)));
        MakeFile("_wonwebproxy.txt", "changed\n");
        SmartPtr<RustServerOp> aGet2 = RustServerOp::NewHttpGet("http://example.com/b");
        Log("proxy once " + Show(Call(EE_WON_HOST_HTTP_PROXY)));
        Files();
        Clock();
        Resolve();
        aGet = NULL;
        aGet2 = NULL;
        sOp = NULL;
        anOp = NULL;
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
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
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
    printf("won_serverop_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
