// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Opens with NESynchronousClient's inline block (0x45c00..0x45d1c), the implicit copy ctor / operator= and the
// deleting dtor. Receive writes the xfer status send in both cases that need it (VC6 cross-jumps the copies; a
// flag-guarded single copy allocates differently).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"
#include "lle/NELagLossSimulator.h"
#include "lle/FSFile.h"
#include "lle/NESynchronousClient.h"

// FUNCTION: LLE 0x1004607d
// Creates the game client on protocol 0 with its thread events, locks, lag/loss simulator and pass-through recorder
NESynchronousClient::NESynchronousClient()
{
    ESVERIFY_AT(NEProtocol::GetCount() > 0, 38);
    long theResult = NEProtocol::GetProtocol(0, this->mProtocol);
    ESVERIFY_AT(theResult == keseOK, 40);
    this->mMySendToSlot = kSlotNone;
    this->mCurrentCommunicationsTick = 0;
    this->mCurrentPhysicsTick = 0;
    this->mConnection = NULL;
    this->mXferConnection = NULL;
    this->mLastUserInputTime = 0;
    this->mUserInputsThisSecond = 0;
    this->mLastPlaybackTick = 0;
    this->mLastCommunicationsTickResponse = NULL;
    this->mMaxNumConnectionTrys = 1;
    this->mNumConnectionTrys = 0;

    this->mStartConnectMessage = NULL;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mIncomingMessageAvailableEvent) == keseOK, 57);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadExitEvent) == keseOK, 58);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadStartedEvent) == keseOK, 59);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadFinishedEvent) == keseOK, 60);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadStartConnectEvent) == keseOK, 61);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMainThreadEndConnectEvent) == keseOK, 62);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mIncomingPacketAvailableEvent) == keseOK, 63);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mXferIncomingAvailableEvent) == keseOK, 64);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mReceiveThreadCanGoEvent) == keseOK, 65);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mReceiveThreadExitEvent) == keseOK, 66);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mReceiveThreadStartedEvent) == keseOK, 67);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mReceiveThreadFinishedEvent) == keseOK, 68);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mIncomingMessagesLock) == keseOK, 71);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mOutgoingMessagesLock) == keseOK, 72);

    this->mLagLossSimulator = new NELagLossSimulator(NESynchronousClient::LagReceiveLossCallback, NESynchronousClient::LagSendLossCallback, this);
    ESVERIFY_AT(this->mLagLossSimulator != NULL, 76);
    this->mLagLossSimulator->DisableLag(false);
    this->mLagLossSimulator->DisableReceiveLoss();
    this->mLagLossSimulator->DisableSendLoss();

    this->mRecordPlayback = new NERecordPlayback(NESynchronousClient::RecordPlaybackCallback, this);
    ESVERIFY_AT(this->mRecordPlayback != NULL, 83);
    this->mRecordPlayback->PassMode();
    this->mReceiveThreadCanGoEvent->Set();
}

// FUNCTION: LLE 0x10046401
// Destroys the client: releases the last tick response, stops game search and frees threads, events and locks
NESynchronousClient::~NESynchronousClient()
{
    if (this->mLastCommunicationsTickResponse != NULL)
        this->mLastCommunicationsTickResponse->Dereference();
    this->SearchForGame(false);
    TSScheduler::DestroyThread(this->mReceiveThread);
    TSScheduler::DestroyThread(this->mMainThread);
    delete this->mRecordPlayback;
    delete this->mLagLossSimulator;
    TSScheduler::DestroyEvent(this->mIncomingMessageAvailableEvent);
    TSScheduler::DestroyEvent(this->mMainThreadExitEvent);
    TSScheduler::DestroyEvent(this->mMainThreadStartedEvent);
    TSScheduler::DestroyEvent(this->mMainThreadFinishedEvent);
    TSScheduler::DestroyEvent(this->mMainThreadStartConnectEvent);
    TSScheduler::DestroyEvent(this->mMainThreadEndConnectEvent);
    TSScheduler::DestroyEvent(this->mIncomingPacketAvailableEvent);
    TSScheduler::DestroyEvent(this->mXferIncomingAvailableEvent);
    TSScheduler::DestroyEvent(this->mReceiveThreadCanGoEvent);
    TSScheduler::DestroyEvent(this->mReceiveThreadExitEvent);
    TSScheduler::DestroyEvent(this->mReceiveThreadStartedEvent);
    TSScheduler::DestroyEvent(this->mReceiveThreadFinishedEvent);
    TSScheduler::DestroyCriticalSection(this->mIncomingMessagesLock);
    TSScheduler::DestroyCriticalSection(this->mOutgoingMessagesLock);
}

