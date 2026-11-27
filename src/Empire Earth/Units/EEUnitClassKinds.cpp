// EEUnitClassKinds.cpp (new, R5b.4 round 32): EEUnitClass's virtuals as dispatchers by the class's kind.
// Round 34: the kind tests are one Rust table (ee_units cls_dispatch.rs, EEClassBody in ee/EEUnitKind.h): each
// dispatcher switches over its row's body for the class, in the order the tests below were written.
// The 36 EEUC* subclasses (one per category the class factory built) are gone: a class is one EEUnitClass whose
// kind is its category's entry in ee_units' table (kinds.rs EE_UNIT_CLASS_KINDS: the class's bit and its
// ancestors'). Each dispatcher tests the kinds deepest first, as the vtables resolved an override: the first
// kind with a body of its own runs it; the others run the base body (Base*) or the default.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitClassKinds.h"
#include "rust/sim/units.h"

// the kinds' trigger answers are Rust (ee_units cls_condition.rs, R5b.4 round 34); the shots' are ee_combat's

template <class T> static T* EEUckObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The bodies the kind dispatchers of ee_units uck_kinds.rs choose (EE_UNITS_UCK_*): one statement each; the class
// host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_uck_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCK_KINDS: return (int64_t)EEUnitClassKinds(theClass);
    case EE_UNITS_UCK_BODY: return EEClassBody(theClass, (size_t)inB);
    case EE_UNITS_UCK_MISSILE_ACTION_CHANGED: theClass->MissileActionChanged(theUnit); return 0;
    case EE_UNITS_UCK_PROJECTILE_ACTION_CHANGED: theClass->ProjectileActionChanged(theUnit); return 0;
    case EE_UNITS_UCK_ACTION_CHANGED: ee_units_class_action_changed((int32_t)inB, (uint64_t)(uintptr_t)theClass, (uint64_t)(uintptr_t)theUnit); return 0;
    case EE_UNITS_UCK_UPDATE: ee_units_class_update((int32_t)inB, (uint64_t)(uintptr_t)theClass, (uint64_t)(uintptr_t)theUnit); return 0;
    case EE_UNITS_UCK_PRIEST_UPDATES: return !theUnit->IsFoundation() || theUnit->mHitPoints <= 0 || theUnit->mCurrentPhysicsAction == 2;
    case EE_UNITS_UCK_TORPEDO_UPDATE: theClass->TorpedoUpdate(theUnit); return 0;
    case EE_UNITS_UCK_MISSILE_UPDATE: theClass->MissileUpdate(theUnit); return 0;
    case EE_UNITS_UCK_PROJECTILE_UPDATE: theClass->ProjectileUpdate(theUnit); return 0;
    case EE_UNITS_UCK_RIPPLES_DUE:
        return theClass->mRippleEffect != NULL && theUnit->mUnitClass->mUnitType == kUnitTypeCatalina && theUnit->mHitPoints != 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_3: return theClass->mRippleEffect != NULL && theUnit->mHitPoints != 0 && (unsigned long)theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_4:
        return (theClass->mUnitType == kUnitTypeDock || theClass->mUnitType == kUnitTypeNavyYard) && theClass->mRippleEffect != NULL && theUnit->mHitPoints != 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_RIPPLES_DUE_5: return theClass->mRippleEffect != NULL && theUnit->mHitPoints > 0 && theClass->mRippleEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_TRAIL_DUE: return theUnit->mHitPoints > 0 && theClass->mTrailEffect != NULL && theUnit->mVelocity != 0.0f && theClass->mTrailEffect->mDetailLevel <= gEffectDetail;
    case EE_UNITS_UCK_TORPEDO_CONDITION: return EECombatCondition(EE_COMBAT_GOAL_TORPEDO, theUnit, (unsigned long)inB, EECombatShotTarget(theUnit), NULL, theClass);
    case EE_UNITS_UCK_MISSILE_CONDITION: return theClass->MissileEvaluateCondition(theUnit, (unsigned long)inB);
    case EE_UNITS_UCK_PROJECTILE_CONDITION: return EECombatCondition(EE_COMBAT_GOAL_PROJECTILE, theUnit, (unsigned long)inB, EECombatShotTarget(theUnit), NULL, theClass);
    // round 9's dispatcher bodies (uck_dispatch.rs: EE_UNITS_UCK_D_*)
    case EE_UNITS_UCK_D_AIR_SOUND: theClass->AirGetSoundParameters(EEUckObject<EEUnit>(inA), EEUckObject<EESoundObject>((uint64_t)inB), EEUckObject<float*>((uint64_t)inC)[0], EEUckObject<float*>((uint64_t)inC)[1], EEUckObject<float*>((uint64_t)inC)[2]); return 0;
    case EE_UNITS_UCK_D_IS_SIDE_PART: return theClass->IsSidePart(theUnit);
    case EE_UNITS_UCK_D_FIRST_PART_COMMAND: return EEGetFirstPart(theUnit)->GetCommandType();
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}

// (TEST ONLY, the UCBD record: its notes are Rust's, ee_sim units ucn_noted.rs over ucbd_note.rs; EEUCNote is gone)
