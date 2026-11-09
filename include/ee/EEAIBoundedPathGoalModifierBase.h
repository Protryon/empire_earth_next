// C:\EEX\Empire Earth\AI Thread\EEAIBoundedPathGoalModifierBase.h (RECONSTRUCTION; file name inferred). The path goals'
// modifiers (EEAIAStarBoundedPathGoalModifiers.cpp 0xf53b..0x10bc7): RTTI EEAIBoundedPathGoalModifierBase >
// EEAIPathGoalModifierBase > UReferenceCounter (vtable 0x439bc4, 4 slots: ??_G 0x10b24, 0x10b12, 0x10b18, Virtual0C
// 0x10b7e) and its subclasses, the graph goals' modifier
// base EEAIGraphPathGoalModifierBase (vtable 0x439c4c: ??_G 0x10b5a, 0x10b48, 0x10b4e) and its route planners. The
// inline virtual bodies are COMDATs at the modifiers' unit end; the base ctor's out-of-line COMDAT 0x9f9d3 and the wall
// pather's inline ctor are in EECPWallManager.cpp (its first user). Slot argument types inferred.
// Requires lle/UReferenceCounter.h, ee/EEAIPathGoalModifierBase.h.
#ifndef EE_EEAIBOUNDEDPATHGOALMODIFIERBASE_H
#define EE_EEAIBOUNDEDPATHGOALMODIFIERBASE_H

struct EEAIAStarNode;
class EEAIAStarBoundedPathGoal;

class EEAIBoundedPathGoalModifierBase : public EEAIPathGoalModifierBase {
public:
    // 0x9f9d3: +0x08 and +0x0c zeroed, the count 1, the vptr last
    EEAIBoundedPathGoalModifierBase() : EEAIPathGoalModifierBase(0), mBoundedGoal(NULL) {}
    EEAIBoundedPathGoalModifierBase(EEAIAStarBoundedPathGoal* inGoal);   // 0xfb57 (out of line)
    // +0x0c 0x10b7e: called by EEAIAStarBoundedPathGoal::IsGoalNode 0xeb14 with the node (result unused)
    virtual bool CheckNode(EEAIAStarNode* inNode) { return inNode == NULL; }
    void SetGoal(EEAIAStarBoundedPathGoal* inGoal);   // 0xfb80 (both goal pointers)

    EEAIAStarBoundedPathGoal* mBoundedGoal; // +0x0c
};

// RTTI EEAIBPGModifierSpace / EEAIBPGModifierSea (vtables 0x439bd8 / 0x439bec; sizeof 0x10; ctors 0xfb98 / 0xfbb0 out of
// line; both slots ICF-shared: +0x04 0x10b8a GetStepCost(false, ..), +0x08 0x10baa IsPassable(0, mBoundedGoal, ..))
class EEAIBPGModifierSpace : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIBPGModifierSpace(EEAIAStarBoundedPathGoal* inGoal);       // 0xfb98
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(false, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(NULL, this->mBoundedGoal, inX, inY, inGoal);
    }
};

class EEAIBPGModifierSea : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIBPGModifierSea(EEAIAStarBoundedPathGoal* inGoal);         // 0xfbb0
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY)
    {
        return this->GetStepCost(false, inFromX, inFromY, inX, inY, inParentX, inParentY);
    }
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
    {
        return this->IsPassable(NULL, this->mBoundedGoal, inX, inY, inGoal);
    }
};

class EEAIGPGModifierComputerPlayerWallPather : public EEAIBoundedPathGoalModifierBase {
public:
    EEAIGPGModifierComputerPlayerWallPather() {}   // inline (new 0x10, EECPWallManager::PlanWall 0x9d9e9)
    virtual long GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX, long inParentY);     // 0x10814
    virtual bool CanEnter(EETile* inTile, long inX, long inY, bool inGoal);    // 0x1083c
};

#endif
