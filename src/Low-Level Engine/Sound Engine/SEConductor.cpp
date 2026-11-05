// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.cpp
// Music score playback on dmusic (src/ThirdParty/dmusic), replacing the original's DirectMusic (COM performance,
// composer, loader and a DLS port). The performance renders at the mixer's output rate and the mixer's device thread
// pulls it (Render, through SEMixer::SetMusicSource). The score files (segments, styles, DLS banks) are read from the
// music folder (Resolve). The public behaviour follows the original's: scores are loaded by name and remembered while
// the conductor is stopped, played, stopped and transitioned to; the intensity is the master groove level.
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

// Creates the music conductor: volume 1, reverb on, not running
SEConductor::SEConductor()
    : mMusicFolder(0x104)
{
    for (unsigned long i = 0; i < 6; i++)
        this->mCallbacks[i] = NULL;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 45);
    this->mMusicFolder = "";
    this->mVolume = 1.0f;
    this->mReverbState = 1;
    this->mMixer = NULL;
    this->mPerformance = NULL;
    this->mLoader = NULL;
    this->mIntensity = 0;
    this->mConductorState = 0;
    this->mNextScoreID = 0;
    this->mPlayingID = -1;
    this->mChannelGroupCount = 1000;
}

// Destroys the conductor, stopping it first if it is running
SEConductor::~SEConductor()
{
    if (this->mConductorState == 1)
        this->UninitializeConductor();
    this->mScores.clear();
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
    long theID = this->mConductorState == 1 ? this->mPlayingID : -1;
    this->mLock->Leave();
    return theID;
}

// Returns the score with the given ID (NULL: none)
SEScore* SEConductor::GetScore(long inID) const
{
    SEScore* theScore = NULL;
    this->mLock->Enter();
    for (unsigned long i = 0; i < this->mScores.size() && theScore == NULL; i++) {
        if (this->mScores[i].mID == inID)
            theScore = this->mScores[i].mScore;
    }
    this->mLock->Leave();
    return theScore;
}

// Returns the ID of the given score (-1: not found or NULL)
long SEConductor::GetScoreID(SEScore* inScore) const
{
    long theID = -1;
    this->mLock->Enter();
    for (unsigned long i = 0; i < this->mScores.size() && theID == -1; i++) {
        if (this->mScores[i].mScore == inScore && inScore != NULL)
            theID = this->mScores[i].mID;
    }
    this->mLock->Leave();
    return theID;
}

// Starts the performance at the mixer's output rate and the loader (reading from the music folder), reapplies
// volume and intensity, reloads the remembered scores and connects the performance to the mixer; returns 1 on failure
long SEConductor::InitializeConductor()
{
    long theResult = 1;
    this->mLock->Enter();
    unsigned long theRate = this->mMixer ? this->mMixer->GetOutputRate() : 0;
    DmLoader* theLoader = NULL;
    DmPerformance* thePerformance = NULL;
    if (theRate && DmLoader_create(&theLoader, DmLoader_DOWNLOAD) == DmResult_SUCCESS) {
        if (DmLoader_addResolver(theLoader, SEConductor::Resolve, this) == DmResult_SUCCESS &&
            DmPerformance_create(&thePerformance, theRate) == DmResult_SUCCESS) {
            this->mLoader = theLoader;
            this->mPerformance = thePerformance;
            this->mConductorState = 1;
            this->mPlayingID = -1;
            this->SetVolume(this->mVolume);
            this->SetIntensity(this->mIntensity, true);
            theResult = kErrorOK;
            for (unsigned long i = 0; i < this->mScores.size() && theResult == kErrorOK; i++) {
                long theID;
                theResult = this->LoadScore(this->mScores[i].mName, theID, true);
            }
            this->mMixer->SetMusicSource(SEConductor::Render, this);
        } else
            DmLoader_release(theLoader);
    }
    this->mLock->Leave();
    return theResult;
}

