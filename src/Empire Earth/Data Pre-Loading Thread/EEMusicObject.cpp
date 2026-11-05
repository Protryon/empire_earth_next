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
#include "ee/EETechTreeTechID.h"

std::vector<EEMusicObject*> EEMusicObject::sSongs[15];      // 0x4e8568
std::vector<EEMusicObject*> EEMusicObject::sShuffled[15];   // 0x4e8470
URandomIntegers gMusicRandom;                     // 0x4e8460

// One record of db\dbmusic.dat (0xc4 bytes, read whole; also what ReadNewMusic fills). Layout inferred.
struct EEMusicRecord {
    int32_t mUnusedDatabaseID;              // +0x000 the database row id (not read)
    int32_t mUnusedIndex;                   // +0x004 the 0-based row index (not read)
    char mLabel[100];                       // +0x008 the row name (the game never reads it; the text's label)
    char mFileName[0x34];                   // +0x6c (a plain name; the song object appends ".sgt")
    float mReverbGain;                      // +0xa0
    float mReverbHighFreqRTRatio;           // +0xa4
    float mReverbMix;                       // +0xa8
    float mReverbTime;                      // +0xac
    char _gapB0[5];
    bool mInEpoch[14];                      // +0xb5..+0xc2
    char _gapC3[1];
};
static_assert(sizeof(EEMusicRecord) == 196, "on-disk record: 196 bytes on every target");

// FUNCTION: EXE 0x4d9c6b
// Reads a song override from Data\Music\New Music.txt (file name, four reverb values; every epoch on);
// returns 0 when the file was read
long EEMusicObject::ReadNewMusic(EEMusicRecord* outRecord)
{
    FSFileSpec theDirectory;
    FSFileSpec theFile;
    char theName[0x100];
    FILE* theHandle;
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Data\\Music\\");
    long theResult = theDirectory.GetFirstFile("New Music.txt", theFile);
    if (theResult != 0)
        return theResult;
    theResult = theFile.FOpen("r");
    if (theResult == 0) {
        theHandle = theFile;
        fscanf(theHandle, "%s\n", theName);
        sprintf(outRecord->mFileName, "%s", theName);
        fscanf(theHandle, "%f\n", &outRecord->mReverbGain);
        fscanf(theHandle, "%f\n", &outRecord->mReverbHighFreqRTRatio);
        fscanf(theHandle, "%f\n", &outRecord->mReverbMix);
        fscanf(theHandle, "%f\n", &outRecord->mReverbTime);
        outRecord->mInEpoch[kTechEpochPrehistoric] = true;
        outRecord->mInEpoch[kTechEpochStone] = true;
        outRecord->mInEpoch[kTechEpochCopper] = true;
        outRecord->mInEpoch[kTechEpochBronze] = true;
        outRecord->mInEpoch[kTechEpochDark] = true;
        outRecord->mInEpoch[kTechEpochMiddle] = true;
        outRecord->mInEpoch[kTechEpochRenaissance] = true;
        outRecord->mInEpoch[kTechEpochImperial] = true;
        outRecord->mInEpoch[kTechEpochIndustrial] = true;
        outRecord->mInEpoch[kTechEpochAtomicWW1] = true;
        outRecord->mInEpoch[kTechEpochAtomicWW2] = true;
        outRecord->mInEpoch[kTechEpochAtomicModern] = true;
        outRecord->mInEpoch[kTechEpochDigital] = true;
        outRecord->mInEpoch[kTechEpochNano] = true;
        theResult = theFile.Close();
    }
    return theResult;
}

