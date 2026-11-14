// NESynchronousClient as a whole Rust object (LLE Network Engine round 3): the original unit, unmodified
// (orig_c/NESynchronousClient.cpp with its header, compiled over the stand-in headers of orig_c/lle: the clock,
// events, threads, messages, the protocol and its connections, the received files of one world), against ee_net's
// Rust client (client.rs) whose host op tables answer with the same world objects, as NESynchronousClient.cpp's
// hosts answer with the game's. Both clients use ee_net's lag simulator and recorder (round 2's goldens).
//
// The client's main and receive threads are real threads that run one at a time (orig_q/lle/q_world.h: the world
// hands the baton on by a fixed rule), so both sides interleave the same way. Each sequence makes the same random
// calls on a client: connects, disconnects, connect requests timed and retried, searches, local inputs (throttled,
// guaranteed or sent, while paused or playing back), the server's packets on the game connection (ticks new, old and
// repeated, start and stop, connection and reconnection replies, pause and unpause with their send-to-all-and-wait
// ids, packets naming no message), transfer records on the transfer connection (whole, in pieces, files made or
// refused, written, closed, deleted), the base class's fields changed, the clock moving on (the threads' timeouts:
// connect retries, silence warnings, a lost connection). Each side's world prints a line for every effect and the
// client's state after every step; the traces must be the same. At the end the threads are stopped, the client
// freed, and the messages left (the original's leaks) listed.
//
// The Rust side's hosts also record each op with its arguments and answer (messages, events, threads, connections and
// files by world id), the API calls and the thread switches: the first sequences are
// rust/crates/ee_net/src/golden_client.txt, which cargo replays against the Rust alone (`--print FILE` writes it).
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#include <string>
#include <vector>

#include "orig_q/lle/q_world.h"
QWorld* gQ;
#include "orig_c/lle/c_world.h"
CNet* gNet;
static bool sVerbose = false;   // --verbose: each trace line to stderr as it is made
#include "orig_c/NESynchronousClient.cpp"
#include "rust/ee_net.h"
#include "ne_tcp_host.h"

int QWorld::IdOf(NEMessage* inMessage)
{
    return inMessage != NULL ? inMessage->mID : 0;
}

// ---- the queues' common calls out for the original client (its lag simulator and recorder pass messages on)

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

// ---- the game's side of the client (its virtual members): the same on both sides

static FSFile* VCreateXferFile(const UWideString& inName, unsigned long inSize)
{
    FSFile* f = gQ->Next() % 4 == 0 ? NULL : new FSFile(gNet->mNextFile++);
    gQ->Line("v createxferfile %s %lu = f%d", inName.mText.c_str(), inSize, f != NULL ? f->mID : 0);
    return f;
}
static bool VIsPlaybackRecordingSession()
{
    bool b = gQ->Next() % 8 == 0;
    gQ->Line("v playbackrecording = %d", (int)b);
    return b;
}
static unsigned long VGetConnectTimeout()
{
    gQ->Line("v connecttimeout");
    return 5000;
}
static unsigned char VGetClientSlot()
{
    unsigned char s = (unsigned char)(gQ->Next() % 4);
    gQ->Line("v clientslot = %d", s);
    return s;
}
static bool VIgnoreConnectionTimeout()
{
    bool b = gQ->Next() % 4 == 0;
    gQ->Line("v ignoretimeout = %d", (int)b);
    return b;
}
static void VSetConnectionState(long inState) { gQ->Line("v connectionstate %ld", inState); }
// takes the message's reference, except a tick's (the client gives that back after its reply)
// (a message the game does not know: half the time the game disconnects, from inside the client's main thread)
static std::function<long()> sDisconnect;
static void VHandleMessage(NEMessage* inMessage)
{
    gQ->Line("v handlemessage m%d %02x", inMessage->mID, inMessage->mType);
    bool theDisconnect = inMessage->mType == 0x29 && gQ->Next() % 2 == 0;
    if (inMessage->mType != kNetMessageCommunicationsTick)
        inMessage->Dereference();
    if (theDisconnect)
        gQ->Line("v disconnect = %ld", sDisconnect());
}
static void VHandleUserInput(NEMessage* inMessage) { gQ->Line("v handleuserinput m%d", inMessage->mID); }
static void VHandleConnectionLost() { gQ->Line("v connectionlost"); }
static unsigned long VHandleStartGame(unsigned long inOffset)
{
    unsigned long t = gQ->Next() % 10000;
    gQ->Line("v startgame %lu = %lu", inOffset, t);
    return t;
}
static void VHandleStopGame() { gQ->Line("v stopgame"); }
static void VHandleConnectionTimeoutWarning(unsigned long inRemaining) { gQ->Line("v timeoutwarning %lu", inRemaining); }

