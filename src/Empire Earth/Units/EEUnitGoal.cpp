// C:\EEX\Empire Earth\Units\EEUnitGoal.cpp (file name inferred: no __FILE__; the class and its key "EEUnitGoal-Data").
// The unit goal base class: 0x23c332..0x23c83a. Unit flags: the exe's module set. Data: the key 0x564470 (guard byte
// 0x564468), the goal counter 0x564480 right after it; vtable 0x43cc64. Its inline dtor's ??_G (0x2276c4) is emitted
// by EEUGLocation.cpp, the trivial slots are ICF-folded elsewhere.

#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"

const UString EEUnitGoal::sData("EEUnitGoal-Data");
// SYNTHETIC: EXE 0x63c332   (wrapper)
// SYNTHETIC: EXE 0x63c33c   (ctor)
// SYNTHETIC: EXE 0x63c34d   (registrar)
// SYNTHETIC: EXE 0x63c359   (dtor)
unsigned long sGoalCounter;

// FUNCTION: EXE 0x63c375
// Creates an active, idle goal of a kind with the next goal id, stamped with the game time
EEUnitGoal::EEUnitGoal(unsigned char inKind, bool inArg)
    : mKind(inKind), mB1C(inArg)
{
    this->mAction = kActionIdle;
    this->mB1D = false;
    this->mIsActive = true;
    this->mStartTime = 0;
    this->mCreationTime = gPhysics->mGameTime;
    this->mID = ++sGoalCounter;
    this->mPathAttempts = 0;
}

// FUNCTION: EXE 0x63c3c2
// Returns the goal's location, passed through the unit's active group (TellGroup) when it is in one
void EEUnitGoal::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    this->GetGoalLocation(outLocation);
    if (inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x63c3e6
// Creates an empty goal of a kind for loading, giving the drawn goal id back (NULL for an unknown kind)
EEUnitGoal* EEUnitGoal::CreateGoal(unsigned char inKind)
{
    EEUnitGoal* theGoal = NULL;
    switch (inKind) {
    case kGoalAttackGround:
        theGoal = EEUGRust::NewAttackGround(GE3DPoint(), false);
        break;
    case kGoalAttackUnit:
        theGoal = EEUGRust::NewAttackUnitEmpty(false);
        break;
    case kGoalBuildRepair:
        theGoal = EEUGRust::NewBuildRepair(false);
        break;
    case kGoalCastCalamity:
        theGoal = EEUGRust::NewCastCalamity(false);
        break;
    case kGoalExplore:
        theGoal = EEUGRust::NewExplore(false);
        break;
    case kGoalGather:
        theGoal = EEUGRust::NewGather(false);
        break;
    case kGoalLoadGarrison:
        theGoal = EEUGRust::NewLoad(kGoalLoadGarrison, false);
        break;
    case kGoalLoadTransport:
        theGoal = EEUGRust::NewLoad(kGoalLoadTransport, false);
        break;
    case kGoalLocation:
        theGoal = EEUGRust::NewLocation(false, false, false, false);
        break;
    case kGoalPatrol:
        theGoal = EEUGRust::NewPatrol(false);
        break;
    case kGoalUnloadTransport:
        theGoal = EEUGRust::NewUnloadTransport(false);
        break;
    case kGoalHealUnit:
        theGoal = EEUGRust::NewHealUnit(false);
        break;
    }
    if (theGoal != NULL)
        theGoal->mID = --sGoalCounter;
    return theGoal;
}

// Compiler-generated for this unit (the inline (bool) ctors, first emitted after their first user CreateGoal; the
// original's order is the class headers' alphabetical one, ours the classes' order in ee/EEUnitGoal.h):
// TEMPLATE: EXE 0x63c589
// TEMPLATE: EXE 0x63c5b2
// TEMPLATE: EXE 0x63c5e3
// TEMPLATE: EXE 0x63c63f
// TEMPLATE: EXE 0x63c685

// FUNCTION: EXE 0x63c6cd
// Returns how many times the goal may try to find a path (3)
long EEUnitGoal::GetMaxPathAttempts()
{
    return 3;
}

// FUNCTION: EXE 0x63c6d1
// Reads the goal's action, flags, id and (by version) start and creation times from a saved game; returns the first
// read error
long EEUnitGoal::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // the version gates are written out per branch (both copies are in the object)
    long theResult = inFile.Read(this->mAction);
    if (inMajorVersion <= 0 && inMinorVersion <= 14)
        return theResult;
    if (inMajorVersion > 0 || inMinorVersion >= 135) {
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mB1C);
        if (inMinorVersion >= 100 && theResult == kErrorOK)
            theResult = inFile.Read(this->mID);
        else
            this->mID = 0;
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mB1D);
        if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 157))
            theResult = inFile.Read(this->mStartTime);
        if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 164))
            theResult = inFile.Read(this->mCreationTime);
    } else {
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mB1C);
        if (inMinorVersion >= 100 && theResult == kErrorOK)
            theResult = inFile.Read(this->mID);
        else
            this->mID = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x63c79e
// Starts the goal's start action on the unit and tells the unit's class its action changed
void EEUnitGoal::Start(EEComplexUnit* inUnit)
{
    inUnit->StartAction(this->GetStartAction());
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x63c7bd
// Writes the goal's action, flags, id, start and creation times to the saved game; returns the first write error
long EEUnitGoal::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = outFile.Write(this->mAction);
    if (theResult == 0)
        theResult = outFile.Write(this->mB1C);
    if (theResult == 0)
        theResult = outFile.Write(this->mID);
    if (theResult == 0)
        theResult = outFile.Write(this->mB1D);
    if (theResult == 0)
        theResult = outFile.Write(this->mStartTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mCreationTime);
    return theResult;
}

// FUNCTION: EXE 0x63c82c
// Returns whether a trigger event is met: by default only "unit facing not required"
bool EEUnitGoal::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return inEvent == kTriggerUnitFacingNotRequired ? true : false;
}

// The trivial slots (ICF-folded into other units' identical bodies):
// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::AddCreatedUnit(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x557269
void EEUnitGoal::DumpOutOfSynch(FILE* inFile)
{
}

// ICF-FOLDED: EXE 0x4fca98
long EEUnitGoal::GetOutOfSynchChecksum()
{
    return 0;
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::GetDebugLine(UWideString& ioText)
{
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::GetDebugText(UWideString& ioText)
{
}

// ICF-FOLDED: EXE 0x4ac9f3
bool EEUnitGoal::IsLoadGoal()
{
    return false;
}

// ICF-FOLDED: EXE 0x5ad67e
long EEUnitGoal::GetStartAction()
{
    return kActionInvalid;
}

// ICF-FOLDED: EXE 0x4adcf5
unsigned long EEUnitGoal::GetCreationTime()
{
    return this->mCreationTime;
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::SetOwner(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::Step(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::PathTaken(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::Abandon(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x67cf38
long EEUnitGoal::RepairPointers(unsigned long, unsigned long)
{
    return 0;
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::RemovedFromQueue(EEComplexUnit* inUnit)
{
}

// ICF-FOLDED: EXE 0x556dd8
void EEUnitGoal::Work(EEComplexUnit* inUnit)
{
}
