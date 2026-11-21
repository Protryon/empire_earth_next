// C:\EEX\Empire Earth\Scenario Manager\EESMObject.cpp (file name inferred: no __FILE__): 0x1c5f73..0x1ca4ee,
// scenario objects and triggers (with EESMTConditionWrapper.cpp and EESMTEffectWrapper.cpp folded in).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMObject.h"
#include <algorithm>
#include "ee/EESMTrigger.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"

#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPlayer.h"
#include "ee/EESMArea.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECombatGoals.h"
#include "rust/sim/units.h"
#include "ee/EESMCondition.h"
// 0x246ea2 (cdecl, 5 arguments; EEFOFindNewTargetNearOldTarget.cpp): the squared distance from inUnit to tile
// (inX, inY), the unit's nearest tile out (provisional name)
float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);

// Creates an object named None that picks units on the map, owned by and selected by player 7, with no filters
// (the numbers: ee_scenario fields.rs object_defaults)
EESMObject::EESMObject()
{
    gResource->GetString(IDS_NONE, this->mName);
    this->mUnits.clear();
    *static_cast<EeScenarioObjectFields*>(this) = ee_scenario_object_defaults();
}

// Destroys the object (members only)
EESMObject::~EESMObject()
{
}

// Copies another object's name and filter settings, and its unit list when it picks units on the map
void EESMObject::Copy(const EESMObject& inObject)
{
    this->mName = inObject.mName;
    *static_cast<EeScenarioObjectFields*>(this) = inObject;   // every number
    if (this->mIsSelectOnMap)
        this->mUnits = inObject.mUnits;
}

// Rebuilds a dynamic object's units (parent or owners), keeps those passing the state/attribute/selected/visible/area/
// near filters, shuffles and caps them; returns whether any pass (visibility uses the selected-by player)
bool EESMObject::Evaluate(bool inIncludeDead)
{
    if (!EESimCaptureRecord::Active())
        return this->EvaluateUncaptured(inIncludeDead);
    EESMCaptureObjectState theBefore = EESMCaptureObjectBegin(this);
    bool theResult = this->EvaluateUncaptured(inIncludeDead);
    EESMCaptureObjectEnd(this, inIncludeDead, theResult, theBefore);
    return theResult;
}

