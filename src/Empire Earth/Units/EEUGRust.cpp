// New (port; docs/RUST_PLAN.md R5b.4, the sixteenth round): the goals whose class moved whole into Rust
// (ee/EEUGRust.h). Each virtual is one call of the Rust body (ee_units patrol.rs for kGoalPatrol) over the host
// below: the patrol group's walk, the units' reads, the references, the typed file calls. The base goal's own
// fields, save and checksum stay EEUnitGoal's.
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
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitKind.h"
#include "rust/ee_units.h"

namespace {

struct EEPatrolIO {
    FSFile* mFile;
};

uint64_t Handle(const void* inObject)
{
    return reinterpret_cast<uintptr_t>(inObject);
}

EEComplexUnit* UnitOf(uint64_t inHandle)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inHandle));
}

EEPPatrolGroup* GroupOf(uint64_t inHandle)
{
    return reinterpret_cast<EEPPatrolGroup*>(static_cast<uintptr_t>(inHandle));
}

uint64_t PatrolNewGroup(void*)
{
    return Handle(new EEPPatrolGroup);
}

void PatrolReference(void*, uint64_t inObject, bool inGroup)
{
    if (inGroup)
        GroupOf(inObject)->Reference();
    else
        UnitOf(inObject)->Reference();
}

void PatrolDereference(void*, uint64_t inObject, bool inGroup)
{
    if (inGroup)
        GroupOf(inObject)->Dereference();
    else
        UnitOf(inObject)->Dereference();
}

uint64_t PatrolGroupNext(void*, uint64_t inGroup, uint64_t inUnit)
{
    return Handle(GroupOf(inGroup)->GetNext(UnitOf(inUnit)));
}

uint64_t PatrolGroupPrevious(void*, uint64_t inGroup, uint64_t inUnit)
{
    return Handle(GroupOf(inGroup)->GetPrevious(UnitOf(inUnit)));
}

uint32_t PatrolGroupCount(void*, uint64_t inGroup)
{
    return (uint32_t)GroupOf(inGroup)->GetNumPoints();
}

uint32_t PatrolGroupID(void*, uint64_t inGroup)
{
    return (uint32_t)GroupOf(inGroup)->mID;
}

uint64_t PatrolGroupNearest(void*, uint64_t inGroup, int64_t inX, int64_t inY)
{
    return Handle(GroupOf(inGroup)->GetNearest((long)inX, (long)inY));
}

void PatrolUnitTile(void*, uint64_t inUnit, int64_t* outX, int64_t* outY)
{
    *outX = (int64_t)UnitOf(inUnit)->mTileX.Get();
    *outY = (int64_t)UnitOf(inUnit)->mTileY.Get();
}

uint32_t PatrolUnitID(void*, uint64_t inUnit)
{
    return (uint32_t)UnitOf(inUnit)->mUnitID;
}

bool PatrolUnitValid(void*, uint64_t inUnit)
{
    return UnitOf(inUnit)->mIsValid ? true : false;
}

int64_t PatrolUnitCommand(void*, uint64_t inUnit)
{
    EEComplexUnit* theUnit = UnitOf(inUnit);
    return theUnit->mCommand ? (int64_t)theUnit->mCommand->mType : 0;
}

void PatrolResetPathAttempts(void*, uint64_t inGoal)
{
    reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inGoal))->SetPathAttempts(0);
}

void PatrolStartAction(void*, uint64_t inUnit, int64_t inAction)
{
    EEComplexUnit* theUnit = UnitOf(inUnit);
    theUnit->StartAction((unsigned long)inAction);
    theUnit->mUnitClass->ActionChanged(theUnit);
}

uint64_t PatrolFindUnit(void*, uint32_t inID)
{
    return Handle(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit(inID)));
}

uint64_t PatrolPlayerGroup(void*, uint64_t inUnit, uint32_t inID)
{
    return Handle(UnitOf(inUnit)->GetPlayer()->GetPatrolGroup(inID));
}

