// EEUCNavalTransport.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCNavalTransport.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCNAVALTRANSPORT_H
#define EE_EEUCNAVALTRANSPORT_H

#include "ee/EEUCTransport.h"

class EEUCTransport;
class EEUnit;

// vtable 0x448718; bases EEUCNavalTransport > EEUCTransport > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23647c; dtor 0x2305ee
class EEUCNavalTransport : public EEUCTransport {
public:
    EEUCNavalTransport();                   // inline
    EEUCNavalTransport(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x218a45 (ICF: EEUCNaval, EEUCNavalCitizen)
    virtual void LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY); // +0x08 0x218490 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual void AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth); // +0x0c 0x217c6d (ICF: EEUCNaval)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x218605 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x218909 (ICF: EEUCNaval, EEUCNavalBuilding...)
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x21aaa9 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88 0x218d4b
    virtual void RemoveFromTile(EEUnit*);  // +0x98 0x21aaf5 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x217eb2 (ICF: EEUCNaval)
    // +0xe4..+0x104: EEUCTransport's signatures
    virtual void FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport, U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile); // +0xe4 0x21a6e0 (ICF: EEUCSpaceTransport)
    virtual void FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile); // +0xe8 0x218a5f (ICF: EEUCSpaceTransport)
    virtual bool HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo);   // +0xf0 0x2135bc trivial: mov al, 1; ret 8 (ICF: EECCCloaking, EEUCHelicopterTransport...)
    virtual bool CanCarry(EEComplexUnit* inUnit1, EEComplexUnit* inUnit2);  // +0xf4 0x21a838 (ICF: EEUCSpaceTransport)
    virtual bool CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY); // +0xf8 0x218c48 (the shore tile's continents)
    virtual bool FindBoardingLocation(EEComplexUnit* inUnit, EEComplexUnit* inTransport, GE3DPoint& inLocation, GE3DPoint& outLocation); // +0xfc 0x218d33 (ICF: EEUCFortress, EEUCHelicopterTransport...)
    virtual void UnloadAt(EEComplexUnit* inTransport, long inX, long inY);   // +0x104 0x218e14 (ICF: EEUCLandTransport)
    // members OPEN: a base's sizeof is unknown (no new site), so this class's own members cannot be placed yet
};

#endif
