// New (port): the scenario manager's host for its Rust logic (ee_scenario, docs/RUST_PLAN.md R5b.5;
// Scenario Manager/EEScenarioHost.cpp): the world's reads by operation code, and the C++ members as the Rust records.
#ifndef EE_EESCENARIOHOST_H
#define EE_EESCENARIOHOST_H

#include <vector>
#include "rust/ee_scenario.h"

class FSFile;
class UWideString;
class EESMCondition;
class EESMTrigger;
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
EeScenarioAreaRecord EESMAreaRecord(const EESMArea& inArea);

// an element's save (ee_scenario save.rs): its file, its strings by slot, its id list (NULL for none), the trigger
// whose entries it reads or writes (NULL for none)
struct EESMStream {
    FSFile* mFile;
    UWideString* const* mStrings;
    std::vector<unsigned long>* mList;
    EESMTrigger* mTrigger;
};
// the stream table over inStream (it must outlive the Rust call)
EeScenarioStream EESMStreamTable(EESMStream* inStream);

#endif
