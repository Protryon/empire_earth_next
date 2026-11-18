// C:\EEX\Empire Earth\User Interface Thread\EEUserInterface.cpp (__FILE__ 0x4cf450). The UI thread: 0x276631..0x27a37e
// (EEUIPGSLineGraph.cpp ends at 0x276631). The ctor, ??_G, dtor, the thread's handlers (in address order), Run,
// HandleMessage (the 0x401..0x443 switch), the Post* members, the window procedure, then the template COMDATs of
// the message vector. Unit flags: the exe module set. Names are provisional unless the header says otherwise.
#include <windows.h>
extern "C" void SPIMSetMuted(bool inMuted);   // port: the mixer plug-in (lle/SEMixer.h)
#include "platform/PlatSim.h"
unsigned long EEMenuWidth();    // port: the menus' resolution (Empire Earth.cpp)
unsigned long EEMenuHeight();
#include <list>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "rust/audio/mixer.h"   // the lobby's updates (ee_screens lobby_form.rs)
#include "rust/game/screens.h"
#include "ee/EEUIFMainGame.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphics.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAction.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EESMMediaStrings.h"
#include <locale.h>
#include "lle/UICheckBox.h"
#include "lle/UIStatusBar.h"
#include "lle/UIBlinkingButton.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EEWonNet.h"
#include "ee/EERenderObject.h"
#include "ee/EEROUserInterface.h"
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
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESaveLoad.h"
#include "ee/EESMDialogMessage.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUIEWonNetServerSelectEvents.h"
#include "ee/EEUIFError.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEARust.h"
#include "ee/EEDatabaseTables.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEStatisticsRecord.h"
// free functions of other units (declared where they are used, as their own units do) (provisional names)
void DeleteEffectBatchBuffers();            // 0x102e1b (before the rasterizer is deactivated)
void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFlag);  // 0x140306 (Empire Earth.cpp)
void EEUnloadGameMedia();
void EEUpdateLandContinents();
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);
bool EEIsLobbyOpen();                       // 0x297eca (the external lobby is running)
void EESetQuitFlag();
void EEResetGraphicsValues();               // 0x10fc7e (before a screen capture without the cursor)
void EEPostGraphicsAll();
// CivBuilderEditCivilization 0x280faf, CivBuilderCreateCivilization 0x280ffd: ee/EECivilizationBuilder.h; EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h
void EELobbyShutdown();
// the lobby's LobbyUpdatePlayer, LobbyUpdateGameOptions, LobbyUpdateForm and LobbyShowTransferProgress: Rust's
// (rust/ee_screens.h, ee_screens lobby_form.rs)
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
static unsigned long sLastLoadPercent;      // 0x565370 (SetLoadProgress)
// Creates the UI thread object: its thread and the message-done event (the action, undo and redo queues and their
// locks are Rust's: ee_screens actions.rs)
EEUserInterface::EEUserInterface()
{
    this->mThread = TSScheduler::GetCurrentThread();
    this->mB05C = false;
    this->mIsUpdating = true;
    this->mIsQuitting = false;
    this->mRandomSeed = TMGetTime();
    this->mIconBarUpdatePosted = false;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 177);
}

// Destroys the UI thread object, releasing every queued, undoable and redoable action
EEUserInterface::~EEUserInterface()
{
    ee_screens_actions_clear(0);
    ee_screens_actions_clear(1);
    ee_screens_actions_clear(2);
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
}

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
    DeleteEffectBatchBuffers();
    theResult = gRasterizer->Deactivate();
    ESVERIFY_AT(theResult == keseOK, 252);
    gRasterizer = GERasterizer::GetRasterizer(inRasterizer);
    gGraphics->PostSetRasterizer(gRasterizer, inScreen);
    gUIManager->ChangeRasterizer(gRasterizer);
    this->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false);
}

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

// DEAD-IN-AOC: base exe 0x661aef. The WON login-done poster (message 0x441); dead in AoC like PostMOTD (no marker).
void EEUserInterface::PostLoginDone()
{
    this->mThread->Post(0x441, NULL, NULL, true);
}