int32_t PatrolWriteU32(void* inContext, uint32_t inValue)
{
    unsigned long theValue = inValue;
    return (int32_t)static_cast<EEPatrolIO*>(inContext)->mFile->Write(theValue);
}

int32_t PatrolWriteBool(void* inContext, bool inValue)
{
    return (int32_t)static_cast<EEPatrolIO*>(inContext)->mFile->Write(inValue);
}

int32_t PatrolReadU32(void* inContext, uint32_t* outValue)
{
    unsigned long theValue = *outValue;
    long theResult = static_cast<EEPatrolIO*>(inContext)->mFile->Read(theValue);
    *outValue = (uint32_t)theValue;
    return (int32_t)theResult;
}

int32_t PatrolReadBool(void* inContext, bool* outValue)
{
    return (int32_t)static_cast<EEPatrolIO*>(inContext)->mFile->Read(*outValue);
}

// one body of the patrol (EE_UNITS_PATROL_*) on inGoal; inFile for the record's
int64_t Patrol(int inOperation, const EEUGRust* inGoal, uint64_t inA, uint64_t inB, FSFile* inFile = NULL)
{
    EEPatrolIO theIO = {inFile};
    EePatrolHost theHost = {&theIO, PatrolNewGroup, PatrolReference, PatrolDereference, PatrolGroupNext,
                            PatrolGroupPrevious, PatrolGroupCount, PatrolGroupID, PatrolGroupNearest, PatrolUnitTile,
                            PatrolUnitID, PatrolUnitValid, PatrolUnitCommand, PatrolResetPathAttempts,
                            PatrolStartAction, PatrolFindUnit, PatrolPlayerGroup, PatrolWriteU32, PatrolWriteBool,
                            PatrolReadU32, PatrolReadBool};
    return ee_units_patrol_goal(inOperation, Handle(inGoal), inA, inB, &theHost);
}

}  // namespace

EEUGRust::EEUGRust(unsigned char inKind, bool inArg)
    : EEUnitGoal(inKind, inArg)
{
}

EEUGRust* EEUGRust::NewPatrol(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalPatrol, inArg);
    Patrol(EE_UNITS_PATROL_CREATE_EMPTY, theGoal, 0, 0);
    return theGoal;
}

EEUGRust* EEUGRust::NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalPatrol, inArg);
    Patrol(EE_UNITS_PATROL_CREATE, theGoal, Handle(inGroup), Handle(inTarget));
    return theGoal;
}

EEPPatrolGroup* EEUGRust::GetPatrolGroup()
{
    return GroupOf((uint64_t)Patrol(EE_UNITS_PATROL_GROUP, this, 0, 0));
}

EEComplexUnit* EEUGRust::GetPatrolTarget()
{
    return UnitOf((uint64_t)Patrol(EE_UNITS_PATROL_TARGET, this, 0, 0));
}

void EEUGRust::SetPatrolGroup(EEPPatrolGroup* inGroup)
{
    Patrol(EE_UNITS_PATROL_SET_GROUP, this, Handle(inGroup), 0);
}

EEUGRust::~EEUGRust()
{
    if (EEUGRustIsCombat(this->mKind)) {
        this->CombatDestroy();
        return;
    }
    Patrol(EE_UNITS_PATROL_DESTROY, this, 0, 0);
}

EEUnitGoal* EEUGRust::Clone()
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatClone();
    EEPPatrolGroup* theGroup = this->GetPatrolGroup();
    EEComplexUnit* theStart = UnitOf((uint64_t)Patrol(EE_UNITS_PATROL_CLONE_TARGET, this, 0, 0));
    return EEUGRust::NewPatrol(theGroup, theGroup != NULL ? theStart : NULL, this->GetB1C());
}

void EEUGRust::DumpOutOfSynch(FILE* inFile)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatDumpOutOfSynch(inFile);
    EEComplexUnit* theTarget = this->GetPatrolTarget();
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Patrol Group ID is %ld\n", this->GetPatrolGroup()->mID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Current Patrol Point #%08lx\n",
            theTarget != NULL ? theTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGPatrol - Going Forward Flag is %d\n",
            Patrol(EE_UNITS_PATROL_FORWARD, this, 0, 0) != 0);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

