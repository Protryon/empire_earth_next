// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The unit opens with sSoundsPool's init chain (0x4f08e) and SESMInstanceNode's out-of-line members; there is no
// SESMInstanceNode.cpp.
#include <windows.h>
#include <float.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/SESoundManager.h"
#include <algorithm>
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (ReallyPlayIt; docs/LOST_DEBUG.md)

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1005278f
// TEMPLATE: LLE 0x10052ba6
// TEMPLATE: LLE 0x10052bd6
// SYNTHETIC: LLE 0x10052ef3
// TEMPLATE: LLE 0x10052293
// TEMPLATE: LLE 0x1005246d
// TEMPLATE: LLE 0x10052b2b
// TEMPLATE: LLE 0x10052b58
// TEMPLATE: LLE 0x10052b79
// TEMPLATE: LLE 0x100523dc
// TEMPLATE: LLE 0x10052c8e
// SYNTHETIC: LLE 0x1004f08e
// SYNTHETIC: LLE 0x1004f098
// SYNTHETIC: LLE 0x1004f0a2
// SYNTHETIC: LLE 0x1004f0ae
// TEMPLATE: LLE 0x1005222d

UMemoryPool<SESMInstanceNode> SESoundManager::sSoundsPool;
TSCriticalSection* SESoundManager::sSoundNodePoolLock;

// std::sort order of the paused queue in Scavenger: larger category priority value (lower priority) first, then
// (equal priority) the quieter instance first, so the loudest end up at the back where Scavenger starts
// (provisional name).
struct SESMPausedOrder {
    SESMPausedOrder(SESoundManager* inManager) : mManager(inManager) {}
    bool operator()(SESMInstanceNode* const& inA, SESMInstanceNode* const& inB) const
    {
        if (this->mManager->GetCategoryPriority(inA->mCategory) > this->mManager->GetCategoryPriority(inB->mCategory))
            return true;
        if (this->mManager->GetCategoryPriority(inA->mCategory) < this->mManager->GetCategoryPriority(inB->mCategory))
            return false;
        return inA->mVolume < inB->mVolume;
    }
    SESoundManager* mManager;
};

// The best channel to give up at one priority level (DumpLowerPrioritySample) (provisional names).
struct SESMDumpCandidate {
    unsigned long mChannel;
    unsigned long mUnused;
    bool mIsLooping;
    float mTimeLeft;                        // seconds
};

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
    this->mCategoryCount = inCategoryCount;
    if (inChannelCount)
        this->mAvailableChannels = inChannelCount;
    else
        this->mAvailableChannels = this->mMixer->GetChannelCount();
    this->mSoundCount = 0;
    this->mNextInstanceID = 0;
    this->mCategories.resize(inCategoryCount);
    for (unsigned long i = 0; i < inCategoryCount; i++)
        this->mCategories[i] = NULL;
    this->mChannels.resize(this->mAvailableChannels);
    for (i = 0; i < this->mAvailableChannels; i++)
        this->mChannels[i] = NULL;

    long theResult = TSScheduler::CreateCriticalSection(this->mLock);
    ESVERIFY_AT(theResult == keseOK, 245);

    ESVERIFY_AT(TSScheduler::CreateEvent(this->mScavengerThreadFinishedEvent) == keseOK, 248);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mScavengerThreadExitEvent) == keseOK, 249);

    ESVERIFY_AT(TSScheduler::CreateThread(this->mScavengerThread, SESoundManager::Scavenger, this) == keseOK, 255);
    this->mScavengerThread->SetName(UString("SoundScavngr"));
    for (i = 0; i < inCategoryCount; i++)
        this->SetCategoryPercentage(i, 1.0f);
    SESoundManager::InitializeSoundsPool(500, 100, 5);
    this->mIsMuted = false;
}

// FUNCTION: LLE 0x1004f3bf
// Destroys the sound manager: stops the scavenger thread, drops every paused instance, deletes every loaded
// sample and category, and stops and destroys the mixer's channels
SESoundManager::~SESoundManager()
{
    std::map<unsigned long, SESMInstanceNode*>::iterator theIterator;
    unsigned long i;
    unsigned long j;
    this->mScavengerThreadExitEvent->Set();
    this->mScavengerThreadFinishedEvent->Wait(INFINITE);
    TSScheduler::DestroyThread(this->mScavengerThread);
    TSScheduler::DestroyEvent(this->mScavengerThreadFinishedEvent);
    TSScheduler::DestroyEvent(this->mScavengerThreadExitEvent);
    TSScheduler::DestroyCriticalSection(this->mLock);
    for (i = 0; i < this->mPaused.size(); i++) {
        if (this->mPaused[i]) {
            this->mPaused[i]->Dereference();
            this->mPaused[i] = NULL;
        }
    }
    for (theIterator = this->mPausedInstances.begin(); theIterator != this->mPausedInstances.end(); theIterator++) {
        if (theIterator->second)
            theIterator->second->Dereference();
    }
    this->mPausedInstances.clear();
    for (i = 0; i < this->mSoundCount; i++) {
        std::vector<SESMInstanceNode*>* theInstances = this->mSounds[i].mInstances;
        unsigned long theMaxInstances = this->GetMaxInstances(i);
        if (!theInstances)
            continue;
        for (j = 0; j < theMaxInstances; j++) {
            delete (*theInstances)[j]->mSample;
            (*theInstances)[j]->mSample = NULL;
            (*theInstances)[j]->Dereference();
        }
        delete theInstances;
    }
    for (i = 0; i < this->mCategoryCount; i++) {
        if (this->mCategories[i])
            delete this->mCategories[i];
    }
    this->mMixer->StopAllChannels();
    this->mMixer->DestroyChannels();
    if (this->mPath)
        this->mPath->Dereference();
}

// FUNCTION: LLE 0x1004f652
// Takes an instance off its channel (stopping it) and puts it on the paused queue
void SESoundManager::AddInstanceToPausedQueue(SESMInstanceNode* inNode)
{
    this->mMixer->StopChannel(inNode->mSample->GetMixerChannel());
    if (inNode->mChannel != -1) {
        if (this->mChannels[inNode->mChannel])
            this->mChannels[inNode->mChannel]->Dereference();
        this->mChannels[inNode->mChannel] = NULL;
    }
    inNode->mChannel = -2;
    this->AddPrioritizedNodeToPausedQueue(inNode);
}

