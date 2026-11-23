// TEST ONLY: the headless simulation's host (EE-SIM.exe; platform/PlatSim.h). Port addition, no original address.
//
// EE-SIM.exe is the game (every unit of EE-AOC.exe) with another entry (EESimMain.cpp: main -> EESimMain). It runs the
// game's own start-up, menus, lobby, loading and simulation with the front end stubbed (platform/PlatSim.h: a hidden
// window, no rendering, no sound) and, instead of a player, a driver. The driver is Rust (ee_game sim_driver: its
// options, the menus and the lobby as the harness drives them, the run on the physics thread's after-tick, the
// samples, the end of the run, the JSON report, --games' batch; EE-SIM --help; the UI's and the world's reads and the
// capture's sweeps: sim_host.rs). This file is its host: the table (EeSimDriverHost: the world lock, the platform
// switches, the C library's streams and environment, the save's and the dump's hooks), the calls and members the
// Rust reads (ee_simcpp_sim*: one case each, EeSimDriverSim), the TEST ONLY parts that read C++ objects whole: the
// per-CT checksum line's fields (--checksum-trace), the state dump (--dump-at, --dump-load, --dump-tick) and the
// physics random stream's trace (--rng-trace); and the fault handler.
#ifdef _WIN32
#include <winsock2.h>
#include <io.h>
#else
#include <execinfo.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <typeinfo>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <deque>
#include <map>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMessage.h"
#include "ee/EEMessageCapture.h"
#include "ee/EEUnitHost.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EECPEngine.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"   // EE_SCREENS_SP_LOAD_GAME (ee_screens menus.rs)
#include "rust/game/sim_driver.h"   // the driver (ee_game sim_driver)
#include "rust/game/app.h"   // the mini map's MMSN (world_capture.rs)
#include "ee/EEMiniMap.h"
#include "rust/sim/combat.h"
#include "rust/sim/netmsg.h"   // the decisions' sweep
#include "rust/sim/units.h"   // the pick and Fill sweeps (ee_sim units/pick_sweep.rs)
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/USingleton.h"
#include "ee/EEUnit.h"
#include "ee/EEFormation.h"
#include "ee/EECPTrace.h"
#include "ee/EEAppCapture.h"
#include "ee/EEPathManager.h"
#include "ee/EEMap.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitUtilities.h"     // EESearchTargetAlongList (the end sweep)
#include "ee/EEPhysicsModel.h"      // the missile sweep's flight steps
void EEEffectCaptureSweep();               // the 3D effects' sweep (ee/EEEffectManager.h)
#include "ee/EEUGRust.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIShell.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"

