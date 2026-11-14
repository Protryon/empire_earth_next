// NESynchronousServer as a whole Rust object (LLE Network Engine round 4): the original unit, unmodified
// (orig_s/NESynchronousServer.cpp with its header, as the round found it: its message manager, tuning and recorder
// already ee_net's Rust objects behind their C ABI), compiled over the stand-in headers of orig_s/lle (the clock,
// events, threads, messages, the protocol and its published server, the transferred files of one world), against
// ee_net's Rust server (server.rs) whose three host op tables answer with the same world objects, as
// NESynchronousServer.cpp's hosts answer with the game's.
//
// The server's main, send and transfer threads are real threads that run one at a time (orig_q/lle/q_world.h: the
// world hands the baton on by a fixed rule), so both sides interleave the same way. Each sequence makes a server
// (Publish sometimes failing) and the same random calls on it: time passing (the send thread's ticks, the main
// thread's waits and retries), the clients' packets (tick responses with inputs, pause answers, checksums, connection,
// disconnection and reconnection requests, connection checks, inputs, bytes naming no message), the server's own
// inputs, start, stop, pause, unpause and send-to-all-and-wait, the base class's fields, the setters, transfers queued
// and aborted, broadcasts, sends, removals, reconnect waits, pings. Each side's world prints a line for every effect
// and the server's state after every step; the traces must be the same. At the end the threads are stopped, the
// server freed, and the messages left (the original's leaks) listed.
//
// The Rust side's hosts also record each op with its arguments and answer (messages, events, threads, the server and
// files by world id), the API calls and the thread switches: the first sequences are
// rust/crates/ee_net/src/golden_server.txt, which cargo replays against the Rust alone (`--print FILE` writes it).
#define _ALLOW_KEYWORD_MACROS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "orig_q/lle/q_world.h"
QWorld* gQ;
#include "orig_s/lle/s_world.h"
SNet* gS;
bool gPlatSimMode = false;
static bool sVerbose = false;
static bool sProgress = false;   // --progress: each sequence's number to stderr as it starts
#define private public
#define protected public
#include "orig_s/NESynchronousServer.cpp"
#undef private
#undef protected
#include "rust/ee_net.h"

int QWorld::IdOf(NEMessage* inMessage)
{
    return inMessage != NULL ? inMessage->mID : 0;
}

// ---- the queues' common calls out for the original server (its recorder passes messages on)

ptrdiff_t NESynchronousCommunications::QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    switch (inOp) {
    case EeNetQOp::Time:
        return (ptrdiff_t)TMGetTime();
    case EeNetQOp::Verify:
        gQ->Line("verify %d", (int)b);
        return 0;
    default:
        gQ->Line("unexpected queue op %d", (int)inOp);
        return 0;
    }
}

// ---- the game's side of the server (its virtual members): the same on both sides

static void VBootPlayer(unsigned char inSlot, bool inBooted, bool inExiting)
{
    gQ->Line("v bootplayer %d %d %d", inSlot, (int)inBooted, (int)inExiting);
}
static bool VServerSlot28()
{
    bool b = gQ->Next() % 5 == 0;
    gQ->Line("v slot28 = %d", (int)b);
    return b;
}
static bool VIsMigrating()
{
    bool b = gQ->Next() % 12 == 0;
    gQ->Line("v migrating = %d", (int)b);
    return b;
}
static void VBeforeRemovingPlayers() { gQ->Line("v beforeremoving"); }
static void VHandleReconnectWaitTimedOut() { gQ->Line("v reconnecttimedout"); }
static void VHandleXferStatus(unsigned char inSlot, unsigned char inPercent, uint32_t inID, unsigned char inStatus)
{
    gQ->Line("v xferstatus %d %d %u %d", inSlot, inPercent, inID, inStatus);
}
static unsigned char VGetPlayerSendToSlot(unsigned char inPlayer)
{
    unsigned char s = gQ->Next() % 4 == 0 ? (unsigned char)0xff : (unsigned char)(inPlayer % 4);
    gQ->Line("v sendtoslot %d = %d", inPlayer, s);
    return s;
}
static bool VFilterPlayerInput(NEMessage* inMessage)
{
    bool b = gQ->Next() % 6 != 0;
    gQ->Line("v filter m%d = %d", inMessage->mID, (int)b);
    return b;
}
static void VPrepareLocalInput(NEMessage* inMessage) { gQ->Line("v prepare m%d", inMessage->mID); }
static bool VIsPlayerPresent(unsigned char inPlayer)
{
    bool b = gQ->Next() % 4 != 0;
    gQ->Line("v present %d = %d", inPlayer, (int)b);
    return b;
}
static void VReportLaggers(const std::vector<unsigned char>& inSlots, bool inFlag)
{
    gQ->Line("v reportlaggers %d %s", (int)inFlag, SHex(inSlots.data(), inSlots.size()).c_str());
}
// takes a client's connection request and disconnection notification (the server keeps the others)
static void VProcessSystemMessage(NEMessage* inMessage)
{
    gQ->Line("v systemmessage m%d %02x", inMessage->mID, inMessage->mType);
    if ((inMessage->mType == kNetMessageClientConnectionRequest ||
         inMessage->mType == kNetMessageClientDisconnectionNotification) && inMessage->mFromSlot != kSlotServer)
        inMessage->Dereference();
}

class OrigServer : public NESynchronousServer {
public:
    OrigServer(unsigned char inMaxPlayers, unsigned long inInterval, unsigned char inPTs, unsigned long inMax,
               unsigned long inMin, float inFactor, unsigned long inMaxInterval, unsigned long inIncrement,
               unsigned long inDecrement, unsigned long inCT, unsigned long inPT, UWideString& inName, bool inAllow,
               bool inAnnounce, unsigned long inAdapter)
        : NESynchronousServer(inMaxPlayers, inInterval, inPTs, inMax, inMin, inFactor, inMaxInterval, inIncrement,
                              inDecrement, inCT, inPT, inName, inAllow, inAnnounce, inAdapter)
    {
    }
    virtual void ExecuteRemovePlayer() {}
    virtual void IsClient() {}
    virtual void IsServer() {}
    virtual void BootPlayer(unsigned char inSlot, bool inBooted, bool inExiting) { VBootPlayer(inSlot, inBooted, inExiting); }
    virtual bool ServerSlot28() { return VServerSlot28(); }
    virtual bool IsMigrating() { return VIsMigrating(); }
    virtual void BeforeRemovingPlayers() { VBeforeRemovingPlayers(); }
    virtual void HandleReconnectWaitTimedOut() { VHandleReconnectWaitTimedOut(); }
    virtual void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercent, uint32_t& inID, unsigned char& inStatus)
    {
        VHandleXferStatus(inSlot, inPercent, inID, inStatus);
    }
    virtual void PlayWarningSound() {}
    virtual void ServerSlot48() {}
    virtual void GetFreePlayerSlot() {}
    virtual void GetDebugOOSHandling() {}
    virtual void ServerSlot54() {}
    virtual unsigned char GetPlayerSendToSlot(unsigned char inPlayer) { return VGetPlayerSendToSlot(inPlayer); }
    virtual bool FilterPlayerInput(NEMessage* inMessage) { return VFilterPlayerInput(inMessage); }
    virtual void PrepareLocalInput(NEMessage* inMessage) { VPrepareLocalInput(inMessage); }
    virtual bool IsPlayerPresent(unsigned char inPlayer) { return VIsPlayerPresent(inPlayer); }
    virtual void ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag) { VReportLaggers(inSlots, inFlag); }
    virtual void ProcessSystemMessage(NEMessage* inMessage) { VProcessSystemMessage(inMessage); }
};

