// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit opens with the ctor.
// Members in alphabetical order, then the three thread functions (Main, Send, Xfer); vector/map COMDATs last.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NERecordPlayback.h"
#include "lle/NESynchronousServer.h"
#include "lle/NEServer.h"
#include "lle/NEMessageManager.h"

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
    this->mSlowCPUCount = 0;
    this->mNetTuneState = 0;
    this->mNetConsecutiveLate = 0;
    this->mNetResendCount = 0;
    this->mNetLateEpisodes = 0;
    this->mNetAtMaxCount = 0;
    this->mHighestResponseTime = 0;
    this->mNetLateCount = 0;
    this->mLagReportActive = false;
    this->mReconnectCount = 0;
    this->mB205 = true;
    this->mProcessingSendToAllAndWait = false;
    this->mLagWarning = false;
    this->mIsOOS = false;
    this->mPhysicsTickCostFactor = inPhysicsTickCostFactor;
    this->mMessageManager = new NEMessageManager(this);
    this->mMessageManager->RegisterSlot(kSlotServer);
    this->mNextChecksumCT = 40;
    this->mChecksumInterval = 1;
    this->mU17C = 100;
    this->mApplicationStartTime = TMGetTime();
    this->mSendInterval = 0xffffffff;
    this->mMaxSendInterval = inMaxSendInterval;
    this->mSendIntervalIncrement = inSendIntervalIncrement;
    this->mSendIntervalDecrement = inSendIntervalDecrement;

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
    this->mRecordPlayback = new NERecordPlayback(NESynchronousServer::RecordPlaybackCallback, this);

    ESVERIFY_AT(this->mRecordPlayback != NULL, 278);
    this->mRecordPlayback->PassMode();

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
        delete this->mRecordPlayback;
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
    delete this->mMessageManager;
    if (this->mGameWasPublished == true)
        this->mServer->Close();
}

// FUNCTION: LLE 0x10047e73
// Cancels a file transfer
void NESynchronousServer::AbortXfer(unsigned long inXferID)
{
    this->mMessageManager->AbortXfer(inXferID);
}

// FUNCTION: LLE 0x10047e85
// Boots every client that has not answered a message, pausing the game around the removal when running
void NESynchronousServer::BootAllLaggers(unsigned long inMessageNumber)
{
    unsigned char theCount = 0;
    unsigned char theSlot;
    if (this->mGameState == kServerStateRunning) {
        this->mPauseGameEvent->Set();
        this->BeforeRemovingPlayers();
        this->mUnpauseGameEvent->Set();
    }
    unsigned char* theLaggers = new unsigned char[this->mMessageManager->mNumClients];
    theSlot = this->mMessageManager->WhoHasNotResponded(inMessageNumber);
    if (theSlot != kSlotNone) {
        theLaggers[0] = theSlot;
        theCount = 1;
    }
    while ((theSlot = this->mMessageManager->WhoHasNotRespondedNext(theSlot, inMessageNumber)) != 0xff)
        theLaggers[theCount++] = theSlot;
    for (unsigned char i = 0; i < theCount; i++)
        this->BootPlayer(theLaggers[i], true, false);
    if (theLaggers != NULL)
        delete theLaggers;
}

// FUNCTION: LLE 0x10047f45
// Boots the client picked as slowest CPU in a multiplayer game and announces its removal
void NESynchronousServer::BootTheSlowestClientCPU()
{
    if (this->mIsMultiplayerSession != true)
        return;
    unsigned char theSlot = this->GetSlowestClientCPU();
    if (theSlot == kSlotNone || theSlot == kSlotServer)
        return;
    this->UnregisterSlot(theSlot);
    NEMessage* theMessage = new NEMRemovePlayer(theSlot, true, false, false);
    if (theMessage != NULL)
        this->ProcessLocalUserInput(theMessage, kSlotNone);
}