// Evaluate's body (Evaluate adds the simulation capture's SMOB record: ee/EEScenarioCapture.h): ee_scenario objeval.rs
// over the object's list and the world (EESMObjectRead / EESMObjectWrite below)
bool EESMObject::EvaluateUncaptured(bool inIncludeDead)
{
    EESMHostContext theContext = {NULL, NULL, NULL, this, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    return ee_scenario_object_evaluate(&theHost, this, inIncludeDead);
}

namespace {

// a unit id handed over as 32 bits (ee_scenario objeval.rs), as the list holds it
EEUnit* SMUnit(int32_t inID)
{
    return gWorld.GetUnit((unsigned long)(uint32_t)inID);
}

// a chain unit for objeval.rs: its id low, its class's type high; -1 at the end
int64_t SMChainUnit(EEUnit* inUnit)
{
    return inUnit == NULL ? -1 : (int64_t)(uint32_t)inUnit->GetUnitID() | ((int64_t)(uint32_t)inUnit->mUnitClass->mUnitType << 32);
}

// two longs as one read: x in the low 32 bits, y in the high (ee_scenario area.rs unpack)
int64_t SMPack(long inX, long inY)
{
    return (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32);
}

int64_t SMFloat(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

}   // namespace

// Evaluate's and FindPlacement's reads, each the original's expression (objeval.rs EE_SCENARIO_READ_OBJ_* and the unit
// reads, placement.rs)
int64_t EESMObjectRead(EESMHostContext* inContext, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, int32_t inD)
{
    EESMObject* theObject = inContext->mObject;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    switch (inOp) {
    case EE_SCENARIO_READ_OBJ_UNIT_COUNT: return (int64_t)theObject->mUnits.size();
    case EE_SCENARIO_READ_OBJ_UNIT_AT: return (int64_t)(uint32_t)theObject->GetUnitAt((unsigned long)inA);
    case EE_SCENARIO_READ_OBJ_EVALUATE: return theManager->GetScenarioObject((unsigned long)inA)->Evaluate(inB != 0);
    case EE_SCENARIO_READ_UNIT_CLASS_TYPE: return SMUnit(inA)->mUnitClass->mUnitType;
    case EE_SCENARIO_READ_UNIT_PLAYER: return SMUnit(inA)->mPlayer->GetPlayerID();
    case EE_SCENARIO_READ_UNIT_COMMAND: return SMUnit(inA)->mComplexUnit->GetCommandType();
    case EE_SCENARIO_READ_UNIT_ATTRIBUTE:
        return theObject->GetUnitAttribute((unsigned long)inB, inC != 0, SMUnit(inA), 0, 0);
    case EE_SCENARIO_READ_UNIT_SEEN_BY: return EECombatIsSeenBy(SMUnit(inA), gWorld.mPlayers[inB], false);
    case EE_SCENARIO_READ_MAP_SIZE: return SMPack((long)gWorld.mMap->mL195610, (long)gWorld.mMap->mL195614);
    case EE_SCENARIO_READ_AREA_CONTAINS_TILE:
        return theManager->GetArea((unsigned long)inA)->ContainsTile(U2DPoint<long>(inB, inC));
    case EE_SCENARIO_READ_OBJECT_UNIT_COUNT: return (int64_t)theManager->GetScenarioObject((unsigned long)inA)->mUnits.size();
    case EE_SCENARIO_READ_OBJECT_UNIT_AT:
        return (int64_t)(uint32_t)theManager->GetScenarioObject((unsigned long)inA)->mUnits[(size_t)inB];
    case EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK: return theManager->GetScenarioObject((unsigned long)inA)->mSelectedByCheck;
    case EE_SCENARIO_READ_UNIT_TILE_OF: return SMPack(SMUnit(inA)->mTileX, SMUnit(inA)->mTileY);
    case EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT: return SMFloat(SMUnit(inA)->mComplexUnit->GetLineOfSight());
    case EE_SCENARIO_READ_UNIT_RANGE: return SMFloat(SMUnit(inA)->mStats->mRange);
    case EE_SCENARIO_READ_UNIT_TILE_SIZE: return SMUnit(inA)->mUnitClass->mTileSize;
    case EE_SCENARIO_READ_CLASS_TILE_SIZE: return gUnitClassTable[inA]->mTileSize;
    case EE_SCENARIO_READ_BLOCK_TAKES: return ee_units_uu_block_takes(0, inA, inB, inC, inD, inD, 0);
    case EE_SCENARIO_READ_MAP_TILE_EXISTS: return gWorld.mMap->GetTile(inA, inB) != NULL;
    case EE_SCENARIO_READ_UNIT_CAN_REACH_TILE:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->CanReachTile(inB, inC);
    case EE_SCENARIO_READ_UNIT_IN_REACH:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->IsInReach(SMUnit(inB));
    }
    return 0;
}

// the object reads' single statements (ee_scenario object_reads.rs EE_SCENARIO_WZ_*: the Rust composes the
// sequences of Evaluate's, FindPlacement's and the conditions' reads), on handles; through the world channel
static_assert(kTerrainDeepWater == 1 && kTerrainShallowWater == 10, "object_reads.rs's numbers");
UWideString sWZText[2];        // the chat test's two texts (the original's locals)
extern U2DPoint<long> sWZPoint;   // the centres' point (EEScenarioHost.cpp)
extern std::vector<U2DPoint<long> > sWZTiles;   // the host's tile list (EEScenarioHost.cpp)
float SMEFloatBits(int64_t inBits) { float theValue; uint32_t theBits = (uint32_t)inBits; memcpy(&theValue, &theBits, 4); return theValue; }
template<class T> T* SMWzOf(int64_t inHandle) { return (T*)(intptr_t)inHandle; }
int64_t SMWzPtr(const void* inObject) { return (int64_t)(intptr_t)inObject; }
int64_t SMEHostWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a);
int64_t SMEObjectWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_WZ_UNIT: return SMWzPtr(SMUnit((int32_t)a[0]));
    case EE_SCENARIO_WZ_UNIT_HIT_POINTS: return SMWzOf<EEUnit>(a[0])->GetHitPoints();
    case EE_SCENARIO_WZ_UNIT_IS_TREE: return SMWzOf<EEUnit>(a[0])->mUnitClass->mIsTree;
    case EE_SCENARIO_WZ_UNIT_COMPLEX: return SMWzOf<EEUnit>(a[0])->mComplexUnit != NULL;
    case EE_SCENARIO_WZ_UNIT_CARRIED: return SMWzOf<EEUnit>(a[0])->mComplexUnit->mPreviousPart != NULL;
    case EE_SCENARIO_WZ_PLAYER_SLOT: return SMWzPtr(gWorld.mPlayers[a[0]]);
    case EE_SCENARIO_WZ_GROUP_CONTAINS: return SMWzOf<EEPlayer>(a[0])->mActiveGroup->Contains(SMWzOf<EEUnit>(a[1])->mComplexUnit);
    case EE_SCENARIO_WZ_VIEWED_UNIT: return SMWzOf<EEPlayer>(a[0])->mViewedUnit == SMWzOf<EEUnit>(a[1]);
    case EE_SCENARIO_WZ_FIRST_UNIT: return SMWzPtr(SMWzOf<EEPlayer>(a[0])->mFirstUnit);
    case EE_SCENARIO_WZ_NEXT_UNIT: return SMWzPtr(static_cast<EEUnit*>(SMWzOf<EEUnit>(a[0])->mNext));
    case EE_SCENARIO_WZ_CURSOR: return SMWzPtr(inContext->mCursor);
    case EE_SCENARIO_WZ_SET_CURSOR: inContext->mCursor = SMWzOf<EEUnit>(a[0]);
        return 0;
    case EE_SCENARIO_WZ_CHAIN_UNIT: return SMChainUnit(SMWzOf<EEUnit>(a[0]));
    case EE_SCENARIO_WZ_AREA: return SMWzPtr(gWorld.mScenarioManager->GetArea((unsigned long)a[0]));
    case EE_SCENARIO_WZ_AREA_CONTAINS_UNIT: return SMWzOf<EESMArea>(a[0])->ContainsUnit(SMWzOf<EEUnit>(a[1]));
    case EE_SCENARIO_WZ_TILE: return SMWzPtr(gWorld.mMap->GetTile((long)a[0], (long)a[1]));
    case EE_SCENARIO_WZ_TILE_TERRAIN: return SMWzOf<EEMapTile>(a[0])->mTerrainType;
    case EE_SCENARIO_WZ_FOOTPRINT_DISTANCE:
        return SMFloat(EEGetFootprintDistance(static_cast<EEComplexUnit*>(SMWzOf<EEUnit>(a[0])), (long)a[1], (long)a[2], SMWzOf<long>(a[3]), SMWzOf<long>(a[4])));
    case EE_SCENARIO_WZ_NEAREST_TILE: SMWzOf<EEUnit>(a[0])->GetNearestFootprintTile(SMWzOf<EEUnit>(a[1]), SMWzOf<long>(a[2]), SMWzOf<long>(a[3]));
        return 0;
    case EE_SCENARIO_WZ_CHAT_TEXT: sWZText[0] = inContext->mCondition->mChatText;
        return 0;
    case EE_SCENARIO_WZ_CHAT_LAST: sWZText[1] = gWorld.mScenarioManager->mLastChatText;
        return 0;
    case EE_SCENARIO_WZ_TEXT_LOWER: PlatMbsLwr((unsigned char*)(const char*)sWZText[a[0]]);
        return 0;
    case EE_SCENARIO_WZ_TEXT_FOUND: return sWZText[1].Find(sWZText[0], 0) != sWZText[1].GetLength();
    case EE_SCENARIO_WZ_POINT_X: return sWZPoint.mX;
    case EE_SCENARIO_WZ_POINT_Y: return sWZPoint.mY;
    case EE_SCENARIO_WZ_PREVIOUS_X: return (long)inContext->mUnit->GetComplexUnit()->mU0DC.GetX();
    case EE_SCENARIO_WZ_PREVIOUS_Y: return (long)inContext->mUnit->GetComplexUnit()->mU0DC.GetY();
    case EE_SCENARIO_WZ_UNIT_VARIABLE: return inContext->mUnit->GetComplexUnit()->mUnitVariables[(size_t)a[0]];
    case EE_SCENARIO_WZ_UNIT_SCALE: return SMFloat(inContext->mUnit->GetComplexUnit()->mPhysicalModel.GetScale());
    case EE_SCENARIO_WZ_TILES_CLEAR: sWZTiles.clear();
        return 0;
    case EE_SCENARIO_WZ_TILES_AREA: gWorld.mScenarioManager->GetArea((unsigned long)a[0])->GetTiles(sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_NEAR_POINT: ee_units_uu_get_tiles_near_point(SMWzOf<const float[3]>(a[0]), SMEFloatBits(a[1]), &sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_COUNT: return (int64_t)sWZTiles.size();
    case EE_SCENARIO_WZ_TILE_X: return sWZTiles[(size_t)a[0]].mX;
    case EE_SCENARIO_WZ_TILE_Y: return sWZTiles[(size_t)a[0]].mY;
    }
    return SMEHostWorld(inContext, inOp, a);   // the continents' and towns' (EEScenarioHost.cpp)
}

// Evaluate's changes of the object's list (objeval.rs EE_SCENARIO_WRITE_OBJ_*)
void EESMObjectWrite(EESMHostContext* inContext, uint32_t inOp, int64_t inB)
{
    std::vector<unsigned long>& theUnits = inContext->mObject->mUnits;
    switch (inOp) {
    case EE_SCENARIO_WRITE_OBJ_CLEAR: theUnits.clear(); break;
    case EE_SCENARIO_WRITE_OBJ_COPY_PARENT:
        gWorld.mScenarioManager->GetScenarioObject((unsigned long)inB)->GetUnits(theUnits);
        break;
    case EE_SCENARIO_WRITE_OBJ_ERASE: theUnits.erase(theUnits.begin() + (size_t)inB); break;
    case EE_SCENARIO_WRITE_OBJ_PUSH: theUnits.push_back((unsigned long)(uint32_t)inB); break;
    case EE_SCENARIO_WRITE_OBJ_SHUFFLE: EEAIRandomShuffle(theUnits.begin(), theUnits.end()); break;
    case EE_SCENARIO_WRITE_OBJ_TRUNCATE: theUnits.erase(theUnits.begin() + (size_t)inB, theUnits.end()); break;
    }
}

// Picks the unit type to create for the object: the parent's first unit's type, the object's class, or a random
// class matching its epoch/family/AI kind/unit set filter; -1 if none
long EESMObject::PickUnitType()
{
    EESMHostContext theContext = {NULL, NULL};   // ee_scenario object.rs pick_unit_type
    EeScenarioHost theHost = EESMHost(&theContext);
    return ee_scenario_object_pick_unit_type(&theHost, this);
}

// Finds a free spot for a new unit of the type that meets the object's area and nearness filters (random tries for
// 'not in/near' filters, then one retry without them); returns whether one was found
bool EESMObject::FindPlacement(unsigned long inUnitType, GE2DPoint& outLocation, bool inTryRandom)
{
    if (!EESimCaptureRecord::Active())
        return this->FindPlacementUncaptured(inUnitType, outLocation, inTryRandom);
    unsigned int theSeed = EESMCaptureSeed();
    bool theResult = this->FindPlacementUncaptured(inUnitType, outLocation, inTryRandom);
    EESMCapturePlacement(this, inUnitType, inTryRandom, theResult, theResult ? outLocation.mX : 0.0f,
                         theResult ? outLocation.mY : 0.0f, theSeed);
    return theResult;
}

// FindPlacement's body (FindPlacement adds the simulation capture's SMFP record: ee/EEScenarioCapture.h): ee_scenario
// placement.rs find_placement over the area's record; the retry without random tries goes through FindPlacement
bool EESMObject::FindPlacementUncaptured(unsigned long inUnitType, GE2DPoint& outLocation, bool inTryRandom)
{
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mArea);
    EeScenarioAreaRecord theRecord = theArea != NULL ? EESMAreaRecord(*theArea) : EeScenarioAreaRecord();
    EESMHostContext theContext = {};
    EeScenarioHost theHost = EESMHost(&theContext);
    float theLocation[2] = {outLocation.mX, outLocation.mY};
    uint32_t theResult = ee_scenario_find_placement(&theHost, this, theArea != NULL ? &theRecord : NULL,
                                                    (uint32_t)inUnitType, inTryRandom, &theLocation);
    outLocation.mX = theLocation[0];
    outLocation.mY = theLocation[1];
    if (theResult == EE_SCENARIO_PLACEMENT_RETRY)
        return this->FindPlacement(inUnitType, outLocation, false);
    return theResult == EE_SCENARIO_PLACEMENT_FOUND;
}

