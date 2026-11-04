// C:\EEX\Empire Earth\UI Events\EEUIEScenarioIntroEvents.cpp (file name inferred: no __FILE__; class prefixes
// EEUIESIE, EEUISCIFormManager). The scenario introduction form's (0x13) events and its form manager:
// 0x1e71cc..0x1e853a (the save/load unit ends at 0x1e71cc; the scenario load unit starts at 0x1e853a). The form
// manager's members come first; its ??_G (0x1e83cb) sits in the ??_G block after the last event, so the manager is
// not a unit of its own (its only `new` is in Get). The ??_Gs of the classes this unit creates (Briefing, Movie,
// SoundOver, Start) follow in declaration order; deque<UIEventTrigger>::push_back (UIManager::AddEventToTrigger,
// SIE Open) closes the unit. Unit flags: the exe module set.
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
#include "lle/TSReadWriteLock.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SEMixer.h"
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIScrollBar.h"
#include "lle/UILayoutUtility.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/FSRestorable.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameSound.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEUIEScenarioIntroEvents.h"
#include "ee/EEStringIDs.h"
// 0x563140: the briefing sound's instance (SoundOver registers it, Close and SoundOver stop it, Open resets it)
static unsigned long sSoundID;

// FUNCTION: EXE 0x5e71cc
// Resets the scenario intro manager: no open files and every flag cleared
void EEUISCIFormManager::Clear()
{
    this->mMap = NULL;
    this->mMovie = NULL;
    this->mFile = NULL;
    this->mSound = NULL;
    this->mIsLoadingFromGame = false;
    this->mIsInGame = false;
    this->mBA0 = false;
    this->mIsCampaign = false;
    this->mScrollBriefingToEnd = false;
}

// FUNCTION: EXE 0x5e7205
// Closes and deletes the manager's map, movie, scenario and sound files
void EEUISCIFormManager::DeleteObjects()
{
    delete this->mMap;
    this->mMap = NULL;
    delete this->mMovie;
    this->mMovie = NULL;
    delete this->mFile;
    this->mFile = NULL;
    delete this->mSound;
    this->mSound = NULL;
}

// FUNCTION: EXE 0x5e7270
// Returns the scenario intro form's manager, creating and attaching it on first use
EEUISCIFormManager* EEUISCIFormManager::Get()
{
    EEUISCIFormManager* theManager;
    UIForm* theForm = gUIManager->GetFormByID(kFormScenarioIntro);
    if (theForm->GetFormManager())
        theManager = static_cast<EEUISCIFormManager*>(theForm->GetFormManager());
    else {
        theManager = new EEUISCIFormManager;
        theForm->SetFormManager(theManager);
    }
    return theManager;
}

// FUNCTION: EXE 0x5e72cf
// Returns the scenario's briefing text
UWideString EEUISCIFormManager::GetBriefing()
{
    return this->mMediaStrings.mInstructions;
}

