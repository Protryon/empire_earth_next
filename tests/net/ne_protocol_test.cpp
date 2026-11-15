// NEProtocol over the TCP plug-in (LLE Network Engine round 2; round 5 made NEConnection and NEServer the plug-in's
// handles, the protocol's members taking them; round 7 moved the class whole to Rust, ee_net ffi_protocol.rs, driven
// here through the test's adapter with the original's members, Proto): random calls through the engine's protocol over a stand-in plug-in (the ee_tcp_* functions below, each call a trace line with its
// arguments and an answer from the trace's random stream). The protocol, its connections and servers are the
// engine's units; the trace is compared with tests/net/golden_protocol.txt, which the original units made: the
// original NEProtocol.cpp with its table of 30 entry points looked up in the TCP shim's PIMPlugin table, and the
// shim's NPIM* functions converting the strings and events (built with NE_PROTOCOL_ORIG, before they were deleted).
//
// `--print FILE` writes the trace.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include "platform/PlatThread.h"
#include "rust/ee_ts.h"
#include "lle/LowLevelEngine.h"
#ifdef NE_PROTOCOL_ORIG
#include "orig_p/lle/NEProtocol.h"   // the original's (tests/net/orig_p: the units before LLE Network Engine round 2)
#endif
#include "lle/NEProtocol.h"
#ifdef NE_PROTOCOL_ORIG
#include "lle/PIMModule.h"
#endif
#include "rust/ee_net_tcp.h"

#ifdef NE_PROTOCOL_ORIG
// NEProtocol's constructor and destructor are its friends' (NEInitialize, NEShutdown, the server): the test is the
// server here (the game's server is not linked)
class NESynchronousServer {
public:
    static NEProtocol* New(PIMModule* inModule) { return new NEProtocol(inModule); }
    static void Delete(NEProtocol* inProtocol) { delete inProtocol; }
    static void ResetXfer(NEProtocol* inProtocol, NEServer* inServer) { inProtocol->ResetXfer(inServer); }
};
typedef NEProtocol Proto;
static bool SameProtocol(NEProtocol* inFound, Proto* inProtocol) { return inFound == inProtocol; }
#else
#include "ne_protocol_members.h"
static bool SameProtocol(NEProtocol* inFound, Proto* inProtocol)
{
    return reinterpret_cast<EeNetProtocol*>(inFound) == inProtocol->mRust;
}
#endif

// ---- the engine functions the units call (the test links only the strings, events and locks)

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
void ESChecksum::AddChecksum(const UString&, unsigned long)
{
}
unsigned long TMGetTime()
{
    return 1000;
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
float URandom::Generate()   // as GEBitmap.cpp
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return (float)((double)this->mSeed / (double)this->mModulus);
}

#ifdef NE_PROTOCOL_ORIG
// PIMModule.cpp's members the original protocol uses
static void* PIMFind(const PIMPlugin* inPlugin, const char* inName)
{
    for (const PIMEntry* theEntry = inPlugin->mEntries; theEntry->mName != NULL; theEntry++)
        if (strcmp(theEntry->mName, inName) == 0)
            return theEntry->mProc;
    return NULL;
}
PIMModule::~PIMModule()
{
}
void* PIMModule::FindEntry(const PIMPlugin* inPlugin, const char* inName)
{
    return PIMFind(inPlugin, inName);
}
extern const PIMPlugin gPIMPluginTCPProtocol;
#endif

// ---- the world: the trace, the random answers, the handles by order of appearance

static std::string sTrace;
static uint32_t sRandom = 1;
static std::map<const void*, int> sIds;
static int sNextId;   // ids by order of appearance (a new wrapper gets a new one: the allocator may reuse an address)   // a wrapper's handle slot (void**) or an event's handle -> its number

