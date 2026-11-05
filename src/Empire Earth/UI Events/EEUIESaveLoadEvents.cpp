// C:\EEX\Empire Earth\UI Events\EEUIESaveLoadEvents.cpp (file name inferred: no __FILE__; class prefixes
// EEUIESLE/SL). The save/load screens' events: 0x1e688c..0x1e71cc (the Camera events unit ends at 0x1e688c; the
// scenario intro unit starts with EEUISCIFormManager's members at 0x1e71cc). DisplayQuestion's callbacks are file
// statics defined right before their users. Every ??_G of these classes is emitted by the creating units.
// Unit flags: the exe module set.
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
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/FSRestorable.h"
#include "lle/NESynchronousServer.h"
#include "lle/UIEditBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameOptions.h"
#include "ee/EEAction.h"
#include "ee/EEASaveGame.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIESaveLoadEvents.h"
#include "ee/EEUIESLLoadScreen.h"
#include "ee/EEUIESLSaveScreen.h"
#include "ee/EEStringIDs.h"
// 0x56313c: a save was handed on (the callbacks set it; ActivateForm and the save screen clear it; SaveFile ignores
// the button while it is set)
static bool sSaved;

// FUNCTION: EXE 0x5e688c
// Clears the saved flag when the save/load form becomes active
long EEUIESLEActivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    sSaved = false;
    return kErrorOK;
}

// FUNCTION: EXE 0x5e6898
// Deletes the file selected in the delete event's list (yes answer to the delete question)
// (the "form" is the DeleteFile event itself)
static void DeleteSelectedFile(UIForm* inEvent)
{
    reinterpret_cast<EEUIESLEDeleteFile*>(inEvent)->mList->DeleteSelectedFile();
}

// FUNCTION: EXE 0x5e68a5
// Asks whether to delete the selected file, deleting it on yes
long EEUIESLEDeleteFile::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    gResource->GetString(IDS_DELETE_SELECTED_FILE, theMessage);
    gUIManager->DisplayQuestion(theMessage, true, reinterpret_cast<UIForm*>(this), DeleteSelectedFile, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e68f5
// Loads the selected saved game (or scenario) and remembers its name as the last loaded one
long EEUIESLELoadFile::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theName;
    UString theFileName;
    FSFileSpec* theFile = NULL;
    UIFileListBox* theList = static_cast<UIFileListBox*>(inForm->GetControl(kLoadGameLoadFileListBox));
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK) {
        theName = theList->GetData(theSelection);
        theFileName = theName + this->mExtension;
        if (EEOpenSaveGameFile(theFileName, this->mDirectory, theFile) == kErrorOK) {
            gUserInterface->PostLoadSavedGame(*theFile);
            if (theFile)
                delete theFile;
            if (gGameScreenActive == false && gUIManager->mInScenarioEditor == true)
                gGamePreferences.mSettings.SetLoadScenarioName(theName);
            else
                gGamePreferences.mSettings.SetLoadSavedGameName(theName);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e69ef
// Posts a scenario save action, drops our reference to it and marks the save as done
// DisplayQuestion's yes callback (and the direct path) of a scenario save: the "form" is the EEASaveGame action
static void PostSaveAction(UIForm* inAction)
{
    gUserInterface->PostAction(reinterpret_cast<EEAction*>(inAction));
    reinterpret_cast<EEAction*>(inAction)->Dereference();
    sSaved = true;
}

// FUNCTION: EXE 0x5e6a10
// Sends a save-game message to every player, waiting up to 15 s, and marks the save as done
// likewise for a saved game: the "form" is the EEMLoadOrSaveGame message
static void SendSaveMessage(UIForm* inMessage)
{
    static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(reinterpret_cast<NEMessage*>(inMessage), 15000, 12);
    sSaved = true;
}

// FUNCTION: EXE 0x5e6a2f
// Drops a reference to the pending save object (no answer to the overwrite question)
static void DereferenceObject(UIForm* inObject)
{
    reinterpret_cast<UReferenceCounter*>(inObject)->Dereference();
}

// FUNCTION: EXE 0x5e6a39
// Saves under the typed name: in a running game the host sends a save message, in the editor a scenario save
// action is posted; asks before overwriting and requires a name; ignored after a save was handed on
long EEUIESLESaveFile::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    FSFileSpec theSpec;
    UString theFileName;
    UString theName;
    if (sSaved == false) {
        theName = static_cast<UIEditBox*>(inForm->GetControl(kSaveGameSaveAsGameEditBox))->GetText(0);
        if (theName.GetLength() > 0) {
            if (gUIManager->GetInScenarioEditor() != true && gGameScreenActive != false) {
                gGamePreferences.mSettings.SetLoadSavedGameName(theName);
                if (gCommunications->IsServer() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) {
                    theFileName = this->mDirectory + theName + this->mExtension;
                    NEMessage* theSave = new EEMLoadOrSaveGame(UWideString(theFileName), TMGetTime(), 0, 0,
                                                               gCommunications->GetNumberOfCTsProcessedInPhysics(), 0);
                    theSpec.SetFile(theName + this->mExtension);
                    theSpec.SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
                    if (theSpec.Open(kFileAccessRead) == kErrorOK) {
                        theSpec.Close();
                        gResource->GetString(IDS_FILE_ALREADY_EXISTS, theMessage);
                        gUIManager->DisplayQuestion(theMessage, true, reinterpret_cast<UIForm*>(theSave), SendSaveMessage,
                                                    DereferenceObject);
                    } else
                        SendSaveMessage(reinterpret_cast<UIForm*>(theSave));
                }
            } else {
                theFileName = theName + this->mExtension;
                gGamePreferences.mSettings.SetLoadScenarioName(theName);
                EEAction* theAction = new EEASaveGame(theFileName, true, this->mDirectory);
                theSpec.SetFile(theFileName);
                theSpec.SetDirectory(gStartupDirectory + this->mDirectory);
                if (theSpec.Open(kFileAccessRead) == kErrorOK) {
                    theSpec.Close();
                    gResource->GetString(IDS_FILE_ALREADY_EXISTS, theMessage);
                    gUIManager->DisplayQuestion(theMessage, true, reinterpret_cast<UIForm*>(theAction), PostSaveAction,
                                                DereferenceObject);
                } else
                    PostSaveAction(reinterpret_cast<UIForm*>(theAction));
            }
        } else {
            gResource->GetString(IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE, theMessage);
            gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e6d93
// Opens the load screen (single player only): pauses the game, lists the directory's files with the extension
// and preselects the last loaded name
long EEUIESLLoadScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UWideString theName;
    UString theVolume(gStartupVolume);
    UString theStartup(gStartupDirectory);
    UIForm* theForm = gUIManager->GetFormByID(kFormLoadGame);
    unsigned long theIndex = 0;
    if (gCommunications->GetIsMultiplayerSession() == false) {
        if (gFreeCamera == false && gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormGameOptions))
            gUIManager->Pop();
        gUIManager->SwapForm(theForm);
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        if (theServer && theServer->GetIsGamePaused() == false) {
            theServer->PauseGame();
            gUserInterface->PostPauseInformation(true);
        }
        UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kLoadGameLoadFileListBox));
        theList->AddValidExtension(this->mExtension);
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + this->mDirectory);
        thePath->AddDirectory(theDirectory, true);
        theList->SetSearchPaths(thePath);
        theList->CreateEntries(1);
        theName = gUIManager->mInScenarioEditor ? gLoadScenarioName : gLoadSavedGameName;
        if (theList->FindString(theName, theIndex) == true) {
            theList->ChangeSelectionIndex(theIndex, true, false);
            theList->SetScrollBarCurrentRange(theIndex);
        }
    }
    thePath->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e6fab