// FUNCTION: EXE 0x5e72e4
// Opens the scenario's map picture (.jpg, else .tga) from Scenarios; returns whether one was found
bool EEUISCIFormManager::OpenMap()
{
    UString theName;
    bool theResult = false;
    long theJPEG;
    long theTarga;
    if (this->mMap)
        delete this->mMap;
    this->mMap = NULL;
    theName = this->mMediaStrings.mMapFile;
    theName += ".jpg";
    theJPEG = EEOpenSaveGameFile(theName, "Scenarios\\", this->mMap);
    if (theJPEG != kErrorOK) {
        theName = this->mMediaStrings.mMapFile;
        theName += ".tga";
        theTarga = EEOpenSaveGameFile(theName, "Scenarios\\", this->mMap);
        if (theTarga == kErrorOK)
            theResult = true;
    } else
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x5e73bd
// Opens the scenario's movie (.bik) from Movies; returns whether it was found
bool EEUISCIFormManager::OpenMovie()
{
    UString theName;
    bool theResult = false;
    long theError;
    if (this->mMovie)
        delete this->mMovie;
    this->mMovie = NULL;
    theName = this->mMediaStrings.mMovieFile;
    theName += ".bik";
    theError = EEOpenSaveGameFile(theName, "Movies\\", this->mMovie);
    if (theError == kErrorOK)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x5e7444
// Reopens the scenario file by name from Scenarios or the data path; returns whether it was found
bool EEUISCIFormManager::OpenScenario()
{
    UString theName;
    bool theResult = false;
    long theError;
    // Without a scenario file there is nothing to reopen (the name was read before the NULL test).
    if (this->mFile == NULL)
        return false;
    theName = this->mFile->GetFile();
    delete this->mFile;
    this->mFile = NULL;
    theError = EEOpenSaveGameFile(theName, "Scenarios\\", this->mFile);
    if (theError == kErrorOK || gDataPath->FindFile(theName, this->mFile) == kErrorOK)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x5e74dc
// Opens the scenario's sound (.mp3) from Sounds; returns whether it was found
bool EEUISCIFormManager::OpenSound()
{
    UString theName;
    bool theResult = false;
    long theError;
    if (this->mSound)
        delete this->mSound;
    this->mSound = NULL;
    theName = this->mMediaStrings.mSoundFile;
    theName += ".mp3";
    theError = EEOpenSaveGameFile(theName, "Sounds\\", this->mSound);
    if (theError == kErrorOK)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x5e7563
// Returns the scenario's history text
UWideString EEUISCIFormManager::GetHistory()
{
    return this->mMediaStrings.mHistory;
}

// FUNCTION: EXE 0x5e7578
// Returns the scenario's hints text
UWideString EEUISCIFormManager::GetHints()
{
    return this->mMediaStrings.mHints;
}

// FUNCTION: EXE 0x5e758d
// Opens the scenario's map picture and copies its file spec; returns whether it was found
bool EEUISCIFormManager::GetMap(FSFileSpec& outSpec)
{
    bool theResult = false;
    if (this->OpenMap()) {
        theResult = true;
        outSpec = *this->mMap;
    }
    return theResult;
}

// FUNCTION: EXE 0x5e75b5
// Opens the scenario's movie and copies its file spec; returns whether it was found
bool EEUISCIFormManager::GetMovie(FSFileSpec& outSpec)
{
    bool theResult = false;
    if (this->OpenMovie()) {
        theResult = true;
        outSpec = *this->mMovie;
    }
    return theResult;
}

// FUNCTION: EXE 0x5e75dd
// Reopens the scenario file and copies its file spec; returns whether it was found
bool EEUISCIFormManager::GetScenario(FSFileSpec& outSpec)
{
    bool theResult = false;
    if (this->OpenScenario()) {
        theResult = true;
        outSpec = *this->mFile;
    }
    return theResult;
}

// FUNCTION: EXE 0x5e7605
// Opens the scenario's sound and copies its file spec; returns whether it was found
bool EEUISCIFormManager::GetSound(FSFileSpec& outSpec)
{
    bool theResult = false;
    if (this->OpenSound()) {
        theResult = true;
        outSpec = *this->mSound;
    }
    return theResult;
}

// FUNCTION: EXE 0x5e762d
// Returns whether the scenario has any briefing, hints, history, map, movie or sound
bool EEUISCIFormManager::HasMedia()
{
    bool theResult = false;
    if (this->mMediaStrings.mInstructions.GetLength() > 0 || this->mMediaStrings.mHints.GetLength() > 0
        || this->mMediaStrings.mHistory.GetLength() > 0 || this->OpenMap() || this->OpenMovie() || this->OpenSound())
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x5e766a
// Starts or stops the main game's briefing button blinking, showing the large briefing alert while it blinks
void EEUISCIFormManager::SetBlinking(bool inBlinking)
{
    EEUIFMainGame* theForm = gUIManager->mMainGameForm;
    UIBlinkingButton* theButton = static_cast<UIBlinkingButton*>(theForm->GetControl(kMainGameGameInfoBriefing));
    UIBlinkingButton* theOther = static_cast<UIBlinkingButton*>(theForm->GetControl(kMainGameLargeScenBriefingAlert));
    theForm->mInfoBlinking = inBlinking;
    if (!theButton)
        return;
    theButton->SetIsBlinking(inBlinking);
    if (!theOther)
        return;
    if (inBlinking)
        theOther->SetDisplayMode(kDisplayModeShown);
    else
        theOther->SetDisplayMode(kDisplayModeHidden);
    theOther->SetIsBlinking(inBlinking);
}

// FUNCTION: EXE 0x5e76c1
// Back button: hides the title and returns to the game, the main game form, the campaign or the scenario load screen
long EEUIESIEBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle)->SetDisplayMode(kDisplayModeHidden);
    if (!theManager)
        return kErrorOK;
    if (theManager->mIsInGame)
        gUserInterface->CloseForm(NULL);
    else if (theManager->mIsLoadingFromGame)
        gUserInterface->OpenForm(gUIManager->mMainGameForm);
    else if (theManager->mIsCampaign)
        gUserInterface->CloseForm(NULL);
    else
        gUserInterface->OpenForm(gUIManager->GetFormByID(kFormScenarioLoad));
    return kErrorOK;
}

// FUNCTION: EXE 0x5e7741
// Shows the briefing text under its heading, scrolled to the end when asked
long EEUIESIEBriefing::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UWideString theTitle;
    UIForm* theForm = gUIManager->GetFormByID(kFormScenarioIntro);
    UIBackground* theBackground = theForm->GetBackgroundByID(0x49);
    UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kScenarioIntroTextLabel));
    UITextLabel* theHeading = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle));
    gResource->GetString(IDS_MENU_BRIEFING, theTitle);
    theHeading->SetText(theTitle);
    theHeading->SetDisplayMode(kDisplayModeShown);
    theBackground->SetVisibility(true);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager) {
        theForm->SetTabStop(1);
        theText = theManager->GetBriefing();
        theLabel->DeleteArray();
        theLabel->AppendText(theText);
    }
    if (theManager->mScrollBriefingToEnd) {
        if (theLabel->GetLineCount() > 0)
            theLabel->SetStartingDisplayLine(theLabel->GetLineCount() - 1);
        theManager->mScrollBriefingToEnd = false;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e786a
// Closes the scenario intro: stops the briefing blinking and sound, drops the map backdrop and waits half a second
long EEUIESIEClose::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    UIBackground* theBackground = inForm->GetBackgroundByID(-2);
    if (theManager) {
        theManager->SetBlinking(false);
        theManager->mIsInGame = false;
        theManager->mIsLoadingFromGame = false;
    }
    if (sSoundID != -1) {
        gSoundManager->Stop(sSoundID, false);
        gSoundManager->SetLatency(200);
    }
    if (theBackground)
        inForm->RemoveBackground(theBackground);
    Sleep(500);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e78ea
// Shows the hints text under its heading
long EEUIESIEHints::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UWideString theTitle;
    UIBackground* theBackground = inForm->GetBackgroundByID(0x49);
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kScenarioIntroTextLabel));
    UITextLabel* theHeading = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle));
    gResource->GetString(IDS_MENU_HINTS, theTitle);
    theHeading->SetText(theTitle);
    theHeading->SetDisplayMode(kDisplayModeShown);
    theBackground->SetVisibility(true);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager) {
        theText = theManager->GetHints();
        theLabel->DeleteArray();
        theLabel->AppendText(theText);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e79ba
// Shows the history text under its heading
long EEUIESIEHistory::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    UWideString theTitle;
    UIForm* theForm = gUIManager->GetFormByID(kFormScenarioIntro);
    UIBackground* theBackground = theForm->GetBackgroundByID(0x49);
    UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kScenarioIntroTextLabel));
    UITextLabel* theHeading = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle));
    gResource->GetString(IDS_MENU_HISTORY, theTitle);
    theHeading->SetText(theTitle);
    theHeading->SetDisplayMode(kDisplayModeShown);
    theBackground->SetVisibility(true);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager) {
        theText = theManager->GetHistory();
        theLabel->DeleteArray();
        theLabel->AppendText(theText);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e7a95
// Shows the map: hides the title, text and text background
long EEUIESIEMap::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIBackground* theBackground = inForm->GetBackgroundByID(0x49);
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControl(kScenarioIntroTextLabel));
    gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle)->SetDisplayMode(kDisplayModeHidden);
    theLabel->DeleteArray();
    theBackground->SetVisibility(false);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e7ae7
