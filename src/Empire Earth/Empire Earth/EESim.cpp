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
// --checksum-trace FILE: after every communications tick, one line of the simulation's state hashes (ChecksumLine;
// docs/RUST_PLAN.md R5a.1), so two runs name the first divergent tick (tools/sim_oracle.py ticks).
// --save-at MS=FILE: at the first communications tick at or after game ms MS, the game saves (the autosave's request,
// EEMLoadOrSaveGame through SendToAllAndWait, under a fixed name); the file is copied to FILE at the end of the run.
// --load-save FILE: instead of the lobby, the game's own load path (main menu -> single player -> Load Game -> the
// file -> Load, as the harness's --load-save), then the run as usual. Together: "load equals continue" (R5a.2;
// tools/sim_oracle.py saveload): a game loaded from a save must trace like the game that went on.
#ifdef _WIN32
#include <winsock2.h>
#else
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
#include "ee/EEMessageCapture.h"
#include "ee/EEUnitStore.h"
#include "ee/EEUnitHost.h"
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUIESinglePlayerEvents.h"
#include "ee/EEUIESinglePlayerLoadEvents.h"
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
#include "ee/EEAreaEffect.h"
#include "ee/EEFormation.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIManager.h"
#include "ee/EEGamePreferences.h"
#include "ee/EERules.h"
#include "ee/EECPTrace.h"
#include "ee/EESimCapture.h"
#include "ee/EEPathManager.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "lle/U2DPoint.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "lle/UReferenceCounter.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EETown.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEAreaUtilities.h"     // EECanHeal (the sweep)
#include "ee/EEUnitUtilities.h"     // EESearchTargetAlongList (the end sweep)
#include "ee/EETechCapture.h"
#include "ee/EESaveGameFiles.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"

// TEST ONLY: the combat formulas' sweep of the simulation capture (ee/EECombatFormulas.h). The computer players'
// games never regenerate, heal or convert, and seldom test the range to hit, so with EE_SIM_CAPTURE on this calls
// those formulas (and the reach and damage effect tests) over each player's newest 64 living units and their
// neighbours: real stats, distances and timers, plus fixed variations. It reads the world and writes nothing back.
// EECanHeal (CBCH: no game heals) runs over each pair both ways, a unit and itself, and a unit and the previous
// player's newest unit.
static void EECombatCaptureSweep()
{
    const EECombatRules& theRules = EEGetRules().mCombat;
    int32_t theTick = (int32_t)gPhysics->mTickTimeMs;
    EEComplexUnit* theOtherPlayers = NULL;
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theAll;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0 && theUnit->mStats != NULL)
                theAll.push_back(theUnit->mComplexUnit);
        size_t theFirst = theAll.size() > 64 ? theAll.size() - 64 : 0;
        for (size_t i = theFirst; i + 1 < theAll.size(); i++) {
            EEComplexUnit* a = theAll[i];
            EEComplexUnit* b = theAll[i + 1];
            EEUnitClassUpgrade* s = a->mStats;
            EEUnitClassUpgrade* t = b->mStats;
            EECanHeal(a, b);
            EECanHeal(b, a);
            EECanHeal(a, a);
            if (theOtherPlayers != NULL)
                EECanHeal(a, theOtherPlayers);
            int32_t theHitPoints = (int32_t)a->mHitPoints;
            int32_t theMax = (int32_t)s->mMaxHitPoints;
            const int32_t kTimers[] = {(int32_t)a->mRegenerationTimer, 0, theTick, 1000};
            const int32_t kHits[] = {theHitPoints, 0, theMax, theMax / 3 + 1};
            for (int32_t theTimer : kTimers) {
                EeCombatTimerIn theT = {theTimer, theTick, (int32_t)theRules.mRegenerationInterval};
                EECombatTimer(theT);
                for (int32_t theHp : kHits) {
                    EeCombatRegenerateIn theR = {theTimer, theTick, theHp, (int32_t)s->mRegeneration, theMax,
                                                 (int32_t)theRules.mRegenerationInterval};
                    EECombatRegenerate(theR);
                    theR.regeneration = 3;
                    EECombatRegenerate(theR);
                }
            }
            for (int32_t theHp : kHits) {
                EeCombatHealIn theH = {theHp, (int32_t)t->mHealingPoints, theMax};
                EECombatHeal(theH);
                theH.healing = 7;
                EECombatHeal(theH);
                EeCombatDamageEffectIn theE = {theHp, theHp - (int32_t)t->mMaxHitPoints / 4, theMax, 0.5f};
                EECombatDamageEffect(theE);
            }
            const uint32_t kElapsed[] = {0, 900, 999, 1000};
            for (uint32_t theElapsed : kElapsed) {
                EeCombatHealStepIn theS = {theElapsed, theTick};
                EECombatHealStep(theS);
                theS.tick_ms = 0;
                EECombatHealStep(theS);
            }
            EeCombatConversionCostIn theC = {(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost), (int32_t)s->mPower};
            EECombatConversionCost(theC);
            const int32_t kWork[] = {(int32_t)a->mWorkTimer, 950, 999, -5};
            for (int32_t theWork : kWork) {
                for (int theAdaptation = 0; theAdaptation < 2; theAdaptation++) {
                    EeCombatConversionIn theV = {theWork, gPhysics->mTickSeconds, t->mConversionResistance, theAdaptation,
                                                 s->mConversionRate, b->mPlayer->mConversionFactor};
                    EECombatConversion(theV);
                    theV.resistance = 1.0f;
                    theV.rate = 0.4f;
                    EECombatConversion(theV);
                }
            }
            float theDistance = a->DistanceSquaredTo(b);
            EECombatWithinRange(theDistance, a->mUnitClass->mHealRange);
            EECombatWithinRange(theDistance, s->mRange);
            for (int theCrusader = 0; theCrusader < 2; theCrusader++) {
                EeCombatRangeIn theG = {s->mRange, a->mUnitClass->mHealRange, theCrusader, theDistance,
                                        b->mUnitClass->IsLandBuilding(), b->mUnitClass->mInnerCollisionSphere};
                EECombatRange(theG);
            }
            for (int theFlags = 0; theFlags < 4; theFlags++) {
                EeCombatReachIn theQ = {theDistance, a->mUnitClass->mInnerCollisionSphere,
                                        b->mUnitClass->mInnerCollisionSphere, 0.0f, theFlags & 1, theFlags >> 1};
                EECombatWorkReach(theQ);
                EECombatGatherReach(theQ);
            }
            if (s->mUnitClass->mProjectileClass != NULL) {
                GE3DPoint theFrom;
                GE3DPoint theTo;
                a->GetPosition(theFrom);
                b->GetPosition(theTo);
                for (int theHigh = 0; theHigh < 2; theHigh++) {
                    EeCombatBallisticIn theB = {theFrom.GetX(), theFrom.GetY(), theFrom.GetZ(), theTo.GetX(), theTo.GetY(),
                                                theTo.GetZ(), s->mLaunchSpeed,
                                                s->mUnitClass->mProjectileClass->GetAcceleration(), theHigh};
                    EECombatBallistic(theB);
                }
            }
        }
        if (!theAll.empty())
            theOtherPlayers = theAll.back();
    }
}

