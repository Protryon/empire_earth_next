// C:\EEX\Empire Earth\Physics Thread\EEPhysics.cpp (__FILE__ 0x4c1e80). The physics thread: 0x151c28..0x15436d (the
// "EEPhysics Data" key's init chain, the class in definition order, the inline COMDATs of other classes after their
// first users). Unit flags: the exe's module set.
#include <windows.h>
#include "platform/PlatFile.h"
#include <vector>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/NEMessage.h"
#include "lle/ISEntity.h"
#include "lle/ISKeyboard.h"
#include "lle/Input System.h"
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
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
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

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EEPhysics.h"
#include "ee/EEMessage.h"
#include "ee/EEAEManager.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEScenarioManager.h"
#include "ee/EETeam.h"
#include "ee/EECampaignProgress.h"
#include "lle/UIBlinkingButton.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "lle/USingleton.h"
#include "lle/UResourceFile.h"
#include "lle/GETerrainMesh.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEEffectManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUserInterface.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEMap.h"
#include "ee/EEStringIDs.h"
#include "ee/EECPTrace.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEMessageCapture.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitStore.h"
#include "rust/sim/world.h"
#include "rust/sim/units.h"   // ee_units_store_checksum_flag1
#include <functional>
#include "ee/EEGameLog.h"
#include "rust/sim/ai.h"   // ee_ai_name_copy
#include "rust/game/physics.h"
#include "rust/game/gamecpp.h"   // EeGameCpp (EECore11OpE: the end of the chain)
#include "rust/sim/app.h"
// chain at 0x551c28 (wrapper)
//   object 0x931ce0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gPhysicsDataKey("EEPhysics Data");

extern UString gCampaignUser;              // 0x531cd0 (defined at the end of this unit; provisional name)
unsigned long EEGetCivTimerLimit();         // 0x27ffae (cdecl: 300000; the civ timer limits, EEUIFMainGame.cpp's range)
unsigned long EEGetCivTimerSecondWarning();  // 0x27ffba (270000) (provisional names)
unsigned long EEGetCivTimerFirstWarning();  // 0x27ffb4 (240000)
extern unsigned long& gContinentManagerChecksum;  // 0x530d10 (the continent manager's checksum; provisional name)
extern unsigned long gCollisionPeriod;      // 0x4c237c (.data = 1 after gCollisionDetectionUnit; provisional name)

// Creates the physics simulation: makes its save-game lock, resets its clocks and starts the thread's main loop
EEPhysics::EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer)
    : RTSPhysics(inGraphics, inTimer, 1, 0xf), mInputMessagesProcessed(0), mLastOutOfSynchChecksum(0)
{
    ee_physics_construct(this);   // (physics.rs)
}

// Destroys the physics simulation, releasing its save-game lock
EEPhysics::~EEPhysics()
{
    TSScheduler::DestroyCriticalSection(this->mSaveGameLock);
}

// The methods' bodies are Rust (ee_game physics.rs, world lane round 1); each C++ call they make is a case of
// ee_physcpp below
void EEPhysics::AutoSave() { ee_physics_auto_save(this); }
void EEPhysics::ClearServerMessages() { ee_physics_clear_server_messages(this); }
void EEPhysics::FlushWorld() { ee_physics_flush_world(this); }
void EEPhysics::RunOwedTicks() { ee_physics_run_owed_ticks(this); }
void EEPhysics::EndGame(bool inIsVictory) { ee_physics_end_game(this, inIsVictory); }
unsigned long EEPhysics::GetOutOfSynchChecksum(bool inDump, unsigned long inFlags)
    { return ee_physics_out_of_synch_checksum(this, inDump, inFlags); }
void EEPhysics::GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime)
    { ee_physics_minutes_and_seconds(this, outMinutes, outSeconds, inTime); }
