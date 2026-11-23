// EEUnitClassPeople.cpp (new, R5b.4 round 32): the citizens and priests' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/SESoundManager.h"

#include "rust/sim/units.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEUGRust.h"
#include "ee/EEAIRandom.h"
#include "ee/EEAIArea.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EERules.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECombatGoals.h"

// ---- EEUCBasicCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The citizens'
// unit class: gathering (resource and storage searches), building and repairing. 0x20bcf7..0x20d571 (after
// EEUCAmbient.cpp; EEUCBasicPriest.cpp follows).

// the default epoch media (EERules.cpp EEDefaultEpochMedia) name these rows
static_assert(kSoundLightning == 0x176 && kSoundThunder2 == 0x1ba && kGraphicEffectLightningUnit == 0x15 &&
              kGraphicEffectDustEffect == 0xab && kGraphicEffectDisintegrationAreaEffectIntermediate == 0xec,
              "EERules.cpp's epoch media rows");

// 0x243c7d EEBlockTakes: ee/EEAreaUtilities.h

// 0x247021: the tiles around a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)

// 0x24697b: the tiles of a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)

// Returns the nearest gatherable resource of a kind within inRange rings of a centre unit, not in ioUnits (tiles
// shuffled per ring; another path area costs 400; a dead animal wins at once; boats look for naval resources)
// (ee_units citizen.rs)
// Finds the next resource of a kind within five times the citizen's sight: around its gather goal's resource first,
// then around the citizen
// Returns where the citizen drops its load: its goal's storage, else a farm's nearest drop site within 20, else the
// owner's nearest built drop site that takes the load (docks and navy yards for boats) (ee_units citizen.rs)
// Researches a tech on a unit (idling it unless only goal kinds 1, 2, 7, 10 are queued); if that kills the unit,
// plays the epoch's lightning or disintegration effects and thunder at its position
// (EETechTree::Research for inUnit)
static const float kUnitEffectSoundFalloff = 4.0f;  // 0x446fd8 (EEComputeSoundParameters's last argument)

void EEPlayUnitEffect(EEComplexUnit* inUnit, long inEffect)
{
    ee_units_ucp_call(EE_UNITS_UCP_DO_PLAY_UNIT_EFFECT, NULL, inUnit, 0, inEffect);   // ucp_people.rs
}

// Runs the work timer (working the gather or repair goal when it expires), then finishes a due unit research
// (instant build: at once); a kind-3 tech tied to the Bundeswehr power goes through EEPlayUnitEffect
// Returns whether the citizen has a gather point beside a resource (not just the resource's own corner tile)
// (ee_units citizen.rs)
// The citizens' searches' host (ee_units citizen.rs, citizen_host.rs; EE_UNITS_CZ_*): the units' reads, the noted
// world tests, each as the original called it (the tile rings, the shuffle and the plain reads are Rust's)
// (the plain reads, EE_UNITS_CZ_UNIT and the tiles' and players' lists, are Rust's views: citizen_host.rs)
extern "C" int64_t ee_units_cz_host(int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsCzIo* io)
{
    EEUnit* theUnit = (EEUnit*)(uintptr_t)inA;
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_UNITS_CZ_MAP_SIZE:
        io->v[0] = gMapWidth;
        io->v[1] = gMapHeight;
        return 0;
    case EE_UNITS_CZ_TILE_CONTINENT:
        return gContinentManager.GetContinent(EETileCoordinate((long)v[0], (long)v[1]), kAIPathLandMass);
    case EE_UNITS_CZ_UNIT_CONTINENT:
        return gContinentManager.GetContinent(theUnit, kAIPathLandMass);
    case EE_UNITS_CZ_BLOCK_TAKES:
        return ee_units_uu_unit_block_takes((uint64_t)(uintptr_t)theUnit, (int32_t)v[0], (int32_t)v[1], (int32_t)v[2], (int32_t)v[3], (int32_t)v[3], 0) != 0;
    case EE_UNITS_CZ_CAN_GATHER:
        return ((EEComplexUnit*)theUnit)->CanGather((EEUnit*)(uintptr_t)inB, (long)v[0], (long)v[1], false, false);
    case EE_UNITS_CZ_CAN_GET_TO_UNIT:
        return gContinentManager.CanGetToUnit(theUnit, (EEUnit*)(uintptr_t)inB);
    case EE_UNITS_CZ_PATH_AREA:
        return gContinentManager.GetPathAreaIDAt(EETileLocation((long)v[0], (long)v[1]));
    case EE_UNITS_CZ_CAN_TAKE:
        return ((EEComplexUnit*)theUnit)->CanTake((EEComplexUnit*)(uintptr_t)inB, (unsigned long)v[0]);
    }
    return 0;
}

