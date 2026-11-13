// New (port; docs/RUST_PLAN.md R5b.2, the twelfth round): the combat lane's goals as EEUGRust (ee/EEUGRust.h): the
// attack on the ground (kGoalAttackGround; was EEUGAttackGround 0x21ce6c..0x21d2f4) and the attack on a unit
// (kGoalAttackUnit; was EEUGAttackUnit 0x21d2f4..0x21e0c1). Their state and bodies are
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
#include "ee/EESaveGameFiles.h"
#include "ee/EEActiveGroup.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitKind.h"
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
    case EE_COMBAT_G_ON_TILE: return theUnit->mTileNode != NULL;
    case EE_COMBAT_G_COMPLEX: return (int64_t)Handle(reinterpret_cast<EEUnit*>(theUnit)->mComplexUnit);
    case EE_COMBAT_G_ATTACKER_FLAGS:
        if (theUnit == NULL)
            return 0;
        return (theUnit->mPlayer->IsHuman() ? EE_COMBAT_G_HUMAN : 0) | (theUnit->mUnitClass->mIsProjectile ? EE_COMBAT_G_PROJECTILE : 0)
            | (theUnit->mUnitClass->IsPriest() ? EE_COMBAT_G_PRIEST : 0);
    case EE_COMBAT_G_NUM_ATTACKERS:
        theUnit->mNumAttackers += (long)a[0];
        return theUnit->mNumAttackers;
    case EE_COMBAT_G_PARTS: return (theUnit->mNextPart != NULL ? 1 : 0) | (theUnit->mPreviousPart != NULL ? 2 : 0);
    case EE_COMBAT_G_ATTACK_PART: return (int64_t)Handle(EEGetAttackPart(theUnit, UnitOf((uint64_t)a[0])));
    case EE_COMBAT_G_REFERENCE: reinterpret_cast<EEUnit*>(theUnit)->Reference(); return 0;
    case EE_COMBAT_G_DEREFERENCE: reinterpret_cast<EEUnit*>(theUnit)->Dereference(); return 0;
    case EE_COMBAT_G_TILE: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(theUnit);
        a[0] = theTarget->mTileX;
        a[1] = theTarget->mTileY;
        a[2] = theTarget->GetTileX();
        a[3] = theTarget->GetTileY();
        return 0;
    }
    case EE_COMBAT_G_FAMILY: return reinterpret_cast<EEUnit*>(theUnit)->mUnitClass->mFamily;
    case EE_COMBAT_G_CATEGORY: return reinterpret_cast<EEUnit*>(theUnit)->mUnitClass->mCategory;
    case EE_COMBAT_G_PLAYER_ATTACK: {
        EEPlayer* thePlayer = theUnit->mPlayer;
        if (a[0] == 1) {
            if (thePlayer->mNumAttacks == 0)
                thePlayer->mCombatState = 2;
            thePlayer->mNumAttacks++;
        } else if (a[0] == -1) {
            if (--thePlayer->mNumAttacks == 0)
                thePlayer->mCombatState = 1;
        } else {
            if (thePlayer->mNumAttacks == 0 && !a[1])
                thePlayer->mCombatState = 2;
            thePlayer->mNumAttacks++;
        }
        return 0;
    }
    case EE_COMBAT_G_VERIFY_TARGET: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[0]));
        ESVERIFY_AT(::EEUUGetIsUnitClassTargetable(theUnit, theTarget->GetClass()) == true, 729);
        ESVERIFY_AT(theTarget->GetClass()->GetWeapon() >= 0 && theTarget->GetClass()->GetWeapon() < keeucwTotal, 730);
        return 0;
    }
    case EE_COMBAT_G_FIND_UNIT: {
        RTSUnit* theFound = ::gWorld.GetUnit((unsigned long)a[0]);
        return a[1] ? (int64_t)Handle(EEUnitCast<EEComplexUnit>(theFound)) : (int64_t)Handle(static_cast<EEUnit*>(theFound));
    }
    case EE_COMBAT_G_FIND_GHOST: return (int64_t)Handle(EEFindLoadedGhostUnit((unsigned long)a[0]));
    case EE_COMBAT_G_ID: return (int64_t)(uint32_t)reinterpret_cast<EEUnit*>(theUnit)->mUnitID;
    case EE_COMBAT_G_HIT_POINTS: return reinterpret_cast<EEUnit*>(theUnit)->mHitPoints.Get();
    case EE_COMBAT_G_VELOCITY: return Bits(theUnit->mVelocity);
    case EE_COMBAT_G_POSITION: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[0]));
        GE3DPoint thePoint(Float(a[2]), Float(a[3]), Float(a[4]));
        if (a[1])
            EEGetFiringPosition(theUnit, theTarget, thePoint);
        else
            EEGetLeadPosition(theUnit, theTarget, thePoint);
        a[0] = Bits(thePoint.GetX());
        a[1] = Bits(thePoint.GetY());
        a[2] = Bits(thePoint.GetZ());
        return 0;
    }
    case EE_COMBAT_G_PLAYER: return (int64_t)Handle(reinterpret_cast<EEUnit*>(theUnit)->mPlayer);
    case EE_COMBAT_G_PLAYER_READ: {
        EEPlayer* thePlayer = reinterpret_cast<EEPlayer*>(static_cast<uintptr_t>(inUnit));
        return a[0] == 1 ? (int64_t)thePlayer->mEnemies[a[1]] : (int64_t)thePlayer->mPlayerID;
    }
    case EE_COMBAT_G_AMMO_TYPE: return reinterpret_cast<EEUnit*>(theUnit)->mUnitClass->mAmmoType;
    case EE_COMBAT_G_CAN_BE_ATTACKED: return reinterpret_cast<EEUnit*>(theUnit)->mUnitClass->CanBeAttacked();
    case EE_COMBAT_G_WRITE_U32: {
        unsigned long theValue = (unsigned long)(uint32_t)a[0];
        return theFile->Write(theValue);
    }
    case EE_COMBAT_G_READ_U32: {
        unsigned long theValue = (unsigned long)(uint32_t)a[0];
        long theResult = theFile->Read(theValue);
        a[0] = (uint32_t)theValue;
        return theResult;
    }
    case EE_COMBAT_G_WRITE_I32: {
        long theValue = (long)a[0];
        return theFile->Write(theValue);
    }
    case EE_COMBAT_G_READ_I32: {
        long theValue = (long)a[0];
        long theResult = theFile->Read(theValue);
        a[0] = theValue;
        return theResult;
    }
    case EE_COMBAT_G_WRITE_BOOL: {
        bool theValue = a[0] != 0;
        return theFile->Write(theValue);
    }
    case EE_COMBAT_G_READ_BOOL: {
        bool theValue = a[0] != 0;
        long theResult = theFile->Read(theValue);
        a[0] = theValue;
        return theResult;
    }
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
    return inKind == kGoalAttackGround || inKind == kGoalAttackUnit;
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