// ---- the Rust side's hosts (each op the statement NESynchronousServer.cpp's hosts make, on the world)

static EeNetComms* sComms;   // the Rust server's base class fields (ee_net comms.rs), as NESynchronousCommunications holds them
static uint32_t BGet(EeNetCommsField f) { return ee_net_comms_get(sComms, f); }
static void BSet(EeNetCommsField f, unsigned long v) { ee_net_comms_set(sComms, f, (uint32_t)v); }
static EeNetServer* sRust;
static UWideString sName("host");
static bool sAnnounce;
static unsigned long sAdapter;

static std::string Tok(char inKind, int inID) { return std::string(1, inKind) + std::to_string(inID); }
static std::string MsgTok(size_t m) { return Tok('m', gQ->IdOf((NEMessage*)m)); }
static std::string EvTok(size_t e) { return Tok('e', e ? ((TSEvent*)e)->mID : 0); }
static std::string ThTok(size_t t) { return Tok('t', t ? ((TSThread*)t)->mID : 0); }
static std::string SvTok(size_t s) { return Tok('v', s ? ((NEServer*)s)->mID : 0); }
static std::string FileTok(size_t f) { return Tok('f', f ? ((FSFileSpec*)f)->mTok : 0); }

static unsigned long RustMainThread(void* p) { return ee_net_sv_main_thread((const EeNetServer*)p); }
static unsigned long RustSendThread(void* p) { return ee_net_sv_send_thread((const EeNetServer*)p); }
static unsigned long RustXferThread(void* p) { return ee_net_sv_xfer_thread((const EeNetServer*)p); }

// the op line before the call (the record's nested ops and switches come after it), the answer after
static void OpLine(const std::string& inArgs) { gQ->Op("op " + inArgs); }
static ptrdiff_t Answer(ptrdiff_t inResult, const std::string& inAnswer = std::string())
{
    gQ->Op("= " + (inAnswer.empty() ? std::to_string((long long)inResult) : inAnswer));
    return inResult;
}

static ptrdiff_t RustQHost(void*, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    std::string theArgs = std::to_string((int)inOp);
    NEMessage* m = (NEMessage*)a;
    switch (inOp) {
    case EeNetQOp::Time:
        OpLine(theArgs);
        return Answer((ptrdiff_t)TMGetTime());
    case EeNetQOp::Verify:
        OpLine(theArgs + " " + std::to_string(a) + " " + std::to_string(b));
        gQ->Line("verify %d", (int)b);
        return Answer(0);
    case EeNetQOp::FpuStart:
        OpLine(theArgs);
        UMSetFloatingPointPrecision(kFPPrecision53);
        return Answer(0);
    case EeNetQOp::FpuCheck:
        OpLine(theArgs);
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        return Answer(0);
    case EeNetQOp::EventNew: {
        OpLine(theArgs);
        TSEvent* e = NULL;
        TSScheduler::CreateEvent(e);
        return Answer((ptrdiff_t)e, EvTok((size_t)e));
    }
    case EeNetQOp::EventFree:
        OpLine(theArgs + " " + EvTok(a));
        TSScheduler::DestroyEvent((TSEvent*)a);
        return Answer(0);
    case EeNetQOp::EventSet:
        OpLine(theArgs + " " + EvTok(a));
        ((TSEvent*)a)->Set();
        return Answer(0);
    case EeNetQOp::EventClear:
        OpLine(theArgs + " " + EvTok(a));
        ((TSEvent*)a)->Clear();
        return Answer(0);
    case EeNetQOp::EventWait:
        OpLine(theArgs + " " + EvTok(a) + " " + std::to_string(b));
        return Answer(((TSEvent*)a)->Wait((unsigned long)b));
    case EeNetQOp::ThreadNew: {
        OpLine(theArgs + " " + std::to_string(a));
        TSThread* t = NULL;
        long r = a == 7 ? TSScheduler::CreateThread(t, RustMainThread, (void*)b)
               : a == 8 ? TSScheduler::CreateThread(t, RustSendThread, (void*)b)
               : a == 9 ? TSScheduler::CreateThread(t, RustXferThread, (void*)b)
                        : (gQ->Line("unexpected thread kind %d", (int)a), 1);
        *(TSThread**)c = t;
        return Answer(r, ThTok((size_t)t) + " " + std::to_string(r));
    }
    case EeNetQOp::ThreadName:
        OpLine(theArgs + " " + ThTok(a) + " " + std::string((const char*)b, c));
        ((TSThread*)a)->SetName(UString(std::string((const char*)b, c).c_str()));
        return Answer(0);
    case EeNetQOp::ThreadFree:
        OpLine(theArgs + " " + ThTok(a));
        TSScheduler::DestroyThread((TSThread*)a);
        return Answer(0);
    case EeNetQOp::MsgReference:
        OpLine(theArgs + " " + MsgTok(a));
        m->Reference();
        return Answer(0);
    case EeNetQOp::MsgDereference:
        OpLine(theArgs + " " + MsgTok(a));
        m->Dereference();
        return Answer(0);
    case EeNetQOp::MsgExportSize:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer((ptrdiff_t)m->ExportSize());
    case EeNetQOp::MsgFromBytes: {
        OpLine(theArgs + " " + SHex((const unsigned char*)a, 4));
        NEMessage* theNew = NEMessage::ConvertMemoryToMessage((unsigned char*)a);
        return Answer((ptrdiff_t)theNew, MsgTok((size_t)theNew));
    }
    default:
        OpLine(theArgs);
        gQ->Line("unexpected queue op %d", (int)inOp);
        return Answer(0);
    }
}

