// C:\EEX\Empire Earth\UI Events\EEUIEMainScreenEvents.cpp (file name inferred: no __FILE__; class prefix EEUIEMSE).
// The main screen form's events: 0x1e1e48..0x1e2144. The form events they create (EEUIEGSEOutOfGameOpen, EEUIESPEOpen,
// EEUIEMSOpenMultiplayerScreen) have their ??_G right after their creators. Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
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
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EEUIEMainGameEvents.h"
#include "ee/EEUIEGameSettingsEvents.h"
#include "ee/EEUIESinglePlayerEvents.h"
#include "ee/EEUIEMainScreenEvents.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"

// FUNCTION: EXE 0x5e1e48
// Opens the game settings form from the main screen, with the event that sets it up out of game
long EEUIEMSEGameSettings::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIEvent* theEvent = new EEUIEGSEOutOfGameOpen;
    inForm->GetManager()->GetFormByID(kFormGameSettings)->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormGameSettings));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e1ead
// Destroys the out-of-game settings-open event and frees it when asked (scalar deleting destructor)
// (EEUIEGSEOutOfGameOpen's ??_G, after its first user)

// FUNCTION: EXE 0x5e1ed8
// Posts the quit request
long EEUIEMSEExitGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->PostQuit();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e1ee8
// Opens the single player form (leaving editor mode, single-player options)
long EEUIEMSESinglePlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static_cast<EEUIManager*>(inForm->GetManager())->mInScenarioEditor = false;
    gGameState.mGameOptions.mIsMultiplayer = false;
    UIEvent* theEvent = new EEUIESPEOpen;
    inForm->GetManager()->GetFormByID(kFormSinglePlayer)->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormSinglePlayer));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e1f5f
// Destroys the single-player open event and frees it when asked (scalar deleting destructor)
// (EEUIESPEOpen's ??_G)

// FUNCTION: EXE 0x5e1f8a
// Starts multiplayer (leaving editor mode) through the external lobby
long EEUIEMSEMultiPlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    theManager->mInScenarioEditor = false;
    gGameState.mGameOptions.mIsMultiplayer = true;
    // EEIsMultiplayerGame(false) is always true, so the branch that opened the multiplayer form could never run and
    // was removed.
    EEStartExternalLobby();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e200a
// Destroys the multiplayer-screen open event and frees it when asked (scalar deleting destructor)
// (EEUIEMSOpenMultiplayerScreen's ??_G; its ProcessEvent is in EEUIEMultiplayerScreenEvents.cpp)

// FUNCTION: EXE 0x5e2035
// Sets up the main screen: shows the copyright and version, adds the Ctrl+D media dump hot keys, deletes the scenario
// editor and the WON connection
long EEUIEMSEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theVersion;
    inForm->GetControl(kMainMenuMainMenuCopyrightNoticeLabel)->SetDisplayMode(kDisplayModeShown);
    sprintf(theVersion, "%s", (const char*)gGameVersion);
    static_cast<UITextLabel*>(inForm->GetControl(kMainMenuBuildInformationTemporary))->SetText(theVersion);
    inForm->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, NULL, NULL, new EEUIEMGDumpLoadedMedia(false), NULL, 0, false);
    inForm->AddHotKey(kKeyD, kKeyControl, kKeyShift, kKeyNone, NULL, NULL, new EEUIEMGDumpLoadedMedia(true), NULL, 0, false);
    static_cast<EEUIManager*>(inForm->GetManager())->DeleteScenarioEditor();
    if (gWonNet == NULL)
        return kErrorOK;
    EEWonNet* theWonNet = gWonNet;
    gWonNet = NULL;
    delete theWonNet;
    return kErrorOK;
}
