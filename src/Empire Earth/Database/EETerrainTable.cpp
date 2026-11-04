// C:\EEX\Empire Earth\Database\EETerrainTable.cpp (name from RTTI EETerrainTable; no __FILE__). 0xdb169..0xdbcc8:
// the two class static tables' init chains, the header lock's chain (Database\EEDatabase.h), the ctor, Load
// (db\dbterrain.dat), Unload, DumpAllLoadedMedia, the mesh functions 0xdb5c8..0xdba99, EECliffTerrainTable (ctor,
// Load, Unload), Get, the ??_Gs and the dtor. EETerrainColorTable.cpp ends at 0xdb169, EETerrainTypeTable.cpp's
// chain starts at 0xdbcc8. Unit flags: the exe's module set.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/TSScheduler.h"
#include "lle/FSFileSpec.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"

#include "ee/EEDatabaseTables.h"
#include "lle/GERenderState.h"

std::vector<EETerrainTable*> EETerrainTable::sTable;
std::vector<EECliffTerrainTable*> EETerrainTable::sCliffTable;

#include <EEDatabase.h>

// 0x4e86d0: the first loaded terrain's +0x50 (-1 until one with a name loads); 0x4e86e8: the first cliff index
// (provisional names)
extern unsigned long gTexturePixelDepth;     // 0x5308b4
// 0x5308b0: the terrain textures' mip levels to drop (LoadTexture; provisional name)
extern unsigned long gTerrainMIPLevelsToDrop;
extern bool gTrilinearFilter;               // 0x5308da (mip filtering on)
extern GETextureManager* gTextureManager;  // 0x531918 (ee/EmpireEarth.h)

long gDefaultTerrain;
long gFirstCliffTerrain;



// One record of db\dbterrain.dat (0xcc bytes after the file name string). Inferred layout.
struct EETerrainRecord {
    char mName[0x68];                       // +0x00 -> mFamilies
    long mID;                               // +0x68 -> +0x50
    long mTextureID;                        // +0x6c -> +0x4c
    long mNameID;                           // +0x70 -> +0x54
    long mTextureTileCoverage;              // +0x74 -> +0x58
    long mTerrainType;                      // +0x78 -> +0x5c
    long mColumns;                          // +0x7c -> +0x60
    long mRows;                             // +0x80 -> +0x64
    bool mIsWater;                          // +0x84 -> +0x84
    bool mHasAlpha;                         // +0x85 -> +0x86
    bool mIsInEditor;                       // +0x86 -> +0x85
    long mColorType;                        // +0x88 -> +0x68
    unsigned long mFirst[3];                // +0x8c: unit class ranges [mFirst[k], mLast[k]] -> mUnitClasses
    unsigned long mLast[3];                 // +0x98
    long mAmbientUnitChance;                // +0xa4 -> +0x7c
    long mWaterAnimationPeriod;             // +0xa8 -> +0x44
    long mWaterDepth;                       // +0xac -> +0x80
    long mRed;                              // +0xb0 (0..255)
    long mGreen;                            // +0xb4
    long mBlue;                             // +0xb8
    char _gapBC[0x0f];
    bool mIsAmbientsOnly;                   // +0xcb -> +0x87
};
static_assert(sizeof(EETerrainRecord) == 204, "on-disk record: 204 bytes on every target");

// One record of db\dbcliffterrain.dat (0x7c bytes). Inferred layout.
struct EECliffTerrainRecord {
    char mName[0x68];                       // +0x00
    long mIndex;                            // +0x68 (negative: no entry)
    long mFirstTerrain;                     // +0x6c -> +0x14
    long mLastTerrain;                      // +0x70 -> +0x18
    long mL74;                              // +0x74 -> +0x1c
    bool mB78;                              // +0x78 -> +0x20
};
static_assert(sizeof(EECliffTerrainRecord) == 124, "on-disk record: 124 bytes on every target");

// FUNCTION: EXE 0x4db245
// Creates an empty terrain: no textures, not water, colour type -1
EETerrainTable::EETerrainTable()
{
    this->mColorType = -1;
    this->mFirstTexture = 0;
    this->mNumTextures = 0;
    this->mTextureID = 0;
    this->mID = 0;
    this->mNameID = 0;
    this->mTextureTileCoverage = 0;
    this->mTerrainType = 0;
    this->mColumns = 0;
    this->mRows = 0;
    this->mAmbientUnitChance = 0;
    this->mWaterAnimationPeriod = 0;
    this->mIsWater = false;
    this->mIsInEditor = false;
    this->mHasAlpha = false;
    this->mTextures = NULL;
    this->mLoadCount = 0;
    this->mIsLoading = false;
    this->mPreviewTexture = NULL;
}

