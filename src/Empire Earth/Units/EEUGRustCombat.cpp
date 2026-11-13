// New (port; docs/RUST_PLAN.md R5b.2, the twelfth round): the combat lane's goals on EEUGRust (ee/EEUGRust.h): the
// attack on the ground (kGoalAttackGround; was EEUGAttackGround 0x21ce6c..0x21d2f4) and the attack on a unit
// (kGoalAttackUnit; was EEUGAttackUnit 0x21d2f4..0x21e0c1). EEUGRust's virtuals go through ee_units' goal dispatch
// (rust_goal.rs), which hands these kinds to ee_combat goal_dispatch.rs: their state and bodies are Rust's
// (goal_objects.rs, goal_attack.rs; the trigger tests and steps goals.rs's). Here: the combat goals' operations of the
// goals' host (EEUGRust.cpp's GoalOp's default case), each the original's statement, the CBGO records (TEST ONLY,
// recorded from the original bodies first), and the accessors the callers use.
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
#include "rust/ee_units.h"

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

EEUnitGoal* UnitGoalOf(uint64_t inHandle)
{
    return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inHandle));
}


}  // namespace

// the combat goals' operations of the goals' host (ee_combat EE_COMBAT_G_*, from 1000: EEUGRust.cpp's GoalOp
// default case), each the original's statement; inFile the call's file
int64_t EECombatGoalHostOp(FSFile* inFile, int32_t inCode, uint64_t inUnit, uint64_t inB, EeUnitsGoalIo* ioArgs)
{
    int64_t* a = ioArgs->i;
    EEComplexUnit* theUnit = UnitOf(inUnit);
    FSFile* theFile = inFile;
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
    // the dispatch's (ee_combat goal_dispatch.rs)
    case EE_COMBAT_G_STEP:
        EECombatStep((int32_t)a[0], theUnit, UnitGoalOf((uint64_t)a[2]), reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[1])));
        return 0;
    case EE_COMBAT_G_EVALUATE:
        return EECombatCondition((int32_t)a[0], theUnit, (unsigned long)a[1], reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[2])),
                                 UnitGoalOf((uint64_t)a[3]), NULL);
    case EE_COMBAT_G_PROJECTILE_CLASS: return reinterpret_cast<EEUnit*>(theUnit)->mUnitClass->mProjectileClass != NULL;
    case EE_COMBAT_G_CAPTURE: {   // TEST ONLY: CBGO
        float thePoint[3] = {Float(a[4]), Float(a[5]), Float(a[6])};
        EECombatCaptureGoal((uint32_t)a[0], (uint32_t)a[1], reinterpret_cast<EEUnit*>(theUnit),
                            reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[2])), (int32_t)a[3], thePoint, (uint32_t)a[7],
                            (uint32_t)((uint64_t)a[7] >> 32));
        return 0;
    }
    case EE_COMBAT_G_PATH_ATTEMPTS: return UnitGoalOf(inUnit)->mPathAttempts;
    case EE_COMBAT_G_SAVE_FLAGS:
        return (EESaveHasLiveState((unsigned long)a[0], (unsigned long)a[1]) ? 1 : 0)
            | (EESaveHasGoalGhosts((unsigned long)a[0], (unsigned long)a[1]) ? 2 : 0);
    case EE_COMBAT_G_CLASS_NAME: {
        EEUnitClass* theClass = reinterpret_cast<EEUnit*>(theUnit)->mUnitClass;
        return theClass != NULL ? (int64_t)(uintptr_t)(const char*)theClass->mName : 0;
    }
    case EE_COMBAT_G_MAX_HIT_POINTS: return reinterpret_cast<EEUnit*>(theUnit)->mStats->mMaxHitPoints;
    case EE_COMBAT_G_HAS_GROUP: return theUnit->mActiveGroup != NULL;
    }
    return 0;
}

namespace {

// one combat body through the goal dispatch; the point in and out
int64_t Combat(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0, int64_t inI0 = 0,
               GE3DPoint* ioPoint = NULL)
{
    EeUnitsGoalIo theArgs = {};
    theArgs.i[0] = inI0;
    if (ioPoint != NULL) {
        theArgs.f[0] = ioPoint->GetX();
        theArgs.f[1] = ioPoint->GetY();
        theArgs.f[2] = ioPoint->GetZ();
    }
    int64_t theResult = EEUGRustGoal(inOperation, inGoal, inA, inB, &theArgs);
    if (ioPoint != NULL)
        ioPoint->SetXYZ(theArgs.f[0], theArgs.f[1], theArgs.f[2]);
    return theResult;
}

EeCombatAttackGoal AttackOf(const EEUGRust* inGoal)
{
    EeCombatAttackGoal theRecord;
    ee_combat_attack_get(Handle(inGoal), &theRecord);
    return theRecord;
}

void AttackSet(const EEUGRust* inGoal, const EeCombatAttackGoal& inRecord)
{
    ee_combat_attack_set(Handle(inGoal), &inRecord);
}

}  // namespace

EEUGRust* EEUGRust::NewAttackGround(const GE3DPoint& inTarget, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackGround, inArg);
    GE3DPoint thePoint = inTarget;
    Combat(EE_COMBAT_GOAL_NEW_GROUND, theGoal, 0, 0, 0, &thePoint);
    return theGoal;
}

GE3DPoint EEUGRust::GetGroundTarget()
{
    float thePoint[3];
    ee_combat_ground_target(Handle(this), &thePoint);
    return GE3DPoint(thePoint[0], thePoint[1], thePoint[2]);
}

EEUGRust* EEUGRust::NewAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackUnit, inArg3);
    Combat(EE_COMBAT_GOAL_NEW_ATTACK, theGoal, Handle(inAttacker), Handle(inTarget), inIsConvert ? 1 : 0);
    return theGoal;
}

EEUGRust* EEUGRust::NewAttackUnitEmpty(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalAttackUnit, inArg);
    Combat(EE_COMBAT_GOAL_NEW_ATTACK, theGoal, 0, 0, 2);
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
    AttackSet(this, theRecord);
}

void EEUGRust::SetAttackTarget(EEUnit* inTarget)
{
    Combat(EE_COMBAT_GOAL_SET_TARGET, this, Handle(inTarget));
}

void EEUGRust::RetargetGatePart(EEComplexUnit*)
{
    Combat(EE_COMBAT_GOAL_RETARGET_GATE, this);
}

void EEUGRust::UpdateTargetTile()
{
    Combat(EE_COMBAT_GOAL_UPDATE_TILE, this);
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
    AttackSet(this, theRecord);
}

unsigned long EEUGRust::GetSidestepCount() const
{
    return AttackOf(this).sidestep;
}

void EEUGRust::SetSidestepCount(unsigned long inCount)
{
    EeCombatAttackGoal theRecord = AttackOf(this);
    theRecord.sidestep = (uint32_t)inCount;
    AttackSet(this, theRecord);
}
