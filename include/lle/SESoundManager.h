// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.h (RECONSTRUCTION; SESoundManager.cpp 0x4f08e..0x532d9).
// Loads sounds into categories and plays instances of them on a mixer's channels; a scavenger thread retires
// finished instances. Member and type names are provisional unless an export or assert names them.
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESOUNDMANAGER_H
#define LLE_SESOUNDMANAGER_H

#include <vector>
#include <map>
#include "lle/UMemoryPool.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"

class FSPath;
class TSThread;
class TSEvent;

// The manager's bookkeeping and decisions (sounds, categories, channels, the paused queue, one-shot sounds, the
// scavenger's choices) are Rust: rust/crates/ee_sound (docs/RUST_PLAN.md R3.6). This class keeps the C++ objects
// (path, mixer, scavenger thread, events, lock, the node pool) and forwards under its lock as the original locked.
struct EeSoundManager;

class LLE_API SESoundManager {              // the original: sizeof 0x98 (implicit copy ctor 0x4c54d)
    friend class SESMInstanceNode;          // LastDereference returns the node to sSoundsPool
public:
    SESoundManager(FSPath* inPath, SEMixer* inMixer, unsigned long inCategoryCount, unsigned long inChannelCount);
    ~SESoundManager();

    static void CleanupMemoryPoolAndDestroyCriticalSections();
    static void DumpMemoryPoolInfo();
    SESMInstanceNode* GetAndReferenceSoundInstance(unsigned long inInstanceID);
    unsigned long GetAvailableChannels() const;
    TSCriticalSection* GetCriticalSection() const { return this->mLock; }
    unsigned long GetHighestTimerDelay() const { return this->mMixer->GetHighestTimerDelay(); }
    unsigned long GetInstanceID(SESMInstanceNode* inNode);
    bool GetIsLooping(unsigned long inInstanceID);
    bool GetIsLooping(SESMInstanceNode* inNode);
    bool GetIsMuted() { return this->mIsMuted; }
    bool GetIsPlaying(unsigned long inInstanceID);
    bool GetIsPlaying(SESMInstanceNode* inNode);
    float GetMasterVolume() const { return this->mMixer->GetVolume(); }
    unsigned long GetMaxChannels() const { return this->mMixer->GetMaximumChannelCount(); }
    float GetPan(unsigned long inInstanceID);
    float GetPan(SESMInstanceNode* inNode);
    FSPath* GetPath() const { return this->mPath; }
    float GetPitch(unsigned long inInstanceID);
    float GetPitch(SESMInstanceNode* inNode);
    unsigned long GetSampleID(unsigned long inInstanceID);
    unsigned long GetSampleID(SESMInstanceNode* inNode);
    unsigned long GetSampleRate(unsigned long inSampleID);
    long GetTimeLeftInMilli(unsigned long inInstanceID);
    long GetTotalPlayTimeInMilli(unsigned long inInstanceID);
    float GetVolume(unsigned long inInstanceID);
    static void InitializeSoundsPool(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount);
    unsigned long Load(const UString& inName, unsigned long inCategory, unsigned long inMaxInstances);
    long MixAhead() { return this->mMixer->MixAhead(); }
    unsigned long Play(unsigned long inSampleID, float inVolume, float inPan, float inPitch, bool inLooping,
                       char* inDebugName);
    SESMInstanceNode* PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan, float inPitch,
                                                  bool inLooping, char* inDebugName);
    long RegisterOneShotSound(unsigned long inSampleID, unsigned long inInterval);
    unsigned long SetCategoryPercentage(unsigned long inCategory, float inPercentage);
    long SetCategoryPriority(unsigned long inCategory, unsigned long inPriority);
    void SetIsMuted(bool inMuted) { this->mIsMuted = inMuted; }
    long SetLatency(unsigned long inMilliseconds) { return this->mMixer->SetLatency(inMilliseconds); }
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
    // the C++ side the Rust manager reaches (extern "C" ee_soundcpp_*, SESoundManager.cpp): a nested class, so
    // that it shares this class's access to the node, the mixer and the members below
    struct Rust;
protected:
    static long CreateSoundPoolLock();
    static void DestroySoundInstance(SESMInstanceNode* inNode);
    static SESMInstanceNode* GetNewSoundInstance();
private:
    static unsigned long Scavenger(void* inManager);

    EeSoundManager* mRust;                  // the bookkeeping (ee_sound), created and destroyed with this object
    FSPath* mPath;                          // (referenced)
    SEMixer* mMixer;
    TSThread* mScavengerThread;
    TSEvent* mScavengerThreadFinishedEvent; // (ctor ESVERIFY text)
    TSEvent* mScavengerThreadExitEvent;     // (ctor ESVERIFY text)
    TSCriticalSection* mLock;               // (GetCriticalSection)
    bool mIsMuted;                          // read by the scavenger pass

    static UMemoryPool<SESMInstanceNode> sSoundsPool;   // 0x1012de90 (defined in SESoundManager.cpp)
protected:
    static TSCriticalSection* sSoundNodePoolLock;       // 0x1012dec8
};

#endif
