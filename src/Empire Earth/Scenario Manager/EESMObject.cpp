// C:\EEX\Empire Earth\Scenario Manager\EESMObject.cpp (file name inferred: no __FILE__): 0x1c5f73..0x1ca4ee,
// scenario objects and triggers (with EESMTConditionWrapper.cpp and EESMTEffectWrapper.cpp folded in).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "lle/FSFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMObject.h"
#include "ee/EESMTConditionWrapper.h"
#include "ee/EESMTEffectWrapper.h"
#include <algorithm>
#include "ee/EESMTrigger.h"
#include "ee/EESMCondition.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPlayer.h"
#include "ee/EESMArea.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"
// 0x246ea2 (cdecl, 5 arguments; EEFOFindNewTargetNearOldTarget.cpp): the squared distance from inUnit to tile
// (inX, inY), the unit's nearest tile out (provisional name)
float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);

// FUNCTION: EXE 0x5c5f73
// Creates an object named None that picks units on the map, owned by and selected by player 7, with no filters
// (the numbers: ee_scenario fields.rs object_defaults)
EESMObject::EESMObject()
{
    gResource->GetString(IDS_NONE, this->mName);
    this->mUnits.clear();
    *static_cast<EeScenarioObjectFields*>(this) = ee_scenario_object_defaults();
}

// SYNTHETIC: EXE 0x5c6019
// (the scalar deleting destructor, emitted with the vtable; slot EESMObject+0x0)

// FUNCTION: EXE 0x5c6035
// Destroys the object (members only)
EESMObject::~EESMObject()
{
}

// FUNCTION: EXE 0x5c6148
// IsNearUnit: ee_scenario placement.rs near_unit (objeval.rs asks it)

// FUNCTION: EXE 0x5c629f
// IsNearTile: ee_scenario placement.rs near_tile (find_placement asks it)

// FUNCTION: EXE 0x5c6386
// Copies another object's name and filter settings, and its unit list when it picks units on the map
void EESMObject::Copy(const EESMObject& inObject)
{
    this->mName = inObject.mName;
    *static_cast<EeScenarioObjectFields*>(this) = inObject;   // every number
    if (this->mIsSelectOnMap)
        this->mUnits = inObject.mUnits;
}

// FUNCTION: EXE 0x5c6458
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

// FUNCTION: EXE 0x5c6b14
// Picks the unit type to create for the object: the parent's first unit's type, the object's class, or a random
// class matching its epoch/family/AI kind/unit set filter; -1 if none
long EESMObject::PickUnitType()
{
    EESMHostContext theContext = {NULL, NULL};   // ee_scenario object.rs pick_unit_type
    EeScenarioHost theHost = EESMHost(&theContext);
    return ee_scenario_object_pick_unit_type(&theHost, this);
}

// FUNCTION: EXE 0x5c6d4f
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

// FUNCTION: EXE 0x5c7749
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

