// C:\EEX\Empire Earth\AI Thread\EEAIPathSubSector.cpp (file name inferred: no __FILE__; RTTI class
// EEAIPathSubSector, vtable 0x43a538). A path sub-sector of the continent map: 0x2be88..0x2c79f (EEAIPathSector.cpp
// ends at 0x2be88; EEAIRectangleArea's ctor starts at 0x2c79f). ctor, ??_G, dtor, the members in address order, then
// the multimap COMDATs.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
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
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"

#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"

// FUNCTION: EXE 0x42be88
// Creates an empty path sub-sector with no sector, ids 0, kind none and no owner
EEAIPathSubSector::EEAIPathSubSector()
    : mSector(NULL), mContinentID(0), mPathAreaID(0), mID(0), mKind(kAITileNone), mPlayerID(0), mIsStale(false)
{
}

// FUNCTION: EXE 0x42bf08
// Destroys the sub-sector (members only)
EEAIPathSubSector::~EEAIPathSubSector()
{
}

// FUNCTION: EXE 0x42bf45
// Returns the unit on the sub-sector's location that makes its kind: the gate, the wall (not a gate), a
// blocking player or gaia unit, or the tree; NULL for other kinds or when none is there
EEComplexUnit* EEAIPathSubSector::GetBlockingUnit()
{
    RTSTileUnitNode* theNode;
    EEComplexUnit* theResult = NULL;
    EETile* theTile;
    switch (this->mKind) {
    case kAITileGate:
        theTile = gWorld.mMap->GetTile(this->mLocation.mX, this->mLocation.mY);
        if (theTile) {
            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
                if (theUnit && theUnit->mUnitClass->mCategory == kUnitCategoryGate) {
                    theResult = theUnit;
                    break;
                }
            }
        }
        break;
    case kAITileWall:
        theTile = gWorld.mMap->GetTile(this->mLocation.mX, this->mLocation.mY);
        if (theTile) {
            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
                if (theUnit && theUnit->mUnitClass->mIsWall && theUnit->mUnitClass->mCategory != kUnitCategoryGate) {
                    theResult = theUnit;
                    break;
                }
            }
        }
        break;
    case kAITileUnitObstacle:
        theTile = gWorld.mMap->GetTile(this->mLocation.mX, this->mLocation.mY);
        if (theTile) {
            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
                if (theUnit && theUnit->mPlayer->mPlayerID != 0 && theUnit->mUnitClass->mBlocksTile) {
                    theResult = theUnit;
                    break;
                }
            }
        }
        break;
    case kAITileGaiaObstacle:
        theTile = gWorld.mMap->GetTileAtLocation(this->mLocation);
        if (theTile) {
            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
                if (theUnit && theUnit->mPlayer->mPlayerID == 0 && theUnit->mUnitClass->mBlocksTile) {
                    theResult = theUnit;
                    break;
                }
            }
        }
        break;
    case kAITileForest:
        theTile = gWorld.mMap->GetTileAtLocation(this->mLocation);
        if (theTile) {
            for (theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
                if (theUnit && theUnit->mUnitClass->mIsTree) {
                    theResult = theUnit;
                    break;
                }
            }
        }
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x42c0b9
// Appends the sub-sector's links to the sub-sector with the given id; always returns false
bool EEAIPathSubSector::GetLinks(unsigned short inID,
                                 std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >& outLinks)
{
    std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::iterator theLink;
    if (this->mLinks.size() > 0) {
        std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::iterator theEnd =
            this->mLinks.upper_bound(inID);
        for (theLink = this->mLinks.lower_bound(inID); theLink != theEnd; ++theLink)
            outLinks.push_back(theLink->second);
    }
    return false;
}

