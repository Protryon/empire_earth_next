// C:\EEX\Empire Earth\UI Events\EEUIECampaignSelectScreenEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIECSSE). The campaign scenario selection form's (0x21) events: 0x1d1766..0x1d2b02 (the CE unit ends at 0x1d1766,
// EEUIEChatEvents.cpp starts at 0x1d2b02). One unit: FormOpen, its ??_G block, then the file-static helpers in
// definition order (the map/scenario-list refresh 0x1d19e8 is followed by EEUIBMapBackground's ??_G, its first user,
// and by its own helpers), the ProcessEvents in vtable order, the file-scope "default" UString (chain 0x1d2ab3) and
// EEUISCIFormManager::SetFile's COMDAT. The unit map's "EEUIBMapBackground.cpp" split at 0x1d19e8 is not a unit: the
// class's ctor is inline (expanded at three new sites in three units) and its virtuals live at 0x2556d9/0x2556e6.
// The folded `gUserInterface->CloseForm(0)` callback is CCCE's 0x1d3e52. Unit flags: the exe's module set.
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
#include "lle/TSReadWriteLock.h"
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
#include "lle/UIEditBox.h"
#include "lle/UIComboBox.h"
#include "lle/UILayoutUtility.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/FSRestorable.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EECampaignProgress.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEUIECampaignSelectScreenEvents.h"
#include "ee/EEStringIDs.h"
static void RefreshCampaign(unsigned long inBuiltIn, bool inShowMap);
static void GetCompletedText(UWideString& outText);
static void SelectNextScenario(std::vector<UWideString>& inCompleted);
static void ShowDifficultyLevel();
static void CloseCampaignForm(UIForm* inForm);

// the difficulty level names (0x446194, .rdata)
static const long kDifficultyStrings[3] = { IDS_LOBBY_EASY, IDS_LOBBY_MEDIUM, IDS_LOBBY_HARD };