// TEST ONLY: the unit-list target search's sweep (CBSL). No game of the capture profiles reaches
// EESearchTargetAlongList (only anti-air ammunition 0x3c does, through EEFindNearbyTarget), so with EE_SIM_CAPTURE on
// it runs once, when the game ends, over each player's newest 64 armed units on the map (priests left out: a list
// unit without a complex unit reaches CanAttackClass through NULL there, as in the original): at the unit's tile, and
// two tiles off with half the range again. Only its CBSL records are written (not the CBAF, CBLB, CBDP, CBHD records
// of the tests it calls), and each unit's ignored targets are put back after it (IgnoreTarget, the expired entries'
// erase): the game is over and no other record follows it.
static void EECombatListSearchSweep()
{
    EESimCaptureRecord::Only(EE_COMBAT_TAG('C', 'B', 'S', 'L'));
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theAll;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL && theUnit->mHitPoints > 0 && theUnit->mStats != NULL && theComplex->mTileNode != NULL
                && theComplex->GetTileX() != -10000 && theComplex->mStats->GetAttack() > 0.0f
                && !theComplex->GetUnitClass()->IsPriest())
                theAll.push_back(theComplex);
        }
        size_t theFirst = theAll.size() > 64 ? theAll.size() - 64 : 0;
        for (size_t i = theFirst; i < theAll.size(); i++) {
            EEComplexUnit* a = theAll[i];
            std::deque<EEComplexUnitAttacker> theIgnored = a->mIgnoredTargets;
            EESearchTargetAlongList(a, a->GetTileX(), a->GetTileY(), 1.0f);
            a->mIgnoredTargets = theIgnored;
            EESearchTargetAlongList(a, a->GetTileX() + 2, a->GetTileY() - 2, 1.5f);
            a->mIgnoredTargets = theIgnored;
        }
    }
    EESimCaptureRecord::Only(0);
}

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
    std::string mReport;                                 // the JSON report ("-": stdout)
    std::string mTrace;                                  // the EE-AI lines ("" stdout)
    std::string mLog;                                    // every debug string
    std::string mChecksumTrace;                          // the per-CT checksum lines ("" none)
    std::vector<std::string> mSets;                      // --set doc.key=value
    std::string mCheats, mPersonality;
    std::string mEngine;                                 // --engine: the computer seats' engines ("" the lobby's: classic)
    long mPortOffset = -1;                               // -1: no fixed ports (kPlatSimAnyPort)
    bool mCalamities = false;                            // --calamities (TEST ONLY; ee/EETechCapture.h)
    long mAutoSaveMs = 0;                                // the game's autosave period (0: none; the game's default 600000)
    std::vector<std::pair<long, std::string>> mSaves;    // --save-at MS=FILE, in game-ms order
    std::string mLoadSave;                               // --load-save FILE ("" a new game from the lobby)
    bool mQuiet = false;
};

