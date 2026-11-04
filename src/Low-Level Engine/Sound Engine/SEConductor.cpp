// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The SE header inline block (SEConductor, SEMixer, SESample, SESMInstanceNode, SESoundManager and SEScore inline
// members, 0x4bbce..0x4c809) comes first, then the DirectMusic conductor (0x4c809..0x4dbcf) and its COMDATs.
// Compiler-generated markers are at the end of the file.
#include <windows.h>
#include <float.h>
#include <math.h>
#include <dmusici.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/SEScore.h"

// FUNCTION: LLE 0x1004c809
// Creates the music conductor: starts COM, makes its lock and notification events, volume 1, reverb on, uninitialised
SEConductor::SEConductor()
    : mMusicFolder(0x104)
{
    CoInitialize(NULL);
    for (unsigned long i = 0; i < 6; i++)
        this->mCallbacks[i] = NULL;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 45);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mEventHandlerExit, false) == keseOK, 48);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mEventHandlerThreadDone) == keseOK, 49);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mNotificationEvent, false) == keseOK, 50);
    this->mMusicFolder = "";
    this->mVolume = 1.0f;
    this->mReverbState = 1;
    this->mMixer = NULL;
    this->mComposer = NULL;
    this->mPerformance = NULL;
    this->mLoader = NULL;
    this->mDirectMusic = NULL;
    this->mPort = NULL;
    this->mIntensity = 0;
    this->mConductorState = 0;
    this->mNextScoreID = 0;
    this->mChannelGroupCount = 1000;
}

// FUNCTION: LLE 0x1004c919
// Destroys the conductor, shutting DirectMusic down first if it is running, then its events, lock and COM
SEConductor::~SEConductor()
{
    if (this->mConductorState == 1)
        this->UninitializeConductor();
    this->mScores.clear();
    TSScheduler::DestroyEvent(this->mEventHandlerExit);
    TSScheduler::DestroyEvent(this->mEventHandlerThreadDone);
    TSScheduler::DestroyEvent(this->mNotificationEvent);
    TSScheduler::DestroyCriticalSection(this->mLock);
    CoUninitialize();
}

// FUNCTION: LLE 0x1004c973
// Notification thread: maps DirectMusic performance/segment notifications onto the six callbacks (0 segment aborted,
// 1 almost ended, 2 ended, 3 started, 4 music started, 5 music stopped) with the score's ID, until told to exit
unsigned long SEConductor::EventHandler(void* inConductor)
{
    IDirectMusicSegmentState* theSegmentState = NULL;
    IDirectMusicSegment* theSegment = NULL;
    DMUS_NOTIFICATION_PMSG* theMessage;
    UMSetFloatingPointPrecision(_PC_53);
    SEConductor* theConductor = static_cast<SEConductor*>(inConductor);
    HANDLE theHandles[2];
    theHandles[0] = theConductor->mNotificationEvent->mHandle;
    theHandles[1] = theConductor->mEventHandlerExit->mHandle;
    for (;;) {
        if (UMGetFloatingPointPrecision() != _PC_53)
            UMSetFloatingPointPrecision(_PC_53);
        if (UMGetFloatingPointRoundingMode() != _RC_NEAR)
            UMSetFloatingPointRoundingMode(_RC_NEAR);
        switch (WaitForMultipleObjects(2, theHandles, FALSE, 3600000)) {
        case WAIT_OBJECT_0:
            if (SUCCEEDED(theConductor->mPerformance->GetNotificationPMsg(&theMessage)) && theMessage) {
                theConductor->mLock->Enter();
                long theID;
                unsigned long theEvent;
                bool theNotify = true;
                if (theMessage->guidNotificationType == GUID_NOTIFICATION_PERFORMANCE) {
                    theID = -1;
                    switch (theMessage->dwNotificationOption) {
                    case DMUS_NOTIFICATION_MUSICSTARTED:
                        theEvent = 4;
                        break;
                    case DMUS_NOTIFICATION_MUSICSTOPPED:
                        theEvent = 5;
                        break;
                    default:
                        theNotify = false;
                        break;
                    }
                } else if (theMessage->guidNotificationType == GUID_NOTIFICATION_SEGMENT) {
                    theID = -1;
                    theSegmentState = NULL;
                    theSegment = NULL;
                    theMessage->punkUser->QueryInterface(IID_IDirectMusicSegmentState, (void**)&theSegmentState);
                    if (theSegmentState) {
                        theSegmentState->GetSegment(&theSegment);
                        if (theSegment) {
                            unsigned long theCount = theConductor->mScores.size();
                            for (unsigned long i = 0; i < theCount && theID == -1; i++) {
                                if (theConductor->mScores[i].mScore &&
                                    theConductor->mScores[i].mScore->mSegment == theSegment)
                                    theID = theConductor->mScores[i].mID;
                            }
                            // Fixed: the reference GetSegment adds was never released
                            theSegment->Release();
                        }
                        theSegmentState->Release();
                        theSegmentState = NULL;
                    }
                    switch (theMessage->dwNotificationOption) {
                    case DMUS_NOTIFICATION_SEGSTART:
                        theEvent = 3;
                        break;
                    case DMUS_NOTIFICATION_SEGEND:
                        theEvent = 2;
                        break;
                    case DMUS_NOTIFICATION_SEGALMOSTEND:
                        theEvent = 1;
                        break;
                    case DMUS_NOTIFICATION_SEGABORT:
                        theEvent = 0;
                        break;
                    default:
                        theNotify = false;
                        break;
                    }
                } else
                    theNotify = false;
                if (theNotify && theConductor->mCallbacks[theEvent])
                    theConductor->mCallbacks[theEvent](*theConductor, theID);
                theConductor->mPerformance->FreePMsg((DMUS_PMSG*)theMessage);
                theConductor->mLock->Leave();
            }
            break;
        case WAIT_OBJECT_0 + 1:
            theConductor->mEventHandlerThreadDone->Set();
            return 0;
        case WAIT_FAILED:
            GetLastError();
            break;
        }
    }
}

