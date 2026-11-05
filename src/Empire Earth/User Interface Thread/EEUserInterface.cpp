// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.cpp (__FILE__ 0x4cf450). The UI thread: 0x276631..0x27a37e
// (EEUIPGSLineGraph.cpp ends at 0x276631). The ctor, ??_G, dtor, the thread's handlers (in address order), Run,
// HandleMessage (the 0x401..0x443 switch), the Post* members, the window procedure, then the template COMDATs of
// the message vector. Unit flags: the exe module set. Names are provisional unless the header says otherwise.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UITLScrolling.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EmpireEarth.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphics.h"
#include "ee/EETileCoordinate.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUIFMGMainGroup.h"
#include <imm.h>
#include <locale.h>
#include "lle/UICheckBox.h"
#include "lle/UIStatusBar.h"
#include "lle/UIBlinkingButton.h"
#include "lle/SECD.h"
#include "lle/SEMixer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EEWonNet.h"
#include "ee/EERenderObject.h"
#include "ee/EEROUserInterface.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEGameState.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESaveLoad.h"
#include "ee/EESMDialogMessage.h"
#include "lle/UIFormManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEUIEPDCSEvents.h"
#include "ee/EEUIEWonNetServerSelectEvents.h"
#include "ee/EEUIFError.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEABootClientFromCanceledGame.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EESETSEffectControls.h"
#include "lle/FSMemoryFile.h"
#include "ee/EEAQuickSaveLoad.h"
// free functions of other units (declared where they are used, as their own units do) (provisional names)
void Fn102E1B();                            // 0x102e1b (before the rasterizer is deactivated)
void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFlag);  // 0x140306 (Empire Earth.cpp)
void EEUnloadGameMedia();                   // 0x140d1e
void EEUpdateLandContinents();              // 0x23d76c
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);  // 0x14cfb1
bool EEIsLobbyOpen();                       // 0x297eca (the external lobby is running)
void EESetQuitFlag();                       // 0x297ed6
void EEResetGraphicsValues();               // 0x10fc7e (before a screen capture without the cursor)
void EEPostGraphicsAll();                   // 0x10fc0c
// Fn280FAF 0x280faf, Fn280FFD 0x280ffd: ee/EECivilizationBuilder.h; EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h
void EELobbyShutdown();                     // 0x295bd2
void LobbyUpdatePlayer(long inSlot, long inFlags);   // 0x1ddd8c
void LobbyUpdateGameOptions();              // 0x1deefb
void LobbyUpdateForm();                     // 0x1db6ec
void LobbyShowTransferProgress(const unsigned char& inCurrent, const unsigned char& inTotal);   // 0x1df093
extern bool gSightUpdatesEnabled;           // 0x4c76ec (set before the tested scenario starts) (provisional name)
RTSRenderObject* gU56534C;                  // 0x56534c (the UI's render object; .bss among this unit's 0x565334..0x56536c)
extern UString gUserInterfaceThreadName;   // 0x52a220 (Empire Earth.cpp)
void EEStopAllSounds();                     // 0x140ceb (Empire Earth.cpp)
#include "ee/EEStringIDs.h"
// one queued thread message (Run collects them under the queue lock, then handles them; 12 bytes)
struct EEUIThreadMessage {
    unsigned long mMessage;
    void* mParameter1;
    void* mParameter2;
};

static const float kGameViewLeft = 0.0f, kGameViewRight = 1600.0f;  // the game view below the top bar (.rdata
static const float kGameViewTop = 35.0f, kGameViewBottom = 1085.0f; // 0x44ba2c..0x44ba38: separate words)

// the IME strings' buffer (0x56536c, its size 0x565368; grown in 0x100-byte steps)
static long sIMEBufferSize;
static char* sIMEBuffer;
static unsigned long sLastLoadPercent;      // 0x565370 (SetLoadProgress)
static bool sMinimized;                     // 0x565374 (WM_SIZE: the volumes are muted)
static bool sControlDown;                   // 0x565375
static bool sAltDown;                       // 0x565376
// FUNCTION: EXE 0x676631
// Creates the UI thread object: its thread, the action, undo and redo queue locks and the message-done event
EEUserInterface::EEUserInterface()
{
    this->mThread = TSScheduler::GetCurrentThread();
    this->mB05C = false;
    this->mIsUpdating = true;
    this->mIsQuitting = false;
    this->mRandomSeed = TMGetTime();
    this->mIconBarUpdatePosted = false;
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mActionLock) == keseOK, 174);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mRedoLock) == keseOK, 175);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mUndoLock) == keseOK, 176);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 177);
}

// SYNTHETIC: EXE 0x676724
// (the scalar deleting destructor, emitted with the vtable; slot EEUserInterface+0x0)

// FUNCTION: EXE 0x676740
// Destroys the UI thread object, releasing every queued, undoable and redoable action and its locks
EEUserInterface::~EEUserInterface()
{
    while (this->GetQueueSize(this->mActionQueue, this->mActionLock) > 0)
        this->PopFront(this->mActionQueue, this->mActionLock)->Dereference();
    while (this->GetQueueSize(this->mUndoQueue, this->mUndoLock) > 0)
        this->PopFront(this->mUndoQueue, this->mUndoLock)->Dereference();
    while (this->GetQueueSize(this->mRedoQueue, this->mRedoLock) > 0)
        this->PopFront(this->mRedoQueue, this->mRedoLock)->Dereference();
    TSScheduler::DestroyCriticalSection(this->mActionLock);
    TSScheduler::DestroyCriticalSection(this->mRedoLock);
    TSScheduler::DestroyCriticalSection(this->mUndoLock);
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
}

// FUNCTION: EXE 0x676804
// Switches to another rasterizer: hides the cursor, releases the screens, deactivates the old rasterizer and
// resizes the game screen to 1024x768
void EEUserInterface::ChangeRasterizer(long inRasterizer, long inScreen)
{
    long theResult;
    gGameWindow->GetClientOrigin();
    gMousePointer->mThread->Post(0x402, NULL, NULL, true);
    while (gMousePointer->mIsShown)
        PlatSleep(1);
    for (unsigned long i = 0; i < gRasterizer->GetScreenCount(); i++)
        gRasterizer->GetScreen(i)->Release(gGameWindow);
    Fn102E1B();
    theResult = gRasterizer->Deactivate();
    ESVERIFY_AT(theResult == keseOK, 252);
    gRasterizer = GERasterizer::GetRasterizer(inRasterizer);
    gGraphics->PostSetRasterizer(gRasterizer, inScreen);
    gUIManager->ChangeRasterizer(gRasterizer);
    this->ResizeGameScreen(0x400, 0x300, true, false);
}

