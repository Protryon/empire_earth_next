// Free lobby functions that forward to the lobby's logic objects (dialogs, pings, routing, CD key, terms of use)
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
#include "rust/won_lobby/lobbyconf.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONCRC16.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "rust/won_lobby/lobbyprv.h"
namespace WONAPI {
// (SDK GetDefaultTimeout)
// Returns the default timeout for lobby network operations: 60 seconds
int LobbyGetDefaultTimeout() { return 60000; }
// Shows an OK/Cancel dialog and returns the button pressed; 100002 (cancel) without a lobby
int LobbyDoYesNoDialog(const GUIString& title,const GUIString& text,bool sound)
{
    return gLobbyData.get()!=NULL?(int)gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::ShowOkCancel, (intptr_t)&title, (intptr_t)&text, sound):100002;
}
// Runs the network operation behind a status dialog with the given title and text
void LobbyRunOpWithStatus(AsyncOp* op,const GUIString& title,const GUIString& text,int mode,unsigned long timeout)
{
    if(gLobbyData.get()!=NULL) {
        gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::RunOpWithStatus, (intptr_t)op, (intptr_t)&title, (intptr_t)&text, mode, (intptr_t)timeout);
    }
}
// DialogLogic::ShowStatus (the status box with a text)
static void LobbyShowStatus(const GUIString& title,const GUIString& text,bool cancel) { gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::ShowStatus, (intptr_t)&title, (intptr_t)&text, cancel); }
// Shows an error dialog describing the given WON status
void LobbyDoErrorDialog(const GUIString& title,WONStatus status)
{
    if(gLobbyData.get()!=NULL) LobbyShowStatus(title, GetWONStatusString(status), false);
}
// Plays the lobby sound unless the lobby is missing or suspended
void LobbyPlaySound(Sound* sound)
{
    if(gLobbyData.get()!=NULL && !LobbyIsSuspended() && sound!=NULL) sound->Play();
}
// Pings the game to measure its latency; inForce sends the ping now even when the ping queue is full
void LobbyPingGame(LobbyGame* game,bool force)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mPingLogic->Call(EeWonLogicCall::PingGame,(intptr_t)game,false,force);
}
// Asks the game's host for the game's details
void LobbyQueryGameDetails(LobbyGame* game)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mPingLogic->Call(EeWonLogicCall::PingGame,(intptr_t)game,true,true);
}
// Cancels every queued and running game ping, marking those games ping unknown
void LobbyKillPings(int unused)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mPingLogic->Call(EeWonLogicCall::PingKill);
}
// Returns the lobby's chat command parser, or NULL without a lobby
ChatCommandParser* LobbyGetChatCommandParser()
{
    return gLobbyData.get()!=NULL?gLobbyData->mChatCommandParser.get():NULL;
}
// Returns the lobby's UDP manager (LAN and ping traffic), or NULL without a lobby
UDPManager* LobbyGetUDPManager()
{
    return gLobbyData.get()!=NULL?gLobbyData->mUDPManager.get():NULL;
}
// Passes out the times of the system and game terms of use; leaves them unchanged without a lobby
void LobbyGetTOUTimes(long& systemTime,long& gameTime) { ee_won_lobbyprv_tou_times(&systemTime, &gameTime); }
// Stops the firewall test the lobby runs at login
void LobbyKillFirewallTest()
{
    if(gLobbyData.get()!=NULL) gLobbyData->mInitLogic->Call(EeWonLogicCall::InitKillFirewall);
}
// Returns the list of lobby servers, or NULL without a lobby
LobbyServerList* LobbyGetServerList()
{
    return gLobbyData.get()!=NULL?gLobbyData->mRoutingLogic->Slot<LobbyServerList>(kRoutingServerList):NULL;
}
// Returns the chat room the player is in, or NULL
LobbyGroup* LobbyGetChatGroup()
{
    return gLobbyData.get()!=NULL?reinterpret_cast<LobbyGroup*>(gLobbyData->mRoutingLogic->Call(EeWonRouteLogicCall::Field, 1)):NULL;
}
}