// FUNCTION: LLE 0x10046513
// Connects to the game server at an address on port 2 with the client's connect timeout
long NESynchronousClient::Connect(const UString& inName, const UString& inHost, const UWideString& inAddress)
{
    return this->mProtocol->Connect(inName, inHost, inAddress, 2, this->mIncomingPacketAvailableEvent, this->mConnection,
                              this->GetConnectTimeout());
}

// FUNCTION: LLE 0x10046540
// Opens the file-transfer connection when connected; always returns not-open, whatever the outcome
long NESynchronousClient::ConnectForXfer()
{
    if (this->mConnection != NULL)
        this->mProtocol->ConnectForXfer((const NEConnection*&)this->mConnection, 1, this->mXferIncomingAvailableEvent, this->mXferConnection);
    return kErrorNotOpen;
}

// FUNCTION: LLE 0x10046562
// Closes the game and file-transfer connections; returns the close result, or already-exists if not connected
long NESynchronousClient::Disconnect()
{
    long theResult = kErrorAlreadyExists;
    if (this->mConnection != NULL) {
        theResult = this->mConnection->Close();
        this->mConnection = NULL;
    }
    this->DisconnectFromXfer();
    return theResult;
}

// FUNCTION: LLE 0x1004658b
// Closes the file-transfer connection; returns the close result, or already-exists if there was none
long NESynchronousClient::DisconnectFromXfer()
{
    long theResult = kErrorAlreadyExists;
    if (this->mXferConnection != NULL) {
        theResult = this->mXferConnection->Close();
        this->mXferConnection = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x100465a7
// Returns the name, host and address of a game found on the network
long NESynchronousClient::GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost,
                                          UWideString& outAddress)
{
    return this->mProtocol->GetDetectedGame(inIndex, outName, outHost, outAddress);
}

// FUNCTION: LLE 0x100465c3
// Lag simulator callback for received messages: passes them on to the recorder
void NESynchronousClient::LagReceiveLossCallback(NEMessage* inMessage, void* inThisPointer)
{
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inThisPointer);
    ESVERIFY_AT(inThisPointer != NULL, 259);
    theClient->mRecordPlayback->Process(inMessage);
}

// FUNCTION: LLE 0x100465f4
// Lag simulator callback for sent messages: sends them on the connection unless a recording is playing back
long NESynchronousClient::LagSendLossCallback(NEMessage* inMessage, void* inThisPointer)
{
    unsigned char* thePayload = NULL;
    unsigned long thePayloadSize;
    long theResult;
    long theSendResult = kErrorOK;
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inThisPointer);
    ESVERIFY_AT(inThisPointer != NULL, 284);
    if (theClient->mRecordPlayback->GetMode() == kRecordModePlaying)
        theResult = kErrorOK;
    else {
        inMessage->ExportMessage(&thePayload, thePayloadSize);
        ESVERIFY_AT(thePayload != NULL && thePayloadSize > 0, 296);
        if (theClient->mConnection != NULL)
            theSendResult = theClient->mConnection->Send(thePayload, thePayloadSize);
        delete thePayload;
        theResult = theSendResult;
    }
    inMessage->Dereference();
    return theResult;
}

