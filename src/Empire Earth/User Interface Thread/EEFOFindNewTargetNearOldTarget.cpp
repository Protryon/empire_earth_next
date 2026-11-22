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
#include "ee/EECPASGFlags.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPathManager.h"
#include "rust/sim/astar.h"   // the path goals' modifiers (modifier_obj.rs)
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
// a float's bits and back (uu_body.rs passes floats as their bits)
static int64_t UbBits(float inValue) { uint32_t theBits; memcpy(&theBits, &inValue, 4); return (int64_t)theBits; }
static float UbFloat(int64_t inBits) { uint32_t theBits = (uint32_t)inBits; float theValue; memcpy(&theValue, &theBits, 4); return theValue; }
template <class T> static T* UbObj(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }
// the first inCapacity tiles of a list into outTiles; their number
static uint32_t UUPairs(const std::vector<U2DPoint<long> >& inTiles, int32_t (*outTiles)[2], uint32_t inCapacity)
{
    for (uint32_t i = 0; i < inTiles.size() && i < inCapacity; i++) {
        outTiles[i][0] = (int32_t)inTiles[i].mX;
        outTiles[i][1] = (int32_t)inTiles[i].mY;
    }
    return (uint32_t)inTiles.size();
}

// TEST ONLY: the unit utilities' record (UUTL) is Rust's (ee_units uu_note.rs); the host's writes note their events
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

// (0x63ddc1, EEUUTileNearerToUnit, the firing ring's sort order: ee_sim combat cg_world.rs)
// (0x63de0f, EEUUPointNearerToUnit, EEFindOpenSpot's sort order: ee_units uu_spots.rs find_open_spot)

// (0x63deae, EEUUTileNearerToPoint, EEGetSortedRingTiles' sort order: ee_units uu.rs sorted_ring_tiles)

// Accepts a tile that is on the map and on the screen's continent and path area (when set)
bool EEFOLocationScreenEnsureContinentAndPathOverlay::Screen(const U2DPoint<long>& inTile)
{
    return ee_units_lf_location_screen(UUHandle(this), inTile.mX, inTile.mY) != 0;   // uu_fill.rs
}


void EEWorkFarm(EEComplexUnit* inTarget, EEComplexUnit* inUnit)
{
    ee_units_ub_body(EE_UNITS_UB_DO_WORK_FARM, UUHandle(inTarget), UUHandle(inUnit), 0, 0, 0);   // uu_body.rs
}


// Shows the wall ghosts for a wall line of a technology and player
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer)
{
    ee_units_ub_body(EE_UNITS_UB_DO_PREVIEW_WALL, UUHandle(&inTiles), 0, inTechnology, inPlayer, 0);   // uu_body.rs
}


// (0x63efa1, EEGetSeabedUnder: ee_units uu_world.rs seabed_under)

// Returns the foundation model a player uses for a building class of its size
EEModel* EEGetPlayerClassGraphic(EEPlayer* inPlayer, EEUnitClass* inClass)
{
    return (EEModel*)(uintptr_t)ee_units_ub_body(EE_UNITS_UB_DO_PLAYER_CLASS_GRAPHIC, UUHandle(inPlayer), UUHandle(inClass), 0, 0, 0);
}

// Computes the launch pitch for a shot at its speed under gravity (low or high arc); false (45 degrees) when
// out of reach
// The launch angle for a shot from inFrom to inTo at inStats' speed under its projectile class's gravity (the low
// arc, or the high one for a class with +0x2e5 or a mortar-like unit); false (and PI/4) when it cannot reach
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo,
                float* outAngle)
{
    return ee_units_lf_ballistic_pitch(UUHandle(inUnit), UUHandle(inStats), (const float(*)[3])&inFrom->mX, (const float(*)[3])&inTo->mX, outAngle) != 0;   // uu_fill.rs
}

// Points a unit's attack goal at a new target and restarts its attack
// inUnit's attack goal (inGoal is an EEUGAttackUnit: 0x241c2d's dynamic_cast) moves on to inTarget, and the unit starts
// its attack action (0x7dd) over
void EERetargetAttack(EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEComplexUnit* inTarget)
{
    ee_units_ub_body(EE_UNITS_UB_DO_RETARGET_ATTACK, UUHandle(inUnit), UUHandle(inGoal), (int64_t)UUHandle(inTarget), 0, 0);   // uu_body.rs
}

// Finds a unit a new target near its old one: another piece of a linked (wall/gate) target, else the
// nearest living enemy within 5 tiles
void EEFindNewTargetNearOld(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FIND_NEW_TARGET_NEAR_OLD, UUHandle(inUnit), UUHandle(inTarget), 0, 0, 0);   // uu_body.rs
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

// Computes a moving unit's Doppler pitch shift against the camera
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    float theCamera[3] = {::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(), ::gWorld.mTransformation.GetZ()};
    ee_units_uu_doppler_pitch(UUHandle(inUnit), *outVolume, theUnit->mVelocity, (const float(*)[4][4])&theUnit->mPhysicalModel.GetLocation().mMatrix[0][0],
                              &theCamera, outPitch);   // ee_units uu_world.rs doppler_pitch
}


// (0x642489, EEGetUnitAngle, and the escape point's sort by it, EEUUAngleLess: ee_units uu_world.rs unit_angle)

// Tells whether a priest can convert the target
bool EECanPriestConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_CAN_PRIEST_CONVERT, UUHandle(inUnit), UUHandle(inTarget), 0, 0, 0) != 0;
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

// (The target search of 0x243d43, EEUUTargetSearch with its filter 0x24ffca and scoring 0x25000c, and the tile
// searches 0x24fcd1 / 0x2503d7, EEUUSearchTiles: ee_combat's search_target, search.rs; its constructor 0x643e0a is
// Search::new there. The world it asks is below.)

// ---- the target searches (EESearchTarget, EESearchTargetAlongList, EEGetPursuerOf, EEFindNewTarget): their bodies,
// tests, inputs and CBST/CBSL/CBPU/CBFT records are ee_sim units uu_search.rs over ee_combat's walks; each C++
// statement they ask is one EE_UNITS_SR_* case here. Handles are the units' pointers.

static EEUnit* EESrUnit(uint64_t inHandle) { return (EEUnit*)(uintptr_t)inHandle; }
static EEComplexUnit* EESrComplex(uint64_t inHandle) { return (EEComplexUnit*)(uintptr_t)inHandle; }

// EEIsLineToUnitBlocked's outputs for a search (ee_sim units uu_search.rs EeUnitsSrLine)
struct EESrLine {
    EEUnit* mBlocker;
    long mX;
    long mY;
};

