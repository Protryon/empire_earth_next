// C:\EEX\Empire Earth\Units\EEComplexUnit.cpp (file name inferred: no __FILE__; the keys "EEComplexUnit-*").
// The complex (mobile, goal-driven) unit: 0x1f57d3..0x2010fd. Unit flags: the exe's module set. Data: a GEColor
// 0x963228, the keys 0x963218..0x9631d0 and the pool 0x963198 (one guard byte 0x963210). Contains the helper
// classes EEUnitAreaEffectList, EEOrbital, EEMReplantFarms; the unit's template COMDATs close it.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/GEModel.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EEPlayer.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEAEManager.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEMapDimensions.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EEWheel.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPathManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUnitBehavior.h"

// chain at 0x5f57d3 (wrapper)
//   object 0x963228  ctor None  dtor None
//   inline ctor, fields (offset=value, declaration order unknown): +0=0.2, +4=0.2, +8=1
//   dtor stub is a bare `ret`: the class has a user-declared empty inline destructor
//   object bound as _kTimeWarpColor
// the colour of a time-warped unit (SetDisabled 0x1fe5b3: SetColor when +0x358 is 0x1a/0x1c) (provisional name)
const GEColor kTimeWarpColor(0.2f, 0.2f, 1.0f);

// chain at 0x5f5954 (wrapper)
//   object 0x963198  ctor ??0?$UMemoryPool@VEEComplexUnit@@@@QAE@XZ  dtor ?Release@?$UMemoryPool@VEEComplexUnit@@@@QAE_NXZ
//   guarded dtor (byte ptr [0x963210], 0x20): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
//   object bound as ?sMemoryPool@EEComplexUnit@@2V?$UMemoryPool@VEEComplexUnit@@@@A
UMemoryPool<EEComplexUnit> EEComplexUnit::sMemoryPool;

#include "ee/EEStringIDs.h"
#include "ee/EESoundObject.h"
#include "lle/UResourceFile.h"
// Original cdecl sight-map family, retained as link calls (no helper implementations).
void EEAddSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EERemoveSightCircle(float inX, float inY, float inRadius, EEPlayer* inPlayer);
void EEMoveSightCircle(float inOldX, float inOldY, float inNewX, float inNewY, float inRadius, EEPlayer* inPlayer);
bool gSightUpdatesEnabled = true;           // 0x4c76ec (.data = 1; just before this unit's 0x4c76f0 table; EEUserInterface/EEWorld/EEMap read it; provisional name)

extern char gEmptyString[];

// Clears every used pool unit's reference count. The original also built a "!Leaking Unit" line per unit and walked
// the reference-counter buckets, but its report was compiled away: neither had an effect (provisional name, from
// Empire Earth.cpp)
void EEReportLeakingUnits() { ee_units_cu_tiny(EE_UNITS_CU_T_DO_REPORT_LEAKING, NULL, NULL, 0, 0, 0.0f, 0.0f, 0.0f); }  // u12a_small.rs

// Writes every used pool unit to the dump file with its class, tile, id, action and growth
// Part of EEWorld's dump; the second copy of the arguments fed a trace the shipping build compiled away
// (provisional name)
void EEDumpLeakedUnits(FSFileSpec& inFile) { ee_units_cu_small(EE_UNITS_CU_S_DO_DUMP_LEAKED, NULL, &inFile, 0); }  // cu_small.rs

// the Rust dump's text into the out-of-sync file
static void EEDumpWrite(void* inFile, const char* inText, size_t inLength) { fwrite(inText, 1, inLength, static_cast<FILE*>(inFile)); }

// Adjusts a value by the unit's morale: positive morale takes a share off, negative morale adds one (combat.yaml
// morale: 80% and 60% in the original)
void EEComplexUnit::ApplyMorale(long inValue, long& outValue)
{
    outValue = EECombatMorale(EeCombatMoraleIn{(int32_t)inValue, (int32_t)this->mPositiveMorale, (int32_t)this->mNegativeMorale,
                                               EEGetRules().mCombat.mPositiveMorale, EEGetRules().mCombat.mNegativeMorale});
}

// SightOn's and SightOff's host (ee_units sight.rs, sight_host.rs; EE_UNITS_SIGHT_*): the unit's and players' reads and
// the sight's world calls, each as the original made it (inOn: SightOn's Add, else SightOff's Remove)
extern "C" int64_t ee_units_sight_host(int32_t inOp, uint8_t inOn, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD,
                                       float (*ioF)[3], EeUnitsSightUnit* outUnit)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    EEPlayer* thePlayer = (EEPlayer*)(uintptr_t)inA;
    float* f = *ioF;
    // (the world's and the players' plain reads are Rust's: ee_units sight_host.rs)
    switch (inOp) {
    case EE_UNITS_SIGHT_U_LAND_BUILDING: return theUnit->GetUnitClass()->IsLandBuilding();
    case EE_UNITS_SIGHT_U_NODE_KIND: return theUnit->GetTechTreeNode() != NULL ? (int32_t)theUnit->GetTechTreeNode()->mData->mKind : -1;
    case EE_UNITS_SIGHT_ENABLED: return gSightUpdatesEnabled;
    case EE_UNITS_SIGHT_FOUNDATION: return theUnit->IsFoundation();
    case EE_UNITS_SIGHT_CENTER: theUnit->AddCenterOffset(f[0], f[1]); return 0;
    case EE_UNITS_SIGHT_ALLIED: return thePlayer->IsAlliedWith((EEPlayer*)(uintptr_t)inB);
    case EE_UNITS_SIGHT_CIRCLE: EEAddSightCircle(f[0], f[1], f[2], thePlayer); return 0;
    case EE_UNITS_SIGHT_CIRCLE_OFF: EERemoveSightCircle(f[0], f[1], f[2], thePlayer); return 0;
    case EE_UNITS_SIGHT_TILE_SIGHT: ((EEMapTile*)(uintptr_t)inA)->AddSight((EEPlayer*)(uintptr_t)inB); return 0;
    case EE_UNITS_SIGHT_TILE_SIGHT_OFF: ((EEMapTile*)(uintptr_t)inA)->RemoveSight((EEPlayer*)(uintptr_t)inB); return 0;
    case EE_UNITS_SIGHT_SHARED: thePlayer->AddSharedSight((EEMapTile*)(uintptr_t)inB, (long)inC, (long)inD); return 0;
    case EE_UNITS_SIGHT_SHARED_OFF: thePlayer->RemoveSharedSight((EEMapTile*)(uintptr_t)inB); return 0;
    case EE_UNITS_SIGHT_SHOW: gMiniMap->CountCellOn((long)inC, (long)inD); return 0;
    case EE_UNITS_SIGHT_SHOW_TILE: ::gWorld.mMap->mTerrainMesh.ReferenceTile((long)inC, (long)inD); return 0;
    case EE_UNITS_SIGHT_SHOW_OFF: gMiniMap->CountCellOff((long)inC, (long)inD); return 0;
    case EE_UNITS_SIGHT_SHOW_TILE_OFF: ::gWorld.mMap->mTerrainMesh.DereferenceTile((long)inC, (long)inD); return 0;
    case EE_UNITS_SIGHT_WONDER: ::gWorld.mMap->AddSight(thePlayer, f[0], f[1], gWorldTable.GetFloatValue(kWorldWonderSightRadius), false); return 0;
    case EE_UNITS_SIGHT_WONDER_OFF: ::gWorld.mMap->RemoveSight(thePlayer, f[0], f[1], gWorldTable.GetFloatValue(kWorldWonderSightRadius), false); return 0;
    }
    return 0;
}

// FleeAfterCalamity's point ahead is the body's first two floats
static_assert(sizeof(GE2DPoint) == 2 * sizeof(float), "a GE2DPoint is two floats");

// Kill's civilization power (EE_UNITS_CU_GET_INSURANCE fills it for the two reads after it)
static EECivPower sInsurancePower;
// the insurance chat line's locals (cu_small.rs: theAmount, theText, thePart)
static char sCuInsuranceAmount[64];
static UWideString sCuInsuranceText;
static UWideString sCuInsurancePart;
// EEDumpLeakedUnits' list (its theUnits)
static std::vector<EEComplexUnit*> sCuLeaked;

// a build queue entry of cu_cargo.rs's (technology, flag)
static EEProductionEntry EECuEntry(int64_t inTechnology, int64_t inFlag)
{
    return EEProductionEntry{(long)inTechnology, inFlag != 0};
}

// the index of a unit in a deque or vector (std::find; -1: not there)
template <class C, class T> static int64_t EECuFind(const C& inUnits, T* inUnit)
{
    typename C::const_iterator theIterator = std::find(inUnits.begin(), inUnits.end(), inUnit);
    return theIterator == inUnits.end() ? -1 : (int64_t)(theIterator - inUnits.begin());
}

// UnloadCargoAt's aircraft at cruise altitude: a copy of the location at height inZ
static void EECuSetTransformationAtZ(EEComplexUnit* inUnit, const GETransformation& inLocation, float inZ)
{
    GETransformation theLocation(inLocation);
    theLocation.SetZ(inZ);
    inUnit->SetTransformation(theLocation, true);
}

// cu_dump.rs's scratch values: the dump's input record and goal location, Describe's text (the originals' locals)
static EeUnitsComplexDumpIn sCuDumpIn;
static GE3DPoint sCuDumpLocation;
static UWideString sCuText;

// cu_world.rs's scratch values: CanReachTile's destination, attach point, position and pitch, MoveToLocation's
// waypoint point, ApplyAreaEffects' location, ReleaseConversionEffect's handle (the originals' locals)
static GE3DPoint sCuDestination;
static GE3DPoint sCuUnused;
static GE3DPoint sCuPosition;
static float sCuPitch;
static GE3DPoint sCuWaypoint;
static GE2DPoint sCuLocation;
static EEEffectHandle sCuHandle;
static EEUnit* sCuBlocker;
static long sCuBlockTile[2];   // x, y

// cu_life.rs's scratch values: SwapGraphic's new angles, orbitals and wheels, StepAside's tiles, IsNextToTile's
// points, Init's copy of the placement (the originals' locals)
static float* sCuAngles;
static EEOrbital** sCuOrbitals;
static EEWheel** sCuWheels;
static std::vector<EETileCoordinate> sCuTiles;
static std::vector<U2DPoint<long> > sCuPoints;
static GETransformation sCuPlacement;

// the fanaticism effect's colour parameter 1..6
static float EECuFanaticismParam(EEGraphicEffect* inEffect, int64_t inIndex)
{
    switch (inIndex) {
    case 1: return inEffect->mParam1;
    case 2: return inEffect->mParam2;
    case 3: return inEffect->mParam3;
    case 4: return inEffect->mParam4;
    case 5: return inEffect->mParam5;
    default: return inEffect->mParam6;
    }
}

template <class T> static T* EECuObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The C++ of EEComplexUnit's Rust bodies (ee_units cu_body.rs; EE_UNITS_CU_*): one statement of the original bodies
// each, in their order; inA an object, inB and inC values, ioF floats in and out
// cu_more.rs's shared locals (the transformation, point and vector its bodies kept), set where they were declared
static GETransformation sCuMoreTransformation;
static GE3DPoint sCuMorePoint;
static GEVector sCuMoreVector;

