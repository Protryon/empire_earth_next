// C:\EEX\Empire Earth\AI Thread\EEAIRectangleArea.cpp (file name inferred: no __FILE__; RTTI class EEAIRectangleArea,
// vtable 0x43a540). A rectangular AI map area: 0x2c79f..0x2cc87 (EEAIPathSubSector.cpp ends at 0x2c79f;
// EEArtificialIntelligence.cpp starts at 0x2cc87). ctor, the continent count, the two random-point slots, then the
// map/random_shuffle COMDATs and ??_G.
#include <windows.h>
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
#include "ee/EEPhysics.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEAIRectangleArea.h"
#include "ee/EEAIRandom.h"
#include "ee/EETown.h"
#include "ee/EEHerd.h"

// Creates an empty rectangle area with no scenario area, tile counts or continent
// NEAR: the original calls the map's _Tree(pred, alloc) ctor out of line (ours expands it and refuses _Tree_val one
// level deeper) and keeps the inner loop counter in edi; EE_LOST_DEBUG 1..16 does not close it
// Recounts the rectangle's land, water and space tiles and stores and returns the continent with the most
// of its tiles (the lowest id on a tie; 0 when none)
unsigned short EEAIRectangleArea::UpdateContinents()
{
    return ee_astar_cm_rect_update_continents(&gContinentManager.mRecord, (uint64_t)(uintptr_t)this);   // Rust: cm_build.rs
}

// Returns a random area tile; when it is off the map, a random on-map tile of the area instead (unchanged when
// there is none) (Rust: cm_world.rs)
U2DPoint<long> EEAIRectangleArea::GetRandomPoint()
{
    return EEAIArea::PointOf(ee_astar_rect_random_point((uint64_t)(uintptr_t)this));
}

// EEAIArea's two slots that read the game's objects (the rest are inline over ee_sim::astar area_ops.rs; EEAIArea.cpp
// went in the game's leaves round 2)

// The area's slots the kinds had their own of (the class has no vtable: the kind is its area type)
void EEAIArea::Create(const std::vector<U2DPoint<long> >& inTiles)
{
    if (this->area_type == kAIAreaHerd)
        static_cast<EEHerd*>(this)->Create(inTiles);
    else
        ee_astar_area_create_of((uint64_t)(uintptr_t)this, reinterpret_cast<const EeHullPoint*>(inTiles.data()), inTiles.size());
}

U2DPoint<long> EEAIArea::GetRandomPoint()
{
    switch (this->area_type) {
    case kAIAreaTown: return static_cast<EETown*>(this)->GetRandomPoint();
    case kAIAreaHerd: return static_cast<EEHerd*>(this)->GetRandomPoint();
    case kAIAreaRectangleArea: return static_cast<EEAIRectangleArea*>(this)->GetRandomPoint();
    }
    return this->AreaRandomPoint();
}

// Returns a random tile of the area from the game's random stream, or (0, 0) when it is empty
U2DPoint<long> EEAIArea::AreaRandomPoint()
{
    return EEAIArea::PointOf(ee_astar_area_random_point((uint64_t)(uintptr_t)this));
}

// Sets the area's bounds in influence-table regions from its hull's bounding box (no table: unchanged)
void EEAIArea::UpdateBounds(EEInfluenceTable* inTable)
{
    ee_astar_area_update_bounds(this, inTable != NULL ? inTable->mRegionSize : 0);
}