// a unit's member for GetUnitAttribute (ee_scenario attribute.rs EE_SCENARIO_UNIT_*, _STATS_*, _CLASS_*): integers and
// bools as they are, floats as their bits; a class field + EE_SCENARIO_OF_STATS reads the upgrade's class
int64_t EESMUnitField(EEUnit* inUnit, int32_t inField)
{
    EEComplexUnit* theComplex = inUnit->GetComplexUnit();
    EEUnitClassUpgrade* theStats = inUnit->GetStats();
    EEUnitClass* theClass = inField >= EE_SCENARIO_OF_STATS ? theStats->mUnitClass : inUnit->GetUnitClass();
    auto Bits = [](float inValue) { uint32_t theBits; memcpy(&theBits, &inValue, 4); return (int64_t)theBits; };
    int32_t theField = inField >= EE_SCENARIO_OF_STATS ? inField - EE_SCENARIO_OF_STATS : inField;
    if (theField >= EE_SCENARIO_UNIT_VARIABLE && theField < EE_SCENARIO_UNIT_VARIABLE + 3)
        return theComplex->mUnitVariables[theField - EE_SCENARIO_UNIT_VARIABLE];
    if (theField >= EE_SCENARIO_STATS_ARMOR && theField < EE_SCENARIO_STATS_ARMOR + 6)
        return Bits(theStats->mArmor[theField - EE_SCENARIO_STATS_ARMOR]);
    if (theField >= EE_SCENARIO_CLASS_ARMOR && theField < EE_SCENARIO_CLASS_ARMOR + 6)
        return theClass->mArmor[theField - EE_SCENARIO_CLASS_ARMOR];
    switch (theField) {
    case EE_SCENARIO_UNIT_HAS_COMPLEX: return theComplex != NULL;
    case EE_SCENARIO_UNIT_HAS_STATS: return theStats != NULL;
    case EE_SCENARIO_UNIT_HAS_CLASS: return theClass != NULL;
    case EE_SCENARIO_UNIT_HIT_POINTS: return inUnit->GetHitPoints();
    case EE_SCENARIO_UNIT_RESOURCE_TYPE: return inUnit->mResourceType;
    case EE_SCENARIO_UNIT_RESOURCE_AMOUNT: return (long)inUnit->mResourceAmount;
    case EE_SCENARIO_UNIT_FLIGHT_TIME: return (long)theComplex->mFlightTime;
    case EE_SCENARIO_UNIT_CARGO: return (long)theComplex->GetNumCargoUnits();
    case EE_SCENARIO_UNIT_GARRISON: return theComplex->mGarrisonCount;
    case EE_SCENARIO_UNIT_AMMUNITION: return (long)theComplex->mAmmunition;
    case EE_SCENARIO_UNIT_PAUSED: return theComplex->mIsAnimationPaused;
    case EE_SCENARIO_UNIT_RELOAD_TIMER: return (long)theComplex->mReloadTimer;
    case EE_SCENARIO_UNIT_SCALE: return Bits(theComplex->mPhysicalModel.GetScale());
    case EE_SCENARIO_UNIT_POWER: return theComplex->GetPower();
    case EE_SCENARIO_UNIT_ROTTING: return (long)theComplex->mRottingAwayTime;
    case EE_SCENARIO_UNIT_BUILT: return theComplex->mIsBuilt;
    case EE_SCENARIO_STATS_MAX_HIT_POINTS: return theStats->mMaxHitPoints;
    case EE_SCENARIO_STATS_CARRY: return theStats->mCarryAmount;
    case EE_SCENARIO_STATS_FLIGHT_TIME: return theStats->mFlightTime;
    case EE_SCENARIO_STATS_MAX_CARRY: return theStats->mMaxObjectCarry;
    case EE_SCENARIO_STATS_RELOAD: return theStats->mReloadTime;
    case EE_SCENARIO_STATS_SCALE: return Bits(theStats->mScale);
    case EE_SCENARIO_STATS_POWER: return theStats->mPower;
    case EE_SCENARIO_STATS_ATTACK: return Bits(theStats->mAttack);
    case EE_SCENARIO_STATS_WORK_RATE: return theStats->mWorkRate;
    case EE_SCENARIO_STATS_LEADERSHIP: return Bits(theStats->mLeadership);
    case EE_SCENARIO_STATS_REPAIR: return theStats->mRepairRate;
    case EE_SCENARIO_STATS_HEALING: return theStats->mHealingPoints;
    case EE_SCENARIO_STATS_RANGE: return Bits(theStats->mRange);
    case EE_SCENARIO_STATS_LINE_OF_SIGHT: return Bits(theStats->mLineOfSight);
    case EE_SCENARIO_STATS_VELOCITY: return Bits(theStats->mMaxVelocity);
    case EE_SCENARIO_STATS_INITIALIZED: return theStats->mIsInitialized;
    case EE_SCENARIO_STATS_CAMERA_FOCUS: return Bits(theStats->mCameraFocus);
    case EE_SCENARIO_CLASS_GARRISON: return theClass->mGarrisonCapacity;
    case EE_SCENARIO_CLASS_AMMUNITION: return theClass->mAmmunition;
    case EE_SCENARIO_CLASS_ROTTING: return theClass->mRottingTime;
    case EE_SCENARIO_CLASS_POWER: return theClass->mPower;
    case EE_SCENARIO_CLASS_RELOAD: return theClass->mReloadTime;
    case EE_SCENARIO_CLASS_ATTACK: return theClass->mAttack;
    case EE_SCENARIO_CLASS_WORK_RATE: return theClass->mWorkRate;
    case EE_SCENARIO_CLASS_FLIGHT_TIME: return theClass->mFlightTime;
    case EE_SCENARIO_CLASS_LEADERSHIP: return theClass->mLeadership;
    case EE_SCENARIO_CLASS_MAX_CARRY: return theClass->mMaxObjectCarry;
    case EE_SCENARIO_CLASS_REPAIR: return theClass->mRepairRate;
    case EE_SCENARIO_CLASS_HIT_POINTS: return theClass->mHitPoints;
    case EE_SCENARIO_CLASS_CARRY: return theClass->mCarryAmount;
    case EE_SCENARIO_CLASS_HEALING: return theClass->mHealingPoints;
    case EE_SCENARIO_CLASS_RANGE: return Bits(theClass->mRange);
    case EE_SCENARIO_CLASS_LINE_OF_SIGHT: return Bits(theClass->mLineOfSight);
    case EE_SCENARIO_CLASS_VELOCITY: return Bits(theClass->mMaxVelocity);
    case EE_SCENARIO_CLASS_TRANSPORT: return theClass->mTransportCapacity;
    case EE_SCENARIO_CLASS_WEIGHT: return theClass->mWeight;
    case EE_SCENARIO_CLASS_TILE_SIZE: return theClass->mTileSize;
    case EE_SCENARIO_CLASS_CRUISE: return Bits(theClass->GetCruiseAltitude());
    case EE_SCENARIO_CLASS_MIN_RANGE: return Bits(theClass->mMinRange);
    case EE_SCENARIO_CLASS_AIR: return theClass->mCanEnterAirTransports;
    case EE_SCENARIO_CLASS_LAND: return theClass->mCanEnterLandTransports;
    case EE_SCENARIO_CLASS_SEA: return theClass->mCanEnterSeaTransports;
    case EE_SCENARIO_CLASS_FORTRESS: return theClass->mCanEnterFortresses;
    }
    return 0;
}


