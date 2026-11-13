// New (port; docs/RUST_PLAN.md R5b.2, the twelfth round): the combat lane's goals as EEUGRust (ee/EEUGRust.h): the
// attack on the ground (kGoalAttackGround; was EEUGAttackGround 0x21ce6c..0x21d2f4). Their state and bodies are
// ee_combat goal_objects.rs's (a record by goal object) and goals.rs's (the trigger tests and steps); here each virtual
// is one call over the host of the C++ objects' operations, the debug and out-of-synch texts' formatting, and the CBGO
// records (TEST ONLY, recorded from the original bodies first).
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
#include "ee/EEUGRust.h"
#include "ee/EECombatGoals.h"
#include "ee/EEAreaUtilities.h"
#include "rust/ee_combat.h"

namespace {

uint64_t Handle(const void* inObject)
{
    return reinterpret_cast<uintptr_t>(inObject);
}

EEComplexUnit* UnitOf(uint64_t inHandle)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inHandle));
}

uint32_t Bits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

float Float(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

// the operations of ee_combat goal_objects.rs (EE_COMBAT_G_*), each the original's statement; inContext the call's file
int64_t GoalOp(void* inContext, int32_t inCode, uint64_t inUnit, EeCombatGoalArgs* ioArgs)
{
    int64_t* a = *ioArgs;
    EEComplexUnit* theUnit = UnitOf(inUnit);
    FSFile* theFile = (FSFile*)inContext;
    switch (inCode) {
    case EE_COMBAT_G_CAN_REACH_TILE:
        return theUnit->CanReachTile((long)a[0], (long)a[1]);
    case EE_COMBAT_G_UNIT_LOCATION: {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        a[0] = Bits(theLocation.GetX());
        a[1] = Bits(theLocation.GetY());
        a[2] = Bits(theLocation.GetZ());
        return 0;
    }
    case EE_COMBAT_G_TELL_GROUP:
        if (theUnit->mActiveGroup == NULL)
            return 0;
        theUnit->mActiveGroup->TellGroup(theUnit, GE3DPoint(Float(a[0]), Float(a[1]), Float(a[2])));
        return 1;
    case EE_COMBAT_G_START_ACTION:
        theUnit->StartAction((unsigned long)a[0]);
        theUnit->mUnitClass->ActionChanged(theUnit);
        return 0;
    case EE_COMBAT_G_WRITE_F32:   // GE3DPoint::Save's: 4 on a closed file
        return theFile->GetIsOpen() ? theFile->Write(Float(a[0])) : 4;
    case EE_COMBAT_G_READ_F32: {
        if (!theFile->GetIsOpen())
            return 4;
        float theValue = Float(a[0]);
        long theResult = theFile->Read(theValue);
        a[0] = Bits(theValue);
        return theResult;
    }
    }
    return 0;
}

EeCombatGoalHost HostOf(FSFile* inFile)
{
    EeCombatGoalHost theHost = {inFile, GoalOp};
    return theHost;
}

}  // namespace

bool EEUGRustIsCombat(unsigned char inKind)
{
    return inKind == kGoalAttackGround;
}

EEUGRust* EEUGRust::NewAttackGround(const GE3DPoint& inTarget, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackGround, inArg);
    ee_combat_ground_create(Handle(theGoal), inTarget.GetX(), inTarget.GetY(), inTarget.GetZ());
    return theGoal;
}

GE3DPoint EEUGRust::GetGroundTarget()
{
    float thePoint[3];
    ee_combat_ground_target(Handle(this), &thePoint);
    return GE3DPoint(thePoint[0], thePoint[1], thePoint[2]);
}

void EEUGRust::CombatDestroy()
{
    ee_combat_goal_drop(Handle(this));
}

EEUnitGoal* EEUGRust::CombatClone()
{
    return EEUGRust::NewAttackGround(this->GetGroundTarget(), this->GetB1C());
}

