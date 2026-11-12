// C:\EEX\Empire Earth\Scenario Manager\EESMCondition.cpp (file name inferred: no __FILE__): 0x1ba89f..0x1bb5a5,
// one scenario trigger condition. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMCondition.h"
#include <vector>
#include <mbstring.h>
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EEPhysics.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"

// FUNCTION: EXE 0x5ba89f
// Creates a player-attribute condition with default players, attribute and operator, and no trigger or time tests
EESMCondition::EESMCondition()
{
    this->SetName("");
    this->SetText("");
    this->mType = kSMConditionPlayerAttribute;
    this->mObjectIndex = 0;
    this->mPlayer = 7;
    this->mAttribute = 2;
    this->mOperator = 2;
    this->mAmount = 0;
    this->mOtherPlayer = 8;
    this->mOtherAttribute = 2;
    this->mTechnology = kTechEpochPrehistoric;
    this->mTrigger = 0;
    this->mIsTriggerOn = false;
    this->mHasTriggerFired = false;
    this->mUsesPlayerAttribute = false;
    this->mMaxTime = 0;
    this->mMinTime = 0;
    this->mHour = 0;
    this->mUsesDifficulty = false;
    this->mUsesElapsedTime = false;
    this->mUsesHour = false;
    this->mDifficulty = 1;
    this->mDiplomacy = 0;
    this->mID = 0;
}

// FUNCTION: EXE 0x5ba976
// Destroys the condition (members only)
EESMCondition::~EESMCondition()
{
}

// FUNCTION: EXE 0x5ba998
// Returns whether the value compares to the target under the operator (<, <=, ==, >=, >); false for any other
bool EESMCondition::Compare(unsigned long inValue, unsigned long inOperator, unsigned long inTarget)
{
    return ee_scenario_compare((uint32_t)inValue, (uint32_t)inOperator, (uint32_t)inTarget);   // ee_scenario condition.rs
}

// FUNCTION: EXE 0x5ba9e7
// Copies every setting of another condition, including its name, texts and ID
void EESMCondition::Copy(const EESMCondition& inCondition)
{
    this->mName = inCondition.mName;
    this->mText = inCondition.mText;
    this->mChatText = inCondition.mChatText;
    this->mType = inCondition.mType;
    this->mObjectIndex = inCondition.mObjectIndex;
    this->mPlayer = inCondition.mPlayer;
    this->mAttribute = inCondition.mAttribute;
    this->mOperator = inCondition.mOperator;
    this->mAmount = inCondition.mAmount;
    this->mOtherPlayer = inCondition.mOtherPlayer;
    this->mOtherAttribute = inCondition.mOtherAttribute;
    this->mTechnology = inCondition.mTechnology;
    this->mTrigger = inCondition.mTrigger;
    this->mIsTriggerOn = inCondition.mIsTriggerOn;
    this->mHasTriggerFired = inCondition.mHasTriggerFired;
    this->mUsesPlayerAttribute = inCondition.mUsesPlayerAttribute;
    this->mMaxTime = inCondition.mMaxTime;
    this->mMinTime = inCondition.mMinTime;
    this->mHour = inCondition.mHour;
    this->mUsesDifficulty = inCondition.mUsesDifficulty;
    this->mUsesElapsedTime = inCondition.mUsesElapsedTime;
    this->mUsesHour = inCondition.mUsesHour;
    this->mDifficulty = inCondition.mDifficulty;
    this->mDiplomacy = inCondition.mDiplomacy;
    this->mID = inCondition.mID;
}

// FUNCTION: EXE 0x5baabf
// Returns whether the condition holds: object exists, player attribute/technology/diplomacy for any or all chosen
// players, trigger on/fired, elapsed time/hour/difficulty, or the last chat text containing a phrase
bool EESMCondition::Evaluate()
{
    if (!EESimCaptureRecord::Active())
        return this->EvaluateUncaptured();
    unsigned int theSeed = EESMCaptureSeed();
    bool theResult = this->EvaluateUncaptured();
    EESMCaptureCondition(this, theResult, theSeed);
    return theResult;
}

