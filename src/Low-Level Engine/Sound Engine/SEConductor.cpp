// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.cpp
// Music score playback on dmusic (src/ThirdParty/dmusic), replacing the original's DirectMusic (COM performance,
// composer, loader and a DLS port). The performance renders at the mixer's output rate and the mixer's device thread
// pulls it (Render, through SEMixer::SetMusicSource). The score files (segments, styles, DLS banks) are read from the
// music folder (Resolve). The public behaviour follows the original's: scores are loaded by name and remembered while
// the conductor is stopped, played, stopped and transitioned to; the intensity is the master groove level.
// The score table and every decision are Rust (rust/crates/ee_sound conductor.rs, docs/RUST_PLAN.md R3.6): this file
// keeps dmusic's calls (the trampolines ee_conductorcpp_*), the scores, the lock and the scalar members, copied into
// the Rust call and back under the lock (SEConductor::Rust::Fields).
#include <windows.h>
#include "platform/PlatFile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dmusic.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SEScore.h"
#include "rust/ee_rust.h"

// The C++ side of the Rust conductor
struct SEConductor::Rust {
    // the scalar members as the Rust call sees them, written back when the call returns (the lock is held)
    struct Fields {
        SEConductor* mConductor;
        EeConductorFields mFields;
        explicit Fields(SEConductor* inConductor) : mConductor(inConductor)
        {
            this->mFields.volume = inConductor->mVolume;
            this->mFields.state = (uint32_t)inConductor->mConductorState;
            this->mFields.reverb_state = (uint32_t)inConductor->mReverbState;
            this->mFields.next_score_id = (int32_t)inConductor->mNextScoreID;
            this->mFields.playing_id = (int32_t)inConductor->mPlayingID;
            this->mFields.intensity = (uint32_t)inConductor->mIntensity;
            this->mFields.channel_group_count = (uint32_t)inConductor->mChannelGroupCount;
        }
        ~Fields()
        {
            this->mConductor->mVolume = this->mFields.volume;
            this->mConductor->mConductorState = this->mFields.state;
            this->mConductor->mReverbState = this->mFields.reverb_state;
            this->mConductor->mNextScoreID = this->mFields.next_score_id;
            this->mConductor->mPlayingID = this->mFields.playing_id;
            this->mConductor->mIntensity = this->mFields.intensity;
            this->mConductor->mChannelGroupCount = this->mFields.channel_group_count;
        }
        Fields(const Fields&) = delete;
        Fields& operator=(const Fields&) = delete;
    };
    static SEConductor* Of(void* inContext) { return static_cast<SEConductor*>(inContext); }
    static uint32_t OutputRate(void* inContext)
    {
        SEConductor* theThis = Of(inContext);
        return theThis->mMixer ? (uint32_t)theThis->mMixer->GetOutputRate() : 0;
    }
    // the loader (reading from the music folder) and the performance; on a failure nothing is kept
    static bool Start(void* inContext, uint32_t inRate)
    {
        SEConductor* theThis = Of(inContext);
        DmLoader* theLoader = NULL;
        DmPerformance* thePerformance = NULL;
        if (DmLoader_create(&theLoader, DmLoader_DOWNLOAD) != DmResult_SUCCESS)
            return false;
        if (DmLoader_addResolver(theLoader, SEConductor::Resolve, theThis) == DmResult_SUCCESS &&
            DmPerformance_create(&thePerformance, inRate) == DmResult_SUCCESS) {
            theThis->mLoader = theLoader;
            theThis->mPerformance = thePerformance;
            return true;
        }
        DmLoader_release(theLoader);
        return false;
    }
    static void Release(void* inContext)
    {
        SEConductor* theThis = Of(inContext);
        DmPerformance_release(theThis->mPerformance);
        DmLoader_release(theThis->mLoader);
        theThis->mPerformance = NULL;
        theThis->mLoader = NULL;
    }
    static void SetMusicSource(void* inContext, bool inOn)
    {
        SEConductor* theThis = Of(inContext);
        if (inOn)
            theThis->mMixer->SetMusicSource(SEConductor::Render, theThis);
        else if (theThis->mMixer)
            theThis->mMixer->SetMusicSource(NULL, NULL);   // the device thread no longer renders after this returns
    }
    static void* Load(void* inContext, const char* inName)
    {
        DmSegment* theSegment = NULL;
        if (DmLoader_getSegment(Of(inContext)->mLoader, inName, &theSegment) != DmResult_SUCCESS || theSegment == NULL)
            return NULL;
        return new SEScore(theSegment, false);
    }
    static void Delete(void* inScore) { delete static_cast<SEScore*>(inScore); }
    static bool IsSecondary(void* inScore) { return static_cast<SEScore*>(inScore)->mIsSecondaryScore; }
    static bool Play(void* inContext, void* inScore, uint32_t inTiming)
    {
        DmSegment* theSegment = inScore ? static_cast<SEScore*>(inScore)->GetSegment() : NULL;
        return DmPerformance_playSegment(Of(inContext)->mPerformance, theSegment, (DmTiming)inTiming) ==
               DmResult_SUCCESS;
    }
    static bool PlayTransition(void* inContext, void* inScore, uint32_t inPattern)
    {
        return DmPerformance_playTransition(Of(inContext)->mPerformance, static_cast<SEScore*>(inScore)->GetSegment(),
                                            (DmEmbellishmentType)inPattern, DmTiming_MEASURE) == DmResult_SUCCESS;
    }
    static void SetVolume(void* inContext, float inAmplitude)
    {
        DmPerformance_setVolume(Of(inContext)->mPerformance, inAmplitude);
    }
    static void SetGrooveLevel(void* inContext, int8_t inLevel)
    {
        DmPerformance_setMasterGrooveLevel(Of(inContext)->mPerformance, inLevel);
    }
};

