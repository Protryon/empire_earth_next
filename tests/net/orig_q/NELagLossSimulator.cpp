// C:\EEX\Low-Level Engine\Network Engine\NELagLossSimulator.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// First Network Engine unit in link order: it emits the exported inline members of NEPlayer, NEMessage and every
// message class, NETimestampedMessage, NELagLossSimulator and TSThread (0x3bc2d..0x3d84c; markers at the end).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/URandom.h"
#include "lle/TSEvent.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"
#include "lle/NELagLossSimulator.h"
#include "lle/TSThread.h"

// Scale of the loss roll: a draw in [1, 100) is compared with the loss percentage (0x100988ec, unit .rdata)
const float kLossPercentageScale = 99.0f;

// Release-build trace: compiled out, but its argument is still evaluated (the label of a dropped message)
inline void NELagLossTrace(const char* inLabel) {}

// Creates a network lag/loss simulator around receive and send callbacks, with lag and loss off
NELagLossSimulator::NELagLossSimulator(void (*inReceiveCallbackFunction)(NEMessage*, void*),
                                       long (*inSendCallbackFunction)(NEMessage*, void*), void* inCallbackData)
{
    ESVERIFY_AT(inReceiveCallbackFunction != NULL && inSendCallbackFunction != NULL, 40);

    this->mReceiveLossPercentage = 0.0f;
    this->mReceiveCallbackFunction = inReceiveCallbackFunction;
    this->mSendCallbackFunction = inSendCallbackFunction;
    this->mSendLossPercentage = 0.0f;
    this->mCallbackData = inCallbackData;
    this->mLagThread = NULL;
    this->mLagQueueChanged = NULL;
    this->mLagThreadExit = NULL;
    this->mLagThreadStarted = NULL;
    this->mLagThreadFinished = NULL;
    this->mLaggedMessageListLock = NULL;
    this->mLagMinimum = 0;
    this->mLagMaximum = 0;
    this->mLagEnabled = false;
    this->mReceiveLossEnabled = false;
    this->mSendLossEnabled = false;
    this->mFlushOnDisable = false;
    this->mRandom.mSeed = TMGetTime();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLaggedMessageListLock) == keseOK, 62);
}

// Destroys the simulator, stopping its lag thread first (lagged messages are not delivered)
NELagLossSimulator::~NELagLossSimulator()
{
    if (this->mLagEnabled)
        this->DisableLag(false);
    TSScheduler::DestroyCriticalSection(this->mLaggedMessageListLock);
}

// Turns on random receive lag between a minimum and maximum (ms) and starts the lag thread
void NELagLossSimulator::EnableLag(unsigned long inMinimum, unsigned long inMaximum)
{
    this->mLagMinimum = inMinimum;
    this->mLagMaximum = inMaximum;
    this->mLagEnabled = true;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mLagQueueChanged) == keseOK, 96);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mLagThreadExit) == keseOK, 97);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mLagThreadStarted) == keseOK, 98);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mLagThreadFinished) == keseOK, 99);
    TSScheduler::CreateThread(this->mLagThread, NELagLossSimulator::LagThreadFunction, this, 0, false);
    this->mLagThreadStarted->Wait(kTSWaitInfinite);
    this->mLagThread->SetName(UString("CommLagLoss"));
}

// Turns off receive lag: stops the lag thread (delivering the queued messages if inFlush) and frees its events
void NELagLossSimulator::DisableLag(bool inFlush)
{
    if (!this->mLagEnabled)
        return;
    this->mFlushOnDisable = inFlush;
    this->mLagThreadExit->Set();
    this->mLagQueueChanged->Set();
    this->mLagThreadFinished->Wait(kTSWaitInfinite);
    TSScheduler::DestroyEvent(this->mLagQueueChanged);
    TSScheduler::DestroyEvent(this->mLagThreadExit);
    TSScheduler::DestroyEvent(this->mLagThreadFinished);
    TSScheduler::DestroyEvent(this->mLagThreadStarted);
    this->mLagQueueChanged = NULL;
    this->mLagThreadExit = NULL;
    this->mLagThreadFinished = NULL;
    this->mLagThreadStarted = NULL;
    this->mLagEnabled = false;
    TSScheduler::DestroyThread(this->mLagThread);
    this->mLagThread = NULL;
}