namespace {

// (the per-CT checksum line's fields and the state dump: sim_driver dump.rs, over the Dm cases below)

FILE* sRngFile = NULL;
// --rng-trace MS=FILE (TEST ONLY, the save/load fidelity hunt): every draw of the physics random stream after game ms
// MS (gURandomTraceHook, lle/URandom.h), one line each: the draw's number, the new seed, the game ms and the callers
// as exe-relative addresses (tools/sim.py resolve adds the map's preferred base). The same MS in a continued game and
// in the game loaded from its save at MS gives two traces whose first differing caller names the state that differs.
// At most kRngTraceMax draws; the hook reads only.
std::string sRngTraceFile;
long sRngTraceMs = -1;
unsigned long sRngLines = 0;
const unsigned long kRngTraceMax = 50000;

void RngTraceHook(URandom* inRandom)
{
    if (gPhysics == NULL || inRandom != &gPhysics->mRandom || gPhysics->mGameTime <= sRngTraceMs)
        return;
    if (sRngFile == NULL) {
        sRngFile = fopen(sRngTraceFile.c_str(), "w");
        if (sRngFile == NULL) {
            gURandomTraceHook = NULL;
            return;
        }
    }
    if (++sRngLines > kRngTraceMax) {
        fclose(sRngFile);
        sRngFile = NULL;
        gURandomTraceHook = NULL;
        return;
    }
    void* theFrames[16];
    unsigned short theCount = RtlCaptureStackBackTrace(1, 16, theFrames, NULL);
    uintptr_t theBase = (uintptr_t)GetModuleHandleA(NULL);
    fprintf(sRngFile, "%lu %08lx %ld u=%lu", sRngLines, (unsigned long)inRandom->GetSeed(), gPhysics->mGameTime,
            gPhysicsSteppingUnit != NULL ? gPhysicsSteppingUnit->mUnitID : 0UL);   // the unit being stepped (0 outside UpdateUnits)
    for (unsigned short i = 0; i < theCount; i++) {
        uintptr_t theFrame = (uintptr_t)theFrames[i];
        if (theFrame >= theBase)
            fprintf(sRngFile, " %llx", (unsigned long long)(theFrame - theBase));
    }
    fprintf(sRngFile, "\n");
}

// ------------------------------------------------------------------------------------------------ the driver's host

size_t PutText(const std::string& inText, char* outBuffer, size_t inCapacity)
{
    if (inCapacity > 0) {
        size_t n = inText.size() < inCapacity - 1 ? inText.size() : inCapacity - 1;
        memcpy(outBuffer, inText.data(), n);
        outBuffer[n] = 0;
    }
    return inText.size();
}

void HostLock(int inOp)
{
    switch (inOp) {
    case EE_SIM_DRIVER_WRITE_LOCK: gWorld.mLock->WriteLock(); break;
    case EE_SIM_DRIVER_WRITE_UNLOCK: gWorld.mLock->WriteUnlock(); break;
    case EE_SIM_DRIVER_READ_LOCK: gWorld.mLock->ReadLock(); break;
    default: gWorld.mLock->ReadUnlock(); break;
    }
}

std::string sDumpLoad;

void HostDumpAfterLoad(const char* inPath)
{
    sDumpLoad = inPath;
    EEAfterLoad([] { ee_sim_driver_dump_state(sDumpLoad.c_str()); });   // at the first tick, after the load's own fix-ups
}

size_t HostExtension(int inWhich, char* outBuffer, size_t inCapacity)
{
    return PutText(inWhich == 0 ? (const char*)gSavedGameExtension : (const char*)gScenarioExtension, outBuffer,
                   inCapacity);
}

void HostSetRun(int inRun)
{
    gPlatSimRun = inRun;
}

void AfterTickTrampoline()
{
    ee_sim_driver_after_tick();
}

void HostSetAfterTick(bool inOn)
{
    gPlatSimAfterTick = inOn ? AfterTickTrampoline : NULL;
}

void HostCancelGame()
{
    gUserInterface->PostCancelGame();
}

bool HostCaptureActive()
{
    return EESimCaptureRecord::Active();
}

long HostEngineChoice(const char* inName)
{
    return EECPChoiceByName(inName);
}

void HostSetEnv(const char* inName, const char* inValue)
{
#ifdef _WIN32
    _putenv_s(inName, inValue);
#else
    setenv(inName, inValue, 1);
#endif
}

int HostDupStderr()
{
#ifdef _WIN32
    return _dup(2);
#else
    return dup(2);
#endif
}

void SetStderr(int inFile)
{
    fflush(stderr);
#ifdef _WIN32
    _dup2(inFile, 2);
#else
    dup2(inFile, 2);
#endif
}

void HostSetStderr(const char* inFile, int inDescriptor)
{
    if (inFile == NULL) {
        SetStderr(inDescriptor);
        return;
    }
    FILE* theErr = fopen(inFile, "w");
    if (theErr != NULL) {
#ifdef _WIN32
        SetStderr(_fileno(theErr));
#else
        SetStderr(fileno(theErr));
#endif
        fclose(theErr);
    }
}

void HostWrite(int inStream, const uint8_t* inBytes, size_t inLength, bool inFlush)
{
    if (inStream >= 3) {   // a descriptor (the batch's markers on the process's stderr)
        fflush(stderr);
#ifdef _WIN32
        _write(inStream - 3, inBytes, (unsigned)inLength);
#else
        if (write(inStream - 3, inBytes, inLength) < 0)
            return;
#endif
        return;
    }
    FILE* theOut = inStream == 1 ? stdout : stderr;
    if (inLength > 0)
        fwrite(inBytes, 1, inLength, theOut);
    if (inFlush || inStream == 2)
        fflush(theOut);
}

void HostRngTrace(long inMs, const char* inPath)
{
    sRngTraceMs = inMs;
    sRngTraceFile = inPath;
    gURandomTraceHook = RngTraceHook;
}

void TickDumpTrampoline()
{
    ee_sim_driver_tick_dump();
}

void HostTickHook()
{
    gPhysicsAfterTickHook = TickDumpTrampoline;
}

void HostQuit(int inCode)
{
    fflush(stdout);
    fflush(stderr);
    if (sRngFile)
        fflush(sRngFile);
    EESimCaptureRecord::Flush();
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), (UINT)inCode);
#endif
    _exit(inCode);
}

long HostWonders(uint64_t inPlayer)
{
    return (long)reinterpret_cast<EEPlayer*>((uintptr_t)inPlayer)->GetNumWonderUnits();
}

void DebugTrampoline(const char* inText)
{
    ee_sim_driver_debug(inText);
}

}   // namespace

// (EESimPreferences is Empire Earth.cpp's, and EERun starts the driver's thread itself, ee_sim_driver_start: so the
// game's start-up names no symbol of this driver)

#ifndef _WIN32
// (Windows: the fault report is sim_driver fault.rs's)
// a fault in the simulation (POSIX): the signal, the address and the return addresses on stderr, then the signal's
// default action (the handler is reset). Linux prints the frames as ee-sim(+0xOFFSET): addr2line -f -C -e ee-sim
// 0xOFFSET names them.
static void EESimFault(int inSignal, siginfo_t* inInfo, void*)
{
    char theText[200];
    int n = snprintf(theText, sizeof(theText), "EE-SIM FAULT signal %d at %p thread %lu; frames:\n", inSignal,
                     inInfo ? inInfo->si_addr : NULL, (unsigned long)(uintptr_t)pthread_self());
    if (n > 0 && write(2, theText, (size_t)n) < 0)
        n = 0;
    void* theFrames[48];
    backtrace_symbols_fd(theFrames, backtrace(theFrames, 48), 2);
}

static void EESimInstallFaultHandler()
{
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    return;   // the sanitizer's own report is better
#endif
#endif
    void* theFrame[1];
    backtrace(theFrame, 1);   // loads the unwinder now, not inside the handler
    struct sigaction theAction;
    memset(&theAction, 0, sizeof(theAction));
    theAction.sa_sigaction = EESimFault;
    theAction.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&theAction.sa_mask);
    for (int theSignal : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(theSignal, &theAction, NULL);
}
#endif