// FUNCTION: EXE 0x6768fe
// Resizes the game screen (unless the size is unchanged and not forced), relays out the forms and redraws the
// mini map
void EEUserInterface::ResizeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inForce, bool inFlag)
{
    bool theWasShown = gMousePointer->mIsShown;
    if (inWidth == gGameWindow->GetWidth() && inHeight == gGameWindow->GetHeight() && !inForce)
        return;
    gMousePointer->mThread->Post(0x402, NULL, NULL, true);
    while (gMousePointer->mIsShown)
        PlatSleep(1);
    while (gMousePointer->mIsShown)
        PlatSleep(1);
    EEChangeGameScreen(inWidth, inHeight, inFlag);
    gUIManager->RelayoutAllForms();
    if (theWasShown)
        gMousePointer->mThread->Post(0x404, NULL, NULL, true);
    if (gMiniMap == NULL)
        return;
    gWorld.mLock->WriteLock();
    gMiniMap->RedrawAll();
    gWorld.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x6769da
// Shows the local player's epoch name in the main game form's status text
void EEUserInterface::ShowEpochName()
{
    UWideString theText;
    EEPlayer* thePlayer = gWorld.mPlayers[gWorld.mCurrentPlayer];
    if (thePlayer != NULL && thePlayer->mTechnologies != NULL) {
        gResource->GetString(thePlayer->mTechnologies->mEpochNameID, theText);
        gUIManager->mMainGameForm->SetStatusText(theText);
    }
}

// FUNCTION: EXE 0x676a38
// Shows the WON game and system messages of the day on the multiplayer form when it is visible
void EEUserInterface::ShowMessageOfTheDay(const UWideString& inGame, const UWideString& inSystem)
{
    gUIManager->LockUIForCopyData();
    UIForm* theForm = gUIManager->GetFormByID(kFormMultiplayer);
    if (gUIManager->IsFormVisible(theForm) == true) {
        UITextLabel* theLabel = static_cast<UITextLabel*>(theForm->GetControl(kMultiplayerMessageOfTheDayTextLabel));
        theLabel->SetDisplayMode(kDisplayModeShown);
        theLabel->DeleteArray();
        theLabel->AppendText(inGame);
        theLabel->AppendText(inSystem);
    }
    gUIManager->UnlockUIForCopyData();
}

// DEAD-IN-AOC: base exe 0x661f01. The message-of-the-day poster (message 0x440, handled by ShowMessageOfTheDay above): two
// new UWideString copies. Only EEWonNet.cpp's DEAD-IN-AOC MOTDCallback calls it, so AoC's link drops it (no marker).
void EEUserInterface::PostMOTD(const UWideString& inGame, const UWideString& inSystem)
{
    UWideString* theGame = new UWideString;
    UWideString* theSystem = new UWideString;
    *theGame = inGame;
    *theSystem = inSystem;
    this->mThread->Post(0x440, theGame, theSystem, true);
}

// DEAD-IN-AOC: base exe 0x661aef. The WON login-done poster (message 0x441); dead in AoC like PostMOTD (no marker).
void EEUserInterface::PostLoginDone()
{
    this->mThread->Post(0x441, NULL, NULL, true);
}

// FUNCTION: EXE 0x676ab0
// Starts a test of the edited scenario: rebuilds continents, herds, towns and AI centres, restarts units and
// switches to the main game form
void EEUserInterface::TestScenario()
{
    GERectangle theRectangle;
    unsigned long i;
    this->PauseGraphics();
    theRectangle.SetLeft(kGameViewLeft);
    theRectangle.SetBottom(kGameViewBottom);
    theRectangle.SetTop(kGameViewTop);
    theRectangle.SetRight(kGameViewRight);
    gWorld.mLock->WriteLock();
    gGameScreenActive = true;
    gContinentManager.Clear(gWorld.mMap);
    gContinentManager.BuildRegions(gWorld.mMap);
    if (gWorld.mPlayers[0] != NULL) {
        gWorld.mPlayers[0]->RegisterHerds();
        gWorld.mPlayers[0]->AssignHerds();
    }
    for (i = 1; i < gWorld.mNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL) {
            gWorld.mPlayers[i]->RegisterTowns();
            gWorld.mPlayers[i]->AssignTowns();
        }
    }
    gPathManager.Reset();
    for (unsigned long j = 1; j < gWorld.mNumPlayers; j++) {
        if (gWorld.mPlayers[j] == NULL)
            continue;
        if (gWorld.mPlayers[j]->mComputerPlayer != NULL)
            gWorld.mPlayers[j]->mComputerPlayer->RefreshResourceCenters();
        else
            gWorld.mPlayers[j]->RefreshTownGroups();
    }
    if (gWorld.mEnvironmentManager != NULL)
        gWorld.mEnvironmentManager->FnE123D();
    gSightUpdatesEnabled = true;
    gWorld.RestartUnits();
    EEUpdateLandContinents();
    gWorld.mLock->WriteUnlock();
    this->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true);
    gUIManager->SwapForm(gUIManager->mMainGameForm);
    gWorld.mLock->WriteLock();
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gWorld.mLock->WriteUnlock();
    EESetGameViewRectangle(theRectangle);
    gAmbientSoundManager.SetEnabled(gGamePreferences.mSettings.mAmbientSounds);
    gAmbientSoundManager.Restart();
    this->ResumeGraphics();
    gPhysics->mThread->Post(0x404, NULL, NULL, true);
    gMusicManager->Start();
}

// FUNCTION: EXE 0x676c9c
// Closes the IME candidate window when the candidate window option is on
void EEUserInterface::CloseIMECandidateWindow()
{
    if (gGamePreferences.mSettings.mUseCandidateWindow == true)
        gUIManager->CloseIMECandidateWindow();
}

// FUNCTION: EXE 0x676cb1
// Closes the IME composition string
void EEUserInterface::CloseIMECompositionString()
{
    gUIManager->CloseIMECompositionString();
}

// FUNCTION: EXE 0x676cbc
// Saves the edited scenario as ScenDefault and starts testing it like a game (reporting a save error)
void EEUserInterface::SaveAndTestScenario()
{
    FSFileSpec theSpec;
    GERectangle theRectangle;
    UString theFileName;
    long theResult;
    this->PauseGraphics();
    theRectangle.SetLeft(kGameViewLeft);
    theRectangle.SetBottom(kGameViewBottom);
    theRectangle.SetTop(kGameViewTop);
    theRectangle.SetRight(kGameViewRight);
    gWorld.mLock->WriteLock();
    gGameScreenActive = true;
    gWorld.mMap->PrepareSave();
    gContinentManager.Clear(gWorld.mMap);
    gContinentManager.BuildRegions(gWorld.mMap);
    if (gUIManager->mScenarioEditorForm != NULL)
        gUIManager->mScenarioEditorForm->FnE123D();
    theFileName = UString("ScenDefault") + gScenarioExtension;
    theResult = SaveGame(theFileName, TMGetTime(), true, UString("data\\") + UString("Scenarios\\"));
    if (theResult != kErrorOK)
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true);
    gWorld.mGameOptions.mLoadType = 1;
    {
        UWideString theName(theFileName);
        gWorld.mGameOptions.mMapType = theName;
    }
    gWorld.SetNumPlayers(gWorld.mMaxPlayers);
    gArtificialIntelligence->CollectComputerPlayers();
    if (gWorld.mPlayers[0] != NULL) {
        gWorld.mPlayers[0]->RegisterHerds();
        gWorld.mPlayers[0]->AssignHerds();
    }
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL) {
            gWorld.mPlayers[i]->RegisterTowns();
            gWorld.mPlayers[i]->AssignTowns();
        }
    }
    gPathManager.Reset();
    for (unsigned long j = 1; j < gWorld.mNumPlayers; j++) {
        if (gWorld.mPlayers[j] == NULL)
            continue;
        if (gWorld.mPlayers[j]->mComputerPlayer != NULL)
            gWorld.mPlayers[j]->mComputerPlayer->RefreshResourceCenters();
        else
            gWorld.mPlayers[j]->RefreshTownGroups();
    }
    if (gWorld.mEnvironmentManager != NULL)
        gWorld.mEnvironmentManager->FnE123D();
    gSightUpdatesEnabled = true;
    gWorld.RestartUnits();
    EEUpdateLandContinents();
    gWorld.mLock->WriteUnlock();
    this->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, false, true);
    gWorld.mLock->WriteLock();
    gUIManager->SwapForm(gUIManager->mMainGameForm);
    EESetGameViewRectangle(theRectangle);
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gWorld.mLock->WriteUnlock();
    gAmbientSoundManager.SetEnabled(gGamePreferences.mSettings.mAmbientSounds);
    gAmbientSoundManager.Restart();
    this->ResumeGraphics();
    gPhysics->mThread->Post(0x404, NULL, NULL, true);
    gMusicManager->Start();
}

// FUNCTION: EXE 0x676fe0
// Releases every action of a queue
void EEUserInterface::ClearQueue(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock)
{
    inLock->Enter();
    while (this->GetQueueSize(inQueue, inLock) > 0)
        this->PopFront(inQueue, inLock)->Dereference();
    inLock->Leave();
}

// FUNCTION: EXE 0x677023
// Leaves the current map: clears the action queues, the world and the game state, optionally resizes to
// 1024x768, then pops back to a form (the post-game summary, or the previous form)
void EEUserInterface::LeaveMap(UIForm* inForm, bool inFlag)
{
    gGraphics->PostReleaseScene();
    gGraphics->PostMessage50AAndWait();
    this->ClearQueue(this->mActionQueue, this->mActionLock);
    this->ClearQueue(this->mUndoQueue, this->mUndoLock);
    this->ClearQueue(this->mRedoQueue, this->mRedoLock);
    gWorld.mLock->WriteLock();
    gDataPreLoading->WaitForMessageQueue();
    gWorld.Clear();
    gGameState.Clear();
    gPhysics->PostInit();
    gGameScreenActive = false;
    gWorld.mLock->WriteUnlock();
    if (inFlag)
        this->ResizeGameScreen(0x400, 0x300, true, false);
    if (inForm != NULL) {
        while (gUIManager->GetCurrentForm() != gUIManager->GetMainGameForm())
            gUIManager->Pop();
        if (inForm == gUIManager->GetFormByID(kFormPostGameSummary) && gWorld.mStatistics.GetRecords().size() == 1)
            gUIManager->Pop();
        else
            gUIManager->SwapForm(inForm);
    } else {
        gUIManager->Pop();
    }
    gUIManager->mB13D = false;
}

// FUNCTION: EXE 0x677170
// Cancels the running game: stops physics, unloads the game media, makes a fresh client, and leaves to the
// post-game summary (or boots the client when the host was lost)
void EEUserInterface::CancelGame()
{
    bool theFlag = false;
    if (gCommunications != NULL && gCommunications->IsClient())
        theFlag = static_cast<EEClient*>(gCommunications)->mServerLostAfterLoading;
    this->PauseGraphics();
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    while (gPhysics->mTimerRunning)
        PlatSleep(1);
    EEUnloadGameMedia();
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
    }
    if (theFlag) {
        bool theLobby = false;
        if (EEIsLobbyOpen()) {
            EESetQuitFlag();
            theLobby = true;
        }
        this->LeaveMap(NULL, true);
        EEABootClientFromCanceledGame* theAction = new EEABootClientFromCanceledGame(theLobby ? 0 : IDS_HOST_OF_THIS_GAME_HAS_GONE, true);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    } else {
        this->LeaveMap(gUIManager->GetFormByID(kFormPostGameSummary), true);
    }
    this->ResumeGraphics();
}