// FUNCTION: EXE 0x5c8274
// Copies the object's unit ids out
void EESMObject::GetUnits(std::vector<unsigned long>& outUnits) const
{
    outUnits = this->mUnits;
}

// FUNCTION: EXE 0x5c8284
// Reads the object's units (appended) and filter settings (files before 0.0xcd: attribute ids above Construction
// Complete move up by 3); returns the first error (ee_scenario save.rs object_load)
long EESMObject::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName};
    EESMStream theStream = {&inFile, theStrings, &this->mUnits};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_object_load(&theTable, this, inMajorVersion, inMinorVersion);
}

// FUNCTION: EXE 0x5c85a4
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

// FUNCTION: EXE 0x5c85de
// Writes the object's units and filter settings (the parent only for minor version >= 0x2a); returns the first error
// (ee_scenario save.rs object_save)
long EESMObject::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName};
    EESMStream theStream = {&outFile, theStrings, &this->mUnits};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_object_save(&theTable, this, inMinorVersion);
}

#include "ee/EESMEffect.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"

// FUNCTION: EXE 0x5c8871
// Raises or lowers a player's unit type stat by an amount or percent of the class value (armor: the bonus over the
// class base changes and is clamped at 0), clamped, and queues the change as a pending effect
// Original cdecl helper. Parameter types follow the dispatcher's pushes and the actual player/upgrade readers.
// Every case is written out in full (the original reads inUpgrade->mUnitClass per case; the int cases' shared
// add/clamp/store tail is VC6 tail merging); theAdd is the original's sete (negate when it is false).
void EEModifyUnitAttribute(unsigned long inAttribute, bool inPercentage, EEPlayer* inPlayer,
                EEUnitClassUpgrade* inUpgrade, long inAmount, long inMode)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEHerd*> theHerds;
    EESMEffect theEffect;
    bool theAdd = inMode == 0;
    long theAmount = inAmount;
    float theFloat;
    EEUnit* theNode;
    long theArmor = 0;
    theEffect.mAttributeUnit = (unsigned long)-1;
    theEffect.mType = kSMEffectObject;
    theEffect.mObjectAction = kSMObjectActionUnitAttribute;
    theEffect.mAttributeUnitType = inUpgrade != NULL ? inUpgrade->mUnitClass->GetUnitType() : 0;
    theEffect.mAttributePlayer = inPlayer != NULL ? inPlayer->GetPlayerID() : -1;
    theEffect.mObject = 0;
    theEffect.mAttribute = inAttribute;
    theEffect.mUsesPercent = inPercentage;
    theEffect.mAttributeAmount = inAmount;
    theEffect.mAttributeMode = inMode;
    if (inUpgrade == NULL)
        return;
    switch (inAttribute) {
    case kSMUnitAttributeShockArmorPlus: case kSMUnitAttributeArrowArmorPlus: case kSMUnitAttributePierceArmorPlus: case kSMUnitAttributeGunArmorPlus: case kSMUnitAttributeLaserArmorPlus: case kSMUnitAttributeMissileArmorPlus: {
        switch (inAttribute) {
        case kSMUnitAttributeShockArmorPlus: theArmor = 0; break;
        case kSMUnitAttributeArrowArmorPlus: theArmor = 1; break;
        case kSMUnitAttributePierceArmorPlus: theArmor = 2; break;
        case kSMUnitAttributeGunArmorPlus: theArmor = 3; break;
        case kSMUnitAttributeLaserArmorPlus: theArmor = 4; break;
        case kSMUnitAttributeMissileArmorPlus: theArmor = 5; break;
        }
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mArmor[theArmor] * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = (inUpgrade->mArmor[theArmor] - (float)inUpgrade->mUnitClass->mArmor[theArmor]) + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        // the clamped bonus goes back on top of the class base armor (the upgrade holds the total armor)
        inUpgrade->mArmor[theArmor] = theFloat + (float)inUpgrade->mUnitClass->mArmor[theArmor];
        break;
    }
    case kSMUnitAttributeReloadTimePlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mReloadTime * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mReloadTime;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mReloadTime = inAmount;
        break;
    case kSMUnitAttributeAttackPlus:
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mAttack * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetAttack() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->mAttack = theFloat;
        break;
    case kSMUnitAttributeWorkRatePlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mWorkRate * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mWorkRate;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mWorkRate = inAmount;
        break;
    case kSMUnitAttributeFlightTimePlus: {
        long theChange;
        if (inPercentage)
            theChange = UMFloatToLong((float)inUpgrade->mUnitClass->mFlightTime * inAmount * 0.01f);
        else
            theChange = (long)(inAmount / 0.001f + 0.5f);
        if (!theAdd)
            theChange = -theChange;
        theChange += inUpgrade->mFlightTime;
        if (theChange < 0)
            theChange = 0;
        inUpgrade->mFlightTime = theChange;
        break;
    }
    case kSMUnitAttributeLeadershipPlus:
        if (inPercentage)
            theAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mLeadership * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetLeadership() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->mLeadership = theFloat;
        break;
    case kSMUnitAttributeMaxObjectsCarriedPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mMaxObjectCarry * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mMaxObjectCarry;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mMaxObjectCarry = inAmount;
        break;
    case kSMUnitAttributeRepairPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mRepairRate * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mRepairRate;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mRepairRate = inAmount;
        break;
    case kSMUnitAttributeHitPointsPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mHitPoints * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mMaxHitPoints;
        if (inAmount < 0)
            inAmount = 0;
        if (inAmount <= 1)
            inAmount = 1;
        inUpgrade->mMaxHitPoints = inAmount;
        break;
    case kSMUnitAttributeCarryAmountPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mCarryAmount * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mCarryAmount;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mCarryAmount = inAmount;
        break;
    case kSMUnitAttributeContainerHealingHPPerSecPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mHealingPoints * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mHealingPoints;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mHealingPoints = inAmount;
        break;
    case kSMUnitAttributeRangePlus:
        if (inPercentage)
            theAmount = UMFloatToLong(inUpgrade->mUnitClass->GetRange() * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetRange() + theFloat;
        if (theFloat < 0.0f)
            theFloat = 0.0f;
        inUpgrade->SetRange(theFloat);
        break;
    case kSMUnitAttributeLineOfSightPlus: {
        if (inPercentage)
            theAmount = UMFloatToLong(inUpgrade->mUnitClass->GetLineOfSight() * inAmount * 0.01f);
        theFloat = (float)theAmount;
        if (!theAdd)
            theFloat = -theFloat;
        float theSpeed = inUpgrade->GetLineOfSight() + theFloat;
        if (theSpeed < 0.0f)
            theSpeed = 0.0f;
        for (theNode = inPlayer->mFirstUnit; theNode != NULL;
             theNode = static_cast<EEUnit*>(theNode->mNext)) {
            if (theNode->mComplexUnit != NULL && theNode->mUnitClass == inUpgrade->mUnitClass) {
                theNode->mComplexUnit->SightOffHere();
                theUnits.push_back(theNode);
            }
        }
        inUpgrade->mLineOfSight = theSpeed;
        for (unsigned long i = 0; i < theUnits.size(); i++)
            theUnits[i]->SightOnHere();
        break;
    }
    case kSMUnitAttributeMaxVelocityPlus:
        if (inPercentage)
            theFloat = inUpgrade->mUnitClass->GetMaxVelocity() * inAmount * 0.01f;
        else
            theFloat = (float)inAmount / 20.0f;
        if (!theAdd)
            theFloat = -theFloat;
        theFloat = inUpgrade->GetMaxVelocity() + theFloat;
        if (theFloat < 0.0001f)
            theFloat = 0.0001f;
        inUpgrade->SetMaxVelocity(theFloat);
        break;
    case kSMUnitAttributePowerPlus:
        if (inPercentage)
            inAmount = UMFloatToLong((float)inUpgrade->mUnitClass->mPower * inAmount * 0.01f);
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mPower;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mPower = inAmount;
        break;
    case kSMUnitAttributeAnimalAIActivePlus: {
        if (inPercentage)
            inAmount = inAmount > 0;
        if (!theAdd)
            inAmount = -inAmount;
        inAmount += inUpgrade->mIsInitialized != false;
        if (inAmount < 0)
            inAmount = 0;
        inUpgrade->mIsInitialized = inAmount > 0;
        inPlayer->GetHerdsOfType(theHerds, inUpgrade->mUnitClass->mUnitType);
        for (long j = (long)theHerds.size() - 1; j >= 0; --j)
            theHerds[j]->mIsActive = inUpgrade->mIsInitialized;
        break;
    }
    case kSMUnitAttributeCameraFocusPlus:
        if (!theAdd)
            inAmount = -inAmount;
        theAmount = inAmount + (long)(inUpgrade->mCameraFocus * 100.0f);
        if (theAmount < 0)
            theAmount = 0;
        inUpgrade->mCameraFocus = (float)theAmount * 0.01f;
        break;
    }
    gWorld.mScenarioManager->AddV134Effect(theEffect);
}