long EEPhysics::GetUnitSwapKind(EEComplexUnit* inUnit) { return ee_physics_unit_swap_kind(this, inUnit); }
void EEPhysics::Init() { ee_physics_init(this); }
void EEPhysics::Lock() { gWorld.mLock->WriteLock(); }
void EEPhysics::QueueServerMessage(NEMessage* inMessage) { this->mServerMessages.push_back(inMessage); }
void EEPhysics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
    { ee_physics_handle_message(this, (uint32_t)inMessage, inParameter1, inParameter2); }
void EEPhysics::Idle() { ee_physics_idle(this); }
// its own lines: the test profile's harness hook (tools/build.py HARNESS_HOOKS)
void EEPhysics::Tick()
{
    ee_physics_tick(this);
}
void EEPhysics::QueueMessage(NEMessage* inMessage) { ee_physics_queue_message(this, inMessage); }
void EEPhysics::SwapUnit(EEUnit* inOld, EEUnit* inNew) { ee_physics_swap_unit(inOld, inNew); }
void EEPhysics::ProcessUnitSwaps() { ee_physics_process_unit_swaps(this); }
void EEPhysics::DrainInput() { ee_physics_drain_input(this); }
// its own lines: the test profile's harness hook (tools/build.py HARNESS_HOOKS)
void EEPhysics::ReportStatistics()
{
    ee_physics_report_statistics(this);
}
long EEPhysics::PostAutoSave() { return ee_physics_post(this, 0x50a); }
long EEPhysics::PostShowDialogMessage() { return ee_physics_post(this, 0x508); }
long EEPhysics::PostInit() { return ee_physics_post(this, 0x509); }
void EEPhysics::UpdateTiming() { ee_physics_update_timing(this); }
void EEPhysics::QueueUnitSwap(EEComplexUnit* inOld, EEComplexUnit* inNew) { ee_physics_queue_unit_swap(this, inOld, inNew); }
void EEPhysics::Unlock() { gWorld.mLock->WriteUnlock(); }
void EEPhysics::UpdateAreaEffectsClock() { ee_physics_update_area_effects_clock(this); }
void EEPhysics::UpdateCivTimer() { ee_physics_update_civ_timer(this); }
void EEPhysics::ServeLoadSaveRequests() { ee_physics_serve_load_save_requests(this); }
void EEPhysics::CheckShiftMode() { ee_physics_check_shift_mode(this); }
void EEPhysics::UpdateWeather() { ee_physics_update_weather(this); }

// Returns the game speed and elapsed game time as text ("<speed> hh:mm:ss")
UWideString EEPhysics::GetTimeString()
{
    char theText[64];
    ee_physics_time_string(this, theText, sizeof theText);
    return UWideString(theText);
}

// port: the loaded values the load's rebuilding set again (EEAfterLoad), then those that wait for the others
// (EEAfterLoadLast), run at the loaded game's first tick (R5a.2, "load equals continue"; the loaded seed is Rust's:
// ee_physics_loaded_seed)
// (ee/EESaveGameFiles.h; the lists and their run are physics.rs')
void EEAfterLoad(std::function<void()> inFixup)
{
    ee_physics_after_load(false, new std::function<void()>(inFixup));
}

void EEAfterLoadLast(std::function<void()> inFixup)
{
    ee_physics_after_load(true, new std::function<void()>(inFixup));
}

