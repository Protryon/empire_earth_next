// EEUnitClassBuildings.cpp (new, R5b.4 round 32): the buildings' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>
#include <functional>

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
#include "lle/SESoundManager.h"
#include "lle/U2DRectangle.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"

#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEUnitKind.h"
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
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEGhostUnit.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEGameVariant.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECombatFormulas.h"
#include "GEWindow.h"
#include "ee/EEUIManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitSetManager.h"

// C:\EEX\Empire Earth\Units\EEUCBuilding.cpp (__FILE__ 0x4c8f28: the volcano assert, line 261). The buildings' unit
// class: 0x20e58d..0x21005e (after EEUCBomb.cpp; EEUCBuildingPriest.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (EEUnitClass.cpp's factory). Order: Virtual04, VirtualE0, EruptVolcano (the volcano erupts),
// CreateUnitAt, CreateUnit, the wall/tower helpers (0x20ea01..0x20ed72: EEGetFacingQuarter = the map quarter a tile faces),
// Virtual44, Virtual74, Virtual78, Virtual84, ReleaseUnit, VirtualA8, Virtual98, VirtualA0, VirtualB4, VirtualBC,
// VirtualB8, UpdateArmed, then EECalamityManager::GetCalamityClass<EECCVolcano>.

// 0x446ff0/0x446ff4: the unit's named constants (one .rdata copy per unit)
static const float kBuildingLift = 0.001f;      // 0x446ff0 (a building sits just above the ground)

static const float kBuildingSunk = -2.0f;       // 0x446ff4 (VirtualA0 6000: below it the building is gone)

// 0x446dc0: the yaw of each map quarter (EEGetFacingQuarter's 0..3) (provisional name)
static const float kQuarterYaws[4] = { 0.0f, 3.1415927f, -1.5707964f, 1.5707964f };

// FUNCTION: EXE 0x60e58d
// Puts docks and navy yards on the water plane and lifts other buildings just above the ground
void EEUnitClass::BuildingGroundLocation(GE3DPoint& ioLocation)
{
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard)
        ioLocation.SetZ(0.0f);
    else
        ioLocation.SetZ(ioLocation.GetZ() + kBuildingLift);
}

// FUNCTION: EXE 0x60e5b8
// Picks the first fully healed garrisoned unit that should leave (a human's unit whose rally queue has a goal, a unit
// with a goal, or one with an explore order) as the exiting unit; returns whether one was picked
bool EEUnitClass::BuildingSelectUnitToRelease(EEComplexUnit* inUnit)
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
                EEUGRust* theExplore = EEExploreGoalCast(theGoal);
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
bool EEUnitClass::EruptVolcano(EEComplexUnit* inUnit)
{
    EELocationCalamityClass* theVolcanoClass = NULL;
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    if (!USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(9, theVolcanoClass))
        ESVerify(__FILE__, 261, "EECalamityManager::Instance()->FindCalamityClass(keectVolcano, theVolcanoClass)"); // the original assert text (provisional names here)
    inUnit->GetPosition(thePosition);
    theVolcanoClass->GetRandomLocation(thePosition, theTarget);
    inUnit->SetGoal(EEUGRust::NewAttackGround(theTarget, false), 0, 0);
    return true;
}

// FUNCTION: EXE 0x60e8ae
// Creates a building at a placement: docks and navy yards on the water plane, others (not the camera object) lifted
// just above it
EEUnit* EEUnitClass::BuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                   EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard) {
        theTransformation.SetX(theTransformation.GetX());
        theTransformation.SetZ(0.0f);
    } else if (this->mUnitType != kUnitTypeCameraObject)
        theTransformation.SetZ(theTransformation.GetZ() + kBuildingLift);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x60e909
// Creates a building on a tile's centre (docks and navy yards at water level, off-map tiles at the -10000 sentinel)
EEUnit* EEUnitClass::BuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
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
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x60ea89
// Returns the map quarter facing the most space terrain within 6 tiles of a tile (half weight to the neighbouring
// sides; its terrain run 0x60ea01 and the side tables 0x4c8ef8/0x4c8f08: ee_units cls_building.rs, R5b.4 round 33)
long EEGetDockFacing(unsigned long inX, unsigned long inY)
{
    EEUCNote theNote(23); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_dock_facing((uint32_t)inX, (uint32_t)inY));
}

// FUNCTION: EXE 0x60eb1c
// Returns a navy yard's facing at a tile: its map quarter if 5 deep-water tiles follow that way, else the next side
// that has them, else the side with the longest run (cls_building.rs)
static long GetNavyYardFacing(unsigned long inX, unsigned long inY)
{
    EEUCNote theNote(22); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_navy_yard_facing((uint32_t)inX, (uint32_t)inY));
}

