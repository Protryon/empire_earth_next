// EEUCPlanetSpaceship.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCPlanetSpaceship.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCPLANETSPACESHIP_H
#define EE_EEUCPLANETSPACESHIP_H

#include "ee/EEUCLandable.h"

class EEComplexUnit;
class EEUCLandable;

// vtable 0x447bf4; bases EEUCPlanetSpaceship > EEUCLandable > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23649e; dtor 0x2305ee
class EEUCPlanetSpaceship : public EEUCLandable {
public:
    EEUCPlanetSpaceship();                  // inline
    EEUCPlanetSpaceship(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCPlanetSpaceship() {}       // +0x00 dtor 0x2305ee
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x218e34
    // members OPEN: a base's sizeof is unknown (no new site), so this class's own members cannot be placed yet
};

#endif
