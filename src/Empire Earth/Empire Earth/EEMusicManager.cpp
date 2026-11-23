// C:\EEX\Empire Earth\Empire Earth\EEMusicManager.cpp (__FILE__ 0x4bfd1c). The music thread: 0x131b7c..0x13242b (the
// two fade globals and the thread name first, then the class in definition order; the two fades are file statics,
// each after its first user). Unit flags: the exe's module set. Data: the fade constants 0x441fb0/0x441fb4, the vtable
// 0x441fbc, the song delay constants 0x441fc0/0x441fc4.
// The decisions (the thread's step, start, pause, stop, the song changes, the fades, the intensity rules, the time
// to the next song) are Rust: rust/crates/ee_audio/src/sound music.rs (docs/RUST_PLAN.md R3.6); the thread, its events,
// the wait and the members' reads are ee_game gamecpp music_io.rs (game core lane round 11). This file keeps the
// constructor's and destructor's forwarders and the trampolines ee_musiccpp_* the Rust reaches the rest through.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEScore.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEMedia.h"
#include "ee/EEMusicObject.h"
#include "ee/EEMusicManager.h"
#include "rust/audio/sound.h"
#include "rust/game/gamecpp.h"   // round 11, lane B (ee_game gamecpp music_io.rs)

// Creates the music manager and starts its music thread at the given priority, waiting until the thread runs
EEMusicManager::EEMusicManager(SEConductor* inConductor, long inThreadPriority)
    : mConductor(inConductor)
{
    ee_gamecpp_music_init(this, inThreadPriority);
}

// Destroys the music manager: tells the thread to exit, waits for it and frees the events and the thread
EEMusicManager::~EEMusicManager()
{
    ee_gamecpp_music_destroy(this);
}

// The C++ side of the Rust music manager: the members, the conductor, the songs, the world and the preferences
static EEMusicManager* Of(void* inContext) { return static_cast<EEMusicManager*>(inContext); }

extern "C" {
// (ee_musiccpp_get, _set: ee_game gamecpp music_io.rs)
void* ee_musiccpp_music(void* inContext) { return Of(inContext)->mMusic; }
void ee_musiccpp_set_music(void* inContext, void* inSong) { Of(inContext)->mMusic = static_cast<EEMusicObject*>(inSong); }
float ee_musiccpp_random(void* inContext) { return Of(inContext)->mRandom.Generate(); }
void ee_musiccpp_request_stop(void* inContext) { Of(inContext)->Stop(); }
void ee_musiccpp_last_error() { PlatGetLastError(); }
// (ee_musiccpp_current_epoch: ee_game gamecpp music_io.rs)
float ee_musiccpp_music_volume() { return gGamePreferences.mSettings.GetMusicVolume(); }
void ee_musiccpp_sleep(uint32_t inMilliseconds) { PlatSleep(inMilliseconds); }
void* ee_musiccpp_random_song(int32_t inEpoch) { return EEMusicObject::GetRandom(inEpoch); }
void ee_musiccpp_song_load(void* inSong) { static_cast<EEMusicObject*>(inSong)->LoadScore(); }
int32_t ee_musiccpp_song_score_id(void* inSong) { return (int32_t)static_cast<EEMusicObject*>(inSong)->mScoreID; }
int32_t ee_musiccpp_song_unload(void* inSong) { return (int32_t)static_cast<EEMusicObject*>(inSong)->UnloadScore(); }
// (ee_musiccpp_song_reverb: ee_game gamecpp music_io.rs)
float ee_musiccpp_volume(void* inContext) { return Of(inContext)->mConductor->GetVolume(); }
void ee_musiccpp_set_volume(void* inContext, float inVolume) { Of(inContext)->mConductor->SetVolume(inVolume); }
void ee_musiccpp_stop(void* inContext) { Of(inContext)->mConductor->Stop(); }
void* ee_musiccpp_score(void* inContext, int32_t inID) { return Of(inContext)->mConductor->GetScore(inID); }
void ee_musiccpp_set_repeat_count(void* inScore, uint32_t inCount)
{
    static_cast<SEScore*>(inScore)->SetRepeatCount(inCount);
}
void ee_musiccpp_set_intensity(void* inContext, uint32_t inIntensity, bool inImmediate)
{
    Of(inContext)->mConductor->SetIntensity(inIntensity, inImmediate);
}
void ee_musiccpp_set_reverb_style(void* inContext, const float* inReverb)
{
    Of(inContext)->mConductor->SetReverbStyle(inReverb[0], inReverb[1], inReverb[2], inReverb[3]);
}
void ee_musiccpp_play(void* inContext, int32_t inID) { Of(inContext)->mConductor->Play(inID); }
void ee_musiccpp_transition_to(void* inContext, int32_t inID, uint32_t inCommand, uint32_t inFlags)
{
    Of(inContext)->mConductor->TransitionTo(inID, inCommand, inFlags);
}
void ee_musiccpp_global_set_intensity(uint32_t inIntensity, bool inImmediate)
{
    gConductor->GetConductor().SetIntensity(inIntensity, inImmediate);
}
}

// (the music thread, MusicManagerMain: ee_game gamecpp music_io.rs ee_gamecpp_music_main)

// Asks the music thread to pause the music
void EEMusicManager::Pause()
{
    this->mPause->Set();
}

// Requests a new music intensity, delaying a drop by five seconds; the music thread applies it
void EEMusicManager::SetIntensity(unsigned long inIntensity)
{
    ee_music_request_intensity(this, (uint32_t)inIntensity);
    this->mSetIntensity->Set();
}

// Asks the music thread to start the music
void EEMusicManager::Start()
{
    this->mStart->Set();
}

// Asks the music thread to stop the music
void EEMusicManager::Stop()
{
    this->mStop->Set();
}

// Counts down the time to the next song, switching songs when it runs out
void EEMusicManager::Update(unsigned long inElapsedTime)
{
    ee_music_update(this, (uint32_t)inElapsedTime);
}
