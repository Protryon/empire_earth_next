// C:\EEX\Empire Earth\Units\EEUGLoadContainer.cpp (file name inferred: no __FILE__). The base of the load goals:
// 0x22585a..0x226a43 (no init chain). Unit flags: the exe's module set. Vtable 0x4476b0. The unit closes with the
// std::sort/unique/erase COMDATs of vector<EEComplexUnit*> (0x22660f..) its list ctor instantiates.
#include <windows.h>
#include <stdio.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEAreaUtilities.h"

// The list ctor sorts its units by id (the sort family 0x22660f.. passes an empty predicate and expands the compare:
// unsigned +0x24, `jae`) (provisional name)
struct EEUGLoadContainerUnitLess {
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
    {
        return inFirst->mUnitID < inSecond->mUnitID;
    }
};

// FUNCTION: EXE 0x62585a
// Creates a load goal for a container and a unit list, with the container's and the units' rendezvous tiles and the
// loading distance
EEUGLoadContainer::EEUGLoadContainer(unsigned char inKind, EEComplexUnit* inContainer,
                                     const UReferenceContainer<EEComplexUnit*>& inUnits,
                                     const U2DPoint<long>& inContainerTile, const U2DPoint<long>& inUnitTile,
                                     float inMinimumDistanceSquared, bool inArg)
    : EEUnitGoal(inKind, inArg)
{
    this->mContainer = inContainer;
    inContainer->Reference();
    this->mMinimumDistanceSquared = inMinimumDistanceSquared;
    this->AddUnits(inUnits);
    this->mContainerTile = inContainerTile;
    this->mUnitTile = inUnitTile;
}

// FUNCTION: EXE 0x6258ef
// Adds a newly created unit to the units to load
void EEUGLoadContainer::AddCreatedUnit(EEComplexUnit* inUnit)
{
    this->AddUnit(inUnit);
}

// SYNTHETIC: EXE 0x6258fb
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x625917
// Creates a load goal for a container and units (sorts the caller's vector by unit id and drops its duplicates)
EEUGLoadContainer::EEUGLoadContainer(unsigned char inKind, EEComplexUnit* inContainer,
                                     std::vector<EEComplexUnit*>& inUnits, bool inArg)
    : EEUnitGoal(inKind, inArg)
{
    this->mContainer = inContainer;
    inContainer->Reference();
    std::sort(inUnits.begin(), inUnits.end(), EEUGLoadContainerUnitLess());
    inUnits.erase(std::unique(inUnits.begin(), inUnits.end()), inUnits.end());
    this->AddUnits(inUnits);
}

// FUNCTION: EXE 0x625a18
// Creates an empty load goal with no container
EEUGLoadContainer::EEUGLoadContainer(unsigned char inKind, bool inArg)
    : EEUnitGoal(inKind, inArg)
{
    this->mMinimumDistanceSquared = 0.0f;
    this->mContainer = NULL;
}

// FUNCTION: EXE 0x625a80
// Destroys the goal, releasing the container and every unit to load
EEUGLoadContainer::~EEUGLoadContainer()
{
    if (this->mContainer != NULL)
        this->mContainer->Dereference();
    this->ClearUnits();
}

// FUNCTION: EXE 0x625ab4
// Adds a unit to load, referencing it, unless it is already listed
void EEUGLoadContainer::AddUnit(EEComplexUnit* inUnit)
{
    if (!this->mUnits.Contains(inUnit)) {
        EEComplexUnit* theUnit = inUnit;
        theUnit->Reference();
        this->mUnits.mItems.push_back(theUnit);
    }
}

// FUNCTION: EXE 0x625ae9
// Adds every unit of a unit list to the units to load
void EEUGLoadContainer::AddUnits(const UReferenceContainer<EEComplexUnit*>& inUnits)
{
    for (std::vector<EEComplexUnit*>::const_iterator theIterator = inUnits.mItems.begin();
         theIterator != inUnits.mItems.end(); ++theIterator)
        this->AddUnit(*theIterator);
}

