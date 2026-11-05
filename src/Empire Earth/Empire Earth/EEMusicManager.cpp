// C:\EEX\Empire Earth\Empire Earth\EEMusicManager.cpp (__FILE__ 0x4bfd1c). The music thread: 0x131b7c..0x13242b (the
// two fade globals and the thread name first, then the class in definition order; the two fades are file statics,
// each after its first user). Unit flags: the exe's module set. Data: the fade constants 0x441fb0/0x441fb4, the vtable
// 0x441fbc, the song delay constants 0x441fc0/0x441fc4.
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

static const float kFadeSteps = 3.0f;
static const float kFadeTime = 0.1f;
static float sFadeStep = 1.0f / kFadeSteps;
static long sFadeSleep = kFadeTime * 1000.0f;
static UString sThreadName("MusicManager");

static const float kMinimumSongDelay = 300000.0f;
static const float kSongDelayRange = 120001.0f;

static void FadeIn(SEConductor* inConductor);
static void FadeOut(SEConductor* inConductor);

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

// FUNCTION: EXE 0x531e0a
// Runs the music thread: waits for exit, pause, intensity, start or stop requests (or the next-song timeout) and
// handles them until told to exit
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
        if (theThis->mIntensityDelay <= theElapsedTime)
            theThis->mIntensityDelay = 0;
        else
            theThis->mIntensityDelay -= theElapsedTime;
        switch (theResult) {
        case kTSWaitFailed:
            PlatGetLastError();
            break;
        case kTSWaitTimeout:
            if (theThis->mState == 2)
                theThis->NextSong();
            break;
        case 4:
            theThis->DoStop();
            break;
        case 3:
            if (theThis->mState == 2)
                theThis->Update(theElapsedTime);
            else
                theThis->DoStart();
            break;
        case 2:
            theThis->DoSetIntensity();
            if (theThis->mState == 2)
                theThis->Update(theElapsedTime);
            break;
        case 1:
            theThis->DoPause();
            break;
        case 0:
            theThis->DoStop();
            theDone = true;
            break;
        }
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
    if (this->mState == 2) {
        this->mState = 1;
        this->mConductor->Stop();
    }
}

// FUNCTION: EXE 0x531f87
// Requests a new music intensity, delaying a drop by five seconds; the music thread applies it
void EEMusicManager::SetIntensity(unsigned long inIntensity)
{
    if (this->mIntensityDelay == 0 && inIntensity < this->mIntensity)
        this->mIntensityDelay = 5000;
    else if (inIntensity > this->mIntensity)
        this->mIntensityDelay = 0;
    this->mRequestedIntensity = inIntensity;
    this->mSetIntensity->Set();
}

// FUNCTION: EXE 0x531fb7
// Applies the requested music intensity (a drop waits for its delay) and, while playing, transitions the score;
// rising and falling use the same transition
void EEMusicManager::DoSetIntensity()
{
    unsigned long theOldIntensity = this->mIntensity;
    if (this->mIntensityDelay != 0 && !(this->mRequestedIntensity > theOldIntensity))
        return;
    this->mIntensity = this->mRequestedIntensity;
    gConductor->GetConductor().SetIntensity(this->mIntensity, false);
    if (this->mIntensity == theOldIntensity || this->mState != 2)
        return;
    if (this->mIntensity > theOldIntensity)
        this->mConductor->TransitionTo(this->mScoreID, kMusicTransitionGroove, kTransitionFlagMeasure);
    else
        this->mConductor->TransitionTo(this->mScoreID, kMusicTransitionGroove, kTransitionFlagMeasure);
}

// FUNCTION: EXE 0x532000
// Asks the music thread to start the music
void EEMusicManager::Start()
{
    this->mStart->Set();
}

// FUNCTION: EXE 0x532009
// Starts a random song of the current epoch (or resumes the paused one) with its reverb, fading in when the
// music was audible, and schedules the next song in five to seven minutes
void EEMusicManager::DoStart()
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTree* theTechnologies = thePlayer != NULL ? thePlayer->mTechnologies : NULL;
    long theEpoch = theTechnologies != NULL ? theTechnologies->mCurrentEpoch : 0;
    bool theFade = this->mConductor->GetVolume() < -0.0001f || this->mConductor->GetVolume() > 0.0001f;
    switch (this->mState) {
    case 0:
        this->mMusic = EEMusicObject::GetRandom(theEpoch);
        this->mMusic->LoadScore();
        this->mScoreID = this->mMusic->mScoreID;
    case 1: {
        this->mConductor->SetVolume(0.0f);
        SEScore* theScore = this->mConductor->GetScore(this->mScoreID);
        if (theScore != NULL) {
            theScore->SetRepeatCount(100000);
            this->mConductor->SetIntensity(this->mIntensity, false);
            this->mConductor->SetReverbStyle(this->mMusic->GetReverbGain(), this->mMusic->GetReverbHighFreqRTRatio(),
                                       this->mMusic->GetReverbMix(), this->mMusic->GetReverbTime());
            this->mConductor->Play(this->mScoreID);
            if (theFade)
                FadeIn(this->mConductor);
            long theDelay = UMFloatToLong(this->mRandom.Generate() * kSongDelayRange + kMinimumSongDelay);
            if (theDelay == 420001)
                theDelay = 420000;
            this->mTimeToNextSong = theDelay;
            this->mState = 2;
        } else
            this->Stop();
        break;
    }
    }
}