extern "C" {
uint32_t ee_conductorcpp_output_rate(void* inContext) { return SEConductor::Rust::OutputRate(inContext); }
bool ee_conductorcpp_start(void* inContext, uint32_t inRate) { return SEConductor::Rust::Start(inContext, inRate); }
void ee_conductorcpp_release(void* inContext) { SEConductor::Rust::Release(inContext); }
void ee_conductorcpp_set_music_source(void* inContext, bool inOn) { SEConductor::Rust::SetMusicSource(inContext, inOn); }
void* ee_conductorcpp_load(void* inContext, const char* inName) { return SEConductor::Rust::Load(inContext, inName); }
void ee_conductorcpp_delete(void* inScore) { SEConductor::Rust::Delete(inScore); }
bool ee_conductorcpp_is_secondary(void* inScore) { return SEConductor::Rust::IsSecondary(inScore); }
bool ee_conductorcpp_play(void* inContext, void* inScore, uint32_t inTiming)
{
    return SEConductor::Rust::Play(inContext, inScore, inTiming);
}
bool ee_conductorcpp_play_transition(void* inContext, void* inScore, uint32_t inPattern)
{
    return SEConductor::Rust::PlayTransition(inContext, inScore, inPattern);
}
void ee_conductorcpp_set_volume(void* inContext, float inAmplitude) { SEConductor::Rust::SetVolume(inContext, inAmplitude); }
void ee_conductorcpp_set_groove_level(void* inContext, int8_t inLevel)
{
    SEConductor::Rust::SetGrooveLevel(inContext, inLevel);
}
}

// Creates the music conductor: volume 1, reverb on, not running, no scores
SEConductor::SEConductor()
    : mMusicFolder(0x104)
{
    for (unsigned long i = 0; i < 6; i++)
        this->mCallbacks[i] = NULL;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 45);
    EeConductorFields theFields = ee_conductor_fields_new();
    this->mRust = ee_conductor_create();
    this->mMusicFolder = "";
    this->mVolume = theFields.volume;
    this->mReverbState = theFields.reverb_state;
    this->mMixer = NULL;
    this->mPerformance = NULL;
    this->mLoader = NULL;
    this->mIntensity = theFields.intensity;
    this->mConductorState = theFields.state;
    this->mNextScoreID = theFields.next_score_id;
    this->mPlayingID = theFields.playing_id;
    this->mChannelGroupCount = theFields.channel_group_count;
}

