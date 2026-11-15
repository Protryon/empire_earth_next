// C:\EEX\Empire Earth\Civilization Builder\EECivilizationBuilder.cpp (file name inferred: no __FILE__ or RTTI).
// 0x27efca..0x27fdfe, between EECBUIListBox.cpp and the civilization file unit at 0x27fdfe: its own
// UString init chain (0x9654a8), then the civilization builder forms' helpers (form 7 the builder, 0xf the name
// dialog, 0x10/0x11 the save/load dialogs, 0x23 the in-game civilization selection) and EEProcessTimerExpired.
// Function names Fn<rva> are placeholders (by address). Unit flags: the exe module set.
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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"

#include "ee/EECBUIListBox.h"

#include "ee/EECivilizationManager.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // EE_SCREENS_PDCS_*: the civilization choice form's events (ee_screens civ_choice.rs)
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIFileListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EEGameSound.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioEditorIDs.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// chain at 0x67efca (wrapper)
//   object 0x9654a8  ctor __imp_??0UString@@QAE@XZ  dtor __imp_??1UString@@UAE@XZ
// the last civilization file name saved (CivBuilderSaveAs; CivBuilderShowSaveDialog offers it in the save box) (provisional name)
UString gLastCivilizationFile;
// SYNTHETIC: EXE 0x67efca   (wrapper)
// SYNTHETIC: EXE 0x67efd4   (ctor)
// SYNTHETIC: EXE 0x67efdf   (registrar)
// SYNTHETIC: EXE 0x67efeb   (dtor)

// FUNCTION: EXE 0x67eff6
// Accepts the change-name dialog: a non-empty typed name becomes the builder's title, then the dialog closes
long CivBuilderAcceptName()
{
    UWideString theText;
    long theResult = 3;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderNewCiv);
    if (theForm == NULL)
        return theResult;
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdNewCivNameTextField);
    UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
    if (theEdit == NULL)
        return theResult;
    theText = theEdit->GetText(0);
    if (!(theText == gEmptyString)) {
        theResult = CivBuilderSetName(theText);
        if (theResult == 0)
            theResult = CivBuilderCloseForm();
    }
    return theResult;
}

