// C:\EEX\Empire Earth\AI Thread\EEAIContinentManager.cpp (file name inferred: no __FILE__; RTTI class
// EEAIContinentManager, vtable 0x43a210). The AI's continent map: 0x1ed2c..0x29fcc (EEAIContinent.cpp's COMDATs end at
// 0x1ed2c; the EEAIFO* function objects start at 0x29fcc).
#include <windows.h>
#include <string.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEDebugTrace.h"

// The continent manager's passes, queries and objects are Rust (ee_sim::astar cm_passes.rs, cm_queries.rs,
// objects.rs; the inline members of ee/EEAIContinentManager.h); here: the members that read the units' classes or the
// scenario manager, and the forests' two members that make units.

// (CNTQ, the queries' capture records: ee_sim::astar cm_capture.rs; the last queries here moved: cm_unit.rs)

// Creates an empty continent manager: no grids, every id and counter at 0 (the record: ee_sim::astar cm.rs)
void EEAIRegisterContinentState();   // EEAIContinentSave.cpp: the objects' layout and the host (cm_io.rs)

EEAIContinentManager::EEAIContinentManager()
{
    ee_astar_cm_init(&this->mRecord);
    EEAIRegisterContinentState();
}


// The influence table's region size (the areas' bounds), 0 without the table
static long EEAIRegionSize() { return gWorld.mInfluenceTable != NULL ? gWorld.mInfluenceTable->mRegionSize : 0; }

// Marks the manager as clearing, resets it, frees its grids and rebuilds the continents and hills of inMap (Rust:
// cm_passes.rs)
void EEAIContinentManager::Clear(EEMap* inMap) { ee_astar_cm_clear(&this->mRecord, (uint64_t)(uintptr_t)inMap, EEAIRegionSize()); }

// Builds the AI regions of inMap after Clear: tile values, path areas and sectors, choke points, the region ids, the
// scenario manager's areas (scenario::entry::region_inputs) and the six chain kinds; frees the work grids (continent
// ids kept in large games) (Rust: cm_passes.rs, cm_inline.rs)
void EEAIContinentManager::BuildRegions(EEMap* inMap)
{
    ee_astar_cm_build_regions_scenario(&this->mRecord, (uint64_t)(uintptr_t)inMap, EEAIRegionSize(), !(inMap->mL195610 < 0x4b && gWorld.mNumPlayers < 5 && gWorld.mPopulationLimit < 1000));
}

// CanGetTo's C++ (ee_sim::astar cm_unit.rs): 0 the map is a space map; 1 the aircraft's planet id, its land
// continent when it had none
extern "C" uint32_t ee_astar_cm_unit_host(int32_t inWhat, uint64_t inUnit)
{
    if (inWhat == 0)
        return gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inUnit;
    if (theUnit->mPlanetID == 0) {
        EEDebugTrace("EEAIContinentManager::GetCanPathToContinent: no planet ID for airplane %d of type %s\n",
                     theUnit->GetUnitID(), theUnit->GetUnitClass()->GetName());
        theUnit->mPlanetID = EEGetLandContinentOf(theUnit);
    }
    return theUnit->mPlanetID;
}
