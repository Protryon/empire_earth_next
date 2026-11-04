// C:\EEX\Empire Earth\Units\EEUnitClassImprovement.cpp (file name inferred: no __FILE__; the class name). A unit
// class's improvement levels (EEUnitClassUpgrade::mImprovement): 0x23a5bf..0x23b61a (after EEUnitClass.cpp;
// EEUnitClassUpgrade.cpp follows). Unit flags: the exe's module set. Improvement kinds (ee/EEImprovementIDs.h):
// 0 area effect, 1 attack, 2..7 the armours, 8 flight time or power, 9 hit points, 10 leadership, 11 range,
// 12 repair, 14 speed, 15 transport; 13 is unused
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassImprovement.h"
#include "ee/EEStringIDs.h"
#include "ee/EEImprovementIDs.h"
// FUNCTION: EXE 0x63a5bf
// Creates the improvement levels with every improvement at level 1 and no points spent
EEUnitClassImprovement::EEUnitClassImprovement()
{
    for (long i = 0; i < 16; i++)
        this->mLevels[i] = 1;
    this->mTotalCost = 0;
}

// 0x449ad0 (after the vtable 0x449acc): the round-up added before the float levels are truncated (GetGainText)
static const float kRoundUp = 0.5001f;

// SYNTHETIC: EXE 0x63a5db
// (EEUnitClassImprovement's scalar deleting destructor)

// FUNCTION: EXE 0x63a5f7
// Destroys the improvement levels (nothing to release)
EEUnitClassImprovement::~EEUnitClassImprovement()
{
}

