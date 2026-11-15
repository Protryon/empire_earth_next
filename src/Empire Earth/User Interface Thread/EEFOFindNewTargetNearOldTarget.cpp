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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
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

extern GEViewport gViewport;                // 0x5308f0 (Empire Earth.cpp)
extern EEToHitTable gToHitTable;           // 0x5318e8 (as EECPAttackSubGroupUnit.cpp declares it)

// The unit's named float constants (.rdata 0x449b54..0x449ba0, around the vtable 0x449ba8) (provisional names)
// (0x449b54..0x449b60, 0x449b98..0x449ba0: the sound's dead zone, falloffs and distances: ee_units uu_world.rs)
static const float kUUSectorLow = -0.39269909f;  // 0x449bb8 (-pi/8)
static const float kUUSectorScale = 1.2732395f;  // 0x449bbc (4/pi)
static const float kUUSectorHigh = 0.39269909f;  // 0x449bc0 (pi/8)
static const float kUUWorkReach = 1.5f;          // 0x449b80
static const float kUUGatherReach = 1.42f;       // 0x449b84
static const float kUUMinimumFacing = 0.1f;      // 0x449b64
static const float kUUTwoOverPi = 0.63661975f;   // 0x449bc4
static const float kUUWallYaw10 = 5.4977875f;    // 0x449bc8
static const float kUUWallYaw9 = 3.926991f;      // 0x449bcc
static const float kUUWallYaw8 = 4.712389f;      // 0x449bd0
static const float kUUClearance = 0.1f;          // 0x449b78
static const float kUUYawNudge = 0.0174f;        // 0x449b68 (0x24286a: about one degree)
static const float kUUSearchTurns = 5.0f;        // 0x449b6c
static const float kUUCheckReach = 2.5f;         // 0x449b70
static const float kUUWidthFactor = 0.5f;        // 0x449b74
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

// TEST ONLY: the unit utilities' record (UUTL, the unit utilities lane, round 1). The outermost call of a noted
// utility on its thread folds its kind, inputs and outputs (units as ids, floats as bits, tile lists whole) into the
// physics tick's FNV-1a 64; the record (the tick, the calls, the digest) is written when a later tick's first call
// comes. Nothing is noted unless the capture runs. Its golden came from the C++ utilities (tools/sim_capture.py).
static thread_local int sUUDepth = 0;
static std::mutex sUUMutex;
static uint64_t sUUDigest = 0xcbf29ce484222325ull;
static uint32_t sUUCalls = 0;
static unsigned long sUUTick = 0;
static uint32_t sUUKinds[128];
class EEUUNote;
static thread_local EEUUNote* sUUCurrent = NULL;   // the thread's recording outermost note (the events' target)
class EEUUNote {
public:
    explicit EEUUNote(uint32_t inKind) : mOn(++sUUDepth == 1 && EESimCaptureRecord::Active()), mKind(inKind)
    {
        this->I(inKind);
        if (this->mOn)
            sUUCurrent = this;
    }
    ~EEUUNote();
    EEUUNote& I(long inValue) { if (this->mOn) this->mWords.push_back((uint32_t)inValue); return *this; }
    EEUUNote& F(float inValue) { uint32_t theBits; memcpy(&theBits, &inValue, 4); return this->I((long)theBits); }
    EEUUNote& U(const EEUnit* inUnit) { return this->I(inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2); }
    EEUUNote& P(const GE3DPoint& inPoint) { return this->F(inPoint.mX).F(inPoint.mY).F(inPoint.mZ); }
    EEUUNote& T(const std::vector<U2DPoint<long> >& inTiles)
    {
        this->I((long)inTiles.size());
        for (size_t i = 0; this->mOn && i < inTiles.size(); i++)
            this->I(inTiles[i].mX).I(inTiles[i].mY);
        return *this;
    }
    EEUUNote& Us(const std::vector<EEUnit*>& inUnits)
    {
        this->I((long)inUnits.size());
        for (size_t i = 0; this->mOn && i < inUnits.size(); i++)
            this->U(inUnits[i]);
        return *this;
    }
    EEUUNote& M(const GETransformation& inTransformation)
    {
        for (int r = 0; this->mOn && r < 4; r++)
            for (int c = 0; c < 4; c++)
                this->F(inTransformation.mMatrix[r][c]);
        return *this;
    }
    // the outputs, read when the call returns
    EEUUNote& OutT(std::vector<U2DPoint<long> >* inTiles) { this->mOutTiles = inTiles; return *this; }
    EEUUNote& OutU(std::vector<EEUnit*>* inUnits) { this->mOutUnits = inUnits; return *this; }
    EEUUNote& OutL(long* inFirst, long* inSecond = NULL) { this->mOutLongs[0] = inFirst; this->mOutLongs[1] = inSecond; return *this; }
    EEUUNote& OutP(GE3DPoint* inPoint) { this->mOutPoint = inPoint; return *this; }
    EEUUNote& OutM(const GETransformation* inTransformation) { this->mOutMatrix = inTransformation; return *this; }
    EEUUNote& OutR(GERectangle* inRectangle) { this->mOutRect = inRectangle; return *this; }
    EEUUNote& Pl(EEPlayer* inPlayer) { return this->I(inPlayer != NULL ? (long)inPlayer->GetPlayerID() : -2); }
    EEUUNote& C(EEUnitClass* inClass) { return this->I(inClass != NULL ? (long)inClass->mUnitType : -2); }
    EEUUNote& Ps(const std::vector<GE3DPoint>& inPoints)
    {
        this->I((long)inPoints.size());
        for (size_t i = 0; this->mOn && i < inPoints.size(); i++)
            this->P(inPoints[i]);
        return *this;
    }
    EEUUNote& OutF(float* inFirst, float* inSecond = NULL) { this->mOutFloats[0] = inFirst; this->mOutFloats[1] = inSecond; return *this; }
    EEUUNote& OutPs(std::vector<GE3DPoint>* inPoints) { this->mOutPoints = inPoints; return *this; }
    template <class V> V R(V inValue) { this->I((long)inValue); return inValue; }
    template <class V> V* RU(V* inUnit) { this->U(inUnit); return inUnit; }
    float RF(float inValue) { this->F(inValue); return inValue; }
    // the units on the tiles of a footprint, read when the call returns
    EEUUNote& OutFoot(long inX, long inY, long inSize) { this->mFoot[0] = inX; this->mFoot[1] = inY; this->mFoot[2] = inSize; return *this; }
    long mFoot[3] = {0, 0, 0};
    bool mOn;
    uint32_t mKind;
    std::vector<uint32_t> mWords;
    std::vector<U2DPoint<long> >* mOutTiles = NULL;
    std::vector<EEUnit*>* mOutUnits = NULL;
    long* mOutLongs[2] = {NULL, NULL};
    GE3DPoint* mOutPoint = NULL;
    const GETransformation* mOutMatrix = NULL;
    GERectangle* mOutRect = NULL;
    float* mOutFloats[2] = {NULL, NULL};
    std::vector<GE3DPoint>* mOutPoints = NULL;
};

// an event of the outermost recording note on this thread (a unit utility's write: a build target, a step aside)
static long UUId(const EEUnit* inUnit) { return inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2; }
static void UUEvent(long inA, long inB = 0, long inC = 0)
{
    if (sUUCurrent != NULL)
        sUUCurrent->I(inA).I(inB).I(inC);
}

EEUUNote::~EEUUNote()
{
    sUUDepth--;
    if (!this->mOn)
        return;
    sUUCurrent = NULL;
    for (long x = this->mFoot[0]; x < this->mFoot[0] + this->mFoot[2]; x++)
        for (long y = this->mFoot[1]; y < this->mFoot[1] + this->mFoot[2]; y++) {
            EETile* theTile = ::gWorld.mMap != NULL ? ::gWorld.mMap->GetTile(x, y) : NULL;
            for (RTSTileUnitNode* theNode = theTile != NULL ? theTile->mUnits : NULL; theNode != NULL; theNode = theNode->mNext)
                this->U(theNode->mUnit);
            this->I(-3);
        }
    if (this->mOutTiles != NULL)
        this->T(*this->mOutTiles);
    if (this->mOutUnits != NULL)
        this->Us(*this->mOutUnits);
    for (int i = 0; i < 2; i++)
        if (this->mOutLongs[i] != NULL)
            this->I(*this->mOutLongs[i]);
    if (this->mOutPoint != NULL)
        this->P(*this->mOutPoint);
    if (this->mOutMatrix != NULL)
        this->M(*this->mOutMatrix);
    for (int i = 0; i < 2; i++)
        if (this->mOutFloats[i] != NULL)
            this->F(*this->mOutFloats[i]);
    if (this->mOutPoints != NULL)
        this->Ps(*this->mOutPoints);
    if (this->mOutRect != NULL)
        this->F(this->mOutRect->GetLeft()).F(this->mOutRect->GetTop()).F(this->mOutRect->GetRight()).F(this->mOutRect->GetBottom());
    std::lock_guard<std::mutex> theLock(sUUMutex);
    unsigned long theTick = gPhysics != NULL ? gPhysics->mNumTicksDone : 0;
    if (theTick != sUUTick) {
        if (sUUCalls != 0) {
            EESimCaptureRecord theRecord(0x4c545555u);   // "UUTL"
            theRecord.U32((unsigned int)sUUTick);
            theRecord.U32(sUUCalls);
            theRecord.U32((unsigned int)(sUUDigest & 0xffffffffu));
            theRecord.U32((unsigned int)(sUUDigest >> 32));
            theRecord.Commit();
        }
        if (theTick / 20000 != sUUTick / 20000) {
            fprintf(stderr, "EE-UU-COUNT %lu", theTick);
            for (int i = 1; i < 128; i++)
                fprintf(stderr, " %u", sUUKinds[i]);
            fprintf(stderr, "\n");
        }
        sUUDigest = 0xcbf29ce484222325ull;
        sUUCalls = 0;
        sUUTick = theTick;
    }
    sUUKinds[this->mKind & 127]++;
    for (size_t i = 0; i < this->mWords.size(); i++)
        for (int b = 0; b < 4; b++) {
            sUUDigest ^= (this->mWords[i] >> (8 * b)) & 0xff;
            sUUDigest *= 0x100000001b3ull;
        }
    sUUCalls++;
}


// Units by id: EEUUUnitIDLess / EEUUUnitIDEqual are declared in ee/EEUnitCollector.h (its EEGetUnitsInRectangle
// sorts with them too; EEAIArea.cpp's family 0xe3d7.. calls these bodies)

// FUNCTION: EXE 0x63dd96
// Orders units by ascending unit id
bool EEUUUnitIDLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    return inFirst->mUnitID < inSecond->mUnitID;
}

// FUNCTION: EXE 0x63ddab
// Tells whether two units have the same unit id
bool EEUUUnitIDEqual::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    return inFirst->mUnitID == inSecond->mUnitID;
}

// (the TU's own sort-and-unique keys, highest unit id first: ee_units uu.rs sort_unique_by_id_descending)

// Built without /Og (frame 0x54: the locals of the two undone DistanceSquaredToTile / DistanceSquared2D expansions);
// the pragma alone makes 0x63ddc1 and 0x63de0f exact (r37_big, with packet B1)
#pragma optimize("g", off)
// FUNCTION: EXE 0x63ddc1
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

// FUNCTION: EXE 0x63dee8
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

// FUNCTION: EXE 0x63df77
// Smooths a ship's path: keeps the ends and drops points whose straight line from the last kept point stays
// on passable water away from other units
// The naval path smoothing (EEUCNaval::AdjustPath): when inSmooth, keep the first and last points and drop each point
// whose straight line from the last kept point stays on passable tiles away from other units
void EEAdjustNavalPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    EEUUNote theNote(1); theNote.U(inUnit).Ps(ioPath).I(inSmooth).OutPs(&ioPath);   // TEST ONLY (UUTL)
    if (!inSmooth)
        return;
    std::vector<float> thePoints;
    for (size_t i = 0; i < ioPath.size(); i++) {
        thePoints.push_back(ioPath[i].mX);
        thePoints.push_back(ioPath[i].mY);
    }
    std::vector<uint32_t> theKept(ioPath.size() + 1);
    uint32_t theCount = ee_units_uu_naval_path(UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[2])(thePoints.empty() ? NULL : &thePoints[0]),
                                               (uint32_t)ioPath.size(), &theKept[0]);
    std::vector<GE3DPoint> theNewPath;
    for (uint32_t i = 0; i < theCount; i++)
        theNewPath.push_back(ioPath[theKept[i]]);
    ioPath = theNewPath;
}

// FUNCTION: EXE 0x63e530
// Collects the units on a tile that block a unit type for a player; true when there are any
bool EEGetBlockingUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner,
               std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(2); theNote.Pl(inPlayer).I(inUnitType).I(inIgnoreOwner).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_tile_units_standing(UUHandle(inPlayer), inUnitType, UUHandle(inTile), inIgnoreOwner, 2, &outUnits) != 0);
}

