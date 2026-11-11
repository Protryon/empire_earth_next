// C:\EEX\Low-Level Engine\Sound Engine\SEMixer.h (RECONSTRUCTION; SEMixer.cpp 0x4e8aa..0x4eef6). A loaded mixer
// plug-in (SPIM): its entry points, resolved by name in the ctor, plus the conductor that plays scores through it.
// The inline forwarders are emitted first by SEConductor.cpp (0x4bdbc..0x4c29a). Requires lle/LowLevelEngine.h.
#ifndef LLE_SEMIXER_H
#define LLE_SEMIXER_H

#include <vector>
#include "lle/SEConductor.h"
#include "lle/PIMModule.h"

struct IDirectSound;
struct SEChannelDescription {               // filled by the plug-in's SPIMCreateChannels; sizeof 0xc (a local in
    unsigned long mUnknown[3];              // SESoundManager::SetMaxChannels 0x51610)
};
class FSFileSpec;
class GEWindow;
class SESample;

long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow);   // Sound Engine.cpp; (provisional names)
void SEShutdown(bool inForced);
// port: the start's state while SEInitializeMixers walks the plug-in table (the Rust decides, SEMixer.cpp's
// trampolines create the mixers)
struct SEMixerStart {
    const FSFileSpec* mDirectory;
    GEWindow* mWindow;
    PIMModule* mModule;                     // the module being walked
};

class LLE_API SEMixer {                     // proven sizeof 0x100
    friend long SEInitializeMixers(const FSFileSpec& inDirectory, GEWindow* inWindow);   // creates the mixers
    friend void SEShutdown(bool inForced);  // deletes them
    friend class SESample;
    friend class SESoundManager;
public:
// Inferred: the renderers (built against newer engine headers, LLE_CURRENT_HEADERS) see the inline accessor and
// the static: DX7HRDisplay.dll imports ?sActiveMixer@SEMixer@@0PAV1@A (DX7Movie::PlayBink 0x100026f8)
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    static SEMixer* GetActiveMixer() { return SEMixer::sActiveMixer; }
#else
    static SEMixer* GetActiveMixer();
#endif
    SEConductor& GetConductor() { return this->mConductor; }
    static unsigned long GetCount();        // (the registry is Rust's: rust/crates/ee_sound mixers.rs)
    float GetCPUUsage() { return (*this->mGetCPUUsage)(); }
    IDirectSound* GetDirectSoundInterface() { return (*this->mGetDirectSoundInterface)(); }
    static long GetMixer(unsigned long inIndex, SEMixer*& outMixer);
    const UString& GetName() { return this->mModule->GetName(); }
    bool IsValid() { return this->mModule->IsValid(); }
    bool ProcessMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
        { return (*this->mProcessMessage)(inMessage, inParameter1, inParameter2); }
    static long UseMixer(SEMixer* inMixer);
    // this port's additions: the mixer's output rate, and the music (the conductor) and movie sound sources its
    // device thread mixes in (interleaved stereo float frames added at the output rate)
    unsigned long GetOutputRate() const { return this->mGetOutputRate ? (*this->mGetOutputRate)() : 0; }
    void SetMusicSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
        { if (this->mSetMusicSource) (*this->mSetMusicSource)(inSource, inContext); }
    void SetMovieSource(void (*inSource)(void*, float*, unsigned long), void* inContext)
        { if (this->mSetMovieSource) (*this->mSetMovieSource)(inSource, inContext); }
    // the C++ side the Rust decisions reach (extern "C" ee_mixerscpp_*, SEMixer.cpp): a nested class, so that it
    // shares this class's access
    struct Rust;
private:
    long ActivateMixer() { return (*this->mActivateMixer)(); }
    long CreateChannels(unsigned long inCount, SEChannelDescription* outChannels)
        { return (*this->mCreateChannels)(inCount, outChannels); }
    long DeactivateMixer() { return (*this->mDeactivateMixer)(); }
    long DestroyChannels() { return (*this->mDestroyChannels)(); }
    void DeleteWaveData(SESample* inSample) { (*this->mDeleteWaveData)(inSample); }
    long DuplicateSample(const SESample& inSample, SESample*& outSample)
        { return (*this->mDuplicateSample)(&inSample, &outSample); }
    unsigned long GetChannelCount() const { return (*this->mGetChannelCount)(); }
    unsigned long GetHighestTimerDelay() const { return (*this->mGetHighestTimerDelay)(); }
    unsigned long GetMaximumChannelCount() const { return (*this->mGetMaximumChannelCount)(); }
    long GetSamplePlayBackTime(SESample* inSample, long& outTotal, long& outRemaining)
        { return (*this->mGetSamplePlayBackTime)(inSample, &outTotal, &outRemaining); }
    unsigned long GetSampleOffset(SESample* inSample) { return (*this->mGetSampleOffset)(inSample); }
    float GetVolume() const { return (*this->mGetVolume)(); }
    void InitializeMixer(GEWindow* inWindow) { (*this->mInitializeMixer)(inWindow); }
    long IsChannelBusyPlaying(unsigned long inChannel) { return (*this->mIsChannelBusyPlaying)(inChannel); }
    long LoadWaveData(FSFileSpec* inSpec, SESample* ioSample) { return (*this->mLoadWaveData)(inSpec, ioSample); }
    long MixAhead() { return (*this->mMixAhead)(); }
    long PlayChannel(unsigned long inChannel) { return (*this->mPlayChannel)(inChannel); }
    long SetChannelSample(unsigned long inChannel, SESample* inSample) { return (*this->mSetChannelSample)(inChannel, inSample); }
    long SetLatency(unsigned long inMilliseconds) { return (*this->mSetLatency)(inMilliseconds); }
    void SetSampleLooping(SESample* inSample, bool inLooping) { (*this->mSetSampleLooping)(inSample, inLooping); }
    long SetSampleOffset(SESample* inSample, unsigned long inOffset) { return (*this->mSetSampleOffset)(inSample, inOffset); }
    long SetSamplePan(SESample* inSample, float inPan) { return (*this->mSetSamplePan)(inSample, inPan); }
    long SetSamplePitch(SESample* inSample, float inPitch) { return (*this->mSetSamplePitch)(inSample, inPitch); }
    long SetSampleVolume(SESample* inSample, float inVolume) { return (*this->mSetSampleVolume)(inSample, inVolume); }
    long SetVolume(float inVolume) { return (*this->mSetVolume)(inVolume); }
    long StopAllChannels() { return (*this->mStopAllChannels)(); }
    long StopChannel(unsigned long inChannel) { return (*this->mStopChannel)(inChannel); }

