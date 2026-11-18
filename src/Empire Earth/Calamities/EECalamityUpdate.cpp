// New (port): the host of the calamity classes' Rust starts, stops and updates (ee/EECalamityUpdate.h; ee_tech
// calamity_start.rs, calamity_update.rs; docs/RUST_PLAN.md R5b.2). The calls every class shares (the contagion
// helper, the sounds, the units near the target and what the damage loops read of them, damage, the physics random
// draws, the target's and the caster's state, the terrain height, the effect manager's DestroyEffect) and each
// class's own world steps, keyed by its enum: what the twenty EECC* subclasses' units held (EECCEarthquake.cpp ..
// EECCVolcano.cpp 0xaebb7..0xb5b7d; calamities lane, round 2).
#include <windows.h>
#include <vector>
#include <algorithm>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"
#include "lle/GEModel.h"
#include "lle/GEViewport.h"

#include "ee/EEUnitKind.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EETileLocation.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"

extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)

namespace {

void StateFrom(EECalamity* inCalamity, EeCalamityState& outState)
{
    outState.time_alive = inCalamity->mTimeAlive;
    outState.float1 = inCalamity->mFloatPlaceHolder1;
    outState.float2 = inCalamity->mFloatPlaceHolder2;
    outState.int1 = (int32_t)inCalamity->mIntPlaceHolder1;
    outState.int2 = (int32_t)inCalamity->mIntPlaceHolder2;
    outState.target[0] = inCalamity->mTargetLocation.mX;
    outState.target[1] = inCalamity->mTargetLocation.mY;
    outState.target[2] = inCalamity->mTargetLocation.mZ;
    outState.epicenter_factor = inCalamity->mEpicenterFactor;
    outState.effect_handle = (uint32_t)inCalamity->mEffectHandle.mValue;
    outState.bool1 = inCalamity->mBoolPlaceHolder1;
    outState.bool2 = inCalamity->mBoolPlaceHolder2;
}

void StateTo(const EeCalamityState& inState, EECalamity* outCalamity)
{
    outCalamity->mTimeAlive = inState.time_alive;
    outCalamity->mFloatPlaceHolder1 = inState.float1;
    outCalamity->mFloatPlaceHolder2 = inState.float2;
    outCalamity->mIntPlaceHolder1 = inState.int1;
    outCalamity->mIntPlaceHolder2 = inState.int2;
    outCalamity->mTargetLocation.mX = inState.target[0];
    outCalamity->mTargetLocation.mY = inState.target[1];
    outCalamity->mTargetLocation.mZ = inState.target[2];
    outCalamity->mEpicenterFactor = inState.epicenter_factor;
    outCalamity->mEffectHandle.mValue = inState.effect_handle;
    outCalamity->mBoolPlaceHolder1 = inState.bool1;
    outCalamity->mBoolPlaceHolder2 = inState.bool2;
}

EEUnit* TargetUnit(EECalamity* inCalamity)
{
    EEUnit* theUnit = NULL;
    inCalamity->GetTargetUnit(theUnit);
    return theUnit;
}

// ---- the classes' world steps (what each EECC* unit held; the original's addresses in each comment) ----

// the class's first effect from the caster to (x, y, z) of the call (CALT records the point); its handle (none
// asked for without inHandle)
uint32_t CasterEffect(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityCall& inCall, bool inHandle)
{
    GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
    EEEffectHandle theEffectHandle;
    EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
    gEffectManager->CreateEffect(inCalamity->mCasterUnit, theLocation, inClass->mGraphicEffect1,
                                 inHandle ? &theEffectHandle : NULL, 0);
    return theEffectHandle.mValue;
}

// a unit's orders cleared and the unit enabled (the time warp's start, the mind control's cure)
void ClearOrders(EEComplexUnit* inUnit)
{
    inUnit->ClearBuildTargets();
    for (unsigned long i = 0; i < inUnit->GetNumGoalQueues(); i++)
        inUnit->ClearGoalQueue(i);
    inUnit->ClearGoals(0);
    inUnit->SetDisabled(0, 0);
}

// a new action for a unit, told to its class
void StartAction(EEComplexUnit* inUnit, long inAction)
{
    inUnit->StartAction(inAction);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// EECCEarthquake::CreateCracks 0xb0502 (with SetAffectedArea 0xb06b5 and CreateCrack 0xb071c): the quake's area,
// then cracks radiating from the target at roughly even, randomly jittered angles until one fails
void QuakeCracks(EECalamityClass* inClass, EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GEVector theDirection;
    GEVector theOffset;
    GE3DPoint theCenter;
    float theStep = 6.2831855f / inClass->mCrackCount;
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    GE2DPoint theTopLeft(theLocation.GetX() - inClass->mRadius, theLocation.GetY() - inClass->mRadius);
    GE2DPoint theBottomRight(theLocation.GetX() + inClass->mRadius, theLocation.GetY() + inClass->mRadius);
    inCalamity->mRectangle = GERectangle(theTopLeft, theBottomRight);
    theCenter = inCalamity->mTargetLocation;
    for (unsigned long i = 0; i < inClass->mCrackCount; i++) {
        theDirection.SetY(gPhysics->mRandom.Generate() - 0.5f + inClass->mRadius);
        theTransformation.Rotate(((gPhysics->mRandom.Generate() * 0.5f - 0.25f) * theStep) + theStep, 0.0f, 0.0f);
        theTransformation.Apply(theDirection, theOffset);
        GE3DPoint theTo(theCenter.GetX() + theOffset.GetX(), theOffset.GetY() + theCenter.mY, 0.0f);
        if (gWorld.GetEffectManager()->CreateEffectBetween(theCenter, theTo, inClass->mGraphicEffect1, NULL) != 0)
            break;
    }
}

// EECCHurricane::CreateStorm 0xb1226: the storm unit centred on the target at the calamity's height, with the storm
// effect on it, as the calamity's target; false if the unit could not be created
bool CreateStorm(EECalamityClass* inClass, EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    theLocation = inCalamity->mTargetLocation;
    float theHalfSize = gUnitClassTable[inClass->mUnitClassIndex]->mTileSize * 0.5f;
    theTransformation.SetX(theLocation.GetX() - theHalfSize);
    theTransformation.SetY(theLocation.GetY() - theHalfSize);
    theTransformation.SetZ(inCalamity->mHeight);
    EEUnit* theUnit = EECreateCalamityUnit(inCalamity, inClass->mUnitClassIndex, theTransformation, -1);
    if (theUnit == NULL)
        return false;
    gEffectManager->CreateUnitEffect(theUnit, inClass->mGraphicEffect2, NULL, 0, 0, 0);
    theUnit->mComplexUnit->SetBuilt(1, 0, 1);
    inCalamity->SetTargetUnit(theUnit);
    return true;
}

// the map tiles within the class's radius of the target (EECCAreaEffect 0xaf411: minus those an area effect makes
// immune when immunity blocks it; EECCLeprosy 0xb1a97: all)
void AffectedTiles(EECalamityClass* inClass, EECalamity* inCalamity, bool inImmunity, std::vector<EETileCoordinate>& outTiles)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    EEGetTilesInRadius(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()), inClass->mRadiusTiles, true, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        if (::gWorld.mMap->GetTile(it->mX, it->mY) == NULL)
            continue;
        if (!inImmunity || !inClass->mIsBlockedByImmunity ||
            !gAEManager.HasAreaEffect(3, it->mX, it->mY, inCalamity->GetPlayerNumber()))
            outTiles.push_back(*it);
    }
}

