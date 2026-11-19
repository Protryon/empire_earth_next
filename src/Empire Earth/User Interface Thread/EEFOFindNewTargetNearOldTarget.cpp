// C:\EEX\Empire Earth\User Interface Thread\EEFOFindNewTargetNearOldTarget.cpp (file name inferred: no __FILE__).
// The game's unit utilities: 0x23dd54..0x2544e9 (EEDSUIEvents.cpp follows). The range opens with a `$E` chain
// (481..483) and the out-of-line predicates its sorts expand; 0x241c2d builds an EEFOFindNewTargetNearOldTarget on
// the stack and 0x241e21 is that class's Virtual04/Virtual08; the rest are cdecl unit utilities (the original file is
// probably "EEUnitUtilities.cpp": EEUU* names, the assert "EEUUGetIsUnitClassTargetable"; the EEFO* classes are its
// local screens). 0x24f0e9..0x24f8f9 are its last functions (the EEHitPointUpgrader functor, the placement tests) and
// 0x24f932..0x2544e9 the template COMDATs its functions instantiate (rbegin/rend for 0x247bef, push_back<GE2DPoint>
// for 0x2481eb, stable_sort for 0x248709, the sorts of 0x241e49/0x246254/0x2481eb; the user templates 0x24fcd1/
// 0x2503d7 (EEUUSearchTiles), 0x250806, 0x250b99, 0x250d71 (an inline EEGetUnitsInArea), 0x25000c).
// 0x24e68f's two function-local static UWideStrings put their atexit dtors at 0x24e7c4/0x24e7cf (not a `$E` chain).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <typeinfo>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <functional>
#include <mutex>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/SESoundManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/UResourceFile.h"
#include "lle/USingleton.h"
#include "lle/UReferenceCounter.h"
#include "lle/LowLevelEngineCore.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMathInline.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitRegions.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETileCoordinate.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEPhysics.h"
#include "ee/EECPTrace.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EEPathManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEDebugTrace.h"
#include "ee/EEGameVariant.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEGhostUnit.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEFOFindNewTargetNearOldTarget.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEFOLocationScreenBase.h"
#include "ee/EEAIUnitTargetingTable.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEMapDimensions.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEAIRandom.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEStringIDs.h"

// EEContagionHelper.cpp's collector over complex units (its only instantiation was EECPPlacement.cpp's)
template long EEGetUnitsInRectangle<EEAnyUnit, std::vector<EEComplexUnit*> >(const U2DRectangle<long>&, std::vector<EEComplexUnit*>&,
                                                                           const EEAnyUnit&, bool);
extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)
extern EEToHitTable gToHitTable;           // 0x5318e8 (as EECPAttackSubGroupUnit.cpp declares it)

// The unit's named float constants (.rdata 0x449b54..0x449ba0, around the vtable 0x449ba8) (provisional names)
// (0x449b54..0x449b60, 0x449b98..0x449ba0: the sound's dead zone, falloffs and distances: ee_units uu_world.rs)
static const float kUUSectorLow = -0.39269909f;  // 0x449bb8 (-pi/8)
static const float kUUSectorScale = 1.2732395f;  // 0x449bbc (4/pi)
static const float kUUSectorHigh = 0.39269909f;  // 0x449bc0 (pi/8)
static const float kUUWorkReach = 1.5f;
static const float kUUGatherReach = 1.42f;
static const float kUUMinimumFacing = 0.1f;
static const float kUUTwoOverPi = 0.63661975f;
static const float kUUWallYaw10 = 5.4977875f;
static const float kUUWallYaw9 = 3.926991f;
static const float kUUWallYaw8 = 4.712389f;
static const float kUUClearance = 0.1f;
static const float kUUYawNudge = 0.0174f;        // 0x449b68 (0x24286a: about one degree)
static const float kUUSearchTurns = 5.0f;
static const float kUUCheckReach = 2.5f;
static const float kUUWidthFactor = 0.5f;
static const float kUUFacingStep = 0.2617994f;   // 0x449bb4 (pi/12)
// (0x23dd54/0x23dd6e/0x23dd82, the dynamic initializers of the sound falloff factors gF564520/gF56451C/gF564518: ee_units
// uu_world.rs sound_parameters computes them)

// the unit utilities' handles (ee_units uu.rs): an object's address (a unit's as its EEUnit*)
static uint64_t UUHandle(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
static EEUnit* UUUnit(uint64_t inUnit) { return (EEUnit*)(uintptr_t)inUnit; }
static EETile* UUTile(uint64_t inTile) { return (EETile*)(uintptr_t)inTile; }
// the first inCapacity tiles of a list into outTiles; their number
static uint32_t UUPairs(const std::vector<U2DPoint<long> >& inTiles, int32_t (*outTiles)[2], uint32_t inCapacity)
{
    for (uint32_t i = 0; i < inTiles.size() && i < inCapacity; i++) {
        outTiles[i][0] = (int32_t)inTiles[i].mX;
        outTiles[i][1] = (int32_t)inTiles[i].mY;
    }
    return (uint32_t)inTiles.size();
}

// TEST ONLY: the unit utilities' record (UUTL) is Rust's (ee_units uu_note.rs): every noted entry point notes its
// call there; the bodies still here note theirs through this wrapper (the same thread depth and record)
class EEUUNote {
public:
    explicit EEUUNote(uint32_t inKind) : mOn(ee_units_uu_note_begin(inKind) != 0) {}
    ~EEUUNote() { ee_units_uu_note_end(); }
    EEUUNote& I(long inValue) { if (this->mOn) ee_units_uu_note_word((uint32_t)inValue); return *this; }
    EEUUNote& F(float inValue) { uint32_t theBits; memcpy(&theBits, &inValue, 4); return this->I((long)theBits); }
    EEUUNote& U(const EEUnit* inUnit) { return this->I(inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2); }
    EEUUNote& P(const GE3DPoint& inPoint) { return this->F(inPoint.mX).F(inPoint.mY).F(inPoint.mZ); }
    EEUUNote& C(EEUnitClass* inClass) { return this->I(inClass != NULL ? (long)inClass->mUnitType : -2); }
    EEUUNote& M(const GETransformation& inTransformation)
    {
        for (int i = 0; i < 16; i++)
            this->F((&inTransformation.mMatrix[0][0])[i]);
        return *this;
    }
    bool mOn;
};
static long UUId(const EEUnit* inUnit) { return inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2; }
static void UUEvent(long inA, long inB = 0, long inC = 0) { ee_units_uu_note_event((int32_t)inA, (int32_t)inB, (int32_t)inC); }
// the record's world: whether the capture runs (0), the physics' ticks (1), a UUTL record written (2)
extern "C" uint32_t ee_units_uu_host_note(int32_t inWhich, uint32_t inTick, uint32_t inCalls, uint32_t inLow, uint32_t inHigh)
{
    if (inWhich == 0)
        return EESimCaptureRecord::Active();
    if (inWhich == 1)
        return gPhysics != NULL ? (uint32_t)gPhysics->mNumTicksDone : 0;
    EESimCaptureRecord theRecord(0x4c545555u);   // "UUTL"
    theRecord.U32(inTick);
    theRecord.U32(inCalls);
    theRecord.U32(inLow);
    theRecord.U32(inHigh);
    theRecord.Commit();
    return 0;
}

// Units by id: EEUUUnitIDLess / EEUUUnitIDEqual are declared in ee/EEUnitCollector.h (its EEGetUnitsInRectangle
// sorts with them too; EEAIArea.cpp's family 0xe3d7.. calls these bodies)

// Orders units by ascending unit id
bool EEUUUnitIDLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    return inFirst->mUnitID < inSecond->mUnitID;
}

// Tells whether two units have the same unit id
bool EEUUUnitIDEqual::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    return inFirst->mUnitID == inSecond->mUnitID;
}

// (the TU's own sort-and-unique keys, highest unit id first: ee_units uu.rs sort_unique_by_id_descending)

// Built without /Og (frame 0x54: the locals of the two undone DistanceSquaredToTile / DistanceSquared2D expansions);
// the pragma alone makes 0x63ddc1 and 0x63de0f exact (r37_big, with packet B1)
#pragma optimize("g", off)
// Orders tiles by their distance from a unit (the nearer first)
bool EEUUTileNearerToUnit::operator()(const U2DPoint<long>& inFirst,
                                    const U2DPoint<long>& inSecond) const
{
    float theFirstDistance = this->mUnit->DistanceSquaredToTile(inFirst);
    float theSecondDistance = this->mUnit->DistanceSquaredToTile(inSecond);
    bool theResult = theFirstDistance < theSecondDistance;
    return theResult;
}

// (0x63de0f, EEUUPointNearerToUnit, EEFindOpenSpot's sort order: ee_units uu_spots.rs find_open_spot)

#pragma optimize("g", on)

// (0x63deae, EEUUTileNearerToPoint, EEGetSortedRingTiles' sort order: ee_units uu.rs sorted_ring_tiles)

// Accepts a tile that is on the map and on the screen's continent and path area (when set)
bool EEFOLocationScreenEnsureContinentAndPathOverlay::Screen(const U2DPoint<long>& inTile)
{
    if (this->mB08 && ::gWorld.mMap->GetTileAtLocation(inTile) == NULL)
        return false;
    if (this->mContinentID != 0) {
        if (inTile.mX >= gMapWidth || inTile.mY >= gMapHeight)
            return false;
        if (gContinentManager.GetContinentID(inTile.mX, inTile.mY) != this->mContinentID)
            return false;
    }
    if (this->mAreaID == 0)
        return true;
    if (inTile.mX >= gMapWidth || inTile.mY >= gMapHeight)
        return false;
    if (gContinentManager.GetContinent(inTile, this->mUseWaterAreas ? 2 : 1) != this->mAreaID)
        return false;
    return true;
}

// Smooths a ship's path: keeps the ends and drops points whose straight line from the last kept point stays
// on passable water away from other units
// The naval path smoothing (EEUCNaval::AdjustPath): when inSmooth, keep the first and last points and drop each point
// whose straight line from the last kept point stays on passable tiles away from other units
void EEAdjustNavalPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    static_assert(sizeof(GE3DPoint) == 12, "a path point is three floats");
    std::vector<uint32_t> theKept(ioPath.size() + 1);
    uint32_t theCount = ee_units_uu_naval_path(UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])(ioPath.empty() ? NULL : &ioPath[0].mX),
                                               (uint32_t)ioPath.size(), inSmooth, &theKept[0]);
    if (!inSmooth)
        return;
    std::vector<GE3DPoint> theNewPath;
    for (uint32_t i = 0; i < theCount; i++)
        theNewPath.push_back(ioPath[theKept[i]]);
    ioPath = theNewPath;
}

// Collects the units on a tile that block a unit type for a player; true when there are any
bool EEGetBlockingUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner,
               std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_tile_units_standing(UUHandle(inPlayer), inUnitType, UUHandle(inTile), inIgnoreOwner, 2, &outUnits) != 0;
}


// Hands the wall pieces out to the group's builders until every piece has one; units that had another goal
// start over
void EEBuildWall(EEActiveGroup* inGroup, std::vector<EEUnit*>& inPieces)
{
    ee_units_uu_build_wall(UUHandle(inGroup), &inPieces);   // ee_units uu_world.rs build_wall (0x63e600 EEHasZeroCount too)
}

void EEWorkFarm(EEComplexUnit* inTarget, EEComplexUnit* inUnit)
{
    if (inTarget->mIsBuilt && inTarget->GetHitPoints() == inTarget->mStats->mMaxHitPoints) {
        inUnit->SetGoal(EEUGRust::NewGather(inTarget, NULL, false), 0, 0);
        inUnit->StartAction(kActionMoveToResource);
    } else {
        inUnit->SetGoal(EEUGRust::NewBuildRepair(inTarget->GetPlayer(), inTarget, false), 0, 0);
        inUnit->StartAction(kActionMoveToRepairSite);
    }
    inUnit->GetUnitClass()->ActionChanged(inUnit);
}


// Shows the wall ghosts for a wall line of a technology and player
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer)
{
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    std::vector<long> theCorners;
    EEDropRepeatedTiles(inTiles);
    EEDropDiagonalCorners(inTiles);
    EEPickWallPieces(inTiles, theCorners, ::gWorld.mMap);
    EEBuildWallGhosts(inTiles, theCorners, inTechnology, thePlayer, 0.0f);
}

// Builds a wall line for a player: a new piece (paid foundation or finished) on every free tile, existing
// pieces reshaped and joined
void EEPlanWall(GE2DPoint inStart, GE2DPoint inEnd, EEUnitClass* inClass, EEPlayer* inPlayer, bool inAsFoundation, bool inSkipPlayerChecks,
                std::vector<EEUnit*>& outPieces)
{
    ee_units_uu_plan_wall(inStart.mX, inStart.mY, inEnd.mX, inEnd.mY, UUHandle(inClass), UUHandle(inPlayer), inAsFoundation,
                          inSkipPlayerChecks, &outPieces);   // ee_units uu_world.rs plan_wall
}


// (0x63efa1, EEGetSeabedUnder: ee_units uu_world.rs seabed_under)

// Puts a submarine at its cruise depth, or at the surface when the seabed is too high
void EEGetSeabedDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    ioLocation.SetZ(ee_units_uu_seabed(0, UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])&ioLocation.mX));
}

// Moves a submarine one small step up or down towards its cruise depth, staying above the seabed
void EEAdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    ioLocation.SetZ(ee_units_uu_seabed(1, UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])&ioLocation.mX));
}

// Tells whether a unit can be pushed aside: alive and not a building, aircraft, projectile or fixed object
bool EEIsUnitMovable(EEComplexUnit* inPlayer, EEComplexUnit* inUnit)
{
    return ee_units_uu_is_unit_movable(UUHandle(static_cast<EEUnit*>(inUnit))) != 0;   // ee_units uu_reads.rs
}

// Finds a free spot for a unit on a tile, stepping in from a neighbour; false when every step collides
bool EEFindPlaceOnTile(EEComplexUnit* inUnit, EEUnitClass* inClass, long inX, long inY, long inSize, long inToX, long inToY,
                long inSquare, GE3DPoint* outLocation)
{
    return ee_units_uu_find_place_on_tile(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inClass), inX, inY, inSize, inToX, inToY, inSquare,
                                          (float(*)[3])&outLocation->mX) != 0;
}

// Collects the tiles around a container unit for a unit, nearest first
void EEGetTilesAroundUnit(EEComplexUnit* inUnit, EEComplexUnit* inContainer, long inRange, long inCorners, bool inArg5,
                std::vector<EETileCoordinate>& outTiles)
{
    ee_units_uu_tiles_around(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)), inRange, inCorners, inArg5, &outTiles);
}

// Collects the ring tiles at a range around a square, sorted by distance from a point
void EEGetSortedRingTiles(const GE3DPoint& inFrom, long inX, long inY, long inSize, long inRange, long inCorners,
                std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_sorted_ring_tiles((const float(*)[3])&inFrom.mX, inX, inY, inSize, inRange, inCorners, &outTiles);
}

// Returns the foundation model a player uses for a building class of its size
EEModel* EEGetPlayerClassGraphic(EEPlayer* inPlayer, EEUnitClass* inClass)
{
    if (inClass->mUnitType == kUnitTypeRoboticFarm)
        return gUnitClassTable[kUnitTypeSpaceFallowFarm]->mModel;
    if (inClass->mUnitType != kUnitTypeAirport) {
        switch (inClass->mTileSize) {
        case 1:
            return inPlayer->mUnitClassUpgrades[0x136]->mModel;
        case 2:
            return inPlayer->mUnitClassUpgrades[0x132]->mModel;
        case 3:
        case 4:
            return inPlayer->mUnitClassUpgrades[0x133]->mModel;
        default:
            return inPlayer->mUnitClassUpgrades[kUnitTypeFoundation6x6]->mModel;
        }
    }
    return inPlayer->mUnitClassUpgrades[kUnitTypeFoundationAirport]->mModel;
}

// Tells whether two techs count as the same research (0x432 and 0x5ab are twins)
bool EEIsSameResearch(long inFirst, long inSecond)
{
    return ee_units_uu_is_same_research(inFirst, inSecond) != 0;
}

// Picks a free spot next to a container among candidate tiles that the unit can stand on and reach
bool EEPickTileNear(std::vector<EETileCoordinate>& inTiles, EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIgnoreBlockedLine,
                GE3DPoint& outLocation)
{
    return ee_units_uu_pick_tile_near(&inTiles, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)),
                                      inIgnoreBlockedLine, (float(*)[3])&outLocation.mX) != 0;
}

// Tells whether a unit may stand on a tile: on the map, off its edge, allowed terrain, no blocking unit
bool EECanStandOn(const EETileCoordinate& inTile, EEComplexUnit* inUnit)
{
    return ee_units_uu_can_stand_on(inTile.mX, inTile.mY, UUHandle(static_cast<EEUnit*>(inUnit))) != 0;
}

// Computes where an attacker should aim to intercept a moving target (else its location)
void EEGetLeadPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EECombatAttackPosition(false, inAttacker, inTarget, outLocation);   // ee_combat leaves.rs lead_position
}

// Computes the launch pitch for a shot at its speed under gravity (low or high arc); false (45 degrees) when
// out of reach
// The launch angle for a shot from inFrom to inTo at inStats' speed under its projectile class's gravity (the low
// arc, or the high one for a class with +0x2e5 or a mortar-like unit); false (and PI/4) when it cannot reach
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo,
                float* outAngle)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EeCombatBallisticIn theIn;
    theIn.from_x = inFrom->GetX();
    theIn.from_y = inFrom->GetY();
    theIn.from_z = inFrom->GetZ();
    theIn.to_x = inTo->GetX();
    theIn.to_y = inTo->GetY();
    theIn.to_z = inTo->GetZ();
    theIn.speed = inStats->mLaunchSpeed;
    theIn.acceleration = inStats->mUnitClass->mProjectileClass->GetAcceleration();
    theIn.high = theClass->mShootsHigh || (inStats->mCanFireOverWalls && inUnit->mIsFiringHigh && inUnit->mAnimation == 8);
    EeCombatBallisticOut theOut = EECombatBallistic(theIn);
    *outAngle = theOut.angle;
    return theOut.reached != 0;
}

