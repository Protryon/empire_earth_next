// C:\EEX\Empire Earth\Units\EEUGPatrol.cpp (file name inferred: no __FILE__; the key "EEUGPatrol-Data"). Patrolling
// between the points of a patrol group: 0x227de8..0x2283bc. Unit flags: the exe's module set. Data: the key 0x5635f8;
// the vtable 0x447920.

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEPPatrolGroup.h"

const UString EEUGPatrol::sData("EEUGPatrol-Data");
// SYNTHETIC: EXE 0x627de8   (wrapper)
// SYNTHETIC: EXE 0x627df2   (ctor)
// SYNTHETIC: EXE 0x627e03   (registrar)
// SYNTHETIC: EXE 0x627e0f   (dtor)

// FUNCTION: EXE 0x627e2b
// Creates a patrol goal with a new empty patrol group and no current point, going forward
EEUGPatrol::EEUGPatrol(bool inArg)
    : EEUnitGoal(kGoalPatrol, inArg)
{
    this->mPatrolGroup = new EEPPatrolGroup;
    this->mTarget = NULL;
    this->mIsGoingForward = true;
}

// FUNCTION: EXE 0x627e67
// Returns get-next-patrol-point as the first action
long EEUGPatrol::GetStartAction()
{
    return kActionGetNextPatrolPoint;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x627e6d

// FUNCTION: EXE 0x627e89
// Creates a patrol goal along a patrol group starting at a given point, referencing both, going forward
EEUGPatrol::EEUGPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg)
    : EEUnitGoal(kGoalPatrol, inArg)
{
    this->mPatrolGroup = inGroup;
    if (inGroup != NULL)
        inGroup->Reference();
    this->mTarget = inTarget;
    if (inTarget != NULL)
        inTarget->Reference();
    this->mIsGoingForward = true;
}

// FUNCTION: EXE 0x627ec3
// Destroys the goal, releasing its patrol group and current point
EEUGPatrol::~EEUGPatrol()
{
    if (this->mPatrolGroup != NULL)
        this->mPatrolGroup->Dereference();
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
}

// FUNCTION: EXE 0x627eed
// Writes the patrol group id, current point's unit id and direction to the out-of-synch dump, then the base goal's
void EEUGPatrol::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Patrol Group ID is %d\n", this->mPatrolGroup->mID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Current Patrol Point #%08x\n",
            this->mTarget != NULL ? this->mTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Going Forward Flag is %d\n", this->mIsGoingForward == true);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x627f48
// Returns the base goal's checksum plus the patrol group id, current point's unit id and direction flag
long EEUGPatrol::GetOutOfSynchChecksum()
{
    long theChecksum = this->mPatrolGroup->mID;
    theChecksum += (this->mTarget != NULL ? this->mTarget->mUnitID : 0) + (this->mIsGoingForward == true);
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x627f73
// Returns a copy of the goal on the same patrol group, starting at the point before the current one
EEUnitGoal* EEUGPatrol::Clone()
{
    EEUnitGoal* theGoal;
    if (this->mPatrolGroup != NULL)
        theGoal = new EEUGPatrol(this->mPatrolGroup, this->mPatrolGroup->GetPrevious(this->mTarget), this->GetB1C());
    else
        theGoal = new EEUGPatrol(this->mPatrolGroup, 0, this->GetB1C());
    return theGoal;
}

// FUNCTION: EXE 0x627fd2
// Appends "pat" and the number of patrol points to the one-line debug text
void EEUGPatrol::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    if (this->mPatrolGroup != NULL)
        sprintf(theText, "pat %d:", this->mPatrolGroup->GetNumPoints());
    else
        sprintf(theText, "pat 0:");
    ioText += theText;
}

// FUNCTION: EXE 0x628031
// Returns the number of points in a patrol group (out-of-line copy of the inline GetNumPoints in
// ee/EEPPatrolGroup.h)

// FUNCTION: EXE 0x628044
// Appends "Patrol" and the point count to the debug text; the point list starts after the last point, so it
// never lists any point or closes
void EEUGPatrol::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "Patrol\nPatrol Points: %d", this->mPatrolGroup != NULL ? this->mPatrolGroup->GetNumPoints() : 0);
    ioText += theText;
    if (this->mPatrolGroup == NULL)
        return;
    sprintf(theText, "Patrol Points: ( ");
    ioText += theText;
    EEComplexUnit* theLast = this->mPatrolGroup->GetLast();
    if (theLast == NULL)
        return;
    EEComplexUnit* thePoint = this->mPatrolGroup->GetNext(theLast);
    while (thePoint != NULL && thePoint != theLast) {
        sprintf(theText, "{%d,%d} ", thePoint->mTileX, thePoint->mTileY);
        ioText += theText;
        thePoint = this->mPatrolGroup->GetNext(thePoint);
    }
    sprintf(theText, " )\n");
    ioText += theText;
}

// FUNCTION: EXE 0x62812a
// Returns the position of the current patrol point
void EEUGPatrol::GetGoalLocation(GE3DPoint& outLocation)
{
    this->mTarget->GetPosition(outLocation);
}

