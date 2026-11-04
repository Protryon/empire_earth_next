// C:\EEX\Empire Earth\Units\EEUCBuilding.cpp (__FILE__ 0x4c8f28: the volcano assert, line 261). The buildings' unit
// class: 0x20e58d..0x21005e (after EEUCBomb.cpp; EEUCBuildingPriest.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (EEUnitClass.cpp's factory). Order: Virtual04, VirtualE0, EruptVolcano (the volcano erupts),
// CreateUnitAt, CreateUnit, the wall/tower helpers (0x20ea01..0x20ed72: Fn20EBDA = the map quarter a tile faces),
// Virtual44, Virtual74, Virtual78, Virtual84, ReleaseUnit, VirtualA8, Virtual98, VirtualA0, VirtualB4, VirtualBC,
// VirtualB8, UpdateArmed, then EECalamityManager::GetCalamityClass<EECCVolcano>.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCVolcano.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGExplore.h"
#include "ee/EEGhostUnit.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEGameVariant.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGUnloadTransport.h"

// 0x446ff0/0x446ff4: the unit's named constants (one .rdata copy per unit)
static const float kBuildingLift = 0.001f;      // 0x446ff0 (a building sits just above the ground)
static const float kBuildingSunk = -2.0f;       // 0x446ff4 (VirtualA0 6000: below it the building is gone)
// 0x446dc0: the yaw of each map quarter (Fn20EBDA's 0..3) (provisional name)
static const float kQuarterYaws[4] = { 0.0f, 3.1415927f, -1.5707964f, 1.5707964f };
// 0x4c8ef8/0x4c8f08: the quarter for each side, and the steps along the four sides (Fn20EA89) (provisional names)
static long sSideQuarters[4] = { 0, 2, 1, 3 };
static long sSideSteps[4][2] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };

// FUNCTION: EXE 0x60e58d
// Puts docks and navy yards on the water plane and lifts other buildings just above the ground
void EEUCBuilding::GroundLocation(GE3DPoint& ioLocation)
{
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard)
        ioLocation.SetZ(0.0f);
    else
        ioLocation.SetZ(ioLocation.GetZ() + kBuildingLift);
}

// FUNCTION: EXE 0x60e5b8
// Picks the first fully healed garrisoned unit that should leave (a human's unit whose rally queue has a goal, a unit
// with a goal, or one with an explore order) as the exiting unit; returns whether one was picked
bool EEUCBuilding::SelectUnitToRelease(EEComplexUnit* inUnit)
{
    bool theResult = false;
    unsigned long theCount = inUnit->mCargo.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theCargo = inUnit->mCargo[i];
        if (theCargo->GetHitPoints() == theCargo->GetStats()->mMaxHitPoints) {
            bool theLeave = false;
            if (theCargo->GetPlayer()->IsHuman()) {
                long theKind = theCargo->GetUnitClass()->mFamily;
                if (theKind == kUnitFamilyAircraft || theKind == kUnitFamilySpaceFighter) {
                    if (!inUnit->mGoalQueues[1].IsShiftMode() && inUnit->GetQueuedGoal(1) != NULL)
                        theLeave = true;
                } else if (theKind == kUnitFamilyBomber) {
                    if (!theCargo->GetUnitClass()->mIsParatrooperBomber) {
                        if (!inUnit->mGoalQueues[2].IsShiftMode() && inUnit->GetQueuedGoal(2) != NULL)
                            theLeave = true;
                    } else {
                        if (!inUnit->mGoalQueues[4].IsShiftMode() && inUnit->GetQueuedGoal(4) != NULL)
                            theLeave = true;
                    }
                } else {
                    if (!inUnit->mGoalQueues[3].IsShiftMode() && inUnit->GetQueuedGoal(3) != NULL)
                        theLeave = true;
                }
            }
            if (theLeave || theCargo->mGoalState != NULL) {
                inUnit->SetExitingUnitID(theCargo->mUnitID);
                theResult = true;
                if (theCargo->mGoalState != NULL && theCargo->GetPlayer()->IsHuman() && !theCargo->mIsDetachedFromAI) {
                    theCargo->mGoalQueues[0].Clear();
                    theCargo->RefreshGoals();
                    theCargo->RefreshActiveGoal();
                }
            }
            if (theCargo->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = theCargo->mGoalQueues[1].GetFront();
                EEUGExplore* theExplore = dynamic_cast<EEUGExplore*>(theGoal);
                if (theGoal != NULL) {
                    theCargo->mGoalQueues[0].Clear();
                    inUnit->SetExitingUnitID(theCargo->mUnitID);
                    if (theExplore != NULL) {
                        theCargo->StartAction(kActionFindUnexploredArea);
                        theCargo->GetUnitClass()->ActionChanged(theCargo);
                    }
                    theResult = true;
                }
            }
        }
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x60e809
// Erupts a volcano: gives it an attack-ground goal at a random spot near it; returns true
bool EEUCBuilding::EruptVolcano(EEComplexUnit* inUnit)
{
    EECCVolcano* theVolcanoClass = NULL;
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    if (!USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(9, theVolcanoClass))
        ESVerify(__FILE__, 261, "EECalamityManager::Instance()->FindCalamityClass(keectVolcano, theVolcanoClass)"); // the original assert text (provisional names here)
    inUnit->GetPosition(thePosition);
    theVolcanoClass->GetRandomLocation(thePosition, theTarget);
    inUnit->SetGoal(new EEUGAttackGround(theTarget, false), 0, 0);
    return true;
}

// FUNCTION: EXE 0x60e8ae
// Creates a building at a placement: docks and navy yards on the water plane, others (not the camera object) lifted
// just above it
EEUnit* EEUCBuilding::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                   EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard) {
        theTransformation.SetX(theTransformation.GetX());
        theTransformation.SetZ(0.0f);
    } else if (this->mUnitType != kUnitTypeCameraObject)
        theTransformation.SetZ(theTransformation.GetZ() + kBuildingLift);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x60e909
