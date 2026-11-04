// C:\EEX\Empire Earth\UI Events\EEUIEChooseCustomCampaignEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIECCCE). The custom campaign form's events: 0x1d3a77..0x1d4165. Cancel's ProcessEvent is identical to
// EEUIECSSEBack's and folded into it (0x1d2aa1), FormClose's into the shared `return 0` (0x27cf38). Unit flags: the
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
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEUIECampaignSelectScreenEvents.h"
#include "ee/EEUIEChooseCustomCampaignEvents.h"
#include "ee/EEStringIDs.h"
static void OnNoCustomCampaigns(UIForm* inForm);

// FUNCTION: EXE 0x5d3a77
// Lists the campaign files in data\Campaigns that are not built-in campaigns and selects the first (none: a message
// that closes the form); wires the buttons. The search path it allocates is never freed
long EEUIECCCEFormOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    std::set<UString> theFiles;
    std::set<UString>::iterator theIterator = 0;
    UWideString theFileName;
    UWideString theMessage;
    UWideString theExtension;
    UString thePattern;
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormChooseCustomCampaign);
    UIEvent* theEvent = new EEUIECCCESelectCampaign;
    theForm->GetControl(kChooseCampaignCampListBox)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECCCEOk;
    theForm->GetControl(kChooseCampaignOKButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECCCECancel;
    theForm->GetControl(kChooseCampaignCancelButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kChooseCampaignCampListBox);
    theList->DeleteAllEntries();
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
    thePath->AddDirectory(theDirectory, true);
    thePattern = UString("*") + gCampaignExtension;
    thePath->ListFiles(thePattern, theFiles);
    thePath->Dereference();     // the search path is released after the listing (it leaked)
    for (theIterator = theFiles.begin(); theIterator != theFiles.end(); theIterator++) {
        theFileName = *theIterator;
        if (theFileName.GetLength() > 4) {
            theExtension = theFileName.GetIndexedSubstring(theFileName.GetLength() - 4, theFileName.GetLength() - 1);
            _mbslwr((unsigned char*)(const char*)theExtension);
            if (theExtension == gCampaignExtension)
                theFileName = theFileName.GetIndexedSubstring(0, theFileName.GetLength() - 5);
        }
        if (EECampaignFormManager::GetBuiltInCampaign(theFileName) <= 0)
            theList->AppendLine(theFileName);
    }
    if (theList->GetLineCount() == 0) {
        gResource->GetString(IDS_NO_CUSTOM_CAMPAIGNS_COULD_BE_FOUND, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, OnNoCustomCampaigns, true);
    } else
        theList->ChangeSelectionIndex(0, true, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d3dd1
// Destroys the select-campaign event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIECCCESelectCampaign, then Ok's and Cancel's)

// FUNCTION: EXE 0x5d3dfc
// Destroys the custom campaign OK event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d3e27
// Destroys the custom campaign Cancel event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d3e52
// Closes the custom campaign form after the no-campaigns message
static void OnNoCustomCampaigns(UIForm* inForm)
{
    gUserInterface->CloseForm(NULL);
}

// FUNCTION: EXE 0x5d3e60
// Loads the selected custom campaign and shows its description (or the load error); stores its name, first scenario
// and description in the campaign form manager (empty on failure)
long EEUIECCCESelectCampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theError;
    UWideString theText;
    UWideString theFile;
    UWideString theName;
    unsigned long theSelection = 0;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormChooseCustomCampaign);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kChooseCampaignCampListBox);
    theList->GetCurrentSelection(theSelection);
    UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kChooseCampaignDescripTextBox));
    theName = theList->GetData(theSelection);
    theFile = "Campaigns\\" + theName + gCampaignExtension;
    if (theManager->mCampaign.Load(theFile, theError, false)) {
        theText = theManager->mCampaign.GetDescription();
        theManager->SetCampaignName(theName);
        theManager->SetFirstScenario(theManager->mCampaign.GetRequiredCiv());
        theManager->SetDescription(theManager->mCampaign.GetDescription());
    } else {
        theText = theError;
        theManager->SetCampaignName("");
        theManager->SetFirstScenario("");
        theManager->SetDescription("");
    }
    theLabel->DeleteArray();
    theLabel->AppendText(theText);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d403a
// Opens the chosen campaign's scenario selection (unlocked scenarios only), closing this form behind a wait message
long EEUIECCCEOk::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager::Get()->mAllScenariosOpen = false;
    theForm->SetFormOpenEvent(new EEUIECSSEFormOpen);
    theForm->SetFormCloseEvent(new EEUIECSSEFormClose);
    if (gResource->GetString(IDS_PLEASE_WAIT, theMessage) == kErrorOK) {
        gUserInterface->CloseForm(NULL);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, false);
    }
    gUserInterface->OpenForm(theForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d410f
// Destroys the scenario-select form-open event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIECSSEFormOpen, then FormClose's)

// FUNCTION: EXE 0x5d413a
// Destroys the scenario-select form-close event and frees it when asked (scalar deleting destructor)

long EEUIECCCECancel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->CloseForm(NULL);
    return kErrorOK;
}

long EEUIECCCEFormClose::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return kErrorOK;
}
