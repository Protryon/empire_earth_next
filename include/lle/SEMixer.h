// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.h (RECONSTRUCTION; SEMixer.cpp 0x4e8aa..0x4eef6). A loaded mixer
// plug-in (SPIM): its entry points, resolved by name when it is made, plus the conductor that plays scores through
// it. Whole in Rust (rust/crates/ee_audio/src/sound mixerobj.rs, the decisions mixers.rs; LLE lane round 6): an SEMixer* is an
// EeSoundMixer*, made by SEInitializeMixers and deleted by SEShutdown (ee_sound_shutdown); the registry and the active mixer are Rust's.
// The members are inline forwarders. Requires lle/LowLevelEngine.h.
#ifndef LLE_SEMIXER_H
#define LLE_SEMIXER_H

#include "rust/base/lle_enums.h"
#include "rust/audio/sound.h"
#include "rust/audio/mixer.h"

struct IDirectSound;
#include "lle/LLEForward.h"

// SEConductor (SEConductor.h, RECONSTRUCTION, merged here in LLE/UI round 2). Music score playback, on the music
// engine (a DirectMusic performance in Rust: segments, styles, bands, played on rustysynth from SoundFonts converted
// from the DLS banks; rust/crates/ee_audio/src/dmusic, ported from dmusic) instead of Microsoft's DirectMusic. The
// performance renders into the mixer's output (SEMixer::SetMusicSource). Every SEMixer has one (SEMixer::GetConductor).
// Whole in Rust (rust/crates/ee_audio/src/sound conductorobj.rs, the decisions conductor.rs; LLE lane round 6): an
// SEConductor* is an EeSoundConductor* inside its mixer; the members are inline forwarders (SEConductor.cpp is gone).
class LLE_API SEConductor : URustView {
public:
    unsigned long GetConductorState() const { return ee_sound_conductor_fields(Rust()).state; }
    float GetVolume() const { return ee_sound_conductor_fields(Rust()).volume; }

    SEScore* GetScore(long inID) const
    {
        return reinterpret_cast<SEScore*>(ee_sound_conductor_score(Rust(), (int32_t)inID));
    }
    // a name already loaded only bumps its load count; while not running only the name is remembered
    long LoadScore(const UWideString& inName, long& outID)
    {
        int32_t theID = (int32_t)outID;
        long theResult = ee_sound_conductor_load_score(Rust(), (const char*)inName, &theID);
        outID = theID;
        return theResult;
    }
    long Play(long inID) { return ee_sound_conductor_play(Rust(), (int32_t)inID); }
    // the engine has no segment queue: the score starts at the next measure (the game never queues)
    long Queue(long inID) { return ee_sound_conductor_queue(Rust(), (int32_t)inID); }
    long SetChannelGroupCount(unsigned long inCount)
    {
        return ee_sound_conductor_set_channel_group_count(Rust(), (uint32_t)inCount);
    }
    long SetConductorState(unsigned long inState) { return ee_sound_conductor_set_state(Rust(), (uint32_t)inState); }
    long SetIntensity(unsigned long inIntensity, bool inImmediate)
    {
        return ee_sound_conductor_set_intensity(Rust(), (uint32_t)inIntensity, inImmediate);
    }
    // the folder without the name's last character (the trailing separator)
    long SetMusicFolder(const UWideString& inFolder)
    {
        return ee_sound_conductor_set_music_folder(Rust(), (const char*)inFolder);
    }
    long SetReverb(unsigned long inReverb) { return ee_sound_conductor_set_reverb(Rust(), (uint32_t)inReverb); }
    // each song's reverb style (input gain and mix in dB, decay time in ms, high-frequency decay ratio), applied in the
    // reverb's current state
    long SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime)
    {
        return ee_sound_conductor_set_reverb_style(Rust(), inGain, inHighFreqRTRatio, inMix, inTime);
    }
    long SetVolume(float inVolume) { return ee_sound_conductor_set_volume(Rust(), inVolume); }
    long Stop() { return ee_sound_conductor_stop(Rust()); }
    long TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
    {
        return ee_sound_conductor_transition_to(Rust(), (int32_t)inID, (uint32_t)inCommand);
    }
    long UnloadScore(long inID) { return ee_sound_conductor_unload_score(Rust(), (int32_t)inID); }
    const EeSoundConductor* Rust() const { return reinterpret_cast<const EeSoundConductor*>(this); }
};

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