// EECCAreaEffect::ApplyEffect 0xaf726 (also the extra damage's and the mind control's, which affects the caster's own
// units too) and EECCLeprosy::ApplyEffect 0xb1dd3: the units in range whose type is in the unit set handed to the
// contagion helper; the area effects take enemies of the caster (of nature without one), the leprosy every unit but
// an immune one (which shows its immunity when immunity blocks the leprosy)
void ApplyEffect(EECalamityClass* inClass, EECalamity* inCalamity)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;
    GE2DPoint the2DLocation;
    EEGraphicEffect* theEffect = NULL;
    bool theLeprosy = inClass->mClassEnum == kCalamityLeprosy;
    inCalamity->GetTargetLocation(theLocation);
    AffectedTiles(inClass, inCalamity, !theLeprosy, theTiles);
    the2DLocation = theLocation.Get2DPoint();
    EEGetUnitsInRadius(the2DLocation, (float)inClass->mRadiusTiles, theUnits);
    EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inCalamity->GetIntPlaceHolder1());
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        EEUnit* theUnit = theUnits[i];
        if (!gUnitSetManager.Contains(theUnit->GetUnitClass()->GetUnitType(), inClass->GetUnitSet()))
            continue;
        if (theLeprosy) {
            if (inClass->mIsBlockedByImmunity && static_cast<EEComplexUnit*>(theUnit)->GetEffect(3, theEffect)) {
                if (theEffect != NULL)
                    gEffectManager->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
            } else
                EEContagionHelperAddUnit(theHelper, theUnit, true);
        } else if (inCalamity->GetCasterUnit() != NULL) {
            EEPlayer* theCasterPlayer = inCalamity->mCasterUnit->mPlayer;
            if ((theUnit->GetPlayer() != theCasterPlayer && theCasterPlayer->mEnemies[theUnit->GetPlayer()->mPlayerID] != 0) ||
                inClass->mClassEnum == kCalamityAEMindControl)
                EEContagionHelperAddUnit(theHelper, theUnit, true);
        } else {
            EEPlayer* theGaia = ::gWorld.mPlayers[0];
            if (theUnit->GetPlayer() != theGaia && theGaia->mEnemies[theUnit->GetPlayer()->mPlayerID] != 0)
                EEContagionHelperAddUnit(theHelper, theUnit, true);
        }
    }
}

// EECCLeprosy::ShowEffects 0xb1cca: the leprosy effect from the caster (or the target without one; 0xb1a52) to a
// random point on every affected tile
void LeprosyEffects(EECalamityClass* inClass, EECalamity* inCalamity)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theSourceLocation;
    GE3DPoint theLocation;
    AffectedTiles(inClass, inCalamity, false, theTiles);
    if (inCalamity->mCasterUnit != NULL) {
        const GETransformation& theTransformation = inCalamity->mCasterUnit->mComplexUnit->mPhysicalModel.GetLocation();
        theSourceLocation.SetX(theTransformation.GetX());
        theSourceLocation.SetY(theTransformation.GetY());
        theSourceLocation.SetZ(theTransformation.GetZ());
    } else
        theSourceLocation = inCalamity->mTargetLocation;
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        theLocation.SetX(it->mX + (gPhysics->GetRandom() - 0.5f) + 0.5f);
        theLocation.SetY(it->mY + (gPhysics->mRandom.Generate() - 0.5f) + 0.5f);
        theLocation.SetZ(::gWorld.mMap->GetTile(it->mX, it->mY)->mHeight);
        gEffectManager->CreateEffectBetween(theSourceLocation, theLocation, inClass->mGraphicEffect1, NULL);
    }
}

// EECCVolcano::ClearArea 0xb46bc: movable units off the volcano's footprint, to their next waypoint outside it, else
// onto the first free ring tile (nearest the sort corner), their path dropped
void VolcanoClearArea(EEUnitClass* inClass, long inX, long inY)
{
    GE3DPoint theLocation;
    GE3DPoint theWaypoint;
    GE2DPoint theWaypoint2D;
    GERectangle theArea;
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    GETransformation theTransformation(false);
    long theSize = inClass->mTileSize;
    long theHalfSize = theSize / 2;
    long theLeft = inX - theHalfSize;
    long theTop = inY - theHalfSize;
    long theExtent = theHalfSize - (theSize % 2 == 0 ? 1 : 0);
    theArea.SetBottom(inY + theExtent);
    theArea.SetTop(theTop);
    theArea.SetLeft(theLeft);
    theArea.SetRight(inX + theExtent);
    EEGetUnitsInRectangle(theArea, theUnits);
    unsigned long theCount = theUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
        if (theUnit == NULL || !EEIsUnitMovable(NULL, theUnit))
            continue;
        bool theMoved = false;
        long theWaypointCount = theUnit->mWaypoints.size();
        if (theWaypointCount > 0) {
            float theArg;
            while (theUnit->NextWaypoint() == 0) {
                if (theUnit->GetPathStart(theWaypoint, theArg) != 0)
                    continue;
                theWaypoint2D = GE2DPoint(theWaypoint.GetX(), theWaypoint.GetY());
                if (theArea.Contains(theWaypoint2D))
                    continue;
                theTransformation = theUnit->mPhysicalModel.GetLocation();
                theTransformation.SetPosition(theWaypoint.GetX(), theWaypoint.GetY(), theWaypoint.GetZ());
                theUnit->SetTransformation(theTransformation, true);
                theMoved = true;
                break;
            }
        }
        if (theMoved)
            continue;
        EEGetSquareBlockRing(0, theLeft, theTop, theSize, theTiles, 0);
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theLeft + theSize - 1, theTop));
        unsigned long theTileCount = theTiles.size();
        for (unsigned long j = 0; j < theTileCount; j++) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[j].mX, theTiles[j].GetY());
            if (theTile == NULL || theTile->mUnits != NULL || !EECanStandOn(theTiles[j], theUnit))
                continue;
            theTransformation = theUnit->mPhysicalModel.GetLocation();
            theLocation.SetX(theTiles[j].mX + 0.5f);
            theLocation.SetY(theTiles[j].GetY() + 0.5f);
            theLocation.SetZ(theTransformation.GetZ());
            if (theUnit->mUnitClass->mDomains & kMovementAir)
                theLocation.SetZ(theUnit->mUnitClass->GetHeightAt(theUnit, theLocation));
            else if (::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(theTiles[j].mX, theTiles[j].GetY()))
                theLocation.SetZ(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
            theTransformation.SetX(theLocation.GetX());
            theTransformation.SetY(theLocation.GetY());
            theTransformation.SetZ(theLocation.GetZ());
            theUnit->SetTransformation(theTransformation, true);
            theUnit->DropPath();
            StartAction(theUnit, kActionIdle);
            break;
        }
    }
}

// EECCVolcano::GetMaximumHeight 0xb4b62: the highest terrain mesh height in the 7 by 7 tile block around the location
float VolcanoMaximumHeight(const GE3DPoint& inLocation)
{
    EEMap* theMap = ::gWorld.mMap;
    float theMaximum = -3.4028235e38f;
    long theRadius = UMFloor(3.5f);
    long theEndX = UMFloor(inLocation.GetX()) + theRadius;
    long theStartY = UMFloor(inLocation.GetY()) - theRadius;
    long theEndY = UMFloor(inLocation.GetY()) + theRadius;
    for (long y = theEndY; y > theStartY; y--) {
        for (long x = UMFloor(inLocation.GetX()) - theRadius; x < theEndX; x++) {
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x, y + 1));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x + 1, y + 1));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x + 1, y));
            theMaximum = max(theMaximum, theMap->mTerrainMesh.GetZ(x, y));
        }
    }
    return theMaximum;
}

