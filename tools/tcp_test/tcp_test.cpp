// Loopback test of the TCP/IP network plug-in (Rust: rust/crates/ee_net_tcp) on the platform layer's sockets
// (platform/PlatSocket.h): one process plays host and client over 127.0.0.1 through the engine's protocol
// (NEProtocol, whole in Rust: ee_net ffi_protocol.rs; its servers and connections), with the arguments the network engine passes
// (NESynchronousServer::Publish: "EEService", port 2 = the datagram game socket; NESynchronousClient::Connect: type
// 2; ConnectForXfer mode 1):
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
// protocol and the plug-in's engine calls with the real UString, UWideString, TSCriticalSection and TSEvent units and the
// platform layer; the rest of the engine the plug-in calls is stood in below (the error system, TMGetTime, the
// floating-point mode, URandom, TSScheduler's create/destroy). One process holds both roles, so the plug-in's
// process-wide globals are shared (its xfer listen socket gSocket7280, which every connection's close also
// closes): the closes after the first see it closed already (result 1, as in a real client process). The LAN game
// search (SearchForGame) is not tested: its querier and the host both bind port 33334, which needs two machines.
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
#include "rust/ee_ts.h"
#include "lle/LowLevelEngine.h"
#include "lle/NEProtocol.h"
#include "rust/ee_net_tcp.h"

