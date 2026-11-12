// C:\EEX\Empire Earth\Empire Earth\EEMusicManager.cpp (__FILE__ 0x4bfd1c). The music thread: 0x131b7c..0x13242b (the
// two fade globals and the thread name first, then the class in definition order; the two fades are file statics,
// each after its first user). Unit flags: the exe's module set. Data: the fade constants 0x441fb0/0x441fb4, the vtable
// 0x441fbc, the song delay constants 0x441fc0/0x441fc4.
// The decisions (the thread's step, start, pause, stop, the song changes, the fades, the intensity rules, the time
// to the next song) are Rust: rust/crates/ee_sound music.rs (docs/RUST_PLAN.md R3.6). This file keeps the members,
// the thread, its events and the wait; the Rust reaches the rest through the trampolines ee_musiccpp_*.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/URandom.h"
#include "lle/UReferenceCounter.h"
#include "lle/SEMixer.h"
#include "lle/SEScore.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEMedia.h"
#include "ee/EEMusicObject.h"
#include "ee/EEMusicManager.h"
#include "rust/ee_sound.h"

static UString sThreadName("MusicManager");

// FUNCTION: EXE 0x531bde
// Creates the music manager and starts its music thread at the given priority, waiting until the thread runs
EEMusicManager::EEMusicManager(SEConductor* inConductor, long inThreadPriority)
    : mConductor(inConductor)
{
    this->mMusic = NULL;
    this->mScoreID = 0;
    this->mIntensityDelay = 0;
    this->mIntensity = 0;
    this->mRequestedIntensity = 0;
    this->mState = 0;
    this->mTimeToNextSong = 600000;
    this->mRandom.SetSeed(TMGetTime());
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated, false) == keseOK, 124);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone, false) == keseOK, 125);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mExit, false) == keseOK, 126);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mPause, false) == keseOK, 127);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mSetIntensity, false) == keseOK, 128);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStart, false) == keseOK, 129);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mStop, false) == keseOK, 130);

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, EEMusicManager::MusicManagerMain, (void*)this) == keseOK, 133);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 134);
    this->mThreadCreated->Wait(kTSWaitInfinite);
    this->mThread->SetName(sThreadName);
    this->mConductor->SetIntensity(this->mIntensity, true);
}

// SYNTHETIC: EXE 0x531d98
// (the scalar deleting destructor, emitted with the vtable; slot EEMusicManager+0x0)

// FUNCTION: EXE 0x531db4
// Destroys the music manager: tells the thread to exit, waits for it and frees the events and the thread
EEMusicManager::~EEMusicManager()
{
    this->mExit->Set();
    this->mThreadDone->Wait(kTSWaitInfinite);
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyEvent(this->mExit);
    TSScheduler::DestroyEvent(this->mPause);
    TSScheduler::DestroyEvent(this->mSetIntensity);
    TSScheduler::DestroyEvent(this->mStart);
    TSScheduler::DestroyEvent(this->mStop);
    TSScheduler::DestroyThread(this->mThread);
}

// The C++ side of the Rust music manager: the members, the conductor, the songs, the world and the preferences
static EEMusicManager* Of(void* inContext) { return static_cast<EEMusicManager*>(inContext); }

extern "C" {
// the members by number (music_ffi.rs: 0 score id, 1 state, 2 intensity delay, 3 intensity, 4 time to the next
// song, 5 requested intensity), as 32-bit values
uint32_t ee_musiccpp_get(void* inContext, uint32_t inField)
{
    EEMusicManager* m = Of(inContext);
    switch (inField) {
    case 0: return (uint32_t)m->mScoreID;
    case 1: return (uint32_t)m->mState;
    case 2: return (uint32_t)m->mIntensityDelay;
    case 3: return (uint32_t)m->mIntensity;
    case 4: return (uint32_t)m->mTimeToNextSong;
    default: return (uint32_t)m->mRequestedIntensity;
    }
}
void ee_musiccpp_set(void* inContext, uint32_t inField, uint32_t inValue)
{
    EEMusicManager* m = Of(inContext);
    switch (inField) {
    case 0: m->mScoreID = (int32_t)inValue; break;
    case 1: m->mState = (int32_t)inValue; break;
    case 2: m->mIntensityDelay = inValue; break;
    case 3: m->mIntensity = inValue; break;
    case 4: m->mTimeToNextSong = inValue; break;
    default: m->mRequestedIntensity = inValue; break;
    }
}
void* ee_musiccpp_music(void* inContext) { return Of(inContext)->mMusic; }
void ee_musiccpp_set_music(void* inContext, void* inSong) { Of(inContext)->mMusic = static_cast<EEMusicObject*>(inSong); }
float ee_musiccpp_random(void* inContext) { return Of(inContext)->mRandom.Generate(); }
void ee_musiccpp_request_stop(void* inContext) { Of(inContext)->Stop(); }
void ee_musiccpp_last_error() { PlatGetLastError(); }
bool ee_musiccpp_current_epoch(int32_t* outEpoch)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTree* theTechnologies = thePlayer != NULL ? thePlayer->mTechnologies : NULL;
    if (theTechnologies == NULL)
        return false;
    *outEpoch = (int32_t)theTechnologies->mCurrentEpoch;
    return true;
}
float ee_musiccpp_music_volume() { return gGamePreferences.mSettings.GetMusicVolume(); }
void ee_musiccpp_sleep(uint32_t inMilliseconds) { PlatSleep(inMilliseconds); }
void* ee_musiccpp_random_song(int32_t inEpoch) { return EEMusicObject::GetRandom(inEpoch); }
void ee_musiccpp_song_load(void* inSong) { static_cast<EEMusicObject*>(inSong)->LoadScore(); }
int32_t ee_musiccpp_song_score_id(void* inSong) { return (int32_t)static_cast<EEMusicObject*>(inSong)->mScoreID; }
int32_t ee_musiccpp_song_unload(void* inSong) { return (int32_t)static_cast<EEMusicObject*>(inSong)->UnloadScore(); }
void ee_musiccpp_song_reverb(void* inSong, float* outReverb)
{
    EEMusicObject* s = static_cast<EEMusicObject*>(inSong);
    outReverb[0] = s->GetReverbGain();
    outReverb[1] = s->GetReverbHighFreqRTRatio();
    outReverb[2] = s->GetReverbMix();
    outReverb[3] = s->GetReverbTime();
}
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

