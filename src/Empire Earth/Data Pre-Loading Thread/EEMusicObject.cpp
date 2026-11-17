// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEMusicObject.cpp (name from RTTI EEMusicObject; no __FILE__).
// 0xd9b2b..0xda3be: the songs of the database. The statics' init chains first (the per-epoch song lists 0x4e8568,
// the per-epoch shuffled lists 0x4e8470, the random generator 0x4e8460, EEDatabase.h's lock 0x4e8458), then the
// "New Music.txt" reader, the class (ctor, ??_G, dtor, LoadTable, ClearAll, GetRandom, LoadScore, UnloadScore) and the
// EEMedia thunks. The statics are plain globals here (the original's are class statics with guarded destructors).
// Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/URandom.h"
#include "lle/SEMixer.h"

#include "ee/RTSGraphics.h"
#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "ee/EEMusicObject.h"
#include "ee/EEDbRecords.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EERules.h"

std::vector<std::vector<EEMusicObject*>> EEMusicObject::sSongs;      // 0x4e8568 (per epoch)
std::vector<std::vector<EEMusicObject*>> EEMusicObject::sShuffled;
URandomIntegers gMusicRandom;

// Reads a song override from Data\Music\New Music.txt (file name, four reverb values; every epoch on) into a dbmusic
// record (ee_data tables.rs); returns 0 when the file was read
long EEMusicObject::ReadNewMusic(unsigned char* outRecord)
{
    FSFileSpec theDirectory;
    FSFileSpec theFile;
    char theName[0x100];
    float theReverb[4];
    FILE* theHandle;
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\Music\\");
    long theResult = theDirectory.GetFirstFile("New Music.txt", theFile);
    if (theResult != 0)
        return theResult;
    theResult = theFile.FOpen("r");
    if (theResult == 0) {
        theHandle = theFile;
        fscanf(theHandle, "%255s\n", theName);
        for (int k = 0; k < 4; k++)
            fscanf(theHandle, "%f\n", &theReverb[k]);
        ee_data_new_music_record(theName, theReverb, outRecord);
        theResult = theFile.Close();
    }
    return theResult;
}

// Creates an unloaded song from its record (name + ".sgt", reverb, epochs; the space epoch copies nano's flag: the
// record and its copy are ee_data tables.rs')
EEMusicObject::EEMusicObject(const unsigned char* inRecord)
{
    UString theName;
    EEDbCopyName(theName, ee_data_music_from_record(inRecord, this));
    this->mFileName = (const char*)theName;
    this->mFileName += ".sgt";
    this->mScoreID = -1;
    this->mState = 0;
    this->mIsLoading = false;
}

// Destroys the song (nothing of its own to free)
EEMusicObject::~EEMusicObject()
{
}

// Files every song of the music table under the epochs it plays in; a New Music.txt override replaces
// the table with that one song. Returns the last read result
// The epochs and their songs are the rules' (epochs.yaml music, EERules.cpp); without epochs.yaml they are the
// table's per-epoch flags, as in the original (the space age plays the nano age's songs).
long EEMusicObject::LoadTable(FSFile* inFile)
{
    std::vector<unsigned char> theRecord(ee_data_music_record_size(), 0);
    EERules& theRules = EEGetRules();
    EEMusicObject::sSongs.assign(theRules.mEpochs.size(), std::vector<EEMusicObject*>());
    EEMusicObject::sShuffled.assign(theRules.mEpochs.size(), std::vector<EEMusicObject*>());
    gMusicRandom.SetSeed(TMGetTime());
    long theResult = EEMusicObject::ReadNewMusic(theRecord.data());
    if (theResult != 0) {
        std::vector<EEMusicObject*> theSongs;   // by row
        theResult = EEDbReadRecords(inFile, ee_data_music_record_size(),
            [](unsigned long) { return true; },
            [&](unsigned long inIndex, const unsigned char* inRow) {
                bool theFlags[14];
                theSongs.push_back(new EEMusicObject(inRow));
                ee_data_music_epoch_flags(inRow, theFlags);
                theRules.SetMusicFromFlags((long)inIndex, theFlags, 14);   // (no-op when epochs.yaml has them)
            });
        for (unsigned long j = 0; j < theRules.mEpochs.size(); j++) {
            for (long theRow : theRules.mEpochs[j].mMusic) {
                if (theRow < 0 || (unsigned long)theRow >= theSongs.size())
                    continue;
                EEMusicObject::sSongs[j].push_back(theSongs[theRow]);
                theSongs[theRow]->Reference();
            }
        }
        for (EEMusicObject* s : theSongs)
            s->Dereference();
    } else {
        EEMusicObject* theSong = new EEMusicObject(theRecord.data());
        for (unsigned long j = 0; j < EEMusicObject::sSongs.size(); j++) {   // the override plays in every epoch
            EEMusicObject::sSongs[j].push_back(theSong);
            theSong->Reference();
        }
        theSong->Dereference();
    }
    return theResult;
}