// FUNCTION: LLE 0x1004f6ac
// Pauses a free instance of the sample (one not on a channel) as looping or, without one, queues a new
// sample-less instance; returns its instance ID and the referenced node
unsigned long SESoundManager::AddNewSampleToPausedQueue(unsigned long inSampleID, float inVolume, float inPan,
                                                        float inPitch, SESMInstanceNode*& outNode)
{
    unsigned long theInstanceID = -1;
    outNode = NULL;
    unsigned long theMaxInstances = this->GetMaxInstances(inSampleID);
    std::vector<SESMInstanceNode*>* theInstances = this->mSounds[inSampleID].mInstances;
    if (theInstances) {
        SESMInstanceNode* theNode = (*theInstances)[0];
        for (unsigned long i = 0; i < theMaxInstances; i++) {
            theNode = (*theInstances)[i];
            if (theNode->mChannel != -1)
                continue;
            theNode->mSample->SetIsLooping(true);
            theNode->mInstanceID = this->mNextInstanceID++;
            theInstanceID = theNode->mInstanceID;
            theNode->mInstanceIndex = i;
            this->AddInstanceToPausedQueue(theNode);
            this->mCategories[theNode->mCategory]->mNumberOfInstances++;
            outNode = theNode;
            theNode->Reference();
            this->SetVolume2(theNode, inVolume);
            this->SetPan2(theNode, inPan);
            this->SetPitch2(theNode, inPitch);
            break;
        }
        if (theInstanceID == -1) {
            SESMInstanceNode* theNewNode = SESoundManager::GetNewSoundInstance();
            theNewNode->mCategory = theNode->mCategory;
            theNewNode->mInstanceID = this->mNextInstanceID++;
            theNewNode->mSample = NULL;
            theNewNode->mPan = inPan;
            theNewNode->mPitch = inPitch;
            theNewNode->mSampleID = inSampleID;
            theNewNode->mChannel = -2;
            theNewNode->mMaxInstances = theNode->mMaxInstances;
            theNewNode->mVolume = inVolume;
            this->mCategories[theNewNode->mCategory]->mNumberOfInstances++;
            this->AddPrioritizedNodeToPausedQueue(theNewNode);
            theInstanceID = theNewNode->mInstanceID;
            outNode = theNewNode;
            theNewNode->Reference();
            theNewNode->Dereference();
        }
    }
    return theInstanceID;
}

// FUNCTION: LLE 0x1004f81b
// Inserts the node in the paused queue before the first node of no larger priority value and maps its instance
// ID to it; the queue and the map each hold a reference
void SESoundManager::AddPrioritizedNodeToPausedQueue(SESMInstanceNode* inNode)
{
    unsigned long theCount = this->mPaused.size();
    unsigned long thePriority = this->GetCategoryPriority(inNode->mCategory);
    unsigned long i;
    bool theInserted = false;
    inNode->Reference();
    if (theCount > 0)
        for (i = 0; i < theCount; i++)
            if (this->GetCategoryPriority(this->mPaused[i]->mCategory) <= thePriority) {
                this->mPaused.insert(this->mPaused.begin() + i, inNode);
                theInserted = true;
                break;
            }
    if (!theInserted)
        this->mPaused.push_back(inNode);
    inNode->Reference();
    this->mPausedInstances[inNode->mInstanceID] = inNode;
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

// FUNCTION: LLE 0x1004f94b
// Stops the instance on the channel, rewinds its sample and frees the channel
void SESoundManager::DumpChannel(unsigned long inChannel)
{
    EE_LOST_DEBUG(2);  // LOST-DEBUG: none independent (owner, 2026-10-25: exact is exact); smallest n
    if (inChannel == -1)
        return;
    SESMInstanceNode* theNode = this->mChannels[inChannel];
    if (!theNode)
        return;
    this->mMixer->StopChannel(theNode->mSample->GetMixerChannel());
    this->mMixer->SetSampleOffset(theNode->mSample, 0);
    this->mCategories[theNode->mCategory]->mNumberOfInstances--;
    theNode->mChannel = -1;
    this->mChannels[inChannel]->Dereference();
    this->mChannels[inChannel] = NULL;
}

// FUNCTION: LLE 0x1004f9bd
// Frees a channel playing a lower-priority sound (the one closest to its end; a looping one is paused, not
// dumped), else an equal-priority one that ends before the new sample would; returns the channel (-1: none)
unsigned long SESoundManager::DumpLowerPrioritySample(unsigned long inSampleID, bool inLooping)
{
    unsigned long theChannel = -1;
    unsigned long thePriority = this->mCategories[this->GetCategory(inSampleID)]->mPriority;
    SESMDumpCandidate theLower;
    SESMDumpCandidate theEqual;
    theLower.mChannel = -1;
    theLower.mIsLooping = false;
    theEqual.mChannel = -1;
    theEqual.mIsLooping = false;
    theLower.mTimeLeft = FLT_MAX;
    theEqual.mTimeLeft = FLT_MAX;
    for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
        SESMInstanceNode* theNode = this->mChannels[i];
        if (!theNode)
            continue;
        if (this->mCategories[theNode->mCategory]->mPriority > thePriority) {
            if (theNode->mSample->GetIsLooping()) {
                if (theLower.mChannel == -1) {
                    theLower.mChannel = i;
                    theLower.mIsLooping = true;
                    theLower.mTimeLeft = FLT_MAX;
                }
            } else {
                float theTimeLeft = this->GetTimeLeftInMilli(theNode) * 0.001f;
                if (theTimeLeft < theLower.mTimeLeft) {
                    theLower.mIsLooping = false;
                    theLower.mTimeLeft = theTimeLeft;
                    theLower.mChannel = i;
                }
            }
        } else if (this->mCategories[theNode->mCategory]->mPriority == thePriority) {
            if (theNode->mSample->GetIsLooping()) {
                if (theEqual.mChannel == -1) {
                    theEqual.mChannel = i;
                    theEqual.mIsLooping = true;
                    theEqual.mTimeLeft = FLT_MAX;
                }
            } else {
                float theTimeLeft = this->GetTimeLeftInMilli(theNode) * 0.001f;
                if (theTimeLeft < theEqual.mTimeLeft) {
                    theEqual.mIsLooping = false;
                    theEqual.mTimeLeft = theTimeLeft;
                    theEqual.mChannel = i;
                }
            }
        }
    }
    if (theLower.mChannel != -1) {
        if (theLower.mIsLooping == true)
            this->AddInstanceToPausedQueue(this->mChannels[theLower.mChannel]);
        else
            this->DumpChannel(theLower.mChannel);
        theChannel = theLower.mChannel;
    } else if (theEqual.mChannel != -1) {
        if (theEqual.mIsLooping == true) {
            this->AddInstanceToPausedQueue(this->mChannels[theEqual.mChannel]);
            theChannel = theEqual.mChannel;
        } else if (!inLooping && this->mSounds[inSampleID].mInstances) {
            if (this->GetTotalPlayTimeInMilli(this->mSounds[inSampleID].mInstances->front()) * 0.001f > theEqual.mTimeLeft) {
                this->DumpChannel(theEqual.mChannel);
                theChannel = theEqual.mChannel;
            }
        }
    }
    return theChannel;
}

// FUNCTION: LLE 0x1004fb66
// Frees the channels whose (non-looping) sample finished; returns the last channel freed (-1: none)
unsigned long SESoundManager::DumpNonPlayingChannels()
{
    unsigned long theChannel = -1;
    for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
        SESMInstanceNode* theNode = this->mChannels[i];
        if (theNode && this->mMixer->IsChannelBusyPlaying(theNode->mSample->GetMixerChannel()) == 6 &&  // idle
            !theNode->mSample->GetIsLooping()) {
            this->DumpChannel(i);
            theChannel = i;
        }
    }
    return theChannel;
}

// FUNCTION: LLE 0x1004fbbc
// Returns the category of the loaded sample (-1: not loaded)
unsigned long SESoundManager::GetCategory(unsigned long inSampleID)
{
    unsigned long theCategory = -1;
    if (this->mSounds[inSampleID].mInstances)
        theCategory = (*this->mSounds[inSampleID].mInstances)[0]->mCategory;
    return theCategory;
}