#include "ee/EEUnitAction.h"
#include "ee/EEUGRust.h"

// FUNCTION: EXE 0x5c8f28
// Adds to, subtracts from or sets one unit's attribute (hit points, cargo, flight time, garrison, ammo, scale, power,
// rot time, built state, variables) by a fixed, random or percentage amount
// Original cdecl helper; random range and the percentage flag are independent of add/subtract/assign mode.
void EEChangeUnit(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode)
{
    std::vector<unsigned long> theUnused;
    std::vector<EEComplexUnit*> theUnits;
    EESMEffect theEffect;
    EEUnitClass* theClass = inUnit != NULL ? inUnit->GetUnitClass() : NULL;
    EEComplexUnit* theComplex = inUnit != NULL ? inUnit->GetComplexUnit() : NULL;
    EEUnitClassUpgrade* theStats = inUnit != NULL ? inUnit->GetStats() : NULL;
    theEffect.mType = kSMEffectObject;
    theEffect.mObjectAction = kSMObjectActionUnitAttribute;
    theEffect.mAttributeUnit = inUnit != NULL ? inUnit->GetUnitID() : (unsigned long)-1;
    theEffect.mAttributeUnitType = theClass != NULL ? theClass->GetUnitType() : 0;
    theEffect.mAttributePlayer = inUnit != NULL ? inUnit->GetPlayer()->GetPlayerID() : -1;
    theEffect.mUsesPercent = false;
    theEffect.mObject = 0;
    theEffect.mAttribute = inAttribute;
    theEffect.mAttributeMode = inMode;
    long theAmount = inMaximum != 0 ? gPhysics->GetRandom(inMinimum, (long)inMaximum) : inMinimum;
    if (inRandom) {
        if (!inPercentage)
            theAmount = gPhysics->GetRandom(0, inMinimum);
        else {
            theAmount = gPhysics->GetRandom(0L, inMinimum + (long)(inMaximum > 100 ? 0 : 100 - inMaximum));
            if (theAmount >= inMinimum)
                theAmount += 1 + (inMaximum - inMinimum);
        }
    }
    int theValue = 0;   // the counter changed below: 1 the reload timer, 2 the power, 3 the rotting time (unit store holders)
    long theBase = 0;
    switch (inAttribute) {
    case kSMUnitAttributeHitPoints:
        if (inUnit != NULL && inUnit->GetHitPoints() > 0) {
            if (inPercentage) theAmount = UMFloatToLong((float)theStats->mMaxHitPoints * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += inUnit->GetHitPoints();
            else if (inMode == kSMAttributeModeDecrease) theAmount = inUnit->GetHitPoints() - theAmount;
            if (theAmount < 1) {
                if (inUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource) {
                    inUnit->SetResourceAmount(0);
                    inUnit->GetPlayer()->UnitTaken(inUnit, false);
                    inUnit->GetPlayer()->KillUnit(inUnit, false);
                } else
                    inUnit->Kill(0, 0, 0);
            } else
                inUnit->SetHitPoints(theAmount);
        }
        break;
    case kSMUnitAttributeAnyResource: case kSMUnitAttributeStone: case kSMUnitAttributeGold: case kSMUnitAttributeWood: case kSMUnitAttributeFood: case kSMUnitAttributeIron: {
        if (inPercentage) theAmount = UMFloatToLong((float)theStats->mCarryAmount * theAmount * 0.01f);
        if (inMode == kSMAttributeModeIncrease) theAmount += inUnit->mResourceAmount;
        else if (inMode == kSMAttributeModeDecrease) theAmount = inUnit->mResourceAmount - theAmount;
        if (theAmount < 0) theAmount = 0;
        unsigned long theState = 6;
        if (inAttribute == kSMUnitAttributeAnyResource) {
            // any resource: a random type unless the unit is gathering, which keeps the type it gathers
            inUnit->SetResourceAmount(theAmount);
            if (theComplex == NULL || theComplex->mCommand == NULL || theComplex->mCommand->mType != kActionGatherResource)
                inUnit->SetResourceType(gPhysics->GetRandom(2L, 6L));
            break;
        } else if (inAttribute == kSMUnitAttributeStone) theState = 4;
        else if (inAttribute == kSMUnitAttributeGold) theState = 5;
        else if (inAttribute == kSMUnitAttributeWood) theState = 3;
        else if (inAttribute == kSMUnitAttributeFood) theState = 2;
        if (theComplex != NULL && theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionGatherResource
            && theComplex->mResourceType != theState)
            break;
        inUnit->SetResourceAmount(theAmount);
        inUnit->SetResourceType(theState);
        break;
    }
    case kSMUnitAttributeFlightTime:
        if (theComplex != NULL) {
            long theTime = inPercentage ? UMFloatToLong((float)theStats->mFlightTime * theAmount * 0.01f)
                                       : (long)(theAmount / 0.001f + 0.5f);
            if (inMode == kSMAttributeModeIncrease) theTime += theComplex->mFlightTime;
            else if (inMode == kSMAttributeModeDecrease) theTime = theComplex->mFlightTime - theTime;
            if (theTime < 0) theTime = 0;
            theComplex->SetFlightTime(theTime);
        }
        break;
    case kSMUnitAttributeGarrisonLevel:
        if (theComplex != NULL && theComplex->GetUnitClass()->mGarrisonCapacity > 0) {
            if (inPercentage) theAmount = UMFloatToLong((float)theClass->mGarrisonCapacity * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += theComplex->mGarrisonCount;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theComplex->mGarrisonCount - theAmount;
            if (theAmount < 0) theAmount = 0;
            long theDelta = theAmount - theComplex->mGarrisonCount;
            if (theComplex->GetUnitClass()->mGarrisonCapacity < theComplex->mGarrisonCount + theDelta)
                theDelta = theComplex->GetUnitClass()->mGarrisonCapacity - theComplex->mGarrisonCount;
            theComplex->AddGarrison(theDelta, theComplex->mGarrisonPopulation);
            if (theComplex->mGarrisonCount == theComplex->GetUnitClass()->mGarrisonCapacity) {
                EEUnitGoal* theGoal = EEUGRust::NewLoad(kGoalLoadGarrison, theComplex, theUnits, false);
                theComplex->QueueGoal(theGoal, 0);
                theComplex->StartAction(kActionWaitForGarrisonCost);
                theComplex->GetUnitClass()->ActionChanged(theComplex);
            }
        }
        break;
    case kSMUnitAttributeAmmunition:
        if (theComplex != NULL) {
            if (inPercentage) theAmount = UMFloatToLong((float)(unsigned long)theClass->mAmmunition * theAmount * 0.01f);
            if (inMode == kSMAttributeModeIncrease) theAmount += theComplex->mAmmunition;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theComplex->mAmmunition - theAmount;
            if (theAmount < 0) theAmount = 0;
            if (theComplex->mAmmunition != (unsigned long)theAmount)
                theComplex->mAmmunition = theAmount;
        }
        break;
    case kSMUnitAttributeAnimationIsPaused:
        if (theComplex != NULL)
            theComplex->SetAnimation(theComplex->mAnimation, theComplex->mPhysicalModel.GetFrame(), theAmount != 0);
        break;
    case kSMUnitAttributeReloadTime:
        if (theComplex != NULL) {
            theBase = theStats->mReloadTime;
            theValue = 1;
        }
        break;
    case kSMUnitAttributeGraphicScale:
        if (theComplex != NULL) {
            float theScale = inPercentage ? theClass->GetScale() * theAmount * 0.01f : (float)theAmount;
            if (inMode == kSMAttributeModeIncrease) theScale = theComplex->mPhysicalModel.GetScale() + theScale;
            else if (inMode == kSMAttributeModeDecrease) theScale = theComplex->mPhysicalModel.GetScale() - theScale;
            if (theScale <= 0.0f) theScale = 0.001f;
            theComplex->mPhysicalModel.SetScale(theScale);
        }
        break;
    case kSMUnitAttributePower:
        if (theComplex != NULL) {
            theBase = theStats->mPower;
            theValue = 2;
        }
        break;
    case kSMUnitAttributeRottingTime:
        if (theComplex != NULL) {
            theBase = theClass->mRottingTime;
            theValue = 3;
        }
        break;
    case kSMUnitAttributeConstructionComplete:
        if (theComplex != NULL)
            theComplex->SetBuilt(theAmount != 0, 1, 1);
        break;
    case kSMUnitAttributeUnitVariable1: case kSMUnitAttributeUnitVariable2: case kSMUnitAttributeUnitVariable3:
        if (theComplex != NULL) {
            long& theCounter = theComplex->mUnitVariables[inAttribute - 17];
            if (inMode == kSMAttributeModeIncrease) theAmount += theCounter;
            else if (inMode == kSMAttributeModeDecrease) theAmount = theCounter - theAmount;
            if (theAmount < 0) theAmount = 0;
            theCounter = theAmount;
        }
        break;
    }
    if (theValue == 0)
        return;
    long theCurrent = theValue == 1 ? theComplex->mReloadTimer
                    : theValue == 2 ? theComplex->mPower : theComplex->mRottingAwayTime;
    if (inPercentage) theAmount = UMFloatToLong((float)theBase * theAmount * 0.01f);
    if (inMode == kSMAttributeModeIncrease) theAmount += theCurrent;
    else if (inMode == kSMAttributeModeDecrease) theAmount = theCurrent - theAmount;
    if (theAmount < 0) theAmount = 0;
    if (theCurrent != theAmount) {
        if (theValue == 1)
            theComplex->mReloadTimer = theAmount;
        else if (theValue == 2)
            theComplex->mPower = theAmount;
        else
            theComplex->mRottingAwayTime = theAmount;
    }
}

// FUNCTION: EXE 0x5c9628
// Renames the object and renames its object conditions to "<name> Exists" (ee_scenario lists.rs)
void EESMObject::SetName(const UWideString& inName)
{
    this->mName = inName;
    if (gWorld.mScenarioManager != NULL)
        gWorld.mScenarioManager->ObjectChanged(this, false, 0);
}

// FUNCTION: EXE 0x5c96b7
// Sets the parent object; a parent chain that loops back to this object resets the parent to 0 (ee_scenario
// lists.rs set_parent)
void EESMObject::SetParent(long inParent)
{
    this->mParent = inParent;
    if (gWorld.mScenarioManager != NULL)
        gWorld.mScenarioManager->ObjectChanged(this, true, inParent);
}

// FUNCTION: EXE 0x5c97a8
// Replaces the object's unit list
void EESMObject::SetUnits(const std::vector<unsigned long>& inUnits)
{
    this->mUnits = inUnits;
}

// FUNCTION: EXE 0x5c97b7
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

// FUNCTION: EXE 0x5c98dd
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

// FUNCTION: EXE 0x5c9966
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

// FUNCTION: EXE 0x5c99c7
// Deletes a condition entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteCondition(unsigned long inIndex)
{
    delete this->mConditions[inIndex];
    this->mConditions.erase(this->mConditions.begin() + inIndex);
    if (inIndex == this->mConditions.size())
        this->mConditions[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
}

// FUNCTION: EXE 0x5c9a3e
// Deletes an effect entry; deleting the last one makes the previous entry end the list
void EESMTrigger::DeleteEffect(unsigned long inIndex)
{
    delete this->mEffects[inIndex];
    this->mEffects.erase(this->mEffects.begin() + inIndex);
    if (inIndex == this->mEffects.size())
        this->mEffects[inIndex - 1]->mOperator = kSMTriggerOperatorEnd;
}

// FUNCTION: EXE 0x5c9ab5
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

// FUNCTION: EXE 0x5c9caf
// (EESMTConditionWrapper::Evaluate: ee_scenario trigger.rs entry)

// FUNCTION: EXE 0x5c9d0e
// Returns the condition entry at an index, or NULL
EESMTConditionWrapper* EESMTrigger::GetCondition(unsigned long inIndex)
{
    EESMTConditionWrapper* theWrapper = NULL;
    if (inIndex < this->mConditions.size())
        theWrapper = this->mConditions[inIndex];
    return theWrapper;
}

// FUNCTION: EXE 0x5c9d32
// Returns the effect entry at an index, or NULL
EESMTEffectWrapper* EESMTrigger::GetEffect(unsigned long inIndex)
{
    EESMTEffectWrapper* theWrapper = NULL;
    if (inIndex < this->mEffects.size())
        theWrapper = this->mEffects[inIndex];
    return theWrapper;
}

// FUNCTION: EXE 0x5c9d56
// Reads the trigger's settings, condition entries and effect entries (id -1 becomes 0); returns the first error
// (ee_scenario save.rs trigger_load)
long EESMTrigger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText};
    EESMStream theStream = {&inFile, theStrings, NULL, this};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_trigger_load(&theTable, this);
}

// FUNCTION: EXE 0x5ca0b3
// Writes the trigger's settings, condition entries and effect entries; returns the first error
// (ee_scenario save.rs trigger_save)
long EESMTrigger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText};
    EESMStream theStream = {&outFile, theStrings, NULL, this};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_trigger_save(&theTable, this);
}

