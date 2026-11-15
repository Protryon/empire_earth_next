// C:\EEX\Empire Earth\AI Thread\EEAIPathGoalModifiers.h (RECONSTRUCTION; file name inferred). The path goals'
// modifiers defined in EEAIAStarBoundedPathGoalModifiers.cpp (0xf53b..0x10bc7) that
// only that unit uses so far: the bounded goal's EEAIBPGModifierEnemyIsAttackableByUnit / WallsDoNotBlock and the
// graph goal's influence-cost route planners (RTTI EEAIGPGModiferUseInfluenceCostsBase [sic] >
// EEAIGraphPathGoalModifierBase > UReferenceCounter, vtable 0x439c38, 4 slots). Slot names by vtable offset, members
// by offset (provisional names). EEAIGPGModifierComputerPlayerWallPlanner (vtable 0x439c8c, over the influence base)
// is declared here over its RTTI bases.
// Requires <vector>, ee/EETileCoordinate.h, ee/EEAIPathGoalModifierBase.h, ee/EEAIBoundedPathGoalModifierBase.h,
// ee/EECPAttackSubGroup.h (EECPASGWeights).
#ifndef EE_EEAIPATHGOALMODIFIERS_H
#define EE_EEAIPATHGOALMODIFIERS_H

class EEComplexUnit;
class EECPAttackSubGroup;

// RTTI EEAIBPGModifierEnemyIsAttackableByUnit (vtable 0x439c00: ??_G 0xfbc8, 0xfcdf, 0xfce5, 0xfc6e; sizeof 0x2c): the
// bounded search ends at the first node within mUnit's range of the goal
class EEAIBPGModifierEnemyIsAttackableByUnit : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIBPGModifierEnemyIsAttackableByUnit(EEComplexUnit* inUnit, EEComplexUnit* inTarget);   // 0xfbe4
    virtual ~EEAIBPGModifierEnemyIsAttackableByUnit();      // 0xfc3e (??_G 0xfbc8)
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY);     // 0xfcdf
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal);    // 0xfce5
    virtual bool CheckNode(EEAIAStarNode* inNode);           // 0xfc6e

    std::vector<EETileCoordinate> mTiles;   // +0x10 (only built and freed here)
    EEComplexUnit* mUnit;                   // +0x20 (referenced; the walker)
    EEComplexUnit* mTarget;                 // +0x24 (referenced)
    float mRangeSquared;                    // +0x28 (the walker's stats +0x9c, squared)
};

// RTTI EEAIBPGModifierWallsDoNotBlock (vtable 0x439c24: ??_G 0x10b24, 0x10814, 0xfdc3, 0x10b7e; sizeof 0x10)
class EEAIBPGModifierWallsDoNotBlock : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIBPGModifierWallsDoNotBlock();       // 0xfdab (out of line)
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY);     // (ICF 0x10814)
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal);    // 0xfdc3
};

// RTTI EEAIGPGModiferUseInfluenceCostsBase (vtable 0x439c38: ??_G 0x10b5a, 0x10b48, 0x10b4e, 0xfec1; sizeof 0x50)
class EEAIGPGModiferUseInfluenceCostsBase : public EEAIGraphPathGoalModifierBase {
public:
    EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID, unsigned long inEnemyID,
                                        const EECPASGWeights& inWeights);   // 0xfe55
    EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID, unsigned long inEnemyID);   // 0xfe8f (weights 0)
    // +0x0c 0xfec1: 1 + the influence at inTo's region: inPlayerID's enemies' (inEnemies), else mEnemyID's
    virtual unsigned long GetInfluenceCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo, bool inEnemies);

    EECPASGWeights mWeights;                // +0x0c
    unsigned long mPlayerID;                // +0x48
    unsigned long mEnemyID;                 // +0x4c
};

// RTTI EEAIGPModifierEnemyAvoidance (vtable 0x439c5c: +0x04 0xff52, +0x08 ICF 0x10aa1; sizeof 0x50)
class EEAIGPModifierEnemyAvoidance : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPModifierEnemyAvoidance(unsigned long inPlayerID);  // 0xff38
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0xff52
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);    // (ICF 0x10aa1)
};

