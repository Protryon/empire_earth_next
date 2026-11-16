// C:\EEX\Empire Earth\AI Thread\EECPPlacement.cpp (file name inferred: no __FILE__; RTTI class
// EECPPlacement, vtable 0x43c050; its key "EECPPlacement-Data" names the class). A computer player's build
// manager: 0x66af0..0x6e8d4 (the class-static key's guarded chain first, then three distance predicates built without
// /Og, the members in address order; the deque/vector/heap/sort COMDATs close the unit; EECPAttackSubGroupUnit.cpp ends
// at 0x66af0 and EECPCivilizationManager.cpp's key chain starts at 0x6e8d4).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

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
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIRandom.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEAEManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EEUnitCollector.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPNext.h"
#include "ee/EEUnitRegions.h"

// the class key (0x4e5d00, guard 0x4e5cf8 bit 1)
const UString EECPPlacement::sData("EECPPlacement-Data");
// SYNTHETIC: EXE 0x466af0   (wrapper)
// SYNTHETIC: EXE 0x466afa   (ctor)
// SYNTHETIC: EXE 0x466b0b   (registrar)
// SYNTHETIC: EXE 0x466b17   (dtor)

// Orders two (continent, town) pairs by the distance of the continent's centre from the reference tile
// Built without /Og (frame locals, `this` spilled to [ebp-0x3c]); the -4 -> -0x20 bool copy is the inline (tile, tile)
// operator's own result. NEAR: #pragma optimize("g", off) alone gives 45 (frame 0x20 vs 0x3c), not exact: removed
bool EETileDistanceLess::operator()(const std::pair<EEAIContinent*, EETown*>& inFirst,
    const std::pair<EEAIContinent*, EETown*>& inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst.first->GetHullCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond.first->GetHullCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// Orders two choke points by the distance of their centre from the reference tile
// Built without /Og, as 0x466b6d
bool EETileDistanceLess::operator()(EEAIChokePoint* inFirst, EEAIChokePoint* inSecond) const
{
    const U2DPoint<long>& theFirstCenter = inFirst->GetHullCenter();
    EETileCoordinate theFirst;
    theFirst.mX = theFirstCenter.mX;
    theFirst.mY = theFirstCenter.mY;
    const U2DPoint<long>& theSecondCenter = inSecond->GetHullCenter();
    EETileCoordinate theSecond;
    theSecond.mX = theSecondCenter.mX;
    theSecond.mY = theSecondCenter.mY;
    return (*this)(theFirst, theSecond);
}

// Creates the build manager for a civilization manager's player, with empty queues and no town centre tile
EECPPlacement::EECPPlacement(EEComputerPlayer* inPlayer)
    : mPlayer(inPlayer)
{
    this->mTownCenterTile = U2DPoint<long>(-10000, -10000);
    this->mNextResourceCenter = 0;
    this->mSiteTilesCollected = false;
}

// Destroys the build manager (members only)
EECPPlacement::~EECPPlacement()
{
}

// Tells whether the player has a unit of the given type within the squared distance of a tile
bool EECPPlacement::HasUnitNear(long inUnitType, long inX, long inY, float inDistance)
{
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(inUnitType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inDistance);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// Tells whether one of the player's anti-air buildings covers a tile (within its range)
// NEAR (69): the two mF9C reads and the product order; temporary vs named predicate
bool EECPPlacement::HasAntiAirNear(long inX, long inY)
{
    std::vector<EEUnit*> theUnits;
    long theType = EECPAntiAirClass(this->mPlayer);
    float theFirst = this->mPlayer->mUnitClassUpgrades[theType]->mRange;
    float theSecond = this->mPlayer->mUnitClassUpgrades[theType]->mRange;
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, theSecond * theFirst);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// Tells whether one of the player's settlements, town centres or capitols is within the distance of a tile
// NEAR (99.5): the predicate temporary's r*r store comes before mCenter.mY here, after it in the original (header
// ctor probes in build/a: body assignment, const-reference x/y: no change)
bool EECPPlacement::HasTownCenterNear(long inX, long inY, float inDistance)
{
    std::vector<EEUnit*> the2B;
    std::vector<EEUnit*> the23;
    std::vector<EEUnit*> the29;
    std::vector<EEUnit*> theOthers;
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(kUnitTypeSettlement, this->mPlayer->mPlayerID, the2B);
    EEGetPlayerUnitsOfType(kUnitTypeTownCenter, this->mPlayer->mPlayerID, the23);
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, the29);
    theUnits.insert(theUnits.end(), the2B.begin(), the2B.end());
    theUnits.insert(theUnits.end(), the23.begin(), the23.end());
    theUnits.insert(theUnits.end(), the29.begin(), the29.end());
    theUnits.insert(theUnits.end(), theOthers.begin(), theOthers.end());
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(),
        EECPUnitWithinDistance(inX, inY, inDistance * inDistance));
    return theIt != theUnits.end();
}

// Tells whether one of the player's gates is within the radius of a tile
bool EECPPlacement::HasGateNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    float theDistance = inRadius * inRadius;
    EEGetPlayerUnitsOfType(EECPGateClass(this->mPlayer), this->mPlayer->GetPlayerID(), theUnits);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(),
        EECPUnitWithinDistance(inX, inY, theDistance));
    return theIt != theUnits.end();
}

// Tells whether one of the player's towers is within a tile's squared distance (inRadius is used as squared)
bool EECPPlacement::HasTowerNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    long theType = EECPTowerClass(this->mPlayer);
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inRadius);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}

// Tells whether one of the player's walls is within a tile's squared distance (inRadius is used as squared)
bool EECPPlacement::HasWallNear(long inX, long inY, float inRadius)
{
    std::vector<EEUnit*> theUnits;
    long theType = EECPWallClass(this->mPlayer);
    EEGetPlayerUnitsOfType(theType, this->mPlayer->mPlayerID, theUnits);
    EECPUnitWithinDistance thePredicate(inX, inY, inRadius);
    std::vector<EEUnit*>::iterator theIt = std::find_if(theUnits.begin(), theUnits.end(), thePredicate);
    return theIt != theUnits.end();
}





// Collects candidate dock tiles (kind 2: water) or space dock tiles (kind 4: space) on the largest such area
// next to the town centre's continent, nearest the town first; then marks the site tiles collected
void EECPPlacement::CollectSiteTiles(char inKind)
{
    std::vector<U2DPoint<long> > theRegion;
    std::vector<U2DPoint<long> > theRing;
    U2DPoint<long> theCenter;
    EEMap* theMap = gWorld.mMap;
    std::vector<U2DPoint<long> >& theTiles = inKind == 2 ? this->mWaterSites : this->mSpaceSites;
    theCenter = this->mTownCenterTile;
    if (theMap == NULL || theCenter.mX == -10000 || theCenter.mY == -10000)
        return;
    unsigned short theKindID = (unsigned short)gContinentManager.GetLargestContinent(inKind);
    EEAIContinent* theKindContinent = gContinentManager.GetContinentFromID(theKindID);
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(theCenter.mX, theCenter.mY);
    if (theContinent == NULL)
        return;
    if (gContinentManager.mTileFlags[theKindID * gContinentManager.mFlagsWidth + theContinent->GetID()]) {
        theKindContinent->GetShoreNextTo(theContinent->GetID(), theTiles);
        theRegion = theTiles;
        for (std::vector<U2DPoint<long> >::iterator it = theRegion.begin(); it != theRegion.end(); it++) {
            EEGetSquareRingTiles(it->mX, it->mY, 1, theRing);
            theTiles.insert(theTiles.end(), theRing.begin(), theRing.end());
        }
        std::sort(theTiles.begin(), theTiles.end());
        theTiles.erase(std::unique(theTiles.begin(), theTiles.end()), theTiles.end());
        for (std::vector<U2DPoint<long> >::iterator theIt = theTiles.begin(); theIt != theTiles.end();) {
            EETile* theTile = theMap->GetTileAtLocation(*theIt);
            if (inKind == 2 ? theTile != NULL && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)
                : theTile != NULL && theTile->mTerrainType == kTerrainSpace)
                theIt++;
            else
                theIt = theTiles.erase(theIt);
        }
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter));
    }
    this->mSiteTilesCollected = true;
}

// Tells whether armed enemy units stand near a tile (influence regions around it, one tile on small island
// or planet maps), ignoring idle searchers (timer above 60 s)
bool EECPPlacement::HasEnemyNear(long inX, long inY)
{
    // a real threat that the player sees or remembers (EECPNextSiteThreat: enemy soldiers seen within kNextThreatRange
    // tiles lately, an enemy tower or armed building in range; docs/AI_RESEARCH.md §8.34). The original counted every
    // armed unit not its own in a box of influence regions, whether the player saw them or not (deleted in stage 5).
    return EECPNextSiteThreat(this->mPlayer, inX, inY, false, NULL) != 0;
}

// Finds an airport site with room for 4 (else 3) runway pieces facing away from the town centre, rings 5..99
// around it, reachable and with no enemies near
bool EECPPlacement::FindAirportSite(EETileCoordinate& outTile)
{
    std::vector<U2DPoint<long> > theTiles;
    bool theResult = false;
    U2DPoint<long>& theTownTile = this->mTownCenterTile;
    EETileCoordinate theCenter;
    theCenter.mX = theTownTile.mX;
    theCenter.mY = theTownTile.mY;
    if (theCenter.mX != -10000 && theCenter.mY != -10000) {
        long theSize = gUnitClassTable[kUnitTypeAirport]->mTileSize;
        long theWallSize = gUnitClassTable[kUnitTypeAirportRunway]->mTileSize;
        long theLength = theSize + theWallSize * 4 + 2;
        long theOffset = -2 - theSize;
        for (long k = 4; k >= 3 && !theResult; k--) {
            for (long r = 5; r < 100 && !theResult; r++) {
                EEGetSquareRingTiles(theCenter.mX, theCenter.mY, r, theTiles);
                for (std::vector<U2DPoint<long> >::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
                    it++) {
                    long theX = it->mX;
                    long theY = it->mY;
                    if (gWorld.mMap->GetTileAtLocation(*it) == NULL
                        || !gContinentManager.AreConnected(gContinentManager.GetContinentAt(theCenter.GetX(), theCenter.GetY()),
                            gContinentManager.GetContinentAt(*it), this->mPlayer->GetCitizenPassability()))
                        continue;
                    switch (EEGetFacingQuarter(theX, theY)) {
                    case 0:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - 1, theSize + 2, theLength, 0);
                        break;
                    case 1:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - theOffset - theLength - 1, theSize + 2,
                            theLength, 0);
                        break;
                    case 2:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - 1, theY - 1, theLength, theSize + 2, 0);
                        break;
                    case 3:
                        theResult = EEPlayerBlockTakes(this->mPlayer, 0x42, theX - theOffset - theLength - 1, theY - 1, theLength,
                            theSize + 2, 0);
                        break;
                    }
                    if (!theResult)
                        continue;
                    theResult = !this->HasEnemyNear(theX, theY);
                    if (theResult) {
                        outTile.mX = it->mX;
                        outTile.mY = it->mY;
                    }
                }
            }
            theLength -= theWallSize;
        }
    }
    return theResult;
}
// Finds an anti-air site around a building (FindAntiAirSiteAround its tile and size)
bool EECPPlacement::FindAntiAirSiteNear(EEUnit* inUnit, EETileCoordinate& outTile)
{
    long theSize = inUnit->mUnitClass->mTileSize;
    EETileCoordinate theTile = inUnit->GetTile();
    return this->FindAntiAirSiteAround(theTile, theSize, outTile);
}

