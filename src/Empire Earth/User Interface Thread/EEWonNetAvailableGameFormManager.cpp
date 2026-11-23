// C:\EEX\Empire Earth\User Interface Thread\EEWonNetAvailableGameFormManager.cpp (file name inferred: no __FILE__;
// RTTI class EEWonNetAvailableGameFormManager). 0x27a37e..0x27aa0b: the WON game room form's manager (ctor, ??_G,
// dtor, Get), then the game's free UI helpers in address order (EETranslateString, EEDisplayScenarioIntro, message
// 0x41b, the resource reference parser; the flare effect, the selection and tile lookups and EEExitGame, which the
// rest of the game calls, are Empire Earth/EEGameUtilities.cpp's). The helpers' names are provisional. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
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
#include "lle/GETerrainMesh.h"
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
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EmpireEarth.h"
#include "ee/EEServer.h"
#include "ee/EEClient.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEMap.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "lle/ISModifierState.h"
#include "rust/won_lobby/wonfront.h"

// 0x27a896 (defined below, after its user)
bool EEGetLanguageStringReference(const UWideString& inText, unsigned long& outID);

// Creates the WON available-games form manager with a 3-second refresh poll
EEWonNetAvailableGameFormManager::EEWonNetAvailableGameFormManager()
{
    this->mPoll = new TMPoll(3000, 0, false);
}

// Destroys the form manager and its poll
EEWonNetAvailableGameFormManager::~EEWonNetAvailableGameFormManager()
{
    delete this->mPoll;
}

// Returns the available-games form's manager, creating and attaching it on first use (Rust: wonfront.rs)
EEWonNetAvailableGameFormManager* EEWonNetAvailableGameFormManager::Get() { return (EEWonNetAvailableGameFormManager*)ee_won_front_call(EeWonFrontCall::FormManagerGet, NULL, 0); }

// Translates a "<string id>::text" reference into the language string; other text is copied unchanged
void EETranslateString(const UWideString& inText, UWideString& outText)
{
    unsigned long theID = 0;
    if (!EEGetLanguageStringReference(inText, theID) || gResource->GetString(theID, outText) != kErrorOK)
        outText = inText;
}

// NEAR: register choice only: the original frees esi (the hoisted GetFormByID import, reloaded after the
// block) for the three string imports; ours keeps esi and calls the UWideString dtor through memory
// Opens the scenario intro (pausing the game) when a single-player scenario starts with intro media; campaign
// media paths get the campaign folder prefix
void EEDisplayScenarioIntro()
{
    EESMMediaStrings theMediaStrings;
    UIForm* theInfoForm = gUIManager->GetFormByID(kFormScenarioIntro);
    if (!gCommunications->IsServer() || !(gCommunications->GetNumberOfCTsProcessedInPhysics() > 0)
        || gCommunications->GetIsMultiplayerSession() || theInfoForm == NULL)
        return;
    EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
    if (::gWorld.mScenarioManager != NULL) {
        theMediaStrings = ::gWorld.mScenarioManager->mMediaStrings;
        if (::gWorld.mGameOptions.mCampaignName.GetLength() > 0 && ::gWorld.mGameOptions.mCampaignChecksum > 0) {
            theMediaStrings.mSoundFile = gU52A280 + theMediaStrings.mSoundFile;
            theMediaStrings.mMapFile = gU52A280 + theMediaStrings.mMapFile;
        }
        theManager->mMediaStrings = theMediaStrings;
    }
    if (!theManager->HasMedia())
        return;
    theManager->mIsLoadingFromGame = false;
    theManager->mIsInGame = true;
    dynamic_cast<EEServer*>(gCommunications)->PauseGame();
    UIForm* theForm = gUIManager->GetFormByID(kFormGameOptions);
    if (gUIManager->GetCurrentForm() == theForm)
        gUserInterface->OpenForm(gUIManager->GetFormByID(kFormScenarioIntro));
    else
        gUserInterface->SwitchToForm(gUIManager->GetFormByID(kFormScenarioIntro));
    gUserInterface->PostPauseInformation(true);
}

// (EEOrderAtCursor, EEMakeCalamityEffect, EEGetFirstSelectedUnit and EEPointUnitOrTile(XY), the game's helpers:
// Empire Earth/EEGameUtilities.cpp)

// Takes a screenshot on F9: Alt, Ctrl or Shift choose the in-game variants, otherwise a plain screenshot
void EEPostF9Key()
{
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    bool theInGame = false;
    if (dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL
        || dynamic_cast<EEUIFScenarioEditor*>(gUIManager->GetCurrentForm()) != NULL)
        theInGame = true;
    if (theModifiers == kModifierAlt && theInGame)
        gUserInterface->PostScreenShot(0x1000, false, false);
    else if (theModifiers == kModifierControl && theInGame)
        gUserInterface->PostScreenShot(0x400, false, false);
    else if (theModifiers == kModifierShift && theInGame)
        gUserInterface->PostScreenShot(-1, false, false);
    else
        gUserInterface->PostScreenShot(-1, true, false);
}

// Parses a "<string id>::text" reference; returns its string id
bool EEGetLanguageStringReference(const UWideString& inText, unsigned long& outID)
{
    std::vector<UWideString> theTokens;
    UWideString theToken;
    bool theResult = false;
    bool theFound = inText.Tokenize("::", theTokens);
    if (theFound && theTokens.size() == 2) {
        theToken = theTokens[0];
        if (sscanf(theToken, "%lu", &outID) == 1)
            theResult = true;
    }
    return theResult;
}

// (EEExitGame: Empire Earth/EEGameUtilities.cpp)