Options sOptions;
FILE* sTraceFile = NULL;
FILE* sLogFile = NULL;
FILE* sChecksumFile = NULL;
FILE* sRngFile = NULL;                                   // --rng-trace (RngTraceHook, below)
std::mutex sOutLock;
std::chrono::steady_clock::time_point sStartWall;
std::chrono::steady_clock::time_point sGameWall;

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
        if (sChecksumFile)
            fflush(sChecksumFile);
        if (sRngFile)
            fflush(sRngFile);
    }
    EESimCaptureRecord::Flush();
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
            else if (IsBuildingCategory(theCategory))
                p.mBuildings++;
            else {
                p.mArmy++;
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

// the end of a game in the tick hook (the physics thread): the end sweep first, in a captured game
void FinishGame(const char* inOutcome)
{
    static bool sSwept = false;
    if (EESimCaptureRecord::Active() && !sSwept) {
        sSwept = true;
        EECombatListSearchSweep();
    }
    Finish(inOutcome);
}

// The out-of-synch checksum's parts read for the per-CT line: unit hit points and tiles (1), positions, heights,
// commands and update counts (2), calamities (0x10), air traffic (0x40), the continent manager's checksum (0x100).
// Left out because reading them is not free of effects: 4, the map's window checksum, advances the map's checksum
// cursor (EEMap::ComputeChecksum); and 0x20, the goals, copies each goal queue and dereferences goal members the
// multiplayer game (flag 1 only) never reads. The game's own OOS check uses flag 1.
const unsigned long kChecksumFlags = 0x1 | 0x2 | 0x10 | 0x40 | 0x100;

// --checksum-trace: one line per communications tick (the physics thread, under the world's write lock, after the
// CT's last physics tick). Only reads: no random draws, no caches filled; EEPhysics::mLastOutOfSynchChecksum, the one member
// GetOutOfSynchChecksum writes (and nothing reads), is put back. The parts beside "oos" say what diverged first:
//   CT <the game's CTs run, from 1> pt=<ticks done> tick=<current tick> ms=<game ms> rng=<physics random seed>
//      oos=<GetOutOfSynchChecksum(kChecksumFlags)> units=<count>/<sum of GetChecksum(3)> ai=<EECPOutOfSynchChecksum>
//      inputs=<user inputs executed>
// The CT number counts from the game's first CT: the communications' own count (GetNumberOfCTsProcessedInPhysics)
// includes the CTs the send thread sent before the sim took over, a wall-clock-dependent number (22-25 in six runs).
// A line only after physics ticks ran: around a save the physics loop passes gPlatSimAfterTick without running a
// tick, a wall-clock-dependent number of times (151 at one autosave of seed 2), and those lines would differ by run.
unsigned long sChecksumLines = 0;
unsigned long sChecksumLastPT = 0;

void ChecksumFields(FILE* inFile)
{
    unsigned long theSaved = gPhysics->mLastOutOfSynchChecksum;
    unsigned long theChecksum = gPhysics->GetOutOfSynchChecksum(false, kChecksumFlags);
    gPhysics->mLastOutOfSynchChecksum = theSaved;
    unsigned long theUnits = 0;
    unsigned long theCount = 0;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        theUnits += (unsigned long)static_cast<EEUnit*>(theUnit)->GetChecksum(3);
        theCount++;
    }
    fprintf(inFile, "pt=%lu tick=%lu ms=%ld rng=%08lx oos=%08lx units=%lu/%08lx ai=%08lx inputs=%ld\n",
            gPhysics->mNumTicksDone, gPhysics->mCurrentTick,
            gPhysics->mGameTime, (unsigned long)(uint32_t)gPhysics->mRandom.GetSeed(), (unsigned long)(uint32_t)theChecksum,
            theCount, (unsigned long)(uint32_t)theUnits, (unsigned long)(uint32_t)EECPOutOfSynchChecksum(), gPhysics->mInputMessagesProcessed);
}

void ChecksumLine()
{
    if (sChecksumFile == NULL || (sChecksumLines > 0 && gPhysics->mNumTicksDone == sChecksumLastPT))
        return;
    sChecksumLastPT = gPhysics->mNumTicksDone;
    fprintf(sChecksumFile, "CT %lu ", ++sChecksumLines);
    ChecksumFields(sChecksumFile);
}