// ---- the attack on a unit (kGoalAttackUnit; was EEUGAttackUnit 0x21d2f4..0x21e0c1): ee_combat goal_attack.rs

namespace {

EeCombatAttackGoal AttackOf(const EEUGRust* inGoal)
{
    EeCombatAttackGoal theRecord;
    ee_combat_attack_get(Handle(inGoal), &theRecord);
    return theRecord;
}

int32_t Attack(int32_t inFunction, EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0, FSFile* inFile = NULL,
               float (*ioPoint)[3] = NULL)
{
    EeCombatGoalHost theHost = HostOf(inFile);
    return ee_combat_attack_goal(inFunction, Handle(inGoal), inA, inB, &theHost, ioPoint);
}

}  // namespace

EEUGRust* EEUGRust::NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackUnit, inArg3);
    Attack(inIsConvert ? EE_COMBAT_ATTACK_CREATE_CONVERT : EE_COMBAT_ATTACK_CREATE, theGoal, Handle(inAttacker),
           Handle(inTarget));
    return theGoal;
}

EEUnit* EEUGRust::GetAttackTarget() const
{
    return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(AttackOf(this).target));
}

EEComplexUnit* EEUGRust::GetAttacker() const
{
    return UnitOf(AttackOf(this).attacker);
}