// LeaveMap, CancelGame, CloseScenarioEditor, EnterScenarioEditor, StartLoadingFromEditor, ShowCivilizationBuilder and
// RestartGame are Rust's whole (ee_screens ui_game.rs; the thread's messages run them by ee_screens_ui_member).
// The game starts' and the loop's calls (EE_SCREENS_UG_OP_*) on the UI object inUI (ee_screens ui_game.rs)
extern "C" int64_t ee_screenscpp_ui_game(void* inUI, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUserInterface* theUI = static_cast<EEUserInterface*>(inUI);
    FSFile* theFile = reinterpret_cast<FSFile*>(inA);
    EEServer* theServer = static_cast<EEServer*>(gCommunications);
    switch (inOp) {
    case EE_SCREENS_UG_OP_GRAPHICS: inA != 0 ? theUI->PauseGraphics() : theUI->ResumeGraphics(); break;
    case EE_SCREENS_UG_OP_WORLD_LOCK: inA != 0 ? gWorld.mLock->WriteLock() : gWorld.mLock->WriteUnlock(); break;
    case EE_SCREENS_UG_OP_SCREEN_ACTIVE: gGameScreenActive = true; break;
    case EE_SCREENS_UG_OP_CONTINENTS:
        gContinentManager.Clear(gWorld.mMap);
        gContinentManager.BuildRegions(gWorld.mMap);
        break;
    case EE_SCREENS_UG_OP_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_SCREENS_UG_OP_PLAYER: {
        EEPlayer* thePlayer = gWorld.mPlayers[inA];
        switch (inB) {
        case EE_SCREENS_UG_PLAYER_EXISTS: return thePlayer != NULL;
        case EE_SCREENS_UG_PLAYER_HERDS: thePlayer->RegisterHerds(); thePlayer->AssignHerds(); break;
        case EE_SCREENS_UG_PLAYER_TOWNS: thePlayer->RegisterTowns(); thePlayer->AssignTowns(); break;
        case EE_SCREENS_UG_PLAYER_COMPUTER: return thePlayer->mComputerPlayer != NULL;
        case EE_SCREENS_UG_PLAYER_CENTRES: thePlayer->mComputerPlayer->RefreshResourceCenters(); break;
        case EE_SCREENS_UG_PLAYER_TOWN_GROUPS: thePlayer->RefreshTownGroups(); break;
        }
        break;
    }
    case EE_SCREENS_UG_OP_PATH_RESET: gPathManager.Reset(); break;
    case EE_SCREENS_UG_OP_SIGHT_ON: gSightUpdatesEnabled = true; break;
    case EE_SCREENS_UG_OP_RESTART_UNITS: gWorld.RestartUnits(); break;
    case EE_SCREENS_UG_OP_LAND_CONTINENTS: EEUpdateLandContinents(); break;
    case EE_SCREENS_UG_OP_RESIZE_GAME: theUI->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, inA != 0, inB != 0); break;
    case EE_SCREENS_UG_OP_SWAP_MAIN: gUIManager->SwapForm(gUIManager->mMainGameForm); break;
    case EE_SCREENS_UG_OP_MINI_MAP: gMiniMap->Redraw(); gMiniMap->Refresh(); break;
    case EE_SCREENS_UG_OP_GAME_VIEW: {
        GERectangle theRectangle;
        theRectangle.SetLeft(kGameViewLeft);
        theRectangle.SetBottom(kGameViewBottom);
        theRectangle.SetTop(kGameViewTop);
        theRectangle.SetRight(kGameViewRight);
        EESetGameViewRectangle(theRectangle);
        break;
    }
    case EE_SCREENS_UG_OP_AMBIENT:
        gAmbientSoundManager.SetEnabled(gGamePreferences.mSettings.mAmbientSounds);
        gAmbientSoundManager.Restart();
        break;
    case EE_SCREENS_UG_OP_PHYSICS_POST: gPhysics->mThread->Post((unsigned long)inA, NULL, NULL, true); break;
    case EE_SCREENS_UG_OP_MUSIC: gMusicManager->Start(); break;
    case EE_SCREENS_UG_OP_PREPARE_SAVE: gWorld.mMap->PrepareSave(); break;
    case EE_SCREENS_UG_OP_SAVE_SCENARIO:
        return SaveGame(UString("ScenDefault") + gScenarioExtension, TMGetTime(), true,
                        UString("data\\") + UString("Scenarios\\"));
    case EE_SCREENS_UG_OP_SAVE_ERROR: gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true); break;
    case EE_SCREENS_UG_OP_SCENARIO_OPTIONS: {
        gWorld.mGameOptions.mLoadType = 1;
        UWideString theName(UString("ScenDefault") + gScenarioExtension);
        gWorld.mGameOptions.mMapType = theName;
        break;
    }
    case EE_SCREENS_UG_OP_MAX_PLAYERS: gWorld.SetNumPlayers(gWorld.mMaxPlayers); break;
    case EE_SCREENS_UG_OP_COLLECT_COMPUTERS: gArtificialIntelligence->CollectComputerPlayers(); break;
    case EE_SCREENS_UG_OP_LOAD_HEADER: {
        EEPSaveGameHeader theHeader;
        theHeader.SetMajorVersion(kSaveGameMajorVersion);
        theHeader.SetMinorVersion(kSaveGameMinorVersion);
        return theHeader.LoadFromFile(*theFile) == kErrorOK;
    }
    case EE_SCREENS_UG_OP_TIMER_RUNNING: return gPhysics->mTimerRunning;
    case EE_SCREENS_UG_OP_SLEEP: PlatSleep((unsigned long)inA); break;
    case EE_SCREENS_UG_OP_NEW_SERVER:
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        break;
    case EE_SCREENS_UG_OP_B13D_OFF: gUIManager->mB13D = false; break;
    case EE_SCREENS_UG_OP_STATE_CLEAR: gGameState.Clear(); break;
    case EE_SCREENS_UG_OP_STOP_SOUNDS: EEStopAllSounds(); break;
    case EE_SCREENS_UG_OP_SWAP_SPLASH: gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash)); break;
    case EE_SCREENS_UG_OP_SERVER_LOAD:
        theServer->AllowNewPlayers(false);
        theServer->mNumLoadResponses = 0;
        theServer->mLoadState = 1;
        theServer->SetGameState(2);
        theServer->SetNumHumanPlayers(1);
        theServer->SetIsMultiplayerSession(false);
        break;
    case EE_SCREENS_UG_OP_OPTIONS_LOCK: inA != 0 ? gGameOptions.mLock->WriteLock() : gGameOptions.mLock->WriteUnlock(); break;
    case EE_SCREENS_UG_OP_OPTIONS_DEFAULTS: gGameOptions.SetDefaults(); break;
    case EE_SCREENS_UG_OP_FILE_IS_SCENARIO: {
        unsigned long theLength = theFile->GetFile().GetLength();
        return theFile->GetFile().Find(gScenarioExtension, 0) < theLength;
    }
    case EE_SCREENS_UG_OP_LOAD_OPTIONS: {
        gGameOptions.mLoadType = (long)inB;
        UWideString theName(theFile->GetFile());
        gGameOptions.mMapType = theName;
        break;
    }
    case EE_SCREENS_UG_OP_POST_LOAD: gDataPreLoading->PostLoadGame(inA != 0, inB != 0); break;
    case EE_SCREENS_UG_OP_INVALID_SAVE: {
        UWideString theMessage;
        gResource->GetString(IDS_SAVE_GAME_ISNT_VALID, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
        break;
    }
    case EE_SCREENS_UG_OP_DELETE_FILE: delete theFile; break;
    case EE_SCREENS_UG_OP_INPUT_RESUME: gISMouse->Resume(); gISKeyboard->Resume(); break;
    case EE_SCREENS_UG_OP_THREAD_NAME: TSThread::SetThreadName(PlatCurrentThreadId(), gUserInterfaceThreadName); break;
    case EE_SCREENS_UG_OP_LOCALE: {
        UWideString theLocale;
        gResource->GetString(IDS_ENGLISH, theLocale);
        setlocale(LC_ALL, theLocale);
        setlocale(LC_NUMERIC, "English");
        // port (decision D6): game text is UTF-8, so the C runtime's character classes stay the "C" locale's; under
        // the language's code page tolower/toupper/isalpha would have changed the bytes of a multi-byte character
        setlocale(LC_CTYPE, "C");
        setlocale(LC_ALL, NULL);
        break;
    }
    case EE_SCREENS_UG_OP_CREATE_RENDER_OBJECT: return theUI->CreateRenderObject();
    case EE_SCREENS_UG_OP_POINTER_POST:
        gMousePointer->mThread->Post((unsigned long)inA, inA == 0x405 ? gRasterizer->GetPrimarySurface(gScreenIndex) : NULL,
                                     NULL, true);
        break;
    case EE_SCREENS_UG_OP_GRAPHICS_START: gGraphics->PostSetFlag54(false); gGraphics->PostResume(); break;
    case EE_SCREENS_UG_OP_FPU:
        if (inA == 0) {
            UMSetFloatingPointPrecision(kFPPrecision53);
            break;
        }
        if (UMGetFloatingPointPrecision() != kFPPrecision53)
            UMSetFloatingPointPrecision(kFPPrecision53);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        break;
    case EE_SCREENS_UG_OP_QUITTING: return theUI->mIsQuitting;
    case EE_SCREENS_UG_OP_QUEUE_LOCK:
        inA != 0 ? theUI->mThread->LockMessageQueue() : theUI->mThread->UnlockMessageQueue();
        break;
    case EE_SCREENS_UG_OP_RECEIVE: {
        EEUIThreadMessage theMessage;
        long theResult = theUI->mThread->Receive(theMessage.mMessage, theMessage.mParameter1, theMessage.mParameter2);
        int64_t* theOut = reinterpret_cast<int64_t*>(inC);
        theOut[0] = theMessage.mMessage;
        theOut[1] = reinterpret_cast<int64_t>(theMessage.mParameter1);
        theOut[2] = reinterpret_cast<int64_t>(theMessage.mParameter2);
        return theResult;
    }
    case EE_SCREENS_UG_OP_HANDLE: theUI->HandleMessage((unsigned long)inA, (LONG_PTR)inB, (LONG_PTR)inC); break;
    case EE_SCREENS_UG_OP_UPDATING: return theUI->mIsUpdating != 0;
    case EE_SCREENS_UG_OP_UPDATE_INTERFACE: theUI->UpdateInterface(); break;
    case EE_SCREENS_UG_OP_FREE_CAMERA: return gFreeCamera;
    case EE_SCREENS_UG_OP_TIME: return TMGetTime();
    case EE_SCREENS_UG_OP_CAMERA: {
        uint32_t theBits = (uint32_t)inA;
        float theSeconds;
        memcpy(&theSeconds, &theBits, 4);
        gWorld.mCameraControl.Update(theSeconds);
        break;
    }
    case EE_SCREENS_UG_OP_SIM_RUN: return gPlatSimRun;
    case EE_SCREENS_UG_OP_RELEASE_SCENE: gGraphics->PostReleaseScene(); gGraphics->PostMessage50AAndWait(); break;
    case EE_SCREENS_UG_OP_ACTIONS_CLEAR: ee_screens_actions_clear((uint32_t)inA); break;
    case EE_SCREENS_UG_OP_WAIT_PRELOAD: gDataPreLoading->WaitForMessageQueue(); break;
    case EE_SCREENS_UG_OP_WORLD_CLEAR: gWorld.Clear(); break;
    case EE_SCREENS_UG_OP_PHYSICS_INIT:
        if (inA == 0) {
            gPhysics->PostInit();
        } else {
            gPhysics->RunOwedTicks();
            gPhysics->Init();
        }
        break;
    case EE_SCREENS_UG_OP_SCREEN_INACTIVE: gGameScreenActive = false; break;
    case EE_SCREENS_UG_OP_RESIZE_MENU: theUI->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), inA != 0, inB != 0); break;
    case EE_SCREENS_UG_OP_NOT_MAIN: return gUIManager->GetCurrentForm() != gUIManager->GetMainGameForm();
    case EE_SCREENS_UG_OP_POP: gUIManager->Pop(); break;
    case EE_SCREENS_UG_OP_SWAP: gUIManager->SwapForm(reinterpret_cast<UIForm*>(inA)); break;
    case EE_SCREENS_UG_OP_FORM: return reinterpret_cast<int64_t>(gUIManager->GetFormByID((unsigned long)inA));
    case EE_SCREENS_UG_OP_ONE_RECORD: return gWorld.mStatistics.GetRecords().size() == 1;
    case EE_SCREENS_UG_OP_PUSH: gUIManager->Push(reinterpret_cast<UIForm*>(inA)); break;
    case EE_SCREENS_UG_OP_SERVER_LOST:
        return gCommunications != NULL && gCommunications->IsClient() &&
               static_cast<EEClient*>(gCommunications)->mServerLostAfterLoading;
    case EE_SCREENS_UG_OP_UNLOAD_MEDIA: EEUnloadGameMedia(); break;
    case EE_SCREENS_UG_OP_NEW_CLIENT:
        if (gCommunications != NULL) {
            delete gCommunications;
            gCommunications = new EEClient;
            static_cast<EEClient*>(gCommunications)->StartThreads();
        }
        break;
    case EE_SCREENS_UG_OP_LOBBY_OPEN: return EEIsLobbyOpen();
    case EE_SCREENS_UG_OP_QUIT_FLAG: EESetQuitFlag(); break;
    case EE_SCREENS_UG_OP_BOOT_CLIENT: {
        EEABootClientFromCanceledGame* theAction = new EEABootClientFromCanceledGame((long)inA, true);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_UG_OP_CREATE_EDITOR: gUIManager->CreateScenarioEditor(); break;
    case EE_SCREENS_UG_OP_LOAD_SCENDEFAULT:
        return EELoadSavedGameFile(UString("ScenDefault") + gScenarioExtension, true, true,
                                   UString("data\\") + UString("Scenarios\\"), 0);
    case EE_SCREENS_UG_OP_STOP_ONE_SHOTS: gSoundManager->StopAllOneShotSounds(); break;
    case EE_SCREENS_UG_OP_SWAP_EDITOR: gUIManager->SwapForm(gUIManager->mScenarioEditorForm); break;
    case EE_SCREENS_UG_OP_LOAD_ERROR: {
        UWideString theMessage;
        gResource->GetString(IDS_THERE_WAS_ERROR_TRYING_TO_LOAD, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, NULL, true);
        break;
    }
    case EE_SCREENS_UG_OP_EDITOR_PAUSES:
        dynamic_cast<EEServer*>(gCommunications)->PauseGame();
        gMusicManager->Pause();
        gAmbientSoundManager.StopSounds();
        gAmbientSoundManager.Pause();
        EEUIECLEFormManager::Clear();
        break;
    case EE_SCREENS_UG_OP_GRAPHICS_PAUSED: return gGraphics->mIsPaused;
    case EE_SCREENS_UG_OP_CB_MUSIC: {
        static UWideString sMusic;   // (the UI thread's: kept across the builder's run)
        UIForm* theBuilder = gUIManager->GetFormByID(kFormCivBuilder);
        if (inA == 0)
            sMusic = theBuilder->GetMusicFileName();
        else
            theBuilder->SetMusicFileName(inA == 1 ? UWideString("") : sMusic);
        break;
    }
    case EE_SCREENS_UG_OP_COPY_LOCK: inA != 0 ? gUIManager->LockUIForCopyData() : gUIManager->UnlockUIForCopyData(); break;
    case EE_SCREENS_UG_OP_CAN_BUY_CIV: return gWorld.mGameOptions.CanBuyCivilization();
    case EE_SCREENS_UG_OP_CUSTOM_CIVS: return gWorld.mGameOptions.mAllowCustomCivs;
    case EE_SCREENS_UG_OP_CIV_CHOSEN: return gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen;
    case EE_SCREENS_UG_OP_CIV_CHOOSE: {
        UIForm* theForm = gUIManager->GetFormByID(kFormPredefinedCiv);
        EESetCivBuilderButtonBlinking(false);
        gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationChosen = true;
        gWorld.mPlayers[gWorld.mCurrentPlayer]->mCivilizationLocked = true;
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_PDCS_OPEN);   // (ee_screens civ_choice.rs)
        theForm->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        theEvent = EECreateScreenEvent(EE_SCREENS_PDCS_CLOSE);
        theForm->SetFormCloseEvent(theEvent);
        theEvent->Dereference();
        gUIManager->Push(theForm);
        break;
    }
    case EE_SCREENS_UG_OP_CIV_EDIT: CivBuilderEditCivilization(); break;
    case EE_SCREENS_UG_OP_CIV_CREATE: CivBuilderCreateCivilization(reinterpret_cast<EECivilization*>(inA), inB != 0); break;
    }
    return 0;
}

