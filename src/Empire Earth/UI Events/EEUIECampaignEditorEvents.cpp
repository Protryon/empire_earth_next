// C:\EEX\Empire Earth\UI Events\EEUIECampaignEditorEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIECE). The campaign editor form's events and their helpers: 0x1cfeef..0x1d1766 (the first UI Events unit). The
// file-scope EECampaignEditorBehavior (0x562df0, init chain 0x1cfeef) is the editor's model. Helpers are file statics
// defined in address order after their first users (forward-declared at the top). Unit flags: the exe's module set.
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
#include "ee/EECampaignFormManager.h"
#include "ee/EECampaignEditorBehavior.h"
#include "ee/EEUIECampaignEditorEvents.h"
#include "ee/EEStringIDs.h"
static long SaveCampaign(UIForm* inForm);
static long SaveCampaignNow(UIForm* inForm);
static void CloseEditor(UIForm* inForm);
static bool CheckUnsavedChanges(UIForm* inForm);
static void OnSaveBeforeOpen(UIForm* inForm);
static void SaveBeforeOpen(UIForm* inForm);
static void OnDiscardBeforeOpen(UIForm* inForm);
static long OpenCampaign(UIForm* inForm);
static void OnSaveBeforeNew(UIForm* inForm);
static void SaveBeforeNew(UIForm* inForm);
static void ShowMainScreen(UIForm* inForm);
static void OnSaveBeforeMainScreen(UIForm* inForm);
static void ShowScenarioEditor(UIForm* inForm);
static void OnSaveBeforeScenarioEditor(UIForm* inForm);
static bool MoveSelection(UIForm* inForm, long inDelta);
static void OnDiscardBeforeNew(UIForm* inForm);
static long NewCampaign(UIForm* inForm);
static bool GetSelectedCampaign(UIForm* inForm, UWideString& outName);
static bool DeleteSelectedCampaign(UIForm* inForm, UWideString& outError);
static void OnDeleteCampaign(UIForm* inForm);
static bool GetSelectedAvailableScenario(UIForm* inForm, UWideString& outName);
static bool GetSelectedScenario(UIForm* inForm, UWideString& outName);
static bool ReadForm(UIForm* inForm);
static void RefreshForm(UIForm* inForm);
static void UpdatePages(UIForm* inForm);
static void ShowPasswordPage(UIForm* inForm);
static void ShowFileName(UIForm* inForm, const UWideString& inText);
static void ShowTitle(UIForm* inForm, const UWideString& inText);
static bool ReadFileName(UIForm* inForm, UWideString& outText);
static bool ReadTitle(UIForm* inForm, UWideString& outText);
static bool ReadDescription(UIForm* inForm, UWideString& outText);
static bool ReadName(UIForm* inForm, UWideString& outText);
static bool ReadPassword(UIForm* inForm, UWideString& outText);
static bool ReadFirstScenario(UIForm* inForm, UWideString& outText);
static bool ReadAuthor(UIForm* inForm, UWideString& outText);
static void ShowAuthor(UIForm* inForm, const UWideString& inText);
static void ShowFirstScenario(UIForm* inForm, const UWideString& inText);
static void ShowName(UIForm* inForm, const UWideString& inText);
static void ShowDescription(UIForm* inForm, const UWideString& inText);
static void ShowPassword(UIForm* inForm, const UWideString& inText);
static void ShowCampaigns(UIForm* inForm);
static void ShowAvailableScenarios(UIForm* inForm);
static void ShowScenarioList(UIForm* inForm);
static void ShowCampaignScenarios(UIForm* inForm);

EECampaignEditorBehavior gCampaignEditor;   // 0x562df0

// FUNCTION: EXE 0x5cff19
// Opens the campaign editor: fills the background image list with the scenario .jpg files and starts a new campaign
long EEUIECEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UString theExtension(".jpg");
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UIFileComboBox* theList = static_cast<UIFileComboBox*>(inForm->GetControl(kCampaignEditorBackgroundImageComboBox));
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    thePath->AddDirectory(theDirectory, true);
    gResource->GetString(IDS_SE_MAP_NONE, theText);
    theList->AddValidExtension(theExtension);
    theExtension.ToUpper();
    theList->AddValidExtension(theExtension);
    theList->SetSearchPaths(thePath);
    theList->CreateEntries(1);
    theList->InsertLine(theText, 0);
    theList->SetCurrentSelection(0);
    thePath->Dereference();
    gCampaignEditor.Reset();
    long theResult = NewCampaign(inForm);
    RefreshForm(inForm);
    return theResult;
}

