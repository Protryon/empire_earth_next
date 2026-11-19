// TEST ONLY: the headless simulation's host (EE-SIM.exe; platform/PlatSim.h). Port addition, no original address.
//
// EE-SIM.exe is the game (every unit of EE-AOC.exe) with another entry (EESimMain.cpp: main -> EESimMain). It runs the
// game's own start-up, menus, lobby, loading and simulation with the front end stubbed (platform/PlatSim.h: a hidden
// window, no rendering, no sound) and, instead of a player, a driver. The driver is Rust (ee_game sim_driver: its
// options, the menus and the lobby as the harness drives them, the run on the physics thread's after-tick, the
// samples, the end of the run, the JSON report, --games' batch; EE-SIM --help). This file is its host
// (EeSimDriverHost): the UI's controls under the UI lock, the world lock, the platform switches, the C library's
// streams and environment, the idle human's set-up, the save requests, --flare and --volcano, and the TEST ONLY
// parts that read or drive C++ objects: the simulation capture's sweeps (the combat formulas', the unit-list
// target search's, the missiles'), the per-CT checksum line's fields (--checksum-trace), the state dump (--dump-at,
// --dump-load, --dump-tick) and the physics random stream's trace (--rng-trace); and the fault handler.
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
#include "ee/EEUnitVectorItem.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
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
#include "ee/EEScenarioManager.h"
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
#include "ee/EEStartingResources.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"   // EE_SCREENS_SP_LOAD_GAME (ee_screens menus.rs)
#include "rust/game/sim_driver.h"   // the driver (ee_game sim_driver)
#include "rust/sim/combat.h"
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
#include "ee/EEAppCapture.h"
#include "ee/EESightCapture.h"
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
#include "ee/EECombatGoals.h"
#include "ee/EEAreaUtilities.h"     // EECanHeal (the sweep)
#include "ee/EEUnitUtilities.h"     // EESearchTargetAlongList (the end sweep)
#include "lle/GEQuaternion.h"
#include "ee/EEPhysicsModel.h"      // the missile sweep's flight steps
#include "ee/EEUnitClassKinds.h"
void EEEffectCaptureSweep();               // the 3D effects' sweep (ee/EEEffectManager.h)
void EEPhysicsUnitSweep(long inCall);      // the units sweep (Physics Thread/EEPhysicsSweep.cpp)
void EEActiveGroupCaptureSweep();          // the active groups' sweep (Units/EEActiveGroupSweep.cpp)
#include "ee/EEUGRust.h"
#include "ee/EETechCapture.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EESimEditor.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "lle/USingleton.h"
#include "platform/PlatSim.h"
#include "platform/PlatTime.h"
#include "rust/game/screens.h"
#include "rust/sim/combat.h"

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

