// TEST ONLY: the headless simulation's driver (EE-SIM.exe; platform/PlatSim.h). Port addition, no original address.
//
// EE-SIM.exe is the game (every unit of EE-AOC.exe) with another entry (EESimMain.cpp: main -> EESimMain). It runs the
// game's own start-up, menus, lobby, loading and simulation with the front end stubbed (platform/PlatSim.h: a hidden
// window, no rendering, no sound) and, instead of a player, this driver:
//   1. on a thread of its own, the menus as the harness drives them (tools/harness/session_exercise.py, ai_game.py):
//      main menu -> single player -> random map; the lobby's slots and options set by line name, absolutely, in the
//      harness's order (the same controls of form 26 and the same defaults); Start. The start seed is pinned
//      (EEMStartLoading::Execute takes gPlatSimSeed, as the harness's --seed pin);
//   2. when the game form is up and the physics timer runs: the idle human's civilization flags set as the harness's
//      civilization pre-empt leaves them (EEPlayer::mCivilizationChosen, mCivilizationLocked), so the civilization
//      timer opens no form; then gPlatSimRun lets the physics thread run (RTSPhysics::Main's simulation branch: one
//      communications tick of 14 physics ticks after the other, no wall clock);
//   3. after every communications tick, on the physics thread (gPlatSimAfterTick): the samples every --sample-seconds
//      of game time and the end of the run (--game-seconds, the game's own end, or with --no-human-defeat the last
//      computer standing, as ai_game.py);
//   4. the report (JSON) and an exit without the game's shutdown.
// EE_AI_TRACE=1: the EE-AI debug strings go to --trace (default stdout) as "debugstring EE-AI ..." lines, the
// harness's session-log form (tools/harness/ai_trace.py extract/compare/summary read the file as it is).
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/FSPath.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/UIComboBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIBitmap.h"
#include "lle/UIBackground.h"
#include "lle/NESynchronousServer.h"
#include "lle/FSRestorable.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEARemovePlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPEngine.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEUICPing.h"
#include "ee/EEUIELobbyEvents.h"
#include "ee/EEUIEPostGameSummaryEvents.h"
#include "ee/EEStartingResources.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIManager.h"
#include "ee/EEGamePreferences.h"
#include "ee/EERules.h"
#include "ee/EECPTrace.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "lle/USingleton.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"

namespace {

// ------------------------------------------------------------------------------------------------ options

struct Options {
    unsigned long mSeed = 2;
    long mComputers = 2;
    std::string mMapSize = "Small";
    std::string mMapType;
    std::string mDifficulty = "Hard";
    std::vector<std::pair<long, std::string>> mLobby;   // --lobby CONTROL=VALUE
    double mGameSeconds = 2700;
    double mSampleSeconds = 30;
    double mWallTimeout = 3600;
    long mPTRate = 40;
    bool mNoHumanDefeat = true;
    bool mHumanOut = true;                               // --keep-human: off
    std::string mReport;                                 // the JSON report ("-": stdout)
    std::string mTrace;                                  // the EE-AI lines ("" stdout)
    std::string mLog;                                    // every debug string
    std::vector<std::string> mSets;                      // --set doc.key=value
    std::string mCheats, mPersonality;
    std::string mEngine;                                 // --engine: the computer seats' engines ("" the lobby's: classic)
    long mPortOffset = -1;                              // -1: a free pair found at start
    long mAutoSaveMs = 0;                                // the game's autosave period (0: none; the game's default 600000)
    bool mQuiet = false;
    long mFlareMs = -1;                                  // --flare SECONDS:PLAYER (TEST ONLY): -1 none
    unsigned long mFlarePlayer = 0;
    long mVolcanoMs = -1;                                // --volcano SECONDS:PLAYER (TEST ONLY): -1 none
    unsigned long mVolcanoPlayer = 0;
    std::string mErr;                                    // --games: the game's stderr ("" the process's)
    std::string mID;                                     // --games: the game's name in the batch's markers
};

Options sOptions;
FILE* sTraceFile = NULL;
FILE* sLogFile = NULL;
std::mutex sOutLock;
std::chrono::steady_clock::time_point sStartWall;
std::chrono::steady_clock::time_point sGameWall;
// the wall time of each phase of the game's run (start-up to the main menu, menus, lobby, loading, game), in the report
std::vector<std::pair<std::string, double> > sPhases;
std::chrono::steady_clock::time_point sPhaseWall;

void Phase(const char* inName)
{
    auto theNow = std::chrono::steady_clock::now();
    sPhases.push_back({inName, std::chrono::duration<double>(theNow - sPhaseWall).count()});
    sPhaseWall = theNow;
}

// --games (several games in one process): the debug strings the start-up wrote before the first game (the AI
// manifest's line), written again at the head of every game's trace and log, so each holds what a run of its own
// would; between two games the debug strings go nowhere (a single run has quit by then)
FILE* sGamesFile = NULL;   // --games FILE or - (stdin): read a line at a time, when the last game has ended
Options sBase;              // the command line's options, under every game's own
bool sBatch = false;
enum { kOutPreamble, kOutGame, kOutBetween } sOutState = kOutPreamble;
std::string sPreambleTrace, sPreambleLog;

void Note(const char* inFormat, ...)
{
    if (sOptions.mQuiet)
        return;
    double theSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - sStartWall).count();
    char theText[1024];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    std::lock_guard<std::mutex> theGuard(sOutLock);
    fprintf(stderr, "[sim %7.1f] %s\n", theSeconds, theText);
    fflush(stderr);
}

// the game's debug strings: EE-AI lines to the trace, everything to --log
void DebugHandler(const char* inText)
{
    if (!inText)
        return;
    std::lock_guard<std::mutex> theGuard(sOutLock);
    if (sBatch && sOutState != kOutGame) {
        if (sOutState == kOutPreamble) {
            size_t theLength = strlen(inText);
            bool theNewLine = theLength == 0 || inText[theLength - 1] != '\n';
            if (!strncmp(inText, "EE-AI", 5))
                sPreambleTrace += std::string("debugstring ") + inText + (theNewLine ? "\n" : "");
            sPreambleLog += std::string(inText) + (theNewLine ? "\n" : "");
        }
        return;
    }
    if (!strncmp(inText, "EE-AI", 5)) {
        FILE* theOut = sTraceFile ? sTraceFile : stdout;
        fputs("debugstring ", theOut);
        fputs(inText, theOut);
        size_t theLength = strlen(inText);
        if (theLength == 0 || inText[theLength - 1] != '\n')
            fputc('\n', theOut);
    }
    if (sLogFile) {
        fputs(inText, sLogFile);
        size_t theLength = strlen(inText);
        if (theLength == 0 || inText[theLength - 1] != '\n')
            fputc('\n', sLogFile);
    }
}

[[noreturn]] void Quit(int inCode)
{
    {
        std::lock_guard<std::mutex> theGuard(sOutLock);
        fflush(stdout);
        fflush(stderr);
        if (sTraceFile)
            fflush(sTraceFile);
        if (sLogFile)
            fflush(sLogFile);
    }
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), (UINT)inCode);
#endif
    _exit(inCode);
}

// a failed run: the report with the error, then the exit (the game is built without exceptions)
[[noreturn]] void Fail(const std::string& inError);

std::string JsonString(const std::string& inText)
{
    std::string out = "\"";
    for (unsigned char c : inText) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += (char)c;
        } else if (c < 0x20 || c >= 0x7f) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else
            out += (char)c;
    }
    return out + "\"";
}

// ------------------------------------------------------------------------------------------------ the samples

struct PlayerSample {
    unsigned long mIndex = 0;
    bool mHuman = false;
    long mEpoch = -1;
    long mPopulation = 0;       // EEPlayer::mPopulationCount (the harness's units_lb14)
    long mVictoryUnits = 0;     // EEPlayer::mEndOfGameCount (victory_units_lb18)
    long mCitizens = 0;
    long mArmy = 0;             // live units that are neither citizens nor buildings (priests and heroes too)
    long mBuildings = 0;
    long mNaval = 0;            // of the army: warships, submarines and naval transports
    long mAir = 0;              // of the army: planes, helicopters and helicopter transports
    long mTowers = 0;           // of the buildings: the towers and space turrets
    long mWalls = 0;            // of the buildings: wall pieces and gates (EEUnitClass::mIsWall, not mIsTower: the towers are walls too)
    long mWonders = 0;          // EEPlayer::GetNumWonderUnits
    long mStrength = 0;         // the army's hit points x attack, summed (soldiers, ships, heroes)
    long mValue = 0;            // the cost of every live unit and building, all resources summed
    long mResources[5] = {0, 0, 0, 0, 0};
    unsigned long mResult = 0;  // EEPlayer::mGameResult: 1 victorious, 2 defeated
};

struct Sample {
    long mGameMs = 0;
    unsigned long mPT = 0;
    double mWall = 0;
    std::vector<PlayerSample> mPlayers;
};

struct Defeat {
    unsigned long mPlayer;
    long mGameMs;
    unsigned long mPT;
};