// FUNCTION: EXE 0x5d1766
// Opens the campaign scenario form: shows the difficulty controls, hooks up its events and lists the campaign
long EEUIECSSEFormOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormCampaignScenarioSelect);
    unsigned long theBuiltIn = EECampaignFormManager::GetBuiltInCampaign(EECampaignFormManager::Get()->mCampaignName);
    theForm->GetControl(kCampaignScenarioDifficultyComboBox)->SetDisplayMode(kDisplayModeShown);
    theForm->GetControl(kCampaignScenarioDifficultyLabel)->SetDisplayMode(kDisplayModeShown);
    UIEvent* theEvent = new EEUIECSSESelectCampaign;
    theForm->GetControl(kCampaignScenarioCustomCampaignSelectListBox)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECSSESelectScenario;
    theForm->GetControl(kCampaignScenarioSelectScenarioListBox)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECSSESelectDifficultyLevel;
    theForm->GetControl(kCampaignScenarioDifficultyComboBox)->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECSSEStartScenario;
    theForm->GetControl(kCampaignScenarioStartGameButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theEvent = new EEUIECSSEBack;
    theForm->GetControl(kCampaignScenarioBackButton)->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    RefreshCampaign(theBuiltIn, true);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d1911
// Destroys the select-campaign event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d193c
// Destroys the select-scenario event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d1967
// Destroys the select-difficulty event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d1992
// Destroys the start-scenario event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d19bd
// Destroys the back event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5d19e8
// Loads the current campaign and lists its open scenarios (completed ones marked), the progress text and the map
static void RefreshCampaign(unsigned long inBuiltIn, bool inShowMap)
{
    EECampaignProgress theProgress;
    std::vector<UWideString> theCompleted;
    std::vector<UWideString> theScenarios;
    FSArchive theArchive;
    UWideString theSuffix;
    UWideString theFileName;
    UWideString theMapName;
    UWideString theText;
    UWideString theString;
    UWideString theError;
    UString theMapFile;
    GERectangle theRectangle;
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    UILayoutUtility theLayout;
    FSFileSpec* theArchiveFile = NULL;
    FSFileSpec* theMapSpec = NULL;
    bool theValid = true;
    long theResult;
    long i;
    std::vector<UWideString>::iterator theFound;
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kCampaignScenarioCampaignProgressLabel));
    EEShowSinglePlayerScreen();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kCampaignScenarioSelectScenarioListBox);
    theList->DeleteAllEntries();
    UIComboBox* theLevels = static_cast<UIComboBox*>(theForm->GetControl(kCampaignScenarioDifficultyComboBox));
    theLevels->DeleteAllEntries();
    for (i = 0; i < 3; i++) {
        gResource->GetString(kDifficultyStrings[i], theString);
        theLevels->AppendLine(theString);
    }
    UIBackground* theBackground = theForm->GetBackgroundByID(-2);
    if (theBackground)
        theForm->RemoveBackground(theBackground);
    theFileName = "Campaigns\\" + theManager->mCampaignName + gCampaignExtension;
    if (theManager->mCampaign.Load(theFileName, theError, false)) {
        if (inBuiltIn != 0) {
            theManager->UpdateChecksum();
            if (inBuiltIn != 1 && inBuiltIn != theManager->mChecksum)
                theValid = false;
        }
        theResult = gDataPath->FindFile(UString(theFileName), theArchiveFile);
        if (theResult == kErrorOK && theValid) {
            theArchive.SetArchive(*theArchiveFile);
            gDataPath->AddArchive(theArchive, true);
            EESetDataPaths();
            if (inShowMap) {
                theMapName = theManager->mCampaign.GetBackgroundImage();
                theMapFile = UString("Scenarios\\" + gU52A280 + theMapName);
                if (gDataPath->FindFile(theMapFile, theMapSpec) == kErrorOK) {
                    theForm->LockDrawBackground();
                    UIBackground* theMap = new EEUIBMapBackground(-2, theMapFile, theDescription, theRectangle,
                                                                  theLayout, theForm, 2, 0, 0, true);
                    theMap->Open();
                    theForm->UnlockDrawBackground();
                    if (theMapSpec)
                        delete theMapSpec;
                }
            }
            theScenarios = theManager->mCampaign.GetScenarios();
            theCompleted.clear();
            theManager->UpdateChecksum();
            theResult = theProgress.Load(UWideString(gCampaignDefaultUser), theManager->mCampaignName,
                                         theManager->GetChecksum(), false);
            if (theResult == kErrorOK)
                theProgress.GetCompletedScenarios(theCompleted);
            GetCompletedText(theSuffix);
            if (!theManager->mAllScenariosOpen) {
                for (i = 0; i <= theCompleted.size(); i++) {
                    if (i >= theScenarios.size())
                        continue;
                    if (i < theCompleted.size()) {
                        theFound = std::find(theScenarios.begin(), theScenarios.end(), theCompleted[i]);
                        if (theFound != theScenarios.end())
                            theList->AppendLine(theScenarios[i] + theSuffix);
                    } else {
                        theList->AppendLine(theScenarios[i]);
                        break;
                    }
                }
                if (theCompleted.size() != 0) {
                    if (theCompleted.size() == theScenarios.size())
                        gResource->GetString(IDS_CAMPAIGN_COMPLETED, theText);
                    else {
                        gResource->GetString(IDS_CAMPAIGN_IN_PROGRESS_OF_COMPLETED, theString);
                        sprintf(theText, theString, theCompleted.size(), theScenarios.size());
                    }
                    theLabel->SetDisplayMode(kDisplayModeShown);
                    theLabel->DeleteArray();
                    theLabel->AppendText(theText);
                } else
                    theLabel->SetDisplayMode(kDisplayModeHidden);
            } else {
                for (i = 0; i < theScenarios.size(); i++) {
                    theFound = std::find(theCompleted.begin(), theCompleted.end(), theScenarios[i]);
                    if (theFound == theCompleted.end())
                        theList->AppendLine(theScenarios[i]);
                    else
                        theList->AppendLine(theScenarios[i] + theSuffix);
                }
                theLabel->SetDisplayMode(kDisplayModeHidden);
            }
            SelectNextScenario(theCompleted);
            ShowDifficultyLevel();
        } else {
            gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, theError);
            theError = theError + "\n\"" + theFileName + "\"";
            gUIManager->DisplayMessage(theError, true, NULL, CloseCampaignForm, true);
        }
        if (theArchiveFile)
            delete theArchiveFile;
    } else {
        theError = theError + "\n\"" + theFileName + "\"";
        gUIManager->DisplayMessage(theError, true, NULL, CloseCampaignForm, true);
    }
}

