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
class EESMEffect;
class EEUnit;

// what a read may ask beyond the world: the condition whose chat text EE_SCENARIO_READ_CHAT_CONTAINS reads, the unit
// of EE_SCENARIO_READ_UNIT_* (NULL where no read asks); the effect whose plan the writes carry out (NULL for none)
struct EESMHostContext {
    EESMCondition* mCondition;
    EEUnit* mUnit;
    EESMEffect* mEffect;
};

// the host table over inContext (it must outlive the Rust call)
EeScenarioHost EESMHost(EESMHostContext* inContext);
EeScenarioAreaRecord EESMAreaRecord(const EESMArea& inArea);

class EEScenarioManager;
// an element's save (ee_scenario save.rs): its file, its strings by slot, its id list (NULL for none), the trigger
// whose entries it reads or writes (NULL for none); the manager's save (manager.rs): the manager whose elements,
// media strings and messages it reads or writes, and the version they are read or written with
struct EESMStream {
    FSFile* mFile;
    UWideString* const* mStrings;
    std::vector<unsigned long>* mList;
    EESMTrigger* mTrigger;
    EEScenarioManager* mManager;
    unsigned long mMajor;
    unsigned long mMinor;
};
// the manager's operations of a stream (EE_SCENARIO_IO_ELEMENT_*, _MEDIA, _MESSAGES; EEScenarioManager.cpp)
int32_t EESMManagerIO(EESMStream* inStream, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue);
// the stream table over inStream (it must outlive the Rust call)
EeScenarioStream EESMStreamTable(EESMStream* inStream);

#endif