// Points a unit's attack goal at a new target and restarts its attack
// inUnit's attack goal (inGoal is an EEUGAttackUnit: 0x241c2d's dynamic_cast) moves on to inTarget, and the unit starts
// its attack action (0x7dd) over
void EERetargetAttack(EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEComplexUnit* inTarget)
{
    EEComplexUnit* theOld = static_cast<EEUGRust*>(inGoal)->GetAttackTarget()->mComplexUnit;
    if (inTarget != theOld)
        inGoal->ReplaceUnit(theOld, inTarget, NULL);
    inUnit->mCollisionResolutionCount = 0.0f;
    inUnit->SetBlockingUnit(NULL);
    if (inUnit->mCollisionState != 0)
        inUnit->mCollisionState = 0;
    inUnit->DropPath();
    inUnit->StartAction(kActionCheckRange);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// Finds a unit a new target near its old one: another piece of a linked (wall/gate) target, else the
// nearest living enemy within 5 tiles
void EEFindNewTargetNearOld(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    EETileCoordinate theTile;
    EEComplexUnit* theLink;

    inUnit->IgnoreTarget(inTarget);
    if (inTarget->mPreviousPart == NULL && inTarget->mNextPart == NULL) {
        theTile.mX = inTarget->GetTileX();
        theTile.mY = inTarget->GetTileY();
        EEFOFindNewTargetNearOldTarget theScreen(inTarget, 25.0f);
        if (!EEFindNewTarget(&theScreen, inUnit->GetLoadGoal(), inUnit, &theTile, 2, true, 0))
            EEDebugTrace(">>> SHAWN %d could not find a target.\n", inTarget->GetUnitClass()->GetName());
    } else {
        theLink = inTarget->GetOutermostCarrier()->mNextPart;
        while (true) {
            if (!(theLink->mNextPart != NULL))
                break;
            if (!theLink->mUnitClass->IsPartLeftOut(theLink) && !inUnit->IsTargetIgnored(theLink, false)) {
                EERetargetAttack(inUnit, EEAttackUnitCast(inUnit->GetLoadGoal()), theLink);
                return;
            }
            theLink = theLink->mNextPart;
        }
        if (inTarget->mUnitClass->GetCategory() == kUnitCategoryGate) {
            EEComplexUnit* theFirst = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(inTarget->mUnitClass)->GetLastLinkedCarrier(
                theLink->GetOutermostCarrier());
            EEComplexUnit* theSecond = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(inTarget->GetUnitClass())->GetLastLinked(
                theFirst->GetOutermostCarrier());
            float theDistance = inUnit->DistanceSquaredTo(theFirst);
            theLink = theFirst;
            if (inUnit->DistanceSquaredTo(theSecond) < theDistance)
                theLink = theSecond;
        }
        theTile.mX = theLink->GetTileX();
        theTile.mY = theLink->GetTileY();
        EEFOFindNewTargetNearOldTarget theScreen(theLink, 25.0f);
        if (!EEFindNewTarget(&theScreen, inUnit->GetLoadGoal(), inUnit, &theTile, 1, true, 0))
            EEDebugTrace(">>> SHAWN %d could not find a target.\n", theLink->GetUnitClass()->GetName());
    }
}

// Accepts a living unit within the screen's distance of the old target (new: ee_combat's screen)
// the old target's screen's candidate distance (asked by ee_combat's screen for a living candidate only)
struct EENearOldScreenContext {
    EEComplexUnit* mCandidate;
    EEComplexUnit* mTarget;
};

static float EENearOldScreenDistance(void* inContext)
{
    EENearOldScreenContext* theContext = (EENearOldScreenContext*)inContext;
    return theContext->mCandidate->DistanceSquaredTo(theContext->mTarget);
}

static bool EENearOldScreen(EEFOFindNewTargetNearOldTarget* inScreen, EEComplexUnit* inUnit)
{
    EENearOldScreenContext theContext = {inUnit, inScreen->mTarget};
    EeCombatScreenIn in = {EE_COMBAT_SCREEN_KIND_NEAR_OLD, (int32_t)inUnit->mHitPoints, inScreen->mDistanceSquared, 0, 0.0f};
    int32_t theResult = ee_combat_screen(&in, &theContext, EENearOldScreenDistance);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'F', 'S'), in, theResult);
    return theResult != 0;
}

bool EEFOFindNewTargetNearOldTarget::Virtual04(EEComplexUnit* inUnit)
{
    return EENearOldScreen(this, inUnit);
}

bool EEFOFindNewTargetNearOldTarget::Virtual08(EEComplexUnit* inUnit)
{
    return EENearOldScreen(this, inUnit);
}

// Computes where an attacker should stand to fire on a target (healers and melee closer, others at range)
void EEGetFiringPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EECombatAttackPosition(true, inAttacker, inTarget, outLocation);   // ee_combat leaves.rs firing_position
}

// Computes a moving unit's Doppler pitch shift against the camera
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    float theCamera[3] = {::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(), ::gWorld.mTransformation.GetZ()};
    ee_units_uu_doppler_pitch(UUHandle(inUnit), *outVolume, theUnit->mVelocity, (const float(*)[4][4])&theUnit->mPhysicalModel.GetLocation().mMatrix[0][0],
                              &theCamera, outPitch);   // ee_units uu_world.rs doppler_pitch
}

// Returns the angle between north and the direction from one point to the other
float EEGetAngleFromNorth(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    return ee_units_uu_angle(1, 0, 0, (const float(*)[3])&inFrom->mX, (const float(*)[3])&inTo->mX);   // uu_world.rs
}

// Returns the ground heading from one point to another
float EEGetHeadingBetween(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    return ee_units_uu_heading_between((const float(*)[3])&inFrom->mX, (const float(*)[3])&inTo->mX);   // uu_world.rs
}


// (0x642489, EEGetUnitAngle, and the escape point's sort by it, EEUUAngleLess: ee_units uu_world.rs unit_angle)

// Tells whether the target unit is attacking the unit
bool EEIsAttackedBy(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return EECombatWorld(EE_COMBAT_WORLD_ATTACKED_BY, inUnit, inTarget) != 0;   // ee_combat leaves.rs is_attacked_by
}

// Tells whether a priest can convert the target
bool EECanPriestConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    bool theFlag;
    EEUnitClass* theClass = inUnit->mUnitClass;
    bool theResult = false;
    if (inTarget != NULL && theClass->IsPriest())
        theResult = EECanConvert(inUnit, inTarget, theFlag);
    return theResult;
}

// Tells whether a unit can attack a target from where it is; else finds the first tile of a path to a place
// it can (outTile)
bool EEIsAttackReachable(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile)
{
    // ee_combat world.rs attack_reachable over the world host (EECombatGoals.cpp: the path search); CBXR records the
    // outcome
    bool theResult = EECombatWorld(EE_COMBAT_WORLD_ATTACK_REACHABLE, inUnit, inTarget, NULL, &outTile->mX, &outTile->mY) != 0;
    EECombatCaptureXRead(EE_COMBAT_XR_ATTACK_REACHABLE, inUnit, inTarget, theResult, (uint32_t)outTile->mX,
                         (uint32_t)outTile->mY);
    return theResult;
}

// Returns a launch facing for a ship leaving a dock (facing only)
bool EEGetDockShipFacing(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation)
{
    return EEGetDockShipPlacement(inPoint, inClass, inUnit, inFacePoint, true, outTransformation);
}

// Finds a launch position and facing for a ship leaving a dock
bool EEFindDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation)
{
    return EEGetDockShipPlacement(inPoint, inClass, inUnit, inFacePoint, false, outTransformation);
}

// Finds a launch position and facing for a unit leaving a dock or carrier: a free tile around the exit, then
// a turn until the way out is clear of shore, other areas and blocking units
bool EEGetDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, bool inArg5,
                GETransformation* outTransformation)
{
    return ee_units_uu_dock_ship_placement((const float(*)[3])&inPoint->mX, UUHandle(inClass), UUHandle(inUnit), inFacePoint, inArg5,
                                           (float(*)[4][4])&outTransformation->mMatrix[0][0]) != 0;
}

// Finds a free placement for a unit of a class around a tile
bool EEFindPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, unsigned long inX, unsigned long inY, long inSize,
                     long inRings, bool inCorners, bool inFacePoint, bool inArg11, GETransformation* outTransformation,
                     EEComplexUnit* inTarget)
{
    return EEFindPlacement(inPoint, NULL, inClass, inX, inY, inSize, inRings, inCorners, false, inFacePoint, inArg11,
                           outTransformation, inTarget);
}

// Finds a free placement around a tile, ring by ring, reachable from the start's continent, with its facing
bool EEFindPlacement(GE3DPoint* inPoint, EEComplexUnit* inUnit, EEUnitClass* inClass, unsigned long inX,
                     unsigned long inY, long inSize, long inRings, bool inCorners, bool inIncludeCenter, bool inFacePoint,
                     bool inArg11, GETransformation* outTransformation, EEComplexUnit* inTarget)
{
    return ee_units_uu_find_placement((const float(*)[3])&inPoint->mX, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inClass), inX, inY,
                                      inSize, inRings, inCorners, inIncludeCenter, inFacePoint, inArg11,
                                      (float(*)[4][4])&outTransformation->mMatrix[0][0], UUHandle(static_cast<EEUnit*>(inTarget))) != 0;
}

// Turns wall pieces with three or more neighbours into plain junction pieces
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap)
{
    std::vector<uint64_t> theUnits;
    for (size_t i = 0; i < inUnits.size(); i++)
        theUnits.push_back(UUHandle(static_cast<EEUnit*>(inUnits[i])));
    ee_units_uu_make_wall_junctions(theUnits.empty() ? NULL : &theUnits[0], (uint32_t)theUnits.size(), &inTiles, UUHandle(inPlayer),
                                    UUHandle(inMap));
}

// Tells whether every tile of a rectangle can take a unit type for the unit's player
bool EEBlockTakes(EEUnit* inUnit, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    return ee_units_uu_unit_block_takes(UUHandle(inUnit), inUnitType, inX, inY, inWidth, inHeight, inIgnoreOwner) != 0;
}

// Tells whether every tile of a rectangle can take a unit type for a player
// returns the result in al (EECPBuildManager tests it)
bool EEPlayerBlockTakes(EEPlayer* inPlayer, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    return ee_units_uu_block_takes(UUHandle(inPlayer), inUnitType, inX, inY, inWidth, inHeight, inIgnoreOwner) != 0;
}

// (The target search of 0x243d43, EEUUTargetSearch with its filter 0x24ffca and scoring 0x25000c, and the tile
// searches 0x24fcd1 / 0x2503d7, EEUUSearchTiles: ee_combat's search_target, search.rs; its constructor 0x643e0a is
// Search::new there. The world it asks is below.)

// ---- the target searches' world (ee_combat search.rs): handles are the units' pointers (a region's complex units
// for EESearchTarget, the unit list's units for EESearchTargetAlongList and EEGetPursuerOf)

enum EESearchKind { kSearchRegions, kSearchAlongList, kSearchPursuer };

struct EESearchContext {
    EESearchKind mKind;
    EEComplexUnit* mUnit;
    U2DPoint<long> mTile;                   // EESearchTargetAlongList's EEIsAttackReachable tile (kept across calls)
    std::vector<int32_t> mTrace;            // the capture's
};

static EEUnit* EESearchUnit(uint64_t inHandle)
{
    return (EEUnit*)(uintptr_t)inHandle;
}

// a handle's complex unit: the region's own, a list unit's mComplexUnit
static EEComplexUnit* EESearchComplex(EESearchContext* inContext, uint64_t inHandle)
{
    if (inContext->mKind == kSearchRegions)
        return (EEComplexUnit*)(uintptr_t)inHandle;
    return EESearchUnit(inHandle)->mComplexUnit;
}

static void EESearchUnitPosition(void* inContext, uint64_t inUnit, float (*outPosition)[2])
{
    EEComplexUnit* theUnit = EESearchComplex((EESearchContext*)inContext, inUnit);
    (*outPosition)[0] = theUnit->mPhysicalModel.GetLocation().GetX();
    (*outPosition)[1] = theUnit->mPhysicalModel.GetLocation().GetY();
}

static uint64_t EESearchListNext(void* inContext, uint64_t inUnit, int32_t inPrevious)
{
    EEUnit* theUnit = EESearchUnit(inUnit);
    EEUnit* theNext = static_cast<EEUnit*>(inPrevious ? theUnit->mTileNode->mPrev : theUnit->mTileNode->mNext);
    return (uint64_t)(uintptr_t)theNext;
}

static int32_t EESearchDummyHead(void* inContext, uint64_t inUnit)
{
    return RTSDummyHead(EESearchUnit(inUnit)) != NULL;
}

// EEUUTargetSearch::IsCandidate and Score's tests of a region's complex unit
static int32_t EESearchRegionTest(EESearchContext* inContext, uint32_t inTest, EEComplexUnit* inTarget)
{
    EEComplexUnit* theUnit = inContext->mUnit;
    EEPlayer* thePlayer = theUnit->mPlayer;
    EEUnitClass* theClass = inTarget->mUnitClass;
    switch (inTest) {
    case EE_COMBAT_SEARCH_TEST_CANDIDATE:
        return inTarget->mPlayer != thePlayer && thePlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] != 0
            && inTarget->mHitPoints > 0 && (!inTarget->mIsDisabled || inTarget->IsProtected());
    case EE_COMBAT_SEARCH_TEST_NATURE:
        return inTarget->mPlayer->GetPlayerID() == 0;
    case EE_COMBAT_SEARCH_TEST_ATTACKED_BY:
        return EEIsAttackedBy(theUnit, inTarget);
    case EE_COMBAT_SEARCH_TEST_WALL:
        return theClass->mIsWall && !theClass->mIsTower;
    case EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS:
        return EECanTargetClass(theUnit, theClass);
    case EE_COMBAT_SEARCH_TEST_FOUNDATION:
        return inTarget->IsFoundation();
    case EE_COMBAT_SEARCH_TEST_SEEN:
        return EEIsSeenBy(inTarget, theUnit->GetPlayer(), true);
    case EE_COMBAT_SEARCH_TEST_OTHER_CONTINENT:
        return theUnit->IsOnOtherContinentThan(inTarget);
    case EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT:
        return theClass->IsPartLeftOut(inTarget);
    case EE_COMBAT_SEARCH_TEST_PRIEST:
        return theClass->IsPriest();
    case EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT:
        return EECanPriestConvert(theUnit, inTarget);
    case EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS:
        return theUnit->CanAttackClass(theClass);
    case EE_COMBAT_SEARCH_TEST_IGNORED:
        return theUnit->mIgnoredTargets.size() != 0 && theUnit->IsTargetIgnored(inTarget, true);
    case EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT: {
        EEUGRust* theGoal = EEAttackUnitCast(inTarget->GetGoal());
        return theGoal != NULL && theGoal->GetAttackTarget()->mComplexUnit == theUnit;
    }
    }
    return 0;
}

// EESearchTargetAlongList's and EEGetPursuerOf's tests of a list unit (its complex unit may be missing)
static int32_t EESearchListTest(EESearchContext* inContext, uint32_t inTest, EEUnit* inTarget)
{
    EEComplexUnit* theUnit = inContext->mUnit;
    EEPlayer* thePlayer = theUnit->mPlayer;
    EEPlayer* theOwner = inTarget->mPlayer;
    EEComplexUnit* theTarget = inTarget->mComplexUnit;
    switch (inTest) {
    case EE_COMBAT_SEARCH_TEST_NOT_ENEMY:
        return !(theOwner != thePlayer && thePlayer->mEnemies[theOwner->GetPlayerID()] != 0);
    case EE_COMBAT_SEARCH_TEST_NATURE:
        return theOwner->GetPlayerID() == 0;
    case EE_COMBAT_SEARCH_TEST_ATTACKED_BY:
        return EEIsAttackedBy(theUnit, inTarget);
    case EE_COMBAT_SEARCH_TEST_TECH_KIND_6: {
        EETechTreeNode* theNode = theOwner->mTechnologies->Get(inTarget->GetUnitClass()->mTechnology);
        return (theNode != NULL ? theNode->mData->mKind : -1) == 6;
    }
    case EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS:
        return EECanTargetClass(theUnit, inTarget->GetUnitClass());
    case EE_COMBAT_SEARCH_TEST_FOUNDATION:
        return theTarget != NULL && theTarget->IsFoundation();
    case EE_COMBAT_SEARCH_TEST_ALIVE:
        return inTarget->mHitPoints > 0;
    case EE_COMBAT_SEARCH_TEST_SEEN:
        return EEIsSeenBy(theTarget, theUnit->GetPlayer(), true);
    case EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT:
        return inTarget->GetUnitClass()->IsPartLeftOut(theTarget);
    case EE_COMBAT_SEARCH_TEST_PRIEST:
        return theUnit->GetUnitClass()->IsPriest();
    case EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT:
        return theTarget != NULL && EECanPriestConvert(theUnit, theTarget);
    case EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS:
        return theUnit->CanAttackClass(theTarget->GetUnitClass());   // (NULL for a list unit without one, as in the original)
    case EE_COMBAT_SEARCH_TEST_IGNORED:
        return theUnit->mIgnoredTargets.size() != 0 && theTarget != NULL && theUnit->IsTargetIgnored(theTarget, true);
    case EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT: {
        EEUGRust* theGoal;
        return theTarget != NULL && (theGoal = EEAttackUnitCast(theTarget->GetGoal())) != NULL
            && theGoal->GetAttackTarget()->mComplexUnit == theUnit;
    }
    case EE_COMBAT_SEARCH_TEST_PURSUES: {
        if (theTarget == NULL || theTarget->mLauncherClass != NULL || theTarget->mGoalState == NULL)
            return 0;
        EEUnitGoal* theGoal = theTarget->mGoalState;
        EEUGRust* theAttack = EEAttackUnitCast(theGoal);
        EEUGRust* theGather = EEGatherGoalCast(theGoal);
        return (theAttack != NULL && theAttack->GetAttackTarget()->mComplexUnit == theUnit)
            || (theGather != NULL && theGather->GetGatherResource() == theUnit);
    }
    }
    return 0;
}

