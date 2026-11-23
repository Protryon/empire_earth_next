// C:\EEX\Empire Earth\Campaign\EECampaignFormManager.cpp (file name inferred: no __FILE__). The
// campaign selection form's manager (RTTI EECampaignFormManager > UIFormManager, vtable 0x44c060): 0x28f0cb..0x28f39a.
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFormManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "rust/game/campaign.h"

// Creates the campaign selection form's manager at the current difficulty, with no campaign chosen
EECampaignFormManager::EECampaignFormManager()
    : mAllScenariosOpen(false)
{
    this->mDifficultyLevel = gDifficultyLevel;
    this->mCampaignName = "";
    this->mScenarioName = "";
    this->mFirstScenario = "";
    this->mChecksum = 0;
}

// Destroys the campaign selection form's manager
EECampaignFormManager::~EECampaignFormManager()
{
}

// Returns the campaign scenario selection form's manager, creating and attaching it on first use
EECampaignFormManager* EECampaignFormManager::Get()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager;
    if (theForm->GetFormManager() != NULL)
        theManager = static_cast<EECampaignFormManager*>(theForm->GetFormManager());
    else {
        theManager = new EECampaignFormManager;
        theForm->SetFormManager(theManager);
    }
    return theManager;
}

// Returns the expected checksum of a built-in campaign (the three AoC campaigns, the demo and the tutorial), else 0
// (ee_game campaign.rs built_in_campaign)
unsigned long EECampaignFormManager::GetBuiltInCampaign(const UWideString& inName)
{
    return ee_campaign_built_in((const char*)inName);
}

// Recomputes the checksum of the chosen campaign's file (0 when it is not found; ee_game campaign.rs)
void EECampaignFormManager::UpdateChecksum()
{
    ee_campaign_update_checksum(this);
}