// TEST ONLY: the missile sweep (CBMA, CBMH and the impact tests' records). No game of the capture profiles flies a
// missile (EEPMMissile: the missile and torpedo classes), so with EE_SIM_CAPTURE and --missile-sweep (the capture's
// missile profile: the oracle's game, its own golden digests of those tags), when the game ends (after the
// unit-list search's sweep), each player's newest four armed units on the map launch one missile of every missile and
// torpedo class at the newest armed unit of the next player: the missile is created on the unit's tile 1.5 above it,
// its launcher class the first class that fires it (else the unit's), alternately at the unit (attack unit) and at its
// position (attack ground), launched as LaunchShot launches it (LaunchProjectile; a class without a launch delay gets
// 0.3 s of boost so both steps fly) and put on the map as FlushWorld puts it (the queued placements, the pending
// units). Then up to 120 ticks of the missile's update without its effects and sound: the physics model's step, the
// flight time's count down, the water and ground impact tests (the first true ends it). Only the flight steps' and
// impact tests' records are written, and no other thread's while it runs; the game is over and nothing simulates
// after it.
static void EECombatMissileSweep()
{
    const unsigned int kTags[] = {EE_COMBAT_TAG('C', 'B', 'M', 'A'), EE_COMBAT_TAG('C', 'B', 'M', 'H'),
                                  EE_COMBAT_TAG('C', 'B', 'G', 'I'), EE_COMBAT_TAG('C', 'B', 'W', 'I')};
    EESimCaptureRecord::Only(kTags, 4);
    EESimCaptureRecord::Exclusive(true);
    std::vector<EEUnitClass*> theClasses;
    std::vector<EEUnitClass*> theLaunchers;
    for (EEUnitClass* theClass : gUnitClassTable) {
        if (theClass == NULL
            || (theClass->mCategory != kUnitCategoryMissile && theClass->mCategory != kUnitCategoryTorpedo))
            continue;
        EEUnitClass* theLauncher = NULL;
        for (EEUnitClass* theOther : gUnitClassTable)
            if (theOther != NULL && theOther->mProjectileClass == theClass) {
                theLauncher = theOther;
                break;
            }
        theClasses.push_back(theClass);
        theLaunchers.push_back(theLauncher);
    }
    std::vector<std::vector<EEComplexUnit*>> theArmed;
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        std::vector<EEComplexUnit*> theAll;
        for (EEUnit* theUnit = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theUnit != NULL;
             theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL && theUnit->mHitPoints > 0 && theUnit->mStats != NULL && theComplex->mTileNode != NULL
                && theComplex->GetTileX() != -10000 && theComplex->mStats->GetAttack() > 0.0f
                && theComplex->mUnitClass->mCategory != kUnitCategoryMissile
                && theComplex->mUnitClass->mCategory != kUnitCategoryTorpedo)
                theAll.push_back(theComplex);
        }
        if (!theAll.empty())
            theArmed.push_back(theAll);
    }
    for (size_t p = 0; theArmed.size() > 1 && p < theArmed.size(); p++) {
        EEComplexUnit* theTarget = theArmed[(p + 1) % theArmed.size()].back();
        const std::vector<EEComplexUnit*>& theUnits = theArmed[p];
        size_t theFirst = theUnits.size() > 4 ? theUnits.size() - 4 : 0;
        for (size_t i = theFirst; i < theUnits.size(); i++) {
            EEComplexUnit* a = theUnits[i];
            for (size_t k = 0; k < theClasses.size(); k++) {
                if (theTarget->mHitPoints <= 0 || theTarget->GetTileX() == -10000)
                    break;
                EEUnitClass* theClass = theClasses[k];
                EEUnit* theCreated = theClass->CreateUnit(a->GetTileX(), a->GetTileY(), a->mPlayer->GetPlayerID(), false, a);
                EEComplexUnit* m = theCreated != NULL ? theCreated->GetComplexUnit() : NULL;
                if (m == NULL)
                    continue;
                GETransformation theStart(a->mPhysicalModel.GetLocation());
                theStart.SetZ(theStart.GetZ() + 1.5f);
                m->mPhysicalModel.SetLocation(theStart);
                m->SetLauncherClass(theLaunchers[k] != NULL ? theLaunchers[k] : a->mUnitClass);
                m->SetOwner(a);
                GE3DPoint theAt;
                theTarget->GetPosition(theAt);
                EEUnitGoal* theGoal;
                if (((i + k) & 1) == 0)
                    theGoal = EEUGRust::NewAttackUnit(m, theTarget, false, false);
                else
                    theGoal = EEUGRust::NewAttackGround(theAt, false);
                m->SetGoal(theGoal, false, false);
                m->SetVelocity(a->GetVelocity());
                theClass->LaunchProjectile(m, theAt);
                if (m->mFlightTime == 0)
                    m->SetFlightTime(300);
                // onto the map, as the next tick's EEPhysics::FlushWorld puts a launched shot
                gWorld.FlushQueuedPlacements();
                a->mPlayer->AddPendingUnits(false, true);
                for (int t = 0; t < 120 && m->mTileX != -10000 && m->mGoalState != NULL; t++) {
                    theClass->mPhysicsModel->Step0(m);
                    long theElapsed = gPhysics->mTickTimeMs != 0 ? gPhysics->mTickTimeMs : 1;
                    if (m->mFlightTime != 0)
                        m->SetFlightTime(m->mFlightTime > theElapsed ? m->mFlightTime - theElapsed : 0);
                    if (theClass->CheckWaterImpact(m) || theClass->CheckGroundImpact(m))
                        break;
                }
            }
        }
    }
    EESimCaptureRecord::Exclusive(false);
    EESimCaptureRecord::Only(0);
}