extern "C" int64_t ee_units_sr_host(int32_t inOp, uint64_t inUnit, uint64_t inA, int64_t inC, float (*ioF)[4])
{
    EEComplexUnit* theUnit = EESrComplex(inUnit);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_SR_SAME_PLAYER: return EESrUnit(inA)->mPlayer == theUnit->mPlayer;
    case EE_UNITS_SR_ENEMY: return theUnit->mPlayer->mEnemies[EESrUnit(inA)->mPlayer->GetPlayerID()] != 0;
    case EE_UNITS_SR_HIT_POINTS: return EESrComplex(inA)->mHitPoints;
    case EE_UNITS_SR_DISABLED: return EESrComplex(inA)->mIsDisabled;
    case EE_UNITS_SR_PROTECTED: return EESrComplex(inA)->IsProtected();
    case EE_UNITS_SR_ATTACKED_BY: return EECombatWorld(EE_COMBAT_WORLD_ATTACKED_BY, theUnit, EESrUnit(inA)) != 0;
    case EE_UNITS_SR_CLASS: return UUHandle(EESrUnit(inA)->GetUnitClass());
    case EE_UNITS_SR_IS_FOUNDATION: return EESrComplex(inA)->IsFoundation();
    case EE_UNITS_SR_SEEN: return EECombatIsSeenBy(EESrComplex(inA), theUnit->GetPlayer(), true);
    case EE_UNITS_SR_OTHER_CONTINENT: return theUnit->IsOnOtherContinentThan(EESrComplex(inA));
    case EE_UNITS_SR_PART_LEFT_OUT: return EESrUnit(inA)->GetUnitClass()->IsPartLeftOut(EESrComplex((uint64_t)inC));
    case EE_UNITS_SR_CLASS_PRIEST: return UbObj<EEUnitClass>(inA)->IsPriest();
    case EE_UNITS_SR_CAN_PRIEST_CONVERT: return EECanPriestConvert(theUnit, EESrComplex(inA));
    case EE_UNITS_SR_CAN_ATTACK_CLASS: return theUnit->CanAttackClass(UbObj<EEUnitClass>(inA));
    case EE_UNITS_SR_IGNORED_COUNT: return (int64_t)theUnit->mIgnoredTargets.size();
    case EE_UNITS_SR_IS_IGNORED: return theUnit->IsTargetIgnored(EESrComplex(inA), inC != 0);
    case EE_UNITS_SR_GOAL: return UUHandle(EESrComplex(inA)->GetGoal());
    case EE_UNITS_SR_ATTACK_CAST: return UUHandle(EEAttackUnitCast(UbObj<EEUnitGoal>(inA)));
    case EE_UNITS_SR_ATTACK_TARGET_COMPLEX: return UUHandle(UbObj<EEUGRust>(inA)->GetAttackTarget()->mComplexUnit);
    case EE_UNITS_SR_TECH_NODE: return UUHandle(EESrUnit(inA)->mPlayer->mTechnologies->Get(EESrUnit(inA)->GetUnitClass()->mTechnology));
    case EE_UNITS_SR_NODE_KIND: return UbObj<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_SR_LAUNCHER: return EESrComplex(inA)->mLauncherClass != NULL;
    case EE_UNITS_SR_GOAL_STATE: return UUHandle(EESrComplex(inA)->mGoalState);
    case EE_UNITS_SR_GATHER_CAST: return UUHandle(EEGatherGoalCast(UbObj<EEUnitGoal>(inA)));
    case EE_UNITS_SR_GATHER_RESOURCE: return UUHandle(UbObj<EEUGRust>(inA)->GetGatherResource());
    case EE_UNITS_SR_COMPLEX: return UUHandle(EESrUnit(inA)->mComplexUnit);
    case EE_UNITS_SR_LIST_NEXT: return UUHandle(static_cast<EEUnit*>(EESrUnit(inA)->mTileNode->mNext));
    case EE_UNITS_SR_LIST_PREV: return UUHandle(static_cast<EEUnit*>(EESrUnit(inA)->mTileNode->mPrev));
    case EE_UNITS_SR_DUMMY_HEAD: return RTSDummyHead(EESrUnit(inA)) != NULL;
    case EE_UNITS_SR_HIT_DISTANCE: f[0] = EECombatHitDistanceSquared(theUnit, EESrUnit(inA), f[0], f[1]); return 0;
    case EE_UNITS_SR_ATTACK_FACTOR: f[0] = gToHitTable.GetAttackFactor(theUnit->GetUnitClass()->mAmmoType, EESrUnit(inA)->GetUnitClass(), theUnit->mPlayer->GetPlayerID(), EESrUnit(inA)->mPlayer->GetPlayerID()); return 0;
    case EE_UNITS_SR_LINE_BLOCKED: return EEIsLineToUnitBlocked(theUnit, EESrUnit(inA), &UbObj<EESrLine>(inC)->mBlocker, &UbObj<EESrLine>(inC)->mX, &UbObj<EESrLine>(inC)->mY);
    case EE_UNITS_SR_REACHABLE: return EEIsAttackReachable(theUnit, EESrComplex(inA), UbObj<U2DPoint<long> >(inC));
    case EE_UNITS_SR_IGNORE: theUnit->IgnoreTarget(EESrComplex(inA)); return 0;
    case EE_UNITS_SR_FOOTPRINT_TILE: theUnit->GetNearestFootprintTile(EESrUnit(inA), UbObj<long>(inC), UbObj<long>(inC) + 1); return 0;
    case EE_UNITS_SR_UNIT_ID: return (int64_t)EESrUnit(inA)->GetUnitID();
    case EE_UNITS_SR_ATTACK: f[0] = theUnit->mStats->GetAttack(); return 0;
    case EE_UNITS_SR_LINE_OF_SIGHT: f[0] = theUnit->GetLineOfSight(); return 0;
    case EE_UNITS_SR_MAX_VELOCITY: f[0] = theUnit->mStats->GetMaxVelocity(); return 0;
    case EE_UNITS_SR_RANGE: f[0] = theUnit->mStats->GetRange(); return 0;
    case EE_UNITS_SR_MIN_RANGE: f[0] = theUnit->mUnitClass->mMinRange; return 0;
    case EE_UNITS_SR_MELEE: return theUnit->mUnitClass->mIsMelee;
    case EE_UNITS_SR_SHOOTS_HIGH: return theUnit->mUnitClass->mShootsHigh;
    case EE_UNITS_SR_OVER_WALLS: return theUnit->mStats->mCanFireOverWalls;
    case EE_UNITS_SR_TILE_X: return theUnit->mTileX;
    case EE_UNITS_SR_TILE_Y: return theUnit->mTileY;
    case EE_UNITS_SR_TICK_RANDOM: return (int32_t)gPhysics->mTickRandom;
    case EE_UNITS_SR_POSITION_X: f[0] = theUnit->mPhysicalModel.GetLocation().GetX(); return 0;
    case EE_UNITS_SR_POSITION_Y: f[0] = theUnit->mPhysicalModel.GetLocation().GetY(); return 0;
    case EE_UNITS_SR_UNIT_REGIONS: return UUHandle(gWorld.mMap->mUnitRegions);
    case EE_UNITS_SR_SCREEN: return UbObj<EEFOFindNewTargetScreenBase>(inA)->Virtual08(EESrComplex((uint64_t)inC));
    case EE_UNITS_SR_RETARGET: EERetargetAttack(theUnit, UbObj<EEUnitGoal>(inA), EESrComplex((uint64_t)inC)); return 0;
    case EE_UNITS_SR_OWN_TECHNOLOGY: return UUHandle(theUnit->GetPlayer()->GetTechTree()->Get(EESrUnit(inA)->GetUnitClass()->GetTechnology()));
    case EE_UNITS_SR_TECHNOLOGY_KIND: return UbObj<EETechnology>(inA)->mData->mKind;
    }
    return 0;
}

// (0x250806, EEUUGetUnitsInArea, the units of an area's unit regions that a screen passes: ee_combat's units_in_area,
// called from EEFindNewTarget's body)

// Returns the best target for a unit around a tile, scored by a target search
// The best target for inUnit around (inX, inY) (the tile search 0x24fcd1, or 0x2503d7 from its position for a
// class with +0x318), scored by an EEUUTargetSearch (uu_search.rs over ee_combat's search_target)
EEUnit* EESearchTarget(EEComplexUnit* inUnit, U2DRectangle<long>* inArea, long inX, long inY, float inScale)
{
    return UUUnit(ee_units_sr_search_target(UUHandle(inUnit), inArea->mLeft, inArea->mTop, inArea->mRight, inArea->mBottom, inX, inY, inScale));
}

// Returns a target near a unit (along the unit list for anti-air ammunition 0x3c)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_FIND_NEARBY_TARGET, UUHandle(inUnit), 0, 0, 0, 0));   // uu_body.rs
}

// Returns the best target for a unit around a tile along the x-sorted unit list, scored by its targeting table
// The best target for inUnit around (inX, inY) along the x-sorted unit list (both ways), scored by the targeting
// table row of its mode (the unit-list twin of EEUUTargetSearch) (uu_search.rs over ee_combat's search_along_list)
EEUnit* EESearchTargetAlongList(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    return UUUnit(ee_units_sr_search_along_list(UUHandle(static_cast<EEUnit*>(inUnit)), inX, inY, inScale));   // uu_search.rs
}

// Returns the best target for a unit around a tile when the influence table shows enemies within its reach
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_FIND_TARGET_NEAR, UUHandle(inUnit), 0, inX, inY, UbBits(inScale)));   // uu_body.rs
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
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_MAKE_BUILDING, UUHandle(inPlayer), 0, inUnitType, inX, inY));   // uu_body.rs
}

// Replaces a queued technology on every living unit of a player whose class makes it
void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology)
{
    ee_units_ub_body(EE_UNITS_UB_DO_REPLACE_QUEUED_TECHNOLOGY, UUHandle(inPlayer), UUHandle(inClass), inNewTechnology, 0, 0);
}

// Replaces a unit with a new one of another class (pack/unpack): hit points in proportion, goals, groups
// and effects moved over
// inUnit replaced by a new unit of inClass for inPlayer (EEMPackUnpack): hit points in proportion, the goals, groups
// and effects moved over
EEComplexUnit* EEReplaceUnit(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_REPLACE_UNIT, UUHandle(inUnit), UUHandle(inClass), (int64_t)inPlayer, 0, 0)));
}

// Creates a copy of a unit (or a new class) for a player on its tile, in its state
EEUnit* EEMakeUnitCopy(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_MAKE_UNIT_COPY, UUHandle(inUnit), UUHandle(inClass), (int64_t)inPlayer, 0, 0));   // uu_body.rs
}


// (0x6450f1, EEClearUnderBuilding: ee_units uu_world.rs clear_under_building)

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

// Finds the nearest free farm of the unit's player, ring by ring (the ring reached goes back)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioRing)
{
    int32_t theRing = (int32_t)*ioRing;
    EEComplexUnit* theFarm = static_cast<EEComplexUnit*>(UUUnit(ee_units_uu_find_free_farm(UUHandle(static_cast<EEUnit*>(inUnit)), inFreeOnly, &theRing)));
    *ioRing = theRing;
    return theFarm;
}

// Returns the nearest unit that attacks or gathers from the unit
// (uu_search.rs over ee_combat's pursuer_of)
EEComplexUnit* EEGetPursuerOf(EEComplexUnit* inUnit)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_sr_pursuer_of(UUHandle(static_cast<EEUnit*>(inUnit)))));   // uu_search.rs
}

// Gives a unit's goal the first enemy near a tile that passes a screen; false when none
// (uu_search.rs over ee_combat's units_in_area and choose_target)
bool EEFindNewTarget(EEFOFindNewTargetScreenBase* inScreen, EEUnitGoal* inGoal, EEComplexUnit* inUnit,
                EETileCoordinate* inTile, long inRadius, bool inArg6, long inArg7)
{
    return ee_units_sr_find_new_target(UUHandle(inScreen), UUHandle(inGoal), UUHandle(inUnit), inTile->GetX(), inTile->GetY(), inRadius, inArg6, inArg7) != 0;
}

// Runs a functor over the units of every player but nature
void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FOR_EACH_PLAYERS_UNIT, UUHandle(inFunctor), 0, 0, 0, 0);   // uu_body.rs
}