// FUNCTION: EXE 0x63a5fe
// Returns whether an improvement applies to a unit class: its upgrade flag, or a base stat above 0 (range above 1)
bool EEUnitClassImprovement::CanUpgrade(EEUnitClass* inUnitClass, long inImprovement)
{
    bool theResult;
    switch (inImprovement) {
    case kImprovementAreaEffect:
        theResult = inUnitClass->mUpgradesAreaEffect;
        break;
    case kImprovementAttack:
        theResult = inUnitClass->mAttack > 0;
        break;
    case kImprovementArrowArmor:
        theResult = inUnitClass->mUpgradesArrowArmor;
        break;
    case kImprovementGunArmor:
        theResult = inUnitClass->mUpgradesGunArmor;
        break;
    case kImprovementLaserArmor:
        theResult = inUnitClass->mUpgradesLaserArmor;
        break;
    case kImprovementMissileArmor:
        theResult = inUnitClass->mUpgradesMissileArmor;
        break;
    case kImprovementPierceArmor:
        theResult = inUnitClass->mUpgradesPierceArmor;
        break;
    case kImprovementShockArmor:
        theResult = inUnitClass->mUpgradesShockArmor;
        break;
    case kImprovementFlightTimeOrPower:
        if (inUnitClass->mFlightTime > 0 || inUnitClass->mPower > 0)
            theResult = true;
        else
            theResult = false;
        break;
    case kImprovementHitPoints:
        theResult = inUnitClass->mHitPoints > 0;
        break;
    case kImprovementLeadership:
        theResult = inUnitClass->mLeadership > 0;
        break;
    case kImprovementRange:
        if (inUnitClass->mRange > 1.0f)
            theResult = true;
        else
            theResult = false;
        break;
    case kImprovementRepair:
        theResult = inUnitClass->mUpgradesRepair;
        break;
    case kImprovementSpeed:
        if (inUnitClass->mMaxVelocity > 0.0f)
            theResult = true;
        else
            theResult = false;
        break;
    case kImprovementTransport:
        theResult = inUnitClass->mTransportCapacity > 0;
        break;
    default:
        theResult = false;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x63a737
// Raises another record's improvements by this record's levels above 1, one level at a time, moving its upgrade's stats
void EEUnitClassImprovement::CopyLevelsTo(EEUnitClassUpgrade* inUpgrade, EEUnitClassImprovement* inTarget)
{
    for (unsigned long i = 0; i < 16; i++) {
        for (long j = 0; j < this->mLevels[i] - 1; j++)
            inTarget->ChangeLevel(inUpgrade->mUnitClass, inUpgrade, i, 1);
    }
}

// FUNCTION: EXE 0x63a77d
// Returns a unit class's base stat for an improvement (flight time when it has one, else power; 0 for unknown kinds)
float EEUnitClassImprovement::GetBaseValue(EEUnitClass* inUnitClass, long inImprovement)
{
    float theResult;
    switch (inImprovement) {
    case kImprovementAreaEffect:
        theResult = inUnitClass->mAreaEffect;
        break;
    case kImprovementAttack:
        theResult = (float)inUnitClass->mAttack;
        break;
    case kImprovementArrowArmor:
        theResult = (float)inUnitClass->mArmor[1];
        break;
    case kImprovementGunArmor:
        theResult = (float)inUnitClass->mArmor[3];
        break;
    case kImprovementLaserArmor:
        theResult = (float)inUnitClass->mArmor[4];
        break;
    case kImprovementMissileArmor:
        theResult = (float)inUnitClass->mArmor[5];
        break;
    case kImprovementPierceArmor:
        theResult = (float)inUnitClass->mArmor[2];
        break;
    case kImprovementShockArmor:
        theResult = (float)inUnitClass->mArmor[0];
        break;
    case kImprovementFlightTimeOrPower:
        theResult = (float)(inUnitClass->mFlightTime > 0 ? inUnitClass->mFlightTime : inUnitClass->mPower);
        break;
    case kImprovementHitPoints:
        theResult = (float)inUnitClass->mHitPoints;
        break;
    case kImprovementLeadership:
        theResult = (float)inUnitClass->mLeadership;
        break;
    case kImprovementRange:
        theResult = inUnitClass->mRange;
        break;
    case kImprovementRepair:
        theResult = (float)inUnitClass->mRepairRate;
        break;
    case kImprovementSpeed:
        theResult = inUnitClass->mMaxVelocity;
        break;
    case kImprovementTransport:
        theResult = (float)inUnitClass->mCarryAmount;
        break;
    default:
        theResult = 0.0f;
    }
    return theResult;
}

// FUNCTION: EXE 0x63a89d
// Returns the level of an improvement
long EEUnitClassImprovement::GetLevel(long inIndex)
{
    return this->mLevels[inIndex];
}

// FUNCTION: EXE 0x63a8a8
// Builds the upgrade button's " + n" help text: the stat gain of the improvement's step to level inLevel + 1
// (flight time in seconds, area effect in tenths, speed in twentieths; 0 when the class has no improvement table)
void EEUnitClassImprovement::GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement,
                                      UWideString& outText)
{
    long theCount = inLevel - this->mLevels[inImprovement] + 1;
    EEUnitClass* theClass = inUpgrade->mUnitClass;
    long theDelta = 0;
    bool theNegative;
    if (theCount < 0) {
        theNegative = true;
        theCount = -theCount;
    } else
        theNegative = false;
    if (theClass->mImprovements != NULL) {
        switch (inImprovement) {
        case kImprovementAreaEffect:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementAreaEffect, inUpgrade->GetAreaEffect(), 0.1f, theCount, theNegative) * 10.0f + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementAreaEffect, inUpgrade->GetAreaEffect(), 0.1f, theCount - 1, theNegative) * 10.0f + kRoundUp);
            break;
        case kImprovementAttack:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementAttack, inUpgrade->GetAttack(), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementAttack, inUpgrade->GetAttack(), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementArrowArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementArrowArmor, inUpgrade->GetArmor(1), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementArrowArmor, inUpgrade->GetArmor(1), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementGunArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementGunArmor, inUpgrade->GetArmor(3), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementGunArmor, inUpgrade->GetArmor(3), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementLaserArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementLaserArmor, inUpgrade->GetArmor(4), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementLaserArmor, inUpgrade->GetArmor(4), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementMissileArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementMissileArmor, inUpgrade->GetArmor(5), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementMissileArmor, inUpgrade->GetArmor(5), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementPierceArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementPierceArmor, inUpgrade->GetArmor(2), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementPierceArmor, inUpgrade->GetArmor(2), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementShockArmor:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementShockArmor, inUpgrade->GetArmor(0), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementShockArmor, inUpgrade->GetArmor(0), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementFlightTimeOrPower:
            if (theClass->mFlightTime > 0)
                theDelta = this->AddIntSteps(theClass, kImprovementFlightTimeOrPower, inUpgrade->mFlightTime, 1, theCount, theNegative) / 1000 +
                           this->AddIntSteps(theClass, kImprovementFlightTimeOrPower, inUpgrade->mFlightTime, 1, theCount - 1, theNegative) / -1000;
            else
                theDelta = this->AddIntSteps(theClass, kImprovementFlightTimeOrPower, inUpgrade->mPower, 1, theCount, theNegative) -
                           this->AddIntSteps(theClass, kImprovementFlightTimeOrPower, inUpgrade->mPower, 1, theCount - 1, theNegative);
            break;
        case kImprovementHitPoints:
            theDelta = this->AddIntSteps(theClass, kImprovementHitPoints, inUpgrade->mMaxHitPoints, 1, theCount, theNegative) -
                       this->AddIntSteps(theClass, kImprovementHitPoints, inUpgrade->mMaxHitPoints, 1, theCount - 1, theNegative);
            break;
        case kImprovementLeadership:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementLeadership, inUpgrade->GetLeadership(), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementLeadership, inUpgrade->GetLeadership(), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementRange:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementRange, inUpgrade->GetRange(), 1.0f, theCount, theNegative) + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementRange, inUpgrade->GetRange(), 1.0f, theCount - 1, theNegative) + kRoundUp);
            break;
        case kImprovementRepair:
            theDelta = this->AddIntSteps(theClass, kImprovementRepair, inUpgrade->mRepairRate, 1, theCount, theNegative) -
                       this->AddIntSteps(theClass, kImprovementRepair, inUpgrade->mRepairRate, 1, theCount - 1, theNegative);
            break;
        case kImprovementSpeed:
            theDelta = (long)(this->AddFloatSteps(theClass, kImprovementSpeed, inUpgrade->GetMaxVelocity(), 0.05f, theCount, theNegative) * 20.0f + kRoundUp);
            theDelta -= (long)(this->AddFloatSteps(theClass, kImprovementSpeed, inUpgrade->GetMaxVelocity(), 0.05f, theCount - 1, theNegative) * 20.0f + kRoundUp);
            break;
        case kImprovementTransport:
            theDelta = this->AddIntSteps(theClass, kImprovementTransport, inUpgrade->mCarryAmount, 1, theCount, theNegative) -
                       this->AddIntSteps(theClass, kImprovementTransport, inUpgrade->mCarryAmount, 1, theCount - 1, theNegative);
            break;
        }
    }
    outText = " + ";
    char theBuffer[64];
    _itoa(theDelta, theBuffer, 10);
    outText += theBuffer;
}