std::mutex sStateLock;
std::condition_variable sDone;
bool sFinished = false;
std::string sOutcome;
std::vector<Sample> sSamples;
std::vector<Defeat> sDefeats;
std::vector<unsigned long> sResults;   // per world player, the last result seen
std::vector<std::vector<long> > sEpochMs;   // per world player: the game ms each epoch was first reached (-1: not)
long sNextSampleMs = 0;
long sStandingSince = -1;              // game ms the last-computer-standing condition holds from
long sVolcanoCastMs = -1;              // --volcano: the cast's game ms, the second logged, the boulders seen
long sVolcanoSecond = 0;
unsigned long sVolcanoShots = 0, sVolcanoTotal = 0;
std::vector<unsigned long> sVolcanoSeen;

bool IsBuildingCategory(long inCategory)
{
    return inCategory == kUnitCategoryBuilding || inCategory == kUnitCategoryNavalBuilding
        || inCategory == kUnitCategoryMiningBuilding || inCategory == kUnitCategoryGate
        || inCategory == kUnitCategoryFortress || inCategory == kUnitCategoryBuildingPriest
        || inCategory == kUnitCategorySpaceBuilding;
}

// under the world lock (the physics thread's, or the driver's read lock)
Sample TakeSample()
{
    Sample s;
    s.mGameMs = gPhysics->mGameTime;
    s.mPT = gPhysics->mNumTicksDone;
    s.mWall = std::chrono::duration<double>(std::chrono::steady_clock::now() - sGameWall).count();
    for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(i);
        if (thePlayer == NULL)
            continue;
        PlayerSample p;
        p.mIndex = i;
        p.mHuman = (long)i == gWorld.mCurrentPlayer;
        p.mEpoch = thePlayer->mTechnologies != NULL ? thePlayer->mTechnologies->mCurrentEpoch : -1;
        p.mPopulation = thePlayer->mPopulationCount;
        p.mVictoryUnits = thePlayer->mEndOfGameCount;
        p.mResult = thePlayer->mGameResult;
        p.mWonders = (long)thePlayer->GetNumWonderUnits();
        for (long r = 0; r < 5; r++)
            p.mResources[r] = thePlayer->mResourceCaps[kResourceFood + r];
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0))
                continue;
            long theCategory = theUnit->mUnitClass->mCategory;
            if (theCategory == kUnitCategoryAmbient || theCategory == kUnitCategoryResource
                || theCategory == kUnitCategoryProjectile)
                continue;   // the AI's path points (ambient units), resources, shots
            if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
                p.mCitizens++;
            else if (IsBuildingCategory(theCategory)) {
                p.mBuildings++;
                unsigned long theFamily = theUnit->mUnitClass->GetFamily();
                if (theFamily == kUnitFamilyTowers || theFamily == kUnitFamilyTowerSpaceTurret)
                    p.mTowers++;
                if (theUnit->mUnitClass->mIsWall && !theUnit->mUnitClass->mIsTower)
                    p.mWalls++;
            } else {
                p.mArmy++;
                if (theCategory == kUnitCategoryNaval || theCategory == kUnitCategorySubmergedNaval
                    || theCategory == kUnitCategoryNavalTransport)
                    p.mNaval++;
                else if (theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryHelicopter
                    || theCategory == kUnitCategoryHelicopterTransport)
                    p.mAir++;
                if (theUnit->mStats != NULL)
                    p.mStrength += (long)(theUnit->mHitPoints * theUnit->mStats->mAttack);
            }
            EETechTreeNode* theNode = thePlayer->mTechnologies != NULL
                ? thePlayer->mTechnologies->Get(theUnit->mUnitClass->mTechnology) : NULL;
            if (theNode != NULL && theNode->mData != NULL)
                for (long r = kResourceFood; r <= kResourceIron; r++)
                    p.mValue += theNode->GetCost(r, theUnit->mUnitClass->mUnitType, 1.0f);
        }
        s.mPlayers.push_back(p);
    }
    return s;
}

void Finish(const char* inOutcome)
{
    gPlatSimRun = 0;
    std::lock_guard<std::mutex> theGuard(sStateLock);
    if (sFinished)
        return;
    sOutcome = inOutcome;
    sFinished = true;
    sDone.notify_all();
}

// the physics thread, after each communications tick, under the world's write lock
void AfterTick()
{
    long theTime = gPhysics->mGameTime;
    unsigned long theCount = gWorld.GetNumPlayers();
    if (sResults.size() < theCount)
        sResults.resize(theCount, 0);
    for (unsigned long i = 1; i < theCount; i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(i);
        unsigned long theResult = thePlayer ? thePlayer->mGameResult : 0;
        long theEpoch = thePlayer && thePlayer->mTechnologies ? thePlayer->mTechnologies->mCurrentEpoch : -1;
        if (theEpoch >= 0) {
            std::lock_guard<std::mutex> theGuard(sStateLock);
            if (sEpochMs.size() < theCount)
                sEpochMs.resize(theCount);
            std::vector<long>& theTimes = sEpochMs[i];
            if ((long)theTimes.size() <= theEpoch)
                theTimes.resize(theEpoch + 1, -1);
            for (long e = 0; e <= theEpoch; e++)   // an advance reaches every epoch up to it (a skip counts for each)
                if (theTimes[e] < 0)
                    theTimes[e] = theTime;
        }
        if (theResult != sResults[i]) {
            sResults[i] = theResult;
            if (theResult == 2)
                sDefeats.push_back(Defeat{i, theTime, gPhysics->mNumTicksDone});
            Note("RESULT player %lu %s at game ms %ld PT %lu", i,
                 theResult == 1 ? "victorious" : theResult == 2 ? "defeated" : "?", theTime, gPhysics->mNumTicksDone);
        }
    }
    if (theTime >= sNextSampleMs) {
        Sample s = TakeSample();
        {
            std::lock_guard<std::mutex> theGuard(sStateLock);
            sSamples.push_back(s);
        }
        sNextSampleMs += (long)(sOptions.mSampleSeconds * 1000);
        if (!sOptions.mQuiet) {
            std::string theLine;
            for (const PlayerSample& p : s.mPlayers) {
                char buf[160];
                snprintf(buf, sizeof(buf), " p%lu e%ld c%ld a%ld b%ld", p.mIndex, p.mEpoch, p.mCitizens, p.mArmy,
                         p.mBuildings);
                theLine += buf;
            }
            Note("game s %ld PT %lu:%s", theTime / 1000, s.mPT, theLine.c_str());
        }
    }
    // --flare (TEST ONLY): once, the idle human's flare at the player's first building, told to the human's allies as
    // EEMSendFlare::Execute tells them (EEPlayer::FlareReceived): the AIs' answer to an ally's flare, which no
    // computer sends
    if (sOptions.mFlareMs >= 0 && theTime >= sOptions.mFlareMs) {
        sOptions.mFlareMs = -1;
        EEPlayer* theHuman = gWorld.GetCurrentPlayer();
        EEPlayer* theTarget = sOptions.mFlarePlayer < theCount ? gWorld.GetPlayer(sOptions.mFlarePlayer) : NULL;
        EEUnit* theSpot = NULL;
        for (EEUnit* u = theTarget != NULL ? theTarget->mFirstUnit : NULL; u != NULL && theSpot == NULL;
             u = static_cast<EEUnit*>(u->mNext))
            if (u->mComplexUnit != NULL && u->mHitPoints > 0 && u->mUnitClass->IsBuilding())
                theSpot = u;
        if (theHuman != NULL && theSpot != NULL) {
            U2DPoint<long> theTile(theSpot->GetTileX(), theSpot->GetTileY());
            std::vector<EEPlayer*> theAllies = theHuman->mAllies;
            for (EEPlayer* a : theAllies)
                a->FlareReceived(theHuman, theTile);
            Note("FLARE at %ld,%ld (player %lu) to %lu allies at game ms %ld", theTile.mX, theTile.mY,
                 sOptions.mFlarePlayer, (unsigned long)theAllies.size(), theTime);
        } else
            Note("FLARE: no building of player %lu", sOptions.mFlarePlayer);
    }
    // --volcano (TEST ONLY): once, a volcano (the calamity class 9, no caster) cast 6 tiles off the player's first
    // building for that player; then every game second the volcano projectiles (boulders) launched in it
    {
        if (sOptions.mVolcanoMs >= 0 && theTime >= sOptions.mVolcanoMs) {
            sOptions.mVolcanoMs = -1;
            EEPlayer* theTarget = sOptions.mVolcanoPlayer < theCount ? gWorld.GetPlayer(sOptions.mVolcanoPlayer) : NULL;
            EEUnit* theSpot = NULL;
            for (EEUnit* u = theTarget != NULL ? theTarget->mFirstUnit : NULL; u != NULL && theSpot == NULL;
                 u = static_cast<EEUnit*>(u->mNext))
                if (u->mComplexUnit != NULL && u->mHitPoints > 0 && u->mUnitClass->IsBuilding())
                    theSpot = u;
            if (theSpot != NULL) {
                long theX = theSpot->GetTileX() + 6, theY = theSpot->GetTileY() + 6;
                bool theCast = USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity(
                    9, 0xffffffffUL, theX, theY, sOptions.mVolcanoPlayer);
                Note("VOLCANO cast %d at %ld,%ld (player %lu) at game ms %ld", theCast ? 1 : 0, theX, theY,
                     sOptions.mVolcanoPlayer, theTime);
                sVolcanoCastMs = theTime;
                sVolcanoSecond = theTime / 1000;
            } else
                Note("VOLCANO: no building of player %lu", sOptions.mVolcanoPlayer);
        }
        if (sVolcanoCastMs >= 0) {
            for (unsigned long i = 0; i < theCount; i++) {
                EEPlayer* thePlayer = gWorld.GetPlayer(i);
                for (EEUnit* u = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; u != NULL;
                     u = static_cast<EEUnit*>(u->mNext))
                    if (u->mUnitClass->GetUnitType() == kUnitTypeVolcanoProjectile &&
                        std::find(sVolcanoSeen.begin(), sVolcanoSeen.end(), u->GetUnitID()) == sVolcanoSeen.end()) {
                        sVolcanoSeen.push_back(u->GetUnitID());
                        sVolcanoShots++;
                        sVolcanoTotal++;
                    }
            }
            if (theTime / 1000 > sVolcanoSecond) {
                Note("VOLCANO s %ld: %lu boulders launched (total %lu)", sVolcanoSecond - sVolcanoCastMs / 1000,
                     sVolcanoShots, sVolcanoTotal);
                sVolcanoSecond = theTime / 1000;
                sVolcanoShots = 0;
                if (sVolcanoSeen.size() > 100000)
                    sVolcanoSeen.clear();
            }
        }
    }
    // the end of the run (tools/harness/ai_game.py's rules)
    if (gPhysics->mGameOver) {
        Finish("game-end");
        return;
    }
    if (sOptions.mNoHumanDefeat) {
        long theAlive = 0;
        bool theOthersDone = true;
        for (unsigned long i = 1; i < theCount; i++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(i);
            if (thePlayer == NULL || (long)i == gWorld.mCurrentPlayer)
                continue;
            if (thePlayer->mEndOfGameCount > 0)
                theAlive++;
            else if (thePlayer->mGameResult == 0)
                theOthersDone = false;
        }
        if (theAlive <= 1 && theOthersDone) {
            if (sStandingSince < 0)
                sStandingSince = theTime;
            else if (theTime - sStandingSince >= 1000) {   // two harness samples apart
                Finish("last-computer-standing");
                return;
            }
        } else
            sStandingSince = -1;
    } else {
        bool theAll = theCount > 1;
        for (unsigned long i = 1; i < theCount; i++) {
            EEPlayer* thePlayer = gWorld.GetPlayer(i);
            if (thePlayer && thePlayer->mGameResult == 0)
                theAll = false;
        }
        if (theAll) {
            Finish("game-end");
            return;
        }
    }
    if (theTime >= (long)(sOptions.mGameSeconds * 1000))
        Finish("time-cap");
}

