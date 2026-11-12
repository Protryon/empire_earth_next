// C:\EEX\Empire Earth\Units\EEUCAirplane.cpp (file name inferred: no __FILE__; the class name). The airplane's unit
// class: 0x20b192..0x20b9fc (after EEUCAirAnimal.cpp; EEUCAmbient.cpp follows). Unit flags: the exe's module set. Its
// ctor is inline (EEUnitClass.cpp's factory). Order: Virtual74 with EEAirTrafficControl::GetNumSlots's COMDAT after
// it, EEIsTileFree, EECreateCarrierAircraft (the carrier's aircraft), FlyOffMap (off the map edge), VirtualBC, VirtualB8.
// Virtual74 is EEUCPlanetSpaceship's twin (0x218e34).
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
#include "lle/U2DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUCAirplane.h"

// 0x446fcc: "no height" (one .rdata copy per unit: a named const; provisional name)
static const float kAirplaneNoHeight = -10000.0f;

// FUNCTION: EXE 0x60b192
// Sets up the plane's flight for its new action: circling (on space maps toward the side it can reach), a holding
// pattern over its home airport (killed when there is none), circling the air control, or leaving the map
void EEUCAirplane::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEAirTrafficControl* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionCircle:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        if ((long)inUnit->mWaypoints.size() > 0)
            inUnit->DropPath();
        if (::gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
            float theDistance = inUnit->mStats->GetMaxVelocity() / inUnit->mStats->GetMaxAngularVelocity();
            theDistance = (float)UMCeil(theDistance);
            GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
            float theAngle = UMArcTan2(theTransformation.GetElement(1, 0), theTransformation.GetElement(0, 0));
            float theLeft = theAngle - 1.5707964f;
            float theRight = theAngle + 1.5707964f;
            bool theLeftNear = gContinentManager.CanGetTo(inUnit,
                (long)(theTransformation.GetX() - UMSin(theLeft) * theDistance),
                (long)(UMCos(theLeft) * theDistance + theTransformation.GetY()));
            bool theRightNear = gContinentManager.CanGetTo(inUnit,
                (long)(theTransformation.GetX() - UMSin(theRight) * theDistance),
                (long)(UMCos(theRight) * theDistance + theTransformation.GetY()));
            if (theLeftNear && !theRightNear)
                inUnit->SetCurrentPhysicsOrder(0);
            else if (!theLeftNear && theRightNear)
                inUnit->SetCurrentPhysicsOrder(1);
            else {
                theLeftNear = gContinentManager.CanGetTo(inUnit,
                    (long)(theTransformation.GetX() - UMSin(theLeft) * theDistance * 2.0f),
                    (long)(UMCos(theLeft) * theDistance * 2.0f + theTransformation.GetY()));
                theRightNear = gContinentManager.CanGetTo(inUnit,
                    (long)(theTransformation.GetX() - UMSin(theRight) * theDistance * 2.0f),
                    (long)(UMCos(theRight) * theDistance * 2.0f + theTransformation.GetY()));
                if (theLeftNear && !theRightNear)
                    inUnit->SetCurrentPhysicsOrder(0);
                else if (!theLeftNear && theRightNear)
                    inUnit->SetCurrentPhysicsOrder(1);
                else if (gPhysics->mRandom.Generate() < 0.5f)
                    inUnit->SetCurrentPhysicsOrder(0);
                else
                    inUnit->SetCurrentPhysicsOrder(1);
            }
        } else if (gPhysics->mRandom.Generate() < 0.5f)
            inUnit->SetCurrentPhysicsOrder(0);
        else
            inUnit->SetCurrentPhysicsOrder(1);
        inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        inUnit->mInitialContactHeading = kAirplaneNoHeight;
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind != kGoalExplore)
            inUnit->ClearGoals(0);
        break;
    case kActionHoldingPattern:
        inUnit->SetCurrentPhysicsAction(0);
        if (inUnit->mHomeBase == NULL) {
            if (theControl->GetNumSlots() > 0)
                inUnit->SetHomeBase(theControl->GetSlot(0).mAirport);
            else
                inUnit->Kill(true, false, false);
            if (inUnit->mHomeBase == NULL)
                break;
        }
        inUnit->DropPath();
        theTarget.SetX(inUnit->mHomeBase->GetTileX() + 1.5f);
        theTarget.SetY(inUnit->mHomeBase->GetTileY() + 1.5f);
        theTarget.SetZ(this->GetCruiseAltitude());
        this->PathToward(inUnit, thePosition, theTarget, true);
        if (gPhysics->mRandom.Generate() < 0.5f)
            inUnit->SetCurrentPhysicsOrder(0);
        else
            inUnit->SetCurrentPhysicsOrder(1);
        inUnit->SetIsCommandable(false);
        if (inUnit->GetNextWaypointPoint(thePosition) == 0)
            inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        break;
    case kActionCircleAirtrafficController:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetCurrentPhysicsOrder(0);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        break;
    case kActionSendParatrooperPlaneOffMap:
        this->FlyOffMap(inUnit);
        break;
    default:
        EEUCLandable::ActionChanged(inUnit);
        break;
    }
}