// FUNCTION: EXE 0x4db2ce
// Loads the terrains from db\dbterrain.dat (texture grid, ambient unit classes, colour, water) numbering their
// textures, and notes the first named one as the default terrain; returns the last read status
long EETerrainTable::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theTextureBase = 0;
    EETerrainRecord theRecord;
    inFile->Open(kFileAccessRead);
    gDefaultTerrain = -1;
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EETerrainTable::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EETerrainTable* theTerrain = new EETerrainTable;
            theTerrain->mFirstTexture = theTextureBase;
            theTerrain->mTextureName.Load(*inFile);
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            theTerrain->mName.Copy(theRecord.mName, 0, strlen(theRecord.mName), 0, true);
            theTerrain->mID = theRecord.mID;
            theTerrain->mNameID = theRecord.mNameID;
            theTerrain->mTextureTileCoverage = theRecord.mTextureTileCoverage;
            theTerrain->mTextureID = theRecord.mTextureID;
            for (long k = 0; k < 3; k++) {
                unsigned long theFirst = theRecord.mFirst[k];
                unsigned long theLast = theRecord.mLast[k];
                if (theFirst > 0 && theLast > 0)
                    for (long theClass = theFirst; theClass <= theLast; theClass++)
                        theTerrain->mUnitClasses.push_back(theClass);
            }
            theTerrain->mAmbientUnitChance = theRecord.mAmbientUnitChance;
            theTerrain->mWaterAnimationPeriod = theRecord.mWaterAnimationPeriod;
            theTerrain->mColumns = theRecord.mColumns;
            theTerrain->mRows = theRecord.mRows;
            long theTextureCount = theTerrain->mColumns * theRecord.mRows;
            theTerrain->mTerrainType = theRecord.mTerrainType;
            theTerrain->mIsWater = theRecord.mIsWater;
            theTerrain->mIsInEditor = theRecord.mIsInEditor;
            theTerrain->mHasAlpha = theRecord.mHasAlpha;
            theTerrain->mColorType = theRecord.mColorType;
            theTerrain->mWaterDepth = theRecord.mWaterDepth;
            theTerrain->mColor = GEColor(theRecord.mRed / 255.0f, theRecord.mGreen / 255.0f,
                                         theRecord.mBlue / 255.0f);
            theTerrain->mIsAmbientsOnly = theRecord.mIsAmbientsOnly;
            if (gDefaultTerrain == -1 && theTerrain->mName.GetLength() > 0 && theTerrain->mID != -1)
                gDefaultTerrain = theTerrain->mID;
            EETerrainTable::sTable[i] = theTerrain;
            theTextureBase += theTextureCount;
            theTerrain->mNumTextures = theTextureCount;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4db4c4
// Releases every terrain's textures, destroys the terrains and empties the table (the texture arrays are not freed)
void EETerrainTable::Unload()
{
    unsigned long j = 0;
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        if (EETerrainTable::sTable[i] == NULL)
            continue;
        unsigned long theTextureCount = EETerrainTable::sTable[i]->mRows * EETerrainTable::sTable[i]->mColumns;
        if (EETerrainTable::sTable[i]->mTextures != NULL && theTextureCount > 0)
            for (; j < theTextureCount; j++)
                EETerrainTable::sTable[i]->mTextures[j]->Dereference();
        j = 0;
        delete EETerrainTable::sTable[i];
    }
    EETerrainTable::sTable.resize(0);
}

// FUNCTION: EXE 0x4db551
// Writes each referenced terrain and each terrain with a list-box preview to a media dump file
void EETerrainTable::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Terrain Table*********\n\n");
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTerrain = EETerrainTable::Get(i);
        if (theTerrain == NULL)
            continue;
        if (theTerrain->mLoadCount > 0)
            fprintf(inFile, "\t%s has a media count of %d\n", (const char*)theTerrain->mName,
                    theTerrain->mLoadCount);
        if (theTerrain->mPreviewTexture != NULL)
            fprintf(inFile, "\t%s was loaded for list box display\n", (const char*)theTerrain->mName);
    }
}

#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "ee/EEUnit.h"
#include "ee/EEButtonObject.h"

