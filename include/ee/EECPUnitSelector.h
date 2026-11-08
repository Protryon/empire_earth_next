// C:\EEX\Empire Earth\AI Thread\EECPUnitSelector.h (RECONSTRUCTION; file name inferred). A computer player's unit
// selector (RTTI EECPUnitSelector, a root class; vtable 0x43c29c: the destructor only; new 8 in EECPCombatManager's
// ctor 0x71d74, kept at EECPCombatManager +0x40) and the request it fills
// (EECPUnitSelectionGroup, RTTI struct, vtable 0x43bcb0; a 0xb0-byte local of EECPAttackGroup 0x57391/0x579f4, ctor
// 0x57470 in EECPAttackGroup.cpp), plus the function objects EECPUnitSelector.cpp builds on the stack. Member names
// (provisional name; "AllocateAlreadyDefinedUnits" is named by its trace string)
// Requires <map>, <vector>, lle/LowLevelEngine.h (UString), ee/EECPAGTask.h, ee/EECPILocation.h,
// ee/EECPCombatStrategy.h, ee/EEUnit.h.
#ifndef EE_EECPUNITSELECTOR_H
#define EE_EECPUNITSELECTOR_H

#include <map>
#include <vector>

class EEComputerPlayer;
class EECPCombatStrategy;
class EECPIntelligencePlayerFile;
class EECPUnitRequirement;
class EEPlayer;
class EEUnit;
class EEUnitClass;

// A computer player's personnel gatherer (RTTI struct EEAIFOPAvailablePersonnelGatherer, vtable 0x43bcb8: ??_G 0x5750e;
// EECPUnitSelectionGroup +0x64). Its members live before EEComputerPlayer.cpp (0x2a486..0x2a5de). (provisional names)
struct EEAIFOPAvailablePersonnelGatherer {
    virtual ~EEAIFOPAvailablePersonnelGatherer() {}   // +0x00 (??_G 0x5750e; inline: the ??_G and EECPUnitSelectionGroup dtor 0x457533 expand it)
    void AddUnit(const std::pair<long, EEUnit*>& inUnit);   // 0x2a486 (ret 4): counts inUnit's class (a copy of the map entry)
    long TakeUnitClass(long inUnitClass);   // 0x2a4d9 (ret 4): takes one of inUnitClass (the class, 0 if none)
    long TakeByCategory(long inCategory);   // 0x2a51e (ret 4): a gathered class of that category (+0x1b8), 0 if none
    long TakeByKind(long inKind);           // 0x2a57e (ret 4): a gathered class of that kind (+0x1d0), 0 if none

    // +0x04: unit class -> (count, class) (0x2a57e: the count at node +0x10, the class at +0x14)
    std::map<long, std::pair<long, EEUnitClass*> > mClasses;
};

struct EECPUnitSelectionGroup {
    EECPUnitSelectionGroup();               // 0x57470 (EECPAttackGroup.cpp)
    virtual ~EECPUnitSelectionGroup();      // +0x00 (??_G 0x5759f)

    std::vector<long> mSelected;            // +0x04 the unit classes chosen (to build or train)
    long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<long> mUnitIDs;             // +0x14 the ids of the units taken
    std::map<long, std::vector<long>*> mClassesByKind;   // +0x24 (FreeUnits deletes the vectors)
    EECPAGTask mTask;                       // +0x30
    EEAIFOPAvailablePersonnelGatherer mGatherer;   // +0x64
    EEUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEUnit*> mUnits;            // +0x74 the candidate units (referenced)
    EECPILocation mLocation;                // +0x84
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x9c
    EEPlayer* mTarget;                      // +0xa0
    EECPIntelligencePlayerFile* GetTargetFile() const { return this->mTargetFile; }
    EECPIntelligencePlayerFile* mTargetFile;   // +0xa4 (GetIntelligenceFiles: the intelligence file of mTarget)
    EECPIntelligencePlayerFile* GetPlayerFile() const { return this->mPlayerFile; }
    EECPIntelligencePlayerFile* mPlayerFile;   // +0xa8 (... of mPlayer)
    EECPCombatStrategy* GetStrategy() const { return this->mStrategy; }
    EECPCombatStrategy* mStrategy;          // +0xac (a Clone of the caller's)
};

// The unit gatherers' base (RTTI EEAIFOGatherUnits, vtable 0x43c2b8: ??_G 0x9b2cc, +0x04 _purecall).
class EEAIFOGatherUnits {
public:
    virtual ~EEAIFOGatherUnits() {}         // +0x00 (??_G 0x9b2cc, shared with the derived class's by ICF)
    virtual bool operator()(EEUnit* inUnit) = 0;   // +0x04
};

// The units of the player's defending groups that may join an attack (RTTI EECPUSGatherUnitsFromDefendingGroups >
// EEAIFOGatherUnits, vtable 0x43c2ac; a 0xc-byte local of GatherDefendingUnits).
class EECPUSGatherUnitsFromDefendingGroups : public EEAIFOGatherUnits {
public:
    // inline: GatherDefendingUnits stores the members before the vptr (an init list)
    EECPUSGatherUnitsFromDefendingGroups(EEComputerPlayer* inPlayer, bool inForAttack, bool inForDefense)
        : mPlayer(inPlayer), mForAttack(inForAttack), mForDefense(inForDefense)
    {
    }
    // +0x04 inline (COMDAT 0x9b24a right after GatherDefendingUnits)
    virtual bool operator()(EEUnit* inUnit);

    EEComputerPlayer* mPlayer;              // +0x04
    bool mForAttack;                        // +0x08 (only units whose class has mIsAttackUnit)
    bool mForDefense;                       // +0x09 (only units whose class has mIsDefenseUnit)
};

