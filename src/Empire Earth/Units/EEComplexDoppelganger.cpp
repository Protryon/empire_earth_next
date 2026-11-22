// C:\EEX\Empire Earth\Units\EEComplexDoppelganger.cpp (file name inferred: no __FILE__; the class name). A complex
// unit's doppelganger (what another player last saw of it), then EEComplexUnit's Load/FinishLoad/RepairPointers/Save
// (0x1f2452..0x1f5180; the std template COMDATs close the unit at 0x1f5180..0x1f57d3). 0x1f1f19..0x1f57d3 (after
// EEActiveGroup.cpp; EEComplexUnit.cpp follows).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"

#include "ee/EEUnitKind.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEComplexDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEDataPreLoading.h"
#include <string.h>
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitAction.h"
#include "ee/EEPPatrolGroup.h"
#include <math.h>
#include <algorithm>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEPathManager.h"
#include "ee/EEUGRust.h"
#include "rust/sim/units.h"

// port (save version 0.256): each loaded unit's contact (collision) list as saved, by unit id, head first. The load
// makes its own contacts while it places the units, and EEPhysicsModel reads the list before it rebuilds it (a unit
// without contacts skips its collision step), so the loaded units collided differently in their first tick. Put back
// at the first tick in two passes over every unit (clear, then add): ClearContacts also removes the unit from its
// contacts' lists, so a unit-by-unit rebuild would undo its neighbours'.
static std::vector<std::pair<std::pair<EEComplexUnit*, unsigned long>, std::vector<unsigned long> > > sLoadedContacts;

static EEComplexUnit* LoadedComplexUnit(unsigned long inID)
{
    return EEUnitCast<EEComplexUnit>(static_cast<EEUnit*>(gWorld.GetUnit(inID)));
}

static void RestoreLoadedContacts()
{
    ee_units_dop_body(EE_UNITS_DOP_DO_RESTORE_CONTACTS, NULL, NULL, 0, 0);   // dop_file.rs
}

// Creates an empty complex doppelganger with no model, hit points or tile
EEComplexDoppelganger::EEComplexDoppelganger()
{
    this->Reset();
}

// Returns the remembered unit's animation state
long EEComplexDoppelganger::GetAnimation()
{
    return this->mAnimation;
}

// Returns itself: this doppelganger is a complex one
EEComplexDoppelganger* EEComplexDoppelganger::GetComplexDoppelganger()
{
    return this;
}

// Returns whether the remembered unit was fully built (the top bit of the packed hit points)
bool EEComplexDoppelganger::IsBuilt()
{
    return (this->mPackedHitPoints & 0x80000000) > 0;
}

// Returns the remembered animation frame
long EEComplexDoppelganger::GetFrame()
{
    return this->mFrame;
}

// Returns the remembered model
EEModel* EEComplexDoppelganger::GetModel()
{
    return this->mModel;
}

// Returns whether the remembered unit was a building foundation
bool EEComplexDoppelganger::IsFoundation()
{
    return this->mIsFoundation;
}

// Copies the remembered position and orientation into the caller's transformation
void EEComplexDoppelganger::GetTransformation(GETransformation& outTransformation)
{
    outTransformation = this->mTransformation;
}

// Returns the remembered scale
float EEComplexDoppelganger::GetScale()
{
    return this->mScale;
}

// Returns the remembered hit points (a resource's amount) without the built bit
long EEComplexDoppelganger::GetHitPoints()
{
    return this->mPackedHitPoints & 0x7fffffff;
}

// Returns the tile X the unit was seen on
long EEComplexDoppelganger::GetTileX()
{
    return this->mTileX;
}

// Returns the tile Y the unit was seen on
long EEComplexDoppelganger::GetTileY()
{
    return this->mTileY;
}

// Destroys the complex doppelganger, releasing its unit and model media
EEComplexDoppelganger::~EEComplexDoppelganger()
{
    this->Clear();
}

// Forgets the model, hit points, frame, animation, scale and tile (-10000, off the map) without releasing anything
void EEComplexDoppelganger::Reset()
{
    ee_units_dop_more(EE_UNITS_DOPX_DO_RESET, this, 0, 0);   // dop_more.rs
}

// Releases the remembered unit and the model's media, then resets
void EEComplexDoppelganger::Clear()
{
    ee_units_dop_more(EE_UNITS_DOPX_DO_CLEAR, this, 0, 0);   // dop_more.rs
}

// Remembers what a viewing player sees of a unit: model, frame, animation, hit points (a resource's amount),
// built flag, position and scale (a unit with no complex part stands built at its tile centre), then loads the media
void EEComplexDoppelganger::Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile)
{
    ee_units_dop_body(EE_UNITS_DOP_DO_INITIALIZE, this, inUnit, (int64_t)(uintptr_t)inPlayer, (int64_t)(uintptr_t)inTile);   // dop_file.rs
}

// Copies another complex doppelganger's remembered view for a new viewing player
void EEComplexDoppelganger::Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile)
{
    ee_units_dop_body(EE_UNITS_DOP_DO_INITIALIZE_COPY, this, inOther, (int64_t)(uintptr_t)inPlayer, (int64_t)(uintptr_t)inTile);   // dop_file.rs
}

// Reads the transformation, model index, hit points, frame, animation, scale and tile after the base data (the
// foundation flag only from minor version 0xe6 on); returns the first read error
long EEComplexDoppelganger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return (long)ee_units_dop_body(EE_UNITS_DOP_DO_LOAD, this, &inFile, inMajorVersion, inMinorVersion);   // dop_file.rs
}

// Turns the loaded model index back into its media (clamping the frame) and puts the doppelganger on its tile; frees
// it if the unit or model is gone; returns 0
long EEComplexDoppelganger::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return (long)ee_units_dop_more(EE_UNITS_DOPX_DO_REPAIR_POINTERS, this, (int64_t)inFirst, (int64_t)inSecond);   // dop_more.rs
}

// Writes the transformation, model index, hit points, frame, animation, scale, tile and foundation flag after the base
// data; returns the first write error
long EEComplexDoppelganger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return (long)ee_units_dop_body(EE_UNITS_DOP_DO_SAVE, this, &outFile, inMajorVersion, inMinorVersion);   // dop_file.rs
}

// the load's scratch values: the build queue entry's second, the slot of the next held unit or unit variable
static unsigned long sLoadSecond;
static size_t sLoadSlot;

// Reads the complex unit's state from the saved game, keeping units and groups as ids for RepairPointers and queueing
// its pending path; fields newer than the file get defaults; returns the read status. port (R5b.4, round 26): the
// body is Rust (ee_units unit_load.rs, with RTSUnit's and EEUnit's parts) over the records this class derives from,
// EEUnitSaveOperation's parts and dop_save.rs's sections
long EEComplexUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEUnitVectorItem theGoals(this);
    EEUnitSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, &theGoals};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                               (uint8_t)(EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0),
                               (uint8_t)(EESaveHasRunningState(inMajorVersion, inMinorVersion) ? 1 : 0)};
    return ee_units_unit_load(&theContext, &EEUnitSaveOperation, this, this, this, &theIn);
}

