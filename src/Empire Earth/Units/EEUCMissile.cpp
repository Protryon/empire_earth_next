// C:\EEX\Empire Earth\Units\EEUCMissile.cpp (file name inferred: no __FILE__; the class name). The missile's unit
// class: 0x217337..0x217c6d (after EEUCMiningBuilding.cpp; EEUCNaval.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (vtable 0x448cf8); +0x74/+0x78/+0xac are also EEUCTorpedo's (ICF).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GETransformation.h"
#include "lle/SESoundManager.h"
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUCMissile.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMiniMap.h"
#include "ee/EmpireEarth.h"

// Actual original pooled words 0x447230/0x447234; values verified independently of code references.
static const float kMissileHeightOffset = 0.3f;
static const float kMissileProximitySquared = 2.25f;

// The original first argument reads only the base tile. Its sole ComplexUnit caller does not uniquely
// recover narrower pointer specificity; this declaration records the minimum proven contract.
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget);

// FUNCTION: EXE 0x617337
// Returns whether the missile hit the ground after its flight time (0.3 higher over its target): sets it down there
// and, for an attack-ground goal, leaves a shell hole and returns true (never for a missile base's missiles)
bool EEUCMissile::CheckGroundImpact(EEComplexUnit* inUnit)
{
    long theX;
    long theY;
    long theType = inUnit->mLauncherClass->mUnitType;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    bool theResult = false;
    if ((theType != kUnitTypeCpMissileBase && theGoal == NULL) || (theType == kUnitTypeCpMissileBase && theGoal != NULL && theGoal->mKind != kGoalAttackGround))
        return false;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    float theOffset = 0.0f;
    bool theNear = false;
    if (theGoal != NULL) {
        theGoal->GetGoalLocation(theTarget);
        theNear = theGoal->mKind != kGoalAttackGround && !(thePosition.DistanceToSquared(theTarget) > kMissileProximitySquared);
//
    }
    if (inUnit->mFlightTime != 0 || theNear)
        return false;
    // a missile base's missile may get here without a goal: there is no goal kind to test (it used to be read
    // through NULL)
    if (theGoal == NULL)
        return false;
    if ((inUnit->mGoalState->mKind == kGoalAttackUnit && !EEIsOnTargetTiles(inUnit, static_cast<EEUGAttackUnit*>(inUnit->GetGoal())->mTarget))
        || (inUnit->mLauncherClass->mUnitType == kUnitTypeCpMissileBase
            && ::gWorld.mMap->GetTile(UMFloatToLong(theTarget.GetX()), UMFloatToLong(theTarget.GetY())) == inUnit->mTile))
        theOffset = kMissileHeightOffset;
    ::gWorld.mMap->GetGroundPoint(thePosition, theTarget);
    if (thePosition.GetZ() < theTarget.GetZ() + theOffset && theTarget.GetZ() >= 0.0f
        && inUnit->mLauncherClass->mUnitType != kUnitTypeMissileBase) {
        inUnit->PlaceOnGround(thePosition);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetZ(theTransformation.GetZ() + theOffset);
        EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
        theResult = inUnit->mGoalState->mKind == kGoalAttackGround;
        if (theResult && inUnit->mUnitClass->mShellHoleEffect != NULL)
            EEPlaceImpactDecal(inUnit, thePosition);
    }
    return theResult;
}

// FUNCTION: EXE 0x6174fb
// Returns whether a unit stands on one of the target's tiles (every part of a multi-part target, its whole footprint)
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget)
{
    EEComplexUnit* theUnit = inTarget->mComplexUnit;
    RTSTile* theTile = inUnit->mTile;
    bool theResult = false;
    if (theUnit != NULL) {
        while (theUnit->mPreviousPart != NULL)
            theUnit = theUnit->mPreviousPart;
        while (theUnit != NULL && !theResult) {
            long theSize = theUnit->mUnitClass->mTileSize;
            if (theSize == 1)
                theResult = theTile == theUnit->mTile;
            else {
                unsigned long theX = theUnit->mTileX;
                unsigned long theEndX = theX + theSize;
                while (theX < theEndX && !theResult) {
                    unsigned long theY = theUnit->mTileY;
                    unsigned long theEndY = theY + theSize;
                    while (theY < theEndY && !theResult) {
                        theResult = theTile == ::gWorld.mMap->GetTile(theX, theY);
                        ++theY;
                    }
                    ++theX;
                }
            }
            theUnit = theUnit->mNextPart;
        }
    } else
        theResult = theTile == inTarget->mTile;
    return theResult;
}

