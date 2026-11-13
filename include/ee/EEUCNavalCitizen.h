// EEUCNavalCitizen.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCNavalCitizen.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCNAVALCITIZEN_H
#define EE_EEUCNAVALCITIZEN_H

#include "ee/EEUCBasicCitizen.h"

class EEComplexUnit;
class EEUCBasicCitizen;
class EEUnit;

// vtable 0x4490a0; bases EEUCNavalCitizen > EEUCBasicCitizen > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x236308; dtor 0x2305ee
class EEUCNavalCitizen : public EEUCBasicCitizen {
public:
    EEUCNavalCitizen();                     // inline
    EEUCNavalCitizen(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCNavalCitizen() {}          // +0x00 dtor 0x2305ee
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x218a45 (ICF: EEUCNaval, EEUCNavalTransport)
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x218490 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth); // +0x0c 0x218539
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x218605 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x218909 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x218664
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x218767
    // members OPEN: a base's sizeof is unknown (no new site), so this class's own members cannot be placed yet
};

#endif