// FUNCTION: LLE 0x10047fc1
// Boots the slowest-responding client, pausing the game around the removal when running
void NESynchronousServer::BootTheSlowestResponder()
{
    if (this->mGameState == kServerStateRunning) {
        this->mPauseGameEvent->Set();
        this->BeforeRemovingPlayers();
        this->mUnpauseGameEvent->Set();
    }
    unsigned char theSlot = this->mMessageManager->GetSlowestResponder();
    if (theSlot != kSlotNone)
        this->BootPlayer(theSlot, true, false);
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
    return this->mMessageManager->mCurrentMessageNumber;
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

// FUNCTION: LLE 0x100480d3
// Returns the client with the highest recorded CPU load; 0xff if none has any
unsigned char NESynchronousServer::GetSlowestClientCPU()
{
    std::map<unsigned char, NEMMClient*>::iterator theClients;
    unsigned char theSlowest = 0xff;
    unsigned long theHighestTotal = 0;
    for (theClients = this->mMessageManager->mClients.begin(); theClients != this->mMessageManager->mClients.end();
         ++theClients) {
        unsigned long theTotal = 0;
        for (unsigned long i = 0; i < 240; i++)
            theTotal += theClients->second->mCPUHistory[i];
        // the client with the highest CPU load over its history (ties: the higher slot)
        if (theTotal > 0 && theTotal >= theHighestTotal) {
            theHighestTotal = theTotal;
            theSlowest = theClients->first;
        }
    }
    return theSlowest;
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
    return this->mMessageManager->LogResponse(inMessage);
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
            this->mRecordPlayback->Process(theInput);
        }
    this->mMessageManager->LogResponseTime(inMessage->GetOriginatingSlot(), inMessage->GetLastCommunicationsTickProcessed(),
                                     inMessage->GetAveragePhysicsTickTime());
    this->mMessageManager->LogResponse(inMessage);
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
        this->mRecordPlayback->Process(inMessage);
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
        this->mRecordPlayback->Process(theMessage);
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
            this->mMessageManager->RegisterSlot(inMessage->GetOriginatingSlot());
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
        this->mMessageManager->LogResponse(inMessage);
        inMessage->Dereference();
        break;
    case kNetMessageClientReconnectionRequest:
        this->mSendCommunicationsTickLock->Enter();
        if (this->mReconnectCount < this->mWaitForReconnectsTime && this->mGameState == kServerStateOpen) {
            this->mMessageManager->RegisterSlot(inMessage->GetOriginatingSlot());
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

// FUNCTION: LLE 0x100488df
// Registers a client slot with the message manager
void NESynchronousServer::RegisterSlot(unsigned char inSlot)
{
    this->mMessageManager->RegisterSlot(inSlot);
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
        this->mMessageManager->Send(theMessage, false);
        theMessage->Dereference();
    } else {
        if (!this->mProcessingSendToAllAndWait && this->mSendToAllAndWaitRequests.size() > 0) {
            this->mSendToAllAndWaitRequests.front().mMessage->SetSendToAllAndWaitMessageID(this->mCurrentCommunicationsTick);
            this->mMessageManager->SendToAllAndWait(this->mSendToAllAndWaitRequests.front().mMessage,
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
            this->mLastCTMessageID = this->mMessageManager->Send(theTick, true);
            this->mCurrentCommunicationsTick++;
            theTick->Dereference();
            if (this->mIsOOS) {
                this->BeforeRemovingPlayers();
                for (unsigned long i = 0; i < this->mOOSSlots.size(); i++)
                    this->RemovePlayerFromGameInProgress(this->mOOSSlots[i], true, true, false);
                this->mIsOOS = false;
                this->mNumHumanPlayers -= (unsigned char)this->mOOSSlots.size();
                this->mOOSSlots.clear();
            }
        }
    }
    this->mSendToAllLock->Leave();
    this->mSendCommunicationsTickLock->Leave();
}

// FUNCTION: LLE 0x10048e31
// Sends a message to one client slot
long NESynchronousServer::SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
{
    return this->mMessageManager->SendTo(inMessage, inSlot, inGuaranteed);
}

// FUNCTION: LLE 0x10048e4b
// Sends a message to every client
long NESynchronousServer::SendMessageToAll(NEMessage* inMessage, bool inGuaranteed)
{
    return this->mMessageManager->Send(inMessage, inGuaranteed);
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
    this->mMessageManager->SetLowestCTs(inCT);
}

// FUNCTION: LLE 0x10048ec6
// Sets the message manager's current message number
void NESynchronousServer::SetMessageManagerCurrentMessageNumber(unsigned long inNumber)
{
    this->mMessageManager->mCurrentMessageNumber = inNumber;
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
    this->mMainThread->SetPriority(THREAD_PRIORITY_TIME_CRITICAL);
    this->mSendThread->SetPriority(THREAD_PRIORITY_TIME_CRITICAL);
    this->mMainThreadStartedEvent->Wait(INFINITE);
    this->mSendThreadStartedEvent->Wait(INFINITE);
    this->mXferThreadStartedEvent->Wait(INFINITE);
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
    this->mSendThreadFinishedEvent->Wait(INFINITE);
    this->mMainThreadExitEvent->Set();
    this->mMainThreadFinishedEvent->Wait(INFINITE);
    this->mXferThreadExitEvent->Set();
    this->mXferThreadFinishedEvent->Wait(INFINITE);
}

// FUNCTION: LLE 0x1004907e
// Sets physics ticks per second from the slowest CPU within the limits; flags a client after repeated slow ticks
bool NESynchronousServer::TuneForSlowCPU()
{
    NESSTrace(this->GetCurrentCommunicationsTick());
    this->mMessageManager->GetLowestCT();
    unsigned short theCPU = this->mMessageManager->GetHighestCPU();
    if (theCPU == 0)
        theCPU = 1;
    this->mPTsPerSecond = (unsigned char)(1000.0f / (theCPU * this->mPhysicsTickCostFactor));
    if (this->mPTsPerSecond > this->mMaxPhysicsTicksPerSecond) {
        this->mPTsPerSecond = (unsigned char)this->mMaxPhysicsTicksPerSecond;
        if (this->mSlowCPUCount < 1)
            this->mSlowCPUCount = 0;
        else
            this->mSlowCPUCount--;
    } else if (this->mPTsPerSecond <= this->mMinPhysicsTicksPerSecond) {
        this->mSlowCPUCount += 2;
        this->mPTsPerSecond = (unsigned char)this->mMinPhysicsTicksPerSecond;
    }
    if (this->mSlowCPUCount >= 15) {
        this->mSlowCPUCount = 0;
        if (this->GetSlowestClientCPU() != kSlotNone) {
            unsigned char theSlot = this->GetSlowestClientCPU();
            this->mSlotsD0.push_back(theSlot);
        }
    }
    unsigned long thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
    if (thePTs < 0xff)
        this->mPTsPerCT = (unsigned char)thePTs;
    else
        this->mPTsPerCT = 0xfe;
    return true;
}

// FUNCTION: LLE 0x10049196
// Adapts the tick send interval to client response times; resends, warns, or boots laggers when late
bool NESynchronousServer::TuneForSlowNetwork()
{
    bool theChanged = false;
    bool theResult = true;
    unsigned char theSlot;
    unsigned long thePTs;
    if (this->mSendInterval < 500)
        this->mSendIntervalDecrement = 10;
    else if (this->mSendInterval < 1000)
        this->mSendIntervalDecrement = 20;
    else
        this->mSendIntervalDecrement = 100;
    switch (this->mNetTuneState) {
    case 0:
        this->mNetLateEpisodes++;
        this->mNetTuneState = 1;
        if (this->mNetLateEpisodes == 2) {
            theResult = false;
            break;
        }
    case 1:
        if (this->mMessageManager->NumberOfOutstandingResponses(this->mLastCTMessageID) > 0) {
            theResult = false;
            this->mNetConsecutiveLate++;
            this->mNetLateCount++;
            if (this->mNetConsecutiveLate >= 2) {
                this->mNetConsecutiveLate = 0;
                this->mNetTuneState = 2;
            }
            break;
        }
        this->mNetConsecutiveLate = 0;
        this->mHighestResponseTime = this->mMessageManager->GetHighestResponseTime();
        if (this->mHighestResponseTime > this->mSendInterval) {
            if (this->mHighestResponseTime + this->mSendIntervalIncrement > this->mMaxSendInterval) {
                if (this->mNetAtMaxCount >= 20) {
                    this->mNetTuneState = 5;
                    this->mNetAtMaxCount = 0;
                } else {
                    this->mNetAtMaxCount++;
                    this->mSendInterval = this->mMaxSendInterval;
                    this->mChangeSendIntervalEvent->Set();
                    thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                    if (thePTs < 0xff)
                        this->mPTsPerCT = (unsigned char)thePTs;
                    else
                        this->mPTsPerCT = 0xfe;
                    theChanged = true;
                }
            } else {
                this->mSendInterval = this->mHighestResponseTime + this->mSendIntervalIncrement;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
            }
        } else if (this->mNetLateCount >= 10) {
            if (this->mSendIntervalIncrement + this->mSendInterval > this->mMaxSendInterval)
                this->mNetTuneState = 5;
            else {
                this->mSendInterval = this->mSendIntervalIncrement + this->mSendInterval;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
                theChanged = true;
            }
        } else if (this->mSendInterval - this->mSendIntervalDecrement > 350) {
            if (this->mNetAtMaxCount > 0)
                this->mNetAtMaxCount--;
            this->mSendInterval = this->mSendInterval - this->mSendIntervalDecrement;
            this->mChangeSendIntervalEvent->Set();
            thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
            if (thePTs < 0xff)
                this->mPTsPerCT = (unsigned char)thePTs;
            else
                this->mPTsPerCT = 0xfe;
        }
        this->mNetLateCount = 0;
        break;
    case 2:
        if (this->mMessageManager->NumberOfOutstandingResponses(this->mLastCTMessageID) > 0) {
            this->mMessageManager->Resend(this->mLastCTMessageID);
            this->mNetResendCount++;
            theResult = false;
            this->mNetTuneState = 3;
            break;
        }
        this->mNetResendCount = 0;
        this->mNetTuneState = 0;
        this->mNetLateEpisodes = 0;
        break;
    case 3:
        if (this->mMessageManager->NumberOfOutstandingResponses(this->mLastCTMessageID) > 0) {
            if (this->mNetResendCount % 3 == 0) {
                this->mLagWarning = true;
                this->mMessageManager->Resend(this->mLastCTMessageID);
            }
            this->mNetResendCount++;
            theResult = false;
            if (this->mNetResendCount > 10) {
                this->mNetResendCount = 0;
                this->mNetTuneState = 4;
            }
            if (this->mSendIntervalIncrement + this->mSendInterval <= this->mMaxSendInterval) {
                this->mNetAtMaxCount++;
                this->mSendInterval = this->mMaxSendInterval;
                this->mChangeSendIntervalEvent->Set();
                thePTs = (unsigned long)(this->mPTsPerSecond / (1000.0f / this->mSendInterval));
                if (thePTs < 0xff)
                    this->mPTsPerCT = (unsigned char)thePTs;
                else
                    this->mPTsPerCT = 0xfe;
            }
            theChanged = true;
            break;
        }
        this->mNetResendCount = 0;
        this->mNetTuneState = 0;
        this->mNetLateEpisodes = 0;
        break;
    case 4:
        this->mNetTuneState = 0;
        if (this->mNetLateEpisodes < 2 && this->mGameState != kServerStateOpen && this->mGameState != kServerStateClosed) {
            this->mLagWarning = true;
            theResult = false;
            break;
        }
        this->BootAllLaggers(this->mLastCTMessageID);
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mNetLateEpisodes = 0;
        theResult = false;
        break;
    case 5:
        this->mNetTuneState = 0;
        if (this->mNetLateEpisodes < 2 && this->mGameState != kServerStateOpen && this->mGameState != kServerStateClosed) {
            this->mLagWarning = true;
            theResult = false;
            break;
        }
        this->BootTheSlowestResponder();
        this->mSendInterval = this->mDefaultSendInterval;
        this->mChangeSendIntervalEvent->Set();
        this->mNetLateEpisodes = 0;
        theResult = false;
        break;
    }
    if (!theChanged)
        return theResult;
    theSlot = this->mMessageManager->WhoHasNotResponded(this->mLastCTMessageID);
    if (theSlot != kSlotNone)
        this->mSlotsD0.push_back(theSlot);
    theSlot = this->mMessageManager->WhoHasNotRespondedNext(theSlot, this->mLastCTMessageID);
    while (theSlot != kSlotNone) {
        this->mSlotsD0.push_back(theSlot);
        theSlot = this->mMessageManager->WhoHasNotRespondedNext(theSlot, this->mLastCTMessageID);
    }
    return theResult;
}

// FUNCTION: LLE 0x1004969a
// Tunes for network and CPU speed and reports (or clears) the lagging players; returns false while lagging
bool NESynchronousServer::TuneServer()
{
    bool theResult = this->TuneForSlowNetwork();
    if (!theResult)
        this->mMessageManager->mForceCTWhenAllResponded = true;
    if (this->mGameState == kServerStateRunning) {
        if (theResult == true)
            theResult = this->TuneForSlowCPU();
        else
            this->TuneForSlowCPU();
    }
    if (this->mSlotsD0.size() != 0) {
        this->ReportLaggers(this->mSlotsD0, this->mLagWarning);
        this->mLagReportActive = true;
        this->mSlotsD0.clear();
        this->mLagWarning = false;
    } else if (this->mLagReportActive) {
        this->ReportLaggers(this->mSlotsD0, this->mLagWarning);
        this->mLagReportActive = false;
        this->mLagWarning = false;
    }
    return theResult;
}

// FUNCTION: LLE 0x10049753
// Unregisters a client slot (never the server or no-slot)
void NESynchronousServer::UnregisterSlot(unsigned char inSlot)
{
    if (inSlot != kSlotServer && inSlot != kSlotNone)
        this->mMessageManager->UnregisterSlot(inSlot);
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
    long theResult = this->mMessageManager->AddXfer(inFile, inUserData, inSlot, outXferID, inP5);
    if (theResult == kErrorOK)
        this->mXferStuffWaitingToGoEvent->Set();
    return theResult;
}

// FUNCTION: LLE 0x10049802
// Server main thread: handles start/pause/stop, client packets, answer waits with retries and reconnect waits
unsigned long NESynchronousServer::Main(void* inServerObject)
{
    unsigned long theSavedSendInterval = INFINITE;
    bool theExit = false;
    unsigned long theDeadline = 0;
    bool theTimedOut = false;
    unsigned long theWaitState = 0;
    unsigned long theRetries = 0;
    unsigned long theTimeout;
    NEConnection* theConnection;
    HANDLE theEvents[11];
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServerObject);
    ESVERIFY_AT(inServerObject != NULL, 2203);
    theServer->mMainThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theServer->mStartGameEvent->mHandle;
    theEvents[1] = theServer->mPauseGameEvent->mHandle;
    theEvents[2] = theServer->mStopGameEvent->mHandle;
    theEvents[3] = theServer->mMainThreadExitEvent->mHandle;
    theEvents[4] = theServer->mInputAvailableEvent->mHandle;
    theEvents[5] = theServer->mWaitForResponseEvent->mHandle;
    theEvents[6] = theServer->mWaitForReconnectsEvent->mHandle;
    theEvents[7] = theServer->mXferInputAvailableEvent->mHandle;
    theEvents[8] = theServer->mXferAcceptEvent->mHandle;
    theEvents[9] = theServer->mResumeMigratedGame->mHandle;
    theEvents[10] = theServer->mUnpauseGameEvent->mHandle;
    theServer->mGameState = kServerStateOpen;
    theServer->mSendInterval = theServer->mDefaultSendInterval;
    theServer->mChangeSendIntervalEvent->Set();
    theTimeout = INFINITE;
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (WaitForMultipleObjects(11, theEvents, FALSE, theTimeout)) {
        case WAIT_OBJECT_0:
            theServer->mStartGameEvent->Clear();
            theServer->mGameState = kServerStateRunning;
            theServer->mSendInterval = theServer->mDefaultSendInterval;
            theServer->mChangeSendIntervalEvent->Set();
            if (theWaitState == 2) {
                theTimeout = INFINITE;
                theWaitState = 0;
            }
            break;
        case WAIT_OBJECT_0 + 1:
            theServer->mPauseGameEvent->Clear();
            if (theWaitState != 2 && theServer->mGameState == kServerStateRunning) {
                theServer->mGameState = kServerStatePaused;
                theServer->mIsGamePaused = true;
            }
            break;
        case WAIT_TIMEOUT:
            theTimedOut = true;
            break;
        case WAIT_FAILED:
            GetLastError();

            ESVERIFY_AT("::WaitForMultipleObjects failed" != NULL, 2254);
            break;
        case WAIT_OBJECT_0 + 2:
            theServer->mStopGameEvent->Clear();
            if (theWaitState != 2) {
                NEMessage* theMessage = new NEMStopGame();
                if (theMessage != NULL) {
                    theMessage->SetOriginatingSlot(kSlotServer);
                    theServer->mMessageManager->Send(theMessage, true);
                    theMessage->Dereference();
                    theServer->mGameState = kServerStateOpen;
                    theServer->mSendInterval = INFINITE;
                    theServer->mChangeSendIntervalEvent->Set();
                }
            }
            break;
        case WAIT_OBJECT_0 + 3:
            theServer->mMainThreadExitEvent->Clear();
            if (theWaitState != 2)
                theExit = true;
            break;
        case WAIT_OBJECT_0 + 4:
            theServer->mInputAvailableEvent->Clear();
            theServer->ProcessPendingPlayerInputs();
            if (theWaitState == 1)
                theTimedOut = true;
            if (theWaitState == 2 && TMGetTime() > theDeadline)
                theServer->HandleReconnectWaitTimedOut();
            break;
        case WAIT_OBJECT_0 + 5:
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
        case WAIT_OBJECT_0 + 6:
            theServer->mWaitForReconnectsEvent->Clear();
            theTimeout = 1000;
            theDeadline = TMGetTime() + 20000;
            theWaitState = 2;
            break;
        case WAIT_OBJECT_0 + 7:
            theServer->mXferInputAvailableEvent->Clear();
            break;
        case WAIT_OBJECT_0 + 8:
            theServer->mXferAcceptEvent->Clear();
            theServer->mProtocol->AcceptForXfer(theServer->mServer, theConnection);
            break;
        case WAIT_OBJECT_0 + 9:
            {
                theServer->mResumeMigratedGame->Clear();
                theServer->mWaitForReconnectsTime = 0;
                unsigned long theNow = TMGetTime();
                if (theNow < theDeadline && theDeadline - theNow > 3500)
                    theDeadline = theNow + 3000;
            }
            break;
        case WAIT_OBJECT_0 + 10:
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
                if (theServer->mMessageManager->NumberOfOutstandingResponses(
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
                    theTimeout = INFINITE;
                    theWaitState = 0;
                } else if (TMGetTime() > theDeadline) {
                    if (++theRetries >= theServer->mSendToAllAndWaitRetries) {
                        theServer->BootAllLaggers(theServer->mSendToAllAndWaitMessageNumber);
                        theServer->mSendInterval = theServer->mDefaultSendInterval;
                        theServer->mChangeSendIntervalEvent->Set();
                        theTimeout = INFINITE;
                    } else {
                        theServer->mMessageManager->Resend(theServer->mSendToAllAndWaitMessageNumber);
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
    if (theSendInterval != INFINITE) {
        theTimeout = theSendInterval - 50;
        if (theTimeout < 0)
            theTimeout = 1;
    }
    unsigned long theLastTime = TMGetTime();
    theServer->mSendThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    HANDLE theEvents[3];
    theEvents[0] = theServer->mChangeSendIntervalEvent->mHandle;
    theEvents[1] = theServer->mSendThreadExitEvent->mHandle;
    theEvents[2] = theServer->mSendThreadForceCTEvent->mHandle;
    for (;;) {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        DWORD theWaitResult = WaitForMultipleObjects(3, theEvents, FALSE, theTimeout);
        unsigned long theNow = TMGetTime();
        long theDrift = theLastTime - theNow + theSendInterval;
        switch (theWaitResult) {
        case WAIT_OBJECT_0:
            theServer->mChangeSendIntervalEvent->Clear();
            theSendInterval = theServer->mSendInterval;
            if (theSendInterval == INFINITE)
                theTimeout = INFINITE;
            else {
                theTimeout = theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theNow;
            break;
        case WAIT_OBJECT_0 + 1:
            theServer->mSendThreadExitEvent->Clear();
            theServer->mSendThreadFinishedEvent->Set();
            return 0;
        case WAIT_OBJECT_0 + 2:
            theServer->mSendThreadForceCTEvent->Clear();
            theServer->SendCommunicationsTick(false, true);
            theServer->mMessageManager->mForceCTWhenAllResponded = false;
            if (theSendInterval == INFINITE)
                theTimeout = INFINITE;
            else {
                theTimeout = theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theNow;
            break;
        case WAIT_TIMEOUT:
            if (theServer->mCurrentCommunicationsTick >= theServer->mNextChecksumCT) {
                theServer->SendCommunicationsTick(true, true);
                theServer->mNextChecksumCT += theServer->mChecksumInterval;
            } else
                theServer->SendCommunicationsTick(false, true);
            theTimeout = INFINITE;
            if (theSendInterval != INFINITE) {
                theTimeout = theDrift + theSendInterval - 50;
                if (theTimeout < 0)
                    theTimeout = 1;
            }
            theLastTime = theDrift + theNow;
            break;
        case WAIT_FAILED:
            GetLastError();
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
    HANDLE theEvents[2];
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServerObject);
    ESVERIFY_AT(inServerObject != NULL, 2824);
    theTimeout = INFINITE;
    theServer->mXferThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theServer->mXferThreadExitEvent->mHandle;
    theEvents[1] = theServer->mXferStuffWaitingToGoEvent->mHandle;
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (WaitForMultipleObjects(2, theEvents, FALSE, theTimeout)) {
        case WAIT_OBJECT_0:
            theServer->mXferThreadExitEvent->Clear();
            theExit = true;
            break;
        case WAIT_OBJECT_0 + 1:
            theServer->mXferStuffWaitingToGoEvent->Clear();
            theXfersWaiting = true;
            theTimeout = 500;
            break;
        case WAIT_TIMEOUT:
            if (theXfersWaiting) {
                theXfersWaiting = theServer->mMessageManager->ProcessXfers();
                if (!theXfersWaiting)
                    theTimeout = INFINITE;
            }
            Sleep(1);
            break;
        case WAIT_FAILED:
            GetLastError();
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