// ------------------------------------------------------------------------------------------------ the menus

const unsigned long kTriggerEvent = 0x3f0;
const unsigned long kFormMain = 12, kFormSingle = 6, kFormLobbyID = 26, kFormGame = 11;
const long kPlayerType = 30, kTeam = 62, kCitizens = 126;
const long kEngine = 275;   // the port's engine boxes (ee/EEUIManager.h kLobbyEngineCombo + slot)
const long kVisibleSlots = 8;

struct UILock {
    UILock() { gUIManager->LockUIForCopyData(); }
    ~UILock() { gUIManager->UnlockUIForCopyData(); }
};

unsigned long TopForm()
{
    if (gUIManager == NULL)
        return 0;
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    return theForm ? theForm->GetFormID() : 0;
}

void SimSleep(long inMilliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(inMilliseconds));
}

void WaitFor(const char* inWhat, double inSeconds, const std::function<bool()>& inTest)
{
    auto theEnd = std::chrono::steady_clock::now() + std::chrono::duration<double>(inSeconds);
    while (!inTest()) {
        if (std::chrono::steady_clock::now() > theEnd)
            Fail(std::string("timed out waiting for ") + inWhat + " (top form " +
                                     std::to_string(TopForm()) + ")");
        SimSleep(50);
    }
}

void WaitForm(unsigned long inForm, double inSeconds = 120)
{
    WaitFor(("form " + std::to_string(inForm)).c_str(), inSeconds, [&] { return TopForm() == inForm; });
}

void Click(unsigned long inForm, unsigned long inControl)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != inForm || inControl >= theForm->GetControlCount())
        Fail("click: form " + std::to_string(inForm) + " control " + std::to_string(inControl) +
                                 " is not on top");
    UIControl* theControl = theForm->GetControl(inControl);
    if (!theControl || !theControl->GetTriggerEvent(kTriggerEvent))
        Fail("click: control " + std::to_string(inControl) + " has no trigger event");
    theControl->FireTriggerEvent(kTriggerEvent);
}

UIComboBox* Combo(UIForm* inForm, long inControl)
{
    if (!inForm || inControl < 0 || (unsigned long)inControl >= inForm->GetControlCount())
        return NULL;
    return dynamic_cast<UIComboBox*>(inForm->GetControl(inControl));
}

// the combo's lines and selection (on the lobby form) -> false when the control is not a combo there
bool ComboState(long inControl, std::vector<std::string>& outLines, long& outSelection)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != kFormLobbyID)
        return false;
    UIComboBox* theCombo = Combo(theForm, inControl);
    if (!theCombo || !theCombo->GetListBox())
        return false;
    outLines.clear();
    const UILBString* theList = theCombo->GetListBox();
    for (unsigned long k = 0; k < theList->GetLineCount(); k++) {
        const char* theText = (const char*)theList->GetData(k);
        outLines.push_back(theText ? theText : "");
    }
    unsigned long theSelection = 0xffffffffUL;
    theCombo->GetCurrentSelection(theSelection);
    outSelection = (long)(int32_t)theSelection;
    return true;
}

std::string Lower(std::string inText)
{
    size_t a = inText.find_first_not_of(" \t");
    size_t b = inText.find_last_not_of(" \t");
    inText = a == std::string::npos ? std::string() : inText.substr(a, b - a + 1);
    for (char& c : inText)
        c = (char)tolower((unsigned char)c);
    return inText;
}

// a line by name (ai_game.py resolve): "#k" an index, else the exact line, else the one line it starts, else the
// one line containing it
long Resolve(const std::string& inValue, const std::vector<std::string>& inLines, const std::string& inWhat)
{
    if (!inValue.empty() && inValue[0] == '#') {
        long k = atol(inValue.c_str() + 1);
        if (k < 0 || k >= (long)inLines.size())
            Fail(inWhat + ": index " + inValue + " outside the combo's " +
                                     std::to_string(inLines.size()) + " lines");
        return k;
    }
    std::string theKey = Lower(inValue);
    std::vector<long> theHits;
    for (size_t k = 0; k < inLines.size(); k++)
        if (Lower(inLines[k]) == theKey)
            return (long)k;
    for (size_t k = 0; k < inLines.size(); k++)
        if (Lower(inLines[k]).compare(0, theKey.size(), theKey) == 0)
            theHits.push_back((long)k);
    if (theHits.empty())
        for (size_t k = 0; k < inLines.size(); k++)
            if (Lower(inLines[k]).find(theKey) != std::string::npos)
                theHits.push_back((long)k);
    if (theHits.size() != 1) {
        std::string theAll;
        for (const std::string& l : inLines)
            theAll += (theAll.empty() ? "" : "|") + l;
        Fail(inWhat + ": '" + inValue + "' matches " + std::to_string(theHits.size()) +
                                 " of the lines " + theAll);
    }
    return theHits[0];
}

struct Chosen {
    long mControl;
    std::string mLabel, mValue, mLine;
    long mIndex;
};
std::vector<Chosen> sChosen;

// the lobby's updates settled: every update posted to the UI thread applied and none new for inQuietMs (with inSince
// >= 0 first a post after that count: the change's own round trip through the local server) -> false after
// inMaxSeconds. Replaces fixed waits (200 ms a change, 1.5 s and 1 s around them): a lobby of ~10 changes took 4-5 s
std::chrono::steady_clock::time_point sLastChange;

bool LobbySettle(long inSince, long inQuietMs, double inMaxSeconds)
{
    auto theStart = std::chrono::steady_clock::now();
    auto theStable = theStart;
    long theLast = PlatSimLobbyPosts();
    while (true) {
        long thePosts = PlatSimLobbyPosts(), theApplies = PlatSimLobbyApplies();
        auto theNow = std::chrono::steady_clock::now();
        if (thePosts != theLast) {
            theLast = thePosts;
            theStable = theNow;
        }
        if ((inSince < 0 || thePosts > inSince) && theApplies >= thePosts &&
            std::chrono::duration<double>(theNow - theStable).count() * 1000 >= inQuietMs)
            return true;
        if (std::chrono::duration<double>(theNow - theStart).count() > inMaxSeconds)
            return false;
        SimSleep(2);
    }
}