// EECCVolcano::CreateVolcano 0xb4d19: the volcano unit for the calamity's player centred on the target, units cleared
// off its footprint, as the calamity's target
void CreateVolcano(EECalamityClass* inClass, EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    EEUnitClass* theClass = gUnitClassTable[inClass->mUnitClassIndex];
    float theHalfSize = theClass->mTileSize * 0.5f;
    theTransformation.SetX(theLocation.GetX() - theHalfSize);
    theTransformation.SetY(theLocation.GetY() - theHalfSize);
    theTransformation.SetZ(inCalamity->mHeight - 0.9f);
    EEUnit* theUnit = EECreateCalamityUnit(inCalamity, inClass->mUnitClassIndex, theTransformation, inCalamity->mPlayerNumber);
    if (theUnit != NULL) {
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
        VolcanoClearArea(theClass, UMFloor(theLocation.GetX() - 0.5f), UMFloor(theLocation.GetY() - 0.5f));
        inCalamity->SetTargetUnit(theUnit);
    }
}

// The warped unit's tint, blinking while it is away (EECCTimeWarp's two .bss colours 0x4e5fb8/0x4e5fc8)
GEColor sWarpColor(0.484375f, 0.0f, 0.734375f);
GEColor sBlackColor(0.0f, 0.0f, 0.0f);

// EECCTimeWarp::PlayEffect 0xb3ede: the departure (main sound; the secondary one without it) or arrival (secondary
// sound) effect at the location
void WarpEffect(EECalamityClass* inClass, GE3DPoint& inLocation, EECalamity* inCalamity, bool inDeparting)
{
    EEEffectHandle theEffectHandle;
    EEGraphicEffect* theEffect = inDeparting ? inClass->mGraphicEffect2 : inClass->mGraphicEffect3;
    if (theEffect != NULL) {
        gEffectManager->CreatePlacedEffect(inLocation, theEffect, &theEffectHandle);
        inCalamity->mEffectHandle = theEffectHandle;
    }
    // the departure plays the main sound (the secondary one used to play here too, so the main sound was never heard)
    EESoundObject* theSound = inDeparting && inClass->mSound != NULL ? inClass->mSound : inClass->mSecondarySound;
    SESMInstanceNode* theNode = gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theSound->GetVolume(), 0.0f,
                                                                           1.0f, theSound->GetLooping(), NULL);
    if (inDeparting) {
        inCalamity->SetSound(theNode);
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
    } else {
        inCalamity->SetSecondarySound(theNode);
        if (inCalamity->mSecondarySound != NULL)
            inCalamity->mSecondarySound->Dereference();
    }
}

// the ground under (inX, inY), 0 off the mesh
float GroundAt(float inX, float inY, bool& outValid)
{
    outValid = ::gWorld.GetMap()->mTerrainMesh.GetIsValidTile(UMFloor(inX), UMFloor(inY));
    return outValid ? ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY) : 0.0f;
}

// EECCTimeWarp::Return 0xb3fc9: the target back near its old tile (at its old height above ground if it flies, a ship
// on the water, facing as before) with the arrival effects; killed if no place is found
void WarpReturn(EECalamityClass* inClass, EECalamity* inCalamity)
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    EEUnit* theTargetUnit;
    GE3DPoint theFoundLocation;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
    theUnit->SetIsTinted(true);
    if (!EEFindPlacement(&theFoundLocation, theUnit, NULL, inCalamity->mTilePlaceHolder1.mX, inCalamity->mTilePlaceHolder1.mY,
                         theUnit->mUnitClass->GetTileSize(), inClass->mPlacementRange, 1, 1, 0, 0, &theTransformation, NULL)) {
        theUnit->Kill(1, 0, 0);
        return;
    }
    bool theValid;
    float theGround = GroundAt(theTransformation.GetX(), theTransformation.GetY(), theValid);
    if (theUnit->mUnitClass->mDomains & kMovementAir)
        theTransformation.SetZ(inCalamity->GetFloatPlaceHolder1() + theGround);
    else if ((theUnit->mUnitClass->mDomains & kMovementSea) && theValid) {
        // put the ship back on the water surface
        GE3DPoint theGroundPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        theUnit->mUnitClass->GroundLocation(theGroundPoint);
        theTransformation.SetZ(theGroundPoint.GetZ());
    }
    theLocation = GE3DPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    theLocation.SetZ(theUnit->GetEffectHeight(true) * 0.5f + theLocation.GetZ());
    gEffectManager->CreatePlacedEffect(theLocation, inClass->mGraphicEffect4, NULL);
    theLocation = GE3DPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
    WarpEffect(inClass, theLocation, inCalamity, false);
    theTransformation.SetOrientationYPR(inCalamity->mFloatPlaceHolder2, 0.0f, 0.0f);
    theUnit->SetTransformation(theTransformation, true);
}

// EECCTimeWarp::Depart 0xb41dd: the live target off the map (off-map reason 3), its height above ground, facing and
// position remembered; otherwise the calamity is marked finished
void WarpDepart(EECalamity* inCalamity)
{
    GE3DPoint thePosition;
    EEUnit* theTargetUnit = NULL;
    GEOrientation theOrientation;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
    if (theUnit->mTileX == -10000 || !(theUnit->mHitPoints > 0)) {
        inCalamity->mIntPlaceHolder2 = -1;
        inCalamity->mBoolPlaceHolder1 = true;
        inCalamity->mBoolPlaceHolder2 = true;
        return;
    }
    theUnit->SetIsTinted(false);
    theUnit->GetPosition(thePosition);
    bool theValid;
    inCalamity->mFloatPlaceHolder1 = thePosition.GetZ() - GroundAt(thePosition.GetX(), thePosition.GetY(), theValid);
    theUnit->mPhysicalModel.GetLocation().ConvertToOrientation(theOrientation);
    inCalamity->SetFloatPlaceHolder2(theOrientation.GetYaw());
    inCalamity->mTargetLocation = thePosition;
    theUnit->TakeOffMap();
    EEWUnitsOffMapInfo* theInfo = theUnit->mPlayer->GetOffMapInfo(theUnit);
    if (theInfo != NULL)
        theInfo->mReason = 3;
}

// the teleported caster placed at the tile (EECCTeleport::Teleport's two tries): its group told, the arrival effect,
// its hit points scaled by the damage share (at least 1), enabled; false if no place is found
bool TeleportTo(EECalamityClass* inClass, EEUnit* inCaster, long inX, long inY)
{
    GETransformation theTransformation;
    GE3DPoint theFoundLocation;
    EEComplexUnit* theUnit = inCaster->mComplexUnit;
    if (!EEFindPlacement(&theFoundLocation, theUnit, NULL, inX, inY, theUnit->mUnitClass->GetTileSize(), inClass->mPlacementRange,
                         1, 1, 0, 0, &theTransformation, NULL))
        return false;
    theUnit->SetTransformation(theTransformation, true);
    if (theUnit->mActiveGroup != NULL)
        theUnit->mActiveGroup->TellGroup(theUnit, GE3DPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ()));
    gEffectManager->CreateUnitEffect(inCaster, inClass->mGraphicEffect2, NULL, 0, 0, 0);
    theUnit->SetHitPoints(max(1, UMFloatToLong(theUnit->mHitPoints * inClass->mDamageShare)));
    theUnit->SetDisabled(0, 0);
    return true;
}

// EECCTeleport::Teleport 0xb3665: the live caster near the target, else near where it left, else killed; the arrival
// sound
void Teleport(EECalamityClass* inClass, EECalamity* inCalamity)
{
    EEUnit* theCaster = inCalamity->GetCasterUnit();
    if (theCaster == NULL || !(theCaster->mHitPoints > 0))
        return;
    inCalamity->SetSecondarySound(gSoundManager->PlayAndReturnReferencedNode(inClass->mSecondarySound->GetSampleID(),
                                                                             inClass->mSecondarySound->GetVolume(), 0.0f, 1.0f,
                                                                             inClass->mSecondarySound->GetLooping(), NULL));
    if (inCalamity->mSecondarySound != NULL)
        inCalamity->mSecondarySound->Dereference();
    EEComplexUnit* theUnit = theCaster->mComplexUnit;
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    if (!TeleportTo(inClass, theCaster, UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY())) &&
        !TeleportTo(inClass, theCaster, UMFloor(theUnit->mU0DC.GetX()), UMFloor(theUnit->mU0DC.GetY())))
        theUnit->Kill(1, 0, 0);
}