// Starts a test of the edited scenario: rebuilds continents, herds, towns and AI centres, restarts units and
// switches to the main game form (ee_screens ui_game.rs)
void EEUserInterface::TestScenario()
{
    ee_screens_ui_test_scenario(this);
}

// Closes the IME composition string
void EEUserInterface::CloseIMECompositionString()
{
    gUIManager->CloseIMECompositionString();
}

// Saves the edited scenario as ScenDefault and starts testing it like a game (reporting a save error; ee_screens
// ui_game.rs)
void EEUserInterface::SaveAndTestScenario()
{
    ee_screens_ui_save_and_test_scenario(this);
}

// Creates the UI's render object and pushes it on the graphics thread
long EEUserInterface::CreateRenderObject()
{
    gU56534C = new EEROUserInterface(gUIManager);
    gGraphics->PostPushRenderObject(gU56534C);
    return kErrorOK;
}

// Loads a saved game or scenario file: checks its header, starts a local server and posts the load; reports
// an invalid file (ee_screens ui_game.rs)
void EEUserInterface::LoadSavedGame(FSFile* inFile)
{
    ee_screens_ui_load_saved_game(this, inFile);
}

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

// Runs the UI thread: collects and handles its messages, keeps the FPU in 53-bit mode, updates the interface
// and the free camera each pass until quitting (ee_screens ui_game.rs)
long EEUserInterface::Run(void* inParameter)
{
    return (long)ee_screens_ui_run(this);
}

