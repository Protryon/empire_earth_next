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
#include "lle/UIIMECandidate.h"
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
#include "rust/ee_units.h"   // ee_units_store_checksum_flag1
#include <functional>
#include "ee/EEGameLog.h"
#include "ee/EECPNext.h"
// chain at 0x551c28 (wrapper)
//   object 0x931ce0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gPhysicsDataKey("EEPhysics Data");

// the unit's other data (provisional names): the area effects' update period (0x4422f8, a unit-local .rdata float)
// (the physics totals 0x531cf0 / 0x531cf4 are EEPhysics::mInputMessagesProcessed / mLastOutOfSynchChecksum)
static const float kAreaEffectInterval = 2000.0f;
static unsigned long sReportedFirst;
static unsigned long sReportedSecond;
static unsigned long sReportPeriod;
static bool sFirstReport = true;            // 0x4c1e34 (.data)
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
    UWideString theName;
    if (gGamePreferences.mSettings.mNumberOfAutoSaves > 0) {
        TMTime theTime;
        TMGetTimeOfDay(theTime);
        sprintf(theName, "%s %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds%s", "AutoSave", theTime.mYear, theTime.mMonth + 1,
                theTime.mDay, theTime.mHour, theTime.mMinute, theTime.mSecond, (const char*)gSavedGameExtension);
        gGamePreferences.mSettings.mLastAutoSave = theName;
        gGamePreferences.Save();
        static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(
            new EEMRecord(kMessageLoadOrSaveGame, {0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), TMGetTime(), 1, 0, theName}),
            15000, 12);
    }
}

// Drops every queued server message (under the server queue lock)
void EEPhysics::ClearServerMessages()
{
    this->mServerMessageQueueLock->Enter();
    while (this->mServerMessages.size() > 0) {
        NEMessage* theMessage = this->mServerMessages.front();
        this->mServerMessages.pop_front();
        theMessage->Dereference();
    }
    this->mServerMessageQueueLock->Leave();
}

// Places the queued units, adds every player's pending units, applies the queued unit swaps, then removes
// every player's killed units
void EEPhysics::FlushWorld()
{
    unsigned long theNumPlayers = gWorld.mNumPlayers;
    RTSCheckUnitList(&gWorld.mUnits, "EEPhysics::FlushWorld (start)");   // EE_CHECK_UNIT_LIST (ee/RTSDummyHeader.h)
    gWorld.FlushQueuedPlacements();
    unsigned long i;
    for (i = 0; i < theNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL)
            gWorld.mPlayers[i]->AddPendingUnits(0, 1);
    }
    gWorld.QueueBornDeactivations();   // port: the units given an id just now (EEWorld::DeactivateUnit)
    this->ProcessUnitSwaps();
    for (i = 0; i < theNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL)
            gWorld.mPlayers[i]->RemoveKilledUnits();
    }
}

// Runs every owed physics tick, draining the input queue for more whenever none are left
void EEPhysics::RunOwedTicks()
{
    if (this->mTicksToProcess == 0)
        this->DrainInput();
    while (this->mTicksToProcess > 0) {
        this->Tick();
        if (this->mTicksToProcess == 0)
            this->DrainInput();
    }
}

// Ends the game: every non-observer player but player 0 wins or loses, a won campaign scenario is recorded,
// the in-game options button blinks, the teams are cleared and a server pauses the game
void EEPhysics::EndGame(bool inIsVictory)
{
    EECampaignProgress theProgress;
    EEGameOptionsPlayer thePlayer;
    long theResult;
    unsigned long i;
    gB529065 = true;
    for (i = 1; i < gWorld.mNumPlayers; i++) {
        if (gWorld.mPlayers[i]->mObserverMode == false) {
            gWorld.RemovePlayerFromGame(inIsVictory, i);
            gWorld.mPlayers[i]->SetGameResult(inIsVictory == true ? 1 : 2);
        }
    }
    if (gWorld.mGameOptions.mCampaignName.GetLength() > 0 && gWorld.mGameOptions.mCampaignChecksum > 0
        && gWorld.GetCurrentPlayer()->mGameResult == 1) {
        theResult = theProgress.Load(UWideString(gCampaignUser), gWorld.mGameOptions.mCampaignName,
                                     gWorld.mGameOptions.mCampaignChecksum, true);
        if (theResult == kErrorOK)
            theProgress.SaveCompletedScenario(UWideString(gCampaignUser), gWorld.mGameOptions.mCampaignName,
                                 gWorld.mGameOptions.mScenarioName, gWorld.mGameOptions.mCampaignChecksum);
    }
    if (gWorld.GetCurrentPlayer()->mGameResult != 0) {
        EESetCivBuilderButtonBlinking(false);
        if (dynamic_cast<EEUIFMainGame*>(gUIManager->GetCurrentForm()) != NULL)
            (static_cast<UIBlinkingButton*>(gUIManager->mMainGameForm->GetControl(kMainGameIngameOptions)))->SetIsBlinking(true);
    }
    gWorld.mTeams.clear();
    if (gCommunications->IsServer() == true)
        (static_cast<NESynchronousServer*>(gCommunications))->PauseGame();
    this->mGameOver = true;
}

