// New (port): the scenario manager's host for its Rust logic (ee_scenario, docs/RUST_PLAN.md R5b.5;
// Scenario Manager/EEScenarioHost.cpp): the world's reads by operation code, and the C++ members as the Rust records.
#ifndef EE_EESCENARIOHOST_H
#define EE_EESCENARIOHOST_H

#include <vector>
#include "rust/sim/scenario.h"

class FSFile;
class UWideString;
class EESMCondition;
class EESMTrigger;
class EESMArea;
class EESMObject;
class EESMEffect;
class EEUnit;
template <class T> class U2DPoint;
class GE2DPoint;
class GETransformation;

// what a read may ask beyond the world: the condition whose chat text EE_SCENARIO_READ_CHAT_CONTAINS reads, the unit
// of EE_SCENARIO_READ_UNIT_* (NULL where no read asks); the effect whose plan the writes carry out (NULL for none);
// the object an evaluation reads and changes (EE_SCENARIO_READ_OBJ_* / EE_SCENARIO_WRITE_OBJ_*) and its cursor in a
// player's unit chain; the area tiles EE_SCENARIO_WRITE_TILE appends to; the units CreateUnits made and its placement,
// its wall pieces and its transformation (world_compose.rs create_unit)
struct EESMHostContext {
    EESMCondition* mCondition;
    EEUnit* mUnit;
    EESMEffect* mEffect;
    EESMObject* mObject;
    EEUnit* mCursor;
    std::vector<U2DPoint<long> >* mTiles;
    std::vector<unsigned long>* mCreated;
    GE2DPoint* mLocation;
    std::vector<EEUnit*>* mWallPieces;
    GETransformation* mTransform;
};
// an object evaluation's and a placement's reads (objeval.rs, placement.rs), the evaluation's writes, the placement's
// tile lists (EESMObject.cpp): Read, Write and the tiles callback hand the rest of their operations over
int64_t EESMObjectRead(EESMHostContext* inContext, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, int32_t inD);
void EESMObjectWrite(EESMHostContext* inContext, uint32_t inOp, int64_t inB);

// an effect run's world call (ee_scenario effect.rs EE_SCENARIO_WORLD_*; EESMEffect.cpp): inContext an effect
// run's context (EESMEffect::Execute's), a[0..5] the arguments
extern "C" int64_t ee_scenariocpp_host_world(void* inContext, uint32_t inOp, const int64_t* a);
// the host table over inContext (it must outlive the Rust call)
EeScenarioHost EESMHost(EESMHostContext* inContext);

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

#endif
