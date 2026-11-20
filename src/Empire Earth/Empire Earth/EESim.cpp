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
                    theTown->mPatrolGroup != NULL ? (long)ee_units_pg_id(theTown->mPatrolGroup) : -1L);
            for (unsigned long v = 0; v < theTown->GetHullPointCount(); v++)
                fprintf(f, "%ld,%ld ", (long)theTown->GetHullPoint(v).GetX(), (long)theTown->GetHullPoint(v).GetY());
            fprintf(f, "\n");
        }
        for (size_t g = 0; g < p->mPatrolGroups.size(); g++) {
            EEPPatrolGroup* theGroup = p->mPatrolGroups[g];
            if (theGroup == NULL)
                continue;
            fprintf(f, "PATROL %lu %lu town=%d loop=%d points=", i, (unsigned long)ee_units_pg_id(theGroup), ee_units_pg_is_town_patrol(theGroup) ? 1 : 0,
                    ee_units_pg_is_loop(theGroup) ? 1 : 0);
            for (size_t k = 0; k < ee_units_pg_point_count(theGroup); k++) {
                EEComplexUnit* thePoint = static_cast<EEComplexUnit*>(ee_units_pg_point(theGroup, k));
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
        HostLock, HostDumpAfterLoad, HostChecksumFields, HostDumpState, HostExtension, HostSetRun, HostSetAfterTick,
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
    default: break;
    }
    return 0.0;
}
