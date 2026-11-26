// C:\EEX\Empire Earth\Player\EEPlayerStatistics.cpp (file name inferred: no __FILE__; RTTI class EEPlayerStatistics,
// vtable 0x442080). 0x16d346..0x16e1a2: the class static's init chain, the point and series operator=, the player
// statistics in definition order, the free function EEAddDeadUnitStatistic (the end-of-game records and
// the history: ee_game gamecpp stats_history.rs), then the vector<EEStatisticsPoint> template tail. EEPlayer.cpp ends at 0x16d346, EEPPlayerEvent.cpp starts
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

// Set's gate (a shutdown, a saved game loading, the scenario editor outside the game screen) and the game time, then
// the series' op (ee_units player_stats.rs: 0 Set, 1 raised, 2 lowered)
void EEPlayerStatistics::Record(uint32_t inOp, long inSeries, long inValue)
{
    ee_units_stats_set(this->mStats, inOp, inSeries, inValue, gPhysics != NULL ? gPhysics->mGameTime : 0,
                       UShutdownInProgress() ? EE_UNITS_STATS_SHUTDOWN : gWorld.mLoadingSavedGame ? EE_UNITS_STATS_LOADING
                       : gUIManager->mInScenarioEditor && !gWorld.mGameScreenActive ? EE_UNITS_STATS_EDITOR : 0);
}

EEStatisticsSeries EEPlayerStatistics::GetSeries(unsigned long inSeries)
{
    EEStatisticsSeries theSeries;
    theSeries.mPoints.resize(ee_units_stats_value(this->mStats, inSeries, 3));
    if (!theSeries.mPoints.empty())
        ee_units_stats_points(this->mStats, inSeries, theSeries.mPoints.data(),
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