// Reads the physics clocks, tick counters, timing averages and civ timer state from a saved game (the game-over
// flag after version 0.0xce, the resource timer from 0xfa); returns the first read error. The original's two era
// bodies are Rust (ee_units physics_save.rs, over the record this class derives from; physics.rs: the base's part,
// the file's primitives, the loaded seed back at the first tick)
long EEPhysics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_physics_load(this, &inFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Writes the physics clocks, tick counters, timing averages, civ timer state, game-over flag and resource timer
// to the saved game; returns the first write error
long EEPhysics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_physics_save(this, &outFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// TEST ONLY: the UI fixture's view of each executed input (EETestFixtureOrders.cpp: the orders' verdicts), called
// before (0) and after (1) the message executes, under the world's write lock; NULL unless the fixture runs
// (EEGlobals.cpp's)
extern void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase);

// New (port): the game log's human orders and selections (TEST ONLY; ee_sim units order_log.rs): its host, a class's
// (inOp 0) or a technology's (1) name, or the line written (2)
extern "C" size_t ee_units_order_log_host(uint8_t inOp, int64_t inKey, uint8_t* ioText, size_t inCapacity)
{
    if (inOp == 2) {
        EEGameLogLine((const char*)ioText);
        return 0;
    }
    return ee_ai_name_copy(inOp, inKey, ioText, inCapacity);   // ee_ai names_db.rs
}

// The just-in-time manufacturing chat line (Tick's resource timer)
// A won campaign scenario recorded in the campaign progress (EndGame)
// The Shift key (0xe1) is up (CheckShiftMode)

#define P64(x) ((int64_t)(intptr_t)(x))
#define PH ((EEPhysics*)p)
#define MSG ((NEMessage*)p)
#define REC ((NEMRecord*)p)
#define PL ((EEPlayer*)p)
#define CX ((EEComplexUnit*)p)
#define FP(i) ((FILE*)a[i])
#define SERVER (static_cast<NESynchronousServer*>(gCommunications))

extern "C" double ee_physcpp_f(EePhysCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// The physics' calls (ee_game physics.rs EePhysCpp): p the physics or the object named, a the integers, f the floats
extern "C" int64_t ee_physcpp(EePhysCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EePhysCpp O;
    switch (inOp) {
    // the physics
    case O::Rec: return P64(static_cast<EeUnitsPhysicsSaved*>(PH));
    case O::SetSpeed: PH->SetSpeed(f[0]);
        break;
    case O::TimerInterval: return PH->mTimerInterval;
    case O::TicksPerSecond: return PH->mTicksPerSecond;
    case O::TimerRunning: return PH->mTimerRunning;
    case O::RandomSeed: return PH->mRandom.GetSeed();
    case O::CreateSaveLock: return TSScheduler::CreateCriticalSection(PH->mSaveGameLock) == keseOK;
    case O::StartMainLoop: PH->mStartThreadMainLoop->Set();
        break;
    case O::RtsLoad: return PH->RTSPhysics::Load((unsigned long)a[1], (unsigned long)a[2], *(FSFile*)a[0]);
    case O::RtsSave: return PH->RTSPhysics::Save((unsigned long)a[1], (unsigned long)a[2], *(FSFile*)a[0]);
    case O::SetRandomSeed: PH->mRandom.SetSeed((unsigned long)a[0]);
        break;
    case O::InputMessages: return PH->mInputMessagesProcessed;
    case O::SetInputMessages: PH->mInputMessagesProcessed = (long)a[0];
        break;
    case O::SetLastChecksum: PH->mLastOutOfSynchChecksum = (unsigned long)a[0];
        break;
    case O::ThreadPost: return PH->mThread != NULL ? PH->mThread->Post((unsigned long)a[0], NULL, NULL, true) : 0;
    case O::RtsHandleMessage: PH->RTSPhysics::HandleMessage((unsigned long)a[0], (void*)a[1], (void*)a[2]);
        break;
    case O::UpdateUnits: return PH->UpdateUnits();
    case O::FixupRun: (*(std::function<void()>*)p)();
        break;
    case O::FixupDelete: delete (std::function<void()>*)p;
        break;
    case O::UnitOrderReset: EEUnitOrderReset();   // New (R5b.4): the stepped units as a fixup ordered them (ee/EEUnitStore.h)
        break;
    case O::ServerLockEnter: PH->mServerMessageQueueLock->Enter();
        break;
    case O::ServerLockLeave: PH->mServerMessageQueueLock->Leave();
        break;
    case O::ServerQueueSize: return (int64_t)PH->mServerMessages.size();
    case O::ServerQueueFront: return P64(PH->mServerMessages.front());
    case O::ServerQueuePopFront: PH->mServerMessages.pop_front();
        break;
    case O::SaveLockEnter: PH->mSaveGameLock->Enter();
        break;
    case O::SaveLockLeave: PH->mSaveGameLock->Leave();
        break;
    case O::LoadSaveCount: return (int64_t)PH->mLoadSaveRequests.size();
    case O::LoadSaveAt: return P64(PH->mLoadSaveRequests[(size_t)a[0]]);
    case O::LoadSaveSetNull: PH->mLoadSaveRequests[(size_t)a[0]] = NULL;
        break;
    case O::LoadSaveClear: PH->mLoadSaveRequests.clear();
        break;
    case O::LoadSavePush: PH->mLoadSaveRequests.push_back((NEMessage*)a[0]);
        break;
    case O::SwapCount: return (int64_t)PH->mUnitSwaps.size();
    case O::SwapOld: return P64(PH->mUnitSwaps[(size_t)a[0]].mOld);
    case O::SwapNew: return P64(PH->mUnitSwaps[(size_t)a[0]].mNew);
    case O::SwapPush: PH->mUnitSwaps.push_back(EEPUnitSwap{(EEComplexUnit*)a[0], (EEComplexUnit*)a[1]});
        break;
    case O::SwapClear: PH->mUnitSwaps.clear();
        break;
    // a message
    case O::MsgType: return MSG->GetType();
    case O::MsgReference: MSG->Reference();
        break;
    case O::MsgDereference: MSG->Dereference();
        break;
    case O::MsgForSpecificSlot: return MSG->GetForSpecificSlot();
    case O::MsgOriginatingSlot: return MSG->GetOriginatingSlot();
    case O::MsgSetOriginatingSlot: MSG->SetOriginatingSlot((unsigned char)a[0]);
        break;
    case O::RecBool: return REC->Get<bool>((unsigned long)a[0]);
    case O::RecU8: return REC->Get<unsigned char>((unsigned long)a[0]);
    case O::RecU32: return REC->Get<uint32_t>((unsigned long)a[0]);
    case O::GameCast: return P64(EEGameMessageCast(MSG));
    case O::ExecuteSelection: EEGameMessageCast(MSG)->ExecuteSelectionMessage();
        break;
    case O::ExecuteGame: ((EEMessage*)p)->ExecuteMessage();
        break;
    case O::Execute: MSG->Execute();
        break;
    case O::RecordBytes: return P64(static_cast<EEMRecord*>((EEMessage*)p)->mRecord);
    case O::RemovePlayer: gCommunications->ExecuteRemovePlayer(REC->Get<unsigned char>(0), REC->Get<bool>(3), REC->Get<bool>(1), REC->Get<bool>(2));
        break;
    case O::NewChecksumResponse: return P64(new NEMRecord(kNetMessageWorldDatabaseChecksumResponse, {(unsigned long)a[0], (unsigned long)a[1]}));
    case O::NewLoadSaveResponse: return P64(new EEMRecord(kMessageLoadOrSaveGameResponse, {(long)a[0]}));
    case O::NewReportStatistics: return P64(new EEMRecord(kMessageReportStatistics, {(unsigned long)a[0], (unsigned long)a[1]}));
    case O::NewExitShiftMode: return P64(new EEMRecord(kMessageExitShiftMode, {(long)a[0]}));
    case O::NewCivTimerExpired: return P64(new EEMRecord(kMessageCivTimerExpired, {(unsigned char)a[0]}));
    // the captures (TEST ONLY)
    case O::CaptureExecuted: EEMessageCaptureExecuted(MSG);
        break;
    case O::UnitStoreCheck: EEUnitStoreCheck((unsigned long)a[0]);
        break;
    case O::TestHookSet: return gEEMessageExecutedTestHook != NULL;
    case O::TestHookCall: gEEMessageExecutedTestHook(MSG, (int)a[0]);
        break;
    // the communications
    case O::IsServer: return gCommunications->IsServer();
    case O::IsClient: return gCommunications->IsClient();
    case O::CommsIsServer: return gCommunications != NULL && gCommunications->IsServer();
    case O::ClientHasLeft: return static_cast<EEClient*>(gCommunications)->mHasLeftGame;
    case O::CtsInPhysics: return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case O::IncrementCts: gCommunications->IncrementNumberOfCTsProcessedInPhysics();
        break;
    case O::SetAverageUpdate: gCommunications->SetAveragePhysicsUpdateTime((long)a[0]);
        break;
    case O::SetAverageGap: gCommunications->SetAverageTimeBetweenPhysicsTicks((long)a[0]);
        break;
    case O::ProcessLocalInput: gCommunications->ProcessLocalUserInput(MSG, (unsigned char)a[0]);
        break;
    case O::LogChecksumResponse: SERVER->LogChecksumResponse(REC);
        break;
    case O::LogResponse: SERVER->LogResponse(MSG);
        break;
    case O::IsPaused: return gCommunications->GetIsGamePaused();
    case O::CurrentCommsTick: return gCommunications->GetCurrentCommunicationsTick();
    case O::PauseGame: SERVER->PauseGame();
        break;
    case O::SendAutoSave: SERVER->SendToAllAndWait(new EEMRecord(kMessageLoadOrSaveGame, {0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 1, 0, UWideString((const char*)a[0])}), 15000, 12);
        break;
    // the world
    case O::NumPlayers: return gWorld.mNumPlayers;
    case O::Player: return P64(gWorld.mPlayers[a[0]]);
    case O::CurrentPlayer: return P64(gWorld.GetCurrentPlayer());
    case O::UpdateActiveUnits: gWorld.UpdateActiveUnits();
        break;
    case O::InCinematic: return gWorld.mInCinematic;
    case O::ScenarioResult: return gWorld.mScenarioManager->mGameResult;
    case O::SetScenarioResult: gWorld.mScenarioManager->mGameResult = (long)a[0];
        break;
    case O::ScenarioUpdate: gWorld.mScenarioManager->Update();
        break;
    case O::ScenarioShowingDialog: return gWorld.mScenarioManager->mIsShowingDialog;
    case O::ScenarioCanShowDialog: return gWorld.mScenarioManager && !gScenarioSimDialogs;
    case O::ScenarioShowDialog: gWorld.mScenarioManager->ShowDialogMessage();
        break;
    case O::ScenarioReplaceUnit: gWorld.mScenarioManager->ReplaceUnit((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TeamsClear: gWorld.mTeams.clear();
        break;
    case O::CameraUnitA: return P64(gWorld.mCameraControl.mUnitA.Get());
    case O::CameraUnitB: return P64(gWorld.mCameraControl.mUnitB.Get());
    case O::CameraFollow: gWorld.mCameraControl.Follow((EEUnit*)a[0], gWorld.mCameraControl.mIsFollowingUnitB, 1.0f, gWorld.mCameraControl.mTurnYaw, gWorld.mCameraControl.mTurnPitch);
        break;
    case O::CameraClearA: gWorld.mCameraControl.ClearUnitA();
        break;
    case O::CameraClearB: gWorld.mCameraControl.ClearUnitB();
        break;
    case O::CameraUpdate: gWorld.mCameraControl.Update(f[0]);
        break;
    case O::SetWindYaw: gWorld.mWindYaw = f[0];
        break;
    case O::SetWindDirection: gWorld.mWindDirection = GEVector(f[0], f[1], f[2]);
        break;
    case O::EnvironmentTick: gWorld.mEnvironmentManager->Tick((unsigned long)a[0]);
        break;
    case O::WorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::WorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::UnitsFirst: return P64(gWorld.mUnits.mFirst);
    case O::UnitNext: return P64(((RTSUnit*)p)->mTileNode->mNext);
    case O::UnitChecksum: return static_cast<EEUnit*>((RTSUnit*)p)->GetChecksum((unsigned long)a[0]);
    case O::UnitDump: static_cast<EEUnit*>((RTSUnit*)p)->OutOfSynchDump(FP(0), (unsigned long)a[1]);
        break;
    case O::ContagionSwapUnit: gWorld.GetContagionManager()->SwapUnit((EEUnit*)a[0], (EEUnit*)a[1]);
        break;
    case O::ContagionDump: gWorld.GetContagionManager()->DumpOutOfSynch(FP(0));
        break;
    case O::MapChecksum: return gWorld.mMap->ComputeChecksum();
    case O::MapDump: gWorld.mMap->DumpOutOfSynch(FP(0));
        break;
    case O::CampaignNameLength: return gWorld.mGameOptions.mCampaignName.GetLength();
    case O::CampaignChecksum: return gWorld.mGameOptions.mCampaignChecksum;
    case O::IsScenarioGame: return gWorld.mGameOptions.mIsScenarioGame;
    case O::Resources: return gWorld.mGameOptions.mResources;
    case O::CanBuyCivilization: return gWorld.mGameOptions.CanBuyCivilization();
    case O::CheckUnitList: RTSCheckUnitList(&gWorld.mUnits, "EEPhysics::FlushWorld (start)");   // EE_CHECK_UNIT_LIST (ee/RTSDummyHeader.h)
        break;
    case O::SetGameOverFlag: gB529065 = true;
        break;
    // a player
    case O::PlayerObserver: return PL->mObserverMode;
    case O::PlayerGameResult: return PL->mGameResult;
    case O::PlayerAddPendingUnits: PL->AddPendingUnits(0, 1);
        break;
    case O::PlayerUnitsSwapped: PL->UnitsSwapped(((EEPhysics*)a[0])->mUnitSwaps);
        break;
    case O::PlayerCivChosen: return PL->mCivilizationChosen;
    case O::PlayerShiftMode: return PL->mShiftMode;
    case O::PlayerAvailablePoints: return static_cast<unsigned long>(PL->mCivilization.GetAvailablePoints());
    case O::PlayerSavedGroupLast: return PL->mSavedGroups.GetLastValue((unsigned long)a[0]);
    case O::PlayerId: return PL->mPlayerID;
    case O::PlayerSlot: return PL->mSlot;
    case O::PlayerGetSlot: return PL->GetSlot();
    case O::PlayerActiveGroup: return P64(PL->mActiveGroup);
    case O::PlayerRunningGroups: return PL->GetNumRunningGroups();
    case O::PlayerRunningGroup: return P64(PL->GetRunningGroup((unsigned long)a[0]));
    case O::PlayerAirTraffic: return P64(PL->mAirTraffic[a[0]]);
    // the groups
    case O::SelectedCount: return ((EESelectedGroup*)p)->GetNumUnits();
    case O::SelectedUnitId: return ((EESelectedGroup*)p)->GetUnit((unsigned long)a[0])->mUnitID;
    case O::GroupCount: return ((EEGroup*)p)->GetNumUnits();
    case O::GroupUnitId: return ((EEGroup*)p)->GetUnit((unsigned long)a[0])->mUnitID;
    // a unit
    case O::UnitPlayer: return P64(CX->GetPlayer());
    case O::UnitIsCitizen: return CX->mIsCitizen;
    case O::UnitReference: CX->Reference();
        break;
    case O::UnitDereference: CX->Dereference();
        break;
    case O::UnitId: return ((EEUnit*)p)->mUnitID;
    // the other systems
    case O::FreeCamera: return gFreeCamera == true;
    case O::PathManagerUpdate: gPathManager.Update();
        break;
    case O::PathManagerDump: gPathManager.DumpOutOfSynch(FP(0));
        break;
    case O::AreaEffectsTick: gAEManager.Tick(f[0]);
        break;
    case O::EffectManagerUpdate: gEffectManager->Update();
        break;
    case O::EffectReplaceUnit: gEffectManager->ReplaceUnit((EEUnit*)a[0], (EEUnit*)a[1]);
        break;
    case O::CalamityUpdate: EECalamityManager::GetInstance()->Update(f[0]);
        break;
    case O::CalamityDump: EECalamityManager::GetInstance()->DumpOutOfSynch(FP(0));
        break;
    case O::CalamityChecksum: return EECalamityManager::GetInstance()->GetOutOfSynchChecksum();
    case O::AiUpdate: gArtificialIntelligence->Update();
        break;
    case O::WorldTableLong: return gWorldTable.GetLongValue((EEWorldConstant)a[0]);
    case O::HasJitPower: return gCivPowersManager.HasPower(gWorld.GetCurrentPlayer(), kCivPowerJustInTimeManufacturing);
    case O::TextNew: return P64(new UWideString);
    case O::TextDelete: delete (UWideString*)p;
        break;
    case O::ResourceString: return gResource->GetString((unsigned long)a[0], *(UWideString*)p);
    case O::ChatLog: gUserInterface->AddChatLogEntry(*(UWideString*)p, 0);
        break;
    case O::IsMainGameForm: return dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL;
    case O::OptionsButtonBlink: (static_cast<UIBlinkingButton*>(gUIManager->GetMainGameForm()->GetControl(kMainGameIngameOptions)))->SetIsBlinking(true);
        break;
    case O::CivTimerLimit: return EEGetCivTimerLimit();
    case O::CivTimerSecondWarning: return EEGetCivTimerSecondWarning();
    case O::CivTimerFirstWarning: return EEGetCivTimerFirstWarning();
    case O::ShiftKey: return gISKeyboard != NULL ? gISKeyboard->GetEntity(kKeyShift, *(ISEntity**)a[0]) : -1;
    case O::EntityValue: return ((ISEntity*)p)->GetValue();
    case O::AutoSaves: return gGamePreferences.mSettings.mNumberOfAutoSaves;
    case O::AutoSaveMilliseconds: return gGamePreferences.mSettings.mAutoSaveInMilliseconds;
    case O::SetLastAutoSave: gGamePreferences.mSettings.mLastAutoSave = UWideString((const char*)a[0]);
        break;
    case O::SavePreferences: gGamePreferences.Save();
        break;
    case O::SavedGameExtension: snprintf((char*)a[0], (size_t)a[1], "%s", (const char*)gSavedGameExtension);
        break;
    case O::CivBuilderBlink: EESetCivBuilderButtonBlinking(a[0] != 0);
        break;
    case O::LoadOrSave: return EELoadOrSave(static_cast<EEMRecord*>(MSG));
    case O::FileOpen: return P64(PlatFOpen((const char*)a[0], "w"));
    case O::FilePuts: fputs((const char*)a[0], (FILE*)p);
        break;
    case O::FileClose: fclose((FILE*)p);
        break;
    case O::ContinentChecksum: return gContinentManagerChecksum;
    case O::CampaignText: return snprintf((char*)a[1], (size_t)a[2], "%s", a[0] == 0 ? (const char*)gCampaignUser : a[0] == 1 ? (const char*)gWorld.mGameOptions.mCampaignName : (const char*)gWorld.mGameOptions.mScenarioName);
    case O::Srand: srand((unsigned int)a[0]);
        break;
    case O::Rand: return rand();
    case O::Verify: ESVERIFY_AT(a[0] != 0, (long)a[1]);
        break;
    default: return (int64_t)ee_physcpp_f(inOp, p, a, f);
    }
    return 0;
}

extern "C" double ee_physcpp_f(EePhysCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EePhysCpp O;
    switch (inOp) {
    case O::Speed: return PH->mSpeed;
    case O::Random: return PH->mRandom.Generate();
    case O::GameSpeed: return gWorld.GetGameSpeed();
    case O::WorldGameSpeed: return gWorld.mGameSpeed;
    case O::TimeOfDay: return gWorld.GetTimeOfDay();
    case O::WindYaw: return gWorld.mWindYaw;
    default: ESVERIFY_AT(false, 1);
    }
    return 0.0;
}

#undef PH
#undef MSG
#undef REC
#undef PL
#undef CX
#undef FP
#undef SERVER

// Sets the collision period (the active units spread over gCollisionDetectionUnit ticks, at least 1), updates
// every player, then steps each active unit (last first); returns the unit count
// New (R5b.4 leaf 6): the loop is Rust over the stepped units' list (ee_units tick.rs update_units: the period, the
// players' updates and each unit's Step from the last to the first, host calls); the list equals the deque in order
// (USCK checks it), and while the capture runs each step checks the list's id against the deque's
static void TickSetCollisionPeriod(void* inContext, uint32_t inValue)
{
    gCollisionPeriod = inValue;
}

static void TickUpdatePlayer(void* inContext, uint32_t inIndex)
{
    if (gWorld.mPlayers[inIndex] != NULL)
        gWorld.mPlayers[inIndex]->UpdatePlayer();
}

static void TickStep(void* inContext, uint32_t inIndex, uint32_t inID)
{
    EEComplexUnit* theUnit = EEUnitHandle<EEComplexUnit>::FromSavedID(inID);   // the stepped list's inIndex-th
    gPhysicsSteppingUnit = theUnit;   // TEST ONLY: named by EE-SIM's random-draw trace
    theUnit->Step();
    ee_units_note_update_commit(inID);   // TEST ONLY (USST kind 12)
}

unsigned long EEPhysics::UpdateUnits()
{
    EeUnitsTickHost theHost;
    theHost.ctx = NULL;
    theHost.num_players = (uint32_t)gWorld.mNumPlayers;
    theHost.detection_unit = (uint32_t)gCollisionDetectionUnit;
    theHost.set_collision_period = TickSetCollisionPeriod;
    theHost.update_player = TickUpdatePlayer;
    theHost.step = TickStep;
    unsigned long theCount = ee_units_update_units(&theHost);
    gPhysicsSteppingUnit = NULL;
    // TEST ONLY: the step leaves' records (USTM, USST; ee_sim units/notes.rs)
    ee_units_note_step_commit(gPhysics != NULL ? (uint32_t)gPhysics->mNumTicksDone : 0);
    return theCount;
}

// (gPhysicsSteppingUnit and gPhysicsAfterTickHook, TEST ONLY, are EEGlobals.cpp's)

// the unit's last global (0x531cd0, "default" 0x4b5210): its init chain follows the template COMDATs
// (EndGame: the campaign progress's user; not EEUIECampaignSelectScreenEvents.cpp's gCampaignDefaultUser 0x562ee0)
UString gCampaignUser("default");

// Steps the environment manager when it is enabled and its next step time has come
// not inline: defined here after the unit's init chain (UpdateWeather calls it)
void EEEnvironmentManager::Tick(unsigned long inTime)
{
    if (this->mNextStep <= inTime && this->mIsEnabled == true)
        this->Step(inTime);
}

// ------------------------------------------------------------------------- round 11, lane E; round 12, fork B: the world lane's leftovers (the end of the chain)

// the C++ calls and members of fork B's bodies in Rust (gamecpp; EeGameCpp's round 12 fork B section): one
// statement each
int64_t EECore11OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // round 13, fork B: the world's dtor (gamecpp world.rs ee_gamecpp_world_destroy; p the world)
    case O::C13bGhostLockFree: TSScheduler::DestroyCriticalSection(((EEWorld*)p)->mGhostUnitsLock);
        break;
    case O::C13bGhostAt: return P64(((EEWorld*)p)->mGhostUnits[(size_t)a[0]]);
    default: ESVERIFY_AT(inOp == EeGameCpp::UnRts, 1);
    }
    return 0;
}

double EECore11OpEF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    ESVERIFY_AT(inOp == EeGameCpp::CxEffectHeight, 1);
    return 0.0;
}
