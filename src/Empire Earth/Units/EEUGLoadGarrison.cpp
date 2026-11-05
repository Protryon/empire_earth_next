// C:\EEX\Empire Earth\Units\EEUGLoadGarrison.cpp (file name inferred: no __FILE__; the key "EEUGLoadGarrison-Data").
// Garrisoning units in a building: 0x226a43..0x2271c7. Unit flags: the exe's module set. Data: the key 0x5635b0
// (guard byte 0x5635a8); vtable 0x447754.


#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEEvent.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadGarrison.h"

const UString EEUGLoadGarrison::sData("EEUGLoadGarrison-Data");
// SYNTHETIC: EXE 0x626a43   (wrapper)
// SYNTHETIC: EXE 0x626a4d   (ctor)
// SYNTHETIC: EXE 0x626a5e   (registrar)
// SYNTHETIC: EXE 0x626a6a   (dtor)

// FUNCTION: EXE 0x626a86
// Creates a goal to garrison units in a building, heading for the building's tile, with a given upgrade, garrison
// cost and validity
EEUGLoadGarrison::EEUGLoadGarrison(EEComplexUnit* inContainer, const UReferenceContainer<EEComplexUnit*>& inUnits,
                                   const U2DPoint<long>& inContainerTile, const U2DPoint<long>& inUnitTile,
                                   float inMinimumDistanceSquared, long inCost, long inUpgrade, bool inIsValid,
                                   bool inArg)
    : EEUGLoadContainer(0xc, inContainer, inUnits, inContainerTile, inUnitTile, inMinimumDistanceSquared, inArg),
      mUpgrade(inUpgrade), mCost(inCost), mIsValid(inIsValid)
{
    this->mDestination.SetX((float)inContainerTile.mX);
    this->mDestination.SetY((float)inContainerTile.mY);
    this->mDestination.SetZ(0.0f);
}

// FUNCTION: EXE 0x626adb
// Returns the number of units garrisoned in the building
long EEUGLoadGarrison::GetUsedCapacity()
{
    return this->mContainer->mGarrisonCount;
}

// FUNCTION: EXE 0x626ae5
// Returns the building's garrison capacity
long EEUGLoadGarrison::GetCapacity()
{
    return this->mContainer->mUnitClass->mGarrisonCapacity;
}

// SYNTHETIC: EXE 0x626af2
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x626b0e
// Creates a goal to garrison units in a building and works out its rendezvous, load distance and upgrade
EEUGLoadGarrison::EEUGLoadGarrison(EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits, bool inArg)
    : EEUGLoadContainer(0xc, inContainer, inUnits, inArg)
{
    this->Initialize();
}

// FUNCTION: EXE 0x626b37
// Creates an empty garrison goal with no building or units
EEUGLoadGarrison::EEUGLoadGarrison(bool inArg)
    : EEUGLoadContainer(0xc, inArg)
{
}

// FUNCTION: EXE 0x626b51
// Destroys the goal (the load-container base releases the building and units)
EEUGLoadGarrison::~EEUGLoadGarrison()
{
}

// FUNCTION: EXE 0x626b5c
// Sets the squared load distance to (1.2 x the building's tile size, at least 2) squared
void EEUGLoadGarrison::ComputeLoadDistance()
{
    float theDistance = this->mContainer->mUnitClass->mTileSize * 1.2f;
    this->mMinimumDistanceSquared = max(2.0f, theDistance) * max(2.0f, theDistance);
}

// FUNCTION: EXE 0x626ba4
// Makes the building's tile the meeting tile for building and units, heading for the building's centre
void EEUGLoadGarrison::ComputeRendezvous()
{
    float theX = (float)this->mContainer->mTileX;
    float theY = (float)this->mContainer->mTileY;
    this->mContainer->AddCenterOffset(theX, theY);
    this->mContainerTile = U2DPoint<long>(this->mContainer->mTileX, this->mContainer->mTileY);
    this->mUnitTile = this->mContainerTile;
    this->mDestination.SetX(theX);
    this->mDestination.SetY(theY);
}

