// C:\EEX\Empire Earth\Units\EEUGAttackUnit.cpp (__FILE__ 0x4c9338). Attacking a unit: 0x21d2f4..0x21e0c1. Unit flags:
// the exe's module set. Data: the key 0x563520 (guard byte 0x563518); vtable 0x447380. Its Virtual30 0x21d460 is
// also EEUGAttackGround's (ICF keeps this copy); its Virtual28/Virtual54 are folded into EEUGAttackGround.cpp's.

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
#include "lle/U2DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGraphicObject.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEAreaUtilities.h"

const UString EEUGAttackUnit::sData("EEUGAttackUnit-Data");
// SYNTHETIC: EXE 0x61d2f4   (wrapper)
// SYNTHETIC: EXE 0x61d2fe   (ctor)
// SYNTHETIC: EXE 0x61d30f   (registrar)
// SYNTHETIC: EXE 0x61d31b   (dtor)

// FUNCTION: EXE 0x61d337
// Creates an attack goal on a target (a multi-part target's attack part unless a priest attacks), referencing
// it; counts AI attackers on it and the player's attacks (not on animals)
EEUGAttackUnit::EEUGAttackUnit(EEComplexUnit* inAttacker, EEUnit* inTarget, bool inArg3, bool inIsConvert)
    : EEUnitGoal(kGoalAttackUnit, inArg3), mIsConvert(inIsConvert)
{
    this->mAttacker = inAttacker;
    EEUnit* theTarget = inTarget;
    if (inTarget->mTileNode != NULL) {
        EEComplexUnit* theComplexUnit = inTarget->mComplexUnit;
        if (inAttacker != NULL && !inAttacker->mPlayer->IsHuman() && !this->mAttacker->mUnitClass->mIsProjectile)
            theComplexUnit->mNumAttackers++;
        if (theComplexUnit->mNextPart != NULL && theComplexUnit->mPreviousPart == NULL && !inAttacker->mUnitClass->IsPriest())
            theTarget = EEGetAttackPart(this->mAttacker, theComplexUnit);
    }
    this->mTarget = theTarget;
    if (theTarget != NULL) {
        theTarget->Reference();
        this->mTargetTileX = theTarget->mTileX;
        this->mTargetTileY = theTarget->mTileY;
    } else {
        this->mTargetTileX = -10000;
        this->mTargetTileY = -10000;
    }
    this->mLocation.SetX(inTarget->GetTileX() + 0.5f);
    this->mLocation.SetY(inTarget->GetTileY() + 0.5f);
    this->mLocation.SetZ(0.0f);
    if (this->mAttacker != NULL && this->mTarget != NULL && this->mTarget->mUnitClass->mFamily != kUnitFamilyAnimal) {
        EEPlayer* thePlayer = this->mAttacker->mPlayer;
        if (thePlayer->mNumAttacks == 0)
            thePlayer->mCombatState = 2;
        thePlayer->mNumAttacks++;
    }
    this->mSidestepCount = 0;
}

// FUNCTION: EXE 0x61d45c
// Returns the goal's location (the target tile's centre, or the last firing position)
GE3DPoint* EEUGAttackUnit::GetLocation()
{
    return &this->mLocation;
}

// FUNCTION: EXE 0x61d460
// Returns check-range as the first action
long EEUGAttackUnit::GetStartAction()
{
    return kActionCheckRange;
}

// FUNCTION: EXE 0x61d466
// Sets the goal's location
void EEUGAttackUnit::SetLocation(const GE3DPoint& inLocation)
{
    this->mLocation = inLocation;
}

// FUNCTION: EXE 0x61d47e
// Sets the path attempt count; 0 also clears the sidestep count
void EEUGAttackUnit::SetPathAttempts(long inValue)
{
    this->mPathAttempts = inValue;
    if (inValue == 0)
        this->mSidestepCount = 0;
}

// SYNTHETIC: EXE 0x61d48f
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x61d4ab
// Destroys the goal: ends one of the player's attacks (combat state 1 at none left), drops the AI attacker
// count and releases the target
EEUGAttackUnit::~EEUGAttackUnit()
{
    if (this->mAttacker != NULL && this->mTarget != NULL && this->mTarget->mUnitClass->mFamily != kUnitFamilyAnimal) {
        EEPlayer* thePlayer = this->mAttacker->mPlayer;
        if (--thePlayer->mNumAttacks == 0)
            thePlayer->mCombatState = 1;
    }
    if (this->mTarget != NULL) {
        if (this->mAttacker != NULL && !this->mAttacker->mPlayer->IsHuman()) {
            EEComplexUnit* theComplexUnit = this->mTarget->mComplexUnit;
            if (theComplexUnit != NULL && !this->mAttacker->mUnitClass->mIsProjectile && theComplexUnit->mNumAttackers > 0)
                theComplexUnit->mNumAttackers--;
        }
        this->mTarget->Dereference();
    }
}

