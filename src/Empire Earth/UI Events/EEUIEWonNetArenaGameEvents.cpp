// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaGameEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNAG). The WON game room form's (0x20) events: 0x1e9fed..0x1eafcb. Unit flags: the exe module set.
// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs); each ProcessEvent is one call, and this unit
// holds the screens' host (ee/EEWonFrontHost.inl), set at static initialisation.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h" // AuthContext (WONAuthContext.h)
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEStringIDs.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUICWonNetStatBox.h"
#include "ee/EEUIEWonNetArenaRoomEvents.h"
#include "ee/EEUIEWonNetModifyUserAccountEvents.h"
#include "ee/EEUIEWonNetServerSelectEvents.h"
#include "rust/won_lobby/wonfront.h"
#include "rust/won_lobby/wonnet.h"
static void OnGamePassword(const char* inPassword);
// gWonNet's user and server list: the Rust object's slots (R6 WON lobby and network round 35)
static WONAPI::AuthContext* WonNetUser() { return static_cast<WONAPI::AuthContext*>(ee_won_wonnet_objects(gWonNet->mRust)->user); }
static WONAPI::DirList& WonNetServers() { return *static_cast<WONAPI::DirList*>(ee_won_wonnet_objects(gWonNet->mRust)->servers); }

#include "ee/EEWonFrontHost.inl"
static const bool sWonFrontHostSet = (ee_won_front_set_host(WonFrontHost), true);

long EEUIEWNAGUpdateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgUpdateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNAGActivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgActivateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNAGBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgBack, inForm, (intptr_t)inEntity); }
long EEUIEWNAGCheckPassword::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgCheckPassword, inForm, (intptr_t)inEntity); }
long EEUIEWNAGClickGameFilter::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgClickGameFilter, inForm, (intptr_t)inEntity); }
long EEUIEWNAGCloseForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgCloseForm, inForm, (intptr_t)inEntity); }
long EEUIEWNAGDeactivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgDeactivateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNAGDoubleclickUser::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgDoubleclickUser, inForm, (intptr_t)inEntity); }
long EEUIEWNAGHostOrJoinGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgHostOrJoinGame, inForm, (intptr_t)inEntity); }
long EEUIEWNAGSendChat::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgSendChat, inForm, (intptr_t)inEntity); }
long EEUIEWNAGToggleHostScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgToggleHostScreen, inForm, (intptr_t)inEntity); }
long EEUIEWNAGToggleJoinScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgToggleJoinScreen, inForm, (intptr_t)inEntity); }
long EEUIEWNAGOpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::AgOpenScreen, inForm, (intptr_t)inEntity); }

// the password prompt's callback: joins the selected WON game with the password typed
static void OnGamePassword(const char* inPassword) { ee_won_front_call(EeWonFrontCall::AgOnGamePassword, NULL, (intptr_t)inPassword); }
