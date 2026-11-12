// New (port): the scenario manager's host for its Rust logic (ee_scenario, docs/RUST_PLAN.md R5b.5;
// Scenario Manager/EEScenarioHost.cpp): the world's reads by operation code, and the C++ members as the Rust records.
#ifndef EE_EESCENARIOHOST_H
#define EE_EESCENARIOHOST_H

#include "rust/ee_scenario.h"

class EESMCondition;

// the host table; inCondition is the condition whose chat text EE_SCENARIO_READ_CHAT_CONTAINS reads (NULL outside a
// condition's test)
EeScenarioHost EESMHost(EESMCondition* inCondition);
EeScenarioCondition EESMConditionRecord(const EESMCondition& inCondition);

#endif
