// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit opens with the ctor.
// Members in alphabetical order, then the three thread functions (Main, Send, Xfer); vector/map COMDATs last.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "lle/NEServer.h"
#include "rust/ee_net.h"
#include "platform/PlatSim.h"

// Release-build trace: compiled out, but its arguments are still evaluated
inline void NESSTrace(unsigned long inA) {}
inline void NESSTrace(unsigned long inA, unsigned long inB) {}

// FUNCTION: LLE 0x10047400
// Creates the game server on protocol 0 and publishes it as "EEService"; on failure stops with no threads/events
NESynchronousServer::NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT,
                                         unsigned long inMaxPhysicsTicksPerSecond, unsigned long inMinPhysicsTicksPerSecond, float inPhysicsTickCostFactor, unsigned long inMaxSendInterval,
                                         unsigned long inSendIntervalIncrement, unsigned long inSendIntervalDecrement,
                                         unsigned long inCurrentCommunicationsTick,
                                         unsigned long inCurrentPhysicsTick, UWideString& inName, bool inAllowNewConnections,
                                         bool inAnnounce, unsigned long inNetworkAdapter)
    : NESynchronousCommunications(inCurrentCommunicationsTick)
{
    ESVERIFY_AT(NEProtocol::GetCount() > 0, 138);
    long theResult = NEProtocol::GetProtocol(0, this->mProtocol);
    ESVERIFY_AT(theResult == keseOK, 140);

    ESVERIFY_AT(inMaxPlayers < knessPlayerSlotNotAvailable+1, 149);
    this->mMaxPlayers = inMaxPlayers;
    this->mDefaultSendInterval = inDefaultSendInterval;
    this->mInitialPTsPerCT = inPTsPerCT;
    this->mPTsPerSecond = 1000 / inDefaultSendInterval * inPTsPerCT;
    this->mGameStartTime = 0;
    this->mCurrentCommunicationsTick = inCurrentCommunicationsTick;
    this->mCurrentPhysicsTick = inCurrentPhysicsTick;
    this->mGameState = kServerStateOpen;
    this->mNumberOfPlayers = 0;
    this->mPTsPerCT = this->mInitialPTsPerCT;
    this->mLastCTMessageID = 0xffffffff;
    this->mLastPingTime = 0;
    this->mAllowNewConnections = inAllowNewConnections;
    this->mName = inName;
    this->mMaxPhysicsTicksPerSecond = inMaxPhysicsTicksPerSecond;
    this->mLowestReconnectCT = 0xffffffff;
    this->mMinPhysicsTicksPerSecond = inMinPhysicsTicksPerSecond;
    this->mSendToAllAndWaitMessageNumber = 0;
    this->mSendToAllAndWaitType = 0;
    this->mSendToAllAndWaitTimeout = 3000;
    this->mSendToAllAndWaitRetries = 40;
    this->mPing = false;
    this->mB200 = false;
    this->mWaitForReconnectsTime = 0;
    this->mU1D0 = 1;
    this->mReconnectCount = 0;
    this->mB205 = true;
    this->mProcessingSendToAllAndWait = false;
    this->mIsOOS = false;
    this->mPhysicsTickCostFactor = inPhysicsTickCostFactor;
    this->mMessageManager = ee_net_mm_new(this, NESynchronousServer::MessageManagerHost);
    ee_net_mm_register_slot(this->mMessageManager, kSlotServer);
    this->mNextChecksumCT = 40;
    this->mChecksumInterval = 1;
    this->mU17C = 100;
    this->mApplicationStartTime = TMGetTime();
    this->mSendInterval = 0xffffffff;
    this->mMaxSendInterval = inMaxSendInterval;
    this->mSendIntervalIncrement = inSendIntervalIncrement;
    // the tuning's state (ee_net tune.rs): its counts 0, the send interval's decrement
    this->mTune = ee_net_tu_new(this, NESynchronousServer::TuneHost, (uint32_t)inSendIntervalDecrement);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mInputAvailableEvent) == keseOK, 231);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferInputAvailableEvent) == keseOK, 234);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferAcceptEvent) == keseOK, 237);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferStuffWaitingToGoEvent) == keseOK, 240);
    theResult = this->mProtocol->Publish("EEService", this->mName, 2, this->mInputAvailableEvent, this->mXferInputAvailableEvent,
                                   this->mXferAcceptEvent, this->mServer, this->mAllowNewConnections, inAnnounce, inNetworkAdapter);
    if (theResult != kErrorOK) {
        this->mGameWasPublished = false;
        this->mRecordPlayback = NULL;
        return;
    }
    this->mGameWasPublished = true;

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mChangeSendIntervalEvent) == keseOK, 255);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadStartedEvent) == keseOK, 256);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadExitEvent) == keseOK, 257);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadFinishedEvent) == keseOK, 258);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mSendThreadStartedEvent) == keseOK, 259);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mSendThreadExitEvent) == keseOK, 260);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mSendThreadFinishedEvent) == keseOK, 261);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mSendThreadForceCTEvent) == keseOK, 262);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStartGameEvent) == keseOK, 263);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mPauseGameEvent) == keseOK, 264);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mUnpauseGameEvent) == keseOK, 265);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mResumeMigratedGame) == keseOK, 266);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStopGameEvent) == keseOK, 267);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mWaitForResponseEvent) == keseOK, 268);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mWaitForReconnectsEvent) == keseOK, 269);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStartPingEvent) == keseOK, 270);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStopPingEvent) == keseOK, 271);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferThreadStartedEvent) == keseOK, 272);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferThreadExitEvent) == keseOK, 273);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferThreadFinishedEvent) == keseOK, 274);
    this->mRecordPlayback = ee_net_rp_new(this, NESynchronousServer::QueueHost);
    ee_net_rp_pass_mode(this->mRecordPlayback);

    theResult = TSScheduler::CreateCriticalSection(this->mChecksumResponsesLock);
    ESVERIFY_AT(theResult == keseOK, 283);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSendToAllLock) == keseOK, 286);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mPauseUnpauseLock) == keseOK, 289);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSendCommunicationsTickLock) == keseOK, 292);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mIncomingPlayerInputsLock) == keseOK, 295);
}

// FUNCTION: LLE 0x10047c20
// Creates a zeroed server debug-info block
NECDIServer::NECDIServer()
    : mU0C(0), mU10(0), mU14(0), mU18(0), mU1C(0), mU20(0), mU24(0), mU28(0), mU2C(0), mU30(0), mU34(0), mU38(0),
      mU3C(0)
{
}

