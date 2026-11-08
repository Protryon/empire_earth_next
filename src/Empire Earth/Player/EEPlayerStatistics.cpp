// C:\EEX\Empire Earth\Player\EEPlayerStatistics.cpp (file name inferred: no __FILE__; RTTI class EEPlayerStatistics,
// vtable 0x442080). 0x16d346..0x16e1a2: the class static's init chain, the point and series operator=, the player
// statistics in definition order, the free function EEAddDeadUnitStatistic, EEStatisticsRecord::Clear/dtor, EEStatisticsHistory's
// Reset/dtor, then the vector<EEStatisticsPoint> template tail. EEPlayer.cpp ends at 0x16d346, EEPPlayerEvent.cpp starts
// at 0x16e1a2. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"
#include "ee/EETileLocation.h"
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEStatisticsHistory.h"
#include "ee/EEStatisticsRecord.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// SYNTHETIC: EXE 0x56d346
// SYNTHETIC: EXE 0x56d350
// SYNTHETIC: EXE 0x56d361
// SYNTHETIC: EXE 0x56d36d
// the class key
const UString EEPlayerStatistics::sKey("EEPlayerStatistics-");

// FUNCTION: EXE 0x56d389
// Copies a statistics point (time and value)
EEStatisticsPoint& EEStatisticsPoint::operator=(const EEStatisticsPoint& inPoint)
{
    this->mTime = inPoint.mTime;
    this->mValue = inPoint.mValue;
    return *this;
}

// FUNCTION: EXE 0x56d39c
// Copies a statistics series: its maximum, history flag and points
EEStatisticsSeries& EEStatisticsSeries::operator=(const EEStatisticsSeries& inSeries)
{
    this->Reset();
    this->mMaxValue = inSeries.mMaxValue;
    this->mKeepHistory = inSeries.mKeepHistory;
    this->mPoints.resize(inSeries.mPoints.size());
    for (unsigned long i = 0; i < inSeries.mPoints.size(); i++)
        this->mPoints[i] = inSeries.mPoints[i];
    return *this;
}

// FUNCTION: EXE 0x56d409
// Copies a player's statistics: name, end time and all 31 series
EEPlayerStatistics& EEPlayerStatistics::operator=(const EEPlayerStatistics& inStatistics)
{
    this->Clear();
    this->mName = inStatistics.mName;
    this->mEndTime = inStatistics.mEndTime;
    for (long i = 0; i < 31; i++)
        this->mSeries[i] = inStatistics.mSeries[i];
    return *this;
}

// FUNCTION: EXE 0x56d453
// Creates empty player statistics
EEPlayerStatistics::EEPlayerStatistics()
{
    this->Clear();
}

// FUNCTION: EXE 0x56d498
// Destroys the player statistics
EEPlayerStatistics::~EEPlayerStatistics()
{
}

// FUNCTION: EXE 0x56d4c6
// Lowers a series by an amount, unless that would take it below zero
void EEPlayerStatistics::Lower(long inSeries, long inAmount)
{
    EEStatisticsPoint thePoint;
    thePoint = this->mSeries[inSeries].mPoints.back();
    if (thePoint.mValue >= (unsigned long)inAmount)
        this->Set(inSeries, thePoint.mValue - inAmount);
}

// FUNCTION: EXE 0x56d507
// Raises a series by an amount
void EEPlayerStatistics::Reset(long inSeries, long inAmount)
{
    EEStatisticsPoint thePoint;
    thePoint = this->mSeries[inSeries].mPoints.back();
    this->Set(inSeries, thePoint.mValue + inAmount);
}

// FUNCTION: EXE 0x56d545
// Records a new value of a series at the current game time (replacing the last point unless the series keeps
// history); skipped while loading a saved game and, in the scenario editor, for most series
void EEPlayerStatistics::Set(long inIndex, long inValue)
{
    EEStatisticsPoint thePoint;
    if (UShutdownInProgress())
        return;
    bool theRecord = !gUIManager->mInScenarioEditor || gGameScreenActive || inIndex == 12 || inIndex == 13 || inIndex == 14 ||
                     inIndex == 16 || inIndex == 19 || inIndex == 15;
    if (gLoadingSavedGame || !theRecord)
        return;
    if (!this->mSeries[inIndex].mKeepHistory)
        this->mSeries[inIndex].mPoints.pop_back();
    thePoint.mValue = inValue;
    thePoint.mTime = gPhysics->mGameTime;
    if ((unsigned long)inValue > this->mSeries[inIndex].mMaxValue)
        this->mSeries[inIndex].mMaxValue = inValue;
    this->mSeries[inIndex].mPoints.push_back(thePoint);
}