// FUNCTION: EXE 0x625b0e
// Adds every unit of a vector to the units to load
void EEUGLoadContainer::AddUnits(const std::vector<EEComplexUnit*>& inUnits)
{
    for (std::vector<EEComplexUnit*>::const_iterator theIterator = inUnits.begin(); theIterator != inUnits.end();
         ++theIterator)
        this->AddUnit(*theIterator);
}

// FUNCTION: EXE 0x625b33
// Drops all units to load once the container has no free capacity
void EEUGLoadContainer::ClearUnitsIfFull()
{
    if (this->GetFreeCapacity() == 0)
        this->ClearUnits();
}

// FUNCTION: EXE 0x625b4b
// Sends the container to its rendezvous tile; a unit ready to board stays put, others go to a transport's boarding spot
// (or a free tile next to another container, else their rendezvous tile); tells the active group
void EEUGLoadContainer::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;
    U2DPoint<long> theTile;
    EEUCTransport* theClass = dynamic_cast<EEUCTransport*>(this->mContainer->GetUnitClass());
    if (inUnit == this->mContainer)
        theTile = this->mContainerTile;
    else
        theTile = this->mUnitTile;
    ::gWorld.mMap->GetTileLocation(theTile.mX, theTile.mY, theLocation);
    if (inUnit == this->mContainer)
        outLocation = theLocation;
    else if (this->IsLoadable(inUnit) && theClass != NULL && theClass->CanLoad(this->mContainer, inUnit)) {
        const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
        outLocation.SetX(theTransformation.GetX());
        outLocation.SetY(theTransformation.GetY());
        outLocation.SetZ(theTransformation.GetZ());
    } else if (theClass != NULL)
        theClass->FindBoardingLocation(inUnit, this->mContainer, theLocation, outLocation);
    else {
        EEGetTilesAroundUnit(inUnit, this->mContainer, 1, 0, 0, theTiles);
        if (!EEPickTileNear(theTiles, inUnit, this->mContainer, 0, outLocation))
            outLocation = theLocation;
    }
    this->mDestination = outLocation;
    if (inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x625cb0
// Releases and forgets every unit to load
void EEUGLoadContainer::ClearUnits()
{
    UReferenceContainer<EEComplexUnit*>& theUnits = this->mUnits;
    theUnits.Clear();
    theUnits.mItems.clear();
}

// FUNCTION: EXE 0x625cc5
// Writes the destination, unit count, container id, rendezvous tiles and loading distance to the out-of-synch dump,
// then the base goal's
void EEUGLoadContainer::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Actual Destination X: %6.6f\n", this->mDestination.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Actual Destination Y: %6.6f\n", this->mDestination.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Actual Destination Z: %6.6f\n", this->mDestination.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Loadable Unit List Size is %d\n", this->mUnits.GetSize());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Container Unit ID is #%08x\n",
            this->mContainer ? this->mContainer->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Container Rendevous Tile X is %d\n", this->mContainerTile.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Container Rendevous Tile Y is %d\n", this->mContainerTile.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Loadable Unit Rendevous Tile X is %d\n", this->mUnitTile.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Loadable Unit Rendevous Tile Y is %d\n", this->mUnitTile.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadContainer - Minimum Loading Distance Squared is %6.6f\n",
            this->mMinimumDistanceSquared);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x625d83
// Returns the base goal's checksum plus the destination's raw bits, unit count, container id, rendezvous tiles and the
// loading distance's raw bits
long EEUGLoadContainer::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    float theValue = this->mDestination.GetX();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDestination.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDestination.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue) + this->mUnits.GetSize();
    long theContainerID = this->mContainer ? this->mContainer->GetUnitID() : 0;
    long theContainerX = this->mContainerTile.mX;
    long theContainerY = this->mContainerTile.mY;
    long theUnitX = this->mUnitTile.mX;
    long theUnitY = this->mUnitTile.mY;
    theValue = this->mMinimumDistanceSquared;
    return EEUnitGoal::GetOutOfSynchChecksum() + theContainerID + theUnitY + theUnitX + theContainerY + theContainerX
        + *reinterpret_cast<long*>(&theValue) + theChecksum;
}

