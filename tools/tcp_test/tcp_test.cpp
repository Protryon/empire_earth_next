// Loopback test of the TCP/IP network plug-in (src/Low-Level Engine/Network Engine/TCP Protocol/TCP Protocol.cpp)
// on the platform layer's sockets (platform/PlatSocket.h): one process plays host and client over 127.0.0.1 through
// the plug-in's own NPIM* entry points, looked up by name in its PIMPlugin table as NEProtocol does
// (PIMModule::GetEntry), with the arguments the network engine passes (NESynchronousServer::Publish: "EEService",
// flags 2 = the datagram game socket; NESynchronousClient::Connect: type 2; ConnectForXfer mode 1):
//
//   1. activate; host publishes (datagram game socket on 33334, file-transfer listen socket on 33335)
//   2. client connects (datagram), sends a join packet (type 0x22); the host's ReceiveFrom registers it as client 0
//   3. host -> client datagram (SendTo / Receive); GetClientAddress is 127.0.0.1
//   4. client opens the file-transfer stream (ConnectForXfer); the host accepts it for client 0 (AcceptForXfer)
//   5. a 64 KB transfer host -> client (XferSendTo / XferReceive), a stream reply client -> host (Send / Receive)
//   6. disconnect: the client closes its stream; the host sees it (IsAlive false, Receive 9 = closed); then every
//      handle is closed and the plug-in deactivated
// Every payload is checked byte for byte after the plug-in's RC4 enciphering round trip.
//
// Build and run: python -I tools/xbuild.py tcptest [--target windows-x86_64|linux-x86_64] [--no-run]. It links the
// plug-in with the real UString, UWideString, NEProtocolConfiguration, TSCriticalSection and TSEvent units and the
// platform layer; the rest of the engine the plug-in calls is stood in below (the error system, TMGetTime, the
// floating-point mode, URandom, TSScheduler's create/destroy). One process holds both roles, so the plug-in's
// process-wide globals are shared (its xfer listen socket gSocket7280, which every NPIMCloseConnection also
// closes): the closes after the first see it closed already (result 1, as in a real client process). The LAN game
// search (NPIMSearchForGame) is not tested: its querier and the host both bind port 33334, which needs two machines.
// Uses ports 33334 and 33335 (the game's): no game may run on this machine meanwhile.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "platform/wintypes.h"
#include "platform/PlatSocket.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "platform/PlatSim.h"
#include "lle/LowLevelEngine.h"
#include "lle/PIMModule.h"

// ---- stand-ins for the engine functions the plug-in calls (the test links no other engine unit)

static unsigned long sNativeError;
unsigned long ESRecordNativeError(unsigned long inError)
{
    sNativeError = inError;
    return inError;
}
unsigned long ESGetNativeError()
{
    return sNativeError;
}
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    fprintf(stderr, "ESVERIFY failed: %s:%lu: %s\n", inFile, inLine, inExpression);
}
void ESChecksum::AddChecksum(const UString&, unsigned long)   // UWideString::GetChecksum's (not called)
{
}
unsigned long TMGetTime()
{
    return (unsigned long)PlatTimeMilliseconds();
}
static unsigned long sPrecision = kFPPrecision53;
static unsigned long sRounding;
void UMSetFloatingPointPrecision(unsigned long inPrecision)
{
    sPrecision = inPrecision;
}
unsigned long UMGetFloatingPointPrecision()
{
    return sPrecision;
}
void UMSetFloatingPointRoundingMode(unsigned long inMode)
{
    sRounding = inMode;
}
unsigned long UMGetFloatingPointRoundingMode()
{
    return sRounding;
}
float URandom::Generate()   // as GEBitmap.cpp
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return (float)((double)this->mSeed / (double)this->mModulus);
}
unsigned long URandom::NextInSeries()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return this->mSeed;
}
long TSScheduler::CreateCriticalSection(TSCriticalSection*& outSection)   // as TSScheduler.cpp
{
    outSection = new TSCriticalSection;
    return outSection ? kErrorOK : kErrorOutOfMemory;
}
void TSScheduler::DestroyCriticalSection(TSCriticalSection* inSection)
{
    delete inSection;
}
long TSScheduler::CreateEvent(TSEvent*& outEvent, bool inManualReset)
{
    PlatEvent* theHandle = PlatEventCreate(inManualReset, 0);
    if (theHandle == NULL)
        return kErrorNativeError;
    outEvent = new TSEvent(theHandle, inManualReset);
    return kErrorOK;
}
void TSScheduler::DestroyEvent(TSEvent* inEvent)
{
    delete inEvent;
}
// the search thread (NPIMSearchForGame) is not tested: no threads
long TSScheduler::CreateThread(TSThread*& outThread, unsigned long (*)(void*), void*, unsigned long, bool)
{
    outThread = NULL;
    return kErrorNativeError;
}
long TSScheduler::DestroyThread(TSThread*)
{
    return kErrorOK;
}

