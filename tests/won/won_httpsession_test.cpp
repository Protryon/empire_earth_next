// HTTPSession and HTTPConnection over real sockets on 127.0.0.1 (net round 26): HTTP get ops added to a session go to
// their host's connection, which prepares them (its socket, no close, no range resume), sends and receives on one
// socket, reuses it for the next get (keep-alive), drops it when the server closes, and kills its queued and running
// ops. A server stand-in on the test's thread reads each request's lines and answers. One line per event, compared
// with rust/crates/ee_won_net/src/golden_httpsession.txt (recorded over HTTPEngine.cpp's hooks before they were answered
// in Rust: ee_won httpcpp.rs). Waits are on events (pumped until a completion ran), with time limits. `--print`
// prints the lines instead, `--trace` each line as it is logged. The exit code is the number of failures.
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
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "platform/PlatTime.h"
#include "rust/ee_won.h"
#include "won_blocking.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static bool sTrace;
static string sPortText;
static void Log(string inLine)
{
    // the listener's port varies from run to run
    for (size_t at; !sPortText.empty() && (at = inLine.find(sPortText)) != string::npos;)
        inLine.replace(at, sPortText.size(), "PORT");
    sLines.push_back(inLine);
    if (sTrace) fprintf(stderr, "%s\n", inLine.c_str());
}
static string N(long long inValue) { return std::to_string(inValue); }
static string Text(const string& inBytes)
{
    string s;
    for (size_t i = 0; i < inBytes.size(); i++) {
        char c = inBytes[i];
        if (c == '\r') s += "\\r";
        else if (c == '\n') s += "\\n";
        else s += c;
    }
    return s;
}

static WONAPICoreEx* sCore;
static std::vector<string> sDone;
class Done : public CompletionBase<AsyncOpPtr> {
    string mName;
public:
    explicit Done(const string& inName) : mName(inName) {}
    virtual void Complete(AsyncOpPtr inOp) { sDone.push_back(mName + " " + WONStatusToString(inOp->GetStatus())); }
};
// pumps until a completion ran (5 s at most), then logs the completions
static void WaitDone(size_t inCount)
{
    for (int i = 0; i < 500 && sDone.size() < inCount; i++) sCore->Pump(10);
    for (size_t i = 0; i < sDone.size(); i++) Log("completed " + sDone[i]);
    sDone.clear();
}
static void Results(const char* inName, RustServerOp* inOp)
{
    Log(string(inName) + ": status " + WONStatusToString(inOp->GetStatus()) + " http " +
        N(inOp->GetNum(EE_WON_HTTPGET_STATUS)) + " length " + N(inOp->GetNum(EE_WON_HTTPGET_CONTENT_LENGTH)) +
        " content \"" + Text(inOp->GetBytes(EE_WON_HTTPGET_CONTENT)) + "\" close " + N(inOp->GetNum(EE_WON_HTTPGET_CLOSE)) +
        " resume " + N(inOp->GetNum(EE_WON_HTTPGET_RANGE_RESUME)) + " send only " +
        N(inOp->GetNum(EE_WON_HTTPGET_ONLY_SEND)) + " recv only " + N(inOp->GetNum(EE_WON_HTTPGET_ONLY_RECV)) +
        " socket " + N(inOp->GetHttpSocket() != NULL));
}
static ByteBufferPtr Bytes(const string& inText)
{
    return new ByteBuffer(inText.data(), (unsigned long)inText.size(), false);
}

// the server stand-in: an accepted connection, each request's lines read and logged, its answer sent
static SmartPtr<AsyncSocket> sListener;
static SmartPtr<AsyncSocket> sPeer;
// the client's steps run in the pumps (the ops' completions), so the server side waits in short slices between them
static void Accept(const char* inWhy)
{
    WONStatus aStatus = WS_TimedOut;
    for (int i = 0; i < 500 && aStatus != WS_Success; i++) {
        sCore->Pump(10);
        sListener->WaitForAccept(10);
        aStatus = sListener->Accept(sPeer);
    }
    Log(string("server accept ") + inWhy + " " + WONStatusToString(aStatus));
}
static void ReadRequest(const char* inWhy)
{
    for (int i = 0, tries = 0; i < 20;) {
        ByteBufferPtr aLine;
        sCore->Pump(10);
        sPeer->WaitForRead(10);
        WONStatus aStatus = WonBlocking::RecvCRMsg(sPeer.get(), aLine, 20);
        if (aStatus == WS_TimedOut && ++tries < 500) continue;
        if (aStatus != WS_Success) {
            Log(string("server ") + inWhy + " read " + WONStatusToString(aStatus));
            return;
        }
        i++;
        string aText(aLine->data(), aLine->length());
        while (!aText.empty() && (unsigned char)aText[aText.size() - 1] <= 32)
            aText.resize(aText.size() - 1);
        Log(string("server ") + inWhy + " line \"" + Text(aText) + "\"");
        if (aText.empty() || aText == "\r" || aText == "\r\n") return;
    }
}
static void Answer(const char* inWhy, const string& inReply)
{
    WONStatus aStatus = WonBlocking::SendBytes(sPeer.get(), Bytes(inReply).get(), 5000);
    Log(string("server ") + inWhy + " answered " + WONStatusToString(aStatus));
}

