// EEFOFindNewTargetNearOldTarget.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names).
// Classes: EEFOFindNewTargetNearOldTarget.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEFOFINDNEWTARGETNEAROLDTARGET_H
#define EE_EEFOFINDNEWTARGETNEAROLDTARGET_H

#include <stdint.h>

#include "ee/EEFOFindNewTargetScreenBase.h"
#include "rust/sim/combat.h"            // ee_units_cgq_near_old_screen

// vtable 0x449ba8; bases EEFOFindNewTargetNearOldTarget > EEFOFindNewTargetScreenBase; sizeof ?; ctor ?; ??_G 0x239ec5 (dtor inline)
class EEFOFindNewTargetNearOldTarget : public EEFOFindNewTargetScreenBase {
public:
    // inline (0x241c2d builds one on the stack: both members stored before the vptr, the init list)
    EEFOFindNewTargetNearOldTarget(EEComplexUnit* inTarget, float inDistanceSquared)
        : mTarget(inTarget), mDistanceSquared(inDistanceSquared) {}
    virtual ~EEFOFindNewTargetNearOldTarget() {} // +0x00 (inline: ??_G 0x239ec5)
    // +0x04 0x241e21: alive and nearer to mTarget than mDistanceSquared (ee_sim combat cg_query.rs near_old_screen,
    // its CBFS record); +0x08 0x241e21 the same body (ICF)
    virtual bool Virtual04(EEComplexUnit* inUnit) { return this->Near(inUnit); }
    virtual bool Virtual08(EEComplexUnit* inUnit) { return this->Near(inUnit); }
    bool Near(EEComplexUnit* inUnit) const
        { return ee_units_cgq_near_old_screen((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)this->mTarget, this->mDistanceSquared); }
    EEComplexUnit* mTarget;                 // +0x04 (provisional names)
    float mDistanceSquared;                 // +0x08 (25.0f from 0x241c2d)
};

#endif