// FUNCTION: LLE 0x10047c65
// Destroys the server: frees events, threads, locks and the recorder, then closes the published server
NESynchronousServer::~NESynchronousServer()
{
    if (this->mGameWasPublished == true) {
        ee_net_rp_free(this->mRecordPlayback);
        TSScheduler::DestroyEvent(this->mChangeSendIntervalEvent);
        TSScheduler::DestroyEvent(this->mMainThreadStartedEvent);
        TSScheduler::DestroyEvent(this->mMainThreadExitEvent);
        TSScheduler::DestroyEvent(this->mMainThreadFinishedEvent);
        TSScheduler::DestroyEvent(this->mResumeMigratedGame);
        TSScheduler::DestroyEvent(this->mSendThreadStartedEvent);
        TSScheduler::DestroyEvent(this->mSendThreadExitEvent);
        TSScheduler::DestroyEvent(this->mSendThreadFinishedEvent);
        TSScheduler::DestroyEvent(this->mSendThreadForceCTEvent);
        TSScheduler::DestroyEvent(this->mStartGameEvent);
        TSScheduler::DestroyEvent(this->mPauseGameEvent);
        TSScheduler::DestroyEvent(this->mUnpauseGameEvent);
        TSScheduler::DestroyEvent(this->mStopGameEvent);
        TSScheduler::DestroyEvent(this->mWaitForResponseEvent);
        TSScheduler::DestroyEvent(this->mWaitForReconnectsEvent);
        TSScheduler::DestroyEvent(this->mXferThreadStartedEvent);
        TSScheduler::DestroyEvent(this->mXferThreadExitEvent);
        TSScheduler::DestroyEvent(this->mXferThreadFinishedEvent);
        TSScheduler::DestroyThread(this->mSendThread);
        TSScheduler::DestroyThread(this->mMainThread);
        TSScheduler::DestroyThread(this->mXferThread);
        TSScheduler::DestroyCriticalSection(this->mChecksumResponsesLock);
        TSScheduler::DestroyCriticalSection(this->mIncomingPlayerInputsLock);
        TSScheduler::DestroyCriticalSection(this->mSendToAllLock);
        TSScheduler::DestroyCriticalSection(this->mPauseUnpauseLock);
        TSScheduler::DestroyCriticalSection(this->mSendCommunicationsTickLock);
    }
    TSScheduler::DestroyEvent(this->mInputAvailableEvent);
    TSScheduler::DestroyEvent(this->mXferInputAvailableEvent);
    TSScheduler::DestroyEvent(this->mXferAcceptEvent);
    TSScheduler::DestroyEvent(this->mXferStuffWaitingToGoEvent);
    ee_net_mm_free(this->mMessageManager);
    ee_net_tu_free(this->mTune);
    if (this->mGameWasPublished == true)
        this->mServer->Close();
}

// New (port): the message manager's calls out. The manager (NEMessageManager.cpp before) is a Rust object
// (rust/crates/ee_net/src/msgmgr.rs); each op is the statement the original made on a message, this server, its
// protocol server or a transfer's file spec, which is the manager's own copy (FileClone/FileFree).
ptrdiff_t NESynchronousServer::MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServer);
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
    switch (inOp) {
    case EeNetMmOp::MsgReference:
        theMessage->Reference();
        return 0;
    case EeNetMmOp::MsgDereference:
        theMessage->Dereference();
        return 0;
    case EeNetMmOp::MsgType:
        return theMessage->GetType();
    case EeNetMmOp::MsgIsResponse:
        return theMessage->IsResponse(reinterpret_cast<NEMessage*>(b));
    case EeNetMmOp::MsgSlot:
        return theMessage->GetOriginatingSlot();
    case EeNetMmOp::MsgSendTo: {
        unsigned char* theData;
        unsigned long theSize;
        reinterpret_cast<NEMessage*>(b)->ExportMessage(&theData, theSize);
        long theResult = theServer->mServer->SendTo(a, theData, theSize);
        delete theData;
        return (uint32_t)theResult;
    }
    case EeNetMmOp::MsgTick:
        return static_cast<NEMCommunicationsTick*>(theMessage)->GetCurrentCommunicationsTick();
    case EeNetMmOp::NewResponse: {
        NEMessage* theSent = reinterpret_cast<NEMessage*>(b);
        NEMessage* theResponse;
        if (a == kNetMessageCommunicationsTick)
            theResponse = new NEMCommunicationsTickResponse(
                theServer->GetNumberOfCTsProcessedInPhysics(),
                static_cast<NEMCommunicationsTick*>(theSent)->GetCurrentCommunicationsTick(),
                theServer->GetAveragePhysicsUpdateTime());
        else if (a == kNetMessageUnPauseGame)
            theResponse = new NEMUnPauseGameResponse(static_cast<NEMUnPauseGame*>(theSent)->GetUniqueID());
        else
            theResponse = new NEMPauseGameResponse(static_cast<NEMPauseGame*>(theSent)->GetUniqueID());
        theResponse->SetOriginatingSlot(kSlotServer);
        return reinterpret_cast<ptrdiff_t>(theResponse);
    }
    case EeNetMmOp::LastTick:
        return theServer->mLastTickReceived;
    case EeNetMmOp::SetLastTick:
        theServer->mLastTickReceived = (long)a;
        return 0;
    case EeNetMmOp::CtsProcessed:
        return (uint32_t)theServer->mNumberOfCTsProcessedInPhysics;
    case EeNetMmOp::AvgPhysics:
        return (uint32_t)theServer->GetAveragePhysicsUpdateTime();
    case EeNetMmOp::ProcessSystemMessage:
        theServer->ProcessSystemMessage(theMessage);
        return 0;
    case EeNetMmOp::ForceCt:
        theServer->mSendThreadForceCTEvent->Set();
        return 0;
    case EeNetMmOp::LastCtMessageId:
        return (uint32_t)theServer->mLastCTMessageID;
    case EeNetMmOp::StartSendToAllAndWait:
        theServer->mSendToAllAndWaitMessageNumber = (unsigned long)a;
        theServer->mSendToAllAndWaitType = (unsigned char)b;
        theServer->mSendToAllAndWaitTimeout = (unsigned long)c;
        theServer->mSendToAllAndWaitRetries = (unsigned long)d;
        theServer->mWaitForResponseEvent->Set();
        return 0;
    case EeNetMmOp::Time:
        return (uint32_t)TMGetTime();
    case EeNetMmOp::ResetClientConnection:
        theServer->ResetClientConnection(a);
        return 0;
    case EeNetMmOp::XferStatus: {
        unsigned char theSlot = (unsigned char)a, thePercent = (unsigned char)b, theStatus = (unsigned char)d;
        uint32_t theID = (uint32_t)c;
        theServer->HandleXferStatus(theSlot, thePercent, theID, theStatus);
        return 0;
    }
    case EeNetMmOp::XferSend: {
        unsigned char theStatus = (unsigned char)b;
        unsigned long theSize = (unsigned long)d;
        NEMXferMessage* theXfer = new NEMXferMessage(theStatus, reinterpret_cast<unsigned char*>(c), theSize);
        unsigned char* theData = new unsigned char[theXfer->ExportSize()];
        theXfer->ExportMessage(&theData, theSize);
        theXfer->Dereference();
        long theResult = theServer->mServer->XferSendTo(a, theData, theSize);
        delete[] theData;
        return theResult == kErrorOK;
    }
    case EeNetMmOp::FileClone: {
        FSFileSpec* theCopy = new FSFileSpec;
        *theCopy = *theFile;
        return reinterpret_cast<ptrdiff_t>(theCopy);
    }
    case EeNetMmOp::FileFree:
        delete theFile;
        return 0;
    case EeNetMmOp::FileIsOpen:
        return theFile->GetIsOpen();
    case EeNetMmOp::FileOpen:
        return theFile->Open(kFileAccessRead) == kErrorOK;
    case EeNetMmOp::FileClose:
        theFile->Close();
        return 0;
    case EeNetMmOp::FileLength: {
        unsigned long theLength = 0;
        theFile->GetLength(theLength);
        return (uint32_t)theLength;
    }
    case EeNetMmOp::FileRead: {
        unsigned long theRead = 0;
        if (theFile->Read(reinterpret_cast<unsigned char*>(b), 0x400, theRead) != kErrorOK)
            return -1;
        return (uint32_t)theRead;
    }
    case EeNetMmOp::FileName:
        return reinterpret_cast<ptrdiff_t>(static_cast<const char*>(theFile->GetFile()));
    }
    return 0;
}

