// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit holds the out-of-line
// members only. ESVERIFY in the ctor is on line 43 (0x2b).
#include <windows.h>
#include <string.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "rust/ee_net.h"

// FUNCTION: LLE 0x10047301
// Creates the shared client/server state: tick counters, pause flag, bandwidth counters and debug lock
NESynchronousCommunications::NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics)
{
    this->mNumberOfCTsProcessedInPhysics = inNumberOfCTsProcessedInPhysics;
    this->mLastTickReceived = -2;
    this->mAveragePhysicsUpdateTime = 0;
    this->mAverageTimeBetweenPhysicsTicks = 0;
    this->mIsGamePaused = false;
    this->mNumHumanPlayers = 0;
    this->mPlayerNumber = 0;
    this->mIsMultiplayerSession = false;
    this->mLastReceiveBytes = 0;
    this->mLastSendBytes = 0;
    this->mLastReceiveTime = this->mLastSendTime = TMGetTime();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mDebugInfoLock) == keseOK, 43);
}

// FUNCTION: LLE 0x10047365
// Destroys the communications state (frees the debug lock)
NESynchronousCommunications::~NESynchronousCommunications()
{
    TSScheduler::DestroyCriticalSection(this->mDebugInfoLock);
}

// FUNCTION: LLE 0x10047375
// Returns bytes received per second since the last call
unsigned long NESynchronousCommunications::GetCurrentReceiveBytesPerSec()
{
    NEProtocol* theProtocol = NULL;
    NEProtocol::GetProtocol(0, theProtocol);
    unsigned long theTime = TMGetTime();
    unsigned long theBytes = theProtocol->GetBytesReceived();
    unsigned long theDeltaBytes = theBytes - this->mLastReceiveBytes;
    unsigned long theDeltaTime = theTime - this->mLastReceiveTime;
    // Fix: a second call within the same millisecond divided by zero; it now reports 0 and keeps measuring from
    // the previous call.
    if (theDeltaTime == 0)
        return 0;
    this->mLastReceiveBytes = theBytes;
    this->mLastReceiveTime = theTime;
    return theDeltaBytes * 1000 / theDeltaTime;
}

// FUNCTION: LLE 0x100473bc
// Returns bytes sent per second since the last call
unsigned long NESynchronousCommunications::GetCurrentSendBytesPerSec()
{
    NEProtocol* theProtocol = NULL;
    NEProtocol::GetProtocol(0, theProtocol);
    unsigned long theTime = TMGetTime();
    unsigned long theBytes = theProtocol->GetBytesSent();
    unsigned long theDeltaBytes = theBytes - this->mLastSendBytes;
    unsigned long theDeltaTime = theTime - this->mLastSendTime;
    // Fix: a second call within the same millisecond divided by zero; it now reports 0 and keeps measuring from
    // the previous call.
    if (theDeltaTime == 0)
        return 0;
    this->mLastSendBytes = theBytes;
    this->mLastSendTime = theTime;
    return theDeltaBytes * 1000 / theDeltaTime;
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

ptrdiff_t NESynchronousCommunications::QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    static const char* const kUnits[] = {"NELagLossSimulator.cpp", "NERecordPlayback.cpp", "NETimestampedMessage.cpp"};
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    TSEvent* theEvent = reinterpret_cast<TSEvent*>(a);
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
    switch (inOp) {
    case EeNetQOp::Time:
        return (ptrdiff_t)TMGetTime();
    case EeNetQOp::Verify:
        ESVerify(kUnits[a < 3 ? a : 0], (unsigned long)b, "(ee_net)");
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
        return TSScheduler::CreateThread(*reinterpret_cast<TSThread**>(c), a == 0 ? NEQueueLagThread : NEQueuePlaybackThread,
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
