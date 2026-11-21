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
#include <memory>
#include "ee/EEGameLog.h"
#include "ee/EECPNext.h"
// chain at 0x551c28 (wrapper)
//   object 0x931ce0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gPhysicsDataKey("EEPhysics Data");

// the unit's other data (provisional names): the area effects' update period (0x4422f8, a unit-local .rdata float)
// (the physics totals 0x531cf0 / 0x531cf4 are EEPhysics::mInputMessagesProcessed / mLastOutOfSynchChecksum)
static const float kAreaEffectInterval = 2000.0f;
extern UString gCampaignUser;              // 0x531cd0 (defined at the end of this unit; provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (the main game form's blinking buttons)
unsigned long EEGetCivTimerLimit();         // 0x27ffae (cdecl: 300000; the civ timer limits, EEUIFMainGame.cpp's range)
unsigned long EEGetCivTimerSecondWarning();  // 0x27ffba (270000) (provisional names)
unsigned long EEGetCivTimerFirstWarning();  // 0x27ffb4 (240000)
extern unsigned long& gContinentManagerChecksum;  // 0x530d10 (the continent manager's checksum; provisional name)
extern unsigned long gCollisionPeriod;      // 0x4c237c (.data = 1 after gCollisionDetectionUnit; provisional name)

// Creates the physics simulation: makes its save-game lock, resets its clocks and starts the thread's main loop
EEPhysics::EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer)
    : RTSPhysics(inGraphics, inTimer, 1, 0xf), mInputMessagesProcessed(0), mLastOutOfSynchChecksum(0)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSaveGameLock) == keseOK, 281);
    this->Init();
    this->mStartThreadMainLoop->Set();
}

// Destroys the physics simulation, releasing its save-game lock
EEPhysics::~EEPhysics()
{
    TSScheduler::DestroyCriticalSection(this->mSaveGameLock);
}

// Autosaves when autosaves are on: names the file "AutoSave <date - time>", records it in the preferences
// and sends every player the save request (waiting up to 15 s)
void EEPhysics::AutoSave()
{
    ee_units_ph_body(EE_UNITS_PH_DO_AUTOSAVE, this, this, 0, 0, 0);   // phys_body.rs
}

// Drops every queued server message (under the server queue lock)
void EEPhysics::ClearServerMessages()
{
    ee_units_ph_body(EE_UNITS_PH_DO_CLEAR_SERVER_MESSAGES, this, this, 0, 0, 0);   // phys_body.rs
}

// Places the queued units, adds every player's pending units, applies the queued unit swaps, then removes
// every player's killed units
void EEPhysics::FlushWorld()
{
    ee_units_ph_body(EE_UNITS_PH_DO_FLUSH_WORLD, this, this, 0, 0, 0);   // phys_body.rs
}

// Runs every owed physics tick, draining the input queue for more whenever none are left
void EEPhysics::RunOwedTicks()
{
    ee_units_ph_body(EE_UNITS_PH_DO_RUN_OWED_TICKS, this, this, 0, 0, 0);   // phys_body.rs
}

// Ends the game: every non-observer player but player 0 wins or loses, a won campaign scenario is recorded,
// the in-game options button blinks, the teams are cleared and a server pauses the game
void EEPhysics::EndGame(bool inIsVictory)
{
    ee_units_ph_body(EE_UNITS_PH_DO_END_GAME, this, this, inIsVictory, 0, 0);   // phys_body.rs
}

// Returns the world's out-of-synch checksum (inFlags picks the parts: map, calamities, air traffic, continents,
// selections); inDump also writes everything to "OOS <date>.log"
// NEAR: frame 8 bytes smaller (two temporaries of the out-of-line begin()/operator+); register choice
unsigned long EEPhysics::GetOutOfSynchChecksum(bool inDump, unsigned long inFlags)
{
    return (unsigned long)ee_units_ph_body(EE_UNITS_PH_DO_OUT_OF_SYNCH_CHECKSUM, this, this, inDump, inFlags, 0);   // phys_body.rs
}

// Returns the game speed and elapsed game time as text ("<speed> hh:mm:ss"); the time-of-day conversions are
// computed and unused
// NEAR: the time of day is also copied to a frame float (fld [ebp-4]) in the original
UWideString EEPhysics::GetTimeString()
{
    UWideString theString;
    ee_units_ph_body(EE_UNITS_PH_DO_TIME_STRING, this, this, (uint64_t)(uintptr_t)&theString, 0, 0);   // phys_body.rs
    return theString;
}

// Splits a game time in ms (0: the current one) into whole minutes and the seconds past them
void EEPhysics::GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime)
{
    ee_units_ph_body(EE_UNITS_PH_DO_MINUTES_AND_SECONDS, this, this, (uint64_t)(uintptr_t)outMinutes, (uint64_t)(uintptr_t)outSeconds, (int64_t)inTime);   // phys_body.rs
}

// Returns the unit's role in the queued unit swaps (the earliest wins): old 1, new 3; other owner 2 / 4;
// citizen to non-citizen 5 / 7, non-citizen to citizen 6 / 8; 0 when it is not swapped
// NEAR: inline budget: the original calls begin() out of line from the second mSound2Index7[i] on; mIsCitizen through a cast
// (EEPUnitSwap holds EEUnit*)
long EEPhysics::GetUnitSwapKind(EEComplexUnit* inUnit)
{
    return (long)ee_units_ph_body(EE_UNITS_PH_DO_UNIT_SWAP_KIND, this, this, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), 0, 0);   // phys_body.rs
}

// Resets the simulation's clocks, tick counters and game flags and drops the queued server messages and
// load/save requests
void EEPhysics::Init()
{
    ee_units_ph_body(EE_UNITS_PH_DO_INIT, this, this, 0, 0, 0);   // phys_body.rs
}

// port: the random seed a saved game was saved with, put back at the loaded game's first tick. The physics chunk is
// read before the load has finished: rebuilding the players draws from the physics random numbers afterwards (the
// computer players' favourite unit types as their epochs are entered again, herds, patrol points, the lobby's
// player records), so the loaded game played on from another seed than the game that was saved (R5a.2, "load
// equals continue"). Not saved state: a load sets it, the first tick spends it.
static unsigned long sLoadedSeed = 0;
static bool sLoadedSeedPending = false;
static std::vector<std::function<void()> > sAfterLoad;   // EEAfterLoad's (ee/EESaveGameFiles.h)

void EEAfterLoad(std::function<void()> inFixup)
{
    sAfterLoad.push_back(inFixup);
}

