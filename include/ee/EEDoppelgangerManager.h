// EEDoppelgangerManager.h (RECONSTRUCTION; file name inferred).
// RTTI EEDoppelgangerManager (vtable 0x446ed8, one slot: the deleting dtor); Units/EEDoppelgangerManager.cpp
// 0x201fd9..0x20360e. The game's instance is gDoppelgangerManager (0x52a328; Empire Earth.cpp's init chain constructs
// it). It owns two object pools (the doppelgangers other players see of units they lost sight of) and keeps them on
// the map's tiles (EETile::mDoppelgangers). Names are provisional. Requires lle/LowLevelEngine.h; includes
// lle/UMemoryPool.h.
#ifndef EE_EEDOPPELGANGERMANAGER_H
#define EE_EEDOPPELGANGERMANAGER_H

#include "lle/UMemoryPool.h"

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
    EEDoppelgangerManager();                // 0x201fd9
    virtual ~EEDoppelgangerManager();       // +0x00 0x202012 (??_G 0x201ff6)
    void Release();                         // 0x202039: each pool's DumpStats(false), then its Release
    // 0x202066: a doppelganger read from inFile (older games: the record is skipped, 0) (provisional name)
    EEDoppelganger* Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    void Free(EEDoppelganger* inDoppelganger);       // 0x2021bc: back into its pool (a doppelganger whose unit or player is gone)
    // 0x202222: whether inTile holds inUnit's doppelganger for player inPlayer (provisional name)
    bool HasDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer);
    bool HasDoppelganger(const EETile* inTile, EEUnit* inUnit);   // 0x202285: for any player (provisional name)
    // 0x2022c4: inUnit's doppelganger for player inPlayer on inTile, 0 if none (provisional name)
    EEDoppelganger* GetDoppelganger(const EETile* inTile, EEUnit* inUnit, long inPlayer);
    // 0x202318: puts inDoppelganger on the tiles of its unit's footprint at (inX, inY) (provisional name)
    void AddToTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways);
    void TileHidden(RTSTile* inTile, EEPlayer* inPlayer);  // 0x20248b (EEPlayer::RemoveSharedSight; provisional name)
    static bool IsOutOfSight(EEUnit* inUnit, EEPlayer* inPlayer);  // 0x20266e (cdecl) (provisional name)
    void AddForUnseenUnit(EEComplexUnit* inUnit);  // 0x2027a3 (provisional name)
    EEDoppelganger* NewDoppelganger(bool inComplex);   // 0x2028d6: from one of the pools (provisional name)
    void TileShown(EETile* inTile, long inX, long inY, EEPlayer* inPlayer);  // 0x2028f2 (provisional name)
    void UnregisterPlayer(EEPlayer* inPlayer);  // 0x2029ba (provisional name)
    // 0x202afd: takes inDoppelganger off the tiles of its footprint at (inX, inY) (provisional name)
    void RemoveFromTiles(EEDoppelganger* inDoppelganger, long inX, long inY, bool inIncludeRunways);
    // 0x202c70 / 0x202c85: the pools' SetSize(inChunkSize, inMaxChunks, 1) (provisional names)
    bool SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks);
    bool SetComplexPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks);

    UMemoryPool<EEDoppelganger> mPool;              // +0x04 (vtable 0x446ee0)
    UMemoryPool<EEComplexDoppelganger> mComplexPool;   // +0x3c (vtable 0x446ee8)
};

extern EEDoppelgangerManager gDoppelgangerManager;          // 0x52a328

#endif
