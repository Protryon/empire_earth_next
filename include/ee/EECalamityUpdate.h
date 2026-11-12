// New (port): the calamity classes' updates in Rust (ee_tech calamity_update.rs; docs/RUST_PLAN.md R5b.2). Every
// class's Update fills an EeCalamityParams from its members and calls EECalamityRunUpdate; the Rust update asks the
// world through EECalamityUpdateHost: the calls every class shares are answered in EECalamityUpdate.cpp, the
// class's own world steps (its effects, units, orders) by the class's static UpdateStep in its own unit.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EECALAMITYUPDATE_H
#define EE_EECALAMITYUPDATE_H

#include <vector>

#include "rust/ee_tech.h"

class EECalamity;
class EECalamityClass;
class EEComplexUnit;
class EESoundObject;
class EEUnit;

struct EECalamityUpdateHost;
// a class's own world steps: false for a call it does not answer
typedef bool (*EECalamityStepFn)(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

struct EECalamityUpdateHost {
    EECalamityClass* mClass;
    EECalamity* mCalamity;
    EESoundObject* mSounds[2];              // the class's sound and secondary sound (UpdateSound's)
    std::vector<EEUnit*> mUnits;            // the last EEGetUnitsNear's
    EECalamityStepFn mStep;
    void* mPending;                         // a class step's object for its next step (the cloak's area effect)
};

// the class's common parameters: its enum, lifetime, immunity flag and which sounds it has
EeCalamityParams EECalamityBaseParams(EECalamityClass* inClass, EESoundObject* inSound, EESoundObject* inSecondarySound);
// runs the class's update in Rust; the calamity's members in and out (EeCalamityState) around it and every host call
long EECalamityRunUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime, const EeCalamityParams& inParams,
                         EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep);

// runs the class's Start / Stop in Rust (ee_tech calamity_start.rs) over the same host: the class's own steps are
// the EeCalamityOp::ClassStep calls its StartStep answers
bool EECalamityRunStart(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                        EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep);
void EECalamityRunStop(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                       EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep);

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