long EEUGRust::GetOutOfSynchChecksum()
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatChecksum();
    long theChecksum = (long)Patrol(EE_UNITS_PATROL_CHECKSUM, this, 0, 0);
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

void EEUGRust::GetDebugLine(UWideString& ioText)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatDebugLine(ioText);
    UWideString theText;
    EEPPatrolGroup* theGroup = this->GetPatrolGroup();
    if (theGroup != NULL)
        sprintf(theText, "pat %ld:", theGroup->GetNumPoints());
    else
        sprintf(theText, "pat 0:");
    ioText += theText;
}

void EEUGRust::GetDebugText(UWideString& ioText)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatDebugText(ioText);
    UWideString theText;
    EEPPatrolGroup* theGroup = this->GetPatrolGroup();
    sprintf(theText, "Patrol\nPatrol Points: %ld", theGroup != NULL ? theGroup->GetNumPoints() : 0);
    ioText += theText;
    if (theGroup == NULL)
        return;
    sprintf(theText, "Patrol Points: ( ");
    ioText += theText;
    for (unsigned long i = 0; i < theGroup->mPatrolPoints.size(); i++) {
        EEComplexUnit* thePoint = theGroup->mPatrolPoints[i];
        if (thePoint == NULL)
            continue;
        sprintf(theText, "{%ld,%ld} ", thePoint->mTileX.Get(), thePoint->mTileY.Get());
        ioText += theText;
    }
    sprintf(theText, " )\n");
    ioText += theText;
}

void EEUGRust::GetGoalLocation(GE3DPoint& outLocation)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatGoalLocation(outLocation);
    this->GetPatrolTarget()->GetPosition(outLocation);
}

long EEUGRust::GetStartAction()
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatStartAction();
    return kActionGetNextPatrolPoint;
}

void EEUGRust::Step(EEComplexUnit* inUnit)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatStep(inUnit);
    Patrol(EE_UNITS_PATROL_STEP, this, Handle(inUnit), 0);
}

long EEUGRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatLoad(inMajorVersion, inMinorVersion, inFile);
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    return (long)Patrol(EE_UNITS_PATROL_LOAD, this, 0, 0, &inFile);
}

long EEUGRust::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatRepairPointers(inFirst, inSecond);
    return (long)Patrol(EE_UNITS_PATROL_REPAIR, this, 0, 0);
}

void EEUGRust::Start(EEComplexUnit* inUnit)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatStart(inUnit);
    Patrol(EE_UNITS_PATROL_START, this, Handle(inUnit), 0);
}

long EEUGRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatSave(inMajorVersion, inMinorVersion, outFile);
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    return (long)Patrol(EE_UNITS_PATROL_SAVE, this, 0, 0, &outFile);
}

void EEUGRust::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (EEUGRustIsCombat(this->mKind))
        this->CombatReplaceUnit(inOld, inNew);
}

bool EEUGRust::Suits(EEUnit* inUnit)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatSuits(inUnit);
    return true;
}

void EEUGRust::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatDestination(inUnit, outLocation);
    EEUnitGoal::GetDestination(inUnit, outLocation);
}

long EEUGRust::GetMaxPathAttempts()
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatMaxPathAttempts();
    return EEUnitGoal::GetMaxPathAttempts();
}

void EEUGRust::SetOwner(EEComplexUnit* inUnit)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatSetOwner(inUnit);
    EEUnitGoal::SetOwner(inUnit);
}

void EEUGRust::SetPathAttempts(long inValue)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatSetPathAttempts(inValue);
    EEUnitGoal::SetPathAttempts(inValue);
}

bool EEUGRust::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatEvaluateCondition(inUnit, inEvent);
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

void EEUGRust::Work(EEComplexUnit* inUnit)
{
    if (EEUGRustIsCombat(this->mKind))
        return this->CombatWork(inUnit);
    EEUnitGoal::Work(inUnit);
}
