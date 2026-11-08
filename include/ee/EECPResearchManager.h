// C:\EEX\Empire Earth\AI Thread\EECPResearchManager.h (RECONSTRUCTION; file name inferred).
// A computer player's research manager (RTTI EECPResearchManager, no base; vtable 0x43c27c: the destructor only;
// sizeof 0x12c: `new` 0x12c in EECPCivilizationManager's ctor 0x6e917, stored at +0x40). EECPResearchManager.cpp
// 0x89fc4..0x8bbbf (the class-static key "EECPResearchManager-Data" first). Member and function names are provisional.
// Requires lle/LowLevelEngine.h, lle/FSFile.h, <vector>.
#ifndef EE_EECPRESEARCHMANAGER_H
#define EE_EECPRESEARCHMANAGER_H

#include <vector>

class EECPCivilizationManager;
class EEComputerPlayer;
class EETechTree;
class EETechTreeNode;

// One entry of the research history (8 bytes: 0x8b467 inserts {counter, id} at the front, at most 5 kept)
struct EECPRMHistoryEntry {
    EECPRMHistoryEntry(long inCounter, long inID) : mCounter(inCounter), mID(inID) {}
    long mCounter;                          // +0x00 a running count (the unit's file-scope counter)
    long mID;                               // +0x04 the tech id
};

class EECPResearchManager {
public:
    EECPResearchManager(EECPCivilizationManager* inManager);   // 0x8a007
    virtual ~EECPResearchManager();                             // 0x8a0b8 (??_G 0x8a09c)

    void ChooseResearch();                  // 0x8a0f0: a candidate whose event gives a 0x20/0x1e0 effect, else a shuffled one the unit manager has units for
    void BuildCandidates();                 // 0x8a447: the ids of epochs <= mEpoch not yet researched
    bool CanAfford(EETechTreeNode* inNode, long& outTotal);  // 0x8a4bf
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x8a5c5
    bool HasLaterEpochIDs();                // 0x8a73f
    void ResearchUnitClass(long inUnitClass);   // 0x8a77a
    void Research(long inID);               // 0x8a79f
    void ResearchCheapest(unsigned long inCount);  // 0x8a91e: researches/builds the inCount cheapest affordable ids (sorted {cost, id})
    void ReplaceUnaffordable();             // 0x8ade8
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x8ae93
    bool TryResearch(long inID);            // 0x8aeaa
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x8b0df
    void SetNextRandomResearchTime(unsigned long inTime);   // 0x8b1fb (named by its ESDebugOut string)
    void EpochReached(long inEpoch);        // 0x8b24e
    bool IsWanted(long inID);               // 0x8b288
    void Update(unsigned long inTime);      // 0x8b34b (named by its ESDebugOut string)
    void AddHistory(long inID);             // 0x8b467

    static const UString sData;             // "EECPResearchManager-Data" (guarded chain 0x89fc4)

    std::vector<long> mCandidates;          // +0x004
    EEEpochArray<std::vector<long>> mEpochIDs;   // +0x014 per epoch (the original: 15)
    std::vector<EECPRMHistoryEntry> mHistory;   // +0x104
    EECPCivilizationManager* GetManager() const { return this->mManager; }
    EECPCivilizationManager* mManager;      // +0x114
    EEComputerPlayer* mPlayer;              // +0x118 (mManager->mPlayer)
    EETechTree* GetTechTree() const { return this->mTechTree; }
    EETechTree* mTechTree;                  // +0x11c (mPlayer->mTechnologies)
    unsigned long mNextTime;                // +0x120 (saved)
    long GetEpoch() const { return this->mEpoch; }
    long mEpoch;                            // +0x124 (-1; saved)
    bool mStarted;                          // +0x128
    bool mRecordsHistory;                   // +0x129
    bool mEnabled;                          // +0x12a (saved)
};

#endif
