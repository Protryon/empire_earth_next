// C:\EEX\Empire Earth\AI Thread\EECPASGSubmarine.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGSubmarine > EECPASGSea; vtable 0x43b9f0; sizeof 0x1ec; ctor 0x513b7 (inGroup, inArg2: EECPASGSea's,
// then the members), ??_G 0x513da, dtor 0x513f6. A submarine attack sub group. Slot signatures: the introducing
// class's (ee/EECPAttackSubGroup.h, checked over every body). Names are provisional.
#ifndef EE_EECPASGSUBMARINE_H
#define EE_EECPASGSUBMARINE_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGSea.h"

class EECPASGSubmarine : public EECPASGSea {
public:
    EECPASGSubmarine(EECPAttackGroup* inGroup, long inKind);   // 0x513b7
    virtual ~EECPASGSubmarine();            // +0x00 (0x513da)
    virtual void Shutdown();                // +0x30 0x51401
    // +0x94 0x51479: the kept copy of an ICF group (EECPASGSea's identical override folds into it; the body sits in
    // EECPASGSubmarine.cpp's range, so this class overrides the slot)
    virtual void PickSplitUnits(std::vector<EEComplexUnit*>& outUnits);
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies);  // +0x158 0x51722
    virtual bool DecideSplit();             // +0x16c 0x27c0b4 (`xor al, al; ret`)
    virtual void RefreshUnits();            // +0x180 0x517f1
    virtual bool UpdateCanAttackShips();    // +0x1e4 0x5140d

    bool mCanAttackShips;                   // +0x1e8 (init list false; slots +0x30/+0x1e4 clear it, +0x158 reads it)
};

#endif