// FUNCTION: EXE 0x5d0095
// Adds the selected available scenario to the campaign and refreshes the scenario lists; 0 on success, else 1
long EEUIECEAddScenario::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theScenario;
    long theResult = 1;
    if (GetSelectedAvailableScenario(inForm, theScenario) && gCampaignEditor.AddScenario(theScenario)) {
        ShowAvailableScenarios(inForm);
        ShowCampaignScenarios(inForm);
        ShowScenarioList(inForm);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d00f6
// Removes the selected scenario from the campaign and refreshes the scenario lists; 0 on success, else 1
long EEUIECERemoveScenario::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theScenario;
    long theResult = 1;
    if (GetSelectedScenario(inForm, theScenario) && gCampaignEditor.RemoveScenario(theScenario)) {
        ShowAvailableScenarios(inForm);
        ShowCampaignScenarios(inForm);
        ShowScenarioList(inForm);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0157
// Moves the selected campaign scenario one place up, keeping it selected; 0 on success, else 1
long EEUIECEMoveScenarioUp::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theScenario;
    long theResult = 1;
    if (GetSelectedScenario(inForm, theScenario) && gCampaignEditor.MoveScenario(theScenario, -1)
        && MoveSelection(inForm, -1)) {
        RefreshForm(inForm);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d01bb
// Moves the selected campaign scenario one place down, keeping it selected; 0 on success, else 1
long EEUIECEMoveScenarioDown::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theScenario;
    long theResult = 1;
    if (GetSelectedScenario(inForm, theScenario) && gCampaignEditor.MoveScenario(theScenario, theResult)
        && MoveSelection(inForm, theResult)) {
        RefreshForm(inForm);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d021d
// Saves the edited campaign (shows a wait message and posts the save)
long EEUIECECampaignSave::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    SaveCampaign(inForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d022c
// Shows a please-wait message and posts the deferred save event for the form; always returns 0
static long SaveCampaign(UIForm* inForm)
{
    UWideString theMessage;
    gResource->GetString(IDS_PLEASE_WAIT, theMessage);
    gUIManager->DisplayMessage(theMessage, true, NULL, 0, false);
    // PostEvent takes its own reference: drop the creation reference (the event leaked).
    UIEvent* theEvent = new EEUIECECampaignSaveContinued(inForm);
    gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return 0;
}

// FUNCTION: EXE 0x5d02a8
// Reads the form into the campaign and saves it, showing any save error; then closes the wait message
long EEUIECECampaignSaveContinued::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theError;
    if (ReadForm(this->mForm)) {
        if (!gCampaignEditor.Save(theError)) {
            gUIManager->DisplayMessage(theError, true, NULL, CloseEditor, true);
            return kErrorOK;
        }
        ShowCampaigns(this->mForm);
    }
    RefreshForm(this->mForm);
    CloseEditor(NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d031f
// Destroys the deferred-save event and frees it when asked (scalar deleting destructor)
// (emitted after its first user's inline members)

// FUNCTION: EXE 0x5d034a
// Opens the selected campaign, first asking whether to save unsaved changes to the current one
long EEUIECECampaignSelected::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    long theResult;
    if (CheckUnsavedChanges(inForm) == true) {
        gResource->GetString(IDS_SAVE_CURRENT_CAMPAIGN_BEFORE_LOADING, theMessage);
        gUIManager->DisplayQuestion(theMessage, true, inForm, OnSaveBeforeOpen, OnDiscardBeforeOpen);
        theResult = 0;
    } else
        theResult = OpenCampaign(inForm);
    return theResult;
}

// FUNCTION: EXE 0x5d03b9
// Reports unsaved changes: unlocked campaign with an entered password or a changed name, author, email or password
static bool CheckUnsavedChanges(UIForm* inForm)
{
    if (gCampaignEditor.GetIsUnlocked() == true) {
        if (gCampaignEditor.GetPasswordEntered() == true
            || UWideString(static_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignNameEditBox))->GetText(0)) != gCampaignEditor.GetName()
            || UWideString(static_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorNameEditBox))->GetText(0)) != gCampaignEditor.GetAuthorName()
            || UWideString(static_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorsEmailEditBox))->GetText(0)) != gCampaignEditor.GetAuthorEmail()
            || UWideString(static_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignPasswordEditBox))->GetText(0)) != gCampaignEditor.GetPassword())
            return true;
    }
    return false;
}

