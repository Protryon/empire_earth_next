// C:\EEX\Empire Earth\Campaign\EECampaign.cpp (file name inferred: no __FILE__). A campaign: the
// archive "Campaigns\<name><gCampaignExtension>" with seven tags and the scenario list, plus the scenario files and
// their media: 0x28befc..0x28d82d (the eight tag keys' init chains, then the members, alphabetical). Unit flags: the
// exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/UIFormManager.h"

#include "ee/EECampaignFormManager.h"
// Creates an empty campaign in the game's data directory, with no required civilization or background image
EECampaign::EECampaign()
{
    this->mDirectory = (const char*)(gStartupDirectory + "data\\");
    this->mRequiredCiv = "";
    this->mBackgroundImage = "";
}
