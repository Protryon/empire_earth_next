// EEUCNavalPriest.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCNavalPriest.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GETransformation.
#ifndef EE_EEUCNAVALPRIEST_H
#define EE_EEUCNAVALPRIEST_H

#include "ee/EEUCBasicPriest.h"

class EEUCBasicPriest;
class EEUnit;

// vtable 0x44724c; bases EEUCNavalPriest > EEUCBasicPriest > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor 0x21880c; ??_G 0x21882c; dtor 0x2305ee
class EEUCNavalPriest : public EEUCBasicPriest {
public:
    // 0x21880c (the class factory 0x2358b1 passes its record): EEUCBasicPriest(inRecord, gPhysicsModels[4]).
    // The dtor is implicit (??_G 0x21882c stores only EEUCBasicPriest's vptr).
    EEUCNavalPriest(EEUnitClassRecord* inRecord);
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth); // +0x0c 0x21884e (ICF: EEUCSubmergedNaval)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x218605 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x218909 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x218942
    // NB: EEUCBasicPriest's sizeof (0x32c) is ASSUMED = the smallest derived class's (no new site)
};

#endif