// FUNCTION: EXE 0x56d5fd
// Returns the highest value a series reached
unsigned long EEPlayerStatistics::GetValue(unsigned long inSeries)
{
    return this->mSeries[inSeries].mMaxValue;
}

// FUNCTION: EXE 0x56d60b
// Returns a series' latest value (0 when empty)
unsigned long EEPlayerStatistics::GetLastValue(unsigned long inSeries)
{
    unsigned long theValue = 0;
    if (this->mSeries[inSeries].mPoints.size() > 0)
        theValue = this->mSeries[inSeries].mPoints.back().mValue;
    return theValue;
}

// FUNCTION: EXE 0x56d63a
// Counts how often a series went up between consecutive points
unsigned long EEPlayerStatistics::CountIncreases(unsigned long inSeries)
{
    unsigned long theCount = 0;
    unsigned long theSize = this->mSeries[inSeries].mPoints.size();
    for (unsigned long i = 0; i < theSize; i++) {
        if (i < theSize - 1 && this->mSeries[inSeries].mPoints[i].mValue < this->mSeries[inSeries].mPoints[i + 1].mValue)
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x56d69e
// Returns the statistics series a dead unit counts in: 14 citizens, 15 heroes, 13 other moving units, 0 none
long EEAddDeadUnitStatistic(EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    long theSeries = 0;
    if (theClass->mCategory != kUnitCategoryResource && theClass->mCategory != kUnitCategoryAmbient && theClass->mFamily != kUnitFamilyAnimal &&
        theClass->mUnitType != kUnitTypePathPoint) {
        long theCategory = theClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
            theSeries = 14;
        else if (theClass->mFamily == kUnitFamilyHero)
            theSeries = 15;
        else if (theClass->mCategory != kUnitCategoryProjectile && theClass->mCategory != kUnitCategoryMissile && theClass->mCategory != kUnitCategoryBomb &&
                 theClass->mCategory != kUnitCategoryTorpedo && inUnit->mStats->mMaxVelocity > 0.0f)
            theSeries = 13;
    }
    return theSeries;
}

// FUNCTION: EXE 0x56d718
// Clears the statistics; series 12..16 keep their history
void EEPlayerStatistics::Clear()
{
    this->mName = gEmptyString;
    this->mEndTime = 0;
    for (long i = 0; i < 31; i++)
        this->mSeries[i].Reset();
    this->mSeries[13].mKeepHistory = true;
    this->mSeries[14].mKeepHistory = true;
    this->mSeries[15].mKeepHistory = true;
    this->mSeries[16].mKeepHistory = true;
    this->mSeries[12].mKeepHistory = true;
}

// FUNCTION: EXE 0x56d76c
// Reads the statistics from the saved game: name, end time, each series' maximum and points (the saved
// history flag is read and dropped)
long EEPlayerStatistics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long thePoints = 0;
    long theResult = this->mName.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mEndTime);
    if (theResult == 0)
        theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        theResult = inFile.Read(this->mSeries[i].mMaxValue);
        if (theResult == 0) {
            bool theFlag;
            theResult = inFile.Read(theFlag);
        }
        if (theResult == 0)
            theResult = inFile.Read(thePoints);
        if (theResult == 0)
            this->mSeries[i].mPoints.resize(thePoints);
        for (unsigned long p = 0; p < thePoints && theResult == 0; p++) {
            theResult = inFile.Read(this->mSeries[i].mPoints[p].mValue);
            if (theResult == 0)
                theResult = inFile.Read(this->mSeries[i].mPoints[p].mTime);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56d86b
// Writes the statistics to the saved game: name, end time, each series' maximum, history flag and points
long EEPlayerStatistics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mEndTime);
    if (theResult == 0) {
        unsigned long theCount = 31;
        theResult = outFile.Write(theCount);
    }
    for (unsigned long i = 0; i < 31 && theResult == 0; i++) {
        theResult = outFile.Write(this->mSeries[i].mMaxValue);
        if (theResult == 0)
            theResult = outFile.Write(this->mSeries[i].mKeepHistory);
        if (theResult == 0) {
            unsigned long thePoints = this->mSeries[i].mPoints.size();
            theResult = outFile.Write(thePoints);
        }
        for (unsigned long p = 0; p < this->mSeries[i].mPoints.size() && theResult == 0; p++) {
            theResult = outFile.Write(this->mSeries[i].mPoints[p].mValue);
            if (theResult == 0)
                theResult = outFile.Write(this->mSeries[i].mPoints[p].mTime);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56d95a
// Removes repeated and zero points from the epoch series (12), keeping its changes
// a point is kept when its value is non-zero and differs from the next one's (the last one when it differs from the
// kept series' last value)
void EEPlayerStatistics::CompactEpochHistory()
{
    EEStatisticsSeries theSeries;
    EEStatisticsPoint thePoint;
    EEStatisticsPoint theNext;
    for (unsigned long i = 0; i < 31; i++) {
        if (i != 12 || this->mSeries[12].mPoints.size() <= 1)
            continue;
        theSeries.Reset();
        theSeries.mKeepHistory = this->mSeries[12].mKeepHistory;
        for (unsigned long j = 0; j < this->mSeries[12].mPoints.size(); j++) {
            thePoint = this->mSeries[12].mPoints[j];
            if (j == this->mSeries[12].mPoints.size() - 1) {
                // (an empty kept series keeps the last point)
                if (theSeries.mPoints.size() > 0 && theSeries.mPoints.back().mValue == thePoint.mValue)
                    continue;
            } else {
                theNext = this->mSeries[12].mPoints[j + 1];
                if (thePoint.mValue == 0 || theNext.mValue == thePoint.mValue)
                    continue;
            }
            if (theSeries.mPoints.size() > 1 && theSeries.mPoints.back().mTime == thePoint.mTime &&
                theSeries.mPoints.back().mTime == 0)
                theSeries.mPoints.pop_back();
            theSeries.mPoints.push_back(thePoint);
            if (thePoint.mValue > theSeries.mMaxValue)
                theSeries.mMaxValue = thePoint.mValue;
        }
        this->mSeries[12] = theSeries;
    }
}

// FUNCTION: EXE 0x56dadc
// Clears a player's end-of-game record: white, not local or human, no slot, zero scores and resources
void EEStatisticsRecord::Clear()
{
    this->mColor = GEColor(1.0f, 1.0f, 1.0f);
    this->mIsLocal = false;
    this->mIsHuman = false;
    this->mName = gEmptyString;
    this->mNameSuffix = gEmptyString;
    this->mSlot = 0xff;
    this->mResult = 0;
    this->mScore = 0;
    this->mEconomyScore = 0;
    this->mEpochScore = 0;
    this->mMilitaryScore = 0;
    this->mTechScore = 0;
    this->mEndTime = 0;
    this->mTeam = 0;
    for (long i = 0; i < 7; i++)
        this->mResources[i] = 0;
    this->mStatistics.Clear();
}

// FUNCTION: EXE 0x56db75
// Destroys the end-of-game record
EEStatisticsRecord::~EEStatisticsRecord()
{
}

// FUNCTION: EXE 0x56db99
// Clears the game history: times, map size, flags, campaign name and every record
void EEStatisticsHistory::Reset()
{
    this->mStartTime = 0;
    this->mCurrentRecordIndex = 0;
    this->mTileCount = 0;
    this->mEndTime = 0;
    this->mVictoryEnabled = false;
    this->mTeamsLocked = false;
    this->mIsScenario = false;
    this->mIsCampaign = false;
    this->mCampaignName.Clear();
    this->mRecords.clear();
}

// FUNCTION: EXE 0x56dbc3
// Destroys the game history
EEStatisticsHistory::~EEStatisticsHistory()
{
}

// TEMPLATE: EXE 0x56dbd8
// TEMPLATE: EXE 0x56dc04
// TEMPLATE: EXE 0x56dc38
// TEMPLATE: EXE 0x56dc88
// TEMPLATE: EXE 0x56dce9
// TEMPLATE: EXE 0x56dd12
// TEMPLATE: EXE 0x56dd33
// TEMPLATE: EXE 0x56dd6e
// TEMPLATE: EXE 0x56de19
// TEMPLATE: EXE 0x56de5d
// TEMPLATE: EXE 0x56de98
// TEMPLATE: EXE 0x56e135
// TEMPLATE: EXE 0x56e159
// TEMPLATE: EXE 0x56e176