// FUNCTION: LLE 0x1004fbdc
// Returns the category's priority (a larger value is a lower priority; -1: no such category)
unsigned long SESoundManager::GetCategoryPriority(unsigned long inCategory)
{
    unsigned long thePriority = -1;
    if (this->mCategories[inCategory])
        thePriority = this->mCategories[inCategory]->mPriority;
    return thePriority;
}

// FUNCTION: LLE 0x1004fbf3
// Returns the first channel with no instance on it (-1: none)
unsigned long SESoundManager::GetFreeChannel()
{
    unsigned long theChannel = -1;
    for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
        if (this->mChannels[i] == NULL) {
            theChannel = i;
            break;
        }
    }
    return theChannel;
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
    unsigned long theIndex;
    if (inInstanceID != -1) {
        this->mLock->Enter();
        SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
        if (theNode || (theNode = this->GetPausedInstance(inInstanceID, NULL)) != NULL) {
            if (theNode->mSample)
                theLooping = theNode->mSample->GetIsLooping();
            else
                theLooping = true;
        }
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
        if (inNode->mSample)
            theLooping = inNode->mSample->GetIsLooping();
        else
            theLooping = true;
        this->mLock->Leave();
    }
    return theLooping;
}

// FUNCTION: LLE 0x1004fcda
// Returns whether the playing or paused instance has its sample on a mixer channel
bool SESoundManager::GetIsPlaying(unsigned long inInstanceID)
{
    bool thePlaying = false;
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if ((theNode || (theNode = this->GetPausedInstance(inInstanceID, NULL)) != NULL) && theNode->mSample &&
        theNode->mSample->GetMixerChannel() != 0 && theNode->mSample->GetMixerChannel() != -1)
        thePlaying = true;
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
        if (inNode->mSample) {
            if (inNode->mSample->GetMixerChannel() != 0 && inNode->mSample->GetMixerChannel() != -1)
                thePlaying = true;
            else
                thePlaying = false;
        }
        this->mLock->Leave();
    }
    return thePlaying;
}

// FUNCTION: LLE 0x1004fd7e
// Returns the instance limit of the loaded sample's category (0: not loaded)
unsigned long SESoundManager::GetMaxCategoryInstances(unsigned long inSampleID)
{
    unsigned long theCount = 0;
    unsigned long theCategory;
    if (this->mSounds[inSampleID].mInstances && (theCategory = this->GetCategory(inSampleID)) != -1)
        theCount = this->mCategories[theCategory]->mMaxInstances;
    return theCount;
}

// FUNCTION: LLE 0x1004fdb1
// Returns how many instances of the sample were loaded (0: not loaded)
unsigned long SESoundManager::GetMaxInstances(unsigned long inSampleID)
{
    unsigned long theCount = 0;
    if (this->mSounds[inSampleID].mInstances)
        theCount = this->mSounds[inSampleID].mInstances->size();
    return theCount;
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

// FUNCTION: LLE 0x1004fe05
// Returns how many instances of the loaded sample's category are playing or paused (0: not loaded)
unsigned long SESoundManager::GetNumberOfCategoryInstances(unsigned long inSampleID)
{
    unsigned long theCount = 0;
    unsigned long theCategory;
    if (this->mSounds[inSampleID].mInstances && (theCategory = this->GetCategory(inSampleID)) != -1)
        theCount = this->mCategories[theCategory]->mNumberOfInstances;
    return theCount;
}

// FUNCTION: LLE 0x1004fe38
// Counts the sample's instances on a channel or paused, plus its sample-less paused instances
unsigned long SESoundManager::GetNumberOfInstances(unsigned long inSampleID)
{
    unsigned long theCount = 0;
    std::vector<SESMInstanceNode*>* theInstances = this->mSounds[inSampleID].mInstances;
    if (theInstances) {
        unsigned long i;
        for (i = 0; i < theInstances->size(); i++)
            if ((*theInstances)[i]->mChannel != -1)
                theCount++;
        for (i = 0; i < this->mPaused.size(); i++)
            if (this->mPaused[i]->mSampleID == inSampleID && this->mPaused[i]->mSample == NULL)
                theCount++;
    }
    return theCount;
}

// FUNCTION: LLE 0x1004feb7
// Returns the pan of the playing or paused instance (0: no such instance)
float SESoundManager::GetPan(unsigned long inInstanceID)
{
    float thePan;
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            thePan = theNode->mSample->GetPan();
        else
            thePan = theNode->mPan;
    } else
        thePan = 0.0f;
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
        if (inNode->mSample)
            thePan = inNode->mSample->GetPan();
        else
            thePan = inNode->mPan;
        this->mLock->Leave();
    }
    return thePan;
}

// FUNCTION: LLE 0x1004ff59
// Returns the pitch of the playing or paused instance (1: no such instance)
float SESoundManager::GetPitch(unsigned long inInstanceID)
{
    float thePitch;
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            thePitch = theNode->mSample->GetPitch();
        else
            thePitch = theNode->mPitch;
    } else
        thePitch = 1.0f;
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
        if (inNode->mSample)
            thePitch = inNode->mSample->GetPitch();
        else
            thePitch = inNode->mPitch;
        this->mLock->Leave();
    }
    return thePitch;
}

// FUNCTION: LLE 0x1004fffb
// Finds the instance on a channel by instance ID and returns it and its channel (NULL: not playing)
SESMInstanceNode* SESoundManager::GetPlayingInstance(unsigned long inInstanceID, unsigned long* outChannel)
{
    SESMInstanceNode* theResult = NULL;
    if (inInstanceID != -1) {
        for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
            SESMInstanceNode* theNode = this->mChannels[i];
            if (!theNode || theNode->mInstanceID != inInstanceID)
                continue;
            *outChannel = i;
            theResult = theNode;
            break;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1005003b
// Finds the paused instance by ID: through the instance map, or (with outIndex) by scanning the queue for its index
SESMInstanceNode* SESoundManager::GetPausedInstance(unsigned long inInstanceID, unsigned long* outIndex)
{
    SESMInstanceNode* theNode = NULL;
    unsigned long theCount;
    if (inInstanceID != -1) {
        if (outIndex == NULL) {
            std::map<unsigned long, SESMInstanceNode*>::iterator theEntry = this->mPausedInstances.find(inInstanceID);
            if (theEntry != this->mPausedInstances.end())
                theNode = theEntry->second;
        } else {
            theCount = this->mPaused.size();
            for (unsigned long i = 0; i < theCount; i++) {
                SESMInstanceNode* thePaused = this->mPaused[i];
                if (!thePaused || thePaused->mInstanceID != inInstanceID)
                    continue;
                *outIndex = i;
                theNode = thePaused;
                break;
            }
        }
    } else
        theCount = this->mPaused.size();
    return theNode;
}

// FUNCTION: LLE 0x100500ce
// Returns a random start offset within the sample (for looping sounds)
unsigned long SESoundManager::GetRandomOffset(SESample* inSample)
{
    unsigned long theOffset = 0;
    if (inSample) {
        float theRandom = this->mRandom.Generate() * inSample->GetLengthInSamples();
        theOffset = UMFloatToLong(theRandom);
    }
    return theOffset;
}

// FUNCTION: LLE 0x10050100
// Returns the sample ID of the playing or paused instance (-1: no such instance)
unsigned long SESoundManager::GetSampleID(unsigned long inInstanceID)
{
    unsigned long theIndex = 0;
    unsigned long theID = -1;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode || (theNode = this->GetPausedInstance(inInstanceID, NULL)) != NULL)
        theID = theNode->mSampleID;
    this->mLock->Leave();
    return theID;
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
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    unsigned long theRate = 0;
    if (theNode || (theNode = this->GetPausedInstance(inInstanceID, NULL)) != NULL) {
        if (theNode->mSample)
            theRate = theNode->mSample->GetSampleRate();
        else
            theRate = (*this->mSounds[theNode->mSampleID].mInstances)[0]->mSample->GetSampleRate();
    }
    this->mLock->Leave();
    return theRate;
}

// FUNCTION: LLE 0x100501eb
// Returns the playing or paused instance with an added reference (NULL: none)
SESMInstanceNode* SESoundManager::GetAndReferenceSoundInstance(unsigned long inInstanceID)
{
    SESMInstanceNode* theNode = NULL;
    unsigned long theIndex;
    if (inInstanceID != -1) {
        this->mLock->Enter();
        theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
        if (theNode || (theNode = this->GetPausedInstance(inInstanceID, NULL)) != NULL)
            theNode->Reference();
        this->mLock->Leave();
    }
    return theNode;
}

// FUNCTION: LLE 0x10050241
// Returns the milliseconds left to play of the node's sample (-1: looping)
long SESoundManager::GetTimeLeftInMilli(SESMInstanceNode* inNode)
{
    long theTotal;
    long theRemaining;
    if (inNode->mSample->GetIsLooping() == true)
        theRemaining = -1;
    else
        this->mMixer->GetSamplePlayBackTime(inNode->mSample, theTotal, theRemaining);
    return theRemaining;
}

// FUNCTION: LLE 0x10050273
// Returns the milliseconds left to play of the playing or paused instance (0: none)
long SESoundManager::GetTimeLeftInMilli(unsigned long inInstanceID)
{
    long theTime = 0;
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode)
        theTime = this->GetTimeLeftInMilli(theNode);
    this->mLock->Leave();
    return theTime;
}

