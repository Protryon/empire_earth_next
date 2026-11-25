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
#include "ee/EEPlayer.h"            // EESavePrimitive
#include "lle/FSFile.h"
#include <string>
#include "rust/sim/units.h"

// (R5b.4, round 28: the class's and the upgrade's fields are read and written in place, ee_units improve.rs over the
// records; the step factors are the class's improvement table's, read there)

// Returns whether an improvement applies to a unit class: its upgrade flag, or a base stat above 0 (range above 1)
bool EEUnitClassImprovement::CanUpgrade(EEUnitClass* inUnitClass, long inImprovement)
{
    return ee_units_improve_can_upgrade_class(inUnitClass, (int32_t)inImprovement);
}

// Raises another record's improvements by this record's levels above 1, one level at a time, moving its upgrade's stats
void EEUnitClassImprovement::CopyLevelsTo(EEUnitClassUpgrade* inUpgrade, EEUnitClassImprovement* inTarget)
{
    ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_COPY_LEVELS, this->mLevels, &this->mTotalCost,
                         EEImprovementContext{inTarget, inUpgrade->mUnitClass, inUpgrade, NULL}, &EEImprovementHost, 0, 0);
}

// Builds the upgrade button's " + n" help text: the stat gain of the improvement's step to level inLevel + 1
void EEUnitClassImprovement::GetGainText(EEUnitClassUpgrade* inUpgrade, long inLevel, long inImprovement,
                                      UWideString& outText)
{
    outText = (" + " + std::to_string(ee_units_improve_gain_records(inUpgrade->mUnitClass, inUpgrade->mUnitClass->mImprovements, inUpgrade,
                                                                    this->mLevels, (int32_t)inLevel, (int32_t)inImprovement))).c_str();
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

// Raises (inDelta > 0) or lowers an improvement's level and points spent and moves the upgrade's stat as many steps
// (range also moves line of sight, hit points stay at least 1); does nothing without the class's improvement table
void EEUnitClassImprovement::ChangeLevel(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade, long inImprovement,
                                      long inDelta)
{
    ee_units_improve_change_level_records(inUnitClass, inUnitClass->mImprovements, inUpgrade, inUpgrade->mUnitClass,
                                          inUpgrade->mUnitClass->mProjectileClass, this->mLevels, &this->mTotalCost,
                                          (int32_t)inImprovement, (int32_t)inDelta);
}

// Reapplies every raised improvement to the upgrade's freshly reset stats (levels end unchanged; the points spent
// are recounted)
void EEUnitClassImprovement::Reapply(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade)
{
    ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_REAPPLY, this->mLevels, &this->mTotalCost,
                         EEImprovementContext{this, inUnitClass, inUpgrade, NULL}, &EEImprovementHost, 0, 0);
}

static_assert(sizeof(EEUnitClassImprovement::mLevels) == 0x40, "saved levels: 16 4-byte words on every target");

// Reads the levels (and points spent, recounted) from a saved game and applies them to the upgrade's stats; saves
// up to minor version 0xd8 drop leadership to level 1; returns the read error. The original's five version branches
// read: up to 0x17 the levels; up to 0x6a the levels and 16 old words (the second read's status); up to 0xa0 the
// levels; then the levels and the points spent (cleared)
long EEUnitClassImprovement::Load(EEUnitClass* inUnitClass, EEUnitClassUpgrade* inUpgrade,
                                  unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_LOAD, this->mLevels, &this->mTotalCost,
                                EEImprovementContext{this, inUnitClass, inUpgrade, &inFile}, &EEImprovementHost,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Writes the levels and the points spent to the saved game; returns the first write error
long EEUnitClassImprovement::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_units_improvement(EE_UNITS_IMPROVEMENT_DO_SAVE, this->mLevels, &this->mTotalCost,
                                EEImprovementContext{this, NULL, NULL, &outFile}, &EEImprovementHost, 0, 0);
}

// The C++ the record's Rust bodies call (ee_units improvement.rs): ChangeLevel on the context's record, the upgrade's
// name, the file's primitives (ee/EEPlayer.h EESavePrimitive)
int32_t EEImprovementHost(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEImprovementContext& c = *static_cast<EEImprovementContext*>(inContext);
    switch (inOp) {
    case EE_UNITS_IMPROVEMENT_CHANGE_LEVEL: c.mImprovement->ChangeLevel(c.mClass, c.mUpgrade, (long)inIndex, (long)io->i);
        return 0;
    case EE_UNITS_IMPROVEMENT_NAME_LOAD: return c.mUpgrade->mName.Load(*c.mFile);
    case EE_UNITS_IMPROVEMENT_NAME_SAVE: return c.mUpgrade->mName.Save(*c.mFile);
    }
    return EESavePrimitive(*c.mFile, inOp, io);
}
