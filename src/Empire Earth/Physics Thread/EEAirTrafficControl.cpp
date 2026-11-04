// C:\EEX\Empire Earth\Physics Thread\EEAirTrafficControl.cpp (name from RTTI EEAirTrafficControl and the Dump strings;
// no __FILE__). 0x14e839..0x151c28: the key chain ("EEAirTrafficControl Data"), the class in address order, then the
// std::vector / std::deque COMDATs its members instantiate. EEPhysics.cpp starts at 0x151c28. Unit flags: the exe's
// module set.
#include <windows.h>
#include <stdio.h>
#include <deque>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEAirTrafficControl.h"

const UString EEAirTrafficControl::sData("EEAirTrafficControl Data");
// SYNTHETIC: EXE 0x54e839   (wrapper)
// SYNTHETIC: EXE 0x54e843   (ctor)
// SYNTHETIC: EXE 0x54e854   (registrar)
// SYNTHETIC: EXE 0x54e860   (dtor)

// FUNCTION: EXE 0x54e87c
// Creates the player's air traffic control with no airports and no air cargo capacity
EEAirTrafficControl::EEAirTrafficControl(EEPlayer* inPlayer)
{
    this->mSlots.clear();
    this->mPlayer = inPlayer;
    this->mMaxCargo = 0;
    this->mUsedCargo = 0;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x54e8b7

// FUNCTION: EXE 0x54e8d3
// Destroys the air traffic control, releasing every queued plane and every airport
EEAirTrafficControl::~EEAirTrafficControl()
{
    std::deque<EEATCItem>::iterator theItem;
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
        for (theItem = theSlot->mTakeoff.begin(); theItem != theSlot->mTakeoff.end(); ++theItem)
            (*theItem).mUnit->Dereference();
        for (theItem = theSlot->mLanding.begin(); theItem != theSlot->mLanding.end(); ++theItem)
            (*theItem).mUnit->Dereference();
        theSlot->mLanding.clear();
        theSlot->mTakeoff.clear();
        theSlot->mAirport->Dereference();
    }
    this->mSlots.clear();
}

// FUNCTION: EXE 0x54e9a8
// Adds the aircraft's weight to the player's used air cargo
void EEAirTrafficControl::AddCargoWeight(EEComplexUnit* inUnit)
{
    this->mUsedCargo += inUnit->GetUnitClass()->mWeight;
}

// FUNCTION: EXE 0x54e9bb
// Registers an airport with empty landing and takeoff queues and adds its capacity to the player's air cargo
void EEAirTrafficControl::AddAirport(EEComplexUnit* inAirport)
{
    EEATCAirport theSlot;
    theSlot.mAirport = inAirport;
    theSlot.mAirport->Reference();
    this->mSlots.push_back(theSlot);
    this->mMaxCargo += inAirport->GetUnitClass()->mTransportCapacity;
}

// FUNCTION: EXE 0x54ea34
// Requeues a plane to land at (kind 1) or take off from an airport, revoking its clearance; returns 0 when
// queued, 7 when no landing room, 3 for an unknown airport or overused air cargo
long EEAirTrafficControl::QueueRequest(EEComplexUnit* inAirport, EEComplexUnit* inUnit, long inKind)
{
    std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin();
    EEATCItem theItem;
    theItem.mUnit = NULL;
    theItem.mKind = 0;
    long theResult = 3;
    if (!(this->mUsedCargo <= this->mMaxCargo) && inKind != 0)
        return theResult;
    while (theSlot != this->mSlots.end()) {
        if (theSlot->mAirport == inAirport) {
        this->RemoveFromEntry(theSlot, inUnit);
        bool theFits;
        if (inKind == 1) {
            long theWeight = inUnit->GetUnitClass()->mWeight;
            theFits = this->GetRoom(theSlot) - theWeight >= 0;
        } else
            theFits = true;
        if (theFits) {
            theItem.mKind = inKind;
            theItem.mUnit = inUnit;
            if (inKind == 1)
                theSlot->mLanding.push_back(theItem);
            else
                theSlot->mTakeoff.push_back(theItem);
            if (inUnit->mIsClearanceGranted)
                inUnit->mIsClearanceGranted = false;
            inUnit->Reference();
            theResult = 0;
        } else
            theResult = 7;
        break;
        }
        ++theSlot;
    }
    return theResult;
}

