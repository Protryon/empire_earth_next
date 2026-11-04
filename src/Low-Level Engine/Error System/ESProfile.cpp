// C:\EEX\Low-Level Engine\Error System\ESProfile.cpp (file name inferred: no __FILE__)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/ESProfile.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x10003ab1
// TEMPLATE: LLE 0x10003e7c
// TEMPLATE: LLE 0x10003ebe
// TEMPLATE: LLE 0x10003f02
// TEMPLATE: LLE 0x10003f3a
// TEMPLATE: LLE 0x10003f7f
// TEMPLATE: LLE 0x10003fc6
// TEMPLATE: LLE 0x1000400a

std::map<UWideString, ESProfileHistory> ESProfile::sProfileHistory;

// FUNCTION: LLE 0x10003859
// Creates an empty profile history
ESProfileHistory::ESProfileHistory()
{
    this->Clear();
}

// FUNCTION: LLE 0x1000386c
// Resets every timing and count of the profile history (minimum time -1 = none yet)
void ESProfileHistory::Clear()
{
    this->mMinimumTime = -1;
    this->mTotalTime = 0;
    this->mLastTime = 0;
    this->mAverageTime = 0;
    this->mFirstTime = 0;
    this->mRecentTime = 0;
    this->mMaximumTime = 0;
    this->mCallCount = 0;
    this->mRecursionCount = 0;
    this->mSampleCount = 0;
    this->mInMicroseconds = false;
}

// FUNCTION: LLE 0x100038b0
// Converts the recorded times from microseconds to milliseconds
void ESProfileHistory::ConvertMicrosecondsToMilliseconds()
{
    if (this->mMinimumTime != -1)
        this->mMinimumTime /= 1000;
    this->mTotalTime /= 1000;
    this->mLastTime /= 1000;
    this->mAverageTime /= 1000;
    this->mFirstTime /= 1000;
    this->mRecentTime /= 1000;
    this->mMaximumTime /= 1000;
    this->mInMicroseconds = false;
}

// FUNCTION: LLE 0x10003953
// Converts the recorded times from milliseconds to microseconds
void ESProfileHistory::ConvertMillisecondsToMicroseconds()
{
    if (this->mMinimumTime != -1)
        this->mMinimumTime *= 1000;
    this->mTotalTime *= 1000;
    this->mLastTime *= 1000;
    this->mAverageTime *= 1000;
    this->mFirstTime *= 1000;
    this->mRecentTime *= 1000;
    this->mMaximumTime *= 1000;
    this->mInMicroseconds = true;
}

// FUNCTION: LLE 0x100039f7
// Forgets every recorded profile history
void ESProfile::ClearAllRecords()
{
    ESProfile::sProfileHistory.clear();
}

// FUNCTION: LLE 0x10003a01
// Copies the named profile's history to outHistory when it exists; returns whether it exists
bool ESProfile::GetProfileInformation(const UWideString& inName, ESProfileHistory& outHistory)
{
    std::map<UWideString, ESProfileHistory>::iterator theRecord = ESProfile::sProfileHistory.find(inName);
    if (theRecord == ESProfile::sProfileHistory.end())
        return false;   // Fixed: used to return true for a missing profile as well
    outHistory = theRecord->second;
    return true;
}

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100036c5
// SYNTHETIC: LLE 0x10003758
// SYNTHETIC: LLE 0x100037eb
// SYNTHETIC: LLE 0x100037f0
// TEMPLATE: LLE 0x100037f5
// SYNTHETIC: LLE 0x100037fb
// SYNTHETIC: LLE 0x10003808
// SYNTHETIC: LLE 0x10003812
// SYNTHETIC: LLE 0x10003832
// SYNTHETIC: LLE 0x1000383e
// TEMPLATE: LLE 0x10003a2f
// TEMPLATE: LLE 0x10003a5a
// TEMPLATE: LLE 0x10003a6e
// TEMPLATE: LLE 0x10003b22