class OrigClient : public NESynchronousClient {
public:
    virtual void ExecuteRemovePlayer() {}
    virtual void IsClient() {}
    virtual void IsServer() {}
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize) { return VCreateXferFile(inName, inSize); }
    virtual bool IsPlaybackRecordingSession() { return VIsPlaybackRecordingSession(); }
    virtual unsigned long GetConnectTimeout() { return VGetConnectTimeout(); }
    virtual unsigned char GetClientSlot() { return VGetClientSlot(); }
    virtual bool IgnoreConnectionTimeout() { return VIgnoreConnectionTimeout(); }
    virtual void SetConnectionState(long inState) { VSetConnectionState(inState); }
    virtual void HandleMessage(NEMessage* inMessage) { VHandleMessage(inMessage); }
    virtual void HandleUserInput(NEMessage* inMessage) { VHandleUserInput(inMessage); }
    virtual void HandleConnectionLost() { VHandleConnectionLost(); }
    virtual unsigned long HandleStartGame(unsigned long inOffset) { return VHandleStartGame(inOffset); }
    virtual void HandleStopGame() { VHandleStopGame(); }
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining) { VHandleConnectionTimeoutWarning(inRemaining); }
};

// ---- the Rust side's hosts (each op the statement NESynchronousClient.cpp's hosts make, on the world)

static EeNetComms* sComms;   // the Rust client's base class fields (ee_net comms.rs), as NESynchronousCommunications holds them
static uint32_t BGet(EeNetCommsField f) { return ee_net_comms_get(sComms, f); }
static void BSet(EeNetCommsField f, unsigned long v) { ee_net_comms_set(sComms, f, (uint32_t)v); }
static EeNetClient* sRust;

static std::string Tok(char inKind, int inID) { return std::string(1, inKind) + std::to_string(inID); }
static std::string MsgTok(size_t m) { return Tok('m', gQ->IdOf((NEMessage*)m)); }
static std::string EvTok(size_t e) { return Tok('e', e ? ((TSEvent*)e)->mID : 0); }
static std::string ThTok(size_t t) { return Tok('t', t ? ((TSThread*)t)->mID : 0); }
static std::string ConnTok(size_t c) { return Tok('c', c ? ((NEConnection*)c)->mID : 0); }
static std::string FileTok(size_t f) { return Tok('f', f ? ((FSFile*)f)->mID : 0); }

static unsigned long RustMainThread(void* p) { return ee_net_cl_main_thread((const EeNetClient*)p); }
static unsigned long RustReceiveThread(void* p) { return ee_net_cl_receive_thread((const EeNetClient*)p); }

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
        long r = a == 4 ? TSScheduler::CreateThread(t, RustMainThread, (void*)b)
               : a == 5 ? TSScheduler::CreateThread(t, RustReceiveThread, (void*)b)
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
        ((NEMessage*)a)->Reference();
        return Answer(0);
    case EeNetQOp::MsgDereference:
        OpLine(theArgs + " " + MsgTok(a));
        ((NEMessage*)a)->Dereference();
        return Answer(0);
    case EeNetQOp::MsgFromBytes: {
        OpLine(theArgs + " " + CHex((const unsigned char*)a, 4));
        NEMessage* m = NEMessage::ConvertMemoryToMessage((unsigned char*)a);
        return Answer((ptrdiff_t)m, MsgTok((size_t)m));
    }
    default:
        OpLine(theArgs);
        gQ->Line("unexpected queue op %d", (int)inOp);
        return Answer(0);
    }
}