namespace WONAPI {
// Returns the lobby server the player is on, or NULL
LobbyServer* LobbyGetCurrentServer()
{
    return gLobbyData.get()!=NULL?reinterpret_cast<LobbyServer*>(gLobbyData->mRoutingLogic->Call(EeWonRouteLogicCall::Field, 0)):NULL;
}
}

namespace WONAPI {
// the SDK's GetEmptyStr (StringUtil.cpp): a shared empty string
static const std::string& GetEmptyStr() { static const std::string anEmpty; return anEmpty; }
// Returns the product id the lobby stamps on its game listings, or 0 without a lobby
unsigned short GetLobbyProductId() { return (unsigned short)ee_won_lobbyprv_get(EeWonLobbyPrvField::LanProductId); }
// (SDK LobbyMisc::DoCDKeyCheck)
// Returns whether the lobby configuration asks for a CD key check
bool LobbyDoCDKeyCheck() { return gLobbyData.get()!=NULL && ee_won_lobbyconf_num(ee_won_lobbyconf_current(), EeWonLobbyConfNum::CdKeyCheck); }
// (SDK LobbyMisc::IsCDKeyValid)
// Returns whether the player's CD key is valid
bool LobbyIsCDKeyValid() { return gLobbyData.get()!=NULL?gLobbyData->mCDKey.IsValid():false; }
// Returns the path of the system terms-of-use file, or an empty string without a lobby (SDK LobbyMisc::GetSysTOUPath)
const std::string& LobbyGetSysTOUPath() { return gLobbyData.get()!=NULL?gLobbyData->mSysTOUPath:GetEmptyStr(); }
// Returns the path of the game terms-of-use file, or an empty string without a lobby (SDK LobbyMisc::GetGameTOUPath)
const std::string& LobbyGetGameTOUPath() { return gLobbyData.get()!=NULL?gLobbyData->mGameTOUPath:GetEmptyStr(); }
// (SDK LobbyMisc::GetNumPingsToDo)
// Returns the number of game pings still to send
int LobbyGetNumPingsToDo() { return gLobbyData.get()!=NULL?(int)gLobbyData->mPingLogic->Call(EeWonLogicCall::PingNumToDo):0; }
// (SDK LobbyDialog::UserCanceledLastStatusOp)
// Returns whether the player cancelled the last operation run behind a status dialog
bool LobbyUserCanceledLastStatusOp() { return gLobbyData.get()!=NULL?gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::Field, 0)!=0:false; }
}

namespace WONAPI {
// (SDK LobbyDialog::DoDialog)
// Shows the dialog modally and returns its result; -1 without a lobby
int LobbyDoDialog(Component* inDialog)
{
    return gLobbyData.get()!=NULL ? (int)gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::DoDialog, (intptr_t)inDialog) : -1;
}
// (SDK LobbyDialog::DoErrorDialog)
// Shows an error dialog with the given title and text
void LobbyMessageBox(const GUIString& inTitle,const GUIString& inText)
{
    if(gLobbyData.get()!=NULL) gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::ShowError, (intptr_t)&inTitle, (intptr_t)&inText);
}
// (SDK LobbyDialog::DoYesNoDialog)
// Shows a Yes/No dialog and returns the button pressed; 100002 (cancel) without a lobby
int LobbyMessageBoxYesNo(const GUIString& inTitle,const GUIString& inText,bool inWarningSound)
{
    return gLobbyData.get()!=NULL ? (int)gLobbyData->mDialogLogic->Call(EeWonDlgLogicCall::ShowYesNo, (intptr_t)&inTitle, (intptr_t)&inText, inWarningSound) : 100002;
}
// (SDK LobbyMisc::RunOp) It calls AsyncOp::RunAsync (0x69f6d0), not the timer variant RunAsTimer (0x69f610).
// Starts the network operation asynchronously, with the default timeout when none is given
void LobbyRunOp(AsyncOp* inOp,unsigned long inTimeout)
{
    if(inTimeout==0) inTimeout=LobbyGetDefaultTimeout();
    inOp->RunAsync(static_cast<long>(inTimeout));
}
}