// Finds an anti-air site in square rings growing out from a tile up to the anti-air range, on the tile's
// continent
bool EECPPlacement::FindAntiAirSiteAround(EETileCoordinate& inTile, long inSize, EETileCoordinate& outTile)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EETileCoordinate> theUnused;
    unsigned short theContinent = gContinentManager.GetContinent(inTile, kAIPathLandMass);
    long theType = EECPAntiAirClass(this->mPlayer);
    bool theResult = false;
    EEUnitClass* theClass = gUnitClassTable[theType];
    long theRange = UMFloatToLong(this->mPlayer->GetUpgrade(theType)->mRange);
    long theX = inTile.GetX();
    long theSize = theClass->GetTileSize();
    long theY = inTile.GetY();
    long theRing = inSize;
    for (long i = 0; i < theRange && !theResult; i++) {
        EEGetSquareBlockRing(1, theX, theY, theRing, theTiles, 0);
        theRing += 2;
        theX--;
        theY--;
        for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
            it++) {
            theResult = this->CanBuildAt(theContinent, theType, *it, theSize + 1, false, true, inTile.GetX(), inTile.GetY());
            if (theResult) {
                outTile.SetX(it->GetX());
                outTile.SetY(it->GetY());
            }
        }
    }
    return theResult;
}

// FindAntiAirSite's building clusters: a box around a building and the number of buildings in it (20-byte elements; the
// pair's (const&, const&) ctor 0x6c60f and operator= 0x6c5ce are out of line). The original's find_if (0x6c027)
// tests the box with U2DRectangle<long>::Contains (0xd190) and grows it with 0x19f9d: one same-layout box type
// (provisional names)
typedef std::pair<EEAIAStarBounds, unsigned long> EECPBMCluster;

struct EECPBMClusterHolds {
    EECPBMClusterHolds(EEUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(const EECPBMCluster& inCluster) const
        { return inCluster.first.Contains(U2DPoint<long>(this->mUnit->mTileX, this->mUnit->mTileY)); }

    EEUnit* mUnit;                          // +0x00
};

struct EECPBMClusterGreater {
    bool operator()(const EECPBMCluster& inFirst, const EECPBMCluster& inSecond) const
        { return inFirst.second > inSecond.second; }
};

// Finds an anti-air site: by a capitol not yet covered, then by a wonder, then in the biggest clusters of
// the player's buildings
bool EECPPlacement::FindAntiAirSite(EETileCoordinate& outTile)
{
    std::vector<EECPBMCluster> theClusters;
    EEAIAStarBounds theBounds;
    std::vector<EEUnit*> theUnits;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETechTree* theTechTree = this->mPlayer->GetTechTree();
    long theType = EECPAntiAirClass(this->mPlayer);
    bool theResult = false;
    std::vector<EEUnit*>::iterator it;
    EEUnit* theUnit;
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theUnits);
    for (it = theUnits.begin(); it != theUnits.end() && !theResult; it++) {
        theResult = !this->HasAntiAirNear((*it)->GetTileX(), (*it)->GetTileY());
        if (theResult)
            theResult = this->FindAntiAirSiteNear(*it, outTile);
    }
    if (theResult)
        return theResult;
    theUnits.clear();
    for (theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EETechTreeNode* theNode = theTechTree->Get(theUnit->mUnitClass->mTechnology);
        if (theNode != NULL && theNode->mData->mKind == 4)
            theUnits.push_back(theUnit);
    }
    for (it = theUnits.begin(); it != theUnits.end() && !theResult; it++) {
        theResult = !this->HasAntiAirNear((*it)->GetTileX(), (*it)->GetTileY());
        if (theResult)
            theResult = this->FindAntiAirSiteNear(*it, outTile);
    }
    if (theResult)
        return theResult;
    long theRadius = UMFloatToLong(this->mPlayer->mUnitClassUpgrades[theType]->mRange * 0.5f);
    for (theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!theUnit->mUnitClass->IsLandBuilding() || theUnit->mUnitClass->mUnitType == kUnitTypeAA10Gun88mm
            || theUnit->mUnitClass->mUnitType == kUnitTypeAA13MissileTower || theUnit->mUnitClass->mIsWall)
            continue;
        std::vector<EECPBMCluster>::iterator theFound = std::find_if(theClusters.begin(),
            theClusters.end(), EECPBMClusterHolds(theUnit));
        if (theFound != theClusters.end())
            theFound->second++;
        else {
            theBounds.mLeft = theUnit->GetTileX();
            theBounds.mTop = theUnit->GetTileY();
            theBounds.mRight = theUnit->GetTileX();
            theBounds.mBottom = theUnit->GetTileY();
            theBounds.Grow(theRadius, true);
            theClusters.push_back(EECPBMCluster(theBounds, 1));
        }
    }
    std::sort(theClusters.begin(), theClusters.end(), EECPBMClusterGreater());
    for (std::vector<EECPBMCluster>::iterator theIt = theClusters.begin();
        theIt != theClusters.end() && !theResult; theIt++) {
        theBounds = theIt->first;
        theTile.mX = theBounds.mLeft + theRadius;
        theTile.mY = theBounds.mTop + theRadius;
        theResult = !this->HasAntiAirNear(theTile.mX, theTile.mY);
        if (theResult)
            theResult = this->FindAntiAirSiteAround(theTile, 1, outTile);
    }
    return theResult;
}

// FindCenterSite's sort of the resource centres (8 bytes by value to std::sort 0x6c120; its operator() sits among the sort
// COMDATs) (provisional name)
// New: a resource centre's squared tile distance from the town centre (1 << 40 with no town centre or unit)
static long long EECPBMHomeDistance2(EECPPlacement* inManager, EECPResourceCenter* inCenter)
{
    EEUnit* theUnit = inCenter->mUnit != NULL ? static_cast<EEUnit*>(inCenter->mUnit)
        : inCenter->mUnits.empty() ? NULL : inCenter->mUnits[0].Get();
    if (theUnit == NULL || inManager->mTownCenterTile.mX == -10000)
        return 1LL << 40;
    long long theDX = theUnit->GetTileX() - inManager->mTownCenterTile.mX;
    long long theDY = theUnit->GetTileY() - inManager->mTownCenterTile.mY;
    return theDX * theDX + theDY * theDY;
}

// New: the tiles from the town centre within which FindCenterSite fills the centres first
static const long long kHomeSiteRange = 20;

// New: orders resource centres by their squared tile distance from the town centre (stable: equal distances
// keep their order; centres with no unit go last)
static void EECPBMNearestHomeFirst(EECPPlacement* inManager, std::vector<EECPResourceCenter*>& ioCenters)
{
    if (inManager->mTownCenterTile.mX == -10000)
        return;
    std::vector<std::pair<long long, unsigned long> > theOrder;
    for (unsigned long i = 0; i < ioCenters.size(); i++)
        theOrder.push_back(std::pair<long long, unsigned long>(EECPBMHomeDistance2(inManager, ioCenters[i]), i));
    std::sort(theOrder.begin(), theOrder.end());
    std::vector<EECPResourceCenter*> theSorted;
    for (unsigned long i = 0; i < theOrder.size(); i++)
        theSorted.push_back(ioCenters[theOrder[i].second]);
    ioCenters.swap(theSorted);
}

struct EECPBMCenterLess {
    EECPBMCenterLess(long inResource, long inPlayerID) : mResource(inResource), mPlayerID(inPlayerID) {}
    // inline (COMDAT 0x6d510 among the sort COMDATs): the centre whose units stand in fewer area effects of kind
    // mResource for mPlayerID comes first
    bool operator()(EECPResourceCenter* inFirst, EECPResourceCenter* inSecond) const
    {
        std::vector<EEUnit*> theUnits;
        unsigned long theFirst = 0;
        unsigned long theSecond = 0;
        unsigned long theCount;
        unsigned long i;
        theUnits.assign(inFirst->mUnits.begin(), inFirst->mUnits.end());
        theCount = theUnits.size();
        for (i = 0; i < theCount; i++) {
            if (gAEManager.HasAreaEffect(this->mResource, theUnits[i]->GetTileX(), (*(theUnits.begin() + i))->GetTileY(),
                this->mPlayerID))
                theFirst++;
        }
        theUnits.clear();
        theUnits.assign(inSecond->mUnits.begin(), inSecond->mUnits.end());
        theCount = theUnits.size();
        for (i = 0; i < theCount; i++) {
            if (gAEManager.HasAreaEffect(this->mResource, theUnits[i]->GetTileX(), theUnits[i]->GetTileY(), this->mPlayerID))
                theSecond++;
        }
        return theFirst < theSecond;
    }

    long mResource;                         // +0x00 (3, 4, 1 by the class)
    long mPlayerID;                         // +0x04
};