// FUNCTION: LLE 0x10046690
// Client main thread: hands incoming messages to the lag simulator, and retries or fails the connect request
// on timeout (5 s between retries, 10 s after the start)
unsigned long NESynchronousClient::Main(void* inClientObject)
{
    unsigned long theTimeout = kTSWaitInfinite;
    TSEvent* theEvents[4];
    NEMessage* theMessage;
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inClientObject);
    ESVERIFY_AT(inClientObject != NULL, 341);
    theClient->mMainThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theClient->mIncomingMessageAvailableEvent;
    theEvents[1] = theClient->mMainThreadExitEvent;
    theEvents[2] = theClient->mMainThreadStartConnectEvent;
    theEvents[3] = theClient->mMainThreadEndConnectEvent;
    for (;;) {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        switch (TSEvent::WaitAny(theEvents, 4, theTimeout)) {
        case 0:
            theClient->mIncomingMessageAvailableEvent->Clear();
            theClient->mIncomingMessagesLock->Enter();
            while (theClient->mIncomingMessages.size() > 0) {
                theMessage = theClient->mIncomingMessages.front();
                theClient->mIncomingMessages.pop_front();
                if (theClient->mConnection == NULL)
                    theMessage->Dereference();
                else
                    theClient->mLagLossSimulator->Process(theMessage);
            }
            theClient->mIncomingMessagesLock->Leave();
            theClient->mReceiveThreadCanGoEvent->Set();
            break;
        case 1:
            theClient->mMainThreadExitEvent->Clear();
            theClient->mMainThreadFinishedEvent->Set();
            return 0;
        case 2:
            theClient->mMainThreadStartConnectEvent->Clear();
            theTimeout = 10000;
            break;
        case 3:
            theClient->mMainThreadEndConnectEvent->Clear();
            theClient->SetConnectionState(2);
            theTimeout = kTSWaitInfinite;
            break;
        case kTSWaitTimeout:
            if (++theClient->mNumConnectionTrys < theClient->mMaxNumConnectionTrys) {
                theClient->mStartConnectMessage->Reference();
                theTimeout = 5000;
                theClient->ProcessLocalUserInput(theClient->mStartConnectMessage, kSlotNone);
            } else {
                theClient->mMaxNumConnectionTrys = 1;
                theClient->mNumConnectionTrys = 0;
                theClient->mStartConnectMessage->Dereference();
                theTimeout = kTSWaitInfinite;
                theClient->mStartConnectMessage = NULL;
                theClient->SetConnectionState(0);
            }
            break;
        case kTSWaitFailed:
            PlatGetLastError();
            break;
        }
    }
}

// FUNCTION: LLE 0x1004687f
// Sends a local player's input as guaranteed (queued for the next tick response)
long NESynchronousClient::ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    return this->ProcessUserInputMessage(inMessage, inPlayer, true);
}

// FUNCTION: LLE 0x10046891
// Sends a local player's input immediately
long NESynchronousClient::ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    return this->ProcessUserInputMessage(inMessage, inPlayer, false);
}