// FUNCTION: EXE 0x677290
// Closes the scenario editor: unloads media, makes a fresh client and leaves the map
void EEUserInterface::CloseScenarioEditor()
{
    this->PauseGraphics();
    EEUnloadGameMedia();
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    while (gPhysics->mTimerRunning)
        PlatSleep(1);
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
    }
    this->LeaveMap(NULL, true);
    this->ResumeGraphics();
}

// FUNCTION: EXE 0x677315
// Enters the scenario editor (reloading ScenDefault when asked), pausing the game, music and ambient sounds
void EEUserInterface::EnterScenarioEditor(bool inLoad)
{
    long theResult = kErrorOK;
    UWideString theMessage;
    UWideString theUnused;
    UString theFileName;
    this->PauseGraphics();
    gUIManager->CreateScenarioEditor();
    this->ResizeGameScreen(0x400, 0x300, false, true);
    gWorld.mLock->WriteLock();
    gGameScreenActive = false;
    if (inLoad == true) {
        gPhysics->RunOwedTicks();
        gPhysics->Init();
        theFileName = UString("ScenDefault") + gScenarioExtension;
        theResult = EELoadSavedGameFile(theFileName, true, true, UString("data\\") + UString("Scenarios\\"), 0);
    } else {
        gSoundManager->StopAllOneShotSounds();
    }
    if (gUIManager->GetCurrentForm() != gUIManager->GetMainGameForm())
        gUIManager->Pop();
    gUIManager->SwapForm(gUIManager->mScenarioEditorForm);
    if (theResult != kErrorOK) {
        gResource->GetString(IDS_THERE_WAS_ERROR_TRYING_TO_LOAD, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
    }
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gWorld.mLock->WriteUnlock();
    dynamic_cast<EEServer*>(gCommunications)->PauseGame();
    gMusicManager->Pause();
    gAmbientSoundManager.StopSounds();
    gAmbientSoundManager.Pause();
    EEUIECLEFormManager::Clear();
    if (gGraphics->mIsPaused)
        this->ResumeGraphics();
}

// FUNCTION: EXE 0x677529
// Returns the number of actions in a queue
unsigned long EEUserInterface::GetQueueSize(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock)
{
    inLock->Enter();
    unsigned long theSize = inQueue.size();
    inLock->Leave();
    return theSize;
}

// FUNCTION: EXE 0x67754b
// Creates the UI's render object and pushes it on the graphics thread
long EEUserInterface::CreateRenderObject()
{
    gU56534C = new EEROUserInterface(gUIManager);
    gGraphics->PostPushRenderObject(gU56534C);
    return kErrorOK;
}

// FUNCTION: EXE 0x67757c
// Loads a saved game or scenario file: checks its header, starts a local server and posts the load; reports
// an invalid file
void EEUserInterface::LoadSavedGame(FSFile* inFile)
{
    long theResult = kErrorOK;
    EEPSaveGameHeader theHeader;
    UWideString theMessage;
    theHeader.SetMajorVersion(kSaveGameMajorVersion);
    theHeader.SetMinorVersion(kSaveGameMinorVersion);
    if (theHeader.LoadFromFile(*inFile) == kErrorOK) {
        this->PauseGraphics();
        gPhysics->mThread->Post(0x402, NULL, NULL, true);
        while (gPhysics->mTimerRunning)
            PlatSleep(1);
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        gUIManager->mB13D = false;
        gGameState.Clear();
        EEStopAllSounds();
        gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash));
        static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0;
        static_cast<EEServer*>(gCommunications)->mLoadState = 1;
        static_cast<EEServer*>(gCommunications)->SetGameState(2);
        static_cast<EEServer*>(gCommunications)->SetNumHumanPlayers(1);
        static_cast<EEServer*>(gCommunications)->SetIsMultiplayerSession(false);
        gGameOptions.mLock->WriteLock();
        gGameOptions.SetDefaults();
        unsigned long theLength = inFile->GetFile().GetLength();
        gGameOptions.mLoadType = inFile->GetFile().Find(gScenarioExtension, 0) < theLength ? 1 : 2;
        {
            UWideString theName(inFile->GetFile());
            gGameOptions.mMapType = theName;
        }
        gGameOptions.mLock->WriteUnlock();
        gDataPreLoading->PostLoadGame(true, true);
        this->ResumeGraphics();
    } else {
        gResource->GetString(IDS_SAVE_GAME_ISNT_VALID, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
    }
    if (inFile != NULL)
        delete inFile;
}

// FUNCTION: EXE 0x6777cd
// Starts loading the edited scenario as a game, behind the splash form
void EEUserInterface::StartLoadingFromEditor()
{
    this->PauseGraphics();
    this->ResizeGameScreen(0x400, 0x300, true, true);
    gUIManager->Push(gUIManager->GetFormByID(kFormSplash));
    gWorld.mLock->WriteLock();
    gGameScreenActive = false;
    gWorld.mLock->WriteUnlock();
    gDataPreLoading->PostLoadGame(false, false);
    this->ResumeGraphics();
}

// FUNCTION: EXE 0x67783c
// Opens the WON server selection, remembering the password when asked
void EEUserInterface::OpenSelectServer()
{
    gUIManager->LockUIForCopyData();
    UIForm* theForm = gUIManager->GetFormByID(kFormMultiplayer);
    if (gUIManager->IsFormVisible(theForm)) {
        EEUIEWNSSOpenScreen* theEvent = new EEUIEWNSSOpenScreen;
        gUIManager->GetFormByID(kFormSelectServer)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        if (static_cast<UICheckBox*>(theForm->GetControl(kMultiplayerRememberPasswordCheckbox))->GetIsChecked() == true)
            gGamePreferences.mSettings.mWonNetPassword = UString(static_cast<UIEditBox*>(theForm->GetControl(kMultiplayerSierraPasswordEditBox))->GetText(0));
        else
            gGamePreferences.mSettings.mWonNetPassword = UString("");
        gGamePreferences.Save();
        gMultiplayerString = UString(static_cast<UIEditBox*>(theForm->GetControl(kMultiplayerSierraPasswordEditBox))->GetText(0));
        gUIManager->PopTo(gUIManager->GetFormByID(kFormMultiplayer));
        gUIManager->Push(gUIManager->GetFormByID(kFormSelectServer));
    }
    gUIManager->UnlockUIForCopyData();
}

// SYNTHETIC: EXE 0x67798d
// (EEUIEWNSSOpenScreen's scalar deleting destructor, after its first user)

// FUNCTION: EXE 0x6779b8
// Runs the UI thread: collects and handles its messages, keeps the FPU in 53-bit mode, updates the interface
// and the free camera each pass until quitting
long EEUserInterface::Run(void* inParameter)
{
    std::vector<EEUIThreadMessage> theMessages;
    UWideString theLocale;
    static unsigned long sLastTime = TMGetTime();
    gISMouse->Resume();
    gISKeyboard->Resume();
    TSThread::SetThreadName(PlatCurrentThreadId(), gUserInterfaceThreadName);
    gResource->GetString(IDS_ENGLISH, theLocale);
    setlocale(LC_ALL, theLocale);
    setlocale(LC_NUMERIC, "English");
    setlocale(LC_ALL, NULL);
    if (this->CreateRenderObject() != kErrorOK)
        return 0;
    gMousePointer->mThread->Post(0x405, gRasterizer->GetPrimarySurface(gScreenIndex), NULL, true);
    gMousePointer->mThread->Post(0x404, NULL, NULL, true);
    gGraphics->PostSetFlag54(false);
    gGraphics->PostResume();
    UMSetFloatingPointPrecision(kFPPrecision53);
    while (!this->mIsQuitting) {
        EEUIThreadMessage theMessage;
        long theResult;
        this->mThread->LockMessageQueue();
        while (true) {
            theResult = this->mThread->Receive(theMessage.mMessage, theMessage.mParameter1, theMessage.mParameter2);
            if (UMGetFloatingPointPrecision() != kFPPrecision53)
                UMSetFloatingPointPrecision(kFPPrecision53);
            if (UMGetFloatingPointRoundingMode() != 0)
                UMSetFloatingPointRoundingMode(0);
            if (theResult != kErrorOK)
                break;
            theMessages.push_back(theMessage);
            PlatSleep(1);
        }
        this->mThread->UnlockMessageQueue();
        for (unsigned long i = 0; i < theMessages.size(); i++) {
            EEUIThreadMessage& theEntry = theMessages[i];
            this->HandleMessage(theEntry.mMessage, (long)reinterpret_cast<intptr_t>(theEntry.mParameter1), (long)reinterpret_cast<intptr_t>(theEntry.mParameter2));
        }
        theMessages.clear();
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        if (this->mIsUpdating && !this->mIsQuitting)
            this->UpdateInterface();
        if (!gFreeCamera) {
            sLastTime = TMGetTime();
        } else {
            unsigned long theTime = TMGetTime();
            gWorld.mCameraControl.Update((theTime - sLastTime) * 0.001f);
            sLastTime = theTime;
        }
        PlatSleep(1);
    }
    return 0;
}