// Pauses the graphics thread and waits until it is paused
void EEUserInterface::PauseGraphics()
{
    gGraphics->PostPause();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        PlatSleep(1);
}

// Runs the queued actions under the UI copy lock and the world's write lock; undoable ones go on the undo list (at
// most 100) and clear the redo list (the original's queue helpers ClearQueue, GetQueueSize, PeekBack, PopFront,
// PopBack and PushBack with it: ee_screens actions.rs)
void EEUserInterface::ProcessActions()
{
    ee_screens_actions_process();
}

// The action queues' host (actions.rs): an action's reference, release, run, undo and undoable flag; the locks
extern "C" uint32_t ee_screenscpp_action(uint32_t inOp, void* inAction)
{
    EEAction* theAction = static_cast<EEAction*>(inAction);
    switch (inOp) {
    case EE_SCREENS_ACTION_REFERENCE:
        theAction->Reference();
        break;
    case EE_SCREENS_ACTION_DEREFERENCE:
        theAction->Dereference();
        break;
    case EE_SCREENS_ACTION_RUN:
        theAction->Virtual4();
        break;
    case EE_SCREENS_ACTION_UNDO:
        theAction->Virtual8();
        break;
    case EE_SCREENS_ACTION_UNDOABLE:
        return theAction->mIsUndoable;
    case EE_SCREENS_ACTION_LOCK:
        gUIManager->LockUIForCopyData();
        gWorld.mLock->WriteLock();
        break;
    case EE_SCREENS_ACTION_UNLOCK:
        gWorld.mLock->WriteUnlock();
        gUIManager->UnlockUIForCopyData();
        break;
    }
    return 0;
}

// Passes the input method's composition string (PLAT_EVENT_TEXT_EDITING; the original read it with
// ImmGetCompositionStringA), its attributes (the edited part as the target clause, the rest as input: IMM's
// ATTR_TARGET_CONVERTED / ATTR_INPUT) and the cursor to the UI
void EEUserInterface::UpdateIMECompositionString(const char* inText, long inCursor, long inLength)
{
    UWideString theString(inText);
    unsigned long theCount = (unsigned long)strlen(inText);
    std::string theAttributes(theCount, '\2');   // ATTR_CONVERTED (non-zero: the attribute string ends at a 0)
    for (unsigned long i = 0; i < theCount; i++)
        if ((long)i >= inCursor && (long)i < inCursor + inLength)
            theAttributes[i] = 1;
    gUIManager->ChangeIMECompositionString(theString, UString(theAttributes.c_str()),
                                          (unsigned long)(inCursor < 0 ? 0 : inCursor));
}

// Passes the input method's result string to the UI (the committed composition)
void EEUserInterface::AddIMEResultString(const char* inText)
{
    gUIManager->AddIMEResultString(inText);
}

// Handles one UI thread message (0x401..0x443): the switch is Rust's (ee_screens ui_thread.rs), over the host below
void EEUserInterface::HandleMessage(unsigned long inMessage, LONG_PTR inParameter1, LONG_PTR inParameter2)
{
    ee_screens_ui_message(this, (uint32_t)inMessage, inParameter1, inParameter2);
}

