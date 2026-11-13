// C:\EEX\Empire Earth\Units\EEUGLocation.cpp (file name inferred: no __FILE__; the key "EEUGLocation-Data"). Going to
// a location: 0x227621..0x227de8. Unit flags: the exe's module set. Data: the key 0x5635e0; kOneDegree 0x447898 and the
// vtable 0x4478a0. EEUnitGoal's inline ??_G (0x2276c4, EEUGAttackGround's folds into it) is first emitted here.

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUnitVectorItem.h"

const UString EEUGLocation::sData("EEUGLocation-Data");
// SYNTHETIC: EXE 0x627621   (wrapper)
// SYNTHETIC: EXE 0x62762b   (ctor)
// SYNTHETIC: EXE 0x62763c   (registrar)
// SYNTHETIC: EXE 0x627648   (dtor)

const float kOneDegree = 0.017453292f;

// FUNCTION: EXE 0x627664
// Creates a goal to move to a point, with the forced-move, face-on-arrival (yaw), auto-waypoint and group path
// settings; no group movement
EEUGLocation::EEUGLocation(float inX, float inY, float inZ, bool inArg4, bool inIsForcedMove, bool inShouldFace, float inYaw,
                           bool inIsAutoWaypoint, unsigned char inGroupPathState)
    : EEUnitGoal(kGoalLocation, inArg4)
{
    this->mLocation.SetX(inX);
    this->mLocation.SetY(inY);
    this->mLocation.SetZ(inZ);
    this->mUseGroupMovement = false;
    this->mIsForcedMove = inIsForcedMove;
    this->mShouldFace = inShouldFace;
    this->mYaw = inYaw;
    this->mIsAutoWaypoint = inIsAutoWaypoint;
    this->mGroupPathState = inGroupPathState;
}

// Compiler-generated: EEUnitGoal's inline ??_G (EEUGAttackGround's and this class's implicit ones fold into it), first
// emitted after this unit's first ctor:
// TEMPLATE: EXE 0x6276c4

// FUNCTION: EXE 0x6276e1
// Creates a goal to move to a map position at height 0, with the forced-move, facing, auto-waypoint and group path
// settings
EEUGLocation::EEUGLocation(const GE2DPoint& inLocation, bool inArg2, bool inIsForcedMove, bool inShouldFace, float inYaw, bool inIsAutoWaypoint,
                           unsigned char inGroupPathState)
    : EEUnitGoal(kGoalLocation, inArg2)
{
    this->mLocation.SetX(inLocation.GetX());
    this->mLocation.SetY(inLocation.GetY());
    this->mLocation.SetZ(0.0f);
    this->mUseGroupMovement = false;
    this->mIsForcedMove = inIsForcedMove;
    this->mShouldFace = inShouldFace;
    this->mYaw = inYaw;
    this->mIsAutoWaypoint = inIsAutoWaypoint;
    this->mGroupPathState = inGroupPathState;
}

// FUNCTION: EXE 0x627742
// Creates a goal to move to a 3D point, with the forced-move, facing, auto-waypoint and group path settings
EEUGLocation::EEUGLocation(const GE3DPoint* inLocation, bool inArg2, bool inIsForcedMove, bool inShouldFace, float inYaw, bool inIsAutoWaypoint,
                           unsigned char inGroupPathState)
    : EEUnitGoal(kGoalLocation, inArg2)
{
    this->mLocation = *inLocation;
    this->mUseGroupMovement = false;
    this->mIsForcedMove = inIsForcedMove;
    this->mShouldFace = inShouldFace;
    this->mYaw = inYaw;
    this->mIsAutoWaypoint = inIsAutoWaypoint;
    this->mGroupPathState = inGroupPathState;
}

// FUNCTION: EXE 0x6277a4
// Creates a move goal with no location yet and no facing, with the forced-move, auto-waypoint and group path settings
EEUGLocation::EEUGLocation(bool inArg, bool inIsForcedMove, bool inIsAutoWaypoint, unsigned char inGroupPathState)
    : EEUnitGoal(kGoalLocation, inArg)
{
    this->mShouldFace = false;
    this->mUseGroupMovement = false;
    this->mYaw = 0.0f;
    this->mIsForcedMove = inIsForcedMove;
    this->mIsAutoWaypoint = inIsAutoWaypoint;
    this->mGroupPathState = inGroupPathState;
}