// Creates a building on a tile's centre (docks and navy yards at water level, off-map tiles at the -10000 sentinel)
EEUnit* EEUCBuilding::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GE3DPoint theCenter;
    GE3DPoint theGround;
    GETransformation theTransformation;
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard) {
        theTransformation.SetX(inX + 0.5f);
        theTransformation.SetY(inY + 0.5f);
        theTransformation.SetZ(0.0f);
    } else if (::gWorld.mMap->GetTile(inX, inY) != NULL) {
        theCenter.SetX(inX + 0.5f);
        theCenter.SetY(inY + 0.5f);
        ::gWorld.mMap->GetGroundPoint(theCenter, theGround);
        theGround.SetZ(theGround.GetZ() + kBuildingLift);
        theTransformation.SetX(theGround.GetX());
        theTransformation.SetY(theGround.GetY());
        theTransformation.SetZ(theGround.GetZ());
    } else {
        theTransformation.SetX(-10000.0f);
        theTransformation.SetY(-10000.0f);
    }
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x60ea01
// Counts the tiles of a terrain that follow a tile in steps of (inDX, inDY), up to inLength; returns whether all did
static bool Fn20EA01(unsigned long inX, unsigned long inY, long inDX, long inDY, long inLength, long inTerrain,
                     long* outCount)
{
    EEMap* theMap = ::gWorld.mMap;
    long i;
    unsigned long theX = inX;
    for (i = 0; i < inLength; i++) {
        theX += inDX;
        inY += inDY;
        EEMapTile* theTile = theMap->GetTile(theX, inY);
        if (theTile == NULL || theTile->mTerrainType != inTerrain)
            break;
    }
    *outCount = i;
    return i >= inLength;
}

