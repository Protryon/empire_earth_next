// C:\EEX\Empire Earth\Campaign\EECampaign.cpp (file name inferred: no __FILE__). A campaign: the
// archive "Campaigns\<name><gCampaignExtension>" with seven tags and the scenario list, plus the scenario files and
// their media: 0x28befc..0x28d82d (the eight tag keys' init chains, then the members, alphabetical). Unit flags: the
// exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSPath.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UEncryption.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMEffect.h"
#include "ee/EEPersistentTemplates.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
#include "rust/game/campaign.h"
// Creates an empty campaign in the game's data directory, with no required civilization or background image
EECampaign::EECampaign()
{
    this->mDirectory = (const char*)(gStartupDirectory + "data\\");
    this->mRequiredCiv = "";
    this->mBackgroundImage = "";
}

// Returns the author's e-mail address
const UWideString& EECampaign::GetAuthorEmail()
{
    return this->mAuthorEmail;
}

// Returns the author's name
const UWideString& EECampaign::GetAuthorName()
{
    return this->mAuthorName;
}

// Returns the background image name
const UWideString& EECampaign::GetBackgroundImage()
{
    return this->mBackgroundImage;
}

// Returns the campaign description
const UWideString& EECampaign::GetDescription()
{
    return this->mDescription;
}

// Returns the campaign name
const UWideString& EECampaign::GetCampaignName()
{
    return this->mCampaignName;
}

// Returns the campaign's scenario names (`return this`: folded into an identical function elsewhere)
const std::vector<UWideString>& EECampaign::GetScenarios()
{
    return this->mScenarios;
}

// Returns the decrypted campaign password
const UWideString& EECampaign::GetPassword()
{
    return this->mPassword;
}

// Returns the civilization the campaign requires ("" for any)
const UWideString& EECampaign::GetRequiredCiv()
{
    return this->mRequiredCiv;
}

// Reads a campaign archive: its tags, decrypted password and scenario list (optionally checking every scenario
// exists); returns whether it worked, with an error text otherwise (ee_game campaign.rs campaign_load)
bool EECampaign::Load(const UWideString& inFile, UWideString& outError, bool inCheckFiles)
{
    return ee_campaign_load(this, (const char*)inFile, &outError, inCheckFiles);
}

// Lists every scenario file of the campaign (optionally with the campaign prefix) with its media and sounds, plus
// the background image; returns false at the first scenario whose files are missing (ee_game campaign.rs)
bool EECampaign::ListScenarioFiles(std::vector<UWideString>& outFiles, UWideString& outError, bool inAdd, bool inPrefixed)
{
    return ee_campaign_list_scenario_files(this, &outFiles, &outError, inAdd, inPrefixed);
}

// Saves the campaign: its tags (password encrypted), then the archive of its files; needs a campaign name; returns
// whether it worked, with an error text otherwise (ee_game campaign.rs campaign_save)
bool EECampaign::Save(UWideString& outError)
{
    return ee_campaign_save(this, &outError);
}

// Sets the author's e-mail address
void EECampaign::SetAuthorEmail(const UWideString& inText)
{
    this->mAuthorEmail = inText;
}

// Sets the author's name
void EECampaign::SetAuthorName(const UWideString& inText)
{
    this->mAuthorName = inText;
}

// Sets the background image name
void EECampaign::SetBackgroundImage(const UWideString& inText)
{
    this->mBackgroundImage = inText;
}

// Sets the campaign description
void EECampaign::SetCampaignDescription(const UWideString& inText)
{
    this->mDescription = inText;
}

// Sets the campaign name
void EECampaign::SetCampaignName(const UWideString& inText)
{
    this->mCampaignName = inText;
}

// Sets the campaign's scenario list
void EECampaign::SetCampaignScenarios(const std::vector<UWideString>& inScenarios)
{
    this->mScenarios = inScenarios;
}

// Sets the directory the campaign is saved into
void EECampaign::SetDirectory(const UWideString& inText)
{
    this->mDirectory = inText;
}

// Sets the campaign password (plain text)
void EECampaign::SetPassword(const UWideString& inText)
{
    this->mPassword = inText;
}

// Sets the civilization the campaign requires
void EECampaign::SetRequiredCiv(const UWideString& inText)
{
    this->mRequiredCiv = inText;
}
