// EEUCSubmergedNaval.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCSubmergedNaval.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCSUBMERGEDNAVAL_H
#define EE_EEUCSUBMERGEDNAVAL_H

#include "ee/EEUnit.h"

class EEComplexUnit;
class EEUnit;
class EEUnitClass;

// vtable 0x448a3c; bases EEUCSubmergedNaval > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x2363ec; dtor 0x2305ee
class EEUCSubmergedNaval : public EEUnitClass {
public:
    virtual ~EEUCSubmergedNaval() {}
    EEUCSubmergedNaval();                   // inline
    EEUCSubmergedNaval(EEUnitClassRecord* inRecord, EEPhysicsModel* inModel);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1; out of line 0x2363ce)
    // +0x00: the implicit dtor (??_G 0x2363ec calls EEUnitClass's 0x2305ee)
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x21ab66
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x21aba6
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth); // +0x0c 0x21884e (ICF: EEUCNavalPriest)
    virtual bool CanAttackMove();           // +0x10 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x21ad2c (ICF: EEUCSeaAnimal)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x21aca0 (ICF: EEUCSeaAnimal)
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x21adff (ICF: EEUCSeaAnimal)
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x20da7a trivial: push dword ptr [esp + 4]; call 0x63511e; ret 4 (ICF: EEUCBasicCitizen, EEUCBasicPriest...)
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x21aef9 (ICF: EEUCSeaAnimal)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88 0x21af45 (ICF: EEUCSeaAnimal)
    virtual void RemoveFromTile(EEUnit*);  // +0x98 0x21aaf5 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x21b010 (ICF: EEUCSeaAnimal)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x21b085
    virtual long Animate(EEComplexUnit*);   // +0xc0 0x20afe9 trivial: push dword ptr [esp + 4]; call 0x638ce7; ret 4 (ICF: EEUCAirAnimal, EEUCAirplane...)
};

#endif