void Select(long inControl, const std::string& inValue, const std::string& inLabel)
{
    std::vector<std::string> theLines;
    long theSelection = -1;
    if (!ComboState(inControl, theLines, theSelection))
        Fail(inLabel + ": control " + std::to_string(inControl) + " is not a combo on the lobby");
    long theIndex = Resolve(inValue, theLines, inLabel);
    if (theSelection != theIndex) {
        // (the client drops a player's inputs past 10 a second, NESynchronousClient::ProcessUserInputMessage: one
        // change every 110 ms at most)
        double theSince = std::chrono::duration<double>(std::chrono::steady_clock::now() - sLastChange).count();
        if (theSince < 0.11)
            SimSleep((long)((0.11 - theSince) * 1000) + 1);
        long thePosts = PlatSimLobbyPosts();
        sLastChange = std::chrono::steady_clock::now();
        {
            UILock theLock;
            UIComboBox* theCombo = Combo(gUIManager->GetCurrentForm(), inControl);
            if (!theCombo)
                Fail(inLabel + ": the lobby went away");
            theCombo->SetCurrentSelection((unsigned long)theIndex);
        }
        WaitFor((inLabel + " to stick").c_str(), 20, [&] {
            std::vector<std::string> l;
            long s = -1;
            return ComboState(inControl, l, s) && s == theIndex;
        });
        if (!LobbySettle(thePosts, 20, 2.0))   // the lobby's update round trip through the local server
            Note("LOBBY %s: no update came back in 2 s", inLabel.c_str());
    }
    sChosen.push_back(Chosen{inControl, inLabel, inValue, theLines[theIndex], theIndex});
    Note("LOBBY %s: control %ld = %ld '%s'", inLabel.c_str(), inControl, theIndex, theLines[theIndex].c_str());
}

void SetUpLobby()
{
    long n = sOptions.mComputers;
    WaitFor("the lobby's combos", 60, [&] {
        std::vector<std::string> l;
        long s;
        for (long k = 0; k < kVisibleSlots; k++)
            if (!ComboState(kPlayerType + k, l, s))
                return false;
        return ComboState(177, l, s) && ComboState(179, l, s) && ComboState(207, l, s);
    });
    SimSleep(300);   // the local server's readiness and queued lobby updates (the harness waits 3 s)
    LobbySettle(-1, 250, 5.0);
    // (control, value, label), applied in order, absolutely (ai_game.py)
    std::vector<std::pair<long, std::string>> theOptions = {
        {175, "Random Map"}, {177, "Small"}, {179, "Continental"}, {181, "Standard - High"},
        {183, "Prehistoric Age"}, {185, "Space Age"}, {187, "1200"}, {189, "Off"}, {205, "Standard"},
        {207, "Easy"}, {209, "Very Fast"}};
    auto set = [&](long inControl, const std::string& inValue) {
        for (auto& o : theOptions)
            if (o.first == inControl) {
                o.second = inValue;
                return;
            }
        theOptions.push_back({inControl, inValue});
    };
    if (!sOptions.mMapSize.empty())
        set(177, sOptions.mMapSize);
    if (!sOptions.mMapType.empty())
        set(179, sOptions.mMapType);
    if (!sOptions.mDifficulty.empty())
        set(207, sOptions.mDifficulty);
    for (auto& l : sOptions.mLobby)
        set(l.first, l.second);
    for (long s = 0; s <= n; s++) {   // the playing slots: no team (free for all), 5 starting citizens
        bool theTeam = false, theCitizens = false;
        for (auto& o : theOptions) {
            theTeam = theTeam || o.first == kTeam + s;
            theCitizens = theCitizens || o.first == kCitizens + s;
        }
        if (!theTeam)
            theOptions.push_back({kTeam + s, "-"});
        if (!theCitizens)
            theOptions.push_back({kCitizens + s, "5"});
    }
    // --engine: each computer seat's engine box (seat s = slot s; a shorter list is taken in turn), unless --lobby
    // sets that box itself; without --engine the boxes keep the lobby's classic
    std::vector<std::string> theEngines;
    for (size_t a = 0; !sOptions.mEngine.empty() && a <= sOptions.mEngine.size();) {
        size_t b = sOptions.mEngine.find(',', a);
        if (b == std::string::npos)
            b = sOptions.mEngine.size();
        theEngines.push_back(sOptions.mEngine.substr(a, b - a));
        a = b + 1;
    }
    for (long s = 1; s <= n && !theEngines.empty(); s++) {
        bool theSet = false;
        for (auto& o : theOptions)
            theSet = theSet || o.first == kEngine + s;
        if (theSet)
            continue;
        // the box's line by the choice's name (ee/EECPEngine.h: classic, random, an AI's id, or next for the
        // manifest's first AI), not by its text: the manifest's names are free text
        const std::string& theName = theEngines[(size_t)(s - 1) % theEngines.size()];
        long theChoice = EECPChoiceByName(theName);
        if (theChoice < 0) {
            std::string theKnown = "classic, random";
            for (const EECPAIEntry& e : EECPAIManifest())
                theKnown += ", " + e.mID;
            Fail("--engine: '" + theName + "' is no engine (" + theKnown + (EECPAIManifest().empty() ? "" : ", next") + ")");
        }
        theOptions.push_back({kEngine + s, "#" + std::to_string(theChoice)});
    }
    Select(kPlayerType, "#0", "slot 0 human");
    for (long s = 1; s < kVisibleSlots; s++)
        Select(kPlayerType + s, s <= n ? "Computer" : "Open", "slot " + std::to_string(s));
    for (auto& o : theOptions)
        Select(o.first, o.second, "control " + std::to_string(o.first));
    LobbySettle(-1, 100, 3.0);
    for (const Chosen& c : sChosen) {
        std::vector<std::string> l;
        long s = -1;
        if (!ComboState(c.mControl, l, s) || s != c.mIndex)
            Fail("lobby selection of control " + std::to_string(c.mControl) +
                                     " changed after setting it");
    }
}

// ------------------------------------------------------------------------------------------------ the report

