
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
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
// Sets the LAN broadcast port, marking the user's preferences changed (the stored port is without the harness slot's
// offset, which LobbyGetLanBroadcastPort adds: TEST ONLY)
void LobbySetLanBroadcastPort(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeLanBroadcastPort((unsigned short)(value - LobbyTestPortOffset()));
}

// Turns chat profanity filtering on or off, marking the user's preferences changed
void LobbySetFilterDirtyWords(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kFilterDirtyWords, value);
}

// Turns automatic away-when-idle on or off, marking the user's preferences changed
void LobbySetDoAutoAFK(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kDoAutoAFK, value);
}

// Turns cross-promotion messages on or off, marking the user's preferences changed
void LobbySetCrossPromotion(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kCrossPromotion, value);
}

// Turns lobby sound effects on or off; the global dirty flag is set to whether it now differs from the saved
// state, which can clear an unrelated pending global change
void LobbySetLobbySoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kLobbySoundEffects, value); // the global flag: whether it differs from the saved one
}

// Turns chat sound effects on or off, marking the user's preferences changed
void LobbySetChatSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kChatSoundEffects, value);
}

// Turns staging-room sound effects on or off, marking the user's preferences changed
void LobbySetStagingSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kStagingSoundEffects, value);
}

// Turns lobby music on or off; the global dirty flag is set to whether it now differs from the saved state,
// which can clear an unrelated pending global change
void LobbySetLobbyMusic(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kLobbyMusic, value); // the global flag: whether it differs from the saved one
}

// Turns confirmation dialogs on or off, marking the user's preferences changed
void LobbySetShowConfirmations(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kShowConfirmations, value);
}

// Turns automatic pinging of listed games on or off, marking the user's preferences changed
void LobbySetAutoPingGames(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kAutoPingGames, value);
}

// Shows or hides games in progress in the game list, marking the user's preferences changed
void LobbySetShowGamesInProgress(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kShowGamesInProgress, value);
}

// Sets whether the user hides their location from friends, marking the user's preferences changed
void LobbySetAnonymousToFriends(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeFlag(LobbyPersistentData::kAnonymousToFriends, value);
}

// Sets the user's connection speed, marking the user's preferences changed
void LobbySetConnectionSpeed(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL)
        data->ChangeConnectionSpeed(value);
}

// Writes _wonlobbypersistent.dat when the global settings changed or when forced
void LobbySaveGlobalPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kGlobalDirty)))
        data->SaveGlobalPrefs();
}

// Writes the current user's preferences file when they changed or when forced
void LobbySaveUserPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kUserDirty)))
        data->SaveUserPrefs();
}
}