static void Line(const char* inFormat, ...)
{
    char theLine[1024];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
    va_end(theArgs);
    sTrace += theLine;
    sTrace += '\n';
}
static uint32_t Next()
{
    sRandom = sRandom * 1103515245u + 12345u;
    return sRandom >> 8;
}
static int32_t Answer()
{
    uint32_t r = Next() % 8;
    return r < 5 ? 0 : (int32_t)r;
}
static int Id(const void* inPointer)
{
    if (inPointer == NULL)
        return 0;
    auto i = sIds.find(inPointer);
    if (i != sIds.end())
        return i->second;
    int theID = ++sNextId;
    sIds[inPointer] = theID;
    return theID;
}
static std::string Text(const uint8_t* inBytes, size_t inLength)
{
    return inBytes == NULL ? "(null)" : "\"" + std::string((const char*)inBytes, inLength) + "\"";
}
static std::string Hex(const uint8_t* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}
// the plug-in gives a new connection or server a handle on success: the random number the original stand-in gave,
// made unique by a count in the bits above it (the trace prints the random number). A handle is known by its value
// (round 5: the engine holds the plug-in's handles, not wrappers with a slot); each open, failed or not, takes the
// next id, as the original stand-in's slot of a new wrapper did.
static uintptr_t sOpened;
static int32_t Open(const char* inWhat, void** outHandle, int32_t inResult)
{
    if (inResult == 0)
        *outHandle = (void*)((++sOpened << 12) | (uintptr_t)(0x100 + Next() % 0x100));
    int theID = ++sNextId;
    if (inResult == 0)
        sIds[*outHandle] = theID;
    Line("  %s h%d = %d (%lx)", inWhat, theID, inResult, (unsigned long)((uintptr_t)*outHandle & 0xfff));
    return inResult;
}
static void Sink(EeTcpSink inSink, void* inContext)
{
    char theText[32];
    snprintf(theText, sizeof(theText), "text%u", Next() % 1000);
    inSink(inContext, (const uint8_t*)theText, strlen(theText));
}

// ---- the stand-in plug-in

