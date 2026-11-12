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
#include "lle/ISDevice.h"
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
static unsigned long sReportedFirst;        // 0x531cf8
static unsigned long sReportedSecond;       // 0x531cfc
static unsigned long sReportPeriod;         // 0x531d00
static bool sFirstReport = true;            // 0x4c1e34 (.data)
extern UString gCampaignUser;              // 0x531cd0 (defined at the end of this unit; provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (the main game form's blinking buttons)
unsigned long EEGetCivTimerLimit();         // 0x27ffae (cdecl: 300000; the civ timer limits, EEUIFMainGame.cpp's range)
unsigned long EEGetCivTimerSecondWarning();  // 0x27ffba (270000) (provisional names)
unsigned long EEGetCivTimerFirstWarning();  // 0x27ffb4 (240000)
extern unsigned long& gContinentManagerChecksum;  // 0x530d10 (the continent manager's checksum; provisional name)
extern unsigned long gCollisionPeriod;      // 0x4c237c (.data = 1 after gCollisionDetectionUnit; provisional name)
// SYNTHETIC: EXE 0x551c28   (wrapper)
// SYNTHETIC: EXE 0x551c32   (ctor)
// SYNTHETIC: EXE 0x551c43   (registrar)
// SYNTHETIC: EXE 0x551c4f   (dtor)

// FUNCTION: EXE 0x551c5a
// Creates the physics simulation: makes its save-game lock, resets its clocks and starts the thread's main loop
EEPhysics::EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer)
    : RTSPhysics(inGraphics, inTimer, 1, 0xf), mInputMessagesProcessed(0), mLastOutOfSynchChecksum(0)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSaveGameLock) == keseOK, 281);
    this->Init();
    this->mStartThreadMainLoop->Set();
}

// SYNTHETIC: EXE 0x551cd4
// (the scalar deleting destructor, emitted with the vtable; slot EEPhysics+0x10)

// FUNCTION: EXE 0x551cf0
// Destroys the physics simulation, releasing its save-game lock
EEPhysics::~EEPhysics()
{
    TSScheduler::DestroyCriticalSection(this->mSaveGameLock);
}

// FUNCTION: EXE 0x551d1f
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
            new EEMLoadOrSaveGame(theName, TMGetTime(), 0, 1, gCommunications->GetNumberOfCTsProcessedInPhysics(), 1),
            15000, 12);
    }
}

// FUNCTION: EXE 0x551de3
// Creates a load-or-save-game message for a file name, time and tick, with its load, quiet and autosave flags
EEMLoadOrSaveGame::EEMLoadOrSaveGame(const UWideString& inS20, unsigned long inTime, unsigned char inIsLoad,
                                     bool inIsQuiet, long inTick, unsigned char inIsAutoSave)
    : EEMessage(kMessageLoadOrSaveGame, true), mIsLoad(inIsLoad), mIsQuiet(inIsQuiet)
{
    this->mFileName = inS20;
    this->mTime = inTime;
    this->mIsAutoSave = inIsAutoSave;
    this->mTick = inTick;
}

// FUNCTION: EXE 0x551e4d
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

// FUNCTION: EXE 0x551e9f
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
    this->ProcessUnitSwaps();
    for (i = 0; i < theNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL)
            gWorld.mPlayers[i]->RemoveKilledUnits();
    }
}

// FUNCTION: EXE 0x551efe
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

// FUNCTION: EXE 0x551f3a
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

// FUNCTION: EXE 0x5520cf
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
        ESEndRecording();
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

// FUNCTION: EXE 0x552525
// (EEPlayer::GetNumRunningGroups: the inline COMDAT after its first user above)

// FUNCTION: EXE 0x55253e
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