// FUNCTION: EXE 0x54eaf7
// Returns an airport's free room, or -1 when the airport is not registered
long EEAirTrafficControl::GetFreeCargoWeight(EEComplexUnit* inAirport)
{
    std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin();
    long theResult = -1;
    while (theSlot != this->mSlots.end()) {
        if (theSlot->mAirport == inAirport) {
            theResult = this->GetRoom(theSlot);
            break;
        }
        ++theSlot;
    }
    return theResult;
}

// FUNCTION: EXE 0x54eb2c
// Returns an airport's free room, counting queued landings not yet inside and departures still inside
long EEAirTrafficControl::GetRoom(std::vector<EEATCAirport>::iterator& inSlot)
{
    long theFree = inSlot->mAirport->mCargoRoom;
    bool theHere = false;
    for (std::deque<EEATCItem>::iterator theItem = inSlot->mLanding.begin(); inSlot->mLanding.end() != theItem; ++theItem) {
        theHere = false;
        for (unsigned long i = 0; i < inSlot->mAirport->mCargo.size() && !theHere; i++)
            theHere = (EEUnit*)inSlot->mAirport->mCargo[i] == (*theItem).mUnit;
        if (!theHere)
            theFree -= (*theItem).mUnit->GetUnitClass()->mWeight;
    }
    for (std::deque<EEATCItem>::iterator theOther = inSlot->mTakeoff.begin(); theOther != inSlot->mTakeoff.end(); ++theOther) {
        theHere = false;
        for (unsigned long k = 0; k < inSlot->mAirport->GetNumCargoUnits() && !theHere; k++)
            theHere = (EEUnit*)inSlot->mAirport->mCargo[k] == (*theOther).mUnit;
        if (theHere)
            theFree += (*theOther).mUnit->GetUnitClass()->mWeight;
    }
    return theFree;
}

