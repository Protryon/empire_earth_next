// C:\EEX\Empire Earth\Physics Thread\EEPhysics.cpp (__FILE__ 0x4c1e80). The physics thread: 0x151c28..0x15436d (the
// "EEPhysics Data" key's init chain, the class in definition order, the inline COMDATs of other classes after their
// first users). Unit flags: the exe's module set.
#include <windows.h>
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
// chain at 0x551c28 (wrapper)
//   object 0x931ce0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gPhysicsDataKey("EEPhysics Data");

// the unit's other data (provisional names): the area effects' update period (0x4422f8, a unit-local .rdata float)
// and the physics totals (0x531cf0: Init clears it)
static const float kAreaEffectInterval = 2000.0f;
long gU531CF0;
unsigned long gU531CF4;                     // 0x531cf4 (GetOutOfSynchChecksum: the last out-of-synch checksum)
static unsigned long sReportedFirst;        // 0x531cf8
static unsigned long sReportedSecond;       // 0x531cfc
static unsigned long sReportPeriod;         // 0x531d00
static bool sFirstReport = true;            // 0x4c1e34 (.data)
extern UString gCampaignUser;              // 0x531cd0 (defined at the end of this unit; provisional name)
// EESetCivBuilderButtonBlinking 0x27fcf0: ee/EmpireEarth.h (the main game form's blinking buttons)
unsigned long EEGetCivTimerLimit();         // 0x27ffae (cdecl: 300000; the civ timer limits, EEUIFMainGame.cpp's range)
unsigned long EEGetCivTimerSecondWarning();  // 0x27ffba (270000) (provisional names)
unsigned long EEGetCivTimerFirstWarning();  // 0x27ffb4 (240000)
extern unsigned long gContinentManagerChecksum;  // 0x530d10 (the continent manager's checksum; provisional name)
extern unsigned long gCollisionPeriod;      // 0x4c237c (.data = 1 after gCollisionDetectionUnit; provisional name)
// SYNTHETIC: EXE 0x551c28   (wrapper)
// SYNTHETIC: EXE 0x551c32   (ctor)
// SYNTHETIC: EXE 0x551c43   (registrar)
// SYNTHETIC: EXE 0x551c4f   (dtor)

