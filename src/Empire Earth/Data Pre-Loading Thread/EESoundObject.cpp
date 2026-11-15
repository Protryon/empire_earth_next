// C:\EEX\Empire Earth\Data Pre-Loading Thread\EESoundObject.cpp (name from RTTI EESoundObject; no __FILE__).
// 0xda56d..0xda984: the sounds of the database. The sound table's init chain (gSoundObjects, 0x4e8678) and
// EEDatabase.h's lock chain (0x4e8670) first, then the class (ctor, ??_G, dtor, LoadTable, UnloadTable, Dump,
// LoadMedia, the two Play overloads, UnloadMedia). EERandomMapTable.cpp ends at 0xda56d, EEStartingResources.cpp
// starts at 0xda984. Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/RTSGraphics.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "ee/EESoundObject.h"

std::vector<EESoundObject*> gSoundObjects;

// One record of db\dbsounds.dat (0x84 bytes, read whole). Layout inferred (only the fields LoadTable copies).
struct EESoundRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mUnusedIndex;                   // +0x068 the 0-based row index (not read)
    int32_t mMaxInstances;                  // +0x6c -> +0x1c
    int32_t mCategory;                      // +0x70 -> +0x20
    int32_t mUnusedPriority;                // +0x74 -> +0x24
    bool mLooping;                          // +0x78 -> +0x2c (looping)
    int32_t mVolume;                        // +0x7c: percent -> +0x14 (float, * 0.01)
    char _gap80[4];
};
static_assert(sizeof(EESoundRecord) == 132, "on-disk record: 132 bytes on every target");

// Creates an empty, unloaded sound with no sample
EESoundObject::EESoundObject()
{
    this->mVolume = 0.0f;
    this->mSampleID = 0;
    this->mMaxInstances = 0;
    this->mCategory = 0;
    this->mL24 = 0;
    this->mMediaCount = 0;
    this->mLooping = false;
    this->mIsLoading = false;
}

// Destroys the sound, unloading its sample if it is loaded
EESoundObject::~EESoundObject()
{
    if (this->mMediaCount > 0) {
        this->mMediaCount = 1;
        this->UnloadMedia();
    }
}

// Reads the sound table (name, instance limit, category, looping, volume percent); returns the last read result
long EESoundObject::LoadTable(FSFile* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    unsigned long i;
    EESoundRecord theRecord;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        gSoundObjects.resize(theCount, NULL);
        for (i = 0; i < theCount; i++) {
            EESoundObject* theSound = new EESoundObject;
            theSound->mName.Load(*inFile);
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            theSound->mMaxInstances = theRecord.mMaxInstances;
            theSound->mCategory = theRecord.mCategory;
            theSound->mL24 = theRecord.mUnusedPriority;
            theSound->mLooping = theRecord.mLooping;
            theSound->mVolume = theRecord.mVolume * 0.01f;
            gSoundObjects[i] = theSound;
        }
    }
    inFile->Close();
    return theResult;
}

// Deletes every sound of the table and empties it
void EESoundObject::UnloadTable()
{
    for (unsigned long i = 0; i < gSoundObjects.size(); i++)
        delete gSoundObjects[i];
    gSoundObjects.resize(0, NULL);
}

// Writes to the file each sound that is still loaded, with its name and use count
void EESoundObject::Dump(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Sound Object Table*********\n\n");
    for (unsigned long i = 0; i < gSoundObjects.size(); i++) {
        EESoundObject* theSound = gSoundObjects[i];
        if (theSound != NULL && theSound->mMediaCount > 0)
            fprintf(inFile, "\t%s has a media count of %ld\n", (const char*)theSound->mName, theSound->mMediaCount);
    }
}

// Loads the sound's sample on first use (counting later uses, which wait for the load); returns 9 on failure
long EESoundObject::LoadMedia()
{
    sDatabaseLock.mLock->Enter();
    long theResult = 0;
    if (this->mMediaCount == 0) {
        this->mMediaCount = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
        if (this->mName.GetLength() > 0) {
            this->mSampleID = gSoundManager->Load(this->mName, this->mCategory, this->mMaxInstances);
            theResult = !UIsNone32(this->mSampleID) ? 0 : 9;
        } else {
            theResult = 9;
        }
        sDatabaseLock.mLock->Enter();
        this->mIsLoading = false;
    } else {
        this->mMediaCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// Sets the sound's volume (folded with EEContagionHelper::SetFunctor in the original: the store to +0x14; a float
// argument and a pointer argument are passed in different registers on x64)
void EESoundObject::SetVolume(float inVolume)
{
    this->mVolume = inVolume;
}

// Plays the sound once at full volume, centred, normal pitch
void EESoundObject::Play()
{
    this->Play(1.0f, 0.0f, 1.0f);
}

// Loads the sample and plays it once as a one-shot at the given volume, pan and pitch; returns 9 on failure
long EESoundObject::Play(float inVolume, float inPan, float inPitch)
{
    long theResult = 0;
    if (this->mName.GetLength() > 0) {
        this->mSampleID = gSoundManager->Load(this->mName, this->mCategory, this->mMaxInstances);
        if (UIsNone32(this->mSampleID))
            theResult = 9;
    } else
        theResult = 9;
    if (theResult == 0) {
        unsigned long theHandle = gSoundManager->Play(this->mSampleID, inVolume, inPan, inPitch, false, NULL);
        if (!UIsNone32(theHandle)) {
            gSoundManager->RegisterOneShotSound(this->mSampleID, theHandle);
        } else {
            theResult = 9;
            gSoundManager->Unload(this->mSampleID);
        }
    }
    return theResult;
}

// Counts one use down; on the last use unloads the sample. Returns 0
long EESoundObject::UnloadMedia()
{
    while (this->mIsLoading == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 1) {
        this->mMediaCount = 0;
        gSoundManager->Unload(this->mSampleID);
        this->mSampleID = 0;
    } else if (this->mMediaCount > 1) {
        this->mMediaCount--;
    }
    sDatabaseLock.mLock->Leave();
    return 0;
}