// Opens the save screen: pauses the game, lists the directory's files with the extension, preselects the last
// name and clears the saved flag
long EEUIESLSaveScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UIForm* theForm = gUIManager->GetFormByID(kFormSaveGame);
    unsigned long theIndex = 0;
    if (gFreeCamera == false && gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormGameOptions))
        gUIManager->SwapForm(theForm);
    else
        gUIManager->Push(theForm);
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    if (theServer->GetIsGamePaused() == false) {
        theServer->PauseGame();
        gUserInterface->PostPauseInformation(true);
    }
    UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kSaveGameSaveFileListBox));
    theList->AddValidExtension(this->mExtension);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + this->mDirectory);
    thePath->AddDirectory(theDirectory, true);
    theList->SetSearchPaths(thePath);
    theList->CreateEntries(1);
    theName = gUIManager->mInScenarioEditor ? gLoadScenarioName : gLoadSavedGameName;
    if (theList->FindString(theName, theIndex) == true) {
        theList->ChangeSelectionIndex(theIndex, true, false);
        theList->SetScrollBarCurrentRange(theIndex);
    }
    sSaved = false;
    thePath->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e717c
// Copies the selected file's name into the save screen's name box
long EEUIESLDisplayNewFile::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theSelection;
    UIForm* theForm = gUIManager->GetFormByID(kFormSaveGame);
    UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kSaveGameSaveFileListBox));
    UIEditBox* theEdit = static_cast<UIEditBox*>(theForm->GetControl(kSaveGameSaveAsGameEditBox));
    if (theList->GetCurrentSelection(theSelection) == kErrorOK)
        theEdit->SetText(theList->GetData(theSelection));
    return kErrorOK;
}

// ICF-FOLDED: EXE 0x5d2aa1
// Closes the campaign scenario form
// (no address of its own: the same body as EEUIECSSEBack::ProcessEvent, which ICF kept)
long EEUIESLECancelSave::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->CloseForm(NULL);
    return kErrorOK;
}
