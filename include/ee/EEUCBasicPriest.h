// EEUCBasicPriest.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCBasicPriest.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCBASICPRIEST_H
#define EE_EEUCBASICPRIEST_H

#include "ee/EEUnit.h"

class EEComplexUnit;
class EEUnitClass;

// vtable 0x447144; bases EEUCBasicPriest > EEUnitClass > UReferenceCounter; sizeof ?; ctor ?; ??_G 0x21882c; dtor 0x2305ee
class EEUCBasicPriest : public EEUnitClass {
public:
    // inline (EEUCLandPriest's ctor 0x216d57 calls EEUnitClass's directly and stores only its own vptr)
    EEUCBasicPriest(EEUnitClassRecord* inRecord, EEPhysicsModel* inArg) : EEUnitClass(true, inRecord, inArg) {}
    // +0x00: an inline user-declared dtor (??_G 0x21882c, every priest class's by ICF, stores this vptr before
    // calling EEUnitClass's 0x2305ee)
    virtual ~EEUCBasicPriest() {}
    virtual bool CanAttackMove();           // +0x10 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x20d998 (ICF: EEUCLandPriest, EEUCNavalPriest)
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x20da7a trivial: push dword ptr [esp + 4]; call 0x63511e; ret 4 (ICF: EEUCBasicCitizen, EEUCLandCitizen...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x20dd2d (ICF: EEUCLandPriest)
    virtual void LoadMedia(EEPlayer* inPlayer);           // +0xa8 0x20dd0a (ICF: EEUCHelicopterPriest, EEUCLandPriest...)
    virtual void UnloadMedia(EEPlayer* inPlayer);         // +0xb4 0x20dee4 (ICF: EEUCHelicopterPriest, EEUCLandPriest...)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x20df05
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x20d336 (ICF: EEUCBasicCitizen, EEUCLandCitizen...)
    // EEUCBasicPriest.cpp's members (provisional names)
    void UpdateConversion(EEComplexUnit* inUnit);  // 0x20d59f (command 0x7d4: the conversion's progress)
    void ConvertTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget);  // 0x20d7f2 (the target changes sides)
    void ShowConversionEffects(EEComplexUnit* inUnit);  // 0x20e16a (category 0x1e: the effects and sound on the unit)
    void HideConversionEffects(EEComplexUnit* inUnit);  // 0x20e274 (category 0x1e: those effects off)
};

#endif