namespace {

// ------------------------------------------------------------------------------------------------ the checksum line

// The out-of-synch checksum's parts read for the per-CT line: unit hit points and tiles (1), positions, heights,
// commands and update counts (2), calamities (0x10), air traffic (0x40), the continent manager's checksum (0x100).
// Left out because reading them is not free of effects: 4, the map's window checksum, advances the map's checksum
// cursor (EEMap::ComputeChecksum); and 0x20, the goals, copies each goal queue and dereferences goal members the
// multiplayer game (flag 1 only) never reads. The game's own OOS check uses flag 1.
const unsigned long kChecksumFlags = 0x1 | 0x2 | 0x10 | 0x40 | 0x100;

// --checksum-trace's fields (the driver writes "CT <n> " or "LOADED " before them; the physics thread, under the
// world's write lock, after the CT's last physics tick). Only reads: no random draws, no caches filled;
// EEPhysics::mLastOutOfSynchChecksum, the one member GetOutOfSynchChecksum writes (and nothing reads), is put back:
//   pt=<ticks done> tick=<current tick> ms=<game ms> rng=<physics random seed>
//   oos=<GetOutOfSynchChecksum(kChecksumFlags)> units=<count>/<sum of GetChecksum(3)> ai=<EECPOutOfSynchChecksum>
//   inputs=<user inputs executed>
std::string ChecksumText()
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
    char theText[512];
    snprintf(theText, sizeof(theText), "pt=%lu tick=%lu ms=%ld rng=%08lx oos=%08lx units=%lu/%08lx ai=%08lx inputs=%ld\n",
             gPhysics->mNumTicksDone, gPhysics->mCurrentTick, gPhysics->mGameTime,
             (unsigned long)(uint32_t)gPhysics->mRandom.GetSeed(), (unsigned long)(uint32_t)theChecksum, theCount,
             (unsigned long)(uint32_t)theUnits, (unsigned long)(uint32_t)EECPOutOfSynchChecksum(),
             gPhysics->mInputMessagesProcessed);
    return theText;
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
    fputs(ChecksumText().c_str(), f);
    fprintf(f, "physics clock=%a tickrandom=%ld tickstoprocess=%ld\n", (double)gPhysics->mGameClock,
            (long)gPhysics->mTickRandom, (long)gPhysics->mTicksToProcess);
    {   // the stepped units and the (de)activation queues, in order (EEPWorldHelper's unit order)
        const char* theNames[3] = {"UPDATE", "ACTIVATE", "DEACTIVATE"};
        for (uint32_t l = 0; l < 3; l++) {   // (ee_sim app active.rs)
            std::vector<uint32_t> theIDs(ee_app_active_list(l, NULL, 0));
            ee_app_active_list(l, theIDs.data(), theIDs.size());
            fprintf(f, "%s %zu:", theNames[l], theIDs.size());
            for (size_t i = 0; i < theIDs.size(); i++)
                fprintf(f, " %lu", EEUnitHandle<EEComplexUnit>::FromSavedID(theIDs[i])->mUnitID);
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
            for (unsigned long v = 0; v < theTown->GetHullPointCount(); v++)
                fprintf(f, "%ld,%ld ", (long)theTown->GetHullPoint(v).GetX(), (long)theTown->GetHullPoint(v).GetY());
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
            fprintf(f, " anim=%lu/%lu/%lu/%d reload=%ld", (unsigned long)x->GetAnimation(), (unsigned long)x->mPhysicalModel.GetFrame(),
                    (unsigned long)x->mAnimationTime, x->mIsAnimationPaused ? 1 : 0, (long)x->mReloadTimer);
            fprintf(f, " wps=");   // the path's points (location, distance, start distance)
            for (size_t w = 0; w < x->mWaypoints.size(); w++)
                fprintf(f, "%a,%a,%a/%a/%a;", (double)x->mWaypoints[w].mLocation.GetX(), (double)x->mWaypoints[w].mLocation.GetY(),
                        (double)x->mWaypoints[w].mLocation.GetZ(), (double)x->mWaypoints[w].mDistance,
                        (double)x->mWaypoints[w].mStartDistance);
            for (long q = 0; q < (long)x->GetNumGoalQueues(); q++) {
                const EEGoalDeque& theGoals = x->GetQueueGoals(q);
                for (size_t g = 0; g < theGoals.size(); g++)
                    fprintf(f, " g%ld:k%u:%08lx", q, (unsigned)theGoals[g]->mKind,
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
                const EEGoalDeque& theGoals = x->GetQueueGoals(q);
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
    // the active calamities, in order: their checksum's fields (EECalamity::GetOutOfSynchChecksum) and their units
    if (EECalamityManager* theCalamities = EECalamityManager::GetInstance()) {
        for (size_t c = 0; c < theCalamities->mCalamities.size(); c++) {
            EECalamity* k = theCalamities->mCalamities[c];
            if (k == NULL)
                continue;
            fprintf(f, "CAL %zu class=%ld sum=%08lx target=%a,%a,%a tile=%lu,%lu int=%ld,%ld ids=%lu,%lu units=%lu,%lu "
                       "player=%lu bools=%d%d%d%d%d floats=%a,%a,%a epi=%a alive=%a discrete=%a elapsed=%a\n",
                    c, k->mClass != NULL ? (long)k->mClass->mClassEnum : -1L, (unsigned long)(uint32_t)k->GetOutOfSynchChecksum(),
                    (double)k->mTargetLocation.GetX(), (double)k->mTargetLocation.GetY(), (double)k->mTargetLocation.GetZ(),
                    (unsigned long)k->mTilePlaceHolder1.mX, (unsigned long)k->mTilePlaceHolder1.mY, k->mIntPlaceHolder1,
                    k->mIntPlaceHolder2, k->mTargetUnitID, k->mOriginatingUnitID,
                    k->mTargetUnit != NULL ? (unsigned long)k->mTargetUnit->GetUnitID() : 0UL,
                    k->mCasterUnit != NULL ? (unsigned long)k->mCasterUnit->GetUnitID() : 0UL, k->mPlayerNumber,
                    k->mBoolPlaceHolder1, k->mBoolPlaceHolder2, k->mInitialized, k->mHasTargetLocation, k->mHasTargetUnit,
                    (double)k->mFloatPlaceHolder1, (double)k->mFloatPlaceHolder2, (double)k->mHeight,
                    (double)k->mEpicenterFactor, (double)k->mTimeAlive, (double)k->mDiscreteUpdateTime,
                    (double)k->mTimeElapsedSinceLastUpdate);
        }
    }
    // the units of each map region in order (RTSRegionTable: FindImpactUnit and the target searches walk them)
    // (ee_combat regions.rs's saved order: the count, then each region's index, size and ids, -1 for none)
    std::vector<uint32_t> theOrder(gWorld.mMap != NULL ? ee_combat_regions_order(gWorld.mMap->mUnitRegions, NULL, 0) : 0);
    if (!theOrder.empty())
        ee_combat_regions_order(gWorld.mMap->mUnitRegions, theOrder.data(), theOrder.size());
    for (size_t k = 1; k < theOrder.size(); k += 2 + theOrder[k + 1]) {
        fprintf(f, "REGION %lu:", (unsigned long)theOrder[k]);
        for (uint32_t n = 0; n < theOrder[k + 1]; n++)
            fprintf(f, " %lu", theOrder[k + 2 + n] != 0xffffffffu ? (unsigned long)theOrder[k + 2 + n] : 0UL);
        fprintf(f, "\n");
    }
    fclose(f);
}

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

const unsigned long kTriggerEvent = 0x3f0;
const unsigned long kFormLobbyID = 26;
const unsigned long kFormLoad = 13, kFormLoadScenario = 4, kFormCampaignSelect = 33;
const unsigned long kCampaignScenarioList = 2;   // the campaign scenario select form's scenario list

struct UILock {
    UILock() { gUIManager->LockUIForCopyData(); }
    ~UILock() { gUIManager->UnlockUIForCopyData(); }
};

size_t PutText(const std::string& inText, char* outBuffer, size_t inCapacity)
{
    if (inCapacity > 0) {
        size_t n = inText.size() < inCapacity - 1 ? inText.size() : inCapacity - 1;
        memcpy(outBuffer, inText.data(), n);
        outBuffer[n] = 0;
    }
    return inText.size();
}

unsigned int HostTopForm()
{
    if (gUIManager == NULL)
        return 0;
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    return theForm ? (unsigned int)theForm->GetFormID() : 0;
}

size_t HostTopFormClass(char* outBuffer, size_t inCapacity)
{
    std::string theClass = "none";   // (an id -1 form: a message box is EEUIFError)
    if (gUIManager) {
        UILock theLock;
        UIForm* theForm = gUIManager->GetCurrentForm();
        if (theForm)
            theClass = typeid(*theForm).name();
    }
    return PutText(theClass, outBuffer, inCapacity);
}

int HostClick(unsigned int inForm, unsigned int inControl)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != inForm || inControl >= theForm->GetControlCount())
        return 1;
    UIControl* theControl = theForm->GetControl(inControl);
    if (!theControl || !theControl->GetTriggerEvent(kTriggerEvent))
        return 2;
    theControl->FireTriggerEvent(kTriggerEvent);
    return 0;
}

UIComboBox* Combo(UIForm* inForm, long inControl)
{
    if (!inForm || inControl < 0 || (unsigned long)inControl >= inForm->GetControlCount())
        return NULL;
    return dynamic_cast<UIComboBox*>(inForm->GetControl(inControl));
}

void PushLines(const UILBString* inList, void* inSink, EeSimDriverPushFn inPush)
{
    for (unsigned long k = 0; k < inList->GetLineCount(); k++) {
        const char* theText = (const char*)inList->GetData(k);
        inPush(inSink, theText ? theText : "");
    }
}

bool HostComboLines(long inControl, void* inSink, EeSimDriverPushFn inPush, long* outSelection)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != kFormLobbyID)
        return false;
    UIComboBox* theCombo = Combo(theForm, inControl);
    if (!theCombo || !theCombo->GetListBox())
        return false;
    PushLines(theCombo->GetListBox(), inSink, inPush);
    unsigned long theSelection = 0xffffffffUL;
    theCombo->GetCurrentSelection(theSelection);
    *outSelection = (long)(int32_t)theSelection;
    return true;
}

