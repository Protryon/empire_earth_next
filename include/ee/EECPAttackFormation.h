// EECPAttackFormation.h (RECONSTRUCTION; file name inferred).
// An attack sub group's formation (RTTI EECPAttackFormation, no base; vtable 0x43bc28, 16 slots; EECPAttackFormation.cpp
// 0x529e7..0x54304; sizeof 0x1c: the sub group's ctor news 0x1c bytes and calls the ctor 0x529e7 with itself), its unit
// analysis (RTTI EECPAttackFormationUnitAnalysis, vtable 0x43bc6c, sizeof 0x40: five multimaps of the sub group's
// units; ctor/dtor COMDATs 0x52d55/0x52dc6 right after their first user Virtual08) and its nodes (RTTI
// EECPAttackFormationNode, vtable 0x43bc88, sizeof 0x80; EECPAttackFormationNode.cpp 0x54304..0x5473a: ctor, ??_G,
// dtor, Create 0x543db; the U2DRectangle<float>::Extend COMDAT 0x546dc). Slot names are provisional (by offset);
// argument
// meanings from the node fields they set. Requires lle/LowLevelEngine.h, lle/GE3DPoint.h, lle/U2DRectangle.h, <map>,
// <vector>.
#ifndef EE_EECPATTACKFORMATION_H
#define EE_EECPATTACKFORMATION_H

#include <map>
#include <vector>

class EECPAttackSubGroup;
class EECPAttackSubGroupUnit;
class EECPILocation;
class EEFormation;
class EECPAttackFormation;

// the units by unit type / by weapon range (EEUnitClassUpgrade::mRange) (provisional names)
typedef std::multimap<long, EECPAttackSubGroupUnit*> EECPAFUnitsByType;
typedef std::multimap<float, EECPAttackSubGroupUnit*> EECPAFUnitsByRange;

class EECPAttackFormationUnitAnalysis {
public:
    EECPAttackFormationUnitAnalysis() {}    // COMDAT 0x52d55
    virtual ~EECPAttackFormationUnitAnalysis() {}   // COMDAT 0x52dc6 (??_G 0x52df9)

    EECPAFUnitsByType mU04;                 // +0x04 ranged units of unit kind 6
    EECPAFUnitsByType mFastUnits;           // +0x10 (the class's mIsHorse, mStats->mFAC > 1, kind != 0x18)
    EECPAFUnitsByType mMeleeUnits;          // +0x1c melee units (mStats->mF9C <= 1)
    EECPAFUnitsByRange mRangedUnits;        // +0x28 ranged units by range
    EECPAFUnitsByType mSupportUnits;        // +0x34 (IsPriest, kind 0x1b, or wounded and flagged mHasParentalVengeance)
};

class EECPAttackFormationNode {
public:
    EECPAttackFormationNode();              // 0x54304
    virtual ~EECPAttackFormationNode();     // +0x00 (??_G 0x5439f, dtor 0x543bb)
    // 0x543db: an EEFormation of mType for mUnits at mPosition, each unit sent to its slot (EEUGLocation) (provisional
    // name)
    void Create(EECPAttackFormation* inFormation);

    EECPAttackSubGroupUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EECPAttackSubGroupUnit*> mUnits;   // +0x04
    EEFormation* mFormation;                // +0x14 (referenced: Dereference before a new Create, in the dtor)
    U2DRectangle<float> mBounds;            // +0x18 (Create: the slots' positions)
    GE3DPoint mPosition;                    // +0x28 (Virtual0C: the location's tile centre)
    GE3DPoint mU34;                         // +0x34
    GE3DPoint mU40;                         // +0x40
    GE3DPoint mU4C;                         // +0x4c
    long mType;                             // +0x58 the EEFormation kind (EEFormation::Create)
    EECPAttackFormationNode* mPrevious;     // +0x5c
    long mL60;                              // +0x60
    unsigned long mFlags;                   // +0x64 (1: not a predecessor; 4, 8: set by the slot arguments)
    long mIndex;                            // +0x68 (the column's count)
    float mSpacing;                         // +0x6c (= 1.0f)
    float mF70;                             // +0x70
    float mF74;                             // +0x74
    float mDepth;                           // +0x78 (the depth so far)
    bool mIsPlaced;                         // +0x7c (placed)
};

class EECPAttackFormation {
public:
    EECPAttackFormation(EECPAttackSubGroup* inSubGroup);   // 0x529e7 (ret 4)
    virtual ~EECPAttackFormation();         // +0x00 (??_G 0x52a1a, dtor 0x52a36)
    virtual void Analyze(EECPAttackFormationUnitAnalysis& outAnalysis);    // +0x04 0x52a5b: the sub group's units sorted
    virtual void Build();                   // +0x08 0x52d1c: Virtual10, Virtual04, Virtual1C
    virtual void Place(const EECPILocation& inLocation, float inArg2);        // +0x0c 0x539f7: place every node
    virtual void DeleteNodes();             // +0x10 0x53a85: delete the nodes
    virtual void PlaceNode(EECPAttackFormationNode* inNode, float inArg2) {}   // +0x14 0x163d6f (ICF: `ret 8`)
    virtual long GetColumnWidth(EECPAFUnitsByType& inUnits);  // +0x18 0x52cfa: a column width (3..5)
    virtual void Arrange(EECPAttackFormationUnitAnalysis& inAnalysis);   // +0x1c 0x52e15: Virtual2C
    virtual void Virtual20(EECPAttackFormationUnitAnalysis& inAnalysis); // +0x20 0x52e21
    virtual void Virtual24(EECPAttackFormationUnitAnalysis& inAnalysis); // +0x24 0x52efe
    virtual void Virtual28(EECPAttackFormationUnitAnalysis& inAnalysis); // +0x28 0x52fec
    virtual void ArrangeDefault(EECPAttackFormationUnitAnalysis& inAnalysis); // +0x2c 0x530d0
    // +0x30 0x53569 / +0x38 0x53244: inUnits re-keyed by unit type, then Virtual34 / Virtual3C
    virtual EECPAttackFormationNode* MakeNodesByRange(EECPAFUnitsByRange& inUnits, EECPAttackFormationNode* inPrevious,
        unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing, bool inArg8, bool inArg9);
    // +0x34 0x5360e: rows of inWidth units (inArg8/inArg9: interleave short- and long-sighted units)
    virtual EECPAttackFormationNode* MakeNodes(EECPAFUnitsByType& inUnits, EECPAttackFormationNode* inPrevious,
        unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing, bool inArg8, bool inArg9);
    virtual EECPAttackFormationNode* MakeNodeByRange(EECPAFUnitsByRange& inUnits, EECPAttackFormationNode* inPrevious,
        unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing, bool inArg8, bool inArg9);
    // +0x3c 0x532e9: one node per width-row of the units sorted by range, deepening mF78
    virtual EECPAttackFormationNode* MakeNode(EECPAFUnitsByType& inUnits, EECPAttackFormationNode* inPrevious,
        unsigned long inFlags, long inL60, long inType, long inWidth, float inSpacing, bool inArg8, bool inArg9);

    // inline, refused out of line (ICF-folded into 0x91f11: EECPASGSea::PlaceFormation 0x44dd9f calls it with ecx = the
    // formation) (provisional name; inline-budget evidence)
    unsigned long GetNumNodes() const { return this->mNodes.size(); }
    std::vector<EECPAttackFormationNode*> mNodes;   // +0x04
    EECPAttackSubGroup* mSubGroup;          // +0x14 (referenced)
    bool mNoForcedMove;                     // +0x18 (Create: the EEUGLocation's third flag is !mB18)
};

#endif