// Returns the world's out-of-synch checksum (inFlags picks the parts: map, calamities, air traffic, continents,
// selections); inDump also writes everything to "OOS <date>.log"
// NEAR: frame 8 bytes smaller (two temporaries of the out-of-line begin()/operator+); register choice
unsigned long EEPhysics::GetOutOfSynchChecksum(bool inDump, unsigned long inFlags)
{
    UWideString theName;
    unsigned long j;
    unsigned long i;
    unsigned long m;
    FILE* theFile;
    EEPlayer* thePlayer;
    EESelectedGroup* theGroup;
    EEGroup* theActiveGroup;
    RTSUnit* theUnit;
    unsigned long theChecksum = 0;
    TMGetTime();
    if (inDump == true) {
        TMTime theTime;
        TMGetTimeOfDay(theTime);
        sprintf(theName, "OOS %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds.log", theTime.mYear, theTime.mMonth + 1, theTime.mDay,
                theTime.mHour, theTime.mMinute, theTime.mSecond);
        theFile = PlatFOpen(theName, "w");
        ESVERIFY_AT(theFile != NULL, 544);
        for (i = 0; i < gWorld.mNumPlayers; i++) {
            thePlayer = gWorld.mPlayers[i];
            fprintf(theFile, "\n");
            fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Player Number: %ld  *************\n",
                    thePlayer->mPlayerID);
            theGroup = thePlayer->mActiveGroup;
            if (theGroup != NULL) {
                fprintf(theFile, "\n");
                fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Selected Group Count: %ld  *************\n",
                        theGroup->GetNumUnits());
                for (j = 0; j < theGroup->GetNumUnits(); j++) {
                    unsigned long theID = theGroup->GetUnit(j)->mUnitID;
                    fprintf(theFile, "OUT OF SYNCH DUMP: Unit #%ld has an id of %ld(%08lx)\n", j, theID, theID);
                }
            }
            fprintf(theFile, "\n");
            fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Active Group Count: %ld  *************\n",
                    thePlayer->GetNumRunningGroups());
            for (j = 0; j < thePlayer->GetNumRunningGroups(); j++) {
                theActiveGroup = thePlayer->mRunningGroups[j];
                fprintf(theFile, "\n");
                fprintf(theFile, "OUT OF SYNCH DUMP:  Active Group #%ld has a unit count of %ld\n", j,
                        theActiveGroup->GetNumUnits());
                for (m = 0; m < theActiveGroup->GetNumUnits(); m++) {
                    unsigned long theID = theActiveGroup->GetUnit(m)->mUnitID;
                    fprintf(theFile, "OUT OF SYNCH DUMP: Unit #%ld has an id of %ld(%08lx)\n", m, theID, theID);
                }
            }
            thePlayer->mAirTraffic[0]->Dump(theFile);
            thePlayer->mAirTraffic[1]->Dump(theFile);
            thePlayer->mAirTraffic[2]->Dump(theFile);
            thePlayer->mAirTraffic[3]->Dump(theFile);
        }
        gPathManager.DumpOutOfSynch(theFile);
        if (inFlags & 4)
            gWorld.mMap->DumpOutOfSynch(theFile);
        if (inFlags & 0x10) {
            EECalamityManager::GetInstance()->DumpOutOfSynch(theFile);
            gWorld.GetContagionManager()->DumpOutOfSynch(theFile);
        }
        for (theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext)
            (static_cast<EEUnit*>(theUnit))->OutOfSynchDump(theFile, inFlags);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Dumped on PT %ld *******\n", this->mNumTicksDone);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Current PT %ld *******\n", this->mCurrentTick);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Continent Manager Checksum: %ld *******\n", gContinentManagerChecksum);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Overall Checksum: %d *******\n", 0);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* this->mRandomNumberGenerator.GetSeed(): %ld *******\n",
                this->mRandom.GetSeed());
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of user-input messages processed: %ld *******\n",
                this->mInputMessagesProcessed);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of Messages in Server Queue: %zu *******\n",
                gPhysics->mServerMessages.size());
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of CT Physics Ticks To Process: %ld *******\n",
                gPhysics->mTicksToProcess);
        fclose(theFile);
    }
    // New (R5b.4): the units' flags 1 (hit points + tile y + tile x of every complex unit on the map) and 2 (the times
    // updated, the active group, the command's type, the location and the height) are the unit store's sums over its
    // records on the map (ee_units store.rs checksum_flag1, checksum_flag2; USCK checks both against this walk's
    // GetChecksum); the walk keeps the other unit flags (0x20, 8). The sums wrap and commute, so the total is the
    // original's. The game's own out-of-sync check (flag 1 alone) walks no unit.
    if (inFlags & 1)
        theChecksum += (unsigned long)ee_units_store_checksum_flag1();
    if (inFlags & 2)
        theChecksum += (unsigned long)ee_units_store_checksum_flag2();
    const unsigned long theUnitFlags = inFlags & ~3ul;
    if (theUnitFlags & (0x20 | 8)) {
        for (theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext)
            theChecksum += static_cast<EEUnit*>(theUnit)->GetChecksum(theUnitFlags);
    }
    for (i = 0; i < gWorld.mNumPlayers; i++) {
        thePlayer = gWorld.mPlayers[i];
        if (inFlags & 0x200) {
            theGroup = thePlayer->mActiveGroup;
            if (theGroup != NULL) {
                for (j = 0; j < theGroup->GetNumUnits(); j++)
                    theChecksum += theGroup->GetUnit(j)->mUnitID * thePlayer->mPlayerID;
            }
        }
        if (!(inFlags & 0x40))
            continue;
        theChecksum += thePlayer->mAirTraffic[0]->GetChecksum();
        theChecksum += thePlayer->mAirTraffic[1]->GetChecksum();
        theChecksum += thePlayer->mAirTraffic[2]->GetChecksum();
        theChecksum += thePlayer->mAirTraffic[3]->GetChecksum();
    }
    theChecksum += this->mCurrentTick + this->mRandom.GetSeed() + this->mInputMessagesProcessed;
    theChecksum += EECPOutOfSynchChecksum();   // new: the players' resources and the AI's state (ee/EECPTrace.h)
    if (inFlags & 4)
        theChecksum += gWorld.mMap->ComputeChecksum();
    if (inFlags & 0x100)
        theChecksum += gContinentManagerChecksum;
    if (inFlags & 0x10)
        theChecksum += EECalamityManager::GetInstance()->GetOutOfSynchChecksum();
    TMGetTime();
    this->mLastOutOfSynchChecksum = theChecksum;
    return theChecksum;
}

// Returns the game speed and elapsed game time as text ("<speed> hh:mm:ss"); the time-of-day conversions are
// computed and unused
// NEAR: the time of day is also copied to a frame float (fld [ebp-4]) in the original
UWideString EEPhysics::GetTimeString()
{
    UWideString theString;
    long theDayHours = UMFloatToLong(gWorld.GetTimeOfDay() * 24.0f);
    float theTimeOfDay = gWorld.GetTimeOfDay();
    unsigned long theHourOfDay = UMFloatToLong(gWorld.GetTimeOfDay() * 24.0f);
    long theMinuteOfDay = UMFloatToLong(theTimeOfDay * 24.0f - theHourOfDay * 60.0f);
    float theTime = gPhysics->mGameClock;
    unsigned long theHours;
    theHours = theTime >= 3600000.0f ? UMFloorUnsigned(theTime * 2.7777778e-07f) : 0;
    unsigned long theMinutes = 0;
    if (theTime >= 60000.0f)
        theMinutes = ((unsigned long)theTime - theHours * 3600000) / 60000;
    unsigned long theSeconds = ((unsigned long)theTime - theHours * 3600000 - theMinutes * 60000) / 1000;
    if (theHours < 100)
        sprintf(theString, "%03.2f %02ld:%02ld:%02ld", gWorld.mGameSpeed, theHours, theMinutes, theSeconds);
    else
        sprintf(theString, "%03.2f %03ld:%02ld:%02ld", gWorld.mGameSpeed, theHours, theMinutes, theSeconds);
    return theString;
}

