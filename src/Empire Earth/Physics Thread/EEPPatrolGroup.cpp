// C:\EEX\Empire Earth\Physics Thread\EEPPatrolGroup.cpp (name from RTTI EEPPatrolGroup; no __FILE__). 0x16066d..0x16100f:
// the patrol points a group of units walks. EEPMLand.cpp's last function ended at 0x16066d, RTSPhysics.cpp starts at
// 0x16100f. Unit flags: the exe's module set.
//
// New (the physics models' lane, round 2): every member's body is Rust (ee_units patrol_group.rs, by the group's
// object: ee_units_patrol_group), the id counter too; the class keeps its fields (its holders read them) and these
// one-line forwarders. Below them, the C++ the bodies call (ee_units_pg_host_*), each the original statement it
// replaced. The ctor's ??_G (0x5606b0), the find_if functor (0x160fba..0x16100f) went with the bodies.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/EEUnitKind.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EESaveGameFiles.h"
#include "rust/ee_units.h"

static uint64_t PGObject(const void* inPointer)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(inPointer));
}

static EEComplexUnit* PGUnit(uint64_t inObject)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inObject));
}

static EEPPatrolGroup* PGGroup(uint64_t inObject)
{
    return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(inObject));
}

static uint64_t PGRun(int32_t inOp, const EEPPatrolGroup* inGroup, EeUnitsPgArgs inArgs)
{
    return ee_units_patrol_group(inOp, PGObject(inGroup), inArgs);
}

static EeUnitsPgArgs PGArgs(const void* inUnit = NULL, float inX = 0.0f, float inY = 0.0f, float inZ = 0.0f,
                            int32_t inA = 0, int32_t inB = 0)
{
    EeUnitsPgArgs theArgs = {};
    theArgs.unit = PGObject(inUnit);
    theArgs.point[0] = inX;
    theArgs.point[1] = inY;
    theArgs.point[2] = inZ;
    theArgs.a = inA;
    theArgs.b = inB;
    return theArgs;
}

// FUNCTION: EXE 0x56066d
// Creates an empty, open patrol group with the next patrol group id
EEPPatrolGroup::EEPPatrolGroup()
{
    this->mIsLoop = false;
    this->mID = (unsigned long)PGRun(EE_UNITS_PG_OP_NEW_ID, NULL, PGArgs());
    this->mIsTownPatrol = false;
}

// FUNCTION: EXE 0x5606cc
// Destroys the patrol group, detaching and killing every patrol point unit
EEPPatrolGroup::~EEPPatrolGroup()
{
    PGRun(EE_UNITS_PG_OP_DESTROY, this, PGArgs());
}

// FUNCTION: EXE 0x560727
// Adds a patrol point at the end at ground height; a point on the first point's tile closes the loop instead
void EEPPatrolGroup::AddPoint(const GE2DPoint& inPoint, unsigned long inPlayerID)
{
    PGRun(EE_UNITS_PG_OP_ADD_POINT, this, PGArgs(NULL, inPoint.GetX(), inPoint.GetY(), 0.0f, (int32_t)inPlayerID));
}

// FUNCTION: EXE 0x5607c8
// Creates a patrol point at the given spot after the last one
void EEPPatrolGroup::AppendPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID)
{
    PGRun(EE_UNITS_PG_OP_APPEND, this, PGArgs(NULL, inPoint.GetX(), inPoint.GetY(), inPoint.GetZ(), (int32_t)inPlayerID));
}

// FUNCTION: EXE 0x5607f4
// Returns the patrol point nearest to the tile (the first of equals), or NULL when there are none
EEComplexUnit* EEPPatrolGroup::GetNearest(long inTileX, long inTileY)
{
    return PGUnit(PGRun(EE_UNITS_PG_OP_NEAREST, this, PGArgs(NULL, 0.0f, 0.0f, 0.0f, (int32_t)inTileX, (int32_t)inTileY)));
}

// FUNCTION: EXE 0x56085d
// Returns the patrol point after the given one (the first for NULL); wraps to the first in a loop or a
// one-point patrol, else NULL at the end
EEComplexUnit* EEPPatrolGroup::GetNext(EEComplexUnit* inUnit)
{
    return PGUnit(PGRun(EE_UNITS_PG_OP_NEXT, this, PGArgs(inUnit)));
}

// FUNCTION: EXE 0x5608db
// Returns the index of the patrol point, or of the point nearest to the unit's tile when it is not one
unsigned long EEPPatrolGroup::IndexOf(EEComplexUnit* inUnit)
{
    return (unsigned long)PGRun(EE_UNITS_PG_OP_INDEX_OF, this, PGArgs(inUnit));
}

// FUNCTION: EXE 0x56098a
// Returns the patrol point before the given one (the first for NULL); wraps to the last in a loop, else
// NULL at the start
EEComplexUnit* EEPPatrolGroup::GetPrevious(EEComplexUnit* inUnit)
{
    return PGUnit(PGRun(EE_UNITS_PG_OP_PREVIOUS, this, PGArgs(inUnit)));
}

