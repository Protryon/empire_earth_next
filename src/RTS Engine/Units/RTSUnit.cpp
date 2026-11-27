// C:\EEX\RTS Engine\Units\RTSUnit.cpp (__FILE__ 0x4ccf20). The engine-side unit (RTTI RTSUnit > UReferenceCounter,
// vtable 0x449b10). LLE round 8: the lists' bodies are Rust (rust/crates/ee_game/src/rts/units.rs: Reset, Initialize,
// Invalidate, FreeNode, the owner's list, the tile chain and its checks, UpdateTile, Clear); the members forward, and
// the shims below are the C++ calls those bodies make (one statement each). The bodies' history: git history before
// LLE round 8.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSDummyHeader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rust/game/rts.h"
#include "rust/sim/units.h"

// New (R5a.4): the unit registry behind EEUnitHandle (ee/EEUnitHandle.h) is Rust (ee_units): a lock-free radix
// tree over the id's bits, so any thread may resolve a handle while another thread inserts a unit or runs its
// last Dereference. Looked up only, never iterated.
RTSUnit* RTSResolveUnitID(uint32_t inUnitID) { return static_cast<RTSUnit*>(ee_rts_unit_resolve(inUnitID)); }


// A handle of a unit without an id resolves to nothing: reported once (such a holder must keep its pointer). The
// typed halves are ee/EEUnitHandle.h's casts.
uint32_t EEUnitHandleID(const RTSUnit* inUnit) { return ee_rts_unit_handle_id(inUnit); }


// Destroys the unit and its tile node (asserts it was invalidated)
RTSUnit::~RTSUnit()
{
    this->mObjectKind.mValue = 0;   // New (port): an RTSUnit again (ee/EEUnitKind.h)
    ESVERIFY_AT(this->mIsValid == false, 62);
    ee_units_store_remove_object((uint64_t)(uintptr_t)this);   // New (R5b.4, round 26): the store reads this object's records; none outlives it
    delete this->mTileNode;
}

// The shims units.rs calls (UnitsCpp: the unit's side)
extern "C" {
EeUnitsRtsUnitFields* ee_rtsucpp_unit_fields(RTSUnit* inUnit) { return inUnit; }
RTSUnitLink** ee_rtsucpp_node_slot(RTSUnit* inUnit) { return &inUnit->mTileNode; }
RTSUnit** ee_rtsucpp_next_slot(RTSUnit* inUnit) { return &inUnit->mNext; }
RTSUnit** ee_rtsucpp_previous_slot(RTSUnit* inUnit) { return &inUnit->mPrevious; }
RTSTile** ee_rtsucpp_tile_slot(RTSUnit* inUnit) { return &inUnit->mTile; }
EEPlayer** ee_rtsucpp_player_slot(RTSUnit* inUnit) { return &inUnit->mPlayer; }
RTSDummyUnit* ee_rtsucpp_dummy_head(RTSUnit* inLink) { return RTSDummyHead(inLink); }
RTSUnit** ee_rtsucpp_head_first_slot(RTSDummyUnit* inHead) { return &inHead->mFirst; }
RTSUnitLink* ee_rtsucpp_link_new() { return new RTSUnitLink; }
void ee_rtsucpp_link_delete(RTSUnitLink* inLink) { delete inLink; }
void ee_rtsucpp_store_linked(RTSUnit* inUnit) { ee_units_store_linked((uint64_t)(uintptr_t)inUnit); }
void ee_rtsucpp_store_remove(RTSUnit* inUnit) { ee_units_store_remove_object((uint64_t)(uintptr_t)inUnit); }
void ee_rtsucpp_unregister(RTSUnit* inUnit) { ee_rts_unit_registry(inUnit, inUnit, false); }
void ee_rtsucpp_register(RTSUnit* inUnit) { ee_rts_unit_registry(inUnit, inUnit, true); }
RTSTile* ee_rtsucpp_map_tile(EEMap* inMap, long inX, long inY) { return inMap->GetTile(inX, inY); }
void ee_rtsucpp_sight_on(RTSUnit* inUnit) { inUnit->SightOnHere(); }
void ee_rtsucpp_sight_off(RTSUnit* inUnit) { inUnit->SightOffHere(); }
void ee_rtsucpp_unit_invalidate(RTSUnit* inUnit) { inUnit->Invalidate(); }
void ee_rtsucpp_unit_repair_pointers(RTSUnit* inUnit, unsigned long inFirst, unsigned long inSecond) { inUnit->RepairPointers(inFirst, inSecond); }
}
