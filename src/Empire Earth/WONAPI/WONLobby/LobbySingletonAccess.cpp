
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
namespace WONAPI {
// FUNCTION: WON 0x0075b1f0
// Returns the account servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetAccountServers() { return gLobbyData.get()!=NULL?gLobbyData->mAccountServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b250
// Returns the authentication servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetAuthServers() { return gLobbyData.get()!=NULL?gLobbyData->mAuthServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b2b0
// Returns the directory servers the lobby uses, or NULL without a lobby
ServerContext* LobbyGetDirServers() { return gLobbyData.get()!=NULL?gLobbyData->mDirServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b310
// Returns the directory servers listing the lobby rooms, or NULL without a lobby
ServerContext* LobbyGetRoomDirServers() { return gLobbyData.get()!=NULL?gLobbyData->mRoomDirServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b370
// Returns the servers used for the firewall test, or NULL without a lobby
ServerContext* LobbyGetFirewallDetectServers() { return gLobbyData.get()!=NULL?gLobbyData->mFirewallDetectServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b3d0
// Returns the servers that offer game patches, or NULL without a lobby
ServerContext* LobbyGetPatchServers() { return gLobbyData.get()!=NULL?gLobbyData->mPatchServers:SmartPtr<ServerContext>(); }
// FUNCTION: WON 0x0075b430
// Makes the room listing use the main directory servers again
void LobbyResetRoomDirServers() { if(gLobbyData.get()!=NULL) gLobbyData->mRoomDirServers=gLobbyData->mDirServers; }
// FUNCTION: WON 0x0075b500
// Records the result of the game version check
void LobbySetVersionStatus(WONStatus status) { if(gLobbyData.get()!=NULL) gLobbyData->mVersionStatus=status; }
// FUNCTION: WON 0x0075b520
// Returns the result of the game version check; status 100004 without a lobby
WONStatus LobbyGetVersionStatus() { return gLobbyData.get()!=NULL?gLobbyData->mVersionStatus:100004; }
// FUNCTION: WON 0x0075b5d0
// Records the times of the system and game terms of use
void LobbySetTOUTimes(long systemTime,long gameTime)
{
    if(gLobbyData.get()!=NULL) {
        gLobbyData->mSysTOUTime=systemTime;
        gLobbyData->mGameTOUTime=gameTime;
    }
}
// FUNCTION: WON 0x0075b670
// Returns whether the recorded system or game terms of use are newer than the given times
bool LobbyIsTOUNewer(long systemTime,long gameTime)
{
    if(gLobbyData.get()!=NULL)
        return gLobbyData->mSysTOUTime>systemTime || gLobbyData->mGameTOUTime>gameTime;
    return false;
}
// FUNCTION: WON 0x00761400
// Returns the lobby's configured network port, or 0 without a lobby
unsigned short LobbyGetLobbyPort() { return gLobbyData.get()!=NULL?gLobbyData->mLobbyPort:0; }
}