// FUNCTION: EXE 0x5d2259
// Destroys the campaign map background and frees it when asked (scalar deleting destructor)
// (emitted after its first user)

// FUNCTION: EXE 0x5d227c
// Returns the suffix (a space and the completed-scenario string) that marks finished scenarios in the list
static void GetCompletedText(UWideString& outText)
{
    gResource->GetString(IDS_SCENARIO_COMPLETED, outText);
    outText = " " + outText;
}

// FUNCTION: EXE 0x5d22d6
// Selects the next scenario to play: the first one not completed when all are open, else the last listed one
static void SelectNextScenario(std::vector<UWideString>& inCompleted)
{
    UWideString theName;
    UWideString theSuffix;
    std::vector<UWideString>::iterator theFound;
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kCampaignScenarioSelectScenarioListBox);
    unsigned long theSelection = theList->GetLineCount();
    if (theManager->mAllScenariosOpen) {
        GetCompletedText(theSuffix);
        for (unsigned long i = 0; i < theList->GetLineCount(); i++) {
            theSelection = i + 1;
            theName = theList->GetData(i);
            theName = theName.GetIndexedSubstring(0, theName.Find(theSuffix, 0) - 1);
            theFound = std::find(inCompleted.begin(), inCompleted.end(), theName);
            if (theFound == inCompleted.end())
                break;
        }
    }
    theList->ChangeSelectionIndex(theSelection ? theSelection - 1 : 0, true, false);
}

// FUNCTION: EXE 0x5d2421
// Shows the current difficulty level in the difficulty combo box
static void ShowDifficultyLevel()
{
    static_cast<UIComboBox*>(gUIManager->GetFormByID(kFormCampaignScenarioSelect)->GetControl(kCampaignScenarioDifficultyComboBox))->SetCurrentSelection(gDifficultyLevel);
}

// FUNCTION: EXE 0x5d2445
// Switches to the custom campaign selected in the list and lists its scenarios (no map)
long EEUIECSSESelectCampaign::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    unsigned long theSelection = 0;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kCampaignScenarioCustomCampaignSelectListBox);
    theList->GetCurrentSelection(theSelection);
    theName = theList->GetData(theSelection);
    theManager->SetCampaignName(theName);
    RefreshCampaign(0, false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5d24cc
// Shows the campaign description in the intro text for the selected scenario and remembers the scenario's name
long EEUIECSSESelectScenario::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEPSaveGameHeader theHeader;
    EESMMediaStrings theMediaStrings;
    UWideString theSuffix;
    UWideString theName;
    UWideString theText;
    FSFileSpec* theFile = NULL;
    unsigned long theSelection = 0;
    long theResult;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kCampaignScenarioSelectScenarioListBox);
    theList->GetCurrentSelection(theSelection);
    if (theList->GetLineCount() > 0) {
        GetCompletedText(theSuffix);
        theName = theList->GetData(theSelection);
        theName = theName.GetIndexedSubstring(0, theName.Find(theSuffix, 0) - 1);
        theResult = gDataPath->FindFile(UString("Scenarios\\" + gU52A280 + theName + gScenarioExtension), theFile);
        if (theResult == kErrorOK) {
            if (theHeader.LoadFromFile(*theFile) == kErrorOK) {
                theMediaStrings = theHeader.mMediaStrings;
                theText = theManager->mDescription;
                if (theText.GetLength() == 0)
                    gResource->GetString(IDS_NO_DESCRIPTION_PROVIDED, theText);
                UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kCampaignScenarioScenIntroTextMultiLineLabel));
                theLabel->DeleteArray();
                theLabel->AppendText(theText);
                theManager->mScenarioName = theName;
            }
        }
    }
    if (theFile)
        delete theFile;
    return kErrorOK;
}

