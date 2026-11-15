// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphContinentalChainGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarGraphContinentalChainGoal > EEAIAStarGraphGoalBase<EEAIAStarGraphTerrainStorage>;
// vtable 0x439eb4 (6 slots: the base's 5, +0x14 an empty `ret 4` ICF 0x156dd8; ??_G ICF-folded into the base's 0x16053:
// the dtor is implicit). EEAIAStarGraphContinentalChainGoal.cpp 0x15dae..0x16070: the graph search that chains the
// continents of the allowed kinds (EEAIContinent::mCategory) and numbers each chain. Members by offset (provisional
// names).
// Requires ee/EEAIAStarGraphGoalBase.h, ee/EEAIAStarGraphTerrainStorage.h, ee/EEAIContinent.h, <vector>.
#ifndef EE_EEAIASTARGRAPHCONTINENTALCHAINGOAL_H
#define EE_EEAIASTARGRAPHCONTINENTALCHAINGOAL_H

class EEAIAStarGraphContinentalChainGoal : public EEAIAStarGraphGoalBase<EEAIAStarGraphTerrainStorage> {
public:
    EEAIAStarGraphContinentalChainGoal();   // 0x15dae
    // +0x14 (ICF 0x156dd8): the search is over (the graph machine's Expand 0x274a3 calls it with itself; the path
    // goal's +0x18 twin is EEAIAStarPathGraphGoal::SearchDone) (provisional name)
    virtual void SearchDone(EEAIAStarGraphMachineBase* inMachine) {}

    // the graph machine's hooks, all expanded inline (0x27434/0x274a3/0x28a34/0x28aa7) (provisional names)
    bool IsGoalNode(EEAIAStarGraphNode* inNode) { return inNode == NULL; }  // a flood: runs until the list is empty
    EEAIContinent* GetStartObject() const { return this->mStartContinent; }
    unsigned short GetGoalID() const { return 0; }
    bool CanEnter(EEAIContinent* inNeighbor, EEAIContinent* inFrom) { return this->AcceptNeighbor(inNeighbor, inFrom); }
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const { return 1; }
    long GetStepCost(EEAIAStarGraphNode* inParent, EEAIContinent* inObject) const { return 1; }

    void NumberChain();                     // 0x15dd5: every node's continent gets the next id of mKind (EEAIContinentManager)
    void GetPath(std::vector<long>& outIndices);   // 0x15e50 (the path's node indices)
    // 0x15ebc: the path's tiles (consecutive duplicates once) and node indices
    void GetPath(std::vector<EETileCoordinate>& outTiles, std::vector<long>& outIndices);
    // 0x15f97 (caller 0x274a3, the graph machine's expand step): whether the search may enter inNeighbor from inFrom
    bool AcceptNeighbor(EEAIContinent* inNeighbor, EEAIContinent* inFrom);

    EEAIContinent* mStartContinent;         // +0x38 the start continent (NumberContinentChains; Start reads its id)
    unsigned long mKind;                    // +0x3c (an EEAIContinentManager::mNextIDs / EEAIContinent::mCounts index)
    long mCount;                            // +0x40 (counted by AcceptNeighbor)
    bool mStartCategory;                    // +0x44
    bool mKinds[5];                         // +0x45 (by EEAIContinent::mCategory: may the chain cross it)
};

#endif