static int32_t EESearchTest(void* inContext, uint32_t inTest, uint64_t inUnit)
{
    EESearchContext* theContext = (EESearchContext*)inContext;
    if (theContext->mKind == kSearchRegions)
        return EESearchRegionTest(theContext, inTest, (EEComplexUnit*)(uintptr_t)inUnit);
    return EESearchListTest(theContext, inTest, EESearchUnit(inUnit));
}

static void EESearchTargetValues(void* inContext, uint64_t inUnit, EeCombatSearchTarget* outTarget)
{
    EESearchContext* theContext = (EESearchContext*)inContext;
    if (theContext->mKind == kSearchRegions) {
        EEComplexUnit* theTarget = (EEComplexUnit*)(uintptr_t)inUnit;
        outTarget->multiple = theTarget->mUnitClass->mTileSize > 1;
        outTarget->unit_type = (int32_t)theTarget->mUnitClass->mUnitType;
        outTarget->attack = theTarget->mStats->GetAttack();
        outTarget->max_velocity = theTarget->mStats->mMaxVelocity;
    } else {
        EEUnit* theTarget = EESearchUnit(inUnit);
        outTarget->multiple = theTarget->GetUnitClass()->mTileSize > 1;
        outTarget->unit_type = (int32_t)theTarget->GetUnitClass()->mUnitType;
        outTarget->attack = theTarget->mStats->GetAttack();
        outTarget->max_velocity = theTarget->mStats->GetMaxVelocity();
    }
}

static float EESearchHitDistance(void* inContext, uint64_t inUnit, float inX, float inY)
{
    return EEGetHitDistanceSquared(((EESearchContext*)inContext)->mUnit, EESearchUnit(inUnit), inX, inY);
}

static float EESearchAttackFactor(void* inContext, uint64_t inUnit)
{
    EEComplexUnit* theUnit = ((EESearchContext*)inContext)->mUnit;
    EEUnit* theTarget = EESearchUnit(inUnit);
    return gToHitTable.GetAttackFactor(theUnit->GetUnitClass()->mAmmoType, theTarget->GetUnitClass(),
                                       theUnit->mPlayer->GetPlayerID(), theTarget->mPlayer->GetPlayerID());
}

static int32_t EESearchLineBlocked(void* inContext, uint64_t inUnit)
{
    EEUnit* theBlocker;
    long theA = -10000;
    long theB = -10000;
    return EEIsLineToUnitBlocked(((EESearchContext*)inContext)->mUnit, EESearchUnit(inUnit), &theBlocker, &theA, &theB);
}

static int32_t EESearchReachable(void* inContext, uint64_t inUnit)
{
    EESearchContext* theContext = (EESearchContext*)inContext;
    if (theContext->mKind == kSearchRegions) {
        U2DPoint<long> theTile;
        theTile.mX = 0;
        theTile.mY = 0;
        return EEIsAttackReachable(theContext->mUnit, (EEComplexUnit*)(uintptr_t)inUnit, &theTile);
    }
    return EEIsAttackReachable(theContext->mUnit, EESearchUnit(inUnit)->mComplexUnit, &theContext->mTile);
}

static void EESearchIgnore(void* inContext, uint64_t inUnit)
{
    EESearchContext* theContext = (EESearchContext*)inContext;
    theContext->mUnit->IgnoreTarget(EESearchComplex(theContext, inUnit));
}

static void EESearchFootprintTile(void* inContext, uint64_t inUnit, int32_t (*outTile)[2])
{
    long theX;
    long theY;
    ((EESearchContext*)inContext)->mUnit->GetNearestFootprintTile(EESearchUnit(inUnit), &theX, &theY);
    (*outTile)[0] = (int32_t)theX;
    (*outTile)[1] = (int32_t)theY;
}

// the capture's trace: the candidate's id (a pursuer's complex unit's), its outcome (a pursuer's distance), the score
static void EESearchTrace(void* inContext, uint64_t inUnit, int32_t inCode, int32_t inScore)
{
    EESearchContext* theContext = (EESearchContext*)inContext;
    if (!EESimCaptureRecord::Active())
        return;
    EEUnit* theUnit = EESearchUnit(inUnit);
    if (theContext->mKind != kSearchAlongList)
        theUnit = EESearchComplex(theContext, inUnit);
    if (theContext->mKind == kSearchPursuer)
        theUnit = EESearchUnit(inUnit)->mComplexUnit;
    theContext->mTrace.push_back((int32_t)theUnit->GetUnitID());
    theContext->mTrace.push_back(inCode);
    if (theContext->mKind != kSearchPursuer)
        theContext->mTrace.push_back(inScore);
}

static EeCombatSearchHost EESearchHost(EESearchContext* inContext)
{
    EeCombatSearchHost theHost = {inContext, gWorld.mMap->mUnitRegions,
                                  EESearchUnitPosition, EESearchListNext, EESearchDummyHead, EESearchTest,
                                  EESearchTargetValues, EESearchHitDistance, EESearchAttackFactor, EESearchLineBlocked,
                                  EESearchReachable, EESearchIgnore, EESearchFootprintTile, EESearchTrace};
    return theHost;
}

// the search's inputs from the unit (EEUUTargetSearch's constructor's reads; EESearchTargetAlongList's locals)
static EeCombatSearchIn EESearchInput(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    EeCombatSearchIn in = {};
    in.attack = inUnit->mStats->GetAttack();
    in.reach = inUnit->GetLineOfSight();
    in.max_velocity = inUnit->mStats->GetMaxVelocity();
    in.range = inUnit->mStats->GetRange();
    in.scale = inScale;
    in.min_range = inUnit->mUnitClass->mMinRange;
    in.melee = inUnit->mUnitClass->mIsMelee;
    in.shoots_high = inUnit->mUnitClass->mShootsHigh;
    in.over_walls = inUnit->mStats->mCanFireOverWalls;
    in.unit_x = (int32_t)inUnit->mTileX;
    in.unit_y = (int32_t)inUnit->mTileY;
    in.x = (int32_t)inX;
    in.y = (int32_t)inY;
    for (long i = 0; i < 3; i++)
        for (long j = 0; j < 10; j++)
            in.table[i * 10 + j] = (int32_t)EEAIUnitTargetingTable::sTable[i][j];
    return in;
}

// (0x250806, EEUUGetUnitsInArea, the units of an area's unit regions that a screen passes: ee_combat's units_in_area,
// called from EEFindNewTarget below)


// Returns the best target for a unit around a tile, scored by a target search
// The best target for inUnit around (inX, inY) (the tile search 0x24fcd1, or 0x2503d7 from its position for a
// class with +0x318), scored by an EEUUTargetSearch (new: the search, its walks and scores are ee_combat's
// search_target; the tests stay here)
EEUnit* EESearchTarget(EEComplexUnit* inUnit, U2DRectangle<long>* inArea, long inX, long inY, float inScale)
{
    EESearchContext theContext;
    theContext.mKind = kSearchRegions;
    theContext.mUnit = inUnit;
    EeCombatSearchHost theHost = EESearchHost(&theContext);
    EeCombatSearchIn in = EESearchInput(inUnit, inX, inY, inScale);
    in.left = (int32_t)inArea->mLeft;
    in.top = (int32_t)inArea->mTop;
    in.right = (int32_t)inArea->mRight;
    in.bottom = (int32_t)inArea->mBottom;
    in.region_size = ee_combat_regions_size(gWorld.mMap->mUnitRegions);
    in.tick_random = (int32_t)gPhysics->mTickRandom;
    in.position_x = inUnit->mPhysicalModel.GetLocation().GetX();
    in.position_y = inUnit->mPhysicalModel.GetLocation().GetY();
    EeCombatSearchOut theOut = ee_combat_search_target(&in, &theHost);
    EEComplexUnit* theBest = (EEComplexUnit*)(uintptr_t)theOut.best;
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'S', 'T'));
        theRecord.I32((int32_t)inUnit->GetUnitID());
        theRecord.I32((int32_t)inX);
        theRecord.I32((int32_t)inY);
        theRecord.F32(inScale);
        theRecord.I32((int32_t)inArea->mLeft);
        theRecord.I32((int32_t)inArea->mTop);
        theRecord.I32((int32_t)inArea->mRight);
        theRecord.I32((int32_t)inArea->mBottom);
        theRecord.I32(inUnit->mUnitClass->mIsMelee);
        theRecord.I32((int32_t)gPhysics->mTickRandom);
        theRecord.I32((int32_t)(theContext.mTrace.size() / 3));
        for (size_t i = 0; i < theContext.mTrace.size(); i++)
            theRecord.I32(theContext.mTrace[i]);
        theRecord.I32(theBest != NULL ? (int32_t)theBest->GetUnitID() : -1);
        theRecord.I32(theOut.score);
        theRecord.I32(theOut.done);
        theRecord.Commit();
    }
    return theBest;
}

// Returns a target near a unit (along the unit list for anti-air ammunition 0x3c)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mAmmoType != 0x3c)
        return EEFindTargetNear(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), 1.0f);
    return EESearchTargetAlongList(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), 1.0f);
}

// Returns the best target for a unit around a tile along the x-sorted unit list, scored by its targeting table
// The best target for inUnit around (inX, inY) along the x-sorted unit list (both ways), scored by the targeting
// table row of its mode (the unit-list twin of EEUUTargetSearch) (new: the walk and the scores are ee_combat's
// search_along_list; the tests stay here)
EEUnit* EESearchTargetAlongList(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    EESearchContext theContext;
    theContext.mKind = kSearchAlongList;
    theContext.mUnit = inUnit;
    EeCombatSearchHost theHost = EESearchHost(&theContext);
    EeCombatSearchIn in = EESearchInput(inUnit, inX, inY, inScale);
    EeCombatSearchOut theOut = ee_combat_search_along_list(&in, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), &theHost);
    EEUnit* theBest = EESearchUnit(theOut.best);
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'S', 'L'));
        theRecord.I32((int32_t)inUnit->GetUnitID());
        theRecord.I32((int32_t)inX);
        theRecord.I32((int32_t)inY);
        // the scaled range, as the C++ search's record had it (it wrote its inScale after `inScale = theRange * inScale`)
        theRecord.F32(in.range * inScale);
        theRecord.I32((int32_t)(theContext.mTrace.size() / 3));
        for (size_t i = 0; i < theContext.mTrace.size(); i++)
            theRecord.I32(theContext.mTrace[i]);
        theRecord.I32(theBest != NULL ? (int32_t)theBest->GetUnitID() : -1);
        theRecord.I32(theOut.score);
        theRecord.Commit();
    }
    return theBest;
}

// Returns the best target for a unit around a tile when the influence table shows enemies within its reach
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    U2DRectangle<long> theArea;
    EEUnit* theResult = NULL;
    EEInfluenceTable* theTable = ::gWorld.mInfluenceTable;
    float theRange = inUnit->mStats->GetRange() * inScale;
    float theReach = inUnit->GetLineOfSight();
    unsigned long thePlayer = inUnit->mPlayer->GetPlayerID();
    long theRadius;

    if (theRange > theReach)
        theRadius = UMCeil(theRange);
    else
        theRadius = UMCeil(theReach);
    theArea.mTop = inY - theRadius;
    theArea.mBottom = theRadius + inY;
    theArea.mLeft = inX - theRadius;
    theArea.mRight = theRadius + inX;
    if ((inUnit->mStats->GetAttack() != 0.0f || inUnit->mUnitClass->mCategory == kUnitCategoryBuildingPriest)
        && (inUnit->mUnitClass->mAmmoType == 0x3c || theTable->SumOthersValueOnTiles(thePlayer, theArea) > 0))
        theResult = EESearchTarget(inUnit, &theArea, inX, inY, inScale);
    return theResult;
}

// Sums the other players' influence values on a rectangle of tiles
long EEInfluenceTable::SumOthersValueOnTiles(unsigned long inPlayer, const U2DRectangle<long>& inTiles)
{
    return this->SumOthersValue(inPlayer, this->GetRegions(inTiles));
}

// Computes a sound's volume, pan and pitch as heard from the camera (silent off tiles the player sees)
// Volume, pan and pitch of a sound of inVolume at inLocation, as heard from the camera (outPitch 1; volume 0 off a
// tile the current player sees)
void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFalloff)
{
    // the camera's view (the C++'s reads), the arithmetic in ee_units uu_world.rs sound_parameters
    EeUnitsUuSoundView theView = {};
    EETile* theTile = ::gWorld.mMap->GetTile(UMFloor(inLocation.GetX()), UMFloor(inLocation.GetY()));
    theView.seen = theTile != NULL && theTile->IsSeenBy(::gWorld.GetCurrentPlayer());
    if (theView.seen) {
        GE3DPoint theScreen;
        EEGetCameraPoint(inLocation, theScreen, theView.depth);
        float theCamera[6] = {theScreen.mX, theScreen.mY, theScreen.mZ, ::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(),
                              ::gWorld.mTransformation.GetZ()};
        memcpy(theView.screen, theCamera, sizeof(theView.screen));
        memcpy(theView.camera, theCamera + 3, sizeof(theView.camera));
        theView.y_clip = gViewport.GetYClippingPlane();
        theView.zoom = ::gWorld.mCameraControl.GetZoom();
    }
    float theOut[3];
    ee_units_uu_sound_parameters(&theView, inVolume, (const float(*)[3])&inLocation.mX, &theOut);
    outVolume = theOut[0];
    outPan = theOut[1];
    outPitch = theOut[2];
}

// Creates a paid building site (one hit point) of a unit type for a player at a tile
EEUnit* EEMakeBuilding(EEPlayer* inPlayer, long inUnitType, long inX, long inY)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    EEUnit* theUnit = theClass->CreateUnit(inX, inY, inPlayer->GetPlayerID(), true, NULL);
    theUnit->SetHitPoints(1);
    if (theUnit->mTile->GetVisibility(inPlayer->GetPlayerID()) > 0 && theUnit->mTile->GetVisibility(inPlayer->GetPlayerID()) <= 1)
        gDoppelgangerManager.AddForUnseenUnit(static_cast<EEComplexUnit*>(theUnit));
    if (inUnitType == 0x42) {
        EEComplexUnit* theLink = dynamic_cast<EEComplexUnit*>(theUnit);
        while ((theLink = theLink->mNextPart) != NULL)
            theLink->SetHitPoints(1);
    }
    EETechnology* theTechnology = inPlayer->GetTechTree()->Get(theClass->GetTechnology());
    unsigned long theType = theUnit->mUnitClass->mUnitType;
    if (theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm) {
        inPlayer->Pay(theTechnology, 0x20, 1.0f);
        if (theUnit->mTileNode != NULL && theUnit->mComplexUnit->mIsPaidFor != true)
            theUnit->mComplexUnit->mIsPaidFor = true;
    }
    inPlayer->AddPendingUnits(false, true);
    ::gWorld.FlushQueuedPlacements();
    theType = theUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary)
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReplantFarms, {theUnit->mUnitID}), kSlotNone);
    return theUnit;
}

// Replaces a queued technology on every living unit of a player whose class makes it
void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology)
{
    std::vector<EEUnit*> theUnits;
    long theID = inClass->mTechnology;
    EETechnology* theTechnology = inPlayer->mTechnologies->Get(theID);
    unsigned long i;
    if (theTechnology == NULL)
        return;
    for (i = 0; i < theTechnology->GetNumUnitClasses(); i++) {
        long theClass = theTechnology->GetUnitClass(i);
        if (!(EEGetPlayerUnitsOfType(theClass, inPlayer->GetPlayerID(), theUnits) > 0))
            continue;
        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
            if ((*it)->mComplexUnit->mHitPoints > 0)
                (*it)->mComplexUnit->ReplaceResearch(theID, inNewTechnology);
        }
    }
}

// Replaces a unit with a new one of another class (pack/unpack): hit points in proportion, goals, groups
// and effects moved over
// inUnit replaced by a new unit of inClass for inPlayer (EEMPackUnpack): hit points in proportion, the goals, groups
// and effects moved over
EEComplexUnit* EEReplaceUnit(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    EEUUNote theNote(85); theNote.U(inUnit).I((long)inPlayer).C(inClass);   // TEST ONLY (UUTL)
    EEComplexUnit* theNew = EEMakeUnitCopy(inUnit, inPlayer, inClass)->mComplexUnit;
    float theFraction = (float)inUnit->mHitPoints / (float)inUnit->mStats->mMaxHitPoints;
    long theHitPoints;
    if (UMFloatToLong((float)theNew->mStats->mMaxHitPoints * theFraction) > 1)
        theHitPoints = UMFloatToLong((float)theNew->mStats->mMaxHitPoints * theFraction);
    else
        theHitPoints = 1;
    theNew->SetHitPoints(theHitPoints);
    EEComplexUnit* theOld = inUnit->mComplexUnit;
    gPhysics->QueueUnitSwap(theOld, theNew);
    if (theOld->mPreviousPart == NULL)
        inUnit->mUnitClass->GateReplaced(inUnit, theNew);
    if (theOld->GetPlayer() == theNew->GetPlayer()) {
        for (long i = 0; i < 20; i++) {
            EEGroup* theGroup = theNew->GetPlayer()->mGroups[i];
            if (theGroup == NULL)
                continue;
            for (unsigned long j = 0; j < theGroup->GetNumUnits(); j++) {
                if (theGroup->GetUnit(j) == theOld) {
                    theGroup->Add(theNew);
                    break;
                }
            }
        }
    }
    inUnit->Kill(false, true, false);
    theNew->DropUnsuitedGoals(0);
    if (inUnit->mUnitClass->mDamageEffect != NULL && theNew->mUnitClass->mDamageEffect != NULL && theOld != NULL
        && theOld->mIsBuilt && inUnit->mUnitClass->mDamageEffect->mParam2 > theFraction)
        ::gWorld.mEffectManager->CreateUnitEffect(theNew, theNew->mUnitClass->mDamageEffect, NULL, 0, 0, 0);
    if (theNew->mUnitClass->mUnitType == kUnitTypeWallSpace && !theNew->IsFoundation() && theNew->GetPlayer()->mTechnologies->Get(kTechWallSpace)->GetIsAvailable())
        EECreateSpaceWallEnergyWall(theNew);
    else if (theNew->mUnitClass->mCategory == kUnitCategoryGate && theNew->mPreviousPart == NULL && theNew->mNextPart != NULL
        && theNew->GetPlayer()->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && theNew->mUnitClass->mUnitType != kUnitTypeGatePalisades)
        EECreateSpaceGateEnergyWall(theNew);
    theNote.U(theNew).I(theNew->mHitPoints);   // TEST ONLY (UUTL)
    return theNew;
}