void WriteReport(int inCode, const std::string& inError)
{
    std::string j = "{\n";
    j += "  \"test_only\": true,\n  \"kind\": \"ee-sim-v1\",\n";
    j += "  \"setup\": {\"seed\": " + std::to_string(sOptions.mSeed) + ", \"computers\": " +
         std::to_string(sOptions.mComputers) + ", \"game_seconds\": " + std::to_string((long)sOptions.mGameSeconds) +
         ", \"pt_rate\": " + std::to_string(sOptions.mPTRate) + ", \"no_human_defeat\": " +
         (sOptions.mNoHumanDefeat ? "true" : "false") + ", \"human_out\": " +
         (sOptions.mHumanOut ? "true" : "false") + ", \"cheats\": " +
         JsonString(sOptions.mCheats.empty() ? "all" : sOptions.mCheats) + ", \"personality\": " +
         JsonString(sOptions.mPersonality) + ", \"engine\": " +
         JsonString(sOptions.mEngine.empty() ? "classic" : sOptions.mEngine) + ", \"sets\": [";
    for (size_t k = 0; k < sOptions.mSets.size(); k++)
        j += (k ? ", " : "") + JsonString(sOptions.mSets[k]);
    j += "],\n    \"lobby\": [";
    for (size_t k = 0; k < sChosen.size(); k++)
        j += std::string(k ? ", " : "") + "{\"control\": " + std::to_string(sChosen[k].mControl) + ", \"value\": " +
             JsonString(sChosen[k].mValue) + ", \"index\": " + std::to_string(sChosen[k].mIndex) + ", \"line\": " +
             JsonString(sChosen[k].mLine) + "}";
    j += "]},\n";
    std::lock_guard<std::mutex> theGuard(sStateLock);
    j += "  \"outcome\": " + JsonString(inError.empty() ? sOutcome : "error") + ",\n";
    if (!inError.empty())
        j += "  \"error\": " + JsonString(inError) + ",\n";
    double theWall = std::chrono::duration<double>(std::chrono::steady_clock::now() - sStartWall).count();
    double theGame = std::chrono::duration<double>(std::chrono::steady_clock::now() - sGameWall).count();
    char buf[256];
    snprintf(buf, sizeof(buf), "  \"wall_seconds\": %.2f,\n  \"wall_seconds_game\": %.2f,\n", theWall,
             sGameWall.time_since_epoch().count() ? theGame : 0.0);
    j += buf;
    j += "  \"wall_phases\": {";
    for (size_t k = 0; k < sPhases.size(); k++) {
        snprintf(buf, sizeof(buf), "%s\"%s\": %.2f", k ? ", " : "", sPhases[k].first.c_str(), sPhases[k].second);
        j += buf;
    }
    j += "},\n";
    const Sample* theLast = sSamples.empty() ? NULL : &sSamples.back();
    j += "  \"game_ms\": " + std::to_string(theLast ? theLast->mGameMs : 0) + ",\n";
    j += "  \"pt\": " + std::to_string(theLast ? theLast->mPT : 0) + ",\n";
    j += "  \"no_human_defeat_hits\": " + std::to_string((long)gPlatSimNoHumanDefeatHits) + ",\n";
    std::vector<unsigned long> theAlive;
    std::vector<unsigned long> theWinners;
    if (theLast)
        for (const PlayerSample& p : theLast->mPlayers) {
            if (!p.mHuman && p.mVictoryUnits > 0)
                theAlive.push_back(p.mIndex);
            if (p.mResult == 1)
                theWinners.push_back(p.mIndex);
        }
    auto list = [](const std::vector<unsigned long>& v) {
        std::string s = "[";
        for (size_t k = 0; k < v.size(); k++)
            s += (k ? ", " : "") + std::to_string(v[k]);
        return s + "]";
    };
    j += "  \"alive_computers\": " + list(theAlive) + ",\n  \"winners\": " + list(theWinners) + ",\n";
    j += "  \"winner\": " + std::string(sOutcome == "last-computer-standing" && theAlive.size() == 1
                                             ? std::to_string(theAlive[0]) : "null") + ",\n";
    j += "  \"defeat_order\": [";
    for (size_t k = 0; k < sDefeats.size(); k++)
        j += std::string(k ? ", " : "") + "{\"player\": " + std::to_string(sDefeats[k].mPlayer) + ", \"game_ms\": " +
             std::to_string(sDefeats[k].mGameMs) + ", \"pt\": " + std::to_string(sDefeats[k].mPT) + "}";
    j += "],\n";
    auto players = [](const Sample& s) {
        std::string o = "[";
        for (size_t k = 0; k < s.mPlayers.size(); k++) {
            const PlayerSample& p = s.mPlayers[k];
            char b[768];
            snprintf(b, sizeof(b),
                     "%s{\"index\": %lu, \"human\": %s, \"result\": %lu, \"epoch\": %ld, \"population\": %ld, "
                     "\"victory_units\": %ld, \"citizens\": %ld, \"army\": %ld, \"buildings\": %ld, "
                     "\"naval\": %ld, \"air\": %ld, \"towers\": %ld, \"walls\": %ld, \"wonders\": %ld, "
                     "\"strength\": %ld, \"value\": %ld, \"resources\": {\"food\": %ld, \"wood\": %ld, \"stone\": %ld, \"gold\": %ld, \"iron\": %ld}}",
                     k ? ", " : "", p.mIndex, p.mHuman ? "true" : "false", p.mResult, p.mEpoch, p.mPopulation,
                     p.mVictoryUnits, p.mCitizens, p.mArmy, p.mBuildings,
                     p.mNaval, p.mAir, p.mTowers, p.mWalls, p.mWonders, p.mStrength, p.mValue, p.mResources[0], p.mResources[1],
                     p.mResources[2], p.mResources[3], p.mResources[4]);
            o += b;
        }
        return o + "]";
    };
    j += "  \"players\": " + (theLast ? players(*theLast) : std::string("[]")) + ",\n";
    // per world player, the game ms each epoch was first reached (index: the epoch; the start epoch and those below
    // it: the first tick's; an epoch never reached is missing from the end)
    j += "  \"epoch_ms\": {";
    bool theFirst = true;
    for (size_t i = 1; i < sEpochMs.size(); i++) {
        if (sEpochMs[i].empty())
            continue;
        j += std::string(theFirst ? "" : ", ") + "\"" + std::to_string(i) + "\": [";
        theFirst = false;
        for (size_t e = 0; e < sEpochMs[i].size(); e++)
            j += (e ? ", " : "") + std::to_string(sEpochMs[i][e]);
        j += "]";
    }
    j += "},\n";
    j += "  \"samples\": [\n";
    for (size_t k = 0; k < sSamples.size(); k++) {
        snprintf(buf, sizeof(buf), "    {\"game_ms\": %ld, \"pt\": %lu, \"wall\": %.2f, \"players\": ", sSamples[k].mGameMs,
                 sSamples[k].mPT, sSamples[k].mWall);
        j += buf + players(sSamples[k]) + (k + 1 < sSamples.size() ? "},\n" : "}\n");
    }
    j += "  ],\n  \"exit_code\": " + std::to_string(inCode) + "\n}\n";
    if (sOptions.mReport.empty() || sOptions.mReport == "-") {
        std::lock_guard<std::mutex> theOut(sOutLock);
        fputs(j.c_str(), stdout);
        fflush(stdout);
    } else {
        std::string theTemporary = sOptions.mReport + ".tmp";
        FILE* f = fopen(theTemporary.c_str(), "wb");
        if (f) {
            fwrite(j.data(), 1, j.size(), f);
            fclose(f);
            remove(sOptions.mReport.c_str());
            rename(theTemporary.c_str(), sOptions.mReport.c_str());
        }
    }
}

void EndGameOutput(int inCode);

void Fail(const std::string& inError)
{
    gPlatSimRun = 0;
    Note("FAILED: %s", inError.c_str());
    WriteReport(2, inError);
    EndGameOutput(2);
    Quit(2);   // (in a batch too: the process's state is unknown; tools/sim.py plays the rest in a new one)
}

// ------------------------------------------------------------------------------------------------ the driver

bool sLeftGame = false;    // --games: the last game was left (LeaveGame), this one's first phase is the leave
int sProcessStderr = -1;   // --games: the process's stderr while a game's own (--err) is in place
size_t sGameIndex = 0;

// a line on the process's stderr (the batch's markers), whatever the game's stderr is
void Marker(const char* inFormat, ...)
{
    char theText[1024];
    va_list theArguments;
    va_start(theArguments, inFormat);
    int n = vsnprintf(theText, sizeof(theText) - 1, inFormat, theArguments);
    va_end(theArguments);
    if (n < 0)
        return;
    if (n > (int)sizeof(theText) - 2)
        n = (int)sizeof(theText) - 2;
    theText[n++] = '\n';
    std::lock_guard<std::mutex> theGuard(sOutLock);
    fflush(stderr);
    if (sProcessStderr >= 0) {
#ifdef _WIN32
        _write(sProcessStderr, theText, (unsigned)n);
#else
        if (write(sProcessStderr, theText, (size_t)n) < 0)
            return;
#endif
        return;
    }
    fwrite(theText, 1, (size_t)n, stderr);
    fflush(stderr);
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

// the driver's state of one game, cleared before each game of a batch
void ResetRun()
{
    {
        std::lock_guard<std::mutex> theGuard(sStateLock);
        sFinished = false;
        sOutcome.clear();
        sSamples.clear();
        sDefeats.clear();
        sResults.clear();
        sEpochMs.clear();
        sNextSampleMs = 0;
        sStandingSince = -1;
        sVolcanoCastMs = -1;
        sVolcanoSecond = 0;
        sVolcanoShots = sVolcanoTotal = 0;
        sVolcanoSeen.clear();
    }
    sChosen.clear();
    gPlatSimNoHumanDefeatHits = 0;
    gPlatSimAfterTick = NULL;
    sGameWall = std::chrono::steady_clock::time_point();
}

// --games: the next game's options, files and stderr in place (the physics thread holds between games)
void BeginGame(size_t inIndex, const Options& inGame, bool inAfterGame)
{
    sGameIndex = inIndex;
    sOptions = inGame;
    ResetRun();
    gPlatSimSeed = (uint32_t)sOptions.mSeed;
    gPlatSimNoHumanDefeat = sOptions.mNoHumanDefeat ? 1 : 0;
    if (inAfterGame) {   // (the first game's wall time and phases count from the process's start)
        sStartWall = sPhaseWall = std::chrono::steady_clock::now();
        sPhases.clear();
    }
    Marker("EE-SIM GAME %lu BEGIN %s", (unsigned long)inIndex, sOptions.mID.c_str());
    std::lock_guard<std::mutex> theGuard(sOutLock);
    if (!sOptions.mErr.empty()) {
        FILE* theErr = fopen(sOptions.mErr.c_str(), "w");
        if (theErr != NULL) {
#ifdef _WIN32
            SetStderr(_fileno(theErr));
#else
            SetStderr(fileno(theErr));
#endif
            fclose(theErr);
        }
    }
}

// --games: the game's trace and log from here (after the last game was left), headed by the start-up's lines
void StartGameOutput()
{
    std::lock_guard<std::mutex> theGuard(sOutLock);
    if (!sOptions.mTrace.empty())
        sTraceFile = fopen(sOptions.mTrace.c_str(), "w");
    if (!sOptions.mLog.empty())
        sLogFile = fopen(sOptions.mLog.c_str(), "w");
    fputs(sPreambleTrace.c_str(), sTraceFile ? sTraceFile : stdout);
    if (sLogFile)
        fputs(sPreambleLog.c_str(), sLogFile);
    sOutState = kOutGame;
}

// --games: the game's files closed and the process's stderr back, then the end marker (exit code and outcome)
void EndGameOutput(int inCode)
{
    if (!sBatch)
        return;
    {
        std::lock_guard<std::mutex> theGuard(sOutLock);
        sOutState = kOutBetween;
        fflush(stdout);
        if (sTraceFile)
            fclose(sTraceFile);
        if (sLogFile)
            fclose(sLogFile);
        sTraceFile = sLogFile = NULL;
        if (!sOptions.mErr.empty() && sProcessStderr >= 0)
            SetStderr(sProcessStderr);
    }
    std::string theOutcome;
    {
        std::lock_guard<std::mutex> theGuard(sStateLock);
        theOutcome = sOutcome;
    }
    Marker("EE-SIM GAME %lu END %d %s", (unsigned long)sGameIndex, inCode,
           inCode == 2 ? "error" : theOutcome.empty() ? "-" : theOutcome.c_str());
}

// the main menu to the end of the game, then the report -> the exit code (0, or 3 at the wall timeout)
int PlayGame()
{
    int theCode = 0;
    {
        Phase(sLeftGame ? "leave" : "startup");
        sLeftGame = false;
        if (TopForm() == kFormMain) {   // (a game left returns to the single player menu)
            Note("UI main menu");
            Click(kFormMain, 0);
        }
        WaitForm(kFormSingle);
        Click(kFormSingle, 1);
        WaitForm(kFormLobbyID);
        Note("UI lobby");
        Phase("menus");
        SetUpLobby();
        Click(kFormLobbyID, 10);
        Note("UI start");
        Phase("lobby");
        long theExpected = sOptions.mComputers + 2;
        WaitFor("the game", 600, [&] {
            return TopForm() == kFormGame && gPhysics != NULL && gPhysics->mTimerRunning &&
                   (long)gWorld.GetNumPlayers() == theExpected && gWorld.GetMap() != NULL;
        });
        SimSleep(500);
        {
            // the idle human as the harness's civilization pre-empt leaves it (ai_game.py: the civilization builder
            // opened and cancelled): the civilization timer sends its messages but opens no form, and the computers'
            // civilizations are not locked at its expiry
            gWorld.mLock->WriteLock();
            EEPlayer* theHuman = gWorld.GetCurrentPlayer();
            theHuman->mCivilizationChosen = true;
            theHuman->mCivilizationLocked = true;
            // (the idle human out of the fighting from the start: the computers' attack choice prefers a human, and the
            // classic's early raids went to this seat, which never fights back, in 60 of 64 games against boom from
            // minute 4. Allied both ways with every computer it is no AI's enemy (the classic's and the Lua view's
            // target lists skip allies); the computers stay at war with each other. An observer seat, or its units
            // removed, ends the single-player game at once. --keep-human: the old seat)
            if (sOptions.mHumanOut) {
                for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) {
                    EEPlayer* theOther = gWorld.GetPlayer(i);
                    if (theOther == NULL || theOther == theHuman)
                        continue;
                    theHuman->SetDiplomaticStance(theOther, 0, gWorld.GetMap());
                    theOther->SetDiplomaticStance(theHuman, 0, gWorld.GetMap());
                }
            }
            sResults.assign(gWorld.GetNumPlayers(), 0);
            gWorld.mLock->WriteUnlock();
        }
        Note("GAME players %lu local %ld: running", gWorld.GetNumPlayers(), gWorld.mCurrentPlayer);
        Phase("loading");
        sGameWall = std::chrono::steady_clock::now();
        gPlatSimAfterTick = AfterTick;
        gPlatSimRun = 1;
        std::unique_lock<std::mutex> theLock(sStateLock);
        if (!sDone.wait_for(theLock, std::chrono::duration<double>(sOptions.mWallTimeout), [] { return sFinished; })) {
            gPlatSimRun = 0;
            sOutcome = "wall-timeout";
            theCode = 3;
        }
    }
    gPlatSimRun = 0;
    SimSleep(50);
    if (gPhysics != NULL) {
        // the final state (the physics thread has stopped between two communications ticks)
        gWorld.mLock->ReadLock();
        Sample s = TakeSample();
        gWorld.mLock->ReadUnlock();
        std::lock_guard<std::mutex> theGuard(sStateLock);
        if (sSamples.empty() || sSamples.back().mGameMs != s.mGameMs)
            sSamples.push_back(s);
    }
    {
        std::lock_guard<std::mutex> theGuard(sStateLock);
        const Sample* theLast = sSamples.empty() ? NULL : &sSamples.back();
        std::string theUnits;
        if (theLast)
            for (const PlayerSample& p : theLast->mPlayers)
                theUnits += " " + std::to_string(p.mIndex) + ":" + std::to_string(p.mPopulation) + "/" +
                            std::to_string(p.mVictoryUnits);
        double theGame = std::chrono::duration<double>(std::chrono::steady_clock::now() - sGameWall).count();
        Note("SIM %s: PT %lu game s %ld units%s; %.1f wall s in game (%.0fx real time)", sOutcome.c_str(),
             theLast ? theLast->mPT : 0UL, theLast ? theLast->mGameMs / 1000 : 0L, theUnits.c_str(), theGame,
             theLast && theGame > 0 ? theLast->mGameMs / 1000.0 / theGame : 0.0);
    }
    Phase("game");
    WriteReport(theCode, std::string());
    return theCode;
}