// ---- EEUCLandCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land citizen's unit
// class: 0x216c48..0x216d4b (after EEUCLandable.cpp; EEUCLandMilitaryHero.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); VirtualB8,
// then EEComplexUnit::ContinueProduction after its first user.

// Starts the next production when nothing is being produced and the build queue is not empty
void EEComplexUnit::ContinueProduction()
{
    if (this->mProductionTime == 0 && this->GetBuildQueueSize() != 0)
        this->StartProduction();
}

// ---- EEUCNavalCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval citizen's
// unit class (a fishing boat): 0x218490..0x21880c (after EEUCNavalBuilding.cpp; EEUCNavalPriest.cpp's out-of-line
// ctor 0x21880c follows). Every slot of ee/EEUCNavalCitizen.h in that range is here: +0x08 (ICF-shared with
// EEUCNaval/EEUCNavalBuilding/EEUCNavalTransport), +0x0c, +0x30 (ICF-shared), +0x74, +0xa0, +0xb8. The ctor is
// inline (EEUnitClass.cpp's factory). Unit flags: the exe's module set.

// Returns the tile under a location along the camera line, at sea level
// Creates a fishing boat on a tile at sea level
// ---- EEUCBasicPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The priests' common
// unit class (conversion): 0x20d571..0x20e2cb (after EEUCBasicCitizen.cpp; EEUCBomb.cpp follows). Unit flags: the
// exe's module set. Order: the conversion cost (static), UpdateConversion (command 0x7d4), ConvertTarget (the convert), Virtual74,
// Virtual78, the can-convert test (static), VirtualA8, VirtualA0, VirtualB4, VirtualB8, then EEComplexUnit's
// helpers that VirtualB8 calls (COMDATs after their first user: inline in the real header? written out of line
// here), ShowConversionEffects, HideConversionEffects.

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);


static const float kBasicPriestSoundFactor = 4.0f;     // 0x446fe4 (the conversion sound's distance factor)

// Returns the faith a conversion by a unit costs: the world table's entry 46, at most the unit's own power (0 if none)
long EEGetConversionCost(EEComplexUnit* inUnit)
{
    EeCombatConversionCostIn theIn = {(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost),
                                      (int32_t)inUnit->mStats->mPower};
    return EECombatConversionCost(theIn);
}

// Works a conversion with enough faith: stops at an ally or own unit, shows the effects and warns the target's owner,
// then after the minimum time tries once a second (resistance, owner factor, rate) and converts on success
// Converts the target to the priest's player (with area conversion, also every convertible visible unit nearby),
// spends the faith and plays the converted unit's creation sound for the two players involved
// Returns whether a priest can convert a target now (enemy, resistance, faith, priests and buildings rules, the
// temple's range); outFlag is set when the target is shielded
// (new: the decision is ee_combat's can_convert over a record of pure reads, gathered here; the damage table is
// read only for a valid ammo column, the shield and the building priest's distance always: none has side effects)
bool EECanConvert(EEComplexUnit* inPriest, EEComplexUnit* inTarget, bool& outFlag)
{
    return ee_units_ucp_can_convert(inPriest, inTarget, &outFlag);   // ucp_people.rs
}

// Loads the conversion indicator effect and the class's media
// Unloads the conversion indicator effect and the class's media
// Updates each of the unit's orbitals
void EEComplexUnit::UpdateOrbitals()
{
    ee_units_uc_rest(EE_UNITS_UC_DO_R_UPDATE_ORBITALS, NULL, this, 0, 0, 0, NULL);   // uc_rest.rs
}

