// New (port), TEST ONLY: the group command sweep (docs/RUST_PLAN.md R5b.4 leaf 4 (c)). No capture game sends a group
// command (EE-SIM's human idles; the computer players send only walls), so the Rust bodies of the group commands
// (rust/crates/ee_units/src/group.rs) are run here over the live world through the dry host (ee/EEUnitHost.h: every
// world call logged, nothing changed), every five game minutes, for every player and a set of selections and
// arguments; next to them the original bodies' statements, copied from EEMessage.cpp before the port with each world
// call made through the same host calls and every read of a unit made on the object (gWorld.GetUnit, the unit's
// class), run over the same dry host. The two logs must be equal (else EE-SIM stops, exit code 3); the USGC record
// holds the Rust body's log, so the capture's digest gates it like the others.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEMap.h"
#include "ee/EEUnitKind.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitHost.h"
#include "rust/ee_rust.h"

namespace {

const uint32_t kNone = 0xffffffffu;

size_t Count(void* inGroup)
{
    return ee_units_host_group_ids(inGroup, NULL, 0);
}

uint32_t UnitAt(void* inGroup, long inIndex)
{
    std::vector<uint32_t> theIDs(Count(inGroup) + 1);
    size_t theCount = ee_units_host_group_ids(inGroup, &theIDs[0], theIDs.size());
    return inIndex >= 0 && (size_t)inIndex < theCount ? theIDs[inIndex] : kNone;
}

uint32_t IDOf(RTSUnit* inUnit)
{
    return inUnit != NULL ? static_cast<uint32_t>(inUnit->mUnitID) : kNone;
}

EEUnit* ObjectOf(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

// EEMAttackGround::Execute (0x4bb4be) as it was, each world call through the host
void OriginalAttackGround(EEPlayer* thePlayer, void* theGroup, float inX, float inY)
{
    GE3DPoint theTarget;
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    if (theUnit != kNone) {
        theTarget.SetX(inX);
        theTarget.SetY(inY);
        theTarget.SetZ(ee_units_host_map_height(inX, inY));   // gWorld.GetMap()->GetHeight(theTarget)
        if (theTarget.GetZ() < 0.0f)
            theTarget.SetZ(0.0f);
        for (unsigned long i = 1; i < 6; i++) {
            void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
            ee_units_host_group_fill(theActiveGroup, theGroup, 0, (int32_t)i);
            if (Count(theActiveGroup) > 0) {
                ee_units_host_pending_push(thePlayer, theActiveGroup);
                ee_units_host_group_keep_ground_attackers(theActiveGroup);
                if (Count(theActiveGroup) > 0)
                    theUnit = UnitAt(theActiveGroup, ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                else
                    theUnit = kNone;
                if (theUnit != kNone) {
                    ee_units_host_group_clear_goal(theActiveGroup);
                    EeGoalSpec theSpec;
                    memset(&theSpec, 0, sizeof(theSpec));
                    theSpec.ctor = EE_GOAL_CTOR_ATTACK_GROUND;   // new EEUGAttackGround(theTarget, true)
                    theSpec.unit_a = kNone;
                    theSpec.unit_b = kNone;
                    theSpec.point[0] = theTarget.GetX();
                    theSpec.point[1] = theTarget.GetY();
                    theSpec.point[2] = theTarget.GetZ();
                    theSpec.flag_a = 1;
                    ee_units_host_group_set_goal(theActiveGroup, ee_units_host_goal_new(&theSpec), 0);
                    ee_units_host_group_set_action(theActiveGroup, kActionAttackLocation);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_send_group_command(theActiveGroup, kNone, thePlayer, 0xb9, 0);
                    ee_units_host_acknowledge_unit(thePlayer, theUnit);
                }
            } else
                ee_units_host_group_release(theActiveGroup);
        }
    }
}

// EEMPatrol::Execute (0x4c2cf2) as it was, each world call through the host, the units read on the objects
void OriginalPatrol(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID)
{
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    unsigned long theTargetID = inTargetID;
    EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(theTargetID));
    if (theUnit != kNone && inTargetID == -1) {
        // theUnit->FindNearestTown(), its patrol group's GetNearest
        uint8_t theFound = 0;
        uint32_t thePoint = ee_units_host_town_patrol_point(theUnit, &theFound);
        if (theFound)
            theTarget = thePoint != kNone ? static_cast<EEComplexUnit*>(ObjectOf(thePoint)) : NULL;
    }
    if (theTarget && theUnit != kNone) {
        if (ObjectOf(theUnit)->mUnitClass->mUnitType == kUnitTypePathPoint) {
            ee_units_host_patrol_add_unit_tile(theUnit, IDOf(theTarget), thePlayer);
        } else {
            void* thePatrolGroup = ee_units_host_unit_patrol_group(IDOf(theTarget));   // theTarget->mPatrolGroup
            for (unsigned long i = 1; i < 6; i++) {
                void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
                ee_units_host_group_fill(theActiveGroup, theGroup, 0, (int32_t)i);
                if (Count(theActiveGroup) > 0) {
                    ee_units_host_pending_push(thePlayer, theActiveGroup);
                    ee_units_host_group_keep_mobile(theActiveGroup);
                    EeGoalSpec theSpec;   // new EEUGPatrol(thePatrolGroup, theTarget, true)
                    memset(&theSpec, 0, sizeof(theSpec));
                    theSpec.ctor = EE_GOAL_CTOR_PATROL_ROUTE;
                    theSpec.unit_a = IDOf(theTarget);
                    theSpec.unit_b = kNone;
                    theSpec.object = thePatrolGroup;
                    theSpec.flag_a = 1;
                    void* theGoal = ee_units_host_goal_new(&theSpec);
                    ee_units_host_group_clear_goal(theActiveGroup);
                    ee_units_host_group_set_goal(theActiveGroup, theGoal, 0);
                    ee_units_host_group_set_action(theActiveGroup, kActionMoveToNextPatrolPoint);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_send_group_command(theActiveGroup, kNone, thePlayer, 0xba, 0);
                    if (Count(theActiveGroup) > 0)
                        ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                           ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                } else
                    ee_units_host_group_release(theActiveGroup);
            }
        }
    }
}

enum SweepBody {
    kSweepAttackGround = 1,
    kSweepPatrol = 2
};

struct SweepCase {
    unsigned long mBody;
    unsigned long mSelection;   // which selection (the record's)
    float mX, mY;               // Attack Ground's point
    int32_t mTarget;            // Patrol's target id
};

void RunBody(const SweepCase& inCase, EEPlayer* inPlayer, void* inSelection, bool inRust)
{
    switch (inCase.mBody) {
    case kSweepAttackGround:
        if (inRust)
            ee_units_attack_ground(inPlayer, inSelection, inCase.mX, inCase.mY);
        else
            OriginalAttackGround(inPlayer, inSelection, inCase.mX, inCase.mY);
        break;
    case kSweepPatrol:
        if (inRust)
            ee_units_patrol(inPlayer, inSelection, inCase.mTarget);
        else
            OriginalPatrol(inPlayer, inSelection, inCase.mTarget);
        break;
    }
}

void RunCase(const SweepCase& inCase, unsigned long inPlayerIndex, EEPlayer* inPlayer, const std::vector<EEComplexUnit*>& inUnits)
{
    std::vector<uint32_t> theLogs[2];
    unsigned long theMismatches[2];
    for (int k = 0; k < 2; k++) {
        void* theSelection = EEUnitHostDryBegin(inUnits);
        RunBody(inCase, inPlayer, theSelection, k == 0);
        EEUnitHostDryEnd(theLogs[k], theMismatches[k]);
    }
    EESimCaptureRecord theRecord(0x43475355u);   // "USGC"
    theRecord.U32((unsigned int)inCase.mBody);
    theRecord.I32((int)inPlayerIndex);
    theRecord.U32((unsigned int)inCase.mSelection);
    theRecord.F32(inCase.mX);
    theRecord.F32(inCase.mY);
    theRecord.I32(inCase.mTarget);
    theRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRecord.U32(IDOf(inUnits[i]));
    theRecord.U32((unsigned int)theLogs[0].size());
    if (!theLogs[0].empty())
        theRecord.Bytes(&theLogs[0][0], (unsigned int)(theLogs[0].size() * 4));
    theRecord.U32((unsigned int)theMismatches[0]);
    theRecord.Commit();
    if (theLogs[0] != theLogs[1] || theMismatches[0] != 0 || theMismatches[1] != 0) {
        size_t theAt = 0;
        while (theAt < theLogs[0].size() && theAt < theLogs[1].size() && theLogs[0][theAt] == theLogs[1][theAt])
            theAt++;
        fprintf(stderr, "EE-GROUP-COMMAND: body %lu player %lu selection %lu target %d: the Rust body's log (%u words) "
                "differs from the original's (%u words) at word %u; keep mismatches %lu/%lu\n", inCase.mBody,
                inPlayerIndex, inCase.mSelection, inCase.mTarget, (unsigned)theLogs[0].size(), (unsigned)theLogs[1].size(),
                (unsigned)theAt, theMismatches[0], theMismatches[1]);
        EESimCaptureRecord::Flush();
        exit(3);
    }
}

} // namespace

void EEGroupCommandSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    // the towns' hull reads (FindNearestTown) are the sweep's, not the game's: inside a scope, the HULL/AREA records
    // skip them (lle/U2DPolygon.h)
    EEGeomCaptureScope theScope;
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    for (unsigned long p = 0; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theOwn;   // its list's complex units (a selection holds only those)
        EEComplexUnit* thePathPoint = NULL;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
            if (theUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
                continue;
            EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
            theOwn.push_back(theComplex);
            if (thePathPoint == NULL && theComplex->mUnitClass.Get() != NULL && theComplex->mUnitClass->mUnitType == kUnitTypePathPoint)
                thePathPoint = theComplex;
        }
        std::vector<EEComplexUnit*> theReversed(theOwn.rbegin(), theOwn.rend());
        std::vector<EEComplexUnit*> theNone;
        std::vector<EEComplexUnit*> theFromPoint;
        if (thePathPoint != NULL) {
            theFromPoint.push_back(thePathPoint);
            theFromPoint.insert(theFromPoint.end(), theOwn.begin(), theOwn.end());
        }
        const std::vector<EEComplexUnit*>* theSelections[4] = {&theOwn, &theReversed, &theNone, &theFromPoint};
        // the targets: -1 (the nearest town's route), its first unit, the next player's first complex unit, its path
        // point, an id no unit has
        std::vector<int32_t> theTargets;
        theTargets.push_back(-1);
        if (!theOwn.empty())
            theTargets.push_back((int32_t)IDOf(theOwn[0]));
        EEPlayer* theNext = gWorld.GetPlayer((p + 1) % theNumPlayers);
        if (theNext != NULL)
            for (RTSUnit* theUnit = theNext->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                if (theUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT) {
                    theTargets.push_back((int32_t)IDOf(theUnit));
                    break;
                }
        if (thePathPoint != NULL)
            theTargets.push_back((int32_t)IDOf(thePathPoint));
        theTargets.push_back(0x7e000001);
        for (unsigned long s = 0; s < 4; s++) {
            const std::vector<EEComplexUnit*>& theUnits = *theSelections[s];
            if (s == 3 && theUnits.empty())
                continue;
            SweepCase theCase;
            theCase.mSelection = s;
            theCase.mTarget = 0;
            // Attack Ground at the first unit's tile (else, or off the map: a unit inside another, the map's centre)
            // and at the map's centre (a point off the map's diamond has no terrain row: the UI never sends one)
            theCase.mBody = kSweepAttackGround;
            bool theOnMap = !theUnits.empty() && theUnits[0]->mTileX.Get() >= 0 && theUnits[0]->mTileY.Get() >= 0
                && gWorld.GetMap()->GetTile((unsigned long)theUnits[0]->mTileX.Get(), (unsigned long)theUnits[0]->mTileY.Get()) != NULL;
            float theCenter = (float)(gWorld.GetMap()->GetSize() / 2) + 0.5f;
            theCase.mX = theOnMap ? (float)theUnits[0]->mTileX.Get() + 0.5f : theCenter;
            theCase.mY = theOnMap ? (float)theUnits[0]->mTileY.Get() + 0.5f : theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = theCenter;
            theCase.mY = theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mBody = kSweepPatrol;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            for (size_t t = 0; t < theTargets.size(); t++) {
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
        }
    }
}
