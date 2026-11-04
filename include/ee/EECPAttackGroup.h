// C:\EEX\Empire Earth\AI Thread\EECPAttackGroup.h (RECONSTRUCTION; file name inferred). A
// computer player's attack group (RTTI EECPAttackGroup, no base; vtable 0x43bc94: ??_G, Load 0x56cc8, +0x08 0x575bb,
// Restore 0x57838, Save 0x58625; EECPAttackGroup.cpp 0x5473a..0x5ac9c, ctor 0x5473a(inCommander, ...)). Only what the
// decompiled units need; fields by offset (provisional names). Requires <vector>, ee/EECPAGTask.h, lle/FSFile.h.
#ifndef EE_EECPATTACKGROUP_H
#define EE_EECPATTACKGROUP_H

#include <deque>
#include "ee/EECPAGOrder.h"

class EECPAttackCommander;
class EECPAttackSubGroup;
class EECPASGLand;
class EECPASGTransport;
class EECPCombatStrategy;
struct EECPUnitSelectionGroup;
class EECPILocation;
class EEUnitClass;
class EEComplexUnit;
class EEUnit;

class EECPAttackGroup {
public:
    virtual ~EECPAttackGroup();             // +0x00 (??_G 0x5479a, dtor 0x547b6)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x04 0x56cc8
    virtual void Update();                  // +0x08 0x575bb
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);                // +0x0c 0x57838
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x10 0x58625

    // (EECPAttackCommander.cpp's calls; provisional names; argument types inferred)
    EECPAttackGroup(EECPAttackCommander* inCommander);   // 0x5473a
    void AssignTask(EECPAttackSubGroup* inSubGroup);    // 0x57962 (ret 4; EECPASGAir::SetState state 0) (provisional name)
    EECPASGTransport* FindLandTransport(EECPASGLand* inLand);  // 0x579f4 (ret 4; EECPASGLand::RequestTransport: a transport for inLand) (provisional name)
    bool JoinSubGroup(EEComplexUnit* inOld, EEComplexUnit* inUnit);  // 0x54829 (ret 8)
    bool PlaceInSubGroup(EEComplexUnit* inUnit);  // 0x54948 (ret 4)
    void TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag); // 0x55cdd (ret 8): drops (inFlag) or re-tasks its subgroups when inPlayer is the target
    EECPAttackSubGroup* FindHolder(EEUnit* inUnit);  // 0x56a9c (ret 4): the sub group holding inUnit, 0 if none
    // 0x56ae6 (ret 8; returns outGroups.size())
    unsigned long ListUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups);
    void CollectUnits(std::vector<EEComplexUnit*>& inUnits);  // 0x56b35 (ret 4)
    bool KeepsUnit(EEComplexUnit* inUnit);  // 0x56caf (ret 4)
    void MakePlan(EECPCombatStrategy& inStrategy, std::vector<long>& outUnitIDs);  // 0x57391 (ret 8)
    bool ReleaseUnit(EEComplexUnit* inUnit);         // 0x577d4 (ret 4)
    // 0x589c4 (ret 8: the first sub group whose slot +0x128 takes the unit change; returns al, types from that slot)
    bool ReplaceUnit(EEComplexUnit* inUnit, EEComplexUnit* inNewUnit);
    bool RemoveUnit(EEUnit* inUnit);        // 0x58a06 (ret 4; a group member: it walks mSubGroups)
    void Tick(unsigned long inTime);        // 0x58a9e (ret 4)
    // inline (COMDAT 0x51cf8 right after its first user EECPAttackCommander::CreateGroup; called out of line there):
    // done when flagged or without sub groups (provisional name)
    bool IsSpent() const { return this->mIsFinished || this->mSubGroups.size() == 0; }

    // 0x56bac: the sub group with id inID (searched from the back), 0 if none (provisional name)
    EECPAttackSubGroup* GetSubGroup(unsigned long inID);
    // mSubGroups.size() out of line (0x91f11, ICF-folded with UReferenceContainer<EEComplexUnit*>::GetSize)
    unsigned long GetNumSubGroups() const { return this->mSubGroups.size(); }
    // inline: the sub group at inIndex (EECPCombatManager::ForgetTargetUnit 0x47355b keeps the begin() + i operator+
    // out of line one level down, which a direct site cannot do) (provisional name)
    EECPAttackSubGroup* GetSubGroupAt(long inIndex) { return *(this->mSubGroups.begin() + inIndex); }

    // EECPAttackGroup.cpp's own members (provisional names; argument types inferred)
    void AssignAirTask(EECPAttackSubGroup* inSubGroup);  // 0x54f62 (ret 4: an air sub group's task)
    void AssignLandTask(EECPAttackSubGroup* inSubGroup);  // 0x5502e (ret 4: a land sub group's task)
    void AssignSeaTask(EECPAttackSubGroup* inSubGroup);  // 0x551d6 (ret 4: a sea sub group's task, continent kind 2)
    void AssignSpaceTask(EECPAttackSubGroup* inSubGroup);  // 0x55754 (ret 4: a space sub group's task, continent kind 4)
    bool MergeSubGroups(EECPAttackSubGroup* inInto, EECPAttackSubGroup* inFrom);  // 0x55d55 (ret 8: inFrom's units into inInto)
    // 0x55dbd (ret 8): one new sub group per kind of inGroup's units and classes, into outSubGroups
    void MakeSubGroups(EECPUnitSelectionGroup& inGroup, std::vector<EECPAttackSubGroup*>& outSubGroups);
    void PlaceSubGroups(EECPUnitSelectionGroup& inGroup);  // 0x56416 (ret 4)
    void RemoveSubGroup(EECPAttackSubGroup* inSubGroup);  // 0x569f8 (ret 4: removes and releases inSubGroup)
    void GetTargetLocations(std::vector<EECPILocation>& outTargets);  // 0x56b73 (ret 4: the target player's targets)
    long GetSubGroupKind(EEUnitClass* inClass);  // 0x56bee (ret 4: the sub group kind of a unit class, 100 if none)
    void DropOrdersOf(EECPAttackSubGroup* inSubGroup);  // 0x56f9e (ret 4: clears the orders naming inSubGroup)
    bool FindAirTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo);  // 0x57017 (ret 0xc)
    bool FindLandTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo);  // 0x571e4 (ret 0xc)
    bool FindSeaTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo);  // 0x5737e (ret 0xc)
    bool FindSpaceTarget(EECPAttackSubGroup* inSubGroup, EECPILocation& inFrom, EECPILocation& outTo);  // 0x5738c (ret 0xc)
    bool ExecuteOrder(EECPAGOrder& inOrder, unsigned long inTime);  // 0x57727 (ret 8: carries out one order)
    void CheckUnit(EECPAGOrder& inOrder);   // 0x577e0 (ret 4)
    // the target filter after GetTargetLocations's GetTargets: empty (ICF-folded into 0x157269, `ret 4`) (provisional
    // name)
    void FilterTargets(std::vector<EECPILocation>& ioTargets);
    void SplitSubGroup(EECPAttackSubGroup* inSubGroup);  // 0x58741 (ret 4: splits inSubGroup's units into a new sub group)
    void GiveImprovements(EECPUnitSelectionGroup& inGroup);  // 0x58b2e (ret 4; not written yet)
    // template COMDATs in EECPCombatManager.cpp's range (0x76b19, 0x76b9f, 0x76c5c): the sub groups inFilter accepts,
    // the last first, appended to outGroups (provisional name)
    template <class P> void Collect(std::vector<EECPAttackSubGroup*>& outGroups, P& inFilter)
    {
        for (long i = this->mSubGroups.size() - 1; i >= 0; i--)
            if (inFilter(this->mSubGroups[i]))
                outGroups.push_back(this->mSubGroups[i]);
    }

    std::vector<EECPAttackSubGroup*> mSubGroups;   // +0x04
    // +0x14 (was a 0x14-byte gap): 0x4c22c is deque<EECPAGOrder>::push_back (0x3c-byte elements, 0x44 per block);
    // EECPASGLand::TakeRequestedUnits 0x47145 pushes an order (2, 7, SetSubGroup(this), mB38) (provisional name)
    std::deque<EECPAGOrder> mOrders;
    EECPAGTask mTask;                       // +0x28
    EECPAttackCommander* mCommander;        // +0x5c (the ctor's first argument)
    // EECPUSGatherUnitsFromDefendingGroups 0x9b24a tests +0x68 (a word) of a sub group's mGroup (provisional name)
    unsigned long mID;                      // +0x60 (the combat manager's counter mU58; Load reads it unsigned)
    long mL64;                              // +0x64
    unsigned short mContinentID;            // +0x68 (compared as a word with EEAIContinent::mID, 0x51af6)
    bool mIsFinished;                       // +0x6a (IsSpent; sizeof 0x6c: new 0x6c)
};

#endif
