// C:\EEX\Empire Earth\Units\EEUGLoadTransport.cpp (file name inferred: no __FILE__; the key "EEUGLoadTransport-Data").
// Loading units into a transport: 0x2271c7..0x227621. Unit flags: the exe's module set. Data: the key 0x5635c8 (guard
// byte 0x5635c0); vtable 0x4477f8. Its Virtual30 (0x1f40) is the ICF representative of the load goals' slot.

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

#include "ee/EEUnitKind.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadTransport.h"

const UString EEUGLoadTransport::sData("EEUGLoadTransport-Data");
// SYNTHETIC: EXE 0x6271c7   (wrapper)
// SYNTHETIC: EXE 0x6271d1   (ctor)
// SYNTHETIC: EXE 0x6271e2   (registrar)
// SYNTHETIC: EXE 0x6271ee   (dtor)

// FUNCTION: EXE 0x62720a
// Creates a goal to load units into a transport, with given meeting tiles and squared load distance
EEUGLoadTransport::EEUGLoadTransport(EEComplexUnit* inTransport, const UReferenceContainer<EEComplexUnit*>& inUnits,
                                     const U2DPoint<long>& inTransportTile, const U2DPoint<long>& inUnitTile,
                                     float inMinimumDistanceSquared, bool inArg)
    : EEUGLoadContainer(7, inTransport, inUnits, inTransportTile, inUnitTile, inMinimumDistanceSquared, inArg)
{
}

// FUNCTION: EXE 0x62723a
// Returns the transport capacity already taken by cargo
long EEUGLoadTransport::GetUsedCapacity()
{
    return this->mContainer->mUnitClass->mTransportCapacity - this->mContainer->mCargoRoom;
}

// FUNCTION: EXE 0x62724d
// Returns the transport's capacity
long EEUGLoadTransport::GetCapacity()
{
    return this->mContainer->mUnitClass->mTransportCapacity;
}

// FUNCTION: EXE 0x62725a
// Returns the transport's free cargo room
long EEUGLoadTransport::GetFreeCapacity()
{
    return this->mContainer->mCargoRoom;
}

// FUNCTION: EXE 0x627264
// Returns prepare-to-move as the first action
long EEUGLoadTransport::GetStartAction()
{
    return kActionPrepareToMove;
}

// SYNTHETIC: EXE 0x62726a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x627286
// Creates a goal to load units into a transport and works out its meeting tiles and load distance
EEUGLoadTransport::EEUGLoadTransport(EEComplexUnit* inTransport, std::vector<EEComplexUnit*>& inUnits, bool inArg)
    : EEUGLoadContainer(7, inTransport, inUnits, inArg)
{
    this->ComputeRendezvous();
    this->ComputeLoadDistance();
}

// FUNCTION: EXE 0x6272b6
// Creates an empty load-transport goal with no transport or units
EEUGLoadTransport::EEUGLoadTransport(bool inArg)
    : EEUGLoadContainer(7, inArg)
{
}

// FUNCTION: EXE 0x6272d0
// Destroys the goal (the load-container base releases the transport and units)
EEUGLoadTransport::~EEUGLoadTransport()
{
}

// FUNCTION: EXE 0x6272db
// Sets the squared load distance to the transport's load range plus half the first unit's inner collision sphere,
// squared and rounded up
void EEUGLoadTransport::ComputeLoadDistance()
{
    float theDistance = 0.0f;
    if (this->mUnits.GetSize() > 0)
        theDistance = this->mUnits.mItems[0]->mUnitClass->mInnerCollisionSphere * 0.5f;
    // STAND-IN (owner 2026-10-31): (double) cast on a float local in an FPU sum; makes 0x6272db exact
    this->mMinimumDistanceSquared = UMCeil(UMSquare(this->mContainer->mUnitClass->mLoadRange + (double)theDistance));
}

// FUNCTION: EXE 0x627340
// Has the transport's class pick the meeting tiles for the transport and the units (the base transport rule when
// there are no units)
void EEUGLoadTransport::ComputeRendezvous()
{
    std::vector<EEComplexUnit*>& theUnits = this->mUnits.mItems;
    EEUCTransport* theClass = dynamic_cast<EEUCTransport*>(this->mContainer->GetUnitClass());
    if (this->mUnits.GetSize() > 0)
        theClass->FindRendezvousTiles(&theUnits, this->mContainer, &this->mContainerTile, &this->mUnitTile);
    else
        theClass->EEUCTransport::FindRendezvousTiles(&theUnits, this->mContainer, &this->mContainerTile, &this->mUnitTile);
}