// ---- the plug-in

extern const PIMPlugin gPIMPluginTCPProtocol;

static void* Entry(const char* inName)
{
    for (const PIMEntry* e = gPIMPluginTCPProtocol.mEntries; e->mName != NULL; e++)
        if (strcmp(e->mName, inName) == 0)
            return e->mProc;
    fprintf(stderr, "no entry point %s\n", inName);
    return NULL;
}

typedef void* Handle;   // the plug-in's TCPServer* / TCPConnection* (the engine's NEServer / NEConnection slot)

static int sFailures;
#define CHECK(cond)                                                                                              \
    do {                                                                                                         \
        if (!(cond)) {                                                                                           \
            fprintf(stderr, "FAIL %s:%d: %s (native error %lu, socket error %u)\n", __FILE__, __LINE__, #cond,  \
                    sNativeError, PlatSocketLastError());                                                        \
            sFailures++;                                                                                         \
        }                                                                                                        \
    } while (0)
#define REQUIRE(cond)                                                                                            \
    do {                                                                                                         \
        CHECK(cond);                                                                                             \
        if (!(cond))                                                                                             \
            return Finish();                                                                                     \
    } while (0)

static int Finish()
{
    printf(sFailures ? "tcp_test: %d FAILED\n" : "tcp_test: all checks passed\n", sFailures);
    return sFailures ? 1 : 0;
}

const long kOK = 0, kWouldBlock = 7, kUnavailable = 9;
const unsigned long kTimeout = 5000;

// Calls inReceive until it stops returning would-block (waiting on inEvent between tries) -> its last result
template <class F>
static long ReceiveWithin(TSEvent* inEvent, F inReceive)
{
    uint64_t theDeadline = PlatTimeMilliseconds() + kTimeout;
    for (;;) {
        long theResult = inReceive();
        if (theResult != kWouldBlock || PlatTimeMilliseconds() > theDeadline)
            return theResult;
        if (inEvent != NULL) {
            inEvent->Wait(50);
            inEvent->Clear();
        } else {
            PlatSleep(1);
        }
    }
}

static void Fill(std::vector<char>& outData, unsigned long inSeed)
{
    for (size_t i = 0; i < outData.size(); i++)
        outData[i] = (char)((i * 131 + inSeed * 7 + (i >> 8)) & 0xff);
}