void EESimGameEnded(int inResult);

// EESimMain: EE-SIM.exe's entry (EESimMain.cpp's main): the driver's options and switches (ee_game sim_driver), the
// debug strings and faults hooked, then the game's own WinMain. On POSIX the game's main calls it for "ee-aoc --sim
// ..." and goes on with the game when it returns -1.
int EESimMain(int argc, char** argv)
{
    static const EeSimDriverHost kHost = {
        HostLock, HostDumpAfterLoad, HostExtension, HostSetRun, HostSetAfterTick,
        HostCancelGame, HostCaptureActive, HostEngineChoice, HostSetEnv, HostDupStderr, HostSetStderr, HostWrite,
        HostRngTrace, HostTickHook, HostQuit, HostWonders};
    EeSimDriverEvents theEvents = {};
    theEvents.sp_load_game = EE_SCREENS_SP_LOAD_GAME;
    theEvents.spl_load = EE_SCREENS_SPL_LOAD;
    theEvents.pc_learning = EE_SCREENS_PC_LEARNING;
    theEvents.sp_campaign = EE_SCREENS_SP_CAMPAIGN;
    theEvents.pc_campaign[0] = EE_SCREENS_PC_CAMPAIGN_1;
    theEvents.pc_campaign[1] = EE_SCREENS_PC_CAMPAIGN_2;
    theEvents.pc_campaign[2] = EE_SCREENS_PC_CAMPAIGN_3;
    theEvents.cs_start = EE_SCREENS_CS_START;
    theEvents.si_start = EE_SCREENS_SI_START;
    theEvents.editors_open = EE_SCREENS_EDITORS_OPEN;
    theEvents.editors_scenario_editor = EE_SCREENS_EDITORS_SCENARIO_EDITOR;
    theEvents.pgs_exit = EE_SCREENS_PGS_EXIT;
    theEvents.form_post_game_summary = kFormPostGameSummary;
    theEvents.load_scenario_button = kLoadGameLoadButton;
    int theCode = ee_sim_driver_main(argc, argv, &kHost, &theEvents);
    if (theCode != EE_SIM_DRIVER_CONTINUE)
        return theCode;
    PlatDebugSetHandler(DebugTrampoline);
#ifdef _WIN32
    ee_sim_driver_fault_install("EE-SIM");
#else
    EESimInstallFaultHandler();
#if !defined(__APPLE__)
    // Linux, where a bench machine has no display: SDL's dummy video driver (the hidden window needs none, and
    // nothing is presented; macOS keeps Cocoa: its windows are Metal windows, which the dummy driver refuses). The
    // GPU device the renderer still creates is any Vulkan adapter, on a machine without a GPU Mesa's lavapipe, kept
    // to one rasterizer thread (it starts one per core, and a bench runs one simulation per core). The environment
    // wins over both.
    setenv("SDL_VIDEO_DRIVER", "dummy", 0);
    setenv("LP_NUM_THREADS", "1", 0);
#endif
#endif
#ifdef _WIN32
    char theEmpty[1] = {0};
    EESimGameEnded(WinMain(GetModuleHandleA(NULL), NULL, theEmpty, SW_HIDE));
    return 4;
#else
    return -1;   // the POSIX main goes on with the game (Empire Earth.cpp: ee-aoc --sim ...)
#endif
}

// EESimGameEnded: the game's own start-up returned before the driver ended the run (it was quit, or failed to start)
void EESimGameEnded(int inResult)
{
    ee_sim_driver_game_ended(inResult);
}

// ------------------------------------------------------------------------------------------------ the Rust's calls

#define P64(x) ((int64_t)(intptr_t)(x))
#define UF ((UIForm*)p)
#define CTL ((UIControl*)p)
#define PL ((EEPlayer*)p)
#define UN ((EEUnit*)p)
#define CX ((EEComplexUnit*)p)
#define UC ((EEUnitClass*)p)

static std::string sRulesError;             // EERulesAddOverride's problem
static std::vector<std::string> sProblems;   // EERulesOverrideProblems'