static std::vector<std::function<void()> > sAfterLoadLast;   // EEAfterLoadLast's

void EEAfterLoadLast(std::function<void()> inFixup)
{
    sAfterLoadLast.push_back(inFixup);
}

// New (R5b.4, round 28): the C++ parts of EEPhysics::Load and Save (ee_units physics_save.rs): the file's primitives
// (EESavePrimitive) and RTSPhysics' part
struct EEPhysicsSaveContext {
    EEPhysics* mPhysics;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
};

static int32_t EEPhysicsSaveOperation(void* inContext, int32_t inOp, uint32_t, EeUnitsPlayerSaveIo* io)
{
    EEPhysicsSaveContext& theContext = *static_cast<EEPhysicsSaveContext*>(inContext);
    if (inOp == EE_UNITS_PHYSICS_SAVE_RTS_LOAD)
        return theContext.mPhysics->RTSPhysics::Load(theContext.mMajorVersion, theContext.mMinorVersion, *theContext.mFile);
    if (inOp == EE_UNITS_PHYSICS_SAVE_RTS_SAVE)
        return theContext.mPhysics->RTSPhysics::Save(theContext.mMajorVersion, theContext.mMinorVersion, *theContext.mFile);
    return EESavePrimitive(*theContext.mFile, inOp, io);
}

// Reads the physics clocks, tick counters, timing averages and civ timer state from a saved game (the game-over
// flag after version 0.0xce, the resource timer from 0xfa); returns the first read error
long EEPhysics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // port (R5b.4, round 28): the original's two era bodies are Rust (ee_units physics_save.rs) over the record this
    // class derives from (EeUnitsPhysicsSaved) and EEPhysicsSaveOperation's parts
    EEPhysicsSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion};
    long theResult = ee_units_physics_load(&theContext, &EEPhysicsSaveOperation, static_cast<EeUnitsPhysicsSaved*>(this),
                                           (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
    if (theResult == kErrorOK) {   // port: the loaded seed comes back at the first tick (sLoadedSeed)
        sLoadedSeed = this->mRandom.GetSeed();
        sLoadedSeedPending = true;
    }
    return theResult;
}

// Takes the world's write lock
void EEPhysics::Lock()
{
    gWorld.mLock->WriteLock();
}

// Appends a message to the server message queue (no lock taken)
void EEPhysics::QueueServerMessage(NEMessage* inMessage)
{
    this->mServerMessages.push_back(inMessage);
}

// Handles the physics thread's messages: start (restarts the tick clock), time of day toggle / reset, scenario
// dialog, re-init, autosave, a load/save request (queued with no lock), 0x50c; passes the rest to the base
void EEPhysics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    ee_units_ph_body(EE_UNITS_PH_DO_HANDLE_MESSAGE, this, this, (uint64_t)(uintptr_t)inParameter1, (uint64_t)(uintptr_t)inParameter2, (int64_t)inMessage);   // phys_body.rs
}

// With the free camera on, drains the input queue and flushes the world under the world lock
void EEPhysics::Idle()
{
    ee_units_ph_body(EE_UNITS_PH_DO_IDLE, this, this, 0, 0, 0);   // phys_body.rs
}

// Runs one physics tick: game clock, civ and resource timers, weather, paths, area effects, units, effects,
// calamities, AI, scenario and game over; then the timing averages and, on a server, the autosave clock
// NEAR: GetLongValue(0x5e) out of line and signed in the original (fild dword); TMGetTime hoisted into ebx
void EEPhysics::Tick()
{
    ee_units_ph_body(EE_UNITS_PH_DO_TICK, this, this, 0, 0, 0);   // phys_body.rs
}

// Queues a load/save request under the save-game lock, taking a reference to it
void EEPhysics::QueueMessage(NEMessage* inMessage)
{
    ee_units_ph_body(EE_UNITS_PH_DO_QUEUE_MESSAGE, this, this, (uint64_t)(uintptr_t)inMessage, 0, 0);   // phys_body.rs
}

// Applies one unit swap: the camera follows the new unit (or drops the old one), and the contagions, the
// scenario and the effects switch to it
// NEAR: the original loads mUnitA into eax first and only the else branch keeps &gWorld.mCameraControl in ebx
void EEPhysics::SwapUnit(EEUnit* inOld, EEUnit* inNew)
{
    ee_units_ph_body(EE_UNITS_PH_DO_SWAP_UNIT, this, this, (uint64_t)(uintptr_t)inOld, (uint64_t)(uintptr_t)inNew, 0);   // phys_body.rs
}

// Applies the queued unit swaps: tells every player but player 0, swaps each pair, releases both units, clears
void EEPhysics::ProcessUnitSwaps()
{
    ee_units_ph_body(EE_UNITS_PH_DO_PROCESS_UNIT_SWAPS, this, this, 0, 0, 0);   // phys_body.rs
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
    std::string theName = inOp != 0 ? EECPNextTechName((long)inKey) : EECPNextClassName((long)inKey);
    size_t theLength = theName.size() < inCapacity ? theName.size() : inCapacity;
    memcpy(ioText, theName.data(), theLength);
    return theLength;
}

static void EEPhysicsLog(NEMessage* inMessage, unsigned long inTime, uint8_t inSelect)
{
    EEPlayer* thePlayer = EEGameLogOn() ? gWorld.GetPlayerBySlot(inMessage->GetOriginatingSlot()) : NULL;
    EEMessage* theRecord = thePlayer != NULL ? EEGameMessageCast(inMessage) : NULL;
    if (thePlayer != NULL)
        ee_units_order_log((uint64_t)(uintptr_t)thePlayer, (uint64_t)(uintptr_t)thePlayer->mActiveGroup, inMessage->GetType(),
                           theRecord != NULL ? static_cast<EEMRecord*>(theRecord)->mRecord : NULL, (uint32_t)inTime, inSelect);
}

// Runs the queued physics tick messages (one while the timer runs): checksums or dumps the world when asked,
// takes the owed ticks and speed, executes the tick's player inputs; a stray non-tick message at the head is dropped
// NEAR: inline budget: the original calls deque::begin() out of line for the first front(); esi/ebx/edi roles
void EEPhysics::DrainInput()
{
    ee_units_ph_body(EE_UNITS_PH_DO_DRAIN_INPUT, this, this, 0, 0, 0);   // phys_body.rs
}

// Every 2.5..5 s (a random period drawn once) sends the current player's two statistics to the others when
// they changed
// (Virtual20's; provisional names)
void EEPhysics::ReportStatistics()
{
    ee_units_ph_body(EE_UNITS_PH_DO_REPORT_STATISTICS, this, this, 0, 0, 0);   // phys_body.rs
}