// Returns a unit's attribute for an object filter: current values, upgrade bonuses over the class, or totals,
// optionally as a percentage of the class or upgrade value; 0 for a NULL unit
long EESMObject::GetUnitAttribute(unsigned long inAttribute, bool inPercent, EEUnit* inUnit,
    long inArg4, long inArg5)
{
    if (inUnit == NULL)
        return 0;
    EESMHostContext theContext = {NULL, inUnit, NULL, NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);   // ee_scenario attribute.rs unit_attribute
    return ee_scenario_unit_attribute(&theHost, (uint32_t)inAttribute, inPercent);
}


// Copies the object's unit ids out
void EESMObject::GetUnits(std::vector<unsigned long>& outUnits) const
{
    outUnits = this->mUnits;
}

// Reads the object's units (appended) and filter settings (files before 0.0xcd: attribute ids above Construction
// Complete move up by 3); returns the first error (ee_scenario save.rs object_load)
long EESMObject::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName};
    EESMStream theStream = {&inFile, theStrings, &this->mUnits};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_object_load(&theTable, this, inMajorVersion, inMinorVersion);
}

// Replaces every occurrence of a unit id in the object's unit list by a new id
void EESMObject::ReplaceUnit(long inOldUnit, long inNewUnit)
{
    long theCount = this->mUnits.size();
    for (long i = 0; i < theCount; i++) {
        unsigned long& theUnit = this->mUnits[i];
        if (theUnit == inOldUnit)
            theUnit = inNewUnit;
    }
}

