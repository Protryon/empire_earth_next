// C:\EEX\RTS Engine\Units\RTSUnit.cpp (__FILE__ 0x4ccf20). The engine-side unit (RTTI RTSUnit > UReferenceCounter,
// vtable 0x449b10). LLE round 8: the lists' bodies are Rust (rust/crates/ee_game/src/rts/units.rs: Reset, Initialize,
// Invalidate, FreeNode, the owner's list, the tile chain and its checks, UpdateTile, Clear); the members forward, and
// the shims below are the C++ calls those bodies make (one statement each). The bodies' history: git history before
// LLE round 8.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSDummyHeader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rust/sim/units.h"   // the unit registry (rust/crates/ee_sim/src/units)
#include "rust/game/rts.h"

// New (R5a.4): the unit registry behind EEUnitHandle (ee/EEUnitHandle.h) is Rust (ee_units): a lock-free radix
// tree over the id's bits, so any thread may resolve a handle while another thread inserts a unit or runs its
// last Dereference. Looked up only, never iterated.
RTSUnit* RTSResolveUnitID(uint32_t inUnitID)
{
    if (inUnitID == 0xffffffff)
        return NULL;
    return static_cast<RTSUnit*>(ee_units_resolve(inUnitID));
}

// A unit's id is final when its player inserts it. A second object with the same id (none in a game: ids count up)
// replaces the first in the registry; the first one's FreeNode then leaves the entry alone.
void RTSRegisterUnit(RTSUnit* inUnit)
{
    uint32_t theID = static_cast<uint32_t>(inUnit->mUnitID);
    if (theID != 0xffffffff)
        ee_units_register(theID, inUnit);
}

void RTSUnregisterUnit(RTSUnit* inUnit)
{
    uint32_t theID = static_cast<uint32_t>(inUnit->mUnitID);
    if (theID != 0xffffffff)
        ee_units_unregister(theID, inUnit);   // only if the entry is still this unit
}

// A handle of a unit without an id resolves to nothing: reported once (such a holder must keep its pointer). The
// EEUnit and EEComplexUnit overloads are in EEUnit.cpp (they need the complete classes).
uint32_t EEUnitHandleID(const RTSUnit* inUnit)
{
    uint32_t theID = static_cast<uint32_t>(inUnit->mUnitID);
    static bool sReported = false;
    if (theID == 0xffffffff && !sReported) {
        sReported = true;
        fprintf(stderr, "EE-HANDLE: a handle to a unit without an id (%p): it resolves to nothing\n",
                (const void*)inUnit);
    }
    return theID;
}

void EEUnitHandleResolve(uint32_t inUnitID, RTSUnit*& outUnit) { outUnit = RTSResolveUnitID(inUnitID); }

// New: the unit chain check (ee/RTSDummyHeader.h)
static bool RTSCheckUnitListFromEnvironment()
{
    const char* theValue = getenv("EE_CHECK_UNIT_LIST");
    bool theOn = theValue != NULL && theValue[0] != '\0' && strcmp(theValue, "0") != 0;
    if (theOn)
        fprintf(stderr, "EE-UNITLIST: checking the unit chains (EE_CHECK_UNIT_LIST)\n");
    return theOn;
}
bool gRTSCheckUnitList = RTSCheckUnitListFromEnvironment();