// EECCTeleport::HoldCaster 0xb390b: the live caster off the map at its position, disabled, off-map reason 4
void TeleportHold(EECalamity* inCalamity)
{
    U2DPoint<float> thePosition;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster == NULL || !(theCaster->mHitPoints > 0))
        return;
    EEComplexUnit* theUnit = theCaster->mComplexUnit;
    theUnit->SetDisabled(1, 0);
    thePosition.SetX(theUnit->mPhysicalModel.GetLocation().GetX());
    thePosition.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
    theUnit->Set2DPosition(thePosition);
    theUnit->TakeOffMap();
    EEWUnitsOffMapInfo* theInfo = theUnit->mPlayer->GetOffMapInfo(theUnit);
    if (theInfo != NULL)
        theInfo->mReason = 4;
}

// EECCLogicBomb's AdjustLocation 0xb21c2: the location raised by the target model's first part's LOD-0 bounding radius
void BombAdjust(EECalamity* inCalamity, GE3DPoint& ioLocation)
{
    EEUnit* theTargetUnit;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEModel* theModel = theTargetUnit->GetModel();
    if (theModel == NULL || theModel->mModels[0] == NULL)
        return;
    ioLocation.SetZ(theModel->mModels[0]->GetBoundingRadius(0) + ioLocation.GetZ());
}

// EECCLogicBomb's disabling of a live target (its update): the disable effect above it, disabled (0x1c), a wonder no
// more; halted, a gate opened
void BombDisable(EECalamityClass* inClass, EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEComplexUnit* theUnit = theTargetUnit->GetComplexUnit();
    if (!(theUnit->GetHitPoints() > 0))
        return;
    if (inClass->mGraphicEffect3 != NULL) {
        EEEffectHandle theEffectHandle;
        float theHeight = theUnit->GetModel()->mModels[0]->GetBoundingRadius(0);
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        thePosition.SetZ(thePosition.GetZ() + theHeight * 1.5f);
        gEffectManager->CreatePlacedEffect(thePosition, inClass->mGraphicEffect3, &theEffectHandle);
        inCalamity->SetEffectHandle(theEffectHandle);
    }
    theUnit->SetDisabled(1, 0x1c);
    EETechTreeNode* theNode = theUnit->GetTechTreeNode();
    if (theNode != NULL && theNode->mData->mKind == 4)
        theUnit->GetPlayer()->RemoveWonder(theUnit);
    if (theTargetUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate)
        theUnit->Halt();
    else {
        StartAction(theUnit, kActionOpen);
        EEUnitClass* theClass = theTargetUnit->GetUnitClass();
        theUnit->SetOpenFraction(1.0f);
        theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(1.0f);
        theClass->GetLastLinked(theUnit)->SetOpenFraction(1.0f);
    }
}

// EECCMeteorStorm::LaunchMeteor 0xb2795: a nature-owned meteor 7 tiles north of and above a random point near the
// target, diving onto that point
void LaunchMeteor(EECalamityClass* inClass, EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    GE3DPoint theStart;
    GE3DPoint theTarget;
    float theOffsetX = (0.5f - gPhysics->GetRandom()) * inClass->mRadius;
    float theOffsetY = (0.5f - gPhysics->GetRandom()) * inClass->mRadius;
    EEUnitClass* theClass = gUnitClassTable[inClass->mUnitClassIndex];
    theStart.SetX(theLocation.GetX() + theOffsetX);
    theStart.SetY(theOffsetY + (theLocation.GetY() + 7.0f));
    theStart.SetZ(theLocation.GetZ() + 7.0f);
    theTarget.SetX(theStart.GetX());
    theTarget.SetY(theLocation.GetY() + theOffsetY);
    theTarget.SetZ(theLocation.GetZ());
    EEUnit* theUnit = theClass->CreateUnit(UMFloor(theStart.GetX()), UMFloor(theStart.GetY()), gWorld.mPlayers[0]->GetPlayerID(), 0,
                                           inCalamity->mCasterUnit);
    EEComplexUnit* theMeteor = theUnit->mComplexUnit;
    theClass->PlayCreationEffect(theMeteor);
    theMeteor->SetLauncherClass(theClass);
    theMeteor->SetOwner(theMeteor);
    theMeteor->SetGoal(EEUGRust::NewAttackGround(theTarget, false), 0, 0);
    theMeteor->SetVelocity(theMeteor->GetMaxVelocity());
    theMeteor->SetProjectileStart(theStart);
    theMeteor->SetLocation(theStart, true);
    theMeteor->SetHeight(theStart.GetZ());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GETransformation theTransformation(theMeteor->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    theMeteor->SetFlightTime(0);
    theMeteor->mOrientation = GEOrientation(theYaw, -0.707f, 0.0f);
    theMeteor->SetProjectileStart(thePosition);
    theMeteor->SetVelocity(UMCos(-0.707f) * theMeteor->GetMaxVelocity());
    theTransformation.SetOrientationYPR(theYaw, -0.707f, 0.0f);
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, theMeteor, &theTileX, &theTileY);
}

// EECCCloaking::CloakUnit 0xafd37: a living capitol that is not already casting casts cloaking on itself, its power
// topped up to the cost first
void CloakUnit(EECalamity* inCalamity, EEComplexUnit* inUnit)
{
    if (!(inUnit->mHitPoints > 0) || (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalCastCalamity))
        return;
    long theCost = inCalamity->mClass->GetCost();
    if (inUnit->GetPower() < theCost && inUnit->mPower != theCost)
        inUnit->mPower = theCost;
    inUnit->SetGoal(EEUGRust::NewCastCalamity(inUnit, 0, inUnit->GetTileX(), inUnit->GetTileY(), 0x43d, true), 0, 0);
    StartAction(inUnit, kActionCalamityPreCast);
}

// EECCCloaking::CloakGroup 0xafb38: the player's capitols chained within twice the cloak radius of the caster, each
// other one cloaked too (a capitol near several members is listed more than once)
void CloakGroup(EECalamity* inCalamity, EEComplexUnit* inUnit, EEAreaEffect* inEffect)
{
    std::vector<EEUnit*> theCandidates;
    std::vector<EEUnit*> theGroup;
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, inUnit->mPlayer->mPlayerID, theCandidates);
    float theRange = inEffect->GetRadius() * 2.0f;
    theGroup.push_back(inUnit);
    bool theAdded;
    do {
        theAdded = false;
        for (unsigned long i = 0; i < theCandidates.size(); i++) {
            EEUnit* theCandidate = EEUnitCast<EEComplexUnit>(theCandidates[i]);
            if (std::find(theGroup.begin(), theGroup.end(), theCandidate) != theGroup.end())
                continue;
            GE3DPoint theCandidateLocation;
            theCandidate->GetPosition(theCandidateLocation);
            unsigned long theGroupSize = theGroup.size();
            for (unsigned long k = 0; k < theGroupSize; k++) {
                GE3DPoint theMemberLocation;
                theGroup[k]->GetPosition(theMemberLocation);
                if (fabs(theCandidateLocation.GetX() - theMemberLocation.GetX()) <= theRange &&
                    fabs(theCandidateLocation.GetY() - theMemberLocation.GetY()) <= theRange) {
                    theGroup.push_back(theCandidate);
                    theAdded = true;
                }
            }
        }
    } while (theAdded);
    for (unsigned long i = 0; i < theGroup.size(); i++) {
        EEComplexUnit* theMember = EEUnitCast<EEComplexUnit>(theGroup[i]);
        if (theMember != inUnit)
            CloakUnit(inCalamity, theMember);
    }
}