// FUNCTION: EXE 0x63e598
// Collects the units on a tile that can move out of the way of a unit type; true when there are any
bool EEGetMovableTileUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner,
               std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(3); theNote.Pl(inPlayer).I(inUnitType).I(inIgnoreOwner).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_tile_units_standing(UUHandle(inPlayer), inUnitType, UUHandle(inTile), inIgnoreOwner, 1, &outUnits) != 0);
}


// FUNCTION: EXE 0x63e630
// Hands the wall pieces out to the group's builders until every piece has one; units that had another goal
// start over
// Hands the wall pieces inPieces out to inGroup's units: spread evenly first, then every unit steps further along the
// wall (alternating sides) until each piece has a builder; the units that had a goal start over
void EEBuildWall(EEActiveGroup* inGroup, std::vector<EEUnit*>& inPieces)
{
    EEUUNote theNote(73); theNote.I(inGroup->GetNumUnits()).Us(inPieces);   // TEST ONLY (UUTL)
    ee_units_uu_build_wall(UUHandle(inGroup), &inPieces);   // ee_units uu_world.rs build_wall (0x63e600 EEHasZeroCount too)
}

// FUNCTION: EXE 0x63eabc
// Sends a citizen to a farm: to gather when it is built and whole, else to build or repair it

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


// FUNCTION: EXE 0x63eb4c
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

// FUNCTION: EXE 0x63ebb2
// Builds a wall line for a player: a new piece (paid foundation or finished) on every free tile, existing
// pieces reshaped and joined
// The wall of class inClass from inStart to inEnd for inPlayer: a new piece on every free tile of the line (paid for
// and started as a foundation when inArg5), the existing pieces re-joined; the new pieces go to outPieces
void EEPlanWall(GE2DPoint inStart, GE2DPoint inEnd, EEUnitClass* inClass, EEPlayer* inPlayer, bool inAsFoundation, bool inSkipPlayerChecks,
                std::vector<EEUnit*>& outPieces)
{
    EEUUNote theNote(74); theNote.F(inStart.mX).F(inStart.mY).F(inEnd.mX).F(inEnd.mY).C(inClass).Pl(inPlayer).I(inAsFoundation).I(inSkipPlayerChecks).Us(outPieces).OutU(&outPieces);   // TEST ONLY (UUTL)
    ee_units_uu_plan_wall(inStart.mX, inStart.mY, inEnd.mX, inEnd.mY, UUHandle(inClass), UUHandle(inPlayer), inAsFoundation,
                          inSkipPlayerChecks, &outPieces);   // ee_units uu_world.rs plan_wall
}


// (0x63efa1, EEGetSeabedUnder: ee_units uu_world.rs seabed_under)

// FUNCTION: EXE 0x63f27e
// Puts a submarine at its cruise depth, or at the surface when the seabed is too high
void EEGetSeabedDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    EEUUNote theNote(51); theNote.U(inUnit).P(ioLocation).OutP(&ioLocation);   // TEST ONLY (UUTL)
    ioLocation.SetZ(ee_units_uu_seabed(0, UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])&ioLocation.mX));
}

// FUNCTION: EXE 0x63f2c0
// Moves a submarine one small step up or down towards its cruise depth, staying above the seabed
void EEAdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    EEUUNote theNote(52); theNote.U(inUnit).P(ioLocation).OutP(&ioLocation);   // TEST ONLY (UUTL)
    ioLocation.SetZ(ee_units_uu_seabed(1, UUHandle(static_cast<EEUnit*>(inUnit)), (const float(*)[3])&ioLocation.mX));
}

// FUNCTION: EXE 0x63f32a
// Tells whether a unit can reach a target across path sub-sectors within a step limit; reports the target
// tile or its closest reachable tile
// Whether inUnit can reach inTarget across path sub-sectors (a path request of inArg5 (10/25 by distance when out
// of 1..25) steps; *outTile: the target sub-sector's location, or its closest tile when inArg4 is set)
bool EEHasPathToTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile, bool inUseClosestTile, long inMaxSteps)
{
    EEUUNote theNote(79); theNote.U(inUnit).U(inTarget).I(inUseClosestTile).I(inMaxSteps).OutL(&outTile->mX, &outTile->mY);   // TEST ONLY (UUTL)
    EECPASGFlags theFlags;
    U2DPoint<long> theFrom(inUnit->mTileX, inUnit->mTileY);
    U2DPoint<long> theTo(inTarget->mTileX, inTarget->mTileY);
    EEAIPathSubSector* theFromSector = gContinentManager.GetPathSubSectorAt(inUnit);
    EEAIPathSubSector* theToSector = gContinentManager.GetPathSubSectorAt(inTarget);
    EEUnitClass* theClass = inUnit->mUnitClass;
    bool theResult = false;
    outTile->mX = 0;
    outTile->mY = 0;
    if (theClass->mMaxVelocity == 0.0f || (theClass->mDomains & kMovementLandOrSea) == 0 || theFromSector == theToSector
        || theFromSector == NULL || theToSector == NULL)
        return true;
    if (inUnit->mContainer != NULL || inTarget->mContainer != NULL)
        return false;
    long theSteps;
    if (inMaxSteps > 0 && inMaxSteps <= 0x19)
        theSteps = inMaxSteps;
    else if (inUnit->DistanceSquaredTo(inTarget) <= 64.0f)
        theSteps = 10;
    else
        theSteps = 0x19;
    theClass->GetTerrainFlags(inUnit, theFlags);
    EEAutoPathRequest* theRequest = gPathManager.RequestPath(inUnit->GetPlayer()->GetPlayerID(), theFrom, theTo, &theFlags, 1,
                                                             theSteps, NULL, 1);
    if (theRequest == NULL)
        return false;
    if (theRequest->mIsDone && theRequest->mPath.size() != 0) {
        EETileCoordinate& theLast = theRequest->mPath.back();
        EEAIPathSubSector* theSector = gContinentManager.GetPathSubSectorAt(theLast.mX, theLast.mY);
        if (theSector != NULL) {
            theResult = theSector == theToSector;
            *outTile = theToSector->mLocation;
            if (!theResult) {
                long theKind = theToSector->mKind;
                if (theKind == kAITileWall || theKind == kAITileGate || theKind == kAITileUnitObstacle) {
                    theResult = theSector->IsNeighbor(theToSector);
                    if (theResult)
                        theResult = theToSector->AreLinksBlocked(theSector, theFlags);
                }
                if (!theResult && !inUnit->GetClass()->mIsMelee) {
                    float theRange = UMSquare(inUnit->mStats->mRange);
                    theResult = theTo.DistanceToSquaredF(theSector->mLocation) <= theRange;
                    if (inUseClosestTile) {
                        *outTile = theSector->mTiles.GetClosestPoint(theTo);
                        theResult = theTo.DistanceToSquaredF(*outTile) <= theRange;
                    }
                }
            }
        }
    }
    theRequest->UReferenceCounter::Dereference();
    return theResult;
}

// FUNCTION: EXE 0x63f630
// Tells whether a unit can be pushed aside: alive and not a building, aircraft, projectile or fixed object
bool EEIsUnitMovable(EEComplexUnit* inPlayer, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->GetUnitClass();
    if (inUnit->GetHitPoints() <= 0)
        return false;
    switch (theClass->GetCategory()) {
    case kUnitCategoryResource:
        if (theClass->mMaxVelocity == 0.0f)
            return false;
        break;
    case kUnitCategoryAmbient:
    case kUnitCategoryBuilding:
    case kUnitCategoryAirplane:
    case kUnitCategoryProjectile:
    case kUnitCategorySubmergedNaval:
    case kUnitCategoryBomb:
    case kUnitCategoryMissile:
    case kUnitCategoryBuilding19:
    case kUnitCategoryTorpedo:
    case kUnitCategoryMiningBuilding:
    case kUnitCategoryGate:
    case kUnitCategoryFortress:
    case kUnitCategoryBuildingPriest:
    case kUnitCategoryPlanetSpaceship:
        return false;
    }
    return true;
}

// FUNCTION: EXE 0x640ddb
// Finds a free spot for a unit on a tile, stepping in from a neighbour; false when every step collides
// A free location for a unit of inClass on tile (inToX, inToY), stepping in from (inX, inY) (the steps of 0x23f697);
// false when every step collides with a unit on the tile (or with the unit's active group's spots)
bool EEFindPlaceOnTile(EEComplexUnit* inUnit, EEUnitClass* inClass, long inX, long inY, long inSize, long inToX, long inToY,
                long inSquare, GE3DPoint* outLocation)
{
    EEUUNote theNote(4); theNote.U(inUnit).C(inClass).I(inX).I(inY).I(inSize).I(inToX).I(inToY).I(inSquare).OutP(outLocation);   // TEST ONLY (UUTL)
    float thePoint[3] = {outLocation->mX, outLocation->mY, outLocation->mZ};
    bool theFound = ee_units_uu_find_place_on_tile(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inClass), inX, inY, inSize, inToX,
                                                   inToY, inSquare, &thePoint) != 0;
    outLocation->mX = thePoint[0];
    outLocation->mY = thePoint[1];
    return theNote.R(theFound);
}

// FUNCTION: EXE 0x64116b
// Collects the tiles around a container unit for a unit, nearest first
void EEGetTilesAroundUnit(EEComplexUnit* inUnit, EEComplexUnit* inContainer, long inRange, long inCorners, bool inArg5,
                std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(5); theNote.U(inUnit).U(inContainer).I(inRange).I(inCorners).I(inArg5).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_tiles_around(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)), inContainer->mTileX,
                             inContainer->mTileY, inContainer->mUnitClass->mTileSize, inRange, inCorners, inArg5, &outTiles);
}

// FUNCTION: EXE 0x641299
// Collects the ring tiles at a range around a square, sorted by distance from a point
void EEGetSortedRingTiles(const GE3DPoint& inFrom, long inX, long inY, long inSize, long inRange, long inCorners,
                std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(6); theNote.P(inFrom).I(inX).I(inY).I(inSize).I(inRange).I(inCorners).OutT(&outTiles);   // TEST ONLY (UUTL)
    float theFrom[3] = {inFrom.mX, inFrom.mY, inFrom.mZ};
    ee_units_uu_sorted_ring_tiles(&theFrom, inX, inY, inSize, inRange, inCorners, &outTiles);
}

// FUNCTION: EXE 0x641419
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

// FUNCTION: EXE 0x6414d2
// Tells whether two techs count as the same research (0x432 and 0x5ab are twins)
bool EEIsSameResearch(long inFirst, long inSecond)
{
    bool theResult = inFirst == inSecond;
    if (theResult)
        return theResult;
    switch (inFirst) {
    case 0x432: theResult = inSecond == 0x5ab; break;
    case 0x5ab: theResult = inSecond == 0x432; break;
    }
    return theResult;
}

// FUNCTION: EXE 0x641504
// Picks a free spot next to a container among candidate tiles that the unit can stand on and reach
// A free location next to inContainer among inTiles for inUnit (its unit can stand there, the continents connect,
// 0x240ddb finds room, and unless inArg4 the tile path to the container is clear)
bool EEPickTileNear(std::vector<EETileCoordinate>& inTiles, EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIgnoreBlockedLine,
                GE3DPoint& outLocation)
{
    EEUUNote theNote(7); theNote.T(inTiles).U(inUnit).U(inContainer).I(inIgnoreBlockedLine).OutP(&outLocation);   // TEST ONLY (UUTL)
    float thePoint[3] = {outLocation.mX, outLocation.mY, outLocation.mZ};
    bool theFound = ee_units_uu_pick_tile_near(&inTiles, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)),
                                               inIgnoreBlockedLine, &thePoint) != 0;
    outLocation.mX = thePoint[0];
    outLocation.mY = thePoint[1];
    return theNote.R(theFound);
}

// FUNCTION: EXE 0x641658
// Tells whether a unit may stand on a tile: on the map, off its edge, allowed terrain, no blocking unit
// Whether inUnit may stand on inTile: on the map, not an edge tile, a terrain its class allows, the tile's water side
// for an amphibious class, and no unit on it that blocks inUnit (always for a class with flag 1 and no Virtual5C)
bool EECanStandOn(const EETileCoordinate& inTile, EEComplexUnit* inUnit)
{
    EEUUNote theNote(8); theNote.I(inTile.mX).I(inTile.mY).U(inUnit);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_can_stand_on(inTile.mX, inTile.mY, UUHandle(static_cast<EEUnit*>(inUnit))) != 0);
}

// FUNCTION: EXE 0x641743
// Computes the bounding rectangle of a list of units' tiles

void EEGetSelectionBounds(std::vector<EEUnit*>& inUnits, GERectangle& outBounds, long inTerrainFilter)
{
    EEUUNote theNote(54); theNote.Us(inUnits).I(inTerrainFilter).F(outBounds.GetLeft()).F(outBounds.GetTop()).F(outBounds.GetRight()).F(outBounds.GetBottom()).OutR(&outBounds);   // TEST ONLY (UUTL)
    float theBounds[4] = {outBounds.GetLeft(), outBounds.GetTop(), outBounds.GetRight(), outBounds.GetBottom()};
    ee_units_uu_selection_bounds(&inUnits, &theBounds, inTerrainFilter);   // ee_units uu_world.rs selection_bounds
    outBounds.SetLeft(theBounds[0]);
    outBounds.SetTop(theBounds[1]);
    outBounds.SetRight(theBounds[2]);
    outBounds.SetBottom(theBounds[3]);
}

