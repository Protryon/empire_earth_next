// C:\EEX\Empire Earth\Map\EETile.cpp (file name inferred: no __FILE__, no asserts). A map tile's game side (RTTI
// EETile > RTSTile, vtable 0x441d0c; the tile-unit list nodes EETUnitList > RTSTUnitList): 0x121191..0x121c78 (the
// node pool's `$E` chain, the class in definition order, then the template COMDATs of the pool and the node classes'
// deleting destructors; RTSMap.cpp's key chains open the next unit). Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"
#include "rust/game/gamecpp.h"

// chain at 0x521191 (wrapper)
//   object 0x9297e0  ctor UMemoryPool<EETUnitList>::UMemoryPool 0x121734  dtor (unguarded: a plain global) 0x1211b1
// The tile-unit list nodes (provisional name).
UMemoryPool<EETUnitList> gTileUnitListPool;

// Destroys the tile, releasing every unit standing on it
EETile::~EETile()
{
    this->RemoveAllUnits();
}

// Records a building image (doppelganger) on the tile
void EETile::AddDoppelganger(EEDoppelganger* inDoppelganger)
{
    this->mDoppelgangers.push_back(inDoppelganger);
}

// Takes a unit list node from the pool for a unit (holding a reference to it) ahead of inNext; returns the node
RTSTileUnitNode* EETile::AddUnit(EEUnit* inUnit, RTSTileUnitNode* inNext)
{
    EETUnitList* theNode = gTileUnitListPool.New();
    inUnit->Reference();
    theNode->mUnit = inUnit;
    theNode->mNext = inNext;
    return theNode;
}

// Reports the tile unit list pool's usage statistics, then frees the pool
void EETile::ReleaseUnitListPool()
{
    gTileUnitListPool.DumpStats(false);
    gTileUnitListPool.Release();
}

// Releases a unit list node's unit reference and returns the node to the pool
void EETile::FreeUnitNode(RTSTileUnitNode* inNode)
{
    inNode->mUnit->Dereference();
    inNode->mUnit = NULL;
    inNode->mNext = NULL;
    gTileUnitListPool.Delete((EETUnitList*)inNode);
}

// Returns the tile's base checksum plus the unit ids of its building images
long EETile::GetChecksum(unsigned long inPlayerCount) const
{
    return ee_gamecpp_tile_checksum(this, inPlayerCount);
}

// Reads the tile and its building images from the saved game; returns the image count's read result
long EETile::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                  unsigned long inPlayerCount)
{
    return ee_gamecpp_tile_load(this, inMajorVersion, inMinorVersion, &inFile, inPlayerCount);
}

// Writes the tile's building image unit ids, then its base state, to the out-of-sync dump
void EETile::Dump(FILE* inFile, unsigned long inPlayerCount) const
{
    ee_gamecpp_tile_dump(this, inFile, inPlayerCount);
}

// Removes a building image from the tile, if it is there
void EETile::RemoveDoppelganger(EEDoppelganger* inDoppelganger)
{
    ee_gamecpp_tile_remove_image(this, inDoppelganger);
}

// Writes the tile and its valid building images that belong to this tile (or to none) to the saved game;
// returns the first error
long EETile::Save(FSFile& outFile, unsigned long inPlayerCount)
{
    return ee_gamecpp_tile_save(this, &outFile, inPlayerCount);
}

// Sets the tile's terrain type and its space flag; a tile below sea level becomes deep water when deep,
// else shallow water unless three of its corners are dry
void EETile::SetTerrainType(long inTerrainType, const EETileLocation& inLocation, RTSMap* inMap)
{
    ee_gamecpp_tile_set_terrain_type(this, inTerrainType, inLocation.mX, inLocation.mY, inMap);
}

// Sizes the tile unit list pool (chunk size and chunk limit)
void EETile::SetUnitListPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    gTileUnitListPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// ------------------------------------------------------------------------- round 9, lane B: the tile's bodies' C++

// the calamity hosts' cases (EEContagionManager.cpp), then lane C's
int64_t EECore9OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpB5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kTerrainDeepWater == 1 && kTerrainShallowWater == 10 && kTerrainSpace == 12, "gamecpp tile.rs");

#define P64(x) ((int64_t)(intptr_t)(x))

// The C++ calls and members of the tile's bodies in Rust (gamecpp tile.rs; EeGameCpp's lane B section, after
// EEMiniMap.cpp's EECore9OpB3): one statement each
int64_t EECore9OpB4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TlRtsChecksum: return ((const EETile*)p)->RTSTile::GetChecksum((unsigned long)a[0]);
    case O::TlRtsLoad: ((EETile*)p)->RTSTile::Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2], (unsigned long)a[3]);
        break;
    case O::TlRtsSave: return ((EETile*)p)->RTSTile::Save(*(FSFile*)a[0], (unsigned long)a[1]);
    case O::TlRtsDump: ((const EETile*)p)->RTSTile::Dump((FILE*)a[0], (unsigned long)a[1]);
        break;
    case O::TlLoadImage: return P64(gDoppelgangerManager.Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]));
    case O::TlImageSetTile: ((EEDoppelganger*)p)->mTile = (EETile*)a[0];
        break;
    case O::TlImageValid: return ((EEDoppelganger*)p)->GetIsValid();
    case O::TlImageTileNull: return ((EEDoppelganger*)p)->GetTile() == NULL;
    case O::TlImageSave: return ((EEDoppelganger*)p)->Save(0, 0, *(FSFile*)a[0]);
    case O::TlEraseImage: ((EETile*)p)->mDoppelgangers.erase(((EETile*)p)->mDoppelgangers.begin() + a[0]);
        break;
    case O::TlDumpImage: fprintf((FILE*)p, "Doppelganger %ld has unit id of %ld\n", (unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TlWriteUlong: return ((FSFile*)p)->Write((unsigned long)a[0]);
    case O::TlDynMap: return P64(dynamic_cast<EEMap*>((RTSMap*)p));
    case O::TlSetIsSpace: ((EEMap*)p)->mTerrainMesh.SetIsSpace((unsigned long)a[0], (unsigned long)a[1], a[2] != 0);
        break;
    case O::TlValidPoint: return ((EEMap*)p)->mTerrainMesh.GetIsValidPoint((unsigned long)a[0], (unsigned long)a[1]);
    case O::TlSetTerrainTypeField: ((EETile*)p)->mTerrainType = (long)a[0];
        break;
    default: return EECore9OpB5(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpB4F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TlHeight: return ((EETile*)p)->GetHeight();
    case O::TlMapTileSize: return gMapTileSize;
    case O::TlMeshZ: return ((EEMap*)p)->mTerrainMesh.GetZ((long)a[0], (long)a[1]);
    default: return EECore9OpB5F(inOp, p, a, f);
    }
}