// Creates a copy of a unit (or a new class) for a player on its tile, in its state
EEUnit* EEMakeUnitCopy(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    if (inClass == NULL)
        inClass = inUnit->mUnitClass;
    long theY = inUnit->GetTileY();
    long theX = inUnit->GetTileX();
    EEUnit* theUnit = inClass->CreateUnit(theX, theY, inPlayer, inUnit->IsFoundation(), NULL);
    theUnit->CopyFrom(inUnit);
    if (theUnit->mTileNode != NULL && theUnit->mUnitClass->IsLandBuilding() && theUnit->mComplexUnit->mIsBuilt) {
        theUnit->mComplexUnit->SetBuilt(0, 0, 1);
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
    }
    return theUnit;
}


// (0x6450f1, EEClearUnderBuilding: ee_units uu_world.rs clear_under_building)

// Returns the compass sector (0..7) of the direction from one tile to another
long EEGetTileDirection(long inX1, long inY1, long inX2, long inY2)
{
    return ee_units_uu_tile_direction(inX1, inY1, inX2, inY2);
}

// Returns the compass sector of the direction from one unit to another
long EEGetDirectionTo(EEUnit* inFrom, EEUnit* inTo)
{
    return EEGetTileDirection(inFrom->GetTileX(), inFrom->GetTileY(), inTo->GetTileX(), inTo->GetTileY());
}

// Unloads what a gatherer carries into a container: the player's resources (with the variant's rate and a
// nearby-site bonus), the effect and the statistics
void EEDepositLoad(EEComplexUnit* inUnit, EEComplexUnit* inContainer)
{
    ee_units_uu_deposit_load(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)));   // uu_world.rs
}

// Tells whether an enemy the unit may target is within its reach
bool EEIsEnemyWithinRange(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope;
    // ee_combat reads.rs enemy_in_range over the C++ host (EECombatGoals.cpp); CBQR records the outcome
    bool theInRange = EECombatIsEnemyWithinRange(inUnit);
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_ENEMY_IN_RANGE, inUnit, NULL, theInRange, (uint32_t)inUnit->mTargetSearchTimer);
    return theInRange;
}

// Walks a tile path back from its end and drops the middle tile wherever two tiles one step apart
// differ by exactly (+1, +1), cutting diagonal corners
void EEDropDiagonalCorners(std::vector<EETileCoordinate>& ioTiles)
{
    ee_units_uu_drop_tiles(0, &ioTiles);
}

// Collects the player's town centers on the corners and sides of a farm's footprint
bool EEGetFarmDropSites(EEComplexUnit* inUnit, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_farm_drop_sites(UUHandle(static_cast<EEUnit*>(inUnit)), &outUnits) != 0;
}

// Finds the nearest free farm of the unit's player, ring by ring (the ring reached goes back)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioRing)
{
    int32_t theRing = (int32_t)*ioRing;
    EEComplexUnit* theFarm = static_cast<EEComplexUnit*>(UUUnit(ee_units_uu_find_free_farm(UUHandle(static_cast<EEUnit*>(inUnit)), inFreeOnly, &theRing)));
    *ioRing = theRing;
    return theFarm;
}

// Returns the nearest unit that attacks or gathers from the unit
// (new: the walk along the unit list and the choice are ee_combat's pursuer_of; the goal tests stay here)
EEComplexUnit* EEGetPursuerOf(EEComplexUnit* inUnit)
{
    EESearchContext theContext;
    theContext.mKind = kSearchPursuer;
    theContext.mUnit = inUnit;
    EeCombatSearchHost theHost = EESearchHost(&theContext);
    long theX = inUnit->GetTileX();
    long theY = inUnit->GetTileY();
    uint64_t thePursuer = ee_combat_pursuer_of((uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit), (int32_t)theX,
                                               (int32_t)theY, &theHost);
    EEComplexUnit* theResult = thePursuer != 0 ? EESearchUnit(thePursuer)->mComplexUnit : NULL;
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'P', 'U'));
        theRecord.I32((int32_t)inUnit->GetUnitID());
        theRecord.I32((int32_t)theX);
        theRecord.I32((int32_t)theY);
        theRecord.I32((int32_t)(theContext.mTrace.size() / 2));
        for (size_t i = 0; i < theContext.mTrace.size(); i++)
            theRecord.I32(theContext.mTrace[i]);
        theRecord.I32(theResult != NULL ? (int32_t)theResult->GetUnitID() : -1);
        theRecord.Commit();
    }
    return theResult;
}

// a unit's id in the CB** capture records (-1: none)
static int32_t EECombatCaptureID(EEUnit* inUnit)
{
    return inUnit != NULL ? (int32_t)inUnit->GetUnitID() : -1;
}

// The world of the new target search (ee_combat units_in_area and choose_target, walks.rs): the unit regions, the
// screen, the candidates and their tests. Handles are the units' pointers.
struct EENewTargetHost {
    EEFOFindNewTargetScreenBase* mScreen;
    EEComplexUnit* mUnit;
    long mIgnoreArgument;
    std::vector<EEComplexUnit*> mUnits;
};

static EEComplexUnit* EENewTargetUnit(uint64_t inHandle)
{
    return (EEComplexUnit*)(uintptr_t)inHandle;
}

static int32_t EENewTargetScreen(void* inContext, uint64_t inUnit)
{
    return ((EENewTargetHost*)inContext)->mScreen->Virtual08(EENewTargetUnit(inUnit));
}

static int32_t EENewTargetMultiple(void* inContext, uint64_t inUnit)
{
    return EENewTargetUnit(inUnit)->mUnitClass->mTileSize > 1;
}

static int32_t EENewTargetTest(void* inContext, uint32_t inTest, uint64_t inHandle)
{
    EENewTargetHost* theHost = (EENewTargetHost*)inContext;
    EEComplexUnit* inUnit = theHost->mUnit;
    EEComplexUnit* theUnit = EENewTargetUnit(inHandle);
    EEPlayer* thePlayer = inUnit->GetPlayer();
    EEPlayer* theOwner = theUnit->GetPlayer();
    switch (inTest) {
    case EE_COMBAT_TARGET_TEST_TECH_KIND_6: {
        EETechnology* theTechnology = thePlayer->GetTechTree()->Get(theUnit->GetUnitClass()->GetTechnology());
        return theTechnology != NULL && theTechnology->mData->mKind == 6;
    }
    case EE_COMBAT_TARGET_TEST_NOT_ENEMY:
        return theOwner == thePlayer || thePlayer->mEnemies[theOwner->GetPlayerID()] == 0;
    case EE_COMBAT_TARGET_TEST_DISABLED:
        return theUnit->mIsDisabled && !theUnit->IsProtected();
    case EE_COMBAT_TARGET_TEST_NATURE:
        return theOwner->GetPlayerID() == 0;
    case EE_COMBAT_TARGET_TEST_ATTACKED_BY:
        return EEIsAttackedBy(inUnit, theUnit);
    case EE_COMBAT_TARGET_TEST_CAN_TARGET_CLASS:
        return EECanTargetClass(inUnit, theUnit->GetUnitClass());
    case EE_COMBAT_TARGET_TEST_FOUNDATION:
        return theUnit->IsFoundation();
    case EE_COMBAT_TARGET_TEST_SEEN:
        return EEIsSeenBy(theUnit, inUnit->GetPlayer(), true);
    case EE_COMBAT_TARGET_TEST_PART_LEFT_OUT:
        return theUnit->GetUnitClass()->IsPartLeftOut(theUnit);
    case EE_COMBAT_TARGET_TEST_CAN_ATTACK_CLASS:
        return inUnit->CanAttackClass(theUnit->GetUnitClass());
    case EE_COMBAT_TARGET_TEST_IGNORED:
        return inUnit->mIgnoredTargets.size() != 0 && inUnit->IsTargetIgnored(theUnit, theHost->mIgnoreArgument != 0);
    }
    return 0;
}

static void EENewTargetPush(void* inContext, uint64_t inUnit)
{
    ((EENewTargetHost*)inContext)->mUnits.push_back(EENewTargetUnit(inUnit));
}

// Gives a unit's goal the first enemy near a tile that passes a screen; false when none
// (new: the region walk under it and the choice are ee_combat's; the tests and the retarget stay here)
bool EEFindNewTarget(EEFOFindNewTargetScreenBase* inScreen, EEUnitGoal* inGoal, EEComplexUnit* inUnit,
                EETileCoordinate* inTile, long inRadius, bool inArg6, long inArg7)
{
    EENewTargetHost theContext = {inScreen, inUnit, inArg7, std::vector<EEComplexUnit*>()};
    // (the units' tiles and ids: ee_combat's reads of the unit store)
    EeCombatTargetHost theHost = {&theContext, gWorld.mMap->mUnitRegions, EENewTargetScreen, EENewTargetMultiple,
                                  EENewTargetTest, EENewTargetPush};
    EeCombatFindTargetIn in = {};
    in.left = (int32_t)(inTile->GetX() - inRadius);
    in.top = (int32_t)(inTile->GetY() - inRadius);
    in.right = (int32_t)(inTile->GetX() + inRadius);
    in.bottom = (int32_t)(inTile->GetY() + inRadius);
    in.skip_kind = inArg6;
    ee_combat_units_in_area(&in, &theHost);
    std::vector<uint64_t> theHandles;
    for (size_t i = 0; i < theContext.mUnits.size(); i++)
        theHandles.push_back((uint64_t)(uintptr_t)theContext.mUnits[i]);
    int32_t theChoice = ee_combat_choose_target(theHandles.empty() ? NULL : &theHandles[0], (uint32_t)theHandles.size(),
                                                inArg6, &theHost);
    bool theFound = theChoice >= 0;
    if (theFound)
        EERetargetAttack(inUnit, inGoal, theContext.mUnits[theChoice]);
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'F', 'T'));
        theRecord.I32(EECombatCaptureID(inUnit));
        theRecord.I32((int32_t)inTile->GetX());
        theRecord.I32((int32_t)inTile->GetY());
        theRecord.I32((int32_t)inRadius);
        theRecord.I32(inArg6);
        theRecord.I32((int32_t)inArg7);
        theRecord.I32((int32_t)theContext.mUnits.size());
        for (size_t j = 0; j < theContext.mUnits.size(); j++)
            theRecord.I32(EECombatCaptureID(theContext.mUnits[j]));
        theRecord.I32(theFound);
        theRecord.I32(theFound ? EECombatCaptureID(theContext.mUnits[theChoice]) : -1);
        theRecord.Commit();
    }
    return theFound;
}

// Finds a place for a computer player's unit to flee to, away from its attackers
bool EEGetEscapePoint(EEComplexUnit* inUnit, GE2DPoint& outLocation)
{
    return ee_units_uu_escape_point(UUHandle(static_cast<EEUnit*>(inUnit)), (float(*)[2])&outLocation.mX) != 0;   // uu_world.rs
}

// Runs a functor over the units of every player but nature
void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor)
{
    for (unsigned long i = 1; i < ::gWorld.mNumPlayers; i++)
        EEForEachUnitOf(::gWorld.mPlayers[i], inFunctor, false);
}

// Runs a functor over a player's units (and its pending units when asked)
void EEForEachUnitOf(EEPlayer* inPlayer, EEUUUnitFunctor* inFunctor, bool inCalamities)
{
    if (inPlayer == NULL)
        return;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        inFunctor->ApplyToUnit(theUnit);
        if (theUnit->mComplexUnit != NULL)
            inFunctor->Apply(theUnit->mComplexUnit);
    }
    if (inCalamities != true)
        return;
    for (unsigned long i = 0; i < inPlayer->GetNumPendingUnits(); i++) {
        inFunctor->ApplyToUnit(inPlayer->GetPendingUnit(i));
        EEComplexUnit* theUnit = static_cast<EEUnit*>(inPlayer->GetPendingUnit(i))->mComplexUnit;
        if (theUnit != NULL)
            inFunctor->Apply(theUnit);
    }
}

// Returns the squared ground distance between two units
float EEGetDistanceSquared(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    return ee_units_uu_distance_squared(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inOther)));
}

// Returns the world position of a unit's current attach point; false (half a tile up) without one
// The world position of inUnit's current tag point (its model part's list, the unit's frame), scaled by its size;
// false (and the point half a tile up) when the part has no tag points (new: ee_combat's attach_point)
bool EEApproachAttachPoint(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[inUnit->mAnimation];
    EeCombatLaunchIn in = {};
    std::vector<float> theChain;
    in.kind = EE_COMBAT_LAUNCH_KIND_ATTACH;
    in.point_count = theList != NULL ? (int32_t)theList->GetNumTagPoints() : 0;
    in.weapon_index = (int32_t)inUnit->mWeaponTagPoint;
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.scale = inUnit->GetStats()->GetScale();
    if (in.point_count != 0)
        EECombatTagPointChain(theList->GetTagPoint(inUnit->mWeaponTagPoint), inUnit->GetModel()->mSound1Frames[0], in.point,
                              theChain, in.chain);
    EeCombatLaunchOut theOut = ee_combat_launch(&in, theChain.empty() ? NULL : (const float(*)[16])&theChain[0]);
    EECombatChainCapture(EE_COMBAT_TAG('C', 'B', 'A', 'P'), in, theChain, theOut);
    outPoint.SetX(theOut.point[0]);
    outPoint.SetY(theOut.point[1]);
    outPoint.SetZ(theOut.point[2]);
    return theOut.result != 0;
}

// Collects the tiles around a rectangle (sides only, no corners)
void EEGetSquareRing(long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_square_ring(inX, inY, inWidth, inHeight, 0, &outTiles);
}

// (the EETileCoordinate twin of 0x64697b is gone: one tile type)

// Collects the tiles around a square (sides only)
void EEGetSideTiles(long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_square_ring(inX, inY, inSize, inSize, 1, &outTiles);
}

// Collects the tiles around a rectangle, with corners or only the corners not closed by two wall pieces
void EEGetBlockRing(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<EETileCoordinate>& outTiles,
                bool inAppend)
{
    ee_units_uu_block_ring(inCorners, inX, inY, inWidth, inHeight, &outTiles, inAppend, 0);
}

// Collects the tiles around a square, corners as asked
void EEGetSquareBlockRing(long inCorners, long inX, long inY, long inSize, std::vector<EETileCoordinate>& outTiles, bool inAppend)
{
    ee_units_uu_block_ring(inCorners, inX, inY, inSize, inSize, &outTiles, inAppend, 1);
}

// Tells whether a unit can heal a target (wounded enough, not an enemy, in its heal set)
// (new: the decision is ee_combat's can_heal over a record of pure reads; the unit set is looked up only for a set
// in the manager's range, which the original indexed unchecked)
bool EECanHeal(EEUnit* inUnit, EEComplexUnit* inTarget)
{
    unsigned long theSet = inUnit->mStats->mWonderUnitSet;
    EeCombatCanHealIn in = {};
    in.has_set = theSet > 0;
    in.repairable = inTarget->IsRepairable();
    in.hit_points = (int32_t)inTarget->mHitPoints;
    in.maximum = (int32_t)inTarget->mStats->mMaxHitPoints;
    in.threshold = inUnit->mUnitClass->GetHealThreshold();
    in.same_unit = inTarget == inUnit;
    in.same_player = inUnit->mPlayer == inTarget->mPlayer;
    in.allied = inTarget->mPlayer->mEnemies[inUnit->mPlayer->GetPlayerID()] == 0;
    in.in_set = theSet != 0 && theSet < gUnitSetManager.GetSize()
                && gUnitSetManager.Contains(inTarget->mUnitClass->mUnitType, theSet);
    int32_t theResult = ee_combat_can_heal(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'H'), in, theResult);
    return theResult != 0;
}

// Returns a unit around the given one whose gather goal works on it (searched in a random rotation)
EEComplexUnit* EEGetWaitingGatherer(EEUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    EEComplexUnit* theResult = NULL;
    if (!UShutdownInProgress()) {
        EEGetResourceSpots(inUnit, theTiles);
        EEGetUnitsOn(theTiles, theUnits);
        float theRandom = gPhysics->mRandom.Generate();
        PermuteValues(theUnits, UMFloatToLong(theRandom));
        for (unsigned long i = 0; i < theUnits.size() && theResult == NULL; i++) {
            EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
            if (theUnit == NULL)
                continue;
            long theCommand;
            theCommand = theUnit->mCommand != NULL ? theUnit->mCommand->mType : 0;
            if (theCommand != kActionWaitForOpenPosition || theUnit->mGoalState == NULL || theUnit->mGoalState->mKind != kGoalGather)
                continue;
            EEUGRust* theGoal = EEGatherGoalCast(theUnit->mGoalState);
            if (theGoal != NULL && theGoal->GetGatherResource() == inUnit)
                theResult = theUnit;
        }
    }
    return theResult;
}

// Returns the tile of a unit's footprint nearest to a tile's centre and that squared distance
float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY)
{
    float thePoint[3] = {(float)inX + 0.5f, (float)inY + 0.5f, 0.0f};
    int32_t theTile[2];
    float theDistance = ee_units_uu_nearest_footprint_tile(UUHandle(static_cast<EEUnit*>(inUnit)), &thePoint, &theTile);
    *outX = theTile[0];
    *outY = theTile[1];
    return theDistance;
}

