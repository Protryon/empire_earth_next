// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.h (RECONSTRUCTION; SEMixer.cpp 0x4e8aa..0x4eef6). A loaded mixer
// plug-in (SPIM): its entry points, resolved by name when it is made, plus the conductor that plays scores through
// it. Whole in Rust (rust/crates/ee_sound mixerobj.rs, the decisions mixers.rs; LLE lane round 6): an SEMixer* is an
// EeSoundMixer*, made by SEInitializeMixers and deleted by SEShutdown (ee_sound_shutdown); the registry and the active mixer are Rust's.
// The members are defined in SEMixer.cpp. Requires lle/LowLevelEngine.h.
#ifndef LLE_SEMIXER_H
#define LLE_SEMIXER_H

#include "lle/SEConductor.h"
#include "lle/PIMModule.h"

struct IDirectSound;
class FSFileSpec;
class GEWindow;

long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow);   // Sound Engine.cpp; (provisional names)
// port: the start's state while SEInitializeMixers walks the plug-in table (rust/crates/ee_sound mixerobj.rs
// EeSoundMixerStart has its layout)
struct SEMixerStart {
    const FSFileSpec* mDirectory;
    GEWindow* mWindow;
    PIMModule* mModule;                     // the module being walked
};

class LLE_API SEMixer {
public:
    static SEMixer* GetActiveMixer();
    SEConductor& GetConductor();
    static unsigned long GetCount();
    float GetCPUUsage();
    IDirectSound* GetDirectSoundInterface();
    static long GetMixer(unsigned long inIndex, SEMixer*& outMixer);
    const UString& GetName();
    bool IsValid();
    bool ProcessMessage(unsigned long inMessage, void* inParameter1, void* inParameter2);
    static long UseMixer(SEMixer* inMixer);
    // this port's additions: the mixer's output rate, and the music (the conductor) and movie sound sources its
    // device thread mixes in (interleaved stereo float frames added at the output rate)
    unsigned long GetOutputRate() const;
    void SetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext);
    void SetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext);
private:
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
