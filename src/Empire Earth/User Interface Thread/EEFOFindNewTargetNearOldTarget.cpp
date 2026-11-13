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

#include "ee/EEUnitStore.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMathInline.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
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
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUCAmbient.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCSpaceship.h"
#include "ee/EEUCAirplane.h"
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
static const float kUUPanDeadZone = 0.25f;       // 0x449b54
static const float kUUEdgeFalloff = 0.25f;       // 0x449b58 (the off-screen falloff distance) (provisional name)
static const float kUUNearSquared = 25.0f;       // 0x449b60
static const float kUUBehindLimit = 4.0f;        // 0x449b5c
static const float kUUFarStart = 27.0f;          // 0x449b98
static const float kUUFarEnd = 35.0f;            // 0x449b9c
static const float kUUFarScale = 0.125f;         // 0x449ba0
static const float kUUSectorLow = -0.39269909f;  // 0x449bb8 (-pi/8)
static const float kUUSectorScale = 1.2732395f;  // 0x449bbc (4/pi)
static const float kUUSectorHigh = 0.39269909f;  // 0x449bc0 (pi/8)
static const float kUUDropDistance = 20.0f;      // 0x449b50
static const float kUUContactMargin = 1.42f;     // 0x449b7c
static const float kUUWorkReach = 1.5f;          // 0x449b80
static const float kUUGatherReach = 1.42f;       // 0x449b84
static const float kUUMinimumFacing = 0.1f;      // 0x449b64
static const float kUUTwoOverPi = 0.63661975f;   // 0x449bc4
static const float kUUNoPath = 10000.0f;         // 0x449b4c
static const float kUUWallYaw10 = 5.4977875f;    // 0x449bc8
static const float kUUWallYaw9 = 3.926991f;      // 0x449bcc
static const float kUUWallYaw8 = 4.712389f;      // 0x449bd0
static const float kUUClearance = 0.1f;          // 0x449b78
static const float kUUYawNudge = 0.0174f;        // 0x449b68 (0x24286a: about one degree)
static const float kUUSearchTurns = 5.0f;        // 0x449b6c
static const float kUUCheckReach = 2.5f;         // 0x449b70
static const float kUUWidthFactor = 0.5f;        // 0x449b74
static const float kUUFacingStep = 0.2617994f;   // 0x449bb4 (pi/12)
// The sound falloff factors (ee/EEUnitUtilities.h; set by the unit's dynamic initializers, the TU's first code
// 0x23dd54/0x23dd6e/0x23dd82: the constants are not folded)
// SYNTHETIC: EXE 0x63dd54
// SYNTHETIC: EXE 0x63dd59
float gF564520 = 1.0f / (1.0f - kUUPanDeadZone);
// SYNTHETIC: EXE 0x63dd6e
// SYNTHETIC: EXE 0x63dd73
float gF56451C = 1.0f / kUUEdgeFalloff;
// SYNTHETIC: EXE 0x63dd82
// SYNTHETIC: EXE 0x63dd87
float gF564518 = 1.0f / kUUBehindLimit;


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

// This TU's own sort-and-unique keys (its family 0x24fc86..0x2543ad expands both): highest unit id first, then the
// repeats of one id dropped (the original's _Unguarded_insert 0x2533e5 stops on `jbe`) (provisional names)
struct EEUUUnitIDGreater {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const { return inFirst->mUnitID > inSecond->mUnitID; }
};
struct EEUUUnitIDSame {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const { return inFirst->mUnitID == inSecond->mUnitID; }
};

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

// FUNCTION: EXE 0x63de0f
// Orders points by their distance from a unit (the nearer first)
bool EEUUPointNearerToUnit::operator()(const GE2DPoint& inFirst,
                                     const GE2DPoint& inSecond) const
{
    float theFirstDistance = this->mUnit->DistanceSquared2D(inFirst);
    float theSecondDistance = this->mUnit->DistanceSquared2D(inSecond);
    bool theResult = theFirstDistance < theSecondDistance;
    return theResult;
}

#pragma optimize("g", on)

// FUNCTION: EXE 0x63de5d
// Orders tiles in the screen's path area before tiles outside it
bool EEUUTileInPathArea::operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const
{
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(inFirst);
    unsigned short theFirst = theArea != NULL ? theArea->mID : 0;
    theArea = gContinentManager.GetPathAreaAt(inSecond);
    unsigned short theSecond = theArea != NULL ? theArea->mID : 0;
    if (theFirst == this->mID && theSecond != this->mID)
        return true;
    return false;
}

// Orders tiles by their distance from a point (0x241299's sort 0x24fb1e): an EETileDistanceLess centred on the point's
// truncated tile. The ctor is out of line (COMDAT 0x24f98e), the operator() is built without /Og (provisional name)
// Its own struct with a user copy ctor, not an EETileDistanceLess subclass: the sort COMDATs 0x24fb1e/0x251105/
// 0x251c50 copy it field by field (a derived implicit copy ctor stays an out-of-line U2DPoint copy)
struct EEUUTileNearerToPoint {
    EEUUTileNearerToPoint(const GE2DPoint& inPoint) : mCenter((long)inPoint.mX, (long)inPoint.mY) {}
    EEUUTileNearerToPoint(const EEUUTileNearerToPoint& inOther) : mCenter(inOther.mCenter) {}
    bool operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const;
    U2DPoint<long> mCenter;
};

// FUNCTION: EXE 0x63deae
// Orders tiles by their distance from a centre tile (the nearer first)
// Built without /Og (memory locals, `leave`; the 0x20 unused frame bytes are the locals of the two U2DPoint::
// DistanceSquared expansions VC6 undoes): the pragma alone makes it exact (r37, with the exe-view U2DPoint member)
#pragma optimize("g", off)
bool EEUUTileNearerToPoint::operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const
{
    long theFirstDistance = this->mCenter.DistanceSquared(inFirst);
    long theSecondDistance = this->mCenter.DistanceSquared(inSecond);
    bool theResult = theFirstDistance < theSecondDistance;
    return theResult;
}
#pragma optimize("g", on)

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
    std::vector<GE3DPoint> theNewPath;
    std::vector<U2DPoint<long> > theLine;
    EETileCoordinate theTile;
    EETileCoordinate theCurrent;
    U2DPoint<long> theRegionPoint;
    EETileCoordinate thePoint;
    theTile.mX = 0;
    theTile.mY = 0;
    theCurrent.mX = 0;
    theCurrent.mY = 0;
    theRegionPoint.mX = 0;
    theRegionPoint.mY = 0;
    thePoint.mX = 0;
    thePoint.mY = 0;
    long theSkipped = 0;
    long theCount = ioPath.size();
    bool theSkipping = false;
    if (inSmooth) {
        theNewPath.reserve(ioPath.size());
        if (theCount > 0) {
            RTSRegionTable<EEComplexUnit*>* theRegions = ::gWorld.mMap->mUnitRegions;
            theNewPath.push_back(*ioPath.begin());
            long theStartY = UMFloor(ioPath.begin()->mY);
            long theStartX = UMFloor(ioPath.begin()->mX);
            for (long i = 1; i < theCount - 1; i++) {
                bool theKeep;
                if (i <= 4 || theSkipped > 3) {
                    theKeep = true;
                } else {
                    if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit &&
                        (float)theCount - inUnit->mStats->GetRange() > (float)i)
                        goto keep;
                    long theY = UMFloor(ioPath[i].mY);
                    long theX = UMFloor(ioPath[i].mX);
                    thePoint.mX = theX;
                    thePoint.mY = theY;
                    theRegionPoint.mX = theX / theRegions->mRegionSize;
                    theRegionPoint.mY = theY / theRegions->mRegionSize;
                    std::vector<EEComplexUnit*>* theRegion = theRegions->GetRegion(theRegionPoint);
                    if (theRegion != NULL && theRegion->size() != 0)
                        goto keep;
                    theRegion = theRegions->GetRegion(U2DPoint<long>((theX - 2) / theRegions->mRegionSize,
                                                                     (theY - 2) / theRegions->mRegionSize));
                    if (theRegion == NULL || theRegion->size() != 0)
                        goto keep;
                    theRegion = theRegions->GetRegion(U2DPoint<long>((theX + 2) / theRegions->mRegionSize,
                                                                     (theY - 2) / theRegions->mRegionSize));
                    if (theRegion == NULL || theRegion->size() != 0)
                        goto keep;
                    theRegion = theRegions->GetRegion(U2DPoint<long>((theX + 2) / theRegions->mRegionSize,
                                                                     (theY + 2) / theRegions->mRegionSize));
                    if (theRegion == NULL || theRegion->size() != 0)
                        goto keep;
                    theRegion = theRegions->GetRegion(U2DPoint<long>((theX - 2) / theRegions->mRegionSize,
                                                                     (theY + 2) / theRegions->mRegionSize));
                    if (theRegion == NULL || theRegion->size() != 0)
                        goto keep;
                    bool theBlocked = false;
                    if (!theSkipping && (theX == theStartX || theY == theStartY))
                        goto skip;
                    theLine.clear();
                    if (theSkipping)
                        EEGetTileLine(theStartX, theStartY, theX, theY, &theLine, 0, 1);
                    theCurrent.mX = theStartX;
                    theCurrent.mY = theStartY;
                    long theLength = theLine.size();
                    for (long j = 1; theLength > j; j++) {
                        if (theBlocked)
                            goto keep;
                        theTile.mX = theLine[j].mX;
                        theTile.mY = theLine[j].mY;
                        theBlocked = !EECanStandOn(theTile, inUnit);
                        if (!theBlocked) {
                            long theDirection = EEGetTileDirection(theCurrent.mX, theCurrent.mY, theTile.mX, theTile.mY);
                            EEGetNextTile(theCurrent, theTile, EETurnDirection(theDirection, 1, true), 1);
                            theBlocked = !EECanStandOn(theTile, inUnit);
                            if (!theBlocked) {
                                EEGetNextTile(theCurrent, theTile, EETurnDirection(theDirection, 1, false), 1);
                                theBlocked = !EECanStandOn(theTile, inUnit);
                            }
                        }
                    }
                    if (theBlocked)
                        goto keep;
                    long theDirection = EEGetTileDirection(theCurrent.mX, theCurrent.mY, theX, theY);
                    EEGetNextTile(theCurrent, theTile, EETurnDirection(theDirection, 1, true), 1);
                    theKeep = !EECanStandOn(theTile, inUnit);
                    if (!theKeep) {
                        EEGetNextTile(theCurrent, theTile, EETurnDirection(theDirection, 1, false), 1);
                        theKeep = !EECanStandOn(theTile, inUnit);
                    }
                }
                if (theKeep) {
                keep:
                    theNewPath.push_back(ioPath[i]);
                    theSkipped = 0;
                    theSkipping = false;
                    theStartX = thePoint.mX;
                    theStartY = thePoint.mY;
                } else {
                skip:
                    theSkipped++;
                    theSkipping = true;
                }
            }
            if (theCount > 1)
                theNewPath.push_back(ioPath.back());
        }
        ioPath = theNewPath;
    }
}

// FUNCTION: EXE 0x63e530
// Collects the units on a tile that block a unit type for a player; true when there are any
bool EEGetBlockingUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner,
               std::vector<EEUnit*>& outUnits)
{
    outUnits.clear();
    if (inTile == NULL)
        return false;   // port: a tile off the map (GetTile's NULL) holds no units; the original read through it
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit != NULL && EEGetTileUnitStanding(theUnit, inPlayer, inUnitType, inIgnoreOwner) == 2)
            outUnits.push_back(theUnit);
    }
    return outUnits.size() > 0;
}

// FUNCTION: EXE 0x63e598
// Collects the units on a tile that can move out of the way of a unit type; true when there are any
bool EEGetMovableTileUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner,
               std::vector<EEUnit*>& outUnits)
{
    outUnits.clear();
    if (inTile == NULL)   // port: a tile off the map
        return false;
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit != NULL && EEGetTileUnitStanding(theUnit, inPlayer, inUnitType, inIgnoreOwner) == 1)
            outUnits.push_back(theUnit);
    }
    return outUnits.size() > 0;
}

// FUNCTION: EXE 0x63e600
// Tells whether any count in the list is 0
// whether any of inCounts is 0 (EEBuildWall: a wall piece nobody builds yet; its only caller passes the counts)
bool EEHasZeroCount(const std::vector<long>& inCounts)
{
    long theNumUnits = inCounts.size();
    std::vector<long>::const_iterator theIterator = inCounts.begin();
    long i = 0;
    bool theResult = false;
    while (true) {
        if (i >= theNumUnits)
            break;
        theResult = *theIterator == 0;
        ++i;
        ++theIterator;
        if (theResult)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x63e630
// Hands the wall pieces out to the group's builders until every piece has one; units that had another goal
// start over
// Hands the wall pieces inPieces out to inGroup's units: spread evenly first, then every unit steps further along the
// wall (alternating sides) until each piece has a builder; the units that had a goal start over
void EEBuildWall(EEActiveGroup* inGroup, std::vector<EEUnit*>& inPieces)
{
    std::vector<long> theCounts;
    std::vector<long> theTargets;
    std::vector<long> theSteps;
    std::map<long, EEComplexUnit*> theUnits;
    std::map<long, EEComplexUnit*>::iterator k;
    long theNumPieces = inPieces.size();
    long theNumUnits = inGroup->GetNumUnits();
    long theSpacing = 0;
    if (theNumUnits > 1)
        theSpacing = UMCeil((float)(theNumPieces - 2) / (theNumUnits - 1));
    theCounts.resize(theNumPieces, 0);
    theTargets.resize(theNumUnits, 0);
    theSteps.resize(theNumUnits, 0);
    long i;
    for (i = 0; i < theNumUnits; i++) {
        theTargets[i] = 0;
        theSteps[i] = 0;
    }
    for (i = 0; i < theNumPieces; i++)
        theCounts[i] = 0;
    long theNext = 0;
    for (i = 0; i < theNumUnits; i++) {
        long thePiece;
        if (i == 0) {
            thePiece = 0;
        } else if (i == theNumUnits - 1) {
            thePiece = theNumPieces - 1;
        } else {
            theNext += theSpacing;
            if (theNext >= theNumPieces)
                theNext = 0;
            thePiece = theNext;
        }
        theTargets[i] = thePiece;
        EEComplexUnit* theUnit = inGroup->GetUnit(i);
        if (theUnit->GetGoal() != NULL && theUnit->GetGoal()->mKind != kGoalBuildRepair)
            theUnits[theUnit->GetUnitID()] = theUnit;
        theUnit->AddBuildTarget(inPieces[theTargets[i]]);
        theSteps[i]++;
        theCounts[thePiece]++;
    }
    while (EEHasZeroCount(theCounts)) {
        for (long j = 0; j < theNumUnits; j++) {
            long theOld = theTargets[j];
            long theNew;
            if (j == 0)
                theNew = theOld + 1;
            else if (j == theNumUnits - 1)
                theNew = theOld - 1;
            else if (theSteps[j] % 2 == 0)
                theNew = theOld + theSteps[j];
            else
                theNew = theOld - theSteps[j];
            if (theNew < 0)
                theNew += theNumPieces;
            if (theNew >= theNumPieces)
                theNew -= theNumPieces;
            theTargets[j] = theNew;
            EEComplexUnit* theUnit = inGroup->GetUnit(j);
            if (theUnit->GetGoal() != NULL && theUnit->GetGoal()->mKind != kGoalBuildRepair)
                theUnits[theUnit->GetUnitID()] = theUnit;
            theUnit->AddBuildTarget(inPieces[theTargets[j]]);
            theSteps[j]++;
            theCounts[theTargets[j]]++;
        }
    }
    for (k = theUnits.begin(); k != theUnits.end(); k++) {
        EEComplexUnit* theUnit = k->second;
        theUnit->ClearGoals(0);
        theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
    }
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

static const float kUUSurfaceDepth = -0.4f;       // 0x449b88 (unit-local; xref scan only this TU)
static const float kUUAscendStep = 0.005f;         // 0x449b8c
static const float kUUDescendStep = -0.005f;        // 0x449b90

// The highest terrain beneath the submerged unit's footprint (0x23efa1) (provisional name).
float EEGetSeabedUnder(EEComplexUnit* inUnit, const GE3DPoint& inLocation);

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
// The original calls RTSTile::GetVisibility out of line through a helper level (0x2035cf at both sites) (provisional name)
inline unsigned long EEPieceVisibility(EEUnit* inUnit, EEPlayer* inPlayer) { return inUnit->mTile->GetVisibility(inPlayer->GetPlayerID()); }
void EEPlanWall(GE2DPoint inStart, GE2DPoint inEnd, EEUnitClass* inClass, EEPlayer* inPlayer, bool inAsFoundation, bool inSkipPlayerChecks,
                std::vector<EEUnit*>& outPieces)
{
    unsigned long i;
    long theX;
    long theY;
    EEUnit* thePiece;
    EEComplexUnit* theComplex;
    long theRaised;
    std::vector<EETileCoordinate> theTiles;
    std::vector<EETileCoordinate> theLine;
    std::vector<long> theCorners;
    std::vector<EEComplexUnit*> theExisting;
    std::vector<EETileCoordinate> thePlaced;
    std::vector<long> theUnused;
    EEGetWallLineXY(inStart, inEnd, theLine);

    EEExtendWallLine(theLine, inPlayer, inClass, theTiles);
    EEPickWallPieces(theTiles, theCorners, ::gWorld.GetMap());
    for (i = 0; i < theTiles.size(); i++) {
        theX = theTiles[i].GetX();
        theY = theTiles[i].GetY();
        if ((!inAsFoundation || inPlayer->CanProduce(inClass->GetTechnology(), 0x20, 1, true, 3, NULL))
            && EECanPlaceOnTile(inPlayer, inClass->GetUnitType(), theX, theY, inSkipPlayerChecks, 0)) {
            thePiece = inClass->CreateUnit(theX, theY, inPlayer->GetPlayerID(), inAsFoundation, NULL);
            theComplex = thePiece->GetComplexUnit();
            ::gWorld.GetMap()->GetTile(theX, theY);
            EETileCoordinate theTile;
            theTile.mX = theX;
            theTile.mY = theY;
            thePlaced.push_back(theTile);
            theRaised = ::gWorld.GetMap()->GetNumRaisedCorners(theX, theY);
            EEShapeWall(theComplex, theCorners[i], theRaised);
            if (inAsFoundation) {
                thePiece->SetHitPoints(1);
                if (EEPieceVisibility(thePiece, inPlayer) > 0
                    && EEPieceVisibility(thePiece, inPlayer) <= 1)
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
            outPieces.push_back(thePiece);
        } else if (::gWorld.GetMap()->GetTile(theX, theY) != NULL) {
            theRaised = ::gWorld.GetMap()->GetNumRaisedCorners(theX, theY);
            thePiece = EEGetWallAt(theTiles[i], inPlayer, inClass->GetUnitType());
            if (thePiece != NULL) {
                theComplex = thePiece->GetComplexUnit();
                EEShapeWall(theComplex, theCorners[i], theRaised);
                if (thePiece->GetHitPoints() > 0 && thePiece->GetPlayer() == inPlayer && thePiece->mUnitClass->mIsWall
                    && thePiece->mUnitClass->GetCategory() != kUnitCategoryGate && theComplex != NULL && theComplex->GetAnimation() != 0xb)
                    theExisting.push_back(theComplex);
            }
        }
    }
    EEMakeWallJunctions(theExisting, thePlaced, inPlayer, ::gWorld.GetMap());
}

static const float kUUFootprintMargin = 1.0f;    // 0x449b94 (provisional name)


// FUNCTION: EXE 0x63efa1
// Returns the highest terrain under a unit's footprint (0 once a tile is off the mesh)
// The highest terrain point under inUnit's footprint tiles (both corners of each tile's row pair, and the far edge of
// the last tile); 0 once a tile is off the mesh
float EEGetSeabedUnder(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    std::vector<U2DPoint<long> > theTiles;
    GETerrainMesh& theMesh = ::gWorld.GetMap()->mTerrainMesh;
    float theZ = theMesh.GetMeshZ(inLocation.GetX(), inLocation.GetY());
    EEGetUnitTiles(inUnit, theTiles, kUUFootprintMargin);
    unsigned long i;
    for (i = 0; i < theTiles.size(); i++) {
        if (theMesh.GetIsValidTile(theTiles[i].GetX(), theTiles[i].GetY())) {
            float theHeight = theMesh.GetZ(theTiles[i].GetX(), theTiles[i].GetY());
            theZ = theZ > theHeight ? theZ : theHeight;
            theHeight = theMesh.GetZ(theTiles[i].GetX(), theTiles[i].GetY() + 1);
            theZ = theZ > theHeight ? theZ : theHeight;
        } else {
            theZ = 0.0f;
        }
    }
    i--;
    if (theMesh.GetIsValidTile(theTiles[i].GetX() + 1, theTiles[i].GetY())) {
        float theHeight = theMesh.GetZ(theTiles[i].GetX() + 1, theTiles[i].GetY());
        theZ = theZ > theHeight ? theZ : theHeight;
        theHeight = theMesh.GetZ(theTiles[i].GetX() + 1, theTiles[i].GetY() + 1);
        theZ = theZ > theHeight ? theZ : theHeight;
    } else {
        theZ = 0.0f;
    }
    return theZ;
}

// FUNCTION: EXE 0x63f27e
// Puts a submarine at its cruise depth, or at the surface when the seabed is too high
void EEGetSeabedDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    float theGround = EEGetSeabedUnder(inUnit, ioLocation);
    float theDepth = inUnit->GetUnitClass()->GetCruiseAltitude();
    if (theGround >= ioLocation.GetZ())
        ioLocation.SetZ(kUUSurfaceDepth);
    else
        ioLocation.SetZ(-theDepth);
}

// FUNCTION: EXE 0x63f2c0
// Moves a submarine one small step up or down towards its cruise depth, staying above the seabed
void EEAdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation)
{
    float theGround = EEGetSeabedUnder(inUnit, ioLocation);
    float theDepth = -inUnit->GetUnitClass()->GetCruiseAltitude();
    float theZ;
    if (theGround >= ioLocation.GetZ()) {
        theZ = ioLocation.GetZ() + kUUAscendStep;
        if (theZ >= kUUSurfaceDepth)
            theZ = kUUSurfaceDepth;
    } else {
        theZ = ioLocation.GetZ() + kUUDescendStep;
        if (theDepth > theZ)
            theZ = theDepth;
    }
    ioLocation.SetZ(theZ);
}

// FUNCTION: EXE 0x63f32a
// Tells whether a unit can reach a target across path sub-sectors within a step limit; reports the target
// tile or its closest reachable tile
// Whether inUnit can reach inTarget across path sub-sectors (a path request of inArg5 (10/25 by distance when out
// of 1..25) steps; *outTile: the target sub-sector's location, or its closest tile when inArg4 is set)
bool EEHasPathToTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile, bool inUseClosestTile, long inMaxSteps)
{
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

// FUNCTION: EXE 0x63f5a3
// Tells whether units with these kind flags can enter a transport, fortress or other container of those flags
bool EECanEnterUnit(unsigned long inFlags, unsigned long inOtherFlags)
{
    return (inOtherFlags & kUnitKindHelicopterTransport) == kUnitKindHelicopterTransport && (inFlags & kUnitKindCanEnterAirTransports) == kUnitKindCanEnterAirTransports
        || (inOtherFlags & kUnitKindLandTransport) == kUnitKindLandTransport && (inFlags & kUnitKindCanEnterLandTransports) == kUnitKindCanEnterLandTransports
        || (inOtherFlags & kUnitKindNavalTransport) == kUnitKindNavalTransport && (inFlags & kUnitKindCanEnterSeaTransports) == kUnitKindCanEnterSeaTransports
        || (inOtherFlags & kUnitKindSpaceTransport) == kUnitKindSpaceTransport && (inFlags & kUnitKindCanEnterSpaceTransports) == kUnitKindCanEnterSpaceTransports
        || (inOtherFlags & kUnitKindFortress) == kUnitKindFortress && (inFlags & kUnitKindCanEnterFortresses) == kUnitKindCanEnterFortresses;
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

// FUNCTION: EXE 0x63f697
// Computes the start offset, step and step count for spacing units across a tile towards a neighbour
// The start (within the tile) and per-step offsets for a walk of inSize steps from a tile towards a neighbour (inDX,
// inDY), and how many steps it takes (squared for inSquare; one when inLength matches the leg); the decode is from the
// original's behaviour over every (inDX, inDY) pair (provisional names)
void EEGetSubTileSteps(float inSize, long inLength, long inDX, long inDY, bool inSquare, float* outStartX,
                float* outStartY, float* outStepX, float* outStepY, long* outCount)
{
    float theHalf = inSize * 0.5f;
    switch (inDX) {
    case -3:
        switch (inDY) {
        case -3:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case -2: case -1: case 0: case 1: case 2: case 3:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        }
        break;
    case -2:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case -1: case 0: case 1: case 2:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 3:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = -inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        }
        break;
    case -1:
        switch (inDY) {
        case -4:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -1:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case 0:
            *outStartX = 1.0f - theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 1: case 2: case 3: case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inLength == inDY ? 1 : (inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize));
            return;
        }
        break;
    case 0:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -1:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 0:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize);
            return;
        case 1: case 2: case 3: case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = -inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        }
        break;
    case 1:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -1:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inLength == 1 ? 1 : (inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize));
            return;
        case 0:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 1:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = theHalf;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case 2: case 3: case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = -inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        }
        break;
    case 2:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -1:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inLength == inDX ? 1 : (inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize));
            return;
        case 0: case 1:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 2:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = theHalf;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case 3: case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = -inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        }
        break;
    case 3:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case -2:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case -1:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inLength == inDX ? 1 : (inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize));
            return;
        case 0: case 1: case 2:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 3:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = theHalf;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case 4:
            *outStartX = 1.0f - theHalf;
            *outStartY = theHalf;
            *outStepX = -inSize;
            *outStepY = 0.0f;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        }
        break;
    case 4:
        switch (inDY) {
        case -3:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = 0.0f;
            *outStepY = -inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case -2: case 0: case 1: case 2: case 3:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = inSize;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize);
            return;
        case 4:
            *outStartX = theHalf;
            *outStartY = theHalf;
            *outStepX = 0.0f;
            *outStepY = theHalf;
            *outCount = inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : 1;
            return;
        case -1:
            *outStartX = theHalf;
            *outStartY = 1.0f - theHalf;
            *outStepX = inSize;
            *outStepY = 0.0f;
            *outCount = inLength == inDX ? 1 : (inSquare ? UMFloor(1.0f / inSize) * UMFloor(1.0f / inSize) : UMFloor(1.0f / inSize));
            return;
        }
        break;
    }
    *outStartX = theHalf;
    *outStartY = theHalf;
    *outStepX = 0.0f;
    *outStepY = inSize;
    long theCount = (long)(1.0f / inSize);
    *outCount = theCount * theCount;
}