// FUNCTION: EXE 0x61d539
// Aims at a lead position on a moving target, else a firing position; keeps it and the target's tile and tells
// the unit's active group
void EEUGAttackUnit::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    EEComplexUnit* theComplexUnit = this->mTarget->mComplexUnit;
    if (theComplexUnit != NULL && theComplexUnit->mVelocity > 0.0f)
        EEGetLeadPosition(this->mAttacker, this->mTarget, outLocation);
    else
        EEGetFiringPosition(this->mAttacker, this->mTarget, outLocation);
    this->SetLocation(outLocation);
    this->UpdateTargetTile();
    if (inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x61d5a3
// Remembers the target's current tile
void EEUGAttackUnit::UpdateTargetTile()
{
    this->mTargetTileX = this->mTarget->mTileX;
    this->mTargetTileY = this->mTarget->mTileY;
}

// FUNCTION: EXE 0x61d5b3
// Retargets a gate target to the gate part the attacker should hit
void EEUGAttackUnit::RetargetGatePart(EEComplexUnit* inUnit)
{
    if (this->mTarget->mUnitClass->mCategory == kUnitCategoryGate)
        this->SetTarget(EEGetAttackPart(this->mAttacker, this->mTarget->mComplexUnit));
}

// FUNCTION: EXE 0x61d5de
// Writes the location, attacker, target, last target tile and convert flag to the out-of-synch dump, then the
// base goal's
void EEUGAttackUnit::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The Actual Destination is (%6.6f, %6.6f, %6.6f)\n",
            this->mLocation.GetX(), this->mLocation.GetY(), this->mLocation.GetZ());
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The attacking unit #%08x is a %s\n", this->mAttacker->mUnitID,
            (const char*)this->mAttacker->mUnitClass->mName);
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The target unit #%08x is a %s\n",
            this->mTarget ? this->mTarget->mUnitID : -1, this->mTarget ? (const char*)this->mTarget->mUnitClass->mName : "???");
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - The last known target X,Y(%d, %d)\n", this->mTargetTileX,
            this->mTargetTileY);
    fprintf(inFile, "EEUGAttackUnit::DumpOOSInformation - Is this a convert goal? %2d\n", this->mIsConvert != 0);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x61d67e
// Returns a copy of the goal for the same attacker and target
EEUnitGoal* EEUGAttackUnit::Clone()
{
    return new EEUGAttackUnit(this->mAttacker, this->mTarget, this->GetB1C(), this->mIsConvert);
}

// FUNCTION: EXE 0x61d6b4
// Returns the base goal's checksum plus the location's raw bits, the attacker and target ids and the last
// target tile
long EEUGAttackUnit::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    float theValue = this->mLocation.GetX();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mLocation.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mLocation.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theChecksum += this->mAttacker ? this->mAttacker->mUnitID : 0;
    theChecksum += this->mTarget ? this->mTarget->mUnitID : 0;
    theChecksum += this->mTargetTileX + this->mTargetTileY;
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x61d703
// Appends "attack" and the target's id and hit points to the one-line debug text
void EEUGAttackUnit::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    ioText += "attack";
    if (this->mTarget != NULL) {
        sprintf(theText, " unit (uid %d, hp %d):", this->mTarget->mUnitID & 0xffffff, this->mTarget->mHitPoints);
        ioText += theText;
    }
}

// FUNCTION: EXE 0x61d763
// Appends "Attack" and the target's name, tile and health to the debug text
void EEUGAttackUnit::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    ioText += "Attack\n";
    if (this->mTarget == NULL)
        return;
    sprintf(theText, "Target Unit: %s at (%d,%d)\nTarget Health: %d/%d (%.2f%%)\n",
            (const char*)this->mTarget->mUnitClass->mName, this->mTarget->mTileX, this->mTarget->mTileY, this->mTarget->mHitPoints,
            this->mTarget->mStats->mMaxHitPoints, (float)this->mTarget->mHitPoints / this->mTarget->mStats->mMaxHitPoints);
    ioText += theText;
}

