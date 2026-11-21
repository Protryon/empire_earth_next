// EEUnitClassBuildings.cpp (new, R5b.4 round 32): the buildings' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>
#include <functional>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/GEModel.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUIManager.h"
#include "ee/EEUnitUtilities.h"
#include "rust/sim/units.h"

// C:\EEX\Empire Earth\Units\EEUCBuilding.cpp (__FILE__ 0x4c8f28: the volcano assert, line 261). The buildings' unit
// class: 0x20e58d..0x21005e (after EEUCBomb.cpp; EEUCBuildingPriest.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (EEUnitClass.cpp's factory). Order: Virtual04, VirtualE0, EruptVolcano (the volcano erupts),
// CreateUnitAt, CreateUnit, the wall/tower helpers (0x20ea01..0x20ed72: EEGetFacingQuarter = the map quarter a tile faces),
// Virtual44, Virtual74, Virtual78, Virtual84, ReleaseUnit, VirtualA8, Virtual98, VirtualA0, VirtualB4, VirtualBC,
// VirtualB8, UpdateArmed, then EECalamityManager::GetCalamityClass<EECCVolcano>.

// 0x446ff0/0x446ff4: the unit's named constants (one .rdata copy per unit)
static const float kBuildingLift = 0.001f;      // 0x446ff0 (a building sits just above the ground)

// 0x446ff4 (-2.0, below it a building is gone) is ee_units cls_condition.rs BUILDING_SUNK

// 0x446dc0: the yaw of each map quarter (EEGetFacingQuarter's 0..3) (provisional name)

// Puts docks and navy yards on the water plane and lifts other buildings just above the ground
void EEUnitClass::BuildingGroundLocation(GE3DPoint& ioLocation)
{
    if (this->mUnitType == kUnitTypeDock || this->mUnitType == kUnitTypeNavyYard)
        ioLocation.SetZ(0.0f);
    else
        ioLocation.SetZ(ioLocation.GetZ() + kBuildingLift);
}

// Picks the first fully healed garrisoned unit that should leave (a human's unit whose rally queue has a goal, a unit
// with a goal, or one with an explore order) as the exiting unit; returns whether one was picked
bool EEUnitClass::BuildingSelectUnitToRelease(EEComplexUnit* inUnit)
{
    return ee_units_ucb_call(EE_UNITS_UCB_DO_SELECT_TO_RELEASE, this, inUnit, 0, 0, 0, NULL) != 0;   // ucb_release.rs
}

// Erupts a volcano: gives it an attack-ground goal at a random spot near it; returns true
bool EEUnitClass::EruptVolcano(EEComplexUnit* inUnit)
{
    EECalamityClass* theVolcanoClass = NULL;
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    if (!USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(9, theVolcanoClass))
        ESVerify(__FILE__, 261, "EECalamityManager::Instance()->FindCalamityClass(keectVolcano, theVolcanoClass)"); // the original assert text (provisional names here)
    inUnit->GetPosition(thePosition);
    theVolcanoClass->GetRandomLocation(thePosition, theTarget);
    inUnit->SetGoal(EEUGRust::NewAttackGround(theTarget, false), 0, 0);
    return true;
}

// Creates a building at a placement: docks and navy yards on the water plane, others (not the camera object) lifted
// just above it
EEUnit* EEUnitClass::BuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                   EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_CREATE_UNIT_AT, this, NULL, EEObj(inCreator), inPlayer, inIsFoundation, &inTransformation.mMatrix);   // ucb_release.rs
}

// Creates a building on a tile's centre (docks and navy yards at water level, off-map tiles at the -10000 sentinel)
EEUnit* EEUnitClass::BuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_CREATE_UNIT, this, NULL, EEObj(inCreator), (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)inPlayer * 2 + inIsFoundation, NULL);   // ucb_release.rs
}

// Returns the map quarter facing the most space terrain within 6 tiles of a tile (half weight to the neighbouring
// sides; its terrain run 0x60ea01 and the side tables 0x4c8ef8/0x4c8f08: ee_units cls_building.rs, R5b.4 round 33)
long EEGetDockFacing(unsigned long inX, unsigned long inY)
{
    EEUCNote theNote(23); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_dock_facing((uint32_t)inX, (uint32_t)inY));
}

// Returns a navy yard's facing at a tile: its map quarter if 5 deep-water tiles follow that way, else the next side
// that has them, else the side with the longest run (cls_building.rs)
static long GetNavyYardFacing(unsigned long inX, unsigned long inY)
{
    EEUCNote theNote(22); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_navy_yard_facing((uint32_t)inX, (uint32_t)inY));
}

// Returns the map quarter (0..3) a tile faces from the middle of its continent (space maps) or of the map
// (cls_building.rs)
long EEGetFacingQuarter(long inX, long inY)
{
    EEUCNote theNote(21); theNote.I(inX).I(inY);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_facing_quarter((int32_t)inX, (int32_t)inY));
}

// Returns the map quarter the unit's tile faces
long EEGetUnitFacingQuarter(EEComplexUnit* inUnit)
{
    return EEGetFacingQuarter(inUnit->GetTileX(), inUnit->GetTileY());
}

