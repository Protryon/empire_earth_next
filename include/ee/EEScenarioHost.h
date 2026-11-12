// New (port): the scenario manager's host for its Rust logic (ee_scenario, docs/RUST_PLAN.md R5b.5;
// Scenario Manager/EEScenarioHost.cpp): the world's reads by operation code, and the C++ members as the Rust records.
#ifndef EE_EESCENARIOHOST_H
#define EE_EESCENARIOHOST_H

#include "rust/ee_scenario.h"

class EESMCondition;
class EESMArea;
class EESMObject;
class EEUnit;

// what a read may ask beyond the world: the condition whose chat text EE_SCENARIO_READ_CHAT_CONTAINS reads, the unit
// of EE_SCENARIO_READ_UNIT_* (NULL where no read asks)
struct EESMHostContext {
    EESMCondition* mCondition;
    EEUnit* mUnit;
};

// the host table over inContext (it must outlive the Rust call)
EeScenarioHost EESMHost(EESMHostContext* inContext);
EeScenarioCondition EESMConditionRecord(const EESMCondition& inCondition);
EeScenarioAreaRecord EESMAreaRecord(const EESMArea& inArea);
EeScenarioObjectFilter EESMObjectFilter(const EESMObject& inObject);

#endif
