// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathGraphGoal.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarPathGraphGoal > EEAIAStarGraphGoalBase<EEAIAStarGraphStorage>; vtable
// 0x43a06c (8 slots); ctor 0x1a64b (EEAIAStarPathGraphGoal.cpp 0x1a64b..0x1a9ec); sizeof 0x74 (EEPathManager's graph
// machine keeps it at +0x64). The sub sector graph search's goal: from mFrom to mTo, sub sectors whose kind the walker's
// flags allow. Names are provisional. Requires ee/EEAIAStarGraphStorage.h, ee/EEAIAStarGraphGoalBase.h,
// ee/EEAIPathGoalModifierBase.h, ee/EECPAttackSubGroup.h (EECPASGFlags), <vector>.
#ifndef EE_EEAIASTARPATHGRAPHGOAL_H
#define EE_EEAIASTARPATHGRAPHGOAL_H

class EEAIPathSubSector;
class EEAIAStarGraphMachineBase;
class EEComplexUnit;
struct EEAIAStarGraphNode;

class EEAIAStarPathGraphGoal : public EEAIAStarGraphGoalBase<EEAIAStarGraphStorage> {
public:
    EEAIAStarPathGraphGoal();               // 0x1a64b
    virtual ~EEAIAStarPathGraphGoal();      // 0x1a6a8 (??_G 0x1a68c)
    virtual void ReleaseModifier();         // +0x14 (ICF 0x65d0c)
    // +0x18 0x1a6c7: the search is over (no open node, the goal reached, or the revs used up); inMachine is the
    // EEAIAStarGraphMachineBase running it
    virtual void SearchDone(EEAIAStarGraphMachineBase* inMachine);
    // the graph machine's hooks (COMDATs after EEPathManager.cpp's path machine) (provisional names)
    EEAIPathSubSector* GetStartObject() const { return this->mFrom; }
    unsigned short GetGoalID() const;                       // (0xaacf0 reads mTo's id inline)
    long Estimate(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo) const;   // 0xab2bd (Manhattan * 10)
    bool CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);   // 0xab2e5 (mModifier's +0x08, else AcceptNeighbor)
    long GetStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo);   // 0xab7bb (mModifier's +0x04, else 0x10799)
    long GetDefaultStepCost(EEAIAStarGraphNode* inParent, EEAIPathSubSector* inTo);   // (ICF 0x10799)
    bool IsGoalNode(EEAIAStarGraphNode* inNode);   // 0x1a76f
    void SetFlags(EEComplexUnit* inUnit);   // 0x1a7d0 (the unit class's Virtual28 fills mPathFlags)
    bool AcceptNeighbor(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);   // 0x1a7ec
    bool HasPassableLink(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom);  // 0x1a830
    virtual bool Prepare();                 // +0x0c 0x1a8c2
    void Reset();                           // 0x1a8e6
    virtual void SetModifier(EEAIGraphPathGoalModifierBase* inModifier);   // +0x1c 0x1a91e
    void SetUnit(EEComplexUnit* inUnit);    // 0x1a93a
    void GetPath(std::vector<long>& outPath);   // (ICF 0x15e50)
    void GetPath(std::vector<U2DPoint<long> >& outTiles, std::vector<long>& outIndices);   // (ICF 0x15ebc)

    U2DPoint<long> mPoint38;                // +0x38
    U2DPoint<long> mPoint40;                // +0x40 (the goal sector's location: Virtual0C)
    EEComplexUnit* mUnit;                   // +0x48 (referenced; EEPathManager::SetGraphRevs: its player, +0x353)
    EEAIGraphPathGoalModifierBase* mModifier;   // +0x4c (referenced)
    EEAIPathSubSector* GetFrom() const { return this->mFrom; }
    EEAIPathSubSector* mFrom;               // +0x50
    EEAIPathSubSector* GetTo() const { return this->mTo; }
    EEAIPathSubSector* mTo;                 // +0x54
    EECPASGFlags mPathFlags;                // +0x58 (the walker's passable sub sector kinds; the request's flags:
                                            // operator= 0xaa1da)
    long mL70;                              // +0x70
};

#endif
