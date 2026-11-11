// C:\EEX\RTS Engine\Units\RTSUnit.cpp (__FILE__ 0x4ccf20). The engine-side unit (RTTI RTSUnit > UReferenceCounter,
// vtable 0x449b10): 0x23d7c0..0x23dd54 (its four key chains to Virtual20). Unit flags: the exe's module set.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSDummyHeader.h"
#include "ee/EEUnitHandle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rust/ee_rust.h"   // the unit registry (rust/crates/ee_units)

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
                inOther->mTileX.Get(), (int)inOther->mIsValid, inOther->mTileNode ? (void*)inOther->mTileNode->mPrev : NULL,
                inOther->mTileNode ? (void*)inOther->mTileNode->mNext : NULL);
    else if (inOther != NULL)
        fprintf(stderr, "; other: the chain's head %p", (const void*)inOther);
    fprintf(stderr, "\n");
    fflush(stderr);
    abort();
}

// Whether a unit (or a chain head) is in a chain: a head always is; a unit has a previous link (a unit or the head)
static bool RTSIsInChain(const RTSUnit* inUnit)
{
    return RTSDummyHead(const_cast<RTSUnit*>(inUnit)) != NULL
        || (inUnit->mTileNode != NULL && inUnit->mTileNode->mPrev != NULL);
}

// The unit's neighbours point back at it (a unit in no chain passes)
static void RTSCheckNeighbours(const RTSUnit* inUnit, const char* inWhere)
{
    RTSUnit* thePrev = inUnit->mTileNode->mPrev;
    RTSUnit* theNext = inUnit->mTileNode->mNext;
    if (thePrev == NULL) {
        if (theNext != NULL)
            RTSUnitListFailed(inWhere, "a next link without a previous one", inUnit, theNext);
        return;
    }
    if (RTSDummyHead(thePrev) != NULL) {
        if (RTSDummyHead(thePrev)->mFirst != inUnit)
            RTSUnitListFailed(inWhere, "the head's first unit is another", inUnit, thePrev);
    } else if (thePrev->mTileNode == NULL || thePrev->mTileNode->mNext != inUnit)
        RTSUnitListFailed(inWhere, "the previous unit's next is another", inUnit, thePrev);
    else if (!RTSIsInChain(thePrev))
        RTSUnitListFailed(inWhere, "the previous unit is in no chain", inUnit, thePrev);
    if (theNext != NULL && (theNext->mTileNode == NULL || theNext->mTileNode->mPrev != inUnit))
        RTSUnitListFailed(inWhere, "the next unit's previous is another", inUnit, theNext);
}

void RTSCheckUnitList(const RTSDummyUnit* inHead, const char* inWhere)
{
    if (!gRTSCheckUnitList || inHead == NULL)
        return;
    const RTSUnit* thePrev = reinterpret_cast<const RTSUnit*>(inHead);
    unsigned long theCount = 0;
    for (const RTSUnit* theUnit = inHead->mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        if (theUnit->mTileNode == NULL)
            RTSUnitListFailed(inWhere, "a unit in the chain has no node", theUnit, thePrev);
        if (theUnit->mTileNode->mPrev != thePrev)
            RTSUnitListFailed(inWhere, "a unit's previous is not the unit before it", theUnit, thePrev);
        if (!theUnit->mIsValid)
            RTSUnitListFailed(inWhere, "an invalid (removed) unit is in the chain", theUnit, thePrev);
        if (++theCount > (1UL << 22))
            RTSUnitListFailed(inWhere, "the chain loops (over 4M units)", theUnit, thePrev);
        thePrev = theUnit;
    }
}

const UString RTSUnit::sUnitID("RTSUnit-UnitID");
const UString RTSUnit::sTileX("RTSUnit-TileX");
const UString RTSUnit::sTileY("RTSUnit-TileY");
const UString RTSUnit::sSpecificData("RTSUnit-Specific-Data");

// FUNCTION: EXE 0x63d8cc
// Creates an invalid unit with no id, tile or tile node
RTSUnit::RTSUnit()
{
    this->Reset();
    this->mIsValid = false;
    this->mIsNodeFreed = true;
}

// SYNTHETIC: EXE 0x63d8ed
// (the scalar deleting destructor)

// FUNCTION: EXE 0x63d909
// Destroys the unit and its tile node (asserts it was invalidated)
RTSUnit::~RTSUnit()
{
    this->mObjectKind.mValue = 0;   // New (port): an RTSUnit again (ee/EEUnitKind.h)
    ESVERIFY_AT(this->mIsValid == false, 62);
    delete this->mTileNode;
}

// FUNCTION: EXE 0x63d93f
// Takes the unit out of its player's unit list and its tile's chain and marks it invalid
void RTSUnit::Invalidate()
{
    ESVERIFY_AT(this->mIsValid == true, 80);
    this->RemoveFromList();
    if (this->mTileNode != NULL) {
        this->Unlink();
    }
    this->mIsValid = false;
}