// FUNCTION: EXE 0x626bf1
// Looks up the unit class the building's garrison technology turns it into; the goal is valid (cost = garrison
// capacity) when that class is available, or with no upgrade for a capitol or improved granary
void EEUGLoadGarrison::ComputeUpgrade()
{
    long theTechnology = this->mContainer->mUnitClass->mGarrisonTechnology;
    this->mIsValid = false;
    EEPlayer* thePlayer = this->mContainer->mPlayer;
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theTechnology);
    if (theNode != NULL) {
        EEEvent* theEvent = EEEvent::Get(theNode->mData->mResearchedEvent);
        this->mUpgrade = EEEvent::GetEventEffect(theEvent->GetEffect(0))->mNewUnitClass;
        EEUnitClass* theClass = gUnitClassTable[this->mUpgrade];
        if (theClass != NULL) {
            EETechTreeNode* theUpgrade = thePlayer->GetTechTree()->Get(theClass->GetTechnology());
            if (theUpgrade != NULL && theUpgrade->GetIsAvailable()) {
                this->mCost = this->mContainer->mUnitClass->mGarrisonCapacity;
                this->mIsValid = true;
            }
        }
    } else {
        long theType = this->mContainer->mUnitClass->mUnitType;
        if (theType == kUnitTypeCapitol || theType == kUnitTypeGranaryImproved) {
            this->mUpgrade = 0;
            this->mIsValid = true;
            this->mCost = this->mContainer->mUnitClass->mGarrisonCapacity;
        }
    }
}

// FUNCTION: EXE 0x626cd6
// Returns a copy of the goal for the same building, units, tiles and upgrade
EEUnitGoal* EEUGLoadGarrison::Clone()
{
    return new EEUGLoadGarrison(this->mContainer, this->mUnits, this->mContainerTile, this->mUnitTile, this->mMinimumDistanceSquared, this->mCost,
                                this->mUpgrade, this->mIsValid, this->GetB1C());
}

// FUNCTION: EXE 0x626d22
// Writes the upgrade, garrison cost and valid flag to the out-of-synch dump, then the load-container base's
void EEUGLoadGarrison::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadGarrison - Garrison upgrade is %ld\n", this->mUpgrade);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadGarrison - Garrison Cost is %ld\n", this->mCost);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLoadGarrison - Is Valid is %d\n", this->mIsValid == true);
    EEUGLoadContainer::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x626d6a
// Returns the load-container base's checksum plus the valid flag, garrison cost and upgrade
long EEUGLoadGarrison::GetOutOfSynchChecksum()
{
    long theResult = (this->mIsValid == true) + this->mCost + this->mUpgrade;
    return theResult + EEUGLoadContainer::GetOutOfSynchChecksum();
}

// FUNCTION: EXE 0x626d85
// Appends "Load Garrison" to the one-line debug text
void EEUGLoadGarrison::GetDebugLine(UWideString& ioText)
{
    ioText += "Load Garrison";
}

// FUNCTION: EXE 0x626d97
// Appends "Load Garrison - Part of:" and the load-container base's text to the debug text
void EEUGLoadGarrison::GetDebugText(UWideString& ioText)
{
    ioText += "Load Garrison - Part of:";
    EEUGLoadContainer::GetDebugText(ioText);
}

// FUNCTION: EXE 0x626db8
// Returns how many more units the garrison cost needs
long EEUGLoadGarrison::GetRemainingLoad()
{
    return this->mCost - this->mContainer->mGarrisonCount;
}

// FUNCTION: EXE 0x626dc5
// Returns the number of free garrison places in the building
long EEUGLoadGarrison::GetFreeCapacity()
{
    return this->mContainer->mUnitClass->mGarrisonCapacity - this->mContainer->mGarrisonCount;
}

// FUNCTION: EXE 0x626dd8
// Works out the rendezvous, the load distance and the upgrade
void EEUGLoadGarrison::Initialize()
{
    this->ComputeRendezvous();
    this->ComputeLoadDistance();
    this->ComputeUpgrade();
}

// FUNCTION: EXE 0x626df6
// Upgrades the building on the garrison-upgrade action, waits while waiting for the garrison cost, else steps the
// load-container base
void EEUGLoadGarrison::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionGarrisonUpgrade:
        this->UpgradeContainer();
        break;
    case kActionWaitForGarrisonCost:
        break;
    default:
        EEUGLoadContainer::Step(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x626e25
// Reads the load-container base, garrison cost, upgrade and valid flag from the saved game; returns the first read
// error
long EEUGLoadGarrison::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->LoadContainer(inMajorVersion, inMinorVersion, inFile, EEUGLoadGarrison::sData);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mCost);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mUpgrade);
    if (theResult == 0)
        theResult = inFile.Read(this->mIsValid);
    return theResult;
}