// --games: the game left as a player leaves it (the game menu's quit: EEUserInterface::CancelGame clears the world and
// the game state and opens the post-game summary), then the summary's exit, back to the single player menu
void LeaveGame()
{
    sLeftGame = true;
    gUserInterface->PostCancelGame();
    unsigned long theLast = 0;
    bool theExited = false;   // (the summary's exit once: a second close, still queued, would close the next form)
    WaitFor("the single player menu after the game", 120, [&] {
        unsigned long theTop = TopForm();
        if (theTop != theLast) {
            Note("UI form %lu", theTop);
            theLast = theTop;
        }
        if (theTop == kFormPostGameSummary && !theExited) {
            theExited = true;
            UILock theLock;
            UIForm* theForm = gUIManager->GetCurrentForm();
            for (unsigned long k = 0; theForm && theForm->GetFormID() == kFormPostGameSummary &&
                                      k < theForm->GetControlCount(); k++) {
                UIControl* theControl = theForm->GetControl(k);
                if (theControl && dynamic_cast<EEUIEPGSEExit*>(theControl->GetTriggerEvent(kTriggerEvent))) {
                    theControl->FireTriggerEvent(kTriggerEvent);
                    break;
                }
            }
        }
        return (theTop == kFormSingle || theTop == kFormMain) && theExited;
    });
    // (the menu settled: the form on top for 300 ms; a click into a form still opening was lost)
    auto theSince = std::chrono::steady_clock::now();
    unsigned long theTop = TopForm();
    WaitFor("the single player menu to settle", 30, [&] {
        unsigned long theNow = TopForm();
        if (theNow != theTop) {
            theTop = theNow;
            theSince = std::chrono::steady_clock::now();
        }
        return (theTop == kFormSingle || theTop == kFormMain) &&
               std::chrono::steady_clock::now() - theSince > std::chrono::milliseconds(300);
    });
}

int NextGame(size_t inIndex, Options& outGame, std::string& outError);

void Drive()
{
    std::vector<std::string> theProblems;
    EERulesOverrideProblems(theProblems);
    if (!theProblems.empty()) {
        std::string theText;
        for (const std::string& p : theProblems)
            theText += (theText.empty() ? "" : "; ") + p;
        if (sBatch)
            BeginGame(0, sOptions, false);
        Fail("--set did not apply: " + theText);
    }
    WaitForm(kFormMain, 300);
    if (!sBatch)
        Quit(PlayGame());
    bool thePlayed = false;   // a game ran and was not left yet
    size_t k = 0;
    for (;; k++) {
        Options theGame;
        std::string theError;
        int theNext = NextGame(k, theGame, theError);
        if (theNext == 0)
            break;
        if (theNext < 0) {
            Marker("EE-SIM GAME %lu END 64 bad-options %s", (unsigned long)k, theError.c_str());
            continue;
        }
        BeginGame(k, theGame, thePlayed);
        if (thePlayed)
            LeaveGame();   // (a failure here is this game's: its report has the error)
        thePlayed = true;
        StartGameOutput();
        int theCode = PlayGame();
        EndGameOutput(theCode);
        if (theCode != 0)
            Quit(theCode);   // a wall timeout leaves a game running: the rest in a new process
    }
    Marker("EE-SIM BATCH END %lu", (unsigned long)k);
    Quit(0);
}

const char* kUsage =
    "EE-SIM.exe: the headless simulation (TEST ONLY). Runs a seeded computer-vs-computer random-map game of the game's\n"
    "own code without window, rendering or sound, as fast as the CPU allows, and writes a JSON report.\n"
    "  --seed N               the start seed (default 2)\n"
    "  --ai N                 computer players, 1..7 (default 2)\n"
    "  --map-size NAME        lobby map size line (default Small)\n"
    "  --map-type NAME        lobby map type line (default Continental)\n"
    "  --difficulty NAME      lobby difficulty line (default Hard)\n"
    "  --lobby CONTROL=VALUE  any lobby combo of form 26 by line name or #index (e.g. 209=#2)\n"
    "  --game-seconds S       stop at this game time (default 2700)\n"
    "  --sample-seconds S     report sample interval in game seconds (default 30)\n"
    "  --wall-timeout S       give up after this wall time (default 3600)\n"
    "  --pt-rate N            Min = Max PTs per Second (default 40, the harness's pin)\n"
    "  --human-defeat         let the idle human be defeated (default: kept in, the harness's --no-human-defeat)\n"
    "  --flare SECONDS:PLAYER the idle human flares at the player's first building at that game time (TEST ONLY:\n"
    "                         its allies, the computers, answer it as an ally's flare)\n"
    "  --volcano SECONDS:PLAYER a volcano cast 6 tiles off the player's first building at that game time; logs the\n"
    "                         boulders launched every game second (TEST ONLY)\n"
    "  --keep-human           keep the idle human at war (default: allied with every computer from the start, so no\n"
    "                         computer attacks it)\n"
    "  --cheats SETS          EE_AI_CHEATS (e.g. none, all, none/all)\n"
    "  --personality NAMES    EE_AI_PERSONALITY (e.g. balanced)\n"
    "  --engine LIST          the computer seats' engine through the lobby's engine boxes: classic, random, an AI's\n"
    "                         id from Data/ai/ais.json (e.g. boom) or next (its first AI); one for every seat or a\n"
    "                         comma list by seat, taken in turn (e.g. boom,classic)\n"
    "  --set DOC.KEY=VALUE    a rules document value (repeatable), e.g. aipersonalities.balanced.military.army_floor=0.2\n"
    "  --data DIR             the data root (EE_DATA_ROOT; default the exe's directory)\n"
    "  --config FILE          the settings file (EE_CONFIG; default <data root>/config.yaml)\n"
    "  --report FILE          the JSON report (default stdout)\n"
    "  --trace FILE           the EE-AI trace lines (with EE_AI_TRACE=1 or --ai-trace; default stdout)\n"
    "  --ai-trace             EE_AI_TRACE=1\n"
    "  --log FILE             every debug string of the game\n"
    "  --autosave-ms N        the game's autosave period in PT time (default 0: none; the game and the harness: 600000)\n"
    "  --port-offset N        move the game's TCP ports 33334/33335 by N (default: a free pair from 35334 up)\n"
    "  --quiet                no progress on stderr\n"
    "  --games FILE           several games in a row in this process, one a line: the line's game options over the\n"
    "                         command line's (# comments; \"...\" quotes). Game options: --seed --ai --map-size\n"
    "                         --map-type --difficulty --lobby --engine --game-seconds --sample-seconds --wall-timeout\n"
    "                         --human-defeat --keep-human --flare --volcano --report --trace --log and\n"
    "                         --err FILE (the game's stderr), --out DIR (DIR/report.json, trace.txt, err.txt),\n"
    "                         --id NAME (the markers' name). The rest (--set --cheats --personality --ai-trace\n"
    "                         --pt-rate --autosave-ms --data --config --port-offset) holds for every game. stderr gets\n"
    "                         \"EE-SIM GAME k BEGIN id\" / \"EE-SIM GAME k END code outcome\" per game and \"EE-SIM\n"
    "                         BATCH END n\"; a failed game (code 2 or 3) ends the process (tools/sim.py play_games\n"
    "                         goes on with the rest in a new one). Each game's report and trace equal its run alone\n";