    SEMixer(PIMModule* inModule, GEWindow* inWindow);
    ~SEMixer();

    SEConductor mConductor;                 // +0x00
    PIMModule* mModule;                     // +0x84
    // the plug-in's SPIM* entry points (+0x88..+0xfc)
    long (*mActivateMixer)();
    long (*mCreateChannels)(unsigned long, SEChannelDescription*);
    long (*mDeactivateMixer)();
    void (*mDeleteWaveData)(SESample*);
    long (*mDestroyChannels)();
    long (*mDuplicateSample)(const SESample*, SESample**);
    unsigned long (*mGetChannelCount)();
    float (*mGetCPUUsage)();
    IDirectSound* (*mGetDirectSoundInterface)();
    unsigned long (*mGetHighestTimerDelay)();
    unsigned long (*mGetMaximumChannelCount)();
    long (*mGetSamplePlayBackTime)(SESample*, long*, long*);
    unsigned long (*mGetSampleOffset)(SESample*);
    float (*mGetVolume)();
    void (*mInitializeMixer)(GEWindow*);
    long (*mIsChannelBusyPlaying)(unsigned long);
    long (*mLoadWaveData)(FSFileSpec*, SESample*);
    long (*mMixAhead)();                    // +0xcc
    long (*mPlayChannel)(unsigned long);
    bool (*mProcessMessage)(unsigned long, void*, void*);
    long (*mSetChannelSample)(unsigned long, SESample*);
    long (*mSetLatency)(unsigned long);
    void (*mSetSampleLooping)(SESample*, bool);
    long (*mSetSampleOffset)(SESample*, unsigned long);
    long (*mSetSamplePan)(SESample*, float);
    long (*mSetSamplePitch)(SESample*, float);
    long (*mSetSampleVolume)(SESample*, float);
    long (*mSetVolume)(float);
    long (*mStopAllChannels)();
    long (*mStopChannel)(unsigned long);    // +0xfc
    // this port's additions (see GetOutputRate)
    unsigned long (*mGetOutputRate)();
    void (*mSetMusicSource)(void (*)(void*, float*, unsigned long), void*);
    void (*mSetMovieSource)(void (*)(void*, float*, unsigned long), void*);

    // the original's registry, static std::vector<SEMixer*> sMixers (0x1012de70), is Rust's (ee_sound mixers.rs)
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)
    static SEMixer* sActiveMixer;           // 0x1012de80 (renderers: see GetActiveMixer)
#endif
};

// port: the audio settings, in the mixer plug-in (Mixer.cpp; EEApplyAudioSettings): the master volume (0..1) over
// effects and music, the mute while the game is in the background, the output device by name (empty: the system's
// default; taken when the mixer opens) and the names of the devices there are (-> their count)
extern "C" void SPIMSetMasterVolume(float inVolume);
extern "C" void SPIMSetMuted(bool inMuted);
extern "C" void SPIMSetOutputDevice(const char* inName);
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);

#endif