// the classes' own steps of their Rust Start and Stop (EeCalamityOp::ClassStep, its index), by class
bool ClassStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECalamityClass* theClass = ioHost.mClass;
    EECalamity* theCalamity = ioHost.mCalamity;
    EEUnit* theTargetUnit = TargetUnit(theCalamity);
    EEUnit* theCaster = theCalamity->mCasterUnit;
    switch (theClass->mClassEnum * 8 + inCall.index) {
    case kCalamityEarthquake * 8 + 1: {
        // CreateSourceEffect's caster part: the quake's area around the target, the source effect from the caster
        GE3DPoint theLocation;
        theCalamity->GetTargetLocation(theLocation);
        GE2DPoint theTopLeft(theLocation.GetX() - theClass->mRadius, theLocation.GetY() - theClass->mRadius);
        GE2DPoint theBottomRight(theLocation.GetX() + theClass->mRadius, theLocation.GetY() + theClass->mRadius);
        GERectangle theRectangle;
        theRectangle.SetTopLeft(theTopLeft);
        theRectangle.SetBottomRight(theBottomRight);
        theCalamity->SetRectangle(theRectangle);
        EEComplexUnit* theUnit = theCaster->mComplexUnit;
        if (theUnit != NULL) {
            const GETransformation& theTransformation = theUnit->mPhysicalModel.GetLocation();
            GE3DPoint theSource(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
            outReply.ok = true;
            outReply.value = (int32_t)gWorld.GetEffectManager()->CreateEffectBetween(theSource, theLocation, theClass->mGraphicEffect2, NULL);
            outReply.x = theSource.DistanceTo(theLocation);
        }
        return true;
    }
    case kCalamityHurricane * 8 + 1:
        CasterEffect(theClass, theCalamity, inCall, false);
        return true;
    case kCalamityLogicBomb * 8 + 4:
    case kCalamityDropObject * 8 + 2:
        CasterEffect(theClass, theCalamity, inCall, true);
        return true;
    case kCalamityAreaEffect * 8 + 1:
    case kCalamityAEExtraDamage * 8 + 1:
    case kCalamityAEMindControl * 8 + 1:
        // the area effects keep the handle
        outReply.id = CasterEffect(theClass, theCalamity, inCall, true);
        return true;
    case kCalamityPlague * 8 + 1:
        // EECCPlague::ShowEffect 0xb3131: a disease effect between the caster's and the target's positions, or a
        // tracer beam between them
        if (theCaster != NULL) {
            long theKind = theClass->mGraphicEffect1->mKind;
            if (theKind == kEffect3DDisease) {
                GE3DPoint theCasterLocation;
                GE3DPoint theTargetLocation;
                theCaster->GetPosition(theCasterLocation);
                theTargetUnit->GetPosition(theTargetLocation);
                gEffectManager->CreateEffectBetween(theCasterLocation, theTargetLocation, theClass->mGraphicEffect1, NULL);
            } else if (theKind == kEffect3DTracerBeam)
                gEffectManager->CreateEffectBetweenUnits(theCaster, theTargetUnit, theClass->mGraphicEffect1, NULL);
        }
        return true;
    case kCalamityLeprosy * 8 + 1:
        LeprosyEffects(theClass, theCalamity);
        return true;
    case kCalamityVolcano * 8 + 1: {
        // the placed effect at the volcano, then the highest ground around it
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect1, NULL);
        outReply.x = VolcanoMaximumHeight(theLocation);
        return true;
    }
    case kCalamityFire * 8 + 1:
        gEffectManager->CreateUnitEffect(theTargetUnit, theClass->mGraphicEffect1, NULL, 0, 0, 0);
        return true;
    case kCalamityMorale * 8 + 1: {
        EEEffectHandle theEffectHandle;
        gEffectManager->CreateEffectBetweenUnits(theCaster, theTargetUnit->mComplexUnit, theClass->mGraphicEffect1, &theEffectHandle);
        outReply.id = theEffectHandle.mValue;
        return true;
    }
    case kCalamityMorale * 8 + 2:
        theTargetUnit->mComplexUnit->mPositiveMorale--;
        return true;
    case kCalamityMorale * 8 + 3:
        theTargetUnit->mComplexUnit->mNegativeMorale--;
        return true;
    case kCalamityTimeWarp * 8 + 1:
        theTargetUnit->mComplexUnit->SetDisabled(1, 0xf);
        theTargetUnit->mComplexUnit->Halt();
        return true;
    case kCalamityTimeWarp * 8 + 2: {
        // the warp's effect above the unit
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        GE3DPoint theLocation;
        EEEffectHandle theEffectHandle;
        theUnit->GetPosition(theLocation);
        theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCaster, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        outReply.id = theEffectHandle.mValue;
        return true;
    }
    case kCalamityTimeWarp * 8 + 3: {
        // untinted, its orders cleared, enabled; its category
        EEComplexUnit* theUnit = theTargetUnit->mComplexUnit;
        theUnit->SetIsTinted(false);
        ClearOrders(theUnit);
        outReply.value = (int32_t)theUnit->mUnitClass->mCategory;
        return true;
    }
    case kCalamityTimeWarp * 8 + 4:
        StartAction(theTargetUnit->mComplexUnit, inCall.value);
        return true;
    case kCalamityTeleport * 8 + 1:
        theCaster->mComplexUnit->SetDisabled(1, 0x10);
        gEffectManager->CreateUnitEffect(theCaster, theClass->mGraphicEffect1, NULL, 0, 0, 0);
        return true;
    case kCalamityTeleport * 8 + 2:
        theCaster->mComplexUnit->SetDisabled(0, 0);
        return true;
    case kCalamityConscription * 8 + 1: {
        EETechnology* theTechnology = theCaster->mPlayer->mTechnologies->Get(kTechConscriptionSpell);
        outReply.ok = theCaster->mPlayer->CanAfford(theTechnology, theCaster->mUnitClass->GetUnitType(), true);
        return true;
    }
    case kCalamityConscription * 8 + 2: {
        EEComplexUnit* theTarget = theTargetUnit->mComplexUnit;
        EETechnology* theTechnology = theCaster->mPlayer->mTechnologies->Get(kTechConscriptionSpell);
        theCaster->mPlayer->Pay(theTechnology, theCaster->mUnitClass->GetUnitType(), 1.0f);
        theTarget->PlayEffect(0x138d);
        EEPlayUnitEffect(theTarget, 0x138d);
        return true;
    }
    case kCalamityLogicBomb * 8 + 1: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        BombAdjust(theCalamity, theLocation);
        outReply.x = theLocation.mX;
        outReply.y = theLocation.mY;
        outReply.z = theLocation.mZ;
        return true;
    }
    case kCalamityLogicBomb * 8 + 2: {
        // the target enabled again; its node's kind (-1 without one)
        theTargetUnit->mComplexUnit->SetDisabled(0, 0);
        theTargetUnit->mUnitClass->ActionChanged(theTargetUnit->mComplexUnit);
        EETechTreeNode* theNode = theTargetUnit->GetTechTreeNode();
        outReply.value = theNode != NULL ? (int32_t)theNode->mData->mKind : -1;
        return true;
    }
    case kCalamityLogicBomb * 8 + 3:
        theTargetUnit->mPlayer->AddWonder(theTargetUnit->mComplexUnit, false);
        return true;
    case kCalamityDropObject * 8 + 1: {
        // the dropped unit next to the target, attacking it
        EEComplexUnit* theDropper = theCaster->mComplexUnit;
        GE3DPoint theLocation;
        EEFindDropLocation(theDropper, &theTargetUnit, &theLocation, theTargetUnit);
        EEUnitClass* theUnitClass = gUnitClassTable[theClass->mUnitClassIndex];
        EEUnit* theUnit = theUnitClass->CreateUnit(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()),
                                                   theDropper->mPlayer->mPlayerID, 0, theDropper);
        theUnit->mComplexUnit->SetLauncherClass(theUnitClass);
        theUnit->mComplexUnit->SetOwner(theUnit->mComplexUnit);
        theUnit->mComplexUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit->GetComplexUnit(), theTargetUnit, 0, 0), 0, 0);
        theUnit->mComplexUnit->SetVelocity(0.0f);
        theUnit->mComplexUnit->SetProjectileStart(theLocation);
        theUnit->mComplexUnit->SetHeight(theLocation.GetZ());
        return true;
    }
    case kCalamityCloaking * 8 + 1: {
        // the cloak's area effect on the caster, the target its position, the sound, the cloak flag; a capitol's group
        EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(theCaster);
        EEAreaEffect* theEffect = gAEManager.CreateAreaEffect(0x2e, theUnit);
        theUnit->mAreaEffects.Add(theEffect);
        theEffect->Dereference();
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theCalamity->mTargetLocation = theLocation;
        theCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(theClass->mSound->GetSampleID(), theClass->mSound->GetVolume(),
                                                                         0.0f, 1.0f, theClass->mSound->GetLooping(), NULL));
        if (theCalamity->mSound != NULL)
            theCalamity->mSound->Dereference();
        theUnit->mCloaked = 1;
        ioHost.mPending = theEffect;
        outReply.ok = theUnit->mUnitClass->mUnitType == kUnitTypeCapitol;
        return true;
    }
    case kCalamityCloaking * 8 + 2:
        CloakGroup(theCalamity, EEUnitCast<EEComplexUnit>(theCaster), static_cast<EEAreaEffect*>(ioHost.mPending));
        return true;
    case kCalamityCloaking * 8 + 3:
        gEffectManager->CreateUnitEffect(theCaster, theClass->mGraphicEffect1, NULL, 0, 0, 0);
        return true;
    case kCalamityCloaking * 8 + 4:
        EEUnitCast<EEComplexUnit>(theCaster)->mAreaEffects.Remove(8);
        return true;
    case kCalamityCloaking * 8 + 5:
        EEUnitCast<EEComplexUnit>(theCaster)->mCloaked = 0;
        return true;
    default:
        return false;
    }
}