// FUNCTION: EXE 0x5d0517
// Saves the current campaign, then opens the selected one (question callback)
static void OnSaveBeforeOpen(UIForm* inForm)
{
    SaveBeforeOpen(inForm);
}

// Saves the edited campaign at once (for the "save before ..." answers, whose next step replaces the form
// contents, so the deferred save would store the next campaign); returns 0 when it was saved
static long SaveCampaignNow(UIForm* inForm)
{
    UWideString theError;
    if (!ReadForm(inForm)) {
        RefreshForm(inForm);
        return 1;
    }
    if (!gCampaignEditor.Save(theError)) {
        gUIManager->DisplayMessage(theError, true, NULL, 0, true);
        return 1;
    }
    ShowCampaigns(inForm);
    return 0;
}

// FUNCTION: EXE 0x5d0522
// Saves the current campaign, then opens the selected one (moved down a line if the save added a file to the list)
static void SaveBeforeOpen(UIForm* inForm)
{
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)inForm->GetControl(10);
    unsigned long theCount = theList->GetLineCount();
    if (SaveCampaignNow(inForm) != 0)
        return;
    unsigned long theSelection;
    if (theCount < theList->GetLineCount() && theList->GetCurrentSelection(theSelection) == kErrorOK)
        theList->ChangeSelectionIndex(theSelection + 1, false, false);
    OpenCampaign(inForm);
}

// FUNCTION: EXE 0x5d0584
// Opens the selected campaign without saving (question callback)
static void OnDiscardBeforeOpen(UIForm* inForm)
{
    OpenCampaign(inForm);
}