// Splits a game time in ms (0: the current one) into whole minutes and the seconds past them
void EEPhysics::GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime)
{
    if (inTime == 0)
        inTime = this->mGameTime;
    *outMinutes = inTime / 1000 / 60;
    *outSeconds = (inTime - *outMinutes * 60000) / 1000;
}

// Returns the unit's role in the queued unit swaps (the earliest wins): old 1, new 3; other owner 2 / 4;
// citizen to non-citizen 5 / 7, non-citizen to citizen 6 / 8; 0 when it is not swapped
// NEAR: inline budget: the original calls begin() out of line from the second mSound2Index7[i] on; mIsCitizen through a cast
// (EEPUnitSwap holds EEUnit*)
long EEPhysics::GetUnitSwapKind(EEComplexUnit* inUnit)
{
    long theResult = 0;
    for (long i = this->mUnitSwaps.size() - 1; i >= 0; i--) {
        if (this->mUnitSwaps[i].mOld == inUnit) {
            EEPlayer* theOldPlayer = this->mUnitSwaps[i].mOld->GetPlayer();
            if (theOldPlayer == this->mUnitSwaps[i].mNew->GetPlayer()) {
                theResult = 1;
                if (static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mOld)->mIsCitizen) {
                    if (!static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mNew)->mIsCitizen)
                        theResult = 5;
                } else {
                    if (static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mNew)->mIsCitizen)
                        theResult = 6;
                }
            } else
                theResult = 2;
        } else if (this->mUnitSwaps[i].mNew == inUnit) {
            EEPlayer* theOldPlayer = this->mUnitSwaps[i].mOld->GetPlayer();
            if (theOldPlayer == this->mUnitSwaps[i].mNew->GetPlayer()) {
                theResult = 3;
                if (static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mOld)->mIsCitizen) {
                    if (!static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mNew)->mIsCitizen)
                        theResult = 7;
                } else {
                    if (static_cast<EEComplexUnit*>(this->mUnitSwaps[i].mNew)->mIsCitizen)
                        theResult = 8;
                }
            } else
                theResult = 4;
        }
    }
    return theResult;
}

// Resets the simulation's clocks, tick counters and game flags and drops the queued server messages and
// load/save requests
void EEPhysics::Init()
{
    this->mCurrentTick = 0;
    this->mNumTicksDone = 0;
    this->mF84 = 0.0f;
    this->mTicksPerGameSecond = 0.0f;
    this->mGameTime = 0;
    this->mTickSeconds = 0.0f;
    this->mTickRandom = 0;
    this->mTimeOfDayStep = 0.0f;
    this->mTickTime = 0.0f;
    this->mGameClock = 0.0f;
    this->mTickStartTime = TMGetTime();
    this->mPreviousTickStartTime = TMGetTime();
    this->mAreaEffectsClock = 0.0f;
    this->mTickTimeMs = 0;
    this->mShiftModeClock = 0.0f;
    this->mTicksToProcess = 0;
    this->mTickGapSum = 0;
    this->mTickGapCount = 0;
    this->mTickWorkSum = 0;
    this->mTickWorkCount = 0;
    this->mAveragePhysicsUpdateTime = 0;
    this->mAverageTimeBetweenPhysicsTicks = 0;
    this->mLD4 = 0;
    this->mTimeOfDayRuns = false;
    this->mGameStarted = false;
    this->mBE2 = false;
    this->mCivTimerRuns = true;
    this->mCivTimerFirstWarning = false;
    this->mCivTimerSecondWarning = false;
    this->mGameOver = false;
    this->mBE7 = false;
    this->mResourceTimerPending = true;
    this->ClearServerMessages();
    this->mSaveGameLock->Enter();
    for (unsigned long i = 0; i < this->mLoadSaveRequests.size(); i++)
        this->mLoadSaveRequests[i]->Dereference();
    this->mLoadSaveRequests.clear();
    this->mSaveGameLock->Leave();
    this->mInputMessagesProcessed = 0;
    this->SetSpeed(this->mSpeed);
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
    switch (inMessage) {
    case 0x404:
        this->mPreviousTickStartTime = this->mTickStartTime = TMGetTime();
        RTSPhysics::HandleMessage(0x404, inParameter1, inParameter2);
        break;
    case 0x502:
        this->mTimeOfDayRuns = !this->mTimeOfDayRuns;
        break;
    case 0x503:
        this->mTimeOfDayRuns = false;
        gWorld.mLock->WriteLock();
        gWorld.SetTimeOfDay(0.35f);
        gWorld.mLock->WriteUnlock();
        break;
    case 0x508:
        if (gWorld.mScenarioManager && !gScenarioSimDialogs)
            gWorld.mScenarioManager->ShowDialogMessage();
        break;
    case 0x509:
        this->Init();
        break;
    case 0x50a:
        this->AutoSave();
        break;
    case 0x50b:
        this->mLoadSaveRequests.push_back(static_cast<NEMessage*>(inParameter1));
        break;
    case 0x50c:
        this->mLD4 = (long)reinterpret_cast<intptr_t>(inParameter1);
        this->mBE7 = true;
        break;
    default:
        RTSPhysics::HandleMessage(inMessage, inParameter1, inParameter2);
        break;
    }
}

// With the free camera on, drains the input queue and flushes the world under the world lock
void EEPhysics::Idle()
{
    if (gFreeCamera != true)
        return;
    this->Lock();
    this->DrainInput();
    this->FlushWorld();
    this->Unlock();
}

