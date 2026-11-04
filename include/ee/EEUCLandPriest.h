// EEUCLandPriest.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCLandPriest.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCLANDPRIEST_H
#define EE_EEUCLANDPRIEST_H

#include "ee/EEUCBasicPriest.h"

class EEUCBasicPriest;

// vtable 0x447060; bases EEUCLandPriest > EEUCBasicPriest > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor 0x216d57; ??_G 0x21882c; dtor 0x2305ee
class EEUCLandPriest : public EEUCBasicPriest {
public:
    EEUCLandPriest(EEUnitClassRecord* inRecord);   // 0x216d57 (EEUnitClass(true, inRecord, gPhysicsModels[0]))
    virtual void GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);  // +0x2c 0x216d94
    // +0x6c inline (COMDAT 0x216d8d after the ctor; the vtable's 0x2130fe is an identical copy ICF kept)
    virtual long GetPopulationCost() { return this->mWeight; }
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x216dcb trivial: push dword ptr [esp + 4]; call 0x60da7a; ret 4
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x216dd7 trivial: push dword ptr [esp + 4]; call 0x60df05; ret 4 (ICF: EEUCHelicopterPriest, EEUCNavalPriest)
    // NB: EEUCBasicPriest's sizeof (0x32c) is ASSUMED = the smallest derived class's (no new site)
};

// 0x531d58: the value EEUCLandPriest's ctor passes on (provisional name; EEUCNavalPriest's is 0x531d68)
extern EEPhysicsModel* gPhysicsModels[12];     // 0x531d58 (EEPMLand.cpp: pointers to the EEPM* singletons; [0] is the land one)

#endif