// Returns the model part for a wall piece kind (0..8); outFlag is set for the corner kinds 0, 2, 6 and 8
// (inferred meaning)
long EEGetWallPiecePart(long inIndex, bool* outFlag)
{
    return ee_units_ucb_wall_piece_part((int32_t)inIndex, outFlag);   // ucb_release.rs
}

// Returns the first living targetable building found in rings 1..19 around a unit's tile
EEUnit* EEUnitClass::FindNearestTargetBuilding(EEUnit* inUnit)
{
    EEUCNote theNote(35); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    EEUnit* theResult = ([&]() -> EEUnit* {
        std::vector<EETileCoordinate> theTiles;
        EEUnit* theResult = NULL;
        EEMap* theMap = ::gWorld.mMap;
        for (unsigned long theRing = 1; theResult == NULL && theRing < 0x14; theRing++) {
            ee_units_uu_square_ring_tiles(inUnit->GetTileX(), inUnit->GetTileY(), theRing, &theTiles);
            for (std::vector<EETileCoordinate>::iterator theTile = theTiles.begin();
                 theResult == NULL && theTile != theTiles.end(); theTile++) {
                EEMapTile* theMapTile = theMap->GetTile(theTile->mX, theTile->mY);
                if (theMapTile == NULL)
                    continue;
                for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if (theUnit != NULL && theUnit->mUnitClass->IsLandBuilding() && theUnit->mHitPoints > 0 &&
                        ee_units_uu_targetable(0, (uint64_t)(uintptr_t)theUnit->GetUnitClass(), 0)) {
                        theResult = theUnit;
                        break;
                    }
                }
            }
        }
        return theResult;
    }());
    theNote.U(theResult);
    return theResult;
}

// Lists the tiles around a building; an airport's include its runway (four times its size toward its facing)
void EEUnitClass::BuildingGetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners,
                             bool inAppend)
{
    ee_units_ucb_call(EE_UNITS_UCB_DO_SURROUNDING_TILES, this, inUnit, EEObj(&outTiles), inCorners, inAppend, NULL);   // ucb_release.rs
}

// Sets up a new building (built, its resource stock, regeneration); an airport turns to its map quarter, a navy yard
// picks its facing and a space dock its yaw
void EEUnitClass::BuildingUnitCreated(EEComplexUnit* inUnit)
{
    ee_units_ucb_call(EE_UNITS_UCB_DO_UNIT_CREATED, this, inUnit, 0, 0, 0, NULL);   // ucb_release.rs
}

// Launches the exiting unit: places it at the exit (runway, water or tag point), gives it the building's rally
// goals or its own (else loads it back) and starts its takeoff from this home base
void EEUnitClass::ReleaseUnit(EEComplexUnit* inUnit)
{
    ee_units_ucb_call(EE_UNITS_UCB_DO_RELEASE_UNIT, this, inUnit, 0, 0, 0, NULL);   // ucb_release.rs
}

// Runs an armed building's tick (physics, turrets, timers, animation, sound) and fires on its attack frame; skips
// disabled ones and standing foundations
void EEUnitClass::UpdateArmed(EEComplexUnit* inUnit)
{
    ee_units_ucb_call(EE_UNITS_UCB_DO_UPDATE_ARMED, this, inUnit, 0, 0, 0, NULL);   // ucb_release.rs
}

// ---- EEUCBuildingPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). A building that
// converts (the temple): 0x21005e..0x21013b (after EEUCBuilding.cpp; EEUCFortress.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory); the slots pick the building's or the priest's handling
// by command/event. RTTI gives EEUCBuildingPriest no EEUCBasicPriest base, so the priest half is the class's own copy
// of EEUCBasicPriest's bodies: identical source compiled to identical code, which LINK's ICF folded into
// EEUCBasicPriest's (0x20d59f, 0x20d7f2, 0x20d998, 0x20dd0a, 0x20dd2d, 0x20dee4, 0x20df05, 0x20e16a, 0x20e274). With
// one class the temple runs the basic priest's bodies themselves (R5b.4 round 32); only the update's copy is left
// (PriestVirtualB8: the basic priest's update is a Rust sequence the temple does not run).

// The temple's copy of EEUCBasicPriest::Update (0x20df05)
void EEUnitClass::PriestVirtualB8(EEComplexUnit* inUnit)
{
    ee_units_ucb_call(EE_UNITS_UCB_DO_PRIEST_UPDATE, this, inUnit, 0, 0, 0, NULL);   // ucb_release.rs
}

// ---- EEUCGate.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The wall gate's unit class:
// 0x21048f..0x212a6f (after EEUCFortress.cpp; EEUCHelicopter.cpp follows). One unit per EEUC* class in alphabetical
// order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). The unit opens with the
// `$E` chains of its two file-static colours, then Virtual20, EEBuildGate (EEMBuildGate's work), the slots and the
// gate helpers in address order; EEMap's corner-height inlines and the sort/unique COMDATs over the tile vector
// close it. A gate is five linked pieces (+0x1cc/+0x1d0): piece 2 is the gate, 0/4 the posts, 1/3 the doors.

// The highlight of a wall that can take a gate, and the normal colour (`$E` chains 0x21048f / 0x2104bf; within one
// unit later globals get lower addresses: 0x5634f0 / 0x5634e0) (provisional names)
static GEColor sGateHighlight(0.0f, 1.0f, 0.0f);

static GEColor sGateNormal(1.0f, 1.0f, 1.0f);

