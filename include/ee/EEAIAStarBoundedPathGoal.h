// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedPathGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarBoundedPathGoal > EEAIAStarGoalBase<EEAIAStarBoundedStorage, EEMap>,
// EEAIAStarPathfindingGoalBase (+0x30); vtables 0x439b64 / 0x439b8c; ctor 0xe9a1 (EEAIAStarBoundedPathGoal.cpp
// 0xe9a1..0xf53b); sizeof 0xbc (EEPathManager's bounded machine keeps it at +0x80, its map at +0x13c). The twin of
// EEAIAStarPathfindingGoal searching inside mBounds, with a bounded-path modifier (+0xb4) next to the base's.
// Slots +0x14..+0x20 are its own. Names are provisional. Requires ee/EEAIAStarStorage.h, ee/EEAIAStarFastStorage.h,
// ee/EEAIAStarBoundedStorage.h, ee/EEAIAStarGoalBase.h, ee/EEAIAStarPathfindingGoalBase.h, <vector>, lle/U2DPoint.h.
#ifndef EE_EEAIASTARBOUNDEDPATHGOAL_H
#define EE_EEAIASTARBOUNDEDPATHGOAL_H
#include "rust/ee_astar.h"

class EEMap;
class EEAIBoundedPathGoalModifierBase;

class EEAIAStarBoundedPathGoal : public EEAIAStarGoalBase<EEAIAStarBoundedStorage, EEMap>, public EEAIAStarPathfindingGoalBase {
public:
    EEAIAStarBoundedPathGoal();             // 0xe9a1
    virtual ~EEAIAStarBoundedPathGoal();    // 0xea01 (??_G 0xe9e5)
    void Reset();                           // 0xea26
    virtual void ReleaseModifier();         // second base +0x04 0xea54 (both modifiers)
    virtual void SearchDone(EEAIAStarMachineBase* inMachine); // +0x08 0xea7b (the machine)
    bool IsGoalNode(EEAIAStarNode* inNode); // 0xeb14
    bool IsStepLimit(long inSteps) const { return false; }   // Expand 0xaaf4c tests only the rev limit
    // +0x14 0xed71 / +0x18 0xecf0 / +0x1c 0xec0e / +0x20 0xec7d (VC6 lists an overload set in reverse order)
    virtual void GetPoints(std::vector<GE3DPoint>& outPath);
    virtual void GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited);
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited);
    virtual void GetPath(std::vector<U2DPoint<long> >& outPath);
    long GetCost(long inFromX, long inFromY, long inX, long inY);          // 0xede4 (the default 0xee51 in ee_astar)
    virtual bool Prepare();                 // +0x0c 0xf1c8 (both bases' slot; the second base's thunk 0xf481)
    virtual void SetModifier(EEAIPathGoalModifierBase* inModifier);   // second base +0x0c 0xf251
    virtual void MarkBlockedTiles();        // second base +0x10 0xf298 (EEAIAStarPathfindingGoal's is folded into it)
    // 0xf4bc (inline COMDAT: also inside mBounds; the default and modifier tests ICF-shared with
    // EEAIAStarPathfindingGoal's): ee_astar path_goals::accept_tile
    bool AcceptTile(long inX, long inY, bool inGoal);
    // New (port): the goal's state for ee_astar's path_goals
    EeAStarPathGoal PathState(EEAIAStarMachineBase* inMachine);

    EEAIAStarBounds mBounds;                // +0xa4 (the search window: EEPathManager::FindPath)
    EEAIBoundedPathGoalModifierBase* mBoundedModifier;   // +0xb4 (referenced; SetModifier's dynamic_cast)
    bool mB8;                               // +0xb8 (= 1; the window is wider than 20 tiles both ways)
};

#endif
