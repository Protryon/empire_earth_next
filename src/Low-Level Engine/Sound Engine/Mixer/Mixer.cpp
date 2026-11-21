// Low-Level Engine\Sound Engine\Mixer\Mixer.cpp
// The sound mixer plug-in (' snd'). Whole in Rust (rust/crates/ee_audio/src/mixer: ffi.rs the SPIM entry points, the
// device choice, the loader and the entry table the engine looks names up in; plugin.rs the table's names; LLE round
// 8 moved the last C++ bodies there). This file keeps the plug-in's name (a UString) and the one-statement shims the
// loader calls on the C++ file objects (ee_mixercpp_*).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "rust/audio/mixer.h"
#include "lle/SEMixer.h"

static UString gMixerName("Mixer");

const UString& SEMixerPlugInName() { return gMixerName; }
bool SEMixerPlugInIsValid() { return ee_mixer_plugin_is_valid(); }

extern "C" {
void* ee_mixercpp_memory_file(FSFileSpec* inSpec) { return dynamic_cast<FSMemoryFile*>(inSpec); }
long ee_mixercpp_memory_length(FSMemoryFile* inFile, unsigned long* outLength) { return inFile->GetLength(*outLength); }
const unsigned char* ee_mixercpp_memory_data(FSMemoryFile* inFile) { return inFile->GetData(); }
void ee_mixercpp_spec_path(FSFileSpec* inSpec, char* outPath, size_t inCapacity) { snprintf(outPath, inCapacity, "%s", (const char*)(inSpec->GetVolume() + inSpec->GetDirectory() + inSpec->GetFile())); }
const char* ee_mixercpp_spec_file(FSFileSpec* inSpec) { return inSpec->GetFile(); }
}