// FUNCTION: EXE 0x6277ef
// Returns a copy of the goal with the same location and settings
EEUnitGoal* EEUGLocation::Clone()
{
    EEUGLocation* theGoal = new EEUGLocation(&this->mLocation, this->GetB1C(), this->mIsForcedMove, this->mShouldFace, this->mYaw, this->mIsAutoWaypoint, this->mGroupPathState);
    theGoal->mUseGroupMovement = this->mUseGroupMovement;
    return theGoal;
}

// FUNCTION: EXE 0x627842
// Writes the location, yaw, flags and group path state to the out-of-synch dump, then the base goal's
void EEUGLocation::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Location is (%6.6f, %6.6f, %6.6f)\n", this->mLocation.GetX(),
            this->mLocation.GetY(), this->mLocation.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Yaw is %6.6f\n", this->mYaw);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Forced Move Flag is %d\n", this->mIsForcedMove == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Should Face Flag is %d\n", this->mShouldFace == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Is Auto Waypoint Flag is %d\n", this->mIsAutoWaypoint == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Use Group Movement Flag is %d\n", this->mUseGroupMovement == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGLocation - Group Path State is %d\n", this->mGroupPathState);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x6278eb
// Returns the base goal's checksum plus the raw bits of the location and yaw, the flags and the group path state
long EEUGLocation::GetOutOfSynchChecksum()
{
    float theValue = this->mLocation.GetX();
    long theChecksum = *reinterpret_cast<int32_t*>(&theValue);
    theValue = this->mLocation.GetY();
    theChecksum += *reinterpret_cast<int32_t*>(&theValue);
    theValue = this->mLocation.GetZ();
    theChecksum += *reinterpret_cast<int32_t*>(&theValue);
    theValue = this->mYaw;
    theChecksum += *reinterpret_cast<int32_t*>(&theValue) + (this->mIsForcedMove == true) + (this->mShouldFace == true) + (this->mIsAutoWaypoint == true) + (this->mUseGroupMovement == true) + this->mGroupPathState;
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x62794d
// Returns whether the unit already faces the goal's yaw (within one degree); if so, snaps it to that yaw exactly
bool EEUGLocation::FaceYaw(EEComplexUnit* inUnit)
{
    bool theResult;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float theDelta = theYaw - this->GetYaw();
    if (theDelta < -3.1415927f)
        theDelta += 6.2831855f;
    else if (theDelta > 3.1415927f)
        theDelta -= 6.2831855f;
    if (theDelta >= -kOneDegree && theDelta <= kOneDegree)
        theResult = true;
    else
        theResult = false;
    if (theResult) {
        GETransformation theNewTransformation;
        theNewTransformation = theTransformation;
        theNewTransformation.SetOrientationYPR(this->mYaw, 0.0f, 0.0f);
        inUnit->SetTransformation(theNewTransformation, true);
    }
    return theResult;
}

// FUNCTION: EXE 0x627a44
// Appends "loc", the location and the auto-waypoint flag to the one-line debug text
void EEUGLocation::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "loc %.2f,%.2f,%.2f: auto(%d)", this->mLocation.GetX(), this->mLocation.GetY(), this->mLocation.GetZ(), this->mIsAutoWaypoint);
    ioText += theText;
}

// FUNCTION: EXE 0x627aa0
// Appends the location, auto-waypoint flag, yaw, attack-move (not forced) and facing flag to the debug text
void EEUGLocation::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "Location: %.2f,%.2f,%.2f\nAutoWaypoint:%s\nYaw:%.4f\nAttackMove:%s\nShouldFace:%s\n",
            this->mLocation.GetX(), this->mLocation.GetY(), this->mLocation.GetZ(), this->mIsAutoWaypoint ? "true" : "false", this->mYaw,
            this->mIsForcedMove ? "false" : "true", this->mShouldFace ? "true" : "false");
    ioText += theText;
}

