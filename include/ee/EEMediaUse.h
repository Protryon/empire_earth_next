// The database objects' media use count (port): EeDataMediaUse (mMediaCount, mIsLoading) is the classes' base, the
// pattern is Rust's (rust/crates/ee_data media.rs): the first user loads outside the unit's lock, later users wait,
// the last one unloads under it. These adapters hand it the unit's lock and C++ lambdas as its steps.
// Requires lle/TSCriticalSection.h.
#ifndef EE_EEMEDIAUSE_H
#define EE_EEMEDIAUSE_H

#include <type_traits>

#include "platform/PlatTime.h"
#include "rust/data/media.h"

// A lambda as a step: its status (a void lambda: 0; a bool one: 1 when true)
template <class F> EeDataMediaStep EEMediaStep(F& inStep)
{
    return {&inStep, [](void* inContext) -> long {
        F& theStep = *static_cast<F*>(inContext);
        if constexpr (std::is_void_v<decltype(theStep())>) {
            theStep();
            return 0;
        } else
            return (long)theStep();
    }};
}

inline EeDataMediaLock EEMediaLock(TSCriticalSection* inLock)
{
    return {inLock, [](void* inLock) { static_cast<TSCriticalSection*>(inLock)->Enter(); },
            [](void* inLock) { static_cast<TSCriticalSection*>(inLock)->Leave(); }, [] { PlatSleep(1); }};
}

// Counts a use; the first loads (inLoad, outside the lock: its status is returned), then runs inAfter under the lock;
// with inResetOnFailure a failed load leaves the count at 0
template <class L, class A>
long EEMediaAcquire(EeDataMediaUse* ioUse, TSCriticalSection* inLock, L inLoad, A inAfter, bool inResetOnFailure = false)
{
    EeDataMediaLock theLock = EEMediaLock(inLock);
    EeDataMediaStep theLoad = EEMediaStep(inLoad);
    EeDataMediaStep theAfter = EEMediaStep(inAfter);
    return ee_data_media_acquire(ioUse, &theLock, &theLoad, &theAfter, inResetOnFailure);
}

template <class L> long EEMediaAcquire(EeDataMediaUse* ioUse, TSCriticalSection* inLock, L inLoad)
{
    return EEMediaAcquire(ioUse, inLock, inLoad, [] {});
}

// Drops a use once any load is done; the last one runs inUnload under the lock (a bool step may keep the use)
template <class U> void EEMediaRelease(EeDataMediaUse* ioUse, TSCriticalSection* inLock, U inUnload)
{
    auto theUnload = [&]() -> bool {
        if constexpr (std::is_void_v<decltype(inUnload())>) {
            inUnload();
            return true;
        } else
            return inUnload();
    };
    EeDataMediaLock theLock = EEMediaLock(inLock);
    EeDataMediaStep theStep = EEMediaStep(theUnload);
    ee_data_media_release(ioUse, &theLock, &theStep);
}

// Runs inStep under the lock once any load is done
template <class F> void EEMediaLocked(EeDataMediaUse* ioUse, TSCriticalSection* inLock, F inStep)
{
    EeDataMediaLock theLock = EEMediaLock(inLock);
    EeDataMediaStep theStep = EEMediaStep(inStep);
    ee_data_media_locked(ioUse, &theLock, &theStep);
}

#endif