bool HostComboSelect(long inControl, unsigned long inIndex)
{
    UILock theLock;
    UIComboBox* theCombo = Combo(gUIManager->GetCurrentForm(), inControl);
    if (!theCombo)
        return false;
    theCombo->SetCurrentSelection(inIndex);
    return true;
}

// the load screen's (EE_SIM_DRIVER_LIST_LOAD), the play scenario screen's or the campaign scenario select form's
// file list (NULL: not on top)
UILBString* ScreenList(int inWhich)
{
    UIForm* theForm = gUIManager->GetCurrentForm();
    unsigned long theForms[3] = {kFormLoad, kFormLoadScenario, kFormCampaignSelect};
    unsigned long theControls[3] = {(unsigned long)kSPLoadGameFileListBox, (unsigned long)kLoadGameLoadFileListBox,
                                    kCampaignScenarioList};
    if (!theForm || inWhich < 0 || inWhich > 2 || theForm->GetFormID() != theForms[inWhich] ||
        theControls[inWhich] >= theForm->GetControlCount())
        return NULL;
    return dynamic_cast<UILBString*>(theForm->GetControl(theControls[inWhich]));
}

bool HostListLines(int inWhich, void* inSink, EeSimDriverPushFn inPush, unsigned long* outSelection)
{
    UILock theLock;
    *outSelection = 0xffffffffUL;
    UILBString* theList = ScreenList(inWhich);
    if (!theList)
        return false;
    unsigned long theSelection = 0xffffffffUL;
    bool theOK = theList->GetCurrentSelection(theSelection) == kErrorOK;
    // (the campaign's list: the selection only when the list gives one; the file lists: as it is written)
    *outSelection = inWhich == EE_SIM_DRIVER_LIST_CAMPAIGN && !theOK ? 0xffffffffUL : theSelection;
    PushLines(theList, inSink, inPush);
    return true;
}