// FUNCTION: EXE 0x60ebda
// Returns the map quarter (0..3) a tile faces from the middle of its continent (space maps) or of the map
// (cls_building.rs)
long EEGetFacingQuarter(long inX, long inY)
{
    EEUCNote theNote(21); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_facing_quarter((int32_t)inX, (int32_t)inY));
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
EEUnit* EEUnitClass::FindNearestTargetBuilding(EEUnit* inUnit)
{
    EEUCNote theNote(35); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    EEUnit* theResult = ([&]() -> EEUnit* {
        std::vector<EETileCoordinate> theTiles;
        EEUnit* theResult = NULL;
        EEMap* theMap = ::gWorld.mMap;
        for (unsigned long theRing = 1; theResult == NULL && theRing < 0x14; theRing++) {
            EEGetSquareRingTiles(inUnit->GetTileX(), inUnit->GetTileY(), theRing, theTiles);
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
    }());
    theNote.U(theResult);
    return theResult;
}

// FUNCTION: EXE 0x60ee56
// Lists the tiles around a building; an airport's include its runway (four times its size toward its facing)
void EEUnitClass::BuildingGetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners,
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
        this->BaseGetSurroundingTiles(inUnit, outTiles, inCorners, inAppend);
}

// FUNCTION: EXE 0x60f17f
// Sets up a new building (built, its resource stock, regeneration); an airport turns to its map quarter, a navy yard
// picks its facing and a space dock its yaw
void EEUnitClass::BuildingUnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetBuilt(0, 1, 1);
    this->BaseUnitCreated(inUnit);
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
        inUnit->SetFacing(GetNavyYardFacing(inUnit->GetTileX(), inUnit->GetTileY()));
    else if (this->mUnitType == kUnitTypeSpaceDock) {
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        theTransformation.SetOrientationYPR(
            EEGetSpaceDockYaw(inUnit->GetTileX() + 1, inUnit->GetTileY() + 1, inUnit->mUnitClass->GetTileSize()), 0.0f, 0.0f);
        inUnit->mPhysicalModel.SetLocation(theTransformation);
    }
}

// FUNCTION: EXE 0x60f2bb
// Launches the exiting unit: places it at the exit (runway, water or tag point), gives it the building's rally
// goals or its own (else loads it back) and starts its takeoff from this home base
void EEUnitClass::ReleaseUnit(EEComplexUnit* inUnit)
{
    GEOrientation theOrientation;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    EEGoalDeque::const_iterator theIterator;
    long theQueue = 0;
    unsigned long theCargoID = inUnit->mExitingUnitID;
    if (UIsNone32(theCargoID))
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
            EEUGRust* theAttack = EEAttackUnitCast(*theIterator);
            EEUnitGoal* theCopy;
            if (theAttack != NULL) {
                if (theAttack->Suits(theCargo) && theAttack->GetAttackTarget() != NULL && theAttack->GetAttackTarget()->mHitPoints > 0)
                    theCopy = theAttack->Clone();
                else {
                    GE2DPoint theTarget((float)theAttack->GetAttackTarget()->GetTileX(), (float)theAttack->GetAttackTarget()->GetTileY());
                    theCopy = EEUGRust::NewLocation(theTarget, false, true, false, 0.0f, false, false);
                }
            } else if ((*theIterator)->Suits(theCargo))
                theCopy = (*theIterator)->Clone();
            else {
                (*theIterator)->GetGoalLocation(thePosition);
                theCopy = EEUGRust::NewLocation(&thePosition, false, true, false, 0.0f, false, false);
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
                            theGoal = EEUGRust::NewUnloadTransport(theCargo, (long)theTarget.GetX(), (long)theTarget.GetY(), false);
                        else
                            theGoal = EEUGRust::NewLocation(&theTarget, false, true, false, 0.0f, false, false);
                    }
                } else {
                    EEUGRust* theAttack = EEAttackUnitCast(*theIterator);
                    if (theAttack != NULL) {
                        if (theAttack->Suits(theCargo) && theAttack->GetAttackTarget() != NULL && theAttack->GetAttackTarget()->mHitPoints > 0)
                            theGoal = theAttack->Clone();
                        else {
                            GE2DPoint theTarget((float)theAttack->GetAttackTarget()->GetTileX(), (float)theAttack->GetAttackTarget()->GetTileY());
                            theGoal = EEUGRust::NewLocation(theTarget, false, true, false, 0.0f, false, false);
                        }
                    } else if ((*theIterator)->Suits(theCargo))
                        theGoal = (*theIterator)->Clone();
                    else {
                        (*theIterator)->GetGoalLocation(thePosition);
                        theGoal = EEUGRust::NewLocation(&thePosition, false, true, false, 0.0f, false, false);
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
                theCopy = EEUGRust::NewUnloadTransport(theCargo, (long)theTarget.GetX(), (long)theTarget.GetY(), false);
            else
                theCopy = EEUGRust::NewLocation(&theTarget, false, true, false, 0.0f, false, false);
            theCargo->QueueGoal(theCopy, 0);
        }
    }
    theCargo->SetHomeBase(inUnit);
    theCargo->StartAction(kActionTakeoff);
    theCargo->mUnitClass->ActionChanged(theCargo);
    inUnit->SetExitingUnitID(-1);
}