// FUNCTION: EXE 0x640ddb
// Finds a free spot for a unit on a tile, stepping in from a neighbour; false when every step collides
// A free location for a unit of inClass on tile (inToX, inToY), stepping in from (inX, inY) (the steps of 0x23f697);
// false when every step collides with a unit on the tile (or with the unit's active group's spots)
bool EEFindPlaceOnTile(EEComplexUnit* inUnit, EEUnitClass* inClass, long inX, long inY, long inSize, long inToX, long inToY,
                long inSquare, GE3DPoint* outLocation)
{
    GE3DPoint thePosition;
    std::vector<EEUnit*> theUnits;
    float theStepX = 0.0f;
    bool theFound = false;
    long theCount = 0;
    float theStepY = 0.0f;
    float theStartX = 0.0f;
    float theStartY = 0.0f;
    if (inClass == NULL)
        inClass = inUnit->mUnitClass;
    float theSize = inClass->mInnerCollisionSphere;
    float theClamped = theSize;
    if (1.0f < theClamped)
        theClamped = 1.0f;
    EEGetUnitsOnTile(inToX, inToY, theUnits);
    long theNumUnits = theUnits.size();
    EEGetSubTileSteps(theClamped, inSize, inToX - inX, inToY - inY, inSquare != 0, &theStartX, &theStartY, &theStepX,
               &theStepY, &theCount);
    float theX = theStartX;
    float theY = theStartY;
    for (long i = 0; theCount > i && !theFound; i++) {
        theFound = true;
        outLocation->SetX((float)(unsigned long)inToX + theX);
        outLocation->SetY((float)(unsigned long)inToY + theY);
        for (long j = 0; j < theNumUnits && theFound; j++) {
            EEUnit* theUnit = theUnits[j];
            EEUnitClass* theClass = theUnit->mUnitClass;
            if (theClass->mUnitType == kUnitTypeFarm || theClass->mUnitType == kUnitTypeRoboticFarm)
                continue;
            if (theUnit->mHitPoints > 0 && !theClass->IsLandBuilding() && theUnit->mUnitClass->mInnerCollisionSphere < 1.5f
                && inUnit != theUnit && (theUnit->mUnitClass->mBlocksTile || theUnit->mUnitClass->mIsCollidable)) {
                theUnit->GetPosition(thePosition);
                float theOtherSize = theUnit->mUnitClass->mInnerCollisionSphere;
                if (theUnit->mUnitClass->mIsTree) {
                    if ((inUnit != NULL && inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 0xe))
                        || (inClass != NULL && inClass->mTerrainTypes[0xe]))
                        theOtherSize = 0.0f;
                    else
                        theOtherSize = 1.0f;
                }
                GE2DPoint theOther(thePosition.GetX(), thePosition.GetY());
                GE2DPoint theLocation(outLocation->GetX(), outLocation->GetY());
                float theDistance = theLocation.DistanceToSquared(theOther);
                float theReach = (theOtherSize + theSize) * 0.5f;
                theFound = !(theReach * theReach > kUUClearance * theSize + theDistance);
            } else {
                theFound = !theUnit->mUnitClass->mIsTree
                           && (inUnit == theUnit
                               || (theUnit->IsPassableBy(inUnit)
                                   && (theUnit->mTileNode == NULL || !theUnit->mComplexUnit->IsFoundation())
                                   && theUnit->mUnitClass->mUnitType != kUnitTypeAirportRunway));
            }
        }
        if (theFound && inUnit != NULL && inUnit->mActiveGroup != NULL && inUnit->mActiveGroup->IsReserved(*outLocation))
            theFound = false;
        if (theFound)
            continue;
        if (theStepY + theY > 1.0f) {
            theY = theStartY;
            theX = theStepY + theX;
        } else if (theStepX + theX > 1.0f) {
            theX = theStartX;
            theY = theStepX + theY;
        } else {
            theX = theStepX + theX;
            theY = theStepY + theY;
        }
        while (theX > 1.0f)
            theX -= 1.0f;
        while (theX < 0.0f)
            theX += 1.0f;
        while (theY > 1.0f)
            theY -= 1.0f;
        while (theY < 0.0f)
            theY += 1.0f;
    }
    return theFound;
}

// 0x24119a (below): the tiles of the ring inRange around the inSize square at (inX, inY), or inTarget's own
// (EEUnitClass::GetSurroundingTiles; virtually when inArg8), nearest to inUnit's unit first
void EEGetTilesAround(EEComplexUnit* inUnit, EEComplexUnit* inTarget, long inX, long inY, long inSize, long inRange,
                long inCorners, bool inArg8, std::vector<U2DPoint<long> >& outTiles);

// FUNCTION: EXE 0x64116b
// Collects the tiles around a container unit for a unit, nearest first
void EEGetTilesAroundUnit(EEComplexUnit* inUnit, EEComplexUnit* inContainer, long inRange, long inCorners, bool inArg5,
                std::vector<EETileCoordinate>& outTiles)
{
    EEGetTilesAround(inUnit, inContainer, inContainer->mTileX, inContainer->mTileY, inContainer->mUnitClass->mTileSize, inRange,
               inCorners, inArg5, outTiles);
}

// FUNCTION: EXE 0x64119a
// Collects the tiles around a target (or a square), nearest to the unit first, with the centre tile first
// for small targets
void EEGetTilesAround(EEComplexUnit* inUnit, EEComplexUnit* inTarget, long inX, long inY, long inSize, long inRange,
                long inCorners, bool inArg8, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.clear();
    if (inTarget != NULL) {
        if (inArg8)
            inTarget->mUnitClass->GetSurroundingTiles(inTarget, outTiles, 1, false);
        else
            inTarget->mUnitClass->EEUnitClass::GetSurroundingTiles(inTarget, outTiles, 1, false);
    } else {
        EEGetSquareBlockRing(inCorners, inX - inRange + 1, inY - inRange + 1, inSize + inRange * 2 - 2, outTiles, false);
    }
    std::sort(outTiles.begin(), outTiles.end(), EEUUTileNearerToUnit(inUnit->mComplexUnit));
    if (inTarget == NULL
        || ((!inTarget->mUnitClass->mIsCollidable || inTarget->mUnitClass->mInnerCollisionSphere < 0.6f) && !inTarget->mUnitClass->mIsTree
            && inTarget->mUnitClass->mUnitType != kUnitTypeFarm))
        outTiles.insert(outTiles.begin(), U2DPoint<long>(inX, inY));
}

// FUNCTION: EXE 0x641299
// Collects the ring tiles at a range around a square, sorted by distance from a point
void EEGetSortedRingTiles(const GE3DPoint& inFrom, long inX, long inY, long inSize, long inRange, long inCorners,
                std::vector<U2DPoint<long> >& outTiles)
{
    std::vector<U2DPoint<long> > theSides;
    std::vector<U2DPoint<long> > theCorners;
    outTiles.clear();
    if (inRange > 0) {
        EEGetSideTiles(inX - inRange + 1, inY - inRange + 1, inSize + inRange * 2 - 2, theSides);
        EEGetCornerTiles(inCorners, inX - inRange + 1, inY - inRange + 1, inSize + inRange * 2 - 2, theCorners);
        outTiles.insert(outTiles.end(), theSides.begin(), theSides.end());
        outTiles.insert(outTiles.end(), theCorners.begin(), theCorners.end());
        EEUUTileNearerToPoint theLess(inFrom.Get2DPoint());
        std::sort(outTiles.begin(), outTiles.end(), theLess);
    } else {
        outTiles.push_back(U2DPoint<long>(inX, inY));
    }
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
    EEAIContinent* theContinent = NULL;
    bool theFound = false;
    EEUnit* theArg5;
    long theX;
    long theY;
    if (inContainer->GetTileNode() != NULL)
        theContinent = inContainer->GetClass()->GetContainerContinent(inContainer->GetComplexUnit(), inUnit->GetComplexUnit());
    for (std::vector<EETileCoordinate>::iterator it = inTiles.begin(); it != inTiles.end() && !theFound; it++) {
        bool theStands = inUnit->GetComplexUnit() == NULL || EECanStandOn(*it, inUnit->GetComplexUnit());
        EEAIContinent* theTileContinent = gContinentManager.GetContinentAt(it->GetX(), it->GetY());
        if (theStands
            && (theTileContinent == NULL || theContinent == NULL || theContinent == theTileContinent
                || gContinentManager.AreConnectedForUnit(inUnit, theTileContinent, theContinent)
                || ((inUnit->mUnitClass->GetDomains() & kMovementSea)
                    && gContinentManager.mTileFlags[theTileContinent->mID * gContinentManager.mFlagsWidth
                                                    + theContinent->mID]))
            && EEFindPlaceOnTile(inUnit->GetComplexUnit(), NULL, inContainer->GetTileX(), inContainer->GetTileY(),
                          inContainer->GetClass()->GetTileSize(), it->GetX(), it->GetY(), 0, &outLocation)
            && (inIgnoreBlockedLine || !EEIsLineBlocked(it->GetX(), it->GetY(), inContainer->GetTileX(), inContainer->GetTileY(), &theArg5, &theX, &theY)))
            theFound = true;
        else
            theFound = false;
    }
    return theFound;
}

// FUNCTION: EXE 0x641658
// Tells whether a unit may stand on a tile: on the map, off its edge, allowed terrain, no blocking unit
// Whether inUnit may stand on inTile: on the map, not an edge tile, a terrain its class allows, the tile's water side
// for an amphibious class, and no unit on it that blocks inUnit (always for a class with flag 1 and no Virtual5C)
bool EECanStandOn(const EETileCoordinate& inTile, EEComplexUnit* inUnit)
{
    EETile* theTile = ::gWorld.mMap->RTSMap::GetTile(inTile.GetX(), inTile.GetY());
    bool theResult = true;
    if ((inUnit->GetUnitClass()->mDomains & kMovementAir) && !inUnit->GetUnitClass()->FliesOver())
        return theResult;
    if (theTile == NULL || ::gWorld.mMap->IsEdgeTile(inTile.GetX(), inTile.GetY())
        || !inUnit->GetUnitClass()->CanMoveOnTerrain(inUnit, theTile->mTerrainType)) {
        theResult = false;
    } else {
        theResult = true;
        if (inUnit->GetUnitClass()->FliesOver() || (inUnit->GetUnitClass()->mDomains & kMovementSpace))
            return theResult;
        if (inUnit->GetUnitClass()->mDomains & kMovementSea) {
            long theFlag = gContinentManager.GetTileFlag(inTile.GetX(), inTile.GetY());
            if (theFlag != 0) {
                if (theFlag < 0)
                    theResult = inUnit->mWaterState == 0;
                else
                    theResult = inUnit->mWaterState != 3;
                if (!theResult)
                    return theResult;
            }
        }
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
            if (!theNode->mUnit->IsPassableBy(inUnit)) {
                theResult = false;
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x641743
// Computes the bounding rectangle of a list of units' tiles

void EEGetSelectionBounds(std::vector<EEUnit*>& inUnits, GERectangle& outBounds, long inTerrainFilter)
{
    GE2DPoint thePoint;
    if (inUnits.size() == 0)
        return;
    EEUnit* theFirst = inUnits[0];
    thePoint.SetX((float)theFirst->GetTileX());
    thePoint.SetY((float)theFirst->GetTileY());
    outBounds.SetBottom(thePoint.GetY());
    outBounds.SetRight(thePoint.GetX());
    outBounds.SetTop(thePoint.GetY());
    outBounds.SetLeft(thePoint.GetX());
    for (unsigned long i = 1; i < inUnits.size(); i++) {
        EEUnit* theUnit = inUnits[i];
        bool theInclude;
        switch (inTerrainFilter) {
        case 1:
            theInclude = !theUnit->GetUnitClass()->CanMoveOnTerrain(theUnit, 1);
            break;
        case 2:
            theInclude = theUnit->GetUnitClass()->CanMoveOnTerrain(theUnit, 1);
            break;
        case 3:
            theInclude = theUnit->GetUnitClass()->CanMoveOnTerrain(theUnit, 0xc);
            break;
        default:
            theInclude = true;
            break;
        }
        if (!theInclude)
            continue;
        thePoint.SetX((float)theUnit->GetTileX());
        thePoint.SetY((float)theUnit->GetTileY());
        outBounds.Extend(thePoint);
    }
}

// FUNCTION: EXE 0x64181a
// Computes where an attacker should aim to intercept a moving target (else its location)
// Where inAttacker should aim at a moving inTarget: the intercept point along the target's next waypoint (the
// quadratic of the two speeds), else the target's location
void EEGetLeadPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation)
{
    GE3DPoint theAttacker(inAttacker->mPhysicalModel.GetLocation());
    EEComplexUnit* theTarget = inTarget->mComplexUnit;
    GE3DPoint thePosition;
    GE3DPoint theNext;
    if (theTarget != NULL && inAttacker != NULL && (long)theTarget->mWaypoints.size() > 1) {
        theTarget->mComplexUnit->GetPosition(thePosition);
        float theDistance = theAttacker.DistanceTo(thePosition);
        float theSpeed = inAttacker->GetMaxVelocity();
        float theTargetSpeed = theTarget->GetMaxVelocity();
        float theIndex;
        theTarget->GetPathStart(theNext, theIndex);
        float theMoveX = theNext.GetX() - thePosition.GetX();
        float theMoveY = theNext.GetY() - thePosition.GetY();
        float theMoveZ = theNext.GetZ() - thePosition.GetZ();
        float theToX = theAttacker.GetX() - thePosition.GetX();
        float theToY = theAttacker.GetY() - thePosition.GetY();
        float theToZ = theAttacker.GetZ() - thePosition.GetZ();
        float theToLength = (float)sqrt(theToY * theToY + theToX * theToX + theToZ * theToZ);
        float theDot = theMoveY * theToY + theMoveZ * theToZ + theMoveX * theToX;
        float theCosine = theDot / ((float)sqrt(theMoveZ * theMoveZ + theMoveY * theMoveY + theMoveX * theMoveX)
                                    * theToLength);
        float theA;
        if (theSpeed != theTargetSpeed)
            theA = theSpeed * theSpeed - theTargetSpeed * theTargetSpeed;
        else
            theA = 0.0f;
        float theFirst;
        float theSecond;
        bool theResult = UMQuadraticEquation(theA, theCosine * theTargetSpeed * theDistance * 2.0f,
                                             -(theDistance * theDistance), theFirst, theSecond);
        if (theResult == true) {
            if (theFirst >= 0.0f) {
                GE3DPoint theOffset(0.0f, theFirst * theTargetSpeed, 0.0f);
                theTarget->mPhysicalModel.GetLocation().Apply(theOffset, outLocation);
            } else if (theSecond >= 0.0f) {
                GE3DPoint theOffset(0.0f, theSecond * theTargetSpeed, 0.0f);
                theTarget->mPhysicalModel.GetLocation().Apply(theOffset, outLocation);
            } else {
                theResult = false;
            }
        }
        U2DPoint<long> theTile((long)outLocation.GetX(), (long)outLocation.GetY());
        if (::gWorld.GetMap()->GetTileAtLocation(theTile) == NULL)
            theResult = false;
        if (theResult)
            return;
    }
    outLocation.SetX(theTarget->mPhysicalModel.GetLocation().GetX());
    outLocation.SetY(theTarget->mPhysicalModel.GetLocation().GetY());
    outLocation.SetZ(theTarget->mPhysicalModel.GetLocation().GetZ());
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
    EEComplexUnit* theOld = static_cast<EEUGAttackUnit*>(inGoal)->mTarget->mComplexUnit;
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
                EERetargetAttack(inUnit, dynamic_cast<EEUGAttackUnit*>(inUnit->GetLoadGoal()), theLink);
                return;
            }
            theLink = theLink->mNextPart;
        }
        if (inTarget->mUnitClass->GetCategory() == kUnitCategoryGate) {
            EEComplexUnit* theFirst = dynamic_cast<EEUCGate*>(inTarget->mUnitClass.Get())->GetLastLinkedCarrier(
                theLink->GetOutermostCarrier());
            EEComplexUnit* theSecond = dynamic_cast<EEUCGate*>(inTarget->GetUnitClass())->GetLastLinked(
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
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOuterTiles;
    std::vector<U2DPoint<long> > theUnused;
    EEComplexUnit* theTargetUnit = inTarget->GetComplexUnit();
    EEUnitClass* theClass = inAttacker->GetUnitClass();
    bool theHealer;
    long theRange;

    if (theClass->mUnitType == kUnitTypeX04RomanLegionnaire && inAttacker->mIsSpecialAttack)
        theHealer = true;
    else
        theHealer = false;
    if (theClass->mIsMelee && !theHealer) {
        if (EEIsInContactRange(inAttacker, inTarget, -1.0f)) {
            const GETransformation& theLocation = inAttacker->mPhysicalModel.GetLocation();
            outLocation.SetX(theLocation.GetX());
            outLocation.SetY(theLocation.GetY());
            outLocation.SetZ(theLocation.GetZ());
        } else if (!EEGetWallSpot(inAttacker, inTarget, outLocation)) {
            EEFindNewTargetNearOld(inAttacker, theTargetUnit);
        }
        return;
    }
    if (theClass->mProjectileClass != NULL && (theClass->mProjectileClass->mCategory == kUnitCategoryProjectile || theClass->mProjectileClass->mCategory == kUnitCategoryBomb)) {
        float theReach;
        if (theHealer)
            theReach = theClass->mHealRange;
        else
            theReach = inAttacker->mStats->mRange;
        float theRadius = theClass->GetInnerCollisionSphere();
        if (EEGetHitDistanceSquared(inAttacker, inTarget, -10000.0f, -10000.0f)
                <= (theRadius + theReach) * (theClass->GetInnerCollisionSphere() + theReach)
            && EECanBallisticHit(inAttacker, inTarget)) {
            const GETransformation& theLocation = inAttacker->mPhysicalModel.GetLocation();
            outLocation.SetX(theLocation.GetX());
            outLocation.SetY(theLocation.GetY());
            outLocation.SetZ(theLocation.GetZ());
            return;
        }
    } else if (EEIsInRangeToHit(inAttacker, inTarget)) {
        const GETransformation& theLocation = inAttacker->mPhysicalModel.GetLocation();
        outLocation.SetX(theLocation.GetX());
        outLocation.SetY(theLocation.GetY());
        outLocation.SetZ(theLocation.GetZ());
        return;
    }
    theRange = (long)inAttacker->mStats->mRange - inTarget->GetUnitClass()->mTileSize;
    if (theRange < 0)
        theRange = 0;
    theRange /= 2;
    EEGetSortedRingTiles(GE3DPoint(inAttacker->mPhysicalModel.GetLocation().GetX(), inAttacker->mPhysicalModel.GetLocation().GetY(),
                         inAttacker->mPhysicalModel.GetLocation().GetZ()), inTarget->GetTileX(), inTarget->GetTileY(),
               inTarget->GetUnitClass()->GetTileSize(), theRange, 0, theOuterTiles);
    EEGetSortedRingTiles(GE3DPoint(inAttacker->mPhysicalModel.GetLocation().GetX(), inAttacker->mPhysicalModel.GetLocation().GetY(),
                         inAttacker->mPhysicalModel.GetLocation().GetZ()), inTarget->GetTileX(), inTarget->GetTileY(),
               inTarget->GetUnitClass()->GetTileSize(), theRange + 1, 0, theTiles);
    theTiles.insert(theTiles.end(), theOuterTiles.begin(), theOuterTiles.end());
    std::sort(theTiles.begin(), theTiles.end(), EEUUTileNearerToUnit(inAttacker));
    if (EEPickTileNear(theTiles, inAttacker, static_cast<EEComplexUnit*>(inTarget), 0, outLocation))
        return;
    if (theTargetUnit != NULL) {
        theTargetUnit->GetPosition(outLocation);
    } else {
        outLocation.mX = (float)inTarget->GetTileX() + 0.5f;
        outLocation.mY = (float)inTarget->GetTileY() + 0.5f;
    }
}

// FUNCTION: EXE 0x642110
// Computes a moving unit's Doppler pitch shift against the camera
// The pitch shift of a moving unit against the camera (the Doppler factor of EEUCLandable::GetSoundParameters 0x2141a3, whose
// base call is the caller's here)
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    GE3DPoint theCamera(::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(),
                        ::gWorld.mTransformation.GetZ());
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    GE3DPoint thePosition;
    float theSpeed = theUnit->mVelocity;
    GEVector theVelocity(0.0f, theSpeed, 0.0f);
    GEVector theHeading;
    GEVector theToCamera;
    if (!(*outVolume > 0.0f) || !(theSpeed > 0.0f))
        return;
    thePosition.SetX(theUnit->mPhysicalModel.GetLocation().GetX());
    thePosition.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
    thePosition.SetZ(theUnit->mPhysicalModel.GetLocation().GetZ());
    theToCamera.SetX(theCamera.GetX() - thePosition.GetX());
    theToCamera.SetY(theCamera.GetY() - thePosition.GetY());
    theToCamera.SetZ(theCamera.GetZ() - thePosition.GetZ());
    if (theToCamera.GetX() >= -0.0001f && 0.0001f >= theToCamera.GetX() && theToCamera.GetY() >= -0.0001f &&
        theToCamera.GetY() <= 0.0001f && theToCamera.GetZ() >= -0.0001f && theToCamera.GetZ() <= 0.0001f) {
        theToCamera.SetX(0.0f);
        theToCamera.SetY(0.0f);
        theToCamera.SetZ(0.0f);
    } else
        theToCamera.Normalize();
    theUnit->mPhysicalModel.GetLocation().Apply(theVelocity, theHeading);
    thePosition += theHeading;
    if (theHeading.GetX() >= -0.0001f && theHeading.GetX() <= 0.0001f && theHeading.GetY() >= -0.0001f &&
        theHeading.GetY() <= 0.0001f && theHeading.GetZ() >= -0.0001f && theHeading.GetZ() <= 0.0001f) {
        theHeading.SetX(0.0f);
        theHeading.SetY(0.0f);
        theHeading.SetZ(0.0f);
    } else
        theHeading.Normalize();
    float theCosine = theHeading.DotProduct(theToCamera);
    float theRatio = theVelocity.GetY() * 0.05f;
    float theGamma = (float)sqrt(1.0f - theVelocity.GetY() * theVelocity.GetY() * 0.0025f);
    *outPitch = theGamma / (1.0f - theRatio * ((theRatio + theCosine) / (1.0f - theRatio * theCosine)));
}

// FUNCTION: EXE 0x642345
// Returns the angle between north and the direction from one point to the other
float EEGetAngleFromNorth(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    GETransformation theTransformation;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theHeading;
    GEVector theDirection(inFrom->GetX() - inTo->GetX(), inFrom->GetY() - inTo->GetY(), 0.0f);
    float theCosine;
    float theAngle;

    theDirection.Normalize();
    theTransformation.Apply(theForward, theHeading);
    theHeading.SetZ(0.0f);
    theHeading.Normalize();
    theCosine = theHeading.DotProduct(theDirection);
    if (theCosine > 1.0f)
        theCosine = 1.0f;
    else if (theCosine < -1.0f)
        theCosine = -1.0f;
    theAngle = UMArcCos(theCosine);
    return theAngle < 0.0f ? -theAngle : theAngle;
}

// FUNCTION: EXE 0x64240d
// Returns the ground heading from one point to another
float EEGetHeadingBetween(GE3DPoint* inFrom, GE3DPoint* inTo)
{
    return UMArcTan2(-(inTo->GetX() - inFrom->GetX()), inTo->GetY() - inFrom->GetY());
}

// FUNCTION: EXE 0x642448
// Returns the heading from one tile to another
float EEGetSeaHeading(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo)
{
    float theY = (float)inTo.GetY() - (float)inFrom.GetY();
    float theX = (float)inTo.GetX() - (float)inFrom.GetX();
    return UMArcTan2(-theX, theY);
}

// FUNCTION: EXE 0x642489
// Returns the heading from one unit's tile to another's
float EEGetUnitAngle(EEUnit* inFrom, EEUnit* inTo)
{
    float theY = (float)inTo->GetTileY() - (float)inFrom->GetTileY();
    float theX = (float)inTo->GetTileX() - (float)inFrom->GetTileX();
    float theAngle = UMArcTan2(-theX, theY);
    if (theAngle > 3.1415927f)
        theAngle -= 3.1415927f;
    if (theAngle < -3.1415927f)
        theAngle += 3.1415927f;
    return theAngle;
}

// EEUUAngleLess::operator() (ee/EEUnitUtilities.h): inline, expanded into the std::sort COMDATs (_Insertion_sort_1
// 0x25201f, _Unguarded_insert 0x2522ba call EEGetUnitAngle twice; there is no out-of-line operator())
inline bool EEUUAngleLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    return EEGetUnitAngle(this->mUnit, inFirst) < EEGetUnitAngle(this->mUnit, inSecond);
}