// FUNCTION: LLE 0x100468a3
// Throttles player input to 10 per second, then sends or queues it if allowed while paused or playing back
// (dropped inputs are released; returns 7 when dropped)
long NESynchronousClient::ProcessUserInputMessage(NEMessage* inMessage, unsigned char inPlayer, bool inGuaranteed)
{
    long theResult = 7;
    bool theAccepted = true;
    unsigned long theTime = TMGetTime();
    if (theTime - this->mLastUserInputTime > 1000) {
        this->mUserInputsThisSecond = 0;
        this->mLastUserInputTime = theTime;
    } else if (this->mUserInputsThisSecond < 10 && inMessage->GetCountsTowardsPlayerInputThrottle())
        this->mUserInputsThisSecond++;
    else {
        inMessage->GetMessageLabel();
        theAccepted = false;
    }
    if (theAccepted) {
        inMessage->SetForSpecificSlot(inPlayer);
        if ((!this->IsPlaybackRecordingSession() || inMessage->GetType() == kNetMessageClientReconnectionRequest) &&
            (!this->mIsGamePaused || !this->mIsMultiplayerSession || inMessage->GetCanSendWhilePaused() == true ||
             inMessage->GetType() == kNetMessageClientRequestPause || inMessage->GetType() == kNetMessageClientReconnectionRequest || inMessage->GetType() == kNetMessageClientDisconnectionNotification) &&
            inMessage->GetIsAvailableToSend() == true) {
            inMessage->SetOriginatingSlot(this->mMySendToSlot);
            this->HandleUserInput(inMessage);
            if (!inGuaranteed)
                theResult = this->SendAMessage(inMessage);
            else {
                this->mOutgoingMessagesLock->Enter();
                this->mOutgoingMessages.push_back(inMessage);
                this->mOutgoingMessagesLock->Leave();
                theResult = kErrorOK;
            }
        } else
            inMessage->Dereference();
    } else
        inMessage->Dereference();
    return theResult;
}

// FUNCTION: LLE 0x100469b6
// Answers a server tick: runs it and replies with the queued inputs, or repeats the reply for an old tick
void NESynchronousClient::ProcessCommunicationsTickMessage(NEMCommunicationsTick* inMessage)
{
    NEMCommunicationsTickResponse* theResponse;
    unsigned long theTick = inMessage->GetCurrentCommunicationsTick();
    if (this->mLastTickReceived != -2) {
        if (theTick <= (unsigned long)this->mLastTickReceived && this->mLastTickReceived != -1) {
            theResponse = static_cast<NEMCommunicationsTickResponse*>(this->mLastCommunicationsTickResponse);
            if (theResponse == NULL || theTick != theResponse->GetServersCommunicationTick())
                theResponse = new NEMCommunicationsTickResponse(this->mNumberOfCTsProcessedInPhysics, theTick,
                                                                this->GetAveragePhysicsUpdateTime());
            else
                theResponse->Reference();
        } else {
            this->mCurrentCommunicationsTick++;
            this->mLastTickReceived = theTick;
            this->HandleMessage(inMessage);
            theResponse = new NEMCommunicationsTickResponse(this->mNumberOfCTsProcessedInPhysics, theTick,
                                                            this->GetAveragePhysicsUpdateTime());
            this->mOutgoingMessagesLock->Enter();
            if (!this->IsPlaybackRecordingSession())
                while (this->mOutgoingMessages.size() > 0) {
                    if (theResponse->AddPlayerInput(this->mOutgoingMessages.front()) != kErrorOK)
                        break;
                    this->mOutgoingMessages.pop_front();
                }
            this->mOutgoingMessagesLock->Leave();
            if (this->mLastCommunicationsTickResponse != NULL)
                this->mLastCommunicationsTickResponse->Dereference();
            this->mLastCommunicationsTickResponse = theResponse;
            theResponse->Reference();
        }
        theResponse->SetOriginatingSlot(this->mMySendToSlot);
        this->SendAMessage(theResponse);
    }
    inMessage->Dereference();
}