// FUNCTION: EXE 0x67f08f
// Finds the file list of the given load (0x11) or save (0x10) dialog; returns false for any other form
bool CivBuilderGetFileList(UIForm* inForm, UIFileListBox** outList)
{
    bool theResult = false;
    UIControl* theControl = NULL;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderLoadCiv);
    if (theForm == inForm)
        theControl = theForm->GetControlById(kCivBuilderIdLoadCivFileListBox);
    else {
        theForm = gUIManager->GetFormByID(kFormCivBuilderSaveCiv);
        if (theForm == inForm)
            theControl = theForm->GetControlById(kCivBuilderIdSaveCivFileListBox);
    }
    if (theControl != NULL) {
        UIFileListBox* theList = dynamic_cast<UIFileListBox*>(theControl);
        if (theList != NULL) {
            *outList = theList;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67f0f7
// Returns the main game form's civilization builder button (control 5), or NULL
UIBlinkingButton* CivBuilderGetMainGameButton()
{
    UIBlinkingButton* theResult = NULL;
    UIForm* theForm = gUIManager->GetFormByID(kFormMainGame);
    if (theForm != NULL) {
        UIControl* theControl = theForm->GetControl(kMainGameCivSelectorBuilder);
        if (theControl != NULL)
            theResult = dynamic_cast<UIBlinkingButton*>(theControl);
    }
    return theResult;
}

// FUNCTION: EXE 0x67f134
// Reads the civilization name from the builder's title label; returns 3 when the label is missing
long CivBuilderGetName(UWideString& outName)
{
    UWideString theText;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    long theResult = 3;
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdCivNameMainTitle);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(theControl);
    if (theLabel != NULL) {
        theText = theLabel->GetText(0);
        outName = theText;
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f1a9
// Reads the text typed in the change-name dialog's edit box; returns 3 when it is missing
long CivBuilderGetTypedName(UWideString& outText)
{
    UWideString theText;
    long theResult = 3;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderNewCiv);
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdNewCivNameTextField);
    UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
    if (theEdit != NULL) {
        theText = theEdit->GetText(0);
        outText = theText;
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f221
// Reads the file name typed in the save dialog's edit box; returns 3 when it is missing
long CivBuilderGetTypedFileName(UWideString& outText)
{
    UWideString theText;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderSaveCiv);
    long theResult = 3;
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdSaveCivNameTextField);
    UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
    if (theEdit != NULL) {
        theText = theEdit->GetText(0);
        outText = theText;
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f299
// Reads the id, bonus and cost of the selected line in the available (0xcb, inBought true) or bought (0xcc)
// upgrade list
bool CivBuilderGetSelectedUpgrade(bool inBought, EECivUpgrade* outUpgrade, unsigned long* outBonus, unsigned long* outCost)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    bool theResult = false;
    EECBUIListBox* theList;
    if (inBought)
        theList = CivBuilderGetUpgradeList(theForm, 0xcb);
    else
        theList = CivBuilderGetUpgradeList(theForm, 0xcc);
    if (theList != NULL && theList->GetSelectedUpgrade(outUpgrade, outBonus, outCost) == 0)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x67f2e8
// Hides the builder's points labels and accept-changes button; always returns 3
long CivBuilderHidePoints()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    if (theForm != NULL) {
        theForm->GetControlById(kCivBuilderIdCivPtsLeftLabel)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControlById(kCivBuilderIdCivPtsLeft)->SetDisplayMode(kDisplayModeHidden);
        theForm->GetControlById(kCivBuilderIdCivAcceptChangesButton)->SetDisplayMode(kDisplayModeHidden);
    }
    return 3;
}

// FUNCTION: EXE 0x67f340
// Closes the current form
long CivBuilderCloseForm()
{
    return gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x67f34e
// Fills both upgrade lists: 0xcc with the given upgrades, 0xcb with all others; both are selectable unless
// the lists are locked (gB565566)
// the original fills control 0xcc (vtable +0xc8) before 0xcb (+0xc4)
long CivBuilderFillUpgradeLists(std::vector<EECivUpgrade>& inUpgrades)
{
    long theResult = 3;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    if (theForm != NULL) {
        UIControl* theControl = theForm->GetControlById(kSETerrainTerrainType);
        EECBUIListBox* theList = dynamic_cast<EECBUIListBox*>(theControl);
        theList->SetCivilizationUpgrades(inUpgrades);
        theList->mSelectUpgradesOnly = gB565566 == false;
        theControl = theForm->GetControlById(kSETerrainTerrainTypeLabel);
        theList = dynamic_cast<EECBUIListBox*>(theControl);
        theList->SetUpgradesExcept(inUpgrades);
        theList->mSelectUpgradesOnly = gB565566 == false;
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f3fc
// Opens the civilization builder form
long CivBuilderPushBuilderForm()
{
    return gUIManager->Push(gUIManager->GetFormByID(kFormCivBuilder));
}

// FUNCTION: EXE 0x67f418
// Opens the form with the given id; returns 3 when it does not exist
long CivBuilderPushForm(unsigned long inFormID)
{
    UIForm* theForm = gUIManager->GetFormByID(inFormID);
    long theResult = 3;
    if (theForm != NULL)
        theResult = gUIManager->Push(theForm);
    return theResult;
}

// FUNCTION: EXE 0x67f43f
// Shows a name in the builder's title label; returns 3 when the label is missing
long CivBuilderSetName(const UWideString& inName)
{
    UWideString theName(inName);
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    long theResult = 3;
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdCivNameMainTitle);
    UITextLabel* theLabel = dynamic_cast<UITextLabel*>(theControl);
    if (theLabel != NULL) {
        theLabel->SetText(theName);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f4a9
// Shows the points left (0 when negative) and the points spent (max - left) on the builder form
long EEShowCivBuilderPoints(long inPointsLeft, long inMax)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    long theResult = 3;
    if (theForm != NULL) {
        UIControl* theControl = theForm->GetControlById(kSETerrainTerrainColorTypeLabel);
        UITextLabel* theLabel = dynamic_cast<UITextLabel*>(theControl);
        if (theLabel != NULL) {
            theLabel->SetText(inMax - inPointsLeft);
            theControl = theForm->GetControlById(kCivBuilderIdCivPtsLeft);
            theLabel = dynamic_cast<UITextLabel*>(theControl);
            if (theLabel != NULL) {
                if (inPointsLeft < 0)
                    theLabel->SetText(0);
                else
                    theLabel->SetText(inPointsLeft);
                theResult = 0;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67f546
// Lists the civilization files (Users\default\Civilizations\*.civ) in a file list box
long CivBuilderFillFileList(UIFileListBox* inList)
{
    FSPath* thePath = new FSPath;
    UString theDirectory;
    UWideString theUnused;
    FSFileSpec theSpec;
    theSpec.SetVolume(gStartupVolume);
    theDirectory = gU565488 + "\\" + gDefaultUserFolder + "\\" + gU565498 + "\\";
    theSpec.SetDirectory(gStartupDirectory + theDirectory);
    thePath->AddDirectory(theSpec, true);
    inList->SetSearchPaths(thePath);
    inList->AddValidExtension(gU565478);
    inList->CreateEntries(1);
    thePath->Dereference();
    return 0;
}

// FUNCTION: EXE 0x67f6a4
// Saves the civilization under the name typed in the save dialog and remembers that name
long CivBuilderSaveAs()
{
    UWideString theName;
    long theResult = CivBuilderGetTypedFileName(theName);
    if (theResult == 0) {
        theResult = CivBuilderSaveCivilization(UString(theName), false);
        gLastCivilizationFile = theName;
    }
    return theResult;
}

// FUNCTION: EXE 0x67f700
// Opens the change-name dialog with the current name (or the default string 0x1b98) selected for editing
long CivBuilderShowNameDialog()
{
    UWideString theText;
    long theResult = CivBuilderPushForm(0xf);
    if (theResult == 0) {
        theResult = CivBuilderGetName(theText);
        UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderNewCiv);
        UIControl* theControl = theForm->GetControlById(kCivBuilderIdNewCivNameTextField);
        UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
        if (theText.GetLength() == 0)
            gResource->GetString(IDS_CIVILIZATION_NAME, theText);
        theEdit->SetText(theText);
        theEdit->SelectText();
        theForm->SetTabStop(theEdit->GetIndex());
    }
    return theResult;
}

// FUNCTION: EXE 0x67f7b1
// Opens the load dialog with its civilization file list
long CivBuilderShowLoadDialog()
{
    return CivBuilderShowFileDialog(0x11, 0x1a9);
}

// FUNCTION: EXE 0x67f7c0
// Opens a dialog and fills its file list control with the civilization files
long CivBuilderShowFileDialog(unsigned long inFormID, unsigned long inControl)
{
    UWideString theUnused;
    long theResult = CivBuilderPushForm(inFormID);
    if (theResult == 0) {
        UIForm* theForm = gUIManager->GetFormByID(inFormID);
        UIControl* theControl = theForm->GetControlById(inControl);
        theResult = CivBuilderFillFileList(dynamic_cast<UIFileListBox*>(theControl));
    }
    return theResult;
}

// FUNCTION: EXE 0x67f824
// Opens the save dialog with the last saved file name (or the civilization's name) in its edit box
long CivBuilderShowSaveDialog()
{
    UWideString theText;
    long theResult = CivBuilderShowFileDialog(0x10, 0x1a4);
    if (theResult != 0)
        return theResult;
    if (gLastCivilizationFile != gEmptyString)
        theText = (const char*)gLastCivilizationFile;
    else
        theResult = CivBuilderGetName(theText);
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderSaveCiv);
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdSaveCivNameTextField);
    UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
    theEdit->SetText(theText);
    return theResult;
}

// FUNCTION: EXE 0x67f8c7
// Returns the form's control with the given id as an upgrade list box, or NULL
EECBUIListBox* CivBuilderGetUpgradeList(UIForm* inForm, unsigned long inControl)
{
    EECBUIListBox* theResult = NULL;
    UIControl* theControl = inForm->GetControlById(inControl);
    if (theControl != NULL)
        theResult = dynamic_cast<EECBUIListBox*>(theControl);
    return theResult;
}

// FUNCTION: EXE 0x67f8f7
// Returns the load dialog's civilization file list
UIFileListBox* CivBuilderGetLoadFileList()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderLoadCiv);
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdLoadCivFileListBox);
    return dynamic_cast<UIFileListBox*>(theControl);
}

// FUNCTION: EXE 0x67f92a
// Moves an upgrade (the selected line when 0) to the bought list or back, then reformats the lists
// (on a buy only the source list is reformatted)
bool CivBuilderMoveUpgrade(UIForm* inForm, bool inBuy, EECivUpgrade inUpgrade)
{
    UWideString theLine;
    UWideString theName;
    bool theResult = false;
    unsigned long theFromID;
    unsigned long theToID;
    unsigned long theIndex;
    long theFound;
    if (inBuy) {
        theFromID = 0xcb;
        theToID = 0xcc;
    } else {
        theFromID = 0xcc;
        theToID = 0xcb;
    }
    EECBUIListBox* theFrom = CivBuilderGetUpgradeList(inForm, theFromID);
    EECBUIListBox* theTo = CivBuilderGetUpgradeList(inForm, theToID);
    if (theFrom != NULL && theTo != NULL && theFrom != theTo) {
        if (inUpgrade == 0)
            theFound = theFrom->GetCurrentSelection(theIndex);
        else
            theFound = theFrom->FindUpgrade(inUpgrade, &theIndex);
        if (theFound == 0) {
            theLine = theFrom->GetData(theIndex);
            if (EECBUIListBox::GetUpgradeName(theLine, theName)) {
                theFrom->DeleteLine(theIndex);
                theFrom->ResetSelection();
                theTo->AddLine(theLine);
            }
        }
        if (inBuy) {
            theFrom->ReformatUpgrades(inBuy);
        } else {
            theFrom->ReformatUpgrades(false);
            theTo->ReformatUpgrades(true);
        }
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67fa40
// Handles the civilization timer: unless a builder form is up, opens the predefined-civilization selection once
// (locking the choice), or the builder when custom civilizations are allowed
long EEProcessTimerExpired()
{
    UIForm* theBuilder = gUIManager->GetFormByID(kFormCivBuilder);
    UIForm* theForm = gUIManager->GetFormByID(kFormPredefinedCiv);
    long theResult = 0;
    if (gUIManager->GetCurrentForm() != theBuilder && gUIManager->GetCurrentForm() != theForm) {
        if (!gAllowCustomCivs) {
            if (!::gWorld.GetCurrentPlayer()->mCivilizationChosen) {
                EESetCivBuilderButtonBlinking(false);
                ::gWorld.GetCurrentPlayer()->mCivilizationChosen = true;
                ::gWorld.GetCurrentPlayer()->mCivilizationLocked = true;
                UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_PDCS_OPEN);
                theForm->SetFormOpenEvent(theEvent);
                theEvent->Dereference();
                theEvent = EECreateScreenEvent(EE_SCREENS_PDCS_CLOSE);
                theForm->SetFormCloseEvent(theEvent);
                theEvent->Dereference();
                gUIManager->Push(theForm);
            }
        } else {
            theResult = CivBuilderEditCivilization();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67fb5f
// Clears the selection of the given upgrade list; returns 3 when it is missing
long CivBuilderResetUpgradeSelection(UIForm* inForm, unsigned long inControl)
{
    EECBUIListBox* theList = CivBuilderGetUpgradeList(inForm, inControl);
    if (theList != NULL) {
        theList->ResetSelection();
        return 0;
    }
    return 3;
}

// FUNCTION: EXE 0x67fb83
// Clears the available upgrade list's (0xcb) selection
long CivBuilderResetAvailableSelection(UIForm* inForm)
{
    return CivBuilderResetUpgradeSelection(inForm, 0xcb);
}

// FUNCTION: EXE 0x67fb94
// Clears the bought upgrade list's (0xcc) selection
long CivBuilderResetBoughtSelection(UIForm* inForm)
{
    return CivBuilderResetUpgradeSelection(inForm, 0xcc);
}

// FUNCTION: EXE 0x67fba5
// Moves every upgrade in the bought list (0xcc) back to the available list; always returns true
bool CivBuilderReturnAllUpgrades()
{
    std::vector<UWideString> theLines;
    EECivUpgrade theUpgrade = 0;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    theLines = CivBuilderGetUpgradeList(theForm, 0xcc)->GetDataArray();
    for (std::vector<UWideString>::iterator i = theLines.begin(); i != theLines.end(); i++) {
        if (EECBUIListBox::IsUpgradeLine(*i) && EECBUIListBox::GetUpgrade(*i, &theUpgrade))
            CivBuilderMoveUpgrade(theForm, false, theUpgrade);
    }
    return true;
}

// FUNCTION: EXE 0x67fc30
// Copies the save dialog's selected file name into its edit box
long CivBuilderSaveNameSelected()
{
    UWideString theText;
    unsigned long theIndex = 0;
    long theResult = 3;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilderSaveCiv);
    UIControl* theControl = theForm->GetControlById(kCivBuilderIdSaveCivFileListBox);
    UIFileListBox* theList = dynamic_cast<UIFileListBox*>(theControl);
    if (theList != NULL) {
        theResult = theList->GetCurrentSelection(theIndex);
        if (theResult == 0) {
            theText = theList->GetData(theIndex);
            theControl = theForm->GetControlById(kCivBuilderIdSaveCivNameTextField);
            UIEditBox* theEdit = dynamic_cast<UIEditBox*>(theControl);
            theEdit->SetText(theText);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67fcf0
// Starts or stops the blinking of the main game's civilization builder button and its large alert
long EESetCivBuilderButtonBlinking(bool inBlinking)
{
    gUIManager->mMainGameForm->mIsButtonBlinking = inBlinking;
    UIBlinkingButton* theButton = CivBuilderGetMainGameButton();
    if (theButton == NULL)
        return 0;
    theButton->SetIsBlinking(inBlinking);
    gL5654B8 = 0;
    UIBlinkingButton* theOther = static_cast<UIBlinkingButton*>(gUIManager->mMainGameForm->GetControl(kMainGameLargeCivBuilderAlert));
    if (theOther == NULL)
        return 0;
    if (inBlinking)
        theOther->SetDisplayMode(kDisplayModeShown);
    else
        theOther->SetDisplayMode(kDisplayModeHidden);
    theOther->SetIsBlinking(inBlinking);
    return 0;
}

// FUNCTION: EXE 0x67fd5a
// Buys an upgrade, playing the button sound on success; returns 0xd when it cannot be moved
long CivBuilderMoveToBought(EECivUpgrade inUpgrade)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    long theResult = 0xd;
    if (CivBuilderMoveUpgrade(theForm, true, inUpgrade)) {
        theResult = 0;
        gSoundManager->Play(gU5654E4, 1.0f, 0.0f, 1.0f, false, NULL);
    }
    return theResult;
}

// FUNCTION: EXE 0x67fdab
// Sells an upgrade back, playing the button sound on success; returns 0xd when it cannot be moved
long CivBuilderMoveToAvailable(EECivUpgrade inUpgrade)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    long theResult = 0xd;
    if (CivBuilderMoveUpgrade(theForm, false, inUpgrade)) {
        gSoundManager->Play(gU5654E4, 1.0f, 0.0f, 1.0f, false, NULL);
        theResult = 0;
    }
    return theResult;
}