// every call of the classes' Rust starts, stops and updates; false for one no class answers
bool Call(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECalamity* theCalamity = ioHost.mCalamity;
    EECalamityClass* theClass = ioHost.mClass;
    switch (inCall.op) {
    // the handles the world views read through (calamity_views.rs: the target's, the caster's and the tiles' plain
    // members never reach this host)
    case EeCalamityOp::TargetHandle: outReply.key = (uint64_t)(uintptr_t)TargetUnit(theCalamity); return true;
    case EeCalamityOp::CasterHandle: outReply.key = (uint64_t)(uintptr_t)theCalamity->mCasterUnit; return true;
    case EeCalamityOp::WorldHandle: outReply.key = (uint64_t)(uintptr_t)&::gWorld; return true;
    case EeCalamityOp::HelperUpdate: {
        EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inCall.index);
        EECalamityCaptureEvent(kCalamityOpHelperUpdate, inCall.index, theHelper != NULL);   // CALW (TEST ONLY)
        if (theHelper != NULL)
            EECalamityCaptureContagion(theHelper, inCall.x);   // CONT (TEST ONLY): theHelper->Update
        return true;
    }
    case EeCalamityOp::Sound:
        theClass->UpdateSound(theCalamity, ioHost.mSounds[inCall.index != 0 ? 1 : 0], inCall.x, inCall.y, inCall.value != 0);
        return true;
    case EeCalamityOp::UnitsNear: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        long theNear = EEGetUnitsNear(theLocation, inCall.w, ioHost.mUnits);
        EECalamityCaptureEvent(kCalamityOpUnitsNear, theNear);   // CALW (TEST ONLY)
        outReply.value = (int32_t)theNear;
        outReply.id = (uint32_t)ioHost.mUnits.size();
        return true;
    }
    case EeCalamityOp::CasterContinent:
        outReply.value = gContinentManager.GetContinent(theCalamity->mCasterUnit, kAIPathNonSpace);
        return true;
    case EeCalamityOp::QuakeUnit: {
        // the earthquake's tests in its order: a land building, alive, then the continent
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        outReply.id = theUnit->mUnitID;
        if (theUnit->mUnitClass->IsLandBuilding()) {
            outReply.flags |= 1;
            if (theUnit->mHitPoints > 0) {
                outReply.flags |= 2;
                outReply.value = gContinentManager.GetContinent(theUnit, kAIPathNonSpace);
            }
        }
        return true;
    }
    case EeCalamityOp::Immune: {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        EEGraphicEffect* theEffect = NULL;
        if (theUnit->GetEffect(3, theEffect)) {
            EECalamityCaptureEvent(kCalamityOpImmune, theUnit->mUnitID);   // CALW (TEST ONLY)
            if (theEffect != NULL)
                gWorld.GetEffectManager()->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
            outReply.ok = true;
        }
        return true;
    }
    case EeCalamityOp::Carried: {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        if (theUnit->IsCarried()) {
            outReply.ok = true;
            outReply.key = (uint64_t)(uintptr_t)theUnit->GetOutermostCarrier();
        }
        return true;
    }
    case EeCalamityOp::TakeHit: {
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        EEUnit* theAttacker = inCall.x != 0.0f ? TargetUnit(theCalamity) : theCalamity->mCasterUnit;
        long theMode = theClass->mIsBlockedByImmunity ? 2 : 3;
        EECalamityCaptureEvent(kCalamityOpTakeHit, theUnit->mUnitID, inCall.value, (float)theCalamity->mPlayerNumber,
                               (float)theMode);   // CALW (TEST ONLY)
        theUnit->TakeHit(theAttacker, inCall.value, theCalamity->mPlayerNumber, theMode);
        return true;
    }
    case EeCalamityOp::StormUnit: {
        // the hurricane's tests in its order, stopping at the first that skips the unit
        EEUnit* theStorm = TargetUnit(theCalamity);
        EEComplexUnit* theStormUnit = theStorm->mComplexUnit;
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        outReply.id = theUnit->mUnitID;
        if (theUnit->mUnitID == theStorm->mUnitID)
            outReply.flags |= 1;
        else if (theUnit->mPlayer == theStormUnit->mPlayer)
            outReply.flags |= 2;
        else if (theStormUnit->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] != 0) {
            outReply.flags |= 4;
            if (gUnitSetManager.Contains(theUnit->mUnitClass->GetUnitType(), theClass->GetUnitSet())) {
                outReply.flags |= 8;
                if (theUnit->mHitPoints > 0) {
                    outReply.flags |= 16;
                    outReply.x = EEGetDistanceSquared(static_cast<EEComplexUnit*>(theUnit), theStormUnit);
                    outReply.y = EEGetUnitRadius(static_cast<EEComplexUnit*>(theUnit), theStormUnit);
                    if (theUnit->mPlayer == ::gWorld.mPlayers[0])
                        outReply.flags |= 32;
                }
            }
        }
        return true;
    }
    case EeCalamityOp::Random: {
        float theRandom = gPhysics->GetRandom();
        EECalamityCaptureEvent(kCalamityOpRandom, 0, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::RandomGenerate: {
        float theRandom = gPhysics->mRandom.Generate();
        EECalamityCaptureEvent(kCalamityOpRandom, 0, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::RandomRange: {
        float theRandom = gPhysics->GetRandom(inCall.x, inCall.y);
        EECalamityCaptureEvent(kCalamityOpRandom, 1, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::PlayClassSound:
        theClass->PlaySound(theCalamity);
        return true;
    case EeCalamityOp::DestroyEffect: {
        EEEffectHandle theHandle;
        theHandle.mValue = (uint32_t)inCall.value;
        EECalamityCaptureEvent(kCalamityOpDestroyEffect, theHandle.mValue);   // CALW (TEST ONLY)
        gEffectManager->DestroyEffect(theHandle);
        return true;
    }
    case EeCalamityOp::MeshZ:
        outReply.x = ::gWorld.mMap->mTerrainMesh.GetMeshZ(inCall.x, inCall.y);
        return true;
    // the starts' and stops' (calamity_start.rs)
    case EeCalamityOp::StopSound:
        if (inCall.index == 0)
            theCalamity->StopSound();
        else
            theCalamity->StopSecondarySound();
        return true;
    case EeCalamityOp::DestroyHelper:
        ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(inCall.index), inCall.value);
        return true;
    case EeCalamityOp::StartHelper: {
        long theHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(theClass->GetContagionFunctor(), theCalamity, NULL);
        outReply.ok = ::gWorld.GetContagionManager()->GetContagionHelper(theHelperID) != NULL;
        outReply.value = (int32_t)theHelperID;
        return true;
    }
    case EeCalamityOp::StartSound: {
        EESoundObject* theSound = ioHost.mSounds[0];
        float theVolume = theSound->GetVolume();
        float thePan = 0.0f;
        float thePitch = 1.0f;
        if (inCall.value != 0) {
            GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
            EEComputeSoundParameters(theSound->GetVolume(), theLocation, theVolume, thePan, thePitch, kCalamitySoundFalloff);
            EECalamityCaptureStartEvent(kCalamityStartSound, 0, 0, theVolume, thePan);   // CALT (TEST ONLY)
        }
        theCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theVolume, thePan, thePitch,
                                                                         theSound->GetLooping(), NULL));
        if (theCalamity->mSound != NULL)
            theCalamity->mSound->Dereference();
        return true;
    }
    case EeCalamityOp::SetHeight:
        theCalamity->mHeight = inCall.x;
        return true;
    case EeCalamityOp::KillTarget: {
        EEUnit* theUnit = TargetUnit(theCalamity);
        theUnit->Kill(0, 0, 0);
        theUnit->mPlayer->DeselectUnit(theUnit);
        theCalamity->SetTargetUnit(NULL);
        return true;
    }
    case EeCalamityOp::CasterSightOff:
        theCalamity->mCasterUnit->mComplexUnit->SightOffHere();
        return true;
    case EeCalamityOp::CasterSight:
        outReply.x = theCalamity->mCasterUnit->mComplexUnit->GetSight();
        return true;
    case EeCalamityOp::CasterSightOn: {
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        theUnit->SetSight(inCall.x);
        theUnit->SightOnHere();
        return true;
    }
    case EeCalamityOp::TargetProtected:
        outReply.ok = TargetUnit(theCalamity)->mComplexUnit->IsProtected();
        return true;
    // the classes' own (each answered for the classes whose runs send it)
    case EeCalamityOp::ClassStep:
        return ClassStep(ioHost, inCall, outReply);
    case EeCalamityOp::Cracks:
        EECalamityCaptureEvent(kCalamityOpCracks);   // CALW (TEST ONLY)
        QuakeCracks(theClass, theCalamity);
        return true;
    case EeCalamityOp::CameraDistance: {
        // the earthquake's UpdateEpicenterFactor: the distance from the camera to the target, in the view plane
        GE3DPoint theViewLocation;
        GE3DPoint theTarget;
        theCalamity->GetTargetLocation(theTarget);
        GE3DPoint theOrigin;
        gViewport.GetCameraInverse().Apply(theTarget, theViewLocation);
        theViewLocation.SetY(0.0f);
        outReply.x = theViewLocation.DistanceTo(theOrigin);
        return true;
    }
    case EeCalamityOp::Shake: {
        // the earthquake's ShakeCamera: the local player's camera (once per player and manager update)
        EEPlayer* thePlayer = ::gWorld.GetCurrentPlayer();
        if (!thePlayer->mEarthquakeShaken) {
            thePlayer->mEarthquakeShaken = true;
            float theAngle = inCall.x;
            float theAmplitude = inCall.y;
            EECalamityCaptureEvent(kCalamityOpShake, 0, 0, theAngle, theAmplitude);   // CALW (TEST ONLY)
            GE3DPoint theLocation;
            ::gWorld.mCameraControl.GetLocation(::gWorld.mCameraControl.mFraction, theLocation);
            theLocation.mX += UMCos(theAngle) * theAmplitude;
            theLocation.SetY(theLocation.GetY() + UMCos(1.5707964f - theAngle) * theAmplitude);
            ::gWorld.mCameraControl.SetLocation(theLocation, true, true);
            theCalamity->mShakeLocation = theLocation;
        }
        return true;
    }
    case EeCalamityOp::CreateStorm: {
        bool theCreated = CreateStorm(theClass, theCalamity);
        EECalamityCaptureEvent(kCalamityOpCreateStorm, theCreated);   // CALW (TEST ONLY)
        return true;
    }
    case EeCalamityOp::StormPrepare: {
        EEComplexUnit* theStormUnit = TargetUnit(theCalamity)->mComplexUnit;
        theStormUnit->SetIsDeletable(false);
        const GETransformation& theStormLocation = theStormUnit->mPhysicalModel.GetLocation();
        outReply.x = theStormLocation.GetX();
        outReply.y = theStormLocation.GetY();
        outReply.z = theStormLocation.GetZ();
        return true;
    }
    case EeCalamityOp::Strike: {
        EEComplexUnit* theStormUnit = TargetUnit(theCalamity)->mComplexUnit;
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        EECalamityCaptureEvent(kCalamityOpStrike, theUnit->mUnitID);   // CALW (TEST ONLY)
        if (theClass->mGraphicEffect3->mKind == kEffect2DLightning)
            gEffectManager->CreateUnitEffect(theUnit, theClass->mGraphicEffect3, NULL, 0, 0, 0);
        else
            gEffectManager->CreateEffectBetweenUnits(theStormUnit, theUnit, theClass->mGraphicEffect3, NULL);
        return true;
    }
    case EeCalamityOp::LaunchMeteor:
        EECalamityCaptureEvent(kCalamityOpLaunchMeteor);   // CALW (TEST ONLY)
        LaunchMeteor(theClass, theCalamity);
        return true;
    case EeCalamityOp::CreateVolcano: {
        CreateVolcano(theClass, theCalamity);
        EEUnit* theVolcano = TargetUnit(theCalamity);
        gEffectManager->CreateUnitEffect(theVolcano, theClass->mGraphicEffect3, NULL, 0, 0, 0);
        long theResult = gEffectManager->CreateUnitEffect(theVolcano, theClass->mGraphicEffect2, NULL, 0, 0, 0);
        EECalamityCaptureEvent(kCalamityOpCreateVolcano, theResult);   // CALW (TEST ONLY)
        outReply.value = (int32_t)theResult;
        return true;
    }
    case EeCalamityOp::WarpTint:
    case EeCalamityOp::WarpAwayEffect: {
        // the time warp's departure: the unit tinted, its effect and sound above it; or the placed effect where it
        // leaves
        EEComplexUnit* theUnit = TargetUnit(theCalamity)->mComplexUnit;
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        if (inCall.op == EeCalamityOp::WarpAwayEffect) {
            theLocation.SetZ(theUnit->GetEffectHeight(true) * 0.5f + theLocation.GetZ());
            gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect4, NULL);
            return true;
        }
        theLocation.SetZ(theUnit->GetEffectHeight(true) + theLocation.GetZ() + 0.2f);
        EECalamityCaptureEvent(kCalamityOpWarpTint);   // CALW (TEST ONLY)
        theUnit->SetIsTinted(true);
        WarpEffect(theClass, theLocation, theCalamity, true);
        return true;
    }
    case EeCalamityOp::WarpDepart:
        EECalamityCaptureEvent(kCalamityOpWarpDepart);   // CALW (TEST ONLY)
        WarpDepart(theCalamity);
        return true;
    case EeCalamityOp::WarpReturn:
        EECalamityCaptureEvent(kCalamityOpWarpReturn);   // CALW (TEST ONLY)
        WarpReturn(theClass, theCalamity);
        return true;
    case EeCalamityOp::WarpBlink: {
        EEComplexUnit* theUnit = TargetUnit(theCalamity)->mComplexUnit;
        EECalamityCaptureEvent(kCalamityOpWarpBlink);   // CALW (TEST ONLY)
        if (theUnit->mColor == sWarpColor)
            theUnit->SetColor(sBlackColor);
        else
            theUnit->SetColor(sWarpColor);
        return true;
    }
    case EeCalamityOp::Hold:
        EECalamityCaptureEvent(kCalamityOpHold);   // CALW (TEST ONLY)
        TeleportHold(theCalamity);
        return true;
    case EeCalamityOp::Teleport:
        EECalamityCaptureEvent(kCalamityOpTeleport);   // CALW (TEST ONLY)
        Teleport(theClass, theCalamity);
        return true;
    case EeCalamityOp::LogicBombMark: {
        EECalamityCaptureEvent(kCalamityOpLogicBombMark);   // CALW (TEST ONLY)
        GE3DPoint theLocation;
        theCalamity->GetTargetLocation(theLocation);
        BombAdjust(theCalamity, theLocation);
        theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()) + 0.05f);
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect2, NULL);
        return true;
    }
    case EeCalamityOp::LogicBombDisable:
        EECalamityCaptureEvent(kCalamityOpLogicBombDisable);   // CALW (TEST ONLY)
        BombDisable(theClass, theCalamity);
        return true;
    case EeCalamityOp::CreateHelper: {
        long theCalamityHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(theClass->mFunctor, theCalamity, NULL);
        EECalamityCaptureEvent(kCalamityOpCreateHelper, theCalamityHelperID);   // CALW (TEST ONLY)
        outReply.value = (int32_t)theCalamityHelperID;
        return true;
    }
    case EeCalamityOp::ApplyEffect:
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        ApplyEffect(theClass, theCalamity);
        return true;
    case EeCalamityOp::PlacedEffect: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect2, &theEffectHandle);
        EECalamityCaptureEvent(kCalamityOpPlacedEffect, theEffectHandle.mValue, 0, theLocation.mX, theLocation.mZ);   // CALW (TEST ONLY)
        outReply.id = (uint32_t)theEffectHandle.mValue;
        return true;
    }
    case EeCalamityOp::Morale: {
        EECalamityCaptureEvent(kCalamityOpMorale);   // CALW (TEST ONLY)
        EEUnit* theTargetUnit = TargetUnit(theCalamity);
        EEGraphicEffect* theEffect = theTargetUnit->mUnitClass->mMoraleEffect;
        if (theEffect != NULL) {
            EEEffectHandle theEffectHandle;
            gEffectManager->CreateUnitEffect(theTargetUnit, theEffect, &theEffectHandle, 0, 0, 0);
            if (theCalamity->mEffectHandle != kNoEffectHandle)
                gEffectManager->DestroyEffect(theCalamity->mEffectHandle);
            theCalamity->mEffectHandle = theEffectHandle;
        }
        if (theClass->mPositive)
            theTargetUnit->mComplexUnit->mPositiveMorale++;
        else
            theTargetUnit->mComplexUnit->mNegativeMorale++;
        EESoundObject* theSound = theClass->mSecondarySound;
        if (theSound != NULL)
            gSoundManager->Play(theSound->GetSampleID(), theSound->GetVolume(), 0.0f, 1.0f, theSound->GetLooping(), NULL);
        return true;
    }
    case EeCalamityOp::SpecialAttack:
    case EeCalamityOp::SpecialLocation: {
        // the caster's order: a special attack on the target unit, or to the target location
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        if (inCall.op == EeCalamityOp::SpecialAttack) {
            EEUnit* theTargetUnit = TargetUnit(theCalamity);
            EECalamityCaptureEvent(kCalamityOpSpecialAttack, theTargetUnit->mUnitID);   // CALW (TEST ONLY)
            theUnit->ClearGoals(0);
            theUnit->SetGoal(EEUGRust::NewAttackUnit(theUnit, theTargetUnit, 1, 0), 0, 0);
            theUnit->mIsSpecialAttack = true;
            StartAction(theUnit, kActionAttackEnemyUnit);
        } else {
            GE3DPoint theLocation;
            theLocation = theCalamity->mTargetLocation;
            EECalamityCaptureEvent(kCalamityOpSpecialLocation);   // CALW (TEST ONLY)
            theUnit->ClearGoals(0);
            theUnit->SetGoal(EEUGRust::NewLocation(&theLocation, 1, 1, 0, 0.0f, 0, 0), 0, 0);
            StartAction(theUnit, kActionSpecialLocation);
        }
        return true;
    }
    default:
        return false;
    }
}

