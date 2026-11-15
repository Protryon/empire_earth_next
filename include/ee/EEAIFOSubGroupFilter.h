// C:\EEX\Empire Earth\AI Thread\EEAIFOSubGroupFilter.h (RECONSTRUCTION; file name inferred; provisional names). A
// non-polymorphic predicate over attack groups and sub groups that EECPDefenseCommander
// builds on the stack (0x799fa, 0x7b2d3) and passes to EECPAttackCommander::CollectFiltered: its ctor (0x2a60a,
// ret 0xc), operator()(group) (0x2a65d: mContinentID == 0), operator()(sub group) (0x2a66d) and SetPackages (0x2a752: the
// packages' vector rebuilt from a map, their sub group ids into mSubGroupIDs) sit before EEComputerPlayer.cpp and are
// not decompiled yet; its dtor (0x79bb5) is an inline COMDAT after its first user. Requires <set>, <vector>, <map>.
#ifndef EE_EEAIFOSUBGROUPFILTER_H
#define EE_EEAIFOSUBGROUPFILTER_H

#include <set>
#include <vector>
#include <map>

class EECPAttackGroup;
class EECPAttackSubGroup;
class EECPMilitaryAidPackage;

struct EEAIFOSubGroupFilter {
    EEAIFOSubGroupFilter(unsigned short inContinent, bool inAllowAdjacent, bool inAllowPackaged);  // 0x2a60a
    bool operator()(EECPAttackGroup* inGroup);   // 0x2a65d (ret 4)
    bool operator()(EECPAttackSubGroup* inSubGroup);   // 0x2a66d (ret 4)
    void SetPackages(std::map<long, EECPMilitaryAidPackage*>& inPackages);   // 0x2a752 (ret 4)

    std::set<unsigned long> mSubGroupIDs;   // +0x00 (_Init 0x2921cd)
    std::vector<EECPMilitaryAidPackage*> mPackages;   // +0x0c
    std::set<long> mKinds;                  // +0x1c the accepted sub group kinds (EECPAttackSubGroup::mKind)
    unsigned short mContinent;              // +0x28
    bool mAllowAdjacent;                    // +0x2a
    bool mAllowPackaged;                    // +0x2b
};

#endif
