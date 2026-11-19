// EEUnitClassShots.cpp (new, R5b.4 round 32): the shots' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
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
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMiniMap.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"

// ---- EEUCProjectile.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The projectile's unit
// class (EEUCBomb derives from it and shares these slots by ICF): 0x219143..0x219c49 (after EEUCPlanetSpaceship.cpp;
// EEUCResource.cpp's +0x7c 0x219c49 follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe0, +0xe4, +0x74, +0x78, +0xac, +0xa0, +0xbc, +0xb8 in source order.

// 0x447338 (3.0, the launch height below which a projectile may hit the ground at once) is ee_combat's (impact.rs)
// 0x447050: the slow-down per unit of depth below the ground

// 0x241a6a (cdecl): the launch angle from inOwner at inLocation for the ammunition (provisional name)
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo, float* outAngle);

// Returns whether a projectile hit the ground (bombs, low launches, after 1 s of flight): attacks the ground there
// unless it came down on its target building or wall, and leaves a shell hole on land
bool EEUnitClass::ProjectileCheckGroundImpact(EEComplexUnit* inUnit)
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

// Returns whether a bomb, or a projectile after 1 s of flight, hit water it cannot cross; if so, it attacks the ground
bool EEUnitClass::ProjectileCheckWaterImpact(EEComplexUnit* inUnit)
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

// Handles the projectile's new action: on a hit plays its effects and sound, drops a parachute's cargo or fires, may
// set a target on fire (flaming arrows), dies and leaves remains; retargets a dead target's tile or its container
void EEUnitClass::ProjectileActionChanged(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(1);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs action_changed: the plan's operations on this host (EECombatShotAction); CBSO records the outcome
    EECombatShotAction(EE_COMBAT_SHOT_PROJECTILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_PROJECTILE_ACTION, inUnit, theBefore);
}

// Sets up a new projectile: records its start point, sets it to full speed and starts it advancing
void EEUnitClass::ProjectileUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    inUnit->SetVelocity(inUnit->mStats->GetMaxVelocity());
    inUnit->SetAnimation(0xd, -1, 0);
    inUnit->StartAction(kActionAdvance);
    this->ActionChanged(inUnit);
}

// Launches the projectile toward a location at the ballistic pitch for its launcher's stats; returns whether one exists
bool EEUnitClass::ProjectileLaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// Updates a living projectile's trail (when moving) and ripples when the effect detail allows them
void EEUnitClass::ProjectileUpdateEffects(EEComplexUnit* inUnit)
{
    if (!(inUnit->mHitPoints > 0))
        return;
    if (this->mTrailEffect != NULL && inUnit->mVelocity != 0.0f && this->mTrailEffect->mDetailLevel <= gEffectDetail)
        this->UpdateTrail(inUnit);
    if (this->mRippleEffect != NULL && this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// Steps the projectile's physics, effects and sound, counts down rotting, fades it below z 0 in space, adds flight time
void EEUnitClass::ProjectileUpdate(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(2);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs update: the plan's operations on this host; CBSO records the outcome
    EECombatShotUpdate(EE_COMBAT_SHOT_PROJECTILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_PROJECTILE_UPDATE, inUnit, theBefore);
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
        theClass->FireShot(theUnit);
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
        theClass->UpdateEffects(theUnit);
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

// ---- EEUCBomb.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The bomb's unit class:
// 0x20e2cb..0x20e58d (after EEUCBasicPriest.cpp; EEUCBuilding.cpp follows). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory). Order: VirtualAC (the drop), VirtualB8 with its function-static colour's
// empty atexit dtor, then EEUnitClass::LaunchStoredMissile (the carrier drops its first bomb; EEUnitClass::FireShot calls it).

// Drops the bomb from its carrier: aims it along the carrier's heading, pitched down by the launcher's tilt (0 to
// -90 degrees), with the launch speed's horizontal part; always returns true
bool EEUnitClass::BombLaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    float theSpeed = inUnit->mPlayer->GetUpgrade(inUnit->mLauncherClass->mUnitType)->mLaunchSpeed;
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float theTilt = inUnit->mLauncherClass->mLaunchPitch;
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f + theTilt;
    if (thePitch > 0.0f)
        thePitch = 0.0f;
    else if (thePitch < -1.5707964f)
        thePitch = -1.5707964f;
    float theVelocity = UMCos(thePitch) * theSpeed;
    inUnit->SetFlightTime(0);
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
    inUnit->SetProjectileStart(thePosition);
    inUnit->SetVelocity(theVelocity);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    long theX;
    long theY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    return true;
}

// Flies the bomb; an SAS explosive (unit type 0x305) makes its target blink red every half second while it attacks
void EEUnitClass::BombUpdate(EEComplexUnit* inUnit)
{
    static GEColor sColor(0.9f, 0.0f, 0.2f);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeBombForSASExplosiveExpert && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
        EEUnit* theUnit = static_cast<EEUGRust*>(inUnit->mGoalState)->GetAttackTarget();
        EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(theUnit);
        if (theTarget != NULL) {
            unsigned long theTime = inUnit->mFlightTime % 1000;
            if (theTime > 500) {
                theTarget->SetIsTinted(true);
                theTarget->SetColor(sColor);
            } else
                theTarget->SetIsTinted(false);
        }
    }
    this->ProjectileUpdate(inUnit);
}

// Has a carrier drop its first cargo unit (a bomb) at its target and set it moving
void EEUnitClass::LaunchStoredMissile(EEComplexUnit* inUnit)
{
    if (!(inUnit->GetNumCargoUnits() > 0))
        return;
    GE3DPoint theLocation;
    EEUnit* theTarget;
    EEFindDropLocation(inUnit, &theTarget, &theLocation, NULL);
    EEComplexUnit* theCargo = inUnit->mCargo[0];
    inUnit->UnloadCargoNow(theCargo);
    this->LaunchShot(inUnit, theTarget, theLocation, theCargo);
    theCargo->SetAnimation(0xd, -1, 0);
    theCargo->StartAction(kActionAdvance);
    theCargo->mUnitClass->ActionChanged(theCargo);
}

// ---- EEUCMissile.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The missile's unit
// class: 0x217337..0x217c6d (after EEUCMiningBuilding.cpp; EEUCNaval.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (vtable 0x448cf8); +0x74/+0x78/+0xac are also EEUCTorpedo's (ICF).

// The pooled words 0x447230/0x447234 (0.3, the height over a target the missile is not on; 2.25, its squared
// proximity to an attack target) are ee_combat's (impact.rs)

// The original first argument reads only the base tile. Its sole ComplexUnit caller does not uniquely
// recover narrower pointer specificity; this declaration records the minimum proven contract.
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget);