// FUNCTION: EXE 0x54ec8c
// Removes the plane from the first takeoff queue holding it, revoking its clearance; returns whether found
bool EEAirTrafficControl::RemoveFromTakeoffQueues(EEComplexUnit* inUnit)
{
    std::deque<EEATCItem>::iterator theItem;
    bool theFound = false;
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end() && !theFound; ++theSlot) {
        theItem = theSlot->mTakeoff.begin();
        while (theItem != theSlot->mTakeoff.end()) {
            theFound = (*theItem).mUnit == inUnit;
            if (theFound) {
                if (inUnit->mIsClearanceGranted)
                    inUnit->mIsClearanceGranted = false;
                inUnit->Dereference();
                theSlot->mTakeoff.erase(theItem, theItem + 1);
                break;
            }
            ++theItem;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x54ed30
// Cancels an airport's queued takeoffs of idle planes of a kind (0 all, 1 fighters, 2 bombers, 3 atomic
// bombers), clearing its exiting unit when it is one of them; returns whether any went
bool EEAirTrafficControl::ClearAirportQueue(EEComplexUnit* inAirport, long inKind)
{
    std::deque<EEATCItem>::iterator theItem;
    bool theResult = false;
    bool theMatch = false;
    bool theFound = false;
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end() && !theFound; ++theSlot) {
        theFound = theSlot->mAirport == inAirport;
        if (!theFound)
            continue;
        theItem = theSlot->mTakeoff.begin();
        while (theItem != theSlot->mTakeoff.end()) {
            switch (inKind) {
            case 0:
                theMatch = true;
                break;
            case 1:
                theMatch = (*theItem).mUnit->GetUnitClass()->mFamily == kUnitFamilyAircraft || (*theItem).mUnit->GetUnitClass()->mFamily == kUnitFamilySpaceFighter;
                break;
            case 2:
                theMatch = (*theItem).mUnit->GetUnitClass()->mFamily == kUnitFamilyBomber;
                break;
            case 3:
                theMatch = (*theItem).mUnit->GetUnitClass()->mFamily == kUnitFamilyAtomicBomber;
                break;
            }
            if (theMatch && (*theItem).mUnit->GetCommandType() == kActionIdle) {
                (*theItem).mUnit->SetB342(false);
                (*theItem).mUnit->Dereference();
                if (theSlot->mAirport->mExitingUnitID != -1) {
                    if ((*theItem).mUnit == theSlot->mAirport->FindPassenger(theSlot->mAirport->mExitingUnitID))
                        theSlot->mAirport->SetExitingUnitID(-1);
                }
                theItem = theSlot->mTakeoff.erase(theItem, theItem + 1);
                theMatch = false;
                theResult = true;
            } else {
                ++theItem;
            }
        }
        break;
    }
    return theResult;
}

// NEAR: the original also calls the deque iterator's operator* after each erase and discards the result (a compiled-away
// trace argument; EE_LOST_TRACE reaches 87.5 / 80.4 but not exact, so it is not used)
// FUNCTION: EXE 0x54eed0
// Drops the plane (every plane when NULL) from an airport's landing and takeoff queues
void EEAirTrafficControl::RemoveFromEntry(std::vector<EEATCAirport>::iterator& inSlot, EEComplexUnit* inUnit)
{
    std::deque<EEATCItem>::iterator theItem;
    theItem = inSlot->mLanding.begin();
    while (theItem != inSlot->mLanding.end()) {
        if (inUnit == NULL || (*theItem).mUnit == inUnit) {
            (*theItem).mUnit->Dereference();
            (*theItem).mUnit->SetB342(false);
            theItem = inSlot->mLanding.erase(theItem);
        } else {
            ++theItem;
        }
    }
    theItem = inSlot->mTakeoff.begin();
    while (!(theItem == inSlot->mTakeoff.end())) {
        if (inUnit == NULL || (*theItem).mUnit == inUnit) {
            (*theItem).mUnit->Dereference();
            (*theItem).mUnit->SetB342(false);
            theItem = inSlot->mTakeoff.erase(theItem, theItem + 1);
        } else {
            ++theItem;
        }
    }
}

// NEAR: the original also calls the deque iterator's operator* after each erase and discards the result (a compiled-away
// trace argument; EE_LOST_TRACE reaches 87.5 / 80.4 but not exact, so it is not used)
// FUNCTION: EXE 0x54f033
// Drops dead planes from an airport's landing and takeoff queues
void EEAirTrafficControl::DropDeadPlanes(std::vector<EEATCAirport>::iterator& inSlot)
{
    std::deque<EEATCItem>::iterator theItem;
    theItem = inSlot->mLanding.begin();
    while (theItem != inSlot->mLanding.end()) {
        if ((*theItem).mUnit->mHitPoints <= 0) {
            (*theItem).mUnit->Dereference();
            theItem = inSlot->mLanding.erase(theItem);
        } else {
            ++theItem;
        }
    }
    theItem = inSlot->mTakeoff.begin();
    while (!(theItem == inSlot->mTakeoff.end())) {
        if ((*theItem).mUnit->mHitPoints <= 0) {
            (*theItem).mUnit->Dereference();
            theItem = inSlot->mTakeoff.erase(theItem, theItem + 1);
        } else {
            ++theItem;
        }
    }
}

// FUNCTION: EXE 0x54f15d
// Writes the air cargo totals and every airport's landing and takeoff queues to the out-of-sync dump
void EEAirTrafficControl::Dump(FILE* inFile)
{
    fprintf(inFile, "EEAirTrafficControl::DumpOOSInformation - The max cargo weight is %d\n", this->mMaxCargo);
    fprintf(inFile, "EEAirTrafficControl::DumpOOSInformation - The used cargo weight is %d\n", this->mUsedCargo);
    for (unsigned long i = 0; i < this->mSlots.size(); i++) {
        fprintf(inFile, "EEAirTrafficControl::DumpOOSInformation - The Airport %d ID is #%08x\n", i, this->mSlots[i].mAirport->GetUnitID());
        unsigned long j;
        for (j = 0; j < this->mSlots[i].mLanding.size(); j++)
            fprintf(inFile, "EEAirTrafficControl::DumpOOSInformation - The Landing Request %d has a unit ID of #%08x with a request of %d\n",
                    j, this->mSlots[i].mLanding[j].mUnit->GetUnitID(), this->mSlots[i].mLanding[j].mKind);
        for (j = 0; j < this->mSlots[i].mTakeoff.size(); j++)
            fprintf(inFile, "EEAirTrafficControl::DumpOOSInformation - The Takeoff Request %d has a unit ID of #%08x with a request of %d\n",
                    j, this->mSlots[i].mTakeoff[j].mUnit->GetUnitID(), this->mSlots[i].mTakeoff[j].mKind);
    }
}

// FUNCTION: EXE 0x54f32f
// Lists the player's other airports that have free room, nearest first
void EEAirTrafficControl::GetCandidates(EEComplexUnit* inAirport, std::vector<EEATCCandidate>& outCandidates)
{
    EEATCCandidate theCandidate;
    theCandidate.mAirport = NULL;
    theCandidate.mDistance = 0.0f;
    theCandidate.mFree = 0;
    outCandidates.clear();
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
        long theFree = this->GetRoom(theSlot);
        if (theFree <= 0 || inAirport == theSlot->mAirport)
            continue;
        theCandidate.mAirport = theSlot->mAirport;
        theCandidate.mDistance = inAirport->GetTileDistanceSquared(theSlot->mAirport);
        theCandidate.mFree = theFree;
        outCandidates.push_back(theCandidate);
    }
    if (outCandidates.size() > 0)
        std::sort(outCandidates.begin(), outCandidates.end());
    for (unsigned long i = 0; i < outCandidates.size(); i++) {
        outCandidates[i];
        outCandidates[i];
        outCandidates[i];
        outCandidates[i];
    }
}