// Finds a site at a resource centre: a settlement at an unclaimed one, else a temple, university or hospital
// at a claimed one, preferring centres whose units stand in fewer matching area effects
bool EECPPlacement::FindCenterSite(long inUnitClass, EETileCoordinate& outTile)
{
    std::vector<EECPResourceCenter*> theCenters;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    long theResource = 0;
    bool theResult = false;
    unsigned long i;
    EECPResourceCenter* theCenter;
    if (inUnitClass == 0x2b) {
        for (i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
            theCenter = this->mPlayer->GetResourceCenter(i);
            if (theCenter != NULL && theCenter->mUnit == NULL)
                theCenters.push_back(theCenter);
        }
        i = 0;
        unsigned long theCount = theCenters.size();
        for (; i < theCount && !theResult; i++) {
            theCenter = theCenters[i];
            theResult = theCenter->FindBuildSite(kUnitTypeSettlement, theTile);
            if (theResult) {
                outTile.mX = theTile.mX;
                outTile.mY = theTile.mY;
            }
        }
    } else {
        switch (inUnitClass) {
        case 0x27:
            theResource = 3;
            break;
        case 0x3c:
            theResource = 4;
            break;
        case 0x3e:
            theResource = 1;
            break;
        }
        for (i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
            theCenter = this->mPlayer->GetResourceCenter(i);
            if (theCenter != NULL && theCenter->mUnit != NULL)
                theCenters.push_back(theCenter);
        }
        unsigned long theCount = theCenters.size();
        if (theCount != 0) {
            // port: the centres within kHomeSiteRange tiles of the town centre first, then the rest, each by the
            // original's area-effect order and nearest first among equals (the original spread temples over every
            // centre: they stood 30+ tiles out, undefended; playtest 2026-11-06)
            EECPBMNearestHomeFirst(this, theCenters);
            std::vector<EECPResourceCenter*>::iterator theFar = std::stable_partition(theCenters.begin(), theCenters.end(),
                [this](EECPResourceCenter* inCenter) { return EECPBMHomeDistance2(this, inCenter) <= kHomeSiteRange * kHomeSiteRange; });
            std::stable_sort(theCenters.begin(), theFar, EECPBMCenterLess(theResource, this->mPlayer->mPlayerID));
            std::stable_sort(theFar, theCenters.end(), EECPBMCenterLess(theResource, this->mPlayer->mPlayerID));
            for (unsigned long j = 0; j < theCount && !theResult; j++) {
                theCenter = theCenters[j];
                theResult = theCenter->FindBuildSite(inUnitClass, theTile);
                if (theResult) {
                    outTile.mX = theTile.mX;
                    outTile.mY = theTile.mY;
                }
            }
        }
    }
    return theResult;
}

// Finds a granary site just beyond a resource centre's drop-off distance with room for farms around it
bool EECPPlacement::FindGranarySite(EETileCoordinate& outTile)
{
    std::vector<EETileCoordinate> theTiles;
    bool theResult = false;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    EETileCoordinate theSite;
    theSite.mX = 0;
    theSite.mY = 0;
    long theRadius = this->mPlayer->mDropOffDistance;
    long theSize = gUnitClassTable[kUnitTypeGranary]->mTileSize;
    // economy.forage_patches: only round the centres within the claim range, nearest home first, and only sites within
    // it (the playtest AI's granaries stood far out, undefended: "stop claiming land it cannot defend")
    bool theClaim = EECPClaimRangeOn(this->mPlayer) && this->mTownCenterTile.mX != -10000;
    std::vector<EECPResourceCenter*> theCenters;
    for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters(); i++)
        if (!theClaim || EECPBMHomeDistance2(this, this->mPlayer->GetResourceCenter(i))
                <= (long long)EECPClaimRange(this->mPlayer) * EECPClaimRange(this->mPlayer))
            theCenters.push_back(this->mPlayer->GetResourceCenter(i));
    if (theClaim)
        EECPBMNearestHomeFirst(this, theCenters);
    for (unsigned long i = 0; i < theCenters.size() && !theResult; i++) {
        EECPResourceCenter* theResourceCenter = theCenters[i];
        theResourceCenter->GetCenter(theCenter);
        unsigned short theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
        for (long r = theRadius + 1; r < theRadius + 0x1e && !theResult; r++) {
            EEGetSquareRingTiles(theCenter.mX, theCenter.mY, r, theTiles);
            for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end() && !theResult;
                it++) {
                theResult = this->CanBuildAt(theContinent, kUnitTypeFarm, *it, theSize * 3, false, true, theCenter.mX, theCenter.mY);
                if (!theResult)
                    continue;
                theSite.mX = it->mX + theSize;
                theSite.mY = it->mY + theSize;
                if (theClaim) {
                    long theDX = theSite.mX - this->mTownCenterTile.mX, theDY = theSite.mY - this->mTownCenterTile.mY;
                    if (theDX * theDX + theDY * theDY > EECPClaimRange(this->mPlayer) * EECPClaimRange(this->mPlayer)) {
                        theResult = false;
                        continue;
                    }
                }
                theResult = this->CanBuildAt(theContinent, kUnitTypeGranary, theSite, theSize, false, true, theCenter.mX,
                    theCenter.mY);
                if (theResult) {
                    outTile.mX = theSite.mX;
                    outTile.mY = theSite.mY;
                }
            }
        }
    }
    return theResult;
}

// Finds a site for a production building at the active resource centres, round robin from the last used one
// Common found result and loop exit preserve the original 117-byte body.
bool EECPPlacement::FindProductionSite(long inUnitClass, EETileCoordinate& outTile)
{
    // port: the active resource centres nearest the town centre first (the original went round robin through all
    // of them: barracks and archery ranges stood 30+ tiles out at an undefended mine, playtest 2026-11-06)
    std::vector<EECPResourceCenter*> theCenters;
    for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
        EECPResourceCenter* theCenter = this->mPlayer->GetResourceCenter(i);
        if (theCenter != NULL && theCenter->mIsActive)
            theCenters.push_back(theCenter);
    }
    EECPBMNearestHomeFirst(this, theCenters);
    for (unsigned long i = 0; i < theCenters.size(); i++)
        if (theCenters[i]->FindBuildSite(inUnitClass, outTile))
            return true;
    return false;
}

// Finds a dock or navy yard site among the water site tiles away from existing ones; sites without a
// builder are skipped
bool EECPPlacement::FindShoreSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    std::vector<EEUnit*> the24;
    std::vector<EEUnit*> the41;
    std::vector<EEUnit*> theUnits;
    unsigned short theContinent = gContinentManager.GetContinent(this->mTownCenterTile, kAIPathLandMass);
    bool theResult = false;
    std::vector<U2DPoint<long> >& theSites = this->mWaterSites;
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    if (theSites.size() > 0) {
        EEGetPlayerUnitsOfType(kUnitTypeDock, thePlayerID, the24);
        EEGetPlayerUnitsOfType(kUnitTypeNavyYard, thePlayerID, the41);
        theUnits.insert(theUnits.end(), the24.begin(), the24.end());
        theUnits.insert(theUnits.end(), the41.begin(), the41.end());
        for (unsigned long i = 0, theCount = theSites.size(); i < theCount && !theResult; i++) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPUnitWithinDistance(theSites[i], 75.0f))
                != theUnits.end())
                continue;
            theResult = this->CanBuildAt(theContinent, inUnitClass, theSites[i], inSize, false, true, this->mTownCenterTile.GetX(),
                this->mTownCenterTile.GetY());
            if (theResult && this->HasBuilderFor(theSites[i], inUnitClass)) {
                outTile.mX = theSites[i].GetX();
                outTile.mY = theSites[i].GetY();
            } else
                theResult = false;   // no builder: keep looking
        }
    }
    return theResult;
}

// Finds a space dock, turret or station site among space tiles away from units of its class; sites
// without a builder are skipped
// Class 0x302 searches the shore tiles of the town continents (nearest first) on planet maps, else the town continent's
// shore tiles near own units; others use mSpaceSites. 0x2d8 keeps 75 from its kind and uses EEIsSpaceDockSiteOpen
// (its tile moved by -1, -1)
bool EECPPlacement::FindSpaceSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    bool theResult = false;
    std::vector<U2DPoint<long> >& theWater = this->mSpaceSites;
    if (theWater.size() == 0)
        return false;
    std::vector<U2DPoint<long> > theSites;
    if (inUnitClass == 0x302) {
        std::vector<U2DPoint<long> > theTiles;
        U2DPoint<long> theCenter;
        if ((gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) {
            std::vector<std::pair<EEAIContinent*, EETown*> > theContinents;
            for (unsigned long i = 0; i < this->mPlayer->mTowns.size(); i++) {
                EETown* theTown = this->mPlayer->mTowns[i];
                if (theTown != NULL && theTown->mContinent != 0) {
                    EEAIContinent* theContinent = gContinentManager.GetContinentFromID(theTown->mContinent);
                    theContinents.push_back(std::pair<EEAIContinent*, EETown*>(theContinent, theTown));
                }
            }
            std::sort(theContinents.begin(), theContinents.end(),
                EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.GetY()));
            for (std::vector<std::pair<EEAIContinent*, EETown*> >::iterator it = theContinents.begin();
                it != theContinents.end(); it++) {
                theTiles = it->first->GetShoreTiles();
                theCenter = it->second->GetHullCenter();
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter.mX, theCenter.GetY()));
                theSites.insert(theSites.end(), theTiles.begin(), theTiles.end());
            }
        } else {
            theTiles = gContinentManager.GetContinentAt(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY())->GetShoreTiles();
            std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(this->mTownCenterTile.mX, this->mTownCenterTile.GetY()));
            std::vector<U2DPoint<long> > theOwn;
            for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                if (theUnit->GetUnitClass()->mUnitType != kUnitTypeSpaceTurret)
                    theOwn.push_back(U2DPoint<long>(theUnit->GetTileX(), theUnit->GetTileY()));
            }
            for (unsigned long i = 0; i < theTiles.size(); i++) {
                theCenter = theTiles[i];
                std::sort(theOwn.begin(), theOwn.end(), EETileDistanceLess(theCenter.mX, theCenter.GetY()));
                if ((unsigned long)theCenter.DistanceToSquared(theOwn[0]) < 25)
                    theSites.push_back(theCenter);
            }
        }
    } else
        theSites = theWater;
    if (theSites.size() > 0) {
        std::vector<EEUnit*> theUnits;
        unsigned short theContinent = gContinentManager.GetContinent(this->mTownCenterTile, kAIPathLandMass);
        unsigned long thePlayerID = this->mPlayer->mPlayerID;
        float theDistance;
        if (inUnitClass == 0x2d8)
            theDistance = 75.0f;
        else
            theDistance = (this->mPlayer->mUnitClassUpgrades[inUnitClass])->mRange
                * (this->mPlayer->mUnitClassUpgrades[inUnitClass])->mRange;
        EEGetPlayerUnitsOfType(inUnitClass, thePlayerID, theUnits);
        for (unsigned long i = 0, theCount = theSites.size(); i < theCount && !theResult; i++) {
            if (std::find_if(theUnits.begin(), theUnits.end(), EECPUnitWithinDistance(theSites[i], theDistance))
                != theUnits.end())
                continue;
            if (inUnitClass != 0x2d8)
                theResult = this->CanBuildAt(theContinent, inUnitClass, theSites[i], inSize, false, true,
                    this->mTownCenterTile.mX, this->mTownCenterTile.GetY());
            else
                theResult = EEIsSpaceDockSiteOpen(theSites[i].mX, theSites[i].GetY(), this->mPlayer);
            if (theResult && !this->HasBuilderFor(theSites[i], inUnitClass))
                theResult = false;   // no builder: keep looking
            if (!theResult)
                continue;
            outTile.mX = theSites[i].mX;
            outTile.mY = theSites[i].GetY();
            if (inUnitClass == 0x2d8) {
                outTile.mX--;
                outTile.mY--;
            }
        }
    }
    return theResult;
}