// 0x447004..0x447018: the unit's named float constants (one .rdata block per unit) (provisional names)
// 0x447004/0x447008 (1.0 open, 0.0 closed) are ee_units cls_condition.rs GATE_OPEN, GATE_CLOSED

static const float kGateStep = 0.2f;            // 0x44700c (per update)

long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex);

void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles);

bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits);

bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

// 0x24c627 (inTiles' units of inUnitType): ee/EEUnitUtilities.h

// 0x24f8f9: after a gate is built for the current player (the new gate's piece 2) (provisional name)
void EECreateSpaceGateEnergyWall(EEComplexUnit* inGate);

// Replaces the other four pieces of a gate whose gate piece was replaced (new owner and class), turning and
// relinking them; the posts go idle and the doors take the new gate piece's action
void EEUnitClass::GateGateReplaced(EEUnit* inGate, EEUnit* inUnit)
{
    std::vector<EEUnit*> thePieces;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    unsigned long theDirection = inUnit->GetComplexUnit()->mGateDirection;
    thePieces.resize(5);
    thePieces[2] = inUnit;
    for (long i = 0; i <= 4; i++) {
        if (i == 2)
            continue;
        EEComplexUnit* theOld = this->GetPart(inGate->GetComplexUnit(), i);
        EEComplexUnit* thePiece = EEReplaceUnit(theOld, inUnit->GetPlayer()->GetPlayerID(), theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, theDirection, i);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(theDirection);
        thePieces[i] = thePiece;
    }
    thePieces[4]->GetComplexUnit()->LinkPart(thePieces[0]->GetComplexUnit());
    thePieces[0]->GetComplexUnit()->LinkPart(thePieces[3]->GetComplexUnit());
    thePieces[3]->GetComplexUnit()->LinkPart(thePieces[1]->GetComplexUnit());
    thePieces[1]->GetComplexUnit()->LinkPart(thePieces[2]->GetComplexUnit());
    thePieces[4]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[0]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[3]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());
    thePieces[1]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());

}

// Turns the five wall pieces across a tile into a closed gate of the technology's class for a player (EEMBuildGate's
// work): weakest wall's health, posts on the lower ground; adds the energy wall once space gates are available
void EEBuildGate(EETileCoordinate& inTile, long inPlayer, long inTechnology, long inDirection)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theWalls;
    std::vector<EEUnit*> theGate;
    GE3DPoint thePost4;
    U2DPoint<long> theTile;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    GE3DPoint thePost0;
    float theHealth = 1.0f;
    EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    theTile.mX = inTile.mX;
    theTile.mY = inTile.mY;
    EEGetWallGateTiles(theTile, inDirection, theTiles);
    ee_units_uu_units_on(0, &theTiles, &theUnits);
    for (unsigned long i = 0; i < theUnits.size(); i++) {
        if (theUnits[i]->GetUnitClass()->mIsWall)
            theWalls.push_back(theUnits[i]);
    }
    std::vector<EEUnit*> theSide;
    if (theWalls.size() == 0 || !EEDoesWallGateFit(inTile, inDirection, theWalls[0]->GetUnitClass()->GetUnitType(), theSide))
        return;
    long j;
    std::vector<EEUnit*>::iterator it;
    for (j = 0, it = theWalls.begin(); j <= 4; j++, it++) {
        if (it == theWalls.end())
            break;
        float theRatio = (float)(*it)->mHitPoints / (*it)->mStats->mMaxHitPoints;
        if (theHealth >= theRatio)
            theHealth = theRatio;
        long thePlayerID = thePlayer->GetPlayerID();
        EEComplexUnit* thePiece = EEReplaceUnit((EEComplexUnit*)*it, thePlayerID, theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, inDirection, j);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(inDirection);
        theGate.push_back(thePiece);
    }
    theGate[4]->mComplexUnit->GetPosition(thePost4);
    theGate[0]->GetComplexUnit()->GetPosition(thePost0);
    float theZ = min(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost4.GetX(), thePost4.GetY()),
                     ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost0.GetX(), thePost0.GetY()));
    thePost4.SetZ(theZ);
    thePost0.SetZ(theZ);
    theGate[4]->GetComplexUnit()->SetLocation(thePost4, true);
    theGate[0]->GetComplexUnit()->SetLocation(thePost0, true);
    theGate[4]->GetComplexUnit()->LinkPart(theGate[0]->GetComplexUnit());
    theGate[0]->GetComplexUnit()->LinkPart(theGate[3]->GetComplexUnit());
    theGate[3]->GetComplexUnit()->LinkPart(theGate[1]->GetComplexUnit());
    theGate[1]->GetComplexUnit()->LinkPart(theGate[2]->GetComplexUnit());
    theGate[4]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[0]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[3]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[1]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[2]->mComplexUnit->StartAction(kActionClosed);
    theGate[2]->SetHitPoints(UMFloatToLong(theGate[2]->GetStats()->mMaxHitPoints * theHealth));
    if (gUIManager->mInScenarioEditor && !gGameScreenActive) {
        ::gWorld.FlushQueuedPlacements();
        thePlayer->AddPendingUnits(0, true);
        thePlayer->RemoveKilledUnits();
    }
    if (thePlayer->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && theClass->GetUnitType() != kUnitTypeGatePalisades)
        EECreateSpaceGateEnergyWall(EEUnitCast<EEComplexUnit>(theGate[2]));
}

