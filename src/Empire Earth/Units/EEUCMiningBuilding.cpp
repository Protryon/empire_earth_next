// C:\EEX\Empire Earth\Units\EEUCMiningBuilding.cpp (file name inferred: no __FILE__; the class name). The mining
// building's unit class (a mine built on a resource unit): 0x217085..0x217337 (after EEUCLandTransport.cpp;
// EEUCMissile.cpp follows). Unit flags: the exe's module set. Its ctor is inline (vtable 0x44843c).
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

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCMiningBuilding.h"

// FUNCTION: EXE 0x617085
// Creates a mine at a placement and ties it to the first resource under its footprint that it can gather
EEUnit* EEUCMiningBuilding::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                         EEUnit* inCreator)
{
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(inTransformation.GetX());
    long theY = UMFloor(inTransformation.GetY());
    long theSize = this->mTileSize;
    bool theFound = false;
    EEUnit* theUnit = EEUCBuilding::CreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
    for (long i = theX; i < theX + theSize && !theFound; i++) {
        for (long j = theY; j < theY + theSize && !theFound; j++) {
            EETile* theTile = theMap->GetTile(i, j);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
                EEUnit* theOther = theNode->mUnit;
                if (theOther != NULL && theUnit->CanGather(theOther, theOther->GetTileX(), theOther->GetTileY(), 0, 0)) {
                    theUnit->mComplexUnit->SetMine(theOther);
                    theFound = true;
                }
            }
        }
    }
    return theUnit;
}

// FUNCTION: EXE 0x6171b6
// Creates a mine on a tile, at the tile's height (0 off the map)
EEUnit* EEUCMiningBuilding::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    EETile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    if (theTile != NULL) {
        float theHeight = theTile->mHeight;
        theTransformation.SetX((float)inX);
        theTransformation.SetY((float)inY);
        theTransformation.SetZ(theHeight);
    } else {
        theTransformation.SetX((float)inX);
        theTransformation.SetY((float)inY);
        theTransformation.SetZ(0.0f);
    }
    return EEUCMiningBuilding::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x617231
// Starts the gather timer when the mine starts gathering; other actions go to the building handler
void EEUCMiningBuilding::ActionChanged(EEComplexUnit* inUnit)
{
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionGatherResource)
        EEUCBuilding::ActionChanged(inUnit);
    else
        inUnit->SetWorkTimer(inUnit->mStats->mGatherTimes[2]);
}

// FUNCTION: EXE 0x61726d
// Answers resource depleted (its resource is gone, invalid or empty); other triggers go to the building class
long EEUCMiningBuilding::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    EEUnit* theMine = inUnit->mMine;
    long theResult = 0;
    if (inEvent != kTriggerResourceDepleted)
        theResult = EEUCBuilding::EvaluateCondition(inUnit, inEvent);
    else if (theMine == NULL || !theMine->mIsValid || theMine->mResourceAmount <= 0)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x6172a5
// Updates the building; while gathering, counts the work timer down and, once it is at 0, moves one unit of
// the resource to the player and restarts the timer
void EEUCMiningBuilding::Update(EEComplexUnit* inUnit)
{
    EEUCBuilding::Update(inUnit);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionGatherResource)
        return;
    long theTime = inUnit->mWorkTimer;
    EEUnit* theMine = inUnit->mMine;
    unsigned long theElapsed = gPhysics->mTickTimeMs;
    if (theTime > 0) {
        if (theElapsed == 0)
            theElapsed = 1;
        long theRemaining = theTime - theElapsed;
        if (theRemaining < 0)
            theRemaining = 0;
        (EEUNA(SET_WORK_TIMER, theRemaining, 0), inUnit->SetWorkTimer(theRemaining));
    }
    if (theTime != 0 || theMine == NULL)
        return;
    (EEUN(DEPLETE_MINE), theMine->DepleteResource(1));
    (EEUNA(ADD_RESOURCE, theMine->mResourceType, 1), inUnit->mPlayer->AddResource(theMine->mResourceType, 1));
    (EEUNA(SET_WORK_TIMER, inUnit->mStats->mGatherTimes[2], 0), inUnit->SetWorkTimer(inUnit->mStats->mGatherTimes[2]));
}