// FUNCTION: EXE 0x54f455
// Returns the squared distance in tiles between the two units
float EEUnit::GetTileDistanceSquared(EEUnit* inOther)
{
    long theDeltaY = this->mTileY - inOther->mTileY;
    long theDeltaX = this->mTileX - inOther->mTileX;
    return (float)(theDeltaX * theDeltaX + theDeltaY * theDeltaY);
}

// FUNCTION: EXE 0x54f47d
// Returns a sync checksum of the air cargo totals, the airports and their queued planes and requests
long EEAirTrafficControl::GetChecksum()
{
    long theSum = this->mUsedCargo + this->mMaxCargo;
    for (unsigned long i = 0; i < this->mSlots.size(); i++) {
        theSum += this->mSlots[i].mAirport->GetUnitID();
        unsigned long j;
        for (j = 0; j < this->mSlots[i].mLanding.size(); j++)
            theSum += this->mSlots[i].mLanding[j].mUnit->GetUnitID() + this->mSlots[i].mLanding[j].mKind;
        for (j = 0; j < this->mSlots[i].mTakeoff.size(); j++)
            theSum += this->mSlots[i].mTakeoff[j].mUnit->GetUnitID() + this->mSlots[i].mTakeoff[j].mKind;
    }
    return theSum;
}

// FUNCTION: EXE 0x54f5fa
// Returns the number of planes waiting to take off from an airport
unsigned long EEAirTrafficControl::GetQueueLength(EEComplexUnit* inAirport)
{
    unsigned long theCount = 0;
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
        if (theSlot->mAirport == inAirport) {
            theCount = theSlot->mTakeoff.size();
            break;
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x54f61c
// Returns the plane's place in its home airport's takeoff (0) or landing queue, or -1 when not queued
long EEAirTrafficControl::GetQueuePosition(EEComplexUnit* inUnit, long inWhich)
{
    std::deque<EEATCItem>::iterator theItem;
    long theIndex = -1;
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
        if (theSlot->mAirport != inUnit->mHomeBase)
            continue;
        std::deque<EEATCItem>::iterator theEnd;
        if (inWhich == 0) {
            theItem = theSlot->mTakeoff.begin();
            theEnd = theSlot->mTakeoff.end();
        } else {
            theItem = theSlot->mLanding.begin();
            theEnd = theSlot->mLanding.end();
        }
        while (theItem != theEnd) {
            theIndex++;
            if ((*theItem).mUnit == inUnit)
                break;
            ++theItem;
        }
        if (theItem != theEnd)
            return theIndex;
        theIndex = -1;
    }
    return theIndex;
}

// FUNCTION: EXE 0x54f6d8
// Returns whether the plane is inside its airport
bool EEAirTrafficControl::IsLandingDone(std::vector<EEATCAirport>::iterator& inSlot, EEComplexUnit* inUnit)
{
    return inUnit->mContainer != NULL ? true : false;
}

// NEAR: the comparison of the two heights
// FUNCTION: EXE 0x54f6e9
// Returns whether the plane has left the airport: outside it and either 5% of cruise altitude above it
// (air traffic type 1) or more than sqrt(6) away
bool EEAirTrafficControl::IsTakeOffDone(std::vector<EEATCAirport>::iterator& inSlot, EEComplexUnit* inUnit)
{
    return (inUnit->GetUnitClass()->mAirTrafficType == 1
                ? !(inUnit->mPhysicalModel.GetLocation().GetZ() < inUnit->GetUnitClass()->GetCruiseAltitude() * 0.05f +
                                                                     inSlot->mAirport->mPhysicalModel.GetLocation().GetZ())
                : !(inSlot->mAirport->mPhysicalModel.GetLocation().DistanceSquaredTo(
                        inUnit->mPhysicalModel.GetLocation()) <= 6.0f)) &&
           inUnit->mContainer == NULL;
}

// FUNCTION: EXE 0x54f75c
// Reads the airports, their queued planes (as unit ids) and the air cargo totals; returns the first read error
long EEAirTrafficControl::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    EEATCAirport theSlot;
    EEATCItem theItem;
    unsigned long theNumSlots;
    unsigned long theNumItems;
    unsigned long theID;
    unsigned long i;
    unsigned long j;
    theItem.mUnit = NULL;
    theItem.mKind = 0;
    this->mSlots.clear();
    long theResult = inFile->Read(theNumSlots);
    if (theResult != kErrorOK)
        return theResult;
    for (i = 0; i < theNumSlots; i++) {
        if (theResult != kErrorOK)
            break;
        theResult = inFile->Read(theID);
        theSlot.mAirport = reinterpret_cast<EEComplexUnit*>(theID);
        theSlot.mLanding.clear();
        theSlot.mTakeoff.clear();
        if (theResult == kErrorOK) {
            theResult = inFile->Read(theNumItems);
            if (theResult == kErrorOK) {
                for (j = 0; j < theNumItems; j++) {
                    if (theResult != kErrorOK)
                        break;
                    theResult = inFile->Read(theID);
                    theItem.mUnit = reinterpret_cast<EEComplexUnit*>(theID);
                    if (theResult != kErrorOK)
                        continue;
                    theResult = inFile->Read(theItem.mKind);
                    if (theResult != kErrorOK)
                        continue;
                    if (theItem.mKind == 1)
                        theSlot.mLanding.push_back(theItem);
                    else
                        theSlot.mTakeoff.push_back(theItem);
                }
                this->mSlots.push_back(theSlot);
            }
        }
    }
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile->Read(this->mMaxCargo);
    if (theResult == kErrorOK)
        theResult = inFile->Read(this->mUsedCargo);
    return theResult;
}

