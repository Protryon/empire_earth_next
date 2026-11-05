// C:\EEX\Low-Level Engine\Thread System\Thread System.cpp (file name inferred: no __FILE__; by analogy with
// Error System\Error System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"

static void TSShutdown(bool inForced);

bool gThreadSystemInitialized;              // 0x12ded0 (provisional name)

// FUNCTION: LLE 0x10053675
// Starts the thread system: registers its shutdown and registers the calling (main) thread as a thread object
long TSInitialize()
{
    long theResult;
    PlatThread* theThread;
    gThreadSystemInitialized = true;
    if (URegisterShutdown(TSShutdown) == kErrorOK) {
        theThread = PlatThreadOpenCurrent();
        if (theThread != NULL)
            theResult = TSScheduler::CreateThread(theThread, PlatCurrentThreadId(), false);
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(PlatGetLastError());
            ESGetNativeError();
        }
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100536f0
// Shuts the thread system down by destroying the calling thread's object
static void TSShutdown(bool inForced)
{
    TSThread* theThread = TSScheduler::GetCurrentThread();
    if (theThread)
        TSScheduler::DestroyThread(theThread);
}
