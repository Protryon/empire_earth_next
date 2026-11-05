// C:\EEX\Empire Earth\User Interface Thread\EEWonNetAvailableGameFormManager.cpp (file name inferred: no __FILE__;
// RTTI class EEWonNetAvailableGameFormManager). 0x27a37e..0x27aa0b: the WON game room form's manager (ctor, ??_G,
// dtor, Get), then the game's free UI helpers in address order (EETranslateString, EEDisplayScenarioIntro, the flare
// effect, the selection and tile lookups, message 0x41b, the resource reference parser, EEExitGame). The helpers'
// names are provisional. Unit flags: the exe module set.
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
#include "lle/UIIMECandidate.h"
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
#include "ee/EEARemovePlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEWonNetAvailableGameFormManager.h"
#include "lle/ISModifierState.h"

// 0x27a896 (defined below, after its user)
bool EEGetLanguageStringReference(const UWideString& inText, unsigned long& outID);

// FUNCTION: EXE 0x67a37e
// Creates the WON available-games form manager with a 3-second refresh poll
EEWonNetAvailableGameFormManager::EEWonNetAvailableGameFormManager()
{
    this->mPoll = new TMPoll(3000, 0, false);
}

// SYNTHETIC: EXE 0x67a3af
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x67a3cb
// Destroys the form manager and its poll
EEWonNetAvailableGameFormManager::~EEWonNetAvailableGameFormManager()
{
    delete this->mPoll;
}

// FUNCTION: EXE 0x67a3eb
// Returns the available-games form's manager, creating and attaching it on first use
EEWonNetAvailableGameFormManager* EEWonNetAvailableGameFormManager::Get()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormAvailableGames);
    EEWonNetAvailableGameFormManager* theManager;
    if (theForm->GetFormManager() != NULL)
        theManager = static_cast<EEWonNetAvailableGameFormManager*>(theForm->GetFormManager());
    else {
        theManager = new EEWonNetAvailableGameFormManager;
        theForm->SetFormManager(theManager);
    }
    return theManager;
}

// FUNCTION: EXE 0x67a432
// Translates a "<string id>::text" reference into the language string; other text is copied unchanged
void EETranslateString(const UWideString& inText, UWideString& outText)
{
    unsigned long theID = 0;
    if (!EEGetLanguageStringReference(inText, theID) || gResource->GetString(theID, outText) != kErrorOK)
        outText = inText;
}

// NEAR: register choice only: the original frees esi (the hoisted GetFormByID import, reloaded after the
// block) for the three string imports; ours keeps esi and calls the UWideString dtor through memory
// FUNCTION: EXE 0x67a470
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

// FUNCTION: EXE 0x67a60c
// Shows the command-confirmation click effect at the ground point under the cursor
void EEOrderAtCursor(const GE2DPoint& inPoint, EEUnit* inUnit)
{
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    EETile* theTile;
    EEPointUnitOrTile(inPoint, theTile, thePoint, true);
    EEMakeCalamityEffect(thePoint, inUnit);
}

// FUNCTION: EXE 0x67a644
// Creates the local-only click confirmation effect on a unit or on the ground
void EEMakeCalamityEffect(const GE3DPoint& inLocation, EEUnit* inUnit)
{
    GE3DPoint theLocation = inLocation;
    EEGraphicEffect* theEffect;
    if (inUnit == NULL) {
        theEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickNoUnit);
        theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(inLocation.GetX(), inLocation.GetY()));
    } else {
        theEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickUnit);
        if (inUnit->mTileNode != NULL)
            inUnit->mComplexUnit->GetPosition(theLocation);
        else
            inUnit->GetPosition(theLocation);
    }
    EEEffectHandle theHandle;
    gEffectManager->CreateEffect(inUnit, theLocation, theEffect, &theHandle, 0);
    EEEffect* theFlare = gEffectManager->GetEffect(theHandle);
    unsigned long thePlayerID = ::gWorld.GetCurrentPlayer()->mPlayerID;
    theFlare->mIsSaved = false;
    theFlare->mIsLocalOnly = true;
    theFlare->mPlayer = thePlayerID;
}

// FUNCTION: EXE 0x67a701
// Returns the local player's first selected unit; false when nothing is selected
bool EEGetFirstSelectedUnit(EEComplexUnit*& outUnit)
{
    EEGroup* theGroup = ::gWorld.GetCurrentPlayer()->mSelectedGroup;
    bool theResult = false;
    if (theGroup->GetNumUnits() != 0) {
        outUnit = theGroup->GetUnit(0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67a737
// Finds the tile under a screen point (picking the visible tile first); false when only the plain map lookup hit
bool EEPointUnitOrTileXY(const GE2DPoint& inPoint, EETile*& outTile, long& outX, long& outY, bool inFlag)
{
    outTile = ::gWorld.mMap->PickScreenTile(inPoint, outX, outY, inFlag);
    if (outTile != NULL)
        return true;
    outTile = ::gWorld.mMap->GetTileAt(inPoint, outX, outY);
    return false;
}

// NEAR: the original stores the tile centre's y to a frame float (inPoint's dead slot) and reloads it for
// GetMeshZ (fstp/fld); ours keeps it on the FP stack (fst)
// FUNCTION: EXE 0x67a77a
// Finds the ground point under a screen point; without a pick it uses the tile centre at terrain height and
// returns false
bool EEPointUnitOrTile(const GE2DPoint& inPoint, EETile*& outTile, GE3DPoint& outPoint, bool inFlag)
{
    outTile = ::gWorld.mMap->GetLocationAt(inPoint, outPoint, inFlag);
    if (outTile != NULL)
        return true;
    long theX;
    long theY;
    outTile = ::gWorld.mMap->GetTileAt(inPoint, theX, theY);
    outPoint.SetX(theX + 0.5f);
    outPoint.SetY(theY + 0.5f);
    outPoint.SetZ(::gWorld.mMap->GetHeight(outPoint));
    return false;
}

// FUNCTION: EXE 0x67a7f5
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

// FUNCTION: EXE 0x67a896
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

// NEAR: the original pushes mCurrentPlayer as a dword: ExecuteRemovePlayer's first parameter is wider than
// the header's unsigned char (NESynchronousCommunications/EEServer/EEClient; header item)
// FUNCTION: EXE 0x67a93e
// Leaves the game: a client removes itself (server lost) or notifies the server; the server posts the removal
// of all players when the game is open, running or paused
void EEExitGame()
{
    if (gCommunications->IsClient() == true) {
        EEClient* theClient = static_cast<EEClient*>(gCommunications);
        if (theClient->GetConnection() != NULL) {
            if (theClient->mIsServerLost)
                gCommunications->ExecuteRemovePlayer(::gWorld.mCurrentPlayer, false, false, true);
            else
                gCommunications->ProcessLocalUserInput(new NEMClientDisconnectionNotification(true), kSlotNone);
        }
    } else {
        unsigned char theState = static_cast<NESynchronousServer*>(gCommunications)->GetGameState();
        if (theState == kServerStateOpen || theState == kServerStateRunning || theState == kServerStatePaused) {
            EEAction* theAction = new EEARemovePlayer(0xfe, true);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
    }
}
