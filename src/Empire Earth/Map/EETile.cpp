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
#include "lle/FSFile.h"
#include "lle/UMemoryPool.h"

#include "ee/EETileLocation.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"

// chain at 0x521191 (wrapper)
//   object 0x9297e0  ctor UMemoryPool<EETUnitList>::UMemoryPool 0x121734  dtor (unguarded: a plain global) 0x1211b1
// The tile-unit list nodes (provisional name).
UMemoryPool<EETUnitList> gTileUnitListPool;
// SYNTHETIC: EXE 0x521191   (wrapper)
// SYNTHETIC: EXE 0x52119b   (ctor)
// SYNTHETIC: EXE 0x5211a5   (registrar)
// SYNTHETIC: EXE 0x5211b1   (dtor)

// FUNCTION: EXE 0x5211c5
// Destroys the tile, releasing every unit standing on it
EETile::~EETile()
{
    this->RemoveAllUnits();
}

// FUNCTION: EXE 0x5211e4
// Records a building image (doppelganger) on the tile
void EETile::AddDoppelganger(EEDoppelganger* inDoppelganger)
{
    this->mDoppelgangers.push_back(inDoppelganger);
}

// FUNCTION: EXE 0x5211f4
// Takes a unit list node from the pool for a unit (holding a reference to it) ahead of inNext; returns the node
RTSTileUnitNode* EETile::AddUnit(EEUnit* inUnit, RTSTileUnitNode* inNext)
{
    EETUnitList* theNode = gTileUnitListPool.New();
    inUnit->Reference();
    theNode->mUnit = inUnit;
    theNode->mNext = inNext;
    return theNode;
}

// FUNCTION: EXE 0x521212
// Reports the tile unit list pool's usage statistics, then frees the pool
void EETile::ReleaseUnitListPool()
{
    gTileUnitListPool.DumpStats(false);
    gTileUnitListPool.Release();
}

// FUNCTION: EXE 0x52122a
// Releases a unit list node's unit reference and returns the node to the pool
void EETile::FreeUnitNode(RTSTileUnitNode* inNode)
{
    inNode->mUnit->Dereference();
    inNode->mUnit = NULL;
    inNode->mNext = NULL;
    gTileUnitListPool.Delete((EETUnitList*)inNode);
}

// FUNCTION: EXE 0x52124e
// Returns the tile's base checksum plus the unit ids of its building images
long EETile::GetChecksum(unsigned long inPlayerCount) const
{
    long theSum = 0;
    for (unsigned long i = 0; i < this->GetNumDoppelgangers(); i++)
        theSum += this->mDoppelgangers[i]->mUnit->mUnitID;
    return RTSTile::GetChecksum(inPlayerCount) + theSum;
}

