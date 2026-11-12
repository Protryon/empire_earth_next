// C:\EEX\Empire Earth\Units\EEUCProjectile.cpp (file name inferred: no __FILE__; the class name). The projectile's unit
// class (EEUCBomb derives from it and shares these slots by ICF): 0x219143..0x219c49 (after EEUCPlanetSpaceship.cpp;
// EEUCResource.cpp's +0x7c 0x219c49 follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe0, +0xe4, +0x74, +0x78, +0xac, +0xa0, +0xbc, +0xb8 in source order.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EEUnitKind.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUCProjectile.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"

// 0x447338 (3.0, the launch height below which a projectile may hit the ground at once) is ee_combat's (impact.rs)
// 0x447050: the slow-down per unit of depth below the ground

// 0x241a6a (cdecl): the launch angle from inOwner at inLocation for the ammunition (provisional name)
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo, float* outAngle);

// FUNCTION: EXE 0x619143
// Returns whether a projectile hit the ground (bombs, low launches, after 1 s of flight): attacks the ground there
// unless it came down on its target building or wall, and leaves a shell hole on land
bool EEUCProjectile::CheckGroundImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's projectile_ground (impact.rs); the ground point, the goal and the shell hole stay here
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_PROJECTILE_GROUND;
    in.bomb = this->mCategory == kUnitCategoryBomb;
    in.flight_time = (int32_t)inUnit->mFlightTime;
    in.start_z = inUnit->mProjectileStart.GetZ();
    inUnit->GetPosition(thePosition);
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    in.tile_present = ::gWorld.mMap->GetTile(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY())) != NULL;
    in.has_goal = theGoal != NULL;
    in.goal_kind = theGoal != NULL ? (int32_t)theGoal->mKind : 0;
    in.shell_hole = inUnit->GetUnitClass()->mShellHoleEffect != NULL;
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (!theOut.result)
        return false;
    // the ground point: the position at the ground's height (the unit's tile's off the map)
    GE3DPoint theGround(thePosition.GetX(), thePosition.GetY(), in.ground_read ? in.ground_z : in.tile_height);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_GROUND)
        inUnit->SetGoal(new EEUGAttackGround(theGround, false), 0, 0);
    if (theOut.decal)
        EEPlaceImpactDecal(inUnit, theGround);
    return true;
}