// FUNCTION: EXE 0x60fb85
// Answers the building triggers (fall finished, enemy spotted when built, plane to launch, volcano target, secondary
// target, capitol, launch requests, takeoff clearance); others go to the common unit class
long EEUnitClass::BuildingEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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
            theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerAirplaneAvailableToLaunch:
        theFlag = this->SelectUnitToRelease(inUnit);
        break;
    case kTriggerChooseRandomVolcanoTarget:
        theFlag = this->EruptVolcano(inUnit);
        break;
    case kTriggerSecondaryTargetFound: {
        EEUGRust* theGoal = EEAttackUnitCast(inUnit->GetGoal());
        theFlag = theGoal != NULL && theGoal->GetAttackTarget()->mHitPoints > 0;
        break;
    }
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
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

// FUNCTION: EXE 0x60fef9
// Runs an armed building's tick (physics, turrets, timers, animation, sound) and fires on its attack frame; skips
// disabled ones and standing foundations
void EEUnitClass::UpdateArmed(EEComplexUnit* inUnit)
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
        inUnit->UpdateRottingAwayTime();
        break;
    }
    }
}

// ---- EEUCBuildingPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). A building that
// converts (the temple): 0x21005e..0x21013b (after EEUCBuilding.cpp; EEUCFortress.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory); the slots pick the building's or the priest's handling
// by command/event. RTTI gives EEUCBuildingPriest no EEUCBasicPriest base, so the priest half is the class's own copy
// of EEUCBasicPriest's bodies: identical source compiled to identical code, which LINK's ICF folded into
// EEUCBasicPriest's (0x20d59f, 0x20d7f2, 0x20d998, 0x20dd0a, 0x20dd2d, 0x20dee4, 0x20df05, 0x20e16a, 0x20e274). With
// one class the temple runs the basic priest's bodies themselves (R5b.4 round 32); only the update's copy is left
// (PriestVirtualB8: the basic priest's update is a Rust sequence the temple does not run).

// FUNCTION: EXE 0x6100c1
// Answers a trigger as a building, except the conversion triggers, which the priest half answers
long EEUnitClass::BuildingPriestEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = this->BuildingEvaluateCondition(inUnit, inEvent);
    switch (inEvent) {
    case kTriggerEnemyUnitConverted:
    case kTriggerGoalIsConversion:
    case kTriggerConvertGoalStillValid:
    case kTriggerHasEnoughManaForConversion:
    case kTriggerAttackerIsLegalConversionTarget:
        theResult = this->BasicPriestEvaluateCondition(inUnit, inEvent);
        break;
    }
    return theResult;
}

// The temple's copy of EEUCBasicPriest::Update (0x20df05)
void EEUnitClass::PriestVirtualB8(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    inUnit->SetLastHitPoints(inUnit->mHitPoints);
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000) {
        this->mPhysicsModel->Step(inUnit);
        inUnit->UpdateTurrets();
        inUnit->UpdateOrbitals();
    }
    this->UpdateEffects(inUnit);
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    this->UpdateCamouflageTimer(inUnit);
    long theAttack = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    if (this->mIsMedic) {
        inUnit->UpdateHealSearchTimer();
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalHealUnit)
            inUnit->mGoalState->Work(inUnit);
    }
    if (inUnit->mStats->mRegeneration > 0)
        this->Regenerate(inUnit);
    this->UpdateDone(inUnit);
    this->RegeneratePower(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theAttack, theFrame);
        break;
    case kActionConvertEnemyUnit:
        this->BasicPriestUpdateConversion(inUnit);
        return;
    case kActionResourcesRotting:
        return;
    case kActionUnitRotting: {
        inUnit->UpdateRottingAwayTime();
        break;
    }
    }
    if (inUnit->mConversionStartTime != 0)
        inUnit->mConversionStartTime = 0;
    inUnit->ReleaseConversionEffect();
    this->BasicPriestHideConversionEffects(inUnit);
}

// ---- EEUCGate.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The wall gate's unit class:
// 0x21048f..0x212a6f (after EEUCFortress.cpp; EEUCHelicopter.cpp follows). One unit per EEUC* class in alphabetical
// order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). The unit opens with the
// `$E` chains of its two file-static colours, then Virtual20, EEBuildGate (EEMBuildGate's work), the slots and the
// gate helpers in address order; EEMap's corner-height inlines and the sort/unique COMDATs over the tile vector
// close it. A gate is five linked pieces (+0x1cc/+0x1d0): piece 2 is the gate, 0/4 the posts, 1/3 the doors.

// The highlight of a wall that can take a gate, and the normal colour (`$E` chains 0x21048f / 0x2104bf; within one
// unit later globals get lower addresses: 0x5634f0 / 0x5634e0) (provisional names)
static GEColor sGateHighlight(0.0f, 1.0f, 0.0f);

