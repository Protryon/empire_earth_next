// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaGameEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNAG). The WON game room form's (0x20) events: 0x1e9fed..0x1eafcb. Unit flags: the exe module set.
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
#include "lle/UIIMECandidate.h"
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
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEStringIDs.h"
static void OnGamePassword(const char* inPassword);

// FUNCTION: EXE 0x5e9fed
// Refreshes the WON game list each time the game room's poll timer fires, while a game-list server is known
long EEUIEWNAGUpdateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (!EEWonNetAvailableGameFormManager::Get()->GetPoll()->Poll())
        return kErrorOK;
    if (gWonNet->mGameListPort != 0xffff) {
        gWonNet->RefreshGameList(NULL);
        gWonNet->UpdateGameList();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea027
// Starts the game room's poll timer and, the first time, installs the update event and fetches the game list
long EEUIEWNAGActivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    TMPoll* thePoll = EEWonNetAvailableGameFormManager::Get()->GetPoll();
    if (!thePoll->GetIsRunning())
        thePoll->Start();
    if (inForm->GetFormUpdateEvent() == NULL) {
        UIEvent* theEvent = new EEUIEWNAGUpdateForm;
        inForm->SetFormUpdateEvent(theEvent);
        theEvent->Dereference();
        gWonNet->RefreshGameList(inForm->GetControl(kAvailableGamesPlayersInRoomListBox));
        gWonNet->UpdateGameList();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea0a6
// Destroys the game room's update event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIEWNAGUpdateForm)

// FUNCTION: EXE 0x5ea0d1
// Leaves the WON room and returns to the previous screen
long EEUIEWNAGBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gWonNet->LeaveRoom();
    return inForm->GetManager()->Pop();
}

// FUNCTION: EXE 0x5ea0ef
// Shows and clears the host-game password and retype boxes when the password box is ticked; hides them otherwise
long EEUIEWNAGCheckPassword::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIEditBox* thePassword = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameEditBox));
    UICheckBox* theCheckBox = static_cast<UICheckBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox));
    UIEditBox* theConfirmation = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesPWRetypeForHostedGameEditBox));
    UIControl* thePasswordLabel = inForm->GetControl(kAvailableGamesPWForHostedGameLabel);
    UIControl* theConfirmationLabel = inForm->GetControl(kAvailableGamesPWRetypeForHostedGameLabel);
    if (theCheckBox->GetIsChecked()) {
        thePassword->DeleteText();
        theConfirmation->DeleteText();
        thePasswordLabel->SetDisplayMode(kDisplayModeShown);
        theConfirmationLabel->SetDisplayMode(kDisplayModeShown);
        thePassword->SetDisplayMode(kDisplayModeShown);
        theConfirmation->SetDisplayMode(kDisplayModeShown);
        inForm->SetTabbing(true);
        inForm->SetTabStop(27);
    } else {
        thePasswordLabel->SetDisplayMode(kDisplayModeHidden);
        theConfirmationLabel->SetDisplayMode(kDisplayModeHidden);
        thePassword->SetDisplayMode(kDisplayModeHidden);
        theConfirmation->SetDisplayMode(kDisplayModeHidden);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea193
// Reapplies the game list filters after a filter toggle changes
long EEUIEWNAGClickGameFilter::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWonNet)
        gWonNet->UpdateGameList();
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea1a7
// Stops the game room's poll timer when the form closes
long EEUIEWNAGCloseForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    TMPoll* thePoll = EEWonNetAvailableGameFormManager::Get()->GetPoll();
    if (thePoll->GetIsRunning() == true)
        thePoll->Stop();
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea1be
// Stops the game room's poll timer and removes the current form's update event
long EEUIEWNAGDeactivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    TMPoll* thePoll = EEWonNetAvailableGameFormManager::Get()->GetPoll();
    if (thePoll->GetIsRunning() == true)
        thePoll->Stop();
    gUIManager->GetCurrentForm()->SetFormUpdateEvent(NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea1eb
// Starts a private message to the double-clicked user: puts /t "name" in the chat box with the caret at the end
long EEUIEWNAGDoubleclickUser::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    UWideString theText;
    UIListBox<UWideString>* theUsers = (UIListBox<UWideString>*)inForm->GetControl(kAvailableGamesPlayersInRoomListBox);
    unsigned long theSelection;
    if (theUsers->GetCurrentSelection(theSelection) == kErrorOK) {
        theName = UWideString(theUsers->GetData(theSelection));
        if (EEIsValidName(theName)) {
            UIEditBox* theChat = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesChatEditBox));
            theChat->DeleteText();
            theText = UWideString("/t \"");
            theText += theName;
            theText += UWideString("\" ");
            theChat->SetText(theText);
            inForm->SetTabbing(true);
            inForm->SetTabStop(16);
            theChat->SetCaretLocation(theText.GetLength(), 0);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea2fb
// Hosts a game (with an optional confirmed password) or joins the selected one, at most once every 5 seconds
// (an in-progress game shows a message and lifts the delay; a password game prompts for its password)
long EEUIEWNAGHostOrJoinGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static unsigned long sLastTime = 0;
    long theResult = 0;
    UWideString theMessage;
    unsigned long theSelection;
    unsigned long theTime = TMGetTime();
    if (theTime - sLastTime > 5000) {
        sLastTime = theTime;
        if (inForm->GetEntityGroup(2)->GetIsEnabled()) {
            UIEditBox* theName = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesHostGameNameEditBox));
            UIEditBox* thePassword = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameEditBox));
            UICheckBox* theCheckBox = static_cast<UICheckBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox));
            UIEditBox* theConfirmation = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesPWRetypeForHostedGameEditBox));
            if (theCheckBox->GetIsChecked() && strcmp(thePassword->GetText(0), "") != 0) {
                if (strcmp(thePassword->GetText(0), theConfirmation->GetText(0)) == 0) {
                    if (EEIsValidName(theName->GetText(0)))
                        gWonNet->HostGame(theName->GetText(0), thePassword->GetText(0));
                    else {
                        gResource->GetString(IDS_GAME_NAME_IS_INVALID, theMessage);
                        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                    }
                } else {
                    gResource->GetString(IDS_YOUR_TWO_PASSWORDS_DONT_MATCH, theMessage);
                    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                }
            } else {
                if (EEIsValidName(theName->GetText(0)))
                    gWonNet->HostGame(theName->GetText(0), NULL);
                else {
                    gResource->GetString(IDS_GAME_NAME_IS_INVALID, theMessage);
                    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                }
            }
        } else {
            EEUICWonNetGamesBox* theGames = static_cast<EEUICWonNetGamesBox*>(inForm->GetControl(kAvailableGamesAvalGamesListBox));
            if (theGames->GetCurrentSelection(theSelection) == kErrorOK) {
                if (theGames->GetIsInProgress(theSelection)) {
                    gResource->GetString(IDS_YOU_CANT_JOIN_THAT_GAME, theMessage);
                    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                    sLastTime = 0;
                } else if (theGames->GetHasPassword(theSelection)) {
                    gResource->GetString(IDS_LOBBY_PASSWORD, theMessage);
                    gUIManager->DisplayPasswordPrompt(theMessage, true, OnGamePassword, true);
                } else
                    gWonNet->JoinGame(theGames->GetGameName(theSelection), NULL);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5ea583
// Returns the name of a game in the WON games box
// (EEUICWonNetGamesBox::GetGameName, GetHasPassword, GetIsInProgress: inline, emitted after their first user)

// FUNCTION: EXE 0x5ea5e7
// Returns whether a game in the WON games box needs a password

// FUNCTION: EXE 0x5ea618
// Returns whether a game in the WON games box is already in progress

// FUNCTION: EXE 0x5ea649
// Joins the selected WON game with the password typed in the prompt
static void OnGamePassword(const char* inPassword)
{
    EEUICWonNetGamesBox* theGames = dynamic_cast<EEUICWonNetGamesBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesAvalGamesListBox));
    unsigned long theSelection;
    if (theGames->GetCurrentSelection(theSelection) == kErrorOK)
        gWonNet->JoinGame(theGames->GetGameName(theSelection), inPassword);
}