// FUNCTION: EXE 0x64181a
// Computes where an attacker should aim to intercept a moving target (else its location)
// Where inAttacker should aim at a moving inTarget: the intercept point along the target's next waypoint (the
// quadratic of the two speeds), else the target's location
void EEGetLeadPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EECombatAttackPosition(false, inAttacker, inTarget, outLocation);   // ee_combat leaves.rs lead_position
}

// FUNCTION: EXE 0x641a6a
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

// FUNCTION: EXE 0x641bc9
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

// FUNCTION: EXE 0x641c2d
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

// FUNCTION: EXE 0x641e21
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

// FUNCTION: EXE 0x641e49
// Computes where an attacker should stand to fire on a target (healers and melee closer, others at range)
void EEGetFiringPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EECombatAttackPosition(true, inAttacker, inTarget, outLocation);   // ee_combat leaves.rs firing_position
}

// FUNCTION: EXE 0x642110
// Computes a moving unit's Doppler pitch shift against the camera
// The pitch shift of a moving unit against the camera (the Doppler factor of EEUCLandable::GetSoundParameters 0x2141a3, whose
// base call is the caller's here)
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    EEUUNote theNote(83); theNote.U(inUnit).F(*outVolume).F(*outPitch).OutF(outPitch);   // TEST ONLY (UUTL)
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    float theCamera[3] = {::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(), ::gWorld.mTransformation.GetZ()};
    ee_units_uu_doppler_pitch(*outVolume, theUnit->mVelocity, (const float(*)[4][4])&theUnit->mPhysicalModel.GetLocation().mMatrix[0][0],
                              &theCamera, outPitch);   // ee_units uu_world.rs doppler_pitch
}

// FUNCTION: EXE 0x642345
// Returns the angle between north and the direction from one point to the other
float EEGetAngleFromNorth(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    EEUUNote theNote(56); theNote.P(*inFrom).P(*inTo);   // TEST ONLY (UUTL)
    return theNote.RF(ee_units_uu_angle(1, 0, 0, (const float(*)[3])&inFrom->mX, (const float(*)[3])&inTo->mX));   // uu_world.rs
}

// FUNCTION: EXE 0x64240d
// Returns the ground heading from one point to another
float EEGetHeadingBetween(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    EEUUNote theNote(57); theNote.P(*inFrom).P(*inTo);   // TEST ONLY (UUTL)
    return theNote.RF(UMArcTan2(-(inTo->GetX() - inFrom->GetX()), inTo->GetY() - inFrom->GetY()));
}

// FUNCTION: EXE 0x642448
// Returns the heading from one tile to another
float EEGetSeaHeading(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo)
{
    EEUUNote theNote(58); theNote.I(inFrom.mX).I(inFrom.mY).I(inTo.mX).I(inTo.mY);   // TEST ONLY (UUTL)
    float theY = (float)inTo.GetY() - (float)inFrom.GetY();
    float theX = (float)inTo.GetX() - (float)inFrom.GetX();
    return theNote.RF(UMArcTan2(-theX, theY));
}


// (0x642489, EEGetUnitAngle, and the escape point's sort by it, EEUUAngleLess: ee_units uu_world.rs unit_angle)

// FUNCTION: EXE 0x6424f6
// Tells whether the target unit is attacking the unit
bool EEIsAttackedBy(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return EECombatWorld(EE_COMBAT_WORLD_ATTACKED_BY, inUnit, inTarget) != 0;   // ee_combat leaves.rs is_attacked_by
}

// FUNCTION: EXE 0x642542
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

// FUNCTION: EXE 0x642575
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

// FUNCTION: EXE 0x64282e
// Returns a launch facing for a ship leaving a dock (facing only)
bool EEGetDockShipFacing(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation)
{
    return EEGetDockShipPlacement(inPoint, inClass, inUnit, inFacePoint, true, outTransformation);
}

// FUNCTION: EXE 0x64284c
// Finds a launch position and facing for a ship leaving a dock
bool EEFindDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation)
{
    return EEGetDockShipPlacement(inPoint, inClass, inUnit, inFacePoint, false, outTransformation);
}

// FUNCTION: EXE 0x64286a
// Finds a launch position and facing for a unit leaving a dock or carrier: a free tile around the exit, then
// a turn until the way out is clear of shore, other areas and blocking units
// A launch facing for inUnit's unit (a unit of inClass leaving it): unless inArg5, first a free space tile within six
// rings around its exit point (fifteen-degree steps both ways off its facing); then, from the facing towards inPoint
// (inArg4) or its yaw, turn until the line out to the class's reach stays on the unit's continent and area, off shore
// tiles, with no blocking unit in the fan or along the unit's width. outTransformation gets the position and yaw;
// the unit's +0x224 the next start yaw (inferred)
bool EEGetDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, bool inArg5,
                GETransformation* outTransformation)
{
    EEUUNote theNote(9); theNote.P(*inPoint).C(inClass).U(inUnit).I(inFacePoint).I(inArg5).OutM(outTransformation).OutF(&inUnit->mComplexUnit->mLaunchYaw);   // TEST ONLY (UUTL)
    float thePoint[3] = {inPoint->mX, inPoint->mY, inPoint->mZ};
    return theNote.R(ee_units_uu_dock_ship_placement(&thePoint, UUHandle(inClass), UUHandle(inUnit), inFacePoint, inArg5,
                                                     (float(*)[4][4])&outTransformation->mMatrix[0][0]) != 0);
}

// FUNCTION: EXE 0x64352f
// Finds a free placement for a unit of a class around a tile
bool EEFindPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, unsigned long inX, unsigned long inY, long inSize,
                     long inRings, bool inCorners, bool inFacePoint, bool inArg11, GETransformation* outTransformation,
                     EEComplexUnit* inTarget)
{
    return EEFindPlacement(inPoint, NULL, inClass, inX, inY, inSize, inRings, inCorners, false, inFacePoint, inArg11,
                           outTransformation, inTarget);
}

// FUNCTION: EXE 0x643561
// Finds a free placement around a tile, ring by ring, reachable from the start's continent, with its facing
// A free place for a unit (inUnit, or one of inClass) around (inX, inY), ring by ring up to inRings, reachable from
// the start's continent; outTransformation faces it from inPoint (inArg10) or south, on the ground
bool EEFindPlacement(GE3DPoint* inPoint, EEComplexUnit* inUnit, EEUnitClass* inClass, unsigned long inX,
                     unsigned long inY, long inSize, long inRings, bool inCorners, bool inIncludeCenter, bool inFacePoint,
                     bool inArg11, GETransformation* outTransformation, EEComplexUnit* inTarget)
{
    EEUUNote theNote(10); theNote.P(*inPoint).U(inUnit).C(inClass).I(inX).I(inY).I(inSize).I(inRings).I(inCorners).I(inIncludeCenter).I(inFacePoint).I(inArg11).U(inTarget).OutM(outTransformation);   // TEST ONLY (UUTL)
    float thePoint[3] = {inPoint->mX, inPoint->mY, inPoint->mZ};
    return theNote.R(ee_units_uu_find_placement(&thePoint, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inClass), inX, inY, inSize,
                                                inRings, inCorners, inIncludeCenter, inFacePoint, inArg11,
                                                (float(*)[4][4])&outTransformation->mMatrix[0][0],
                                                UUHandle(static_cast<EEUnit*>(inTarget))) != 0);
}

// FUNCTION: EXE 0x643978
// Classifies a unit on a tile for a placement: 0 ignored, 1 movable, 2 blocking
// How inPlayer sees inUnit as a candidate of unit type inUnitType: 0 not at all, 1 a target, 2 otherwise (inferred)
char EEGetTileUnitStanding(EEUnit* inUnit, EEPlayer* inPlayer, long inUnitType, bool inIgnoreOwner)
{
    EEUUNote theNote(11); theNote.U(inUnit).Pl(inPlayer).I(inUnitType).I(inIgnoreOwner);   // TEST ONLY (UUTL)
    return theNote.R((char)ee_units_uu_tile_unit_standing(UUHandle(inUnit), UUHandle(inPlayer), inUnitType, inIgnoreOwner));
}

// FUNCTION: EXE 0x643ae1
// Turns wall pieces with three or more neighbours into plain junction pieces
// Wall pieces with three or more neighbours (inPlayer's live pieces of a wall class around them, or tiles of inTiles
// within one tile) become plain pieces facing south (inferred)
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap)
{
    std::vector<uint64_t> theUnits;
    for (size_t i = 0; i < inUnits.size(); i++)
        theUnits.push_back(UUHandle(static_cast<EEUnit*>(inUnits[i])));
    ee_units_uu_make_wall_junctions(theUnits.empty() ? NULL : &theUnits[0], (uint32_t)theUnits.size(), &inTiles, UUHandle(inPlayer),
                                    UUHandle(inMap));
}

// FUNCTION: EXE 0x643c7d
// Tells whether every tile of a rectangle can take a unit type for the unit's player
bool EEBlockTakes(EEUnit* inUnit, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    EEUUNote theNote(12); theNote.U(inUnit).I(inUnitType).I(inX).I(inY).I(inWidth).I(inHeight).I(inIgnoreOwner);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_block_takes(UUHandle(inUnit != NULL ? inUnit->mPlayer : NULL), inUnitType, inX, inY, inWidth, inHeight,
                                             inIgnoreOwner != 0) != 0);
}

// FUNCTION: EXE 0x643ce6
// Tells whether every tile of a rectangle can take a unit type for a player
// returns the result in al (EECPBuildManager tests it)
bool EEPlayerBlockTakes(EEPlayer* inPlayer, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    EEUUNote theNote(13); theNote.Pl(inPlayer).I(inUnitType).I(inX).I(inY).I(inWidth).I(inHeight).I(inIgnoreOwner);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_block_takes(UUHandle(inPlayer), inUnitType, inX, inY, inWidth, inHeight, inIgnoreOwner != 0) != 0);
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

// the circle screen of 0x24bf01 (ctor 0x24bfd3 out of line right after its first user; operator() 0x250d37 an inline
// COMDAT for EEGetUnitsInRectangle 0x250b99) (provisional names; moved here from ee/EEUnitUtilities.h)
struct EEUUCircleScreen {
    EEUUCircleScreen(const GE3DPoint& inCenter, float inRadius);
    bool operator()(EEComplexUnit* inUnit) const
    {
        return GE2DPoint(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY())
                   .DistanceToSquared(this->mCenter) <= this->mRadiusSquared;
    }
    GE2DPoint mCenter;                      // +0x00
    float mRadiusSquared;                   // +0x08
};

// FUNCTION: EXE 0x643d43
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

// FUNCTION: EXE 0x643f78
// Returns a target near a unit (along the unit list for anti-air ammunition 0x3c)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mAmmoType != 0x3c)
        return EEFindTargetNear(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), 1.0f);
    return EESearchTargetAlongList(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), 1.0f);
}

// FUNCTION: EXE 0x643fa7
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

// FUNCTION: EXE 0x64485b
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

// FUNCTION: EXE 0x64495a
// Sums the other players' influence values on a rectangle of tiles
long EEInfluenceTable::SumOthersValueOnTiles(unsigned long inPlayer, const U2DRectangle<long>& inTiles)
{
    return this->SumOthersValue(inPlayer, this->GetRegions(inTiles));
}

// FUNCTION: EXE 0x6449ca
// Computes a sound's volume, pan and pitch as heard from the camera (silent off tiles the player sees)
// Volume, pan and pitch of a sound of inVolume at inLocation, as heard from the camera (outPitch 1; volume 0 off a
// tile the current player sees)
void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFalloff)
{
    EEUUNote theNote(82); theNote.F(inVolume).P(inLocation).F(inFalloff).OutF(&outVolume, &outPan);   // TEST ONLY (UUTL)
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

// FUNCTION: EXE 0x644c83
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

// FUNCTION: EXE 0x644deb
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

// FUNCTION: EXE 0x644e95
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

// FUNCTION: EXE 0x645073
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

// FUNCTION: EXE 0x6451df
// Returns the compass sector (0..7) of the direction from one tile to another
long EEGetTileDirection(long inX1, long inY1, long inX2, long inY2)
{
    EEUUNote theNote(14); theNote.I(inX1).I(inY1).I(inX2).I(inY2);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_tile_direction(inX1, inY1, inX2, inY2));
}

// FUNCTION: EXE 0x645261
// Returns the compass sector of the direction from one unit to another
long EEGetDirectionTo(EEUnit* inFrom, EEUnit* inTo)
{
    return EEGetTileDirection(inFrom->GetTileX(), inFrom->GetTileY(), inTo->GetTileX(), inTo->GetTileY());
}

// FUNCTION: EXE 0x645284
// Unloads what a gatherer carries into a container: the player's resources (with the variant's rate and a
// nearby-site bonus), the effect and the statistics
// inUnit unloads what it carries into inContainer: the player's resources (the variant's rate; a bonus when the
// resource site is near), the effect and the statistics
void EEDepositLoad(EEComplexUnit* inUnit, EEComplexUnit* inContainer)
{
    EEUUNote theNote(84); theNote.U(inUnit).U(inContainer).I(inUnit->mResourceAmount).I((long)inUnit->mResourceType);   // TEST ONLY (UUTL)
    ee_units_uu_deposit_load(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inContainer)));   // uu_world.rs
}