// --dump-at / --dump-load (TEST ONLY, the save/load fidelity hunt): the state the checksum line sums, item by item, so
// two dumps name the units and players that differ. Reads only (the goals' own out-of-synch checksums are read, as
// the checksum's flag 0x20 would).
void DumpState(const std::string& inPath)
{
    FILE* f = fopen(inPath.c_str(), "w");
    if (!f)
        return;
    fprintf(f, "STATE ");
    ChecksumFields(f);
    fprintf(f, "physics clock=%a tickrandom=%ld tickstoprocess=%ld\n", (double)gPhysics->mGameClock,
            (long)gPhysics->mTickRandom, (long)gPhysics->mTicksToProcess);
    {   // the stepped units and the (de)activation queues, in order (EEPWorldHelper's unit order)
        const std::deque<EEUnitHandle<EEComplexUnit> >* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
        const char* theNames[3] = {"UPDATE", "ACTIVATE", "DEACTIVATE"};
        for (int l = 0; l < 3; l++) {
            fprintf(f, "%s %zu:", theNames[l], theLists[l]->size());
            for (size_t i = 0; i < theLists[l]->size(); i++)
                fprintf(f, " %lu", (*theLists[l])[i]->mUnitID);
            fprintf(f, "\n");
        }
    }
    for (unsigned long i = 0; i < gWorld.GetNumPlayers(); i++) {
        EEPlayer* p = gWorld.GetPlayer(i);
        if (!p)
            continue;
        fprintf(f, "P %lu pop=%ld eog=%ld epoch=%ld res=", i, (long)p->mPopulationCount, (long)p->mEndOfGameCount,
                p->mTechnologies ? (long)p->mTechnologies->mCurrentEpoch : -1L);
        for (long r = 0; r < 5; r++)
            fprintf(f, "%s%a", r ? "," : "", (double)p->mResourceCaps[kResourceFood + r]);
        fprintf(f, "\n");
        // the towns (hull, patrol group) and the patrol groups (their points as unit@tile)
        for (size_t t = 0; t < p->mTowns.size(); t++) {
            EETown* theTown = p->mTowns[t];
            if (theTown == NULL)
                continue;
            fprintf(f, "TOWN %lu %ld units=%ld patrol=%ld hull=", i, (long)theTown->mTownID, (long)theTown->mNumUnits,
                    theTown->mPatrolGroup != NULL ? (long)theTown->mPatrolGroup->mID : -1L);
            for (unsigned long v = 0; v < theTown->mHull.GetPointCount(); v++)
                fprintf(f, "%ld,%ld ", (long)theTown->mHull.GetPoint(v)->GetX(), (long)theTown->mHull.GetPoint(v)->GetY());
            fprintf(f, "\n");
        }
        for (size_t g = 0; g < p->mPatrolGroups.size(); g++) {
            EEPPatrolGroup* theGroup = p->mPatrolGroups[g];
            if (theGroup == NULL)
                continue;
            fprintf(f, "PATROL %lu %lu town=%d loop=%d points=", i, (unsigned long)theGroup->mID, theGroup->mIsTownPatrol ? 1 : 0,
                    theGroup->mIsLoop ? 1 : 0);
            for (size_t k = 0; k < theGroup->mPatrolPoints.size(); k++) {
                EEComplexUnit* thePoint = theGroup->mPatrolPoints[k];
                if (thePoint != NULL)
                    fprintf(f, "%lu@%ld,%ld ", (unsigned long)thePoint->mUnitID, (long)thePoint->mTileX, (long)thePoint->mTileY);
            }
            fprintf(f, "\n");
        }
    }
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {   // each player's unit list in its order (save 0.270)
        if (gWorld.mPlayers[i] == NULL)
            continue;
        fprintf(f, "PLAYERUNITS %lu:", i);
        for (EEUnit* u = gWorld.mPlayers[i]->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext))
            fprintf(f, " %lu", (unsigned long)u->mUnitID);
        fprintf(f, "\n");
    }
    EECPOutOfSynchDump(f);   // the "ai" parts per player and the computer players' timers
    gPathManager.DumpSim(f);   // the searches in progress, the pending requests, the timers
    for (RTSUnit* r = gWorld.mUnits.mFirst; r != NULL; r = r->mTileNode->mNext) {
        EEUnit* u = static_cast<EEUnit*>(r);
        EEComplexUnit* c = dynamic_cast<EEComplexUnit*>(u);
        fprintf(f, "U %lu type=%ld player=%ld hp=%a tile=%ld,%ld", u->GetUnitID(), (long)u->mUnitClass->mUnitType,
                u->GetPlayer() ? (long)u->GetPlayer()->mPlayerID : -1L, (double)u->mHitPoints, (long)u->mTileX,
                (long)u->mTileY);
        if (c != NULL && c->mComplexUnit != NULL) {
            EEComplexUnit* x = c->mComplexUnit;
            fprintf(f, " upd=%lu cmd=%ld grp=%d loc=%a,%a,%a h=%a c1=%08lx c2=%08lx wp=%zu pathdone=%d arrived=%d vel=%a blocked=%lu",
                    (unsigned long)x->mTimesUpdated,
                    x->mCommand ? (long)x->mCommand->mType : -1L, x->mActiveGroup != NULL,
                    (double)x->mPhysicalModel.GetLocation().GetX(), (double)x->mPhysicalModel.GetLocation().GetY(),
                    (double)x->mPhysicalModel.GetLocation().GetZ(), (double)x->mHeight,
                    (unsigned long)(uint32_t)c->GetChecksum(1), (unsigned long)(uint32_t)c->GetChecksum(2),
                    x->mWaypoints.size(), x->mIsPathDone ? 1 : 0, x->mHasArrived ? 1 : 0, (double)x->GetVelocity(),
                    x->mBlockingUnit != NULL ? x->mBlockingUnit->mUnitID : 0UL);
            fprintf(f, " wps=");   // the path's points (location, distance, start distance)
            for (size_t w = 0; w < x->mWaypoints.size(); w++)
                fprintf(f, "%a,%a,%a/%a/%a;", (double)x->mWaypoints[w].mLocation.GetX(), (double)x->mWaypoints[w].mLocation.GetY(),
                        (double)x->mWaypoints[w].mLocation.GetZ(), (double)x->mWaypoints[w].mDistance,
                        (double)x->mWaypoints[w].mStartDistance);
            for (long q = 0; q < (long)x->GetNumGoalQueues(); q++) {
                std::deque<EEUnitGoal*>& theGoals = x->GetQueueGoals(q);
                for (size_t g = 0; g < theGoals.size(); g++)
                    fprintf(f, " g%ld:%s:%08lx", q, typeid(*theGoals[g]).name(),
                            (unsigned long)(uint32_t)theGoals[g]->GetOutOfSynchChecksum());
            }
            // what the raw bytes hold only as pointers: the contacts, the last attacker, the area effects
            fprintf(f, "\n  refs contacts=");
            for (UListNode<EEComplexUnit*>* n = x->mContacts.mHead; n != NULL; n = n->mNext)
                fprintf(f, "%lu,", n->mData != NULL ? (unsigned long)n->mData->GetUnitID() : 0UL);
            fprintf(f, " attacker=%lu owner=%lu holder=%lu ae=%zu/%zu", x->mLastAttacker != NULL ? (unsigned long)x->mLastAttacker->GetUnitID() : 0UL,
                    x->mOwner != NULL ? (unsigned long)x->mOwner->GetUnitID() : 0UL,
                    x->mHolder != NULL ? (unsigned long)x->mHolder->GetUnitID() : 0UL,
                    x->mAreaEffects.mItems.size(), x->mAffectingAreaEffects.mItems.size());
            for (size_t a = 0; a < x->mAffectingAreaEffects.mItems.size(); a++) {
                EEAreaEffect* e = x->mAffectingAreaEffects.mItems[a];
                fprintf(f, " %lu", e->mUnit != NULL ? (unsigned long)e->mUnit->GetUnitID() : 0UL);
            }
            for (size_t a = 0; a < x->mAreaEffects.mItems.size(); a++)
                fprintf(f, " own@%a,%a", (double)x->mAreaEffects.mItems[a]->mLocation.mX, (double)x->mAreaEffects.mItems[a]->mLocation.mY);
            fprintf(f, "\n  raw ");   // the unit's bytes (pointers differ by run; build/clang/layout.json names offsets)
            for (size_t b = 0; b < sizeof(EEComplexUnit); b++)
                fprintf(f, "%02x", reinterpret_cast<const unsigned char*>(c)[b]);
            fprintf(f, "\n");
            for (long q = 0; q < (long)x->GetNumGoalQueues(); q++) {   // each goal's own out-of-synch dump
                std::deque<EEUnitGoal*>& theGoals = x->GetQueueGoals(q);
                for (size_t g = 0; g < theGoals.size(); g++) {
                    fprintf(f, "  unit %lu goal %ld.%zu\n", u->GetUnitID(), q, g);
                    theGoals[g]->DumpOutOfSynch(f);
                }
            }
            continue;
        }
        fprintf(f, "\n  rawu ");   // a simple unit's bytes (layout.json "EEUnit")
        for (size_t b = 0; b < sizeof(EEUnit); b++)
            fprintf(f, "%02x", reinterpret_cast<const unsigned char*>(u)[b]);
        fprintf(f, "\n");
    }
    // the units of each map region in order (RTSRegionTable: FindImpactUnit and the target searches walk them)
    if (gWorld.mMap != NULL && gWorld.mMap->mUnitRegions != NULL) {
        RTSRegionTable<EEComplexUnit*>* theTable = gWorld.mMap->mUnitRegions;
        for (unsigned long r = 0; r < theTable->mRegionsX * theTable->mRegionsY; r++) {
            if (theTable->mRegions[r].size() < 2)
                continue;
            fprintf(f, "REGION %lu:", r);
            for (size_t k = 0; k < theTable->mRegions[r].size(); k++)
                fprintf(f, " %lu", theTable->mRegions[r][k] != NULL ? (unsigned long)theTable->mRegions[r][k]->mUnitID : 0UL);
            fprintf(f, "\n");
        }
    }
    fclose(f);
}