// Releases every song and empties all epoch song lists and shuffled lists
void EEMusicObject::ClearAll()
{
    for (unsigned long i = 0; i < EEMusicObject::sSongs.size(); i++) {
        unsigned long theCount = EEMusicObject::sSongs[i].size();
        for (unsigned long j = 0; j < theCount; j++)
            EEMusicObject::sSongs[i][j]->Dereference();
        EEMusicObject::sSongs[i].clear();
        EEMusicObject::sShuffled[i].clear();
    }
}

// Returns the next song of the epoch's shuffled order, reshuffling when it runs out; NULL if the epoch has none
EEMusicObject* EEMusicObject::GetRandom(long inEpoch)
{
    std::vector<EEMusicObject*> theSongs;
    EEMusicObject* theResult;
    if (inEpoch < 0 || (unsigned long)inEpoch >= EEMusicObject::sSongs.size())   // the rules' epochs
        return NULL;
    if (EEMusicObject::sShuffled[inEpoch].size() == 0 && EEMusicObject::sSongs[inEpoch].size() > 0) {
        for (unsigned long i = 0; i < EEMusicObject::sSongs[inEpoch].size(); i++)
            theSongs.push_back(EEMusicObject::sSongs[inEpoch][i]);
        while (theSongs.size() != 0) {
            long theIndex = gMusicRandom.Generate(0, theSongs.size() - 1);
            EEMusicObject::sShuffled[inEpoch].push_back(theSongs[theIndex]);
            theSongs.erase(theSongs.begin() + theIndex);
        }
    }
    if (EEMusicObject::sShuffled[inEpoch].size() > 0) {
        theResult = EEMusicObject::sShuffled[inEpoch].back();
        EEMusicObject::sShuffled[inEpoch].pop_back();
    } else {
        theResult = NULL;
    }
    return theResult;
}

// Loads the song's score on first use (counting later uses, which wait for the load); returns the load result
long EEMusicObject::LoadScore()
{
    SEConductor& theConductor = gConductor->GetConductor();
    long theResult = 0;
    long theID;
    sDatabaseLock.mLock->Enter();
    if (this->mState == 0) {
        this->mState = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
        theResult = theConductor.LoadScore(this->mFileName, theID);
        if (theResult == 0) {
            this->mScoreID = theID;
        } else {
            sDatabaseLock.mLock->Enter();
            this->mState = 0;
            sDatabaseLock.mLock->Leave();
        }
        sDatabaseLock.mLock->Enter();
        this->mIsLoading = false;
    } else {
        this->mState++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// Counts one use down; on the last use unloads the score unless the conductor reports it busy (5)
long EEMusicObject::UnloadScore()
{
    SEConductor& theConductor = gConductor->GetConductor();
    long theResult = 0;
    while (this->mIsLoading == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mState == 1) {
        theResult = theConductor.UnloadScore(this->mScoreID);
        if (theResult != 5) {
            this->mState = 0;
            this->mScoreID = -1;
        }
    } else if (this->mState > 1) {
        this->mState--;
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// (0x4da37a: vector<EEMusicObject*>::push_back, ICF-shared)

// Loads the song's score (media interface)
// EEMedia's slots: `this` arrives as the EEMedia part (add ecx, -8), then a tail jump
long EEMusicObject::LoadMedia()
{
    return this->LoadScore();
}

// Unloads the song's score (media interface)
long EEMusicObject::UnloadMedia()
{
    return this->UnloadScore();
}