// Writes the object's units and filter settings (the parent only for minor version >= 0x2a); returns the first error
// (ee_scenario save.rs object_save)
long EESMObject::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName};
    EESMStream theStream = {&outFile, theStrings, &this->mUnits};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_object_save(&theTable, this, inMinorVersion);
}

// Adds to, subtracts from or sets one unit's attribute (hit points, cargo, flight time, garrison, ammo, scale, power,
// rot time, built state, variables) by a fixed, random or percentage amount
// Original cdecl helper; random range and the percentage flag are independent of add/subtract/assign mode.
static void EEChangeUnitUncaptured(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode);
void EEChangeUnit(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode)
{
    unsigned int theSeed = EESimCaptureRecord::Active() ? EESMCaptureSeed() : 0;
    long theID = inUnit != NULL ? (long)inUnit->mUnitID : -1;   // the unit by id after (a kill may free it)
    EEChangeUnitUncaptured(inAttribute, inPercentage, inUnit, inMinimum, inMaximum, inRandom, inMode);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY: SMCU (ee/EEScenarioCapture.h)
        long theArgs[7] = {(long)inAttribute, inPercentage, theID, inMinimum, (long)inMaximum, inRandom, inMode};
        EESMCaptureUnitChange(0, theArgs, theID >= 0 ? gWorld.GetUnit((unsigned long)theID) : NULL, NULL, NULL, theSeed);
    }
}

