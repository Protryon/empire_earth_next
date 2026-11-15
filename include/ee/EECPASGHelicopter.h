// C:\EEX\Empire Earth\AI Thread\EECPASGHelicopter.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGHelicopter > EECPASGLand; vtable 0x43ab10; sizeof 0x250; ctor 0x4597f (inGroup, inArg2: EECPASGLand's,
// then the members), ??_G 0x4599b, dtor 0x459b7. A helicopter attack sub group (no member of its own: sizeof
// EECPASGLand's). Slot signatures: the introducing class's (ee/EECPAttackSubGroup.h; checked over every body); names
// are provisional.
#ifndef EE_EECPASGHELICOPTER_H
#define EE_EECPASGHELICOPTER_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGLand.h"

class EECPASGHelicopter : public EECPASGLand {
public:
    EECPASGHelicopter(EECPAttackGroup* inGroup, long inKind);   // 0x4597f
    virtual ~EECPASGHelicopter();           // +0x00 (0x4599b)
    // +0x1c 0x459c2
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights);   // +0x54 0x459d6
    virtual void PickSplitUnits(std::vector<EEComplexUnit*>& outUnits);  // +0x94 0x45a00
    virtual float GetGuardHighGroundPercentage();  // +0xac 0x45a4e
    virtual float GetGuardContinentPercentage();  // +0xb0 0x41948 (`fld dword ptr [0x83948c]; ret`)
    virtual bool CanReach(const EECPILocation* inLocation);    // +0xc8 0xb3257 (`mov al, 1; ret 4`)
    virtual bool TakeRequestedUnits();      // +0x154 0x459e6
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies);  // +0x158 0x45a68
    virtual bool DecideSplit();             // +0x16c 0x27c0b4 (`xor al, al; ret`)
    virtual void RequestPath(void* inFrom, void* inTo);   // +0x178 0x45b25
    virtual void PrunePath();               // +0x184 0xe123d (`ret`)
};

#endif