// FUNCTION: LLE 0x10046b08
// Handles a server message: start/stop game, ticks, connect/reconnect replies (send slot), pause and unpause
void NESynchronousClient::ProcessMessage(NEMessage* inMessage)
{
    switch (inMessage->GetType()) {
    case kNetMessageStopGame:
        this->HandleStopGame();
        this->SendAMessage(new NEMStopGameResponse());
        break;
    case kNetMessageStartGame:
        {
            unsigned long theTime = this->HandleStartGame(static_cast<NEMStartGame*>(inMessage)->GetOffsetStartTime());
            this->SendAMessage(new NEMStartGameResponse(theTime));
            this->HandleMessage(inMessage);
        }
        break;
    case kNetMessageCommunicationsTick:
        this->ProcessCommunicationsTickMessage(static_cast<NEMCommunicationsTick*>(inMessage));
        break;
    case kNetMessageClientConnectionRequestResponse:
        if (static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetSendToSlot() != kSlotNone) {
            this->mMySendToSlot = static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetSendToSlot();
            this->ConnectForXfer();
        }
        this->mLastTickReceived = -2;
        this->mMainThreadEndConnectEvent->Set();
        this->HandleMessage(inMessage);
        if (static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetError() == kConnectionAccepted)
            this->mLastTickReceived = static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetCurrentCT() - 1;
        break;
    case kNetMessageWorldDatabaseChecksum:
        this->HandleMessage(inMessage);
        break;
    case kNetMessageServerIsAlive:
        inMessage->Dereference();
        break;
    case kNetMessageClientReconnectionRequestResponse:
        if (static_cast<NEMClientReconnectionRequestResponse*>(inMessage)->GetNewSendToSlot() != kSlotNone)
            this->mMySendToSlot = static_cast<NEMClientReconnectionRequestResponse*>(inMessage)->GetNewSendToSlot();
        this->HandleMessage(inMessage);
        this->mLastTickReceived = static_cast<NEMClientReconnectionRequestResponse*>(inMessage)->GetCurrentCT() - 1;
        break;
    case kNetMessageUnPauseGame:
        {
            NEMessage* theResponse = new NEMUnPauseGameResponse(static_cast<NEMUnPauseGame*>(inMessage)->GetUniqueID());
            theResponse->SetOriginatingSlot(this->mMySendToSlot);
            this->SendAMessage(theResponse);
            this->SetIsGamePaused(false);
        }
        break;
    case kNetMessagePauseGame:
        {
            NEMessage* theResponse = new NEMPauseGameResponse(static_cast<NEMPauseGame*>(inMessage)->GetUniqueID());
            theResponse->SetOriginatingSlot(this->mMySendToSlot);
            this->SendAMessage(theResponse);
            this->SetIsGamePaused(true);
        }
        break;
    default:
        this->HandleMessage(inMessage);
        break;
    }
}

