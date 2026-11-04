// C:\EEX\RTS Engine\Player\RTSPlayer.cpp (file name inferred: no __FILE__; the six "RTSPlayer-*" key chains). The
// engine-side player (RTTI RTSPlayer, vtable 0x442b5c; class in ee/EEPlayer.h): 0x16ef03..0x16f892 (its key chains
// to the push_back COMDAT 0x16f866; EESEEvents.cpp's ShowTab 0x16f892 opens the next unit). Unit flags: the exe's
// module set.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/ESChecksum.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/NEMessageID.h"

const UString RTSPlayer::sSaveVisMap("RTSPlayer-SaveVisMap");
const UString RTSPlayer::sUniqueID("RTSPlayer-UniqueID");
const UString RTSPlayer::sPlayerNumber("RTSPlayer-PlayerNumber");
const UString RTSPlayer::sUnitCount("RTSPlayer-UnitCount");
const UString RTSPlayer::sHashBucketLength("RTSPlayer-HashBucketLength");
const UString RTSPlayer::sObserverMode("RTSPlayer-ObserverMode");

// FUNCTION: EXE 0x56f095
// Creates an empty player with no units
RTSPlayer::RTSPlayer()
{
    this->Reset();
}

// SYNTHETIC: EXE 0x56f112
// (the scalar deleting destructor, emitted with the vtable; slot RTSPlayer+0x0)

// FUNCTION: EXE 0x56f12e
// Creates a named player with the given id; every player but player 0 keeps a visibility map
RTSPlayer::RTSPlayer(const UWideString& inName, unsigned long inPlayerID)
{
    this->Reset();
    this->mName = inName;
    this->mPlayerID = inPlayerID;
    this->mSaveVisMap = inPlayerID != 0;
}

// FUNCTION: EXE 0x56f1cf
// Destroys the player
RTSPlayer::~RTSPlayer()
{
}

// FUNCTION: EXE 0x56f1f3
// Adds every pending unit to the player, optionally releasing the pending list afterwards
void RTSPlayer::AddPendingUnits(bool inKeepID, bool inRelease)
{
    for (unsigned long i = 0; i < this->mPendingUnits.size(); ++i)
        this->AddUnit(this->mPendingUnits[i], inKeepID);
    if (inRelease)
        this->ReleasePendingUnits();
}

// FUNCTION: EXE 0x56f233
// Adds a unit to the player; unless it keeps its id, gives it a new unit id (player id in the top byte) and
// puts its sight on its tile
void RTSPlayer::AddUnit(RTSUnit* inUnit, bool inKeepID)
{
    if (!inKeepID) {
        inUnit->mUnitID = this->mPlayerID * 0x1000000 + this->mNextUniqueID;
        this->mNextUniqueID = (this->mNextUniqueID + 1) & 0xffffff;
        if (inUnit->mTileNode != NULL)
            inUnit->SightOnHere();
    }
    this->InsertUnit(inUnit);
}

// FUNCTION: EXE 0x56f27f
// Takes every unit queued as killed out of the player's unit list and hash buckets, then invalidates and
// releases it
void RTSPlayer::RemoveKilledUnits()
{
    for (unsigned long i = 0; i < this->mKilledUnits.size(); i++) {
        RTSUnit* theUnit = this->mKilledUnits[i];
        unsigned long theBucket = theUnit->mUnitID & 0x7f;
        if (theUnit->mTileNode != NULL && this->mSaveVisMap)
            theUnit->SightOffHere();
        this->RemovingUnit(theUnit);
        if (this->mFirstUnit == theUnit)
            this->mFirstUnit = static_cast<EEUnit*>(theUnit->mNext);
        if (this->mLastUnit == theUnit)
            this->mLastUnit = static_cast<EEUnit*>(theUnit->mPrevious);
        this->mUnitBuckets[theBucket].mCount--;
        if (this->mUnitBuckets[theBucket].mCount != 0 && this->mUnitBuckets[theBucket].mFirst == theUnit)
            this->mUnitBuckets[theBucket].mFirst = static_cast<EEUnit*>(theUnit->mNext);
        else if (this->mUnitBuckets[theBucket].mCount == 0)
            this->mUnitBuckets[theBucket].mFirst = NULL;
        theUnit->RemoveFromList();
        this->UnitRemoved(theUnit);
        this->mUnitCount--;
        theUnit->Invalidate();
        theUnit->Dereference();
    }
    this->RemovalsDone();
    this->mKilledUnits.clear();
}

