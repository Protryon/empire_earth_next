// New (port), TEST ONLY: the units sweep of EE-SIM --unit-sweep (the simulation capture's units profile,
// tools/sim_capture.py; the physics models' lane, round 2). No game of the other capture profiles steps an airplane,
// a submarine, a sea animal or a spaceship (and a helicopter only in the calamity game), so every five game minutes
// this sweep makes every player such units and sends them at the next player: the models' bodies (pm_air.rs,
// pm_ground.rs) and the carrier planes' deck walk run in a played game, and PMST records them.
//
// Each call, for every player with a unit on the map (its anchor: its first living building there, else its first
// living unit): first its producers, each made once and again after its death, built, at the first tile around the
// anchor (rings out to 40) where EECanPlaceOnTile takes it: an airport, a dock, a navy yard, an aircraft carrier
// (on water) and a space carrier (in space; a map without water or space has none of those). Then one unit of every
// second class of the swept models (airplane, helicopter, submarine, sea animal, spaceship; not the buildings,
// shots, bombs, missiles and torpedoes), the half by the call and player, produced as the game produces it
// (EEUnitClass::ProduceUnit): aircraft into the producer of their air traffic type (airport, carrier, navy yard,
// space carrier), sea units at the dock, space units at the space carrier, the rest at the anchor. Each new unit
// then gets one goal at the next player: armed units attack (aircraft and helicopters the anchor, or the carrier
// every second class; sea units the carrier, else the dock; space units the space carrier), the others move to it.
// The computer players' AI may take the units over afterwards. Everything is in the unit and class tables' order
// and draws no random number of its own; the same code runs over the original C++ models for the golden.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitClassIDs.h"

void EEPhysicsUnitSweep(long inCall);

namespace {

// the producers, by their index in sProducers' rows; an aircraft's air traffic type t is produced by kTraffic[t]
enum { kAirport, kDock, kNavyYard, kCarrier, kSpaceCarrier, kProducers };
const long kProducerTypes[kProducers] = {kUnitTypeAirport, kUnitTypeDock, kUnitTypeNavyYard,
                                         kUnitTypeS11EnterpriseAircraftCarrier, kUnitTypeSp15SpaceCarrier};
const int kTraffic[5] = {-1, kAirport, kCarrier, kNavyYard, kSpaceCarrier};

// each player's producers' unit ids (0: none yet)
std::vector<std::vector<unsigned long>> sProducers;

bool OnMap(EEUnit* inUnit)
{
    return inUnit != NULL && inUnit->mComplexUnit != NULL && inUnit->mHitPoints > 0 && inUnit->mTileNode != NULL
           && inUnit->GetTileX() != -10000;
}

EEComplexUnit* Anchor(EEPlayer* inPlayer)
{
    EEUnit* theFirst = NULL;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!OnMap(theUnit))
            continue;
        if (theUnit->mComplexUnit->mUnitClass->IsBuilding())
            return theUnit->mComplexUnit;
        if (theFirst == NULL)
            theFirst = theUnit;
    }
    return theFirst != NULL ? theFirst->mComplexUnit : NULL;
}

unsigned long NewestID(EEPlayer* inPlayer)
{
    unsigned long theID = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitID > theID)
            theID = theUnit->mUnitID;
    return theID;
}