// FUNCTION: LLE 0x10047e73
// Cancels a file transfer
void NESynchronousServer::AbortXfer(unsigned long inXferID)
{
    ee_net_mm_abort_xfer(this->mMessageManager, inXferID);
}

// FUNCTION: LLE 0x10047e85
// Boots every client that has not answered a message (ee_net tune.rs)
void NESynchronousServer::BootAllLaggers(unsigned long inMessageNumber)
{
    ee_net_tu_boot_all_laggers(this->mTune, (uint32_t)inMessageNumber);
}

// FUNCTION: LLE 0x10048011
// Sends a message to every present player's slot; stops at the first send error and returns it
long NESynchronousServer::BroadcastMessage(NEMessage* inMessage)
{
    unsigned char* theData;
    unsigned long theSize;
    long theResult = kErrorOK;
    inMessage->ExportMessage(&theData, theSize);
    for (unsigned char i = 0; i < this->mMaxPlayers; i++)
        if (this->IsPlayerPresent(i)) {
            unsigned char theSlot = this->GetPlayerSendToSlot(i);
            if (theSlot != kSlotNone) {
                theResult = this->mServer->SendTo(theSlot, theData, theSize);
                if (theResult != kErrorOK)
                    break;
            }
        }
    delete theData;
    return theResult;
}

// FUNCTION: LLE 0x10048093
// Returns the message manager's current message number
unsigned long NESynchronousServer::GetMessageManagerCurrentMessageNumber()
{
    return ee_net_mm_current_number(this->mMessageManager);
}

// FUNCTION: LLE 0x100480a0
// Returns the send slot of a player, or 0xff when unknown
unsigned char NESynchronousServer::GetSendToSlotForPlayer(unsigned char inPlayer)
{
    unsigned char theSlot = 0xff;
    std::map<unsigned char, unsigned char>::iterator theIt = this->mSendToSlotArray.find(inPlayer);
    if (theIt != this->mSendToSlotArray.end())
        theSlot = theIt->second;
    return theSlot;
}

struct NESSSlotOrder { NESSSlotOrder(unsigned char inFirst) : mFirst(inFirst) {} bool operator()(unsigned char inA, unsigned char inB) const { return inA == this->mFirst && inB != this->mFirst; } unsigned char mFirst; };
// FUNCTION: LLE 0x10048139
// Collects world checksums per tick; once every human has answered, marks the out-of-sync slots
// (with 2 humans the client; else those off the most common checksum, server wins ties)
void NESynchronousServer::LogChecksumResponse(NEMWorldDatabaseChecksumResponse* inMessage)
{
    std::map<unsigned long, unsigned long> theCounts;
    std::multimap<unsigned long, unsigned char>::iterator theResponses;
    std::map<unsigned long, unsigned long>::iterator theCount;
    unsigned char theNumPlayers = this->mNumHumanPlayers;
    unsigned long theServersChecksum = 0;
    unsigned long theHighestCount = 0;
    unsigned long theMostCommon = 0;
    this->mChecksumResponsesLock->Enter();
    unsigned long theTick = inMessage->GetCommunicationsTick();
    unsigned long theChecksum = inMessage->GetWorldDatabaseChecksum();
    unsigned char theSlot = inMessage->GetOriginatingSlot();
    if (theTick > 0 && theNumPlayers > 1) {
        std::map<unsigned long, std::multimap<unsigned long, unsigned char> >::iterator theTickResponses =
            this->mChecksumResponses.find(theTick);
        if (theTickResponses == this->mChecksumResponses.end())
            this->mChecksumResponses[theTick].insert(std::make_pair(theChecksum, theSlot));
        else {
            this->mChecksumResponses[theTick].insert(std::make_pair(theChecksum, theSlot));
            if (this->mChecksumResponses[theTick].size() == theNumPlayers) {
                for (theResponses = this->mChecksumResponses[theTick].begin();
                     theResponses != this->mChecksumResponses[theTick].end(); ++theResponses) {
                    unsigned long theKey = theResponses->first;
                    theCounts[theKey] = theCounts[theKey] + 1;
                }
                if (theCounts.size() != 1) {
                    for (theResponses = this->mChecksumResponses[theTick].begin();
                         theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                        ;
                    if (this->mNumHumanPlayers == 2) {
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->second != 0xfe)
                                this->mOOSSlots.push_back(theResponses->second);
                    } else {
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->second == 0xfe) {
                                theServersChecksum = theResponses->first;
                                break;
                            }
                        for (theCount = theCounts.begin(); theCount != theCounts.end(); ++theCount)
                            if (theCount->second > theHighestCount ||
                                (theCount->second == theHighestCount && theCount->first == theServersChecksum)) {
                                theMostCommon = theCount->first;
                                theHighestCount = theCount->second;
                            }
                        for (theResponses = this->mChecksumResponses[theTick].begin();
                             theResponses != this->mChecksumResponses[theTick].end(); ++theResponses)
                            if (theResponses->first != theMostCommon)
                                this->mOOSSlots.push_back(theResponses->second);
                        if (this->mOOSSlots.size() > 1)
                            std::sort(this->mOOSSlots.begin(), this->mOOSSlots.end(), NESSSlotOrder(kSlotServer));
                    }
                    this->mIsOOS = true;
                }
                this->mChecksumResponses.erase(this->mChecksumResponses.begin(), theTickResponses);
            }
        }
    }
    this->mChecksumResponsesLock->Leave();
}

// FUNCTION: LLE 0x10048472
// Records a client's response to a message
long NESynchronousServer::LogResponse(NEMessage* inMessage)
{
    ee_net_mm_log_response(this->mMessageManager, inMessage);
    return kErrorOK;
}

// FUNCTION: LLE 0x10048484
// Sends a pause request to every client and waits for their answers
void NESynchronousServer::PauseGame()
{
    this->mPauseUnpauseLock->Enter();
    NEMessage* theMessage = new NEMPauseGame(this->mCurrentCommunicationsTick);
    if (theMessage != NULL) {
        theMessage->SetOriginatingSlot(kSlotServer);
        this->SendToAllAndWait(theMessage, 3000, 40);
        theMessage->Dereference();
    }
    this->mPauseUnpauseLock->Leave();
}

// FUNCTION: LLE 0x100484de
// Handles a client's tick response: replays its inputs and logs its response time and answer
void NESynchronousServer::ProcessCommunicationsTickResponse(NEMCommunicationsTickResponse* inMessage)
{
    NEMessage* theInput;
    for (unsigned long i = 0; i < inMessage->GetNumberOfPlayerInputs(); i++)
        if (inMessage->GetPlayerInput(i, theInput) == kErrorOK) {
            theInput->Reference();
            ee_net_rp_process(this->mRecordPlayback, theInput);
        }
    ee_net_mm_log_response_time(this->mMessageManager, inMessage->GetOriginatingSlot(), inMessage->GetLastCommunicationsTickProcessed(),
                                     inMessage->GetAveragePhysicsTickTime());
    ee_net_mm_log_response(this->mMessageManager, inMessage);
    inMessage->Dereference();
}

// FUNCTION: LLE 0x1004855d
// Takes a local input from the server's player, unless paused in multiplayer, and passes it to the recorder
long NESynchronousServer::ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    if ((!this->mIsMultiplayerSession || !this->mIsGamePaused || inMessage->GetCanSendWhilePaused() == true) &&
        inMessage->GetIsAvailableToSend() == true) {
        inMessage->SetOriginatingSlot(kSlotServer);
        inMessage->SetForSpecificSlot(inPlayer);
        this->PrepareLocalInput(inMessage);
        ee_net_rp_process(this->mRecordPlayback, inMessage);
    } else
        inMessage->Dereference();
    return kErrorOK;
}

