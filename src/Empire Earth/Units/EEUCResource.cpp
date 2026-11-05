// C:\EEX\Empire Earth\Units\EEUCResource.cpp (file name inferred: no __FILE__; the class name). The resource's unit
// class: 0x219c49..0x219d67 (after EEUCProjectile.cpp; EEUCSeaAnimal.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (no new site in the range); +0x70/+0x98 are ICF-folded elsewhere.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUCResource.h"

// FUNCTION: EXE 0x619c49
// Sets up a new resource unit with the class's resource type and amount
void EEUCResource::SimpleUnitCreated(EEUnit* inUnit)
{
    EEUnitClass::SimpleUnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// FUNCTION: EXE 0x619c7e
// Adds the resource to every tile of its footprint (its own tile first), unless it is queued for deletion
void EEUCResource::AddToTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    if (inUnit->mIsQueuedForDelete)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            EEMapTile* theOther = theMap->GetTile(i, j);
            if (theOther != NULL)
                theOther->mUnits = theOther->AddUnit(inUnit, theOther->mUnits);
        }
    }
}

// ICF-FOLDED: EXE 0x60bbd0
// Removes the unit from every map tile of its footprint, its own tile first
// (no address of its own: the same body as EEUCAmbient::RemoveFromTile, which ICF kept)
void EEUCResource::RemoveFromTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->RemoveUnit(inUnit);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            EEMapTile* theOther = theMap->GetTile(i, j);
            if (theOther != NULL)
                theOther->RemoveUnit(inUnit);
        }
    }
}
