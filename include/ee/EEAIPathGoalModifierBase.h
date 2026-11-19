// C:\EEX\Empire Earth\AI Thread\EEAIPathGoalModifierBase.h (RECONSTRUCTION, STUB; file name inferred). RTTI
// EEAIPathGoalModifierBase > UReferenceCounter; vtable 0x439c14 (3 slots: ??_G 0x10b24, 0x10b12, 0x10b18).
// A path goal's modifier: EEAIAStarPathfindingGoalBase::SetModifier 0x19bf3 references it and stores itself at +0x08;
// the goals ask it for a step's cost (+0x04: EEAIAStarPathfindingGoal::GetCost 0x19528) and whether a tile may be
// entered (+0x08: AcceptTile 0x19a3f). The slots' bodies sit in EEAIAStarBoundedPathGoalModifiers.cpp. Requires
// lle/UReferenceCounter.h.
#ifndef EE_EEAIPATHGOALMODIFIERBASE_H
#define EE_EEAIPATHGOALMODIFIERBASE_H

class EETile;
struct EeAStarPathGoalRec;                // rust/sim/astar.h (the search's goal: ee_sim::astar path_search.rs)

class EEComplexUnit;

class EEAIPathGoalModifierBase : public UReferenceCounter {
public:
    // the bounded modifier base's ctors store +0x08 before their vptr (the base's init list: 0xfb57 the goal,
    // 0xfbe4/0xfdab zero); EEPathManager's sea/space modifiers assign it in their body
    EEAIPathGoalModifierBase() {}
    EEAIPathGoalModifierBase(EeAStarPathGoalRec* inGoal) : mGoal(inGoal) {}
    // inline: EEAIBoundedPathGoalModifierBase::SetGoal 0xfb80 converts its goal with a test/jmp (an argument) where
    // its ctor's init list uses neg/sbb (provisional name)
    void SetGoal(EeAStarPathGoalRec* inGoal) { this->mGoal = inGoal; }
    // ??_G 0x10b24 stores its own vptr first: a user-declared empty dtor
    virtual ~EEAIPathGoalModifierBase() {}
    // +0x04 0x10b12: the cost of the step (inFromX, inFromY) -> (inX, inY); the parent tile (-10000 when none)
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY) { return 1; }
    // +0x08 0x10b18: whether tile inTile (inX, inY) may be entered
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal) { return inTile != NULL; }

    // 0xf53b / 0xf9bb: the land/sea/space modifiers' shared tests (called on this; provisional name)
    long GetStepCost(bool inSea, long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY);
    // (the walker: inGoal's unit; the original took the goal or the bounded goal, and the tile)
    bool IsPassable(const EeAStarPathGoalRec* inGoal, long inX, long inY, bool inAtGoal);
    // 0xfa75 / 0xfb08 (whether a unit blocks the walker at sea / in space): EEAIAStarView::HasStepBlocker

    EeAStarPathGoalRec* mGoal;              // +0x08 (the goal using it: the Rust search's record, path_search.rs)
};

// RTTI EEAIGraphPathGoalModifierBase > UReferenceCounter; vtable 0x439c4c (3 slots: ??_G 0x10b5a, 0x10b48, 0x10b4e).
// The sub sector graph goal's modifier (EEAIAStarPathGraphGoal::SetModifier 0x1a91e stores the goal at +0x08); its
// route planners (EEAIBoundedPathGoalModifierBase.h) weigh the steps by the influence table. Slot types inferred.
class EEAIPathSubSector;
class AStarGraphableObjectBase;
struct EeAStarGraphGoalRec;               // rust/sim/astar.h (the graph search's goal: ee_sim::astar graph_search.rs)

// A graph search node as the route planners see one (GetCost's parent: its object, the sub sector; the costs as the
// Rust node had them). Names are provisional.
struct EEAIAStarGraphNode {
    AStarGraphableObjectBase* mObject;      // +0x00 (the graph object: the sub sector)
    long mIndex;                            // +0x10 (its id)
    unsigned long mCost;                    // +0x14
    long mPathCost;                         // +0x18
    long mEstimate;                         // +0x1c
    long mTileX;                            // +0x20 (the node's tile)
    long mTileY;
};

// Whether a sub sector has no link to inFrom or one of them has an end of a kind the goal's flags pass (the path
// graph goal's HasPassableLink; its route planners ask it too)
bool EEAIGraphHasPassableLink(const EeAStarGraphGoalRec* inGoal, EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);

class EEAIGraphPathGoalModifierBase : public UReferenceCounter {
public:
    // +0x08 zeroed before the vptr (0xfe55 / 0xfe8f)
    EEAIGraphPathGoalModifierBase() : mGoal(NULL) {}
    virtual ~EEAIGraphPathGoalModifierBase() {}
    // +0x04 0x10b48: the cost of the step from inFrom's sub sector to inTo
    virtual long GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo) { return 1; }
    // +0x08 0x10b4e: whether inSector may be entered
    virtual bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom) { return inSector != NULL; }

    EeAStarGraphGoalRec* mGoal;             // +0x08 (the Rust search's goal record)
};

#endif