// FUNCTION: EXE 0x61d7e2
// Returns whether the target is a gate's middle part whose gate is not closed (despite the name)
bool EEUGAttackUnit::IsGateClosed(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mTarget->mUnitClass->mCategory == kUnitCategoryGate) {
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(this->mTarget->GetUnitClass());
        EEComplexUnit* theComplexUnit = this->mTarget->mComplexUnit;
        if (!theGate->IsSidePart(theComplexUnit)) {
            EEComplexUnit* theCarrier = theComplexUnit;
            while (theCarrier->mPreviousPart != NULL)
                theCarrier = theCarrier->mPreviousPart;
            if ((theCarrier->mCommand ? theCarrier->mCommand->mType : 0) != kActionClosed)
                theResult = true;
            else
                theResult = false;
        } else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61d857
// Returns whether the target is still reachable (melee units must reach it; at most 25 path attempts); if not,
// the unit starts ignoring it
bool EEUGAttackUnit::IsStillReachable(EEComplexUnit* inUnit)
{
    U2DPoint<long> theTile;
    bool theResult;
    if (!inUnit->mUnitClass->mIsMelee || (theResult = inUnit->CanReachUnit(this->mTarget)) != false) {
        theResult = EEIsAttackReachable(inUnit, this->mTarget->GetComplexUnit(), &theTile);
        if (theResult) {
            theResult = (unsigned long)this->mPathAttempts <= 0x19;
            if (theResult)
                return theResult;
        }
    }
    EEComplexUnit* theComplexUnit = this->mTarget->mComplexUnit;
    if (theComplexUnit != NULL && !inUnit->IsTargetIgnored(theComplexUnit, true))
        inUnit->IgnoreTarget(theComplexUnit);
    return theResult;
}

// FUNCTION: EXE 0x61d8d7
// Returns the path attempt count plus one
long EEUGAttackUnit::GetMaxPathAttempts()
{
    return this->mPathAttempts + 1;
}

// FUNCTION: EXE 0x61d8dc
// Retargets gate parts, faces the target (turning animation 0xd), or fires: attack animation, a shot on its
// frame else a randomised reload timer; drops the path and resets the path attempts
void EEUGAttackUnit::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionChangeTargetForSpecialEnemy:
        this->RetargetGatePart(inUnit);
        break;
    case kActionFaceEnemyUnit:
        inUnit->SetCurrentPhysicsAction(1);
        if (!inUnit->mUnitClass->FacesTarget(inUnit, this->mTarget, false) && inUnit->mAnimation != 0xd)
            inUnit->SetAnimation(0xd, -1, 0);
        inUnit->DropPath();
        break;
    case kActionAttackEnemyUnit: {
            inUnit->SetCurrentPhysicsAction(1);
            long theSet = EEGetAttackFrameSet(inUnit);
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

// FUNCTION: EXE 0x61d9d3
// Sets the attacking unit
void EEUGAttackUnit::SetOwner(EEComplexUnit* inAttacker)
{
    this->mAttacker = inAttacker;
}

// FUNCTION: EXE 0x61d9dd
// Reads the target and attacker ids, last target tile and (from version 0.238) the convert flag; returns the first
// read error
long EEUGAttackUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID;
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        theResult = inFile.Read(theUnitID);
        if (theResult == 0)
            this->mTarget = reinterpret_cast<EEUnit*>(theUnitID);
    }
    // a failed base goal or target id read is reported (the attacker id read used to replace it)
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(theUnitID);
    if (theResult != 0)
        return theResult;
    this->mAttacker = reinterpret_cast<EEComplexUnit*>(theUnitID);
    theResult = inFile.Read(this->mTargetTileX);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTargetTileY);
    if (theResult != 0)
        return theResult;
    if (inMajorVersion <= 0 && inMinorVersion < 0xee)
        this->mIsConvert = false;
    else
        theResult = inFile.Read(this->mIsConvert);
    return theResult;
}