// The sort-and-unique of a vector (cdecl template 0x211ebc and its range half 0x211f07) (provisional names)
template <class V, class L, class E> typename V::iterator EESortUniqueRange(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    typename V::iterator theResult;
    theResult = std::unique(ioValues.begin(), ioValues.end(), inEqual);
    return theResult;
}

template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    typename V::iterator theEnd = EESortUniqueRange(ioValues, inLess, inEqual);
    ioValues.erase(theEnd, ioValues.end());
}

// Collects the footprint tiles of every gate piece not left out (doors and gate piece while open), sorted and unique
void EEUnitClass::GateGetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    if (!inAppend)
        outTiles.clear();
    EEComplexUnit* thePiece = inUnit->GetComplexUnit();
    while (thePiece->mPreviousPart != NULL)
        thePiece = thePiece->mPreviousPart;
    for (; thePiece != NULL; thePiece = thePiece->mNextPart) {
        if (!this->IsPartLeftOut(thePiece))
            this->BaseGetSurroundingTiles(thePiece, outTiles, inCorners, true);
    }
    // the COMDATs sort U2DPoint<long>s (operator< 0x13710, vector<U2DPoint<long> >::erase 0x288efb); EETileCoordinate
    // is U2DPoint<long>, so the tiles need no vector cast
    EESortUnique(outTiles, std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >());
}

// Returns whether a gate fits across a wall unit's tile in a direction (1..4), giving the five walls of the type
// (EECPWallManager.cpp declares it the same way)
bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return EEDoesWallGateFit(theTile, inSide, inUnitType, outUnits);
}

// Returns whether a gate fits across a tile in a direction: five built, living walls of the type on level tiles
// (else outUnits is emptied)
bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits)
{
    EEUCNote theNote(25); theNote.T(inTile).I(inDirection).I(inUnitType);   // TEST ONLY (UCBD)
    bool theResult = theNote.R([&]() -> bool {
        std::vector<U2DPoint<long> > theTiles;
        std::vector<EEUnit*> theUnused;
        bool theResult = true;
        U2DPoint<long> theTile(inTile.mX, inTile.mY);
        EEGetWallGateTiles(theTile, inDirection, theTiles);
        ee_units_uu_units_of_type_on_tiles(&theTiles, inUnitType, &outUnits);
        std::vector<EEUnit*>::iterator it = outUnits.begin();
        while (true) {
            if (it == outUnits.end())
                break;
            if ((*it)->mComplexUnit->mIsBuilt && (*it)->mHitPoints > 0)
                theResult = true;
            else
                theResult = false;
            it++;
            if (!theResult)
                break;
        }
        if (theResult && outUnits.size() == 5)
            theResult = EEAreTilesLevel(theTiles);
        else {
            outUnits.clear();
            theResult = false;
        }
        return theResult;
    }());
    theNote.Us(outUnits);
    return theResult;
}

// Returns the first piece of a unit's linked chain (+0x1cc)
EEComplexUnit* EEGetFirstPart(EEComplexUnit* inUnit)
{
    while (inUnit->mPreviousPart != NULL)
        inUnit = inUnit->mPreviousPart;
    return inUnit;
}

// Returns the middle unit of a list (the lower of the two middles for an even count)
static EEUnit* EEGetMiddleOf(std::vector<EEUnit*>& inUnits)
{
    unsigned long theIndex = inUnits.size() / 2;
    if (inUnits.size() % 2 == 0)
        theIndex--;
    return inUnits[theIndex];
}

// Turns gate piece inIndex for a gate in direction 1..4; returns its animation: 9 a post, 10 a diagonal post, 11 else
// (the yaws: cls_building.rs)
long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex)
{
    EEUCNote theNote(24); theNote.I(inDirection).I(inIndex);   // TEST ONLY (UCBD)
    float theYaw;
    bool theTurned;
    long theResult = ee_units_cls_gate_part_turn((int32_t)inDirection, (int32_t)inIndex, &theYaw, &theTurned);
    if (theTurned)
        ioTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theNote.R(theResult);   // TEST ONLY (UCBD)
    theNote.F(ioTransformation.GetYaw());
    return theResult;
}

// Returns piece inIndex (0..4) of a gate, found along its linked chain (2 is the gate piece itself)
EEComplexUnit* EEUnitClass::GetPart(EEComplexUnit* inGate, long inIndex)
{
    EEUCNote theNote(29); theNote.C(this).U(inGate).I(inIndex);   // TEST ONLY (UCBD)
    EEComplexUnit* theResult = ([&]() -> EEComplexUnit* {
        EEComplexUnit* theResult = NULL;
        switch (inIndex) {
        case 4:
            theResult = this->GetLastLinked(inGate);
            break;
        case 3:
            theResult = this->GetFourthPart(inGate);
            break;
        case 2:
            theResult = inGate;
            break;
        case 1:
            theResult = this->GetSecondPart(inGate);
            break;
        case 0:
            theResult = this->GetLastLinkedCarrier(inGate);
            break;
        }
        return theResult;
    }());
    theNote.U(theResult);
    return theResult;
}