// FUNCTION: EXE 0x63adbd
// Returns an improvement's display name (flight time or power by the class; empty for unknown kinds)
void EEUnitClassImprovement::GetName(EEUnitClass* inUnitClass, long inImprovement, UWideString& outName)
{
    switch (inImprovement) {
    case kImprovementAreaEffect:
        gResource->GetString(IDS_AREA_EFFECT, outName);
        break;
    case kImprovementAttack:
        gResource->GetString(IDS_IMPROVEMENT_ATTACK, outName);
        break;
    case kImprovementArrowArmor:
        gResource->GetString(IDS_ARROW, outName);
        break;
    case kImprovementGunArmor:
        gResource->GetString(IDS_GUN, outName);
        break;
    case kImprovementLaserArmor:
        gResource->GetString(IDS_LASER, outName);
        break;
    case kImprovementMissileArmor:
        gResource->GetString(IDS_MISSILE, outName);
        break;
    case kImprovementPierceArmor:
        gResource->GetString(IDS_PIERCE, outName);
        break;
    case kImprovementShockArmor:
        gResource->GetString(IDS_SHOCK, outName);
        break;
    case kImprovementFlightTimeOrPower:
        gResource->GetString(inUnitClass->mFlightTime > 0 ? IDS_IMPROVEMENT_FLIGHT_TIME : IDS_IMPROVEMENT_POWER, outName);
        break;
    case kImprovementHitPoints:
        gResource->GetString(IDS_IMPROVEMENT_HIT_POINTS, outName);
        break;
    case kImprovementLeadership:
        gResource->GetString(IDS_LEADERSHIP, outName);
        break;
    case kImprovementRange:
        gResource->GetString(IDS_IMPROVEMENT_RANGE, outName);
        break;
    case kImprovementRepair:
        gResource->GetString(IDS_IMPROVEMENT_REPAIR, outName);
        break;
    case kImprovementSpeed:
        gResource->GetString(IDS_IMPROVEMENT_SPEED, outName);
        break;
    case kImprovementTransport:
        gResource->GetString(IDS_TRANSPORT, outName);
        break;
    default:
        outName = "";
        break;
    }
}

