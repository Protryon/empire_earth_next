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
#include "ee/EEUGRust.h"
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
#include "ee/EEUCMissile.h"
#include "ee/EEMiniMap.h"
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
        inUnit->SetGoal(EEUGRust::NewAttackGround(theGround, false), 0, 0);
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
        inUnit->SetGoal(EEUGRust::NewAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x619388
// Handles the projectile's new action: on a hit plays its effects and sound, drops a parachute's cargo or fires, may
// set a target on fire (flaming arrows), dies and leaves remains; retargets a dead target's tile or its container
void EEUCProjectile::ActionChanged(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(1);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs action_changed: the plan's operations on this host (EECombatShotAction); CBSO records the outcome
    EECombatShotAction(EE_COMBAT_SHOT_PROJECTILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_PROJECTILE_ACTION, inUnit, theBefore);
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
    return EECombatCondition(EE_COMBAT_GOAL_PROJECTILE, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
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
    EECombatHostReadScope theScope(2);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs update: the plan's operations on this host; CBSO records the outcome
    EECombatShotUpdate(EE_COMBAT_SHOT_PROJECTILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_PROJECTILE_UPDATE, inUnit, theBefore);
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

// New (port): the host of ee_combat shots.rs's plans (EEUCProjectile/EEUCMissile::ActionChanged and Update,
// EEUCTorpedo::Update): each read the original's expression, each operation its statement, on the unit and its
// class (the original's `this`, cast as the original's member function saw it: virtual calls dispatch as they did).
// The unit's plain fields (command, location, goal kind, type, hit points, tile x, rotting, flight and cloak times) are
// the unit store's (shots.rs StoredShot), never asked here.

namespace {

struct EEShotContext {
    EEComplexUnit* mUnit;
    EEUnitClass* mClass;
    int32_t mKind;                          // EE_COMBAT_SHOT_*
};

uint32_t EEShotFloat(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

float EEShotArg(int32_t inBits)
{
    float theValue;
    memcpy(&theValue, &inBits, 4);
    return theValue;
}

GE3DPoint EEShotLocation(EEComplexUnit* inUnit)
{
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    return GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
}

EEUGRust* EEShotAttack(EEComplexUnit* inUnit)
{
    return static_cast<EEUGRust*>(inUnit->mGoalState);
}

int32_t EEShotOp(void* inContext, int32_t inOp, int32_t inArg, int32_t inArg1)
{
    EEShotContext* theContext = (EEShotContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnitClass* theClass = theContext->mClass;
    bool theMissile = theContext->mKind != EE_COMBAT_SHOT_PROJECTILE;
    switch (inOp) {
    // ---- reads
    case EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME:
        return (int32_t)theClass->mRottingTime;
    case EE_COMBAT_SHOT_R_TILE_HEIGHT:
        return (int32_t)EEShotFloat(theUnit->mTile->GetHeight());
    case EE_COMBAT_SHOT_R_SPLASH_EFFECT:
        return theClass->mSplashEffect != NULL;
    case EE_COMBAT_SHOT_R_DEATH_EFFECT:
        return theClass->mDeathEffect != NULL;
    case EE_COMBAT_SHOT_R_DEATH_SOUND:
        return theUnit->mStats->mDeathSound != NULL;
    case EE_COMBAT_SHOT_R_CARGO:
        return (int32_t)theUnit->GetNumCargoUnits();
    case EE_COMBAT_SHOT_R_LINE_OF_SIGHT:
        return (int32_t)EEShotFloat(theUnit->GetLineOfSight());
    case EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND:
        return (int32_t)theUnit->mComplexUnit->mGoalState->mKind;
    case EE_COMBAT_SHOT_R_FLAMING_SET: {
        EECalamityClass* theCalamity = NULL;
        USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(0x17, theCalamity);
        EEUnit* theTarget = static_cast<EEUGRust*>(theUnit->mComplexUnit->mGoalState)->GetAttackTarget();
        return gUnitSetManager.GetSet(theCalamity->GetUnitSet())->Contains(theTarget->mUnitClass->mUnitType);
    }
    case EE_COMBAT_SHOT_R_RANDOM:
        return (int32_t)EEShotFloat(gPhysics->mRandom.Generate());
    case EE_COMBAT_SHOT_R_FLAMING_CHANCE:
        return (int32_t)EEShotFloat((float)gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mChance);
    case EE_COMBAT_SHOT_R_REMAINS:
        return theClass->mRemainsClass != NULL;
    case EE_COMBAT_SHOT_R_IS_PROJECTILE:
        return theClass->mIsProjectile;
    case EE_COMBAT_SHOT_R_WORLD_CLEARING:
        return gWorldClearing;
    case EE_COMBAT_SHOT_R_FREE_CAMERA:
        return gFreeCamera;
    case EE_COMBAT_SHOT_R_TARGET_HIT_POINTS:
        return (int32_t)EEShotAttack(theUnit)->GetAttackTarget()->mHitPoints;
    case EE_COMBAT_SHOT_R_POPULATION_COST:
        return (int32_t)theUnit->mUnitClass->GetPopulationCost();
    case EE_COMBAT_SHOT_R_PHYSICS_MODEL:
        return theClass->mPhysicsModel != NULL;
    case EE_COMBAT_SHOT_R_TICK:
        return (int32_t)gPhysics->mTickTimeMs;
    case EE_COMBAT_SHOT_R_POSITION_Z: {
        GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
        theUnit->GetPosition(thePosition);
        return (int32_t)EEShotFloat(thePosition.GetZ());
    }
    // ---- operations
    case EE_COMBAT_SHOT_SET_ANIMATION:
        theUnit->SetAnimation(inArg, -1, 0);
        break;
    case EE_COMBAT_SHOT_SET_ROTTING:
        theUnit->SetRottingAwayTime(inArg);
        break;
    case EE_COMBAT_SHOT_UPDATE_ROTTING:
        theUnit->UpdateRottingAwayTime();
        break;
    case EE_COMBAT_SHOT_SPLASH: {
        GE3DPoint thePosition = EEShotLocation(theUnit);
        thePosition.SetZ(0.0f);
        gEffectManager->CreatePlacedEffect(thePosition, theClass->mSplashEffect, NULL);
        break;
    }
    case EE_COMBAT_SHOT_GROUND_LOCATION: {
        GE3DPoint thePosition = EEShotLocation(theUnit);
        GE3DPoint theGround(0.0f, 0.0f, 0.0f);
        ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
        // the location's x, y, z set in place, as the original's const_cast stores did (through the setter, which
        // reports them to the unit store)
        theUnit->mPhysicalModel.SetLocationXYZ(theGround);
        break;
    }
    case EE_COMBAT_SHOT_SAME_LOCATION: {
        GE3DPoint thePosition = EEShotLocation(theUnit);
        theUnit->mPhysicalModel.SetLocationXYZ(thePosition);
        break;
    }
    case EE_COMBAT_SHOT_DEATH_EFFECT:
        gEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0);
        break;
    case EE_COMBAT_SHOT_DEATH_SOUND: {
        float theVolume;
        float thePan;
        float thePitch;
        EESoundObject* theSound = theUnit->mStats->mDeathSound;
        theClass->GetSoundParameters(theUnit, theSound, &theVolume, &thePan, &thePitch);
        gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        break;
    }
    case EE_COMBAT_SHOT_PHYSICS_ACTION:
        theUnit->SetCurrentPhysicsAction((unsigned long)inArg);
        break;
    case EE_COMBAT_SHOT_DROP_PATH:
        theUnit->DropPath();
        break;
    case EE_COMBAT_SHOT_UNLOAD_CARGO:
        theUnit->UnloadCargoAtTile((long)EEShotArg(inArg), (long)EEShotArg(inArg1), false);
        break;
    case EE_COMBAT_SHOT_KILL_CARGO:
        theUnit->KillCargo();
        break;
    case EE_COMBAT_SHOT_FIRE_SHOT:
        if (theMissile)
            static_cast<EEUCMissile*>(theClass)->FireShot(theUnit);
        else
            static_cast<EEUCProjectile*>(theClass)->FireShot(theUnit);
        break;
    case EE_COMBAT_SHOT_UNTINT_TARGET: {
        EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(EEShotAttack(theUnit)->GetAttackTarget());
        if (theTarget != NULL)
            theTarget->SetIsTinted(false);
        break;
    }
    case EE_COMBAT_SHOT_SIGHT_OFF:
        theUnit->SightOffHere();
        break;
    case EE_COMBAT_SHOT_SET_HIT_POINTS:
        theUnit->SetHitPoints(inArg);
        break;
    case EE_COMBAT_SHOT_SIGHT_ON:
        theUnit->SightOnHere();
        break;
    case EE_COMBAT_SHOT_CREATE_CALAMITY:
        USingleton<EECalamityManager>::GetInstance()->CreateUnitCalamity(
            0x18, -1, static_cast<EEUGRust*>(theUnit->mComplexUnit->mGoalState)->GetAttackTarget()->GetUnitID(),
            theUnit->mPlayer->GetPlayerID());
        break;
    case EE_COMBAT_SHOT_START_ACTION:
        theUnit->StartAction((unsigned long)inArg);
        break;
    case EE_COMBAT_SHOT_ACTION_CHANGED:
        theClass->ActionChanged(theUnit);
        break;
    case EE_COMBAT_SHOT_CREATE_REMAINS:
        theClass->CreateRemainsUnit(theUnit);
        break;
    case EE_COMBAT_SHOT_ATTACK_TARGET_TILE: {
        EEUGRust* theGoal = EEShotAttack(theUnit);
        GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
        thePosition.SetX((float)theGoal->GetAttackTarget()->mTileX);
        thePosition.SetY((float)theGoal->GetAttackTarget()->mTileY);
        EEMapTile* theTile = ::gWorld.mMap->GetTile(theGoal->GetAttackTarget()->mTileX, theGoal->GetAttackTarget()->mTileY);
        if (theTile != NULL)
            thePosition.SetZ(theTile->GetHeight());
        theUnit->SetGoal(EEUGRust::NewAttackGround(thePosition, false), 0, 0);
        break;
    }
    case EE_COMBAT_SHOT_RETARGET_CONTAINER: {
        EEUGRust* theGoal = EEShotAttack(theUnit);
        theGoal->SetAttackTarget(theGoal->GetAttackTarget()->mComplexUnit->mContainer);
        break;
    }
    case EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED:
        theClass->HandleActionChanged(theUnit);
        break;
    case EE_COMBAT_SHOT_ATTACK_GOAL_LOCATION: {
        EEUnitGoal* theGoal = theUnit->mGoalState;
        GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
        theGoal->GetGoalLocation(theTarget);
        theUnit->SetGoal(EEUGRust::NewAttackGround(theTarget, theGoal->GetB1C()), 0, 0);
        break;
    }
    case EE_COMBAT_SHOT_POPULATION:
        theUnit->mPlayer->mPopulationCount -= inArg;
        break;
    case EE_COMBAT_SHOT_MINIMAP_OFF:
        gMiniMap->MoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY, -10000, -10000);
        break;
    case EE_COMBAT_SHOT_STEP_PHYSICS:
        theClass->mPhysicsModel->Step(theUnit);
        break;
    case EE_COMBAT_SHOT_ORBITALS:
        theUnit->UpdateOrbitals();
        break;
    case EE_COMBAT_SHOT_EFFECTS:
        if (theMissile)
            static_cast<EEUCMissile*>(theClass)->UpdateEffects(theUnit);
        else
            static_cast<EEUCProjectile*>(theClass)->UpdateEffects(theUnit);
        break;
    case EE_COMBAT_SHOT_REFRESH_SOUND:
        theClass->RefreshSound(theUnit);
        break;
    case EE_COMBAT_SHOT_SET_OPACITY:
        theUnit->SetOpacityChain(EEShotArg(inArg));
        break;
    case EE_COMBAT_SHOT_SET_FLIGHT_TIME:
        theUnit->SetFlightTime(inArg);
        break;
    case EE_COMBAT_SHOT_SET_CLOAKED:
        theUnit->mCloaked = (unsigned long)(long)inArg;
        break;
    case EE_COMBAT_SHOT_WATER_PLANE: {
        GETransformation theTransformation(false);
        theTransformation = theUnit->mPhysicalModel.GetLocation();
        theTransformation.SetZ(0.0f);
        theUnit->SetTransformation(theTransformation, true);
        break;
    }
    }
    return 0;
}

}   // namespace

void EECombatShotAction(int32_t inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEShotContext theContext = {inUnit, inClass, inKind};
    EeCombatShotHost theHost = {&theContext, EEShotOp};
    ee_combat_shot_action_changed(inKind, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), &theHost);
}

void EECombatShotUpdate(int32_t inKind, EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEShotContext theContext = {inUnit, inClass, inKind};
    EeCombatShotHost theHost = {&theContext, EEShotOp};
    ee_combat_shot_update(inKind, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), &theHost);
}