// FUNCTION: EXE 0x42c10f
// Returns true when the sub-sector's gate lets the player through (no live gate, open, or own/non-enemy and
// unlocked); false for side parts and closed enemy or locked gates (the last gate found carries over)
bool EEAIPathSubSector::IsPassable(long inPlayer)
{
    bool thePassable = false;
    EEComplexUnit* theGate = NULL;
    EEUCGate* theGateClass = NULL;
    U2DPoint<long> theTile;
    for (long i = this->mTiles.Size() - 1; i >= 0 && !thePassable; i--) {
        theTile = this->mTiles[i];
        EETile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
        if (!theMapTile)
            continue;
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (!theUnit || theUnit->GetUnitClass()->mCategory != kUnitCategoryGate)
                continue;
            theGate = theUnit->mComplexUnit;
            theGateClass = dynamic_cast<EEUCGate*>(theGate->GetUnitClass());
            break;
        }
        if (!theGate || theGate->mHitPoints <= 0) {
            thePassable = true;
            break;
        }
        if (theGateClass->IsSidePart(theGate))
            thePassable = false;
        else if ((theGate->mCommand ? theGate->mCommand->mType : 0) != kActionClosed ||
                 ((gWorld.mPlayers[inPlayer] == theGate->mPlayer ||
                   theGate->mPlayer->mEnemies[gWorld.mPlayers[inPlayer]->mPlayerID] == 0) && !theGate->mIsLocked))
            thePassable = true;
        else
            thePassable = false;
    }
    return thePassable;
}

// FUNCTION: EXE 0x42c219
// Returns false only when the other sub-sector is a neighbour with links to this one and no link end's kind
// (an index into inFlags.mFlags) is allowed; true otherwise
bool EEAIPathSubSector::AreLinksBlocked(EEAIPathSubSector* inOther, const EECPASGFlags& inFlags)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    bool theResult = true;
    if (inOther && this->IsNeighbor(inOther) && inOther != this) {
        inOther->GetLinks(this->mID, theLinks);
        theResult = theLinks.size() == 0;
        for (long i = theLinks.size() - 1; i >= 0 && !theResult; i--) {
            if (inFlags.mFlags[theLinks[i].first->mKind] == true || inFlags.mFlags[theLinks[i].second->mKind] == true)
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42c2c5
// Drops every link to or through the other sub-sector and removes it from the neighbours; returns whether it
// was a neighbour
bool EEAIPathSubSector::RemoveNeighbor(EEAIPathSubSector* inOther)
{
    bool theResult = false;
    std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::iterator theLink;
    for (theLink = this->mLinks.begin(); theLink != this->mLinks.end();) {
        if (theLink->first == inOther->mID || theLink->second.first == inOther ||
            theLink->second.second == inOther)
            theLink = this->mLinks.erase(theLink);
        else
            ++theLink;
    }
    std::vector<EEAIPathSubSector*>::iterator theNeighbor = std::find(this->mNeighbors.begin(), this->mNeighbors.end(), inOther);
    if (theNeighbor != this->mNeighbors.end()) {
        theResult = true;
        this->mNeighbors.erase(theNeighbor);
    }
    return theResult;
}

// FUNCTION: EXE 0x42c379
// Drops the sub-sector's links to the sub-sector with the given id
void EEAIPathSubSector::RemoveLinks(unsigned short inID)
{
    if (this->mLinks.size() <= 0)
        return;
    std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::iterator theEnd =
        this->mLinks.upper_bound(inID);
    std::multimap<unsigned short, std::pair<EEAIPathSubSector*, EEAIPathSubSector*> >::iterator theFirst =
        this->mLinks.lower_bound(inID);
    if (theFirst != this->mLinks.end())
        this->mLinks.erase(theFirst, theEnd);
}

// FUNCTION: EXE 0x42c3bc
// Fills the sub-sector's tile set from a tile list
void EEAIPathSubSector::CreateTiles(const std::vector<U2DPoint<long> >& inTiles)
{
    this->mTiles.Create(inTiles);
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// SYNTHETIC: EXE 0x42beec
// TEMPLATE: EXE 0x42c3cc
// TEMPLATE: EXE 0x42c3f7
// TEMPLATE: EXE 0x42c6a5
// TEMPLATE: EXE 0x42c716
// TEMPLATE: EXE 0x42c742
// TEMPLATE: EXE 0x42c774
