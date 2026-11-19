// C:\EEX\Empire Earth\Scenario Manager\EESMCondition.cpp (file name inferred: no __FILE__): 0x1ba89f..0x1bb5a5,
// one scenario trigger condition. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EESMCondition.h"
#include <vector>
#include <mbstring.h>
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"

// Creates a player-attribute condition with default players, attribute and operator, and no trigger or time tests
// (the numbers: ee_scenario fields.rs condition_defaults)
EESMCondition::EESMCondition()
{
    this->SetName("");
    this->SetText("");
    *static_cast<EeScenarioConditionFields*>(this) = ee_scenario_condition_defaults();
}

// Destroys the condition (members only)
EESMCondition::~EESMCondition()
{
}

// Returns whether the value compares to the target under the operator (<, <=, ==, >=, >); false for any other
bool EESMCondition::Compare(unsigned long inValue, unsigned long inOperator, unsigned long inTarget)
{
    return ee_scenario_compare((uint32_t)inValue, (uint32_t)inOperator, (uint32_t)inTarget);   // ee_scenario condition.rs
}

// Copies every setting of another condition, including its name, texts and ID
void EESMCondition::Copy(const EESMCondition& inCondition)
{
    this->mName = inCondition.mName;
    this->mText = inCondition.mText;
    this->mChatText = inCondition.mChatText;
    *static_cast<EeScenarioConditionFields*>(this) = inCondition;   // every number
}

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
    EESMHostContext theContext = {this, NULL};
    EeScenarioHost theHost = EESMHost(&theContext);
    return ee_scenario_condition_evaluate(&theHost, this);
}

// Reads the condition (diplomacy above 1 becomes 1); returns the first error, ignoring the chat text's
// (ee_scenario save.rs condition_load)
long EESMCondition::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText, &this->mChatText};
    EESMStream theStream = {&inFile, theStrings, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_condition_load(&theTable, this);
}

// Writes the condition; returns the first error (ee_scenario save.rs condition_save)
long EESMCondition::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    UWideString* theStrings[] = {&this->mName, &this->mText, &this->mChatText};
    EESMStream theStream = {&outFile, theStrings, NULL};
    EeScenarioStream theTable = EESMStreamTable(&theStream);
    return ee_scenario_condition_save(&theTable, this);
}
