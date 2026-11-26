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
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEMap.h"
#include "ee/EEAIArea.h"

#include "ee/EEAIRectangleArea.h"
#include "ee/EETown.h"
#include "ee/EEHerd.h"

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