// FUNCTION: EXE 0x54f88d
// Hands replaced aircraft's cargo weight and queue entries to their replacements; a replacement that is no
// aircraft leaves every queue
void EEAirTrafficControl::ReplaceUnits(std::vector<EEPUnitSwap>& inSwaps)
{
    std::deque<EEATCItem>::iterator theItem;
    for (std::vector<EEPUnitSwap>::iterator theSwap = inSwaps.begin(); theSwap != inSwaps.end(); ++theSwap) {
        EEComplexUnit* theOld = theSwap->mOld;
        EEComplexUnit* theNew = theSwap->mNew;
        bool theOldIsAircraft = theOld->GetUnitClass()->mCategory == kUnitCategoryAirplane || theOld->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship;
        bool theNewIsAircraft = theNew->GetUnitClass()->mCategory == kUnitCategoryAirplane || theNew->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship;
        if (theOldIsAircraft)
            this->RemoveCargoWeight(theOld);
        if (theNewIsAircraft)
            this->AddCargoWeight(theNew);
        if (!theOldIsAircraft)
            continue;
        for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
            if (!theNewIsAircraft) {
                this->RemoveFromEntry(theSlot, theOld);
                continue;
            }
            for (theItem = theSlot->mLanding.begin(); theItem != theSlot->mLanding.end(); ++theItem) {
                if ((*theItem).mUnit != theOld)
                    continue;
                (*theItem).mUnit->Dereference();
                (*theItem).mUnit = theNew;
                (*theItem).mUnit->Reference();
            }
            for (theItem = theSlot->mTakeoff.begin(); theItem != theSlot->mTakeoff.end(); ++theItem) {
                if ((*theItem).mUnit != theOld)
                    continue;
                (*theItem).mUnit->Dereference();
                (*theItem).mUnit = theNew;
                (*theItem).mUnit->Reference();
            }
        }
    }
}