// FUNCTION: EXE 0x6454eb
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

// FUNCTION: EXE 0x64572b
// Splits calamity flags into their single bits
void EESplitCalamityFlags(unsigned long inFlags, std::vector<unsigned long>& outFlags)
{
    if ((inFlags & 1) != 0)
        outFlags.push_back(1);
    if ((inFlags & 2) != 0)
        outFlags.push_back(2);
    if ((inFlags & 4) != 0)
        outFlags.push_back(4);
    if ((inFlags & 8) != 0)
        outFlags.push_back(8);
    if ((inFlags & 0x10) != 0)
        outFlags.push_back(0x10);
}

// FUNCTION: EXE 0x6457a3
// Walks a tile path back from its end and drops the middle tile wherever two tiles one step apart
// differ by exactly (+1, +1), cutting diagonal corners
void EEDropDiagonalCorners(std::vector<EETileCoordinate>& ioTiles)
{
    EEUUNote theNote(15); theNote.T(ioTiles).OutT(&ioTiles);   // TEST ONLY (UUTL)
    ee_units_uu_drop_tiles(0, &ioTiles);
}

// FUNCTION: EXE 0x645903
// Collects the player's town centers on the corners and sides of a farm's footprint
bool EEGetFarmDropSites(EEComplexUnit* inUnit, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(16); theNote.U(inUnit).Us(outUnits).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_farm_drop_sites(UUHandle(static_cast<EEUnit*>(inUnit)), &outUnits) != 0);
}

// FUNCTION: EXE 0x645a19
// Finds the nearest free farm of the unit's player, ring by ring (the ring reached goes back)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioRing)
{
    EEUUNote theNote(17); theNote.U(inUnit).I(inFreeOnly).I(*ioRing).OutL(ioRing);   // TEST ONLY (UUTL)
    int32_t theRing = (int32_t)*ioRing;
    EEUnit* theFarm = UUUnit(ee_units_uu_find_free_farm(UUHandle(static_cast<EEUnit*>(inUnit)), inFreeOnly, &theRing));
    *ioRing = theRing;
    return theNote.RU(theFarm != NULL ? theFarm->mComplexUnit : NULL);
}

// FUNCTION: EXE 0x645bc4
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

// FUNCTION: EXE 0x645dd6
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

// FUNCTION: EXE 0x646254
// Finds a place for a computer player's unit to flee to, away from its attackers
// A place for a computer player's unit to move off to, away from the units attacking it: the middle of the widest
// gap between them pushed out to its reach, else a step away from the first attacker (EEUnitClass::FindSafeLocation)
bool EEGetEscapePoint(EEComplexUnit* inUnit, GE2DPoint& outLocation)
{
    EEUUNote theNote(18); theNote.U(inUnit).OutF(&outLocation.mX, &outLocation.mY);   // TEST ONLY (UUTL)
    float thePoint[2] = {outLocation.mX, outLocation.mY};
    bool theResult = ee_units_uu_escape_point(UUHandle(static_cast<EEUnit*>(inUnit)), &thePoint) != 0;   // uu_world.rs
    outLocation.mX = thePoint[0];
    outLocation.mY = thePoint[1];
    return theNote.R(theResult);
}

// FUNCTION: EXE 0x6466fe
// Runs a functor over the units of every player but nature
void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor)
{
    for (unsigned long i = 1; i < ::gWorld.mNumPlayers; i++)
        EEForEachUnitOf(::gWorld.mPlayers[i], inFunctor, false);
}

// FUNCTION: EXE 0x64672f
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

// FUNCTION: EXE 0x6467bb
// Returns the squared ground distance between two units
float EEGetDistanceSquared(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    GE2DPoint theFirst(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY());
    GE2DPoint theSecond(inOther->mPhysicalModel.GetLocation().GetX(), inOther->mPhysicalModel.GetLocation().GetY());
    return theFirst.DistanceToSquared(theSecond);
}

// FUNCTION: EXE 0x64680c
// Returns the bounds of a unit's area effect of a kind, grown by its radius; false when it has none
bool EEGetAreaEffectBounds(long inKind, EEComplexUnit* inUnit, EEAIAStarBounds* outBounds)
{
    bool theFound = false;
    outBounds->Clear();
    if (inUnit != NULL) {
        for (std::vector<EEAreaEffect*>::iterator it = inUnit->mAreaEffects.mItems.begin();
             it != inUnit->mAreaEffects.mItems.end(); it++) {
            if ((*it)->mTemplate->mKind != inKind)
                continue;
            theFound = true;
            long theRadius = UMFloatToLong((*it)->GetRadius());
            outBounds->mTop = inUnit->GetTileY();
            outBounds->mLeft = inUnit->GetTileX();
            outBounds->mBottom = inUnit->GetTileY();
            outBounds->mRight = inUnit->GetTileX();
            outBounds->Grow(theRadius, true);
            break;
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x646889
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

// FUNCTION: EXE 0x64697b
// Collects the tiles around a rectangle (sides only, no corners)
void EEGetSquareRing(long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(19); theNote.I(inX).I(inY).I(inWidth).I(inHeight).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_square_ring(inX, inY, inWidth, inHeight, &outTiles);
}

// (the EETileCoordinate twin of 0x64697b is gone: one tile type)

// FUNCTION: EXE 0x646a26
// Collects the tiles around a square (sides only)
void EEGetSideTiles(long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(20); theNote.I(inX).I(inY).I(inSize).OutT(&outTiles);   // TEST ONLY (UUTL)
    EEGetSquareRing(inX, inY, inSize, inSize, outTiles);
}

// FUNCTION: EXE 0x646a42
// Collects the tiles around a rectangle, with corners or only the corners not closed by two wall pieces
// The tiles around the inWidth x inHeight rectangle at (inX, inY); the corners too when inCorners, else only those
// not closed by two wall pieces (appended to outTiles when inAppend)
void EEGetBlockRing(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<EETileCoordinate>& outTiles,
                bool inAppend)
{
    EEUUNote theNote(21); theNote.I(inCorners).I(inX).I(inY).I(inWidth).I(inHeight).I(inAppend).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_block_ring(inCorners, inX, inY, inWidth, inHeight, &outTiles, inAppend);
}

// FUNCTION: EXE 0x646bd0
// Collects the tiles around a square, corners as asked
void EEGetSquareBlockRing(long inCorners, long inX, long inY, long inSize, std::vector<EETileCoordinate>& outTiles, bool inAppend)
{
    EEUUNote theNote(22); theNote.I(inCorners).I(inX).I(inY).I(inSize).I(inAppend).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    EEGetBlockRing(inCorners, inX, inY, inSize, inSize, outTiles, inAppend);
}

// FUNCTION: EXE 0x646bf2
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

// FUNCTION: EXE 0x646c87
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

// FUNCTION: EXE 0x646ea2
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

// FUNCTION: EXE 0x646edc
// Tells whether both tiles hold a wall piece
bool EEIsCornerBlocked(unsigned long inX1, unsigned long inY1, unsigned long inX2, unsigned long inY2)
{
    EEUUNote theNote(23); theNote.I(inX1).I(inY1).I(inX2).I(inY2);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_is_corner_blocked((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2) != 0);
}

// FUNCTION: EXE 0x646f93
// Returns the nearest piece of a target's linked group that the unit may attack
EEComplexUnit* EEGetAttackPart(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_uu_attack_part(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inTarget)))));
}

// FUNCTION: EXE 0x647021
// Collects the four corner tiles of a rectangle (only the open ones unless corners are asked)
// The four corner tiles of the inWidth x inHeight rectangle at (inX, inY); with inCorners false only those not closed
// by two wall pieces
void EEGetRingCorners(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(24); theNote.I(inCorners).I(inX).I(inY).I(inWidth).I(inHeight).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_ring_corners(inCorners, inX, inY, inWidth, inHeight, &outTiles);
}


// (0x647125, EEGetEnemiesAround: ee_units uu_world.rs enemies_around)

// FUNCTION: EXE 0x64720e
// Collects the corner tiles of a square
void EEGetCornerTiles(long inCorners, long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(25); theNote.I(inCorners).I(inX).I(inY).I(inSize).OutT(&outTiles);   // TEST ONLY (UUTL)
    EEGetRingCorners(inCorners, inX, inY, inSize, inSize, outTiles);
}

// FUNCTION: EXE 0x64722d
// Tells whether a unit can attack a target from where it stands (in range, not blocked)
bool EEIsInRangeToHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, inUnit, inTarget) != 0;   // ee_combat leaves.rs in_range_to_hit
}

// (0x647316, EECanReach: ee_combat's can_reach, called by leaves.rs in_contact_range)

// FUNCTION: EXE 0x64734c
// Tells whether a unit is close enough to a target to build or repair it
bool EEIsInWorkReach(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    EEUnitClass* theClass = inTarget->mUnitClass;
    EeCombatReachIn theIn = {0.0f, inUnit->mUnitClass->mInnerCollisionSphere, theClass->mInnerCollisionSphere, 0.0f,
                             theClass->mIsWall, theClass->mUnitType == kUnitTypeRoboticFarm};
    theIn.distance_squared = inUnit->DistanceSquaredTo(inTarget);
    return EECombatWorkReach(theIn) != 0;
}

// FUNCTION: EXE 0x6473e0
// Tells whether a unit at a squared distance reaches a target (gather and arrival test)
bool EEIsInGatherReach(float inDistanceSquared, EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EeCombatReachIn theIn = {inDistanceSquared, inUnit->mUnitClass->mInnerCollisionSphere,
                             inTarget->mUnitClass->mInnerCollisionSphere, 0.0f, inTarget->mUnitClass->mIsTree,
                             inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen};
    return EECombatGatherReach(theIn) != 0;
}

// FUNCTION: EXE 0x64745d
// Tells whether a gatherer has arrived at its target (2D distance to the target's position)
// inUnit at inTarget (the gather goal's arrival): the 2D distance to the target's position (a dead unit's model
// point, a tile centre for an unplaced one)
bool EEIsWithinReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEUUNote theNote(60); theNote.U(inUnit).U(inTarget);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_within_reach(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inTarget), NULL) != 0);   // uu_world.rs
}

// FUNCTION: EXE 0x6475a0
// Tells whether a location is within a unit's reach of a target
bool EEIsWithinReachOf(const GE3DPoint& inLocation, EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEUUNote theNote(61); theNote.P(inLocation).U(inUnit).U(inTarget);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_within_reach(UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inTarget),
                                              (const float(*)[3])&inLocation.mX) != 0);   // uu_world.rs
}

// FUNCTION: EXE 0x6476c5
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

// FUNCTION: EXE 0x647772
// Tells whether a unit touches a target (collision spheres; walls count as radius 1)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared)
{
    long theBits = 0;   // ee_combat leaves.rs in_contact_range: the distance's bits as the tile's x
    memcpy(&theBits, &inDistanceSquared, 4);
    return EECombatWorld(EE_COMBAT_WORLD_CONTACT_RANGE, inUnit, inTarget, NULL, &theBits, &theBits) != 0;
}

// the targetable tests' record (ee_combat leaves.rs: targetable, can_target_class); a NULL class is never targetable
static bool EETargetable(EEUnit* inAttacker, EEUnitClass* inClass, bool inCanTarget)
{
    if (inClass == NULL)
        return false;
    EeCombatTargetableIn in = {inAttacker != NULL, 0, 0, 0, (int32_t)inClass->mCategory, (uint32_t)inClass->mUnitType,
                               (uint32_t)inClass->mFamily, inClass->CanBeAttacked()};
    if (inAttacker != NULL) {
        in.ammo_type = (int32_t)inAttacker->mUnitClass->mAmmoType;
        in.land_building = inAttacker->mUnitClass->IsLandBuilding();
        in.computer = inAttacker->mPlayer != NULL && inAttacker->mPlayer->mComputerPlayer != NULL;
    }
    return ee_combat_targetable(&in, inCanTarget);
}

// FUNCTION: EXE 0x6477f9
// Tells whether a unit may target a class (walls only by siege units or computer players)
bool EECanTargetClass(EEComplexUnit* inUnit, EEUnitClass* inClass)
{
    return EETargetable(inUnit, inClass, true);
}

// FUNCTION: EXE 0x647848
// Tells whether a class can be targeted at all (by an attacker's ammunition when given)
bool EEUUGetIsUnitClassTargetable(EEUnit* inAttacker, EEUnitClass* inClass)
{
    return EETargetable(inAttacker, inClass, false);
}

// FUNCTION: EXE 0x6478f7
// Tells whether a unit is a cloaked enemy of a player
bool EEIsCloakedEnemyOf(EEUnit* inUnit, EEPlayer* inPlayer)
{
    return inUnit->mTileNode != NULL
        && (inUnit->mComplexUnit->mIsCloakable ? inUnit->mComplexUnit->mIsCloaked : false)
        && inPlayer != inUnit->mPlayer && inUnit->mPlayer->mEnemies[inPlayer->GetPlayerID()] != 0;
}

