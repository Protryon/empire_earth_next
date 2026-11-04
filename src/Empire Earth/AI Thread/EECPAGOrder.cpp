// C:\EEX\Empire Earth\AI Thread\EECPAGOrder.cpp (file name inferred: no __FILE__; RTTI class EECPAGOrder,
// vtable 0x43a6ac). An attack group order: 0x412b1..0x4162f (EEComputerPlayer.cpp ends at 0x412b1, EECPAGTask's ctor
// starts at 0x4162f). The members in definition order; ??_G after the default ctor, EECPILocation's inline ??_G after
// the dtor (its first user).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFile.h"

#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPAGOrder.h"

// FUNCTION: EXE 0x4412b1
// Creates an inactive attack group order of no type, with no sub groups and no unit
EECPAGOrder::EECPAGOrder()
    : mSubGroup(NULL), mOtherSubGroup(NULL), mUnitID(-1), mL28(0), mType(kAGOrderNone), mSplitKind(0), mState(0), mIsActive(false)
{
}

// SYNTHETIC: EXE 0x4412e1
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4412fd
// Creates a copy of an order, taking its own references on both sub groups
EECPAGOrder::EECPAGOrder(const EECPAGOrder& inOrder)
{
    this->mLocation = inOrder.mLocation;
    this->mSplitKind = inOrder.mSplitKind;
    this->mUnitID = inOrder.mUnitID;
    this->mL28 = inOrder.mL28;
    this->mType = inOrder.mType;
    this->mState = inOrder.mState;
    this->mIsActive = inOrder.mIsActive;
    this->mSubGroup = NULL;
    this->mOtherSubGroup = NULL;
    this->SetSubGroup(inOrder.mSubGroup);
    this->SetOtherSubGroup(inOrder.mOtherSubGroup);
}

// FUNCTION: EXE 0x441369
// Destroys the order, releasing its references on both sub groups
EECPAGOrder::~EECPAGOrder()
{
    this->ReleaseSubGroup();
    this->ReleaseOtherSubGroup();
}

// SYNTHETIC: EXE 0x441387
// (EECPILocation's scalar deleting destructor, inline in ee/EECPILocation.h; this unit uses its vtable first)

// FUNCTION: EXE 0x4413a4
// Reads the order from a saved game (sub groups as ids until Restore); returns the first read error
long EECPAGOrder::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mLocation.Load(inMajorVersion, inMinorVersion, inFile);
    unsigned long theID;
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mSubGroup = reinterpret_cast<EECPAttackSubGroup*>(theID);
    }
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mOtherSubGroup = reinterpret_cast<EECPAttackSubGroup*>(theID);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUnitID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mL28);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsActive);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSplitKind);
    return theResult;
}

// FUNCTION: EXE 0x44144b
// Releases the reference on the second sub group and clears it
void EECPAGOrder::ReleaseOtherSubGroup()
{
    if (this->mOtherSubGroup)
        this->mOtherSubGroup->Dereference();
    this->mOtherSubGroup = NULL;
}

// FUNCTION: EXE 0x441461
// Releases the reference on the order's sub group and clears it
void EECPAGOrder::ReleaseSubGroup()
{
    if (this->mSubGroup)
        this->mSubGroup->Dereference();
    this->mSubGroup = NULL;
}

// FUNCTION: EXE 0x441477
// Turns the loaded sub group ids back into the attack group's sub groups and references them; returns OK
long EECPAGOrder::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion, EECPAttackGroup* inGroup)
{
    if (this->mSubGroup) {
        this->mSubGroup = inGroup->mCommander->GetSubGroup(USavedID(this->mSubGroup));
        if (this->mSubGroup)
            this->mSubGroup->Reference();
    } else {
        this->mSubGroup = NULL;
    }
    if (this->mOtherSubGroup) {
        this->mOtherSubGroup = inGroup->mCommander->GetSubGroup(USavedID(this->mOtherSubGroup));
        if (this->mOtherSubGroup)
            this->mOtherSubGroup->Reference();
    } else {
        this->mOtherSubGroup = NULL;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x4414c6
// Writes the order to the saved game (sub groups by id, 0 for none); returns the first write error
long EECPAGOrder::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mLocation.Save(inMajorVersion, inMinorVersion, outFile);
    unsigned long theID = this->mSubGroup ? this->mSubGroup->GetID() : 0;
    if (theResult == kErrorOK)
        theResult = outFile.Write(theID);
    theID = this->mOtherSubGroup ? this->mOtherSubGroup->GetID() : 0;
    if (theResult == kErrorOK)
        theResult = outFile.Write(theID);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitID);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mL28);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mState);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsActive);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSplitKind);
    return theResult;
}

// FUNCTION: EXE 0x44159a
// Sets the order's second sub group, moving the reference from the old one
void EECPAGOrder::SetOtherSubGroup(EECPAttackSubGroup* inSubGroup)
{
    if (this->mOtherSubGroup == inSubGroup)
        return;
    this->ReleaseOtherSubGroup();
    this->mOtherSubGroup = inSubGroup;
    if (this->mOtherSubGroup)
        this->mOtherSubGroup->Reference();
}

// FUNCTION: EXE 0x4415bb
// Sets the order's sub group, moving the reference from the old one
void EECPAGOrder::SetSubGroup(EECPAttackSubGroup* inSubGroup)
{
    if (inSubGroup == this->mSubGroup)
        return;
    this->ReleaseSubGroup();
    this->mSubGroup = inSubGroup;
    if (this->mSubGroup)
        this->mSubGroup->Reference();
}

// FUNCTION: EXE 0x4415dc
// Copies another order, moving the sub group references to its sub groups
EECPAGOrder& EECPAGOrder::operator=(const EECPAGOrder& inOrder)
{
    this->mLocation = inOrder.mLocation;
    this->mUnitID = inOrder.mUnitID;
    this->mL28 = inOrder.mL28;
    this->mType = inOrder.mType;
    this->mState = inOrder.mState;
    this->mIsActive = inOrder.mIsActive;
    this->mSplitKind = inOrder.mSplitKind;
    this->SetSubGroup(inOrder.mSubGroup);
    this->SetOtherSubGroup(inOrder.mOtherSubGroup);
    return *this;
}
