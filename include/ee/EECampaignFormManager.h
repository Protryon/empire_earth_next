// EECampaignFormManager (RECONSTRUCTION; file name inferred). The campaign
// selection form's (0x21) manager (RTTI EECampaignFormManager > UIFormManager; vtable 0x44c060, ctor 0x28f0cb,
// sizeof 0x110). Get() 0x28f1ae creates it on first use. EECampaign (provisional name): the campaign it holds
// (+0x04). Names are provisional. Requires lle/UIFormManager.h.
#ifndef EE_EECAMPAIGNFORMMANAGER_H
#define EE_EECAMPAIGNFORMMANAGER_H

#include <vector>

class EESMEffect;
class FSArchive;

class EECampaign {
public:
    // EECampaign.cpp 0x28befc..0x28d82d (provisional name; the archive tag keys' chains, then the members in
    // alphabetical order); the dtor is implicit (inline COMDAT 0x1317a9). Names are provisional.
    EECampaign();                           // 0x28c08c (+0x74 = gStartupDirectory + "data\")
    // 0x28c122: copies every file of inFiles found in gDataPath to the list outFiles (refilled) names
    bool CopyScenarioFiles(std::vector<UWideString>& inFiles, std::vector<UWideString>& outFiles);
    bool CreateScenarioList(const UWideString& inFile, UWideString& outError);  // 0x28c36e
    UWideString DecryptPassword(const UWideString& inPassword);    // 0x28c4a5 (UEDecryptUString)
    UWideString EncryptPassword(const UWideString& inPassword);    // 0x28c517 (UEEncryptUString)
    UWideString GetScenarioListFile();      // 0x28c589 ("Campaigns\" + gU52A280 + "ScenarioList.txt")
    const UWideString& GetAuthorEmail();    // 0x28c5f2 (+0x10)
    const UWideString& GetAuthorName();     // 0x28c5f6 (+0x24)
    const UWideString& GetCampaignName();   // 0x28c602 (+0x60)
    const UWideString& GetPassword();       // 0x28c606 (+0x88)
    // 0x28ca06: the scenario files (and, through ListScenarioMedia, their media) of the campaign
    bool ListScenarioFiles(std::vector<UWideString>& outFiles, UWideString& outError, bool inAdd, bool inPrefixed);
    bool ListScenarioMedia(const UWideString& inFile, std::vector<UWideString>& ioFiles, UWideString& outError,
                           bool inPrefixed);    // 0x28cc2e
    bool ListEffectSound(const UWideString& inFile, EESMEffect* inEffect, std::vector<UWideString>& ioFiles,
                         UWideString& outError, bool inPrefixed);   // 0x28ceba
    bool ListEffectSounds(const UWideString& inFile, std::vector<UWideString>& ioFiles, UWideString& outError,
                          bool inPrefixed);     // 0x28d219
    bool NotArchived(UWideString inFile);   // 0x28d2e3
    bool SaveArchive(FSArchive* inArchive, UWideString& outError);  // 0x28d317
    bool Save(UWideString& outError);       // 0x28d53b
    void SetAuthorEmail(const UWideString& inText);         // 0x28d79b (+0x10)
    void SetAuthorName(const UWideString& inText);          // 0x28d7ab (+0x24)
    void SetBackgroundImage(const UWideString& inText);     // 0x28d7bb (+0x38)
    void SetCampaignDescription(const UWideString& inText); // 0x28d7cb (+0x4c)
    void SetCampaignName(const UWideString& inText);        // 0x28d7db (+0x60)
    void SetCampaignScenarios(const std::vector<UWideString>& inScenarios);    // 0x28d7eb (+0x00)
    void SetDirectory(const UWideString& inText);           // 0x28d7f7 (+0x74)
    void SetPassword(const UWideString& inText);            // 0x28d807 (+0x88)
    void SetRequiredCiv(const UWideString& inText);         // 0x28d81a (+0x9c)

    // 0x28c614: loads the campaign file; outError gets the message when it fails
    bool Load(const UWideString& inFile, UWideString& outError, bool inFlag);
    const UWideString& GetDescription();    // 0x28c5fe (+0x4c; out of line)
    const UWideString& GetRequiredCiv();    // 0x28c60d (+0x9c; out of line; provisional name)
    const UWideString& GetBackgroundImage();  // 0x28c5fa (+0x38; out of line; the CSSE's map name; provisional name)
    // 0x12c738 (ICF-folded `return this`; out of line): mScenarios (the CSSE copies it; provisional name)
    const std::vector<UWideString>& GetScenarios();

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
    EECampaignFormManager();                // 0x28f0cb
    virtual ~EECampaignFormManager();       // 0x28f16c (??_G 0x28f150)
    static EECampaignFormManager* Get();    // 0x28f1ae
    // 0x28f1f8: index of a built-in campaign by name (0: a custom one; provisional name)
    static unsigned long GetBuiltInCampaign(const UWideString& inName);
    void SetCampaignName(const UWideString& inName) { this->mCampaignName = inName; }
    void SetDescription(const UWideString& inDescription) { this->mDescription = inDescription; }
    void SetFirstScenario(const UWideString& inScenario) { this->mFirstScenario = inScenario; }
    // 0x28f285: mChecksum of the campaign file ("Campaigns\" + name + extension; 0 when it is missing); name
    // (provisional name)
    void UpdateChecksum();
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
