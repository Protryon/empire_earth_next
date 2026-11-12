// EEUCNavalBuilding.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCNavalBuilding.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCNAVALBUILDING_H
#define EE_EEUCNAVALBUILDING_H

#include "ee/EEUCBuilding.h"

class EEComplexUnit;
class EEUCBuilding;
class EEUnit;

// vtable 0x448c0c; bases EEUCNavalBuilding > EEUCBuilding > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23626a; dtor 0x2305ee
class EEUCNavalBuilding : public EEUCBuilding {
public:
    EEUCNavalBuilding();                    // inline
    EEUCNavalBuilding(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x218158
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x218490 (ICF: EEUCNaval, EEUCNavalCitizen...)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x218605 (ICF: EEUCNaval, EEUCNavalCitizen...)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x218909 (ICF: EEUCNaval, EEUCNavalCitizen...)
    virtual bool IsLandBuilding();          // +0x54 0xac9f3 trivial: xor al, al; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x21a129 (ICF: EEUCSpaceBuilding)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88 0x21a144 (ICF: EEUCSpaceBuilding)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x2183f8 (ICF: EEUCSpaceBuilding)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x21845a
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x20d336 (ICF: EEUCBasicCitizen, EEUCBasicPriest...)
    virtual bool SelectUnitToRelease(EEComplexUnit* inUnit);  // +0xe0 0x218164 (ICF: EEUCSpaceBuilding)
    virtual bool HasSunk(EEComplexUnit* inUnit);     // +0xe4 0x218320 NEW
};

#endif
