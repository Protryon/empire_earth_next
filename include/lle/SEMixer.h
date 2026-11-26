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
    SEConductor& GetConductor() { return *reinterpret_cast<SEConductor*>(ee_sound_mixer_conductor(Rust())); }
    static unsigned long GetCount() { return ee_sound_mixer_count(); }
    const UString& GetName() { return SEMixerPlugInName(); }
    EeSoundMixer* Rust() const { return reinterpret_cast<EeSoundMixer*>(const_cast<SEMixer*>(this)); }
};

// port: the audio settings, in the mixer plug-in (Mixer.cpp; EEApplyAudioSettings): the master volume (0..1) over
// effects and music, the mute while the game is in the background, the output device by name (empty: the system's
// default; taken when the mixer opens) and the names of the devices there are (-> their count)
// (SPIMSetMasterVolume, SPIMSetMuted, SPIMSetOutputDevice, SPIMGetOutputDevices: rust/audio/mixer.h)

#endif