// FUNCTION: LLE 0x100485b6
// Queues the server's own input for the next tick, dropping it while migrating or paused
long NESynchronousServer::ProcessLocalUserInput2(NEMessage* inMessage)
{
    if (this->IsMigrating() && inMessage->GetType() != 0x7c && inMessage->GetType() != 0x91)
        inMessage->Dereference();
    else if (this->mGameState == kServerStatePaused && this->mIsMultiplayerSession && !inMessage->GetIsAvailableToSend())
        inMessage->Dereference();
    else {
        inMessage->SetOriginatingSlot(kSlotServer);
        if (this->FilterPlayerInput(inMessage)) {
            this->mIncomingPlayerInputsLock->Enter();
            this->mIncomingPlayerInputs.push_back(inMessage);
            this->mIncomingPlayerInputsLock->Leave();
        }
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x10048637
// Reads one pending client packet and passes it to the recorder with its client slot
void NESynchronousServer::ProcessPendingPlayerInputs()
{
    unsigned char theBuffer[0x200];
    unsigned long theSize;
    unsigned long theIndex;
    if (this->mServer->ReceiveFrom(0x200, theBuffer, theSize, theIndex) != kErrorOK)
        return;
    NEMessage* theMessage = NEMessage::ConvertMemoryToMessage(theBuffer);
    if (theMessage != NULL) {
        theMessage->SetOriginatingSlot((unsigned char)theIndex);
        ee_net_rp_process(this->mRecordPlayback, theMessage);
    }
}

// FUNCTION: LLE 0x10048690
// Handles a client message: connect/reconnect requests, tick responses, checksums, pause answers; queues inputs
void NESynchronousServer::ProcessPendingPlayerInputs2(NEMessage* inMessage)
{
    ESVERIFY_AT(inMessage != NULL, 958);
    switch (inMessage->GetType()) {
    case kNetMessageAddPlayerResponse:
    case kNetMessageStartGameResponse:
    case kNetMessageStopGameResponse:
        inMessage->Dereference();
        break;
    case kNetMessageClientConnectionRequest:
        if (this->mGameState == kServerStateOpen) {
            ee_net_mm_register_slot(this->mMessageManager, inMessage->GetOriginatingSlot());
            this->ProcessSystemMessage(inMessage);
        }
        break;
    case kNetMessageClientDisconnectionNotification:
        this->ProcessSystemMessage(inMessage);
        break;
    case kNetMessageCommunicationsTickResponse:
        this->ProcessCommunicationsTickResponse(static_cast<NEMCommunicationsTickResponse*>(inMessage));
        break;
    case kNetMessagePlayerConnectionCheckResponse:
        this->ProcessSystemMessage(inMessage);
        inMessage->Dereference();
        break;
    case kNetMessageWorldDatabaseChecksumResponse:
        this->LogChecksumResponse(static_cast<NEMWorldDatabaseChecksumResponse*>(inMessage));
        inMessage->Dereference();
        break;
    case kNetMessagePauseGameResponse:
    case kNetMessageUnPauseGameResponse:
        ee_net_mm_log_response(this->mMessageManager, inMessage);
        inMessage->Dereference();
        break;
    case kNetMessageClientReconnectionRequest:
        this->mSendCommunicationsTickLock->Enter();
        if (this->mReconnectCount < this->mWaitForReconnectsTime && this->mGameState == kServerStateOpen) {
            ee_net_mm_register_slot(this->mMessageManager, inMessage->GetOriginatingSlot());
            if (static_cast<NEMClientReconnectionRequest*>(inMessage)->GetCurrentCT() < this->mLowestReconnectCT) {
                this->mLowestReconnectCT = static_cast<NEMClientReconnectionRequest*>(inMessage)->GetCurrentCT();
                this->mRestartCT = this->mLowestReconnectCT;
            }
            NESSTrace(this->GetCurrentCommunicationsTick(), this->mNumberOfCTsProcessedInPhysics);
            this->SendMessageTo(inMessage->GetOriginatingSlot(),
                          new NEMClientReconnectionRequestResponse(0, inMessage->GetOriginatingSlot(),
                                                                   this->GetCurrentCommunicationsTick()),
                          true);
            this->ProcessSystemMessage(inMessage);
            inMessage->Dereference();
            if (++this->mReconnectCount == this->mWaitForReconnectsTime) {
                this->mNumHumanPlayers = (unsigned char)this->mReconnectCount + 1;
                this->mResumeMigratedGame->Set();
            }
        } else
            inMessage->Dereference();
        this->mSendCommunicationsTickLock->Leave();
        break;
    default:
        if (this->FilterPlayerInput(inMessage)) {
            this->mIncomingPlayerInputsLock->Enter();
            this->mIncomingPlayerInputs.push_back(inMessage);
            this->mIncomingPlayerInputsLock->Leave();
        }
        break;
    }
}

// FUNCTION: LLE 0x100488a6
// Recorder callback: routes the server's own messages and client messages to their handlers
void NESynchronousServer::RecordPlaybackCallback(NEMessage* inMessage, void* inThisPointer)
{
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inThisPointer);

    ESVERIFY_AT(inThisPointer != NULL, 1104);
    if (inMessage->GetOriginatingSlot() == kSlotServer)
        theServer->ProcessLocalUserInput2(inMessage);
    else
        theServer->ProcessPendingPlayerInputs2(inMessage);
}

// The recorder's mode (ee_net recplay.rs; a server that was not published has none: passing)
unsigned char NESynchronousServer::GetRecordPlaybackMode() const
{
    return ee_net_rp_mode(this->mRecordPlayback);
}

// The recorder's calls out: its callback is this server's, the rest common
ptrdiff_t NESynchronousServer::QueueHost(void* inServer, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    if (inOp != EeNetQOp::Deliver)
        return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d);
    NESynchronousServer::RecordPlaybackCallback(reinterpret_cast<NEMessage*>(b), inServer);
    return 0;
}

// FUNCTION: LLE 0x100488df
// Registers a client slot with the message manager
void NESynchronousServer::RegisterSlot(unsigned char inSlot)
{
    ee_net_mm_register_slot(this->mMessageManager, inSlot);
}

// FUNCTION: LLE 0x100488f1
// Announces the removal of every player that uses the given send slot
void NESynchronousServer::RemovePlayer(unsigned char inSlot, bool inA)
{
    for (unsigned char i = 0; i < this->mMaxPlayers; i++)
        if (this->IsPlayerPresent(i) && inSlot == this->GetPlayerSendToSlot(i)) {
            NEMessage* theMessage = new NEMRemovePlayer(i, false, false, false);
            if (theMessage != NULL)
                this->ProcessLocalUserInput(theMessage, kSlotNone);
        }
}