void EEUGRust::SetAttacker(EEComplexUnit* inAttacker)
{
    EeCombatAttackGoal theRecord = AttackOf(this);
    theRecord.attacker = Handle(inAttacker);
    ee_combat_attack_set(Handle(this), &theRecord);
}

void EEUGRust::SetAttackTarget(EEUnit* inTarget)
{
    Attack(EE_COMBAT_ATTACK_SET_TARGET, this, Handle(inTarget));
}

void EEUGRust::RetargetGatePart(EEComplexUnit*)
{
    Attack(EE_COMBAT_ATTACK_RETARGET_GATE, this);
}

void EEUGRust::UpdateTargetTile()
{
    Attack(EE_COMBAT_ATTACK_UPDATE_TILE, this);
}

long EEUGRust::GetAttackTileX() const
{
    return AttackOf(this).tile[0];
}

long EEUGRust::GetAttackTileY() const
{
    return AttackOf(this).tile[1];
}

bool EEUGRust::IsConvertAttack() const
{
    return AttackOf(this).convert != 0;
}

GE3DPoint EEUGRust::GetAttackLocation() const
{
    EeCombatAttackGoal theRecord = AttackOf(this);
    return GE3DPoint(Float(theRecord.location[0]), Float(theRecord.location[1]), Float(theRecord.location[2]));
}

void EEUGRust::SetAttackLocation(const GE3DPoint& inLocation)
{
    EeCombatAttackGoal theRecord = AttackOf(this);
    theRecord.location[0] = Bits(inLocation.GetX());
    theRecord.location[1] = Bits(inLocation.GetY());
    theRecord.location[2] = Bits(inLocation.GetZ());
    ee_combat_attack_set(Handle(this), &theRecord);
}

unsigned long EEUGRust::GetSidestepCount() const
{
    return AttackOf(this).sidestep;
}

void EEUGRust::SetSidestepCount(unsigned long inCount)
{
    EeCombatAttackGoal theRecord = AttackOf(this);
    theRecord.sidestep = (uint32_t)inCount;
    ee_combat_attack_set(Handle(this), &theRecord);
}

EEUGRust* EEUGRust::NewAttackUnitEmpty(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackUnit, inArg);
    Attack(EE_COMBAT_ATTACK_CREATE_EMPTY, theGoal);
    return theGoal;
}

// the attack on a unit's bodies (EEUGRust's Combat* for kGoalAttackUnit)

static void AttackDumpOutOfSynch(EEUGRust* inGoal, FILE* inFile)
{
    GE3DPoint theLocation = inGoal->GetAttackLocation();
    EEComplexUnit* theAttacker = inGoal->GetAttacker();
    EEUnit* theTarget = inGoal->GetAttackTarget();
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The Actual Destination is (%6.6f, %6.6f, %6.6f)\n",
            theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The attacking unit #%08lx is a %s\n", theAttacker->mUnitID,
            (const char*)theAttacker->mUnitClass->mName);
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The target unit #%08lx is a %s\n",
            theTarget ? theTarget->mUnitID : -1, theTarget ? (const char*)theTarget->mUnitClass->mName : "???");
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The last known target X,Y(%ld, %ld)\n", inGoal->GetAttackTileX(),
            inGoal->GetAttackTileY());
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - Is this a convert goal? %2d\n", inGoal->IsConvertAttack());
    inGoal->EEUnitGoal::DumpOutOfSynch(inFile);
}

static void AttackDebugLine(EEUGRust* inGoal, UWideString& ioText)
{
    UWideString theText;
    ioText += "attack";
    EEUnit* theTarget = inGoal->GetAttackTarget();
    if (theTarget != NULL) {
        sprintf(theText, " unit (uid %ld, hp %ld):", theTarget->mUnitID & 0xffffff, theTarget->mHitPoints.Get());
        ioText += theText;
    }
}