// Writes the physics clocks, tick counters, timing averages, civ timer state, game-over flag and resource timer
// to the saved game; returns the first write error
long EEPhysics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEPhysicsSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion};   // port: ee_units physics_save.rs
    return ee_units_physics_save(&theContext, &EEPhysicsSaveOperation, static_cast<EeUnitsPhysicsSaved*>(this));
}

// Posts the autosave message (0x50a) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostAutoSave()
{
    return (long)ee_units_ph_body(EE_UNITS_PH_DO_POST, this, this, 0, 0, 0x50a);   // phys_body.rs
}

// Posts the show-scenario-dialog message (0x508) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostShowDialogMessage()
{
    return (long)ee_units_ph_body(EE_UNITS_PH_DO_POST, this, this, 0, 0, 0x508);   // phys_body.rs
}

// Posts the re-init message (0x509) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostInit()
{
    return (long)ee_units_ph_body(EE_UNITS_PH_DO_POST, this, this, 0, 0, 0x509);   // phys_body.rs
}

// Recomputes the ticks per game second and the tick length (s and ms) from the tick rate and the game speed
void EEPhysics::UpdateTiming()
{
    ee_units_ph_body(EE_UNITS_PH_DO_UPDATE_TIMING, this, this, 0, 0, 0);   // phys_body.rs
}

// Queues a unit swap (inOld replaced by inNew), taking a reference to both units
void EEPhysics::QueueUnitSwap(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    ee_units_ph_body(EE_UNITS_PH_DO_QUEUE_UNIT_SWAP, this, this, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inOld), (uint64_t)(uintptr_t)static_cast<EEUnit*>(inNew), 0);   // phys_body.rs
}

// Releases the world's write lock
void EEPhysics::Unlock()
{
    gWorld.mLock->WriteUnlock();
}

// Advances the area effects clock and ticks the area effects once per 2 s of game time
void EEPhysics::UpdateAreaEffectsClock()
{
    ee_units_ph_body(EE_UNITS_PH_DO_AREA_EFFECTS_CLOCK, this, this, 0, 0, 0);   // phys_body.rs
}

// Runs the civilization timer on a server: warnings at 240 s and 270 s, expiry at 300 s, and the civ builder
// button blinks for a player with no civilization; elsewhere (or with no civ buying) the timer stops
void EEPhysics::UpdateCivTimer()
{
    ee_units_ph_body(EE_UNITS_PH_DO_UPDATE_CIV_TIMER, this, this, 0, 0, 0);   // phys_body.rs
}

// Between ticks runs every queued load/save request (under the save-game lock) and sends one response with
// the last result (logged on a server); with ticks owed it waits
// NEAR: the original inlines the EEMLoadOrSaveGameResponse ctor (ee/EEMessage.h declares it out of line, 0xc1025)
// and so calls _Ptrit::operator+ out of line
void EEPhysics::ServeLoadSaveRequests()
{
    ee_units_ph_body(EE_UNITS_PH_DO_SERVE_LOAD_SAVE, this, this, 0, 0, 0);   // phys_body.rs
}

// Every 4 s of game time takes the current player out of shift mode when the Shift key (0xe1) is up
// (Virtual20's; provisional name)
void EEPhysics::CheckShiftMode()
{
    ee_units_ph_body(EE_UNITS_PH_DO_CHECK_SHIFT_MODE, this, this, 0, 0, 0);   // phys_body.rs
}

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

// (kWindDrift, 0x442370, the wind direction random walk step: phys_body.rs WIND_DRIFT)

// Advances the day/night clock (when it runs) and the wind direction's random walk, then ticks the environment
// EEEnvironmentManager::Tick is an out-of-line member defined at the end of this file (0x55439f)
void EEPhysics::UpdateWeather()
{
    ee_units_ph_body(EE_UNITS_PH_DO_UPDATE_WEATHER, this, this, 0, 0, 0);   // phys_body.rs
}

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

// ---- the C++ of EEPhysics' Rust bodies (ee_units phys_body.rs; EE_UNITS_PH_*): one statement of the original
// bodies each, in their order; inA, inB objects or values, inC, inD values, ioF floats. The plain members are the
// Rust record the class derives from: the bodies read and write them themselves.

template <class T> static T* PhObj(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }
static int64_t PhBits(float inValue) { uint32_t theBits; memcpy(&theBits, &inValue, 4); return (int64_t)theBits; }

// the bodies' scratch: Tick's running fix-ups, the JIT notice's text, the time of day, the dump's and the autosave's
// name, the dump file, EndGame's campaign progress, the wind's transformation and the Shift key
static std::vector<std::function<void()> > sPhFixups;
static UWideString sPhText;
static TMTime sPhTime;
static UWideString sPhName;
static FILE* sPhFile;
static std::unique_ptr<EECampaignProgress> sPhProgress;
static GETransformation sPhWind;
static ISEntity* sPhKey;

// the dump's closing lines (the counters) and the file closed
static void PhDumpClose(EEPhysics* inPhysics)
{
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Dumped on PT %ld *******\n", inPhysics->mNumTicksDone);
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Current PT %ld *******\n", inPhysics->mCurrentTick);
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Continent Manager Checksum: %ld *******\n", gContinentManagerChecksum);
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Overall Checksum: %d *******\n", 0);
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* this->mRandomNumberGenerator.GetSeed(): %ld *******\n", inPhysics->mRandom.GetSeed());
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Number of user-input messages processed: %ld *******\n",
            inPhysics->mInputMessagesProcessed);
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Number of Messages in Server Queue: %zu *******\n", gPhysics->mServerMessages.size());
    fprintf(sPhFile, "OUT OF SYNCH DUMP: ******* Number of CT Physics Ticks To Process: %ld *******\n", gPhysics->mTicksToProcess);
    fclose(sPhFile);
}

