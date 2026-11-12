// EEUCLandMilitaryHero.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCLandMilitaryHero.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCLANDMILITARYHERO_H
#define EE_EEUCLANDMILITARYHERO_H

#include "ee/EEUCLand.h"

class EEUCLand;

// vtable 0x448524; bases EEUCLandMilitaryHero > EEUCLand > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x236414; dtor 0x2305ee
class EEUCLandMilitaryHero : public EEUCLand {
public:
    EEUCLandMilitaryHero();                 // inline
    EEUCLandMilitaryHero(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCLandMilitaryHero() {}      // +0x00 dtor 0x2305ee
};

#endif