// Runs one physics tick: game clock, civ and resource timers, weather, paths, area effects, units, effects,
// calamities, AI, scenario and game over; then the timing averages and, on a server, the autosave clock
// NEAR: GetLongValue(0x5e) out of line and signed in the original (fild dword); TMGetTime hoisted into ebx
void EEPhysics::Tick()
{
    if (EESimCaptureRecord::Active())   // TEST ONLY: the unit store equals the objects (ee/EEUnitStore.h)
        EEUnitStoreCheck(0);
    if (sLoadedSeedPending) {   // port: a loaded game goes on from the saved seed (sLoadedSeed)
        sLoadedSeedPending = false;
        this->mRandom.SetSeed(sLoadedSeed);
    }
    if (!sAfterLoad.empty()) {   // port: the loaded values the load's rebuilding set again (EEAfterLoad)
        std::vector<std::function<void()> > theFixups;
        theFixups.swap(sAfterLoad);
        for (size_t i = 0; i < theFixups.size(); i++)
            theFixups[i]();
        EEUnitOrderReset();   // New (R5b.4): the stepped units as a fixup ordered them (ee/EEUnitStore.h)
    }
    if (!sAfterLoadLast.empty()) {   // port: then those that wait for the others (EEAfterLoadLast)
        std::vector<std::function<void()> > theFixups;
        theFixups.swap(sAfterLoadLast);
        for (size_t i = 0; i < theFixups.size(); i++)
            theFixups[i]();
    }
    this->ReportStatistics();
    if (this->mTicksToProcess == 0) {
        this->DrainInput();
        this->ServeLoadSaveRequests();
    }
    if (this->mTicksToProcess != 0 || gCommunications->GetIsGamePaused() || this->mGameOver == true || gWorld.mCameraControl.mUnitA == NULL)
        gWorld.mCameraControl.Update(this->mSpeed);
    if (!(this->mTicksToProcess > 0))
        return;
    this->mGameClock = this->GetTickTime() + this->mGameClock;
    this->mGameTime = (long)this->mGameClock;
    this->mTickRandom = UMFloatToLong(this->mRandom.Generate());
    this->mPreviousTickStartTime = this->mTickStartTime;
    this->mTickStartTime = TMGetTime();
    this->mTicksToProcess--;
    if (this->mGameOver != false)
        return;
    gWorld.UpdateActiveUnits();
    if (this->mCivTimerRuns)
        this->UpdateCivTimer();
    if (this->mResourceTimerPending) {
        long theResourceTime = gWorldTable.GetLongValue(kWorldDeathmatchJitManufacturingDelay) * 1000;
        if (theResourceTime <= this->mGameClock) {
            this->mResourceTimerPending = false;
            if (gWorld.mGameOptions.mResources == 4
                && gCivPowersManager.HasPower(gWorld.GetCurrentPlayer(), kCivPowerJustInTimeManufacturing)) {
                UWideString theText;
                gResource->GetString(IDS_JUST_IN_TIME_MANUFANUFACTURING_IS_NOW, theText);
                gUserInterface->AddChatLogEntry(theText, 0);
            }
        }
    }
    this->UpdateWeather();
    gPathManager.Update();
    this->UpdateAreaEffectsClock();
    this->CheckShiftMode();
    this->UpdateUnits();
    gEffectManager->Update();
    EECalamityManager::GetInstance()->Update(this->mTickTime);
    gArtificialIntelligence->Update();
    long theResult = 0;
    if (!gWorld.mInCinematic)
        theResult = gWorld.mScenarioManager->mGameResult;
    gWorld.mScenarioManager->Update();
    long theNewResult = gWorld.mInCinematic ? 0 : gWorld.mScenarioManager->mGameResult;
    if (theNewResult == 0) {
        if (!gWorld.mInCinematic) {
            theNewResult = gWorld.CheckGameOver();
            if (theNewResult != 0) {
                gWorld.mScenarioManager->mGameResult = theNewResult;
                this->EndGame(theNewResult == 1);
            }
        }
    } else if (theResult == 0)
        this->EndGame(theNewResult == 1);
    this->FlushWorld();
    unsigned long theStart = this->mTickStartTime;
    this->mTickWorkSum += TMGetTime() - theStart;
    this->mTickWorkCount++;
    if (this->mTickWorkSum >= 1000) {
        this->mAveragePhysicsUpdateTime = this->mTickWorkSum / this->mTickWorkCount;
        this->mTickWorkSum = 0;
        this->mTickWorkCount = 0;
    }
    this->mTickGapSum += theStart - this->mPreviousTickStartTime;
    this->mTickGapCount++;
    if (this->mTickGapSum >= 1000) {
        this->mAverageTimeBetweenPhysicsTicks = this->mTickGapSum / this->mTickGapCount;
        this->mTickGapSum = 0;
        this->mTickGapCount = 0;
    }
    this->mNumTicksDone++;
    if (this->mGameStarted) {
        this->mGameStarted = false;
        if ((gWorld.mGameOptions.mCampaignName.GetLength() > 0 && gWorld.mGameOptions.mCampaignChecksum > 0)
            || gWorld.mGameOptions.mIsScenarioGame)
            if (static_cast<unsigned long>(gWorld.GetCurrentPlayer()->mCivilization.GetAvailablePoints()) > 0)
                EESetCivBuilderButtonBlinking(true);
    }
    if (gCommunications->IsServer() != true || !(gGamePreferences.mSettings.mAutoSaveInMilliseconds > 0))
        return;
    this->mAutoSaveClock += this->mTimerInterval;
    if (!(this->mAutoSaveClock > gGamePreferences.mSettings.mAutoSaveInMilliseconds) || gWorld.mInCinematic
        || gWorld.mScenarioManager->mIsShowingDialog)
        return;
    this->mAutoSaveClock = 0;
    gPhysics->PostAutoSave();
    gCommunications->GetCurrentCommunicationsTick();
    gCommunications->GetCurrentCommunicationsTick();
}

// Queues a load/save request under the save-game lock, taking a reference to it
void EEPhysics::QueueMessage(NEMessage* inMessage)
{
    this->mSaveGameLock->Enter();
    this->mLoadSaveRequests.push_back(inMessage);
    inMessage->Reference();
    this->mSaveGameLock->Leave();
}

// Applies one unit swap: the camera follows the new unit (or drops the old one), and the contagions, the
// scenario and the effects switch to it
// NEAR: the original loads mUnitA into eax first and only the else branch keeps &gWorld.mCameraControl in ebx
void EEPhysics::SwapUnit(EEUnit* inOld, EEUnit* inNew)
{
    EEUnit* theUnitA = gWorld.mCameraControl.mUnitA;
    if (gWorld.mCameraControl.mUnitB == inOld && theUnitA == inOld)
        gWorld.mCameraControl.Follow(inNew, gWorld.mCameraControl.mIsFollowingUnitB, 1.0f,
                                       gWorld.mCameraControl.mTurnYaw, gWorld.mCameraControl.mTurnPitch);
    else {
        EECameraControl& theCamera = gWorld.mCameraControl;
        if (theUnitA == inOld)
            theCamera.ClearUnitA();
        if (theCamera.mUnitB == inOld)
            theCamera.ClearUnitB();
    }
    gWorld.GetContagionManager()->SwapUnit(inOld, inNew);
    gWorld.mScenarioManager->ReplaceUnit(inOld->mUnitID, inNew->mUnitID);
    gEffectManager->ReplaceUnit(inOld, inNew);
}

// Applies the queued unit swaps: tells every player but player 0, swaps each pair, releases both units, clears
void EEPhysics::ProcessUnitSwaps()
{
    if (this->mUnitSwaps.size() <= 0)
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++)
        gWorld.mPlayers[i]->UnitsSwapped(this->mUnitSwaps);
    for (std::vector<EEPUnitSwap>::iterator it = this->mUnitSwaps.begin(); it != this->mUnitSwaps.end(); it++) {
        EEUnit* theOld = it->mOld;
        EEUnit* theNew = it->mNew;
        this->SwapUnit(theOld, theNew);
        theOld->Dereference();
        theNew->Dereference();
    }
    this->mUnitSwaps.clear();
}

