// C:\EEX\Empire Earth\Units\EEDoppelgangerManager.cpp (file name inferred: no __FILE__; the class name). The pools of
// doppelgangers (what a player still sees of units he lost sight of) and their tiles: 0x201fd9..0x20360e (after
// EEDoppelganger.cpp; EEGhostUnit.cpp follows). Unit flags: the exe's module set. vtable 0x446ed8; the pools'
// template members, EEDoppelganger's/EEComplexDoppelganger's ??_E (from the chunks' new[]) and the chunk ctors
// close the unit in first-reference order.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEComplexDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEGameState.h"
#include "ee/EEMiniMap.h"
#include "ee/EEAreaUtilities.h"

// FUNCTION: EXE 0x601fd9
// Creates the manager with empty simple and complex doppelganger pools
EEDoppelgangerManager::EEDoppelgangerManager()
{
}

// SYNTHETIC: EXE 0x601ff6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x602012
// Destroys the manager (the pools free themselves)
EEDoppelgangerManager::~EEDoppelgangerManager()
{
}

// FUNCTION: EXE 0x602039
// Logs the stats of both doppelganger pools and frees them
void EEDoppelgangerManager::Release()
{
    this->mPool.DumpStats(false);
    this->mPool.Release();
    this->mComplexPool.DumpStats(false);
    this->mComplexPool.Release();
}

// FUNCTION: EXE 0x602066
// Reads one doppelganger from the saved game into a new pooled one and queues it for pointer repair;
// skips an old-format record (minor version < 0xbd), returning NULL
EEDoppelganger* EEDoppelgangerManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEDoppelganger* theDoppelganger = NULL;
    if (inMajorVersion == 0 && inMinorVersion < 0xbd) {
        // old saves: the doppelganger record is skipped
        unsigned long theValue;
        long theCoordinate;
        bool theFlag;
        if (inMinorVersion <= 0x18) {
            inFile.Read(theValue);
            inFile.Read(theCoordinate);
            inFile.Read(theCoordinate);
            inFile.Read(theCoordinate);
            inFile.Read(theValue);
            inFile.Read(theFlag);
        } else {
            if (inMinorVersion <= 0x64) {
                inFile.Read(theValue);
                inFile.Read(theCoordinate);
                inFile.Read(theCoordinate);
                inFile.Read(theCoordinate);
                inFile.Read(theValue);
                inFile.Read(theFlag);
                if (inMinorVersion < 0x27)
                    return theDoppelganger;
            } else {
                inFile.Read(theValue);
                inFile.Read(theCoordinate);
                inFile.Read(theCoordinate);
                inFile.Read(theCoordinate);
                inFile.Read(theValue);
            }
            inFile.Read(theValue);
            inFile.Read(theValue);
        }
    } else {
        bool theComplex;
        if (inFile.Read(theComplex) == kErrorOK) {
            theDoppelganger = this->NewDoppelganger(theComplex);
            theDoppelganger->Load(inMajorVersion, inMinorVersion, inFile);
            gGameState.mRestorables.push_back(theDoppelganger);
        }
    }
    return theDoppelganger;
}

// FUNCTION: EXE 0x6021bc
// Clears a doppelganger and returns it to its pool
void EEDoppelgangerManager::Free(EEDoppelganger* inDoppelganger)
{
    if (inDoppelganger->IsComplex()) {
        dynamic_cast<EEComplexDoppelganger*>(inDoppelganger)->Clear();
        this->mComplexPool.Delete(dynamic_cast<EEComplexDoppelganger*>(inDoppelganger));
    } else {
        inDoppelganger->Clear();
        this->mPool.Delete(inDoppelganger);
    }
}