// FUNCTION: EXE 0x52128d
// Reads the tile and its building images from the saved game; returns the image count's read result
long EETile::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                  unsigned long inPlayerCount)
{
    RTSTile::Load(inMajorVersion, inMinorVersion, inFile, inPlayerCount);
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EEDoppelganger* theDoppelganger = gDoppelgangerManager.Load(inMajorVersion, inMinorVersion, inFile);
            if (theDoppelganger)
                theDoppelganger->mTile = this;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5212ec
// Writes the tile's building image unit ids, then its base state, to the out-of-sync dump
void EETile::Dump(FILE* inFile, unsigned long inPlayerCount) const
{
    for (unsigned long i = 0; i < this->GetNumDoppelgangers(); i++)
        fprintf(inFile, "Doppelganger %ld has unit id of %ld\n", i,this->mDoppelgangers[i]->mUnit->mUnitID);
    RTSTile::Dump(inFile, inPlayerCount);
}

// FUNCTION: EXE 0x52133c
// Removes a building image from the tile, if it is there
void EETile::RemoveDoppelganger(EEDoppelganger* inDoppelganger)
{
    std::vector<EEDoppelganger*>::iterator theIterator = std::find(this->mDoppelgangers.begin(), this->mDoppelgangers.end(), inDoppelganger);
    if (theIterator != this->mDoppelgangers.end())
        this->mDoppelgangers.erase(theIterator);
}

// FUNCTION: EXE 0x52139e
// Writes the tile and its valid building images that belong to this tile (or to none) to the saved game;
// returns the first error
// NEAR: inline budget: the original calls begin() + i out of line for the first three mU18[i], inlines the last
long EETile::Save(FSFile& outFile, unsigned long inPlayerCount)
{
    std::vector<EEDoppelganger*> theDoppelgangers;
    long theResult = RTSTile::Save(outFile, inPlayerCount);
    if (theResult == 0) {
        for (unsigned long i = 0; i < this->mDoppelgangers.size(); i++) {
            if (this->mDoppelgangers[i]->GetIsValid()) {
                if (this->mDoppelgangers[i]->GetTile() == NULL || this->mDoppelgangers[i]->GetTile() == this)
                    theDoppelgangers.push_back(this->mDoppelgangers[i]);
            }
        }
        unsigned long theCount = theDoppelgangers.size();
        theResult = outFile.Write(theCount);
        if (theResult == 0) {
            for (unsigned long j = 0; j < theDoppelgangers.size() && theResult == 0; j++)
                theResult = theDoppelgangers[j]->Save(0, 0, outFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5214bf
// Sets the tile's terrain type and its space flag; a tile below sea level becomes deep water when deep,
// else shallow water unless three of its corners are dry
// NEAR: register/slot allocation: the original reloads inLocation after the first UMFloor (asm) corner
void EETile::SetTerrainType(long inTerrainType, const EETileLocation& inLocation, RTSMap* inMap)
{
    GETerrainMesh& theMesh = dynamic_cast<EEMap*>(inMap)->mTerrainMesh;
    long theX = inLocation.mX;
    long theY = inLocation.mY;
    theMesh.SetIsSpace(theX, theY, inTerrainType == kTerrainSpace);
    if (this->GetHeight() < 0.0f) {
        if (this->GetHeight() < -(gF52988C * 0.75f))
            inTerrainType = kTerrainDeepWater;
        else {
            long theCount = 0;
            if (theMesh.GetIsValidPoint(theX, theY))
                theCount += UMFloor(theMesh.GetZ(theX, theY) >= 0.0f ? 1 : 0);
            if (theMesh.GetIsValidPoint(theX + 1, theY))
                theCount += UMFloor(theMesh.GetZ(theX + 1, theY) >= 0.0f ? 1 : 0);
            if (theMesh.GetIsValidPoint(theX, theY + 1))
                theCount += UMFloor(theMesh.GetZ(theX, theY + 1) >= 0.0f ? 1 : 0);
            if (theMesh.GetIsValidPoint(theX + 1, theY + 1))
                theCount += UMFloor(theMesh.GetZ(theX + 1, theY + 1) >= 0.0f ? 1 : 0);
            if (theCount < 3)
                inTerrainType = kTerrainShallowWater;
        }
    }
    this->mTerrainType = inTerrainType;
}

// FUNCTION: EXE 0x5216f3
// Sizes the tile unit list pool (chunk size and chunk limit)
void EETile::SetUnitListPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    gTileUnitListPool.SetSize(inChunkSize, inMaxChunks, 1);
}

// TEMPLATE: EXE 0x521708
// (vector<EEDoppelganger*>::push_back; the template COMDATs close the unit)

// TEMPLATE: EXE 0x521734
// (UMemoryPool<EETUnitList>::UMemoryPool)

// SYNTHETIC: EXE 0x52176e
// (UMemoryPool<EETUnitList>'s scalar deleting destructor)

// TEMPLATE: EXE 0x521790
// (UMemoryPool<EETUnitList>::Release)

// TEMPLATE: EXE 0x5217f4
// (UMemoryPool<EETUnitList>::Delete)

// TEMPLATE: EXE 0x521857
// (UMemoryPool<EETUnitList>::DumpStats)

// TEMPLATE: EXE 0x5218ba
// (UMemoryPool<EETUnitList>::New)

// TEMPLATE: EXE 0x5219a9
// (UMemoryPool<EETUnitList>::SetSize)

// TEMPLATE: EXE 0x521ab6
// (UMemoryPoolChunk<EETUnitList>::UMemoryPoolChunk)

// SYNTHETIC: EXE 0x521b37
// (EETUnitList's vector deleting destructor)

// TEMPLATE: EXE 0x521ba7
// (UMemoryPool<EETUnitList>::CountUsed)

// TEMPLATE: EXE 0x521bdc
// (UMemoryPool<EETUnitList>::AddChunk)

// SYNTHETIC: EXE 0x521c4f
// (RTSTUnitList's scalar deleting destructor)
