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
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"

#include "ee/EEDatabaseTables.h"
#include "ee/EEDbRecords.h"
#include "ee/EEMediaUse.h"
#include "lle/GERenderState.h"

std::vector<EETerrainTable*> EETerrainTable::sTable;
std::vector<EECliffTerrainTable*> EETerrainTable::sCliffTable;

#include <EEDatabase.h>

// 0x4e86d0: the first loaded terrain's +0x50 (-1 until one with a name loads); 0x4e86e8: the first cliff index
// (provisional names)
extern unsigned long& gTexturePixelDepth;
// 0x5308b0: the terrain textures' mip levels to drop (LoadTexture; provisional name)
extern unsigned long& gTerrainMIPLevelsToDrop;
extern bool& gTrilinearFilter;               // 0x5308da (mip filtering on)
extern GETextureManager* gTextureManager;  // 0x531918 (ee/EmpireEarth.h)

long gDefaultTerrain;
long gFirstCliffTerrain;



// Creates an empty terrain: no textures, not water, colour type -1 (ee_data tables.rs)
EETerrainTable::EETerrainTable() : EeDataMediaUse()
{
    ee_data_terrain_default(this);
    this->mFirstTexture = 0;
    this->mTextures = NULL;
    this->mPreviewTexture = NULL;
}

// Loads the terrains from db\dbterrain.dat (each record after its texture file name; the record and its copy are
// ee_data tables.rs': texture grid, ambient unit classes, colour, water) numbering their textures, and notes the first
// named one as the default terrain; returns the last read status
long EETerrainTable::Load(FSFileSpec* inFile)
{
    long theTextureBase = 0;
    EETerrainTable* theTerrain = NULL;
    gDefaultTerrain = -1;
    return EEDbReadLeadRecords(inFile, ee_data_terrain_record_size(),
        [](unsigned long inCount) { EETerrainTable::sTable.resize(inCount, NULL); return true; },
        [&](unsigned long) {
            theTerrain = new EETerrainTable;
            theTerrain->mFirstTexture = theTextureBase;
            theTerrain->mTextureName.Load(*inFile);
        },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            EeDataTerrainExtra theExtra;
            ee_data_terrain_from_record(inRecord, theTerrain, &theExtra);
            EEDbCopyName(theTerrain->mName, theExtra.name);
            for (long k = 0; k < 3; k++)
                if (theExtra.first[k] > 0 && theExtra.last[k] > 0)
                    for (long theClass = theExtra.first[k]; (unsigned long)theClass <= theExtra.last[k]; theClass++)
                        theTerrain->mUnitClasses.push_back(theClass);
            theTerrain->mColor = GEColor(theExtra.color[0], theExtra.color[1], theExtra.color[2]);
            if (gDefaultTerrain == -1 && theTerrain->mName.GetLength() > 0 && theTerrain->mID != -1)
                gDefaultTerrain = theTerrain->mID;
            EETerrainTable::sTable[inIndex] = theTerrain;
            theTextureBase += theTerrain->mNumTextures;
        });
}

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

// Writes each referenced terrain and each terrain with a list-box preview to a media dump file
void EETerrainTable::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Terrain Table*********\n\n");
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTerrain = EETerrainTable::Get(i);
        if (theTerrain == NULL)
            continue;
        if (theTerrain->mMediaCount > 0)
            fprintf(inFile, "\t%s has a media count of %ld\n", (const char*)theTerrain->mName,
                    theTerrain->mMediaCount);
        if (theTerrain->mPreviewTexture != NULL)
            fprintf(inFile, "\t%s was loaded for list box display\n", (const char*)theTerrain->mName);
    }
}

#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "ee/EEUnit.h"
#include "ee/EEButtonObject.h"

// Adds a reference to the terrain's textures; the first one loads them and attaches them (columns mirrored) to the
// mesh's texture set, later callers wait for that load
void EETerrainTable::Load(GETerrainMesh& ioMesh)
{
    EEMediaAcquire(this, sDatabaseLock.mLock, [this] { this->LoadTextures(); }, [&] {
        GETextureSet* theSet = ioMesh.GetRegisteredTextureSet(this->mTextureSetID);
        for (long y = 0; y < this->mRows; y++) {
            for (long x = 0; x < this->mColumns; x++)
                theSet->SetTexture(this->mColumns - x - 1, y, this->mTextures[this->mColumns * y + x]);
        }
    });
}

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