// Counts the reload timer down by the tick, not below 0
// New (R5b.4 leaf 6): the countdown is Rust over the unit store (ee_units step.rs step_countdown); the write is the
// holder's. TEST ONLY: while the capture runs, USTM records it
void EEComplexUnit::UpdateReloadTimer()
{
    ee_units_ucp_call(EE_UNITS_UCP_DO_RELOAD_TIMER, NULL, this, 0, 0);   // ucp_people.rs
}

// Counts the target search timer down by the tick, not below 0
// New (R5b.4 leaf 6): Rust over the unit store, as UpdateReloadTimer
void EEComplexUnit::UpdateTargetSearchTimer()
{
    ee_units_ucp_call(EE_UNITS_UCP_DO_TARGET_SEARCH_TIMER, NULL, this, 0, 0);   // ucp_people.rs
}

// New (R5b.4 leaf 6): Rust over the unit store (ee_units step.rs step_heal: the same f32 operations in order)
void EEComplexUnit::UpdateHealSearchTimer()
{
    ee_units_ucp_call(EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER, NULL, this, 0, 0);   // ucp_people.rs
}

// New (R5b.4 leaf 6): the rotting countdown of the classes' Updates (kActionUnitRotting; 19 inline copies of it were
// in the classes' Update bodies): the time left minus the tick, not below 0 (Rust over the unit store: ee_units step.rs
// step_rotting), written through the holder; returns the time (the callers that rot linked parts give them the same).
// TEST ONLY: while the capture runs, USTM records it
long EEComplexUnit::UpdateRottingAwayTime()
{
    return (long)ee_units_ucp_call(EE_UNITS_UCP_DO_ROTTING_TIME, NULL, this, 0, 0);   // ucp_people.rs
}

// New (R5b.4 leaf 6): the resources' rotting of the classes' Updates (kActionResourcesRotting; 3 inline copies: the
// animals, the fish, the default): the time plus the tick; past 4 s the resource amount loses one per whole 4 s (not
// below 0) and the time keeps the rest (Rust over the unit store: ee_units step.rs step_resources_rotting). TEST ONLY:
// while the capture runs, USTM records it
void EEComplexUnit::UpdateResourcesRotting()
{
    ee_units_ucp_call(EE_UNITS_UCP_DO_RESOURCES_ROTTING, NULL, this, 0, 0);   // ucp_people.rs
}

// Turns the unit's turrets when its model has parts
void EEComplexUnit::UpdateTurrets()
{
    if (this->GetModel()->GetNumParts() != 0)
        this->mUnitClass->TurnTurrets(this);
}

// For a temple not already showing them, starts the tower swirl and smoke effects and plays the tower convert sound
// For a temple, destroys the tower swirl and smoke effects
// ---- EEUCLandPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land priest's unit
// class: 0x216d57..0x216de3 (EEUCLandTransport.cpp's slots follow; the unit map's 0x216d57..0x21880c block is one
// unit per class in alphabetical order: LandPriest, LandTransport, MiningBuilding, Missile, Naval, NavalBuilding,
// NavalCitizen). Unit flags: the exe's module set. Its ctor is followed by EEUnitClass's inline +0x24/+0x68/+0x6c
// (every unit class's vtable points at these copies), then its own slots. vtable 0x447060.

// Gets the sound parameters the common way; silent when the priest is not seen
// ---- EEUCNavalPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval priest's
// unit class: 0x21880c..0x218a45 (after EEUCNavalCitizen's slots; EEUCNavalTransport.cpp's +0x04 0x218a45 follows).
// Unit flags: the exe's module set. Its ctor is out of line (vtable 0x44724c), followed by the ??_G every priest class
// shares (EEUCBasicPriest's inline dtor), then its own slots +0x0c, +0x34, +0xa0.

// Creates a priest ship at a placement on the water plane (z 0)
// ---- EEUCHelicopterPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The priest
// helicopter's unit class: 0x212edf..0x21339d (after EEUCHelicopter.cpp; EEUCHelicopterTransport.cpp follows). One unit
// per EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0x24, +0x2c, +0x6c (EEUCLandPriest's inline twin is ICF-folded into it), +0x74, +0x78, +0xa0 in
// source order.

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way; 0x249560 / 0x2495f5: ee/EEUnitUtilities.h)
// (provisional name)

// Gets the sound parameters with a Doppler pitch; silent while the helicopter is on the map but not seen
// New (R5b.4, the fourteenth round): EEUCHelicopterPriest::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPriestPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    ee_units_uc_rest(EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_PATH, inClass, inUnit, 0, 0, 0, NULL);   // uc_rest.rs
}