// FUNCTION: EXE 0x6424f6
// Tells whether the target unit is attacking the unit
bool EEIsAttackedBy(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEComplexUnit* theTarget = inTarget->mComplexUnit;
    bool theResult = false;
    if (!inUnit->mUnitClass->mIsTower && theTarget != NULL && theTarget->mGoalState != NULL) {
        EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(theTarget->mGoalState.Get());
        if (theGoal != NULL)
            theResult = theGoal->mTarget == inUnit;
    }
    return theResult;
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

// The tiles of a vector that are among mTiles (0x24286a's remove_if: find_if 0x25125c, operator() 0x251282 out of
// line) (provisional name)
struct EEUUTileAmong {
    EEUUTileAmong(std::vector<U2DPoint<long> >& inTiles) : mTiles(inTiles) {}
    bool operator()(const U2DPoint<long>& inTile) const
        { return std::find(this->mTiles.begin(), this->mTiles.end(), inTile) != this->mTiles.end(); }
    std::vector<U2DPoint<long> >& mTiles;
};

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
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theMore;
    std::vector<U2DPoint<long> > theLine;
    std::vector<U2DPoint<long> > theOwnTiles;
    GE2DPoint theCenter;
    bool theFound = false;
    long theCount = 0;
    long theKind = inArg5 ? 1 : 3;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    float theX = (float)theUnit->mTileX;
    float theY = (float)theUnit->mTileY;
    theUnit->AddCenterOffset(theX, theY);
    if (!inArg5) {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        float theFacing = UMArcTan2(theLocation.GetElement(1, 0), theLocation.GetElement(0, 0));
        float theTryX = theX;
        float theTryY = theY;
        bool theFree = false;
        for (long i = 0; i < 6 && !theFree; i++) {
            float theRing = (float)i;
            for (long j = 0; j <= 6; j++) {
                float theOffset = j * kUUFacingStep;
                float theAngle = theOffset + theFacing;
                theTryX = theX - UMSin(theAngle) * theRing;
                theTryY = UMCos(theAngle) * theRing + theY;
                if (::gWorld.mMap->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(theTryX), UMFloorUnsigned(theTryY))
                    && ::gWorld.mMap->mTerrainMesh.GetIsSpaceTile(UMFloorUnsigned(theTryX), UMFloorUnsigned(theTryY))
                    && !gContinentManager.IsShoreTile(UMFloor(theTryX), UMFloor(theTryY))) {
                    theFree = true;
                    break;
                }
                theOffset = theFacing - theOffset;
                theTryX = theX - UMSin(theOffset) * theRing;
                theTryY = UMCos(theOffset) * theRing + theY;
                if (::gWorld.mMap->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(theTryX), UMFloorUnsigned(theTryY))
                    && ::gWorld.mMap->mTerrainMesh.GetIsSpaceTile(UMFloorUnsigned(theTryX), UMFloorUnsigned(theTryY))
                    && !gContinentManager.IsShoreTile(UMFloor(theTryX), UMFloor(theTryY))) {
                    theFree = true;
                    break;
                }
            }
        }
        if (theFree) {
            theX = theTryX;
            theY = theTryY;
        }
    }
    theCenter.mX = theX;
    theCenter.mY = theY;
    float theRadius = inClass->mOuterCollisionSphere;
    float theHalf = theRadius * 0.5f;
    float theInner = theHalf > 1.0f ? theHalf : 1.0f;
    float theDistance = theHalf * kUUWidthFactor + (theHalf > 1.5f ? theHalf : 1.5f) + 0.5f;
    float theStep = theInner / theDistance * 0.5f;
    float theYaw;
    if (inFacePoint) {
        float theTargetX = inPoint->GetX();
        float theTargetY = inPoint->GetY();
        theYaw = UMArcTan2(theTargetY - GE3DPoint(theUnit->mPhysicalModel.GetLocation().GetX(),
                                                  theUnit->mPhysicalModel.GetLocation().GetY(),
                                                  theUnit->mPhysicalModel.GetLocation().GetZ()).GetY(),
                           theTargetX - GE3DPoint(theUnit->mPhysicalModel.GetLocation().GetX(),
                                                  theUnit->mPhysicalModel.GetLocation().GetY(),
                                                  theUnit->mPhysicalModel.GetLocation().GetZ()).GetX());
    } else {
        theYaw = theUnit->mLaunchYaw + theStep;
    }
    float theEnd = theYaw + 6.2831855f;
    unsigned short theContinent;
    theContinent = !inArg5 ? gContinentManager.GetContinent(U2DPoint<long>(UMFloor(theX), UMFloor(theY)), theKind) : theUnit->GetFootprintContinent(theKind);
    while (true) {
        if (!((float)theCount < kUUSearchTurns))
            break;
        for (; theYaw < theEnd; theYaw += theStep * 2.0f) {
            float theCos = UMCos(theYaw);
            float theSin = UMSin(theYaw);
            outTransformation->SetX(theCos * theDistance + theX);
            outTransformation->SetY(theSin * theDistance + theY);
            float theReach = theDistance + theHalf;
            long theEndX = UMFloatToLong(theReach * theCos + theX);
            long theEndY = UMFloatToLong(theReach * theSin + theY);
            unsigned short theTileContinent = gContinentManager.GetContinent(
                U2DPoint<long>(UMFloor(outTransformation->GetX()), UMFloor(outTransformation->GetY())), theKind);
            EEGetTileLine(UMFloor(theX), UMFloor(theY), theEndX, theEndY, &theLine, 1, 1);
            EEGetFootprintTiles(theUnit, theOwnTiles);
            std::vector<U2DPoint<long> >::iterator theLast =
                std::remove_if(theLine.begin(), theLine.end(), EEUUTileAmong(theOwnTiles));
            if (theLast != theLine.end())
                theLine.erase(theLast, theLine.end());
            if (theTileContinent == theContinent && theContinent != 0
                && gContinentManager.InSameArea(theLine, theKind) && !gContinentManager.HasShoreTile(theLine)) {
                theUnits.clear();
                theMore.clear();
                EEGetUnitsInFan(theCenter, theDistance + kUUCheckReach, theYaw - theStep, theStep + theYaw,
                           UMFloatToLong(theRadius * 3.0f), theUnits);
                EEGetUnitsOnTile(UMFloor(outTransformation->GetX()), UMFloor(outTransformation->GetY()), theMore);
                std::vector<EEUnit*>::iterator it; for (it = theMore.begin(); it != theMore.end(); it++)
                    theUnits.push_back(*it);
                if (theMore.size() > 0 && theUnits.size() > 1)
                    EESortUnique(theUnits, EEUUUnitIDGreater(), EEUUUnitIDSame());
                GE3DPoint theBack;
                GE3DPoint theFront;
                GEVector theDirection;
                GE3DLine theWidth;
                GEVector theForward(0.0f, 1.0f, 0.0f);
                outTransformation->SetOrientationYPR(theYaw - 1.5707964f, 0.0f, 0.0f);
                theLine.clear();
                theMore.clear();
                outTransformation->Apply(theForward, theDirection);
                theDirection.Normalize();
                float theLength = inClass->mInnerCollisionSphere + 1.0f;
                theDirection.mX = theLength * theDirection.mX;
                theDirection.mY = theDirection.mY * theLength;
                theDirection.mZ = theDirection.mZ * theLength;
                theWidth.SetStart(GE3DPoint(outTransformation->GetX(), outTransformation->GetY(),
                                            outTransformation->GetZ()));
                theWidth.SetDirection(theDirection);
                theWidth.GetPointAt(0.5f, theFront);
                theWidth.GetPointAt(-0.5f, theBack);
                EEGetTileLine(UMFloor(theFront.GetX()), UMFloor(theFront.GetY()), UMFloor(theBack.GetX()),
                           UMFloor(theBack.GetY()), &theLine, 1, 0);
                EEGetUnitsOn(theLine, theMore);
                for (it = theMore.begin(); it != theMore.end(); it++)
                    theUnits.push_back(*it);
                if (theMore.size() > 0 && theUnits.size() > 1)
                    EESortUnique(theUnits, EEUUUnitIDGreater(), EEUUUnitIDSame());
                bool theBlocked = false;
                for (it = theUnits.begin(); it != theUnits.end() && !theBlocked; it++) {
                    EEUnit* theOther = *it;
                    EEUnitClass* theClass = theOther->mUnitClass;
                    if (theOther->mHitPoints > 0
                        && (inArg5 ? (theClass->mDomains & kMovementSea) != 0 : (theClass->mDomains & kMovementSpace) != 0)
                        && !theClass->mIsDecoration && theClass->mFamily != kUnitFamilyFish)
                        theBlocked = true;
                    else if (theOther != theUnit && (theClass->mUnitType == kUnitTypeNavyYard || theClass->mUnitType == kUnitTypeDock))
                        theBlocked = true;
                    else
                        theBlocked = false;
                }
                theFound = !theBlocked;
                if (theFound)
                    goto done;
            }
        }
        theCount++;
        theYaw = kUUYawNudge + theYaw - 6.2831855f;
    }
done:
    while (theYaw > 6.2831855f)
        theYaw -= 6.2831855f;
    float theNext;
    if (theFound) {
        outTransformation->SetOrientationYPR(theYaw - 1.5707964f, 0.0f, 0.0f);
        theNext = theStep + theYaw;
    } else {
        theNext = kUUYawNudge + theStep + theYaw;
    }
    if (theNext != theUnit->mLaunchYaw)
        theUnit->mLaunchYaw = theNext;
    return theFound;
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
    std::vector<U2DPoint<long> > theUnused1;
    std::vector<U2DPoint<long> > theUnused2;
    std::vector<U2DPoint<long> > theTiles;
    GE3DPoint theLocation;
    GE3DPoint theGround;
    EEMap* theMap = ::gWorld.mMap;
    long theKind = 0;
    float theYaw = 3.1415927f;
    long theToY = 0;
    long theToX = 0;
    bool theSearching = true;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    EEAIContinent* theContinent;
    unsigned short theID;
    long theRing;
    unsigned long theCount;
    unsigned long i;
    EEUnit* theA;
    long theB;
    long theC;

    if (inClass != NULL || (inClass = inUnit->mUnitClass) != NULL)
        theKind = inClass->GetPassableFlags(inUnit);
    theID = gContinentManager.GetContinentID(inX, inY);
    theContinent = theID != 0 ? gContinentManager.GetContinentByID(theID) : NULL;
    theRing = inIncludeCenter == false;
    do {
        if (theRing > inRings)
            break;
        theTiles.clear();
        if (inFacePoint)
            EEGetSortedRingTiles(*inPoint, inX, inY, inSize, theRing, inCorners, theTiles);
        else
            EEGetRingTiles(inX, inY, inSize, theRing, inCorners, theTiles);
        theCount = theTiles.size();
        for (i = 0; theSearching && i < theCount; i++) {
            unsigned long theX = theTiles[i].mX;
            unsigned long theY = theTiles[i].mY;
            EEMapTile* theTile = theMap->GetTile(theX, theY);
            EEAIContinent* theOther = gContinentManager.GetContinentAt(theTiles[i].mX,
                                                                     theTiles[i].mY);
            if (theTile == NULL || !inClass->CanMoveOnTerrain(inUnit, theTile->mTerrainType) || theMap->IsEdgeTile(theX, theY)
                || theContinent == NULL || theOther == NULL
                || (theContinent != theOther && !gContinentManager.AreConnected(theContinent, theOther, theKind)
                    && (((inClass->mDomains & kMovementSea) == 0 && !inArg11)
                        || !gContinentManager.mTileFlags[theOther->mID
                                                        + theContinent->mID * gContinentManager.mFlagsWidth])))
                continue;
            if (!inCorners && inRings > 1) {
                if (inTarget != NULL) {
                    EEGetFootprintDistance(inTarget, theX, theY, &theToX, &theToY);
                } else {
                    theToX = inX;
                    theToY = inY;
                }
                if (EEIsLineBlocked(theX, theY, theToX, theToY, &theA, &theB, &theC))
                    continue;
            }
            if (EEFindPlaceOnTile(inUnit, inClass, inIncludeCenter ? theX : inX, inIncludeCenter ? theY : inY, inSize, theX, theY,
                           inIncludeCenter, &theLocation))
                theSearching = false;
        }
        theRing++;
    } while (theSearching);
    if (!theSearching) {
        if (inFacePoint)
            theYaw = EEGetHeadingBetween(&theLocation, inPoint);
        outTransformation->SetOrientationYPR(theYaw, 0.0f, 0.0f);
        theMap->GetGroundPoint(theLocation, theGround);
        outTransformation->SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
        if (inClass->mPitchesWithTerrain)
            thePitch = theMap->GetTerrainPitch(*outTransformation);
        if (inClass->mRollsWithTerrain)
            theRoll = theMap->GetTerrainRoll(*outTransformation);
        outTransformation->SetOrientationYPR(theYaw, thePitch, theRoll);
    }
    return !theSearching;
}

// FUNCTION: EXE 0x643978
// Classifies a unit on a tile for a placement: 0 ignored, 1 movable, 2 blocking
// How inPlayer sees inUnit as a candidate of unit type inUnitType: 0 not at all, 1 a target, 2 otherwise (inferred)
char EEGetTileUnitStanding(EEUnit* inUnit, EEPlayer* inPlayer, long inUnitType, bool inIgnoreOwner)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    EEPlayer* theOwner = inUnit->mPlayer;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    EEUnitClass* theUnitClass = inUnit->mUnitClass;
    unsigned long theType = theUnitClass->mUnitType;

    if (theUnitClass->mIsDecoration || theType == kUnitTypeWaypoint || theType == kUnitTypePathPoint)
        return 0;
    if (inUnit->mHitPoints == 0 && theUnitClass->mCategory != kUnitCategoryResource)
        return 0;
    if (theUnitClass->mCanFly && theComplexUnit != NULL
        && theComplexUnit->mPhysicalModel.GetLocation().GetZ() > theComplexUnit->mTile->GetHeight())
        return 0;
    if (theUnitClass->IsLandBuilding() && theComplexUnit != NULL && theComplexUnit->IsFoundation() && inPlayer != NULL
        && inPlayer->IsHuman() && theOwner != inPlayer)
        return 0;
    if (gUnitSetManager.Contains(theType, theClass->mOverlapUnitSet))
        return 0;
    if (inPlayer == NULL)
        return 2;
    if ((inIgnoreOwner || (theOwner != inPlayer && inPlayer->mEnemies[theOwner->GetPlayerID()] != 0)) && theComplexUnit != NULL
        && theComplexUnit->mCurrentPhysicsAction == 0)
        return 1;
    if ((inIgnoreOwner || theOwner == inPlayer || inPlayer->mEnemies[theOwner->GetPlayerID()] == 0) && theComplexUnit != NULL
        && theComplexUnit->mStats->mMaxVelocity > 0.0f)
        return 1;
    if (theUnitClass->mFamily == kUnitFamilyAnimal && theComplexUnit != NULL && theComplexUnit->mStats->mAttack == 0.0f)
        return 1;
    return 2;
}

// FUNCTION: EXE 0x643ae1
// Turns wall pieces with three or more neighbours into plain junction pieces
// Wall pieces with three or more neighbours (inPlayer's live pieces of a wall class around them, or tiles of inTiles
// within one tile) become plain pieces facing south (inferred)
void EEMakeWallJunctions(std::vector<EEComplexUnit*>& inUnits, std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer,
                    EEMap* inMap)
{
    unsigned long i;
    unsigned long theCount;
    unsigned long theY;
    unsigned long theX;
    unsigned long theNeighbourY;

    for (i = 0; i < inUnits.size(); i++) {
        EEComplexUnit* theUnit = inUnits[i];
        unsigned long theNeighbourX;
        theX = theUnit->GetTileX();
        theY = theUnit->GetTileY();
        theCount = 0;
        for (theNeighbourX = theX - 1; theNeighbourX <= theX + 1; theNeighbourX++) {
            for (theNeighbourY = theY - 1; theNeighbourY <= theY + 1; theNeighbourY++) {
                if (theNeighbourX == theX && theNeighbourY == theY)
                    continue;
                EETile* theTile = inMap->GetTile(theNeighbourX, theNeighbourY);
                if (theTile == NULL)
                    continue;
                RTSTileUnitNode* theNode = theTile->mUnits;
                if (theNode == NULL)
                    continue;
                for (; theNode != NULL; theNode = theNode->mNext) {
                    EEUnit* theOther = theNode->mUnit;   // each node's unit (was the first node's for all)
                    if (theOther != NULL && theOther->mPlayer == inPlayer && theOther->mHitPoints > 0
                        && theOther->mUnitClass->mIsWall)
                        theCount++;
                }
            }
        }
        for (long j = inTiles.size() - 1; j >= 0; j--) {
            float theDX = (float)(inTiles[j].mX - theX);
            float theDY = (float)(inTiles[j].mY - theY);
            if (UMAbs(theDX) <= 1.0f && UMAbs(theDY) <= 1.0f)
                theCount++;
        }
        if (theCount < 3)
            continue;
        theUnit->SetAnimation(0xb, -1, 0);
        GETransformation theTransformation(theUnit->mPhysicalModel.GetLocation());
        theTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f);
        theUnit->SetTransformation(theTransformation, true);
    }
}

// FUNCTION: EXE 0x643c7d
// Tells whether every tile of a rectangle can take a unit type for the unit's player
bool EEBlockTakes(EEUnit* inUnit, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    bool theResult = true;
    for (long theX = inX; theX < inX + inWidth && theResult; theX++) {
        for (long theY = inY; theY < inY + inHeight && theResult; theY++)
            theResult = EECanPlaceOnTile(inUnit != NULL ? inUnit->mPlayer : NULL, inUnitType, theX, theY, 0, inIgnoreOwner);
    }
    return theResult;
}

