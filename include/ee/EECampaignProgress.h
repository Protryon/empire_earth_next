// EECampaignProgress (RECONSTRUCTION; file name inferred; provisional class name). A
// player's progress through a campaign: the scenarios completed so far, read from the "Users\<player>\" archive
// (the "%s.%s.%d" key: campaign, ..., checksum). Not polymorphic, sizeof 0x34 (the CSSE helper's stack local at
// 0x1d19e8). Its members live in the unit 0x28f39a..0x290514 (unnamed: init chains for "Checksum" and three other
// UWideString keys, then ctor 0x28f462, dtor 0x28f4b9, GetCompletedScenarios 0x28f4fc, 0x28f56d, Load 0x28fb05,
// 0x28ffed). Names are provisional. Requires <vector>, lle/LowLevelEngine.h.
#ifndef EE_EECAMPAIGNPROGRESS_H
#define EE_EECAMPAIGNPROGRESS_H

#include <vector>
#include "rust/game/campaign.h"

struct EeCampaignProgress;

// The progress is Rust's whole (ee_game campaign.rs, world lane round 1: the three lists and the count, Load,
// RestoreCivilization, SaveCompletedScenario; MakeKey is campaign.rs make_key); the class is its handle
class EECampaignProgress {
public:
    // 0x28f462 (three vector() ctors, clear()s, mL30 = 0)
    EECampaignProgress() : mRust(ee_campaign_progress_new()) {  }
    ~EECampaignProgress() { ee_campaign_progress_free(this->mRust); }
    // 0x28f4fc: outScenarios.clear(), resize to the completed count, then copies from the last element down
    void GetCompletedScenarios(std::vector<UWideString>& outScenarios) { ee_campaign_progress_completed(this->mRust, &outScenarios); }
    // 0x28f56d (EEWorld::StartGame: (UWideString("default"), campaign, scenario, checksum); provisional name)
    long RestoreCivilization(const UWideString& inUser, const UWideString& inCampaign, const UWideString& inScenario, unsigned long inChecksum) { return ee_campaign_progress_restore(this->mRust, (const char*)inUser, (const char*)inCampaign, (const char*)inScenario, inChecksum); }
    // 0x28fb05: loads inUser's progress in inCampaign (keseOK when found); the CSSE passes "default", the campaign
    // name and EECampaignFormManager::mChecksum (argument names are provisional)
    long Load(const UWideString& inUser, const UWideString& inCampaign, unsigned long inChecksum, bool inExtract) { return ee_campaign_progress_load(this->mRust, (const char*)inUser, (const char*)inCampaign, inChecksum, inExtract); }

    EeCampaignProgress* mRust;              // (was +0x00 mCompletedScenarios, +0x10 mCivilizationKeys,
                                            // +0x20 mCivPointsAvailable, +0x30 mNumScenariosCompleted)
private:
    EECampaignProgress(const EECampaignProgress&);
    EECampaignProgress& operator=(const EECampaignProgress&);
};

#endif