// FUNCTION: LLE 0x10048983
// Announces a player's removal from a running game; a booted client's slot is unregistered and counted out
void NESynchronousServer::RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    unsigned char thePlayer = 0xfe;
    bool theFound = true;
    if (inSlot != kSlotServer) {
        theFound = false;
        for (thePlayer = 1; thePlayer < this->mMaxPlayers; thePlayer++) {
            this->GetPlayerSendToSlot(thePlayer);
            if (!this->IsPlayerPresent(thePlayer) || this->GetPlayerSendToSlot(thePlayer) != inSlot)
                continue;
            if (thePlayer != kSlotServer && inWasBooted == true) {
                this->UnregisterSlot(inSlot);
                this->mNumHumanPlayers--;
            }
            theFound = true;
            break;
        }
    }
    if (!theFound)
        return;
    NEMessage* theMessage = new NEMRemovePlayer(thePlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
    if (theMessage != NULL)
        this->ProcessLocalUserInput(theMessage, kSlotNone);
}

// FUNCTION: LLE 0x10048a4a
// Starts or stops searching the network for "EEService" games
long NESynchronousServer::SearchForGame(bool inStart)
{
    UString theName("EEService");
    UString theHost("Anything");
    return this->mProtocol->SearchForGame(theHost, theName, inStart);
}

// FUNCTION: LLE 0x10048a9d
// Sends the next communications tick with queued inputs (or a keep-alive while tuning fails)
// and removes out-of-sync players, subtracting them from the human count a second time
void NESynchronousServer::SendCommunicationsTick(bool inGenerateChecksum, bool inB)
{
    unsigned char thePTs;
    this->mSendCommunicationsTickLock->Enter();
    this->mSendToAllLock->Enter();
    if (!this->mIsOOS && inB && this->TuneServer() != true) {
        NEMessage* theMessage = new NEMServerIsAlive();
        theMessage->SetOriginatingSlot(kSlotServer);
        ee_net_mm_send(this->mMessageManager, theMessage, false);
        theMessage->Dereference();
    } else {
        if (!this->mProcessingSendToAllAndWait && this->mSendToAllAndWaitRequests.size() > 0) {
            this->mSendToAllAndWaitRequests.front().mMessage->SetSendToAllAndWaitMessageID(this->mCurrentCommunicationsTick);
            ee_net_mm_send_to_all_and_wait(this->mMessageManager, this->mSendToAllAndWaitRequests.front().mMessage,
                                              this->mSendToAllAndWaitRequests.front().mTimeout,
                                              this->mSendToAllAndWaitRequests.front().mRetries);
            this->mSendToAllAndWaitRequests.front().mMessage->GetMessageLabel();
            this->mSendToAllAndWaitRequests.front().mMessage->Dereference();
            this->mSendToAllAndWaitRequests.erase(this->mSendToAllAndWaitRequests.begin());
            this->mProcessingSendToAllAndWait = true;
        }
        if (this->mGameState != kServerStateRunning) {
            thePTs = this->mGameState == kServerStatePaused || this->mProcessingSendToAllAndWait == true ? 0 : 0xff;
            if (this->mPing == true && TMGetTime() - this->mLastPingTime > 3000) {
                this->mLastPingTime = TMGetTime();
                this->ProcessLocalUserInput(new NEMPlayerConnectionCheck(this->mLastPingTime), kSlotNone);
            }
        } else if (!this->mIsOOS && !this->mProcessingSendToAllAndWait && (!this->ServerSlot28() || this->mIsMultiplayerSession != true)) {
            thePTs = this->mPTsPerCT;
            this->mCurrentPhysicsTick += thePTs;
        } else
            thePTs = 0;
        bool theGenerateChecksum = ((inGenerateChecksum && thePTs > 0) || this->mIsOOS) && this->mIsMultiplayerSession;
        NEMCommunicationsTick* theTick = new NEMCommunicationsTick(this->mCurrentCommunicationsTick, thePTs, this->mPTsPerSecond,
                                                            theGenerateChecksum, this->mIsOOS);
        if (theTick != NULL) {
            unsigned long theSize = theTick->ExportSize();
            if (!this->mProcessingSendToAllAndWait) {
                this->mIncomingPlayerInputsLock->Enter();
                while (this->mIncomingPlayerInputs.size() > 0) {
                    NEMessage* theInput = this->mIncomingPlayerInputs.front();
                    if (theInput->ExportSize() + theSize + 0x1c >= 0x240)
                        break;
                    this->mIncomingPlayerInputs.pop_front();
                    theTick->AddPlayerInput(theInput);
                    theSize += theInput->ExportSize();
                }
                this->mIncomingPlayerInputsLock->Leave();
            }
            theTick->SetOriginatingSlot(kSlotServer);
            this->mLastCTMessageID = ee_net_mm_send(this->mMessageManager, theTick, true);
            this->mCurrentCommunicationsTick++;
            theTick->Dereference();
            if (this->mIsOOS) {
                this->BeforeRemovingPlayers();
                for (unsigned long i = 0; i < this->mOOSSlots.size(); i++)
                    this->RemovePlayerFromGameInProgress(this->mOOSSlots[i], true, true, false);
                this->mIsOOS = false;
                // (RemovePlayerFromGameInProgress has already counted each removed human out of mNumHumanPlayers)
                this->mOOSSlots.clear();
            }
        }
    }
    this->mSendToAllLock->Leave();
    this->mSendCommunicationsTickLock->Leave();
}

// TEST ONLY (platform/PlatSim.h): the headless simulation's next communications tick, sent by the physics thread as
// soon as it has run the last one. The rate is the configured maximum (what TuneForSlowCPU settles on when Min = Max
// PTs per Second, the harness's --pt-rate pin) and the PTs per tick those of the default send interval (14 at 40 PTs
// per second and 350 ms), never tuned by wall-clock response times, so every run cuts the game into the same ticks.
void NESynchronousServer::SimCommunicationsTick()
{
    this->mSendCommunicationsTickLock->Enter();
    this->mPTsPerSecond = (unsigned char)this->mMaxPhysicsTicksPerSecond;
    unsigned long thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mDefaultSendInterval));
    this->mPTsPerCT = thePTs < 0xff ? (unsigned char)thePTs : 0xfe;
    this->mSendCommunicationsTickLock->Leave();
    this->SendCommunicationsTick(false, false);
}

// FUNCTION: LLE 0x10048e31
// Sends a message to one client slot
long NESynchronousServer::SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
{
    return ee_net_mm_send_to(this->mMessageManager, inMessage, inSlot, inGuaranteed);
}

// FUNCTION: LLE 0x10048e4b
// Sends a message to every client
long NESynchronousServer::SendMessageToAll(NEMessage* inMessage, bool inGuaranteed)
{
    return ee_net_mm_send(this->mMessageManager, inMessage, inGuaranteed);
}