// Sets up a new priest helicopter as a priest, then lifts it to its flying height
// the people's scratch (ucp_people.rs): the effect handle, the Bundeswehr power, the area conversion's units
static EEEffectHandle sUcpHandle;
static EECivPower sUcpPower;
static std::vector<EEUnit*> sUcpUnits;

template <class T> static T* EEUcpObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// EECanConvert's reads of the priest and the target (ucp_people.rs EE_UNITS_UCP_CC_*; floats in ioF[0])
static int64_t EEUcpCanConvertRead(EEComplexUnit* inPriest, EEComplexUnit* inTarget, int64_t inWhich, float* ioF)
{
    switch (inWhich) {
    case EE_UNITS_UCP_CC_CRUSADER: return inPriest->mUnitClass->IsCrusader();
    case EE_UNITS_UCP_CC_ADAPTATION: return gCivPowersManager.HasPower(inPriest->GetPlayer(), kCivPowerAdaptation);
    case EE_UNITS_UCP_CC_SAME_PLAYER: return inTarget->mPlayer == inPriest->mPlayer;
    case EE_UNITS_UCP_CC_ENEMY: return inPriest->mPlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] != 0;
    case EE_UNITS_UCP_CC_CONVERSION_RATE: ioF[0] = inPriest->mStats->mConversionRate; return 0;
    case EE_UNITS_UCP_CC_TARGET_DISABLED: return inTarget->mIsDisabled;
    case EE_UNITS_UCP_CC_TARGET_PROTECTED: return inTarget->IsProtected();
    case EE_UNITS_UCP_CC_RESISTANCE: ioF[0] = inTarget->mStats->mConversionResistance; return 0;
    case EE_UNITS_UCP_CC_TARGET_TYPE: return inTarget->mUnitClass->mUnitType;
    case EE_UNITS_UCP_CC_TARGET_HIT_POINTS: return (int32_t)inTarget->mHitPoints;
    case EE_UNITS_UCP_CC_ATTACKABLE: return inTarget->mUnitClass->CanBeAttacked();
    case EE_UNITS_UCP_CC_AMMO_TYPE: return inPriest->mUnitClass->mAmmoType;
    case EE_UNITS_UCP_CC_DAMAGE: ioF[0] = EEDamageFactor(inTarget->mUnitClass->mFamily, inPriest->mUnitClass->mAmmoType); return 0;
    case EE_UNITS_UCP_CC_TARGET_CATEGORY: return inTarget->mUnitClass->mCategory;
    case EE_UNITS_UCP_CC_PRIEST_CATEGORY: return inPriest->mUnitClass->mCategory;
    case EE_UNITS_UCP_CC_CONVERTS_PRIESTS: return inPriest->mStats->mConvertsPriests;
    case EE_UNITS_UCP_CC_POWER: return (int32_t)inPriest->GetPower();
    case EE_UNITS_UCP_CC_CONVERTS_BUILDINGS: return inPriest->mStats->mConvertsBuildings;
    case EE_UNITS_UCP_CC_TARGET_LAND_BUILDING: return inTarget->mUnitClass->IsLandBuilding();
    case EE_UNITS_UCP_CC_EPOCH: return inPriest->mPlayer->mTechnologies->mCurrentEpoch;
    case EE_UNITS_UCP_CC_RANGE: ioF[0] = inPriest->mStats->mRange; return 0;
    case EE_UNITS_UCP_CC_PRIEST_CYBER: return inPriest->mUnitClass->mIsCyber;
    case EE_UNITS_UCP_CC_TARGET_CYBER: return inTarget->mUnitClass->mIsCyber;
    case EE_UNITS_UCP_CC_SHIELDED: return inTarget->HasEffect(4);
    }
    return 0;
}

// the step timers by ucp_people.rs's index (0 reload, 1 target search, 2 rotting, 3 resource amount, 4 heal search)
static int64_t EEUcpTimer(EEComplexUnit* inUnit, int64_t inWhich, float* ioF)
{
    switch (inWhich) {
    case 0: return inUnit->mReloadTimer;
    case 1: return inUnit->mTargetSearchTimer;
    case 2: return inUnit->mRottingAwayTime;
    case 3: return inUnit->mResourceAmount;
    case 4: ioF[0] = inUnit->mHealSearchTimer; return 0;
    }
    return 0;
}