// RTTI EEAIFOUnitSelectionScreener (vtable slot +0x04 only; base of the screeners).
class EEAIFOUnitSelectionScreener {
public:
    virtual ~EEAIFOUnitSelectionScreener() {}
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 (inline 0x2de43: ESVERIFY(false), in EEAIFunctionObjects.h)
};

// RTTI EEAIFOCurrentlyBuildableUnitScreener > EEAIFOUnitSelectionScreener (vtable 0x43a690: ??_G 0x783ca, +0x04
// 0x2ae09); an 8-byte local of GatherCandidates.
class EEAIFOCurrentlyBuildableUnitScreener : public EEAIFOUnitSelectionScreener {
public:
    EEAIFOCurrentlyBuildableUnitScreener(EEComputerPlayer* inPlayer) : mPlayer(inPlayer) {}
    virtual bool Screen(EEUnitClass* inClass);   // +0x04 0x2ae09

    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x04
};

// RTTI EECPUSFavoriteUnitsScreen (vtable 0x43a4c0: ??_G 0x2a978); an 8-byte local of AllocateFavoriteUnits with four bools set.
// Its four tests (ret 4, one per unit kind of the strategy's counts) (provisional names)
class EECPUSFavoriteUnitsScreen {
public:
    EECPUSFavoriteUnitsScreen() : mAllowAir(true), mAllowSea(true), mAllowLand(true), mAllowSpace(true) {}
    virtual ~EECPUSFavoriteUnitsScreen() {}   // +0x00 (??_G 0x2a978; AllocateFavoriteUnits destroys its local inline)
    bool IsFavoriteAir(EEUnitClass* inClass);  // 0x2a9bd (the mL34 kind)
    bool IsFavoriteSea(EEUnitClass* inClass);  // 0x2aab4 (the mL38 kind)
    bool IsFavoriteLand(EEUnitClass* inClass);  // 0x2aba7 (the mL3C kind)
    bool IsFavoriteSpace(EEUnitClass* inClass);  // 0x2ad11 (the mL40 kind)

    bool mAllowAir;                         // +0x04
    bool mAllowSea;                         // +0x05
    bool mAllowLand;                        // +0x06
    bool mAllowSpace;                       // +0x07
};

class EECPUnitSelector {
public:
    EECPUnitSelector(EEComputerPlayer* inPlayer);   // 0x979ff
    virtual ~EECPUnitSelector();            // +0x00 0x97a2d (??_G 0x97a11)

    // requirement kind 3 (EECPUnitRequirement::mKind; AllocateRequirements's switch); named by its trace string
    void AllocateAlreadyDefinedUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement);   // 0x97a34
    void AllocateHeldUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement);  // 0x97c61 (kind 1)
    void AllocateFavoriteUnits(EECPUnitSelectionGroup* inGroup);  // 0x980a0 (the strategy's mB48)
    void AllocateRequirements(EECPUnitSelectionGroup* inGroup);  // 0x98731 (every requirement of the strategy by its kind)
    void AllocateCounterUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement);  // 0x987c7 (kind 0)
    void AllocateSpecialUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement);  // 0x995ea (kind 2)
    void RedistributeDomainCounts(EECPUnitSelectionGroup* inGroup);  // 0x9a1d1
    // 0x9a44f: the gathered classes of inGroup sorted into the strategy's four kinds
    void SplitByDomain(EECPUnitSelectionGroup* inGroup, std::vector<long>& outKind34, std::vector<long>& outKind38,
                 std::vector<long>& outKind3C, std::vector<long>& outKind40);
    void FreeUnits(EECPUnitSelectionGroup* inGroup);   // 0x9a5a9
    void FreeClassesByKind(std::map<long, std::vector<long>*>& ioMap);   // 0x9a5fc
    void Select(EECPUnitSelectionGroup* inGroup);   // 0x9a64a (EECPAttackGroup 0x57391/0x579f4)
    void ComputeDomainShares(EECPUnitSelectionGroup* inGroup);  // 0x9a695
    float GetDomainShare(EECPUnitSelectionGroup* inGroup, bool inSea);  // 0x9ab2f (ret 8)
    void ComputeGroupSize(EECPUnitSelectionGroup* inGroup);  // 0x9ad60
    void GatherDefendingUnits(EECPUnitSelectionGroup* inGroup);  // 0x9b126 (the defending groups' units)
    void GatherCandidates(EECPUnitSelectionGroup* inGroup);  // 0x9b2e9
    // 0x9b34e: weights inClass's three linked classes (+0x1d4: 3, +0x1d8: 2, +0x1dc: 1) found in inMap into ioCounts
    void WeighCounters(EEUnitClass* inClass, std::map<long, long>& ioCounts, std::map<long, std::vector<long>*>& inMap);
    void GetIntelligenceFiles(EECPUnitSelectionGroup* inGroup);  // 0x9b497 (the two intelligence files)
    bool IsCandidate(long inClass, std::map<long, std::vector<long>*>& inMap);  // 0x9b4fa (inClass is in any list)
    bool IsAvailable(long inClass, EEAIFOPAvailablePersonnelGatherer& inGatherer);  // 0x9b569
    void KeepBestClasses(std::map<long, std::vector<long>*>& ioMap);  // 0x9b5ca
    long GetRandomOfKind(long inKind, std::map<long, std::vector<long>*>& inMap);  // 0x9b817 (a random class of that list)
    long GetRandomOfCategory(long inCategory, std::map<long, std::vector<long>*>& inMap);  // 0x9b86b (a random class of that category)
    void FilterByStrategy(EECPUnitSelectionGroup* inGroup);  // 0x9b949

    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x04
};

#endif
