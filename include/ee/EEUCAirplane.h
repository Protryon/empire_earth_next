// EEUCAirplane.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCAirplane.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCAIRPLANE_H
#define EE_EEUCAIRPLANE_H

#include "ee/EEUCLandable.h"

class EEComplexUnit;
class EEUnit;
class EEUCLandable;
template <class T> class U2DPoint;

// vtable 0x449434; bases EEUCAirplane > EEUCLandable > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23649e; dtor 0x2305ee
class EEUCAirplane : public EEUCLandable {
public:
    EEUCAirplane();                         // inline
    EEUCAirplane(EEUnitClassRecord* inRecord);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    EEUCAirplane(EEUnitClassRecord* inRecord, EEPhysicsModel* inModel);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1)
    virtual ~EEUCAirplane() {}              // +0x00 dtor 0x2305ee
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x20b192
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x20b9b4
    // +0xe4 0x21b361 (ICF: EEUCAirAnimal, EEUCHelicopterTransport...): as EEUCTransport's +0xe8 (EEUGUnloadTransport 0x228739)
    virtual void FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination, U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile);
    void FlyOffMap(EEComplexUnit* inUnit);  // 0x20b824 (Virtual74's command 0x3ff: off the map edge) (provisional name)
    // members OPEN: a base's sizeof is unknown (no new site), so this class's own members cannot be placed yet
};

// cdecl helpers (provisional names)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer);  // 0x20b6ff (a carrier's aircraft: the best fighter researched)
EEComplexUnit* EEConvertUnit(EEComplexUnit* inUnit, long inPlayer, bool inIsConversion);  // 0x24e7da (the unit itself, passed on to EEMakeUnitCopy/EEAddDeadUnitStatistic)

#endif