// FUNCTION: LLE 0x10046cdf
// Client receive thread: reads game packets and file transfers, reports a lost connection after 75 s silence
// (warning every 10 s before)
unsigned long NESynchronousClient::Receive(void* inClientObject)
{
    bool theExit = false;
    bool theTimedOut = false;
    unsigned char* theXferBuffer = NULL;
    unsigned long theXferReceived = 0;
    FSFile* theFile = NULL;
    unsigned long theLastTime = 0;
    unsigned long theElapsed = 0;
    unsigned long theTimeout = 10000;
    TSEvent* theEvents[3];
    unsigned long theLength;
    unsigned char theBuffer[0x240];
    NEMessage* theMessage;
    unsigned long theCount;
    uint32_t theXferID;     // 4 bytes on the wire
    unsigned long theReceived;
    uint32_t theSize;
    unsigned char* theData;
    long theResult;
    unsigned char theSlot;
    unsigned char thePercentDone = 100;
    unsigned char theStatusID = 3;
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inClientObject);
    ESVERIFY_AT(inClientObject != NULL, 900);
    theClient->mReceiveThreadStartedEvent->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    theEvents[0] = theClient->mIncomingPacketAvailableEvent;
    theEvents[1] = theClient->mReceiveThreadExitEvent;
    theEvents[2] = theClient->mXferIncomingAvailableEvent;
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        TMGetTime();
        switch (TSEvent::WaitAny(theEvents, 3, theTimeout)) {
        case 0:
            theClient->mIncomingPacketAvailableEvent->Clear();
            theClient->mReceiveThreadCanGoEvent->Wait(kTSWaitInfinite);
            theClient->mReceiveThreadCanGoEvent->Clear();
            theCount = 0;
            if (theClient->mConnection != NULL) {
                do {
                    theResult = theClient->mConnection->Receive(0x240, theBuffer, theLength);
                    if (theResult == kErrorOK) {
                        theMessage = NEMessage::ConvertMemoryToMessage(theBuffer);
                        if (theMessage != NULL) {
                            theClient->mIncomingMessagesLock->Enter();
                            theClient->mIncomingMessages.push_back(theMessage);
                            theClient->mIncomingMessagesLock->Leave();
                            theCount++;
                        }
                    }
                } while (theResult == kErrorOK);
                if (theCount > 0) {
                    theClient->mIncomingMessageAvailableEvent->Set();
                    theTimeout = 10000;
                    theLastTime = TMGetTime();
                    theElapsed = 0;
                    break;
                }
            }
            if (TMGetTime() - theLastTime < 10000)
                theTimeout = 10000 - (TMGetTime() - theLastTime);
            else
                theTimedOut = true;
            theClient->mReceiveThreadCanGoEvent->Set();
            break;
        case 1:
            theTimeout = 10000;
            theElapsed = 0;
            theClient->mReceiveThreadExitEvent->Clear();
            theExit = true;
            break;
        case 2:
            theTimeout = 10000;
            theElapsed = 0;
            theClient->mXferIncomingAvailableEvent->Clear();
            if (theXferReceived == 0)
                theXferBuffer = new unsigned char[NEMXferMessage::StaticExportSize()];
            theClient->XferReceive(NEMXferMessage::StaticExportSize() - theXferReceived,
                                   theXferBuffer + theXferReceived, theReceived);

            theXferReceived += theReceived;
            if (theXferReceived > NEMXferMessage::StaticExportSize()) {
                ESVERIFY_AT("NESynchronousClient::Receive() - too much xfer data" == NULL, 1038);
            } else if (theXferReceived == NEMXferMessage::StaticExportSize()) {
                NEMXferMessage* theXferMessage = new NEMXferMessage(theXferBuffer);
                delete [] theXferBuffer;
                theXferReceived = 0;
                switch (theXferMessage->GetID()) {
                case 0:
                    theData = theXferMessage->GetData();
                    memcpy(&theXferID, theData, 4);
                    theData += 4;
                    memcpy(&theSize, theData, 4);
                    theData += 4;
                    theFile = theClient->CreateXferFile(UWideString(reinterpret_cast<char*>(theData)), theSize);
                    if (theFile == NULL) {
                        theSlot = theClient->GetClientSlot();
                        theClient->ProcessLocalUserInput(
                            new NEMXferStatus(theSlot, thePercentDone, theXferID, theStatusID), kSlotServer);
                    }
                    break;
                case 1:
                    if (theFile != NULL)
                        theFile->Write(theXferMessage->GetData(), theXferMessage->GetDataSize(), theReceived);
                    else {
                        theXferID = 0xffff;
                        theSlot = theClient->GetClientSlot();
                        theClient->ProcessLocalUserInput(
                            new NEMXferStatus(theSlot, thePercentDone, theXferID, theStatusID), kSlotServer);
                    }
                    break;
                case 2:
                    if (theFile != NULL) {
                        theFile->Close();
                        delete theFile;
                        theFile = NULL;
                    }
                    break;
                case 3:
                    if (theFile != NULL) {
                        theFile->Close();
                        theFile->Delete();
                        delete theFile;
                        theFile = NULL;
                    }
                    break;
                }
                theXferMessage->Dereference();
            }
            break;
        case kTSWaitTimeout:
            theTimedOut = true;
            break;
        case kTSWaitFailed:
            theTimeout = 10000;
            theElapsed = 0;
            PlatGetLastError();
            break;
        }
        if (theTimedOut == true) {
            theElapsed += 10000;
            if (theElapsed >= 75000) {
                if (theClient->mConnection != NULL && theClient->mCurrentCommunicationsTick != 0 &&
                    !theClient->IgnoreConnectionTimeout()) {
                    TMGetTime();
                    theClient->ClearIncomingMessages();
                    theClient->HandleConnectionLost();
                }
                theElapsed = 0;
            } else
                theClient->HandleConnectionTimeoutWarning(75000 - theElapsed);
            theTimedOut = false;
            theTimeout = 10000;
            theLastTime = TMGetTime();
        }
    } while (!theExit);
    theClient->mReceiveThreadFinishedEvent->Set();
    return 0;
}