// TEST ONLY: the UI fixture's view of each executed input (EETestFixtureOrders.cpp: the orders' verdicts), called
// before (0) and after (1) the message executes, under the world's write lock; NULL unless the fixture runs
void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase) = NULL;

// New (port): the game log's human orders (ee/EEGameLog.h, TEST ONLY: training data from playtests). One "order" line per
// order message a human player's units run, as it executes in the lockstep (the same in multiplayer and replays),
// and one "select" line when a human's selection changes. It reads the message and the units; it changes nothing.
static void EEPhysicsLogUnits(std::string& ioLine, EEGroup* inGroup)
{
    std::map<std::string, long> theClasses;
    std::string theIDs;
    unsigned long n = inGroup != NULL ? inGroup->GetNumUnits() : 0;
    for (unsigned long i = 0; i < n; i++) {
        EEComplexUnit* u = inGroup->GetUnit((long)i);
        if (u == NULL || u->mUnitClass == NULL)
            continue;
        theClasses[EECPNextClassName(u->mUnitClass->mUnitType)]++;
        if (i < 24) {
            char b[24];
            snprintf(b, sizeof(b), "%s%lu", theIDs.empty() ? "" : ",", (unsigned long)u->mUnitID);
            theIDs += b;
        } else if (i == 24)
            theIDs += ",...";
    }
    std::string theList;
    for (std::map<std::string, long>::const_iterator it = theClasses.begin(); it != theClasses.end(); ++it) {
        char b[96];
        snprintf(b, sizeof(b), "%s%s:%ld", theList.empty() ? "" : ",", it->first.c_str(), it->second);
        theList += b;
    }
    char b[64];
    snprintf(b, sizeof(b), " units=%lu", n);
    ioLine += b;
    ioLine += " classes=" + (theList.empty() ? std::string("-") : theList);
    ioLine += " ids=" + (theIDs.empty() ? std::string("-") : theIDs);
}

static void EEPhysicsLogTarget(std::string& ioLine, long inID)
{
    EEUnit* t = inID > 0 ? gWorld.GetUnit((unsigned long)inID) : NULL;
    char b[160];
    if (t != NULL && t->mUnitClass != NULL)
        snprintf(b, sizeof(b), " target=%ld:%s:p%lu@%ld,%ld", inID, EECPNextClassName(t->mUnitClass->mUnitType).c_str(),
                 t->mPlayer != NULL ? (unsigned long)t->mPlayer->mPlayerID : 0UL, (long)t->GetTileX(), (long)t->GetTileY());
    else
        snprintf(b, sizeof(b), " target=%ld:-", inID);
    ioLine += b;
}

static void EEPhysicsLogAt(std::string& ioLine, float inX, float inY)
{
    char b[64];
    snprintf(b, sizeof(b), " at=%ld,%ld", (long)inX, (long)inY);
    ioLine += b;
}

static void EEPhysicsLogTech(std::string& ioLine, EEPlayer* inPlayer, long inTech, const char*& ioVerb)
{
    EETechTreeNode* theNode = inPlayer->GetTechTree() != NULL ? inPlayer->GetTechTree()->Get(inTech) : NULL;
    if (theNode != NULL && theNode->mData != NULL && theNode->mData->mMakesUnit) {
        if (strcmp(ioVerb, "research") == 0)
            ioVerb = "train";
        ioLine += " class=" + EECPNextClassName(theNode->mData->mUnitClass);
    } else
        ioLine += " tech=" + EECPNextTechName(inTech);
}

