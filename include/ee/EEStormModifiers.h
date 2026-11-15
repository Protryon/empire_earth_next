// C:\EEX\Empire Earth\Empire Earth\EEStormModifiers.h (RECONSTRUCTION; file name inferred). The storm's path modifiers
// (EEEMStorm.cpp): the bounded goal's EEAIBPGModifierStormShaper (RTTI > EEAIBoundedPathGoalModifierBase; vtable
// 0x441f2c: ??_G 0x10b24 (base's), +4 0x12e3ee, +8 0x12e48b, +0xc 0x12e3d8; new 0x40, ctor 0x12e395 out of line) and
// the graph goal's route planners (RTTI EEAIGPGModifierStormRoutePlannerRain/ Snow >
// EEAIGPGModifierStormRoutePlannerBase > EEAIGraphPathGoalModifierBase; vtables 0x441f4c/0x441f5c: ??_G 0x10b5a, +4
// 0x12dd54 / 0x12dcca, +8 0xac9ee (`mov al,1; ret 8`, ICF-shared); new 0xc, ctors inline: count 1, mGoal 0, one vptr).
// Requires lle/U2DRectangle.h, ee/EEAIPathGoalModifierBase.h, ee/EEAIBoundedPathGoalModifierBase.h.
#ifndef EE_EESTORMMODIFIERS_H
#define EE_EESTORMMODIFIERS_H
#include "rust/ee_astar.h"

class EEAIBPGModifierStormShaper : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIBPGModifierStormShaper();           // 0x12e395 (base: +8/+0xc 0; rectangles; vptr; +0x34, +0x38, +0x30 = 0x40, +0x3d, +0x3c)
    // +0x04 0x12e3ee: 40, or 1 on snow terrain (kind 9) / a hill, 5 off hills unless kind 1 (snow); 1 on kinds 1/10 (rain)
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY);
    // +0x08 0x12e48b: inside mInner: true; inside mOuter: random <= k; else random by the distance to the goal
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal);
    // +0x0c 0x12e3d8: `return inNode == 0 || mL34 >= mL38;`
    virtual bool CheckNode(EEAIAStarNode* inNode);
    // New (port): the state ee_astar's storm_cost / storm_can_enter read
    EeAStarStorm StormState() const;

    U2DRectangle<long> mInner;              // +0x10 (the start +-4, clamped to the map)
    U2DRectangle<long> mOuter;              // +0x20 (mInner +-4 again)
    long mDistanceSquared;                  // +0x30 (= 0x40)
    long mSteps;                            // +0x34 (= 0)
    long mMaxSteps;                         // +0x38 (= 0; Fn12DF63: GetRandom(0x2d, 0x87))
    bool mSnow;                             // +0x3c (types 2 and 4)
    bool mRain;                             // +0x3d (types 1 and 3)
};

class EEAIGPGModifierStormRoutePlannerBase : public EEAIGraphPathGoalModifierBase {
public:
    EEAIGPGModifierStormRoutePlannerBase() {}
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom) { return true; }    // 0xac9ee (ICF)
};

class EEAIGPGModifierStormRoutePlannerRain : public EEAIGPGModifierStormRoutePlannerBase {
public:
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0x12dd54
};

class EEAIGPGModifierStormRoutePlannerSnow : public EEAIGPGModifierStormRoutePlannerBase {
public:
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0x12dcca
};

#endif