void HostCall(void* inContext, EeCalamityState* ioState, const EeCalamityCall* inCall, EeCalamityReply* outReply)
{
    EECalamityUpdateHost& theHost = *static_cast<EECalamityUpdateHost*>(inContext);
    // the calamity as the Rust update has it, for the world step; and back after it
    StateTo(*ioState, theHost.mCalamity);
    *outReply = EeCalamityReply();
    bool theAnswered = Call(theHost, *inCall, *outReply);
    ESVERIFY_AT(theAnswered, 1);
    StateFrom(theHost.mCalamity, *ioState);
}

}   // namespace

namespace {

// the host of one Rust run over the class and calamity (its sounds: the ones the class's run passes); the calamity's
// members in and out (EeCalamityState) around it
struct EECalamityRun {
    EECalamityUpdateHost mHost;
    EeCalamityHostFfi mFfi;
    EeCalamityState mState;

    EECalamityRun(EECalamityClass* inClass, EECalamity* inCalamity, EESoundObject* inSound, EESoundObject* inSecondarySound)
    {
        this->mHost.mClass = inClass;
        this->mHost.mCalamity = inCalamity;
        this->mHost.mSounds[0] = inSound;
        this->mHost.mSounds[1] = inSecondarySound;
        this->mHost.mPending = NULL;
        this->mFfi.ctx = &this->mHost;
        this->mFfi.call = &HostCall;
        StateFrom(inCalamity, this->mState);
    }
    ~EECalamityRun() { StateTo(this->mState, this->mHost.mCalamity); }
};

}   // namespace

bool EECalamityRunStart(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                        EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    return ee_calamity_start(&inParams, &theRun.mState, &theRun.mFfi);
}

void EECalamityRunStop(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                       EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    ee_calamity_stop(&inParams, &theRun.mState, &theRun.mFfi);
}

long EECalamityRunUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime, const EeCalamityParams& inParams,
                         EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EECalamityRun theRun(inClass, inCalamity, inSound, inSecondarySound);
    return ee_calamity_update(&inParams, &theRun.mState, inElapsedTime, &theRun.mFfi);
}
