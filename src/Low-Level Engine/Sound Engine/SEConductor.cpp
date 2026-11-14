// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.cpp
// Music score playback on dmusic (src/ThirdParty/dmusic), replacing the original's DirectMusic. Whole in Rust
// (rust/crates/ee_sound conductorobj.rs: dmusic's loader and performance, the scores, the music folder's files, the
// lock; the decisions conductor.rs; LLE lane round 6): every member is a forwarder.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEConductor.h"
#include "rust/ee_sound.h"

#define RUST reinterpret_cast<EeSoundConductor*>(const_cast<SEConductor*>(this))

unsigned long SEConductor::GetChannelGroupCount() { return ee_sound_conductor_fields(RUST).channel_group_count; }
unsigned long SEConductor::GetConductorState() const { return ee_sound_conductor_fields(RUST).state; }
unsigned long SEConductor::GetIntensity() const { return ee_sound_conductor_fields(RUST).intensity; }
const char* SEConductor::GetMusicFolder() const { return ee_sound_conductor_music_folder(RUST); }
unsigned long SEConductor::GetReverbState() const { return ee_sound_conductor_fields(RUST).reverb_state; }
float SEConductor::GetVolume() const { return ee_sound_conductor_fields(RUST).volume; }
void SEConductor::SetNotificationCallback(unsigned long inEvent, void (*inCallback)(SEConductor&, long))
{
    ee_sound_conductor_set_callback(RUST, (uint32_t)inEvent, reinterpret_cast<void*>(inCallback));
}
long SEConductor::GetCurrentlyPlayingScore() { return ee_sound_conductor_playing(RUST); }
SEScore* SEConductor::GetScore(long inID) const { return reinterpret_cast<SEScore*>(ee_sound_conductor_score(RUST, (int32_t)inID)); }
long SEConductor::GetScoreID(SEScore* inScore) const { return ee_sound_conductor_score_id(RUST, reinterpret_cast<EeSoundScore*>(inScore)); }
// a name already loaded only bumps its load count; while not running only the name is remembered
long SEConductor::LoadScore(const UWideString& inName, long& outID)
{
    int32_t theID = (int32_t)outID;
    long theResult = ee_sound_conductor_load_score(RUST, (const char*)inName, &theID);
    outID = theID;
    return theResult;
}
long SEConductor::Play(long inID) { return ee_sound_conductor_play(RUST, (int32_t)inID); }
// dmusic has no segment queue: the score starts at the next measure (the game never queues)
long SEConductor::Queue(long inID) { return ee_sound_conductor_queue(RUST, (int32_t)inID); }
long SEConductor::SetChannelGroupCount(unsigned long inCount) { return ee_sound_conductor_set_channel_group_count(RUST, (uint32_t)inCount); }
long SEConductor::SetConductorState(unsigned long inState) { return ee_sound_conductor_set_state(RUST, (uint32_t)inState); }
long SEConductor::SetIntensity(unsigned long inIntensity, bool inImmediate) { return ee_sound_conductor_set_intensity(RUST, (uint32_t)inIntensity, inImmediate); }
// the folder without the name's last character (the trailing separator)
long SEConductor::SetMusicFolder(const UWideString& inFolder) { return ee_sound_conductor_set_music_folder(RUST, (const char*)inFolder); }
long SEConductor::SetReverb(unsigned long inReverb) { return ee_sound_conductor_set_reverb(RUST, (uint32_t)inReverb); }
// the original set the DirectMusic port's Waves reverb; dmusic's synthesizer has none
long SEConductor::SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime) { return kErrorOK; }
long SEConductor::SetVolume(float inVolume) { return ee_sound_conductor_set_volume(RUST, inVolume); }
long SEConductor::Stop() { return ee_sound_conductor_stop(RUST); }
long SEConductor::TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
{
    return ee_sound_conductor_transition_to(RUST, (int32_t)inID, (uint32_t)inCommand);
}
long SEConductor::UnloadScore(long inID) { return ee_sound_conductor_unload_score(RUST, (int32_t)inID); }