// SYNTHETIC: EXE 0x61048f   (wrapper)
// SYNTHETIC: EXE 0x610499   (ctor)
// SYNTHETIC: EXE 0x6104b2   (registrar)
// SYNTHETIC: EXE 0x6104be   (dtor)
static GEColor sGateNormal(1.0f, 1.0f, 1.0f);

// SYNTHETIC: EXE 0x6104bf   (wrapper)
// SYNTHETIC: EXE 0x6104c9   (ctor)
// SYNTHETIC: EXE 0x6104e2   (registrar)
// SYNTHETIC: EXE 0x6104ee   (dtor)

// 0x447004..0x447018: the unit's named float constants (one .rdata block per unit) (provisional names)
static const float kGateOpen = 1.0f;            // 0x447004 (+0x228: fully open)

static const float kGateClosed = 0.0f;          // 0x447008

static const float kGateStep = 0.2f;            // 0x44700c (per update)

long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex);

void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles);

bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits);

bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// 0x24c627 (inTiles' units of inUnitType): ee/EEUnitUtilities.h
// 0x24c433: the units on inTiles (EECPWallManager.cpp declares it the same way)
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);

// 0x24f8f9: after a gate is built for the current player (the new gate's piece 2) (provisional name)
void EECreateSpaceGateEnergyWall(EEComplexUnit* inGate);

// FUNCTION: EXE 0x6104ef
// Replaces the other four pieces of a gate whose gate piece was replaced (new owner and class), turning and
// relinking them; the posts go idle and the doors take the new gate piece's action
void EEUnitClass::GateGateReplaced(EEUnit* inGate, EEUnit* inUnit)
{
    std::vector<EEUnit*> thePieces;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    unsigned long theDirection = inUnit->GetComplexUnit()->mGateDirection;
    thePieces.resize(5);
    thePieces[2] = inUnit;
    for (long i = 0; i <= 4; i++) {
        if (i == 2)
            continue;
        EEComplexUnit* theOld = this->GetPart(inGate->GetComplexUnit(), i);
        EEComplexUnit* thePiece = EEReplaceUnit(theOld, inUnit->GetPlayer()->GetPlayerID(), theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, theDirection, i);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(theDirection);
        thePieces[i] = thePiece;
    }
    thePieces[4]->GetComplexUnit()->LinkPart(thePieces[0]->GetComplexUnit());
    thePieces[0]->GetComplexUnit()->LinkPart(thePieces[3]->GetComplexUnit());
    thePieces[3]->GetComplexUnit()->LinkPart(thePieces[1]->GetComplexUnit());
    thePieces[1]->GetComplexUnit()->LinkPart(thePieces[2]->GetComplexUnit());
    thePieces[4]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[0]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[3]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());
    thePieces[1]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());

}

// FUNCTION: EXE 0x61080d
// Turns the five wall pieces across a tile into a closed gate of the technology's class for a player (EEMBuildGate's
// work): weakest wall's health, posts on the lower ground; adds the energy wall once space gates are available
void EEBuildGate(EETileCoordinate& inTile, long inPlayer, long inTechnology, long inDirection)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theWalls;
    std::vector<EEUnit*> theGate;
    GE3DPoint thePost4;
    U2DPoint<long> theTile;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    GE3DPoint thePost0;
    float theHealth = 1.0f;
    EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    theTile.mX = inTile.mX;
    theTile.mY = inTile.mY;
    EEGetWallGateTiles(theTile, inDirection, theTiles);
    EEGetUnitsOn(theTiles, theUnits);
    for (unsigned long i = 0; i < theUnits.size(); i++) {
        if (theUnits[i]->GetUnitClass()->mIsWall)
            theWalls.push_back(theUnits[i]);
    }
    std::vector<EEUnit*> theSide;
    if (theWalls.size() == 0 || !EEDoesWallGateFit(inTile, inDirection, theWalls[0]->GetUnitClass()->GetUnitType(), theSide))
        return;
    long j;
    std::vector<EEUnit*>::iterator it;
    for (j = 0, it = theWalls.begin(); j <= 4; j++, it++) {
        if (it == theWalls.end())
            break;
        float theRatio = (float)(*it)->mHitPoints / (*it)->mStats->mMaxHitPoints;
        if (theHealth >= theRatio)
            theHealth = theRatio;
        long thePlayerID = thePlayer->GetPlayerID();
        EEComplexUnit* thePiece = EEReplaceUnit((EEComplexUnit*)*it, thePlayerID, theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, inDirection, j);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(inDirection);
        theGate.push_back(thePiece);
    }
    theGate[4]->mComplexUnit->GetPosition(thePost4);
    theGate[0]->GetComplexUnit()->GetPosition(thePost0);
    float theZ = min(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost4.GetX(), thePost4.GetY()),
                     ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost0.GetX(), thePost0.GetY()));
    thePost4.SetZ(theZ);
    thePost0.SetZ(theZ);
    theGate[4]->GetComplexUnit()->SetLocation(thePost4, true);
    theGate[0]->GetComplexUnit()->SetLocation(thePost0, true);
    theGate[4]->GetComplexUnit()->LinkPart(theGate[0]->GetComplexUnit());
    theGate[0]->GetComplexUnit()->LinkPart(theGate[3]->GetComplexUnit());
    theGate[3]->GetComplexUnit()->LinkPart(theGate[1]->GetComplexUnit());
    theGate[1]->GetComplexUnit()->LinkPart(theGate[2]->GetComplexUnit());
    theGate[4]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[0]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[3]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[1]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[2]->mComplexUnit->StartAction(kActionClosed);
    theGate[2]->SetHitPoints(UMFloatToLong(theGate[2]->GetStats()->mMaxHitPoints * theHealth));
    if (gUIManager->mInScenarioEditor && !gGameScreenActive) {
        ::gWorld.FlushQueuedPlacements();
        thePlayer->AddPendingUnits(0, true);
        thePlayer->RemoveKilledUnits();
    }
    if (thePlayer->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && theClass->GetUnitType() != kUnitTypeGatePalisades)
        EECreateSpaceGateEnergyWall(EEUnitCast<EEComplexUnit>(theGate[2]));
}

