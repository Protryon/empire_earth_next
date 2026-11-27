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
    EEDoppelganger* NewDoppelganger(bool inComplex);   // 0x2028d6: from one of the pools (provisional name)
    // 0x202c70 / 0x202c85: the pools' SetSize(inChunkSize, inMaxChunks, 1) (provisional names)
    bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks) { return this->mPool.SetSize(inChunkSize, inMaxChunks, 1); }
    bool SetComplexPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks) { return this->mComplexPool.SetSize(inChunkSize, inMaxChunks, 1); }

    UMemoryPool<EEDoppelganger> mPool;              // +0x04 (vtable 0x446ee0)
    UMemoryPool<EEComplexDoppelganger> mComplexPool;   // +0x3c (vtable 0x446ee8)
};

extern EEDoppelgangerManager gDoppelgangerManager;

#endif