static void EEPhysicsLogOrder(NEMessage* inMessage, unsigned long inTime)
{
    if (!EEGameLogOn())
        return;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(inMessage->GetOriginatingSlot());
    if (thePlayer == NULL || !thePlayer->IsHuman())
        return;
    const char* theVerb = NULL;
    std::string theRest;
    bool theGroup = true;   // the order is for the player's active group
    // the orders are records (EEMRecord): Get<T>(i) reads member i in wire order (ee_netmsg classes.rs)
    switch (inMessage->GetType()) {
    case kMessagePathTo: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "move";
        EEPhysicsLogAt(theRest, m->Get<float>(2), m->Get<float>(3));
        if (m->Get<bool>(4))
            theRest += " forced=1";
        if (m->Get<bool>(6))
            theRest += " queued=1";
        break;
    }
    case kMessageAttackUnit: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = m->Get<bool>(3) ? "convert" : "attack";
        EEPhysicsLogTarget(theRest, m->Get<int32_t>(1));
        if (m->Get<bool>(2))
            theRest += " queued=1";
        break;
    }
    case kMessageGatherResource: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "gather";
        EEPhysicsLogTarget(theRest, m->Get<int32_t>(1));
        if (m->Get<bool>(2))
            theRest += " queued=1";
        break;
    }
    case kMessageBuildUnit: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "build";
        EEPhysicsLogTech(theRest, thePlayer, m->Get<int32_t>(2), theVerb);
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(1).GetX(), m->Get<GE2DPoint>(1).GetY());
        break;
    }
    case kMessageRepairUnit:
        theVerb = "repair";
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessageHaltSelectedGroup:
    case kMessageStopActiveGroup:
        theVerb = "stop";
        break;
    case kMessageLoadGarrison: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "garrison";
        EEPhysicsLogTarget(theRest, m->Get<int32_t>(1));
        if (m->Get<int32_t>(2) > 0) {
            char b[32];
            snprintf(b, sizeof(b), " unit=%ld", (long)m->Get<int32_t>(2));
            theRest += b;
        }
        break;
    }
    case kMessageLoadTransport:
        theVerb = "load";
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessageUnloadTransport: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "unload";
        theGroup = false;
        EEPhysicsLogTarget(theRest, m->Get<int32_t>(1));
        EEPhysicsLogAt(theRest, (float)m->Get<int32_t>(2), (float)m->Get<int32_t>(3));
        break;
    }
    case kMessageImmediateCargoUnload:
        theVerb = "unload";
        theGroup = false;
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessagePatrol:
        theVerb = "patrol";
        break;
    case kMessageAddPatrolPoint: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "patrol_point";
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(1).GetX(), m->Get<GE2DPoint>(1).GetY());
        break;
    }
    case kMessageChangeFormation: {
        char b[32];
        theVerb = "formation";
        snprintf(b, sizeof(b), " kind=%ld", (long)static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        theRest += b;
        break;
    }
    case kMessageResearchTechnology: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "research";
        theGroup = false;
        EEPhysicsLogTarget(theRest, m->Get<int32_t>(1));
        EEPhysicsLogTech(theRest, thePlayer, m->Get<int32_t>(2), theVerb);
        if (m->Get<bool>(3))
            theRest += " times=5";
        break;
    }
    case kMessageExplore:
        theVerb = "explore";
        break;
    case kMessageAttackGround: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "attack_ground";
        EEPhysicsLogAt(theRest, m->Get<float>(1), m->Get<float>(2));
        break;
    }
    case kMessageHealUnit:
        theVerb = "heal";
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessageSetRallyGoal: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "rally";
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(1).GetX(), m->Get<GE2DPoint>(1).GetY());
        if (m->Get<int32_t>(2) > 0)
            EEPhysicsLogTarget(theRest, m->Get<int32_t>(2));
        break;
    }
    case kMessageReturnToBase:
        theVerb = "return";
        break;
    case kMessageDropoffResource:
        theVerb = "dropoff";
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessageSetUnitBehavior: {
        char b[32];
        theVerb = "stance";
        snprintf(b, sizeof(b), " kind=%ld", (long)static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        theRest += b;
        break;
    }
    case kMessageBuildWall: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "wall";
        EEPhysicsLogTech(theRest, thePlayer, m->Get<int32_t>(3), theVerb);
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(1).GetX(), m->Get<GE2DPoint>(1).GetY());
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(2).GetX(), m->Get<GE2DPoint>(2).GetY());
        break;
    }
    case kMessageBuildGate: {
        EEMRecord* m = static_cast<EEMRecord*>(inMessage);
        theVerb = "gate";
        EEPhysicsLogTech(theRest, thePlayer, m->Get<int32_t>(2), theVerb);
        EEPhysicsLogAt(theRest, m->Get<GE2DPoint>(1).GetX(), m->Get<GE2DPoint>(1).GetY());
        break;
    }
    case kMessageDisband:
    case kMessageKillUnit:
    case kMessageSelfDestruct:
        theVerb = "delete";
        break;
    case kMessageImproveUnit:
        theVerb = "improve";
        theGroup = false;
        EEPhysicsLogTarget(theRest, static_cast<EEMRecord*>(inMessage)->Get<int32_t>(1));
        break;
    case kMessagePackUnpack:
        theVerb = static_cast<EEMRecord*>(inMessage)->Get<bool>(1) ? "pack" : "unpack";
        break;
    case kMessageGoFanatic:
        theVerb = "fanatic";
        break;
    case kMessageReplantFarms:
        theVerb = "replant";
        break;
    case kMessageCastCalamity:
        theVerb = "calamity";
        break;
    default:
        return;
    }
    char theHead[96];
    snprintf(theHead, sizeof(theHead), "EE-GAME %lu p%lu order verb=%s", inTime, (unsigned long)thePlayer->mPlayerID, theVerb);
    std::string theLine(theHead);
    if (theGroup)
        EEPhysicsLogUnits(theLine, thePlayer->mActiveGroup);
    theLine += theRest;
    theLine += "\n";
    EEGameLogLine(theLine.c_str());
}

static void EEPhysicsLogSelection(NEMessage* inMessage, unsigned long inTime)
{
    if (!EEGameLogOn())
        return;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(inMessage->GetOriginatingSlot());
    if (thePlayer == NULL || !thePlayer->IsHuman() || thePlayer->mPlayerID >= 64)
        return;
    std::string theUnits;
    EEPhysicsLogUnits(theUnits, thePlayer->mActiveGroup);
    static std::string sLast[64];
    if (theUnits == sLast[thePlayer->mPlayerID])
        return;
    sLast[thePlayer->mPlayerID] = theUnits;
    char theHead[64];
    snprintf(theHead, sizeof(theHead), "EE-GAME %lu p%lu select", inTime, (unsigned long)thePlayer->mPlayerID);
    EEGameLogLine((std::string(theHead) + theUnits + "\n").c_str());
}

