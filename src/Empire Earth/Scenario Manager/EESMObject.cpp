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
    case EE_SCENARIO_READ_UNIT_FLAGS: {
        EEUnit* theUnit = SMUnit(inA);
        if (theUnit == NULL)
            return 0;
        return 1 | (theUnit->GetHitPoints() > 0 ? 2 : 0) | (theUnit->mUnitClass->mIsTree ? 4 : 0)
               | (theUnit->mComplexUnit != NULL ? 8 : 0)
               | (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mPreviousPart != NULL ? 16 : 0);
    }
    case EE_SCENARIO_READ_UNIT_CLASS_TYPE: return SMUnit(inA)->mUnitClass->mUnitType;
    case EE_SCENARIO_READ_UNIT_PLAYER: return SMUnit(inA)->mPlayer->GetPlayerID();
    case EE_SCENARIO_READ_UNIT_COMMAND: return SMUnit(inA)->mComplexUnit->GetCommandType();
    case EE_SCENARIO_READ_UNIT_ATTRIBUTE:
        return theObject->GetUnitAttribute((unsigned long)inB, inC != 0, SMUnit(inA), 0, 0);
    case EE_SCENARIO_READ_UNIT_SELECTED_BY: {
        EEPlayer* thePlayer = gWorld.mPlayers[inA];
        EEUnit* theUnit = SMUnit(inB);
        return thePlayer->mActiveGroup->Contains(theUnit->mComplexUnit) || thePlayer->mViewedUnit == theUnit;
    }
    case EE_SCENARIO_READ_UNIT_SEEN_BY: return EEIsSeenBy(SMUnit(inA), gWorld.mPlayers[inB], false);
    case EE_SCENARIO_READ_AREA_CONTAINS_UNIT: {
        EESMArea* theArea = theManager->GetArea((unsigned long)inA);
        return theArea == NULL ? -1 : theArea->ContainsUnit(SMUnit(inB));
    }
    case EE_SCENARIO_READ_PLAYER_UNIT:
        if (inB == 0) {
            EEPlayer* thePlayer = gWorld.mPlayers[inA];
            inContext->mCursor = thePlayer != NULL ? thePlayer->mFirstUnit : NULL;
        } else
            inContext->mCursor = static_cast<EEUnit*>(inContext->mCursor->mNext);
        return SMChainUnit(inContext->mCursor);
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
    case EE_SCENARIO_READ_BLOCK_TAKES: return EEPlayerBlockTakes(NULL, inA, inB, inC, inD, inD, 0);
    case EE_SCENARIO_READ_MAP_TILE_DRY: {
        EEMapTile* theTile = gWorld.mMap->GetTile(inA, inB);
        return theTile != NULL && theTile->mTerrainType != kTerrainDeepWater && theTile->mTerrainType != kTerrainShallowWater;
    }
    case EE_SCENARIO_READ_MAP_TILE_EXISTS: return gWorld.mMap->GetTile(inA, inB) != NULL;
    case EE_SCENARIO_READ_FOOTPRINT_DISTANCE: {
        long theX = 0;
        long theY = 0;
        return SMFloat(EEGetFootprintDistance(static_cast<EEComplexUnit*>(SMUnit(inA)), inB, inC, &theX, &theY));
    }
    case EE_SCENARIO_READ_UNIT_CAN_REACH_TILE:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->CanReachTile(inB, inC);
    case EE_SCENARIO_READ_UNIT_NEAREST_TILE: {
        long theX;
        long theY;
        SMUnit(inA)->GetNearestFootprintTile(SMUnit(inB), &theX, &theY);
        return SMPack(theX, theY);
    }
    case EE_SCENARIO_READ_UNIT_IN_REACH:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->IsInReach(SMUnit(inB));
    }
    return 0;
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