// EEChangeUnit's body (EEChangeUnit adds the simulation capture's SMCU record): the draws, the clamps and which value
// changes are ee_scenario unit_change.rs's, the unit's reads and writes EESMEffect.cpp's EE_SCENARIO_WORLD_CU_*
static void EEChangeUnitUncaptured(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode)
{
    EESMHostContext theContext = {NULL, inUnit};
    EeScenarioHost theHost = EESMHost(&theContext);
    ee_scenario_change_unit(&theHost, inAttribute, inPercentage, inMinimum, inMaximum, inRandom, inMode);
}

// Renames the object and renames its object conditions to "<name> Exists" (ee_scenario lists.rs)
void EESMObject::SetName(const UWideString& inName)
{
    this->mName = inName;
    if (gWorld.mScenarioManager != NULL)
        gWorld.mScenarioManager->ObjectChanged(this, false, 0);
}

// Sets the parent object; a parent chain that loops back to this object resets the parent to 0 (ee_scenario
// lists.rs set_parent)
void EESMObject::SetParent(long inParent)
{
    this->mParent = inParent;
    if (gWorld.mScenarioManager != NULL)
        gWorld.mScenarioManager->ObjectChanged(this, true, inParent);
}

// Replaces the object's unit list
void EESMObject::SetUnits(const std::vector<unsigned long>& inUnits)
{
    this->mUnits = inUnits;
}