void EEUGRust::CombatDumpOutOfSynch(FILE* inFile)
{
    GE3DPoint theTarget = this->GetGroundTarget();
    fprintf(inFile, "OUT OF SYNCH DUMP:  Attack Ground Location(%6.6f, %6.6f, %6.6f)\n", theTarget.GetX(), theTarget.GetY(),
            theTarget.GetZ());
}

long EEUGRust::CombatChecksum()
{
    return EEUnitGoal::GetOutOfSynchChecksum() + ee_combat_ground_checksum(Handle(this));
}

void EEUGRust::CombatDebugLine(UWideString& ioText)
{
    UWideString theText;
    GE3DPoint theTarget = this->GetGroundTarget();
    sprintf(theText, " attack grnd %.2f,%.2f:", theTarget.GetX(), theTarget.GetY());
    ioText += theText;
}

void EEUGRust::CombatDebugText(UWideString& ioText)
{
    UWideString theText;
    GE3DPoint theTarget = this->GetGroundTarget();
    sprintf(theText, "Attack Ground %.2f,%.2f\n", theTarget.GetX(), theTarget.GetY());
    ioText += theText;
}

void EEUGRust::CombatGoalLocation(GE3DPoint& outLocation)
{
    outLocation = this->GetGroundTarget();
}

long EEUGRust::CombatStartAction()
{
    return kActionCheckRange;
}

void EEUGRust::CombatStep(EEComplexUnit* inUnit)
{
    // ee_combat goals.rs step: the plan's operations executed by ee/EECombatGoals.h's host (CBGS)
    EECombatStep(EE_COMBAT_GOAL_ATTACK_GROUND, inUnit, this, NULL);
}

long EEUGRust::CombatLoad(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    EeCombatGoalHost theHost = HostOf(&inFile);
    return ee_combat_ground_load(Handle(this), &theHost);
}

long EEUGRust::CombatRepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return EEUnitGoal::RepairPointers(inFirst, inSecond);
}

void EEUGRust::CombatStart(EEComplexUnit* inUnit)
{
    EeCombatGoalArgs theArgs = {kActionInitialAttackState};
    GoalOp(NULL, EE_COMBAT_G_START_ACTION, Handle(inUnit), &theArgs);
}

long EEUGRust::CombatSave(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    EeCombatGoalHost theHost = HostOf(&outFile);
    return ee_combat_ground_save(Handle(this), &theHost);
}

void EEUGRust::CombatReplaceUnit(EEComplexUnit*, EEComplexUnit*)
{
}

bool EEUGRust::CombatSuits(EEUnit* inUnit)
{
    bool theResult = inUnit->mUnitClass->mProjectileClass != NULL;
    GE3DPoint theTarget = this->GetGroundTarget();
    float thePoint[3] = {theTarget.GetX(), theTarget.GetY(), theTarget.GetZ()};
    EECombatCaptureGoal(EE_COMBAT_GO_SUITS, kGoalAttackGround, inUnit, NULL, theResult, thePoint);
    return theResult;
}

void EEUGRust::CombatDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    EeCombatGoalHost theHost = HostOf(NULL);
    float thePoint[3];
    ee_combat_ground_destination(Handle(this), Handle(inUnit), &theHost, &thePoint);
    outLocation.SetXYZ(thePoint[0], thePoint[1], thePoint[2]);
    EECombatCaptureGoal(EE_COMBAT_GO_DESTINATION, kGoalAttackGround, inUnit, NULL, inUnit->mActiveGroup != NULL, thePoint);
}

long EEUGRust::CombatMaxPathAttempts()
{
    return EEUnitGoal::GetMaxPathAttempts();
}

void EEUGRust::CombatSetOwner(EEComplexUnit* inUnit)
{
    EEUnitGoal::SetOwner(inUnit);
}

void EEUGRust::CombatSetPathAttempts(long inValue)
{
    EEUnitGoal::SetPathAttempts(inValue);
}

bool EEUGRust::CombatEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC)
    return EECombatCondition(EE_COMBAT_GOAL_ATTACK_GROUND, inUnit, inEvent, NULL, this, NULL) != 0;
}

void EEUGRust::CombatWork(EEComplexUnit* inUnit)
{
    EEUnitGoal::Work(inUnit);
}