// FUNCTION: EXE 0x5d26ef
// Sets the campaign, game and option difficulty to the level picked in the combo box
long EEUIECSSESelectDifficultyLevel::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    unsigned long theLevel;
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    static_cast<UIComboBox*>(theForm->GetControl(kCampaignScenarioDifficultyComboBox))->GetCurrentSelection(theLevel);
    theManager->mDifficultyLevel = theLevel;
    gDifficultyLevel = theLevel;
    gDifficultyLevel2 = theLevel;
    gGameOptions.mLock->WriteLock();
    gGameOptions.mDifficulty = theLevel;
    gGameOptions.mLock->WriteUnlock();
    return kErrorOK;
}

// FUNCTION: EXE 0x5d274a
// Loads the selected scenario's header and opens the scenario intro form for it; shows an error if it cannot
long EEUIECSSEStartScenario::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEPSaveGameHeader theHeader;
    EESMMediaStrings theMediaStrings;
    UWideString theSuffix;
    UWideString theName;
    UWideString theError;
    FSFileSpec* theFile = NULL;
    unsigned long theSelection = 0;
    long theResult;
    UIForm* theForm = inForm->GetManager()->GetFormByID(kFormCampaignScenarioSelect);
    UIForm* theInfoForm = inForm->GetManager()->GetFormByID(kFormScenarioIntro);
    EECampaignFormManager* theManager = EECampaignFormManager::Get();
    EEUISCIFormManager* theInfo = EEUISCIFormManager::Get();
    UIListBox<UWideString>* theList = (UIListBox<UWideString>*)theForm->GetControl(kCampaignScenarioSelectScenarioListBox);
    theList->GetCurrentSelection(theSelection);
    if (theList->GetLineCount() > 0) {
        GetCompletedText(theSuffix);
        theName = theList->GetData(theSelection);
        theName = theName.GetIndexedSubstring(0, theName.Find(theSuffix, 0) - 1);
        theResult = gDataPath->FindFile(UString("Scenarios\\" + gU52A280 + theName + gScenarioExtension), theFile);
        if (theResult == kErrorOK) {
            if (theHeader.LoadFromFile(*theFile) == kErrorOK) {
                theMediaStrings = theHeader.mMediaStrings;
                theMediaStrings.mSoundFile = gU52A280 + theMediaStrings.mSoundFile;
                theMediaStrings.mMapFile = gU52A280 + theMediaStrings.mMapFile;
                theManager->UpdateChecksum();
                theInfo->mMediaStrings = theMediaStrings;
                theInfo->SetFile(theFile);
                theInfo->mIsCampaign = true;
                gUserInterface->OpenForm(theInfoForm);
            } else {
                delete theFile;     // not handed to the intro form: freed here (it leaked)
                gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, theError);
                theError = theError + "\n\"" + theName + "\"";
                gUIManager->DisplayMessage(theError, true, NULL, CloseCampaignForm, true);
            }
        } else {
            gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, theError);
            theError = theError + "\n\"" + theName + "\"";
            gUIManager->DisplayMessage(theError, true, NULL, CloseCampaignForm, true);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5d2aa1
// Closes the campaign scenario form
long EEUIECSSEBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->CloseForm(NULL);
    return kErrorOK;
}

long EEUIECSSEFormClose::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return kErrorOK;
}

// (ICF-folded into CCCE's OnNoCustomCampaigns, 0x1d3e52)
static void CloseCampaignForm(UIForm* inForm)
{
    gUserInterface->CloseForm(NULL);
}

// SYNTHETIC: EXE 0x5d2ab3
// SYNTHETIC: EXE 0x5d2abd
// SYNTHETIC: EXE 0x5d2ace
// SYNTHETIC: EXE 0x5d2ada
UString gCampaignDefaultUser("default");

// FUNCTION: EXE 0x5d2ae5
// Replaces the intro form's scenario file, deleting the old one (EEUISCIFormManager::SetFile)
// (the out-of-line COMDAT of StartScenario's call)
