// EEUCBuildingPriest.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCBuildingPriest.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCBUILDINGPRIEST_H
#define EE_EEUCBUILDINGPRIEST_H

#include "ee/EEUCBuilding.h"

class EEComplexUnit;
class EEUCBuilding;

// vtable 0x447fb8; bases EEUCBuildingPriest > EEUCBuilding > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23626a; dtor 0x2305ee
class EEUCBuildingPriest : public EEUCBuilding {
public:
    EEUCBuildingPriest();                   // inline
    EEUCBuildingPriest(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCBuildingPriest() {}        // +0x00 dtor 0x2305ee
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x21005e
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x2100b5 trivial: push dword ptr [esp + 4]; call 0x60f17f; ret 4
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x2100c1
    // +0xa8/+0xb4/+0xbc: inline, emitted with the vtable in EEUnitClass.cpp (0x236436/0x236442/0x23644e, between the
    // factory's ??_G bodies); the first two forward to the priest half (ICF-folded into EEUCBasicPriest's 0x20dd0a /
    // 0x20dee4), the third to the building's
    virtual void LoadMedia(EEPlayer* inPlayer) { this->PriestVirtualA8(inPlayer); }  // +0xa8 0x236436
    virtual void UnloadMedia(EEPlayer* inPlayer) { this->PriestVirtualB4(inPlayer); }  // +0xb4 0x236442
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x21010c
    virtual void UpdateEffects(EEComplexUnit* inUnit) { EEUCBuilding::UpdateEffects(inUnit); }  // +0xbc 0x23644e
    // the priest half: the class's own copies of EEUCBasicPriest's bodies (identical source; ICF-folded into
    // EEUCBasicPriest's 0x20d998/0x20dd0a/0x20dd2d/0x20dee4/0x20df05/0x20d59f/0x20d7f2/0x20e16a/0x20e274; owner
    // decision) (provisional names)
    void PriestVirtualA8(EEPlayer* inPlayer);
    long PriestVirtualA0(EEComplexUnit* inUnit, unsigned long inEvent);
    void PriestVirtualB4(EEPlayer* inPlayer);
    void PriestVirtualB8(EEComplexUnit* inUnit);
    void UpdateConversion(EEComplexUnit* inUnit);
    void ConvertTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
    void ShowConversionEffects(EEComplexUnit* inUnit);
    void HideConversionEffects(EEComplexUnit* inUnit);
};

#endif