// FUNCTION: EXE 0x643ce6
// Tells whether every tile of a rectangle can take a unit type for a player
// returns the result in al (EECPBuildManager tests it)
bool EEPlayerBlockTakes(EEPlayer* inPlayer, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner)
{
    bool theResult = true;
    for (long theX = inX; theX < inX + inWidth && theResult; theX++) {
        for (long theY = inY; theY < inY + inHeight && theResult; theY++)
            theResult = EECanPlaceOnTile(inPlayer, inUnitType, theX, theY, 0, inIgnoreOwner);
    }
    return theResult;
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

static void EESearchRegions(void* inContext, int32_t inLeft, int32_t inTop, int32_t inRight, int32_t inBottom,
                            int32_t (*outRegions)[4])
{
    U2DRectangle<long> theArea;
    theArea.mLeft = inLeft;
    theArea.mTop = inTop;
    theArea.mRight = inRight;
    theArea.mBottom = inBottom;
    U2DRectangle<long> theRegions = gWorld.mMap->mUnitRegions->GetRegions(theArea);
    (*outRegions)[0] = (int32_t)theRegions.GetTopLeft().mX;
    (*outRegions)[1] = (int32_t)theRegions.GetTopLeft().mY;
    (*outRegions)[2] = (int32_t)theRegions.GetBottomRight().mX;
    (*outRegions)[3] = (int32_t)theRegions.GetBottomRight().mY;
}

static int32_t EESearchRegionLength(void* inContext, int32_t inX, int32_t inY)
{
    std::vector<EEComplexUnit*>* theRegion = gWorld.mMap->mUnitRegions->GetRegion((long)inX, (long)inY);
    return theRegion != NULL ? (int32_t)theRegion->size() : -1;
}

static uint64_t EESearchRegionUnit(void* inContext, int32_t inX, int32_t inY, int32_t inIndex)
{
    std::vector<EEComplexUnit*>* theRegion = gWorld.mMap->mUnitRegions->GetRegion((long)inX, (long)inY);
    return (uint64_t)(uintptr_t)(*theRegion)[inIndex];
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
        EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inTarget->GetGoal());
        return theGoal != NULL && theGoal->mTarget->mComplexUnit == theUnit;
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
        EEUGAttackUnit* theGoal;
        return theTarget != NULL && (theGoal = dynamic_cast<EEUGAttackUnit*>(theTarget->GetGoal())) != NULL
            && theGoal->mTarget->mComplexUnit == theUnit;
    }
    case EE_COMBAT_SEARCH_TEST_PURSUES: {
        if (theTarget == NULL || theTarget->mLauncherClass != NULL || theTarget->mGoalState == NULL)
            return 0;
        EEUnitGoal* theGoal = theTarget->mGoalState;
        EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoal);
        EEUGRust* theGather = EEGatherGoalCast(theGoal);
        return (theAttack != NULL && theAttack->mTarget->mComplexUnit == theUnit)
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
    EeCombatSearchHost theHost = {inContext, EESearchRegions, EESearchRegionLength, EESearchRegionUnit,
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
    in.region_size = (int32_t)gWorld.mMap->mUnitRegions->mRegionSize;
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
    GE3DPoint theCamera;
    GE3DPoint theScreen;
    float theHeight = gViewport.GetYClippingPlane();
    float theScale = 1.0f;
    float theVertical = 1.0f;
    float thePan = 0.0f;
    float theDepth;
    float theX;

    EETile* theTile = ::gWorld.mMap->GetTile(UMFloor(inLocation.GetX()), UMFloor(inLocation.GetY()));
    if (theTile == NULL || !theTile->IsSeenBy(::gWorld.GetCurrentPlayer())) {
        outPitch = 1.0f;
        outPan = 0.0f;
        outVolume = 0.0f;
        return;
    }
    EEGetCameraPoint(inLocation, theScreen, theDepth);
    theX = theDepth < 0.0f ? -theScreen.GetX() : theScreen.GetX();
    if (theX < -kUUPanDeadZone)
        thePan = -((-theX - kUUPanDeadZone) * gF564520);
    else if (theX > kUUPanDeadZone)
        thePan = (theX - kUUPanDeadZone) * gF564520;
    theCamera.SetX(::gWorld.mTransformation.GetX());
    theCamera.SetY(::gWorld.mTransformation.GetY());
    theCamera.SetZ(::gWorld.mTransformation.GetZ());
    if (theCamera.DistanceToSquared(inLocation) > kUUNearSquared) {
        if (theScreen.GetX() < -1.0f)
            theScale = 1.0f - (-theScreen.GetX() - 1.0f) * gF56451C;
        else if (theScreen.GetX() > 1.0f)
            theScale = 1.0f - (theScreen.GetX() - 1.0f) * gF56451C;
        if (-theHeight > theScreen.mY)
            theVertical = 1.0f - (-theScreen.GetY() - theHeight) * gF56451C;
        else if (gViewport.GetYClippingPlane() < theScreen.mY)
            theVertical = 1.0f - (theScreen.GetY() - theHeight) * gF56451C;
        if (theVertical < theScale)
            theScale = theVertical;
    }
    if (theDepth < 0.0f) {
        float theBehind = -theDepth;
        if (theBehind > kUUBehindLimit)
            theVertical = 0.0f;
        else
            theVertical = 1.0f - gF564518 * theBehind;
    } else {
        // port: past the original's farthest zoom (Maximum Zoom Out above 100%) the camera's distance grows with the
        // zoom, so the far fade does too; with the original's fixed 27..35 the whole view went silent zoomed out
        float theZoom = ::gWorld.mCameraControl.GetZoom();
        float theReach = theZoom > 1.0f ? theZoom : 1.0f;
        if (theDepth >= kUUFarStart * theReach)
            theVertical = (kUUFarEnd * theReach - theDepth) * (kUUFarScale / theReach);
    }
    if (theVertical < theScale)
        theScale = theVertical;
    outPitch = 1.0f;
    outPan = (thePan < 1.0f ? thePan : 1.0f) > -1.0f ? (thePan < 1.0f ? thePan : 1.0f) : -1.0f;
    outVolume = ((theScale < 1.0f ? theScale : 1.0f) > 0.0f ? (theScale < 1.0f ? theScale : 1.0f) : 0.0f) *
                inVolume;
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
        gCommunications->ProcessLocalUserInput(new EEMReplantFarms(theUnit->mUnitID), kSlotNone);
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

// FUNCTION: EXE 0x6450f1
// Clears the living ambient units from under a new building's rectangle
bool EEClearUnderBuilding(long inX, long inY, long inSize)
{
    for (long theX = inX; theX < inX + inSize; theX++) {
        for (long theY = inY; theY < inY + inSize; theY++) {
            EETile* theTile = ::gWorld.mMap->GetTile(theX, theY);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit == NULL)
                    continue;
                EEUnitClass* theClass = theUnit->mUnitClass;
                if (dynamic_cast<EEUCAmbient*>(theClass) == NULL || theClass->mUnitType == kUnitTypePathPoint || !theClass->mIsDeletable)
                    continue;
                if (theUnit->mResourceAmount != 0)
                    theUnit->mResourceAmount = 0;
                theUnit->Kill(false, false, false);
                theUnit->mPlayer->KillUnit(theUnit, false);
            }
        }
    }
    return false;
}

// FUNCTION: EXE 0x6451df
// Returns the compass sector (0..7) of the direction from one tile to another
long EEGetTileDirection(long inX1, long inY1, long inX2, long inY2)
{
    float theAngle = UMArcTan2((float)(inX1 - inX2), (float)(inY2 - inY1));
    long theSector = 4;
    if (theAngle > kUUSectorHigh) {
        theSector = (long)((theAngle - kUUSectorHigh) * kUUSectorScale);
    } else if (theAngle < kUUSectorLow) {
        theSector = (long)(5.0f - (kUUSectorHigh + theAngle) * kUUSectorScale);
        if (theSector >= 8)
            theSector = 3;
    }
    return theSector;
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
    EEEffectHandle theHandle;
    float theBonus;
    long theLevel;
    long theFirst;
    unsigned long theSecond;
    inContainer->CanTake(inUnit, inUnit->mResourceType);
    long theCarried = inUnit->mResourceAmount;
    unsigned long theKind = inUnit->mResourceType;
    long theAmount = UMFloatToLong(::gWorld.GetGameVariant()->mDepositFactor * theCarried);
    bool theNear = false;
    theBonus = 0.0f;
    if ((theKind == kResourceGold || theKind == kResourceIron || theKind == kResourceStone) && inUnit->mLastResource != NULL
        && inContainer->DistanceSquaredToUnit(static_cast<EEUnit*>(inUnit->mLastResource)) <= kUUDropDistance) {
        theLevel = inContainer->mGarrisonCount;
        theFirst = max(0, min(theLevel, gUnitClassTable[kUnitTypeSettlement]->mGarrisonCapacity));
        theSecond = min((unsigned long)(theLevel - theFirst), gUnitClassTable[kUnitTypeTownCenter]->mGarrisonCapacity - theFirst);
        theBonus = (theLevel - theSecond - theFirst) * gUnitClassTable[kUnitTypeCapitol]->mGarrisonBonus
                   + gUnitClassTable[kUnitTypeTownCenter]->mGarrisonBonus * (long)theSecond + gUnitClassTable[kUnitTypeSettlement]->mGarrisonBonus * theFirst;
        theNear = true;
    } else if (theKind == kResourceFood && inUnit->mLastResource != NULL && static_cast<EEUnit*>(inUnit->mLastResource)->mUnitClass->mUnitType == kUnitTypeFarm
               && (inContainer->mUnitClass->mUnitType == kUnitTypeGranaryImproved || inContainer->mUnitClass->mUnitType == kUnitTypeGranary)
               && inContainer->DistanceSquaredToUnit(static_cast<EEUnit*>(inUnit->mLastResource)) <= kUUDropDistance) {
        theLevel = inContainer->mGarrisonCount;
        theFirst = max(0, min(theLevel, gUnitClassTable[kUnitTypeGranary]->mGarrisonCapacity));
        theSecond = min((unsigned long)(theLevel - theFirst), gUnitClassTable[kUnitTypeGranaryImproved]->mGarrisonCapacity - theFirst);
        theBonus = gUnitClassTable[kUnitTypeGranaryImproved]->mGarrisonBonus * (long)theSecond + gUnitClassTable[kUnitTypeGranary]->mGarrisonBonus * theFirst;
        theNear = true;
    }
    if (theNear && theBonus > 0.0f) {
        theAmount = UMFloatToLong((theBonus + 1.0f) * theAmount);
        ::gWorld.mEffectManager->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), &theHandle, 1, 0, 0);
    }
    inContainer->mPlayer->AddResource(inUnit->mResourceType, theAmount);
    // TEST ONLY (docs/AI_RESEARCH.md §8.51, EE_AI_TRACE with EE_AI_EXP deposits): each deposit of a computer player,
    // to time a gatherer's loads (trace only: nothing here changes the game)
    if (gCPTraceOn) {
        EEComputerPlayer* theComputer = dynamic_cast<EEComputerPlayer*>(inContainer->mPlayer);
        if (theComputer != NULL && EECPExperiment(theComputer, "deposits"))
            EE_CP_TRACE(theComputer, "deposit", "unit=%lu cls=%ld res=%lu carried=%ld amount=%ld bonus=%.3f store=%lu t=%lu",
                (unsigned long)inUnit->GetUnitID(), (long)inUnit->mUnitClass->mUnitType, theKind, theCarried, theAmount,
                (double)theBonus, (unsigned long)inContainer->GetUnitID(), (unsigned long)gPhysics->mGameTime);
    }
    switch (inUnit->mResourceType) {
    case kResourceFood:
        inContainer->mPlayer->mSavedGroups.Reset(7, theAmount);
        break;
    case kResourceWood:
        inContainer->mPlayer->mSavedGroups.Reset(8, theAmount);
        break;
    case kResourceStone:
        inContainer->mPlayer->mSavedGroups.Reset(10, theAmount);
        break;
    case kResourceGold:
        inContainer->mPlayer->mSavedGroups.Reset(9, theAmount);
        break;
    case kResourceIron:
        inContainer->mPlayer->mSavedGroups.Reset(0xb, theAmount);
        break;
    }
    if (inUnit->mResourceAmount != 0)
        inUnit->mResourceAmount = 0;
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
    long thePrevX = 0;
    long thePrevY = 0;
    unsigned long theCount = ioTiles.size();
    long theLastX = ioTiles[theCount - 1].mX;
    long theLastY = ioTiles[theCount - 1].mY;
    if (theCount > 1) {
        thePrevX = ioTiles[theCount - 2].mX;
        thePrevY = ioTiles[theCount - 2].mY;
    }
    for (long i = theCount - 3; i >= 0; i--) {
        if (i >= (long)theCount)
            continue;
        long theX = ioTiles[i].mX;
        long theY = ioTiles[i].mY;
        if (theLastX - theX == 1 && theLastY - theY == 1) {
            ioTiles.erase(ioTiles.begin() + i + 1);
            theCount = ioTiles.size();
        } else {
            theLastX = thePrevX;
            theLastY = thePrevY;
        }
        thePrevX = theX;
        thePrevY = theY;
    }
}

// FUNCTION: EXE 0x645903
// Collects the player's town centers on the corners and sides of a farm's footprint
bool EEGetFarmDropSites(EEComplexUnit* inUnit, std::vector<EEUnit*>& outUnits)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    long theSize = inUnit->mUnitClass->mTileSize;
    long theX = inUnit->GetTileX();
    long theY = inUnit->GetTileY();
    long theStart = -theSize;
    for (long theDX = -theSize; theDX <= theSize; theDX += theSize) {
        for (long theDY = theStart; theDY <= theSize; theDY += theSize) {
            unsigned long theType = inUnit->mUnitClass->mUnitType;
            if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary || theDX != 0 || theDY != 0)
                theTiles.push_back(U2DPoint<long>(theX + theDX, theDY + theY));
        }
    }
    EEGetUnitsOn(theTiles, theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        unsigned long theType = (*it)->mUnitClass->mUnitType;
        if ((theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary) && (*it)->mPlayer == inUnit->mPlayer)
            outUnits.push_back(*it);
    }
    return outUnits.size() > 0;
}

// FUNCTION: EXE 0x645a19
// Finds the nearest free farm of the unit's player, ring by ring (the ring reached goes back)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioRing)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EEUnit*> theUnits;
    long theRing = *ioRing;
    EETileCoordinate theFrom;
    EETileCoordinate theTile;
    EEComplexUnit* theResult = NULL;
    bool theFound = false;
    long theSize;
    long theLast = EEGetTownCenterLastRing(&theSize);
    std::pair<long, long> theCenter(inUnit->GetTileX(), inUnit->GetTileY());
    theFrom.mX = theCenter.first;
    theFrom.mY = theCenter.second;
    do {
        if (theRing > theLast)
            break;
        EEGetSlotTile(theFrom, theRing, theTile);
        theTiles.push_back(theTile);
        EEGetUnitsOn(theTiles, theUnits);
        for (unsigned long i = 0; !theFound && theUnits.size() > 0 && i < theUnits.size(); i++) {
            if (theUnits[i]->mTileNode == NULL)
                continue;
            EEComplexUnit* theOther = theUnits[i]->GetComplexUnit();
            unsigned long theType = theOther->GetClass()->GetUnitType();
            if ((theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm) || !(theOther->GetHitPoints() > 0) || theOther->GetPlayer() != inUnit->GetPlayer())
                continue;
            if (!inFreeOnly || (theType == kUnitTypeFarm && theOther->mIsBuilt && ::gWorld.GetResourceUsers(theOther) == 0)
                || (!theOther->GetIsBuilt() && theOther->mNumBuilders == 0)) {
                theFound = true;
                theResult = theOther;
            }
        }
        theTiles.clear();
        theRing++;
    } while (!theFound);
    *ioRing = theRing;
    return theResult;
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

static void EENewTargetRegions(void* inContext, int32_t inLeft, int32_t inTop, int32_t inRight, int32_t inBottom,
                               int32_t (*outRegions)[4])
{
    U2DRectangle<long> theArea;
    theArea.mLeft = inLeft;
    theArea.mTop = inTop;
    theArea.mRight = inRight;
    theArea.mBottom = inBottom;
    U2DRectangle<long> theRegions = gWorld.mMap->mUnitRegions->GetRegions(theArea);
    (*outRegions)[0] = (int32_t)theRegions.GetTopLeft().mX;
    (*outRegions)[1] = (int32_t)theRegions.GetTopLeft().mY;
    (*outRegions)[2] = (int32_t)theRegions.GetBottomRight().mX;
    (*outRegions)[3] = (int32_t)theRegions.GetBottomRight().mY;
}

static int32_t EENewTargetRegionLength(void* inContext, int32_t inX, int32_t inY)
{
    std::vector<EEComplexUnit*>* theRegion = gWorld.mMap->mUnitRegions->GetRegion((long)inX, (long)inY);
    return theRegion != NULL ? (int32_t)theRegion->size() : -1;
}

static uint64_t EENewTargetRegionUnit(void* inContext, int32_t inX, int32_t inY, int32_t inIndex)
{
    std::vector<EEComplexUnit*>* theRegion = gWorld.mMap->mUnitRegions->GetRegion((long)inX, (long)inY);
    return (uint64_t)(uintptr_t)(*theRegion)[inIndex];
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
    EeCombatTargetHost theHost = {&theContext, EENewTargetRegions, EENewTargetRegionLength, EENewTargetRegionUnit,
                                  EENewTargetScreen, EENewTargetMultiple, EENewTargetTest, EENewTargetPush};
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
    GE3DPoint theMiddle;
    GE3DPoint thePoint;
    GE3DLine theLine;
    unsigned long theBest = 0;
    std::vector<EEUnit*> theAttackers;
    long theReach = UMFloatToLong(inUnit->GetLineOfSight());
    bool theResult = false;
    float theAngle;
    float theGap;
    float theStart;
    unsigned long i;

    EEGetEnemiesAround(inUnit, theAttackers);
    if (theAttackers.size() != 0) {
        if (theAttackers.size() > 1) {
            std::sort(theAttackers.begin(), theAttackers.end(), EEUUAngleLess(inUnit));
            theStart = EEGetUnitAngle(inUnit, theAttackers[0]);
            theGap = EEGetUnitAngle(inUnit, theAttackers[1]) - theStart;
            theBest = 0;
            for (i = 1; i < theAttackers.size() - 1; i++) {
                float theFrom = EEGetUnitAngle(inUnit, theAttackers[i]);
                float theTo = EEGetUnitAngle(inUnit, theAttackers[(i + 1)]);
                if (!(theTo - theFrom > theGap))
                    continue;
                theGap = theTo - theFrom;
                theStart = theFrom;
                theBest = i;
            }
            theAngle = theGap * 0.5f + theStart;
        } else {
            theAngle = -EEGetUnitAngle(inUnit, theAttackers[0]);
        }
        float theAcross = -UMCos(1.5707964f - theAngle);
        float theAlong = UMCos(theAngle);
        EEUnit* theFirst = theAttackers[theBest];
        EEUnit* theSecond = *(theAttackers.begin() + (theBest + 1) % theAttackers.size());
        if (theFirst != theSecond) {
            GEVector theDirection(
                theSecond->mComplexUnit->mPhysicalModel.GetLocation().GetX() - theFirst->mComplexUnit->mPhysicalModel.GetLocation().GetX(),
                theSecond->mComplexUnit->mPhysicalModel.GetLocation().GetY() - theFirst->mComplexUnit->mPhysicalModel.GetLocation().GetY(),
                0.0f);
            if (theDirection.GetX() != 0.0f || theDirection.GetY() != 0.0f) {
                theLine.SetStart(GE3DPoint(theFirst->mComplexUnit->mPhysicalModel.GetLocation().GetX(),
                                           theFirst->mComplexUnit->mPhysicalModel.GetLocation().GetY(),
                                           theFirst->mComplexUnit->mPhysicalModel.GetLocation().GetZ()));
                theLine.SetDirection(theDirection);
                theLine.GetPointAt(0.5f, thePoint);
                float theHalf = theDirection.GetMagnitudeSquared();
                float theRadius = (float)((theReach + 1) * (theReach + 1));
                if (theHalf < theRadius) {
                    float theOut = (float)sqrt(theRadius - theHalf);
                    // the original rounds the x offset through a float slot (fstp [ebp-0x58]) and keeps the y
                    // offset in st (0x246585); folding both products into the sums moved P2's flee goal by 1 ulp
                    // (a differential game run showed the 1-ulp flee goal as a later yaw divergence)
                    GE2DPoint theOffset(theAcross * theOut, theAlong * theOut);
                    outLocation.SetX(thePoint.GetX() + theOffset.GetX());
                    outLocation.SetY(thePoint.GetY() + theOffset.GetY());
                } else {
                    outLocation.SetX(thePoint.GetX());
                    outLocation.SetY(thePoint.GetY());
                }
                theResult = ::gWorld.mMap->RTSMap::GetTile(UMFloor(outLocation.GetX()), UMFloor(outLocation.GetY())) != NULL;
                if (theResult)
                    return theResult;
            }
        }
    }
    if (theAttackers.size() != 0) {
        long theDirection = EETurnDirection(EEGetDirectionTo(inUnit, theAttackers[0]), 4, 1);
        float theDistance;
        if (inUnit->GetLineOfSight() > 0.0f)
            theDistance = inUnit->GetLineOfSight() * 0.5f;
        else
            theDistance = 1.0f;
        GE2DPoint theFrom((float)inUnit->mTileX, (float)inUnit->mTileY);
        EEMoveInDirection(theFrom, outLocation, theDirection, UMFloatToLong(theDistance));
        theResult = ::gWorld.mMap->GetTile(UMFloor(outLocation.GetX()), UMFloor(outLocation.GetY())) != NULL;
    }
    return theResult;
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
    outTiles.clear();
    outTiles.reserve((inWidth + inHeight) * 2);
    long theBottom = inHeight + inY;
    for (long theY = inY; theY <= theBottom - 1; theY++) {
        outTiles.push_back(U2DPoint<long>(inX - 1, theY));
        outTiles.push_back(U2DPoint<long>(inX + inWidth, theY));
    }
    long theRight = inX - 1 + inWidth;
    for (long theX = inX; theX <= theRight; theX++) {
        outTiles.push_back(U2DPoint<long>(theX, inY - 1));
        outTiles.push_back(U2DPoint<long>(theX, theBottom));
    }
}

// (the EETileCoordinate twin of 0x64697b is gone: one tile type)

























// FUNCTION: EXE 0x646a26
// Collects the tiles around a square (sides only)
void EEGetSideTiles(long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    EEGetSquareRing(inX, inY, inSize, inSize, outTiles);
}

// FUNCTION: EXE 0x646a42
// Collects the tiles around a rectangle, with corners or only the corners not closed by two wall pieces
// The tiles around the inWidth x inHeight rectangle at (inX, inY); the corners too when inCorners, else only those
// not closed by two wall pieces (appended to outTiles when inAppend)
void EEGetBlockRing(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<EETileCoordinate>& outTiles,
                bool inAppend)
{
    if (!inAppend) {
        outTiles.clear();
        outTiles.reserve((inWidth + inHeight) * 2 + 4);
    }
    long theBottom = inHeight + inY;
    for (long theY = inY; theY <= theBottom - 1; theY++) {
        outTiles.push_back(EETileCoordinate(inX - 1, theY));
        outTiles.push_back(EETileCoordinate(inWidth + inX, theY));
    }
    long theRight = inWidth + inX;
    for (long theX = inX; theX <= theRight - 1; theX++) {
        outTiles.push_back(EETileCoordinate(theX, inY - 1));
        outTiles.push_back(EETileCoordinate(theX, theBottom));
    }
    if ((char)inCorners || !EEIsCornerBlocked(inX - 1, inY, inX, inY - 1))
        outTiles.push_back(EETileCoordinate(inX - 1, inY - 1));
    if ((char)inCorners || !EEIsCornerBlocked(inX - 1, theBottom - 1, inX, inHeight + 1))
        outTiles.push_back(EETileCoordinate(inX - 1, theBottom));
    if ((char)inCorners || !EEIsCornerBlocked(theRight, inY, theRight - 1, inY - 1))
        outTiles.push_back(EETileCoordinate(theRight, inY - 1));
    if ((char)inCorners || !EEIsCornerBlocked(theRight - 1, theBottom, theRight, theBottom - 1))
        outTiles.push_back(EETileCoordinate(theRight, theBottom));
}

