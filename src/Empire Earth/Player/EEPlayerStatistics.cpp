// C:\EEX\Empire Earth\Player\EEPlayerStatistics.cpp (file name inferred: no __FILE__; RTTI class EEPlayerStatistics,
// vtable 0x442080). 0x16d346..0x16e1a2: the class static's init chain, the point and series operator=, the player
// statistics in definition order, the free function EEAddDeadUnitStatistic, EEStatisticsRecord::Clear/dtor, EEStatisticsHistory's
// Reset/dtor, then the vector<EEStatisticsPoint> template tail. EEPlayer.cpp ends at 0x16d346, EEPPlayerEvent.cpp starts
// at 0x16e1a2. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEStatisticsRecord.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// the class key
const UString EEPlayerStatistics::sKey("EEPlayerStatistics-");

// Set's gate (a shutdown, a saved game loading, the scenario editor outside the game screen) and the game time, then
// the series' op (ee_units player_stats.rs: 0 Set, 1 raised, 2 lowered)
void EEPlayerStatistics::Record(uint32_t inOp, long inSeries, long inValue)
{
    uint32_t theFlags = UShutdownInProgress() ? EE_UNITS_STATS_SHUTDOWN
                        : gLoadingSavedGame ? EE_UNITS_STATS_LOADING
                        : gUIManager->mInScenarioEditor && !gGameScreenActive ? EE_UNITS_STATS_EDITOR : 0;
    ee_units_stats_set(this->mStats, inOp, inSeries, inValue, gPhysics != NULL ? gPhysics->mGameTime : 0, theFlags);
}

EEStatisticsSeries EEPlayerStatistics::GetSeries(unsigned long inSeries)
{
    EEStatisticsSeries theSeries;
    theSeries.mPoints.resize(ee_units_stats_value(this->mStats, inSeries, 3));
    if (!theSeries.mPoints.empty())
        ee_units_stats_points(this->mStats, inSeries, reinterpret_cast<EeUnitsStatPoint*>(&theSeries.mPoints[0]),
                              theSeries.mPoints.size());
    theSeries.mMaxValue = ee_units_stats_value(this->mStats, inSeries, 0);
    theSeries.mKeepHistory = ee_units_stats_value(this->mStats, inSeries, 4) != 0;
    return theSeries;
}

// Reads the statistics from the saved game: the name, then Rust's (the end time, each series' maximum and points)
long EEPlayerStatistics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    return theResult != 0 ? theResult : ee_units_stats_load(this->mStats, &inFile, &EESaveFilePrimitive, &this->mEndTime);
}

// Writes the statistics to the saved game: the name, then Rust's (the end time, each series)
long EEPlayerStatistics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    return theResult != 0 ? theResult : ee_units_stats_save(this->mStats, &outFile, &EESaveFilePrimitive, this->mEndTime);
}

// Returns the statistics series a dead unit counts in: 14 citizens, 15 heroes, 13 other moving units, 0 none
long EEAddDeadUnitStatistic(EEComplexUnit* inUnit)
{
    return ee_units_dead_unit_statistic((uint64_t)(uintptr_t)static_cast<RTSUnit*>(inUnit));   // ee_units player_rest.rs
}

// Clears a player's end-of-game record: white, not local or human, no slot, zero scores and resources (the plain
// members from mColor to mIsHuman: ee_units player_stats.rs)
static_assert(offsetof(EEStatisticsRecord, mResources) - offsetof(EEStatisticsRecord, mColor) == offsetof(EeUnitsStatsRecordTail, resources) && offsetof(EEStatisticsRecord, mSlot) - offsetof(EEStatisticsRecord, mColor) == offsetof(EeUnitsStatsRecordTail, slot) && offsetof(EEStatisticsRecord, mIsHuman) - offsetof(EEStatisticsRecord, mColor) == offsetof(EeUnitsStatsRecordTail, is_human), "EeUnitsStatsRecordTail");
void EEStatisticsRecord::Clear()
{
    this->mName = gEmptyString;
    this->mNameSuffix = gEmptyString;
    this->mStatistics.Clear();
    ee_units_stats_record_clear(reinterpret_cast<EeUnitsStatsRecordTail*>(&this->mColor));
}

// Destroys the end-of-game record
EEStatisticsRecord::~EEStatisticsRecord()
{
}

// Clears the game history: times, map size, flags (the plain members from mStartTime to mIsCampaign: ee_units
// player_stats.rs), campaign name and every record
static_assert(offsetof(EEStatisticsHistory, mTeamsLocked) - offsetof(EEStatisticsHistory, mStartTime) == offsetof(EeUnitsStatsHistoryTail, teams_locked) && offsetof(EEStatisticsHistory, mIsCampaign) - offsetof(EEStatisticsHistory, mStartTime) == offsetof(EeUnitsStatsHistoryTail, is_campaign), "EeUnitsStatsHistoryTail");
void EEStatisticsHistory::Reset()
{
    ee_units_stats_history_clear(reinterpret_cast<EeUnitsStatsHistoryTail*>(&this->mStartTime));
    this->mCampaignName.Clear();
    this->mRecords.clear();
}

// Destroys the game history
EEStatisticsHistory::~EEStatisticsHistory()
{
}