// The sort-and-unique of a vector (cdecl template 0x211ebc and its range half 0x211f07) (provisional names)
template <class V, class L, class E> typename V::iterator EESortUniqueRange(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    typename V::iterator theResult;
    theResult = std::unique(ioValues.begin(), ioValues.end(), inEqual);
    return theResult;
}

template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    typename V::iterator theEnd = EESortUniqueRange(ioValues, inLess, inEqual);
    ioValues.erase(theEnd, ioValues.end());
}

// FUNCTION: EXE 0x610e86
// Collects the footprint tiles of every gate piece not left out (doors and gate piece while open), sorted and unique
void EEUnitClass::GateGetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    if (!inAppend)
        outTiles.clear();
    EEComplexUnit* thePiece = inUnit->GetComplexUnit();
    while (thePiece->mPreviousPart != NULL)
        thePiece = thePiece->mPreviousPart;
    for (; thePiece != NULL; thePiece = thePiece->mNextPart) {
        if (!this->IsPartLeftOut(thePiece))
            this->BaseGetSurroundingTiles(thePiece, outTiles, inCorners, true);
    }
    // the COMDATs sort U2DPoint<long>s (operator< 0x13710, vector<U2DPoint<long> >::erase 0x288efb); EETileCoordinate
    // is U2DPoint<long>, so the tiles need no vector cast
    EESortUnique(outTiles, std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >());
}

// FUNCTION: EXE 0x610eee
// Returns whether a gate fits across a wall unit's tile in a direction (1..4), giving the five walls of the type
// (EECPWallManager.cpp declares it the same way)
bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return EEDoesWallGateFit(theTile, inSide, inUnitType, outUnits);
}

// FUNCTION: EXE 0x610f19
// Returns whether a gate fits across a tile in a direction: five built, living walls of the type on level tiles
// (else outUnits is emptied)
bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits)
{
    EEUCNote theNote(25); theNote.T(inTile).I(inDirection).I(inUnitType);   // TEST ONLY (UCBD)
    bool theResult = theNote.R([&]() -> bool {
        std::vector<U2DPoint<long> > theTiles;
        std::vector<EEUnit*> theUnused;
        bool theResult = true;
        U2DPoint<long> theTile(inTile.mX, inTile.mY);
        EEGetWallGateTiles(theTile, inDirection, theTiles);
        EEGetWallUnitsOfTypeOnTiles(theTiles, inUnitType, outUnits);
        std::vector<EEUnit*>::iterator it = outUnits.begin();
        while (true) {
            if (it == outUnits.end())
                break;
            if ((*it)->mComplexUnit->mIsBuilt && (*it)->mHitPoints > 0)
                theResult = true;
            else
                theResult = false;
            it++;
            if (!theResult)
                break;
        }
        if (theResult && outUnits.size() == 5)
            theResult = EEAreTilesLevel(theTiles);
        else {
            outUnits.clear();
            theResult = false;
        }
        return theResult;
    }());
    theNote.Us(outUnits);
    return theResult;
}

// FUNCTION: EXE 0x610fe2
// Returns the first piece of a unit's linked chain (+0x1cc)
EEComplexUnit* EEGetFirstPart(EEComplexUnit* inUnit)
{
    while (inUnit->mPreviousPart != NULL)
        inUnit = inUnit->mPreviousPart;
    return inUnit;
}

// FUNCTION: EXE 0x610ff5
// Returns the middle unit of a list (the lower of the two middles for an even count)
static EEUnit* EEGetMiddleOf(std::vector<EEUnit*>& inUnits)
{
    unsigned long theIndex = inUnits.size() / 2;
    if (inUnits.size() % 2 == 0)
        theIndex--;
    return inUnits[theIndex];
}