// Destroys the conductor, stopping it first if it is running (a stopped conductor holds no score)
SEConductor::~SEConductor()
{
    if (this->mConductorState == 1)
        this->UninitializeConductor();
    ee_conductor_destroy(this->mRust);
    this->mRust = NULL;
    TSScheduler::DestroyCriticalSection(this->mLock);
}

// The mixer's device thread: renders the next frames of music (interleaved stereo float)
void SEConductor::Render(void* inConductor, float* outFrames, unsigned long inCount)
{
    SEConductor* theConductor = static_cast<SEConductor*>(inConductor);
    if (theConductor->mPerformance)
        DmPerformance_renderPcm(theConductor->mPerformance, outFrames, 2 * inCount,
                                (DmRenderOptions)(DmRender_FLOAT | DmRender_STEREO));
}

// dmusic's loader: reads a score file (segment, style or DLS bank) from the music folder into a malloc'd buffer
void* SEConductor::Resolve(void* inConductor, const char* inFile, size_t* outLength)
{
    SEConductor* theConductor = static_cast<SEConductor*>(inConductor);
    char thePath[MAX_PATH];
    const char* theFolder = theConductor->mMusicFolder;
    if (theFolder[0])
        snprintf(thePath, MAX_PATH, "%s\\%s", theFolder, inFile);
    else
        snprintf(thePath, MAX_PATH, "%s", inFile);
    thePath[MAX_PATH - 1] = 0;
    FILE* theStream = PlatFOpen(thePath, "rb");
    if (theStream == NULL)
        return NULL;
    fseek(theStream, 0, SEEK_END);
    long theLength = ftell(theStream);
    fseek(theStream, 0, SEEK_SET);
    void* theData = theLength > 0 ? malloc(theLength) : NULL;
    if (theData && fread(theData, 1, theLength, theStream) != (size_t)theLength) {
        free(theData);
        theData = NULL;
    }
    fclose(theStream);
    *outLength = theData ? (size_t)theLength : 0;
    return theData;
}

// Returns the ID of the score playing now (-1: none or not running)
long SEConductor::GetCurrentlyPlayingScore()
{
    this->mLock->Enter();
    long theID;
    {
        Rust::Fields theFields(this);
        theID = ee_conductor_currently_playing(&theFields.mFields);
    }
    this->mLock->Leave();
    return theID;
}

// Returns the score with the given ID (NULL: none)
SEScore* SEConductor::GetScore(long inID) const
{
    this->mLock->Enter();
    SEScore* theScore = static_cast<SEScore*>(ee_conductor_score(this->mRust, (int32_t)inID));
    this->mLock->Leave();
    return theScore;
}

// Returns the ID of the given score (-1: not found or NULL)
long SEConductor::GetScoreID(SEScore* inScore) const
{
    this->mLock->Enter();
    long theID = ee_conductor_score_id(this->mRust, inScore);
    this->mLock->Leave();
    return theID;
}

// Starts the performance at the mixer's output rate and the loader (reading from the music folder), reapplies
// volume and intensity, reloads the remembered scores and connects the performance to the mixer; returns 1 on failure
long SEConductor::InitializeConductor()
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_initialize(this->mRust, this, &theFields.mFields);
    }
    this->mLock->Leave();
    return theResult;
}

// Loads a music score by file name and returns its ID; a name already loaded only bumps its load count; while not
// running only the name is remembered (inSecondary reloads an entry at start-up)
long SEConductor::LoadScore(const UWideString& inName, long& outID, bool inSecondary)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        int32_t theID = 0;
        bool theWritten = false;
        theResult = ee_conductor_load_score(this->mRust, this, &theFields.mFields, (const char*)inName, inSecondary,
                                            &theID, &theWritten);
        if (theWritten)
            outID = theID;
    }
    this->mLock->Leave();
    return theResult;
}