// Load stores the saved fractions as floats in the longs.
// Turns the saved hit point, flight, garrison and power fractions back into amounts (at least 1 if nonzero; port,
// R5b.4 round 27: ee_units unit_load.rs finish_fractions over the record); marks an old save's (<= 200) unbuilt land
// building at 1 hit point as a foundation, reloading its media
long EEComplexUnit::FinishLoad(long inMajorVersion, long inMinorVersion)
{
    return (long)ee_units_dop_more(EE_UNITS_DOPX_DO_FINISH_LOAD, this, inMajorVersion, inMinorVersion);   // dop_more.rs
}

/// Turns the loaded ids back into units, groups, research and behaviour, dropping the missing ones; strands a unit whose
// transport or owner vanished and clears goal queues that fail to repair; returns the last status. port (the units'
// lane, round 7): the body is Rust (ee_units dop_repair.rs) over ee_units_dop_host's statements
long EEComplexUnit::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return (long)ee_units_dop_repair_pointers(this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// RepairPointers's locals (the cargo and research kept, the new turret angles)
static std::vector<EEComplexUnit*> sRepairCargo;
static std::vector<EEProductionEntry> sRepairResearch;
static float* sRepairTurrets;

static float EEDopFloat(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, sizeof theValue);
    return theValue;
}

static int64_t EEDopBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof theBits);
    return theBits;
}

template <class T> static T* EEDopObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// an ignored target of dop_load.rs's six values (tile x, y, from x, y, unit id, time)
static EEComplexUnitAttacker EEDopAttacker(const int64_t* inValues)
{
    EEComplexUnitAttacker theAttacker;
    theAttacker.mTileX = (long)inValues[0];
    theAttacker.mTileY = (long)inValues[1];
    theAttacker.mFromX = (long)inValues[2];
    theAttacker.mFromY = (long)inValues[3];
    theAttacker.mUnitID = (unsigned long)inValues[4];
    theAttacker.mTime = (unsigned long)inValues[5];
    return theAttacker;
}