// FUNCTION: LLE 0x100502c6
// Returns the length in milliseconds of the sample's first instance, but only while an instance of it is
// playing or paused (else 0; -1: looping)
long SESoundManager::GetTotalPlayTimeInMilli(unsigned long inSampleID)
{
    long theTime = 0;
    this->mLock->Enter();
    if (this->GetNumberOfInstances(inSampleID) > 0) {
        SESMInstanceNode* theNode = (*this->mSounds[inSampleID].mInstances)[0];
        if (theNode && theNode->mSample)
            theTime = this->GetTotalPlayTimeInMilli(theNode);
    }
    this->mLock->Leave();
    return theTime;
}

// FUNCTION: LLE 0x1005031d
// Returns the length in milliseconds of the node's sample (-1: looping)
long SESoundManager::GetTotalPlayTimeInMilli(SESMInstanceNode* inNode)
{
    long theTotal;
    long theRemaining;
    if (inNode->mSample->GetIsLooping() == true)
        theTotal = -1;
    else
        this->mMixer->GetSamplePlayBackTime(inNode->mSample, theTotal, theRemaining);
    return theTotal;
}

// FUNCTION: LLE 0x1005034f
// Returns the instance's volume under the lock
float SESoundManager::GetVolume(unsigned long inInstanceID)
{
    this->mLock->Enter();
    float theVolume = this->GetVolume2(inInstanceID);
    // Fixed: the lock used to be entered a second time here instead of left, so it was never released
    this->mLock->Leave();
    return theVolume;
}

// FUNCTION: LLE 0x10050384
// Returns the volume of the playing or paused instance (1: no such instance); the caller holds the lock
float SESoundManager::GetVolume2(unsigned long inInstanceID)
{
    float theVolume;
    unsigned long theIndex;
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            theVolume = theNode->mSample->GetVolume();
        else
            theVolume = theNode->mVolume;
    } else
        theVolume = 1.0f;
    return theVolume;
}

// FUNCTION: LLE 0x100503c2
// Returns the volume of the node's instance (1: no node); the caller holds the lock
float SESoundManager::GetVolume2(SESMInstanceNode* inNode)
{
    float theVolume = 1.0f;
    if (inNode) {
        if (inNode->mSample)
            theVolume = inNode->mSample->GetVolume();
        else
            theVolume = inNode->mVolume;
    }
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
    SESMSound theSound;
    UString theName(inName);
    unsigned long theSampleID = -1;
    FSFileSpec* theFileSpec = NULL;
    this->mLock->Enter();
    if (inCategory < this->mCategoryCount && inMaxInstances > 0 && this->mPath->FindFile(theName, theFileSpec) == kErrorOK) {
        std::vector<SESMInstanceNode*>* theInstances = NULL;
        for (theSampleID = 0; theSampleID < this->mSoundCount; theSampleID++) {
            if (this->mSounds[theSampleID].mName == inName) {
                theInstances = this->mSounds[theSampleID].mInstances;
                break;
            }
        }
        if (theInstances == NULL)
            theInstances = new std::vector<SESMInstanceNode*>;
        inMaxInstances = min(this->mAvailableChannels, inMaxInstances);
        if (theInstances->size() < inMaxInstances) {
            for (unsigned long i = theInstances->size(); i < inMaxInstances; i++) {
                SESMInstanceNode* theNode = SESoundManager::GetNewSoundInstance();
                theNode->mCategory = inCategory;
                theNode->mChannel = -1;
                theNode->mInstanceID = -1;
                theNode->mSampleID = theSampleID;
                theNode->mMaxInstances = inMaxInstances;
                if (i != 0) {
                    theNode->mSample = NULL;
                    this->mMixer->DuplicateSample(*(*theInstances)[0]->mSample, theNode->mSample);
                } else {
                    theNode->mSample = new SESample;
                    theNode->mSample->LoadWaveData(theFileSpec);
                }
                theInstances->push_back(theNode);
            }
        }
        if (theSampleID == this->mSoundCount) {
            theSound.mName = inName;
            theSound.mInstances = theInstances;
            theSound.mLoadCount = 1;
            this->mSounds.push_back(theSound);
        } else {
            this->mSounds[theSampleID].mInstances = theInstances;
            this->mSounds[theSampleID].mLoadCount++;
        }
        this->mSoundCount = this->mSounds.size();
        if (this->mCategories[inCategory] == NULL) {
            this->mCategories[inCategory] = new SESMCategory;
            this->mCategories[inCategory]->mMaxInstances = 0;
            this->mCategories[inCategory]->mNumberOfInstances = 0;
            this->mCategories[inCategory]->mPriority = -1;
        }
        if (theFileSpec)
            delete theFileSpec;
    }
    this->mLock->Leave();
    return theSampleID;
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
// an instance of the sample or its category is replaced) and plays on it; a looping one with none is queued paused
SESMInstanceNode* SESoundManager::PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan,
                                                              float inPitch, bool inLooping, char* inDebugName)
{
    SESMInstanceNode* theNode = NULL;
    unsigned long theChannel;
    if (this->mSounds.size() == 0 || inSampleID > this->mSounds.size() - 1 || this->mSounds[inSampleID].mInstances == NULL ||
        (inVolume == 0.0f && !inLooping))
        return NULL;
    this->mLock->Enter();
    if (this->GetNumberOfInstances(inSampleID) >= this->GetMaxInstances(inSampleID) ||
        this->GetNumberOfCategoryInstances(inSampleID) >= this->GetMaxCategoryInstances(inSampleID))
        this->DumpNonPlayingChannels();
    if (this->GetNumberOfInstances(inSampleID) < this->GetMaxInstances(inSampleID) &&
        this->GetNumberOfCategoryInstances(inSampleID) < this->GetMaxCategoryInstances(inSampleID)) {
        theChannel = this->GetFreeChannel();
        if (theChannel == -1)
            theChannel = this->DumpNonPlayingChannels();
        if (theChannel == -1)
            theChannel = this->DumpLowerPrioritySample(inSampleID, inLooping);
        if (theChannel == -1) {
            if (inLooping == true)
                this->AddNewSampleToPausedQueue(inSampleID, inVolume, inPan, inPitch, theNode);
        } else
            this->ReallyPlayIt(theChannel, inSampleID, inVolume, inPan, inPitch, inLooping, theNode);
    } else if (inLooping == true)
        this->AddNewSampleToPausedQueue(inSampleID, inVolume, inPan, inPitch, theNode);
    else {
        if (this->GetNumberOfInstances(inSampleID) >= this->GetMaxInstances(inSampleID))
            theChannel = this->ReplaceNonLoopingInstance(inSampleID);
        else
            theChannel = this->ReplaceCategoryInstance(inSampleID);
        if (theChannel != -1)
            this->ReallyPlayIt(theChannel, inSampleID, inVolume, inPan, inPitch, inLooping, theNode);
    }
    this->mLock->Leave();
    return theNode;
}

