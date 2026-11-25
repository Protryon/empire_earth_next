// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.h (RECONSTRUCTION; SEMixer.cpp 0x4e8aa..0x4eef6). A loaded mixer
// plug-in (SPIM): its entry points, resolved by name when it is made, plus the conductor that plays scores through
// it. Whole in Rust (rust/crates/ee_audio/src/sound mixerobj.rs, the decisions mixers.rs; LLE lane round 6): an SEMixer* is an
// EeSoundMixer*, made by SEInitializeMixers and deleted by SEShutdown (ee_sound_shutdown); the registry and the active mixer are Rust's.
// The members are inline forwarders. Requires lle/LowLevelEngine.h.
#ifndef LLE_SEMIXER_H
#define LLE_SEMIXER_H

#include "lle/SEConductor.h"
#include "rust/audio/mixer.h"

struct IDirectSound;
#include "lle/LLEForward.h"

// the mixer plug-in's name (PIMGetName; Mixer.cpp)
const UString& SEMixerPlugInName();

class LLE_API SEMixer : URustView {
public:
    static SEMixer* GetActiveMixer() { return reinterpret_cast<SEMixer*>(ee_sound_mixer_active()); }
    SEConductor& GetConductor() { return *reinterpret_cast<SEConductor*>(ee_sound_mixer_conductor(Rust())); }
    static unsigned long GetCount() { return ee_sound_mixer_count(); }
    // the registered mixer with the given index; kErrorBeyondEnd past the last
    static long GetMixer(unsigned long inIndex, SEMixer*& outMixer)
    {
        return ee_sound_mixer_get((uint32_t)inIndex, reinterpret_cast<EeSoundMixer**>(&outMixer));
    }
    const UString& GetName() { return SEMixerPlugInName(); }
    bool IsValid() { return ee_mixer_plugin_is_valid(); }   // (PIMIsValid)
    bool ProcessMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
    {
        return ee_sound_mixer_process_message(Rust(), inMessage, inParameter1, inParameter2);
    }
    // the given mixer active (NULL: none), the current one's music stopped and the mixer deactivated first
    static long UseMixer(SEMixer* inMixer) { return ee_sound_mixer_use(reinterpret_cast<EeSoundMixer*>(inMixer)); }
    // this port's addition: the mixer's output rate
    unsigned long GetOutputRate() const { return ee_sound_mixer_output_rate(Rust()); }
    EeSoundMixer* Rust() const { return reinterpret_cast<EeSoundMixer*>(const_cast<SEMixer*>(this)); }
};

// port: the audio settings, in the mixer plug-in (Mixer.cpp; EEApplyAudioSettings): the master volume (0..1) over
// effects and music, the mute while the game is in the background, the output device by name (empty: the system's
// default; taken when the mixer opens) and the names of the devices there are (-> their count)
// (SPIMSetMasterVolume, SPIMSetMuted, SPIMSetOutputDevice, SPIMGetOutputDevices: rust/audio/mixer.h)

#endif