// Runs a functor over a player's units (and its pending units when asked)
void EEForEachUnitOf(EEPlayer* inPlayer, EEUUUnitFunctor* inFunctor, bool inCalamities)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FOR_EACH_UNIT_OF, UUHandle(inPlayer), UUHandle(inFunctor), inCalamities, 0, 0);   // uu_body.rs
}

// Returns the world position of a unit's current attach point; false (half a tile up) without one
// The world position of inUnit's current tag point (its model part's list, the unit's frame), scaled by its size;
// false (and the point half a tile up) when the part has no tag points (new: ee_combat's attach_point)
bool EEApproachAttachPoint(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    return ee_units_lf_attach_point(UUHandle(inUnit), (float(*)[3])&outPoint.mX) != 0;   // uu_fill.rs
}

// (the EETileCoordinate twin of 0x64697b is gone: one tile type)

// Tells whether a unit can heal a target (wounded enough, not an enemy, in its heal set)
// (new: the decision is ee_combat's can_heal over a record of pure reads; the unit set is looked up only for a set
// in the manager's range, which the original indexed unchecked)
bool EECanHeal(EEUnit* inUnit, EEComplexUnit* inTarget)
{
    return ee_units_lf_can_heal(UUHandle(inUnit), UUHandle(inTarget)) != 0;   // uu_fill.rs
}

// Returns a unit around the given one whose gather goal works on it (searched in a random rotation)
EEComplexUnit* EEGetWaitingGatherer(EEUnit* inUnit)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_WAITING_GATHERER, UUHandle(inUnit), 0, 0, 0, 0)));
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


// (0x647125, EEGetEnemiesAround: ee_units uu_world.rs enemies_around)

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

// Tells whether a unit can hit a target from its attach point (a clear line to the drop location)
bool EECanBallisticHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_CAN_BALLISTIC_HIT, UUHandle(inUnit), UUHandle(inTarget), 0, 0, 0) != 0;   // uu_body.rs
}

// Tells whether a unit touches a target (collision spheres; walls count as radius 1)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared)
{
    long theBits = 0;   // ee_combat leaves.rs in_contact_range: the distance's bits as the tile's x
    memcpy(&theBits, &inDistanceSquared, 4);
    return EECombatWorld(EE_COMBAT_WORLD_CONTACT_RANGE, inUnit, inTarget, NULL, &theBits, &theBits) != 0;
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

// Returns a random number in [min, max]
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom)
{
    long theEnd = inMax + 1;
    long theResult = UMFloatToLong(inRandom.Generate() * (theEnd - inMin) + inMin);
    if (theResult == theEnd)
        theResult = inMax;
    return theResult;
}

// Returns where a unit casts its calamity from: 90% of the calamity's range from the target
void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    ee_units_ub_body(EE_UNITS_UB_DO_HELICOPTER_PRIEST_DESTINATION, UUHandle(inUnit), UUHandle(&outPoint), 0, 0, 0);
}

// Collects the tiles of a line, walking in from both ends (optionally the end tiles and 4-connected)
// The tiles of the line (inX1, inY1)-(inX2, inY2): a symmetric double-step rasteriser that walks in from both ends
// (inEnds: the two end tiles too; !inThin: the extra tiles that make the line 4-connected), in order from (inX1, inY1)
// (new: the rasteriser is ee_combat's tile_line)
void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<U2DPoint<long> >* outTiles, bool inEnds,
                bool inThin)
{
    ee_units_lf_tile_line((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2, inEnds, inThin, outTiles);   // uu_line.rs
}

// (the U2DPoint<long> twin of 0x64b556 is gone: one tile type)

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



// Returns the tile two tiles away in a direction (1..8)
void EEGetSlotTile(const EETileCoordinate& inTile, long inDirection, EETileCoordinate& outTile)
{
    int32_t theTile[2];
    ee_units_uu_slot_tile(inTile.mX, inTile.mY, inDirection, &theTile);
    outTile.mX = theTile[0];
    outTile.mY = theTile[1];
}

// Picks the wall piece (straight, corner or slope) for every tile of a wall line from its neighbours and the
// terrain heights
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap)
{
    std::vector<int32_t> theCorners(inTiles.size() + 1);
    ee_units_uu_pick_wall_pieces(UUHandle(inMap), &inTiles, &theCorners[0]);
    outCorners.assign(theCorners.begin(), theCorners.end() - 1);
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
// With EE_SIM_CAPTURE, a CBLB record: the call's units and tiles, the result, the last tile, the blocker (uu_line.rs)
bool EEIsLineToUnitBlocked(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5)
{
    return ee_units_lf_line_blocked(UUHandle(inUnit), UUHandle(inTarget), (uint64_t*)outArg3, outArg4, outArg5) != 0;   // uu_line.rs
}

// The line of fire's statements (ee_sim units uu_line.rs: the walk is ee_combat's line_blocked, the shot's start
// ee_combat's weapon_height over the weapon tag point's data gathered here, CBWP and CBLB records there)
extern "C" int64_t ee_units_lf_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float (*ioF)[4])
{
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_LF_TILE_X: return UUUnit(inA)->GetTileX();
    case EE_UNITS_LF_TILE_Y: return UUUnit(inA)->GetTileY();
    case EE_UNITS_LF_SKIPS_CHECK: return UUUnit(inA)->GetUnitClass()->mSkipsLineOfFireCheck;
    case EE_UNITS_LF_HIT_POINTS: return UUUnit(inA)->GetHitPoints();
    case EE_UNITS_LF_MELEE: return UUUnit(inA)->GetUnitClass()->mIsMelee;
    case EE_UNITS_LF_UNIT_ID: return (int64_t)UUUnit(inA)->GetUnitID();
    case EE_UNITS_LF_COMPLEX: return UUHandle(UUUnit(inA)->GetComplexUnit());
    case EE_UNITS_LF_LOCATION: memcpy(UbObj<float>(inC), UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_LF_MODEL: return UUHandle(UUUnit(inA)->GetModel());
    case EE_UNITS_LF_TAG_LIST: return UUHandle(UbObj<EEModel>(inA)->mTagPoints[UbObj<EEComplexUnit>(inB)->mAnimation]);
    case EE_UNITS_LF_FRAME: return (int64_t)UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetFrame();
    case EE_UNITS_LF_WEAPON_INDEX: return (int64_t)UbObj<EEComplexUnit>(inA)->mWeaponTagPoint;
    case EE_UNITS_LF_SCALE: f[0] = UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetScale(); return 0;
    case EE_UNITS_LF_NUM_PARTS: return (int64_t)UbObj<EEModel>(inA)->GetNumParts();
    case EE_UNITS_LF_FIND_TURRET: return UbObj<EEModel>(inA)->FindTurretPart(UbObj<EETagPointList>(inB)->GetTagPoint((unsigned long)inC)->mModel, *UbObj<unsigned long>(inD));
    case EE_UNITS_LF_TURRET_ANGLE: f[0] = UbObj<EEComplexUnit>(inA)->mTurretAngles[inC]; return 0;
    case EE_UNITS_LF_LIST_POINT_AT: return UUHandle(*(UbObj<EETagPointList>(inA)->mPoints.begin() + inC));
    case EE_UNITS_LF_LIST_SIZE: return (int64_t)UbObj<EETagPointList>(inA)->mPoints.size();
    case EE_UNITS_LF_LIST_TAG_POINT: return UUHandle(UbObj<EETagPointList>(inA)->GetTagPoint((unsigned long)inC));
    case EE_UNITS_LF_BOX_TOP: *UbObj<GE3DPoint>(inD) = UbObj<EEModel>(inA)->mModels[UbObj<EEComplexUnit>(inB)->mAnimation]->GetBoundingBox((unsigned long)inC).GetTopRightBack(); return 0;
    case EE_UNITS_LF_POINT_LOCAL: *UbObj<GE3DPoint>(inD) = UbObj<EETagPoint>(inA)->mModel->GetTagPoint(UbObj<EETagPoint>(inA)->mIndex, (unsigned long)inC); return 0;
    case EE_UNITS_LF_POINT_MODEL: return UUHandle(UbObj<EETagPoint>(inA)->mModel);
    case EE_UNITS_LF_INTERNAL_TRANSFORM: memcpy(UbObj<float>(inD), UbObj<GEModel>(inA)->GetInternalTransform((unsigned long)inC).mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_LF_PARENT: return UUHandle(UbObj<GEModel>(inA)->GetParentModel());
    case EE_UNITS_LF_STATS_SCALE: f[0] = UUUnit(inA)->mStats->GetScale(); return 0;
    case EE_UNITS_LF_BUILT: return UbObj<EEComplexUnit>(inA)->mIsBuilt;
    case EE_UNITS_LF_MAX_HIT_POINTS: return UbObj<EEComplexUnit>(inA)->mStats->mMaxHitPoints;
    case EE_UNITS_LF_Z: f[0] = UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().GetZ(); return 0;
    // uu_fill.rs's
    case EE_UNITS_LF_LAUNCH_SPEED: f[0] = UbObj<EEUnitClassUpgrade>(inA)->mLaunchSpeed; return 0;
    case EE_UNITS_LF_PROJECTILE_ACCELERATION: f[0] = UbObj<EEUnitClassUpgrade>(inA)->mUnitClass->mProjectileClass->GetAcceleration(); return 0;
    case EE_UNITS_LF_SHOOTS_HIGH: return UbObj<EEComplexUnit>(inA)->mUnitClass->mShootsHigh;
    case EE_UNITS_LF_STATS_OVER_WALLS: return UbObj<EEUnitClassUpgrade>(inA)->mCanFireOverWalls;
    case EE_UNITS_LF_FIRING_HIGH: return UbObj<EEComplexUnit>(inA)->mIsFiringHigh;
    case EE_UNITS_LF_ANIMATION: return (int64_t)UbObj<EEComplexUnit>(inA)->mAnimation;
    case EE_UNITS_LF_WONDER_SET: return (int64_t)UUUnit(inA)->mStats->mWonderUnitSet;
    case EE_UNITS_LF_REPAIRABLE: return UbObj<EEComplexUnit>(inA)->IsRepairable();
    case EE_UNITS_LF_HEAL_THRESHOLD: f[0] = UUUnit(inA)->mUnitClass->GetHealThreshold(); return 0;
    case EE_UNITS_LF_PLAYER: return UUHandle(UUUnit(inA)->mPlayer);
    case EE_UNITS_LF_ALLIED: return UUUnit(inA)->mPlayer->mEnemies[UUUnit(inB)->mPlayer->GetPlayerID()] == 0;
    case EE_UNITS_LF_SET_COUNT: return (int64_t)gUnitSetManager.GetSize();
    case EE_UNITS_LF_SET_CONTAINS: return gUnitSetManager.Contains(UbObj<EEComplexUnit>(inA)->mUnitClass->mUnitType, (unsigned long)inC);
    case EE_UNITS_LF_SOUND_FRAME0: return (int64_t)UUUnit(inA)->GetModel()->mSound1Frames[0];
    case EE_UNITS_LF_TAG_POINT_COUNT: return (int64_t)UbObj<EETagPointList>(inA)->GetNumTagPoints();
    case EE_UNITS_LF_SCREEN_ON_MAP: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mB08;
    case EE_UNITS_LF_TILE_AT_LOCATION: return ::gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)inC, (long)inD)) != NULL;
    case EE_UNITS_LF_SCREEN_CONTINENT: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mContinentID;
    case EE_UNITS_LF_MAP_WIDTH: return (int64_t)gMapWidth;
    case EE_UNITS_LF_MAP_HEIGHT: return (int64_t)gMapHeight;
    case EE_UNITS_LF_CONTINENT_ID: return gContinentManager.GetContinentID((long)inC, (long)inD);
    case EE_UNITS_LF_SCREEN_AREA: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mAreaID;
    case EE_UNITS_LF_SCREEN_WATER: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mUseWaterAreas;
    case EE_UNITS_LF_CONTINENT_OF: return gContinentManager.GetContinent(U2DPoint<long>((long)inC, (long)inD), (long)inB);
    }
    return 0;
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
    ee_units_ub_body(EE_UNITS_UB_DO_UPDATE_UNIT_TILE, UUHandle(inTransformation), UUHandle(inUnit), (int64_t)UUHandle(outX), (int64_t)UUHandle(outY), 0);
}