// Loads a music score by file name and returns its ID; a name already loaded only bumps its load count; while not
// running only the name is remembered (inSecondary reloads an entry at start-up)
long SEConductor::LoadScore(const UWideString& inName, long& outID, bool inSecondary)
{
    SEConductorScore theScore;
    bool theFound = false;
    long theResult = kErrorOK;
    unsigned long i;
    this->mLock->Enter();
    for (i = 0; i < this->mScores.size() && !theFound; i++)
        theFound = this->mScores[i].mName == inName;
    if (theFound && !inSecondary) {
        ++this->mScores[i - 1].mLoadCount;
        outID = this->mScores[i - 1].mID;
    } else if (this->mConductorState == 1) {
        DmSegment* theSegment = NULL;
        if (DmLoader_getSegment(this->mLoader, (const char*)inName, &theSegment) != DmResult_SUCCESS ||
            theSegment == NULL) {
            theResult = 1;
        } else {
            SEScore* theNewScore = new SEScore(theSegment, false);
            if (inSecondary) {
                this->mScores[i - 1].mScore = theNewScore;
                outID = this->mScores[i - 1].mID;
            } else {
                theScore.mName = inName;
                theScore.mID = this->mNextScoreID;
                theScore.mScore = theNewScore;
                theScore.mLoadCount = 1;
                this->mScores.push_back(theScore);
                this->mNextScoreID++;
                if (this->mNextScoreID == -1)
                    this->mNextScoreID = 0;
                outID = theScore.mID;
            }
        }
    } else if (!inSecondary) {
        theScore.mName = inName;
        theScore.mScore = NULL;
        theScore.mID = this->mNextScoreID;
        theScore.mLoadCount = 1;
        this->mScores.push_back(theScore);
        this->mNextScoreID++;
        if (this->mNextScoreID == -1)
            this->mNextScoreID = 0;
        outID = theScore.mID;
    }
    this->mLock->Leave();
    return theResult;
}

// Plays the score with the given ID now; returns 1 on a dmusic error
long SEConductor::Play(long inID)
{
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    long theResult = kErrorOK;
    if (this->mConductorState == 1 && theScore) {
        if (DmPerformance_playSegment(this->mPerformance, theScore->GetSegment(), DmTiming_INSTANT) == DmResult_SUCCESS)
            this->mPlayingID = inID;
        else
            theResult = 1;
    }
    this->mLock->Leave();
    return theResult;
}

// Plays the score after the current one. dmusic has no segment queue: the score starts at the next measure (the
// game never queues)
long SEConductor::Queue(long inID)
{
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    long theResult = kErrorOK;
    if (this->mConductorState == 1 && theScore) {
        if (theScore->mIsSecondaryScore)
            theResult = kErrorFailed;
        else if (DmPerformance_playSegment(this->mPerformance, theScore->GetSegment(), DmTiming_MEASURE) ==
                 DmResult_SUCCESS)
            this->mPlayingID = inID;
        else
            theResult = 1;
    }
    this->mLock->Leave();
    return theResult;
}

// Sets the channel-group count (at most 1000; kept for the interface); busy while the conductor is running
long SEConductor::SetChannelGroupCount(unsigned long inCount)
{
    this->mLock->Enter();
    long theResult = kErrorOK;
    if (this->mConductorState == 0)
        this->mChannelGroupCount = inCount < 1000 ? inCount : 1000;
    else
        theResult = kErrorBusy;
    this->mLock->Leave();
    return theResult;
}