// FUNCTION: EXE 0x5609e4
// Creates a patrol point at a map position (at ground height) after the given point
void EEPPatrolGroup::InsertPatrolPointAt(const GE2DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inUnit)
{
    PGRun(EE_UNITS_PG_OP_INSERT_AT, this, PGArgs(inUnit, inPoint.GetX(), inPoint.GetY(), 0.0f, (int32_t)inPlayerID));
}

// FUNCTION: EXE 0x560a58
// Creates a path point unit for the player at the spot and inserts it after the given point (at the end
// for NULL)
void EEPPatrolGroup::InsertPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inAfter)
{
    PGRun(EE_UNITS_PG_OP_INSERT, this, PGArgs(inAfter, inPoint.GetX(), inPoint.GetY(), inPoint.GetZ(), (int32_t)inPlayerID));
}

// FUNCTION: EXE 0x560ade
// Reads the group's id, its patrol point unit ids (as placeholders for RepairPointers) and the loop flag
// from the saved game; returns the first read error
long EEPPatrolGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    EeUnitsPgArgs theArgs = PGArgs(NULL, 0.0f, 0.0f, 0.0f, (int32_t)inMajorVersion, (int32_t)inMinorVersion);
    theArgs.file = PGObject(inFile);
    return (long)(int32_t)(uint32_t)PGRun(EE_UNITS_PG_OP_LOAD, this, theArgs);
}

// FUNCTION: EXE 0x560b72
// Removes a patrol point; the player drops the group once it has none left
void EEPPatrolGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    PGRun(EE_UNITS_PG_OP_REMOVE, this, PGArgs(inUnit));
}

// FUNCTION: EXE 0x560bf2
// Turns the loaded unit ids back into patrol point units, claiming those that point at another group;
// returns 3 at the first id with no unit
long EEPPatrolGroup::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return (long)(int32_t)(uint32_t)PGRun(EE_UNITS_PG_OP_REPAIR, this, PGArgs());
}

// FUNCTION: EXE 0x560cdc
// Restarts patrol group ids at 0
void EEPPatrolGroup::ResetIDs()
{
    PGRun(EE_UNITS_PG_OP_RESET_IDS, NULL, PGArgs());
}

// FUNCTION: EXE 0x560ce4
// Writes the group's id, its patrol point unit ids and the loop flag to the saved game; returns the first
// write error
long EEPPatrolGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    EeUnitsPgArgs theArgs = PGArgs(NULL, 0.0f, 0.0f, 0.0f, (int32_t)inMajorVersion, (int32_t)inMinorVersion);
    theArgs.file = PGObject(inFile);
    return (long)(int32_t)(uint32_t)PGRun(EE_UNITS_PG_OP_SAVE, this, theArgs);
}

// FUNCTION: EXE 0x560d70
// Selects the patrol points for the player, stopping at the first that cannot be selected
void EEPPatrolGroup::Select(EEPlayer* inPlayer, bool inFlag)
{
    PGRun(EE_UNITS_PG_OP_SELECT, this, PGArgs(inPlayer, 0.0f, 0.0f, 0.0f, inFlag ? 1 : 0));
}

// FUNCTION: EXE 0x560da9
// Replaces the patrol route with the given tiles: kills points on no listed tile, adds missing ones and
// orders the points as listed, as a closed loop
void EEPPatrolGroup::SetPatrolPoints(std::vector<U2DPoint<long> > inPoints, unsigned long inPlayerID)
{
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inPoints.size(); i++) {
        theTiles.push_back((int32_t)inPoints[i].mX);
        theTiles.push_back((int32_t)inPoints[i].mY);
    }
    ee_units_patrol_group_set_points(PGObject(this), theTiles.empty() ? NULL : &theTiles[0], (uint32_t)inPoints.size(),
                                     (uint32_t)inPlayerID);
}

// ---- the C++ the Rust bodies call (ee_units patrol_group.rs; each the original statement it replaced)

extern "C" uint32_t ee_units_pg_host_points(uint64_t inGroup, uint64_t* outPoints, uint32_t inCapacity)
{
    const std::vector<EEComplexUnit*>& thePoints = PGGroup(inGroup)->mPatrolPoints;
    for (uint32_t i = 0; i < inCapacity && i < thePoints.size(); i++)
        outPoints[i] = PGObject(thePoints[i]);
    return (uint32_t)thePoints.size();
}

extern "C" void ee_units_pg_host_set_points(uint64_t inGroup, const uint64_t* inPoints, uint32_t inCount)
{
    std::vector<EEComplexUnit*>& thePoints = PGGroup(inGroup)->mPatrolPoints;
    thePoints.clear();
    for (uint32_t i = 0; i < inCount; i++)
        thePoints.push_back(PGUnit(inPoints[i]));
}

