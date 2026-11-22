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
#include "rust/sim/units.h"
#include "ee/EEGameState.h"
#include "ee/EEMiniMap.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECombatGoals.h"

// Creates the manager with empty simple and complex doppelganger pools
EEDoppelgangerManager::EEDoppelgangerManager()
{
}

// Destroys the manager (the pools free themselves)
EEDoppelgangerManager::~EEDoppelgangerManager()
{
}

// Logs the stats of both doppelganger pools and frees them
void EEDoppelgangerManager::Release()
{
    this->mPool.DumpStats(false);
    this->mPool.Release();
    this->mComplexPool.DumpStats(false);
    this->mComplexPool.Release();
}

// Reads one doppelganger from the saved game into a new pooled one and queues it for pointer repair;
// skips an old-format record (minor version < 0xbd), returning NULL
EEDoppelganger* EEDoppelgangerManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return reinterpret_cast<EEDoppelganger*>(static_cast<uintptr_t>(ee_units_dm_body(EE_UNITS_DM_DO_LOAD, &inFile, inMajorVersion, inMinorVersion, 0)));   // dm_body.rs
}

// Clears a doppelganger and returns it to its pool
void EEDoppelgangerManager::Free(EEDoppelganger* inDoppelganger)
{
    ee_units_dm_body(EE_UNITS_DM_DO_FREE, inDoppelganger, 0, 0, 0);   // dm_body.rs
}

// Returns whether a tile holds the given player's doppelganger of a unit
bool EEDoppelgangerManager::HasDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer)
{
    return ee_units_dm_body(EE_UNITS_DM_DO_HAS_OF_PLAYER, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, inPlayer, 0) != 0;   // dm_body.rs
}

// Returns whether a tile holds any player's doppelganger of a unit
bool EEDoppelgangerManager::HasDoppelganger(const EETile* inTile, EEUnit* inUnit)
{
    return ee_units_dm_body(EE_UNITS_DM_DO_HAS, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, 0, 0) != 0;   // dm_body.rs
}

// Returns the given player's doppelganger of a unit on a tile, or NULL
EEDoppelganger* EEDoppelgangerManager::GetDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer)
{
    return reinterpret_cast<EEDoppelganger*>(static_cast<uintptr_t>(ee_units_dm_body(EE_UNITS_DM_DO_GET, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, inPlayer, 0)));   // dm_body.rs
}

// Puts a doppelganger on every tile of its unit's square, for an airport also on the 3 runway squares ahead
void EEDoppelgangerManager::AddToTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways)
{
    ee_units_dm_body(EE_UNITS_DM_DO_ADD_TO_TILES, inDoppelganger, inX, inY, inIncludeRunways);   // dm_body.rs
}

// As a tile drops out of a player's sight, leaves him doppelgangers of the resources, ambients, fish and
// built land buildings there (foundations only his own) that are wholly out of sight; all parts of a complex one
void EEDoppelgangerManager::TileHidden(RTSTile* inTile, EEPlayer* inPlayer)
{
    ee_units_dm_body(EE_UNITS_DM_DO_TILE_HIDDEN, inTile, (int64_t)(uintptr_t)inPlayer, 0, 0);   // dm_body.rs
}

// Returns whether every tile under the unit (all parts of a complex one) is out of the player's sight;
// false for a cloaked unit or one whose tech tree node is of kind 4
bool EEDoppelgangerManager::IsOutOfSight(EEUnit* inUnit, EEPlayer* inPlayer)
{
    return ee_units_dm_body(EE_UNITS_DM_DO_IS_OUT_OF_SIGHT, inUnit, (int64_t)(uintptr_t)inPlayer, 0, 0) != 0;   // dm_body.rs
}

// Remembers a resource, ambient, fish or building its own player cannot see (when he keeps a visibility
// map): clears his doppelgangers on its tile and adds a new one there and on the minimap
void EEDoppelgangerManager::AddForUnseenUnit(EEComplexUnit* inUnit)
{
    ee_units_dm_body(EE_UNITS_DM_DO_ADD_FOR_UNSEEN, inUnit, 0, 0, 0);   // dm_body.rs
}

