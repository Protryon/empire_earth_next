// New (port), TEST ONLY: the active groups' sweep (AGPL records; R5b.4 round 35, the golden of the active group's
// bodies). No capture game makes an active group plan (EE-SIM's human idles, the computer players send only walls and
// the unit sweep sets goals directly), so with EE_SIM_CAPTURE and --group-sweep (the capture's groups profile), when the
// game ends, every player's units get real group commands: an EEActiveGroup of its first mobile units (and its airports
// and navy yards) for each case (moves far and near, alone and together, queued; attacks on a unit and on the ground;
// gather and drop off; heal; explore; patrol; unload; disband; the four spreads; the reserved destination tests), each
// planned as the commands plan it (SetGoal, mAction, the player's pending groups, Plan). After each case one record:
// the group as left (its units, the 17 values, moving together), every selected unit's goal, queues, command, place in
// the formation and active group, the player's pending groups, and one random draw (the draws made). The game is over:
// nothing simulates after it (EESimCaptureRecord::Close). Seed 6 faults in its sweep, in the original C++ as in the
// Rust bodies: a group path found at once (RequestPaths -> MoveAlong -> SetGroupPathGoals) hands a unit
// SetGoal(goal, true), whose keep-current branch reads mCommand->mType; its golden is not recorded.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGRust.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEFormation.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitAction.h"
#include "ee/EECombatFormulas.h"
#include "ee/EESimCapture.h"

void EEActiveGroupCaptureSweep();

namespace {

const unsigned int kTag = EE_COMBAT_TAG('A', 'G', 'P', 'L');

void Point(EESimCaptureRecord& r, const GE3DPoint& inPoint)
{
    r.F32(inPoint.mX);
    r.F32(inPoint.mY);
    r.F32(inPoint.mZ);
}

void RecordGoal(EESimCaptureRecord& r, EEUnitGoal* inGoal)
{
    r.I32(inGoal != NULL ? (int)inGoal->mKind : -1);
    if (inGoal == NULL)
        return;
    r.U32((unsigned int)inGoal->mID);
    GE3DPoint theLocation;
    inGoal->GetGoalLocation(theLocation);
    Point(r, theLocation);
}

void Record(long inCase, EEPlayer* inPlayer, EEActiveGroup* inGroup, const std::vector<EEComplexUnit*>& inUnits,
            unsigned int inAnswers)
{
    EESimCaptureRecord r(kTag);
    r.I32((int)inCase);
    r.U32((unsigned int)inPlayer->GetPlayerID());
    r.U32(inAnswers);
    r.U32((unsigned int)inGroup->GetNumUnits());
    for (unsigned long i = 0; i < inGroup->GetNumUnits(); i++)
        r.U32((unsigned int)inGroup->GetUnit(i)->mUnitID);
    r.F32s(&inGroup->mSpeed, 17);
    r.U32(inGroup->mMovesTogether);
    r.U32((unsigned int)inPlayer->mPendingGroups.size());
    for (size_t i = 0; i < inUnits.size(); i++) {
        EEComplexUnit* u = inUnits[i];
        r.U32((unsigned int)u->mUnitID);
        r.I32((int)u->mHitPoints);
        RecordGoal(r, u->mGoalState);
        r.U32((unsigned int)u->mGoalQueues.size());
        for (size_t q = 0; q < u->mGoalQueues.size(); q++) {
            r.U32((unsigned int)u->mGoalQueues[q].mGoals.size());
            for (size_t k = 0; k < u->mGoalQueues[q].mGoals.size(); k++)
                RecordGoal(r, u->mGoalQueues[q].mGoals[k]);
        }
        r.U32((unsigned int)u->GetCommandType());
        r.U32((unsigned int)u->mFormationSlot);
        r.I32((int)u->mFormationRank);
        r.U32(u->mIsSpecialAttack);
        r.U32(u->mActiveGroup == inGroup);
        r.U32(u->mActiveGroup != NULL);
    }
    r.I32((int)gPhysics->GetRandom(0L, 0x3fffffffL));
    r.Commit();
}

// a fresh group of the units in the formation kind
EEActiveGroup* NewGroup(EEPlayer* inPlayer, const std::vector<EEComplexUnit*>& inUnits, long inFormation)
{
    EEActiveGroup* theGroup = new EEActiveGroup(inPlayer);
    for (size_t i = 0; i < inUnits.size(); i++)
        if (inUnits[i]->mHitPoints > 0 && inUnits[i]->mIsValid)
            theGroup->AddMember(inUnits[i], true);
    EEFormation* theFormation = EEFormation::Create(inFormation);
    if (theFormation != NULL) {
        theGroup->SetFormation(theFormation);
        theFormation->Dereference();
    }
    return theGroup;
}

// the commands' way: the goal, the action, into the pending groups, Plan
void Run(EEPlayer* inPlayer, EEActiveGroup* inGroup, EEUnitGoal* inGoal, long inAction, bool inQueued)
{
    inGroup->SetGoal(inGoal, inQueued);
    inGroup->mAction = inAction;
    inPlayer->mPendingGroups.push_back(inGroup);
    inGroup->Plan();
}

EEUnitGoal* Move(const GE3DPoint& inPoint, bool inTogether, float inYaw)
{
    GE3DPoint thePoint = inPoint;
    EEUGRust* theGoal = static_cast<EEUGRust*>(EEUGRust::NewLocation(&thePoint, true, true, inYaw != 0.0f, inYaw, false, 0));
    theGoal->SetUseGroupMovement(inTogether);
    return theGoal;
}

bool Usable(EEUnit* inUnit)
{
    EEComplexUnit* c = inUnit->mComplexUnit;
    return c != NULL && inUnit->mHitPoints > 0 && c->mIsValid && c->mTileNode != NULL && c->GetTileX() != -10000
        && c->mStats != NULL && c->mUnitClass != NULL;
}

}   // namespace

void EEActiveGroupCaptureSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord::Only(kTag);
    EESimCaptureRecord::Exclusive(true);
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    // the resources (gaia's first units with an amount) and each player's newest usable unit (the targets)
    std::vector<EEUnit*> theResources;
    std::vector<EEComplexUnit*> theNewest(theNumPlayers, (EEComplexUnit*)NULL);
    for (unsigned long p = 0; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        for (EEUnit* u = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
            if (p == 0 && u->mResourceAmount > 0 && u->mTileNode != NULL && theResources.size() < 4)
                theResources.push_back(u);
            if (p != 0 && Usable(u))
                theNewest[p] = u->mComplexUnit;
        }
    }
    for (unsigned long p = 1; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theMobile, theBuildings, theAll;
        for (EEUnit* u = thePlayer->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
            if (!Usable(u))
                continue;
            EEComplexUnit* c = u->mComplexUnit;
            unsigned long theType = c->mUnitClass->mUnitType;
            if (c->mStats->GetMaxVelocity() > 0.0f && theMobile.size() < 10)
                theMobile.push_back(c);
            else if ((theType == kUnitTypeAirport || theType == kUnitTypeNavyYard || c->mUnitClass->mTileSize > 1)
                     && theBuildings.size() < 3)
                theBuildings.push_back(c);
        }
        if (theMobile.empty())
            continue;
        theAll = theMobile;
        theAll.insert(theAll.end(), theBuildings.begin(), theBuildings.end());
        EEComplexUnit* theTarget = NULL;
        for (unsigned long k = 1; k < theNumPlayers && theTarget == NULL; k++)
            theTarget = theNewest[(p + k) % theNumPlayers];
        if (theTarget == NULL || theTarget->mPlayer == thePlayer)
            theTarget = theMobile.back();
        GE3DPoint theFar, theNear;
        theTarget->GetPosition(theFar);
        theMobile[0]->GetPosition(theNear);
        theNear.mX += 2.0f;
        theNear.mY += 2.0f;
        EEUnit* theResource = theResources.empty() ? NULL : theResources[p % theResources.size()];
        EEComplexUnit* theStorage = theBuildings.empty() ? theMobile[0] : theBuildings[0];
        long theCase = 0;
        // moves: far and alone, far and together, near and queued, near and together, every formation kind
        for (long f = 0; f < 8; f++) {
            EEActiveGroup* g = NewGroup(thePlayer, f < 4 ? theAll : theMobile, f);
            Run(thePlayer, g, Move((f & 2) ? theNear : theFar, (f & 1) != 0, 0.25f * (float)f), kActionPrepareToMove, f >= 4);
            Record(theCase++, thePlayer, g, theAll, 0);
        }
        // attacks: a unit (special), a unit queued, the ground
        for (long k = 0; k < 3; k++) {
            EEActiveGroup* g = NewGroup(thePlayer, theAll, 1);
            g->mIsSpecialAttack = k == 0;
            EEUnitGoal* theGoal = k < 2 ? EEUGRust::NewAttackUnit(theMobile[0], theTarget, true, false)
                                        : EEUGRust::NewAttackGround(theFar, true);
            Run(thePlayer, g, theGoal, k < 2 ? kActionAttackEnemyUnit : kActionAttackLocation, k == 1);
            Record(theCase++, thePlayer, g, theAll, 0);
        }
        // gather, then drop off, plain and queued
        if (theResource != NULL) {
            for (long k = 0; k < 4; k++) {
                EEActiveGroup* g = NewGroup(thePlayer, theMobile, 0);
                Run(thePlayer, g, EEUGRust::NewGather(theResource, theStorage, true),
                    k < 2 ? kActionGatherResource : kActionDropoffResource, (k & 1) != 0);
                Record(theCase++, thePlayer, g, theMobile, 0);
            }
        }
        // heal, explore, unload, disband
        {
            EEActiveGroup* g = NewGroup(thePlayer, theAll, 2);
            Run(thePlayer, g, EEUGRust::NewHealUnit(theMobile.back(), true), kActionHealUnit, false);
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theAll, 3);
            Run(thePlayer, g, EEUGRust::NewExplore(true), kActionFindUnexploredArea, false);
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theAll, 4);
            Run(thePlayer, g, EEUGRust::NewUnloadTransport(theMobile[0], theTarget->GetTileX(), theTarget->GetTileY(), true),
                kActionUnloadTransport, false);
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theMobile, 0);
            Run(thePlayer, g, Move(theFar, false, 0.0f), kActionDisband, false);
            Record(theCase++, thePlayer, g, theMobile, 0);
        }
        // patrol along the player's first town patrol
        for (unsigned long t = 0; t < thePlayer->mTowns.size(); t++) {
            EETown* theTown = thePlayer->mTowns[t];
            if (theTown == NULL || theTown->mPatrolGroup == NULL || theTown->mPatrolGroup->GetLast() == NULL)
                continue;
            EEPPatrolGroup* thePatrol = theTown->mPatrolGroup;
            EEActiveGroup* g = NewGroup(thePlayer, theAll, 5);
            Run(thePlayer, g, EEUGRust::NewPatrol(thePatrol, thePatrol->GetLast(), true), kActionMoveToNextPatrolPoint, false);
            Record(theCase++, thePlayer, g, theAll, 0);
            break;
        }
        // the spreads (a fresh group of all the units each)
        {
            EEActiveGroup* g = NewGroup(thePlayer, theAll, 1);
            g->SetGoal(Move(theFar, false, 0.0f), false);
            g->SpreadAroundUnit(theTarget, theTarget->GetTileX(), theTarget->GetTileY());
            Record(theCase++, thePlayer, g, theAll, 0);
            if (theResource != NULL) {
                g = NewGroup(thePlayer, theAll, 1);
                g->SetGoal(Move(theFar, false, 0.0f), true);
                g->SpreadAroundResource(theResource);
                Record(theCase++, thePlayer, g, theAll, 0);
            }
            g = NewGroup(thePlayer, theAll, 2);
            g->SetGoal(Move(theFar, false, 0.0f), false);
            g->SpreadAroundTile(theStorage->GetTileX(), theStorage->GetTileY());
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theAll, 2);
            g->SetGoal(Move(theFar, false, 0.0f), false);
            g->SpreadForUnload(theTarget->GetTileX(), theTarget->GetTileY());
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theAll, 0);
            g->KeepAttackers();
            Record(theCase++, thePlayer, g, theAll, 0);
            g = NewGroup(thePlayer, theAll, 0);
            g->KeepMobileUnits();
            Record(theCase++, thePlayer, g, theAll, 0);
        }
        // the reserved destinations: every unit's position told, then points at, near and away from them
        {
            EEActiveGroup* g = NewGroup(thePlayer, theAll, 0);
            for (size_t i = 0; i < theAll.size(); i++) {
                GE3DPoint thePoint;
                theAll[i]->GetPosition(thePoint);
                g->TellGroup(theAll[i], thePoint);
            }
            unsigned int theAnswers = 0;
            for (size_t i = 0; i < theAll.size() && i < 5; i++) {
                GE3DPoint thePoint;
                theAll[i]->GetPosition(thePoint);
                const float kOffsets[3] = {0.0f, 0.2f, 7.0f};
                for (int o = 0; o < 3; o++) {
                    GE3DPoint theProbe(thePoint.mX + kOffsets[o], thePoint.mY + kOffsets[o], thePoint.mZ);
                    theAnswers = theAnswers << 2 | (g->IsReserved(theProbe) ? 1u : 0u) | (g->IsCellTaken(theProbe) ? 2u : 0u);
                }
            }
            Record(theCase++, thePlayer, g, theAll, theAnswers);
        }
    }
    EESimCaptureRecord::Exclusive(false);
    EESimCaptureRecord::Only(0);
}
