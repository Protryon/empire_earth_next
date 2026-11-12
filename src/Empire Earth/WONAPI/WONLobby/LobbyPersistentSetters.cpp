
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
// FUNCTION: WON 0x0075e3f0
// Sets the LAN broadcast port, marking the user's preferences changed
void LobbySetLanBroadcastPort(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetLanBroadcastPort()) {
        data->SetLanBroadcastPort(value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e410
// Turns chat profanity filtering on or off, marking the user's preferences changed
void LobbySetFilterDirtyWords(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kFilterDirtyWords)) {
        data->SetFlag(LobbyPersistentData::kFilterDirtyWords, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e430
// Turns automatic away-when-idle on or off, marking the user's preferences changed
void LobbySetDoAutoAFK(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kDoAutoAFK)) {
        data->SetFlag(LobbyPersistentData::kDoAutoAFK, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e450
// Turns cross-promotion messages on or off, marking the user's preferences changed
void LobbySetCrossPromotion(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kCrossPromotion)) {
        data->SetFlag(LobbyPersistentData::kCrossPromotion, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e4e0
// Turns lobby sound effects on or off; the global dirty flag is set to whether it now differs from the saved
// state, which can clear an unrelated pending global change
void LobbySetLobbySoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL || value==data->GetFlag(LobbyPersistentData::kLobbySoundEffects))
        return;
    data->SetFlag(LobbyPersistentData::kLobbySoundEffects, value);
    data->SetFlag(LobbyPersistentData::kUserDirty, true);
    data->SetFlag(LobbyPersistentData::kGlobalDirty, value!=data->GetFlag(LobbyPersistentData::kSavedLobbySoundEffects));
}

// FUNCTION: WON 0x0075e510
// Turns chat sound effects on or off, marking the user's preferences changed
void LobbySetChatSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kChatSoundEffects)) {
        data->SetFlag(LobbyPersistentData::kChatSoundEffects, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e530
// Turns staging-room sound effects on or off, marking the user's preferences changed
void LobbySetStagingSoundEffects(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kStagingSoundEffects)) {
        data->SetFlag(LobbyPersistentData::kStagingSoundEffects, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e550
// Turns lobby music on or off; the global dirty flag is set to whether it now differs from the saved state,
// which can clear an unrelated pending global change
void LobbySetLobbyMusic(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data==NULL || value==data->GetFlag(LobbyPersistentData::kLobbyMusic))
        return;
    data->SetFlag(LobbyPersistentData::kLobbyMusic, value);
    data->SetFlag(LobbyPersistentData::kUserDirty, true);
    data->SetFlag(LobbyPersistentData::kGlobalDirty, value!=data->GetFlag(LobbyPersistentData::kSavedLobbyMusic));
}

// FUNCTION: WON 0x0075e580
// Turns confirmation dialogs on or off, marking the user's preferences changed
void LobbySetShowConfirmations(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kShowConfirmations)) {
        data->SetFlag(LobbyPersistentData::kShowConfirmations, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e5a0
// Turns automatic pinging of listed games on or off, marking the user's preferences changed
void LobbySetAutoPingGames(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kAutoPingGames)) {
        data->SetFlag(LobbyPersistentData::kAutoPingGames, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e5c0
// Shows or hides games in progress in the game list, marking the user's preferences changed
void LobbySetShowGamesInProgress(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kShowGamesInProgress)) {
        data->SetFlag(LobbyPersistentData::kShowGamesInProgress, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e5e0
// Sets whether the user hides their location from friends, marking the user's preferences changed
void LobbySetAnonymousToFriends(bool value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetFlag(LobbyPersistentData::kAnonymousToFriends)) {
        data->SetFlag(LobbyPersistentData::kAnonymousToFriends, value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e600
// Sets the user's connection speed, marking the user's preferences changed
void LobbySetConnectionSpeed(unsigned short value)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && value!=data->GetConnectionSpeed()) {
        data->SetConnectionSpeed(value);
        data->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
}

// FUNCTION: WON 0x0075e660
// Writes _wonlobbypersistent.dat when the global settings changed or when forced
void LobbySaveGlobalPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kGlobalDirty)))
        data->SaveGlobalPrefs();
}

// FUNCTION: WON 0x0075e680
// Writes the current user's preferences file when they changed or when forced
void LobbySaveUserPrefs(bool force)
{
    LobbyPersistentData* data=GetLobbyPersistentData();
    if(data!=NULL && (force || data->GetFlag(LobbyPersistentData::kUserDirty)))
        data->SaveUserPrefs();
}
}