// The thread messages' calls (EE_SCREENS_UI_OP_*) on the UI object inUI: the UI manager and its forms, the locks,
// the graphics thread, the thread object's handlers; the pointers are the messages' parameters
extern "C" int64_t ee_screenscpp_ui_message(void* inUI, uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    EEUserInterface* theUI = static_cast<EEUserInterface*>(inUI);
    EEUIFMainGame* theMain = gUIManager != NULL ? gUIManager->mMainGameForm : NULL;
    UIForm* theForm = reinterpret_cast<UIForm*>(inA);
    switch (inOp) {
    case EE_SCREENS_UI_OP_PAUSE: theUI->PauseGraphics(); break;
    case EE_SCREENS_UI_OP_RESUME: theUI->ResumeGraphics(); break;
    case EE_SCREENS_UI_OP_LOCK:
        if (inA == 0) gUIManager->LockUIForCopyData();
        else if (inA == 1) gWorld.mLock->ReadLock();
        else gWorld.mLock->WriteLock();
        break;
    case EE_SCREENS_UI_OP_UNLOCK:
        if (inA == 0) gUIManager->UnlockUIForCopyData();
        else if (inA == 1) gWorld.mLock->ReadUnlock();
        else gWorld.mLock->WriteUnlock();
        break;
    case EE_SCREENS_UI_OP_HAS_MANAGER: return gUIManager != NULL;
    case EE_SCREENS_UI_OP_MAIN_FORM: return (int64_t)theMain;
    case EE_SCREENS_UI_OP_EDITOR_FORM: return (int64_t)gUIManager->mScenarioEditorForm;
    case EE_SCREENS_UI_OP_CURRENT_FORM: return (int64_t)gUIManager->GetCurrentForm();
    case EE_SCREENS_UI_OP_FORM_BY_ID: return (int64_t)gUIManager->GetFormByID(inA);
    case EE_SCREENS_UI_OP_SWAP: gUIManager->SwapForm(theForm); break;
    case EE_SCREENS_UI_OP_PUSH: gUIManager->Push(theForm); break;
    case EE_SCREENS_UI_OP_POP: gUIManager->Pop(); break;
    case EE_SCREENS_UI_OP_POP_TO: gUIManager->PopTo(theForm); break;
    case EE_SCREENS_UI_OP_ON_STACK: return gUIManager->IsFormOnStack(theForm);
    case EE_SCREENS_UI_OP_VISIBLE: return gUIManager->IsFormVisible(theForm);
    case EE_SCREENS_UI_OP_REDRAW: gUIManager->RedrawAll(); break;
    case EE_SCREENS_UI_OP_STATUS_VALUE:
        static_cast<UIStatusBar*>(theForm->GetControl(inB))->SetCurrentRangeValue(inC);
        break;
    case EE_SCREENS_UI_OP_RESIZE_GAME: theUI->ResizeGameScreen(gGameWindowWidth, gGameWindowHeight, true, true); break;
    case EE_SCREENS_UI_OP_RESIZE_MENU: theUI->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false); break;
    case EE_SCREENS_UI_OP_SCREEN_ACTIVE: gGameScreenActive = true; break;
    case EE_SCREENS_UI_OP_POST_LOAD_GAME: gDataPreLoading->PostLoadGame(false, false); break;
    case EE_SCREENS_UI_OP_SHOW_EPOCH_NAME: theUI->ShowEpochName(); break;
    case EE_SCREENS_UI_OP_BACKGROUND_MEDIA: theMain->Rust(EE_SCREENS_MF_DO_UPDATE_MEDIA, 1); break;
    case EE_SCREENS_UI_OP_SET_UPDATING: theUI->mIsUpdating = inA; break;
    case EE_SCREENS_UI_OP_SET_QUITTING: theUI->mIsQuitting = true; break;
    case EE_SCREENS_UI_OP_LOAD_SAVED_GAME: theUI->LoadSavedGame(reinterpret_cast<FSFile*>(inA)); break;
    case EE_SCREENS_UI_OP_PROCESS_EVENT:
        reinterpret_cast<UIEvent*>(inA)->ProcessEvent(NULL, reinterpret_cast<UIEntity*>(inB));
        reinterpret_cast<UIEvent*>(inA)->Dereference();
        break;
    case EE_SCREENS_UI_OP_GFX_POST_PAUSE: return gGraphics->PostPause() == kErrorOK;
    case EE_SCREENS_UI_OP_GFX_PAUSED: return gGraphics->mIsPaused;
    case EE_SCREENS_UI_OP_SLEEP: PlatSleep(1); break;
    case EE_SCREENS_UI_OP_GFX_POST_RESUME: gGraphics->PostResume(); break;
    case EE_SCREENS_UI_OP_QUICK_LOAD: {
        EEAQuickSaveLoad* theAction = reinterpret_cast<EEAQuickSaveLoad*>(inA);
        theAction->mIsLoad = true;
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_UI_OP_LOADING_TEXT: theUI->ShowLoadingText(inA); break;
    case EE_SCREENS_UI_OP_SAVE_AND_TEST: theUI->SaveAndTestScenario(); break;
    case EE_SCREENS_UI_OP_RESET_GFX_VALUES: EEResetGraphicsValues(); break;
    case EE_SCREENS_UI_OP_SCREEN_CAPTURE: {
        EEScreenCapture* theCapture = reinterpret_cast<EEScreenCapture*>(inA);
        ESVERIFY_AT(TSScheduler::CreateEvent(theCapture->mCaptureComplete) == keseOK, 1956);
        gGraphics->PostScreenShot(theCapture);
        theCapture->mCaptureComplete->Wait(kTSWaitInfinite);
        TSScheduler::DestroyEvent(theCapture->mCaptureComplete);
        if (theCapture->mWidth != -1)
            theCapture->mSurface->Dereference();
        delete theCapture;
        break;
    }
    case EE_SCREENS_UI_OP_POST_GFX_ALL: EEPostGraphicsAll(); break;
    case EE_SCREENS_UI_OP_CLOSE_EDITOR: ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_CLOSE_EDITOR, 0, 0, 0); break;
    case EE_SCREENS_UI_OP_ENTER_EDITOR: ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_ENTER_EDITOR, inA != 0, 0, 0); break;
    case EE_SCREENS_UI_OP_CANCEL_GAME: ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_CANCEL_GAME, 0, 0, 0); break;
    case EE_SCREENS_UI_OP_LOAD_FROM_EDITOR: ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_LOAD_FROM_EDITOR, 0, 0, 0); break;
    case EE_SCREENS_UI_OP_TEST_SCENARIO: theUI->TestScenario(); break;
    case EE_SCREENS_UI_OP_REFRESH_GROUP: theMain->mRefreshGroup = true; break;
    case EE_SCREENS_UI_OP_ERROR_HAS_CALLBACK: return reinterpret_cast<EEUIFErrorCallbackData*>(inA)->mCallback != NULL;
    case EE_SCREENS_UI_OP_ERROR_CALLBACK: {
        EEUIFErrorCallbackData* theData = reinterpret_cast<EEUIFErrorCallbackData*>(inA);
        theData->mCallback(static_cast<UIForm*>(theData->mData));
        break;
    }
    case EE_SCREENS_UI_OP_DELETE_FORM: delete theForm; break;
    case EE_SCREENS_UI_OP_DELETE_ERROR_DATA: delete reinterpret_cast<EEUIFErrorCallbackData*>(inA); break;
    case EE_SCREENS_UI_OP_CHAT_SHOW: {
        EEChatLogEntry* theEntry = reinterpret_cast<EEChatLogEntry*>(inA);
        theMain->DisplayChat(theEntry->mColor, theEntry->mName, theEntry->mText, theEntry->mTime, theEntry->mType);
        break;
    }
    case EE_SCREENS_UI_OP_CHAT_TYPE: return reinterpret_cast<EEChatLogEntry*>(inA)->mType;
    case EE_SCREENS_UI_OP_CHAT_LOG: EEUIECLEFormManager::AddEntry(*reinterpret_cast<EEChatLogEntry*>(inA)); break;
    case EE_SCREENS_UI_OP_DELETE_CHAT: delete reinterpret_cast<EEChatLogEntry*>(inA); break;
    case EE_SCREENS_UI_OP_MOVIE_PLAYING: return gGraphics->mIsMoviePlaying;
    case EE_SCREENS_UI_OP_PLAY_MOVIE:
        return gGraphics->PlayMovie(*reinterpret_cast<FSFileSpec*>(inA), reinterpret_cast<UReferenceCounter*>(inB)) == kErrorOK;
    case EE_SCREENS_UI_OP_SET_POINTER: gMouse->mPointer = inA; break;
    case EE_SCREENS_UI_OP_STOP_MOVIE: gGraphics->PostStopMovie(); break;
    case EE_SCREENS_UI_OP_REPOST_MOVIE:
        gUserInterface->PostPlayMovie(*reinterpret_cast<FSFileSpec*>(inA), reinterpret_cast<UReferenceCounter*>(inB));
        break;
    case EE_SCREENS_UI_OP_DEREFERENCE: reinterpret_cast<UReferenceCounter*>(inA)->Dereference(); break;
    case EE_SCREENS_UI_OP_DELETE_SPEC: delete reinterpret_cast<FSFileSpec*>(inA); break;
    case EE_SCREENS_UI_OP_SHOW_INFORMATION: theMain->ShowInformation(reinterpret_cast<EEUIFMGInformation*>(inA)); break;
    case EE_SCREENS_UI_OP_DIALOG_LOG: {
        EEChatLogEntry* theEntry = new EEChatLogEntry;
        theEntry->mText = reinterpret_cast<EESMDialogMessage*>(inA)->mText;
        theEntry->mType = 1;
        EEUIECLEFormManager::AddEntry(*theEntry);
        delete theEntry;
        break;
    }
    case EE_SCREENS_UI_OP_DELETE_DIALOG: delete reinterpret_cast<EESMDialogMessage*>(inA); break;
    case EE_SCREENS_UI_OP_HOST_GONE: {
        EESetQuitFlag();
        EEABootClientFromCanceledGame* theAction = new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        static_cast<NESynchronousClient*>(gCommunications)->Disconnect();
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
        break;
    }
    case EE_SCREENS_UI_OP_UPDATE_ICON_BAR:
        theMain->Rust(EE_SCREENS_MF_DO_ICON_BAR);
        theUI->mIconBarUpdatePosted = false;
        break;
    case EE_SCREENS_UI_OP_CHANGE_RASTERIZER: theUI->ChangeRasterizer(inA, inB); break;
    case EE_SCREENS_UI_OP_INFORMATION:
        theMain->Rust(EE_SCREENS_MF_DO_INFO_PAUSE, inA != 0);
        break;
    case EE_SCREENS_UI_OP_CIV_BUILDER:
        // (STAND-IN, owner 2026-11-01: the flag is the low byte of the raw dword 0x278b46 pushes)
        ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_CIV_BUILDER, inC != 0 ? 0 : inA, inC != 0 ? 0 : (uint8_t)inB, inC != 0);
        break;
    case EE_SCREENS_UI_OP_POST_EVENT: gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(inA), NULL); break;
    case EE_SCREENS_UI_OP_HIDE_MESSAGES: theMain->Rust(EE_SCREENS_MF_DO_HIDE_MESSAGES); break;
    case EE_SCREENS_UI_OP_SELECT_TAB: gUIManager->mScenarioEditorForm->SelectTab((unsigned char)inA, inB); break;
    case EE_SCREENS_UI_OP_RESTART: ee_screens_ui_member(theUI, EE_SCREENS_UG_DO_RESTART, 0, 0, 0); break;
    case EE_SCREENS_UI_OP_SAVED_GROUP: static_cast<EEUIBSavedGroup*>(theMain->GetControl(inA))->Refresh(); break;
    case EE_SCREENS_UI_OP_SELECT_SERVER: theUI->OpenSelectServer(); break;
    case EE_SCREENS_UI_OP_MOTD:
        theUI->ShowMessageOfTheDay(*reinterpret_cast<UWideString*>(inA), *reinterpret_cast<UWideString*>(inB));
        break;
    case EE_SCREENS_UI_OP_DELETE_WSTRING: delete reinterpret_cast<UWideString*>(inA); break;
    case EE_SCREENS_UI_OP_LOBBY:
        if (inA == 0) ee_screens_lobby_update_player(inB, inC);
        else if (inA == 1) ee_screens_lobby_update_game_options();
        else if (inA == 2) ee_screens_lobby_show_transfer_progress((uint8_t)inB, (uint8_t)inC);
        else ee_screens_lobby_update_form();
        break;
    case EE_SCREENS_UI_OP_LAG: theUI->ShowLagStatus(inA, inB != 0); break;
    case EE_SCREENS_UI_OP_QUEUE_DONE: theUI->mMessageQueueDone->Set(); break;
    }
    return 0;
}