// FUNCTION: EXE 0x5ea6bd
// Sends the chat box's text to the WON room (prefixed with ": ") and clears the box
long EEUIEWNAGSendChat::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIEditBox* theEdit = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesChatEditBox));
    UWideString theText(theEdit->GetText(0));
    UWideString theLine;
    if (!gWonNet)
        return kErrorOK;
    if (theText.GetLength() > 0) {
        theLine = ": ";
        theLine += theText;
        gWonNet->SendChat(theLine);
    }
    theEdit->DeleteText();
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea743
// Switches the game room to its Host tab: host controls on, button labelled Host, password unticked, caret in the name
long EEUIEWNAGToggleHostScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    inForm->GetEntityGroup(2)->Enable();
    inForm->GetEntityGroup(1)->Disable();
    gResource->GetString(IDS_HOST, theText);
    UIButton* theButton = static_cast<UIButton*>(inForm->GetControl(kAvailableGamesJoinHostGameButton));
    theButton->SetText(theText, theButton->GetGEText()->GetFont());
    static_cast<UICheckBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox))->SetChecked(false);
    inForm->SetTabStop(24);
    UIEditBox* theName = static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesHostGameNameEditBox));
    theName->SetCaretLocation(UWideString(theName->GetText(0)).GetLength(), 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea804
// Switches the game room to its Join tab: join controls on, button labelled Join
long EEUIEWNAGToggleJoinScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    inForm->GetEntityGroup(1)->Enable();
    inForm->GetEntityGroup(2)->Disable();
    gResource->GetString(IDS_JOIN, theText);
    UIButton* theButton = static_cast<UIButton*>(inForm->GetControl(kAvailableGamesJoinHostGameButton));
    theButton->SetText(theText, theButton->GetGEText()->GetFont());
    inForm->SetTabStop(16);
    return kErrorOK;
}