// Lists the five tiles of a gate line through a tile: direction 1 along x, 2 along y, 3 and 4 the diagonals
// (cls_building.rs)
void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles)
{
    EEUCNote theNote(33); theNote.T(inTile).I(inDirection).Ts(outTiles);   // TEST ONLY (UCBD)
    ee_units_cls_wall_gate_tiles((int32_t)inTile.mX, (int32_t)inTile.mY, (int32_t)inDirection, &outTiles);
    theNote.Ts(outTiles);   // TEST ONLY (UCBD)
}

// Returns whether a living enemy unit of unit set 0x62 is within reach of the gate or its next two pieces
// (the squared distance is compared with the unsquared reach; cls_building.rs)
bool EEUnitClass::AreEnemiesNear(EEComplexUnit* inUnit)
{
    EEUCNote theNote(27); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_gate_near(EEObj(inUnit), false));
}

// Returns whether a unit that may pass is within reach of the gate or its next two pieces: a mobile land unit
// the owner is not hostile to, or an enemy of unit set 0x62 (cls_building.rs)
bool EEUnitClass::IsPasserNear(EEComplexUnit* inUnit)
{
    EEUCNote theNote(28); theNote.C(this).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_gate_near(EEObj(inUnit), true));
}

// Returns whether a tile holds a highlighted living wall that can take a gate, giving its gate direction
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection)
{
    bool theFound = false;
    EEMapTile* theTile = ::gWorld.GetMap()->GetTile(inTile.mX, inTile.mY);
    if (theTile != NULL) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->mUnitClass->mIsWall && theUnit->mHitPoints > 0 && theUnit->mTileNode != NULL &&
                theUnit->GetComplexUnit()->mIsTinted) {
                theFound = true;
                ioDirection = theUnit->GetComplexUnit()->mGateDirection;
            }
        }
    }
    return theFound;
}

// Returns the piece before the last one of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinkedCarrier(EEComplexUnit* inUnit)
{
    return this->GetLastLinked(inUnit)->mPreviousPart;
}

// Returns gate piece 1 (the piece before piece 3); NULL when the chain is too short (a gate created as a single
// piece, e.g. by a scenario effect)
EEComplexUnit* EEUnitClass::GetSecondPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theFourth = this->GetFourthPart(inGate);
    return theFourth != NULL ? theFourth->mPreviousPart : NULL;
}

// Returns the last piece of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinked(EEComplexUnit* inUnit)
{
    while (inUnit->mNextPart != NULL)
        inUnit = inUnit->mNextPart;
    return inUnit;
}

// Returns gate piece 3 (two pieces before the end of the chain); NULL when the chain is too short (a gate created
// as a single piece, e.g. by a scenario effect)
EEComplexUnit* EEUnitClass::GetFourthPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theCarrier = this->GetLastLinkedCarrier(inGate);
    return theCarrier != NULL ? theCarrier->mPreviousPart : NULL;
}

// Restores the normal colour of a player's walls of a type and clears their gate direction (after gate placement)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    ee_units_uu_player_units_of_type(inUnitType, inPlayer->mPlayerID, &theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        EEComplexUnit* theUnit = (*it)->mComplexUnit;
        if (theUnit == NULL)
            continue;
        theUnit->SetColor(sGateNormal);
        theUnit->SetIsTinted(false);
        theUnit->SetGateDirection(0);
    }
}

// Sets up a new gate piece the building's way; the chain's first piece starts closed
void EEUnitClass::GateUnitCreated(EEComplexUnit* inUnit)
{
    this->BuildingUnitCreated(inUnit);
    EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(inUnit->GetUnitClass());
    if (inUnit->mPreviousPart == NULL && inUnit->mNextPart != NULL) {
        inUnit->StartAction(kActionClosed);
        this->ActionChanged(inUnit);
    }
}

// Highlights the middle wall of a gate line through each of a player's walls of a type when one or three of the
// four directions fit (an XOR of the four tests); the first fitting direction is used
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theSide1;
    std::vector<EEUnit*> theSide2;
    std::vector<EEUnit*> theSide3;
    std::vector<EEUnit*> theSide4;
    EEClearGateHighlights(inPlayer, inUnitType);
    ee_units_uu_player_units_of_type(inUnitType, inPlayer->mPlayerID, &theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        bool the1 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 1, inUnitType, theSide1);
        bool the2 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 2, inUnitType, theSide2);
        bool the3 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 3, inUnitType, theSide3);
        bool the4 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 4, inUnitType, theSide4);
        if (the1 ^ the2 ^ the3 ^ the4) {   // the first fitting direction's middle wall
            std::vector<EEUnit*>* theSides[4] = {&theSide1, &theSide2, &theSide3, &theSide4};
            bool theFits[4] = {the1, the2, the3, the4};
            for (long d = 0; d < 4; d++) {
                if (!theFits[d])
                    continue;
                EEComplexUnit* theWall = EEGetMiddleOf(*theSides[d])->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(d + 1);
                }
                break;
            }
        }
        theSide1.clear();
        theSide2.clear();
        theSide3.clear();
        theSide4.clear();
    }
}

// Returns whether all tiles are on the map, flat or ramps (0 or 2 raised corners), and differ in height by at most
// 0.6 (0x447014; cls_building.rs) (EECPWallManager.cpp declares it the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles)
{
    EEUCNote theNote(26); theNote.Ts(inTiles);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cls_tiles_level(&inTiles));
}