// TEMPLATE: EXE 0x60b671
// EEAirTrafficControl::GetNumSlots (inline in ee/EEAirTrafficControl.h)

// FUNCTION: EXE 0x60b687
// Returns whether a tile is on the map, not cliff, deep water or space, and not on the map edge
bool EEIsTileFree(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    // The terrain and edge checks keep the original short-circuit order.
    return theTile != NULL && theTile->mTerrainType != kTerrainCliff && theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainSpace &&
           !::gWorld.mMap->IsEdgeTile(inX, inY);
}

// FUNCTION: EXE 0x60b6ff
// Fills a new carrier with aircraft of the best of three fighter techs its player has (else unit class 0x1f)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer)
{
    GETransformation theTransformation;
    long thePlayerID = inPlayer;
    if (thePlayerID == -1)
        thePlayerID = inUnit->mPlayer->mPlayerID;
    EEPlayer* thePlayer = ::gWorld.mPlayers[thePlayerID];
    long theType;
    if (thePlayer->mTechnologies->Get(kTechInf15Watchman)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf15Watchman)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf15Watchman)->mData->mUnitClass;
    else if (thePlayer->mTechnologies->Get(kTechInf14Guardian)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf14Guardian)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf14Guardian)->mData->mUnitClass;
    else if (thePlayer->mTechnologies->Get(kTechInf13Sentinel)->GetIsAvailable() && thePlayer->mTechnologies->Get(kTechInf13Sentinel)->mIsAllowed)
        theType = thePlayer->mTechnologies->Get(kTechInf13Sentinel)->mData->mUnitClass;
    else
        theType = 0x1f;
    EEUnitClass* theClass = gUnitClassTable[theType];
    theTransformation.SetX(-10000.0f);
    theTransformation.SetY(-10000.0f);
    for (long i = inUnit->GetNumCargoUnits(); i < inUnit->mUnitClass->mTransportCapacity; i++) {
        EEUnit* theAircraft = theClass->CreateUnitAt(theTransformation, thePlayerID, 0, inUnit);
        inUnit->LoadCargo(theAircraft->mComplexUnit, 1, 0);
    }
}

// FUNCTION: EXE 0x60b824
// Sends a leaving plane off the map: hands it to player 0, fuels it for 1000 s and flies it out 1000 tiles along
// its direction from the map origin
void EEUCAirplane::FlyOffMap(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GEVector theDirection;
    inUnit->GetPosition(thePosition);
    theDirection.SetX(thePosition.GetX());
    theDirection.SetY(thePosition.GetY());
    theDirection.Normalize();
    long theX = UMFloor(theDirection.GetX() * 1000.0f);
    long theY = UMFloor(theDirection.GetY() * 1000.0f);
    if (::gWorld.mMap->GetTile(theX, theY) != NULL) {
        theX = -10000;
        theY = -10000;
    }
    EEComplexUnit* theUnit = EEConvertUnit(inUnit, 0, 0);
    theUnit->SetFlightTime(1000000);
    EEUGLocation* theGoal = new EEUGLocation(theX + 0.5f, theY + 0.5f, 0.0f, true, true, false, 0.0f, false, false);
    theUnit->DropPath();
    this->AddWaypointAt(theUnit, theX + 0.5f, theY + 0.5f, this->GetCruiseAltitude(), true);
    theUnit->SetGoal(theGoal, 0, 0);
    theUnit->StartAction(kActionFly);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

// FUNCTION: EXE 0x60b9b4
// Updates the water ripples of a living Catalina when the effect detail allows them
void EEUCAirplane::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mUnitClass->mUnitType == kUnitTypeCatalina && inUnit->mHitPoints != 0 &&
        this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// ICF-FOLDED: EXE 0x61b361
// The stopping tile, the first cargo tile and the final unload tile are all the destination
// (no address of its own: the same body as EEUCTransport::FindUnloadTiles, which ICF kept)
void EEUCAirplane::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile,
                                   U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    *outTile = *outFirstCargoTile = *outUnloadTile = *inDestination;
}