// Tells whether both tiles hold a wall piece
bool EEIsCornerBlocked(unsigned long inX1, unsigned long inY1, unsigned long inX2, unsigned long inY2)
{
    return ee_units_uu_is_corner_blocked((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2) != 0;
}

// Returns the nearest piece of a target's linked group that the unit may attack
EEComplexUnit* EEGetAttackPart(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_uu_attack_part(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inTarget)))));
}

// Collects the four corner tiles of a rectangle (only the open ones unless corners are asked)
void EEGetRingCorners(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_ring_corners(inCorners, inX, inY, inWidth, inHeight, 0, &outTiles);
}


// (0x647125, EEGetEnemiesAround: ee_units uu_world.rs enemies_around)

// Collects the corner tiles of a square
void EEGetCornerTiles(long inCorners, long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_ring_corners(inCorners, inX, inY, inSize, inSize, 1, &outTiles);
}

// Tells whether a unit can attack a target from where it stands (in range, not blocked)
bool EEIsInRangeToHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, inUnit, inTarget) != 0;   // ee_combat leaves.rs in_range_to_hit
}

// (0x647316, EECanReach: ee_combat's can_reach, called by leaves.rs in_contact_range)

// Tells whether a unit is close enough to a target to build or repair it
bool EEIsInWorkReach(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    EEUnitClass* theClass = inTarget->mUnitClass;
    EeCombatReachIn theIn = {0.0f, inUnit->mUnitClass->mInnerCollisionSphere, theClass->mInnerCollisionSphere, 0.0f,
                             theClass->mIsWall, theClass->mUnitType == kUnitTypeRoboticFarm};
    theIn.distance_squared = inUnit->DistanceSquaredTo(inTarget);
    return EECombatWorkReach(theIn) != 0;
}

// Tells whether a unit at a squared distance reaches a target (gather and arrival test)
bool EEIsInGatherReach(float inDistanceSquared, EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EeCombatReachIn theIn = {inDistanceSquared, inUnit->mUnitClass->mInnerCollisionSphere,
                             inTarget->mUnitClass->mInnerCollisionSphere, 0.0f, inTarget->mUnitClass->mIsTree,
                             inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen};
    return EECombatGatherReach(theIn) != 0;
}

// Tells whether a gatherer has arrived at its target (2D distance to the target's position)
bool EEIsWithinReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return ee_units_uu_within_reach(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inTarget), NULL) != 0;   // uu_world.rs
}

// Tells whether a unit can hit a target from its attach point (a clear line to the drop location)
bool EECanBallisticHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    GE3DPoint theDrop;
    GE3DPoint theTag;
    EEUnit* theTarget;
    long theB;
    long theC;
    float theAngle;
    bool theResult;

    EEApproachAttachPoint(inUnit, theTag);
    theResult = !inUnit->IsOnOtherContinentThan(static_cast<EEComplexUnit*>(inTarget));
    if (theResult) {
        EEFindDropLocation(inUnit, &theTarget, &theDrop, inTarget);
        theResult = EEGetBallisticPitch(inUnit, inUnit->GetStats(), &theTag, &theDrop, &theAngle);
        if (theResult && !inUnit->mUnitClass->mShootsHigh && !inUnit->GetStats()->mCanFireOverWalls)
            theResult = !EEIsLineToUnitBlocked(inUnit, inTarget, &theTarget, &theB, &theC);
    }
    return theResult;
}

// Tells whether a unit touches a target (collision spheres; walls count as radius 1)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared)
{
    long theBits = 0;   // ee_combat leaves.rs in_contact_range: the distance's bits as the tile's x
    memcpy(&theBits, &inDistanceSquared, 4);
    return EECombatWorld(EE_COMBAT_WORLD_CONTACT_RANGE, inUnit, inTarget, NULL, &theBits, &theBits) != 0;
}


// Tells whether a unit may target a class (walls only by siege units or computer players)
bool EECanTargetClass(EEComplexUnit* inUnit, EEUnitClass* inClass)
{
    return ee_units_uu_targetable(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inClass), 1) != 0;
}

// Tells whether a class can be targeted at all (by an attacker's ammunition when given)
bool EEUUGetIsUnitClassTargetable(EEUnit* inAttacker, EEUnitClass* inClass)
{
    return ee_units_uu_targetable(UUHandle(inAttacker), UUHandle(inClass), 0) != 0;
}

// Tells whether a unit is a cloaked enemy of a player
bool EEIsCloakedEnemyOf(EEUnit* inUnit, EEPlayer* inPlayer)
{
    return ee_units_uu_cloaked_enemy(UUHandle(inUnit), UUHandle(inPlayer)) != 0;
}

// Tells whether a player sees any tile of a unit's footprint
bool EEIsFootprintVisibleTo(EEUnit* inUnit, EEPlayer* inPlayer)
{
    return ee_units_uu_footprint_visible(UUHandle(inUnit), UUHandle(inPlayer)) != 0;   // uu_world.rs
}

// Tells whether a player sees a unit (explored may be enough; cloaked enemies never)
bool EEIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
{
    return EECombatIsSeenBy(inUnit, inPlayer, inExplored);   // ee_combat leaves.rs is_seen_by
}

// Tells whether the owner of another unit sees the unit (explored is enough)
bool EEIsSeenByOwnerOf(EEComplexUnit* inUnit, EEUnit* inOther)
{
    return EEIsSeenBy(inUnit, inOther->GetPlayer(), true);
}

// Finds a place for a unit next to a target, preferring its own path area and continent
// A place for inUnit next to inTarget, preferring tiles in inUnit's own path area and continent (inferred)
bool EEFindResourceSpot(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation)
{
    return ee_units_uu_spot(1, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inTarget)), (float(*)[3])&outLocation.mX) != 0;
}

// Finds a place for a unit to attack a target from (around a wall it stands in, or next to it)
bool EEGetWallSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    return ee_units_uu_spot(3, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inTarget), (float(*)[3])&outLocation.mX) != 0;
}

// Returns a unit's radius seen from another: full in front or behind, down to a tenth from the side
float EEGetUnitRadius(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    return ee_units_uu_angle(0, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inOther)), NULL, NULL);
}

// Returns the tile a distance away in a compass direction (tile form)
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance)
{
    int32_t theTile[2];
    ee_units_uu_tile_in_direction(inTile.mX, inTile.mY, inDirection, inDistance, &theTile);
    outTile.mX = theTile[0];
    outTile.mY = theTile[1];
}

// (the U2DPoint<long> twin of 0x648ecc is gone: one tile type)

// Moves a point a number of tiles in a compass direction, keeping its offset within the tile
void EEMoveInDirection(const GE2DPoint& inFrom, GE2DPoint& outTo, long inDirection, long inDistance)
{
    float thePoint[2];
    ee_units_uu_move_in_direction(inFrom.mX, inFrom.mY, inDirection, inDistance, &thePoint);
    outTo.SetX(thePoint[0]);
    outTo.SetY(thePoint[1]);
}

// Turns a compass direction by a random amount within a spread
long EETurnAway(long inDirection, long inSpread)
{
    return ee_units_uu_turn_away(inDirection, inSpread);
}

// Collects the tiles a unit covers, widened by a margin (a line along a ship's heading, else a disc)
// The tiles inUnit covers, widened by inMargin (a line along a ship's heading; a disc otherwise) (inferred)
void EEGetUnitTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles, float inMargin)
{
    ee_units_uu_unit_tiles_margin(UUHandle(inUnit), inMargin, &outTiles);   // ee_units uu_world.rs unit_tiles
}

// Returns a random number in [min, max]
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom)
{
    long theEnd = inMax + 1;
    long theResult = UMFloatToLong(inRandom.Generate() * (theEnd - inMin) + inMin);
    if (theResult == theEnd)
        theResult = inMax;
    return theResult;
}

// Returns the id of a player's leading unit, -1 when none
unsigned long EEGetLeadUnitID(EEPlayer* inPlayer)
{
    return (unsigned long)ee_units_uu_lead_unit_id(UUHandle(inPlayer));
}

// Collects the ring of tiles at a distance around a tile
void EEGetSquareRingTiles(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_square_ring_tiles(inX, inY, inRadius, &outTiles);
}

// Finds where a dropped unit lands on a target: the model's drop point (lower when damaged) or its tile
void EEFindDropLocation(EEComplexUnit* inUnit, EEUnit** outTarget, GE3DPoint* outLocation, EEUnit* inTarget)
{
    *outTarget = EECombatFindDropLocation(inUnit, inTarget, *outLocation);   // ee_combat leaves.rs find_drop_location
}

// Returns the squared ground distance from a unit (or a point) to where it would hit a target
float EEGetHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
{
    return EECombatHitDistanceSquared(inUnit, inTarget, inX, inY);   // ee_combat leaves.rs hit_distance_squared
}

// Returns a point 90% of a unit's range from a target towards the unit
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outPoint)
{
    ee_units_uu_aim_point(UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])&inTarget->mX, (float(*)[3])&outPoint->mX);
}

// Returns where a unit casts its calamity from: 90% of the calamity's range from the target
void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    EEUUNote theNote(64); theNote.U(inUnit).P(outPoint);   // TEST ONLY (UUTL)
    GEVector theDirection;
    GEVector theOffset;
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    EEUGRust* theGoal = EECastCalamityGoalCast(inUnit->GetGoal());
    theGoal->GetGoalLocation(theTarget);
    long theType = theGoal->GetCalamityType();
    EETechTree* theTree = inUnit->mPlayer->mTechnologies;
    EETechnology* theTechnology = theTree->Get(theType);
    if (theTechnology == NULL) {
        theNote.P(outPoint);   // TEST ONLY (UUTL)
        return;
    }
    EECalamityEnum theEnum = (EECalamityEnum)theTechnology->mData->mCalamity;
    EECalamityClass* theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theEnum);
    inUnit->GetPosition(thePosition);
    theDirection.SetX(thePosition.GetX() - theTarget.GetX());
    theDirection.SetY(thePosition.GetY() - theTarget.GetY());
    theDirection.SetZ(thePosition.GetZ() - theTarget.GetZ());
    theDirection.Normalize();
    theDirection.Scale((float)(unsigned long)theClass->mRange * 0.9f, theOffset);
    outPoint.SetX(theTarget.GetX() + theOffset.GetX());
    outPoint.SetY(theTarget.GetY() + theOffset.GetY());
    outPoint.SetZ(theTarget.GetZ() + theOffset.GetZ());
    theNote.P(outPoint);   // TEST ONLY (UUTL)
}

// Collects the tiles of a line, walking in from both ends (optionally the end tiles and 4-connected)
// The tiles of the line (inX1, inY1)-(inX2, inY2): a symmetric double-step rasteriser that walks in from both ends
// (inEnds: the two end tiles too; !inThin: the extra tiles that make the line 4-connected), in order from (inX1, inY1)
// (new: the rasteriser is ee_combat's tile_line)
void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<U2DPoint<long> >* outTiles, bool inEnds,
                bool inThin)
{
    long theDX = inX1 - inX2;
    if (theDX < 0)
        theDX = -theDX;
    long theDY = inY1 - inY2;
    if (theDY < 0)
        theDY = -theDY;
    // a line has at most two tiles a step, and its two ends
    std::vector<int32_t> thePairs((size_t)(theDX + theDY) * 4 + 8);
    uint32_t theCapacity = (uint32_t)(thePairs.size() / 2);
    uint32_t theCount = ee_combat_tile_line((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2, inEnds, inThin,
                                            (int32_t(*)[2])&thePairs[0], theCapacity);
    if (theCount > theCapacity) {
        thePairs.resize((size_t)theCount * 2);
        ee_combat_tile_line((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2, inEnds, inThin,
                            (int32_t(*)[2])&thePairs[0], theCount);
    }
    outTiles->clear();
    outTiles->reserve(theCount);
    for (uint32_t i = 0; i < theCount; i++)
        outTiles->push_back(U2DPoint<long>(thePairs[i * 2], thePairs[i * 2 + 1]));
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'T', 'L'));
        int32_t theIn[6] = {(int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2, inEnds, inThin};
        theRecord.Bytes(theIn, sizeof(theIn));
        theRecord.I32((int32_t)theCount);
        theRecord.Bytes(&thePairs[0], theCount * 8);
        theRecord.Commit();
    }
}

// Collects the tiles whose nearest point lies within a distance of a point
void EEGetTilesNearPoint(const GE3DPoint& inPoint, float inDistance, std::vector<EETileCoordinate>& outTiles)
{
    ee_units_uu_get_tiles_near_point((const float(*)[3])&inPoint.mX, inDistance, &outTiles);
}

// Collects the square of tiles within a radius of a tile
void EEGetTilesInRadius(long inX, long inY, unsigned long inRadius, bool inInclusive,
                        std::vector<EETileCoordinate>& outTiles)
{
    ee_units_uu_tiles_in_radius(inX, inY, (uint32_t)inRadius, inInclusive, &outTiles);
}

// (the U2DPoint<long> twin of 0x64b556 is gone: one tile type)

// Collects the tiles of a unit's footprint (a gate's pieces; a long wonder's full length)
void EEGetFootprintTiles(EEUnit* inUnit, std::vector<EETileCoordinate>& outTiles)
{
    ee_units_uu_unit_tiles(0, UUHandle(inUnit), &outTiles);
}

// Collects the tiles of a wall line between two tiles
void EEGetWallLine(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, std::vector<EETileCoordinate>& outTiles)
{
    ee_units_uu_wall_line(inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, &outTiles);
}

// Returns the path distance from a unit to a tile (10000 when unreachable) and the path's first tile
float EEGetPathLengthTo(EEComplexUnit* inUnit, EETileCoordinate& ioTile, unsigned long inLimit, bool inWalls)
{
    int32_t theTile[2] = {(int32_t)ioTile.mX, (int32_t)ioTile.mY};
    float theResult = ee_units_uu_path_length_to(UUHandle(static_cast<EEUnit*>(inUnit)), &theTile, (uint32_t)inLimit, inWalls);
    ioTile.mX = theTile[0];
    ioTile.mY = theTile[1];
    return theResult;   // ee_units uu_world.rs path_length_to
}

// Tells whether a tile lies on the diamond map's edge
bool RTSMap::IsEdgeTile(const EETileLocation& inTile)
{
    long theY = inTile.mY;
    long theX = inTile.mX;
    return theX + theY == this->mL195608 || theY - theX == this->mL195610 || theX + theY == this->mL19560C || theX - theY == this->mL195610;
}

// Collects the units within a radius of a point; their number
long EEGetUnitsNear(const GE3DPoint& inCenter, float inRadius, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_near((const float(*)[3])&inCenter.mX, inRadius, &outUnits);   // ee_units uu_reads.rs
}


// Collects the units on the tiles of a screen rectangle; their number
long EEGetUnitsInRectangle(const GERectangle& inRectangle, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_in_rectangle(inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom(),
                                          &outUnits);   // uu_ffi.rs
}

// Collects the units within a radius of a point (the square of tiles around it); their number
long EEGetUnitsInRadius(GE2DPoint inCenter, float inRadius, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_in_radius(inCenter.mX, inCenter.mY, inRadius, &outUnits);   // ee_units uu_reads.rs
}

// Collects a player's units of a type; their number
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayer, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_player_units_of_type(inUnitType, inPlayer, &outUnits);
}

// Collects the units on a tile; their number
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_on_tile(inX, inY, &outUnits);
}

// Collects the units on a list of tiles
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_on(0, &inTiles, &outUnits);
}

// Collects the units of a type on a list of tiles; their number
long EEGetWallUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_of_type_on_tiles(&inTiles, inUnitType, &outUnits);
}

// Collects the free tiles around a unit (its ring, corners included)
void EEGetResourceSpots(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles)
{
    ee_units_uu_unit_tiles(1, UUHandle(inUnit), &outTiles);
}


// Enters a fortress into the unit regions under its footprint, clearing ambient units outside a load
void EEAddBuildingRegion(EEUnit* inUnit)
{
    ee_units_uu_footprint(2, UUHandle(inUnit));   // ee_units uu_world.rs building_region
}

// Adds a unit to every tile of its footprint
void EEAddToFootprintTiles(EEUnit* inUnit)
{
    ee_units_uu_footprint(0, UUHandle(inUnit));   // ee_units uu_world.rs footprint_link
}
// Returns the tile two tiles away in a direction (1..8)
void EEGetSlotTile(const EETileCoordinate& inTile, long inDirection, EETileCoordinate& outTile)
{
    int32_t theTile[2];
    ee_units_uu_slot_tile(inTile.mX, inTile.mY, inDirection, &theTile);
    outTile.mX = theTile[0];
    outTile.mY = theTile[1];
}

// Returns a resource's column (iron 3, gold 2, stone 1, else as listed), -1 for none
long EEGetResourceIndex(EEUnit* inResource)
{
    return ee_units_uu_resource_index(UUHandle(inResource));
}

// Picks the wall piece (straight, corner or slope) for every tile of a wall line from its neighbours and the
// terrain heights
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap)
{
    std::vector<int32_t> theCorners(inTiles.size() + 1);
    ee_units_uu_pick_wall_pieces(UUHandle(inMap), &inTiles, &theCorners[0]);
    outCorners.assign(theCorners.begin(), theCorners.end() - 1);
}

// Takes a fortress out of the unit regions under its footprint
void EERemoveBuildingRegion(EEUnit* inUnit)
{
    ee_units_uu_footprint(3, UUHandle(inUnit));   // ee_units uu_world.rs building_region
}

// Removes a unit from every tile of its footprint
void EERemoveFromFootprintTiles(EEUnit* inUnit)
{
    ee_units_uu_footprint(1, UUHandle(inUnit));   // ee_units uu_world.rs footprint_link
}

// Drops repeated tiles from a list
void EEDropRepeatedTiles(std::vector<EETileCoordinate>& ioTiles)
{
    ee_units_uu_drop_tiles(1, &ioTiles);
}

// Turns an angle by a random share of a full turn, kept in (-pi, pi]
float EEJitterHeading(float inAngle, float inMin, float inMax)
{
    return ee_units_uu_jitter_heading(inAngle, inMin, inMax);
}