// FUNCTION: EXE 0x677c1f
// Opens the civilization builder (or the predefined civilization selection) without the builder's music
void EEUserInterface::ShowCivilizationBuilder(long inX, bool inFlag1, bool inFlag2)
{
    UWideString theMusic(gUIManager->GetFormByID(kFormCivBuilder)->GetMusicFileName());
    UIForm* theForm = gUIManager->GetFormByID(kFormPredefinedCiv);
    UIEvent* theEvent;
    this->PauseGraphics();
    gUIManager->LockUIForCopyData();
    gWorld.mLock->WriteLock();
    if (inFlag1 || inFlag2) {
        gUIManager->GetFormByID(kFormCivBuilder)->SetMusicFileName("");
    }
    if (inFlag2) {
        if (gWorld.mGameOptions.CanBuyCivilization() && !gWorld.mGameOptions.mAllowCustomCivs &&
            !gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen) {
            EESetCivBuilderButtonBlinking(false);
            gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen = true;
            gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationLocked = true;
            theEvent = new EEUIEPDCSFormOpen;
            theForm->SetFormOpenEvent(theEvent);
            theEvent->Dereference();
            theEvent = new EEUIEPDCSFormClose;
            theForm->SetFormCloseEvent(theEvent);
            theEvent->Dereference();
            gUIManager->Push(theForm);
        } else {
            Fn280FAF();
        }
    } else {
        Fn280FFD(reinterpret_cast<EECivilization*>(inX), inFlag1);
    }
    gUIManager->GetFormByID(kFormCivBuilder)->SetMusicFileName(theMusic);
    gWorld.mLock->WriteUnlock();
    gUIManager->UnlockUIForCopyData();
    this->ResumeGraphics();
}

// SYNTHETIC: EXE 0x677e0d
// (EEUIEPDCSFormOpen's scalar deleting destructor, after its first user)

// SYNTHETIC: EXE 0x677e38
// (EEUIEPDCSFormClose's scalar deleting destructor)

// FUNCTION: EXE 0x677e63
// Pauses the graphics thread and waits until it is paused
void EEUserInterface::PauseGraphics()
{
    gGraphics->PostPause();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        PlatSleep(1);
}

// FUNCTION: EXE 0x677e8e
// Returns the last action of a queue
EEAction* EEUserInterface::PeekBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock)
{
    inLock->Enter();
    EEAction* theAction = inQueue.back();
    inLock->Leave();
    return theAction;
}

// FUNCTION: EXE 0x677eb4
// Removes and returns the first action of a queue
EEAction* EEUserInterface::PopFront(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock)
{
    inLock->Enter();
    EEAction* theAction = inQueue.front();
    inQueue.pop_front();
    inLock->Leave();
    return theAction;
}

// FUNCTION: EXE 0x677ef2
// Removes and returns the last action of a queue
EEAction* EEUserInterface::PopBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock)
{
    inLock->Enter();
    EEAction* theAction = inQueue.back();
    inQueue.pop_back();
    inLock->Leave();
    return theAction;
}

// FUNCTION: EXE 0x677f21
// Runs the queued actions; undoable ones go on the undo list (at most 100) and clear the redo list
void EEUserInterface::ProcessActions()
{
    EEAction* theLast;
    if (!(this->GetQueueSize(this->mActionQueue, this->mActionLock) > 0))
        return;
    gUIManager->LockUIForCopyData();
    gWorld.mLock->WriteLock();
    if (this->GetQueueSize(this->mUndoQueue, this->mUndoLock) == 0)
        theLast = NULL;
    else
        theLast = this->PeekBack(this->mUndoQueue, this->mUndoLock);
    while (this->GetQueueSize(this->mActionQueue, this->mActionLock) > 0) {
        EEAction* theAction = this->PopFront(this->mActionQueue, this->mActionLock);
        theAction->Virtual4();
        if (!theAction->mIsUndoable)
            theAction->Dereference();
        else {
            while (this->GetQueueSize(this->mRedoQueue, this->mRedoLock) != 0)
                this->PopFront(this->mRedoQueue, this->mRedoLock)->Dereference();
            if (theAction == theLast)
                theAction->Dereference();
            else {
                this->PushBack(this->mUndoQueue, this->mUndoLock, theAction);
                if (this->GetQueueSize(this->mUndoQueue, this->mUndoLock) > 100)
                    this->PopFront(this->mUndoQueue, this->mUndoLock)->Dereference();
            }
        }
    }
    gWorld.mLock->WriteUnlock();
    gUIManager->UnlockUIForCopyData();
}

// FUNCTION: EXE 0x678040
// Passes the IME candidate list to the UI when the candidate window option is on
void EEUserInterface::UpdateIMECandidateList(HWND inWindow)
{
    if (gGamePreferences.mSettings.mUseCandidateWindow != true)
        return;
    HIMC theContext = ImmGetContext(inWindow);
    if (theContext == NULL)
        return;
    long theSize = ImmGetCandidateListA(theContext, 0, NULL, 0);
    if (theSize >= sIMEBufferSize) {
        delete sIMEBuffer;
        sIMEBufferSize = (theSize / 0x100 + 1) * 0x100;
        sIMEBuffer = new char[sIMEBufferSize];
    }
    ImmGetCandidateListA(theContext, 0, (LPCANDIDATELIST)sIMEBuffer, sIMEBufferSize);
    gUIManager->ChangeIMECandidateWindow(*reinterpret_cast<UIIMECandidateList*>(sIMEBuffer));
    ImmReleaseContext(inWindow, theContext);
}

// FUNCTION: EXE 0x6780d3
// Passes the IME composition string, its attributes and cursor to the UI
void EEUserInterface::UpdateIMECompositionString(HWND inWindow)
{
    UWideString theString;
    HIMC theContext = ImmGetContext(inWindow);
    if (theContext != NULL) {
        long theCursor = ImmGetCompositionStringA(theContext, GCS_CURSORPOS, NULL, 0);
        long theSize = ImmGetCompositionStringA(theContext, GCS_COMPSTR, NULL, 0);
        if (theSize >= sIMEBufferSize) {
            delete sIMEBuffer;
            sIMEBufferSize = (theSize / 0x100 + 1) * 0x100;
            sIMEBuffer = new char[sIMEBufferSize];
        }
        theSize = ImmGetCompositionStringA(theContext, GCS_COMPSTR, sIMEBuffer, sIMEBufferSize);
        sIMEBuffer[theSize] = 0;
        theString = sIMEBuffer;
        theSize = ImmGetCompositionStringA(theContext, GCS_COMPATTR, sIMEBuffer, sIMEBufferSize);
        sIMEBuffer[theSize] = 0;
        gUIManager->ChangeIMECompositionString(theString, UString(sIMEBuffer), theCursor);
        ImmReleaseContext(inWindow, theContext);
    }
}

// FUNCTION: EXE 0x6781da
// Passes the IME result string to the UI
void EEUserInterface::AddIMEResultString(HWND inWindow)
{
    HIMC theContext = ImmGetContext(inWindow);
    if (theContext == NULL)
        return;
    long theSize = ImmGetCompositionStringA(theContext, GCS_RESULTSTR, NULL, 0);
    if (theSize >= sIMEBufferSize) {
        delete sIMEBuffer;
        sIMEBufferSize = (theSize / 0x100 + 1) * 0x100;
        sIMEBuffer = new char[sIMEBufferSize];
    }
    theSize = ImmGetCompositionStringA(theContext, GCS_RESULTSTR, sIMEBuffer, sIMEBufferSize);
    sIMEBuffer[theSize] = 0;
    gUIManager->AddIMEResultString(sIMEBuffer);
    ImmReleaseContext(inWindow, theContext);
}

