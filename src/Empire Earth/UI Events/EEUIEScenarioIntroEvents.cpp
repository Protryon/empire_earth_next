// C:\EEX\Empire Earth\UI Events\EEUIEScenarioIntroEvents.cpp (file name inferred: no __FILE__; class prefixes
// EEUIESIE, EEUISCIFormManager). The scenario introduction form's (0x13) form manager (its events are Rust's
// whole: ee_screens scenario_screens.rs, over UI Events/EEScreens.cpp):
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
#include "ee/EEStringIDs.h"
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
// Opens the scenario's map picture (.jpg, else .png; the original's second choice was .tga) from Scenarios; returns
// whether one was found
bool EEUISCIFormManager::OpenMap()
{
    UString theName;
    bool theResult = false;
    long theJPEG;
    long thePNG;
    if (this->mMap)
        delete this->mMap;
    this->mMap = NULL;
    theName = this->mMediaStrings.mMapFile;
    theName += ".jpg";
    theJPEG = EEOpenSaveGameFile(theName, "Scenarios\\", this->mMap);
    if (theJPEG != kErrorOK) {
        theName = this->mMediaStrings.mMapFile;
        theName += ".png";
        thePNG = EEOpenSaveGameFile(theName, "Scenarios\\", this->mMap);
        if (thePNG == kErrorOK)
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

// FUNCTION: EXE 0x5e83cb
// Destroys the scenario intro manager, deleting its open files, and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e84a8
// Appends an event trigger to the deque (std::deque push_back, from UIManager::AddEventToTrigger in Open)
