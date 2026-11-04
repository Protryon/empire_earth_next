// C:\EEX\Empire Earth\Units\EEUGHealUnit.cpp (file name inferred: no __FILE__; the key "EEUGHealUnit-StringID").
// Healing a unit: 0x22528d..0x22585a. Unit flags: the exe's module set. Data: the key; the vtable 0x447630.

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

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEAreaUtilities.h"

const UString EEUGHealUnit::sData("EEUGHealUnit-StringID");
// SYNTHETIC: EXE 0x62528d   (wrapper)
// SYNTHETIC: EXE 0x625297   (ctor)
// SYNTHETIC: EXE 0x6252a8   (registrar)
// SYNTHETIC: EXE 0x6252b4   (dtor)

// FUNCTION: EXE 0x6252d0
// Creates an empty heal goal with no target
EEUGHealUnit::EEUGHealUnit(bool inArg)
    : EEUGLocation(inArg, false, false, false), mTarget(NULL), mElapsedTime(0)
{
    this->mKind = kGoalHealUnit;
}

// FUNCTION: EXE 0x6252f9
// Returns turn-to-face-healee as the first action
long EEUGHealUnit::GetStartAction()
{
    return kActionTurnToFaceHealee;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x6252ff

// FUNCTION: EXE 0x62531b
// Creates a heal goal on a target unit, located at its position, referencing it
EEUGHealUnit::EEUGHealUnit(EEComplexUnit* inTarget, bool inArg)
    : EEUGLocation(&inTarget->mPhysicalModel.GetLocation().GetPosition(), inArg, false, false, 0.0f, false, false),
      mTarget(inTarget)
{
    this->mKind = kGoalHealUnit;
    inTarget->Reference();
    this->mElapsedTime = 0;
}

// FUNCTION: EXE 0x625378
// Destroys the goal: ends the target's heal effect and releases it
EEUGHealUnit::~EEUGHealUnit()
{
    if (this->mTarget != NULL) {
        this->mTarget->ReleaseHealEffect();
        this->mTarget->Dereference();
        this->mTarget = NULL;
        this->mElapsedTime = 0;
    }
}

// FUNCTION: EXE 0x6253a5
// Returns a copy of the goal for the same target
EEUnitGoal* EEUGHealUnit::Clone()
{
    return new EEUGHealUnit(this->mTarget, this->GetB1C());
}

// FUNCTION: EXE 0x6253d2
// Writes the healee's id and the elapsed heal time to the out-of-synch dump, then the base goal's
void EEUGHealUnit::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGHealUnit - Healee unit id is #%08x\n",
            this->mTarget != NULL ? this->mTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGHealUnit - Elasped Time in Milliseconds %d\n", this->mElapsedTime);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x625415
// Returns the base goal's checksum plus the healee's id and the elapsed heal time
long EEUGHealUnit::GetOutOfSynchChecksum()
{
    long theChecksum = (this->mTarget != NULL ? this->mTarget->mUnitID : 0) + this->mElapsedTime;
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x625432
// Appends "Heal" and the target's name, tile and health to the debug text
void EEUGHealUnit::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    ioText += "Heal\n";
    if (this->mTarget == NULL)
        return;
    long theHitPoints = this->mTarget->mHitPoints;
    long theMaxHitPoints = this->mTarget->mStats->mMaxHitPoints;
    sprintf(theText, "Target Unit: %s at (%d,%d)\nTarget Health: %d/%d (%.2f%%)\n", (const char*)this->mTarget->mUnitClass->mName,
            this->mTarget->mTileX, this->mTarget->mTileY, theHitPoints, theMaxHitPoints,
            100.0f * theHitPoints / theMaxHitPoints);   // a percentage (the raw fraction used to be printed)
    ioText += theText;
}

// FUNCTION: EXE 0x6254b1
// Returns the target's position
void EEUGHealUnit::GetGoalLocation(GE3DPoint& outLocation)
{
    this->mTarget->GetPosition(outLocation);
}

// FUNCTION: EXE 0x6254c0
// Returns whether the target is within the healer class's heal range of the healer
bool EEUGHealUnit::IsInRange(EEComplexUnit* inHealer, EEComplexUnit* inTarget)
{
    GE3DPoint theTargetPosition;
    GE3DPoint theHealerPosition;
    float theRange = inHealer->mUnitClass->mHealRange;
    inHealer->GetPosition(theHealerPosition);
    inTarget->GetPosition(theTargetPosition);
    return theHealerPosition.DistanceToSquared(theTargetPosition) <= UMSquare(theRange);
}

// FUNCTION: EXE 0x625534
// On the turn-to-face action, plays the turning animation (0xd) until the healer faces the target and drops its path
void EEUGHealUnit::Step(EEComplexUnit* inUnit)
{
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionTurnToFaceHealee)
        return;
    inUnit->SetCurrentPhysicsAction(1);
    if (!inUnit->GetUnitClass()->FacesTarget(inUnit, this->mTarget, false) && inUnit->mAnimation != 0xd)
        inUnit->SetAnimation(0xd, -1, 0);
    inUnit->DropPath();
}

