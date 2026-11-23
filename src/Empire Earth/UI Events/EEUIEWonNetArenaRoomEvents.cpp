// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaRoomEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNAR). The WON room list form's events: 0x1eafcb..0x1eb5e2. Unit flags: the exe module set.
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

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h" // AuthContext (WONAuthContext.h)
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUIEWonNetModifyUserAccountEvents.h"
#include "ee/EEUIEWonNetArenaRoomEvents.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEWonNetUI.h"
#include "rust/won_lobby/wonfront.h"

// R6 WON round 32: the bodies are Rust (ee_won_lobby wonfront.rs; the host is EEUIEWonNetArenaGameEvents.cpp's)
long EEUIEWNARUpdateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArUpdateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNARActivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArActivateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNARDeactivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArDeactivateForm, inForm, (intptr_t)inEntity); }
long EEUIEWNARDoubleClickRoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArDoubleClickRoom, inForm, (intptr_t)inEntity); }
long EEUIEWNARClickUpdateAccount::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArClickUpdateAccount, inForm, (intptr_t)inEntity); }
long EEUIEWNARJoinRoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArJoinRoom, inForm, (intptr_t)inEntity); }
long EEUIEWNAROpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_won_front_call(EeWonFrontCall::ArOpenScreen, inForm, (intptr_t)inEntity); }

long EEUIEWNARBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return inForm->GetManager()->Pop();
}

// The UI that the WON network code (EEWonNet.cpp) calls, set before main (ee/EEWonNetUI.h)
static void WonNetDisplayMessage(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                 EEUIFErrorCallback inCallback, bool inFlag)
{
    gUIManager->DisplayMessage(inMessage, inAlignment, inForm, inCallback, inFlag);
}

static void WonNetDisplayCancel(const UWideString& inMessage, unsigned long inAlignment, UIForm* inForm,
                                EEUIFErrorCallback inCallback, UIForm* inCancel)
{
    gUIManager->DisplayCancel(inMessage, inAlignment, inForm, inCallback, inCancel);
}

static const bool sWonNetUIRegistered = (gEEWonNetUI = EEWonNetUI{
    [] { return gGamePreferences.Save(); },
    WonNetDisplayMessage,
    WonNetDisplayCancel,
    []() -> UIEvent* { return new EEUIEWNAROpenScreen; },
    []() -> UIEvent* { return new EEUIEWNAGOpenScreen; },
}, true);