template <class T> static uint64_t EEDopHandle(T* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

// dop_file.rs's scratch values: the loaded model id, the contacts being restored (the originals' locals)
static long sDopModelID;
static EeUnitsFinishLoadIn sDopMaxima;   // FinishLoad's maxima (dop_more.rs)
static std::vector<std::pair<std::pair<EEComplexUnit*, unsigned long>, std::vector<unsigned long> > > sDopContacts;

// a complex doppelganger's member (dop_file.rs EE_UNITS_DOP_FIELD_*) read from the file
static long EEDopReadField(EEComplexDoppelganger* inDop, FSFile& inFile, int64_t inField)
{
    switch (inField) {
    case EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS: return inFile.Read(inDop->mPackedHitPoints);
    case EE_UNITS_DOP_FIELD_FRAME: return inFile.Read(inDop->mFrame);
    case EE_UNITS_DOP_FIELD_ANIMATION: return inFile.Read(inDop->mAnimation);
    case EE_UNITS_DOP_FIELD_SCALE: return inFile.Read(inDop->mScale);
    case EE_UNITS_DOP_FIELD_TILE_X: return inFile.Read(inDop->mTileX);
    case EE_UNITS_DOP_FIELD_TILE_Y: return inFile.Read(inDop->mTileY);
    default: return inFile.Read(inDop->mIsFoundation);
    }
}

// a complex doppelganger's member written to the file
static long EEDopWriteField(EEComplexDoppelganger* inDop, FSFile& outFile, int64_t inField)
{
    switch (inField) {
    case EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS: return outFile.Write(inDop->mPackedHitPoints);
    case EE_UNITS_DOP_FIELD_FRAME: return outFile.Write(inDop->mFrame);
    case EE_UNITS_DOP_FIELD_ANIMATION: return outFile.Write(inDop->mAnimation);
    case EE_UNITS_DOP_FIELD_SCALE: return outFile.Write(inDop->mScale);
    case EE_UNITS_DOP_FIELD_TILE_X: return outFile.Write(inDop->mTileX);
    case EE_UNITS_DOP_FIELD_TILE_Y: return outFile.Write(inDop->mTileY);
    default: return outFile.Write(inDop->mIsFoundation);
    }
}

// a complex doppelganger's member copied from another's
static void EEDopCopyField(EEComplexDoppelganger* inDop, EEComplexDoppelganger* inOther, int64_t inField)
{
    switch (inField) {
    case EE_UNITS_DOP_FIELD_MODEL: inDop->mModel = inOther->mModel; break;
    case EE_UNITS_DOP_FIELD_FRAME: inDop->mFrame = inOther->mFrame; break;
    case EE_UNITS_DOP_FIELD_ANIMATION: inDop->mAnimation = inOther->mAnimation; break;
    case EE_UNITS_DOP_FIELD_PACKED_HIT_POINTS: inDop->mPackedHitPoints = inOther->mPackedHitPoints; break;
    case EE_UNITS_DOP_FIELD_SCALE: inDop->mScale = inOther->mScale; break;
    case EE_UNITS_DOP_FIELD_TRANSFORMATION: inDop->mTransformation = inOther->mTransformation; break;
    case EE_UNITS_DOP_FIELD_TILE_X: inDop->mTileX = inOther->mTileX; break;
    case EE_UNITS_DOP_FIELD_TILE_Y: inDop->mTileY = inOther->mTileY; break;
    default: inDop->mIsFoundation = inOther->mIsFoundation; break;
    }
}

// The C++ of EEComplexUnit::RepairPointers's Rust body (ee_units dop_repair.rs; EE_UNITS_DOP_*): one statement of the
// original each, in its order; inA an object, inB and inC values
extern "C" int64_t ee_units_dop_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEUnitGoal* theGoal = EEDopObject<EEUnitGoal>(inA);
    EEComplexUnit* theOther = EEDopObject<EEComplexUnit>(inA);
    EETechTreeNode* theNode = EEDopObject<EETechTreeNode>(inA);
    EEComplexDoppelganger* theDop = static_cast<EEComplexDoppelganger*>(inUnit);
    FSFile* theFile = EEDopObject<FSFile>(inA);
    EEUnit* theSeen = EEDopObject<EEUnit>(inA);
    switch (inOp) {
    case EE_UNITS_DOP_BASE_REPAIR: return theUnit->EEUnit::RepairPointers((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOP_COMMAND_IS_NULL: return theUnit->mCommand == NULL;
    case EE_UNITS_DOP_HAS_CLASS: return theUnit->GetUnitClass() != NULL;
    case EE_UNITS_DOP_START_ACTION: theUnit->StartAction((long)inB); return 0;
    case EE_UNITS_DOP_SET_PATROL_GROUP: theUnit->mPatrolGroup = EEDopObject<EEPPatrolGroup>(inA); return 0;
    case EE_UNITS_DOP_SET_CURRENT_RESEARCH: theUnit->mCurrentResearch = (long)inB; return 0;
    case EE_UNITS_DOP_BUILD_QUEUE_CLEAR: theUnit->mBuildQueue.clear(); return 0;
    case EE_UNITS_DOP_CITIZEN_QUEUE_CLEAR: theUnit->mCitizenBuildQueue.clear(); return 0;
    case EE_UNITS_DOP_CARGO_CLEAR: theUnit->mCargo.clear(); return 0;
    case EE_UNITS_DOP_SET_PREVIOUS_PART: theUnit->mPreviousPart = theOther; return 0;
    case EE_UNITS_DOP_SET_NEXT_PART: theUnit->mNextPart = theOther; return 0;
    case EE_UNITS_DOP_SET_HOME_BASE: theUnit->mHomeBase = theOther; return 0;
    case EE_UNITS_DOP_SET_CONTAINER: theUnit->mContainer = theOther; return 0;
    case EE_UNITS_DOP_SET_GATHERED_FIELD: theUnit->mGatheredResource = EEDopObject<EEUnit>(inA); return 0;
    case EE_UNITS_DOP_SET_LAST_RESOURCE_FIELD: theUnit->mLastResource = EEDopObject<EEUnit>(inA); return 0;
    case EE_UNITS_DOP_SET_LAST_ATTACKER: theUnit->mLastAttacker = theOther; return 0;
    case EE_UNITS_DOP_SET_OWNER: theUnit->mOwner = theOther; return 0;
    case EE_UNITS_DOP_SET_MINE: theUnit->mMine = EEDopObject<EEUnit>(inA); return 0;
    case EE_UNITS_DOP_SET_HOLDER: theUnit->mHolder = theOther; return 0;
    case EE_UNITS_DOP_SET_BEHAVIOR: theUnit->mBehavior = EEDopObject<EEUnitBehavior>(inA); return 0;
    case EE_UNITS_DOP_SET_HERD: theUnit->mHerd = EEDopObject<EEHerd>(inA); return 0;
    case EE_UNITS_DOP_HELD_COUNT: return (int64_t)theUnit->mHeldUnits.size();
    case EE_UNITS_DOP_SET_HELD: theUnit->mHeldUnits[(size_t)inB] = theOther; return 0;
    case EE_UNITS_DOP_GOAL_QUEUE_COUNT: return (int64_t)theUnit->mGoalQueues.size();
    case EE_UNITS_DOP_GOAL_COUNT: return (int64_t)theUnit->mGoalQueues[(size_t)inB].mGoals.size();
    case EE_UNITS_DOP_GOAL_AT: return EEDopHandle(theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]);
    case EE_UNITS_DOP_GOAL_REPAIR: return theGoal->RepairPointers((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOP_GOAL_KIND: return theGoal->mKind;
    case EE_UNITS_DOP_ATTACKER_IS_OTHER: return static_cast<EEUGRust*>(theGoal)->GetAttacker() != theUnit;
    case EE_UNITS_DOP_SET_ATTACKER: static_cast<EEUGRust*>(theGoal)->SetAttacker(theUnit); return 0;
    case EE_UNITS_DOP_GOAL_QUEUE_CLEAR: theUnit->mGoalQueues[(size_t)inB].Clear(); return 0;
    case EE_UNITS_DOP_CITIZEN_COUNT: return (int64_t)theUnit->mCitizenBuildQueue.size();
    case EE_UNITS_DOP_CITIZEN_RESOLVE:
        theUnit->mCitizenBuildQueue[(size_t)inB] = static_cast<EEUnit*>(gWorld.GetUnit(USavedID(theUnit->mCitizenBuildQueue[(size_t)inB])));
        return 0;
    case EE_UNITS_DOP_CITIZEN_REFERENCE: theUnit->mCitizenBuildQueue[(size_t)inB]->Reference(); return 0;
    case EE_UNITS_DOP_CARGO_COUNT: return (int64_t)theUnit->mCargo.size();
    case EE_UNITS_DOP_CARGO_ID: return USavedID(theUnit->mCargo[(size_t)inB]);
    case EE_UNITS_DOP_HAS_LOADED_ID: return theUnit->GetPlayer()->HasLoadedUnitID((unsigned long)inB);
    case EE_UNITS_DOP_WORLD_COMPLEX: return EEDopHandle(EEUnitCast<EEComplexUnit>(gWorld.GetUnit((unsigned long)inB)));
    case EE_UNITS_DOP_REFERENCE: EEDopObject<RTSUnit>(inA)->Reference(); return 0;
    case EE_UNITS_DOP_SCRATCH_CARGO_CLEAR: sRepairCargo.clear(); return 0;
    case EE_UNITS_DOP_SCRATCH_CARGO_PUSH: sRepairCargo.push_back(theOther); return 0;
    case EE_UNITS_DOP_CARGO_ASSIGN: theUnit->mCargo = sRepairCargo; return 0;
    case EE_UNITS_DOP_ADD_REPATROL: theUnit->GetPlayer()->AddUnitToRepatrol(theUnit); return 0;
    case EE_UNITS_DOP_LOCATION_AUTO: return EELocationGoalCast(theGoal)->GetIsAutoWaypoint() == true;
    case EE_UNITS_DOP_BUILD_TECH: return static_cast<EEUGRust*>(theGoal)->GetBuildTechTreeID();
    case EE_UNITS_DOP_COMMAND_TYPE: return theUnit->GetCommandType();
    case EE_UNITS_DOP_PHYSICS_ACTION: return theUnit->mCurrentPhysicsAction;
    case EE_UNITS_DOP_SET_PHYSICS_ACTION: theUnit->mCurrentPhysicsAction = (long)inB; return 0;
    case EE_UNITS_DOP_PATROL_IS_UNSET: return theUnit->mPatrolGroup == (EEPPatrolGroup*)-1;
    case EE_UNITS_DOP_PLAYER_PATROL_GROUP: return EEDopHandle(theUnit->GetPlayer()->GetPatrolGroup(USavedID(theUnit->mPatrolGroup)));
    case EE_UNITS_DOP_PATROL_GROUP: return EEDopHandle(theUnit->mPatrolGroup);
    case EE_UNITS_DOP_PATROL_REFERENCE: ee_units_pg_reference(theUnit->mPatrolGroup); return 0;
    case EE_UNITS_DOP_PREVIOUS_PART_ID: return USavedID(theUnit->mPreviousPart);
    case EE_UNITS_DOP_NEXT_PART_ID: return USavedID(theUnit->mNextPart);
    case EE_UNITS_DOP_HOME_BASE_ID: return USavedID(theUnit->mHomeBase);
    case EE_UNITS_DOP_CONTAINER_ID: return USavedID(theUnit->mContainer);
    case EE_UNITS_DOP_TRANSPORT_CAPACITY: return theOther->mUnitClass->mTransportCapacity;
    case EE_UNITS_DOP_HAS_CARGO_SELF: return theOther->HasCargo(theUnit);
    case EE_UNITS_DOP_HAS_CARGO_ID: return theOther->HasCargo(reinterpret_cast<EEComplexUnit*>(theUnit->mUnitID));
    case EE_UNITS_DOP_CURRENT_RESEARCH: return theUnit->mCurrentResearch;
    case EE_UNITS_DOP_SET_PRODUCTION_TIME: theUnit->mProductionTime = (long)inB; return 0;
    case EE_UNITS_DOP_TECH_NODE: return EEDopHandle(theUnit->mPlayer->mTechnologies->Get((long)inB));
    case EE_UNITS_DOP_IS_LAND_BUILDING: return theUnit->GetUnitClass()->IsLandBuilding();
    case EE_UNITS_DOP_NODE_CLASS_MISSING:
        return theNode->mData->mUnitClass < 0 || gUnitClassTable[theNode->mData->mUnitClass] == NULL;
    case EE_UNITS_DOP_NODE_DISALLOW: theNode->mIsAllowed = false; return 0;
    case EE_UNITS_DOP_BUILD_QUEUE_COUNT: return (int64_t)theUnit->mBuildQueue.size();
    case EE_UNITS_DOP_BUILD_QUEUE_TECH: return theUnit->mBuildQueue[(size_t)inB].mTechnology;
    case EE_UNITS_DOP_SCRATCH_RESEARCH_CLEAR: sRepairResearch.clear(); return 0;
    case EE_UNITS_DOP_SCRATCH_RESEARCH_PUSH: sRepairResearch.push_back(theUnit->mBuildQueue[(size_t)inB]); return 0;
    case EE_UNITS_DOP_BUILD_QUEUE_ASSIGN: theUnit->mBuildQueue = sRepairResearch; return 0;
    case EE_UNITS_DOP_GATHERED_LINK: return EEDopHandle(dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(theUnit->mGatheredResource))));
    case EE_UNITS_DOP_LINK_USABLE:
        return EEDopObject<EEUnit>(inA)->GetUnitID() != 0 || EEDopObject<EEUnit>(inA)->mResourceType != kResourceNone;
    case EE_UNITS_DOP_SET_GATHERED_RESOURCE: theUnit->SetGatheredResource(EEDopObject<EEUnit>(inA)); return 0;
    case EE_UNITS_DOP_LAST_ATTACKER_ID: return USavedID(theUnit->mLastAttacker);
    case EE_UNITS_DOP_HAS_GHOSTS: return EESaveHasGhosts((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOP_FIND_LOADED_GHOST: return EEDopHandle(EEFindLoadedGhost((unsigned long)inB));
    case EE_UNITS_DOP_OWNER_IS_UNSET: return theUnit->mOwner == (EEComplexUnit*)-1 || theUnit->mOwner == NULL;
    case EE_UNITS_DOP_OWNER_ID: return USavedID(theUnit->mOwner);
    case EE_UNITS_DOP_MINE_IS_UNSET: return theUnit->mMine == (EEUnit*)-1;
    case EE_UNITS_DOP_MINE_LINK: return EEDopHandle(dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(theUnit->mMine))));
    case EE_UNITS_DOP_MODEL_PARTS: return (int64_t)theUnit->GetModel()->GetNumParts();
    case EE_UNITS_DOP_NUM_TURRET_ANGLES: return (int64_t)theUnit->mNumTurretAngles;
    case EE_UNITS_DOP_SCRATCH_TURRETS_NEW: sRepairTurrets = inB == 0 ? NULL : new float[(size_t)inB]; return 0;
    case EE_UNITS_DOP_TURRET_ANGLE: return EEDopBits(theUnit->mTurretAngles[(size_t)inB]);
    case EE_UNITS_DOP_SCRATCH_TURRET_SET: sRepairTurrets[(size_t)inB] = EEDopFloat(inC); return 0;
    case EE_UNITS_DOP_TURRETS_DELETE: delete[] theUnit->mTurretAngles; return 0;
    case EE_UNITS_DOP_TURRETS_ASSIGN: theUnit->mTurretAngles = sRepairTurrets; return 0;
    case EE_UNITS_DOP_PAST_LAST_FRAME:
        return theUnit->GetStats()->mModel->mLastFrames[theUnit->mAnimation] <= theUnit->mPhysicalModel.GetFrame();
    case EE_UNITS_DOP_RESTART_ANIMATION:
        theUnit->SetAnimationFrame(theUnit->mStats->mModel->mStartFrames[theUnit->mAnimation]);
        return 0;
    case EE_UNITS_DOP_POST_LOAD_TECHNOLOGY: gDataPreLoading->PostLoadTechnology(theUnit->GetPlayer()->mPlayerID, (long)inB); return 0;
    case EE_UNITS_DOP_OWN_TECH_NODE: return EEDopHandle(theUnit->GetTechTreeNode());
    case EE_UNITS_DOP_NODE_KIND: return theNode->mData->mKind;
    case EE_UNITS_DOP_HIT_POINTS: return theUnit->GetHitPoints();
    case EE_UNITS_DOP_IS_BUILT: return theUnit->mIsBuilt == true;
    case EE_UNITS_DOP_ADD_WONDER: theUnit->GetPlayer()->AddWonder(theUnit, inB != 0); return 0;
    case EE_UNITS_DOP_HELD_RESOLVE:
        theUnit->mHeldUnits[(size_t)inB] = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(theUnit->mHeldUnits[(size_t)inB])));
        return 0;
    case EE_UNITS_DOP_HELD_AT: return EEDopHandle(theUnit->mHeldUnits[(size_t)inB]);
    case EE_UNITS_DOP_HELD_ERASE: theUnit->mHeldUnits.erase(theUnit->mHeldUnits.begin() + (size_t)inB); return 0;
    case EE_UNITS_DOP_HOLDER_IS_UNSET: return theUnit->mHolder == (EEComplexUnit*)-1;
    case EE_UNITS_DOP_HOLDER_ID: return USavedID(theUnit->mHolder);
    case EE_UNITS_DOP_ADD_STRANDED: theUnit->GetPlayer()->AddStrandedUnit(theUnit); return 0;
    case EE_UNITS_DOP_BEHAVIOR_ID: return USavedID(theUnit->mBehavior);
    case EE_UNITS_DOP_HAS_LIVE_STATE: return EESaveHasLiveState((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOP_BEHAVIOR_GET: return EEDopHandle(EEUnitBehavior::Get((unsigned long)inB));
    case EE_UNITS_DOP_BEHAVIOR: return EEDopHandle(theUnit->mBehavior);
    case EE_UNITS_DOP_CLASS_BEHAVIOR: return EEDopHandle(theUnit->GetUnitClass()->mBehavior);
    case EE_UNITS_DOP_DEFAULT_BEHAVIOR: return EEDopHandle(EEUnitBehavior::sTable[5]);
    case EE_UNITS_DOP_HAS_PLAYER: return theUnit->GetPlayer() != NULL;
    case EE_UNITS_DOP_TAGS_EXCEEDED: return theUnit->mStats->mModel->mTagPoints[theUnit->mAnimation] != NULL && theUnit->mStats->mModel->mTagPoints[theUnit->mAnimation]->GetNumTagPoints() <= theUnit->mWeaponTagPoint;
    case EE_UNITS_DOP_SET_WEAPON_TAG_POINT: theUnit->mWeaponTagPoint = (long)inB; return 0;
    case EE_UNITS_DOP_LAST_RESOURCE_LINK: return EEDopHandle(dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(theUnit->mLastResource))));
    case EE_UNITS_DOP_SET_LAST_RESOURCE: theUnit->SetLastResource(EEDopObject<EEUnit>(inA)); return 0;
    case EE_UNITS_DOP_AREA_EFFECTS_CLEAR: theUnit->mAreaEffects.Clear(); return 0;
    case EE_UNITS_DOP_AREA_EFFECTS_ITEMS_CLEAR: theUnit->mAreaEffects.mItems.clear(); return 0;
    case EE_UNITS_DOP_NUM_AREA_EFFECTS: return (int64_t)theUnit->mNumAreaEffects;
    case EE_UNITS_DOP_MAKE_AREA_EFFECTS: theUnit->MakeAreaEffects(); return 0;
    case EE_UNITS_DOP_HAS_RUNNING_STATE: return EESaveHasRunningState((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOP_SET_NUM_AREA_EFFECTS: theUnit->mNumAreaEffects = (unsigned long)inB; return 0;
    case EE_UNITS_DOP_REFRESH_GOALS: theUnit->RefreshGoals(); return 0;
    case EE_UNITS_DOP_REFRESH_ACTIVE_GOAL: theUnit->RefreshActiveGoal(); return 0;
    case EE_UNITS_DOP_FIRST_QUEUE_BACK: return EEDopHandle(theUnit->mGoalQueues[0].mGoals.back());
    case EE_UNITS_DOP_IS_AUTO_WAYPOINT_GOAL:
        return EELocationGoalCast(theGoal) != NULL && EELocationGoalCast(theGoal)->GetIsAutoWaypoint();
    case EE_UNITS_DOP_FIRST_QUEUE_POP_BACK: theUnit->mGoalQueues[0].PopBack(); return 0;
    case EE_UNITS_DOP_CLASS_ROTTING_TIME: return theUnit->GetUnitClass()->mRottingTime;
    case EE_UNITS_DOP_ROTTING_AWAY_TIME: return theUnit->mRottingAwayTime;
    case EE_UNITS_DOP_SET_ROTTING_AWAY_TIME: theUnit->mRottingAwayTime = (long)inB; return 0;
    case EE_UNITS_DOP_DELETABLE_FROM_CLASS: theUnit->mIsDeletable = theUnit->GetUnitClass()->mIsDeletable; return 0;
    case EE_UNITS_DOP_CATEGORY: return theUnit->GetUnitClass()->mCategory;
    case EE_UNITS_DOP_COMMAND_KIND: return theUnit->mCommand->mType;
    case EE_UNITS_DOP_RESOURCE_AMOUNT: return theUnit->mResourceAmount;
    case EE_UNITS_DOP_SET_RESOURCE_AMOUNT: theUnit->mResourceAmount = (long)inB; return 0;
    // dop_file.rs's
    case EE_UNITS_DOP_BASE_LOAD: return theDop->EEDoppelganger::Load((unsigned long)inB, (unsigned long)inC, *theFile);
    case EE_UNITS_DOP_LOAD_TRANSFORMATION: theDop->mTransformation.Load(*theFile); return 0;
    case EE_UNITS_DOP_READ_MODEL_ID: return theFile->Read(sDopModelID);
    case EE_UNITS_DOP_SET_MODEL_FROM_ID: theDop->mModel = reinterpret_cast<EEModel*>(sDopModelID); return 0;
    case EE_UNITS_DOP_READ_FIELD: return EEDopReadField(theDop, *theFile, inB);
    case EE_UNITS_DOP_BASE_SAVE: return theDop->EEDoppelganger::Save((unsigned long)inB, (unsigned long)inC, *theFile);
    case EE_UNITS_DOP_SAVE_TRANSFORMATION: theDop->mTransformation.Save(*theFile); return 0;
    case EE_UNITS_DOP_WRITE_MODEL_ID: return theFile->Write((long)theDop->mModel->mMediaIndex);
    case EE_UNITS_DOP_WRITE_FIELD: return EEDopWriteField(theDop, *theFile, inB);
    case EE_UNITS_DOP_BASE_INITIALIZE:
        theDop->EEDoppelganger::Initialize(theSeen, EEDopObject<EEPlayer>((uint64_t)inB), EEDopObject<RTSTile>((uint64_t)inC));
        return 0;
    case EE_UNITS_DOP_SEEN_IS_RESOURCE: return theSeen->mUnitClass->mCategory == kUnitCategoryResource;
    case EE_UNITS_DOP_PACK_RESOURCE: theDop->mPackedHitPoints = theSeen->mResourceAmount | 0x80000000; return 0;
    case EE_UNITS_DOP_PACK_HIT_POINTS: theDop->mPackedHitPoints = theSeen->mHitPoints; return 0;
    case EE_UNITS_DOP_SEEN_COMPLEX: return EEDopHandle(theSeen->mComplexUnit);
    case EE_UNITS_DOP_SEEN_IS_FOUNDATION: return theOther->IsFoundation() == true;
    case EE_UNITS_DOP_MODEL_FOUNDATION:
        theDop->mModel = EEGetPlayerClassGraphic(theOther->GetPlayer(), EEDopObject<EEUnit>((uint64_t)inB)->mUnitClass);
        return 0;
    case EE_UNITS_DOP_MODEL_OF: theDop->mModel = theSeen->GetModel(); return 0;
    case EE_UNITS_DOP_FRAME_OF: theDop->mFrame = theOther->mPhysicalModel.GetFrame(); return 0;
    case EE_UNITS_DOP_ANIMATION_OF: theDop->mAnimation = theOther->mAnimation; return 0;
    case EE_UNITS_DOP_SEEN_IS_BUILT: return theOther->mIsBuilt;
    case EE_UNITS_DOP_MARK_BUILT: theDop->mPackedHitPoints |= 0x80000000; return 0;
    case EE_UNITS_DOP_TRANSFORMATION_OF: theDop->mTransformation = theOther->mPhysicalModel.GetLocation(); return 0;
    case EE_UNITS_DOP_SCALE_OF: theDop->mScale = theOther->mPhysicalModel.GetScale(); return 0;
    case EE_UNITS_DOP_FOUNDATION_OF: theDop->mIsFoundation = theOther->IsFoundation(); return 0;
    case EE_UNITS_DOP_FRAME_ZERO: theDop->mFrame = 0; return 0;
    case EE_UNITS_DOP_ANIMATION_ZERO: theDop->mAnimation = 0; return 0;
    case EE_UNITS_DOP_SCALE_OF_CLASS: theDop->mScale = theSeen->mUnitClass->GetScale(); return 0;
    case EE_UNITS_DOP_NOT_FOUNDATION: theDop->mIsFoundation = false; return 0;
    case EE_UNITS_DOP_IDENTITY: theDop->mTransformation.MakeIdentity(); return 0;
    case EE_UNITS_DOP_HALF_SIZE: return EEDopBits(theSeen->mUnitClass->mTileSize * 0.5f);
    case EE_UNITS_DOP_SEEN_TILE: return inB == 0 ? theSeen->mTileX : theSeen->mTileY;
    case EE_UNITS_DOP_MESH_Z: return EEDopBits(::gWorld.mMap->mTerrainMesh.GetMeshZ(EEDopFloat(inB), EEDopFloat(inC)));
    case EE_UNITS_DOP_SET_POSITION:
        theDop->mTransformation.SetPosition(GE3DPoint(EEDopFloat(inB), EEDopFloat(inC), EEDopFloat((int64_t)inA)));
        return 0;
    case EE_UNITS_DOP_SET_TILE_X: theDop->mTileX = (long)inB; return 0;
    case EE_UNITS_DOP_SET_TILE_Y: theDop->mTileY = (long)inB; return 0;
    case EE_UNITS_DOP_LOAD_MEDIA: theDop->mModel->LoadMedia(); return 0;
    case EE_UNITS_DOP_BASE_INITIALIZE_COPY:
        theDop->EEDoppelganger::Initialize(EEDopObject<EEDoppelganger>(inA), EEDopObject<EEPlayer>((uint64_t)inB), EEDopObject<RTSTile>((uint64_t)inC));
        return 0;
    case EE_UNITS_DOP_COPY_FIELD: EEDopCopyField(theDop, dynamic_cast<EEComplexDoppelganger*>(EEDopObject<EEDoppelganger>(inA)), inB); return 0;
    case EE_UNITS_DOP_CONTACTS_TAKE: sDopContacts.clear(); sDopContacts.swap(sLoadedContacts); return 0;
    case EE_UNITS_DOP_CONTACTS_COUNT: return (int64_t)sDopContacts.size();
    case EE_UNITS_DOP_CONTACTS_VALID:
        return static_cast<RTSUnit*>(sDopContacts[(size_t)inB].first.first) == gWorld.GetUnit(sDopContacts[(size_t)inB].first.second);
    case EE_UNITS_DOP_CONTACTS_CLEAR: sDopContacts[(size_t)inB].first.first->ClearContacts(); return 0;
    case EE_UNITS_DOP_CONTACTS_ID_COUNT: return (int64_t)sDopContacts[(size_t)inB].second.size();
    case EE_UNITS_DOP_CONTACTS_UNIT: return EEDopHandle(LoadedComplexUnit(sDopContacts[(size_t)inB].second[(size_t)inC]));
    case EE_UNITS_DOP_CONTACTS_ADD: sDopContacts[(size_t)inB].first.first->AddContact(theOther); return 0;
    // dop_load.rs's live state at the first tick
    case EE_UNITS_DOP_IS_WORLD_UNIT: return static_cast<RTSUnit*>(theUnit) == gWorld.GetUnit((unsigned long)inB);
    case EE_UNITS_DOP_SET_ANIMATION_FRACTION: theUnit->mAnimationFraction = EEDopFloat(inB); return 0;
    case EE_UNITS_DOP_SET_NEXT_FRAME: theUnit->mNextFrame = (long)inB; return 0;
    case EE_UNITS_DOP_SET_COLLISION_RADIUS: theUnit->mCollisionRadius = EEDopFloat(inB); return 0;
    case EE_UNITS_DOP_SET_PUSH_VELOCITY_X: theUnit->mPushVelocityX = EEDopFloat(inB); return 0;
    case EE_UNITS_DOP_SET_PUSH_VELOCITY_Y: theUnit->mPushVelocityY = EEDopFloat(inB); return 0;
    case EE_UNITS_DOP_SET_F24C: theUnit->mF24C = EEDopFloat(inB); return 0;
    case EE_UNITS_DOP_SET_AI_STATE: theUnit->mAIState = (long)inB; return 0;
    case EE_UNITS_DOP_SET_NEXT_TRIGGER_TIME: theUnit->mNextTriggerTime = (unsigned long)inB; return 0;
    case EE_UNITS_DOP_IGNORED_TARGETS_CLEAR: theUnit->mIgnoredTargets.clear(); return 0;
    case EE_UNITS_DOP_IGNORED_TARGETS_PUSH: theUnit->mIgnoredTargets.push_back(EEDopAttacker(EEDopObject<const int64_t>(inA))); return 0;
    case EE_UNITS_DOP_BOUNDED_PATH_TILES_CLEAR: theUnit->mBoundedPathTiles.clear(); return 0;
    case EE_UNITS_DOP_BOUNDED_PATH_TILES_PUSH: theUnit->mBoundedPathTiles.push_back(U2DPoint<long>((long)inB, (long)inC)); return 0;
    case EE_UNITS_DOP_SET_FRAME: theUnit->mPhysicalModel.SetFrame((unsigned long)inB); return 0;
    case EE_UNITS_DOP_SET_FLIGHT_TIME: theUnit->mFlightTime = (long)inB; return 0;
    case EE_UNITS_DOP_SET_LAST_HIT_POINTS: theUnit->mLastHitPoints = (long)inB; return 0;
    case EE_UNITS_DOP_SET_GARRISON_COUNT: theUnit->mGarrisonCount = (long)inB; return 0;
    case EE_UNITS_DOP_SET_POWER: theUnit->mPower = (long)inB; return 0;
    case EE_UNITS_DOP_MODEL_PARTS_OR_ZERO: return theUnit->GetModel() != NULL ? (int64_t)theUnit->GetModel()->GetNumParts() : 0;
    case EE_UNITS_DOP_HAS_TURRET_ANGLES: return theUnit->mTurretAngles != NULL;
    case EE_UNITS_DOP_SET_TURRET_ANGLE: theUnit->mTurretAngles[(size_t)inB] = EEDopFloat(inC); return 0;
    // dop_more.rs's (the doppelganger's Reset, Clear, RepairPointers; the unit's FinishLoad)
    case EE_UNITS_DOPX_ZERO_SCALE: theDop->mScale = 0.0f; return 0;
    case EE_UNITS_DOPX_CLEAR_MODEL: theDop->mModel = NULL; return 0;
    case EE_UNITS_DOPX_ZERO_HIT_POINTS: theDop->mPackedHitPoints = 0; return 0;
    case EE_UNITS_DOPX_ZERO_FRAME: theDop->mFrame = 0; return 0;
    case EE_UNITS_DOPX_ZERO_ANIMATION: theDop->mAnimation = 0; return 0;
    case EE_UNITS_DOPX_TILE_X_OFF: theDop->mTileX = -10000; return 0;
    case EE_UNITS_DOPX_TILE_Y_OFF: theDop->mTileY = -10000; return 0;
    case EE_UNITS_DOPX_NOT_FOUNDATION: theDop->mIsFoundation = false; return 0;
    case EE_UNITS_DOPX_BASE_CLEAR: theDop->EEDoppelganger::Clear(); return 0;
    case EE_UNITS_DOPX_HAS_MODEL: return theDop->mModel != NULL;
    case EE_UNITS_DOPX_UNLOAD_MODEL: theDop->mModel->UnloadMedia(); return 0;
    case EE_UNITS_DOPX_MODEL_SAVED_ID: return USavedID(theDop->mModel);
    case EE_UNITS_DOPX_BASE_REPAIR: return theDop->EEDoppelganger::RepairPointers((unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_DOPX_MEDIA: return (int64_t)(uintptr_t)EEMedia::Get((unsigned long)inB);
    case EE_UNITS_DOPX_LOAD_MEDIA: return EEDopObject<EEModel>(inA)->LoadMedia();
    case EE_UNITS_DOPX_SET_MODEL: theDop->mModel = EEDopObject<EEModel>(inA); return 0;
    case EE_UNITS_DOPX_CLAMP_FRAME: theDop->mFrame = min(theDop->mFrame, EEDopObject<EEModel>(inA)->mModel->GetFrameCount() - 1); return 0;
    case EE_UNITS_DOPX_ADD_TO_TILES: gDoppelgangerManager.AddToTiles(theDop, theDop->mTileX, theDop->mTileY, theDop->mUnit->mUnitClass->GetUnitType() == kUnitTypeAirport); return 0;
    case EE_UNITS_DOPX_FREE: gDoppelgangerManager.Free(theDop); return 0;
    case EE_UNITS_DOPX_FINISH_FRACTIONS: ee_units_unit_finish_fractions(theUnit, &(sDopMaxima = EeUnitsFinishLoadIn{theUnit->mStats->mMaxHitPoints, theUnit->mStats->mFlightTime, theUnit->GetUnitClass()->mGarrisonCapacity, theUnit->mStats->mPower})); return 0;
    case EE_UNITS_DOPX_OLD_FOUNDATION: return theUnit->mIsFoundation = theUnit->GetUnitClass()->IsLandBuilding() && theUnit->GetUnitClass()->GetUnitType() != kUnitTypeFarm && !theUnit->mIsBuilt && theUnit->GetHitPoints() == 1;
    case EE_UNITS_DOPX_UNLOAD_CLASS_MEDIA: theUnit->GetPlayer()->UnloadUnitClassMedia(theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_DOPX_POST_LOAD_CLASS: gDataPreLoading->PostLoadUnitClass(theUnit->GetPlayer()->mPlayerID, theUnit->GetUnitClass()->GetUnitType()); return 0;
    case EE_UNITS_DOPX_BASE_FINISH_LOAD: return theUnit->EEUnit::FinishLoad((long)inB, (long)inC);
    default: return 0;
    }
}

// RepairPointers's helper, out of line in this unit.
// Shows the model of the unit's current animation state at the given frame
void EEComplexUnit::SetAnimationFrame(unsigned long inFrame)
{
    unsigned long theState = this->mAnimation;
    this->mPhysicalModel.SetModel(this->GetModel()->mModels[theState]);
    this->mPhysicalModel.SetFrame(inFrame);
}

// Writes the complex unit's state to the saved game, units and groups as ids and hit points, flight time, garrison
// and power as fractions of their maximum; returns the write status.
// port (R5b.4, round 26): the body is Rust (ee_units unit_save.rs, with RTSUnit's and EEUnit's parts) over the records
// this class derives from and EEUnitSaveOperation's parts
long EEComplexUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEUnitSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                               (uint8_t)(EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0),
                               (uint8_t)(EESaveHasRunningState(inMajorVersion, inMinorVersion) ? 1 : 0)};
    return ee_units_unit_save(&theContext, &EEUnitSaveOperation, this, this, this, &theIn);
}

// The C++ parts of the units' saves (ee_units unit_save.rs): the file's primitives with the overload the original
// called, the composite members' Save and Load, and the loads' sets that call a method. The ids, maxima, floats, the
// vectors' and the contacts' sections and the loads' plain stores are read and written in place (ee_units
// unit_save_views.rs); the deques' and goal queues' sections, the path goal, the herd, the location and path loads and
// the compound sets are Rust (the units' lane, round 8: ee_units dop_save.rs), each statement an EE_UNITS_DOPS_* case
// (the shared locals below stand for the original's)
static U2DPoint<long> sSaveGoalTile;
static GETransformation sLoadLocation(false);
static EEComplexUnitWaypoint sLoadWaypoint;

int32_t EEUnitSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsUnitSaveIo* io)
{
    EEUnitSaveContext& theContext = *static_cast<EEUnitSaveContext*>(inContext);
    FSFile& outFile = *theContext.mFile;
    FSFile& inFile = outFile;
    EEUnit* theUnit = static_cast<EEUnit*>(theContext.mUnit);   // (the unit operations only)
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);   // (the complex operations only)
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(outFile, inOp, io);
    switch (inOp) {
    case EE_UNITS_UNIT_SAVE_PART:
        switch (inIndex) {
        case EE_UNITS_UNIT_SAVE_PART_PROJECTILE_START: return theComplex->mProjectileStart.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_LOCATION: return theComplex->mPhysicalModel.GetLocation().Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_ANGULAR_VELOCITY: return theComplex->mAngularVelocity.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_ORIENTATION: return theComplex->mOrientation.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_U0DC: return theComplex->mU0DC.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_COLOR: return theComplex->mColor.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_FIRE_EFFECT: return outFile.Write(theComplex->mFireEffect.mValue);
        case EE_UNITS_UNIT_SAVE_PART_LAST_MAP_POSITION: return theComplex->mLastMapPosition.Save(outFile);
        default: return 0;
        }
    // ---- dop_save.rs's statements
    case EE_UNITS_DOPS_CITIZEN_QUEUE_SIZE: io->i = (int64_t)theComplex->mCitizenBuildQueue.size(); return 0;
    case EE_UNITS_DOPS_CITIZEN_VALID: io->i = theComplex->mCitizenBuildQueue[inIndex]->mIsValid; return 0;
    case EE_UNITS_DOPS_CITIZEN_ID: io->i = (int64_t)theComplex->mCitizenBuildQueue[inIndex]->mUnitID; return 0;
    case EE_UNITS_DOPS_WAYPOINT_COUNT: io->i = (int64_t)theComplex->mWaypoints.size(); return 0;
    case EE_UNITS_DOPS_WAYPOINT_SAVE_LOCATION: return theComplex->mWaypoints[inIndex].mLocation.Save(outFile);
    case EE_UNITS_DOPS_WAYPOINT_DISTANCE: io->f = theComplex->mWaypoints[inIndex].mDistance; return 0;
    case EE_UNITS_DOPS_WAYPOINT_START_DISTANCE: io->f = theComplex->mWaypoints[inIndex].mStartDistance; return 0;
    case EE_UNITS_DOPS_MODEL_PARTS: io->i = (int64_t)theComplex->GetModel()->GetNumParts(); return 0;
    case EE_UNITS_DOPS_TURRET_ANGLE: io->f = theComplex->mTurretAngles[inIndex]; return 0;
    case EE_UNITS_DOPS_GOAL_QUEUE_COUNT: io->i = (int64_t)theComplex->mGoalQueues.size(); return 0;
    case EE_UNITS_DOPS_GOAL_QUEUE_SAVE: return theComplex->mGoalQueues[inIndex].Save(theContext.mMajorVersion, theContext.mMinorVersion, outFile);
    case EE_UNITS_DOPS_IGNORED_COUNT: io->i = (int64_t)theComplex->mIgnoredTargets.size(); return 0;
    case EE_UNITS_DOPS_IGNORED_TILE_X: io->i = theComplex->mIgnoredTargets[inIndex].mTileX; return 0;
    case EE_UNITS_DOPS_IGNORED_TILE_Y: io->i = theComplex->mIgnoredTargets[inIndex].mTileY; return 0;
    case EE_UNITS_DOPS_IGNORED_FROM_X: io->i = theComplex->mIgnoredTargets[inIndex].mFromX; return 0;
    case EE_UNITS_DOPS_IGNORED_FROM_Y: io->i = theComplex->mIgnoredTargets[inIndex].mFromY; return 0;
    case EE_UNITS_DOPS_IGNORED_UNIT_ID: io->i = (int64_t)theComplex->mIgnoredTargets[inIndex].mUnitID; return 0;
    case EE_UNITS_DOPS_IGNORED_TIME: io->i = (int64_t)theComplex->mIgnoredTargets[inIndex].mTime; return 0;
    case EE_UNITS_DOPS_PATH_GOAL: io->b = gPathManager.GetPathGoal(theComplex, sSaveGoalTile) ? 1 : 0; return 0;
    case EE_UNITS_DOPS_TILE_SAVE: return sSaveGoalTile.Save(outFile);
    case EE_UNITS_DOPS_TILE_RESET: sSaveGoalTile = U2DPoint<long>(); return 0;
    case EE_UNITS_DOPS_HAS_HERD: io->i = theComplex->mHerd != NULL; return 0;
    case EE_UNITS_DOPS_HERD_HAS_UNIT: theComplex->mHerd->HasUnit(theComplex); return 0;
    case EE_UNITS_DOPS_LOCATION_LOAD: return sLoadLocation.Load(inFile);
    case EE_UNITS_DOPS_SET_LOCATION: theComplex->mPhysicalModel.SetLocation(sLoadLocation); return 0;
    case EE_UNITS_DOPS_TILE_LOAD: return sSaveGoalTile.Load(inFile);
    case EE_UNITS_DOPS_ADD_PENDING: gPathManager.AddPending(theComplex->mUnitID, sSaveGoalTile); return 0;
    case EE_UNITS_DOPS_LOADING_SCENARIO: io->i = GetIsLoadingScenario() ? 1 : 0; return 0;
    case EE_UNITS_DOPS_HAS_CLASS: io->i = theComplex->GetUnitClass() != NULL; return 0;
    case EE_UNITS_DOPS_CLASS_SCALE: io->f = theComplex->GetUnitClass()->GetScale(); return 0;
    case EE_UNITS_DOPS_SET_SCALE: theComplex->mPhysicalModel.SetScale(io->f); return 0;
    case EE_UNITS_DOPS_FINDS_ACTION: return theComplex->GetUnitClass()->FindAction((unsigned long)io->i) != NULL;
    case EE_UNITS_DOPS_START_ACTION: theComplex->StartAction((unsigned long)io->i); return 0;
    case EE_UNITS_DOPS_HAS_LAST_ATTACKER: io->i = theComplex->mLastAttacker != NULL; return 0;
    case EE_UNITS_DOPS_DEREFERENCE_LAST_ATTACKER: theComplex->mLastAttacker->Dereference(); return 0;
    case EE_UNITS_DOPS_CLEAR_LAST_ATTACKER: theComplex->mLastAttacker = NULL; return 0;
    case EE_UNITS_DOPS_HAS_OWNER: io->i = theComplex->mOwner != NULL; return 0;
    case EE_UNITS_DOPS_DEREFERENCE_OWNER: theComplex->mOwner->Dereference(); return 0;
    case EE_UNITS_DOPS_CLEAR_OWNER: theComplex->mOwner = NULL; return 0;
    case EE_UNITS_DOPS_WAYPOINTS_CLEAR: theComplex->mWaypoints.clear(); return 0;
    case EE_UNITS_DOPS_WAYPOINT_LOAD_LOCATION: return sLoadWaypoint.mLocation.Load(inFile);
    case EE_UNITS_DOPS_SET_WAYPOINT_DISTANCE: sLoadWaypoint.mDistance = io->f; return 0;
    case EE_UNITS_DOPS_SET_WAYPOINT_START_DISTANCE: sLoadWaypoint.mStartDistance = io->f; return 0;
    case EE_UNITS_DOPS_WAYPOINT_PUSH: theComplex->mWaypoints.push_back(sLoadWaypoint); return 0;
    case EE_UNITS_DOPS_GOAL_QUEUE_LOAD: return theComplex->mGoalQueues[inIndex].Load(theContext.mMajorVersion, theContext.mMinorVersion, inFile);
    case EE_UNITS_DOPS_SPARE_QUEUE_LOAD: return theContext.mGoals->Load(theContext.mMajorVersion, theContext.mMinorVersion, inFile);
    case EE_UNITS_DOPS_READ_CONVERSION_EFFECT: return inFile.Read(theComplex->mConversionEffect.mValue);
    case EE_UNITS_DOPS_READ_HEAL_EFFECT: return inFile.Read(theComplex->mHealEffect.mValue);
    // ---- the loads (ee_units unit_load.rs)
    case EE_UNITS_UNIT_LOAD_TURRET_ANGLE:
        return inFile.Read(theComplex->mTurretAngles[inIndex]);
    case EE_UNITS_UNIT_LOAD_PART:
        switch (inIndex) {
        case EE_UNITS_UNIT_LOAD_PART_PROJECTILE_START: return theComplex->mProjectileStart.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_ANGULAR_VELOCITY: return theComplex->mAngularVelocity.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_ORIENTATION: return theComplex->mOrientation.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_U0DC: return theComplex->mU0DC.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_COLOR: return theComplex->mColor.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_FIRE_EFFECT: return inFile.Read(theComplex->mFireEffect.mValue);
        case EE_UNITS_UNIT_LOAD_PART_LAST_MAP_POSITION: return theComplex->mLastMapPosition.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_X: return inFile.Read(theComplex->mInitialContactLocation.mX);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_Y: return inFile.Read(theComplex->mInitialContactLocation.mY);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_Z: return inFile.Read(theComplex->mInitialContactLocation.mZ);
        default: return 0;
        }
    case EE_UNITS_UNIT_LOAD_SET:
        switch (inIndex) {
        case EE_UNITS_UNIT_LOAD_SET_TURRETS: theComplex->ResizeTurretAngles((long)io->i); return 0;
        // dop_load.rs's sections
        case EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_CLEAR: theComplex->mBuildQueue.clear(); return 0;
        case EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_SECOND: sLoadSecond = (unsigned long)io->i; return 0;
        case EE_UNITS_UNIT_LOAD_SET_BUILD_QUEUE_PUSH: theComplex->mBuildQueue.push_back(EEProductionEntry{(long)io->i, sLoadSecond}); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CITIZEN_QUEUE_CLEAR: theComplex->mCitizenBuildQueue.clear(); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CITIZEN_QUEUE_PUSH: theComplex->mCitizenBuildQueue.push_back(reinterpret_cast<EEUnit*>((unsigned long)io->i)); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CARGO_CLEAR: theComplex->mCargo.clear(); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CARGO_PUSH: theComplex->mCargo.push_back(reinterpret_cast<EEComplexUnit*>((unsigned long)io->i)); return 0;
        case EE_UNITS_UNIT_LOAD_SET_HELD_RESIZE: theComplex->mHeldUnits.resize((long)io->i, NULL); return 0;
        case EE_UNITS_UNIT_LOAD_SET_SLOT: sLoadSlot = (size_t)io->i; return 0;
        case EE_UNITS_UNIT_LOAD_SET_HELD_UNIT: theComplex->mHeldUnits[sLoadSlot] = reinterpret_cast<EEComplexUnit*>((unsigned long)io->i); return 0;
        case EE_UNITS_UNIT_LOAD_SET_UNIT_VARIABLE: theComplex->mUnitVariables[sLoadSlot] = (long)io->i; return 0;
        case EE_UNITS_UNIT_LOAD_SET_LIVE_REGISTER:
            EEAfterLoad([theSelf = theComplex, theID = (uint32_t)theComplex->mUnitID,
                         theLive = std::shared_ptr<void>((void*)(uintptr_t)io->i, &ee_units_unit_live_free)]() { ee_units_unit_live_apply(theSelf, theID, theLive.get()); });
            return 0;
        case EE_UNITS_UNIT_LOAD_SET_CONTACTS_REGISTER: EEAfterLoad(RestoreLoadedContacts); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CONTACTS_BEGIN: sLoadedContacts.push_back(std::make_pair(std::make_pair(theComplex, (unsigned long)theComplex->mUnitID), std::vector<unsigned long>())); return 0;
        case EE_UNITS_UNIT_LOAD_SET_CONTACT_ID: sLoadedContacts.back().second.push_back((unsigned long)io->i); return 0;
        default: return 0;
        }
    case EE_UNITS_UNIT_LOAD_GET:
        switch (inIndex) {
        case EE_UNITS_UNIT_LOAD_GET_TURRET_COUNT: io->i = theComplex->mTurretAngles != NULL ? theComplex->mNumTurretAngles : 0; return 0;
        case EE_UNITS_UNIT_LOAD_GET_TURRET_ANGLE: io->f = theComplex->mTurretAngles[(size_t)io->i]; return 0;
        case EE_UNITS_UNIT_LOAD_GET_CONTACTS_EMPTY: io->i = sLoadedContacts.empty(); return 0;
        default: return 0;
        }
    default:
        return 0;
    }
}

bool EEComplexDoppelganger::IsComplex()
{
    return true;
}
