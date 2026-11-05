// C:\EEX\Low-Level Engine\Thread System\TSEvent.h (RECONSTRUCTION; layout proven by TSEvent.cpp 0x53cee..0x53d8b,
// TSScheduler::CreateEvent operator new(8)).
#ifndef LLE_TSEVENT_H
#define LLE_TSEVENT_H

#include "platform/PlatThread.h"

// TSEvent::Wait / WaitAny timeouts are milliseconds; this one waits forever (Win32 INFINITE)
const unsigned long kTSWaitInfinite = PLAT_WAIT_INFINITE;
// TSEvent::WaitAny results besides the index of the event that was set
enum TSWaitResult {
    kTSWaitTimeout = PLAT_WAIT_TIMEOUT,
    kTSWaitFailed = PLAT_WAIT_FAILED,
};

class LLE_API TSEvent {
    friend class TSScheduler;
public:
    long Set();
    long Clear();
    long Wait(unsigned long inTimeout);
    // Waits until one of the events is set (at most PLAT_WAIT_MAX) -> its index (the lowest when several are),
    // kTSWaitTimeout or kTSWaitFailed (PlatGetLastError); the threads' message loops wait on several at once.
    static long WaitAny(TSEvent* const* inEvents, unsigned long inCount, unsigned long inTimeout);
    // exported inline members, ICF-folded: GetIsManuallyReset 0x3d783 (in NELagLossSimulator.cpp's inline block,
    // just before TSThread's), operator void* 0x10b8d
    bool GetIsManuallyReset() { return this->mManualReset; }
    operator void*() const { return this->mHandle; }

    PlatEvent* mHandle;                     // +0x00 (a Win32 event HANDLE in the original; TMTimer's event timers use it)
private:
    TSEvent(PlatEvent* inHandle, bool inManualReset);
    ~TSEvent();

    bool mManualReset;                      // +0x04
};

#endif
