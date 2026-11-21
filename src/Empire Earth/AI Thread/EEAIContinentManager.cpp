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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEAIForest.h"
#include "ee/EEPathManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMArea.h"
#include "ee/EEDebugTrace.h"
#include "rust/sim/units.h"

// The continent manager's passes, queries and objects are Rust (ee_sim::astar cm_passes.rs, cm_queries.rs,
// objects.rs; the inline members of ee/EEAIContinentManager.h); here: the members that read the units' classes or the
// scenario manager, and the forests' two members that make units.

// Resets the global continent manager and frees its tile grids (Rust: cm_passes.rs)
void EEAIContinentManager::Shutdown()
{
    ee_astar_cm_shutdown(&gContinentManager.mRecord);
}

// Creates an empty continent manager: no grids, every id and counter at 0 (the record: ee_sim::astar cm.rs)
void EEAIRegisterContinentState();   // EEAIContinentSave.cpp: the objects' layout and the host (cm_io.rs)

EEAIContinentManager::EEAIContinentManager()
{
    ee_astar_cm_init(&this->mRecord);
    EEAIRegisterContinentState();
}

// Destroys the continent manager by shutting down the global one (Shutdown), then frees its tables
EEAIContinentManager::~EEAIContinentManager()
{
    EEAIContinentManager::Shutdown();
    ee_astar_cm_free(&this->mRecord);
}

// Deletes every area, path sector and sub sector, empties the tables and zeroes the counters (not the grids)
void EEAIContinentManager::Reset()
{
    ee_astar_cm_reset_all(&this->mRecord);
}


// The influence table's region size (the areas' bounds), 0 without the table
static long EEAIRegionSize()
{
    return gWorld.mInfluenceTable != NULL ? gWorld.mInfluenceTable->mRegionSize : 0;
}

// Marks the manager as clearing, resets it, frees its grids and rebuilds the continents and hills of inMap (Rust:
// cm_passes.rs)
void EEAIContinentManager::Clear(EEMap* inMap)
{
    ee_astar_cm_clear(&this->mRecord, (uint64_t)(uintptr_t)inMap, EEAIRegionSize());
}

// Builds the AI regions of inMap after Clear: tile values, path areas and sectors, choke points, the region ids, the
// scenario manager's areas and the six chain kinds; frees the work grids (continent ids kept in large games) (Rust:
// cm_passes.rs)
void EEAIContinentManager::BuildRegions(EEMap* inMap)
{
    std::vector<EeScenarioArea> theInputs;
    std::vector<long> theIDs;
    EEScenarioManager* theManager = gWorld.GetScenarioManager();
    if (theManager) {
        theInputs.resize(theManager->mAreas.size());
        theIDs.resize(theManager->mAreas.size());
        for (long i = theManager->mAreas.size() - 1; i >= 0; i--) {
            EESMArea* theSource = theManager->mAreas[i];
            theInputs[i].set = theSource && !theSource->IsUnset();
            theIDs[i] = theSource ? theSource->mID : 0;
            if (!theInputs[i].set)
                continue;
            theInputs[i].top = theSource->GetRect().GetTop();
            theInputs[i].bottom = theSource->GetRect().GetBottom();
            theInputs[i].left = theSource->GetRect().GetLeft();
            theInputs[i].right = theSource->GetRect().GetRight();
            theInputs[i].height = theSource->GetRect().GetHeight();
            theInputs[i].width = theSource->GetRect().GetWidth();
        }
    }
    bool theKeep = !(inMap->mL195610 < 0x4b && gWorld.mNumPlayers < 5 && gWorld.mPopulationLimit < 1000);
    ee_astar_cm_build_regions(&this->mRecord, (uint64_t)(uintptr_t)inMap, EEAIRegionSize(), theInputs.empty() ? NULL : &theInputs[0],
                              theIDs.empty() ? NULL : &theIDs[0], theInputs.size(), theKeep);
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

// 0x563bc8 (.bss) = gUnitClassTable[0x147] (0x563bc8 - 0x5636ac = 0x51c): the unit class RegrowTrees creates
// (its vtable +0x30 is EEUnitClass::CreateUnit(x, y, player, bool, creator)); no store to it is known in the exe

// When inTile is in the forest, creates inCount units of type 0x147 at random forest tiles within inRadius of
// inTile (11 tries each, else inTile) flagged as regrowing; returns whether inTile is in the forest
// (EEUnit 0x22a3fd calls it)
bool EEAIForest::RegrowTrees(const U2DPoint<long>& inTile, long inRadius, long inCount)
{
    U2DRectangle<long> theArea;
    U2DPoint<long> thePoint;
    bool theResult = this->Contains(inTile);
    if (theResult) {
        theArea.Set(inTile);
        theArea.Grow(inRadius, true);
        for (long i = inCount - 1; i >= 0; i--) {
            bool theFound = false;
            for (long theTries = 10; theTries >= 0 && !theFound; theTries--) {
                long theY = gPhysics->GetRandom(theArea.mTop, theArea.mBottom);
                thePoint.mX = gPhysics->GetRandom(theArea.mLeft, theArea.mRight);
                thePoint.mY = theY;
                theFound = this->Contains(thePoint);
            }
            if (!theFound)
                thePoint = inTile;
            EEComplexUnit* theUnit = gUnitClassTable[kUnitTypeResEagle]->CreateUnit(thePoint.mX, thePoint.mY, 0, false, NULL)->mComplexUnit;
            if (theUnit->mIsRegrowing != true)
                theUnit->mIsRegrowing = true;
        }
    }
    return theResult;
}


