// New (port): unit kinds as data (docs/RUST_PLAN.md R5b.4). The class tests the code made with dynamic_cast, as
// reads of the kind tables ee_units owns (rust/crates/ee_units/src/kinds.rs):
//   EEGoalCast<EEUGGather>(theGoal)              == dynamic_cast<EEUGGather*>(theGoal)
//   EEUnitClassCast<EEUCGate>(theClass)          == dynamic_cast<EEUCGate*>(theClass)
//   EEUnitCast<EEComplexUnit>(theUnit)           == dynamic_cast<EEComplexUnit*>(theUnit)   (from any RTSUnit)
// A goal's class follows from EEUnitGoal::mKind, a unit class's from EEUnitClass::mCategory and a unit object's from
// RTSUnit::mObjectKind (kinds.rs says why);
// NULL stays NULL. Include it after the goal and class headers of the classes a file tests.
#ifndef EE_EEUNITKIND_H
#define EE_EEUNITKIND_H

#include <stddef.h>
#include <stdint.h>

#include "rust/ee_units.h"   // the EE_GOAL_KIND_* and EE_CLASS_KIND_* bits
#include "ee/EEUnitStore.h"   // the unit store's holders (EEUnitStoreField: mGoalState, mUnitClass)

// the tables (kinds.rs; cbindgen exports no statics)
extern "C" {
extern const uint32_t EE_UNIT_GOAL_KINDS[256];    // the goal classes of each EEUnitGoal::mKind
extern const uint64_t EE_UNIT_CLASS_KINDS[256];   // the unit classes of each EEUnitClass::mCategory (below 256)
extern const uint32_t EE_UNIT_OBJECT_KINDS[256];  // the unit object classes of each RTSUnit::mObjectKind
}

class EEUnitGoal;
class EEUGLocation;
class EEUGBuildRepair;
class EEUGGather;
class EEUGLoadContainer;
class EEUGLoadTransport;
class EEUGUnloadTransport;
class EEUGExplore;
class EEUGCastCalamity;
class EEUGLoadGarrison;
class EEUGHealUnit;

// each goal class's bit
template <class T> struct EEGoalKindBit;
template <> struct EEGoalKindBit<EEUGLocation> { static const uint32_t kBit = EE_GOAL_KIND_LOCATION; };
template <> struct EEGoalKindBit<EEUGBuildRepair> { static const uint32_t kBit = EE_GOAL_KIND_BUILD_REPAIR; };
template <> struct EEGoalKindBit<EEUGGather> { static const uint32_t kBit = EE_GOAL_KIND_GATHER; };
template <> struct EEGoalKindBit<EEUGLoadContainer> { static const uint32_t kBit = EE_GOAL_KIND_LOAD_CONTAINER; };
template <> struct EEGoalKindBit<EEUGLoadTransport> { static const uint32_t kBit = EE_GOAL_KIND_LOAD_TRANSPORT; };
template <> struct EEGoalKindBit<EEUGUnloadTransport> { static const uint32_t kBit = EE_GOAL_KIND_UNLOAD_TRANSPORT; };
template <> struct EEGoalKindBit<EEUGExplore> { static const uint32_t kBit = EE_GOAL_KIND_EXPLORE; };
template <> struct EEGoalKindBit<EEUGCastCalamity> { static const uint32_t kBit = EE_GOAL_KIND_CAST_CALAMITY; };
template <> struct EEGoalKindBit<EEUGLoadGarrison> { static const uint32_t kBit = EE_GOAL_KIND_LOAD_GARRISON; };
template <> struct EEGoalKindBit<EEUGHealUnit> { static const uint32_t kBit = EE_GOAL_KIND_HEAL_UNIT; };

// whether inGoal (not NULL) is a T
template <class T, class G> inline bool EEGoalIs(const G* inGoal)
{
    return (EE_UNIT_GOAL_KINDS[inGoal->mKind] & EEGoalKindBit<T>::kBit) != 0;
}

// dynamic_cast<T*>(inGoal) for the goal classes
template <class T, class G> inline T* EEGoalCast(G* inGoal)
{
    return inGoal != NULL && EEGoalIs<T>(inGoal) ? static_cast<T*>(inGoal) : NULL;
}
template <class T, class G> inline const T* EEGoalCast(const G* inGoal)
{
    return inGoal != NULL && EEGoalIs<T>(inGoal) ? static_cast<const T*>(inGoal) : NULL;
}
// a unit store holder's goal (EEComplexUnit::mGoalState)
template <class T, class G, unsigned int F> inline T* EEGoalCast(const EEUnitStoreField<G*, F>& inGoal)
{
    return EEGoalCast<T>(inGoal.Get());
}

class EEUnitClass;
class EEUCAmbient;
class EEUCResource;
class EEUCBuilding;
class EEUCLandable;
class EEUCAirplane;
class EEUCNaval;
class EEUCLand;
class EEUCBasicCitizen;
class EEUCLandCitizen;
class EEUCNavalCitizen;
class EEUCHelicopter;
class EEUCProjectile;
class EEUCBomb;
class EEUCMissile;
class EEUCTorpedo;
class EEUCBasicPriest;
class EEUCLandPriest;
class EEUCNavalPriest;
class EEUCHelicopterPriest;
class EEUCNavalBuilding;
class EEUCMiningBuilding;
class EEUCGate;
class EEUCBuildingPriest;
class EEUCSpaceBuilding;
class EEUCSubmergedNaval;
class EEUCSeaAnimal;
class EEUCAirAnimal;
class EEUCTransport;
class EEUCLandTransport;
class EEUCHelicopterTransport;
class EEUCNavalTransport;
class EEUCSpaceTransport;
class EEUCFortress;
class EEUCLandMilitaryHero;
class EEUCSpaceship;
class EEUCPlanetSpaceship;