// FUNCTION: EXE 0x64793c
// Tells whether a player sees any tile of a unit's footprint
bool EEIsFootprintVisibleTo(EEUnit* inUnit, EEPlayer* inPlayer)
{
    EEUUNote theNote(62); theNote.U(inUnit).Pl(inPlayer);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_footprint_visible(UUHandle(inUnit), UUHandle(inPlayer)) != 0);   // uu_world.rs
}

// FUNCTION: EXE 0x6479eb
// Tells whether a player sees a unit (explored may be enough; cloaked enemies never)
// Whether inPlayer sees inUnit (inExplored: explored is enough; a cloaked enemy is never seen; any tile of a large
// unit's footprint)
bool EEIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
{
    return EECombatIsSeenBy(inUnit, inPlayer, inExplored);   // ee_combat leaves.rs is_seen_by
}

// FUNCTION: EXE 0x647bd8
// Tells whether the owner of another unit sees the unit (explored is enough)
bool EEIsSeenByOwnerOf(EEComplexUnit* inUnit, EEUnit* inOther)
{
    return EEIsSeenBy(inUnit, inOther->GetPlayer(), true);
}

// FUNCTION: EXE 0x647bef
// Extends a wall line to the walls of the same type it joins at either end
// The tile line of a wall through inTiles (from the walls of inClass's type it joins at either end) (inferred)
long EEExtendWallLine(std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer, EEUnitClass* inClass,
                    std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(26); theNote.T(inTiles).Pl(inPlayer).C(inClass).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_extend_wall_line(&inTiles, UUHandle(inPlayer), (uint32_t)inClass->mUnitType, &outTiles));
}

// TEMPLATE: EXE 0x6481da
// (the reverse_iterator copy ctor, out of line right after its first user 0x247bef)

// FUNCTION: EXE 0x648709
// Finds a place for a unit next to a target, preferring its own path area and continent
// A place for inUnit next to inTarget, preferring tiles in inUnit's own path area and continent (inferred)
bool EEFindResourceSpot(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation)
{
    EEUUNote theNote(27); theNote.U(inUnit).U(inTarget).OutP(&outLocation);   // TEST ONLY (UUTL)
    float thePoint[3] = {outLocation.mX, outLocation.mY, outLocation.mZ};
    bool theFound = ee_units_uu_spot(1, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inTarget)), &thePoint) != 0;
    outLocation.mX = thePoint[0];
    outLocation.mY = thePoint[1];
    outLocation.mZ = thePoint[2];
    return theNote.R(theFound);
}

// FUNCTION: EXE 0x64899b
// Finds a free spot next to a target (or the nearest piece of its group) for a unit
bool EEFindNextFreeTile(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation)
{
    EEUUNote theNote(28); theNote.U(inUnit).U(inTarget).OutP(&outLocation);   // TEST ONLY (UUTL)
    float thePoint[3] = {outLocation.mX, outLocation.mY, outLocation.mZ};
    bool theFound = ee_units_uu_spot(2, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inTarget)), &thePoint) != 0;
    outLocation.mX = thePoint[0];
    outLocation.mY = thePoint[1];
    outLocation.mZ = thePoint[2];
    return theNote.R(theFound);
}

// FUNCTION: EXE 0x648c7e
// Finds a place for a unit to attack a target from (around a wall it stands in, or next to it)
bool EEGetWallSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    EEUUNote theNote(29); theNote.U(inUnit).U(inTarget).OutP(&outLocation);   // TEST ONLY (UUTL)
    float thePoint[3] = {outLocation.mX, outLocation.mY, outLocation.mZ};
    bool theFound = ee_units_uu_spot(3, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(inTarget), &thePoint) != 0;
    outLocation.mX = thePoint[0];
    outLocation.mY = thePoint[1];
    outLocation.mZ = thePoint[2];
    return theNote.R(theFound);
}

// FUNCTION: EXE 0x648d05
// Returns a unit's radius seen from another: full in front or behind, down to a tenth from the side
float EEGetUnitRadius(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    EEUUNote theNote(55); theNote.U(inUnit).U(inOther);   // TEST ONLY (UUTL)
    return theNote.RF(ee_units_uu_angle(0, UUHandle(static_cast<EEUnit*>(inUnit)), UUHandle(static_cast<EEUnit*>(inOther)), NULL, NULL));
}

// FUNCTION: EXE 0x648ecc
// Returns the tile a distance away in a compass direction (tile form)
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance)
{
    EEUUNote theNote(30); theNote.I(inTile.mX).I(inTile.mY).I(inDirection).I(inDistance).OutL(&outTile.mX, &outTile.mY);   // TEST ONLY (UUTL)
    int32_t theTile[2];
    ee_units_uu_tile_in_direction(inTile.mX, inTile.mY, inDirection, inDistance, &theTile);
    outTile.mX = theTile[0];
    outTile.mY = theTile[1];
}

// (the U2DPoint<long> twin of 0x648ecc is gone: one tile type)

// FUNCTION: EXE 0x648efd
// Moves a point a number of tiles in a compass direction, keeping its offset within the tile
void EEMoveInDirection(const GE2DPoint& inFrom, GE2DPoint& outTo, long inDirection, long inDistance)
{
    EEUUNote theNote(31); theNote.F(inFrom.mX).F(inFrom.mY).I(inDirection).I(inDistance).OutF(&outTo.mX, &outTo.mY);   // TEST ONLY (UUTL)
    float thePoint[2];
    ee_units_uu_move_in_direction(inFrom.mX, inFrom.mY, inDirection, inDistance, &thePoint);
    outTo.SetX(thePoint[0]);
    outTo.SetY(thePoint[1]);
}

// FUNCTION: EXE 0x648f86
// Turns a compass direction by a random amount within a spread
long EETurnAway(long inDirection, long inSpread)
{
    EEUUNote theNote(32); theNote.I(inDirection).I(inSpread);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_turn_away(inDirection, inSpread));
}

// FUNCTION: EXE 0x648fe4
// Collects the tiles a unit covers, widened by a margin (a line along a ship's heading, else a disc)
// The tiles inUnit covers, widened by inMargin (a line along a ship's heading; a disc otherwise) (inferred)
void EEGetUnitTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles, float inMargin)
{
    EEUUNote theNote(53); theNote.U(inUnit).F(inMargin).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_unit_tiles_margin(UUHandle(inUnit), inMargin, &outTiles);   // ee_units uu_world.rs unit_tiles
}

// FUNCTION: EXE 0x6491c9
// Returns a random number in [min, max]
long EEGetRandomInt(long inMin, long inMax, URandom& inRandom)
{
    long theEnd = inMax + 1;
    long theResult = UMFloatToLong(inRandom.Generate() * (theEnd - inMin) + inMin);
    if (theResult == theEnd)
        theResult = inMax;
    return theResult;
}

// FUNCTION: EXE 0x649201
// Picks a random tile of the bounds that a screen accepts; (0, 0) when none in the tries
bool EEFOFindLocation(EEAIAStarBounds& inBounds, U2DPoint<long>& outTile, URandom& inRandom,
                      EEFOLocationScreenBase& inScreen, long inTries)
{
    bool theFound = false;
    U2DPoint<long> theTile(0, 0);
    outTile.mX = 0;
    outTile.mY = 0;
    for (long i = inTries - 1; i >= 0; i--) {
        if (theFound)
            break;
        long theY = EEGetRandomInt(inBounds.mTop, inBounds.mBottom, inRandom);
        theTile.mX = EEGetRandomInt(inBounds.mLeft, inBounds.mRight, inRandom);
        theTile.mY = theY;
        theFound = inScreen.Screen(theTile);
    }
    if (theFound)
        outTile = theTile;
    return theFound;
}

// FUNCTION: EXE 0x64927c
// Returns the id of a player's leading unit, -1 when none
unsigned long EEGetLeadUnitID(EEPlayer* inPlayer)
{
    RTSUnit* theUnit = NULL;
    unsigned long theResult = (unsigned long)-1;
    if (inPlayer->mSelectedGroup->GetNumUnits() != 0)
        theUnit = inPlayer->mSelectedGroup->GetUnit(0);
    if (inPlayer->mSelectedViewedUnit != NULL)
        theUnit = inPlayer->mSelectedViewedUnit;
    if (theUnit != NULL)
        theResult = theUnit->mUnitID;
    return theResult;
}

// FUNCTION: EXE 0x6492c2
// Collects the ring of tiles at a distance around a tile
void EEGetSquareRingTiles(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(33); theNote.I(inX).I(inY).I(inRadius).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_square_ring_tiles(inX, inY, inRadius, &outTiles);
}

// FUNCTION: EXE 0x64939d
// Finds where a dropped unit lands on a target: the model's drop point (lower when damaged) or its tile
// Where a unit dropped onto inTarget (its goal's target when NULL) lands: the model's drop point (scaled down with
// a damaged unit's health), else its tile; *outTarget the target (NULL when not placed)
void EEFindDropLocation(EEComplexUnit* inUnit, EEUnit** outTarget, GE3DPoint* outLocation, EEUnit* inTarget)
{
    *outTarget = EECombatFindDropLocation(inUnit, inTarget, *outLocation);   // ee_combat leaves.rs find_drop_location
}

// FUNCTION: EXE 0x6494cb
// Returns the squared ground distance from a unit (or a point) to where it would hit a target
float EEGetHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
{
    return EECombatHitDistanceSquared(inUnit, inTarget, inX, inY);   // ee_combat leaves.rs hit_distance_squared
}

// FUNCTION: EXE 0x649560
// Returns a point 90% of a unit's range from a target towards the unit
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outPoint)
{
    EEUUNote theNote(63); theNote.U(inUnit).P(*inTarget).OutP(outPoint);   // TEST ONLY (UUTL)
    GE3DPoint thePosition;
    GEVector theOffset;
    inUnit->mPhysicalModel.GetLocation().GetPosition(thePosition);
    GEVector theDirection(thePosition.GetX() - inTarget->GetX(), thePosition.GetY() - inTarget->GetY(),
                          thePosition.GetZ() - inTarget->GetZ());
    theDirection.Normalize();
    theDirection.Scale(inUnit->mStats->mRange * 0.9f, theOffset);
    outPoint->SetX(inTarget->GetX() + theOffset.GetX());
    outPoint->SetY(inTarget->GetY() + theOffset.GetY());
    outPoint->SetZ(inTarget->GetZ() + theOffset.GetZ());
}

// FUNCTION: EXE 0x6495f5
// Returns where a unit casts its calamity from: 90% of the calamity's range from the target
void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    EEUUNote theNote(64); theNote.U(inUnit).P(outPoint).OutP(&outPoint);   // TEST ONLY (UUTL)
    GEVector theDirection;
    GEVector theOffset;
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    EEUGRust* theGoal = EECastCalamityGoalCast(inUnit->GetGoal());
    theGoal->GetGoalLocation(theTarget);
    long theType = theGoal->GetCalamityType();
    EETechTree* theTree = inUnit->mPlayer->mTechnologies;
    EETechnology* theTechnology = theTree->Get(theType);
    if (theTechnology == NULL)
        return;
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
}

// FUNCTION: EXE 0x649727
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

// FUNCTION: EXE 0x64b212
// Collects the tiles whose nearest point lies within a distance of a point
void EEGetTilesNearPoint(const GE3DPoint& inPoint, float inDistance, std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(34); theNote.P(inPoint).F(inDistance).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_tiles_near_point(inPoint.mX, inPoint.mY, inDistance, &outTiles);
}

// FUNCTION: EXE 0x64b556
// Collects the square of tiles within a radius of a tile
void EEGetTilesInRadius(long inX, long inY, unsigned long inRadius, bool inInclusive,
                        std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(35); theNote.I(inX).I(inY).I((long)inRadius).I(inInclusive).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_tiles_in_radius(inX, inY, (uint32_t)inRadius, inInclusive, &outTiles);
}

// (the U2DPoint<long> twin of 0x64b556 is gone: one tile type)

// FUNCTION: EXE 0x64b5b3
// Collects the tiles of a unit's footprint (a gate's pieces; a long wonder's full length)
void EEGetFootprintTiles(EEUnit* inUnit, std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(36); theNote.U(inUnit).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_unit_tiles(0, UUHandle(inUnit), &outTiles);
}

// FUNCTION: EXE 0x64b747
// Collects the tiles of a wall line between two points
void EEGetWallLineXY(const GE2DPoint& inFrom, const GE2DPoint& inTo, std::vector<EETileCoordinate>& outTiles)
{
    EETileCoordinate theFrom;
    EETileCoordinate theTo;
    theFrom.mX = (long)inFrom.GetX();
    theFrom.mY = (long)inFrom.GetY();
    theTo.mX = (long)inTo.GetX();
    theTo.mY = (long)inTo.GetY();
    EEGetWallLine(theFrom, theTo, outTiles);
}