// Tells whether a wall piece on a tile rises above a height
bool EEIsTileTopAbove(unsigned long inX, unsigned long inY, float inHeight, EEUnit** outArg4, bool inBare)
{
    uint64_t theWall = 0;
    bool theResult = ee_units_uu_tile_top_above((int32_t)inX, (int32_t)inY, inHeight, inBare, &theWall) != 0;   // uu_world.rs
    *outArg4 = UUUnit(theWall);
    return theResult;
}

// Tells whether a wall blocks a tile line (diagonal steps between two pieces count); reports the last free tile
// Whether a wall blocks the line from (inX1, inY1) to (inX2, inY2) (diagonal steps between two pieces count);
// outX/outY the last free tile
bool EEIsLineBlocked(long inX1, long inY1, long inX2, long inY2, EEUnit** outArg5, long* outX, long* outY)
{
    uint64_t theWall = 0;
    uint8_t theWritten = 0;
    int32_t theLast[2];
    bool theResult = ee_units_uu_line_blocked(inX1, inY1, inX2, inY2, &theWall, &theWritten, &theLast) != 0;   // uu_world.rs
    if (theWritten)
        *outArg5 = UUUnit(theWall);
    *outX = theLast[0];
    *outY = theLast[1];
    return theResult;
}
// Tells whether something on the tile line from a unit to a target blocks its shot
// Whether something on the tile line from inUnit to inTarget blocks inUnit's shot (the shot's height runs from
// inUnit's weapon point to the top of inTarget); outX/outY: the last tile reached (inferred)
// With EE_SIM_CAPTURE, a CBLB record: the call's units and tiles, the result, the last tile, the blocker (its golden
// lines came from the C++ walk)
static bool EEIsLineToUnitBlockedWalk(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5);

bool EEIsLineToUnitBlocked(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5)
{
    EEUnit* const kUnwritten = (EEUnit*)(uintptr_t)1;   // EEIsTileTopAbove writes the blocker, never reads it
    EEUnit* theBlocker = kUnwritten;
    bool theResult = EEIsLineToUnitBlockedWalk(inUnit, inTarget, &theBlocker, outArg4, outArg5);
    if (theBlocker != kUnwritten)
        *outArg3 = theBlocker;
    if (EESimCaptureRecord::Active()) {
        int32_t theIn[6] = {EECombatCaptureID(inUnit), EECombatCaptureID(inTarget), (int32_t)inUnit->GetTileX(),
                            (int32_t)inUnit->GetTileY(), (int32_t)inTarget->GetTileX(), (int32_t)inTarget->GetTileY()};
        int32_t theOut[5] = {theResult, (int32_t)*outArg4, (int32_t)*outArg5, theBlocker != kUnwritten,
                             theBlocker != kUnwritten ? EECombatCaptureID(theBlocker) : -1};
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'L', 'B'), theIn, theOut);
    }
    return theResult;
}

// The world of the line of fire (ee_combat line_blocked, walks.rs): the shooter, the target, the caller's blocker
struct EELineOfFireHost {
    EEComplexUnit* mUnit;
    EEUnit* mTarget;
    EEUnit** mBlocker;
    std::vector<int32_t> mPairs;
};

// the shot's start: the shooter's height, or (not bare) its weapon tag point's. The model's data at its frame is
// gathered here (the tag point, the model chain's transforms, the turret's yaw, the box); the arithmetic is ee_combat's
// weapon_height (geometry.rs)
static float EELineOfFireStartHeight(void* inContext, int32_t inBare)
{
    EEComplexUnit* theUnit = ((EELineOfFireHost*)inContext)->mUnit->GetComplexUnit();
    EeCombatWeaponIn in = {};
    std::vector<float> theChain;
    memcpy(in.location, theUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    if (inBare) {
        in.kind = EE_COMBAT_WEAPON_KIND_BARE;
    } else {
        EEModel* theModel = ((EELineOfFireHost*)inContext)->mUnit->GetModel();
        EETagPointList* theList = theModel->mTagPoints[theUnit->mAnimation];
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        unsigned long theIndex = theUnit->mWeaponTagPoint;
        in.list_present = theList != NULL;
        in.scale = theUnit->mPhysicalModel.GetScale();
        EETagPoint* thePoint = NULL;
        if (theModel->GetNumParts() > 0) {
            in.kind = EE_COMBAT_WEAPON_KIND_TURRET;
            unsigned long thePart;
            if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, thePart) == 0) {
                in.turret = 1;
                in.yaw = theUnit->mTurretAngles[thePart];
            }
            thePoint = *(theList->mPoints.begin() + theIndex);
        } else if (theList != NULL && theList->mPoints.size() > theIndex) {
            in.kind = EE_COMBAT_WEAPON_KIND_POINT;
            thePoint = theList->GetTagPoint(theIndex);
        } else {
            in.kind = EE_COMBAT_WEAPON_KIND_BOX;
            const GE3DPoint& theTop = theModel->mModels[theUnit->mAnimation]->GetBoundingBox(theFrame).GetTopRightBack();
            in.top[0] = theTop.GetX();
            in.top[1] = theTop.GetY();
            in.top[2] = theTop.GetZ();
        }
        if (thePoint != NULL) {
            const GE3DPoint& theLocal = thePoint->mModel->GetTagPoint(thePoint->mIndex, theFrame);
            in.point[0] = theLocal.GetX();
            in.point[1] = theLocal.GetY();
            in.point[2] = theLocal.GetZ();
            for (GEModel* theLink = thePoint->mModel; theLink != NULL; theLink = theLink->GetParentModel()) {
                const GETransformation& theStep = theLink->GetInternalTransform(theFrame);
                theChain.insert(theChain.end(), &theStep.mMatrix[0][0], &theStep.mMatrix[0][0] + 16);
                in.chain++;
            }
        }
    }
    float theHeight = ee_combat_weapon_height(&in, theChain.empty() ? NULL : (const float(*)[16])&theChain[0]);
    if (EESimCaptureRecord::Active()) {
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'W', 'P'));
        theRecord.Bytes(&in, sizeof(in));
        if (!theChain.empty())
            theRecord.F32s(&theChain[0], (unsigned int)theChain.size());
        theRecord.F32(theHeight);
        theRecord.Commit();
    }
    return theHeight;
}

static const int32_t* EELineOfFireTileLine(void* inContext, int32_t inFromX, int32_t inFromY, int32_t inToX, int32_t inToY,
                                           uint32_t* outCount)
{
    EELineOfFireHost* theHost = (EELineOfFireHost*)inContext;
    std::vector<U2DPoint<long> > theTiles;
    EEGetTileLine(inFromX, inFromY, inToX, inToY, &theTiles, 0, 1);
    theHost->mPairs.clear();
    for (size_t i = 0; i < theTiles.size(); i++) {
        theHost->mPairs.push_back((int32_t)theTiles[i].mX);
        theHost->mPairs.push_back((int32_t)theTiles[i].mY);
    }
    *outCount = (uint32_t)theTiles.size();
    return theHost->mPairs.empty() ? NULL : &theHost->mPairs[0];
}

static void EELineOfFireTargetTop(void* inContext, EeCombatTargetTop* outTop)
{
    EEUnit* inTarget = ((EELineOfFireHost*)inContext)->mTarget;
    EEComplexUnit* theTarget = inTarget->GetComplexUnit();
    if (theTarget == NULL)
        return;
    unsigned long theFrame = theTarget->mPhysicalModel.GetFrame();
    unsigned long theSet = theTarget->mAnimation;
    outTop->present = 1;
    outTop->box_top = inTarget->GetModel()->mModels[theSet]->GetBoundingBox(theFrame).GetTopRightBack().GetZ();
    outTop->scale = inTarget->mStats->GetScale();
    outTop->built = theTarget->mIsBuilt;
    outTop->hit_points = (int32_t)theTarget->GetHitPoints();
    outTop->max_hit_points = (int32_t)theTarget->mStats->mMaxHitPoints;
    outTop->z = theTarget->mPhysicalModel.GetLocation().GetZ();
}

static int32_t EELineOfFireTileTopAbove(void* inContext, int32_t inX, int32_t inY, float inHeight, int32_t inBare)
{
    EELineOfFireHost* theHost = (EELineOfFireHost*)inContext;
    return EEIsTileTopAbove((long)inX, (long)inY, inHeight, theHost->mBlocker, inBare != 0);
}

// (new: the walk is ee_combat's line_blocked; the weapon point, the target's top, the tile line and the walls stay here)
static bool EEIsLineToUnitBlockedWalk(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5)
{
    EELineOfFireHost theContext = {inUnit, inTarget, outArg3, std::vector<int32_t>()};
    EeCombatLineHost theHost = {&theContext, EELineOfFireStartHeight, EELineOfFireTileLine, EELineOfFireTargetTop,
                                EELineOfFireTileTopAbove};
    EeCombatLineIn in = {};
    in.from_x = (int32_t)inUnit->GetTileX();
    in.from_y = (int32_t)inUnit->GetTileY();
    in.to_x = (int32_t)inTarget->GetTileX();
    in.to_y = (int32_t)inTarget->GetTileY();
    in.skips_check = inUnit->GetUnitClass()->mSkipsLineOfFireCheck;
    in.target_alive = inTarget->GetHitPoints() > 0;
    in.bare = inUnit->GetUnitClass()->mIsMelee;
    EeCombatLineOut theOut = ee_combat_line_blocked(&in, &theHost);
    *outArg4 = theOut.x;
    *outArg5 = theOut.y;
    return theOut.blocked != 0;
}

// Shapes a wall ghost as the piece for a corner
void EEOrientWallGhost(EEGhostUnit* inGhost, long inCorner, long inRaisedCorners)
{
    GETransformation theTransformation(inGhost->mTransformation);
    inGhost->SetAnimation(EEOrientWallPiece(theTransformation, inCorner, inRaisedCorners));
    inGhost->mTransformation = theTransformation;
}

// Returns the frame set of a wall piece for a corner (1..12) and sets its facing
long EEOrientWallPiece(GETransformation& ioTransformation, long inCorner, long inRaisedCorners)
{
    EeUnitsUuWallPiece thePiece = ee_units_uu_wall_piece(inCorner);
    if (thePiece.turns)
        ioTransformation.SetOrientationYPR(thePiece.yaw, 0.0f, 0.0f);
    return thePiece.frame_set;
}

// Moves a unit to a transformation, snapping to the closest tile when off the map
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY)
{
    EEUUNote theNote(81); theNote.M(*inTransformation).U(inUnit);   // TEST ONLY (UUTL)
    GE3DPoint thePoint;
    *outX = UMFloor(inTransformation->GetX());
    *outY = UMFloor(inTransformation->GetY());
    EETile* theTile = ::gWorld.mMap->GetTile(*outX, *outY);
    if (theTile != NULL) {
        inUnit->SetTransformation(*inTransformation, true);
    } else {
        thePoint.SetX(inTransformation->GetX());
        thePoint.SetY(inTransformation->GetY());
        thePoint.SetZ(inTransformation->GetZ());
        ::gWorld.mMap->GetClosestTile(&thePoint, outX, outY);
        inUnit->mPhysicalModel.SetLocation(*inTransformation);
        inUnit->MoveTo(*outX, *outY, true);
        if ((long)inUnit->mWaypoints.size() > 0 && (inUnit->mUnitClass->mDomains & kMovementAir) != 0) {
            GE3DPoint theGoal;
            inUnit->GetNextWaypointPoint(theGoal);
            float theHeight = inUnit->mUnitClass->GetHeightAt(inUnit, theGoal);
            if (theHeight != inUnit->mHeight)
                inUnit->mHeight = theHeight;
        }
    }
    theNote.I(*outX).I(*outY).M(inUnit->mPhysicalModel.GetLocation());   // TEST ONLY (UUTL)
}

// Shapes a wall piece as the piece for a corner
void EEShapeWall(EEComplexUnit* inUnit, long inCorner, long inRaisedCorners)
{
    ee_units_uu_shape_wall(UUHandle(static_cast<EEUnit*>(inUnit)), inCorner, inRaisedCorners);   // ee_units uu_world.rs shape_wall
}

// Builds the text of a civilization upgrade ("category: attribute percent%"); false when a string is missing
// The text of civilization upgrade inUpgrade ("<category>: <attribute> <percent>%"); false when a string is missing.
// Its two function-local static strings' atexit dtors follow it (0x24e7c4 / 0x24e7cf).
bool EEFn24E68F(EECivUpgrade inUpgrade, UWideString& outText)
{
    unsigned long theCategory;
    unsigned long theIndex;
    float theValue;
    float theBonus;
    long theStringID;
    bool theResult = false;
    gCivilizationManager.SplitAttribute(inUpgrade, &theCategory, &theIndex);
    gCivilizationManager.LookupLevel(inUpgrade, &theValue, &theBonus);
    if (gCivilizationManager.GetCategoryNameID(theCategory, &theStringID)) {
        static UWideString sCategory;
        if (gResource->GetString(theStringID, sCategory) == 0) {
            static UWideString sAttribute;
            theStringID = EECivilizationManager::LevelNameID(theIndex);   // civbuilder.yaml attribute names
            if (gResource->GetString(theStringID, sAttribute) == 0) {
                if (!EECivilizationManager::IsPowerLevel(theIndex))
                    sprintf(outText, "%s: %s %ld%%", (const char*)sCategory, (const char*)sAttribute,
                            (long)(theValue * 100.0f + 0.5f));
                else
                    sprintf(outText, "%s", (const char*)sAttribute);
                theResult = true;
            }
        }
    }
    return theResult;
}