// FUNCTION: EXE 0x611028
// Turns gate piece inIndex for a gate in direction 1..4; returns its animation: 9 a post, 10 a diagonal post, 11 else
// (the yaws: cls_building.rs)
long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex)
{
    EEUCNote theNote(24); theNote.I(inDirection).I(inIndex);   // TEST ONLY (UCBD)
    float theYaw;
    bool theTurned;
    long theResult = ee_units_cls_gate_part_turn((int32_t)inDirection, (int32_t)inIndex, &theYaw, &theTurned);
    if (theTurned)
        ioTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theNote.R(theResult);   // TEST ONLY (UCBD)
    theNote.F(ioTransformation.GetYaw());
    return theResult;
}

// FUNCTION: EXE 0x61112a
// Returns piece inIndex (0..4) of a gate, found along its linked chain (2 is the gate piece itself)
EEComplexUnit* EEUnitClass::GetPart(EEComplexUnit* inGate, long inIndex)
{
    EEUCNote theNote(29); theNote.C(this).U(inGate).I(inIndex);   // TEST ONLY (UCBD)
    EEComplexUnit* theResult = ([&]() -> EEComplexUnit* {
        EEComplexUnit* theResult = NULL;
        switch (inIndex) {
        case 4:
            theResult = this->GetLastLinked(inGate);
            break;
        case 3:
            theResult = this->GetFourthPart(inGate);
            break;
        case 2:
            theResult = inGate;
            break;
        case 1:
            theResult = this->GetSecondPart(inGate);
            break;
        case 0:
            theResult = this->GetLastLinkedCarrier(inGate);
            break;
        }
        return theResult;
    }());
    theNote.U(theResult);
    return theResult;
}

// FUNCTION: EXE 0x611171
// Lists the five tiles of a gate line through a tile: direction 1 along x, 2 along y, 3 and 4 the diagonals
// (cls_building.rs)
void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles)
{
    EEUCNote theNote(33); theNote.T(inTile).I(inDirection).Ts(outTiles);   // TEST ONLY (UCBD)
    ee_units_cls_wall_gate_tiles((int32_t)inTile.mX, (int32_t)inTile.mY, (int32_t)inDirection, &outTiles);
    theNote.Ts(outTiles);   // TEST ONLY (UCBD)
}

// FUNCTION: EXE 0x6113a9
// Returns whether a living enemy unit of unit set 0x62 is within reach of the gate or its next two pieces
// (the squared distance is compared with the unsquared reach; cls_building.rs)
bool EEUnitClass::AreEnemiesNear(EEComplexUnit* inUnit)
{
    EEUCNote theNote(27); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_gate_near(EEObj(inUnit), false));
}

// FUNCTION: EXE 0x611506
// Returns whether a unit that may pass is within reach of the gate or its next two pieces: a mobile land unit
// the owner is not hostile to, or an enemy of unit set 0x62 (cls_building.rs)
bool EEUnitClass::IsPasserNear(EEComplexUnit* inUnit)
{
    EEUCNote theNote(28); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_gate_near(EEObj(inUnit), true));
}

// FUNCTION: EXE 0x6116a2
// Returns whether a tile holds a highlighted living wall that can take a gate, giving its gate direction
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection)
{
    bool theFound = false;
    EEMapTile* theTile = ::gWorld.GetMap()->GetTile(inTile.mX, inTile.mY);
    if (theTile != NULL) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->mUnitClass->mIsWall && theUnit->mHitPoints > 0 && theUnit->mTileNode != NULL &&
                theUnit->GetComplexUnit()->mIsTinted) {
                theFound = true;
                ioDirection = theUnit->GetComplexUnit()->mGateDirection;
            }
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x611723
// Returns the piece before the last one of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinkedCarrier(EEComplexUnit* inUnit)
{
    return this->GetLastLinked(inUnit)->mPreviousPart;
}

// FUNCTION: EXE 0x611735
// Returns gate piece 1 (the piece before piece 3); NULL when the chain is too short (a gate created as a single
// piece, e.g. by a scenario effect)
EEComplexUnit* EEUnitClass::GetSecondPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theFourth = this->GetFourthPart(inGate);
    return theFourth != NULL ? theFourth->mPreviousPart : NULL;
}

// FUNCTION: EXE 0x611747
// Returns the last piece of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinked(EEComplexUnit* inUnit)
{
    while (inUnit->mNextPart != NULL)
        inUnit = inUnit->mNextPart;
    return inUnit;
}

// FUNCTION: EXE 0x61175c
// Returns gate piece 3 (two pieces before the end of the chain); NULL when the chain is too short (a gate created
// as a single piece, e.g. by a scenario effect)
EEComplexUnit* EEUnitClass::GetFourthPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theCarrier = this->GetLastLinkedCarrier(inGate);
    return theCarrier != NULL ? theCarrier->mPreviousPart : NULL;
}

// FUNCTION: EXE 0x61176e
// Restores the normal colour of a player's walls of a type and clears their gate direction (after gate placement)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(inUnitType, inPlayer->mPlayerID, theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        EEComplexUnit* theUnit = (*it)->mComplexUnit;
        if (theUnit == NULL)
            continue;
        theUnit->SetColor(sGateNormal);
        theUnit->SetIsTinted(false);
        theUnit->SetGateDirection(0);
    }
}