// FUNCTION: EXE 0x4db5c8
// Adds a reference to the terrain's textures; the first one loads them and attaches them (columns mirrored) to the
// mesh's texture set, later callers wait for that load
void EETerrainTable::Load(GETerrainMesh& ioMesh)
{
    sDatabaseLock.mLock->Enter();
    if (this->mLoadCount == 0) {
        this->mLoadCount = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
        this->LoadTextures();
        sDatabaseLock.mLock->Enter();
        GETextureSet* theSet = ioMesh.GetRegisteredTextureSet(this->mTextureSetID);
        for (long y = 0; y < this->mRows; y++) {
            for (long x = 0; x < this->mColumns; x++)
                theSet->SetTexture(this->mColumns - x - 1, y, this->mTextures[this->mColumns * y + x]);
        }
        this->mIsLoading = false;
    } else {
        this->mLoadCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            Sleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
}

// FUNCTION: EXE 0x4db69d
// Loads one terrain texture and sets its terrain stage modes (clamped; linear mip filter when trilinear is on)
// No NULL test: the original references the result directly
GETexture* EETerrainTable::LoadTexture(const UString& inFile)
{
    GETextureDescription theDescription = { 1, 0, gTexturePixelDepth, gTerrainMIPLevelsToDrop, false, false, this->mHasAlpha, true };
    GETexture* theTexture = gTextureManager->LoadTexture(inFile, theDescription);
    theTexture->Reference();
    theTexture->SetFrame(0);
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipMapBias(-1.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    if (gTrilinearFilter) theTexture->SetStageMipFilterMode(kMipFilterLinear, -1); else theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    return theTexture;
}

// FUNCTION: EXE 0x4db762
// Loads every texture of the terrain's grid (its file name numbered _n, from the last down) into its texture array
void EETerrainTable::LoadTextures()
{
    unsigned long theCount = this->mRows * this->mColumns;
    this->mTextures = new GETexture*[theCount];
    unsigned long theIndex = 0;
    for (unsigned long n = theCount; n > 0; n--) {
        const char* theDot = strrchr(this->mTextureName, '.');
        if (theDot != NULL) {
            char theName[256];
            long theLength = theDot - (const char*)this->mTextureName;
            memcpy(theName, (const char*)this->mTextureName, theLength);
            sprintf(theName + theLength, "_%d%s", n, theDot);
            UString theFile(theName);
            this->mTextures[theIndex++] = this->LoadTexture(theFile);
        } else {
            UString theFile;
            sprintf(theFile, "%s_%d", (const char*)this->mTextureName, n);
            this->mTextures[theIndex++] = this->LoadTexture(theFile);
        }
    }
}

// FUNCTION: EXE 0x4db85a
// Loads each terrain's list-box preview texture (its file name with _1) and references the button of its first unit
// class
// (the terrain list of EEUIFSETerrain)
void EELoadTerrainPreviews()
{
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTable = EETerrainTable::Get(i);
        if (theTable == NULL)
            continue;
        const char* theDot = strrchr(theTable->mTextureName, '.');
        if (theDot != NULL) {
            char theName[256];
            long theLength = theDot - (const char*)theTable->mTextureName;
            memcpy(theName, (const char*)theTable->mTextureName, theLength);
            sprintf(theName + theLength, "_1%s", theDot);
            UString theFile(theName);
            theTable->mPreviewTexture = theTable->LoadTexture(theFile);
        } else {
            UString theFile;
            sprintf(theFile, "%s_1", (const char*)theTable->mTextureName);
            theTable->mPreviewTexture = theTable->LoadTexture(theFile);
        }
        if (theTable->mUnitClasses.size() <= 0)
            continue;
        EEUnitClass* theClass = gUnitClassTable[theTable->mUnitClasses[0]];
        if (theClass != NULL)
            theClass->mButton->Acquire();
    }
}

// FUNCTION: EXE 0x4db96c
// Attaches the terrain's loaded textures (columns mirrored) to a texture set, once any load has finished
void EETerrainTable::AttachTextures(GETextureSet* inTextureSet)
{
    while (this->mIsLoading == true)
        Sleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mLoadCount > 0) {
        for (long y = 0; y < this->mRows; y++) {
            for (long x = 0; x < this->mColumns; x++)
                inTextureSet->SetTexture(this->mColumns - x - 1, y, this->mTextures[this->mColumns * y + x]);
        }
    }
    sDatabaseLock.mLock->Leave();
}