// FUNCTION: EXE 0x628139
// On get-next-patrol-point, moves on to the next point in the current direction, turning back at either end, and
// starts the unit moving; stays put on a one-point patrol already reached
void EEUGPatrol::Step(EEComplexUnit* inUnit)
{
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionGetNextPatrolPoint)
        return;
    // test for a missing patrol group first: a loaded goal can have a current point but no group
    if (this->mPatrolGroup == NULL)
        return;
    if (this->mTarget != NULL && this->mPatrolGroup->GetNumPoints() == 1 && inUnit->mTileX == this->mTarget->mTileX
        && inUnit->mTileY == this->mTarget->mTileY)
        return;
    EEComplexUnit* theNext;
    if (this->mIsGoingForward) {
        theNext = this->mPatrolGroup->GetNext(this->mTarget);
        if (theNext == NULL) {
            theNext = this->mPatrolGroup->GetPrevious(this->mTarget);
            this->mIsGoingForward = false;
        }
    } else {
        theNext = this->mPatrolGroup->GetPrevious(this->mTarget);
        if (theNext == NULL) {
            theNext = this->mPatrolGroup->GetNext(this->mTarget);
            this->mIsGoingForward = true;
        }
    }
    if (theNext == NULL)
        return;
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = theNext;
    theNext->Reference();
    this->SetPathAttempts(0);
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x628208
// Reads the base goal, the patrol group id and current point's unit id (resolved by RepairPointers) and the
// direction; releases the goal's own patrol group first; returns the first read error
long EEUGPatrol::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    unsigned long theID;
    theResult = inFile.Read(theID);
    if (theResult != 0)
        return theResult;
    this->SetPatrolGroup(NULL);
    this->mPatrolGroup = reinterpret_cast<EEPPatrolGroup*>(theID);
    theResult = inFile.Read(theID);
    if (theResult == 0) {
        this->mTarget = reinterpret_cast<EEComplexUnit*>(theID);
        theResult = inFile.Read(this->mIsGoingForward);
    }
    return theResult;
}

// FUNCTION: EXE 0x628265
// Turns the loaded ids back into the current point and the owner's patrol group, referencing both; returns 3 when
// the point or the group is gone
long EEUGPatrol::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 0;
    if (reinterpret_cast<unsigned long>(this->mTarget) != -1) {
        this->mTarget = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget)));
        if (this->mTarget != NULL) {
            this->mTarget->Reference();
            if (reinterpret_cast<unsigned long>(this->mPatrolGroup) != -1) {
                this->mPatrolGroup = this->mTarget->GetPlayer()->GetPatrolGroup(reinterpret_cast<unsigned long>(this->mPatrolGroup));
                if (this->mPatrolGroup != NULL)
                    this->mPatrolGroup->Reference();
                else {
                    theResult = 3;
                    this->mPatrolGroup = NULL;
                }
            } else
                this->mPatrolGroup = NULL;
        } else {
            theResult = 3;
            this->mPatrolGroup = NULL;
        }
    } else {
        this->mTarget = NULL;
        theResult = 3;
        this->mPatrolGroup = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x6282d9
// Makes the patrol point nearest the unit the current one and starts the unit moving (nothing when there is none)
void EEUGPatrol::Start(EEComplexUnit* inUnit)
{
    if (this->mTarget != NULL) {
        this->mTarget->Dereference();
        this->mTarget = NULL;
    }
    EEComplexUnit* theNearest = this->mPatrolGroup->GetNearest(inUnit->GetTileX(), inUnit->GetTileY());
    if (theNearest == NULL)
        return;
    this->mTarget = theNearest;
    theNearest->Reference();
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x628325
// Writes the base goal, patrol group id (-1 when empty), current point's unit id (-1 when invalid) and direction;
// returns the first write error
long EEUGPatrol::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    unsigned long theID;
    if (this->mPatrolGroup != NULL && this->mPatrolGroup->GetNumPoints() != 0)
        theID = this->mPatrolGroup->mID;
    else
        theID = -1;
    theResult = outFile.Write(theID);
    if (theResult != 0)
        return theResult;
    unsigned long theUnitID;
    if (this->mTarget != NULL && this->mTarget->mIsValid)
        theUnitID = this->mTarget->mUnitID;
    else
        theUnitID = -1;
    theResult = outFile.Write(theUnitID);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsGoingForward);
    return theResult;
}

// FUNCTION: EXE 0x6283ac
// Leaves every trigger to the common goal
bool EEUGPatrol::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// ICF-FOLDED: EXE 0x563659
// Replaces the goal's patrol group, moving the reference
// EEUGPatrol::SetPatrolGroup is defined at its address in EEPlayer.cpp (a copy here duplicated the definition in
// the full link)

// The slots ICF-folded into other units' identical bodies:
// ICF-FOLDED: EXE 0x6907a3
// Does nothing: the goal holds no units
void EEUGPatrol::ReplaceUnit(EEComplexUnit*, EEComplexUnit*, EEComplexUnit*)
{
}

// ICF-FOLDED: EXE 0x620a18
// Returns true: any unit may take the goal
bool EEUGPatrol::Suits(EEUnit* inUnit)
{
    return true;
}