// Runs the queued physics tick messages (one while the timer runs): checksums or dumps the world when asked,
// takes the owed ticks and speed, executes the tick's player inputs; a stray non-tick message at the head is dropped
// NEAR: inline budget: the original calls deque::begin() out of line for the first front(); esi/ebx/edi roles
void EEPhysics::DrainInput()
{
    this->mServerMessageQueueLock->Enter();
    while (this->mServerMessages.size() > 0) {
        if (this->mTicksToProcess != 0 && this->mTimerRunning)
            break;
        NEMessage* theMessage = this->mServerMessages.front();
        if (theMessage->GetType() != kNetMessagePhysicsTick) {
            // player inputs are consumed with their tick, so the head should always be a tick; drop a stray
            // message instead of spinning on it for ever while holding the queue lock
            this->mServerMessages.pop_front();
            theMessage->Dereference();
            continue;
        }
        this->mServerMessages.pop_front();
        // the inputs that follow, the PTs to perform, the checksum and dump flags, the PTs per second (not on the wire)
        NEMRecord* theTick = static_cast<NEMRecord*>(theMessage);
        if (theTick->Get<bool>(2) == true) {
            if (gCommunications->IsServer() == true
                || (gCommunications->IsClient() == true && static_cast<EEClient*>(gCommunications)->mHasLeftGame == 0)) {
                bool theDump = theTick->Get<bool>(3);
                unsigned long theChecksum = this->GetOutOfSynchChecksum(theDump, 1);
                if (!theDump) {
                    unsigned long theTick2 = gCommunications->GetNumberOfCTsProcessedInPhysics();
                    NEMRecord* theResponse = new NEMRecord(kNetMessageWorldDatabaseChecksumResponse, {theTick2, theChecksum});
                    theResponse->SetOriginatingSlot(gWorld.GetCurrentPlayer()->mSlot);
                    if (gCommunications->IsClient())
                        gCommunications->ProcessLocalUserInput(theResponse, kSlotNone);
                    else {
                        static_cast<NESynchronousServer*>(gCommunications)->LogChecksumResponse(theResponse);
                        theResponse->Dereference();
                    }
                }
            }
        }
        gCommunications->IncrementNumberOfCTsProcessedInPhysics();
        gCommunications->SetAveragePhysicsUpdateTime(this->mAveragePhysicsUpdateTime);
        gCommunications->SetAverageTimeBetweenPhysicsTicks(this->mAverageTimeBetweenPhysicsTicks);
        this->mCurrentTick += theTick->Get<unsigned char>(1);
        this->mTicksToProcess = theTick->Get<unsigned char>(1);
        this->SetSpeed(1.0f / theTick->Get<unsigned char>(4));
        unsigned long theCount = theTick->Get<uint32_t>(0);
        theTick->Dereference();
        gCommunications->GetNumberOfCTsProcessedInPhysics();
        for (; theCount > 0; theCount--) {
            NEMessage* theInput = this->mServerMessages.front();
            this->mServerMessages.pop_front();
            theInput->GetMessageLabel();
            if (theInput->GetForSpecificSlot() == kSlotNone)
                this->mInputMessagesProcessed++;
            EEMessageCaptureExecuted(theInput);   // TEST ONLY: the simulation capture's NMEX record (off by default)
            if (EESimCaptureRecord::Active())   // TEST ONLY: the unit store equals the objects before the message
                EEUnitStoreCheck(theInput->GetType());
            if (gEEMessageExecutedTestHook != NULL)   // TEST ONLY: the UI fixture's orders (EETestFixtureOrders.cpp)
                gEEMessageExecutedTestHook(theInput, 0);
            switch (theInput->GetType()) {
            case kNetMessageRemovePlayer: {
                NEMRecord* theRemove = static_cast<NEMRecord*>(theInput);   // the player, out of synch, exiting, booted
                gCommunications->ExecuteRemovePlayer(theRemove->Get<unsigned char>(0), theRemove->Get<bool>(3),
                                                      theRemove->Get<bool>(1), theRemove->Get<bool>(2));
                break;
            }
            case kMessageSelectUnit:
            case kMessageAddUnitToSelectedGroup:
            case kMessageRemoveUnitFromSelectedGroup:
            case kMessageUnselectAllUnits:
            case kMessageSelectUnits:
            case kMessageAddUnitsToSelectedGroup:
            case kMessageAddAndRemoveUnitsFromSelectedGroup:
            case kMessageAlwaysAddUnitToSelectedGroup:
            case kMessageSavedGroupAddSelection:
            case kMessageSavedGroupSelect:
            case kMessageSelectTheseUnits:
                EEGameMessageCast(theInput)->ExecuteSelectionMessage();
                EEPhysicsLogSelection(theInput, this->mGameTime);   // new (port): the game log's human selection
                break;
            default: {
                EEPhysicsLogOrder(theInput, this->mGameTime);   // new (port): the game log's human orders
                EEMessage* theGameMessage = EEGameMessageCast(theInput);
                if (theGameMessage != NULL)
                    theGameMessage->ExecuteMessage();
                else
                    theInput->Execute();
                break;
            }
            }
            if (gEEMessageExecutedTestHook != NULL)
                gEEMessageExecutedTestHook(theInput, 1);
            theInput->Dereference();
        }
    }
    this->mServerMessageQueueLock->Leave();
}

// Every 2.5..5 s (a random period drawn once) sends the current player's two statistics to the others when
// they changed
// (Virtual20's; provisional names)
void EEPhysics::ReportStatistics()
{
    static unsigned long sLastReport = TMGetTime();
    unsigned long theTime = TMGetTime();
    if (sFirstReport) {
        sFirstReport = false;
        srand(TMGetTime());
        sReportPeriod = rand() % 2500u + 2500u;
    }
    if (!(theTime - sLastReport > sReportPeriod))
        return;
    sLastReport = theTime;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    unsigned long theFirst = thePlayer->mSavedGroups.GetLastValue(0x14);
    unsigned long theSecond = thePlayer->mSavedGroups.GetLastValue(0x15);
    if (theFirst == sReportedFirst && theSecond == sReportedSecond)
        return;
    sReportedFirst = theFirst;
    sReportedSecond = theSecond;
    gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReportStatistics, {theFirst, theSecond}), kSlotNone);
}

// Creates the game timer
void EEPhysics::CreateGameTimer()
{
    EECreateGameTimer();
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
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x50a, NULL, NULL, true);
    return theResult;
}

// Posts the show-scenario-dialog message (0x508) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostShowDialogMessage()
{
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x508, NULL, NULL, true);
    return theResult;
}

// Posts the re-init message (0x509) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostInit()
{
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x509, NULL, NULL, true);
    return theResult;
}

// Recomputes the ticks per game second and the tick length (s and ms) from the tick rate and the game speed
void EEPhysics::UpdateTiming()
{
    this->mTicksPerGameSecond = this->mTicksPerSecond / gWorld.GetGameSpeed();
    this->mTickSeconds = 1.0f / this->mTicksPerGameSecond;
    this->mTickTime = this->mTickSeconds * 1000.0f;
    this->mTickTimeMs = UMFloatToLong(this->mTickTime);
}

// Queues a unit swap (inOld replaced by inNew), taking a reference to both units
void EEPhysics::QueueUnitSwap(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    inOld->Reference();
    inNew->Reference();
    EEPUnitSwap theSwap;
    theSwap.mOld = inOld;
    theSwap.mNew = inNew;
    this->mUnitSwaps.push_back(theSwap);
}

// Releases the world's write lock
void EEPhysics::Unlock()
{
    gWorld.mLock->WriteUnlock();
}

// Advances the area effects clock and ticks the area effects once per 2 s of game time
void EEPhysics::UpdateAreaEffectsClock()
{
    this->mAreaEffectsClock += this->mTickTime;
    if (this->mAreaEffectsClock >= kAreaEffectInterval) {
        gAEManager.Tick(kAreaEffectInterval);
        this->mAreaEffectsClock -= kAreaEffectInterval;
    }
}

// Runs the civilization timer on a server: warnings at 240 s and 270 s, expiry at 300 s, and the civ builder
// button blinks for a player with no civilization; elsewhere (or with no civ buying) the timer stops
void EEPhysics::UpdateCivTimer()
{
    if (gCommunications != NULL && gCommunications->IsServer()) {
        if (gWorld.mGameOptions.CanBuyCivilization()) {
            if (this->mTickWorkCount == 0 && !gWorld.GetCurrentPlayer()->mCivilizationChosen && !gWorld.GetCurrentPlayer()->mObserverMode)
                EESetCivBuilderButtonBlinking(true);
            EEMRecord* theMessage;
            if (this->mGameTime > EEGetCivTimerLimit()) {
                this->mCivTimerRuns = false;
                theMessage = new EEMRecord(kMessageCivTimerExpired, {(unsigned char)0});
            } else if (this->mGameTime > EEGetCivTimerSecondWarning() && !this->mCivTimerSecondWarning) {
                this->mCivTimerSecondWarning = true;
                theMessage = new EEMRecord(kMessageCivTimerExpired, {(unsigned char)1});
            } else if (this->mGameTime > EEGetCivTimerFirstWarning() && !this->mCivTimerFirstWarning) {
                this->mCivTimerFirstWarning = true;
                theMessage = new EEMRecord(kMessageCivTimerExpired, {(unsigned char)1});
            } else
                return;
            if (theMessage != NULL)
                gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        } else
            this->mCivTimerRuns = false;
    } else {
        this->mCivTimerRuns = false;
        if (gWorld.mGameOptions.CanBuyCivilization() && !gWorld.GetCurrentPlayer()->mCivilizationChosen && !gWorld.GetCurrentPlayer()->mObserverMode)
            EESetCivBuilderButtonBlinking(true);
    }
}