static ptrdiff_t RustSvHost(void*, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    std::string theArgs = "s" + std::to_string((int)inOp);
    NEMessage* m = (NEMessage*)a;
    if (getenv("NE_SV_OPS"))
        fprintf(stderr, "sv op %d %zx %zx %zx\n", (int)inOp, a, b, c);
    NEProtocol* p;
    NEProtocol::GetProtocol(0, p);
    switch (inOp) {
    case EeNetSvOp::WaitAny: {
        std::string theList;
        for (size_t i = 0; i < b; i++)
            theList += (i ? "," : "") + EvTok((size_t)((TSEvent* const*)a)[i]);
        OpLine(theArgs + " " + theList + " " + std::to_string(c));
        return Answer(TSEvent::WaitAny((TSEvent* const*)a, (unsigned long)b, (unsigned long)c));
    }
    case EeNetSvOp::ThreadPriority:
        OpLine(theArgs + " " + ThTok(a));
        ((TSThread*)a)->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
        return Answer(0);
    case EeNetSvOp::Sleep:
        OpLine(theArgs + " " + std::to_string(a));
        PlatSleep((unsigned long)a);
        return Answer(0);
    case EeNetSvOp::SimMode:
        OpLine(theArgs);
        return Answer(gPlatSimMode);
    case EeNetSvOp::ProtocolCount:
        OpLine(theArgs);
        return Answer((ptrdiff_t)NEProtocol::GetCount());
    case EeNetSvOp::ProtocolGet:
        OpLine(theArgs);
        return Answer(NEProtocol::GetProtocol(0, *(NEProtocol**)a), "0 p");
    case EeNetSvOp::Publish: {
        TSEvent* const* e = (TSEvent* const*)b;
        OpLine(theArgs + " p " + EvTok((size_t)e[0]) + "," + EvTok((size_t)e[1]) + "," + EvTok((size_t)e[2]) + " " +
               std::to_string(d));
        long r = p->Publish("EEService", sName, 2, e[0], e[1], e[2], *(NEServer**)c, d != 0, sAnnounce, sAdapter);
        return Answer(r, std::to_string(r) + " " + SvTok(r == 0 ? *(size_t*)c : 0));
    }
    case EeNetSvOp::AllowNewConnections:
        OpLine(theArgs + " p " + SvTok(b) + " " + std::to_string(c));
        p->AllowNewConnections((NEServer*)b, c != 0);
        return Answer(0);
    case EeNetSvOp::AcceptForXfer: {
        OpLine(theArgs + " p " + SvTok(b));
        NEConnection* theConnection;
        p->AcceptForXfer((NEServer*)b, theConnection);
        return Answer(0);
    }
    case EeNetSvOp::SearchForGame: {
        OpLine(theArgs + " p " + std::to_string(b));
        UString theName("EEService");
        UString theHost("Anything");
        return Answer(p->SearchForGame(theHost, theName, b != 0));
    }
    case EeNetSvOp::ServerSendTo:
        OpLine(theArgs + " " + SvTok(a) + " " + std::to_string(b) + " " + SHex((const unsigned char*)c, d));
        return Answer(((NEServer*)a)->SendTo(b, (const unsigned char*)c, (unsigned long)d));
    case EeNetSvOp::ServerReceiveFrom: {
        OpLine(theArgs + " " + SvTok(a));
        unsigned long theSize = 0, theIndex = 0;
        long r = ((NEServer*)a)->ReceiveFrom(0x200, (unsigned char*)b, theSize, theIndex);
        *(uint32_t*)c = (uint32_t)theSize;
        *(uint32_t*)d = (uint32_t)theIndex;
        return Answer(r, std::to_string(r) + " " + std::to_string(theSize) + " " + std::to_string(theIndex) + " " +
                             SHex((const unsigned char*)b, 4));
    }
    case EeNetSvOp::ServerClose:
        OpLine(theArgs + " " + SvTok(a));
        ((NEServer*)a)->Close();
        return Answer(0);
    case EeNetSvOp::MsgType:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer(m->GetType());
    case EeNetSvOp::MsgSetSlot:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b) + " " + std::to_string(c));
        if (b != 0)
            m->SetOriginatingSlot((unsigned char)c);
        else
            m->SetForSpecificSlot((unsigned char)c);
        return Answer(0);
    case EeNetSvOp::MsgFlag:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        return Answer((EeNetSvMsgFlag)b == EeNetSvMsgFlag::CanSendWhilePaused ? m->GetCanSendWhilePaused()
                                                                                : m->GetIsAvailableToSend());
    case EeNetSvOp::MsgGet: {
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        ptrdiff_t r = 0;
        switch ((EeNetSvMsgField)b) {
        case EeNetSvMsgField::OriginatingSlot: r = m->GetOriginatingSlot(); break;
        case EeNetSvMsgField::NumberOfPlayerInputs: r = (uint32_t)((NEMCommunicationsTickResponse*)m)->GetNumberOfPlayerInputs(); break;
        case EeNetSvMsgField::LastCtProcessed: r = (uint32_t)((NEMCommunicationsTickResponse*)m)->GetLastCommunicationsTickProcessed(); break;
        case EeNetSvMsgField::AveragePhysicsTickTime: r = (uint32_t)((NEMCommunicationsTickResponse*)m)->GetAveragePhysicsTickTime(); break;
        case EeNetSvMsgField::ReconnectionCt: r = (uint32_t)((NEMClientReconnectionRequest*)m)->GetCurrentCT(); break;
        case EeNetSvMsgField::ChecksumTick: r = (uint32_t)((NEMWorldDatabaseChecksumResponse*)m)->GetCommunicationsTick(); break;
        case EeNetSvMsgField::Checksum: r = (uint32_t)((NEMWorldDatabaseChecksumResponse*)m)->GetWorldDatabaseChecksum(); break;
        }
        return Answer(r);
    }
    case EeNetSvOp::MsgNew: {
        OpLine(theArgs + " " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c) + " " +
               std::to_string(d));
        NEMessage* theNew = NULL;
        switch ((EeNetSvNewMsg)a) {
        case EeNetSvNewMsg::RemovePlayer: {
            unsigned char thePlayer = (unsigned char)b;
            theNew = new NEMRemovePlayer(thePlayer, (c & 1) != 0, (c & 2) != 0, (c & 4) != 0);
            break;
        }
        case EeNetSvNewMsg::PauseGame: theNew = new NEMPauseGame((unsigned long)b); break;
        case EeNetSvNewMsg::UnPauseGame: theNew = new NEMUnPauseGame((unsigned long)b); break;
        case EeNetSvNewMsg::ServerIsAlive: theNew = new NEMServerIsAlive(); break;
        case EeNetSvNewMsg::StopGame: theNew = new NEMStopGame(); break;
        case EeNetSvNewMsg::PlayerConnectionCheck: theNew = new NEMPlayerConnectionCheck((unsigned long)b); break;
        case EeNetSvNewMsg::CommunicationsTick:
            theNew = new NEMCommunicationsTick((unsigned long)b, (unsigned char)c, (unsigned char)(c >> 8),
                                               ((c >> 16) & 1) != 0, d != 0);
            break;
        case EeNetSvNewMsg::ReconnectionRequestResponse:
            theNew = new NEMClientReconnectionRequestResponse((long)(int32_t)b, (unsigned char)c, (unsigned long)d);
            break;
        }
        return Answer((ptrdiff_t)theNew, MsgTok((size_t)theNew));
    }
    case EeNetSvOp::MsgTickResponseInput: {
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        NEMessage* theInput = NULL;
        long r = ((NEMCommunicationsTickResponse*)m)->GetPlayerInput((unsigned long)b, theInput);
        *(NEMessage**)c = theInput;
        return Answer(r, std::to_string(r) + " " + MsgTok((size_t)(r == 0 ? theInput : NULL)));
    }
    case EeNetSvOp::MsgAddInput:
        OpLine(theArgs + " " + MsgTok(a) + " " + MsgTok(b));
        return Answer(((NEMCommunicationsTick*)m)->AddPlayerInput((NEMessage*)b));
    case EeNetSvOp::MsgSetS2awId:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        m->SetSendToAllAndWaitMessageID((unsigned long)b);
        return Answer(0);
    case EeNetSvOp::MsgExportNew: {
        OpLine(theArgs + " " + MsgTok(a));
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        m->ExportMessage(&theData, theSize);
        *(uint32_t*)b = (uint32_t)theSize;
        return Answer((ptrdiff_t)theData, std::to_string(theSize) + " " + SHex(theData, theSize));
    }
    case EeNetSvOp::BytesFree:
        OpLine(theArgs);
        delete[] (unsigned char*)a;
        return Answer(0);
    case EeNetSvOp::BootPlayer:
        OpLine(theArgs + " " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c));
        VBootPlayer((unsigned char)a, b != 0, c != 0);
        return Answer(0);
    case EeNetSvOp::ServerSlot28:
        OpLine(theArgs);
        return Answer(VServerSlot28());
    case EeNetSvOp::IsMigrating:
        OpLine(theArgs);
        return Answer(VIsMigrating());
    case EeNetSvOp::BeforeRemovingPlayers:
        OpLine(theArgs);
        VBeforeRemovingPlayers();
        return Answer(0);
    case EeNetSvOp::HandleReconnectWaitTimedOut:
        OpLine(theArgs);
        VHandleReconnectWaitTimedOut();
        return Answer(0);
    case EeNetSvOp::PlayerSendToSlot:
        OpLine(theArgs + " " + std::to_string(a));
        return Answer(VGetPlayerSendToSlot((unsigned char)a));
    case EeNetSvOp::FilterPlayerInput:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer(VFilterPlayerInput(m));
    case EeNetSvOp::PrepareLocalInput:
        OpLine(theArgs + " " + MsgTok(a));
        VPrepareLocalInput(m);
        return Answer(0);
    case EeNetSvOp::IsPlayerPresent:
        OpLine(theArgs + " " + std::to_string(a));
        return Answer(VIsPlayerPresent((unsigned char)a));
    case EeNetSvOp::ReportLaggers: {
        OpLine(theArgs + " " + SHex((const unsigned char*)a, b) + " " + std::to_string(c));
        std::vector<unsigned char> theSlots((const unsigned char*)a, (const unsigned char*)a + b);
        VReportLaggers(theSlots, c != 0);
        return Answer(0);
    }
    case EeNetSvOp::ProcessSystemMessage:
        OpLine(theArgs + " " + MsgTok(a));
        VProcessSystemMessage(m);
        return Answer(0);
    case EeNetSvOp::ProcessLocalUserInput:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        return Answer(ee_net_sv_process_local_user_input(sRust, m, (uint8_t)b));
    }
    OpLine(theArgs);
    return Answer(0);
}