// FUNCTION: EXE 0x602222
// Returns whether a tile holds the given player's doppelganger of a unit
bool EEDoppelgangerManager::HasDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer)
{
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    bool theFound = false;
    if (inTile != NULL) {
        for (unsigned long i = 0; i < inTile->GetNumDoppelgangers() && !theFound; i++) {
            EEDoppelganger* theDoppelganger = inTile->mDoppelgangers[i];
            if (theDoppelganger->mPlayer == thePlayer && inUnit == theDoppelganger->mUnit)
                theFound = true;
            else
                theFound = false;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x602285
// Returns whether a tile holds any player's doppelganger of a unit
bool EEDoppelgangerManager::HasDoppelganger(const EETile* inTile, EEUnit* inUnit)
{
    bool theFound = false;
    for (unsigned long i = 0; i < inTile->GetNumDoppelgangers(); i++) {
        if (inUnit == inTile->mDoppelgangers[i]->mUnit) {
            theFound = true;
            break;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x6022c4
// Returns the given player's doppelganger of a unit on a tile, or NULL
EEDoppelganger* EEDoppelgangerManager::GetDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer)
{
    EEDoppelganger* theResult = NULL;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    for (unsigned long i = 0; i < inTile->GetNumDoppelgangers(); i++) {
        EEDoppelganger* theDoppelganger = inTile->mDoppelgangers[i];
        if (theDoppelganger->mPlayer == thePlayer && inUnit == theDoppelganger->mUnit) {
            theResult = theDoppelganger;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x602318
// Puts a doppelganger on every tile of its unit's square, for an airport also on the 3 runway squares ahead
void EEDoppelgangerManager::AddToTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways)
{
    unsigned long theSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    for (unsigned long i = 0; i < theSize; i++) {
        for (unsigned long j = 0; j < theSize; j++) {
            EEMapTile* theTile = ::gWorld.mMap->GetTile(inX + j, inY + i);
            if (theTile != NULL)
                theTile->AddDoppelganger(inDoppelganger);
        }
    }
    if (inIncludeRunways != true)
        return;
    switch (inDoppelganger->mUnit->mComplexUnit->mFacing) {
    case 0:
        this->AddToTiles(inDoppelganger, inX, inY + theSize, false);
        this->AddToTiles(inDoppelganger, inX, inY + 2 * theSize, false);
        this->AddToTiles(inDoppelganger, inX, inY + 3 * theSize, false);
        break;
    case 1:
        this->AddToTiles(inDoppelganger, inX, inY - theSize, false);
        this->AddToTiles(inDoppelganger, inX, inY - 2 * theSize, false);
        this->AddToTiles(inDoppelganger, inX, inY - 3 * theSize, false);
        break;
    case 2:
        this->AddToTiles(inDoppelganger, inX + theSize, inY, false);
        this->AddToTiles(inDoppelganger, inX + 2 * theSize, inY, false);
        this->AddToTiles(inDoppelganger, inX + 3 * theSize, inY, false);
        break;
    default:
        this->AddToTiles(inDoppelganger, inX - theSize, inY, false);
        this->AddToTiles(inDoppelganger, inX - 2 * theSize, inY, false);
        this->AddToTiles(inDoppelganger, inX - 3 * theSize, inY, false);
        break;
    }
}

// FUNCTION: EXE 0x60248b
// As a tile drops out of a player's sight, leaves him doppelgangers of the resources, ambients, fish and
// built land buildings there (foundations only his own) that are wholly out of sight; all parts of a complex one
void EEDoppelgangerManager::TileHidden(RTSTile* inTile, EEPlayer* inPlayer)
{
    bool theIsCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer;
    unsigned long thePlayerID = inPlayer->mPlayerID;
    if (inTile == NULL)
        return;
    RTSTileUnitNode* theNode = inTile->mUnits;
    if (theNode == NULL || inTile->GetVisibility(thePlayerID) > 1)
        return;
    do {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit != NULL) {
            EEUnitClass* theClass = theUnit->mUnitClass;
            unsigned long theType = theClass->mUnitType;
            long theCategory = theClass->mCategory;
            if (theType != kUnitTypePathPoint && theType != kUnitTypeWaypoint && !theClass->mIsFlare && !theClass->mIsBoobyTrap
                && (theCategory == kUnitCategoryResource || theCategory == kUnitCategoryAmbient || theClass->mFamily == kUnitFamilyFish
                    || (theClass->IsLandBuilding() && theUnit->mHitPoints > 0
                        && (!theUnit->mComplexUnit->IsFoundation() || theUnit->mPlayer == inPlayer)))
                && !this->HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, thePlayerID)
                && EEDoppelgangerManager::IsOutOfSight(theUnit, inPlayer)) {
                if (theUnit->mComplexUnit != NULL) {
                    EEComplexUnit* theCarrier = theUnit->mComplexUnit;
                    while (theCarrier->mPreviousPart != NULL)
                        theCarrier = theCarrier->mPreviousPart;
                    theUnit = theCarrier;
                }
                bool theIsWall = theUnit->mUnitClass->mUnitType == kUnitTypeAirport;
                while (theUnit != NULL) {
                    RTSTile* theTile = theUnit->mTile;
                    if (theTile != NULL && theUnit->mUnitClass->mScale != 0.0f) {
                        EEDoppelganger* theDoppelganger =
                            this->NewDoppelganger(theUnit->mComplexUnit != NULL || theUnit->mUnitClass->mTileSize > 1);
                        theDoppelganger->Initialize(theUnit, inPlayer, theTile);
                        if (theIsCurrent)
                            gMiniMap->AddDoppelganger(theDoppelganger, theUnit->mTileX, theUnit->mTileY, theIsWall);
                        this->AddToTiles(theDoppelganger, theUnit->mTileX, theUnit->mTileY, theIsWall);
                    }
                    theUnit = theUnit->mComplexUnit != NULL && !theIsWall ? theUnit->mComplexUnit->mNextPart : 0;
                }
            }
        }
        theNode = theNode->mNext;
    } while (theNode != NULL);
}

// FUNCTION: EXE 0x60266e
// Returns whether every tile under the unit (all parts of a complex one) is out of the player's sight;
// false for a cloaked unit or one whose tech tree node is of kind 4
bool EEDoppelgangerManager::IsOutOfSight(EEUnit* inUnit, EEPlayer* inPlayer)
{
    EEMap* theMap = ::gWorld.mMap;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    bool theResult = true;
    if (theUnit != NULL) {
        while (theUnit->mPreviousPart != NULL)
            theUnit = theUnit->mPreviousPart;
        inUnit = theUnit;
        theResult = theUnit->GetTechTreeNode() != NULL && theUnit->GetTechTreeNode()->mData->mKind == 4 ? false : true;
        theResult &= (theUnit->mIsCloakable ? theUnit->mIsCloaked : false) == false;
    }
    while (inUnit != NULL && theResult) {
        long theSize = inUnit->mUnitClass->mTileSize;
        if (theSize <= 1)
            theSize = 1;
        unsigned long theX = inUnit->mTileX;
        unsigned long theY = inUnit->mTileY;
        unsigned long theXEnd = theX + theSize;
        unsigned long theYEnd = theY + theSize;
        for (unsigned long i = theX; i < theXEnd && theResult; i++) {
            for (unsigned long j = theY; j < theYEnd && theResult; j++) {
                EEMapTile* theTile = theMap->GetTile(i, j);
                if (theTile != NULL && theTile->GetVisibility(inPlayer->mPlayerID) <= 1)
                    theResult = true;
                else
                    theResult = false;
            }
        }
        inUnit = inUnit->mComplexUnit ? inUnit->mComplexUnit->mNextPart : 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x6027a3
// Remembers a resource, ambient, fish or building its own player cannot see (when he keeps a visibility
// map): clears his doppelgangers on its tile and adds a new one there and on the minimap
void EEDoppelgangerManager::AddForUnseenUnit(EEComplexUnit* inUnit)
{
    RTSTile* theTile = inUnit->mTile;
    EEPlayer* thePlayer = inUnit->mPlayer;
    if (theTile == NULL || thePlayer->mSaveVisMap != true || EEIsSeenBy(inUnit, thePlayer, true))
        return;
    EEUnitClass* theClass = inUnit->mUnitClass;
    long theKind = theClass->mFamily;
    long theCategory = theClass->mCategory;
    if (((!theClass->IsLandBuilding() || !(inUnit->mHitPoints > 0) || inUnit->mComplexUnit->mIsCloakable
          || (inUnit->mComplexUnit->IsFoundation() && inUnit->mPlayer != thePlayer))
         && theCategory != kUnitCategoryResource && theCategory != kUnitCategoryAmbient && theKind != kUnitFamilyFish)
        || inUnit->mUnitClass->mScale == 0.0f)
        return;
    this->TileShown(static_cast<EETile*>(inUnit->mTile), inUnit->mTileX, inUnit->mTileY, inUnit->GetPlayer());
    bool theIsWall = inUnit->mUnitClass->mUnitType == kUnitTypeAirport;
    EEDoppelganger* theDoppelganger =
        this->NewDoppelganger(inUnit->mComplexUnit != NULL || inUnit->mUnitClass->mTileSize > 1);
    theDoppelganger->Initialize(inUnit, thePlayer, theTile);
    if (::gWorld.mPlayers[::gWorld.mCurrentPlayer] == thePlayer)
        gMiniMap->AddDoppelganger(theDoppelganger, inUnit->mTileX, inUnit->mTileY, theIsWall);
    this->AddToTiles(theDoppelganger, inUnit->mTileX, inUnit->mTileY, theIsWall);
}

// FUNCTION: EXE 0x6028d6
// Takes a new doppelganger from the complex or the simple pool
EEDoppelganger* EEDoppelgangerManager::NewDoppelganger(bool inComplex)
{
    if (inComplex == false)
        return this->mPool.New();
    return this->mComplexPool.New();
}

// FUNCTION: EXE 0x6028f2
// As a tile comes into a player's sight, removes his doppelgangers on it from the map and minimap and frees them
void EEDoppelgangerManager::TileShown(EETile* inTile, long inX, long inY, EEPlayer* inPlayer)
{
    bool theIsCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer;
    unsigned long i = 0;
    while (i < inTile->GetNumDoppelgangers()) {
        EEDoppelganger*& theEntry = inTile->mDoppelgangers[i];
        EEDoppelganger* theDoppelganger = theEntry;
        EEComplexDoppelganger* theComplex = theDoppelganger->GetComplexDoppelganger();
        if (theDoppelganger->mPlayer == inPlayer) {
            bool theIsWall = theDoppelganger->mUnit->mUnitClass->mUnitType == kUnitTypeAirport;
            long theX = inX;
            long theY = inY;
            if (theComplex != NULL) {
                theX = theComplex->mTileX;
                theY = theComplex->mTileY;
            }
            if (theIsCurrent)
                gMiniMap->RemoveDoppelganger(theDoppelganger, theX, theY, theIsWall);
            this->RemoveFromTiles(theDoppelganger, theX, theY, theIsWall);
            this->Free(theDoppelganger);
        } else
            i++;
    }
}

// FUNCTION: EXE 0x6029ba
// Removes and frees every doppelganger a player sees or owns the unit of, over the whole map
void EEDoppelgangerManager::UnregisterPlayer(EEPlayer* inPlayer)
{
    bool theIsCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == inPlayer;
    EEMap* theMap = ::gWorld.mMap;
    if (theMap == NULL)
        return;
    long theSize = theMap->mSize;
    for (long x = 0; x < theSize; x++) {
        for (long y = 0; y < theSize; y++) {
            EEMapTile* theTile = theMap->GetTile(x, y);
            if (theTile == NULL)
                continue;
            unsigned long i = 0;
            while (i < theTile->GetNumDoppelgangers()) {
                EEDoppelganger*& theEntry = theTile->mDoppelgangers[i];
                EEDoppelganger* theDoppelganger = theEntry;
                if ((theDoppelganger->GetTile() == NULL || theDoppelganger->GetTile() == theTile)
                    && (theDoppelganger->mPlayer == inPlayer || theDoppelganger->GetPlayer() == inPlayer)) {
                    bool theIsWall = theDoppelganger->mUnit->mUnitClass->mUnitType == kUnitTypeAirport;
                    if (theIsCurrent && theDoppelganger->mPlayer == inPlayer)
                        gMiniMap->RemoveDoppelganger(theDoppelganger, x, y, theIsWall);
                    this->RemoveFromTiles(theDoppelganger, x, y, theIsWall);
                    this->Free(theDoppelganger);
                } else
                    i++;
            }
        }
    }
}

// FUNCTION: EXE 0x602afd
// Takes a doppelganger off every tile of its unit's square, for an airport also off the 3 runway squares
void EEDoppelgangerManager::RemoveFromTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways)
{
    unsigned long theSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    for (unsigned long i = 0; i < theSize; i++) {
        for (unsigned long j = 0; j < theSize; j++) {
            EEMapTile* theTile = ::gWorld.mMap->GetTile(inX + j, inY + i);
            if (theTile != NULL)
                theTile->RemoveDoppelganger(inDoppelganger);
        }
    }
    if (inIncludeRunways != true)
        return;
    switch (inDoppelganger->mUnit->mComplexUnit->mFacing) {
    case 0:
        this->RemoveFromTiles(inDoppelganger, inX, inY + theSize, false);
        this->RemoveFromTiles(inDoppelganger, inX, inY + 2 * theSize, false);
        this->RemoveFromTiles(inDoppelganger, inX, inY + 3 * theSize, false);
        break;
    case 1:
        this->RemoveFromTiles(inDoppelganger, inX, inY - theSize, false);
        this->RemoveFromTiles(inDoppelganger, inX, inY - 2 * theSize, false);
        this->RemoveFromTiles(inDoppelganger, inX, inY - 3 * theSize, false);
        break;
    case 2:
        this->RemoveFromTiles(inDoppelganger, inX + theSize, inY, false);
        this->RemoveFromTiles(inDoppelganger, inX + 2 * theSize, inY, false);
        this->RemoveFromTiles(inDoppelganger, inX + 3 * theSize, inY, false);
        break;
    default:
        this->RemoveFromTiles(inDoppelganger, inX - theSize, inY, false);
        this->RemoveFromTiles(inDoppelganger, inX - 2 * theSize, inY, false);
        this->RemoveFromTiles(inDoppelganger, inX - 3 * theSize, inY, false);
        break;
    }
}

// FUNCTION: EXE 0x602c70
// Sets the simple doppelganger pool's chunk size and chunk limit; returns whether it succeeded
bool EEDoppelgangerManager::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return this->mPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// FUNCTION: EXE 0x602c85
// Sets the complex doppelganger pool's chunk size and chunk limit; returns whether it succeeded
bool EEDoppelgangerManager::SetComplexPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return this->mComplexPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// TEMPLATE: EXE 0x602c9a
// UMemoryPool<EEDoppelganger>::UMemoryPool

// TEMPLATE: EXE 0x602cd4
// UMemoryPool<EEComplexDoppelganger>::UMemoryPool

// TEMPLATE: EXE 0x602d0e
// UMemoryPool<EEDoppelganger>::`scalar deleting destructor'

// TEMPLATE: EXE 0x602d30
// UMemoryPool<EEComplexDoppelganger>::`scalar deleting destructor'

// TEMPLATE: EXE 0x602d52
// UMemoryPool<EEDoppelganger>::Release (the <EEComplexDoppelganger> copy is ICF-folded into it)

// TEMPLATE: EXE 0x602db6
// UMemoryPool<EEDoppelganger>::Delete

// TEMPLATE: EXE 0x602e10
// UMemoryPool<EEDoppelganger>::DumpStats

// TEMPLATE: EXE 0x602e73
// UMemoryPool<EEDoppelganger>::New

// TEMPLATE: EXE 0x602f55
// UMemoryPool<EEDoppelganger>::SetSize

// TEMPLATE: EXE 0x603062
// UMemoryPool<EEComplexDoppelganger>::Delete

// TEMPLATE: EXE 0x6030c6
// UMemoryPool<EEComplexDoppelganger>::DumpStats

// TEMPLATE: EXE 0x603129
// UMemoryPool<EEComplexDoppelganger>::New

// TEMPLATE: EXE 0x60320b
// UMemoryPool<EEComplexDoppelganger>::SetSize

// TEMPLATE: EXE 0x603318
// UMemoryPoolChunk<EEDoppelganger>::UMemoryPoolChunk

// TEMPLATE: EXE 0x603398
// UMemoryPoolChunk<EEComplexDoppelganger>::UMemoryPoolChunk

// TEMPLATE: EXE 0x603418
// UMemoryPoolChunk<EEDoppelganger>::~UMemoryPoolChunk (<EEComplexDoppelganger>'s ICF-folded into it)

// TEMPLATE: EXE 0x603433
// EEDoppelganger::`vector deleting destructor'

// TEMPLATE: EXE 0x60348e
// EEComplexDoppelganger::`vector deleting destructor'

// TEMPLATE: EXE 0x6034e9
// UMemoryPool<EEDoppelganger>::AddChunk

// TEMPLATE: EXE 0x60355c
// UMemoryPool<EEComplexDoppelganger>::AddChunk

// TEMPLATE: EXE 0x6035cf
// an inline tile member (the player's visibility byte; first used by TileHidden; not written)

// FUNCTION: EXE 0x6035ef
// Returns whether a terrain kind is land (not deep or shallow water, cliff or space)
// Kinds 1, 9, 10, 12 are not land. An ordinary function at the end of this unit, called from EEGhostUnit.cpp
// and EECloakingUtilities.cpp 0x23d2f9 (provisional name)
bool EEIsLandTerrain(long inTerrainType)
{
    return inTerrainType != kTerrainDeepWater && inTerrainType != kTerrainShallowWater && inTerrainType != kTerrainCliff && inTerrainType != kTerrainSpace;
}
