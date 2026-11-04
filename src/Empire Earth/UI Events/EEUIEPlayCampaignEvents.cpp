// C:\EEX\Empire Earth\UI Events\EEUIEPlayCampaignEvents.cpp (file name inferred: no __FILE__; class prefix EEUIEPCE).
// The campaign choice form's events: 0x1e2292..0x1e295f (the init chains of the ten file-static description and
// first-scenario strings first). Unit flags: the exe's module set.
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
#include "ee/EEUIECampaignSelectScreenEvents.h"
#include "ee/EEUIEChooseCustomCampaignEvents.h"
#include "ee/EEUIEPlayCampaignEvents.h"
#include "ee/EEStringIDs.h"
static UWideString sRomanDescription;        // 0x5630c8
static UWideString sPacificDescription;      // 0x5630b0
static UWideString sAsianDescription;        // 0x563098
static UWideString sLearningDescription;     // 0x563080
static UWideString sDemoDescription;         // 0x563068 (unused)
static UWideString sRomanFirstScenario;      // 0x563050
static UWideString sPacificFirstScenario;    // 0x563038
static UWideString sAsianFirstScenario;      // 0x563020
static UWideString sLearningFirstScenario;   // 0x563008
static UWideString sDemoFirstScenario;       // 0x562ff0 (unused)

static void StartCampaign(const UWideString& inName, const UWideString& inDescription,
                          const UWideString& inFirstScenario, bool inLearning);
static void LoadCampaignInfo(const UWideString& inName, UWideString& outDescription, UWideString& outFirstScenario);

// FUNCTION: EXE 0x5e244a
// Leaves the campaign choice form for the single player screen
long EEUIEPCEBackToSinglePlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetManager()->Pop();
    EEShowSinglePlayerScreen();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e2464
// Starts the Roman campaign
long EEUIEPCECampaign1::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    StartCampaign(gCampaignAOCRoman, sRomanDescription, sRomanFirstScenario, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e2482
// Opens a campaign's scenario choice form with its name, description and first scenario (all scenarios open for the
// learning campaign), showing Please wait while it opens
static void StartCampaign(const UWideString& inName, const UWideString& inDescription,
                          const UWideString& inFirstScenario, bool inLearning)
{
    UWideString theMessage;
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    theManager->SetCampaignName(inName);
    theManager->SetDescription(inDescription);
    theManager->mAllScenariosOpen = inLearning;
    theManager->SetFirstScenario(inFirstScenario);
    UIEvent* theEvent = new EEUIECSSEFormOpen;
    theForm->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECSSEFormClose;
    theForm->SetFormCloseEvent(theEvent);
    theEvent->Dereference();
    if (gResource->GetString(IDS_PLEASE_WAIT, theMessage) == 0) {
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, false);
        gUserInterface->OpenForm(theForm);
    } else
        gUserInterface->SwitchToForm(theForm);
}

// FUNCTION: EXE 0x5e2599
// Starts the Pacific campaign
long EEUIEPCECampaign2::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    StartCampaign(gCampaignAOCPacific, sPacificDescription, sPacificFirstScenario, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e25b7
// Starts the Asian campaign
long EEUIEPCECampaign3::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    StartCampaign(gCampaignAOCAsian, sAsianDescription, sAsianFirstScenario, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e25d5
// Shows the description of the campaign whose button the mouse is over
long EEUIEPCEMouseOver::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theDescription;
    UITextLabel* theLabel = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormPlayCampaign)->GetControl(kPlayCampaignCampaignIntroMultiLineText));
    switch (static_cast<UIControl*>(inEntity)->GetIndex()) {
    case 3:
        theDescription = sRomanDescription;
        break;
    case 4:
        theDescription = sPacificDescription;
        break;
    case 5:
        theDescription = sAsianDescription;
        break;
    }
    theLabel->DeleteArray();
    theLabel->AppendText(theDescription);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e264f
