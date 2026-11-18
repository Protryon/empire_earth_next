// New (port): the calamity classes' starts, stops and updates in Rust (ee_tech calamity_start.rs, calamity_update.rs;
// docs/RUST_PLAN.md R5b.2). A class's Start, Stop and Update fill an EeCalamityParams and call EECalamityRunStart /
// RunStop / RunUpdate; the Rust run asks the world through EECalamityUpdateHost, answered in EECalamityUpdate.cpp:
// the calls every class shares and each class's own world steps (its effects, units, orders), keyed by its enum.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYUPDATE_H
#define EE_EECALAMITYUPDATE_H

#include <vector>

#include "rust/sim/tech.h"

class EECalamity;
class EECalamityClass;
class EEComplexUnit;
class EESoundObject;
class EEUnit;

struct EECalamityUpdateHost {
    EECalamityClass* mClass;
    EECalamity* mCalamity;
    EESoundObject* mSounds[2];              // the class's sound and secondary sound (UpdateSound's)
    std::vector<EEUnit*> mUnits;            // the last EEGetUnitsNear's
    void* mPending;                         // a class step's object for its next step (the cloak's area effect)
};

// runs the class's update in Rust; the calamity's members in and out (EeCalamityState) around it and every host call
long EECalamityRunUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime, const EeCalamityParams& inParams,
                         EESoundObject* inSound, EESoundObject* inSecondarySound);

// runs the class's Start / Stop in Rust (ee_tech calamity_start.rs) over the same host: the class's own steps are
// the EeCalamityOp::ClassStep calls
bool EECalamityRunStart(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                        EESoundObject* inSound, EESoundObject* inSecondarySound);
void EECalamityRunStop(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                       EESoundObject* inSound, EESoundObject* inSecondarySound);

// the cast tests in Rust (ee_tech calamity_cast.rs; EECalamityCast.cpp): the class's enum, the tile, the caster and
// the immunity flag of a question
EeCalamityCast EECalamityCastOf(EECalamityClass* inClass, EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);
// CanCastAt as the override of inMode runs it: 0 allowed, 1 refused, 2 protected by an area effect
long EECalamityCanCastAt(EECalamityClass* inClass, EeCalamityCastMode inMode, EEComplexUnit* inUnit, unsigned long inX,
                         unsigned long inY, bool inCheck, EEUnit* inTargetUnit);
// a unit class's IsValidTarget
bool EECalamityValidTarget(EECalamityClass* inClass, EEComplexUnit* inUnit, EEUnit* inTargetUnit);
// a location class's IsValidLocation (inUnitClassIndex: the hurricane's or the volcano's unit class)
bool EECalamityValidLocation(EECalamityClass* inClass, const EeCalamityCast& inCast, EEComplexUnit* inUnit,
                             long inUnitClassIndex);

#endif