// FUNCTION: EXE 0x6192d0
// Returns whether a bomb, or a projectile after 1 s of flight, hit water it cannot cross; if so, it attacks the ground
bool EEUCProjectile::CheckWaterImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's projectile_water (impact.rs)
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_PROJECTILE_WATER;
    in.bomb = this->mCategory == kUnitCategoryBomb;
    in.flight_time = (int32_t)inUnit->mFlightTime;
    inUnit->GetPosition(thePosition);
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_WATER)
        inUnit->SetGoal(new EEUGAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x619388
// Handles the projectile's new action: on a hit plays its effects and sound, drops a parachute's cargo or fires, may
// set a target on fire (flaming arrows), dies and leaves remains; retargets a dead target's tile or its container
void EEUCProjectile::ActionChanged(EEComplexUnit* inUnit)
{
    float theVolume;
    float thePan;
    float thePitch;
    bool theAlive;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GE3DPoint theGround(0.0f, 0.0f, 0.0f);
    EEUnitGoal* theGoal = inUnit->mGoalState;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionUnitRotting:
        inUnit->SetAnimation(5, -1, 0);
        inUnit->SetRottingAwayTime(this->mRottingTime);
        break;
    case kActionHitTarget: {
        thePosition.SetX(inUnit->mPhysicalModel.GetLocation().GetX());
        thePosition.SetY(inUnit->mPhysicalModel.GetLocation().GetY());
        float theZ = inUnit->mPhysicalModel.GetLocation().GetZ();
        thePosition.SetZ(theZ);
        if (thePosition.GetZ() < 0.0f && inUnit->mTile->GetHeight() < 0.0f && theGoal->mKind != kGoalAttackUnit && this->mSplashEffect != NULL) {
            thePosition.SetZ(0.0f);
            gEffectManager->CreatePlacedEffect(thePosition, this->mSplashEffect, NULL);
        } else if (this->mDeathEffect != NULL) {
            // the original keeps the z in st(0) (theZ) and compares z < height (EEUCMissile's twin shape)
            if (thePosition.GetZ() < inUnit->mTile->GetHeight())
                ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
            else
                theGround = thePosition;
            // the location's x, y, z set in place, as the original's const_cast stores did (New, R5b.4: through the
            // setter, which reports them to the unit store)
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
        if (inUnit->mUnitClass->mUnitType == kUnitTypeParachuteBomb) {
            inUnit->UnloadCargoAtTile((long)thePosition.GetX(), (long)thePosition.GetY(), false);
            if (inUnit->GetNumCargoUnits() != 0)
                inUnit->KillCargo();
        } else {
            this->FireShot(inUnit);
        }
        if (inUnit->mUnitClass->mUnitType == kUnitTypeBombForSASExplosiveExpert && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
            EEUnit* theUnit = static_cast<EEUGAttackUnit*>(inUnit->mGoalState)->mTarget;
            EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(theUnit);
            if (theTarget != NULL)
                theTarget->SetIsTinted(false);
        }
        if (inUnit->GetLineOfSight() > 0.0f) {
            inUnit->SightOffHere();
            theAlive = true;
        } else {
            theAlive = false;
        }
        inUnit->SetHitPoints(0);
        if (theAlive)
            inUnit->SightOnHere();
        if (inUnit->mUnitClass->mUnitType == kUnitTypeFlamingArrow && inUnit->mComplexUnit->mGoalState->mKind == kGoalAttackUnit) {
            EECalamityClass* theClass = NULL;
            USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(0x17, theClass);
            EEUnit* theTarget = static_cast<EEUGAttackUnit*>(inUnit->mComplexUnit->mGoalState)->mTarget;
            if (gUnitSetManager.GetSet(theClass->GetUnitSet())->Contains(theTarget->mUnitClass->mUnitType)) {
                EECivPower* thePower = gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows);
                float theRoll = gPhysics->mRandom.Generate() * 100.0f;
                if (theRoll <= thePower->mChance)
                    USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(
                        0x18, -1, static_cast<EEUGAttackUnit*>(inUnit->mComplexUnit->mGoalState)->mTarget->GetUnitID(),
                        inUnit->mPlayer->GetPlayerID());
            }
        }
        if (inUnit->mGoalState->mKind == kGoalAttackUnit)
            inUnit->StartAction(kActionDead);
        else
            inUnit->StartAction(kActionDying);
        this->ActionChanged(inUnit);
        if (this->mRemainsClass != NULL && this->mIsProjectile && !gWorldClearing && !gFreeCamera)
            this->CreateRemainsUnit(inUnit);
        break;
    }
    case kActionSelectNewTarget: {
        EEUnit* theTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
        if (theTarget->mHitPoints < 1) {
            thePosition.SetX((float)(static_cast<EEUGAttackUnit*>(theGoal))->mTarget->mTileX);
            thePosition.SetY((float)(static_cast<EEUGAttackUnit*>(theGoal))->mTarget->mTileY);
            EEMapTile* theTile = ::gWorld.mMap->GetTile(static_cast<EEUGAttackUnit*>(theGoal)->mTarget->mTileX,
                                                        static_cast<EEUGAttackUnit*>(theGoal)->mTarget->mTileY);
            if (theTile != NULL)
                thePosition.SetZ(theTile->GetHeight());
            inUnit->SetGoal(new EEUGAttackGround(thePosition, false), 0, 0);
        } else {
            static_cast<EEUGAttackUnit*>(theGoal)->SetTarget(theTarget->mComplexUnit->mContainer);
        }
        break;
    }
    default:
        this->HandleActionChanged(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x6197d3
// Sets up a new projectile: records its start point, sets it to full speed and starts it advancing
void EEUCProjectile::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    inUnit->SetVelocity(inUnit->mStats->GetMaxVelocity());
    inUnit->SetAnimation(0xd, -1, 0);
    inUnit->StartAction(kActionAdvance);
    this->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x619850
// Launches the projectile toward a location at the ballistic pitch for its launcher's stats; returns whether one exists
bool EEUCProjectile::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theDirection(0.0f, 0.0f, 0.0f);
    EEUnitClassUpgrade* theStats = inUnit->mPlayer->mUnitClassUpgrades[inUnit->mLauncherClass->mUnitType];
    float theAngle;
    bool theResult = EEGetBallisticPitch(inUnit->mOwner, theStats, &thePosition, &inLocation, &theAngle);
    theTransformation.Apply(theUp, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    inUnit->SetFlightTime(0);
    inUnit->mOrientation = GEOrientation(theYaw, theAngle, 0.0f);
    inUnit->SetProjectileStart(thePosition);
    inUnit->SetVelocity(UMCos(theAngle) * theStats->mLaunchSpeed);
    theTransformation.SetOrientationYPR(theYaw, theAngle, 0.0f);
    long theX;
    long theY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    return theResult;
}

// FUNCTION: EXE 0x6199a5
// Answers the projectile triggers: hit unit (attacks it if targetable), flight time over, ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUCProjectile::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC): the target is the attack-unit goal's; a hit unit's attack goal is set here
    EeCombatConditionIn in;
    EECombatFillCondition(in, EE_COMBAT_GOAL_PROJECTILE, inUnit, inEvent, EECombatShotTarget(inUnit));
    return EECombatCondition(in, inUnit, NULL, this);
}

// FUNCTION: EXE 0x619ae9
// Updates a living projectile's trail (when moving) and ripples when the effect detail allows them
void EEUCProjectile::UpdateEffects(EEComplexUnit* inUnit)
{
    if (!(inUnit->mHitPoints > 0))
        return;
    if (this->mTrailEffect != NULL && inUnit->mVelocity != 0.0f && this->mTrailEffect->mDetailLevel <= gEffectDetail)
        this->UpdateTrail(inUnit);
    if (this->mRippleEffect != NULL && this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// FUNCTION: EXE 0x619b4a
// Steps the projectile's physics, effects and sound, counts down rotting, fades it below z 0 in space, adds flight time
void EEUCProjectile::Update(EEComplexUnit* inUnit)
{
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    inUnit->UpdateOrbitals();
    this->UpdateEffects(inUnit);
    this->RefreshSound(inUnit);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionUnitRotting) {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
    } else if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionFadingOutInSpace) {
        GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
        inUnit->GetPosition(thePosition);
        if (thePosition.GetZ() < 0.0f)
            inUnit->SetOpacityChain(1.0f - thePosition.GetZ() * -0.16666667f);
    }
    unsigned long theElapsed = gPhysics->mTickTimeMs;
    if (theElapsed == 0)
        theElapsed = 1;
    inUnit->SetFlightTime(inUnit->mFlightTime + theElapsed);
}

// ICF-FOLDED: EXE 0x556dd8
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCProjectile::AddToRegion(class EEUnit *)
{
}

// ICF-FOLDED: EXE 0x556dd8
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCProjectile::RemoveFromRegion(class EEUnit *)
{
}