// FUNCTION: LLE 0x1004cb52
// Returns the ID of the score whose segment is playing now (-1: none or not running)
long SEConductor::GetCurrentlyPlayingScore()
{
    long theID = -1;
    unsigned long theCount = this->mScores.size();
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        MUSIC_TIME theTime;
        IDirectMusicSegmentState* theSegmentState;
        if (SUCCEEDED(this->mPerformance->GetTime(NULL, &theTime)) &&
            SUCCEEDED(this->mPerformance->GetSegmentState(&theSegmentState, theTime))) {
            IDirectMusicSegment* theSegment;
            if (SUCCEEDED(theSegmentState->GetSegment(&theSegment))) {
                for (unsigned long i = 0; i < theCount && theID == -1; i++) {
                    // Fixed: a score whose load failed (no score object) used to be dereferenced
                    if (this->mScores[i].mScore && this->mScores[i].mScore->mSegment == theSegment)
                        theID = this->mScores[i].mID;
                }
                // Fixed: the reference GetSegment adds was never released
                theSegment->Release();
            }
            theSegmentState->Release();
        }
    }
    this->mLock->Leave();
    return theID;
}

// FUNCTION: LLE 0x1004cc00
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

// FUNCTION: LLE 0x1004cc5b
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

// FUNCTION: LLE 0x1004ccbc
// Starts DirectMusic (loader, composer, performance, port, notifications), reapplies volume, reverb, intensity and
// folder, reloads the remembered scores and starts the notification thread; returns 1 and releases all on failure
long SEConductor::InitializeConductor()
{
    long theResult;
    IDirectSound* theDirectSound = this->mMixer->GetDirectSoundInterface();
    HRESULT theError;
    IDirectMusicPerformance* thePerformance = NULL;
    IDirectMusicComposer* theComposer = NULL;
    IDirectMusicLoader* theLoader = NULL;
    IDirectMusicPort* thePort = NULL;
    IDirectMusic* theDirectMusic = NULL;
    BOOL theAutoDownload = FALSE;
    this->mLock->Enter();
    theResult = CoCreateInstance(CLSID_DirectMusicLoader, NULL, CLSCTX_INPROC, IID_IDirectMusicLoader,
                                 (void**)&theLoader);
    if (SUCCEEDED(theResult)) {
        theError = CoCreateInstance(CLSID_DirectMusicComposer, NULL, CLSCTX_INPROC, IID_IDirectMusicComposer,
                                    (void**)&theComposer);
        if (SUCCEEDED(theError)) {
            theResult = CoCreateInstance(CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC,
                                         IID_IDirectMusicPerformance2, (void**)&thePerformance);
            if (SUCCEEDED(theResult)) {
                theResult = thePerformance->Init(&theDirectMusic, theDirectSound, NULL);
                if (SUCCEEDED(theResult)) {
                    theResult = this->InitializePorts(thePerformance, theDirectMusic, thePort);
                    if (theResult == kErrorOK) {
                        if (SUCCEEDED(theError = thePerformance->SetNotificationHandle(this->mNotificationEvent->mHandle, 0)) &&
                            SUCCEEDED(theError = thePerformance->AddNotificationType(GUID_NOTIFICATION_PERFORMANCE)) &&
                            SUCCEEDED(theError = thePerformance->AddNotificationType(GUID_NOTIFICATION_SEGMENT))) {
                            thePerformance->SetGlobalParam(GUID_PerfAutoDownload, &theAutoDownload, sizeof(BOOL));
                            theLoader->EnableCache(GUID_DirectMusicAllTypes, TRUE);
                            this->mComposer = theComposer;
                            this->mPerformance = thePerformance;
                            this->mLoader = theLoader;
                            this->mDirectMusic = theDirectMusic;
                            this->mPort = thePort;
                            this->mConductorState = 1;
                            this->SetVolume(this->mVolume);
                            this->SetReverb(this->mReverbState);
                            this->SetIntensity(this->mIntensity, true);
                            if (this->mMusicFolder.GetLength() > 0)
                                theResult = this->SetDirectMusicFolder(this->mMusicFolder);
                            for (unsigned long i = 0; i < this->mScores.size() && theResult == kErrorOK; i++) {
                                long theID;
                                theResult = this->LoadScore(this->mScores[i].mName, theID, true);
                            }
                            if (theResult == kErrorOK)
                                ESVERIFY_AT(TSScheduler::CreateThread(this->mEventHandler, SEConductor::EventHandler, this) == keseOK, 485);
                        } else {
                            thePort->Release();
                            theDirectMusic->Release();
                            theLoader->Release();
                            thePerformance->Release();
                            theComposer->Release();
                            ESRecordNativeError(theError);
                            theResult = 1;
                        }
                    } else {
                        theDirectMusic->Release();
                        theLoader->Release();
                        thePerformance->Release();
                        theComposer->Release();
                    }
                } else {
                    thePerformance->Release();
                    theComposer->Release();
                    theLoader->Release();
                    ESRecordNativeError(theResult);
                    theResult = 1;
                }
            } else {
                theComposer->Release();
                theLoader->Release();
                ESRecordNativeError(theResult);
                theResult = 1;
            }
        } else {
            theLoader->Release();
            ESRecordNativeError(theError);
            theResult = 1;
        }
    } else {
        ESRecordNativeError(theResult);
        theResult = 1;
    }
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004cf65
// Creates and activates a DLS output port (the default, else the first enumerated one) with the channel-group count
// capped by the port, adds it to the performance; returns 1 on failure (also when there is no DLS output port)
long SEConductor::InitializePorts(IDirectMusicPerformance* inPerformance, IDirectMusic* inDirectMusic,
                                  IDirectMusicPort*& outPort)
{
    long theResult = kErrorOK;
    HRESULT theError;
    IDirectMusicPort* thePort;
    DWORD i;
    this->mLock->Enter();
    GUID theGUID;
    theError = inDirectMusic->GetDefaultPort(&theGUID);
    if (SUCCEEDED(theError)) {
        DMUS_PORTPARAMS theParams;
        memset(&theParams, 0, sizeof(theParams));
        theParams.dwSize = sizeof(theParams);
        theParams.dwChannelGroups = 1;
        theParams.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS;
        theError = inDirectMusic->CreatePort(theGUID, &theParams, &thePort, NULL);
        if (SUCCEEDED(theError)) {
            DMUS_PORTCAPS theCaps;
            memset(&theCaps, 0, sizeof(theCaps));
            theCaps.dwSize = sizeof(theCaps);
            theError = thePort->GetCaps(&theCaps);
            if (SUCCEEDED(theError)) {
                if (theCaps.dwClass != DMUS_PC_OUTPUTCLASS || (theCaps.dwFlags & DMUS_PC_DLS) != DMUS_PC_DLS) {
                    thePort->Release();
                    thePort = NULL;
                }
                if (thePort == NULL) {
                    i = 0;
                    do {
                        memset(&theCaps, 0, sizeof(theCaps));
                        theCaps.dwSize = sizeof(theCaps);
                        theError = inDirectMusic->EnumPort(i, &theCaps);
                        if (SUCCEEDED(theError) && theCaps.dwClass == DMUS_PC_OUTPUTCLASS &&
                            (theCaps.dwFlags & DMUS_PC_DLS) == DMUS_PC_DLS) {
                            memcpy(&theGUID, &theCaps.guidPort, sizeof(GUID));
                            memset(&theParams, 0, sizeof(theParams));
                            theParams.dwSize = sizeof(theParams);
                            theParams.dwChannelGroups = 1;
                            theParams.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS;
                            if (FAILED(inDirectMusic->CreatePort(theGUID, &theParams, &thePort, NULL)))
                                thePort = NULL;
                            break;
                        }
                        i++;
                        // Fixed: looped while SUCCEEDED, but EnumPort ends the list with S_FALSE (a success code),
                        // so the loop never ended when no port was a DLS output port
                    } while (theError == S_OK);
                }
                if (thePort == NULL) {
                    // Fixed: with no DLS output port the NULL port used to be dereferenced below
                    this->mLock->Leave();
                    return 1;
                }
                if (this->mChannelGroupCount > theCaps.dwMaxChannelGroups)
                    this->mChannelGroupCount = theCaps.dwMaxChannelGroups;
                if (SUCCEEDED(theError = thePort->SetNumChannelGroups(this->mChannelGroupCount)) &&
                    SUCCEEDED(theError = thePort->Activate(TRUE)) &&
                    SUCCEEDED(theError = inPerformance->AddPort(thePort))) {
                    for (i = 0; i < this->mChannelGroupCount && theResult == kErrorOK; i++) {
                        theError = inPerformance->AssignPChannelBlock(i, thePort, i + 1);
                        if (FAILED(theError)) {
                            ESRecordNativeError(theError);
                            theResult = 1;
                        }
                    }
                    HRESULT theActivateError = inDirectMusic->Activate(TRUE);
                    if (SUCCEEDED(theActivateError)) {
                        if (theResult == kErrorOK)
                            outPort = thePort;
                        else {
                            thePort->Release();
                            ESGetNativeError();
                        }
                    } else {
                        thePort->Release();
                        ESRecordNativeError(theActivateError);
                        theResult = 1;
                    }
                } else {
                    thePort->Release();
                    ESRecordNativeError(theError);
                    theResult = 1;
                }
            } else {
                thePort->Release();
                ESRecordNativeError(theError);
                theResult = 1;
            }
        } else {
            ESRecordNativeError(theError);
            theResult = 1;
        }
    } else {
        ESRecordNativeError(theError);
        theResult = 1;
    }
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d1d5
// Loads a music score by file name and returns its ID; a name already loaded only bumps its load count; while not
// running only the name is remembered (inSecondary reloads an entry at start-up)
long SEConductor::LoadScore(const UWideString& inName, long& outID, bool inSecondary)
{
    SEConductorScore theScore;
    bool theFound = false;
    IDirectMusicSegment* theSegment = NULL;
    long theResult = kErrorOK;
    unsigned long i;
    this->mLock->Enter();
    for (i = 0; i < this->mScores.size() && !theFound; i++)
        theFound = this->mScores[i].mName == inName;
    if (theFound && !inSecondary) {
        ++this->mScores[i - 1].mLoadCount;
        outID = this->mScores[i - 1].mID;
        theResult = kErrorOK;
    } else if (this->mConductorState == 1) {
        WCHAR theFileName[MAX_PATH];
        DMUS_OBJECTDESC theDescription;
        mbstowcs(theFileName, inName, MAX_PATH);
        memset(&theDescription, 0, sizeof(theDescription));
        theDescription.guidClass = CLSID_DirectMusicSegment;
        theDescription.dwSize = sizeof(theDescription);
        wcscpy(theDescription.wszFileName, theFileName);
        theDescription.dwValidData = DMUS_OBJ_CLASS | DMUS_OBJ_FILENAME;
        HRESULT theError = this->mLoader->GetObject(&theDescription, IID_IDirectMusicSegment2, (void**)&theSegment);
        if (FAILED(theError) && theError != DMUS_S_PARTIALLOAD) {
            ESRecordNativeError(theError);
            theResult = 1;
        } else if (FAILED(theError = theSegment->SetParam(GUID_Download, -1, 0, 0, this->mPerformance))) {
            // Fixed: a failed download used to leak the segment and report success with outID unset
            theSegment->Release();
            ESRecordNativeError(theError);
            theResult = 1;
        } else {
            SEScore* theNewScore = new SEScore(theSegment, false);
            if (inSecondary) {
                this->mScores[i - 1].mScore = theNewScore;
                outID = this->mScores[i - 1].mID;
                theResult = kErrorOK;
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
                theResult = kErrorOK;
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
        theResult = kErrorOK;
        outID = theScore.mID;
    }
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d42b
// Plays the score with the given ID (as a secondary segment when it is one); returns 1 on a DirectMusic error
long SEConductor::Play(long inID)
{
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    long theResult = kErrorOK;
    if (this->mConductorState == 1 && theScore) {
        HRESULT theError;
        if (!theScore->mIsSecondaryScore)
            theError = this->mPerformance->PlaySegment(theScore->GetSegment(), DMUS_SEGF_DEFAULT, 0, NULL);
        else
            theError = this->mPerformance->PlaySegment(theScore->GetSegment(), DMUS_SEGF_SECONDARY | DMUS_SEGF_DEFAULT, 0,
                                                 NULL);
        if (FAILED(theError)) {
            ESRecordNativeError(theError);
            theResult = 1;
        }
    }
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d48c
// Queues the score to play after the current one; secondary scores cannot be queued (fails); OK when not running
long SEConductor::Queue(long inID)
{
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    long theResult = 1;
    if (this->mConductorState == theResult && theScore) {
        if (!theScore->mIsSecondaryScore) {
            HRESULT theError = this->mPerformance->PlaySegment(theScore->GetSegment(), DMUS_SEGF_QUEUE | DMUS_SEGF_DEFAULT,
                                                         0, NULL);
            if (FAILED(theError)) {
                ESRecordNativeError(theError);
                goto done;
            }
        } else {
            theResult = kErrorFailed;
            goto done;
        }
    }
    theResult = kErrorOK;
done:
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d4f0
// Sets the channel-group count (at most 1000) used at the next start; busy while the conductor is running
long SEConductor::SetChannelGroupCount(unsigned long inCount)
{
    this->mLock->Enter();
    long theResult = kErrorOK;
    if (this->mConductorState == 0)
    {
        if (inCount < 1000)
            this->mChannelGroupCount = inCount;
        else
            this->mChannelGroupCount = 1000;
    }
    else
        theResult = kErrorBusy;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d532
// Starts (non-zero) or shuts down (0) DirectMusic when the requested state differs from the current one
long SEConductor::SetConductorState(unsigned long inState)
{
    long theResult;
    this->mLock->Enter();
    if (inState != this->mConductorState) {
        if (inState == 0)
            theResult = this->UninitializeConductor();
        else
            theResult = this->InitializeConductor();
    } else
        theResult = kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d56e
// Sets DirectMusic's search folder for all its file types while running; returns 1 on failure
long SEConductor::SetDirectMusicFolder(const UWideString& inFolder)
{
    long theResult;
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        WCHAR thePath[MAX_PATH];
        mbstowcs(thePath, inFolder, MAX_PATH);
        theResult = kErrorOK;
        HRESULT theError = this->mLoader->SetSearchDirectory(GUID_DirectMusicAllTypes, thePath, FALSE);
        if (FAILED(theError)) {
            ESRecordNativeError(theError);
            theResult = 1;
        }
    } else
        theResult = kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d5de
// Sets the music intensity (DirectMusic master groove level); while not running the intensity is reset to 0
long SEConductor::SetIntensity(unsigned long inIntensity, bool inImmediate)
{
    long theResult;
    char theLevel;
    this->mLock->Enter();
    if (inIntensity != this->mIntensity || inImmediate) {
        theResult = 1;
        if (this->mConductorState == theResult) {
            this->mIntensity = inIntensity;
            theLevel = (char)inIntensity;
            HRESULT theError = this->mPerformance->SetGlobalParam(GUID_PerfMasterGrooveLevel, &theLevel, 1);
            if (FAILED(theError)) {
                ESRecordNativeError(theError);
                goto done;
            }
        } else
            this->mIntensity = 0;
    }
    theResult = kErrorOK;
done:
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d643
// Sets the music folder, dropping the given name's last character (the trailing separator); kept only on success
long SEConductor::SetMusicFolder(const UWideString& inFolder)
{
    UWideString theFolder(0x104);
    long theResult;
    this->mLock->Enter();
    this->mMusicFolder = "";
    theFolder.CopyString(inFolder, inFolder.GetLength() - 1);
    if (this->mConductorState == 1)
        theResult = this->SetDirectMusicFolder(theFolder);
    else
        theResult = kErrorOK;
    if (theResult == kErrorOK)
        this->mMusicFolder = theFolder;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d6c2
// Turns the port's reverb effect on (1) or off; ignored while not running
long SEConductor::SetReverb(unsigned long inReverb)
{
    IKsControl* theControl = NULL;
    DWORD theEffects = 0;
    this->mLock->Enter();
    if (this->mConductorState == 1 && SUCCEEDED(this->mPort->QueryInterface(IID_IKsControl, (void**)&theControl))) {
        KSPROPERTY theProperty;
        ULONG theBytes;
        memset(&theProperty, 0, sizeof(theProperty));
        theProperty.Set = GUID_DMUS_PROP_Effects;
        theProperty.Id = 0;
        theProperty.Flags = KSPROPERTY_TYPE_GET;
        theControl->KsProperty(&theProperty, sizeof(theProperty), &theEffects, sizeof(theEffects), &theBytes);
        if (inReverb == 1)
            theEffects |= DMUS_EFFECT_REVERB;
        else
            theEffects &= ~DMUS_EFFECT_REVERB;
        memset(&theProperty, 0, sizeof(theProperty));
        theProperty.Set = GUID_DMUS_PROP_Effects;
        theProperty.Id = 0;
        theProperty.Flags = KSPROPERTY_TYPE_SET;
        theControl->KsProperty(&theProperty, sizeof(theProperty), &theEffects, sizeof(theEffects), &theBytes);
        theControl->Release();
    }
    this->mLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1004d7ab
// Sets the Waves reverb parameters (gain, high-frequency ratio, mix, time) on the port while running
long SEConductor::SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime)
{
    IKsControl* theControl = NULL;
    DMUS_WAVES_REVERB_PARAMS theParams;
    ULONG theBytes;
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        theParams.fInGain = inGain;
        theParams.fHighFreqRTRatio = inHighFreqRTRatio;
        theParams.fReverbMix = inMix;
        theParams.fReverbTime = inTime;
        if (SUCCEEDED(this->mPort->QueryInterface(IID_IKsControl, (void**)&theControl))) {
            KSPROPERTY theProperty;
            memset(&theProperty, 0, sizeof(theProperty));
            theProperty.Set = GUID_DMUS_PROP_WavesReverb;
            theProperty.Id = 0;
            theProperty.Flags = KSPROPERTY_TYPE_SET;
            theControl->KsProperty(&theProperty, sizeof(theProperty), &theParams, sizeof(theParams), &theBytes);
            theControl->Release();
        }
    }
    this->mLock->Leave();
    return kErrorOK;
}

// DirectMusic volume in hundredths of a decibel: 1000 * ln(volume) (a named constant: the multiply reads a float
// operand, where a literal would become a double)
const float kVolumeScale = 1000.0f;

// FUNCTION: LLE 0x1004d84f
// Sets the music volume (0..1; quieter than 0.01 is silent); applied to the port at once while running
long SEConductor::SetVolume(float inVolume)
{
    IKsControl* theControl = NULL;
    long theVolume;
    ULONG theBytes;
    this->mLock->Enter();
    this->mVolume = inVolume;
    if (this->mConductorState == 1) {
        if (inVolume > 0.01f)
            theVolume = UMFloatToLong(log(inVolume) * kVolumeScale);
        else
            theVolume = -20000;
        if (SUCCEEDED(this->mPort->QueryInterface(IID_IKsControl, (void**)&theControl))) {
            KSPROPERTY theProperty;
            memset(&theProperty, 0, sizeof(theProperty));
            theProperty.Set = GUID_DMUS_PROP_Volume;
            theProperty.Id = 0;
            theProperty.Flags = KSPROPERTY_TYPE_SET;
            theControl->KsProperty(&theProperty, sizeof(theProperty), &theVolume, sizeof(theVolume), &theBytes);
            theControl->Release();
        }
    }
    this->mLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1004d920
// Stops all music immediately while running; returns 1 on a DirectMusic error
long SEConductor::Stop()
{
    long theResult;
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        theResult = kErrorOK;
        HRESULT theError = this->mPerformance->Stop(NULL, NULL, 0, 0);
        if (FAILED(theError)) {
            ESRecordNativeError(theError);
            theResult = 1;
        }
    } else
        theResult = kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004d960
// Composes an automatic transition (fill, break, intro, end, end-and-intro, else groove) to the given score
// (NULL: none) on a measure, long or modulating boundary; returns 1 on a DirectMusic error
long SEConductor::TransitionTo(long inID, unsigned long inCommand, unsigned long inFlags)
{
    DWORD theCommand;
    DWORD theFlags;
    long theResult;
    HRESULT theError;
    this->mLock->Enter();
    SEScore* theScore = this->GetScore(inID);
    if (this->mConductorState == 1) {
        switch (inCommand) {
        case kMusicTransitionFill:
            theCommand = DMUS_COMMANDT_FILL;
            break;
        case kMusicTransitionBreak:
            theCommand = DMUS_COMMANDT_BREAK;
            break;
        case kMusicTransitionIntro:
            theCommand = DMUS_COMMANDT_INTRO;
            break;
        case kMusicTransitionEnd:
            theCommand = DMUS_COMMANDT_END;
            break;
        case kMusicTransitionEndAndIntro:
            theCommand = DMUS_COMMANDT_ENDANDINTRO;
            break;
        default:
            theCommand = DMUS_COMMANDT_GROOVE;
            break;
        }
        switch (inFlags) {
        case kTransitionFlagModulate:
            theFlags = DMUS_COMPOSEF_MODULATE;
            break;
        case kTransitionFlagLong:
            theFlags = DMUS_COMPOSEF_LONG;
            break;
        default:
            theFlags = DMUS_COMPOSEF_MEASURE;
            break;
        }
        if (theScore)
            theError = this->mComposer->AutoTransition(this->mPerformance, theScore->mSegment, (WORD)theCommand, theFlags, NULL,
                                                 NULL, NULL, NULL);
        else
            theError = this->mComposer->AutoTransition(this->mPerformance, NULL, (WORD)theCommand, theFlags, NULL, NULL, NULL,
                                                 NULL);
        if (FAILED(theError)) {
            ESRecordNativeError(theError);
            theResult = 1;
        } else
            theResult = kErrorOK;
    } else
        theResult = kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1004da37
// Stops the notification thread and, if running, the music: deletes every loaded score (keeping the names) and
// releases all DirectMusic objects
long SEConductor::UninitializeConductor()
{
    this->mEventHandlerExit->Set();
    this->mEventHandlerThreadDone->Wait(INFINITE);
    this->mLock->Enter();
    if (this->mConductorState == 1) {
        this->mPerformance->SetNotificationHandle(NULL, 0);
        TSScheduler::DestroyThread(this->mEventHandler);
        this->mPerformance->Stop(NULL, NULL, 0, 0);
        for (unsigned long i = 0; i < this->mScores.size(); i++) {
            if (this->mScores[i].mScore)
                delete this->mScores[i].mScore;
            this->mScores[i].mScore = NULL;
        }
        this->mPerformance->CloseDown();
        if (this->mPort)
            this->mPort->Release();
        if (this->mDirectMusic)
            this->mDirectMusic->Release();
        if (this->mLoader)
            this->mLoader->Release();
        if (this->mPerformance)
            this->mPerformance->Release();
        if (this->mComposer)
            this->mComposer->Release();
        this->mPort = NULL;
        this->mDirectMusic = NULL;
        this->mLoader = NULL;
        this->mPerformance = NULL;
        this->mComposer = NULL;
        this->mConductorState = 0;
    }
    this->mLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x1004db25
// Unloads one load of the score with the given ID, deleting it at the last; busy while it is playing
long SEConductor::UnloadScore(long inID)
{
    long theResult;
    this->mLock->Enter();
    bool theFound = false;
    unsigned long i;
    for (i = 0; i < this->mScores.size(); i++) {
        if (this->mScores[i].mID == inID) {
            theFound = true;
            break;
        }
    }
    if (theFound) {
        SEScore* theScore = this->mScores[i].mScore;
        if (inID == -1)
            theResult = kErrorNotFound;
        else if (inID == this->GetCurrentlyPlayingScore())
            theResult = kErrorBusy;
        else {
            std::vector<SEConductorScore>::iterator theEntry(&this->mScores[i]);
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

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1004de55
// TEMPLATE: LLE 0x1004e69a
// TEMPLATE: LLE 0x1004e6c5
// TEMPLATE: LLE 0x1004e705
// TEMPLATE: LLE 0x1004e75e
// TEMPLATE: LLE 0x1004e786
// TEMPLATE: LLE 0x1004e7aa
// TEMPLATE: LLE 0x1004e7c7
// SYNTHETIC: LLE 0x1004e7f3
// SYNTHETIC: LLE 0x1004bbce
// SYNTHETIC: LLE 0x1004bbd2
// SYNTHETIC: LLE 0x1004bbd6
// SYNTHETIC: LLE 0x1004bbda
// SYNTHETIC: LLE 0x1004bbde
// SYNTHETIC: LLE 0x1004bbf0
// SYNTHETIC: LLE 0x1004bbff
// SYNTHETIC: LLE 0x1004bc09
// SYNTHETIC: LLE 0x1004bcb1
// SYNTHETIC: LLE 0x1004bdbc
// SYNTHETIC: LLE 0x1004bdc2
// SYNTHETIC: LLE 0x1004bdda
// SYNTHETIC: LLE 0x1004bde0
// SYNTHETIC: LLE 0x1004bde6
// SYNTHETIC: LLE 0x1004bdee
// SYNTHETIC: LLE 0x1004bdf7
// SYNTHETIC: LLE 0x1004be0f
// SYNTHETIC: LLE 0x1004be15
// SYNTHETIC: LLE 0x1004be28
// SYNTHETIC: LLE 0x1004be2e
// SYNTHETIC: LLE 0x1004be34
// SYNTHETIC: LLE 0x1004be42
// SYNTHETIC: LLE 0x1004be55
// SYNTHETIC: LLE 0x1004be5b
// SYNTHETIC: LLE 0x1004be61
// SYNTHETIC: LLE 0x1004be67
// SYNTHETIC: LLE 0x1004be7f
// SYNTHETIC: LLE 0x1004be8d
// SYNTHETIC: LLE 0x1004be93
// SYNTHETIC: LLE 0x1004bea1
// SYNTHETIC: LLE 0x1004beaf
// SYNTHETIC: LLE 0x1004bec2
// SYNTHETIC: LLE 0x1004bec8
// SYNTHETIC: LLE 0x1004bed6
// SYNTHETIC: LLE 0x1004bee9
// SYNTHETIC: LLE 0x1004bef7
// SYNTHETIC: LLE 0x1004bf0a
// SYNTHETIC: LLE 0x1004bf1d
// SYNTHETIC: LLE 0x1004bf34
// SYNTHETIC: LLE 0x1004bf4b
// SYNTHETIC: LLE 0x1004bf62
// SYNTHETIC: LLE 0x1004bf74
// SYNTHETIC: LLE 0x1004bf7a
// SYNTHETIC: LLE 0x1004bf88
// SYNTHETIC: LLE 0x1004c111
// SYNTHETIC: LLE 0x1004c29a
// SYNTHETIC: LLE 0x1004c2d7
// SYNTHETIC: LLE 0x1004c32a
// SYNTHETIC: LLE 0x1004c341
// SYNTHETIC: LLE 0x1004c345
// SYNTHETIC: LLE 0x1004c349
// SYNTHETIC: LLE 0x1004c34d
// SYNTHETIC: LLE 0x1004c351
// SYNTHETIC: LLE 0x1004c355
// SYNTHETIC: LLE 0x1004c359
// SYNTHETIC: LLE 0x1004c365
// SYNTHETIC: LLE 0x1004c369
// SYNTHETIC: LLE 0x1004c36d
// SYNTHETIC: LLE 0x1004c380
// SYNTHETIC: LLE 0x1004c393
// SYNTHETIC: LLE 0x1004c3a6
// SYNTHETIC: LLE 0x1004c3bd
// SYNTHETIC: LLE 0x1004c3d4
// SYNTHETIC: LLE 0x1004c3eb
// SYNTHETIC: LLE 0x1004c3f5
// SYNTHETIC: LLE 0x1004c3ff
// SYNTHETIC: LLE 0x1004c409
// SYNTHETIC: LLE 0x1004c413
// SYNTHETIC: LLE 0x1004c41d
// SYNTHETIC: LLE 0x1004c427
// SYNTHETIC: LLE 0x1004c47e
// SYNTHETIC: LLE 0x1004c490
// SYNTHETIC: LLE 0x1004c4a0
// SYNTHETIC: LLE 0x1004c500
// SYNTHETIC: LLE 0x1004c507
// SYNTHETIC: LLE 0x1004c510
// SYNTHETIC: LLE 0x1004c519
// SYNTHETIC: LLE 0x1004c522
// SYNTHETIC: LLE 0x1004c526
// SYNTHETIC: LLE 0x1004c52f
// SYNTHETIC: LLE 0x1004c53c
// SYNTHETIC: LLE 0x1004c54d
// SYNTHETIC: LLE 0x1004c62c
// SYNTHETIC: LLE 0x1004c6ff
// SYNTHETIC: LLE 0x1004c703
// SYNTHETIC: LLE 0x1004c712
// SYNTHETIC: LLE 0x1004c725
// SYNTHETIC: LLE 0x1004c75a
// SYNTHETIC: LLE 0x1004c787
// TEMPLATE: LLE 0x1004dbcf
// TEMPLATE: LLE 0x1004dc16
// TEMPLATE: LLE 0x1004dd68
// TEMPLATE: LLE 0x1004dd7b
// TEMPLATE: LLE 0x1004dda7
// TEMPLATE: LLE 0x1004de1a
// TEMPLATE: LLE 0x1004de67
// TEMPLATE: LLE 0x1004deb1
// TEMPLATE: LLE 0x1004e019
// TEMPLATE: LLE 0x1004e04d
// TEMPLATE: LLE 0x1004e09d
// TEMPLATE: LLE 0x1004e106
// TEMPLATE: LLE 0x1004e13f
// TEMPLATE: LLE 0x1004e177
// TEMPLATE: LLE 0x1004e1b1
// TEMPLATE: LLE 0x1004e1c7
// TEMPLATE: LLE 0x1004e201
// TEMPLATE: LLE 0x1004e239
// TEMPLATE: LLE 0x1004e2aa
// TEMPLATE: LLE 0x1004e30a
// TEMPLATE: LLE 0x1004e5bc
// TEMPLATE: LLE 0x1004e641
// TEMPLATE: LLE 0x1004e673
// TEMPLATE: LLE 0x1004e6d9
// TEMPLATE: LLE 0x1004e72e
// TEMPLATE: LLE 0x1004e805
// TEMPLATE: LLE 0x1004e82c
// TEMPLATE: LLE 0x1004e84d
// TEMPLATE: LLE 0x1004e869
// TEMPLATE: LLE 0x1004e885
// SYNTHETIC: LLE 0x1004bd5b
// SYNTHETIC: LLE 0x1004c4a5
// SYNTHETIC: LLE 0x1004c7ae
