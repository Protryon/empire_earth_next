// C:\EEX\Empire Earth\Graphics Thread\EEGraphicalGoal.h (RECONSTRUCTION; file name inferred). One
// "go here" marker of the selected units (RTTI-less, vtable 0x4412a0: the deleting dtor only), held by value (0x28 bytes,
// the vectors' idiv 0x28) in EEGraphicalGoalManager's map. Type 0 is a marching line between mFrom and mTo (graphic
// effect 0x60), types 1..6 an icon placed at mTo (0x62, 0xd2, 0xd4, 0xd5, 0xd6, 0xf5). Defined by
// EEGraphicalGoalManager.cpp 0x107616..0x10779d. Names are provisional. Requires lle/GE3DPoint.h, ee/EEEffect.h.
#ifndef EE_EEGRAPHICALGOAL_H
#define EE_EEGRAPHICALGOAL_H

class EEGraphicalGoal {
public:
    EEGraphicalGoal(long inType, long inPlayer);   // 0x107616 (inPlayer: the effect's mL20)
    virtual ~EEGraphicalGoal();             // 0x1076e3 (empty; called out of line for the vector temporaries)
    void DestroyEffect();                   // 0x1076ea
    void Update();                          // 0x1076f9: moves the effect when mChanged

    GE3DPoint mFrom;                        // +0x04
    GE3DPoint mTo;                          // +0x10
    EEEffectHandle mEffect;                 // +0x1c
    unsigned long mType;                    // +0x20 (unsigned: Update`s `cmp 6; ja`)
    bool mChanged;                          // +0x24 (not set by the ctor)
};

#endif