// FUNCTION: EXE 0x6175cb
// Returns whether the missile hit water (z 0 or below on terrain it cannot cross); if so, it attacks the ground there
bool EEUCMissile::CheckWaterImpact(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint theTarget;
    bool theResult = false;
    long theTerrain = inUnit->mTile->mTerrainType;
    if (thePosition.GetZ() <= 0.0f && !this->CanMoveOnTerrain(inUnit, theTerrain)) {
        theTarget.SetY(thePosition.GetY());
        theTarget.SetZ(0.0f);
        theTarget.SetX(thePosition.GetX());
        inUnit->SetGoal(new EEUGAttackGround(theTarget, false), 0, 0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x61765d
// Handles the missile's new action: on a hit plays the splash or death effect and sound, fires its shot and dies
// (attack-unit goals, ICBM ground strikes); switches to attack ground; on retarget follows the target's container
void EEUCMissile::ActionChanged(EEComplexUnit* inUnit)
{
    float theVolume;
    float thePan;
    float thePitch;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GE3DPoint theGround(0.0f, 0.0f, 0.0f);
    EEUnitGoal* theGoal = inUnit->mGoalState;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionSwitchToAttackGround: {
        GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
        theGoal->GetGoalLocation(theTarget);
        inUnit->SetGoal(new EEUGAttackGround(theTarget, theGoal->GetB1C()), 0, 0);
        inUnit->StartAction(kActionAdvance);
        this->ActionChanged(inUnit);
        break;
    }
    case kActionHitTarget: {
        thePosition.SetX(inUnit->mPhysicalModel.GetLocation().GetX());
        thePosition.SetY(inUnit->mPhysicalModel.GetLocation().GetY());
        thePosition.SetZ(inUnit->mPhysicalModel.GetLocation().GetZ());
        if (thePosition.GetZ() < 0.0f && inUnit->mTile->GetHeight() < 0.0f && theGoal->mKind != kGoalAttackUnit && this->mSplashEffect != NULL) {
            thePosition.SetZ(0.0f);
            gEffectManager->CreatePlacedEffect(thePosition, this->mSplashEffect, NULL);
        } else if (this->mDeathEffect != NULL) {
            if (thePosition.GetZ() < inUnit->mTile->GetHeight())
                ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
            else
                theGround = thePosition;
            inUnit->mPhysicalModel.SetLocationXYZ(theGround);
            gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0);
        }
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(4, -1, 0);
        inUnit->DropPath();
        if (inUnit->mHitPoints > 0)
            this->FireShot(inUnit);
        if (inUnit->mGoalState->mKind == kGoalAttackUnit
            || (inUnit->mUnitClass->mUnitType == kUnitTypeCpICBM && inUnit->mGoalState->mKind == kGoalAttackGround)) {
            inUnit->StartAction(kActionDead);
            this->ActionChanged(inUnit);
            if (inUnit->mUnitClass->GetPopulationCost() > 0)
                inUnit->mPlayer->mPopulationCount -= inUnit->mUnitClass->GetPopulationCost();
            gMiniMap->MoveUnit(inUnit, inUnit->mTileX, inUnit->mTileY, -10000, -10000);
        }
        break;
    }
    case kActionSelectNewTarget: {
        EEUGAttackUnit* theAttack = static_cast<EEUGAttackUnit*>(theGoal);
        if (theAttack->mTarget->mHitPoints < 1) {
            inUnit->StartAction(kActionDying);
            this->ActionChanged(inUnit);
        } else
            theAttack->SetTarget(theAttack->mTarget->mComplexUnit->mContainer);
        break;
    }
    default:
        this->HandleActionChanged(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x617916
// Sets up a new missile: records its start point and stops it; all but the ICBM start the flight animation and advance
void EEUCMissile::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    inUnit->SetVelocity(0.0f);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeCpICBM)
        return;
    inUnit->SetAnimation(0xd, -1, 0);
    inUnit->StartAction(kActionAdvance);
    this->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x61799b
// Launches the missile: tilts it to its launcher's launch pitch and starts the launch delay and a 300 s lifetime; true
bool EEUCMissile::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GETransformation theRotation;
    EEUnitClass* theClass = inUnit->mLauncherClass;
    theRotation.SetOrientationYPR(0.0f, theClass->mLaunchPitch, 0.0f);
    GETransformation theTransformation(false);
    inUnit->mPhysicalModel.GetLocation().Apply(theRotation, theTransformation);
    long theX;
    long theY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    inUnit->SetFlightTime(theClass->mMissileDelayTime);
    inUnit->mCloaked = 300000;
    inUnit->SetVelocity(0.0f);
    return true;
}

// FUNCTION: EXE 0x617a37
// Answers the missile triggers: hit unit (attacks it; also true when its lifetime ran out), ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUCMissile::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerProjectileHitUnit: {
        EEUnit* theTarget = this->FindImpactUnit(inUnit);
        if (theTarget != NULL) {
            inUnit->SetGoal(new EEUGAttackUnit(inUnit, theTarget, false, false), 0, 0);
            theFlag = true;
        } else if (inUnit->mCloaked == 0)
            theFlag = true;
        break;
    }
    case kTriggerResourcesRottedAway:
    case kTriggerUnitRottedAway:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit)
            theFlag = true;
        else if (EEUnitClass::EvaluateCondition(inUnit, inEvent))
            theFlag = true;
        break;
    case kTriggerDyingAnimationFinished:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit)
            theFlag = true;
        else if (EEUnitClass::EvaluateCondition(inUnit, inEvent))
            theFlag = true;
        break;
    case kTriggerProjectileHitGround:
        theFlag = this->CheckGroundImpact(inUnit);
        break;
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerUnitTargetNoLongerValid:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
            EEUnit* theTarget = static_cast<EEUGAttackUnit*>(inUnit->GetGoal())->mTarget;
            if (theTarget->mHitPoints < 1)
                theFlag = true;
            else if (theTarget->mComplexUnit != NULL && theTarget->mComplexUnit->mContainer != NULL)
                theFlag = true;
            else
                theFlag = false;
        }
        break;
    case kTriggerProjectileHitWater:
        theFlag = this->CheckWaterImpact(inUnit);
        break;
    case kTriggerNewTargetSelected:
        theFlag = true;
        break;
    case kTriggerUnitHitSpace:
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x617b77
// Updates the trail of a living, moving missile when the effect detail allows it
void EEUCMissile::UpdateEffects(EEComplexUnit* inUnit)
{
    if (inUnit->mHitPoints > 0 && this->mTrailEffect != NULL && inUnit->mVelocity != 0.0f && this->mTrailEffect->mDetailLevel <= gEffectDetail)
        this->UpdateTrail(inUnit);
}

// FUNCTION: EXE 0x617bb2
// Steps the missile's physics, effects and sound, and counts down its rotting time, launch delay and lifetime
void EEUCMissile::Update(EEComplexUnit* inUnit)
{
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    this->UpdateEffects(inUnit);
    this->RefreshSound(inUnit);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionUnitRotting) {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
    }
    long theElapsed = gPhysics->mTickTimeMs;
    if (theElapsed == 0)
        theElapsed = 1;
    if (inUnit->mFlightTime != 0) {
        long theTime = inUnit->mFlightTime - theElapsed;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetFlightTime(theTime);
    }
    if ((long)inUnit->mCloaked <= 0)
        return;
    long theTime = inUnit->mCloaked - theElapsed;
    if (theTime < 0)
        theTime = 0;
    inUnit->mCloaked = theTime;
}