// Returns whether the missile hit the ground after its flight time (0.3 higher over its target): sets it down there
// and, for an attack-ground goal, leaves a shell hole and returns true (never for a missile base's missiles)
bool EEUnitClass::MissileCheckGroundImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's missile_ground (impact.rs); the placement, its tile and the shell hole stay here
    long theType = inUnit->mLauncherClass->mUnitType;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_MISSILE_GROUND;
    in.flight_time = (int32_t)inUnit->mFlightTime;
    in.has_goal = theGoal != NULL;
    in.goal_kind = theGoal != NULL ? (int32_t)theGoal->mKind : 0;
    in.launcher_cp_base = theType == kUnitTypeCpMissileBase;
    in.launcher_missile_base = theType == kUnitTypeMissileBase;
    in.shell_hole = inUnit->mUnitClass->mShellHoleEffect != NULL;
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.place) {
        long theX;
        long theY;
        inUnit->PlaceOnGround(thePosition);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetZ(theTransformation.GetZ() + theOut.offset);
        EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    }
    if (theOut.decal)
        EEPlaceImpactDecal(inUnit, thePosition);
    return theOut.result != 0;
}

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
                    while (true) {
                        if (!(theY < theEndY && !theResult))
                            break;
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

// Returns whether the missile hit water (z 0 or below on terrain it cannot cross); if so, it attacks the ground there
bool EEUnitClass::MissileCheckWaterImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's missile_water (impact.rs)
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation());
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_MISSILE_WATER;
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_WATER)
        inUnit->SetGoal(EEUGRust::NewAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// The impact tests' lazy reads (ee/EECombatFormulas.h's EECombatImpact): the unit, the class whose terrain test runs
namespace {

struct EEImpactContext {
    EEUnitClass* mClass;
    EEComplexUnit* mUnit;
};

float EEImpactGroundZ(void* inContext, float inX, float inY, float inZ)
{
    GE3DPoint thePoint(inX, inY, inZ);
    GE3DPoint theGround;
    ::gWorld.mMap->GetGroundPoint(thePoint, theGround);
    return theGround.GetZ();
}

float EEImpactTileHeight(void* inContext)
{
    return ((EEImpactContext*)inContext)->mUnit->mTile->GetHeight();
}

void EEImpactTarget(void* inContext, EeCombatImpactTarget* outTarget)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    EEUnit* theTarget = static_cast<EEUGRust*>(theUnit->mGoalState)->GetAttackTarget();
    EEUnitClass* theClass = theTarget->GetUnitClass();
    outTarget->building = theClass->IsLandBuilding() || theClass->mIsWall;
    outTarget->tile[0] = (int32_t)theTarget->GetTileX();
    outTarget->tile[1] = (int32_t)theTarget->GetTileY();
    outTarget->size = (int32_t)theClass->mTileSize;
    outTarget->unit_tile[0] = (int32_t)theUnit->GetTileX();
    outTarget->unit_tile[1] = (int32_t)theUnit->GetTileY();
}

void EEImpactGoalLocation(void* inContext, float* outPoint)
{
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    ((EEImpactContext*)inContext)->mUnit->mGoalState->GetGoalLocation(theLocation);
    outPoint[0] = theLocation.GetX();
    outPoint[1] = theLocation.GetY();
    outPoint[2] = theLocation.GetZ();
}

int32_t EEImpactOnTargetTiles(void* inContext)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    return EEIsOnTargetTiles(theUnit, static_cast<EEUGRust*>(theUnit->GetGoal())->GetAttackTarget());
}

