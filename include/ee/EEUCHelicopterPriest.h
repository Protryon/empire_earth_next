// EEUCHelicopterPriest.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCHelicopterPriest.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCHELICOPTERPRIEST_H
#define EE_EEUCHELICOPTERPRIEST_H

#include "ee/EEUCBasicPriest.h"

class EEComplexUnit;
class EEUCBasicPriest;

// vtable 0x448270; bases EEUCHelicopterPriest > EEUCBasicPriest > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x21882c; dtor 0x2305ee
class EEUCHelicopterPriest : public EEUCBasicPriest {
public:
    EEUCHelicopterPriest();                 // inline
    EEUCHelicopterPriest(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCHelicopterPriest() {}      // +0x00 dtor 0x2305ee
    virtual float GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation); // +0x24 0x212edf
    virtual void GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);  // +0x2c 0x2130b3
    virtual bool FliesOver();               // +0x5c 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual long GetPopulationCost();       // +0x6c 0x2130fe trivial: mov eax, dword ptr [ecx + 0x258]; ret (ICF: EEUCLandPriest)
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x213105
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x2132c0
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x21aaa9 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual void RemoveFromTile(EEUnit*);  // +0x98 0x21aaf5 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x213344
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x157269 trivial: ret 4 (ICF: EEAECalamityImmunity, EEAEConversionImmunity...)
    // NB: EEUCBasicPriest's sizeof (0x32c) is ASSUMED = the smallest derived class's (no new site)
};

#endif