// FUNCTION: EXE 0x625dfe
// Appends the container, the passengers, the rendezvous tiles and the loading distance to the debug text (the
// "Load Container." header is formatted but never appended)
void EEUGLoadContainer::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "Load Container.\n");
    if (this->mContainer != NULL) {
        sprintf(theText, "Container: %s, (%d,%d)\n", (const char*)this->mContainer->mUnitClass->mName, this->mContainer->mTileX,
                this->mContainer->mTileY);
        ioText += theText;
    }
    sprintf(theText, "Passengers: %d\n", this->mUnits.GetSize());
    ioText += theText;
    if (this->mUnits.GetSize() > 0) {
        for (std::vector<EEComplexUnit*>::iterator theIterator = this->mUnits.mItems.begin();
             theIterator != this->mUnits.mItems.end(); ++theIterator) {
            sprintf(theText, "%s at %d,%d\n", (const char*)(*theIterator)->mUnitClass->mName, (*theIterator)->mTileX,
                    (*theIterator)->mTileY);
            ioText += theText;
        }
    }
    sprintf(theText, "ContainerRendezvous:(%d,%d)\nLoadableUnitRendezvous:(%d,%d)\nMinLoadingDistance:%.4f\n",
            this->mContainerTile.mX, this->mContainerTile.mY, this->mUnitTile.mX, this->mUnitTile.mY, this->mMinimumDistanceSquared);
    ioText += theText;
}

// FUNCTION: EXE 0x625efd
// Returns whether a listed unit is within loading distance of the container with a clear line to it
bool EEUGLoadContainer::IsLoadable(EEComplexUnit* inUnit)
{
    bool theResult = false;
    long theX = 0;
    long theY = 0;
    std::vector<EEComplexUnit*>::iterator theIterator;
    EEUnit* theDistance;
    if (this->mUnits.Contains(inUnit, theIterator) && inUnit != NULL && this->mContainer != NULL
        && this->mContainer->DistanceSquaredTo(inUnit) <= this->mMinimumDistanceSquared)
        theResult = !EEIsLineBlocked(inUnit->GetTileX(), inUnit->GetTileY(), this->mContainer->GetTileX(), this->mContainer->GetTileY(),
                                &theDistance, &theX, &theY);
    return theResult;
}

// FUNCTION: EXE 0x625f7b
// Returns the goal's destination
void EEUGLoadContainer::GetGoalLocation(GE3DPoint& outLocation)
{
    outLocation = this->mDestination;
}

// FUNCTION: EXE 0x625f93
// Loads a unit ready to board, drops the extra units when full, takes a unit that cannot reach the container off both
// load goals, or plays a waiting animation (7 or 8 at random)
void EEUGLoadContainer::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionLoadIntoContainer:
        if (this->IsLoadable(inUnit))
            this->LoadUnit(inUnit);
        break;
    case kActionMoveToRendezvous:
        return;
    case kActionPickedUpLoadableUnits:
        return;
    case kActionGetContainerProgress:
        return;
    case kActionGetNextTransportWaypoint:
        return;
    case kActionReacquireLoadContainer:
        return;
    case kActionBumpExtraLoadableUnits:
        this->ClearUnitsIfFull();
        break;
    case kActionCannotReachContainer: {
            EEUGLoadContainer* theGoal = dynamic_cast<EEUGLoadContainer*>(this->mContainer->GetLoadGoal());
            // the container may no longer have a load goal (Abandon checks the same cast)
            if (theGoal != NULL)
                theGoal->RemoveUnit(inUnit);
        }
        this->RemoveUnit(inUnit);
        break;
    case kActionReacquireGoal:
    case kActionWaitForPassengers:
    case kActionWaitForTransport:
        inUnit->SetCurrentPhysicsAction(3);
        if (inUnit->mAnimation != 7 && inUnit->mAnimation != 8) {
            if (gPhysics->mRandom.Generate() >= 0.5f && !inUnit->mStats->mCanFireOverWalls)
                inUnit->SetAnimation(8, -1, 0);
            else
                inUnit->SetAnimation(7, -1, 0);
        }
        break;
    }
}