// the message manager's calls out that reach the C++ (NESynchronousServer::MessageManagerHost's, on the world)
static ptrdiff_t RustMmHost(void*, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    std::string theArgs = "x" + std::to_string((int)inOp);
    NEMessage* theMessage = (NEMessage*)a;
    FSFileSpec* theFile = (FSFileSpec*)a;
    NEServer* theServer = (NEServer*)ee_net_sv_server(sRust);
    switch (inOp) {
    case EeNetMmOp::MsgReference:
        OpLine(theArgs + " " + MsgTok(a));
        theMessage->Reference();
        return Answer(0);
    case EeNetMmOp::MsgDereference:
        OpLine(theArgs + " " + MsgTok(a));
        theMessage->Dereference();
        return Answer(0);
    case EeNetMmOp::MsgType:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer(theMessage->GetType());
    case EeNetMmOp::MsgIsResponse:
        OpLine(theArgs + " " + MsgTok(a) + " " + MsgTok(b));
        return Answer(theMessage->IsResponse((NEMessage*)b));
    case EeNetMmOp::MsgSlot:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer(theMessage->GetOriginatingSlot());
    case EeNetMmOp::MsgSendTo: {
        OpLine(theArgs + " " + std::to_string(a) + " " + MsgTok(b));
        unsigned char* theData;
        unsigned long theSize;
        ((NEMessage*)b)->ExportMessage(&theData, theSize);
        long r = theServer->SendTo(a, theData, theSize);
        delete[] theData;
        return Answer((uint32_t)r);
    }
    case EeNetMmOp::MsgTick:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer((ptrdiff_t)((NEMCommunicationsTick*)theMessage)->GetCurrentCommunicationsTick());
    case EeNetMmOp::NewResponse: {
        OpLine(theArgs + " " + std::to_string(a) + " " + MsgTok(b));
        NEMessage* theSent = (NEMessage*)b;
        NEMessage* theResponse;
        if (a == kNetMessageCommunicationsTick)
            theResponse = new NEMCommunicationsTickResponse(BGet(EeNetCommsField::CtsProcessed),
                                                            ((NEMCommunicationsTick*)theSent)->GetCurrentCommunicationsTick(),
                                                            BGet(EeNetCommsField::AveragePhysicsUpdateTime));
        else if (a == kNetMessageUnPauseGame)
            theResponse = new NEMUnPauseGameResponse(((NEMUnPauseGame*)theSent)->GetUniqueID());
        else
            theResponse = new NEMPauseGameResponse(((NEMPauseGame*)theSent)->GetUniqueID());
        theResponse->SetOriginatingSlot(kSlotServer);
        return Answer((ptrdiff_t)theResponse, MsgTok((size_t)theResponse));
    }
    case EeNetMmOp::ProcessSystemMessage:
        OpLine(theArgs + " " + MsgTok(a));
        VProcessSystemMessage(theMessage);
        return Answer(0);
    case EeNetMmOp::ResetClientConnection: {
        OpLine(theArgs + " " + std::to_string(a));
        NEProtocol* p;
        NEProtocol::GetProtocol(0, p);
        p->ResetClientConnection(theServer, a);
        return Answer(0);
    }
    case EeNetMmOp::XferStatus:
        OpLine(theArgs + " " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c) + " " + std::to_string(d));
        VHandleXferStatus((unsigned char)a, (unsigned char)b, (uint32_t)c, (unsigned char)d);
        return Answer(0);
    case EeNetMmOp::XferSend: {
        OpLine(theArgs + " " + std::to_string(a) + " " + std::to_string(b) + " " + SHex((const unsigned char*)c, d));
        unsigned long theSize = (unsigned long)d;
        NEMXferMessage* theXfer = new NEMXferMessage((unsigned char)b, (unsigned char*)c, theSize);
        unsigned char* theData = new unsigned char[theXfer->ExportSize()];
        delete[] theData;
        theXfer->ExportMessage(&theData, theSize);
        theXfer->Dereference();
        long r = theServer->XferSendTo(a, theData, theSize);
        delete[] theData;
        return Answer(r == 0);
    }
    case EeNetMmOp::FileClone: {
        OpLine(theArgs + " " + FileTok(a));
        FSFileSpec* theCopy = new FSFileSpec;
        *theCopy = *theFile;
        return Answer((ptrdiff_t)theCopy, FileTok((size_t)theCopy));
    }
    case EeNetMmOp::FileFree:
        OpLine(theArgs + " " + FileTok(a));
        delete theFile;
        return Answer(0);
    case EeNetMmOp::FileIsOpen:
        OpLine(theArgs + " " + FileTok(a));
        return Answer(theFile->GetIsOpen());
    case EeNetMmOp::FileOpen:
        OpLine(theArgs + " " + FileTok(a));
        return Answer(theFile->Open(kFileAccessRead) == 0);
    case EeNetMmOp::FileClose:
        OpLine(theArgs + " " + FileTok(a));
        theFile->Close();
        return Answer(0);
    case EeNetMmOp::FileLength: {
        OpLine(theArgs + " " + FileTok(a));
        unsigned long theLength = 0;
        theFile->GetLength(theLength);
        return Answer((uint32_t)theLength);
    }
    case EeNetMmOp::FileRead: {
        OpLine(theArgs + " " + FileTok(a));
        unsigned long theRead = 0;
        if (theFile->Read((unsigned char*)b, 0x400, theRead) != 0)
            return Answer(-1, "-1 -");
        return Answer((uint32_t)theRead, std::to_string(theRead) + " " + SHex((const unsigned char*)b, theRead));
    }
    case EeNetMmOp::FileName:
        OpLine(theArgs + " " + FileTok(a));
        return Answer((ptrdiff_t)theFile->GetFile(), theFile->GetFile());
    default:
        OpLine(theArgs);
        gQ->Line("unexpected mm op %d", (int)inOp);
        return Answer(0);
    }
}

// ---- one sequence on one side

struct Rng {
    uint32_t mState;
    uint32_t Next()
    {
        this->mState = this->mState * 1664525u + 1013904223u;
        return this->mState >> 8;
    }
    uint32_t Below(uint32_t inN) { return this->Next() % inN; }
};

struct Sequence {
    bool mRust;
    Rng mRandom;
    OrigServer* mOrig;
    bool mPublished;

