// C:\EEX\Empire Earth\UI Events\EEUIEPDCSEvents.cpp (file name inferred: no __FILE__; class prefix EEUIEPDCS). The
// civilization choice form's events (form 0x23: Cancel/Ok buttons, the civilization list 3, its statistics list 4,
// a label 5): 0x1e5e2e..0x1e6711 (EEUIEPostGameSummaryEvents.cpp ends at 0x1e5e2e; EEUIECameraEvents.cpp starts at
// 0x1e6711). FormOpen first, then the ??_G block of the events it creates in declaration order (ClearCivStats' in-class
// ProcessEvent before its ??_G), SelectCiv with the static file helper and EEUIEPDCSLoadCiv's out-of-line ctor,
// in-class ProcessEvent and ??_G after it, Ok with EEMCivUpgrades' out-of-line ctor, and the template COMDATs
// (vector<unsigned short> copy ctor, vector<EECivilizationEntry>::_Buy). Unit flags: the exe's module set.
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
#include "ee/EEUIEPDCSEvents.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x5e5e2e
// Sets up the civilization choice form: hooks up Cancel, Ok (also Enter and double click), selection and clearing,
// fills the civilization list, shows the selected one's statistics and pauses the game
long EEUIEPDCSFormOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EECBUIListBox* theCivs = static_cast<EECBUIListBox*>(inForm->GetControl(kPredefinedCivPredefinedListBox));
    EECBUIListBox* theStats = static_cast<EECBUIListBox*>(inForm->GetControl(kPredefinedCivStatsListBox));
    UIEvent* theEvent = new EEUIEPDCSCancel;
    inForm->GetControl(kPredefinedCivCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEPDCSOk;
    inForm->GetControl(kPredefinedCivOKButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Reference();
    theEvent->Reference();
    theEvent->Reference();
    inForm->AddHotKey(DIK_RETURN, kKeyNone, kKeyNone, kKeyNone, theEvent, NULL, NULL, NULL, 0, false);
    inForm->AddHotKey(DIK_NUMPADENTER, kKeyNone, kKeyNone, kKeyNone, theEvent, NULL, NULL, NULL, 0, false);
    theCivs->SetTriggerEvent(kControlTriggerDoubleClicked, theEvent);
    theEvent->Dereference();
    theEvent->Dereference();
    theEvent = new EEUIEPDCSSelectCiv;
    theCivs->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEPDCSClearCivStats;
    theCivs->SetTriggerEvent(kControlTriggerButtonDown, theEvent);
    theEvent->Dereference();
    theCivs->SetSortLines(false);
    theCivs->SetAutoSelect(false);
    theCivs->mC13C = GEColor(0.0f, 0.0f, 0.39f);
    theStats->SetAutoSelect(false);
    theStats->mShowPoints = false;
    theStats->mSelectUpgradesOnly = false;
    theStats->SetSortLines(true);
    theStats->mC13C = GEColor(0.18f, 0.18f, 0.28f);
    gCivilizationManager.FillCivilizationList(theCivs);
    inForm->SetTabStop(3);
    if (theCivs->GetHasSelection() == true)
        theCivs->GetTriggerEvent(kControlTriggerSelectionChanged)->ProcessEvent(inForm, theCivs);
    gWorld.PauseGame();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e6021
// Destroys the select-civilization event and frees it when asked (scalar deleting destructor)
// (EEUIEPDCSSelectCiv's ??_G, then Ok's and Cancel's, after their first user)

// FUNCTION: EXE 0x5e604c
// Destroys the Ok event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e6077
// Destroys the Cancel event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e60a2
// Clears the statistics list when no civilization (or the first line) is selected
// (EEUIEPDCSClearCivStats' in-class ProcessEvent, then its ??_G)

// FUNCTION: EXE 0x5e60e7
// Destroys the clear-statistics event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e61db
// Finds the file of the civilization selected in the list by its name; returns 3 when there is none
// the selected line of the civilization list is "category/name/..."; the file of the civilization with that name.
// Defined before SelectCiv: VC6 emits a static after its first user (0x5e61db follows SelectCiv, before LoadCiv's
// ctor); a forward declaration with the body after SelectCiv puts it after LoadCiv's ??_G instead.
static long GetSelectedCivilizationFile(UWideString& outFile)
{
    std::vector<EECivilizationEntry> theCivs;
    std::vector<UWideString> theTokens;
    UWideString theName;
    UWideString theCivName;
    UWideString theLine;
    UWideString theUnused;
    UWideString theFile("");
    unsigned long theSelection;
    unsigned long i;
    EECBUIListBox* theList = static_cast<EECBUIListBox*>(gUIManager->GetFormByID(kFormPredefinedCiv)->GetControl(kPredefinedCivPredefinedListBox));
    long theResult = 3;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK) {
        theLine = theList->GetData(theSelection);
        theLine.Tokenize(EECBUIListBox::sSeparator, theTokens);
        if (theTokens.size() > 2) {
            theName = theTokens[1];
            gCivilizationManager.GetCivilizations(theCivs);
            for (i = 0; i < theCivs.size(); i++) {
                gResource->GetString(theCivs[i].mNameID, theCivName);
                if (theCivName == theName) {
                    theFile = theCivs[i].GetFileName();
                    break;
                }
            }
            if (theFile.GetLength() > 0) {
                outFile = theFile;
                theResult = 0;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5e610c
// Clears the statistics and posts loading the selected civilization, showing Please wait
long EEUIEPDCSSelectCiv::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theFile;
    UWideString theMessage;
    EECBUIListBox* theStats = static_cast<EECBUIListBox*>(inForm->GetControl(4));
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(5));
    theStats->DeleteAllEntries();
    if (GetSelectedCivilizationFile(theFile) == kErrorOK) {
        EEUIEPDCSLoadCiv* theEvent = new EEUIEPDCSLoadCiv;
        theEvent->mFile = UString(theFile);
        gResource->GetString(IDS_PLEASE_WAIT, theMessage);
        theLabel->SetText(theMessage);
        theLabel->SetDisplayMode(kDisplayModeShown);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e639a
// Creates the load-civilization event with no file
// (EEUIEPDCSLoadCiv's implicit ctor, its in-class ProcessEvent and its ??_G, after their first user)

// FUNCTION: EXE 0x5e63c9
// Loads the selected civilization file and shows its upgrades in the statistics list, or deselects it and shows the
// load error

// FUNCTION: EXE 0x5e64bf
// Destroys the load-civilization event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e64ed
// Requests the selected civilization's upgrades for the current player (through the host in multiplayer) and closes
// the form, or shows why not
long EEUIEPDCSOk::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    std::vector<unsigned short> theUpgrades;
    UWideString theFile;
    UWideString theMessage;
    UWideString theName;
    long theResult;
    if (GetSelectedCivilizationFile(theFile) == kErrorOK) {
        theResult = EELoadCivilization(UString(theFile), EEGetCivilizationDirectory(false), theName, theUpgrades);
        if (theResult == kErrorOK) {
            NEMessage* theRequest = new EEMCivUpgradesRequested(gWorld.mCurrentPlayer, theUpgrades, theName, 0);
            if (gCommunications->GetIsMultiplayerSession())
                gCommunications->ProcessLocalUserInput(theRequest, kSlotServer);
            else {
                theRequest->Execute();
                theRequest->Dereference();
            }
            gUserInterface->CloseForm(NULL);
        } else {
            gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, theMessage);
            gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
        }
    } else {
        gResource->GetString(IDS_PLEASE_SELECT_CIVILIZATION_FROM_LIST, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e6635
// Creates the message that requests a player's civilization upgrades
// (EEMCivUpgrades' ctor, out of line after its first user; then the template COMDATs)

// FUNCTION: EXE 0x5e6694
// Copies a vector of upgrade ids (vector copy constructor)

// FUNCTION: EXE 0x5e66da
// Allocates room for a number of civilization entries (vector _Buy)

// ICF-FOLDED: EXE 0x67cf38
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEUIEPDCSFormClose::ProcessEvent(class UIForm *,class UIEntity *)
{
    return 0;
}