bool HostListSelect(int inWhich, unsigned long inIndex)
{
    UILock theLock;
    UILBString* theList = ScreenList(inWhich);
    if (!theList)
        return false;
    theList->ChangeSelectionIndex(inIndex, true, false);
    return true;
}

long HostControlWithEvent(unsigned int inForm, unsigned int inCode)
{
    UILock theLock;
    UIForm* theForm = gUIManager->GetCurrentForm();
    if (!theForm || theForm->GetFormID() != inForm)
        return -1;
    for (unsigned long k = 0; k < theForm->GetControlCount(); k++) {
        UIControl* theControl = theForm->GetControl(k);
        EEScreenEvent* theEvent =
            theControl ? dynamic_cast<EEScreenEvent*>(theControl->GetTriggerEvent(kTriggerEvent)) : NULL;
        if (theEvent != NULL && theEvent->mEvent == inCode)
            return (long)k;
    }
    return -1;
}

void HostPhysics(EeSimDriverPhysics* outPhysics)
{
    outPhysics->present = gPhysics != NULL;
    if (gPhysics == NULL)
        return;
    outPhysics->timer_running = gPhysics->mTimerRunning;
    outPhysics->game_over = gPhysics->mGameOver;
    outPhysics->game_ms = gPhysics->mGameTime;
    outPhysics->pt = gPhysics->mNumTicksDone;
}