// FUNCTION: EXE 0x63aee3
// Returns the improvement points one level costs: 1 for the armour kinds (2..7), else 2
long EEUnitClassImprovement::GetCost(long inImprovement)
{
    long theResult = 2;
    if ((unsigned long)inImprovement >= 2 && (unsigned long)inImprovement <= 7)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x63aef9
// Returns one level's stat step for a unit class: its improvement table factor (0.1 when unset, 0 for repair) times
// the class's base value
float EEUnitClassImprovement::GetStep(EEUnitClass* inUnitClass, long inImprovement)
{
    float theFactor = 0.0f;
    if (inImprovement != 12) {
        theFactor = inUnitClass->mImprovements->mEntries[inImprovement].mStepFactor;
        if (theFactor == 0.0f)
            theFactor = 0.1f;
    }
    return this->GetBaseValue(inUnitClass, inImprovement) * theFactor;
}

// FUNCTION: EXE 0x63af3c
// Returns an integer stat moved inCount steps up or down (the step truncated, at least inMinimum)
long EEUnitClassImprovement::AddIntSteps(EEUnitClass* inUnitClass, long inImprovement, long inValue, long inMinimum,
                                      long inCount, bool inNegative)
{
    long theStep = (long)this->GetStep(inUnitClass, inImprovement);
    if (theStep < inMinimum)
        theStep = inMinimum;
    theStep *= inNegative ? -1 : 1;
    if (inCount > 0)
        inValue += theStep * inCount;
    return inValue;
}

// FUNCTION: EXE 0x63af79
// Returns a float stat moved inCount steps up or down (the step at least inMinimum)
float EEUnitClassImprovement::AddFloatSteps(EEUnitClass* inUnitClass, long inImprovement, float inValue,
                                       float inMinimum, long inCount, bool inNegative)
{
    float theStep = this->GetStep(inUnitClass, inImprovement);
    if (theStep < inMinimum)
        theStep = inMinimum;
    theStep *= !inNegative ? 1.0f : -1.0f;
    for (long i = inCount; i > 0; i--)
        inValue += theStep;
    return inValue;
}

// FUNCTION: EXE 0x63afc1
// Raises (inDelta > 0) or lowers an improvement's level and points spent and moves the upgrade's stat as many steps
// (range also moves line of sight, hit points stay at least 1); does nothing without the class's improvement table
void EEUnitClassImprovement::ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement,
                                      long inDelta)
{
    long theCost = this->GetCost(inImprovement);
    if (inUnitClass->mImprovements == NULL)
        return;
    this->mLevels[inImprovement] += inDelta;
    this->mTotalCost += theCost * inDelta;
    bool theNegative;
    if (inDelta < 0) {
        theNegative = true;
        inDelta = -inDelta;
    } else {
        theNegative = false;
    }
    switch (inImprovement) {
    case kImprovementAreaEffect:
        inUpgrade->mAreaEffect = this->AddFloatSteps(inUnitClass, kImprovementAreaEffect, inUpgrade->GetAreaEffect(), 0.1f, inDelta, theNegative);
        break;
    case kImprovementAttack:
        inUpgrade->mAttack = this->AddFloatSteps(inUnitClass, kImprovementAttack, inUpgrade->GetAttack(), 1.0f, inDelta, theNegative);
        break;
    case kImprovementArrowArmor:
        inUpgrade->mArmor[1] = this->AddFloatSteps(inUnitClass, kImprovementArrowArmor, inUpgrade->GetArmor(1), 1.0f, inDelta, theNegative);
        break;
    case kImprovementGunArmor:
        inUpgrade->mArmor[3] = this->AddFloatSteps(inUnitClass, kImprovementGunArmor, inUpgrade->GetArmor(3), 1.0f, inDelta, theNegative);
        break;
    case kImprovementLaserArmor:
        inUpgrade->mArmor[4] = this->AddFloatSteps(inUnitClass, kImprovementLaserArmor, inUpgrade->GetArmor(4), 1.0f, inDelta, theNegative);
        break;
    case kImprovementMissileArmor:
        inUpgrade->mArmor[5] = this->AddFloatSteps(inUnitClass, kImprovementMissileArmor, inUpgrade->GetArmor(5), 1.0f, inDelta, theNegative);
        break;
    case kImprovementPierceArmor:
        inUpgrade->mArmor[2] = this->AddFloatSteps(inUnitClass, kImprovementPierceArmor, inUpgrade->GetArmor(2), 1.0f, inDelta, theNegative);
        break;
    case kImprovementShockArmor:
        inUpgrade->mArmor[0] = this->AddFloatSteps(inUnitClass, kImprovementShockArmor, inUpgrade->GetArmor(0), 1.0f, inDelta, theNegative);
        break;
    case kImprovementFlightTimeOrPower:
        if (inUpgrade->mUnitClass->mFlightTime > 0)
            inUpgrade->mFlightTime = this->AddIntSteps(inUnitClass, kImprovementFlightTimeOrPower, inUpgrade->mFlightTime, 1, inDelta, theNegative);
        else
            inUpgrade->mPower = this->AddIntSteps(inUnitClass, kImprovementFlightTimeOrPower, inUpgrade->mPower, 1, inDelta, theNegative);
        break;
    case kImprovementHitPoints: {
        long theHitPoints = this->AddIntSteps(inUnitClass, kImprovementHitPoints, inUpgrade->mMaxHitPoints, 1, inDelta, theNegative);
        if (theHitPoints <= 1)
            theHitPoints = 1;
        inUpgrade->mMaxHitPoints = theHitPoints;
        break;
    }
    case kImprovementLeadership:
        inUpgrade->mLeadership = this->AddFloatSteps(inUnitClass, kImprovementLeadership, inUpgrade->GetLeadership(), 1.0f, inDelta, theNegative);
        break;
    case kImprovementRange:
        inUpgrade->SetRange(this->AddFloatSteps(inUnitClass, kImprovementRange, inUpgrade->GetRange(), 1.0f, inDelta, theNegative));
        inUpgrade->mLineOfSight = this->AddFloatSteps(inUnitClass, kImprovementRange, inUpgrade->GetLineOfSight(), 1.0f, inDelta, theNegative);
        break;
    case kImprovementRepair: {
        long* theField = &inUpgrade->mRepairRate;
        *theField = this->AddIntSteps(inUnitClass, kImprovementRepair, *theField, 1, inDelta, theNegative);
        break;
    }
    case kImprovementSpeed:
        inUpgrade->SetMaxVelocity(this->AddFloatSteps(inUnitClass, kImprovementSpeed, inUpgrade->GetMaxVelocity(), 0.05f, inDelta, theNegative));
        break;
    case kImprovementTransport: {
        long* theField = &inUpgrade->mCarryAmount;
        *theField = this->AddIntSteps(inUnitClass, kImprovementTransport, *theField, 1, inDelta, theNegative);
        break;
    }
    }
}

