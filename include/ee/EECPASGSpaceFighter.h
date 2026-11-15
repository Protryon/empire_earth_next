// C:\EEX\Empire Earth\AI Thread\EECPASGSpaceFighter.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGSpaceFighter > EECPASGSpaceship; vtable 0x43b5dc; sizeof 0x1e8; ctor 0x4fd61 (inGroup, inArg2:
// EECPASGSpaceship's, then the members), ??_G 0x4fd7d, dtor 0x4fd99. A space fighter attack sub group (no member of
// its own). Slot signatures: the introducing class's (ee/EECPAttackSubGroup.h; checked over every body); names
// are provisional.
#ifndef EE_EECPASGSPACEFIGHTER_H
#define EE_EECPASGSPACEFIGHTER_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGSpaceship.h"

class EECPASGSpaceFighter : public EECPASGSpaceship {
public:
    EECPASGSpaceFighter(EECPAttackGroup* inGroup, long inKind);   // 0x4fd61
    virtual ~EECPASGSpaceFighter();         // +0x00 (0x4fd7d)
    // +0x1c 0x4fda4
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void Disband();                 // +0x68 0x41d2f
    virtual bool GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction);  // +0xe4 0x42220
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x500d8
    virtual void SetDestination(const EECPILocation& inLocation);  // +0xfc 0x50433
    // +0x100 0x5072c
    virtual void SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag);
    virtual void RequestRemoval();          // +0x160 0x423d4
    virtual void Update(long inArg1);       // +0x168 0x5019d
    virtual void Launch(long inTime);       // +0x1e0 0x41ef8
    virtual void GoIdle();                  // +0x1e4 0x4fffb
};

#endif