static ptrdiff_t RustClHost(void*, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    std::string theArgs = "c" + std::to_string((int)inOp);
    NEMessage* m = (NEMessage*)a;
    NEConnection* theConnection = (NEConnection*)a;
    FSFile* theFile = (FSFile*)a;
    NEProtocol* theProtocol;
    NEProtocol::GetProtocol(0, theProtocol);
    switch (inOp) {
    case EeNetClOp::WaitAny: {
        std::string theList;
        for (size_t i = 0; i < b; i++)
            theList += (i ? "," : "") + EvTok((size_t)((TSEvent* const*)a)[i]);
        OpLine(theArgs + " " + theList + " " + std::to_string(c));
        return Answer(TSEvent::WaitAny((TSEvent* const*)a, (unsigned long)b, (unsigned long)c));
    }
    case EeNetClOp::ThreadPriority:
        OpLine(theArgs + " " + ThTok(a));
        ((TSThread*)a)->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
        return Answer(0);
    case EeNetClOp::ProtocolCount:
        OpLine(theArgs);
        return Answer((ptrdiff_t)NEProtocol::GetCount());
    case EeNetClOp::ProtocolGet:
        OpLine(theArgs);
        return Answer(NEProtocol::GetProtocol(0, *(NEProtocol**)a), "0 p");
    case EeNetClOp::Connect: {
        OpLine(theArgs + " " + EvTok(b) + " " + ConnTok(*(size_t*)c) + " " + std::to_string(d));
        const void* const* s = (const void* const*)a;
        long r = theProtocol->Connect(*(const UString*)s[0], *(const UString*)s[1], *(const UWideString*)s[2], 2,
                                      (TSEvent*)b, *(NEConnection**)c, (unsigned long)d);
        return Answer(r, std::to_string(r) + " " + ConnTok(*(size_t*)c));
    }
    case EeNetClOp::ConnectForXfer: {
        OpLine(theArgs + " " + ConnTok(a) + " " + EvTok(b) + " " + ConnTok(*(size_t*)c));
        const NEConnection* theFrom = theConnection;
        long r = theProtocol->ConnectForXfer(theFrom, 1, (TSEvent*)b, *(NEConnection**)c);
        return Answer(r, std::to_string(r) + " " + ConnTok(*(size_t*)c));
    }
    case EeNetClOp::ConnClose:
        OpLine(theArgs + " " + ConnTok(a));
        return Answer(theConnection->Close());
    case EeNetClOp::ConnReceive: {
        OpLine(theArgs + " " + ConnTok(a) + " " + std::to_string(c) + " " + std::to_string(*(uint32_t*)d));
        unsigned long theLength = *(uint32_t*)d;
        long r = theConnection->Receive((unsigned long)c, (unsigned char*)b, theLength);
        *(uint32_t*)d = (uint32_t)theLength;
        return Answer(r, std::to_string(r) + " " + std::to_string(theLength) + " " + CHex((unsigned char*)b, 4));
    }
    case EeNetClOp::ConnSend:
        OpLine(theArgs + " " + ConnTok(a) + " " + CHex((const unsigned char*)b, c));
        return Answer(theConnection->Send((const unsigned char*)b, (unsigned long)c));
    case EeNetClOp::DetectedGameCount:
        OpLine(theArgs);
        return Answer((ptrdiff_t)theProtocol->GetDetectedGameCount());
    case EeNetClOp::DetectedGame: {
        OpLine(theArgs + " " + std::to_string(a));
        void* const* s = (void* const*)b;
        return Answer(theProtocol->GetDetectedGame((unsigned long)a, *(UString*)s[0], *(UString*)s[1], *(UWideString*)s[2]));
    }
    case EeNetClOp::SearchForGame: {
        OpLine(theArgs + " " + std::to_string(a));
        UString theName("EEService");
        UString theHost("Anything");
        return Answer(theProtocol->SearchForGame(theHost, theName, a != 0));
    }
    case EeNetClOp::XferReceive: {
        OpLine(theArgs + " " + std::to_string(b) + " " + std::to_string(*(uint32_t*)c));
        unsigned long theReceived = *(uint32_t*)c;
        long r = theProtocol->XferReceive((unsigned long)b, (unsigned char*)a, theReceived);
        *(uint32_t*)c = (uint32_t)theReceived;
        return Answer(r, std::to_string(r) + " " + std::to_string(theReceived) + " " +
                             CHex((unsigned char*)a, theReceived));
    }
    case EeNetClOp::XferExportSize:
        OpLine(theArgs);
        return Answer((ptrdiff_t)NEMXferMessage::StaticExportSize());
    case EeNetClOp::MsgType:
        OpLine(theArgs + " " + MsgTok(a));
        return Answer(m->GetType());
    case EeNetClOp::MsgFlag: {
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        ptrdiff_t r = 0;
        switch ((EeNetClMsgFlag)b) {
        case EeNetClMsgFlag::CountsTowardsPlayerInputThrottle: r = m->GetCountsTowardsPlayerInputThrottle(); break;
        case EeNetClMsgFlag::CanSendWhilePaused: r = m->GetCanSendWhilePaused(); break;
        case EeNetClMsgFlag::IsAvailableToSend: r = m->GetIsAvailableToSend(); break;
        }
        return Answer(r);
    }
    case EeNetClOp::MsgSetSlot:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b) + " " + std::to_string(c));
        if (b != 0)
            m->SetOriginatingSlot((unsigned char)c);
        else
            m->SetForSpecificSlot((unsigned char)c);
        return Answer(0);
    case EeNetClOp::MsgS2awId: {
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(*(uint32_t*)b));
        unsigned long theID = *(uint32_t*)b;
        bool r = m->GetSendToAllAndWaitMessageID(theID);
        *(uint32_t*)b = (uint32_t)theID;
        return Answer(r, std::to_string((int)r) + " " + std::to_string(*(uint32_t*)b));
    }
    case EeNetClOp::MsgGet: {
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        switch ((EeNetClMsgField)b) {
        case EeNetClMsgField::TickCt: return Answer((ptrdiff_t)((NEMCommunicationsTick*)m)->GetCurrentCommunicationsTick());
        case EeNetClMsgField::ServersTick: return Answer((ptrdiff_t)((NEMCommunicationsTickResponse*)m)->GetServersCommunicationTick());
        case EeNetClMsgField::OffsetStartTime: return Answer((ptrdiff_t)((NEMStartGame*)m)->GetOffsetStartTime());
        case EeNetClMsgField::SendToSlot: return Answer(((NEMClientConnectionRequestResponse*)m)->GetSendToSlot());
        case EeNetClMsgField::ConnectionError: return Answer((ptrdiff_t)(uint32_t)((NEMClientConnectionRequestResponse*)m)->GetError());
        case EeNetClMsgField::ConnectionCt: return Answer((ptrdiff_t)((NEMClientConnectionRequestResponse*)m)->GetCurrentCT());
        case EeNetClMsgField::NewSendToSlot: return Answer(((NEMClientReconnectionRequestResponse*)m)->GetNewSendToSlot());
        case EeNetClMsgField::ReconnectionCt: return Answer((ptrdiff_t)((NEMClientReconnectionRequestResponse*)m)->GetCurrentCT());
        case EeNetClMsgField::UniqueId:
            return Answer((ptrdiff_t)(m->GetType() == kNetMessagePauseGame ? ((NEMPauseGame*)m)->GetUniqueID()
                                                                           : ((NEMUnPauseGame*)m)->GetUniqueID()));
        case EeNetClMsgField::XferId: return Answer(((NEMXferMessage*)m)->GetID());
        case EeNetClMsgField::XferData:
            return Answer((ptrdiff_t)((NEMXferMessage*)m)->GetData(), CHex(((NEMXferMessage*)m)->GetData(), kCXferRecord - 2));
        case EeNetClMsgField::XferDataSize: return Answer((ptrdiff_t)((NEMXferMessage*)m)->GetDataSize());
        }
        return Answer(0);
    }
    case EeNetClOp::MsgNew: {
        NEMessage* theNew = NULL;
        switch ((EeNetClNewMsg)a) {
        case EeNetClNewMsg::TickResponse:
            OpLine(theArgs + " 0 " + std::to_string(b) + " " + std::to_string(c) + " " + std::to_string(d));
            theNew = new NEMCommunicationsTickResponse((unsigned long)b, (unsigned long)c, (unsigned long)d);
            break;
        case EeNetClNewMsg::StopGameResponse:
            OpLine(theArgs + " 1");
            theNew = new NEMStopGameResponse();
            break;
        case EeNetClNewMsg::StartGameResponse: {
            OpLine(theArgs + " 2 " + std::to_string(b));
            unsigned long theTime = (unsigned long)b;
            theNew = new NEMStartGameResponse(theTime);
            break;
        }
        case EeNetClNewMsg::UnPauseGameResponse:
            OpLine(theArgs + " 3 " + std::to_string(b));
            theNew = new NEMUnPauseGameResponse((unsigned long)b);
            break;
        case EeNetClNewMsg::PauseGameResponse:
            OpLine(theArgs + " 4 " + std::to_string(b));
            theNew = new NEMPauseGameResponse((unsigned long)b);
            break;
        case EeNetClNewMsg::XferMessage:
            OpLine(theArgs + " 5 " + CHex((unsigned char*)b, kCXferRecord));
            theNew = new NEMXferMessage((unsigned char*)b);
            break;
        case EeNetClNewMsg::XferStatus: {
            OpLine(theArgs + " 6 " + std::to_string(b) + " " + std::to_string(c));
            unsigned char theSlot = (unsigned char)b, thePercent = (unsigned char)(b >> 8), theStatus = (unsigned char)(b >> 16);
            uint32_t theID = (uint32_t)c;
            theNew = new NEMXferStatus(theSlot, thePercent, theID, theStatus);
            break;
        }
        }
        return Answer((ptrdiff_t)theNew, MsgTok((size_t)theNew));
    }
    case EeNetClOp::MsgAddInput:
        OpLine(theArgs + " " + MsgTok(a) + " " + MsgTok(b));
        return Answer(m->AddPlayerInput((NEMessage*)b));
    case EeNetClOp::MsgExportNew: {
        OpLine(theArgs + " " + MsgTok(a));
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        m->ExportMessage(&theData, theSize);
        *(uint32_t*)b = (uint32_t)theSize;
        return Answer((ptrdiff_t)theData, std::to_string(theSize) + " " + CHex(theData, theSize));
    }
    case EeNetClOp::BytesFree:
        OpLine(theArgs);
        delete[] (unsigned char*)a;
        return Answer(0);
    case EeNetClOp::FsWrite: {
        OpLine(theArgs + " " + FileTok(a) + " " + CHex((const unsigned char*)b, c) + " " + std::to_string(*(uint32_t*)d));
        unsigned long theWritten = *(uint32_t*)d;
        long r = theFile->Write((const unsigned char*)b, (unsigned long)c, theWritten);
        *(uint32_t*)d = (uint32_t)theWritten;
        return Answer(r, std::to_string(r) + " " + std::to_string(theWritten));
    }
    case EeNetClOp::FsClose:
        OpLine(theArgs + " " + FileTok(a));
        theFile->Close();
        return Answer(0);
    case EeNetClOp::FsDelete:
        OpLine(theArgs + " " + FileTok(a));
        theFile->Delete();
        return Answer(0);
    case EeNetClOp::FsFree:
        OpLine(theArgs + " " + FileTok(a));
        delete theFile;
        return Answer(0);
    case EeNetClOp::CreateXferFile: {
        OpLine(theArgs + " " + std::string((const char*)a) + " " + std::to_string(b));
        FSFile* f = VCreateXferFile(UWideString((const char*)a), (unsigned long)b);
        return Answer((ptrdiff_t)f, FileTok((size_t)f));
    }
    case EeNetClOp::IsPlaybackRecordingSession:
        OpLine(theArgs);
        return Answer(VIsPlaybackRecordingSession());
    case EeNetClOp::ConnectTimeout:
        OpLine(theArgs);
        return Answer((ptrdiff_t)VGetConnectTimeout());
    case EeNetClOp::ClientSlot:
        OpLine(theArgs);
        return Answer(VGetClientSlot());
    case EeNetClOp::IgnoreConnectionTimeout:
        OpLine(theArgs);
        return Answer(VIgnoreConnectionTimeout());
    case EeNetClOp::SetConnectionState:
        OpLine(theArgs + " " + std::to_string((int32_t)a));
        VSetConnectionState((long)(int32_t)a);
        return Answer(0);
    case EeNetClOp::HandleMessage:
        OpLine(theArgs + " " + MsgTok(a));
        VHandleMessage(m);
        return Answer(0);
    case EeNetClOp::HandleUserInput:
        OpLine(theArgs + " " + MsgTok(a));
        VHandleUserInput(m);
        return Answer(0);
    case EeNetClOp::HandleConnectionLost:
        OpLine(theArgs);
        VHandleConnectionLost();
        return Answer(0);
    case EeNetClOp::HandleStartGame:
        OpLine(theArgs + " " + std::to_string(a));
        return Answer((ptrdiff_t)VHandleStartGame((unsigned long)a));
    case EeNetClOp::HandleStopGame:
        OpLine(theArgs);
        VHandleStopGame();
        return Answer(0);
    case EeNetClOp::HandleConnectionTimeoutWarning:
        OpLine(theArgs + " " + std::to_string(a));
        VHandleConnectionTimeoutWarning((unsigned long)a);
        return Answer(0);
    case EeNetClOp::ProcessLocalUserInput:
        OpLine(theArgs + " " + MsgTok(a) + " " + std::to_string(b));
        return Answer(ee_net_cl_process_user_input(sRust, m, (uint8_t)b, false));
    }
    OpLine(theArgs);
    return Answer(0);
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
    OrigClient* mOrig;
    uint32_t mServerTick;      // the server's ticks the world sends (it repeats and steps back too)
    uint32_t mUniqueID;        // the pause and unpause ids

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
    // the client's state, read through its accessors
    void State()
    {
        unsigned long ct, pt, trys, count;
        int slot, conn, mode;
        long last;
        bool paused;
        if (this->mRust) {
            ct = ee_net_cl_current_communications_tick(sRust);
            pt = ee_net_cl_current_physics_tick(sRust);
            slot = ee_net_cl_my_send_to_slot(sRust);
            trys = ee_net_cl_num_connection_trys(sRust);
            NEConnection* c = (NEConnection*)ee_net_cl_connection(sRust);
            conn = c != NULL ? c->mID : 0;
            mode = ee_net_cl_record_playback_mode(sRust);
            last = (long)(int32_t)BGet(EeNetCommsField::LastTickReceived);
            paused = BGet(EeNetCommsField::IsGamePaused) != 0;
        } else {
            ct = this->mOrig->GetCurrentCommunicationsTick();
            pt = this->mOrig->GetCurrentPhysicsTick();
            slot = this->mOrig->GetMySendToSlot();
            trys = this->mOrig->GetNumConnectionTrys();
            conn = this->mOrig->GetConnection() != NULL ? this->mOrig->GetConnection()->mID : 0;
            mode = this->mOrig->GetRecordPlaybackMode();
            last = this->mOrig->mLastTickReceived;
            paused = this->mOrig->mIsGamePaused;
        }
        count = (unsigned long)gQ->mMessages.size();
        gQ->Line("state ct=%lu pt=%lu slot=%d trys=%lu conn=c%d last=%ld paused=%d mode=%d msgs=%lu", ct, pt, slot, trys,
                 conn, last, (int)paused, mode, count);
        if (this->mRust)
            gQ->Op("state " + std::to_string(ct) + " " + std::to_string(pt) + " " + std::to_string(slot) + " " +
                   std::to_string(trys) + " c" + std::to_string(conn) + " " + std::to_string((int)mode));
    }
    NEMessage* NewMessage(unsigned char inType)
    {
        NEMessage* m = new NEMessage(inType);
        m->mCounts = false;
        return m;
    }
    // a message from the server
    NEMessage* ServerMessage()
    {
        NEMessage* m;
        uint32_t k = this->mRandom.Below(14);
        if (k < 5) {
            m = this->NewMessage(kNetMessageCommunicationsTick);
            this->mServerTick += this->mRandom.Below(4) == 0 ? 0 : 1;
            m->mTick = this->mServerTick - (this->mRandom.Below(5) == 0 ? this->mRandom.Below(3) : 0);
        } else if (k == 5) {
            m = this->NewMessage(kNetMessageStartGame);
            m->mOffset = this->mRandom.Below(1000);
        } else if (k == 6) {
            m = this->NewMessage(kNetMessageStopGame);
        } else if (k == 7 || k == 8) {
            m = this->NewMessage(k == 7 ? kNetMessageClientConnectionRequestResponse : kNetMessageClientReconnectionRequestResponse);
            m->mSlot = this->mRandom.Below(3) == 0 ? kSlotNone : (unsigned char)this->mRandom.Below(8);
            m->mError = this->mRandom.Below(3) == 0 ? 3 : kConnectionAccepted;
            m->mCT = this->mServerTick + this->mRandom.Below(3);
        } else if (k == 9 || k == 10) {
            m = this->NewMessage(k == 9 ? kNetMessagePauseGame : kNetMessageUnPauseGame);
            m->mS2aw = true;
            this->mUniqueID += this->mRandom.Below(3);
            m->mS2awID = this->mUniqueID;
        } else if (k == 11) {
            m = this->NewMessage(kNetMessageServerIsAlive);
        } else if (k == 12) {
            m = this->NewMessage(kNetMessageWorldDatabaseChecksum);
        } else {
            m = this->NewMessage(0x29);
        }
        return m;
    }
    // a transfer record (its id 4 is none the client knows); a file's name ends in the record
    void XferRecord(std::vector<unsigned char>& outRecord)
    {
        outRecord.assign(kCXferRecord, 0);
        static const unsigned char kIDs[] = {0, 1, 1, 1, 1, 2, 3, 4};   // (writes the most)
        unsigned char theID = kIDs[this->mRandom.Below(8)];
        outRecord[0] = theID;
        for (unsigned long i = 1; i < kCXferRecord; i++)
            outRecord[i] = (unsigned char)this->mRandom.Next();
        if (theID == 0) {
            uint32_t theXfer = this->mRandom.Below(50), theSize = this->mRandom.Below(100000);
            memcpy(&outRecord[1], &theXfer, 4);
            memcpy(&outRecord[5], &theSize, 4);
            std::string theName = "file" + std::to_string(this->mRandom.Below(100));
            memcpy(&outRecord[9], theName.c_str(), theName.size() + 1);
        }
        outRecord[kCXferRecord - 2] = 0;   // the data's last byte: a name always ends
    }

    void Step()
    {
        uint32_t theWhat = this->mRandom.Below(100);
        char theCall[160];
        long r = 0;
        NESynchronousClient* o = this->mOrig;
        if (theWhat < 5) {   // the world changes
            gQ->mClock += this->mRandom.Below(300);
            if (this->mRandom.Below(4) == 0)
                gQ->mPrecision = this->mRandom.Below(2) == 0 ? 0x20000 : kFPPrecision53;
            if (this->mRandom.Below(4) == 0)
                gQ->mRounding = this->mRandom.Below(2) == 0 ? 0x100 : 0;
            gQ->Line("env %lu %lx %lx", gQ->mClock, gQ->mPrecision, gQ->mRounding);
            return;
        }
        if (theWhat < 17) {   // time passes, the threads running at their deadlines
            unsigned long theTime = this->mRandom.Below(4) == 0 ? this->mRandom.Below(90000) : this->mRandom.Below(12000);
            snprintf(theCall, sizeof(theCall), "pass %lu", theTime);
            this->Call(theCall);
            gQ->Pass(theTime);
        } else if (theWhat < 30) {   // the server's packets
            if (gNet->mGame == 0)
                return;
            CConn& c = gNet->mConns[gNet->mGame];
            uint32_t theCount = 1 + this->mRandom.Below(3);
            std::string theIDs;
            for (uint32_t i = 0; i < theCount; i++) {
                int theID = this->mRandom.Below(16) == 0 ? 0 : this->ServerMessage()->mID;
                c.mPackets.push_back(theID);
                theIDs += " m" + std::to_string(theID);
            }
            snprintf(theCall, sizeof(theCall), "packets c%d%s", gNet->mGame, theIDs.c_str());
            this->Call(theCall);
            gQ->Set(c.mEvent);
        } else if (theWhat < 40) {   // transfer bytes (without a transfer connection: a connection reply with a slot)
            if (gNet->mXfer == 0) {
                if (gNet->mGame == 0)
                    return;
                NEMessage* m = this->NewMessage(kNetMessageClientConnectionRequestResponse);
                m->mSlot = (unsigned char)this->mRandom.Below(8);
                m->mError = kConnectionAccepted;
                m->mCT = this->mServerTick;
                gNet->mConns[gNet->mGame].mPackets.push_back(m->mID);
                snprintf(theCall, sizeof(theCall), "packets c%d m%d", gNet->mGame, m->mID);
                this->Call(theCall);
                gQ->Set(gNet->mConns[gNet->mGame].mEvent);
                this->Result(0);
                this->Call("run");
                gQ->LetRun();
                this->Result(0);
                this->State();
                return;
            }
            std::vector<unsigned char> theRecord;
            this->XferRecord(theRecord);
            size_t theCut = this->mRandom.Below(12) == 0 ? this->mRandom.Below(kCXferRecord) : kCXferRecord;
            gNet->mXferStream.insert(gNet->mXferStream.end(), theRecord.begin(), theRecord.begin() + theCut);
            snprintf(theCall, sizeof(theCall), "xfer c%d %s", gNet->mXfer, CHex(theRecord.data(), theCut).c_str());
            this->Call(theCall);
            gQ->Set(gNet->mConns[gNet->mXfer].mEvent);
        } else if (theWhat < 50) {   // a local input
            static const unsigned char kTypes[] = {0x40, 0x40, 0x40, kNetMessageClientRequestPause,
                                                   kNetMessageClientReconnectionRequest,
                                                   kNetMessageClientDisconnectionNotification};
            // (a burst sometimes: the throttle's ten a second)
            uint32_t theInputs = this->mRandom.Below(5) == 0 ? 8 + this->mRandom.Below(8) : 1;
            for (uint32_t i = 0; i < theInputs; i++) {
                if (i > 0)
                    this->Result(r);
                NEMessage* m = new NEMessage(kTypes[this->mRandom.Below(6)]);
                m->mCounts = this->mRandom.Below(4) != 0;
                m->mWhilePaused = this->mRandom.Below(5) == 0;
                m->mAvailable = this->mRandom.Below(8) != 0;
                bool theGuaranteed = this->mRandom.Below(3) == 0;
                unsigned char thePlayer = (unsigned char)(this->mRandom.Below(4) == 0 ? kSlotNone : this->mRandom.Below(4));
                snprintf(theCall, sizeof(theCall), "input m%d %d %d", m->mID, (int)theGuaranteed, thePlayer);
                this->Call(theCall);
                if (this->mRust)
                    r = ee_net_cl_process_user_input(sRust, m, thePlayer, theGuaranteed);
                else
                    r = theGuaranteed ? o->ProcessGuaranteedLocalUserInput(m, thePlayer) : o->ProcessLocalUserInput(m, thePlayer);
            }
        } else if (theWhat < 54) {
            this->Call("connect");
            UString theName("player"), theHost("server");
            UWideString theAddress("10.0.0.1");
            if (this->mRust) {
                const void* s[3] = {&theName, &theHost, &theAddress};
                r = ee_net_cl_connect(sRust, s);
            } else
                r = o->Connect(theName, theHost, theAddress);
        } else if (theWhat < 56) {
            this->Call("disconnect");
            r = this->mRust ? ee_net_cl_disconnect(sRust) : o->Disconnect();
        } else if (theWhat < 59) {
            NEMessage* m = this->NewMessage(kNetMessageClientConnectionRequest);
            snprintf(theCall, sizeof(theCall), "startconnect m%d", m->mID);
            this->Call(theCall);
            if (this->mRust)
                ee_net_cl_send_start_connect_message(sRust, m);
            else
                o->SendStartConnectMessage(m);
            m->Dereference();   // the caller's reference (the client holds its own)
        } else if (theWhat < 60) {
            bool theStart = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "search %d", (int)theStart);
            this->Call(theCall);
            r = this->mRust ? ee_net_cl_search_for_game(sRust, theStart) : o->SearchForGame(theStart);
        } else if (theWhat < 62) {
            unsigned long theIndex = this->mRandom.Below(3);
            snprintf(theCall, sizeof(theCall), "detected %lu", theIndex);
            this->Call(theCall);
            UString theName, theHost;
            UWideString theAddress;
            unsigned long theCount;
            if (this->mRust) {
                void* s[3] = {&theName, &theHost, &theAddress};
                theCount = ee_net_cl_detected_game_count(sRust);
                r = ee_net_cl_detected_game(sRust, (uint32_t)theIndex, s);
            } else {
                theCount = o->GetDetectedGameCount();
                r = o->GetDetectedGame(theIndex, theName, theHost, theAddress);
            }
            gQ->Line("detected %lu %s %s %s", theCount, theName.mText.c_str(), theHost.mText.c_str(), theAddress.mText.c_str());
        } else if (theWhat < 70) {   // the base class's fields
            uint32_t theField = this->mRandom.Below(5);
            long theValue;
            if (theField == 0) {
                static const long kLast[] = {-2, -1, 0};
                theValue = this->mRandom.Below(2) == 0 ? kLast[this->mRandom.Below(3)]
                                                       : (long)this->mServerTick - (long)this->mRandom.Below(3);
            } else if (theField == 1 || theField == 2)
                theValue = this->mRandom.Below(2);
            else
                theValue = this->mRandom.Below(500);
            snprintf(theCall, sizeof(theCall), "base %u %ld", theField, theValue);
            this->Call(theCall);
            if (this->mRust) {
                static const EeNetCommsField kFields[] = {EeNetCommsField::LastTickReceived, EeNetCommsField::IsGamePaused,
                                                          EeNetCommsField::IsMultiplayerSession,
                                                          EeNetCommsField::AveragePhysicsUpdateTime, EeNetCommsField::CtsProcessed};
                BSet(kFields[theField], (unsigned long)theValue);
            } else {
                if (theField == 0) o->mLastTickReceived = theValue;
                else if (theField == 1) o->mIsGamePaused = theValue != 0;
                else if (theField == 2) o->mIsMultiplayerSession = theValue != 0;
                else if (theField == 3) o->mAveragePhysicsUpdateTime = theValue;
                else o->mNumberOfCTsProcessedInPhysics = theValue;
            }
        } else if (theWhat < 76) {   // the setters
            uint32_t theKind = this->mRandom.Below(4);
            unsigned long theValue = theKind == 2 ? 1 + this->mRandom.Below(4) : this->mRandom.Below(300);
            snprintf(theCall, sizeof(theCall), "set %u %lu", theKind, theValue);
            this->Call(theCall);
            if (this->mRust) {
                if (theKind == 0) ee_net_cl_set_current_communications_tick(sRust, (uint32_t)theValue);
                else if (theKind == 1) ee_net_cl_set_current_physics_tick(sRust, (uint32_t)theValue);
                else if (theKind == 2) ee_net_cl_set_max_num_connection_trys(sRust, (uint32_t)theValue);
                else ee_net_cl_set_my_send_to_slot(sRust, (uint8_t)theValue);
            } else {
                if (theKind == 0) o->SetCurrentCommunicationsTick(theValue);
                else if (theKind == 1) o->SetCurrentPhysicsTick(theValue);
                else if (theKind == 2) o->SetMaxNumConnectionTrys(theValue);
                else o->SetMySendToSlot((unsigned char)theValue);
            }
        } else if (theWhat < 78) {
            bool theSlow = this->mRandom.Below(2) != 0;
            snprintf(theCall, sizeof(theCall), "playback %d", (int)theSlow);
            this->Call(theCall);
            if (this->mRust) {
                if (theSlow) ee_net_cl_set_slow_playback(sRust); else ee_net_cl_set_normal_playback(sRust);
            } else {
                if (theSlow) o->SetSlowPlayback(); else o->SetNormalPlayback();
            }
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
        this->mServerTick = 0;
        this->mUniqueID = 0;
        ee_net_comms_free(sComms);
        sComms = ee_net_comms_new(0);
        this->Call("new");
        if (this->mRust) {
            sRust = ee_net_cl_new(NULL, RustClHost, RustQHost, sComms, false);
            sDisconnect = [] { return (long)ee_net_cl_disconnect(sRust); };
        } else {
            this->mOrig = new OrigClient;
            OrigClient* o = this->mOrig;
            sDisconnect = [o] { return o->Disconnect(); };
        }
        this->Result(0);
        this->Call("start");
        if (this->mRust)
            ee_net_cl_start_threads(sRust);
        else
            this->mOrig->StartThreads();
        this->Result(0);
        this->State();
        for (int i = 0; i < inSteps; i++)
            this->Step();
        this->Call("stop");
        if (this->mRust)
            ee_net_cl_stop_threads(sRust);
        else
            this->mOrig->StopThreads();
        this->Result(0);
        this->Call("free");
        if (this->mRust)
            ee_net_cl_free(sRust);
        else
            delete this->mOrig;
        this->Result(0);
        gQ->Line("threads %d", (int)gQ->mThreads.size());
        if (gQ->mThreads.size() != 1) {
            fprintf(stderr, "ne_client_test: sequence %u (%s) left a thread running\n", inSeed, this->mRust ? "rust" : "orig");
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
    gQ = theWorld;
    gNet = new CNet;
    Sequence theSequence = {};
    theSequence.mRust = inRust;
    theSequence.Run(inSeed, inSteps);
    std::string theTrace = theWorld->mTrace;
    outOps = theWorld->mOps;
    // the messages the client leaks, and the connections it never closed, go with the world
    for (auto& m : theWorld->mMessages)
        delete m.second;
    delete gNet;
    gNet = NULL;
    delete theWorld;
    gQ = NULL;
    return theTrace;
}

int main(int argc, char** argv)
{
    int theCount = 1500, theSteps = 80, theRecorded = 40, theShow = 0;
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
        else if (strcmp(argv[i], "--verbose") == 0)
            sVerbose = true;
    }
    int theDiffering = 0;
    size_t theLines = 0;
    std::string theGolden;
    for (int s = 0; s < theCount; s++) {
        if (sVerbose) {
            fprintf(stderr, "sequence %d\n", s + 1);
            fflush(stderr);
        }
        std::string theOps;
        std::string theOrig = RunSide(false, (uint32_t)s + 1, theSteps, false, theOps);
        bool theRecord = thePrint != NULL && s < theRecorded;
        std::string theRust = RunSide(true, (uint32_t)s + 1, theSteps, theRecord, theOps);
        if (theRecord)
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
    // every kind of line but the states and the detected games
    for (auto& k : theKinds)
        if (k.first.compare(0, 6, "state ") != 0 && k.first.compare(0, 9, "detected ") != 0)
            printf("  %-28s %d\n", k.first.c_str(), k.second);
    printf("ne_client_test: %d sequences, %zu trace lines, %d differing\n", theCount, theLines, theDiffering);
    return theDiffering == 0 ? 0 : 1;
}