// A terrain texture's file: the texture name numbered _n before its extension (ee_data tables.rs)
static UString EETerrainTextureFile(const UString& inName, long inNumber)
{
    char theName[256];
    ee_data_terrain_texture_file(inName, inNumber, theName, sizeof(theName));
    return UString(theName);
}

// Loads every texture of the terrain's grid (its file name numbered _n, from the last down) into its texture array
void EETerrainTable::LoadTextures()
{
    unsigned long theCount = this->mRows * this->mColumns;
    this->mTextures = new GETexture*[theCount];
    unsigned long theIndex = 0;
    for (unsigned long n = theCount; n > 0; n--)
        this->mTextures[theIndex++] = this->LoadTexture(EETerrainTextureFile(this->mTextureName, n));
}

// Loads each terrain's list-box preview texture (its file name with _1) and references the button of its first unit
// class (the terrain list of EEUIFSETerrain)
void EELoadTerrainPreviews()
{
    for (unsigned long i = 0; i < EETerrainTable::sTable.size(); i++) {
        EETerrainTable* theTable = EETerrainTable::Get(i);
        if (theTable == NULL)
            continue;
        theTable->mPreviewTexture = theTable->LoadTexture(EETerrainTextureFile(theTable->mTextureName, 1));
        if (theTable->mUnitClasses.size() <= 0)
            continue;
        EEUnitClass* theClass = gUnitClassTable[theTable->mUnitClasses[0]];
        if (theClass != NULL)
            theClass->mButton->Acquire();
    }
}

// Attaches the terrain's loaded textures (columns mirrored) to a texture set, once any load has finished
void EETerrainTable::AttachTextures(GETextureSet* inTextureSet)
{
    EEMediaLocked(this, sDatabaseLock.mLock, [&] {
        if (this->mMediaCount > 0) {
            for (long y = 0; y < this->mRows; y++) {
                for (long x = 0; x < this->mColumns; x++)
                    inTextureSet->SetTexture(this->mColumns - x - 1, y, this->mTextures[this->mColumns * y + x]);
            }
        }
    });
}

// Drops a reference to the terrain's textures; the last one detaches them from the mesh and releases them
void EETerrainTable::Unload(GETerrainMesh& ioMesh)
{
    EEMediaRelease(this, sDatabaseLock.mLock, [&] {
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
    });
}

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

// Loads the cliff terrains from db\dbcliffterrain.dat (empty slots for negative indices; the record and its copy are
// ee_data tables.rs') and notes the first one; returns the last read status
long EECliffTerrainTable::Load(FSFileSpec* inFile)
{
    bool theFound = false;
    return EEDbReadRecords(inFile, ee_data_cliff_terrain_record_size(),
        [](unsigned long inCount) { EETerrainTable::sCliffTable.resize(inCount, NULL); return true; },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            EECliffTerrainTable* theCliff = new EECliffTerrainTable;
            EeDataName theName;
            if (ee_data_cliff_terrain_from_record(inRecord, theCliff, &theName)) {
                EEDbCopyName(theCliff->mName, theName);
                if (!theFound) {
                    gFirstCliffTerrain = inIndex;
                    theFound = true;
                }
            } else {
                delete theCliff;
                theCliff = NULL;
            }
            EETerrainTable::sCliffTable[inIndex] = theCliff;
        });
}

// Destroys every cliff terrain and empties the cliff table; returns 0
long EECliffTerrainTable::Unload()
{
    for (unsigned long i = 0; i < EETerrainTable::sCliffTable.size(); i++)
        delete EETerrainTable::sCliffTable[i];
    EETerrainTable::sCliffTable.clear();
    return 0;
}

// Returns the terrain at an index, or NULL when out of range or unused (negative id)
EETerrainTable* EETerrainTable::Get(long inIndex)
{
    if ((unsigned long)inIndex < EETerrainTable::sTable.size() && EETerrainTable::sTable[inIndex]->mID >= 0)
        return EETerrainTable::sTable[inIndex];
    return NULL;
}

// Destroys the terrain (members only)
EETerrainTable::~EETerrainTable()
{
}
