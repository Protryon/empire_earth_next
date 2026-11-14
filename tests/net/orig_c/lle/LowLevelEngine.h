// ne_client_test stand-in for the engine headers NESynchronousClient.cpp sees: the error codes, ESVERIFY_AT (a world
// line), the world's clock and floating-point control, strings, events (WaitAny), threads and critical sections.
#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include "c_world.h"
#define LLE_API
enum { kErrorOK = 0, keseOK = 0, kErrorAlreadyExists = 2, kErrorNotOpen = 4, kErrorTimeout = 0x11 };
#define ESVERIFY_AT(inCondition, inLine) do { if (!(inCondition)) gQ->Line("verify %d", (int)(inLine)); } while (0)
inline unsigned long TMGetTime() { return gQ->mClock; }
const unsigned long kFPPrecision53 = 0x10000;
inline void UMSetFloatingPointPrecision(unsigned long inPrecision)
{
    gQ->mPrecision = inPrecision;
    gQ->Line("fpu precision %lx", inPrecision);
}
inline unsigned long UMGetFloatingPointPrecision() { return gQ->mPrecision; }
inline void UMSetFloatingPointRoundingMode(unsigned long inMode)
{
    gQ->mRounding = inMode;
    gQ->Line("fpu rounding %lx", inMode);
}
inline unsigned long UMGetFloatingPointRoundingMode() { return gQ->mRounding; }
inline unsigned long PlatGetLastError() { return 0; }
#define PLAT_PRIORITY_TIME_CRITICAL 15

class UString {
public:
    UString(const char* inText = "") : mText(inText) {}
    std::string mText;
};
class UWideString {
public:
    UWideString(const char* inText = "") : mText(inText) {}
    std::string mText;
};

const unsigned long kTSWaitInfinite = kQInfinite;
enum TSWaitResult { kTSWaitTimeout = -1, kTSWaitFailed = -2 };
class TSEvent {
public:
    explicit TSEvent(int inID) : mID(inID) {}
    long Set() { gQ->Set(this->mID); return 0; }
    long Clear() { gQ->Clear(this->mID); return 0; }
    long Wait(unsigned long inTimeout) { return gQ->Wait(this->mID, inTimeout); }
    static long WaitAny(TSEvent* const* inEvents, unsigned long inCount, unsigned long inTimeout)
    {
        std::vector<int> theIDs;
        for (unsigned long i = 0; i < inCount; i++)
            theIDs.push_back(inEvents[i]->mID);
        return gQ->WaitAny(theIDs, inTimeout);
    }
    int mID;
};
class TSThread {
public:
    explicit TSThread(int inID) : mID(inID) {}
    void SetName(const UString& inName) { gQ->Line("thread name t%d %s", this->mID, inName.mText.c_str()); }
    long SetPriority(long inPriority)
    {
        gQ->Line("thread priority t%d %ld", this->mID, inPriority);
        return 0;
    }
    int mID;
};
// the world's threads run one at a time, so a lock has nothing to do (the Rust's locks are its own)
class TSCriticalSection {
public:
    void Enter() {}
    void Leave() {}
};
struct TSScheduler {
    static long CreateCriticalSection(TSCriticalSection*& outSection) { outSection = new TSCriticalSection; return 0; }
    static void DestroyCriticalSection(TSCriticalSection* inSection) { delete inSection; }
    static long CreateEvent(TSEvent*& outEvent) { outEvent = new TSEvent(gQ->NewEvent()); return 0; }
    static void DestroyEvent(TSEvent* inEvent)
    {
        if (inEvent == NULL)
            return;
        gQ->FreeEvent(inEvent->mID);
        delete inEvent;
    }
    static long CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                             unsigned long inStack = 0, bool inSuspended = false)
    {
        outThread = new TSThread(gQ->NewThread([inFunction, inParameter] { return inFunction(inParameter); }));
        return 0;
    }
    static long DestroyThread(TSThread* inThread)
    {
        if (inThread == NULL)
            return 0;
        gQ->FreeThread(inThread->mID);
        delete inThread;
        return 0;
    }
};