extern "C" int64_t ee_units_cu_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC, float (*ioF)[4])
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EECuObject<EEComplexUnit>(inA);
    EEUnitClass* theClass = inA != 0 ? EECuObject<EEUnitClass>(inA) : theUnit->GetUnitClass();
    EEModel* theModel = EECuObject<EEModel>(inA);
    EETechTreeNode* theNode = EECuObject<EETechTreeNode>(inA);
    EEPlayer* thePlayer = EECuObject<EEPlayer>(inA);
    FILE* theFile = EECuObject<FILE>(inA);
    float* f = *ioF;
    switch (inOp) {
    // u12a_small.rs's statements (units round 12)
    case EE_UNITS_CU_T_PM_SET_XY: theUnit->mPhysicalModel.SetLocationXY(f[0], f[1]); return 0;
    case EE_UNITS_CU_T_PM_SET_XYZ: theUnit->mPhysicalModel.SetLocationXYZ(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_CU_T_PM_SET_LOCATION: theUnit->mPhysicalModel.SetLocation(*EECuObject<GETransformation>(inA)); return 0;
    case EE_UNITS_CU_T_PM_SET_MODEL: theUnit->mPhysicalModel.SetModel(EECuObject<GEModel>(inA)); return 0;
    case EE_UNITS_CU_T_PM_SET_TILE_SCALE: theUnit->mPhysicalModel.SetScale(theUnit->GetUnitClass()->GetTileSize()); return 0;
    case EE_UNITS_CU_T_POOL_NEW: return (int64_t)(uintptr_t)(inB != 0 ? new EEComplexUnit : EEComplexUnit::sMemoryPool.New());
    case EE_UNITS_CU_T_INIT: theUnit->Init(EECuObject<EEUnitClass>(EECuObject<uint64_t>(inA)[0]), *EECuObject<GETransformation>(EECuObject<uint64_t>(inA)[1]), EECuObject<EEPlayer>(EECuObject<uint64_t>(inA)[2]), (unsigned long)EECuObject<uint64_t>(inA)[3]); return 0;
    case EE_UNITS_CU_T_NODE_POOL_SET_SIZE: UList<EEComplexUnit*>::sNodePool.SetSize((unsigned long)inB, (unsigned long)inC, 3); return 0;
    case EE_UNITS_CU_T_POOL_SET_SIZE: return EEComplexUnit::sMemoryPool.SetSize((unsigned long)inB, (unsigned long)inC, 1);
    case EE_UNITS_CU_T_POOL_DUMP_STATS: EEComplexUnit::sMemoryPool.DumpStats(false); return 0;
    case EE_UNITS_CU_T_POOL_RELEASE: EEComplexUnit::sMemoryPool.Release(); return 0;
    case EE_UNITS_CU_T_LEAKED_ZERO_REFERENCES: sCuLeaked[(size_t)inB]->mReferenceCount = 0; return 0;
    case EE_UNITS_CU_T_CAPTURE_IN_REACH: EECombatCaptureHostRead(EE_COMBAT_HR_IN_REACH, theUnit, EECuObject<EEUnit>(inA), inB != 0, (uint32_t)inC, (uint32_t)theUnit->mWeaponTagPoint); return 0;
    // cu_small.rs's
    case EE_UNITS_CU_S_ANIMATION_MODEL: theUnit->mPhysicalModel.SetModel(theUnit->GetModel()->mModels[theUnit->mAnimation]); return 0;
    case EE_UNITS_CU_S_SET_MODEL_FRAME: theUnit->mPhysicalModel.SetFrame((unsigned long)inB); return 0;
    case EE_UNITS_CU_S_LEAKED_CLEAR: sCuLeaked.clear(); return 0;
    case EE_UNITS_CU_S_LEAKED_GET: EEComplexUnit::sMemoryPool.GetUsed(sCuLeaked); return 0;
    case EE_UNITS_CU_S_LEAKED_COUNT: return (int64_t)sCuLeaked.size();
    case EE_UNITS_CU_S_LEAKED_HEADER: fprintf(*EECuObject<FSFileSpec>(inA), "\n\n**********LEAKED UNITS*********\n\n"); return 0;
    case EE_UNITS_CU_S_LEAKED_LINE: fprintf(*EECuObject<FSFileSpec>(inA), "Leaking Unit: %s at (%ld,%ld). UnitID: %ld with action:%s, with %6.6f growth rate\n", (const char*)sCuLeaked[inB]->GetUnitClass()->GetName(), sCuLeaked[inB]->GetTileX(), sCuLeaked[inB]->GetTileY(), sCuLeaked[inB]->GetUnitID(), (const char*)EEUnitAction::GetActionName(sCuLeaked[inB]->mCommand->mType), sCuLeaked[inB]->GetGrowth()); return 0;
    case EE_UNITS_CU_S_SOUND_DEREFERENCE: theUnit->mSound->Dereference(); return 0;
    case EE_UNITS_CU_S_SOUND_REFERENCE: theUnit->mSound->Reference(); return 0;
    case EE_UNITS_CU_S_DESTROY_HEAL_EFFECT: gWorld.mEffectManager->DestroyEffect(theUnit->mHealEffect); return 0;
    case EE_UNITS_CU_S_REFERENCE_OTHER: theOther->Reference(); return 0;
    case EE_UNITS_CU_S_SET_HOLDER_OF: theOther->SetHolder(theUnit); return 0;
    case EE_UNITS_CU_S_HELD_PUSH: theUnit->mHeldUnits.push_back(theOther); return 0;
    case EE_UNITS_CU_S_HELD_SORT: std::sort(theUnit->mHeldUnits.begin(), theUnit->mHeldUnits.end(), [](EEComplexUnit* inA, EEComplexUnit* inB) { return inA->GetGrowth() < inB->GetGrowth(); }); return 0;
    case EE_UNITS_CU_S_SIGHT_ON_HERE: theUnit->SightOn(theUnit->GetTileX(), theUnit->GetTileY()); return 0;
    case EE_UNITS_CU_S_MINIMAP_SHOW: gMiniMap->MoveUnit(theUnit, -10000, -10000, theUnit->GetTileX(), theUnit->GetTileY()); return 0;
    case EE_UNITS_CU_S_WORLD_IGNORE: EECombatWorld(EE_COMBAT_WORLD_IGNORE, theUnit, theOther); return 0;
    case EE_UNITS_CU_S_CAPTURE_IGNORE: EECombatCaptureIgnored(EE_COMBAT_XR_IGNORE, theUnit, theOther, 0, 0); return 0;
    case EE_UNITS_CU_S_WORLD_IS_IGNORED: return EECombatWorld(inB != 0 ? EE_COMBAT_WORLD_IS_IGNORED : EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE, theUnit, theOther) != 0;
    case EE_UNITS_CU_S_CAPTURE_IS_IGNORED: EECombatCaptureIgnored(EE_COMBAT_XR_IS_IGNORED, theUnit, theOther, (int32_t)inB, inC != 0); return 0;
    case EE_UNITS_CU_S_WORLD_CAN_REACH: return EECombatWorld(EE_COMBAT_WORLD_CAN_REACH_UNIT, theUnit, EECuObject<EEUnit>(inA)) != 0;
    case EE_UNITS_CU_S_CAPTURE_CAN_REACH: EECombatCaptureXRead(EE_COMBAT_XR_CAN_REACH_UNIT, theUnit, EECuObject<EEUnit>(inA), (int32_t)inB); return 0;
    case EE_UNITS_CU_S_WORLD_BORDERING: return EECombatWorld(EE_COMBAT_WORLD_BORDERING, theUnit, EECuObject<EEUnit>(inA)) != 0;
    case EE_UNITS_CU_S_CAPTURE_BORDERING: EECombatCaptureXRead(EE_COMBAT_XR_BORDERING, theUnit, EECuObject<EEUnit>(inA), (int32_t)inB); return 0;
    case EE_UNITS_CU_S_RESET_COMPLEX: theUnit->ResetEEComplexUnit(true, false); return 0;
    case EE_UNITS_CU_S_RESET_UNIT: theUnit->ResetEEUnit(); return 0;
    case EE_UNITS_CU_S_RESET: theUnit->Reset(); return 0;
    case EE_UNITS_CU_S_FREE_NODE: theUnit->FreeNode(); return 0;
    case EE_UNITS_CU_S_ZERO_REFERENCES: theUnit->mReferenceCount = 0; return 0;
    case EE_UNITS_CU_S_POOL_DELETE: EEComplexUnit::sMemoryPool.Delete(theUnit); return 0;
    case EE_UNITS_CU_S_INSURANCE_AMOUNT: sprintf(sCuInsuranceAmount, "%ld", (long)inB); return 0;
    case EE_UNITS_CU_S_INSURANCE_TEXT_NEW: sCuInsuranceText = UWideString(); return 0;
    case EE_UNITS_CU_S_INSURANCE_PART_NEW: sCuInsurancePart = UWideString(); return 0;
    case EE_UNITS_CU_S_INSURANCE_RECEIVED: gResource->GetString(IDS_YOU_HAVE_RECEIVED, sCuInsuranceText); return 0;
    case EE_UNITS_CU_S_INSURANCE_APPEND_AMOUNT: sCuInsuranceText += sCuInsuranceAmount; return 0;
    case EE_UNITS_CU_S_INSURANCE_APPEND_SPACE: sCuInsuranceText += " "; return 0;
    case EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME: gResource->GetString((unsigned long)inB + IDS_RESOURCE_NAME_BASE, sCuInsurancePart); return 0;
    case EE_UNITS_CU_S_INSURANCE_APPEND_PART: sCuInsuranceText += sCuInsurancePart; return 0;
    case EE_UNITS_CU_S_INSURANCE_IN_INSURANCE: gResource->GetString(IDS_IN_INSURANCE, sCuInsurancePart); return 0;
    case EE_UNITS_CU_S_INSURANCE_CHAT_LINE: gUserInterface->AddChatLogEntry(thePlayer->mColor, UWideString(gEmptyString), sCuInsuranceText, 5000, 2); return 0;
    // Kill's
    case EE_UNITS_CU_UNIT_TAKEN: theUnit->mPlayer->UnitTaken(theUnit, inB != 0); return 0;
    case EE_UNITS_CU_WORLD_CLEARING: return gWorld.mClearing;
    case EE_UNITS_CU_UNIT_SET: return (uint64_t)(uintptr_t)gUnitSetManager.GetSet((unsigned long)inB);
    case EE_UNITS_CU_SET_HAS_TYPE: return EECuObject<EEUnitSet>(inA)->Contains(theUnit->GetUnitClass()->GetUnitType());
    case EE_UNITS_CU_UPDATE_FREE_UNIT_POWER: thePlayer->UpdateFreeUnitPower(); return 0;
    case EE_UNITS_CU_DEBUG_DEAD_CITIZEN: ESDebugOut("EEComplexUnit::Kill - Increment dead citizen count - type - %ld\n", theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_CU_ACTIVATE: ::gWorld.ActivateUnit(theUnit); return 0;
    case EE_UNITS_CU_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_CU_SWAP_KIND: return gPhysics->GetUnitSwapKind(theUnit);
    case EE_UNITS_CU_STATS_RESET: theUnit->mPlayer->mSavedGroups.Reset((long)inB, 1); return 0;
    case EE_UNITS_CU_ADD_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theUnit);
    case EE_UNITS_CU_STATS_LOWER: theUnit->mPlayer->mSavedGroups.Lower((long)inB, 1); return 0;
    case EE_UNITS_CU_HAS_INSURANCE: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerInsurance);
    case EE_UNITS_CU_GET_INSURANCE: return gCivPowersManager.GetPower(kCivPowerInsurance, sInsurancePower);
    case EE_UNITS_CU_INSURANCE_COVERS: return gUnitSetManager.Contains(theUnit->GetUnitClass()->GetUnitType(), sInsurancePower.mUnitSet);
    case EE_UNITS_CU_INSURANCE_AMOUNT: return UFloatBits(sInsurancePower.mAmount);
    case EE_UNITS_CU_DEREFERENCE: EECuObject<RTSUnit>(inA)->Dereference(); return 0;
    case EE_UNITS_CU_HOLDER_RELEASE: theUnit->mHolder->ReleaseHeld(theUnit); return 0;
    case EE_UNITS_CU_CANCEL_PATHS: gPathManager.CancelPaths(theUnit, true); return 0;
    case EE_UNITS_CU_ATC_REMOVE_AIRPORT: ee_units_atc_remove_airport(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_ATC_REMOVE_CARGO: ee_units_atc_remove_cargo_weight(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_CU_KILL_PART: theOther->Kill(inB != 0, false, false); return 0;
    case EE_UNITS_CU_FREE_CAMERA: return gFreeCamera;
    case EE_UNITS_CU_CREATE_DEATH_UNIT: theUnit->GetUnitClass()->CreateDeathUnit(theUnit); return 0;
    case EE_UNITS_CU_CREATE_REMAINS_UNIT: theUnit->GetUnitClass()->CreateRemainsUnit(theUnit); return 0;
    case EE_UNITS_CU_CONTAINER_RELEASE: theUnit->mContainer->ReleaseCargo(theUnit); return 0;
    case EE_UNITS_CU_AFFECTING_KEEP_STRONGEST: theUnit->mAffectingAreaEffects.KeepStrongest(); return 0;
    case EE_UNITS_CU_AFFECTING_CLEAR: theUnit->mAffectingAreaEffects.Clear(); return 0;
    case EE_UNITS_CU_AFFECTING_ITEMS_CLEAR: theUnit->mAffectingAreaEffects.mItems.clear(); return 0;
    case EE_UNITS_CU_AREA_EFFECTS_CLEAR: theUnit->mAreaEffects.Clear(); return 0;
    case EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR: theUnit->mAreaEffects.mItems.clear(); return 0;
    case EE_UNITS_CU_UNIT_KILL: theUnit->EEUnit::Kill((inB & 1) != 0, (inB & 2) != 0, (inB & 4) != 0); return 0;
    // Invalidate's
    case EE_UNITS_CU_SOUND_STOP: gSoundManager->Stop(theUnit->mSound, false); return 0;
    case EE_UNITS_CU_SOUND_DEREFERENCE: theUnit->mSound->Dereference(); return 0;
    case EE_UNITS_CU_BUILD_TARGET_COUNT: return theUnit->mCitizenBuildQueue.size();
    case EE_UNITS_CU_BUILD_TARGET_DEREFERENCE: theUnit->mCitizenBuildQueue[(size_t)inB]->Dereference(); return 0;
    case EE_UNITS_CU_BUILD_TARGETS_CLEAR: theUnit->mCitizenBuildQueue.clear(); return 0;
    case EE_UNITS_CU_IGNORED_CLEAR: theUnit->mIgnoredTargets.clear(); return 0;
    case EE_UNITS_CU_UNIT_REMOVE: theUnit->EEUnit::Remove(theUnit->mIsFoundation); return 0;
    case EE_UNITS_CU_IS_LAND_BUILDING: return theUnit->GetUnitClass()->IsLandBuilding();
    case EE_UNITS_CU_UNLOAD_BUTTONS: theUnit->mPlayer->UnloadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType()); return 0;
    // StartProduction's
    case EE_UNITS_CU_CAN_PRODUCE: return theUnit->GetPlayer()->CanProduce((long)inB, theUnit->GetUnitClass()->GetUnitType(), 1, false, 2, NULL);
    case EE_UNITS_CU_CLASS_IS_AIRCRAFT: return theClass->IsAircraft();
    case EE_UNITS_CU_ATC_CARGO: return ee_units_atc_cargo(theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1], (int32_t)inB);
    case EE_UNITS_CU_RANDOM: return UFloatBits(gPhysics->mRandom.Generate());
    case EE_UNITS_CU_UI_UPDATE_JIT: gUserInterface->UpdateJITManufacturing(); return 0;
    case EE_UNITS_CU_QUEUE_POP_FRONT: theUnit->mBuildQueue.erase(theUnit->mBuildQueue.begin()); return 0;
    case EE_UNITS_CU_REFUND_TECHNOLOGY: theUnit->GetPlayer()->RefundTechnology(theNode, theUnit->GetUnitClass()->GetUnitType(), UBitsFloat(inB)); return 0;
    // SetBuilt's (inA the part)
    case EE_UNITS_CU_EE_PLAYER: return (uint64_t)(uintptr_t)dynamic_cast<EEPlayer*>(theOther->mPlayer);
    case EE_UNITS_CU_PART_IS_LAND_BUILDING: return theOther->mUnitClass->IsLandBuilding();
    case EE_UNITS_CU_PART_SIGHT_OFF: theOther->SightOff(theOther->GetTileX(), theOther->GetTileY()); return 0;
    case EE_UNITS_CU_PART_SIGHT_ON: theOther->SightOn(theOther->GetTileX(), theOther->GetTileY()); return 0;
    case EE_UNITS_CU_PART_ATC_ADD_AIRPORT: ee_units_atc_add_airport(EECuObject<EEPlayer>((uint64_t)inB)->mAirTraffic[inC - 1], theOther); return 0;
    case EE_UNITS_CU_PART_UNIT_COMPLETED: EECuObject<EEPlayer>((uint64_t)inB)->UnitCompleted(theOther); return 0;
    case EE_UNITS_CU_PART_REPLANT:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReplantFarms, {theOther->mUnitID}), kSlotNone);
        return 0;
    case EE_UNITS_CU_PART_LOAD_BUTTONS: theOther->mPlayer->LoadUnitClassButtons(theOther->mUnitClass->GetUnitType()); return 0;
    // StartAction's
    case EE_UNITS_CU_SET_COMMAND: theUnit->mCommand = theUnit->GetUnitClass()->FindAction((long)inB); return 0;
    // SetAnimation's (inA the model)
    case EE_UNITS_CU_MODEL: return (uint64_t)(uintptr_t)theUnit->GetModel();
    case EE_UNITS_CU_MODEL_START_FRAME: return (long)theModel->mStartFrames[inB];
    case EE_UNITS_CU_MODEL_LAST_FRAME: return (long)theModel->mLastFrames[inB];
    case EE_UNITS_CU_PM_SET_MODEL: theUnit->mPhysicalModel.SetModel(theModel->mModels[inB]); return 0;
    case EE_UNITS_CU_PM_SET_FRAME: theUnit->mPhysicalModel.SetFrame((long)inB); return 0;
    case EE_UNITS_CU_SET_NEXT_FRAME: theUnit->SetNextFrame((long)inB); return 0;
    case EE_UNITS_CU_SOUND_STOP_UNLESS_LOOPING: gSoundManager->Stop(theUnit->mSound, !gSoundManager->GetIsLooping(theUnit->mSound)); return 0;
    case EE_UNITS_CU_MODEL_SOUND1_FRAME: return (long)theModel->mSound1Frames[inB];
    case EE_UNITS_CU_MODEL_SOUND2_FRAME: return (long)theModel->mSound2Frames[inB];
    case EE_UNITS_CU_MODEL_SOUND1: return (uint64_t)(uintptr_t)theModel->mSounds1[inB];
    case EE_UNITS_CU_MODEL_SOUND2: return (uint64_t)(uintptr_t)theModel->mSounds2[inB];
    case EE_UNITS_CU_SOUND_PARAMETERS: theUnit->GetUnitClass()->GetSoundParameters(theUnit, EECuObject<EESoundObject>(inA), &f[0], &f[1], &f[2]); return 0;
    case EE_UNITS_CU_PLAY_FRAME_SOUND: theUnit->SetSound(gSoundManager->PlayAndReturnReferencedNode( EECuObject<EESoundObject>(inA)->GetSampleID(), f[0], f[1], f[2], EECuObject<EESoundObject>(inA)->GetLooping(), const_cast<char*>((const char*)theUnit->GetUnitClass()->mName))); return 0;
    // PayInsurance's (inA the player)
    case EE_UNITS_CU_PLAYER_REFUND_TECHNOLOGY: theUnit->mPlayer->RefundTechnology(theNode, theUnit->GetUnitClass()->GetUnitType(), UBitsFloat(inB)); return 0;
    case EE_UNITS_CU_DISBAND_EFFECT: gWorld.mEffectManager->CreateUnitEffect(theUnit, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), NULL, true, 0, 0); return 0;
    case EE_UNITS_CU_INSURANCE_SOUND: return (uint64_t)(uintptr_t)EESoundObject::Get(kSoundInsuranceCollected);
    case EE_UNITS_CU_PLAY_AT_VOLUME: EECuObject<EESoundObject>(inA)->Play(EECuObject<EESoundObject>(inA)->GetVolume(), 0.0f, 1.0f); return 0;
    // FindContacts' (inA a class, 0: the unit's; a tile, a node or another unit)
    case EE_UNITS_CU_FLIES_OVER: return theClass->FliesOver();
    case EE_UNITS_CU_OTHER_IS_FOUNDATION: return theOther->IsFoundation();
    // the production queue's (cu_cargo.rs; inA a node or class)
    case EE_UNITS_CU_QUEUE_POP_BACK: theUnit->mBuildQueue.pop_back(); return 0;
    case EE_UNITS_CU_REFUND_ONE: theUnit->mPlayer->RefundTechnology(theNode, theUnit->GetUnitClass()->mUnitType, 1.0f); return 0;
    case EE_UNITS_CU_QUEUE_CLEAR: theUnit->mBuildQueue.clear(); return 0;
    case EE_UNITS_CU_QUEUE_PUSH_BACK: theUnit->mBuildQueue.push_back(EECuEntry(inB, inC)); return 0;
    case EE_UNITS_CU_QUEUE_PUSH_FRONT: theUnit->mBuildQueue.insert(theUnit->mBuildQueue.begin(), EECuEntry(inB, inC)); return 0;
    case EE_UNITS_CU_QUEUE_ERASE_AT: theUnit->mBuildQueue.erase(theUnit->mBuildQueue.begin() + (size_t)inB); return 0;
    case EE_UNITS_CU_CAN_AFFORD: return theUnit->GetPlayer()->CanAfford(theNode, theUnit->GetUnitClass()->GetUnitType(), true);
    case EE_UNITS_CU_SCENARIO_GAME: return ::gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_CU_HERO_COUNT: return theUnit->mPlayer->GetHeroCount(1, 1);
    case EE_UNITS_CU_PAY: theUnit->mPlayer->Pay(theNode, theUnit->GetUnitClass()->GetUnitType(), 1.0f); return 0;
    // the cargo's (inA another unit)
    case EE_UNITS_CU_CARGO_PUSH: theUnit->mCargo.push_back(theOther); return 0;
    case EE_UNITS_CU_REFERENCE: EECuObject<RTSUnit>(inA)->Reference(); return 0;
    case EE_UNITS_CU_OFF_MAP_INFO: return (uint64_t)(uintptr_t)theOther->mPlayer->GetOffMapInfo(theOther);
    case EE_UNITS_CU_SET_OFF_MAP_REASON: EECuObject<EEWUnitsOffMapInfo>(inA)->mReason = (long)inB; return 0;
    case EE_UNITS_CU_CARGO_ADDED: theUnit->GetUnitClass()->CargoAdded(theUnit, theOther, inB != 0); return 0;
    case EE_UNITS_CU_ACTIVE_GROUP_HAS: return theUnit->mPlayer->mActiveGroup->Contains(theOther);
    case EE_UNITS_CU_POST_424: gUserInterface->PostMessage424Once(); return 0;
    case EE_UNITS_CU_KILL_UNIT: theOther->Kill(0, 0, 0); return 0;
    case EE_UNITS_CU_UNIT_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theOther);
    case EE_UNITS_CU_UNIT_STATS_RESET: theOther->mPlayer->mSavedGroups.Reset((long)inB, 1); return 0;
    case EE_UNITS_CU_UNIT_STATS_LOWER: theOther->mPlayer->mSavedGroups.Lower((long)inB, 1); return 0;
    case EE_UNITS_CU_CARGO_REMOVED: theUnit->GetUnitClass()->CargoRemoved(theUnit, theOther); return 0;
    case EE_UNITS_CU_CARGO_ERASE_AT: theUnit->mCargo.erase(theUnit->mCargo.begin() + (size_t)inB); return 0;
    case EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE: theOther->mContainer->Dereference(); return 0;
    case EE_UNITS_CU_CARGO_UNLOADED: theUnit->GetUnitClass()->CargoUnloaded(theUnit, theOther); return 0;
    case EE_UNITS_CU_UNIT_SET_TRANSFORMATION: theOther->SetTransformation(*EECuObject<GETransformation>((uint64_t)inB), true); return 0;
    case EE_UNITS_CU_UNIT_SET_PLANET: theOther->mPlanetID = EEGetLandContinentOf(theOther); return 0;
    case EE_UNITS_CU_UNIT_SET_TRANSFORMATION_AT_Z: EECuSetTransformationAtZ(theOther, *EECuObject<GETransformation>((uint64_t)inB), UBitsFloat(inC)); return 0;
    case EE_UNITS_CU_ACTIVATE_UNIT: ::gWorld.ActivateUnit(theOther); return 0;
    case EE_UNITS_CU_DEACTIVATE_UNIT: ::gWorld.DeactivateUnit(theOther); return 0;
    case EE_UNITS_CU_UNIT_IS_HUMAN: return theOther->mPlayer->IsHuman();
    case EE_UNITS_CU_UNIT_ACTION_CHANGED: theOther->mUnitClass->ActionChanged(theOther); return 0;
    case EE_UNITS_CU_UNIT_REFERENCE_COUNT: return theOther->mReferenceCount;
    case EE_UNITS_CU_TRANSPORT_HAS_ROOM: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->HasRoom(theUnit, theOther->GetComplexUnit());
    case EE_UNITS_CU_TRANSPORT_UNLOAD_RANGE: return UFloatBits(EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->mUnloadRange);
    case EE_UNITS_CU_UNLOAD_CARGO_AT: return (uint64_t)(uintptr_t)theUnit->UnloadCargoAt((unsigned long)inB, *EECuObject<GETransformation>(inA));
    case EE_UNITS_CU_TELEPORT_IN_EFFECT: ::gWorld.GetEffectManager()->CreateUnitEffect(theOther, EEGraphicEffect::Get(kGraphicEffectCalamityTeleportIn), NULL, false, 0, 0); return 0;
    case EE_UNITS_CU_UNLOAD_FAILED_TEXT: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->GetUnloadFailedTextID();
    case EE_UNITS_CU_ADD_EVENT: theUnit->mPlayer->mEvents.AddEvent(U2DPoint<long>(theUnit->GetTileX(), theUnit->GetTileY()), 2, theUnit->mPlayer, (long)inB, false); return 0;
    case EE_UNITS_CU_SET99_HAS_TYPE: return gUnitSetManager.GetSet(99)->Contains(theUnit->GetUnitClass()->GetUnitType());
    case EE_UNITS_CU_TRANSPORT_DOMAINS: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass())->mDomains;
    case EE_UNITS_CU_TRANSPORT_CLASS: return (uint64_t)(uintptr_t)EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theUnit->GetUnitClass());
    // the build queue's and contacts'
    case EE_UNITS_CU_BUILD_GOAL_TARGET: return (uint64_t)(uintptr_t)EEBuildRepairGoalCast(theUnit->GetGoal())->GetBuildTarget();
    case EE_UNITS_CU_BUILD_QUEUE_PUSH_FRONT: theUnit->mCitizenBuildQueue.push_front(EECuObject<EEUnit>(inA)); return 0;
    case EE_UNITS_CU_BUILD_QUEUE_PUSH_BACK: theUnit->mCitizenBuildQueue.push_back(EECuObject<EEUnit>(inA)); return 0;
    case EE_UNITS_CU_BUILD_QUEUE_FIND: return EECuFind(theUnit->mCitizenBuildQueue, EECuObject<EEUnit>(inA));
    case EE_UNITS_CU_BUILD_QUEUE_ERASE_AT:
        theUnit->mCitizenBuildQueue.erase(theUnit->mCitizenBuildQueue.begin() + (size_t)inB);   // (Dinkum deque erase(P))
        return 0;
    case EE_UNITS_CU_CONTACT_ADD_HEAD: theUnit->mContacts.AddHead(theOther); return 0;
    case EE_UNITS_CU_REFERENCE_THIS: theUnit->Reference(); return 0;
    // cu_misc.rs's (inA a part or a model, inB a player index)
    case EE_UNITS_CU_KEEP_MAP_POSITION: theUnit->GetPosition(theUnit->mLastMapPosition); return 0;
    case EE_UNITS_CU_DESELECT: theUnit->mPlayer->DeselectUnit(theOther); return 0;
    case EE_UNITS_CU_PLAYER_VIEWS: return EECuObject<EEPlayer>((uint64_t)inB)->mViewedUnit == theOther;
    case EE_UNITS_CU_PLAYER_UPDATE_SELECTION: EECuObject<EEPlayer>((uint64_t)inB)->UpdateSelection(NULL, true); return 0;
    case EE_UNITS_CU_PLAYER_SELECTED_VIEWS: return EECuObject<EEPlayer>((uint64_t)inB)->mSelectedViewedUnit == theOther;
    case EE_UNITS_CU_PLAYER_CLEAR_SELECTED_VIEWED: EECuObject<EEPlayer>((uint64_t)inB)->SetSelectedViewedUnit(NULL); return 0;
    case EE_UNITS_CU_PART_KEEP_MAP_POSITION: theOther->GetPosition(theOther->mLastMapPosition); return 0;
    case EE_UNITS_CU_PART_SET_AWAY: theOther->SetLocation(GE3DPoint(-10000.0f, -10000.0f, theOther->mPhysicalModel.GetLocation().GetZ()), true); return 0;
    case EE_UNITS_CU_CAST_CALAMITY_TYPE: return EECastCalamityGoalCast(theUnit->GetGoal())->GetCalamityType();
    case EE_UNITS_CU_UNIT_LOAD_MEDIA: theUnit->EEUnit::LoadMedia(inB != 0); return 0;
    case EE_UNITS_CU_LOAD_BUTTONS: theUnit->GetPlayer()->LoadUnitClassButtons(theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_CU_PM_SET_FOUNDATION_MODEL: theUnit->mPhysicalModel.SetModel(EEGetPlayerClassGraphic(theUnit->GetPlayer(), theUnit->GetUnitClass())->mModel); return 0;
    case EE_UNITS_CU_PM_SET_ANIMATION_MODEL: theUnit->mPhysicalModel.SetModel(theModel->mModels[theUnit->mAnimation]); return 0;
    case EE_UNITS_CU_RESIZE_TURRET_ANGLES: theUnit->ResizeTurretAngles(theModel->GetNumParts()); return 0;
    case EE_UNITS_CU_MODEL_ORBITAL_COUNT: return theModel->GetOrbitalCount();
    case EE_UNITS_CU_NEW_ORBITALS: theUnit->mOrbitals = new EEOrbital*[theUnit->mNumOrbitals]; return 0;
    case EE_UNITS_CU_NEW_ORBITAL: theUnit->mOrbitals[inB] = new EEOrbital(*theModel->mOrbitals[inB]); return 0;
    case EE_UNITS_CU_MODEL_WHEEL_COUNT: return theModel->GetWheelCount();
    case EE_UNITS_CU_NEW_WHEELS: theUnit->mWheels = new EEWheel*[theUnit->mNumWheels]; return 0;
    case EE_UNITS_CU_NEW_WHEEL: theUnit->mWheels[inB] = new EEWheel(*theModel->mWheels[inB]); return 0;
    case EE_UNITS_CU_WHEEL_ANGLE_ZERO: theUnit->mWheels[inB]->mAngle = 0.0f; return 0;
    case EE_UNITS_CU_ATC_ADD_AIRPORT: ee_units_atc_add_airport(theUnit->mPlayer->mAirTraffic[inB - 1], theUnit); return 0;
    case EE_UNITS_CU_TIME_WARP_COLOR: theUnit->SetColor(kTimeWarpColor); return 0;
    case EE_UNITS_CU_HAS_CAMOUFLAGE: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerCamouflage);
    case EE_UNITS_CU_CAMOUFLAGE_SET: return (uint64_t)(uintptr_t)gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    case EE_UNITS_CU_MINIMAP_REMOVE: gMiniMap->MoveUnit(theUnit, -10000, -10000, theUnit->GetTileX(), theUnit->GetTileY()); return 0;
    case EE_UNITS_CU_CAMOUFLAGE_TIMING2: return gWorldTable.GetLongValue2(kWorldCamouflageTiming);
    case EE_UNITS_CU_CAMOUFLAGE_TIMING: return gWorldTable.GetLongValue(kWorldCamouflageTiming);
    case EE_UNITS_CU_RELEASE_RESOURCE_USER: ::gWorld.ReleaseResourceUser(theUnit->mGatheredResource); return 0;
    case EE_UNITS_CU_WAITING_GATHERER: return (uint64_t)(uintptr_t)EEGetWaitingGatherer(theUnit->mGatheredResource);
    case EE_UNITS_CU_ADD_RESOURCE_USER: ::gWorld.AddResourceUser(theUnit->mGatheredResource); return 0;
    case EE_UNITS_CU_POST_UNLOAD_TECHNOLOGY: gDataPreLoading->PostUnloadTechnology(theUnit->mPlayer->mPlayerID, theUnit->mCurrentResearch); return 0;
    case EE_UNITS_CU_POST_LOAD_TECHNOLOGY: gDataPreLoading->PostLoadTechnology(theUnit->mPlayer->mPlayerID, (long)inB); return 0;
    case EE_UNITS_CU_DESTROY_FIRE_EFFECT: gWorld.mEffectManager->DestroyEffect(theUnit->mFireEffect); return 0;
    case EE_UNITS_CU_REMOVE_CLOAK_EFFECT: EERemoveCloakEffect(theUnit); return 0;
    case EE_UNITS_CU_ATC_CLEAR_AIRPORT_QUEUE: ee_units_atc_clear_airport_queue(theUnit->mPlayer->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1], theUnit, (uint32_t)inB); return 0;
    case EE_UNITS_CU_RANDOM_POINT_AHEAD: return EEGetRandomPointAhead(theUnit, reinterpret_cast<GE2DPoint*>(f));
    case EE_UNITS_CU_FLEE_GOAL: theUnit->SetGoal(EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), true, true, false, 0.0f, false, false), 0, 0); return 0;
    case EE_UNITS_CU_ABANDON_GOAL: theUnit->mGoalQueues[0].GetGoalAt((unsigned long)inB)->Abandon(theUnit); return 0;
    case EE_UNITS_CU_GOAL_QUEUE_CLEAR: theUnit->mGoalQueues[(size_t)inB].Clear(); return 0;
    // cu_dump.rs's
    case EE_UNITS_CU_DUMP_IN: return (int64_t)(uintptr_t)&sCuDumpIn;
    case EE_UNITS_CU_DUMP_ANGULAR_VELOCITY: f[0] = inB == 0 ? theUnit->mAngularVelocity.GetYaw() : inB == 1 ? theUnit->mAngularVelocity.GetPitch() : theUnit->mAngularVelocity.GetRoll(); return 0;
    case EE_UNITS_CU_DUMP_QUEUE_SIZE: return (int64_t)(inB == 0 ? theUnit->mBuildQueue.size() : theUnit->mCitizenBuildQueue.size());
    case EE_UNITS_CU_DUMP_PROJECTILE_START: f[0] = inB == 0 ? theUnit->mProjectileStart.GetX() : inB == 1 ? theUnit->mProjectileStart.GetY() : theUnit->mProjectileStart.GetZ(); return 0;
    case EE_UNITS_CU_DUMP_LOCATION_ELEMENT: f[0] = theUnit->mPhysicalModel.GetLocation().GetElement((unsigned long)inB, (unsigned long)inC); return 0;
    case EE_UNITS_CU_DUMP_PART: ee_units_complex_dump((uint32_t)inB, theUnit, theUnit, theUnit, theUnit->GetUnitClass(), theUnit->GetStats(), &sCuDumpIn, &EEDumpWrite, theFile); return 0;
    case EE_UNITS_CU_DUMP_ACTION: fprintf(theFile, "OUT OF SYNCH DUMP:  Action:             %s(%ld)\n", (const char*)EEUnitAction::GetActionName(theUnit->GetComplexUnit()->GetCommandType()), theUnit->GetComplexUnit()->GetCommandType()); return 0;
    case EE_UNITS_CU_DUMP_NO_GROUP: fprintf(theFile, "OUT OF SYNCH DUMP:  This unit has no active group\n"); return 0;
    case EE_UNITS_CU_DUMP_GROUP: fprintf(theFile, "OUT OF SYNCH DUMP:  This unit has an active group with an action of %ld\n", theUnit->GetComplexUnit()->mActiveGroup->mFields.mAction); return 0;
    case EE_UNITS_CU_DUMP_GOALS: fprintf(theFile, "OUT OF SYNCH DUMP:  **************    GOALS        *************\n"); return 0;
    case EE_UNITS_CU_DUMP_QUEUE: fprintf(theFile, "OUT OF SYNCH DUMP:  ************** GOAL Queue(%6ld) Number Of Goals(%6zu) *************\n", (long)inB, theUnit->GetComplexUnit()->mGoalQueues[(size_t)inB].mGoals.size()); return 0;
    case EE_UNITS_CU_DUMP_GOAL_KIND: fprintf(theFile, "OUT OF SYNCH DUMP:  Goal Number(%6ld) Has a type of:\t%6d\n", (long)inC, theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]->mKind); return 0;
    case EE_UNITS_CU_DUMP_GOAL: theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]->DumpOutOfSynch(theFile); return 0;
    case EE_UNITS_CU_DUMP_GOAL_LOCATION: theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]->GetGoalLocation(sCuDumpLocation); return 0;
    case EE_UNITS_CU_DUMP_STATE_LOCATION: theUnit->GetComplexUnit()->mGoalState->GetGoalLocation(sCuDumpLocation); return 0;
    case EE_UNITS_CU_DUMP_LOCATION: fprintf(theFile, "OUT OF SYNCH DUMP:  Goal Location (x): %6.6f\n", (double)sCuDumpLocation.GetX()); return 0;
    case EE_UNITS_CU_DUMP_LOCATION_Y: fprintf(theFile, "OUT OF SYNCH DUMP:  Goal Location (y): %6.6f\n", (double)sCuDumpLocation.GetY()); return 0;
    case EE_UNITS_CU_DUMP_LOCATION_Z: fprintf(theFile, "OUT OF SYNCH DUMP:  Goal Location (z): %6.6f\n", (double)sCuDumpLocation.GetZ()); return 0;
    case EE_UNITS_CU_GOAL_AUTO_WAYPOINT: return static_cast<EEUGRust*>(theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC])->GetIsAutoWaypoint() == true;
    case EE_UNITS_CU_DUMP_AUTO_WAYPOINT: fprintf(theFile, "OUT OF SYNCH DUMP:  Location Goal Is An AutoWaypoint\n"); return 0;
    case EE_UNITS_CU_DUMP_NOT_AUTO_WAYPOINT: fprintf(theFile, "OUT OF SYNCH DUMP:  Location Goal Is Not An AutoWaypoint\n"); return 0;
    case EE_UNITS_CU_DUMP_GOAL_STATE: fprintf(theFile, "OUT OF SYNCH DUMP:  Current Goal:\t%6d\n", theUnit->GetComplexUnit()->mGoalState->mKind); return 0;
    case EE_UNITS_CU_DUMP_WAYPOINT_SIZE: fprintf(theFile, "OUT OF SYNCH DUMP:  Waypoint Size:%zu\n", theUnit->GetComplexUnit()->mWaypoints.size()); return 0;
    case EE_UNITS_CU_DUMP_WAYPOINT: fprintf(theFile, "OUT OF SYNCH DUMP:  Waypoint[%ld]: X:%f, Y:%f, Z:%f\n", (long)inB, (double)theUnit->GetComplexUnit()->mWaypoints[(size_t)inB].mLocation.GetX(), (double)theUnit->GetComplexUnit()->mWaypoints[(size_t)inB].mLocation.GetY(), (double)theUnit->GetComplexUnit()->mWaypoints[(size_t)inB].mLocation.GetZ()); return 0;
    case EE_UNITS_CU_DUMP_AREA_HEADER: fprintf(theFile, "OUT OF SYNCH DUMP:  **************************** Unit Area Effects ********************\n\n"); return 0;
    case EE_UNITS_CU_AFFECTING_SIZE: return (int64_t)theUnit->mAffectingAreaEffects.GetSize();
    case EE_UNITS_CU_DUMP_AFFECTING_HEADER: fprintf(theFile, "OUT OF SYNCH DUMP:  **************************** Unit Area Effects Affected By ********************\n"); return 0;
    case EE_UNITS_CU_DUMP_AFFECTING: theUnit->mAffectingAreaEffects.mItems[(size_t)inB]->Dump(theFile); return 0;
    case EE_UNITS_CU_DUMP_GENERATED_HEADER: fprintf(theFile, "\nOUT OF SYNCH DUMP:  **************************** Unit Area Effects Generated ********************\n"); return 0;
    case EE_UNITS_CU_DUMP_AREA_EFFECT: theUnit->mAreaEffects.mItems[(size_t)inB]->Dump(theFile); return 0;
    case EE_UNITS_CU_DESCRIBE_BASE: theUnit->EEUnit::Describe(sCuText = UWideString()); return 0;
    case EE_UNITS_CU_DESCRIBE_APPEND: *EECuObject<UWideString>(inA) += sCuText; return 0;
    case EE_UNITS_CU_DESCRIBE_ACTION: sprintf(sCuText, "act %s:", (const char*)EEUnitAction::GetActionName(theUnit->mCommand->mType)); return 0;
    case EE_UNITS_CU_DESCRIBE_COLLISION: sprintf(sCuText, "coll %d", theUnit->mCollisionState); return 0;
    case EE_UNITS_CU_DESCRIBE_COLLISION_TILE: sprintf(sCuText, " / tile(%ld,%ld)", theUnit->mCollisionTile.mX, theUnit->mCollisionTile.mY); return 0;
    case EE_UNITS_CU_DESCRIBE_COLON: *EECuObject<UWideString>(inA) += ":"; return 0;
    case EE_UNITS_CU_DESCRIBE_VELOCITY: sprintf(sCuText, "vel %.4f:", theUnit->mVelocity); return 0;
    case EE_UNITS_CU_DESCRIBE_CLEAR: sCuText.Clear(); return 0;
    case EE_UNITS_CU_DESCRIBE_GOAL: theUnit->mGoalState->GetDebugLine(sCuText); return 0;
    // cu_world.rs's
    case EE_UNITS_CU_ATTACK_GOAL_OF: return (uint64_t)(uintptr_t)EEAttackUnitCast(theOther->GetGoal());
    case EE_UNITS_CU_ATTACK_TARGET_IS_THIS: return EECuObject<EEUGRust>(inA)->GetAttackTarget() == theUnit;
    case EE_UNITS_CU_IN_REACH_OF: return theOther->IsInReach(theUnit);
    case EE_UNITS_CU_LINE_FROM_BLOCKED: return EEIsLineToUnitBlocked(theOther, theUnit, &sCuBlocker, &sCuBlockTile[0], &sCuBlockTile[1]);
    case EE_UNITS_CU_GROUND_GOAL_OF: return (uint64_t)(uintptr_t)EEAttackGroundCast(theOther->GetGoal());
    case EE_UNITS_CU_GROUND_TARGET: return UFloatBits(inB == 0 ? EECuObject<EEUGRust>(inA)->GetGroundTarget().GetX() : EECuObject<EEUGRust>(inA)->GetGroundTarget().GetY());
    case EE_UNITS_CU_CARGO_DEREFERENCE_AT: theUnit->mCargo[(size_t)inB]->Dereference(); return 0;
    case EE_UNITS_CU_DEACTIVATE_THIS: ::gWorld.DeactivateUnit(theUnit); return 0;
    case EE_UNITS_CU_PAIRS_SIZE: return (int64_t)EECuObject<const std::vector<EEPUnitSwap> >(inA)->size();
    case EE_UNITS_CU_PAIR_UNIT: return (uint64_t)(uintptr_t)(inC == 0 ? (*EECuObject<const std::vector<EEPUnitSwap> >(inA))[(size_t)inB].mOld : (*EECuObject<const std::vector<EEPUnitSwap> >(inA))[(size_t)inB].mNew);
    case EE_UNITS_CU_GOAL_REPLACE: EECuObject<EEUnitGoal>(inA)->ReplaceUnit(EECuObject<EEComplexUnit>((uint64_t)inB), EECuObject<EEComplexUnit>((uint64_t)inC), theUnit); return 0;
    case EE_UNITS_CU_SIGHT_READY: return theUnit->mPlayer != NULL && theUnit->mPlayer->mSaveVisMap == true && theUnit->GetLineOfSight() > 0.0f && gSightUpdatesEnabled;
    case EE_UNITS_CU_SIGHT_ON: theUnit->SightOn((unsigned long)inB, (unsigned long)inC); return 0;
    case EE_UNITS_CU_SIGHT_OFF: theUnit->SightOff((unsigned long)inB, (unsigned long)inC); return 0;
    case EE_UNITS_CU_MOVE_SIGHT_CIRCLE: EEMoveSightCircle(f[0], f[1], f[2], f[3], theUnit->GetLineOfSight(), theUnit->mPlayer); return 0;
    case EE_UNITS_CU_GATE_IS_SIDE_PART: return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetUnitClass())->IsSidePart(theUnit);
    case EE_UNITS_CU_IS_FOUNDATION_NOW: return theUnit->IsFoundation();
    case EE_UNITS_CU_SET_DESTINATION: sCuDestination = GE3DPoint((float)(long)inB, (float)(long)inC, ::gWorld.mMap->GetTile((long)inB, (long)inC)->mHeight); return 0;
    case EE_UNITS_CU_APPROACH_ATTACH: EEApproachAttachPoint(theUnit, sCuUnused = GE3DPoint(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CU_GET_POSITION: theUnit->GetPosition(sCuPosition = GE3DPoint(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CU_BALLISTIC_PITCH: return EEGetBallisticPitch(theUnit, theUnit->GetStats(), &sCuPosition, &sCuDestination, &sCuPitch);
    case EE_UNITS_CU_LINE_BLOCKED: return EEIsLineBlocked(theUnit->GetTileX(), theUnit->GetTileY(), (long)inB, (long)inC, &sCuBlocker, &sCuBlockTile[0], &sCuBlockTile[1]);
    case EE_UNITS_CU_DISTANCE_SQUARED_TO_TILE: return UFloatBits(theUnit->DistanceSquaredToTileXY((long)inB, (long)inC));
    case EE_UNITS_CU_CAN_GET_TO: return ::gContinentManager.CanGetTo(theUnit, (long)inB, (long)inC);
    case EE_UNITS_CU_IS_SPACE_MAP: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_CU_CONTINENT_NON_SPACE: return ::gContinentManager.GetContinent(theUnit, kAIPathNonSpace);
    case EE_UNITS_CU_TILE_CONTINENT_NON_SPACE: return ::gContinentManager.GetContinent(EETileCoordinate((long)inB, (long)inC), kAIPathNonSpace);
    case EE_UNITS_CU_CONTINENT_AT: return (uint64_t)(uintptr_t)::gContinentManager.GetContinentAt(theOther);
    case EE_UNITS_CU_CONTINENT_CATEGORY: return EECuObject<EEAIContinent>(inA)->GetCategory();
    case EE_UNITS_CU_MAP_SIZE: return inB == 0 ? (int64_t)gMapWidth : (int64_t)gMapHeight;
    case EE_UNITS_CU_CONTINENT_ID: return ::gContinentManager.GetContinentID((long)inB, (long)inC);
    case EE_UNITS_CU_CONTINENT_BY_ID: return (uint64_t)(uintptr_t)::gContinentManager.GetContinentByID((unsigned short)inB);
    case EE_UNITS_CU_NUM_TOWNS: return theUnit->mPlayer->GetNumTowns();
    case EE_UNITS_CU_TOWN_AT: return (uint64_t)(uintptr_t)theUnit->mPlayer->GetTown(inB);
    case EE_UNITS_CU_TOWN_CONTAINS: return EECuObject<EETown>(inA)->Contains(U2DPoint<long>((long)inB, (long)inC));
    case EE_UNITS_CU_TOWN_DISTANCE: return U2DPoint<long>((long)inB, (long)inC).DistanceToSquared(EECuObject<EETown>(inA)->GetHullClosestOutlinePoint(U2DPoint<long>((long)inB, (long)inC)));
    case EE_UNITS_CU_WAYPOINT_COUNT: return (int64_t)theUnit->mWaypoints.size();
    case EE_UNITS_CU_NEXT_WAYPOINT_POINT: theUnit->GetNextWaypointPoint(sCuWaypoint = GE3DPoint(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CU_SET_HEIGHT_AT_POINT: theUnit->SetHeight(theUnit->GetUnitClass()->GetHeightAt(theUnit, sCuWaypoint)); return 0;
    case EE_UNITS_CU_HAS_SWIM_ANIMATION: return theUnit->HasSwimAnimation();
    case EE_UNITS_CU_IS_SWIMMING: return theUnit->IsSwimming();
    case EE_UNITS_CU_HAS_PARATROOPERS: return ::gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerParatroopers);
    case EE_UNITS_CU_QUEUE_GOAL: theUnit->QueueGoal(EECuObject<EEUnitGoal>(inA), (unsigned long)inB); return 0;
    case EE_UNITS_CU_AREA_EFFECTS_SIZE: return (int64_t)theUnit->mAreaEffects.GetSize();
    case EE_UNITS_CU_AREA_EFFECT_RADIUS: return UFloatBits(theUnit->mAreaEffects.mItems[(size_t)inB]->GetRadius());
    case EE_UNITS_CU_COLLECT_EFFECTS: ::gAEManager.CollectEffects(UBitsFloat(inB), sCuLocation = GE2DPoint(theUnit->mPhysicalModel.GetLocation().GetX(), theUnit->mPhysicalModel.GetLocation().GetY()), (unsigned long)inC, &theUnit->mAreaEffects, theUnit); return 0;
    case EE_UNITS_CU_AFFECTING_KIND: return theUnit->mAffectingAreaEffects.mItems[(size_t)inB]->mTemplate->mKind;
    case EE_UNITS_CU_SET_OUT_EFFECT: *EECuObject<EEGraphicEffect*>(inA) = theUnit->mAffectingAreaEffects.mItems[(size_t)inB]->mTemplate->mGraphicEffect; return 0;
    case EE_UNITS_CU_CITIZEN_FRONT: return (uint64_t)(uintptr_t)theUnit->mCitizenBuildQueue.front();
    case EE_UNITS_CU_CITIZEN_POP_FRONT: theUnit->mCitizenBuildQueue.pop_front(); return 0;
    case EE_UNITS_CU_START_FANATICISM: theUnit->mCloaked = gPhysics->mGameTime + gWorldTable.GetLongValue(kWorldFanaticism); return 0;
    case EE_UNITS_CU_FANATICISM_COST: return UFloatBits(gWorldTable.GetFloatValue(kWorldFanaticism));
    case EE_UNITS_CU_HELD_ERASE: theUnit->mHeldUnits.erase(theUnit->mHeldUnits.begin() + (size_t)inB); return 0;
    case EE_UNITS_CU_HELD_DEREFERENCE: theUnit->mHeldUnits[(size_t)inB]->Dereference(); return 0;
    case EE_UNITS_CU_HELD_CLEAR: theUnit->mHeldUnits.clear(); return 0;
    case EE_UNITS_CU_CITIZEN_FRONT_DEREFERENCE: theUnit->mCitizenBuildQueue.front()->Dereference(); return 0;
    case EE_UNITS_CU_CONTACT_REMOVE_THIS: EECuObject<UListNode<EEComplexUnit*> >(inA)->mData->RemoveContactUnit(theUnit); return 0;
    case EE_UNITS_CU_CONTACT_DEREFERENCE: EECuObject<UListNode<EEComplexUnit*> >(inA)->mData->Dereference(); return 0;
    case EE_UNITS_CU_CONTACTS_CLEAR: theUnit->mContacts.Clear(); return 0;
    case EE_UNITS_CU_CONTACTS_REMOVE: theUnit->mContacts.Remove(EECuObject<UListNode<EEComplexUnit*> >(inA)); return 0;
    case EE_UNITS_CU_AFFECTING_REMOVED_FROM: theUnit->mAffectingAreaEffects.mItems[(size_t)inB]->RemovedFrom(theUnit); return 0;
    case EE_UNITS_CU_DESTROY_CONVERSION_EFFECT: gWorld.mEffectManager->DestroyEffect(sCuHandle = theUnit->mConversionEffect); return 0;
    case EE_UNITS_CU_UNLOAD_CARGO_AT_INDEX: return (uint64_t)(uintptr_t)theUnit->UnloadCargoAt((unsigned long)inB, *EECuObject<const GETransformation>(inA));
    case EE_UNITS_CU_RELOAD_RANGE: return UFloatBits(inB == 0 ? gWorldTable.GetFloatValue(kWorldReloadTimeRandomRange) : gWorldTable.GetFloatValue2(kWorldReloadTimeRandomRange));
    case EE_UNITS_CU_RANDOM_BETWEEN: return UFloatBits(gPhysics->GetRandom(UBitsFloat(inB), UBitsFloat(inC)));
    // cu_life.rs: CopyFrom's
    case EE_UNITS_CU_COPY_PROJECTILE_START: theUnit->mProjectileStart = theOther->mProjectileStart; return 0;
    case EE_UNITS_CU_COPY_LOCATION: theUnit->mPhysicalModel.SetLocation(theOther->mPhysicalModel.GetLocation()); return 0;
    case EE_UNITS_CU_COPY_ANGULAR_VELOCITY: theUnit->mAngularVelocity = theOther->mAngularVelocity; return 0;
    case EE_UNITS_CU_COPY_ORIENTATION: theUnit->mOrientation = theOther->mOrientation; return 0;
    case EE_UNITS_CU_COPY_CONTACT_LOCATION: theUnit->mInitialContactLocation = theOther->mInitialContactLocation; return 0;
    case EE_UNITS_CU_COPY_UNIT_VARIABLES: theUnit->mUnitVariables = theOther->mUnitVariables; return 0;
    case EE_UNITS_CU_PLAYER_IS_HUMAN: return theUnit->GetPlayer()->IsHuman();
    case EE_UNITS_CU_BEHAVIOR_IS: return theUnit->mBehavior == EEUnitBehavior::sTable[inB];
    case EE_UNITS_CU_BEHAVIOR_TABLE: return (uint64_t)(uintptr_t)EEUnitBehavior::sTable[inB];
    case EE_UNITS_CU_UNIT_MODEL_PARTS: return (int64_t)theOther->GetModel()->GetNumParts();
    case EE_UNITS_CU_COPY_ORBITAL_ANGLE: theUnit->mOrbitals[inB]->mAngle = theOther->mOrbitals[inB]->mAngle; return 0;
    case EE_UNITS_CU_COPY_WHEEL_ANGLE: theUnit->mWheels[inB]->mAngle = theOther->mWheels[inB]->mAngle; return 0;
    case EE_UNITS_CU_HAS_CAMOUFLAGE_POWER: return gCivPowersManager.HasPower(theUnit->GetPlayer(), kCivPowerCamouflage);
    case EE_UNITS_CU_CAMOUFLAGE_POWER_SET: return (uint64_t)(uintptr_t)gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    case EE_UNITS_CU_SET_CONTAINS_UNIT: return EECuObject<EEUnitSet>(inA)->Contains(EECuObject<EEComplexUnit>((uint64_t)inB)->GetUnitClass()->GetUnitType());
    case EE_UNITS_CU_COPY_LAST_MAP_POSITION: theUnit->mLastMapPosition = theOther->mLastMapPosition; return 0;
    case EE_UNITS_CU_COPY_U0DC: theUnit->mU0DC = theOther->mU0DC; return 0;
    case EE_UNITS_CU_MOVE_PASSENGERS: EEMovePassengers(theUnit, theOther, theOther->GetPlayer() != theUnit->GetPlayer()); return 0;
    case EE_UNITS_CU_TAKE_OVER_LOAD: EETakeOverLoad(theUnit, theOther); return 0;
    case EE_UNITS_CU_GOAL_KIND_OF: return EECuObject<EEUnitGoal>(inA)->mKind;
    case EE_UNITS_CU_SET_UNLOAD_TRANSPORT: EEUnloadGoalCast(EECuObject<EEUnitGoal>(inA))->SetUnloadTransport(theUnit); return 0;
    case EE_UNITS_CU_GOAL_CLONE: return (uint64_t)(uintptr_t)EECuObject<EEUnitGoal>(inA)->Clone();
    case EE_UNITS_CU_QUEUE_PUSH_GOAL: theUnit->mGoalQueues[(size_t)inB].mGoals.push_back(EECuObject<EEUnitGoal>(inA)); return 0;
    case EE_UNITS_CU_GOAL_SET_QUEUE_OWNER: EECuObject<EEUnitGoal>(inA)->SetOwner(theUnit->mGoalQueues[(size_t)inB].GetUnit()); return 0;
    case EE_UNITS_CU_COPY_WAYPOINTS: theUnit->mWaypoints = theOther->mWaypoints; return 0;
    case EE_UNITS_CU_COPY_BUILD_QUEUE: theUnit->mBuildQueue = theOther->mBuildQueue; return 0;
    case EE_UNITS_CU_SELF_ASSIGN_AREA_EFFECTS: theOther->mAreaEffects = theOther->mAreaEffects; return 0;
    case EE_UNITS_CU_SELF_ASSIGN_AFFECTING: theOther->mAffectingAreaEffects = theOther->mAffectingAreaEffects; return 0;
    case EE_UNITS_CU_REPLACE_CARGO_WITH_THIS: theOther->ReplaceCargo(EECuObject<EEComplexUnit>((uint64_t)inB), theUnit); return 0;
    case EE_UNITS_CU_UNIT_COPY_FROM: theUnit->EEUnit::CopyFrom(EECuObject<RTSUnit>(inA)); return 0;
    // ResetEEComplexUnit's
    case EE_UNITS_CU_RESET_BUILD_QUEUE: theUnit->mBuildQueue.clear(); return 0;
    case EE_UNITS_CU_RESET_CITIZEN_QUEUE: theUnit->mCitizenBuildQueue.clear(); return 0;
    case EE_UNITS_CU_RESET_BOUNDED_PATH_TILES: theUnit->mBoundedPathTiles.clear(); return 0;
    case EE_UNITS_CU_RESET_CARGO: theUnit->mCargo.clear(); return 0;
    case EE_UNITS_CU_RESET_WAYPOINTS: theUnit->mWaypoints.clear(); return 0;
    case EE_UNITS_CU_RESET_GOAL_QUEUES: theUnit->mGoalQueues.clear(); return 0;
    case EE_UNITS_CU_RESET_UNIT_VARIABLES: theUnit->mUnitVariables.clear(); return 0;
    case EE_UNITS_CU_RESIZE_UNIT_VARIABLES: theUnit->mUnitVariables.resize(3, 0); return 0;
    case EE_UNITS_CU_RESET_LAST_MAP_POSITION: theUnit->mLastMapPosition = GE3DPoint(0.0f, 0.0f, 0.0f); return 0;
    case EE_UNITS_CU_RESET_U0DC_X: theUnit->mU0DC.SetX(-10000.0f); return 0;
    case EE_UNITS_CU_RESET_U0DC_Y: theUnit->mU0DC.SetY(-10000.0f); return 0;
    case EE_UNITS_CU_RESET_PROJECTILE_START: theUnit->mProjectileStart = GE3DPoint(0.0f, 0.0f, 0.0f); return 0;
    case EE_UNITS_CU_PM_CLEAR_MODEL: theUnit->mPhysicalModel.SetModel(NULL); return 0;
    case EE_UNITS_CU_PM_SET_SCALE: theUnit->mPhysicalModel.SetScale(UBitsFloat(inB)); return 0;
    case EE_UNITS_CU_PM_RESET_LOCATION: theUnit->mPhysicalModel.SetLocation(GETransformation()); return 0;
    case EE_UNITS_CU_PM_RESET_FRAME: theUnit->mPhysicalModel.SetFrame(0); return 0;
    case EE_UNITS_CU_RESET_PITCH: theUnit->mAngularVelocity.SetPitch(0.0f); return 0;
    case EE_UNITS_CU_RESET_YAW: theUnit->mAngularVelocity.SetYaw(0.0f); return 0;
    case EE_UNITS_CU_RESET_ROLL: theUnit->mAngularVelocity.SetRoll(0.0f); return 0;
    case EE_UNITS_CU_RESET_ORIENTATION: theUnit->mOrientation = theUnit->mAngularVelocity; return 0;
    case EE_UNITS_CU_RESET_BLUE: theUnit->mColor.SetBlue(1.0f); return 0;
    case EE_UNITS_CU_RESET_GREEN: theUnit->mColor.SetGreen(1.0f); return 0;
    case EE_UNITS_CU_RESET_RED: theUnit->mColor.SetRed(1.0f); return 0;
    case EE_UNITS_CU_RESIZE_GOAL_QUEUES: theUnit->mGoalQueues.resize((size_t)inB, EEUnitVectorItem(theUnit)); return 0;
    // FreeNode's
    case EE_UNITS_CU_DELETE_TURRET_ANGLES: delete[] theUnit->mTurretAngles; return 0;
    case EE_UNITS_CU_DELETE_ORBITAL: delete theUnit->mOrbitals[inB]; return 0;
    case EE_UNITS_CU_DELETE_ORBITALS: delete[] theUnit->mOrbitals; return 0;
    case EE_UNITS_CU_DELETE_WHEEL: delete theUnit->mWheels[inB]; return 0;
    case EE_UNITS_CU_DELETE_WHEELS: delete[] theUnit->mWheels; return 0;
    case EE_UNITS_CU_UNIT_FREE_NODE: theUnit->EEUnit::FreeNode(); return 0;
    // ClearGoals'
    case EE_UNITS_CU_QUEUE_ABANDON_FRONT: theUnit->mGoalQueues[(size_t)inB].GetFront()->Abandon(theUnit); return 0;
    case EE_UNITS_CU_QUEUE_POP_FRONT_GOAL: theUnit->mGoalQueues[(size_t)inB].PopFront(); return 0;
    case EE_UNITS_CU_GOAL_SUITS: return EECuObject<EEUnitGoal>(inA)->Suits(theUnit);
    case EE_UNITS_CU_GOAL_START: EECuObject<EEUnitGoal>(inA)->Start(theUnit); return 0;
    // SwapGraphic's
    case EE_UNITS_CU_GRAPHIC_START_FRAME: return (int64_t)EECuObject<EEGraphicObject>(inA)->mStartFrames[inB];
    case EE_UNITS_CU_GRAPHIC_FRAME_COUNT: return (int64_t)EECuObject<EEGraphicObject>(inA)->mFrameCounts[inB];
    case EE_UNITS_CU_GRAPHIC_LAST_FRAME: return (int64_t)EECuObject<EEGraphicObject>(inA)->mLastFrames[inB];
    case EE_UNITS_CU_FLOAT_TO_LONG: return UMFloatToLong(UBitsFloat(inB));
    case EE_UNITS_CU_GRAPHIC_NUM_PARTS: return (int64_t)EECuObject<EEGraphicObject>(inA)->GetNumParts();
    case EE_UNITS_CU_SCRATCH_ANGLES_NEW: sCuAngles = new float[(size_t)inB]; return 0;
    case EE_UNITS_CU_SCRATCH_ANGLE_COPY: sCuAngles[inB] = (unsigned long)inB < (unsigned long)inC ? theUnit->mTurretAngles[inB] : 0.0f; return 0;
    case EE_UNITS_CU_TURRET_ANGLES_ASSIGN: theUnit->mTurretAngles = sCuAngles; return 0;
    case EE_UNITS_CU_GRAPHIC_ORBITAL_COUNT: return (int64_t)EECuObject<EEGraphicObject>(inA)->GetOrbitalCount();
    case EE_UNITS_CU_SCRATCH_ORBITALS_NEW: sCuOrbitals = inB != 0 ? new EEOrbital*[(size_t)inB] : NULL; return 0;
    case EE_UNITS_CU_SCRATCH_ORBITAL_NEW: sCuOrbitals[inB] = new EEOrbital(*EECuObject<EEGraphicObject>(inA)->mOrbitals[inB]); return 0;
    case EE_UNITS_CU_SCRATCH_ORBITAL_KEEP_ANGLE: sCuOrbitals[inB]->mAngle = theUnit->mOrbitals[inB]->mAngle; return 0;
    case EE_UNITS_CU_ORBITALS_ASSIGN: theUnit->mOrbitals = sCuOrbitals; return 0;
    case EE_UNITS_CU_GRAPHIC_WHEEL_COUNT: return (int64_t)EECuObject<EEGraphicObject>(inA)->GetWheelCount();
    case EE_UNITS_CU_SCRATCH_WHEELS_NEW: sCuWheels = inB != 0 ? new EEWheel*[(size_t)inB] : NULL; return 0;
    case EE_UNITS_CU_SCRATCH_WHEEL_NEW: sCuWheels[inB] = new EEWheel(*EECuObject<EEGraphicObject>(inA)->mWheels[inB]); return 0;
    case EE_UNITS_CU_SCRATCH_WHEEL_KEEP_ANGLE: sCuWheels[inB]->mAngle = theUnit->mWheels[inB]->mAngle; return 0;
    case EE_UNITS_CU_WHEELS_ASSIGN: theUnit->mWheels = sCuWheels; return 0;
    // StepAside's
    case EE_UNITS_CU_GATHER_GOAL: return (uint64_t)(uintptr_t)EEGatherGoalCast(theUnit->GetGoal());
    case EE_UNITS_CU_FIND_GATHER_SPOT: return EECuObject<EEUGRust>(inA)->FindGatherSpot(theUnit);
    case EE_UNITS_CU_SCRATCH_TILES_CLEAR: sCuTiles.clear(); return 0;
    case EE_UNITS_CU_SCRATCH_TILES_PUSH: sCuTiles.push_back(EETileCoordinate((long)inB, (long)inC)); return 0;
    case EE_UNITS_CU_SCRATCH_TILES_SHUFFLE: EEAIRandomShuffle(sCuTiles.begin(), sCuTiles.end()); return 0;
    case EE_UNITS_CU_SCRATCH_TILES_SIZE: return (int64_t)sCuTiles.size();
    case EE_UNITS_CU_SCRATCH_TILES_BACK_X: return sCuTiles.back().mX;
    case EE_UNITS_CU_SCRATCH_TILES_BACK_Y: return sCuTiles.back().GetY();
    case EE_UNITS_CU_SCRATCH_TILES_POP: sCuTiles.pop_back(); return 0;
    case EE_UNITS_CU_NEW_LOCATION_GOAL: return (uint64_t)(uintptr_t)EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), false, true, false, 0.0f, false, 0);
    case EE_UNITS_CU_SET_GOAL: theUnit->SetGoal(EECuObject<EEUnitGoal>(inA), inB != 0, (long)inC); return 0;
    // IsNextToTile's
    case EE_UNITS_CU_SCRATCH_POINTS_CLEAR: sCuPoints.clear(); return 0;
    case EE_UNITS_CU_SCRATCH_SQUARE_RING:
        ee_units_uu_square_ring(theUnit->GetTileX(), theUnit->GetTileY(), theUnit->GetUnitClass()->mTileSize, theUnit->GetUnitClass()->mTileSize, 1, &sCuPoints);
        return 0;
    case EE_UNITS_CU_SCRATCH_RING_CORNERS:
        ee_units_uu_ring_corners(0, theUnit->GetTileX(), theUnit->GetTileY(), theUnit->GetUnitClass()->mTileSize, theUnit->GetUnitClass()->mTileSize, 1, &sCuPoints);
        return 0;
    case EE_UNITS_CU_SCRATCH_POINTS_SIZE: return (int64_t)sCuPoints.size();
    case EE_UNITS_CU_SCRATCH_POINT_X: return sCuPoints[(size_t)inB].GetX();
    case EE_UNITS_CU_SCRATCH_POINT_Y: return sCuPoints[(size_t)inB].GetY();
    // UpdateFanaticismColor's
    case EE_UNITS_CU_FANATICISM_PERIOD: return EEGraphicEffect::Get(kGraphicEffectFanaticismEffect)->mIntParam1;
    case EE_UNITS_CU_FANATICISM_PARAM: return UFloatBits(EECuFanaticismParam(EEGraphicEffect::Get(kGraphicEffectFanaticismEffect), inB));
    case EE_UNITS_CU_SET_COLOR: theUnit->SetColor(GEColor(f[0], f[1], f[2])); return 0;
    // GetChecksum's
    case EE_UNITS_CU_GOAL_CHECKSUM: return theUnit->mComplexUnit->GetQueueGoals((unsigned long)inB)[(size_t)inC]->GetOutOfSynchChecksum();
    case EE_UNITS_CU_AFFECTING_COUNT: return (int64_t)theUnit->mAffectingAreaEffects.mItems.size();
    case EE_UNITS_CU_AFFECTING_CHECKSUM: return theUnit->mAffectingAreaEffects.mItems[(size_t)inB]->Checksum();
    case EE_UNITS_CU_AREA_EFFECT_COUNT: return (int64_t)theUnit->mAreaEffects.mItems.size();
    case EE_UNITS_CU_AREA_EFFECT_CHECKSUM: return theUnit->mAreaEffects.mItems[(size_t)inB]->Checksum();
    case EE_UNITS_CU_UNIT_CHECKSUM: return theUnit->EEUnit::GetChecksum((unsigned long)inB);
    // Init's
    case EE_UNITS_CU_UNIT_INIT: theUnit->EEUnit::Init(theClass, EECuObject<EEPlayer>((uint64_t)inB)); return 0;
    case EE_UNITS_CU_INIT_COLLIDABLE: theUnit->mIsCollidable = theClass->mIsCollidable && (!(theClass->mDomains & kMovementAir) || theClass->FliesOver()) && !theClass->mIsProjectile && theClass->mUnitType != kUnitTypeWaypoint; return 0;
    case EE_UNITS_CU_INIT_CAN_BE_HIDDEN: theUnit->mCanBeHidden = (!(theClass->mDomains & kMovementAir) && !theClass->mIsObstacle && !theClass->mIsProjectile && theClass->mMaxVelocity > 0.0f) || theClass->mFamily == kUnitFamilyAABuilding; return 0;
    case EE_UNITS_CU_UM_FLOOR: return UMFloor(UBitsFloat(inB));
    case EE_UNITS_CU_PLACEMENT_COPY: sCuPlacement = *EECuObject<const GETransformation>(inA); return 0;
    case EE_UNITS_CU_PLACEMENT_X: return UFloatBits(sCuPlacement.GetX());
    case EE_UNITS_CU_PLACEMENT_Y: return UFloatBits(sCuPlacement.GetY());
    case EE_UNITS_CU_PLACEMENT_SET_X: sCuPlacement.SetX(UBitsFloat(inB)); return 0;
    case EE_UNITS_CU_PLACEMENT_SET_Y: sCuPlacement.SetY(UBitsFloat(inB)); return 0;
    case EE_UNITS_CU_PM_SET_PLACEMENT: theUnit->mPhysicalModel.SetLocation(sCuPlacement); return 0;
    case EE_UNITS_CU_CREATE_AREA_EFFECT: return (uint64_t)(uintptr_t)gAEManager.CreateAreaEffect((long)inB, theUnit);
    case EE_UNITS_CU_AREA_EFFECTS_ADD: theUnit->mAreaEffects.Add(EECuObject<EEAreaEffect>(inA)); return 0;
    case EE_UNITS_CU_EFFECT_DEREFERENCE: EECuObject<EEAreaEffect>(inA)->Dereference(); return 0;
    // cu_more.rs's (QueueResearch, ResizeTurretAngles, AddCenterOffset, GetEffectHeight, PlaceOnGround, the facings)
    case EE_UNITS_CUM_QUEUE_UNCAPTURED: theUnit->QueueResearchUncaptured((long)inB, inC != 0); return 0;
    case EE_UNITS_CUM_TURRETS_DELETE: delete[] theUnit->mTurretAngles; return 0;
    case EE_UNITS_CUM_TURRETS_NEW: theUnit->mTurretAngles = new float[(unsigned long)inB]; return 0;
    case EE_UNITS_CUM_EFFECT_TOP: theUnit->mPhysicalModel.GetLocation().Apply(theUnit->GetModel()->mModels[theUnit->GetAnimation()]->GetBoundingBox(inB != 0 ? theUnit->mPhysicalModel.GetFrame() : 0).GetTopRightBack(), sCuMorePoint = GE3DPoint(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CUM_POINT_OUT: memcpy(f, &sCuMorePoint.mX, 3 * sizeof(float)); return 0;
    case EE_UNITS_CUM_LOAD_LOCATION: sCuMoreTransformation = theUnit->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_CUM_LOCATION_OUT: sCuMoreTransformation.GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_CUM_SET_YPR: sCuMoreTransformation.SetOrientationYPR(f[0], f[1], f[2]); return 0;
    case EE_UNITS_CUM_APPLY_TRANSFORMATION: theUnit->SetTransformation(sCuMoreTransformation, true); return 0;
    case EE_UNITS_CUM_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), sCuMorePoint = GE3DPoint(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CUM_CLASS_GROUND: theUnit->GetUnitClass()->GroundLocation(sCuMorePoint); return 0;
    case EE_UNITS_CUM_POSITION_AT_POINT: sCuMoreTransformation.SetPosition(sCuMorePoint.GetX(), sCuMorePoint.GetY(), sCuMorePoint.GetZ()); return 0;
    case EE_UNITS_CUM_APPLY_UP: sCuMoreTransformation.Apply(GEVector(0.0f, 1.0f, 0.0f), sCuMoreVector = GEVector(0.0f, 0.0f, 0.0f)); return 0;
    case EE_UNITS_CUM_VECTOR_OUT: f[0] = sCuMoreVector.GetX(), f[1] = sCuMoreVector.GetY(), f[2] = sCuMoreVector.GetZ(); return 0;
    case EE_UNITS_CUM_TERRAIN_PITCH: f[0] = ::gWorld.mMap->GetTerrainPitch(sCuMoreTransformation); return 0;
    case EE_UNITS_CUM_TERRAIN_ROLL: f[0] = ::gWorld.mMap->GetTerrainRoll(sCuMoreTransformation); return 0;
    case EE_UNITS_CUM_SET_LOCATION: theUnit->mPhysicalModel.SetLocation(sCuMoreTransformation); return 0;
    case EE_UNITS_CUM_HAS_MODEL: return EECuObject<EEUnit>(inA)->GetComplexUnit() != NULL && EECuObject<EEUnit>(inA)->GetModel()->mModel != NULL;
    case EE_UNITS_CUM_TOP_POINT_OF: memcpy(f, &EECuObject<EEUnit>(inA)->GetModel()->mTopPoint.mX, 3 * sizeof(float)); return 0;
    case EE_UNITS_CUM_APPLY_OF: EECuObject<EEUnit>(inA)->GetComplexUnit()->mPhysicalModel.GetLocation().Apply(GE3DPoint(f[0], f[1], f[2]), sCuMorePoint); return 0;
    case EE_UNITS_CUM_POSITION_OF: EECuObject<EEUnit>(inA)->GetPosition(sCuMorePoint); return 0;
    }
    return 0;
}

// The C++ calls of the Rust path bodies (ee_sim::units waypoints.rs, ee_sim::astar pm_apply.rs): one statement each
extern "C" int64_t ee_units_wp_host(int32_t inOp, uint64_t inUnit, uint64_t inArg, float* ioPoint)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inUnit;
    EEUnitGoal* theGoal = (EEUnitGoal*)(uintptr_t)inArg;
    switch (inOp) {
    case EE_UNITS_WP_SET_GOAL: theUnit->SetGoal(theGoal, 1, 0); return 0;
    case EE_UNITS_WP_INSERT_GOAL: theUnit->mGoalQueues[0].Insert(1, theGoal); return 0;
    case EE_UNITS_WP_START_ACTION: theUnit->StartAction((unsigned long)inArg); return 0;
    case EE_UNITS_WP_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_WP_IS_HUMAN: return theUnit->GetPlayer()->IsHuman();
    case EE_UNITS_WP_CLEAR_BOUNDED: theUnit->mBoundedPathTiles.clear(); return 0;
    case EE_UNITS_WP_REGION_SIZE: return gEEAIRegionSize;
    case EE_UNITS_WP_GOAL_POINT: { GE3DPoint thePoint = EELocationGoalPoint(theGoal); memcpy(ioPoint, &thePoint.mX, 12); return 0; }
    case EE_UNITS_WP_CLOSEST_TILE: {
        GE3DPoint thePoint(ioPoint[0], ioPoint[1], ioPoint[2]);
        long theX = 0, theY = 0;
        ::gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        return (int64_t)(uint32_t)theX | ((int64_t)(uint32_t)theY << 32);
    }
    }
    return 0;
}

// ---- EEWheel.cpp's bodies (0x23c83a..0x23ceca: the wheels' update and some cdecl unit helpers; units round 11:
// the file is gone)

// (the wheel's construction, destruction and EEIsWheelModel: EEGraphicObject.cpp, which makes the wheels)

// Rolls the wheel back by the unit's travel this tick (speed or turn rate, whichever is the larger share of its top)
// over its scaled radius, in physics actions 0, 4 and 5 above a tenth of the top; wraps the angle by a full turn
void EEWheel::Update(EEComplexUnit* inUnit)
{
    this->mAngle = ee_units_cu_wheel_angle(EeUnitsWheelIn{this->mAngle, inUnit->mPhysicalModel.GetScale(), this->mRadius,
        (uint32_t)inUnit->mCurrentPhysicsAction, inUnit->mVelocity, inUnit->mAngularVelocity.mYaw, inUnit->mStats->GetInverseMaxVelocity(),
        inUnit->mStats->GetInverseMaxAngularVelocity(), inUnit->mStats->GetMaxVelocity(), gPhysics->GetTickSeconds()});   // cu_wheel.rs
}

// Returns the unit's attack animation set: 0xb for the special attack of Roman Legionnaires and capital ships,
// 8 when it fires high over walls, else 0
long EEGetAttackFrameSet(EEComplexUnit* inUnit)
{
    return ee_units_cu_attack_frame_set(inUnit->mIsSpecialAttack, (uint32_t)inUnit->mUnitClass->mUnitType, inUnit->mStats->mCanFireOverWalls,
                                        inUnit->mIsFiringHigh);   // cu_wheel.rs
}

// Replaces a target the unit can't reach: with its position when that is reachable, else with a reachable spot up to
// 9 tiles away, halved toward the target to within half a tile (the unit's position when none is found; Rust)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget) { ee_units_find_landing_point((uint64_t)(uintptr_t)inUnit, (float(*)[3])&ioTarget->mX); }  // ee_sim units landing.rs

// Returns a resource's market price (1..1000) from the world table's rate and the player's trade balance in it;
// 1 lower when inBuy is set, else 1 higher
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy)
{
    float theRate = 1.0f;
    long theTrade = inPlayer->mResourceTrade[inResource - 2];
    switch (inResource) {
    case 2: theRate = gWorldTable.GetFloatValue(kWorldTradeRateFoodWood); break;
    case 3: theRate = gWorldTable.GetFloatValue2(kWorldTradeRateFoodWood); break;
    case 4: theRate = gWorldTable.GetFloatValue(kWorldTradeRateStoneIron); break;
    case 6: theRate = gWorldTable.GetFloatValue2(kWorldTradeRateStoneIron); break;
    }
    return (long)ee_netmsg_trade_price(theRate, theTrade, inBuy);   // the price curve (ee_netmsg execute.rs)
}

// Picks a point 4..12 tiles from the unit along its goal-to-unit heading (+-20 degrees) on its land continent, up to
// 13 tries; returns whether found, else writes its position ((0, 0) when it has no goal)
// cdecl with a real GE2DPoint output: EEComplexUnit::FleeAfterCalamity and 0xbaab pass one (provisional name)
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint) { return ee_units_random_point_ahead((uint64_t)(uintptr_t)inUnit, (float(*)[2])&outPoint->mX); }  // ee_sim units landing.rs

// Returns the unit type of the player's farms: 0x33a once the robotic farm upgrade is researched, else 0x2a
long EEGetFarmType(EEPlayer* inPlayer) { return inPlayer->mTechnologies->Get(kTechRoboticFarmUpgrade)->mIsResearched ? 0x33a : 0x2a; }
