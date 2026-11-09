// C:\EEX\Empire Earth\AI Thread\EECPCombatStrategy.h (RECONSTRUCTION; file name inferred). A computer
// player's combat strategy (RTTI EECPCombatStrategy, no base; vtable 0x43c0a4: the destructor only; sizeof 0x4c: new
// 0x4c in Clone 0x773bf). Built on the stack by EECPAttackCommander 0x51bb9/0x51d8e and EECPAttackGroup, kept by
// EECPUnitSelector at +0xac. EECPCombatStrategy.cpp 0x77084..0x77718. Member names are provisional.
// Requires <vector>, ee/EECPUnitRequirement.h.
#ifndef EE_EECPCOMBATSTRATEGY_H
#define EE_EECPCOMBATSTRATEGY_H

#include <vector>

class EECPIntelligencePlayerFile;

class EECPCombatStrategy {
public:
    EECPCombatStrategy();                   // 0x77084
    EECPCombatStrategy(const EECPCombatStrategy& inOther);   // 0x770f6
    virtual ~EECPCombatStrategy();          // +0x00 0x7716d (??_G 0x770da)

    void Reset();                           // 0x7717b
    // 0x771be (ret 8): splits mTotal over the four shares by what inOwn lacks (inOther unused)
    void Calculate(EECPIntelligencePlayerFile* inOwn, EECPIntelligencePlayerFile* inOther);
    EECPCombatStrategy* Clone();            // 0x773bf
    // 0x97c53 (EECPUnitSelector.cpp, between AllocateAlreadyDefinedUnits and AllocateHeldUnits; both call it out of
    // line at a depth-1 site, so not an inline): one unit of mL30 less, not below 0 (provisional name)
    void DecrementRemaining();
    // inline (COMDAT 0x7366e in EECPCombatManager.cpp's range, its first user 0x47360a; EECPUnitSelector 0x49ad60 calls
    // it out of line) (provisional name)
    unsigned long GetNumRequirements() const { return this->mRequirements.size(); }

    std::vector<EECPUnitRequirement> mRequirements;   // +0x04
    float mAirShare;                        // +0x14 the share of the first count (inOwn->mL7C)
    float mSeaShare;                        // +0x18 (inOwn->mL78)
    float mLandShare;                       // +0x1c (inOwn->mL80)
    float mSpaceShare;                      // +0x20 (inOwn->mL84)
    long mTownID;                           // +0x24
    short mContinentID;                     // +0x28
    long GetNumUnits() const { return this->mNumUnits; }
    long mNumUnits;                         // +0x2c the total
    long GetNumRemaining() const { return this->mNumRemaining; }
    long mNumRemaining;                     // +0x30
    long GetNumAir() const { return this->mNumAir; }
    long mNumAir;                           // +0x34 the four counts Calculate fills
    long GetNumSea() const { return this->mNumSea; }
    long mNumSea;                           // +0x38
    long GetNumLand() const { return this->mNumLand; }
    long mNumLand;                          // +0x3c (also gets the remainder)
    long GetNumSpace() const { return this->mNumSpace; }
    long mNumSpace;                         // +0x40
    long GetKind() const { return this->mKind; }
    long mKind;                             // +0x44
    bool mAddFavorites;                     // +0x48
};

#endif