// FUNCTION: EXE 0x625595
// Reads a has-target flag, then the target id and elapsed heal time (no base goal data); returns the first read error
long EEUGHealUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theTargetID = -1;
    bool theHasTarget = false;
    long theResult = inFile.Read(theHasTarget);
    if (theResult != kErrorOK || !theHasTarget)
        return theResult;
    theResult = inFile.Read(theTargetID);
    if (theResult == kErrorOK) {
        this->mTarget = reinterpret_cast<EEComplexUnit*>(theTargetID);
        theResult = inFile.Read(this->mElapsedTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x6255e3
// Turns the loaded target id back into the target, referencing it; returns 3 when the target is gone
long EEUGHealUnit::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 0;
    if (this->mTarget != NULL) {
        theResult = 3;
        RTSUnit* theUnit = ::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget));
        if (theUnit != NULL) {
            this->mTarget = static_cast<EEUnit*>(theUnit)->mComplexUnit;
            if (this->mTarget != NULL) {
                this->mTarget->Reference();
                theResult = 0;
            }
        } else
            this->mTarget = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x62561e
// Writes a has-target flag, then the target id and elapsed heal time (no base goal data); returns the first write error
long EEUGHealUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    bool theHasTarget = false;
    if (this->mTarget != NULL)
        theHasTarget = true;
    long theResult = outFile.Write(theHasTarget);
    if (theResult != 0 || !theHasTarget)
        return theResult;
    unsigned long theTargetID = this->mTarget->mUnitID;
    theResult = outFile.Write(theTargetID);
    if (theResult == 0)
        theResult = outFile.Write(this->mElapsedTime);
    return theResult;
}

// FUNCTION: EXE 0x62567a
// Sets the target, moving the reference, and restarts the heal timer
void EEUGHealUnit::SetTarget(EEComplexUnit* inTarget)
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = inTarget;
    if (inTarget != NULL)
        inTarget->Reference();
    this->mElapsedTime = 0;
}

// FUNCTION: EXE 0x62569f
// Answers the heal triggers (healee out of a human player's sight, destroyed, faced, in range, repairable and below the
// heal threshold for a non-enemy healer); others go to the location goal
bool EEUGHealUnit::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    switch (inEvent) {
    case kTriggerHealeeNoLongerVisible:
        if (!inUnit->mPlayer->IsHuman() || EEIsSeenBy(this->mTarget, inUnit->GetPlayer(), true))
            return false;
        return true;
    case kTriggerHealeeDestroyed:
        return this->mTarget->mHitPoints <= 0 ? true : false;
    case kTriggerFacingHealee:
        return inUnit->mUnitClass->FacesTarget(inUnit, this->mTarget, false);
    case kTriggerGoalIsHeal:
        return true;
    case kTriggerHealeeInHealingRange:
        return this->IsInRange(inUnit, this->mTarget);
    case kTriggerTargetNeedsHealing: {
            long theHitPoints = this->mTarget->mHitPoints;
            long theMaxHitPoints = this->mTarget->mStats->mMaxHitPoints;
            if (this->mTarget->IsRepairable() && (float)theHitPoints / theMaxHitPoints <= inUnit->mUnitClass->GetHealThreshold()
                && (inUnit->mPlayer == this->mTarget->mPlayer || this->mTarget->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0))
                return true;
            return false;
        }
    }
    return EEUGLocation::EvaluateCondition(inUnit, inEvent);
}

// FUNCTION: EXE 0x625789
// While healing a living, non-enemy target (not in cinematics), adds the healer's healing points each full second, up
// to the target's maximum hit points
void EEUGHealUnit::Work(EEComplexUnit* inUnit)
{
    unsigned long theElapsed = gPhysics->mTickTimeMs;
    if (theElapsed == 0)
        theElapsed = 1;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionHealUnit || this->mTarget == NULL || ::gWorld.mInCinematic
        || !(this->mTarget->mHitPoints > 0)
        || (inUnit->mPlayer != this->mTarget->mPlayer && this->mTarget->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] != 0))
        return;
    this->mElapsedTime += theElapsed;
    if (this->mElapsedTime > 1000) {
        this->mTarget->SetHitPoints(min(inUnit->mStats->mHealingPoints + this->mTarget->mHitPoints, this->mTarget->mStats->mMaxHitPoints));
        this->mElapsedTime -= 1000;
    }
}

// FUNCTION: EXE 0x625828
// Retargets the goal to the new unit when the old unit is its target
void EEUGHealUnit::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (inOld == this->mTarget)
        this->SetTarget(inNew);
}

// FUNCTION: EXE 0x62583d
// Returns whether the unit can heal the goal's target
bool EEUGHealUnit::Suits(EEUnit* inUnit)
{
    bool theResult = false;
    if (EECanHeal(inUnit, this->mTarget))
        theResult = true;
    return theResult;
}

// ICF-FOLDED: EXE 0x557269
// Does nothing: the healer does not run the location goal's start (the original slot holds an empty body)
void EEUGHealUnit::Start(EEComplexUnit* inUnit)
{
}