// Evaluate's body (Evaluate adds the simulation capture's SMCE record: ee/EEScenarioCapture.h)
bool EESMCondition::EvaluateUncaptured()
{
    // the test is Rust's (ee_scenario condition.rs evaluate) over the host's reads (EEScenarioHost.cpp)
    EeScenarioHost theHost = EESMHost(this);
    EeScenarioCondition theRecord = EESMConditionRecord(*this);
    return ee_scenario_condition_evaluate(&theHost, &theRecord);
}

// FUNCTION: EXE 0x5baf78
// Returns a player's value for a condition: a resource (any: a random one), population, units, difficulty, scenario
// variable, tribute sent/received, statistic or mining limit; 0 without a player
unsigned long EESMCondition::GetPlayerValue(long inPlayer, unsigned long inType)
{
    EeScenarioHost theHost = EESMHost(this);   // ee_scenario condition.rs player_value
    return ee_scenario_player_value(&theHost, (int32_t)inPlayer, (uint32_t)inType);
}

// FUNCTION: EXE 0x5bb154
// Reads the condition (diplomacy above 1 becomes 1); returns the first error, ignoring the chat text's
long EESMCondition::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mText.Load(inFile);
    if (theResult == kErrorOK) {
        this->mChatText.Load(inFile);
        theResult = inFile.Read(this->mType);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mObjectIndex);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOperator);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAmount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOtherPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mOtherAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsTriggerOn);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHasTriggerFired);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMaxTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMinTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHour);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesDifficulty);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesElapsedTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mUsesHour);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mDifficulty);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(this->mDiplomacy);
        if (this->mDiplomacy >= 2)
            this->mDiplomacy = 1;
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mID);
    return theResult;
}

// FUNCTION: EXE 0x5bb326
// Updates the condition's object index after objects inOld/inNew swap, or after inOld is deleted (inNew -1)
void EESMCondition::RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mObjectIndex;
    if (theIndex == inOldIndex) {
        this->mObjectIndex = (!UIsNone32(inNewIndex)) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mObjectIndex = inOldIndex;
    } else if (UIsNone32(inNewIndex) && theIndex > inOldIndex) {
        this->mObjectIndex = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5bb35c
// Updates the condition's trigger index after triggers inOld/inNew swap, or after inOld is deleted (inNew -1)
void EESMCondition::RenumberTriggers(unsigned long inOldIndex, unsigned long inNewIndex)
{
    unsigned long theIndex = this->mTrigger;
    if (theIndex == inOldIndex) {
        this->mTrigger = (!UIsNone32(inNewIndex)) ? inNewIndex : 0;
    } else if (theIndex == inNewIndex) {
        this->mTrigger = inOldIndex;
    } else if (UIsNone32(inNewIndex) && theIndex > inOldIndex) {
        this->mTrigger = theIndex - 1;
    }
}

// FUNCTION: EXE 0x5bb392
// Maps the condition's object index through the ID map
void EESMCondition::RemapObjects(EESMIDMap& inMap)
{
    EERemapID(this->mObjectIndex, inMap);
}

// FUNCTION: EXE 0x5bb3a4
// Maps the condition's trigger index through the ID map
void EESMCondition::RemapTriggers(EESMIDMap& inMap)
{
    EERemapID(this->mTrigger, inMap);
}

// FUNCTION: EXE 0x5bb3b6
// Writes the condition; returns the first error
long EESMCondition::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == kErrorOK) {
        // The text and chat text results are checked like every other field (they were ignored).
        theResult = this->mText.Save(outFile);
        if (theResult == kErrorOK)
            theResult = this->mChatText.Save(outFile);
        if (theResult == kErrorOK)
            theResult = outFile.Write(this->mType);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mObjectIndex);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOperator);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAmount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOtherPlayer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mOtherAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTechnology);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTrigger);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsTriggerOn);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHasTriggerFired);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesPlayerAttribute);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHour);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesDifficulty);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesElapsedTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesHour);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDifficulty);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDiplomacy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mID);
    return theResult;
}
