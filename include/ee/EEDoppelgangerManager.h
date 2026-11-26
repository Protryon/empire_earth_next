// EEDoppelgangerManager.h (RECONSTRUCTION; file name inferred).
// RTTI EEDoppelgangerManager (vtable 0x446ed8, one slot: the deleting dtor); Units/EEDoppelgangerManager.cpp
// 0x201fd9..0x20360e. The game's instance is gDoppelgangerManager (0x52a328; Empire Earth.cpp's init chain constructs
// it). It owns two object pools (the doppelgangers other players see of units they lost sight of) and keeps them on
// the map's tiles (EETile::mDoppelgangers). Names are provisional. New (units round 11): the bodies are inline
// here over Rust (ee_sim units dm_body.rs, dm_dop.rs). Requires lle/LowLevelEngine.h; includes
// lle/UList.h (UMemoryPool).
#ifndef EE_EEDOPPELGANGERMANAGER_H
#define EE_EEDOPPELGANGERMANAGER_H

#include "lle/UList.h"   // UMemoryPool
#include "rust/sim/units.h"             // the inline bodies' Rust calls (dm_body.rs, dm_dop.rs)

class EEComplexDoppelganger;
class EEComplexUnit;
class EEDoppelganger;
class EEPlayer;
class EETile;
class EEUnit;
class FSFile;
class RTSTile;

class EEDoppelgangerManager {
public:
    EEDoppelgangerManager();
    virtual ~EEDoppelgangerManager();       // +0x00 0x202012 (??_G 0x201ff6)
    void Release() { ee_units_dm_dop(EE_UNITS_DM_DG_DO_MANAGER_RELEASE, NULL, NULL, NULL, NULL); }   // 0x202039: each pool's DumpStats(false), then its Release (dm_dop.rs)
    // 0x202066: a doppelganger read from inFile (older games: the record is skipped, 0) (provisional name)
    EEDoppelganger* Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return reinterpret_cast<EEDoppelganger*>(static_cast<uintptr_t>(ee_units_dm_body(EE_UNITS_DM_DO_LOAD, &inFile, inMajorVersion, inMinorVersion, 0))); }
    void Free(EEDoppelganger* inDoppelganger) { ee_units_dm_body(EE_UNITS_DM_DO_FREE, inDoppelganger, 0, 0, 0); }   // 0x2021bc: cleared, back into its pool
    // 0x202222: whether inTile holds inUnit's doppelganger for player inPlayer (provisional name)
    bool HasDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer) { return ee_units_dm_body(EE_UNITS_DM_DO_HAS_OF_PLAYER, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, inPlayer, 0) != 0; }
    bool HasDoppelganger(const EETile* inTile, EEUnit* inUnit) { return ee_units_dm_body(EE_UNITS_DM_DO_HAS, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, 0, 0) != 0; }   // 0x202285: for any player (provisional name)
    // 0x2022c4: inUnit's doppelganger for player inPlayer on inTile, 0 if none (provisional name)
    EEDoppelganger* GetDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer) { return reinterpret_cast<EEDoppelganger*>(static_cast<uintptr_t>(ee_units_dm_body(EE_UNITS_DM_DO_GET, const_cast<EETile*>(inTile), (int64_t)(uintptr_t)inUnit, inPlayer, 0))); }
    // 0x202318: puts inDoppelganger on the tiles of its unit's footprint at (inX, inY) (provisional name)
    void AddToTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways) { ee_units_dm_body(EE_UNITS_DM_DO_ADD_TO_TILES, inDoppelganger, inX, inY, inIncludeRunways); }
    // 0x20248b (EEPlayer::RemoveSharedSight; provisional name): as a tile drops out of a player's sight, leaves him
    // doppelgangers of the resources, ambients, fish and built land buildings there that are wholly out of sight
    void TileHidden(RTSTile* inTile, EEPlayer* inPlayer) { ee_units_dm_body(EE_UNITS_DM_DO_TILE_HIDDEN, inTile, (int64_t)(uintptr_t)inPlayer, 0, 0); }
    // whether every tile under the unit (all parts of a complex one) is out of the player's sight; false for a
    // cloaked unit or one whose tech tree node is of kind 4
    static bool IsOutOfSight(EEUnit* inUnit, EEPlayer* inPlayer) { return ee_units_dm_body(EE_UNITS_DM_DO_IS_OUT_OF_SIGHT, inUnit, (int64_t)(uintptr_t)inPlayer, 0, 0) != 0; }
    // remembers a resource, ambient, fish or building its own player cannot see (with a visibility map)
    void AddForUnseenUnit(EEComplexUnit* inUnit) { ee_units_dm_body(EE_UNITS_DM_DO_ADD_FOR_UNSEEN, inUnit, 0, 0, 0); }
    EEDoppelganger* NewDoppelganger(bool inComplex);   // 0x2028d6: from one of the pools (provisional name)
    // as a tile comes into a player's sight: his doppelgangers on it off the map and minimap, freed
    void TileShown(EETile* inTile, long inX, long inY, EEPlayer* inPlayer) { ee_units_dm_body(EE_UNITS_DM_DO_TILE_SHOWN, inTile, inX, inY, (int64_t)(uintptr_t)inPlayer); }
    // every doppelganger a player sees or owns the unit of, over the whole map, removed and freed
    void UnregisterPlayer(EEPlayer* inPlayer) { ee_units_dm_body(EE_UNITS_DM_DO_UNREGISTER_PLAYER, inPlayer, 0, 0, 0); }
    // 0x202afd: takes inDoppelganger off the tiles of its footprint at (inX, inY) (provisional name)
    void RemoveFromTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways) { ee_units_dm_body(EE_UNITS_DM_DO_REMOVE_FROM_TILES, inDoppelganger, inX, inY, inIncludeRunways); }
    // 0x202c70 / 0x202c85: the pools' SetSize(inChunkSize, inMaxChunks, 1) (provisional names)
    bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks) { return this->mPool.SetSize(inChunkSize, inMaxChunks, 1); }
    bool SetComplexPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks) { return this->mComplexPool.SetSize(inChunkSize, inMaxChunks, 1); }

    UMemoryPool<EEDoppelganger> mPool;              // +0x04 (vtable 0x446ee0)
    UMemoryPool<EEComplexDoppelganger> mComplexPool;   // +0x3c (vtable 0x446ee8)
};

extern EEDoppelgangerManager gDoppelgangerManager;

#endif