// Updates the interface: keyboard and mouse input, the UI under the world read lock, then the queued actions
void EEUserInterface::UpdateInterface()
{
    gISKeyboard->Refresh();
    gMouse->mInputLock->Enter();
    gISMouse->Refresh();
    gMouse->Update();
    gMouse->mInputLock->Leave();
    gWorld.mLock->ReadLock();
    if (!EELobbyIsShown())
        gUIManager->UpdateUI(gMouse->mPosition);
    gWorld.mLock->ReadUnlock();
    this->ProcessActions();
}

// Queues an action for the UI thread (taking a reference; ee_screens actions.rs)
void EEUserInterface::PostAction(EEAction* inAction)
{
    ee_screens_actions_post(inAction);
}

// Opens the IME composition string
void EEUserInterface::OpenIMECompositionString()
{
    gUIManager->OpenIMECompositionString();
}

// Redoes the last undone action; returns 7 when there is none (ee_screens actions.rs)
long EEUserInterface::Redo()
{
    return ee_screens_actions_redo();
}

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

// Posts a background media update (message 0x403)
void EEUserInterface::PostUpdateBackgroundMedia()
{
    this->mThread->Post(0x403, NULL, NULL, true);
}

// Posts showing the epoch name (message 0x404)
void EEUserInterface::PostShowEpochName()
{
    this->mThread->Post(0x404, NULL, NULL, true);
}

// Posts a scenario dialog message (message 0x422)
void EEUserInterface::PostDialogMessage(const EESMDialogMessage& inMessage)
{
    this->mThread->Post(0x422, new EESMDialogMessage(inMessage), NULL, true);
}

