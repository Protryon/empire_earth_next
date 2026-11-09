// Tree area configuration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00795f10
// Handles the LISTSORTTYPE config key for a tree area; passes other keys to the list area config
bool TreeAreaConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="LISTSORTTYPE")
    {
        this->mTreeArea->SetSortType(this->ReadSortType());
        return true;
    }
    return ListAreaConfig::Virtual24(instruction);
}
}
