// EECampaignFormManager (RECONSTRUCTION; file name inferred). The campaign
// selection form's (0x21) manager (RTTI EECampaignFormManager > UIFormManager; vtable 0x44c060, ctor 0x28f0cb,
// sizeof 0x110). Get() 0x28f1ae creates it on first use. EECampaign (provisional name): the campaign it holds
// (+0x04). Names are provisional. Requires lle/UIFormManager.h.
#ifndef EE_EECAMPAIGNFORMMANAGER_H
#define EE_EECAMPAIGNFORMMANAGER_H

#include <vector>
#include "rust/game/campaign.h"
#include "lle/UIFormManager.h"


class EECampaign {
public:
    // EECampaign.cpp 0x28befc..0x28d82d (provisional name; the archive tag keys' chains, then the members in
    // alphabetical order); the dtor is implicit (inline COMDAT 0x1317a9). Names are provisional.
    EECampaign();                           // 0x28c08c (+0x74 = gStartupDirectory + "data\")
    // Returns the author's e-mail address
    // 0x28c5f2 (+0x10)
    const UWideString& GetAuthorEmail() { return this->mAuthorEmail; }
    // Returns the author's name
    // 0x28c5f6 (+0x24)
    const UWideString& GetAuthorName() { return this->mAuthorName; }
    // Returns the campaign name
    // 0x28c602 (+0x60)
    const UWideString& GetCampaignName() { return this->mCampaignName; }
    // Returns the decrypted campaign password
    // 0x28c606 (+0x88)
    const UWideString& GetPassword() { return this->mPassword; }
    // 0x28ca06: the scenario files and their media of the campaign (ee_game campaign.rs, with Save)
    // Lists every scenario file of the campaign (optionally with the campaign prefix) with its media and sounds, plus
    // the background image; returns false at the first scenario whose files are missing (ee_game campaign.rs)
    bool ListScenarioFiles(std::vector<UWideString>& outFiles, UWideString& outError, bool inAdd, bool inPrefixed) { return ee_campaign_list_scenario_files(this, &outFiles, &outError, inAdd, inPrefixed); }
    // Saves the campaign: its tags (password encrypted), then the archive of its files; needs a campaign name; returns
    // whether it worked, with an error text otherwise (ee_game campaign.rs campaign_save)
    bool Save(UWideString& outError) { return ee_campaign_save(this, &outError); }
    // Sets the author's e-mail address
    // 0x28d79b (+0x10)
    void SetAuthorEmail(const UWideString& inText) { this->mAuthorEmail = inText; }
    // Sets the author's name
    // 0x28d7ab (+0x24)
    void SetAuthorName(const UWideString& inText) { this->mAuthorName = inText; }
    // Sets the background image name
    // 0x28d7bb (+0x38)
    void SetBackgroundImage(const UWideString& inText) { this->mBackgroundImage = inText; }
    // Sets the campaign description
    // 0x28d7cb (+0x4c)
    void SetCampaignDescription(const UWideString& inText) { this->mDescription = inText; }
    // Sets the campaign name
    // 0x28d7db (+0x60)
    void SetCampaignName(const UWideString& inText) { this->mCampaignName = inText; }
    // Sets the campaign's scenario list
    // 0x28d7eb (+0x00)
    void SetCampaignScenarios(const std::vector<UWideString>& inScenarios) { this->mScenarios = inScenarios; }
    // Sets the directory the campaign is saved into
    // 0x28d7f7 (+0x74)
    void SetDirectory(const UWideString& inText) { this->mDirectory = inText; }
    // Sets the campaign password (plain text)
    // 0x28d807 (+0x88)
    void SetPassword(const UWideString& inText) { this->mPassword = inText; }
    // Sets the civilization the campaign requires
    // 0x28d81a (+0x9c)
    void SetRequiredCiv(const UWideString& inText) { this->mRequiredCiv = inText; }