// FUNCTION: EXE 0x646bd0
// Collects the tiles around a square, corners as asked
void EEGetSquareBlockRing(long inCorners, long inX, long inY, long inSize, std::vector<EETileCoordinate>& outTiles, bool inAppend)
{
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
            EEUGRust* theGoal = EEGatherGoalCast(theUnit->mGoalState.Get());
            if (theGoal != NULL && theGoal->GetGatherResource() == inUnit)
                theResult = theUnit;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x646d8d
// Returns the tile of a unit's footprint nearest to a point and that squared distance
float EEGetNearestFootprintTile(EEComplexUnit* inUnit, const GE3DPoint& inPoint, long* outX, long* outY)
{
    float theBest = 3.4028235e+38f;
    GE3DPoint theCenter;
    long theSize = inUnit->mUnitClass->mTileSize;
    *outX = inUnit->GetTileX();
    *outY = inUnit->GetTileY();
    if (theSize > 1) {
        long theRight = inUnit->GetTileX() + theSize;
        long theBottom = inUnit->GetTileY() + theSize;
        for (long theX = inUnit->GetTileX(); theX < theRight; theX++) {
            for (long theY = inUnit->GetTileY(); theY < theBottom; theY++) {
                float theCenterX = (float)theX + 0.5f;
                theCenter.SetX(theCenterX);
                theCenter.SetY((float)theY + 0.5f);
                theCenter.SetZ(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(theCenter.GetX(), theCenter.GetY()));
                float theDistance = theCenter.DistanceToSquared(inPoint);
                if (!(theDistance < theBest))
                    continue;
                theBest = theDistance;
                *outX = theX;
                *outY = theY;
            }
        }
    } else {
        theCenter.SetX((float)inUnit->GetTileX() + 0.5f);
        theCenter.SetY((float)inUnit->GetTileY() + 0.5f);
        theBest = inPoint.DistanceToSquared(theCenter);
    }
    return theBest;
}

// FUNCTION: EXE 0x646ea2
// Returns the tile of a unit's footprint nearest to a tile's centre and that squared distance
float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY)
{
    GE3DPoint thePoint((float)inX + 0.5f, (float)inY + 0.5f, 0.0f);
    return EEGetNearestFootprintTile(inUnit, thePoint, outX, outY);
}

// FUNCTION: EXE 0x646edc
// Tells whether both tiles hold a wall piece
bool EEIsCornerBlocked(unsigned long inX1, unsigned long inY1, unsigned long inX2, unsigned long inY2)
{
    EEMap* theMap = ::gWorld.mMap;
    bool theFirst = false;
    bool theSecond = false;
    EETile* theTile = theMap->GetTile(inX1, inY1);
    EETile* theOther = theMap->GetTile(inX2, inY2);
    if (theTile == NULL || theOther == NULL)
        return false;
    RTSTileUnitNode* theNode;
    for (theNode = theTile->mUnits; theNode != NULL && theNode->mUnit != NULL && !theFirst; theNode = theNode->mNext)
        theFirst = theNode->mUnit->mUnitClass->mIsWall;
    for (theNode = theOther->mUnits; theNode != NULL && theNode->mUnit != NULL && !theSecond; theNode = theNode->mNext)
        theSecond = theNode->mUnit->mUnitClass->mIsWall;
    return theFirst && theSecond;
    return false;
}

// FUNCTION: EXE 0x646f93
// Returns the nearest piece of a target's linked group that the unit may attack
EEComplexUnit* EEGetAttackPart(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    EEComplexUnit* theResult = NULL;
    float theBest = 0.0f;
    bool theFound = false;
    EEComplexUnit* thePiece = inTarget;
    while (thePiece->mPreviousPart != NULL)
        thePiece = thePiece->mPreviousPart;
    if (!thePiece->mUnitClass->IsPartLeftOut(thePiece)) {
        theBest = inUnit->mComplexUnit->DistanceSquaredTo(thePiece);
        theResult = thePiece;
    }
    while (thePiece->mNextPart != NULL) {
        thePiece = thePiece->mNextPart;
        if (thePiece->mUnitClass->IsPartLeftOut(thePiece))
            continue;
        float theDistance = inUnit->mComplexUnit->DistanceSquaredTo(thePiece);
        if (theFound && !(theBest > theDistance))
            continue;
        theBest = theDistance;
        theResult = thePiece;
        theFound = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x647021
// Collects the four corner tiles of a rectangle (only the open ones unless corners are asked)
// The four corner tiles of the inWidth x inHeight rectangle at (inX, inY); with inCorners false only those not closed
// by two wall pieces
void EEGetRingCorners(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles)
{
    outTiles.clear();
    if ((char)inCorners || !EEIsCornerBlocked(inX - 1, inY, inX, inY - 1))
        outTiles.push_back(U2DPoint<long>(inX - 1, inY - 1));
    long theBottom = inY + inHeight;
    if ((char)inCorners || !EEIsCornerBlocked(inX - 1, theBottom - 1, inX, inHeight + 1))
        outTiles.push_back(U2DPoint<long>(inX - 1, theBottom));
    long theRight = inX + inWidth;
    if ((char)inCorners || !EEIsCornerBlocked(theRight, inY, theRight - 1, inY - 1))
        outTiles.push_back(U2DPoint<long>(theRight, inY - 1));
    if ((char)inCorners || !EEIsCornerBlocked(theRight - 1, theBottom, theRight, theBottom - 1))
        outTiles.push_back(U2DPoint<long>(theRight, theBottom));
}

// FUNCTION: EXE 0x647125
// Collects the living enemy units within a unit's reach
void EEGetEnemiesAround(EEComplexUnit* inUnit, std::vector<EEUnit*>& outAttackers)
{
    GE3DPoint thePosition;
    std::vector<EEUnit*> theUnits;
    unsigned long theCount;
    unsigned long i;
    outAttackers.clear();
    thePosition = GE3DPoint(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                            inUnit->mPhysicalModel.GetLocation().GetZ());
    EEGetUnitsNear(thePosition, inUnit->GetLineOfSight(), theUnits);
    theCount = theUnits.size();
    outAttackers.reserve(theCount);
    for (i = 0; i < theCount; i++) {
        EEUnit* theUnit = theUnits[i];
        if (theUnit->mHitPoints > 0 && theUnit->mPlayer != inUnit->mPlayer
            && inUnit->mPlayer->mEnemies[theUnit->mPlayer->GetPlayerID()] != 0)
            outAttackers.push_back(theUnit);
    }
}

// FUNCTION: EXE 0x64720e
// Collects the corner tiles of a square
void EEGetCornerTiles(long inCorners, long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles)
{
    EEGetRingCorners(inCorners, inX, inY, inSize, inSize, outTiles);
}

// FUNCTION: EXE 0x64722d
// Tells whether a unit can attack a target from where it stands (in range, not blocked)
bool EEIsInRangeToHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    long theA = 0;
    long theB = 0;
    EEUnit* theC;
    EeCombatRangeIn theIn = {};
    theIn.range = inUnit->mStats->mRange;
    theIn.crusader = inUnit->mUnitClass->IsCrusader();
    if (theIn.crusader)
        theIn.heal_range = inUnit->mUnitClass->mHealRange;
    theIn.distance_squared = EEGetHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
    theIn.building = inTarget->mUnitClass->IsLandBuilding();
    if (theIn.building)
        theIn.inner_sphere = inTarget->mUnitClass->mInnerCollisionSphere;
    bool theResult = EECombatRange(theIn) != 0;
    if (!theResult)
        return theResult;
    theResult = !inUnit->IsOnOtherContinentThan(static_cast<EEComplexUnit*>(inTarget));
    if (theResult && !inUnit->mUnitClass->mShootsHigh && !inUnit->mStats->mCanFireOverWalls)
        theResult = !EEIsLineToUnitBlocked(inUnit, inTarget, &theC, &theA, &theB);
    return theResult;
}

// FUNCTION: EXE 0x647316
// Tells whether two radii (plus a margin), scaled, reach across a squared distance
bool EECanReach(float inDistanceSquared, float inRadius, float inOtherRadius, float inScale)
{
    EeCombatReachIn theIn = {inDistanceSquared, inRadius, inOtherRadius, inScale, 0, 0};
    return EECombatCanReach(theIn) != 0;
}

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
    GE3DPoint theTargetPosition;
    GE3DPoint theOffset;
    GE2DPoint theTargetPoint;
    GE3DPoint theUnitPosition;
    GE2DPoint theUnitPoint;
    GE2DPoint theTile;
    float theDistance;

    inUnit->GetPosition(theUnitPosition);
    if (inTarget->mTileNode != NULL) {
        if (inTarget->mHitPoints <= 0 && inTarget->mComplexUnit != NULL && inTarget->GetModel()->mModel != NULL) {
            EEModel* theModel = inTarget->GetModel();
            theOffset = theModel->mTopPoint;
            float theScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
            theOffset.SetX(theOffset.GetX() * theScale);
            theOffset.SetY(theOffset.GetY() * theScale);
            inTarget->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, theTargetPosition);
        } else {
            inTarget->GetPosition(theTargetPosition);
        }
        theTargetPoint.SetX(theTargetPosition.GetX());
        theTargetPoint.SetY(theTargetPosition.GetY());
        theUnitPoint.SetX(theUnitPosition.GetX());
        theUnitPoint.SetY(theUnitPosition.GetY());
        theDistance = theTargetPoint.DistanceToSquared(theUnitPoint);
    } else {
        theTile.SetX((float)inTarget->GetTileX() + 0.5f);
        theTile.SetY((float)inTarget->GetTileY() + 0.5f);
        theDistance = inUnit->DistanceSquared2D(theTile);
    }
    return EEIsInGatherReach(theDistance, inUnit, inTarget);
}

// FUNCTION: EXE 0x6475a0
// Tells whether a location is within a unit's reach of a target
bool EEIsWithinReachOf(const GE3DPoint& inLocation, EEComplexUnit* inUnit, EEUnit* inTarget)
{
    GE3DPoint theTargetPosition;
    GE3DPoint theOffset;
    GE2DPoint theTargetPoint;
    GE2DPoint theTile;
    GE2DPoint theLocation = inLocation.Get2DPoint();
    float theDistance;

    if (inTarget->mTileNode != NULL) {
        if (inTarget->mHitPoints <= 0 && inTarget->mComplexUnit != NULL && inTarget->GetModel()->mModel != NULL) {
            EEModel* theModel = inTarget->GetModel();
            theOffset = theModel->mTopPoint;
            float theScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
            theOffset.SetX(theOffset.GetX() * theScale);
            theOffset.SetY(theOffset.GetY() * theScale);
            inTarget->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, theTargetPosition);
        } else {
            inTarget->GetPosition(theTargetPosition);
        }
        theTargetPoint.SetX(theTargetPosition.GetX());
        theTargetPoint.SetY(theTargetPosition.GetY());
        theDistance = theTargetPoint.DistanceToSquared(theLocation);
    } else {
        theTile.SetX((float)inTarget->GetTileX() + 0.5f);
        theTile.SetY((float)inTarget->GetTileY() + 0.5f);
        theDistance = theLocation.DistanceToSquared(theTile);
    }
    return EEIsInGatherReach(theDistance, inUnit, inTarget);
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
    float theDistance = inDistanceSquared;
    float theRadius = inTarget->mUnitClass->GetInnerCollisionSphere();
    if (inDistanceSquared < 0.0f)
        theDistance = EEGetHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
    if (inTarget->mUnitClass->mIsWall)
        theRadius = 1.0f;
    return EECanReach(theDistance, inUnit->mUnitClass->GetInnerCollisionSphere(), theRadius, kUUContactMargin);
}

// FUNCTION: EXE 0x6477f9
// Tells whether a unit may target a class (walls only by siege units or computer players)
bool EECanTargetClass(EEComplexUnit* inUnit, EEUnitClass* inClass)
{
    bool theResult = EEUUGetIsUnitClassTargetable(inUnit, inClass);
    if (!theResult || inUnit == NULL || (inClass->mUnitType != kUnitTypeBalloon10ObservationBalloon && inClass->mUnitType != kUnitTypeBalloon09HotAirBalloon))
        return theResult;
    if (!inUnit->mUnitClass->IsLandBuilding() && inUnit->mPlayer->mComputerPlayer == NULL)
        theResult = false;
    else
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x647848
// Tells whether a class can be targeted at all (by an attacker's ammunition when given)
bool EEUUGetIsUnitClassTargetable(EEUnit* inAttacker, EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass != NULL) {
        if (inAttacker != NULL && inAttacker->mUnitClass->mAmmoType == 0x3c) {
            unsigned long theKind = inClass->mFamily;
            if (!inClass->CanBeAttacked() || EEDamageFactor(theKind, 0x3c) <= 0.0f)
                return false;
        } else {
            long theCategory = inClass->mCategory;
            if (theCategory == kUnitCategoryProjectile || theCategory == kUnitCategoryBomb || theCategory == kUnitCategoryMissile || theCategory == kUnitCategoryTorpedo)
                return false;
        }
        unsigned long theType = inClass->mUnitType;
        theResult = true;
        if (theType == kUnitTypeHurricane || theType == kUnitTypeWaypoint || theType == kUnitTypeTankObstacle || theType == kUnitTypeVolcano)
            return false;
        if (theType > kUnitTypeX11HauptmannDurerProphet && (theType <= kUnitTypeFlare2 || theType == kUnitTypeAntiMatterStorm))
            return false;
    }
    return theResult;
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
    std::vector<EETileCoordinate> theTiles;
    unsigned long i = 0;
    bool theResult = false;
    EEMap* theMap = ::gWorld.mMap;
    if (theMap == NULL)
        return theResult;
    if (inUnit->mUnitClass->mTileSize > 1) {
        EEGetFootprintTiles(inUnit, theTiles);
        unsigned long theCount = theTiles.size();
        for (; i < theCount; i++) {
            if (theResult)
                break;
            EETile* theTile = theMap->GetTileAtLocation(theTiles[i]);
            if (theTile != NULL && theTile->IsVisibleTo(inPlayer))
                theResult = true;
            else
                theResult = false;
        }
    } else {
        if (inUnit->mTile != NULL && static_cast<EETile*>(inUnit->mTile)->IsVisibleTo(inPlayer))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x6479eb
// Tells whether a player sees a unit (explored may be enough; cloaked enemies never)
// Whether inPlayer sees inUnit (inExplored: explored is enough; a cloaked enemy is never seen; any tile of a large
// unit's footprint)
bool EEIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
{
    std::vector<EETileCoordinate> theTiles;
    EEMap* theMap = ::gWorld.mMap;
    bool theResult = true;
    if (theMap != NULL && inPlayer->mSaveVisMap) {
        if (inUnit != NULL) {
            EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
            if (theComplexUnit == NULL || theComplexUnit->mIsCloakable && theComplexUnit->mIsCloaked && inUnit->mPlayer != inPlayer
                && inPlayer->mEnemies[inUnit->mPlayer->GetPlayerID()] != 0)
                theResult = theComplexUnit == NULL;
        }
        if (theResult) {
            theResult = false;
            if (inUnit->mUnitClass->mTileSize > 1 || inUnit->mUnitClass->mCategory == kUnitCategoryGate) {
                EEGetFootprintTiles(inUnit, theTiles);
                if (inExplored) {
                    unsigned long theCount = theTiles.size();
                    for (unsigned long i = 0; i < theCount; i++) {
                        if (theResult)
                            break;
                        EETileCoordinate& thePoint = theTiles[i];
                        EETile* theTile = theMap->GetTile(thePoint.mX, thePoint.mY);
                        theResult = theTile != NULL && theTile->IsSeenBy(inPlayer);
                    }
                } else {
                    unsigned long theCount = theTiles.size();
                    for (unsigned long i = 0; i < theCount; i++) {
                        if (theResult)
                            break;
                        EETileCoordinate& thePoint = theTiles[i];
                        EETile* theTile = theMap->GetTile(thePoint.mX, thePoint.mY);
                        theResult = theTile != NULL && theTile->GetVisibility(inPlayer->GetPlayerID()) > 1;
                    }
                }
            } else {
                EETile* theTile = theMap->GetTileAtLocation(EETileLocation(inUnit->GetTileX(), inUnit->GetTileY()));
                if (inExplored == true)
                    theResult = theTile != NULL && theTile->IsSeenBy(inPlayer);
                else
                    theResult = theTile != NULL && theTile->GetVisibility(inPlayer->GetPlayerID()) > 1;
            }
        }
    }
    return theResult;
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
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theRing;
    std::vector<EETileCoordinate> theLine;
    std::vector<EEUnit*> theUnits;
    EETileCoordinate theFrom;
    EETileCoordinate theTo;
    long theResult;
    theFrom.mX = 0;
    theFrom.mY = 0;
    theTo.mX = 0;
    theTo.mY = 0;
    if (inTiles.size() >= 2) {
        theFrom = inTiles[1];
        theTo = inTiles[0];
        EEExtendWall(theFrom, theTo, inPlayer, inClass->mUnitType, outTiles);
        theResult = outTiles.size();
        outTiles.insert(outTiles.end(), inTiles.begin(), inTiles.end());
        theFrom = inTiles[inTiles.size() - 2];
        theTo = inTiles[inTiles.size() - 1];
        EEExtendWall(theFrom, theTo, inPlayer, inClass->mUnitType, theLine);
        outTiles.insert(outTiles.end(), theLine.rbegin(), theLine.rend());
    } else {
        EEGetSideTiles(inTiles[0].GetX(), inTiles[0].GetY(), 1, theTiles);
        EEGetCornerTiles(0, inTiles[0].GetX(), inTiles[0].GetY(), 1, theRing);
        theTiles.insert(theTiles.begin(), theRing.begin(), theRing.end());
        EEGetUnitsOfTypeOnTiles(theTiles, inPlayer, inClass->mUnitType, theUnits);
        if (theUnits.size() == 2) {
            theFrom = inTiles[0];
            theTo.mX = theUnits[0]->mTileX;
            theTo.mY = theUnits[0]->mTileY;
            EEExtendWall(theFrom, theTo, inPlayer, inClass->mUnitType, outTiles);
            outTiles.push_back(theTo);
            outTiles.push_back(theFrom);
            long theY = theUnits[1]->mTileY;
            EETileCoordinate theOther(theUnits[1]->mTileX, theY);
            outTiles.push_back(theOther);
            theFrom = inTiles[0];
            theTo.mX = theUnits[1]->mTileX;
            theTo.mY = theUnits[1]->mTileY;
            EEExtendWall(theFrom, theTo, inPlayer, inClass->mUnitType, theLine);
            outTiles.insert(outTiles.end(), theLine.rbegin(), theLine.rend());
        } else if (theUnits.size() == 1) {
            theFrom = inTiles[0];
            theTo.mX = theUnits[0]->GetTileX();
            theTo.mY = theUnits[0]->GetTileY();
            EEExtendWall(theFrom, theTo, inPlayer, inClass->mUnitType, outTiles);
            outTiles.push_back(theTo);
            outTiles.push_back(theFrom);
        } else {
            outTiles.insert(outTiles.end(), inTiles.begin(), inTiles.end());
        }
        theResult = 0;
    }
    return theResult;
}

// TEMPLATE: EXE 0x6481da
// (the reverse_iterator copy ctor, out of line right after its first user 0x247bef)

// FUNCTION: EXE 0x6481eb
// Finds a free spot on the ring around a target for a unit (nearest of eight points first)
// A free spot on the ring around inTarget for inUnit (the eight points at the two radii, nearest first; inTarget's
// own position if none is free) (inferred)
bool EEFindOpenSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    std::vector<GE2DPoint> thePoints;
    GE3DPoint thePoint;
    std::vector<EETileCoordinate> theUnused1;
    GE3DPoint theTargetPosition;
    GE3DPoint theOffset;
    std::vector<EETileCoordinate> theUnused2;
    std::vector<EEUnit*> theUnits;
    float theTargetRadius = inTarget->GetUnitClass()->GetInnerCollisionSphere();
    float theRadius = inUnit->GetUnitClass()->GetInnerCollisionSphere();
    if (inTarget->GetHitPoints() <= 0 && inTarget->mComplexUnit != NULL && inTarget->GetModel()->mModel != NULL) {
        EEModel* theModel = inTarget->GetModel();
        theOffset = theModel->mTopPoint;
        theOffset.SetX(theOffset.GetX() * inTarget->mComplexUnit->mPhysicalModel.GetScale());
        theOffset.SetY(theOffset.GetY() * inTarget->mComplexUnit->mPhysicalModel.GetScale());
        inTarget->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, theTargetPosition);
    } else {
        inTarget->GetPosition(theTargetPosition);
    }
    float theX = theTargetPosition.GetX();
    float theY = theTargetPosition.GetY();
    float theHalf = theRadius * 0.5f;
    float theTargetHalf = theTargetRadius * 0.5f;
    float theDistance = theTargetHalf + theHalf;
    thePoints.push_back(GE2DPoint(theX, theDistance + theY));
    thePoints.push_back(GE2DPoint(theDistance + theX, theY));
    thePoints.push_back(GE2DPoint(theX, theY - theTargetHalf - theHalf));
    thePoints.push_back(GE2DPoint(theX - theTargetHalf - theHalf, theY));
    float theRight = theDistance + theX;
    float theTop = theDistance + theY;
    thePoints.push_back(GE2DPoint(theRight, theTop));
    float theBottom = theY - theDistance;
    thePoints.push_back(GE2DPoint(theRight, theBottom));
    float theLeft = theX - theDistance;
    thePoints.push_back(GE2DPoint(theLeft, theBottom));
    thePoints.push_back(GE2DPoint(theLeft, theTop));
    std::sort(thePoints.begin(), thePoints.end(), EEUUPointNearerToUnit(inUnit));
    EEGetUnitsNearTile(theTargetPosition, theDistance + 2.0f, theUnits);
    unsigned long i = 0;
    bool theFree;
    do {
        thePoint.SetX(thePoints[i].GetX());
        thePoint.SetY(thePoints[i].GetY());
        if (inUnit->mActiveGroup != NULL)
            inUnit->mActiveGroup->IsReserved(thePoint);
        EETileCoordinate theTile;
        theTile.mX = (long)(thePoints[i]).GetX();
        theTile.mY = (long)(thePoints[i]).GetY();
        theFree = EECanStandOn(theTile, inUnit);
        std::vector<EEUnit*>::iterator it;
        for (it = theUnits.begin(); it != theUnits.end() && theFree; it++) {
            EEUnit* theOther = *it;
            if (theOther->GetUnitClass()->mIsTree && theOther->mTileX == UMFloor(thePoints[i].GetX())
                && theOther->mTileY == UMFloor(thePoints[i].GetY())) {
                theFree = false;
            } else if (theOther->GetHitPoints() > 0 && (theOther->GetUnitClass()->mBlocksTile || theOther->GetUnitClass()->mIsCollidable)) {
                theOther->GetPosition(theOffset);
                GE2DPoint theOtherPoint(theOffset.GetX(), theOffset.GetY());
                float theSquared = thePoints[i].DistanceToSquared(theOtherPoint);
                float theReach = (theOther->GetUnitClass()->GetInnerCollisionSphere() + theRadius) * 0.5f;
                if (theSquared + kUUClearance * theRadius < theReach * theReach)
                    theFree = false;
                else
                    theFree = true;
            }
        }
        if (!theFree)
            i++;
    } while (i < thePoints.size() && !theFree);
    if (theFree) {
        outLocation.SetX(thePoints[i].GetX());
        outLocation.SetY(thePoints[i].GetY());
    } else {
        outLocation = theTargetPosition;
    }
    return theFree;
}