// FUNCTION: EXE 0x5d058f
// Loads the selected campaign file into the editor and refreshes the form; on a load error shows it and starts anew
static long OpenCampaign(UIForm* inForm)
{
    UWideString theError;
    UWideString theName;
    long theResult = 3;
    if (GetSelectedCampaign(inForm, theName)) {
        if (gCampaignEditor.Load(theName, theError)) {
            ShowPassword(inForm, "");
            RefreshForm(inForm);
            theResult = 0;
        } else {
            static_cast<EEUIManager*>(inForm->GetManager())->DisplayMessage(theError, true, NULL, 0, true);
            NewCampaign(inForm);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0633
// Reads the form and unlocks the campaign with the entered password, then refreshes the form; always returns 3
long EEUIECEPasswordEntered::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString thePassword;
    if (!ReadPassword(inForm, thePassword))
        return 3;
    ReadForm(inForm);
    gCampaignEditor.SetPassword(thePassword);
    RefreshForm(inForm);
    return 3;
}

// FUNCTION: EXE 0x5d0684
// Asks whether to delete the selected campaign file, or tells the player to select one first
long EEUIECECampaignDelete::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theQuestion;
    UWideString theMessage;
    if (GetSelectedCampaign(inForm, theMessage)) {
        gResource->GetString(IDS_DELETE_SELECTED_FILE, theQuestion);
        gUIManager->DisplayQuestion(theQuestion, true, inForm, OnDeleteCampaign, 0);
    } else {
        gResource->GetString(IDS_YOU_MUST_FIRST_SELECT_CAMPAIGN, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d0722
// Starts a new campaign, first asking whether to save unsaved changes to the current one
long EEUIECECampaignNew::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    long theResult;
    if (CheckUnsavedChanges(inForm) == true) {
        gResource->GetString(IDS_SAVE_CURRENT_CAMPAIGN_BEFORE_CREATING, theMessage);
        gUIManager->DisplayQuestion(theMessage, true, inForm, OnSaveBeforeNew, OnDiscardBeforeNew);
        theResult = 0;
    } else
        theResult = NewCampaign(inForm);
    return theResult;
}

// FUNCTION: EXE 0x5d0791
// Saves the current campaign, then starts a new one (question callback)
static void OnSaveBeforeNew(UIForm* inForm)
{
    SaveBeforeNew(inForm);
}

// FUNCTION: EXE 0x5d079c
// Saves the current campaign, then starts a new one
static void SaveBeforeNew(UIForm* inForm)
{
    if (SaveCampaignNow(inForm) == 0)
        NewCampaign(inForm);
}

// FUNCTION: EXE 0x5d07b5
// Leaves the campaign editor for the main screen, first asking whether to save unsaved changes
long EEUIECEMainScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    if (CheckUnsavedChanges(inForm) == true) {
        gResource->GetString(IDS_SAVE_CURRENT_CAMPAIGN_BEFORE_EXITING, theMessage);
        static_cast<EEUIManager*>(inForm->GetManager())->DisplayQuestion(theMessage, true, inForm, OnSaveBeforeMainScreen,
                                                              ShowMainScreen);
    } else
        ShowMainScreen(inForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d081d
// Closes the campaign editor form to return to the main screen
static void ShowMainScreen(UIForm* inForm)
{
    UWideString theText;
    gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x5d0844
// Saves the current campaign, then returns to the main screen
static void OnSaveBeforeMainScreen(UIForm* inForm)
{
    if (SaveCampaignNow(inForm) == 0)
        ShowMainScreen(inForm);
}

// FUNCTION: EXE 0x5d085d
// Switches to the scenario editor, first asking whether to save unsaved campaign changes
long EEUIECEScenarioEditorOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    if (CheckUnsavedChanges(inForm) == true) {
        gResource->GetString(IDS_SAVE_CURRENT_CAMPAIGN_BEFORE_EXITING, theMessage);
        static_cast<EEUIManager*>(inForm->GetManager())->DisplayQuestion(theMessage, true, inForm, OnSaveBeforeScenarioEditor,
                                                              ShowScenarioEditor);
    } else
        ShowScenarioEditor(inForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d08c5
// Creates and starts the scenario editor
static void ShowScenarioEditor(UIForm* inForm)
{
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    theManager->CreateScenarioEditor();
    theManager->StartScenarioEditor();
}

// FUNCTION: EXE 0x5d08e0
// Saves the current campaign, then switches to the scenario editor
static void OnSaveBeforeScenarioEditor(UIForm* inForm)
{
    if (SaveCampaignNow(inForm) == 0)
        ShowScenarioEditor(inForm);
}

// FUNCTION: EXE 0x5d08f9
// Moves the selection in the campaign's scenario list by a delta (not above the top); returns whether it moved
static bool MoveSelection(UIForm* inForm, long inDelta)
{
    bool theResult = false;
    unsigned long theSelection = 0;
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorScensInCampListBox));
    if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK) {
        long theIndex = inDelta + theSelection;
        if (theIndex >= 0) {
            theList->ChangeSelectionIndex(theIndex, false, true);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0960
// Starts a new campaign without saving (question callback)
static void OnDiscardBeforeNew(UIForm* inForm)
{
    NewCampaign(inForm);
}

// FUNCTION: EXE 0x5d096b
// Resets the editor to a new campaign, clears the campaign file selection and refreshes the form; 0 on success
static long NewCampaign(UIForm* inForm)
{
    UWideString theText;
    long theResult = gCampaignEditor.New();
    if (theResult == 0) {
        UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorCampaignFileListBox));
        if (theList) {
            theList->ClearSelection();
            ShowPassword(inForm, "");
            RefreshForm(inForm);
        } else
            theResult = 3;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d09fb
// Returns the selected campaign file name (with the campaign extension); false when nothing is selected
static bool GetSelectedCampaign(UIForm* inForm, UWideString& outName)
{
    bool theResult = false;
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorCampaignFileListBox));
    unsigned long theSelection;
    if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK) {
        outName = (const char*)theList->GetData(theSelection);
        outName += gCampaignExtension;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0a6c
// Deletes the selected campaign file; false (with the error text) when none is selected or the delete fails
static bool DeleteSelectedCampaign(UIForm* inForm, UWideString& outError)
{
    UWideString theName;
    bool theResult = false;
    if (GetSelectedCampaign(inForm, theName))
        theResult = gCampaignEditor.Delete(theName, outError);
    return theResult;
}

// FUNCTION: EXE 0x5d0ab1
// Deletes the selected campaign file and opens the next selection (or a new campaign); shows the error on failure
static void OnDeleteCampaign(UIForm* inForm)
{
    UWideString theError;
    if (DeleteSelectedCampaign(inForm, theError)) {
        RefreshForm(inForm);
        if (OpenCampaign(inForm) == 3) {
            gCampaignEditor.New();
            RefreshForm(inForm);
        }
    } else
        gUIManager->DisplayMessage(theError, true, NULL, 0, true);
}

// FUNCTION: EXE 0x5d0b1d
// Returns the selected available scenario's name; false when nothing (or an empty line) is selected
static bool GetSelectedAvailableScenario(UIForm* inForm, UWideString& outName)
{
    bool theResult = false;
    unsigned long theSelection = 0;
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorScensAvailListBox));
    if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK) {
        outName = theList->GetData(theSelection);
        if (outName != "")
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0b94
// Returns the name of the selected scenario in the campaign's scenario list; false when nothing is selected
static bool GetSelectedScenario(UIForm* inForm, UWideString& outName)
{
    bool theResult = false;
    unsigned long theSelection = 0;
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorScensInCampListBox));
    if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK) {
        outName = theList->GetData(theSelection);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0bfa
// Copies the form fields (name, description, author, email, password, civilization, background) into the campaign
static bool ReadForm(UIForm* inForm)
{
    UWideString theText;
    bool theResult = false;
    if (ReadName(inForm, theText)) {
        gCampaignEditor.SetName(theText);
        if (ReadDescription(inForm, theText)) {
            gCampaignEditor.SetDescription(theText);
            if (ReadTitle(inForm, theText)) {
                gCampaignEditor.SetAuthorName(theText);
                if (ReadFileName(inForm, theText)) {
                    gCampaignEditor.SetAuthorEmail(theText);
                    if (ReadPassword(inForm, theText)) {
                        gCampaignEditor.SetPassword(theText);
                        if (ReadFirstScenario(inForm, theText)) {
                            gCampaignEditor.SetRequiredCiv(theText);
                            if (ReadAuthor(inForm, theText)) {
                                gCampaignEditor.SetBackgroundImage(theText);
                                theResult = true;
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0ced
// Shows the campaign's fields and every list in the form, then shows the page that fits its lock state
static void RefreshForm(UIForm* inForm)
{
    std::vector<UWideString> theScenarios;
    UWideString theTitle;
    UWideString theFileName;
    UWideString theDescription;
    UWideString theName;
    UWideString theAuthor;
    UWideString theFirstScenario;
    theName = gCampaignEditor.GetName();
    ShowName(inForm, theName);
    theTitle = gCampaignEditor.GetAuthorName();
    ShowTitle(inForm, theTitle);
    theFileName = gCampaignEditor.GetAuthorEmail();
    ShowFileName(inForm, theFileName);
    theAuthor = gCampaignEditor.GetBackgroundImage();
    ShowAuthor(inForm, theAuthor);
    theFirstScenario = gCampaignEditor.GetRequiredCiv();
    ShowFirstScenario(inForm, theFirstScenario);
    theDescription = gCampaignEditor.GetDescription();
    ShowDescription(inForm, theDescription);
    ShowCampaigns(inForm);
    ShowAvailableScenarios(inForm);
    ShowCampaignScenarios(inForm);
    ShowScenarioList(inForm);
    UpdatePages(inForm);
}

// FUNCTION: EXE 0x5d0e64
// Shows the editing page, or the password page when the campaign is locked
static void UpdatePages(UIForm* inForm)
{
    UWideString theText1;
    UWideString theText2;
    inForm->GetEntityGroup(2)->Disable();
    inForm->GetEntityGroup(1)->Enable();
    if (!gCampaignEditor.GetIsUnlocked())
        ShowPasswordPage(inForm);
}

// FUNCTION: EXE 0x5d0ec8
// Shows the password page and disables the editing page
static void ShowPasswordPage(UIForm* inForm)
{
    inForm->GetEntityGroup(1)->Disable();
    inForm->GetEntityGroup(2)->Enable();
}

// FUNCTION: EXE 0x5d0eeb
// Shows the author's email in its edit box and its read-only label
static void ShowFileName(UIForm* inForm, const UWideString& inText)
{
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorsEmailEditBox));
    if (theBox)
        theBox->SetText(inText);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(kCampaignEditorDisabledAuthorsEmailEditBox));
    if (theLabel)
        theLabel->SetText(inText);
}

// FUNCTION: EXE 0x5d0f53
// Shows the author's name in its edit box and its read-only label
static void ShowTitle(UIForm* inForm, const UWideString& inText)
{
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorNameEditBox));
    if (theBox)
        theBox->SetText(inText);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(kCampaignEditorDisabledAuthorNameEditBox));
    if (theLabel)
        theLabel->SetText(inText);
}

// FUNCTION: EXE 0x5d0fbb
// Reads the author's email edit box; false if the control is missing
static bool ReadFileName(UIForm* inForm, UWideString& outText)
{
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorsEmailEditBox));
    if (theBox) {
        outText = theBox->GetText(0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d0ffd
// Reads the author's name edit box; false if the control is missing
static bool ReadTitle(UIForm* inForm, UWideString& outText)
{
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorAuthorNameEditBox));
    if (theBox) {
        outText = theBox->GetText(0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d103f
// Reads the whole campaign description edit box; false if the control is missing
static bool ReadDescription(UIForm* inForm, UWideString& outText)
{
    UWideString theText;
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignDescripEditBox));
    if (theBox) {
        theBox->GetAllText(outText);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d1091
// Reads the campaign name edit box; false if the control is missing
static bool ReadName(UIForm* inForm, UWideString& outText)
{
    UWideString theText;
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignNameEditBox));
    if (theBox) {
        outText = theBox->GetText(0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d10ea
// Reads the whole password edit box; false if the control is missing
static bool ReadPassword(UIForm* inForm, UWideString& outText)
{
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignPasswordEditBox));
    if (theBox) {
        theBox->GetAllText(outText);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d1128
// Reads the whole required-civilization edit box; false if the control is missing
static bool ReadFirstScenario(UIForm* inForm, UWideString& outText)
{
    bool theResult = false;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCivNameEditBox));
    if (theBox) {
        theBox->GetAllText(outText);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d1166
// Reads the selected background image file name (empty when none is selected); false if the control is missing
static bool ReadAuthor(UIForm* inForm, UWideString& outText)
{
    bool theResult = false;
    UIFileComboBox* theList = dynamic_cast<UIFileComboBox*>(inForm->GetControl(kCampaignEditorBackgroundImageComboBox));
    if (theList) {
        unsigned long theSelection;
        if (theList->GetCurrentSelection(theSelection) == kErrorOK)
            outText = theList->GetListBox()->GetData(theSelection);
        else
            outText = "";
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x5d11d1
// Selects the background image by name (case-insensitive), else adds and selects a "none" line; mirrors it in its label
static void ShowAuthor(UIForm* inForm, const UWideString& inText)
{
    UWideString theText;
    bool theFound = false;
    UIFileComboBox* theList = dynamic_cast<UIFileComboBox*>(inForm->GetControl(kCampaignEditorBackgroundImageComboBox));
    if (theList) {
        const UILBString* theLines = theList->GetListBox();
        for (unsigned long i = 0; i < theLines->GetLineCount(); i++) {
            if (!theLines->GetData(i).EqualsIgnoreCase(inText))
                continue;
            theFound = true;
            theList->SetCurrentSelection(i);
            break;
        }
        if (!theFound) {
            gResource->GetString(IDS_SE_MAP_NONE, theText);
            theList->CreateEntries(1);
            theList->InsertLine(theText, 0);
            theList->SetCurrentSelection(0);
        }
    }
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(kCampaignEditorDisabledBGImageCombo));
    if (theLabel) {
        if (theFound)
            theLabel->SetText(inText);
        else
            theLabel->SetText(UWideString(""));
    }
}

// FUNCTION: EXE 0x5d131b
// Shows the translated required-civilization name in its edit box and its read-only label
static void ShowFirstScenario(UIForm* inForm, const UWideString& inText)
{
    UWideString theText;
    EETranslateString(inText, theText);
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCivNameEditBox));
    if (theBox)
        theBox->SetText(theText);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(kCampaignEditorDisabledCivNameEditBox));
    if (theLabel)
        theLabel->SetText(theText);
}

// FUNCTION: EXE 0x5d13ad
// Shows the translated campaign name in its edit box
static void ShowName(UIForm* inForm, const UWideString& inText)
{
    UWideString theText;
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(2));
    if (theBox) {
        EETranslateString(inText, theText);
        theBox->SetText(theText);
    }
}

// FUNCTION: EXE 0x5d140c
// Shows the translated campaign description in its edit box and its read-only label
static void ShowDescription(UIForm* inForm, const UWideString& inText)
{
    UWideString theText;
    EETranslateString(inText, theText);
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignDescripEditBox));
    if (theBox)
        theBox->SetText(theText);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(inForm->GetControl(kCampaignEditorDisabledCampaignDescripEditBox));
    if (theLabel)
        theLabel->SetText(theText);
}

// FUNCTION: EXE 0x5d1498
// Puts a text into the password edit box
static void ShowPassword(UIForm* inForm, const UWideString& inText)
{
    UIEditBox* theBox = dynamic_cast<UIEditBox*>(inForm->GetControl(kCampaignEditorCampaignPasswordEditBox));
    if (theBox)
        theBox->SetText(inText);
}

// FUNCTION: EXE 0x5d14d0
// Refills the campaign file list from the editor, keeping the selected line index
static void ShowCampaigns(UIForm* inForm)
{
    std::vector<UWideString> theCampaigns;
    bool theHadSelection = false;
    unsigned long theSelection;
    gCampaignEditor.GetScenarios(theCampaigns);
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorCampaignFileListBox));
    if (!theList)
        return;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK)
        theHadSelection = true;
    theList->DeleteAllEntries();
    theList->AppendStringArray(theCampaigns);
    if (theHadSelection != false)
        theList->ChangeSelectionIndex(theSelection, false, true);
}

