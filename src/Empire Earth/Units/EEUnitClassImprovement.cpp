// C:\EEX\Empire Earth\Units\EEUnitClassImprovement.cpp (file name inferred: no __FILE__; the class name). A unit
// class's improvement levels (EEUnitClassUpgrade::mImprovement): 0x23a5bf..0x23b61a (after EEUnitClass.cpp;
// EEUnitClassUpgrade.cpp follows). Unit flags: the exe's module set. Improvement kinds (ee/EEImprovementIDs.h):
// 0 area effect, 1 attack, 2..7 the armours, 8 flight time or power, 9 hit points, 10 leadership, 11 range,
// 12 repair, 14 speed, 15 transport; 13 is unused. New (port, R5b.4 round 20): the arithmetic is Rust (ee_units
// improve.rs: CanUpgrade 0x23a5fe, GetBaseValue 0x23a77d, the gain of GetGainText 0x23a8a8, GetCost 0x23aee3, GetStep
// 0x23aef9, AddIntSteps 0x23af3c, AddFloatSteps 0x23af79, ChangeLevel 0x23afc1's levels and stats) over the class's
// fields and the upgrade's stats as records; here the records, the stats written back, the names, Load and Save.
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

#include "ee/EEUnitStore.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassImprovement.h"
#include "ee/EEStringIDs.h"
#include "ee/EEImprovementIDs.h"
#include "rust/ee_units.h"

namespace {

// the class's fields the arithmetic reads
EeUnitsImproveClass ClassRecord(EEUnitClass* inClass)
{
    EeUnitsImproveClass theRecord = {};
    theRecord.area_effect = inClass->mAreaEffect;
    theRecord.attack = inClass->mAttack;
    for (int i = 0; i < 6; i++)
        theRecord.armor[i] = inClass->mArmor[i];
    theRecord.flight_time = inClass->mFlightTime;
    theRecord.power = inClass->mPower;
    theRecord.hit_points = inClass->mHitPoints;
    theRecord.leadership = inClass->mLeadership;
    theRecord.range = inClass->mRange;
    theRecord.repair_rate = inClass->mRepairRate;
    theRecord.max_velocity = inClass->mMaxVelocity;
    theRecord.carry_amount = inClass->mCarryAmount;
    theRecord.transport_capacity = inClass->mTransportCapacity;
    theRecord.upgrades_area_effect = inClass->mUpgradesAreaEffect;
    // by improvement 2..7: arrow, gun, laser, missile, pierce, shock
    theRecord.upgrades_armor[0] = inClass->mUpgradesArrowArmor;
    theRecord.upgrades_armor[1] = inClass->mUpgradesGunArmor;
    theRecord.upgrades_armor[2] = inClass->mUpgradesLaserArmor;
    theRecord.upgrades_armor[3] = inClass->mUpgradesMissileArmor;
    theRecord.upgrades_armor[4] = inClass->mUpgradesPierceArmor;
    theRecord.upgrades_armor[5] = inClass->mUpgradesShockArmor;
    theRecord.upgrades_repair = inClass->mUpgradesRepair;
    theRecord.has_table = inClass->mImprovements != NULL;
    if (inClass->mImprovements != NULL)
        for (int i = 0; i < 16; i++)
            theRecord.step_factors[i] = inClass->mImprovements->mEntries[i].mStepFactor;
    return theRecord;
}

// the upgrade's stats a level change moves
EeUnitsImproveStats StatsRecord(EEUnitClassUpgrade* inUpgrade)
{
    EeUnitsImproveStats theRecord = {};
    theRecord.area_effect = inUpgrade->GetAreaEffect();
    theRecord.attack = inUpgrade->GetAttack();
    for (int i = 0; i < 6; i++)
        theRecord.armor[i] = inUpgrade->GetArmor(i);
    theRecord.flight_time = inUpgrade->mFlightTime;
    theRecord.power = inUpgrade->mPower;
    theRecord.max_hit_points = inUpgrade->mMaxHitPoints;
    theRecord.leadership = inUpgrade->GetLeadership();
    theRecord.range = inUpgrade->GetRange();
    theRecord.line_of_sight = inUpgrade->GetLineOfSight();
    theRecord.repair_rate = inUpgrade->mRepairRate;
    theRecord.max_velocity = inUpgrade->GetMaxVelocity();
    theRecord.carry_amount = inUpgrade->mCarryAmount;
    theRecord.upgrade_has_flight_time = inUpgrade->mUnitClass->mFlightTime > 0;
    return theRecord;
}

}  // namespace

