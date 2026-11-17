// C:\EEX\Empire Earth\Calamities\EEContagionFunctor.h (RECONSTRUCTION; file name inferred). What a contagion
// helper applies to the units it reaches. The original's base (vtable 0x43c65c, every virtual an inline default) had
// six subclasses: EEFireFunctor (0xb6bd1..0xb6e58), EELeprosyFunctor (0xb6e58..0xb6f21), EEPlagueFunctor
// (0xb6fb1..0xb70cc), EEAreaEffectFunctor (0xac991..0xacb75) and its EEAEExtraDamageFunctor (0xac7b9..) and
// EEAEMindControlFunctor (0xac891..).
// Requires <stdint.h>.
#ifndef EE_EECONTAGIONFUNCTOR_H
#define EE_EECONTAGIONFUNCTOR_H

#include "rust/sim/tech.h"   // EeContagionFunctorFields (ee_tech contagion_functor.rs)

class EECalamity;
class EECalamityClass;
class EEGraphicEffect;
class EEUnit;

// New (port): the six functor classes as one, keyed by its kind (mKind, the EE_CALAMITY_FUNCTOR_* the class's
// Initialize made). Its numbers are the Rust record EeContagionFunctorFields (ee_tech contagion_functor.rs: what
// each kind's constructor stored, the getters the helper's update reads, Apply's damage); its effects and its world
// steps (Infect, Cure, Apply) are here, one switch by kind (EEContagionManager.cpp). Every kind's CanInfectType was
// the unit set test (the ICF-shared 0xb6c4d), every UpdateUnits empty.
class EEContagionFunctor : public EeContagionFunctorFields {
public:
    // the functor of inKind for the class (its record, effects and immunity flag; the fire references its effect)
    EEContagionFunctor(EECalamityClass* inClass, uint32_t inKind);
    ~EEContagionFunctor();                  // the fire's 0xb6c2f: its effect released

    bool CanInfectType(long inCategory, long inUnitType);   // 0xb6c4d: the unit type is in the functor's unit set
    bool CanInfect(EEUnit* inUnit);         // 0xb66f8 (EEContagionManager.cpp) (provisional name)
    // the discrete updates of the calamity in this helper update (0xb6c7c, 0xb6eae, 0xb6ff2)
    void Update(float inElapsedTime, EECalamity* inCalamity);
    void Infect(EEUnit* inUnit, EECalamity* inCalamity);   // a unit was infected
    void Cure(EEUnit* inUnit, EECalamity* inCalamity);     // a unit was cured
    void Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity);   // once per infected unit and update

    EEGraphicEffect* mEffect;               // the fire's burning effect (referenced), the mind control's effect
    EEGraphicEffect* mEffect2;              // the area effect's target effect
    EEGraphicEffect* mEffect3;              // the area effect's hit effect
};

#endif