extern "C" {
int32_t ee_tcp_activate()
{
    int32_t r = Answer();
    Line("  activate = %d", r);
    return r;
}
int32_t ee_tcp_deactivate()
{
    int32_t r = Answer();
    Line("  deactivate = %d", r);
    return r;
}
bool ee_tcp_is_valid()
{
    bool r = Next() % 4 != 0;
    Line("  is_valid = %d", r);
    return r;
}
int32_t ee_tcp_get_status()
{
    int32_t r = Answer();
    Line("  get_status = %d", r);
    return r;
}
int32_t ee_tcp_accept(void** server, void** connection)
{
    Line("  accept h%d", Id(*server));
    return Open("accept", connection, Answer());
}
int32_t ee_tcp_accept_for_xfer(void** server, void** connection)
{
    Line("  accept_for_xfer h%d", Id(*server));
    return Open("accept_for_xfer", connection, Answer());
}
void ee_tcp_allow_new_connections(void** server, bool allow)
{
    Line("  allow_new_connections h%d %d", Id(*server), allow);
}
int32_t ee_tcp_close_connection(void** connection)
{
    int32_t r = Answer();
    Line("  close_connection h%d = %d", Id(*connection), r);
    return r;
}
int32_t ee_tcp_close_server(void** server)
{
    int32_t r = Answer();
    Line("  close_server h%d = %d", Id(*server), r);
    return r;
}
int32_t ee_tcp_connect(const uint8_t* name, size_t name_length, const uint8_t* host, size_t host_length,
                       const uint8_t* address, size_t address_length, int32_t kind, void* event, void** connection,
                       uint32_t adapter)
{
    Line("  connect %s %s %s %d e%d %u", Text(name, name_length).c_str(), Text(host, host_length).c_str(),
         Text(address, address_length).c_str(), kind, Id(event), adapter);
    return Open("connect", connection, Answer());
}
int32_t ee_tcp_connect_for_xfer(void** host, int32_t mode, void* event, void** connection)
{
    Line("  connect_for_xfer h%d %d e%d", Id(*host), mode, Id(event));
    return Open("connect_for_xfer", connection, Answer());
}
int32_t ee_tcp_get_client_address(void** server, uint32_t index, EeTcpSink sink, void* context)
{
    int32_t r = Answer();
    Line("  get_client_address h%d %u = %d", Id(*server), index, r);
    if (r == 0)
        Sink(sink, context);
    return r;
}
int32_t ee_tcp_get_detected_game(uint32_t index, EeTcpSink sink_name, void* context_name, EeTcpSink sink_host,
                                 void* context_host, EeTcpSink sink_address, void* context_address)
{
    int32_t r = Answer();
    Line("  get_detected_game %u = %d", index, r);
    if (r == 0) {
        Sink(sink_name, context_name);
        Sink(sink_host, context_host);
        Sink(sink_address, context_address);
    }
    return r;
}
uint32_t ee_tcp_get_detected_game_count()
{
    uint32_t r = Next() % 5;
    Line("  get_detected_game_count = %u", r);
    return r;
}
void ee_tcp_get_local_address(uint32_t adapter, EeTcpSink sink, void* context)
{
    Line("  get_local_address %u", adapter);
    Sink(sink, context);
}
uint32_t ee_tcp_enumerate_adapters(EeTcpSink sink, void* context)
{
    uint32_t n = Next() % 4;
    Line("  enumerate_adapters = %u", n);
    for (uint32_t i = 0; i < n; i++)
        Sink(sink, context);
    return n;
}
bool ee_tcp_is_alive(void** connection)
{
    bool r = Next() % 3 != 0;
    Line("  is_alive h%d = %d", Id(*connection), r);
    return r;
}
int32_t ee_tcp_publish(const uint8_t* name, size_t name_length, const uint8_t* address, size_t address_length,
                       uint32_t flags, void* data_event, void* listen_event, void* accept_event, void** server,
                       bool flag55, bool announce, uint32_t adapter)
{
    Line("  publish %s %s %u e%d e%d e%d %d %d %u", Text(name, name_length).c_str(),
         Text(address, address_length).c_str(), flags, Id(data_event), Id(listen_event), Id(accept_event), flag55,
         announce, adapter);
    return Open("publish", server, Answer());
}
static int32_t Receive(const char* inWhat, uint32_t size, uint8_t* buffer, uint32_t* received)
{
    int32_t r = Answer();
    uint32_t n = r == 0 ? Next() % (size + 1) : 0;
    for (uint32_t i = 0; i < n; i++)
        buffer[i] = (uint8_t)Next();
    Line("  %s in %u = %d %u %s", inWhat, *received, r, n, Hex(buffer, n).c_str());
    if (r == 0 || Next() % 2 == 0)
        *received = n;
    return r;
}
int32_t ee_tcp_receive(void** connection, uint32_t size, uint8_t* buffer, uint32_t* received)
{
    Line("  receive h%d %u", Id(*connection), size);
    return Receive("receive", size, buffer, received);
}
int32_t ee_tcp_receive_from(void** server, uint32_t size, uint8_t* buffer, uint32_t* received, uint32_t* index)
{
    Line("  receive_from h%d %u in %u", Id(*server), size, *index);
    int32_t r = Receive("receive_from", size, buffer, received);
    if (r == 0)
        *index = Next() % 8;
    return r;
}
void ee_tcp_reset_client_connection(void** server, uint32_t index)
{
    Line("  reset_client_connection h%d %u", Id(*server), index);
}
void ee_tcp_reset_xfer(void** server)
{
    Line("  reset_xfer h%d", Id(*server));
}
int32_t ee_tcp_search_for_game(const uint8_t* name, size_t length, bool start)
{
    int32_t r = Answer();
    Line("  search_for_game %s %d = %d", Text(name, length).c_str(), start, r);
    return r;
}
int32_t ee_tcp_send(void** connection, uint8_t* data, uint32_t length)
{
    int32_t r = Answer();
    Line("  send h%d %s = %d", Id(*connection), Hex(data, length).c_str(), r);
    return r;
}
int32_t ee_tcp_send_single(const uint8_t* host, size_t host_length, uint32_t port, const uint8_t* data,
                           uint32_t length)
{
    int32_t r = Answer();
    // the plug-in takes the port as 16 bits (the original passed an unsigned short through an unsigned long
    // parameter: the upper bits were whatever the register held)
    Line("  send_single %s %u %s = %d", Text(host, host_length).c_str(), port & 0xffff, Hex(data, length).c_str(), r);
    return r;
}
int32_t ee_tcp_send_to(void** server, uint32_t index, uint8_t* data, uint32_t length)
{
    int32_t r = Answer();
    Line("  send_to h%d %u %s = %d", Id(*server), index, Hex(data, length).c_str(), r);
    return r;
}
int32_t ee_tcp_xfer_receive(uint32_t size, uint8_t* buffer, uint32_t* received)
{
    Line("  xfer_receive %u", size);
    return Receive("xfer_receive", size, buffer, received);
}
int32_t ee_tcp_xfer_send_to(void** server, uint32_t index, const uint8_t* data, uint32_t length)
{
    int32_t r = Answer();
    Line("  xfer_send_to h%d %u %s = %d", Id(*server), index, Hex(data, length).c_str(), r);
    return r;
}
}

