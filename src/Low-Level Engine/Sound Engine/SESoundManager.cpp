// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The unit opens with sSoundsPool's init chain (0x4f08e) and SESMInstanceNode's out-of-line members; there is no
// SESMInstanceNode.cpp.
//
// The manager's bookkeeping and decisions are Rust (rust/crates/ee_sound, docs/RUST_PLAN.md R3.6): which channel an
// instance gets, the instance and category limits, priorities, the paused queue, one-shot sounds, the scavenger's
// choices and the random start offsets. This file keeps the C++ objects and their lifetimes (the pooled, reference
// counted instance nodes held by units and effects, the samples shared with the mixer, the path, the scavenger
// thread, the events and the lock) and forwards to Rust under the lock, taken where the original took it. The
// checks the original made before taking the lock (a null node, a none instance id) stay here, before `this` is
// used: the headless simulation calls Stop(node) on a null gSoundManager with a null node.
// Rust reaches the C++ objects through the ee_soundcpp_* functions at the end (SESoundManager::Rust).
#include <windows.h>
#include <float.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SESoundManager.h"
#include "rust/ee_sound.h"

UMemoryPool<SESMInstanceNode> SESoundManager::sSoundsPool;
TSCriticalSection* SESoundManager::sSoundNodePoolLock;

// FUNCTION: LLE 0x1004f0d3
// Creates a free, cleared sound instance node with no references
SESMInstanceNode::SESMInstanceNode()
{
    this->Clear();
    this->mReferenceCount = 0;
    this->mIsFree = true;
}

// SESMInstanceNode::~SESMInstanceNode: destroys the node (empty); out of line here (its body is ICF-folded with the
// copy kept at 0x1006e04c)
SESMInstanceNode::~SESMInstanceNode()
{
}

// FUNCTION: LLE 0x1004f0f4
// Clears the node: no sample, category, instance or channel, zero volume, pan and pitch
void SESMInstanceNode::Clear()
{
    this->mCategory = -1;
    this->mSampleID = -1;
    this->mMaxInstances = 0;
    this->mInstanceID = -1;
    this->mInstanceIndex = -1;
    this->mChannel = -1;
    this->mSample = NULL;
    this->mVolume = 0.0f;
    this->mPan = 0.0f;
    this->mPitch = 0.0f;
    this->mIsFree = false;
}

// FUNCTION: LLE 0x1004f121
// Returns the node to the shared node pool once its last reference is gone
void SESMInstanceNode::LastDereference()
{
    this->mReferenceCount = 0;
    this->mIsFree = true;
    SESoundManager::sSoundNodePoolLock->Enter();
    SESoundManager::sSoundsPool.Delete(this);
    SESoundManager::sSoundNodePoolLock->Leave();
}

// FUNCTION: LLE 0x1004f14f
// Clears a node fresh from the pool and gives it its first reference
void SESMInstanceNode::InitializeFromPool()
{
    this->Clear();
    this->mReferenceCount++;
}

// FUNCTION: LLE 0x1004f15c
// Creates the sound manager for a mixer: categories (each allowed every channel), channels, its lock, events,
// the scavenger thread and the shared node pool
SESoundManager::SESoundManager(FSPath* inPath, SEMixer* inMixer, unsigned long inCategoryCount,
                               unsigned long inChannelCount)
{
    ESVERIFY_AT(inMixer != NULL, 204);
    ESVERIFY_AT(inCategoryCount > 0, 205);

    ESVERIFY_AT(SESoundManager::CreateSoundPoolLock() == keseOK, 209);
    this->mPath = inPath;
    if (inPath)
        inPath->Reference();
    this->mMixer = inMixer;
    this->mIsMuted = false;
    // the state (every category at 100%, empty channels) exists before the scavenger thread can run
    this->mRust = ee_sound_create(inCategoryCount, inChannelCount ? inChannelCount : this->mMixer->GetChannelCount());

    long theResult = TSScheduler::CreateCriticalSection(this->mLock);
    ESVERIFY_AT(theResult == keseOK, 245);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mScavengerThreadFinishedEvent) == keseOK, 248);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mScavengerThreadExitEvent) == keseOK, 249);

    ESVERIFY_AT(TSScheduler::CreateThread(this->mScavengerThread, SESoundManager::Scavenger, this) == keseOK, 255);
    this->mScavengerThread->SetName(UString("SoundScavngr"));
    SESoundManager::InitializeSoundsPool(500, 100, 5);
}