// FUNCTION: EXE 0x611905
// Sets up a new gate piece the building's way; the chain's first piece starts closed
void EEUnitClass::GateUnitCreated(EEComplexUnit* inUnit)
{
    this->BuildingUnitCreated(inUnit);
    EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(inUnit->GetUnitClass());
    if (inUnit->mPreviousPart == NULL && inUnit->mNextPart != NULL) {
        inUnit->StartAction(kActionClosed);
        this->ActionChanged(inUnit);
    }
}

// FUNCTION: EXE 0x611957
// Highlights the middle wall of a gate line through each of a player's walls of a type when one or three of the
// four directions fit (an XOR of the four tests); the first fitting direction is used
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theSide1;
    std::vector<EEUnit*> theSide2;
    std::vector<EEUnit*> theSide3;
    std::vector<EEUnit*> theSide4;
    EEClearGateHighlights(inPlayer, inUnitType);
    EEGetPlayerUnitsOfType(inUnitType, inPlayer->mPlayerID, theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        bool the1 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 1, inUnitType, theSide1);
        bool the2 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 2, inUnitType, theSide2);
        bool the3 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 3, inUnitType, theSide3);
        bool the4 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 4, inUnitType, theSide4);
        if (the1 ^ the2 ^ the3 ^ the4) {
            if (the1) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide1)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(1);
                }
            } else if (the2) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide2)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(2);
                }
            } else if (the3) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide3)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(3);
                }
            } else if (the4) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide4)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(4);
                }
            }
        }
        theSide1.clear();
        theSide2.clear();
        theSide3.clear();
        theSide4.clear();
    }
}

// FUNCTION: EXE 0x611c35
// Answers the gate's triggers: open, closed, locked, special (enemy) unit or any passer in opening range;
// others the building's way
long EEUnitClass::GateEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerIsOpen:
        theFlag = inUnit->mOpenFraction == kGateOpen;
        break;
    case kTriggerIsClosed:
        theFlag = inUnit->mOpenFraction == kGateClosed;
        break;
    case kTriggerLocked:
        theFlag = inUnit->mIsLocked;
        break;
    case kTriggerNotLocked:
        theFlag = !inUnit->mIsLocked;
        break;
    case kTriggerSpecialUnitInOpeningRange:
        theFlag = this->AreEnemiesNear(inUnit);
        break;
    case kTriggerSpecialUnitNotInOpeningRange:
        theFlag = !this->AreEnemiesNear(inUnit);
        break;
    case kTriggerUnitInOpeningRange:
        theFlag = this->IsPasserNear(inUnit);
        break;
    case kTriggerUnitNotInOpeningRange:
        theFlag = !this->IsPasserNear(inUnit);
        break;
    default:
        theResult = this->BuildingEvaluateCondition(inUnit, inEvent);
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x611d00
// Returns whether all tiles are on the map, flat or ramps (0 or 2 raised corners), and differ in height by at most
// 0.6 (0x447014; cls_building.rs) (EECPWallManager.cpp declares it the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles)
{
    EEUCNote theNote(26); theNote.Ts(inTiles);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_tiles_level(&inTiles));
}

// ---- EEUCMiningBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The mining
// building's unit class (a mine built on a resource unit): 0x217085..0x217337 (after EEUCLandTransport.cpp;
// EEUCMissile.cpp follows). Unit flags: the exe's module set. Its ctor is inline (vtable 0x44843c).

// FUNCTION: EXE 0x617085
// Creates a mine at a placement and ties it to the first resource under its footprint that it can gather
EEUnit* EEUnitClass::MiningBuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                         EEUnit* inCreator)
{
    EEMap* theMap = ::gWorld.mMap;
    long theX = UMFloor(inTransformation.GetX());
    long theY = UMFloor(inTransformation.GetY());
    long theSize = this->mTileSize;
    bool theFound = false;
    EEUnit* theUnit = this->BuildingCreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
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
EEUnit* EEUnitClass::MiningBuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
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
    return this->MiningBuildingCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x61726d
// Answers resource depleted (its resource is gone, invalid or empty); other triggers go to the building class
long EEUnitClass::MiningBuildingEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    EEUnit* theMine = inUnit->mMine;
    long theResult = 0;
    if (inEvent != kTriggerResourceDepleted)
        theResult = this->BuildingEvaluateCondition(inUnit, inEvent);
    else if (theMine == NULL || !theMine->mIsValid || theMine->mResourceAmount <= 0)
        theResult = 1;
    return theResult;
}

// ---- EEUCNavalBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval building's
// unit class (a dock): 0x218158..0x218490 (after EEUCNaval.cpp; EEUCNavalCitizen.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline. Virtual74/78/88 are ICF-folded into EEUCSpaceBuilding.cpp's copies; VirtualA0/E0
// here are also EEUCSpaceBuilding's (ICF).

