// C:\EEX\Empire Earth\UI Events\EEUIEMainGameOptionsEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEMGOE). The in-game options menu's events: 0x1e0e1b..0x1e1b8d. The confirmation callbacks are file statics
// defined after their users; OpenOptionsScreen (created by the main game form) creates the other events, so their
// ??_G block follows it; the shared EEAction ??_G (0x1e1b69, EEAUserPauseGame's) closes the unit. Unit flags: the
// exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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
#include "lle/UIBlinkingButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEPhysics.h"
#include "ee/EEServer.h"
#include "ee/EEAction.h"
#include "ee/EEAUserPauseGame.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIESLLoadScreen.h"
#include "ee/EEUIESLSaveScreen.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // EE_SCREENS_GS_IN_GAME_OPEN (ee_screens settings_form.rs)
#include "ee/EEUIEMainGameOptionsEvents.h"
#include "ee/EEStringIDs.h"
static void OnResign(UIForm* inForm);
static void OnRestartGame(UIForm* inForm);
static void OnReturnToEditor(UIForm* inForm);
static void OnExitGame(UIForm* inForm);

// FUNCTION: EXE 0x5e0e1b
// Closes the options form and returns to the game
long EEUIEMGOEReturnToGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x5e0e2b
// Asks whether to resign
long EEUIEMGOEResign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    gResource->GetString(IDS_DO_YOU_WISH_TO_RESIGN, theQuestion);
    theManager->DisplayQuestion(theQuestion, true, NULL, OnResign, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0e7f
// Resigns the local player (sends the resign message) and closes the form
static void OnResign(UIForm* inForm)
{
    gCommunications->ProcessLocalUserInput(new EEMResign, kSlotNone);
    gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x5e0eb5
// Creates the resign message
// (EEMResign's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x5e0ed9
// Asks whether to restart the game
long EEUIEMGOERestartGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    gResource->GetString(IDS_DO_YOU_WISH_TO_RESTART_THIS_GAME, theQuestion);
    theManager->DisplayQuestion(theQuestion, true, NULL, OnRestartGame, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0f2d
// Posts the game restart
static void OnRestartGame(UIForm* inForm)
{
    gUserInterface->PostRestartGame();
}

// FUNCTION: EXE 0x5e0f38
// Asks whether to leave the test game and return to the scenario editor
long EEUIEMGOEReturnToEditor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    gResource->GetString(IDS_EXIT_TEST_MODE_AND_RETURN_TO_EDITOR, theQuestion);
    theManager->DisplayQuestion(theQuestion, true, NULL, OnReturnToEditor, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e0f8c
// Returns to the scenario editor from a single-player test game: stops the simulation and posts the editor switch
static void OnReturnToEditor(UIForm* inForm)
{
    if (!gCommunications->IsServer() || !(gCommunications->GetNumberOfCTsProcessedInPhysics() > 0)
        || gCommunications->GetIsMultiplayerSession() != false)
        return;
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    gUserInterface->PostEnterScenarioEditor(gUIManager->mInScenarioEditor);
    gUserInterface->PostPauseInformation(true);
}

// FUNCTION: EXE 0x5e0fec
// Opens the load screen on the Saved Games folder
long EEUIEMGOELoadSavedGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theDirectory;
    theDirectory = UString("data\\") + UString("Saved Games\\");
    UIEvent* theEvent = new EEUIESLLoadScreen(gSavedGameExtension, theDirectory);
    gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e108a
// Opens the save screen on the Scenarios folder in the scenario editor, else on the Saved Games folder
long EEUIEMGOESaveGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theExtension;
    UString theDirectory;
    theExtension = gUIManager->mInScenarioEditor == true ? gScenarioExtension : gSavedGameExtension;
    theDirectory = UString("data\\") + UString(gUIManager->mInScenarioEditor == true ? "Scenarios\\" : "Saved Games\\");
    UIEvent* theEvent = new EEUIESLSaveScreen(theExtension, theDirectory);
    gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e1167
// Pauses the game (the host directly, a client by request to the host) and closes the form
long EEUIEMGOEPauseGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) {
        if (gCommunications->IsServer())
            gUserInterface->PostAction(new EEAUserPauseGame);
        else
            gCommunications->ProcessLocalUserInput(new NEMClientRequestPause, kSlotNone);
    }
    return gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x5e11ef
// Opens the chat log form (previous messages)
long EEUIEMGOEPreviousMessages::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->OpenForm(gUIManager->GetFormByID(kFormChatLog));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e120e
// Opens the scenario load form from a single-player game: posts itself once first, then swaps to the form keeping its
// music, pauses the game and remembers the main game form
long EEUIEMGOELoadScenario::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMusic;
    UIForm* theInfoForm = gUIManager->GetFormByID(kFormScenarioIntro);
    if (this->mPosted == false) {
        this->mPosted = true;
        gUserInterface->PostEvent(this, NULL);
    } else if (gCommunications->IsServer() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0
               && gCommunications->GetIsMultiplayerSession() == false && theInfoForm != NULL) {
        EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
        theMusic = gUIManager->GetFormByID(kFormScenarioLoad)->GetMusicFileName();
        gUIManager->GetFormByID(kFormScenarioLoad)->SetMusicFileName("");
        gUIManager->Pop();
        gUIManager->SwapForm(gUIManager->GetFormByID(kFormScenarioLoad));
        gUIManager->GetFormByID(kFormScenarioLoad)->SetMusicFileName(theMusic);
        dynamic_cast<EEServer*>(gCommunications)->PauseGame();
        gUserInterface->PostPauseInformation(true);
        theManager->mIsInGame = false;
        theManager->mIsLoadingFromGame = true;
        gSavedMainGameForm = gUIManager->mMainGameForm;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e136e
// Shows the scenario briefing
long EEUIEMGOEGameBriefing::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEDisplayScenarioIntro();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e1378
// Opens the game settings form, with the event that sets it up for in-game use
long EEUIEMGOEGameSettings::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_GS_IN_GAME_OPEN);
    inForm->GetManager()->GetFormByID(kFormGameSettings)->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    return gUserInterface->OpenForm(inForm->GetManager()->GetFormByID(kFormGameSettings));
}

// FUNCTION: EXE 0x5e1407
// Asks whether to quit the game
long EEUIEMGOEExitGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    gResource->GetString(IDS_DO_YOU_WISH_TO_QUIT, theQuestion);
    theManager->DisplayQuestion(theQuestion, true, NULL, OnExitGame, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e145b
// Exits the game, unless the host is migrating or this is a playback session (then says why not)
static void OnExitGame(UIForm* inForm)
{
    UWideString theMessage;
    if (!(gCommunications->IsServer() ? static_cast<NESynchronousServer*>(gCommunications)->IsMigrating()
                                                  : static_cast<NESynchronousClient*>(gCommunications)->IsPlaybackRecordingSession()))
        EEExitGame();
    else {
        gResource->GetString(IDS_YOU_CANNOT_EXIT_GAME_WHILE, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
    }
}

// FUNCTION: EXE 0x5e14cb
// Sets up the options form: pauses the game, stops the options button blinking, labels Pause/Unpause and hooks up
// every button, showing Save to the host only and restart/load buttons in single player only
long EEUIEMGOEOpenOptionsScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    gWorld.PauseGame();
    static_cast<UIBlinkingButton*>(gUIManager->mMainGameForm->GetControl(kMainGameIngameOptions))->SetIsBlinking(false);
    if (gWorld.mUserPaused == true) {
        gResource->GetString(IDS_UNPAUSE_GAME, theText);
        UIButton* theButton = static_cast<UIButton*>(inForm->GetControl(kGameOptionsPauseUnpauseButton));
        theButton->SetText(theText, theButton->GetGEText()->GetFont());
    }
    UIEvent* theEvent = new EEUIEMGOECloseOptions;
    inForm->SetFormCloseEvent(theEvent);
    theEvent->Dereference();
    // divergence: Escape closes the menu, the other half of the main game's Escape toggle (EEUIEMGCancelUpdateState).
    // The form keeps its hot keys between openings and has none of its own, so drop the previous opening's first.
    inForm->RemoveAllHotKeys();
    inForm->AddHotKey(kKeyEscape, kKeyNone, kKeyNone, kKeyNone, new EEUIEMGOEReturnToGame, NULL, NULL, NULL, 0, false);
    theEvent = new EEUIEMGOEReturnToGame;
    inForm->GetControl(kGameOptionsReturnToGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGOEResign;
    inForm->GetControl(kGameOptionsResignButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGOEPauseGame;
    inForm->GetControl(kGameOptionsPauseUnpauseButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGOEPreviousMessages;
    inForm->GetControl(kGameOptionsPrevMessagesButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGOEGameBriefing;
    inForm->GetControl(kGameOptionsGameBriefingButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEMGOEGameSettings;
    inForm->GetControl(kGameOptionsGameSettingsButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    if (gCommunications->IsServer() == true) {
        theEvent = new EEUIEMGOESaveGame;
        inForm->GetControl(kGameOptionsSaveGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        inForm->GetControl(kGameOptionsSaveGameButton)->SetDisplayMode(kDisplayModeShown);
    } else
        inForm->GetControl(kGameOptionsSaveGameButton)->SetDisplayMode(kDisplayModeHidden);
    if (gCommunications->GetIsMultiplayerSession()) {
        inForm->GetControl(kGameOptionsRestartGameButton)->SetDisplayMode(kDisplayModeHidden);
        inForm->GetControl(kGameOptionsPlaySavedGameButton)->SetDisplayMode(kDisplayModeHidden);
        inForm->GetControl(kGameOptionsPlayScenarioButton)->SetDisplayMode(kDisplayModeHidden);
    } else {
        inForm->GetControl(kGameOptionsRestartGameButton)->SetDisplayMode(kDisplayModeShown);
        inForm->GetControl(kGameOptionsPlaySavedGameButton)->SetDisplayMode(kDisplayModeShown);
        inForm->GetControl(kGameOptionsPlayScenarioButton)->SetDisplayMode(kDisplayModeShown);
        theEvent = new EEUIEMGOELoadSavedGame;
        inForm->GetControl(kGameOptionsPlaySavedGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        theEvent = new EEUIEMGOELoadScenario;
        inForm->GetControl(kGameOptionsPlayScenarioButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        theEvent = new EEUIEMGOERestartGame;
        inForm->GetControl(kGameOptionsRestartGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
    }
    if (gUIManager->GetInScenarioEditor() == true) {
        theEvent = new EEUIEMGOEReturnToEditor;
        inForm->GetControl(kGameOptionsExitEEButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        gResource->GetString(IDS_RETURN_TO_EDITOR, theText);
        UIButton* theButton = static_cast<UIButton*>(inForm->GetControl(kGameOptionsExitEEButton));
        theButton->SetText(theText, theButton->GetGEText()->GetFont());
        theButton->SetStringTableID(IDS_EXIT_TEST_MODE_AND_RETURN_TO_EDITOR);
    } else {
        theEvent = new EEUIEMGOEExitGame;
        inForm->GetControl(kGameOptionsExitEEButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        inForm->GetControl(kGameOptionsExitEEButton)->SetStringTableID(IDS_QUIT_CURRENT_GAME_AND_VIEW);
    }
    if (gWorld.GetCurrentPlayer()->GetObserverMode() == true || gWorld.GetGameOptions().GetIsScenarioGame() == true)
        inForm->GetControl(kGameOptionsResignButton)->SetDisplayMode(kDisplayModeHidden);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e193a
// Destroys the return-to-game event and frees it when asked (scalar deleting destructor)
// (the ??_G block of the events OpenOptionsScreen creates, in declaration order: ReturnToGame, Resign, RestartGame,
// ReturnToEditor, LoadSavedGame, SaveGame, PauseGame, PreviousMessages, LoadScenario, GameBriefing, CloseOptions,
// GameSettings, ExitGame)

// FUNCTION: EXE 0x5e1965
// Destroys the resign event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1990
// Destroys the restart event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e19bb
// Destroys the return-to-editor event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e19e6
// Destroys the load-saved-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1a11
// Destroys the save-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1a3c
// Destroys the pause-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1a67
// Destroys the previous-messages event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1a92
// Destroys the load-scenario event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1abd
// Destroys the game-briefing event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1ae8
// Destroys the close-options event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1b13
// Destroys the game-settings event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1b3e
// Destroys the exit-game event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e1b69
// Destroys an action and frees it when asked (scalar deleting destructor shared by simple actions)
// (EEAction's ??_G: EEAUserPauseGame's implicit one, ICF-shared by every action without destructible members)

// ICF-FOLDED: EXE 0x5e136e
// Shows the scenario briefing
// (no address of its own: the same body as EEUIEMGOEGameBriefing::ProcessEvent, which ICF kept)
long EEUIEMGDisplayScenarioIntro::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEDisplayScenarioIntro();
    return kErrorOK;
}

// ICF-FOLDED: EXE 0x67cf38
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEUIEMGOECloseOptions::ProcessEvent(class UIForm *,class UIEntity *)
{
    return 0;
}
