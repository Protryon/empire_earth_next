// New (port), TEST ONLY: the group command sweep (docs/RUST_PLAN.md R5b.4 leaf 4 (c)). No capture game sends a group
// command (EE-SIM's human idles; the computer players send only walls), so the Rust bodies of the group commands
// (rust/crates/ee_units/src/group.rs) are run here over the live world through the dry host (ee/EEUnitHost.h: every
// world call logged, nothing changed), every five game minutes, for every player and a set of selections and
// arguments. The USGC record holds the Rust body's log, so the capture's digest gates it like the others; a dry host
// mismatch stops EE-SIM (exit code 3). (R5b.4 round 31: the original bodies' statements, which ran next to the Rust
// ones over the same dry host until every body had matched them in the four profiles since rounds 6-9 and the
// messages lane's rounds 1-2, are retired; the USGC goldens recorded while they agreed remain the check.)
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
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitKind.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitHost.h"
#include "ee/EEUnitGoal.h"
#include "rust/ee_units.h"

namespace {

const uint32_t kNone = 0xffffffffu;

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

enum SweepBody {
    kSweepAttackGround = 1,
    kSweepPatrol = 2,
    kSweepGather = 3,
    kSweepRepair = 4,
    kSweepAddPatrolPoint = 5,
    kSweepAttackUnit = 6,
    kSweepHealUnit = 7,
    kSweepPathTo = 8,
    kSweepDropoffResource = 9,
    kSweepBuildWall = 10,
    kSweepUnloadTransport = 11,
    kSweepBuildUnit = 12,
    kSweepSmartFarmers = 13,
    kSweepCastCalamity = 14,
    kSweepSetAirportCommand = 15,
    kSweepSetRallyGoal = 16,
    kSweepClearAirportCommands = 17,
    kSweepStopActiveGroup = 18,
    kSweepLoadGarrison = 19,
    kSweepLoadTransport = 20,
    kSweepResearchTechnology = 21,
    kSweepHackANewUnit = 22
};

struct SweepCase {
    unsigned long mBody;
    unsigned long mSelection;   // which selection (the record's)
    float mX, mY;               // Attack Ground's and Add Patrol Point's point
    int32_t mTarget;            // Patrol's, Gather's and Repair's target id
    bool mQueued;               // Gather's, Repair's and Attack Unit's
    bool mConvert;              // Attack Unit's
    float mFacing;              // Path To's
    unsigned long mFlags;       // Path To's: 1 forced, 2 rotating, 4 queued, 8 group movement
    float mEndX, mEndY;         // Build Wall's end (mX, mY its start; mTarget its technology)
    int32_t mTechnology;        // Build Unit's and Cast Calamity's (mX, mY their tile; mTarget Cast Calamity's target)
    int32_t mCaster;            // Cast Calamity's caster id; Set Rally Goal's building id
    int32_t mCommand;           // Set Airport Command's command
};

void RunBody(const SweepCase& inCase, EEPlayer* inPlayer, void* inSelection)
{
    switch (inCase.mBody) {
    case kSweepAttackGround:
        ee_units_attack_ground(inPlayer, inSelection, inCase.mX, inCase.mY);
        break;
    case kSweepPatrol:
        ee_units_patrol(inPlayer, inSelection, inCase.mTarget);
        break;
    case kSweepGather:
        ee_units_gather(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued);
        break;
    case kSweepAddPatrolPoint:
        ee_units_add_patrol_point(inPlayer, inSelection, inCase.mX, inCase.mY);
        break;
    case kSweepAttackUnit:
        ee_units_attack_unit(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued, inCase.mConvert);
        break;
    case kSweepHealUnit:
        ee_units_heal_unit(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        break;
    case kSweepPathTo: {
        float theX = inCase.mX, theY = inCase.mY;
        ee_units_path_to(inPlayer, inSelection, &theX, &theY, inCase.mFacing, (inCase.mFlags & 1) != 0,
                             (inCase.mFlags & 2) != 0, (inCase.mFlags & 4) != 0, (inCase.mFlags & 8) != 0);
        break;
    }
    case kSweepBuildWall:
        ee_units_build_wall(inPlayer, inSelection, inCase.mTarget, inCase.mX, inCase.mY, inCase.mEndX, inCase.mEndY);
        break;
    case kSweepUnloadTransport:
        ee_units_unload_transport(inPlayer, inSelection, (int64_t)inCase.mX, (int64_t)inCase.mY);
        break;
    case kSweepBuildUnit:
        ee_units_build_unit(inPlayer, inSelection, inCase.mTechnology, (int64_t)inCase.mX, (int64_t)inCase.mY,
                                inCase.mQueued);
        break;
    case kSweepSmartFarmers:
        ee_units_smart_farmers(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        break;
    case kSweepCastCalamity:
        ee_units_cast_calamity(inPlayer, inSelection, (uint32_t)inCase.mCaster, inCase.mTarget, (int32_t)inCase.mX,
                                   (int32_t)inCase.mY, inCase.mTechnology);
        break;
    case kSweepDropoffResource:
        ee_units_dropoff_resource(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        break;
    case kSweepRepair:
        ee_units_repair(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued);
        break;
    case kSweepSetAirportCommand:
        ee_units_set_airport_command(inPlayer, inSelection, inCase.mX, inCase.mY, inCase.mCommand, inCase.mTarget,
                                         inCase.mQueued ? 1 : 0);
        break;
    case kSweepSetRallyGoal:
        // the building owner's selection is the sweep's (the dry host's player_selection)
        ee_units_set_rally_goal(inPlayer, (uint32_t)inCase.mCaster, inCase.mX, inCase.mY, inCase.mTarget,
                                    inCase.mQueued ? 1 : 0);
        break;
    case kSweepClearAirportCommands:
        ee_units_clear_airport_commands(inSelection);
        break;
    case kSweepStopActiveGroup:
        ee_units_stop_active_group(inSelection);
        break;
    case kSweepLoadGarrison:
        ee_units_load_garrison(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        break;
    case kSweepLoadTransport:   // mCaster: the passenger id
        ee_units_load_transport(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mCaster);
        break;
    case kSweepResearchTechnology:   // mTarget: the building; mQueued: five times
        ee_units_research_technology(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mTechnology,
                                         inCase.mQueued ? 1 : 0);
        break;
    case kSweepHackANewUnit:   // mTechnology: the class; mTarget: the player id
        ee_units_hack_new_unit(inPlayer, inCase.mTechnology, inCase.mX, inCase.mY, (int64_t)inCase.mTarget);
        break;
    }
}

void RunCase(const SweepCase& inCase, unsigned long inPlayerIndex, EEPlayer* inPlayer, const std::vector<EEComplexUnit*>& inUnits)
{
    std::vector<uint32_t> theLogs[1];
    unsigned long theMismatches[1];
    void* theSelection = EEUnitHostDryBegin(inUnits);
    RunBody(inCase, inPlayer, theSelection);
    EEUnitHostDryEnd(theLogs[0], theMismatches[0]);
    EESimCaptureRecord theRecord(0x43475355u);   // "USGC"
    theRecord.U32((unsigned int)inCase.mBody);
    theRecord.I32((int)inPlayerIndex);
    theRecord.U32((unsigned int)inCase.mSelection);
    theRecord.F32(inCase.mX);
    theRecord.F32(inCase.mY);
    theRecord.I32(inCase.mTarget);
    if (inCase.mBody == kSweepGather || inCase.mBody == kSweepRepair)
        theRecord.U32(inCase.mQueued ? 1u : 0u);
    if (inCase.mBody == kSweepAttackUnit)
        theRecord.U32((inCase.mQueued ? 1u : 0u) | (inCase.mConvert ? 2u : 0u));
    if (inCase.mBody == kSweepPathTo) {
        theRecord.U32((unsigned int)inCase.mFlags);
        theRecord.F32(inCase.mFacing);
    }
    if (inCase.mBody == kSweepBuildWall) {
        theRecord.F32(inCase.mEndX);
        theRecord.F32(inCase.mEndY);
    }
    if (inCase.mBody == kSweepBuildUnit)
        theRecord.U32((unsigned int)inCase.mTechnology | (inCase.mQueued ? 0x80000000u : 0u));
    if (inCase.mBody == kSweepCastCalamity) {
        theRecord.I32(inCase.mTechnology);
        theRecord.I32(inCase.mCaster);
    }
    if (inCase.mBody == kSweepSetAirportCommand)
        theRecord.U32((unsigned int)inCase.mCommand | (inCase.mQueued ? 0x80000000u : 0u));
    if (inCase.mBody == kSweepSetRallyGoal) {
        theRecord.I32(inCase.mCaster);
        theRecord.U32(inCase.mQueued ? 1u : 0u);
    }
    if (inCase.mBody == kSweepLoadTransport)
        theRecord.I32(inCase.mCaster);
    if (inCase.mBody == kSweepHackANewUnit)
        theRecord.I32(inCase.mTechnology);
    if (inCase.mBody == kSweepResearchTechnology)
        theRecord.U32((unsigned int)inCase.mTechnology | (inCase.mQueued ? 0x80000000u : 0u));
    theRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRecord.U32(IDOf(inUnits[i]));
    theRecord.U32((unsigned int)theLogs[0].size());
    if (!theLogs[0].empty())
        theRecord.Bytes(&theLogs[0][0], (unsigned int)(theLogs[0].size() * 4));
    theRecord.U32((unsigned int)theMismatches[0]);
    theRecord.Commit();
    if (theMismatches[0] != 0) {
        fprintf(stderr, "EE-GROUP-COMMAND: body %lu player %lu selection %lu target %d: %lu dry host mismatches\n",
                inCase.mBody, inPlayerIndex, inCase.mSelection, inCase.mTarget, theMismatches[0]);
        EESimCaptureRecord::Flush();
        exit(3);
    }
}

// the player's technologies whose node makes inUnitType (the first one), or -1
int32_t FindTechnology(EEPlayer* inPlayer, long inUnitType)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    for (long e = 0; e < theTree->mEpochs.size(); e++) {
        std::vector<long> theIDs;
        theTree->mEpochs[e].GetNodeIDs(theIDs);
        for (size_t i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(theIDs[i]);
            if (theNode != NULL && theNode->mData != NULL && theNode->mData->mUnitClass == inUnitType)
                return (int32_t)theIDs[i];
        }
    }
    return -1;
}

// the player's calamity technologies: the first of each calamity class kind (class 0x12, cast by all the selected,
// any other) and Teleport when the tree has it
void FindCalamityTechnologies(EEPlayer* inPlayer, std::vector<int32_t>& outIDs)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    int32_t theGroupCast = -1, theByAll = -1, theOther = -1;
    for (long e = 0; e < theTree->mEpochs.size(); e++) {
        std::vector<long> theIDs;
        theTree->mEpochs[e].GetNodeIDs(theIDs);
        for (size_t i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(theIDs[i]);
            if (theNode == NULL || theNode->mData == NULL || theNode->mData->mCalamity == 0 || theIDs[i] == kTechTeleport)
                continue;
            EECalamityClass* theClass =
                EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
            if (theClass == NULL)
                continue;
            int32_t& theSlot = theClass->mClassEnum == 0x12 ? theGroupCast : theClass->mIsCastByAllSelected ? theByAll : theOther;
            if (theSlot == -1)
                theSlot = (int32_t)theIDs[i];
        }
    }
    int32_t theFound[3] = {theGroupCast, theByAll, theOther};
    for (int k = 0; k < 3; k++)
        if (theFound[k] != -1)
            outIDs.push_back(theFound[k]);
    if (theTree->Get(kTechTeleport) != NULL)
        outIDs.push_back(kTechTeleport);
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
    // the first unit of any list with resources left (a mine, a tree, a herd animal)
    RTSUnit* theResource = NULL;
    for (unsigned long p = 0; p < theNumPlayers && theResource == NULL; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL && theResource == NULL; theUnit = theUnit->mNext)
            if (theUnit->mObjectKind.mValue >= EE_OBJECT_UNIT && static_cast<EEUnit*>(theUnit)->mResourceAmount > 0)
                theResource = theUnit;
    }
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
            if (thePathPoint == NULL && theComplex->mUnitClass != NULL && theComplex->mUnitClass->mUnitType == kUnitTypePathPoint)
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
        std::vector<int32_t> theGatherTargets;
        if (theResource != NULL)
            theGatherTargets.push_back((int32_t)IDOf(theResource));
        if (!theOwn.empty())
            theGatherTargets.push_back((int32_t)IDOf(theOwn[0]));
        theGatherTargets.push_back(0x7e000001);
        for (unsigned long s = 0; s < 4; s++) {
            const std::vector<EEComplexUnit*>& theUnits = *theSelections[s];
            if (s == 3 && theUnits.empty())
                continue;
            SweepCase theCase;
            theCase.mSelection = s;
            theCase.mTarget = 0;
            theCase.mQueued = false;
            theCase.mConvert = false;
            theCase.mFacing = 0.0f;
            theCase.mFlags = 0;
            theCase.mEndX = 0.0f;
            theCase.mEndY = 0.0f;
            theCase.mTechnology = 0;
            theCase.mCaster = 0;
            theCase.mCommand = 0;
            // Attack Ground at the first unit's tile (else, or off the map: a unit inside another, the map's centre)
            // and at the map's centre (a point off the map's diamond has no terrain row: the UI never sends one)
            theCase.mBody = kSweepAttackGround;
            bool theOnMap = !theUnits.empty() && theUnits[0]->mTileX >= 0 && theUnits[0]->mTileY >= 0
                && gWorld.GetMap()->GetTile((unsigned long)theUnits[0]->mTileX, (unsigned long)theUnits[0]->mTileY) != NULL;
            float theCenter = (float)(gWorld.GetMap()->GetSize() / 2) + 0.5f;
            theCase.mX = theOnMap ? (float)theUnits[0]->mTileX + 0.5f : theCenter;
            theCase.mY = theOnMap ? (float)theUnits[0]->mTileY + 0.5f : theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            float theFirstX = theCase.mX, theFirstY = theCase.mY;
            theCase.mX = theCenter;
            theCase.mY = theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            // Add Patrol Point at the same two points (the selection with a path point first takes the point into its
            // route; the empty one makes a lone patrol)
            theCase.mBody = kSweepAddPatrolPoint;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = theFirstX;
            theCase.mY = theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            // Path To at the same two points, with no flag and with all four (facing 0.75)
            theCase.mBody = kSweepPathTo;
            for (int f = 0; f < 2; f++) {
                theCase.mFlags = f ? 0xfu : 0u;
                theCase.mFacing = f ? 0.75f : 0.0f;
                theCase.mX = theFirstX;
                theCase.mY = theFirstY;
                RunCase(theCase, p, thePlayer, theUnits);
                theCase.mX = theCenter;
                theCase.mY = theCenter;
                RunCase(theCase, p, thePlayer, theUnits);
            }
            theCase.mFlags = 0;
            theCase.mFacing = 0.0f;
            theCase.mBody = kSweepPatrol;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            for (size_t t = 0; t < theTargets.size(); t++) {
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Gather from the world's first resource with something left, from its first unit and from no unit;
            // Repair its first unit, the next player's and no unit; each plain and queued
            for (int q = 0; q < 2; q++) {
                theCase.mQueued = q != 0;
                theCase.mBody = kSweepGather;
                for (size_t t = 0; t < theGatherTargets.size(); t++) {
                    theCase.mTarget = theGatherTargets[t];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
                theCase.mBody = kSweepRepair;
                for (size_t t = 1; t < theTargets.size(); t++) {
                    if (thePathPoint != NULL && theTargets[t] == (int32_t)IDOf(thePathPoint))
                        continue;
                    theCase.mTarget = theTargets[t];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
                // Attack Unit: the next player's first unit, its own first unit, the resource, no unit; plain and
                // queued, the first target also as a conversion
                theCase.mBody = kSweepAttackUnit;
                std::vector<int32_t> theAttackTargets(theTargets.begin() + 1, theTargets.end());
                if (theResource != NULL)
                    theAttackTargets.push_back((int32_t)IDOf(theResource));
                for (size_t t = 0; t < theAttackTargets.size(); t++) {
                    if (thePathPoint != NULL && theAttackTargets[t] == (int32_t)IDOf(thePathPoint))
                        continue;
                    theCase.mTarget = theAttackTargets[t];
                    for (int c = 0; c < (t == 0 ? 2 : 1); c++) {
                        theCase.mConvert = c != 0;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
                }
                theCase.mConvert = false;
            }
            // Heal Unit: its own first unit, the next player's, no unit
            theCase.mBody = kSweepHealUnit;
            theCase.mQueued = false;
            for (size_t t = 1; t < theTargets.size(); t++) {
                if (thePathPoint != NULL && theTargets[t] == (int32_t)IDOf(thePathPoint))
                    continue;
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Dropoff Resource at its own first unit, the next player's, its path point and no unit
            theCase.mBody = kSweepDropoffResource;
            for (size_t t = 1; t < theTargets.size(); t++) {
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Build Wall from the first unit's tile (or the map's centre) eight tiles east, and a one-tile wall at the
            // centre (the dry host plans nothing: the technology is only logged)
            theCase.mBody = kSweepBuildWall;
            theCase.mTarget = 0x2a;
            theCase.mX = theFirstX;
            theCase.mY = theFirstY;
            theCase.mEndX = theFirstX + 8.0f;
            theCase.mEndY = theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = theCenter;
            theCase.mY = theCenter;
            theCase.mEndX = theCenter;
            theCase.mEndY = theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mEndX = 0.0f;
            theCase.mEndY = 0.0f;
            // Unload Transport at the first unit's tile (or the map's centre) and at the centre
            theCase.mBody = kSweepUnloadTransport;
            theCase.mTarget = 0;
            theCase.mX = (float)(long)theFirstX;
            theCase.mY = (float)(long)theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = (float)(long)theCenter;
            theCase.mY = (float)(long)theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            // Build Unit: a granary, a house and a capitol (the technologies the player's tree makes them with) and a
            // technology it has no node of, at the first unit's tile and the centre, plain and queued (the dry host
            // lets every production and placement through)
            theCase.mBody = kSweepBuildUnit;
            int32_t theBuildTechs[4] = {FindTechnology(thePlayer, kUnitTypeGranary), FindTechnology(thePlayer, kUnitTypeHouse),
                                        FindTechnology(thePlayer, kUnitTypeCapitol), 0x7e00};
            for (int b = 0; b < 4; b++) {
                if (theBuildTechs[b] == -1)
                    continue;
                theCase.mTechnology = theBuildTechs[b];
                for (int q = 0; q < 2; q++) {
                    theCase.mQueued = q != 0;
                    theCase.mX = (float)(long)theFirstX;
                    theCase.mY = (float)(long)theFirstY;
                    RunCase(theCase, p, thePlayer, theUnits);
                    if (b == 0) {
                        theCase.mX = (float)(long)theCenter;
                        theCase.mY = (float)(long)theCenter;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
                }
            }
            theCase.mQueued = false;
            // Smart Farmers at its own first unit, the next player's, its path point, no unit and its first granary
            theCase.mBody = kSweepSmartFarmers;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            std::vector<int32_t> theFarmTargets(theTargets.begin() + 1, theTargets.end());
            for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
                if (theUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
                    continue;
                EEUnitClass* theClass = static_cast<EEComplexUnit*>(theUnit)->mUnitClass;
                if (theClass != NULL && (theClass->mUnitType == kUnitTypeGranary || theClass->mUnitType == kUnitTypeGranaryImproved)) {
                    theFarmTargets.push_back((int32_t)IDOf(theUnit));
                    break;
                }
            }
            for (size_t t = 0; t < theFarmTargets.size(); t++) {
                theCase.mTarget = theFarmTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Cast Calamity: casters its first unit and its first unit with power; targets none and the next player's
            // first unit; the calamity technologies of its tree (one of each class kind, Teleport); at the first unit's
            // tile (the dry host answers every CanCastAt "can")
            theCase.mBody = kSweepCastCalamity;
            std::vector<int32_t> theCalamities;
            FindCalamityTechnologies(thePlayer, theCalamities);
            std::vector<int32_t> theCasters;
            if (!theOwn.empty())
                theCasters.push_back((int32_t)IDOf(theOwn[0]));
            for (size_t k = 0; k < theOwn.size(); k++)
                if (theOwn[k]->GetPower() > 0) {
                    theCasters.push_back((int32_t)IDOf(theOwn[k]));
                    break;
                }
            int32_t theCastTargets[2] = {-1, theTargets.size() > 2 ? theTargets[2] : -1};
            theCase.mX = (float)(long)theFirstX;
            theCase.mY = (float)(long)theFirstY;
            for (size_t c = 0; c < theCasters.size(); c++)
                for (int t = 0; t < 2; t++)
                    for (size_t k = 0; k < theCalamities.size(); k++) {
                        theCase.mCaster = theCasters[c];
                        theCase.mTarget = theCastTargets[t];
                        theCase.mTechnology = theCalamities[k];
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
            theCase.mCaster = 0;
            theCase.mTechnology = 0;
            // Set Airport Command (game messages lane, round 1): the commands of each queue kind (8, 10, 13, 15, 20)
            // and one with no queue (11), at the first unit's tile, toward no unit, the next player's first unit,
            // its own first unit and an id no unit has, plain and queued
            theCase.mBody = kSweepSetAirportCommand;
            theCase.mX = theFirstX;
            theCase.mY = theFirstY;
            int32_t theAirportCommands[6] = {8, 10, 13, 15, 20, 11};
            std::vector<int32_t> theOrderTargets;
            theOrderTargets.push_back(-1);
            if (theTargets.size() > 2)
                theOrderTargets.push_back(theTargets[2]);
            if (!theOwn.empty())
                theOrderTargets.push_back((int32_t)IDOf(theOwn[0]));
            theOrderTargets.push_back(0x7e000001);
            for (int c = 0; c < 6; c++)
                for (size_t t = 0; t < theOrderTargets.size(); t++)
                    for (int q = 0; q < 2; q++) {
                        theCase.mCommand = theAirportCommands[c];
                        theCase.mTarget = theOrderTargets[t];
                        theCase.mQueued = q != 0;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
            theCase.mCommand = 0;
            theCase.mQueued = false;
            // Set Rally Goal: from its first unit, first airport, navy yard and building (a land building), toward no
            // unit, the world's resource, the building itself, the next player's first unit, its path point, its
            // first garrison, farm and damaged building, and an id no unit has; plain and queued
            theCase.mBody = kSweepSetRallyGoal;
            std::vector<int32_t> theBuildings;
            if (!theOwn.empty())
                theBuildings.push_back((int32_t)IDOf(theOwn[0]));
            int32_t theFound[6] = {-1, -1, -1, -1, -1, -1};   // airport, navy yard, building, garrison, farm, damaged
            for (size_t k = 0; k < theOwn.size(); k++) {
                EEComplexUnit* theUnit = theOwn[k];
                EEUnitClass* theClass = theUnit->mUnitClass;
                if (theClass == NULL)
                    continue;
                int32_t theID = (int32_t)IDOf(theUnit);
                if (theFound[0] == -1 && theClass->mUnitType == kUnitTypeAirport)
                    theFound[0] = theID;
                if (theFound[1] == -1 && theClass->mUnitType == kUnitTypeNavyYard)
                    theFound[1] = theID;
                if (theFound[2] == -1 && theClass->IsLandBuilding())
                    theFound[2] = theID;
                if (theFound[3] == -1 && theClass->mGarrisonCapacity > 0)
                    theFound[3] = theID;
                if (theFound[4] == -1 && theClass->mUnitType == kUnitTypeFarm)
                    theFound[4] = theID;
                if (theFound[5] == -1 && theClass->IsLandBuilding() && theUnit->mHitPoints < theUnit->mStats->mMaxHitPoints)
                    theFound[5] = theID;
            }
            for (int k = 0; k < 3; k++)
                if (theFound[k] != -1)
                    theBuildings.push_back(theFound[k]);
            std::vector<int32_t> theRallyTargets;
            theRallyTargets.push_back(-1);
            if (theResource != NULL)
                theRallyTargets.push_back((int32_t)IDOf(theResource));
            if (theTargets.size() > 2)
                theRallyTargets.push_back(theTargets[2]);
            if (thePathPoint != NULL)
                theRallyTargets.push_back((int32_t)IDOf(thePathPoint));
            for (int k = 3; k < 6; k++)
                if (theFound[k] != -1)
                    theRallyTargets.push_back(theFound[k]);
            theRallyTargets.push_back(0x7e000001);
            for (size_t b = 0; b < theBuildings.size(); b++) {
                theCase.mCaster = theBuildings[b];
                for (size_t t = 0; t <= theRallyTargets.size(); t++)
                    for (int q = 0; q < 2; q++) {
                        // the extra target: the building itself (the rally point cleared)
                        theCase.mTarget = t < theRallyTargets.size() ? theRallyTargets[t] : theBuildings[b];
                        theCase.mQueued = q != 0;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
            }
            theCase.mCaster = 0;
            theCase.mQueued = false;
            // Clear Airport Commands and Stop Active Group over the selection
            theCase.mTarget = 0;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            theCase.mBody = kSweepClearAirportCommands;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mBody = kSweepStopActiveGroup;
            RunCase(theCase, p, thePlayer, theUnits);
            // Load Garrison (game messages lane, round 2): into its first garrison, its first unit, the next player's
            // first unit and an id no unit has
            std::vector<int32_t> theCarriers;
            if (theFound[3] != -1)
                theCarriers.push_back(theFound[3]);
            if (!theOwn.empty())
                theCarriers.push_back((int32_t)IDOf(theOwn[0]));
            if (theTargets.size() > 2)
                theCarriers.push_back(theTargets[2]);
            theCarriers.push_back(0x7e000001);
            theCase.mBody = kSweepLoadGarrison;
            for (size_t t = 0; t < theCarriers.size(); t++) {
                theCase.mTarget = theCarriers[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Load Transport: onto its first transport and the carriers above, with the selection, its last unit as
            // the passenger and a passenger id no unit has
            int32_t theTransport = -1;
            for (size_t k = 0; k < theOwn.size() && theTransport == -1; k++)
                if (theOwn[k]->mUnitClass != NULL && theOwn[k]->mUnitClass->IsTransport())
                    theTransport = (int32_t)IDOf(theOwn[k]);
            if (theTransport != -1)
                theCarriers.insert(theCarriers.begin(), theTransport);
            int32_t thePassengers[3] = {-1, theOwn.empty() ? -1 : (int32_t)IDOf(theOwn.back()), 0x7e000001};
            theCase.mBody = kSweepLoadTransport;
            for (size_t t = 0; t < theCarriers.size(); t++)
                for (int k = 0; k < 3; k++) {
                    theCase.mTarget = theCarriers[t];
                    theCase.mCaster = thePassengers[k];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
            theCase.mCaster = 0;
            // Research Technology (game messages lane, round 2): at its first unit, first land building, the next
            // player's first unit and an id no unit has; the first technology of its tree of each kind 0-9 and its
            // first hero's; once and five times
            std::vector<int32_t> theResearchTechs;
            {
                EETechTree* theTree = thePlayer->mTechnologies;
                int32_t theByKind[11];
                for (int k = 0; k < 11; k++)
                    theByKind[k] = -1;
                for (long e = 0; e < theTree->mEpochs.size(); e++) {
                    std::vector<long> theIDs;
                    theTree->mEpochs[e].GetNodeIDs(theIDs);
                    for (size_t i = 0; i < theIDs.size(); i++) {
                        EETechTreeNode* theNode = theTree->Get(theIDs[i]);
                        if (theNode == NULL || theNode->mData == NULL)
                            continue;
                        long theKind = theNode->mData->mKind;
                        if (theKind >= 0 && theKind < 10 && theByKind[theKind] == -1)
                            theByKind[theKind] = (int32_t)theIDs[i];
                        long theClass = theNode->mData->mUnitClass;
                        if (theByKind[10] == -1 && theClass > 0 && (unsigned long)theClass < EEUnitClassCount()
                            && gUnitClassTable[theClass] && gUnitClassTable[theClass]->mAIKind == kAIKindHero)
                            theByKind[10] = (int32_t)theIDs[i];
                    }
                }
                for (int k = 0; k < 11; k++)
                    if (theByKind[k] != -1)
                        theResearchTechs.push_back(theByKind[k]);
            }
            std::vector<int32_t> theResearchAt;
            if (!theOwn.empty())
                theResearchAt.push_back((int32_t)IDOf(theOwn[0]));
            if (theFound[2] != -1)
                theResearchAt.push_back(theFound[2]);
            if (theTargets.size() > 2)
                theResearchAt.push_back(theTargets[2]);
            theResearchAt.push_back(0x7e000001);
            theCase.mBody = kSweepResearchTechnology;
            for (size_t b = 0; b < theResearchAt.size(); b++)
                for (size_t k = 0; k < theResearchTechs.size(); k++)
                    for (int q = 0; q < 2; q++) {
                        theCase.mTarget = theResearchAt[b];
                        theCase.mTechnology = theResearchTechs[k];
                        theCase.mQueued = q != 0;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
            theCase.mTechnology = 0;
            theCase.mQueued = false;
            // Hack A New Unit (game messages lane, round 2): no class (the cheat class), its first unit's class, its
            // first building's, the world's resource's and a class past the table; for itself at the first unit's
            // tile, the map's centre, its corner and its far edge
            std::vector<int32_t> theHackClasses;
            theHackClasses.push_back(-1);
            if (!theOwn.empty() && theOwn[0]->mUnitClass != NULL)
                theHackClasses.push_back((int32_t)theOwn[0]->mUnitClass->mUnitType);
            if (theFound[2] != -1)
                theHackClasses.push_back((int32_t)ObjectOf((uint32_t)theFound[2])->mUnitClass->mUnitType);
            if (theResource != NULL && static_cast<EEUnit*>(theResource)->mUnitClass != NULL)
                theHackClasses.push_back((int32_t)static_cast<EEUnit*>(theResource)->mUnitClass->mUnitType);
            theHackClasses.push_back(0x7fff);
            float theEdge = (float)gWorld.GetMap()->GetSize() - 0.5f;
            float theHackPoints[4][2] = {{theFirstX, theFirstY}, {theCenter, theCenter}, {0.5f, 0.5f}, {theEdge, theEdge}};
            theCase.mBody = kSweepHackANewUnit;
            theCase.mTarget = (int32_t)thePlayer->mPlayerID;
            for (size_t k = 0; k < theHackClasses.size(); k++)
                for (int t = 0; t < 4; t++) {
                    theCase.mTechnology = theHackClasses[k];
                    theCase.mX = theHackPoints[t][0];
                    theCase.mY = theHackPoints[t][1];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
            theCase.mTechnology = 0;
        }
    }
}