// The driver's C++ calls and members (sim_ops.rs EeSimDriverSim): p the object, a and f the arguments, s a text
extern "C" int64_t ee_simcpp_sim(EeSimDriverSim inOp, uintptr_t p, const int64_t* a, const float* f, const char* s)
{
    typedef EeSimDriverSim O;
    switch (inOp) {
    // the user interface
    case O::UiPresent: return gUIManager != NULL;
    case O::UiLock: gUIManager->LockUIForCopyData();
        break;
    case O::UiUnlock: gUIManager->UnlockUIForCopyData();
        break;
    case O::CurrentForm: return P64(gUIManager->GetCurrentForm());
    case O::EditorForm: return P64(static_cast<UIForm*>(gUIManager->GetScenarioEditorForm()));
    case O::FormId: return UF->GetFormID();
    case O::FormShellOf: return P64(EEUIShell::Of(UF));
    case O::FormControlCount: return UF->GetControlCount();
    case O::FormControl: return P64(UF->GetControl((unsigned long)a[0]));
    case O::ControlTriggerEvent: return P64(CTL->GetTriggerEvent((unsigned long)a[0]));
    case O::ControlFire: CTL->FireTriggerEvent((unsigned long)a[0]);
        break;
    case O::ComboOf: return P64(dynamic_cast<UIComboBox*>(CTL));
    case O::ComboList: return P64(((UIComboBox*)p)->GetListBox());
    case O::ComboSelection: return ((UIComboBox*)p)->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::ComboSetSelection: ((UIComboBox*)p)->SetCurrentSelection((unsigned long)a[0]);
        break;
    case O::ListOf: return P64(dynamic_cast<UILBString*>(CTL));
    case O::ListLineCount: return ((UILBString*)p)->GetLineCount();
    case O::ListSelection: return ((UILBString*)p)->GetCurrentSelection(*(unsigned long*)a[0]);
    case O::ListChangeSelection: ((UILBString*)p)->ChangeSelectionIndex((unsigned long)a[0], a[1] != 0, a[2] != 0);
        break;
    case O::ScreenEventOf: return P64(dynamic_cast<EEScreenEvent*>((UIEvent*)p));
    case O::ScreenEventCode: return ((EEScreenEvent*)p)->mEvent;
    case O::SpLoadGameFileList: return kSPLoadGameFileListBox;
    case O::LoadScenarioFileList: return kLoadGameLoadFileListBox;
    // the physics and the world
    case O::PhysicsPresent: return gPhysics != NULL;
    case O::PhysicsTimerRunning: return gPhysics->mTimerRunning;
    case O::PhysicsGameOver: return gPhysics->mGameOver;
    case O::PhysicsGameTime: return gPhysics->mGameTime;
    case O::PhysicsTicksDone: return gPhysics->mNumTicksDone;
    case O::PhysicsTickMs: return gPhysics->mTickTimeMs;
    case O::PhysicsFlushPlacements: gWorld.FlushQueuedPlacements();
        break;
    case O::MapPresent: return gWorld.GetMap() != NULL;
    case O::CurrentPlayer: return P64(gWorld.GetCurrentPlayer());
    case O::NumPlayers: return gWorld.GetNumPlayers();
    case O::GetPlayer: return P64(gWorld.GetPlayer((unsigned long)a[0]));
    case O::WorldNumPlayers: return gWorld.mNumPlayers;
    case O::WorldPlayersAt: return P64(gWorld.mPlayers[a[0]]);
    case O::PlayerSetCivilizationChosen: PL->mCivilizationChosen = a[0] != 0;
        break;
    case O::PlayerSetCivilizationLocked: PL->mCivilizationLocked = a[0] != 0;
        break;
    case O::PlayerSetStance: PL->SetDiplomaticStance((EEPlayer*)a[0], 0, gWorld.GetMap());
        break;
    case O::PlayerFirstUnit: return P64(PL->mFirstUnit);
    case O::PlayerAllyCount: return PL->mAllies.size();
    case O::PlayerAllyAt: return P64(PL->mAllies[a[0]]);
    case O::PlayerFlareReceived: PL->FlareReceived((EEPlayer*)a[0], U2DPoint<long>((long)a[1], (long)a[2]));
        break;
    case O::PlayerTechnologies: return P64(PL->mTechnologies);
    case O::PlayerId: return PL->GetPlayerID();
    case O::PlayerAddPendingUnits: PL->AddPendingUnits(false, true);
        break;
    case O::TreeGet: return P64(((EETechTree*)p)->Get((long)a[0]));
    case O::NodeData: return P64(((EETechTreeNode*)p)->mData);
    case O::NodeCost: return ((EETechTreeNode*)p)->GetCost((long)a[0], (unsigned long)a[1], 1.0f);
    case O::CastLocationCalamity:
        return USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity(9, 0xffffffffUL, (long)a[0], (long)a[1],
                                                                                    (unsigned long)a[2]);
    case O::VolcanoProjectileType: return kUnitTypeVolcanoProjectile;
    case O::RequestSaveNew:   // load, quiet, tick, time, auto-save, the wait id, the name
        return P64(static_cast<NEMessage*>(new EEMRecord(kMessageLoadOrSaveGame,
            {0, true, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 0, 0, UWideString(s)})));
    case O::SendToAllAndWait: static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait((NEMessage*)p, 15000, 12);
        break;
    case O::DerefMessage: ((NEMessage*)p)->Dereference();
        break;
    // the platform switches
    case O::SetSimMode: gPlatSimMode = 1;
        break;
    case O::SetSeedSet: gPlatSimSeedSet = (int)a[0];
        break;
    case O::SetSeed: gPlatSimSeed = (uint32_t)a[0];
        break;
    case O::SetNoHumanDefeat: gPlatSimNoHumanDefeat = (int)a[0];
        break;
    case O::SetScenarioDialogs: gScenarioSimDialogs = a[0] != 0;
        break;
    case O::SetPortOffset: gPlatSimPortOffset = (int)a[0];
        break;
    case O::AnyPort: return kPlatSimAnyPort;
    case O::ClearNoHumanDefeatHits: gPlatSimNoHumanDefeatHits = 0;
        break;
    case O::NoHumanDefeatHits: return (long)gPlatSimNoHumanDefeatHits;
    // the rules
    case O::RulesAdd: return EERulesAddOverride(s, sRulesError);
    case O::RulesProblemsLoad: EERulesOverrideProblems(sProblems);
        break;
    case O::RulesProblemCount: return sProblems.size();
    // the units
    case O::UnitNext: return P64(static_cast<EEUnit*>(UN->mNext));
    case O::UnitComplex: return P64(UN->mComplexUnit);
    case O::ComplexOf: return P64(UN->mComplexUnit);
    case O::UnitGetComplex: return P64(UN->GetComplexUnit());
    case O::UnitHitPoints: return UN->mHitPoints;
    case O::UnitHasStats: return UN->mStats != NULL;
    case O::UnitIsBuilding: return UN->mUnitClass->IsBuilding();
    case O::UnitType: return UN->mUnitClass->GetUnitType();
    case O::UnitId: return UN->GetUnitID();
    case O::UnitTileX: return UN->GetTileX();
    case O::UnitTileY: return UN->GetTileY();
    case O::ComplexHitPoints: return CX->mHitPoints;
    case O::ComplexOnMap: return CX->mTileNode != NULL;
    case O::ComplexTileX: return CX->GetTileX();
    case O::ComplexTileY: return CX->GetTileY();
    case O::UnitTileXRaw: return CX->mTileX;
    case O::ComplexIsPriest: return CX->GetUnitClass()->IsPriest();
    case O::ComplexCategory: return CX->mUnitClass->mCategory;
    case O::ComplexClass: return P64(CX->mUnitClass);
    case O::ComplexLocation: return P64(&CX->mPhysicalModel.GetLocation());
    case O::ComplexSetLocation: CX->mPhysicalModel.SetLocation(*(const GETransformation*)a[0]);
        break;
    case O::ComplexSetLauncherClass: CX->SetLauncherClass((EEUnitClass*)a[0]);
        break;
    case O::ComplexSetOwner: CX->SetOwner((EEComplexUnit*)a[0]);
        break;
    case O::ComplexPosition: CX->GetPosition(*(GE3DPoint*)a[0]);
        break;
    case O::ComplexSetGoal: CX->SetGoal((EEUnitGoal*)a[0], false, false);
        break;
    case O::ComplexSetVelocity: CX->SetVelocity(f[0]);
        break;
    case O::ComplexFlightTime: return CX->mFlightTime;
    case O::ComplexSetFlightTime: CX->SetFlightTime((long)a[0]);
        break;
    case O::ComplexHasGoalState: return CX->mGoalState != NULL;
    case O::ComplexPlayer: return P64(CX->mPlayer);
    case O::IgnoredSave: return P64(new std::deque<EEComplexUnitAttacker>(CX->mIgnoredTargets));
    case O::IgnoredRestore: CX->mIgnoredTargets = *(std::deque<EEComplexUnitAttacker>*)a[0];
        break;
    case O::IgnoredFree: delete (std::deque<EEComplexUnitAttacker>*)p;
        break;
    case O::SearchAlongList: EESearchTargetAlongList(CX, (long)a[0], (long)a[1], f[0]);
        break;
    case O::NewAttackUnit: return P64(EEUGRust::NewAttackUnit(CX, (EEComplexUnit*)a[0], false, false));
    case O::NewAttackGround: return P64(EEUGRust::NewAttackGround(GE3DPoint(f[0], f[1], f[2]), false));
    case O::CanHeal: EECanHeal(CX, (EEComplexUnit*)a[0]);
        break;
    case O::UpgradeProjectile: return P64(((EEUnitClassUpgrade*)p)->mUnitClass->mProjectileClass);
    // the unit classes
    case O::ClassCount: return gUnitClassTable.size();
    case O::ClassAt: return P64(gUnitClassTable[a[0]]);
    case O::ClassCategory: return UC->mCategory;
    case O::ClassProjectile: return P64(UC->mProjectileClass);
    case O::ClassCreateUnit: return P64(UC->CreateUnit((long)a[0], (long)a[1], (unsigned long)a[2], false, (EEComplexUnit*)a[3]));
    case O::ClassLaunchProjectile: UC->LaunchProjectile((EEComplexUnit*)a[0], GE3DPoint(f[0], f[1], f[2]));
        break;
    case O::ClassStep: UC->mPhysicsModel->Step0((EEComplexUnit*)a[0]);
        break;
    case O::ClassWaterImpact: return UC->CheckWaterImpact((EEComplexUnit*)a[0]);
    case O::ClassGroundImpact: return UC->CheckGroundImpact((EEComplexUnit*)a[0]);
    case O::MissileCategory: return kUnitCategoryMissile;
    case O::TorpedoCategory: return kUnitCategoryTorpedo;
    // the capture and its sweeps
    case O::CaptureActive: return EESimCaptureRecord::Active();
    case O::CaptureOnly: EESimCaptureRecord::Only((unsigned int)a[0]);
        break;
    case O::CaptureOnlyList: EESimCaptureRecord::Only((const unsigned int*)a[0], (size_t)a[1]);
        break;
    case O::CaptureExclusive: EESimCaptureRecord::Exclusive(a[0] != 0);
        break;
    case O::CaptureHold: EESimCaptureRecord::Hold(a[0] != 0);
        break;
    case O::CaptureClose: EESimCaptureRecord::Close();
        break;
    case O::CombatFlush: EECombatCaptureFlush();
        break;
    case O::FormationSweep: EEFormationCaptureSweep();
        break;
    case O::TechSweep: EETechCaptureSweep();
        break;
    case O::CalamitySweep: EECalamityCaptureSweep();
        break;
    case O::GroupCommandSweep: EEGroupCommandSweep();
        break;
    case O::MessageSweep: EEMessageCaptureSweep();
        break;
    case O::CastCalamities: EETechCaptureCastCalamities((long)a[0]);
        break;
    case O::MiniMapPresent: return gMiniMap != NULL;
    case O::MiniMapSnapshot: ee_app_capture_minimap_snapshot(gMiniMap->mRust, gMiniMap->mPixels, gMiniMap->mPixelsPerRow);
        break;
    case O::SightSnapshot: EESightCaptureSnapshot();
        break;
    case O::CheatSweep: EEAppCaptureCheatSweep();
        break;
    case O::StormSweep: EEAppCaptureStormSweep();
        break;
    case O::EffectSweep: EEEffectCaptureSweep();
        break;
    case O::StatisticsSweep: EEAppCaptureStatisticsSweep();
        break;
    case O::RegenerationInterval: return EEGetRules().mCombat.mRegenerationInterval;
    case O::ConversionFaithCost: return gWorldTable.GetLongValue(kWorldConversionFaithCost);
    case O::DialogShowing:
        return gScenarioSimDialogs && gWorld.mScenarioManager != NULL && gWorld.mScenarioManager->mIsShowingDialog;
    case O::ShowDialogMessage: gWorld.mScenarioManager->ShowDialogMessage();
        break;
    // the map's sight
    case O::MapSize: return gWorld.mMap->mSize;
    case O::MapAddSight: gWorld.mMap->AddSight(PL, f[0], f[1], f[2], a[0] != 0);
        break;
    case O::MapRemoveSight: gWorld.mMap->RemoveSight(PL, f[0], f[1], f[2], a[0] != 0);
        break;
    case O::MapExploreUnseen: gWorld.mMap->ExploreUnseen(PL);
        break;
    case O::MapFogSeen: gWorld.mMap->FogSeen(PL);
        break;
    case O::MapComputeChecksum: gWorld.mMap->ComputeChecksum();
        break;
    // the state dump (sim_driver dump.rs)
    case O::DmPhysLastOos: return gPhysics->mLastOutOfSynchChecksum;
    case O::DmPhysSetLastOos: gPhysics->mLastOutOfSynchChecksum = (unsigned long)a[0];
        break;
    case O::DmPhysOos: return gPhysics->GetOutOfSynchChecksum(false, (unsigned long)a[0]);
    case O::DmPhysTicksDone: return gPhysics->mNumTicksDone;
    case O::DmPhysCurrentTick: return gPhysics->mCurrentTick;
    case O::DmPhysSeed: return (uint32_t)gPhysics->mRandom.GetSeed();
    case O::DmPhysInputs: return gPhysics->mInputMessagesProcessed;
    case O::DmPhysTickRandom: return (long)gPhysics->mTickRandom;
    case O::DmPhysTicksToProcess: return (long)gPhysics->mTicksToProcess;
    case O::DmCpOos: return (uint32_t)EECPOutOfSynchChecksum();
    case O::DmWorldFirstUnit: return P64(static_cast<EEUnit*>(gWorld.mUnits.mFirst));
    case O::DmWorldNextUnit: return P64(static_cast<EEUnit*>(UN->mTileNode->mNext));
    case O::DmUnitChecksum: return (uint32_t)UN->GetChecksum((long)a[0]);
    case O::DmComplexChecksum: return (uint32_t)CX->GetChecksum((long)a[0]);
    case O::DmActiveUnitId: return EEUnitHandle<EEComplexUnit>::FromSavedID((uint32_t)a[0])->mUnitID;
    case O::DmPlPop: return (long)PL->mPopulationCount;
    case O::DmPlEog: return (long)PL->mEndOfGameCount;
    case O::DmPlEpoch: return PL->mTechnologies ? (long)PL->mTechnologies->mCurrentEpoch : -1L;
    case O::DmPlTownCount: return PL->mTowns.size();
    case O::DmPlTownAt: return P64(PL->mTowns[a[0]]);
    case O::DmTownId: return (long)((EETown*)p)->mTownID;
    case O::DmTownUnits: return (long)((EETown*)p)->mNumUnits;
    case O::DmTownPatrol: return P64(((EETown*)p)->mPatrolGroup);
    case O::DmTownHullCount: return ((EETown*)p)->GetHullPointCount();
    case O::DmTownHullX: return (long)((EETown*)p)->GetHullPoint((unsigned long)a[0]).GetX();
    case O::DmTownHullY: return (long)((EETown*)p)->GetHullPoint((unsigned long)a[0]).GetY();
    case O::DmPlPatrolCount: return PL->mPatrolGroups.size();
    case O::DmPlPatrolAt: return P64(PL->mPatrolGroups[a[0]]);
    case O::DmPgId: return (unsigned long)ee_units_pg_id((EEPPatrolGroup*)p);
    case O::DmPgTown: return ee_units_pg_is_town_patrol((EEPPatrolGroup*)p) ? 1 : 0;
    case O::DmPgLoop: return ee_units_pg_is_loop((EEPPatrolGroup*)p) ? 1 : 0;
    case O::DmPgCount: return ee_units_pg_point_count((EEPPatrolGroup*)p);
    case O::DmPgPoint: return P64(static_cast<EEComplexUnit*>(ee_units_pg_point((EEPPatrolGroup*)p, (size_t)a[0])));
    case O::DmCxUnitId: return CX->mUnitID;
    case O::DmCxTileYRaw: return CX->mTileY;
    case O::DmUnitIdRaw: return UN->mUnitID;
    case O::DmCpDump: EECPOutOfSynchDump((FILE*)a[0]);
        break;
    case O::DmPathDump: gPathManager.DumpSim((FILE*)a[0]);
        break;
    case O::DmUnitTypeRaw: return (long)UN->mUnitClass->mUnitType;
    case O::DmUnitPlayerId: return UN->GetPlayer() ? (long)UN->GetPlayer()->mPlayerID : -1L;
    case O::DmUnitTileXRaw: return (long)UN->mTileX;
    case O::DmUnitTileYRaw: return (long)UN->mTileY;
    case O::DmComplexOf: return P64(dynamic_cast<EEComplexUnit*>(UN));
    case O::DmCxComplex: return P64(CX->mComplexUnit);
    case O::DmCxUpdated: return (unsigned long)CX->mTimesUpdated;
    case O::DmCxCommand: return CX->mCommand ? (long)CX->mCommand->mType : -1L;
    case O::DmCxGroup: return CX->mActiveGroup != NULL;
    case O::DmCxWpCount: return CX->mWaypoints.size();
    case O::DmCxPathDone: return CX->mIsPathDone ? 1 : 0;
    case O::DmCxArrived: return CX->mHasArrived ? 1 : 0;
    case O::DmCxBlockingId: return CX->mBlockingUnit != NULL ? CX->mBlockingUnit->mUnitID : 0UL;
    case O::DmCxAnim: return (unsigned long)CX->GetAnimation();
    case O::DmCxFrame: return (unsigned long)CX->mPhysicalModel.GetFrame();
    case O::DmCxAnimTime: return (unsigned long)CX->mAnimationTime;
    case O::DmCxPaused: return CX->mIsAnimationPaused ? 1 : 0;
    case O::DmCxReload: return (long)CX->mReloadTimer;
    case O::DmCxQueues: return (long)CX->GetNumGoalQueues();
    case O::DmCxQueueSize: return CX->GetQueueGoals((long)a[0]).size();
    case O::DmCxGoal: return P64(CX->GetQueueGoals((long)a[0])[(size_t)a[1]]);
    case O::DmGoalKind: return (unsigned)((EEUnitGoal*)p)->mKind;
    case O::DmGoalOos: return (uint32_t)((EEUnitGoal*)p)->GetOutOfSynchChecksum();
    case O::DmGoalDump: ((EEUnitGoal*)p)->DumpOutOfSynch((FILE*)a[0]);
        break;
    case O::DmCxContactHead: return P64(CX->mContacts.mHead);
    case O::DmNodeNext: return P64(((UListNode<EEComplexUnit*>*)p)->mNext);
    case O::DmNodeUnitId: return ((UListNode<EEComplexUnit*>*)p)->mData != NULL ? (unsigned long)((UListNode<EEComplexUnit*>*)p)->mData->GetUnitID() : 0UL;
    case O::DmCxAttackerId: return CX->mLastAttacker != NULL ? (unsigned long)CX->mLastAttacker->GetUnitID() : 0UL;
    case O::DmCxOwnerId: return CX->mOwner != NULL ? (unsigned long)CX->mOwner->GetUnitID() : 0UL;
    case O::DmCxHolderId: return CX->mHolder != NULL ? (unsigned long)CX->mHolder->GetUnitID() : 0UL;
    case O::DmCxAeCount: return CX->mAreaEffects.mItems.size();
    case O::DmCxAffectingCount: return CX->mAffectingAreaEffects.mItems.size();
    case O::DmCxAffectingUnitId: return CX->mAffectingAreaEffects.mItems[a[0]]->mUnit != NULL ? (unsigned long)CX->mAffectingAreaEffects.mItems[a[0]]->mUnit->GetUnitID() : 0UL;
    case O::DmCxSize: return sizeof(EEComplexUnit);
    case O::DmUnitSize: return sizeof(EEUnit);
    case O::DmCalManager: return P64(EECalamityManager::GetInstance());
    case O::DmCalCount: return ((EECalamityManager*)p)->mCalamities.size();
    case O::DmCalAt: return P64(((EECalamityManager*)p)->mCalamities[a[0]]);
    case O::DmCalClass: return ((EECalamity*)p)->mClass != NULL ? (long)((EECalamity*)p)->mClass->mClassEnum : -1L;
    case O::DmCalOos: return (uint32_t)((EECalamity*)p)->GetOutOfSynchChecksum();
    case O::DmCalTileX: return (unsigned long)((EECalamity*)p)->mTilePlaceHolder1.mX;
    case O::DmCalTileY: return (unsigned long)((EECalamity*)p)->mTilePlaceHolder1.mY;
    case O::DmCalInt1: return ((EECalamity*)p)->mIntPlaceHolder1;
    case O::DmCalInt2: return ((EECalamity*)p)->mIntPlaceHolder2;
    case O::DmCalTargetId: return ((EECalamity*)p)->mTargetUnitID;
    case O::DmCalOriginId: return ((EECalamity*)p)->mOriginatingUnitID;
    case O::DmCalTargetUnitId: return ((EECalamity*)p)->mTargetUnit != NULL ? (unsigned long)((EECalamity*)p)->mTargetUnit->GetUnitID() : 0UL;
    case O::DmCalCasterId: return ((EECalamity*)p)->mCasterUnit != NULL ? (unsigned long)((EECalamity*)p)->mCasterUnit->GetUnitID() : 0UL;
    case O::DmCalPlayer: return ((EECalamity*)p)->mPlayerNumber;
    case O::DmCalBool1: return (int)((EECalamity*)p)->mBoolPlaceHolder1;
    case O::DmCalBool2: return (int)((EECalamity*)p)->mBoolPlaceHolder2;
    case O::DmCalInitialized: return (int)((EECalamity*)p)->mInitialized;
    case O::DmCalHasLocation: return (int)((EECalamity*)p)->mHasTargetLocation;
    case O::DmCalHasUnit: return (int)((EECalamity*)p)->mHasTargetUnit;
    case O::DmMapRegions: return gWorld.mMap != NULL ? P64(gWorld.mMap->mUnitRegions) : 0;
    case O::DmOpen: return P64(fopen(s, "w"));
    case O::DmPuts: fputs(s, (FILE*)p);
        break;
    case O::DmClose: fclose((FILE*)p);
        break;
    default: break;
    }
    return 0;
}

