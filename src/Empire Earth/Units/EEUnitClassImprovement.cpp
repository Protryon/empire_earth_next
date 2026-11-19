// C:\EEX\Empire Earth\Units\EEUnitClassImprovement.cpp (file name inferred: no __FILE__; the class name). A unit
// class's improvement levels (EEUnitClassUpgrade::mImprovement): 0x23a5bf..0x23b61a (after EEUnitClass.cpp;
// EEUnitClassUpgrade.cpp follows). Unit flags: the exe's module set. Improvement kinds (ee/EEImprovementIDs.h):
// 0 area effect, 1 attack, 2..7 the armours, 8 flight time or power, 9 hit points, 10 leadership, 11 range,
// 12 repair, 14 speed, 15 transport; 13 is unused. New (port, R5b.4 round 20): the arithmetic is Rust (ee_units
// improve.rs: CanUpgrade 0x23a5fe, GetBaseValue 0x23a77d, the gain of GetGainText 0x23a8a8, GetCost 0x23aee3, GetStep
// 0x23aef9, AddIntSteps 0x23af3c, AddFloatSteps 0x23af79, ChangeLevel 0x23afc1's levels and stats) over the class's
// fields and the upgrade's stats as records; from round 28 those are read and written in place (the class's and the
// upgrade's Rust records, SetRange and SetMaxVelocity with them); here the step factors, the names, Load and Save.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUnit.h"
#include "ee/EEStringIDs.h"
#include "ee/EEImprovementIDs.h"

namespace {

// New (R5b.4, round 28): the class's improvement table's step factors for the Rust (null: no table); the class's and
// the upgrade's fields are read and written in place (ee_units improve.rs over the records)
const float* StepFactors(EEUnitClass* inClass, float* outFactors)
{
    if (inClass->mImprovements == NULL)
        return NULL;
    for (int i = 0; i < 16; i++)
        outFactors[i] = inClass->mImprovements->mEntries[i].mStepFactor;
    return outFactors;
}

}  // namespace

// Creates the improvement levels with every improvement at level 1 and no points spent
EEUnitClassImprovement::EEUnitClassImprovement()
{
    for (long i = 0; i < 16; i++)
        this->mLevels[i] = 1;
    this->mTotalCost = 0;
}

// Destroys the improvement levels (nothing to release)
EEUnitClassImprovement::~EEUnitClassImprovement()
{
}

// Returns whether an improvement applies to a unit class: its upgrade flag, or a base stat above 0 (range above 1)
bool EEUnitClassImprovement::CanUpgrade(EEUnitClass* inUnitClass, long inImprovement)
{
    return ee_units_improve_can_upgrade_class(inUnitClass, (int32_t)inImprovement);
}

// Raises another record's improvements by this record's levels above 1, one level at a time, moving its upgrade's stats
void EEUnitClassImprovement::CopyLevelsTo(EEUnitClassUpgrade* inUpgrade, EEUnitClassImprovement* inTarget)
{
    for (unsigned long i = 0; i < 16; i++) {
        for (long j = 0; j < this->mLevels[i] - 1; j++)
            inTarget->ChangeLevel(inUpgrade->mUnitClass, inUpgrade, i, 1);
    }
}

// Returns the level of an improvement
long EEUnitClassImprovement::GetLevel(long inIndex)
{
    return this->mLevels[inIndex];
}

// Builds the upgrade button's " + n" help text: the stat gain of the improvement's step to level inLevel + 1
void EEUnitClassImprovement::GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement,
                                      UWideString& outText)
{
    float theFactors[16];
    long theDelta = ee_units_improve_gain_records(inUpgrade->mUnitClass, StepFactors(inUpgrade->mUnitClass, theFactors),
                                                  inUpgrade, this->mLevels, (int32_t)inLevel, (int32_t)inImprovement);
    outText = " + ";
    char theBuffer[64];
    PlatIToA(theDelta, theBuffer, 10);
    outText += theBuffer;
}

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

// Returns the improvement points one level costs: 1 for the armour kinds (2..7), else 2
long EEUnitClassImprovement::GetCost(long inImprovement)
{
    return ee_units_improve_cost((int32_t)inImprovement);
}

// Raises (inDelta > 0) or lowers an improvement's level and points spent and moves the upgrade's stat as many steps
// (range also moves line of sight, hit points stay at least 1); does nothing without the class's improvement table
void EEUnitClassImprovement::ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement,
                                      long inDelta)
{
    float theFactors[16];
    int32_t theTotal = (int32_t)this->mTotalCost;
    ee_units_improve_change_level_records(inUnitClass, StepFactors(inUnitClass, theFactors), inUpgrade, inUpgrade->mUnitClass,
                                          inUpgrade->mUnitClass->mProjectileClass, this->mLevels, &theTotal,
                                          (int32_t)inImprovement, (int32_t)inDelta);
    this->mTotalCost = theTotal;
}

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

// Writes the levels and the points spent to the saved game; returns the first write error
long EEUnitClassImprovement::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theWritten;
    long theResult = outFile.Write((const char*)this->mLevels, 0x40, theWritten);
    if (theResult == 0)
        theResult = outFile.Write(this->mTotalCost);
    return theResult;
}
