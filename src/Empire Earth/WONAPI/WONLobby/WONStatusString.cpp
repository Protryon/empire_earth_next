// Localized text for a WONStatus (GetWONStatusString; which text is ee_won statusstr.rs).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"
namespace WONAPI {
const char* WONStatusToString(WONStatus);
}
using namespace WONAPI;

// Formats a failure message with the status's internal description filled in
static GUIString GeneralStatusError(const GUIString& text,WONStatus status)
{
    const char* description=WONStatusToString(status);
    return LobbyFormatString(text,GUIString(new GUIStringData(description)));
}
namespace WONAPI {
// Returns the player-facing message for a WON status: a specific text for account, login and server errors,
// else a generic HTTP, MOTD, routing, GameSpy or general failure text with the status description (the choice is
// ee_won's statusstr.rs, the original's switch)
GUIString GetWONStatusString(WONStatus status)
{
    EeWonStatusLobbyText aText=ee_won_status_lobby_text((int32_t)status);
    const GUIString& aString=gLobbyResources[(int)aText.res].mString;
    if(aText.general)
        return GeneralStatusError(aString,status);
    return aString;
}
}