// Starts (non-zero) or stops (0) the conductor when the requested state differs from the current one
long SEConductor::SetConductorState(unsigned long inState)
{
    long theResult;
    this->mLock->Enter();
    if (inState != this->mConductorState)
        theResult = inState == 0 ? this->UninitializeConductor() : this->InitializeConductor();
    else
        theResult = kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// Sets the music intensity (the master groove level); while not running it is kept for the start
long SEConductor::SetIntensity(unsigned long inIntensity, bool inImmediate)
{
    this->mLock->Enter();
    if (inIntensity != this->mIntensity || inImmediate) {
        this->mIntensity = inIntensity;
        if (this->mConductorState == 1)
            DmPerformance_setMasterGrooveLevel(this->mPerformance, (int8_t)inIntensity);
    }
    this->mLock->Leave();
    return kErrorOK;
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
// hundredths of a decibel, an amplitude of volume^(ln 10 / 2)
long SEConductor::SetVolume(float inVolume)
{
    this->mLock->Enter();
    this->mVolume = inVolume;
    if (this->mConductorState == 1)
        DmPerformance_setVolume(this->mPerformance, inVolume > 0.01f ? (float)pow(inVolume, 1.1512925) : 0.0f);
    this->mLock->Leave();
    return kErrorOK;
}

// Stops all music now while running; returns 1 on a dmusic error
long SEConductor::Stop()
{
    long theResult = kErrorOK;
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        if (DmPerformance_playSegment(this->mPerformance, NULL, DmTiming_INSTANT) != DmResult_SUCCESS)
            theResult = 1;
        this->mPlayingID = -1;
    }
    this->mLock->Leave();
    return theResult;
}

// Plays a transition pattern (fill, break, intro, end, else groove) from the playing score's style into the given
// score (NULL: into silence) at the next measure; returns 1 on a dmusic error
long SEConductor::TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
{
    long theResult = kErrorOK;
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    if (this->mConductorState == 1) {
        DmEmbellishmentType theEmbellishment;
        switch (inCommand) {
        case kMusicTransitionFill:
            theEmbellishment = DmEmbellishment_FILL;
            break;
        case kMusicTransitionBreak:
            theEmbellishment = DmEmbellishment_BREAK;
            break;
        case kMusicTransitionIntro:
            theEmbellishment = DmEmbellishment_INTRO;
            break;
        case kMusicTransitionEnd:
        case kMusicTransitionEndAndIntro:
            theEmbellishment = DmEmbellishment_END;
            break;
        default:
            theEmbellishment = DmEmbellishment_GROOVE;
            break;
        }
        DmResult theError = theScore
            ? DmPerformance_playTransition(this->mPerformance, theScore->GetSegment(), theEmbellishment, DmTiming_MEASURE)
            : DmPerformance_playSegment(this->mPerformance, NULL, DmTiming_MEASURE);
        if (theError == DmResult_SUCCESS)
            this->mPlayingID = theScore ? inID : -1;
        else
            theResult = 1;
    }
    this->mLock->Leave();
    return theResult;
}

// Stops the conductor: disconnects it from the mixer, deletes every loaded score (keeping the names) and releases
// the performance and the loader
long SEConductor::UninitializeConductor()
{
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        if (this->mMixer)
            this->mMixer->SetMusicSource(NULL, NULL);   // the device thread no longer renders after this returns
        for (unsigned long i = 0; i < this->mScores.size(); i++) {
            if (this->mScores[i].mScore)
                delete this->mScores[i].mScore;
            this->mScores[i].mScore = NULL;
        }
        DmPerformance_release(this->mPerformance);
        DmLoader_release(this->mLoader);
        this->mPerformance = NULL;
        this->mLoader = NULL;
        this->mPlayingID = -1;
        this->mConductorState = 0;
    }
    this->mLock->Leave();
    return kErrorOK;
}

// Unloads one load of the score with the given ID, deleting it at the last; busy while it is playing
long SEConductor::UnloadScore(long inID)
{
    long theResult;
    this->mLock->Enter();
    unsigned long i;
    for (i = 0; i < this->mScores.size() && this->mScores[i].mID != inID; i++)
        ;
    if (i < this->mScores.size()) {
        SEScore* theScore = this->mScores[i].mScore;
        if (inID == -1)
            theResult = kErrorNotFound;
        else if (inID == this->GetCurrentlyPlayingScore())
            theResult = kErrorBusy;
        else {
            std::vector<SEConductorScore>::iterator theEntry = this->mScores.begin() + i;
            if (theEntry->mLoadCount == 1) {
                this->mScores.erase(theEntry);
                if (theScore)
                    delete theScore;
            } else
                theEntry->mLoadCount--;
            theResult = kErrorOK;
        }
    } else
        theResult = kErrorNotFound;
    this->mLock->Leave();
    return theResult;
}
