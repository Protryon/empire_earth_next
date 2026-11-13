// EEUCSpaceBuilding.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCSpaceBuilding.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCSPACEBUILDING_H
#define EE_EEUCSPACEBUILDING_H

#include "ee/EEUCBuilding.h"

class EEComplexUnit;
class EEUCBuilding;
class EEUnit;

// vtable 0x447cdc; bases EEUCSpaceBuilding > EEUCBuilding > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23626a; dtor 0x2305ee
class EEUCSpaceBuilding : public EEUCBuilding {
public:
    EEUCSpaceBuilding();                    // inline
    EEUCSpaceBuilding(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x219fe3
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x219ff3
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x21a0dd
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x21a0a0
    virtual bool IsLandBuilding();          // +0x54 0xac9f3 trivial: xor al, al; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x21a129 (ICF: EEUCNavalBuilding)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88 0x21a144 (ICF: EEUCNavalBuilding)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x2183f8 (ICF: EEUCNavalBuilding)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x21a1cb
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x157269 trivial: ret 4 (ICF: EEAECalamityImmunity, EEAEConversionImmunity...)
    virtual bool SelectUnitToRelease(EEComplexUnit* inUnit);  // +0xe0 0x218164 (ICF: EEUCNavalBuilding)
    virtual bool HasSunk(EEComplexUnit* inUnit);     // +0xe4 0xb3257 NEW trivial: mov al, 1; ret 4 (ICF: EECCSpecialAttack, EECCSpecialLocation...)
};

#endif