// FUNCTION: EXE 0x6260c3
// Reads the base goal, container and unit ids, loading distance, destination and rendezvous tiles (one shared point
// before version 0.221); returns the first read error
long EEUGLoadContainer::LoadContainer(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                                      const UString& inKey)
{
    unsigned long theCount = 0;
    unsigned long theUnitID = -1;
    GE2DPoint thePoint;
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(theUnitID);
    this->mContainer = reinterpret_cast<EEComplexUnit*>(theUnitID);
    if (theResult == 0) {
        theResult = inFile.Read(theCount);
        if (theResult == 0) {
            this->ClearUnits();
            for (unsigned long i = 0; i < theCount; i++) {
                if (theResult != 0)
                    break;
                theResult = inFile.Read(theUnitID);
                if (theResult == 0)
                    this->mUnitIDs.push_back(theUnitID);
            }
            if (theResult == 0)
                theResult = inFile.Read(this->mMinimumDistanceSquared);
        }
    }
    if (inMajorVersion == 0 && inMinorVersion < 0xdd) {
        if (theResult == 0) {
            theResult = thePoint.Load(inFile);
            this->mContainerTile.mX = UMFloor(thePoint.GetX());
            this->mContainerTile.mY = UMFloor(thePoint.GetY());
            this->mUnitTile = this->mContainerTile;
            if (theResult == 0)
                theResult = this->mDestination.Load(inFile);
        }
    } else if (theResult == 0) {
        theResult = this->mDestination.Load(inFile);
        if (theResult == 0) {
            theResult = this->mContainerTile.Load(inFile);
            if (theResult == 0)
                theResult = this->mUnitTile.Load(inFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x626219
// Releases and removes the unit to load at an iterator
void EEUGLoadContainer::RemoveUnitAt(std::vector<EEComplexUnit*>::iterator inIterator)
{
    this->mUnits.Erase(inIterator);
}

// FUNCTION: EXE 0x626229
// Takes a unit that is neither the container nor loaded off the container's own load goal
void EEUGLoadContainer::Abandon(EEComplexUnit* inUnit)
{
    if (this->mContainer == NULL || inUnit == this->mContainer || inUnit->mContainer != NULL)
        return;
    EEUGLoadContainer* theGoal = dynamic_cast<EEUGLoadContainer*>(this->mContainer->GetLoadGoal());
    if (theGoal != NULL && theGoal->HasUnit(inUnit))
        theGoal->RemoveUnit(inUnit);
}

// FUNCTION: EXE 0x62627f
// Turns the loaded ids back into the container and units, referencing them; returns 3 and clears the container when
// it is gone
long EEUGLoadContainer::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = 0;
    RTSUnit* theUnit = ::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mContainer));
    if (theUnit != NULL) {
        this->mContainer = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        this->mContainer->Reference();
        for (std::vector<unsigned long>::iterator theIterator = this->mUnitIDs.begin(); theIterator != this->mUnitIDs.end();
             ++theIterator) {
            RTSUnit* theLoadable = ::gWorld.GetUnit(*theIterator);
            if (theLoadable != NULL && static_cast<EEUnit*>(theLoadable)->mComplexUnit != 0)
                this->AddUnit(static_cast<EEUnit*>(theLoadable)->mComplexUnit);
        }
    } else {
        this->mContainer = NULL;
        theResult = 3;
    }
    this->mUnitIDs.clear();
    return theResult;
}

// FUNCTION: EXE 0x6262e3
// Releases and removes a unit to load
void EEUGLoadContainer::RemoveUnit(EEComplexUnit* inUnit)
{
    this->mUnits.Remove(inUnit);
}

// FUNCTION: EXE 0x6262f3
// Writes the base goal, container and unit ids, loading distance, destination and rendezvous tiles; returns the first
// write error
long EEUGLoadContainer::SaveContainer(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                                      const UString& inKey)
{
    unsigned long theCount = this->mUnits.GetSize();
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mContainer->GetUnitID());
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    // stop at the first failed unit id write (each result used to overwrite the previous one)
    for (std::vector<EEComplexUnit*>::iterator theIterator = this->mUnits.mItems.begin();
         theIterator != this->mUnits.mItems.end() && theResult == 0; ++theIterator)
        theResult = outFile.Write((*theIterator)->GetUnitID());
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mMinimumDistanceSquared);
    if (theResult != 0)
        return theResult;
    theResult = this->mDestination.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mContainerTile.Save(outFile);
    if (theResult == 0)
        theResult = this->mUnitTile.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x6263b1