// Takes a new doppelganger from the complex or the simple pool
EEDoppelganger* EEDoppelgangerManager::NewDoppelganger(bool inComplex)
{
    if (inComplex == false)
        return this->mPool.New();
    return this->mComplexPool.New();
}

// As a tile comes into a player's sight, removes his doppelgangers on it from the map and minimap and frees them
void EEDoppelgangerManager::TileShown(EETile* inTile, long inX, long inY, EEPlayer* inPlayer)
{
    ee_units_dm_body(EE_UNITS_DM_DO_TILE_SHOWN, inTile, inX, inY, (int64_t)(uintptr_t)inPlayer);   // dm_body.rs
}

// Removes and frees every doppelganger a player sees or owns the unit of, over the whole map
void EEDoppelgangerManager::UnregisterPlayer(EEPlayer* inPlayer)
{
    ee_units_dm_body(EE_UNITS_DM_DO_UNREGISTER_PLAYER, inPlayer, 0, 0, 0);   // dm_body.rs
}

// Takes a doppelganger off every tile of its unit's square, for an airport also off the 3 runway squares
void EEDoppelgangerManager::RemoveFromTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways)
{
    ee_units_dm_body(EE_UNITS_DM_DO_REMOVE_FROM_TILES, inDoppelganger, inX, inY, inIncludeRunways);   // dm_body.rs
}

// Sets the simple doppelganger pool's chunk size and chunk limit; returns whether it succeeded
bool EEDoppelgangerManager::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return this->mPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// Sets the complex doppelganger pool's chunk size and chunk limit; returns whether it succeeded
bool EEDoppelgangerManager::SetComplexPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return this->mComplexPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// Returns whether a terrain kind is land (not deep or shallow water, cliff or space)
// Kinds 1, 9, 10, 12 are not land. An ordinary function at the end of this unit, called from EEGhostUnit.cpp
// and EECloakingUtilities.cpp 0x23d2f9 (provisional name)
bool EEIsLandTerrain(long inTerrainType)
{
    return inTerrainType != kTerrainDeepWater && inTerrainType != kTerrainShallowWater && inTerrainType != kTerrainCliff && inTerrainType != kTerrainSpace;
}

// dm_body.rs's scratch values: the saved game's file and the bool read last (Load's locals)
static FSFile* sDmFile;
static bool sDmFlag;
static unsigned long sDmValue;
static long sDmCoordinate;

