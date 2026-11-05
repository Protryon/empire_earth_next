// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/SEMixer.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

std::vector<SEMixer*> SEMixer::sMixers;
SEMixer* SEMixer::sActiveMixer;

// FUNCTION: LLE 0x1004e8f3
// Creates a sound mixer from a mixer plug-in: registers it, looks up every SPIM entry point and initialises it
SEMixer::SEMixer(PIMModule* inModule, GEWindow* inWindow)
{
    this->mModule = inModule;
    SEMixer::sMixers.push_back(this);
    this->mActivateMixer = (long (*)())inModule->GetEntry("SPIMActivateMixer");
    this->mCreateChannels = (long (*)(unsigned long, SEChannelDescription*))inModule->GetEntry("SPIMCreateChannels");
    this->mDeactivateMixer = (long (*)())inModule->GetEntry("SPIMDeactivateMixer");
    this->mDeleteWaveData = (void (*)(SESample*))inModule->GetEntry("SPIMDeleteWaveData");
    this->mDestroyChannels = (long (*)())inModule->GetEntry("SPIMDestroyChannels");
    this->mDuplicateSample = (long (*)(const SESample*, SESample**))inModule->GetEntry("SPIMDuplicateSample");
    this->mGetChannelCount = (unsigned long (*)())inModule->GetEntry("SPIMGetChannelCount");
    this->mGetCPUUsage = (float (*)())inModule->GetEntry("SPIMGetCPUUsage");
    this->mGetDirectSoundInterface = (IDirectSound* (*)())inModule->GetEntry("SPIMGetDirectSoundInterface");
    this->mGetHighestTimerDelay = (unsigned long (*)())inModule->GetEntry("SPIMGetHighestTimerDelay");
    this->mGetMaximumChannelCount = (unsigned long (*)())inModule->GetEntry("SPIMGetMaximumChannelCount");
    this->mGetSamplePlayBackTime = (long (*)(SESample*, long*, long*))inModule->GetEntry("SPIMGetSamplePlayBackTime");
    this->mGetSampleOffset = (unsigned long (*)(SESample*))inModule->GetEntry("SPIMGetSampleOffset");
    this->mGetVolume = (float (*)())inModule->GetEntry("SPIMGetVolume");
    this->mInitializeMixer = (void (*)(GEWindow*))inModule->GetEntry("SPIMInitializeMixer");
    this->mIsChannelBusyPlaying = (long (*)(unsigned long))inModule->GetEntry("SPIMIsChannelBusyPlaying");
    this->mLoadWaveData = (long (*)(FSFileSpec*, SESample*))inModule->GetEntry("SPIMLoadWaveData");
    this->mPlayChannel = (long (*)(unsigned long))inModule->GetEntry("SPIMPlayChannel");
    this->mProcessMessage = (bool (*)(unsigned long, void*, void*))inModule->GetEntry("SPIMProcessMessage");
    this->mSetChannelSample = (long (*)(unsigned long, SESample*))inModule->GetEntry("SPIMSetChannelSample");
    this->mMixAhead = (long (*)())inModule->GetEntry("SPIMMixAhead");
    this->mSetLatency = (long (*)(unsigned long))inModule->GetEntry("SPIMSetLatency");
    this->mSetSampleOffset = (long (*)(SESample*, unsigned long))inModule->GetEntry("SPIMSetSampleOffset");
    this->mSetSampleLooping = (void (*)(SESample*, bool))inModule->GetEntry("SPIMSetSampleLooping");
    this->mSetSamplePan = (long (*)(SESample*, float))inModule->GetEntry("SPIMSetSamplePan");
    this->mSetSamplePitch = (long (*)(SESample*, float))inModule->GetEntry("SPIMSetSamplePitch");
    this->mSetSampleVolume = (long (*)(SESample*, float))inModule->GetEntry("SPIMSetSampleVolume");
    this->mSetVolume = (long (*)(float))inModule->GetEntry("SPIMSetVolume");
    this->mStopAllChannels = (long (*)())inModule->GetEntry("SPIMStopAllChannels");
    this->mStopChannel = (long (*)(unsigned long))inModule->GetEntry("SPIMStopChannel");
    this->mGetOutputRate = (unsigned long (*)())inModule->GetEntry("SPIMGetOutputRate");
    this->mSetMusicSource = (void (*)(void (*)(void*, float*, unsigned long), void*))inModule->GetEntry(
        "SPIMSetMusicSource");
    this->InitializeMixer(inWindow);
    this->mConductor.SetMixer(this);
}

// FUNCTION: LLE 0x1004ed53
// Destroys the mixer: stops the music and deactivates it if active, unregisters it and unloads its plug-in
SEMixer::~SEMixer()
{
    if (this == SEMixer::sActiveMixer) {
        this->mConductor.SetConductorState(0);
        this->DeactivateMixer();
        SEMixer::sActiveMixer = NULL;
    }
    std::vector<SEMixer*>::iterator theMixer = std::find(SEMixer::sMixers.begin(), SEMixer::sMixers.end(), this);
    if (theMixer != SEMixer::sMixers.end())
        SEMixer::sMixers.erase(theMixer);
    this->mModule->Unload();
}

// FUNCTION: LLE 0x1004ede9
// Returns the registered mixer with the given index; kErrorBeyondEnd past the last
long SEMixer::GetMixer(unsigned long inIndex, SEMixer*& outMixer)
{
    if (inIndex < SEMixer::sMixers.size()) {
        outMixer = SEMixer::sMixers[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1004ee15
// Makes the given mixer the active one (NULL: none), stopping and deactivating the current one first; fails if
// either step fails
long SEMixer::UseMixer(SEMixer* inMixer)
{
    long theResult = kErrorOK;
    if (SEMixer::sActiveMixer) {
        if ((theResult = SEMixer::sActiveMixer->mConductor.SetConductorState(0)) == kErrorOK &&
            (theResult = SEMixer::sActiveMixer->DeactivateMixer()) == kErrorOK)
            SEMixer::sActiveMixer = NULL;
        else
            theResult = kErrorFailed;
    }
    if (theResult != kErrorOK || !inMixer)
        return theResult;
    if ((theResult = inMixer->ActivateMixer()) == kErrorOK)
        SEMixer::sActiveMixer = inMixer;
    else
        theResult = kErrorFailed;
    return theResult;
}

// Init chain and template COMDATs emitted in this unit (markers at the end):
// SYNTHETIC: LLE 0x1004e8aa
// SYNTHETIC: LLE 0x1004e8b4
// SYNTHETIC: LLE 0x1004e8cc
// SYNTHETIC: LLE 0x1004e8d8
// TEMPLATE: LLE 0x1004ee69
// TEMPLATE: LLE 0x1004ee95