// FUNCTION: EXE 0x618164
// Picks the first fully healed docked ship that should leave (a human's ship whose own or the dock's rally goal is an
// attack, move or explore goal; any ship with a goal for a computer player); returns whether one was picked
bool EEUnitClass::NavalBuildingSelectUnitToRelease(EEComplexUnit* inUnit)
{
    bool theResult = false;
    unsigned long theCount = inUnit->GetNumCargoUnits();
    for (unsigned long i = 0; !theResult && i < theCount; i++) {
        EEComplexUnit* theCargo = inUnit->mCargo[i];
        if (theCargo->mHitPoints != theCargo->mStats->mMaxHitPoints)
            continue;
        bool theReady = false;
        if (theCargo->mPlayer->IsHuman()) {
            if (theCargo->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = (theCargo->mGoalQueues.begin() + 1)->GetFront();
                if (EEAttackUnitCast(theGoal) == 0 &&
                    !EEGoalHasLocation(theGoal) &&
                    EEExploreGoalCast(theGoal) == 0)
                    theGoal = NULL;
                if (theGoal != NULL)
                    theReady = true;
            }
            if (!theReady && inUnit->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = (inUnit->mGoalQueues.begin() + 1)->GetFront();
                if (EEAttackUnitCast(theGoal) == 0 &&
                    !EEGoalHasLocation(theGoal) &&
                    EEExploreGoalCast(theGoal) == 0)
                    theGoal = NULL;
                if (theGoal != NULL)
                    theReady = true;
            }
        }
        if (theReady || (inUnit->mPlayer->mComputerPlayer != NULL && theCargo->mGoalState != NULL)) {
            inUnit->SetExitingUnitID(theCargo->mUnitID);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x618320
// Returns whether the dock's transformed bounding box lies wholly below the water line
bool EEUnitClass::NavalBuildingHasSunk(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    const GEPhysicalModel& thePhysicalModel = inUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    inUnit->GetPosition(thePosition);
    GETransformation theTransformation(false);
    thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                         theTransformation);
    float theScale = thePhysicalModel.GetScale();
    theTransformation.Scale(theScale, theScale, theScale);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    theBox = theModel->GetBoundingBox(inUnit->mPhysicalModel.GetFrame());
    theBox.Transform(theTransformation, theTransformedBox);
    return theTransformedBox.GetTopRightBack().GetZ() < 0.0f ? true : false;
}

// FUNCTION: EXE 0x6183f8
// Answers the dock triggers: unit has a rally goal, ship ready to launch, sunk after dying; others to the building
long EEUnitClass::NavalBuildingEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theHandled;
    switch (inEvent) {
    case kTriggerFighterHasGoal:
        theHandled = inUnit->GetQueuedGoal(1) != NULL;
        break;
    case kTriggerAirplaneAvailableToLaunch:
        theHandled = this->SelectUnitToRelease(inUnit);
        break;
    case kTriggerDyingAnimationFinished:
        theHandled = this->HasSunk(inUnit);
        break;
    default:
        theResult = this->BuildingEvaluateCondition(inUnit, inEvent);
        theHandled = false;
        break;
    }
    if (theHandled)
        theResult = 1;
    return theResult;
}

// ---- EEUCSpaceBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The space
// building's unit class: 0x219fe3..0x21a219 (after EEUCSeaAnimal.cpp; EEUCSpaceship.cpp follows). Unit flags: the
// exe's module set. Its ctor is inline. Virtual74/78/88 are also EEUCNavalBuilding's (ICF: these copies are kept);
// VirtualA0/E0 are EEUCNavalBuilding.cpp's (ICF).

// FUNCTION: EXE 0x619ff3
// Returns the tile under a location along the camera line, at the cruise altitude
void EEUnitClass::SpaceBuildingLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    float theZ = this->mCruiseAltitude;
    theLine.SolveForZ(theZ, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x61a0a0
// Creates a space building at a placement, at the cruise altitude
EEUnit* EEUnitClass::SpaceBuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                        EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(this->mCruiseAltitude);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x61a0dd
// Creates a space building on a tile, at the cruise altitude
EEUnit* EEUnitClass::SpaceBuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(this->mCruiseAltitude);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x61a129
// Sets up a new space building as a building, already built
void EEUnitClass::SpaceBuildingUnitCreated(EEComplexUnit* inUnit)
{
    this->BuildingUnitCreated(inUnit);
    inUnit->SetBuilt(1, 0, 1);
}

// FUNCTION: EXE 0x61a144
// Returns whether the space building may stop: already sinking, a slow request, or at its next waypoint's tile
bool EEUnitClass::SpaceBuildingHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theLocation;
    bool theResult = false;
    if (inUnit->mCollisionState == 8 || inValue <= 0.05f)
        theResult = true;
    else {
        inUnit->GetNextWaypointPoint(theLocation);
        if (inUnit->GetTileX() == UMFloor(theLocation.GetX()) && inUnit->GetTileY() == UMFloor(theLocation.GetY()))
            theResult = true;
    }
    return theResult;
}