// FUNCTION: EXE 0x63d977
// Frees the invalid unit's tile node
void RTSUnit::FreeNode()
{
    ESVERIFY_AT(this->mIsValid == false, 119);
    RTSUnregisterUnit(this);   // New (R5a.4): its handles resolve to nothing from here (ee/EEUnitHandle.h)
    EEUnitStoreRemove(this);   // New (R5b.4): a no-op unless a teardown dropped the list's reference (ee/EEUnitStore.h)
    delete this->mTileNode;
    this->mTileNode = NULL;
    this->mIsNodeFreed = true;
}

// FUNCTION: EXE 0x63d9a8
// Appends the unit's id and tile to a debug description
void RTSUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "uid %ld, at %ld,%ld:", this->mUnitID & 0xffffff, this->mTileX.Get(), this->mTileY.Get());
    ioString += theString;
}

// FUNCTION: EXE 0x63d9f6
// Appends the unit's id and tile to a multi-line debug description
void RTSUnit::DescribeLong(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "UnitID:%ld\nTile:(%ld,%ld)\n", this->mUnitID & 0xffffff, this->mTileX.Get(), this->mTileY.Get());
    ioString += theString;
}

// FUNCTION: EXE 0x63da44
// Returns a hash of the unit's id and tile
unsigned long RTSUnit::HashValue()
{
    return static_cast<uint32_t>(((this->mUnitID << 4) + this->mTileY << 4) + this->mTileX);   // a 32-bit hash on every data model
}

// FUNCTION: EXE 0x63da54
// Resets the unit as valid, creating a tile node when it is to be linked into tiles
void RTSUnit::Initialize(long inLinked)
{
    this->Reset();
    this->mIsValid = true;
    this->mIsNodeFreed = false;
    if (inLinked == 0) {
        this->mTileNode = NULL;
    } else {
        this->mTileNode = new RTSUnitLink;
    }
}

// FUNCTION: EXE 0x63da8e
// Clears the unit's id, tile, list links, player and flags (off the map)
void RTSUnit::Reset()
{
    this->mUnitID = 0xffffffff;
    this->mTileNode = NULL;
    this->mTile = NULL;
    this->mTileX = -10000;
    this->mTileY = -10000;
    this->mNext = NULL;
    this->mPrevious = NULL;
    this->mPlayer = NULL;
    this->mIsQueuedForDelete = false;
    this->mIsEditorSelected = false;
    this->mIsValid = false;
    this->mIsNodeFreed = false;
}

// FUNCTION: EXE 0x63dabb
// Moves the unit in its player's unit list to after another unit (to the head with NULL)
void RTSUnit::InsertAfter(RTSUnit* inUnit)
{
    this->RemoveFromList();
    if (inUnit != NULL) {
        this->mNext = inUnit->mNext;
        inUnit->mNext = this;
        if (this->mNext != NULL) {
            this->mNext->mPrevious = this;
        }
    }
    this->mPrevious = inUnit;
}

// FUNCTION: EXE 0x63dae5
// Moves the unit in a tile's unit chain to after another unit or to the front after the chain's dummy head
void RTSUnit::Link(RTSUnit* inUnit)
{
    this->Unlink();
    if (gRTSCheckUnitList && !this->mIsValid)
        RTSUnitListFailed("RTSUnit::Link", "linking a removed (invalid) unit", this, inUnit);
    if (gRTSCheckUnitList && !RTSIsInChain(inUnit))
        RTSUnitListFailed("RTSUnit::Link", "linking after a unit that is in no chain", this, inUnit);
    RTSDummyUnit* theHead = RTSDummyHead(inUnit);
    if (theHead != NULL) {
        this->mTileNode->mNext = theHead->mFirst;
        this->mTileNode->mPrev = inUnit;
        theHead->mFirst = this;
    } else {
        this->mTileNode->mNext = inUnit->mTileNode->mNext;
        this->mTileNode->mPrev = inUnit;
        inUnit->mTileNode->mNext = this;
    }
    if (this->mTileNode->mNext != NULL) {
        this->mTileNode->mNext->mTileNode->mPrev = this;
    }
    if (gRTSCheckUnitList)
        RTSCheckNeighbours(this, "RTSUnit::Link (after)");
    EEUnitStoreLinked(this);   // New (R5b.4): the store's on-the-map flag (ee/EEUnitStore.h)
}

// FUNCTION: EXE 0x63db49
// Drops the last reference to the unit
void RTSUnit::LastDereference()
{
    UReferenceCounter::Dereference();
}