// Sets up the campaign choice form: loads the three campaigns' descriptions and hooks up the mouse-over descriptions
long EEUIEPCEFormOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    LoadCampaignInfo(gCampaignAOCRoman, sRomanDescription, sRomanFirstScenario);
    LoadCampaignInfo(gCampaignAOCPacific, sPacificDescription, sPacificFirstScenario);
    LoadCampaignInfo(gCampaignAOCAsian, sAsianDescription, sAsianFirstScenario);
    UIForm* theForm = gUIManager->GetFormByID(kFormPlayCampaign);
    UIEvent* theEvent = new EEUIEPCEMouseOver;
    theForm->GetControl(kPlayCampaignGreekCampaignIButton)->SetTriggerEvent(kControlTriggerMouseOver, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEPCEMouseOver;
    theForm->GetControl(kPlayCampaignBritishCampaignIIButton)->SetTriggerEvent(kControlTriggerMouseOver, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEPCEMouseOver;
    theForm->GetControl(kPlayCampaignGermanCampaignIIIButton)->SetTriggerEvent(kControlTriggerMouseOver, theEvent);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e2755
// Destroys the mouse-over event and frees it when asked (scalar deleting destructor)
// (EEUIEPCEMouseOver's ??_G)

// FUNCTION: EXE 0x5e2780
// Loads a campaign file's description and first scenario, or a no-description text when it cannot be loaded
static void LoadCampaignInfo(const UWideString& inName, UWideString& outDescription, UWideString& outFirstScenario)
{
    UWideString theFile;
    UWideString theText;
    UWideString theError;
    UIForm* theForm = gUIManager->GetFormByID(kFormPlayCampaign);
    EECampaign* theCampaign = &EECampaignFormManager::Get()->mCampaign;
    theFile = "Campaigns\\" + inName + gCampaignExtension;
    if (theCampaign->Load(theFile, theError, false)) {
        outDescription = theCampaign->GetDescription();
        outFirstScenario = theCampaign->GetRequiredCiv();
    } else
        gResource->GetString(IDS_NO_DESCRIPTION_PROVIDED, outDescription);
}

// FUNCTION: EXE 0x5e2866
// Switches to the custom campaign choice form, with its open and close events
long EEUIEPCECustomCampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormChooseCustomCampaign);
    // The form takes its own references: drop the creation references (both events leaked).
    UIEvent* theOpenEvent = new EEUIECCCEFormOpen;
    UIEvent* theCloseEvent = new EEUIECCCEFormClose;
    theForm->SetFormOpenEvent(theOpenEvent);
    theOpenEvent->Dereference();
    theForm->SetFormCloseEvent(theCloseEvent);
    theCloseEvent->Dereference();
    gUserInterface->SwitchToForm(theForm);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e28da
// Destroys the custom-campaign open event and frees it when asked (scalar deleting destructor)
// (EEUIECCCEFormOpen's ??_G)

// FUNCTION: EXE 0x5e2905
// Destroys the custom-campaign close event and frees it when asked (scalar deleting destructor)
// (EEUIECCCEFormClose's ??_G)

// FUNCTION: EXE 0x5e2930
// Loads and starts the learning campaign with all its scenarios open
long EEUIEPCELearningCampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    LoadCampaignInfo(gCampaignEELearning, sLearningDescription, sLearningFirstScenario);
    StartCampaign(gCampaignEELearning, sLearningDescription, sLearningFirstScenario, true);
    return kErrorOK;
}

// SYNTHETIC: EXE 0x5e2292
// SYNTHETIC: EXE 0x5e229c
// SYNTHETIC: EXE 0x5e22a7
// SYNTHETIC: EXE 0x5e22b3
// SYNTHETIC: EXE 0x5e22be
// SYNTHETIC: EXE 0x5e22c8
// SYNTHETIC: EXE 0x5e22d3
// SYNTHETIC: EXE 0x5e22df
// SYNTHETIC: EXE 0x5e22ea
// SYNTHETIC: EXE 0x5e22f4
// SYNTHETIC: EXE 0x5e22ff
// SYNTHETIC: EXE 0x5e230b
// SYNTHETIC: EXE 0x5e2316
// SYNTHETIC: EXE 0x5e2320
// SYNTHETIC: EXE 0x5e232b
// SYNTHETIC: EXE 0x5e2337
// SYNTHETIC: EXE 0x5e2342
// SYNTHETIC: EXE 0x5e234c
// SYNTHETIC: EXE 0x5e2357
// SYNTHETIC: EXE 0x5e2363
// SYNTHETIC: EXE 0x5e236e
// SYNTHETIC: EXE 0x5e2378
// SYNTHETIC: EXE 0x5e2383
// SYNTHETIC: EXE 0x5e238f
// SYNTHETIC: EXE 0x5e239a
// SYNTHETIC: EXE 0x5e23a4
// SYNTHETIC: EXE 0x5e23af
// SYNTHETIC: EXE 0x5e23bb
// SYNTHETIC: EXE 0x5e23c6
// SYNTHETIC: EXE 0x5e23d0
// SYNTHETIC: EXE 0x5e23db
// SYNTHETIC: EXE 0x5e23e7
// SYNTHETIC: EXE 0x5e23f2
// SYNTHETIC: EXE 0x5e23fc
// SYNTHETIC: EXE 0x5e2407
// SYNTHETIC: EXE 0x5e2413
// SYNTHETIC: EXE 0x5e241e
// SYNTHETIC: EXE 0x5e2428
// SYNTHETIC: EXE 0x5e2433
// SYNTHETIC: EXE 0x5e243f