// each unit class's bit
template <class T> struct EEUnitClassKindBit;
#define EE_UNIT_CLASS_KIND(T, B) \
    template <> struct EEUnitClassKindBit<T> { static const uint64_t kBit = EE_CLASS_KIND_##B; }
EE_UNIT_CLASS_KIND(EEUCAmbient, AMBIENT);
EE_UNIT_CLASS_KIND(EEUCResource, RESOURCE);
EE_UNIT_CLASS_KIND(EEUCBuilding, BUILDING);
EE_UNIT_CLASS_KIND(EEUCLandable, LANDABLE);
EE_UNIT_CLASS_KIND(EEUCAirplane, AIRPLANE);
EE_UNIT_CLASS_KIND(EEUCNaval, NAVAL);
EE_UNIT_CLASS_KIND(EEUCLand, LAND);
EE_UNIT_CLASS_KIND(EEUCBasicCitizen, BASIC_CITIZEN);
EE_UNIT_CLASS_KIND(EEUCLandCitizen, LAND_CITIZEN);
EE_UNIT_CLASS_KIND(EEUCNavalCitizen, NAVAL_CITIZEN);
EE_UNIT_CLASS_KIND(EEUCHelicopter, HELICOPTER);
EE_UNIT_CLASS_KIND(EEUCProjectile, PROJECTILE);
EE_UNIT_CLASS_KIND(EEUCBomb, BOMB);
EE_UNIT_CLASS_KIND(EEUCMissile, MISSILE);
EE_UNIT_CLASS_KIND(EEUCTorpedo, TORPEDO);
EE_UNIT_CLASS_KIND(EEUCBasicPriest, BASIC_PRIEST);
EE_UNIT_CLASS_KIND(EEUCLandPriest, LAND_PRIEST);
EE_UNIT_CLASS_KIND(EEUCNavalPriest, NAVAL_PRIEST);
EE_UNIT_CLASS_KIND(EEUCHelicopterPriest, HELICOPTER_PRIEST);
EE_UNIT_CLASS_KIND(EEUCNavalBuilding, NAVAL_BUILDING);
EE_UNIT_CLASS_KIND(EEUCMiningBuilding, MINING_BUILDING);
EE_UNIT_CLASS_KIND(EEUCGate, GATE);
EE_UNIT_CLASS_KIND(EEUCBuildingPriest, BUILDING_PRIEST);
EE_UNIT_CLASS_KIND(EEUCSpaceBuilding, SPACE_BUILDING);
EE_UNIT_CLASS_KIND(EEUCSubmergedNaval, SUBMERGED_NAVAL);
EE_UNIT_CLASS_KIND(EEUCSeaAnimal, SEA_ANIMAL);
EE_UNIT_CLASS_KIND(EEUCAirAnimal, AIR_ANIMAL);
EE_UNIT_CLASS_KIND(EEUCTransport, TRANSPORT);
EE_UNIT_CLASS_KIND(EEUCLandTransport, LAND_TRANSPORT);
EE_UNIT_CLASS_KIND(EEUCHelicopterTransport, HELICOPTER_TRANSPORT);
EE_UNIT_CLASS_KIND(EEUCNavalTransport, NAVAL_TRANSPORT);
EE_UNIT_CLASS_KIND(EEUCSpaceTransport, SPACE_TRANSPORT);
EE_UNIT_CLASS_KIND(EEUCFortress, FORTRESS);
EE_UNIT_CLASS_KIND(EEUCLandMilitaryHero, LAND_MILITARY_HERO);
EE_UNIT_CLASS_KIND(EEUCSpaceship, SPACESHIP);
EE_UNIT_CLASS_KIND(EEUCPlanetSpaceship, PLANET_SPACESHIP);
#undef EE_UNIT_CLASS_KIND

// whether inClass (not NULL) is a T
template <class T, class C> inline bool EEUnitClassIs(const C* inClass)
{
    unsigned long theCategory = inClass->mCategory;
    return theCategory < 256 && (EE_UNIT_CLASS_KINDS[theCategory] & EEUnitClassKindBit<T>::kBit) != 0;
}

// dynamic_cast<T*>(inClass) for the unit classes
template <class T, class C> inline T* EEUnitClassCast(C* inClass)
{
    return inClass != NULL && EEUnitClassIs<T>(inClass) ? static_cast<T*>(inClass) : NULL;
}
template <class T, class C> inline const T* EEUnitClassCast(const C* inClass)
{
    return inClass != NULL && EEUnitClassIs<T>(inClass) ? static_cast<const T*>(inClass) : NULL;
}
// a unit store holder's class (EEUnit::mUnitClass)
template <class T, class C, unsigned int F> inline T* EEUnitClassCast(const EEUnitStoreField<C*, F>& inClass)
{
    return EEUnitClassCast<T>(inClass.Get());
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