    // 0x28c614: loads the campaign file; outError gets the message when it fails
    // Reads a campaign archive: its tags, decrypted password and scenario list (optionally checking every scenario
    // exists); returns whether it worked, with an error text otherwise (ee_game campaign.rs campaign_load)
    bool Load(const UWideString& inFile, UWideString& outError, bool inCheckFiles) { return ee_campaign_load(this, (const char*)inFile, &outError, inCheckFiles); }
    // Returns the campaign description
    // 0x28c5fe (+0x4c; out of line)
    const UWideString& GetDescription() { return this->mDescription; }
    // Returns the civilization the campaign requires ("" for any)
    // 0x28c60d (+0x9c; out of line; provisional name)
    const UWideString& GetRequiredCiv() { return this->mRequiredCiv; }
    // Returns the background image name
    // 0x28c5fa (+0x38; out of line; the CSSE's map name; provisional name)
    const UWideString& GetBackgroundImage() { return this->mBackgroundImage; }
    // 0x12c738 (ICF-folded `return this`; out of line): mScenarios (the CSSE copies it; provisional name)
    // Returns the campaign's scenario names (`return this`: folded into an identical function elsewhere)
    const std::vector<UWideString>& GetScenarios() { return this->mScenarios; }

    // layout: the ctor 0x28c08c builds a vector of UWideStrings and eight UWideStrings
    std::vector<UWideString> mScenarios;    // +0x00 (`_Buy` names the element type; provisional name)
    UWideString mAuthorEmail;               // +0x10
    UWideString mAuthorName;                // +0x24
    UWideString mBackgroundImage;           // +0x38
    UWideString mDescription;               // +0x4c (GetDescription)
    UWideString mCampaignName;              // +0x60
    UWideString mDirectory;                 // +0x74
    UWideString mPassword;                  // +0x88
    UWideString mRequiredCiv;               // +0x9c (GetRequiredCiv)
};

class EECampaignFormManager : public UIFormManager {
public:
    EECampaignFormManager();
    // Destroys the campaign selection form's manager
    virtual ~EECampaignFormManager() {  }
    // Returns the campaign scenario selection form's manager, creating and attaching it on first use
    static EECampaignFormManager* Get() { return static_cast<EECampaignFormManager*>(static_cast<UIFormManager*>(ee_campaign_form_manager())); }
    // 0x28f1f8: index of a built-in campaign by name (0: a custom one; provisional name)
    // Returns the expected checksum of a built-in campaign (the three AoC campaigns, the demo and the tutorial), else 0
    // (ee_game campaign.rs built_in_campaign)
    static unsigned long GetBuiltInCampaign(const UWideString& inName) { return ee_campaign_built_in((const char*)inName); }
    void SetCampaignName(const UWideString& inName) { this->mCampaignName = inName; }
    void SetDescription(const UWideString& inDescription) { this->mDescription = inDescription; }
    void SetFirstScenario(const UWideString& inScenario) { this->mFirstScenario = inScenario; }
    // 0x28f285: mChecksum of the campaign file ("Campaigns\" + name + extension; 0 when it is missing); name
    // (provisional name)
    // Recomputes the checksum of the chosen campaign's file (0 when it is not found; ee_game campaign.rs)
    void UpdateChecksum() { ee_campaign_update_checksum(this); }
    unsigned long GetChecksum() const { return this->mChecksum; }  // (the CSSE loads it before the other arguments)

    EECampaign mCampaign;                   // +0x004
    UWideString mDescription;               // +0x0b4
    UWideString mCampaignName;              // +0x0c8
    UWideString mScenarioName;              // +0x0dc
    UWideString mFirstScenario;             // +0x0f0
    long mDifficultyLevel;                  // +0x104 (the ctor's body stores; the CSSE's SelectDifficultyLevel)
    unsigned long mChecksum;                // +0x108 (= 0; UpdateChecksum; GetBuiltInCampaign's value must match)
    bool mAllScenariosOpen;                 // +0x10c (cleared when a custom campaign is picked; the learning
                                            // campaign sets it: the CSSE then lists every scenario)
};

extern UString gCampaignExtension;          // 0x530b78 (the campaign file extension; provisional name)

#endif
