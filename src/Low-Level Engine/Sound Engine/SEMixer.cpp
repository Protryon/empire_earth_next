// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The registry of mixers and the decisions (which mixer is active, how a change stops, deactivates and activates,
// the destructor's, the start's choice of plug-ins and the shutdown's order) are Rust: rust/crates/ee_sound
// mixers.rs (docs/RUST_PLAN.md R3.6). This file keeps the plug-in's entry points, the conductor, the active mixer
// pointer the renderers read, and the trampolines ee_mixerscpp_* through which the Rust reaches them.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/SEMixer.h"
#include "rust/ee_rust.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

SEMixer* SEMixer::sActiveMixer;

// The C++ side of the Rust decisions
struct SEMixer::Rust {
    static SEMixer* Of(void* inMixer) { return static_cast<SEMixer*>(inMixer); }
    static long StopMusic(void* inMixer) { return Of(inMixer)->mConductor.SetConductorState(0); }
    static long Activate(void* inMixer) { return Of(inMixer)->ActivateMixer(); }
    static long Deactivate(void* inMixer) { return Of(inMixer)->DeactivateMixer(); }
    static void* Create(SEMixerStart* inStart) { return new SEMixer(inStart->mModule, inStart->mWindow); }
    static void Delete(void* inMixer) { delete Of(inMixer); }
    static void* Active() { return SEMixer::sActiveMixer; }
    static void SetActive(void* inMixer) { SEMixer::sActiveMixer = Of(inMixer); }
};

extern "C" {
void* ee_mixerscpp_active() { return SEMixer::Rust::Active(); }
void ee_mixerscpp_set_active(void* inMixer) { SEMixer::Rust::SetActive(inMixer); }
int32_t ee_mixerscpp_stop_music(void* inMixer) { return (int32_t)SEMixer::Rust::StopMusic(inMixer); }
int32_t ee_mixerscpp_activate(void* inMixer) { return (int32_t)SEMixer::Rust::Activate(inMixer); }
int32_t ee_mixerscpp_deactivate(void* inMixer) { return (int32_t)SEMixer::Rust::Deactivate(inMixer); }
int32_t ee_mixerscpp_first_module(void* inStart)
{
    SEMixerStart* theStart = static_cast<SEMixerStart*>(inStart);
    return (int32_t)PIMModule::GetFirstModule(*theStart->mDirectory, ' snd', theStart->mModule);
}
int32_t ee_mixerscpp_next_module(void* inStart)
{
    return (int32_t)PIMModule::GetNextModule(static_cast<SEMixerStart*>(inStart)->mModule);
}
void* ee_mixerscpp_create_mixer(void* inStart) { return SEMixer::Rust::Create(static_cast<SEMixerStart*>(inStart)); }
bool ee_mixerscpp_is_valid(void* inMixer) { return SEMixer::Rust::Of(inMixer)->IsValid(); }
void ee_mixerscpp_delete_mixer(void* inMixer) { SEMixer::Rust::Delete(inMixer); }
}

// FUNCTION: LLE 0x1004e8f3
// Creates a sound mixer from a mixer plug-in: registers it, looks up every SPIM entry point and initialises it
SEMixer::SEMixer(PIMModule* inModule, GEWindow* inWindow)
{
    this->mModule = inModule;
    ee_mixers_register(this);
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
    this->mSetMovieSource = (void (*)(void (*)(void*, float*, unsigned long), void*))inModule->GetEntry(
        "SPIMSetMovieSource");
    this->InitializeMixer(inWindow);
    this->mConductor.SetMixer(this);
}

// FUNCTION: LLE 0x1004ed53
// Destroys the mixer: stops the music and deactivates it if active, unregisters it and unloads its plug-in
SEMixer::~SEMixer()
{
    ee_mixers_destroy(this);
    this->mModule->Unload();
}

// Returns how many mixers are registered
unsigned long SEMixer::GetCount()
{
    return ee_mixers_count();
}

// FUNCTION: LLE 0x1004ede9
// Returns the registered mixer with the given index; kErrorBeyondEnd past the last
long SEMixer::GetMixer(unsigned long inIndex, SEMixer*& outMixer)
{
    void* theMixer = NULL;
    long theResult = ee_mixers_get((uint32_t)inIndex, &theMixer);
    if (theResult == kErrorOK)
        outMixer = static_cast<SEMixer*>(theMixer);
    return theResult;
}

// FUNCTION: LLE 0x1004ee15
// Makes the given mixer the active one (NULL: none), stopping and deactivating the current one first; fails if
// either step fails
long SEMixer::UseMixer(SEMixer* inMixer)
{
    return ee_mixers_use(inMixer);
}

// Init chain and template COMDATs emitted in this unit (markers at the end):
// SYNTHETIC: LLE 0x1004e8aa
// SYNTHETIC: LLE 0x1004e8b4
// SYNTHETIC: LLE 0x1004e8cc
// SYNTHETIC: LLE 0x1004e8d8
// TEMPLATE: LLE 0x1004ee69
// TEMPLATE: LLE 0x1004ee95