// FUNCTION: EXE 0x64b794
// Collects the tiles of a wall line between two tiles
void EEGetWallLine(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, std::vector<EETileCoordinate>& outTiles)
{
    EEUUNote theNote(37); theNote.I(inFrom.mX).I(inFrom.mY).I(inTo.mX).I(inTo.mY).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_wall_line(inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, &outTiles);
}

// FUNCTION: EXE 0x64b7e9
// Collects the units on a list of tiles (sorted and unique when a large one is among them)
long EEGetUnitsOnTiles(std::vector<EETileLocation>& inTiles, std::vector<EEUnit*>& outUnits)
{
    return ee_units_uu_units_on(1, &inTiles, &outUnits);
}

// FUNCTION: EXE 0x64b8ac
// Returns the path distance from a unit to a tile (10000 when unreachable) and the path's first tile
// The path distance from inUnit to ioTile (10000 when the path does not reach it; ioTile becomes the path's first
// tile, or (-1, -1)); inWalls lets walls not block (inferred)
float EEGetPathLengthTo(EEComplexUnit* inUnit, EETileCoordinate& ioTile, unsigned long inLimit, bool inWalls)
{
    EEUUNote theNote(80); theNote.U(inUnit).I(ioTile.mX).I(ioTile.mY).I((long)inLimit).I(inWalls).OutL(&ioTile.mX, &ioTile.mY);   // TEST ONLY (UUTL)
    int32_t theTile[2] = {(int32_t)ioTile.mX, (int32_t)ioTile.mY};
    float theResult = ee_units_uu_path_length_to(UUHandle(static_cast<EEUnit*>(inUnit)), &theTile, (uint32_t)inLimit, inWalls);
    ioTile.mX = theTile[0];
    ioTile.mY = theTile[1];
    return theNote.RF(theResult);   // ee_units uu_world.rs path_length_to
}

// FUNCTION: EXE 0x64baf6
// Tells whether a tile lies on the diamond map's edge
bool RTSMap::IsEdgeTile(const EETileLocation& inTile)
{
    long theY = inTile.mY;
    long theX = inTile.mX;
    return theX + theY == this->mL195608 || theY - theX == this->mL195610 || theX + theY == this->mL19560C || theX - theY == this->mL195610;
}

// FUNCTION: EXE 0x64bf01
// Collects the units within a radius of a point; their number
long EEGetUnitsNear(const GE3DPoint& inCenter, float inRadius, std::vector<EEUnit*>& outUnits)
{
    EEUUCircleScreen theScreen(inCenter, inRadius);
    U2DRectangle<long> theArea;
    outUnits.clear();
    theArea.mTop = UMFloor(inCenter.GetY() - inRadius);
    theArea.mLeft = UMFloor(inCenter.GetX() - inRadius);
    theArea.mBottom = UMCeil(inCenter.GetY() + inRadius);
    theArea.mRight = UMCeil(inCenter.GetX() + inRadius);
    return EEGetUnitsInRectangle(theArea, outUnits, theScreen, true);
}

// FUNCTION: EXE 0x64bfd3
// Creates a circle screen for a centre and radius
EEUUCircleScreen::EEUUCircleScreen(const GE3DPoint& inCenter, float inRadius)
{
    this->mCenter = inCenter.Get2DPoint();
    this->mRadiusSquared = inRadius * inRadius;
}

// FUNCTION: EXE 0x64c146
// Collects the units on the tiles of a screen rectangle; their number
long EEGetUnitsInRectangle(const GERectangle& inRectangle, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(38); theNote.F(inRectangle.GetLeft()).F(inRectangle.GetTop()).F(inRectangle.GetRight()).F(inRectangle.GetBottom()).Us(outUnits).OutU(&outUnits);   // TEST ONLY (UUTL)
    std::vector<U2DPoint<long> > theTiles;
    long theRight = UMFloor(inRectangle.GetRight());
    long theBottom = UMFloor(inRectangle.GetBottom());
    long theLeft = UMFloor(inRectangle.GetLeft());
    long theTop = UMFloor(inRectangle.GetTop());
    theTiles.reserve((theBottom - theTop) * (theRight - theLeft));
    long theX = theLeft;
    while (true) {
        if (theX > theRight)
            break;
        for (long theY = theTop; theY <= theBottom; theY++)
            theTiles.push_back(U2DPoint<long>(theX, theY));
        theX++;
    }
    return theNote.R(EEGetUnitsOnTiles(theTiles, outUnits));
}

// FUNCTION: EXE 0x64c22a
// Collects the units within a radius of a point (the square of tiles around it); their number
// The units in inRadius of inCenter (the square of tiles around it; one less on the far side unless the point is
// near a tile's middle)
long EEGetUnitsInRadius(GE2DPoint inCenter, float inRadius, std::vector<EEUnit*>& outUnits)
{
    U2DRectangle<long> theArea;
    EEAnyUnit theFilter;
    long theY = UMFloor(inCenter.GetY());
    long theX = UMFloor(inCenter.GetX());
    float theFractionX = inCenter.GetX() - (float)theX;
    float theFractionY = inCenter.GetY() - theY;
    long theRadius = UMFloatToLong(inRadius);
    long theFar;
    outUnits.clear();
    if (theFractionX <= 0.25f || theFractionX >= 0.75f || theFractionY <= 0.25f || theFractionY >= 0.75f)
        theFar = theRadius - 1;
    else
        theFar = theRadius;
    if (theRadius >= 0) {
        theArea.mTop = theY - theRadius;
        theArea.mBottom = theY + theFar;
        theArea.mRight = theX + theFar;
        theArea.mLeft = theX - theRadius;
        return EEGetUnitsInRectangle(theArea, outUnits, theFilter, true);
    }
    return 0;
}

// FUNCTION: EXE 0x64c312
// Returns the living unit of a type on a tile, or NULL
EEUnit* EEGetTileUnitOfType(const EETileCoordinate& inTile, unsigned long inUnitType)
{
    EEUUNote theNote(39); theNote.I(inTile.mX).I(inTile.mY).I((long)inUnitType);   // TEST ONLY (UUTL)
    return theNote.RU(UUUnit(ee_units_uu_tile_unit_of_type(inTile.mX, inTile.mY, (uint32_t)inUnitType, 0, 0)));
}

// FUNCTION: EXE 0x64c395
// Collects a player's units of a type; their number
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayer, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(40); theNote.I(inUnitType).I(inPlayer).Us(outUnits).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_player_units_of_type(inUnitType, inPlayer, &outUnits));
}

// FUNCTION: EXE 0x64c3df
// Collects the units on a tile; their number
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(41); theNote.I(inX).I(inY).OutU(&outUnits);   // TEST ONLY (UUTL)
    std::vector<U2DPoint<long> > theTiles;
    outUnits.clear();
    theTiles.push_back(U2DPoint<long>(inX, inY));
    return theNote.R(EEGetUnitsOn(theTiles, outUnits));
}

// FUNCTION: EXE 0x64c433
// Collects the units on a list of tiles
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(42); theNote.T(inTiles).Us(outUnits).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_units_on(0, &inTiles, &outUnits));
}

// FUNCTION: EXE 0x64c4be
// Returns a player's living unit of a type on a tile, or NULL
EEUnit* EEGetWallAt(const EETileCoordinate& inTile, EEPlayer* inPlayer, long inUnitType)
{
    return UUUnit(ee_units_uu_tile_unit_of_type(inTile.mX, inTile.mY, (uint32_t)inUnitType, UUHandle(inPlayer), 1));
}

// FUNCTION: EXE 0x64c627
// Collects the units of a type on a list of tiles; their number
long EEGetWallUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    EEUUNote theNote(43); theNote.T(inTiles).I(inUnitType).Us(outUnits).OutU(&outUnits);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_units_of_type_on_tiles(&inTiles, 0, 0, inUnitType, &outUnits));
}

// FUNCTION: EXE 0x64c6fc
// Collects the free tiles around a unit (its ring, corners included)
void EEGetResourceSpots(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles)
{
    EEUUNote theNote(44); theNote.U(inUnit).T(outTiles).OutT(&outTiles);   // TEST ONLY (UUTL)
    ee_units_uu_unit_tiles(1, UUHandle(inUnit), &outTiles);
}


// FUNCTION: EXE 0x64c8bc
// Enters a fortress into the unit regions under its footprint, clearing ambient units outside a load
// inUnit (a fortress) entered in the unit regions under its footprint; outside a load, the ambient units on its
// tiles (and its carriers') are cleared (inferred)
void EEAddBuildingRegion(EEUnit* inUnit)
{
    EEUUNote theNote(69); theNote.U(inUnit).OutFoot(inUnit->GetTileX(), inUnit->GetTileY(), inUnit->GetUnitClass()->mTileSize);   // TEST ONLY (UUTL)
    ee_units_uu_footprint(2, UUHandle(inUnit));   // ee_units uu_world.rs building_region
}

// FUNCTION: EXE 0x64caef
// Adds a unit to every tile of its footprint
void EEAddToFootprintTiles(EEUnit* inUnit)
{
    EEUUNote theNote(66); theNote.U(inUnit).OutFoot(inUnit->GetTileX(), inUnit->GetTileY(), inUnit->mUnitClass->mTileSize);   // TEST ONLY (UUTL)
    ee_units_uu_footprint(0, UUHandle(inUnit));   // ee_units uu_world.rs footprint_link
}
// FUNCTION: EXE 0x64cbd7
// Returns the tile two tiles away in a direction (1..8)
void EEGetSlotTile(const EETileCoordinate& inTile, long inDirection, EETileCoordinate& outTile)
{
    EEUUNote theNote(45); theNote.I(inTile.mX).I(inTile.mY).I(inDirection).OutL(&outTile.mX, &outTile.mY);   // TEST ONLY (UUTL)
    int32_t theTile[2];
    ee_units_uu_slot_tile(inTile.mX, inTile.mY, inDirection, &theTile);
    outTile.mX = theTile[0];
    outTile.mY = theTile[1];
}

// FUNCTION: EXE 0x64cc5f
// Returns a resource's column (iron 3, gold 2, stone 1, else as listed), -1 for none
long EEGetResourceIndex(EEUnit* inResource)
{
    long theResult = -1;
    switch (inResource->mResourceType) {
    case kResourceIron:
        theResult = 3;
        break;
    case kResourceGold:
        theResult = 2;
        break;
    case kResourceStone:
        theResult = 1;
        break;
    case kResourceWood:
        return 0;
    case kResourceFood:
        if (inResource->mTileNode == NULL)
            theResult = 5;
        else
            theResult = inResource->mUnitClass->IsLandBuilding() ? 6 : 4;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x64cca5
// Picks the wall piece (straight, corner or slope) for every tile of a wall line from its neighbours and the
// terrain heights
// The corner (slope) piece of each wall tile of inTiles from the step to the next tile and the heights: runs along y
// (2), along x (4) and the two diagonals (3: same signs, 1: opposite); a step in a run picks the piece from the tile
// heights (0.4 apart: a slope), the previous tile's raised corners and its terrain corner heights. outCorners gets one
// value per tile (0: flat) (inferred)
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap)
{
    std::vector<int32_t> theCorners(inTiles.size() + 1);
    ee_units_uu_pick_wall_pieces(UUHandle(inMap), &inTiles, &theCorners[0]);
    outCorners.assign(theCorners.begin(), theCorners.end() - 1);
}

// FUNCTION: EXE 0x64d779
// Takes a fortress out of the unit regions under its footprint
// NEAR: the original removes through the vector remove helper 0x239ee2 (find + erase; see EEUnitClass::RemoveFromRegion)
// inUnit (a fortress) taken out of the unit regions under its footprint (inferred)
void EERemoveBuildingRegion(EEUnit* inUnit)
{
    EEUUNote theNote(70); theNote.U(inUnit);   // TEST ONLY (UUTL)
    ee_units_uu_footprint(3, UUHandle(inUnit));   // ee_units uu_world.rs building_region
}

// FUNCTION: EXE 0x64d946
// Removes a unit from every tile of its footprint
void EERemoveFromFootprintTiles(EEUnit* inUnit)
{
    EEUUNote theNote(67); theNote.U(inUnit).OutFoot(inUnit->GetTileX(), inUnit->GetTileY(), inUnit->mUnitClass->mTileSize);   // TEST ONLY (UUTL)
    ee_units_uu_footprint(1, UUHandle(inUnit));   // ee_units uu_world.rs footprint_link
}

// FUNCTION: EXE 0x64da0c
// Drops repeated tiles from a list
void EEDropRepeatedTiles(std::vector<EETileCoordinate>& ioTiles)
{
    EEUUNote theNote(46); theNote.T(ioTiles).OutT(&ioTiles);   // TEST ONLY (UUTL)
    ee_units_uu_drop_tiles(1, &ioTiles);
}

// FUNCTION: EXE 0x64da82
// Turns a compass direction by a number of steps, either way
long EETurnDirection(long inDirection, long inSteps, bool inClockwise)
{
    EEUUNote theNote(47); theNote.I(inDirection).I(inSteps).I(inClockwise);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_turn_direction(inDirection, inSteps, inClockwise));
}