static void EEUcpSetTimer(EEComplexUnit* inUnit, int64_t inWhich, int64_t inValue, float* ioF)
{
    switch (inWhich) {
    case 0: inUnit->SetReloadTimer((long)inValue); break;
    case 1: inUnit->SetTargetSearchTimer((long)inValue); break;
    case 2: inUnit->SetRottingAwayTime((long)inValue); break;
    case 3: inUnit->SetResourceAmount((long)inValue); break;
    case 4: inUnit->SetHealSearchTimer(ioF[0]); break;
    }
}

// The C++ of the people's Rust bodies (ee_units ucp_people.rs; EE_UNITS_UCP_*): one statement of the original bodies
// each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucp_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcpObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCP_QUEUE0_LEN: return theUnit->mGoalQueues[0].mGoals.size();
    case EE_UNITS_UCP_QUEUE0_KIND: return (unsigned char)(*(theUnit->mGoalQueues.begin() + 0)).mGoals[(size_t)inB]->mKind;
    case EE_UNITS_UCP_CLEAR_RESOURCE_AMOUNT: theUnit->SetResourceAmount(0); return 0;
    case EE_UNITS_UCP_RESEARCH: theUnit->mPlayer->mTechnologies->Research((long)inB, theUnit); return 0;
    case EE_UNITS_UCP_REMOVAL_SOUND: return EEObj(EESoundObject::Get(EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalSound));
    case EE_UNITS_UCP_REMOVAL_EFFECT_COUNT: return EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalEffects.size();
    case EE_UNITS_UCP_REMOVAL_EFFECT: gWorld.GetEffectManager()->CreatePlacedEffect(GE3DPoint(f[0], f[1], f[2]), EEGraphicEffect::Get(EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalEffects[(size_t)inB]), &sUcpHandle); return 0;
    case EE_UNITS_UCP_SOUND_AT: EEComputeSoundParameters(1.0f, GE3DPoint(f[0], f[1], f[2]), f[0], f[1], f[2], kUnitEffectSoundFalloff); return 0;
    case EE_UNITS_UCP_REMOVAL_PITCH_OFFSET: f[0] = EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalPitchOffset; return 0;
    case EE_UNITS_UCP_SOUND_PLAY: EEUcpObject<EESoundObject>(inA)->Play(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UCP_THUNDER_EVERY: return EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalThunderEvery;
    case EE_UNITS_UCP_THUNDER_SOUND: return EEObj(EESoundObject::Get(EEGetRules().Epoch(theUnit->mPlayer->mTechnologies->mCurrentEpoch).mRemovalThunder));
    case EE_UNITS_UCP_RESET_HANDLE: sUcpHandle.mValue = 0; return 0;
    case EE_UNITS_UCP_WORK_TIMER: return theUnit->mWorkTimer;
    case EE_UNITS_UCP_SET_WORK_TIMER: theUnit->SetWorkTimer((long)inB); return 0;
    case EE_UNITS_UCP_GOAL_WORK: theUnit->mGoalState->Work(theUnit); return 0;
    case EE_UNITS_UCP_UPDATE_FANATICISM: theUnit->UpdateFanaticismColor(); return 0;
    case EE_UNITS_UCP_CURRENT_RESEARCH: return theUnit->mCurrentResearch;
    case EE_UNITS_UCP_RESEARCH_NODE: return EEObj(theUnit->mPlayer->mTechnologies->Get(theUnit->mCurrentResearch));
    case EE_UNITS_UCP_NODE_MAKES_UNIT: return EEUcpObject<EETechTreeNode>(inA)->mData->mMakesUnit;
    case EE_UNITS_UCP_NODE_KIND: return EEUcpObject<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_UCP_GET_BUNDESWEHR: return gCivPowersManager.GetPower(kCivPowerBundeswehr, sUcpPower);
    case EE_UNITS_UCP_POWER_IS_PREREQUISITE: return sUcpPower.mTechnology == EEUcpObject<EETechTreeNode>(inA)->mData->mPrerequisite1;
    case EE_UNITS_UCP_POWER: return theUnit->GetPower();
    case EE_UNITS_UCP_CONVERSION_COST: return EEGetConversionCost(theOther);
    case EE_UNITS_UCP_CONVERSION_TARGET: return EEObj(static_cast<EEComplexUnit*>(EEAttackUnitCast(theUnit->GetGoal())->GetAttackTarget()));
    case EE_UNITS_UCP_SAME_PLAYER: return theOther->mPlayer == theUnit->mPlayer;
    case EE_UNITS_UCP_ENEMY_STATE: return theUnit->mPlayer->mEnemies[theOther->mPlayer->GetPlayerID()];
    case EE_UNITS_UCP_RELEASE_CONVERSION: theUnit->ReleaseConversionEffect(); return 0;
    case EE_UNITS_UCP_CLEAR_GOALS: theUnit->ClearGoals(0); return 0;
    case EE_UNITS_UCP_NO_CONVERSION_EFFECT: return UIsNone32(theUnit->mConversionEffect.mValue);
    case EE_UNITS_UCP_CREATE_INDICATOR: return gWorld.GetEffectManager()->CreateUnitEffect(theOther, EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator), &sUcpHandle, 0, 0, 0);
    case EE_UNITS_UCP_HANDLE_DIFFERS: return theUnit->mConversionEffect != sUcpHandle;
    case EE_UNITS_UCP_KEEP_HANDLE: theUnit->mConversionEffect = sUcpHandle; return 0;
    case EE_UNITS_UCP_CONVERSION_EVENT: theOther->mPlayer->mEvents.AddPlayerEvent(EETileLocation(theOther->mTileX, theOther->mTileY), EETileLocation(theUnit->mTileX, theUnit->mTileY), 1, theOther->GetPlayer(), -1, false); return 0;
    case EE_UNITS_UCP_TOO_SOON: return gPhysics->mGameTime - theUnit->mConversionStartTime < (unsigned long)theClass->mMinConversionTime;
    case EE_UNITS_UCP_WORK_TIMER_INT: return (int32_t)theUnit->mWorkTimer;
    case EE_UNITS_UCP_RESISTANCE: f[0] = theOther->mStats->mConversionResistance; return 0;
    case EE_UNITS_UCP_HAS_ADAPTATION: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerAdaptation);
    case EE_UNITS_UCP_IS_CRUSADER: return theUnit->mUnitClass->IsCrusader();
    case EE_UNITS_UCP_UNIT_CATEGORY: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_UCP_TYPE_OF: return theOther->mUnitClass->mUnitType;
    case EE_UNITS_UCP_CONVERSION_RATE: f[0] = theUnit->mStats->mConversionRate; return 0;
    case EE_UNITS_UCP_CONVERSION_FACTOR: f[0] = theOther->mPlayer->mConversionFactor; return 0;
    case EE_UNITS_UCP_CONVERSION: *EEUcpObject<EeCombatConversionOut>((uint64_t)inB) = EECombatConversion(*EEUcpObject<EeCombatConversionIn>(inA)); return 0;
    case EE_UNITS_UCP_RANDOM: f[0] = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_UCP_DISABLED_OF: return theOther->GetComplexUnit()->mIsDisabled;
    case EE_UNITS_UCP_PROTECTED_OF: return theOther->GetComplexUnit()->IsProtected();
    case EE_UNITS_UCP_PLAYER_OF: return EEObj(theOther->mPlayer);
    case EE_UNITS_UCP_CONVERT_UNIT: return EEObj(EEConvertUnit(theOther, theUnit->mPlayer->GetPlayerID(), 1));
    case EE_UNITS_UCP_STATS_OF: return EEObj(theOther->mStats);
    case EE_UNITS_UCP_AREA_CONVERSION: return theUnit->mPlayer->mAreaConversion == true;
    case EE_UNITS_UCP_UNITS_NEAR: ee_units_uu_units_near((const float(*)[3])f, gWorldTable.GetFloatValue(kWorldAreaConversionRadius), &sUcpUnits); return 0;
    case EE_UNITS_UCP_UNITS_AT: return EEObj(sUcpUnits[(size_t)inB]);
    case EE_UNITS_UCP_TILE_SEEN: return ((EEMapTile*)EEUcpObject<EEUnit>(inA)->mTile)->IsSeenBy(theUnit->GetPlayer()) == true;
    case EE_UNITS_UCP_SPEND_FAITH: theUnit->SetPower(theUnit->mPower - EEGetConversionCost(theUnit)); return 0;
    case EE_UNITS_UCP_CONVERSION_START: return theUnit->mConversionStartTime;
    case EE_UNITS_UCP_CLEAR_CONVERSION_START: theUnit->mConversionStartTime = 0; return 0;
    case EE_UNITS_UCP_STATS_CREATION_SOUND: return EEObj(EEUcpObject<EEUnitClassUpgrade>(inA)->mCreationSound);
    case EE_UNITS_UCP_UNIT_PLAYER_CURRENT: return theUnit->mPlayer == ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
    case EE_UNITS_UCP_PLAYER_CURRENT: return EEUcpObject<EEPlayer>(inA) == ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
    case EE_UNITS_UCP_PLAY_FULL: gSoundManager->Play(EEUcpObject<EESoundObject>(inA)->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL); return 0;
    case EE_UNITS_UCP_CLASS_CATEGORY: return theClass->mCategory;
    case EE_UNITS_UCP_CLOAKED: return theUnit->mCloaked;
    case EE_UNITS_UCP_CREATE_EFFECT: return gWorld.GetEffectManager()->CreateUnitEffect(theUnit, EEGraphicEffect::Get((long)inB), &sUcpHandle, 0, 0, 0);
    case EE_UNITS_UCP_CLOAKED_HANDLE: theUnit->mCloaked = sUcpHandle.mValue; return 0;
    case EE_UNITS_UCP_GROW_HANDLE: theUnit->SetGrowTime(sUcpHandle.mValue); return 0;
    case EE_UNITS_UCP_TOWER_SOUND_AT: EEComputeSoundParameters(EESoundObject::Get(kSoundPriestTowerConvert)->mVolume, GE3DPoint(f[0], f[1], f[2]), f[0], f[1], f[2], kBasicPriestSoundFactor); return 0;
    case EE_UNITS_UCP_PLAY_TOWER_SOUND: gSoundManager->Play(EESoundObject::Get(kSoundPriestTowerConvert)->mSampleID, f[0], f[1], f[2], false, NULL); return 0;
    case EE_UNITS_UCP_HAS_CLOAKED_EFFECT: return (long)theUnit->mCloaked > 0;
    case EE_UNITS_UCP_DESTROY_CLOAKED_EFFECT: gWorld.GetEffectManager()->DestroyEffect(EEEffectHandle{(uint32_t)theUnit->mCloaked}); return 0;
    case EE_UNITS_UCP_CLEAR_CLOAKED: theUnit->mCloaked = 0; return 0;
    case EE_UNITS_UCP_HAS_GROW_EFFECT: return theUnit->mGrowTime > 0;
    case EE_UNITS_UCP_DESTROY_GROW_EFFECT: gWorld.GetEffectManager()->DestroyEffect(EEEffectHandle{(uint32_t)theUnit->mGrowTime}); return 0;
    case EE_UNITS_UCP_CLEAR_GROW_TIME: theUnit->SetGrowTime(0); return 0;
    case EE_UNITS_UCP_CAN_CONVERT_READ: return EEUcpCanConvertRead(theUnit, theOther, inB, f);
    case EE_UNITS_UCP_CAN_CONVERT_CAPTURE: EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'N'), *EEUcpObject<EeCombatCanConvertIn>(inA), *EEUcpObject<EeCombatCanConvertOut>((uint64_t)inB)); return 0;
    case EE_UNITS_UCP_TIMER: return EEUcpTimer(theUnit, inB, f);
    case EE_UNITS_UCP_SET_TIMER: EEUcpSetTimer(theUnit, inB, inC, f); return 0;
    case EE_UNITS_UCP_CAPTURE_ACTIVE: return EESimCaptureRecord::Active();
    case EE_UNITS_UCP_TICK_TIME: f[0] = gPhysics->mTickTime; return 0;
    case EE_UNITS_UCP_UNITS_CLEAR: sUcpUnits.clear(); return 0;
    case EE_UNITS_UCP_UNITS_LEN: return sUcpUnits.size();
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
