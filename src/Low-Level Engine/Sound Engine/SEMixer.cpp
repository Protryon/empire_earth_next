// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// SEMixer whole in Rust (rust/crates/ee_audio/src/sound mixerobj.rs: the plug-in's entry points, the registry, the active
// mixer; the decisions mixers.rs; LLE lane round 6): every member is a forwarder. The plug-in's walk and entry
// lookup (ee_soundcpp_module_*) are the mixer plug-in's own (Mixer/Mixer.cpp), the one sound plug-in.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEMixer.h"
#include "rust/audio/sound.h"

#define RUST reinterpret_cast<EeSoundMixer*>(const_cast<SEMixer*>(this))

SEMixer* SEMixer::GetActiveMixer() { return reinterpret_cast<SEMixer*>(ee_sound_mixer_active()); }
SEConductor& SEMixer::GetConductor() { return *reinterpret_cast<SEConductor*>(ee_sound_mixer_conductor(RUST)); }
unsigned long SEMixer::GetCount() { return ee_sound_mixer_count(); }
// the registered mixer with the given index; kErrorBeyondEnd past the last
long SEMixer::GetMixer(unsigned long inIndex, SEMixer*& outMixer)
{
    return ee_sound_mixer_get((uint32_t)inIndex, reinterpret_cast<EeSoundMixer**>(&outMixer));
}
const UString& SEMixer::GetName() { return SEMixerPlugInName(); }
bool SEMixer::IsValid() { return SEMixerPlugInIsValid(); }
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
