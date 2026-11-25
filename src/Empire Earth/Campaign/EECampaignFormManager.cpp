// C:\EEX\Empire Earth\Campaign\EECampaignFormManager.cpp (file name inferred: no __FILE__). The
// campaign selection form's manager (RTTI EECampaignFormManager > UIFormManager, vtable 0x44c060): 0x28f0cb..0x28f39a.
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <set>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/UIFormManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EECampaignFormManager.h"

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
