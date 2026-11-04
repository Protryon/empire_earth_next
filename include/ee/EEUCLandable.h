// EEUCLandable.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCLandable.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUCLANDABLE_H
#define EE_EEUCLANDABLE_H

#include "ee/EEUnit.h"

class EEComplexUnit;
class EEUnitClass;

// vtable 0x4497d0; bases EEUCLandable > EEUnitClass > UReferenceCounter; sizeof ?; ctor ?; ??_G 0x23649e; dtor 0x2305ee
class EEUCLandable : public EEUnitClass {
public:
    EEUCLandable();                         // inline
    EEUCLandable(EEUnitClassRecord* inRecord, int inIndex);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1; out of line 0x236205)
    EEUCLandable(EEUnitClassRecord* inRecord, EEPhysicsModel* inModel);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1; out of line 0x23622a)
    virtual ~EEUCLandable() {}              // +0x00 dtor 0x2305ee
    virtual bool CanAttackMove();           // +0x10 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual float GetHeightAt(EEComplexUnit* inUnit, const GE3DPoint& inLocation); // +0x24 0x213ed2 (ICF: EEUCAirplane, EEUCPlanetSpaceship)
    virtual void GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch);  // +0x2c 0x2141a3 (ICF: EEUCAirplane, EEUCPlanetSpaceship)
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x2146f0
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x215290 (ICF: EEUCAirplane, EEUCPlanetSpaceship)
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x21aaa9 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual bool HasArrived(EEComplexUnit* inUnit, float inValue);  // +0x88 0x215ed8 (ICF: EEUCAirplane, EEUCPlanetSpaceship)
    virtual void RemoveFromTile(EEUnit*);  // +0x98 0x21aaf5 (ICF: EEUCAirAnimal, EEUCAirplane...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x21655a (ICF: EEUCAirplane, EEUCPlanetSpaceship)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x216a11
    virtual long Animate(EEComplexUnit*);   // +0xc0 0x20afe9 trivial: push dword ptr [esp + 4]; call 0x638ce7; ret 4 (ICF: EEUCAirAnimal, EEUCAirplane...)
    // +0xe0 0x21695d NEW (ICF: EEUCAirAnimal, EEUCAirplane...): the flight state from GetNextFlightState
    virtual void UpdateFlightState(EEComplexUnit* inUnit, long inTime, long inLeft);
    // non-virtual members (EEUCAirAnimal::ReturnToAirport / VirtualB8 call them) (provisional name)
    void FlyToGoal(GE3DPoint& inPoint, EEComplexUnit* inUnit);  // 0x214402
    unsigned long GetNextFlightState(EEComplexUnit* inUnit);  // 0x215ccf (the unit's next flight state: EEComplexUnit::mCurrentPhysicsOrder)
    // EEUCLandable.cpp's other members (provisional names)
    bool ReturnToAirport(EEComplexUnit* inUnit);  // 0x213c60 (EEUCAirAnimal::ReturnToAirport's twin: onto its airport)
    void GetGroundHeight(GE3DPoint& inPoint, float* outHeight);  // 0x214612
    long GetNextFlightPoint(EEComplexUnit* inUnit, GE3DPoint& outTarget, float* outClimb);  // 0x2146a9 (GetNextWaypointPoint's result)
    bool IsBombOnTarget(EEComplexUnit* inUnit);  // 0x2152d0 (a bomb dropped now reaches the target)
    void ApproachCarrier(EEComplexUnit* inUnit, bool inSteep);  // 0x21562f (the carrier approach)
    void TakeOffFromCarrier(EEComplexUnit* inUnit);  // 0x215a4d (the carrier take-off)
    void ApproachRunway(EEComplexUnit* inUnit, long inDX, long inDY);  // 0x215f7d (the runway approach)
    void ApproachRunwayPosX(EEComplexUnit* inUnit);  // 0x216308
    void ApproachRunwayPosY(EEComplexUnit* inUnit);  // 0x216318
    void ApproachRunwayNegY(EEComplexUnit* inUnit);  // 0x216328
    void ApproachRunwayNegX(EEComplexUnit* inUnit);  // 0x216338
    void TakeOffFromRunway(EEComplexUnit* inUnit, long inDX, long inDY);  // 0x216348 (the take-off run)
    void TakeOffFromRunwayPosX(EEComplexUnit* inUnit);  // 0x216430
    void TakeOffFromRunwayPosY(EEComplexUnit* inUnit);  // 0x216440
    void TakeOffFromRunwayNegY(EEComplexUnit* inUnit);  // 0x216450
    void TakeOffFromRunwayNegX(EEComplexUnit* inUnit);  // 0x216460
    bool IsAwayFromGoal(EEComplexUnit* inUnit);  // 0x216470 (away from the carrier's deck point)
};

// cdecl helpers of the flying classes (provisional names; types inferred)
void Fn24B212(const GE3DPoint& inPoint, float inDistance, std::vector<EETileCoordinate>& outTiles);   // 0x24b212 (EEMap.cpp's range)
// 0x24939d: EEFindDropLocation (ee/EEUnit.h)
float EEGetHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inArg3, float inArg4);  // 0x2494cb

#endif