// FUNCTION: LLE 0x10047136
// Recorder callback: processes a message, skipping send-to-all-and-wait messages for ticks already seen
void NESynchronousClient::RecordPlaybackCallback(NEMessage* inMessage, void* inThisPointer)
{
    unsigned long theTick;
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inThisPointer);
    ESVERIFY_AT(inThisPointer != NULL, 1171);
    if (!inMessage->GetSendToAllAndWaitMessageID(theTick))
        theClient->ProcessMessage(inMessage);
    else if (theTick > theClient->mLastPlaybackTick) {
        theClient->mLastPlaybackTick = theTick;
        theClient->ProcessMessage(inMessage);
    } else
        inMessage->Dereference();
}

// FUNCTION: LLE 0x10047195
// Starts or stops searching the network for "EEService" games
long NESynchronousClient::SearchForGame(bool inStart)
{
    UString theName("EEService");
    UString theHost("Anything");
    return this->mProtocol->SearchForGame(theHost, theName, inStart);
}

// FUNCTION: LLE 0x100471e5
// Sends a message through the lag/loss simulator
long NESynchronousClient::SendAMessage(NEMessage* inMessage)
{
    return this->mLagLossSimulator->Send(inMessage);
}

// FUNCTION: LLE 0x100471f7
// Starts the client's main ("CommMain") and receive ("CommRecv") threads at time-critical priority
void NESynchronousClient::StartThreads()
{
    ESVERIFY_AT(TSScheduler::CreateThread(this->mMainThread, NESynchronousClient::Main, this) == keseOK, 1245);
    ESVERIFY_AT(TSScheduler::CreateThread(this->mReceiveThread, NESynchronousClient::Receive, this) == keseOK, 1246);
    this->mMainThread->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
    this->mReceiveThread->SetPriority(PLAT_PRIORITY_TIME_CRITICAL);
    this->mMainThreadStartedEvent->Wait(kTSWaitInfinite);
    this->mReceiveThreadStartedEvent->Wait(kTSWaitInfinite);
    this->mMainThread->SetName(UString("CommMain"));
    this->mReceiveThread->SetName(UString("CommRecv"));
}

// FUNCTION: LLE 0x100472cf
// Stops the receive and main threads and waits for both to finish
void NESynchronousClient::StopThreads()
{
    this->mReceiveThreadExitEvent->Set();
    this->mReceiveThreadFinishedEvent->Wait(kTSWaitInfinite);
    this->mMainThreadExitEvent->Set();
    this->mMainThreadFinishedEvent->Wait(kTSWaitInfinite);
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10040223
// SYNTHETIC: LLE 0x10045c00
// SYNTHETIC: LLE 0x10045c55
// SYNTHETIC: LLE 0x10045c59
// SYNTHETIC: LLE 0x10045c5f
// SYNTHETIC: LLE 0x10045c66
// SYNTHETIC: LLE 0x10045c6d
// SYNTHETIC: LLE 0x10045c9f
// SYNTHETIC: LLE 0x10045cc4
// SYNTHETIC: LLE 0x10045ccf
// SYNTHETIC: LLE 0x10045cdc
// SYNTHETIC: LLE 0x10045ceb
// SYNTHETIC: LLE 0x10045cf8
// SYNTHETIC: LLE 0x10045d05
// SYNTHETIC: LLE 0x10045d1c
// SYNTHETIC: LLE 0x10045e9f
// SYNTHETIC: LLE 0x1004601c
