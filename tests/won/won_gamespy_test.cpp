// GameSpySupport and the GameSpy Query & Reporting SDK under it over 127.0.0.1, one line per event, compared with
// rust/crates/ee_won/src/golden_gamespy.txt (recorded over GameSpySupport.cpp, gqueryreporting.c and nonport.c
// before their move into ee_won gamespy.rs, net round 28). A master server stand-in on 127.0.0.1:27900 receives the
// heartbeats; a client socket sends the queries and reads the replies: the key-value maps, every query type, the
// echo, the validation, several queries in one datagram and two datagrams in one pump, a reply split into packets,
// a key too big to send, the map dump's buffer limit, a secure value too long, the auto pump, a second instance on
// the next port, an instance whose start-up failed pumping the last one started (the SDK's current record), the
// shutdown's state change. The query port is chosen by the system: lines name it "A" (and "A+1"). `--print` prints
// the lines instead. The exit code is the number of failures.
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
#include "ee/wonapi/WONGameSpySupport.h"
#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"

using namespace WONAPI;

#ifndef EE_WON_GAMESPY_RUST
extern "C" char qr_hostname[64]; // the SDK's master server host (gqueryreporting.c)
#endif
static void SetMasterHost(const char* inHost)
{
#ifdef EE_WON_GAMESPY_RUST
    GameSpySupport::SetMasterHost(inHost);
#else
    strcpy(qr_hostname, inHost);
#endif
}

static std::vector<std::string> sLines;
static void Log(const std::string& inLine)
{
    sLines.push_back(inLine);
    if (getenv("WON_GAMESPY_TRACE") != NULL)
        fprintf(stderr, "%s\n", inLine.c_str());
}
static std::string N(long long inValue) { return std::to_string(inValue); }

static int sPortA = -1; // the first instance's query port
// the bytes as text: the port numbers named relative to A, long texts as their length, hash, head and tail
static std::string Port(int inPort)
{
    if (sPortA < 0 || inPort < sPortA || inPort > sPortA + 5)
        return N(inPort);
    return inPort == sPortA ? "A" : "A+" + N(inPort - sPortA);
}
static std::string Text(const std::string& inBytes)
{
    std::string s;
    for (size_t i = 0; i < inBytes.size(); i++) {
        unsigned char c = (unsigned char)inBytes[i];
        if (c >= 32 && c < 127)
            s += (char)c;
        else {
            char h[8];
            snprintf(h, sizeof(h), "<%02x>", c);
            s += h;
        }
    }
    if (s.size() <= 160)
        return s;
    unsigned int aHash = 2166136261u;
    for (size_t i = 0; i < inBytes.size(); i++)
        aHash = (aHash ^ (unsigned char)inBytes[i]) * 16777619u;
    char h[16];
    snprintf(h, sizeof(h), "%08x", aHash);
    return "len " + N((long long)inBytes.size()) + " fnv " + h + " " + s.substr(0, 60) + " ... " + s.substr(s.size() - 40);
}

static PlatSocket sMaster = PLAT_SOCKET_INVALID, sClient = PLAT_SOCKET_INVALID;
static PlatSockAddrIn Addr(uint32_t inHost, int inPort)
{
    PlatSockAddrIn a;
    memset(&a, 0, sizeof(a));
    a.mFamily = PLAT_AF_INET;
    a.mPort = (uint16_t)(((inPort & 0xff) << 8) | ((inPort >> 8) & 0xff));
    a.mAddress = inHost;
    return a;
}
static int PortOf(const PlatSockAddrIn& a) { return ((a.mPort & 0xff) << 8) | (a.mPort >> 8); }
static const uint32_t kLoopback = 0x0100007f; // 127.0.0.1, network order