    void Call(const std::string& inCall)
    {
        if (this->mRust)
            gQ->Op("call " + inCall);
        gQ->Line("call %s", inCall.c_str());
    }
    void Result(long inResult)
    {
        if (this->mRust)
            gQ->Op("ret " + std::to_string(inResult));
        gQ->Line("= %ld", inResult);
    }
    unsigned long CT()
    {
        return this->mRust ? ee_net_sv_get(sRust, EeNetSvGet::CurrentCommunicationsTick)
                           : this->mOrig->mCurrentCommunicationsTick;
    }
    // the server's state, read through its fields (the Rust's accessors)
    void State()
    {
        unsigned long v[14];
        unsigned long inputs, s2aw, humans, last;
        bool paused;
        if (this->mRust) {
            static const EeNetSvGet kFields[] = {
                EeNetSvGet::CurrentCommunicationsTick, EeNetSvGet::CurrentPhysicsTick, EeNetSvGet::GameState,
                EeNetSvGet::SendInterval, EeNetSvGet::PTsPerCt, EeNetSvGet::PTsPerSecond, EeNetSvGet::LastCtMessageId,
                EeNetSvGet::ProcessingSendToAllAndWait, EeNetSvGet::IsOos, EeNetSvGet::NextChecksumCt,
                EeNetSvGet::ReconnectCount, EeNetSvGet::LowestReconnectCt, EeNetSvGet::WaitForReconnectsTime,
                EeNetSvGet::GameWasPublished};
            for (int i = 0; i < 14; i++)
                v[i] = ee_net_sv_get(sRust, kFields[i]);
            uint32_t a, b;
            ee_net_sv_queue_lengths(sRust, &a, &b);
            inputs = a;
            s2aw = b;
            humans = (unsigned char)BGet(EeNetCommsField::NumHumanPlayers);
            last = (unsigned long)(long)(int32_t)BGet(EeNetCommsField::LastTickReceived);
            paused = BGet(EeNetCommsField::IsGamePaused) != 0;
        } else {
            OrigServer* o = this->mOrig;
            v[0] = o->mCurrentCommunicationsTick;
            v[1] = o->mCurrentPhysicsTick;
            v[2] = o->mGameState;
            v[3] = o->mSendInterval;
            v[4] = o->mPTsPerCT;
            v[5] = o->mPTsPerSecond;
            v[6] = o->mLastCTMessageID;
            v[7] = o->mProcessingSendToAllAndWait;
            v[8] = o->mIsOOS;
            v[9] = o->mNextChecksumCT;
            v[10] = o->mReconnectCount;
            v[11] = o->mLowestReconnectCT;
            v[12] = o->mWaitForReconnectsTime;
            v[13] = o->mGameWasPublished;
            inputs = (unsigned long)o->mIncomingPlayerInputs.size();
            s2aw = (unsigned long)o->mSendToAllAndWaitRequests.size();
            humans = o->mNumHumanPlayers;
            last = (unsigned long)o->mLastTickReceived;
            paused = o->mIsGamePaused;
        }
        std::string theState;
        for (int i = 0; i < 14; i++)
            theState += (i ? " " : "") + std::to_string(v[i]);
        gQ->Line("state %s inputs=%lu s2aw=%lu humans=%lu last=%ld paused=%d msgs=%lu", theState.c_str(), inputs,
                 s2aw, humans, (long)last, (int)paused, (unsigned long)gQ->mMessages.size());
        if (this->mRust)
            gQ->Op("state " + theState + " " + std::to_string(inputs) + " " + std::to_string(s2aw));
    }
    // an input's export size: a quarter of them sizes that fill a tick's packet to its last byte (12 + 536 + 0x1c)
    unsigned long InputSize()
    {
        static const unsigned long kExact[] = {268, 268, 524, 134};
        return this->mRandom.Below(4) == 0 ? kExact[this->mRandom.Below(4)] : 8 + this->mRandom.Below(300);
    }
    NEMessage* NewMessage(unsigned char inType, unsigned long inSize = 12)
    {
        return new NEMessage(inType, inSize);
    }
    // a client's message
    NEMessage* ClientMessage()
    {
        NEMessage* m;
        uint32_t k = this->mRandom.Below(20);
        unsigned long ct = this->CT();
        if (k < 7) {
            NEMCommunicationsTickResponse* r = (NEMCommunicationsTickResponse*)this->NewMessage(kNetMessageCommunicationsTickResponse);
            r->mKey = ct - (ct > 0 ? 1 + this->mRandom.Below(ct < 3 ? ct : 3) : 0);
            r->mProcessed = r->mKey;
            r->mAverage = this->mRandom.Below(4) == 0 ? 600 + this->mRandom.Below(400) : this->mRandom.Below(200);
            uint32_t theInputs = this->mRandom.Below(4) == 0 ? 1 + this->mRandom.Below(3) : 0;
            for (uint32_t i = 0; i < theInputs; i++)
                r->mInputs.push_back(this->NewMessage(0x40, this->InputSize())->mID);
            if (this->mRandom.Below(10) == 0)
                r->mExtraInputs = 1;
            m = r;
        } else if (k < 9) {
            m = this->NewMessage(this->mRandom.Below(2) ? kNetMessagePauseGameResponse : kNetMessageUnPauseGameResponse);
            m->mKey = gS->mPauseKeys.empty() ? ct : gS->mPauseKeys[this->mRandom.Below((uint32_t)gS->mPauseKeys.size())];
        } else if (k < 11) {
            m = this->NewMessage(kNetMessageWorldDatabaseChecksumResponse);
            m->mCT = ct > 2 ? ct - 1 - this->mRandom.Below(2) : 1;
            m->mChecksum = this->mRandom.Below(5) == 0 ? 2000 : 1000;
        } else if (k == 11) {
            m = this->NewMessage(kNetMessageClientConnectionRequest);
        } else if (k == 12) {
            m = this->NewMessage(kNetMessageClientDisconnectionNotification);
        } else if (k == 13) {
            m = this->NewMessage(kNetMessageClientReconnectionRequest);
            m->mCT = ct > 5 ? ct - this->mRandom.Below(5) : ct;
        } else if (k == 14) {
            m = this->NewMessage(kNetMessagePlayerConnectionCheckResponse);
        } else if (k == 15) {
            static const unsigned char kTypes[] = {kNetMessageAddPlayerResponse, kNetMessageStartGameResponse,
                                                   kNetMessageStopGameResponse};
            m = this->NewMessage(kTypes[this->mRandom.Below(3)]);
        } else {
            m = this->NewMessage(0x40, this->InputSize());
            m->mAvailable = this->mRandom.Below(6) != 0;
        }
        return m;
    }

