// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.h (RECONSTRUCTION). Music score playback, on the music
// engine (a DirectMusic performance in Rust: segments, styles, bands, played on rustysynth from SoundFonts converted
// from the DLS banks; rust/crates/ee_audio/src/dmusic, ported from dmusic) instead of Microsoft's DirectMusic. The
// performance renders into the mixer's output (SEMixer::SetMusicSource). Every SEMixer has one (SEMixer::GetConductor).
// Whole in Rust (rust/crates/ee_audio/src/sound conductorobj.rs, the decisions conductor.rs; LLE lane round 6): an
// SEConductor* is an EeSoundConductor* inside its mixer; the members are inline forwarders (SEConductor.cpp is gone).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SECONDUCTOR_H
#define LLE_SECONDUCTOR_H
#include "rust/base/lle_enums.h"

#include "rust/audio/sound.h"

#include "lle/LLEForward.h"

class LLE_API SEConductor : URustView {
public:
    unsigned long GetConductorState() const { return ee_sound_conductor_fields(Rust()).state; }
    float GetVolume() const { return ee_sound_conductor_fields(Rust()).volume; }

    SEScore* GetScore(long inID) const { return reinterpret_cast<SEScore*>(ee_sound_conductor_score(Rust(), (int32_t)inID)); }
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
    long SetChannelGroupCount(unsigned long inCount) { return ee_sound_conductor_set_channel_group_count(Rust(), (uint32_t)inCount); }
    long SetConductorState(unsigned long inState) { return ee_sound_conductor_set_state(Rust(), (uint32_t)inState); }
    long SetIntensity(unsigned long inIntensity, bool inImmediate) { return ee_sound_conductor_set_intensity(Rust(), (uint32_t)inIntensity, inImmediate); }
    // the folder without the name's last character (the trailing separator)
    long SetMusicFolder(const UWideString& inFolder) { return ee_sound_conductor_set_music_folder(Rust(), (const char*)inFolder); }
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

#endif
