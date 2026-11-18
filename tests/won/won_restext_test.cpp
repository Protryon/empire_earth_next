// The lobby dialogs', popups' and controls' strings as resource rows (R6 WONLobby round 17: lobbydlg.rs, lobbypopup.rs
// and lobbyctl.rs string_row, which the game's objects send instead of the string): the original switches
// (LobbyDialogHost.cpp's AdminString and PopupString, LobbyCtrlHost.cpp's CtlString, copied unchanged but for the EE
// game's two strings, stand-ins here) against the rows, for every value from -5 to 60: the row's slot must be the
// string the switch returned, and a value with no row must be one the switch answered with an empty string (or, for
// the dialogs, the EE game's two, which stay strings).   exit code: the number of mismatches
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceGlobals.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_lobby/lobbyctl.h"

using namespace WONAPI;

LobbyResourceSlot gLobbyResources[EE_WON_LOBBYRES_COUNT];
wchar_t gEmptyWideString[2];
static GUIString sEeAbort, sEeLeave;

namespace Orig {
static const GUIString& AdminString(intptr_t inString)
{
    static GUIString sEmpty;
    switch ((EeWonLobbyDlgString)inString) {
    case EeWonLobbyDlgString::Warn: return LOBBY_STRING(AdminActionDialog_Warn_String);
    case EeWonLobbyDlgString::Mute: return LOBBY_STRING(AdminActionDialog_Mute_String);
    case EeWonLobbyDlgString::UnMute: return LOBBY_STRING(AdminActionDialog_UnMute_String);
    case EeWonLobbyDlgString::Kick: return LOBBY_STRING(AdminActionDialog_Kick_String);
    case EeWonLobbyDlgString::Ban: return LOBBY_STRING(AdminActionDialog_Ban_String);
    case EeWonLobbyDlgString::ExplanationOpt: return LOBBY_STRING(AdminActionDialog_ExplanationOpt_String);
    case EeWonLobbyDlgString::ExplanationReq: return LOBBY_STRING(AdminActionDialog_ExplanationReq_String);
    case EeWonLobbyDlgString::Ok: return LOBBY_STRING(LobbyGlobal_OK_String);
    case EeWonLobbyDlgString::Cancel: return LOBBY_STRING(LobbyGlobal_Cancel_String);
    case EeWonLobbyDlgString::AbortLaunch: return sEeAbort;
    case EeWonLobbyDlgString::LeaveGame: return sEeLeave;
    default: return sEmpty;
    }
}
static const GUIString& PopupString(intptr_t inString)
{
    typedef EeWonLobbyPopupString S;
    switch ((S)inString) {
    case S::Whisper: return LOBBY_STRING(ClientOptionsCtrl_Whisper_String);
    case S::Invite: return LOBBY_STRING(ClientOptionsCtrl_Invite_String);
    case S::Uninvite: return LOBBY_STRING(ClientOptionsCtrl_Uninvite_String);
    case S::Locate: return LOBBY_STRING(FriendOptionsCtrl_Locate_String);
    case S::AddToFriends: return LOBBY_STRING(ClientOptionsCtrl_AddToFriends_String);
    case S::RemoveFromFriends: return LOBBY_STRING(ClientOptionsCtrl_RemoveFromFriends_String);
    case S::ClearAfk: return LOBBY_STRING(ClientOptionsCtrl_ClearAFK_String);
    case S::SetAfk: return LOBBY_STRING(ClientOptionsCtrl_SetAFK_String);
    case S::Ignore: return LOBBY_STRING(ClientOptionsCtrl_Ignore_String);
    case S::Unignore: return LOBBY_STRING(ClientOptionsCtrl_Unignore_String);
    case S::Block: return LOBBY_STRING(ClientOptionsCtrl_Block_String);
    case S::Unblock: return LOBBY_STRING(ClientOptionsCtrl_Unblock_String);
    case S::Warn: return LOBBY_STRING(ClientOptionsCtrl_Warn_String);
    case S::UnMute: return LOBBY_STRING(ClientOptionsCtrl_UnMute_String);
    case S::Mute: return LOBBY_STRING(ClientOptionsCtrl_Mute_String);
    case S::Kick: return LOBBY_STRING(ClientOptionsCtrl_Kick_String);
    case S::Ban: return LOBBY_STRING(ClientOptionsCtrl_Ban_String);
    case S::GameJoin: return LOBBY_STRING(GameOptionsPopup_Join_String);
    case S::GamePing: return LOBBY_STRING(GameOptionsPopup_Ping_String);
    case S::GameQueryDetails: return LOBBY_STRING(GameOptionsPopup_QueryDetails_String);
    case S::GameHideDetails: return LOBBY_STRING(GameOptionsPopup_HideDetails_String);
    case S::GameShowDetails: return LOBBY_STRING(GameOptionsPopup_ShowDetails_String);
    case S::GameRemove: return LOBBY_STRING(GameOptionsPopup_Remove_String);
    case S::FriendRefreshList: return LOBBY_STRING(FriendOptionsCtrl_RefreshList_String);
    case S::FriendRemove: return LOBBY_STRING(FriendOptionsCtrl_RemoveFromFriends_String);
    }
    return gEmptyGUIString;
}
static const GUIString& CtlString(intptr_t inString)
{
    typedef EeWonLobbyCtlString S;
    switch ((S)inString) {
    case S::Ready: return LOBBY_STRING(GameStagingCtrl_Ready_String);
    case S::NotReady: return LOBBY_STRING(GameStagingCtrl_NotReady_String);
    case S::StartGame: return LOBBY_STRING(GameStagingCtrl_StartGame_String);
    case S::LeaveGame: return LOBBY_STRING(GameStagingCtrl_LeaveGame_String);
    case S::DissolveGame: return LOBBY_STRING(GameStagingCtrl_DissolveGame_String);
    case S::Ban: return LOBBY_STRING(GameUserListCtrl_Ban_String);
    case S::Kick: return LOBBY_STRING(GameUserListCtrl_Kick_String);
    case S::CheckingAddress: return LOBBY_STRING(LobbyOptions_CheckingAddress_String);
    case S::AddressUnavailable: return LOBBY_STRING(LobbyOptions_AddressUnavailable_String);
    }
    return gEmptyGUIString;
}
}

static int sBad, sChecks;
static void Check(const char* inTable, int v, int row, const GUIString* inWant, bool inEmptyOk)
{
    sChecks++;
    bool ok = row >= 0 ? &gLobbyResources[row].mString == inWant : inEmptyOk;
    if (!ok) {
        sBad++;
        printf("FAIL %s %d: row %d\n", inTable, v, row);
    }
}

int main()
{
    for (int v = -5; v <= 60; v++) {
        const GUIString* d = &Orig::AdminString(v);
        Check("dialog", v, ee_won_lobbydlg_string_row(v), d, d == &sEeAbort ? v == 10 : d == &sEeLeave ? v == 11 : d->length() == 0 && !(d >= &gLobbyResources[0].mString && d <= &gLobbyResources[EE_WON_LOBBYRES_COUNT - 1].mString));
        const GUIString* p = &Orig::PopupString(v);
        Check("popup", v, ee_won_lobbypopup_string_row(v), p, p == &gEmptyGUIString);
        const GUIString* c = &Orig::CtlString(v);
        Check("control", v, ee_won_lobbyctl_string_row(v), c, c == &gEmptyGUIString);
    }
    printf("won_restext_test: %d checks, %d mismatches\n", sChecks, sBad);
    return sBad;
}