    void Step()
    {
        uint32_t theWhat = this->mRandom.Below(100);
        char theCall[200];
        long r = 0;
        OrigServer* o = this->mOrig;
        const EeNetServer* s = sRust;
        bool R = this->mRust;
        // (a server whose Publish failed has no threads' events or server: the original read them uninitialized; an
        // unregistered slot resets the server's connection)
        if (!this->mPublished && ((theWhat >= 44 && theWhat < 50) || (theWhat >= 62 && theWhat < 70) || (theWhat >= 76 && theWhat < 78) ||
                                 (theWhat >= 81 && theWhat < 83) || theWhat >= 88))
            return;
        if (theWhat < 4) {   // the world changes
            gQ->mClock += this->mRandom.Below(300);
            if (this->mRandom.Below(4) == 0)
                gQ->mPrecision = this->mRandom.Below(2) == 0 ? 0x20000 : kFPPrecision53;
            if (this->mRandom.Below(4) == 0)
                gQ->mRounding = this->mRandom.Below(2) == 0 ? 0x100 : 0;
            gQ->Line("env %lu %lx %lx", gQ->mClock, gQ->mPrecision, gQ->mRounding);
            return;
        }
        if (theWhat < 20) {   // time passes, the threads running at their deadlines
            unsigned long theTime = this->mRandom.Below(5) == 0 ? this->mRandom.Below(25000) : this->mRandom.Below(1500);
            snprintf(theCall, sizeof(theCall), "pass %lu", theTime);
            this->Call(theCall);
            gQ->Pass(theTime);
        } else if (theWhat < 34) {   // the clients' packets
            if (!this->mPublished)
                return;
            uint32_t theCount = 1 + this->mRandom.Below(3);
            std::string theIDs;
            for (uint32_t i = 0; i < theCount; i++) {
                int theID = 0;
                unsigned char theSlot = (unsigned char)this->mRandom.Below(4);
                if (this->mRandom.Below(16) != 0) {
                    NEMessage* m = this->ClientMessage();
                    theID = m->mID;
                }
                gS->mPackets.push_back({theID, theSlot});
                theIDs += " m" + std::to_string(theID) + ":" + std::to_string(theSlot);
            }
            snprintf(theCall, sizeof(theCall), "packets%s", theIDs.c_str());
            this->Call(theCall);
            gQ->Set(gS->mInputEvent);
        } else if (theWhat < 44) {   // the server's own input
            static const unsigned char kTypes[] = {0x40, 0x40, 0x40, 0x7c, 0x91};
            NEMessage* m = this->NewMessage(kTypes[this->mRandom.Below(5)], this->InputSize());
            m->mWhilePaused = this->mRandom.Below(4) == 0;
            m->mAvailable = this->mRandom.Below(6) != 0;
            unsigned char thePlayer = (unsigned char)(this->mRandom.Below(4) == 0 ? kSlotNone : this->mRandom.Below(4));
            snprintf(theCall, sizeof(theCall), "input m%d %d", m->mID, thePlayer);
            this->Call(theCall);
            r = R ? ee_net_sv_process_local_user_input(s, m, thePlayer) : o->ProcessLocalUserInput(m, thePlayer);
        } else if (theWhat < 48) {
            this->Call("start");
            if (R) ee_net_sv_set_event(s, EeNetSvEvent::StartGame); else o->StartGame();
        } else if (theWhat < 50) {
            this->Call("stop");
            if (R) ee_net_sv_set_event(s, EeNetSvEvent::StopGame); else o->StopGame();
        } else if (theWhat < 53) {
            this->Call("pause");
            if (R) ee_net_sv_pause_game(s, true); else o->PauseGame();
        } else if (theWhat < 56) {
            this->Call("unpause");
            if (R) ee_net_sv_pause_game(s, false); else o->UnpauseGame();
        } else if (theWhat < 58) {
            NEMessage* m = this->NewMessage(0x50, 20);
            m->mKey = 7000 + m->mID;
            unsigned long theTimeout = 100 + this->mRandom.Below(3000), theRetries = 1 + this->mRandom.Below(5);
            snprintf(theCall, sizeof(theCall), "s2aw m%d %lu %lu", m->mID, theTimeout, theRetries);
            this->Call(theCall);
            r = R ? ee_net_sv_send_to_all_and_wait(s, m, (uint32_t)theTimeout, (uint32_t)theRetries)
                  : (long)o->SendToAllAndWait(m, theTimeout, theRetries);
            m->Dereference();
        } else if (theWhat < 62) {   // the base class's fields
            uint32_t theField = this->mRandom.Below(5);
            unsigned long theValue = theField == 0 ? 1 + this->mRandom.Below(4) : theField == 3 ? this->mRandom.Below(800)
                                   : theField == 4 ? this->mRandom.Below(100) : this->mRandom.Below(2);
            snprintf(theCall, sizeof(theCall), "base %u %lu", theField, theValue);
            this->Call(theCall);
            if (R) {
                static const EeNetCommsField kFields[] = {EeNetCommsField::NumHumanPlayers,
                                                          EeNetCommsField::IsMultiplayerSession, EeNetCommsField::IsGamePaused,
                                                          EeNetCommsField::AveragePhysicsUpdateTime, EeNetCommsField::CtsProcessed};
                BSet(kFields[theField], theValue);
            } else {
                if (theField == 0) o->mNumHumanPlayers = (unsigned char)theValue;
                else if (theField == 1) o->mIsMultiplayerSession = theValue != 0;
                else if (theField == 2) o->mIsGamePaused = theValue != 0;
                else if (theField == 3) o->mAveragePhysicsUpdateTime = theValue;
                else o->mNumberOfCTsProcessedInPhysics = theValue;
            }
        } else if (theWhat < 64) {
            bool theAllow = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "allow %d", (int)theAllow);
            this->Call(theCall);
            if (R) ee_net_sv_allow_new_players(s, theAllow); else o->AllowNewPlayers(theAllow);
        } else if (theWhat < 65) {
            static const unsigned long kIntervals[] = {40, 200, 350, 700, 3000, 0xffffffff};
            unsigned long theInterval = kIntervals[this->mRandom.Below(6)];
            snprintf(theCall, sizeof(theCall), "interval %lu", theInterval);
            this->Call(theCall);
            if (R) ee_net_sv_set_send_interval(s, (uint32_t)theInterval); else o->SetSendInterval(theInterval);
        } else if (theWhat < 66) {
            this->Call("forcect");
            if (R) ee_net_sv_set_event(s, EeNetSvEvent::ForceNextCt); else o->ForceNextCT();
        } else if (theWhat < 67) {
            bool thePing = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "ping %d", (int)thePing);
            this->Call(theCall);
            if (R) ee_net_sv_set(s, EeNetSvSet::Ping, thePing); else if (thePing) o->StartPing(); else o->StopPing();
        } else if (theWhat < 68) {
            unsigned long theTime = 1 + this->mRandom.Below(3);
            snprintf(theCall, sizeof(theCall), "reconnects %lu", theTime);
            this->Call(theCall);
            if (R) ee_net_sv_start_waiting_for_reconnects(s, (uint32_t)theTime); else o->StartWaitingForReconnects(theTime);
            if (this->mRandom.Below(2) == 0) {   // (the resume near the 3.5 s the wait is cut to 3 s from)
                this->Result(0);
                this->Call("run");
                gQ->LetRun();
                this->Result(0);
                unsigned long theWait = 16000 + this->mRandom.Below(1200);
                snprintf(theCall, sizeof(theCall), "pass %lu", theWait);
                this->Call(theCall);
                gQ->Pass(theWait);
                this->Result(0);
                this->Call("resume");
                if (R) ee_net_sv_set_event(s, EeNetSvEvent::ResumeMigratedGame); else o->ResumeMigratedGame();
            }
        } else if (theWhat < 69) {
            this->Call("resume");
            if (R) ee_net_sv_set_event(s, EeNetSvEvent::ResumeMigratedGame); else o->ResumeMigratedGame();
        } else if (theWhat < 70) {
            unsigned long theTick = this->CT() + this->mRandom.Below(5);
            snprintf(theCall, sizeof(theCall), "setct %lu", theTick);
            this->Call(theCall);
            if (R) ee_net_sv_set(s, EeNetSvSet::CurrentCommunicationsTick, (uint32_t)theTick); else o->SetCurrentCommunicationsTick(theTick);
        } else if (theWhat < 72) {
            FSFileSpec theSpec(1 + (int)this->mRandom.Below(9), this->mRandom.Below(4000));
            unsigned char theSlot = (unsigned char)this->mRandom.Below(4);
            snprintf(theCall, sizeof(theCall), "xfer f%d %lu %d", theSpec.mTok, theSpec.mLength, theSlot);
            this->Call(theCall);
            unsigned long theID = 0;
            if (R) {
                uint32_t theRID = 0;
                r = ee_net_sv_xfer_file_to_client(s, &theSpec, 5, theSlot, &theRID) ? 0 : 0x10;
                theID = theRID;
            } else
                r = o->XferFileToClient(theSpec, 5, theSlot, theID, false);
            gQ->Line("xfer id %lu", theID);
        } else if (theWhat < 73) {
            unsigned long theID = this->mRandom.Below(6);
            snprintf(theCall, sizeof(theCall), "abortxfer %lu", theID);
            this->Call(theCall);
            if (R) ee_net_sv_abort_xfer(s, (uint32_t)theID); else o->AbortXfer(theID);
        } else if (theWhat < 75) {
            unsigned long ct = this->CT();
            unsigned long theTick = ct > 2 ? ct - 1 - this->mRandom.Below(2) : 1;
            unsigned long theChecksum = this->mRandom.Below(5) == 0 ? 2000 : 1000;
            snprintf(theCall, sizeof(theCall), "checksum %lu %lu", theTick, theChecksum);
            this->Call(theCall);
            if (R)
                ee_net_sv_log_checksum_response(s, (uint32_t)theTick, (uint32_t)theChecksum, kSlotServer);
            else {
                NEMessage* m = this->NewMessage(kNetMessageWorldDatabaseChecksumResponse);
                m->mCT = theTick;
                m->mChecksum = theChecksum;
                m->SetOriginatingSlot(kSlotServer);
                o->LogChecksumResponse((NEMWorldDatabaseChecksumResponse*)m);
                m->Dereference();
            }
            if (R) {   // (the same lines as the original's message)
                NEMessage* m = this->NewMessage(kNetMessageWorldDatabaseChecksumResponse);
                m->SetOriginatingSlot(kSlotServer);
                m->Dereference();
            }
        } else if (theWhat < 76) {
            if (!this->mPublished)
                return;
            NEMessage* m = this->NewMessage(0x60, 10 + this->mRandom.Below(40));
            snprintf(theCall, sizeof(theCall), "broadcast m%d", m->mID);
            this->Call(theCall);
            r = R ? ee_net_sv_broadcast_message(s, m) : o->BroadcastMessage(m);
            m->Dereference();
        } else if (theWhat < 78) {
            static const unsigned char kSlots[] = {kSlotServer, kSlotNone, 0, 1, 2, 3};
            unsigned char theSlot = kSlots[this->mRandom.Below(6)];
            bool a = this->mRandom.Below(2) != 0, b = this->mRandom.Below(4) == 0, c = this->mRandom.Below(4) == 0;
            snprintf(theCall, sizeof(theCall), "remove %d %d %d %d", theSlot, (int)a, (int)b, (int)c);
            this->Call(theCall);
            if (R) ee_net_sv_remove_player_from_game_in_progress(s, theSlot, a, b, c);
            else o->RemovePlayerFromGameInProgress(theSlot, a, b, c);
        } else if (theWhat < 80) {
            if (!this->mPublished)
                return;
            NEMessage* m = this->NewMessage(0x61, 10 + this->mRandom.Below(40));
            m->SetOriginatingSlot(kSlotServer);
            bool theAll = this->mRandom.Below(2) != 0, theGuaranteed = this->mRandom.Below(2) != 0;
            unsigned char theSlot = (unsigned char)this->mRandom.Below(4);
            snprintf(theCall, sizeof(theCall), "send m%d %d %d %d", m->mID, (int)theAll, theSlot, (int)theGuaranteed);
            this->Call(theCall);
            if (theAll)
                r = R ? (long)ee_net_sv_send_message_to_all(s, m, theGuaranteed) : o->SendMessageToAll(m, theGuaranteed);
            else
                r = R ? (long)ee_net_sv_send_message_to(s, theSlot, m, theGuaranteed) : o->SendMessageTo(theSlot, m, theGuaranteed);
            m->Dereference();
        } else if (theWhat < 81) {
            this->Call("locksct");
            if (R) {
                ee_net_sv_lock_send_communications_tick(s);
                ee_net_sv_unlock_send_communications_tick(s);
            } else {
                o->LockSendCommunicationsTick();
                o->UnlockSendCommunicationsTick();
            }
        } else if (theWhat < 83) {
            unsigned char theSlot = (unsigned char)this->mRandom.Below(4);
            bool theRegister = this->mRandom.Below(3) != 0;
            snprintf(theCall, sizeof(theCall), "slot %d %d", theSlot, (int)theRegister);
            this->Call(theCall);
            if (theRegister) {
                if (R) ee_net_sv_register_slot(s, theSlot); else o->RegisterSlot(theSlot);
            } else {
                if (R) ee_net_sv_unregister_slot(s, theSlot); else o->UnregisterSlot(theSlot);
            }
        } else if (theWhat < 84) {
            static const unsigned char kStates[] = {0, 1, 3, 4};
            unsigned char theState = kStates[this->mRandom.Below(4)];
            snprintf(theCall, sizeof(theCall), "state %d", theState);
            this->Call(theCall);
            if (R) ee_net_sv_set(s, EeNetSvSet::GameState, theState); else o->SetGameState(theState);
        } else if (theWhat < 85) {
            unsigned char thePlayer = (unsigned char)this->mRandom.Below(4), theSlot = (unsigned char)this->mRandom.Below(4);
            uint32_t k = this->mRandom.Below(3);
            snprintf(theCall, sizeof(theCall), "sendtoslot %u %d %d", k, thePlayer, theSlot);
            this->Call(theCall);
            if (k == 0) {
                if (R) ee_net_sv_set_send_to_slot_for_player(s, thePlayer, theSlot); else o->SetSendToSlotForPlayer(thePlayer, theSlot);
            } else if (k == 1)
                r = R ? ee_net_sv_send_to_slot_for_player(s, thePlayer) : o->GetSendToSlotForPlayer(thePlayer);
            else {
                if (R) ee_net_sv_clear_send_to_slot_array(s); else o->ClearSendToSlotArray();
            }
        } else if (theWhat < 86) {
            bool theStart = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "search %d", (int)theStart);
            this->Call(theCall);
            r = R ? ee_net_sv_search_for_game(s, theStart) : o->SearchForGame(theStart);
        } else if (theWhat < 87) {
            unsigned long theCT = this->mRandom.Below(50);
            snprintf(theCall, sizeof(theCall), "lowestcts %lu", theCT);
            this->Call(theCall);
            if (R) ee_net_sv_set_message_manager_lowest_cts(s, (uint32_t)theCT); else o->SetMessageManagerLowestCTs(theCT);
        } else if (theWhat < 88) {
            this->Call("number");
            r = R ? (long)ee_net_sv_message_manager_current_number(s) : (long)o->GetMessageManagerCurrentMessageNumber();
        } else if (theWhat < 90 && this->mPublished) {   // the transfer connection's events
            bool theAccept = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "xferevent %d", (int)theAccept);
            this->Call(theCall);
            gQ->Set(theAccept ? gS->mXferAcceptEvent : gS->mXferInputEvent);
        } else if (theWhat < 92 && gPlatSimMode) {
            this->Call("simtick");
            if (R) ee_net_sv_sim_communications_tick(s); else o->SimCommunicationsTick();
        } else if (theWhat < 93) {
            this->Call("tune");
            r = R ? ee_net_sv_tune_server(s) : o->TuneServer();
        } else
            return;
        this->Result(r);
        this->Call("run");
        gQ->LetRun();
        this->Result(0);
        this->State();
    }

    void Run(uint32_t inSeed, int inSteps)
    {
        this->mRandom.mState = inSeed;
        ee_net_comms_free(sComms);
        sComms = NULL;
        gS->mPublishFails = this->mRandom.Below(12) == 0;
        gPlatSimMode = this->mRandom.Below(10) == 0;
        static const unsigned long kIntervals[] = {200, 350, 350, 350, 700};
        unsigned char theMax = (unsigned char)(2 + this->mRandom.Below(7));
        unsigned long theInterval = kIntervals[this->mRandom.Below(5)];
        unsigned char thePTs = (unsigned char)(4 + this->mRandom.Below(20));
        unsigned long theMaxRate = 20 + this->mRandom.Below(40), theMinRate = 5 + this->mRandom.Below(15);
        float theFactor = 0.5f + 0.25f * this->mRandom.Below(8);
        unsigned long theMaxInterval = 1000 + this->mRandom.Below(3000), theIncrement = 50 + this->mRandom.Below(100);
        unsigned long theDecrement = 10 + this->mRandom.Below(50), theCT = this->mRandom.Below(3) == 0 ? 50 : 0;
        bool theAllow = this->mRandom.Below(4) != 0;
        sAnnounce = this->mRandom.Below(2) != 0;
        sAdapter = this->mRandom.Below(3);
        char theCall[200];
        snprintf(theCall, sizeof(theCall), "new %d %lu %d %lu %lu %g %lu %lu %lu %lu %d %d %lu %d", theMax, theInterval,
                 thePTs, theMaxRate, theMinRate, theFactor, theMaxInterval, theIncrement, theDecrement, theCT,
                 (int)theAllow, (int)sAnnounce, sAdapter, (int)gPlatSimMode);
        this->Call(theCall);
        if (this->mRust) {
            EeNetSvArgs a = {theMax, (uint32_t)theInterval, thePTs, (uint32_t)theMaxRate, (uint32_t)theMinRate,
                             theFactor, (uint32_t)theMaxInterval, (uint32_t)theIncrement, (uint32_t)theDecrement,
                             (uint32_t)theCT, 0, theAllow};
            sComms = ee_net_comms_new((uint32_t)theCT);   // (the base class's constructor takes the tick)
            BSet(EeNetCommsField::NumHumanPlayers, 1);
            sRust = ee_net_sv_new(NULL, RustSvHost, RustMmHost, RustQHost, &a, sComms);
            this->mPublished = ee_net_sv_get(sRust, EeNetSvGet::GameWasPublished) != 0;
        } else {
            this->mOrig = new OrigServer(theMax, theInterval, thePTs, theMaxRate, theMinRate, theFactor, theMaxInterval,
                                         theIncrement, theDecrement, theCT, 0, sName, theAllow, sAnnounce, sAdapter);
            this->mPublished = this->mOrig->mGameWasPublished;
        }
        this->Result(0);
        this->Call("startthreads");
        if (this->mRust)
            ee_net_sv_start_threads(sRust);
        else
            this->mOrig->StartThreads();
        this->Result(0);
        this->State();
        for (int i = 0; i < inSteps; i++)
            this->Step();
        // (a reconnect wait ignores the exit until the game starts: the game's timeout handler would start it)
        if (this->mPublished) {
            this->Call("start");
            if (this->mRust) ee_net_sv_set_event(sRust, EeNetSvEvent::StartGame); else this->mOrig->StartGame();
            this->Result(0);
            this->Call("run");
            gQ->LetRun();
            this->Result(0);
        }
        this->Call("stopthreads");
        if (this->mRust)
            ee_net_sv_stop_threads(sRust);
        else
            this->mOrig->StopThreads();
        this->Result(0);
        this->Call("free");
        if (this->mRust)
            ee_net_sv_free(sRust);
        else
            delete this->mOrig;
        this->Result(0);
        gQ->Line("threads %d", (int)gQ->mThreads.size());
        if (gQ->mThreads.size() != 1) {
            fprintf(stderr, "ne_server_test: sequence %u (%s) left a thread running\n", inSeed, this->mRust ? "rust" : "orig");
            fflush(stderr);
            _exit(3);
        }
        std::string theLeft;
        for (auto& m : gQ->mMessages)
            theLeft += " m" + std::to_string(m.first) + ":" + std::to_string(m.second->mCount);
        gQ->Line("left%s", theLeft.c_str());
    }
};