// ---- the trigger's members (ee_scenario trigger_members.rs: each a forward; the vectors and strings stay here, reached
// by ee_scenariocpp_trigger's single statements)

static_assert(EE_SCENARIO_TR_EFFECTS - EE_SCENARIO_TR_CONDITIONS == 20 && kSMTriggerOperatorEnd == 0,
              "trigger_members.rs's numbers");

// Creates an enabled, non-looping trigger with one empty condition entry and one empty effect entry
EESMTrigger::EESMTrigger() { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_CONSTRUCT, 0, 0, 0, 0); }

// Destroys the trigger, deleting all its condition and effect entries
EESMTrigger::~EESMTrigger() { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_DESTROY, 0, 0, 0, 0); }

// Copies a trigger: texts, entries, the flags, the condition duration and the id
void EESMTrigger::Copy(const EESMTrigger& inTrigger)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_COPY, 0, (int64_t)(intptr_t)&inTrigger, 0, 0);
}

// Deletes a condition entry; the new last one ends the chain
void EESMTrigger::DeleteCondition(unsigned long inIndex)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_DELETE, 0, (int64_t)inIndex, 0, 0);
}

// Deletes an effect entry; the new last one ends the chain
void EESMTrigger::DeleteEffect(unsigned long inIndex)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_DELETE, 1, (int64_t)inIndex, 0, 0);
}

// Fires the trigger when its AND/OR condition chain has held for the set duration: queues all effects, or one random
// OR-separated group, at their delays, and turns a non-looping trigger off
// (EEScenarioManager::Update 0x1b20e6)
void EESMTrigger::Evaluate(float inTime)
{
    if (!EESimCaptureRecord::Active()) {
        this->EvaluateUncaptured(inTime);
        return;
    }
    EESMCaptureTriggerState theBefore = EESMCaptureTriggerBefore(this);
    unsigned int theSeed = EESMCaptureSeed();
    this->EvaluateUncaptured(inTime);
    EESMCaptureTrigger(this, theBefore, inTime, theSeed);
}

// Evaluate's body: trigger.rs evaluate over the entries (trigger_members.rs)
void EESMTrigger::EvaluateUncaptured(float inTime)
{
    EESMHostContext theContext = {NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    ee_scenario_trigger_evaluate_member(&theHost, this, inTime);
}

// Returns condition entry inIndex, or NULL past the end
EESMTConditionWrapper* EESMTrigger::GetCondition(unsigned long inIndex)
{
    return (EESMTConditionWrapper*)(intptr_t)ee_scenario_trigger_member(this, EE_SCENARIO_TRM_GET, 0, (int64_t)inIndex, 0, 0);
}

// Returns effect entry inIndex, or NULL past the end
EESMTEffectWrapper* EESMTrigger::GetEffect(unsigned long inIndex)
{
    return (EESMTEffectWrapper*)(intptr_t)ee_scenario_trigger_member(this, EE_SCENARIO_TRM_GET, 1, (int64_t)inIndex, 0, 0);
}

// Reads the trigger's settings, condition entries and effect entries (id -1 becomes 0); returns the first error
// (ee_scenario save.rs trigger_load)
long EESMTrigger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText};
    EESMStream theStream = {&inFile, theStrings, NULL, this};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_trigger_load(&theTable, this);
}

// Writes the trigger's settings, condition entries and effect entries; returns the first error
// (ee_scenario save.rs trigger_save)
long EESMTrigger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText};
    EESMStream theStream = {&outFile, theStrings, NULL, this};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_trigger_save(&theTable, this);
}

// Sets condition entry inIndex
void EESMTrigger::SetCondition(unsigned long inIndex, bool inNot, unsigned long inCondition)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_SET, 0, (int64_t)inIndex, inNot, (int64_t)inCondition);
}