// Between ticks runs every queued load/save request (under the save-game lock) and sends one response with
// the last result (logged on a server); with ticks owed it waits
// NEAR: the original inlines the EEMLoadOrSaveGameResponse ctor (ee/EEMessage.h declares it out of line, 0xc1025)
// and so calls _Ptrit::operator+ out of line
void EEPhysics::ServeLoadSaveRequests()
{
    this->mSaveGameLock->Enter();
    if (this->mLoadSaveRequests.size() > 0) {
        if (this->mTicksToProcess == 0) {
            long theResult = 0;
            for (unsigned long i = 0; i < this->mLoadSaveRequests.size(); i++) {
                theResult = EELoadOrSave(static_cast<EEMRecord*>(this->mLoadSaveRequests[i]));
                this->mLoadSaveRequests[i]->Dereference();
                this->mLoadSaveRequests[i] = NULL;
            }
            this->mLoadSaveRequests.clear();
            EEMRecord* theResponse = new EEMRecord(kMessageLoadOrSaveGameResponse, {theResult});
            theResponse->SetOriginatingSlot(gWorld.GetCurrentPlayer()->GetSlot());
            if (gCommunications->IsServer()) {
                static_cast<NESynchronousServer*>(gCommunications)->LogResponse(theResponse);
                theResponse->Dereference();
            } else
                gCommunications->ProcessLocalUserInput(theResponse, kSlotServer);
        } else {
            gCommunications->GetNumberOfCTsProcessedInPhysics();
            gCommunications->GetCurrentCommunicationsTick();
        }
    }
    this->mSaveGameLock->Leave();
}

// Every 4 s of game time takes the current player out of shift mode when the Shift key (0xe1) is up
// (Virtual20's; provisional name)
void EEPhysics::CheckShiftMode()
{
    this->mShiftModeClock += this->mTickTime;
    if (!(this->mShiftModeClock >= 4000.0f))
        return;
    this->mShiftModeClock -= 4000.0f;
    if (!gWorld.GetCurrentPlayer()->mShiftMode)
        return;
    ISKeyboard* theKeyboard = gISKeyboard;
    ISEntity* theKey;
    if (theKeyboard != NULL && theKeyboard->GetEntity(kKeyShift, theKey) == kErrorOK && theKey->GetValue() == 0)
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageExitShiftMode, {this->mGameTime}), kSlotNone);
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
    if (EEUnitStepNoting() && (inIndex >= gWorld.mUpdateUnits.size() || gWorld.mUpdateUnits[inIndex].GetSavedID() != inID))
        EEUnitStepDiffers("UpdateUnits (the stepped unit's id)", inID, (long)inIndex,
                          inIndex < gWorld.mUpdateUnits.size() ? (long)gWorld.mUpdateUnits[inIndex].GetSavedID() : -1);
    gPhysicsSteppingUnit = gWorld.mUpdateUnits[inIndex];   // TEST ONLY: named by EE-SIM's random-draw trace
    gWorld.mUpdateUnits[inIndex]->Step();
    EEUnitUpdateCommit(inID);   // TEST ONLY (USST kind 12)
}

unsigned long EEPhysics::UpdateUnits()
{
    if ((size_t)ee_units_order_count() != gWorld.mUpdateUnits.size()) {
        if (EEUnitStepNoting())   // TEST ONLY: never seen (USCK compares the two at every check)
            EEUnitStepDiffers("UpdateUnits (the stepped units' count)", 0, (long)ee_units_order_count(),
                              (long)gWorld.mUpdateUnits.size());
        EEUnitOrderReset();   // a played game's safety net: the steps index the deque by the list's count
    }
    EeUnitsTickHost theHost;
    theHost.ctx = NULL;
    theHost.num_players = (uint32_t)gWorld.mNumPlayers;
    theHost.detection_unit = (uint32_t)gCollisionDetectionUnit;
    theHost.set_collision_period = TickSetCollisionPeriod;
    theHost.update_player = TickUpdatePlayer;
    theHost.step = TickStep;
    unsigned long theCount = ee_units_update_units(&theHost);
    gPhysicsSteppingUnit = NULL;
    EEUnitStepCommit();   // TEST ONLY: the step leaves' record (USTM, ee/EEUnitStore.h)
    return theCount;
}

EEComplexUnit* gPhysicsSteppingUnit = NULL;   // TEST ONLY (ee/EEPhysics.h)
void (*gPhysicsAfterTickHook)(void) = NULL;   // TEST ONLY (ee/EEPhysics.h; RTSPhysics::Main's simulation loop)

static const float kWindDrift = 0.005f;   // 0x442370 (unit-local const: the wind direction random walk step; provisional name)

// Advances the day/night clock (when it runs) and the wind direction's random walk, then ticks the environment
// EEEnvironmentManager::Tick is an out-of-line member defined at the end of this file (0x55439f)
void EEPhysics::UpdateWeather()
{
    GETransformation theTransformation;
    GEVector theNorth(0.0f, 0.001f, 0.0f);
    GEVector theWind(0.0f, 0.0f, 0.0f);
    if (this->mTimeOfDayRuns) {
        float theTime = this->GetTickSeconds() * 0.0016666667f + this->mTimeOfDayStep;
        if (theTime >= 0.0f) {
            float theTimeOfDay = theTime + gWorld.GetTimeOfDay();
            if (theTimeOfDay >= 1.0f)
                theTimeOfDay = 0.0f;
            gWorld.SetTimeOfDay(theTimeOfDay);
            theTime = 0.0f;
        }
        this->mTimeOfDayStep = theTime;
    }
    float theAngle = gWorld.mWindYaw;
    URandom& theRandom = gPhysics->mRandom;
    float theElapsed = this->GetTickTime();
    gWorld.mWindYaw = ((theRandom.Generate() * 2.0f - 1.0f) * theElapsed) * kWindDrift + theAngle;
    float theYaw = gWorld.mWindYaw;
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theTransformation.Apply(theNorth, theWind);
    gWorld.mWindDirection = theWind;
    gWorld.mEnvironmentManager->Tick(this->mGameTime);
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