// FUNCTION: EXE 0x61da63
// Turns the loaded ids back into target and attacker, referencing the target and counting the player's attack;
// returns 3 and clears both when the target is gone
long EEUGAttackUnit::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 3;
    if (reinterpret_cast<unsigned long>(this->mTarget) != -1) {
        this->mTarget = static_cast<EEUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget)));
        if (this->mTarget != NULL) {
            this->mTarget->Reference();
            theResult = 0;
            if (this->mTargetTileX == 0 && this->mTargetTileY == 0) {
                this->mTargetTileX = this->mTarget->mTileX;
                this->mTargetTileY = this->mTarget->mTileY;
            }
            if (reinterpret_cast<unsigned long>(this->mAttacker) != -1) {
                this->mAttacker = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mAttacker)));
                if (this->mAttacker != NULL) {
                    EEPlayer* thePlayer = this->mAttacker->mPlayer;
                    if (thePlayer->mNumAttacks == 0)
                        thePlayer->mCombatState = 2;
                    thePlayer->mNumAttacks++;
                    theResult = 0;
                }
            } else
                this->mAttacker = NULL;
        } else {
            this->mTarget = NULL;
            this->mAttacker = NULL;
        }
    } else {
        this->mTarget = NULL;
        this->mAttacker = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x61db0d
// Writes the target id (-1 when dead or none), attacker id, last target tile and convert flag; returns the
// first write error
long EEUGAttackUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTarget != NULL && this->mTarget->mHitPoints > 0 ? this->mTarget->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAttacker ? this->mAttacker->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTargetTileX);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTargetTileY);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsConvert);
    return theResult;
}

// FUNCTION: EXE 0x61dbaa
// Switches the target, moving the reference and remembering its tile (-10000 when none)
void EEUGAttackUnit::SetTarget(EEUnit* inTarget)
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = inTarget;
    if (inTarget != NULL) {
        ESVERIFY_AT(::EEUUGetIsUnitClassTargetable(this->mAttacker, inTarget->GetClass()) == true, 729);
        ESVERIFY_AT(inTarget->GetClass()->GetWeapon() >= 0 && inTarget->GetClass()->GetWeapon() < keeucwTotal, 730);
        this->mTarget->Reference();
        this->mTargetTileX = inTarget->mTileX;
        this->mTargetTileY = inTarget->mTileY;
    } else {
        this->mTargetTileX = -10000;
        this->mTargetTileY = -10000;
    }
}