// FUNCTION: EXE 0x63a5bf
// Creates the improvement levels with every improvement at level 1 and no points spent
EEUnitClassImprovement::EEUnitClassImprovement()
{
    for (long i = 0; i < 16; i++)
        this->mLevels[i] = 1;
    this->mTotalCost = 0;
}

// FUNCTION: EXE 0x63a5f7
// Destroys the improvement levels (nothing to release)
EEUnitClassImprovement::~EEUnitClassImprovement()
{
}

// FUNCTION: EXE 0x63a5fe
// Returns whether an improvement applies to a unit class: its upgrade flag, or a base stat above 0 (range above 1)
bool EEUnitClassImprovement::CanUpgrade(EEUnitClass* inUnitClass, long inImprovement)
{
    EeUnitsImproveClass theClass = ClassRecord(inUnitClass);
    return ee_units_improve_can_upgrade(&theClass, (int32_t)inImprovement);
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

// FUNCTION: EXE 0x63a89d
// Returns the level of an improvement
long EEUnitClassImprovement::GetLevel(long inIndex)
{
    return this->mLevels[inIndex];
}

// FUNCTION: EXE 0x63a8a8
// Builds the upgrade button's " + n" help text: the stat gain of the improvement's step to level inLevel + 1
void EEUnitClassImprovement::GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement,
                                      UWideString& outText)
{
    EeUnitsImproveClass theClass = ClassRecord(inUpgrade->mUnitClass);
    EeUnitsImproveStats theStats = StatsRecord(inUpgrade);
    long theDelta = ee_units_improve_gain(&theClass, &theStats, this->mLevels, (int32_t)inLevel, (int32_t)inImprovement);
    outText = " + ";
    char theBuffer[64];
    PlatIToA(theDelta, theBuffer, 10);
    outText += theBuffer;
}

// FUNCTION: EXE 0x63adbd
// Returns an improvement's display name (flight time or power by the class; empty for unknown kinds)
void EEUnitClassImprovement::GetName(EEUnitClass* inUnitClass, long inImprovement, UWideString& outName)
{
    static const long kNames[16] = {IDS_AREA_EFFECT, IDS_IMPROVEMENT_ATTACK, IDS_ARROW, IDS_GUN, IDS_LASER,
                                    IDS_MISSILE, IDS_PIERCE, IDS_SHOCK, 0, IDS_IMPROVEMENT_HIT_POINTS,
                                    IDS_LEADERSHIP, IDS_IMPROVEMENT_RANGE, IDS_IMPROVEMENT_REPAIR, 0,
                                    IDS_IMPROVEMENT_SPEED, IDS_TRANSPORT};
    if (inImprovement == kImprovementFlightTimeOrPower)
        gResource->GetString(inUnitClass->mFlightTime > 0 ? IDS_IMPROVEMENT_FLIGHT_TIME : IDS_IMPROVEMENT_POWER, outName);
    else if ((unsigned long)inImprovement < 16 && kNames[inImprovement] != 0)
        gResource->GetString(kNames[inImprovement], outName);
    else
        outName = "";
}

// FUNCTION: EXE 0x63aee3
// Returns the improvement points one level costs: 1 for the armour kinds (2..7), else 2
long EEUnitClassImprovement::GetCost(long inImprovement)
{
    return ee_units_improve_cost((int32_t)inImprovement);
}