// FUNCTION: EXE 0x626e72
// Garrisons one of the goal's units (it leaves the building's own garrison goal and drops off its resources first);
// plays the garrison sound and returns true, or idles the unit when the cost is met or the building refuses it
bool EEUGLoadGarrison::LoadUnit(EEComplexUnit* inUnit)
{
    EEComplexUnit* theContainer = this->mContainer;
    bool theResult = false;
    if (inUnit != NULL && this->mContainer != NULL) {
        std::vector<EEComplexUnit*>::iterator theIterator;
        if (this->mUnits.Contains(inUnit, theIterator)) {
            EEUGLoadGarrison* theGoal = dynamic_cast<EEUGLoadGarrison*>(this->mContainer->GetGoal());
            if (theGoal != NULL)
                theGoal->RemoveUnit(inUnit);
            if (inUnit->mResourceAmount > 0 && this->mContainer->CanTake(inUnit, inUnit->mResourceType))
                EEDepositLoad(inUnit, this->mContainer);
            if ((this->mCost == this->mContainer->mGarrisonCount || !this->mContainer->GarrisonUnit(inUnit)) && inUnit->mHitPoints > 0) {
                inUnit->StartAction(kActionIdle);
                inUnit->mUnitClass->ActionChanged(inUnit);
            } else {
                theResult = true;
                EESoundObject* theSound = EESoundObject::Get(kSoundGarrison);
                if (theSound != NULL) {
                    float theVolume;
                    float thePan;
                    float thePitch;
                    theContainer->mUnitClass->GetSoundParameters(theContainer, theSound, &theVolume, &thePan, &thePitch);
                    theSound->Play(theVolume, thePan, thePitch);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x626f8c
// Starts a living unit preparing to move
void EEUGLoadGarrison::Start(EEComplexUnit* inUnit)
{
    if (inUnit->mHitPoints > 0) {
        inUnit->StartAction(kActionPrepareToMove);
        inUnit->mUnitClass->ActionChanged(inUnit);
    }
}

// FUNCTION: EXE 0x626fb0
// Writes the load-container base, garrison cost, upgrade and valid flag to the saved game; returns the first write
// error
long EEUGLoadGarrison::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->SaveContainer(inMajorVersion, inMinorVersion, outFile, EEUGLoadGarrison::sData);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mCost);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mUpgrade);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsValid);
    return theResult;
}

// FUNCTION: EXE 0x627003
// Answers the garrison triggers (building in range and able to take the unit, garrison cost met, goal is a load,
// trigger 0x2ee7: building alive but unit dead, facing done/not needed, no waypoints); others go to the base
bool EEUGLoadGarrison::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    switch (inEvent) {
    case kTriggerContainerInRange:
        if (inUnit == this->mContainer || this->mContainer->mHitPoints <= 0 || !this->IsLoadable(inUnit) || !inUnit->mUnitClass->mCanGarrison
            || this->mContainer->GetCommandType() == kActionGarrisonUpgrade)
            return false;
        return true;
    case kTriggerGarrisonCostMet:
        return this->mCost == this->mContainer->mGarrisonCount ? true : false;
    case kTriggerGoalIsLoadGarrison:
    case kTriggerGoalIsLoadContainer:
        return this->mKind == kGoalLoadGarrison ? true : false;
    case 0x2ee7:
        if (this->mContainer->mHitPoints > 0 && inUnit->mHitPoints <= 0)
            return true;
        return false;
    case kTriggerCompletedUnitFacing:
    case kTriggerUnitFacingNotRequired:
        return true;
    case kTriggerNoMoreWaypoints:
        return false;
    }
    return EEUGLoadContainer::EvaluateCondition(inUnit, inEvent);
}

// FUNCTION: EXE 0x6270e0
// Follows a replaced building (a same-player one is retargeted, unless the unit's next goal is a garrison, which is
// cleared) and swaps a replaced unit in the load list (dropped when the new one changed player)
void EEUGLoadGarrison::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit* inUnit)
{
    std::vector<long> theUnused;
    bool theSamePlayer = false;
    if (inOld->mPlayer == inNew->mPlayer)
        theSamePlayer = true;
    if (this->mContainer == inOld) {
        if (inUnit != inNew && inUnit->GetQueuedGoal(1) != NULL && inUnit->GetQueuedGoal(1)->mKind == kGoalLoadGarrison) {
            inUnit->ClearGoals(1);
            return;
        }
        if (theSamePlayer && inNew != inUnit) {
            this->mContainer->Dereference();
            this->mContainer = inNew;
            inNew->Reference();
            this->Initialize();
        }
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

// FUNCTION: EXE 0x62719e
// Replaces the building with its upgraded unit class, when there is one
void EEUGLoadGarrison::UpgradeContainer()
{
    if (this->mUpgrade != 0 && gUnitClassTable[this->mUpgrade] != NULL)
        EEReplaceUnit(this->mContainer, this->mContainer->mPlayer->mPlayerID, gUnitClassTable[this->mUpgrade]);
}