// FUNCTION: EXE 0x6273a2
// Returns a copy of the goal for the same transport, units and tiles
EEUnitGoal* EEUGLoadTransport::Clone()
{
    return new EEUGLoadTransport(this->mContainer, this->mUnits, this->mContainerTile, this->mUnitTile, this->mMinimumDistanceSquared, this->GetB1C());
}

// FUNCTION: EXE 0x6273e2
// Writes the load-container base to the out-of-synch dump
void EEUGLoadTransport::DumpOutOfSynch(FILE* inFile)
{
    EEUGLoadContainer::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x6273ee
// Returns the load-container base's checksum
long EEUGLoadTransport::GetOutOfSynchChecksum()
{
    return EEUGLoadContainer::GetOutOfSynchChecksum();
}

// FUNCTION: EXE 0x6273f3
// Appends "Load Transport" to the one-line debug text
void EEUGLoadTransport::GetDebugLine(UWideString& ioText)
{
    ioText += "Load Transport";
}

// FUNCTION: EXE 0x627405
// Appends "Load Transport. Part of " and the load-container base's text to the debug text
void EEUGLoadTransport::GetDebugText(UWideString& ioText)
{
    ioText += "Load Transport. Part of ";
    EEUGLoadContainer::GetDebugText(ioText);
}

// FUNCTION: EXE 0x627426
// Returns whether the transport carries the unit
bool EEUGLoadTransport::CanContain(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mContainer != NULL)
        theResult = this->mContainer->HasCargo(inUnit);
    return theResult;
}

// FUNCTION: EXE 0x62743b
// Returns the total weight of the units still to load
long EEUGLoadTransport::GetRemainingLoad()
{
    long theResult = 0;
    for (std::vector<EEComplexUnit*>::iterator theIterator = this->mUnits.mItems.begin(); theIterator != this->mUnits.mItems.end();
         ++theIterator)
        theResult += (*theIterator)->mUnitClass->mWeight;
    return theResult;
}

// FUNCTION: EXE 0x62745c
// Reads the load-container base from the saved game; returns its read error
long EEUGLoadTransport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return this->LoadContainer(inMajorVersion, inMinorVersion, inFile, EEUGLoadTransport::sData);
}

// FUNCTION: EXE 0x627475
// Loads one of the goal's units into the transport (it leaves the transport's own load goal first); plays the
// loading sound and returns true, or idles the unit when the transport refuses it (result 7)
bool EEUGLoadTransport::LoadUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEComplexUnit* theContainer = this->mContainer;
    if (inUnit != NULL && theContainer != NULL) {
        std::vector<EEComplexUnit*>::iterator theIterator;
        if (this->mUnits.Contains(inUnit, theIterator)) {
            EEUGLoadTransport* theGoal = EEGoalCast<EEUGLoadTransport>(this->mContainer->GetGoal());
            if (theGoal != NULL)
                theGoal->RemoveUnit(inUnit);
            if (this->mContainer->LoadCargo(inUnit, 0, 0) == 7) {
                inUnit->StartAction(kActionIdle);
                inUnit->mUnitClass->ActionChanged(inUnit);
            } else {
                theResult = true;
                EESoundObject* theSound = EESoundObject::Get(kSoundTransportLoad);
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

// FUNCTION: EXE 0x62755b
// Writes the load-container base to the saved game; returns its write error
long EEUGLoadTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return this->SaveContainer(inMajorVersion, inMinorVersion, outFile, EEUGLoadTransport::sData);
}

// FUNCTION: EXE 0x627574
// Answers the load-transport triggers (goal is a load, transport alive and able to take the unit, trigger 0x2ee7:
// transport alive and carrying the unit); others go to the load-container base
bool EEUGLoadTransport::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    EEUCTransport* theClass = NULL;
    if (this->mContainer != NULL && this->mContainer->mHitPoints > 0)
        theClass = dynamic_cast<EEUCTransport*>(this->mContainer->GetUnitClass());
    switch (inEvent) {
    case kTriggerGoalIsLoadContainer:
        return this->mKind == kGoalLoadTransport ? true : false;
    case kTriggerContainerInRange:
        return inUnit != this->mContainer && this->mContainer->mHitPoints > 0 && this->IsLoadable(inUnit)
            && theClass->CanLoad(this->mContainer, inUnit) ? true : false;
    case 0x2ee7:
        return this->mContainer->mHitPoints > 0 && this->CanContain(inUnit) ? true : false;
    }
    return EEUGLoadContainer::EvaluateCondition(inUnit, inEvent);
}