// Answers the load triggers (dropped by the container's goal, container dead, full, idle or out of waypoints, units
// left to pick up, rendezvous reached); others go to the common goal
bool EEUGLoadContainer::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    U2DPoint<long> theTile;
    switch (inEvent) {
    case kTriggerContainerHasLostGoal: {
            EEUGLoadContainer* theGoal = dynamic_cast<EEUGLoadContainer*>(this->mContainer->GetLoadGoal());
            std::vector<EEComplexUnit*>::iterator theIterator;
            if (theGoal != NULL) {
                EEComplexUnit* theUnit = inUnit;
                if (theGoal->mUnits.Contains(theUnit, theIterator))
                    return false;
            }
            return true;
        }
    case kTriggerContainerHasNoMoreWaypoints: {
            EEComplexUnit* theContainer = this->mContainer;
            return !(theContainer->mHitPoints <= 0 || !theContainer->mWaypoints.empty());
        }
    case kTriggerContainerIsDead: {
            EEComplexUnit* theContainer = this->mContainer;
            return theContainer == NULL || theContainer->mHitPoints <= 0 || theContainer->mIsDisabled;
        }
    case kTriggerContainerIsFull:
        return this->GetFreeCapacity() <= 0 ? true : false;
    case kTriggerContainerIsIdle: {
            EEComplexUnit* theContainer = this->mContainer;
            return !(theContainer->mHitPoints <= 0 || theContainer->GetCommandType() != kActionIdle);
        }
    case kTriggerExtraLoadableUnitsBumped:
        return this->GetRemainingLoad() == 0 ? true : false;
    case kTriggerMoreLoadableUnitsToPickUp:
        return this->GetFreeCapacity() > 0 && this->mUnits.GetSize() > 0 ? true : false;
    case kTriggerNoMoreLoadableUnitsToPickUp:
        return this->GetFreeCapacity() == 0 || this->mUnits.GetSize() == 0 ? true : false;
    case kTriggerHaveReachedRendezvousPoint:
        if (inUnit == this->mContainer)
            theTile = this->mContainerTile;
        else
            theTile = this->mUnitTile;
        return inUnit->DistanceSquaredToTile(theTile) <= 1.0f ? true : false;
    case kTriggerHaveNotReachedRendezvousPoint:
        if (inUnit == this->mContainer)
            theTile = this->mContainerTile;
        else
            theTile = this->mUnitTile;
        return inUnit->DistanceSquaredToTile(theTile) > 1.0f ? true : false;
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// FUNCTION: EXE 0x626596
// Returns whether a unit is on the load list
bool EEUGLoadContainer::HasUnit(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*>::iterator theIterator;
    return this->mUnits.Contains(inUnit, theIterator);
}

// FUNCTION: EXE 0x6265ae
// Replaces the container or a listed unit with the new unit when both have the same player (a listed old unit is
// removed either way)
void EEUGLoadContainer::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    bool theSamePlayer = false;
    if (inOld->mPlayer == inNew->mPlayer)
        theSamePlayer = true;
    if (this->mContainer == inOld && theSamePlayer) {
        this->mContainer->Dereference();
        this->mContainer = inNew;
        inNew->Reference();
    }
    {
        std::vector<EEComplexUnit*>::iterator theIterator;
        if (this->mUnits.Contains(inOld, theIterator)) {
            if (theSamePlayer)
                this->AddUnit(inNew);
            this->RemoveUnit(inOld);
        }
    }
}

// TEMPLATE: EXE 0x62660f
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6266cc
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6266f5
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6267c8
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6267f9
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x62688a
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6268c4
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x626905
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x62692e
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x626957
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x6269bc
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

// TEMPLATE: EXE 0x626a0f
// (a std::sort / unique / erase COMDAT of vector<EEComplexUnit*>; emitted by the list ctor)