// FUNCTION: LLE 0x100508e7
// Puts the sample's first instance not on a channel on the given channel with the volume, pan and pitch, and
// starts it (a looping one at a random offset); returns its instance ID and the referenced node
unsigned long SESoundManager::ReallyPlayIt(unsigned long inChannel, unsigned long inSampleID, float inVolume,
                                           float inPan, float inPitch, bool inLooping, SESMInstanceNode*& outNode)
{
    EE_LOST_DEBUG(1);  // LOST-DEBUG (PROVISIONAL, owner experiment 2026-10-24): no independent evidence; base-game LLE identical (exact only at n = 1)
    unsigned long theInstanceID = -1;
    outNode = NULL;
    unsigned long theMaxInstances = this->GetMaxInstances(inSampleID);
    std::vector<SESMInstanceNode*>* theInstances = this->mSounds[inSampleID].mInstances;
    if (theInstances) {
        for (unsigned long i = 0; i < theMaxInstances; i++) {
            SESMInstanceNode* theNode = (*theInstances)[i];
            if (theNode->mChannel != -1)
                continue;
            this->mMixer->StopChannel(inChannel);
            this->mMixer->StopChannel(theNode->mSample->GetMixerChannel());
            theNode->mSample->SetMixerChannel(-1);
            if (this->mMixer->SetChannelSample(inChannel, theNode->mSample) != kErrorOK)
                continue;
            theNode->mInstanceID = this->mNextInstanceID++;
            theInstanceID = theNode->mInstanceID;
            theNode->mSample->SetIsLooping(inLooping);
            theNode->mInstanceIndex = i;
            outNode = theNode;
            theNode->Reference();
            if (inLooping)
                this->mMixer->SetSampleOffset(theNode->mSample, this->GetRandomOffset(theNode->mSample));
            this->mCategories[this->GetCategory(inSampleID)]->mNumberOfInstances++;
            theNode->mChannel = inChannel;
            if (this->mChannels[inChannel])
                this->mChannels[inChannel]->Dereference();
            this->mChannels[inChannel] = theNode;
            theNode->Reference();
            this->SetVolume2(theNode, inVolume);
            this->SetPan2(theNode, inPan);
            this->SetPitch2(theNode, inPitch);
            this->mMixer->PlayChannel(inChannel);
            break;
        }
    }
    return theInstanceID;
}

// FUNCTION: LLE 0x10050a9c
// Remembers a playing or paused instance as a one-shot sound, unloaded once it stops playing
long SESoundManager::RegisterOneShotSound(unsigned long inSampleID, unsigned long inInstanceID)
{
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode && theNode->mSample) {
        SESMOneShotSound theSound;
        theSound.mSample = theNode->mSample;
        theSound.mSampleID = inSampleID;
        theSound.mInstanceID = inInstanceID;
        this->mOneShotSounds.push_back(theSound);
    }
    this->mLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x10050b03
// Frees the channel of the instance (of the sample, else of its category) closest to its end; returns that
// channel, or an empty channel seen on the way (-1: none)
unsigned long SESoundManager::ReplaceCategoryInstance(unsigned long inSampleID)
{
    unsigned long theFreeChannel = -1;
    unsigned long theCategory = this->GetCategory(inSampleID);
    SESMInstanceNode* theCandidate = NULL;
    bool theFoundSample = false;
    for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
        SESMInstanceNode* theNode = this->mChannels[i];
        if (theNode) {
            if (theNode->mSampleID == inSampleID)
                theFoundSample = true;
            else if (theFoundSample || theNode->mCategory != theCategory)
                continue;
            if (theCandidate == NULL)
                theCandidate = theNode;
            else if (this->GetTimeLeftInMilli(theNode) < this->GetTimeLeftInMilli(theCandidate))
                theCandidate = theNode;
        } else
            theFreeChannel = i;
    }
    if (!theCandidate)
        return theFreeChannel;
    unsigned long theChannel = theCandidate->mChannel;
    if (theChannel != -1 && theChannel != -2) {
        if (theCandidate->mSampleID == inSampleID) {
            if (theCandidate->mSample->GetIsLooping())
                return theFreeChannel;
            this->DumpChannel(theChannel);
        } else if (theCandidate->mSample->GetIsLooping())
            this->AddInstanceToPausedQueue(theCandidate);
        else
            this->DumpChannel(theChannel);
    } else
        theChannel = -1;
    if (theChannel != -1)
        return theChannel;
    return theFreeChannel;
}

// FUNCTION: LLE 0x10050bdc
// Frees the channel of the quietest non-looping instance of the sample quieter than 0.01; returns it (-1: none)
unsigned long SESoundManager::ReplaceNonLoopingInstance(unsigned long inSampleID)
{
    unsigned long theChannel = -1;
    float theVolume = 0.01f;
    for (unsigned long i = 0; i < this->mAvailableChannels; i++) {
        SESMInstanceNode* theNode = this->mChannels[i];
        if (theNode && theNode->mSampleID == inSampleID && !theNode->mSample->GetIsLooping() &&
            this->GetVolume2(theNode) < theVolume) {
            theChannel = i;
            theVolume = this->GetVolume2(theNode);
        }
    }
    if (theChannel != -1)
        this->DumpChannel(theChannel);
    return theChannel;
}

