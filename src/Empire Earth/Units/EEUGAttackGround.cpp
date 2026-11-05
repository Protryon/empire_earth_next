// C:\EEX\Empire Earth\Units\EEUGAttackGround.cpp (file name inferred: no __FILE__; the key "EEUGAttackGround-Data").
// Attacking the ground at a location: 0x21ce6c..0x21d2f4. Unit flags: the exe's module set. Data: the key 0x563508
// (guard byte 0x563500); vtable 0x43cbe8. The inline ctor and slots are first emitted by EECCMeteorStorm.cpp
// (0xb2a4b..), the (bool) ctor by EEUnitGoal.cpp; Virtual30 is ICF-folded into EEUGAttackUnit.cpp's 0x21d460.

#include <windows.h>
#include <stdint.h>
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
#include "ee/EEGraphicObject.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEAreaUtilities.h"

const UString EEUGAttackGround::sData("EEUGAttackGround-Data");
// SYNTHETIC: EXE 0x61ce6c   (wrapper)
// SYNTHETIC: EXE 0x61ce76   (ctor)
// SYNTHETIC: EXE 0x61ce87   (registrar)
// SYNTHETIC: EXE 0x61ce93   (dtor)

// FUNCTION: EXE 0x61ceaf
// Stays where the unit is when it can reach the target location, else heads for the location; tells the unit's
// active group
void EEUGAttackGround::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles1;
    std::vector<EETileCoordinate> theTiles2;
    std::vector<EETileCoordinate> theTiles3;
    long theY = UMFloor(this->mTarget.GetY());
    long theX = UMFloor(this->mTarget.GetX());
    if (inUnit->CanReachTile(theX, theY)) {
        const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
        outLocation.SetX(theTransformation.GetX());
        outLocation.SetY(theTransformation.GetY());
        outLocation.SetZ(theTransformation.GetZ());
    } else
        outLocation = this->mTarget;
    if (inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x61cf93
// Returns a copy of the goal for the same location
EEUnitGoal* EEUGAttackGround::Clone()
{
    return new EEUGAttackGround(this->mTarget, this->GetB1C());
}

// FUNCTION: EXE 0x61cfc1
// Writes the target location to the out-of-synch dump
void EEUGAttackGround::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  Attack Ground Location(%6.6f, %6.6f, %6.6f)\n", this->mTarget.GetX(), this->mTarget.GetY(),
            this->mTarget.GetZ());
}

// FUNCTION: EXE 0x61cfee
// Returns the base goal's checksum plus the raw bits of the target location's coordinates
long EEUGAttackGround::GetOutOfSynchChecksum()
{
    float theValue = this->mTarget.GetX();
    long theChecksum = *reinterpret_cast<int32_t*>(&theValue);
    theValue = this->mTarget.GetY();
    theChecksum += *reinterpret_cast<int32_t*>(&theValue);
    theValue = this->mTarget.GetZ();
    return EEUnitGoal::GetOutOfSynchChecksum() + *reinterpret_cast<int32_t*>(&theValue) + theChecksum;
}

// FUNCTION: EXE 0x61d016
// Appends the target location to the one-line debug text
void EEUGAttackGround::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, " attack grnd %.2f,%.2f:", this->mTarget.GetX(), this->mTarget.GetY());
    ioText += theText;
}

// FUNCTION: EXE 0x61d065
// Appends "Attack Ground" and the target location to the debug text
void EEUGAttackGround::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "Attack Ground %.2f,%.2f\n", this->mTarget.GetX(), this->mTarget.GetY());
    ioText += theText;
}

// FUNCTION: EXE 0x61d0b4
// Returns the target location
void EEUGAttackGround::GetGoalLocation(GE3DPoint& outLocation)
{
    outLocation = this->mTarget;
}

// FUNCTION: EXE 0x61d0cc
// Faces the location, or fires: attack animation (set 8 when firing high over walls), a shot on its frame else
// a randomised reload timer; drops the path and resets the path attempts
void EEUGAttackGround::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionFaceLocation:
        inUnit->SetCurrentPhysicsAction(1);
        inUnit->DropPath();
        break;
    case kActionAttackEnemyUnit:
    case kActionAttackLocation: {
            inUnit->SetCurrentPhysicsAction(1);
            long theSet = inUnit->mStats->mCanFireOverWalls && inUnit->mIsFiringHigh ? 8 : 0;
            unsigned long theFrame = inUnit->GetModel()->mSound1Frames[theSet];
            inUnit->SetAnimation(theSet, -1, 0);
            if (theFrame == inUnit->mPhysicalModel.GetFrame())
                inUnit->mUnitClass->FireShot(inUnit);
            else
                inUnit->Fn1FEA22(inUnit->mStats->mReloadTime);
            inUnit->SetTargetSearchTimer(0);
            inUnit->DropPath();
            this->SetPathAttempts(0);
        }
        break;
    }
}

// FUNCTION: EXE 0x61d1b2
// Reads the base goal and the target location from the saved game; returns the first read error
long EEUGAttackGround::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = this->mTarget.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x61d1db
// Starts the unit's initial attack state and lets its unit class set the action up
void EEUGAttackGround::Start(EEComplexUnit* inUnit)
{
    inUnit->StartAction(kActionInitialAttackState);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x61d1f9
// Writes the base goal and the target location to the saved game; returns the first write error
long EEUGAttackGround::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = this->mTarget.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x61d222
// Answers the attack-ground triggers (location reachable, inside the firing arc, ranged attacker, reload timer
// at 0, fly off after the attack for paratrooper bombers); others go to the common goal
bool EEUGAttackGround::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    switch (inEvent) {
    case kTriggerLocationInWeaponRange:
        return inUnit->CanReachTile(UMFloor(this->mTarget.GetX()), UMFloor(this->mTarget.GetY()));
    case kTriggerLocationInsideFiringArch:
        return inUnit->GetUnitClass()->FacesPoint(inUnit, this->mTarget, true);
    case kTriggerAttackerIsRangedUnit:
        return inUnit->mStats->mRange > 1.0f ? true : false;
    case kTriggerReadyToFireAgain:
        return inUnit->mReloadTimer == 0 ? true : false;
    case kTriggerFlyOffMapAfterAttack:
        return inUnit->mUnitClass->mIsParatrooperBomber;
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// ICF-FOLDED: EXE 0x61d460
// Returns check-range as the first action
long EEUGAttackGround::GetStartAction()
{
    return kActionCheckRange;
}

// ICF-FOLDED: EXE 0x6907a3
// Does nothing: the goal holds no units
void EEUGAttackGround::ReplaceUnit(EEComplexUnit*, EEComplexUnit*, EEComplexUnit*)
{
}
