// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit holds the out-of-line
// members only.
#include <windows.h>
#include <string.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "rust/ee_net.h"

// FUNCTION: LLE 0x10047301
// Creates the shared client/server state (ee_net comms.rs): the CTs processed given, the last tick received -2, the
// rest 0. (The debug lock, the bytes-per-second meters and their clock reads went: no caller.)
NESynchronousCommunications::NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics)
{
    this->mComms = ee_net_comms_new((uint32_t)inNumberOfCTsProcessedInPhysics);
}

// FUNCTION: LLE 0x10047365
// Gives up the base's reference to the state (the Rust server or client, freed first, held its own)
NESynchronousCommunications::~NESynchronousCommunications()
{
    ee_net_comms_free(this->mComms);
}

// The calls out of the lag simulator and the recorder (ee_net lagloss.rs, recplay.rs) that the original units made
// on the engine: each op is the statement NELagLossSimulator.cpp, NERecordPlayback.cpp or NETimestampedMessage.cpp
// made (the owner answers Deliver, the callbacks)
static unsigned long NEQueueLagThread(void* inSimulator)
{
    return ee_net_ll_thread(static_cast<const EeNetLagLoss*>(inSimulator));
}

static unsigned long NEQueuePlaybackThread(void* inRecorder)
{
    return ee_net_rp_thread(static_cast<const EeNetRecPlay*>(inRecorder));
}

// the client's threads (ee_net client.rs): its own lag simulator's and recorder's, its main and receive threads
static unsigned long NEClientLagThread(void* inSimulator)
{
    return ee_net_cl_lag_thread(inSimulator);
}

static unsigned long NEClientPlaybackThread(void* inRecorder)
{
    return ee_net_cl_playback_thread(inRecorder);
}

static unsigned long NEClientMainThread(void* inClient)
{
    return ee_net_cl_main_thread(static_cast<const EeNetClient*>(inClient));
}

static unsigned long NEClientReceiveThread(void* inClient)
{
    return ee_net_cl_receive_thread(static_cast<const EeNetClient*>(inClient));
}

static unsigned long NEServerPlaybackThread(void* inRecorder)
{
    return ee_net_sv_playback_thread(inRecorder);
}

static unsigned long NEServerMainThread(void* inServer)
{
    return ee_net_sv_main_thread(static_cast<const EeNetServer*>(inServer));
}

static unsigned long NEServerSendThread(void* inServer)
{
    return ee_net_sv_send_thread(static_cast<const EeNetServer*>(inServer));
}

static unsigned long NEServerXferThread(void* inServer)
{
    return ee_net_sv_xfer_thread(static_cast<const EeNetServer*>(inServer));
}

ptrdiff_t NESynchronousCommunications::QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    static const char* const kUnits[] = {"NELagLossSimulator.cpp", "NERecordPlayback.cpp", "NETimestampedMessage.cpp",
                                         "NESynchronousClient.cpp", "NESynchronousServer.cpp"};
    static unsigned long (*const kThreads[])(void*) = {NEQueueLagThread, NEQueuePlaybackThread, NEClientLagThread,
                                                       NEClientPlaybackThread, NEClientMainThread, NEClientReceiveThread,
                                                       NEServerPlaybackThread, NEServerMainThread, NEServerSendThread,
                                                       NEServerXferThread};
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    TSEvent* theEvent = reinterpret_cast<TSEvent*>(a);
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
    switch (inOp) {
    case EeNetQOp::Time:
        return (ptrdiff_t)TMGetTime();
    case EeNetQOp::Verify:
        ESVerify(kUnits[a < 5 ? a : 0], (unsigned long)b, "(ee_net)");
        return 0;
    case EeNetQOp::FpuStart:
        UMSetFloatingPointPrecision(kFPPrecision53);
        return 0;
    case EeNetQOp::FpuCheck:
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        return 0;
    case EeNetQOp::EventNew: {
        TSEvent* theNew = NULL;
        if (TSScheduler::CreateEvent(theNew) != keseOK)
            return 0;
        return (ptrdiff_t)theNew;
    }
    case EeNetQOp::EventFree:
        TSScheduler::DestroyEvent(theEvent);
        return 0;
    case EeNetQOp::EventSet:
        theEvent->Set();
        return 0;
    case EeNetQOp::EventClear:
        theEvent->Clear();
        return 0;
    case EeNetQOp::EventWait:
        return theEvent->Wait((unsigned long)b);
    case EeNetQOp::ThreadNew:
        return TSScheduler::CreateThread(*reinterpret_cast<TSThread**>(c), kThreads[a < 10 ? a : 0],
                                         reinterpret_cast<void*>(b), 0, false);
    case EeNetQOp::ThreadName:
        reinterpret_cast<TSThread*>(a)->SetName(UString(std::string(reinterpret_cast<const char*>(b), c).c_str()));
        return 0;
    case EeNetQOp::ThreadFree:
        TSScheduler::DestroyThread(reinterpret_cast<TSThread*>(a));
        return 0;
    case EeNetQOp::MsgReference:
        theMessage->Reference();
        return 0;
    case EeNetQOp::MsgDereference:
        theMessage->Dereference();
        return 0;
    case EeNetQOp::MsgExportSize:
        return (ptrdiff_t)theMessage->ExportSize();
    case EeNetQOp::MsgExport: {
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        theMessage->ExportMessage(&theData, theSize);
        memcpy(reinterpret_cast<void*>(b), theData, theSize < c ? theSize : c);
        delete[] theData;
        return (ptrdiff_t)theSize;
    }
    case EeNetQOp::MsgFromBytes:
        return (ptrdiff_t)NEMessage::ConvertMemoryToMessage(reinterpret_cast<unsigned char*>(a));
    case EeNetQOp::Deliver:
        return 0;
    case EeNetQOp::FileCreate:
        return theFile->Create();
    case EeNetQOp::FileDelete:
        return theFile->Delete();
    case EeNetQOp::FileOpen:
        return theFile->Open((unsigned long)b);
    case EeNetQOp::FileClose:
        theFile->Close();
        return 0;
    case EeNetQOp::FileWrite: {
        unsigned long theWritten;
        return theFile->Write(reinterpret_cast<const unsigned char*>(b), (unsigned long)c, theWritten);
    }
    case EeNetQOp::FileWriteU32:
        return theFile->Write((uint32_t)b);
    case EeNetQOp::FileReadU32:
        return theFile->Read(*reinterpret_cast<uint32_t*>(b));
    case EeNetQOp::FileRead: {
        unsigned long theRead;
        return theFile->Read(reinterpret_cast<unsigned char*>(b), (unsigned long)c, theRead);
    }
    }
    return 0;
}