// FUNCTION: EXE 0x67828f
// Handles one UI thread message (0x401..0x443): loading, forms, saved games, scenario editor, screenshots,
// chat log, movies, civilization builder, lobby updates and load progress
void EEUserInterface::HandleMessage(unsigned long inMessage, long inParameter1, long inParameter2)
{
    switch (inMessage) {
    case 0x407: {
        this->PauseGraphics();
        this->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true);
        UIForm* theCurrent = gUIManager->GetCurrentForm();
        if (theCurrent == gUIManager->GetFormByID(kFormLobby) || theCurrent == gUIManager->GetFormByID(kFormSinglePlayerLoadGame) ||
            theCurrent == gUIManager->GetFormByID(kFormScenarioIntro))
            gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash));
        else
            gUIManager->Push(gUIManager->GetFormByID(kFormSplash));
        gWorld.mLock->WriteLock();
        gGameScreenActive = true;
        gWorld.mLock->WriteUnlock();
        gDataPreLoading->PostLoadGame(false, false);
        gUIManager->FnE123D();
        this->ResumeGraphics();
        break;
    }
    case 0x404:
        this->ShowEpochName();
        break;
    case 0x403:
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        gUIManager->mMainGameForm->UpdateBackgroundMedia(true);
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x402:
        this->mIsUpdating = inParameter1;
        break;
    case WM_QUIT:
    case kThreadMessageQuit:
        this->mIsQuitting = true;
        break;

    case 0x408:
        this->LoadSavedGame(reinterpret_cast<FSFile*>(inParameter1));
        break;
    case 0x40e: {
        this->PauseGraphics();
        gWorld.mLock->ReadLock();
        UIEvent* theEvent = reinterpret_cast<UIEvent*>(inParameter1);
        theEvent->ProcessEvent(NULL, reinterpret_cast<UIEntity*>(inParameter2));
        theEvent->Dereference();
        gWorld.mLock->ReadUnlock();
        this->ResumeGraphics();
        break;
    }
    case 0x40d:
        this->PauseGraphics();
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        gUIManager->SwapForm(reinterpret_cast<UIForm*>(inParameter1));
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        this->ResumeGraphics();
        break;
    case 0x40c: {
        if (gGraphics->PostPause() != kErrorOK)
            break;
        for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
            PlatSleep(1);
        EEAQuickSaveLoad* theAction = reinterpret_cast<EEAQuickSaveLoad*>(inParameter1);
        theAction->mIsLoad = true;
        theAction->mGameState = &gGameState;
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case 0x40b:
        this->ShowLoadingText(inParameter1);
        break;
    case 0x40a:
        gUIManager->LockUIForCopyData();
        if (gUIManager != NULL && gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormSplash))
            (static_cast<UIStatusBar*>(gUIManager->GetFormByID(kFormSplash)->GetControl(kSplashLoadStatus)))->SetCurrentRangeValue(inParameter1);
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x40f:
        this->SaveAndTestScenario();
        break;
    case 0x41b: {
        bool theKeepCursor = inParameter2 > 0;
        if (!theKeepCursor)
            EEResetGraphicsValues();
        EEScreenCapture* theScreenCaptureStruct = reinterpret_cast<EEScreenCapture*>(inParameter1);
        ESVERIFY_AT(TSScheduler::CreateEvent(theScreenCaptureStruct->mCaptureComplete) == keseOK, 1956);
        gGraphics->PostScreenShot(theScreenCaptureStruct);
        theScreenCaptureStruct->mCaptureComplete->Wait(kTSWaitInfinite);
        TSScheduler::DestroyEvent(theScreenCaptureStruct->mCaptureComplete);
        if (theScreenCaptureStruct->mWidth != -1)
            theScreenCaptureStruct->mSurface->Dereference();
        delete theScreenCaptureStruct;
        if (!theKeepCursor)
            EEPostGraphicsAll();
        gUIManager->RedrawAll();
        break;
    }
    case 0x410:
        this->CloseScenarioEditor();
        break;
    case 0x411:
        this->EnterScenarioEditor(inParameter1 > 0);
        break;
    case 0x412:
        this->CancelGame();
        break;
    case 0x413:
        this->StartLoadingFromEditor();
        break;
    case 0x415:
        this->TestScenario();
        break;
    case 0x416:
        if (gUIManager->mMainGameForm != NULL)
            gUIManager->mMainGameForm->mRefreshGroup = true;
        break;
    case 0x419:
        this->PauseGraphics();
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        if (inParameter1 == 0)
            gUIManager->Pop();
        else
            gUIManager->PopTo(reinterpret_cast<UIForm*>(inParameter1));
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        this->ResumeGraphics();
        break;
    case 0x414:
        this->PauseGraphics();
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        gUIManager->Push(reinterpret_cast<UIForm*>(inParameter1));
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        this->ResumeGraphics();
        break;
    case 0x417: {
        this->PauseGraphics();
        gUIManager->LockUIForCopyData();
        UIForm* theForm = reinterpret_cast<UIForm*>(inParameter1);
        if (!gUIManager->IsFormOnStack(theForm)) {
            EEUIFErrorCallbackData* theData = reinterpret_cast<EEUIFErrorCallbackData*>(inParameter2);
            if (theData->mCallback != NULL) {
                gWorld.mLock->ReadLock();
                theData->mCallback(static_cast<UIForm*>(theData->mData));
                gWorld.mLock->ReadUnlock();
            }
            if (theForm != NULL)
                delete theForm;
            delete theData;
        }
        gUIManager->UnlockUIForCopyData();
        this->ResumeGraphics();
        break;
    }
    case 0x418: {
        if (gUIManager == NULL || gUIManager->mMainGameForm == NULL)
            break;
        EEChatLogEntry* theEntry = reinterpret_cast<EEChatLogEntry*>(inParameter1);
        gUIManager->LockUIForCopyData();
        gUIManager->mMainGameForm->DisplayChat(theEntry->mColor, theEntry->mName, theEntry->mText,
                                               theEntry->mTime, theEntry->mType);
        gUIManager->UnlockUIForCopyData();
        if (theEntry->mType != 2)
            EEUIECLEFormManager::AddEntry(*theEntry);
        if (theEntry != NULL)
            delete theEntry;
        break;
    }
    case 0x41c: {
        FSFileSpec* theSpec = reinterpret_cast<FSFileSpec*>(inParameter1);
        if (!gGraphics->mIsMoviePlaying) {
            if (gGraphics->PlayMovie(*theSpec, reinterpret_cast<UReferenceCounter*>(inParameter2)) == kErrorOK)
                gMouse->mPointer = 6;
            while (!gGraphics->mIsMoviePlaying)
                PlatSleep(1);
        } else {
            gGraphics->PostStopMovie();
            while (gGraphics->mIsMoviePlaying)
                ;
            PlatSleep(1);
            gUserInterface->PostPlayMovie(*theSpec, reinterpret_cast<UReferenceCounter*>(inParameter2));
        }
        if (inParameter2 != 0)
            (reinterpret_cast<UReferenceCounter*>(inParameter2))->Dereference();
        if (theSpec != NULL)
            delete theSpec;
        break;
    }
    case 0x422: {
        EESMDialogMessage* theDialog = reinterpret_cast<EESMDialogMessage*>(inParameter1);
        if (gUIManager != NULL && gUIManager->mMainGameForm != NULL) {
            gUIManager->LockUIForCopyData();
            gUIManager->mMainGameForm->ShowInformation((EEUIFMGInformation*)theDialog);
            gUIManager->UnlockUIForCopyData();
        }
        EEChatLogEntry* theEntry = new EEChatLogEntry;
        theEntry->mText = theDialog->mText;
        theEntry->mType = 1;
        EEUIECLEFormManager::AddEntry(*theEntry);
        delete theEntry;
        if (theDialog != NULL)
            delete theDialog;
        break;
    }
    case 0x429:
        this->PauseGraphics();
        this->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true);
        gWorld.mLock->ReadLock();
        gUIManager->Pop();
        gWorld.mLock->ReadUnlock();
        this->ResumeGraphics();
        break;
    case 0x430: {
        this->PauseGraphics();
        this->ResizeGameScreen(0x400, 0x300, true, false);
        if (inParameter2 <= 0) {
            gUIManager->Pop();
        } else {
            EESetQuitFlag();
            EEABootClientFromCanceledGame* theAction = new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
            static_cast<NESynchronousClient*>(gCommunications)->Disconnect();
            if (gCommunications != NULL)
                delete gCommunications;
            gCommunications = new EEClient;
            static_cast<EEClient*>(gCommunications)->StartThreads();
        }
        this->ResumeGraphics();
        break;
    }
    case 0x424:
        if (gUIManager == NULL || gUIManager->mMainGameForm == NULL)
            break;
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        gUIManager->mMainGameForm->UpdateIconBar();
        this->mIconBarUpdatePosted = false;
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x41e: {
        if (gGraphics->PostPause() != kErrorOK)
            break;
        for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
            PlatSleep(1);
        this->ChangeRasterizer(inParameter1, inParameter2);
        gGraphics->PostResume();
        for (bool theStopped = gGraphics->mIsPaused; theStopped != false; theStopped = gGraphics->mIsPaused)
            PlatSleep(1);
        gUIManager->RedrawAll();
        break;
    }
    case 0x427:
        if (gUIManager == NULL || gUIManager->mMainGameForm == NULL)
            break;
        gUIManager->LockUIForCopyData();
        if ((unsigned long)inParameter1 > 0)
            gUIManager->mMainGameForm->PauseInformation();
        else
            gUIManager->mMainGameForm->ResumeInformation();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x420:
        this->ShowCivilizationBuilder(inParameter1, *reinterpret_cast<bool*>(&inParameter2), false);  // STAND-IN (owner 2026-11-01): re-typing read, 0x278b46 pushes the raw dword
        break;
    case 0x41d:
        gUIManager->RedrawAll();
        gMouse->mPointer = 0;
        if (inParameter1 != 0) {
            gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(inParameter1), NULL);
            reinterpret_cast<UIEvent*>(inParameter1)->Dereference();
        }
        break;
    case 0x432:
        if (gUIManager->mMainGameForm != NULL)
            gUIManager->mMainGameForm->HideMessages();
        break;
    case 0x421:
        if (gUIManager->mScenarioEditorForm == NULL)
            break;
        this->PauseGraphics();
        gWorld.mLock->WriteLock();
        gUIManager->mScenarioEditorForm->SelectTab((unsigned char)inParameter1, inParameter2);
        gWorld.mLock->WriteUnlock();
        this->ResumeGraphics();
        break;
    case 0x426:
        gUIManager->LockUIForCopyData();
        if (gUIManager != NULL) {
            if (gUIManager->GetMainGameForm() == gUIManager->GetCurrentForm())
                gUIManager->mMainGameForm->FnE123D();
        }
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x428:
        this->RestartGame();
        break;
    case 0x435:
        if (gUIManager == NULL || gUIManager->mMainGameForm == NULL)
            break;
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
        if (gUIManager->IsFormVisible(gUIManager->mMainGameForm))
            (static_cast<EEUIBSavedGroup*>(gUIManager->mMainGameForm->GetControl(inParameter1 + 0xd3)))->Refresh();
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x441:
        this->OpenSelectServer();
        break;
    case 0x440:
        this->ShowMessageOfTheDay(*reinterpret_cast<UWideString*>(inParameter1), *reinterpret_cast<UWideString*>(inParameter2));
        if (inParameter1 != 0)
            delete reinterpret_cast<UWideString*>(inParameter1);
        if (inParameter2 != 0)
            delete reinterpret_cast<UWideString*>(inParameter2);
        break;
    case 0x41f:
        this->ShowCivilizationBuilder(0, false, true);
        break;
    case 0x436:
        gUIManager->LockUIForCopyData();
        LobbyUpdatePlayer(inParameter1, inParameter2);
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x437:
        gUIManager->LockUIForCopyData();
        LobbyUpdateGameOptions();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x439: {
        gUIManager->LockUIForCopyData();
        unsigned char theTotal = (unsigned char)inParameter2;
        unsigned char theCurrent = (unsigned char)inParameter1;
        LobbyShowTransferProgress(theCurrent, theTotal);
        gUIManager->UnlockUIForCopyData();
        break;
    }
    case 0x438:
        gUIManager->LockUIForCopyData();
        LobbyUpdateForm();
        gUIManager->UnlockUIForCopyData();
        break;
    case 0x442:
        this->ShowLagStatus((unsigned short)inParameter1, (unsigned long)inParameter2 > 0);
    case 0x443:
        gUIManager->LockUIForCopyData();
        if (gUIManager->IsFormVisible(gUIManager->GetFormByID(kFormSplash)))
            (static_cast<UIStatusBar*>(gUIManager->GetFormByID(kFormSplash)->GetControl(inParameter1 + 0x14)))
                ->SetCurrentRangeValue(inParameter2);
        gUIManager->UnlockUIForCopyData();
    case 0x431:
        this->mMessageQueueDone->Set();
        break;
    }
}