static void AttackDebugText(EEUGRust* inGoal, UWideString& ioText)
{
    UWideString theText;
    ioText += "Attack\n";
    EEUnit* theTarget = inGoal->GetAttackTarget();
    if (theTarget == NULL)
        return;
    sprintf(theText, "Target Unit: %s at (%ld,%ld)\nTarget Health: %ld/%ld (%.2f%%)\n",
            (const char*)theTarget->mUnitClass->mName, theTarget->mTileX.Get(), theTarget->mTileY.Get(),
            theTarget->mHitPoints.Get(), theTarget->mStats->mMaxHitPoints,
            (float)theTarget->mHitPoints / theTarget->mStats->mMaxHitPoints);
    ioText += theText;
}

void EEUGRust::CombatDestroy()
{
    if (this->mKind == kGoalAttackUnit)
        Attack(EE_COMBAT_ATTACK_DESTROY, this);
    ee_combat_goal_drop(Handle(this));
}

EEUnitGoal* EEUGRust::CombatClone()
{
    if (this->mKind == kGoalAttackUnit)
        return EEUGRust::NewAttackUnit(this->GetAttacker(), this->GetAttackTarget(), this->GetB1C(), this->IsConvertAttack());
    return EEUGRust::NewAttackGround(this->GetGroundTarget(), this->GetB1C());
}

void EEUGRust::CombatDumpOutOfSynch(FILE* inFile)
{
    if (this->mKind == kGoalAttackUnit)
        return AttackDumpOutOfSynch(this, inFile);
    GE3DPoint theTarget = this->GetGroundTarget();
    fprintf(inFile, "OUT OF SYNCH DUMP:  Attack Ground Location(%6.6f, %6.6f, %6.6f)\n", theTarget.GetX(), theTarget.GetY(),
            theTarget.GetZ());
}

long EEUGRust::CombatChecksum()
{
    if (this->mKind == kGoalAttackUnit)
        return EEUnitGoal::GetOutOfSynchChecksum() + Attack(EE_COMBAT_ATTACK_CHECKSUM, this);
    return EEUnitGoal::GetOutOfSynchChecksum() + ee_combat_ground_checksum(Handle(this));
}

void EEUGRust::CombatDebugLine(UWideString& ioText)
{
    if (this->mKind == kGoalAttackUnit)
        return AttackDebugLine(this, ioText);
    UWideString theText;
    GE3DPoint theTarget = this->GetGroundTarget();
    sprintf(theText, " attack grnd %.2f,%.2f:", theTarget.GetX(), theTarget.GetY());
    ioText += theText;
}

void EEUGRust::CombatDebugText(UWideString& ioText)
{
    if (this->mKind == kGoalAttackUnit)
        return AttackDebugText(this, ioText);
    UWideString theText;
    GE3DPoint theTarget = this->GetGroundTarget();
    sprintf(theText, "Attack Ground %.2f,%.2f\n", theTarget.GetX(), theTarget.GetY());
    ioText += theText;
}

void EEUGRust::CombatGoalLocation(GE3DPoint& outLocation)
{
    if (this->mKind == kGoalAttackUnit)
        return (void)(outLocation = this->GetAttackLocation());
    outLocation = this->GetGroundTarget();
}

long EEUGRust::CombatStartAction()
{
    return kActionCheckRange;
}

void EEUGRust::CombatStep(EEComplexUnit* inUnit)
{
    if (this->mKind == kGoalAttackUnit)
        return EECombatStep(EE_COMBAT_GOAL_ATTACK_UNIT, inUnit, this, this->GetAttackTarget());
    // ee_combat goals.rs step: the plan's operations executed by ee/EECombatGoals.h's host (CBGS)
    EECombatStep(EE_COMBAT_GOAL_ATTACK_GROUND, inUnit, this, NULL);
}

long EEUGRust::CombatLoad(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    if (this->mKind == kGoalAttackUnit)
        return Attack(EE_COMBAT_ATTACK_LOAD, this, inMajorVersion,
                      (uint64_t)(uint32_t)inMinorVersion | ((uint64_t)EESaveHasLiveState(inMajorVersion, inMinorVersion) << 32),
                      &inFile);
    EeCombatGoalHost theHost = HostOf(&inFile);
    return ee_combat_ground_load(Handle(this), &theHost);
}

