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
#include "lle/NEPlayer.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"
#include "lle/NELagLossSimulator.h"
#include "lle/TSThread.h"

// Scale of the loss roll: a draw in [1, 100) is compared with the loss percentage (0x100988ec, unit .rdata)
const float kLossPercentageScale = 99.0f;

// Release-build trace: compiled out, but its argument is still evaluated (the label of a dropped message)
inline void NELagLossTrace(const char* inLabel) {}

// FUNCTION: LLE 0x1003d84c
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

// FUNCTION: LLE 0x1003d90c
// Destroys the simulator, stopping its lag thread first (lagged messages are not delivered)
NELagLossSimulator::~NELagLossSimulator()
{
    if (this->mLagEnabled)
        this->DisableLag(false);
    TSScheduler::DestroyCriticalSection(this->mLaggedMessageListLock);
}

// FUNCTION: LLE 0x1003d92e
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

// FUNCTION: LLE 0x1003da15
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

// FUNCTION: LLE 0x1003da83
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

// FUNCTION: LLE 0x1003dc58
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

// FUNCTION: LLE 0x1003dd87
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

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1003bc2d
// SYNTHETIC: LLE 0x1003bc65
// SYNTHETIC: LLE 0x1003bc69
// SYNTHETIC: LLE 0x1003bc73
// SYNTHETIC: LLE 0x1003bca1
// SYNTHETIC: LLE 0x1003bca6
// SYNTHETIC: LLE 0x1003bcaa
// SYNTHETIC: LLE 0x1003bcae
// SYNTHETIC: LLE 0x1003bcb2
// SYNTHETIC: LLE 0x1003bcbc
// SYNTHETIC: LLE 0x1003bcc6
// SYNTHETIC: LLE 0x1003bcf9
// SYNTHETIC: LLE 0x1003bd20
// SYNTHETIC: LLE 0x1003bd3e
// SYNTHETIC: LLE 0x1003bd7f
// SYNTHETIC: LLE 0x1003bdbe
// SYNTHETIC: LLE 0x1003bdd7
// SYNTHETIC: LLE 0x1003bddb
// SYNTHETIC: LLE 0x1003bde1
// SYNTHETIC: LLE 0x1003be0e
// SYNTHETIC: LLE 0x1003be35
// SYNTHETIC: LLE 0x1003beb0
// SYNTHETIC: LLE 0x1003bed0
// SYNTHETIC: LLE 0x1003beff
// SYNTHETIC: LLE 0x1003bf05
// SYNTHETIC: LLE 0x1003bf1d
// SYNTHETIC: LLE 0x1003bf2f
// SYNTHETIC: LLE 0x1003bf36
// SYNTHETIC: LLE 0x1003bf93
// SYNTHETIC: LLE 0x1003bfda
// SYNTHETIC: LLE 0x1003bffb
// SYNTHETIC: LLE 0x1003c001
// SYNTHETIC: LLE 0x1003c040
// SYNTHETIC: LLE 0x1003c079
// SYNTHETIC: LLE 0x1003c0d4
// SYNTHETIC: LLE 0x1003c132
// SYNTHETIC: LLE 0x1003c171
// SYNTHETIC: LLE 0x1003c18a
// SYNTHETIC: LLE 0x1003c190
// SYNTHETIC: LLE 0x1003c194
// SYNTHETIC: LLE 0x1003c1d3
// SYNTHETIC: LLE 0x1003c20c
// SYNTHETIC: LLE 0x1003c288
// SYNTHETIC: LLE 0x1003c2b1
// SYNTHETIC: LLE 0x1003c2e0
// SYNTHETIC: LLE 0x1003c2e6
// SYNTHETIC: LLE 0x1003c307
// SYNTHETIC: LLE 0x1003c322
// SYNTHETIC: LLE 0x1003c34b
// SYNTHETIC: LLE 0x1003c37a
// SYNTHETIC: LLE 0x1003c380
// SYNTHETIC: LLE 0x1003c3a1
// SYNTHETIC: LLE 0x1003c3d1
// SYNTHETIC: LLE 0x1003c400
// SYNTHETIC: LLE 0x1003c406
// SYNTHETIC: LLE 0x1003c42d
// SYNTHETIC: LLE 0x1003c464
// SYNTHETIC: LLE 0x1003c493
// SYNTHETIC: LLE 0x1003c499
// SYNTHETIC: LLE 0x1003c49d
// SYNTHETIC: LLE 0x1003c4ca
// SYNTHETIC: LLE 0x1003c4f1
// SYNTHETIC: LLE 0x1003c543
// SYNTHETIC: LLE 0x1003c563
// SYNTHETIC: LLE 0x1003c592
// SYNTHETIC: LLE 0x1003c598
// SYNTHETIC: LLE 0x1003c5b0
// SYNTHETIC: LLE 0x1003c5f1
// SYNTHETIC: LLE 0x1003c63a
// SYNTHETIC: LLE 0x1003c64d
// SYNTHETIC: LLE 0x1003c674
// SYNTHETIC: LLE 0x1003c695
// SYNTHETIC: LLE 0x1003c6a0
// SYNTHETIC: LLE 0x1003c6fb
// SYNTHETIC: LLE 0x1003c737
// SYNTHETIC: LLE 0x1003c75b
// SYNTHETIC: LLE 0x1003c75f
// SYNTHETIC: LLE 0x1003c763
// SYNTHETIC: LLE 0x1003c767
// SYNTHETIC: LLE 0x1003c76b
// SYNTHETIC: LLE 0x1003c76f
// SYNTHETIC: LLE 0x1003c775
// SYNTHETIC: LLE 0x1003c7c0
// SYNTHETIC: LLE 0x1003c7ff
// SYNTHETIC: LLE 0x1003c859
// SYNTHETIC: LLE 0x1003c886
// SYNTHETIC: LLE 0x1003c8aa
// SYNTHETIC: LLE 0x1003c8af
// SYNTHETIC: LLE 0x1003c8b3
// SYNTHETIC: LLE 0x1003c8b9
// SYNTHETIC: LLE 0x1003c8bd
// SYNTHETIC: LLE 0x1003c8fe
// SYNTHETIC: LLE 0x1003c933
// SYNTHETIC: LLE 0x1003c938
// SYNTHETIC: LLE 0x1003c992
// SYNTHETIC: LLE 0x1003c9bb
// SYNTHETIC: LLE 0x1003c9ea
// SYNTHETIC: LLE 0x1003c9f0
// SYNTHETIC: LLE 0x1003c9fe
// SYNTHETIC: LLE 0x1003ca25
// SYNTHETIC: LLE 0x1003ca46
// SYNTHETIC: LLE 0x1003ca6f
// SYNTHETIC: LLE 0x1003ca9e
// SYNTHETIC: LLE 0x1003caa4
// SYNTHETIC: LLE 0x1003cac5
// SYNTHETIC: LLE 0x1003cae0
// SYNTHETIC: LLE 0x1003cb33
// SYNTHETIC: LLE 0x1003cb83
// SYNTHETIC: LLE 0x1003cbb2
// SYNTHETIC: LLE 0x1003cbb6
// SYNTHETIC: LLE 0x1003cbbc
// SYNTHETIC: LLE 0x1003cbf5
// SYNTHETIC: LLE 0x1003cc28
// SYNTHETIC: LLE 0x1003cc51
// SYNTHETIC: LLE 0x1003cc80
// SYNTHETIC: LLE 0x1003cc86
// SYNTHETIC: LLE 0x1003cca7
// SYNTHETIC: LLE 0x1003ccd0
// SYNTHETIC: LLE 0x1003ccff
// SYNTHETIC: LLE 0x1003cd05
// SYNTHETIC: LLE 0x1003cd26
// SYNTHETIC: LLE 0x1003cd6c
// SYNTHETIC: LLE 0x1003cd9b
// SYNTHETIC: LLE 0x1003cd9f
// SYNTHETIC: LLE 0x1003cda3
// SYNTHETIC: LLE 0x1003cda7
// SYNTHETIC: LLE 0x1003cdab
// SYNTHETIC: LLE 0x1003cdb1
// SYNTHETIC: LLE 0x1003cde4
// SYNTHETIC: LLE 0x1003ce11
// SYNTHETIC: LLE 0x1003ce31
// SYNTHETIC: LLE 0x1003ce60
// SYNTHETIC: LLE 0x1003ce66
// SYNTHETIC: LLE 0x1003ce7e
// SYNTHETIC: LLE 0x1003ced0
// SYNTHETIC: LLE 0x1003cf01
// SYNTHETIC: LLE 0x1003cf30
// SYNTHETIC: LLE 0x1003cf36
// SYNTHETIC: LLE 0x1003cf57
// SYNTHETIC: LLE 0x1003cf88
// SYNTHETIC: LLE 0x1003cfb7
// SYNTHETIC: LLE 0x1003cfbd
// SYNTHETIC: LLE 0x1003cfde
// SYNTHETIC: LLE 0x1003cffe
// SYNTHETIC: LLE 0x1003d02d
// SYNTHETIC: LLE 0x1003d033
// SYNTHETIC: LLE 0x1003d04b
// SYNTHETIC: LLE 0x1003d06b
// SYNTHETIC: LLE 0x1003d09a
// SYNTHETIC: LLE 0x1003d0a0
// SYNTHETIC: LLE 0x1003d0b8
// SYNTHETIC: LLE 0x1003d0e1
// SYNTHETIC: LLE 0x1003d110
// SYNTHETIC: LLE 0x1003d116
// SYNTHETIC: LLE 0x1003d13d
// SYNTHETIC: LLE 0x1003d166
// SYNTHETIC: LLE 0x1003d195
// SYNTHETIC: LLE 0x1003d19b
// SYNTHETIC: LLE 0x1003d1bc
// SYNTHETIC: LLE 0x1003d1f4
// SYNTHETIC: LLE 0x1003d223
// SYNTHETIC: LLE 0x1003d229
// SYNTHETIC: LLE 0x1003d250
// SYNTHETIC: LLE 0x1003d271
// SYNTHETIC: LLE 0x1003d2ab
// SYNTHETIC: LLE 0x1003d2da
// SYNTHETIC: LLE 0x1003d2e0
// SYNTHETIC: LLE 0x1003d307
// SYNTHETIC: LLE 0x1003d359
// SYNTHETIC: LLE 0x1003d3c8
// SYNTHETIC: LLE 0x1003d3f7
// SYNTHETIC: LLE 0x1003d3fb
// SYNTHETIC: LLE 0x1003d402
// SYNTHETIC: LLE 0x1003d409
// SYNTHETIC: LLE 0x1003d40f
// SYNTHETIC: LLE 0x1003d453
// SYNTHETIC: LLE 0x1003d49f
// SYNTHETIC: LLE 0x1003d4f7
// SYNTHETIC: LLE 0x1003d543
// SYNTHETIC: LLE 0x1003d572
// SYNTHETIC: LLE 0x1003d576
// SYNTHETIC: LLE 0x1003d57a
// SYNTHETIC: LLE 0x1003d580
// SYNTHETIC: LLE 0x1003d584
// SYNTHETIC: LLE 0x1003d5b7
// SYNTHETIC: LLE 0x1003d5e4
// SYNTHETIC: LLE 0x1003d5f8
// SYNTHETIC: LLE 0x1003d5ff
// SYNTHETIC: LLE 0x1003d604
// SYNTHETIC: LLE 0x1003d609
// SYNTHETIC: LLE 0x1003d617
// SYNTHETIC: LLE 0x1003d625
// SYNTHETIC: LLE 0x1003d63a
// SYNTHETIC: LLE 0x1003d63e
// SYNTHETIC: LLE 0x1003d642
// SYNTHETIC: LLE 0x1003d653
// SYNTHETIC: LLE 0x1003d65d
// SYNTHETIC: LLE 0x1003d667
// SYNTHETIC: LLE 0x1003d6f0
// SYNTHETIC: LLE 0x1003d783
// SYNTHETIC: LLE 0x1003d787
// SYNTHETIC: LLE 0x1003d78b
// SYNTHETIC: LLE 0x1003d78f
// SYNTHETIC: LLE 0x1003d7a1
// SYNTHETIC: LLE 0x1003d7a8
// SYNTHETIC: LLE 0x1003d7af
// SYNTHETIC: LLE 0x1003d7ba
// SYNTHETIC: LLE 0x1003d803
// SYNTHETIC: LLE 0x1003ddda
// SYNTHETIC: LLE 0x1003de2a
// SYNTHETIC: LLE 0x1003de66
// SYNTHETIC: LLE 0x1003df0a
// SYNTHETIC: LLE 0x1003dfb3
// SYNTHETIC: LLE 0x1003dfdb