// FUNCTION: EXE 0x4d9e0c
// Creates an unloaded song from its record (name + ".sgt", reverb, epochs; the space epoch copies nano's flag)
EEMusicObject::EEMusicObject(EEMusicRecord* inRecord)
{
    this->mFileName = inRecord->mFileName;
    this->mFileName += ".sgt";
    this->mReverbHighFreqRTRatio = inRecord->mReverbHighFreqRTRatio;
    this->mReverbGain = inRecord->mReverbGain;
    this->mReverbMix = inRecord->mReverbMix;
    this->mReverbTime = inRecord->mReverbTime;
    this->mInEpoch[kTechEpochPrehistoric] = inRecord->mInEpoch[kTechEpochPrehistoric];
    this->mInEpoch[kTechEpochStone] = inRecord->mInEpoch[kTechEpochStone];
    this->mInEpoch[kTechEpochCopper] = inRecord->mInEpoch[kTechEpochCopper];
    this->mInEpoch[kTechEpochBronze] = inRecord->mInEpoch[kTechEpochBronze];
    this->mInEpoch[kTechEpochDark] = inRecord->mInEpoch[kTechEpochDark];
    this->mInEpoch[kTechEpochMiddle] = inRecord->mInEpoch[kTechEpochMiddle];
    this->mInEpoch[kTechEpochRenaissance] = inRecord->mInEpoch[kTechEpochRenaissance];
    this->mInEpoch[kTechEpochImperial] = inRecord->mInEpoch[kTechEpochImperial];
    this->mInEpoch[kTechEpochIndustrial] = inRecord->mInEpoch[kTechEpochIndustrial];
    this->mInEpoch[kTechEpochAtomicWW1] = inRecord->mInEpoch[kTechEpochAtomicWW1];
    this->mInEpoch[kTechEpochAtomicWW2] = inRecord->mInEpoch[kTechEpochAtomicWW2];
    this->mInEpoch[kTechEpochAtomicModern] = inRecord->mInEpoch[kTechEpochAtomicModern];
    this->mInEpoch[kTechEpochDigital] = inRecord->mInEpoch[kTechEpochDigital];
    this->mInEpoch[kTechEpochNano] = inRecord->mInEpoch[kTechEpochNano];
    this->mInEpoch[kTechEpochSpace] = inRecord->mInEpoch[kTechEpochNano];
    this->mScoreID = -1;
    this->mState = 0;
    this->mIsLoading = false;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x4d9f1c

// FUNCTION: EXE 0x4d9f38
// Destroys the song (nothing of its own to free)
EEMusicObject::~EEMusicObject()
{
}

// FUNCTION: EXE 0x4d9f6c
// Files every song of the music table under the epochs it plays in; a New Music.txt override replaces
// the table with that one song. Returns the last read result
long EEMusicObject::LoadTable(FSFile* inFile)
{
    EEMusicRecord theRecord;
    inFile->Open(kFileAccessRead);
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    unsigned long i;
    EEMusicObject* theSong;
    gMusicRandom.SetSeed(TMGetTime());
    long theResult = EEMusicObject::ReadNewMusic(&theRecord);
    if (theResult != 0) {
        theResult = inFile->Read(theCount);
        if (theResult == 0) {
            for (i = 0; i < theCount; i++) {
                theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
                theSong = new EEMusicObject(&theRecord);
                for (long j = 0; j < 15; j++) {
                    if (theSong->mInEpoch[j]) {
                        EEMusicObject::sSongs[j].push_back(theSong);
                        theSong->Reference();
                    }
                }
                theSong->Dereference();
            }
        }
    } else {
        theSong = new EEMusicObject(&theRecord);
        for (long j = 0; j < 15; j++) {
            if (theSong->mInEpoch[j]) {
                EEMusicObject::sSongs[j].push_back(theSong);
                theSong->Reference();
            }
        }
        theSong->Dereference();
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4da0b9
// Releases every song and empties all epoch song lists and shuffled lists
void EEMusicObject::ClearAll()
{
    for (long i = 0; i < 15; i++) {
        unsigned long theCount = EEMusicObject::sSongs[i].size();
        for (unsigned long j = 0; j < theCount; j++)
            EEMusicObject::sSongs[i][j]->Dereference();
        EEMusicObject::sSongs[i].clear();
        EEMusicObject::sShuffled[i].clear();
    }
}

// FUNCTION: EXE 0x4da116
// Returns the next song of the epoch's shuffled order, reshuffling when it runs out; NULL if the epoch has none
EEMusicObject* EEMusicObject::GetRandom(long inEpoch)
{
    std::vector<EEMusicObject*> theSongs;
    EEMusicObject* theResult;
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

// FUNCTION: EXE 0x4da251
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

// FUNCTION: EXE 0x4da315
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

// SYNTHETIC: EXE 0x4da3a6
// (EEMedia's vector deleting dtor thunk)

// FUNCTION: EXE 0x4da3ae
// Loads the song's score (media interface)
// EEMedia's slots: `this` arrives as the EEMedia part (add ecx, -8), then a tail jump
long EEMusicObject::LoadMedia()
{
    return this->LoadScore();
}

// FUNCTION: EXE 0x4da3b6
// Unloads the song's score (media interface)
long EEMusicObject::UnloadMedia()
{
    return this->UnloadScore();
}

// TEMPLATE: EXE 0x4da37a
// (std::vector<EEMusicObject*>::push_back, a template COMDAT emitted by this unit)

// The init chains (gMusicSongs, gMusicShuffled: guarded dtors in the original; gMusicRandom; EEDatabase.h's lock):
// SYNTHETIC: EXE 0x4d9b2b
// SYNTHETIC: EXE 0x4d9b35
// SYNTHETIC: EXE 0x4d9b59
// SYNTHETIC: EXE 0x4d9b65
// SYNTHETIC: EXE 0x4d9b8f
// SYNTHETIC: EXE 0x4d9b99
// SYNTHETIC: EXE 0x4d9bbd
// SYNTHETIC: EXE 0x4d9bc9
// SYNTHETIC: EXE 0x4d9bf3
// SYNTHETIC: EXE 0x4d9bf8
// SYNTHETIC: EXE 0x4d9c21
// SYNTHETIC: EXE 0x4d9c2b
// SYNTHETIC: EXE 0x4d9c51
// SYNTHETIC: EXE 0x4d9c5d