int main()
{
    // the test's ports are the game's (33334, 33335): set EE_TCP_TEST_PORT_OFFSET when a game runs on this machine
    if (const char* theOffset = getenv("EE_TCP_TEST_PORT_OFFSET"))
        gPlatSimPortOffset = atoi(theOffset);

    long (*Activate)() = (long (*)())Entry("NPIMActivate");
    long (*Deactivate)() = (long (*)())Entry("NPIMDeactivate");
    bool (*IsValid)() = (bool (*)())Entry("PIMIsValid");
    long (*Publish)(const UString&, const UWideString&, unsigned long, TSEvent*, TSEvent*, TSEvent*, Handle*, bool,
                    bool, unsigned long) =
        (long (*)(const UString&, const UWideString&, unsigned long, TSEvent*, TSEvent*, TSEvent*, Handle*, bool, bool,
                  unsigned long))Entry("NPIMPublish");
    long (*Connect)(const UString&, const UString&, const UWideString&, long, TSEvent*, Handle*, unsigned long) =
        (long (*)(const UString&, const UString&, const UWideString&, long, TSEvent*, Handle*, unsigned long))Entry(
            "NPIMConnect");
    long (*Send)(Handle*, char*, unsigned long) = (long (*)(Handle*, char*, unsigned long))Entry("NPIMSend");
    long (*Receive)(Handle*, unsigned long, char*, unsigned long*) =
        (long (*)(Handle*, unsigned long, char*, unsigned long*))Entry("NPIMReceive");
    long (*ReceiveFrom)(Handle*, unsigned long, char*, unsigned long*, unsigned long*) =
        (long (*)(Handle*, unsigned long, char*, unsigned long*, unsigned long*))Entry("NPIMReceiveFrom");
    long (*SendTo)(Handle*, unsigned long, char*, unsigned long) =
        (long (*)(Handle*, unsigned long, char*, unsigned long))Entry("NPIMSendTo");
    long (*ConnectForXfer)(Handle*, long, TSEvent*, Handle*) =
        (long (*)(Handle*, long, TSEvent*, Handle*))Entry("NPIMConnectForXfer");
    long (*AcceptForXfer)(Handle*, Handle*) = (long (*)(Handle*, Handle*))Entry("NPIMAcceptForXfer");
    long (*XferSendTo)(Handle*, unsigned long, char*, unsigned long) =
        (long (*)(Handle*, unsigned long, char*, unsigned long))Entry("NPIMXferSendTo");
    long (*XferReceive)(unsigned long, char*, unsigned long*) =
        (long (*)(unsigned long, char*, unsigned long*))Entry("NPIMXferReceive");
    bool (*IsAlive)(Handle*) = (bool (*)(Handle*))Entry("NPIMIsAlive");
    long (*CloseConnection)(Handle*) = (long (*)(Handle*))Entry("NPIMCloseConnection");
    long (*CloseServer)(Handle*) = (long (*)(Handle*))Entry("NPIMCloseServer");
    long (*GetClientAddress)(Handle*, unsigned long, UWideString&) =
        (long (*)(Handle*, unsigned long, UWideString&))Entry("NPIMGetClientAddress");
    const UWideString& (*GetLocalAddress)(unsigned long) =
        (const UWideString& (*)(unsigned long))Entry("NPIMGetLocalAddress");
    long (*EnumerateAdapters)(std::vector<UWideString>&) =
        (long (*)(std::vector<UWideString>&))Entry("NPIMEnumerateNetworkAdapters");
    REQUIRE(Activate && Publish && Connect && Send && Receive && ReceiveFrom && SendTo && ConnectForXfer
            && AcceptForXfer && XferSendTo && XferReceive && IsAlive && CloseConnection && CloseServer);

    // 1. activate, publish
    REQUIRE(Activate() == kOK);
    CHECK(IsValid());
    std::vector<UWideString> theAdapters;
    long theAdapterCount = EnumerateAdapters(theAdapters);
    printf("adapters: %ld", theAdapterCount);
    for (size_t i = 0; i < theAdapters.size(); i++)
        printf("%s \"%s\"", i ? "," : "", (const char*)theAdapters[i]);
    printf("\nlocal address (adapter 0): %s\n", (const char*)GetLocalAddress(0));

    TSEvent *theHostData, *theHostXfer, *theHostAccept, *theClientData, *theClientXfer;
    REQUIRE(TSScheduler::CreateEvent(theHostData) == kErrorOK && TSScheduler::CreateEvent(theHostXfer) == kErrorOK
            && TSScheduler::CreateEvent(theHostAccept) == kErrorOK
            && TSScheduler::CreateEvent(theClientData) == kErrorOK
            && TSScheduler::CreateEvent(theClientXfer) == kErrorOK);
    Handle theServer = NULL;
    long theResult = Publish("EEService", UWideString("tcp_test game"), 2, theHostData, theHostXfer, theHostAccept,
                             &theServer, false, false, 0);
    printf("publish: %ld\n", theResult);
    REQUIRE(theResult == kOK && theServer != NULL);

    // 2. client connects and joins
    Handle theClient = NULL;
    theResult = Connect("127.0.0.1", "EEService", UWideString("tcp_test game"), 2, theClientData, &theClient, 0);
    printf("connect: %ld\n", theResult);
    REQUIRE(theResult == kOK && theClient != NULL);

    std::vector<char> theJoin(200), theBuffer(0x10000 + 16);
    Fill(theJoin, 1);
    theJoin[1] = 0x22;                       // a join packet (byte 0 is the cipher key the plug-in writes)
    std::vector<char> theSent = theJoin;
    CHECK(Send(&theClient, theSent.data(), (unsigned long)theSent.size()) == kOK);
    unsigned long theReceived = 0, theIndex = 99;
    theResult = ReceiveWithin(theHostData, [&] {
        return ReceiveFrom(&theServer, (unsigned long)theBuffer.size(), theBuffer.data(), &theReceived, &theIndex);
    });
    printf("host receive (join): %ld, %lu bytes, client %lu\n", theResult, theReceived, theIndex);
    REQUIRE(theResult == kOK && theReceived == theJoin.size() && theIndex == 0);
    CHECK(memcmp(theBuffer.data() + 1, theJoin.data() + 1, theJoin.size() - 1) == 0);
    UWideString theClientAddress;
    CHECK(GetClientAddress(&theServer, 0, theClientAddress) == kOK);
    printf("client 0 address: %s\n", (const char*)theClientAddress);
    CHECK(strcmp(theClientAddress, "127.0.0.1") == 0);

    // 3. host -> client datagram
    std::vector<char> theReply(900);
    Fill(theReply, 2);
    theSent = theReply;
    CHECK(SendTo(&theServer, 0, theSent.data(), (unsigned long)theSent.size()) == kOK);
    theReceived = 0;
    theResult = ReceiveWithin(theClientData, [&] {
        return Receive(&theClient, (unsigned long)theBuffer.size(), theBuffer.data(), &theReceived);
    });
    printf("client receive (datagram): %ld, %lu bytes\n", theResult, theReceived);
    CHECK(theResult == kOK && theReceived == theReply.size()
          && memcmp(theBuffer.data() + 1, theReply.data() + 1, theReply.size() - 1) == 0);

    // 4. the file-transfer stream
    Handle theClientStream = NULL, theHostStream = NULL;
    theResult = ConnectForXfer(&theClient, 1, theClientXfer, &theClientStream);
    printf("connect for xfer: %ld\n", theResult);
    REQUIRE(theResult == kOK && theClientStream != NULL);
    theResult = ReceiveWithin(theHostAccept, [&] { return AcceptForXfer(&theServer, &theHostStream); });
    printf("accept for xfer: %ld\n", theResult);
    REQUIRE(theResult == kOK && theHostStream != NULL);
    CHECK(IsAlive(&theClientStream) && IsAlive(&theHostStream));

    // 5. host -> client transfer, client -> host stream reply
    std::vector<char> theFile(0x10000);
    Fill(theFile, 3);
    theSent = theFile;
    CHECK(XferSendTo(&theServer, 0, theSent.data(), (unsigned long)theSent.size()) == kOK);
    std::vector<char> theGot;
    uint64_t theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theGot.size() < theFile.size() && PlatTimeMilliseconds() < theDeadline) {
        theReceived = 0;
        theResult = XferReceive((unsigned long)theBuffer.size(), theBuffer.data(), &theReceived);
        if (theResult == kOK)
            theGot.insert(theGot.end(), theBuffer.begin(), theBuffer.begin() + theReceived);
        else if (theResult == kWouldBlock)
            PlatSleep(1);
        else
            break;
    }
    printf("client xfer receive: %lu of %lu bytes\n", (unsigned long)theGot.size(), (unsigned long)theFile.size());
    CHECK(theGot == theFile);

    std::vector<char> theAnswer(3000);
    Fill(theAnswer, 4);
    theSent = theAnswer;
    CHECK(Send(&theClientStream, theSent.data(), (unsigned long)theSent.size()) == kOK);
    theGot.clear();
    theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theGot.size() < theAnswer.size() && PlatTimeMilliseconds() < theDeadline) {
        theReceived = 0;
        theResult = Receive(&theHostStream, (unsigned long)theBuffer.size(), theBuffer.data(), &theReceived);
        if (theResult == kOK)
            theGot.insert(theGot.end(), theBuffer.begin(), theBuffer.begin() + theReceived);
        else if (theResult == kWouldBlock)
            PlatSleep(1);
        else
            break;
    }
    printf("host stream receive: %lu of %lu bytes\n", (unsigned long)theGot.size(), (unsigned long)theAnswer.size());
    CHECK(theGot == theAnswer);

    // 6. disconnect
    theResult = CloseConnection(&theClientStream);
    printf("client closes its stream: %ld\n", theResult);
    CHECK(theResult == kOK && theClientStream == NULL);
    bool theAlive = true;
    theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theAlive && PlatTimeMilliseconds() < theDeadline) {
        theAlive = IsAlive(&theHostStream);
        if (theAlive)
            PlatSleep(1);
    }
    CHECK(!theAlive);
    theResult = Receive(&theHostStream, (unsigned long)theBuffer.size(), theBuffer.data(), &theReceived);
    printf("host sees the close: alive %d, receive %ld\n", (int)theAlive, theResult);
    CHECK(theResult == kUnavailable);
    // the shared xfer listen socket is closed already (see the head comment): these report 1 for it
    printf("close host stream: %ld\n", CloseConnection(&theHostStream));
    printf("close client: %ld\n", CloseConnection(&theClient));
    CHECK(theHostStream == NULL && theClient == NULL);
    printf("close server: %ld\n", CloseServer(&theServer));
    TSScheduler::DestroyEvent(theHostData);
    TSScheduler::DestroyEvent(theHostXfer);
    TSScheduler::DestroyEvent(theHostAccept);
    TSScheduler::DestroyEvent(theClientData);
    TSScheduler::DestroyEvent(theClientXfer);
    CHECK(Deactivate() == kOK);
    return Finish();
}