extern "C" int64_t ee_units_ph_host(int32_t inOp, void* inPhysics, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float (*ioF)[4])
{
    EEPhysics* thePhysics = static_cast<EEPhysics*>(inPhysics);
    NEMessage* theMessage = PhObj<NEMessage>(inA);
    NEMRecord* theRecord = static_cast<NEMRecord*>(theMessage);
    EEPlayer* thePlayer = PhObj<EEPlayer>(inA);
    EEUnit* theUnit = PhObj<EEUnit>(inA);
    EEUnit* theOther = PhObj<EEUnit>(inB);
    float* f = *ioF;
    switch (inOp) {
    // DrainInput's, ClearServerMessages'
    case EE_UNITS_PH_QUEUE_LOCK_ENTER: thePhysics->mServerMessageQueueLock->Enter(); return 0;
    case EE_UNITS_PH_QUEUE_LOCK_LEAVE: thePhysics->mServerMessageQueueLock->Leave(); return 0;
    case EE_UNITS_PH_SERVER_MESSAGES: return (int64_t)thePhysics->mServerMessages.size();
    case EE_UNITS_PH_FRONT: return (int64_t)(uintptr_t)thePhysics->mServerMessages.front();
    case EE_UNITS_PH_POP_FRONT: thePhysics->mServerMessages.pop_front(); return 0;
    case EE_UNITS_PH_DEREFERENCE: theMessage->Dereference(); return 0;
    case EE_UNITS_PH_TIMER_RUNNING: return thePhysics->mTimerRunning;
    case EE_UNITS_PH_MESSAGE_TYPE: return theMessage->GetType();
    case EE_UNITS_PH_RECORD_BOOL: return theRecord->Get<bool>((unsigned int)inC);
    case EE_UNITS_PH_RECORD_BYTE: return theRecord->Get<unsigned char>((unsigned int)inC);
    case EE_UNITS_PH_RECORD_U32: return theRecord->Get<uint32_t>((unsigned int)inC);
    case EE_UNITS_PH_IS_SERVER: return gCommunications->IsServer();
    case EE_UNITS_PH_IS_CLIENT: return gCommunications->IsClient();
    case EE_UNITS_PH_HAS_LEFT_GAME: return static_cast<EEClient*>(gCommunications)->mHasLeftGame;
    case EE_UNITS_PH_OUT_OF_SYNCH_CHECKSUM: return thePhysics->GetOutOfSynchChecksum(inC != 0, 1);
    case EE_UNITS_PH_CTS_PROCESSED: return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case EE_UNITS_PH_CHECKSUM_RESPONSE:
        return (int64_t)(uintptr_t)static_cast<NEMessage*>(new NEMRecord(kNetMessageWorldDatabaseChecksumResponse, {(unsigned long)inC, (unsigned long)inD}));
    case EE_UNITS_PH_SET_SLOT_CURRENT: theMessage->SetOriginatingSlot(gWorld.GetCurrentPlayer()->mSlot); return 0;
    case EE_UNITS_PH_LOCAL_INPUT: gCommunications->ProcessLocalUserInput(theMessage, kSlotNone); return 0;
    case EE_UNITS_PH_LOG_CHECKSUM_RESPONSE: static_cast<NESynchronousServer*>(gCommunications)->LogChecksumResponse(theRecord); return 0;
    case EE_UNITS_PH_INCREMENT_CTS: gCommunications->IncrementNumberOfCTsProcessedInPhysics(); return 0;
    case EE_UNITS_PH_SET_AVERAGE_UPDATE: gCommunications->SetAveragePhysicsUpdateTime((long)inC); return 0;
    case EE_UNITS_PH_SET_AVERAGE_BETWEEN: gCommunications->SetAverageTimeBetweenPhysicsTicks((long)inC); return 0;
    case EE_UNITS_PH_SET_SPEED: thePhysics->SetSpeed(f[0]); return 0;
    case EE_UNITS_PH_MESSAGE_LABEL: theMessage->GetMessageLabel(); return 0;
    case EE_UNITS_PH_FOR_NO_SLOT: return theMessage->GetForSpecificSlot() == kSlotNone;
    case EE_UNITS_PH_INPUTS_PROCESSED_INCREMENT: thePhysics->mInputMessagesProcessed++; return 0;
    case EE_UNITS_PH_CAPTURE_EXECUTED: EEMessageCaptureExecuted(theMessage); return 0;
    case EE_UNITS_PH_CAPTURE_ACTIVE: return EESimCaptureRecord::Active();
    case EE_UNITS_PH_UNIT_STORE_CHECK: EEUnitStoreCheck((long)inC); return 0;
    case EE_UNITS_PH_TEST_HOOK_SET: return gEEMessageExecutedTestHook != NULL;
    case EE_UNITS_PH_TEST_HOOK: gEEMessageExecutedTestHook(theMessage, (int)inC); return 0;
    case EE_UNITS_PH_REMOVE_PLAYER:   // the player, out of synch, exiting, booted
        gCommunications->ExecuteRemovePlayer(theRecord->Get<unsigned char>(0), theRecord->Get<bool>(3), theRecord->Get<bool>(1), theRecord->Get<bool>(2));
        return 0;
    case EE_UNITS_PH_EXECUTE_SELECTION: EEGameMessageCast(theMessage)->ExecuteSelectionMessage(); return 0;
    case EE_UNITS_PH_LOG: EEPhysicsLog(theMessage, thePhysics->mGameTime, (uint8_t)inC); return 0;
    case EE_UNITS_PH_GAME_MESSAGE: return (int64_t)(uintptr_t)EEGameMessageCast(theMessage);
    case EE_UNITS_PH_EXECUTE_GAME_MESSAGE: PhObj<EEMessage>(inA)->ExecuteMessage(); return 0;
    case EE_UNITS_PH_EXECUTE: theMessage->Execute(); return 0;
    // Tick's
    case EE_UNITS_PH_WORLD_VIEW_CHECK:
        ee_world_view_check(reinterpret_cast<uintptr_t>(&gWorld), gPhysics != NULL ? (uint32_t)gPhysics->mNumTicksDone : 0);
        return 0;
    case EE_UNITS_PH_LOADED_SEED_PENDING: return sLoadedSeedPending;
    case EE_UNITS_PH_CLEAR_LOADED_SEED_PENDING: sLoadedSeedPending = false; return 0;
    case EE_UNITS_PH_SET_LOADED_SEED: thePhysics->mRandom.SetSeed(sLoadedSeed); return 0;
    case EE_UNITS_PH_FIXUPS_EMPTY: return (inC != 0 ? sAfterLoadLast : sAfterLoad).empty();
    case EE_UNITS_PH_FIXUPS_TAKE: sPhFixups.swap(inC != 0 ? sAfterLoadLast : sAfterLoad); return 0;
    case EE_UNITS_PH_FIXUPS_COUNT: return (int64_t)sPhFixups.size();
    case EE_UNITS_PH_FIXUP_RUN: sPhFixups[(size_t)inC](); return 0;
    case EE_UNITS_PH_FIXUPS_CLEAR: sPhFixups.clear(); return 0;
    case EE_UNITS_PH_UNIT_ORDER_RESET: EEUnitOrderReset(); return 0;
    case EE_UNITS_PH_REPORT_STATISTICS: thePhysics->ReportStatistics(); return 0;
    case EE_UNITS_PH_DRAIN_INPUT: thePhysics->DrainInput(); return 0;
    case EE_UNITS_PH_SERVE_LOAD_SAVE: thePhysics->ServeLoadSaveRequests(); return 0;
    case EE_UNITS_PH_PAUSED: return gCommunications->GetIsGamePaused();
    case EE_UNITS_PH_NO_CAMERA_UNIT: return gWorld.mCameraControl.mUnitA == NULL;
    case EE_UNITS_PH_CAMERA_UPDATE: gWorld.mCameraControl.Update(thePhysics->mSpeed); return 0;
    case EE_UNITS_PH_RANDOM_GENERATE: return PhBits(thePhysics->mRandom.Generate());
    case EE_UNITS_PH_TIME: return (int64_t)TMGetTime();
    case EE_UNITS_PH_UPDATE_ACTIVE_UNITS: gWorld.UpdateActiveUnits(); return 0;
    case EE_UNITS_PH_UPDATE_CIV_TIMER: thePhysics->UpdateCivTimer(); return 0;
    case EE_UNITS_PH_RESOURCE_TIME: return gWorldTable.GetLongValue(kWorldDeathmatchJitManufacturingDelay) * 1000;
    case EE_UNITS_PH_RESOURCES: return gWorld.mGameOptions.mResources;
    case EE_UNITS_PH_HAS_JIT_POWER: return gCivPowersManager.HasPower(gWorld.GetCurrentPlayer(), kCivPowerJustInTimeManufacturing);
    case EE_UNITS_PH_JIT_TEXT: gResource->GetString(IDS_JUST_IN_TIME_MANUFANUFACTURING_IS_NOW, sPhText); return 0;
    case EE_UNITS_PH_CHAT_TEXT: gUserInterface->AddChatLogEntry(sPhText, 0); return 0;
    case EE_UNITS_PH_UPDATE_WEATHER: thePhysics->UpdateWeather(); return 0;
    case EE_UNITS_PH_PATH_UPDATE: gPathManager.Update(); return 0;
    case EE_UNITS_PH_AREA_EFFECTS_CLOCK: thePhysics->UpdateAreaEffectsClock(); return 0;
    case EE_UNITS_PH_CHECK_SHIFT_MODE: thePhysics->CheckShiftMode(); return 0;
    case EE_UNITS_PH_UPDATE_UNITS: thePhysics->UpdateUnits(); return 0;
    case EE_UNITS_PH_EFFECTS_UPDATE: gEffectManager->Update(); return 0;
    case EE_UNITS_PH_CALAMITIES_UPDATE: EECalamityManager::GetInstance()->Update(thePhysics->mTickTime); return 0;
    case EE_UNITS_PH_AI_UPDATE: gArtificialIntelligence->Update(); return 0;
    case EE_UNITS_PH_IN_CINEMATIC: return gWorld.mInCinematic;
    case EE_UNITS_PH_SCENARIO_RESULT: return gWorld.mScenarioManager->mGameResult;
    case EE_UNITS_PH_SCENARIO_UPDATE: gWorld.mScenarioManager->Update(); return 0;
    case EE_UNITS_PH_CHECK_GAME_OVER: return gWorld.CheckGameOver();
    case EE_UNITS_PH_SET_SCENARIO_RESULT: gWorld.mScenarioManager->mGameResult = (long)inC; return 0;
    case EE_UNITS_PH_END_GAME: thePhysics->EndGame(inC != 0); return 0;
    case EE_UNITS_PH_FLUSH_WORLD: thePhysics->FlushWorld(); return 0;
    case EE_UNITS_PH_CAMPAIGN_NAME_LENGTH: return gWorld.mGameOptions.mCampaignName.GetLength();
    case EE_UNITS_PH_CAMPAIGN_CHECKSUM: return gWorld.mGameOptions.mCampaignChecksum;
    case EE_UNITS_PH_SCENARIO_GAME: return gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_PH_AVAILABLE_POINTS: return static_cast<unsigned long>(gWorld.GetCurrentPlayer()->mCivilization.GetAvailablePoints());
    case EE_UNITS_PH_CIV_BLINK: EESetCivBuilderButtonBlinking(inC != 0); return 0;
    case EE_UNITS_PH_AUTOSAVE_PERIOD: return gGamePreferences.mSettings.mAutoSaveInMilliseconds;
    case EE_UNITS_PH_TIMER_INTERVAL: return thePhysics->mTimerInterval;
    case EE_UNITS_PH_SHOWING_DIALOG: return gWorld.mScenarioManager->mIsShowingDialog;
    case EE_UNITS_PH_POST_AUTOSAVE: gPhysics->PostAutoSave(); return 0;
    case EE_UNITS_PH_CURRENT_CT: gCommunications->GetCurrentCommunicationsTick(); return 0;
    // FlushWorld's, RunOwedTicks'
    case EE_UNITS_PH_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_UNITS_PH_PLAYER_AT: return (int64_t)(uintptr_t)gWorld.mPlayers[inC];
    case EE_UNITS_PH_CHECK_UNIT_LIST: RTSCheckUnitList(&gWorld.mUnits, "EEPhysics::FlushWorld (start)"); return 0;
    case EE_UNITS_PH_FLUSH_PLACEMENTS: gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_PH_ADD_PENDING_UNITS: thePlayer->AddPendingUnits(0, 1); return 0;
    case EE_UNITS_PH_QUEUE_BORN_DEACTIVATIONS: gWorld.QueueBornDeactivations(); return 0;
    case EE_UNITS_PH_PROCESS_UNIT_SWAPS: thePhysics->ProcessUnitSwaps(); return 0;
    case EE_UNITS_PH_REMOVE_KILLED_UNITS: thePlayer->RemoveKilledUnits(); return 0;
    case EE_UNITS_PH_TICK: thePhysics->Tick(); return 0;
    // EndGame's
    case EE_UNITS_PH_SET_GAME_ENDED: gB529065 = true; return 0;
    case EE_UNITS_PH_OBSERVER: return thePlayer->mObserverMode;
    case EE_UNITS_PH_REMOVE_FROM_GAME: gWorld.RemovePlayerFromGame(inC != 0, (unsigned long)inD); return 0;
    case EE_UNITS_PH_SET_GAME_RESULT: thePlayer->SetGameResult((long)inC); return 0;
    case EE_UNITS_PH_CURRENT_GAME_RESULT: return gWorld.GetCurrentPlayer()->mGameResult;
    case EE_UNITS_PH_PROGRESS_LOAD:
        return (sPhProgress.reset(new EECampaignProgress()), sPhProgress->Load(UWideString(gCampaignUser), gWorld.mGameOptions.mCampaignName,
                                                                             gWorld.mGameOptions.mCampaignChecksum, true));
    case EE_UNITS_PH_PROGRESS_SAVE:
        sPhProgress->SaveCompletedScenario(UWideString(gCampaignUser), gWorld.mGameOptions.mCampaignName, gWorld.mGameOptions.mScenarioName,
                                           gWorld.mGameOptions.mCampaignChecksum);
        return 0;
    case EE_UNITS_PH_MAIN_GAME_CURRENT: return dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL;
    case EE_UNITS_PH_OPTIONS_BLINK:
        (static_cast<UIBlinkingButton*>(gUIManager->GetMainGameForm()->GetControl(kMainGameIngameOptions)))->SetIsBlinking(true);
        return 0;
    case EE_UNITS_PH_TEAMS_CLEAR: gWorld.mTeams.clear(); return 0;
    case EE_UNITS_PH_APP_TEAMS_CLEAR: ee_app_teams_clear(); return 0;
    case EE_UNITS_PH_PAUSE_GAME: (static_cast<NESynchronousServer*>(gCommunications))->PauseGame(); return 0;
    case EE_UNITS_PH_PROGRESS_RELEASE: sPhProgress.reset(); return 0;
    // GetOutOfSynchChecksum's (and its dump's)
    case EE_UNITS_PH_TIME_OF_DAY: TMGetTimeOfDay(sPhTime); return 0;
    case EE_UNITS_PH_DUMP_NAME:
        sprintf(sPhName, "OOS %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds.log", sPhTime.mYear, sPhTime.mMonth + 1, sPhTime.mDay, sPhTime.mHour,
                sPhTime.mMinute, sPhTime.mSecond);
        return 0;
    case EE_UNITS_PH_DUMP_OPEN: sPhFile = PlatFOpen(sPhName, "w"); return 0;
    case EE_UNITS_PH_DUMP_VERIFY: ESVERIFY_AT(sPhFile != NULL, 544); return 0;
    case EE_UNITS_PH_DUMP_NEWLINE: fprintf(sPhFile, "\n"); return 0;
    case EE_UNITS_PH_DUMP_PLAYER:
        fprintf(sPhFile, "OUT OF SYNCH DUMP:  **************  Player Number: %ld  *************\n", thePlayer->mPlayerID);
        return 0;
    case EE_UNITS_PH_ACTIVE_GROUP: return (int64_t)(uintptr_t)thePlayer->mActiveGroup;
    case EE_UNITS_PH_GROUP_NUM_UNITS: return PhObj<EESelectedGroup>(inA)->GetNumUnits();
    case EE_UNITS_PH_DUMP_SELECTED_COUNT:
        fprintf(sPhFile, "OUT OF SYNCH DUMP:  **************  Selected Group Count: %ld  *************\n", (unsigned long)inC);
        return 0;
    case EE_UNITS_PH_GROUP_UNIT_ID: return PhObj<EESelectedGroup>(inA)->GetUnit((unsigned long)inC)->mUnitID;
    case EE_UNITS_PH_DUMP_UNIT:
        fprintf(sPhFile, "OUT OF SYNCH DUMP: Unit #%ld has an id of %ld(%08lx)\n", (unsigned long)inC, (unsigned long)inD, (unsigned long)inD);
        return 0;
    case EE_UNITS_PH_RUNNING_GROUPS: return thePlayer->GetNumRunningGroups();
    case EE_UNITS_PH_DUMP_ACTIVE_COUNT:
        fprintf(sPhFile, "OUT OF SYNCH DUMP:  **************  Active Group Count: %ld  *************\n", (unsigned long)inC);
        return 0;
    case EE_UNITS_PH_RUNNING_GROUP: return (int64_t)(uintptr_t)thePlayer->GetRunningGroup((unsigned long)inC);
    case EE_UNITS_PH_ACTIVE_GROUP_NUM_UNITS: return PhObj<EEGroup>(inA)->GetNumUnits();
    case EE_UNITS_PH_DUMP_ACTIVE_GROUP:
        fprintf(sPhFile, "OUT OF SYNCH DUMP:  Active Group #%ld has a unit count of %ld\n", (unsigned long)inC, (unsigned long)inD);
        return 0;
    case EE_UNITS_PH_ACTIVE_GROUP_UNIT_ID: return PhObj<EEGroup>(inA)->GetUnit((unsigned long)inC)->mUnitID;
    case EE_UNITS_PH_AIR_TRAFFIC: return (int64_t)(uintptr_t)thePlayer->mAirTraffic[inC];
    case EE_UNITS_PH_DUMP_AIR_TRAFFIC: ee_units_atc_dump(PhObj<EeUnitsAtc>(inA), sPhFile); return 0;
    case EE_UNITS_PH_DUMP_PATHS: gPathManager.DumpOutOfSynch(sPhFile); return 0;
    case EE_UNITS_PH_DUMP_MAP: gWorld.mMap->DumpOutOfSynch(sPhFile); return 0;
    case EE_UNITS_PH_DUMP_CALAMITIES: EECalamityManager::GetInstance()->DumpOutOfSynch(sPhFile); return 0;
    case EE_UNITS_PH_DUMP_CONTAGIONS: gWorld.GetContagionManager()->DumpOutOfSynch(sPhFile); return 0;
    case EE_UNITS_PH_FIRST_UNIT: return (int64_t)(uintptr_t)gWorld.mUnits.mFirst;
    case EE_UNITS_PH_NEXT_UNIT: return (int64_t)(uintptr_t)PhObj<RTSUnit>(inA)->mTileNode->mNext;
    case EE_UNITS_PH_DUMP_UNIT_STATE: static_cast<EEUnit*>(PhObj<RTSUnit>(inA))->OutOfSynchDump(sPhFile, (unsigned long)inC); return 0;
    case EE_UNITS_PH_DUMP_CLOSE: PhDumpClose(thePhysics); return 0;
    case EE_UNITS_PH_UNIT_CHECKSUM: return (unsigned long)static_cast<EEUnit*>(PhObj<RTSUnit>(inA))->GetChecksum((unsigned long)inC);
    case EE_UNITS_PH_SEED_AND_INPUTS: return (unsigned long)(thePhysics->mRandom.GetSeed() + thePhysics->mInputMessagesProcessed);
    case EE_UNITS_PH_CP_CHECKSUM: return (unsigned long)EECPOutOfSynchChecksum();
    case EE_UNITS_PH_MAP_CHECKSUM: return (unsigned long)gWorld.mMap->ComputeChecksum();
    case EE_UNITS_PH_CONTINENT_CHECKSUM: return (unsigned long)gContinentManagerChecksum;
    case EE_UNITS_PH_CALAMITY_CHECKSUM: return (unsigned long)EECalamityManager::GetInstance()->GetOutOfSynchChecksum();
    case EE_UNITS_PH_SET_LAST_CHECKSUM: thePhysics->mLastOutOfSynchChecksum = (unsigned long)inC; return 0;
    case EE_UNITS_PH_PLAYER_ID: return thePlayer->mPlayerID;
    // GetTimeString's (inC the hours, inD the minutes and seconds, inB the three-digit hours)
    case EE_UNITS_PH_TIME_TEXT:
        sprintf(*PhObj<UWideString>(inA), inB != 0 ? "%03.2f %03ld:%02ld:%02ld" : "%03.2f %02ld:%02ld:%02ld", gWorld.mGameSpeed,
                (unsigned long)inC, (unsigned long)((uint64_t)inD & 0xffffffffu), (unsigned long)((uint64_t)inD >> 32));
        return 0;
    case EE_UNITS_PH_GAME_CLOCK: return PhBits(gPhysics->mGameClock);
    // GetUnitSwapKind's, ProcessUnitSwaps', SwapUnit's, QueueUnitSwap's
    case EE_UNITS_PH_SWAPS: return (int64_t)thePhysics->mUnitSwaps.size();
    case EE_UNITS_PH_SWAP_OLD: return (int64_t)(uintptr_t)static_cast<EEUnit*>(thePhysics->mUnitSwaps[(size_t)inC].mOld);
    case EE_UNITS_PH_SWAP_NEW: return (int64_t)(uintptr_t)static_cast<EEUnit*>(thePhysics->mUnitSwaps[(size_t)inC].mNew);
    case EE_UNITS_PH_UNIT_PLAYER: return (int64_t)(uintptr_t)theUnit->GetPlayer();
    case EE_UNITS_PH_IS_CITIZEN: return static_cast<EEComplexUnit*>(theUnit)->mIsCitizen;
    case EE_UNITS_PH_UNITS_SWAPPED: thePlayer->UnitsSwapped(thePhysics->mUnitSwaps); return 0;
    case EE_UNITS_PH_SWAP_UNIT: thePhysics->SwapUnit(theUnit, theOther); return 0;
    case EE_UNITS_PH_UNIT_DEREFERENCE: theUnit->Dereference(); return 0;
    case EE_UNITS_PH_SWAPS_CLEAR: thePhysics->mUnitSwaps.clear(); return 0;
    case EE_UNITS_PH_CAMERA_UNIT_A: return (int64_t)(uintptr_t)static_cast<EEUnit*>(gWorld.mCameraControl.mUnitA);
    case EE_UNITS_PH_CAMERA_UNIT_B: return (int64_t)(uintptr_t)static_cast<EEUnit*>(gWorld.mCameraControl.mUnitB);
    case EE_UNITS_PH_CAMERA_FOLLOW:
        gWorld.mCameraControl.Follow(theUnit, gWorld.mCameraControl.mIsFollowingUnitB, 1.0f, gWorld.mCameraControl.mTurnYaw,
                                     gWorld.mCameraControl.mTurnPitch);
        return 0;
    case EE_UNITS_PH_CAMERA_CLEAR_A: gWorld.mCameraControl.ClearUnitA(); return 0;
    case EE_UNITS_PH_CAMERA_CLEAR_B: gWorld.mCameraControl.ClearUnitB(); return 0;
    case EE_UNITS_PH_CONTAGION_SWAP: gWorld.GetContagionManager()->SwapUnit(theUnit, theOther); return 0;
    case EE_UNITS_PH_SCENARIO_REPLACE: gWorld.mScenarioManager->ReplaceUnit(theUnit->mUnitID, theOther->mUnitID); return 0;
    case EE_UNITS_PH_EFFECTS_REPLACE: gEffectManager->ReplaceUnit(theUnit, theOther); return 0;
    case EE_UNITS_PH_UNIT_REFERENCE: theUnit->Reference(); return 0;
    case EE_UNITS_PH_SWAPS_PUSH: thePhysics->mUnitSwaps.push_back({static_cast<EEComplexUnit*>(theUnit), static_cast<EEComplexUnit*>(theOther)}); return 0;
    // Init's, ServeLoadSaveRequests', QueueMessage's
    case EE_UNITS_PH_CLEAR_SERVER_MESSAGES: thePhysics->ClearServerMessages(); return 0;
    case EE_UNITS_PH_SAVE_LOCK_ENTER: thePhysics->mSaveGameLock->Enter(); return 0;
    case EE_UNITS_PH_SAVE_LOCK_LEAVE: thePhysics->mSaveGameLock->Leave(); return 0;
    case EE_UNITS_PH_REQUESTS: return (int64_t)thePhysics->mLoadSaveRequests.size();
    case EE_UNITS_PH_REQUEST_DEREFERENCE: thePhysics->mLoadSaveRequests[(size_t)inC]->Dereference(); return 0;
    case EE_UNITS_PH_REQUESTS_CLEAR: thePhysics->mLoadSaveRequests.clear(); return 0;
    case EE_UNITS_PH_INPUTS_PROCESSED_CLEAR: thePhysics->mInputMessagesProcessed = 0; return 0;
    case EE_UNITS_PH_SPEED: return PhBits(thePhysics->mSpeed);
    case EE_UNITS_PH_LOAD_OR_SAVE: return EELoadOrSave(static_cast<EEMRecord*>(thePhysics->mLoadSaveRequests[(size_t)inC]));
    case EE_UNITS_PH_REQUEST_CLEAR: thePhysics->mLoadSaveRequests[(size_t)inC] = NULL; return 0;
    case EE_UNITS_PH_LOAD_SAVE_RESPONSE:
        return (int64_t)(uintptr_t)static_cast<NEMessage*>(new EEMRecord(kMessageLoadOrSaveGameResponse, {(long)inC}));
    case EE_UNITS_PH_SET_SLOT_CURRENT_GET: theMessage->SetOriginatingSlot(gWorld.GetCurrentPlayer()->GetSlot()); return 0;
    case EE_UNITS_PH_LOG_RESPONSE: static_cast<NESynchronousServer*>(gCommunications)->LogResponse(static_cast<EEMRecord*>(theMessage)); return 0;
    case EE_UNITS_PH_LOCAL_INPUT_SERVER: gCommunications->ProcessLocalUserInput(theMessage, kSlotServer); return 0;
    case EE_UNITS_PH_REQUESTS_PUSH: thePhysics->mLoadSaveRequests.push_back(theMessage); return 0;
    case EE_UNITS_PH_REFERENCE: theMessage->Reference(); return 0;
    // UpdateCivTimer's
    case EE_UNITS_PH_HAS_COMMUNICATIONS: return gCommunications != NULL;
    case EE_UNITS_PH_CAN_BUY_CIVILIZATION: return gWorld.mGameOptions.CanBuyCivilization();
    case EE_UNITS_PH_CIVILIZATION_CHOSEN: return gWorld.GetCurrentPlayer()->mCivilizationChosen;
    case EE_UNITS_PH_CURRENT_OBSERVER: return gWorld.GetCurrentPlayer()->mObserverMode;
    case EE_UNITS_PH_CIV_TIMER_LIMIT: return EEGetCivTimerLimit();
    case EE_UNITS_PH_CIV_TIMER_SECOND: return EEGetCivTimerSecondWarning();
    case EE_UNITS_PH_CIV_TIMER_FIRST: return EEGetCivTimerFirstWarning();
    case EE_UNITS_PH_CIV_TIMER_MESSAGE:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageCivTimerExpired, {(unsigned char)inC}), kSlotNone);
        return 0;
    // CheckShiftMode's, UpdateWeather's
    case EE_UNITS_PH_SHIFT_MODE: return gWorld.GetCurrentPlayer()->mShiftMode;
    case EE_UNITS_PH_SHIFT_UP: return gISKeyboard != NULL && gISKeyboard->GetEntity(kKeyShift, sPhKey) == kErrorOK && sPhKey->GetValue() == 0;
    case EE_UNITS_PH_EXIT_SHIFT_MODE:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageExitShiftMode, {thePhysics->mGameTime}), kSlotNone);
        return 0;
    case EE_UNITS_PH_GET_TIME_OF_DAY: return PhBits(gWorld.GetTimeOfDay());
    case EE_UNITS_PH_SET_TIME_OF_DAY: gWorld.SetTimeOfDay(f[0]); return 0;
    case EE_UNITS_PH_WIND_YAW: return PhBits(gWorld.mWindYaw);
    case EE_UNITS_PH_SET_WIND_YAW: gWorld.mWindYaw = f[0]; return 0;
    case EE_UNITS_PH_WIND_ORIENT: (sPhWind = GETransformation()).SetOrientationYPR(f[0], 0.0f, 0.0f); return 0;
    case EE_UNITS_PH_WIND_DIRECTION: sPhWind.Apply(GEVector(0.0f, 0.001f, 0.0f), gWorld.mWindDirection); return 0;
    case EE_UNITS_PH_ENVIRONMENT_TICK: gWorld.mEnvironmentManager->Tick(thePhysics->mGameTime); return 0;
    // ReportStatistics', AutoSave's
    case EE_UNITS_PH_LAST_STATISTIC: return (unsigned long)gWorld.GetCurrentPlayer()->mSavedGroups.GetLastValue((long)inC);
    case EE_UNITS_PH_REPORT_MESSAGE:
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReportStatistics, {(unsigned long)inC, (unsigned long)inD}), kSlotNone);
        return 0;
    case EE_UNITS_PH_SEED_RAND: srand(TMGetTime()); return 0;
    case EE_UNITS_PH_RAND: return rand();
    case EE_UNITS_PH_AUTOSAVES: return gGamePreferences.mSettings.mNumberOfAutoSaves;
    case EE_UNITS_PH_AUTOSAVE_NAME:
        sprintf(sPhName, "%s %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds%s", "AutoSave", sPhTime.mYear, sPhTime.mMonth + 1, sPhTime.mDay,
                sPhTime.mHour, sPhTime.mMinute, sPhTime.mSecond, (const char*)gSavedGameExtension);
        return 0;
    case EE_UNITS_PH_SET_LAST_AUTOSAVE: gGamePreferences.mSettings.mLastAutoSave = sPhName; return 0;
    case EE_UNITS_PH_PREFERENCES_SAVE: gGamePreferences.Save(); return 0;
    case EE_UNITS_PH_SEND_SAVE_REQUEST:
        static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(
            new EEMRecord(kMessageLoadOrSaveGame, {0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 1, 0, sPhName}), 15000, 12);
        return 0;
    // HandleMessage's
    case EE_UNITS_PH_RTS_HANDLE_MESSAGE: thePhysics->RTSPhysics::HandleMessage((unsigned long)inC, (void*)(uintptr_t)inA, (void*)(uintptr_t)inB); return 0;
    case EE_UNITS_PH_WORLD_LOCK: gWorld.mLock->WriteLock(); return 0;
    case EE_UNITS_PH_WORLD_UNLOCK: gWorld.mLock->WriteUnlock(); return 0;
    case EE_UNITS_PH_SCENARIO_DIALOGS: return gWorld.mScenarioManager && !gScenarioSimDialogs;
    case EE_UNITS_PH_SHOW_DIALOG: gWorld.mScenarioManager->ShowDialogMessage(); return 0;
    case EE_UNITS_PH_INIT: thePhysics->Init(); return 0;
    case EE_UNITS_PH_AUTOSAVE: thePhysics->AutoSave(); return 0;
    // UpdateAreaEffectsClock's, UpdateTiming's, Idle's, the posts', GetMinutesAndSeconds'
    case EE_UNITS_PH_AREA_EFFECTS_TICK: gAEManager.Tick(kAreaEffectInterval); return 0;
    case EE_UNITS_PH_TICKS_PER_SECOND: return thePhysics->mTicksPerSecond;
    case EE_UNITS_PH_GAME_SPEED: return PhBits(gWorld.GetGameSpeed());
    case EE_UNITS_PH_FREE_CAMERA: return gFreeCamera == true;
    case EE_UNITS_PH_LOCK: thePhysics->Lock(); return 0;
    case EE_UNITS_PH_UNLOCK: thePhysics->Unlock(); return 0;
    case EE_UNITS_PH_HAS_THREAD: return thePhysics->mThread != NULL;
    case EE_UNITS_PH_POST: return thePhysics->mThread->Post((unsigned long)inC, NULL, NULL, true);
    case EE_UNITS_PH_SET_ULONG: *PhObj<unsigned long>(inA) = (unsigned long)inC; return 0;
    }
    return 0;
}