// FUNCTION: EXE 0x678ebc
// Destroys a chat log entry
// (EEChatLogEntry's implicit destructor, after its first user)

// FUNCTION: EXE 0x678ed3
// Updates the interface: keyboard and mouse input, the UI under the world read lock, then the queued actions
void EEUserInterface::UpdateInterface()
{
    gISKeyboard->Refresh();
    gMouse->mInputLock->Enter();
    gISMouse->Refresh();
    gMouse->Update();
    gMouse->mInputLock->Leave();
    gWorld.mLock->ReadLock();
    if (!Fn297ED0())
        gUIManager->UpdateUI(gMouse->mPosition);
    gWorld.mLock->ReadUnlock();
    this->ProcessActions();
}

// FUNCTION: EXE 0x678f54
// Appends an action to a queue
void EEUserInterface::PushBack(std::deque<EEAction*>& inQueue, TSCriticalSection* inLock, EEAction* inAction)
{
    inLock->Enter();
    inQueue.push_back(inAction);
    inLock->Leave();
}

// FUNCTION: EXE 0x678f79
// Queues an action for the UI thread (taking a reference)
void EEUserInterface::PostAction(EEAction* inAction)
{
    inAction->Reference();
    this->PushBack(this->mActionQueue, this->mActionLock, inAction);
}

// FUNCTION: EXE 0x678f90
// Opens the IME candidate window (creating it first) when the candidate window option is on
void EEUserInterface::OpenIMECandidateWindow()
{
    if (gGamePreferences.mSettings.mUseCandidateWindow != true)
        return;
    if (gUIManager->GetCandidateWindow() == NULL)
        gUIManager->CreateIMECandidateWindow(gU531938);
    gUIManager->OpenIMECandidateWindow();
}

// FUNCTION: EXE 0x678fc0
// Opens the IME composition string
void EEUserInterface::OpenIMECompositionString(HWND inWindow)
{
    gUIManager->OpenIMECompositionString();
}

// FUNCTION: EXE 0x678fce
// Redoes the last undone action; returns 7 when there is none
long EEUserInterface::Redo()
{
    long theResult = kErrorOK;
    if (this->GetQueueSize(this->mRedoQueue, this->mRedoLock) != 0) {
        EEAction* theAction = this->PopBack(this->mRedoQueue, this->mRedoLock);
        theAction->Virtual4();
        this->PushBack(this->mUndoQueue, this->mUndoLock, theAction);
    } else {
        theResult = 7;
    }
    return theResult;
}

// FUNCTION: EXE 0x679013
// Restarts the single player game: a fresh local server, the splash form, then reloads the game
void EEUserInterface::RestartGame()
{
    this->PauseGraphics();
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    while (gPhysics->mTimerRunning)
        PlatSleep(1);
    if (gCommunications != NULL)
        delete gCommunications;
    gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
    static_cast<EEServer*>(gCommunications)->StartThreads();
    this->LeaveMap(gUIManager->GetFormByID(kFormSplash), false);
    gGameScreenActive = true;
    static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
    static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0;
    static_cast<EEServer*>(gCommunications)->mLoadState = 1;
    static_cast<EEServer*>(gCommunications)->SetGameState(2);
    static_cast<EEServer*>(gCommunications)->SetNumHumanPlayers(1);
    static_cast<EEServer*>(gCommunications)->SetIsMultiplayerSession(false);
    gDataPreLoading->PostLoadGame(true, false);
    this->ResumeGraphics();
}

// FUNCTION: EXE 0x679139
// Posts a screenshot request (message 0x41b)
void EEUserInterface::PostScreenShot(long inX, bool inFlag1, bool inFlag2)
{
    EEScreenCapture* theCapture = new EEScreenCapture;
    theCapture->mCaptureComplete = NULL;
    theCapture->mSurface = NULL;
    theCapture->mIsPostGame = inFlag2;
    theCapture->mWidth = inX;
    this->mThread->Post(0x41b, theCapture, (void*)inFlag1, true);
}

// FUNCTION: EXE 0x679174
// Posts a background media update (message 0x403)
void EEUserInterface::PostUpdateBackgroundMedia()
{
    this->mThread->Post(0x403, NULL, NULL, true);
}

// FUNCTION: EXE 0x679189
// Posts showing the epoch name (message 0x404)
void EEUserInterface::PostShowEpochName()
{
    this->mThread->Post(0x404, NULL, NULL, true);
}

// FUNCTION: EXE 0x67919e
// Posts a scenario dialog message (message 0x422)
void EEUserInterface::PostDialogMessage(const EESMDialogMessage& inMessage)
{
    this->mThread->Post(0x422, new EESMDialogMessage(inMessage), NULL, true);
}