// FUNCTION: EXE 0x63b35b
// Reapplies every raised improvement to the upgrade's freshly reset stats (levels end unchanged; the points spent
// are recounted)
void EEUnitClassImprovement::Reapply(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade)
{
    // the points spent are recounted from scratch as the levels are reapplied
    this->mTotalCost = 0;
    for (unsigned long i = 0; i < 16; i++) {
        if (!(this->mLevels[i] > 1))
            continue;
        long theDelta = this->mLevels[i] - 1;
        this->mLevels[i] = 1;
        this->ChangeLevel(inUnitClass, inUpgrade, i, theDelta);
    }
}

// FUNCTION: EXE 0x63b39b
// Reads the levels (and points spent, recounted) from a saved game and applies them to the upgrade's stats; saves
// before minor version 0xd9 drop leadership to level 1; returns the read error
long EEUnitClassImprovement::Load(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade,
                                  unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theRead;
    long theOld[16];
    long theResult;
    unsigned long i;
    if (inMajorVersion == 0) {
        if (inMinorVersion <= 0x17) {
            theResult = inFile.Read((char*)this->mLevels, 0x40, theRead);
            i = 0;
            do {
                if (i != kImprovementLeadership) {
                    if (this->mLevels[i] > 1)
                        this->ChangeLevel(inUnitClass, inUpgrade, i, this->mLevels[i] - 1);
                    theOld[i] = this->mLevels[i];
                } else
                    this->mLevels[10] = 1;
                i++;
            } while (i < 16);
            return theResult;
        }
        if (inMinorVersion <= 0x29) {
            inFile.Read((char*)this->mLevels, 0x40, theRead);
            theResult = inFile.Read(reinterpret_cast<char*>(theOld), 0x40, theRead);
            i = 0;
            do {
                if (i != kImprovementLeadership) {
                    long theLevel = this->mLevels[i];
                    if (theLevel > 1) {
                        this->mLevels[i] = 1;
                        this->ChangeLevel(inUnitClass, inUpgrade, i, theLevel - 1);
                    }
                } else
                    this->mLevels[10] = 1;
                i++;
            } while (i < 16);
            return theResult;
        }
        if (inMinorVersion <= 0x6a) {
            inFile.Read((char*)this->mLevels, 0x40, theRead);
            theResult = inFile.Read(reinterpret_cast<char*>(theOld), 0x40, theRead);
            i = 0;
            do {
                if (i != kImprovementLeadership) {
                    long theLevel = this->mLevels[i];
                    if (theLevel > 1) {
                        this->mLevels[i] = 1;
                        this->ChangeLevel(inUnitClass, inUpgrade, i, theLevel - 1);
                    }
                } else
                    this->mLevels[10] = 1;
                i++;
            } while (i < 16);
            return theResult;
        }
        if (inMinorVersion <= 0xa0) {
            theResult = inFile.Read((char*)this->mLevels, 0x40, theRead);
            i = 0;
            do {
                if (i != kImprovementLeadership) {
                    long theLevel = this->mLevels[i];
                    if (theLevel > 1) {
                        this->mLevels[i] = 1;
                        this->ChangeLevel(inUnitClass, inUpgrade, i, theLevel - 1);
                    }
                } else
                    this->mLevels[10] = 1;
                i++;
            } while (i < 16);
            return theResult;
        }
        if (inMinorVersion <= 0xd8) {
            theResult = inFile.Read((char*)this->mLevels, 0x40, theRead);
            if (theResult == 0) {
                theResult = inFile.Read(this->mTotalCost);
                this->mTotalCost = 0;
            }
            i = 0;
            do {
                if (i != kImprovementLeadership) {
                    long theLevel = this->mLevels[i];
                    if (theLevel > 1) {
                        this->mLevels[i] = 1;
                        this->ChangeLevel(inUnitClass, inUpgrade, i, theLevel - 1);
                    }
                } else
                    this->mLevels[10] = 1;
                i++;
            } while (i < 16);
            return theResult;
        }
    }
    theResult = inFile.Read((char*)this->mLevels, 0x40, theRead);
    if (theResult == 0) {
        theResult = inFile.Read(this->mTotalCost);
        this->mTotalCost = 0;
    }
    for (i = 0; i < 16; i++) {
        long theLevel = this->mLevels[i];
        if (theLevel > 1) {
            this->mLevels[i] = 1;
            this->ChangeLevel(inUnitClass, inUpgrade, i, theLevel - 1);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x63b5e4
// Writes the levels and the points spent to the saved game; returns the first write error
long EEUnitClassImprovement::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theWritten;
    long theResult = outFile.Write((const char*)this->mLevels, 0x40, theWritten);
    if (theResult == 0)
        theResult = outFile.Write(this->mTotalCost);
    return theResult;
}
