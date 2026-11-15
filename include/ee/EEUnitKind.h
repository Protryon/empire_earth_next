// New (port): unit kinds as data (docs/RUST_PLAN.md R5b.4). The class tests the code made with dynamic_cast, as
// reads of the kind tables ee_units owns (rust/crates/ee_units/src/kinds.rs):
//   EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theClass) == dynamic_cast<EEUCGate*>(theClass) (the classes until round 32)
//   EEUnitCast<EEComplexUnit>(theUnit)           == dynamic_cast<EEComplexUnit*>(theUnit)   (from any RTSUnit)
// A unit class's from EEUnitClass::mCategory and a unit object's from
// RTSUnit::mObjectKind (kinds.rs says why);
// NULL stays NULL. Include it after the goal and class headers of the classes a file tests.
#ifndef EE_EEUNITKIND_H
#define EE_EEUNITKIND_H

#include <stddef.h>
#include <stdint.h>

#include "rust/ee_units.h"   // the EE_GOAL_KIND_* and EE_CLASS_KIND_* bits

// the tables (kinds.rs; cbindgen exports no statics)
extern "C" {
extern const uint32_t EE_UNIT_GOAL_KINDS[256];    // the goal classes of each EEUnitGoal::mKind
extern const uint64_t EE_UNIT_CLASS_KINDS[256];   // the unit classes of each EEUnitClass::mCategory (below 256)
extern const uint32_t EE_UNIT_OBJECT_KINDS[256];  // the unit object classes of each RTSUnit::mObjectKind
extern const uint8_t EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_COUNT][256];  // each virtual's body by category (cls_dispatch.rs)
}

// The goal classes' test (EEGoalCast<EEUGLocation>) went with the last goal class that was a C++ class of its own:
// every goal is an EEUGRust tested by its mKind (ee/EEUGRust.h: EELocationGoalCast and the others, R5b.4 round 21).
// The unit classes went the same way in R5b.4 round 32: the 36 EEUC* subclasses are kinds of one EEUnitClass, tested
// by these bits (EEUnitClassKinds.cpp's dispatchers run each kind's bodies).

// a class's kind bits (its category's entry: its own kind's EE_CLASS_KIND_* bit and its ancestors')
template <class C> inline uint64_t EEUnitClassKinds(const C* inClass)
{
    unsigned long theCategory = inClass->mCategory;
    return theCategory < 256 ? EE_UNIT_CLASS_KINDS[theCategory] : 0;
}

// the body EEUnitClass's virtual inVirtual (an EE_UNITS_BODY_* row) runs for inClass: 0 the default, n its n-th
template <class C> inline unsigned EEClassBody(const C* inClass, size_t inVirtual)
{
    unsigned long theCategory = inClass->mCategory;
    return theCategory < 256 ? EE_UNIT_CLASS_BODIES[inVirtual][theCategory] : 0;
}

// whether inClass (not NULL) is of kind B (an EE_CLASS_KIND_* bit): what dynamic_cast<EEUC*> tested
template <uint64_t B, class C> inline bool EEUnitClassIsKind(const C* inClass)
{
    return (EEUnitClassKinds(inClass) & B) != 0;
}

// inClass when it is of kind B, else NULL (NULL stays NULL)
template <uint64_t B, class C> inline C* EEUnitClassOfKind(C* inClass)
{
    return inClass != NULL && EEUnitClassIsKind<B>(inClass) ? inClass : NULL;
}

class EEUnit;
class EEComplexUnit;

// each unit object class's bit
template <class T> struct EEUnitObjectKindBit;
template <> struct EEUnitObjectKindBit<EEUnit> { static const uint32_t kBit = EE_OBJECT_KIND_UNIT; };
template <> struct EEUnitObjectKindBit<EEComplexUnit> { static const uint32_t kBit = EE_OBJECT_KIND_COMPLEX_UNIT; };

// whether inUnit (an RTSUnit or derived, not NULL) is a T
template <class T, class U> inline bool EEUnitIs(const U* inUnit)
{
    return (EE_UNIT_OBJECT_KINDS[inUnit->mObjectKind.mValue] & EEUnitObjectKindBit<T>::kBit) != 0;
}

// dynamic_cast<T*>(inUnit) for the unit object classes
template <class T, class U> inline T* EEUnitCast(U* inUnit)
{
    return inUnit != NULL && EEUnitIs<T>(inUnit) ? static_cast<T*>(inUnit) : NULL;
}
template <class T, class U> inline const T* EEUnitCast(const U* inUnit)
{
    return inUnit != NULL && EEUnitIs<T>(inUnit) ? static_cast<const T*>(inUnit) : NULL;
}

#endif