// FUNCTION: EXE 0x5ea884
// Sets up the WON game room: hooks every button, list and toggle to its event, opens on the Join tab
// and shows the user, the room and the last hosted game name
long EEUIEWNAGOpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEUIEWNSetTriggerEvent(new EEUIEWNAGBack, inForm->GetControl(kAvailableGamesBackButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGToggleHostScreen, inForm->GetControl(kAvailableGamesHostGameTab), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGToggleJoinScreen, inForm->GetControl(kAvailableGamesJoinGameTab), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGHostOrJoinGame, inForm->GetControl(kAvailableGamesJoinHostGameButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGHostOrJoinGame, inForm->GetControl(kAvailableGamesHostGameNameEditBox), kControlTriggerEnterPressed);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGHostOrJoinGame, inForm->GetControl(kAvailableGamesAvalGamesListBox), kControlTriggerDoubleClicked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGDoubleclickUser, inForm->GetControl(kAvailableGamesPlayersInRoomListBox), kControlTriggerDoubleClicked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGCheckPassword, inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox), kControlTriggerChecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGCheckPassword, inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox), kControlTriggerUnchecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesInProgressToggle), kControlTriggerChecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesInProgressToggle), kControlTriggerUnchecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesPasswordToggle), kControlTriggerChecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesPasswordToggle), kControlTriggerUnchecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesPrivateToggle), kControlTriggerChecked);
    EEUIEWNSetTriggerEvent(new EEUIEWNAGClickGameFilter, inForm->GetControl(kAvailableGamesPrivateToggle), kControlTriggerUnchecked);
    UIEvent* theEvent = new EEUIEWNAGActivateForm;
    inForm->SetFormActivateEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEWNAGDeactivateForm;
    inForm->SetFormDeactivateEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEWNAGCloseForm;
    inForm->SetFormCloseEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEWNAGSendChat;
    inForm->GetControl(kAvailableGamesChatEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
    theEvent->Dereference();
    inForm->GetEntityGroup(1)->Enable();
    inForm->GetEntityGroup(2)->Disable();
    static_cast<UIRadioButton*>(inForm->GetControl(kAvailableGamesJoinGameTab))->Check();
    gResource->GetString(IDS_JOIN, theText);
    UIButton* theButton = static_cast<UIButton*>(inForm->GetControl(kAvailableGamesJoinHostGameButton));
    theButton->SetText(theText, theButton->GetGEText()->GetFont());
    inForm->SetTabbing(true);
    inForm->SetTabStop(16);
    UITextLabel* theUserLabel = static_cast<UITextLabel*>(inForm->GetControl(kAvailableGamesPlayersNameLabel));
    std::string theUser = WONAPI::WStringToString(gWonNet->mUser->GetName());
    theUserLabel->SetText(UWideString(theUser.c_str()));
    UITextLabel* theRoomLabel = static_cast<UITextLabel*>(inForm->GetControl(kAvailableGamesRoomsNameLabel));
    std::string theRoom = WONAPI::WStringToString(gWonNet->mRoomName);
    theRoomLabel->SetText(UWideString(theRoom.c_str()));
    static_cast<UIEditBox*>(inForm->GetControl(kAvailableGamesHostGameNameEditBox))->SetText(gHostedGameName);
    static_cast<UICheckBox*>(inForm->GetControl(kAvailableGamesPWForHostedGameCheckbox))->SetChecked(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5eadd6
// Installs an event as a control's trigger event and releases the caller's reference
// (EEUIEWNSetTriggerEvent, the static function of EEUIEWonNetArenaGameEvents.h)

// FUNCTION: EXE 0x5eadf2
// Destroys the game room's activate event and frees it when asked (scalar deleting destructor)
// (??_G of the events in declaration order: ActivateForm, Back, CheckPassword, ClickGameFilter, CloseForm,
// DeactivateForm, DoubleclickUser, HostOrJoinGame, SendChat, ToggleHostScreen, ToggleJoinScreen)

// FUNCTION: EXE 0x5eae1d
// Destroys the game room's back event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eae48
// Destroys the game room's password-check event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eae73
// Destroys the game room's filter event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eae9e
// Destroys the game room's close event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eaec9
// Destroys the game room's deactivate event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eaef4
// Destroys the game room's user double-click event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eaf1f
// Destroys the game room's host-or-join event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eaf4a
// Destroys the game room's send-chat event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eaf75
// Destroys the game room's Host tab event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eafa0
// Destroys the game room's Join tab event and frees it when asked (scalar deleting destructor)