// FUNCTION: EXE 0x56f35e
// Releases and empties the pending units
void RTSPlayer::ReleasePendingUnits()
{
    for (unsigned long i = 0; i < this->mPendingUnits.size(); ++i)
        this->mPendingUnits[i]->Dereference();
    this->mPendingUnits.clear();
}

// FUNCTION: EXE 0x56f394
// Adds the player's name, id, unit count and next unit id to the sync checksum
void RTSPlayer::GetChecksum(ESChecksum& ioChecksum)
{
    unsigned long theChecksum = this->mPlayerID * 10 + this->mUnitCount + this->mNextUniqueID;
    this->mName.GetChecksum(ioChecksum);
    ioChecksum.AddChecksum("RTSPlayer-HashChecksum", 0);
    ioChecksum.AddChecksum("RTSPlayer", theChecksum);
}

// FUNCTION: EXE 0x56f40f
// Returns a pending unit by index, NULL past the end
RTSUnit* RTSPlayer::GetPendingUnit(unsigned long inIndex)
{
    RTSUnit* theResult = NULL;
    if (inIndex < this->mPendingUnits.size())
        theResult = this->mPendingUnits[inIndex];
    return theResult;
}

// FUNCTION: EXE 0x56f433
// Finds one of the player's units by unit id in its hash bucket; NULL if none
RTSUnit* RTSPlayer::GetUnit(unsigned long inUnitID)
{
    unsigned long theBucket = inUnitID & 0x7f;
    unsigned long theCount = this->mUnitBuckets[theBucket].mCount;
    RTSUnit* theResult = NULL;
    RTSUnit* theUnit = this->mUnitBuckets[theBucket].mFirst;
    for (unsigned long i = 0; i < theCount; i++) {
        if (theUnit->mUnitID == inUnitID) {
            theResult = theUnit;
            break;
        }
        theUnit = theUnit->mNext;
    }
    return theResult;
}

// FUNCTION: EXE 0x56f464
// Clears the player's slot, unit list, ids, unit count and vision flags
void RTSPlayer::Reset()
{
    this->mSlot = kSlotNone;
    this->mFirstUnit = NULL;
    this->mLastUnit = NULL;
    this->mNextUniqueID = 0;
    this->mPlayerID = 0;
    this->mUnitCount = 0;
    this->mSeesAll = false;
    this->mSaveVisMap = false;
    this->mObserverMode = false;
}

// FUNCTION: EXE 0x56f49e
// Links a referenced unit into the player's unit list beside the other units of its hash bucket
void RTSPlayer::InsertUnit(RTSUnit* inUnit)
{
    unsigned long theBucket = inUnit->mUnitID;
    inUnit->Reference();
    inUnit->mPlayer = static_cast<EEPlayer*>(this);
    this->mUnitCount++;
    theBucket &= 0x7f;
    if (this->mFirstUnit == NULL) {
        inUnit->InsertAfter(NULL);
        this->mFirstUnit = static_cast<EEUnit*>(inUnit);
        this->mLastUnit = static_cast<EEUnit*>(inUnit);
        this->mUnitBuckets[theBucket].mFirst = static_cast<EEUnit*>(inUnit);
        this->mUnitBuckets[theBucket].mCount = 1;
    } else if (this->mUnitBuckets[theBucket].mCount == 0) {
        inUnit->InsertAfter(this->mLastUnit);
        this->mLastUnit = static_cast<EEUnit*>(inUnit);
        this->mUnitBuckets[theBucket].mFirst = static_cast<EEUnit*>(inUnit);
        this->mUnitBuckets[theBucket].mCount = 1;
    } else {
        inUnit->InsertAfter(this->mUnitBuckets[theBucket].mFirst);
        if (inUnit->mNext == NULL)
            this->mLastUnit = static_cast<EEUnit*>(inUnit);
        this->mUnitBuckets[theBucket].mCount++;
    }
}

