// C:\EEX\Empire Earth\AI Thread\EEPathManager.cpp (file name inferred: no __FILE__; RTTI classes
// EEAutoPathRequest (vtable 0x43c430), EEPathManager (vtable 0x43c438), EEAIPathGoalModifierSpace/Sea
// (0x43c448/0x43c458), the three A* machines (ee/EEAIAStarMachine.h)). 0xa5c31..0xabbaf.
// The manager is Rust (ee_sim::astar pm.rs: the searches in progress and their nodes, the spare machines, the
// pending units, the counters, the request record, the requests' and the updates' decisions, the saved game's
// parts); this file is its host over the machines, units and requests, and keeps the bounded machine's searches.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <math.h>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <string.h>
#include <memory>
#include <functional>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESaveGameFiles.h"

#include "ee/EEPathManager.h"

static_assert(sizeof(GE3DPoint) == sizeof(EePathPoint) && offsetof(GE3DPoint, mZ) == offsetof(EePathPoint, z),
              "GE3DPoint");

// The requests (EEAutoPathRequest) are Rust records (ee_sim::astar request.rs), with the owners waiting for a
// loaded one and their saved record. The manager is Rust (ee_sim::astar pm.rs); the C++ answers its host's
// operations over the units and the map, one statement each.

// the loaded game's fixups (pm.rs, request.rs): after every owner's own, and at the first tick
extern "C" void ee_path_after_load_last(void (*inFixup)())
{
    EEAfterLoadLast(inFixup);
}

extern "C" void ee_path_after_load(void (*inFixup)())
{
    EEAfterLoad(inFixup);
}

// The Rust manager's host: one call per operation (pm.rs EE_PATH_*)
static int64_t EEPathManagerHost(void* inContext, int32_t inOp, EePathIo* io)
{
    switch (inOp) {
    case EE_PATH_WORLD_UNIT:
        return (int64_t)(uintptr_t)dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)io->i[0])));
    case EE_PATH_CLOSEST_TILE:   // ClampTile's (pm_apply.rs): map h1's tile closest to the GE3DPoint at i2, as longs at i0, i1
        reinterpret_cast<EEMap*>(io->h[1])->GetClosestTile(reinterpret_cast<GE3DPoint*>(&io->i[2]), reinterpret_cast<long*>(&io->i[0]), reinterpret_cast<long*>(&io->i[1]));
        return 0;
    case EE_PATH_UNIT_STRANDED:
        reinterpret_cast<EEComplexUnit*>(io->h[0])->mPlayer->UnitStranded(reinterpret_cast<EEComplexUnit*>(io->h[0]));
        return 0;
    }
    return 0;
}

// Creates the path manager with no bounded machine, nothing queued and region updates off
EEPathManager::EEPathManager() : mRust(ee_path_manager_new(EEPathManagerHost, this)), mLC8(5)
{
}

// Destroys the path manager (Clear, then the Rust manager freed)
EEPathManager::~EEPathManager() { this->mRust = ee_path_manager_destroy(this->mRust); }