// Posts selecting a scenario editor tab (message 0x421)
void EEUserInterface::PostSelectEditorTab(long inX, long inY)
{
    this->mThread->Post(0x421, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// Posts starting the scenario test (message 0x40f)
void EEUserInterface::StartScenarioTest()
{
    this->mThread->Post(0x40f, NULL, NULL, true);
}

// Posts a display change (message 0x429)
void EEUserInterface::PostDisplayChanged(bool inFlag)
{
    this->mThread->Post(0x429, (void*)inFlag, NULL, true);
}

// Posts cancelling the game (message 0x412)
void EEUserInterface::PostCancelGame()
{
    this->mThread->Post(0x412, NULL, NULL, true);
}

// Posts that loading the game failed (message 0x430)
void EEUserInterface::PostLoadGameFailed(bool inFlag)
{
    this->mThread->Post(0x430, NULL, (void*)inFlag, true);
}

// Posts leaving the scenario editor (message 0x410)
void EEUserInterface::LeaveScenarioEditor()
{
    this->mThread->Post(0x410, NULL, NULL, true);
}

// Posts entering the scenario editor (message 0x411)
long EEUserInterface::PostEnterScenarioEditor(bool inFlag)
{
    return this->mThread->Post(0x411, (void*)inFlag, NULL, true);
}

// Posts quitting the UI thread
void EEUserInterface::PostQuit()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
}

// Posts showing or hiding the pause information (message 0x427)
void EEUserInterface::PostPauseInformation(bool inFlag)
{
    this->mThread->Post(0x427, (void*)inFlag, NULL, true);
}

// Posts that the host migrated (message 0x426)
void EEUserInterface::PostHostMigrated()
{
    this->mThread->Post(0x426, NULL, NULL, true);
}

// Posts starting to load the game (0x407; 0x413 from the scenario editor)
void EEUserInterface::PostStartLoading()
{
    if (!gUIManager->mInScenarioEditor)
        this->mThread->Post(0x407, NULL, NULL, true);
    else
        this->mThread->Post(0x413, NULL, NULL, true);
}

// Posts loading a saved game file (message 0x408)
void EEUserInterface::PostLoadSavedGame(const FSFileSpec& inSpec)
{
    this->mThread->Post(0x408, new FSFileSpec(inSpec), NULL, true);
}

// Posts a UI event for a form (message 0x40e)
long EEUserInterface::PostEvent(UIEvent* inEvent, UIForm* inForm)
{
    inEvent->Reference();
    return this->mThread->Post(0x40e, inEvent, inForm, true);
}

// Posts playing a movie (message 0x41c)
void EEUserInterface::PostPlayMovie(const FSFileSpec& inSpec, UReferenceCounter* inObject)
{
    FSFileSpec* theSpec = new FSFileSpec(inSpec);
    if (inObject != NULL)
        inObject->Reference();
    this->mThread->Post(0x41c, theSpec, inObject, true);
}

// Posts that a movie finished (message 0x41d)
void EEUserInterface::PostMovieDone(UReferenceCounter* inObject)
{
    if (inObject != NULL)
        inObject->Reference();
    this->mThread->Post(0x41d, inObject, NULL, true);
}

// Posts hiding the messages (message 0x432)
void EEUserInterface::PostHideMessages()
{
    this->mThread->Post(0x432, NULL, NULL, true);
}

// Posts opening the civilization builder (message 0x420)
long EEUserInterface::OpenCivilizationBuilder(EECivilization* inCivilization, bool inFlag)
{
    return this->mThread->Post(0x420, inCivilization, (void*)inFlag, true);
}

// Posts closing a form (message 0x419)
long EEUserInterface::CloseForm(UIForm* inForm)
{
    return this->mThread->Post(0x419, inForm, NULL, true);
}

// Posts switching to a form (message 0x414)
long EEUserInterface::SwitchToForm(UIForm* inForm)
{
    return this->mThread->Post(0x414, inForm, NULL, true);
}

// Posts showing a loaded object (message 0x40c)
void EEUserInterface::ShowLoadedObject(UReferenceCounter* inObject)
{
    inObject->Reference();
    this->mThread->Post(0x40c, inObject, NULL, true);
}

// Posts that a message box closed, with its callback data (message 0x417)
void EEUserInterface::PostErrorClosed(UIForm* inForm, EEUIFErrorCallbackData* inData)
{
    this->mThread->Post(0x417, inForm, inData, true);
}

// Posts restarting the game (message 0x428)
void EEUserInterface::PostRestartGame()
{
    this->mThread->Post(0x428, NULL, NULL, true);
}

// Posts starting the scenario test the second way (message 0x415)
void EEUserInterface::StartScenarioTest2()
{
    this->mThread->Post(0x415, NULL, NULL, true);
}

// Posts showing the civilization builder (message 0x41f)
long EEUserInterface::PostDisplayCivBuilder()
{
    return this->mThread->Post(0x41f, NULL, NULL, true);
}

// Posts opening a form (message 0x40d)
long EEUserInterface::OpenForm(UIForm* inForm)
{
    return this->mThread->Post(0x40d, inForm, NULL, true);
}

// Posts changing the rasterizer (message 0x41e)
void EEUserInterface::PostChangeRasterizer(long inX, long inY)
{
    this->mThread->Post(0x41e, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

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

// Posts a white system line to the chat log, shown three times the usual time
long EEUserInterface::AddChatLogEntry(const UWideString& inText, long inY)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    return this->AddChatLogEntry(theColor, " > ", inText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 3, 2);
}

// Posts a just-in-time manufacturing update (message 0x416)
void EEUserInterface::UpdateJITManufacturing()
{
    this->mThread->Post(0x416, NULL, NULL, true);
}

// Posts a lobby player update (message 0x436)
void EEUserInterface::PostLobbyUpdatePlayer(long inX, long inY)
{
    this->mThread->Post(0x436, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// Posts a lobby game options update (message 0x437)
void EEUserInterface::PostLobbyUpdateGameOptions()
{
    this->mThread->Post(0x437, NULL, NULL, true);
}

// Posts a player's lag status (message 0x442)
void EEUserInterface::SetLagStatus(unsigned short inLag, unsigned char inStatus)
{
    this->mThread->Post(0x442, reinterpret_cast<void*>(inLag), (void*)(inStatus == 1), true);
}

// Posts a player's load percentage (message 0x443)
void EEUserInterface::SetLoadPercent(long inPlayer, unsigned long inPercent)
{
    this->mThread->Post(0x443, reinterpret_cast<void*>(inPlayer), reinterpret_cast<void*>(inPercent), true);
}

// Posts the lobby's transfer progress (message 0x439)
void EEUserInterface::PostLobbyShowTransferProgress(long inX, long inY)
{
    this->mThread->Post(0x439, reinterpret_cast<void*>(inX), reinterpret_cast<void*>(inY), true);
}

// Posts a lobby form update (message 0x438)
void EEUserInterface::PostLobbyUpdateForm()
{
    this->mThread->Post(0x438, NULL, NULL, true);
}

// Posts refreshing a saved group button (message 0x435)
void EEUserInterface::PostRefreshSavedGroup(long inX)
{
    this->mThread->Post(0x435, reinterpret_cast<void*>(inX), NULL, true);
}

// Posts the load progress percentage; in multiplayer also tells the others at every 20 percent step
void EEUserInterface::SetLoadProgress(float inFraction)
{
    if (inFraction < 0.0f)
        inFraction = 0.0f;
    else if (inFraction > 1.0f)
        inFraction = 1.0f;
    unsigned long thePercent = (unsigned long)(inFraction * 100.0f);
    if (sLastLoadPercent / 20 < thePercent / 20 && gCommunications != NULL && gCommunications->GetNumHumanPlayers() > 1)
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadPercent, {this->mLocalPlayer, thePercent}), kSlotNone);
    sLastLoadPercent = thePercent;
    this->mThread->Post(0x40a, reinterpret_cast<void*>(thePercent), NULL, true);
}

// Posts a loading text (message 0x40b)
void EEUserInterface::SetLoadingText(long inX)
{
    this->mThread->Post(0x40b, reinterpret_cast<void*>(inX), NULL, true);
}

// Posts the icon bar update (message 0x424) once until it is handled
void EEUserInterface::PostMessage424Once()
{
    if (this != NULL && !this->mIconBarUpdatePosted) {
        this->mThread->Post(0x424, NULL, NULL, true);
        this->mIconBarUpdatePosted = true;
    }
}

// Shows a lag state: a waiting message in the game, or the blinking lag button in the lobby (ee_screens ui_game.rs)
void EEUserInterface::ShowLagStatus(unsigned long inPlayer, bool inLagging) { ee_screens_ui_show_lag_status(inPlayer, inLagging); }

// Undoes the last action; returns 7 when there is none (ee_screens actions.rs)
long EEUserInterface::Undo()
{
    return ee_screens_actions_undo();
}

// Resumes the graphics thread and waits until it runs
void EEUserInterface::ResumeGraphics()
{
    gGraphics->PostResume();
    for (bool theStarted = gGraphics->mIsPaused; theStarted != false; theStarted = gGraphics->mIsPaused)
        PlatSleep(1);
}

// Handles the game window's events (platform/PlatWindow.h; the original's window procedure, case by case): close,
// minimise and restore (muting while minimised), focus (the game minimises itself and suspends input while inactive),
// the Control/AltGr state, typed text and the input method's composition (shown by the edit box; the system draws
// its candidate list). The decisions and the original's statics are Rust's (ee_screens ui_thread.rs), over the host
// below. The window's cursor is hidden at creation (WM_SETCURSOR's SetCursor(NULL)), and the screen saver / Alt-Tab
// system commands need no handling.
void EEUserInterface::HandleEvent(const PlatInputEvent& inEvent)
{
    ee_screens_ui_event(inEvent.mType, inEvent.mKey.mVirtualKey, inEvent.mText.mText, inEvent.mText.mCursor,
                        inEvent.mText.mLength);
}

// The window events' calls (EE_SCREENS_UI_EV_*): the globals' NULL tests, the lobby, the volumes, the input
// devices, the mouse pointer's thread, the input method; inA is the event's text for the IME ops
extern "C" int64_t ee_screenscpp_ui_event(uint32_t inOp, int64_t inA, int64_t inB, int64_t inC)
{
    float theVolume;
    uint32_t theBits = (uint32_t)inA;
    memcpy(&theVolume, &theBits, 4);
    const char* theText = reinterpret_cast<const char*>(inA);
    switch (inOp) {
    case EE_SCREENS_UI_EV_HAS:
        switch (inA) {
        case EE_SCREENS_UI_GLOBAL_USER_INTERFACE: return gUserInterface != NULL;
        case EE_SCREENS_UI_GLOBAL_MANAGER: return gUIManager != NULL;
        case EE_SCREENS_UI_GLOBAL_CONDUCTOR: return gConductor != NULL;
        case EE_SCREENS_UI_GLOBAL_SOUND: return gSoundManager != NULL;
        case EE_SCREENS_UI_GLOBAL_KEYBOARD: return gISKeyboard != NULL;
        case EE_SCREENS_UI_GLOBAL_MOUSE: return gISMouse != NULL;
        case EE_SCREENS_UI_GLOBAL_WINDOW_AND_MOUSE: return gGameWindow != NULL && gMouse != NULL;
        case EE_SCREENS_UI_GLOBAL_POINTER: return gMousePointer != NULL;
        default: return gGameWindow != NULL;
        }
    case EE_SCREENS_UI_EV_LOBBY_SHOWN: return EELobbyIsShown();
    case EE_SCREENS_UI_EV_LOBBY_OPEN: return EEIsLobbyOpen();
    case EE_SCREENS_UI_EV_LOBBY_SHUTDOWN: EELobbyShutdown(); break;
    case EE_SCREENS_UI_EV_POST_QUIT: gUserInterface->mThread->Post(0x12, NULL, NULL, false); break;   // WM_QUIT
    case EE_SCREENS_UI_EV_MUSIC_VOLUME:
        theVolume = gConductor != NULL ? gConductor->GetConductor().GetVolume() : 0.0f;
        memcpy(&theBits, &theVolume, 4);
        return theBits;
    case EE_SCREENS_UI_EV_SET_MUSIC_VOLUME: gConductor->GetConductor().SetVolume(theVolume); break;
    case EE_SCREENS_UI_EV_MASTER_VOLUME:
        theVolume = gSoundManager != NULL ? gSoundManager->GetMasterVolume() : 0.0f;
        memcpy(&theBits, &theVolume, 4);
        return theBits;
    case EE_SCREENS_UI_EV_SET_MASTER_VOLUME: gSoundManager->SetMasterVolume(theVolume); break;
    case EE_SCREENS_UI_EV_MUTE: SPIMSetMuted(inA != 0); break;   // port: "Mute in Background"
    case EE_SCREENS_UI_EV_KEYBOARD:
        if (inA != 0) gISKeyboard->Resume();
        else gISKeyboard->Suspend();
        break;
    case EE_SCREENS_UI_EV_MOUSE:
        if (inA != 0) gISMouse->Resume();
        else gISMouse->Suspend();
        break;
    case EE_SCREENS_UI_EV_CENTER_MOUSE:
        gMouse->SetPosition(GE2DPoint((float)(gGameWindow->GetWidth() >> 1), (float)(gGameWindow->GetHeight() >> 1)));
        break;
    case EE_SCREENS_UI_EV_POINTER_POST: gMousePointer->mThread->Post(inA, NULL, NULL, true); break;
    case EE_SCREENS_UI_EV_POINTER_SHOWN: return gMousePointer->mIsShown;
    case EE_SCREENS_UI_EV_SLEEP: PlatSleep(1); break;
    case EE_SCREENS_UI_EV_MUTE_IN_BACKGROUND: return gGamePreferences.mSettings.mMuteInBackground;   // port
    case EE_SCREENS_UI_EV_QUITTING: return gUserInterface->mIsQuitting;
    case EE_SCREENS_UI_EV_FULL_SCREEN: return gGamePreferences.mSettings.GetFullScreen();
    case EE_SCREENS_UI_EV_MINIMIZE: PlatWindowMinimize(gGameWindow->GetOSRepresentation()); break;   // CloseWindow
    case EE_SCREENS_UI_EV_IME_OPEN: gUserInterface->OpenIMECompositionString(); break;
    case EE_SCREENS_UI_EV_IME_UPDATE: gUserInterface->UpdateIMECompositionString(theText, inB, inC); break;
    case EE_SCREENS_UI_EV_IME_RESULT: gUserInterface->AddIMEResultString(theText); break;
    case EE_SCREENS_UI_EV_IME_CLOSE: gUserInterface->CloseIMECompositionString(); break;
    case EE_SCREENS_UI_EV_TRANSLATED_KEY: gUIManager->UpdateTranslatedKey(theText, 1); break;
    }
    return 0;
}

// Receive's window event pump on the window thread (TSThread::SetEventPump): one event per call
bool EEPumpWindowEvent()
{
    PlatInputEvent theEvent;
    if (!PlatWindowPollEvent(&theEvent))
        return false;
    if (theEvent.mType == PLAT_EVENT_WAKE)   // the WON lobby's wake-up (PlatWindowWake) while it is not running
        return true;
    static const bool sLog = getenv("EE_WINDOW_EVENT_LOG") != NULL;   // opt-in: every event as a debug string
    if (sLog)
        ESDebugOut("EE-WINDOW-EVENT %u (key %u/%u, text '%s') ui %d window %lux%lu\n", (unsigned)theEvent.mType,
                   (unsigned)theEvent.mKey.mScanCode, (unsigned)theEvent.mKey.mVirtualKey,
                   theEvent.mType == PLAT_EVENT_TEXT ? theEvent.mText.mText : "", gUserInterface != NULL,
                   gGameWindow ? gGameWindow->GetWidth() : 0, gGameWindow ? gGameWindow->GetHeight() : 0);
    ISHandleEvent(theEvent);   // the Input System's keyboard and mouse (DirectInput's devices before)
    EEUserInterface::HandleEvent(theEvent);
    return true;
}

#ifdef _WIN32
// The Win32 messages that are not window events (PlatWindowSetNativeMessageHook): the WON lobby's two messages,
// handled as the window procedure did; nonzero drops the message. (CD audio's MCI notifications went to SECD, which
// is gone: the port never opened a CD drive.)
int EENativeMessageHook(void* inWindow, uint32_t inMessage, uint64_t inWParam, int64_t inLParam)
{
    switch (inMessage) {
    case 0x433:
        PostMessageA((HWND)inWindow, 0x434, 0, 0);
        if (gWonNet != NULL)
            gWonNet->FnCFFDD(0);
        return 1;
    case 0x434:
        if (gWonNet != NULL)
            gWonNet->FnCFFF2();
        return 1;
    }
    return 0;
}
#endif

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