bool HostEditorUp()
{
    UILock theLock;
    return gUIManager->GetScenarioEditorForm() != NULL && gUIManager->GetCurrentForm() == gUIManager->GetScenarioEditorForm() &&
           gWorld.GetMap() != NULL;
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

// the idle human as the harness's civilization pre-empt leaves it (ai_game.py: the civilization builder opened and
// cancelled): the civilization timer sends its messages but opens no form, and the computers' civilizations are not
// locked at its expiry. With inHumanOut, allied both ways with every computer from the start: the computers' attack
// choice prefers a human, and the classic's early raids went to this seat, which never fights back (the computers
// stay at war with each other; an observer seat, or its units removed, ends the single-player game at once)
void HostPrepareHuman(bool inHumanOut)
{
    EEPlayer* theHuman = gWorld.GetCurrentPlayer();
    theHuman->mCivilizationChosen = true;
    theHuman->mCivilizationLocked = true;
    if (!inHumanOut)
        return;
    for (unsigned long i = 1; i < gWorld.GetNumPlayers(); i++) {
        EEPlayer* theOther = gWorld.GetPlayer(i);
        if (theOther == NULL || theOther == theHuman)
            continue;
        theHuman->SetDiplomaticStance(theOther, 0, gWorld.GetMap());
        theOther->SetDiplomaticStance(theHuman, 0, gWorld.GetMap());
    }
}

std::string sDumpLoad;

void HostDumpAfterLoad(const char* inPath)
{
    sDumpLoad = inPath;
    EEAfterLoad([] { DumpState(sDumpLoad); });   // at the first tick, after the load's own fix-ups
}

size_t HostChecksumFields(char* outBuffer, size_t inCapacity)
{
    return PutText(ChecksumText(), outBuffer, inCapacity);
}

void HostDumpState(const char* inPath)
{
    DumpState(inPath);
}

// a --save-at request, as EEPhysics::AutoSave sends one (but quiet and not an autosave: no autosave is trimmed, the
// preferences are not written). It runs at the start of the next communications tick that carries it (EEPhysics::
// Tick: DrainInput executes it, ServeLoadSaveRequests saves), between two communications ticks
void HostRequestSave(const char* inName)
{
    UWideString theFile(inName);   // load, quiet, tick, time, auto-save, the wait id, the name
    NEMessage* theSave = new EEMRecord(kMessageLoadOrSaveGame, {0, true, gCommunications->GetNumberOfCTsProcessedInPhysics(),
                                                                TMGetTime(), 0, 0, theFile});
    static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(theSave, 15000, 12);
    theSave->Dereference();
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

void HostSetSwitches(const EeSimDriverSwitches* inSwitches)
{
    gPlatSimMode = 1;
    gPlatSimSeedSet = inSwitches->seed_set;   // a loaded game: the file's seed, as the game loads it
    gPlatSimSeed = inSwitches->seed;
    gPlatSimNoHumanDefeat = inSwitches->no_human_defeat;
    gScenarioSimDialogs = inSwitches->scenario_dialogs;
    // no fixed ports by default: a pair checked free at start could be taken by another process (another sim, a
    // connection's ephemeral port) before the random-map screen's local server bound it; that server then was not
    // published and its first input crashed (NERecordPlayback::Process on its NULL recorder)
    gPlatSimPortOffset = inSwitches->port_offset >= 0 ? inSwitches->port_offset : kPlatSimAnyPort;
}

long HostNoHumanDefeatHits(long inOp)
{
    if (inOp == 0)
        gPlatSimNoHumanDefeatHits = 0;
    return (long)gPlatSimNoHumanDefeatHits;
}

void HostCancelGame()
{
    gUserInterface->PostCancelGame();
}

int HostEditorStep(const char* const* inWords, size_t inCount, void* inSink, EeSimDriverPushFn inPush)
{
    std::vector<std::string> theWords(inWords, inWords + inCount);
    std::vector<std::string> theLines;
    int theCode = EESimEditorStep(theWords, theLines);
    for (const std::string& l : theLines)
        inPush(inSink, l.c_str());
    return theCode;
}

// the player's first living building (NULL: none)
EEUnit* FirstBuilding(unsigned long inPlayer)
{
    EEPlayer* thePlayer = gWorld.GetPlayer(inPlayer);
    for (EEUnit* u = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; u != NULL; u = static_cast<EEUnit*>(u->mNext))
        if (u->mComplexUnit != NULL && u->mHitPoints > 0 && u->mUnitClass->IsBuilding())
            return u;
    return NULL;
}

// --flare: the idle human's flare at the building, told to the human's allies as EEMSendFlare::Execute tells them
// (EEPlayer::FlareReceived): the AIs' answer to an ally's flare, which no computer sends
void HostFlare(unsigned long inPlayer, EeSimDriverSpot* outSpot)
{
    EEPlayer* theHuman = gWorld.GetCurrentPlayer();
    EEUnit* theSpot = FirstBuilding(inPlayer);
    if (theHuman == NULL || theSpot == NULL)
        return;
    U2DPoint<long> theTile(theSpot->GetTileX(), theSpot->GetTileY());
    std::vector<EEPlayer*> theAllies = theHuman->mAllies;
    for (EEPlayer* a : theAllies)
        a->FlareReceived(theHuman, theTile);
    *outSpot = {true, theTile.mX, theTile.mY, (unsigned long)theAllies.size()};
}

// --volcano: a volcano (the calamity class 9, no caster) cast 6 tiles off the building for that player
void HostVolcano(unsigned long inPlayer, EeSimDriverSpot* outSpot)
{
    EEUnit* theSpot = FirstBuilding(inPlayer);
    if (theSpot == NULL)
        return;
    long theX = theSpot->GetTileX() + 6, theY = theSpot->GetTileY() + 6;
    bool theCast = USingleton<EECalamityManager>::GetInstance()->CreateLocationCalamity(9, 0xffffffffUL, theX, theY,
                                                                                         inPlayer);
    *outSpot = {true, theX, theY, theCast ? 1UL : 0UL};
}

void HostVolcanoBoulders(void* inSink, EeSimDriverPushFn inPush)
{
    for (unsigned long i = 0; i < gWorld.GetNumPlayers(); i++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(i);
        for (EEUnit* u = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; u != NULL; u = static_cast<EEUnit*>(u->mNext))
            if (u->mUnitClass->GetUnitType() == kUnitTypeVolcanoProjectile)
                inPush(inSink, std::to_string(u->GetUnitID()).c_str());
    }
}

bool HostCaptureActive()
{
    return EESimCaptureRecord::Active();
}

void HostSweep(int inKind, long inArg)
{
    switch (inKind) {
    case EE_SIM_DRIVER_SWEEP_FIVE_MINUTES:   // every five game minutes of a captured game
        EEFormationCaptureSweep();   // (ee/EEFormation.h)
        EECombatCaptureSweep();
        EETechCaptureSweep();   // the tech trees' sweep (ee/EETechCapture.h)
        EECalamityCaptureSweep();   // CanCastAt over every calamity class (ee/EETechCapture.h)
        EEMessageCaptureDecisionSweep();   // the Execute decisions' sweep (ee/EEMessageCapture.h)
        EESelectPickSweep();   // the box selection's pick over every player's units (ee/EEUnitStore.h)
        EEFillSweep();   // the active groups' Fill test over every player's units (ee/EEUnitStore.h)
        EEGroupCommandSweep();   // the group commands' bodies over the dry host (ee/EEUnitHost.h)
        break;
    case EE_SIM_DRIVER_SWEEP_MESSAGES: EEMessageCaptureSweep(); break;   // once (ee/EEMessageCapture.h)
    case EE_SIM_DRIVER_SWEEP_CALAMITIES: EETechCaptureCastCalamities(inArg); break;   // --calamities
    case EE_SIM_DRIVER_SWEEP_UNITS: EEPhysicsUnitSweep(inArg); break;   // --unit-sweep (EEPhysicsSweep.cpp)
    case EE_SIM_DRIVER_SWEEP_MINUTE:   // every game minute: the mini map's cells (MMSN), the tile sight (TSGT)
        EEAppCaptureMiniMapSnapshot();
        EESightCaptureSnapshot();
        break;
    case EE_SIM_DRIVER_SWEEP_CHEATS: EEAppCaptureCheatSweep(); break;   // --app-sweep at five game minutes
    case EE_SIM_DRIVER_SWEEP_STORMS: EEAppCaptureStormSweep(); break;   // --app-sweep at ten
    case EE_SIM_DRIVER_SWEEP_END: {   // the end of a game; inArg 0: nothing swept, only the flush
        if (inArg & 2) {   // the 3D effects' sweep alone (the game's own records held since its start)
            EESimCaptureRecord::Hold(false);
            EEEffectCaptureSweep();
            EECombatCaptureFlush();
            EESimCaptureRecord::Close();
            return;
        }
        if (inArg & 4) {   // the active groups' sweep alone (the game's own records held since its start)
            EESimCaptureRecord::Hold(false);
            EEActiveGroupCaptureSweep();
            EECombatCaptureFlush();
            EESimCaptureRecord::Close();
            return;
        }
        if (inArg & 1)
            EECombatListSearchSweep();
        if (inArg & 8) {
            // EEMap's sight and out-of-synch checksum (MAPS, MAPK; their TSGT): no game reaches them. The changed
            // sight would reach the other threads: nothing records after it
            EECombatCaptureFlush();
            const unsigned int kTags[] = {EECaptureTag("MAPS"), EECaptureTag("MAPK"), EECaptureTag("TSGT")};
            EESimCaptureRecord::Only(kTags, 3);
            EESimCaptureRecord::Exclusive(true);
            EESightCaptureSweep();
            // EEWorld::RecordStatistics (WSTA): only a game left runs it (EEWorld::Clear); its random draws and the
            // computers' topped-up totals reach nothing after it
            EESimCaptureRecord::Only(EECaptureTag("WSTA"));
            EEAppCaptureStatisticsSweep();
            EESimCaptureRecord::Exclusive(false);
            EESimCaptureRecord::Only(0);
            EESimCaptureRecord::Close();
        }
        if (inArg & 16) {
            EECombatMissileSweep();
            // the missiles it made stay on the map (the AI and UI threads would see them): nothing records after it
            EECombatCaptureFlush();
            EESimCaptureRecord::Close();
        }
        EECombatCaptureFlush();
        break;
    }
    case EE_SIM_DRIVER_SWEEP_HOLD: EESimCaptureRecord::Hold(inArg != 0); break;
    case EE_SIM_DRIVER_SWEEP_DIALOG:   // --scenario: a shown dialog message ends now (ee/EEScenarioManager.h)
        if (gScenarioSimDialogs && gWorld.mScenarioManager != NULL && gWorld.mScenarioManager->mIsShowingDialog)
            gWorld.mScenarioManager->ShowDialogMessage();
        break;
    }
}

bool HostRulesAdd(const char* inText, char* outError, size_t inCapacity)
{
    std::string theError;
    if (EERulesAddOverride(inText, theError))
        return true;
    PutText(theError, outError, inCapacity);
    return false;
}

void HostRulesProblems(void* inSink, EeSimDriverPushFn inPush)
{
    std::vector<std::string> theProblems;
    EERulesOverrideProblems(theProblems);
    for (const std::string& p : theProblems)
        inPush(inSink, p.c_str());
}

long HostEngineChoice(const char* inName)
{
    return EECPChoiceByName(inName);
}

void HostEngineIDs(void* inSink, EeSimDriverPushFn inPush)
{
    for (const EECPAIEntry& e : EECPAIManifest())
        inPush(inSink, e.mID.c_str());
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

// a unit class's value in a sample: the cost of the player's tech node of the class's technology, every resource
long HostUnitValue(uint64_t inPlayer, long inTechnology, unsigned long inUnitType)
{
    EEPlayer* thePlayer = reinterpret_cast<EEPlayer*>((uintptr_t)inPlayer);
    EETechTreeNode* theNode = thePlayer->mTechnologies != NULL ? thePlayer->mTechnologies->Get(inTechnology) : NULL;
    long theValue = 0;
    if (theNode != NULL && theNode->mData != NULL)
        for (long r = kResourceFood; r <= kResourceIron; r++)
            theValue += theNode->GetCost(r, inUnitType, 1.0f);
    return theValue;
}

void DebugTrampoline(const char* inText)
{
    ee_sim_driver_debug(inText);
}

}   // namespace

// EESimPreferences: the settings the simulation runs with, over config.yaml (EERun, after gGamePreferences.Load): a
// window (hidden: DX7Screen::CreateAWindow), the harness's pins (32-bit, Min = Max PTs per Second), no autosave, no music
void EESimPreferences()
{
    long thePTRate = 40, theAutoSave = 0;
    ee_sim_driver_preferences(&thePTRate, &theAutoSave);
    gGamePreferences.mSettings.mFullScreen = false;
    gGamePreferences.mSettings.mGameBitDepth = 32;
    gGamePreferences.mSettings.mTextureBitDepth = 32;
    gGamePreferences.mSettings.mGameWindowWidth = 800;
    gGamePreferences.mSettings.mGameWindowHeight = 600;
    gGamePreferences.mSettings.mMinPTsPerSecond = thePTRate;
    gGamePreferences.mSettings.mMaxPTsPerSecond = thePTRate;
    gGamePreferences.mSettings.mAutoSaveInMilliseconds = (unsigned long)theAutoSave;
    gGamePreferences.mSettings.mMusicEnabled = false;
    gGamePreferences.mSettings.mWaitForVSync = false;
}

// EESimStart: the driver's thread (EERun, after the engine and the threads are up, before the UI loop)
void EESimStart()
{
    ee_sim_driver_start();
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

// EESimMain: EE-SIM.exe's entry (EESimMain.cpp's main): the driver's options and switches (ee_game sim_driver), the
// debug strings and faults hooked, then the game's own WinMain. On POSIX the game's main calls it for "ee-aoc --sim
// ..." and goes on with the game when it returns -1.
int EESimMain(int argc, char** argv)
{
    static const EeSimDriverHost kHost = {
        HostTopForm, HostTopFormClass, HostClick, HostComboLines, HostComboSelect, HostListLines, HostListSelect,
        HostControlWithEvent, HostPhysics, HostEditorUp, HostLock, HostPrepareHuman, HostDumpAfterLoad,
        HostChecksumFields, HostDumpState, HostRequestSave, HostExtension, HostSetRun, HostSetAfterTick,
        HostSetSwitches, HostNoHumanDefeatHits, HostCancelGame, HostEditorStep, HostFlare, HostVolcano,
        HostVolcanoBoulders, HostCaptureActive, HostSweep, HostRulesAdd, HostRulesProblems, HostEngineChoice,
        HostEngineIDs, HostSetEnv, HostDupStderr, HostSetStderr, HostWrite, HostRngTrace, HostTickHook, HostQuit,
        HostWonders, HostUnitValue};
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
