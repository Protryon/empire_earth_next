// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp (file name by analogy with Error System.cpp / Thread System.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "GEWindow.h"
#include "lle/SEMixer.h"
#include "rust/ee_sound.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// FUNCTION: LLE 0x100532d9
// Initialises the sound engine: registers its shutdown and loads the mixer plug-ins. The original's CD audio
// (SECD, MCI; SEInitializeCD) is gone: the game never asked for it (inUseCD was always false)
long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow)
{
    URegisterShutdown(SEShutdown);
    return SEInitializeMixers(inDirectory, inWindow);
}

// FUNCTION: LLE 0x1005354b
// Creates a mixer for every valid sound plug-in of the plug-in table; kErrorNotFound when none loads (the choice:
// rust/crates/ee_sound mixers.rs)
long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow)
{
    SEMixerStart theStart = {&inDirectory, inWindow, NULL};
    return ee_mixers_initialize(&theStart);
}

// FUNCTION: LLE 0x100535e2
// Deletes every mixer (the first one until none is left: ee_sound mixers.rs)
void SEShutdown(bool inForced)
{
    ee_mixers_delete_all();
}