// a port offset whose TCP and UDP ports 33334/33335 are free now, from one the process id picks (the harness's
// parallel games use 33334 + 16 k, k <= 32)
long FreePortOffset()
{
#ifdef _WIN32
    long theOffset = 2000 + 2 * (long)(GetCurrentProcessId() % 8000);
    WSADATA theData;
    if (WSAStartup(MAKEWORD(2, 2), &theData) != 0)
        return theOffset;
    for (int k = 0; k < 400; k++, theOffset = 2000 + (theOffset - 2000 + 2) % 16000) {
        bool theFree = true;
        for (int p = 0; p < 2 && theFree; p++)
            for (int theType = SOCK_STREAM; theType <= SOCK_DGRAM && theFree; theType++) {
                SOCKET s = socket(AF_INET, theType, 0);
                if (s == INVALID_SOCKET)
                    continue;
                sockaddr_in a = {};
                a.sin_family = AF_INET;
                a.sin_port = htons((u_short)(33334 + p + theOffset));
                a.sin_addr.s_addr = htonl(INADDR_ANY);
                if (bind(s, (sockaddr*)&a, sizeof(a)) != 0)
                    theFree = false;
                closesocket(s);
            }
        if (theFree)
            break;
    }
    WSACleanup();
    return theOffset;
#else
    return 2000 + 2 * (long)(getpid() % 8000);
#endif
}

// a path made absolute now (the game moves the current directory to its data root at start)
std::string Absolute(const std::string& inPath)
{
    if (inPath.empty() || inPath == "-")
        return inPath;
#ifdef _WIN32
    char theFull[4096];
    if (_fullpath(theFull, inPath.c_str(), sizeof(theFull)))
        return theFull;
    return inPath;
#else
    if (inPath[0] == '/')
        return inPath;
    char theDirectory[4096];
    return getcwd(theDirectory, sizeof(theDirectory)) ? std::string(theDirectory) + "/" + inPath : inPath;
#endif
}

void SetEnv(const char* inName, const std::string& inValue)
{
#ifdef _WIN32
    _putenv_s(inName, inValue.c_str());
#else
    setenv(inName, inValue.c_str(), 1);
#endif
}

// the process's stderr, kept while a game's own is in place (--games)
int EESimDupStderr()
{
#ifdef _WIN32
    return _dup(2);
#else
    return dup(2);
#endif
}

// a game's own option (the command line's, or a line's of the --games file) at inArgs[ioIndex] -> 1 taken (ioIndex
// at its last word), 0 not a game option, -1 a bad one (outError)
int GameOption(const std::vector<std::string>& inArgs, size_t& ioIndex, Options& ioOptions, std::string& outError)
{
    const std::string& a = inArgs[ioIndex];
    static const char* const kWithValue[] = {"--seed", "--ai", "--map-size", "--map-type", "--difficulty", "--lobby",
        "--game-seconds", "--sample-seconds", "--wall-timeout", "--flare", "--volcano", "--engine", "--report",
        "--trace", "--log", "--err", "--out", "--id"};
    bool theValued = false;
    for (const char* n : kWithValue)
        theValued = theValued || a == n;
    std::string v;
    if (theValued) {
        if (ioIndex + 1 >= inArgs.size()) {
            outError = a + " needs a value";
            return -1;
        }
        v = inArgs[++ioIndex];
    }
    auto pair = [&](const char* inForm, long& outMs, unsigned long& outPlayer) {
        size_t c = v.find(':');
        if (c == std::string::npos) {
            outError = std::string(a) + " " + inForm;
            return -1;
        }
        outMs = (long)(atof(v.substr(0, c).c_str()) * 1000);
        outPlayer = (unsigned long)atol(v.substr(c + 1).c_str());
        return 1;
    };
    if (a == "--seed")
        ioOptions.mSeed = strtoul(v.c_str(), NULL, 0);
    else if (a == "--ai")
        ioOptions.mComputers = atol(v.c_str());
    else if (a == "--map-size")
        ioOptions.mMapSize = v;
    else if (a == "--map-type")
        ioOptions.mMapType = v;
    else if (a == "--difficulty")
        ioOptions.mDifficulty = v;
    else if (a == "--lobby") {
        size_t eq = v.find('=');
        if (eq == std::string::npos) {
            outError = "--lobby CONTROL=VALUE";
            return -1;
        }
        ioOptions.mLobby.push_back({atol(v.substr(0, eq).c_str()), v.substr(eq + 1)});
    } else if (a == "--game-seconds")
        ioOptions.mGameSeconds = atof(v.c_str());
    else if (a == "--sample-seconds")
        ioOptions.mSampleSeconds = atof(v.c_str());
    else if (a == "--wall-timeout")
        ioOptions.mWallTimeout = atof(v.c_str());
    else if (a == "--human-defeat")
        ioOptions.mNoHumanDefeat = false;
    else if (a == "--no-human-defeat")
        ioOptions.mNoHumanDefeat = true;
    else if (a == "--keep-human")
        ioOptions.mHumanOut = false;
    else if (a == "--flare")
        return pair("SECONDS:PLAYER", ioOptions.mFlareMs, ioOptions.mFlarePlayer);
    else if (a == "--volcano")
        return pair("SECONDS:PLAYER", ioOptions.mVolcanoMs, ioOptions.mVolcanoPlayer);
    else if (a == "--engine")
        ioOptions.mEngine = v;
    else if (a == "--report")
        ioOptions.mReport = Absolute(v);
    else if (a == "--trace")
        ioOptions.mTrace = Absolute(v);
    else if (a == "--log")
        ioOptions.mLog = Absolute(v);
    else if (a == "--err")
        ioOptions.mErr = Absolute(v);
    else if (a == "--id")
        ioOptions.mID = v;
    else if (a == "--out") {   // a game's folder: report.json, trace.txt, err.txt
        std::string d = Absolute(v);
#ifdef _WIN32
        CreateDirectoryA(d.c_str(), NULL);
        d += "\\";
#else
        mkdir(d.c_str(), 0777);
        d += "/";
#endif
        ioOptions.mReport = d + "report.json";
        ioOptions.mTrace = d + "trace.txt";
        ioOptions.mErr = d + "err.txt";
    } else
        return 0;
    return 1;
}

bool OptionsInRange(const Options& inOptions)
{
    return inOptions.mComputers >= 1 && inOptions.mComputers <= 7 && inOptions.mGameSeconds > 0 &&
           inOptions.mSampleSeconds > 0;
}

// a line of the --games file -> its words (blanks between them; "..." keeps blanks, \" a quote)
std::vector<std::string> Words(const std::string& inLine)
{
    std::vector<std::string> theWords;
    std::string theWord;
    bool theIn = false, theQuoted = false;
    for (size_t k = 0; k < inLine.size(); k++) {
        char c = inLine[k];
        if (c == '\\' && k + 1 < inLine.size() && inLine[k + 1] == '"') {
            theWord += '"';
            theIn = true;
            k++;
        } else if (c == '"') {
            theQuoted = !theQuoted;
            theIn = true;
        } else if (!theQuoted && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            if (theIn)
                theWords.push_back(theWord);
            theWord.clear();
            theIn = false;
        } else {
            theWord += c;
            theIn = true;
        }
    }
    if (theIn)
        theWords.push_back(theWord);
    return theWords;
}

