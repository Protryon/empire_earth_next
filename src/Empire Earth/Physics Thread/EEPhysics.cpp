// C:\EEX\Empire Earth\Physics Thread\EEPhysics.cpp (__FILE__ 0x4c1e80). The physics thread: 0x151c28..0x15436d (the
// "EEPhysics Data" key's init chain, the class in definition order, the inline COMDATs of other classes after their
// first users). Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <stdlib.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/Input System.h"
#include "lle/GERhombus.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
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
#include "ee/EEEnvironmentManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEMap.h"
#include "ee/EEMessageCapture.h"
#include <functional>
#include "ee/EEGameLog.h"
// chain at 0x551c28 (wrapper)
//   object 0x931ce0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ

extern UString gCampaignUser;              // 0x531cd0 (defined at the end of this unit; provisional name)
unsigned long EEGetCivTimerLimit();         // 0x27ffae (cdecl: 300000; the civ timer limits, EEUIFMainGame.cpp's range)
unsigned long EEGetCivTimerSecondWarning();  // 0x27ffba (270000) (provisional names)
unsigned long EEGetCivTimerFirstWarning();  // 0x27ffb4 (240000)
extern unsigned long& gContinentManagerChecksum;  // 0x530d10 (the continent manager's checksum; provisional name)

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

void EEPhysics::Lock() { gWorld.mLock->WriteLock(); }
void EEPhysics::Unlock() { gWorld.mLock->WriteUnlock(); }
// its own lines: the test profile's harness hook (tools/build.py HARNESS_HOOKS)
void EEPhysics::Tick()
{
    ee_physics_tick(this);
}
// its own lines: the test profile's harness hook (tools/build.py HARNESS_HOOKS)
void EEPhysics::ReportStatistics()
{
    ee_physics_report_statistics(this);
}

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
    case O::UnitStep: CX->Step();
        break;
    case O::PlayerUpdate: PL->UpdatePlayer();
        break;
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
    case O::RecordBytes: return P64(static_cast<EEMRecord*>((EEMessage*)p)->mRecord);
    case O::RemovePlayer: gCommunications->ExecuteRemovePlayer(REC->Get<unsigned char>(0), REC->Get<bool>(3), REC->Get<bool>(1), REC->Get<bool>(2));
        break;
    case O::NewChecksumResponse: return P64(new NEMRecord(kNetMessageWorldDatabaseChecksumResponse, {(unsigned long)a[0], (unsigned long)a[1]}));
    case O::NewReportStatistics: return P64(new EEMRecord(kMessageReportStatistics, {(unsigned long)a[0], (unsigned long)a[1]}));
    case O::NewExitShiftMode: return P64(new EEMRecord(kMessageExitShiftMode, {(long)a[0]}));
    case O::NewCivTimerExpired: return P64(new EEMRecord(kMessageCivTimerExpired, {(unsigned char)a[0]}));
    // the captures (TEST ONLY)
    case O::CaptureExecuted: EEMessageCaptureExecuted(MSG);
        break;
    case O::UnitStoreCheck: EEUnitStoreCheck((unsigned long)a[0]);
        break;
    case O::TestHookCall: gEEMessageExecutedTestHook(MSG, (int)a[0]);
        break;
    // the communications
    case O::ProcessLocalInput: gCommunications->ProcessLocalUserInput(MSG, (unsigned char)a[0]);
        break;
    case O::LogChecksumResponse: SERVER->LogChecksumResponse(REC);
        break;
    case O::LogResponse: SERVER->LogResponse(MSG);
        break;
    case O::PauseGame: SERVER->PauseGame();
        break;
    case O::SendAutoSave: SERVER->SendToAllAndWait(new EEMRecord(kMessageLoadOrSaveGame, {0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 1, 0, UWideString((const char*)a[0])}), 15000, 12);
        break;
    // the world
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
    case O::SetWindDirection: gWorld.mWindDirection = GEVector(f[0], f[1], f[2]);
        break;
    case O::EnvironmentTick: gWorld.mEnvironmentManager->Tick((unsigned long)a[0]);
        break;
    case O::UnitNext: return P64(((RTSUnit*)p)->mTileNode->mNext);
    case O::UnitChecksum: return static_cast<EEUnit*>((RTSUnit*)p)->GetChecksum((unsigned long)a[0]);
    case O::UnitDump: static_cast<EEUnit*>((RTSUnit*)p)->OutOfSynchDump(FP(0), (unsigned long)a[1]);
        break;
    case O::ContagionSwapUnit: gWorld.GetContagionManager()->SwapUnit((EEUnit*)a[0], (EEUnit*)a[1]);
        break;
    case O::ContagionDump: gWorld.GetContagionManager()->DumpOutOfSynch(FP(0));
        break;
    case O::MapDump: gWorld.mMap->DumpOutOfSynch(FP(0));
        break;
    case O::CampaignNameLength: return gWorld.mGameOptions.mCampaignName.GetLength();
    case O::CanBuyCivilization: return gWorld.mGameOptions.CanBuyCivilization();
    case O::CheckUnitList: RTSCheckUnitList(&gWorld.mUnits, "EEPhysics::FlushWorld (start)");   // EE_CHECK_UNIT_LIST (ee/RTSDummyHeader.h)
        break;
    // a player
    case O::PlayerAddPendingUnits: PL->AddPendingUnits(0, 1);
        break;
    case O::PlayerUnitsSwapped: PL->UnitsSwapped(((EEPhysics*)a[0])->mUnitSwaps);
        break;
    case O::PlayerAvailablePoints: return static_cast<unsigned long>(PL->mCivilization.GetAvailablePoints());
    case O::PlayerSavedGroupLast: return PL->mSavedGroups.GetLastValue((unsigned long)a[0]);
    case O::PlayerRunningGroups: return PL->GetNumRunningGroups();
    case O::PlayerRunningGroup: return P64(PL->GetRunningGroup((unsigned long)a[0]));
    case O::PlayerAirTraffic: return P64(PL->mAirTraffic[a[0]]);
    // a unit
    case O::UnitReference: CX->Reference();
        break;
    case O::UnitDereference: CX->Dereference();
        break;
    // the other systems
    case O::PathManagerUpdate: gPathManager.Update();
        break;
    case O::PathManagerDump: gPathManager.DumpOutOfSynch(FP(0));
        break;
    case O::AreaEffectsTick: gAEManager.Tick(f[0]);
        break;
    case O::EffectManagerUpdate: gWorld.mEffectManager->Update();
        break;
    case O::EffectReplaceUnit: gWorld.mEffectManager->ReplaceUnit((EEUnit*)a[0], (EEUnit*)a[1]);
        break;
    case O::CalamityUpdate: EECalamityManager::GetInstance()->Update(f[0]);
        break;
    case O::CalamityDump: EECalamityManager::GetInstance()->DumpOutOfSynch(FP(0));
        break;
    case O::CalamityChecksum: return EECalamityManager::GetInstance()->GetOutOfSynchChecksum();
    case O::AiUpdate: gArtificialIntelligence->Update();
        break;
    case O::HasJitPower: return gCivPowersManager.HasPower(gWorld.GetCurrentPlayer(), kCivPowerJustInTimeManufacturing);
    case O::IsMainGameForm: return dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL;
    case O::OptionsButtonBlink: (static_cast<UIBlinkingButton*>(gUIManager->GetMainGameForm()->GetControl(kMainGameIngameOptions)))->SetIsBlinking(true);
        break;
    case O::CivTimerLimit: return EEGetCivTimerLimit();
    case O::CivTimerSecondWarning: return EEGetCivTimerSecondWarning();
    case O::CivTimerFirstWarning: return EEGetCivTimerFirstWarning();
    case O::ShiftKey: return gISKeyboard != NULL ? gISKeyboard->GetEntity(kKeyShift, *(ISEntity**)a[0]) : -1;
    case O::EntityValue: return ((ISEntity*)p)->GetValue();
    case O::SetLastAutoSave: gGamePreferences.mSettings.mLastAutoSave = UWideString((const char*)a[0]);
        break;
    case O::SavedGameExtension: snprintf((char*)a[0], (size_t)a[1], "%s", (const char*)gSavedGameExtension);
        break;
    case O::CivBuilderBlink: EESetCivBuilderButtonBlinking(a[0] != 0);
        break;
    case O::LoadOrSave: return EELoadOrSave(static_cast<EEMRecord*>(MSG));
    case O::FileOpen: return P64(PlatFOpen((const char*)a[0], "w"));
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
    case O::TimeOfDay: return gWorld.GetTimeOfDay();
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

// (UpdateUnits, the unit loop's host: ee_game physics.rs update_units)
// (gPhysicsSteppingUnit and gPhysicsAfterTickHook, TEST ONLY, are EEGlobals.cpp's)

// the unit's last global (0x531cd0, "default" 0x4b5210): its init chain follows the template COMDATs
// (EndGame: the campaign progress's user; not EEUIECampaignSelectScreenEvents.cpp's gCampaignDefaultUser 0x562ee0)
UString gCampaignUser("default");