// ---- EEUCMiningBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The mining
// building's unit class (a mine built on a resource unit): 0x217085..0x217337 (after EEUCLandTransport.cpp;
// EEUCMissile.cpp follows). Unit flags: the exe's module set. Its ctor is inline (vtable 0x44843c).

// Creates a mine at a placement and ties it to the first resource under its footprint that it can gather
EEUnit* EEUnitClass::MiningBuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                         EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_MINING_CREATE_UNIT_AT, this, NULL, EEObj(inCreator), inPlayer, inIsFoundation, &inTransformation.mMatrix);   // ucb_release.rs
}

// Creates a mine on a tile, at the tile's height (0 off the map)
EEUnit* EEUnitClass::MiningBuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_MINING_CREATE_UNIT, this, NULL, EEObj(inCreator), (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)inPlayer * 2 + inIsFoundation, NULL);   // ucb_release.rs
}

// ---- EEUCNavalBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval building's
// unit class (a dock): 0x218158..0x218490 (after EEUCNaval.cpp; EEUCNavalCitizen.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline. Virtual74/78/88 are ICF-folded into EEUCSpaceBuilding.cpp's copies; VirtualA0/E0
// here are also EEUCSpaceBuilding's (ICF).

// Picks the first fully healed docked ship that should leave (a human's ship whose own or the dock's rally goal is an
// attack, move or explore goal; any ship with a goal for a computer player); returns whether one was picked
bool EEUnitClass::NavalBuildingSelectUnitToRelease(EEComplexUnit* inUnit)
{
    return ee_units_ucb_call(EE_UNITS_UCB_DO_NAVAL_SELECT_TO_RELEASE, this, inUnit, 0, 0, 0, NULL) != 0;   // ucb_release.rs
}

// Returns whether the dock's transformed bounding box lies wholly below the water line
bool EEUnitClass::NavalBuildingHasSunk(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    const GEPhysicalModel& thePhysicalModel = inUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    inUnit->GetPosition(thePosition);
    GETransformation theTransformation(false);
    thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                         theTransformation);
    float theScale = thePhysicalModel.GetScale();
    theTransformation.Scale(theScale, theScale, theScale);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    theBox = theModel->GetBoundingBox(inUnit->mPhysicalModel.GetFrame());
    theBox.Transform(theTransformation, theTransformedBox);
    return theTransformedBox.GetTopRightBack().GetZ() < 0.0f ? true : false;
}

// ---- EEUCSpaceBuilding.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The space
// building's unit class: 0x219fe3..0x21a219 (after EEUCSeaAnimal.cpp; EEUCSpaceship.cpp follows). Unit flags: the
// exe's module set. Its ctor is inline. Virtual74/78/88 are also EEUCNavalBuilding's (ICF: these copies are kept);
// VirtualA0/E0 are EEUCNavalBuilding.cpp's (ICF).

// Returns the tile under a location along the camera line, at the cruise altitude
void EEUnitClass::SpaceBuildingLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    float theZ = this->mCruiseAltitude;
    theLine.SolveForZ(theZ, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// Creates a space building at a placement, at the cruise altitude
EEUnit* EEUnitClass::SpaceBuildingCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                        EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_SPACE_CREATE_UNIT_AT, this, NULL, EEObj(inCreator), inPlayer, inIsFoundation, &inTransformation.mMatrix);   // ucb_release.rs
}

// Creates a space building on a tile, at the cruise altitude
EEUnit* EEUnitClass::SpaceBuildingCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    return (EEUnit*)(uintptr_t)ee_units_ucb_call(EE_UNITS_UCB_DO_SPACE_CREATE_UNIT, this, NULL, EEObj(inCreator), (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)inPlayer * 2 + inIsFoundation, NULL);   // ucb_release.rs
}

// Sets up a new space building as a building, already built
void EEUnitClass::SpaceBuildingUnitCreated(EEComplexUnit* inUnit)
{
    this->BuildingUnitCreated(inUnit);
    inUnit->SetBuilt(1, 0, 1);
}

// Returns whether the space building may stop: already sinking, a slow request, or at its next waypoint's tile
bool EEUnitClass::SpaceBuildingHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theLocation;
    bool theResult = false;
    if (inUnit->mCollisionState == 8 || inValue <= 0.05f)
        theResult = true;
    else {
        inUnit->GetNextWaypointPoint(theLocation);
        if (inUnit->GetTileX() == UMFloor(theLocation.GetX()) && inUnit->GetTileY() == UMFloor(theLocation.GetY()))
            theResult = true;
    }
    return theResult;
}

template <class T> static T* EEUcbObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// ReleaseUnit's yaw is the orientation's first float
static_assert(offsetof(GEOrientation, mYaw) == 0, "a GEOrientation starts with its yaw");