long EEUGRust::CombatRepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    if (this->mKind == kGoalAttackUnit)
        return Attack(EE_COMBAT_ATTACK_REPAIR, this, EESaveHasLiveState(inFirst, inSecond));
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
    if (this->mKind == kGoalAttackUnit)
        return Attack(EE_COMBAT_ATTACK_SAVE, this,
                      (EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0)
                          | (EESaveHasGoalGhosts(inMajorVersion, inMinorVersion) ? 2 : 0), 0, &outFile);
    EeCombatGoalHost theHost = HostOf(&outFile);
    return ee_combat_ground_save(Handle(this), &theHost);
}

void EEUGRust::CombatReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    if (this->mKind == kGoalAttackUnit)
        Attack(EE_COMBAT_ATTACK_REPLACE, this, Handle(inOld), Handle(inNew));
}

bool EEUGRust::CombatSuits(EEUnit* inUnit)
{
    if (this->mKind == kGoalAttackUnit) {
        bool theSuits = Attack(EE_COMBAT_ATTACK_SUITS, this, Handle(inUnit)) != 0;
        EECombatCaptureGoal(EE_COMBAT_GO_SUITS, kGoalAttackUnit, inUnit, this->GetAttackTarget(), theSuits);
        return theSuits;
    }
    bool theResult = inUnit->mUnitClass->mProjectileClass != NULL;
    GE3DPoint theTarget = this->GetGroundTarget();
    float thePoint[3] = {theTarget.GetX(), theTarget.GetY(), theTarget.GetZ()};
    EECombatCaptureGoal(EE_COMBAT_GO_SUITS, kGoalAttackGround, inUnit, NULL, theResult, thePoint);
    return theResult;
}

void EEUGRust::CombatDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    if (this->mKind == kGoalAttackUnit) {
        float thePoint[3] = {outLocation.GetX(), outLocation.GetY(), outLocation.GetZ()};
        Attack(EE_COMBAT_ATTACK_DESTINATION, this, Handle(inUnit), 0, NULL, &thePoint);
        outLocation.SetXYZ(thePoint[0], thePoint[1], thePoint[2]);
        EECombatCaptureGoal(EE_COMBAT_GO_DESTINATION, kGoalAttackUnit, inUnit, this->GetAttackTarget(),
                            inUnit->mActiveGroup != NULL, thePoint, (uint32_t)this->GetAttackTileX(),
                            (uint32_t)this->GetAttackTileY());
        return;
    }
    EeCombatGoalHost theHost = HostOf(NULL);
    float thePoint[3];
    ee_combat_ground_destination(Handle(this), Handle(inUnit), &theHost, &thePoint);
    outLocation.SetXYZ(thePoint[0], thePoint[1], thePoint[2]);
    EECombatCaptureGoal(EE_COMBAT_GO_DESTINATION, kGoalAttackGround, inUnit, NULL, inUnit->mActiveGroup != NULL, thePoint);
}

long EEUGRust::CombatMaxPathAttempts()
{
    if (this->mKind == kGoalAttackUnit)
        return this->mPathAttempts + 1;
    return EEUnitGoal::GetMaxPathAttempts();
}

void EEUGRust::CombatSetOwner(EEComplexUnit* inUnit)
{
    if (this->mKind == kGoalAttackUnit)
        return this->SetAttacker(inUnit);
    EEUnitGoal::SetOwner(inUnit);
}

void EEUGRust::CombatSetPathAttempts(long inValue)
{
    if (this->mKind == kGoalAttackUnit && inValue == 0)
        this->SetSidestepCount(0);
    EEUnitGoal::SetPathAttempts(inValue);
}

bool EEUGRust::CombatEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    if (this->mKind == kGoalAttackUnit)
        return EECombatCondition(EE_COMBAT_GOAL_ATTACK_UNIT, inUnit, inEvent, this->GetAttackTarget(), this, NULL) != 0;
    // ee_combat goals.rs condition (CBTC)
    return EECombatCondition(EE_COMBAT_GOAL_ATTACK_GROUND, inUnit, inEvent, NULL, this, NULL) != 0;
}

void EEUGRust::CombatWork(EEComplexUnit* inUnit)
{
    EEUnitGoal::Work(inUnit);
}
