// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathfindingGoalBase.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarPathfindingGoalBase, no base; vtable 0x43a04c (6 slots; +0x10 pure);
// EEAIAStarPathfindingGoalBase.cpp 0x19a93..0x1a64b. The path search goal's common part (EEAIAStarPathfindingGoal and
// EEAIAStarBoundedPathGoal have it at +0x30): the unit that walks (referenced), its modifier, the start/goal bounds.
// Slots named by vtable offset, members by offset (provisional names). Requires ee/EEAIAStarStorage.h
// (EEAIAStarBounds),
// ee/EEUnit.h, ee/EEAIPathGoalModifierBase.h, <vector>.
// slot +0x08 is named Virtual0C (the derived goals' one Virtual0C 0xf1c8/0x19983 overrides it and
// EEAIAStarGoalBase's +0x0c: one name in both bases); Virtual14 takes the goal point; mUnit is a complex unit
// (EEUnitClass::CanMoveOnTerrain and EEUnit::IsPassableBy take it); SetStart/SetGoal take the goal base's U2DPoint<long>.
#ifndef EE_EEAIASTARPATHFINDINGGOALBASE_H
#define EE_EEAIASTARPATHFINDINGGOALBASE_H

#include "ee/EEMathInline.h"   // Estimate's UMAbs(long): the original's COMDAT is the integer body (ICF twin 0x4ab2bd)

class EEComplexUnit;

class EEAIAStarPathfindingGoalBase {
public:
    EEAIAStarPathfindingGoalBase();         // 0x19a93
    virtual ~EEAIAStarPathfindingGoalBase();   // +0x00 (dtor 0x19b22)
    virtual void ReleaseModifier();         // +0x04 0x19b6e
    virtual bool Prepare();                 // +0x08 0x19b84
    virtual void SetModifier(EEAIPathGoalModifierBase* inModifier);   // +0x0c 0x19bf3
    virtual void MarkBlockedTiles() = 0;    // +0x10 (pure; provisional name, not Virtual10: it would hide EEAIAStarGoalBase::Attach in the derived goals, which the machine's Run calls)
    // +0x14 0x19ccb: mBlockedTiles = the unit's blocked tiles, plus (class flag 4) the tiles of the units the
    // EEAIPathBlockerCollector finds around inGoal
    virtual void CollectBlockedTiles(const U2DPoint<long>& inGoal);
    // inline (COMDAT 0xab2bd after the path machine's Start 0xaa915; expanded in its AddNode 0xab504): the
    // Manhattan distance times 10 (provisional name)
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const
    {
        return (UMAbs(inFrom.GetX() - inTo.GetX()) + UMAbs(inFrom.GetY() - inTo.GetY())) * 10;
    }

    void CollectUnitTiles();                // 0x19b4b (the unit's class flag 4: 0x248fe4(unit, mUnitTiles, 0.0f))
    void Reset();                           // 0x19b9d
    void SetUnit(EEComplexUnit* inUnit);    // 0x19c0f (referenced; its continent id, mAvoidUnits from its player/class)
    void SetGoalBounds(const U2DPoint<long>& inTile, long inMargin);  // 0x19c77 (mGoalBounds = the tile, grown)
    void SetStartBounds(const U2DPoint<long>& inTile, long inMargin);  // 0x19ca1 (mStartBounds = the tile, grown)
    // inline (COMDAT 0xf489 after its first user EEAIAStarBoundedPathGoal::Reset; EEAIAStarPathfindingGoal::Reset
    // 0x19124 calls it too) (provisional name)
    void ClearSearch()
    {
        this->Reset();
        this->ReleaseModifier();
        this->mStartBounds.Clear();
        this->mGoalBounds.Clear();
        this->mBlockedTiles.clear();
    }

    std::vector<U2DPoint<long> > mBlockedTiles; // +0x04
    std::vector<U2DPoint<long> > mUnitTiles; // +0x14
    EEAIAStarBounds mBounds24;              // +0x24
    EEAIAStarBounds mGoalBounds;            // +0x34
    EEAIAStarBounds mStartBounds;           // +0x44
    U2DPoint<long> mDestination;            // +0x54 (ctor: zeroed by its default ctor before mF68; Q1)
    EEComplexUnit* GetUnit() const { return this->mUnit; }
    EEComplexUnit* mUnit;                   // +0x5c (referenced)
    EEAIPathGoalModifierBase* mModifier;    // +0x60 (referenced)
    long mL64;                              // +0x64
    float mOffsetX;                         // +0x68
    float mOffsetY;                         // +0x6c
    unsigned short GetContinentID() const { return this->mContinentID; }
    unsigned short mContinentID;            // +0x70 (the unit's)
    bool mIsBounded;                        // +0x72
    bool mAvoidUnits;                       // +0x73
};

#endif
