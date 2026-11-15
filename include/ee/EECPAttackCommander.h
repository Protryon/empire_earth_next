// C:\EEX\Empire Earth\AI Thread\EECPAttackCommander.h (RECONSTRUCTION; file name inferred). A
// computer player's attack commander (RTTI EECPAttackCommander, no base; vtable 0x43bc10: the destructor only;
// EECPAttackCommander.cpp 0x5198e..0x529e7, ctor 0x5198e called by EECPCombatManager). Only what the decompiled units
// need (provisional names).
#ifndef EE_EECPATTACKCOMMANDER_H
#define EE_EECPATTACKCOMMANDER_H

struct EEPUnitSwap;                         // (ee/EEPhysics.h)
class EECPAttackSubGroup;
class EEUnit;
class EEComplexUnit;
class EEComputerPlayer;
class EECPAttackGroup;
class EECPIntelligence;
class EECPPersonnel;
class EECPAGTask;
class EECPCombatStrategy;
class EECPCombatManager;

class EECPAttackCommander {
public:
    virtual ~EECPAttackCommander();         // +0x00 (??_G 0x519d9, dtor 0x519f5)

    // 0x52015: the sub group with id inID in any of its attack groups (the last group first), 0 if none
    EECPAttackSubGroup* GetSubGroup(unsigned long inID);
    // 0x51f7f (ret 4): the sub group of any attack group (the last first) that holds inUnit (provisional name)
    EECPAttackSubGroup* FindHolder(EEUnit* inUnit);

    // (EECPCombatManager.cpp's calls; provisional names; argument types inferred)
    EECPAttackCommander(EEComputerPlayer* inPlayer, EECPPersonnel* inPersonnel, EECPIntelligence* inIntelligence);   // 0x5198e
    bool Offer(EEComplexUnit* inOld, EEComplexUnit* inUnit);     // 0x51a45 (ret 8): any group's 0x54829 (the last first)
    void TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag); // 0x51e14 (ret 8): each group's 0x55cdd, then 0x51e61(+0x24)
    void DeleteGroups();                    // 0x51ef0
    void CollectDefendingUnits(std::vector<EEComplexUnit*>& inUnits);  // 0x52060 (ret 4): each group with mContinentID == 0: 0x56b35
    bool KeepsUnit(EEComplexUnit* inUnit);  // 0x520bd (ret 4): any group's 0x56caf (the last first)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x520ff
    void UpdateGroups();                    // 0x521d1
    void UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps);  // 0x5220a (ret 4): each group's ReplaceUnit(old, new)
    bool ReleaseFromAreaGroups(EEComplexUnit* inUnit);  // 0x52283 (ret 4; returns the first group's 0x577d4 that is true)
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x522ec
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x52337
    bool RemoveUnit(EEUnit* inUnit);        // 0x523b0 (ret 4)
    unsigned long ListUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups);  // 0x51fc0 (ret 8; returns outGroups.size())
    // (EECPDefenseCommander.cpp's calls; provisional names): a new attack group for inTask from a default
    // strategy (0x51bb9, ret 8) or from inStrategy (0x51d8e, ret 0xc; the group's unit ids into outUnitIDs)
    EECPAttackGroup* CreateGroup(EECPAGTask& inTask, long inKind);
    EECPAttackGroup* CreateGroupFromStrategy(EECPAGTask& inTask, const EECPCombatStrategy& inStrategy, std::vector<long>& outUnitIDs);
    // template COMDAT 0x7d48e in EECPDefenseCommander.cpp's range: the groups inFilter accepts (the last first,
    // through the deque iterator), then each one's sub groups
    template <class P> void CollectFiltered(std::vector<EECPAttackSubGroup*>& outGroups, P& inFilter)
    {
        for (long i = this->mGroups.size() - 1; i >= 0; i--)
            if (inFilter(*(this->mGroups.begin() + i)))
                (*(this->mGroups.begin() + i))->Collect(outGroups, inFilter);
    }
    void Update(unsigned long inTime);      // 0x523f2 (ret 4)
    // (EECPAttackCommander.cpp; provisional names)
    bool TakeUnit(EEComplexUnit* inUnit);        // 0x51a8a (ret 4; EECPAttackSubGroup 0x5d29b, 0x869f3): a group on inUnit's continent takes it
    void DropSpentGroups(unsigned long inTime);  // 0x51e61 (ret 4): deletes the finished groups; mL24 = inTime + 45000
    EECPCombatManager* GetCombatManager();  // 0x52056 (the player's combat manager; called by the groups and sub groups)
    EECPIntelligence* GetIntelligence() { return this->mIntelligence; }  // inline (EECPAttackGroup 0x456b73 needs both accessors)
    // inline (EECPCombatManager 0x4734fa/0x47355b: the bounds test, then the out-of-line deque operator[] 0x1efed0)
    EECPAttackGroup* GetGroup(unsigned long inIndex) { return inIndex < this->mGroups.size() ? this->mGroups[inIndex] : 0; }
    // template COMDATs in EECPCombatManager.cpp's range (0x767cd/0x76831: the groups with mContinentID == 0; 0x768bc: every
    // group): each group's Collect, the last group first
    template <class P> void Collect(std::vector<EECPAttackSubGroup*>& outGroups, P& inFilter)
    {
        for (long i = this->mGroups.size() - 1; i >= 0; i--) {
            EECPAttackGroup* theGroup = this->mGroups[i];
            if (theGroup && theGroup->mContinentID == 0)
                this->mGroups[i]->Collect(outGroups, inFilter);
        }
    }
    template <class P> void CollectAll(std::vector<EECPAttackSubGroup*>& outGroups, P& inFilter)
    {
        for (long i = this->mGroups.size() - 1; i >= 0; i--)
            if (this->mGroups[i])
                this->mGroups[i]->Collect(outGroups, inFilter);
    }

    std::deque<EECPAttackGroup*> mGroups;   // +0x04 (size() +0x14)
    // the ctor's three arguments (EECPDefenseCommander reads them; EECPAttackSubGroup's ctor reads +0x18)
    EEComputerPlayer* mPlayer;              // +0x18
    // inline (EECPAttackSubGroup::Restore 0x45f622 loads the commander chain into ecx before pushing GetRequest's
    // argument: the object comes from a getter) (provisional name)
    EECPPersonnel* GetPersonnel() { return this->mPersonnel; }
    EECPPersonnel* mPersonnel;              // +0x1c
    EECPIntelligence* mIntelligence;        // +0x20
    long mNextDropTime;                     // +0x24
    long mL28;                              // +0x28
    long mUpdatesThisTick;                  // +0x2c (a counter: EECPAttackSubGroupUnit 0x63d50 increments it, Virtual28 scales by it)
    long mMaxUpdatesPerTick;                // +0x30 (0x23: Update's per-call limit on mL2C)
    long mL34;                              // +0x34
    long mL38;                              // +0x38 (sizeof 0x3c)
};

#endif