// Converts a unit to another player: a converted priest may hand over a civ power or upgrade, the class
// follows the new owner, the new unit keeps hit points, actions and groups
// inUnit converted to player inPlayer (the priests, the scenario effect): a converted priest or prophet may hand over
// a civilization power (or a random upgrade) the new owner lacks; the class follows the new owner's powers; the new
// unit takes over its hit point share, actions and groups; inArg3 counts it in the statistics
EEComplexUnit* EEConvertUnit(EEComplexUnit* inUnit, long inPlayer, bool inIsConversion)
{
    EEUUNote theNote(86); theNote.U(inUnit).I(inPlayer).I(inIsConversion);   // TEST ONLY (UUTL)
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    EEComplexUnit* theOld = NULL;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    if ((theClass->mUnitType == kUnitTypeSettlement || theClass->mUnitType == kUnitTypeTownCenter) && gCivPowersManager.HasPower(thePlayer, kCivPowerAdaptation)
        && inUnit->GetComplexUnit()->mIsBuilt) {
        bool theGranted = false;
        UWideString theFormat;
        UWideString theMessage;
        UWideString theName;
        unsigned long theUpgrade = 0;
        for (unsigned long i = 0x26; i < gCivPowersManager.GetNumPowers() + 0x26; i++) {
            if (gCivPowersManager.HasPower(inUnit->GetPlayer(), i) && !gCivPowersManager.HasPower(thePlayer, i)) {
                theUpgrade = gCivilizationManager.MakePowerUpgrade(EECivilizationManager::PowerLevel(i));
                gCivPowersManager.GetName(i, theName);
                goto grant;
            }
        }
        {
            std::vector<EECivUpgrade>& theAll = inUnit->GetPlayer()->mCivilization.GetPurchasedUpgrades();
            std::vector<EECivUpgrade> theList;
            std::vector<EECivUpgrade>::iterator it; for (it = theAll.begin(); it != theAll.end(); it++)
                theList.push_back(*it);
            EEAIRandomShuffle(theList.begin(), theList.end());
            it = theList.begin();
            while (true) {
                if (it == theList.end())
                    break;
                if (!EEHasCivilizationUpgrade(&thePlayer->mCivilization, *it)) {
                    theUpgrade = *it;
                    EEFn24E68F(theUpgrade, theName);
                    theGranted = true;
                    break;
                }
                it++;
            }
        }
        if (theGranted) {
        grant:
            EECivilization& theCivilization = thePlayer->mCivilization;
            long thePoints = theCivilization.GetAvailablePoints();
            unsigned long theCost;
            theCivilization.CanAfford(theUpgrade, &theCost, NULL);
            theCivilization.SetPointsAndMax(theCivilization.GetMaxPoints() + theCost);
            theCivilization.Buy(theUpgrade, thePlayer->GetPlayerID(), true);
            theCivilization.SetPoints(thePoints);
            if (::gWorld.GetCurrentPlayer() == thePlayer) {
                gResource->GetString(IDS_ACQUIRED_THROUGH_ADAPTATION, theFormat);
                sprintf(theMessage, theFormat, (const char*)theName);
                gUserInterface->AddChatLogEntry(inUnit->GetPlayer()->mColor, inUnit->GetPlayer()->mName, theMessage,
                                                gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
            }
        }
        if (theClass->mUnitType == kUnitTypeTownCenter)
            theClass = gUnitClassTable[kUnitTypeSettlement];
    }
    if (!gCivPowersManager.HasPower(thePlayer, kCivPowerFlamingArrows)) {
        long theType = theClass->mUnitType;
        if (theType == kUnitTypeArch03SimpleBowmanFlaming || theType == kUnitTypeArch05CompositeBowFlaming || theType == kUnitTypeArch06LongBowFlaming) {
            switch (theType) {
            case kUnitTypeArch03SimpleBowmanFlaming: theClass = gUnitClassTable[kUnitTypeArch03SimpleBowman]; break;
            case kUnitTypeArch05CompositeBowFlaming: theClass = gUnitClassTable[kUnitTypeArch05CompositeBow]; break;
            case kUnitTypeArch06LongBowFlaming: theClass = gUnitClassTable[kUnitTypeArch06LongBow]; break;
            }
        }
    }
    if (!gCivPowersManager.HasPower(thePlayer, kCivPowerCrusaders) && theClass->IsCrusader()) {
        switch (theClass->mUnitType) {
        case kUnitTypeInf04ShortSwordCrusader: theClass = gUnitClassTable[kUnitTypeInf04ShortSword]; break;
        case kUnitTypeInf06LongSwordCrusader: theClass = gUnitClassTable[kUnitTypeInf06LongSword]; break;
        case kUnitTypeCav04BronzeSpearCavalryCrusader: theClass = gUnitClassTable[kUnitTypeCav04BronzeSpearCavalry]; break;
        case kUnitTypeCav06KnightCrusader: theClass = gUnitClassTable[kUnitTypeCav06Knight]; break;
        }
    }
    if (!gCivPowersManager.HasPower(thePlayer, kCivPowerEmissaries) && theClass->mUnitType == kUnitTypeEmissary)
        theClass = gUnitClassTable[kUnitTypePriest];
    float theFraction = (float)inUnit->GetHitPoints() / (float)inUnit->mStats->mMaxHitPoints;
    inUnit->GetPlayer()->DeselectUnit(inUnit->GetComplexUnit());
    for (long i = 0; i < 20; i++) {
        if (inUnit->GetPlayer()->mGroups[i]->Contains(inUnit->GetComplexUnit()))
            inUnit->GetPlayer()->mGroups[i]->RemoveUnit(inUnit->GetComplexUnit());
    }
    thePlayer->SetUnitClassMedia(inUnit->GetUnitClass()->mUnitType, 0x7f);
    EEUnit* theNewUnit = EEMakeUnitCopy(inUnit, inPlayer, theClass);
    thePlayer->UnloadUnitClassMedia(inUnit->GetUnitClass()->mUnitType);
    if (inUnit->GetUnitClass() != theClass && theClass->mUnitType == kUnitTypeSettlement)
        theNewUnit->GetComplexUnit()->ClearGarrison();
    EEComplexUnit* theNew = theNewUnit->GetComplexUnit();
    if (theNew != NULL) {
        if (theNew->GetUnitClass()->IsLandBuilding() && theNew->mIsBuilt) {
            if (!theNew->GetUnitClass()->mIsWall || theNew->GetUnitClass()->mIsTower)
                theNew->StartAction(kActionIdle);
            if (theNew->GetBuildQueueSize() > 0)
                theNew->mBuildQueue.clear();
            if (theClass->mIsWall && theClass->mCategory != kUnitCategoryGate)
                theNew->SetAnimation(inUnit->GetComplexUnit()->mAnimation, -1, 0);
            else if (theClass->mCategory == kUnitCategoryGate)
                theNew->StartAction(inUnit->GetComplexUnit()->mCommand != NULL ? inUnit->GetComplexUnit()->mCommand->mType : 0);
        } else {
            if (theClass->mCategory == kUnitCategoryAirplane || theClass->mCategory == kUnitCategoryPlanetSpaceship)
                theNew->StartAction(kActionCircle);
            else
                theNew->StartAction(kActionIdle);
        }
        theOld = inUnit->GetComplexUnit();
        gPhysics->QueueUnitSwap(theOld, theNew);
        if (theOld->mPreviousPart == NULL)
            inUnit->GetUnitClass()->GateReplaced(inUnit, theNew);
        theNew->ClearAllGoalQueues();
        theNew->mBuildQueue.clear();
        theNew->ClearBuildTargets();
        theNew->PlayEffect(100000);
        if (theNew->mProductionTime != 0)
            theNew->mProductionTime = 0;
        if (!theNew->GetUnitClass()->mIsWall)
            theClass->ActionChanged(theNew);
    }
    inUnit->Kill(false, false, true);
    if (inIsConversion) {
        EEPlayerStatistics& theStatistics = thePlayer->mSavedGroups;
        theStatistics.Reset(5, 1);
        long theSeries = EEAddDeadUnitStatistic(inUnit);
        if (theSeries != 0) {
            inUnit->GetPlayer()->mSavedGroups.Lower(theSeries, 1);
            theStatistics.Reset(theSeries, 1);
        }
        if (inUnit->GetUnitClass()->mUnitType != kUnitTypePathPoint) {
            long theCategory = inUnit->GetUnitClass()->mCategory;
            if (theCategory == kUnitCategoryBuilding || theCategory == kUnitCategoryBuilding19 || theCategory == kUnitCategoryGate || theCategory == kUnitCategoryMiningBuilding
                || theCategory == kUnitCategoryFortress)
                inUnit->GetPlayer()->mSavedGroups.Reset(1, 1);
            else
                inUnit->GetPlayer()->mSavedGroups.Reset(4, 1);
        }
    }
    if (inUnit->GetUnitClass()->mDamageEffect != NULL && theNew->GetUnitClass()->mDamageEffect != NULL && theOld != NULL
        && theOld->mIsBuilt && theFraction < inUnit->GetUnitClass()->mDamageEffect->mParam2)
        gWorld.GetEffectManager()->CreateUnitEffect(theNew, theNew->GetUnitClass()->mDamageEffect, NULL, 0, 0, 0);
    theNote.U(theNew).C(theNew != NULL ? theNew->mUnitClass : NULL);   // TEST ONLY (UUTL)
    return theNew;
}

// Tells whether a civilization bought an upgrade
// Whether inValue is in inCivilization's mPurchasedUpgrades (read through the out-of-line GetPurchasedUpgrades 0x12c738, ICF with EECampaign::GetScenarios)
bool EEHasCivilizationUpgrade(EECivilization* inCivilization, EECivUpgrade inValue)
{
    std::vector<EECivUpgrade>& theList = inCivilization->GetPurchasedUpgrades();
    for (std::vector<EECivUpgrade>::iterator it = theList.begin(); it != theList.end(); it++) {
        if (*it == inValue)
            return true;
    }
    return false;
}

// Makes the units on a building site's footprint step aside for it
void EEStepAsideFromSite(EEUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, EETileCoordinate& inTile, bool inForce)
{
    ee_units_uu_step_aside(UUHandle(inUnit), UUHandle(inClass), UUHandle(inPlayer), inTile.mX, inTile.mY, inForce);   // uu_world.rs
}

// Moves a container's cargo into the unit's care (also placing it on the map for a conversion)
void EEMovePassengers(EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIsConversion)
{
    EEPlayer* thePlayer = inUnit->mPlayer;
    GETransformation theTransformation;
    theTransformation.SetPosition(-10000.0f, -10000.0f, 0.0f);
    if (inContainer == NULL || !(inContainer->GetNumCargoUnits() > 0))
        return;
    while (inContainer->GetNumCargoUnits() != 0) {
        // (EEConvertUnit returns the unit it placed: the original passes its result on)
        EEComplexUnit* theCargo = inContainer->UnloadCargoAt(inContainer->GetNumCargoUnits() - 1, theTransformation);
        if (inIsConversion)
            theCargo = EEConvertUnit(theCargo, thePlayer->GetPlayerID(), 1);
        inUnit->LoadCargo(theCargo, false, inIsConversion);
    }
}

// Gives a unit another unit's load (as much as its class allows), emptying the other
void EETakeOverLoad(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    inUnit->AddGarrison(inOther->mGarrisonCount - inUnit->mUnitClass->mInitialGarrisonCount, inOther->mGarrisonPopulation);
    inOther->ClearGarrison();
}

// ---- 0x24f0e9..0x24f932: the rest of this TU's own functions (the unit map's "EEHitPointUpgrader.cpp" slice; the
// template COMDATs that follow, 0x24f932..0x2544e9, are instantiated by this file's functions, so the TU goes on) ----

// The functor that rescales the hit points of every unit of type mUnitType by mRatio (vtable 0x449bd8, shares
// EEUUUnitFunctor's ??_G) (provisional names)
class EEHitPointUpgrader : public EEUUUnitFunctor {
public:
    EEHitPointUpgrader(unsigned long inUnitType, float inRatio) : mUnitType(inUnitType), mRatio(inRatio) {}
    virtual void Apply(EEComplexUnit* inUnit);
    unsigned long mUnitType;                // +0x04
    float mRatio;                           // +0x08
};

// Raises a class upgrade's hit points by a share of its base, and the player's units of that type with it
// inUpgrade's hit points raised by inFactor of its class's base; inPlayer's units of type inType follow (inferred)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer)
{
    long theBase = inUpgrade->mUnitClass->mHitPoints;
    EEHitPointUpgrader theUpgrader(inType, (theBase * inFactor + inUpgrade->GetMaxHitPoints()) / inUpgrade->GetMaxHitPoints());
    long theBonus = inUpgrade->mUnitClass->mHitPoints;
    if (theBonus <= 0)
        return;
    inFactor *= theBonus;
    long theHitPoints = max(UMFloatToLong(inUpgrade->GetMaxHitPoints() + inFactor), 1);
    inUpgrade->mMaxHitPoints = max(theHitPoints, 1);
    if (!gLoadingSavedGame)
        EEForEachUnitOf(inPlayer, &theUpgrader, true);
}

// Scales the hit points of a living unit of the upgraded type by the ratio (at least 1)
void EEHitPointUpgrader::Apply(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mUnitType == this->mUnitType && inUnit->mHitPoints > 0 && !inUnit->IsFoundation()) {
        if (inUnit->mHitPoints > 0)
            inUnit->SetHitPoints(max(UMFloatToLong(inUnit->mHitPoints * this->mRatio), 1));
    }
}

// Tells whether a unit type can be placed on a tile for a player
bool EECanPlaceOnTile(EEPlayer* inPlayer, long inUnitType, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    bool theFlag;
    return EECanPlaceUnitOnTile(inPlayer, NULL, inUnitType, inX, inY, inSkipPlayerChecks, inIgnoreOwner, theFlag);
}

// Tells whether a unit type can be placed at a location (the editor's own test in free camera mode)
bool EECanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    if (gFreeCamera == true)
        return EEEditorCanPlace(inPlayer, inType, inLocation, inX, inY);
    bool theFlag;
    return EECanPlaceUnitOnTile(inPlayer, NULL, inType, inX, inY, inSkipPlayerChecks, inIgnoreOwner, theFlag);
}

// Tells whether a unit type can be placed on a tile for a player (or a unit's continent)
bool EECanPlaceUnitOnTile(EEPlayer* inPlayer, EEComplexUnit* inUnit, long inUnitType, long inX, long inY, bool inSkipPlayerChecks,
                long inIgnoreOwner, bool& outFlag)
{
    uint8_t theFlag = 2;   // (written only when the tile's blockers were asked, as the C++ wrote outFlag)
    bool theResult = ee_units_uu_can_place_unit_on_tile(UUHandle(inPlayer), UUHandle(static_cast<EEUnit*>(inUnit)), inUnitType, inX, inY,
                                                        inSkipPlayerChecks, inIgnoreOwner, &theFlag) != 0;
    if (theFlag != 2)
        outFlag = theFlag != 0;
    return theResult;
}

// Tells whether a unit type can be placed in the scenario editor (free camera)
// The scenario editor's (free camera) placement test (inferred)
bool EEEditorCanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY)
{
    return ee_units_uu_editor_can_place(UUHandle(inPlayer), inType, (const float(*)[3])&inLocation->mX, inX, inY) != 0;   // uu_world.rs
}

// (0x64f6f7, EEIsEditorSpacingClear: ee_units uu_world.rs editor_spacing_clear)

// Tells whether a tile has no unit (trees aside) blocking a class for a player, and is not space
bool EEIsTileUnblocked(EEPlayer* inPlayer, long inUnitClass, EETile* inTile)
{
    return ee_units_uu_is_tile_unblocked(UUHandle(inPlayer), inUnitClass, UUHandle(inTile)) != 0;
}

// Gives a space wall its energy wall effect
void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectSpaceWallEnergyWall), &theHandle, 0, 0, 0);
    inUnit->mFireEffect = theHandle;
}

// Gives a space gate its energy wall effect
void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectSpaceGateEnergyWall), &theHandle, 0, 0, 0);
    inUnit->mFireEffect = theHandle;
}

// ---- the unit utilities' world (ee_units uu.rs over uu_host.rs): units, classes, players, tiles, maps and continents
// cross as their objects' addresses (a unit as its EEUnit*), the vectors as the C++ objects ----

extern "C" uint8_t ee_units_uu_host_is_edge_tile(int32_t inX, int32_t inY) { return ::gWorld.mMap->IsEdgeTile((long)inX, (long)inY); }
extern "C" int32_t ee_units_uu_host_raised_corners(uint64_t inMap, int32_t inX, int32_t inY)
{
    return (int32_t)((EEMap*)(uintptr_t)inMap)->GetNumRaisedCorners(inX, inY);
}
extern "C" float ee_units_uu_host_mesh_z(uint64_t inMap, int32_t inX, int32_t inY) { return ((EEMap*)(uintptr_t)inMap)->mTerrainMesh.GetZ(inX, inY); }
extern "C" float ee_units_uu_host_mesh_ground(float inX, float inY) { return ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY); }


extern "C" void ee_units_uu_host_position(uint64_t inUnit, float* outPoint)
{
    GE3DPoint thePoint;
    UUUnit(inUnit)->GetPosition(thePoint);
    outPoint[0] = thePoint.mX;
    outPoint[1] = thePoint.mY;
    outPoint[2] = thePoint.mZ;
}
extern "C" uint8_t ee_units_uu_host_passable_by(uint64_t inUnit, uint64_t inBy)
{
    return UUUnit(inUnit)->IsPassableBy(inBy != 0 ? static_cast<EEComplexUnit*>(UUUnit(inBy)) : NULL);
}
extern "C" uint8_t ee_units_uu_host_is_foundation(uint64_t inUnit) { return UUUnit(inUnit)->IsFoundation(); }
extern "C" uint8_t ee_units_uu_host_can_move_on_terrain(uint64_t inClass, uint64_t inUnit, int32_t inTerrain)
{
    return ((EEUnitClass*)(uintptr_t)inClass)->CanMoveOnTerrain(UUUnit(inUnit), inTerrain);
}
extern "C" int32_t ee_units_uu_host_passable_flags(uint64_t inClass, uint64_t inUnit)
{
    return (int32_t)((EEUnitClass*)(uintptr_t)inClass)->GetPassableFlags(UUUnit(inUnit));
}
extern "C" uint32_t ee_units_uu_host_surrounding_tiles(uint64_t inTarget, uint8_t inVirtual, int32_t (*outTiles)[2], uint32_t inCapacity)
{
    EEComplexUnit* theTarget = static_cast<EEComplexUnit*>(UUUnit(inTarget));
    std::vector<EETileCoordinate> theTiles;
    if (inVirtual)
        theTarget->mUnitClass->GetSurroundingTiles(theTarget, theTiles, 1, false);
    else
        theTarget->mUnitClass->BaseGetSurroundingTiles(theTarget, theTiles, 1, false);
    return UUPairs(theTiles, outTiles, inCapacity);
}
extern "C" uint64_t ee_units_uu_host_container_continent(uint64_t inContainer, uint64_t inUnit)
{
    EEUnit* theContainer = UUUnit(inContainer);
    return UUHandle(theContainer->mUnitClass->GetContainerContinent(theContainer->GetComplexUnit(), static_cast<EEComplexUnit*>(UUUnit(inUnit))));
}
// the continent manager's questions (uu_host.rs CONTINENT_*)
extern "C" uint64_t ee_units_uu_host_continent(int32_t inWhich, uint64_t inA, uint64_t inB, uint64_t inC)
{
    EEAIContinent* theA = (EEAIContinent*)(uintptr_t)inA;
    long theX = (long)(int32_t)inA;
    long theY = (long)(int32_t)inB;
    switch (inWhich) {
    case 0: return gContinentManager.GetContinentID(theX, theY);
    case 1: return UUHandle(gContinentManager.GetContinentByID((unsigned short)inA));
    case 2: return UUHandle(gContinentManager.GetContinentAt(theX, theY));
    case 3: return theA->GetID();
    case 4: return gContinentManager.AreConnected(theA, (EEAIContinent*)(uintptr_t)inB, (long)(int32_t)inC);
    case 5: return gContinentManager.AreConnectedForUnit(UUUnit(inC), theA, (EEAIContinent*)(uintptr_t)inB);
    case 6: return (uint64_t)(int64_t)gContinentManager.GetTileFlag(theX, theY);
    case 7: return gContinentManager.AreAdjacent(inA, inB);
    case 8: return theA->GetCategory();
    case 9: return gContinentManager.AreConnectedByID((unsigned short)inA, (unsigned short)inB, (long)(int32_t)inC);
    case 10: return gContinentManager.GetContinent(U2DPoint<long>(theX, theY), (long)(int32_t)inC);
    case 11: return gContinentManager.IsShoreTile(theX, theY);
    case 12: return static_cast<EEComplexUnit*>(UUUnit(inA))->GetFootprintContinent((long)(int32_t)inC);
    }
    return 0;
}
// (one call: the line's CBTL record is written once, as the C++ callers' was; the room is the line's most)
extern "C" uint32_t ee_units_uu_host_tile_line(int32_t inX1, int32_t inY1, int32_t inX2, int32_t inY2, uint8_t inEnds, uint8_t inThin,
                                               int32_t (*outTiles)[2], uint32_t inCapacity)
{
    std::vector<U2DPoint<long> > theTiles;
    EEGetTileLine(inX1, inY1, inX2, inY2, &theTiles, inEnds != 0, inThin != 0);
    return UUPairs(theTiles, outTiles, inCapacity);
}
extern "C" uint8_t ee_units_uu_host_reserved(uint64_t inGroup, const float* inPoint)
{
    return ((EEActiveGroup*)(uintptr_t)inGroup)->IsReserved(GE3DPoint(inPoint[0], inPoint[1], inPoint[2]));
}
extern "C" void ee_units_uu_host_ground_point(const float* inPoint, float* outPoint)
{
    GE3DPoint theGround;
    ::gWorld.mMap->GetGroundPoint(GE3DPoint(inPoint[0], inPoint[1], inPoint[2]), theGround);
    outPoint[0] = theGround.mX;
    outPoint[1] = theGround.mY;
    outPoint[2] = theGround.mZ;
}
extern "C" float ee_units_uu_host_terrain_tilt(const float* inMatrix, uint8_t inRoll)
{
    GETransformation theTransformation;
    memcpy(theTransformation.mMatrix, inMatrix, sizeof(theTransformation.mMatrix));
    return inRoll ? ::gWorld.mMap->GetTerrainRoll(theTransformation) : ::gWorld.mMap->GetTerrainPitch(theTransformation);
}
// a player's questions (uu_host.rs PLAYER_*; the unit set's in the player's place)
extern "C" int32_t ee_units_uu_host_player(int32_t inWhich, uint64_t inPlayer, int32_t inValue)
{
    EEPlayer* thePlayer = (EEPlayer*)(uintptr_t)inPlayer;
    switch (inWhich) {
    case 1: return thePlayer->IsHuman();
    case 2: return gUnitSetManager.Contains((long)inPlayer, (unsigned long)inValue);
    case 3: return (int32_t)thePlayer->mActiveGroup->GetNumUnits();
    case 4: return thePlayer->mActiveGroup->GetContinent();
    case 5: return (int32_t)thePlayer->GetCitizenPassability();
    }
    return 0;
}
extern "C" int32_t ee_units_uu_host_facing_quarter(int32_t inX, int32_t inY) { return (int32_t)EEGetFacingQuarter(inX, inY); }
extern "C" int32_t ee_units_uu_host_resource_users(uint64_t inUnit) { return (int32_t)::gWorld.GetResourceUsers(UUUnit(inUnit)); }
extern "C" int32_t ee_units_uu_host_random_long(int32_t inMin, int32_t inMax) { return (int32_t)gPhysics->GetRandom((long)inMin, (long)inMax); }
extern "C" float ee_units_uu_host_random_float(float inMin, float inMax) { return gPhysics->GetRandom(inMin, inMax); }
extern "C" void ee_units_uu_host_set_animation(uint64_t inComplex, int32_t inAnimation)
{
    static_cast<EEComplexUnit*>(UUUnit(inComplex))->SetAnimation(inAnimation, -1, 0);
}
extern "C" void ee_units_uu_host_set_transformation(uint64_t inComplex, const float* inMatrix)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(UUUnit(inComplex));
    GETransformation theTransformation(theUnit->mPhysicalModel.GetLocation());
    memcpy(theTransformation.mMatrix, inMatrix, sizeof(theTransformation.mMatrix));
    theUnit->SetTransformation(theTransformation, true);
}