static std::string RunSide(bool inRust, uint32_t inSeed, int inSteps, bool inRecord, std::string& outOps)
{
    QWorld* theWorld = new QWorld;
    theWorld->mRandom = inSeed * 2654435761u + 7;
    theWorld->mRecord = inRecord;
    theWorld->mEcho = sVerbose;
    theWorld->mZeroWaitTicks = true;
    gQ = theWorld;
    gS = new SNet;
    Sequence theSequence = {};
    theSequence.mRust = inRust;
    theSequence.Run(inSeed, inSteps);
    std::string theTrace = theWorld->mTrace;
    outOps = theWorld->mOps;
    for (auto& m : theWorld->mMessages)
        delete m.second;
    delete gS;
    gS = NULL;
    delete theWorld;
    gQ = NULL;
    return theTrace;
}

int main(int argc, char** argv)
{
    // (the record keeps the first 40 sequences whose ops stay under 120 KB: cargo's golden stays a few MB)
    int theCount = 1500, theSteps = 80, theRecorded = 40, theShow = 0, theFirst = 1, theKept = 0;
    const char* thePrint = NULL;
    std::map<std::string, int> theKinds;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc)
            theCount = atoi(argv[++i]);
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            theSteps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--show") == 0 && i + 1 < argc)
            theShow = atoi(argv[++i]);
        else if (strcmp(argv[i], "--first") == 0 && i + 1 < argc)
            theFirst = atoi(argv[++i]);
        else if (strcmp(argv[i], "--progress") == 0)
            sProgress = true;
        else if (strcmp(argv[i], "--verbose") == 0)
            sVerbose = true;
    }
    int theDiffering = 0;
    size_t theLines = 0;
    std::string theGolden;
    for (int s = theFirst - 1; s < theFirst - 1 + theCount; s++) {
        if (sVerbose || sProgress) {
            fprintf(stderr, "sequence %d\n", s + 1);
            fflush(stderr);
        }
        std::string theOps;
        std::string theOrig = RunSide(false, (uint32_t)s + 1, theSteps, false, theOps);
        bool theRecord = thePrint != NULL && theKept < theRecorded;
        std::string theRust = RunSide(true, (uint32_t)s + 1, theSteps, theRecord, theOps);
        if (theRecord && theOps.size() < 120000 && ++theKept)
            theGolden += "seq " + std::to_string(s + 1) + "\n" + theOps;
        if (s + 1 == theShow)
            printf("%s", theOrig.c_str());
        for (size_t i = 0, theEnd; i < theOrig.size(); i = theEnd + 1) {
            theEnd = theOrig.find('\n', i);
            std::string theLine = theOrig.substr(i, theEnd - i);
            size_t theFrom = theLine.find(' ') + 1;
            size_t theSecond = theLine.find(' ', theFrom);
            size_t theThird = theSecond == std::string::npos ? std::string::npos : theLine.find(' ', theSecond + 1);
            theKinds[theLine.substr(theFrom, theThird == std::string::npos ? std::string::npos : theThird - theFrom)]++;
            theLines++;
        }
        if (theOrig != theRust) {
            if (theDiffering++ < 3) {
                size_t i = 0;
                while (i < theOrig.size() && i < theRust.size() && theOrig[i] == theRust[i])
                    i++;
                size_t theStart = theOrig.rfind('\n', i == 0 ? 0 : i - 1);
                theStart = theStart == std::string::npos ? 0 : theStart + 1;
                printf("sequence %d differs:\n  orig: %.300s\n  rust: %.300s\n", s + 1, theOrig.c_str() + theStart,
                       theRust.c_str() + theStart);
            }
        }
    }
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(theGolden.data(), 1, theGolden.size(), theFile);
        fclose(theFile);
    }
    for (auto& k : theKinds)
        if (k.first.compare(0, 6, "state ") != 0 && k.first.compare(0, 6, "call p") != 0)
            printf("  %-28s %d\n", k.first.c_str(), k.second);
    printf("ne_server_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
