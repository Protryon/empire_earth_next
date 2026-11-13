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
#include "ee/EECombatFormulas.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EECombatGoals.h"
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

// ICF-FOLDED: EXE 0x52c734
// Returns the unit being healed (folded with EECCAreaEffect::GetContagionFunctor in the original: `mov eax,[ecx+0x38]`)
EEComplexUnit* EEUGHealUnit::GetTarget()
{
    return this->mTarget;
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
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGHealUnit - Healee unit id is #%08lx\n",
            this->mTarget != NULL ? this->mTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGHealUnit - Elasped Time in Milliseconds %ld\n", this->mElapsedTime);
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
    sprintf(theText, "Target Unit: %s at (%ld,%ld)\nTarget Health: %ld/%ld (%.2f%%)\n", (const char*)this->mTarget->mUnitClass->mName,
            this->mTarget->mTileX.Get(), this->mTarget->mTileY.Get(), theHitPoints, theMaxHitPoints,
            100.0f * theHitPoints / theMaxHitPoints);   // a percentage (the raw fraction used to be printed)
    ioText += theText;
}

// FUNCTION: EXE 0x6254b1
// Returns the target's position
void EEUGHealUnit::GetGoalLocation(GE3DPoint& outLocation)
{
    this->mTarget->GetPosition(outLocation);
}

// IsInRange 0x6254c0: ee_combat reads.rs heal_in_range (EECombatGoals.cpp EECombatHealInRange)

// FUNCTION: EXE 0x625534
// On the turn-to-face action, plays the turning animation (0xd) until the healer faces the target and drops its path
void EEUGHealUnit::Step(EEComplexUnit* inUnit)
{
    // ee_combat goals.rs step: the plan's operations executed by ee/EECombatGoals.h's host (CBGS)
    EECombatStep(EE_COMBAT_GOAL_HEAL, inUnit, this, this->mTarget);
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
        RTSUnit* theUnit = ::gWorld.GetUnit(USavedID(this->mTarget));
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
    // ee_combat goals.rs condition (CBTC); the location goal's test is the host's base read
    return EECombatCondition(EE_COMBAT_GOAL_HEAL, inUnit, inEvent, this->mTarget, this, NULL) != 0;
}

// FUNCTION: EXE 0x625789
// While healing a living, non-enemy target (not in cinematics), adds the healer's healing points each full second, up
// to the target's maximum hit points
void EEUGHealUnit::Work(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(3);
    // ee_combat shots.rs heal_work over the plain reads (the target's 0 without one); CBSO records the outcome
    // (the command, the target's hit points and players, the stats: ee_combat store.rs's, from the unit store)
    EeCombatHealWorkIn in;
    ee_combat_fill_heal_work(&in, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), (uint64_t)(uintptr_t)this->mTarget);
    in.cinematic = ::gWorld.mInCinematic;
    in.elapsed = (uint32_t)this->mElapsedTime;
    in.tick_ms = (int32_t)gPhysics->mTickTimeMs;
    EeCombatHealWorkOut theOut = ee_combat_heal_work(&in);
    if (theOut.work) {
        this->mElapsedTime = theOut.elapsed;
        if (theOut.heal)
            this->mTarget->SetHitPoints(theOut.hit_points);
    }
    if (theScope.mOuter)
        EECombatCaptureHealWork(inUnit, this->mTarget, this->mElapsedTime);
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