// FUNCTION: EXE 0x627b2a
// Reads the base goal, location and the settings each save version has (forced move from 0.15, yaw and facing from
// 0.26, auto waypoint from 0.135, group movement from 0.173); returns the first read error
long EEUGLocation::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // the version gates are written out per branch, like EEUnitGoal::Load's
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (inMajorVersion <= 0 && inMinorVersion <= 14) {
        if (theResult == 0)
            theResult = this->mLocation.Load(inFile);
    } else if (inMajorVersion <= 0 && inMinorVersion <= 25) {
        if (theResult == 0)
            theResult = this->mLocation.Load(inFile);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsForcedMove);
    } else if (inMajorVersion <= 0 && inMinorVersion < 135) {
        if (theResult == 0)
            theResult = this->mLocation.Load(inFile);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsForcedMove);
        if (theResult == 0)
            theResult = inFile.Read(this->mYaw);
        if (theResult == 0)
            theResult = inFile.Read(this->mShouldFace);
    } else if (inMajorVersion <= 0 && inMinorVersion < 173) {
        if (theResult == 0)
            theResult = this->mLocation.Load(inFile);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsForcedMove);
        if (theResult == 0)
            theResult = inFile.Read(this->mYaw);
        if (theResult == 0)
            theResult = inFile.Read(this->mShouldFace);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsAutoWaypoint);
    } else {
        if (theResult == 0)
            theResult = this->mLocation.Load(inFile);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsForcedMove);
        if (theResult == 0)
            theResult = inFile.Read(this->mYaw);
        if (theResult == 0)
            theResult = inFile.Read(this->mShouldFace);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsAutoWaypoint);
        if (theResult == 0)
            theResult = inFile.Read(this->mUseGroupMovement);
    }
    return theResult;
}

// FUNCTION: EXE 0x627c99
// Starts the unit preparing to move; a non-aircraft already at the location turns to face (or idles) instead, and
// an unforced move resets its target search timer
void EEUGLocation::Start(EEComplexUnit* inUnit)
{
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory != kUnitCategoryAirplane && theCategory != kUnitCategoryPlanetSpaceship) {
        const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
        GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        if (thePosition == this->mLocation) {
            if (this->mShouldFace)
                inUnit->StartAction(kActionTurnToUnitFacing);
            else
                inUnit->StartAction(kActionIdle);
            inUnit->mUnitClass->ActionChanged(inUnit);
            return;
        }
        if (!this->mIsForcedMove)
            inUnit->SetTargetSearchTimer(0);
    }
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x627d31
// Writes the base goal, location, forced-move flag, yaw, facing, auto-waypoint and group movement flags to the
// saved game; returns the first write error
long EEUGLocation::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = this->mLocation.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsForcedMove);
    if (theResult == 0)
        theResult = outFile.Write(this->mYaw);
    if (theResult == 0)
        theResult = outFile.Write(this->mShouldFace);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsAutoWaypoint);
    if (theResult == 0)
        theResult = outFile.Write(this->mUseGroupMovement);
    return theResult;
}

// FUNCTION: EXE 0x627da5
// Answers the facing triggers (facing done; facing not needed when the goal does not face or more goals follow);
// others go to the common goal
bool EEUGLocation::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    switch (inEvent) {
    case kTriggerCompletedUnitFacing:
        return this->FaceYaw(inUnit);
    case kTriggerUnitFacingNotRequired:
        return !this->mShouldFace || inUnit->mGoalQueues[0].mGoals.size() > 1 ? true : false;
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// The slots ICF-folded into other units' identical bodies:
// ICF-FOLDED: EXE 0x61d0b4
// Returns the goal's location
void EEUGLocation::GetGoalLocation(GE3DPoint& outLocation)
{
    outLocation = this->mLocation;
}

// ICF-FOLDED: EXE 0x627264
// Returns prepare-to-move as the first action
long EEUGLocation::GetStartAction()
{
    return kActionPrepareToMove;
}

// ICF-FOLDED: EXE 0x6907a3
// Does nothing: the goal holds no units
void EEUGLocation::ReplaceUnit(EEComplexUnit*, EEComplexUnit*, EEComplexUnit*)
{
}

// ICF-FOLDED: EXE 0x620a18
// Returns true: any unit may take the goal
bool EEUGLocation::Suits(EEUnit* inUnit)
{
    return true;
}