// FUNCTION: EXE 0x63afc1
// Raises (inDelta > 0) or lowers an improvement's level and points spent and moves the upgrade's stat as many steps
// (range also moves line of sight, hit points stay at least 1); does nothing without the class's improvement table
void EEUnitClassImprovement::ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement,
                                      long inDelta)
{
    EEUnitStatsScope theStatsScope(inUpgrade);   // New (R5b.4, the fourteenth round): the stats table (ee/EEUnitStore.h)
    EeUnitsImproveClass theClass = ClassRecord(inUnitClass);
    EeUnitsImproveStats theStats = StatsRecord(inUpgrade);
    int32_t theTotal = (int32_t)this->mTotalCost;
    bool theMoved = ee_units_improve_change_level(&theClass, &theStats, this->mLevels, &theTotal,
                                                  (int32_t)inImprovement, (int32_t)inDelta);
    this->mTotalCost = theTotal;
    if (!theMoved)
        return;
    switch (inImprovement) {
    case kImprovementAreaEffect:
        inUpgrade->mAreaEffect = theStats.area_effect;
        break;
    case kImprovementAttack:
        inUpgrade->mAttack = theStats.attack;
        break;
    case kImprovementArrowArmor:
    case kImprovementGunArmor:
    case kImprovementLaserArmor:
    case kImprovementMissileArmor:
    case kImprovementPierceArmor:
    case kImprovementShockArmor:
        for (int i = 0; i < 6; i++)
            inUpgrade->mArmor[i] = theStats.armor[i];
        break;
    case kImprovementFlightTimeOrPower:
        if (theStats.upgrade_has_flight_time)
            inUpgrade->mFlightTime = theStats.flight_time;
        else
            inUpgrade->mPower = theStats.power;
        break;
    case kImprovementHitPoints:
        inUpgrade->mMaxHitPoints = theStats.max_hit_points;
        break;
    case kImprovementLeadership:
        inUpgrade->mLeadership = theStats.leadership;
        break;
    case kImprovementRange:
        inUpgrade->SetRange(theStats.range);
        inUpgrade->mLineOfSight = theStats.line_of_sight;
        break;
    case kImprovementRepair:
        inUpgrade->mRepairRate = theStats.repair_rate;
        break;
    case kImprovementSpeed:
        inUpgrade->SetMaxVelocity(theStats.max_velocity);
        break;
    case kImprovementTransport:
        inUpgrade->mCarryAmount = theStats.carry_amount;
        break;
    }
}

// FUNCTION: EXE 0x63b35b
// Reapplies every raised improvement to the upgrade's freshly reset stats (levels end unchanged; the points spent
// are recounted)
void EEUnitClassImprovement::Reapply(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade)
{
    this->mTotalCost = 0;
    for (unsigned long i = 0; i < 16; i++) {
        if (!(this->mLevels[i] > 1))
            continue;
        long theDelta = this->mLevels[i] - 1;
        this->mLevels[i] = 1;
        this->ChangeLevel(inUnitClass, inUpgrade, i, theDelta);
    }
}

static_assert(sizeof(EEUnitClassImprovement::mLevels) == 0x40, "saved levels: 16 4-byte words on every target");

// FUNCTION: EXE 0x63b39b
// Reads the levels (and points spent, recounted) from a saved game and applies them to the upgrade's stats; saves
// up to minor version 0xd8 drop leadership to level 1; returns the read error. The original's five version branches
// read: up to 0x17 the levels; up to 0x6a the levels and 16 old words (the second read's status); up to 0xa0 the
// levels; then the levels and the points spent (cleared)
long EEUnitClassImprovement::Load(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade,
                                  unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theRead;
    int32_t theOld[16];   // 0x40 raw bytes, as mLevels
    long theResult;
    bool theOldSave = inMajorVersion == 0 && inMinorVersion <= 0xd8;
    if (theOldSave && inMinorVersion > 0x17 && inMinorVersion <= 0x6a) {
        inFile.Read((char*)this->mLevels, 0x40, theRead);
        theResult = inFile.Read(reinterpret_cast<char*>(theOld), 0x40, theRead);
    } else {
        theResult = inFile.Read((char*)this->mLevels, 0x40, theRead);
        if (!(theOldSave && inMinorVersion <= 0xa0) && theResult == 0) {
            theResult = inFile.Read(this->mTotalCost);
            this->mTotalCost = 0;
        }
    }
    for (unsigned long i = 0; i < 16; i++) {
        if (theOldSave && i == kImprovementLeadership) {
            this->mLevels[i] = 1;
            continue;
        }
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
