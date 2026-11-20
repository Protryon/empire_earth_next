// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp (file name by analogy with Error System.cpp / Thread System.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "GEWindow.h"
#include "lle/SEMixer.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "rust/audio/sound.h"

// The sound classes (SESoundManager, SESample, SESMInstanceNode, SEMixer, SEScore, SEConductor) are inline forwarders
// over rust/crates/ee_audio/src/sound; this unit keeps the start-up and the C++ the Rust objects call back.

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

// ---- the C++ the Rust objects reach (rust/crates/ee_audio/src/sound sys.rs)
extern "C" {
void ee_soundcpp_verify(const char* inExpression, uint32_t inLine) { ESVerify(__FILE__, inLine, inExpression); }
void ee_soundcpp_fp_precision53() { UMSetFloatingPointPrecision(kFPPrecision53); }
void ee_soundcpp_fp_check()
{
    if (UMGetFloatingPointPrecision() != kFPPrecision53)
        UMSetFloatingPointPrecision(kFPPrecision53);
    if (UMGetFloatingPointRoundingMode() != 0)
        UMSetFloatingPointRoundingMode(0);
}
void ee_soundcpp_path_reference(void* inPath) { static_cast<FSPath*>(inPath)->Reference(); }
void ee_soundcpp_path_dereference(void* inPath) { static_cast<FSPath*>(inPath)->Dereference(); }
// FindFile on the path (NULL: not found; a spec the path returned with an error leaks, as before)
void* ee_soundcpp_find_file(void* inPath, const char* inName)
{
    FSFileSpec* theFileSpec = NULL;
    if (static_cast<FSPath*>(inPath)->FindFile(UString(inName), theFileSpec) != kErrorOK)
        return NULL;
    return theFileSpec;
}
void ee_soundcpp_file_delete(void* inFile) { delete static_cast<FSFileSpec*>(inFile); }
}
