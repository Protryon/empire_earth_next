// C:\EEX\Empire Earth\Civilization Builder\EECBUIEvents.cpp (file name inferred: no __FILE__)
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
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIFileListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EECBCivilizationCreator.h"
#include "ee/EECivilization.h"
#include "ee/EEGameOptions.h"
#include "ee/EEAction.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EECivilizationBuilder.h"

#include "ee/EEUIECivilizationBuilderEvents.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEScenarioEditorIDs.h"

// whether the next accepted name change is the first one (initialized data 0x4cfa60 = 1; only this unit uses it)
static bool sFirstChange = true;

// FUNCTION: EXE 0x67cd62
// Opens the change-name dialog
long EEUIECBShowChangeNameDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderShowNameDialog();
}

// FUNCTION: EXE 0x67cd6a
// Accepts the typed civilization name, then applies the edits and closes the builder; after an unchanged
// civilization's save (first-change flag cleared) it saves the file instead and sets the flag again
long EEUIECBAcceptChangeName::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    long theResult = CivBuilderGetTypedName(theName);
    if (theResult != kErrorOK)
        return theResult;
    CivBuilderSetCivilizationName(theName);
    theResult = CivBuilderAcceptName();
    if (theResult != kErrorOK)
        return theResult;
    if (sFirstChange)
        theResult = CivBuilderClose(true);
    else {
        theResult = CivBuilderSaveAs();
        sFirstChange = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67cdd1
// Opens the load-civilization dialog, or tells the player the civilization can no longer be changed
long EEUIECBShowLoadCivDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderShowLoad();
}

// FUNCTION: EXE 0x67cdd9
// Loads the civilization file selected in the load dialog, shows its name and closes the dialog
long EEUIECBAcceptLoadCivilization::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theName;
    long theResult;
    UIFileListBox* theList = CivBuilderGetLoadFileList();
    if (theList != NULL) {
        unsigned long theSelection;
        theResult = theList->GetCurrentSelection(theSelection);
        if (theResult == kErrorOK) {
            theName = (const char*)theList->GetData(theSelection);
            theResult = CivBuilderLoadCivilization(theName);
            if (theResult == kErrorOK) {
                theResult = CivBuilderSetName(*gEditableCivilization->GetName());
                if (theResult == kErrorOK)
                    theResult = CivBuilderCloseForm();
            }
        }
    } else
        theResult = kErrorNotFound;
    return theResult;
}

// FUNCTION: EXE 0x67ce68
// Opens the save-civilization dialog
long EEUIECBShowSaveCivDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderShowSave();
}

// FUNCTION: EXE 0x67ce70
// Closes the current dialog (also the cancel of the load and change-name dialogs, ICF-folded here)
long EEUIECBCancelSaveCivDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderCloseForm();
}

// the other two cancel events: same body, ICF-folded into 0x27ce70 (no marker)
long EEUIECBCancelLoadCivDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderCloseForm();
}

long EEUIECBCancelChangeName::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderCloseForm();
}

// FUNCTION: EXE 0x67ce78
// Saves the civilization; an unchanged one first takes the typed file name as its own name (clearing the
// first-change flag)
long EEUIECBAcceptSaveCivilization::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    long theResult;
    if (gEditableCivilization->GetIsUnchanged()) {
        theResult = CivBuilderGetTypedFileName(theName);
        if (theResult == kErrorOK) {
            CivBuilderSetCivilizationName(theName);
            theResult = CivBuilderSetName(theName);
            sFirstChange = false;
            if (theResult == kErrorOK)
                theResult = CivBuilderSaveAs();
        }
    } else
        theResult = CivBuilderSaveAs();
    return theResult;
}

// FUNCTION: EXE 0x67cedf
// Copies the save list's selected file name into the save dialog's edit box
long EEUIECBSaveNameSelected::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderSaveNameSelected();
}

// FUNCTION: EXE 0x67cee7
// On a bought upgrade's selection, clears the available list's selection (while the civilization may change)
long EEUIECBPurchasedUpgradeSelected::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    if (CivBuilderCanChange())
        theResult = CivBuilderResetAvailableSelection(inForm);
    return theResult;
}

// FUNCTION: EXE 0x67cf05
// On an available upgrade's selection, clears the bought list's selection
long EEUIECBPotentialUpgradeSelected::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderResetBoughtSelection(inForm);
}

// FUNCTION: EXE 0x67cf12
// Buys the selected available upgrade unless the lists are locked
long EEUIECBUpgradePurchaseAttempt::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    if (!gB565566)
        theResult = CivBuilderBuyUpgrade(0);
    return theResult;
}

// FUNCTION: EXE 0x67cf26
// Sells the selected bought upgrade back unless the lists are locked
long EEUIECBUpgradeReturned::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    if (!gB565566)
        theResult = CivBuilderReturnUpgrade();
    return theResult;
}

// Does nothing (0x27cf38: `return 0`, ICF-shared by 19 vtables; the first copy is this unit's)
long EEUIECBUpdateControls::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return kErrorOK;
}

// FUNCTION: EXE 0x67cf3d
// Discards the civilization edits
long EEUIECBCancelChanges::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return CivBuilderClose(false);
}

// FUNCTION: EXE 0x67cf48
// Accepts the edits; in multiplayer with points left over it asks for confirmation first
long EEUIECBAcceptChanges::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    bool theFlag = CivBuilderCanChange();
    if (gCommunications->GetIsMultiplayerSession() && theFlag && gEditableCivilization->GetPointsLeft() > 0)
        theResult = CivBuilderAskAccept(inForm);
    else
        CivBuilderAcceptChanges(NULL);
    return theResult;
}

// FUNCTION: EXE 0x67cf8b
// Asks to delete the civilization file selected in the load or save dialog
long EEUIECBDisplayDeleteCivDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    UIFileListBox* theList;
    if (inForm->GetFormID() == kFormCivBuilderLoadCiv)
        theList = CivBuilderGetLoadFileList();
    else
        theList = static_cast<UIFileListBox*>(inForm->GetControlById(kCivBuilderIdSaveCivFileListBox));
    if (theList != NULL) {
        unsigned long theSelection;
        if (theList->GetCurrentSelection(theSelection) == kErrorOK)
            theResult = CivBuilderAskDelete(inForm, inEntity);
    }
    return theResult;
}

// The ICF-shared copy LINK kept in this unit's range
// SYNTHETIC: EXE 0x67cf38
