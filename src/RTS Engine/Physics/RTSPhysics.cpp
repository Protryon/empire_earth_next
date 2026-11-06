// C:\EEX\RTS Engine\Physics\RTSPhysics.cpp (__FILE__ 0x4c26c4). The simulation thread's engine side (RTTI
// RTSPhysics, vtable 0x442988): 0x16100f..0x161825 (the "RTSPhysics Data" key's init chain, the class in definition
// order, the deque<long>::pop_back COMDAT the ICF kept here; EEPlayer.cpp's key chains open the next unit at
// 0x161825). Unit flags: the exe's module set.
#include <windows.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/TMTimer.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "platform/PlatSim.h"

#include "ee/RTSGraphics.h"
#include "ee/EEPhysics.h"
#include "ee/EECommunications.h"
// chain at 0x56100f (wrapper)
//   object 0x931d90  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gRTSPhysicsDataKey("RTSPhysics Data");
// SYNTHETIC: EXE 0x56100f   (wrapper)
// SYNTHETIC: EXE 0x561019   (ctor)
// SYNTHETIC: EXE 0x56102a   (registrar)
// SYNTHETIC: EXE 0x561036   (dtor)

// FUNCTION: EXE 0x561041
// Creates the simulation with its events, a stopped timer and its thread; waits until the thread starts
RTSPhysics::RTSPhysics(RTSGraphics* inGraphics, TMTimer* inTimer, long inDefaultTicksPerSecond, long inThreadPriority)
    : mRandom(1)
{
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated) == keseOK, 49);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone) == keseOK, 50);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStartThreadMainLoop) == keseOK, 51);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mTimerTick,false) == keseOK, 52);
    this->mGraphics = inGraphics;
    this->mSpeed = 1.0f;
    this->mDefaultTicksPerSecond = inDefaultTicksPerSecond;
    this->mTicksPerSecond = 1;
    this->mTimerInterval = 1000;
    this->mTicksRun = 0;
    this->mTicksOwed = 0;
    this->mL28 = 0;
    this->mTimerRunning = false;
    this->mStopThread = false;
    this->mRandom.SetSeed(1);
    this->mTimer = inTimer;
    this->mTimer->Stop();
    this->mTimer->SetTimerType(1);
    this->mTimer->SetEvent(this->mTimerTick);
    this->mTimer->SetInterval(this->mTimerInterval, 0);
    this->mSlowPlayback = true;

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, RTSPhysics::Main, (void*)this) == keseOK, 81);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 82);
    this->mThreadCreated->Wait(kTSWaitInfinite);
    this->mThread->SetName(gPhysicsThreadName);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mServerMessageQueueLock) == keseOK, 91);
}

// SYNTHETIC: EXE 0x561204
// (the scalar deleting destructor, emitted with the vtable; slot RTSPhysics+0x10)

// FUNCTION: EXE 0x561220
// Stops the timer, destroys the thread and its events and releases every queued server message
RTSPhysics::~RTSPhysics()
{
    this->mTimer->Stop();
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyEvent(this->mStartThreadMainLoop);
    TSScheduler::DestroyEvent(this->mTimerTick);
    TSScheduler::DestroyThread(this->mThread);
    this->mServerMessageQueueLock->Enter();
    while (this->mServerMessages.size() > 0) {
        NEMessage* theMessage = this->mServerMessages.front();
        this->mServerMessages.pop_front();
        theMessage->Dereference();
    }
    this->mServerMessageQueueLock->Leave();
    TSScheduler::DestroyCriticalSection(this->mServerMessageQueueLock);
}

// ICF-FOLDED: EXE 0x4437ab
// Returns the simulation's next random number between 0 and 1
// (`add ecx, 0x18; jmp __imp_Generate`: its body was folded into an identical AI-range function)
float RTSPhysics::GetRandom()
{
    return this->mRandom.Generate();
}

// FUNCTION: EXE 0x5612b9
// Returns a simulation random number between the minimum and the maximum
float RTSPhysics::GetRandom(float inMinimum, float inMaximum)
{
    return this->mRandom.Generate() * (inMaximum - inMinimum) + inMinimum;
}