// the first tile in rings around the anchor (each ring's row y - r, then its sides, then row y + r) that takes the type
bool FindSpot(EEPlayer* inPlayer, long inType, EEComplexUnit* inAnchor, long& outX, long& outY)
{
    long cx = inAnchor->GetTileX(), cy = inAnchor->GetTileY();
    for (long r = 1; r <= 40; r++) {
        for (long y = cy - r; y <= cy + r; y++) {
            long theStep = (y == cy - r || y == cy + r) ? 1 : 2 * r;
            for (long x = cx - r; x <= cx + r; x += theStep) {
                if (x < 0 || y < 0 || gWorld.mMap->GetTile(x, y) == NULL)
                    continue;
                if (EECanPlaceOnTile(inPlayer, inType, x, y, false, 1)) {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
    }
    return false;
}

EEComplexUnit* Producer(unsigned long inPlayer, int inIndex)
{
    unsigned long theID = sProducers[inPlayer][inIndex];
    EEUnit* theUnit = theID != 0 ? gWorld.GetUnit(theID) : NULL;
    return OnMap(theUnit) ? theUnit->mComplexUnit : NULL;
}

void MakeProducers(EEPlayer* inPlayer, unsigned long p, EEComplexUnit* inAnchor)
{
    for (int i = 0; i < kProducers; i++) {
        if (Producer(p, i) != NULL)
            continue;
        EEUnitClass* theClass = (unsigned long)kProducerTypes[i] < gUnitClassTable.size() ? gUnitClassTable[kProducerTypes[i]] : NULL;
        long x, y;
        if (theClass == NULL || !FindSpot(inPlayer, kProducerTypes[i], inAnchor, x, y))
            continue;
        EEUnit* theUnit = theClass->CreateUnit(x, y, inPlayer->GetPlayerID(), false, NULL);
        if (theUnit == NULL)
            continue;
        // its id first (the player's insert gives it): finishing it activates it, and the active list holds ids
        inPlayer->AddPendingUnits(false, true);
        gWorld.FlushQueuedPlacements();
        if (theUnit->mTileNode != NULL)
            theUnit->GetComplexUnit()->SetBuilt(1, 0, 1);
        gWorld.mMap->PlaceUnitOnGround(theUnit);
        sProducers[p][i] = theUnit->mUnitID;
    }
}

bool Swept(EEUnitClass* inClass)
{
    if (inClass == NULL || inClass->mPhysicsModel == NULL || inClass->IsBuilding() || inClass->IsLandBuilding())
        return false;
    long theKind = -1;   // its gPhysicsModels[] index (the singletons' order: 0 land .. 11 spaceship)
    for (long i = 0; i < 12; i++)
        if (gPhysicsModels[i] == inClass->mPhysicsModel)
            theKind = i;
    if (theKind != 1 && theKind != 2 && theKind != 5 && theKind != 9 && theKind != 11)
        return false;
    long c = inClass->mCategory;
    return c != kUnitCategoryProjectile && c != kUnitCategoryBomb && c != kUnitCategoryMissile
           && c != kUnitCategoryTorpedo && c != kUnitCategoryNavalBuilding && c != kUnitCategorySpaceBuilding;
}

EEComplexUnit* ProducerFor(unsigned long p, EEUnitClass* inClass, EEComplexUnit* inAnchor)
{
    if (inClass->IsAircraft())
        return inClass->mAirTrafficType >= 1 && inClass->mAirTrafficType <= 4
                   ? Producer(p, kTraffic[inClass->mAirTrafficType]) : NULL;
    if ((inClass->GetDomains() & kMovementSpace) != 0)
        return Producer(p, kSpaceCarrier);
    if ((inClass->GetDomains() & kMovementSea) != 0)
        return Producer(p, kDock);
    return inAnchor;
}

}   // namespace

void EEPhysicsUnitSweep(long inCall)
{
    unsigned long n = gWorld.GetNumPlayers();
    if (sProducers.size() < n)
        sProducers.resize(n, std::vector<unsigned long>(kProducers, 0));
    std::vector<EEComplexUnit*> theAnchors(n, NULL);
    for (unsigned long p = 1; p < n; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        theAnchors[p] = thePlayer != NULL ? Anchor(thePlayer) : NULL;
        if (theAnchors[p] != NULL)
            MakeProducers(thePlayer, p, theAnchors[p]);
    }
    long theMade = 0, theFailed = 0;
    for (unsigned long p = 1; p < n; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (theAnchors[p] == NULL)
            continue;
        unsigned long q = p;
        do
            q = q + 1 < n ? q + 1 : 1;
        while (q != p && theAnchors[q] == NULL);
        if (q == p)
            continue;
        EEComplexUnit* theTown = theAnchors[q];
        EEComplexUnit* theCarrier = Producer(q, kCarrier);
        EEComplexUnit* theDock = Producer(q, kDock);
        EEComplexUnit* theSpace = Producer(q, kSpaceCarrier);
        for (unsigned long c = 0; c < gUnitClassTable.size(); c++) {
            EEUnitClass* theClass = gUnitClassTable[c];
            if (!Swept(theClass) || (long)((c + p + (unsigned long)inCall) & 1) != 0)
                continue;
            EEComplexUnit* theProducer = ProducerFor(p, theClass, theAnchors[p]);
            if (theProducer == NULL) {
                theFailed++;
                continue;
            }
            unsigned long theBefore = NewestID(thePlayer);
            bool theDone = theProducer->mUnitClass->ProduceUnit(theProducer, theClass, false);
            thePlayer->AddPendingUnits(false, true);
            gWorld.FlushQueuedPlacements();
            EEComplexUnit* theNew = NULL;
            for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
                if (theUnit->mUnitID > theBefore && theUnit->mComplexUnit != NULL
                    && theUnit->mComplexUnit->mUnitClass == theClass)
                    theNew = theUnit->mComplexUnit;
            if (!theDone || theNew == NULL) {
                theFailed++;
                continue;
            }
            theMade++;
            EEComplexUnit* theTarget;
            if ((theClass->GetDomains() & kMovementSpace) != 0)
                theTarget = theSpace;
            else if ((theClass->GetDomains() & kMovementSea) != 0)
                theTarget = theCarrier != NULL ? theCarrier : theDock;
            else
                theTarget = (c & 2) != 0 && theCarrier != NULL ? theCarrier : theTown;
            if (theTarget == NULL)
                theTarget = theTown;
            EEUnitGoal* theGoal;
            if (theNew->mStats != NULL && theNew->mStats->GetAttack() > 0.0f) {
                theGoal = EEUnitGoal::NewAttackUnit(theNew, theTarget, true, false);
            } else {
                GE3DPoint theAt;
                theTarget->GetPosition(theAt);
                theGoal = EEUnitGoal::NewLocation(&theAt, false, true, false, 0.0f, false, 0);
            }
            theNew->SetGoal(theGoal, 0, 0);
        }
    }
    fprintf(stderr, "EE-UNIT-SWEEP %ld made %ld failed %ld\n", inCall, theMade, theFailed);
}
