// C:\EEX\Empire Earth\Units\EEUnitClass.cpp (__FILE__ 0x2358b1's string). The unit class base: its `$E` globals, the
// ctor 0x22f317 (copies a dbunits record), dtor 0x2305ee and members, the class factory 0x2358b1 (one inline ctor per
// EEUC* subclass), the EEUC* inline dtors and slots emitted with their vtables (0x2362e3..0x23649e), more EEUnitClass
// members and the EEFOFindNewTarget* functors to 0x23a5bf (EEUnitClassImprovement.cpp follows).
// Range 0x22f272..0x23a5bf. Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEUCUpgradeTable.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"   // the tag points' model chain (EECombatTagPointChain)
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/UResourceFile.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGlobalStorage.h"
#include "lle/FSPersistent.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEAEManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEWheel.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIContinent.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEAnimal.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEDbObject.h"
#include "ee/EEDatabase.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEFOFindNewTargetInMeleeRange.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"

void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

static const float kSoundDistanceFactor = 4.0f;  // 0x447ac4 (Virtual2C)

// the unit classes by type (0x5636ac in the original: 0x367 entries up to sUnitTypes at 0x564448; ee/EEUnit.h)
// (the original's fixed EEUnitClassCount() slots; now one per unit type, sized by CreateUnitClasses: EEUnitClassCount() empty slots until then)
// (constructed here, its storage EEGlobals.cpp's: ee/EEGlobalStorage.h)
EE_CONSTRUCT_GLOBAL_AS(std::vector<EEUnitClass*>, gUnitClassTable, gUnitClassTable, (EEUnitClassCount()));
// (gSpaceTurretHeight, gSpaceStationHeight: EEGlobals.cpp's)

// 0x564448: the unit types registered in gUnitClassTable (Register 0x236a9c push_backs; the vector right after the
// table, guarded dtor bit 1 of 0x563698: a class static).
EE_CONSTRUCT_GLOBAL_AS(std::vector<unsigned long>, EEUnitClassUnitTypes, EEUnitClass::sUnitTypes, );

// (the effects are contiguous, GetEffects' array: EeUnitsUnitClassLinks, its Rust test)

// (0x630c55 ApplyAreaDamage, 0x630fd2 ApplyHit and the damage of one hit: ee_combat damage.rs since the tenth
// round, over the world host in EECombatGoals.cpp)

// (FireShot and its CBXA record: ee_sim units ucn_noted.rs, inline in ee/EEUnit.h)

// Builds the airport's three runway pieces in its facing direction on levelled ground and links them to it
void EEUnitClass::CreateRunways(EEComplexUnit* inUnit, EEPlayer* inPlayer, bool inIsFoundation)
{
    ee_units_lv_create_runways(EEObj(this), EEObj(inUnit), inPlayer->GetPlayerID(), inIsFoundation);   // cls_level.rs
}

// (FindImpactUnit and its CBQR record: ee_sim units ucn_noted.rs, inline in ee/EEUnit.h)

// (FacesPoint and its CBQR record: ucn_noted.rs, inline in ee/EEUnit.h)

// Plays the class's creation effect on the unit
void EEUnitClass::PlayCreationEffect(EEUnit* inUnit)
{
    if (this->mCreationEffect != NULL)
        gWorld.mEffectManager->CreateUnitEffect(inUnit, this->mCreationEffect, NULL, 0, 0, 0);
}

// The order sUnitTypes is sorted in: the unit names (gResource strings of the classes' +0x260); its operator() is
// emitted out of line among the std::sort COMDATs (0x63a19c) (provisional name)
struct EEUnitClassNameLess {
    bool operator()(long inType1, long inType2) const
    {
        UWideString theName1;
        UWideString theName2;
        gResource->GetString(gUnitClassTable[inType1]->mNameStringID, theName1);
        gResource->GetString(gUnitClassTable[inType2]->mNameStringID, theName2);
        return theName1 < theName2;
    }
};

// the closest tile's x and y (not read), the carrier's tag transformation, the area cloak's modifier
static long sMvTile[2];   // x, y
static GETransformation sMvTransformation;
static long sMvInput;
static long sMvRadius;

