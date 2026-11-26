// EEUnitClassPeople.cpp (new, R5b.4 round 32): the citizens and priests' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEUGRust.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EERules.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECombatFormulas.h"

// ---- EEUCBasicCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The citizens'
// unit class: gathering (resource and storage searches), building and repairing. 0x20bcf7..0x20d571 (after
// EEUCAmbient.cpp; EEUCBasicPriest.cpp follows).

// the default epoch media (EERules.cpp EEDefaultEpochMedia) name these rows
static_assert(kSoundLightning == 0x176 && kSoundThunder2 == 0x1ba && kGraphicEffectLightningUnit == 0x15 &&
              kGraphicEffectDustEffect == 0xab && kGraphicEffectDisintegrationAreaEffectIntermediate == 0xec,
              "EERules.cpp's epoch media rows");

// 0x243c7d EEBlockTakes: ee/EEAreaUtilities.h

// Researches a tech on a unit (idling it unless only goal kinds 1, 2, 7, 10 are queued); if that kills the unit,
// plays the epoch's lightning or disintegration effects and thunder at its position
// (EETechTree::Research for inUnit)

void EEPlayUnitEffect(EEComplexUnit* inUnit, long inEffect) { ee_units_ucp_call(EE_UNITS_UCP_DO_PLAY_UNIT_EFFECT, NULL, inUnit, 0, inEffect); }  // ucp_people.rs

// The citizens' searches' host (ee_units citizen.rs, citizen_host.rs; EE_UNITS_CZ_*): the units' reads, the noted
// world tests, each as the original called it (the tile rings, the shuffle and the plain reads are Rust's)
// (the plain reads, EE_UNITS_CZ_UNIT and the tiles' and players' lists, are Rust's views: citizen_host.rs)
extern "C" int64_t ee_units_cz_host(int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsCzIo* io)
{
    EEUnit* theUnit = (EEUnit*)(uintptr_t)inA;
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_UNITS_CZ_MAP_SIZE: io->v[0] = gMapWidth; io->v[1] = gMapHeight; return 0;
    case EE_UNITS_CZ_TILE_CONTINENT: return gContinentManager.GetContinent(EETileCoordinate((long)v[0], (long)v[1]), kAIPathLandMass);
    case EE_UNITS_CZ_UNIT_CONTINENT: return gContinentManager.GetContinent(theUnit, kAIPathLandMass);
    case EE_UNITS_CZ_BLOCK_TAKES: return ee_units_uu_unit_block_takes((uint64_t)(uintptr_t)theUnit, (int32_t)v[0], (int32_t)v[1], (int32_t)v[2], (int32_t)v[3], (int32_t)v[3], 0) != 0;
    case EE_UNITS_CZ_CAN_GATHER: return ((EEComplexUnit*)theUnit)->CanGather((EEUnit*)(uintptr_t)inB, (long)v[0], (long)v[1], false, false);
    case EE_UNITS_CZ_CAN_GET_TO_UNIT: return gContinentManager.CanGetToUnit(theUnit, (EEUnit*)(uintptr_t)inB);
    case EE_UNITS_CZ_PATH_AREA: return gContinentManager.GetPathAreaIDAt(EETileLocation((long)v[0], (long)v[1]));
    case EE_UNITS_CZ_CAN_TAKE: return ((EEComplexUnit*)theUnit)->CanTake((EEComplexUnit*)(uintptr_t)inB, (unsigned long)v[0]);
    }
    return 0;
}

// ---- EEUCBasicPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The priests' common
// unit class (conversion): 0x20d571..0x20e2cb (after EEUCBasicCitizen.cpp; EEUCBomb.cpp follows). Unit flags: the
// exe's module set. Order: the conversion cost (static), UpdateConversion (command 0x7d4), ConvertTarget (the convert), Virtual74,
// Virtual78, the can-convert test (static), VirtualA8, VirtualA0, VirtualB4, VirtualB8, then EEComplexUnit's
// helpers that VirtualB8 calls (COMDATs after their first user: inline in the real header? written out of line
// here), ShowConversionEffects, HideConversionEffects.


// Returns the faith a conversion by a unit costs: the world table's entry 46, at most the unit's own power (0 if none)
long EEGetConversionCost(EEComplexUnit* inUnit)
{
    return EECombatConversionCost(EeCombatConversionCostIn{(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost),
                                                           (int32_t)inUnit->mStats->mPower});
}

// Returns whether a priest can convert a target now (enemy, resistance, faith, priests and buildings rules, the
// temple's range); outFlag is set when the target is shielded
// (new: the decision is ee_combat's can_convert over a record of pure reads, gathered here; the damage table is
// read only for a valid ammo column, the shield and the building priest's distance always: none has side effects)
bool EECanConvert(EEComplexUnit* inPriest, EEComplexUnit* inTarget, bool& outFlag) { return ee_units_ucp_can_convert(inPriest, inTarget, &outFlag); }  // ucp_people.rs

// Turns the unit's turrets when its model has parts
void EEComplexUnit::UpdateTurrets()
{
    if (this->GetModel()->GetNumParts() != 0)
        this->mUnitClass->TurnTurrets(this);
}

// ---- EEUCHelicopterPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The priest
// helicopter's unit class: 0x212edf..0x21339d (after EEUCHelicopter.cpp; EEUCHelicopterTransport.cpp follows). One unit
// per EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0x24, +0x2c, +0x6c (EEUCLandPriest's inline twin is ICF-folded into it), +0x74, +0x78, +0xa0 in
// source order.

// New (R5b.4, the fourteenth round): EEUCHelicopterPriest::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPriestPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_uc_rest(EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_PATH, inClass, inUnit, 0, 0, 0, NULL); }  // uc_rest.rs