// Lag thread: delivers each queued message when its time is due; on exit optionally delivers the rest at once
unsigned long NELagLossSimulator::LagThreadFunction(void* inArgument)
{
    NELagLossSimulator* theSimulator = static_cast<NELagLossSimulator*>(inArgument);
    unsigned long theTimeout = kTSWaitInfinite;
    unsigned long theTime;
    long theResult;
    NETimestampedMessage* theMessage;
    bool theExiting = false;
    bool theRunning = true;
    ESVERIFY_AT(inArgument != NULL, 175);
    theSimulator->mLagThreadStarted->Set();
    UMSetFloatingPointPrecision(kFPPrecision53);
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        theResult = theSimulator->mLagQueueChanged->Wait(theTimeout);
        if (theResult == kErrorOK) {
            theSimulator->mLagQueueChanged->Clear();
            if (theSimulator->mLagThreadExit->Wait(0) == kErrorOK) {
                theSimulator->mLagThreadExit->Clear();
                theExiting = true;
                theRunning = false;
            }
        } else if (theResult == kErrorTimeout) {
            theSimulator->mLaggedMessageListLock->Enter();
            if (theSimulator->mLaggedMessageList.size() > 0) {
                theMessage = theSimulator->mLaggedMessageList.front();
                theSimulator->mLaggedMessageList.erase(theSimulator->mLaggedMessageList.begin());
                if (theMessage->GetMessage()) {
                    theMessage->GetMessage()->Reference();
                    theSimulator->mReceiveCallbackFunction(theMessage->GetMessage(), theSimulator->mCallbackData);
                }
                delete theMessage;
            }
            theSimulator->mLaggedMessageListLock->Leave();
        } else
            theRunning = false;
        theSimulator->mLaggedMessageListLock->Enter();
        if (theSimulator->mLaggedMessageList.size() > 0) {
            theTimeout = theSimulator->mLaggedMessageList.front()->GetTimestamp();
            theTime = TMGetTime();
            if (theTimeout < theTime)
                theTimeout = 0;
            else
                theTimeout -= theTime;
        } else
            theTimeout = kTSWaitInfinite;
        theSimulator->mLaggedMessageListLock->Leave();
    } while (theRunning);
    if (theExiting && theSimulator->mFlushOnDisable) {
        theSimulator->mLaggedMessageListLock->Enter();
        while (theSimulator->mLaggedMessageList.size() > 0) {
            theMessage = theSimulator->mLaggedMessageList.front();
            theSimulator->mLaggedMessageList.erase(theSimulator->mLaggedMessageList.begin());
            if (theMessage->GetMessage()) {
                theMessage->GetMessage()->Reference();
                theSimulator->mReceiveCallbackFunction(theMessage->GetMessage(), theSimulator->mCallbackData);
            }
            delete theMessage;
        }
        theSimulator->mLaggedMessageListLock->Leave();
    }
    theSimulator->mLagThreadFinished->Set();
    return 0;
}

// Handles a received message: drops it by the receive loss chance, queues it with a random lag, or passes it on
long NELagLossSimulator::Process(NEMessage* inMessage)
{
    unsigned long theTime;
    float theLag;
    NETimestampedMessage* theLaggedMessage;
    std::vector<NETimestampedMessage*>::iterator i;
    if (inMessage == NULL)
        return kErrorOK;
    if (this->mReceiveLossEnabled && this->mRandom.Generate() * kLossPercentageScale + 1.0f < this->mReceiveLossPercentage) {
        NELagLossTrace(inMessage->GetMessageLabel());
        inMessage->Dereference();
    } else if (this->mLagEnabled) {
        if (this->mLagMinimum == this->mLagMaximum)
            theTime = TMGetTime() + this->mLagMaximum;
        else {
            float theMinimum = (float)this->mLagMinimum;
            float theMaximum = (float)this->mLagMaximum;
            theLag = this->mRandom.Generate(theMinimum, theMaximum);
            theTime = TMGetTime() + (long)theLag;
        }
        theLaggedMessage = new NETimestampedMessage(inMessage, theTime);
        this->mLaggedMessageListLock->Enter();
        for (i = this->mLaggedMessageList.begin(); i != this->mLaggedMessageList.end(); i++)
            if (theLaggedMessage->GetTimestamp() < (*i)->GetTimestamp())
                break;
        this->mLaggedMessageList.insert(i, theLaggedMessage);
        this->mLaggedMessageListLock->Leave();
        this->mLagQueueChanged->Set();
    } else
        this->mReceiveCallbackFunction(inMessage, this->mCallbackData);
    return kErrorOK;
}

// Sends a message unless the send loss chance drops it; returns the send callback's result
long NELagLossSimulator::Send(NEMessage* inMessage)
{
    long theResult = kErrorOK;
    if (inMessage != NULL) {
        if (this->mSendLossEnabled && this->mRandom.Generate() * kLossPercentageScale + 1.0f < this->mSendLossPercentage) {
            NELagLossTrace(inMessage->GetMessageLabel());
            inMessage->Dereference();
        } else
            theResult = this->mSendCallbackFunction(inMessage, this->mCallbackData);
    }
    return theResult;
}