// FUNCTION: EXE 0x61dc35
// Answers the attack-unit triggers (target destroyed, hidden, out of sight or range, moving, reachable, cease
// fire ...); a living destroyed-test falls through to the hidden test; others go to the common goal
bool EEUGAttackUnit::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    bool theResult = false;
    EEUnit* theArg3 = NULL;
    long theArg4;
    long theArg5;
    switch (inEvent) {
    case kTriggerEnemyUnitDestroyed:
        if (this->mTarget->mHitPoints <= 0 || this->mTarget->mTileX == -10000) {
            theResult = true;
            break;
        }
    case kTriggerEnemyUnitUndetectable:
        if (this->mTarget != NULL && this->mTarget->mComplexUnit->mIsCloakable && this->mTarget->mComplexUnit->GetIsCloaked()
            && this->mTarget->mPlayer != inUnit->mPlayer && inUnit->mPlayer->mEnemies[this->mTarget->mPlayer->mPlayerID] != 0
            && this->mTarget->mUnitClass->mCloakRadius == 0)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerUnitNotOnMap:
        theResult = this->mTarget->mTileX == -10000;
        break;
    case kTriggerUnitInWeaponRange:
        if (this->mIsConvert && inUnit->mUnitClass->IsCrusader())
            theResult = EEIsInRangeToHit(inUnit, this->mTarget);
        else
            theResult = inUnit->IsInReach(this->mTarget);
        break;
    case kTriggerEnemyUnitNoLongerVisible:
        if ((inUnit->mPlayer->IsHuman() || inUnit->mStats->GetRange() > inUnit->GetLineOfSight())
            && !EEIsSeenBy(this->mTarget, inUnit->GetPlayer(), false))
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerEnemyUnitInsideFiringArch:
        theResult = inUnit->GetUnitClass()->FacesTarget(inUnit, this->mTarget, true);
        break;
    case kTriggerEnemyUnitLeftLOS: {
            float theRange = inUnit->mStats->mRange;
            float theMinimum = inUnit->GetLineOfSight();
            if (theRange > theMinimum)
                theResult = !EEIsSeenBy(this->mTarget, inUnit->GetPlayer(), false);
            else if (inUnit->DistanceSquaredToUnit(this->mTarget) > theMinimum * theMinimum)
                theResult = true;
            else
                theResult = false;
        }
        break;
    case kTriggerEnemyUnitMoving: {
            EEComplexUnit* theComplexUnit = this->mTarget->mComplexUnit;
            if (theComplexUnit != NULL && theComplexUnit->mVelocity > 0.0f
                && !inUnit->GetUnitClass()->FacesTarget(inUnit, theComplexUnit, true))
                theResult = true;
            else
                theResult = false;
        }
        break;
    case kTriggerFacingEnemyUnit:
        theResult = inUnit->GetUnitClass()->FacesTarget(inUnit, this->mTarget, false);
        break;
    case kTriggerReadyToFireAgain:
        theResult = inUnit->mReloadTimer >= inUnit->mStats->mReloadTime;
        break;
    case kTriggerUnitOnTileBorderingTarget:
        theResult = inUnit->IsOnTileBorderingTarget(this->mTarget);
        break;
    case kTriggerAttackerIsRangedUnit:
        if (inUnit->mStats->mRange > 1.0f)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerTargetIsNotDefensiveBuilding:
        if (this->mTarget != NULL && this->mTarget->mUnitClass->IsLandBuilding() && this->mTarget->mStats->mAttack == 0.0f)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerBlockedByWall:
        if (!inUnit->mStats->mCanFireOverWalls)
            theResult = EEIsLineToUnitBlocked(inUnit, this->mTarget, &theArg3, &theArg4, &theArg5);
        break;
    case kTriggerEnemyIsBuilding:
        theResult = this->mTarget->mUnitClass->mFamily == kUnitFamilyBuilding;
        break;
    case kTriggerCeaseFire:
        if (this->mTarget->mPlayer != inUnit->mPlayer && inUnit->mPlayer->mEnemies[this->mTarget->mPlayer->mPlayerID] != 0)
            theResult = false;
        else if (inUnit->mHerd == NULL)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerEnemyUnitMoved:
        if (this->mTarget->mTileX != this->mTargetTileX || this->mTarget->mTileY != this->mTargetTileY)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerTargetIsSpecialEnemy:
        theResult = this->IsGateClosed(inUnit);
        break;
    case kTriggerEnemyUnitHasNotMovedOneTile:
        if (this->mTarget->mTileX == this->mTargetTileX && this->mTarget->mTileY == this->mTargetTileY)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerEnemyUnitReachable:
        theResult = this->IsStillReachable(inUnit);
        break;
    case kTriggerEnemyUnitInLOS:
        theResult = EEIsEnemyWithinRange(inUnit);
        break;
    default:
        theResult = EEUnitGoal::EvaluateCondition(inUnit, inEvent);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x61e01e
// Replaces the attacker, or a target whose complex unit is the old unit, with the new unit (moving the reference)
void EEUGAttackUnit::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (inOld == this->mAttacker) {
        this->mAttacker = inNew;
    } else if (this->mTarget != NULL && this->mTarget->mComplexUnit != NULL && this->mTarget->mComplexUnit == inOld) {
        this->mTarget->Dereference();
        this->mTarget = inNew;
        this->mTarget->Reference();
    }
}

// FUNCTION: EXE 0x61e058
// Returns whether the goal's target is an attackable enemy of the unit that its ammunition can damage (any
// enemy when it has no ammunition type)
bool EEUGAttackUnit::Suits(EEUnit* inUnit)
{
    bool theResult = false;
    if (this->mTarget != NULL && this->mTarget->mPlayer != inUnit->mPlayer
        && inUnit->mPlayer->mEnemies[this->mTarget->mPlayer->mPlayerID] != 0) {
        long theKind = inUnit->mUnitClass->mAmmoType;
        if (theKind >= 0) {
            EEUnitClass* theClass = this->mTarget->mUnitClass;
            long theTargetKind = theClass->mFamily;
            if (theClass->CanBeAttacked() && gU4E8708[theTargetKind][theKind] > 0.0f)
                theResult = true;
            else
                theResult = false;
        } else
            theResult = true;
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x61d0b4
// Returns the goal's location
void EEUGAttackUnit::GetGoalLocation(GE3DPoint& outLocation)
{
    outLocation = this->mLocation;
}

// ICF-FOLDED: EXE 0x61d1db
// Starts the unit's initial attack state and lets its unit class set the action up
void EEUGAttackUnit::Start(EEComplexUnit* inUnit)
{
    inUnit->StartAction(kActionInitialAttackState);
    inUnit->mUnitClass->ActionChanged(inUnit);
}
