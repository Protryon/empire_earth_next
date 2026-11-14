// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.h (RECONSTRUCTION; SESoundManager.cpp 0x4f08e..0x532d9).
// Loads sounds into categories and plays instances of them on a mixer's channels; a scavenger thread retires
// finished instances. Whole in Rust (rust/crates/ee_sound managerobj.rs, the decisions manager.rs; LLE lane round
// 6): an SESoundManager* is an EeSoundManager*, made by Create and deleted by Destroy (the original's constructor
// and destructor); the members are defined in SESoundManager.cpp. Requires lle/LowLevelEngine.h.
#ifndef LLE_SESOUNDMANAGER_H
#define LLE_SESOUNDMANAGER_H

#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"

class FSPath;

class LLE_API SESoundManager {
public:
    static SESoundManager* Create(FSPath* inPath, SEMixer* inMixer, unsigned long inCategoryCount,
                                  unsigned long inChannelCount);
    static void Destroy(SESoundManager* inManager);

    static void CleanupMemoryPoolAndDestroyCriticalSections();
    static void DumpMemoryPoolInfo() {}     // empty in this build
    SESMInstanceNode* GetAndReferenceSoundInstance(unsigned long inInstanceID);
    unsigned long GetAvailableChannels() const;
    TSCriticalSection* GetCriticalSection() const;
    unsigned long GetHighestTimerDelay() const;
    unsigned long GetInstanceID(SESMInstanceNode* inNode);
    bool GetIsLooping(unsigned long inInstanceID);
    bool GetIsLooping(SESMInstanceNode* inNode);
    bool GetIsMuted();
    bool GetIsPlaying(unsigned long inInstanceID);
    bool GetIsPlaying(SESMInstanceNode* inNode);
    float GetMasterVolume() const;
    unsigned long GetMaxChannels() const;
    float GetPan(unsigned long inInstanceID);
    float GetPan(SESMInstanceNode* inNode);
    FSPath* GetPath() const;
    float GetPitch(unsigned long inInstanceID);
    float GetPitch(SESMInstanceNode* inNode);
    unsigned long GetSampleID(unsigned long inInstanceID);
    unsigned long GetSampleID(SESMInstanceNode* inNode);
    unsigned long GetSampleRate(unsigned long inSampleID);
    long GetTimeLeftInMilli(unsigned long inInstanceID);
    long GetTotalPlayTimeInMilli(unsigned long inInstanceID);
    float GetVolume(unsigned long inInstanceID);
    // the node pool's chunk sizes: the pool is Rust's, sized as it grows (kept for the interface)
    static void InitializeSoundsPool(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount)
        {}
    unsigned long Load(const UString& inName, unsigned long inCategory, unsigned long inMaxInstances);
    long MixAhead();
    unsigned long Play(unsigned long inSampleID, float inVolume, float inPan, float inPitch, bool inLooping,
                       char* inDebugName);
    SESMInstanceNode* PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan, float inPitch,
                                                  bool inLooping, char* inDebugName);
    long RegisterOneShotSound(unsigned long inSampleID, unsigned long inInterval);
    unsigned long SetCategoryPercentage(unsigned long inCategory, float inPercentage);
    long SetCategoryPriority(unsigned long inCategory, unsigned long inPriority);
    void SetIsMuted(bool inMuted);
    long SetLatency(unsigned long inMilliseconds);
    long SetMasterVolume(float inVolume);
    long SetMaxChannels(unsigned long inCount);
    long SetPan(unsigned long inInstanceID, float inPan);
    long SetPan(SESMInstanceNode* inNode, float inPan);
    void SetPath(FSPath* inPath);
    long SetPitch(unsigned long inInstanceID, float inPitch);
    long SetPitch(SESMInstanceNode* inNode, float inPitch);
    long SetSoundParameters(unsigned long inInstanceID, float inVolume, float inPan, float inPitch);
    long SetSoundParameters(SESMInstanceNode* inNode, float inVolume, float inPan, float inPitch);
    long SetVolume(unsigned long inInstanceID, float inVolume);
    long SetVolume(SESMInstanceNode* inNode, float inVolume);
    long Stop(unsigned long inInstanceID, bool inImmediately);
    long Stop(SESMInstanceNode* inNode, bool inImmediately);
    long StopAllOneShotSounds();
    long Unload(unsigned long inSampleID);
private:
    SESoundManager() = delete;
    ~SESoundManager() = delete;
};

#endif