// FUNCTION: EXE 0x648709
// Finds a place for a unit next to a target, preferring its own path area and continent
// A place for inUnit next to inTarget, preferring tiles in inUnit's own path area and continent (inferred)
bool EEFindResourceSpot(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EETileCoordinate> theOthers;
    bool theResult = false;
    if (inTarget->mStats != NULL && inTarget->mStats->mMaxVelocity > 0.0f) {
        theResult = EEFindOpenSpot(inUnit, inTarget, outLocation);
    } else {
        EEGetTilesAroundUnit(inUnit, inTarget, 1, 0, 1, theTiles);
        EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY));
        std::stable_sort(theTiles.begin(), theTiles.end(), EEUUTileInPathArea(theArea ? theArea->mID : 0));
        if (inUnit != NULL && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen) {
            EETileCoordinate theTile;
            theTile.mX = inUnit->mTileX;
            theTile.mY = inUnit->mTileY;
            unsigned short theContinent = gContinentManager.GetContinent(theTile, (inUnit->mUnitClass->mDomains & kMovementSea) ? 1 : 2);
            long i;
            for (i = theTiles.size() - 1; i >= 0; i--) {
                if (theContinent == 0 || theContinent == gContinentManager.GetContinent(theTiles[i],
                        (inUnit->mUnitClass->mDomains & kMovementSea) ? 1 : 2))
                    continue;
                EEAIContinent* theFrom = gContinentManager.GetContinentAt(inUnit->GetTileX(), inUnit->mTileY);
                EEAIContinent* theTo = gContinentManager.GetContinentAt(theTiles[i].GetX(),
                                                                        theTiles[i].GetY());
                if (gContinentManager.AreConnectedForUnit(inUnit, theTo, theFrom))
                    theOthers.push_back(theTiles[i]);
                theTiles.erase(theTiles.begin() + i);
            }
            if (theTiles.empty() && !theOthers.empty())
                theTiles = theOthers;
        }
        if (theTiles.size() != 0)
            theResult = EEPickTileNear(theTiles, inUnit, inTarget, 0, outLocation);
    }
    return theResult;
}

// FUNCTION: EXE 0x64899b
// Finds a free spot next to a target (or the nearest piece of its group) for a unit
bool EEFindNextFreeTile(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    EEComplexUnit* theTarget = inTarget;
    bool theOuter = true;
    if (theTarget->mNextPart != NULL) {
        theTarget = EEGetAttackPart(inUnit, theTarget);
        theOuter = theTarget->mPreviousPart == NULL;
    }
    EEGetTilesAroundUnit(inUnit, theTarget, 1, 0, theOuter, theTiles);
    return EEPickTileNear(theTiles, inUnit, theTarget, 0, outLocation);
}

// FUNCTION: EXE 0x648a15
// Drops the tiles on the far side of a target's wall or gate line from the unit
// Drops the tiles of ioTiles on the other side of inTarget's wall (or gate) line from inUnit (inferred)
void EEGetWallSideTiles(EEComplexUnit* inUnit, EEComplexUnit* inTarget, std::vector<EETileCoordinate>& ioTiles)
{
    GE3DPlane thePlane;
    GE3DPoint theUnitPosition;
    GE3DPoint theTilePoint;
    GE3DPoint theFirst;
    GE3DPoint theSecond;
    GE3DPoint theThird;
    bool theDefined = false;
    EEUnitClass* theClass = inTarget->mUnitClass;
    if (theClass->mCategory != kUnitCategoryGate && !theClass->mIsWall)
        return;
    if (theClass->mCategory == kUnitCategoryGate) {
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(theClass);
        theGate->GetLastLinkedCarrier(inTarget->GetOutermostCarrier())->GetPosition(theFirst);
        theGate->GetLastLinked(inTarget->GetOutermostCarrier())->GetPosition(theSecond);
        theThird.SetX(theSecond.GetX());
        theThird.SetY(theSecond.GetY());
        theThird.SetZ(theSecond.GetZ() + 10.0f);
        theDefined = true;
    } else {
        std::vector<EETileCoordinate> theTiles;
        std::vector<EETileCoordinate> theLine;
        EETileCoordinate theTile;
        theTile.mX = inTarget->mTileX;
        theTile.mY = inTarget->mTileY;
        theTiles.push_back(theTile);
        EEExtendWallLine(theTiles, inTarget->mPlayer, inTarget->mUnitClass, theLine);
        if (theLine.size() > 3) {
            theFirst.SetX((float)theLine.front().mX);
            theFirst.SetY((float)theLine.front().mY);
            theSecond.SetX((float)theLine.back().mX);
            theSecond.SetY((float)theLine.back().mY);
            theThird.SetX(theSecond.GetX());
            theThird.SetY(theSecond.GetY());
            theDefined = true;
            theThird.SetZ(theSecond.GetZ() + 10.0f);
        }
    }
    if (!theDefined)
        return;
    thePlane.Define(theFirst, theSecond, theThird);
    inUnit->GetPosition(theUnitPosition);
    bool theSide = thePlane.IsPointOnOrAbovePlane(theUnitPosition);
    std::vector<EETileCoordinate>::iterator it = ioTiles.begin();
    while (it != ioTiles.end()) {
        theTilePoint.SetX((float)it->mX);
        theTilePoint.SetY((float)it->mY);
        if (theSide == thePlane.IsPointOnOrAbovePlane(theTilePoint))
            it++;
        else
            it = ioTiles.erase(it);
    }
}

// FUNCTION: EXE 0x648c7e
// Finds a place for a unit to attack a target from (around a wall it stands in, or next to it)
bool EEGetWallSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    bool theResult;
    if (inTarget->mStats->mMaxVelocity <= 0.0f) {
        EEGetTilesAroundUnit(inUnit, static_cast<EEComplexUnit*>(inTarget), 1, 0, 0, theTiles);
        EEGetWallSideTiles(inUnit, inTarget->mComplexUnit, theTiles);
        theResult = EEPickTileNear(theTiles, inUnit, static_cast<EEComplexUnit*>(inTarget), 0, outLocation);
    } else {
        theResult = EEFindOpenSpot(inUnit, inTarget, outLocation);
    }
    return theResult;
}

// FUNCTION: EXE 0x648d05
// Returns a unit's radius seen from another: full in front or behind, down to a tenth from the side
float EEGetUnitRadius(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    float theOtherX = inOther->mPhysicalModel.GetLocation().GetX();
    float theX = inUnit->mPhysicalModel.GetLocation().GetX();
    float theOtherY = inOther->mPhysicalModel.GetLocation().GetY();
    float theY = inUnit->mPhysicalModel.GetLocation().GetY();
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theHeading;
    inUnit->mPhysicalModel.GetLocation().Apply(theForward, theHeading);
    float theFacing = UMArcTan2(-theHeading.GetX(), theHeading.GetY());
    float theAngle = UMAbs(theFacing - UMArcTan2(theOtherX - theX, -(theOtherY - theY)));
    if (theAngle > 3.1415927f)
        theAngle = 6.2831855f - theAngle;
    if (theAngle > 1.5707964f)
        theAngle = 3.1415927f - theAngle;
    theAngle = 1.0f - theAngle * kUUTwoOverPi;
    if (theAngle <= kUUMinimumFacing)
        theAngle = kUUMinimumFacing;
    return inUnit->mUnitClass->GetInnerCollisionSphere() * theAngle;
}

// FUNCTION: EXE 0x648e06
// Returns the tile a distance away in a compass direction
void EEGetTileInDirection(long inX, long inY, long* outX, long* outY, long inDirection, long inDistance)
{
    switch (inDirection) {
    case 0:
        *outX = inX - inDistance;
        *outY = inY + inDistance;
        break;
    case 1:
        *outX = inX - inDistance;
        *outY = inY;
        break;
    case 2:
        *outX = inX - inDistance;
        *outY = inY - inDistance;
        break;
    case 3:
        *outX = inX;
        *outY = inY - inDistance;
        break;
    case 4:
        *outX = inX;
        *outY = inY + inDistance;
        break;
    case 5:
        *outX = inX + inDistance;
        *outY = inY + inDistance;
        break;
    case 6:
        *outX = inX + inDistance;
        *outY = inY;
        break;
    case 7:
        *outX = inX + inDistance;
        *outY = inY - inDistance;
        break;
    default:
        *outX = inX;
        *outY = inY;
        break;
    }
}

// FUNCTION: EXE 0x648ecc
// Returns the tile a distance away in a compass direction (tile form)
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance)
{
    long theX;
    long theY;
    EEGetTileInDirection(inTile.mX, inTile.mY, &theX, &theY, inDirection, inDistance);
    outTile.mX = theX;
    outTile.mY = theY;
}

// (the U2DPoint<long> twin of 0x648ecc is gone: one tile type)










// FUNCTION: EXE 0x648efd
// Moves a point a number of tiles in a compass direction, keeping its offset within the tile
void EEMoveInDirection(const GE2DPoint& inFrom, GE2DPoint& outTo, long inDirection, long inDistance)
{
    long theX = UMFloor(inFrom.GetX());
    long theY = UMFloor(inFrom.GetY());
    float theOffsetX = inFrom.GetX() - theX;
    float theOffsetY = inFrom.GetY() - theY;
    EEGetTileInDirection(theX, theY, &theX, &theY, inDirection, inDistance);
    outTo.SetX(theX + theOffsetX);
    outTo.SetY(theY + theOffsetY);
}

// FUNCTION: EXE 0x648f86
// Turns a compass direction by a random amount within a spread
long EETurnAway(long inDirection, long inSpread)
{
    long theTurn = gPhysics->GetRandom(0L, inSpread * 2 + 1) + (4 - inSpread);
    long theResult = inDirection;
    bool theSide = gPhysics->GetRandom(0L, 1L) != 0;
    if (theTurn != 0)
        theResult = EETurnDirection(theResult, theTurn % 8, theSide);
    return theResult;
}

// FUNCTION: EXE 0x648fe4
// Collects the tiles a unit covers, widened by a margin (a line along a ship's heading, else a disc)
// The tiles inUnit covers, widened by inMargin (a line along a ship's heading; a disc otherwise) (inferred)
void EEGetUnitTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles, float inMargin)
{
    GE3DPoint thePosition;
    GE3DPoint theFront;
    GE3DPoint theBack;
    GEVector theDirection;
    GE3DLine theLine;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (inUnit->mTileX == -10000 || inUnit->mTileY == -10000)
        return;
    if (inUnit->mUnitClass->GetTileSize() > 1) {
        EEGetFootprintTiles(inUnit, outTiles);
    } else if (theUnit != NULL && (inUnit->mUnitClass->mDomains & kMovementSea)) {
        theUnit->mPhysicalModel.GetLocation().Apply(theForward, theDirection);
        theDirection.Normalize();
        theDirection.Scale(inMargin + inUnit->mUnitClass->GetInnerCollisionSphere(), theDirection);
        inUnit->GetPosition(thePosition);
        theLine.SetStart(thePosition);
        theLine.SetDirection(theDirection);
        theLine.GetPointAt(0.5f, theFront);
        theLine.GetPointAt(-0.5f, theBack);
        EEGetTileLine(UMFloor(theFront.GetX()), UMFloor(theFront.GetY()), UMFloor(theBack.GetX()),
                   UMFloor(theBack.GetY()), &outTiles, 1, 0);
    } else {
        inUnit->GetPosition(thePosition);
        EEGetTilesNearPoint(thePosition, inMargin + inUnit->mUnitClass->GetInnerCollisionSphere(), outTiles);
    }
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
    long theSide = inRadius * 2 + 1;
    long theInner = inRadius * 2 - 1;
    long i;
    outTiles.clear();
    outTiles.reserve((theInner + theSide) * 2);
    long theLeft = inX - inRadius;
    for (i = 0; i < theSide; i++)
        outTiles.push_back(U2DPoint<long>(i + theLeft, inY + inRadius));
    long theY = inY - 1 + inRadius;
    for (long j = theInner; j > 0; j--) {
        outTiles.push_back(U2DPoint<long>(theLeft, theY));
        outTiles.push_back(U2DPoint<long>(inX + inRadius, theY));
        theY--;
    }
    for (i = 0; i < theSide; i++)
        outTiles.push_back(U2DPoint<long>(theLeft + i, inY - inRadius));
}

// FUNCTION: EXE 0x64939d
// Finds where a dropped unit lands on a target: the model's drop point (lower when damaged) or its tile
// Where a unit dropped onto inTarget (its goal's target when NULL) lands: the model's drop point (scaled down with
// a damaged unit's health), else its tile; *outTarget the target (NULL when not placed)
// (new: the model's and the tile's point are ee_combat's drop_point; the goal's location and the target stay here)
void EEFindDropLocation(EEComplexUnit* inUnit, EEUnit** outTarget, GE3DPoint* outLocation, EEUnit* inTarget)
{
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    GE3DPoint theLocation;
    EeCombatDropIn in = {};
    if (inTarget == NULL && (theGoal == NULL || theGoal->mKind != kGoalAttackUnit)) {
        in.kind = EE_COMBAT_DROP_KIND_GOAL;
        theGoal->GetGoalLocation(theLocation);
    } else {
        if (inTarget == NULL)
            inTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
        if (inTarget->mComplexUnit != NULL && inTarget->GetModel()->mModel != NULL) {
            in.kind = EE_COMBAT_DROP_KIND_MODEL;
            in.offset[0] = inTarget->GetModel()->mDefaultPoint.GetX();
            in.offset[1] = inTarget->GetModel()->mDefaultPoint.GetY();
            in.offset[2] = inTarget->GetModel()->mDefaultPoint.GetZ();
            in.scale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
            in.built = inTarget->mComplexUnit->mIsBuilt;
            in.hit_points = (int32_t)inTarget->mHitPoints;
            in.max_hit_points = (int32_t)inTarget->mStats->mMaxHitPoints;
            memcpy(in.transformation, inTarget->mComplexUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.transformation));
        } else if (inTarget->mTile != NULL) {
            in.kind = EE_COMBAT_DROP_KIND_TILE;
            in.tile_x = (int32_t)inTarget->GetTileX();
            in.tile_y = (int32_t)inTarget->GetTileY();
            in.tile_height = inTarget->mTile->GetHeight();
        } else {
            in.kind = EE_COMBAT_DROP_KIND_NONE;
            inTarget = NULL;
        }
        if (in.kind != EE_COMBAT_DROP_KIND_NONE) {
            EeCombatDropOut thePoint = ee_combat_drop_point(&in);
            theLocation.SetX(thePoint.x);
            theLocation.SetY(thePoint.y);
            theLocation.SetZ(thePoint.z);
        }
    }
    *outTarget = inTarget;
    *outLocation = theLocation;
    if (EESimCaptureRecord::Active()) {
        struct { float x, y, z; int32_t target; } theOut = {outLocation->GetX(), outLocation->GetY(), outLocation->GetZ(),
                                                            EECombatCaptureID(*outTarget)};
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'P'), in, theOut);
    }
}

static void EEFindDropLocationOriginal(EEComplexUnit* inUnit, EEUnit** outTarget, GE3DPoint* outLocation, EEUnit* inTarget)
{
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    GE3DPoint theOffset;
    GE3DPoint theLocation;
    if (inTarget == NULL && (theGoal == NULL || theGoal->mKind != kGoalAttackUnit)) {
        theGoal->GetGoalLocation(theLocation);
    } else {
        if (inTarget == NULL)
            inTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
        if (inTarget->mComplexUnit != NULL && inTarget->GetModel()->mModel != NULL) {
            theOffset = inTarget->GetModel()->mDefaultPoint;
            float theScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
            theOffset.SetX(theOffset.GetX() * theScale);
            theOffset.SetY(theOffset.GetY() * theScale);
            theOffset.SetZ(theOffset.GetZ() * theScale);
            if (!inTarget->mComplexUnit->mIsBuilt)
                theOffset.SetZ(theOffset.GetZ() * ((float)inTarget->mHitPoints / (float)inTarget->mStats->mMaxHitPoints));
            inTarget->mComplexUnit->mPhysicalModel.GetLocation().Apply(theOffset, theLocation);
        } else if (inTarget->mTile != NULL) {
            theLocation.SetX((float)inTarget->GetTileX() + 0.5f);
            theLocation.SetY((float)inTarget->GetTileY() + 0.5f);
            theLocation.SetZ(inTarget->mTile->GetHeight());
        } else {
            inTarget = NULL;
        }
    }
    *outTarget = inTarget;
    *outLocation = theLocation;
}

// FUNCTION: EXE 0x6494cb
// Returns the squared ground distance from a unit (or a point) to where it would hit a target
float EEGetHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
{
    GE3DPoint theTarget;
    GE3DPoint thePosition;
    EEUnit* theDropTarget;
    inUnit->GetPosition(thePosition);
    if (!inUnit->mUnitClass->mIsMelee)
        EEFindDropLocation(inUnit, &theDropTarget, &theTarget, inTarget);
    else
        inTarget->GetPosition(theTarget);
    // the arithmetic is ee_combat's hit_distance (screen.rs)
    EeCombatHitDistanceIn in = {{thePosition.GetX(), thePosition.GetY(), thePosition.GetZ()},
                                {theTarget.GetX(), theTarget.GetY(), theTarget.GetZ()}, inX, inY};
    float theDistance = ee_combat_hit_distance(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'H', 'D'), in, theDistance);
    return theDistance;
}

// FUNCTION: EXE 0x649560
// Returns a point 90% of a unit's range from a target towards the unit
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outPoint)
{
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
    float theDistanceSquared = inDistance * inDistance;
    long theLeft = UMFloor(inPoint.GetX() - inDistance);
    long theTop = UMFloor(inPoint.GetY() - inDistance);
    long theRight = UMFloor(inPoint.GetX() + inDistance);
    long theBottom = UMFloor(inPoint.GetY() + inDistance);
    if (inDistance > 1.0f) {
        for (long theX = theLeft; theX <= theRight; theX++) {
            float theCenterX = (float)theX + 0.5f;
            for (long theY = theTop; theY <= theBottom; theY++) {
                float theNearX = theCenterX < inPoint.GetX() ? (float)(theX + 1) : (float)theX;
                float theNearY = (float)theY + 0.5f < inPoint.GetY() ? (float)(theY + 1) : (float)theY;
                if ((inPoint.GetY() - theNearY) * (inPoint.GetY() - theNearY)
                    + (inPoint.GetX() - theNearX) * (inPoint.GetX() - theNearX) <= theDistanceSquared)
                    outTiles.push_back(EETileCoordinate(theX, theY));
            }
        }
    } else {
        outTiles.push_back(EETileCoordinate(UMFloor(inPoint.GetX()), UMFloor(inPoint.GetY())));
    }
}

// FUNCTION: EXE 0x64b3ed
// Collects the tiles under a fan of rays from a centre, sorted and unique
void EEGetFanTiles(const GE2DPoint& inCenter, float inRadius, float inFrom, float inTo, long inSteps,
                std::vector<U2DPoint<long> >& outTiles)
{
    std::vector<U2DPoint<long> > theLine;
    long theLastX = 0;
    long theLastY = 0;
    long theY = UMFloor(inCenter.GetY());
    long theX = UMFloor(inCenter.GetX());
    float theStep = (inTo - inFrom) / (float)inSteps;
    for (long i = inSteps; i >= 0; i--) {
        long theEndX = UMFloor(UMCos(inFrom) * inRadius + (float)theX);
        long theEndY = UMFloor(UMCos(1.5707964f - inFrom) * inRadius + (float)theY);
        if (theEndX != theLastX && theEndY != theLastY) {
            EEGetTileLine(theX, theY, theEndX, theEndY, &theLine, 1, 0);
            for (long j = 0; j < (long)theLine.size(); j++)
                outTiles.push_back(theLine[j]);
            theLastX = theEndX;
            theLastY = theEndY;
        }
        inFrom += theStep;
    }
    EESortUnique(outTiles, std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >());
}

// FUNCTION: EXE 0x64b556
// Collects the square of tiles within a radius of a tile
void EEGetTilesInRadius(long inX, long inY, unsigned long inRadius, bool inInclusive,
                        std::vector<EETileCoordinate>& outTiles)
{
    long theFar = inInclusive ? inRadius : inRadius - 1;
    if ((long)inRadius < 0)
        return;
    for (long theX = inX - inRadius; theX <= inX + theFar; theX++) {
        for (long theY = inY - inRadius; theY <= theFar + inY; theY++)
            outTiles.push_back(EETileCoordinate(theX, theY));
    }
}

// (the U2DPoint<long> twin of 0x64b556 is gone: one tile type)




















// FUNCTION: EXE 0x64b5b3
// Collects the tiles of a unit's footprint (a gate's pieces; a long wonder's full length)
void EEGetFootprintTiles(EEUnit* inUnit, std::vector<EETileCoordinate>& outTiles)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    unsigned long theY = inUnit->GetTileY();
    long theSize = theClass->mTileSize;
    unsigned long theX = inUnit->GetTileX();
    if (theClass->mCategory == kUnitCategoryGate) {
        EEComplexUnit* thePiece = inUnit->mComplexUnit;
        while (thePiece->mPreviousPart != NULL)
            thePiece = thePiece->mPreviousPart;
        for (; thePiece->mNextPart != NULL; thePiece = thePiece->mNextPart)
            outTiles.push_back(EETileCoordinate(thePiece->GetTileX(), thePiece->GetTileY()));
        outTiles.push_back(EETileCoordinate(thePiece->GetTileX(), thePiece->GetTileY()));
    } else {
        long theHeight = theSize;
        unsigned long theStartX = theX;
        unsigned long theStartY = theY;
        if (theClass->mUnitType == kUnitTypeAirport) {
            EEUCBuilding* theBuilding = dynamic_cast<EEUCBuilding*>(theClass);
            switch (EEGetFacingQuarter(theX, theY)) {
            case 0:
                theHeight = theSize * 4;
                break;
            case 1:
                theStartY = theY - theSize * 3;
                break;
            case 2:
                theSize = theSize * 4;
                break;
            default:
                theStartX = theX - theSize * 3;
                break;
            }
        }
        outTiles.clear();
        for (unsigned long theTileX = theStartX; theTileX < theX + theSize; theTileX++) {
            for (unsigned long theTileY = theStartY; theTileY < theHeight + theY; theTileY++)
                outTiles.push_back(EETileCoordinate(theTileX, theTileY));
        }
    }
}

// FUNCTION: EXE 0x64b6db
// Extends a wall line past its end while the tiles can take a wall piece
void EEExtendWall(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, EEPlayer* inPlayer, long inUnitType,
                std::vector<EETileCoordinate>& ioTiles)
{
    long theDX = inTo.mX - inFrom.mX;
    long theDY = inTo.mY - inFrom.mY;
    EETileCoordinate theTile(inTo.mX + theDX, inTo.mY + theDY);
    while (EEGetWallAt(theTile, inPlayer, inUnitType)) {
        ioTiles.insert(ioTiles.begin(), theTile);
        theTile.mX += theDX;
        theTile.mY += theDY;
    }
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
    outTiles.clear();
    if (inFrom.mX == inTo.mX && inFrom.mY == inTo.mY) {
        outTiles.push_back(inFrom);
    } else {
        EEGetTileLine(inFrom.mX, inFrom.mY, inTo.mX, inTo.mY, &outTiles, 1, 1);
        EEDropDiagonalCorners(outTiles);
        EEDropRepeatedTiles(outTiles);
    }
}

