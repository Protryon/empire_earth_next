// C:\EEX\Empire Earth\Units\EEUCAmbient.cpp (file name inferred: no __FILE__; the class name). The ambient object's
// unit class: 0x20b9fc..0x20bcf7 (after EEUCAirplane.cpp; EEUCBasicCitizen.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x04, +0x08, +0x64, +0x7c, +0x84, +0x98, +0xb8 in
// source order (unit type 0x234 sits on the water plane).
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

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEUCAmbient.h"

// FUNCTION: EXE 0x60b9fc
// Puts cattails on the water plane (height 0); other ambient objects keep their height
void EEUCAmbient::GroundLocation(GE3DPoint& ioLocation)
{
    if (this->mUnitType == kUnitTypeAmbCattails1)
        ioLocation.SetZ(0.0f);
}

// FUNCTION: EXE 0x60ba14
// For cattails, returns the tile where the view line through a point meets the water plane; otherwise sets nothing
void EEUCAmbient::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    if (this->mUnitType != kUnitTypeAmbCattails1)
        return;
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x60bac9
// Returns whether the ambient unit can be selected: only path points can
bool EEUCAmbient::IsSelectable(EEUnit* inUnit)
{
    return inUnit->mUnitClass->mUnitType == kUnitTypePathPoint;
}

// FUNCTION: EXE 0x60bae4
// Sets up a new ambient unit like any unit
void EEUCAmbient::SimpleUnitCreated(EEUnit* inUnit)
{
    EEUnitClass::SimpleUnitCreated(inUnit);
}

// FUNCTION: EXE 0x60baf0
// Adds the unit to every map tile of its footprint, its own tile first
void EEUCAmbient::AddToTile(EEUnit* inUnit)
{
    long theSize = this->GetTileSize();
    EEMap* theMap = ::gWorld.mMap;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theEndX = theX + theSize;
    long theEndY = theY + theSize;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
    for (long i = theX; i < theEndX; i++) {
        for (long j = theY; j < theEndY; j++) {
            if (i == theX && j == theY)
                continue;
            theTile = theMap->GetTile(i, j);
            if (theTile != NULL)
                theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
        }
    }
}

// FUNCTION: EXE 0x60bbd0
// Removes the unit from every map tile of its footprint, its own tile first
void EEUCAmbient::RemoveFromTile(EEUnit* inUnit)
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

// FUNCTION: EXE 0x60bc8e
// While rotting, counts down the unit's rotting time and gives its linked parts the same time
void EEUCAmbient::Update(EEComplexUnit* inUnit)
{
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionUnitRotting)
        return;
    long theTime = inUnit->UpdateRottingAwayTime();
    for (EEComplexUnit* theUnit = inUnit->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart)
        theUnit->SetRottingAwayTime(theTime);
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
bool EEUCAmbient::IsPickable()
{
    return false;
}