// FUNCTION: LLE 0x10048e61
// Queues a message to be sent to every client with answers awaited (timeout, retries) on a later tick
unsigned long NESynchronousServer::SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout,
                                                    unsigned long inRetries)
{
    NESendToAllAndWaitRequest theRequest;
    this->mSendToAllLock->Enter();
    inMessage->GetMessageLabel();
    inMessage->Reference();
    theRequest.mTimeout = inTimeout;
    theRequest.mRetries = inRetries;
    theRequest.mMessage = inMessage;
    this->mSendToAllAndWaitRequests.push_back(theRequest);
    this->mSendToAllLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x10048eb4
// Sets the lowest communications tick in the message manager
void NESynchronousServer::SetMessageManagerLowestCTs(unsigned long inCT)
{
    ee_net_mm_set_lowest_cts(this->mMessageManager, inCT);
}

// FUNCTION: LLE 0x10048ec6
// Sets the message manager's current message number
void NESynchronousServer::SetMessageManagerCurrentMessageNumber(unsigned long inNumber)
{
    ee_net_mm_set_current_number(this->mMessageManager, inNumber);
}

// FUNCTION: LLE 0x10048ed9
// Starts the server's main, send and transfer threads ("CommMain", "CommSend", "CommXfer") once published
void NESynchronousServer::StartThreads()
{
    if (this->mGameWasPublished != true)
        return;
    ESVERIFY_AT(TSScheduler::CreateThread(this->mMainThread, NESynchronousServer::Main, this) == keseOK, 1584);
    ESVERIFY_AT(TSScheduler::CreateThread(this->mSendThread, NESynchronousServer::Send, this) == keseOK, 1585);
    ESVERIFY_AT(TSScheduler::CreateThread(this->mXferThread, NESynchronousServer::Xfer, this) == keseOK, 1586);
    this->mMainThread->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
    this->mSendThread->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
    this->mMainThreadStartedEvent->Wait(kTSWaitInfinite);
    this->mSendThreadStartedEvent->Wait(kTSWaitInfinite);
    this->mXferThreadStartedEvent->Wait(kTSWaitInfinite);
    this->mMainThread->SetName(UString("CommMain"));
    this->mSendThread->SetName(UString("CommSend"));
    this->mXferThread->SetName(UString("CommXfer"));
}

// FUNCTION: LLE 0x10049028
// Stops the send, main and transfer threads and waits for each to finish
void NESynchronousServer::StopThreads()
{
    if (this->mGameWasPublished != true)
        return;
    this->mSendThreadExitEvent->Set();
    this->mSendThreadFinishedEvent->Wait(kTSWaitInfinite);
    this->mMainThreadExitEvent->Set();
    this->mMainThreadFinishedEvent->Wait(kTSWaitInfinite);
    this->mXferThreadExitEvent->Set();
    this->mXferThreadFinishedEvent->Wait(kTSWaitInfinite);
}

// FUNCTION: LLE 0x1004969a
// Tunes for network and CPU speed and reports (or clears) the lagging players; returns false while lagging
// (ee_net tune.rs: TuneServer, TuneForSlowNetwork, TuneForSlowCPU and the booting they do)
bool NESynchronousServer::TuneServer()
{
    return ee_net_tu_tune_server(this->mTune);
}

// The tuning's calls out (ee_net tune.rs), each the statement the original members made
ptrdiff_t NESynchronousServer::TuneHost(void* inServer, EeNetTuOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServer);
    const EeNetMsgMgr* theManager = theServer->mMessageManager;
    switch (inOp) {
    case EeNetTuOp::Get:
        switch ((EeNetTuField)a) {
        case EeNetTuField::SendInterval:
            return (uint32_t)theServer->mSendInterval;
        case EeNetTuField::SendIntervalIncrement:
            return (uint32_t)theServer->mSendIntervalIncrement;
        case EeNetTuField::MaxSendInterval:
            return (uint32_t)theServer->mMaxSendInterval;
        case EeNetTuField::PTsPerSecond:
            return theServer->mPTsPerSecond;
        case EeNetTuField::MaxPTsPerSecond:
            return (uint32_t)theServer->mMaxPhysicsTicksPerSecond;
        case EeNetTuField::MinPTsPerSecond:
            return (uint32_t)theServer->mMinPhysicsTicksPerSecond;
        case EeNetTuField::CostFactor: {
            uint32_t theBits;
            memcpy(&theBits, &theServer->mPhysicsTickCostFactor, 4);
            return theBits;
        }
        case EeNetTuField::DefaultSendInterval:
            return (uint32_t)theServer->mDefaultSendInterval;
        case EeNetTuField::LastCtMessageId:
            return (uint32_t)theServer->mLastCTMessageID;
        case EeNetTuField::GameState:
            return theServer->mGameState;
        case EeNetTuField::IsMultiplayerSession:
            return theServer->mIsMultiplayerSession;
        case EeNetTuField::CurrentCommunicationsTick:
            return (uint32_t)theServer->GetCurrentCommunicationsTick();
        case EeNetTuField::PTsPerCt:
            return theServer->mPTsPerCT;
        }
        return 0;
    case EeNetTuOp::Set:
        if ((EeNetTuField)a == EeNetTuField::SendInterval)
            theServer->mSendInterval = (uint32_t)b;
        else if ((EeNetTuField)a == EeNetTuField::PTsPerSecond)
            theServer->mPTsPerSecond = (unsigned char)b;
        else if ((EeNetTuField)a == EeNetTuField::PTsPerCt)
            theServer->mPTsPerCT = (unsigned char)b;
        return 0;
    case EeNetTuOp::EventSet:
        (a == 0 ? theServer->mChangeSendIntervalEvent : a == 1 ? theServer->mPauseGameEvent : theServer->mUnpauseGameEvent)->Set();
        return 0;
    case EeNetTuOp::MmOutstanding:
        return ee_net_mm_outstanding(theManager, (uint32_t)a);
    case EeNetTuOp::MmHighestResponseTime:
        return ee_net_mm_highest_response_time(theManager);
    case EeNetTuOp::MmHighestCpu:
        return ee_net_mm_highest_cpu(theManager);
    case EeNetTuOp::MmLowestCt:
        return ee_net_mm_lowest_ct(theManager);
    case EeNetTuOp::MmResend:
        ee_net_mm_resend(theManager, (uint32_t)a);
        return 0;
    case EeNetTuOp::MmWho:
        return ee_net_mm_who_has_not_responded(theManager, (uint32_t)a);
    case EeNetTuOp::MmWhoNext:
        return ee_net_mm_who_has_not_responded_next(theManager, (uint8_t)a, (uint32_t)b);
    case EeNetTuOp::MmNumClients:
        return ee_net_mm_num_clients(theManager);
    case EeNetTuOp::MmSlowestResponder:
        return ee_net_mm_slowest_responder(theManager);
    case EeNetTuOp::MmSlowestClientCpu:
        return ee_net_mm_slowest_client_cpu(theManager);
    case EeNetTuOp::MmSetForceCt:
        ee_net_mm_set_force_ct(theManager, a != 0);
        return 0;
    case EeNetTuOp::UnregisterSlot:
        theServer->UnregisterSlot((unsigned char)a);
        return 0;
    case EeNetTuOp::BeforeRemovingPlayers:
        theServer->BeforeRemovingPlayers();
        return 0;
    case EeNetTuOp::BootPlayer:
        theServer->BootPlayer((unsigned char)a, b != 0, c != 0);
        return 0;
    case EeNetTuOp::ReportLaggers: {
        const unsigned char* theFirst = reinterpret_cast<const unsigned char*>(a);
        std::vector<unsigned char> theSlots(theFirst, theFirst + b);
        theServer->ReportLaggers(theSlots, c != 0);
        return 0;
    }
    case EeNetTuOp::RemovePlayerInput: {
        unsigned char theSlot = (unsigned char)a;
        NEMessage* theMessage = new NEMRemovePlayer(theSlot, true, false, false);
        theServer->ProcessLocalUserInput(theMessage, kSlotNone);
        return 0;
    }
    }
    return 0;
}

// FUNCTION: LLE 0x10049753
// Unregisters a client slot (never the server or no-slot)
void NESynchronousServer::UnregisterSlot(unsigned char inSlot)
{
    if (inSlot != kSlotServer && inSlot != kSlotNone)
        ee_net_mm_unregister_slot(this->mMessageManager, inSlot);
}