// FUNCTION: EXE 0x64db3f
// Turns an angle by a random share of a full turn, kept in (-pi, pi]
float EEJitterHeading(float inAngle, float inMin, float inMax)
{
    EEUUNote theNote(48); theNote.F(inAngle).F(inMin).F(inMax);   // TEST ONLY (UUTL)
    return theNote.RF(ee_units_uu_jitter_heading(inAngle, inMin, inMax));
}


// FUNCTION: EXE 0x64dbd2
// Tells whether a wall piece on a tile rises above a height
// Whether a wall piece on tile (inX, inY) rises above inHeight (its height scaled by the unit's size and health)
// (the original calls RTSMap::GetTile out of line: inline budget)
bool EEIsTileTopAbove(unsigned long inX, unsigned long inY, float inHeight, EEUnit** outArg4, bool inBare)
{
    EEUUNote theNote(77); theNote.I((long)inX).I((long)inY).F(inHeight).I(inBare);   // TEST ONLY (UUTL)
    uint64_t theWall = 0;
    bool theResult = ee_units_uu_tile_top_above((int32_t)inX, (int32_t)inY, inHeight, inBare, &theWall) != 0;   // uu_world.rs
    *outArg4 = UUUnit(theWall);
    theNote.U(*outArg4);   // TEST ONLY (UUTL)
    return theNote.R(theResult);
}

// FUNCTION: EXE 0x64dd2e
// Tells whether a wall blocks a tile line (diagonal steps between two pieces count); reports the last free tile
// Whether a wall blocks the line from (inX1, inY1) to (inX2, inY2) (diagonal steps between two pieces count);
// outX/outY the last free tile
bool EEIsLineBlocked(long inX1, long inY1, long inX2, long inY2, EEUnit** outArg5, long* outX, long* outY)
{
    EEUUNote theNote(78); theNote.I(inX1).I(inY1).I(inX2).I(inY2).OutL(outX, outY);   // TEST ONLY (UUTL)
    uint64_t theWall = 0;
    uint8_t theWritten = 0;
    int32_t theLast[2];
    bool theResult = ee_units_uu_line_blocked(inX1, inY1, inX2, inY2, &theWall, &theWritten, &theLast) != 0;   // uu_world.rs
    if (theWritten)
        *outArg5 = UUUnit(theWall);
    *outX = theLast[0];
    *outY = theLast[1];
    return theNote.R(theResult);
}
// FUNCTION: EXE 0x64df23
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

// FUNCTION: EXE 0x64e3b2
// Shapes a wall ghost as the piece for a corner
void EEOrientWallGhost(EEGhostUnit* inGhost, long inCorner, long inRaisedCorners)
{
    GETransformation theTransformation(inGhost->mTransformation);
    inGhost->SetAnimation(EEOrientWallPiece(theTransformation, inCorner, inRaisedCorners));
    inGhost->mTransformation = theTransformation;
}

// FUNCTION: EXE 0x64e3f4
// Returns the frame set of a wall piece for a corner (1..12) and sets its facing
long EEOrientWallPiece(GETransformation& ioTransformation, long inCorner, long inRaisedCorners)
{
    EeUnitsUuWallPiece thePiece = ee_units_uu_wall_piece(inCorner);
    if (thePiece.turns)
        ioTransformation.SetOrientationYPR(thePiece.yaw, 0.0f, 0.0f);
    return thePiece.frame_set;
}

// FUNCTION: EXE 0x64e518
// Moves a unit to a transformation, snapping to the closest tile when off the map
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY)
{
    EEUUNote theNote(81); theNote.M(*inTransformation).U(inUnit).OutL(outX, outY).OutM(&inUnit->mPhysicalModel.GetLocation());   // TEST ONLY (UUTL)
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
}

// FUNCTION: EXE 0x64e647
// Shapes a wall piece as the piece for a corner
void EEShapeWall(EEComplexUnit* inUnit, long inCorner, long inRaisedCorners)
{
    EEUUNote theNote(76); theNote.U(inUnit).I(inCorner).I(inRaisedCorners).OutM(&inUnit->mPhysicalModel.GetLocation());   // TEST ONLY (UUTL)
    ee_units_uu_shape_wall(UUHandle(static_cast<EEUnit*>(inUnit)), inCorner);   // ee_units uu_world.rs shape_wall
}

// FUNCTION: EXE 0x64e68f
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

// SYNTHETIC: EXE 0x64e7c4
// (the atexit dtor of 0x24e68f's static sAttribute)

// SYNTHETIC: EXE 0x64e7cf
// (the atexit dtor of 0x24e68f's static sCategory)

// FUNCTION: EXE 0x64e7da
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
        for (unsigned long i = 0x26; i < gCivPowersManager.mPowers.size() + 0x26; i++) {
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

// FUNCTION: EXE 0x64edcf
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

// FUNCTION: EXE 0x64edf6
// Makes the units on a building site's footprint step aside for it
// The units on the footprint of a unit of inClass at inTile (or of inUnit) make way for it
// Actual caller24effd..24f01a forwards player/class/current footprint tile and the bool flag.
// The original caller reads its flag as a byte; the old dropped-argument placeholder is removed.
void EEStepAsideFromSite(EEUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, EETileCoordinate& inTile, bool inForce)
{
    EEUUNote theNote(71); theNote.U(inUnit).C(inClass).Pl(inPlayer).I(inTile.mX).I(inTile.mY).I(inForce);   // TEST ONLY (UUTL)
    ee_units_uu_step_aside(UUHandle(inUnit), UUHandle(inClass), UUHandle(inPlayer), inTile.mX, inTile.mY, inForce);   // uu_world.rs
}

// FUNCTION: EXE 0x64f03d
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

// FUNCTION: EXE 0x64f0bc
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

// FUNCTION: EXE 0x64f0e9
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

// FUNCTION: EXE 0x64f18d
// Scales the hit points of a living unit of the upgraded type by the ratio (at least 1)
void EEHitPointUpgrader::Apply(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mUnitType == this->mUnitType && inUnit->mHitPoints > 0 && !inUnit->IsFoundation()) {
        if (inUnit->mHitPoints > 0)
            inUnit->SetHitPoints(max(UMFloatToLong(inUnit->mHitPoints * this->mRatio), 1));
    }
}

// FUNCTION: EXE 0x64f1f8
// Tells whether a unit type can be placed on a tile for a player
bool EECanPlaceOnTile(EEPlayer* inPlayer, long inUnitType, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    bool theFlag;
    return EECanPlaceUnitOnTile(inPlayer, NULL, inUnitType, inX, inY, inSkipPlayerChecks, inIgnoreOwner, theFlag);
}

// FUNCTION: EXE 0x64f21d
// Tells whether a unit type can be placed at a location (the editor's own test in free camera mode)
bool EECanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    if (gFreeCamera == true)
        return EEEditorCanPlace(inPlayer, inType, inLocation, inX, inY);
    bool theFlag;
    return EECanPlaceUnitOnTile(inPlayer, NULL, inType, inX, inY, inSkipPlayerChecks, inIgnoreOwner, theFlag);
}

// FUNCTION: EXE 0x64f264
// Tells whether a unit type can be placed on a tile for a player (or a unit's continent)
// Whether a unit of inUnitType may be placed at (inX, inY) (for inUnit's continent, or inPlayer's selection's);
// outFlag: the second unit test (inferred)
bool EECanPlaceUnitOnTile(EEPlayer* inPlayer, EEComplexUnit* inUnit, long inUnitType, long inX, long inY, bool inSkipPlayerChecks,
                long inIgnoreOwner, bool& outFlag)
{
    EEUUNote theNote(49); theNote.Pl(inPlayer).U(inUnit).I(inUnitType).I(inX).I(inY).I(inSkipPlayerChecks).I(inIgnoreOwner);   // TEST ONLY (UUTL)
    uint8_t theFlag = 2;   // (written only when the tile's blockers were asked, as the C++ wrote outFlag)
    bool theResult = ee_units_uu_can_place_unit_on_tile(UUHandle(inPlayer), UUHandle(static_cast<EEUnit*>(inUnit)), inUnitType, inX, inY,
                                                        inSkipPlayerChecks, inIgnoreOwner != 0, &theFlag) != 0;
    if (theFlag != 2)
        outFlag = theFlag != 0;
    return theNote.R(theResult);
}

// FUNCTION: EXE 0x64f4f2
// Tells whether a unit type can be placed in the scenario editor (free camera)
// The scenario editor's (free camera) placement test (inferred)
bool EEEditorCanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY)
{
    EEUUNote theNote(72); theNote.Pl(inPlayer).I(inType).P(*inLocation).I(inX).I(inY);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_editor_can_place(UUHandle(inPlayer), inType, (const float(*)[3])&inLocation->mX, inX, inY) != 0);   // uu_world.rs
}

// (0x64f6f7, EEIsEditorSpacingClear: ee_units uu_world.rs editor_spacing_clear)

// FUNCTION: EXE 0x64f86d
// Tells whether a tile has no unit (trees aside) blocking a class for a player, and is not space
bool EEIsTileUnblocked(EEPlayer* inPlayer, long inUnitClass, EETile* inTile)
{
    EEUUNote theNote(50); theNote.Pl(inPlayer).I(inUnitClass);   // TEST ONLY (UUTL)
    return theNote.R(ee_units_uu_is_tile_unblocked(UUHandle(inPlayer), inUnitClass, UUHandle(inTile)) != 0);
}

// FUNCTION: EXE 0x64f8c0
// Gives a space wall its energy wall effect
void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectSpaceWallEnergyWall), &theHandle, 0, 0, 0);
    inUnit->mFireEffect = theHandle;
}

// FUNCTION: EXE 0x64f8f9
// Gives a space gate its energy wall effect
void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectSpaceGateEnergyWall), &theHandle, 0, 0, 0);
    inUnit->mFireEffect = theHandle;
}

// ---- the unit utilities' world (ee_units uu.rs over uu_host.rs): units, classes, players, tiles, maps and continents
// cross as their objects' addresses (a unit as its EEUnit*), the vectors as the C++ objects ----

extern "C" uint64_t ee_units_uu_host_map() { return UUHandle(::gWorld.mMap); }
extern "C" uint64_t ee_units_uu_host_map_tile(uint64_t inMap, int32_t inX, int32_t inY)
{
    return inMap != 0 ? UUHandle(((EEMap*)(uintptr_t)inMap)->GetTile((long)inX, (long)inY)) : 0;
}
extern "C" int32_t ee_units_uu_host_tile_terrain(uint64_t inTile) { return (int32_t)UUTile(inTile)->mTerrainType; }
extern "C" float ee_units_uu_host_tile_height(uint64_t inTile) { return UUTile(inTile)->GetHeight(); }
extern "C" uint32_t ee_units_uu_host_tile_units(uint64_t inTile, uint64_t* outUnits, uint32_t inCapacity)
{
    uint32_t theCount = 0;
    for (RTSTileUnitNode* theNode = UUTile(inTile)->mUnits; theNode != NULL; theNode = theNode->mNext, theCount++)
        if (theCount < inCapacity)
            outUnits[theCount] = UUHandle(theNode->mUnit);
    return theCount;
}
extern "C" uint8_t ee_units_uu_host_is_edge_tile(int32_t inX, int32_t inY) { return ::gWorld.mMap->IsEdgeTile((long)inX, (long)inY); }
extern "C" int32_t ee_units_uu_host_raised_corners(uint64_t inMap, int32_t inX, int32_t inY)
{
    return (int32_t)((EEMap*)(uintptr_t)inMap)->GetNumRaisedCorners(inX, inY);
}
extern "C" float ee_units_uu_host_mesh_z(uint64_t inMap, int32_t inX, int32_t inY) { return ((EEMap*)(uintptr_t)inMap)->mTerrainMesh.GetZ(inX, inY); }
extern "C" float ee_units_uu_host_mesh_ground(float inX, float inY) { return ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY); }

extern "C" void ee_units_uu_host_unit(uint64_t inUnit, EeUnitsUuUnit* outUnit)
{
    EEUnit* theUnit = UUUnit(inUnit);
    if (theUnit == NULL)
        return;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    EEUnitClass* theClass = theUnit->mUnitClass;
    outUnit->obj = inUnit;
    outUnit->rts = static_cast<EeUnitsRtsUnitFields*>(theUnit);
    outUnit->unit = static_cast<EeUnitsUnitFields*>(theUnit);
    outUnit->cx = theComplex != NULL ? static_cast<EeUnitsComplexUnitFields*>(theComplex) : NULL;
    outUnit->class_fields = theClass != NULL ? static_cast<EeUnitsUnitClassFields*>(theClass) : NULL;
    outUnit->class_obj = UUHandle(theClass);
    outUnit->complex = UUHandle(static_cast<EEUnit*>(theComplex));
    outUnit->player = UUHandle(theUnit->mPlayer);
    outUnit->tile = UUHandle(theUnit->mTile);
    outUnit->player_id = theUnit->mPlayer != NULL ? (int32_t)theUnit->mPlayer->GetPlayerID() : -1;
    outUnit->on_map = theUnit->mTileNode != NULL;
    outUnit->land_building = theClass != NULL && theClass->IsLandBuilding();
    outUnit->flies_over = theClass != NULL && theClass->FliesOver();
    if (theComplex == NULL)
        return;
    outUnit->active_group = UUHandle(theComplex->mActiveGroup);
    const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
    outUnit->location[0] = theLocation.GetX();
    outUnit->location[1] = theLocation.GetY();
    outUnit->location[2] = theLocation.GetZ();
    if (theComplex->mStats != NULL) {
        outUnit->stats_max_velocity = theComplex->mStats->mMaxVelocity;
        outUnit->stats_attack = theComplex->mStats->mAttack;
    }
    outUnit->has_stats = theComplex->mStats != NULL;
    outUnit->command = theComplex->mCommand != NULL ? (int32_t)theComplex->mCommand->mType : 0;
}