// FUNCTION: EXE 0x63db4f
// Reads the unit's tile, id and flags (the editor selection is cleared); returns the first read error
long RTSUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitStoreRead(inFile, this->mTileX);   // the unit store's holders (ee/EEUnitStore.h)
    if (theResult != kErrorOK)
        return theResult;
    theResult = EEUnitStoreRead(inFile, this->mTileY);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mIsQueuedForDelete);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mIsEditorSelected);
    this->mIsEditorSelected = false;
    if (theResult == kErrorOK) {
        theResult = EEUnitStoreRead(inFile, this->mIsValid);
    }
    return theResult;
}

// FUNCTION: EXE 0x63dbb7
// Unlinks the unit from its player's unit list
void RTSUnit::RemoveFromList()
{
    if (this->mNext != NULL) {
        this->mNext->mPrevious = this->mPrevious;
    }
    if (this->mPrevious != NULL) {
        this->mPrevious->mNext = this->mNext;
    }
    this->mNext = NULL;
    this->mPrevious = NULL;
}

// FUNCTION: EXE 0x63dbda
// Unlinks the unit from its tile's unit chain
void RTSUnit::Unlink()
{
    if (gRTSCheckUnitList)
        RTSCheckNeighbours(this, "RTSUnit::Unlink (before)");
    if (this->mTileNode->mNext != NULL) {
        this->mTileNode->mNext->mTileNode->mPrev = this->mTileNode->mPrev;
    }
    if (this->mTileNode->mPrev != NULL) {
        RTSDummyUnit* theHead = RTSDummyHead(this->mTileNode->mPrev);
        if (theHead != NULL) {
            theHead->mFirst = this->mTileNode->mNext;
        } else {
            this->mTileNode->mPrev->mTileNode->mNext = this->mTileNode->mNext;
        }
    }
    this->mTileNode->mNext = NULL;
    this->mTileNode->mPrev = NULL;
    EEUnitStoreLinked(this);   // New (R5b.4): the store's on-the-map flag (ee/EEUnitStore.h)
}

// FUNCTION: EXE 0x63dc3a
// Writes the unit's tile, id and flags (editor selection as false; versions up to 0.6 write id and tile
// only); returns the first write error
long RTSUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult;
    bool theIsEditorSelected;
    if (inMajorVersion == 0 && inMinorVersion <= 6) {
        theResult = outFile.Write(this->mUnitID);
        if (theResult == kErrorOK) {
            theResult = outFile.Write(this->mTileX);
            if (theResult == kErrorOK) {
                theResult = outFile.Write(this->mTileY);
            }
        }
    } else {
        theResult = outFile.Write(this->mTileX);
        if (theResult == kErrorOK) {
            theResult = outFile.Write(this->mTileY);
            if (theResult == kErrorOK) {
                theResult = outFile.Write(this->mUnitID);
                if (theResult == kErrorOK) {
                    theResult = outFile.Write(this->mIsQueuedForDelete);
                    if (theResult == kErrorOK) {
                        theIsEditorSelected = false;
                        theResult = outFile.Write(theIsEditorSelected);
                        if (theResult == kErrorOK) {
                            theResult = outFile.Write(this->mIsValid);
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x63dcf3
// Looks up the unit's tile on the map, marking the unit off the map when there is no tile
void RTSUnit::UpdateTile(EEMap* inMap)
{
    if (this->mTileX == -10000 || this->mTileY == -10000) {
        this->mTile = NULL;
    } else {
        RTSTile* theTile = inMap->GetTile(this->mTileX, this->mTileY);
        if (theTile == NULL) {
            this->mTileX = -10000;
            this->mTileY = -10000;
        }
        this->mTile = theTile;
    }
}

// FUNCTION: EXE 0x63dd46
// Resets the unit as invalid
void RTSUnit::Clear()
{
    this->Reset();
    this->mIsValid = false;
}

// SYNTHETIC: EXE 0x63d7c0
// SYNTHETIC: EXE 0x63d7ca
// SYNTHETIC: EXE 0x63d7db
// SYNTHETIC: EXE 0x63d7e7
// SYNTHETIC: EXE 0x63d803
// SYNTHETIC: EXE 0x63d80d
// SYNTHETIC: EXE 0x63d81e
// SYNTHETIC: EXE 0x63d82a
// SYNTHETIC: EXE 0x63d846
// SYNTHETIC: EXE 0x63d850
// SYNTHETIC: EXE 0x63d861
// SYNTHETIC: EXE 0x63d86d
// SYNTHETIC: EXE 0x63d889
// SYNTHETIC: EXE 0x63d893
// SYNTHETIC: EXE 0x63d8a4
// SYNTHETIC: EXE 0x63d8b0

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body of EECCAreaEffect::Reset)
void RTSUnit::CopyFrom(class RTSUnit *)
{
}

// ICF-FOLDED: EXE 0x67cf38
// (no address of its own: folded into the shared `xor eax, eax; ret` body of EEUIECSSEFormClose::ProcessEvent)
long RTSUnit::RepairPointers(unsigned long,unsigned long)
{
    return 0;
}
