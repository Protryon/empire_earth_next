// C:\EEX\Empire Earth\Campaign\EECampaign.cpp (file name inferred: no __FILE__). A campaign: the
// archive "Campaigns\<name><gCampaignExtension>" with seven tags and the scenario list, plus the scenario files and
// their media: 0x28befc..0x28d82d (the eight tag keys' init chains, then the members, alphabetical). Unit flags: the
// exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <string.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/FSRestorable.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"

#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEScenarioManager.h"
#include "ee/EECampaignFormManager.h"
#include "rust/game/campaign.h"
// Creates an empty campaign in the game's data directory, with no required civilization or background image
EECampaign::EECampaign()
{
    this->mDirectory = (const char*)(gStartupDirectory + "data\\");
    this->mRequiredCiv = "";
    this->mBackgroundImage = "";
}
