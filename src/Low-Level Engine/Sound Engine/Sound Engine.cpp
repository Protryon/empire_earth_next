// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp (file name by analogy with Error System.cpp / Thread System.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "GEWindow.h"
#include "lle/SEMixer.h"
#include "rust/ee_sound.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Initialises the sound engine: registers its shutdown and loads the mixer plug-ins. The original's CD audio
// (SECD, MCI; SEInitializeCD) is gone: the game never asked for it (inUseCD was always false)
long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow)
{
    SEMixerStart theStart = {&inDirectory, inWindow, NULL};
    return ee_sound_initialize(reinterpret_cast<EeSoundMixerStart*>(&theStart));
}

// Creates a mixer for every valid sound plug-in of the plug-in table; kErrorNotFound when none loads (the choice:
// rust/crates/ee_audio/src/sound mixers.rs)
long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow)
{
    SEMixerStart theStart = {&inDirectory, inWindow, NULL};
    return ee_sound_mixers_initialize(reinterpret_cast<EeSoundMixerStart*>(&theStart));
}