// FUNCTION: EXE 0x5612d3
// Returns a simulation random integer from the minimum to the maximum inclusive
long RTSPhysics::GetRandom(long inMinimum, long inMaximum)
{
    long theLimit = inMaximum + 1;
    long theResult = UMFloatToLong(this->mRandom.Generate() * (theLimit - inMinimum) + inMinimum);
    if (theResult == theLimit)
        theResult = inMaximum;
    return theResult;
}

// FUNCTION: EXE 0x56130d
// Reads the random seed, speed, tick counters and flags and resets the timer, restarting it only if it was
// already running (the saved running flag is read but dropped); returns the first read error
long RTSPhysics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    unsigned long theSeed;
    bool theTimerRunning;
    bool theStopThread;
    theResult = inFile.Read(theSeed);
    if (theResult == kErrorOK) {
        this->mRandom.SetSeed(theSeed);
        theResult = inFile.Read(this->mL28);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theTimerRunning);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mSpeed);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mTimerInterval);
                    if (theResult == kErrorOK) {
                        theResult = inFile.Read(this->mTicksPerSecond);
                        if (theResult == kErrorOK) {
                            theResult = inFile.Read(this->mTicksRun);
                            if (theResult == kErrorOK) {
                                theResult = inFile.Read(this->mDefaultTicksPerSecond);
                                if (theResult == kErrorOK) {
                                    theResult = inFile.Read(theStopThread);
                                    if (theResult == kErrorOK) {
                                        this->mStopThread = theStopThread;
                                        theResult = inFile.Read(this->mSlowPlayback);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    this->mTicksOwed = 0;
    this->mTimer->Stop();
    this->mTimer->SetInterval(this->mTimerInterval, 0);
    if (this->mTimerRunning)
        theResult = this->mTimer->Start();
    return theResult;
}

// FUNCTION: EXE 0x56140c
// Runs the simulation thread: handles its messages and, while the timer runs, ticks the game (at most twice
// per timer tick, limited by the communication ticks) and wakes the renderer; idles otherwise
unsigned long RTSPhysics::Main(void* inPointer)
{
    RTSPhysics* thePhysics = static_cast<RTSPhysics*>(inPointer);
    unsigned long theLastTime = 0;
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    long theResult;
    thePhysics->mThreadCreated->Set();
    thePhysics->mStartThreadMainLoop->Wait(kTSWaitInfinite);
    UMSetFloatingPointPrecision(kFPPrecision53);
    while (!thePhysics->mStopThread) {
        theResult = thePhysics->mThread->Receive(theMessage, theParameter1, theParameter2);
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        if (theResult == kErrorOK)
            thePhysics->HandleMessage(theMessage, theParameter1, theParameter2);
        if (thePhysics->mTimerRunning && !thePhysics->mStopThread && gPlatSimMode) {
            // TEST ONLY: the headless simulation (platform/PlatSim.h) runs the game as fast as it goes: the next
            // communications tick is sent at once (no timer, no send interval) and its physics ticks run back to back
            EEPhysics* theEEPhysics = dynamic_cast<EEPhysics*>(thePhysics);
            NESynchronousServer* theServer = dynamic_cast<NESynchronousServer*>(gCommunications);
            if (!gPlatSimRun || theEEPhysics == NULL || theServer == NULL) {
                PlatSleep(1);
                continue;
            }
            theServer->SimCommunicationsTick();
            thePhysics->Lock();
            do {
                thePhysics->Tick();
                thePhysics->mTicksRun++;
            } while (theEEPhysics->mTicksToProcess > 0 && thePhysics->mTimerRunning && !thePhysics->mStopThread);
            if (gPlatSimAfterTick)
                gPlatSimAfterTick();
            thePhysics->Unlock();
        } else if (thePhysics->mTimerRunning && !thePhysics->mStopThread) {
            unsigned long theTicks = (TMGetTime() - theLastTime) / thePhysics->mTimerInterval;
            if (theTicks > 1)
                thePhysics->mTicksOwed += theTicks - 1;
            do {
                theResult = thePhysics->mTimerTick->Wait(3000);
                theLastTime = TMGetTime();
                if (theResult == 0x11) {
                    thePhysics->mTimer->Stop();
                    thePhysics->mTimer->SetInterval(thePhysics->mTimerInterval, 0);
                    theResult = thePhysics->mTimer->Start();
                    ESDebugOut("!RTSPhysics::Main: the timer wasn't ticking. Performed a hard reset on the timer.\n");
                }
            } while (theResult == 0x11);
            thePhysics->Lock();
            thePhysics->mTicksOwed++;
            EEPhysics* theEEPhysics = dynamic_cast<EEPhysics*>(thePhysics);
            bool theBehind = gCommunications->GetNumberOfCTsProcessedInPhysics() + 1
                < gCommunications->GetCurrentCommunicationsTick();
            if (theBehind)
                thePhysics->mTicksOwed = 100000;
            if (!theBehind && theEEPhysics != NULL && theEEPhysics->mTicksToProcess < thePhysics->mTicksOwed) {
                thePhysics->mTicksOwed = theEEPhysics->mTicksToProcess;
                if (thePhysics->mTicksOwed == 0)
                    thePhysics->mTicksOwed = 1;
            }
            if (thePhysics->mTicksOwed > 0 && thePhysics->mTimerRunning) {
                thePhysics->Tick();
                thePhysics->mTicksRun++;
                thePhysics->mTicksOwed--;
                if (thePhysics->mTicksOwed > 0 && thePhysics->mTimerRunning) {
                    thePhysics->Tick();
                    thePhysics->mTicksRun++;
                    thePhysics->mTicksOwed--;
                }
            }
            thePhysics->mGraphics->mRenderNow->Set();
            thePhysics->Unlock();
        } else {
            thePhysics->Idle();
            thePhysics->mGraphics->mRenderNow->Set();
            PlatSleep(1);
        }
    }
    thePhysics->mThreadDone->Set();
    // no assert string in .data, no twin): a compiled-away debug check stood in by 
    return 0;
}

// FUNCTION: EXE 0x561609
// Handles a thread message: quit, stop the timer, stop it and reset the speed, or start the timer
void RTSPhysics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    switch (inMessage) {
    case kThreadMessageQuit:
        this->mStopThread = true;
        break;
    case 0x402:
        this->mTimerRunning = false;
        this->mTimer->Stop();
        this->mTimerTick->Clear();
        this->mTicksRun = 0;
        this->mTicksOwed = 0;
        break;
    case 0x403:
        this->mTimerRunning = false;
        this->mTimer->Stop();
        this->mTimerTick->Clear();
        this->SetSpeed(1.0f / this->mDefaultTicksPerSecond);
        this->mTicksRun = 0;
        this->mTicksOwed = 0;
        break;
    case 0x404:
        this->mTicksRun = 0;
        this->mTicksOwed = 0;
        this->mTimerTick->Clear();
        this->mTimer->Start();
        this->mTimerRunning = true;
        break;
    }
}

// FUNCTION: EXE 0x5616a4
// Writes the random seed, timer flags, speed and tick counters; returns the first write error
long RTSPhysics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult;
    bool theTimerRunning = this->mTimerRunning;
    bool theStopThread = this->mStopThread;
    unsigned long theSeed = this->mRandom.GetSeed();
    theResult = outFile.Write(theSeed);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mL28);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theTimerRunning);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mSpeed);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTimerInterval);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTicksPerSecond);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTicksRun);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mDefaultTicksPerSecond);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theStopThread);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSlowPlayback);
    return theResult;
}

// FUNCTION: EXE 0x561774
// Sets the seconds per tick, reprogramming the timer when it changes, then updates the timing
void RTSPhysics::SetSpeed(float inSpeed)
{
    if (inSpeed != this->mSpeed) {
        this->mSpeed = inSpeed;
        this->mTimerInterval = UMFloorUnsigned(inSpeed * 1000.0f);
        this->mTicksPerSecond = UMFloorUnsigned(1.0f / inSpeed);
        this->mTimer->Stop();
        this->mTimer->SetInterval(this->mTimerInterval, 0);
        if (this->mTimerRunning)
            this->mTimer->Start();
    }
    this->UpdateTiming();
}

// TEMPLATE: EXE 0x561812
// std::deque<long>::pop_back (the ICF representative of the deques' identical pop_back)
