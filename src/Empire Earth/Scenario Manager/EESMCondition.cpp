// C:\EEX\Empire Earth\Scenario Manager\EESMCondition.cpp (file name inferred: no __FILE__): 0x1ba89f..0x1bb5a5,
// one scenario trigger condition. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EESMCondition.h"
#include <vector>
#include <mbstring.h>

// Creates a player-attribute condition with default players, attribute and operator, and no trigger or time tests
// (the numbers: ee_scenario fields.rs condition_defaults; the strings empty)
EESMCondition::EESMCondition() : EeScenarioConditionFields(ee_scenario_condition_defaults()) {}

// Destroys the condition (members only)
EESMCondition::~EESMCondition()
{
}

// Returns whether the value compares to the target under the operator (<, <=, ==, >=, >); false for any other
bool EESMCondition::Compare(unsigned long inValue, unsigned long inOperator, unsigned long inTarget)
{
    return ee_scenario_compare((uint32_t)inValue, (uint32_t)inOperator, (uint32_t)inTarget);   // ee_scenario condition.rs
}

// Copies every setting of another condition, including its name, texts and ID (ee_scenario entry.rs)
void EESMCondition::Copy(const EESMCondition& inCondition)
{
    ee_scenario_en_condition_copy(this, (void*)&inCondition);
}

// Returns whether the condition holds: object exists, player attribute/technology/diplomacy for any or all chosen
// players, trigger on/fired, elapsed time/hour/difficulty, or the last chat text containing a phrase (ee_scenario
// condition.rs over the host's reads; entry.rs adds the simulation capture's SMCE record)
bool EESMCondition::Evaluate()
{
    return ee_scenario_en_condition_evaluate(this);
}

// Reads the condition (diplomacy above 1 becomes 1); returns the first error, ignoring the chat text's
// (ee_scenario save.rs condition_load)
long EESMCondition::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_scenario_en_condition_io(this, &inFile, false);
}

// Writes the condition; returns the first error (ee_scenario save.rs condition_save)
long EESMCondition::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_scenario_en_condition_io(this, &outFile, true);
}