// FUNCTION: LLE 0x1004976e
// Sends an unpause request to every client and waits for their answers
void NESynchronousServer::UnpauseGame()
{
    this->mPauseUnpauseLock->Enter();
    NEMessage* theMessage = new NEMUnPauseGame(this->mCurrentCommunicationsTick);
    if (theMessage != NULL) {
        theMessage->SetOriginatingSlot(kSlotServer);
        this->SendToAllAndWait(theMessage, 3000, 40);
        theMessage->Dereference();
    }
    this->mPauseUnpauseLock->Leave();
}

// FUNCTION: LLE 0x100497c8
// Queues a file transfer to a client slot and wakes the transfer thread
long NESynchronousServer::XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot,
                                           unsigned long& outXferID, bool inP5)
{
    uint32_t theXferID = 0;
    long theResult = ee_net_mm_add_xfer(this->mMessageManager, &inFile, (uint32_t)inUserData, inSlot, &theXferID)
                         ? kErrorOK : kErrorNotFound;
    if (theResult == kErrorOK)
        outXferID = theXferID;
    if (theResult == kErrorOK)
        this->mXferStuffWaitingToGoEvent->Set();
    return theResult;
}

// FUNCTION: LLE 0x10049802
// Server main thread: handles start/pause/stop, client packets, answer waits with retries and reconnect waits
unsigned long NESynchronousServer::Main(void* inServerObject)
{
    unsigned long theSavedSendInterval = kTSWaitInfinite;
    bool theExit = false;
    unsigned long theDeadline = 0;
    bool theTimedOut = false;
    unsigned long theWaitState = 0;
    unsigned long theRetries = 0;
    unsigned long theTimeout;
    NEConnection* theConnection;
    TSEvent* theEvents[11];
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServerObject);
    ESVERIFY_AT(inServerObject != NULL, 2203);
    theServer->mMainThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theServer->mStartGameEvent;
    theEvents[1] = theServer->mPauseGameEvent;
    theEvents[2] = theServer->mStopGameEvent;
    theEvents[3] = theServer->mMainThreadExitEvent;
    theEvents[4] = theServer->mInputAvailableEvent;
    theEvents[5] = theServer->mWaitForResponseEvent;
    theEvents[6] = theServer->mWaitForReconnectsEvent;
    theEvents[7] = theServer->mXferInputAvailableEvent;
    theEvents[8] = theServer->mXferAcceptEvent;
    theEvents[9] = theServer->mResumeMigratedGame;
    theEvents[10] = theServer->mUnpauseGameEvent;
    theServer->mGameState = kServerStateOpen;
    theServer->mSendInterval = theServer->mDefaultSendInterval;
    theServer->mChangeSendIntervalEvent->Set();
    theTimeout = kTSWaitInfinite;
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (TSEvent::WaitAny(theEvents, 11, theTimeout)) {
        case 0:
            theServer->mStartGameEvent->Clear();
            theServer->mGameState = kServerStateRunning;
            theServer->mSendInterval = theServer->mDefaultSendInterval;
            theServer->mChangeSendIntervalEvent->Set();
            if (theWaitState == 2) {
                theTimeout = kTSWaitInfinite;
                theWaitState = 0;
            }
            break;
        case 1:
            theServer->mPauseGameEvent->Clear();
            if (theWaitState != 2 && theServer->mGameState == kServerStateRunning) {
                theServer->mGameState = kServerStatePaused;
                theServer->mIsGamePaused = true;
            }
            break;
        case kTSWaitTimeout:
            theTimedOut = true;
            break;
        case kTSWaitFailed:
            PlatGetLastError();

            ESVERIFY_AT("::WaitForMultipleObjects failed" != NULL, 2254);
            break;
        case 2:
            theServer->mStopGameEvent->Clear();
            if (theWaitState != 2) {
                NEMessage* theMessage = new NEMStopGame();
                if (theMessage != NULL) {
                    theMessage->SetOriginatingSlot(kSlotServer);
                    ee_net_mm_send(theServer->mMessageManager, theMessage, true);
                    theMessage->Dereference();
                    theServer->mGameState = kServerStateOpen;
                    theServer->mSendInterval = kTSWaitInfinite;
                    theServer->mChangeSendIntervalEvent->Set();
                }
            }
            break;
        case 3:
            theServer->mMainThreadExitEvent->Clear();
            if (theWaitState != 2)
                theExit = true;
            break;
        case 4:
            theServer->mInputAvailableEvent->Clear();
            theServer->ProcessPendingPlayerInputs();
            if (theWaitState == 1)
                theTimedOut = true;
            if (theWaitState == 2 && TMGetTime() > theDeadline)
                theServer->HandleReconnectWaitTimedOut();
            break;
        case 5:
            theServer->mWaitForResponseEvent->Clear();
            theSavedSendInterval = theServer->mSendInterval;
            if (theServer->mIsMultiplayerSession) {
                theServer->mSendInterval = 3000;
                theServer->mChangeSendIntervalEvent->Set();
            }
            theTimeout = theSavedSendInterval;
            theDeadline = TMGetTime() + theServer->mSendToAllAndWaitTimeout;
            theRetries = 0;
            theWaitState = 1;
            break;
        case 6:
            theServer->mWaitForReconnectsEvent->Clear();
            theTimeout = 1000;
            theDeadline = TMGetTime() + 20000;
            theWaitState = 2;
            break;
        case 7:
            theServer->mXferInputAvailableEvent->Clear();
            break;
        case 8:
            theServer->mXferAcceptEvent->Clear();
            theServer->mProtocol->AcceptForXfer(theServer->mServer, theConnection);
            break;
        case 9:
            {
                theServer->mResumeMigratedGame->Clear();
                theServer->mWaitForReconnectsTime = 0;
                unsigned long theNow = TMGetTime();
                if (theNow < theDeadline && theDeadline - theNow > 3500)
                    theDeadline = theNow + 3000;
            }
            break;
        case 10:
            theServer->mUnpauseGameEvent->Clear();
            if (theWaitState != 2 && theServer->mGameState == kServerStatePaused) {
                theServer->mIsGamePaused = false;
                theServer->mGameState = kServerStateRunning;
            }
            break;
        }
        if (theTimedOut == true) {
            switch (theWaitState) {
            case 1:
                if (ee_net_mm_outstanding(theServer->mMessageManager, 
                        theServer->mSendToAllAndWaitMessageNumber) == 0) {
                    theServer->mSendToAllLock->Enter();
                    switch (theServer->mSendToAllAndWaitType) {
                    case kNetMessagePauseGame:
                        if (theServer->mGameState != kServerStatePaused)
                            theServer->mPauseGameEvent->Set();
                        break;
                    case kNetMessageUnPauseGame:
                        if (theServer->mGameState == kServerStatePaused)
                            theServer->mUnpauseGameEvent->Set();
                        break;
                    }
                    theServer->mProcessingSendToAllAndWait = false;
                    theServer->mSendInterval = theSavedSendInterval;
                    theServer->mChangeSendIntervalEvent->Set();
                    theServer->mSendToAllLock->Leave();
                    theTimeout = kTSWaitInfinite;
                    theWaitState = 0;
                } else if (TMGetTime() > theDeadline) {
                    if (++theRetries >= theServer->mSendToAllAndWaitRetries) {
                        theServer->BootAllLaggers(theServer->mSendToAllAndWaitMessageNumber);
                        theServer->mSendInterval = theServer->mDefaultSendInterval;
                        theServer->mChangeSendIntervalEvent->Set();
                        theTimeout = kTSWaitInfinite;
                    } else {
                        ee_net_mm_resend(theServer->mMessageManager, theServer->mSendToAllAndWaitMessageNumber);
                        theDeadline = TMGetTime() + theServer->mSendToAllAndWaitTimeout;
                    }
                }
                break;
            case 2:
                if (TMGetTime() > theDeadline)
                    theServer->HandleReconnectWaitTimedOut();
                break;
            }
            theTimedOut = false;
        }
    } while (!theExit);
    NESSTrace(theServer->GetCurrentCommunicationsTick());
    theServer->mMainThreadFinishedEvent->Set();
    return 0;
}