static void RTSUnitListFailed(const char* inWhere, const char* inWhat, const RTSUnit* inUnit, const RTSUnit* inOther)
{
    fprintf(stderr, "EE-UNITLIST FAILED in %s: %s; unit %p id %08lx x %ld y %ld valid %d node %p prev %p next %p",
            inWhere, inWhat, (const void*)inUnit, inUnit ? inUnit->mUnitID : 0UL, inUnit ? inUnit->mTileX : 0L,
            inUnit ? inUnit->mTileY : 0L, inUnit ? (int)inUnit->mIsValid : -1, inUnit ? (void*)inUnit->mTileNode : NULL,
            inUnit && inUnit->mTileNode ? (void*)inUnit->mTileNode->mPrev : NULL,
            inUnit && inUnit->mTileNode ? (void*)inUnit->mTileNode->mNext : NULL);
    if (inOther != NULL && RTSDummyHead(const_cast<RTSUnit*>(inOther)) == NULL)
        fprintf(stderr, "; other %p id %08lx x %ld valid %d prev %p next %p", (const void*)inOther, inOther->mUnitID,
                inOther->mTileX, (int)inOther->mIsValid, inOther->mTileNode ? (void*)inOther->mTileNode->mPrev : NULL,
                inOther->mTileNode ? (void*)inOther->mTileNode->mNext : NULL);
    else if (inOther != NULL)
        fprintf(stderr, "; other: the chain's head %p", (const void*)inOther);
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

void RTSCheckUnitList(const RTSDummyUnit* inHead, const char* inWhere) { ee_rts_unit_check_list(inHead, inWhere); }

RTSUnit::RTSUnit() { ee_rts_unit_construct(this); }

// Destroys the unit and its tile node (asserts it was invalidated)
RTSUnit::~RTSUnit()
{
    this->mObjectKind.mValue = 0;   // New (port): an RTSUnit again (ee/EEUnitKind.h)
    ESVERIFY_AT(this->mIsValid == false, 62);
    EEUnitStoreRemove(this);   // New (R5b.4, round 26): the store reads this object's records; none outlives it
    delete this->mTileNode;
}


// Appends the unit's id and tile to a debug description
void RTSUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "uid %ld, at %ld,%ld:", this->mUnitID & 0xffffff, this->mTileX, this->mTileY);
    ioString += theString;
}

// Appends the unit's id and tile to a multi-line debug description
void RTSUnit::DescribeLong(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "UnitID:%ld\nTile:(%ld,%ld)\n", this->mUnitID & 0xffffff, this->mTileX, this->mTileY);
    ioString += theString;
}



// Reads the unit's tile, id and flags (the editor selection is cleared); returns the first read error. port (R5b.4,
// round 26): the body is Rust (ee_units unit_load.rs rts_load)
long RTSUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEUnitSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion, 0, 0};
    return ee_units_unit_load(&theContext, &EEUnitSaveOperation, this, NULL, NULL, &theIn);
}


// Writes the unit's tile, id and flags (editor selection as false; versions up to 0.6 write id and tile
// only); returns the first write error. port (R5b.4, round 26): the body is Rust (ee_units unit_save.rs rts_save)
long RTSUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEUnitSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion, 0, 0};
    return ee_units_unit_save(&theContext, &EEUnitSaveOperation, this, NULL, NULL, &theIn);
}

void RTSUnit::CopyFrom(class RTSUnit *)
{
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
void ee_rtsucpp_store_linked(RTSUnit* inUnit) { EEUnitStoreLinked(inUnit); }
void ee_rtsucpp_store_remove(RTSUnit* inUnit) { EEUnitStoreRemove(inUnit); }
void ee_rtsucpp_unregister(RTSUnit* inUnit) { RTSUnregisterUnit(inUnit); }
void ee_rtsucpp_register(RTSUnit* inUnit) { RTSRegisterUnit(inUnit); }
bool ee_rtsucpp_check_on() { return gRTSCheckUnitList; }
void ee_rtsucpp_list_failed(const char* inWhere, const char* inWhat, const RTSUnit* inUnit, const RTSUnit* inOther) { RTSUnitListFailed(inWhere, inWhat, inUnit, inOther); }
RTSTile* ee_rtsucpp_map_tile(EEMap* inMap, long inX, long inY) { return inMap->GetTile(inX, inY); }
void ee_rtsucpp_sight_on(RTSUnit* inUnit) { inUnit->SightOnHere(); }
void ee_rtsucpp_sight_off(RTSUnit* inUnit) { inUnit->SightOffHere(); }
void ee_rtsucpp_unit_invalidate(RTSUnit* inUnit) { inUnit->Invalidate(); }
void ee_rtsucpp_unit_repair_pointers(RTSUnit* inUnit, unsigned long inFirst, unsigned long inSecond) { inUnit->RepairPointers(inFirst, inSecond); }
}