// --games: the next game's line (blank lines and # comments skipped): its game options over the command line's ->
// 1 a game, 0 the end of the file, -1 a bad line (outError)
int NextGame(size_t inIndex, Options& outGame, std::string& outError)
{
    std::string theRow;
    while (true) {
        theRow.clear();
        int c;
        while ((c = fgetc(sGamesFile)) != EOF && c != '\n')
            theRow += (char)c;
        if (c == EOF && theRow.empty())
            return 0;
        std::vector<std::string> theWords = Words(theRow);
        if (theWords.empty() || theWords[0][0] == '#')
            continue;
        outGame = sBase;   // (a game's files are its own: none from the command line)
        outGame.mReport.clear();
        outGame.mTrace.clear();
        outGame.mLog.clear();
        outGame.mErr.clear();
        outGame.mID = std::to_string(inIndex);
        for (size_t i = 0; i < theWords.size(); i++) {
            int r = GameOption(theWords, i, outGame, outError);
            if (r <= 0) {
                if (r == 0)
                    outError = theWords[i] + " is not a game's option (--help)";
                return -1;
            }
        }
        if (!OptionsInRange(outGame)) {
            outError = "an option is out of range";
            return -1;
        }
        return 1;
    }
}

}   // namespace

// EESimPreferences: the settings the simulation runs with, over config.yaml (EERun, after gGamePreferences.Load): a
// window (hidden: DX7Screen::CreateAWindow), the harness's pins (32-bit, Min = Max PTs per Second), no autosave, no music
void EESimPreferences()
{
    gGamePreferences.mSettings.mFullScreen = false;
    gGamePreferences.mSettings.mGameBitDepth = 32;
    gGamePreferences.mSettings.mTextureBitDepth = 32;
    gGamePreferences.mSettings.mGameWindowWidth = 800;
    gGamePreferences.mSettings.mGameWindowHeight = 600;
    gGamePreferences.mSettings.mMinPTsPerSecond = sOptions.mPTRate;
    gGamePreferences.mSettings.mMaxPTsPerSecond = sOptions.mPTRate;
    gGamePreferences.mSettings.mAutoSaveInMilliseconds = (unsigned long)sOptions.mAutoSaveMs;
    gGamePreferences.mSettings.mMusicEnabled = false;
    gGamePreferences.mSettings.mWaitForVSync = false;
}

// EESimStart: the driver's thread (EERun, after the engine and the threads are up, before the UI loop)
void EESimStart()
{
    std::thread(Drive).detach();
}

#ifdef _WIN32
// a fault in the simulation: the code, the place and the return addresses (exe addresses: EE-SIM.map) on stderr
static LONG CALLBACK EESimFault(EXCEPTION_POINTERS* inInfo)
{
    DWORD theCode = inInfo->ExceptionRecord->ExceptionCode;
    if (theCode < 0xC0000000UL || theCode == 0xE06D7363UL)   // not a fault (thread names, debug strings, C++)
        return EXCEPTION_CONTINUE_SEARCH;
    static volatile LONG sOnce = 0;
    if (InterlockedExchange(&sOnce, 1))
        return EXCEPTION_CONTINUE_SEARCH;
    void* theFrames[48];
    USHORT theCount = RtlCaptureStackBackTrace(0, 48, theFrames, NULL);
    char theText[2048];
    int n = snprintf(theText, sizeof(theText), "EE-SIM FAULT %08lx at %p thread %lu; frames:", (unsigned long)theCode,
                     inInfo->ExceptionRecord->ExceptionAddress, (unsigned long)GetCurrentThreadId());
    for (USHORT k = 0; k < theCount && n < (int)sizeof(theText) - 24; k++)
        n += snprintf(theText + n, sizeof(theText) - n, " %p", theFrames[k]);
    fprintf(stderr, "%s\n", theText);
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
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

// EESimMain: EE-SIM.exe's entry (EESimMain.cpp's main): the options, the switches, then the game's own WinMain. On
// POSIX the game's main calls it for "ee-aoc --sim ..." and goes on with the game when it returns -1.
int EESimMain(int argc, char** argv)
{
    sStartWall = sPhaseWall = std::chrono::steady_clock::now();
    std::string theData, theConfig, theGames;
    bool theTraceOn = false;
    std::vector<std::string> theArgs(argv + 1, argv + argc);
    for (size_t i = 0; i < theArgs.size(); i++) {
        std::string a = theArgs[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= theArgs.size()) {
                fprintf(stderr, "EE-SIM: %s needs a value\n", a.c_str());
                exit(64);
            }
            return theArgs[++i];
        };
        std::string theError;
        int theGameOption = GameOption(theArgs, i, sOptions, theError);
        if (theGameOption < 0) {
            fprintf(stderr, "EE-SIM: %s\n", theError.c_str());
            return 64;
        }
        if (theGameOption > 0)
            continue;
        if (a == "-h" || a == "--help") {
            fputs(kUsage, stdout);
            return 0;
        } else if (a == "--pt-rate")
            sOptions.mPTRate = atol(value().c_str());
        else if (a == "--cheats")
            sOptions.mCheats = value();
        else if (a == "--personality")
            sOptions.mPersonality = value();
        else if (a == "--set") {
            std::string v = value();
            if (!EERulesAddOverride(v, theError)) {
                fprintf(stderr, "EE-SIM: --set %s\n", theError.c_str());
                return 64;
            }
            sOptions.mSets.push_back(v);
        } else if (a == "--data")
            theData = Absolute(value());
        else if (a == "--config")
            theConfig = Absolute(value());
        else if (a == "--games")
            theGames = value() == "-" ? std::string("-") : Absolute(theArgs[i]);
        else if (a == "--ai-trace")
            theTraceOn = true;
        else if (a == "--quiet")
            sOptions.mQuiet = true;
        else if (a == "--port-offset")
            sOptions.mPortOffset = atol(value().c_str());
        else if (a == "--autosave-ms")
            sOptions.mAutoSaveMs = atol(value().c_str());
        else {
            fprintf(stderr, "EE-SIM: unknown option %s (--help)\n", a.c_str());
            return 64;
        }
    }
    if (!OptionsInRange(sOptions) || sOptions.mPTRate < 1 || sOptions.mPTRate > 250) {
        fprintf(stderr, "EE-SIM: an option is out of range (--help)\n");
        return 64;
    }
    if (!theGames.empty()) {
        std::string theError;
        sGamesFile = theGames == "-" ? stdin : fopen(theGames.c_str(), "rb");
        if (sGamesFile == NULL) {
            fprintf(stderr, "EE-SIM: cannot read --games %s\n", theGames.c_str());
            return 66;
        }
        sBase = sOptions;
        sBatch = true;
        sProcessStderr = EESimDupStderr();
    }
    if (!theData.empty())
        SetEnv("EE_DATA_ROOT", theData);
    if (!theConfig.empty())
        SetEnv("EE_CONFIG", theConfig);
    if (theTraceOn)
        SetEnv("EE_AI_TRACE", "1");
    if (!sOptions.mCheats.empty())
        SetEnv("EE_AI_CHEATS", sOptions.mCheats);
    if (!sOptions.mPersonality.empty())
        SetEnv("EE_AI_PERSONALITY", sOptions.mPersonality);
    if (!sBatch && !sOptions.mTrace.empty()) {
        sTraceFile = fopen(sOptions.mTrace.c_str(), "w");
        if (!sTraceFile) {
            fprintf(stderr, "EE-SIM: cannot write %s\n", sOptions.mTrace.c_str());
            return 66;
        }
    }
    if (!sBatch && !sOptions.mLog.empty())
        sLogFile = fopen(sOptions.mLog.c_str(), "w");
    gPlatSimMode = 1;
    gPlatSimSeedSet = 1;
    gPlatSimSeed = (uint32_t)sOptions.mSeed;   // (--games: each game's, BeginGame)
    gPlatSimNoHumanDefeat = sOptions.mNoHumanDefeat ? 1 : 0;
    gPlatSimPortOffset = sOptions.mPortOffset >= 0 ? (int)sOptions.mPortOffset : (int)FreePortOffset();
    PlatDebugSetHandler(DebugHandler);
#ifdef _WIN32
    AddVectoredExceptionHandler(1, EESimFault);
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
    if (sBatch)
        Note("EE-SIM games from %s, ports %d/%d", theGames.c_str(), 33334 + gPlatSimPortOffset,
             33335 + gPlatSimPortOffset);
    else
        Note("EE-SIM seed %lu, %ld computers, cap %.0f game s, ports %d/%d", sOptions.mSeed, sOptions.mComputers,
             sOptions.mGameSeconds, 33334 + gPlatSimPortOffset, 33335 + gPlatSimPortOffset);
#ifdef _WIN32
    char theEmpty[1] = {0};
    EESimGameEnded(WinMain(GetModuleHandleA(NULL), NULL, theEmpty, SW_HIDE));
#else
    return -1;   // the POSIX main goes on with the game (Empire Earth.cpp: ee-aoc --sim ...)
#endif
}

// EESimGameEnded: the game's own start-up returned before the driver ended the run (it was quit, or failed to start)
void EESimGameEnded(int inResult)
{
    WriteReport(4, "the game ended before the simulation (start-up failed?)");
    Quit(inResult ? inResult : 4);
}