// FUNCTION: EXE 0x64b7e9
// Collects the units on a list of tiles (sorted and unique when a large one is among them)
long EEGetUnitsOnTiles(std::vector<EETileLocation>& inTiles, std::vector<EEUnit*>& outUnits)
{
    EEUnit* theUnit = NULL;
    bool theLarge = false;
    EEMap* theMap = ::gWorld.mMap;
    if (theMap != NULL) {
        for (long i = inTiles.size() - 1; i >= 0; i--) {
            EETile* theTile = theMap->GetTileAtLocation(inTiles[i]);
            if (theTile == NULL)
                continue;
            RTSTileUnitNode* theNode = theTile->mUnits;
            while (true) {
                if (theNode == NULL)
                    break;
                theUnit = theNode->mUnit;
                if (!theLarge && theUnit->mUnitClass->mTileSize <= 1)
                    theLarge = false;
                else
                    theLarge = true;
                outUnits.push_back(theUnit);
                theNode = theNode->mNext;
            }
        }
        if (theLarge)
            EESortUnique(outUnits, EEUUUnitIDGreater(), EEUUUnitIDSame());
    }
    return outUnits.size();
}

// FUNCTION: EXE 0x64b8ac
// Returns the path distance from a unit to a tile (10000 when unreachable) and the path's first tile
// The path distance from inUnit to ioTile (10000 when the path does not reach it; ioTile becomes the path's first
// tile, or (-1, -1)); inWalls lets walls not block (inferred)
float EEGetPathLengthTo(EEComplexUnit* inUnit, EETileCoordinate& ioTile, unsigned long inLimit, bool inWalls)
{
    std::vector<U2DPoint<long> > thePath;
    U2DPoint<long> theFrom(inUnit->GetTileX(), inUnit->GetTileY());
    U2DPoint<long> theTo(ioTile.GetX(), ioTile.GetY());
    EEAIAStarBounds theBounds;
    float theResult = kUUNoPath;
    EEAIBPGModifierWallsDoNotBlock* theModifier = NULL;
    long theWidth;
    long theHeight;
    long theMargin;
    if (inWalls)
        theModifier = new EEAIBPGModifierWallsDoNotBlock();
    theBounds.Set(theFrom);
    theBounds.Extend(theTo);
    theWidth = theBounds.mRight - theBounds.mLeft;
    theHeight = theBounds.mBottom - theBounds.mTop;
    theMargin = 1;
    if (max(theWidth, theHeight) / 2 > 1)
        theMargin = max(theWidth, theHeight) / 2;
    theBounds.mTop -= theMargin;
    if (theBounds.mTop <= 0)
        theBounds.mTop = 0;
    theBounds.mLeft -= theMargin;
    if (theBounds.mLeft <= 0)
        theBounds.mLeft = 0;
    theBounds.mBottom = min(theMargin + theBounds.mBottom, ::gWorld.GetMap()->mSize - 1);
    theBounds.mRight = min(theMargin + theBounds.mRight, ::gWorld.GetMap()->mSize - 1);
    if (inLimit > 0x32)
        inLimit = 0x32;
    gPathManager.FindPath(inUnit, ::gWorld.GetMap(), theFrom, theTo, theBounds, thePath, 0xb, inLimit, theModifier, NULL);
    if (thePath.size() == 0 || thePath[0].GetX() != theTo.GetX() || thePath[0].GetY() != theTo.GetY()) {
        ioTile.mX = -1;
        ioTile.mY = -1;
    } else {
        ioTile.mX = thePath[0].GetX();
        ioTile.mY = thePath[0].mY;
        theResult = (float)ioTile.DistanceTo(theFrom);
    }
    if (theModifier != NULL)
        theModifier->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x64ba4e
// Returns the last ring index around a town center
long EEGetTownCenterLastRing(long* outSize)
{
    *outSize = gUnitClassTable[kUnitTypeGranary]->mTileSize;
    return max(*outSize, 3) * max(*outSize, 3) - 1;
}

// FUNCTION: EXE 0x64ba75
// Removes the tiles that are off the map or on the map's edge
void EERemoveEdgeTiles(std::vector<EETileLocation>& ioTiles)
{
    EEMap* theMap = ::gWorld.mMap;
    for (unsigned long i = 0; i < ioTiles.size(); i++) {
        if (theMap->GetTileAtLocation(ioTiles[i]) == NULL || theMap->IsEdgeTile(ioTiles[i])) {
            ioTiles.erase(ioTiles.begin() + i);
            i--;
        }
    }
}

// FUNCTION: EXE 0x64baf6
// Tells whether a tile lies on the diamond map's edge
bool RTSMap::IsEdgeTile(const EETileLocation& inTile)
{
    long theY = inTile.mY;
    long theX = inTile.mX;
    return theX + theY == this->mL195608 || theY - theX == this->mL195610 || theX + theY == this->mL19560C || theX - theY == this->mL195610;
}

// FUNCTION: EXE 0x64bb36
// Collects the ring of tiles at an index around a square, sorted
void EEGetRingTiles(long inX, long inY, long inSize, long inRing, long inCorners,
                std::vector<U2DPoint<long> >& outTiles)
{
    std::vector<U2DPoint<long> > theSides;
    std::vector<U2DPoint<long> > theCorners;
    std::vector<U2DPoint<long> > theTiles;
    outTiles.clear();
    if (inRing > 0) {
        long theSize = inSize + inRing * 2 - 2;
        EEGetSideTiles(inX - inRing + 1, inY - inRing + 1, theSize, theSides);
        EEGetCornerTiles(inCorners, inX - inRing + 1, inY - inRing + 1, theSize, theCorners);
        theTiles.insert(theTiles.end(), theSides.begin(), theSides.end());
        theTiles.insert(theTiles.end(), theCorners.begin(), theCorners.end());
        if (theTiles.size() == 12) {
            outTiles.push_back(theTiles[4]);
            outTiles.push_back(theTiles[6]);
            outTiles.push_back(theTiles[10]);
            outTiles.push_back(theTiles[1]);
            outTiles.push_back(theTiles[3]);
            outTiles.push_back(theTiles[11]);
            outTiles.push_back(theTiles[7]);
            outTiles.push_back(theTiles[5]);
            outTiles.push_back(theTiles[9]);
            outTiles.push_back(theTiles[2]);
            outTiles.push_back(theTiles[0]);
            outTiles.push_back(theTiles[8]);
        } else if (theTiles.size() == 8) {
            U2DPoint<long> theCenter;
            theCenter.mX = inX;
            theCenter.mY = inY;
            theTiles.push_back(theCenter);
            outTiles.push_back(theTiles[8]);
            outTiles.push_back(theTiles[2]);
            outTiles.push_back(theTiles[6]);
            outTiles.push_back(theTiles[1]);
            outTiles.push_back(theTiles[7]);
            outTiles.push_back(theTiles[3]);
            outTiles.push_back(theTiles[5]);
            outTiles.push_back(theTiles[0]);
            outTiles.push_back(theTiles[4]);
        } else {
            outTiles = theTiles;
        }
    } else {
        outTiles.push_back(U2DPoint<long>(inX, inY));
    }
    EERemoveEdgeTiles(outTiles);
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

// FUNCTION: EXE 0x64c003
// Collects the units on the tiles within a radius whose position also lies within it; their number
long EEGetUnitsNearTile(const GE3DPoint& inCenter, float inRadius, std::vector<EEUnit*>& outUnits)
{
    GE2DPoint theCenter(inCenter.GetX(), inCenter.GetY());
    GE2DPoint thePosition;
    std::vector<EETileCoordinate> theTiles;
    float theRadiusSquared = inRadius * inRadius;
    outUnits.clear();
    EEGetTilesNearPoint(inCenter, inRadius, theTiles);
    EEGetUnitsOnTiles(theTiles, outUnits);
    std::vector<EEUnit*>::iterator it = outUnits.begin();
    while (it != outUnits.end()) {
        EEUnit* theUnit = *it;
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit == NULL) {
            thePosition.SetX((float)theUnit->GetTileX() + 0.5f);
            thePosition.SetY((float)theUnit->GetTileY() + 0.5f);
        } else {
            thePosition.SetX(theComplexUnit->mPhysicalModel.GetLocation().GetX());
            thePosition.SetY(theComplexUnit->mPhysicalModel.GetLocation().GetY());
        }
        if (thePosition.DistanceToSquared(theCenter) <= theRadiusSquared)
            it++;
        else
            it = outUnits.erase(it);
    }
    return outUnits.size();
}

// FUNCTION: EXE 0x64c0ed
// Collects the units under a fan of rays; their number
long EEGetUnitsInFan(const GE2DPoint& inCenter, float inRadius, float inFrom, float inTo, long inSteps,
                std::vector<EEUnit*>& outUnits)
{
    std::vector<U2DPoint<long> > theTiles;
    EEGetFanTiles(inCenter, inRadius, inFrom, inTo, inSteps, theTiles);
    return EEGetUnitsOnTiles(theTiles, outUnits);
}

// FUNCTION: EXE 0x64c146
// Collects the units on the tiles of a screen rectangle; their number
long EEGetUnitsInRectangle(const GERectangle& inRectangle, std::vector<EEUnit*>& outUnits)
{
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
    return EEGetUnitsOnTiles(theTiles, outUnits);
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
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = NULL;
    bool theFound = false;
    if (theMap != NULL) {
        EETile* theTile = theMap->RTSMap::GetTile(inTile.mX, inTile.mY);
        if (theTile != NULL) {
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
                theUnit = theNode->mUnit;
                if (theUnit == NULL || (theUnit->mHitPoints <= 0 && !theUnit->mUnitClass->mIsTree)
                    || theUnit->mUnitClass->mUnitType != inUnitType)
                    theFound = false;
                else
                    theFound = true;
            }
        }
    }
    return theFound ? theUnit : NULL;
}

// FUNCTION: EXE 0x64c395
// Collects a player's units of a type; their number
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayer, std::vector<EEUnit*>& outUnits)
{
    EEUnit* theUnit = NULL;
    long theCount = 0;
    for (theUnit = ::gWorld.mPlayers[inPlayer]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->mUnitType == inUnitType) {
            outUnits.push_back(theUnit);
            theCount++;
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x64c3df
// Collects the units on a tile; their number
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits)
{
    std::vector<U2DPoint<long> > theTiles;
    outUnits.clear();
    theTiles.push_back(U2DPoint<long>(inX, inY));
    return EEGetUnitsOn(theTiles, outUnits);
}

// FUNCTION: EXE 0x64c433
// Collects the units on a list of tiles
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits)
{
    EEMap* theMap = ::gWorld.mMap;
    long theCount = 0;
    if (theMap != NULL) {
        for (unsigned long i = 0; i < inTiles.size(); i++) {
            EETile* theTile = theMap->GetTile(inTiles[i].mX, inTiles[i].mY);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit != NULL) {
                    outUnits.push_back(theUnit);
                    theCount++;
                }
            }
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x64c4be
// Returns a player's living unit of a type on a tile, or NULL
EEUnit* EEGetWallAt(const EETileCoordinate& inTile, EEPlayer* inPlayer, long inUnitType)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = NULL;
    bool theFound = false;
    if (theMap != NULL) {
        EETile* theTile = theMap->RTSMap::GetTile(inTile.mX, inTile.mY);
        if (theTile != NULL) {
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
                theUnit = theNode->mUnit;
                if (theUnit == NULL || (theUnit->mHitPoints <= 0 && !theUnit->mUnitClass->mIsTree)
                    || theUnit->mPlayer != inPlayer || theUnit->mUnitClass->mUnitType != inUnitType)
                    theFound = false;
                else
                    theFound = true;
            }
        }
    }
    return theFound ? theUnit : NULL;
}

