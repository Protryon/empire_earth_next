// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// SEMixer whole in Rust (rust/crates/ee_sound mixerobj.rs: the plug-in's entry points, the registry, the active
// mixer; the decisions mixers.rs; LLE lane round 6): every member is a forwarder. This file keeps the plug-in
// module's calls (extern "C" ee_soundcpp_module_*, the start's walk over the plug-in table).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEMixer.h"
#include "rust/ee_sound.h"

#define RUST reinterpret_cast<EeSoundMixer*>(const_cast<SEMixer*>(this))

SEMixer* SEMixer::GetActiveMixer() { return reinterpret_cast<SEMixer*>(ee_sound_mixer_active()); }
SEConductor& SEMixer::GetConductor() { return *reinterpret_cast<SEConductor*>(ee_sound_mixer_conductor(RUST)); }
unsigned long SEMixer::GetCount() { return ee_sound_mixer_count(); }
// the registered mixer with the given index; kErrorBeyondEnd past the last
long SEMixer::GetMixer(unsigned long inIndex, SEMixer*& outMixer)
{
    return ee_sound_mixer_get((uint32_t)inIndex, reinterpret_cast<EeSoundMixer**>(&outMixer));
}
const UString& SEMixer::GetName() { return static_cast<PIMModule*>(ee_sound_mixer_module(RUST))->GetName(); }
bool SEMixer::IsValid() { return static_cast<PIMModule*>(ee_sound_mixer_module(RUST))->IsValid(); }
bool SEMixer::ProcessMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    return ee_sound_mixer_process_message(RUST, inMessage, inParameter1, inParameter2);
}
// the given mixer active (NULL: none), the current one's music stopped and the mixer deactivated first
long SEMixer::UseMixer(SEMixer* inMixer) { return ee_sound_mixer_use(reinterpret_cast<EeSoundMixer*>(inMixer)); }
unsigned long SEMixer::GetOutputRate() const { return ee_sound_mixer_output_rate(RUST); }
void SEMixer::SetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    ee_sound_mixer_set_music_source(RUST, inSource, inContext);
}
void SEMixer::SetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
{
    ee_sound_mixer_set_movie_source(RUST, inSource, inContext);
}

// ---- the plug-in module as the Rust mixers reach it (rust/crates/ee_sound sys.rs)
extern "C" {
void* ee_soundcpp_module_entry(void* inModule, const char* inName)
{
    return static_cast<PIMModule*>(inModule)->GetEntry(UString(inName));
}
bool ee_soundcpp_module_is_valid(void* inModule) { return static_cast<PIMModule*>(inModule)->IsValid(); }
void ee_soundcpp_module_unload(void* inModule) { static_cast<PIMModule*>(inModule)->Unload(); }
int32_t ee_soundcpp_first_module(void* inStart)
{
    SEMixerStart* theStart = static_cast<SEMixerStart*>(inStart);
    return (int32_t)PIMModule::GetFirstModule(*theStart->mDirectory, ' snd', theStart->mModule);
}
int32_t ee_soundcpp_next_module(void* inStart)
{
    return (int32_t)PIMModule::GetNextModule(static_cast<SEMixerStart*>(inStart)->mModule);
}
}