// The movement bodies' world (ee_units cls_move_host.rs EE_UNITS_CLS_MV_*: the class bodies' host switch passes them
// here, where the path manager's and the screens' headers are); one statement a case (round 8: the compound cases
// split, their choices Rust's)
int64_t EEUnitClassMoveOp(int32_t inOp, EEComplexUnit* inUnit, uint64_t inB, int64_t inC, int64_t inD, EeUnitsClsIo* io)
{
    EEUnitGoal* theGoal = (EEUnitGoal*)(uintptr_t)inB;
    EEComplexUnit* theOther = (EEComplexUnit*)(uintptr_t)inB;
    GE3DPoint& p = *reinterpret_cast<GE3DPoint*>(io->p);
    switch (inOp) {
    case EE_UNITS_CLS_MV_CLOSEST_TILE: return EEObj(::gWorld.mMap->GetClosestTile(&p, &sMvTile[0], &sMvTile[1]));
    case EE_UNITS_CLS_MV_PURSUER: return EEObj(EEGetPursuerOf(inUnit));
    case EE_UNITS_CLS_MV_MAX_VELOCITY: io->p[0] = inC != 0 ? inUnit->GetMaxAngularVelocity() : inUnit->GetMaxVelocity(); return 0;
    case EE_UNITS_CLS_MV_DEREFERENCE_LAST_ATTACKER: inUnit->mLastAttacker->Dereference(); return 0;
    case EE_UNITS_CLS_MV_GOAL_LOCATION: theGoal->GetGoalLocation(p); return 0;
    case EE_UNITS_CLS_MV_GOAL_DESTINATION: theGoal->GetDestination(inUnit, p); return 0;
    case EE_UNITS_CLS_MV_GOAL_FLAG: return theGoal->mB1C;
    case EE_UNITS_CLS_MV_GOAL_B1D: return theGoal->mB1D;
    case EE_UNITS_CLS_MV_AUTO_WAYPOINT: return theGoal->GetIsAutoWaypoint();
    case EE_UNITS_CLS_MV_GROUP_PATH_STATE: return theGoal->GetGroupPathState();
    case EE_UNITS_CLS_MV_GATHER_GOAL: return EEObj(EEGatherGoalCast(theGoal));
    case EE_UNITS_CLS_MV_GATHER_RESOURCE: return EEObj(static_cast<EEUGRust*>(theGoal)->GetGatherResource());
    case EE_UNITS_CLS_MV_PATH_ATTEMPTS: theGoal->mPathAttempts += (long)inC; return 0;
    case EE_UNITS_CLS_MV_SIDESTEP: return theGoal->GetSidestepCount();
    case EE_UNITS_CLS_MV_SET_SIDESTEP: theGoal->SetSidestepCount((unsigned long)inC); return 0;
    case EE_UNITS_CLS_MV_SET_GROUP_PATH_STATE: theGoal->SetGroupPathState((unsigned char)inC); return 0;
    case EE_UNITS_CLS_MV_ATTACK_TARGET: return EEObj(theGoal->GetAttackTarget()->GetComplexUnit());
    case EE_UNITS_CLS_MV_MELEE_RETARGET: {
        EETileCoordinate theTile = inUnit->GetTile();
        EEFOFindNewTargetInMeleeRange theScreen(inUnit, theOther);
        return EEFindNewTarget(&theScreen, (EEUnitGoal*)(uintptr_t)inC, inUnit, &theTile, 1,
                               theOther != NULL ? theOther->mUnitClass->mIsWall : false, 1);
    }
    // the waypoints' operations: Rust (cls_host.rs waypoint_op)
    case EE_UNITS_CLS_MV_DISTANCE_3D: io->f = inUnit->DistanceSquared3D(p); return 0;
    case EE_UNITS_CLS_MV_BOUNDED: inUnit->mBoundedPathTiles.clear(); return 0;
    case EE_UNITS_CLS_MV_PUSH_BOUNDED: inUnit->mBoundedPathTiles.push_back(U2DPoint<long>((long)io->a, (long)io->b)); return 0;
    case EE_UNITS_CLS_MV_CONTACT_DIFFERS: return inUnit->mInitialContactLocation != p;
    case EE_UNITS_CLS_MV_INITIAL_CONTACT: inUnit->mInitialContactLocation = p; return 0;
    // PathToGoal's request: Rust (cls_move_host.rs) but the player's virtual
    case EE_UNITS_CLS_MV_AVOIDS_ENEMIES: return inUnit->mPlayer->AvoidsEnemies(inUnit);
    // the carrier inUnit's tag point inC at the plane's frame (cls_carrier.rs)
    case EE_UNITS_CLS_CR_TAG_LIST: return EEObj(inUnit->GetModel()->mTagPoints[inUnit->mAnimation]);
    case EE_UNITS_CLS_CR_TAG_COUNT: return ((EETagPointList*)(uintptr_t)inB)->mPoints.size();
    case EE_UNITS_CLS_CR_SCRATCH_LOCATION: sMvTransformation = inUnit->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_CLS_CR_SCRATCH_SCALE: EETagPointList::Scale(sMvTransformation, inUnit->GetStats()->GetScale()); return 0;
    case EE_UNITS_CLS_CR_TAG: ((EETagPointList*)(uintptr_t)inD)->mPoints[(size_t)inC]->GetPoint(theOther->mPhysicalModel.GetFrame(), sMvTransformation, p); return 0;
    case EE_UNITS_CLS_CR_SET_ORIENTATION: inUnit->mOrientation = GEOrientation(io->p[0], io->p[1], io->p[2]); return 0;
    case EE_UNITS_CLS_CR_SET_POSITION: sMvTransformation.SetPosition(p); return 0;
    case EE_UNITS_CLS_CR_SET_SCRATCH_TRANSFORMATION: inUnit->SetTransformation(sMvTransformation, true); return 0;
    case EE_UNITS_CLS_CR_SET_SCALE: inUnit->mPhysicalModel.SetScale(io->f); return 0;
    case EE_UNITS_CLS_CR_HAS_LOCATION: return EEGoalHasLocation(theGoal);
    case EE_UNITS_CLS_CR_DECK_GOAL: return EEObj(EEUGRust::NewLocation(&p, false, true, false, 0.0f, false, false));
    case EE_UNITS_CLS_CR_SET_LOCATION_POINT: EESetLocationGoalPoint(theGoal, p); return 0;
    case EE_UNITS_CLS_CR_LOCATION_GOAL_POINT: p = EELocationGoalPoint(inUnit->GetGoal()); return 0;
    case EE_UNITS_CLS_CR_RANDOM: io->f = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_CLS_CR_HEIGHT_AT: io->f = ((EEUnitClass*)(uintptr_t)inB)->GetHeightAt(inUnit, p); return 0;
    // the ground levelling's (cls_level.rs): the mesh and the tile are the handle a
    case EE_UNITS_CLS_LV_MESH_Z: io->f = ((GETerrainMesh*)(uintptr_t)inUnit)->GetZ((unsigned long)inC, (unsigned long)inD); return 0;
    case EE_UNITS_CLS_LV_SET_MESH_Z: ((GETerrainMesh*)(uintptr_t)inUnit)->SetZ((unsigned long)inC, (unsigned long)inD, io->f); return 0;
    case EE_UNITS_CLS_LV_MAP_MESH: return EEObj(&::gWorld.mMap->mTerrainMesh);
    case EE_UNITS_CLS_LV_CREATE_UNIT: return EEObj(((EEUnitClass*)(uintptr_t)inB)->CreateUnit((long)inC, (long)inD, (long)io->a, io->b != 0, inUnit)->mComplexUnit);
    case EE_UNITS_CLS_LV_SET_FACING: inUnit->SetFacing((unsigned long)inC); return 0;
    case EE_UNITS_CLS_LV_CAMERA_OFF: gCameraFollowing = false; return 0;
    // the status ticks' (cls_status.rs)
    case EE_UNITS_CLS_ST_POWER_INTERVAL: return EEGetRules().mCombat.mPowerRegenerationInterval;
    case EE_UNITS_CLS_ST_MINI_MAP: gMiniMap->MoveUnit(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), -10000, -10000); return 0;
    case EE_UNITS_CLS_ST_MINI_MAP_SHOW: gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY()); return 0;
    case EE_UNITS_CLS_ST_RESET_MODIFIERS: sMvInput = sMvRadius = 0; return 0;
    case EE_UNITS_CLS_ST_AREA_CLOAK: inUnit->mAffectingAreaEffects.ApplyModifiers(3, sMvInput, sMvRadius, inUnit); return 0;
    case EE_UNITS_CLS_ST_MODIFIER_RADIUS: return sMvRadius;
    case EE_UNITS_CLS_ST_CAMOUFLAGE_TIMING: io->f = gWorldTable.GetFloatValue(kWorldCamouflageTiming); return 0;
    // round 9 (cls_host.rs EE_UNITS_CLS_R_*: the class bodies' compound reads split; b the object)
    case EE_UNITS_CLS_R_TILE_LOCATION: ::gWorld.mMap->GetTileLocation((long)inC, (long)inD, p); return 0;
    case EE_UNITS_CLS_R_LOAD_GOAL: return EEObj(EELoadTransportGoalCast(inUnit->GetGoal()));
    case EE_UNITS_CLS_R_LOAD_GOAL_TILE: return (int64_t)(uint32_t)static_cast<EEUGRust*>(theGoal)->GetLoadUnitTile().mX | ((int64_t)(uint32_t)static_cast<EEUGRust*>(theGoal)->GetLoadUnitTile().mY << 32);
    case EE_UNITS_CLS_R_ATTACK_GOAL: return EEObj(EEAttackUnitCast(inUnit->GetGoal()));
    case EE_UNITS_CLS_R_ATTACK_TARGET_OF: return EEObj(static_cast<EEUGRust*>(theGoal)->GetAttackTarget());
    case EE_UNITS_CLS_R_HULL_CENTER: return (int64_t)(uint32_t)((EEAIContinent*)(uintptr_t)inB)->GetHullCenter().mX | ((int64_t)(uint32_t)((EEAIContinent*)(uintptr_t)inB)->GetHullCenter().mY << 32);
    case EE_UNITS_CLS_R_GROUND_TARGET: p = static_cast<EEUGRust*>(theGoal)->GetGroundTarget(); return 0;
    case EE_UNITS_CLS_R_PATROL_TARGET: return EEObj(static_cast<EEUGRust*>(theGoal)->GetPatrolTarget());
    case EE_UNITS_CLS_R_EXPLORE_CENTER: static_cast<EEUGRust*>(theGoal)->GetExploreTileCenter(p); return 0;
    case EE_UNITS_CLS_MV_FLEE_GOAL: inUnit->SetGoal(EEUGRust::NewLocation(GE2DPoint(io->p[0], io->p[1]), true, true, false, 0.0f, false, 0), inC != 0, 0); return 0;
    }
    return 0;
}

// Creates a target screen that accepts units closer to the attacker than its current target
EEFOFindNewTargetInMeleeRange::EEFOFindNewTargetInMeleeRange(EEComplexUnit* inUnit, EEUnit* inTarget)
    : mUnit(inUnit), mTarget(inTarget)
{
    this->mRange = EECombatHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
}

// Returns whether a candidate is alive and closer to the attacker than its current target (units round 12: Rust,
// combat/screen.rs ee_units_melee_screen, with its CBFS capture record)
bool EEFOFindNewTargetInMeleeRange::Virtual04(EEComplexUnit* inUnit) { return ee_units_melee_screen(EECombatObj(this->mUnit), EECombatObj(inUnit), (int32_t)inUnit->mHitPoints, this->mRange); }

bool EEFOFindNewTargetInMeleeRange::Virtual08(EEComplexUnit* inUnit) { return ee_units_melee_screen(EECombatObj(this->mUnit), EECombatObj(inUnit), (int32_t)inUnit->mHitPoints, this->mRange); }

// (Attacked and its CBXA record: ucn_noted.rs, inline in ee/EEUnit.h)


extern GEViewport gViewport;
extern bool& gOrthographicView;

// ProduceUnit's tile line (EE_UNITS_UC_LINE_RESET, EE_UNITS_UC_TILE_LINE and its reads)
static std::vector<EETileCoordinate> sUcLine;
static std::vector<EETileCoordinate> sUcTiles9;   // GetContainerContinent's tiles (a local of the original)

// ProduceUnit's ship and flier: outAdjusted = the unit's location at the placement's yaw (no pitch or roll), set
static void EEUcAlignYaw(EEComplexUnit* inUnit, GETransformation& inPlacement, GETransformation& outAdjusted)
{
    GEOrientation theOrientation;
    outAdjusted = inUnit->mPhysicalModel.GetLocation();
    inPlacement.ConvertToOrientation(theOrientation);
    outAdjusted.SetOrientationYPR(theOrientation.GetYaw(), 0.0f, 0.0f);
    inUnit->SetTransformation(outAdjusted, true);
}

// the goal points uc_produce.rs hands in place (three floats)
static_assert(sizeof(GE3DPoint) == 3 * sizeof(float), "a GE3DPoint is three floats");

// Heal's local effect handle (EE_UNITS_UC_RESET_HANDLE, the creations, EE_UNITS_UC_HANDLE)
static EEEffectHandle sUcHandle;

// the creations' and launches' scratch (uc_create.rs, uc_anim.rs): the launch chain, the closest tile,
// the turret part, the three formulas' outputs (each read right after its statement)
static std::vector<float> sUcChain;
static long sUcTile[2];   // x, y
static unsigned long sUcPart;
static EeCombatTimerOut sUcTimer;
static EeCombatHealOut sUcHeal;
static EeCombatRegenerateOut sUcRegen;
// the related classes by uc_media.rs's index (the dtor's order)
// (the related classes' and effects' members and the record's ids in the constructor's order: Rust's,
// ee_sim world unit_class_fields.rs effect_slot, related_slot, record_effect, record_related)
#define UC_LINKS(inClass) static_cast<EeUnitsUnitClassLinks*>(inClass)