// FUNCTION: EXE 0x4db9e3
// Drops a reference to the terrain's textures; the last one detaches them from the mesh and releases them
void EETerrainTable::Unload(GETerrainMesh& ioMesh)
{
    while (this->mIsLoading == true)
        Sleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mLoadCount == 1) {
        this->mLoadCount = 0;
        GETextureSet* theSet = ioMesh.GetRegisteredTextureSet(this->mTextureSetID);
        for (long y = 0; y < this->mRows; y++) {
            for (long x = 0; x < this->mColumns; x++)
                theSet->SetTexture(x, y, NULL);
        }
        unsigned long theCount = this->mColumns * this->mRows;
        for (unsigned long i = 0; i < theCount; i++) {
            this->mTextures[i]->Dereference();
            this->mTextures[i] = NULL;
        }
        delete[] this->mTextures;
        this->mTextures = NULL;
    } else if (this->mLoadCount > 0) {
        this->mLoadCount--;
    }
    sDatabaseLock.mLock->Leave();
}

// FUNCTION: EXE 0x4dba99
// Releases each terrain's preview texture and its first unit class's button reference
void EEReleaseTerrainPreviews()
{
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTerrain = EETerrainTable::Get(i);
        if (theTerrain == NULL)
            continue;
        theTerrain->mPreviewTexture->Dereference();
        theTerrain->mPreviewTexture = NULL;
        if (theTerrain->mUnitClasses.size() <= 0)
            continue;
        EEUnitClass* theClass = gUnitClassTable[theTerrain->mUnitClasses[0]];
        if (theClass != NULL)
            theClass->mButton->Release();
    }
}

// FUNCTION: EXE 0x4dbafe
// Creates an empty cliff terrain entry
EECliffTerrainTable::EECliffTerrainTable()
{
    this->mFirstTerrain = 0;
    this->mLastTerrain = 0;
    this->mL1C = 0;
    this->mB20 = false;
}

// FUNCTION: EXE 0x4dbb22
// Loads the cliff terrains from db\dbcliffterrain.dat (empty slots for negative indices) and notes the first one;
// returns the last read status
long EECliffTerrainTable::Load(FSFileSpec* inFile)
{
    EECliffTerrainTable* theCliff = NULL;
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EECliffTerrainRecord theRecord;
    inFile->Open(kFileAccessRead);
    bool theFound = false;
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EETerrainTable::sCliffTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (theRecord.mIndex >= 0) {
                theCliff = new EECliffTerrainTable;
                theCliff->mName = theRecord.mName;
                theCliff->mFirstTerrain = theRecord.mFirstTerrain;
                theCliff->mLastTerrain = theRecord.mLastTerrain;
                theCliff->mB20 = theRecord.mB78;
                theCliff->mL1C = theRecord.mL74;
                if (!theFound) {
                    gFirstCliffTerrain = i;
                    theFound = true;
                }
            }
            EETerrainTable::sCliffTable[i] = theCliff;
            theCliff = NULL;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4dbbfb
// Destroys every cliff terrain and empties the cliff table; returns 0
long EECliffTerrainTable::Unload()
{
    for (unsigned long i = 0; i < EETerrainTable::sCliffTable.size(); i++)
        delete EETerrainTable::sCliffTable[i];
    EETerrainTable::sCliffTable.clear();
    return 0;
}

// FUNCTION: EXE 0x4dbc39
// Returns the terrain at an index, or NULL when out of range or unused (negative id)
EETerrainTable* EETerrainTable::Get(long inIndex)
{
    if ((unsigned long)inIndex < EETerrainTable::sTable.size() && EETerrainTable::sTable[inIndex]->mID >= 0)
        return EETerrainTable::sTable[inIndex];
    return NULL;
}

// SYNTHETIC: EXE 0x4dbc61
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4dbc7d
// Destroys the terrain (members only)
EETerrainTable::~EETerrainTable()
{
}

// SYNTHETIC: EXE 0x4dbca2
// (EECliffTerrainTable's scalar deleting destructor)

// SYNTHETIC: EXE 0x4db169
// SYNTHETIC: EXE 0x4db173
// SYNTHETIC: EXE 0x4db18b
// SYNTHETIC: EXE 0x4db197
// SYNTHETIC: EXE 0x4db1b2
// SYNTHETIC: EXE 0x4db1bc
// SYNTHETIC: EXE 0x4db1d4
// SYNTHETIC: EXE 0x4db1e0
// SYNTHETIC: EXE 0x4db1fb
// SYNTHETIC: EXE 0x4db205
// SYNTHETIC: EXE 0x4db22b
// SYNTHETIC: EXE 0x4db237