// Builds the text of a civilization upgrade ("category: attribute percent%"); false when a string is missing
// The text of civilization upgrade inUpgrade ("<category>: <attribute> <percent>%"); false when a string is missing.
// Its two function-local static strings' atexit dtors follow it (0x24e7c4 / 0x24e7cf).
bool EEFn24E68F(EECivUpgrade inUpgrade, UWideString& outText)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_UPGRADE_TEXT, 0, UUHandle(&outText), (int64_t)inUpgrade, 0, 0) != 0;   // uu_body.rs
}

// Converts a unit to another player: a converted priest may hand over a civ power or upgrade, the class
// follows the new owner, the new unit keeps hit points, actions and groups
// inUnit converted to player inPlayer (the priests, the scenario effect): a converted priest or prophet may hand over
// a civilization power (or a random upgrade) the new owner lacks; the class follows the new owner's powers; the new
// unit takes over its hit point share, actions and groups; inArg3 counts it in the statistics
EEComplexUnit* EEConvertUnit(EEComplexUnit* inUnit, long inPlayer, bool inIsConversion)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_CONVERT_UNIT, UUHandle(inUnit), 0, inPlayer, inIsConversion, 0)));
}

// Tells whether a civilization bought an upgrade
// Whether inValue is in inCivilization's mPurchasedUpgrades (read through the out-of-line GetPurchasedUpgrades 0x12c738, ICF with EECampaign::GetScenarios)
bool EEHasCivilizationUpgrade(EECivilization* inCivilization, EECivUpgrade inValue)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_HAS_CIVILIZATION_UPGRADE, UUHandle(inCivilization), 0, (int64_t)inValue, 0, 0) != 0;
}

// Moves a container's cargo into the unit's care (also placing it on the map for a conversion)
void EEMovePassengers(EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIsConversion)
{
    ee_units_ub_body(EE_UNITS_UB_DO_MOVE_PASSENGERS, UUHandle(inUnit), UUHandle(inContainer), inIsConversion, 0, 0);   // uu_body.rs
}

// Gives a unit another unit's load (as much as its class allows), emptying the other
void EETakeOverLoad(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    ee_units_ub_body(EE_UNITS_UB_DO_TAKE_OVER_LOAD, UUHandle(inUnit), UUHandle(inOther), 0, 0, 0);   // uu_body.rs
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
    ee_units_ub_body(EE_UNITS_UB_DO_UPGRADE_HIT_POINTS, UUHandle(inUpgrade), UUHandle(inPlayer), inType, UbBits(inFactor), 0);
}

// Scales the hit points of a living unit of the upgraded type by the ratio (at least 1)
void EEHitPointUpgrader::Apply(EEComplexUnit* inUnit)
{
    ee_units_ub_body(EE_UNITS_UB_DO_HIT_POINT_APPLY, UUHandle(static_cast<EEUUUnitFunctor*>(this)), UUHandle(inUnit), 0, 0, 0);
}

// Tells whether a unit type can be placed at a location (the editor's own test in free camera mode)
bool EECanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    if (gFreeCamera == true)
        return ee_units_uu_editor_can_place(UUHandle(inPlayer), inType, (const float(*)[3])&inLocation->mX, inX, inY) != 0;
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

// (0x64f6f7, EEIsEditorSpacingClear: ee_units uu_world.rs editor_spacing_clear)

// Gives a space wall its energy wall effect
void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit)
{
    ee_units_ub_body(EE_UNITS_UB_DO_SPACE_ENERGY_WALL, UUHandle(inUnit), 0, 0, 0, 0);   // uu_body.rs
}

// Gives a space gate its energy wall effect
void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit)
{
    ee_units_ub_body(EE_UNITS_UB_DO_SPACE_ENERGY_WALL, UUHandle(inUnit), 0, 1, 0, 0);   // uu_body.rs
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
    // (the drop point, the path area and the gate's ends: ee_sim units uu_world_ops.rs over these statements)
    switch (inWhich) {
    case 1: return theUnit->mUnitClass->IsPartLeftOut(static_cast<EEComplexUnit*>(theUnit));
    case 4: return theUnit->GetHitPoints();
    case 5: return theUnit->mComplexUnit != NULL;
    case 6: return theUnit->GetModel()->mModel != NULL;
    case 7: *(GE3DPoint*)outPoint = theUnit->GetModel()->mTopPoint; return 0;
    case 8: outPoint[0] = theUnit->mComplexUnit->mPhysicalModel.GetScale(); return 0;
    case 9: theUnit->mComplexUnit->mPhysicalModel.GetLocation().Apply(*(GE3DPoint*)outPoint, *(GE3DPoint*)(outPoint + 3)); return 0;
    case 10: theUnit->GetPosition(*(GE3DPoint*)outPoint); return 0;
    case 12: return ((EEAIPathArea*)(uintptr_t)inUnit)->GetID();
    case 13: EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinkedCarrier(static_cast<EEComplexUnit*>(theUnit)->GetOutermostCarrier())->GetPosition(*(GE3DPoint*)outPoint); return 0;
    case 14: EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinked(static_cast<EEComplexUnit*>(theUnit)->GetOutermostCarrier())->GetPosition(*(GE3DPoint*)outPoint); return 0;
    }
    return 0;
}

// the vectors of tiles and units
// ---- the round-2 world (ee_units uu_world.rs over uu_host.rs): the questions and writes of the moved bodies ----

// (EEPlanWall's new piece, UUMakeWallPiece: ee_sim units uu_world_ops.rs make_wall_piece over the cases 43-56 below)

