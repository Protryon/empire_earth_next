// EECampaignEditorBehavior (RECONSTRUCTION; class name from RTTI; file name inferred; provisional member and method
// names). The campaign editor's model: RTTI EECampaignEditorBehavior (vtable 0x44c058, one slot: ??_G 0x28d885),
// ctor 0x28d82d, dtor 0x28d8a1, members 0x28d8d6..0x28e86f (its own unit, not UI Events).
// The game has one, gCampaignEditor (0x562df0, init chain 0x1cfeef; a file-scope global of the original
// EEUIECampaignEditorEvents.cpp, now of UI Events/EEScreens.cpp, the campaign editor's host: ee_screens campaign_editor.rs).
// Layout from the ctor/dtor: +0x04 EECampaign (ctor 0x28c08c, dtor 0x1317a9), +0xb4/+0xc4 vector<UWideString>
// (GetScenarios/GetAvailableScenarios copy them), +0xd4 UWideString (SetPassword), +0xe8/+0xe9 bools (sizeof 0xec).
// Requires <vector>, ee/EECampaignFormManager.h (EECampaign).
#ifndef EE_EECAMPAIGNEDITORBEHAVIOR_H
#define EE_EECAMPAIGNEDITORBEHAVIOR_H

class EECampaignEditorBehavior {
public:
    EECampaignEditorBehavior();
    virtual ~EECampaignEditorBehavior();

    bool AddScenario(const UWideString& inScenario);
    bool MoveScenario(const UWideString& inScenario, long inDelta);
    long New();
    bool Delete(const UWideString& inName, UWideString& outError);
    void GetScenarios(std::vector<UWideString>& outScenarios);    // 0x28dd83 (+0xb4)
    void GetAvailableScenarios(std::vector<UWideString>& outScenarios);   // 0x28dd96 (+0xc4)
    bool GetIsUnlocked();
    const UWideString& GetPassword() const { return this->mPassword; }
    bool GetPasswordEntered() const { return this->mPasswordEntered; }
    const UWideString& GetBackgroundImage();  // 0x28de0c (campaign +0x38)
    const UWideString& GetRequiredCiv();    // 0x28de14 (campaign +0x9c)
    void GetCampaignScenarios(std::vector<UWideString>& outScenarios);    // 0x28de1c (campaign +0x00)
    UWideString GetAuthorEmail();           // 0x28de31 (campaign +0x10)
    UWideString GetAuthorName();            // 0x28de4b (campaign +0x24)
    UWideString GetDescription();           // 0x28de65 (campaign +0x4c)
    UWideString GetName();                  // 0x28de7f (campaign +0x60)
    bool GetScenarioList(std::vector<UWideString>& outScenarios, UWideString& outError);
    bool Reset();
    bool RemoveScenario(const UWideString& inScenario);
    bool Save(UWideString& outError);
    bool Load(const UWideString& inName, UWideString& outError);
    void SetAuthorEmail(const UWideString& inText);
    void SetAuthorName(const UWideString& inText);
    void SetBackgroundImage(const UWideString& inText);
    void SetDescription(const UWideString& inText);
    void SetName(const UWideString& inText);
    void SetPassword(const UWideString& inPassword);    // 0x28e842 (+0xd4; sets mPasswordEntered)
    void SetRequiredCiv(const UWideString& inText);
    // Save's checks, defined after the setters (provisional names)
    bool ValidateName(UWideString& outError);       // 0x28e86f (the campaign name is not empty, else string 0x1bd0)
    bool Validate(UWideString& outError);           // 0x28e8c7 (the three checks)
    bool ValidateOther(UWideString& outError);      // `return true`, ICF-folded into 0xb3257
    bool ValidateScenarios(UWideString& outError);  // 0x28e8fe (at least one scenario, else string 0x1bca)

    EECampaign mCampaign;                   // +0x04
    std::vector<UWideString> mScenarios;    // +0xb4
    std::vector<UWideString> mAvailableScenarios;   // +0xc4
    UWideString mPassword;                  // +0xd4
    bool mIsNew;                            // +0xe8
    bool mPasswordEntered;                  // +0xe9
};

#endif