// FUNCTION: EXE 0x5d156e
// Refills the available scenarios list from the editor, keeping the selected line index
static void ShowAvailableScenarios(UIForm* inForm)
{
    std::vector<UWideString> theScenarios;
    bool theHadSelection = false;
    unsigned long theSelection;
    gCampaignEditor.GetAvailableScenarios(theScenarios);
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorScensAvailListBox));
    if (!theList)
        return;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK)
        theHadSelection = true;
    theList->DeleteAllEntries();
    theList->AppendStringArray(theScenarios);
    if (theHadSelection != false)
        theList->ChangeSelectionIndex(theSelection, false, true);
}

// FUNCTION: EXE 0x5d160c
// Refills the list of files the campaign uses (its scenario list) and shows any error the editor reports
static void ShowScenarioList(UIForm* inForm)
{
    UWideString theError;
    std::vector<UWideString> theScenarios;
    bool theResult = gCampaignEditor.GetScenarioList(theScenarios, theError);
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorAssetsInCampaignListBox));
    if (theList) {
        theList->DeleteAllEntries();
        theList->AppendStringArray(theScenarios);
    }
    if (theResult == false)
        (static_cast<EEUIManager*>(inForm->GetManager()))->DisplayMessage(theError, true, NULL, 0, true);
}