// FUNCTION: EXE 0x551c5a
// Creates the physics simulation: makes its save-game lock, resets its clocks and starts the thread's main loop
EEPhysics::EEPhysics(EEGraphics* inGraphics, TMTimer* inTimer)
    : RTSPhysics(inGraphics, inTimer, 1, 0xf)
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
        sprintf(theName, "%s %4d.%02d.%02d - %02dh.%02dm.%02ds%s", "AutoSave", theTime.mYear, theTime.mMonth + 1,
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
        sprintf(theName, "OOS %4d.%02d.%02d - %02dh.%02dm.%02ds.log", theTime.mYear, theTime.mMonth + 1, theTime.mDay,
                theTime.mHour, theTime.mMinute, theTime.mSecond);
        theFile = fopen(theName, "w");
        ESVERIFY_AT(theFile != NULL, 544);
        for (i = 0; i < gWorld.mNumPlayers; i++) {
            thePlayer = gWorld.mPlayers[i];
            fprintf(theFile, "\n");
            fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Player Number: %d  *************\n",
                    thePlayer->mPlayerID);
            theGroup = thePlayer->mActiveGroup;
            if (theGroup != NULL) {
                fprintf(theFile, "\n");
                fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Selected Group Count: %d  *************\n",
                        theGroup->GetNumUnits());
                for (j = 0; j < theGroup->GetNumUnits(); j++) {
                    unsigned long theID = theGroup->GetUnit(j)->mUnitID;
                    fprintf(theFile, "OUT OF SYNCH DUMP: Unit #%d has an id of %d(%08x)\n", j, theID, theID);
                }
            }
            fprintf(theFile, "\n");
            fprintf(theFile, "OUT OF SYNCH DUMP:  **************  Active Group Count: %d  *************\n",
                    thePlayer->GetNumRunningGroups());
            for (j = 0; j < thePlayer->GetNumRunningGroups(); j++) {
                theActiveGroup = thePlayer->mRunningGroups[j];
                fprintf(theFile, "\n");
                fprintf(theFile, "OUT OF SYNCH DUMP:  Active Group #%d has a unit count of %d\n", j,
                        theActiveGroup->GetNumUnits());
                for (m = 0; m < theActiveGroup->GetNumUnits(); m++) {
                    unsigned long theID = theActiveGroup->GetUnit(m)->mUnitID;
                    fprintf(theFile, "OUT OF SYNCH DUMP: Unit #%d has an id of %d(%08x)\n", m, theID, theID);
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
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Overall Checksum: %ld *******\n", 0);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* this->mRandomNumberGenerator.GetSeed(): %ld *******\n",
                this->mRandom.GetSeed());
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of user-input messages processed: %ld *******\n",
                gU531CF0);
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of Messages in Server Queue: %d *******\n",
                gPhysics->mServerMessages.size());
        fprintf(theFile, "OUT OF SYNCH DUMP: ******* Number of CT Physics Ticks To Process: %d *******\n",
                gPhysics->mTicksToProcess);
        fclose(theFile);
        ESEndRecording();
    }
    for (theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext)
        theChecksum += static_cast<EEUnit*>(theUnit)->GetChecksum(inFlags);
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
    theChecksum += this->mCurrentTick + this->mRandom.GetSeed() + gU531CF0;
    if (inFlags & 4)
        theChecksum += gWorld.mMap->ComputeChecksum();
    if (inFlags & 0x100)
        theChecksum += gContinentManagerChecksum;
    if (inFlags & 0x10)
        theChecksum += EECalamityManager::GetInstance()->GetOutOfSynchChecksum();
    TMGetTime();
    gU531CF4 = theChecksum;
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
        sprintf(theString, "%03.2f %02d:%02d:%02d", gWorld.mGameSpeed, theHours, theMinutes, theSeconds);
    else
        sprintf(theString, "%03.2f %03d:%02d:%02d", gWorld.mGameSpeed, theHours, theMinutes, theSeconds);
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
    gU531CF0 = 0;
    this->SetSpeed(this->mSpeed);
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
                                this->mTickTimeMs = (long)this->mTickTime;
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
                                this->mTickTimeMs = (long)this->mTickTime;
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
        if (gWorld.mScenarioManager)
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
        this->mLD4 = reinterpret_cast<long>(inParameter1);
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
        long theResourceTime = gWorldTable.GetLongValue(0x5e) * 1000;
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
                gU531CF0++;
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
                dynamic_cast<EEMessage*>(theInput)->ExecuteSelectionMessage();
                break;
            default: {
                EEMessage* theGameMessage = dynamic_cast<EEMessage*>(theInput);
                if (theGameMessage != NULL)
                    theGameMessage->ExecuteMessage();
                else
                    theInput->Execute();
                break;
            }
            }
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
unsigned long EEPhysics::UpdateUnits()
{
    unsigned long theCount = gWorld.mUpdateUnits.size();
    unsigned long theNumPlayers = gWorld.mNumPlayers;
    gCollisionPeriod = UMCeilUnsigned((float)theCount / gCollisionDetectionUnit);
    if (gCollisionPeriod == 0)
        gCollisionPeriod = 1;
    for (unsigned long i = 0; i < theNumPlayers; i++) {
        if (gWorld.mPlayers[i] != NULL)
            gWorld.mPlayers[i]->UpdatePlayer();
    }
    for (long j = gWorld.mUpdateUnits.size() - 1; j >= 0; j--)
        gWorld.mUpdateUnits[j]->Step();
    return theCount;
}

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
    theTransformation.SetOrientationYPR(gWorld.mWindYaw, 0.0f, 0.0f);
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
