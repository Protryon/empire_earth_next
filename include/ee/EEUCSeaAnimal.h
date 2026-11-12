// EEUCSeaAnimal.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCSeaAnimal.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint.
#ifndef EE_EEUCSEAANIMAL_H
#define EE_EEUCSEAANIMAL_H

#include "ee/EEUCSubmergedNaval.h"

class EEUCSubmergedNaval;

// vtable 0x44818c; bases EEUCSeaAnimal > EEUCSubmergedNaval > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x2363ec; dtor 0x2305ee
class EEUCSeaAnimal : public EEUCSubmergedNaval {
public:
    EEUCSeaAnimal();                        // inline
    EEUCSeaAnimal(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x219d67 trivial: push dword ptr [esp + 4]; call 0x61ab66; ret 4
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x219d73
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth); // +0x0c 0x219d87
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x20d336 (ICF: EEUCBasicCitizen, EEUCBasicPriest...)
};

#endif