// Plays the score with the given ID now; returns 1 on a dmusic error
long SEConductor::Play(long inID)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_play(this->mRust, this, &theFields.mFields, (int32_t)inID);
    }
    this->mLock->Leave();
    return theResult;
}

// Plays the score after the current one. dmusic has no segment queue: the score starts at the next measure (the
// game never queues)
long SEConductor::Queue(long inID)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_queue(this->mRust, this, &theFields.mFields, (int32_t)inID);
    }
    this->mLock->Leave();
    return theResult;
}

// Sets the channel-group count (at most 1000; kept for the interface); busy while the conductor is running
long SEConductor::SetChannelGroupCount(unsigned long inCount)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_set_channel_group_count(&theFields.mFields, (uint32_t)inCount);
    }
    this->mLock->Leave();
    return theResult;
}

// Starts (non-zero) or stops (0) the conductor when the requested state differs from the current one
long SEConductor::SetConductorState(unsigned long inState)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_set_state(this->mRust, this, &theFields.mFields, (uint32_t)inState);
    }
    this->mLock->Leave();
    return theResult;
}

// Sets the music intensity (the master groove level); while not running it is kept for the start
long SEConductor::SetIntensity(unsigned long inIntensity, bool inImmediate)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_set_intensity(this->mRust, this, &theFields.mFields, (uint32_t)inIntensity, inImmediate);
    }
    this->mLock->Leave();
    return theResult;
}

// Sets the music folder, dropping the given name's last character (the trailing separator)
long SEConductor::SetMusicFolder(const UWideString& inFolder)
{
    UWideString theFolder(0x104);
    this->mLock->Enter();
    theFolder.CopyString(inFolder, inFolder.GetLength() - 1);
    this->mMusicFolder = theFolder;
    this->mLock->Leave();
    return kErrorOK;
}

// Turns the reverb on (1) or off; dmusic's synthesizer has no reverb, so only the state is kept
long SEConductor::SetReverb(unsigned long inReverb)
{
    this->mLock->Enter();
    this->mReverbState = inReverb;
    this->mLock->Leave();
    return kErrorOK;
}

// The original set the DirectMusic port's Waves reverb; dmusic's synthesizer has none
long SEConductor::SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime)
{
    return kErrorOK;
}

// Sets the music volume (0..1; quieter than 0.01 is silent). The original set the port to 1000 * ln(volume)
// hundredths of a decibel, an amplitude of volume^(ln 10 / 2) (ee_sound conductor.rs amplitude)
long SEConductor::SetVolume(float inVolume)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_set_volume(this->mRust, this, &theFields.mFields, inVolume);
    }
    this->mLock->Leave();
    return theResult;
}

// Stops all music now while running; returns 1 on a dmusic error
long SEConductor::Stop()
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_stop(this->mRust, this, &theFields.mFields);
    }
    this->mLock->Leave();
    return theResult;
}

// Plays a transition pattern (fill, break, intro, end, else groove) from the playing score's style into the given
// score (NULL: into silence) at the next measure; returns 1 on a dmusic error
long SEConductor::TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_transition_to(this->mRust, this, &theFields.mFields, (int32_t)inID, (uint32_t)inCommand);
    }
    this->mLock->Leave();
    return theResult;
}

// Stops the conductor: disconnects it from the mixer, deletes every loaded score (keeping the names) and releases
// the performance and the loader
long SEConductor::UninitializeConductor()
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_uninitialize(this->mRust, this, &theFields.mFields);
    }
    this->mLock->Leave();
    return theResult;
}

// Unloads one load of the score with the given ID, deleting it at the last; busy while it is playing
long SEConductor::UnloadScore(long inID)
{
    this->mLock->Enter();
    long theResult;
    {
        Rust::Fields theFields(this);
        theResult = ee_conductor_unload_score(this->mRust, this, &theFields.mFields, (int32_t)inID);
    }
    this->mLock->Leave();
    return theResult;
}