extern "C" uint64_t ee_units_uu_host_world(int32_t inWhich, uint64_t inA, uint64_t inB, uint64_t inC, int32_t inX, int32_t inY, float inF)
{
    EEUnit* theUnit = UUUnit(inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    EEActiveGroup* theGroup = (EEActiveGroup*)(uintptr_t)inA;
    switch (inWhich) {
    // (the build target, the idle restart, a tile's unit, the ambient clearing, the step aside, the editor's
    // transport and the wall piece: ee_sim units uu_world_ops.rs over the cases from 20)
    case 4: return ((EEPlayer*)(uintptr_t)inA)->CanProduce(((EEUnitClass*)(uintptr_t)inB)->GetTechnology(), 0x20, 1, true, 3, NULL);
    case 8:
        ee_combat_regions_footprint(::gWorld.mMap->mUnitRegions, inX, inY, (int32_t)inC, (uint64_t)(uintptr_t)theComplex, inB != 0);
        return 0;
    case 9: return gLoadingSavedGame;
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
    case 18: return EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(theUnit->GetUnitClass())->HasGatherPoint(theComplex, UUUnit(inB));   // (its UCBD note)
    case 20: return (uint64_t)(int64_t)UUId(theUnit);
    case 21: theComplex->AddBuildTarget(UUUnit(inB)); return 0;
    case 22: theComplex->ClearGoals(0); return 0;
    case 23: theComplex->StartAction(kActionIdle); return 0;
    case 24: theComplex->mUnitClass->ActionChanged(theComplex); return 0;
    case 25: UUTile(inA)->mUnits = UUTile(inA)->AddUnit(UUUnit(inB), UUTile(inA)->mUnits); return 0;
    case 26: UUTile(inA)->RemoveUnit(UUUnit(inB)); return 0;
    case 27: return EEUnitClassOfKind<EE_CLASS_KIND_AMBIENT>(theUnit->mUnitClass) != NULL;
    case 28: return (uint64_t)(int64_t)theUnit->mUnitClass->mUnitType;
    case 29: return theUnit->mUnitClass->mIsDeletable;
    case 30: return (uint64_t)(int64_t)theUnit->mResourceAmount;
    case 31: theUnit->mResourceAmount = 0; return 0;
    case 32: theUnit->Kill(false, false, false); return 0;
    case 33: theUnit->mPlayer->KillUnit(theUnit, false); return 0;
    case 34: theComplex->StepAside((EEPlayer*)(uintptr_t)inB, (EEUnitClass*)(uintptr_t)inC, inX, inY, inF != 0.0f); return 0;
    case 35: return (uint64_t)(int64_t)gUIManager->GetScenarioEditorForm()->GetUnitUnderMouse();
    case 36: return UUHandle(static_cast<EEUnit*>(::gWorld.GetUnit(inX)));
    case 37: return UUHandle(theUnit->GetComplexUnit());
    case 38: return UUHandle(theComplex->GetPlayer());
    case 39: return (uint64_t)(int64_t)theUnit->mUnitClass->mTransportCapacity;
    case 40: return (uint64_t)(int64_t)theComplex->mCargoRoom;
    case 41: return (uint64_t)(int64_t)gUnitClassTable[inX]->mWeight;
    case 42: return gUnitClassTable[inX]->CanEnter(theComplex->mUnitClass);
    case 43: return UUHandle(((EEUnitClass*)(uintptr_t)inA)->CreateUnit(inX, inY, ((EEPlayer*)(uintptr_t)inB)->GetPlayerID(), inF != 0.0f, NULL));
    case 44: return (uint64_t)(int64_t)::gWorld.GetMap()->GetNumRaisedCorners(inX, inY);
    case 45: theUnit->SetHitPoints(inX); return 0;
    case 46: return (uint64_t)theUnit->mTile->GetVisibility(((EEPlayer*)(uintptr_t)inB)->GetPlayerID());
    case 47: gDoppelgangerManager.AddForUnseenUnit(theComplex); return 0;
    case 48: ((EEPlayer*)(uintptr_t)inB)->Pay(((EEPlayer*)(uintptr_t)inB)->GetTechTree()->Get(((EEUnitClass*)(uintptr_t)inA)->GetTechnology()), 0x20, 1.0f); return 0;
    case 49: return theComplex->mUnitClass->mIsWall && !theComplex->mUnitClass->mIsTower;
    case 50: theComplex->GetPlayer()->AddConstructionSite(theComplex); return 0;
    case 51: return (uint64_t)(int64_t)theUnit->GetStats()->mMaxHitPoints;
    case 52: theComplex->SetBuilt(1, 0, 1); return 0;
    case 53: return ((EEPlayer*)(uintptr_t)inB)->GetTechTree()->Get(kTechWallSpace)->GetIsAvailable();
    case 54: EECreateSpaceWallEnergyWall(theComplex); return 0;
    case 55: return theComplex->mIsPaidFor;
    case 56: theComplex->mIsPaidFor = true; return 0;
    case 57: return (uint64_t)(int64_t)theUnit->mUnitClass->mCategory;
    case 58: return theUnit->mComplexUnit->mIsBuilt;
    case 59: return UUHandle(gContinentManager.GetPathAreaAt(U2DPoint<long>(inX, inY)));
    }
    return 0;
}

// a wall's model box top at its frame through its location (EEIsTileTopAbove's wall rise)
static GE3DPoint UUBoxTop(EEUnit* inUnit)
{
    GE3DPoint thePoint;
    inUnit->mComplexUnit->mPhysicalModel.GetLocation().Apply(inUnit->GetModel()->mModels[inUnit->mComplexUnit->GetAnimation()]->GetBoundingBox(inUnit->mComplexUnit->mPhysicalModel.GetFrame()).GetTopRightBack(), thePoint);
    return thePoint;
}

extern "C" float ee_units_uu_host_world_f(int32_t inWhich, uint64_t inA, uint64_t inB, float inF)
{
    EEUnit* theUnit = UUUnit(inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    switch (inWhich) {
    case 1: return EEIsInGatherReach(inF, theComplex, UUUnit(inB)) ? 1.0f : 0.0f;
    // (EEIsTileTopAbove's wall rise: ee_sim units uu_world_ops.rs wall_rise over the cases from 5)
    case 4: return ::gWorld.GetGameVariant()->mDepositFactor;
    case 5: return UUBoxTop(theUnit).GetZ();
    case 6: return theUnit->mStats->GetScale();
    case 7: return (float)theUnit->mComplexUnit->mHitPoints;
    case 8: return (float)theUnit->mComplexUnit->mStats->mMaxHitPoints;
    }
    return 0.0f;
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
    ee_units_uu_step_aside((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inClass, (uint64_t)(uintptr_t)inPlayer, theTile.mX, theTile.mY, inArg);
}

// ---- the C++ of the unit utilities' Rust bodies (ee_units uu_body.rs; EE_UNITS_UB_*): one statement of the
// original bodies each, in their order; inA, inB objects (a unit as its EEUnit*), inC, inD, inE values, ioF floats

static_assert(sizeof(GE3DPoint) == 3 * sizeof(float), "a GE3DPoint is three floats");
static_assert(sizeof(GEVector) == 3 * sizeof(float), "a GEVector is three floats");

// the bodies' scratch: EEConvertUnit's strings, upgrade list and cost; EEFn24E68F's attribute, strings and string id;
// the unit and tile lists; EEPreviewWall's corners; the helicopter priest's direction; EEUpdateUnitTile's waypoint
// point; EEFindTargetNear's area; EEUpgradeHitPoints' functor
struct EEUbStrings {
    UWideString mFormat;
    UWideString mMessage;
    UWideString mName;
};
static EEUbStrings sUbStrings;
static std::vector<EECivUpgrade> sUbUpgrades;
static unsigned long sUbCost;
static unsigned long sUbAttribute[2];
static long sUbStringID;
static UWideString sUbCategoryString;
static UWideString sUbAttributeString;
static std::vector<EEUnit*> sUbUnits;
static std::vector<U2DPoint<long> > sUbTiles;
static std::vector<long> sUbCorners;
static GEVector sUbDirection;
static GE3DPoint sUbPoint;
static U2DRectangle<long> sUbArea;
static EEHitPointUpgrader sUbUpgrader(0, 0.0f);

// EEMovePassengers' place for the unloaded cargo (off the map)
static GETransformation UbOffMap()
{
    GETransformation theTransformation;
    theTransformation.SetPosition(-10000.0f, -10000.0f, 0.0f);
    return theTransformation;
}
static const GETransformation sUbOffMap = UbOffMap();

// EEFindNewTargetNearOld's search near the old target (its screen and tile are the original's locals)
static bool UbFindNewTarget(EEComplexUnit* inTarget, EEComplexUnit* inUnit, long inX, long inY, long inRadius)
{
    EETileCoordinate theTile(inX, inY);
    EEFOFindNewTargetNearOldTarget theScreen(inTarget, 25.0f);
    return EEFindNewTarget(&theScreen, inUnit->GetLoadGoal(), inUnit, &theTile, inRadius, true, 0);
}

extern "C" int64_t ee_units_ub_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, int64_t inE, float (*ioF)[4])
{
    EEUnit* theUnit = UUUnit(inA);
    EEComplexUnit* theComplex = UbObj<EEComplexUnit>(inA);
    EEComplexUnit* theOther = UbObj<EEComplexUnit>(inB);
    EEUnitClass* theClass = UbObj<EEUnitClass>(inA);
    EEPlayer* thePlayer = UbObj<EEPlayer>(inA);
    EEGroup* theGroup = UbObj<EEGroup>(inA);
    EEUnitGoal* theGoal = UbObj<EEUnitGoal>(inA);
    EETechnology* theTechnology = UbObj<EETechnology>(inA);
    EEUnitClassUpgrade* theUpgrade = UbObj<EEUnitClassUpgrade>(inA);
    EEUUUnitFunctor* theFunctor = UbObj<EEUUUnitFunctor>(inA);
    GE3DPoint* thePoint = UbObj<GE3DPoint>(inA);
    GETransformation* theTransformation = UbObj<GETransformation>(inA);
    float* f = *ioF;
    switch (inOp) {
    // EEConvertUnit's
    case EE_UNITS_UB_PLAYER_AT: return UUHandle(::gWorld.mPlayers[inC]);
    case EE_UNITS_UB_UNIT_CLASS: return UUHandle(theUnit->GetUnitClass());
    case EE_UNITS_UB_CLASS_TYPE: return theClass->mUnitType;
    case EE_UNITS_UB_HAS_POWER: return gCivPowersManager.HasPower(thePlayer, (unsigned long)inC);
    case EE_UNITS_UB_IS_BUILT: return theUnit->GetComplexUnit()->mIsBuilt;
    case EE_UNITS_UB_STRINGS_RESET: sUbStrings = EEUbStrings(); return 0;
    case EE_UNITS_UB_NUM_POWERS: return gCivPowersManager.GetNumPowers();
    case EE_UNITS_UB_UNIT_HAS_POWER: return gCivPowersManager.HasPower(theUnit->GetPlayer(), (unsigned long)inC);
    case EE_UNITS_UB_MAKE_POWER_UPGRADE: return gCivilizationManager.MakePowerUpgrade(EECivilizationManager::PowerLevel((unsigned long)inC));
    case EE_UNITS_UB_POWER_NAME: gCivPowersManager.GetName((unsigned long)inC, sUbStrings.mName); return 0;
    case EE_UNITS_UB_UPGRADES_COPY: sUbUpgrades = theUnit->GetPlayer()->mCivilization.GetPurchasedUpgrades(); return 0;
    case EE_UNITS_UB_UPGRADES_SHUFFLE: EEAIRandomShuffle(sUbUpgrades.begin(), sUbUpgrades.end()); return 0;
    case EE_UNITS_UB_UPGRADES_COUNT: return (int64_t)sUbUpgrades.size();
    case EE_UNITS_UB_HAS_UPGRADE: return EEHasCivilizationUpgrade(&thePlayer->mCivilization, (EECivUpgrade)inC);
    case EE_UNITS_UB_UPGRADE_AT: return (int64_t)sUbUpgrades[(size_t)inC];
    case EE_UNITS_UB_UPGRADE_NAME: return EEFn24E68F((EECivUpgrade)inC, sUbStrings.mName);
    case EE_UNITS_UB_AVAILABLE_POINTS: return thePlayer->mCivilization.GetAvailablePoints();
    case EE_UNITS_UB_CAN_AFFORD: return thePlayer->mCivilization.CanAfford((EECivUpgrade)inC, &sUbCost, NULL);
    case EE_UNITS_UB_SET_POINTS_AND_MAX: thePlayer->mCivilization.SetPointsAndMax(thePlayer->mCivilization.GetMaxPoints() + sUbCost); return 0;
    case EE_UNITS_UB_BUY: thePlayer->mCivilization.Buy((EECivUpgrade)inC, thePlayer->GetPlayerID(), true); return 0;
    case EE_UNITS_UB_SET_POINTS: thePlayer->mCivilization.SetPoints((long)inC); return 0;
    case EE_UNITS_UB_CURRENT_PLAYER: return UUHandle(::gWorld.GetCurrentPlayer());
    case EE_UNITS_UB_ADAPTATION_FORMAT: gResource->GetString(IDS_ACQUIRED_THROUGH_ADAPTATION, sUbStrings.mFormat); return 0;
    case EE_UNITS_UB_ADAPTATION_MESSAGE: sprintf(sUbStrings.mMessage, sUbStrings.mFormat, (const char*)sUbStrings.mName); return 0;
    case EE_UNITS_UB_ADAPTATION_CHAT:
        gUserInterface->AddChatLogEntry(theUnit->GetPlayer()->mColor, theUnit->GetPlayer()->mName, sUbStrings.mMessage,
                                        gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        return 0;
    case EE_UNITS_UB_CLASS_OF_TYPE: return UUHandle(gUnitClassTable[inC]);
    case EE_UNITS_UB_IS_CRUSADER: return theClass->IsCrusader();
    case EE_UNITS_UB_HIT_FRACTION: return UbBits((float)theUnit->GetHitPoints() / (float)theUnit->mStats->mMaxHitPoints);
    case EE_UNITS_UB_DESELECT: theUnit->GetPlayer()->DeselectUnit(theUnit->GetComplexUnit()); return 0;
    case EE_UNITS_UB_GROUP_CONTAINS: return theUnit->GetPlayer()->mGroups[inC]->Contains(theUnit->GetComplexUnit());
    case EE_UNITS_UB_GROUP_REMOVE: theUnit->GetPlayer()->mGroups[inC]->RemoveUnit(theUnit->GetComplexUnit()); return 0;
    case EE_UNITS_UB_SET_CLASS_MEDIA: thePlayer->SetUnitClassMedia(UUUnit(inB)->GetUnitClass()->mUnitType, 0x7f); return 0;
    case EE_UNITS_UB_UNLOAD_CLASS_MEDIA: thePlayer->UnloadUnitClassMedia(UUUnit(inB)->GetUnitClass()->mUnitType); return 0;
    case EE_UNITS_UB_COMPLEX_OF: return UUHandle(theUnit->GetComplexUnit());
    case EE_UNITS_UB_CLEAR_GARRISON: theComplex->ClearGarrison(); return 0;
    case EE_UNITS_UB_IS_LAND_BUILDING: return theClass->IsLandBuilding();
    case EE_UNITS_UB_IS_WALL: return theClass->mIsWall;
    case EE_UNITS_UB_IS_TOWER: return theClass->mIsTower;
    case EE_UNITS_UB_START_ACTION: theComplex->StartAction((long)inC); return 0;
    case EE_UNITS_UB_BUILD_QUEUE_SIZE: return theComplex->GetBuildQueueSize();
    case EE_UNITS_UB_BUILD_QUEUE_CLEAR: theComplex->mBuildQueue.clear(); return 0;
    case EE_UNITS_UB_CATEGORY: return theClass->mCategory;
    case EE_UNITS_UB_SET_ANIMATION_OF: theComplex->SetAnimation(UUUnit(inB)->GetComplexUnit()->mAnimation, -1, 0); return 0;
    case EE_UNITS_UB_START_COMMAND_OF:
        theComplex->StartAction(UUUnit(inB)->GetComplexUnit()->mCommand != NULL ? UUUnit(inB)->GetComplexUnit()->mCommand->mType : 0);
        return 0;
    case EE_UNITS_UB_QUEUE_SWAP: gPhysics->QueueUnitSwap(theComplex, theOther); return 0;
    case EE_UNITS_UB_PREVIOUS_PART: return UUHandle(theComplex->mPreviousPart);
    case EE_UNITS_UB_GATE_REPLACED: theUnit->GetUnitClass()->GateReplaced(theUnit, theOther); return 0;
    case EE_UNITS_UB_CLEAR_ALL_GOAL_QUEUES: theComplex->ClearAllGoalQueues(); return 0;
    case EE_UNITS_UB_CLEAR_BUILD_TARGETS: theComplex->ClearBuildTargets(); return 0;
    case EE_UNITS_UB_PLAY_EFFECT: theComplex->PlayEffect((long)inC); return 0;
    case EE_UNITS_UB_PRODUCTION_TIME: return theComplex->mProductionTime;
    case EE_UNITS_UB_CLEAR_PRODUCTION_TIME: theComplex->mProductionTime = 0; return 0;
    case EE_UNITS_UB_ACTION_CHANGED: theClass->ActionChanged(theOther); return 0;
    case EE_UNITS_UB_KILL: theComplex->Kill((inC & 1) != 0, (inC & 2) != 0, (inC & 4) != 0); return 0;
    case EE_UNITS_UB_STATS_RESET: thePlayer->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_UB_ADD_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theComplex);
    case EE_UNITS_UB_UNIT_STATS_LOWER: theUnit->GetPlayer()->mSavedGroups.Lower((long)inC, 1); return 0;
    case EE_UNITS_UB_UNIT_STATS_RESET: theUnit->GetPlayer()->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_UB_HAS_DAMAGE_EFFECT: return theClass->mDamageEffect != NULL;
    case EE_UNITS_UB_DAMAGE_PARAM2: return UbBits(theClass->mDamageEffect->mParam2);
    case EE_UNITS_UB_CREATE_DAMAGE_EFFECT:
        gWorld.GetEffectManager()->CreateUnitEffect(theComplex, theComplex->GetUnitClass()->mDamageEffect, NULL, 0, 0, 0);
        return 0;
    // EEMakeUnitCopy's, EEReplaceUnit's, EEMakeBuilding's
    case EE_UNITS_UB_TILE_X: return theUnit->GetTileX();
    case EE_UNITS_UB_TILE_Y: return theUnit->GetTileY();
    case EE_UNITS_UB_IS_FOUNDATION: return theUnit->IsFoundation();
    case EE_UNITS_UB_CREATE_UNIT: return UUHandle(theClass->CreateUnit((long)inC, (long)inD, (unsigned long)inE, inB != 0, NULL));
    case EE_UNITS_UB_COPY_FROM: theUnit->CopyFrom(UUUnit(inB)); return 0;
    case EE_UNITS_UB_TILE_NODE: return UUHandle(theUnit->mTileNode);
    case EE_UNITS_UB_SET_BUILT: theComplex->SetBuilt((long)inC, (long)inD, (long)inE); return 0;
    case EE_UNITS_UB_SET_HIT_POINTS: theUnit->SetHitPoints((long)inC); return 0;
    case EE_UNITS_UB_MAX_HIT_POINTS: return theUnit->mStats->mMaxHitPoints;
    case EE_UNITS_UB_PLAYER_OF: return UUHandle(theUnit->GetPlayer());
    case EE_UNITS_UB_GROUP_AT: return UUHandle(thePlayer->mGroups[inC]);
    case EE_UNITS_UB_GROUP_NUM_UNITS: return theGroup->GetNumUnits();
    case EE_UNITS_UB_GROUP_UNIT: return UUHandle(theGroup->GetUnit((unsigned long)inC));
    case EE_UNITS_UB_GROUP_ADD: theGroup->Add(theOther); return 0;
    case EE_UNITS_UB_DROP_UNSUITED_GOALS: theComplex->DropUnsuitedGoals(0); return 0;
    case EE_UNITS_UB_TECH_AVAILABLE: return thePlayer->mTechnologies->Get((long)inC)->GetIsAvailable();
    case EE_UNITS_UB_NEXT_PART: return UUHandle(theComplex->mNextPart);
    case EE_UNITS_UB_ENERGY_WALL:
        gWorld.GetEffectManager()->CreateUnitEffect(theComplex, EEGraphicEffect::Get(inC != 0 ? kGraphicEffectSpaceGateEnergyWall : kGraphicEffectSpaceWallEnergyWall),
                                                    &theComplex->mFireEffect, 0, 0, 0);
        return 0;
    case EE_UNITS_UB_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_UB_TILE_VISIBILITY: return theUnit->mTile->GetVisibility(UbObj<EEPlayer>(inB)->GetPlayerID());
    case EE_UNITS_UB_ADD_FOR_UNSEEN: gDoppelgangerManager.AddForUnseenUnit(theComplex); return 0;
    case EE_UNITS_UB_PAY_CLASS: thePlayer->Pay(thePlayer->GetTechTree()->Get(UbObj<EEUnitClass>(inB)->GetTechnology()), 0x20, 1.0f); return 0;
    case EE_UNITS_UB_IS_PAID_FOR: return theComplex->mIsPaidFor;
    case EE_UNITS_UB_SET_PAID_FOR: theComplex->mIsPaidFor = true; return 0;
    case EE_UNITS_UB_ADD_PENDING_UNITS: thePlayer->AddPendingUnits(false, true); return 0;
    case EE_UNITS_UB_FLUSH_PLACEMENTS: ::gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_UB_REPLANT_FARMS: gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReplantFarms, {theUnit->mUnitID}), kSlotNone); return 0;
    case EE_UNITS_UB_PLAYER_ID: return thePlayer->GetPlayerID();
    // EEReplaceQueuedTechnology's
    case EE_UNITS_UB_CLASS_TECHNOLOGY: return theClass->mTechnology;
    case EE_UNITS_UB_TECHNOLOGY: return UUHandle(thePlayer->mTechnologies->Get((long)inC));
    case EE_UNITS_UB_TECH_NUM_CLASSES: return theTechnology->GetNumUnitClasses();
    case EE_UNITS_UB_TECH_CLASS: return theTechnology->GetUnitClass((unsigned long)inC);
    case EE_UNITS_UB_UNITS_OF_TYPE: return ee_units_uu_player_units_of_type((long)inC, thePlayer->GetPlayerID(), &sUbUnits);
    case EE_UNITS_UB_LIST_UNIT: return UUHandle(sUbUnits[(size_t)inC]);
    case EE_UNITS_UB_REPLACE_RESEARCH: theComplex->ReplaceResearch((long)inC, (long)inD); return 0;
    // EEWorkFarm's, EERetargetAttack's, EEFindNewTargetNearOld's
    case EE_UNITS_UB_SET_GATHER_GOAL: theComplex->SetGoal(EEUGRust::NewGather(theOther, NULL, false), 0, 0); return 0;
    case EE_UNITS_UB_SET_REPAIR_GOAL: theComplex->SetGoal(EEUGRust::NewBuildRepair(theOther->GetPlayer(), theOther, false), 0, 0); return 0;
    case EE_UNITS_UB_ATTACK_TARGET: return UUHandle(static_cast<EEUGRust*>(theGoal)->GetAttackTarget()->mComplexUnit);
    case EE_UNITS_UB_GOAL_REPLACE_UNIT: theGoal->ReplaceUnit(theOther, UbObj<EEComplexUnit>((uint64_t)inC), NULL); return 0;
    case EE_UNITS_UB_CLEAR_COLLISION_COUNT: theComplex->mCollisionResolutionCount = 0.0f; return 0;
    case EE_UNITS_UB_CLEAR_BLOCKING: theComplex->SetBlockingUnit(NULL); return 0;
    case EE_UNITS_UB_COLLISION_STATE: return theComplex->mCollisionState;
    case EE_UNITS_UB_CLEAR_COLLISION_STATE: theComplex->mCollisionState = 0; return 0;
    case EE_UNITS_UB_DROP_PATH: theComplex->DropPath(); return 0;
    case EE_UNITS_UB_IGNORE_TARGET: theComplex->IgnoreTarget(theOther); return 0;
    case EE_UNITS_UB_FIND_NEW_TARGET: return UbFindNewTarget(theComplex, theOther, (long)inC, (long)inD, (long)inE);
    case EE_UNITS_UB_NO_TARGET_TRACE: EEDebugTrace(">>> SHAWN %d could not find a target.\n", theComplex->GetUnitClass()->GetName()); return 0;
    case EE_UNITS_UB_OUTERMOST_CARRIER: return UUHandle(theComplex->GetOutermostCarrier());
    case EE_UNITS_UB_PART_LEFT_OUT: return theClass->IsPartLeftOut(theOther);
    case EE_UNITS_UB_TARGET_IGNORED: return theComplex->IsTargetIgnored(theOther, inC != 0);
    case EE_UNITS_UB_LOAD_ATTACK_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EEAttackUnitCast(theComplex->GetLoadGoal())));
    case EE_UNITS_UB_LAST_LINKED_CARRIER: return UUHandle(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theClass)->GetLastLinkedCarrier(theOther));
    case EE_UNITS_UB_LAST_LINKED: return UUHandle(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theClass)->GetLastLinked(theOther));
    case EE_UNITS_UB_DISTANCE_SQUARED_TO: return UbBits(theComplex->DistanceSquaredTo(theOther));
    // EEGetHelicopterPriestDestination's
    case EE_UNITS_UB_CALAMITY_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EECastCalamityGoalCast(theComplex->GetGoal())));
    case EE_UNITS_UB_GOAL_LOCATION: static_cast<EEUGRust*>(theGoal)->GetGoalLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UB_CALAMITY_TYPE: return static_cast<EEUGRust*>(theGoal)->GetCalamityType();
    case EE_UNITS_UB_UNIT_TECHNOLOGY: return UUHandle(theUnit->mPlayer->mTechnologies->Get((long)inC));
    case EE_UNITS_UB_CALAMITY_RANGE:
        return USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)theTechnology->mData->mCalamity)->mRange;
    case EE_UNITS_UB_POSITION: theComplex->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UB_DIRECTION_SET: sUbDirection = *reinterpret_cast<GEVector*>(f); return 0;
    case EE_UNITS_UB_DIRECTION_NORMALIZE: sUbDirection.Normalize(); return 0;
    case EE_UNITS_UB_DIRECTION_SCALE: sUbDirection.Scale(f[0], *reinterpret_cast<GEVector*>(f)); return 0;
    case EE_UNITS_UB_SET_POINT: *thePoint = *reinterpret_cast<GE3DPoint*>(f); return 0;
    case EE_UNITS_UB_POINT: *reinterpret_cast<GE3DPoint*>(f) = *thePoint; return 0;
    // EEUpdateUnitTile's
    case EE_UNITS_UB_TRANSFORMATION_POSITION: theTransformation->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UB_TRANSFORMATION_MATRIX: memcpy((void*)(uintptr_t)inC, theTransformation->mMatrix, sizeof(theTransformation->mMatrix)); return 0;
    case EE_UNITS_UB_LOCATION_MATRIX: memcpy((void*)(uintptr_t)inC, theComplex->mPhysicalModel.GetLocation().mMatrix, 64); return 0;
    case EE_UNITS_UB_SET_LONG: *UbObj<long>(inA) = (long)inC; return 0;
    case EE_UNITS_UB_READ_LONG: return *UbObj<long>(inA);
    case EE_UNITS_UB_TILE_EXISTS: return ::gWorld.mMap->GetTile((long)inC, (long)inD) != NULL;
    case EE_UNITS_UB_SET_TRANSFORMATION: theComplex->SetTransformation(*UbObj<GETransformation>(inB), true); return 0;
    case EE_UNITS_UB_CLOSEST_TILE: ::gWorld.mMap->GetClosestTile(reinterpret_cast<GE3DPoint*>(f), UbObj<long>((uint64_t)inC), UbObj<long>((uint64_t)inD)); return 0;
    case EE_UNITS_UB_SET_LOCATION: theComplex->mPhysicalModel.SetLocation(*UbObj<GETransformation>(inB)); return 0;
    case EE_UNITS_UB_MOVE_TO: theComplex->MoveTo((long)inC, (long)inD, true); return 0;
    case EE_UNITS_UB_WAYPOINT_COUNT: return (long)theComplex->mWaypoints.size();
    case EE_UNITS_UB_DOMAINS: return theClass->mDomains;
    case EE_UNITS_UB_NEXT_WAYPOINT_POINT: theComplex->GetNextWaypointPoint(sUbPoint); return 0;
    case EE_UNITS_UB_HEIGHT_AT: return UbBits(theComplex->mUnitClass->GetHeightAt(theComplex, sUbPoint));
    case EE_UNITS_UB_HEIGHT: return UbBits(theComplex->mHeight);
    case EE_UNITS_UB_SET_HEIGHT: theComplex->mHeight = UbFloat(inC); return 0;
    // EEGetWaitingGatherer's
    case EE_UNITS_UB_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_UB_UNIT_TILES: ee_units_uu_unit_tiles(1, inA, &sUbTiles); return 0;
    case EE_UNITS_UB_UNITS_ON_TILES: ee_units_uu_units_on(0, &sUbTiles, &sUbUnits); return 0;
    case EE_UNITS_UB_RANDOM_GENERATE: return UbBits(gPhysics->mRandom.Generate());
    case EE_UNITS_UB_PERMUTE_UNITS: PermuteValues(sUbUnits, (long)inC); return 0;
    case EE_UNITS_UB_LIST_SIZE: return (int64_t)sUbUnits.size();
    case EE_UNITS_UB_LISTS_CLEAR: return (sUbUnits.clear(), sUbTiles.clear(), 0);
    case EE_UNITS_UB_COMMAND: return theComplex->mCommand != NULL ? theComplex->mCommand->mType : 0;
    case EE_UNITS_UB_GOAL_STATE: return UUHandle(theComplex->mGoalState);
    case EE_UNITS_UB_GOAL_KIND: return theGoal->mKind;
    case EE_UNITS_UB_GATHER_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EEGatherGoalCast(theGoal)));
    case EE_UNITS_UB_GATHER_RESOURCE: return UUHandle(static_cast<EEUGRust*>(theGoal)->GetGatherResource());
    // EEUpgradeHitPoints', the functor's and EEForEachUnitOf's
    case EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS: return theUpgrade->mUnitClass->mHitPoints;
    case EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS: return theUpgrade->GetMaxHitPoints();
    case EE_UNITS_UB_MAKE_UPGRADER: return UUHandle(static_cast<EEUUUnitFunctor*>(&(sUbUpgrader = EEHitPointUpgrader((unsigned long)inC, UbFloat(inD)))));
    case EE_UNITS_UB_SET_UPGRADE_MAX_HIT_POINTS: theUpgrade->mMaxHitPoints = (long)inC; return 0;
    case EE_UNITS_UB_LOADING_SAVED_GAME: return gLoadingSavedGame;
    case EE_UNITS_UB_FIRST_UNIT: return UUHandle(thePlayer->mFirstUnit);
    case EE_UNITS_UB_NEXT_UNIT: return UUHandle(static_cast<EEUnit*>(theUnit->mNext));
    case EE_UNITS_UB_APPLY_TO_UNIT: theFunctor->ApplyToUnit(UUUnit(inB)); return 0;
    case EE_UNITS_UB_APPLY: theFunctor->Apply(theOther); return 0;
    case EE_UNITS_UB_NUM_PENDING: return thePlayer->GetNumPendingUnits();
    case EE_UNITS_UB_PENDING_UNIT: return UUHandle(static_cast<EEUnit*>(thePlayer->GetPendingUnit((unsigned long)inC)));
    case EE_UNITS_UB_NUM_PLAYERS: return ::gWorld.mNumPlayers;
    case EE_UNITS_UB_UPGRADER_TYPE: return static_cast<EEHitPointUpgrader*>(theFunctor)->mUnitType;
    case EE_UNITS_UB_UPGRADER_RATIO: return UbBits(static_cast<EEHitPointUpgrader*>(theFunctor)->mRatio);
    // EEMovePassengers', EETakeOverLoad's, EEGetPlayerClassGraphic's
    case EE_UNITS_UB_NUM_CARGO: return theComplex->GetNumCargoUnits();
    case EE_UNITS_UB_UNLOAD_CARGO_AT: return UUHandle(theComplex->UnloadCargoAt((unsigned long)inC, sUbOffMap));
    case EE_UNITS_UB_LOAD_CARGO: theComplex->LoadCargo(theOther, false, inC != 0); return 0;
    case EE_UNITS_UB_TAKE_GARRISON:
        theComplex->AddGarrison(theOther->mGarrisonCount - theComplex->mUnitClass->mInitialGarrisonCount, theOther->mGarrisonPopulation);
        return 0;
    case EE_UNITS_UB_TILE_SIZE: return theClass->mTileSize;
    case EE_UNITS_UB_TABLE_MODEL: return UUHandle(gUnitClassTable[inC]->mModel);
    case EE_UNITS_UB_UPGRADE_MODEL: return UUHandle(thePlayer->mUnitClassUpgrades[inC]->mModel);
    // EEFindTargetNear's, EEFindNearbyTarget's, EECanBallisticHit's, EECanPriestConvert's
    case EE_UNITS_UB_RANGE: return UbBits(theUnit->mStats->GetRange());
    case EE_UNITS_UB_LINE_OF_SIGHT: return UbBits(theComplex->GetLineOfSight());
    case EE_UNITS_UB_ATTACK: return UbBits(theUnit->mStats->GetAttack());
    case EE_UNITS_UB_AMMO_TYPE: return theClass->mAmmoType;
    case EE_UNITS_UB_AREA_TOP: sUbArea.mTop = (long)inC; return 0;
    case EE_UNITS_UB_AREA_BOTTOM: sUbArea.mBottom = (long)inC; return 0;
    case EE_UNITS_UB_AREA_LEFT: sUbArea.mLeft = (long)inC; return 0;
    case EE_UNITS_UB_AREA_RIGHT: sUbArea.mRight = (long)inC; return 0;
    case EE_UNITS_UB_SUM_OTHERS_INFLUENCE: return ::gWorld.mInfluenceTable->SumOthersValueOnTiles((unsigned long)inC, sUbArea);
    case EE_UNITS_UB_SEARCH_TARGET: return UUHandle(EESearchTarget(theComplex, &sUbArea, (long)inC, (long)inD, UbFloat(inE)));
    case EE_UNITS_UB_SEARCH_ALONG_LIST: return UUHandle(EESearchTargetAlongList(theComplex, (long)inC, (long)inD, UbFloat(inE)));
    case EE_UNITS_UB_APPROACH_ATTACH_POINT: return EEApproachAttachPoint(theComplex, *UbObj<GE3DPoint>(inB));
    case EE_UNITS_UB_OTHER_CONTINENT: return theComplex->IsOnOtherContinentThan(theOther);
    case EE_UNITS_UB_FIND_DROP_LOCATION: return UUHandle(EECombatFindDropLocation(theComplex, UUUnit(inB), *UbObj<GE3DPoint>((uint64_t)inC)));
    case EE_UNITS_UB_BALLISTIC_PITCH:
        return EEGetBallisticPitch(theComplex, theComplex->GetStats(), UbObj<GE3DPoint>(inB), UbObj<GE3DPoint>((uint64_t)inC), UbObj<float>((uint64_t)inD));
    case EE_UNITS_UB_SHOOTS_HIGH: return theClass->mShootsHigh;
    case EE_UNITS_UB_FIRES_OVER_WALLS: return theComplex->GetStats()->mCanFireOverWalls;
    case EE_UNITS_UB_LINE_TO_UNIT_BLOCKED:
        return EEIsLineToUnitBlocked(theComplex, UUUnit(inB), UbObj<EEUnit*>((uint64_t)inC), UbObj<long>((uint64_t)inD), UbObj<long>((uint64_t)inE));
    case EE_UNITS_UB_IS_PRIEST: return theClass->IsPriest();
    case EE_UNITS_UB_CAN_CONVERT: return EECanConvert(theComplex, theOther, *UbObj<bool>((uint64_t)inC));
    // EEPreviewWall's
    case EE_UNITS_UB_DROP_TILES: ee_units_uu_drop_tiles((int32_t)inC, UbObj<std::vector<EETileCoordinate> >(inA)); return 0;
    case EE_UNITS_UB_PICK_WALL_PIECES: EEPickWallPieces(*UbObj<std::vector<EETileCoordinate> >(inA), sUbCorners, ::gWorld.mMap); return 0;
    case EE_UNITS_UB_BUILD_WALL_GHOSTS: EEBuildWallGhosts(*UbObj<std::vector<EETileCoordinate> >(inA), sUbCorners, (long)inC, UbObj<EEPlayer>(inB), 0.0f); return 0;
    case EE_UNITS_UB_CORNERS_CLEAR: sUbCorners.clear(); return 0;
    // EEHasCivilizationUpgrade's, EEFn24E68F's
    case EE_UNITS_UB_PURCHASED_COUNT: return (int64_t)UbObj<EECivilization>(inA)->GetPurchasedUpgrades().size();
    case EE_UNITS_UB_PURCHASED_AT: return (int64_t)UbObj<EECivilization>(inA)->GetPurchasedUpgrades()[(size_t)inC];
    case EE_UNITS_UB_SPLIT_ATTRIBUTE: gCivilizationManager.SplitAttribute((EECivUpgrade)inC, &sUbAttribute[0], &sUbAttribute[1]); return 0;
    case EE_UNITS_UB_LOOKUP_LEVEL: gCivilizationManager.LookupLevel((EECivUpgrade)inC, &f[0], &f[1]); return 0;
    case EE_UNITS_UB_CATEGORY_NAME_ID: return gCivilizationManager.GetCategoryNameID(sUbAttribute[0], &sUbStringID);
    case EE_UNITS_UB_CATEGORY_STRING: return gResource->GetString(sUbStringID, sUbCategoryString);
    case EE_UNITS_UB_LEVEL_NAME_ID: sUbStringID = EECivilizationManager::LevelNameID(sUbAttribute[1]); return 0;
    case EE_UNITS_UB_ATTRIBUTE_STRING: return gResource->GetString(sUbStringID, sUbAttributeString);
    case EE_UNITS_UB_IS_POWER_LEVEL: return EECivilizationManager::IsPowerLevel(sUbAttribute[1]);
    case EE_UNITS_UB_LEVEL_TEXT:
        sprintf(*UbObj<UWideString>(inA), "%s: %s %ld%%", (const char*)sUbCategoryString, (const char*)sUbAttributeString, (long)inC);
        return 0;
    case EE_UNITS_UB_POWER_TEXT: sprintf(*UbObj<UWideString>(inA), "%s", (const char*)sUbAttributeString); return 0;
    }
    return 0;
}
