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
#include "ee/EEDbRecords.h"
#include "ee/EEMediaUse.h"

std::vector<EESoundObject*> gSoundObjects;

// Creates an empty, unloaded sound with no sample
EESoundObject::EESoundObject() : EeDataSoundFields(), EeDataMediaUse()
{
    this->mSampleID = 0;
}

// Destroys the sound, unloading its sample if it is loaded
EESoundObject::~EESoundObject()
{
    if (this->mMediaCount > 0) {
        this->mMediaCount = 1;
        this->UnloadMedia();
    }
}

// Reads the sound table (each record after its sample's name; the record and its copy are ee_data tables.rs':
// instance limit, category, looping, volume percent); returns the last read result
long EESoundObject::LoadTable(FSFile* inFile)
{
    EESoundObject* theSound = NULL;
    return EEDbReadLeadRecords(inFile, ee_data_sound_record_size(),
        [](unsigned long inCount) { gSoundObjects.resize(inCount, NULL); return true; },
        [&](unsigned long) {
            theSound = new EESoundObject;
            theSound->mName.Load(*inFile);
        },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            ee_data_sound_from_record(inRecord, theSound);
            gSoundObjects[inIndex] = theSound;
        });
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

// Loads the sound's sample on first use (later uses wait for the load: ee/EEMediaUse.h); returns 9 on failure
long EESoundObject::LoadMedia()
{
    return EEMediaAcquire(this, sDatabaseLock.mLock, [this]() -> long {
        if (this->mName.GetLength() <= 0)
            return 9;
        this->mSampleID = gSoundManager->Load(this->mName, this->mCategory, this->mMaxInstances);
        return !UIsNone32(this->mSampleID) ? 0 : 9;
    });
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
    EEMediaRelease(this, sDatabaseLock.mLock, [this] {
        gSoundManager->Unload(this->mSampleID);
        this->mSampleID = 0;
    });
    return 0;
}