// The driver's texts: copied into sText, valid until the next text
extern "C" const char* ee_simcpp_sim_text(EeSimDriverSim inOp, uintptr_t p, int64_t a)
{
    typedef EeSimDriverSim O;
    static std::string sText;
    switch (inOp) {
    case O::ShellKindName: return (sText = ((EEUIShell*)p)->KindName()).c_str();
    case O::FormTypeName: return (sText = typeid(*UF).name()).c_str();
    case O::ListData: return (sText = (const char*)((UILBString*)p)->GetData((unsigned long)a)).c_str();
    case O::RulesProblem: return (sText = sProblems[a]).c_str();
    case O::RulesError: return sRulesError.c_str();
    case O::EngineId: return (sText = EECPAIID(ee_cp_engine_ai_at((size_t)a))).c_str();
    default: break;
    }
    return NULL;
}

// The driver's floats
extern "C" double ee_simcpp_sim_float(EeSimDriverSim inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeSimDriverSim O;
    switch (inOp) {
    case O::PhysicsTickSeconds: return gPhysics->mTickSeconds;
    case O::ComplexAttack: return CX->mStats->GetAttack();
    case O::ComplexVelocity: return CX->GetVelocity();
    case O::ClassAcceleration: return UC->GetAcceleration();
    case O::TradeFoodWood: return gWorldTable.GetFloatValue(kWorldTradeRateFoodWood);
    case O::TradeFoodWood2: return gWorldTable.GetFloatValue2(kWorldTradeRateFoodWood);
    case O::TradeStoneIron: return gWorldTable.GetFloatValue(kWorldTradeRateStoneIron);
    case O::TradeStoneIron2: return gWorldTable.GetFloatValue2(kWorldTradeRateStoneIron);
    case O::WonderSightRadius: return gWorldTable.GetFloatValue(kWorldWonderSightRadius);
    // the state dump (sim_driver dump.rs)
    case O::DmPhysClock: return (double)gPhysics->mGameClock;
    case O::DmPlResource: return PL->mResourceCaps[kResourceFood + a[0]];
    case O::DmUnitHp: return UN->mHitPoints;
    case O::DmCxLocX: return CX->mPhysicalModel.GetLocation().GetX();
    case O::DmCxLocY: return CX->mPhysicalModel.GetLocation().GetY();
    case O::DmCxLocZ: return CX->mPhysicalModel.GetLocation().GetZ();
    case O::DmCxHeight: return CX->mHeight;
    case O::DmCxWpX: return CX->mWaypoints[a[0]].mLocation.GetX();
    case O::DmCxWpY: return CX->mWaypoints[a[0]].mLocation.GetY();
    case O::DmCxWpZ: return CX->mWaypoints[a[0]].mLocation.GetZ();
    case O::DmCxWpDistance: return CX->mWaypoints[a[0]].mDistance;
    case O::DmCxWpStart: return CX->mWaypoints[a[0]].mStartDistance;
    case O::DmCxAeX: return CX->mAreaEffects.mItems[a[0]]->mLocation.mX;
    case O::DmCxAeY: return CX->mAreaEffects.mItems[a[0]]->mLocation.mY;
    case O::DmCalTargetX: return ((EECalamity*)p)->mTargetLocation.GetX();
    case O::DmCalTargetY: return ((EECalamity*)p)->mTargetLocation.GetY();
    case O::DmCalTargetZ: return ((EECalamity*)p)->mTargetLocation.GetZ();
    case O::DmCalFloat1: return ((EECalamity*)p)->mFloatPlaceHolder1;
    case O::DmCalFloat2: return ((EECalamity*)p)->mFloatPlaceHolder2;
    case O::DmCalHeight: return ((EECalamity*)p)->mHeight;
    case O::DmCalEpicenter: return ((EECalamity*)p)->mEpicenterFactor;
    case O::DmCalTimeAlive: return ((EECalamity*)p)->mTimeAlive;
    case O::DmCalDiscrete: return ((EECalamity*)p)->mDiscreteUpdateTime;
    case O::DmCalElapsed: return ((EECalamity*)p)->mTimeElapsedSinceLastUpdate;
    default: break;
    }
    return 0.0;
}
