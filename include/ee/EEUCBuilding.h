// EEUCBuilding.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEUCBuilding.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GE3DPoint, GETransformation.
#ifndef EE_EEUCBUILDING_H
#define EE_EEUCBUILDING_H

#include "ee/EEUnit.h"
#include "ee/EEUnitClassIDs.h"

class EEComplexUnit;
class EEUnit;
class EEUnitClass;

// vtable 0x449520; bases EEUCBuilding > EEUnitClass > UReferenceCounter; sizeof 0x32c; ctor inline; ??_G 0x23626a; dtor 0x2305ee
class EEUCBuilding : public EEUnitClass {
public:
    EEUCBuilding();                         // inline
    EEUCBuilding(EEUnitClassRecord* inRecord, EEPhysicsModel* inModel);   // inline: defined in EEUnitClass.cpp (the class factory 0x2358b1; out of line 0x2361e3)
    EEUnit* FindNearestTargetBuilding(EEUnit* inUnit);  // 0x20ed72 (EnabledChanged passes this in ecx; provisional name)
    virtual ~EEUCBuilding() {}              // +0x00 dtor 0x2305ee
    virtual void GroundLocation(GE3DPoint& ioLocation); // +0x04 0x20e58d (ICF: EEUCBuildingPriest, EEUCGate...)
    virtual EEUnit* CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x30 0x20e909 (ICF: EEUCBuildingPriest)
    virtual EEUnit* CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator); // +0x34 0x20e8ae (ICF: EEUCBuildingPriest)
    virtual void GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long, bool);  // +0x44 0x20ee56 (ICF: EEUCBuildingPriest, EEUCMiningBuilding...)
    virtual bool IsBuilding();              // +0x48 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual bool IsLandBuilding();          // +0x54 0x18aa9 trivial: mov al, 1; ret (ICF: ?$EEAIAStarGoalBase@VEEAIAStarBoundedStorage@@VEEMap@@, ?$EEAIAStarGoalBase@VEEAIAStarChokePointStorage@@VEEMap@@...)
    virtual bool IsPickable() { return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano; }  // +0x60 inline (COMDAT 0x2361ad in EEUnitClass.cpp's range)
    virtual bool IsSelectable(EEUnit* inUnit) { return !this->mIsFlare && this->mUnitType != kUnitTypeVolcano; }  // +0x64 (a unit: EEPlayer::SelectUnit 0x16a53b) inline (COMDAT 0x2361c7 in EEUnitClass.cpp's range)
    virtual long Virtual70() { return 1; }   // +0x70 inline (COMDAT 0x236201, ICF: EECCCloaking, EECCLineOfSight...)
    virtual void ActionChanged(EEComplexUnit* inUnit); // +0x74 0x20f015
    virtual void UnitCreated(EEComplexUnit* inUnit);  // +0x78 0x20f17f (ICF: EEUCMiningBuilding)
    virtual void AddToRegion(EEUnit*);     // +0x80 0x21033d (ICF: EEUCBuildingPriest, EEUCFortress...)
    virtual void AddToTile(EEUnit* inUnit);   // +0x84 0x20f2ae (ICF: EEUCBuildingPriest, EEUCFortress...)
    virtual void RemoveFromRegion(EEUnit*);  // +0x94 0x21034a (ICF: EEUCBuildingPriest, EEUCFortress...)
    virtual void RemoveFromTile(EEUnit*);  // +0x98 0x20fb78 (ICF: EEUCBuildingPriest, EEUCFortress...)
    virtual long EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0xa0 0x20fb85
    virtual void LoadMedia(EEPlayer* inPlayer);           // +0xa8 0x20fb6c trivial: push dword ptr [esp + 4]; call 0x6355e3; ret 4 (ICF: EEUCGate, EEUCMiningBuilding...)
    virtual void UnloadMedia(EEPlayer* inPlayer);         // +0xb4 0x20fd56 trivial: push dword ptr [esp + 4]; call 0x638432; ret 4 (ICF: EEUCGate, EEUCMiningBuilding...)
    virtual void Update(EEComplexUnit* inUnit);      // +0xb8 0x20fd9d
    virtual void UpdateEffects(EEComplexUnit*); // +0xbc 0x20fd62 (ICF: EEUCGate, EEUCMiningBuilding)
    virtual bool SelectUnitToRelease(EEComplexUnit* inUnit);  // +0xe0 0x20e5b8 NEW (ICF: EEUCBuildingPriest, EEUCGate...)
    // EEUCBuilding.cpp's members (provisional names)
    bool EruptVolcano(EEComplexUnit* inUnit);  // 0x20e809 (GetMinHeight 0x7ee: the volcano erupts)
    void ReleaseUnit(EEComplexUnit* inUnit);  // 0x20f2bb (EnabledChanged 0x32ca)
    void UpdateArmed(EEComplexUnit* inUnit);  // 0x20fef9 (VirtualB8: an attacking building)
};

// EEUCBuilding.cpp's helpers and its callees (cdecl; provisional names)
long EEGetDockFacing(unsigned long inX, unsigned long inY);  // 0x20ea89 (the side with the most water: a dock's facing)
long EEGetFacingQuarter(long inX, long inY);  // 0x20ebda (the map quarter 0..3 the tile faces)
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit); // 0x20ed11 (EEGetFacingQuarter at the unit's tile)
void EEGetBlockRing(long inArg, long inX, long inY, long inWidth, long inHeight, std::vector<EETileCoordinate>& outTiles,
                bool inArg7);           // 0x246a42 (the tiles of a rectangle)
float EEGetSpaceDockYaw(long inX, long inY, long inSize);  // 0x23d008 (a yaw for the footprint at (inX, inY))
void EEAddToFootprintTiles(EEUnit* inUnit);  // 0x24caef (NotifyRootVisibility)
void EERemoveFromFootprintTiles(EEUnit* inUnit);  // 0x24d946 (GetMouseChild)

#endif
