
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
// Creates the notice that the whole game list was rebuilt, holding the list
SyncGameListEvent::SyncGameListEvent(LobbyGameList* games)
    : LobbyEvent(LobbyEvent_SyncGameList),mGames(games) {}
// Destroys the notice, releasing the game list
SyncGameListEvent::~SyncGameListEvent() {}
}