// FUNCTION: EXE 0x5d16b3
// Refills the campaign's scenario list from the editor, keeping the selected line index
static void ShowCampaignScenarios(UIForm* inForm)
{
    std::vector<UWideString> theScenarios;
    UWideString theText;
    bool theHadSelection = false;
    unsigned long theSelection;
    gCampaignEditor.GetCampaignScenarios(theScenarios);
    UIListBox<UWideString>* theList = dynamic_cast<UIListBox<UWideString>*>(inForm->GetControl(kCampaignEditorScensInCampListBox));
    if (!theList)
        return;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK)
        theHadSelection = true;
    theList->DeleteAllEntries();
    theList->AppendStringArray(theScenarios);
    if (theHadSelection != false)
        theList->ChangeSelectionIndex(theSelection, false, true);
}

// Closes the top form, the wait or error message (the save callback: ICF-folded into
// EEUIEChooseCustomCampaignEvents.cpp's identical static, 0x1d3e52)
static void CloseEditor(UIForm* inForm)
{
    gUserInterface->CloseForm(NULL);
}

// SYNTHETIC: EXE 0x5cfeef
// SYNTHETIC: EXE 0x5cfef9
// SYNTHETIC: EXE 0x5cff03
// SYNTHETIC: EXE 0x5cff0f