// FUNCTION: LLE 0x1004f3bf
// Destroys the sound manager: stops the scavenger thread, drops every paused instance, deletes every loaded
// sample and category, and stops and destroys the mixer's channels
SESoundManager::~SESoundManager()
{
    this->mScavengerThreadExitEvent->Set();
    this->mScavengerThreadFinishedEvent->Wait(kTSWaitInfinite);
    TSScheduler::DestroyThread(this->mScavengerThread);
    TSScheduler::DestroyEvent(this->mScavengerThreadFinishedEvent);
    TSScheduler::DestroyEvent(this->mScavengerThreadExitEvent);
    TSScheduler::DestroyCriticalSection(this->mLock);
    ee_sound_destroy(this->mRust, this);
    this->mRust = NULL;
    if (this->mPath)
        this->mPath->Dereference();
}

// FUNCTION: LLE 0x1004f8ee
// Frees the shared node pool and destroys its lock (once)
void SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections()
{
    if (!SESoundManager::sSoundNodePoolLock)
        return;
    SESoundManager::sSoundNodePoolLock->Enter();
    SESoundManager::sSoundsPool.Release();
    SESoundManager::sSoundNodePoolLock->Leave();
    TSScheduler::DestroyCriticalSection(SESoundManager::sSoundNodePoolLock);
    SESoundManager::sSoundNodePoolLock = NULL;
}

// FUNCTION: LLE 0x1004f926
// Creates the lock of the shared node pool
long SESoundManager::CreateSoundPoolLock()
{
    return TSScheduler::CreateCriticalSection(SESoundManager::sSoundNodePoolLock);
}

// FUNCTION: LLE 0x1004f932
// Drops one reference to the instance node (NULL is ignored)
void SESoundManager::DestroySoundInstance(SESMInstanceNode* inNode)
{
    if (inNode)
        inNode->Dereference();
}

// Returns the number of channels the manager uses (out of line now: the count is Rust's)
unsigned long SESoundManager::GetAvailableChannels() const
{
    this->mLock->Enter();
    unsigned long theCount = ee_sound_available_channels(this->mRust);
    this->mLock->Leave();
    return theCount;
}

// FUNCTION: LLE 0x1004fc19
// Returns the node's instance ID (-1: no node)
unsigned long SESoundManager::GetInstanceID(SESMInstanceNode* inNode)
{
    unsigned long theID = -1;
    if (inNode) {
        this->mLock->Enter();
        theID = inNode->mInstanceID;
        this->mLock->Leave();
    }
    return theID;
}

// FUNCTION: LLE 0x1004fc46
// Returns whether the playing or paused instance loops (a paused one without a sample counts as looping)
bool SESoundManager::GetIsLooping(unsigned long inInstanceID)
{
    bool theLooping = false;
    if (!UIsNone32(inInstanceID)) {
        this->mLock->Enter();
        theLooping = ee_sound_is_looping(this->mRust, this, (uint32_t)inInstanceID);
        this->mLock->Leave();
    }
    return theLooping;
}

// FUNCTION: LLE 0x1004fca3
// Returns whether the node's instance loops (one without a sample counts as looping)
bool SESoundManager::GetIsLooping(SESMInstanceNode* inNode)
{
    bool theLooping = false;
    if (inNode) {
        this->mLock->Enter();
        theLooping = ee_sound_node_is_looping(inNode);
        this->mLock->Leave();
    }
    return theLooping;
}

