// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathfindingGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarPathfindingGoal > EEAIAStarGoalBase<EEAIAStarStorage, EEMap>,
// EEAIAStarPathfindingGoalBase (+0x30); vtables 0x439ffc / 0x43a014; ctor 0x190bf (EEAIAStarPathfindingGoal.cpp
// 0x190bf..0x19a93); sizeof 0xa4 (EEPathManager's path machine keeps it at +0x6c, its map at +0x110). The machine's
// slots call the members below non-virtually. Names are provisional. Requires ee/EEAIAStarStorage.h,
// ee/EEAIAStarGoalBase.h,
// ee/EEAIAStarPathfindingGoalBase.h, ee/EEMap.h, <vector>, lle/U2DPoint.h, lle/GE3DPoint.h.
#ifndef EE_EEAIASTARPATHFINDINGGOAL_H
#define EE_EEAIASTARPATHFINDINGGOAL_H
#include "rust/ee_astar.h"

class EEMap;

class EEAIAStarPathfindingGoal : public EEAIAStarGoalBase<EEAIAStarStorage, EEMap>, public EEAIAStarPathfindingGoalBase {
public:
    EEAIAStarPathfindingGoal();             // 0x190bf
    virtual ~EEAIAStarPathfindingGoal();    // 0x190ff (??_G 0x190e3 calls it)
    void Reset();                           // 0x19124
    // +0x08 0x19138: the search is over (no open node left, the goal's neighbourhood reached, or the revs used up);
    // inMachine is the EEAIAStarMachineBase running it (EEAIAStarGoalBase<S, M>::Virtual08 takes it)
    virtual void SearchDone(EEAIAStarMachineBase* inMachine);
    bool IsGoalNode(EEAIAStarNode* inNode); // 0x191d1
    void GetPath(std::vector<U2DPoint<long> >& outPath);   // 0x1929e
    void GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited);   // 0x19315
    void GetPoints(std::vector<GE3DPoint>& outPath);  // 0x19388
    void GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited);  // 0x19429
    // 0x19528 (the modifier's, else the default 0x19595): ee_astar path_goals::get_cost
    long GetCost(long inFromX, long inFromY, long inX, long inY);
    virtual bool Prepare();                 // +0x0c 0x19983 (both bases' slot; the second base's thunk 0x199f2)
    virtual void MarkBlockedTiles();        // second base +0x10 (ICF-folded into EEAIAStarBoundedPathGoal's 0xf298)
    // 0x199fa (the modifier's 0x19a3f, else the default 0x1990c): ee_astar path_goals::accept_tile
    bool AcceptTile(long inX, long inY, bool inGoal);
    // New (port): the goal's state for ee_astar's path_goals
    EeAStarPathGoal PathState(EEAIAStarMachineBase* inMachine);
};

#endif