// FUNCTION: EXE 0x54fa2b
// For a lost airport: destroys departing planes still low outside it and sends each landing plane to the
// nearest airport with room, destroying it when there is none
void EEAirTrafficControl::SendPlanesAway(std::vector<EEATCAirport>::iterator& inSlot)
{
    std::vector<EEATCCandidate> theCandidates;
    std::deque<EEATCItem>::iterator theItem;
    this->GetCandidates(inSlot->mAirport, theCandidates);
    for (theItem = inSlot->mTakeoff.begin(); theItem != inSlot->mTakeoff.end(); ++theItem) {
        EEComplexUnit* theUnit = (*theItem).mUnit;
        if (theUnit->mPhysicalModel.GetLocation().GetZ() < theUnit->GetUnitClass()->GetCruiseAltitude() * 0.35f && theUnit->mContainer == NULL && theUnit->mHitPoints > 0)
            theUnit->Kill(1, 0, 0);
    }
    for (theItem = inSlot->mLanding.begin(); theItem != inSlot->mLanding.end(); ++theItem) {
        EEComplexUnit* theUnit = (*theItem).mUnit;
        if (!(theUnit->mHitPoints > 0))
            continue;
        long theNeeded = theUnit->GetUnitClass()->mWeight;
        EEComplexUnit* theTarget = NULL;
        for (unsigned long i = 0; i < theCandidates.size() && theTarget == NULL; i++) {
            if (theCandidates[i].mFree >= theNeeded)
                theTarget = theCandidates[i].mAirport;
        }
        if (theTarget != NULL) {
            (*theItem).mUnit->SetHomeBase(theTarget);
            (*theItem).mUnit->StartAction(kActionRequestLanding);
            (*theItem).mUnit->GetUnitClass()->ActionChanged((*theItem).mUnit);
            theTarget = NULL;
        } else {
            (*theItem).mUnit->Kill(1, 0, 0);
        }
    }
}

// FUNCTION: EXE 0x54fc39
// Subtracts the aircraft's weight from the player's used air cargo
void EEAirTrafficControl::RemoveCargoWeight(EEComplexUnit* inUnit)
{
    this->mUsedCargo -= inUnit->GetUnitClass()->mWeight;
}

// FUNCTION: EXE 0x54fc4c
// Unregisters an airport: drops its dead planes and capacity, sends its planes away and releases them
void EEAirTrafficControl::RemoveAirport(EEComplexUnit* inAirport)
{
    std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin();
    while (theSlot != this->mSlots.end()) {
        EEComplexUnit* theAirport = theSlot->mAirport;
        if (inAirport == theAirport) {
            this->DropDeadPlanes(theSlot);
            this->mMaxCargo -= theAirport->GetUnitClass()->mTransportCapacity;
            this->SendPlanesAway(theSlot);
            this->RemoveFromEntry(theSlot, NULL);
            theAirport->Dereference();
            theSlot = this->mSlots.erase(theSlot);
        } else {
            ++theSlot;
        }
    }
}

// FUNCTION: EXE 0x54fcc1
// Drops the plane from the given airport's queues
void EEAirTrafficControl::RemovePlane(EEComplexUnit* inAirport, EEComplexUnit* inUnit)
{
    for (std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin(); theSlot != this->mSlots.end(); ++theSlot) {
        if (theSlot->mAirport == inAirport) {
            this->RemoveFromEntry(theSlot, inUnit);
            return;
        }
    }
}

// FUNCTION: EXE 0x54fcf4
// Turns the loaded unit ids back into airports and planes, dropping those that no longer exist; returns 0
long EEAirTrafficControl::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin();
    while (theSlot != this->mSlots.end()) {
        EEComplexUnit* theAirport = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit((unsigned long)theSlot->mAirport));
        if (theAirport != NULL) {
            theSlot->mAirport = theAirport;
            theAirport->Reference();
            std::deque<EEATCItem>::iterator theItem = theSlot->mLanding.begin();
            while (theItem != theSlot->mLanding.end()) {
                EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit((unsigned long)(*theItem).mUnit));
                if (theUnit != NULL) {
                    (*theItem).mUnit = theUnit;
                    theUnit->Reference();
                    ++theItem;
                } else {
                    theItem = theSlot->mLanding.erase(theItem);
                }
            }
            theItem = theSlot->mTakeoff.begin();
            while (theItem != theSlot->mTakeoff.end()) {
                EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit((unsigned long)(*theItem).mUnit));
                if (theUnit != NULL) {
                    (*theItem).mUnit = theUnit;
                    theUnit->Reference();
                    ++theItem;
                } else {
                    theItem = theSlot->mTakeoff.erase(theItem);
                }
            }
            ++theSlot;
        } else {
            theSlot = this->mSlots.erase(theSlot);
        }
    }
    return 0;
}