// ---- the sequence

static uint32_t sStep = 7;
static uint32_t Below(uint32_t inN)
{
    sStep = sStep * 1664525u + 1013904223u;
    return (sStep >> 8) % inN;
}

static void Run(int inSteps)
{
#ifdef NE_PROTOCOL_ORIG
    const PIMPlugin* thePlugin = &gPIMPluginTCPProtocol;
    // a module of the plug-in, laid out as PIMModule (its constructor is PIMModule.cpp's, private)
    struct Module {
        void* mGetName;
        void* mGetType;
        void* mIsValid;
        const PIMPlugin* mPlugin;
    };
    Module* theFields = (Module*)operator new(sizeof(PIMModule));
    theFields->mGetName = PIMFind(thePlugin, "PIMGetName");
    theFields->mGetType = PIMFind(thePlugin, "PIMGetType");
    theFields->mIsValid = PIMFind(thePlugin, "PIMIsValid");
    theFields->mPlugin = thePlugin;
    Line("new");
    Proto* p = NESynchronousServer::New((PIMModule*)theFields);
#else
    Line("new");
    Proto* p = NESynchronousServer::New();
#endif
    Line("count %lu name \"%s\" type %lu", NEProtocol::GetCount(), (const char*)p->GetName(), p->GetProtocolType());
    TSEvent* theEvents[3];
    for (int i = 0; i < 3; i++) {
        TSScheduler::CreateEvent(theEvents[i]);
        Id(theEvents[i]->GetHandle());
    }
    std::vector<NEServer*> theServers;
    std::vector<NEConnection*> theConnections;
    static const char* kNames[] = {"EEService", "", "a game"};
    for (int s = 0; s < inSteps; s++) {
        uint32_t theWhat = Below(26);
        unsigned char theBuffer[64];
        unsigned long theReceived = Below(5), theIndex = Below(9);
        long r = 0;
        NEServer* theServer = theServers.empty() ? NULL : theServers[Below((uint32_t)theServers.size())];
        NEConnection* theConnection =
            theConnections.empty() ? NULL : theConnections[Below((uint32_t)theConnections.size())];
        std::vector<unsigned char> theData(Below(6));
        for (auto& b : theData)
            b = (unsigned char)Below(256);
        switch (theWhat) {
        case 0: {
            NEServer* theNew = NULL;
            UWideString theAddress(kNames[Below(3)]);
            Line("publish");
            r = p->Publish(kNames[Below(3)], theAddress, Below(4), theEvents[Below(3)], theEvents[Below(3)],
                           theEvents[Below(3)], theNew, Below(2) != 0, Below(2) != 0, Below(3));
            Line("= %ld %d", r, theNew != NULL);
            if (theNew != NULL)
                theServers.push_back(theNew);
            continue;
        }
        case 1: {
            NEConnection* theNew = NULL;
            Line("connect");
            r = p->Connect(kNames[Below(3)], kNames[Below(3)], UWideString(kNames[Below(3)]), Below(4),
                           theEvents[Below(3)], theNew, Below(9000));
            Line("= %ld %d", r, theNew != NULL);
            if (theNew != NULL)
                theConnections.push_back(theNew);
            continue;
        }
        case 2:
        case 3: {
            if (theServer == NULL)
                continue;
            NEConnection* theNew = NULL;
            Line(theWhat == 2 ? "accept" : "accept_for_xfer");
            r = theWhat == 2 ? p->Accept(theServer, theNew) : p->AcceptForXfer(theServer, theNew);
            Line("= %ld %d", r, theNew != NULL);
            if (theNew != NULL)
                theConnections.push_back(theNew);
            continue;
        }
        case 4: {
            if (theConnection == NULL)
                continue;
            NEConnection* theNew = NULL;
            Line("connect_for_xfer");
            r = p->ConnectForXfer(theConnection, Below(3), theEvents[Below(3)], theNew);
            Line("= %ld %d", r, theNew != NULL);
            if (theNew != NULL)
                theConnections.push_back(theNew);
            continue;
        }
        case 5:
            if (theConnection == NULL)
                continue;
            Line("close connection");
            r = p->Close(theConnection);
            if (r == kErrorOK)
                for (size_t i = 0; i < theConnections.size(); i++)
                    if (theConnections[i] == theConnection) {
                        theConnections.erase(theConnections.begin() + i);
                        break;
                    }
            break;
        case 6:
            if (theServer == NULL)
                continue;
            Line("close server");
            r = p->Close(theServer);
            if (r == kErrorOK)
                for (size_t i = 0; i < theServers.size(); i++)
                    if (theServers[i] == theServer) {
                        theServers.erase(theServers.begin() + i);
                        break;
                    }
            break;
        case 7:
            if (theServer == NULL)
                continue;
            Line("allow");
            p->AllowNewConnections(theServer, Below(2) != 0);
            break;
        case 8:
            if (theConnection == NULL)
                continue;
            Line("receive in %lu", theReceived);
            r = p->Receive(theConnection, Below(65), theBuffer, theReceived);
            Line("received %lu", theReceived);
            break;
        case 9:
            if (theServer == NULL)
                continue;
            Line("receive_from in %lu %lu", theReceived, theIndex);
            r = p->ReceiveFrom(theServer, Below(65), theBuffer, theReceived, theIndex);
            Line("received %lu %lu", theReceived, theIndex);
            break;
        case 10:
            Line("xfer_receive in %lu", theReceived);
            r = p->XferReceive(Below(65), theBuffer, theReceived);
            Line("received %lu", theReceived);
            break;
        case 11:
            if (theConnection == NULL)
                continue;
            Line("send");
            r = p->Send(theConnection, theData.data(), (unsigned long)theData.size());
            break;
        case 12:
            if (theServer == NULL)
                continue;
            Line("send_to");
            r = p->SendTo(theServer, Below(8), theData.data(), (unsigned long)theData.size());
            break;
        case 13:
            if (theServer == NULL)
                continue;
            Line("xfer_send_to");
            r = p->XferSendTo(theServer, Below(8), theData.data(), (unsigned long)theData.size());
            break;
        case 14:
            Line("send_single");
            r = p->SendSingle(kNames[Below(3)], (unsigned short)Below(65536), theData.data(),
                              (unsigned long)theData.size());
            break;
        case 15:
            if (theConnection == NULL)
                continue;
            Line("is_alive");
            r = p->IsAlive(theConnection);
            break;
        case 16: {
            if (theServer == NULL)
                continue;
            UWideString theAddress("before");
            Line("client_address");
            r = p->GetClientAddress(theServer, Below(8), theAddress);
            Line("address \"%s\"", (const char*)theAddress);
            break;
        }
        case 17: {
            UString theName("n"), theHost("h");
            UWideString theAddress("a");
            Line("detected_game");
            r = p->GetDetectedGame(Below(5), theName, theHost, theAddress);
            Line("game \"%s\" \"%s\" \"%s\"", (const char*)theName, (const char*)theHost, (const char*)theAddress);
            break;
        }
        case 18:
            Line("detected_game_count");
            r = (long)p->GetDetectedGameCount();
            break;
        case 19: {
            Line("local_address");
            UWideString& theAddress = p->GetLocalAddress(Below(3));
            Line("address \"%s\"", (const char*)theAddress);
            break;
        }
        case 20: {
            std::vector<UWideString> theAdapters(Below(3), UWideString("old"));
            Line("adapters");
            r = (long)p->EnumerateNetworkAdapters(theAdapters);
            for (auto& a : theAdapters)
                Line("adapter \"%s\"", (const char*)a);
            break;
        }
        case 21:
            Line("status");
            r = p->GetStatus(UWideString(kNames[Below(3)]), Below(9));
            break;
        case 22:
            if (theServer == NULL)
                continue;
            Line("reset_client");
            p->ResetClientConnection(theServer, Below(8));
            break;
        case 23:
            if (theServer == NULL)
                continue;
            Line("reset_xfer");
            NESynchronousServer::ResetXfer(p, theServer);
            break;
        case 24:
            Line("search");
            r = p->SearchForGame(kNames[Below(3)], kNames[Below(3)], Below(2) != 0);
            break;
        default: {
            NEProtocol* theFound = NULL;
            r = NEProtocol::GetProtocol(Below(3), theFound);
            Line("get_protocol = %ld %d", r, SameProtocol(theFound, p));
            continue;
        }
        }
        Line("= %ld sent %lu received %lu", r, p->GetBytesSent(), p->GetBytesReceived());
    }
    Line("free");
    NESynchronousServer::Delete(p);
    Line("count %lu", NEProtocol::GetCount());
    for (int i = 0; i < 3; i++)
        TSScheduler::DestroyEvent(theEvents[i]);
}