// FUNCTION: EXE 0x532148
// Fades the music in from silence to the music volume setting
static void FadeIn(SEConductor* inConductor)
{
    float theVolume = 0.0f;
    while (theVolume < gGamePreferences.mSettings.GetMusicVolume()) {
        inConductor->SetVolume(theVolume);
        theVolume += sFadeStep * kFadeTime;
        PlatSleep(sFadeSleep);
    }
    inConductor->SetVolume(gGamePreferences.mSettings.GetMusicVolume());
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
    this->mState = 0;
    this->mConductor->Stop();
    if (this->mMusic == NULL)
        return;
    long theResult;
    do {
        theResult = this->mMusic->UnloadScore();
        PlatSleep(1);
    } while (theResult == 5);
    this->mMusic = NULL;
}

// FUNCTION: EXE 0x5321ef
// Switches to another random song of the current epoch (fading out and in when audible) and schedules the next
// song in five to seven minutes
void EEMusicManager::NextSong()
{
    EEMusicObject* theOldMusic = this->mMusic;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EETechTree* theTechnologies = thePlayer != NULL ? thePlayer->mTechnologies : NULL;
    long theEpoch = theTechnologies != NULL ? theTechnologies->mCurrentEpoch : 0;
    bool theFade = this->mConductor->GetVolume() < -0.0001f || this->mConductor->GetVolume() > 0.0001f;
    long theDelay;
    if (theTechnologies != NULL) {
        this->mMusic = EEMusicObject::GetRandom(theEpoch);
        if (this->mMusic == theOldMusic)
            this->mMusic = EEMusicObject::GetRandom(theEpoch);
        if (this->mMusic != theOldMusic) {
            this->mMusic->LoadScore();
            this->mScoreID = this->mMusic->mScoreID;
            if (theFade)
                FadeOut(this->mConductor);
            this->mConductor->Stop();
            if (theOldMusic != NULL) {
                long theResult;
                do {
                    theResult = theOldMusic->UnloadScore();
                    PlatSleep(1);
                } while (theResult == 5);
            }
            SEScore* theScore = this->mConductor->GetScore(this->mScoreID);
            if (theScore == NULL)
                return;
            theScore->SetRepeatCount(100000);
            this->mConductor->SetIntensity(this->mIntensity, false);
            this->mConductor->SetReverbStyle(this->mMusic->GetReverbGain(), this->mMusic->GetReverbHighFreqRTRatio(),
                                       this->mMusic->GetReverbMix(), this->mMusic->GetReverbTime());
            this->mConductor->Play(this->mScoreID);
            if (theFade)
                FadeIn(this->mConductor);
            theDelay = UMFloatToLong(this->mRandom.Generate() * kSongDelayRange + kMinimumSongDelay);
        } else
            theDelay = UMFloatToLong(this->mRandom.Generate() * kSongDelayRange + kMinimumSongDelay);
    } else
        theDelay = UMFloatToLong(this->mRandom.Generate() * kSongDelayRange + kMinimumSongDelay);
    if (theDelay == 420001)
        theDelay = 420000;
    this->mTimeToNextSong = theDelay;
    this->mState = 2;
}

// FUNCTION: EXE 0x5323a9
// Fades the music out from the music volume setting to silence
static void FadeOut(SEConductor* inConductor)
{
    float theVolume = gGamePreferences.mSettings.GetMusicVolume();
    while (theVolume > 0.0f) {
        inConductor->SetVolume(theVolume);
        theVolume -= sFadeStep * kFadeTime;
        PlatSleep(sFadeSleep);
    }
    inConductor->SetVolume(0.0f);
}

// FUNCTION: EXE 0x532410
// Counts down the time to the next song, switching songs when it runs out
void EEMusicManager::Update(unsigned long inElapsedTime)
{
    if (this->mTimeToNextSong > inElapsedTime)
        this->mTimeToNextSong -= inElapsedTime;
    else
        this->NextSong();
}

// SYNTHETIC: EXE 0x531b7c
// SYNTHETIC: EXE 0x531b81
// SYNTHETIC: EXE 0x531b90
// SYNTHETIC: EXE 0x531b95
// SYNTHETIC: EXE 0x531bac
// SYNTHETIC: EXE 0x531bb6
// SYNTHETIC: EXE 0x531bc7
// SYNTHETIC: EXE 0x531bd3