template <class T> static T* EEDmObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// The C++ of EEDoppelgangerManager's Rust bodies (ee_units dm_body.rs; EE_UNITS_DM_*): one statement of the
// originals each, in their order; inA an object (a doppelganger, a tile, a unit, a player), inB and inC values or a
// second object. The manager is gDoppelgangerManager, the one there is.
extern "C" int64_t ee_units_dm_host(int32_t inOp, uint64_t inA, int64_t inB, int64_t inC)
{
    EEDoppelgangerManager& theManager = gDoppelgangerManager;
    EEDoppelganger* theDoppelganger = EEDmObject<EEDoppelganger>(inA);
    EEMapTile* theTile = EEDmObject<EEMapTile>(inA);
    EEUnit* theUnit = EEDmObject<EEUnit>(inA);
    EEPlayer* thePlayer = EEDmObject<EEPlayer>(inA);
    RTSTileUnitNode* theNode = EEDmObject<RTSTileUnitNode>(inA);
    switch (inOp) {
    case EE_UNITS_DM_SET_FILE: sDmFile = EEDmObject<FSFile>(inA); return 0;
    case EE_UNITS_DM_READ_ULONG: return sDmFile->Read(sDmValue);
    case EE_UNITS_DM_READ_LONG: return sDmFile->Read(sDmCoordinate);
    case EE_UNITS_DM_READ_BOOL: return sDmFile->Read(sDmFlag);
    case EE_UNITS_DM_FLAG: return sDmFlag;
    case EE_UNITS_DM_NEW: return (uint64_t)(uintptr_t)theManager.NewDoppelganger(inB != 0);
    case EE_UNITS_DM_LOAD: theDoppelganger->Load((unsigned long)inB, (unsigned long)inC, *sDmFile); return 0;
    case EE_UNITS_DM_RESTORABLE: gGameState.mRestorables.push_back(theDoppelganger); return 0;
    case EE_UNITS_DM_IS_COMPLEX: return theDoppelganger->IsComplex();
    case EE_UNITS_DM_COMPLEX_CLEAR: dynamic_cast<EEComplexDoppelganger*>(theDoppelganger)->Clear(); return 0;
    case EE_UNITS_DM_COMPLEX_POOL_DELETE: theManager.mComplexPool.Delete(dynamic_cast<EEComplexDoppelganger*>(theDoppelganger)); return 0;
    case EE_UNITS_DM_CLEAR: theDoppelganger->Clear(); return 0;
    case EE_UNITS_DM_POOL_DELETE: theManager.mPool.Delete(theDoppelganger); return 0;
    case EE_UNITS_DM_PLAYER_AT: return (uint64_t)(uintptr_t)::gWorld.mPlayers[inB];
    case EE_UNITS_DM_NUM_DOPPELGANGERS: return (int64_t)theTile->GetNumDoppelgangers();
    case EE_UNITS_DM_DOPPELGANGER_AT: return (uint64_t)(uintptr_t)theTile->mDoppelgangers[(size_t)inB];
    case EE_UNITS_DM_DOPPELGANGER_PLAYER: return (uint64_t)(uintptr_t)theDoppelganger->mPlayer;
    case EE_UNITS_DM_DOPPELGANGER_UNIT: return (uint64_t)(uintptr_t)theDoppelganger->mUnit;
    case EE_UNITS_DM_DOPPELGANGER_TILE_SIZE: return theDoppelganger->mUnit->mUnitClass->mTileSize;
    case EE_UNITS_DM_MAP_TILE: return (uint64_t)(uintptr_t)::gWorld.mMap->GetTile((long)inB, (long)inC);
    case EE_UNITS_DM_TILE_ADD: theTile->AddDoppelganger(EEDmObject<EEDoppelganger>((uint64_t)inB)); return 0;
    case EE_UNITS_DM_TILE_REMOVE: theTile->RemoveDoppelganger(EEDmObject<EEDoppelganger>((uint64_t)inB)); return 0;
    case EE_UNITS_DM_DOPPELGANGER_FACING: return theDoppelganger->mUnit->mComplexUnit->mFacing;
    case EE_UNITS_DM_IS_CURRENT: return ::gWorld.mPlayers[::gWorld.mCurrentPlayer] == thePlayer;
    case EE_UNITS_DM_PLAYER_ID: return thePlayer->mPlayerID;
    case EE_UNITS_DM_TILE_FIRST_NODE: return (uint64_t)(uintptr_t)EEDmObject<RTSTile>(inA)->mUnits;
    case EE_UNITS_DM_TILE_VISIBILITY: return EEDmObject<RTSTile>(inA)->GetVisibility((unsigned long)inB);
    case EE_UNITS_DM_NODE_UNIT: return (uint64_t)(uintptr_t)theNode->mUnit;
    case EE_UNITS_DM_NODE_NEXT: return (uint64_t)(uintptr_t)theNode->mNext;
    case EE_UNITS_DM_UNIT_TYPE: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_DM_CLASS_FLAG: return inB == 0 ? theUnit->mUnitClass->mIsFlare : theUnit->mUnitClass->mIsBoobyTrap;
    case EE_UNITS_DM_CATEGORY: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_DM_FAMILY: return theUnit->mUnitClass->mFamily;
    case EE_UNITS_DM_IS_LAND_BUILDING: return theUnit->mUnitClass->IsLandBuilding();
    case EE_UNITS_DM_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_DM_IS_FOUNDATION: return theUnit->mComplexUnit->IsFoundation();
    case EE_UNITS_DM_UNIT_PLAYER: return (uint64_t)(uintptr_t)theUnit->mPlayer;
    case EE_UNITS_DM_UNIT_TILE: return (uint64_t)(uintptr_t)theUnit->mTile;
    case EE_UNITS_DM_UNIT_COMPLEX: return (uint64_t)(uintptr_t)theUnit->mComplexUnit;
    case EE_UNITS_DM_PREVIOUS_PART: return (uint64_t)(uintptr_t)EEDmObject<EEComplexUnit>(inA)->mPreviousPart;
    case EE_UNITS_DM_NEXT_PART: return (uint64_t)(uintptr_t)EEDmObject<EEComplexUnit>(inA)->mNextPart;
    case EE_UNITS_DM_HAS_SCALE: return theUnit->mUnitClass->mScale != 0.0f;
    case EE_UNITS_DM_UNIT_TILE_SIZE: return theUnit->mUnitClass->mTileSize;
    case EE_UNITS_DM_INITIALIZE:
        theDoppelganger->Initialize(EEDmObject<EEUnit>((uint64_t)inB), EEDmObject<EEPlayer>((uint64_t)inC), EEDmObject<EEUnit>((uint64_t)inB)->mTile);
        return 0;
    case EE_UNITS_DM_MINIMAP_ADD:
        gMiniMap->AddDoppelganger(theDoppelganger, EEDmObject<EEUnit>((uint64_t)inB)->mTileX, EEDmObject<EEUnit>((uint64_t)inB)->mTileY, inC != 0);
        return 0;
    case EE_UNITS_DM_UNIT_TILE_XY: return inB == 0 ? theUnit->mTileX : theUnit->mTileY;
    case EE_UNITS_DM_IS_WONDER:
        return EEDmObject<EEComplexUnit>(inA)->GetTechTreeNode() != NULL && EEDmObject<EEComplexUnit>(inA)->GetTechTreeNode()->mData->mKind == 4;
    case EE_UNITS_DM_IS_CLOAKED:
        return EEDmObject<EEComplexUnit>(inA)->mIsCloakable ? EEDmObject<EEComplexUnit>(inA)->mIsCloaked : false;
    case EE_UNITS_DM_SAVES_VIS_MAP: return thePlayer->mSaveVisMap == true;
    case EE_UNITS_DM_IS_SEEN_BY: return EECombatIsSeenBy(EEDmObject<EEComplexUnit>(inA), EEDmObject<EEPlayer>((uint64_t)inB), true);
    case EE_UNITS_DM_IS_CLOAKABLE: return theUnit->mComplexUnit->mIsCloakable;
    case EE_UNITS_DM_GET_PLAYER: return (uint64_t)(uintptr_t)theUnit->GetPlayer();
    case EE_UNITS_DM_COMPLEX_OF: return (uint64_t)(uintptr_t)theDoppelganger->GetComplexDoppelganger();
    case EE_UNITS_DM_COMPLEX_TILE: return inB == 0 ? EEDmObject<EEComplexDoppelganger>(inA)->mTileX : EEDmObject<EEComplexDoppelganger>(inA)->mTileY;
    case EE_UNITS_DM_MINIMAP_REMOVE: gMiniMap->RemoveDoppelganger(theDoppelganger, (long)inB, (long)inC, false); return 0;
    case EE_UNITS_DM_MINIMAP_REMOVE_WALL: gMiniMap->RemoveDoppelganger(theDoppelganger, (long)inB, (long)inC, true); return 0;
    case EE_UNITS_DM_HAS_MAP: return ::gWorld.mMap != NULL;
    case EE_UNITS_DM_MAP_SIZE: return ::gWorld.mMap->mSize;
    case EE_UNITS_DM_DOPPELGANGER_TILE: return (uint64_t)(uintptr_t)theDoppelganger->GetTile();
    case EE_UNITS_DM_DOPPELGANGER_GET_PLAYER: return (uint64_t)(uintptr_t)theDoppelganger->GetPlayer();
    case EE_UNITS_DM_DOPPELGANGER_UNIT_TYPE: return theDoppelganger->mUnit->mUnitClass->mUnitType;
    default: return 0;
    }
}