// FUNCTION: EXE 0x531e0a
// Runs the music thread: waits for exit, pause, intensity, start or stop requests (or the next-song timeout) and
// hands each wake-up to the Rust step until it says to exit
unsigned long EEMusicManager::MusicManagerMain(void* inThis)
{
    unsigned long theLastTime = TMGetTime();
    EEMusicManager* theThis = static_cast<EEMusicManager*>(inThis);
    bool theDone = false;
    theThis->mThreadCreated->Set();
    TSEvent* theEvents[5];
    theEvents[0] = theThis->mExit;
    theEvents[1] = theThis->mPause;
    theEvents[2] = theThis->mSetIntensity;
    theEvents[3] = theThis->mStart;
    theEvents[4] = theThis->mStop;
    UMSetFloatingPointPrecision(kFPPrecision53);
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        long theResult = TSEvent::WaitAny(theEvents, 5, theThis->mTimeToNextSong);
        unsigned long theTime = TMGetTime();
        unsigned long theElapsedTime = theTime - theLastTime;
        theLastTime = theTime;
        theDone = ee_music_step(theThis, (int32_t)theResult, (uint32_t)theElapsedTime);
    } while (!theDone);
    theThis->mThreadDone->Set();
    return 0;
}

// FUNCTION: EXE 0x531f67
// Asks the music thread to pause the music
void EEMusicManager::Pause()
{
    this->mPause->Set();
}

// FUNCTION: EXE 0x531f70
// Pauses the playing music
void EEMusicManager::DoPause()
{
    ee_music_do(this, 0);
}

// FUNCTION: EXE 0x531f87
// Requests a new music intensity, delaying a drop by five seconds; the music thread applies it
void EEMusicManager::SetIntensity(unsigned long inIntensity)
{
    ee_music_request_intensity(this, (uint32_t)inIntensity);
    this->mSetIntensity->Set();
}

// FUNCTION: EXE 0x531fb7
// Applies the requested music intensity (a drop waits for its delay) and, while playing, transitions the score
void EEMusicManager::DoSetIntensity()
{
    ee_music_do(this, 1);
}

// FUNCTION: EXE 0x532000
// Asks the music thread to start the music
void EEMusicManager::Start()
{
    this->mStart->Set();
}

// FUNCTION: EXE 0x532009
// Starts a random song of the current epoch (or resumes the paused one) with its reverb, fading in when the music
// was audible, and schedules the next song in five to seven minutes
void EEMusicManager::DoStart()
{
    ee_music_do(this, 2);
}

// FUNCTION: EXE 0x5321b1
// Asks the music thread to stop the music
void EEMusicManager::Stop()
{
    this->mStop->Set();
}

// FUNCTION: EXE 0x5321ba
// Stops the music and unloads the current song's score
void EEMusicManager::DoStop()
{
    ee_music_do(this, 3);
}

// FUNCTION: EXE 0x5321ef
// Switches to another random song of the current epoch (fading out and in when audible) and schedules the next
// song in five to seven minutes
void EEMusicManager::NextSong()
{
    ee_music_do(this, 4);
}

// FUNCTION: EXE 0x532410
// Counts down the time to the next song, switching songs when it runs out
void EEMusicManager::Update(unsigned long inElapsedTime)
{
    ee_music_update(this, (uint32_t)inElapsedTime);
}

// SYNTHETIC: EXE 0x531b7c
// SYNTHETIC: EXE 0x531b81
// SYNTHETIC: EXE 0x531b90
// SYNTHETIC: EXE 0x531b95
// SYNTHETIC: EXE 0x531bac
// SYNTHETIC: EXE 0x531bb6
// SYNTHETIC: EXE 0x531bc7
// SYNTHETIC: EXE 0x531bd3
