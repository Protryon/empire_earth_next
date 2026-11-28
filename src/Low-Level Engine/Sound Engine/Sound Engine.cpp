// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp and Mixer\Mixer.cpp (the mixer plug-in's unit, built with
// the engine's flags since LLE/UI round 3): the Sound Engine's one unit. Unit flags (docs/CALIBRATION.md "lle"):
// -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS. Each original unit is a section, headed by its path and keeping its comment.
#include <windows.h>
#include <stdio.h>   // (used directly: kept for the POSIX build)

#include "lle/LowLevelEngine.h"
#include "lle/SEMixer.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

// ======== Sound Engine/Sound Engine.cpp ========
// C:\EEX\Low-Level Engine\Sound Engine\Sound Engine.cpp (file name by analogy with Error System.cpp / Thread System.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS

// The sound classes (SESoundManager, SESample, SESMInstanceNode, SEMixer, SEScore, SEConductor) are inline forwarders
// over rust/crates/ee_audio/src/sound; this unit keeps the start-up and the C++ the Rust objects call back.

// Initialises the sound engine: registers its shutdown and loads the mixer plug-ins. The original's CD audio
// (SECD, MCI; SEInitializeCD) is gone: the game never asked for it (inUseCD was always false)
// (the start's state, the directory, the window and the plug-in being walked, is Rust's EeSoundMixerStart; a mixer is
// made for every valid sound plug-in of the plug-in table, kErrorNotFound when none loads: ee_audio sound mixers.rs)
long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow) { return ee_sound_initialize(&inDirectory, inWindow); }

// ---- the C++ the Rust objects reach (rust/crates/ee_audio/src/sound sys.rs: ESVerify and the floating-point control
// are ee_base's, called there)
extern "C" {
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

// ======== Sound Engine/Mixer/Mixer.cpp ========
// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in (' snd'). Whole in Rust (rust/crates/ee_audio/src/mixer: ffi.rs the SPIM entry points, the
// device choice, the loader and the entry table the engine looks names up in; plugin.rs the table's names; LLE round
// 8 moved the last C++ bodies there). This file keeps the plug-in's name (a UString) and the one-statement shims the
// loader calls on the C++ file objects (ee_mixercpp_*).

static UString gMixerName("Mixer");

const UString& SEMixerPlugInName() { return gMixerName; }

extern "C" {
void* ee_mixercpp_memory_file(FSFileSpec* inSpec) { return dynamic_cast<FSMemoryFile*>(inSpec); }
long ee_mixercpp_memory_length(FSMemoryFile* inFile, unsigned long* outLength) { return inFile->GetLength(*outLength); }
const unsigned char* ee_mixercpp_memory_data(FSMemoryFile* inFile) { return inFile->GetData(); }
void ee_mixercpp_spec_path(FSFileSpec* inSpec, char* outPath, size_t inCapacity) { snprintf(outPath, inCapacity, "%s", (const char*)(inSpec->GetVolume() + inSpec->GetDirectory() + inSpec->GetFile())); }
const char* ee_mixercpp_spec_file(FSFileSpec* inSpec) { return inSpec->GetFile(); }
}
