// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.h (RECONSTRUCTION; SEMixer.cpp 0x4e8aa..0x4eef6). A loaded mixer
// plug-in (SPIM): its entry points, resolved by name when it is made, plus the conductor that plays scores through
// it. Whole in Rust (rust/crates/ee_audio/src/sound mixerobj.rs, the decisions mixers.rs; LLE lane round 6): an SEMixer* is an
// EeSoundMixer*, made by SEInitializeMixers and deleted by SEShutdown (ee_sound_shutdown); the registry and the active mixer are Rust's.
// The members are inline forwarders. Requires lle/LowLevelEngine.h.
#ifndef LLE_SEMIXER_H
#define LLE_SEMIXER_H

#include "lle/SEConductor.h"

struct IDirectSound;
class FSFileSpec;
class GEWindow;

long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow);   // Sound Engine.cpp; (provisional names)
// port: the start's state while SEInitializeMixers walks the plug-in table (rust/crates/ee_audio/src/sound mixerobj.rs
// EeSoundMixerStart has its layout)
struct SEMixerStart {
    const FSFileSpec* mDirectory;
    GEWindow* mWindow;
    void* mModule;                          // the plug-in being walked (Mixer.cpp's entry table)
};

// the mixer plug-in's name and validity test (PIMGetName, PIMIsValid; Mixer.cpp)
const UString& SEMixerPlugInName();
bool SEMixerPlugInIsValid();

class LLE_API SEMixer {
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
    bool IsValid() { return SEMixerPlugInIsValid(); }
    bool ProcessMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
    {
        return ee_sound_mixer_process_message(Rust(), inMessage, inParameter1, inParameter2);
    }
    // the given mixer active (NULL: none), the current one's music stopped and the mixer deactivated first
    static long UseMixer(SEMixer* inMixer) { return ee_sound_mixer_use(reinterpret_cast<EeSoundMixer*>(inMixer)); }
    // this port's addition: the mixer's output rate
    unsigned long GetOutputRate() const { return ee_sound_mixer_output_rate(Rust()); }
private:
    EeSoundMixer* Rust() const { return reinterpret_cast<EeSoundMixer*>(const_cast<SEMixer*>(this)); }
    SEMixer() = delete;
    ~SEMixer() = delete;
};

// port: the audio settings, in the mixer plug-in (Mixer.cpp; EEApplyAudioSettings): the master volume (0..1) over
// effects and music, the mute while the game is in the background, the output device by name (empty: the system's
// default; taken when the mixer opens) and the names of the devices there are (-> their count)
extern "C" void SPIMSetMasterVolume(float inVolume);
extern "C" void SPIMSetMuted(bool inMuted);
extern "C" void SPIMSetOutputDevice(const char* inName);
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);

#endif