// FUNCTION: EXE 0x6791d3
// Posts selecting a scenario editor tab (message 0x421)
void EEUserInterface::PostSelectEditorTab(long inX, long inY)
{
    this->mThread->Post(0x421, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// FUNCTION: EXE 0x6791ee
// Posts starting the scenario test (message 0x40f)
void EEUserInterface::StartScenarioTest()
{
    this->mThread->Post(0x40f, NULL, NULL, true);
}

// FUNCTION: EXE 0x679203
// Posts a display change (message 0x429)
void EEUserInterface::PostDisplayChanged(bool inFlag)
{
    this->mThread->Post(0x429, (void*)inFlag, NULL, true);
}

// FUNCTION: EXE 0x67921e
// Posts cancelling the game (message 0x412)
void EEUserInterface::PostCancelGame()
{
    this->mThread->Post(0x412, NULL, NULL, true);
}

// FUNCTION: EXE 0x679233
// Posts that loading the game failed (message 0x430)
void EEUserInterface::PostLoadGameFailed(bool inFlag)
{
    this->mThread->Post(0x430, NULL, (void*)inFlag, true);
}

// FUNCTION: EXE 0x67924e
// Posts leaving the scenario editor (message 0x410)
void EEUserInterface::LeaveScenarioEditor()
{
    this->mThread->Post(0x410, NULL, NULL, true);
}

// FUNCTION: EXE 0x679263
// Posts entering the scenario editor (message 0x411)
long EEUserInterface::PostEnterScenarioEditor(bool inFlag)
{
    return this->mThread->Post(0x411, (void*)inFlag, NULL, true);
}

// FUNCTION: EXE 0x67927e
// Posts quitting the UI thread
void EEUserInterface::PostQuit()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
}

// FUNCTION: EXE 0x679293
// Posts showing or hiding the pause information (message 0x427)
void EEUserInterface::PostPauseInformation(bool inFlag)
{
    this->mThread->Post(0x427, (void*)inFlag, NULL, true);
}

// FUNCTION: EXE 0x6792ae
// Posts that the host migrated (message 0x426)
void EEUserInterface::PostHostMigrated()
{
    this->mThread->Post(0x426, NULL, NULL, true);
}

// FUNCTION: EXE 0x6792c3
// Posts starting to load the game (0x407; 0x413 from the scenario editor)
void EEUserInterface::PostStartLoading()
{
    if (!gUIManager->mInScenarioEditor)
        this->mThread->Post(0x407, NULL, NULL, true);
    else
        this->mThread->Post(0x413, NULL, NULL, true);
}

// FUNCTION: EXE 0x6792ed
// Posts loading a saved game file (message 0x408)
void EEUserInterface::PostLoadSavedGame(const FSFileSpec& inSpec)
{
    this->mThread->Post(0x408, new FSFileSpec(inSpec), NULL, true);
}

// FUNCTION: EXE 0x67932d
// Posts a UI event for a form (message 0x40e)
long EEUserInterface::PostEvent(UIEvent* inEvent, UIForm* inForm)
{
    inEvent->Reference();
    return this->mThread->Post(0x40e, inEvent, inForm, true);
}

// FUNCTION: EXE 0x67934c
// Posts playing a movie (message 0x41c)
void EEUserInterface::PostPlayMovie(const FSFileSpec& inSpec, UReferenceCounter* inObject)
{
    FSFileSpec* theSpec = new FSFileSpec(inSpec);
    if (inObject != NULL)
        inObject->Reference();
    this->mThread->Post(0x41c, theSpec, inObject, true);
}

// FUNCTION: EXE 0x679396
// Posts that a movie finished (message 0x41d)
void EEUserInterface::PostMovieDone(UReferenceCounter* inObject)
{
    if (inObject != NULL)
        inObject->Reference();
    this->mThread->Post(0x41d, inObject, NULL, true);
}

// FUNCTION: EXE 0x6793b7
// Posts hiding the messages (message 0x432)
void EEUserInterface::PostHideMessages()
{
    this->mThread->Post(0x432, NULL, NULL, true);
}

// FUNCTION: EXE 0x6793cc
// Posts opening the civilization builder (message 0x420)
long EEUserInterface::OpenCivilizationBuilder(EECivilization* inCivilization, bool inFlag)
{
    return this->mThread->Post(0x420, inCivilization, (void*)inFlag, true);
}

// FUNCTION: EXE 0x6793e9
// Posts closing a form (message 0x419)
long EEUserInterface::CloseForm(UIForm* inForm)
{
    return this->mThread->Post(0x419, inForm, NULL, true);
}

// FUNCTION: EXE 0x679402
// Posts switching to a form (message 0x414)
long EEUserInterface::SwitchToForm(UIForm* inForm)
{
    return this->mThread->Post(0x414, inForm, NULL, true);
}

// FUNCTION: EXE 0x67941b
// Posts showing a loaded object (message 0x40c)
void EEUserInterface::ShowLoadedObject(UReferenceCounter* inObject)
{
    inObject->Reference();
    this->mThread->Post(0x40c, inObject, NULL, true);
}

// FUNCTION: EXE 0x679438
// Posts that a message box closed, with its callback data (message 0x417)
void EEUserInterface::PostErrorClosed(UIForm* inForm, EEUIFErrorCallbackData* inData)
{
    this->mThread->Post(0x417, inForm, inData, true);
}

// FUNCTION: EXE 0x679453
// Posts restarting the game (message 0x428)
void EEUserInterface::PostRestartGame()
{
    this->mThread->Post(0x428, NULL, NULL, true);
}

// FUNCTION: EXE 0x679468
// Posts starting the scenario test the second way (message 0x415)
void EEUserInterface::StartScenarioTest2()
{
    this->mThread->Post(0x415, NULL, NULL, true);
}

// FUNCTION: EXE 0x67947d
// Posts showing the civilization builder (message 0x41f)
long EEUserInterface::PostDisplayCivBuilder()
{
    return this->mThread->Post(0x41f, NULL, NULL, true);
}

// FUNCTION: EXE 0x679492
// Posts opening a form (message 0x40d)
long EEUserInterface::OpenForm(UIForm* inForm)
{
    return this->mThread->Post(0x40d, inForm, NULL, true);
}

// FUNCTION: EXE 0x6794ab
// Posts changing the rasterizer (message 0x41e)
void EEUserInterface::PostChangeRasterizer(long inX, long inY)
{
    this->mThread->Post(0x41e, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// FUNCTION: EXE 0x6794c6
// Posts a chat log entry with a name, colour, text, display time and type (message 0x418)
long EEUserInterface::AddChatLogEntry(const GEColor& inColor, const UWideString& inName, const UWideString& inText,
                                      unsigned long inTime, unsigned long inType)
{
    EEChatLogEntry* theEntry = new EEChatLogEntry;
    theEntry->mName = inName;
    theEntry->mColor = inColor;
    theEntry->mText = inText;
    theEntry->mTime = inTime;
    theEntry->mType = inType;
    return this->mThread->Post(0x418, theEntry, NULL, true);
}

// FUNCTION: EXE 0x679536
// Posts a white system line to the chat log, shown three times the usual time
long EEUserInterface::AddChatLogEntry(const UWideString& inText, long inY)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    return this->AddChatLogEntry(theColor, " > ", inText, gWorldTable.GetLongValue(25) * 3, 2);
}

// FUNCTION: EXE 0x679591
// Posts a just-in-time manufacturing update (message 0x416)
void EEUserInterface::UpdateJITManufacturing()
{
    this->mThread->Post(0x416, NULL, NULL, true);
}

// FUNCTION: EXE 0x6795a6
// Posts a lobby player update (message 0x436)
void EEUserInterface::PostLobbyUpdatePlayer(long inX, long inY)
{
    this->mThread->Post(0x436, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// FUNCTION: EXE 0x6795c1
// Posts a lobby game options update (message 0x437)
void EEUserInterface::PostLobbyUpdateGameOptions()
{
    this->mThread->Post(0x437, NULL, NULL, true);
}

// FUNCTION: EXE 0x6795d6
// Posts a player's lag status (message 0x442)
void EEUserInterface::SetLagStatus(unsigned short inLag, unsigned char inStatus)
{
    this->mThread->Post(0x442, reinterpret_cast<void*>(inLag), (void*)(inStatus == 1), true);
}

// FUNCTION: EXE 0x6795fa
// Posts a player's load percentage (message 0x443)
void EEUserInterface::SetLoadPercent(long inPlayer, unsigned long inPercent)
{
    this->mThread->Post(0x443, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inPercent), true);
}

// FUNCTION: EXE 0x679615
// Posts the lobby's transfer progress (message 0x439)
void EEUserInterface::PostLobbyShowTransferProgress(long inX, long inY)
{
    this->mThread->Post(0x439, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// FUNCTION: EXE 0x679630
// Posts a lobby form update (message 0x438)
void EEUserInterface::PostLobbyUpdateForm()
{
    this->mThread->Post(0x438, NULL, NULL, true);
}

// FUNCTION: EXE 0x679645
// Posts refreshing a saved group button (message 0x435)
void EEUserInterface::PostRefreshSavedGroup(long inX)
{
    this->mThread->Post(0x435, reinterpret_cast<void*>(inX), NULL, true);
}

// FUNCTION: EXE 0x67965e
// Posts the load progress percentage; in multiplayer also tells the others at every 20 percent step
void EEUserInterface::SetLoadProgress(float inFraction)
{
    if (inFraction < 0.0f)
        inFraction = 0.0f;
    else if (inFraction > 1.0f)
        inFraction = 1.0f;
    unsigned long thePercent = (unsigned long)(inFraction * 100.0f);
    if (sLastLoadPercent / 20 < thePercent / 20 && gCommunications != NULL && gCommunications->GetNumHumanPlayers() > 1)
        gCommunications->ProcessLocalUserInput(new EEMLoadPercent(this->mLocalPlayer, thePercent), kSlotNone);
    sLastLoadPercent = thePercent;
    this->mThread->Post(0x40a, reinterpret_cast<void*>(thePercent), NULL, true);
}

// FUNCTION: EXE 0x679709
// Creates a load-percent message
// (EEMLoadPercent's inline ctor, after its first user)

// FUNCTION: EXE 0x67973d
// Posts a loading text (message 0x40b)
void EEUserInterface::SetLoadingText(long inX)
{
    this->mThread->Post(0x40b, reinterpret_cast<void*>(inX), NULL, true);
}

// FUNCTION: EXE 0x679756
// Posts the icon bar update (message 0x424) once until it is handled
void EEUserInterface::PostMessage424Once()
{
    if (this != NULL && !this->mIconBarUpdatePosted) {
        this->mThread->Post(0x424, NULL, NULL, true);
        this->mIconBarUpdatePosted = true;
    }
}

// FUNCTION: EXE 0x67977c
// Shows a lag state: a waiting message in the game, or the blinking lag button in the lobby
void EEUserInterface::ShowLagStatus(unsigned long inPlayer, bool inLagging)
{
    UWideString theText;
    gUIManager->LockUIForCopyData();
    if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormMainGame)) {
        if (inLagging) {
            gResource->GetString(IDS_WAITING_FOR_PLAYERS, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
        }
    } else if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormLobby)) {
        UIBlinkingButton* theButton = static_cast<UIBlinkingButton*>(gUIManager->GetCurrentForm()->GetControl(0x111));
        if (inPlayer == 0) {
            theButton->SetDisplayMode(kDisplayModeHidden);
        } else {
            theButton->SetDisplayMode(kDisplayModeShown);
            theButton->SetIsBlinking(true);
        }
    }
    gUIManager->UnlockUIForCopyData();
}

// FUNCTION: EXE 0x67985e
// Undoes the last action; returns 7 when there is none
long EEUserInterface::Undo()
{
    long theResult = kErrorOK;
    if (this->GetQueueSize(this->mUndoQueue, this->mUndoLock) > 0) {
        EEAction* theAction = this->PopBack(this->mUndoQueue, this->mUndoLock);
        theAction->Virtual8();
        this->PushBack(this->mRedoQueue, this->mRedoLock, theAction);
    } else {
        theResult = 7;
    }
    return theResult;
}

// FUNCTION: EXE 0x6798a3
// Resumes the graphics thread and waits until it runs
void EEUserInterface::ResumeGraphics()
{
    gGraphics->PostResume();
    for (bool theStarted = gGraphics->mIsPaused; theStarted != false; theStarted = gGraphics->mIsPaused)
        PlatSleep(1);
}

// FUNCTION: EXE 0x6798ce
// Handles the game window's messages: creation, close, size, activation (minimizing and suspending input while
// inactive), keys, IME and the lobby's custom messages
LRESULT CALLBACK EEUserInterface::WindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam)
{
    static float sMasterVolume = gSoundManager != NULL ? gSoundManager->GetMasterVolume() : 0.0f;
    static float sMusicVolume = gConductor != NULL ? gConductor->GetConductor().GetVolume() : 0.0f;
    if (Fn297ED0() && inMessage != WM_SIZE)
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    switch (inMessage) {
    case WM_CREATE: {
        HIMC theContext = ImmGetContext(inWindow);
        if (theContext != NULL) {
            ImmSetOpenStatus(theContext, TRUE);
            ImmReleaseContext(inWindow, theContext);
        }
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    }
    case WM_DESTROY:
    case WM_CLOSE:
        if (EEIsLobbyOpen())
            EELobbyShutdown();
        PostQuitMessage(0);
        break;
    case WM_MOVE:
        RedrawWindow(GetDesktopWindow(), NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_SIZE:
        if (inWParam == SIZE_MINIMIZED) {
            if (!sMinimized) {
                if (gConductor != NULL) {
                    sMusicVolume = gConductor->GetConductor().GetVolume();
                    gConductor->GetConductor().SetVolume(0.0f);
                }
                if (gSoundManager != NULL) {
                    sMasterVolume = gSoundManager->GetMasterVolume();
                    gSoundManager->SetMasterVolume(0.0f);
                }
                sMinimized = true;
            }
        } else if (inWParam == SIZE_RESTORED && sMinimized == true) {
            if (gConductor != NULL)
                gConductor->GetConductor().SetVolume(sMusicVolume);
            if (gSoundManager != NULL)
                gSoundManager->SetMasterVolume(sMasterVolume);
            sMinimized = false;
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(inWParam) != WA_INACTIVE) {
            WINDOWPLACEMENT thePlacement;
            memset(&thePlacement, 0, sizeof(thePlacement));
            thePlacement.length = sizeof(thePlacement);
            GetWindowPlacement(inWindow, &thePlacement);
            if (thePlacement.showCmd != SW_SHOWMINIMIZED) {
                if (gISKeyboard != NULL)
                    gISKeyboard->Resume();
                if (gISMouse != NULL)
                    gISMouse->Resume();
                if (gGameWindow != NULL && gMouse != NULL) {
                    gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1),
                                                    (float)(gGameWindow->GetHeight() >> 1)));
                }
                if (gMousePointer != NULL)
                    gMousePointer->mThread->Post(0x404, NULL, NULL, true);
            }
        }
        break;
    case WM_PAINT:
    case WM_ERASEBKGND:
        break;
    case WM_ACTIVATEAPP:
        if (inWParam == FALSE) {
            if (gUserInterface != NULL && !gUserInterface->mIsQuitting)
                CloseWindow(inWindow);
            if (gISKeyboard != NULL)
                gISKeyboard->Suspend();
            if (gISMouse != NULL)
                gISMouse->Suspend();
            if (gGameWindow != NULL && gMouse != NULL) {
                gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1),
                                                (float)(gGameWindow->GetHeight() >> 1)));
            }
            if (gMousePointer != NULL) {
                gMousePointer->mThread->Post(0x402, NULL, NULL, true);
                while (gMousePointer->mIsShown)
                    PlatSleep(1);
            }
        } else {
            if (gISKeyboard != NULL)
                gISKeyboard->Resume();
            if (gISMouse != NULL)
                gISMouse->Resume();
            if (gGameWindow != NULL && gMouse != NULL) {
                gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1),
                                                (float)(gGameWindow->GetHeight() >> 1)));
            }
            if (gMousePointer != NULL)
                gMousePointer->mThread->Post(0x404, NULL, NULL, true);
        }
        break;
    case WM_SETCURSOR:
        SetCursor(NULL);
        break;
    case WM_KEYDOWN:
        if (!sControlDown)
            sControlDown = inWParam == VK_CONTROL;
        if (!sAltDown)
            sAltDown = inWParam == VK_MENU;
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_KEYUP:
        if (sControlDown == true)
            sControlDown = inWParam != VK_CONTROL;
        if (sAltDown == true)
            sAltDown = inWParam != VK_MENU;
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_CHAR:
        if (!sControlDown || sAltDown == true) {
            char theKey = (char)inWParam;
            if (theKey != '\t' && theKey != 0x1b && theKey != '\b' && theKey != '\r')
                gUIManager->UpdateTranslatedKey(reinterpret_cast<const char*>(&inWParam), (short)inLParam);
        }
        break;
    case WM_IME_STARTCOMPOSITION:
        gUserInterface->OpenIMECompositionString(inWindow);
        break;
    case WM_IME_ENDCOMPOSITION:
        gUserInterface->CloseIMECompositionString();
        break;
    case WM_IME_COMPOSITION:
        if (inLParam & GCS_RESULTSTR)
            gUserInterface->AddIMEResultString(inWindow);
        else if (inLParam & (GCS_COMPSTR | GCS_CURSORPOS))
            gUserInterface->UpdateIMECompositionString(inWindow);
        break;
    case WM_SYSCOMMAND:
        if (inWParam == SC_SCREENSAVE || inWParam == SC_MONITORPOWER)
            break;
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_IME_SETCONTEXT:
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_IME_NOTIFY:
        switch (inWParam) {
        case IMN_CLOSESTATUSWINDOW:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        case IMN_OPENSTATUSWINDOW:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        case IMN_CHANGECANDIDATE:
            gUserInterface->UpdateIMECandidateList(inWindow);
            break;
        case IMN_CLOSECANDIDATE:
            gUserInterface->CloseIMECandidateWindow();
            break;
        case IMN_OPENCANDIDATE:
            gUIManager->LockUIForCopyData();
            gUserInterface->OpenIMECandidateWindow();
            gUserInterface->UpdateIMECandidateList(inWindow);
            gUIManager->UnlockUIForCopyData();
            break;
        case IMN_SETCONVERSIONMODE:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        case IMN_SETSENTENCEMODE:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        case IMN_SETOPENSTATUS:
            break;
        case IMN_GUIDELINE:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        default:
            return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
        }
        break;
    case WM_IME_CONTROL:
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_IME_COMPOSITIONFULL:
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_IME_SELECT:
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    case WM_IME_CHAR:
        break;
    case MM_MCINOTIFY:
        SECD::Notify(inWParam, inLParam);
        break;
    case 0x433:
        PostMessage(gGameWindow->GetOSRepresentation(), 0x434, 0, 0);
        if (gWonNet != NULL)
            gWonNet->FnCFFDD(0);
        break;
    case 0x434:
        if (gWonNet != NULL)
            gWonNet->FnCFFF2();
        break;
    default:
        return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
    }
    return 0;
}

// FUNCTION: EXE 0x679fbc
// Shows a loading text on the splash form
void EEUserInterface::ShowLoadingText(long inStringID)
{
    UWideString theText;
    gUIManager->LockUIForCopyData();
    if (gUIManager != NULL && gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormSplash)) {
        gResource->GetString(inStringID, theText);
        static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormSplash)->GetControl(kSplashLoadLabel))->SetText(theText);
    }
    gUIManager->UnlockUIForCopyData();
}

// FUNCTION: EXE 0x67a050
// Appends a thread message to the message vector (std::vector::push_back)
// (std::vector<EEUIThreadMessage>::push_back, after its first user)

// FUNCTION: EXE 0x67a07c
// Inserts copies of a thread message into the message vector (std::vector::insert)
// (std::vector<EEUIThreadMessage>::insert(iterator, size_type, const T&))

// FUNCTION: EXE 0x67a342
// Copies thread messages (std::copy, ICF-shared)
// (std::copy over EEUIThreadMessage, ICF-shared)

// FUNCTION: EXE 0x67a36a
// Constructs a thread message in place (std::_Construct)
// (std::_Construct<EEUIThreadMessage>)