// FUNCTION: LLE 0x10050c57
// Scavenger thread: every second retires finished or silent channels, detaches the samples of silent paused
// instances, and restarts the loudest paused instances (by category priority, then volume) on free channels
unsigned long SESoundManager::Scavenger(void* inThisObject)
{
    std::vector<unsigned long> theFreeChannels;
    std::vector<unsigned long> theUnused;
    SESoundManager* theManager = static_cast<SESoundManager*>(inThisObject);
    bool theDone = false;
    ESVERIFY_AT(inThisObject != NULL, 2626);
    HANDLE theHandles[1];
    theHandles[0] = theManager->mScavengerThreadExitEvent->mHandle;
    UMSetFloatingPointPrecision(_PC_53);
    do {
        if (UMGetFloatingPointPrecision() != _PC_53)
            UMSetFloatingPointPrecision(_PC_53);
        if (UMGetFloatingPointRoundingMode() != _RC_NEAR)
            UMSetFloatingPointRoundingMode(_RC_NEAR);
        theManager->UpdateOneShotSounds();
        switch (WaitForMultipleObjects(1, theHandles, FALSE, 1000)) {
        case WAIT_OBJECT_0:
            theManager->mScavengerThreadExitEvent->Clear();
            theDone = true;
            break;
        case WAIT_TIMEOUT: {
            theManager->mLock->Enter();
            unsigned long theChannelCount = theManager->mAvailableChannels;
            if (theFreeChannels.size() != theChannelCount)
                theFreeChannels.resize(theChannelCount);
            unsigned long theFreeCount = 0;
            unsigned long i;
            for (i = 0; i < theChannelCount; i++) {
                SESMInstanceNode* theNode = theManager->mChannels[i];
                if (theNode == NULL)
                    theFreeChannels[theFreeCount++] = i;
                else if ((theManager->GetVolume2(theNode) < 0.01f ||
                          theManager->mMixer->IsChannelBusyPlaying(theNode->mSample->GetMixerChannel()) == 6) &&
                         theNode->mSample->GetIsLooping() == false) {
                    theManager->DumpChannel(i);
                    theFreeChannels[theFreeCount++] = i;
                } else if (theNode->mSample->GetIsLooping() == true && theNode->mChannel == i &&
                           (theManager->mIsMuted || theManager->GetVolume2(theNode) < 0.01f)) {
                    theManager->AddInstanceToPausedQueue(theNode);
                    theFreeChannels[theFreeCount++] = i;
                }
            }
            long thePausedCount;
            thePausedCount = theManager->mIsMuted ? 0 : theManager->mPaused.size();
            long j;
            for (j = 0; j < thePausedCount; j++) {
                SESMInstanceNode* theNode = theManager->mPaused[j];
                if (!theNode->mSample || !(theNode->mSample->GetVolume() < 0.01f))
                    continue;
                theNode->mChannel = -2;
                theNode->mSample->SetMixerChannel(-1);
                theNode->mSample->SetVolume(0.0f);
                if (theNode->mInstanceIndex != -1 && theManager->mSounds[theNode->mSampleID].mInstances) {
                    (*theManager->mSounds[theNode->mSampleID].mInstances)[theNode->mInstanceIndex] =
                        SESoundManager::GetNewSoundInstance();
                    (*theManager->mSounds[theNode->mSampleID].mInstances)[theNode->mInstanceIndex]->mSample =
                        theNode->mSample;
                    (*theManager->mSounds[theNode->mSampleID].mInstances)[theNode->mInstanceIndex]->mSampleID =
                        theNode->mSampleID;
                    (*theManager->mSounds[theNode->mSampleID].mInstances)[theNode->mInstanceIndex]->mCategory =
                        theNode->mCategory;
                    (*theManager->mSounds[theNode->mSampleID].mInstances)[theNode->mInstanceIndex]->mMaxInstances =
                        theNode->mMaxInstances;
                    theNode->Dereference();
                }
                theNode->mInstanceIndex = -1;
                theNode->mSample = NULL;
            }
            unsigned long thePausedSize;
            if (!theManager->mIsMuted && (thePausedSize = theManager->mPaused.size()) != 0 && theFreeCount > 0) {
                std::sort(theManager->mPaused.begin(), theManager->mPaused.end(), SESMPausedOrder(theManager));
                unsigned long theFreeIndex = theFreeCount - 1;
                for (j = thePausedSize - 1; j >= 0; j--) {
                    if (theFreeCount == 0)
                        break;
                    SESMInstanceNode* theNode = theManager->mPaused[j];
                    float theVolume = theNode->mSample ? theNode->mSample->GetVolume() : theNode->mVolume;
                    if (theVolume > 0.01f) {
                        if (theNode->mSample) {
                            theManager->mMixer->SetSampleOffset(theNode->mSample,
                                                                theManager->GetRandomOffset(theNode->mSample));
                            theManager->mMixer->SetChannelSample(theFreeChannels[theFreeIndex], theNode->mSample);
                            theManager->mMixer->PlayChannel(theFreeChannels[theFreeIndex]);
                            theNode->mChannel = theFreeChannels[theFreeIndex];
                            if (theManager->mChannels[theFreeChannels[theFreeIndex]])
                                theManager->mChannels[theFreeChannels[theFreeIndex]]->Dereference();
                            theManager->mChannels[theFreeChannels[theFreeIndex]] = theNode;
                            theNode->Reference();
                            theManager->mPaused.erase(theManager->mPaused.begin() + j);
                            theFreeCount--;
                            theFreeIndex--;
                            theManager->mPausedInstances.erase(theNode->mInstanceID);
                            theNode->Dereference();
                            theNode->Dereference();
                        } else {
                            unsigned long theSampleID = theNode->mSampleID;
                            unsigned long theMaxInstances = theManager->GetMaxInstances(theSampleID);
                            std::vector<SESMInstanceNode*>* theInstances = theManager->mSounds[theSampleID].mInstances;
                            if (theInstances) {
                                for (unsigned long k = 0; k < theMaxInstances; k++) {
                                    SESMInstanceNode* theOther = (*theInstances)[k];
                                    if (theOther->mChannel != -1)
                                        continue;
                                    theNode->mSample = theOther->mSample;
                                    theOther->mSample = NULL;
                                    theNode->mSample->SetIsLooping(true);
                                    theNode->mInstanceIndex = k;
                                    theNode->mChannel = theFreeChannels[theFreeIndex];
                                    if (theManager->mChannels[theNode->mChannel])
                                        theManager->mChannels[theNode->mChannel]->Dereference();
                                    theManager->mChannels[theNode->mChannel] = theNode;
                                    theNode->Reference();
                                    theManager->SetVolume2(theNode, theNode->mVolume);
                                    theManager->SetPan2(theNode, theNode->mPan);
                                    theManager->SetPitch2(theNode, theNode->mPitch);
                                    theManager->mMixer->SetSampleOffset(theNode->mSample,
                                                                        theManager->GetRandomOffset(theNode->mSample));
                                    theManager->mMixer->SetChannelSample(theNode->mChannel, theNode->mSample);
                                    theManager->mMixer->PlayChannel(theNode->mChannel);
                                    theOther->Dereference();
                                    (*theInstances)[k] = theNode;
                                    theNode->Reference();
                                    theNode->Dereference();
                                    theManager->mPaused.erase(theManager->mPaused.begin() + j);
                                    theFreeCount--;
                                    theFreeIndex--;
                                    theManager->mPausedInstances.erase(theNode->mInstanceID);
                                    theNode->Dereference();
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            theManager->mLock->Leave();
            Sleep(1);
            break;
        }
        case WAIT_FAILED:
            GetLastError();
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
    float theCount = this->mAvailableChannels * inPercent;
    unsigned long theMaxInstances = UMFloatToLong(theCount);
    if (theMaxInstances == 0)
        theMaxInstances = 1;
    if (this->mCategories[inCategory] == NULL) {
        this->mCategories[inCategory] = new SESMCategory;
        this->mCategories[inCategory]->mNumberOfInstances = 0;
        // Fixed: the priority used to be left uninitialised (Load's -1 never ran for these categories)
        this->mCategories[inCategory]->mPriority = -1;
    }
    this->mCategories[inCategory]->mMaxInstances = theMaxInstances;
    this->mCategories[inCategory]->mPercentage = inPercent;
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
    long theResult = kErrorOK;
    unsigned long i;
    SEChannelDescription theDescription;
    this->mLock->Enter();
    if (inCount == 0)
        theResult = kErrorFailed;
    else if (inCount != this->mAvailableChannels) {
        for (i = 0; i < this->mChannels.size(); i++) {
            SESMInstanceNode* theNode = this->mChannels[i];
            if (!theNode)
                continue;
            if (theNode->mSample->GetIsLooping() == true)
                this->AddInstanceToPausedQueue(theNode);
            else
                this->DumpChannel(i);
        }
        this->mMixer->DestroyChannels();
        theResult = this->mMixer->CreateChannels(inCount, &theDescription);
        this->mAvailableChannels = inCount;
        for (i = 0; i < this->mChannels.size(); i++) {
            SESMInstanceNode* theNode = this->mChannels[i];
            if (theNode)
                theNode->Dereference();
        }
        this->mChannels.clear();
        this->mChannels.resize(this->mAvailableChannels);
        for (i = 0; i < this->mAvailableChannels; i++)
            this->mChannels[i] = NULL;
        for (i = 0; i < this->mCategories.size(); i++)
            this->SetCategoryPercentage(i, this->mCategories[i]->mPercentage);
    }
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
    long theResult = this->SetPan2(inInstanceID, inPan);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100517d1
// Sets the pan of the node's instance, under the lock
long SESoundManager::SetPan(SESMInstanceNode* inNode, float inPan)
{
    this->mLock->Enter();
    long theResult = this->SetPan2(inNode, inPan);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051809
// Sets the pan of the playing or paused instance (on its sample, else on the node); the caller holds the lock
long SESoundManager::SetPan2(unsigned long inInstanceID, float inPan)
{
    long theResult = kErrorOK;
    unsigned long theIndex;
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            theResult = theNode->mSample->SetPan(inPan);
        else
            theNode->mPan = inPan;
    }
    return theResult;
}

// FUNCTION: LLE 0x1005185e
// Sets the pan of the node's instance (on its sample, else on the node); the caller holds the lock
long SESoundManager::SetPan2(SESMInstanceNode* inNode, float inPan)
{
    long theResult = kErrorOK;
    if (!inNode)
        return theResult;
    if (inNode->mSample)
        theResult = inNode->mSample->SetPan(inPan);
    else
        inNode->mPan = inPan;
    return theResult;
}

// FUNCTION: LLE 0x1005188f
// Sets the pitch of the playing or paused instance, under the lock
long SESoundManager::SetPitch(unsigned long inInstanceID, float inPitch)
{
    this->mLock->Enter();
    long theResult = this->SetPitch2(inInstanceID, inPitch);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100518c7
// Sets the pitch of the node's instance, under the lock
long SESoundManager::SetPitch(SESMInstanceNode* inNode, float inPitch)
{
    this->mLock->Enter();
    long theResult = this->SetPitch2(inNode, inPitch);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x100518ff
// Sets the pitch of the playing or paused instance (on its sample, else on the node); the caller holds the lock
long SESoundManager::SetPitch2(unsigned long inInstanceID, float inPitch)
{
    long theResult = kErrorOK;
    unsigned long theIndex;
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            theResult = theNode->mSample->SetPitch(inPitch);
        else
            theNode->mPitch = inPitch;
    }
    return theResult;
}

// FUNCTION: LLE 0x10051954
// Sets the pitch of the node's instance (on its sample, else on the node); the caller holds the lock
long SESoundManager::SetPitch2(SESMInstanceNode* inNode, float inPitch)
{
    long theResult = kErrorOK;
    if (!inNode)
        return theResult;
    if (inNode->mSample)
        theResult = inNode->mSample->SetPitch(inPitch);
    else
        inNode->mPitch = inPitch;
    return theResult;
}

// FUNCTION: LLE 0x10051985
// Sets the category's priority (a larger value is a lower priority); -1 if the category does not exist
long SESoundManager::SetCategoryPriority(unsigned long inCategory, unsigned long inPriority)
{
    long theResult = -1;
    if (this->mCategories[inCategory]) {
        this->mCategories[inCategory]->mPriority = inPriority;
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: LLE 0x100519a2
// Sets the volume, pan and pitch of the playing or paused instance; returns the first sample error
long SESoundManager::SetSoundParameters(unsigned long inInstanceID, float inVolume, float inPan, float inPitch)
{
    long theResult = kErrorOK;
    unsigned long theIndex;
    this->mLock->Enter();
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        theNode->mVolume = inVolume;
        theNode->mPitch = inPitch;
        theNode->mPan = inPan;
        if (theNode->mSample) {
            theResult = theNode->mSample->SetVolume(inVolume);
            if (theResult == kErrorOK) {
                theResult = theNode->mSample->SetPitch(inPitch);
                if (theResult == kErrorOK)
                    theResult = theNode->mSample->SetPan(inPan);
            }
        }
    } else
        theResult = kErrorOK;
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
        inNode->mVolume = inVolume;
        inNode->mPitch = inPitch;
        inNode->mPan = inPan;
        if (inNode->mSample) {
            theResult = inNode->mSample->SetVolume(inVolume);
            if (theResult == kErrorOK) {
                theResult = inNode->mSample->SetPitch(inPitch);
                if (theResult == kErrorOK)
                    theResult = inNode->mSample->SetPan(inPan);
            }
        }
        this->mLock->Leave();
    } else
        theResult = kErrorOK;
    return theResult;
}

// FUNCTION: LLE 0x10051af7
// Sets the volume of the playing or paused instance, under the lock
long SESoundManager::SetVolume(unsigned long inInstanceID, float inVolume)
{
    this->mLock->Enter();
    long theResult = this->SetVolume2(inInstanceID, inVolume);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051b2f
// Sets the volume of the node's instance, under the lock
long SESoundManager::SetVolume(SESMInstanceNode* inNode, float inVolume)
{
    this->mLock->Enter();
    long theResult = this->SetVolume2(inNode, inVolume);
    this->mLock->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10051b67
// Sets the volume of the playing or paused instance on its sample and, unlike SetPan2/SetPitch2, on the node too
long SESoundManager::SetVolume2(unsigned long inInstanceID, float inVolume)
{
    long theResult = kErrorOK;
    unsigned long theIndex;
    SESMInstanceNode* theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode == NULL)
        theNode = this->GetPausedInstance(inInstanceID, NULL);
    if (theNode) {
        if (theNode->mSample)
            theResult = theNode->mSample->SetVolume(inVolume);
        theNode->mVolume = inVolume;
    }
    return theResult;
}

// FUNCTION: LLE 0x10051bc0
// Sets the volume of the node's instance (on its sample, else on the node); the caller holds the lock
long SESoundManager::SetVolume2(SESMInstanceNode* inNode, float inVolume)
{
    long theResult = kErrorOK;
    if (!inNode)
        return theResult;
    if (inNode->mSample)
        theResult = inNode->mSample->SetVolume(inVolume);
    else
        inNode->mVolume = inVolume;
    return theResult;
}

// FUNCTION: LLE 0x10051bf1
// Stops the playing or paused instance and frees its channel or queue slot, or with inLetFinish only ends its
// looping so it plays out
// inLetFinish: only clear the looping flag and let the sample play out (StopAllOneShotSounds passes false).
long SESoundManager::Stop(unsigned long inInstanceID, bool inLetFinish)
{
    unsigned long theIndex = 0;
    SESMInstanceNode* theNode;
    if (inInstanceID == -1)
        return kErrorOK;
    this->mLock->Enter();
    theNode = this->GetPlayingInstance(inInstanceID, &theIndex);
    if (theNode) {
        if (!inLetFinish) {
            this->mMixer->StopChannel(theNode->mSample->GetMixerChannel());
            this->mMixer->SetSampleOffset(theNode->mSample, 0);
            theNode->mChannel = -1;
            this->mCategories[theNode->mCategory]->mNumberOfInstances--;
            if (this->mChannels[theIndex])
                this->mChannels[theIndex]->Dereference();
            this->mChannels[theIndex] = NULL;
        } else
            theNode->mSample->SetIsLooping(false);
    } else {
        theNode = this->GetPausedInstance(inInstanceID, &theIndex);
        if (theNode) {
            if (theNode->mSample) {
                if (!inLetFinish) {
                    this->mMixer->StopChannel(theNode->mSample->GetMixerChannel());
                    this->mMixer->SetSampleOffset(theNode->mSample, 0);
                    theNode->mChannel = -1;
                    this->mCategories[theNode->mCategory]->mNumberOfInstances--;
                    this->mPaused.erase(this->mPaused.begin() + theIndex);
                    this->mPausedInstances.erase(inInstanceID);
                    theNode->Dereference();
                    theNode->Dereference();
                } else
                    theNode->mSample->SetIsLooping(false);
            } else {
                this->mCategories[theNode->mCategory]->mNumberOfInstances--;
                this->mPaused.erase(this->mPaused.begin() + theIndex);
                this->mPausedInstances.erase(inInstanceID);
                theNode->Dereference();
                theNode->Dereference();
            }
        }
    }
    this->mLock->Leave();
    return kErrorOK;
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
    if (theInstanceID != -1)
        this->Stop(theInstanceID, inImmediately);
    return kErrorOK;
}

// FUNCTION: LLE 0x10051e72
// Stops and unloads every remembered one-shot sound
long SESoundManager::StopAllOneShotSounds()
{
    this->mLock->Enter();
    std::vector<SESMOneShotSound>::iterator theSound = this->mOneShotSounds.begin();
    while (theSound != this->mOneShotSounds.end()) {
        this->Stop(theSound->mInstanceID, false);
        this->Unload(theSound->mSampleID);
        theSound = this->mOneShotSounds.erase(theSound);
    }
    this->mLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x10051ece
// Unloads one load of the sample; at the last drops its paused instances, frees its channels and deletes its
// instances and samples; fails for an unknown sample
long SESoundManager::Unload(unsigned long inSampleID)
{
    SESMInstanceNode* theNode = NULL;
    unsigned long i;
    if (this->mSounds.size() == 0 || !(inSampleID <= this->mSounds.size() - 1) || !this->mSounds[inSampleID].mInstances)
        return kErrorFailed;
    this->mLock->Enter();
    if (this->mSounds[inSampleID].mLoadCount > 1)
        this->mSounds[inSampleID].mLoadCount--;
    else {
        this->mSounds[inSampleID].mLoadCount = 0;
        i = 0;
        while (i < this->mPaused.size()) {
            theNode = this->mPaused[i];
            if (theNode && theNode->mSampleID == inSampleID) {
                std::map<unsigned long, SESMInstanceNode*>::iterator theIterator =
                    this->mPausedInstances.find(theNode->mInstanceID);
                this->mCategories[theNode->mCategory]->mNumberOfInstances--;
                this->mPaused.erase(this->mPaused.begin() + i);
                this->mPausedInstances.erase(theIterator);
                theNode->Dereference();
                theNode->Dereference();
            } else
                i++;
        }
        for (i = 0; i < this->mAvailableChannels; i++) {
            SESMInstanceNode* theChannelNode = this->mChannels[i];
            if (theChannelNode && theChannelNode->mSampleID == inSampleID)
                this->DumpChannel(i);
        }
        std::vector<SESMInstanceNode*>* theInstances = this->mSounds[inSampleID].mInstances;
        unsigned long theMaxInstances = this->GetMaxInstances(inSampleID);
        if (theInstances) {
            for (i = 0; i < theMaxInstances; i++) {
                delete (*theInstances)[i]->mSample;
                (*theInstances)[i]->mSample = NULL;
                (*theInstances)[i]->Dereference();
            }
            delete theInstances;
            this->mSounds[inSampleID].mInstances = NULL;
        }
    }
    this->mLock->Leave();
    return kErrorOK;
    return kErrorFailed;
}

// FUNCTION: LLE 0x100521cc
// Unloads the one-shot sounds whose channel stopped playing
void SESoundManager::UpdateOneShotSounds()
{
    this->mLock->Enter();
    std::vector<SESMOneShotSound>::iterator theSound = this->mOneShotSounds.begin();
    while (theSound != this->mOneShotSounds.end()) {
        if (this->mMixer->IsChannelBusyPlaying(theSound->mSample->GetMixerChannel()) == 5)  // kSPIMChannelPlaying
            theSound++;
        else {
            this->Unload(theSound->mSampleID);
            theSound = this->mOneShotSounds.erase(theSound);
        }
    }
    this->mLock->Leave();
}

// TEMPLATE: LLE 0x10052267
// TEMPLATE: LLE 0x100522f4
// TEMPLATE: LLE 0x1005234c
// TEMPLATE: LLE 0x100523c8
// TEMPLATE: LLE 0x100523fe
// TEMPLATE: LLE 0x100527b8
// TEMPLATE: LLE 0x10052a6e
// TEMPLATE: LLE 0x10052c0e
// TEMPLATE: LLE 0x10052ca8
// TEMPLATE: LLE 0x10052d1b
// TEMPLATE: LLE 0x10052d44
// TEMPLATE: LLE 0x10052e17
// TEMPLATE: LLE 0x10052e48
// TEMPLATE: LLE 0x10052f6b
// TEMPLATE: LLE 0x10052fa5
// TEMPLATE: LLE 0x10052fe6
// TEMPLATE: LLE 0x100530f7
// TEMPLATE: LLE 0x10053127
// TEMPLATE: LLE 0x1005316b
// TEMPLATE: LLE 0x10053194
// TEMPLATE: LLE 0x10053201
// TEMPLATE: LLE 0x10053254
// TEMPLATE: LLE 0x100532a5
// TEMPLATE: LLE 0x1005281c
// TEMPLATE: LLE 0x1005287f
// TEMPLATE: LLE 0x10052961
// TEMPLATE: LLE 0x10052320
// TEMPLATE: LLE 0x10052377
// TEMPLATE: LLE 0x10052413
// TEMPLATE: LLE 0x10052482
// TEMPLATE: LLE 0x10052768

// No marker: exported, empty in this build; ICF-folded into the bare ret at 0x10080c07 (UMath.cpp).
void SESoundManager::DumpMemoryPoolInfo()
{
}