// the protocol (whole in Rust, ee_net ffi_protocol.rs) with the original's members, made and freed as the server
// did (tests/net/ne_protocol_members.h)
#include "../../tests/net/ne_protocol_members.h"

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
long TSScheduler::CreateCriticalSection(TSCriticalSection*& outSection)   // as TSScheduler.cpp (ee_ts)
{
    outSection = reinterpret_cast<TSCriticalSection*>(ee_ts_section_new());
    return kErrorOK;
}
void TSScheduler::DestroyCriticalSection(TSCriticalSection* inSection)
{
    ee_ts_section_free(reinterpret_cast<EeTsSection*>(inSection));
}
long TSScheduler::CreateEvent(TSEvent*& outEvent, bool inManualReset)
{
    return ee_ts_event_new(reinterpret_cast<EeTsEvent**>(&outEvent), inManualReset);
}
void TSScheduler::DestroyEvent(TSEvent* inEvent)
{
    ee_ts_event_free(reinterpret_cast<EeTsEvent*>(inEvent));
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

// ---- the protocol

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

static void Fill(std::vector<unsigned char>& outData, unsigned long inSeed)
{
    for (size_t i = 0; i < outData.size(); i++)
        outData[i] = (unsigned char)((i * 131 + inSeed * 7 + (i >> 8)) & 0xff);
}

int main()
{
    // the test's ports are the game's (33334, 33335): set EE_TCP_TEST_PORT_OFFSET when a game runs on this machine
    if (const char* theOffset = getenv("EE_TCP_TEST_PORT_OFFSET"))
        gPlatSimPortOffset = atoi(theOffset);

    // 1. activate (the protocol's constructor), publish
    Proto* theProtocol = NESynchronousServer::New();
    REQUIRE(NEProtocol::GetCount() == 1);
    CHECK(ee_tcp_is_valid());
    printf("protocol: %s, type %lu\n", (const char*)theProtocol->GetName(), theProtocol->GetProtocolType());
    std::vector<UWideString> theAdapters;
    unsigned long theAdapterCount = theProtocol->EnumerateNetworkAdapters(theAdapters);
    printf("adapters: %lu", theAdapterCount);
    for (size_t i = 0; i < theAdapters.size(); i++)
        printf("%s \"%s\"", i ? "," : "", (const char*)theAdapters[i]);
    printf("\nlocal address (adapter 0): %s\n", (const char*)theProtocol->GetLocalAddress(0));

    TSEvent *theHostData, *theHostXfer, *theHostAccept, *theClientData, *theClientXfer;
    REQUIRE(TSScheduler::CreateEvent(theHostData) == kErrorOK && TSScheduler::CreateEvent(theHostXfer) == kErrorOK
            && TSScheduler::CreateEvent(theHostAccept) == kErrorOK
            && TSScheduler::CreateEvent(theClientData) == kErrorOK
            && TSScheduler::CreateEvent(theClientXfer) == kErrorOK);
    NEServer* theServer = NULL;
    long theResult = theProtocol->Publish("EEService", UWideString("tcp_test game"), 2, theHostData, theHostXfer,
                                          theHostAccept, theServer, false, false, 0);
    printf("publish: %ld\n", theResult);
    REQUIRE(theResult == kOK && theServer != NULL);

    // 2. client connects and joins
    NEConnection* theClient = NULL;
    theResult = theProtocol->Connect("127.0.0.1", "EEService", UWideString("tcp_test game"), 2, theClientData,
                                     theClient, 0);
    printf("connect: %ld\n", theResult);
    REQUIRE(theResult == kOK && theClient != NULL);

    std::vector<unsigned char> theJoin(200), theBuffer(0x10000 + 16);
    Fill(theJoin, 1);
    theJoin[1] = 0x22;                       // a join packet (byte 0 is the cipher key the plug-in writes)
    std::vector<unsigned char> theSent = theJoin;
    CHECK(theProtocol->Send(theClient, theSent.data(), (unsigned long)theSent.size()) == kOK);
    unsigned long theReceived = 0, theIndex = 99;
    theResult = ReceiveWithin(theHostData, [&] {
        return theProtocol->ReceiveFrom(theServer, (unsigned long)theBuffer.size(), theBuffer.data(), theReceived, theIndex);
    });
    printf("host receive (join): %ld, %lu bytes, client %lu\n", theResult, theReceived, theIndex);
    REQUIRE(theResult == kOK && theReceived == theJoin.size() && theIndex == 0);
    CHECK(memcmp(theBuffer.data() + 1, theJoin.data() + 1, theJoin.size() - 1) == 0);
    UWideString theClientAddress;
    CHECK(theProtocol->GetClientAddress(theServer, 0, theClientAddress) == kOK);
    printf("client 0 address: %s\n", (const char*)theClientAddress);
    CHECK(strcmp(theClientAddress, "127.0.0.1") == 0);

    // 3. host -> client datagram
    std::vector<unsigned char> theReply(900);
    Fill(theReply, 2);
    theSent = theReply;
    CHECK(theProtocol->SendTo(theServer, 0, theSent.data(), (unsigned long)theSent.size()) == kOK);
    theReceived = 0;
    theResult = ReceiveWithin(theClientData, [&] {
        return theProtocol->Receive(theClient, (unsigned long)theBuffer.size(), theBuffer.data(), theReceived);
    });
    printf("client receive (datagram): %ld, %lu bytes\n", theResult, theReceived);
    CHECK(theResult == kOK && theReceived == theReply.size()
          && memcmp(theBuffer.data() + 1, theReply.data() + 1, theReply.size() - 1) == 0);

    // 4. the file-transfer stream
    NEConnection *theClientStream = NULL, *theHostStream = NULL;
    theResult = theProtocol->ConnectForXfer(theClient, 1, theClientXfer, theClientStream);
    printf("connect for xfer: %ld\n", theResult);
    REQUIRE(theResult == kOK && theClientStream != NULL);
    theResult = ReceiveWithin(theHostAccept, [&] { return theProtocol->AcceptForXfer(theServer, theHostStream); });
    printf("accept for xfer: %ld\n", theResult);
    REQUIRE(theResult == kOK && theHostStream != NULL);
    CHECK(theProtocol->IsAlive(theClientStream) && theProtocol->IsAlive(theHostStream));

    // 5. host -> client transfer, client -> host stream reply
    std::vector<unsigned char> theFile(0x10000);
    Fill(theFile, 3);
    theSent = theFile;
    CHECK(theProtocol->XferSendTo(theServer, 0, theSent.data(), (unsigned long)theSent.size()) == kOK);
    std::vector<unsigned char> theGot;
    uint64_t theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theGot.size() < theFile.size() && PlatTimeMilliseconds() < theDeadline) {
        theReceived = 0;
        theResult = theProtocol->XferReceive((unsigned long)theBuffer.size(), theBuffer.data(), theReceived);
        if (theResult == kOK)
            theGot.insert(theGot.end(), theBuffer.begin(), theBuffer.begin() + theReceived);
        else if (theResult == kWouldBlock)
            PlatSleep(1);
        else
            break;
    }
    printf("client xfer receive: %lu of %lu bytes\n", (unsigned long)theGot.size(), (unsigned long)theFile.size());
    CHECK(theGot == theFile);

    std::vector<unsigned char> theAnswer(3000);
    Fill(theAnswer, 4);
    theSent = theAnswer;
    CHECK(theProtocol->Send(theClientStream, theSent.data(), (unsigned long)theSent.size()) == kOK);
    theGot.clear();
    theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theGot.size() < theAnswer.size() && PlatTimeMilliseconds() < theDeadline) {
        theReceived = 0;
        theResult = theProtocol->Receive(theHostStream, (unsigned long)theBuffer.size(), theBuffer.data(), theReceived);
        if (theResult == kOK)
            theGot.insert(theGot.end(), theBuffer.begin(), theBuffer.begin() + theReceived);
        else if (theResult == kWouldBlock)
            PlatSleep(1);
        else
            break;
    }
    printf("host stream receive: %lu of %lu bytes\n", (unsigned long)theGot.size(), (unsigned long)theAnswer.size());
    CHECK(theGot == theAnswer);
    // every byte sent and received through the protocol is counted (the datagrams, the transfer, the stream)
    printf("protocol bytes: sent %lu, received %lu\n", theProtocol->GetBytesSent(), theProtocol->GetBytesReceived());
    CHECK(theProtocol->GetBytesSent() == theJoin.size() + theReply.size() + theFile.size() + theAnswer.size());
    CHECK(theProtocol->GetBytesReceived() == theJoin.size() + theReply.size() + theFile.size() + theAnswer.size());

    // 6. disconnect (a connection the plug-in closes is freed)
    theResult = theProtocol->Close(theClientStream);
    printf("client closes its stream: %ld\n", theResult);
    CHECK(theResult == kOK);
    bool theAlive = true;
    theDeadline = PlatTimeMilliseconds() + kTimeout;
    while (theAlive && PlatTimeMilliseconds() < theDeadline) {
        theAlive = theProtocol->IsAlive(theHostStream);
        if (theAlive)
            PlatSleep(1);
    }
    CHECK(!theAlive);
    theResult = theProtocol->Receive(theHostStream, (unsigned long)theBuffer.size(), theBuffer.data(), theReceived);
    printf("host sees the close: alive %d, receive %ld\n", (int)theAlive, theResult);
    CHECK(theResult == kUnavailable);
    // the shared xfer listen socket is closed already (see the head comment): these report 1 for it, and the
    // plug-in keeps the connections it could not close
    printf("close host stream: %ld\n", theProtocol->Close(theHostStream));
    printf("close client: %ld\n", theProtocol->Close(theClient));
    printf("close server: %ld\n", theProtocol->Close(theServer));
    TSScheduler::DestroyEvent(theHostData);
    TSScheduler::DestroyEvent(theHostXfer);
    TSScheduler::DestroyEvent(theHostAccept);
    TSScheduler::DestroyEvent(theClientData);
    TSScheduler::DestroyEvent(theClientXfer);
    NESynchronousServer::Delete(theProtocol);   // deactivates the plug-in
    CHECK(NEProtocol::GetCount() == 0);
    return Finish();
}