// FUNCTION: EXE 0x56f539
// Reads the player's vision flags, ids, color (deriving the shade color), name, units and observer mode;
// returns the first read error
long RTSPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    unsigned long theHashBucketLength = 0;
    unsigned long theLoaded = 0;
    unsigned long theUnitCount = 0;
    theResult = inFile.Read(this->mSaveVisMap);
    if (theResult == 0)
        theResult = inFile.Read(this->mSeesAll);
    if (theResult == 0) {
        if (inMajorVersion == 0 && inMinorVersion <= 0xcf)
            this->mSeesAll = false;
        theResult = inFile.Read(this->mNextUniqueID);
    }
    if (theResult == 0)
        theResult = inFile.Read(this->mPlayerID);
    if (theResult == 0)
        theResult = this->mColor.Load(inFile);
    if (theResult == 0) {
        this->mShadeColor.SetRed(this->mColor.GetRed() * 0.6f);
        this->mShadeColor.SetGreen(this->mColor.GetGreen() * 0.6f);
        this->mShadeColor.SetBlue(this->mColor.GetBlue() * 0.6f);
        theResult = this->mName.Load(inFile);
    }
    if (theResult == 0)
        theResult = inFile.Read(theHashBucketLength);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < 0x80 && theResult == 0; i++) {
        theResult = inFile.Read(theUnitCount);
        if (theResult != 0)
            continue;
        for (unsigned long j = 0; j < theUnitCount && theResult == 0; j++) {
            theResult = this->LoadUnit(inMajorVersion, inMinorVersion, inFile);
            if (theResult == 0)
                theLoaded++;
        }
    }
    this->AddPendingUnits(true, false);
    if (theResult == 0)
        theResult = inFile.Read(this->mObserverMode);
    return theResult;
}

// FUNCTION: EXE 0x56f6ae
// Queues a unit for removal unless it is already queued (forced queues it again)
void RTSPlayer::KillUnit(RTSUnit* inUnit, bool inForce)
{
    if (!inUnit->mIsQueuedForDelete || inForce) {
        inUnit->mIsQueuedForDelete = true;
        this->mKilledUnits.push_back(inUnit);
    }
}

// FUNCTION: EXE 0x56f6d3
// Repairs the loaded pointers of every unit of the player; always succeeds
long RTSPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        theUnit->RepairPointers(inFirst, inSecond);
    return 0;
}

// FUNCTION: EXE 0x56f6f8
// Writes the player's vision flags, ids, color, name, unit count, units and observer mode; returns the first
// write error
long RTSPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mSaveVisMap);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mSeesAll);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mNextUniqueID);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mPlayerID);
    if (theResult != 0)
        return theResult;
    theResult = this->mColor.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mName.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mUnitCount);
    if (theResult != 0)
        return theResult;
    theResult = this->SaveUnits(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mObserverMode);
    return theResult;
}

// FUNCTION: EXE 0x56f7b1
// Writes each hash bucket's unit count and units (the first, then the rest from last to second); returns the first
// write error
long RTSPlayer::SaveUnits(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult;
    unsigned long theTotal = 0;
    for (long i = 0; i < 0x80; i++) {
        unsigned long theCount = this->mUnitBuckets[i].mCount;
        theResult = outFile.Write(theCount);
        if (theResult != 0)
            return theResult;
        unsigned long theSaved = 0;
        RTSUnit* theUnit = this->mUnitBuckets[i].mFirst;
        if (theUnit != NULL) {
            theResult = this->SaveUnit(theUnit, inMajorVersion, inMinorVersion, outFile);
            theSaved = 1;
            unsigned long j;
            for (j = 0; j < theCount - 1; j++)
                theUnit = theUnit->mNext;
            // stop at the first failed unit write (each result used to overwrite the previous one)
            for (j = 0; j < theCount - 1 && theResult == 0; j++) {
                theResult = this->SaveUnit(theUnit, inMajorVersion, inMinorVersion, outFile);
                theSaved++;
                theUnit = theUnit->mPrevious;
            }
            if (theResult != 0)
                return theResult;
        }
        theTotal += theSaved;
    }
    return theResult;
}

// Compiler-generated for this unit (template COMDATs; names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x56f866

// SYNTHETIC: EXE 0x56ef03
// SYNTHETIC: EXE 0x56ef0d
// SYNTHETIC: EXE 0x56ef1e
// SYNTHETIC: EXE 0x56ef2a
// SYNTHETIC: EXE 0x56ef46
// SYNTHETIC: EXE 0x56ef50
// SYNTHETIC: EXE 0x56ef61
// SYNTHETIC: EXE 0x56ef6d
// SYNTHETIC: EXE 0x56ef89
// SYNTHETIC: EXE 0x56ef93
// SYNTHETIC: EXE 0x56efa4
// SYNTHETIC: EXE 0x56efb0
// SYNTHETIC: EXE 0x56efcc
// SYNTHETIC: EXE 0x56efd6
// SYNTHETIC: EXE 0x56efe7
// SYNTHETIC: EXE 0x56eff3
// SYNTHETIC: EXE 0x56f00f
// SYNTHETIC: EXE 0x56f019
// SYNTHETIC: EXE 0x56f02a
// SYNTHETIC: EXE 0x56f036
// SYNTHETIC: EXE 0x56f052
// SYNTHETIC: EXE 0x56f05c
// SYNTHETIC: EXE 0x56f06d
// SYNTHETIC: EXE 0x56f079