// FindPlacement's tile lists (placement.rs EE_SCENARIO_TILES_NEAR_POINT, _SIDE, _CORNER)
void EESMObjectTiles(uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, std::vector<U2DPoint<long> >& outTiles)
{
    float theRadius;
    memcpy(&theRadius, &inC, 4);
    switch (inOp) {
    case EE_SCENARIO_TILES_NEAR_POINT: EEGetTilesNearPoint(GE3DPoint((float)inA, (float)inB, 0.0f), theRadius, outTiles); break;
    case EE_SCENARIO_TILES_SIDE: EEGetSideTiles(inA, inB, inC, outTiles); break;
    case EE_SCENARIO_TILES_CORNER: EEGetCornerTiles(0, inA, inB, inC, outTiles); break;
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

// a unit's member for GetUnitAttribute that no record holds (ee_scenario attribute.rs; unit_views.rs reads the others
// from the world views): the unit variables (mUnitVariables[inField - EE_SCENARIO_UNIT_VARIABLE]) and the physical
// model's scale (EE_SCENARIO_UNIT_SCALE, its bits)
int64_t EESMUnitField(EEUnit* inUnit, int32_t inField)
{
    EEComplexUnit* theComplex = inUnit->GetComplexUnit();
    int32_t theField = inField >= EE_SCENARIO_OF_STATS ? inField - EE_SCENARIO_OF_STATS : inField;
    if (theField >= EE_SCENARIO_UNIT_VARIABLE && theField < EE_SCENARIO_UNIT_VARIABLE + 3)
        return theComplex->mUnitVariables[theField - EE_SCENARIO_UNIT_VARIABLE];
    float theScale = theComplex->mPhysicalModel.GetScale();
    uint32_t theBits;
    memcpy(&theBits, &theScale, 4);
    return theBits;
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

// Creates an enabled, non-looping trigger with one empty condition entry and one empty effect entry
EESMTrigger::EESMTrigger()
{
    EESMTConditionWrapper* theCondition;
    EESMTEffectWrapper* theEffect;
    this->SetName("");
    this->SetText("");
    this->mIsOn = true;
    this->mHasFired = false;
    this->mIsInCinematic = false;
    this->mIsLooping = false;
    this->mConditionDuration = 0;
    this->mConditionStartTime = 0;
    theCondition = new EESMTConditionWrapper;
    this->mConditions.push_back(theCondition);
    this->SetCondition(0, false, 0);
    this->SetConditionOperator(0, 0);
    theEffect = new EESMTEffectWrapper;
    this->mEffects.push_back(theEffect);
    this->SetEffect(0, 0, 0);
    this->SetEffectOperator(0, 0);
    this->mID = 0;
}

// Destroys the trigger and its condition and effect entries
EESMTrigger::~EESMTrigger()
{
    while (this->mConditions.size() > 0) {
        if (this->mConditions.back() != NULL)
            delete this->mConditions.back();
        this->mConditions.pop_back();
    }
    while (this->mEffects.size() > 0) {
        if (this->mEffects.back() != NULL)
            delete this->mEffects.back();
        this->mEffects.pop_back();
    }
}

// Copies another trigger's name, text, conditions, effects and flags (not its condition start time)
void EESMTrigger::Copy(const EESMTrigger& inTrigger)
{
    this->mName = inTrigger.mName;
    this->mText = inTrigger.mText;
    this->CopyConditions(inTrigger.mConditions);
    this->CopyEffects(inTrigger.mEffects);
    this->mIsOn = inTrigger.mIsOn;
    this->mHasFired = inTrigger.mHasFired;
    this->mIsLooping = inTrigger.mIsLooping;
    this->mConditionDuration = inTrigger.mConditionDuration;
    this->mID = inTrigger.mID;
    this->mIsInCinematic = inTrigger.mIsInCinematic;
}

// Deletes a condition entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteCondition(unsigned long inIndex)
{
    delete this->mConditions[inIndex];
    this->mConditions.erase(this->mConditions.begin() + inIndex);
    if (inIndex == this->mConditions.size())
        this->mConditions[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
}

// Deletes an effect entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteEffect(unsigned long inIndex)
{
    delete this->mEffects[inIndex];
    this->mEffects.erase(this->mEffects.begin() + inIndex);
    if (inIndex == this->mEffects.size())
        this->mEffects[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
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

// Evaluate's body (Evaluate adds the simulation capture's SMTE record: ee/EEScenarioCapture.h)
void EESMTrigger::EvaluateUncaptured(float inTime)
{
    // the chain, the duration and the effects' choice are Rust's (ee_scenario trigger.rs); the C++ schedules them
    EeScenarioTriggerState theState = {this->mIsOn, this->mHasFired, this->mIsLooping, this->mIsInCinematic,
                                       (uint32_t)this->mConditionDuration, (uint32_t)this->mConditionStartTime};
    std::vector<EeScenarioConditionEntry> theConditions(this->mConditions.size());
    for (size_t i = 0; i < theConditions.size(); i++)
        theConditions[i] = {(uint32_t)this->mConditions[i]->mValue, (int32_t)this->mConditions[i]->mOperator,
                            this->mConditions[i]->mIsNot};
    std::vector<EeScenarioEffectEntry> theEffects(this->mEffects.size());
    for (size_t k = 0; k < theEffects.size(); k++)
        theEffects[k] = {(int32_t)this->mEffects[k]->mOperator, (uint32_t)this->mEffects[k]->mDelay,
                         (uint32_t)this->mEffects[k]->mEffectIndex};
    std::vector<EeScenarioTimedEffect> theTimed(theEffects.size() + 1);
    size_t theCount = 0;
    EESMHostContext theContext = {NULL, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    ee_scenario_trigger_evaluate(&theHost, &theState, gWorld.mInCinematic, theConditions.data(), theConditions.size(),
                                 theEffects.data(), theEffects.size(), inTime, theTimed.data(), &theCount);
    this->mIsOn = theState.is_on;
    this->mHasFired = theState.has_fired;
    this->mConditionStartTime = theState.condition_start_time;
    for (size_t t = 0; t < theCount; t++)
        gWorld.mScenarioManager->AddTimedEffect((long)theTimed[t].effect, theTimed[t].time, this->mIsInCinematic);
}

// Returns the condition entry at an index, or NULL
EESMTConditionWrapper* EESMTrigger::GetCondition(unsigned long inIndex)
{
    EESMTConditionWrapper* theWrapper = NULL;
    if (inIndex < this->mConditions.size())
        theWrapper = this->mConditions[inIndex];
    return theWrapper;
}

// Returns the effect entry at an index, or NULL
EESMTEffectWrapper* EESMTrigger::GetEffect(unsigned long inIndex)
{
    EESMTEffectWrapper* theWrapper = NULL;
    if (inIndex < this->mEffects.size())
        theWrapper = this->mEffects[inIndex];
    return theWrapper;
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

// Sets a condition entry's condition index and NOT flag
void EESMTrigger::SetCondition(unsigned long inIndex, bool inNot, unsigned long inCondition)
{
    this->mConditions[inIndex]->mIsNot = inNot;
    this->mConditions[inIndex]->mValue = inCondition;
}

// Sets a condition entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetConditionOperator(unsigned long inIndex, long inOperator)
{
    if (inIndex == this->mConditions.size() - 1 && inOperator != 0)
        this->mConditions.push_back(new EESMTConditionWrapper);
    this->mConditions[inIndex]->mOperator = inOperator;
}

// Replaces the trigger's condition entries by copies of the given ones
void EESMTrigger::CopyConditions(const std::vector<EESMTConditionWrapper*>& inConditions)
{
    while (this->mConditions.size() > 1)
        this->DeleteCondition(1);
    for (unsigned long i = 0; i < inConditions.size(); i++) {
        EESMTConditionWrapper* theWrapper = inConditions[i];
        this->SetCondition(i, theWrapper->mIsNot, theWrapper->mValue);
        this->SetConditionOperator(i, theWrapper->mOperator);
    }
}

// Sets an effect entry's effect index and delay
void EESMTrigger::SetEffect(unsigned long inIndex, long inDelay, unsigned long inEffect)
{
    this->mEffects[inIndex]->mDelay = inDelay;
    this->mEffects[inIndex]->mEffectIndex = inEffect;
}

// Sets an effect entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetEffectOperator(unsigned long inIndex, long inType)
{
    if (inIndex == this->mEffects.size() - 1 && inType != 0)
        this->mEffects.push_back(new EESMTEffectWrapper);
    this->mEffects[inIndex]->mOperator = inType;
}

// Replaces the trigger's effect entries by copies of the given ones
void EESMTrigger::CopyEffects(const std::vector<EESMTEffectWrapper*>& inEffects)
{
    while (this->mEffects.size() > 1)
        this->DeleteEffect(1);
    for (unsigned long i = 0; i < inEffects.size(); i++) {
        EESMTEffectWrapper* theWrapper = inEffects[i];
        this->SetEffect(i, theWrapper->mDelay, theWrapper->mEffectIndex);
        this->SetEffectOperator(i, theWrapper->mOperator);
    }
}