// Plays the scenario's movie (the briefing sound follows when it ends), or goes straight to the sound
long EEUIESIEMovie::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theMovie;
    UIForm* theForm = gUIManager->GetFormByID(kFormScenarioIntro);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (!theManager)
        return kErrorOK;
    UIEvent* theEvent = new EEUIESIESoundOver;
    if (theManager->OpenMovie()) {
        theForm->GetEntityGroup(0)->Disable();
        theManager->GetMovie(theMovie);
        gUserInterface->PostPlayMovie(theMovie, theEvent);
    } else
        gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e7b92
// Re-enables the form, plays the scenario's briefing sound once and posts the briefing page
long EEUIESIESoundOver::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theFile;
    UString theLower;
    UString theDirectory;
    FSFileSpec theSound;
    unsigned long theSample;
    UIForm* theForm = gUIManager->GetFormByID(kFormScenarioIntro);
    if (sSoundID != -1)
        gSoundManager->Stop(sSoundID, false);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager) {
        theForm->GetEntityGroup(0)->Enable();
        if (theManager->OpenSound()) {
            theManager->GetSound(theSound);
            theFile = theSound.GetFile();
            theLower = theFile;
            theLower.ToLower();
            theDirectory = UWideString("Sounds\\");
            theDirectory.ToLower();
            if (theLower.Find(theDirectory, 0) == theLower.GetLength())
                theFile = UString("Sounds\\") + theFile;
            gSoundManager->SetLatency(500);
            EELoadSound(theFile, 0x31, theSample, sSoundID, 0);
            gSoundManager->RegisterOneShotSound(theSample, sSoundID);
        }
        UIEvent* theEvent = new EEUIESIEBriefing;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e7d65
// Opens the scenario intro: loads the map backdrop, shows the map/movie/start buttons that apply and plays the
// movie or briefing; starts the scenario at once when it has no media
long EEUIESIEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theMap;
    UString theFile;
    UString theLower;
    UString theDirectory;
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
    UIEvent* theEvent;
    bool theStart;
    sSoundID = -1;
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager) {
        UIBackground* theBackground = inForm->GetBackgroundByID(-2);
        theManager->GetMap(theMap);
        theFile = theMap.GetFile();
        theLower = theFile;
        theLower.ToLower();
        theDirectory = UWideString("Scenarios\\");
        theDirectory.ToLower();
        if (theLower.Find(theDirectory, 0) == theLower.GetLength())
            theFile = UString("Scenarios\\") + theFile;
        if (theBackground == NULL || strcmp(static_cast<UITextureBackground*>(theBackground)->GetTextureFileName(), theFile) != 0) {
            if (theBackground)
                inForm->RemoveBackground(theBackground);
            if (theManager->OpenMap()) {
                inForm->LockDrawBackground();
                UIBackground* theBackdrop = new EEUIBMapBackground(-2, theFile, theDescription, theRectangle,
                                                                   theLayout, inForm, 2, 0, 0, true);
                theBackdrop->Open();
                inForm->UnlockDrawBackground();
            }
        }
    }
    inForm->GetBackgroundByID(0x81)->SetVisibility(false);
    UIControl* theMapButton = inForm->GetControl(kScenarioIntroMapButton);
    if (theManager->OpenMap())
        theMapButton->SetDisplayMode(kDisplayModeShown);
    else
        theMapButton->SetDisplayMode(kDisplayModeHidden);
    UIControl* theMovieButton = inForm->GetControl(kScenarioIntroMovieButton);
    if (theManager->OpenMovie() || theManager->OpenSound())
        theMovieButton->SetDisplayMode(kDisplayModeShown);
    else
        theMovieButton->SetDisplayMode(kDisplayModeHidden);
    if (theManager->mIsInGame)
        inForm->GetControl(kScenarioIntroStartGamePlayButton)->SetDisplayMode(kDisplayModeHidden);
    else
        inForm->GetControl(kScenarioIntroStartGamePlayButton)->SetDisplayMode(kDisplayModeShown);
    if (theManager->mIsInGame == false && theManager->mIsLoadingFromGame == false)
        theEvent = new EEUIESIEMovie;
    else {
        theEvent = new EEUIESIEBriefing;
        if (theManager->mIsInGame)
            theManager->mScrollBriefingToEnd = true;
    }
    theStart = theManager->GetBriefing().GetLength() == 0 && theManager->GetHints().GetLength() == 0
               && theManager->GetHistory().GetLength() == 0 && !theManager->OpenMap() && !theManager->OpenMovie()
               && !theManager->OpenSound();
    if (theStart)
        inForm->GetManager()->AddEventToTrigger(new EEUIESIEStart, inForm, NULL);
    else {
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e8147
// Starts the scenario: loads the saved game, or sets up single-player options (campaign or scenario), creates a
// local server and sends start loading
long EEUIESIEStart::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theScenario;
    std::vector<long> theUnused;
    gUIManager->GetFormByID(kFormScenarioIntro)->GetControl(kScenarioIntroTitle)->SetDisplayMode(kDisplayModeHidden);
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (theManager && theManager->GetScenario(theScenario)) {
        gSoundManager->SetMasterVolume(0.0f);
        if (theManager->mIsLoadingFromGame) {
            gUserInterface->PostLoadSavedGame(theScenario);
            theManager->mIsLoadingFromGame = false;
        } else if (theManager->mIsInGame == false) {
            gGameOptions.mLock->WriteLock();
            gGameOptions.SetDefaults();
            if (theManager->mIsCampaign) {
                EECampaignFormManager* theCampaign = EECampaignFormManager::Get();
                gGameOptions.mLoadType = 4;
                gGameOptions.mDifficulty = theCampaign->mDifficultyLevel;
                gGameOptions.mCampaignName = theCampaign->mCampaignName;
                gGameOptions.mCampaignChecksum = theCampaign->mChecksum;
                gGameOptions.mScenarioName = theCampaign->mScenarioName;
                gGameOptions.mMapName = theCampaign->mFirstScenario;
            } else {
                gGameOptions.mLoadType = 1;
                gLoadScenarioName = theScenario.GetFile().GetIndexedSubstring(0, theScenario.GetFile().GetLength() - 5);
            }
            gGameOptions.SetMapType((const char*)theScenario.GetFile());
            gGameOptions.mIsMultiplayer = false;
            gGameOptions.mLock->WriteUnlock();
            if (gCommunications)
                delete gCommunications;
            gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0,
                                           true, true);
            static_cast<EEServer*>(gCommunications)->StartThreads();
            static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
            static_cast<EEServer*>(gCommunications)->StopPing();
            static_cast<EEServer*>(gCommunications)->mLoadState = true;
            NEMessage* theMessage = new EEMStartLoading(1, TMGetTime());
            gLocalPlayerOut = false;
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e83cb
// Destroys the scenario intro manager, deleting its open files, and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e83fc
// Destroys the briefing event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e8427
// Destroys the movie event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e8452
// Destroys the sound-over event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e847d
// Destroys the start event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e84a8
// Appends an event trigger to the deque (std::deque push_back, from UIManager::AddEventToTrigger in Open)