// a class by object, or the class table's entry of a unit type (inUnitType >= 0)
extern "C" void ee_units_uu_host_class(uint64_t inClass, int32_t inUnitType, EeUnitsUuClass* outClass)
{
    EEUnitClass* theClass = inUnitType >= 0 ? gUnitClassTable[inUnitType] : (EEUnitClass*)(uintptr_t)inClass;
    if (theClass == NULL)
        return;
    outClass->obj = UUHandle(theClass);
    outClass->fields = static_cast<EeUnitsUnitClassFields*>(theClass);
    outClass->land_building = theClass->IsLandBuilding();
    outClass->flies_over = theClass->FliesOver();
}
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
    case 7: return gContinentManager.mTileFlags[inA * gContinentManager.mFlagsWidth + inB];
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
    case 0: return thePlayer->mEnemies[inValue] != 0;
    case 1: return thePlayer->IsHuman();
    case 2: return gUnitSetManager.Contains((long)inPlayer, (unsigned long)inValue);
    case 3: return (int32_t)thePlayer->mActiveGroup->GetNumUnits();
    case 4: return thePlayer->mActiveGroup->GetContinent();
    case 5: return (int32_t)thePlayer->GetCitizenPassability();
    case 6: return thePlayer->mSaveVisMap;
    }
    return 0;
}
extern "C" uint32_t ee_units_uu_host_player_units(int32_t inPlayer, uint64_t* outUnits, uint32_t inCapacity)
{
    uint32_t theCount = 0;
    for (EEUnit* theUnit = ::gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext), theCount++)
        if (theCount < inCapacity)
            outUnits[theCount] = UUHandle(theUnit);
    return theCount;
}
extern "C" uint64_t ee_units_uu_host_part(uint64_t inComplex, uint8_t inNext)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(UUUnit(inComplex));
    return UUHandle(static_cast<EEUnit*>(inNext ? theUnit->mNextPart : theUnit->mPreviousPart));
}
extern "C" int32_t ee_units_uu_host_facing_quarter(int32_t inX, int32_t inY) { return (int32_t)EEGetFacingQuarter(inX, inY); }
extern "C" int32_t ee_units_uu_host_resource_users(uint64_t inUnit) { return (int32_t)::gWorld.GetResourceUsers(UUUnit(inUnit)); }
extern "C" int32_t ee_units_uu_host_random_long(int32_t inMin, int32_t inMax) { return (int32_t)gPhysics->GetRandom((long)inMin, (long)inMax); }
extern "C" float ee_units_uu_host_random_float(float inMin, float inMax) { return gPhysics->GetRandom(inMin, inMax); }
extern "C" void ee_units_uu_host_location(uint64_t inComplex, float* outMatrix)
{
    memcpy(outMatrix, static_cast<EEComplexUnit*>(UUUnit(inComplex))->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float));
}
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
// the map's unit regions: the region size (inWhich 0), the units of region (inX, inY)
extern "C" int32_t ee_units_uu_host_regions(int32_t inWhich, int32_t inX, int32_t inY)
{
    EeCombatUnitRegions* theRegions = ::gWorld.mMap->mUnitRegions;
    if (inWhich == 0)
        return (int32_t)ee_combat_regions_size(theRegions);
    return (int32_t)ee_combat_regions_len(theRegions, (uint32_t)inX, (uint32_t)inY);
}
extern "C" int32_t ee_units_uu_host_goal_kind(uint64_t inUnit)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(UUUnit(inUnit));
    return theUnit->mGoalState != NULL ? (int32_t)theUnit->mGoalState->mKind : -1;
}
extern "C" float ee_units_uu_host_range(uint64_t inUnit) { return static_cast<EEComplexUnit*>(UUUnit(inUnit))->mStats->GetRange(); }
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
    case 0: return (uint64_t)theGroup->GetNumUnits();
    case 1: return UUHandle(static_cast<EEUnit*>(theGroup->GetUnit(inX)));
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
    case 16: return (uint64_t)::gWorld.GetMap()->mSize;
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
    case 0: return theComplex->GetLineOfSight();
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

extern "C" uint32_t ee_units_uu_host_units_near(const float* inCenter, float inRadius, uint64_t* outUnits, uint32_t inCapacity)
{
    std::vector<EEUnit*> theUnits;
    EEGetUnitsNear(GE3DPoint(inCenter[0], inCenter[1], inCenter[2]), inRadius, theUnits);
    for (uint32_t i = 0; i < theUnits.size() && i < inCapacity; i++)
        outUnits[i] = UUHandle(theUnits[i]);
    return (uint32_t)theUnits.size();
}

extern "C" uint32_t ee_units_uu_host_region_units(int32_t inX, int32_t inY, uint64_t* outUnits, uint32_t inCapacity)
{
    EEUnitRegion theRegion(::gWorld.mMap->mUnitRegions, inX, inY);
    for (uint32_t i = 0; i < (uint32_t)theRegion.size() && i < inCapacity; i++)
        outUnits[i] = UUHandle(static_cast<EEUnit*>(theRegion[i]));
    return (uint32_t)theRegion.size();
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

// ---- template COMDATs 0x24f932..0x2544e9 (instantiated by the functions above; names from tools/csname.py) ----
// TEMPLATE: EXE 0x64f932
// TEMPLATE: EXE 0x64f960
// TEMPLATE: EXE 0x64f9d7
// TEMPLATE: EXE 0x64fa03
// TEMPLATE: EXE 0x64fa0e
// TEMPLATE: EXE 0x64fa5e
// TEMPLATE: EXE 0x64fb1e
// TEMPLATE: EXE 0x64fc04
// TEMPLATE: EXE 0x64fc86
// TEMPLATE: EXE 0x64fcd1
// TEMPLATE: EXE 0x64ffca
// TEMPLATE: EXE 0x65000c
// TEMPLATE: EXE 0x6503d7
// TEMPLATE: EXE 0x650806
// TEMPLATE: EXE 0x6509af
// TEMPLATE: EXE 0x650a6c
// TEMPLATE: EXE 0x650aa5
// TEMPLATE: EXE 0x650b65
// TEMPLATE: EXE 0x650b99
// TEMPLATE: EXE 0x650d37
// TEMPLATE: EXE 0x650d71
// TEMPLATE: EXE 0x650eff
// TEMPLATE: EXE 0x650f26
// TEMPLATE: EXE 0x650f51
// TEMPLATE: EXE 0x650f7c
// TEMPLATE: EXE 0x650f93
// TEMPLATE: EXE 0x650fbc
// TEMPLATE: EXE 0x651094
// TEMPLATE: EXE 0x6510d2
// TEMPLATE: EXE 0x651105
// TEMPLATE: EXE 0x651212
// TEMPLATE: EXE 0x65125c
// TEMPLATE: EXE 0x651282
// TEMPLATE: EXE 0x6512d4
// TEMPLATE: EXE 0x65130e
// TEMPLATE: EXE 0x65138b
// TEMPLATE: EXE 0x6513b4
// TEMPLATE: EXE 0x651487
// TEMPLATE: EXE 0x6514b8
// TEMPLATE: EXE 0x651792
// TEMPLATE: EXE 0x6517bb
// TEMPLATE: EXE 0x651893
// TEMPLATE: EXE 0x6518d2
// TEMPLATE: EXE 0x65193f
// TEMPLATE: EXE 0x651971
// TEMPLATE: EXE 0x6519b8
// TEMPLATE: EXE 0x6519cc
// TEMPLATE: EXE 0x651a82
// TEMPLATE: EXE 0x651abc
// TEMPLATE: EXE 0x651afd
// TEMPLATE: EXE 0x651c14
// TEMPLATE: EXE 0x651c50
// TEMPLATE: EXE 0x651d12
// TEMPLATE: EXE 0x651d56
// TEMPLATE: EXE 0x651da3
// TEMPLATE: EXE 0x651eba
// TEMPLATE: EXE 0x651ef6
// TEMPLATE: EXE 0x651fb3
// TEMPLATE: EXE 0x65201f
// TEMPLATE: EXE 0x6520d9
// TEMPLATE: EXE 0x652113
// TEMPLATE: EXE 0x652154
// TEMPLATE: EXE 0x6522ba
// TEMPLATE: EXE 0x6522ff
// TEMPLATE: EXE 0x65232a
// TEMPLATE: EXE 0x652356
// TEMPLATE: EXE 0x65238a
// TEMPLATE: EXE 0x652441
// TEMPLATE: EXE 0x65247b
// TEMPLATE: EXE 0x6524bc
// TEMPLATE: EXE 0x6525d3
// TEMPLATE: EXE 0x65260f
// TEMPLATE: EXE 0x652725
// TEMPLATE: EXE 0x65277b
// TEMPLATE: EXE 0x6527d4
// TEMPLATE: EXE 0x6527fd
// TEMPLATE: EXE 0x652862
// TEMPLATE: EXE 0x652895
// TEMPLATE: EXE 0x6528be
// TEMPLATE: EXE 0x652991
// TEMPLATE: EXE 0x6529c2
// TEMPLATE: EXE 0x6529fa
// TEMPLATE: EXE 0x652a49
// TEMPLATE: EXE 0x652a8d
// TEMPLATE: EXE 0x652ace
// TEMPLATE: EXE 0x652ae0
// TEMPLATE: EXE 0x652b3a
// TEMPLATE: EXE 0x652b63
// TEMPLATE: EXE 0x652b88
// TEMPLATE: EXE 0x652bb1
// TEMPLATE: EXE 0x652c98
// TEMPLATE: EXE 0x652fb2
// TEMPLATE: EXE 0x652fce
// TEMPLATE: EXE 0x65305c
// TEMPLATE: EXE 0x6530c9
// TEMPLATE: EXE 0x653163
// TEMPLATE: EXE 0x6531db
// TEMPLATE: EXE 0x65326c
// TEMPLATE: EXE 0x6532a6
// TEMPLATE: EXE 0x6532e7
// TEMPLATE: EXE 0x6533e5
// TEMPLATE: EXE 0x65340a
// TEMPLATE: EXE 0x65344c
// TEMPLATE: EXE 0x6534d2
// TEMPLATE: EXE 0x653525
// TEMPLATE: EXE 0x6535b4
// TEMPLATE: EXE 0x653622
// TEMPLATE: EXE 0x653649
// TEMPLATE: EXE 0x6536ff
// TEMPLATE: EXE 0x65383b
// TEMPLATE: EXE 0x6538d3
// TEMPLATE: EXE 0x653910
// TEMPLATE: EXE 0x6539aa
// TEMPLATE: EXE 0x653a56
// TEMPLATE: EXE 0x653a8e
// TEMPLATE: EXE 0x653ac6
// TEMPLATE: EXE 0x653c68
// TEMPLATE: EXE 0x653cb2
// TEMPLATE: EXE 0x653d15
// TEMPLATE: EXE 0x653d5d
// TEMPLATE: EXE 0x653dc0
// TEMPLATE: EXE 0x653e14
// TEMPLATE: EXE 0x653e58
// TEMPLATE: EXE 0x653e81
// TEMPLATE: EXE 0x653ebf
// TEMPLATE: EXE 0x653f20
// TEMPLATE: EXE 0x653f54
// TEMPLATE: EXE 0x653fb7
// TEMPLATE: EXE 0x654000
// TEMPLATE: EXE 0x65403c
// TEMPLATE: EXE 0x654114
// TEMPLATE: EXE 0x654196
// TEMPLATE: EXE 0x65420b
// TEMPLATE: EXE 0x654280
// TEMPLATE: EXE 0x6542c1
// TEMPLATE: EXE 0x654326
// TEMPLATE: EXE 0x654379
// TEMPLATE: EXE 0x6543ad
// TEMPLATE: EXE 0x6543ec
// TEMPLATE: EXE 0x654420

// ICF-FOLDED: EXE 0x620a18
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEFOFindNewTargetScreenBase::Virtual04(class EEComplexUnit *)
{
    return true;
}

// ICF-FOLDED: EXE 0x620a18
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEFOFindNewTargetScreenBase::Virtual08(class EEComplexUnit *)
{
    return true;
}

// ICF-FOLDED: EXE 0x64edf6
// Makes the units on the footprint of a unit of inClass placed at inTile (or of inUnit) step aside for it
// (no address of its own: the same body as EEStepAsideFromSite, which ICF kept; that one only reads the tile)
void EEPlaceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, const U2DPoint<long>& inTile, bool inArg)
{
    EETileCoordinate theTile(inTile);
    EEStepAsideFromSite(inUnit, inClass, inPlayer, theTile, inArg);
}