// FUNCTION: EXE 0x5ca29d
// Sets a condition entry's condition index and NOT flag
void EESMTrigger::SetCondition(unsigned long inIndex, bool inNot, unsigned long inCondition)
{
    this->mConditions[inIndex]->mIsNot = inNot;
    this->mConditions[inIndex]->mValue = inCondition;
}

// FUNCTION: EXE 0x5ca2c3
// Sets a condition entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetConditionOperator(unsigned long inIndex, long inOperator)
{
    if (inIndex == this->mConditions.size() - 1 && inOperator != 0)
        this->mConditions.push_back(new EESMTConditionWrapper);
    this->mConditions[inIndex]->mOperator = inOperator;
}

// FUNCTION: EXE 0x5ca331
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

// FUNCTION: EXE 0x5ca39b
// Sets an effect entry's effect index and delay
void EESMTrigger::SetEffect(unsigned long inIndex, long inDelay, unsigned long inEffect)
{
    this->mEffects[inIndex]->mDelay = inDelay;
    this->mEffects[inIndex]->mEffectIndex = inEffect;
}

// FUNCTION: EXE 0x5ca3c1
// Sets an effect entry's operator; a non-end operator on the last entry appends an empty entry
void EESMTrigger::SetEffectOperator(unsigned long inIndex, long inType)
{
    if (inIndex == this->mEffects.size() - 1 && inType != 0)
        this->mEffects.push_back(new EESMTEffectWrapper);
    this->mEffects[inIndex]->mOperator = inType;
}

// FUNCTION: EXE 0x5ca42f
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

// FUNCTION: EXE 0x5ca496
// Appends a condition entry to a trigger's list (vector<EESMTConditionWrapper*>::push_back)

// FUNCTION: EXE 0x5ca4c2
// Appends an effect entry to a trigger's list (vector<EESMTEffectWrapper*>::push_back)