int main(int argc, char** argv)
{
    sTrace = argc > 1 && strcmp(argv[argc - 1], "--trace") == 0;
    string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    string aDir = (aSlash == string::npos ? string(".") : aExe.substr(0, aSlash)) + "/httpsession_files/";
    PlatCreateDirectory(aDir.c_str());
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(aDir.c_str());
        aCore.Startup();
        sCore = &aCore;
        sListener = new BlockingSocket(AsyncSocket::TCP);
        sListener->Fn2B0D90(0, true);
        sListener->Listen(5);
        sPortText = N(sListener->GetLocalPort());
        string aHost = "127.0.0.1:" + sPortText;

        SmartPtr<HTTPSession> aSession = new HTTPSession;
        Log("session ops " + N(aSession->GetNumOps()));

        // a get on a new connection: prepared, sent, its keep-alive reply read
        SmartPtr<RustServerOp> aOne = RustServerOp::NewHttpGet(aHost, "/one");
        aOne->SetCompletion(new Done("one"));
        aSession->AddOp(aOne);
        Log("one added: session ops " + N(aSession->GetNumOps()));
        Accept("one");
        ReadRequest("one");
        Answer("one", "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello");
        WaitDone(1);
        Results("one", aOne);
        Log("session ops " + N(aSession->GetNumOps()));

        // the next get to the host reuses the connection's socket
        SmartPtr<RustServerOp> aTwo = RustServerOp::NewHttpGet(aHost, "/two");
        aTwo->SetCompletion(new Done("two"));
        aSession->AddOp(aTwo);
        ReadRequest("two");
        Answer("two", "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nbye");
        WaitDone(1);
        Results("two", aTwo);
        sPeer->Close();
        for (int i = 0; i < 20; i++) aCore.Pump(10);

        // the server closed: a new socket for the next get; a not found
        SmartPtr<RustServerOp> aThree = RustServerOp::NewHttpGet(aHost, "/three");
        aThree->SetCompletion(new Done("three"));
        aSession->AddOp(aThree);
        Accept("three");
        ReadRequest("three");
        Answer("three", "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
        WaitDone(1);
        Results("three", aThree);

        // two gets queued while the server is silent, then the session killed
        SmartPtr<RustServerOp> aFour = RustServerOp::NewHttpGet(aHost, "/four");
        aFour->SetCompletion(new Done("four"));
        SmartPtr<RustServerOp> aFive = RustServerOp::NewHttpGet(aHost, "/five");
        aFive->SetCompletion(new Done("five"));
        aSession->AddOp(aFour);
        aSession->AddOp(aFive);
        ReadRequest("four");
        Log("queued: session ops " + N(aSession->GetNumOps()));
        aSession->KillAll();
        WaitDone(2);
        Results("four", aFour);
        Results("five", aFive);
        Log("killed: session ops " + N(aSession->GetNumOps()));

        sPeer->Close();
        sPeer = NULL;
        aSession = NULL;
        aOne = aTwo = aThree = aFour = aFive = NULL;
        sListener->Close();
        sListener = NULL;
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
    string aGoldenPath = __FILE__;
    aSlash = aGoldenPath.find_last_of("/\\");
    aGoldenPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aGoldenPath += "../../rust/crates/ee_won_net/src/golden_httpsession.txt";
    FILE* aGolden = fopen(aGoldenPath.c_str(), "rb");
    if (aGolden == NULL) {
        printf("FAIL: cannot open %s\n", aGoldenPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aGolden)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aGolden);
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
    printf("won_httpsession_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
