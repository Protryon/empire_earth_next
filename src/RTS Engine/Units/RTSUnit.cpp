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

#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSDummyHeader.h"

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
    delete this->mTileNode;
    this->mTileNode = NULL;
    this->mIsNodeFreed = true;
}

// FUNCTION: EXE 0x63d9a8
// Appends the unit's id and tile to a debug description
void RTSUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "uid %d, at %d,%d:", this->mUnitID & 0xffffff, this->mTileX, this->mTileY);
    ioString += theString;
}

// FUNCTION: EXE 0x63d9f6
// Appends the unit's id and tile to a multi-line debug description
void RTSUnit::DescribeLong(UWideString& ioString)
{
    UWideString theString;
    sprintf(theString, "UnitID:%d\nTile:(%d,%d)\n", this->mUnitID & 0xffffff, this->mTileX, this->mTileY);
    ioString += theString;
}

// FUNCTION: EXE 0x63da44
// Returns a hash of the unit's id and tile
unsigned long RTSUnit::HashValue()
{
    return ((this->mUnitID << 4) + this->mTileY << 4) + this->mTileX;
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
    long theResult = inFile.Read(this->mTileX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTileY);
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
        theResult = inFile.Read(this->mIsValid);
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
