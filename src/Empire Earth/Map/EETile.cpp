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

// Sizes the tile unit list pool (chunk size and chunk limit)
void EETile::SetUnitListPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    gTileUnitListPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// ------------------------------------------------------------------------- round 9, lane B: the tile's bodies' C++

// (their cases: EEMap.cpp's EECore9OpB2)
// (its constants are checked against the headers by gamecpp cpp_consts_tests.rs)