// FUNCTION: EXE 0x64c54a
// Collects a player's units of a type on a list of tiles; their number
long EEGetUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer, long inUnitType,
                std::vector<EEUnit*>& outUnits)
{
    EEMap* theMap = ::gWorld.mMap;
    long theCount = 0;
    long theX;
    long theY;
    EEUnit* theUnit;
    if (theMap != NULL) {
        for (unsigned long i = 0; i < inTiles.size(); i++) {
            theX = inTiles[i].GetX();
            theY = inTiles[i].GetY();
            EETile* theTile = theMap->GetTile(theX, theY);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->GetUnits(); theNode != NULL; theNode = theNode->GetNext()) {
                theUnit = theNode->GetUnit();
                if (theUnit != NULL && theUnit->GetUnitClass()->GetUnitType() == inUnitType && theUnit->GetPlayer() == inPlayer) {
                    outUnits.push_back(theUnit);
                    theCount++;
                }
            }
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x64c627
// Collects the units of a type on a list of tiles; their number
long EEGetWallUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    EEMap* theMap = ::gWorld.mMap;
    long theCount = 0;
    long theX;
    long theY;
    EEUnit* theUnit;
    if (theMap != NULL) {
        for (unsigned long i = 0; i < inTiles.size(); i++) {
            theX = inTiles[i].GetX();
            theY = inTiles[i].GetY();
            EETile* theTile = theMap->GetTile(theX, theY);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->GetUnits(); theNode != NULL; theNode = theNode->GetNext()) {
                theUnit = theNode->GetUnit();
                if (theUnit != NULL && theUnit->GetUnitClass()->GetUnitType() == inUnitType) {

                    outUnits.push_back(theUnit);
                    theCount++;
                }
            }
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x64c6fc
// Collects the free tiles around a unit (its ring, corners included)
void EEGetResourceSpots(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theCorners;
    EEMap* theMap = ::gWorld.mMap;
    if (theMap == NULL)
        return;
    EEGetSideTiles(inUnit->GetTileX() - 1, inUnit->GetTileY() - 1, inUnit->mUnitClass->mTileSize + 2, theTiles);
    EEGetCornerTiles(0, inUnit->GetTileX() - 1, inUnit->GetTileY() - 1, inUnit->mUnitClass->mTileSize + 2, theCorners);
    theTiles.insert(theTiles.begin(), theCorners.begin(), theCorners.end());
    for (unsigned long i = 0; i < theTiles.size(); i++) {
        EETile* theTile = theMap->RTSMap::GetTile(theTiles[i].mX, theTiles[i].mY);
        if (theTile == NULL)
            continue;
        RTSTileUnitNode* theNode = theTile->mUnits;
        if (theNode != NULL) {
            bool theFree = true;
            do {
                if (!theFree)
                    break;
                EEUnit* theUnit = theNode->mUnit;
                if (theUnit != NULL && theUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen && theUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen
                    && !(theUnit->IsPassableBy(inUnit->GetComplexUnit()) && !theUnit->IsFoundation()))
                    theFree = false;
                theNode = theNode->mNext;
            } while (theNode != NULL);
            if (theFree)
                outTiles.push_back(theTiles[i]);
        } else {
            outTiles.push_back(theTiles[i]);
        }
    }
}


// FUNCTION: EXE 0x64c8bc
// Enters a fortress into the unit regions under its footprint, clearing ambient units outside a load
// inUnit (a fortress) entered in the unit regions under its footprint; outside a load, the ambient units on its
// tiles (and its carriers') are cleared (inferred)
void EEAddBuildingRegion(EEUnit* inUnit)
{
    U2DRectangle<long> theTiles;
    U2DRectangle<long> theRegions;
    RTSRegionTable<EEComplexUnit*>* theTable = ::gWorld.mMap->mUnitRegions;
    long theSize = inUnit->GetUnitClass()->mTileSize;
    if (inUnit->mTileNode == NULL)
        return;
    long theX = inUnit->GetTileX();
    long theY = inUnit->GetTileY();
    theTiles.mLeft = theX;
    theTiles.mTop = theY;
    theTiles.mBottom = theY + theSize - 1;
    theTiles.mRight = theX + theSize - 1;
    theRegions = theTable->GetRegions(theTiles);
    if (theRegions.GetTopLeft() == theRegions.GetBottomRight()) {
        EEComplexUnit* theUnit = inUnit->GetComplexUnit();
        std::vector<EEComplexUnit*>* theRegion = theTable->GetRegion(theRegions.GetTopLeft());
        if (theRegion != NULL)
            theRegion->push_back(theUnit);
    } else {
        for (long x = theRegions.GetTopLeft().mX; x <= theRegions.GetBottomRight().mX; x++) {
            for (long y = theRegions.GetTopLeft().mY; y <= theRegions.GetBottomRight().mY; y++) {
                theTable->Add(x, y, inUnit->GetComplexUnit());
            }
        }
    }
    if (gLoadingSavedGame)
        return;
    if ((inUnit->GetUnitClass()->mCategory == kUnitCategoryNavalBuilding || inUnit->GetUnitClass()->mCategory == kUnitCategorySpaceBuilding) && inUnit->GetUnitClass()->mAirTrafficType != 0)
        return;
    while (inUnit != NULL) {
        EEClearUnderBuilding(inUnit->GetTileX(), inUnit->GetTileY(), inUnit->GetUnitClass()->mTileSize);
        inUnit = inUnit->mTileNode != NULL ? inUnit->GetComplexUnit()->mNextPart : NULL;
    }
}

// FUNCTION: EXE 0x64caef
// Adds a unit to every tile of its footprint
void EEAddToFootprintTiles(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    long theSize = inUnit->mUnitClass->mTileSize;
    unsigned long theX = inUnit->GetTileX();
    unsigned long theY = inUnit->GetTileY();
    long theEndX = theX + theSize;
    long theEndY = theSize + theY;
    EETile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
    for (long theTileX = theX; theTileX < theEndX; theTileX++) {
        for (long theTileY = theY; theTileY < theEndY; theTileY++) {
            if (theTileX == theX && theTileY == theY)
                continue;
            theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL)
                theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
        }
    }
}
// FUNCTION: EXE 0x64cbd7
// Returns the tile two tiles away in a direction (1..8)
void EEGetSlotTile(const EETileCoordinate& inTile, long inDirection, EETileCoordinate& outTile)
{
    long theSize = 0;
    long theDY = 0;
    long theDX = 0;
    EEGetTownCenterLastRing(&theSize);
    switch (inDirection) {
    case 1:
        theDX = -2;
        theDY = 2;
        break;
    case 2:
        theDX = 0;
        theDY = 2;
        break;
    case 3:
        theDX = theDY = 2;
        break;
    case 4:
        theDX = -2;
        break;
    case 5:
        theDX = 2;
        break;
    case 6:
        theDX = theDY = -2;
        break;
    case 7:
        theDX = 0;
        theDY = -2;
        break;
    case 8:
        theDX = 2;
        theDY = -2;
        break;
    }
    outTile.mX = inTile.mX + theDX;
    outTile.mY = inTile.mY + theDY;
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
    float thePrevHeight = 0.0f;
    long thePrevRaised = 0;
    unsigned long theCount = inTiles.size();
    long theRun = 0;
    long thePrevX = 0;
    long thePrevY = 0;
    unsigned long i;
    long theX;
    long theY;
    long theDX;
    long theXUp;
    long theDY;
    long theYUp;
    EETile* theTile;
    long theRaised;
    float theHeight;
    outCorners.resize(theCount, 0);
    for (unsigned long j = 0; j < theCount; j++)
        outCorners[j] = 0;
    for (i = 1; i < theCount; i++) {
        theX = inTiles[i].mX;
        theY = inTiles[i].mY;
        theDX = inTiles[i - 1].mX - theX;
        theXUp = theDX >= 0;
        if (!theXUp)
            theDX = -theDX;
        theDY = inTiles[i - 1].mY - theY;
        theYUp = theDY >= 0;
        if (!theYUp)
            theDY = -theDY;
        theTile = inMap->GetTile(theX, theY);
        theRaised = inMap->GetNumRaisedCorners(theX, theY);
        theHeight = theTile->mHeight;
        GETerrainMesh& theMesh = inMap->mTerrainMesh;
        if (theDX == 0 && theDY == 1) {
            if (theRun == 2) {
                if (thePrevHeight - theHeight >= 0.4f) {
                    if (thePrevRaised == 2)
                        outCorners[i - 1] = theYUp ? 5 : 7;
                    else
                        outCorners[i - 1] = 0;
                } else if (theHeight - thePrevHeight >= 0.4f) {
                    if (thePrevRaised == 2)
                        outCorners[i - 1] = theYUp ? 7 : 5;
                    else
                        outCorners[i - 1] = 0;
                } else if (thePrevRaised == 2) {
                    if (UMAbs(theMesh.GetZ(thePrevX, thePrevY) - theMesh.GetZ(theX, theY)) < 0.4f
                        && UMAbs(theMesh.GetZ(thePrevX + 1, thePrevY) - theMesh.GetZ(theX + 1, theY)) < 0.4f
                        && UMAbs(theMesh.GetZ(thePrevX, thePrevY + 1) - theMesh.GetZ(theX, theY + 1)) < 0.4f
                        && UMAbs(theMesh.GetZ(thePrevX + 1, thePrevY + 1) - theMesh.GetZ(theX + 1, theY + 1)) < 0.4f)
                        outCorners[i - 1] = 2;
                    else if (theRaised != 1
                             || (theMesh.GetZ(thePrevX, thePrevY) <= theMesh.GetZ(thePrevX + 1, thePrevY)
                                 && theMesh.GetZ(thePrevX + 1, thePrevY) <= theMesh.GetZ(thePrevX, thePrevY))) {
                        if (theMesh.GetZ(thePrevX + 1, thePrevY + 1) < theMesh.GetZ(thePrevX, thePrevY))
                            outCorners[i - 1] = 7;
                        else
                            outCorners[i - 1] = 5;
                    } else {
                        outCorners[i - 1] = 2;
                    }
                } else if (thePrevRaised == 1) {
                    outCorners[i - 1] = 0;
                } else {
                    outCorners[i - 1] = 2;
                }
            } else {
                theRun = 2;
            }
        } else if (theDY == 0) {
            if (theDX == 1) {
                if (theRun == 4) {
                    if (thePrevHeight - theHeight >= 0.4f) {
                        if (thePrevRaised == 2)
                            outCorners[i - 1] = theXUp ? 8 : 6;
                        else
                            outCorners[i - 1] = 0;
                    } else if (theHeight - thePrevHeight >= 0.4f) {
                        if (thePrevRaised == 2)
                            outCorners[i - 1] = theXUp ? 6 : 8;
                        else
                            outCorners[i - 1] = 0;
                    } else if (thePrevRaised == 2) {
                        if (UMAbs(theMesh.GetZ(thePrevX, thePrevY) - theMesh.GetZ(theX, theY)) < 0.4f
                            && UMAbs(theMesh.GetZ(thePrevX + 1, thePrevY) - theMesh.GetZ(theX + 1, theY)) < 0.4f
                            && UMAbs(theMesh.GetZ(thePrevX, thePrevY + 1) - theMesh.GetZ(theX, theY + 1)) < 0.4f
                            && UMAbs(theMesh.GetZ(thePrevX + 1, thePrevY + 1) - theMesh.GetZ(theX + 1, theY + 1))
                                   < 0.4f)
                            outCorners[i - 1] = 1;
                        else if (theRaised != 1
                                 || (theMesh.GetZ(thePrevX, thePrevY) <= theMesh.GetZ(thePrevX, thePrevY + 1)
                                     && theMesh.GetZ(thePrevX, thePrevY + 1) <= theMesh.GetZ(thePrevX, thePrevY))) {
                            if (theMesh.GetZ(thePrevX + 1, thePrevY + 1) < theMesh.GetZ(thePrevX, thePrevY))
                                outCorners[i - 1] = 6;
                            else
                                outCorners[i - 1] = 8;
                        } else {
                            outCorners[i - 1] = 1;
                        }
                    } else if (thePrevRaised == 1) {
                        outCorners[i - 1] = 0;
                    } else {
                        outCorners[i - 1] = 1;
                    }
                }
                theRun = 4;
            }
        } else if (theDX == 1 && theDY == 1) {
            if (theYUp == theXUp) {
                if (theRun == 3) {
                    if (UMAbs(thePrevHeight - theHeight) < 0.4f) {
                        if (thePrevRaised == 2) {
                            if (theMesh.GetZ(thePrevX + 1, thePrevY + 1) < theMesh.GetZ(thePrevX, thePrevY))
                                outCorners[i - 1] = 12;
                            else
                                outCorners[i - 1] = 10;
                        } else {
                            outCorners[i - 1] = 4;
                        }
                    } else if (thePrevRaised != 2) {
                        outCorners[i - 1] = 0;
                    } else if (theMesh.GetZ(thePrevX, thePrevY) <= theMesh.GetZ(thePrevX + 1, thePrevY + 1)) {
                        outCorners[i - 1] = 10;
                    } else {
                        outCorners[i - 1] = 12;
                    }
                }
                theRun = 3;
            } else if (theRun == 1) {
                if (UMAbs(thePrevHeight - theHeight) < 0.4f) {
                    if (thePrevRaised == 2) {
                        if (theMesh.GetZ(thePrevX + 1, thePrevY + 1) < theMesh.GetZ(thePrevX, thePrevY))
                            outCorners[i - 1] = 9;
                        else
                            outCorners[i - 1] = 11;
                    } else {
                        outCorners[(i - 1)] = 3;
                    }
                } else if (thePrevRaised != 2) {
                    outCorners[(i - 1)] = 0;
                } else if (theMesh.GetZ(thePrevX, thePrevY) <= theMesh.GetZ(thePrevX + 1, thePrevY + 1)) {
                    outCorners[(i - 1)] = 11;
                } else {
                    outCorners[(i - 1)] = 9;
                }
            } else {
                theRun = 1;
            }
        }
        thePrevRaised = theRaised;
        thePrevHeight = theHeight;
        thePrevY = theY;
        thePrevX = theX;
    }
}

// FUNCTION: EXE 0x64d779
// Takes a fortress out of the unit regions under its footprint
// NEAR: the original removes through the vector remove helper 0x239ee2 (find + erase; see EEUnitClass::RemoveFromRegion)
// inUnit (a fortress) taken out of the unit regions under its footprint (inferred)
void EERemoveBuildingRegion(EEUnit* inUnit)
{
    RTSRegionTable<EEComplexUnit*>* theTable = ::gWorld.mMap->mUnitRegions;
    U2DRectangle<long> theTiles;
    U2DRectangle<long> theRegions;
    long theSize = inUnit->GetUnitClass()->mTileSize;
    if (inUnit->mTileNode == NULL)
        return;
    theTiles.mLeft = inUnit->GetTileX();
    theTiles.mRight = inUnit->GetTileX() + theSize - 1;
    theTiles.mTop = inUnit->GetTileY();
    theTiles.mBottom = inUnit->GetTileY() + theSize - 1;
    theRegions = theTable->GetRegions(theTiles);
    if (theRegions.GetTopLeft() == theRegions.GetBottomRight()) {
        EEComplexUnit* theUnit = inUnit->GetComplexUnit();
        RTSRegion<EEComplexUnit*>* theRegion = theTable->GetRegion(theRegions.GetTopLeft());
        if (theRegion != NULL)
            theRegion->Remove(theUnit);
    } else {
        for (long x = theRegions.GetTopLeft().mX;; x++) {
            if (!(x <= theRegions.GetBottomRight().mX))
                break;
            for (long y = theRegions.GetTopLeft().mY; y <= theRegions.GetBottomRight().mY; y++) {
                EEComplexUnit* theUnit = inUnit->GetComplexUnit();
                RTSRegion<EEComplexUnit*>* theRegion = theTable->GetRegion(x, y);
                if (theRegion != NULL)
                    theRegion->Remove(theUnit);
            }
        }
    }
}

// FUNCTION: EXE 0x64d946
// Removes a unit from every tile of its footprint
void EERemoveFromFootprintTiles(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    long theSize = inUnit->mUnitClass->mTileSize;
    unsigned long theX = inUnit->GetTileX();
    unsigned long theY = inUnit->GetTileY();
    long theEndX = theX + theSize;
    long theEndY = theSize + theY;
    EETile* theTile = theMap->GetTile(theX, theY);
    if (theTile != NULL)
        theTile->RemoveUnit(inUnit);
    for (long theTileX = theX; theTileX < theEndX; theTileX++) {
        for (long theTileY = theY; theTileY < theEndY; theTileY++) {
            if (theTileX == theX && theTileY == theY)
                continue;
            theTile = theMap->GetTile(theTileX, theTileY);
            if (theTile != NULL)
                theTile->RemoveUnit(inUnit);
        }
    }
}

// FUNCTION: EXE 0x64da0c
// Drops repeated tiles from a list
void EEDropRepeatedTiles(std::vector<EETileCoordinate>& ioTiles)
{
    long thePrevX = 1000;
    long thePrevY = 1000;
    for (unsigned long i = 0; i < ioTiles.size(); i++) {
        long theX = ioTiles[i].mX;
        long theY = ioTiles[i].mY;
        if (thePrevX == theX && thePrevY == theY) {
            ioTiles.erase(ioTiles.begin() + i);
            i--;
        } else {
            thePrevX = theX;
            thePrevY = theY;
        }
    }
}

// FUNCTION: EXE 0x64da82
// Turns a compass direction by a number of steps, either way
long EETurnDirection(long inDirection, long inSteps, bool inClockwise)
{
    if (inDirection < 0 || inDirection > 7)
        return inDirection;
    for (long i = inSteps - 1; i >= 0; i--) {
        switch (inDirection) {
        case 0:
            inDirection = inClockwise ? 4 : 1;
            break;
        case 1:
            inDirection = inClockwise ? 0 : 2;
            break;
        case 2:
            inDirection = inClockwise ? 1 : 3;
            break;
        case 3:
            inDirection = inClockwise ? 2 : 7;
            break;
        case 4:
            inDirection = inClockwise ? 5 : 0;
            break;
        case 5:
            inDirection = inClockwise ? 6 : 4;
            break;
        case 6:
            inDirection = inClockwise ? 7 : 5;
            break;
        case 7:
            inDirection = inClockwise ? 3 : 6;
            break;
        }
    }
    return inDirection;
}

// FUNCTION: EXE 0x64db3f
// Turns an angle by a random share of a full turn, kept in (-pi, pi]
float EEJitterHeading(float inAngle, float inMin, float inMax)
{
    float theTurn = inMin == inMax ? inMin : gPhysics->GetRandom(inMin, inMax);
    float theAngle = 6.2831855f * (theTurn * 0.01f) + inAngle;
    if (theAngle > 6.2831855f) {
        float theTurns = theAngle * 0.15915494f;
        theAngle = ((float)UMCeil(theTurns) - theTurns) * 6.2831855f;
    }
    if (theAngle > 3.1415927f)
        theAngle -= 6.2831855f;
    else if (theAngle < -3.1415927f)
        theAngle += 6.2831855f;
    return theAngle;
}


// FUNCTION: EXE 0x64dbd2
// Tells whether a wall piece on a tile rises above a height
// Whether a wall piece on tile (inX, inY) rises above inHeight (its height scaled by the unit's size and health)
// (the original calls RTSMap::GetTile out of line: inline budget)
bool EEIsTileTopAbove(unsigned long inX, unsigned long inY, float inHeight, EEUnit** outArg4, bool inBare)
{
    bool theResult = false;
    GE3DPoint thePoint;
    EEUnit* theWall = NULL;
    float theTop;
    EETile* theTile = ::gWorld.GetMap()->GetTile(inX, inY);
    if (theTile != NULL) {
        theTop = theTile->GetHeight();
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && theWall == NULL; theNode = theNode->mNext) {
            theWall = theNode->mUnit;
            if (!theWall->mUnitClass->mIsWall || theWall->IsPassableBy(NULL)) {
                theWall = NULL;
            } else if (!inBare && theWall->mTileNode != NULL) {
                if (theWall->mUnitClass->mCategory != kUnitCategoryGate)
                    theWall->mComplexUnit->mPhysicalModel.GetLocation().Apply(
                        theWall->GetModel()->mModels[theWall->mComplexUnit->GetAnimation()]->GetBoundingBox(
                            theWall->mComplexUnit->mPhysicalModel.GetFrame()).GetTopRightBack(), thePoint);
                else
                    thePoint.SetZ(1.2f);
                float theRise = thePoint.GetZ() * theWall->mStats->GetScale();
                if (!theWall->mComplexUnit->mIsBuilt)
                    theRise = ((float)theWall->mComplexUnit->mHitPoints
                               / (float)theWall->mComplexUnit->mStats->mMaxHitPoints) * theRise;
                theTop = theRise + theTop;
            } else {
                theTop = theTop + 1.0f;
            }
        }
        if (!inBare || theWall != NULL) {
            if (theTop > inHeight)
                theResult = true;
            else
                theResult = false;
        }
    }
    *outArg4 = theWall;
    return theResult;
}

// FUNCTION: EXE 0x64dd2e
// Tells whether a wall blocks a tile line (diagonal steps between two pieces count); reports the last free tile
// Whether a wall blocks the line from (inX1, inY1) to (inX2, inY2) (diagonal steps between two pieces count);
// outX/outY the last free tile
bool EEIsLineBlocked(long inX1, long inY1, long inX2, long inY2, EEUnit** outArg5, long* outX, long* outY)
{
    std::vector<EETileCoordinate> theLine;
    EETile* theFrom = ::gWorld.GetMap()->GetTile(inX1, inY1);
    EETile* theTo = ::gWorld.GetMap()->GetTile(inX2, inY2);
    long theLastY = 0;
    bool theBlocked = false;
    unsigned long theLastX = 0;
    if (theFrom != NULL && theTo != NULL) {
        EEGetTileLine(inX1, inY1, inX2, inY2, &theLine, 0, 1);
        theLastY = inY1;
        float theHeight = theFrom->GetHeight();
        for (unsigned long i = 0; theLastX = inX1, theLine.size() > 0 && i < theLine.size(); i++) {
            if (theBlocked)
                break;
            long theX = theLine[i].mX;
            long theY = theLine[i].mY;
            long theDX = theX - inX1;
            long theDY = theY - theLastY;
            if (UMAbs(theDX) == 1 && UMAbs(theDY) == 1 && EEIsTileTopAbove(theX, theLastY, theHeight, outArg5, true)
                && EEIsTileTopAbove(inX1, theY, theHeight, outArg5, true)) {
                theBlocked = true;
            } else {
                theBlocked = EEIsTileTopAbove(theX, theY, theHeight, outArg5, true);
                theLastY = theY;
            }
            inX1 = theX;
        }
        if (!theBlocked && UMAbs(inX2 - inX1) == 1 && UMAbs(inY2 - theLastY) == 1
            && EEIsTileTopAbove(inX2, theLastY, theHeight, outArg5, true) && EEIsTileTopAbove(inX1, inY2, theHeight, outArg5, true)) {
            theBlocked = true;
            theLastX = inX2;
        }
    }
    *outX = theLastX;
    *outY = theLastY;
    return theBlocked;
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
    long theSet;
    switch (inCorner) {
    case 1:
        theSet = 9;
        ioTransformation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f);
        break;
    case 2:
        theSet = 9;
        break;
    case 3:
        theSet = 10;
        ioTransformation.SetOrientationYPR(0.78539819f, 0.0f, 0.0f);
        break;
    case 4:
        theSet = 10;
        ioTransformation.SetOrientationYPR(2.3561945f, 0.0f, 0.0f);
        break;
    case 5:
        theSet = 6;
        break;
    case 6:
        theSet = 6;
        ioTransformation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f);
        break;
    case 7:
        theSet = 6;
        ioTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
        break;
    case 8:
        theSet = 6;
        ioTransformation.SetOrientationYPR(kUUWallYaw8, 0.0f, 0.0f);
        break;
    case 9:
        theSet = 3;
        ioTransformation.SetOrientationYPR(kUUWallYaw9, 0.0f, 0.0f);
        break;
    case 10:
        theSet = 3;
        ioTransformation.SetOrientationYPR(kUUWallYaw10, 0.0f, 0.0f);
        break;
    case 11:
        theSet = 3;
        ioTransformation.SetOrientationYPR(0.78539819f, 0.0f, 0.0f);
        break;
    case 12:
        theSet = 3;
        ioTransformation.SetOrientationYPR(2.3561945f, 0.0f, 0.0f);
        break;
    case 0:
        theSet = 11;
        break;
    default:
        theSet = 11;
        break;
    }
    return theSet;
}

// FUNCTION: EXE 0x64e518
// Moves a unit to a transformation, snapping to the closest tile when off the map
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY)
{
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
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    long theFrameSet = EEOrientWallPiece(theTransformation, inCorner, inRaisedCorners);
    inUnit->SetAnimation(theFrameSet, -1, 0);
    inUnit->SetTransformation(theTransformation, true);
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
    std::vector<EETileCoordinate> theTiles;
    unsigned long i;
    if (inUnit != NULL) {
        EEGetFootprintTiles(inUnit, theTiles);
    } else {
        long theSize = inClass->mTileSize;
        float theHalf = (float)theSize * 0.5f;
        long theLeft = inTile.GetX() - UMFloorUnsigned(theHalf);
        long theTop = inTile.GetY() - UMFloorUnsigned(theHalf);
        for (long theX = theLeft; theX < theLeft + theSize; theX++) {
            for (long theY = theTop; theY < theTop + theSize; theY++)
                theTiles.push_back(EETileCoordinate(theX, theY));
        }
    }
    for (i = 0; theTiles.size() > 0 && i < theTiles.size(); i++) {
        EETile* theMapTile = ::gWorld.GetMap()->RTSMap::GetTile(theTiles[i].GetX(), theTiles[i].GetY());
        if (theMapTile == NULL)   // port: a footprint tile off the map (a site at the map's edge): the fault of Small Islands seed 11
            continue;
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
            EEUnit* theOther = theNode->mUnit;
            EEComplexUnit* theComplexUnit;
            if (theOther == NULL || (theComplexUnit = theOther->GetComplexUnit()) == NULL || theComplexUnit->IsFoundation())
                continue;
            // port: a unit removed while its tile node lingers (PORT_LEARNINGS: the x-sorted list and the tile lists
            // keep a removed unit's node until it is freed), or one without stats or a tile (a path point, a unit
            // not placed yet): nothing to step aside. A builder beginning on a granary beside removed path points
            // faulted here (EEUGBuildRepair::BeginRepair, Small Islands seed 11)
            if (!theOther->mIsValid || theOther->mTile == NULL || theComplexUnit->GetStats() == NULL || theComplexUnit->GetClass() == NULL)
                continue;
            if ((theComplexUnit->GetClass()->GetFamily() != kUnitFamilyAnimal || theComplexUnit->GetStats()->GetAttack() != 0.0f) && !inForce) {
                if (theOther->GetPlayer() != inPlayer && (theOther->mPlayer == NULL || inPlayer == NULL
                                                          || inPlayer->mEnemies[theOther->mPlayer->GetPlayerID()] != 0))
                    continue;
                long theCommand = theComplexUnit->mCommand != NULL ? theComplexUnit->mCommand->mType : 0;
                if (theCommand != kActionIdle && theCommand != kActionWaitForOpenPosition && theCommand != kActionGatherResource)
                    continue;
            }
            if (!theComplexUnit->GetClass()->mCanFly
                || theComplexUnit->mPhysicalModel.GetLocation().GetZ() <= theComplexUnit->mTile->GetHeight())
                theComplexUnit->StepAside(inPlayer, inClass, theTiles[i].GetX(),
                                         theTiles[i].GetY(), inForce);
        }
    }
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
    EEUnitStatsScope theStatsScope(inUpgrade);   // New (R5b.4, the fourteenth round): the stats table (ee/EEUnitStore.h)
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
    std::vector<EEUnit*> theUnits;
    EEMap* theMap = ::gWorld.mMap;
    EETile* theTile = theMap->GetTile(inX, inY);
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    bool theResult = theTile != NULL;
    if (theTile == NULL)
        return theResult;
    unsigned short theContinent = gContinentManager.GetContinentID(inX, inY);
    if (!theClass->mTerrainTypes[kTerrainDeepWater] && !theClass->mTerrainTypes[kTerrainSpace]) {
        if (inUnit != NULL) {
            unsigned short theID = gContinentManager.GetContinentID(inX, inY);
            EEAIContinent* theTo = theID ? gContinentManager.GetContinentByID(theID) : NULL;
            theID = gContinentManager.GetContinentID(inUnit->GetTileX(), inUnit->GetTileY());
            EEAIContinent* theFrom = theID ? gContinentManager.GetContinentByID(theID) : NULL;
            theResult = gContinentManager.AreConnectedForUnit(inUnit, theFrom, theTo);
        } else if (!inSkipPlayerChecks && inPlayer != NULL && inPlayer->mActiveGroup->GetNumUnits() != 0) {
            unsigned short theID = inPlayer->mActiveGroup->GetContinent();
            EEAIContinent* theOther = gContinentManager.GetContinentByID(theID);
            if (theOther != NULL && ((theOther->mCategory != kContinentCategoryWater && !theClass->mTerrainTypes[kTerrainDeepWater])
                                     || (theOther->mCategory != kContinentCategorySpace && !theClass->mTerrainTypes[kTerrainSpace])))
                theResult = gContinentManager.AreConnectedByID(theID, theContinent, inPlayer->GetCitizenPassability());
        }
    }
    if (theResult && !inSkipPlayerChecks && inPlayer != NULL && inPlayer->mSaveVisMap)
        theResult = theTile->IsVisibleTo(inPlayer);
    if (theResult) {
        if (theMap->RTSMap::GetTile(inX + 1, inY) == NULL || theMap->RTSMap::GetTile(inX - 1, inY) == NULL
            || theMap->RTSMap::GetTile(inX, inY - 1) == NULL || theMap->RTSMap::GetTile(inX, inY + 1) == NULL) {
            theResult = false;
        } else {
            theResult = theClass->mTerrainTypes[theTile->mTerrainType];
            if (theResult) {
                if (!theClass->mBuildsOnElevation && !theMap->IsTileFlat(inX, inY)) {
                    theResult = false;
                } else {
                    theResult = !EEGetBlockingUnits(inPlayer, inUnitType, theTile, inIgnoreOwner, theUnits);
                    if (theResult)
                        outFlag = EEGetMovableTileUnits(inPlayer, inUnitType, theTile, inIgnoreOwner, theUnits);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x64f4f2
// Tells whether a unit type can be placed in the scenario editor (free camera)
// The scenario editor's (free camera) placement test (inferred)
bool EEEditorCanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY)
{
    std::vector<EEUnit*> theUnits;
    EEMap* theMap = ::gWorld.GetMap();
    EEUnitClass* theClass = gUnitClassTable[inType];
    bool theResult = inType == 0x167;
    EETile* theTile = theMap->GetTile(inX, inY);
    if (theTile != NULL && !theResult) {
        long theID = gUIManager->mScenarioEditorForm->GetUnitScreen()->mUnitUnderMouse;
        if (theID != -1) {
            EEUnit* theUnit = static_cast<EEUnit*>(::gWorld.GetUnit(theID));
            if (theUnit != NULL) {
                EEComplexUnit* theComplex = theUnit->GetComplexUnit();
                if (theComplex != NULL && theComplex->GetPlayer() == inPlayer && theUnit->mUnitClass->mTransportCapacity > 0
                    && theComplex->mCargoRoom >= theClass->mWeight && theClass->CanEnter(theComplex->mUnitClass))
                    goto CheckEdges;
            }
        }
        theResult = theClass->mTerrainTypes[theTile->mTerrainType];
        if (theResult) {
            if (!theClass->mBuildsOnElevation && !theMap->IsTileFlat(inX, inY))
                theResult = false;
            else
                theResult = !EEGetBlockingUnits(inPlayer, inType, theTile, 0, theUnits);
        }
        if (theClass->GetTileSize() <= 1 && theResult == true) {
            if (theClass->IsLandBuilding())
                goto CheckEdges;
            if (theClass->GetUnitType() == kUnitTypePathPoint || theClass->mIsComplex == 1)
                theResult = EEIsEditorSpacingClear(theClass, inLocation, inX, inY);
        }
    } else if (theTile != NULL || theResult) {
        theResult = true;
    }
    if (!theResult)
        return false;
CheckEdges:
    return theMap->RTSMap::GetTile(inX + 1, inY) != NULL && theMap->RTSMap::GetTile(inX - 1, inY) != NULL
        && theMap->RTSMap::GetTile(inX, inY + 1) != NULL && theMap->RTSMap::GetTile(inX, inY - 1) != NULL;
}

// FUNCTION: EXE 0x64f6f7
// Tells whether no unit around a location stands too close for a class in the scenario editor
// Whether no unit in the regions around (inX, inY) stands closer to inLocation than the two radii (units of a set
// that inClass may overlap excepted) (inferred)
bool EEIsEditorSpacingClear(EEUnitClass* inClass, GE3DPoint* inLocation, long inX, long inY)
{
    GE3DPoint thePosition;
    bool theResult = true;
    RTSRegionTable<EEComplexUnit*>* theTable = ::gWorld.mMap->mUnitRegions;
    long theRegionX = inX / theTable->mRegionSize;
    long theRegionY = inY / theTable->mRegionSize;
    long y;
    long x;
    unsigned long i;
    for (y = theRegionY - 1; theResult == true && y <= theRegionY + 1; y++) {
        for (x = theRegionX - 1; theResult == true && x <= theRegionX + 1; x++) {
            std::vector<EEComplexUnit*>* theRegion = theTable->GetRegion(x, y);
            if (theRegion == NULL)
                continue;
            for (i = 0; theResult == true && i < theRegion->size(); i++) {
                EEComplexUnit* theUnit = (*theRegion)[i];
                if (theUnit == NULL)
                    continue;
                theUnit->GetPosition(thePosition);
                float theRadius = inClass->mInnerCollisionSphere;
                float theOther = theUnit->mUnitClass->mInnerCollisionSphere;
                if (inLocation->DistanceTo(thePosition) <= (theRadius + theOther) * 0.5f
                    && !gUnitSetManager.Contains(theUnit->mUnitClass->mUnitType, inClass->mOverlapUnitSet))
                    theResult = false;
                else
                    theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x64f86d
// Tells whether a tile has no unit (trees aside) blocking a class for a player, and is not space
bool EEIsTileUnblocked(EEPlayer* inPlayer, long inUnitClass, EETile* inTile)
{
    long theCount = 0;
    if (inTile->mTerrainType == kTerrainSpace)
        return false;
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit != NULL && !theUnit->mUnitClass->mIsTree && EEGetTileUnitStanding(theUnit, inPlayer, inUnitClass, true) == 2)
            theCount++;
    }
    return theCount == 0;
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