// a tile's questions: flat (the map's), visible to the player, a valid or a space tile of the terrain mesh
extern "C" uint8_t ee_units_uu_host_tile_test(int32_t inWhich, uint64_t inTile, uint64_t inPlayer, int32_t inX, int32_t inY)
{
    switch (inWhich) {
    case 0: return ::gWorld.mMap->IsTileFlat(inX, inY);
    case 1: return UUTile(inTile)->IsVisibleTo((EEPlayer*)(uintptr_t)inPlayer);
    case 2: return ::gWorld.mMap->mTerrainMesh.GetIsValidTile((unsigned long)(uint32_t)inX, (unsigned long)(uint32_t)inY);
    case 3: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceTile((unsigned long)(uint32_t)inX, (unsigned long)(uint32_t)inY);
    }
    return 0;
}
extern "C" void ee_units_uu_host_center_offset(uint64_t inComplex, float* ioPoint)
{
    static_cast<EEComplexUnit*>(UUUnit(inComplex))->AddCenterOffset(ioPoint[0], ioPoint[1]);
}
// a tile list's questions: all in one area of the kind (inWhich 0), any shore tile
extern "C" uint8_t ee_units_uu_host_tiles_test(int32_t inWhich, const int32_t (*inTiles)[2], uint32_t inCount, int32_t inKind)
{
    std::vector<U2DPoint<long> > theTiles;
    for (uint32_t i = 0; i < inCount; i++)
        theTiles.push_back(U2DPoint<long>(inTiles[i][0], inTiles[i][1]));
    return inWhich == 0 ? gContinentManager.InSameArea(theTiles, inKind) : gContinentManager.HasShoreTile(theTiles);
}

// the spot searches' questions (ee_units uu_spots.rs): where a unit dropped onto inUnit stands (0), whether a piece is
// left out (1), the path area at (inX, inY) (2), a gate's two ends (3)
extern "C" int32_t ee_units_uu_host_spot(int32_t inWhich, uint64_t inUnit, int32_t inX, int32_t inY, float* outPoint)
{
    EEUnit* theUnit = UUUnit(inUnit);
    GE3DPoint thePoint;
    switch (inWhich) {
    case 0:
        if (theUnit->GetHitPoints() <= 0 && theUnit->mComplexUnit != NULL && theUnit->GetModel()->mModel != NULL) {
            GE3DPoint theOffset = theUnit->GetModel()->mTopPoint;
            theOffset.SetX(theOffset.GetX() * theUnit->mComplexUnit->mPhysicalModel.GetScale());
            theOffset.SetY(theOffset.GetY() * theUnit->mComplexUnit->mPhysicalModel.GetScale());
            theUnit->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, thePoint);
        } else {
            theUnit->GetPosition(thePoint);
        }
        break;
    case 1:
        return theUnit->mUnitClass->IsPartLeftOut(static_cast<EEComplexUnit*>(theUnit));
    case 2: {
        EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(U2DPoint<long>(inX, inY));
        return theArea != NULL ? theArea->GetID() : 0;
    }
    case 3: {
        EEComplexUnit* theGate = static_cast<EEComplexUnit*>(theUnit);
        EEUnitClass* theClass = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theGate->mUnitClass);
        theClass->GetLastLinkedCarrier(theGate->GetOutermostCarrier())->GetPosition(thePoint);
        GE3DPoint theSecond;
        theClass->GetLastLinked(theGate->GetOutermostCarrier())->GetPosition(theSecond);
        outPoint[3] = theSecond.mX;
        outPoint[4] = theSecond.mY;
        outPoint[5] = theSecond.mZ;
        break;
    }
    }
    outPoint[0] = thePoint.mX;
    outPoint[1] = thePoint.mY;
    outPoint[2] = thePoint.mZ;
    return 0;
}

// the vectors of tiles and units
// ---- the round-2 world (ee_units uu_world.rs over uu_host.rs): the questions and writes of the moved bodies ----

// EEPlanWall's new piece (its CreateUnit, shape, health, payment and paid flag, in the C++'s order)
static EEUnit* UUMakeWallPiece(EEUnitClass* inClass, EEPlayer* inPlayer, long inX, long inY, long inCorner, bool inAsFoundation)
{
    EEUnit* thePiece = inClass->CreateUnit(inX, inY, inPlayer->GetPlayerID(), inAsFoundation, NULL);
    EEComplexUnit* theComplex = thePiece->GetComplexUnit();
    EEShapeWall(theComplex, inCorner, ::gWorld.GetMap()->GetNumRaisedCorners(inX, inY));
    if (inAsFoundation) {
        thePiece->SetHitPoints(1);
        // (the original calls RTSTile::GetVisibility out of line through a helper level, 0x2035cf)
        unsigned long theVisibility = thePiece->mTile->GetVisibility(inPlayer->GetPlayerID());
        if (theVisibility > 0 && thePiece->mTile->GetVisibility(inPlayer->GetPlayerID()) <= 1)
            gDoppelgangerManager.AddForUnseenUnit(static_cast<EEComplexUnit*>(thePiece));
        inPlayer->Pay(inPlayer->GetTechTree()->Get(inClass->GetTechnology()), 0x20, 1.0f);
        if (theComplex->mUnitClass->mIsWall && !theComplex->mUnitClass->mIsTower)
            theComplex->GetPlayer()->AddConstructionSite(theComplex);
    } else {
        thePiece->SetHitPoints(thePiece->GetStats()->mMaxHitPoints);
        theComplex->SetBuilt(1, 0, 1);
        if (theComplex->mUnitClass->GetUnitType() == kUnitTypeWallSpace && inPlayer->GetTechTree()->Get(kTechWallSpace)->GetIsAvailable())
            EECreateSpaceWallEnergyWall(theComplex);
    }
    if (theComplex->mIsPaidFor != true)
        theComplex->mIsPaidFor = true;
    return thePiece;
}

extern "C" uint64_t ee_units_uu_host_world(int32_t inWhich, uint64_t inA, uint64_t inB, uint64_t inC, int32_t inX, int32_t inY, float inF)
{
    EEUnit* theUnit = UUUnit(inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    EEActiveGroup* theGroup = (EEActiveGroup*)(uintptr_t)inA;
    switch (inWhich) {
    case 2:
        UUEvent(UUId(theUnit), UUId(UUUnit(inB)), 1);   // TEST ONLY (UUTL)
        theComplex->AddBuildTarget(UUUnit(inB));
        return 0;
    case 3:
        UUEvent(UUId(theUnit), 0, 2);   // TEST ONLY (UUTL)
        theComplex->ClearGoals(0);
        theComplex->StartAction(kActionIdle);
        theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    case 4: return ((EEPlayer*)(uintptr_t)inA)->CanProduce(((EEUnitClass*)(uintptr_t)inB)->GetTechnology(), 0x20, 1, true, 3, NULL);
    case 5: return UUHandle(UUMakeWallPiece((EEUnitClass*)(uintptr_t)inA, (EEPlayer*)(uintptr_t)inB, inX, inY, (long)(int32_t)inC, inF != 0.0f));
    case 6: {
        EETile* theTile = UUTile(inA);
        if (inX != 0)
            theTile->mUnits = theTile->AddUnit(UUUnit(inB), theTile->mUnits);
        else
            theTile->RemoveUnit(UUUnit(inB));
        return 0;
    }
    case 7: {   // EEClearUnderBuilding's unit
        EEUnitClass* theClass = theUnit->mUnitClass;
        if (EEUnitClassOfKind<EE_CLASS_KIND_AMBIENT>(theClass) == NULL || theClass->mUnitType == kUnitTypePathPoint || !theClass->mIsDeletable)
            return 0;
        if (theUnit->mResourceAmount != 0)
            theUnit->mResourceAmount = 0;
        theUnit->Kill(false, false, false);
        theUnit->mPlayer->KillUnit(theUnit, false);
        return 0;
    }
    case 8:
        ee_combat_regions_footprint(::gWorld.mMap->mUnitRegions, inX, inY, (int32_t)inC, (uint64_t)(uintptr_t)theComplex, inB != 0);
        return 0;
    case 9: return gLoadingSavedGame;
    case 10:
        UUEvent(UUId(theUnit), inX, inY);   // TEST ONLY (UUTL)
        theComplex->StepAside((EEPlayer*)(uintptr_t)inB, (EEUnitClass*)(uintptr_t)inC, inX, inY, inF != 0.0f);
        return 0;
    case 11: theComplex->CanTake(static_cast<EEComplexUnit*>(UUUnit(inB)), (unsigned long)inX); return 0;
    case 12: return UUHandle(static_cast<EEUnit*>(theComplex->mLastResource));
    case 13: {
        EEEffectHandle theHandle;
        ::gWorld.mEffectManager->CreateUnitEffect(theComplex, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), &theHandle, 1, 0, 0);
        return 0;
    }
    case 14: {   // the deposit (inA the container, inB the unit, inC near, inX the amount, inY the carried load, inF the bonus)
        EEComplexUnit* theCarrier = static_cast<EEComplexUnit*>(UUUnit(inB));
        UUEvent(inX, (long)inC, 3);   // TEST ONLY (UUTL)
        theComplex->mPlayer->AddResource(theCarrier->mResourceType, inX);
        // TEST ONLY (docs/AI_RESEARCH.md §8.51, EE_AI_TRACE with EE_AI_EXP deposits): each deposit of a computer player,
        // to time a gatherer's loads (trace only: nothing here changes the game)
        if (gCPTraceOn) {
            EEComputerPlayer* theComputer = dynamic_cast<EEComputerPlayer*>(theComplex->mPlayer);
            if (theComputer != NULL && EECPExperiment(theComputer, "deposits"))
                EE_CP_TRACE(theComputer, "deposit", "unit=%lu cls=%ld res=%lu carried=%ld amount=%ld bonus=%.3f store=%lu t=%lu",
                    (unsigned long)theCarrier->GetUnitID(), (long)theCarrier->mUnitClass->mUnitType, theCarrier->mResourceType, (long)inY,
                    (long)inX, (double)inF, (unsigned long)theComplex->GetUnitID(), (unsigned long)gPhysics->mGameTime);
        }
        return 0;
    }
    case 15: theComplex->mPlayer->mSavedGroups.Reset(inX, inY); return 0;
    case 17: {   // EEEditorCanPlace's transport under the mouse (inA the player, inX the unit type)
        EEUnitClass* theClass = gUnitClassTable[inX];
        long theID = gUIManager->mScenarioEditorForm->GetUnitScreen()->GetUnitUnderMouse();
        EEUnit* theCarrier = theID != -1 ? static_cast<EEUnit*>(::gWorld.GetUnit(theID)) : NULL;
        EEComplexUnit* theTransport = theCarrier != NULL ? theCarrier->GetComplexUnit() : NULL;
        return theTransport != NULL && theTransport->GetPlayer() == (EEPlayer*)(uintptr_t)inA && theCarrier->mUnitClass->mTransportCapacity > 0
            && theTransport->mCargoRoom >= theClass->mWeight && theClass->CanEnter(theTransport->mUnitClass);
    }
    }
    return 0;
}

extern "C" float ee_units_uu_host_world_f(int32_t inWhich, uint64_t inA, uint64_t inB, float inF)
{
    EEUnit* theUnit = UUUnit(inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    switch (inWhich) {
    case 1: return EEIsInGatherReach(inF, theComplex, UUUnit(inB)) ? 1.0f : 0.0f;
    case 2: {   // EEIsTileTopAbove's wall rise
        GE3DPoint thePoint;
        if (theUnit->mUnitClass->mCategory != kUnitCategoryGate)
            theUnit->mComplexUnit->mPhysicalModel.GetLocation().Apply(
                theUnit->GetModel()->mModels[theUnit->mComplexUnit->GetAnimation()]->GetBoundingBox(
                    theUnit->mComplexUnit->mPhysicalModel.GetFrame()).GetTopRightBack(), thePoint);
        else
            thePoint.SetZ(1.2f);
        float theRise = thePoint.GetZ() * theUnit->mStats->GetScale();
        if (!theUnit->mComplexUnit->mIsBuilt)
            theRise = ((float)theUnit->mComplexUnit->mHitPoints / (float)theUnit->mComplexUnit->mStats->mMaxHitPoints) * theRise;
        return theRise;
    }
    case 3: return theComplex->DistanceSquaredToUnit(UUUnit(inB));
    case 4: return ::gWorld.GetGameVariant()->mDepositFactor;
    }
    return 0.0f;
}



extern "C" uint8_t ee_units_uu_host_find_path(uint64_t inUnit, const int32_t* inFrom, const int32_t* inTo, const int32_t* inBounds,
                                              uint32_t inLimit, uint8_t inWalls, int32_t* outFirst)
{
    std::vector<U2DPoint<long> > thePath;
    EEAIAStarBounds theBounds;
    EEAIBPGModifierWallsDoNotBlock* theModifier = inWalls ? new EEAIBPGModifierWallsDoNotBlock() : NULL;
    theBounds.mLeft = inBounds[0];
    theBounds.mTop = inBounds[1];
    theBounds.mRight = inBounds[2];
    theBounds.mBottom = inBounds[3];
    gPathManager.FindPath(static_cast<EEComplexUnit*>(UUUnit(inUnit)), ::gWorld.GetMap(), U2DPoint<long>(inFrom[0], inFrom[1]),
                          U2DPoint<long>(inTo[0], inTo[1]), theBounds, thePath, 0xb, inLimit, theModifier, NULL);
    if (theModifier != NULL)
        theModifier->Dereference();
    if (thePath.size() == 0)
        return 0;
    outFirst[0] = (int32_t)thePath[0].mX;
    outFirst[1] = (int32_t)thePath[0].mY;
    return 1;
}

extern "C" uint32_t ee_units_uu_host_tiles_len(const void* inTiles) { return (uint32_t)((const std::vector<U2DPoint<long> >*)inTiles)->size(); }
extern "C" void ee_units_uu_host_tiles_get(const void* inTiles, int32_t (*outTiles)[2])
{
    const std::vector<U2DPoint<long> >& theTiles = *(const std::vector<U2DPoint<long> >*)inTiles;
    UUPairs(theTiles, outTiles, (uint32_t)theTiles.size());
}
extern "C" void ee_units_uu_host_tiles_set(void* outTiles, const int32_t (*inTiles)[2], uint32_t inCount)
{
    std::vector<U2DPoint<long> >& theTiles = *(std::vector<U2DPoint<long> >*)outTiles;
    theTiles.clear();
    for (uint32_t i = 0; i < inCount; i++)
        theTiles.push_back(U2DPoint<long>(inTiles[i][0], inTiles[i][1]));
}
extern "C" uint32_t ee_units_uu_host_units_len(const void* inUnits) { return (uint32_t)((const std::vector<EEUnit*>*)inUnits)->size(); }
extern "C" void ee_units_uu_host_units_get(const void* inUnits, uint64_t* outUnits)
{
    const std::vector<EEUnit*>& theUnits = *(const std::vector<EEUnit*>*)inUnits;
    for (size_t i = 0; i < theUnits.size(); i++)
        outUnits[i] = UUHandle(theUnits[i]);
}
extern "C" void ee_units_uu_host_units_set(void* outUnits, const uint64_t* inUnits, uint32_t inCount)
{
    std::vector<EEUnit*>& theUnits = *(std::vector<EEUnit*>*)outUnits;
    theUnits.clear();
    for (uint32_t i = 0; i < inCount; i++)
        theUnits.push_back(UUUnit(inUnits[i]));
}

bool EEFOFindNewTargetScreenBase::Virtual04(class EEComplexUnit *)
{
    return true;
}

bool EEFOFindNewTargetScreenBase::Virtual08(class EEComplexUnit *)
{
    return true;
}

// Makes the units on the footprint of a unit of inClass placed at inTile (or of inUnit) step aside for it
void EEPlaceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, const U2DPoint<long>& inTile, bool inArg)
{
    EETileCoordinate theTile(inTile);
    EEStepAsideFromSite(inUnit, inClass, inPlayer, theTile, inArg);
}