// Sets condition entry inIndex's operator (appending an entry after the last when it gets one)
void EESMTrigger::SetConditionOperator(unsigned long inIndex, long inOperator)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_SET_OPERATOR, 0, (int64_t)inIndex, inOperator, 0);
}

// Sets effect entry inIndex
void EESMTrigger::SetEffect(unsigned long inIndex, long inDelay, unsigned long inEffect)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_SET, 1, (int64_t)inIndex, (int64_t)(unsigned long)inDelay, (int64_t)inEffect);
}

// Sets effect entry inIndex's operator (appending an entry after the last when it gets one)
void EESMTrigger::SetEffectOperator(unsigned long inIndex, long inType)
{
    ee_scenario_trigger_member(this, EE_SCENARIO_TRM_SET_OPERATOR, 1, (int64_t)inIndex, inType, 0);
}

// the trigger members' single statements (trigger_members.rs EE_SCENARIO_TR_*): on trigger inTrigger, entry i, value v
extern "C" int64_t ee_scenariocpp_trigger(uint32_t inOp, void* inTrigger, int64_t i, int64_t v)
{
    EESMTrigger* t = static_cast<EESMTrigger*>(inTrigger);
    std::vector<EESMTConditionWrapper*>& c = t->mConditions;
    std::vector<EESMTEffectWrapper*>& e = t->mEffects;
    switch (inOp) {
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_COUNT: return (int64_t)c.size();
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_PUSH_NEW: c.push_back(new EESMTConditionWrapper);
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_BACK_EXISTS: return c.back() != NULL;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_DELETE_BACK: delete c.back();
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_POP: c.pop_back();
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_DELETE_AT: delete c[(size_t)i];
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_ERASE: c.erase(c.begin() + (size_t)i);
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_FIRST: return c[(size_t)i]->mIsNot = v != 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_SECOND: return (int64_t)(c[(size_t)i]->mValue = (unsigned long)v);
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_OPERATOR: return c[(size_t)i]->mOperator = (long)v;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_FIRST: return c[(size_t)i]->mIsNot;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SECOND: return (int64_t)c[(size_t)i]->mValue;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_OPERATOR: return c[(size_t)i]->mOperator;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_AT: return (int64_t)(intptr_t)c[(size_t)i];
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_COUNT: return (int64_t)e.size();
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_PUSH_NEW: e.push_back(new EESMTEffectWrapper);
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_BACK_EXISTS: return e.back() != NULL;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_DELETE_BACK: delete e.back();
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_POP: e.pop_back();
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_DELETE_AT: delete e[(size_t)i];
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_ERASE: e.erase(e.begin() + (size_t)i);
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_FIRST: return (int64_t)(e[(size_t)i]->mDelay = (unsigned long)v);
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_SECOND: return (int64_t)(e[(size_t)i]->mEffectIndex = (unsigned long)v);
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_OPERATOR: return e[(size_t)i]->mOperator = (long)v;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_FIRST: return (int64_t)e[(size_t)i]->mDelay;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SECOND: return (int64_t)e[(size_t)i]->mEffectIndex;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_OPERATOR: return e[(size_t)i]->mOperator;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_AT: return (int64_t)(intptr_t)e[(size_t)i];
    case EE_SCENARIO_TR_NAME_EMPTY: t->SetName("");
        return 0;
    case EE_SCENARIO_TR_TEXT_EMPTY: t->SetText("");
        return 0;
    case EE_SCENARIO_TR_COPY_NAME: t->mName = static_cast<EESMTrigger*>((void*)(intptr_t)i)->mName;
        return 0;
    case EE_SCENARIO_TR_COPY_TEXT: t->mText = static_cast<EESMTrigger*>((void*)(intptr_t)i)->mText;
        return 0;
    case EE_SCENARIO_TR_FIELDS: return (int64_t)(intptr_t) static_cast<EeScenarioTriggerFields*>(t);
    case EE_SCENARIO_TR_ADD_TIMED: gWorld.mScenarioManager->AddTimedEffect((long)i, (unsigned long)v, t->mIsInCinematic);
        return 0;
    case EE_SCENARIO_TR_WORLD_CINEMATIC: return gWorld.mInCinematic;
    }
    return 0;
}