// FUNCTION: LLE 0x1004fcda
// Returns whether the playing or paused instance has its sample on a mixer channel
bool SESoundManager::GetIsPlaying(unsigned long inInstanceID)
{
    this->mLock->Enter();
    bool thePlaying = ee_sound_is_playing(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return thePlaying;
}

// FUNCTION: LLE 0x1004fd38
// Returns whether the node's sample is on a mixer channel
bool SESoundManager::GetIsPlaying(SESMInstanceNode* inNode)
{
    bool thePlaying = false;
    if (inNode) {
        this->mLock->Enter();
        thePlaying = ee_sound_node_is_playing(inNode);
        this->mLock->Leave();
    }
    return thePlaying;
}

// FUNCTION: LLE 0x1004fdd7
// Takes a cleared, referenced node from the shared node pool
SESMInstanceNode* SESoundManager::GetNewSoundInstance()
{
    SESoundManager::sSoundNodePoolLock->Enter();
    SESMInstanceNode* theNode = SESoundManager::sSoundsPool.New();
    theNode->InitializeFromPool();
    SESoundManager::sSoundNodePoolLock->Leave();
    return theNode;
}

// FUNCTION: LLE 0x1004feb7
// Returns the pan of the playing or paused instance (0: no such instance)
float SESoundManager::GetPan(unsigned long inInstanceID)
{
    this->mLock->Enter();
    float thePan = ee_sound_pan(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return thePan;
}

// FUNCTION: LLE 0x1004ff16
// Returns the pan of the node's instance (0: no node)
float SESoundManager::GetPan(SESMInstanceNode* inNode)
{
    float thePan = 0.0f;
    if (inNode) {
        this->mLock->Enter();
        thePan = ee_sound_node_pan(inNode);
        this->mLock->Leave();
    }
    return thePan;
}

// FUNCTION: LLE 0x1004ff59
// Returns the pitch of the playing or paused instance (1: no such instance)
float SESoundManager::GetPitch(unsigned long inInstanceID)
{
    this->mLock->Enter();
    float thePitch = ee_sound_pitch(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return thePitch;
}

// FUNCTION: LLE 0x1004ffb8
// Returns the pitch of the node's instance (0: no node)
float SESoundManager::GetPitch(SESMInstanceNode* inNode)
{
    float thePitch = 0.0f;
    if (inNode) {
        this->mLock->Enter();
        thePitch = ee_sound_node_pitch(inNode);
        this->mLock->Leave();
    }
    return thePitch;
}

// FUNCTION: LLE 0x10050100
// Returns the sample ID of the playing or paused instance (-1: no such instance)
unsigned long SESoundManager::GetSampleID(unsigned long inInstanceID)
{
    this->mLock->Enter();
    uint32_t theID = ee_sound_sample_id(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return theID == 0xffffffffu ? (unsigned long)-1 : (unsigned long)theID;
}

// FUNCTION: LLE 0x10050151
// Returns the sample ID of the node's instance (-1: no node)
unsigned long SESoundManager::GetSampleID(SESMInstanceNode* inNode)
{
    unsigned long theID = -1;
    if (inNode) {
        this->mLock->Enter();
        theID = inNode->mSampleID;
        this->mLock->Leave();
    }
    return theID;
}

// FUNCTION: LLE 0x1005017e
// Returns the sample rate of the playing or paused instance, taken from the sample's first instance if it has none
unsigned long SESoundManager::GetSampleRate(unsigned long inInstanceID)
{
    this->mLock->Enter();
    unsigned long theRate = ee_sound_sample_rate(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return theRate;
}

// FUNCTION: LLE 0x100501eb
// Returns the playing or paused instance with an added reference (NULL: none)
SESMInstanceNode* SESoundManager::GetAndReferenceSoundInstance(unsigned long inInstanceID)
{
    SESMInstanceNode* theNode = NULL;
    if (!UIsNone32(inInstanceID)) {
        this->mLock->Enter();
        theNode = static_cast<SESMInstanceNode*>(ee_sound_find_instance(this->mRust, this, (uint32_t)inInstanceID));
        if (theNode)
            theNode->Reference();
        this->mLock->Leave();
    }
    return theNode;
}

// FUNCTION: LLE 0x10050273
// Returns the milliseconds left to play of the playing or paused instance (0: none; -1: looping)
long SESoundManager::GetTimeLeftInMilli(unsigned long inInstanceID)
{
    this->mLock->Enter();
    long theTime = ee_sound_time_left(this->mRust, this, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return theTime;
}

// FUNCTION: LLE 0x100502c6
// Returns the length in milliseconds of the sample's first instance, but only while an instance of it is
// playing or paused (else 0; -1: looping)
long SESoundManager::GetTotalPlayTimeInMilli(unsigned long inSampleID)
{
    this->mLock->Enter();
    long theTime = ee_sound_total_play_time(this->mRust, this, (uint32_t)inSampleID);
    this->mLock->Leave();
    return theTime;
}

// FUNCTION: LLE 0x1005034f
// Returns the instance's volume under the lock (1: no such instance)
float SESoundManager::GetVolume(unsigned long inInstanceID)
{
    this->mLock->Enter();
    float theVolume = ee_sound_volume(this->mRust, this, (uint32_t)inInstanceID);
    // Fixed: the lock used to be entered a second time here instead of left, so it was never released
    this->mLock->Leave();
    return theVolume;
}

// FUNCTION: LLE 0x100503e0
// Sets the chunk sizes of the shared node pool
void SESoundManager::InitializeSoundsPool(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount)
{
    SESoundManager::sSoundsPool.SetSize(inChunkSize, inMaxChunks, inChunkCount);
}

// FUNCTION: LLE 0x100503f7
// Loads a sample from the sound path into a category with up to the given number of instances (at most one per
// channel; extra ones duplicate the first); a name already loaded gains instances and a load; returns the sample ID
unsigned long SESoundManager::Load(const UString& inName, unsigned long inCategory, unsigned long inMaxInstances)
{
    this->mLock->Enter();
    uint32_t theSampleID = ee_sound_load(this->mRust, this, (const char*)inName, (uint32_t)inCategory,
                                         (uint32_t)inMaxInstances);
    this->mLock->Leave();
    return theSampleID == 0xffffffffu ? (unsigned long)-1 : (unsigned long)theSampleID;
}

// FUNCTION: LLE 0x100506df
// Plays the sample and returns the new instance ID (-1: not played)
unsigned long SESoundManager::Play(unsigned long inSampleID, float inVolume, float inPan, float inPitch,
                                   bool inLooping, char* inDebugName)
{
    SESMInstanceNode* theNode = this->PlayAndReturnReferencedNode(inSampleID, inVolume, inPan, inPitch, inLooping,
                                                                  inDebugName);
    unsigned long theInstanceID = -1;
    if (theNode) {
        this->mLock->Enter();
        theInstanceID = theNode->mInstanceID;
        theNode->Dereference();
        this->mLock->Leave();
    }
    return theInstanceID;
}

// FUNCTION: LLE 0x10050749
// Finds a channel (a free one, else one whose sample finished, else one of lower priority; at the instance limits
// an instance of the sample or its category is replaced) and plays on it; a looping one with none is queued paused.
// The original tested the sample ID before taking the lock; the test reads the Rust state, so it is under the lock.
SESMInstanceNode* SESoundManager::PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan,
                                                              float inPitch, bool inLooping, char* inDebugName)
{
    this->mLock->Enter();
    SESMInstanceNode* theNode = static_cast<SESMInstanceNode*>(
        ee_sound_play(this->mRust, this, (uint32_t)inSampleID, inVolume, inPan, inPitch, inLooping));
    this->mLock->Leave();
    return theNode;
}

// FUNCTION: LLE 0x10050a9c
// Remembers a playing or paused instance as a one-shot sound, unloaded once it stops playing
long SESoundManager::RegisterOneShotSound(unsigned long inSampleID, unsigned long inInstanceID)
{
    this->mLock->Enter();
    long theResult = ee_sound_register_one_shot(this->mRust, this, (uint32_t)inSampleID, (uint32_t)inInstanceID);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10050c57
// Scavenger thread: every second retires finished or silent channels, detaches the samples of silent paused
// instances, and restarts the loudest paused instances (by category priority, then volume) on free channels
unsigned long SESoundManager::Scavenger(void* inThisObject)
{
    SESoundManager* theManager = static_cast<SESoundManager*>(inThisObject);
    bool theDone = false;
    ESVERIFY_AT(inThisObject != NULL, 2626);
    TSEvent* theHandles[1];
    theHandles[0] = theManager->mScavengerThreadExitEvent;
    UMSetFloatingPointPrecision(kFPPrecision53);
    do {
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        theManager->mLock->Enter();
        ee_sound_update_one_shots(theManager->mRust, theManager);
        theManager->mLock->Leave();
        switch (TSEvent::WaitAny(theHandles, 1, 1000)) {
        case 0:
            theManager->mScavengerThreadExitEvent->Clear();
            theDone = true;
            break;
        case kTSWaitTimeout:
            theManager->mLock->Enter();
            ee_sound_scavenge(theManager->mRust, theManager, theManager->mIsMuted);
            theManager->mLock->Leave();
            PlatSleep(1);
            break;
        case kTSWaitFailed:
            PlatGetLastError();
            break;
        }
    } while (!theDone);
    theManager->mScavengerThreadFinishedEvent->Set();
    return 0;
}

// FUNCTION: LLE 0x1005154b
// Sets the share of the channels a category may use (0..1, at least one channel), creating the category
// (with the lowest priority, as Load creates one)
unsigned long SESoundManager::SetCategoryPercentage(unsigned long inCategory, float inPercent)
{
    ESVERIFY_AT(inPercent >= 0.0 && inPercent <= 1.0, 2936);
    this->mLock->Enter();
    unsigned long theMaxInstances = ee_sound_set_category_percentage(this->mRust, (uint32_t)inCategory, inPercent);
    this->mLock->Leave();
    return theMaxInstances;
}

// FUNCTION: LLE 0x100515fb
// Sets the mixer's master volume
long SESoundManager::SetMasterVolume(float inVolume)
{
    return this->mMixer->SetVolume(inVolume);
}

// FUNCTION: LLE 0x10051610
// Changes the number of channels: pauses looping and stops other instances, recreates the mixer's channels
// and recomputes each category's instance limit
long SESoundManager::SetMaxChannels(unsigned long inCount)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_max_channels(this->mRust, this, (uint32_t)inCount);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051778
// Replaces the sound search path (referenced)
void SESoundManager::SetPath(FSPath* inPath)
{
    if (this->mPath)
        this->mPath->Dereference();
    this->mPath = inPath;
    if (this->mPath)
        this->mPath->Reference();
}

// FUNCTION: LLE 0x10051799
// Sets the pan of the playing or paused instance, under the lock
long SESoundManager::SetPan(unsigned long inInstanceID, float inPan)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_pan(this->mRust, this, (uint32_t)inInstanceID, inPan);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100517d1
// Sets the pan of the node's instance, under the lock
long SESoundManager::SetPan(SESMInstanceNode* inNode, float inPan)
{
    this->mLock->Enter();
    long theResult = inNode ? ee_sound_node_set_pan(inNode, inPan) : kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x1005188f
// Sets the pitch of the playing or paused instance, under the lock
long SESoundManager::SetPitch(unsigned long inInstanceID, float inPitch)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_pitch(this->mRust, this, (uint32_t)inInstanceID, inPitch);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100518c7
// Sets the pitch of the node's instance, under the lock
long SESoundManager::SetPitch(SESMInstanceNode* inNode, float inPitch)
{
    this->mLock->Enter();
    long theResult = inNode ? ee_sound_node_set_pitch(inNode, inPitch) : kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051985
// Sets the category's priority (a larger value is a lower priority); -1 if the category does not exist
long SESoundManager::SetCategoryPriority(unsigned long inCategory, unsigned long inPriority)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_category_priority(this->mRust, (uint32_t)inCategory, (uint32_t)inPriority);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100519a2
// Sets the volume, pan and pitch of the playing or paused instance; returns the first sample error
long SESoundManager::SetSoundParameters(unsigned long inInstanceID, float inVolume, float inPan, float inPitch)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_parameters(this->mRust, this, (uint32_t)inInstanceID, inVolume, inPan, inPitch);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051a5d
// Sets the volume, pan and pitch of the node's instance; returns the first sample error
long SESoundManager::SetSoundParameters(SESMInstanceNode* inNode, float inVolume, float inPan, float inPitch)
{
    long theResult = kErrorOK;
    if (inNode) {
        this->mLock->Enter();
        theResult = ee_sound_node_set_parameters(inNode, inVolume, inPan, inPitch);
        this->mLock->Leave();
    }
    return theResult;
}

// FUNCTION: LLE 0x10051af7
// Sets the volume of the playing or paused instance (on its sample and on the node), under the lock
long SESoundManager::SetVolume(unsigned long inInstanceID, float inVolume)
{
    this->mLock->Enter();
    long theResult = ee_sound_set_volume(this->mRust, this, (uint32_t)inInstanceID, inVolume);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051b2f
// Sets the volume of the node's instance (on its sample, else on the node), under the lock
long SESoundManager::SetVolume(SESMInstanceNode* inNode, float inVolume)
{
    this->mLock->Enter();
    long theResult = inNode ? ee_sound_node_set_volume(inNode, inVolume) : kErrorOK;
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051bf1
// Stops the playing or paused instance and frees its channel or queue slot, or with inLetFinish only ends its
// looping so it plays out
long SESoundManager::Stop(unsigned long inInstanceID, bool inLetFinish)
{
    if (UIsNone32(inInstanceID))
        return kErrorOK;
    this->mLock->Enter();
    long theResult = ee_sound_stop(this->mRust, this, (uint32_t)inInstanceID, inLetFinish);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051e31
// Stops the node's instance by ID, passing the flag through (true: let it play out)
long SESoundManager::Stop(SESMInstanceNode* inNode, bool inImmediately)
{
    if (!inNode)
        return kErrorOK;
    this->mLock->Enter();
    unsigned long theInstanceID = inNode->mInstanceID;
    this->mLock->Leave();
    if (!UIsNone32(theInstanceID))
        this->Stop(theInstanceID, inImmediately);
    return kErrorOK;
}

// FUNCTION: LLE 0x10051e72
// Stops and unloads every remembered one-shot sound
long SESoundManager::StopAllOneShotSounds()
{
    this->mLock->Enter();
    long theResult = ee_sound_stop_all_one_shots(this->mRust, this);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051ece
// Unloads one load of the sample; at the last drops its paused instances, frees its channels and deletes its
// instances and samples; fails for an unknown sample (the test, before the lock in the original, reads the Rust
// state, so it is under the lock)
long SESoundManager::Unload(unsigned long inSampleID)
{
    this->mLock->Enter();
    long theResult = ee_sound_unload(this->mRust, this, (uint32_t)inSampleID);
    this->mLock->Leave();
    return theResult;
}

// No marker: exported, empty in this build; ICF-folded into the bare ret at 0x10080c07 (UMath.cpp).
void SESoundManager::DumpMemoryPoolInfo()
{
}

// The C++ objects as the Rust manager sees them (ee_sound/src/ffi.rs `mod sys`). Called with the manager's lock
// held; none of them calls back into the manager. A nested class: it shares SESoundManager's access to the node's
// fields and (as SEMixer's friend) to the mixer's entry points.
struct SESoundManager::Rust {
    static SEMixer* Mixer(void* inContext) { return static_cast<SESoundManager*>(inContext)->mMixer; }
    static FSPath* Path(void* inContext) { return static_cast<SESoundManager*>(inContext)->mPath; }
    // a stored none (-1) or paused (-2) keeps all bits set on LP64, as the C++ wrote it (`= -1`)
    static unsigned long Wide(uint32_t inValue)
    {
        return inValue >= 0xfffffffeu ? (unsigned long)(long)(int32_t)inValue : (unsigned long)inValue;
    }
    static void Get(const SESMInstanceNode* inNode, EeSoundNodeFields* outFields)
    {
        outFields->category = (uint32_t)inNode->mCategory;
        outFields->sample_id = (uint32_t)inNode->mSampleID;
        outFields->max_instances = (uint32_t)inNode->mMaxInstances;
        outFields->instance_id = (uint32_t)inNode->mInstanceID;
        outFields->instance_index = (uint32_t)inNode->mInstanceIndex;
        outFields->channel = (uint32_t)inNode->mChannel;
        outFields->sample = inNode->mSample;
        outFields->volume = inNode->mVolume;
        outFields->pan = inNode->mPan;
        outFields->pitch = inNode->mPitch;
    }
    static void Set(SESMInstanceNode* ioNode, const EeSoundNodeFields* inFields)
    {
        ioNode->mCategory = Wide(inFields->category);
        ioNode->mSampleID = Wide(inFields->sample_id);
        ioNode->mMaxInstances = inFields->max_instances;
        ioNode->mInstanceID = Wide(inFields->instance_id);
        ioNode->mInstanceIndex = Wide(inFields->instance_index);
        ioNode->mChannel = Wide(inFields->channel);
        ioNode->mSample = static_cast<SESample*>(inFields->sample);
        ioNode->mVolume = inFields->volume;
        ioNode->mPan = inFields->pan;
        ioNode->mPitch = inFields->pitch;
    }
    static SESMInstanceNode* NewNode() { return SESoundManager::GetNewSoundInstance(); }
    static SESample* Duplicate(void* inContext, SESample* inSample)
    {
        SESample* theCopy = NULL;
        Mixer(inContext)->DuplicateSample(*inSample, theCopy);
        return theCopy;
    }
    static long StopChannel(void* inContext, unsigned long inChannel) { return Mixer(inContext)->StopChannel(inChannel); }
    static long SetSampleOffset(void* inContext, SESample* inSample, unsigned long inOffset)
    {
        return Mixer(inContext)->SetSampleOffset(inSample, inOffset);
    }
    static long ChannelState(void* inContext, unsigned long inChannel)
    {
        return Mixer(inContext)->IsChannelBusyPlaying(inChannel);
    }
    static void PlaybackTime(void* inContext, SESample* inSample, int32_t* outTotal, int32_t* outRemaining)
    {
        long theTotal = 0;
        long theRemaining = 0;
        Mixer(inContext)->GetSamplePlayBackTime(inSample, theTotal, theRemaining);
        *outTotal = (int32_t)theTotal;
        *outRemaining = (int32_t)theRemaining;
    }
    static long SetChannelSample(void* inContext, unsigned long inChannel, SESample* inSample)
    {
        return Mixer(inContext)->SetChannelSample(inChannel, inSample);
    }
    static long PlayChannel(void* inContext, unsigned long inChannel) { return Mixer(inContext)->PlayChannel(inChannel); }
    static long StopAllChannels(void* inContext) { return Mixer(inContext)->StopAllChannels(); }
    static long DestroyChannels(void* inContext) { return Mixer(inContext)->DestroyChannels(); }
    static long CreateChannels(void* inContext, unsigned long inCount)
    {
        SEChannelDescription theDescription;
        return Mixer(inContext)->CreateChannels(inCount, &theDescription);
    }
};

extern "C" {
void* ee_soundcpp_node_new()
{
    return SESoundManager::Rust::NewNode();
}
void ee_soundcpp_node_reference(void* inNode)
{
    static_cast<SESMInstanceNode*>(inNode)->Reference();
}
void ee_soundcpp_node_dereference(void* inNode)
{
    static_cast<SESMInstanceNode*>(inNode)->Dereference();
}
void ee_soundcpp_node_get(void* inNode, EeSoundNodeFields* outFields)
{
    SESoundManager::Rust::Get(static_cast<SESMInstanceNode*>(inNode), outFields);
}
void ee_soundcpp_node_set(void* inNode, const EeSoundNodeFields* inFields)
{
    SESoundManager::Rust::Set(static_cast<SESMInstanceNode*>(inNode), inFields);
}
bool ee_soundcpp_sample_is_looping(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetIsLooping();
}
void ee_soundcpp_sample_set_looping(void* inSample, bool inLooping)
{
    static_cast<SESample*>(inSample)->SetIsLooping(inLooping);
}
float ee_soundcpp_sample_volume(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetVolume();
}
float ee_soundcpp_sample_pan(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetPan();
}
float ee_soundcpp_sample_pitch(void* inSample)
{
    return static_cast<SESample*>(inSample)->GetPitch();
}
int32_t ee_soundcpp_sample_set_volume(void* inSample, float inValue)
{
    return (int32_t)static_cast<SESample*>(inSample)->SetVolume(inValue);
}
int32_t ee_soundcpp_sample_set_pan(void* inSample, float inValue)
{
    return (int32_t)static_cast<SESample*>(inSample)->SetPan(inValue);
}
int32_t ee_soundcpp_sample_set_pitch(void* inSample, float inValue)
{
    return (int32_t)static_cast<SESample*>(inSample)->SetPitch(inValue);
}
uint32_t ee_soundcpp_sample_mixer_channel(void* inSample)
{
    return (uint32_t)static_cast<SESample*>(inSample)->GetMixerChannel();
}
void ee_soundcpp_sample_set_mixer_channel(void* inSample, uint32_t inChannel)
{
    static_cast<SESample*>(inSample)->SetMixerChannel(SESoundManager::Rust::Wide(inChannel));
}
uint32_t ee_soundcpp_sample_length(void* inSample)
{
    return (uint32_t)static_cast<SESample*>(inSample)->GetLengthInSamples();
}
uint32_t ee_soundcpp_sample_rate(void* inSample)
{
    return (uint32_t)static_cast<SESample*>(inSample)->GetSampleRate();
}
void* ee_soundcpp_sample_load(void*, void* inFile)
{
    SESample* theSample = new SESample;
    theSample->LoadWaveData(static_cast<FSFileSpec*>(inFile));
    return theSample;
}
void* ee_soundcpp_sample_duplicate(void* inContext, void* inSample)
{
    return SESoundManager::Rust::Duplicate(inContext, static_cast<SESample*>(inSample));
}
void ee_soundcpp_sample_delete(void* inSample)
{
    delete static_cast<SESample*>(inSample);
}
// FindFile on the manager's path (NULL: not found; a spec the path returned with an error leaks, as before)
void* ee_soundcpp_find_file(void* inContext, const char* inName)
{
    FSPath* thePath = SESoundManager::Rust::Path(inContext);
    FSFileSpec* theFileSpec = NULL;
    if (!thePath || thePath->FindFile(UString(inName), theFileSpec) != kErrorOK)
        return NULL;
    return theFileSpec;
}
void ee_soundcpp_file_delete(void* inFile)
{
    delete static_cast<FSFileSpec*>(inFile);
}
void ee_soundcpp_mixer_stop_channel(void* inContext, uint32_t inChannel)
{
    SESoundManager::Rust::StopChannel(inContext, SESoundManager::Rust::Wide(inChannel));
}
void ee_soundcpp_mixer_set_sample_offset(void* inContext, void* inSample, uint32_t inOffset)
{
    SESoundManager::Rust::SetSampleOffset(inContext, static_cast<SESample*>(inSample), inOffset);
}
int32_t ee_soundcpp_mixer_channel_state(void* inContext, uint32_t inChannel)
{
    return (int32_t)SESoundManager::Rust::ChannelState(inContext, SESoundManager::Rust::Wide(inChannel));
}
void ee_soundcpp_mixer_playback_time(void* inContext, void* inSample, int32_t* outTotal, int32_t* outRemaining)
{
    SESoundManager::Rust::PlaybackTime(inContext, static_cast<SESample*>(inSample), outTotal, outRemaining);
}
int32_t ee_soundcpp_mixer_set_channel_sample(void* inContext, uint32_t inChannel, void* inSample)
{
    return (int32_t)SESoundManager::Rust::SetChannelSample(inContext, SESoundManager::Rust::Wide(inChannel),
                                                           static_cast<SESample*>(inSample));
}
void ee_soundcpp_mixer_play_channel(void* inContext, uint32_t inChannel)
{
    SESoundManager::Rust::PlayChannel(inContext, SESoundManager::Rust::Wide(inChannel));
}
void ee_soundcpp_mixer_stop_all_channels(void* inContext)
{
    SESoundManager::Rust::StopAllChannels(inContext);
}
void ee_soundcpp_mixer_destroy_channels(void* inContext)
{
    SESoundManager::Rust::DestroyChannels(inContext);
}
int32_t ee_soundcpp_mixer_create_channels(void* inContext, uint32_t inCount)
{
    return (int32_t)SESoundManager::Rust::CreateChannels(inContext, inCount);
}
}
