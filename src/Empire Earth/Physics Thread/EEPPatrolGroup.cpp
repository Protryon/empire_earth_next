// C:\EEX\Empire Earth\Physics Thread\EEPPatrolGroup.cpp (name from RTTI EEPPatrolGroup; no __FILE__). 0x16066d..0x16100f:
// the patrol points a group of units walks. EEPMLand.cpp's last function ended at 0x16066d, RTSPhysics.cpp starts at
// 0x16100f. Unit flags: the exe's module set.
//
// port (the units' state lane, round 1): a patrol group is Rust's object (ee_units patrol_group.rs: its state and
// every member's body; the class and its forwarders are gone, the holders call ee_units_pg_*). Here: the C++ the
// bodies call (ee_units_pg_host_*), each the original statement it replaced, and SetPatrolPoints' vector adapter.
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
#include "rust/sim/units.h"

static uint64_t PGObject(const void* inPointer)
{
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(inPointer));
}

static EEComplexUnit* PGUnit(uint64_t inObject)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inObject));
}

static FSFile* PGFile(uint64_t inFile)
{
    return reinterpret_cast<FSFile*>(static_cast<uintptr_t>(inFile));
}

static EEPPatrolGroup* PGGroup(uint64_t inObject)
{
    return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(inObject));
}

// Replaces the patrol route with the given tiles: kills points on no listed tile, adds missing ones and orders the
// points as listed, as a closed loop
void EEPGSetPatrolPoints(EEPPatrolGroup* inGroup, const std::vector<U2DPoint<long> >& inPoints, unsigned long inPlayerID)
{
    ee_units_pg_set_points(inGroup, inPoints.empty() ? NULL : &inPoints[0].mX, (uint32_t)inPoints.size(), (uint32_t)inPlayerID);
}

// ---- the C++ the Rust bodies call (ee_units patrol_group.rs; each the original statement it replaced)

extern "C" float ee_units_pg_host_distance(uint64_t inUnit, int32_t inX, int32_t inY)
{
    return PGUnit(inUnit)->DistanceSquaredToTileXY(inX, inY);
}

// the map's tile at ((long)x, (long)y) HasUnit(probe) (AddPoint's first point)
extern "C" uint8_t ee_units_pg_host_tile_has(float inX, float inY, uint64_t inProbe)
{
    return ::gWorld.mMap->GetTile((long)inX, (long)inY)->HasUnit(PGUnit(inProbe)) ? 1 : 0;
}

// a path point of the player's at (x, y) (patrol_group.rs create_point gives it the group)
extern "C" uint64_t ee_units_pg_host_create_point(float inX, float inY, uint32_t inPlayerID)
{
    return PGObject(gUnitClassTable[kUnitTypePathPoint]->CreateUnit((long)inX, (long)inY, (long)inPlayerID, 0, NULL));
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

// the saved game's typed calls (unsigned long, bool)
extern "C" int32_t ee_units_pg_host_write(uint64_t inFile, int32_t inWhich, uint32_t inValue)
{
    return inWhich == EE_UNITS_PG_WRITE_U32 ? (int32_t)PGFile(inFile)->Write((unsigned long)inValue) : (int32_t)PGFile(inFile)->Write(inValue != 0);
}

extern "C" int32_t ee_units_pg_host_read_u32(uint64_t inFile, unsigned long* ioValue)
{
    return (int32_t)PGFile(inFile)->Read(*ioValue);
}

extern "C" int32_t ee_units_pg_host_read_bool(uint64_t inFile, bool* ioValue)
{
    return (int32_t)PGFile(inFile)->Read(*ioValue);
}