int main(int argc, char** argv)
{
    const char* thePrint = NULL;
    const char* theGolden = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--golden") == 0 && i + 1 < argc)
            theGolden = argv[++i];
    }
    for (int s = 0; s < 40; s++) {
        Line("seq %d", s + 1);
        sStep = (uint32_t)s * 2654435761u + 7;
        sRandom = (uint32_t)s + 1;
        sIds.clear();
        sNextId = 0;
        sOpened = 0;
        Run(150);
    }
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(sTrace.data(), 1, sTrace.size(), theFile);
        fclose(theFile);
    }
    size_t theLines = 0;
    for (char c : sTrace)
        theLines += c == '\n';
    // by default the golden next to this source
    std::string theDefault = __FILE__;
    theDefault = theDefault.substr(0, theDefault.find_last_of("/\\") + 1) + "golden_protocol.txt";
    if (theGolden == NULL && thePrint == NULL)
        theGolden = theDefault.c_str();
    if (theGolden == NULL) {
        printf("ne_protocol_test: %zu trace lines written\n", theLines);
        return 0;
    }
    FILE* theFile = fopen(theGolden, "rb");
    if (theFile == NULL) {
        printf("ne_protocol_test: no golden %s\n", theGolden);
        return 1;
    }
    std::string theWant;
    char theChunk[65536];
    size_t n;
    while ((n = fread(theChunk, 1, sizeof(theChunk), theFile)) > 0)
        theWant.append(theChunk, n);
    fclose(theFile);
    size_t i = 0;
    while (i < theWant.size() && i < sTrace.size() && theWant[i] == sTrace[i])
        i++;
    if (theWant == sTrace) {
        printf("ne_protocol_test: %zu trace lines, equal to the golden\n", theLines);
        return 0;
    }
    size_t theStart = theWant.rfind('\n', i == 0 ? 0 : i - 1);
    theStart = theStart == std::string::npos ? 0 : theStart + 1;
    printf("ne_protocol_test: differs from the golden at byte %zu:\n  golden: %.300s\n  now:    %.300s\n", i,
           theWant.c_str() + theStart, sTrace.c_str() + theStart);
    return 1;
}