// The C++ of the buildings' Rust bodies (ee_units ucb_release.rs; EE_UNITS_UCB_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucb_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcbObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCB_LOCATION_YAW: theUnit->mPhysicalModel.GetLocation().ConvertToOrientation(*reinterpret_cast<GEOrientation*>(f)); return 0;
    case EE_UNITS_UCB_CARGO_SIZE: return theUnit->mCargo.size();
    case EE_UNITS_UCB_CARGO_INDEX: return EEObj(theUnit->mCargo[(size_t)inB]);
    case EE_UNITS_UCB_HIT_POINTS_OF: return theOther->GetHitPoints();
    case EE_UNITS_UCB_MAX_HIT_POINTS_OF: return theOther->GetStats()->mMaxHitPoints;
    case EE_UNITS_UCB_HUMAN_OF: return theOther->GetPlayer()->IsHuman();
    case EE_UNITS_UCB_QUEUE_SHIFT_MODE: return theUnit->mGoalQueues[(size_t)inB].GetUnit()->mPlayer->mShiftMode;
    case EE_UNITS_UCB_GOAL_STATE_OF: return EEObj(theOther->mGoalState);
    case EE_UNITS_UCB_UNIT_ID_OF: return theOther->mUnitID;
    case EE_UNITS_UCB_SET_EXITING: theUnit->SetExitingUnitID((unsigned long)inB); return 0;
    case EE_UNITS_UCB_DETACHED: return theOther->mIsDetachedFromAI;
    case EE_UNITS_UCB_CLEAR_QUEUE0: theOther->mGoalQueues[0].Clear(); return 0;
    case EE_UNITS_UCB_REFRESH_GOALS: theOther->RefreshGoals(); return 0;
    case EE_UNITS_UCB_REFRESH_ACTIVE: theOther->RefreshActiveGoal(); return 0;
    case EE_UNITS_UCB_QUEUE_LEN_OF: return theOther->GetQueueGoals((unsigned long)inB).size();
    case EE_UNITS_UCB_QUEUE_FRONT_OF: return EEObj(theOther->mGoalQueues[(size_t)inB].GetFront());
    case EE_UNITS_UCB_EXPLORE_CAST: return EEObj(EEExploreGoalCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_ATTACK_CAST: return EEObj(EEAttackUnitCast(EEUcbObject<EEUnitGoal>(inA)));
    case EE_UNITS_UCB_HAS_LOCATION: return EEGoalHasLocation(EEUcbObject<EEUnitGoal>(inA));
    case EE_UNITS_UCB_HIT_POINTS_FIELD_OF: return theOther->mHitPoints;
    case EE_UNITS_UCB_MAX_HIT_POINTS_FIELD_OF: return theOther->mStats->mMaxHitPoints;
    case EE_UNITS_UCB_PLAYER_HUMAN_OF: return theOther->mPlayer->IsHuman();
    case EE_UNITS_UCB_COMPUTER_PLAYER: return EEObj(theUnit->mPlayer->mComputerPlayer);
    case EE_UNITS_UCB_EXITING_ID: return theUnit->mExitingUnitID;
    case EE_UNITS_UCB_AIR_TRAFFIC_SELF: return theUnit->mUnitClass->mAirTrafficType;
    case EE_UNITS_UCB_TILE_SIZE_FIELD: return theClass->mTileSize;
    case EE_UNITS_UCB_FACING: return theUnit->mFacing;
    case EE_UNITS_UCB_FIND_PASSENGER: return EEObj(theUnit->FindPassenger((unsigned long)inB));
    case EE_UNITS_UCB_TAGS_EMPTY: return EEUcbObject<EETagPointList>(inA)->mPoints.empty();
    case EE_UNITS_UCB_TAGS_SCALE_STATS: EEUcbObject<EETagPointList>(inA)->Scale(*EEUcbObject<GETransformation>((uint64_t)inB), theUnit->mStats->GetScale()); return 0;
    case EE_UNITS_UCB_TAG0_POINT: EEUcbObject<EETagPointList>(inA)->mPoints[0]->GetPoint(EEUcbObject<EEComplexUnit>((uint64_t)inB)->mPhysicalModel.GetFrame(), *EEUcbObject<GETransformation>((uint64_t)inC), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCB_SET_CARGO_SCALE: theOther->mPhysicalModel.SetScale(theOther->mStats->GetScale() * ((f[0] * 0.5f) / theOther->mUnitClass->GetCruiseAltitude() + 0.5f)); return 0;
    case EE_UNITS_UCB_UNLOAD_BY_ID: return EEObj(theUnit->UnloadCargoByID((unsigned long)inB, *EEUcbObject<GETransformation>(inA)));
    case EE_UNITS_UCB_CATEGORY_SELF: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_UCB_QUEUE_GOAL_OF: return EEObj(theOther->GetQueueGoals((unsigned long)inB)[(size_t)inC]);
    case EE_UNITS_UCB_UNIT_HIT_POINTS: return EEUcbObject<EEUnit>(inA)->mHitPoints;
    case EE_UNITS_UCB_UNIT_TYPE_SELF: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_UCB_CATEGORY_OF: return theOther->mUnitClass->mCategory;
    case EE_UNITS_UCB_FAMILY_OF: return theOther->mUnitClass->mFamily;
    case EE_UNITS_UCB_PARATROOPER_OF: return theOther->mUnitClass->mIsParatrooperBomber;
    case EE_UNITS_UCB_GOAL_OF: return EEObj(theOther->GetGoal());
    case EE_UNITS_UCB_LOAD_CARGO: theUnit->LoadCargo(theOther, false, 0); return 0;
    case EE_UNITS_UCB_ATC_REMOVE_TAKEOFF: ee_units_atc_remove_from_takeoff_queues(theOther->mPlayer->mAirTraffic[theOther->mUnitClass->mAirTrafficType - 1], theOther); return 0;
    case EE_UNITS_UCB_UNIT_QUEUE_LEN: return theUnit->mGoalQueues[(size_t)inB].mGoals.size();
    case EE_UNITS_UCB_UNIT_QUEUE_GOAL: return EEObj(theUnit->mGoalQueues[(size_t)inB].mGoals[(size_t)inC]);
    case EE_UNITS_UCB_TILE_FREE: return EEIsTileFree(theOther, (unsigned long)inB, (unsigned long)inC);
    case EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCB_UNIT_TYPE_OF: return theOther->mUnitClass->mUnitType;
    case EE_UNITS_UCB_SET_HOME_BASE: theOther->SetHomeBase(theUnit); return 0;
    case EE_UNITS_UCB_SET_BUILT: theUnit->SetBuilt((long)inB, (long)(inC & 1), (long)(inC >> 1)); return 0;
    case EE_UNITS_UCB_SET_RESOURCE_TYPE: theUnit->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCB_RESOURCE_TYPE: return theUnit->mResourceType;
    case EE_UNITS_UCB_SET_RESOURCE_AMOUNT: theUnit->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCB_FACING_QUARTER: return EEGetUnitFacingQuarter(theUnit);
    case EE_UNITS_UCB_SET_FACING: theUnit->SetFacing((unsigned long)inB); return 0;
    case EE_UNITS_UCB_NAVY_YARD_FACING: return GetNavyYardFacing(theUnit->GetTileX(), theUnit->GetTileY());
    case EE_UNITS_UCB_SPACE_DOCK_YAW: f[0] = EEGetSpaceDockYaw(theUnit->GetTileX() + 1, theUnit->GetTileY() + 1, theUnit->mUnitClass->GetTileSize()); return 0;
    case EE_UNITS_UCB_DISABLED: return theUnit->mIsDisabled;
    case EE_UNITS_UCB_PHYSICS_ACTION: return theUnit->mCurrentPhysicsAction;
    case EE_UNITS_UCB_UPDATE_TURRETS: theUnit->UpdateTurrets(); return 0;
    case EE_UNITS_UCB_TILE_X_FIELD: return theUnit->mTileX;
    case EE_UNITS_UCB_UPDATE_HEAL_SEARCH: theUnit->UpdateHealSearchTimer(); return 0;
    case EE_UNITS_UCB_IS_MEDIC: return theClass->mIsMedic;
    case EE_UNITS_UCB_GOAL_WORK: theUnit->mGoalState->Work(theUnit); return 0;
    case EE_UNITS_UCB_REGENERATION: return theUnit->mStats->mRegeneration;
    case EE_UNITS_UCB_REGENERATE_POWER: theClass->RegeneratePower(theUnit); return 0;
    case EE_UNITS_UCB_PRIEST_CONVERSION: theClass->BasicPriestUpdateConversion(theUnit); return 0;
    case EE_UNITS_UCB_CONVERSION_START: return theUnit->mConversionStartTime;
    case EE_UNITS_UCB_CLEAR_CONVERSION_START: theUnit->mConversionStartTime = 0; return 0;
    case EE_UNITS_UCB_RELEASE_CONVERSION: theUnit->ReleaseConversionEffect(); return 0;
    case EE_UNITS_UCB_HIDE_CONVERSION: theClass->BasicPriestHideConversionEffects(theUnit); return 0;
    case EE_UNITS_UCB_CRUISE_ALTITUDE: f[0] = theClass->mCruiseAltitude; return 0;
    case EE_UNITS_UCB_CAN_GATHER: return EEUcbObject<EEUnit>(inA)->CanGather(EEUcbObject<EEUnit>((uint64_t)inB), EEUcbObject<EEUnit>((uint64_t)inB)->GetTileX(), EEUcbObject<EEUnit>((uint64_t)inB)->GetTileY(), 0, 0);
    case EE_UNITS_UCB_TILE_UNITS: return EEObj(EEUcbObject<EETile>(inA)->mUnits);
    case EE_UNITS_UCB_NODE_UNIT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mUnit);
    case EE_UNITS_UCB_NODE_NEXT: return EEObj(EEUcbObject<RTSTileUnitNode>(inA)->mNext);
    case EE_UNITS_UCB_SET_MINE: EEUcbObject<EEUnit>(inA)->mComplexUnit->SetMine(EEUcbObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCB_PREVIOUS_PART_OF: return EEObj(theOther->mPreviousPart);
    case EE_UNITS_UCB_TILE_SIZE_OF: return theOther->mUnitClass->GetTileSize();
    case EE_UNITS_UCB_FACING_QUARTER_AT: return EEGetFacingQuarter((long)inB, (long)inC);
    case EE_UNITS_UCB_TILES_CLEAR: EEUcbObject<std::vector<EETileCoordinate>>(inA)->clear(); return 0;
    case EE_UNITS_UCB_BASE_SURROUNDING: theClass->BaseGetSurroundingTiles(theUnit, *EEUcbObject<std::vector<EETileCoordinate>>(inA), (long)inB, inC != 0); return 0;
    case EE_UNITS_UCB_COMPLEX_FIELD_OF: return EEObj(theOther->mComplexUnit);
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