template <class T> static T* EEUcObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// the constructor's effects: the record's ids and the class's members, in the original's order (uc_class.rs)
// CreateUnitClasses' dbobjects file
static FSFileSpec* sUcDbFile;

// The C++ of EEUnitClass's Rust bodies (ee_units uc_body.rs; EE_UNITS_UC_*): one statement of the original bodies
// each, in their order; inClass the class the body runs on, inUnit its unit, inA an object, inB and inC values, ioF
// floats in and out
extern "C" int64_t ee_units_uc_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                    float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    // the constructor's (uc_class.rs; the unit is the class record)
    case EE_UNITS_UC_RESEARCH_RESIZE: theClass->mResearchTechnologies.resize((size_t)inB); return 0;
    case EE_UNITS_UC_RESEARCH_SET: theClass->mResearchTechnologies[(size_t)inB] = static_cast<EEUnitClassRecord*>(inUnit)->mAttributes[inB]; return 0;
    case EE_UNITS_UC_SET_BUTTON: theClass->mButton = EEButtonObject::Get(static_cast<EEUnitClassRecord*>(inUnit)->mButton); return 0;
    case EE_UNITS_UC_SET_MODEL: theClass->mModel = (EEModel*)EEMedia::Get(static_cast<EEUnitClassRecord*>(inUnit)->mGraphic); return 0;
    case EE_UNITS_UC_SET_CREATION_SOUND: theClass->mCreationSound = EESoundObject::Get(static_cast<EEUnitClassRecord*>(inUnit)->mCreationSound); return 0;
    case EE_UNITS_UC_SET_ACKNOWLEDGE_SOUND:
        theClass->mAcknowledgeSounds[inB] = EESoundObject::Get(inB == 0 ? static_cast<EEUnitClassRecord*>(inUnit)->mAcknowledgeSound1 : static_cast<EEUnitClassRecord*>(inUnit)->mAcknowledgeSound2);
        return 0;
    case EE_UNITS_UC_SET_SELECTION_SOUND:
        theClass->mSelectionSounds[inB] = EESoundObject::Get(inB == 0 ? static_cast<EEUnitClassRecord*>(inUnit)->mSelectionSound1 : static_cast<EEUnitClassRecord*>(inUnit)->mSelectionSound2);
        return 0;
    case EE_UNITS_UC_SET_DEATH_SOUND: theClass->mDeathSound = EESoundObject::Get(static_cast<EEUnitClassRecord*>(inUnit)->mDeathSound); return 0;
    case EE_UNITS_UC_LOS_WARNING:
        ESDebugOut("EEUnitClass::EEUnitClass: unit %s has an LOS of %f.\n", static_cast<EEUnitClassRecord*>(inUnit)->mName, static_cast<EEUnitClassRecord*>(inUnit)->mLineOfSight);
        return 0;
    case EE_UNITS_UC_COPY_SCRIPT_NAME:
        theClass->mAIScriptName.Copy((const char*)static_cast<EEUnitClassRecord*>(inUnit)->mAIScriptName, 0, strlen((const char*)static_cast<EEUnitClassRecord*>(inUnit)->mAIScriptName), 0, true);
        return 0;
    case EE_UNITS_UC_COPY_NAME:
        theClass->mName.Copy((const char*)static_cast<EEUnitClassRecord*>(inUnit)->mName, 0, strlen((const char*)static_cast<EEUnitClassRecord*>(inUnit)->mName), 0, true);
        return 0;
    case EE_UNITS_UC_FIND_ANIMAL: return EEObj(EEAnimal::Find(static_cast<EEUnitClassRecord*>(inUnit)->mUnitType));
    case EE_UNITS_UC_ANIMAL_ID: return EEUcObject<EEAnimal>(inA)->mAnimalID;
    case EE_UNITS_UC_RECORD_TECH_NODE: return EEObj(gTechTree.Get(static_cast<EEUnitClassRecord*>(inUnit)->mTechnology));
    case EE_UNITS_UC_NODE_EPOCH: return EEUcObject<EETechTreeNode>(inA)->mEpoch;
    case EE_UNITS_UC_NUM_RESEARCH: return theClass->GetNumResearchTechnologies();
    case EE_UNITS_UC_RESEARCH_TECH_NODE: return EEObj(gTechTree.Get(theClass->mResearchTechnologies[(size_t)inB]));
    case EE_UNITS_UC_SET_IMPROVEMENTS: theClass->mImprovements = EEUCUpgradeTable::sTable[static_cast<EEUnitClassRecord*>(inUnit)->mUpgradeTableIndex]; return 0;
    case EE_UNITS_UC_RECORD_EFFECT: return EEObj(EEGraphicEffect::Get(ee_units_uc_record_effect(static_cast<EEUnitClassRecord*>(inUnit), (int32_t)inB)));
    case EE_UNITS_UC_EFFECT_REFERENCE: EEUcObject<EEGraphicEffect>(inA)->Reference(); return 0;
    case EE_UNITS_UC_SCRIPT_NAME_LENGTH: return theClass->mAIScriptName.GetLength();
    case EE_UNITS_UC_SET_AI_SCRIPT: theClass->mAIScript = EEUnitAction::GetScript(theClass->mAIScriptName); return 0;
    case EE_UNITS_UC_SET_CLASS_BEHAVIOR: theClass->mBehavior = EEUnitBehavior::sTable[static_cast<EEUnitClassRecord*>(inUnit)->mBehavior]; return 0;
    // CreateUnitClasses' (uc_class.rs)
    case EE_UNITS_UC_DB_FIND: return EEDbFindFile(gDataPath, kDbObjectsFile, sUcDbFile = NULL);
    case EE_UNITS_UC_VERIFY: ESVERIFY_AT(inB != 0, (long)inC); return 0;
    case EE_UNITS_UC_DB_FILE: return sUcDbFile != NULL;
    case EE_UNITS_UC_DB_LOAD: return gDatabaseObjects.Load(sUcDbFile);
    case EE_UNITS_UC_DB_DELETE: delete sUcDbFile; return 0;
    case EE_UNITS_UC_DB_COUNT: return gDatabaseObjects.GetCount();
    case EE_UNITS_UC_DB_RECORD: return EEObj(gDatabaseObjects.GetEntry((unsigned long)inB)->mRecord);
    case EE_UNITS_UC_TABLE_ASSIGN: gUnitClassTable.assign((size_t)inB, NULL); return 0;
    case EE_UNITS_UC_NEW_CLASS: return EEObj(new EEUnitClass(inB >= 0, EEUcObject<EEUnitClassRecord>(inA), inB >= 0 ? gPhysicsModels[inB] : NULL));
    case EE_UNITS_UC_REGISTER_CLASS: EEUnitClass::RegisterUnitClass(EEUcObject<EEUnitClass>(inA)); return 0;
    case EE_UNITS_UC_REFERENCE_RELATED: (*ee_units_uc_related_slot(UC_LINKS(EEUcObject<EEUnitClass>(inA)), (int32_t)inB))->Reference(); return 0;
    case EE_UNITS_UC_SET_IS_WALL: EEUcObject<EEUnitClass>(inA)->mIsWall = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 9); return 0;
    case EE_UNITS_UC_SET_IS_CYBER: EEUcObject<EEUnitClass>(inA)->mIsCyber = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x38); return 0;
    case EE_UNITS_UC_SET_IS_HORSE: EEUcObject<EEUnitClass>(inA)->mIsHorse = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x6e); return 0;
    case EE_UNITS_UC_SET_IS_HUMAN: EEUcObject<EEUnitClass>(inA)->mIsHuman = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x71); return 0;
    case EE_UNITS_UC_SET_IS_MEDIC: EEUcObject<EEUnitClass>(inA)->mIsMedic = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x81); return 0;
    case EE_UNITS_UC_SET_IS_FLARE: EEUcObject<EEUnitClass>(inA)->mIsFlare = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x8a); return 0;
    case EE_UNITS_UC_SET_IS_BOOBY_TRAP: EEUcObject<EEUnitClass>(inA)->mIsBoobyTrap = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x97); return 0;
    case EE_UNITS_UC_SET_IS_PARATROOPER_BOMBER: EEUcObject<EEUnitClass>(inA)->mIsParatrooperBomber = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x96); return 0;
    case EE_UNITS_UC_SET_SKIPS_LINE_OF_FIRE: EEUcObject<EEUnitClass>(inA)->mSkipsLineOfFireCheck = gUnitSetManager.Contains(EEUcObject<EEUnitClass>(inA)->mUnitType, 0x86); return 0;
    case EE_UNITS_UC_SET_LAND_BUILDING_CLASS: EEUcObject<EEUnitClass>(inA)->mIsLandBuildingClass = EEUcObject<EEUnitClass>(inA)->IsLandBuilding(); return 0;
    case EE_UNITS_UC_DB_CLEAR: gDatabaseObjects.Clear(); return 0;
    case EE_UNITS_UC_SORT_TYPES: std::sort(EEUnitClass::sUnitTypes.begin(), EEUnitClass::sUnitTypes.end(), EEUnitClassNameLess()); return 0;
    // Heal's
    case EE_UNITS_UC_HEAL_GOAL: return (uint64_t)(uintptr_t)EEHealGoalCast(theUnit->GetGoal());
    case EE_UNITS_UC_HEAL_IN_RANGE: return EECombatHealInRange(theUnit, EEUcObject<EEUGRust>(inA)->GetHealTarget());
    case EE_UNITS_UC_EFFECT_KIND: return static_cast<unsigned long>(EEUcObject<EEGraphicEffect>(inA)->mKind);
    case EE_UNITS_UC_HEAL_BETWEEN:
        return ::gWorld.GetEffectManager()->CreateEffectBetweenUnits(theUnit, EEUcObject<EEUGRust>((uint64_t)inB)->GetHealTarget(),
                                                                     EEUcObject<EEGraphicEffect>(inA), &sUcHandle) != 0;
    case EE_UNITS_UC_HEAL_ON_TARGET:
        return ::gWorld.GetEffectManager()->CreateUnitEffect(EEUcObject<EEUGRust>((uint64_t)inB)->GetHealTarget(),
                                                             EEUcObject<EEGraphicEffect>(inA), &sUcHandle, false, 0, 0) != 0;
    case EE_UNITS_UC_RESET_HANDLE: sUcHandle.mValue = 0; return 0;
    case EE_UNITS_UC_HANDLE: return sUcHandle.mValue;
    // ProduceUnit's (uc_produce.rs; inA a class, a goal or a unit)
    case EE_UNITS_UC_EXTRA_POPULATION: return theUnit->GetPlayer()->GetExtraPopulation((long)inB);
    case EE_UNITS_UC_POPULATION_LIMIT: return theUnit->GetPlayer()->GetPopulationLimit(0);
    case EE_UNITS_UC_TOTAL_POPULATION: return theUnit->GetPlayer()->GetTotalPopulation(true, false);
    case EE_UNITS_UC_CLASS_IS_AIRCRAFT: return EEUcObject<EEUnitClass>(inA)->IsAircraft();
    case EE_UNITS_UC_ATC_CARGO: return ee_units_atc_cargo(theUnit->GetPlayer()->mAirTraffic[inB - 1], (int32_t)inC);
    case EE_UNITS_UC_ATC_FREE_WEIGHT: return ee_units_atc_free_cargo_weight(theUnit->GetPlayer()->mAirTraffic[inB - 1], theUnit);
    case EE_UNITS_UC_CREATE_AT:
        return (uint64_t)(uintptr_t)EEUcObject<EEUnitClass>(inA)
            ->CreateUnitAt(*EEUcObject<GETransformation>((uint64_t)inB), theUnit->GetPlayer()->GetPlayerID(), false, theUnit)->mComplexUnit;
    case EE_UNITS_UC_CARRIER_AIRCRAFT: EECreateCarrierAircraft(theOther, -1); return 0;
    case EE_UNITS_UC_IS_HUMAN: return theUnit->GetPlayer()->IsHuman();
    case EE_UNITS_UC_QUEUED_GOAL: return (uint64_t)(uintptr_t)theUnit->GetQueuedGoal((unsigned long)inB);
    // round 9's bodies (uc_rest.rs: EE_UNITS_UC_R_*)
    case EE_UNITS_UC_R_GATHER_RESOURCE: return EEObj(static_cast<EEUGRust*>(EEUcObject<EEUnitGoal>(inA))->GetGatherResource());
    case EE_UNITS_UC_R_CAN_GET_TO_UNIT: return gContinentManager.CanGetToUnit(EEUcObject<EEUnit>(inA), theUnit);
    case EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND: return EEObj(theClass->FindResourceOfKind(theUnit, EEUcObject<EEUnit>(inA), EEUcObject<EEUnit>((uint64_t)inB), (long)(int32_t)UFloatBits(f[0]), (unsigned long)UFloatBits(f[1]), EEUcObject<std::vector<EEUnit*> >((uint64_t)inC), true));
    case EE_UNITS_UC_R_FIND_DROP_LOCATION: EECombatFindDropLocation(theUnit, NULL, *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_R_GOAL_DESTINATION: EEUcObject<EEUnitGoal>(inA)->GetDestination(theUnit, *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_R_PRIEST_DESTINATION: EEGetHelicopterPriestDestination(theUnit, *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_R_LANDING_POINT: EEFindLandingPoint(theUnit, reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_R_HEIGHT_AT: f[0] = theClass->GetHeightAt(theUnit, GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UC_R_SET_HEIGHT: theUnit->SetHeight(f[0]); return 0;
    case EE_UNITS_UC_R_ELEVATION_LAUNCHER: return EEGetRules().mCombat.IsElevationLauncher((long)inB);
    case EE_UNITS_UC_R_ELEVATION_ATTACKER: return EEGetRules().mCombat.IsElevationAttacker((long)inB);
    case EE_UNITS_UC_R_ELEVATION_TARGET: return EEGetRules().mCombat.IsElevationTarget((long)inB);
    case EE_UNITS_UC_R_WORLD_FLOAT: f[0] = gWorldTable.GetFloatValue((EEWorldConstant)inB); return 0;
    case EE_UNITS_UC_R_ELEVATION: f[0] = EECombatElevation(*EEUcObject<EeCombatElevationIn>(inA)); return 0;
    case EE_UNITS_UC_R_SOUND_VOLUME: f[0] = EEUcObject<EESoundObject>(inA)->mVolume; return 0;
    case EE_UNITS_UC_R_COMPUTE_SOUND: EEComputeSoundParameters(f[3], GE3DPoint(f[0], f[1], f[2]), f[0], f[1], f[2], kSoundDistanceFactor); return 0;
    case EE_UNITS_UC_R_IS_SEEN: return EEIsSeen(EEUcObject<EEUnit>(inA));
    case EE_UNITS_UC_R_DOPPLER_PITCH: EEGetDopplerPitch(EEUcObject<EEUnit>(inA), EEUcObject<EESoundObject>((uint64_t)inB), &f[0], &f[1], &f[2]); return 0;
    case EE_UNITS_UC_R_LOAD_INDICATOR: EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false); return 0;
    case EE_UNITS_UC_R_UNLOAD_INDICATOR: EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload(); return 0;
    case EE_UNITS_UC_R_ORBITAL_UPDATE: theUnit->mOrbitals[inB]->Update(); return 0;
    case EE_UNITS_UC_R_RTS_TILE_ADD: EEUcObject<RTSTile>(inA)->mUnits = EEUcObject<RTSTile>(inA)->AddUnit(EEUcObject<EEUnit>((uint64_t)inB), EEUcObject<RTSTile>(inA)->mUnits); return 0;
    case EE_UNITS_UC_R_RTS_TILE_REMOVE: EEUcObject<RTSTile>(inA)->RemoveUnit(EEUcObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_R_CLASS_REFERENCE: EEUcObject<EEUnitClass>(inA)->Reference(); return 0;
    case EE_UNITS_UC_R_CLASS_DEREFERENCE: EEUcObject<EEUnitClass>(inA)->Dereference(); return 0;
    case EE_UNITS_UC_R_TABLE_SET: gUnitClassTable[(size_t)inB] = EEUcObject<EEUnitClass>(inA); return 0;
    case EE_UNITS_UC_R_TYPES_PUSH: EEUnitClass::sUnitTypes.push_back((unsigned long)inB); return 0;
    case EE_UNITS_UC_R_SET_GONE_TIME: theClass->mLastUnitGoneTime = TMGetTime(); return 0;
    case EE_UNITS_UC_R_GROUP_INVERSE_VELOCITY: f[0] = EEUcObject<EEActiveGroup>(inA)->GetInverseMaxVelocity(theUnit); return 0;
    case EE_UNITS_UC_R_CAN_HEAL: return EECanHeal(theUnit, theOther);
    case EE_UNITS_UC_R_CONTINENT_OF: return EEObj(gContinentManager.GetContinentAt(theOther));
    case EE_UNITS_UC_R_TILES_CLEAR: sUcTiles9.clear(); return 0;
    case EE_UNITS_UC_R_SURROUNDING_TILES: theClass->GetSurroundingTiles(theUnit, sUcTiles9, 1, false); return 0;
    case EE_UNITS_UC_R_TILES_SIZE: return sUcTiles9.size();
    case EE_UNITS_UC_R_CONTINENT_AT_TILE: return EEObj(gContinentManager.GetContinentAt(sUcTiles9[(size_t)inB].GetX(), sUcTiles9[(size_t)inB].GetY()));
    case EE_UNITS_UC_R_MAP_TILE_AT_TILE: return EEObj(gWorld.mMap->GetTileAtLocation(sUcTiles9[(size_t)inB]));
    case EE_UNITS_UC_R_CONTINENT_IS_LAND: return EEUcObject<EEAIContinent>(inA)->GetCategory() == kContinentCategoryLand;
    case EE_UNITS_UC_R_EDGE_TILE: return ::gWorld.mMap->IsEdgeTile((long)inB, (long)inC);
    case EE_UNITS_UC_R_VEC_TILES_SIZE: return EEUcObject<std::vector<EETileCoordinate> >(inA)->size();
    case EE_UNITS_UC_R_VEC_TILES_CLEAR: EEUcObject<std::vector<EETileCoordinate> >(inA)->clear(); return 0;
    case EE_UNITS_UC_R_UPGRADE_BUTTON: EEUcObject<EEUnitClassUpgrade>(inA)->mButton = EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mButton; return 0;
    case EE_UNITS_UC_R_UPGRADE_MODEL: EEUcObject<EEUnitClassUpgrade>(inA)->SetModel(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mModel); return 0;
    case EE_UNITS_UC_R_UPGRADE_ACKNOWLEDGE: EEUcObject<EEUnitClassUpgrade>(inA)->SetAcknowledgeSound(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mAcknowledgeSounds[inB], (long)inB); return 0;
    case EE_UNITS_UC_R_UPGRADE_CREATION: EEUcObject<EEUnitClassUpgrade>(inA)->SetCreationSound(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mCreationSound); return 0;
    case EE_UNITS_UC_R_UPGRADE_DEATH: EEUcObject<EEUnitClassUpgrade>(inA)->SetDeathSound(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mDeathSound); return 0;
    case EE_UNITS_UC_R_UPGRADE_SELECTION: EEUcObject<EEUnitClassUpgrade>(inA)->SetSelectionSound(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mSelectionSounds[inB], (long)inB); return 0;
    case EE_UNITS_UC_R_UPGRADE_RANGE: EEUcObject<EEUnitClassUpgrade>(inA)->SetRange(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass->mRange); return 0;
    case EE_UNITS_UC_R_UPGRADE_INIT: ee_units_upgrade_init(EEUcObject<EEUnitClassUpgrade>(inA), EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass); return 0;
    case EE_UNITS_UC_R_UPGRADE_REAPPLY: EEUcObject<EEUnitClassUpgrade>(inA)->mImprovement.Reapply(EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass, EEUcObject<EEUnitClassUpgrade>(inA)); return 0;
    case EE_UNITS_UC_R_UPGRADE_INIT_FINISH: ee_units_upgrade_init_finish(EEUcObject<EEUnitClassUpgrade>(inA), EEUcObject<EEUnitClassUpgrade>(inA)->mUnitClass); return 0;
    case EE_UNITS_UC_GOAL_LOCATION: EEUcObject<EEUnitGoal>(inA)->GetGoalLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_GOAL_KIND: return EEUcObject<EEUnitGoal>(inA)->mKind;
    case EE_UNITS_UC_ATTACK_TARGET: return (uint64_t)(uintptr_t)EEAttackUnitCast(EEUcObject<EEUnitGoal>(inA))->GetAttackTarget();
    case EE_UNITS_UC_POSITION: EEUcObject<EEUnit>(inA)->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_ALIGN_YAW: EEUcAlignYaw(theOther, *EEUcObject<GETransformation>((uint64_t)inB), *EEUcObject<GETransformation>((uint64_t)inC)); return 0;
    case EE_UNITS_UC_AREA_EFFECT_AT: return (uint64_t)(uintptr_t)gAEManager.FindEffectAtTile(8, theOther->GetTileX(), theOther->GetTileY(), theOther->GetPlayer()->GetPlayerID());
    case EE_UNITS_UC_EFFECT_OWNER: return (uint64_t)(uintptr_t)EEUcObject<EEAreaEffect>(inA)->mUnit;
    case EE_UNITS_UC_EFFECT_COVERS:
        return EEUcObject<EEAreaEffect>(inA)->Covers(EEUcObject<EEAreaEffect>(inA)->mUnit->GetPlayer()->GetPlayerID(),
                                                     EEUcObject<EEComplexUnit>((uint64_t)inB),
                                                     GE2DPoint(EEUcObject<EEAreaEffect>(inA)->mUnit->mPhysicalModel.GetLocation().GetX(),
                                                               EEUcObject<EEAreaEffect>(inA)->mUnit->mPhysicalModel.GetLocation().GetY()));
    case EE_UNITS_UC_QUEUE_LEN: return theUnit->GetQueueGoals((unsigned long)inB).size();
    case EE_UNITS_UC_QUEUE_GOAL: return (uint64_t)(uintptr_t)theUnit->GetQueueGoals((unsigned long)inB)[(size_t)inC];
    case EE_UNITS_UC_GOAL_SUITS: return EEUcObject<EEUnitGoal>(inA)->Suits(EEUcObject<EEComplexUnit>((uint64_t)inB));
    case EE_UNITS_UC_ADD_BUILD_TARGET: theOther->AddBuildTarget(EEBuildRepairGoalCast(EEUcObject<EEUnitGoal>((uint64_t)inB))->GetBuildTarget()); return 0;
    case EE_UNITS_UC_GOAL_CLONE: return (uint64_t)(uintptr_t)EEUcObject<EEUnitGoal>(inA)->Clone();
    case EE_UNITS_UC_GOAL_IS_LOAD: return EEUcObject<EEUnitGoal>(inA)->IsLoadGoal();
    case EE_UNITS_UC_GOAL_ADD_CREATED: EEUcObject<EEUnitGoal>(inA)->AddCreatedUnit(EEUcObject<EEComplexUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_LOAD_CONTAINER: return (uint64_t)(uintptr_t)EELoadGarrisonGoalCast(EEUcObject<EEUnitGoal>(inA))->GetLoadContainer();
    case EE_UNITS_UC_ADD_LOAD_UNIT: EELoadGarrisonGoalCast(EEUcObject<EEUnitGoal>(inA))->AddLoadUnit(EEUcObject<EEComplexUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_NEW_LOCATION: return (uint64_t)(uintptr_t)EEUGRust::NewLocation(reinterpret_cast<GE3DPoint*>(f), false, true, false, 0.0f, false, 0);
    case EE_UNITS_UC_GOAL_SET_LOCATION: static_cast<EEUGRust*>(EEUcObject<EEUnitGoal>(inA))->SetLocation(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UC_LINE_RESET: sUcLine.clear(); return 0;
    case EE_UNITS_UC_TILE_LINE: EEGetTileLine(theOther->GetTileX(), theOther->GetTileY(), (long)inB, (long)inC, &sUcLine, 1, 1); return 0;
    case EE_UNITS_UC_LINE_LEN: return (long)sUcLine.size();
    case EE_UNITS_UC_LINE_X: return sUcLine[(size_t)inB].mX;
    case EE_UNITS_UC_LINE_Y: return sUcLine[(size_t)inB].mY;
    case EE_UNITS_UC_CAN_GATHER:
        return theOther->CanGather(EEUcObject<EEUnit>((uint64_t)inB), EEUcObject<EEUnit>((uint64_t)inB)->GetTileX(),
                                   EEUcObject<EEUnit>((uint64_t)inB)->GetTileY(), false, true);
    case EE_UNITS_UC_NEW_GATHER: return (uint64_t)(uintptr_t)EEUGRust::NewGather(EEUcObject<EEUnit>(inA), 0, true);
    case EE_UNITS_UC_DELETE_GOAL: delete EEUcObject<EEUnitGoal>(inA); return 0;
    case EE_UNITS_UC_GATHER_BEHAVIOR: theOther->SetBehavior(EEUnitBehavior::sTable[5]); return 0;
    case EE_UNITS_UC_QUEUE_GOAL_ON: theOther->QueueGoal(EEUcObject<EEUnitGoal>((uint64_t)inB), 0); return 0;
    case EE_UNITS_UC_START_GOAL_ACTION: theOther->StartAction(theOther->mGoalState->GetStartAction()); return 0;
    case EE_UNITS_UC_PLAY_SAMPLE: gSoundManager->Play(EEUcObject<EESoundObject>(inA)->mSampleID, EEUcObject<EESoundObject>(inA)->mVolume, 0.0f, 1.0f, false, NULL); return 0;
    // UpdateBuilding's
    case EE_UNITS_UC_IN_CINEMATIC: return gWorld.mInCinematic;
    case EE_UNITS_UC_RESEARCH_MAKES_UNIT: return theUnit->mPlayer->mTechnologies->Get(theUnit->mCurrentResearch)->mData->mMakesUnit;
    case EE_UNITS_UC_RESEARCH_CLASS: return (uint64_t)(uintptr_t)gUnitClassTable[theUnit->mPlayer->mTechnologies->Get(theUnit->mCurrentResearch)->mData->mUnitClass];
    case EE_UNITS_UC_RESEARCH: theUnit->mPlayer->mTechnologies->Research(theUnit->mCurrentResearch, theUnit); return 0;
    case EE_UNITS_UC_SET_PRODUCTION_TIME: theUnit->SetProductionTime((long)inB); return 0;
    case EE_UNITS_UC_UPDATE_ROTTING: return (long)theUnit->UpdateRottingAwayTime();
    case EE_UNITS_UC_SET_ROTTING: theOther->SetRottingAwayTime((long)inB); return 0;
    case EE_UNITS_UC_CONTINUE_PRODUCTION: theUnit->ContinueProduction(); return 0;
    // UpdateAnimal's
    case EE_UNITS_UC_KEEP_LAST_HIT_POINTS: theUnit->SetLastHitPoints(theUnit->mHitPoints); return 0;
    case EE_UNITS_UC_PHYSICS_STEP: theClass->mPhysicsModel->Step(theUnit); return 0;
    case EE_UNITS_UC_UPDATE_DONE: theClass->UpdateDone(theUnit); return 0;
    // RefreshSound's
    case EE_UNITS_UC_SOUND_NONE: return UIsNone32(gSoundManager->GetInstanceID(theUnit->mSound));
    case EE_UNITS_UC_IN_VIEW: return theUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView);
    case EE_UNITS_UC_SOUND_SAMPLE: return gSoundManager->GetSampleID(theUnit->mSound);
    case EE_UNITS_UC_MODEL_SOUND1: return (uint64_t)(uintptr_t)theUnit->GetModel()->mSounds1[theUnit->GetAnimation()];
    case EE_UNITS_UC_MODEL_SOUND2: return (uint64_t)(uintptr_t)theUnit->GetModel()->mSounds2[theUnit->mAnimation];
    case EE_UNITS_UC_SAMPLE_ID: return EEUcObject<EESoundObject>(inA)->mSampleID;
    case EE_UNITS_UC_SOUND_PARAMETERS: theClass->GetSoundParameters(theUnit, EEUcObject<EESoundObject>(inA), &f[0], &f[1], &f[2]); return 0;
    case EE_UNITS_UC_SET_SOUND_PARAMETERS: gSoundManager->SetSoundParameters(theUnit->mSound, f[0], f[1], f[2]); return 0;
    // CreateDeathUnit's
    case EE_UNITS_UC_CREATE_UNIT: return (uint64_t)(uintptr_t)EEUcObject<EEUnitClass>(inA)->CreateUnit((long)inB, (long)inC, theUnit->mPlayer->GetPlayerID(), false, NULL)->GetComplexUnit();
    case EE_UNITS_UC_NEW_ATTACK_GROUND: return (uint64_t)(uintptr_t)EEUGRust::NewAttackGround(GE3DPoint(f[0], f[1], f[2]), false);
    case EE_UNITS_UC_OWN_LAUNCHER_CLASS: theOther->SetLauncherClass(theOther->GetUnitClass()); return 0;
    case EE_UNITS_UC_SET_PROJECTILE_START: theOther->SetProjectileStart(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UC_PLACED_EFFECT: gWorld.mEffectManager->CreatePlacedEffect(GE3DPoint(f[0], f[1], f[2]), EEUcObject<EEGraphicEffect>(inA), NULL); return 0;
    // the creations', launches' and deaths' (uc_create.rs; inA an object, a transformation's address or a player)
    case EE_UNITS_UC_SIMPLE_CREATE: return EEObj(EEUnit::Create(theClass, (long)inB, (long)inC, EEUcObject<EEPlayer>(inA)));
    case EE_UNITS_UC_GROUND_POINT: EEUcObject<EEMap>(inA)->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_TERRAIN_PITCH: f[0] = EEUcObject<EEMap>(inA)->GetTerrainPitch(*EEUcObject<GETransformation>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_TERRAIN_ROLL: f[0] = EEUcObject<EEMap>(inA)->GetTerrainRoll(*EEUcObject<GETransformation>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_LEVEL_GROUND: theClass->LevelGround(&EEUcObject<EEMap>(inA)->mTerrainMesh, (unsigned long)inB, (unsigned long)inC, &f[0]); return 0;
    case EE_UNITS_UC_COMPLEX_CREATE: return EEObj(EEComplexUnit::Create(theClass, *EEUcObject<GETransformation>(inA), EEUcObject<EEPlayer>((uint64_t)inB), (unsigned long)inC));
    case EE_UNITS_UC_PENDING_PUSH: EEUcObject<EEPlayer>(inA)->mPendingUnits.push_back(EEUcObject<RTSUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_REFERENCE: EEUcObject<RTSUnit>(inA)->Reference(); return 0;
    case EE_UNITS_UC_DEREFERENCE: EEUcObject<RTSUnit>(inA)->Dereference(); return 0;
    case EE_UNITS_UC_FREE_CAMERA: return gFreeCamera;
    case EE_UNITS_UC_CHECK_BUTTONS: EEUcObject<EEPlayer>(inA)->CheckButtonsPreloaded(theClass->mUnitType); return 0;
    case EE_UNITS_UC_SET_PLANET: theOther->mPlanetID = EEGetLandContinentOf(theOther); return 0;
    case EE_UNITS_UC_SET_ORIENTATION: theOther->mOrientation = GEOrientation(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UC_LOAD_CLASS_MEDIA_OF: EEUcObject<EEPlayer>(inA)->LoadUnitClassMedia(theClass->mUnitType); return 0;
    case EE_UNITS_UC_UNIT_LOAD_MEDIA: EEUcObject<EEUnit>(inA)->LoadMedia(inB != 0); return 0;
    case EE_UNITS_UC_ADD_UNIT_ENTRY: ::gWorld.AddUnitEntry(EEUcObject<EEUnit>(inA), EEUcObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_CREATE_RUNWAYS: theClass->CreateRunways(theOther, EEUcObject<EEPlayer>((uint64_t)inB), inC != 0); return 0;
    case EE_UNITS_UC_ATTACHED_EFFECT_ON: gWorld.mEffectManager->CreateUnitEffect(EEUcObject<EEUnit>(inA), theClass->mAttachedEffect, NULL, inB != 0, 0, 0); return 0;
    case EE_UNITS_UC_INSERT_UNLISTED: EEUnitStoreInsertUnlisted(EEUcObject<EEUnit>(inA)); return 0;
    case EE_UNITS_UC_FARM_TYPE: return EEGetFarmType(EEUcObject<EEPlayer>(inA));
    case EE_UNITS_UC_CAN_PRODUCE: return EEUcObject<EEPlayer>(inA)->CanProduce(EEUcObject<EEUnitClass>((uint64_t)inB)->GetTechnology(), 0x20, 1, false, 3, NULL);
    case EE_UNITS_UC_CREATE_UNIT_FOR: return EEObj(EEUcObject<EEUnitClass>(inA)->CreateUnit((int32_t)inB, (int32_t)(inB >> 32), (long)inC, false, NULL));
    case EE_UNITS_UC_LOCATION_MATRIX: *EEUcObject<GETransformation>((uint64_t)inB) = theOther->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_UC_SET_PHYSICAL_LOCATION: theOther->mPhysicalModel.SetLocation(*EEUcObject<GETransformation>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_CREATE_AT_FOR: return EEObj(EEUcObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcObject<GETransformation>((uint64_t)inB), (long)inC, false, NULL));
    case EE_UNITS_UC_CREATE_AT_BY: return EEObj(EEUcObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcObject<GETransformation>((uint64_t)inB), (long)inC, false, theUnit));
    case EE_UNITS_UC_CREATE_UNIT_BY: return EEObj(EEUcObject<EEUnitClass>(inA)->CreateUnit((int32_t)inB, (int32_t)(inB >> 32), (long)inC, false, theUnit));
    case EE_UNITS_UC_CREATION_EFFECT_ON: gWorld.mEffectManager->CreateUnitEffect(EEUcObject<EEUnit>(inA), theClass->mCreationEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_UC_WORLD_CREATION_EFFECT_ON: ::gWorld.GetEffectManager()->CreateUnitEffect(EEUcObject<EEUnit>(inA), theClass->mCreationEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_UC_SET_LAUNCHER: theOther->SetLauncherClass(theClass); return 0;
    case EE_UNITS_UC_NEW_ATTACK_UNIT: return EEObj(EEUGRust::NewAttackUnit(theOther, EEUcObject<EEUnit>((uint64_t)inB), false, false));
    case EE_UNITS_UC_SET_HEIGHT_OF: theOther->SetHeight(f[0]); return 0;
    case EE_UNITS_UC_LAUNCH_PROJECTILE: return EEUcObject<EEUnitClass>(inA)->LaunchProjectile(EEUcObject<EEComplexUnit>((uint64_t)inB), GE3DPoint(f[0], f[1], f[2]));
    case EE_UNITS_UC_TAG_LIST: return EEObj(theUnit->GetModel()->mTagPoints[theUnit->mAnimation]);
    case EE_UNITS_UC_TAG_COUNT: return EEUcObject<EETagPointList>(inA)->GetNumTagPoints();
    case EE_UNITS_UC_CHAIN_RESET: sUcChain.clear(); return 0;
    case EE_UNITS_UC_TAG_CHAIN:
        EECombatTagPointChain(theUnit->GetModel()->mTagPoints[theUnit->mAnimation]->GetTagPoint((unsigned long)inC), theUnit->mPhysicalModel.GetFrame(), EEUcObject<EeCombatLaunchIn>((uint64_t)inB)->point, sUcChain, EEUcObject<EeCombatLaunchIn>((uint64_t)inB)->chain);
        return 0;
    case EE_UNITS_UC_CHAIN_DATA: return EEObj(sUcChain.data());
    case EE_UNITS_UC_CHAIN_CAPTURE: EECombatChainCapture((unsigned int)inA, *EEUcObject<EeCombatLaunchIn>((uint64_t)inB), sUcChain, *EEUcObject<EeCombatLaunchOut>((uint64_t)inC)); return 0;
    case EE_UNITS_UC_GET_TILE: return EEObj(::gWorld.mMap->GetTile((long)inB, (long)inC));
    case EE_UNITS_UC_CLOSEST_TILE: ::gWorld.mMap->GetClosestTile(reinterpret_cast<GE3DPoint*>(f), &(sUcTile[0] = (long)inB), &(sUcTile[1] = (long)inC)); return 0;
    case EE_UNITS_UC_TILE_OUT_X: return sUcTile[0];
    case EE_UNITS_UC_TILE_OUT_Y: return sUcTile[1];
    case EE_UNITS_UC_FLAMING_CLASS: return EEObj(gUnitClassTable[gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mUnitType]);
    case EE_UNITS_UC_CAN_ATTACK_CLASS: return theUnit->CanAttackClass(EEUcObject<EEUnit>(inA)->GetUnitClass());
    case EE_UNITS_UC_MOVE_PASSENGERS: EEMovePassengers(theOther, theUnit, false); return 0;
    case EE_UNITS_UC_FIND_TURRET_PART: return theUnit->GetModel()->FindTurretPart(theUnit->GetModel()->mTagPoints[theUnit->GetAnimation()]->GetTagPoint(theUnit->mWeaponTagPoint)->mModel, sUcPart);
    case EE_UNITS_UC_TURRET_PART: return sUcPart;
    case EE_UNITS_UC_CREATE_DEATH_CLASS_UNIT: theClass->mDeathClass->CreateUnit(theUnit->GetTileX(), theUnit->GetTileY(), ::gWorld.mPlayers[0]->GetPlayerID(), 0, NULL); return 0;
    case EE_UNITS_UC_DEATH_EFFECT_ON: gWorld.mEffectManager->CreateUnitEffect(theUnit, theClass->mDeathEffect, NULL, 0, 0, 0); return 0;
    case EE_UNITS_UC_PLAY_SOUND: gSoundManager->Play(EEUcObject<EESoundObject>(inA)->mSampleID, f[0], f[1], f[2], EEUcObject<EESoundObject>(inA)->mLooping, NULL); return 0;
    case EE_UNITS_UC_SET_PHYSICS_ACTION: theUnit->SetCurrentPhysicsAction((long)inB); return 0;
    case EE_UNITS_UC_NOT_COMMANDABLE: theUnit->SetIsCommandable(false); return 0;
    case EE_UNITS_UC_NEW_LOCATION_2D: return EEObj(EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), false, true, false, 0.0f, false, false));
    case EE_UNITS_UC_WORLD_FLOAT: f[0] = gWorldTable.GetFloatValue((EEWorldConstant)inB); return 0;
    case EE_UNITS_UC_UPDATE_UNIT_TILE: EEUpdateUnitTile(EEUcObject<GETransformation>(inA), theUnit, &sUcTile[0], &sUcTile[1]); return 0;
    case EE_UNITS_UC_SET_HEAL_SEARCH_TIMER: theUnit->SetHealSearchTimer(theClass->mHealDelay); return 0;
    case EE_UNITS_UC_SET_FULL_LAST_HIT_POINTS: theUnit->SetLastHitPoints(theUnit->mStats->mMaxHitPoints); return 0;
    case EE_UNITS_UC_SET_AMMUNITION: theUnit->SetAmmunition(theClass->mAmmunition); return 0;
    case EE_UNITS_UC_SET_BEHAVIOR: theUnit->SetBehavior(theClass->mBehavior); return 0;
    // the per-tick bodies' (uc_anim.rs)
    case EE_UNITS_UC_MODEL_LOOPS: return theUnit->GetModel()->mLoops[inB];
    case EE_UNITS_UC_MODEL_SPEED_SCALED: return theUnit->GetModel()->mIsSpeedScaled[inB];
    case EE_UNITS_UC_MODEL_SHARES_FRAMES: return theUnit->GetModel()->mSharesFrames[inB];
    case EE_UNITS_UC_IS_FOUNDATION: return theUnit->IsFoundation();
    case EE_UNITS_UC_MODEL_HOLD_TIME: return theUnit->GetModel()->mHoldTimes[inB];
    case EE_UNITS_UC_MODEL_START_FRAME: return theUnit->GetModel()->mStartFrames[inB];
    case EE_UNITS_UC_MODEL_LAST_FRAME: return theUnit->GetModel()->mLastFrames[inB];
    case EE_UNITS_UC_MODEL_FRAME_COUNT: return theUnit->GetModel()->mFrameCounts[inB];
    case EE_UNITS_UC_MODEL_FRAME_TIME: return theUnit->GetModel()->mFrameTimes[inB];
    case EE_UNITS_UC_MODEL_SOUND1_FRAME: return theUnit->GetModel()->mSound1Frames[inB];
    case EE_UNITS_UC_MODEL_SOUND2_FRAME: return theUnit->GetModel()->mSound2Frames[inB];
    case EE_UNITS_UC_YAW_VELOCITY: f[0] = theUnit->mAngularVelocity.GetYaw(); return 0;
    case EE_UNITS_UC_SET_PAUSED: theUnit->SetIsAnimationPaused(inB != 0); return 0;
    case EE_UNITS_UC_SET_ANIMATION_TIME: theUnit->SetAnimationTime((unsigned long)inB); return 0;
    case EE_UNITS_UC_STOP_SOUND: gSoundManager->Stop(EEUcObject<SESMInstanceNode>(inA), !gSoundManager->GetIsLooping(EEUcObject<SESMInstanceNode>(inA))); return 0;
    case EE_UNITS_UC_RANDOM: f[0] = gPhysics->GetRandom(); return 0;
    case EE_UNITS_UC_MODEL_TAGS: return EEObj(theUnit->GetModel()->GetTagPoints((unsigned long)inB, (long)inC));
    case EE_UNITS_UC_TIME_OF_DAY: f[0] = gWorld.mTimeOfDay; return 0;
    case EE_UNITS_UC_IN_STORM: return gWorld.mEnvironmentManager->IsInStorm(U2DPoint<long>(theUnit->GetTileX(), theUnit->GetTileY()), false, true);
    case EE_UNITS_UC_IS_SEEN: return EEIsSeen(theUnit);
    case EE_UNITS_UC_HORSE_BREATH: gWorld.mEffectManager->CreateEffectBetweenUnits(theUnit, theUnit, EEGraphicEffect::Get(kGraphicEffectHorseBreathUnits), NULL); return 0;
    case EE_UNITS_UC_MODEL_SOUNDS1_AT: return EEObj(theUnit->GetModel()->mSounds1[inB]);
    case EE_UNITS_UC_MODEL_SOUNDS2_AT: return EEObj(theUnit->GetModel()->mSounds2[inB]);
    case EE_UNITS_UC_SET_NEXT_FRAME: theUnit->SetNextFrame((unsigned long)inB); return 0;
    case EE_UNITS_UC_IS_LOOPING: return gSoundManager->GetIsLooping(EEUcObject<SESMInstanceNode>(inA));
    case EE_UNITS_UC_PLAY_REFERENCED:
        return EEObj(gSoundManager->PlayAndReturnReferencedNode(EEUcObject<EESoundObject>(inA)->GetSampleID(), f[0], f[1], f[2], EEUcObject<EESoundObject>(inA)->GetLooping(), const_cast<char*>((const char*)theClass->mName)));
    case EE_UNITS_UC_NODE_DEREFERENCE: EEUcObject<SESMInstanceNode>(inA)->Dereference(); return 0;
    case EE_UNITS_UC_ATTACK_FRAME_SET: return EEGetAttackFrameSet(theUnit);
    case EE_UNITS_UC_TURRET_PART_COUNT: return theUnit->GetModel()->mTurretParts.size();
    case EE_UNITS_UC_TURRET_PART_POSITION: *reinterpret_cast<GE3DPoint*>(f) = theUnit->GetModel()->mTurretParts[(size_t)inB]->mPosition; return 0;
    case EE_UNITS_UC_TURRET_PART_TRANSFORM: *EEUcObject<GETransformation>(inA) = theUnit->GetModel()->mTurretParts[(size_t)inB]->mTransformation; return 0;
    case EE_UNITS_UC_SET_TURRET_ANGLE: theUnit->mTurretAngles[inB] = f[0]; return 0;
    case EE_UNITS_UC_SET_TRAIL_TIMER: theUnit->SetTrailEffectTimer((long)inB); return 0;
    case EE_UNITS_UC_TRAIL_EFFECT_ON: gWorld.mEffectManager->CreateUnitEffect(theUnit, theClass->mTrailEffect, NULL, 0, 0, 0); return 0;
    case EE_UNITS_UC_RESET_TRAIL_TIMER: theUnit->SetTrailEffectTimer(theClass->mTrailEffect->mTime1); return 0;
    case EE_UNITS_UC_INVERSE_MAX_VELOCITY: f[0] = theUnit->GetInverseMaxVelocity(); return 0;
    case EE_UNITS_UC_RIPPLE_TIME1: return theClass->mRippleEffect->mTime1;
    case EE_UNITS_UC_RIPPLE_TIME2: return theClass->mRippleEffect->mTime2;
    case EE_UNITS_UC_RIPPLE_TAGS: return EEObj(theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0x22));
    case EE_UNITS_UC_TAGS_SIZE: return EEUcObject<EETagPointList>(inA)->mPoints.size();
    case EE_UNITS_UC_SEEN_BY_CURRENT: return EECombatIsSeenBy(theUnit, ::gWorld.GetCurrentPlayer(), true);
    case EE_UNITS_UC_TAGS_SCALE: EEUcObject<EETagPointList>(inA)->Scale(*EEUcObject<GETransformation>((uint64_t)inB), theUnit->mPhysicalModel.GetScale()); return 0;
    case EE_UNITS_UC_TAG_POINT_AT:
        EEUcObject<EETagPointList>(inA)->GetTagPoint((unsigned long)inB)->GetPoint(theUnit->mPhysicalModel.GetFrame(), *EEUcObject<GETransformation>((uint64_t)inC), *reinterpret_cast<GE3DPoint*>(f));
        return 0;
    case EE_UNITS_UC_RIPPLE_EFFECT_AT: gWorld.mEffectManager->CreateTransformationEffect(*EEUcObject<GETransformation>(inA), theClass->mRippleEffect, NULL); return 0;
    case EE_UNITS_UC_COMBAT_TIMER:
        sUcTimer = EECombatTimer(EeCombatTimerIn{(int32_t)theUnit->mRegenerationTimer, (int32_t)gPhysics->mTickTimeMs, (int32_t)EEGetRules().mCombat.mRegenerationInterval});
        return sUcTimer.due;
    case EE_UNITS_UC_TIMER_VALUE: return sUcTimer.timer;
    case EE_UNITS_UC_CARGO_AT: return EEObj(theUnit->GetCargo((unsigned long)inB));
    case EE_UNITS_UC_COMBAT_HEAL:
        sUcHeal = EECombatHeal(EeCombatHealIn{(int32_t)theOther->GetHitPoints(), (int32_t)theUnit->mStats->mHealingPoints, (int32_t)theOther->GetStats()->mMaxHitPoints});
        return sUcHeal.set;
    case EE_UNITS_UC_HEAL_HIT_POINTS: return sUcHeal.hit_points;
    case EE_UNITS_UC_GROUP_CONTAINS: return theUnit->GetPlayer()->mActiveGroup->Contains(theUnit);
    case EE_UNITS_UC_RESET_GROUP_PANEL: gUIManager->GetMainGameForm()->mMainGroup->Reset(); return 0;
    case EE_UNITS_UC_SET_REGENERATION_TIMER: theUnit->SetRegenerationTimer((long)inB); return 0;
    case EE_UNITS_UC_COMBAT_REGENERATE:
        sUcRegen = EECombatRegenerate(EeCombatRegenerateIn{(int32_t)theUnit->mRegenerationTimer, (int32_t)gPhysics->mTickTimeMs, (int32_t)theUnit->mHitPoints, (int32_t)theUnit->mStats->mRegeneration, (int32_t)theUnit->mStats->mMaxHitPoints, (int32_t)EEGetRules().mCombat.mRegenerationInterval});
        return sUcRegen.set;
    case EE_UNITS_UC_REGENERATE_HIT_POINTS: return sUcRegen.hit_points;
    case EE_UNITS_UC_REGENERATE_TIMER: return sUcRegen.timer;
    case EE_UNITS_UC_PLAYER_GROUP_CONTAINS: return theUnit->mPlayer->mActiveGroup->Contains(theUnit);
    case EE_UNITS_UC_IS_PATH_MARKER: return EEUcObject<EEUnit>(inA)->IsPathMarker();
    case EE_UNITS_UC_CLASSES_CLEAR: EEUcObject<std::vector<EEUnitClass*>>(inA)->clear(); return 0;
    case EE_UNITS_UC_SET_CONTAINS: return gUnitSetManager.Contains((unsigned long)inB, (unsigned long)inC) == true;
    case EE_UNITS_UC_CLASSES_PUSH: EEUcObject<std::vector<EEUnitClass*>>(inA)->push_back(EEUcObject<EEUnitClass>((uint64_t)inB)); return 0;
    // the media and lifetime bodies' (uc_media.rs)
    case EE_UNITS_UC_SOUND_LOAD: EEUcObject<EESoundObject>(inA)->LoadMedia(); return 0;
    case EE_UNITS_UC_SOUND_UNLOAD: EEUcObject<EESoundObject>(inA)->UnloadMedia(); return 0;
    case EE_UNITS_UC_MODEL_LOAD: EEUcObject<EEModel>(inA)->LoadMedia(); return 0;
    case EE_UNITS_UC_MODEL_UNLOAD: EEUcObject<EEModel>(inA)->UnloadMedia(); return 0;
    case EE_UNITS_UC_BUTTON_ACQUIRE: EEUcObject<EEButtonObject>(inA)->Acquire(); return 0;
    case EE_UNITS_UC_BUTTON_RELEASE: EEUcObject<EEButtonObject>(inA)->Release(); return 0;
    case EE_UNITS_UC_TECH_TREE: return EEObj(EEUcObject<EEPlayer>(inA)->mTechnologies);
    case EE_UNITS_UC_RESEARCH_COUNT: return theClass->mResearchTechnologies.size();
    case EE_UNITS_UC_RESEARCH_NODE: return EEObj(EEUcObject<EETechTree>(inA)->Get(theClass->mResearchTechnologies[(size_t)inB]));
    case EE_UNITS_UC_PLAYER_IS_HUMAN: return EEUcObject<EEPlayer>(inA)->IsHuman();
    case EE_UNITS_UC_NODE_KIND: return EEUcObject<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_UC_NODE_CALAMITY: return EEUcObject<EETechTreeNode>(inA)->mData->mCalamity;
    case EE_UNITS_UC_CALAMITY_LOAD: USingleton<EECalamityManager>::GetInstance()->LoadMedia(EEUcObject<EEPlayer>(inA), (EECalamityEnum)inB); return 0;
    case EE_UNITS_UC_CALAMITY_UNLOAD: USingleton<EECalamityManager>::GetInstance()->UnloadMedia(EEUcObject<EEPlayer>(inA), (EECalamityEnum)inB); return 0;
    case EE_UNITS_UC_COMMAND_ACQUIRE: EEButtonObject::Get((long)inB)->Acquire(); return 0;
    case EE_UNITS_UC_COMMAND_RELEASE: EEButtonObject::Get((long)inB)->Release(); return 0;
    case EE_UNITS_UC_NODE_BUTTON_ACQUIRE: EEUcObject<EETechTreeNode>(inA)->mButton->Acquire(); return 0;
    case EE_UNITS_UC_NODE_BUTTON_RELEASE: EEUcObject<EETechTreeNode>(inA)->mButton->Release(); return 0;
    case EE_UNITS_UC_SET_CLASS_MEDIA: EEUcObject<EEPlayer>(inA)->SetUnitClassMedia((long)inB, 0x7f); return 0;
    case EE_UNITS_UC_UNLOAD_CLASS_MEDIA: EEUcObject<EEPlayer>(inA)->UnloadUnitClassMedia((long)inB); return 0;
    case EE_UNITS_UC_EFFECT_LOAD_ID: EEGraphicEffect::Get((long)inB)->Load(false); return 0;
    case EE_UNITS_UC_EFFECT_UNLOAD_ID: EEGraphicEffect::Get((long)inB)->Unload(); return 0;
    case EE_UNITS_UC_EFFECT_LOAD: EEUcObject<EEGraphicEffect>(inA)->Load(false); return 0;
    case EE_UNITS_UC_EFFECT_UNLOAD: EEUcObject<EEGraphicEffect>(inA)->Unload(); return 0;
    case EE_UNITS_UC_EFFECT_DEREFERENCE: EEUcObject<EEGraphicEffect>(inA)->Dereference(); return 0;
    case EE_UNITS_UC_SET_MEDIA_LOADED: theClass->mIsMediaLoaded = inB != 0; return 0;
    case EE_UNITS_UC_CLASS_DEREFERENCE: EEUcObject<EEUnitClass>(inA)->Dereference(); return 0;
    }
    return 0;
}