// FUNCTION: LLE 0x10049d13
// Server send thread: sends a communications tick every send interval (with a checksum when due)
unsigned long NESynchronousServer::Send(void* inServerObject)
{
    static unsigned long sStartTime = TMGetTime();
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServerObject);
    ESVERIFY_AT(inServerObject != NULL, 2647);
    unsigned long theSendInterval = theServer->mSendInterval;
    long theTimeout = theSendInterval;
    if (theSendInterval != kTSWaitInfinite) {
        theTimeout = theSendInterval - 50;
        if (theTimeout < 0)
            theTimeout = 1;
    }
    unsigned long theLastTime = TMGetTime();
    theServer->mSendThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    TSEvent* theEvents[3];
    theEvents[0] = theServer->mChangeSendIntervalEvent;
    theEvents[1] = theServer->mSendThreadExitEvent;
    theEvents[2] = theServer->mSendThreadForceCTEvent;
    for (;;) {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        long theWaitResult = TSEvent::WaitAny(theEvents, 3, theTimeout);
        unsigned long theNow = TMGetTime();
        long theDrift = theLastTime - theNow + theSendInterval;
        switch (theWaitResult) {
        case 0:
            theServer->mChangeSendIntervalEvent->Clear();
            theSendInterval = theServer->mSendInterval;
            if (theSendInterval == kTSWaitInfinite)
                theTimeout = kTSWaitInfinite;
            else {
                theTimeout = theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theNow;
            break;
        case 1:
            theServer->mSendThreadExitEvent->Clear();
            theServer->mSendThreadFinishedEvent->Set();
            return 0;
        case 2:
            theServer->mSendThreadForceCTEvent->Clear();
            if (!(gPlatSimMode && theServer->SimGameRunning()))   // the simulation's physics thread sends them
                theServer->SendCommunicationsTick(false, true);
            ee_net_mm_set_force_ct(theServer->mMessageManager, false);
            if (theSendInterval == kTSWaitInfinite)
                theTimeout = kTSWaitInfinite;
            else {
                theTimeout = theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theNow;
            break;
        case kTSWaitTimeout:
            if (gPlatSimMode && theServer->SimGameRunning()) {   // the simulation's physics thread sends them
                theTimeout = 50;
                theLastTime = theNow;
                break;
            }
            if (theServer->mCurrentCommunicationsTick >= theServer->mNextChecksumCT) {
                theServer->SendCommunicationsTick(true, true);
                theServer->mNextChecksumCT += theServer->mChecksumInterval;
            } else
                theServer->SendCommunicationsTick(false, true);
            theTimeout = kTSWaitInfinite;
            if (theSendInterval != kTSWaitInfinite) {
                theTimeout = theDrift + theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theDrift + theNow;
            break;
        case kTSWaitFailed:
            PlatGetLastError();
            break;
        }
    }
}

// FUNCTION: LLE 0x10049eee
// Server transfer thread: pumps queued file transfers every 500 ms until none remain
unsigned long NESynchronousServer::Xfer(void* inServerObject)
{
    bool theExit = false;
    bool theXfersWaiting = false;
    unsigned long theTimeout;
    TSEvent* theEvents[2];
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServerObject);
    ESVERIFY_AT(inServerObject != NULL, 2824);
    theTimeout = kTSWaitInfinite;
    theServer->mXferThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theServer->mXferThreadExitEvent;
    theEvents[1] = theServer->mXferStuffWaitingToGoEvent;
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (TSEvent::WaitAny(theEvents, 2, theTimeout)) {
        case 0:
            theServer->mXferThreadExitEvent->Clear();
            theExit = true;
            break;
        case 1:
            theServer->mXferStuffWaitingToGoEvent->Clear();
            theXfersWaiting = true;
            theTimeout = 500;
            break;
        case kTSWaitTimeout:
            if (theXfersWaiting) {
                theXfersWaiting = ee_net_mm_process_xfers(theServer->mMessageManager);
                if (!theXfersWaiting)
                    theTimeout = kTSWaitInfinite;
            }
            PlatSleep(1);
            break;
        case kTSWaitFailed:
            PlatGetLastError();
            break;
        }
    } while (!theExit);
    theServer->mXferThreadFinishedEvent->Set();
    return 0;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// TEMPLATE: LLE 0x1004a000
// TEMPLATE: LLE 0x1004a02c
// TEMPLATE: LLE 0x1004a07d
// TEMPLATE: LLE 0x1004a0a8
// TEMPLATE: LLE 0x1004a0d4
// TEMPLATE: LLE 0x1004a135
// TEMPLATE: LLE 0x1004a160
// TEMPLATE: LLE 0x1004a19a
// TEMPLATE: LLE 0x1004a208
// TEMPLATE: LLE 0x1004a233
// TEMPLATE: LLE 0x1004a26d
// TEMPLATE: LLE 0x1004a295
// TEMPLATE: LLE 0x1004a2c0
// TEMPLATE: LLE 0x1004a304
// TEMPLATE: LLE 0x1004a313
// TEMPLATE: LLE 0x1004a329
// TEMPLATE: LLE 0x1004a33a
// TEMPLATE: LLE 0x1004a38a
// TEMPLATE: LLE 0x1004a461
// TEMPLATE: LLE 0x1004a4d2
// TEMPLATE: LLE 0x1004a76d
// TEMPLATE: LLE 0x1004a796
// TEMPLATE: LLE 0x1004a92a
// TEMPLATE: LLE 0x1004a953
// TEMPLATE: LLE 0x1004a985
// TEMPLATE: LLE 0x1004aa7b
// TEMPLATE: LLE 0x1004aaa9
// TEMPLATE: LLE 0x1004aacb
// TEMPLATE: LLE 0x1004aaf9
// TEMPLATE: LLE 0x1004ab0b
// TEMPLATE: LLE 0x1004ab20
// TEMPLATE: LLE 0x1004a9c6
// TEMPLATE: LLE 0x1004ab36
// TEMPLATE: LLE 0x1004ab5f
// TEMPLATE: LLE 0x1004ac2c
// TEMPLATE: LLE 0x1004ac5f
// TEMPLATE: LLE 0x1004acf8
// TEMPLATE: LLE 0x1004acfd
// TEMPLATE: LLE 0x1004ad34
// TEMPLATE: LLE 0x1004ad70
// TEMPLATE: LLE 0x1004ae5a
// TEMPLATE: LLE 0x1004ae7e
// TEMPLATE: LLE 0x1004aebe
// TEMPLATE: LLE 0x1004aee7
// TEMPLATE: LLE 0x1004af0c
// TEMPLATE: LLE 0x1004af74
// TEMPLATE: LLE 0x1004afca
// TEMPLATE: LLE 0x1004afdd
// TEMPLATE: LLE 0x1004b01e
// The std::sort chain over vector<unsigned char> 0x1004a9c6..0x1004b04f (Dinkum 2.33 <algorithm>, NESSSlotOrder).
// TEMPLATE: LLE 0x1004a067
// TEMPLATE: LLE 0x1004a440