// RTTI EEAIGPGModifierComputerPlayerWallPlanner > EEAIGPGModiferUseInfluenceCostsBase > EEAIGraphPathGoalModifierBase >
// UReferenceCounter (vtable 0x439c8c: ??_G 0x10b5a, 0x10799, 0x107cb, 0xfec1, 0x10655; new 0x50 in EECPWallManager,
// ctor 0x10639 (ret 8) out of line in the modifiers' unit, over the base ctor 0xfe8f). Slots not declared yet
// (provisional name)
class EEAIGPGModifierComputerPlayerWallPlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierComputerPlayerWallPlanner(unsigned long inPlayerID, long inEnemyID);  // 0x10639
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // +0x04 0x10799
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);    // +0x08 0x107cb
    // +0x10 0x10655: whether a tile of inSector holds no live gate, or one inPlayerID may not pass (provisional name)
    virtual bool IsGateOpen(EEAIPathSubSector* inSector, unsigned long inPlayerID);
};

// RTTI EEAIGPGModifierLandAttackSubGroupRoutePlanner (vtable 0x439c70: +0x04 0x10074, +0x08 0x10465; sizeof 0x5c)
class EEAIGPGModifierLandAttackSubGroupRoutePlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierLandAttackSubGroupRoutePlanner(EECPAttackSubGroup* inSubGroup, unsigned short inContinent1,
                                                  unsigned short inContinent2, bool inAnyContinent,
                                                  const EECPASGWeights& inWeights);   // 0x1001a
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0x10074
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);    // 0x10465

    EECPAttackSubGroup* mSubGroup;          // +0x50
    unsigned short mContinent1;             // +0x54
    unsigned short mContinent2;             // +0x56
    bool mAnyContinent;                     // +0x58
    bool mAvoidFriendlyBlockers;            // +0x59 (= true)
};

// RTTI EEAIGPGModifierLandWallAvoider, vtable0x43afd4, allocation0x5c.
// Original49387/49b5d construct the actual parent with zero continents and false;
// no additional fields. Cost0x45d71 lives in the Helicopter original TU.
class EEAIGPGModifierLandWallAvoider : public EEAIGPGModifierLandAttackSubGroupRoutePlanner {
public:
    EEAIGPGModifierLandWallAvoider(EECPAttackSubGroup* inSubGroup, const EECPASGWeights& inWeights)
        : EEAIGPGModifierLandAttackSubGroupRoutePlanner(inSubGroup, 0, 0, false, inWeights) {}
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);
};

// RTTI EEAIGPGModifierSeaRoutePlanner / HelicopterAttackSubGroupRoutePlanner / AirplaneAttackSubGroupRoutePlanner /
// SpaceRoutePlanner (vtables 0x439ca4 / 0x439cb8 / 0x439ccc / 0x439ce0; sizeof 0x54); the slots are ICF-shared:
// +0x04 0x10a20 (the airplane's 0x10985), +0x08 0x10aa1
class EEAIGPGModifierSeaRoutePlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierSeaRoutePlanner(unsigned long inPlayerID, EECPAttackSubGroup* inSubGroup,
                                   const EECPASGWeights& inWeights);   // 0x108d6
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);

    EECPAttackSubGroup* mSubGroup;          // +0x50
};

class EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner(EECPAttackSubGroup* inSubGroup,
                                                        const EECPASGWeights& inWeights);   // 0x10909
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);

    EECPAttackSubGroup* mSubGroup;          // +0x50
};

class EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner(EECPAttackSubGroup* inSubGroup,
                                                      const EECPASGWeights& inWeights);   // 0x10947
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0x10985
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);

    EECPAttackSubGroup* mSubGroup;          // +0x50
};

class EEAIGPGModifierSpaceRoutePlanner : public EEAIGPGModiferUseInfluenceCostsBase {
public:
    EEAIGPGModifierSpaceRoutePlanner(unsigned long inPlayerID, EECPAttackSubGroup* inSubGroup,
                                     const EECPASGWeights& inWeights);   // 0x109ed
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo);     // 0x10a20
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);    // 0x10aa1

    EECPAttackSubGroup* mSubGroup;          // +0x50
};

#endif