// FUNCTION: EXE 0x55268d
// Splits a game time in ms (0: the current one) into whole minutes and the seconds past them
void EEPhysics::GetMinutesAndSeconds(unsigned long* outMinutes, unsigned long* outSeconds, unsigned long inTime)
{
    if (inTime == 0)
        inTime = this->mGameTime;
    *outMinutes = inTime / 1000 / 60;
    *outSeconds = (inTime - *outMinutes * 60000) / 1000;
}

// FUNCTION: EXE 0x5526d2
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

// FUNCTION: EXE 0x552909
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

// FUNCTION: EXE 0x552a4d
// Reads the physics clocks, tick counters, timing averages and civ timer state from a saved game (the game-over
// flag after version 0.0xce, the resource timer from 0xfa); returns the first read error
long EEPhysics::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    unsigned long theValue = 0;
    if (inMajorVersion == 0 && inMinorVersion <= 0xce) {
        theResult = RTSPhysics::Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mCurrentTick);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mF84);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mTicksPerGameSecond);
                    if (theResult == kErrorOK) {
                        theResult = inFile.Read(this->mTickSeconds);
                        if (theResult == kErrorOK) {
                            theResult = inFile.Read(this->mTimeOfDayStep);
                            if (theResult == kErrorOK) {
                                theResult = inFile.Read(this->mTickTime);
                                this->mTickTimeMs = UMFloatToLong(this->mTickTime);   // port: as UpdateTiming rounds it (the cast truncated)
                                if (theResult == kErrorOK) {
                                    theResult = inFile.Read(this->mTickStartTime);
                                    if (theResult == kErrorOK) {
                                        theResult = inFile.Read(this->mPreviousTickStartTime);
                                        if (theResult == kErrorOK) {
                                            theResult = inFile.Read(this->mNumTicksDone);
                                            if (theResult == kErrorOK) {
                                                theResult = inFile.Read(this->mTicksToProcess);
                                                if (theResult == kErrorOK) {
                                                    theResult = inFile.Read(this->mTickGapSum);
                                                    if (theResult == kErrorOK) {
                                                        theResult = inFile.Read(this->mTickGapCount);
                                                        if (theResult == kErrorOK) {
                                                            theResult = inFile.Read(this->mTickWorkSum);
                                                            if (theResult == kErrorOK) {
                                                                theResult = inFile.Read(this->mTickWorkCount);
                                                                if (theResult == kErrorOK) {
                                                                    theResult = inFile.Read(this->mGameClock);
                                                                    this->mGameTime = (long)this->mGameClock;
                                                                    if (theResult == kErrorOK) {
                                                                        theResult = inFile.Read(theValue);
                                                                        this->mAveragePhysicsUpdateTime = theValue;
                                                                        if (theResult == kErrorOK) {
                                                                            theResult = inFile.Read(theValue);
                                                                            this->mAverageTimeBetweenPhysicsTicks = theValue;
                                                                            if (theResult == kErrorOK) {
                                                                                theResult = inFile.Read(this->mTimeOfDayRuns);
                                                                                if (theResult == kErrorOK) {
                                                                                    theResult = inFile.Read(this->mAreaEffectsClock);
                                                                                    if (theResult == kErrorOK) {
                                                                                        theResult = inFile.Read(this->mCivTimerRuns);
                                                                                        if (theResult == kErrorOK) {
                                                                                            theResult = inFile.Read(this->mCivTimerFirstWarning);
                                                                                            if (theResult == kErrorOK) {
                                                                                                theResult = inFile.Read(this->mCivTimerSecondWarning);
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            this->mGameOver = false;
        }
    } else {
        theResult = RTSPhysics::Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mCurrentTick);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mF84);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mTicksPerGameSecond);
                    if (theResult == kErrorOK) {
                        theResult = inFile.Read(this->mTickSeconds);
                        if (theResult == kErrorOK) {
                            theResult = inFile.Read(this->mTimeOfDayStep);
                            if (theResult == kErrorOK) {
                                theResult = inFile.Read(this->mTickTime);
                                this->mTickTimeMs = UMFloatToLong(this->mTickTime);   // port: as UpdateTiming rounds it (the cast truncated)
                                if (theResult == kErrorOK) {
                                    theResult = inFile.Read(this->mTickStartTime);
                                    if (theResult == kErrorOK) {
                                        theResult = inFile.Read(this->mPreviousTickStartTime);
                                        if (theResult == kErrorOK) {
                                            theResult = inFile.Read(this->mNumTicksDone);
                                            if (theResult == kErrorOK) {
                                                theResult = inFile.Read(this->mTicksToProcess);
                                                if (theResult == kErrorOK) {
                                                    theResult = inFile.Read(this->mTickGapSum);
                                                    if (theResult == kErrorOK) {
                                                        theResult = inFile.Read(this->mTickGapCount);
                                                        if (theResult == kErrorOK) {
                                                            theResult = inFile.Read(this->mTickWorkSum);
                                                            if (theResult == kErrorOK) {
                                                                theResult = inFile.Read(this->mTickWorkCount);
                                                                if (theResult == kErrorOK) {
                                                                    theResult = inFile.Read(this->mGameClock);
                                                                    this->mGameTime = (long)this->mGameClock;
                                                                    if (theResult == kErrorOK) {
                                                                        theResult = inFile.Read(theValue);
                                                                        this->mAveragePhysicsUpdateTime = theValue;
                                                                        if (theResult == kErrorOK) {
                                                                            theResult = inFile.Read(theValue);
                                                                            this->mAverageTimeBetweenPhysicsTicks = theValue;
                                                                            if (theResult == kErrorOK) {
                                                                                theResult = inFile.Read(this->mTimeOfDayRuns);
                                                                                if (theResult == kErrorOK) {
                                                                                    theResult = inFile.Read(this->mAreaEffectsClock);
                                                                                    if (theResult == kErrorOK) {
                                                                                        theResult = inFile.Read(this->mCivTimerRuns);
                                                                                        if (theResult == kErrorOK) {
                                                                                            theResult = inFile.Read(this->mCivTimerFirstWarning);
                                                                                            if (theResult == kErrorOK) {
                                                                                                theResult = inFile.Read(this->mCivTimerSecondWarning);
                                                                                                if (theResult == kErrorOK) {
                                                                                                    theResult = inFile.Read(this->mGameOver);
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (inMinorVersion >= 0xfa && theResult == kErrorOK)
                theResult = inFile.Read(this->mResourceTimerPending);
        }
    }
    if (theResult == kErrorOK) {   // port: the loaded seed comes back at the first tick (sLoadedSeed)
        sLoadedSeed = this->mRandom.GetSeed();
        sLoadedSeedPending = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x552f29
// Takes the world's write lock
void EEPhysics::Lock()
{
    gWorld.mLock->WriteLock();
}

// FUNCTION: EXE 0x552f35
// Appends a message to the server message queue (no lock taken)
void EEPhysics::QueueServerMessage(NEMessage* inMessage)
{
    this->mServerMessages.push_back(inMessage);
}

// FUNCTION: EXE 0x552f45
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

// FUNCTION: EXE 0x55305a
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

// FUNCTION: EXE 0x553082
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

// FUNCTION: EXE 0x553403
// Queues a load/save request under the save-game lock, taking a reference to it
void EEPhysics::QueueMessage(NEMessage* inMessage)
{
    this->mSaveGameLock->Enter();
    this->mLoadSaveRequests.push_back(inMessage);
    inMessage->Reference();
    this->mSaveGameLock->Leave();
}

// FUNCTION: EXE 0x553430
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

// FUNCTION: EXE 0x5534d1
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
    switch (inMessage->GetType()) {
    case kMessagePathTo: {
        EEMPathTo* m = static_cast<EEMPathTo*>(inMessage);
        theVerb = "move";
        EEPhysicsLogAt(theRest, m->mX, m->mY);
        if (m->mIsForcedMove)
            theRest += " forced=1";
        if (m->mIsQueued)
            theRest += " queued=1";
        break;
    }
    case kMessageAttackUnit: {
        EEMAttackUnit* m = static_cast<EEMAttackUnit*>(inMessage);
        theVerb = m->mIsConvert ? "convert" : "attack";
        EEPhysicsLogTarget(theRest, m->mTargetID);
        if (m->mIsQueued)
            theRest += " queued=1";
        break;
    }
    case kMessageGatherResource: {
        EEMGatherResource* m = static_cast<EEMGatherResource*>(inMessage);
        theVerb = "gather";
        EEPhysicsLogTarget(theRest, m->mTargetID);
        if (m->mIsQueued)
            theRest += " queued=1";
        break;
    }
    case kMessageBuildUnit: {
        EEMBuildUnit* m = static_cast<EEMBuildUnit*>(inMessage);
        theVerb = "build";
        EEPhysicsLogTech(theRest, thePlayer, m->mTechnology, theVerb);
        EEPhysicsLogAt(theRest, m->mPoint.GetX(), m->mPoint.GetY());
        break;
    }
    case kMessageRepairUnit:
        theVerb = "repair";
        EEPhysicsLogTarget(theRest, static_cast<EEMRepairUnit*>(inMessage)->mTargetID);
        break;
    case kMessageHaltSelectedGroup:
    case kMessageStopActiveGroup:
        theVerb = "stop";
        break;
    case kMessageLoadGarrison: {
        EEMLoadGarrison* m = static_cast<EEMLoadGarrison*>(inMessage);
        theVerb = "garrison";
        EEPhysicsLogTarget(theRest, m->mGarrisonID);
        if (m->mUnitID > 0) {
            char b[32];
            snprintf(b, sizeof(b), " unit=%ld", (long)m->mUnitID);
            theRest += b;
        }
        break;
    }
    case kMessageLoadTransport:
        theVerb = "load";
        EEPhysicsLogTarget(theRest, static_cast<EEMLoadTransport*>(inMessage)->mTransportID);
        break;
    case kMessageUnloadTransport: {
        EEMUnloadTransport* m = static_cast<EEMUnloadTransport*>(inMessage);
        theVerb = "unload";
        theGroup = false;
        EEPhysicsLogTarget(theRest, m->mUnitID);
        EEPhysicsLogAt(theRest, (float)m->mX, (float)m->mY);
        break;
    }
    case kMessageImmediateCargoUnload:
        theVerb = "unload";
        theGroup = false;
        EEPhysicsLogTarget(theRest, static_cast<EEMImmediateCargoUnload*>(inMessage)->mTransportID);
        break;
    case kMessagePatrol:
        theVerb = "patrol";
        break;
    case kMessageAddPatrolPoint: {
        EEMAddPatrolPoint* m = static_cast<EEMAddPatrolPoint*>(inMessage);
        theVerb = "patrol_point";
        EEPhysicsLogAt(theRest, m->mPoint.GetX(), m->mPoint.GetY());
        break;
    }
    case kMessageChangeFormation: {
        char b[32];
        theVerb = "formation";
        snprintf(b, sizeof(b), " kind=%ld", (long)static_cast<EEMChangeFormation*>(inMessage)->mFormationType);
        theRest += b;
        break;
    }
    case kMessageResearchTechnology: {
        EEMResearchTechnology* m = static_cast<EEMResearchTechnology*>(inMessage);
        theVerb = "research";
        theGroup = false;
        EEPhysicsLogTarget(theRest, m->mBuildingID);
        EEPhysicsLogTech(theRest, thePlayer, m->mTechnology, theVerb);
        if (m->mFiveTimes)
            theRest += " times=5";
        break;
    }
    case kMessageExplore:
        theVerb = "explore";
        break;
    case kMessageAttackGround: {
        EEMAttackGround* m = static_cast<EEMAttackGround*>(inMessage);
        theVerb = "attack_ground";
        EEPhysicsLogAt(theRest, m->mX, m->mY);
        break;
    }
    case kMessageHealUnit:
        theVerb = "heal";
        EEPhysicsLogTarget(theRest, static_cast<EEMHealUnit*>(inMessage)->mTargetID);
        break;
    case kMessageSetRallyGoal: {
        EEMSetRallyGoal* m = static_cast<EEMSetRallyGoal*>(inMessage);
        theVerb = "rally";
        EEPhysicsLogAt(theRest, m->mPoint.GetX(), m->mPoint.GetY());
        if (m->mTargetID > 0)
            EEPhysicsLogTarget(theRest, m->mTargetID);
        break;
    }
    case kMessageReturnToBase:
        theVerb = "return";
        break;
    case kMessageDropoffResource:
        theVerb = "dropoff";
        EEPhysicsLogTarget(theRest, static_cast<EEMDropoffResource*>(inMessage)->mDropoffID);
        break;
    case kMessageSetUnitBehavior: {
        char b[32];
        theVerb = "stance";
        snprintf(b, sizeof(b), " kind=%ld", (long)static_cast<EEMSetUnitBehavior*>(inMessage)->mBehavior);
        theRest += b;
        break;
    }
    case kMessageBuildWall: {
        EEMBuildWall* m = static_cast<EEMBuildWall*>(inMessage);
        theVerb = "wall";
        EEPhysicsLogTech(theRest, thePlayer, m->mTechnology, theVerb);
        EEPhysicsLogAt(theRest, m->mStart.GetX(), m->mStart.GetY());
        EEPhysicsLogAt(theRest, m->mEnd.GetX(), m->mEnd.GetY());
        break;
    }
    case kMessageBuildGate: {
        EEMBuildGate* m = static_cast<EEMBuildGate*>(inMessage);
        theVerb = "gate";
        EEPhysicsLogTech(theRest, thePlayer, m->mTechnology, theVerb);
        EEPhysicsLogAt(theRest, m->mPoint.GetX(), m->mPoint.GetY());
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
        EEPhysicsLogTarget(theRest, static_cast<EEMImproveUnit*>(inMessage)->mUnitID);
        break;
    case kMessagePackUnpack:
        theVerb = static_cast<EEMPackUnpack*>(inMessage)->mPack ? "pack" : "unpack";
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

// FUNCTION: EXE 0x55355c
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
        NEMPhysicsTick* theTick = static_cast<NEMPhysicsTick*>(theMessage);
        if (theTick->GetGenerateChecksum() == true) {
            if (gCommunications->IsServer() == true
                || (gCommunications->IsClient() == true && static_cast<EEClient*>(gCommunications)->mHasLeftGame == 0)) {
                bool theDump = theTick->GetDumpTheWorld();
                unsigned long theChecksum = this->GetOutOfSynchChecksum(theDump, 1);
                if (!theDump) {
                    unsigned long theTick2 = gCommunications->GetNumberOfCTsProcessedInPhysics();
                    NEMWorldDatabaseChecksumResponse* theResponse =
                        new NEMWorldDatabaseChecksumResponse(theTick2, theChecksum);
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
        this->mCurrentTick += theTick->GetPhysicsTicksToPerform();
        this->mTicksToProcess = theTick->GetPhysicsTicksToPerform();
        this->SetSpeed(1.0f / theTick->GetPhysicsTicksPerSecond());
        unsigned long theCount = theTick->GetNumberOfPlayerInputs();
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
                NEMRemovePlayer* theRemove = static_cast<NEMRemovePlayer*>(theInput);
                gCommunications->ExecuteRemovePlayer(theRemove->GetPlayerIndex(), theRemove->GetWasBooted(),
                                                      theRemove->GetPlayerIsOOS(), theRemove->GetGameIsExiting());
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

// FUNCTION: EXE 0x553811
// (the shared scalar deleting destructor of the engine messages with implicit destructors: NEMWorldDatabaseChecksumResponse's,
// emitted after its first user above)

// FUNCTION: EXE 0x553836
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
    gCommunications->ProcessLocalUserInput(new EEMReportStatistics(theFirst, theSecond), kSlotNone);
}

// FUNCTION: EXE 0x553917
// Creates a report-statistics message carrying the player's two statistics
EEMReportStatistics::EEMReportStatistics(long inL10, long inL14)
    : EEMessage(kMessageReportStatistics, false), mL10(inL10)
{
    this->mL14 = inL14;
}

// FUNCTION: EXE 0x55394b
// Creates the game timer
void EEPhysics::CreateGameTimer()
{
    EECreateGameTimer();
}

// FUNCTION: EXE 0x553950
// Writes the physics clocks, tick counters, timing averages, civ timer state, game-over flag and resource timer
// to the saved game; returns the first write error
long EEPhysics::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = RTSPhysics::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCurrentTick);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mF84);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTicksPerGameSecond);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickSeconds);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTimeOfDayStep);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickStartTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPreviousTickStartTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumTicksDone);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTicksToProcess);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickGapSum);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickGapCount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickWorkSum);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTickWorkCount);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGameClock);
    if (theResult == kErrorOK)
        theResult = outFile.Write((unsigned long)this->mAveragePhysicsUpdateTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write((unsigned long)this->mAverageTimeBetweenPhysicsTicks);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTimeOfDayRuns);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAreaEffectsClock);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivTimerRuns);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivTimerFirstWarning);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCivTimerSecondWarning);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGameOver);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mResourceTimerPending);
    return theResult;
}

// FUNCTION: EXE 0x553b8d
// Posts the autosave message (0x50a) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostAutoSave()
{
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x50a, NULL, NULL, true);
    return theResult;
}

// FUNCTION: EXE 0x553ba6
// Posts the show-scenario-dialog message (0x508) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostShowDialogMessage()
{
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x508, NULL, NULL, true);
    return theResult;
}

// FUNCTION: EXE 0x553bbf
// Posts the re-init message (0x509) to the physics thread; returns the post result (0 with no thread)
long EEPhysics::PostInit()
{
    long theResult = 0;
    if (this->mThread != NULL)
        theResult = this->mThread->Post(0x509, NULL, NULL, true);
    return theResult;
}

// FUNCTION: EXE 0x553bd8
// Recomputes the ticks per game second and the tick length (s and ms) from the tick rate and the game speed
void EEPhysics::UpdateTiming()
{
    this->mTicksPerGameSecond = this->mTicksPerSecond / gWorld.GetGameSpeed();
    this->mTickSeconds = 1.0f / this->mTicksPerGameSecond;
    this->mTickTime = this->mTickSeconds * 1000.0f;
    this->mTickTimeMs = UMFloatToLong(this->mTickTime);
}

// FUNCTION: EXE 0x553c23
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

// FUNCTION: EXE 0x553c4d
// Releases the world's write lock
void EEPhysics::Unlock()
{
    gWorld.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x553c59
// Advances the area effects clock and ticks the area effects once per 2 s of game time
void EEPhysics::UpdateAreaEffectsClock()
{
    this->mAreaEffectsClock += this->mTickTime;
    if (this->mAreaEffectsClock >= kAreaEffectInterval) {
        gAEManager.Tick(kAreaEffectInterval);
        this->mAreaEffectsClock -= kAreaEffectInterval;
    }
}

// FUNCTION: EXE 0x553ca1
// Runs the civilization timer on a server: warnings at 240 s and 270 s, expiry at 300 s, and the civ builder
// button blinks for a player with no civilization; elsewhere (or with no civ buying) the timer stops
void EEPhysics::UpdateCivTimer()
{
    if (gCommunications != NULL && gCommunications->IsServer()) {
        if (gWorld.mGameOptions.CanBuyCivilization()) {
            if (this->mTickWorkCount == 0 && !gWorld.GetCurrentPlayer()->mCivilizationChosen && !gWorld.GetCurrentPlayer()->mObserverMode)
                EESetCivBuilderButtonBlinking(true);
            EEMCivTimerExpired* theMessage;
            if (this->mGameTime > EEGetCivTimerLimit()) {
                this->mCivTimerRuns = false;
                theMessage = new EEMCivTimerExpired((unsigned char)0);
            } else if (this->mGameTime > EEGetCivTimerSecondWarning() && !this->mCivTimerSecondWarning) {
                this->mCivTimerSecondWarning = true;
                theMessage = new EEMCivTimerExpired((unsigned char)1);
            } else if (this->mGameTime > EEGetCivTimerFirstWarning() && !this->mCivTimerFirstWarning) {
                this->mCivTimerFirstWarning = true;
                theMessage = new EEMCivTimerExpired((unsigned char)1);
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

// FUNCTION: EXE 0x553dd2
// Creates a civ timer message: a warning (1) or the expiry (0)
EEMCivTimerExpired::EEMCivTimerExpired(unsigned char inIsWarning)
    : EEMessage(kMessageCivTimerExpired, false)
{
    this->mIsWarning = inIsWarning;
}

// FUNCTION: EXE 0x553dff
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
                theResult = static_cast<EEMLoadOrSaveGame*>(this->mLoadSaveRequests[i])->LoadOrSave();
                this->mLoadSaveRequests[i]->Dereference();
                this->mLoadSaveRequests[i] = NULL;
            }
            this->mLoadSaveRequests.clear();
            EEMLoadOrSaveGameResponse* theResponse = new EEMLoadOrSaveGameResponse(theResult);
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

// FUNCTION: EXE 0x553f47
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
        gCommunications->ProcessLocalUserInput(new EEMExitShiftMode(this->mGameTime), kSlotNone);
}

// FUNCTION: EXE 0x553ff1
// Creates an exit-shift-mode message stamped with the game time
EEMExitShiftMode::EEMExitShiftMode(long inGameTime)
    : EEMessage(kMessageExitShiftMode, false)
{
    this->mGameTime = inGameTime;
}

// FUNCTION: EXE 0x55401e
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

// FUNCTION: EXE 0x5540dc
// (EEComplexUnit::Step: the inline COMDAT after its first user above)

static const float kWindDrift = 0.005f;   // 0x442370 (unit-local const: the wind direction random walk step; provisional name)

// FUNCTION: EXE 0x5540fb
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

// FUNCTION: EXE 0x554215
// (vector<NEMessage*>::push_back: the template COMDAT)

// FUNCTION: EXE 0x554241
// (vector<EEPUnitSwap>::push_back: the template COMDAT)

// FUNCTION: EXE 0x55426d
// (deque<NEMessage*>::_Growmap: the template COMDAT)

// the unit's last global (0x531cd0, "default" 0x4b5210): its init chain follows the template COMDATs
// (EndGame: the campaign progress's user; not EEUIECampaignSelectScreenEvents.cpp's gCampaignDefaultUser 0x562ee0)
UString gCampaignUser("default");
// SYNTHETIC: EXE 0x55436d   (wrapper)
// SYNTHETIC: EXE 0x554377   (ctor)
// SYNTHETIC: EXE 0x554388   (registrar)
// SYNTHETIC: EXE 0x554394   (dtor)

// FUNCTION: EXE 0x55439f
// Steps the environment manager when it is enabled and its next step time has come
// not inline: defined here after the unit's init chain (UpdateWeather calls it)
void EEEnvironmentManager::Tick(unsigned long inTime)
{
    if (this->mNextStep <= inTime && this->mIsEnabled == true)
        this->Step(inTime);
}