std::vector<std::pair<long, std::string>> sDumps;   // --dump-at MS=FILE, in game-ms order
size_t sNextDump = 0;
std::string sDumpLoad;                               // --dump-load FILE

// --dump-tick MS=FILE (TEST ONLY): the state dump after the first physics tick at or after game ms MS, in a continued
// game and in a loaded one alike (gPhysicsAfterTickHook, ee/EEPhysics.h: after every physics tick of the headless
// loop, where --dump-at writes after a communications tick's last)
std::vector<std::pair<long, std::string>> sTickDumps;   // in game-ms order
size_t sNextTickDump = 0;

void TickDumpHook()
{
    while (sNextTickDump < sTickDumps.size() && gPhysics->mGameTime >= sTickDumps[sNextTickDump].first)
        DumpState(sTickDumps[sNextTickDump++].second);
}

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

// the physics thread, after each communications tick, under the world's write lock
// --save-at: the saves' names in data\Saved Games (the game's folder; copied out at the end of the run)
std::string SaveName(size_t inIndex)
{
    return "EE-SIM save " + std::to_string(inIndex);
}

std::string SavePath(const std::string& inName)
{
    return std::string("data\\Saved Games\\") + inName + (const char*)gSavedGameExtension;
}

size_t sNextSave = 0;

// the save requests of --save-at, as EEPhysics::AutoSave sends one (but quiet and not an autosave: no autosave is
// trimmed, the preferences are not written). It runs at the start of the next communications tick that carries it
// (EEPhysics::Tick: DrainInput executes it, ServeLoadSaveRequests saves), between two communications ticks: the
// loaded game goes on from there.
void RequestSaves(long inTime)
{
    while (sNextSave < sOptions.mSaves.size() && inTime >= sOptions.mSaves[sNextSave].first) {
        std::string theName = SaveName(sNextSave) + (const char*)gSavedGameExtension;
        NEMessage* theSave = new EEMLoadOrSaveGame(UWideString(theName.c_str()), TMGetTime(), 0, true,
                                                   gCommunications->GetNumberOfCTsProcessedInPhysics(), 0);
        static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(theSave, 15000, 12);
        theSave->Dereference();
        Note("SAVE %zu requested at game ms %ld PT %lu", sNextSave, inTime, gPhysics->mNumTicksDone);
        sNextSave++;
    }
}

bool CopyAFile(const std::string& inFrom, const std::string& inTo)
{
    FILE* a = fopen(inFrom.c_str(), "rb");
    if (!a)
        return false;
    FILE* b = fopen(inTo.c_str(), "wb");
    if (!b) {
        fclose(a);
        return false;
    }
    char theBuffer[65536];
    size_t n;
    bool theOK = true;
    while ((n = fread(theBuffer, 1, sizeof(theBuffer), a)) > 0)
        theOK = theOK && fwrite(theBuffer, 1, n, b) == n;
    fclose(a);
    return fclose(b) == 0 && theOK;
}

