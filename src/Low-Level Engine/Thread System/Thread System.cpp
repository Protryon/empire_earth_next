// C:\EEX\Low-Level Engine\Thread System\Thread System.cpp (file name inferred: no __FILE__; by analogy with
// Error System\Error System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_ts.h"

// FUNCTION: LLE 0x10053675
// Starts the thread system: registers its shutdown (ee_ts_shutdown: destroys the calling thread's object), then the
// calling (main) thread as a thread object (rust/crates/ee_ts)
long TSInitialize()
{
    if (URegisterShutdown(ee_ts_shutdown) != kErrorOK)
        return kErrorFailed;
    return ee_ts_initialize();
}