// the datagrams waiting on a socket: the first within inFirstWait ms, then any within 50 ms of the last
static std::vector<std::string> Drain(PlatSocket inSocket, unsigned inFirstWait)
{
    std::vector<std::string> aGot;
    unsigned aWait = inFirstWait;
    for (;;) {
        if (PlatSocketWait(inSocket, PLAT_SOCKET_WAIT_READ, aWait) <= 0)
            break;
        char aBuf[4096];
        PlatSockAddrIn aFrom;
        int n = PlatSocketRecvFrom(inSocket, aBuf, sizeof(aBuf), &aFrom);
        if (n < 0)
            break;
        aGot.push_back(std::string(aBuf, (size_t)n));
        aWait = 50;
    }
    return aGot;
}
// a heartbeat's text with its port named
static std::string Heartbeat(const std::string& inText)
{
    const std::string kHead = "\\heartbeat\\";
    if (inText.compare(0, kHead.size(), kHead) != 0)
        return Text(inText);
    size_t anEnd = inText.find('\\', kHead.size());
    int aPort = atoi(inText.substr(kHead.size(), anEnd - kHead.size()).c_str());
    if (sPortA < 0)
        sPortA = aPort;
    return kHead + Port(aPort) + Text(inText.substr(anEnd));
}
static void LogHeartbeats(const char* inWhat, unsigned inWait)
{
    std::vector<std::string> aGot = Drain(sMaster, inWait);
    if (aGot.empty())
        Log(std::string(inWhat) + " no heartbeat");
    for (size_t i = 0; i < aGot.size(); i++)
        Log(std::string(inWhat) + " heartbeat " + Heartbeat(aGot[i]));
}
// pumps until the master hears a heartbeat (the first pumps wait for the GameSpy service lookup to finish)
static void PumpUntilHeartbeat(const char* inWhat, GameSpySupport* inPumped)
{
    for (int i = 0; i < 300; i++) {
        inPumped->Pump();
        if (PlatSocketWait(sMaster, PLAT_SOCKET_WAIT_READ, 20) > 0) {
            LogHeartbeats(inWhat, 200);
            return;
        }
    }
    Log(std::string(inWhat) + " no heartbeat after 300 pumps");
}
static void LogReplies(const std::string& inWhat, unsigned inWait)
{
    std::vector<std::string> aGot = Drain(sClient, inWait);
    if (aGot.empty())
        Log(inWhat + " no reply");
    for (size_t i = 0; i < aGot.size(); i++)
        Log(inWhat + " reply " + Text(aGot[i]));
}
static void Send(int inPort, const std::string& inQuery)
{
    PlatSockAddrIn aTo = Addr(kLoopback, inPort);
    PlatSocketSendTo(sClient, inQuery.data(), (uint32_t)inQuery.size(), &aTo);
}
// a query to the port, a pump of the instance, its replies
static void Query(GameSpySupport* inPumped, int inPort, const std::string& inQuery)
{
    Send(inPort, inQuery);
    PlatSleep(20);
    inPumped->Pump();
    LogReplies("query " + Port(inPort) + " '" + Text(inQuery) + "'", 300);
}
static std::string Key(GameSpySupport* g, GameSpyQueryType inType, const char* inKey)
{
    const char* v = g->GetKeyValue(inType, inKey);
    return v == NULL ? std::string("(null)") : "'" + Text(v) + "'";
}
static void LogState(const char* inWhat, GameSpySupport* g)
{
    Log(std::string(inWhat) + " status " + N(g->GetStatus()) + " gamespy " + N(g->GetGameSpyStatus()) + " autopump "
        + N(g->IsAutoPumping()));
}

