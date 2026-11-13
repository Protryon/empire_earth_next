// WONRouting's RoutingGetUserStatusOp over 127.0.0.1, one line per event, compared with
// rust/crates/ee_won/src/golden_userstatus.txt (recorded over the C++ class before its move to Rust): a UDPManager
// bound on the loopback, a routing server stand-in (a datagram socket) that logs each request it receives and sends
// the scripted reply; the queried ids added one by one (a duplicate kept once) or as a whole map (LocateFriends),
// the statuses read in the map's order, a reply with the wrong header, a count that does not match, a reply cut
// short, no ids, no reply. `--print` prints the lines instead. The exit code is the number of failures.
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

using namespace WONAPI;

static std::vector<std::string> sLines;
static void Log(const std::string& inLine) { sLines.push_back(inLine); }
static std::string N(long long inValue) { return std::to_string(inValue); }
static std::string St(WONStatus inStatus) { return WONStatusToString(inStatus); }
static std::string Hex(const ByteBuffer* inBuffer)
{
    if (inBuffer == NULL)
        return "(null)";
    static const char kDigits[] = "0123456789abcdef";
    std::string s = N(inBuffer->length()) + ":";
    for (unsigned long i = 0; i < inBuffer->length() && i < 64; i++) {
        unsigned char c = static_cast<unsigned char>(inBuffer->data()[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}
static ByteBufferPtr Bytes(const std::string& inData) { return new ByteBuffer(inData.data(), (unsigned long)inData.size(), false); }

struct Done : public CompletionBase<AsyncOpPtr> {
    int mCount = 0;
    virtual void Complete(AsyncOpPtr) { this->mCount++; }
};

static WONAPICoreEx* sCore;
static void PumpFor(Done* inDone, int inCount)
{
    uint32_t aStart = WONTickCount();
    while (inDone->mCount < inCount && WONTickCount() - aStart < 8000)
        sCore->Pump(10);
}

typedef std::map<unsigned long, unsigned char> StatusMap;
static std::string Show(const StatusMap& inMap)
{
    std::string s = "{";
    for (StatusMap::const_iterator i = inMap.begin(); i != inMap.end(); ++i)
        s += (i == inMap.begin() ? "" : " ") + N((long long)i->first) + ":" + N(i->second);
    return s + "}";
}

static SmartPtr<UDPManager> sManager;
static SmartPtr<BlockingSocket> sServer;
static IPAddr sServerAddr;

// one query: the op made, its ids added (or its whole map set), run; the scripted replies answer its request
static void Query(const char* inName, const std::vector<unsigned long>& inIds, bool inWholeMap,
                  const std::vector<std::string>& inReplies, bool inFast = false)
{
    Log(std::string("== ") + inName);
    SmartPtr<RoutingGetUserStatusOp> anOp = new RoutingGetUserStatusOp(sServerAddr, sManager);
    if (inWholeMap) {
        StatusMap m;
        for (size_t i = 0; i < inIds.size(); i++)
            m[inIds[i]] = 0;
#ifdef EE_WON_USER_STATUS_RUST
        anOp->SetFriendStatusMap(m);
#else
        anOp->mFriendStatusMap = m;
#endif
    } else {
        for (size_t i = 0; i < inIds.size(); i++)
            anOp->AddWONUserId(inIds[i]);
    }
    Log("before " + Show(anOp->GetFriendStatusMap()) + " retransmits " + N(anOp->GetNumRetransmits()) + " timeout "
        + N((long long)anOp->GetMsgTimeout()));
    if (inFast) {
        anOp->SetNumRetransmits(0);
        anOp->SetMsgTimeout(50);
    }
    SmartPtr<Done> aDone = new Done;
    anOp->SetCompletion(aDone);
    anOp->RunAsync(10000);
    sCore->Pump(20); // RunAsync only queues: the pump starts the op, which sends
    // one request; every scripted reply answers it, in order (a retransmit needs the pump: none is waited for)
    ByteBufferPtr aGot;
    IPAddr aFrom;
    WONStatus aStatus = sServer->RecvBytesFrom(aGot, 512, aFrom, 5000);
    Log("server got " + St(aStatus) + " " + Hex(aGot.get()));
    for (size_t r = 0; aStatus == WS_Success && r < inReplies.size(); r++) {
        sServer->SendBytesTo(Bytes(inReplies[r]), aFrom, 5000);
        if (r + 1 < inReplies.size()) { // a reply the op refuses: still waiting after it was read
            uint32_t aStart = WONTickCount();
            while (WONTickCount() - aStart < 200)
                sCore->Pump(10);
            Log("after reply " + N((long long)r) + " done " + N(aDone->mCount) + " status " + St(anOp->GetStatus()));
        }
    }
    PumpFor(aDone, 1);
    sCore->Pump(20);
    Log("done " + N(aDone->mCount) + " status " + St(anOp->GetStatus()) + " map " + Show(anOp->GetFriendStatusMap())
        + " outgoing " + Hex(anOp->GetOutgoing()));
}

static std::string R(std::initializer_list<int> inBytes)
{
    std::string s;
    for (int b : inBytes)
        s += (char)b;
    return s;
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_userstatus.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");
        aCore.Startup();
        sCore = &aCore;
        sManager = new UDPManager;
        Log("bind " + St(sManager->Bind(0, false)));
        sServer = new BlockingSocket(1);
        sServer->Bind(IPAddr("127.0.0.1", 0), false);
        sServerAddr = IPAddr("127.0.0.1", sServer->GetLocalPort());
        Query("three ids, one twice", {5, 2, 900000, 2}, false, {R({3, 7, 73, 3, 1, 0, 4})});
        Query("the whole map", {10, 11}, true, {R({3, 7, 73, 2, 9, 8})});
        Query("wrong header, then right", {7}, false, {R({3, 7, 74, 1, 5}), R({3, 7, 73, 1, 6})});
        Query("count mismatch", {1, 2, 3}, false, {R({3, 7, 73, 2, 1, 1})});
        Query("cut short", {1, 2, 3}, false, {R({3, 7, 73, 3, 7})});
        Query("no ids", {}, false, {R({3, 7, 73, 0})});
        Query("large ids", {0xffffffffUL, 0x80000000UL, 1}, true, {R({3, 7, 73, 3, 1, 2, 3, 99})});
        Query("no reply", {4}, false, {}, true);
        sServer = NULL;
        sManager->Close();
        sManager = NULL;
        aCore.Pump(50);
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
    printf("won_userstatus_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