// FUNCTION: EXE 0x54fead
// Writes the airports, their queued planes (unit id and request) and the air cargo totals; returns the first error
long EEAirTrafficControl::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    unsigned long theNumSlots = this->mSlots.size();
    unsigned long theValue;
    long theResult = inFile->Write(theNumSlots);
    for (unsigned long i = 0; i < theNumSlots && theResult == 0; i++) {
        theValue = this->mSlots[i].mAirport->GetUnitID();
        theResult = inFile->Write(theValue);
        if (theResult != 0)
            break;
        theValue = this->mSlots[i].mLanding.size() + this->mSlots[i].mTakeoff.size();
        theResult = inFile->Write(theValue);
        unsigned long j;
        for (j = 0; j < this->mSlots[i].mLanding.size() && theResult == 0; j++) {
            theValue = this->mSlots[i].mLanding[j].mUnit->GetUnitID();
            theResult = inFile->Write(theValue);
            if (theResult == 0)
                theResult = inFile->Write((unsigned long&)this->mSlots[i].mLanding[j].mKind);
        }
        for (j = 0; j < this->mSlots[i].mTakeoff.size() && theResult == 0; j++) {
            theValue = this->mSlots[i].mTakeoff[j].mUnit->GetUnitID();
            theResult = inFile->Write(theValue);
            if (theResult == 0)
                theResult = inFile->Write((unsigned long&)this->mSlots[i].mTakeoff[j].mKind);
        }
    }
    if (theResult == 0) {
        theResult = inFile->Write(this->mMaxCargo);
        if (theResult == 0)
            theResult = inFile->Write(this->mUsedCargo);
    }
    return theResult;
}