static void Script()
{
    SmartPtr<ServerContext> aDirs = new ServerContext; // no directory server: the service lookup fails at once
    SmartPtr<GameSpySupport> a = new GameSpySupport("eetest", "1.00", "s3cr3tKEY", aDirs.get());
    LogState("new", a.get());
    Log("basic gamename " + Key(a.get(), (GameSpyQueryType)0, "gamename") + " gamever "
        + Key(a.get(), (GameSpyQueryType)0, "gamever") + " info hostname " + Key(a.get(), GameSpyQueryType_Info, "hostname"));
    a->SetKeyValue(GameSpyQueryType_Info, "hostname", "Test Host");
    a->SetKeyValue(GameSpyQueryType_Info, "mapname", "Earth");
    a->SetKeyValue(GameSpyQueryType_Info, "numplayers", "2");
    a->SetGameMode("openwaiting");
    a->SetKeyValue(GameSpyQueryType_Rules, "difficulty", "hard");
    a->SetKeyValue(GameSpyQueryType_Rules, "epoch", "5");
    a->SetKeyValue(GameSpyQueryType_Players, "player_0", "Bob");
    a->SetKeyValue(GameSpyQueryType_Players, "player_1", "Al");
    // (each call its own statement: the order of a + chain's operands is the compiler's)
    bool anErased = a->EraseKeyValue(GameSpyQueryType_Rules, "epoch");
    bool anAgain = a->EraseKeyValue(GameSpyQueryType_Rules, "epoch");
    bool aNone = a->EraseKeyValue(GameSpyQueryType_Rules, "none");
    Log("erase epoch " + N(anErased) + " again " + N(anAgain) + " none " + N(aNone));
    Log("info gamemode " + Key(a.get(), GameSpyQueryType_Info, "gamemode") + " rules epoch "
        + Key(a.get(), GameSpyQueryType_Rules, "epoch") + " players player_1 " + Key(a.get(), GameSpyQueryType_Players, "player_1"));
    a->LockKeyValues();
    a->LockKeyValues();
    a->SetKeyValue(GameSpyQueryType_Rules, "epoch", "7");
    a->UnlockKeyValues();
    a->UnlockKeyValues();

    Log("startup " + N(a->Startup(IPAddr("127.0.0.1", 0))));
    LogState("started", a.get());
    PumpUntilHeartbeat("a first", a.get());
    Log("port A " + N(sPortA >= 0));

    const char* kQueries[] = {"\\basic\\", "\\info\\", "\\rules\\", "\\players\\", "\\status\\", "\\packets\\",
                              "\\echo\\hello world\\", "\\secure\\abcdef\\", "\\basic\\\\secure\\0123456789\\",
                              "\\info\\\\echo\\x\\", "\\nothing\\", "", "\\basic", "\\status\\\\secure\\",
                              "\\players\\\\basic\\\\rules\\"};
    for (size_t i = 0; i < sizeof(kQueries) / sizeof(kQueries[0]); i++)
        Query(a.get(), sPortA, kQueries[i]);
    Query(a.get(), sPortA, std::string("\\echo\\a\0b\\\\basic\\", 15)); // the query ends at its first NUL
    // a secure value of 128 characters (validated) and of 129 (no reply at all: the final packet is not sent)
    Query(a.get(), sPortA, "\\secure\\" + std::string(128, 'v') + "\\");
    Query(a.get(), sPortA, "\\basic\\\\secure\\" + std::string(129, 'w') + "\\");
    // a query of 255 bytes, the most the SDK reads
    Query(a.get(), sPortA, "\\echo\\" + std::string(255 - 7, 'e') + "\\");
    // two datagrams in one pump
    Send(sPortA, "\\basic\\");
    Send(sPortA, "\\rules\\");
    PlatSleep(30);
    a->Pump();
    LogReplies("two queries", 300);

    // a reply split into packets: info near the buffer's size, then the status of every map
    a->SetKeyValue(GameSpyQueryType_Info, "long1", std::string(600, 'L').c_str());
    a->SetKeyValue(GameSpyQueryType_Info, "long2", std::string(690, 'M').c_str());
    Query(a.get(), sPortA, "\\info\\");
    Query(a.get(), sPortA, "\\status\\");
    Query(a.get(), sPortA, "\\packets\\");
    a->EraseKeyValue(GameSpyQueryType_Info, "long1");
    a->EraseKeyValue(GameSpyQueryType_Info, "long2");
    // the basic keys (29 bytes) and info (1,321) make exactly 1,350: not under the limit, so two packets
    a->SetKeyValue(GameSpyQueryType_Info, "pad", std::string(1249, 'P').c_str());
    Query(a.get(), sPortA, "\\status\\");
    a->EraseKeyValue(GameSpyQueryType_Info, "pad");
    // a key too big to send after a small one: only the small one goes
    a->SetKeyValue(GameSpyQueryType_Players, "zz", std::string(1380, 'Z').c_str());
    Query(a.get(), sPortA, "\\players\\");
    a->EraseKeyValue(GameSpyQueryType_Players, "zz");
    // the map dump's limit: a pair that fills the buffer to its last byte and one after it are left out
    a->EraseAllKeyValues(GameSpyQueryType_Rules);
    a->SetKeyValue(GameSpyQueryType_Rules, "k1", std::string(996, 'a').c_str());
    a->SetKeyValue(GameSpyQueryType_Rules, "k2", std::string(396, 'b').c_str());
    a->SetKeyValue(GameSpyQueryType_Rules, "k3", "c");
    Query(a.get(), sPortA, "\\rules\\");
    a->EraseAllKeyValues(GameSpyQueryType_Rules);
    a->SetKeyValue(GameSpyQueryType_Rules, "k1", std::string(995, 'a').c_str());
    a->SetKeyValue(GameSpyQueryType_Rules, "k2", std::string(396, 'b').c_str());
    Query(a.get(), sPortA, "\\rules\\");
    a->EraseAllKeyValues(GameSpyQueryType_Rules);
    Log("rules k1 " + Key(a.get(), GameSpyQueryType_Rules, "k1"));
    a->SetKeyValue(GameSpyQueryType_Rules, "difficulty", "easy");

    // the auto pump answers without a pump; off, a query waits for the next pump
    a->SetAutoPumpTime(20);
    LogState("autopump 20", a.get());
    Send(sPortA, "\\basic\\");
    LogReplies("autopumped", 2000);
    a->SetAutoPumpTime(0);
    LogState("autopump 0", a.get());
    PlatSleep(100);
    Drain(sClient, 50);
    Send(sPortA, "\\rules\\");
    LogReplies("no autopump", 300);
    a->Pump();
    LogReplies("pumped", 300);
    LogHeartbeats("a quiet", 100);

    // a second instance asked for A's port takes the next one
    SmartPtr<GameSpySupport> b = new GameSpySupport("eetest2", "2.00", "key2", aDirs.get());
    b->SetGameMode("closedplaying");
    b->SetKeyValue(GameSpyQueryType_Info, "hostname", "Second");
    Log("b startup " + N(b->Startup(IPAddr("127.0.0.1", (unsigned short)sPortA))));
    LogState("b started", b.get());
    PumpUntilHeartbeat("b first", b.get());
    Query(b.get(), sPortA + 1, "\\basic\\\\info\\");
    Query(a.get(), sPortA, "\\basic\\");

    // an instance whose start-up failed (no port on an address not this machine's) pumps the SDK's current
    // record: the last one started (b), its queries and its state change
    SmartPtr<GameSpySupport> f = new GameSpySupport("eefail", "3.00", "key3", aDirs.get());
    f->SetGameMode("failing");
    Log("f startup " + N(f->Startup(IPAddr("192.0.2.1", 0))));
    LogState("f started", f.get());
    PumpUntilHeartbeat("f first", f.get());
    Query(f.get(), sPortA + 1, "\\info\\");
    Query(f.get(), sPortA, "\\info\\");
    Drain(sClient, 300);
    LogHeartbeats("f quiet", 100);

    // the shutdown: the state change to "exiting" sent; the socket still answers at the next pump
    a->Shutdown();
    LogState("a shut down", a.get());
    LogHeartbeats("a shutdown", 300);
    Log("info gamemode " + Key(a.get(), GameSpyQueryType_Info, "gamemode"));
    Query(a.get(), sPortA, "\\info\\");
    b->Shutdown();
    LogHeartbeats("b shutdown", 300);
    f->Shutdown();
    LogHeartbeats("f shutdown", 300);
    f = NULL;
    b = NULL;
    a = NULL;
    aDirs = NULL;
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_gamespy.txt";
}

int main(int argc, char** argv)
{
    AsyncSocket::StartWinsock();
    sMaster = PlatSocketOpen(PLAT_SOCKET_DGRAM);
    sClient = PlatSocketOpen(PLAT_SOCKET_DGRAM);
    PlatSockAddrIn aMaster = Addr(kLoopback, 27900), aClient = Addr(kLoopback, 0);
    if (!PlatSocketBind(sMaster, &aMaster) || !PlatSocketBind(sClient, &aClient)) { // 1: bound
        printf("FAIL: cannot bind the master stand-in (127.0.0.1:27900) or the client\n");
        return 1;
    }
    SetMasterHost("127.0.0.1");
    {
        WONAPICoreEx aCore;
        aCore.SetDoPumpThread(true); // the timers' and the service lookup's completions run on the pump thread
        aCore.Startup();
        Script();
        aCore.Pump(20);
        aCore.Shutdown();
    }
    PlatSocketClose(sMaster);
    PlatSocketClose(sClient);
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
    char aLine[8192];
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
    printf("won_gamespy_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