void AfterTick()
{
    ChecksumLine();
    long theTime = gPhysics->mGameTime;
    RequestSaves(theTime);
    while (sNextDump < sDumps.size() && theTime >= sDumps[sNextDump].first)
        DumpState(sDumps[sNextDump++].second);
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
    // TEST ONLY: the formation and combat sweeps of the simulation capture (ee/EEFormation.h, above), every five game
    // minutes
    static long sNextSweepMs = 300000;
    if (EESimCaptureRecord::Active() && theTime >= sNextSweepMs) {
        sNextSweepMs += 300000;
        EEFormationCaptureSweep();
        EECombatCaptureSweep();
        EETechCaptureSweep();   // the tech trees' sweep (ee/EETechCapture.h)
        EECalamityCaptureSweep();   // CanCastAt over every calamity class (ee/EETechCapture.h)
        EEMessageCaptureDecisionSweep();   // the Execute decisions' sweep (ee/EEMessageCapture.h)
        EESelectPickSweep();   // the box selection's pick over every player's units (ee/EEUnitStore.h)
        EEFillSweep();   // the active groups' Fill test over every player's units (ee/EEUnitStore.h)
        EEGroupCommandSweep();   // the group commands' bodies over the dry host (ee/EEUnitHost.h)
        static bool sMessageSweepDone = false;   // the game messages' sweep, once (ee/EEMessageCapture.h)
        if (!sMessageSweepDone) {
            sMessageSweepDone = true;
            EEMessageCaptureSweep();
        }
    }
    // TEST ONLY: --calamities casts every calamity class in turn, every five game minutes (ee/EETechCapture.h)
    static long sNextCastMs = 300000;
    static long sCasts = 0;
    if (sOptions.mCalamities && theTime >= sNextCastMs) {
        sNextCastMs += 300000;
        EETechCaptureCastCalamities(sCasts++);
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
    // the end of the run (tools/harness/ai_game.py's rules)
    if (gPhysics->mGameOver) {
        FinishGame("game-end");
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
                FinishGame("last-computer-standing");
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
            FinishGame("game-end");
            return;
        }
    }
    if (theTime >= (long)(sOptions.mGameSeconds * 1000))
        FinishGame("time-cap");
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

void Select(long inControl, const std::string& inValue, const std::string& inLabel)
{
    std::vector<std::string> theLines;
    long theSelection = -1;
    if (!ComboState(inControl, theLines, theSelection))
        Fail(inLabel + ": control " + std::to_string(inControl) + " is not a combo on the lobby");
    long theIndex = Resolve(inValue, theLines, inLabel);
    if (theSelection != theIndex) {
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
        SimSleep(200);   // the lobby's update round trip through the local server
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
    SimSleep(1500);   // the local server's readiness and queued lobby updates (the harness waits 3 s)
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
    SimSleep(1000);
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
         (sOptions.mNoHumanDefeat ? "true" : "false") + ", \"cheats\": " +
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
            char b[512];
            snprintf(b, sizeof(b),
                     "%s{\"index\": %lu, \"human\": %s, \"result\": %lu, \"epoch\": %ld, \"population\": %ld, "
                     "\"victory_units\": %ld, \"citizens\": %ld, \"army\": %ld, \"buildings\": %ld, "
                     "\"strength\": %ld, \"value\": %ld, \"resources\": {\"food\": %ld, \"wood\": %ld, \"stone\": %ld, \"gold\": %ld, \"iron\": %ld}}",
                     k ? ", " : "", p.mIndex, p.mHuman ? "true" : "false", p.mResult, p.mEpoch, p.mPopulation,
                     p.mVictoryUnits, p.mCitizens, p.mArmy, p.mBuildings, p.mStrength, p.mValue, p.mResources[0], p.mResources[1],
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

void Fail(const std::string& inError)
{
    gPlatSimRun = 0;
    Note("FAILED: %s", inError.c_str());
    WriteReport(2, inError);
    Quit(2);
}

// ------------------------------------------------------------------------------------------------ the load

// the control of the form on top whose trigger event is a T (-1: none)
template <class T> long ControlWithEvent(unsigned long inForm)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != inForm)
        return -1;
    for (unsigned long k = 0; k < theForm->GetControlCount(); k++) {
        UIControl* theControl = theForm->GetControl(k);
        if (theControl && dynamic_cast<T*>(theControl->GetTriggerEvent(kTriggerEvent)) != NULL)
            return (long)k;
    }
    return -1;
}

const unsigned long kFormLoad = 13;
const char* kLoadName = "EE-SIM load";

// the load screen's file list (NULL: not on top)
UILBString* LoadList()
{
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != kFormLoad || (unsigned long)kSPLoadGameFileListBox >= theForm->GetControlCount())
        return NULL;
    return dynamic_cast<UILBString*>(theForm->GetControl(kSPLoadGameFileListBox));
}

// the line of kLoadName in the load screen's file list (-1: not listed) and the list's selection
long LoadLine(unsigned long& outSelection)
{
    UILock theLock;
    outSelection = 0xffffffffUL;
    UILBString* theList = LoadList();
    if (!theList)
        return -1;
    theList->GetCurrentSelection(outSelection);
    for (unsigned long k = 0; k < theList->GetLineCount(); k++) {
        const char* theText = (const char*)theList->GetData(k);
        if (theText && !strcmp(theText, kLoadName))
            return (long)k;
    }
    return -1;
}

// --load-save: the file staged as data\Saved Games\EE-SIM load.ees, then the single-player form's Load Game, the file
// selected in the load screen's list (UIListBox::ChangeSelectionIndex, as the screen's own keys do), Load
// (EEUIESPLELoadFile: the file validated, EEMStartLoading of a saved game)
void LoadFromMenu()
{
    std::string theStaged = SavePath(kLoadName);
    remove(theStaged.c_str());
    if (!CopyAFile(sOptions.mLoadSave, theStaged))
        Fail("--load-save: cannot copy " + sOptions.mLoadSave + " to " + theStaged);
    long theButton = ControlWithEvent<EEUIESPELoadGame>(kFormSingle);
    if (theButton < 0)
        Fail("--load-save: no Load Game button on the single-player form");
    Click(kFormSingle, (unsigned long)theButton);
    WaitForm(kFormLoad);
    unsigned long theSelection = 0;
    WaitFor("the load screen to list the file", 60, [&] { return LoadLine(theSelection) >= 0; });
    long theLine = LoadLine(theSelection);
    {
        UILock theLock;
        UILBString* theList = LoadList();
        if (!theList)
            Fail("--load-save: the load screen went away");
        theList->ChangeSelectionIndex((unsigned long)theLine, true, false);
    }
    WaitFor("the load selection to stick", 20,
            [&] { return LoadLine(theSelection) == theLine && theSelection == (unsigned long)theLine; });
    SimSleep(200);
    long theLoad = ControlWithEvent<EEUIESPLELoadFile>(kFormLoad);
    if (theLoad < 0)
        Fail("--load-save: no Load button on the load screen");
    Note("UI load %s", sOptions.mLoadSave.c_str());
    Click(kFormLoad, (unsigned long)theLoad);
}

// ------------------------------------------------------------------------------------------------ the driver

void Drive()
{
    int theCode = 0;
    std::string theError;
    {
        std::vector<std::string> theProblems;
        EERulesOverrideProblems(theProblems);
        if (!theProblems.empty()) {
            std::string theText;
            for (const std::string& p : theProblems)
                theText += (theText.empty() ? "" : "; ") + p;
            Fail("--set did not apply: " + theText);
        }
        for (size_t k = 0; k < sOptions.mSaves.size(); k++)
            remove(SavePath(SaveName(k)).c_str());   // a stage is reused: no file of an earlier run
        WaitForm(kFormMain, 300);
        Note("UI main menu");
        Click(kFormMain, 0);
        WaitForm(kFormSingle);
        bool theLoaded = !sOptions.mLoadSave.empty();
        if (theLoaded)
            LoadFromMenu();
        else {
            Click(kFormSingle, 1);
            WaitForm(kFormLobbyID);
            Note("UI lobby");
            SetUpLobby();
            Click(kFormLobbyID, 10);
            Note("UI start");
        }
        long theExpected = sOptions.mComputers + 2;
        WaitFor("the game", 600, [&] {
            return TopForm() == kFormGame && gPhysics != NULL && gPhysics->mTimerRunning &&
                   (theLoaded ? gWorld.GetNumPlayers() > 1 : (long)gWorld.GetNumPlayers() == theExpected) &&
                   gWorld.GetMap() != NULL;
        });
        SimSleep(500);
        if (theLoaded) {
            // the saved players carry their flags (the harness's loaded game: no civilization pre-empt)
            gWorld.mLock->WriteLock();
            sResults.assign(gWorld.GetNumPlayers(), 0);
            for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++)
                if (gWorld.GetPlayer(i) != NULL)
                    sResults[i] = gWorld.GetPlayer(i)->mGameResult;
            Note("LOADED %s: game ms %ld PT %lu", sOptions.mLoadSave.c_str(), gPhysics->mGameTime,
                 gPhysics->mNumTicksDone);
            if (sChecksumFile) {   // the loaded state (not a "CT" line: the comparisons skip it)
                fprintf(sChecksumFile, "LOADED ");
                ChecksumFields(sChecksumFile);
            }
            if (!sDumpLoad.empty())
                EEAfterLoad([] { DumpState(sDumpLoad); });   // at the first tick, after the load's own fix-ups
            gWorld.mLock->WriteUnlock();
        } else {
            // the idle human as the harness's civilization pre-empt leaves it (ai_game.py: the civilization builder
            // opened and cancelled): the civilization timer sends its messages but opens no form, and the computers'
            // civilizations are not locked at its expiry
            gWorld.mLock->WriteLock();
            EEPlayer* theHuman = gWorld.GetCurrentPlayer();
            theHuman->mCivilizationChosen = true;
            theHuman->mCivilizationLocked = true;
            sResults.assign(gWorld.GetNumPlayers(), 0);
            gWorld.mLock->WriteUnlock();
        }
        Note("GAME players %lu local %ld: running", gWorld.GetNumPlayers(), gWorld.mCurrentPlayer);
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
    if (theError.empty() && gPhysics != NULL) {
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
        Note("SIM %s: PT %lu game s %ld units%s; %.1f wall s in game (%.0fx real time)",
             theError.empty() ? sOutcome.c_str() : "error", theLast ? theLast->mPT : 0UL,
             theLast ? theLast->mGameMs / 1000 : 0L, theUnits.c_str(), theGame,
             theLast && theGame > 0 ? theLast->mGameMs / 1000.0 / theGame : 0.0);
    }
    for (size_t k = 0; k < sOptions.mSaves.size(); k++) {
        if (k >= sNextSave)
            Note("SAVE %zu (game ms %ld) not requested: the run ended before", k, sOptions.mSaves[k].first);
        else if (!CopyAFile(SavePath(SaveName(k)), sOptions.mSaves[k].second)) {
            Note("SAVE %zu: %s not written or not copied to %s", k, SavePath(SaveName(k)).c_str(),
                 sOptions.mSaves[k].second.c_str());
            theCode = theCode ? theCode : 5;
        }
    }
    WriteReport(theCode, theError);
    Quit(theCode);
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
    "  --checksum-trace FILE  one line of state hashes per communications tick (tick, random seed, out-of-synch\n"
    "                         checksum, units, AI); reading them changes nothing (tools/sim_oracle.py ticks)\n"
    "  --autosave-ms N        the game's autosave period in PT time (default 0: none; the game and the harness: 600000)\n"
    "  --save-at MS=FILE      save the game at the first communications tick at or after game ms MS (repeatable); the\n"
    "                         file is copied to FILE at the end of the run\n"
    "  --load-save FILE       load this saved game (the game's Load Game screen) instead of starting from the lobby\n"
    "  --dump-at MS=FILE      write the units' and players' state at the first tick at or after game ms MS (repeatable)\n"
    "  --dump-load FILE       with --load-save: the same of the loaded state (at its first tick, before it runs)\n"
    "  --dump-tick MS=FILE    the same after the first physics tick at or after game ms MS (repeatable; --dump-at\n"
    "                         writes after a communications tick's last physics tick)\n"
    "  --rng-trace MS=FILE    every draw of the physics random stream after game ms MS with its callers (exe-relative\n"
    "                         addresses, tools/sim.py resolve), at most 50,000 draws\n"
    "  --port-offset N        move the game's TCP ports 33334/33335 by N (default: none fixed, the system picks a\n"
    "                         free port at each bind)\n"
    "  --calamities           TEST ONLY: every five game minutes the computer players cast the next eighth of the\n"
    "                         calamity classes (the capture's calamity game; ee/EETechCapture.h)\n"
    "  --quiet                no progress on stderr\n";

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
#endif

void EESimGameEnded(int inResult);

// EESimMain: EE-SIM.exe's entry (EESimMain.cpp's main): the options, the switches, then the game's own WinMain. On
// POSIX the game's main calls it for "ee-aoc --sim ..." and goes on with the game when it returns -1.
int EESimMain(int argc, char** argv)
{
    sStartWall = std::chrono::steady_clock::now();
    std::string theData, theConfig;
    bool theTraceOn = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                fprintf(stderr, "EE-SIM: %s needs a value\n", a.c_str());
                exit(64);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") {
            fputs(kUsage, stdout);
            return 0;
        } else if (a == "--seed")
            sOptions.mSeed = strtoul(value().c_str(), NULL, 0);
        else if (a == "--ai")
            sOptions.mComputers = atol(value().c_str());
        else if (a == "--map-size")
            sOptions.mMapSize = value();
        else if (a == "--map-type")
            sOptions.mMapType = value();
        else if (a == "--difficulty")
            sOptions.mDifficulty = value();
        else if (a == "--lobby") {
            std::string v = value();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                fprintf(stderr, "EE-SIM: --lobby CONTROL=VALUE\n");
                return 64;
            }
            sOptions.mLobby.push_back({atol(v.substr(0, eq).c_str()), v.substr(eq + 1)});
        } else if (a == "--game-seconds")
            sOptions.mGameSeconds = atof(value().c_str());
        else if (a == "--sample-seconds")
            sOptions.mSampleSeconds = atof(value().c_str());
        else if (a == "--wall-timeout")
            sOptions.mWallTimeout = atof(value().c_str());
        else if (a == "--pt-rate")
            sOptions.mPTRate = atol(value().c_str());
        else if (a == "--human-defeat")
            sOptions.mNoHumanDefeat = false;
        else if (a == "--no-human-defeat")
            sOptions.mNoHumanDefeat = true;
        else if (a == "--cheats")
            sOptions.mCheats = value();
        else if (a == "--personality")
            sOptions.mPersonality = value();
        else if (a == "--engine")
            sOptions.mEngine = value();
        else if (a == "--set") {
            std::string v = value(), theError;
            if (!EERulesAddOverride(v, theError)) {
                fprintf(stderr, "EE-SIM: --set %s\n", theError.c_str());
                return 64;
            }
            sOptions.mSets.push_back(v);
        } else if (a == "--data")
            theData = Absolute(value());
        else if (a == "--config")
            theConfig = Absolute(value());
        else if (a == "--report")
            sOptions.mReport = Absolute(value());
        else if (a == "--trace")
            sOptions.mTrace = Absolute(value());
        else if (a == "--ai-trace")
            theTraceOn = true;
        else if (a == "--log")
            sOptions.mLog = Absolute(value());
        else if (a == "--checksum-trace")
            sOptions.mChecksumTrace = Absolute(value());
        else if (a == "--quiet")
            sOptions.mQuiet = true;
        else if (a == "--calamities")
            sOptions.mCalamities = true;
        else if (a == "--port-offset")
            sOptions.mPortOffset = atol(value().c_str());
        else if (a == "--autosave-ms")
            sOptions.mAutoSaveMs = atol(value().c_str());
        else if (a == "--save-at") {
            std::string v = value();
            size_t eq = v.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 == v.size()) {
                fprintf(stderr, "EE-SIM: --save-at MS=FILE\n");
                return 64;
            }
            sOptions.mSaves.push_back({atol(v.substr(0, eq).c_str()), Absolute(v.substr(eq + 1))});
        } else if (a == "--load-save")
            sOptions.mLoadSave = Absolute(value());
        else if (a == "--dump-at") {
            std::string v = value();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                fprintf(stderr, "EE-SIM: --dump-at MS=FILE\n");
                return 64;
            }
            sDumps.push_back({atol(v.substr(0, eq).c_str()), Absolute(v.substr(eq + 1))});
        } else if (a == "--dump-load")
            sDumpLoad = Absolute(value());
        else if (a == "--dump-tick") {
            std::string v = value();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                fprintf(stderr, "EE-SIM: --dump-tick MS=FILE\n");
                return 64;
            }
            sTickDumps.push_back({atol(v.substr(0, eq).c_str()), Absolute(v.substr(eq + 1))});
            gPhysicsAfterTickHook = TickDumpHook;
        }
        else if (a == "--rng-trace") {
            std::string v = value();
            size_t eq = v.find('=');
            if (eq == std::string::npos) {
                fprintf(stderr, "EE-SIM: --rng-trace MS=FILE\n");
                return 64;
            }
            sRngTraceMs = atol(v.substr(0, eq).c_str());
            sRngTraceFile = Absolute(v.substr(eq + 1));
            gURandomTraceHook = RngTraceHook;
        } else {
            fprintf(stderr, "EE-SIM: unknown option %s (--help)\n", a.c_str());
            return 64;
        }
    }
    if (sOptions.mComputers < 1 || sOptions.mComputers > 7 || sOptions.mPTRate < 1 || sOptions.mPTRate > 250 ||
        !(sOptions.mGameSeconds > 0) || !(sOptions.mSampleSeconds > 0)) {
        fprintf(stderr, "EE-SIM: an option is out of range (--help)\n");
        return 64;
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
    if (!sOptions.mTrace.empty()) {
        sTraceFile = fopen(sOptions.mTrace.c_str(), "w");
        if (!sTraceFile) {
            fprintf(stderr, "EE-SIM: cannot write %s\n", sOptions.mTrace.c_str());
            return 66;
        }
    }
    if (!sOptions.mLog.empty())
        sLogFile = fopen(sOptions.mLog.c_str(), "w");
    if (!sOptions.mChecksumTrace.empty()) {
        sChecksumFile = fopen(sOptions.mChecksumTrace.c_str(), "w");
        if (!sChecksumFile) {
            fprintf(stderr, "EE-SIM: cannot write %s\n", sOptions.mChecksumTrace.c_str());
            return 66;
        }
    }
    std::stable_sort(sOptions.mSaves.begin(), sOptions.mSaves.end(),
                     [](const std::pair<long, std::string>& a, const std::pair<long, std::string>& b) {
                         return a.first < b.first;
                     });
    gPlatSimMode = 1;
    gPlatSimSeedSet = sOptions.mLoadSave.empty() ? 1 : 0;   // a loaded game: the file's seed, as the game loads it
    gPlatSimSeed = (uint32_t)sOptions.mSeed;
    gPlatSimNoHumanDefeat = sOptions.mNoHumanDefeat ? 1 : 0;
    // no fixed ports by default: a pair checked free at start could be taken by another process (another sim, a
    // connection's ephemeral port) before the random-map screen's local server bound it; that server then was not
    // published and its first input crashed (NERecordPlayback::Process on its NULL recorder)
    gPlatSimPortOffset = sOptions.mPortOffset >= 0 ? (int)sOptions.mPortOffset : kPlatSimAnyPort;
    PlatDebugSetHandler(DebugHandler);
#ifdef _WIN32
    AddVectoredExceptionHandler(1, EESimFault);
#endif
    if (gPlatSimPortOffset == kPlatSimAnyPort)
        Note("EE-SIM seed %lu, %ld computers, cap %.0f game s, ports any free", sOptions.mSeed, sOptions.mComputers,
             sOptions.mGameSeconds);
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
