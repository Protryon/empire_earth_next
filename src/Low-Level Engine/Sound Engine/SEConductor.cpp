// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.cpp
// Music score playback on the music engine (rust/crates/ee_audio/src/dmusic: a DirectMusic performance on rustysynth,
// ported from dmusic), replacing the original's DirectMusic. Whole in Rust
// (rust/crates/ee_audio/src/sound conductorobj.rs: the engine's loader and performance, the scores, the music folder's files, the
// lock; the decisions conductor.rs; LLE lane round 6): every member is a forwarder.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEConductor.h"
#include "rust/audio/sound.h"

#define RUST reinterpret_cast<EeSoundConductor*>(const_cast<SEConductor*>(this))

unsigned long SEConductor::GetConductorState() const { return ee_sound_conductor_fields(RUST).state; }
float SEConductor::GetVolume() const { return ee_sound_conductor_fields(RUST).volume; }
SEScore* SEConductor::GetScore(long inID) const { return reinterpret_cast<SEScore*>(ee_sound_conductor_score(RUST, (int32_t)inID)); }
// a name already loaded only bumps its load count; while not running only the name is remembered
long SEConductor::LoadScore(const UWideString& inName, long& outID)
{
    int32_t theID = (int32_t)outID;
    long theResult = ee_sound_conductor_load_score(RUST, (const char*)inName, &theID);
    outID = theID;
    return theResult;
}
long SEConductor::Play(long inID) { return ee_sound_conductor_play(RUST, (int32_t)inID); }
// the engine has no segment queue: the score starts at the next measure (the game never queues)
long SEConductor::Queue(long inID) { return ee_sound_conductor_queue(RUST, (int32_t)inID); }
long SEConductor::SetChannelGroupCount(unsigned long inCount) { return ee_sound_conductor_set_channel_group_count(RUST, (uint32_t)inCount); }
long SEConductor::SetConductorState(unsigned long inState) { return ee_sound_conductor_set_state(RUST, (uint32_t)inState); }
long SEConductor::SetIntensity(unsigned long inIntensity, bool inImmediate) { return ee_sound_conductor_set_intensity(RUST, (uint32_t)inIntensity, inImmediate); }
// the folder without the name's last character (the trailing separator)
long SEConductor::SetMusicFolder(const UWideString& inFolder) { return ee_sound_conductor_set_music_folder(RUST, (const char*)inFolder); }
long SEConductor::SetReverb(unsigned long inReverb) { return ee_sound_conductor_set_reverb(RUST, (uint32_t)inReverb); }
// each song's reverb style (input gain and mix in dB, decay time in ms, high-frequency decay ratio), applied in the
// reverb's current state
long SEConductor::SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime) { return ee_sound_conductor_set_reverb_style(RUST, inGain, inHighFreqRTRatio, inMix, inTime); }
long SEConductor::SetVolume(float inVolume) { return ee_sound_conductor_set_volume(RUST, inVolume); }
long SEConductor::Stop() { return ee_sound_conductor_stop(RUST); }
long SEConductor::TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
{
    return ee_sound_conductor_transition_to(RUST, (int32_t)inID, (uint32_t)inCommand);
}
long SEConductor::UnloadScore(long inID) { return ee_sound_conductor_unload_score(RUST, (int32_t)inID); }