// Finds a tower site: along town continent shores on island maps, then at a resource centre, then near an
// open gate, then at a choke point near a town not yet guarded by that class
bool EECPPlacement::FindTowerSite(long inUnitClass, long inSize, EETileCoordinate& outTile)
{
    std::vector<EEUnit*> theGates;
    std::vector<EEAIChokePoint*> theChokePoints;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<std::pair<EEAIContinent*, EETown*> > theContinents;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theRing;
    std::vector<EETileCoordinate> theRadiusTiles;
    U2DPoint<long> theCenter;
    EETileCoordinate theSite;
    EETileArea theArea;
    EEMap* theMap = gWorld.GetMap();
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    long theGateType = EECPGateClass(this->mPlayer);
    unsigned long thePlayerID = this->GetPlayer()->GetPlayerID();
    bool theResult = false;
    unsigned long i;
    unsigned long j;
    unsigned long theCount;
    if (this->mTownCenterTile.GetX() != -10000 && this->mTownCenterTile.GetY() != -10000) {
        float theRange = (this->GetPlayer()->mUnitClassUpgrades[inUnitClass]->GetRange() * 1.5f) * (this->GetPlayer()->mUnitClassUpgrades[inUnitClass]->GetRange() * 1.5f);
        if (theMap != NULL && (theMap->GetMapCharacteristics() & kRMMapIslandMap) == kRMMapIslandMap) {
            for (i = 0; i < this->GetPlayer()->GetTownList().size(); i++) {
                EETown* theTown = this->GetPlayer()->mTowns[i];
                if (theTown != NULL && theTown->GetContinent() != 0) {
                    EEAIContinent* theContinent = gContinentManager.GetContinentFromID(theTown->GetContinent());
                    theContinents.push_back(std::pair<EEAIContinent*, EETown*>(theContinent, theTown));
                }
            }
            std::sort(theContinents.begin(), theContinents.end(),
                EETileDistanceLess(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY()));
            for (std::vector<std::pair<EEAIContinent*, EETown*> >::iterator it = theContinents.begin();
                it != theContinents.end() && !theResult; it++) {
                theTiles = it->first->GetShoreTiles();
                theCenter = it->second->GetHullCenter();
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theCenter.GetX(), theCenter.GetY()));
                for (i = 0, theCount = theTiles.size(); i < theCount && !theResult; i++) {
                    theCenter = theTiles[i];
                    bool theNear;
                    if (inUnitClass == 0x2f0)
                        theNear = this->HasUnitNear(0x2f0, theCenter.GetX(), theCenter.GetY(), theRange);
                    else
                        theNear = this->HasTowerNear(theCenter.GetX(), theCenter.GetY(), theRange);
                    if (theNear)
                        continue;
                    for (unsigned long r = 1; r <= 2 && !theResult; r++) {
                        EEGetSquareRingTiles(theCenter.GetX(), theCenter.GetY(), r, theRing);
                        for (unsigned long k = 0, n = theRing.size(); k < n && !theResult; k++) {
                            EETile* theTile = theMap->GetTile((*(theRing.begin() + k)).GetX(),
                                theRing[k].GetY());
                            if (theClass != NULL && theTile != NULL && theClass->mTerrainTypes[theTile->GetTerrainType()]
                                && EEPlayerBlockTakes(this->GetPlayer(), inUnitClass, (*(theRing.begin() + k)).GetX(),
                                    (*(theRing.begin() + k)).GetY(), inSize, inSize, 0)) {
                                theResult = true;
                                outTile.mX = theRing[k].GetX();
                                outTile.mY = (*(theRing.begin() + k)).GetY();
                            } else
                                theResult = false;
                        }
                    }
                }
            }
        }
        if (!theResult) {
            // port: the resource centres the gatherers work first, the nearest the town centre first (the original
            // took them in list order: a tower at a far, unworked mine guarded nothing the raiders went for)
            std::vector<std::pair<long long, unsigned long> > theOrder;
            for (i = 0; i < this->GetPlayer()->mResourceCenters.size(); i++) {
                EECPResourceCenter* theResourceCenter = this->GetPlayer()->GetResourceCenter(i);
                if (theResourceCenter == NULL || theResourceCenter->mUnit == NULL)
                    continue;
                long long theDX = theResourceCenter->mUnit->GetTileX() - this->mTownCenterTile.mX;
                long long theDY = theResourceCenter->mUnit->GetTileY() - this->mTownCenterTile.mY;
                theOrder.push_back(std::pair<long long, unsigned long>(
                    (theResourceCenter->mIsActive ? 0LL : 1LL << 40) + theDX * theDX + theDY * theDY, i));
            }
            std::sort(theOrder.begin(), theOrder.end());
            for (j = 0; j < theOrder.size() && !theResult; j++) {
                EECPResourceCenter* theResourceCenter = this->GetPlayer()->GetResourceCenter(theOrder[j].second);
                theResult = theResourceCenter->FindBuildSite(inUnitClass, theSite);
                if (theResult) {
                    outTile.mX = theSite.GetX();
                    outTile.mY = theSite.GetY();
                }
            }
        }
        if (!theResult) {
            EEUnitClass* theGateClass = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(gUnitClassTable[theGateType]);
            EEGetPlayerUnitsOfType(theGateType, thePlayerID, theGates);
            std::vector<EEUnit*>::iterator theGate;
            for (theGate = theGates.begin(); theGate != theGates.end();) {
                EEComplexUnit* theComplex = (*theGate)->GetComplexUnit();
                if (theComplex->GetPreviousPart() == NULL && theComplex->GetNextPart() != NULL)
                    theGate++;
                else
                    theGate = theGates.erase(theGate);
            }
            std::sort(theGates.begin(), theGates.end(), EECPUnitTileDistanceLess(this->mTownCenterTile));
            for (theGate = theGates.begin(); theGate != theGates.end() && !theResult; theGate++) {
                bool theNear;
                if (inUnitClass == 0x2f0)
                    theNear = this->HasUnitNear(0x2f0, (*theGate)->GetTileX(), (*theGate)->GetTileY(), theRange);
                else
                    theNear = this->HasTowerNear((*theGate)->GetTileX(), (*theGate)->GetTileY(), theRange);
                if (!theNear) {
                    EEGetTilesInRadius((*theGate)->GetTileX(), (*theGate)->GetTileY(), 5, true, theRadiusTiles);
                    std::vector<EETileCoordinate>::iterator theIt;
                    for (theIt = theRadiusTiles.begin(); theIt != theRadiusTiles.end();) {
                        if (this->HasGateNear(theIt->GetX(), theIt->GetY(), 2.0f) || this->HasWallNear(theIt->GetX(), theIt->GetY(), 4.0f)   // 2 tiles squared, like the gate test (was 2: 1.4 tiles)
                            || (inUnitClass == 0x2f0 ? this->HasUnitNear(0x2f0, theIt->GetX(), theIt->GetY(), theRange)
                                : this->HasTowerNear(theIt->GetX(), theIt->GetY(), theRange)))
                            theIt = theRadiusTiles.erase(theIt);
                        else
                            theIt++;
                    }
                    EEAIRandomShuffle(theRadiusTiles.begin(), theRadiusTiles.end());
                    for (theIt = theRadiusTiles.begin(); theIt != theRadiusTiles.end() && !theResult; theIt++) {
                        theResult = EEPlayerBlockTakes(this->GetPlayer(), inUnitClass, theIt->GetX(), theIt->GetY(), inSize, inSize, 0);
                        if (theResult) {
                            outTile.mX = theIt->GetX();
                            outTile.mY = theIt->GetY();
                        }
                    }
                }
            }
        }
        if (!theResult) {
            gContinentManager.GetChokePoints(theChokePoints,
                gContinentManager.GetContinentAt(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY())->GetID(), true);
            if (theChokePoints.size() > 0) {
                std::sort(theChokePoints.begin(), theChokePoints.end(),
                    EETileDistanceLess(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY()));
                for (i = 0, theCount = theChokePoints.size(); i < theCount && !theResult; i++) {
                    bool theNearTown = false;
                    for (j = 0; j < this->GetPlayer()->GetNumTowns() && !theNearTown; j++) {
                        EETown* theTown = this->GetPlayer()->mTowns[j];
                        theNearTown = theTown->GetHullCenter().DistanceToSquared(
                            (*(theChokePoints.begin() + i))->GetHullCenter()) < 50;
                    }
                    if (theNearTown) {
                        theArea.mTop = theChokePoints[i]->GetHullBounds().mTop;
                        theArea.mLeft = (*(theChokePoints.begin() + i))->GetHullBounds().mLeft;
                        theArea.mBottom = (*(theChokePoints.begin() + i))->GetHullBounds().mBottom;
                        theArea.mRight = (*(theChokePoints.begin() + i))->GetHullBounds().mRight;
                        theArea.Grow(UMFloatToLong((*(this->GetPlayer()->mUnitClassUpgrades.begin() + inUnitClass))->GetRange() * 2.0f), true);
                        EEAnyUnit theFilter;
                        EEGetUnitsInRectangle(theArea, theUnits, theFilter, true);
                        bool theTaken = false;
                        unsigned long k, n;
                        for (k = 0, n = theUnits.size(); k < n && !theTaken; k++) {
                            EEComplexUnit* theUnit = theUnits[k];
                            theTaken = theUnit->GetHitPoints() > 0 && theUnit->GetUnitClass()->GetUnitType() == inUnitClass;
                        }
                        if (!theTaken) {
                            theChokePoints[i]->GetTileSet().BuildPointList(theRing, 1);
                            std::sort(theRing.begin(), theRing.end(),
                                EETileDistanceLess(this->mTownCenterTile.GetX(), this->mTownCenterTile.GetY()));
                            unsigned long m;
                            for (m = 0, n = theRing.size(); m < n && !theResult; m++)
                                theResult = EEPlayerBlockTakes(this->GetPlayer(), inUnitClass, theRing[m].GetX(), theRing[m].GetY(), inSize,
                                    inSize, 0);
                            if (theResult) {
                                outTile.mX = theRing[m - 1].GetX();
                                outTile.mY = theRing[m - 1].GetY();
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// Finds a wonder site with a margin of inSize in rings around the town centre (only once a wall gate exists)
// NEAR (38): the outTile store through EETileCoordinate(theTiles[i].GetX() + h, h + theTiles[i].GetY()) scores 62.5
// but the equivalence check calls it DIFF (the mX read through the out-of-line operator+ return)
bool EECPPlacement::FindWonderSite(long inUnitClass, unsigned long inSize, EETileCoordinate& outTile)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    std::vector<EETileCoordinate> theTiles;
    bool theResult = false;
    EETileCoordinate theCenter(0, 0);
    long theClassSize = theClass->GetTileSize();
    theCenter = this->mTownCenterTile;
    if (theCenter.GetX() != -10000 && theCenter.GetY() != -10000) {
        unsigned short theContinent = gContinentManager.GetContinent(theCenter, kAIPathLandMass);
        {   // (the original searched only once its wall manager had planned a gate; stage 6 has none)
            for (unsigned long r = 1; r < 200; r++) {
                EEGetSquareRingTiles(theCenter.GetX(), theCenter.GetY(), r, theTiles);
                unsigned long theCount = theTiles.size();
                if (theCount == 0)
                    break;
                for (unsigned long i = 0; i < theCount && !theResult; i++) {
                    theResult = this->CanBuildAt(theContinent, inUnitClass, theTiles[i], theClassSize + inSize, false, true,
                        theCenter.GetX(), theCenter.GetY());
                    if (theResult) {
                        outTile.SetX(theTiles[i].GetX() + (inSize >> 1));
                        outTile.SetY((inSize >> 1) + theTiles[i].GetY());
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46a228
// Finds a building site for a class by its kind (wonder, granary, dock, centre, airport, tower, production,
// anti-air, space) or, for farms, an unbuilt farm foundation; returns whether one was found
// NEAR (55): the switch layout, which depends on the tile parameter: the shared declaration takes a long*
// (EECPCivilizationManager.cpp), this definition a tile reference
bool EECPPlacement::FindSite(long inUnitClass, EETileCoordinate& theTile)
{
    long theID = gUnitClassTable[inUnitClass] != NULL ? gUnitClassTable[inUnitClass]->GetTechnology() : 100000;
    EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(theID);
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    bool theResult = false;
    if (theClass == NULL)
        return theResult;
    if (theNode != NULL && theNode->mData->mKind == 4) {
        for (long theSize = 10; theSize != 0 && !theResult; theSize -= 2)
            theResult = this->FindWonderSite(inUnitClass, theSize, theTile);
        return theResult;
    }
    switch (inUnitClass) {
    case kUnitTypeGranary:
        return this->FindGranarySite(theTile);
    case kUnitTypeDock:
    case kUnitTypeNavyYard:
        return this->FindShoreSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeTemple:
    case kUnitTypeSettlement:
    case kUnitTypeUniversity:
    case kUnitTypeHospital:
        return this->FindCenterSite(inUnitClass, theTile);
    case kUnitTypeAirport:
        return this->FindAirportSite(theTile);
    case kUnitTypeGuardTowerPaleo:
    case kUnitTypeTankObstacle:
    case kUnitTypeGuardTowerCopper:
    case kUnitTypeGuardTowerBronze:
    case kUnitTypeGuardTowerMiddle:
    case kUnitTypeGuardTowerImperial:
    case kUnitTypeGuardTowerWW1:
    case kUnitTypeGuardTowerDigital:
    case kUnitTypePriestTower:
    case kUnitTypeGuardTowerSpace:
        return this->FindTowerSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeBarracks:
    case kUnitTypeArcheryRange:
    case kUnitTypeSiegeFactory:
    case kUnitTypeStable:
    case kUnitTypeMissileBase:
    case kUnitTypeTankFactory:
    case kUnitTypeCyberFactory:
    case kUnitTypeCyberLaboratory:
    case kUnitTypeCpMissileBase:
        return this->FindProductionSite(inUnitClass, theTile);
    case kUnitTypeAA10Gun88mm:
    case kUnitTypeAA13MissileTower:
    case kUnitTypeAA15MissileTower:
        return this->FindAntiAirSite(theTile);
    case kUnitTypeSpaceDock:
    case kUnitTypeSpaceTurret:
    case kUnitTypeOrbitalSpaceStation:
        return this->FindSpaceSite(inUnitClass, theClass->GetTileSize(), theTile);
    case kUnitTypeFarm:
    case kUnitTypeRoboticFarm:
        {
            for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL && !theResult;
                theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                theResult = false;
                if (theUnit->mUnitClass->GetUnitType() != inUnitClass || theUnit->mComplexUnit->mIsBuilt
                    || theUnit->mComplexUnit->mHitPoints != 1)
                    continue;
                theTile.mX = theUnit->mComplexUnit->mTileX;
                theTile.mY = theUnit->mComplexUnit->mTileY;
                theResult = true;
            }
        }
        return theResult;
    }
    return theResult;
}


// FUNCTION: EXE 0x46a833
// Tells whether a square of inSize at a tile lies within the distance of the far end of an airport runway
bool EECPPlacement::IsNearAirportEnd(long inX, long inY, long inSize, float inDistance)
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theCenter(inX + inSize / 2, inSize / 2 + inY);
    U2DPoint<long> theGate;
    long theGateSize = gUnitClassTable[kUnitTypeAirportRunway]->mTileSize;
    long theHalf = UMFloatToLong(theGateSize * 0.5f);
    bool theResult = false;
    float theRange = ((float)theHalf + inDistance) * ((float)theHalf + inDistance);
    EEGetPlayerUnitsOfType(kUnitTypeAirport, this->mPlayer->mPlayerID, theUnits);
    unsigned long i = 0;
    unsigned long theCount = theUnits.size();
    for (; i < theCount && !theResult; i++) {
        EEComplexUnit* theWall = theUnits[i]->mComplexUnit;
        EEComplexUnit* theLast = theWall->mNextPart;
        while (theLast->mNextPart != NULL)
            theLast = theLast->mNextPart;
        EETileCoordinate theEnd = theLast->GetTile();
        long theX = theEnd.mX;
        long theY = theEnd.mY;
        switch (theWall->mFacing) {
        case 0:
            theGate.mX = theX + theHalf;
            theGate.mY = theY + theGateSize;
            break;
        case 1:
            theGate.mX = theX + theHalf;
            theGate.mY = theY;
            break;
        case 2:
            theX += theGateSize;
        case 3:
            theGate.mX = theX;
            theGate.mY = theY + theHalf;
            break;
        }
        if ((float)theCenter.DistanceToSquared(theGate) > theRange)
            theResult = false;
        else
            theResult = true;
    }
    return theResult;
}




// Records a unit's tile as the town centre tile
// SetXY: VC6 evaluates the arguments right to left (mTileY first)
void EECPPlacement::SetTownCenter(EEUnit* inUnit)
{
    this->mTownCenterTile.SetXY(inUnit->mTileX, inUnit->mTileY);
}
// (static: only CanBuildAt calls it; emitted before it) (provisional name)
// Tells whether a unit class cannot stand on a tile (off the map or wrong terrain)
static bool EECPBMCannotStandOn(const EETileLocation& inTile, EEUnitClass* inClass)
{
    EETile* theTile = gWorld.mMap != NULL ? gWorld.mMap->GetTileAtLocation(inTile) : NULL;
    return !(theTile != NULL && inClass->mTerrainTypes[theTile->mTerrainType]);
}

// CanBuildAt: the test of a site class's tiles (find_if 0x6c4bb, its operator() expanded): the tile's kind-2 continent
// is mContinent (provisional name)
struct EECPBMOnContinent {
    EECPBMOnContinent(unsigned short inContinent) : mContinent(inContinent) {}
    bool operator()(const U2DPoint<long>& inTile) const
        { return gContinentManager.GetContinent(inTile, kAIPathLandMass) == this->mContinent; }

    unsigned short mContinent;              // +0x00
};

// Tells whether a class can be built at a tile: free ground on the right (or reachable) continent, no enemies;
// strictly also no choke point, hill, bush, runway end, town centre, gate near and walkable ground around it
// inMargin adds a one-tile margin; the reachability test (civ power 0x30) starts at (inX, inY); site classes need
// a side or corner tile on inContinent. The border must be passable except one run (at least 3 tiles for site classes)
bool EECPPlacement::CanBuildAt(unsigned short inContinent, long inUnitClass, EETileCoordinate& inTile, long inSize,
    bool inMargin, bool inStrict, long inX, long inY)
{
    std::vector<U2DPoint<long> > theTiles;
    U2DPoint<long> theCorner;
    unsigned long theCount = 0;
    long theX = inTile.GetX();
    long theY = inTile.GetY();
    bool theResult = EEPlayerBlockTakes(this->mPlayer, inUnitClass, inMargin ? theX - 1 : theX, inMargin ? theY - 1 : theY,
        inMargin ? inSize + 2 : inSize, inMargin ? inSize + 2 : inSize, 0);
    if (theResult) {
        if (!EECPIsSiteClass(inUnitClass) && inUnitClass != kUnitTypeAA10Gun88mm && inUnitClass != kUnitTypeAA13MissileTower && inUnitClass != kUnitTypeAA15MissileTower) {
            if (gCivPowersManager.HasPower(this->mPlayer, kCivPowerPathfinding) && (inX != -1 || inY != -1)) {
                unsigned short theID = gContinentManager.GetContinentID(inX, inY);
                EEAIContinent* theFrom = theID != 0 ? gContinentManager.GetContinentByID(theID) : NULL;
                if (theFrom != NULL)
                    theResult = gContinentManager.AreConnected(theFrom, gContinentManager.GetContinentAt(inTile.GetX(), inTile.GetY()),
                        this->mPlayer->GetCitizenPassability());
                else
                    theResult = gContinentManager.GetContinent(inTile, kAIPathLandMass) == inContinent;
            } else
                theResult = gContinentManager.GetContinent(inTile, kAIPathLandMass) == inContinent;
        }
    }
    if (theResult) {
        if (EECPIsSiteClass(inUnitClass)) {
            EEGetSideTiles(theX, theY, inSize, theTiles);
            theResult = std::find_if(theTiles.begin(), theTiles.end(), EECPBMOnContinent(inContinent)) != theTiles.end();
            if (!theResult) {
                EEGetCornerTiles(1, theX, theY, inSize, theTiles);
                theResult = std::find_if(theTiles.begin(), theTiles.end(), EECPBMOnContinent(inContinent))
                    != theTiles.end();
            }
        }
    }
    if (theResult) {
        theResult = !this->HasEnemyNear(theX, theY);
        if (theResult && inStrict) {
            bool theOpen;
            if (gWorld.GetMap() != NULL && ((gWorld.GetMap()->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.GetMap()->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
                theOpen = true;
            else
                theOpen = false;
            if (!theOpen)
                theResult = gContinentManager.GetChokePointAt(inTile) == NULL;
            if (theResult) {
                theResult = gContinentManager.GetHillAt(inTile) == NULL;
                if (theResult) {
                    if (inUnitClass == kUnitTypeFarm || inUnitClass == kUnitTypeRoboticFarm)
                        theResult = EEGetTileUnitOfType(inTile, kUnitTypeResForageBush) == NULL;
                    if (theResult) {
                        if (EECPCountOwn(this->mPlayer, kUnitTypeAirport) > 0)
                            theResult = !this->IsNearAirportEnd(theX, theY, inSize, 2.0f);
                        if (theResult) {
                            if (!EECPIsSiteClass(inUnitClass))
                                theResult = !this->HasTownCenterNear(theX, theY, 3.0f);
                            if (theResult) {
                                if (!theOpen)
                                    theResult = !this->HasGateNear(theX, theY, 4.0f);
                                if (theResult && inUnitClass != kUnitTypeGranary && inUnitClass != kUnitTypeFarm && inUnitClass != kUnitTypeRoboticFarm
                                    && inUnitClass != kUnitTypeAA10Gun88mm && inUnitClass != kUnitTypeAA13MissileTower && inUnitClass != kUnitTypeAA15MissileTower) {
                                    long i;
                                    theTiles.clear();
                                    theCorner.mY = inTile.GetY() - 1;
                                    theCorner.mX = inTile.GetX() - 1;
                                    theTiles.push_back(theCorner);
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mY++;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mX++;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize + 1; i++) {
                                        theCorner.mY--;
                                        theTiles.push_back(theCorner);
                                    }
                                    for (i = 0; i < inSize; i++) {
                                        theCorner.mX--;
                                        theTiles.push_back(theCorner);
                                    }
                                    EEUnitClass* theClass;
                                    if (inUnitClass == kUnitTypeDock || inUnitClass == kUnitTypeNavyYard)
                                        theClass = gUnitClassTable[kUnitTypeS02FishingBoatStone];
                                    else if (inUnitClass == kUnitTypeSpaceDock || inUnitClass == kUnitTypeSpaceTurret || inUnitClass == kUnitTypeOrbitalSpaceStation)
                                        theClass = gUnitClassTable[kUnitTypeSp15SpaceTransport];
                                    else
                                        theClass = gUnitClassTable[kUnitTypeCitizen];
                                    unsigned long j;
                                    for (j = 0; EECPBMCannotStandOn(*theTiles.begin(), theClass) && j < theTiles.size();
                                        j++) {
                                        theCorner.mX = theTiles.begin()->GetX();
                                        theCorner.mY = theTiles.begin()->GetY();
                                        theTiles.erase(theTiles.begin());
                                        theTiles.push_back(theCorner);
                                    }
                                    for (j = 0; !EECPBMCannotStandOn(*theTiles.begin(), theClass) && j < theTiles.size();
                                        j++) {
                                        theCorner.mX = theTiles.begin()->GetX();
                                        theCorner.mY = theTiles.begin()->GetY();
                                        theTiles.erase(theTiles.begin());
                                        theTiles.push_back(theCorner);
                                    }
                                    std::vector<U2DPoint<long> >::iterator it;
                                    for (it = theTiles.begin(); it != theTiles.end();) {
                                        if (!EECPBMCannotStandOn(*it, theClass))
                                            break;
                                        it = theTiles.erase(it);
                                        if (EECPIsSiteClass(inUnitClass))
                                            theCount++;
                                    }
                                    if (EECPIsSiteClass(inUnitClass))
                                        theResult = theCount >= 3;
                                    for (it = theTiles.begin(); it != theTiles.end() && theResult; it++)
                                        theResult = !EECPBMCannotStandOn(*it, theClass);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// Tells whether a class is built on a shore or space site (dock, navy yard, space dock and two space classes)
bool EECPIsSiteClass(long inUnitClass)
{
    return inUnitClass == 0x24 || inUnitClass == 0x41 || inUnitClass == 0x2d8 || inUnitClass == 0x302
        || inUnitClass == 0x31f;
}

// (EECPResourceManager.cpp's range: 0x4921f7)
bool EECPUnitWithinDistance::operator()(EEUnit* inUnit) const
{
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    return (float)theTile.DistanceToSquared(this->mCenter) <= this->mDistanceSquared;
}

// The deque/vector/heap/sort COMDATs (named through call sites)
// TEMPLATE: EXE 0x466cfb
// TEMPLATE: EXE 0x468849
// TEMPLATE: EXE 0x46b6dc
// TEMPLATE: EXE 0x46b76c
// TEMPLATE: EXE 0x46b798
// TEMPLATE: EXE 0x46b82b
// TEMPLATE: EXE 0x46b857
// TEMPLATE: EXE 0x46b883
// TEMPLATE: EXE 0x46b92c
// TEMPLATE: EXE 0x46ba3a
// TEMPLATE: EXE 0x46ba94
// TEMPLATE: EXE 0x46bd85
// TEMPLATE: EXE 0x46bdbe
// TEMPLATE: EXE 0x46bde5
// TEMPLATE: EXE 0x46be1f
// TEMPLATE: EXE 0x46be46
// TEMPLATE: EXE 0x46be8e
// TEMPLATE: EXE 0x46bf74
// TEMPLATE: EXE 0x46c027
// TEMPLATE: EXE 0x46c065
// TEMPLATE: EXE 0x46c120
// TEMPLATE: EXE 0x46c1e6
// TEMPLATE: EXE 0x46c2cc
// TEMPLATE: EXE 0x46c3b0
// TEMPLATE: EXE 0x46c494
// TEMPLATE: EXE 0x46c4bb
// TEMPLATE: EXE 0x46c4e7
// TEMPLATE: EXE 0x46c536
// TEMPLATE: EXE 0x46c5ce
// TEMPLATE: EXE 0x46c5f3
// TEMPLATE: EXE 0x46c62b
// TEMPLATE: EXE 0x46c65e
// TEMPLATE: EXE 0x46c76b
// TEMPLATE: EXE 0x46c7fa
// TEMPLATE: EXE 0x46c854
// TEMPLATE: EXE 0x46c87d
// TEMPLATE: EXE 0x46c956
// TEMPLATE: EXE 0x46c993
// TEMPLATE: EXE 0x46c9c0
// TEMPLATE: EXE 0x46ca9c
// TEMPLATE: EXE 0x46cad0
// TEMPLATE: EXE 0x46cb03
// TEMPLATE: EXE 0x46cc10
// TEMPLATE: EXE 0x46cc55
// TEMPLATE: EXE 0x46cc88
// TEMPLATE: EXE 0x46cd90
// TEMPLATE: EXE 0x46cdc3
// TEMPLATE: EXE 0x46cecb
// TEMPLATE: EXE 0x46cf0d
// TEMPLATE: EXE 0x46cf51
// TEMPLATE: EXE 0x46cf9e
// TEMPLATE: EXE 0x46d0b5
// TEMPLATE: EXE 0x46d1cd
// TEMPLATE: EXE 0x46d21a
// TEMPLATE: EXE 0x46d2d1
// TEMPLATE: EXE 0x46d30e
// TEMPLATE: EXE 0x46d34d
// TEMPLATE: EXE 0x46d446
// TEMPLATE: EXE 0x46d470
// TEMPLATE: EXE 0x46d510
// TEMPLATE: EXE 0x46d62f
// TEMPLATE: EXE 0x46d66c
// TEMPLATE: EXE 0x46d6b0
// TEMPLATE: EXE 0x46d7d0
// TEMPLATE: EXE 0x46d803
// TEMPLATE: EXE 0x46d8c1
// TEMPLATE: EXE 0x46d905
// TEMPLATE: EXE 0x46d952
// TEMPLATE: EXE 0x46da69
// TEMPLATE: EXE 0x46daa5
// TEMPLATE: EXE 0x46dae9
// TEMPLATE: EXE 0x46db36
// TEMPLATE: EXE 0x46db69
// TEMPLATE: EXE 0x46dc1a
// TEMPLATE: EXE 0x46dc5e
// TEMPLATE: EXE 0x46dcab
// TEMPLATE: EXE 0x46ddcb
// TEMPLATE: EXE 0x46ddfe
// TEMPLATE: EXE 0x46deaa
// TEMPLATE: EXE 0x46ded6
// TEMPLATE: EXE 0x46df09
// TEMPLATE: EXE 0x46df70
// TEMPLATE: EXE 0x46df99
// TEMPLATE: EXE 0x46dfbe
// TEMPLATE: EXE 0x46e005
// TEMPLATE: EXE 0x46e032
// TEMPLATE: EXE 0x46e092
// TEMPLATE: EXE 0x46e0c5
// TEMPLATE: EXE 0x46e122
// TEMPLATE: EXE 0x46e155
// TEMPLATE: EXE 0x46e1b2
// TEMPLATE: EXE 0x46e1e5
// TEMPLATE: EXE 0x46e26d
// TEMPLATE: EXE 0x46e2ce
// TEMPLATE: EXE 0x46e2fc
// TEMPLATE: EXE 0x46e36d
// TEMPLATE: EXE 0x46e3c3
// TEMPLATE: EXE 0x46e457
// TEMPLATE: EXE 0x46e4d0
// TEMPLATE: EXE 0x46e540
// TEMPLATE: EXE 0x46e5ba
// TEMPLATE: EXE 0x46e683
// TEMPLATE: EXE 0x46e6ca
// TEMPLATE: EXE 0x46e719
// TEMPLATE: EXE 0x46e750
// TEMPLATE: EXE 0x46e7b0
// TEMPLATE: EXE 0x46e7fd
// TEMPLATE: EXE 0x46e841
// TEMPLATE: EXE 0x46e890

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x46a20e

// Template and inline COMDATs this unit already emits (identified by name)
// TEMPLATE: EXE 0x46bace
// TEMPLATE: EXE 0x46c585
// TEMPLATE: EXE 0x46c5a2
// TEMPLATE: EXE 0x46e62a


// whether one of the player's living citizens on the map could build a class at a tile: the unit manager's FindBuilder
// rule (the site's path area, or for the site classes and towers a shore of its continent touching the footprint) over
// every citizen, whoever holds it (the Lua verbs pick the builder)
bool EECPPlacement::HasBuilderFor(const U2DPoint<long>& inTile, long inUnitClass)
{
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(inTile);
    bool theEdge = gWorld.mMap != NULL && gWorld.mMap->IsEdgeTile(inTile.mX, inTile.mY);
    for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* c = theUnit->mComplexUnit;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (c == NULL || theCategory != kUnitCategoryLandCitizen || !(theUnit->mHitPoints > 0) || c->mTileX == -10000)
            continue;
        if (theEdge || gContinentManager.GetPathAreaAt(c) == theArea)
            return true;
        if (EECPIsSiteClass(inUnitClass) || inUnitClass == EECPTowerClass(this->mPlayer)) {
            long theSize = gUnitClassTable[inUnitClass]->mTileSize;
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(c->GetTileX(), c->GetTileY());
            if (theContinent == NULL)
                continue;
            EEAIListView<U2DPoint<long> > theTiles = theContinent->GetShoreTiles();
            for (long x = inTile.mX; x < inTile.mX + theSize; x++)
                for (long y = inTile.mY; y < inTile.mY + theSize; y++)
                    if (std::find(theTiles.begin(), theTiles.end(), U2DPoint<long>(x, y)) != theTiles.end())
                        return true;
        }
    }
    return false;
}

// the site tiles collected again after a load
long EECPPlacement::Restore()
{
    this->mWaterSites.clear();
    this->CollectSiteTiles(2);
    this->mSpaceSites.clear();
    this->CollectSiteTiles(4);
    return 0;
}

// the town centre tile: the first own unit tracked with no tile yet (the unit manager's UnitAdded rule), saved
long EECPPlacement::Load(FSFile& inFile)
{
    return this->mTownCenterTile.Load(inFile);
}

long EECPPlacement::Save(FSFile& outFile)
{
    return this->mTownCenterTile.Save(outFile);
}

void EECPPlacement::Reset()
{
    this->mTownCenterTile = U2DPoint<long>(-10000, -10000);
    this->mTypesSeen.clear();
}

void EECPPlacement::UnitAdded(EEComplexUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mUnitClass == NULL || !(inUnit->mHitPoints > 0))
        return;
    long theCategory = inUnit->mUnitClass->mCategory;
    long theType = inUnit->mUnitClass->mUnitType;
    if (theCategory == kUnitCategoryProjectile || theCategory == kUnitCategoryBomb || theCategory == kUnitCategoryMissile
        || theType == kUnitTypePathPoint)
        return;
    if (!this->mTypesSeen.insert(theType).second)
        return;
    if (this->mTownCenterTile.mX == -10000 && this->mTownCenterTile.mY == -10000 && !inUnit->mUnitClass->IsLandBuilding())
        this->SetTownCenter(inUnit);
}

long EECPCountOwn(EEComputerPlayer* inPlayer, long inType)
{
    long theCount = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitClass->mUnitType == inType && theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0)
            theCount++;
    return theCount;
}

// ---------------------------------------------------------------------------------------------------------------
// The epoch's classes (EECPCivilizationManager's, moved here with the placement searches in stage 6)

long EECPAntiAirClass(EEComputerPlayer* inPlayer)
{
    long theResult = 0;
    long theEpoch = inPlayer->mTechnologies->mCurrentEpoch;
    if (theEpoch >= kTechEpochAtomicWW1) {
        if (theEpoch > kTechEpochAtomicModern) {
            if (theEpoch > kTechEpochNano) {
                if (theEpoch == kTechEpochSpace)
                    theResult = 0x300;
            } else
                theResult = 0x93;
        } else
            theResult = 0x91;
    }
    return theResult;
}

long EECPFishingBoatClass(EEComputerPlayer* inPlayer)
{
    long theResult = 0;
    long theEpoch = inPlayer->mTechnologies->mCurrentEpoch;
    if (theEpoch >= kTechEpochPrehistoric) {
        if (theEpoch > kTechEpochCopper) {
            if (theEpoch > kTechEpochIndustrial) {
                if (theEpoch <= kTechEpochSpace)
                    theResult = 0xd3;
            } else
                theResult = 0x6d;
        } else
            theResult = 0x6c;
    }
    return theResult;
}

long EECPGateClass(EEComputerPlayer* inPlayer)
{
    long theResult = 0;
    switch (inPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
        theResult = 0x2e;
        break;
    case kTechEpochCopper:
        theResult = 0x1d9;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1c6;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1bc;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1bb;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1ba;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x126;
        break;
    case kTechEpochSpace:
        theResult = 0x2ff;
        break;
    }
    return theResult;
}

long EECPTowerClass(EEComputerPlayer* inPlayer)
{
    long theResult = 0;
    switch (inPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
        theResult = 0x2f;
        break;
    case kTechEpochCopper:
        theResult = 0x1cd;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1ce;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1cf;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1d0;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1d1;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x1d2;
        break;
    case kTechEpochSpace:
        theResult = 0x2fd;
        break;
    }
    return theResult;
}

long EECPWallClass(EEComputerPlayer* inPlayer)
{
    long theResult = 0;
    switch (inPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochCopper:
        theResult = 0x1d3;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1d4;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1d5;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1d6;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1d7;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x1d8;
        break;
    case kTechEpochSpace:
        theResult = 0x2fe;
        break;
    }
    return theResult;
}

// ---------------------------------------------------------------------------------------------------------------
// The placement searches of the Lua verbs (ai.place; the build verb's rules, EECPNextTactics.cpp before stage 6)

static EECPResourceCenter* EECPNextSiteAt(EEComputerPlayer* inPlayer, long inSite)
{
    if (inSite < 0 || (unsigned long)inSite >= inPlayer->GetNumResourceCenters())
        return NULL;
    return inPlayer->GetResourceCenter((unsigned long)inSite);
}

// the tile of a centre's first resource unit with some left (what its site in the view shows; a worked mine's hit
// points are 0)
static bool EECPNextSiteTile(EECPResourceCenter* inCenter, long& outX, long& outY)
{
    for (unsigned long u = 0; u < inCenter->mUnits.size(); u++) {
        EEUnit* theUnit = inCenter->mUnits[u];
        if (theUnit->mResourceAmount > 0 && theUnit->mTile != NULL) {
            outX = theUnit->GetTileX();
            outY = theUnit->GetTileY();
            return true;
        }
    }
    return false;
}

// the first tile in the rings inFirst..inLast round a tile where the class can be built (CanBuildAt, as
// EECPRMPatchSite's rings round a forage patch)
static bool EECPNextRingSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inFirst, long inLast,
    EETileCoordinate& outTile)
{
    EECPPlacement* theBuild = inPlayer->mPlacement;
    EEUnitClass* theClass = gUnitClassTable[inType];
    unsigned short theContinent = gContinentManager.GetContinent(U2DPoint<long>(inX, inY), kAIPathLandMass);
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = inFirst; theRing <= inLast; theRing++) {
        if (theRing == 0) {
            EETileCoordinate theTile(inX, inY);
            if (theBuild->CanBuildAt(theContinent, inType, theTile, theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTile;
                return true;
            }
            continue;
        }
        theTiles.clear();
        EEGetSquareRingTiles(inX, inY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (theBuild->CanBuildAt(theContinent, inType, theTiles[k], theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// a tower's tile in the rings inFirst..inLast round a tile, none of the player's towers within its range of it (the
// rings' order: deterministic; CanBuildAt: only tiles in sight, no armed enemy near)
static bool EECPNextTowerRingSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inFirst, long inLast,
    EETileCoordinate& outTile)
{
    EECPPlacement* theBuild = inPlayer->mPlacement;
    EEUnitClass* theClass = gUnitClassTable[inType];
    unsigned short theContinent = gContinentManager.GetContinent(U2DPoint<long>(inX, inY), kAIPathLandMass);
    float theSpacing = inPlayer->mUnitClassUpgrades[inType]->mRange;
    std::vector<EETileCoordinate> theTiles;
    for (long theRing = inFirst; theRing <= inLast; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(inX, inY, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++)
            if (!theBuild->HasTowerNear(theTiles[k].mX, theTiles[k].mY, theSpacing * theSpacing)
                && theBuild->CanBuildAt(theContinent, inType, theTiles[k], theClass->GetTileSize(), true, true, inX, inY)) {
                outTile = theTiles[k];
                return true;
            }
    }
    return false;
}

// the tiles between a building of inSize at a tile (its corner) and a resource unit's footprint (0: touching)
static long EECPNextGap(long inX, long inY, long inSize, EEUnit* inUnit)
{
    long theUX = inUnit->GetTileX(), theUY = inUnit->GetTileY();
    long theUSize = inUnit->mUnitClass->GetTileSize() > 0 ? inUnit->mUnitClass->GetTileSize() : 1;
    long theDX = (std::max)((std::max)(theUX - (inX + inSize), inX - (theUX + theUSize)), 0L);
    long theDY = (std::max)((std::max)(theUY - (inY + inSize), inY - (theUY + theUSize)), 0L);
    return (std::max)(theDX, theDY);
}

static long long EECPNextDistance2Of(long inX, long inY, long inX2, long inY2)
{
    long long theDX = inX - inX2, theDY = inY - inY2;
    return theDX * theDX + theDY * theDY;
}

// a drop site by its resource (§8.35, the owner: "settlements directly adjacent to all resource mines, not just randomly
// 3 tiles away"): of the tiles in the rings 1..inLast round a tile where the class can be built (CanBuildAt keeps a
// one-tile margin free, the gatherers' side), the one nearest the resource units (inUnits; the tile itself when
// empty), then nearest the town centre (the shorter way home), then the first in the rings' order
static bool EECPNextAdjacentSite(EEComputerPlayer* inPlayer, long inType, long inX, long inY, long inLast,
    const std::vector<EEUnit*>& inUnits, EETileCoordinate& outTile, long& outGap)
{
    EECPPlacement* theBuild = inPlayer->mPlacement;
    EEUnitClass* theClass = gUnitClassTable[inType];
    long theSize = theClass->GetTileSize() > 0 ? theClass->GetTileSize() : 1;
    // the town's land (a tree's tile is forest, on no land continent: every settlement by a forest was refused)
    unsigned short theContinent = gContinentManager.GetContinent(theBuild->mTownCenterTile, kAIPathLandMass);
    long theTownX = theBuild->mTownCenterTile.mX, theTownY = theBuild->mTownCenterTile.mY;
    std::vector<EETileCoordinate> theTiles;
    bool theFound = false;
    long theBestGap = 0;
    long long theBestHome = 0;
    for (long theRing = 1; theRing <= inLast; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(inX - theSize / 2, inY - theSize / 2, theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++) {
            long theGap = 1000;
            if (inUnits.empty()) {
                long theDX = (std::max)((std::max)(inX - (theTiles[k].mX + theSize - 1), theTiles[k].mX - inX), 0L);
                long theDY = (std::max)((std::max)(inY - (theTiles[k].mY + theSize - 1), theTiles[k].mY - inY), 0L);
                theGap = (std::max)(theDX, theDY);
            }
            for (unsigned long u = 0; u < inUnits.size(); u++)
                theGap = (std::min)(theGap, EECPNextGap(theTiles[k].mX, theTiles[k].mY, theSize, inUnits[u]));
            if (theFound && theGap > theBestGap)
                continue;
            long long theDX = theTiles[k].mX - theTownX, theDY = theTiles[k].mY - theTownY;
            long long theHome = theDX * theDX + theDY * theDY;
            if (theFound && theGap == theBestGap && theHome >= theBestHome)
                continue;
            if (!theBuild->CanBuildAt(theContinent, inType, theTiles[k], theSize, true, true, inX, inY))
                continue;
            theFound = true;
            theBestGap = theGap;
            theBestHome = theHome;
            outTile = theTiles[k];
            outGap = theGap;
        }
        if (theFound && theBestGap <= 1 && theRing >= theBestGap + 2)
            break;   // (nothing nearer lies further out)
    }
    return theFound;
}

static const long kNextDockSpacing = 12;   // tiles: fish this near one of the player's docks are served by it
// The placement searches for the Lua verbs (data/ai/lib/verbs.lua, docs/AI_LUA_VERBS.md; ai.place): one search each,
// the rules build() placed by; which search, round what and in what order is the script's choice

// the dock's tile by the nearest fish within kNextDockReach of the town centre with none of the player's docks within
// kNextDockSpacing of it (any fish, seen or not: the start's food, as EECPNextDockByFish); false: none (outReason)
static bool EECPNextDockFishSite(EEComputerPlayer* inPlayer, EETileCoordinate& outTile, long& outFishX, long& outFishY)
{
    EECPPlacement* theBuild = inPlayer->mPlacement;
    if (theBuild->mTownCenterTile.mX == -10000 || gUnitClassTable[kUnitTypeDock] == NULL || gWorld.mPlayers[0] == NULL)
        return false;
    long theTownX = theBuild->mTownCenterTile.mX, theTownY = theBuild->mTownCenterTile.mY;
    std::vector<EEUnit*> theDocks;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (theUnit->mUnitClass->mUnitType == kUnitTypeDock && theUnit->mHitPoints > 0)
            theDocks.push_back(theUnit);
    EEUnitClass* theClass = gUnitClassTable[kUnitTypeDock];
    long theSize = theClass->GetTileSize() > 0 ? theClass->GetTileSize() : 1;
    unsigned short theContinent = gContinentManager.GetContinent(theBuild->mTownCenterTile, kAIPathLandMass);
    EEUnit* theBestFish = NULL;
    long long theBestD2 = 0;
    std::vector<EETileCoordinate> theTiles;
    for (EEUnit* theFish = gWorld.mPlayers[0]->mFirstUnit; theFish != NULL; theFish = static_cast<EEUnit*>(theFish->mNext)) {
        if (theFish->mUnitClass->mFamily != kUnitFamilyFish || theFish->mResourceType != kResourceFood
            || !(theFish->mResourceAmount > 0) || theFish->mTile == NULL)
            continue;
        long long theDX = theFish->GetTileX() - theTownX, theDY = theFish->GetTileY() - theTownY;
        long long theD2 = theDX * theDX + theDY * theDY;
        if (theD2 > (long long)kNextDockReach * kNextDockReach || (theBestFish != NULL && theD2 >= theBestD2))
            continue;
        bool theServed = false;
        for (unsigned long d = 0; d < theDocks.size() && !theServed; d++) {
            long long theX = theDocks[d]->GetTileX() - theFish->GetTileX(), theY = theDocks[d]->GetTileY() - theFish->GetTileY();
            theServed = theX * theX + theY * theY <= (long long)kNextDockSpacing * kNextDockSpacing;
        }
        if (theServed)
            continue;
        bool theShore = false;
        for (long theRing = 1; theRing <= 6 && !theShore; theRing++) {
            theTiles.clear();
            EEGetSquareRingTiles(theFish->GetTileX(), theFish->GetTileY(), theRing, theTiles);
            for (unsigned long k = 0; k < theTiles.size() && !theShore; k++)
                theShore = theBuild->CanBuildAt(theContinent, kUnitTypeDock, theTiles[k], theSize, false, true, theTownX, theTownY);
        }
        if (theShore) {
            theBestFish = theFish;
            theBestD2 = theD2;
        }
    }
    if (theBestFish == NULL)
        return false;
    bool theFound = false;
    long long theBestHome = 0;
    for (long theRing = 1; theRing <= 6 && !theFound; theRing++) {
        theTiles.clear();
        EEGetSquareRingTiles(theBestFish->GetTileX(), theBestFish->GetTileY(), theRing, theTiles);
        for (unsigned long k = 0; k < theTiles.size(); k++) {
            if (!theBuild->CanBuildAt(theContinent, kUnitTypeDock, theTiles[k], theSize, false, true, theTownX, theTownY))
                continue;
            long long theDX = theTiles[k].mX - theTownX, theDY = theTiles[k].mY - theTownY;
            if (!theFound || theDX * theDX + theDY * theDY < theBestHome) {
                theFound = true;
                theBestHome = theDX * theDX + theDY * theDY;
                outTile = theTiles[k];
            }
        }
    }
    outFishX = theBestFish->GetTileX();
    outFishY = theBestFish->GetTileY();
    return theFound;
}

bool EECPNextPlaceSearch(EEComputerPlayer* inPlayer, long inType, long inMode, long inX, long inY, long inFirst,
    long inLast, const std::vector<long>& inUnits, long inSite, long& outX, long& outY, long& outGap)
{
    outX = outY = -1;
    outGap = -1;
    if (inType < 0 || (unsigned long)inType >= EEUnitClassCount() || gUnitClassTable[inType] == NULL
        || inPlayer->mPlacement == NULL || gWorld.mMap == NULL)
        return false;
    EETileCoordinate theTile(0, 0);
    bool theFound = false;
    switch (inMode) {
    case kNextPlaceRing:
        theFound = EECPNextRingSite(inPlayer, inType, inX, inY, inFirst, inLast, theTile);
        break;
    case kNextPlaceTower:
        theFound = EECPNextTowerRingSite(inPlayer, inType, inX, inY, inFirst, inLast, theTile);
        break;
    case kNextPlaceAdjacent: {
        std::vector<EEUnit*> theUnits;
        for (unsigned long k = 0; k < inUnits.size(); k++) {
            EEUnit* theUnit = gWorld.GetUnit((unsigned long)inUnits[k]);
            if (theUnit != NULL && theUnit->mIsValid && theUnit->mTile != NULL)
                theUnits.push_back(theUnit);
        }
        theFound = EECPNextAdjacentSite(inPlayer, inType, inX, inY, inLast, theUnits, theTile, outGap);
        break;
    }
    case kNextPlaceCentre: {
        EECPResourceCenter* theCenter = EECPNextSiteAt(inPlayer, inSite);
        theFound = theCenter != NULL && theCenter->FindBuildSite(inType, theTile);
        break;
    }
    case kNextPlaceClassic:
        theFound = inPlayer->mPlacement->FindSite(inType, theTile);
        break;
    case kNextPlaceDock: {
        long theFishX = -1, theFishY = -1;
        theFound = EECPNextDockFishSite(inPlayer, theTile, theFishX, theFishY);
        outGap = theFishX >= 0 ? theFishX * 65536 + theFishY : -1;   // (the fish's tile, for the trace)
        break;
    }
    default:
        return false;
    }
    if (!theFound || (theTile.mX == 0 && theTile.mY == 0))
        return false;
    outX = theTile.mX;
    outY = theTile.mY;
    return true;
}