// FUNCTION: EXE 0x60ea89
// Returns the map quarter facing the most space terrain within 6 tiles of a tile (half weight to the neighbouring
// sides)
long EEGetDockFacing(unsigned long inX, unsigned long inY)
{
    long theResult = 0;
    long theCounts[4];
    long theScores[4];
    long i;
    for (i = 0; i < 4; i++)
        Fn20EA01(inX, inY, sSideSteps[i][0], sSideSteps[i][1], 6, 0xc, &theCounts[i]);
    for (i = 0; i < 4; i++)
        theScores[i] = (theCounts[(i + 3) & 3] >> 1) + (theCounts[(i + 1) & 3] >> 1) + theCounts[i];
    long theBest = -1;
    for (i = 0; i < 4; i++) {
        if (theScores[i] > theBest) {
            theBest = theScores[i];
            theResult = sSideQuarters[i];
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x60eb1c
// Returns a navy yard's facing at a tile: its map quarter if 5 deep-water tiles follow that way, else the next side
// that has them, else the side with the longest run
static long Fn20EB1C(unsigned long inX, unsigned long inY)
{
    long theResult = EEGetFacingQuarter(inX, inY);
    long theBest = 0;
    long theQuarters[4] = { 0, 2, 1, 3 };
    long theSteps[8] = { 0, 1, 1, 0, 0, -1, -1, 0 };
    long theCount;
    long theSide;
    for (theSide = 0; theSide < 4; theSide++) {
        if (theQuarters[theSide] == theResult)
            break;
    }
    long theTries;
    for (theTries = 0; theTries < 4; theTries++) {
        if (Fn20EA01(inX, inY, theSteps[theSide * 2], theSteps[theSide * 2 + 1], 5, 1, &theCount))
            break;
        if (theCount > theBest) {
            theBest = theCount;
            theResult = theQuarters[theSide];
        }
        theSide++;
        if (theSide >= 4)
            theSide = 0;
    }
    if (theTries < 4)
        theResult = theQuarters[theSide];
    return theResult;
}

// FUNCTION: EXE 0x60ebda
// Returns the map quarter (0..3) a tile faces from the middle of its continent (space maps) or of the map
long EEGetFacingQuarter(long inX, long inY)
{
    GEVector theDirection;
    EEMap* theMap = ::gWorld.mMap;
    long theResult = 2;
    float theX = (float)inX;
    float theY = (float)inY;
    long theCenterX;
    long theCenterY;
    float theMiddleX;
    float theMiddleY;
    EEAIContinent* theContinent = NULL;
    if (theMap->mTerrainMesh.GetIsSpaceMap())
        theContinent = gContinentManager.GetContinentAt(inX, inY);
    if (theContinent != NULL) {
        theCenterX = theContinent->mHull.GetCenter().mX;
        theCenterY = theContinent->mHull.GetCenter().mY;
        theMiddleX = (float)theCenterX;
        theMiddleY = (float)theCenterY;
    } else {
        theMiddleY = (float)theMap->mSize * 0.5f;
        theMiddleX = theMiddleY;
        theCenterX = theMap->mSize / 2;
        theCenterY = theCenterX;
    }
    if (inX != theCenterX || inY != theCenterY) {
        theDirection.SetX(theMiddleX - theX);
        theDirection.SetY(theMiddleY - theY);
        theDirection.SetZ(0.0f);
        theDirection.Normalize();
        float theAngle = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
        if (theAngle <= 0.78539819f && theAngle >= -0.78539819f)
            theResult = 0;
        else if (theAngle <= -0.78539819f && theAngle >= -2.3561945f)
            theResult = 2;
        else if (theAngle <= 2.3561945f && theAngle >= 0.78539819f)
            theResult = 3;
        else
            theResult = 1;
    }
    return theResult;
}

// FUNCTION: EXE 0x60ed11
// Returns the map quarter the unit's tile faces
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit)
{
    return EEGetFacingQuarter(inUnit->GetTileX(), inUnit->GetTileY());
}

// FUNCTION: EXE 0x60ed25
// Returns the model part for a wall piece kind (0..8); outFlag is set for the corner kinds 0, 2, 6 and 8
// (inferred meaning)
long EEGetWallPiecePart(long inIndex, bool* outFlag)
{
    long theResult;
    *outFlag = inIndex == 0 || inIndex == 2 || inIndex == 6 || inIndex == 8;
    switch (inIndex) {
    case 0:
    case 3:
        theResult = 3;
        break;
    case 1:
    case 2:
        theResult = 0;
        break;
    case 5:
    case 8:
        theResult = 2;
        break;
    default:
        theResult = 1;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x60ed72
// Returns the first living targetable building found in rings 1..19 around a unit's tile
EEUnit* EEUCBuilding::FindNearestTargetBuilding(EEUnit* inUnit)
{
    std::vector<EETileCoordinate> theTiles;
    EEUnit* theResult = NULL;
    EEMap* theMap = ::gWorld.mMap;
    for (unsigned long theRing = 1; theResult == NULL && theRing < 0x14; theRing++) {
        Fn2492C2(inUnit->GetTileX(), inUnit->GetTileY(), theRing, theTiles);
        for (std::vector<EETileCoordinate>::iterator theTile = theTiles.begin();
             theResult == NULL && theTile != theTiles.end(); theTile++) {
            EEMapTile* theMapTile = theMap->GetTile(theTile->mX, theTile->mY);
            if (theMapTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit != NULL && theUnit->mUnitClass->IsLandBuilding() && theUnit->mHitPoints > 0 &&
                    EEUUGetIsUnitClassTargetable(NULL, theUnit->GetUnitClass())) {
                    theResult = theUnit;
                    break;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x60ee56
// Lists the tiles around a building; an airport's include its runway (four times its size toward its facing)
void EEUCBuilding::GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners,
                             bool inAppend)
{
    float theTop = 0.0f;
    float theLeft = 0.0f;
    float theBottom = 0.0f;
    float theRight = 0.0f;
    if (!inAppend)
        outTiles.clear();
    if (this->mUnitType == kUnitTypeAirport) {
        EEComplexUnit* theUnit = inUnit->mComplexUnit;
        while (theUnit->mPreviousPart != NULL)
            theUnit = theUnit->mPreviousPart;
        long theSize = theUnit->mUnitClass->GetTileSize();
        long theY = theUnit->GetTileY();
        long theX = theUnit->GetTileX();
        switch (EEGetFacingQuarter(theX, theY)) {
        case 0:
            theLeft = (float)theX;
            theRight = (float)(theX + theSize);
            theBottom = (float)theY;
            theTop = (float)(theY + theSize * 4);
            break;
        case 1:
            theLeft = (float)theX;
            theRight = (float)(theX + theSize);
            theBottom = (float)(theY + theSize * -3);
            theTop = (float)(theY + theSize);
            break;
        case 2:
            theLeft = (float)theX;
            theRight = (float)(theX + theSize * 4);
            theBottom = (float)theY;
            theTop = (float)(theY + theSize);
            break;
        case 3:
            theLeft = (float)(theX + theSize * -3);
            theRight = (float)(theX + theSize);
            theBottom = (float)theY;
            theTop = (float)(theY + theSize);
            break;
        }
        EEGetBlockRing(inCorners, UMFloor(theLeft), UMFloor(theBottom), UMFloor(theRight - theLeft),
                   -UMFloor(theBottom - theTop), outTiles, inAppend);
    } else
        EEUnitClass::GetSurroundingTiles(inUnit, outTiles, inCorners, inAppend);
}

// FUNCTION: EXE 0x60f015
// Handles a building's new action: retargets to the nearest building, dies with its linked part, opens the airport,
// asks takeoff clearance or launches the next plane; other actions use the common handling
void EEUCBuilding::ActionChanged(EEComplexUnit* inUnit)
{
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionSearchForSecondaryTarget: {
        EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
        if (theGoal != NULL && theGoal->mTarget != NULL) {
            EEUnit* theTarget = this->FindNearestTargetBuilding(theGoal->mTarget);
            if (theTarget != NULL)
                theGoal->SetTarget(theTarget);
        }
        return;
    }
    case kActionTrackEnemyUnit:
        return;
    case kActionDying:
        inUnit->SetResourceAmount(0);
        if (inUnit->mNextPart != NULL) {
            inUnit->mNextPart->StartAction(kActionDying);
            inUnit->mNextPart->mUnitClass->ActionChanged(inUnit->mNextPart);
        }
        this->HandleActionChanged(inUnit);
        return;
    case kActionResourcesRotting:
        inUnit->SetResourceAmount(0);
        break;
    case kActionPrepareToMove:
        if (inUnit->mUnitClass->mAIKind != kAIKindAircraftCarrier && inUnit->mUnitClass->mUnitType != kUnitTypeSp15SpaceCarrier)
            return;
        break;
    case kActionAirportOpen:
        inUnit->SetExitingUnitID(-1);
        return;
    case kActionWaitForTakeoffClearance:
        if (inUnit->mExitingUnitID != -1) {
            EEComplexUnit* theCargo = inUnit->FindPassenger(inUnit->mExitingUnitID);
            if (theCargo != NULL)
                theCargo->mPlayer->mAirTraffic[theCargo->mUnitClass->mAirTrafficType - 1]->QueueRequest(inUnit, theCargo, 0);
        }
        return;
    case kActionLaunchNextAirplane:
        this->ReleaseUnit(inUnit);
        return;
    }
    this->HandleActionChanged(inUnit);
}

// FUNCTION: EXE 0x60f17f
// Sets up a new building (built, its resource stock, regeneration); an airport turns to its map quarter, a navy yard
// picks its facing and a space dock its yaw
void EEUCBuilding::UnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetBuilt(0, 1, 1);
    EEUnitClass::UnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
    inUnit->SetRegenerationTimer(1000);
    if (this->mUnitType == kUnitTypeAirport) {
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        long theQuarter = EEGetUnitFacingQuarter(inUnit);
        inUnit->SetFacing(theQuarter);
        float theYaw = kQuarterYaws[theQuarter];
        theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
        inUnit->mPhysicalModel.SetLocation(theTransformation);
    } else if (this->mUnitType == kUnitTypeNavyYard)
        inUnit->SetFacing(Fn20EB1C(inUnit->GetTileX(), inUnit->GetTileY()));
    else if (this->mUnitType == kUnitTypeSpaceDock) {
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        theTransformation.SetOrientationYPR(
            EEGetSpaceDockYaw(inUnit->GetTileX() + 1, inUnit->GetTileY() + 1, inUnit->mUnitClass->GetTileSize()), 0.0f, 0.0f);
        inUnit->mPhysicalModel.SetLocation(theTransformation);
    }
}

// FUNCTION: EXE 0x60f2ae
// Adds the building to every tile of its footprint
void EEUCBuilding::AddToTile(EEUnit* inUnit)
{
    EEAddToFootprintTiles(inUnit);
}

// FUNCTION: EXE 0x60f2bb
// Launches the exiting unit: places it at the exit (runway, water or tag point), gives it the building's rally
// goals or its own (else loads it back) and starts its takeoff from this home base
void EEUCBuilding::ReleaseUnit(EEComplexUnit* inUnit)
{
    GEOrientation theOrientation;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    std::deque<EEUnitGoal*>::iterator theIterator;
    long theQueue = 0;
    unsigned long theCargoID = inUnit->mExitingUnitID;
    if (theCargoID == (unsigned long)-1)
        return;
    bool theWaterExit = inUnit->mUnitClass->mAirTrafficType == 3;
    thePosition.SetX((float)inUnit->GetTileX() + (float)this->mTileSize * 0.5f);
    thePosition.SetY((float)inUnit->GetTileY() + (float)this->mTileSize * 0.5f);
    thePosition.SetZ(theWaterExit ? 0.0f : inUnit->mTile->mHeight);
    float theYaw;
    if (inUnit->mUnitClass->mAirTrafficType == 1 || theWaterExit) {
        switch (inUnit->mFacing) {
        case 0: theYaw = 0.0f; break;
        case 1: theYaw = 3.1415927f; break;
        case 2: theYaw = -1.5707964f; break;
        default: theYaw = 1.5707964f; break;
        }
        if (theWaterExit) {
            thePosition.SetX(thePosition.GetX() - 2.0f * UMCos(1.5707964f - theYaw));
            thePosition.SetY(thePosition.GetY() + 2.0f * UMCos(theYaw));
        }
    } else {
        inUnit->mPhysicalModel.GetLocation().ConvertToOrientation(theOrientation);
        theYaw = theOrientation.GetYaw();
        EETagPointList* thePoints = inUnit->GetModel()->mTagPoints[inUnit->GetAnimation()];
        EEComplexUnit* theCargo = inUnit->FindPassenger(theCargoID);
        if (theCargo != NULL) {
            if (thePoints != NULL && !thePoints->mPoints.empty()) {
                thePoints->Scale(theTransformation, inUnit->mStats->GetScale());
                thePoints->mPoints[0]->GetPoint(theCargo->mPhysicalModel.GetFrame(), theTransformation, thePosition);
            }
            theCargo->mPhysicalModel.SetScale(theCargo->mStats->GetScale()
                * ((thePosition.GetZ() * 0.5f) / theCargo->mUnitClass->GetCruiseAltitude() + 0.5f));
        }
    }
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    EEComplexUnit* theCargo = inUnit->UnloadCargoByID(theCargoID, theTransformation);
    theCargo->mOrientation = GEOrientation(theYaw, 0.0f, 0.0f);
    theCargo->mPlanetID = EEGetLandContinentOf(theCargo);
    bool theCarrier = (inUnit->mUnitClass->mCategory == kUnitCategoryNavalBuilding || inUnit->mUnitClass->mCategory == kUnitCategorySpaceBuilding)
        && inUnit->mUnitClass->mAirTrafficType != 0;
    EEUnitGoal* theGoal = NULL;
    if (!theCargo->mGoalQueues[1].mGoals.empty()) {
        theIterator = theCargo->GetQueueGoals(1).begin();
        for (; theIterator != theCargo->GetQueueGoals(1).end(); ++theIterator) {
            EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(*theIterator);
            EEUnitGoal* theCopy;
            if (theAttack != NULL) {
                if (theAttack->Suits(theCargo) && theAttack->mTarget != NULL && theAttack->mTarget->mHitPoints > 0)
                    theCopy = theAttack->Clone();
                else {
                    GE2DPoint theTarget((float)theAttack->mTarget->GetTileX(), (float)theAttack->mTarget->GetTileY());
                    theCopy = new EEUGLocation(theTarget, false, true, false, 0.0f, false, false);
                }
            } else if ((*theIterator)->Suits(theCargo))
                theCopy = (*theIterator)->Clone();
            else {
                (*theIterator)->GetGoalLocation(thePosition);
                theCopy = new EEUGLocation(&thePosition, false, true, false, 0.0f, false, false);
            }
            theCargo->QueueGoal(theCopy, 0);
        }
    } else {
        if (theCarrier)
            theQueue = 1;
        else if (inUnit->mUnitClass->mUnitType == kUnitTypeNavyYard)
            theQueue = 2;
        else if (theCargo->mUnitClass->mCategory == kUnitCategoryAirplane || theCargo->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship) {
            unsigned long theKind = theCargo->mUnitClass->mFamily;
            if (theKind == kUnitFamilyAircraft || theKind == kUnitFamilySpaceFighter)
                theQueue = 1;
            else if (theKind == kUnitFamilyBomber)
                theQueue = theCargo->mUnitClass->mIsParatrooperBomber ? 4 : 2;
            else
                theQueue = 3;
        }
        if (theQueue != 0)
            theGoal = inUnit->GetQueuedGoal(theQueue);
        if (!theCargo->mPlayer->IsHuman() || theGoal == NULL) {
            if (theCargo->GetGoal() != NULL)
                theGoal = theCargo->GetGoal();
            if (theGoal == NULL) {
                inUnit->LoadCargo(theCargo, false, 0);
                if (theCargo == NULL)
                    return;
                theCargo->mPlayer->mAirTraffic[theCargo->mUnitClass->mAirTrafficType - 1]->RemoveFromTakeoffQueues(theCargo);
                inUnit->SetExitingUnitID(-1);
                return;
            }
        }
        if (!theCargo->mIsDetachedFromAI && (theCargo->mPlayer->IsHuman() || theCargo->GetGoal() == NULL)) {
            theCargo->ClearGoalQueue(0);
            theIterator = inUnit->mGoalQueues[theQueue].mGoals.begin();
            for (; theIterator != inUnit->mGoalQueues[theQueue].mGoals.end(); ++theIterator) {
                if (theQueue == 4) {
                    EEUnitGoal* theRally = *theIterator;
                    if (theRally != NULL) {
                        GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
                        bool theLast = theIterator == inUnit->mGoalQueues[4].mGoals.end() - 1;
                        theRally->GetGoalLocation(theTarget);
                        if (theLast && EEIsTileFree(theCargo, UMFloorUnsigned(theTarget.GetX()),
                                                  UMFloorUnsigned(theTarget.GetY())))
                            theGoal = new EEUGUnloadTransport(theCargo, (long)theTarget.GetX(), (long)theTarget.GetY(), false);
                        else
                            theGoal = new EEUGLocation(&theTarget, false, true, false, 0.0f, false, false);
                    }
                } else {
                    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(*theIterator);
                    if (theAttack != NULL) {
                        if (theAttack->Suits(theCargo) && theAttack->mTarget != NULL && theAttack->mTarget->mHitPoints > 0)
                            theGoal = theAttack->Clone();
                        else {
                            GE2DPoint theTarget((float)theAttack->mTarget->GetTileX(), (float)theAttack->mTarget->GetTileY());
                            theGoal = new EEUGLocation(theTarget, false, true, false, 0.0f, false, false);
                        }
                    } else if ((*theIterator)->Suits(theCargo))
                        theGoal = (*theIterator)->Clone();
                    else {
                        (*theIterator)->GetGoalLocation(thePosition);
                        theGoal = new EEUGLocation(&thePosition, false, true, false, 0.0f, false, false);
                    }
                }
                theCargo->QueueGoal(theGoal, 0);
            }
        } else if (!theCargo->mPlayer->IsHuman() && theCargo->mUnitClass->mUnitType == kUnitTypeParatrooperPlane
                   && theCargo->GetGoal() != NULL) {
            GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
            // The plane's own goal is its only (so last) destination: drop the troops there if the tile is free
            bool theLast = true;
            theCargo->GetGoal()->GetGoalLocation(theTarget);
            EEUnitGoal* theCopy;
            if (theLast && EEIsTileFree(theCargo, UMFloorUnsigned(theTarget.GetX()), UMFloorUnsigned(theTarget.GetY())))
                theCopy = new EEUGUnloadTransport(theCargo, (long)theTarget.GetX(), (long)theTarget.GetY(), false);
            else
                theCopy = new EEUGLocation(&theTarget, false, true, false, 0.0f, false, false);
            theCargo->QueueGoal(theCopy, 0);
        }
    }
    theCargo->SetHomeBase(inUnit);
    theCargo->StartAction(kActionTakeoff);
    theCargo->mUnitClass->ActionChanged(theCargo);
    inUnit->SetExitingUnitID(-1);
}

// FUNCTION: EXE 0x60fb6c
// Loads the building class's media
void EEUCBuilding::LoadMedia(EEPlayer* inPlayer)
{
    EEUnitClass::LoadMedia(inPlayer);
}

// FUNCTION: EXE 0x60fb78
// Removes the building from every tile of its footprint
void EEUCBuilding::RemoveFromTile(EEUnit* inUnit)
{
    EERemoveFromFootprintTiles(inUnit);
}

// FUNCTION: EXE 0x60fb85
// Answers the building triggers (fall finished, enemy spotted when built, plane to launch, volcano target, secondary
// target, capitol, launch requests, takeoff clearance); others go to the common unit class
long EEUCBuilding::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerDyingAnimationFinished: {
        long theType = inUnit->mUnitClass->mUnitType;
        if (theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation || theType == kUnitTypeSpaceDock ||
            inUnit->mPhysicalModel.GetLocation().GetZ() < kBuildingSunk)
            theResult = 1;
        break;
    }
    case kTriggerEnemyUnitSpotted:
        if (inUnit->mIsBuilt)
            theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerAirplaneAvailableToLaunch:
        theFlag = this->SelectUnitToRelease(inUnit);
        break;
    case kTriggerChooseRandomVolcanoTarget:
        theFlag = this->EruptVolcano(inUnit);
        break;
    case kTriggerSecondaryTargetFound: {
        EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
        theFlag = theGoal != NULL && theGoal->mTarget->mHitPoints > 0;
        break;
    }
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerCalamityTargetStillValid:
        theFlag = this->mUnitType == kUnitTypeCapitol;
        break;
    case kTriggerNoLaunchRequests:
        if (inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1]->GetQueueLength(inUnit) == 0 ||
            inUnit->mExitingUnitID == -1)
            theResult = 1;
        break;
    case kTriggerTakeoffClearanceGranted: {
        long theID = inUnit->mExitingUnitID;
        if (theID != -1) {
            EEComplexUnit* theCargo = inUnit->FindPassenger(theID);
            if (theCargo != NULL)
                theFlag = theCargo->mIsClearanceGranted;
            else {
                inUnit->SetExitingUnitID(-1);
                EEUnit* theUnit = ::gWorld.GetUnit(theID);
                if (theUnit != NULL && theUnit->mComplexUnit != NULL)
                    inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1]->RemovePlane(inUnit,
                                                                                         theUnit->mComplexUnit);
            }
        }
        break;
    }
    case kTriggerLaunchedAirplane:
        theResult = 1;
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x60fd56
// Unloads the building class's media
void EEUCBuilding::UnloadMedia(EEPlayer* inPlayer)
{
    EEUnitClass::UnloadMedia(inPlayer);
}

// FUNCTION: EXE 0x60fd62
// Updates the water ripples of a living dock or navy yard when the effect detail allows them
void EEUCBuilding::UpdateEffects(EEComplexUnit* inUnit)
{
    if ((this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard) && this->mRippleEffect != NULL && inUnit->mHitPoints != 0 &&
        this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// FUNCTION: EXE 0x60fd9d
// Runs a building's tick: walls and houses only camouflage (calamity 0x17); unarmed ones update as buildings, armed
// ones fight; a built robotic farm adds 1 food each gather interval
void EEUCBuilding::Update(EEComplexUnit* inUnit)
{
    if ((inUnit->mUnitClass->mFamily == kUnitFamilyWalls || inUnit->mUnitClass->mUnitType == kUnitTypeHouse) && inUnit->mHitPoints > 0) {
        if (USingleton<EECalamityManager>::GetInstance()->IsCalamityActive(0x17))
            this->UpdateCamouflageTimer(inUnit);
        return;
    }
    if ((this->mAttack == 0.0f && !this->IsPriest()) || this->mUnitType == kUnitTypeCpMissileBase) {
        if (inUnit->mCurrentPhysicsAction == 2 && this->mPhysicsModel != NULL && inUnit->GetTileX() != -10000)
            this->mPhysicsModel->Step(inUnit);
        if (this->mUnitType == kUnitTypeCapitol && inUnit->mStats->mPower > 0) {
            this->Fn157269(inUnit);
            this->RegeneratePower(inUnit);
        }
        this->UpdateBuilding(inUnit);
    } else
        this->UpdateArmed(inUnit);
    if (inUnit->mUnitClass->mUnitType != kUnitTypeRoboticFarm || !((unsigned long)gPhysics->mGameTime >= inUnit->mGrazeTime) || !inUnit->mIsBuilt)
        return;
    inUnit->mPlayer->AddResource(kResourceFood, 1);
    inUnit->mPlayer->mSavedGroups.Reset(7, 1);
    long theTime = gPhysics->mGameTime;
    long theDelay = inUnit->mStats->mGatherTimes[6];
    float theScale = ::gWorld.GetGameVariant()->mWorkTimeFactor;
    inUnit->SetGrazeTime(theTime + UMFloatToLong(theScale * theDelay));
}

// FUNCTION: EXE 0x60fef9
// Runs an armed building's tick (physics, turrets, timers, animation, sound) and fires on its attack frame; skips
// disabled ones and standing foundations
void EEUCBuilding::UpdateArmed(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    if (inUnit->mIsDisabled && inUnit->mHitPoints > 0)
        return;
    if (inUnit->IsFoundation() && inUnit->mHitPoints > 0 && inUnit->mCurrentPhysicsAction != 2)
        return;
    if (this->mPhysicsModel != NULL && inUnit->GetTileX() != -10000) {
        this->mPhysicsModel->Step(inUnit);
        inUnit->UpdateTurrets();
    }
    inUnit->UpdateOrbitals();
    this->UpdateCamouflageTimer(inUnit);
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    long theAttack = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionTrackEnemyUnit:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theAttack, theFrame);
        break;
    case kActionUnitRotting: {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
        break;
    }
    }
}

// TEMPLATE: EXE 0x60fff7
// EECalamityManager::GetCalamityClass<EECCVolcano> (inline in ee/EECalamityManager.h)
