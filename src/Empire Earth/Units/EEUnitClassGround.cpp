// EEUnitClassGround.cpp (new, R5b.4 round 32): the ambient, resource, land and naval kinds' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEWheel.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"   // EEObj (the host)

template <class T> static T* EEUcgObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The C++ of the ground classes' Rust bodies (ee_units ucg_ground.rs; EE_UNITS_UCG_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucg_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcgObject<EEComplexUnit>(inA);
    EEUnit* theSimple = EEUcgObject<EEUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCG_TILE_ADD: EEUcgObject<EEMapTile>(inA)->mUnits = EEUcgObject<EEMapTile>(inA)->AddUnit(EEUcgObject<EEUnit>((uint64_t)inB), EEUcgObject<EEMapTile>(inA)->mUnits); return 0;
    case EE_UNITS_UCG_TILE_REMOVE: EEUcgObject<EEMapTile>(inA)->RemoveUnit(EEUcgObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCG_BASE_SIMPLE_CREATED: theClass->BaseSimpleUnitCreated(theSimple); return 0;
    case EE_UNITS_UCG_SET_RESOURCE_TYPE: theSimple->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCG_SET_RESOURCE_AMOUNT: theSimple->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCG_SET_CLASS_POWER: theUnit->SetPower(theUnit->mUnitClass->mPower); return 0;
    case EE_UNITS_UCG_MAP_TILE: return EEObj(::gWorld.GetMap()->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCG_CREATE_PLANE: return EEObj(EEUcgObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcgObject<GETransformation>((uint64_t)inB), 0, 0, theUnit)->mComplexUnit);
    case EE_UNITS_UCG_CARRIER_AIRCRAFT: EECreateCarrierAircraft(theOther, theUnit->mPlayer->mPlayerID); return 0;
    case EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCG_SET_FLIGHT_TIME_OF: theOther->SetFlightTime((unsigned long)inB); return 0;
    case EE_UNITS_UCG_WAYPOINT_COUNT: return theUnit->mWaypoints.size();
    case EE_UNITS_UCG_NEXT_WAYPOINT: theUnit->GetNextWaypointPoint(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCG_WORLD_LINE: memcpy(EEUcgObject<GE3DLine>(inA), &::gWorld.mLine, sizeof(GE3DLine)); return 0;
    case EE_UNITS_UCG_MESH_Z: f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_UNITS_UCG_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCG_TERRAIN_Z: f[0] = ::gWorld.mMap->mTerrainMesh.GetZ((unsigned long)inB, (unsigned long)inC); return 0;
    case EE_UNITS_UCG_RANDOM_GENERATE: f[0] = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_UCG_RANDOM_RANGE: f[0] = gPhysics->GetRandom(f[0], f[1]); return 0;
    case EE_UNITS_UCG_WHEEL_UPDATE: theUnit->mWheels[inB]->Update(theUnit); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
