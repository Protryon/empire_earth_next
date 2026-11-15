// EECampaignProgress (RECONSTRUCTION; file name inferred; provisional class name). A
// player's progress through a campaign: the scenarios completed so far, read from the "Users\<player>\" archive
// (the "%s.%s.%d" key: campaign, ..., checksum). Not polymorphic, sizeof 0x34 (the CSSE helper's stack local at
// 0x1d19e8). Its members live in the unit 0x28f39a..0x290514 (unnamed: init chains for "Checksum" and three other
// UWideString keys, then ctor 0x28f462, dtor 0x28f4b9, GetCompletedScenarios 0x28f4fc, 0x28f56d, Load 0x28fb05,
// 0x28ffed). Names are provisional. Requires <vector>, lle/LowLevelEngine.h.
#ifndef EE_EECAMPAIGNPROGRESS_H
#define EE_EECAMPAIGNPROGRESS_H

#include <vector>

class EECampaignProgress {
public:
    EECampaignProgress();                   // 0x28f462 (three vector() ctors, clear()s, mL30 = 0)
    ~EECampaignProgress();                  // 0x28f4b9
    // 0x28f4fc: outScenarios.clear(), resize to the completed count, then copies from the last element down
    void GetCompletedScenarios(std::vector<UWideString>& outScenarios);
    // 0x28f56d (EEWorld::StartGame: (UWideString("default"), campaign, scenario, checksum); provisional name)
    long RestoreCivilization(const UWideString& inUser, const UWideString& inCampaign, const UWideString& inScenario, unsigned long inChecksum);
    // 0x28f4d5: outKey = "<campaign>.<scenario>.<checksum>" (the archive tag names; provisional name)
    void MakeKey(UWideString& outKey, const UWideString& inCampaign, const UWideString& inScenario,
                 unsigned long inChecksum);
    // 0x28fb05: loads inUser's progress in inCampaign (keseOK when found); the CSSE passes "default", the campaign
    // name and EECampaignFormManager::mChecksum (argument names are provisional)
    long Load(const UWideString& inUser, const UWideString& inCampaign, unsigned long inChecksum, bool inFlag);
    // 0x28ffed (`ret 0x10`; EEPhysics::EndGame after a successful Load: user, campaign, scenario, checksum; provisional
    // name)
    long SaveCompletedScenario(const UWideString& inUser, const UWideString& inCampaign, const UWideString& inScenario,
                  unsigned long inChecksum);

    std::vector<UWideString> mCompletedScenarios;   // +0x00 (GetCompletedScenarios copies it)
    std::vector<UWideString> mCivilizationKeys;     // +0x10
    std::vector<long> mCivPointsAvailable;          // +0x20
    long mNumScenariosCompleted;                    // +0x30
};

#endif