// FUNCTION: EXE 0x55014c
// Returns the nearest other airport with room for the plane, or NULL (also when air cargo is overused)
EEComplexUnit* EEAirTrafficControl::FindNearestAirport(EEComplexUnit* inUnit)
{
    std::vector<EEATCCandidate> theCandidates;
    EEComplexUnit* theResult = NULL;
    long theNeeded = inUnit->GetUnitClass()->mWeight;
    if (this->mUsedCargo <= this->mMaxCargo) {
        this->GetCandidates(inUnit, theCandidates);
        for (unsigned long i = 0; i < theCandidates.size() && theResult == NULL; i++) {
            if (theCandidates[i].mFree >= theNeeded)
                theResult = theCandidates[i].mAirport;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5501d4
// Advances every airport's landings, then its takeoffs unless a landing blocks them; a destroyed airport
// is removed and its planes sent away
void EEAirTrafficControl::Update()
{
    std::vector<EEATCAirport>::iterator theSlot = this->mSlots.begin();
    while (theSlot != this->mSlots.end()) {
        EEComplexUnit* theAirport = theSlot->mAirport;
        bool theFlag = false;
        if (theAirport->mHitPoints > 0) {
            this->UpdateLandings(theSlot, &theFlag);
            if (!theFlag)
                this->UpdateTakeoffs(theSlot);
            ++theSlot;
        } else {
            this->DropDeadPlanes(theSlot);
            this->mMaxCargo -= theAirport->GetUnitClass()->mTransportCapacity;
            this->SendPlanesAway(theSlot);
            this->RemoveFromEntry(theSlot, NULL);
            theAirport->Dereference();
            theSlot = this->mSlots.erase(theSlot);
        }
    }
}

// Never lets a landing block takeoffs (always false). ICF-folded into 0x4bb118 (`xor al, al; ret 4`): the
// original's call in UpdateLandings binds there (no own address)
bool EEAirTrafficControl::BlocksTakeoffs(EEComplexUnit* inUnit)
{
    return false;
}

// FUNCTION: EXE 0x550277
// Clears landing planes in queue order, redirecting or destroying one the overfull airport cannot take;
// drops the first landed or dead plane and stops there, returning whether one was dropped
bool EEAirTrafficControl::UpdateLandings(std::vector<EEATCAirport>::iterator& inSlot, bool* outFlag)
{
    std::vector<EEATCCandidate> theCandidates;
    bool theDone = false;
    std::deque<EEATCItem>::iterator theItem = inSlot->mLanding.begin();
    while (theItem != inSlot->mLanding.end() && !theDone) {
        EEComplexUnit* theUnit = (*theItem).mUnit;
        long theWeight = (*theItem).mUnit->GetUnitClass()->mWeight;
        if (theUnit->mHitPoints > 0) {
            if (theUnit->mIsClearanceGranted != true)
                theUnit->mIsClearanceGranted = true;
            theDone = this->IsLandingDone(inSlot, theUnit);
            if (!theDone) {
                if (this->GetRoom(inSlot) < -theWeight) {
                    this->GetCandidates(inSlot->mAirport, theCandidates);
                    EEComplexUnit* theTarget = NULL;
                    for (unsigned long i = 0; i < theCandidates.size() && theTarget == NULL; i++) {
                        if (theCandidates[i].mFree >= theWeight)
                            theTarget = theCandidates[i].mAirport;
                    }
                    if (theTarget != NULL) {
                        if (this->QueueRequest(theTarget, (*theItem).mUnit, 1) == 0) {
                            (*theItem).mUnit->SetHomeBase(theTarget);
                            (*theItem).mUnit->StartAction(kActionRequestLanding);
                            (*theItem).mUnit->GetUnitClass()->ActionChanged((*theItem).mUnit);
                        } else {
                            (*theItem).mUnit->Kill(1, 0, 0);
                        }
                    } else {
                        (*theItem).mUnit->Kill(1, 0, 0);
                    }
                }
                if (this->BlocksTakeoffs((*theItem).mUnit))
                    *outFlag = true;
                ++theItem;
                continue;
            }
        } else {
            theDone = true;
        }
        theUnit->Dereference();
        theItem = inSlot->mLanding.erase(theItem, theItem + 1);
    }
    return theDone;
}

// FUNCTION: EXE 0x550487
// Clears departing planes in queue order and drops each that has left or died; false at the first still leaving
bool EEAirTrafficControl::UpdateTakeoffs(std::vector<EEATCAirport>::iterator& inSlot)
{
    bool theResult = false;
    std::deque<EEATCItem>::iterator theItem;
    theItem = inSlot->mTakeoff.begin();
    while (theItem != inSlot->mTakeoff.end()) {
        EEComplexUnit* theUnit = (*theItem).mUnit;
        if (theUnit->mHitPoints > 0) {
            if (theUnit->mIsClearanceGranted != true)
                theUnit->mIsClearanceGranted = true;
            theResult = this->IsTakeOffDone(inSlot, theUnit);
        } else {
            theResult = true;
        }
        if (!theResult)
            break;
        theUnit->Dereference();
        if (theUnit->mIsClearanceGranted)
            theUnit->mIsClearanceGranted = false;
        theItem = inSlot->mTakeoff.erase(theItem, theItem + 1);
    }
    return theResult;
}

// Template COMDATs (std::vector / deque / sort over the types above), in address order:
// TEMPLATE: EXE 0x54ea04
// TEMPLATE: EXE 0x550549
// TEMPLATE: EXE 0x55056b
// TEMPLATE: EXE 0x5505f2
// TEMPLATE: EXE 0x55061e
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x550691
// TEMPLATE: EXE 0x5506cc
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5506f8
// TEMPLATE: EXE 0x55083e
// TEMPLATE: EXE 0x55093e
// TEMPLATE: EXE 0x550964
// TEMPLATE: EXE 0x5509bb
// TEMPLATE: EXE 0x550a25
// TEMPLATE: EXE 0x550a5d
// TEMPLATE: EXE 0x550ab7
// TEMPLATE: EXE 0x550afb
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x550b35
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x550b47
// TEMPLATE: EXE 0x550b70
// TEMPLATE: EXE 0x550e63
// TEMPLATE: EXE 0x550e8a
// TEMPLATE: EXE 0x550ec6
// TEMPLATE: EXE 0x550ef1
// TEMPLATE: EXE 0x550f08
// TEMPLATE: EXE 0x550fb3
// TEMPLATE: EXE 0x550fdc
// TEMPLATE: EXE 0x551000
// TEMPLATE: EXE 0x55102c
// TEMPLATE: EXE 0x551049
// TEMPLATE: EXE 0x551075
// TEMPLATE: EXE 0x55108b
// TEMPLATE: EXE 0x5510b8
// TEMPLATE: EXE 0x5510ca
// (already compiled by this unit)

// TEMPLATE: EXE 0x5510f1
// TEMPLATE: EXE 0x55121b
// TEMPLATE: EXE 0x551240
// TEMPLATE: EXE 0x551305
// TEMPLATE: EXE 0x55133d
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x5516eb
// TEMPLATE: EXE 0x551734
// TEMPLATE: EXE 0x5517b7
// TEMPLATE: EXE 0x551867
// TEMPLATE: EXE 0x5518a1
// TEMPLATE: EXE 0x5518d7
// TEMPLATE: EXE 0x5519de
// TEMPLATE: EXE 0x551a08
// TEMPLATE: EXE 0x551a6a
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x551a8f
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x551ab4
// TEMPLATE: EXE 0x551b33
// TEMPLATE: EXE 0x551b90
// TEMPLATE: EXE 0x551be8