extern "C" void ee_units_pg_host_fields(uint64_t inGroup, uint8_t inSet, EeUnitsPgFields* ioFields)
{
    EEPPatrolGroup* theGroup = PGGroup(inGroup);
    if (inSet) {
        theGroup->mID = ioFields->id;
        theGroup->mIsLoop = ioFields->is_loop != 0;
        theGroup->mIsTownPatrol = ioFields->is_town_patrol != 0;
    } else {
        ioFields->id = (uint32_t)theGroup->mID;
        ioFields->is_loop = theGroup->mIsLoop ? 1 : 0;
        ioFields->is_town_patrol = theGroup->mIsTownPatrol ? 1 : 0;
    }
}

// a point's GetTile() (EEPatrolTileIs) and GetTileX/GetTileY (SetPatrolPoints, IndexOf)
extern "C" void ee_units_pg_host_unit_tile(uint64_t inUnit, int32_t* outX, int32_t* outY)
{
    EEComplexUnit* theUnit = PGUnit(inUnit);
    *outX = (int32_t)theUnit->GetTileX();
    *outY = (int32_t)theUnit->GetTileY();
}

extern "C" float ee_units_pg_host_distance(uint64_t inUnit, int32_t inX, int32_t inY)
{
    return PGUnit(inUnit)->DistanceSquaredToTileXY(inX, inY);
}

// the map's tile at ((long)x, (long)y): its height; whether it HasUnit(probe) (AddPoint's first point)
extern "C" uint8_t ee_units_pg_host_tile_at(float inX, float inY, uint64_t inProbe, float* outHeight)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile((long)inX, (long)inY);
    *outHeight = theTile->GetHeight();
    return inProbe != 0 && theTile->HasUnit(PGUnit(inProbe)) ? 1 : 0;
}

extern "C" uint64_t ee_units_pg_host_create_point(uint64_t inGroup, float inX, float inY, uint32_t inPlayerID)
{
    EEUnit* theCreated = gUnitClassTable[kUnitTypePathPoint]->CreateUnit((long)inX, (long)inY, (long)inPlayerID, 0, NULL);
    EEComplexUnit* theUnit = theCreated->GetComplexUnit();
    theUnit->SetPatrolGroup(PGGroup(inGroup));
    return PGObject(theUnit);
}

// the unit-side calls: patrol_group.rs EE_UNITS_PG_*
extern "C" uint64_t ee_units_pg_host_unit(int32_t inWhich, uint64_t inUnit, uint64_t inOther)
{
    EEComplexUnit* theUnit = PGUnit(inUnit);
    switch (inWhich) {
    case EE_UNITS_PG_SET_GROUP:
        theUnit->SetPatrolGroup(PGGroup(inOther));
        return 0;
    case EE_UNITS_PG_GROUP_OF:
        return PGObject(theUnit->GetPatrolGroup());
    case EE_UNITS_PG_KILL:
        theUnit->Kill(1, 0, 0);
        theUnit->GetPlayer()->KillUnit(theUnit, false);
        return 0;
    case EE_UNITS_PG_KILL_UNIT:
        theUnit->GetPlayer()->KillUnit(theUnit, false);
        return 0;
    case EE_UNITS_PG_REMOVE_GROUP:
        theUnit->GetPlayer()->RemovePatrolGroup(PGGroup(inOther));
        return 0;
    case EE_UNITS_PG_UNIT_ID:
        return (uint64_t)(uint32_t)theUnit->mUnitID;
    case EE_UNITS_PG_FIND_UNIT:
        return PGObject(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((long)(int32_t)(uint32_t)inOther)));
    case EE_UNITS_PG_SELECT:
    case EE_UNITS_PG_SELECT_FLAG:
        return reinterpret_cast<EEPlayer*>(static_cast<uintptr_t>(inOther))->TrySelectUnit(theUnit, inWhich == EE_UNITS_PG_SELECT_FLAG) ? 1 : 0;
    default:
        return 0;
    }
}

extern "C" uint8_t ee_units_pg_host_live_state(uint32_t inMajorVersion, uint32_t inMinorVersion)
{
    return EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0;
}

// the saved game's typed calls (unsigned long, bool): a read takes the value and gives back what the file left
extern "C" int32_t ee_units_pg_host_file(uint64_t inFile, int32_t inWhich, uint32_t* ioValue)
{
    FSFile* theFile = reinterpret_cast<FSFile*>(static_cast<uintptr_t>(inFile));
    switch (inWhich) {
    case EE_UNITS_PG_WRITE_U32:
        return (int32_t)theFile->Write((unsigned long)*ioValue);
    case EE_UNITS_PG_WRITE_BOOL:
        return (int32_t)theFile->Write(*ioValue != 0);
    case EE_UNITS_PG_READ_U32: {
        unsigned long theValue = *ioValue;
        long theResult = theFile->Read(theValue);
        *ioValue = (uint32_t)theValue;
        return (int32_t)theResult;
    }
    default: {
        bool theValue = *ioValue != 0;
        long theResult = theFile->Read(theValue);
        *ioValue = theValue ? 1 : 0;
        return (int32_t)theResult;
    }
    }
}