int32_t EEImpactSameTile(void* inContext, float inX, float inY)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    return ::gWorld.mMap->GetTile(UMFloatToLong(inX), UMFloatToLong(inY)) == theUnit->mTile;
}

int32_t EEImpactCanMove(void* inContext)
{
    EEImpactContext* theContext = (EEImpactContext*)inContext;
    return theContext->mClass->CanMoveOnTerrain(theContext->mUnit, theContext->mUnit->mTile->mTerrainType);
}

}   // namespace

// New (port): ee_combat's impact test over the unit (ee/EECombatFormulas.h); captured as CBGI or CBWI
EeCombatImpactOut EECombatImpact(EEUnitClass* inClass, EEComplexUnit* inUnit, EeCombatImpactIn& in)
{
    EEImpactContext theContext = {inClass, inUnit};
    EeCombatImpactHost theHost = {&theContext,          EEImpactGroundZ,       EEImpactTileHeight, EEImpactTarget,
                                  EEImpactGoalLocation, EEImpactOnTargetTiles, EEImpactSameTile,   EEImpactCanMove};
    EeCombatImpactOut theOut = ee_combat_impact(&in, &theHost);
    bool theWater = in.kind == EE_COMBAT_IMPACT_PROJECTILE_WATER || in.kind == EE_COMBAT_IMPACT_MISSILE_WATER;
    EECombatCapture(theWater ? EE_COMBAT_TAG('C', 'B', 'W', 'I') : EE_COMBAT_TAG('C', 'B', 'G', 'I'), in, theOut);
    return theOut;
}

// Handles the missile's new action: on a hit plays the splash or death effect and sound, fires its shot and dies
// (attack-unit goals, ICBM ground strikes); switches to attack ground; on retarget follows the target's container
void EEUnitClass::MissileActionChanged(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(1);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs action_changed over EEUnitClass.cpp's host; CBSO records the outcome
    EECombatShotAction(EE_COMBAT_SHOT_MISSILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_MISSILE_ACTION, inUnit, theBefore);
}

// Sets up a new missile: records its start point and stops it; all but the ICBM start the flight animation and advance
void EEUnitClass::MissileUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    inUnit->SetVelocity(0.0f);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeCpICBM)
        return;
    inUnit->SetAnimation(0xd, -1, 0);
    inUnit->StartAction(kActionAdvance);
    this->ActionChanged(inUnit);
}

// Launches the missile: tilts it to its launcher's launch pitch and starts the launch delay and a 300 s lifetime; true
bool EEUnitClass::MissileLaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
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

// Answers the missile triggers: hit unit (attacks it; also true when its lifetime ran out), ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUnitClass::MissileEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC): the target is the attack-unit goal's; a hit unit's attack goal is set here
    return EECombatCondition(EE_COMBAT_GOAL_MISSILE, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
}

// Steps the missile's physics, effects and sound, and counts down its rotting time, launch delay and lifetime
void EEUnitClass::MissileUpdate(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(2);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs update over EEUnitClass.cpp's host; CBSO records the outcome
    EECombatShotUpdate(EE_COMBAT_SHOT_MISSILE, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_MISSILE_UPDATE, inUnit, theBefore);
}

// ---- EEUCTorpedo.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The torpedo's unit class:
// 0x21b163..0x21b27a (after EEUCSubmergedNaval.cpp; EEUCTransport.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline; +0xe4 is ICF-folded (0x29059d: false).

// When the torpedo has reached ground at or above sea level, gives it an attack-ground goal there and returns
// true
bool EEUnitClass::TorpedoCheckGroundImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's torpedo_ground (impact.rs)
    GE3DPoint thePosition;
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_TORPEDO_GROUND;
    inUnit->GetPosition(thePosition);
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_GROUND)
        inUnit->SetGoal(EEUGRust::NewAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// Flies the torpedo as a missile and keeps it from rising above the water plane (height 0)
void EEUnitClass::TorpedoUpdate(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(2);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs update (the missile's, then the water plane) over EEUnitClass.cpp's host; CBSO records the outcome
    EECombatShotUpdate(EE_COMBAT_SHOT_TORPEDO, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_TORPEDO_UPDATE, inUnit, theBefore);
}

